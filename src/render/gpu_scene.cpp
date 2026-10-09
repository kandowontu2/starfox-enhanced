#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/gpu_dispatch.hpp"
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/gpu_scene_counters.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include "starfox/render/gpu_model.hpp"
#include "starfox/render/face_material.hpp"
#include "starfox/render/gpu_projection.hpp"
#include "starfox/render/gpu_ray_geometry.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/stereo_output.hpp"
#include <cstring>
#include <stdexcept>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <utility>
#include <unordered_map>
#include <cmath>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/sdl_d3d12_bridge.h"
#if __has_include(<vulkan/vulkan.h>)
#define STARFOX_SCENE_VULKAN_TIMESTAMPS 1
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#endif
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include "starfox/render/gpu_preparation.hpp"
#include "gpu_model_timing_hook.hpp"
#include "starfox/render/gpu_retirement.hpp"
#include "shaders/generated/scene_portable.hpp"
#endif
namespace starfox::render {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {
#include "gpu_scene_timestamps.inc"
#include "gpu_scene_draw_timestamps.inc"
template<class Predicate> void wait_stereo_worker(std::unique_lock<std::mutex>& lock,
    std::condition_variable& cv,Predicate ready) {
    auto* events=preparation_events;
    if(!events || !events->pump) {cv.wait(lock,ready);return;}
    while(!ready()) {
        if(cv.wait_for(lock,std::chrono::milliseconds(2),ready)) break;
        lock.unlock();
        {ScopedPreparationEvents suspend(nullptr);events->pump(events->user);}
        lock.lock();
    }
}
struct StereoEncodingDecision {
    std::mutex mutex;
    std::condition_variable cv;
    bool ready{};
    unsigned decision{}; // 0 pending, 1 cancel, 2 submit after the left eye.
    void encoded() {std::lock_guard lock(mutex);ready=true;cv.notify_all();}
    void wait_encoded() {std::unique_lock lock(mutex);wait_stereo_worker(lock,cv,[&]{return ready;});}
    void decide(bool submit) {std::lock_guard lock(mutex);decision=submit?2U:1U;cv.notify_all();}
    bool wait_decision() {std::unique_lock lock(mutex);cv.wait(lock,[&]{return decision!=0;});return decision==2;}
};
}
#endif
struct GpuStereoScene::Parallel {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    std::mutex mutex;
    std::condition_variable cv;
    std::function<void()> job;
    std::exception_ptr error;
    bool stop{},busy{};
    std::thread worker;
    Parallel():worker([this] {
        std::unique_lock lock(mutex);
        for(;;) {
            cv.wait(lock,[&]{return stop || bool(job);});
            if(stop) return;
            auto current=std::move(job);job={};
            lock.unlock();
            std::exception_ptr failure;
            try {current();} catch(...) {failure=std::current_exception();}
            // Destroy borrowed captures before publishing completion.
            current={};
            lock.lock();error=failure;busy=false;cv.notify_all();
        }
    }) {}
    ~Parallel() {
        {std::lock_guard lock(mutex);stop=true;cv.notify_all();}
        worker.join();
    }
    void start(std::function<void()> next) {
        std::lock_guard lock(mutex);
        if(busy) throw std::runtime_error("Stereo encoder already has a borrowed frame");
        error={};job=std::move(next);busy=true;cv.notify_all();
    }
    void wait() {
        std::unique_lock lock(mutex);wait_stereo_worker(lock,cv,[&]{return !busy;});
        auto failure=std::exchange(error,{});lock.unlock();
        if(failure) std::rethrow_exception(failure);
    }
#endif
};
#include "gpu_stereo_model_sources.inc"
GpuStereoScene::GpuStereoScene()=default;
GpuStereoScene::~GpuStereoScene(){release_device();}
StereoSourceTopologies::StereoSourceTopologies(std::span<GpuSceneDraw> left,
    std::span<GpuSceneDraw> right,std::uint64_t budget,bool prepare_projection,bool prepare_faces,bool prepare_rays) {
    const auto clear=[](std::span<GpuSceneDraw> draws) {
        for(auto& draw:draws) if(auto* model=std::get_if<GpuModelDraw>(&draw)) {
            model->prepared_topology=nullptr;model->prepared_projection=nullptr;model->prepared_gpu=nullptr;model->prepared_faces=nullptr;model->prepared_rays=nullptr;
        }
    };
    clear(left);clear(right);
    struct ResetOnFailure {
        std::span<GpuSceneDraw> left,right;bool complete{};
        ~ResetOnFailure() {
            if(!complete) for(auto draws:{left,right}) for(auto& draw:draws)
                if(auto* model=std::get_if<GpuModelDraw>(&draw)) {
                    model->prepared_topology=nullptr;model->prepared_projection=nullptr;model->prepared_faces=nullptr;model->prepared_rays=nullptr;
                }
        }
    } reset{left,right};
    if(left.size()!=right.size()) throw std::runtime_error("Stereo source recordings differ in length");
    struct ProjectionEntry {
        std::size_t frame{};double scale{};bool continuous{},byte_coordinates{};
        const PreparedProjectionSource* source{};
    };
    struct Entry {
        const PreparedBspSource* source[2]{};bool attempted[2]{};
        std::vector<ProjectionEntry> projections;
        std::vector<const PreparedFacesSource*> faces;
    };
    std::unordered_map<const assets::Shape*,Entry> index;
    for(std::size_t i=0;i<left.size();++i) {
        auto* l=std::get_if<GpuModelDraw>(&left[i]);
        auto* r=std::get_if<GpuModelDraw>(&right[i]);
        if(bool(l)!=bool(r)) throw std::runtime_error("Stereo source draw kinds differ");
        if(!l) continue;
        const bool flat=l->pose.explosion_progress!=0;
        if(l->shape!=r->shape || flat!=(r->pose.explosion_progress!=0)
            || l->pose.simple_scaled_sprite!=r->pose.simple_scaled_sprite
            || l->pose.collapse_to_axis_line!=r->pose.collapse_to_axis_line)
            throw std::runtime_error("Stereo source topology policies differ");
        if(!l->shape || l->pose.simple_scaled_sprite || l->pose.collapse_to_axis_line || !budget) continue;
        auto& entry=index[l->shape];
        if(!entry.attempted[flat]) {
            entry.attempted[flat]=true;
            if(bytes_<budget) {
                sources_.emplace_back(*l->shape,flat);
                const auto bytes=sources_.back().storage_bytes();
                if(bytes<=budget-bytes_) {bytes_+=bytes;entry.source[flat]=&sources_.back();}
                else sources_.pop_back();
            }
        }
        l->prepared_topology=r->prepared_topology=entry.source[flat];
        if(entry.source[flat]) models_+=2;
        if(!prepare_projection) continue;
        const auto frame=l->shape->frames.empty()?0:l->pose.animation_frame%l->shape->frames.size();
        const bool continuous=l->settings.render_scale>1 || l->pose.continuous_geometry;
        if(frame!=(r->shape->frames.empty()?0:r->pose.animation_frame%r->shape->frames.size())
            || continuous!=(r->settings.render_scale>1 || r->pose.continuous_geometry))
            throw std::runtime_error("Stereo projection source policies differ");
        auto found=std::find_if(entry.projections.begin(),entry.projections.end(),[&](const auto& p) {
            return p.frame==frame && p.continuous==continuous
                && (continuous || !p.byte_coordinates || p.scale==l->pose.scale);
        });
        if(found==entry.projections.end()) {
            if(bytes_>=budget) continue;
            projections_.emplace_back(*l->shape,l->pose,l->settings);
            const auto& source=projections_.back();
            if(!source.matches(*r->shape,r->pose,r->settings))
                throw std::runtime_error("Stereo native source prescaling differs");
            const auto bytes=source.storage_bytes();
            entry.projections.push_back({frame,l->pose.scale,continuous,source.byte_coordinates(),nullptr});
            found=std::prev(entry.projections.end());
            if(bytes<=budget-bytes_) {bytes_+=bytes;found->source=&source;}
            else projections_.pop_back();
        } else if(found->source && !found->source->matches(*r->shape,r->pose,r->settings))
            throw std::runtime_error("Stereo native source prescaling differs");
        l->prepared_projection=r->prepared_projection=found->source;
        if(found->source) projection_models_+=2;
    }
    // Prioritize existing topology/vertex sources within the common budget.
    // Prepare each actual material state once; differing eye states bind
    // different packets. No projected result or mutable fragment is borrowed.
    if(prepare_faces) for(auto draws:{left,right}) for(auto& draw:draws) {
        auto* model=std::get_if<GpuModelDraw>(&draw);
        if(!model || !model->prepared_topology || !PreparedFacesSource::supported(model->pose)) continue;
        auto& variants=index[model->shape].faces;
        const auto found=std::find_if(variants.begin(),variants.end(),[&](const auto* source) {
            return source->matches(*model->shape,*model->prepared_topology,model->pose,model->settings);
        });
        if(found!=variants.end()) model->prepared_faces=*found;
        else if(bytes_<budget) {
            faces_.emplace_back(*model->shape,*model->prepared_topology,model->pose,model->settings);
            const auto bytes=faces_.back().storage_bytes();
            if(bytes<=budget-bytes_) {
                bytes_+=bytes;model->prepared_faces=&faces_.back();variants.push_back(model->prepared_faces);
            } else faces_.pop_back();
        }
        if(model->prepared_faces) ++face_models_;
    }
    if(prepare_rays) {
        // Resolve all users before construction: shadow-only packets do not
        // allocate material connectivity, mixed users prepare it just once.
        struct RayUsers {bool materials{};std::vector<GpuModelDraw*> models;};
        std::unordered_map<const PreparedFacesSource*,RayUsers> requests;
        for(auto draws:{left,right}) for(auto& draw:draws) {
            auto* model=std::get_if<GpuModelDraw>(&draw);
            if(!model || !model->prepared_faces || !model->ray_geometry || model->emissive) continue;
            auto& request=requests[model->prepared_faces];
            request.materials|=model->ray_materials;request.models.push_back(model);
        }
        for(const auto& faces:faces_) {
            const auto found=requests.find(&faces);
            if(found==requests.end() || bytes_>=budget) continue;
            rays_.emplace_back(faces,found->second.materials);
            const auto bytes=rays_.back().storage_bytes();
            if(bytes>budget-bytes_) {rays_.pop_back();continue;}
            bytes_+=bytes;
            for(auto* model:found->second.models) {model->prepared_rays=&rays_.back();++ray_models_;}
        }
    }
    reset.complete=true;
}
void GpuStereoScene::release_device() noexcept {
    resident_ready_=false;
    // Every render call joins/cancels its borrowed job before returning.
    // Stop the idle worker before releasing any eye or SDL device resources.
    parallel_.reset();
    for(auto& eye:eyes_) eye.release_device();
    source_uploads_.reset();
}
bool stereo_screen_fixed_layer(std::span<const GpuSceneDraw> draws,
    bool sky_displacement,bool crosshair_displacement) noexcept {
    const auto fixed_raster=[&](const RasterCommands* commands) {
        return commands && (!crosshair_displacement
            || std::none_of(commands->commands.begin(),commands->commands.end(),
                [](const auto& c){return c.textured==4 && (c.reserved1&4U); }));
    };
    return std::all_of(draws.begin(),draws.end(),[&](const auto& draw) {
        if(const auto* raster=std::get_if<GpuRasterDraw>(&draw)) return fixed_raster(raster->commands);
        if(const auto* indexed=std::get_if<GpuIndexedLayerDraw>(&draw)) return fixed_raster(indexed->commands);
        if(const auto* bg=std::get_if<GpuBackgroundDraw>(&draw)) return bg->ppu
            && !(sky_displacement && bg->settings.layer==2 && bg->settings.tag==PixelLayer::background);
        return false;
    });
}
std::optional<std::vector<GpuSceneDraw>> resize_scene_raster(
    std::span<const GpuSceneDraw> frame,std::uint32_t width,std::uint32_t height) {
    if(!width || !height || width>32767 || height>32767) return {};
    std::vector<GpuSceneDraw> result(frame.begin(),frame.end());
    for(auto& item:result) {
        const bool valid=std::visit([&](auto& draw) {
            using T=std::decay_t<decltype(draw)>;
            if constexpr(std::is_same_v<T,GpuRasterDraw>) {
                if(!draw.commands || draw.commands->width()!=width || draw.commands->height()!=height) return false;
                draw.independent_raster_size=true;
            } else if constexpr(std::is_same_v<T,GpuIndexedLayerDraw>) {
                if(draw.reference_size!=std::array<std::uint32_t,2>{width,height}) return false;
            } else {
                unsigned scale;
                if constexpr(std::is_same_v<T,GpuModelDraw>) scale=draw.settings.render_scale;
                else scale=draw.scale;
                if(scale<1 || scale>max_gpu_render_scale || width%scale || height%scale) return false;
                const std::array<std::uint32_t,2> logical{width/scale,height/scale};
                if constexpr(std::is_same_v<T,GpuBackgroundDraw>) draw.settings.logical_viewport=logical;
                else draw.logical_viewport=logical;
                if constexpr(std::is_same_v<T,GpuModelDraw>) {
                    if(draw.pose.wave_mode) return false;
                    draw.pose.continuous_geometry=true;draw.pose.subpixel_projection=true;
                    if(draw.previous_pose) {
                        draw.previous_pose->continuous_geometry=true;
                        draw.previous_pose->subpixel_projection=true;
                    }
                }
            }
            return true;
        },item);
        if(!valid) return {};
    }
    return result;
}
bool enqueue_stereo_texture_pack(void* command,void* left,void* right,
    void* destination,StereoOutput mode,std::uint32_t width,std::uint32_t height) {
    if(stereo_overlay(mode)) return false;
    const auto layout=stereo_output_layout(mode,width,height);
    if(!layout || !command || !left || !destination || destination==left
        || (layout->eye_count==2 && (!right || destination==right || left==right))) return false;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    for(unsigned eye=0;eye<layout->eye_count;++eye) {
        SDL_GPUBlitInfo blit{};
        blit.source={static_cast<SDL_GPUTexture*>(eye?right:left),0,0,0,0,width,height};
        blit.destination={static_cast<SDL_GPUTexture*>(destination),0,0,
            layout->eyes[eye].x,layout->eyes[eye].y,layout->eyes[eye].width,layout->eyes[eye].height};
        // Only the first eye clears. Cycling or clearing the second pass can
        // discard the first eye; retain the same backing texture throughout.
        blit.load_op=eye?SDL_GPU_LOADOP_LOAD:SDL_GPU_LOADOP_CLEAR;
        blit.clear_color={0,0,0,1};
        blit.filter=(mode==StereoOutput::half_sbs || mode==StereoOutput::half_top_bottom)?SDL_GPU_FILTER_LINEAR:SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(static_cast<SDL_GPUCommandBuffer*>(command),&blit);
    }
    return true;
#else
    return false;
#endif
}
bool render_stereo_overlay(void* renderer,void* left,void* right,StereoOutput mode,
    std::uint32_t width,std::uint32_t height,std::optional<std::array<float,4>> viewport) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!renderer || !left || !right || left==right || !width || !height
        || width>32767 || height>32767
        || (!stereo_interlaced(mode) && mode!=StereoOutput::anaglyph_red_cyan)) return false;
    auto* r=static_cast<SDL_Renderer*>(renderer);
    const auto v=viewport.value_or(std::array<float,4>{0,0,float(width),float(height)});
    for(float component:v) if(!std::isfinite(component)) return false;
    if(v[0]<0 || v[1]<0 || v[2]<=0 || v[3]<=0 || v[0]+v[2]>width || v[1]+v[3]>height) return false;
    for(unsigned eye=0;eye<2;++eye) {
        auto* texture=static_cast<SDL_Texture*>(eye?right:left);
        SDL_BlendMode blend;SDL_ScaleMode filter;float red,green,blue,alpha;
        if(!SDL_GetTextureBlendMode(texture,&blend) || !SDL_GetTextureScaleMode(texture,&filter)
            || !SDL_GetTextureColorModFloat(texture,&red,&green,&blue) || !SDL_GetTextureAlphaModFloat(texture,&alpha)) return false;
        bool ok=SDL_SetTextureColorModFloat(texture,1,1,1) && SDL_SetTextureAlphaModFloat(texture,1)
            && SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR)
            && SDL_SetTextureBlendMode(texture,stereo_interlaced(mode)?SDL_BLENDMODE_NONE:SDL_BLENDMODE_ADD);
        std::vector<SDL_Vertex> vertices;
        const SDL_FColor tint=stereo_interlaced(mode)?SDL_FColor{1,1,1,1}:
            (eye?SDL_FColor{0,1,1,1}:SDL_FColor{1,0,0,1});
        const auto quad=[&](float top,float bottom) {
            const float t=(top-v[1])/v[3],b=(bottom-v[1])/v[3];
            const SDL_Vertex a{{v[0],top},tint,{0,t}},c{{v[0]+v[2],bottom},tint,{1,b}};
            const SDL_Vertex ab{{v[0]+v[2],top},tint,{1,t}},ac{{v[0],bottom},tint,{0,b}};
            vertices.insert(vertices.end(),{a,ab,c,a,c,ac});
        };
        if(stereo_interlaced(mode)) {
            vertices.reserve(std::size_t(height)*3+6);
            for(unsigned row=unsigned(std::floor(v[1]));row<unsigned(std::ceil(v[1]+v[3]));++row)
                if(((row&1U)^(mode==StereoOutput::interlaced_reversed?1U:0U))==eye)
                    quad(std::max(float(row),v[1]),std::min(float(row+1),v[1]+v[3]));
        } else quad(v[1],v[1]+v[3]);
        if(ok && !vertices.empty()) ok=SDL_RenderGeometry(r,texture,vertices.data(),int(vertices.size()),nullptr,0);
        const bool restored=SDL_SetTextureBlendMode(texture,blend) && SDL_SetTextureScaleMode(texture,filter)
            && SDL_SetTextureColorModFloat(texture,red,green,blue) && SDL_SetTextureAlphaModFloat(texture,alpha);
        if(!ok || !restored) return false;
    }
    return true;
#else
    return false;
#endif
}
std::optional<std::vector<GpuSceneDraw>> stereo_scene_eye(
    std::span<const GpuSceneDraw> frame,unsigned eye,double separation,double convergence) {
    if(eye>1 || !stereo_eye_projections(separation,convergence,1)) return {};
    std::vector<GpuSceneDraw> result(frame.begin(),frame.end());
    for(auto& draw:result) if(auto* model=std::get_if<GpuModelDraw>(&draw)) {
        const auto offsets=stereo_eye_projections(separation,convergence,model->settings.focal_length);
        if(!offsets) return {};
        model->pose.x-=(*offsets)[eye].eye_x;
        model->pose.vanish_x+=(*offsets)[eye].projection_offset_x;
        if(!std::isfinite(model->pose.x) || !std::isfinite(model->pose.vanish_x)) return {};
        // Integer native projection would round away a subpixel eye offset.
        model->pose.continuous_geometry=true;
        model->pose.subpixel_projection=true;
        if(model->previous_pose) {
            model->previous_pose->x-=(*offsets)[eye].eye_x;
            model->previous_pose->vanish_x+=(*offsets)[eye].projection_offset_x;
            if(!std::isfinite(model->previous_pose->x) || !std::isfinite(model->previous_pose->vanish_x)) return {};
            model->previous_pose->continuous_geometry=true;
            model->previous_pose->subpixel_projection=true;
        }
    } else if(auto* grid=std::get_if<GpuGridDraw>(&draw)) {
        grid->eye_x=float((eye?1:-1)*separation*.5);
        grid->convergence=float(convergence);
    } else if(auto* text=std::get_if<GpuTextDraw>(&draw)) {
        text->eye_x=float((eye?1:-1)*separation*.5);text->convergence=float(convergence);
    } else if(auto* particles=std::get_if<GpuParticleDraw>(&draw)) {
        particles->eye_x=float((eye?1:-1)*separation*.5);particles->convergence=float(convergence);
    } else if(auto* dust=std::get_if<GpuDustDraw>(&draw)) {
        dust->eye_x=float((eye?1:-1)*separation*.5);dust->convergence=float(convergence);
    }
    return result;
}
std::optional<std::array<GpuRasterOutput,2>> GpuStereoScene::enqueue(
    void* device,void* command,std::uint32_t width,std::uint32_t height,
    std::span<const GpuSceneDraw> frame,double separation,double convergence,const GpuScene::MsaaSettings* msaa) {
    resident_ready_=false;
    auto left=stereo_scene_eye(frame,0,separation,convergence);
    auto right=stereo_scene_eye(frame,1,separation,convergence);
    if(!left || !right || !device || !command || !width || !height) return {};
    std::optional<StereoSourceTopologies> sources;
    try {if(!std::getenv("STARFOX_TEST_DUPLICATE_STEREO_TOPOLOGY")) sources.emplace(*left,*right,
        8U*1024*1024,!std::getenv("STARFOX_TEST_DUPLICATE_STEREO_PROJECTION"),
        !std::getenv("STARFOX_TEST_DUPLICATE_STEREO_FACES"),
        !std::getenv("STARFOX_TEST_DUPLICATE_STEREO_RAYS"));}
    catch(const std::exception&) {return {};}
    StereoModelSourceRecording source_upload(source_uploads_,device,*left,*right);
    try {source_upload.enqueue(command);} catch(const std::exception&) {return {};}
    std::array<GpuRasterOutput,2> result;
    result[0]=eyes_[0].enqueue_batch(device,command,width,height,*left,{},msaa);
    if(!result[0].pixels) return {};
    result[1]=eyes_[1].enqueue_batch(device,command,width,height,*right,{},msaa);
    if(!result[1].pixels) return {};
    return result;
}
void GpuSceneRecording::reset(std::uint32_t width,std::uint32_t height) {
    draws_.clear();raster_.clear();width_=width;height_=height;
}
void GpuSceneRecording::flush(RasterCommands& pending) {
    if(!width_ || !height_ || pending.width()!=width_ || pending.height()!=height_)
        throw std::runtime_error("Invalid recorded scene raster dimensions");
    if(pending.commands.empty()) return;
    raster_.push_back(std::move(pending));
    pending.reset(width_,height_);
    draws_.emplace_back(GpuRasterDraw{&raster_.back(),true,true});
}
void GpuSceneRecording::append_model(RasterCommands& pending,GpuModelDraw draw) {
    const auto scale=draw.settings.render_scale;
    if(!draw.shape || scale<1 || scale>max_gpu_render_scale || width_%scale || height_%scale)
        throw std::runtime_error("Invalid recorded scene model");
    flush(pending);draws_.emplace_back(std::move(draw));
}
void GpuSceneRecording::finish(RasterCommands& pending) {flush(pending);}
void GpuSceneRecording::append_indexed_layer(RasterCommands& pending,RasterCommands source,
    const LayerCompositeSettings& settings,std::uint32_t source_scale,std::uint32_t destination_scale) {
    if(!source_scale || source_scale>max_gpu_render_scale || !destination_scale || destination_scale>max_gpu_render_scale)
        throw std::runtime_error("Invalid indexed layer scales");
    if(source.commands.empty()) return;
    flush(pending);raster_.push_back(std::move(source));
    draws_.emplace_back(GpuIndexedLayerDraw{&raster_.back(),settings,source_scale,destination_scale,{width_,height_}});
}
void GpuSceneRecording::append_background(RasterCommands& pending,GpuBackgroundDraw draw) {
    if(!draw.ppu || !draw.scale || draw.scale>max_gpu_render_scale || width_%draw.scale || height_%draw.scale
        || draw.settings.layer<1 || draw.settings.layer>3)
        throw std::runtime_error("Invalid recorded background");
    flush(pending);draws_.emplace_back(std::move(draw));
}
void GpuSceneRecording::append_text(RasterCommands& pending,GpuTextDraw draw) {
    if(!draw.scale || draw.scale>max_gpu_render_scale || width_%draw.scale || height_%draw.scale || draw.frame.glyphs.size()>256)
        throw std::runtime_error("Invalid recorded text");
    if(draw.frame.glyphs.empty()) return;
    flush(pending);draws_.emplace_back(std::move(draw));
}
void GpuSceneRecording::append_particles(RasterCommands& pending,GpuParticleDraw draw) {
    if(!draw.scale || draw.scale>max_gpu_render_scale || width_%draw.scale || height_%draw.scale
        || draw.frame.particles.size()>300)
        throw std::runtime_error("Invalid recorded particles");
    std::erase_if(draw.frame.particles,[&](const auto& p){return !p.life || p.owner!=draw.frame.owner;});
    if(draw.frame.particles.empty()) return;
    flush(pending);draws_.emplace_back(std::move(draw));
}
void GpuSceneRecording::append_dust(RasterCommands& pending,GpuDustDraw draw) {
    if(!draw.scale || draw.scale>max_gpu_render_scale || width_%draw.scale || height_%draw.scale
        || draw.frame.points.size()>simulation::kMaximumDustPoints)
        throw std::runtime_error("Invalid recorded dust");
    if(draw.frame.points.empty()) return;
    flush(pending);draws_.emplace_back(std::move(draw));
}
void GpuSceneRecording::append_grid(RasterCommands& pending,GpuGridDraw draw) {
    if(!draw.scale || draw.scale>max_gpu_render_scale || width_%draw.scale || height_%draw.scale)
        throw std::runtime_error("Invalid recorded grid scale");
    flush(pending);draws_.emplace_back(std::move(draw));
}
void GpuSceneRecording::replay(Framebuffer& frame,SurfaceBuffer* surfaces) const {
    if(frame.stored_width()!=width_ || frame.stored_height()!=height_
        || (surfaces && (surfaces->width()!=width_ || surfaces->height()!=height_)))
        throw std::runtime_error("Recorded scene replay dimensions differ");
    for(const auto& draw:draws_) {
        const auto* model=std::get_if<GpuModelDraw>(&draw);
        const auto* background=std::get_if<GpuBackgroundDraw>(&draw);
        const auto* text=std::get_if<GpuTextDraw>(&draw);
        const auto* dust=std::get_if<GpuDustDraw>(&draw);
        const auto* particles=std::get_if<GpuParticleDraw>(&draw);
        const auto* grid=std::get_if<GpuGridDraw>(&draw);
        const auto* raster=std::get_if<GpuRasterDraw>(&draw);
        if((model && (model->logical_viewport[0] || model->logical_viewport[1]))
            || (raster && raster->independent_raster_size)
            || (grid && (grid->logical_viewport[0] || grid->logical_viewport[1]))
            || (dust && (dust->logical_viewport[0] || dust->logical_viewport[1]))
            || (particles && (particles->logical_viewport[0] || particles->logical_viewport[1]))
            || (text && (text->logical_viewport[0] || text->logical_viewport[1]))
            || (background && (background->settings.logical_viewport[0] || background->settings.logical_viewport[1])))
            throw std::runtime_error("Reduced GPU scene must fall back to its original-resolution recording");
    }
    frame.record_to(nullptr);frame.clear();if(surfaces) surfaces->clear();
    // Scene initialization is transparent, not a full-screen black draw.
    if(frame.tracks_write_coverage()) frame.begin_write_coverage();
    struct RestoreScale {
        Framebuffer& frame;std::uint32_t scale;
        ~RestoreScale(){frame.set_draw_scale(scale);}
    } restore{frame,frame.draw_scale()};
    for(const auto& draw:draws_) {
        if(const auto* model=std::get_if<GpuModelDraw>(&draw)) {
            frame.set_draw_scale(model->settings.render_scale);
            SoftwareRenderer(model->settings).draw(*model->shape,model->pose,frame,false,
                model->surface_metadata?surfaces:nullptr);
        } else if(const auto* bg=std::get_if<GpuBackgroundDraw>(&draw)) {
            frame.set_draw_scale(bg->scale);const ScopedLayer tag(frame,bg->settings.tag);
            const auto& s=bg->settings;
            if(s.layer==1) BackgroundRenderer{}.draw_bg1(*bg->ppu,frame,s.priority,s.horizontal_origin,
                s.extend_horizontal,s.horizontal_inset,s.transparent_cgram_black,s.mosaic_staging_inset,s.text_outline);
            else if(s.layer==2) BackgroundRenderer{}.draw_bg2(*bg->ppu,s.scroll_x,s.scroll_y,frame,s.priority,
                s.horizontal_origin,s.extend_horizontal,s.wrap_horizontal,s.transparent_cgram_black,s.single_occurrence_top_rows,s.unique_regions,
                s.ending_star_extension,s.game_over_star_extension,s.sky_source_min);
            else BackgroundRenderer{}.draw_bg3(*bg->ppu,frame,s.priority,s.horizontal_origin,s.extend_horizontal);
        } else if(const auto* layer=std::get_if<GpuIndexedLayerDraw>(&draw)) {
            Framebuffer source(layer->commands->width()/layer->source_scale,layer->commands->height()/layer->source_scale,layer->source_scale);
            source.enable_layer_tags(true);replay_raster_commands(*layer->commands,source,nullptr);
            frame.set_draw_scale(layer->scale);composite_transparent_layer(source,frame,layer->settings);
        } else if(const auto* text=std::get_if<GpuTextDraw>(&draw)) {
            frame.set_draw_scale(text->scale);ScaledTextRenderer::draw_projected(text->frame,frame);
        } else if(const auto* particles=std::get_if<GpuParticleDraw>(&draw)) {
            frame.set_draw_scale(particles->scale);ParticleRenderer::draw_frame(particles->frame,frame);
        } else if(const auto* dust=std::get_if<GpuDustDraw>(&draw)) {
            frame.set_draw_scale(dust->scale);DustRenderer::draw_dust_frame(dust->frame,frame);
        } else if(const auto* grid=std::get_if<GpuGridDraw>(&draw)) {
            frame.set_draw_scale(grid->scale);
            const ScopedLayer layer{frame,PixelLayer::world_geometry};
            const auto points=project_source_grid(grid->camera,grid->matrix,width_/grid->scale,height_/grid->scale);
            if(grid->lines) {
                DustRenderer::draw_grid_lines_frame({points,grid->line_start},frame,grid->colour);
                continue;
            }
            for(std::size_t i=0;i<points.count;++i) {
                const auto& p=points.points[i];
                frame.set(p.x,p.y,grid->colour);
                if(p.depth<512) frame.set(p.x-1,p.y+1,grid->colour);
            }
        } else {
            const auto& raster=std::get<GpuRasterDraw>(draw);
            replay_raster_commands(*raster.commands,frame,raster.surface_metadata?surfaces:nullptr,false);
        }
    }
}
struct GpuScene::Impl {
    std::string status{"GPU scene merge unavailable"};
    std::array<std::uint64_t,4> submission_cost{};
    unsigned topology_models{},prepared_topology_models{},prepared_projection_models{};
    GpuModelUploadInfo model_uploads{};
    bool gpu_fast{};
    GpuModel models[2];
    GpuMsaa msaa[2];
    std::unique_ptr<GpuScene> msaa_layer_mapper;
    void* msaa_result{};
    GpuRaster raster;
    GpuBackground background;
    GpuProjection grid_projection;
    GpuRaster grid_raster;
    GpuRayGeometry ray_expander;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUFence* fence{};
    std::vector<SDL_GPUFence*> retired;
    // One owned stereo command references both scene instances. Each retains
    // the same fence until its resources can safely be reused/released; the
    // fence itself is released only once, by the last owner.
    struct SharedSubmission {
        SDL_GPUDevice* device{};SDL_GPUFence* fence{};
        ~SharedSubmission(){if(fence) SDL_ReleaseGPUFence(device,fence);}
    };
    std::vector<std::shared_ptr<SharedSubmission>> shared_submissions;
    SceneGpuTimestamps gpu_timestamps;
    SceneDrawTimestamps draw_timestamps;
    int timestamp_ticket{-1};
    void timestamp_submitted(SDL_GPUFence* submitted) noexcept {
        gpu_timestamps.submitted(timestamp_ticket,submitted);timestamp_ticket=-1;
        draw_timestamps.submitted(submitted);
    }
    void timestamp_cancel() noexcept {
        gpu_timestamps.cancel(timestamp_ticket);timestamp_ticket=-1;
        draw_timestamps.cancel();
    }
    void timestamp_retired(SDL_GPUFence* submitted) noexcept {
        gpu_timestamps.retired(submitted);draw_timestamps.retired(submitted);
    }
    bool owned_encoding{};
    bool batch_encoding{};
    bool batch_slot_written[2]{};
    GpuRasterOutput resident{};
    SDL_GPUBuffer* pixels[2]{},*surfaces[2]{};Uint32 capacity[2]{},surface_capacity[2]{};std::uint64_t generation{};
    SDL_GPUBuffer* depths[2]{};Uint32 depth_capacity[2]{};
    SDL_GPUBuffer* motions[2]{};Uint32 motion_capacity[2]{};
    SDL_GPUBuffer* ray_vertices{};
    SDL_GPUBuffer* msaa_palette{};
    SDL_GPUTransferBuffer* msaa_palette_upload{};
    struct RayTopology {
        std::vector<std::array<std::uint32_t,4>> triangles;
        SDL_GPUBuffer* buffer{};
        bool submitted{};
    };
    std::vector<RayTopology> ray_topologies;
    std::size_t ray_topology_bytes{};
    SDL_GPUTransferBuffer* ray_upload{};
    Uint32 ray_capacity{},topology_capacity{},ray_vertex_count{};
    SDL_GPUBuffer* ray_material_topology{};
    SDL_GPUTransferBuffer* ray_material_upload{};
    Uint32 ray_material_topology_capacity{},ray_material_offset{};
    bool ray_gpu_materials_complete{};
    RayGeometryOutput ray_output{};
    RayMaterials ray_materials;
    bool ray_materials_complete{true};
    ~Impl(){release();}
    static void require(bool value){if(!value) throw std::runtime_error(SDL_GetError());}
    void release()noexcept {
        model_uploads={};
        if(fence) {
            wait_gpu_retirement(device,true,&fence,1);
            timestamp_retired(fence);
            SDL_ReleaseGPUFence(device,fence);fence=nullptr;
        }
        if(!retired.empty()) wait_gpu_retirement(device,true,retired.data(),Uint32(retired.size()));
        for(auto* previous:retired) {timestamp_retired(previous);SDL_ReleaseGPUFence(device,previous);}
        retired.clear();
        for(const auto& shared:shared_submissions) {
            wait_gpu_retirement(device,true,&shared->fence,1);
            timestamp_retired(shared->fence);
        }
        shared_submissions.clear();
        timestamp_cancel();gpu_timestamps.release();draw_timestamps.release();
        resident={};
        msaa_result=nullptr;
        for(auto& aa:msaa) aa.release_device();
        msaa_layer_mapper.reset();
        if(msaa_palette) SDL_ReleaseGPUBuffer(device,msaa_palette);
        if(msaa_palette_upload) SDL_ReleaseGPUTransferBuffer(device,msaa_palette_upload);
        msaa_palette=nullptr;msaa_palette_upload=nullptr;
        ray_output={};ray_vertex_count=0;
        ray_expander.release_device();
        if(ray_vertices) SDL_ReleaseGPUBuffer(device,ray_vertices);
        if(ray_material_topology) SDL_ReleaseGPUBuffer(device,ray_material_topology);
        if(ray_material_upload) SDL_ReleaseGPUTransferBuffer(device,ray_material_upload);
        ray_material_topology=nullptr;ray_material_upload=nullptr;ray_material_topology_capacity=ray_material_offset=0;
        clear_ray_topologies(true);
        if(ray_upload) SDL_ReleaseGPUTransferBuffer(device,ray_upload);
        ray_vertices=nullptr;ray_upload=nullptr;ray_capacity=topology_capacity=0;
        for(auto& model:models) model.release_device();
        raster.release_device();background.release_device();
        grid_projection.release_device();grid_raster.release_device();
        for(unsigned i=0;i<2;++i){if(pixels[i]) SDL_ReleaseGPUBuffer(device,pixels[i]);if(surfaces[i]) SDL_ReleaseGPUBuffer(device,surfaces[i]);pixels[i]=surfaces[i]=nullptr;capacity[i]=surface_capacity[i]=0;}
        for(unsigned i=0;i<2;++i){if(depths[i]) SDL_ReleaseGPUBuffer(device,depths[i]);depths[i]=nullptr;depth_capacity[i]=0;}
        for(unsigned i=0;i<2;++i){if(motions[i]) SDL_ReleaseGPUBuffer(device,motions[i]);motions[i]=nullptr;motion_capacity[i]=0;}
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        pipeline=nullptr;device=nullptr;
    }
    bool pending() const noexcept {return fence || !retired.empty() || !shared_submissions.empty();}
    void clear_ray_topologies(bool all) {
        for(auto it=ray_topologies.begin();it!=ray_topologies.end();) {
            if(all || !it->submitted) {
                if(it->buffer) SDL_ReleaseGPUBuffer(device,it->buffer);
                ray_topology_bytes-=it->triangles.size()*16U;
                it=ray_topologies.erase(it);
            } else ++it;
        }
    }
    void prepare_rays(Uint32 triangles,bool materials) {
        ray_material_offset=materials?triangles*48U:0;
        ray_gpu_materials_complete=materials;
        const auto bytes=triangles*(materials?112U:48U);
        if(ray_capacity>=bytes) return;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
        auto* buffer=SDL_CreateGPUBuffer(device,&info);require(buffer);
        if(ray_vertices) SDL_ReleaseGPUBuffer(device,ray_vertices);
        ray_vertices=buffer;ray_capacity=bytes;
    }
    void append_rays(SDL_GPUCommandBuffer* command,const GpuModelRaySource& source) {
        const auto triangles=source.triangle_indices(),material_topology=source.material_indices();
        if(triangles.empty()) return;
        const auto texel_base=Uint32(ray_materials.texels.size());
        if(!source.materials_complete || (!source.materials.triangles.empty() && source.materials.triangles.size()!=triangles.size()))
            ray_materials_complete=false;
        if(ray_materials_complete) {
            const auto offset=ray_materials.texels.size();
            if(offset+source.materials.texels.size()>16'000'000) ray_materials_complete=false;
            else {
                for(auto material:source.materials.triangles) {
                    material.offset+=std::uint32_t(offset);
                    ray_materials.triangles.push_back(material);
                }
                ray_materials.texels.insert(ray_materials.texels.end(),source.materials.texels.begin(),source.materials.texels.end());
            }
        }
        const auto bytes=triangles.size_bytes();
        if(bytes>UINT32_MAX || (std::uint64_t(ray_vertex_count)+triangles.size()*3U)*16U>ray_capacity)
            throw std::runtime_error("GPU ray scene exceeds reserved topology");
        model_uploads.input_bytes+=bytes;
        auto* topology=static_cast<SDL_GPUBuffer*>(source.triangle_topology);
        if(topology) {
            model_uploads.shared_bytes+=bytes;++model_uploads.shared_buffers;
            model_uploads.ray_shared_bytes+=bytes;++model_uploads.ray_shared_buffers;
        } else {
        auto found=std::find_if(ray_topologies.begin(),ray_topologies.end(),[&](const auto& item){
            return item.triangles.size()==triangles.size() && std::equal(item.triangles.begin(),item.triangles.end(),triangles.begin());
        });
        if(found==ray_topologies.end()) {
            if(ray_topologies.size()>=128 || ray_topology_bytes+bytes>16U*1024*1024) clear_ray_topologies(true);
            ray_topologies.push_back({{triangles.begin(),triangles.end()},nullptr,false});ray_topology_bytes+=bytes;
            found=std::prev(ray_topologies.end());
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,Uint32(bytes),0};
            found->buffer=SDL_CreateGPUBuffer(device,&info);require(found->buffer);
            if(topology_capacity<bytes) {
            SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,Uint32(bytes),0};
            auto* upload=SDL_CreateGPUTransferBuffer(device,&upload_info);require(upload);
            if(ray_upload) SDL_ReleaseGPUTransferBuffer(device,ray_upload);
            ray_upload=upload;
            topology_capacity=Uint32(bytes);
            }
        auto* mapped=SDL_MapGPUTransferBuffer(device,ray_upload,true);require(mapped);
        std::memcpy(mapped,triangles.data(),bytes);SDL_UnmapGPUTransferBuffer(device,ray_upload);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation from{ray_upload,0};SDL_GPUBufferRegion to{found->buffer,0,Uint32(bytes)};
        scene_counters::upload_buffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        model_uploads.uploaded_bytes+=bytes;++model_uploads.uploaded_buffers;
        } else ++model_uploads.reused_buffers;
        topology=found->buffer;
        }
        GpuRayGeometrySettings settings;settings.triangles=Uint32(triangles.size());
        settings.points=source.point_count;settings.mode=source.mode;
        const GpuRayGeometryTarget target{ray_vertices,(ray_material_offset?ray_material_offset:ray_capacity)/16U,ray_vertex_count,ray_vertex_count==0};
        auto* expanded=static_cast<SDL_GPUBuffer*>(ray_expander.enqueue(device,command,source.points,source.residuals,topology,settings,&target));
        if(!expanded) throw std::runtime_error(ray_expander.status());
        if(ray_material_offset && ray_gpu_materials_complete) {
            if(source.reflection_excluded && ray_materials_complete) {
                const GpuRayMaterialTarget excluded_target{ray_vertices,ray_capacity,ray_material_offset+ray_vertex_count/3*64U,false};
                // Reject records on GPU without reading dummy input descriptors.
                if(!ray_expander.enqueue_materials(device,command,topology,topology,topology,topology,
                    Uint32(triangles.size()),0,0,0,0,&excluded_target,true))
                    throw std::runtime_error(ray_expander.status());
            } else if(!source.material_commands || !source.material_corners || !source.material_polygons
                || material_topology.size()!=triangles.size() || !ray_materials_complete)
                ray_gpu_materials_complete=false;
            else {
                model_uploads.input_bytes+=bytes;
                auto* connectivity=static_cast<SDL_GPUBuffer*>(source.material_connectivity);
                if(connectivity) {
                    model_uploads.shared_bytes+=bytes;++model_uploads.shared_buffers;
                    model_uploads.ray_shared_bytes+=bytes;++model_uploads.ray_shared_buffers;
                } else {
                if(ray_material_topology_capacity<bytes) {
                    SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,Uint32(bytes),0};
                    auto* next=SDL_CreateGPUBuffer(device,&info);require(next);
                    SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,Uint32(bytes),0};
                    auto* next_upload=SDL_CreateGPUTransferBuffer(device,&upload_info);
                    if(!next_upload){SDL_ReleaseGPUBuffer(device,next);require(false);}
                    if(ray_material_topology) SDL_ReleaseGPUBuffer(device,ray_material_topology);
                    if(ray_material_upload) SDL_ReleaseGPUTransferBuffer(device,ray_material_upload);
                    ray_material_topology=next;ray_material_upload=next_upload;ray_material_topology_capacity=Uint32(bytes);
                }
                auto* data=SDL_MapGPUTransferBuffer(device,ray_material_upload,true);require(data);
                std::memcpy(data,material_topology.data(),bytes);SDL_UnmapGPUTransferBuffer(device,ray_material_upload);
                auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
                SDL_GPUTransferBufferLocation from{ray_material_upload,0};SDL_GPUBufferRegion to{ray_material_topology,0,Uint32(bytes)};
                scene_counters::upload_buffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
                model_uploads.uploaded_bytes+=bytes;++model_uploads.uploaded_buffers;
                connectivity=ray_material_topology;
                }
                const GpuRayMaterialTarget material_target{ray_vertices,ray_capacity,ray_material_offset+ray_vertex_count/3*64U,false};
                const GpuRayMaterialLookup material_lookup{source.material_lookup,source.material_lookup_count};
                auto* packed=static_cast<SDL_GPUBuffer*>(ray_expander.enqueue_materials(device,command,connectivity,
                    source.material_corners,source.material_polygons,source.material_commands,Uint32(triangles.size()),
                    source.material_corner_count,source.material_count,Uint32(source.materials.texels.size()),texel_base,&material_target,false,
                    source.material_lookup?&material_lookup:nullptr));
                if(!packed) throw std::runtime_error(ray_expander.status());
            }
        }
        // Write the reserved scene range directly, avoiding a temporary output
        // and an extra GPU copy/barrier pair for every caster model.
        ray_vertex_count+=Uint32(triangles.size()*3U);
    }
    SDL_GPUBuffer* upload_msaa_palette(SDL_GPUCommandBuffer* command,std::span<const Rgba8> palette) {
        if(palette.empty() || palette.size()>256) throw std::runtime_error("Invalid MSAA palette");
        if(!msaa_palette) {
            SDL_GPUBufferCreateInfo b{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,1024,0};
            msaa_palette=SDL_CreateGPUBuffer(device,&b);require(msaa_palette);
        }
        if(!msaa_palette_upload) {
            SDL_GPUTransferBufferCreateInfo t{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,1024,0};
            msaa_palette_upload=SDL_CreateGPUTransferBuffer(device,&t);require(msaa_palette_upload);
        }
        auto* words=static_cast<Uint32*>(SDL_MapGPUTransferBuffer(device,msaa_palette_upload,true));require(words);
        std::fill_n(words,256,0U);
        for(unsigned i=0;i<palette.size();++i) {
            const auto c=palette[i];words[i]=Uint32(c.r)|(Uint32(c.g)<<8)|(Uint32(c.b)<<16)|(Uint32(c.a)<<24);
        }
        SDL_UnmapGPUTransferBuffer(device,msaa_palette_upload);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation from{msaa_palette_upload,0};SDL_GPUBufferRegion to{msaa_palette,0,1024};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);return msaa_palette;
    }
    void finish() {
        if(fence) {
            require(SDL_WaitForGPUFences(device,true,&fence,1));
            timestamp_retired(fence);
            SDL_ReleaseGPUFence(device,fence);fence=nullptr;
        }
        if(!retired.empty()) require(SDL_WaitForGPUFences(device,true,retired.data(),Uint32(retired.size())));
        for(auto* previous:retired) {timestamp_retired(previous);SDL_ReleaseGPUFence(device,previous);}
        retired.clear();
        for(const auto& shared:shared_submissions) {
            require(SDL_WaitForGPUFences(device,true,&shared->fence,1));
            timestamp_retired(shared->fence);
        }
        shared_submissions.clear();
    }
    void retire_submission() {
        if(fence) {retired.push_back(fence);fence=nullptr;}
        // Cycle-enabled buffers retain earlier commands' backing storage.
        // Bound outstanding work to two older scenes plus the next submission.
        while(!retired.empty() && (retired.size()+shared_submissions.size()>2 || SDL_QueryGPUFence(device,retired.front()))) {
            auto* previous=retired.front();require(SDL_WaitForGPUFences(device,true,&previous,1));
            timestamp_retired(previous);
            SDL_ReleaseGPUFence(device,previous);retired.erase(retired.begin());
        }
        while(!shared_submissions.empty() && (retired.size()+shared_submissions.size()>2
            || SDL_QueryGPUFence(device,shared_submissions.front()->fence))) {
            const auto& previous=shared_submissions.front();
            require(SDL_WaitForGPUFences(device,true,&previous->fence,1));
            timestamp_retired(previous->fence);
            shared_submissions.erase(shared_submissions.begin());
        }
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next && pipeline) return;
        release();device=next;
        bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        if(!spirv && !dxil && !(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL)) throw std::runtime_error("GPU scene merge requires Vulkan, Metal or D3D12");
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spirv?scene_shader::spirv:dxil?scene_shader::dxil:reinterpret_cast<const Uint8*>(scene_shader::metal);
        info.code_size=spirv?sizeof(scene_shader::spirv):dxil?sizeof(scene_shader::dxil):std::strlen(scene_shader::metal);info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_uniform_buffers=1;info.num_readonly_storage_buffers=8;info.num_readwrite_storage_buffers=4;
        info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        pipeline=create_gpu_compute_pipeline(device,&info);require(pipeline);
    }
    void allocate(unsigned slot,Uint32 count,bool surface,bool depth,bool motion) {
        const auto motion_count=motion?count:1U;
        if(motion_capacity[slot]<motion_count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,motion_count*16,0};
            auto* m=SDL_CreateGPUBuffer(device,&info);require(m);
            if(motions[slot]) SDL_ReleaseGPUBuffer(device,motions[slot]);
            motions[slot]=m;motion_capacity[slot]=motion_count;
        }
        const auto depth_count=depth?count:1U;
        if(depth_capacity[slot]<depth_count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,depth_count*4,0};
            auto* d=SDL_CreateGPUBuffer(device,&info);require(d);
            if(depths[slot]) SDL_ReleaseGPUBuffer(device,depths[slot]);
            depths[slot]=d;depth_capacity[slot]=depth_count;
        }
        const auto surface_count=surface?count:1U;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,0,0};
        if(surface_capacity[slot]<surface_count) {
            info.size=surface_count*16;
            auto* s=SDL_CreateGPUBuffer(device,&info);require(s);
            if(surfaces[slot]) SDL_ReleaseGPUBuffer(device,surfaces[slot]);
            surfaces[slot]=s;surface_capacity[slot]=surface_count;
        }
        if(capacity[slot]<count) {
            info.size=count*4;
            auto* p=SDL_CreateGPUBuffer(device,&info);require(p);
            if(pixels[slot]) SDL_ReleaseGPUBuffer(device,pixels[slot]);
            pixels[slot]=p;capacity[slot]=count;
        }
    }
#endif
};
GpuScene::GpuScene():impl_(std::make_unique<Impl>()){}
GpuScene::~GpuScene()=default;
const std::string& GpuScene::status()const noexcept{return impl_->status;}
void GpuScene::set_gpu_fast(bool enabled)noexcept{impl_->gpu_fast=enabled;}
std::array<std::uint64_t,4> GpuScene::submission_cost()const noexcept{return impl_->submission_cost;}
GpuModelUploadInfo GpuScene::model_upload_info()const noexcept {
    auto result=impl_->model_uploads;
    // Retained memo capacity belongs to the two producers, not every draw that
    // used them. Byte/copy demand remains accumulated over the whole recording.
    result.snapshot_bytes=impl_->models[0].upload_info().snapshot_bytes+impl_->models[1].upload_info().snapshot_bytes;
    return result;
}
void GpuScene::release_device()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
GpuRasterOutput GpuScene::enqueue(void* command,const GpuRasterOutput& front,const GpuRasterOutput* back,bool front_world,bool emissive,
    const GpuIndexedLayerDraw* layer,std::uint32_t output_width,std::uint32_t output_height,
    const GpuProjection::MotionSurfaceSettings* rigid_motion) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!impl_->owned_encoding && impl_->pending()) throw std::runtime_error("Finish submitted scene work before borrowed enqueue");
        impl_->resident={};
        if(layer && (!layer->source_scale || layer->source_scale>max_gpu_render_scale || !layer->scale || layer->scale>max_gpu_render_scale
            || !layer->reference_size[0] || !layer->reference_size[1]
            || !front.width || !front.height || front.width%layer->source_scale || front.height%layer->source_scale))
            throw std::runtime_error("Invalid GPU indexed layer mapping");
        const auto width=layer?output_width:front.width,height=layer?output_height:front.height;
        auto count=std::uint64_t(width)*height;
        if(!command || !front.device || !front.pixels || !count || count>UINT32_MAX/16
            || (back && (!back->pixels || back->device!=front.device || back->width!=width || back->height!=height)))
            throw std::runtime_error("Invalid GPU scene merge input");
        if(rigid_motion && (layer || !front.geometry_depth || front.motion
            || rigid_motion->width!=width || rigid_motion->height!=height
            || !GpuProjection::valid_motion_surface_settings(*rigid_motion)))
            throw std::runtime_error("Invalid deferred foreground motion input");
        impl_->initialize(static_cast<SDL_GPUDevice*>(front.device));
        unsigned slot=0;
        const auto aliases=[&](unsigned index){
            const auto matches=[&](void* p){return p && (p==impl_->pixels[index] || p==impl_->surfaces[index] || p==impl_->depths[index] || p==impl_->motions[index]);};
            return matches(front.pixels) || matches(front.surfaces) || matches(front.geometry_depth) || matches(front.motion)
                || (back && (matches(back->pixels) || matches(back->surfaces) || matches(back->geometry_depth) || matches(back->motion)));
        };
        // Null handles are not aliases of unallocated scratch.
        if(impl_->pixels[0] && aliases(0)) slot=1;
        if(impl_->pixels[slot] && aliases(slot)) throw std::runtime_error("GPU scene merge has no non-aliased scratch slot");
        const bool surface=front.surfaces || (back && back->surfaces);
        const bool depth=front.geometry_depth || (back && back->geometry_depth);
        const bool motion=rigid_motion || front.motion || (back && back->motion);
        impl_->allocate(slot,Uint32(count),surface,depth,motion);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const LayerCompositeSettings mapping=layer?layer->settings:LayerCompositeSettings{};
        struct MergeSettings {std::array<Uint32,32> compose;GpuProjection::MotionSurfaceSettings motion;};
        static_assert(sizeof(MergeSettings)==240);
        MergeSettings settings{{Uint32(count),layer?0U:front.surfaces?1U:0U,back?1U:0U,back && back->surfaces?1U:0U,
            depth?1U:0U,front.geometry_depth?1U:0U,back && back->geometry_depth?1U:0U,(front_world?1U:0U)|(emissive?2U:0U),
            motion?1U:0U,front.motion?1U:0U,back && back->motion?1U:0U,layer?1U:0U,
            width,height,front.width,front.height,
            layer?layer->source_scale:1U,layer?layer->scale:1U,layer?layer->reference_size[0]:width,layer?layer->reference_size[1]:height,
            Uint32(mapping.offset_x),Uint32(mapping.offset_y),Uint32(mapping.clip_left),Uint32(mapping.clip_top),
            Uint32(mapping.clip_right),Uint32(mapping.clip_bottom),Uint32(mapping.mosaic_origin_x),Uint32(mapping.mosaic_origin_y),
            layer && (mapping.mosaic&mapping.mosaic_layer_mask)?Uint32((mapping.mosaic>>4)+1):1U,surface?1U:0U,rigid_motion?1U:0U,0},{}};
        if(rigid_motion) settings.motion=*rigid_motion;
        const auto dispatch=linear_gpu_dispatch64(Uint32(count));
        settings.compose[31]=dispatch.row_stride;
        SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding outputs[4]{};outputs[0].buffer=impl_->pixels[slot];outputs[1].buffer=impl_->surfaces[slot];outputs[2].buffer=impl_->depths[slot];outputs[3].buffer=impl_->motions[slot];
        // The first write to each ping-pong slot may overlap a preceding
        // submitted frame, so cycle it. Later writes in this ordered command
        // can reuse the same backing instead of allocating another full-size
        // copy for every model/layer (especially costly at 4×).
        const bool cycle=!impl_->batch_encoding || !impl_->batch_slot_written[slot];
        outputs[0].cycle=outputs[1].cycle=outputs[2].cycle=outputs[3].cycle=cycle;
        auto* pass=scene_counters::begin_compute_pass(cmd,nullptr,0,outputs,4);Impl::require(pass);SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(front.pixels),static_cast<SDL_GPUBuffer*>(front.surfaces?front.surfaces:front.pixels),
            static_cast<SDL_GPUBuffer*>(back?back->pixels:front.pixels),static_cast<SDL_GPUBuffer*>(back && back->surfaces?back->surfaces:front.pixels),
            static_cast<SDL_GPUBuffer*>(front.geometry_depth?front.geometry_depth:front.pixels),
            static_cast<SDL_GPUBuffer*>(back && back->geometry_depth?back->geometry_depth:front.pixels),
            static_cast<SDL_GPUBuffer*>(front.motion?front.motion:front.pixels),
            static_cast<SDL_GPUBuffer*>(back && back->motion?back->motion:front.pixels)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,8);SDL_DispatchGPUCompute(pass,dispatch.x,dispatch.y,1);SDL_EndGPUComputePass(pass);
        scene_counters::add(scene_counters::Counter::full_frame_dispatches);
        if(impl_->batch_encoding) impl_->batch_slot_written[slot]=true;
        impl_->status="GPU scene painter merge resident";
        return {front.device,impl_->pixels[slot],surface?impl_->surfaces[slot]:nullptr,width,height,++impl_->generation,
            depth?impl_->depths[slot]:nullptr,motion?impl_->motions[slot]:nullptr};
    }catch(const std::exception& error){impl_->status=error.what();}
#endif
    return {};
}
GpuRasterOutput GpuScene::enqueue_batch(void* device,void* command,std::uint32_t width,
    std::uint32_t height,std::span<const GpuSceneDraw> draws,std::array<float,2> jitter,const MsaaSettings* msaa) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!impl_->owned_encoding && impl_->pending()) throw std::runtime_error("Finish submitted scene work before borrowed batch enqueue");
        impl_->resident={};
        impl_->msaa_result=nullptr;
        impl_->topology_models=impl_->prepared_topology_models=impl_->prepared_projection_models=0;
        impl_->model_uploads={};
        impl_->ray_output={};impl_->ray_vertex_count=0;
        impl_->ray_materials.triangles.clear();impl_->ray_materials.texels.clear();impl_->ray_materials_complete=true;
        if(!valid_raster_jitter(jitter) || !device || !command || !width || !height || std::uint64_t(width)*height>UINT32_MAX/16)
            throw std::runtime_error("Invalid GPU scene batch dimensions");
        if(msaa && ((!msaa->palette && (msaa->cpu_palette.empty() || msaa->cpu_palette.size()>256)) || !msaa_sample_count(msaa->samples)))
            throw std::runtime_error("Invalid scene MSAA settings");
        // Validate the entire layout before encoding any draws. Geometry packing
        // can still fail later; the caller must cancel, never submit a partial scene.
        std::uint64_t ray_triangles=0;bool rays_requested=false,materials_requested=false;
        bool rays_complete=std::getenv("STARFOX_DISABLE_GPU_RAY_GEOMETRY")==nullptr;
        for(const auto& draw:draws) {
            if(const auto* model=std::get_if<GpuModelDraw>(&draw)) {
                const auto scale=model->settings.render_scale;
                const bool custom=model->logical_viewport[0] || model->logical_viewport[1];
                if(!model->shape || scale<1 || scale>max_gpu_render_scale || (!custom && (width%scale || height%scale
                    || width/scale>32767 || height/scale>32767)) || (custom &&
                    (!model->logical_viewport[0] || !model->logical_viewport[1] || model->logical_viewport[0]>32767 || model->logical_viewport[1]>32767)))
                    throw std::runtime_error("Invalid GPU scene model layout");
                // Whole-object sprites are texel billboards. The reference
                // renderer's shadow-only path returns before emitting faces,
                // so they are not missing GPU casters and must not invalidate
                // the other models' resident ray scene.
                if(model->ray_geometry && !model->emissive && !model->pose.simple_scaled_sprite) {
                    rays_requested=true;
                    materials_requested|=model->ray_materials;
                    for(const auto& face:model->shape->faces)
                        if(!face.sprite && face.vertex_indices.size()>=3) ray_triangles+=face.vertex_indices.size()-2;
                    if(ray_triangles>1'000'000) throw std::runtime_error("GPU ray scene topology too large");
                }
            } else if(const auto* layer=std::get_if<GpuIndexedLayerDraw>(&draw)) {
                if(!layer->commands || !layer->source_scale || layer->source_scale>max_gpu_render_scale || !layer->scale || layer->scale>max_gpu_render_scale
                    || !layer->reference_size[0] || !layer->reference_size[1]
                    || layer->commands->width()%layer->source_scale || layer->commands->height()%layer->source_scale)
                    throw std::runtime_error("Invalid indexed scene layer");
            } else if(const auto* bg=std::get_if<GpuBackgroundDraw>(&draw)) {
                const auto logical=bg->settings.logical_viewport;
                const bool custom=logical[0] || logical[1];
                if(!bg->ppu || !bg->scale || bg->scale>max_gpu_render_scale
                    || (!custom && (width%bg->scale || height%bg->scale))
                    || (custom && (!logical[0] || !logical[1] || logical[0]>4096 || logical[1]>4096))
                    || bg->settings.layer<1 || bg->settings.layer>3)
                    throw std::runtime_error("Invalid GPU background layout");
            } else if(const auto* text=std::get_if<GpuTextDraw>(&draw)) {
                const auto logical=text->logical_viewport;
                const bool custom=logical[0] || logical[1];
                if(!text->scale || text->scale>max_gpu_render_scale
                    || (!custom && (width%text->scale || height%text->scale))
                    || (custom && (!logical[0] || !logical[1] || logical[0]>2048 || logical[1]>2048))
                    || text->frame.glyphs.size()>256)
                    throw std::runtime_error("Invalid GPU text layout");
            } else if(const auto* particles=std::get_if<GpuParticleDraw>(&draw)) {
                const auto logical=particles->logical_viewport;
                const bool custom=logical[0] || logical[1];
                if(!particles->scale || particles->scale>max_gpu_render_scale
                    || (!custom && (width%particles->scale || height%particles->scale))
                    || (custom && (logical[0]<2 || logical[1]<2 || logical[0]>32767 || logical[1]>32767))
                    || particles->frame.particles.size()>300)
                    throw std::runtime_error("Invalid GPU particle layout");
            } else if(const auto* dust=std::get_if<GpuDustDraw>(&draw)) {
                const auto logical=dust->logical_viewport;
                const bool custom=logical[0] || logical[1];
                if(!dust->scale || dust->scale>max_gpu_render_scale
                    || (!custom && (width%dust->scale || height%dust->scale))
                    || (custom && (logical[0]<2 || logical[1]<2 || logical[0]>32767 || logical[1]>32767))
                    || dust->frame.points.size()>simulation::kMaximumDustPoints
                    || std::abs(std::int64_t(dust->frame.exclude_left))>32767
                    || std::abs(std::int64_t(dust->frame.exclude_right))>32767)
                    throw std::runtime_error("Invalid GPU dust layout");
            } else if(const auto* grid=std::get_if<GpuGridDraw>(&draw)) {
                const auto logical=grid->logical_viewport;
                const bool custom=logical[0] || logical[1];
                if(!grid->scale || grid->scale>max_gpu_render_scale
                    || (!custom && (width%grid->scale || height%grid->scale))
                    || (custom && (!logical[0] || !logical[1] || logical[0]>32767 || logical[1]>32767)))
                    throw std::runtime_error("Invalid GPU grid layout");
            } else {
                const auto& raster=std::get<GpuRasterDraw>(draw);
                if(!raster.commands || !raster.commands->width() || !raster.commands->height()
                    || (!raster.independent_raster_size && (raster.commands->width()!=width || raster.commands->height()!=height)))
                    throw std::runtime_error("Invalid GPU scene raster layout");
            }
        }
        // Initialize before encoding layers: switching devices must not release
        // the first layer's newly allocated model/raster resources during merge.
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->draw_timestamps.begin_batch(impl_->owned_encoding);
        if(impl_->owned_encoding)
            impl_->timestamp_ticket=impl_->gpu_timestamps.begin(static_cast<SDL_GPUDevice*>(device),
                static_cast<SDL_GPUCommandBuffer*>(command),draws.size());
        void* aa_palette=msaa?msaa->palette:nullptr;
        if(msaa && !aa_palette) {
            aa_palette=impl_->upload_msaa_palette(static_cast<SDL_GPUCommandBuffer*>(command),msaa->cpu_palette);
        }
        // Borrowed/failed commands can be cancelled by their owner. Only
        // successful owned submissions promote uploads to persistent cache.
        impl_->clear_ray_topologies(false);
        const bool encode_rays=rays_complete && rays_requested;
        if(encode_rays && ray_triangles) impl_->prepare_rays(Uint32(ray_triangles),materials_requested);
        impl_->batch_encoding=true;
        impl_->batch_slot_written[0]=impl_->batch_slot_written[1]=false;
        struct BatchCycleGuard {
            Impl* impl;
            ~BatchCycleGuard(){impl->batch_encoding=false;}
        } cycle_guard{impl_.get()};
        GpuMsaaSamples previous_samples{};
        const auto accumulate_msaa=[&](const GpuRasterOutput& front,const GpuMsaaFaces& faces) {
            if(!msaa) return;
            auto& aa=impl_->msaa[0];
            const GpuMsaaLayer layer{front.pixels,faces.kinds,faces.triangle_count/128};
            impl_->msaa_result=aa.enqueue(device,command,nullptr,faces.triangles,aa_palette,width,height,
                faces.triangle_count,msaa->samples,previous_samples.buffer?&previous_samples:nullptr,true,
                faces.texels,faces.texel_bytes,faces.triangles!=nullptr,&layer);
            if(!impl_->msaa_result) throw std::runtime_error(aa.status());
            previous_samples=aa.sample_output();
        };
        if(draws.empty()) {
            RasterCommands empty;empty.reset(width,height);
            auto output=impl_->raster.enqueue_commands(device,command,empty,false);
            if(!output.pixels) throw std::runtime_error(impl_->raster.status());
            accumulate_msaa(output,{});
            output.msaa_color=msaa && msaa->defer_palette?nullptr:impl_->msaa_result;
            impl_->status="Empty GPU scene batch cleared resident";
            impl_->gpu_timestamps.end(impl_->timestamp_ticket,static_cast<SDL_GPUCommandBuffer*>(command));
            return output;
        }
        GpuRasterOutput output{};
        unsigned model_slot=0;
        unsigned inline_motion_models=0;
        unsigned fused_world_models=0;
        unsigned inplace_models=0;
        const bool merge_motion=std::getenv("STARFOX_TEST_SEPARATE_MODEL_MOTION")==nullptr;
        const bool fuse_world=std::getenv("STARFOX_TEST_SEPARATE_WORLD_MODEL_MERGE")==nullptr;
        const bool fused=!msaa && !std::getenv("STARFOX_TEST_SEPARATE_SCENE_MERGE");
        const bool preserve_background=!std::getenv("STARFOX_TEST_DISABLE_INPLACE_SPANS");
        // A later model may request new metadata. Do not lend an earlier model's
        // backing until it has storage for every requested model field; otherwise
        // that renderer's later copy fallback could alias its own old output.
        const bool model_surfaces=std::any_of(draws.begin(),draws.end(),[](const auto& draw) {
            const auto* m=std::get_if<GpuModelDraw>(&draw);return m && m->surface_metadata;
        });
        const bool model_depth=std::any_of(draws.begin(),draws.end(),[](const auto& draw) {
            const auto* m=std::get_if<GpuModelDraw>(&draw);return m && (m->geometry_depth || m->previous_pose);
        });
        struct ModelUploadBatch {
            GpuModel (&models)[2];
            explicit ModelUploadBatch(GpuModel (&value)[2]):models(value) {
                if(!SDL_getenv("STARFOX_TEST_DUPLICATE_MODEL_UPLOADS"))
                    for(auto& model:models) model.begin_upload_batch();
            }
            ~ModelUploadBatch(){for(auto& model:models) model.end_upload_batch();}
        } upload_batch{impl_->models};
        unsigned draw_index=0;
        int output_owner=-1;
        for(const auto& draw:draws) {
            const auto draw_ticket=impl_->draw_timestamps.begin(static_cast<SDL_GPUDevice*>(device),
                static_cast<SDL_GPUCommandBuffer*>(command),draw_index++,draw);
            struct DrawTimingGuard {
                SceneDrawTimestamps& timings;int index;SDL_GPUCommandBuffer* command;
                ~DrawTimingGuard(){timings.end(index,command);}
            } draw_timing{impl_->draw_timestamps,draw_ticket,static_cast<SDL_GPUCommandBuffer*>(command)};
            GpuRasterOutput front{};
            GpuMsaaFaces msaa_faces{};
            std::optional<GpuProjection::MotionSurfaceSettings> deferred_motion;
            bool world_sprite=false;
            if(const auto* model=std::get_if<GpuModelDraw>(&draw)) {
                scene_counters::add(scene_counters::Counter::model_draws);
                const auto scale=model->settings.render_scale;
                // Never hand a draw to the renderer whose raster buffers hold
                // the running scene. GPU FAST in-place draws can keep it there
                // across many models; that renderer's own raster (an emissive
                // or unfused model) would cycle or alias them and lose the scene.
                if(int(model_slot)==output_owner) model_slot^=1U;
                const bool custom=model->logical_viewport[0]!=0;
                world_sprite=model->geometry_depth && model->identity.has_value();
                const bool fuse_model=fused && !model->previous_pose && !output.motion
                    && (fuse_world || (!model->emissive && !world_sprite))
                    && !((model->logical_viewport[0] || jitter!=std::array<float,2>{}) && model->pose.simple_scaled_sprite);
                const bool lend_background=fuse_model && preserve_background && output.row_span_canonical
                    && (!model_surfaces || output.surfaces) && (!model_depth || output.geometry_depth);
                unsigned renderer_slot=model_slot;model_slot^=1U;
                // Sparse draws preserve the original producer's backing instead
                // of alternating output buffers. At a motion/noncanonical copy
                // transition, select the other producer BEFORE overwriting that
                // background. Otherwise front and back would become the same
                // new model image and erase earlier shadows/grid/HUD ownership.
                if(!lend_background && impl_->models[renderer_slot].output_aliases(output)) {
                    renderer_slot^=1U;
                    if(impl_->models[renderer_slot].output_aliases(output))
                        throw std::runtime_error("Model painter has no independent raster target");
                }
                auto& renderer=impl_->models[renderer_slot];
                auto pose=model->pose;auto previous=model->previous_pose;
                if(msaa || jitter!=std::array<float,2>{}) {
                    pose.continuous_geometry=pose.subpixel_projection=true;
                    if(previous) previous->continuous_geometry=previous->subpixel_projection=true;
                }
                const std::array<float,2> model_jitter{model->raster_jitter[0]+jitter[0],model->raster_jitter[1]+jitter[1]};
                GpuModelRaySource rays;
                rays.request_materials=model->ray_materials;
                rays.reference_materials=model->ray_material_reference;
                const bool casts_rays=encode_rays && model->ray_geometry && !model->emissive
                    && !pose.simple_scaled_sprite;
                const ScopedModelGpuTimingHook model_timing{draw_ticket>=0?ModelGpuTimingHook{
                    &impl_->draw_timestamps,draw_ticket,[](void* owner,int index,void* buffer) noexcept {
                        static_cast<SceneDrawTimestamps*>(owner)->mark(index,static_cast<SDL_GPUCommandBuffer*>(buffer));
                    },impl_->draw_timestamps.batch_id(),draw_index-1}:ModelGpuTimingHook{}};
                front=renderer.enqueue(device,command,*model->shape,pose,model->settings,
                    custom?model->logical_viewport[0]:width/scale,custom?model->logical_viewport[1]:height/scale,model->surface_metadata,fuse_model && output.pixels?&output:nullptr,nullptr,model->geometry_depth,
                    casts_rays?&rays:nullptr,previous?&*previous:nullptr,model_jitter,
                    custom?std::array<std::uint32_t,2>{width,height}:std::array<std::uint32_t,2>{},msaa?&msaa_faces:nullptr,msaa?msaa->samples:8,
                    merge_motion?&deferred_motion:nullptr,
                    fuse_model?((world_sprite?1U:0U)|(model->emissive?2U:0U)):0U,
                    lend_background,model->prepared_topology,model->prepared_projection,model->prepared_gpu,model->prepared_faces,model->prepared_rays,
                    !msaa && fuse_model && output.pixels && model->bounded_raster,model->bounded_raster);
                if(!front.pixels) throw std::runtime_error(renderer.status());
                impl_->model_uploads=add_model_uploads(impl_->model_uploads,renderer.upload_info());
                if(!pose.simple_scaled_sprite && !pose.collapse_to_axis_line) {
                    ++impl_->topology_models;
                    if(model->prepared_topology) ++impl_->prepared_topology_models;
                    if(model->prepared_projection) ++impl_->prepared_projection_models;
                }
                if(casts_rays) {
                    if(!rays.points) {
                        if(std::getenv("STARFOX_TRACE_GPU_RAYS")) std::cerr<<"ray-scene missing producer: "<<model->shape->name<<" status="<<renderer.status()<<'\n';
                        rays_complete=false;
                    }
                    else impl_->append_rays(static_cast<SDL_GPUCommandBuffer*>(command),rays);
                }
                if(fuse_model) {
                    if(output.pixels && front.pixels==output.pixels) ++inplace_models;
                    if(world_sprite || model->emissive) ++fused_world_models;
                    if(front.pixels!=output.pixels) output_owner=int(renderer_slot);
                    output=front;continue;
                }
            } else if(const auto* layer=std::get_if<GpuIndexedLayerDraw>(&draw)) {
                front=impl_->raster.enqueue_commands(device,command,*layer->commands,false,true);
                if(!front.pixels) throw std::runtime_error(impl_->raster.status());
                if(msaa) {
                    if(!impl_->msaa_layer_mapper) impl_->msaa_layer_mapper=std::make_unique<GpuScene>();
                    const auto mapped=impl_->msaa_layer_mapper->enqueue(command,front,nullptr,false,false,layer,width,height);
                    if(!mapped.pixels) throw std::runtime_error(impl_->msaa_layer_mapper->status());
                    accumulate_msaa(mapped,{});
                }
                const auto back=output;
                output=enqueue(command,front,back.pixels?&back:nullptr,false,false,layer,width,height);output_owner=-1;
                if(!output.pixels) throw std::runtime_error(impl_->status);
                continue;
            } else if(const auto* bg=std::get_if<GpuBackgroundDraw>(&draw)) {
                auto settings=bg->settings;settings.raster_jitter[0]+=jitter[0];settings.raster_jitter[1]+=jitter[1];
                front=impl_->background.enqueue(device,command,*bg->ppu,width,height,bg->scale,settings);
                if(!front.pixels) throw std::runtime_error(impl_->background.status());
            } else if(const auto* text=std::get_if<GpuTextDraw>(&draw)) {
                auto& projection=impl_->grid_projection;
                auto* pixels=projection.enqueue_text(device,command,text->frame,width,height,text->scale,
                    static_cast<std::uint8_t>(PixelLayer::three_d),text->eye_x,text->convergence,text->logical_viewport,jitter);
                if(!pixels) throw std::runtime_error(projection.status());
                front={device,pixels,nullptr,width,height};
            } else if(const auto* particles=std::get_if<GpuParticleDraw>(&draw)) {
                const auto& frame=particles->frame;
                const auto count=std::count_if(frame.particles.begin(),frame.particles.end(),
                    [&](const auto& p){return p.life && p.owner==frame.owner;});
                if(!count) continue;
                auto& projection=impl_->grid_projection;
                const auto logical=particles->logical_viewport;
                const bool custom=logical[0]!=0;
                const std::array<std::uint32_t,3> mapping=custom?std::array<std::uint32_t,3>{logical[0],logical[1],width}:std::array<std::uint32_t,3>{};
                if(!projection.enqueue_particle_frame(device,command,frame,custom?logical[0]:width/particles->scale,custom?logical[1]:height/particles->scale,
                    particles->eye_x,particles->convergence)) throw std::runtime_error(projection.status());
                auto* spans=projection.enqueue_particle_spans(command,height,particles->scale,
                    static_cast<std::uint8_t>(PixelLayer::three_d),frame.pose.effect_clip_left,frame.pose.effect_clip_right,mapping,jitter);
                if(!spans) throw std::runtime_error(projection.status());
                front=impl_->grid_raster.enqueue_row_spans(device,command,spans,std::uint32_t(count),width,height,false,nullptr,true,
                    nullptr,false,0,0,0,nullptr,{},{},0,false,false,impl_->gpu_fast);
                if(!front.pixels) throw std::runtime_error(impl_->grid_raster.status());
            } else if(const auto* dust=std::get_if<GpuDustDraw>(&draw)) {
                if(dust->frame.points.empty()) continue;
                auto& projection=impl_->grid_projection;
                const auto logical=dust->logical_viewport;
                const bool custom=logical[0]!=0;
                const std::array<std::uint32_t,3> mapping=custom?std::array<std::uint32_t,3>{logical[0],logical[1],width}:std::array<std::uint32_t,3>{};
                if(!projection.enqueue_dust_frame(device,command,dust->frame,custom?logical[0]:width/dust->scale,custom?logical[1]:height/dust->scale,dust->eye_x,dust->convergence))
                    throw std::runtime_error(projection.status());
                auto* spans=projection.enqueue_dust_spans(command,height,dust->scale,
                    static_cast<std::uint8_t>(PixelLayer::world_geometry),std::int16_t(dust->frame.exclude_left),std::int16_t(dust->frame.exclude_right),mapping,jitter);
                if(!spans) throw std::runtime_error(projection.status());
                front=impl_->grid_raster.enqueue_row_spans(device,command,spans,std::uint32_t(dust->frame.points.size()),width,height,false,nullptr,true,
                    nullptr,false,0,0,0,nullptr,{},{},0,false,false,impl_->gpu_fast);
                if(!front.pixels) throw std::runtime_error(impl_->grid_raster.status());
            } else if(const auto* grid=std::get_if<GpuGridDraw>(&draw)) {
                auto& projection=impl_->grid_projection;
                const auto logical=grid->logical_viewport;
                const bool custom=logical[0]!=0;
                const std::array<std::uint32_t,3> mapping=custom?std::array<std::uint32_t,3>{logical[0],logical[1],width}:std::array<std::uint32_t,3>{};
                if(!projection.enqueue_grid(device,command,source_grid_lattice(grid->camera,grid->matrix),
                    custom?logical[0]:width/grid->scale,custom?logical[1]:height/grid->scale,grid->eye_x,grid->convergence))
                    throw std::runtime_error(projection.status());
                auto* spans=projection.enqueue_grid_spans(command,height,grid->scale,grid->colour,
                    static_cast<std::uint8_t>(PixelLayer::world_geometry),grid->lines?grid->line_start.data():nullptr,mapping,jitter);
                if(!spans) throw std::runtime_error(projection.status());
                front=impl_->grid_raster.enqueue_row_spans(device,command,spans,grid->lines?675:225,width,height,false,nullptr,true,
                    nullptr,false,0,0,0,nullptr,{},{},0,false,false,impl_->gpu_fast);
                if(!front.pixels) throw std::runtime_error(impl_->grid_raster.status());
            } else {
                const auto& raster=std::get<GpuRasterDraw>(draw);
                front=impl_->raster.enqueue_commands(device,command,*raster.commands,
                    raster.surface_metadata,raster.gpu_binning,
                    raster.independent_raster_size?std::array<std::uint32_t,2>{width,height}:std::array<std::uint32_t,2>{},jitter,raster.prepared_row_bins);
                if(!front.pixels) throw std::runtime_error(impl_->raster.status());
            }
            accumulate_msaa(front,msaa_faces);
            const auto back=output;
            const auto* model_draw=std::get_if<GpuModelDraw>(&draw);
            output_owner=-1;
            output=enqueue(command,front,back.pixels?&back:nullptr,world_sprite,
                model_draw && model_draw->emissive,nullptr,0,0,deferred_motion?&*deferred_motion:nullptr);
            if(!output.pixels) throw std::runtime_error(impl_->status);
            if(deferred_motion) ++inline_motion_models;
        }
        if(!output.pixels) {
            // A nonempty list may consist entirely of inactive particle/dust draws.
            // It must clear just like an empty list, not retain the previous frame.
            RasterCommands empty;empty.reset(width,height);
            output=impl_->raster.enqueue_commands(device,command,empty,false);
            if(!output.pixels) throw std::runtime_error(impl_->raster.status());
            accumulate_msaa(output,{});
        }
        impl_->status="Ordered mixed GPU scene batch resident";
        if(inline_motion_models && std::getenv("STARFOX_TRACE_GPU"))
            std::cerr<<"scene-motion-merge: models="<<inline_motion_models<<'\n';
        if(fused_world_models && std::getenv("STARFOX_TRACE_GPU"))
            std::cerr<<"scene-world-raster-merge: models="<<fused_world_models<<'\n';
        if(inplace_models && std::getenv("STARFOX_TRACE_GPU"))
            std::cerr<<"scene-inplace-raster: models="<<inplace_models<<'\n';
        output.msaa_color=msaa && msaa->defer_palette?nullptr:impl_->msaa_result;
        if(rays_requested && rays_complete)
            impl_->ray_output={device,impl_->ray_vertex_count?impl_->ray_vertices:nullptr,impl_->ray_vertex_count,true,
                impl_->ray_materials_complete && (impl_->ray_gpu_materials_complete || impl_->ray_materials.triangles.size()*3==impl_->ray_vertex_count)
                    ? &impl_->ray_materials : nullptr,impl_->ray_gpu_materials_complete?impl_->ray_material_offset:0};
        impl_->gpu_timestamps.end(impl_->timestamp_ticket,static_cast<SDL_GPUCommandBuffer*>(command));
        return output;
    } catch(const std::exception& error) {impl_->msaa_result=nullptr;impl_->ray_output={};impl_->status=error.what();}
#endif
    return {};
}
bool GpuScene::render_resident(void* device,std::uint32_t width,std::uint32_t height,
    std::span<const GpuSceneDraw> draws,std::array<float,2> jitter,const MsaaSettings* msaa,GpuModelSourcePool* source_upload) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUCommandBuffer* command=nullptr;
    try {
        const bool trace_all=std::getenv("STARFOX_TRACE_SCENE_COST")!=nullptr;
        const auto* slow_text=std::getenv("STARFOX_TRACE_SLOW_FRAME_US");
        const auto slow_us=slow_text?std::strtoull(slow_text,nullptr,10):0ULL;
        const bool profile=trace_all || slow_us!=0;
        impl_->submission_cost={};
        const auto begin=profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        if(std::getenv("STARFOX_TEST_SERIAL_SCENE")) impl_->finish();
        else impl_->retire_submission();
        const auto retired=profile?std::chrono::steady_clock::now():begin;
        impl_->resident={};
        if(!device) throw std::runtime_error("Missing GPU scene device");
        command=SDL_AcquireGPUCommandBuffer(static_cast<SDL_GPUDevice*>(device));Impl::require(command);
        if(source_upload && !source_upload->enqueue(command)) throw std::runtime_error(source_upload->status());
        impl_->owned_encoding=true;
        const auto output=enqueue_batch(device,command,width,height,draws,jitter,msaa);
        const auto encoded=profile?std::chrono::steady_clock::now():begin;
        impl_->owned_encoding=false;
        if(!output.pixels) throw std::runtime_error(impl_->status);
        auto* submitted=command;command=nullptr;
        impl_->fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);Impl::require(impl_->fence);
        impl_->timestamp_submitted(impl_->fence);
        for(auto& topology:impl_->ray_topologies) topology.submitted=true;
        if(profile) {
            const auto submitted_at=std::chrono::steady_clock::now();
            const auto us=[](auto a,auto b){return std::chrono::duration_cast<std::chrono::microseconds>(b-a).count();};
            impl_->submission_cost={std::uint64_t(us(begin,retired)),std::uint64_t(us(retired,encoded)),
                std::uint64_t(us(encoded,submitted_at)),draws.size()};
            if(trace_all)
                std::cerr<<"scene-cost-us retire="<<us(begin,retired)<<" encode="<<us(retired,encoded)
                    <<" submit="<<us(encoded,submitted_at)<<" draws="<<draws.size()<<'\n';
        }
        impl_->resident=output;
        impl_->status="Ordered GPU scene submitted resident";
        return true;
    } catch(const std::exception& error) {
        impl_->owned_encoding=false;
        if(command) SDL_CancelGPUCommandBuffer(command);
        impl_->timestamp_cancel();
        impl_->msaa_result=nullptr;impl_->resident={};impl_->ray_output={};impl_->status=error.what();
    }
#else
    (void)device;(void)width;(void)height;(void)draws;
#endif
    return false;
}
bool GpuStereoScene::render_resident(void* device,std::uint32_t width,std::uint32_t height,
    std::span<const GpuSceneDraw> frame,double separation,double convergence,const GpuScene::MsaaSettings* msaa) {
    resident_ready_=false;
    auto left=stereo_scene_eye(frame,0,separation,convergence);
    auto right=stereo_scene_eye(frame,1,separation,convergence);
    if(!left || !right || !device || !width || !height) return false;
    std::optional<StereoSourceTopologies> sources;
    try {if(!std::getenv("STARFOX_TEST_DUPLICATE_STEREO_TOPOLOGY")) sources.emplace(*left,*right,
        8U*1024*1024,!std::getenv("STARFOX_TEST_DUPLICATE_STEREO_PROJECTION"),
        !std::getenv("STARFOX_TEST_DUPLICATE_STEREO_FACES"),
        !std::getenv("STARFOX_TEST_DUPLICATE_STEREO_RAYS"));}
    catch(const std::exception& error) {
        for(auto& eye:eyes_) eye.impl_->status=error.what();
        return false;
    }
    StereoModelSourceRecording source_upload(source_uploads_,device,*left,*right);
    const auto report_sources=[&] {
        if(!std::getenv("STARFOX_TEST_STEREO_RESULT")) return;
        const auto models=eyes_[0].impl_->topology_models+eyes_[1].impl_->topology_models;
        const auto prepared=eyes_[0].impl_->prepared_topology_models+eyes_[1].impl_->prepared_topology_models;
        const int policy=sources?1:0;
        static thread_local int reported=-1;
        if(models && reported!=policy) {
            std::cerr<<"stereo-topology-policy: "<<(policy?"pair-shared":"duplicated")
                <<" sources="<<(sources?sources->source_count():models)<<" models="<<models<<" prepared="<<prepared<<'\n';
            reported=policy;
        }
        const auto projection=eyes_[0].impl_->prepared_projection_models+eyes_[1].impl_->prepared_projection_models;
        const int projection_policy=sources && !std::getenv("STARFOX_TEST_DUPLICATE_STEREO_PROJECTION")?1:0;
        static thread_local int projection_reported=-1;
        if(models && projection_reported!=projection_policy) {
            std::cerr<<"stereo-projection-policy: "<<(projection_policy?"pair-shared":"duplicated")
                <<" sources="<<(projection_policy?sources->projection_source_count():models)
                <<" models="<<models<<" prepared="<<projection<<'\n';
            projection_reported=projection_policy;
        }
        const auto uploads=model_upload_info();
        if(std::getenv("STARFOX_TRACE_STEREO_INPUT_UPLOADS")) {
            std::cerr<<"stereo-model-uploads: input="<<uploads.input_bytes<<" uploaded="<<uploads.uploaded_bytes
                <<" shared="<<uploads.shared_bytes<<" copies="<<uploads.uploaded_buffers
                <<" storage="<<uploads.source_storage_bytes<<'\n';
            std::cerr<<"stereo-ray-uploads: shared="<<uploads.ray_shared_bytes<<" buffers="<<uploads.ray_shared_buffers<<'\n';
            std::cerr<<"stereo-pose-uniforms: bytes="<<uploads.pose_uniform_bytes<<" pushes="<<uploads.pose_uniform_pushes<<'\n';
        }
        const int face_policy=sources && !std::getenv("STARFOX_TEST_DUPLICATE_STEREO_FACES")?1:0;
        static thread_local int face_reported=-1;
        if(models && face_reported!=face_policy) {
            std::cerr<<"stereo-face-policy: "<<(face_policy?"pair-shared":"duplicated")
                <<" sources="<<(sources?sources->face_source_count():0)
                <<" prepared="<<(sources?sources->face_model_count():0)<<'\n';
            face_reported=face_policy;
        }
        if(std::getenv("STARFOX_TRACE_STEREO_INPUT_UPLOADS"))
            std::cerr<<"stereo-ray-source-policy: "<<(sources && !std::getenv("STARFOX_TEST_DUPLICATE_STEREO_RAYS")?"pair-shared":"duplicated")
                <<" sources="<<(sources?sources->ray_source_count():0)
                <<" prepared="<<(sources?sources->ray_model_count():0)<<'\n';
        const int upload_policy=uploads.shared_bytes?1:0;
        static thread_local int upload_reported=-1;
        if(models && upload_reported!=upload_policy) {
            std::cerr<<"stereo-source-upload-policy: "<<(upload_policy?"pair-shared":"duplicated")
                <<" input="<<uploads.input_bytes<<" uploaded="<<uploads.uploaded_bytes
                <<" shared="<<uploads.shared_bytes<<" copies="<<uploads.uploaded_buffers
                <<" storage="<<uploads.source_storage_bytes<<'\n';
            upload_reported=upload_policy;
        }
    };
    // Retain the original path as a same-binary diagnostic reference while
    // measuring joined submission or parallel encoding on actual adapters.
#if defined(STARFOX_SDL_GPU_EFFECTS)
    const bool joined=SDL_getenv("STARFOX_TEST_JOINED_STEREO_SUBMISSIONS")
        && !SDL_getenv("STARFOX_TEST_SPLIT_STEREO_SUBMISSIONS");
#else
    constexpr bool joined=false;
#endif
#if defined(STARFOX_SDL_GPU_EFFECTS)
    const bool parallel=!joined && SDL_getenv("STARFOX_TEST_PARALLEL_STEREO_ENCODING")
        && !SDL_getenv("STARFOX_TEST_SERIAL_STEREO_ENCODING");
    if(parallel) {
        SDL_GPUCommandBuffer* command=nullptr;
        try {
            const bool trace=std::getenv("STARFOX_TRACE_SCENE_COST")!=nullptr;
            const bool profile=trace || std::getenv("STARFOX_TRACE_SLOW_FRAME_US")!=nullptr;
            const auto now=[&]{return profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};};
            const auto us=[](auto a,auto b){return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(b-a).count());};
            const auto begin=now();
            for(auto& eye:eyes_) {
                eye.impl_->submission_cost={};eye.impl_->resident={};eye.impl_->ray_output={};
                if(std::getenv("STARFOX_TEST_SERIAL_SCENE")) eye.impl_->finish();
                else eye.impl_->retire_submission();
            }
            if(!parallel_) parallel_=std::make_unique<Parallel>();
            // Legacy chunks are shared between eye views. CPU bin_rows()
            // mutates their vectors; two concurrent encoders must never race
            // that mutation against each other's uploads. Prepare each chunk
            // once before the worker starts, then both readers upload it as-is.
            if(!SDL_getenv("STARFOX_TEST_GPU_BINS")) {
                std::vector<RasterCommands*> prepared;
                for(auto* eye_frame:{&*left,&*right}) for(auto& draw:*eye_frame)
                    if(auto* raster=std::get_if<GpuRasterDraw>(&draw);raster && !raster->gpu_binning) {
                        if(!raster->commands) throw std::runtime_error("Missing shared raster commands");
                        if(!raster->prepared_row_bins
                            && std::find(prepared.begin(),prepared.end(),raster->commands)==prepared.end()) {
                            raster->commands->bin_rows();prepared.push_back(raster->commands);
                        }
                        raster->prepared_row_bins=true;
                    }
            }
            const auto retired=now();
            std::array<GpuRasterOutput,2> outputs{};
            StereoEncodingDecision decision;
            // Freeze the pool's current backing before either thread binds it.
            // The worker still submits only after this left command succeeds.
            command=SDL_AcquireGPUCommandBuffer(static_cast<SDL_GPUDevice*>(device));GpuScene::Impl::require(command);
            source_upload.enqueue(command);
            // SDL command buffers cannot move between threads. The worker
            // acquires, records and submits/cancels only its own right buffer.
            // No outputs are published until BOTH encodes and submits succeed.
            parallel_->start([&] {
                auto& eye=eyes_[1];SDL_GPUCommandBuffer* right_command=nullptr;
                struct CancelRight {
                    SDL_GPUCommandBuffer*& command;
                    ~CancelRight(){if(command) SDL_CancelGPUCommandBuffer(command);}
                } cancel{right_command};
                {
                  struct Ready {
                    StereoEncodingDecision& decision;
                    ~Ready(){decision.encoded();}
                  } ready{decision};
                  try {
                    right_command=SDL_AcquireGPUCommandBuffer(static_cast<SDL_GPUDevice*>(device));GpuScene::Impl::require(right_command);
                    eye.impl_->owned_encoding=true;
                    outputs[1]=eye.enqueue_batch(device,right_command,width,height,*right,{},msaa);
                    eye.impl_->owned_encoding=false;
                    if(!outputs[1].pixels) throw std::runtime_error(eye.status());
                    if(SDL_getenv("STARFOX_TEST_FAIL_STEREO_ENCODING_RIGHT"))
                        throw std::runtime_error("Injected right eye encoding failure");
                } catch(const std::exception& error) {
                    outputs[1]={};eye.impl_->owned_encoding=false;eye.impl_->status=error.what();
                  } catch(...) {
                    outputs[1]={};eye.impl_->owned_encoding=false;eye.impl_->status="Unexpected right eye encoding failure";
                  }
                }
                const bool submit=decision.wait_decision() && outputs[1].pixels;
                try {
                    if(submit) {
                        auto* submitted=right_command;right_command=nullptr;
                        eye.impl_->fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);GpuScene::Impl::require(eye.impl_->fence);
                        eye.impl_->timestamp_submitted(eye.impl_->fence);
                        for(auto& topology:eye.impl_->ray_topologies) topology.submitted=true;
                        eye.impl_->resident=outputs[1];eye.impl_->status="Parallel right GPU scene submitted resident";
                    } else {
                        if(right_command) {SDL_CancelGPUCommandBuffer(right_command);right_command=nullptr;}
                        eye.impl_->timestamp_cancel();
                        outputs[1]={};eye.impl_->msaa_result=nullptr;eye.impl_->resident={};eye.impl_->ray_output={};
                    }
                } catch(const std::exception& error) {
                    if(right_command) {SDL_CancelGPUCommandBuffer(right_command);right_command=nullptr;}
                    eye.impl_->timestamp_cancel();
                    outputs[1]={};eye.impl_->msaa_result=nullptr;eye.impl_->resident={};eye.impl_->ray_output={};eye.impl_->status=error.what();
                }
            });
            struct JoinRight {
                Parallel& worker;StereoEncodingDecision& decision;bool joined{};
                ~JoinRight(){if(!joined){decision.decide(false);try {worker.wait();} catch(...) {}}}
                void finish(){worker.wait();joined=true;}
            } join{*parallel_,decision};
            auto& eye=eyes_[0];
            eye.impl_->owned_encoding=true;
            outputs[0]=eye.enqueue_batch(device,command,width,height,*left,{},msaa);
            eye.impl_->owned_encoding=false;
            if(!outputs[0].pixels) throw std::runtime_error(eye.status());
            if(SDL_getenv("STARFOX_TEST_FAIL_STEREO_ENCODING_AFTER_LEFT"))
                throw std::runtime_error("Injected failure after left eye encoding");
            decision.wait_encoded();
            if(!outputs[1].pixels) throw std::runtime_error(eyes_[1].status());
            const auto encoded=now();
            auto* submitted=command;command=nullptr;
            eye.impl_->fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);GpuScene::Impl::require(eye.impl_->fence);
            eye.impl_->timestamp_submitted(eye.impl_->fence);
            for(auto& topology:eye.impl_->ray_topologies) topology.submitted=true;
            eye.impl_->resident=outputs[0];eye.impl_->status="Parallel left GPU scene submitted resident";
            decision.decide(true);join.finish();
            if(!outputs[1].pixels) throw std::runtime_error(eyes_[1].status());
            if(profile) {
                const auto completed=now();
                // Report wall intervals, not the sum of overlapping eye CPU
                // encodes. Draw counts still include both independent eyes.
                eyes_[0].impl_->submission_cost={us(begin,retired),us(retired,encoded),us(encoded,completed),left->size()};
                eyes_[1].impl_->submission_cost={0,0,0,right->size()};
                if(trace) std::cerr<<"stereo-scene-cost-us retire="<<us(begin,retired)<<" encode="<<us(retired,encoded)
                    <<" submit="<<us(encoded,completed)<<" draws="<<(left->size()+right->size())<<" submissions=2 parallel=1\n";
            }
            static bool reported=false;
            if(!reported && SDL_getenv("STARFOX_TEST_STEREO_RESULT")) {
                std::cerr<<"stereo-scene-submission: parallel\n";reported=true;
            }
            resident_ready_=true;report_sources();return true;
        } catch(const std::exception& error) {
            if(command) SDL_CancelGPUCommandBuffer(command);
            for(auto& eye:eyes_) {
                eye.impl_->timestamp_cancel();
                eye.impl_->owned_encoding=false;eye.impl_->resident={};eye.impl_->ray_output={};
                eye.impl_->msaa_result=nullptr;eye.impl_->status=error.what();
            }
            return false;
        }
    }
#endif
    if(!joined) {
        if(!eyes_[0].render_resident(device,width,height,*left,{},msaa,source_upload.pool)
            || !eyes_[1].render_resident(device,width,height,*right,{},msaa)) return false;
#if defined(STARFOX_SDL_GPU_EFFECTS)
        static bool split_reported=false;
        if(!split_reported && SDL_getenv("STARFOX_TEST_STEREO_RESULT")) {
            std::cerr<<"stereo-scene-submission: split\n";split_reported=true;
        }
#endif
        resident_ready_=true;report_sources();return true;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUCommandBuffer* command=nullptr;
    try {
        const bool trace=std::getenv("STARFOX_TRACE_SCENE_COST")!=nullptr;
        const bool profile=trace || std::getenv("STARFOX_TRACE_SLOW_FRAME_US")!=nullptr;
        const auto now=[&]{return profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};};
        const auto us=[](auto a,auto b){return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(b-a).count());};
        const auto begin=now();
        for(auto& eye:eyes_) {
            eye.impl_->submission_cost={};eye.impl_->resident={};eye.impl_->ray_output={};
            if(std::getenv("STARFOX_TEST_SERIAL_SCENE")) eye.impl_->finish();
            else eye.impl_->retire_submission();
        }
        const auto retired=now();
        command=SDL_AcquireGPUCommandBuffer(static_cast<SDL_GPUDevice*>(device));GpuScene::Impl::require(command);
        source_upload.enqueue(command);
        std::array<GpuRasterOutput,2> outputs{};
        for(auto& eye:eyes_) eye.impl_->owned_encoding=true;
        outputs[0]=eyes_[0].enqueue_batch(device,command,width,height,*left,{},msaa);
        if(!outputs[0].pixels) throw std::runtime_error(eyes_[0].status());
        const auto left_encoded=now();
        // Only diagnostics can inject a fault here. It exercises cancellation
        // after a fully encoded eye without ever submitting that partial pair.
        if(SDL_getenv("STARFOX_TEST_FAIL_STEREO_ENCODING_AFTER_LEFT"))
            throw std::runtime_error("Injected failure after left eye encoding");
        outputs[1]=eyes_[1].enqueue_batch(device,command,width,height,*right,{},msaa);
        if(!outputs[1].pixels) throw std::runtime_error(eyes_[1].status());
        const auto encoded=now();
        for(auto& eye:eyes_) {
            eye.impl_->owned_encoding=false;
            eye.impl_->shared_submissions.reserve(eye.impl_->shared_submissions.size()+1);
        }
        // Allocate all host ownership before submission: no allocation failure
        // can leave an untracked in-flight pair or expose just one new eye.
        auto submission=std::make_shared<GpuScene::Impl::SharedSubmission>();
        submission->device=static_cast<SDL_GPUDevice*>(device);
        auto* submitted=command;command=nullptr;
        submission->fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);GpuScene::Impl::require(submission->fence);
        for(unsigned eye=0;eye<2;++eye) {
            eyes_[eye].impl_->timestamp_submitted(submission->fence);
            eyes_[eye].impl_->shared_submissions.push_back(submission);
            for(auto& topology:eyes_[eye].impl_->ray_topologies) topology.submitted=true;
            eyes_[eye].impl_->resident=outputs[eye];
            eyes_[eye].impl_->status="Stereo GPU scene submitted jointly resident";
        }
        if(profile) {
            const auto completed=now();
            eyes_[0].impl_->submission_cost={us(begin,retired),us(retired,left_encoded),us(encoded,completed),left->size()};
            eyes_[1].impl_->submission_cost={0,us(left_encoded,encoded),0,right->size()};
            if(trace) std::cerr<<"stereo-scene-cost-us retire="<<us(begin,retired)<<" encode="<<us(retired,encoded)
                <<" submit="<<us(encoded,completed)<<" draws="<<(left->size()+right->size())<<" submissions=1\n";
        }
        static bool joined_reported=false;
        if(!joined_reported && SDL_getenv("STARFOX_TEST_STEREO_RESULT")) {
            std::cerr<<"stereo-scene-submission: joined\n";joined_reported=true;
        }
        resident_ready_=true;report_sources();return true;
    } catch(const std::exception& error) {
        if(command) SDL_CancelGPUCommandBuffer(command);
        for(auto& eye:eyes_) {
            eye.impl_->timestamp_cancel();
            eye.impl_->owned_encoding=false;eye.impl_->resident={};eye.impl_->ray_output={};
            eye.impl_->msaa_result=nullptr;eye.impl_->status=error.what();
        }
    }
#endif
    return false;
}
std::array<std::uint64_t,4> GpuStereoScene::submission_cost() const noexcept {
    std::array<std::uint64_t,4> total{};
    for(const auto& eye:eyes_) {
        const auto cost=eye.submission_cost();
        for(unsigned field=0;field<total.size();++field) total[field]+=cost[field];
    }
    return total;
}
GpuModelUploadInfo GpuStereoScene::model_upload_info() const noexcept {
    auto result=add_model_uploads(eyes_[0].model_upload_info(),eyes_[1].model_upload_info());
    // Input demand counts model consumers, not the deduplicated pool twice.
    // Actual upload bytes/copies include the one immutable pool copy pass.
    if(source_uploads_) {
        const auto sources=source_uploads_->upload_info();
        result.uploaded_bytes+=sources.uploaded_bytes;result.uploaded_buffers+=sources.uploaded_buffers;
        result.source_storage_bytes=sources.source_storage_bytes;
    }
    return result;
}
bool GpuStereoScene::wait_for_completion() {
    // Evaluate both, even when the first failed. Each may retain shared or
    // reference-path submissions that must retire before borrowed enqueue.
    const bool left=eyes_[0].wait_for_completion();
    const bool right=eyes_[1].wait_for_completion();
    return left && right;
}
unsigned GpuStereoScene::msaa_samples() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!resident_ready_ || !eyes_[0].impl_->msaa_result || !eyes_[1].impl_->msaa_result) return 0;
    const auto left=eyes_[0].impl_->msaa[0].sample_output().count;
    const auto right=eyes_[1].impl_->msaa[0].sample_output().count;
    return left==right?left:0;
#else
    return 0;
#endif
}
bool GpuStereoScene::resolve_msaa_palette(std::span<const Rgba8> palette) {
    if(!resident_ready_ || !msaa_samples()) return false;
    const bool left=eyes_[0].resolve_msaa_palette(palette);
#if defined(STARFOX_SDL_GPU_EFFECTS)
    const bool fail_right=SDL_getenv("STARFOX_TEST_FAIL_STEREO_MSAA_RESOLVE_RIGHT")!=nullptr;
#else
    constexpr bool fail_right=false;
#endif
    // Retire/resolve both owners even when the first fails. A successfully
    // recolored left eye must not publish alongside a stale right eye.
    const bool right=!fail_right && eyes_[1].resolve_msaa_palette(palette);
    resident_ready_=left && right;
    return resident_ready_;
}
GpuRasterOutput GpuScene::resident_output() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_->resident;
#else
    return {};
#endif
}
void* GpuScene::msaa_output() const noexcept {return impl_->msaa_result;}
bool GpuScene::resolve_msaa_palette(std::span<const Rgba8> palette) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUCommandBuffer* command=nullptr;
    try {
        if(!impl_->resident.pixels || !impl_->msaa_result) throw std::runtime_error("No resident MSAA scene to resolve");
        impl_->retire_submission();
        command=SDL_AcquireGPUCommandBuffer(impl_->device);Impl::require(command);
        auto* colors=impl_->upload_msaa_palette(command,palette);
        auto* output=impl_->msaa[0].resolve_scene(command,colors);
        if(!output) throw std::runtime_error(impl_->msaa[0].status());
        auto* submitted=command;command=nullptr;
        impl_->fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);Impl::require(impl_->fence);
        impl_->msaa_result=output;impl_->resident.msaa_color=output;
        impl_->status="Retained MSAA palette resolved without geometry replay";return true;
    } catch(const std::exception& error) {
        if(command) SDL_CancelGPUCommandBuffer(command);
        impl_->msaa_result=nullptr;impl_->resident.msaa_color=nullptr;impl_->status=error.what();
    }
#else
    (void)palette;
#endif
    return false;
}
GpuScene::RayGeometryOutput GpuScene::ray_geometry_output() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_->ray_output;
#else
    return {};
#endif
}
bool GpuScene::wait_for_completion() {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_->resident.pixels && !impl_->pending()) return false;
    try {impl_->finish();}
    catch(const std::exception& error){impl_->status=error.what();return false;}
    return true;
#else
    return false;
#endif
}
bool GpuScene::readback(Framebuffer& frame,SurfaceBuffer* surfaces) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    const auto output=resident_output();
    if(!output.device || !output.pixels || !output.width || !output.height
        || frame.stored_width()!=output.width || frame.stored_height()!=output.height
        || (surfaces && (surfaces->width()!=output.width || surfaces->height()!=output.height))) return false;
    auto* device=static_cast<SDL_GPUDevice*>(output.device);
    SDL_GPUCommandBuffer* command=nullptr;
    SDL_GPUTransferBuffer* transfer=nullptr;
    SDL_GPUFence* fence=nullptr;
    try {
        if(!wait_for_completion()) return false;
        const Uint32 pixels=output.width*output.height,bytes=pixels*4;
        const bool normals=surfaces && output.surfaces;
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=bytes+(normals?pixels*16:0);
        transfer=SDL_CreateGPUTransferBuffer(device,&info);Impl::require(transfer);
        command=SDL_AcquireGPUCommandBuffer(device);Impl::require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);Impl::require(copy);
        for(unsigned i=0;i<(normals?2U:1U);++i) {
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(i?output.surfaces:output.pixels),0,i?pixels*16:bytes};
            SDL_GPUTransferBufferLocation target{transfer,i?bytes:0};
            SDL_DownloadFromGPUBuffer(copy,&source,&target);
        }
        SDL_EndGPUCopyPass(copy);
        auto* submitted=command;command=nullptr;
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);Impl::require(fence);
        Impl::require(SDL_WaitForGPUFences(device,true,&fence,1));
        const bool coverage=frame.tracks_write_coverage();
        if(coverage) frame.begin_write_coverage();
        const auto* packed=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,transfer,false));Impl::require(packed);
        const auto* values=reinterpret_cast<const float*>(packed+pixels);
        if(surfaces) surfaces->clear();
        if(frame.draw_scale()>1) frame.enable_dither_pairs(true);
        frame.clear_dither_pairs();
        for(unsigned y=0;y<output.height;++y) for(unsigned x=0;x<output.width;++x) {
            const auto i=std::size_t(y)*output.width+x;
            if(coverage && (packed[i]&(1U<<26))) frame.set_stored(x,y,std::uint8_t(packed[i]));
            else frame.pixels()[i]=std::uint8_t(packed[i]);
            if(packed[i]&0x80000000U) frame.set_dither_alternate(i,std::uint8_t(packed[i]>>8));
            if(frame.layer_tags_enabled()) frame.layer_tags()[i]=gpu_pixel_layer(packed[i]);
            if(normals && (packed[i]&(1U<<24))) surfaces->set(x,y,
                {values[i*4],values[i*4+1],values[i*4+2],values[i*4+3]},std::uint8_t(packed[i]>>16));
        }
        SDL_UnmapGPUTransferBuffer(device,transfer);
        SDL_ReleaseGPUFence(device,fence);SDL_ReleaseGPUTransferBuffer(device,transfer);
        return true;
    } catch(const std::exception& error) {
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) SDL_ReleaseGPUFence(device,fence);
        if(transfer) SDL_ReleaseGPUTransferBuffer(device,transfer);
        impl_->status=error.what();
    }
#else
    (void)frame;(void)surfaces;
#endif
    return false;
}
}
