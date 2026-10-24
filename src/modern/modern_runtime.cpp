#include "starfox/modern/modern_runtime.hpp"
#include "starfox/modern/frame_policy.hpp"
#include "starfox/modern/sdl_render_state.hpp"
#include "starfox/vr/decal_surface.hpp"
#include "starfox/vr/source_models.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string_view>
namespace starfox::modern {
namespace {
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what+(SDL_GetError()[0]?std::string(": ")+SDL_GetError():""));}
// SDL_PIXELFORMAT_ABGR8888 render targets are R8G8B8A8_UNORM on SDL_GPU.
constexpr auto target_format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
}
vr::EyeCamera flat_scene_camera(const vr::GameSceneSnapshot& scene,float aspect) noexcept {
    auto camera=vr::source_panel_camera(scene);
    if(!(aspect>0)) return camera;
    // source_panel_camera maps the 256x224 canvas onto the target (m[0]=2 for
    // a 128-pixel half width). Keep 112 rows above/below centre and widen.
    const float half_width=112.F*aspect;
    camera.projection[0]=256.F/half_width;
    camera.projection[8]=(128.F-(float(scene.source_vanishing_point[0])+16.F))/half_width;
    // The Super FX clips polygons at z = 0, so the cockpit shell's struts a
    // few units from the eye stay on screen. Near one source unit (1/256 m),
    // not 0.05 m: reversed infinite depth keeps D32F precision.
    camera.projection[14]=-1.F/256.F;
    return camera;
}
namespace {
// Headset preview (testing without a headset): STARFOX_MODERN_HEADSET_PREVIEW=
// "yaw,pitch[,fov]" in degrees (yaw left, pitch up, horizontal field of view,
// default 100) plans the frame exactly as the PCVR app does and draws it
// from the seat with that head orientation, through the same shader.
struct HeadsetPreview {float yaw{},pitch{},fov{100};};
std::optional<HeadsetPreview> headset_preview() {
    const char* value=std::getenv("STARFOX_MODERN_HEADSET_PREVIEW");
    if(!value || !*value) return {};
    HeadsetPreview preview;
    if(std::sscanf(value,"%f,%f,%f",&preview.yaw,&preview.pitch,&preview.fov)<2 || !(preview.fov>10 && preview.fov<170)) return {};
    return preview;
}
// vr::eye_camera's conventions (Vulkan Y down, infinite far, near 0.05 m)
// for an eye at the seat turned by yaw then pitch.
vr::EyeCamera headset_preview_camera(const HeadsetPreview& preview,float aspect) noexcept {
    constexpr float degrees=3.14159265358979F/180.F;
    const float cy=std::cos(preview.yaw*degrees),sy=std::sin(preview.yaw*degrees);
    const float cp=std::cos(preview.pitch*degrees),sp=std::sin(preview.pitch*degrees);
    // Eye-to-world R = Ry(yaw) Rx(pitch); the view is its transpose.
    const float r[3][3]{{cy,sy*sp,sy*cp},{0,cp,-sp},{-sy,cy*sp,cy*cp}}; // r[row][column]
    vr::EyeCamera camera{};
    for(unsigned row=0;row<3;++row) for(unsigned column=0;column<3;++column) camera.view[column*4+row]=r[column][row];
    camera.view[15]=1;
    const float x=std::tan(preview.fov*.5F*degrees),y=x/std::max(aspect,.01F);
    auto& m=camera.projection;
    m[0]=1/x;m[5]=-1/y;m[10]=-1;m[11]=-1;m[14]=-.05F;
    return camera;
}
}
int hud_edge_shift(float aspect) noexcept {
    if(!(aspect>0)) return 0;
    // flat_scene_camera shows 224 rows and 224 x aspect source columns.
    const float hud_width=std::min(224.F*aspect,224.F*max_hud_aspect);
    return std::max(0,int(std::floor((hud_width-256.F)/2.F)));
}
struct ModernRuntime::State {
    State(const simulation::GameSimulation& game,const assets::RomImage& rom,const assets::SymbolMap& symbols)
        :history(game,rom,symbols),models(rom,symbols,true,true),dialogue_layout(rom,symbols),
        trigonometry(simulation::TrigTables::load(rom,symbols)) {
        inputs.rom=&rom;inputs.symbols=&symbols;inputs.dialogue_layout=&dialogue_layout;
        inputs.trigonometry=&trigonometry;
        inputs.srgb=false;inputs.ex_experience=game.experience()==simulation::Experience::starfox_ex;
        inputs.flat_landscape=true;
        // STARFOX_MODERN_CORRIDOR=0 keeps the native tunnel layer (A/B).
        inputs.tunnel_corridor=!(std::getenv("STARFOX_MODERN_CORRIDOR") && std::string_view(std::getenv("STARFOX_MODERN_CORRIDOR"))=="0");
        inputs.vector_reticle=true;
        inputs.space_roll_geometry=true;
        inputs.flat_space_horizon=true;
        inputs.flat_tunnel_extension=true;
        inputs.bg3_behind_bg2=true;
        // The desktop's Super FX UI layer: 224 wide, centred, 16 rows down.
        inputs.comms_offset={16,16};
        inputs.pause_text=true;
        inputs.landscape_ground_dots=!(std::getenv("STARFOX_MODERN_GROUND_DOTS") && std::string_view(std::getenv("STARFOX_MODERN_GROUND_DOTS"))=="0");
        // STARFOX_MODERN_LANDSCAPE=0 keeps the native BG2 planet layer.
        const auto* landscape=std::getenv("STARFOX_MODERN_LANDSCAPE");
        inputs.landscape_world=!(landscape && std::string_view(landscape)=="0");
        // Classic ground bands stream with the world by default (testing);
        // STARFOX_MODERN_CLASSIC_SCROLL=0 keeps them fixed like the original.
        const auto* scroll=std::getenv("STARFOX_MODERN_CLASSIC_SCROLL");
        inputs.classic_ground_scroll=!(scroll && std::string_view(scroll)=="0");
        // Texture overlays inset into a backing face (signs, markings) are
        // painter-ordered on the cartridge; mark them as decals so they keep
        // drawing over their backing face instead of z-fighting with it.
        vr::set_inset_decals(true);
        vr::set_solid_decals(true);
        if(preview) {
            // The PCVR app's inputs (application.cpp): VR's own cockpit, no
            // flat-screen layer substitutes, no widescreen HUD shift.
            inputs.trigonometry=nullptr;inputs.flat_landscape=inputs.space_roll_geometry=false;
            inputs.flat_space_horizon=inputs.flat_tunnel_extension=false;
            inputs.comms_offset={};inputs.pause_text=false;
        }
        // Replacement model textures, portraits and sprites
        // (<exe>/textures/tex_*.png, face_*.png, obj_*.png) and, with
        // STARFOX_MODERN_DUMP_TEXTURES=1, a dump of the source artwork in the
        // same layout (<exe>/texture_dump/).
        if(const char* base=SDL_GetBasePath()) {
            overrides=load_texture_overrides(std::string(base)+"textures/");
            if(!overrides.empty()) models.set_texture_overrides(&overrides);
            portraits=load_portrait_overrides(std::string(base)+"textures/");
            if(!portraits.empty()) inputs.portrait_overrides=&portraits;
            sprites=load_sprite_overrides(std::string(base)+"textures/");
            if(!sprites.empty()) inputs.sprite_overrides=&sprites;
            if(const auto* dump_flag=std::getenv("STARFOX_MODERN_DUMP_TEXTURES"); dump_flag && std::string_view(dump_flag)=="1") {
                dump=std::make_unique<TextureDump>(std::string(base)+"texture_dump/");
                std::fprintf(stderr,"MODERN texture dump: %s\n",dump->directory().c_str());
                models.set_texture_observer([this](const assets::TextureImage& texture,const std::array<uint16_t,16>& palette) {
                    dump->model_texture(texture,palette);
                });
            }
        }
    }
    vr::GameSceneHistory history;
    vr::SourceModels models;
    render::ScaledTextRenderer dialogue_layout;
    simulation::TrigTables trigonometry;
    vr::FrameLayerInputs inputs;
    std::optional<HeadsetPreview> preview{headset_preview()};
    render::TextureOverrides overrides,portraits;
    render::SpriteOverrides sprites;
    std::unique_ptr<TextureDump> dump;
    SdlFrameRenderer frame;
    SDL_GPUDevice* device{};
    SDL_Renderer* renderer{};
    SDL_Texture* target{};
    SDL_Texture* overlay{};
    SDL_Texture* dialog{};
    std::vector<uint8_t> overlay_pixels;
    SDL_GPUTexture* depth{};
    // ANTI-ALIASING: multisampled colour, resolved into target's texture.
    SDL_GPUTexture* multisample{};
    SDL_GPUSampleCount samples{SDL_GPU_SAMPLECOUNT_1};
    SDL_GPUTextureFormat depth_format{};
    int width{},height{};
    bool captured{};
    void release_targets() noexcept {
        if(target) SDL_DestroyTexture(target);
        if(overlay) SDL_DestroyTexture(overlay);
        if(dialog) SDL_DestroyTexture(dialog);
        if(depth && device) SDL_ReleaseGPUTexture(device,depth);
        if(multisample && device) SDL_ReleaseGPUTexture(device,multisample);
        target=overlay=dialog=nullptr;depth=multisample=nullptr;width=height=0;
    }
    ~State() {release_targets();frame.release();}
};
ModernRuntime::ModernRuntime(const simulation::GameSimulation& game,const assets::RomImage& rom,const assets::SymbolMap& symbols)
    :state_(std::make_unique<State>(game,rom,symbols)) {}
ModernRuntime::~ModernRuntime()=default;
void ModernRuntime::capture_tick() {state_->history.capture();state_->captured=true;}
void ModernRuntime::reset_interpolation() noexcept {state_->history.reset_interpolation();}
void ModernRuntime::release_renderer() noexcept {
    auto& s=*state_;
    s.release_targets();s.frame.release();s.device=nullptr;s.renderer=nullptr;
}
std::size_t ModernRuntime::unsupported_models() const noexcept {return state_->frame.world().unsupported_models();}
bool ModernRuntime::draws(const simulation::GameSimulation& game) noexcept {
    // Every cartridge screen the shared VR scene code draws (title, intro,
    // planet select, results, game over, credits...). The host's pre-game
    // and runtime option menus are drawn by the existing presentation path.
    return flat_frame_supported(game.in_setup_menu(),game.runtime_options_open(),game.stereo_output());
}
bool ModernRuntime::present(SDL_Renderer* renderer,double alpha,int requested_width,int requested_height,const char* capture_path,
    const HostOverlay& overlay,const HostOverlay& dialog,const vr::StageTallyLayer* tally) {
    auto& s=*state_;
    try {
        require(renderer!=nullptr,"MODERN needs a renderer");
        if(!s.captured) {
            // Ticks run before the first gameplay tick (preroll, menus) were
            // not captured: start from the current state without interpolation.
            s.history.capture();s.history.reset_interpolation();s.captured=true;
        }
        auto* device=static_cast<SDL_GPUDevice*>(SDL_GetPointerProperty(
            SDL_GetRendererProperties(renderer),SDL_PROP_RENDERER_GPU_DEVICE_POINTER,nullptr));
        require(device!=nullptr,"MODERN needs SDL's GPU render driver");
        if(device!=s.device || renderer!=s.renderer) {
            s.release_targets();s.frame.release();
            s.device=device;s.renderer=renderer;
            s.depth_format=SDL_GPUTextureSupportsFormat(device,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)?SDL_GPU_TEXTUREFORMAT_D32_FLOAT:SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            require(s.frame.initialize(device,target_format,s.depth_format),s.frame.status());
            if(const auto* filter=std::getenv("STARFOX_MODERN_FILTER"); filter && std::string_view(filter)=="0") s.frame.set_filter_2d(false);
            if(const char* base=SDL_GetBasePath()) s.frame.set_texture_directory(std::string(base)+"textures/");
            s.frame.set_texture_dump(s.dump.get());
            if(s.preview) s.frame.set_frame_settings({true,.25F});
        }
        int width=requested_width,height=requested_height;
        // Default: the window's output, not whatever intermediate target is bound.
        if(width<=0 || height<=0)
            require(SDL_GetRenderOutputSize(renderer,&width,&height) && width>0 && height>0,"Read MODERN output size");
        // ANTI-ALIASING (any type) is hardware MSAA here: LIGHT 2x, MEDIUM 4x,
        // HEAVY 8x, capped at what the device supports for both formats.
        SDL_GPUSampleCount samples=SDL_GPU_SAMPLECOUNT_1;
        {
            using Mode=simulation::AntiAliasingMode;
            const auto mode=s.history.game().anti_aliasing_mode();
            samples=mode==Mode::heavy?SDL_GPU_SAMPLECOUNT_8:mode==Mode::medium?SDL_GPU_SAMPLECOUNT_4
                :mode==Mode::light?SDL_GPU_SAMPLECOUNT_2:SDL_GPU_SAMPLECOUNT_1;
            if(const auto* forced=std::getenv("STARFOX_MODERN_MSAA")) {
                const int count=std::atoi(forced);
                samples=count>=8?SDL_GPU_SAMPLECOUNT_8:count>=4?SDL_GPU_SAMPLECOUNT_4:count>=2?SDL_GPU_SAMPLECOUNT_2:SDL_GPU_SAMPLECOUNT_1;
            }
            while(samples!=SDL_GPU_SAMPLECOUNT_1 && !(SDL_GPUTextureSupportsSampleCount(device,target_format,samples)
                && SDL_GPUTextureSupportsSampleCount(device,s.depth_format,samples)))
                samples=static_cast<SDL_GPUSampleCount>(int(samples)-1);
        }
        if(width!=s.width || height!=s.height || samples!=s.samples) {
            if(std::getenv("STARFOX_TRACE_MODERN")) {
                int window_w=0,window_h=0,current_w=0,current_h=0;
                SDL_GetWindowSizeInPixels(SDL_GetRenderWindow(renderer),&window_w,&window_h);
                SDL_GetCurrentRenderOutputSize(renderer,&current_w,&current_h);
                std::fprintf(stderr,"modern-target: output=%dx%d window=%dx%d current=%dx%d\n",
                    width,height,window_w,window_h,current_w,current_h);
            }
            s.release_targets();
            s.target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,width,height);
            require(s.target!=nullptr,"Create MODERN target");
            SDL_SetTextureBlendMode(s.target,SDL_BLENDMODE_NONE);
            SDL_GPUTextureCreateInfo depth{};
            depth.type=SDL_GPU_TEXTURETYPE_2D;depth.format=s.depth_format;depth.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            depth.width=uint32_t(width);depth.height=uint32_t(height);depth.layer_count_or_depth=1;depth.num_levels=1;
            depth.sample_count=samples;
            s.depth=SDL_CreateGPUTexture(device,&depth);
            require(s.depth!=nullptr,"Create MODERN depth");
            if(samples!=SDL_GPU_SAMPLECOUNT_1) {
                SDL_GPUTextureCreateInfo multisample=depth;
                multisample.format=target_format;multisample.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
                s.multisample=SDL_CreateGPUTexture(device,&multisample);
                require(s.multisample!=nullptr,"Create MODERN multisample target");
            }
            s.samples=samples;
            s.frame.set_sample_count(samples);
            s.width=width;s.height=height;
        }
        auto* colour=static_cast<SDL_GPUTexture*>(SDL_GetPointerProperty(
            SDL_GetTextureProperties(s.target),SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER,nullptr));
        require(colour!=nullptr,"MODERN target has no GPU texture");
        const auto previous=s.history.previous(),current=s.history.current();
        require(previous && current && current->ppu,"MODERN scene history incomplete");
        if(s.dump) s.dump->sprites(*current->ppu);
        // The desktop's 3D ASTEROIDS setting (VR passes its own preference).
        s.models.set_asteroid_models(s.history.game().asteroid_models());
        auto world=s.models.assemble_world_interpolated(*previous,*current,alpha,s.inputs.srgb,true);
        require(world.pending.empty(),"MODERN world assembly pending");
        require(source_models_supported(world.compute_models),"MODERN warp/axis models require the complete fallback renderer");
        // The renderer's own queued work must reach the GPU before ours.
        SDL_FlushRenderer(renderer);
        auto* command=SDL_AcquireGPUCommandBuffer(device);
        require(command!=nullptr,"Acquire MODERN command buffer");
        s.inputs.hud_edge_shift=s.preview?0:hud_edge_shift(float(width)/float(height));
        if(tally && !s.preview) s.inputs.stage_tally=*tally;else s.inputs.stage_tally.reset();
        // flat_scene_camera fills the height with the 224 source rows.
        s.frame.set_target(unsigned(width),unsigned(height),unsigned(std::max(1L,std::lround(height/224.))));
        s.frame.set_two_d_filter(unsigned(s.history.game().two_d_filter()));
        if(!s.frame.prepare(command,*previous,*current,alpha,std::move(world),s.inputs)) {
            SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(s.frame.status());
        }
        const auto camera=s.preview?headset_preview_camera(*s.preview,float(width)/float(height))
            :flat_scene_camera(*current,float(width)/float(height));
        if(!s.frame.record_filtered_layers(command,camera)) {
            SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(s.frame.status());
        }
        const auto clear=vr::frame_clear_colour(*current,s.inputs.srgb);
        SDL_GPUColorTargetInfo target{};target.texture=colour;target.load_op=SDL_GPU_LOADOP_CLEAR;
        target.store_op=SDL_GPU_STOREOP_STORE;target.clear_color={clear[0],clear[1],clear[2],1};
        if(s.multisample) {
            target.texture=s.multisample;target.store_op=SDL_GPU_STOREOP_RESOLVE;target.resolve_texture=colour;
        }
        SDL_GPUDepthStencilTargetInfo depth_target{};depth_target.texture=s.depth;depth_target.clear_depth=SdlFrameRenderer::clear_depth();
        depth_target.load_op=SDL_GPU_LOADOP_CLEAR;depth_target.store_op=SDL_GPU_STOREOP_DONT_CARE;
        depth_target.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE;depth_target.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE;
        auto* pass=SDL_BeginGPURenderPass(command,&target,1,&depth_target);
        if(!pass) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error("Begin MODERN render pass");}
        const bool recorded=s.frame.record(command,pass,camera);
        SDL_EndGPURenderPass(pass);
        require(SDL_SubmitGPUCommandBuffer(command),"Submit MODERN frame");
        require(recorded,s.frame.status());
        // Blit to the window without the desktop's logical presentation.
        const SdlRenderStateGuard restore_state{renderer};
        const auto mode=restore_state.logical_mode();
        SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderTarget(renderer,nullptr);SDL_SetRenderViewport(renderer,nullptr);
        // Same presentation as the existing renderer: letterboxed (or integer
        // scaled) at the frame's own aspect; only FIT SCREEN stretches.
        SDL_FRect destination{0,0,0,0};
        bool letterbox=mode!=SDL_LOGICAL_PRESENTATION_STRETCH;
        int output_w=0,output_h=0;
        if(letterbox && SDL_GetCurrentRenderOutputSize(renderer,&output_w,&output_h) && output_w>0 && output_h>0) {
            double scale=std::min(double(output_w)/width,double(output_h)/height);
            if(mode==SDL_LOGICAL_PRESENTATION_INTEGER_SCALE && scale>=1) scale=std::floor(scale);
            destination.w=float(std::round(width*scale));destination.h=float(std::round(height*scale));
            destination.x=float(std::floor((output_w-destination.w)/2));destination.y=float(std::floor((output_h-destination.h)/2));
            SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
        } else letterbox=false;
        require(SDL_RenderTexture(renderer,s.target,nullptr,letterbox?&destination:nullptr),"Draw MODERN frame");
        if(capture_path) {
            // Capture the GPU target that is blitted 1:1 to the window (never
            // a CPU buffer). SDL_RenderReadPixels returns nothing for hidden
            // test windows, so download the texture itself.
            SDL_GPUTransferBufferCreateInfo info{};
            info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=uint32_t(width)*uint32_t(height)*4U;
            if(auto* download=SDL_CreateGPUTransferBuffer(device,&info)) {
                auto* copy_command=SDL_AcquireGPUCommandBuffer(device);
                auto* copy=copy_command?SDL_BeginGPUCopyPass(copy_command):nullptr;
                if(copy) {
                    SDL_GPUTextureRegion region{};region.texture=colour;region.w=uint32_t(width);region.h=uint32_t(height);region.d=1;
                    SDL_GPUTextureTransferInfo destination{download,0,uint32_t(width),uint32_t(height)};
                    SDL_DownloadFromGPUTexture(copy,&region,&destination);SDL_EndGPUCopyPass(copy);
                    if(auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(copy_command)) {
                        SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);
                        if(auto* pixels=SDL_MapGPUTransferBuffer(device,download,false)) {
                            if(auto* surface=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_ABGR8888,pixels,width*4)) {
                                SDL_SaveBMP(surface,capture_path);SDL_DestroySurface(surface);
                            }
                            SDL_UnmapGPUTransferBuffer(device,download);
                        }
                    }
                } else if(copy_command) SDL_CancelGPUCommandBuffer(copy_command);
                SDL_ReleaseGPUTransferBuffer(device,download);
            }
        }
        // Host text (FPS counter; the EXIT GAME? / save-slot dialog on a dark
        // panel) drawn over the frame, so those screens need no fallback.
        const auto draw_overlay=[&](const HostOverlay& layer,SDL_Texture*& texture,const char* what) {
            if(!layer.ink || layer.canvas_width<=0 || layer.canvas_height<=0) return;
            // One source pixel wider and taller for the shadow.
            const int ow=int(layer.ink->width())+1,oh=int(layer.ink->height())+1;
            if(texture && (texture->w!=ow || texture->h!=oh)) {SDL_DestroyTexture(texture);texture=nullptr;}
            if(!texture) {
                texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STREAMING,ow,oh);
                require(texture!=nullptr,std::string("Create MODERN ")+what);
                SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
            }
            s.overlay_pixels.assign(std::size_t(ow)*std::size_t(oh)*4U,0);
            const auto ink=[&](int x,int y) {
                return x>=0 && y>=0 && x<ow-1 && y<oh-1 && layer.ink->get(uint32_t(x),uint32_t(y))!=0;
            };
            for(int y=0;y<oh;++y) for(int x=0;x<ow;++x) {
                const bool white=ink(x,y),shadow=!white && ink(x-1,y-1);
                auto* p=&s.overlay_pixels[(std::size_t(y)*std::size_t(ow)+std::size_t(x))*4U];
                if(!white && !shadow) {
                    if(layer.panel) {p[0]=p[1]=p[2]=0;p[3]=200;}
                    continue;
                }
                p[0]=p[1]=p[2]=white?255:0;p[3]=255;
            }
            require(SDL_UpdateTexture(texture,nullptr,s.overlay_pixels.data(),ow*4),std::string("Update MODERN ")+what);
            SDL_FRect frame=destination;
            if(!letterbox) {
                int full_w=0,full_h=0;SDL_GetCurrentRenderOutputSize(renderer,&full_w,&full_h);
                frame={0,0,float(full_w),float(full_h)};
            }
            const float sx=frame.w/float(layer.canvas_width),sy=frame.h/float(layer.canvas_height);
            const SDL_FRect at{frame.x+float(layer.x)*sx,frame.y+float(layer.y)*sy,float(ow)*sx,float(oh)*sy};
            require(SDL_RenderTexture(renderer,texture,nullptr,&at),std::string("Draw MODERN ")+what);
        };
        draw_overlay(overlay,s.overlay,"overlay");
        draw_overlay(dialog,s.dialog,"dialog");
        const bool presented=SDL_RenderPresent(renderer);
        require(presented,"Present MODERN frame");
        status_.clear();return true;
    } catch(const std::exception& error) {status_=error.what();return false;}
}
}
