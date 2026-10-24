#include "starfox/modern/sdl_frame.hpp"
#include "starfox/vr/background_tiles.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace starfox::modern {
namespace {
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what);}
// PNG (or BMP) to RGBA words with red in the low byte; empty when absent.
std::optional<vr::ModernImage> load_rgba(const std::string& path) {
    SDL_Surface* loaded=SDL_LoadSurface(path.c_str());
    if(!loaded) return {};
    SDL_Surface* rgba=SDL_ConvertSurface(loaded,SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(loaded);
    if(!rgba) return {};
    vr::ModernImage image{unsigned(rgba->w),unsigned(rgba->h),{}};
    if(image.width==0 || image.height==0 || image.width>8192 || image.height>8192) {SDL_DestroySurface(rgba);return {};}
    image.pixels.resize(std::size_t(image.width)*image.height);
    for(unsigned y=0;y<image.height;++y)
        std::memcpy(image.pixels.data()+std::size_t(y)*image.width,static_cast<const uint8_t*>(rgba->pixels)+std::size_t(y)*rgba->pitch,image.width*4U);
    SDL_DestroySurface(rgba);
    return image;
}
SceneBlend sdl_blend(vr::SceneBlend blend) {
    switch(blend) {
    case vr::SceneBlend::opaque: return SceneBlend::opaque;
    case vr::SceneBlend::add: return SceneBlend::add;
    case vr::SceneBlend::subtract: return SceneBlend::subtract;
    case vr::SceneBlend::half_add: return SceneBlend::half_add;
    case vr::SceneBlend::half_subtract: return SceneBlend::half_subtract;
    case vr::SceneBlend::alpha: return SceneBlend::alpha;
    default: throw std::runtime_error("MODERN overlay blend has no SDL_GPU pipeline");
    }
}
}
void SdlFrameRenderer::set_texture_directory(std::string directory) {
    texture_directory_=std::move(directory);
    if(texture_directory_.empty()) {frame_.set_image_loader({});return;}
    frame_.set_image_loader([this](const std::string& name) {
        const auto path=texture_directory_+name;
        auto image=load_rgba(path);
        std::fprintf(stderr,"MODERN textures: %s %s\n",path.c_str(),image?"loaded":"(not present)");
        return image;
    });
}
void SdlFrameRenderer::set_texture_dump(TextureDump* dump) {
    texture_dump_=dump;
    if(!dump) {frame_.set_landscape_observer({});return;}
    frame_.set_landscape_observer([this](uint16_t background,const simulation::SnesPpuState& ppu,float horizon,std::span<const uint32_t> haze) {
        texture_dump_->landscape(background,ppu,horizon,haze);
    });
}
bool SdlFrameRenderer::initialize(SDL_GPUDevice* device,SDL_GPUTextureFormat colour,SDL_GPUTextureFormat depth) {
    release();
    if(!pipelines_.initialize(device,colour,depth,true)) {status_=pipelines_.status();return false;}
    if(!layer_filter_.initialize(device,colour,depth)) {status_=layer_filter_.status();return false;}
    device_=device;status_="MODERN frame ready";return true;
}
void SdlFrameRenderer::release() noexcept {
    for(auto* packets:{&stars_,&backgrounds_,&tunnel_,&cockpit_,&sprites_,&reticle_,&circle_,&shutter_,&textures_,&colour_math_,&replaced_sprites_,&engine_glow_}) packets->release();
    layer_filter_.release();ray_shadows_.release();shadows_active_=false;
    frame_.reset();world_.release();uploads_.release();pipelines_.release();device_=nullptr;
}
bool SdlFrameRenderer::prepare(SDL_GPUCommandBuffer* command,const vr::GameSceneSnapshot& previous,
    const vr::GameSceneSnapshot& current,double alpha,vr::SourceModelPackets world,const vr::FrameLayerInputs& inputs) {
    try {
        require(device_ && command,"MODERN frame not initialized");
        frame_.prepare(previous,current,alpha,std::move(world),inputs);
        const auto& f=frame_;
        // The 2D filter needs whole output pixels per source pixel.
        const bool filtering=two_d_filter_!=0 && !f.settings().headset && std::lround(target_height_/224.F)>=2;
        using Flow=simulation::GameFlowState;
        const auto flow=current.flow;
        filter_sprites_=filtering;
        filter_backgrounds_=filtering && (flow==Flow::title || flow==Flow::controls_type || flow==Flow::controls_choice
            || flow==Flow::planet_select || flow==Flow::game_over || flow==Flow::continue_choice);
        require(stars_.initialize(device_,uploads_,f.stars,f.star_keys,false),stars_.status());
        if(f.layers_rebuilt) require(tunnel_.initialize(device_,uploads_,f.tunnel_surround,{},false),tunnel_.status());
        if(f.layers_rebuilt || sprites_split_!=filter_sprites_) {
            // Replacement artwork (resident images, vertex texture bit 23)
            // is already high resolution: draw it after the filtered layer.
            std::vector<vr::DrawPacket> native,replaced;
            if(filter_sprites_) {
                for(const auto& packet:f.sprites) {
                    const auto vertices=packet.geometry.vertex_view();
                    std::vector<vr::SceneVertex> kept,moved;
                    for(std::size_t i=0;i+2<vertices.size();i+=3) {
                        auto& into=(vertices[i].texture[3]&8388608U)?moved:kept;
                        into.insert(into.end(),vertices.begin()+std::ptrdiff_t(i),vertices.begin()+std::ptrdiff_t(i+3));
                    }
                    if(moved.empty()) {native.push_back(packet);continue;}
                    auto hd=packet;hd.geometry.shared_vertices.reset();hd.geometry.vertices=std::move(moved);
                    hd.geometry.line_vertices.clear();hd.geometry.shared_line_vertices.reset();
                    replaced.push_back(std::move(hd));
                    auto rest=packet;rest.geometry.shared_vertices.reset();rest.geometry.vertices=std::move(kept);
                    native.push_back(std::move(rest));
                }
            } else native.assign(f.sprites.begin(),f.sprites.end());
            require(sprites_.initialize(device_,uploads_,native,{},false),sprites_.status());
            require(replaced_sprites_.initialize(device_,uploads_,replaced,{},false),replaced_sprites_.status());
            sprites_split_=filter_sprites_;
        }
        if(f.backgrounds_rebuilt) require(backgrounds_.initialize(device_,uploads_,f.backgrounds,{},false),backgrounds_.status());
        require(backgrounds_.update_models(f.background_models),"MODERN background motion update failed");
        if(f.textures_rebuilt) require(textures_.initialize(device_,uploads_,f.textures,{},false),textures_.status());
        if(!f.textures.empty()) require(textures_.update_models(f.texture_models),"MODERN landscape texture motion update failed");
        if(f.cockpit) require(cockpit_.initialize(device_,uploads_,std::span(&*f.cockpit,1),{},false),cockpit_.status());
        if(f.reticle) require(reticle_.initialize(device_,uploads_,std::span(&f.reticle->packet,1),{},false,false,
            sdl_blend(f.reticle->blend)),reticle_.status());
        if(f.circle) require(circle_.initialize(device_,uploads_,std::span(&f.circle->packet,1),{},false,false,
            sdl_blend(f.circle->blend)),circle_.status());
        if(f.colour_math) require(colour_math_.initialize(device_,uploads_,std::span(&f.colour_math->packet,1),{},false,false,
            sdl_blend(f.colour_math->blend)),colour_math_.status());
        if(f.engine_glow) require(engine_glow_.initialize(device_,uploads_,std::span(&f.engine_glow->packet,1),{},true,false,
            sdl_blend(f.engine_glow->blend)),engine_glow_.status());
        if(f.shutter) require(shutter_.initialize(device_,uploads_,std::span(&*f.shutter,1),{},false),shutter_.status());
        require(uploads_.flush(device_,command),uploads_.status());
        require(world_.initialize(device_,command,f.world,f.moving),world_.status());
        require(world_.record_compute(command),world_.status());
        status_="MODERN frame prepared";return true;
    } catch(const std::exception& error) {uploads_.clear();status_=error.what();return false;}
}
bool SdlFrameRenderer::record_shadows(SDL_GPUCommandBuffer* command,const vr::EyeCamera& camera,
    std::optional<render::shadows::Vec3> toward_light,const std::function<bool(uint32_t)>& caster,const ShadowSettings& settings,
    const LightInputs* lights) {
    shadows_active_=sun_active_=false;shadow_settings_=settings;point_lights_.clear();
    shadow_settings_.brightness=std::clamp(settings.brightness,0.F,1.F);
    shadow_settings_.samples=std::clamp(settings.samples,1U,16U);
    const bool receiver=std::any_of(frame_.backgrounds.begin(),frame_.backgrounds.end(),[&](const auto& packet) {
        // The landscape ground where the cartridge draws shadows; the 3D
        // corridor and the boss room always.
        return (settings.cartridge_shadows && vr::is_landscape_ground(packet)) || vr::is_tunnel_corridor(packet)
            || vr::is_nucleus_room(packet) || vr::is_nucleus_floor(packet);
    });
    const bool sun=toward_light && receiver && !settings.no_shadows;
    const auto eye=reversed_depth_camera(camera);
    if(sun || lights) {
        // Emissive lights need the casters for their own shadow rays, sun
        // or not (space has no sun shadows); any direction serves the
        // casters' sun-plane bounds then.
        if(!ray_shadows_.record(device_,command,world_,eye,sun?*toward_light:render::shadows::Vec3{0,-1,0},caster)) {
            status_=ray_shadows_.status();world_.set_hide_source_shadows(false);return false;
        }
        sun_active_=sun;
        if(lights) point_lights_=ray_shadows_.lights(eye,frame_.world.compute_models,frame_.world.packets,frame_.world.handles,
            lights->emitter,lights->bomb,max_point_lights,shadow_settings_.light_reach,lights->tick);
        shadows_active_=sun_active_ || !point_lights_.empty();
    }
    // Ray-traced shadows replace the cartridge's polygons, never both.
    world_.set_hide_source_shadows(sun_active_ || settings.no_shadows);
    pipelines_.set_shadow_buffers(shadows_active_?ray_shadows_.triangles():nullptr,shadows_active_?ray_shadows_.objects():nullptr);
    return true;
}
void SdlFrameRenderer::push_fog(SDL_GPUCommandBuffer* command,const vr::EyeCamera& camera,unsigned mode) const noexcept {
    // scene_sdl.hlsl SdlFog: fog colour, fog params, the shadow block, then
    // the emissive point lights.
    std::array<float,40+16*max_point_lights> data{};
    if(mode && frame_.fog) {
        const auto& fog=*frame_.fog;
        for(unsigned c=0;c<4;++c) data[c]=fog.colour[c];
        // Reversed depth: normalized depth = near / distance (metres, 256
        // source units each).
        data[4]=fog.start;data[5]=fog.end;data[6]=std::abs(camera.projection[14])*256.F;data[7]=float(mode);
    }
    // Receivers: the landscape's ground (mode 2) and the world's models (mode 1).
    // Sun shadows (light_params.w) and point lights choose them separately.
    const bool sun=sun_active_ && ray_shadows_.object_count() && (mode==2 || (mode==1 && shadow_settings_.models));
    const bool lit=!point_lights_.empty() && (mode==1 || mode==2);
    if(shadows_active_ && (sun || lit)) {
        const auto& light=ray_shadows_.light();
        data[8]=mode==2?2.F:1.F;data[9]=float(ray_shadows_.object_count());data[10]=shadow_settings_.brightness;
        data[11]=float(shadow_settings_.samples);
        data[12]=float(light.x);data[13]=float(light.y);data[14]=float(light.z);data[15]=2e-4F;
        data[16]=float(target_width_);data[17]=target_height_;data[18]=shadow_settings_.radius;
        std::copy(shadow_unproject_.begin(),shadow_unproject_.end(),data.begin()+20);
        // light_params: point light count, brightness steps, strength, sun on.
        data[36]=lit?float(point_lights_.size()):0.F;data[37]=float(shadow_settings_.light_steps);
        data[38]=shadow_settings_.light_strength;data[39]=sun?1.F:0.F;
        for(std::size_t i=0;lit && i<point_lights_.size();++i) {
            const auto& point=point_lights_[i];
            float* out=data.data()+40+i*16;
            out[0]=point.position[0];out[1]=point.position[1];out[2]=point.position[2];out[3]=point.radius;
            out[4]=point.colour[0];out[5]=point.colour[1];out[6]=point.colour[2];out[7]=std::bit_cast<float>(point.object);
            out[8]=point.axis[0];out[9]=point.axis[1];out[10]=point.axis[2];out[11]=point.aspect;
            out[12]=point.hardness;out[13]=point.projected?1.F:0.F;out[14]=point.fade_height;
            out[15]=point.background_only?1.F:0.F;
        }
    }
    SDL_PushGPUFragmentUniformData(command,1,data.data(),sizeof(data));
}
bool SdlFrameRenderer::record_filtered_layers(SDL_GPUCommandBuffer* command,const vr::EyeCamera& source_camera) {
    push_fog(command,source_camera,0);
    if(!filter_backgrounds_ && !filter_sprites_) return true;
    const auto& f=frame_;
    // One texel per source pixel: 224 rows at the target's aspect.
    const unsigned factor=unsigned(std::max(1L,std::lround(target_height_/224.F)));
    const unsigned width=unsigned(std::max(1L,std::lround(float(target_width_)*224.F/target_height_)));
    if(!layer_filter_.resize(width,224,factor)) {status_=layer_filter_.status();return false;}
    auto camera=reversed_depth_camera(source_camera);
    camera.effects[3]|=16U<<8; // One output pixel per source pixel; nearest art.
    auto& pipelines=layer_filter_.pipelines();
    for(unsigned layer=0;layer<SdlLayerFilter::layer_count;++layer) {
        if(!(layer?filter_sprites_:filter_backgrounds_)) continue;
        auto* pass=layer_filter_.begin_layer(command,layer);
        if(!pass) {status_=layer_filter_.status();return false;}
        auto layer_camera=camera;
        if(!layer) layer_camera.effects[2]=f.sky_turn;
        const bool recorded=layer?sprites_.record(command,pass,pipelines,layer_camera)
            :backgrounds_.record(command,pass,pipelines,layer_camera);
        SDL_EndGPURenderPass(pass);
        if(!recorded) {status_="MODERN filtered layer recording failed: "+pipelines.status();return false;}
        if(!layer_filter_.filter(command,layer,two_d_filter_)) {status_=layer_filter_.status();return false;}
    }
    return true;
}
bool SdlFrameRenderer::record(SDL_GPUCommandBuffer* command,SDL_GPURenderPass* pass,const vr::EyeCamera& source_camera) {
    const auto& f=frame_;
    auto camera=reversed_depth_camera(source_camera);
    if(filter_2d_) camera.effects[3]|=1U;
    if(shadows_active_) {
        // Rows of the inverse of the projection the scene shader draws with
        // (sdl_scene_constants: Y negated), for receiver positions.
        std::array<double,32> m{};
        for(unsigned row=0;row<4;++row) for(unsigned column=0;column<4;++column) {
            m[row*8+column]=camera.projection[column*4+row]*(row==1?-1.:1.);
            m[row*8+4+column]=row==column?1.:0.;
        }
        bool invertible=true;
        for(unsigned column=0;column<4 && invertible;++column) {
            unsigned pivot=column;
            for(unsigned row=column+1;row<4;++row) if(std::abs(m[row*8+column])>std::abs(m[pivot*8+column])) pivot=row;
            if(std::abs(m[pivot*8+column])<1e-30) {invertible=false;break;}
            for(unsigned k=0;k<8;++k) std::swap(m[column*8+k],m[pivot*8+k]);
            const double scale=1/m[column*8+column];
            for(unsigned k=0;k<8;++k) m[column*8+k]*=scale;
            for(unsigned row=0;row<4;++row) if(row!=column) {
                const double factor=m[row*8+column];
                for(unsigned k=0;k<8;++k) m[row*8+k]-=factor*m[column*8+k];
            }
        }
        if(!invertible) shadows_active_=false;
        else for(unsigned row=0;row<4;++row) for(unsigned column=0;column<4;++column)
            shadow_unproject_[row*4+column]=float(m[row*8+4+column]);
    }
    // Output pixels per source pixel, 4.4 fixed point (bits 8-19): ground dots.
    // A headset view measures it like the PCVR app: the native 1/256 tangent.
    const float source_pixel=f.settings().headset?target_height_*std::abs(camera.projection[5])/512.F:source_pixel_;
    camera.effects[3]|=uint32_t(std::clamp(source_pixel*16.F,16.F,4095.F))<<8;
    const auto fail=[&](const char* layer) {status_=std::string("MODERN ")+layer+" recording failed: "+pipelines_.status();return false;};
    push_fog(command,camera,0);
    if(!stars_.record(command,pass,pipelines_,camera)) return fail("star");
    push_fog(command,camera,2); // The 3D ground and the sky above its horizon.
    {
        // A headset preview passes the source camera heading to the ground.
        auto background_camera=camera;
        if(f.settings().headset) background_camera.effects[2]=f.landscape_heading;
        else background_camera.effects[2]=f.sky_turn;
        if(filter_backgrounds_) {
            if(!layer_filter_.composite(command,pass,0,samples_)) {status_=layer_filter_.status();return false;}
        } else if(!backgrounds_.record(command,pass,pipelines_,background_camera)) return fail("background");
    }
    if(!f.textures.empty()) {
        auto mapped=camera;mapped.effects[2]=f.texture_mapping;
        if(!textures_.record(command,pass,pipelines_,mapped)) return fail("landscape texture");
    }
    push_fog(command,camera,0);
    if(!tunnel_.record(command,pass,pipelines_,camera)) return fail("tunnel surround");
    push_fog(command,camera,1);
    if(!world_.record(command,pass,pipelines_,camera)) return fail("world");
    push_fog(command,camera,0);
    if(f.engine_glow && !engine_glow_.record(command,pass,pipelines_,camera)) return fail("engine glow");
    if(f.cockpit && !cockpit_.record(command,pass,pipelines_,camera)) return fail("cockpit HUD");
    if(f.circle && !f.circle->over_hud && !circle_.record(command,pass,pipelines_,camera)) return fail("circle");
    const vr::Matrix4 ndc{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    if(f.colour_math && !f.colour_math->over_hud && !colour_math_.record(command,pass,pipelines_,vr::EyeCamera{ndc,ndc}))
        return fail("colour math");
    if(filter_sprites_) {
        if(!layer_filter_.composite(command,pass,1,samples_)) {status_=layer_filter_.status();return false;}
    } else if(!sprites_.record(command,pass,pipelines_,camera)) return fail("sprite");
    if(!replaced_sprites_.record(command,pass,pipelines_,camera)) return fail("replacement sprite");
    if(f.reticle && !reticle_.record(command,pass,pipelines_,camera)) return fail("reticle");
    if(f.circle && f.circle->over_hud && !circle_.record(command,pass,pipelines_,camera)) return fail("circle");
    if(f.colour_math && f.colour_math->over_hud && !colour_math_.record(command,pass,pipelines_,vr::EyeCamera{ndc,ndc}))
        return fail("colour math");
    // Head-locked normalized-device shutter: identity view and projection.
    if(f.shutter && !shutter_.record(command,pass,pipelines_,vr::EyeCamera{ndc,ndc})) return fail("shutter");
    return true;
}
}
