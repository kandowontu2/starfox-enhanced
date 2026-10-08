// Cartridge-backed game-frame integration check. Readback is fixture-only;
// production uploads native packets once and renders independent GPU eyes.
#include "starfox/render/calibrated_game_scene.hpp"
#include "starfox/render/calibrated_game_scene_fx.hpp"
#include "starfox/render/calibrated_ground.hpp"
#include "starfox/render/calibrated_ground_receiver.hpp"
#include "starfox/render/calibrated_game_motion.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace {
using namespace starfox;
void require(bool ok,const char* error) {if(!ok) throw std::runtime_error(error);}
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUTexture* colors[2]{},*depths[2]{};
    SDL_GPUTransferBuffer* read{};SDL_GPUCommandBuffer* command{};
    static constexpr unsigned width=256,height=224;
    ~Gpu() {
        if(device) {
            if(command) SDL_CancelGPUCommandBuffer(command);
            SDL_WaitForGPUIdle(device);
            for(unsigned eye=0;eye<2;++eye) {
                if(colors[eye]) SDL_ReleaseGPUTexture(device,colors[eye]);
                if(depths[eye]) SDL_ReleaseGPUTexture(device,depths[eye]);
            }
            if(read) SDL_ReleaseGPUTransferBuffer(device,read);
            SDL_DestroyGPUDevice(device);
        }
        SDL_Quit();
    }
    void initialize(const char* backend) {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,true,backend);
        require(device,SDL_GetError());
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
        info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
        for(unsigned eye=0;eye<2;++eye) {
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            colors[eye]=SDL_CreateGPUTexture(device,&info);require(colors[eye],SDL_GetError());
            info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            depths[eye]=SDL_CreateGPUTexture(device,&info);require(depths[eye],SDL_GetError());
        }
        const SDL_GPUTransferBufferCreateInfo download{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,2*width*height*4,0};
        read=SDL_CreateGPUTransferBuffer(device,&download);require(read,SDL_GetError());
    }
    std::vector<std::uint8_t> render(render::GpuCalibratedScene& scene,const render::CalibratedGameFrame& frame,bool check_rays=false) {
        command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        const auto packets=frame.packets();
        require(scene.upload_packets(command,packets),scene.status().c_str());
        const auto token=scene.upload_token();
        const float convergence=frame.settings.convergence_source_units/256.F;
        for(unsigned eye=0;eye<2;++eye) {
            XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;
            view.pose.position.x=eye?.03F:-.03F;
            const float offset=-view.pose.position.x/convergence;
            view.fov={std::atan(-.5F+offset),std::atan(.5F+offset),std::atan(.4375F),-std::atan(.4375F)};
            auto camera=vr::eye_camera(view,1,1.F/256.F);require(camera.has_value(),"Invalid test eye");
            for(unsigned col=0;col<4;++col) camera->projection[col*4+1]*=-1;
            require(scene.enqueue_eye(command,colors[eye],depths[eye],width,height,*camera,frame.clear),scene.status().c_str());
            if(check_rays && std::any_of(packets.begin(),packets.end(),[](const auto& p){return p.ray_caster || p.ground_receiver;})) {
                for(const auto& p:packets) if(p.ground_receiver)
                    require(bool(render::calibrated_ground_ray_receiver(*p.packet,*camera,float(frame.settings.reflections)/3,
                        false,p.model_override?&*p.model_override:nullptr)),"Cartridge floor lost its calibrated ray plane");
                const auto rays=scene.enqueue_ray_geometry(command,eye,width,height,*camera,256,16,frame.clear);
                if(!rays.complete) {
                    for(const auto& draw:frame.draws) if(draw.ray_caster) {
                        std::uint32_t flags=0;for(const auto& v:draw.packet.geometry.vertex_view()) flags|=v.texture[3];
                        std::cerr<<"Selected source "<<draw.source_key<<" flags=0x"<<std::hex<<flags<<std::dec
                            <<" triangles="<<draw.packet.geometry.vertex_view().size()/3<<" line_vertices="<<draw.packet.geometry.line_view().size()<<'\n';
                    }
                }
                require(rays.complete,scene.status().c_str());
            }
        }
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,SDL_GetError());
        for(unsigned eye=0;eye<2;++eye) {
            SDL_GPUTextureRegion source{colors[eye],0,0,0,0,0,width,height,1};
            SDL_GPUTextureTransferInfo target{};target.transfer_buffer=read;
            target.offset=eye*width*height*4;target.pixels_per_row=width;target.rows_per_layer=height;
            SDL_DownloadFromGPUTexture(copy,&source,&target);
        }
        SDL_EndGPUCopyPass(copy);
        const bool submitted=SDL_SubmitGPUCommandBuffer(command);command=nullptr;require(submitted,SDL_GetError());
        require(scene.notify_submitted(token),"Lost calibrated upload ownership");
        require(SDL_WaitForGPUIdle(device),SDL_GetError());
        const auto* mapped=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,read,false));require(mapped,SDL_GetError());
        std::vector<std::uint8_t> pixels(mapped,mapped+2*width*height*4);
        SDL_UnmapGPUTransferBuffer(device,read);return pixels;
    }
};
}
int main(int argc,char** argv) try {
    if(argc<5) throw std::runtime_error("usage: check_calibrated_game_scene BACKEND ROM SYMBOLS MAP [MAP ...] [--enhanced] [--ground] [--rays] [--scene-fx] [--capture DIRECTORY]; or BACKEND RUNTIME-BUNDLE EX-BUNDLE|ORIGINAL-BUNDLE MAP [MAP ...] [OPTIONS]");
    bool enhanced=false,check_rays=false,ground=false,scene_fx=false;std::filesystem::path capture;
    for(int arg=4;arg<argc;++arg) {
        if(std::string_view(argv[arg])=="--enhanced") enhanced=true;
        if(std::string_view(argv[arg])=="--ground") ground=true;
        if(std::string_view(argv[arg])=="--rays") check_rays=true;
        if(std::string_view(argv[arg])=="--scene-fx") scene_fx=true;
        if(std::string_view(argv[arg])=="--capture") {
            require(arg+1<argc,"Missing capture directory");capture=argv[++arg];std::filesystem::create_directories(capture);
        }
    }
    std::unordered_map<std::string,std::vector<std::uint8_t>> images;
    const render::CalibratedBackdropLoader loader=[&](unsigned,std::string_view path)->std::span<const std::uint8_t> {
        auto& bytes=images[std::string(path)];
        if(bytes.empty()) {std::ifstream input(std::string(path),std::ios::binary);require(bool(input),"Missing enhanced backdrop");
            bytes.assign(std::istreambuf_iterator<char>(input),{});}
        return bytes;
    };
    // Exercise the same local companion as the executable after generated EX
    // ROM build caches have been removed. Validate its exact patches/symbols
    // manifest and payload checksums; never export private ROM data to disk.
    std::optional<assets::RuntimeBundlePayload> bundle;
    const auto kind=std::string_view(argv[3]);
    if(kind=="EX-BUNDLE" || kind=="ORIGINAL-BUNDLE") {
        const auto read=[](const char* path) {
            std::ifstream file(path,std::ios::binary);require(bool(file),"Missing runtime fixture resource");
            return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(file),{}};
        };
        const std::array paths{"assets/patches/ultrastarfox-v12.bps","assets/symbols/ultrastarfox.txt",
            "assets/patches/starfox-ex-v12.bps","assets/symbols/starfox-ex.txt",
            "assets/patches/retail-japan-v10-to-usa-v12.bps","assets/patches/retail-japan-v11-to-usa-v12.bps",
            "assets/patches/retail-usa-v10-to-v12.bps","assets/patches/retail-usa-v11-to-v12.bps",
            "assets/patches/retail-europe-v10-to-usa-v12.bps","assets/patches/retail-europe-v11-to-usa-v12.bps",
            "assets/patches/retail-germany-v10-to-usa-v12.bps"};
        std::array<std::vector<std::uint8_t>,11> bytes;
        std::array<assets::RuntimeManifestResource,11> manifest;
        for(unsigned n=0;n<11;++n) {bytes[n]=read(paths[n]);manifest[n]={bytes[n],n==1 || n==3};}
        bundle=assets::decode_runtime_bundle(read(argv[2]),assets::runtime_asset_manifest(manifest));
    }
    const auto rom=bundle?assets::RomImage(kind=="EX-BUNDLE"?std::move(bundle->starfox_ex_rom):std::move(bundle->original_rom))
        :assets::RomImage::load(argv[2]);
    const auto symbols=bundle?assets::SymbolMap::parse(kind=="EX-BUNDLE"?bundle->starfox_ex_symbols:bundle->original_symbols)
        :assets::SymbolMap::load(argv[3]);
    Gpu gpu;gpu.initialize(argv[1]);render::GpuCalibratedScene scene;
    require(scene.initialize(gpu.device,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),scene.status().c_str());
    const auto rig=render::calibrated_game_rig();
    require(rig.meters_to_virtual==1 && rig.convergence==4 && rig.near_plane==1.F/256.F,
        "Runtime/game world-unit contract drifted");
    unsigned rendered=0,stereo_frames=0,captured_points=0;
    unsigned finite_floors=0,matched_floors=0,radiance_rejections=0,static_clock_holds=0;
    for(int map=4;map<argc;++map) {
        if(std::string_view(argv[map])=="--enhanced" || std::string_view(argv[map])=="--rays"
            || std::string_view(argv[map])=="--ground" || std::string_view(argv[map])=="--scene-fx") continue;
        if(std::string_view(argv[map])=="--capture") {++map;continue;}
        simulation::GameSimulation game(rom,symbols,argv[map],{},true);
        if(kind=="EX-BUNDLE") game.set_experience(simulation::Experience::starfox_ex);
        game.set_god_mode(true);game.set_timing_mode(simulation::TimingMode::unlocked_20_fps);
        audio::Spc700Audio audio;
        vr::GameSceneHistory history(game,rom,symbols);
        render::CalibratedGameScene assembler(rom,symbols);
        const auto advance=[&] {const auto result=game.tick({});(void)audio.render_logic_tick(result.audio_port_writes);
            game.synchronize_apu_output_ports(audio.output_ports());history.capture();};
        for(unsigned tick=0;tick<600;++tick) advance();
        std::size_t models=0,artwork=0;
        for(unsigned checkpoint=0;checkpoint<3;++checkpoint) {
            for(unsigned tick=0;tick<23;++tick) advance();
            const auto owned=history.current();const auto revision=owned->revision;
            const auto game_revision=game.scene_revision();
            render::CalibratedGameSettings settings;
            settings.enhanced_sky=enhanced;
            settings.enhanced_ground=ground;
            if(scene_fx) {settings.scene_enhancements=255;settings.particle_enhancements=15;}
            settings.world_effects={2,100,0,0};settings.model_effects={1,100,0,0};
            for(double alpha:{0.,.5,1.}) {
                settings.effect_seconds=float(checkpoint)+float(alpha)/60;
                const auto frame=assembler.assemble(history.previous(),owned,game,alpha,settings,loader);
                if(ground) {
                    const auto stage=symbols.find("NEWMAP"),lava=symbols.find("LEVEL6_6");
                    const bool exact_lava=game.experience()==simulation::Experience::starfox_ex
                        && !stage.empty() && !lava.empty()
                        && (std::uint32_t(game.map().read_native_word(stage.front()))
                            |(std::uint32_t(game.map().read_native_byte(stage.front()+2))<<16))==lava.front();
                    unsigned lava_receivers=0;
                    for(const auto& draw:frame.draws) {
                        const auto words=draw.packet.geometry.texel_view();
                        if(words.size()==render::calibrated_ground_offset+render::calibrated_ground_words
                            && !draw.packet.geometry.vertex_view().empty()
                            && (draw.packet.geometry.vertex_view().front().texture[3]&render::calibrated_ground_flag)
                            && words[render::calibrated_ground_offset+8]==9) ++lava_receivers;
                    }
                    require(!lava_receivers || exact_lava,"Automatic native lava leaked to a background alias/other course");
                    if(exact_lava && owned->background_landscape && owned->landscape_grid_height<0
                        && owned->flow!=simulation::GameFlowState::ex_pregame_menu && !owned->ppu->tunnel_scene)
                        require(lava_receivers==1,"EX 6-6 lost its native automatic lava receiver");
                }
                require(render::valid_calibrated_scene_fx(frame),"Cartridge capture published invalid/disabled scene effects");
                if(scene_fx) {
                    const auto held=assembler.assemble(history.previous(),owned,game,alpha,settings,loader);
                    require(held.scene_fx==frame.scene_fx && held.scene_fx_origin==frame.scene_fx_origin
                        && held.scene_fx_source_to_rig==frame.scene_fx_source_to_rig,"Held cartridge capture spawned/aged events");
                    auto off=settings;off.scene_enhancements=off.particle_enhancements=0;
                    require(!assembler.assemble(history.previous(),owned,game,alpha,off,loader).scene_fx.active(),"Cartridge OFF retained physical events");
                    captured_points+=frame.scene_fx.count;
                } else require(!frame.scene_fx.active(),"Unenhanced cartridge capture spawned particles");
                require(frame.current==owned && frame.current->revision==revision,"Both eyes must retain the same source tick");
                require(!frame.draws.empty(),"Complete game frame has no passes");
                const auto descriptors=frame.packets();
                require(descriptors.size()==frame.draws.size(),"Calibrated frame lost ordered passes");
                // Real assembly must retain a distinct finite-floor identity.
                // The independent MRT fixture checks moving plane/eye math;
                // this checks cartridge ownership and local temporal gating.
                const auto held=assembler.assemble(history.previous(),owned,game,alpha,settings,loader);
                XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;
                view.fov={-.5F,.5F,.4F,-.4F};
                auto camera=vr::eye_camera(view,1,1.F/256.F).value();
                for(unsigned col=0;col<4;++col) camera.projection[col*4+1]*=-1;
                const auto matches=render::calibrated_game_motion_draws(frame,&held,camera,camera);
                std::size_t motion_index=0;unsigned floor_occurrences=0;
                for(const auto& draw:frame.draws) {
                    const auto source=render::calibrated_ground_motion_source(draw.packet);
                    if(source) {
                        ++finite_floors;++floor_occurrences;
                        require(draw.source_key==render::calibrated_ground_source_key
                            && draw.layer==render::CalibratedGameLayer::world && !draw.ray_caster && !draw.after_rays,
                            "Cartridge finite floor lost its unique world identity");
                        const bool static_floor=!source->motion && (source->material==0 || source->material==8);
                        require(motion_index<matches.size() && matches[motion_index].valid==static_floor,
                            "Cartridge floor borrowed liquid/radiance flow or lost static correspondence");
                        if(static_floor) {
                            ++matched_floors;
                            if(ground && !scene_fx && source->enhanced) {
                                auto palette=settings;palette.world_effects={unsigned(render::Effect::monochrome),100,0,0};
                                palette.model_effects={unsigned(render::Effect::sepia),35,0,0};
                                const auto first=assembler.assemble(history.previous(),owned,game,alpha,palette,loader);
                                palette.effect_seconds+=1;
                                const auto tick=assembler.assemble(history.previous(),owned,game,alpha,palette,loader);
                                require(render::same_calibrated_presentation(first,tick),"Frozen cartridge terrain clock changed source/upload identity");
                                ++static_clock_holds;
                            }
                        } else ++radiance_rejections;
                    } else require(draw.source_key!=render::calibrated_ground_source_key,
                        "Sky/model/UI borrowed the finite-floor source identity");
                    motion_index+=!draw.packet.geometry.vertex_view().empty();
                    motion_index+=!draw.packet.geometry.line_view().empty();
                }
                require(floor_occurrences<=1 && motion_index==matches.size(),"Cartridge floor/motion ordering became ambiguous");
                bool crossed_ray_boundary=false;
                for(std::size_t i=0;i<frame.draws.size();++i) {
                    const auto& draw=frame.draws[i];const auto& p=descriptors[i];
                    require(p.packet==&draw.packet,"Calibrated descriptor borrows another frame");
                    require(p.ray_caster==draw.ray_caster && p.after_rays==draw.after_rays,
                        "Native ray descriptor lost explicit caster/boundary flags");
                    const unsigned layer=draw.after_rays || draw.packet.preserve_native_colour?0:
                        draw.layer==render::CalibratedGameLayer::world?1:
                        draw.layer==render::CalibratedGameLayer::model && draw.ray_caster?2:0;
                    require(p.effect_layer==layer,"Native descriptor lost independent world/model/protected effect coverage");
                    require(p.reflection_environment==draw.reflection_environment
                        && draw.reflection_environment==(draw.layer==render::CalibratedGameLayer::world && !draw.after_rays),
                        "Native scenery descriptor included model/UI or omitted authored world layers");
                    require(!p.reflective_material,"Default native scene unexpectedly selected a model material");
                    if(draw.after_rays) crossed_ray_boundary=true;
                    else require(!crossed_ray_boundary,"Native ray boundary reordered an early source pass");
                    if(draw.ray_caster) {
                        require(draw.layer==render::CalibratedGameLayer::model && !draw.after_rays
                            && !draw.packet.preserve_native_colour && draw.source_key<=0xffffU,
                            "Ground/UI/shadow/portrait pass selected as a model ray caster");
                        const auto object=std::find_if(frame.current->objects.begin(),frame.current->objects.end(),
                            [&](const auto& item){return item.handle==draw.source_key;});
                        require(object!=frame.current->objects.end(),"Native caster lost source identity");
                        for(const auto* name:{"LASERLINE","LASER_0","ELASER2","ELASER2_S2","ELASER2A",
                                "PLAYERBEAM","RINGLASER","OVALBEAM","BOSS_8_0"})
                            for(const auto address:symbols.find(name)) if(address>=0x8000U && address<=0xffffU)
                                require(object->object.shape!=address,"Emissive beam/nucleus selected as a ray caster");
                    }
                    if(draw.source_key==0x30000U)
                        require(draw.layer==render::CalibratedGameLayer::world && p.effects_override==settings.world_effects,
                            "Ground grids borrowed model-only styling");
                    if(draw.layer==render::CalibratedGameLayer::screen) {
                        require(i+1==frame.draws.size() && p.camera_override.has_value(),"Shutter is not final/screen locked");
                        require(draw.after_rays && !draw.ray_caster,"Shutter entered the world ray pass");
                    } else require(!p.camera_override,"Layer styling collapsed the calibrated eye cameras");
                    if(draw.layer==render::CalibratedGameLayer::model) ++models;
                    else ++artwork;
                }
                const auto pixels=gpu.render(scene,frame,check_rays);++rendered;
                if(ground && checkpoint==0 && alpha==1) {
                    unsigned gradients=0;
                    for(unsigned material:{1U,2U,3U,4U,5U,6U,7U,8U,9U}) {
                        auto selected=settings;selected.ground_material=material;
                        if(material>=5 && material<=7) {selected.ray_tracing=3;selected.reflections=3;selected.water_caustics=3;}
                        const auto manual=assembler.assemble(history.previous(),owned,game,alpha,selected,loader);
                        const auto receivers=manual.packets();
                        for(const auto& p:receivers) if(p.ground_receiver)
                            require(material>=5 && material<=7 && !p.ray_caster && !p.after_rays
                                && render::calibrated_ground_material(*p.packet)==material,
                                "Cartridge reflective floor borrowed model/post-ray ownership");
                        for(const auto& draw:manual.draws) {
                            const auto words=draw.packet.geometry.texel_view();
                            if(words.size()>15 && draw.layer==render::CalibratedGameLayer::world
                                && !draw.packet.geometry.vertex_view().empty()
                                && (draw.packet.geometry.vertex_view().front().texture[3]&render::calibrated_ground_flag)
                                && words.size()==render::calibrated_ground_offset+render::calibrated_ground_words) {
                                require(draw.reflection_environment && !draw.ray_caster && !draw.after_rays,
                                    "Enhanced ground left the actual world/reflection environment");
                                const auto offset=render::calibrated_ground_offset;
                                require(words[offset+8]==(material>=5?material:0),
                                    "Native manual ground selection lost its surface type");
                                const bool static_colour=words[offset+13]==0 && (words[offset+8]==0 || words[offset+8]==8);
                                require(std::bit_cast<float>(words[offset+11])==(static_colour?0.F:selected.effect_seconds),
                                    "Native surface dropped an active clock or retained an unused one");
                                ++gradients;
                            }
                        }
                        (void)gpu.render(scene,manual,check_rays);++rendered;
                    }
                    if(owned->background_landscape && owned->landscape_grid_height<0
                        && owned->flow!=simulation::GameFlowState::ex_pregame_menu)
                        require(gradients==9,"Native registered landscape lost a manual water/ground/metal/lava selection");
                    else require(!gradients,"Native terrain painted a menu/tunnel/space backdrop");
                    if(gradients) {
                        // Keep atlas/geometry unchanged: the cached ground
                        // regions must still follow a new live source palette.
                        auto live=std::make_shared<vr::GameSceneSnapshot>(*owned);
                        auto changed=std::make_shared<simulation::SnesPpuState>(*owned->ppu);
                        for(auto& ink:changed->cgram) ink=((ink&31)/2)|((((ink>>5)&31)/2)<<5)|((((ink>>10)&31)/2)<<10);
                        live->ppu=changed;live->cgram=changed->cgram;
                        auto selected=settings;selected.ground_material=1;
                        const auto normal=assembler.assemble(owned,owned,game,1,selected,loader);
                        const auto faded=assembler.assemble(live,live,game,1,selected,loader);
                        const auto endpoints=[](const auto& frame) {
                            for(const auto& draw:frame.draws) {
                                const auto words=draw.packet.geometry.texel_view();
                                if(words.size()==render::calibrated_ground_offset+render::calibrated_ground_words
                                    && !draw.packet.geometry.vertex_view().empty()
                                    && (draw.packet.geometry.vertex_view().front().texture[3]&(8U|render::calibrated_ground_flag))==(8U|render::calibrated_ground_flag)) {
                                    std::array<float,6> result{};
                                    for(unsigned c=0;c<6;++c) result[c]=std::bit_cast<float>(words[render::calibrated_ground_offset+c]);
                                    return result;
                                }
                            }
                            throw std::runtime_error("Palette change lost native ground");
                        };
                        const auto bright=endpoints(normal),dark=endpoints(faded);
                        for(unsigned c=0;c<6;++c) require(dark[c]<=bright[c]+.001F,"Native palette fade brightened the terrain");
                        require(bright!=dark && owned->ppu!=live->ppu,"Native palette cache froze or mutated retained input");
                        (void)gpu.render(scene,faded,check_rays);++rendered;
                        if(game.experience()==simulation::Experience::original) {
                            // Auto water used to select class 5, then lose its
                            // whole receiver because ramp extraction excluded
                            // that class. A live palette-only change exercises
                            // the actual cached cartridge assembler, not a
                            // handcrafted ground packet.
                            auto blue=std::make_shared<vr::GameSceneSnapshot>(*owned);
                            auto water_ppu=std::make_shared<simulation::SnesPpuState>(*owned->ppu);
                            for(unsigned ink=1;ink<256;++ink)
                                water_ppu->cgram[ink]=std::uint16_t((2+ink%5)|((8+ink%8)<<5)|((22+ink%10)<<10));
                            blue->ppu=water_ppu;blue->cgram=water_ppu->cgram;
                            selected.ground_material=0;selected.ray_tracing=0;selected.reflections=0;
                            selected.effect_seconds=1.375F;
                            const auto auto_water=assembler.assemble(blue,blue,game,1,selected,loader);
                            unsigned liquids=0;
                            for(const auto& packet:auto_water.packets()) if(render::calibrated_ground_material(*packet.packet)==5) {
                                require(!packet.ground_receiver && !packet.ray_caster,
                                    "RT-OFF Auto water secretly enabled optical ray tracing");
                                ++liquids;
                            }
                            require(liquids==1,"Live palette Auto water lost its native liquid receiver");
                            const auto wave1=gpu.render(scene,auto_water);++rendered;
                            selected.effect_seconds=2.25F;
                            const auto later=assembler.assemble(blue,blue,game,1,selected,loader);
                            require(endpoints(auto_water)==endpoints(later),
                                "Native water baked a clock-dependent CPU shade into its palette ramp");
                            const auto wave2=gpu.render(scene,later);++rendered;
                            require(wave1!=wave2,"RT-OFF native water failed to animate its wave shading");
                            require(gpu.render(scene,auto_water)==wave1,
                                "Retained RT-OFF water changed its source clock during retry");++rendered;
                        }
                    }
                    auto off=settings;off.enhanced_ground=false;
                    const auto restored=assembler.assemble(history.previous(),owned,game,alpha,off,loader);
                    auto baseline=off;baseline.ground_material=0;
                    const auto exact=assembler.assemble(history.previous(),owned,game,alpha,baseline,loader);
                    require(gpu.render(scene,restored)==gpu.render(scene,exact),"Ground OFF failed exact native frame restoration");
                }
                if(checkpoint==0 && alpha==1) {
                    auto host=frame;
                    render::Framebuffer ui(256,224);
                    for(int y=90;y<104;++y) for(int x=120;x<136;++x) ui.set(x,y,1);
                    std::array<render::Rgba8,256> ink{};ink[1]={255,220,64,255};
                    render::append_calibrated_host_ui(host,ui,ink,15,std::array<int,4>{12,20,244,223});
                    const auto host_packets=host.packets();
                    require(host.current==frame.current && host.previous==frame.previous
                        && !host_packets.back().camera_override && !host_packets.back().depth_test
                        && host_packets.back().effects_override==std::array<unsigned,4>{},
                        "Host GUI changed source ownership or borrowed a world effect/camera");
                    require(host_packets.back().after_rays && !host_packets.back().ray_caster,"Host GUI entered traced surfaces");
                    const auto gui=gpu.render(scene,host);
                    for(unsigned eye=0;eye<2;++eye) {
                        const auto at=(eye*Gpu::width*Gpu::height+97*Gpu::width+128)*4;
                        require(gui[at]==255 && gui[at+1]==220 && gui[at+2]==64 && gui[at+3]==255,
                            "Native host GUI was displaced or styled by the world/model effects");
                    }
                    const auto count=host.draws.size();bool rejected=false;
                    try {render::append_calibrated_host_ui(host,ui,ink,16);}
                    catch(const std::invalid_argument&) {rejected=true;}
                    require(rejected && host.draws.size()==count,"Invalid native GUI partially mutated its frame");
                }
                if(!capture.empty() && checkpoint==0 && alpha==1) {
                    for(unsigned eye=0;eye<2;++eye) {
                        auto* surface=SDL_CreateSurfaceFrom(Gpu::width,Gpu::height,SDL_PIXELFORMAT_RGBA32,
                            const_cast<std::uint8_t*>(pixels.data())+eye*Gpu::width*Gpu::height*4,Gpu::width*4);
                        require(surface,SDL_GetError());
                        const auto path=capture/(std::string(argv[map])+"-eye"+std::to_string(eye)+".bmp");
                        const bool saved=SDL_SaveBMP(surface,path.string().c_str());SDL_DestroySurface(surface);require(saved,SDL_GetError());
                    }
                }
                const auto midpoint=pixels.begin()+Gpu::width*Gpu::height*4;
                stereo_frames+=!std::equal(pixels.begin(),midpoint,midpoint);
                require(history.current()==owned && game.scene_revision()==game_revision,"Eye rendering advanced simulation");
            }
            // Failure does not publish a partial frame; existing immutable
            // frame/snapshot remain usable after a malformed interpolation.
            bool rejected=false;
            try {(void)assembler.assemble(history.previous(),owned,game,std::nan(""),settings);}
            catch(const std::invalid_argument&) {rejected=true;}
            require(rejected && history.current()==owned,"Malformed frame mutated published source state");
            for(unsigned fault=0;fault<3;++fault) {
                auto invalid=settings;
                if(fault==2) invalid.ground_motion=4;else if(fault) invalid.chromatic=4;else invalid.contrast=4;
                rejected=false;
                try {(void)assembler.assemble(history.previous(),owned,game,1,invalid);}
                catch(const std::invalid_argument&) {rejected=true;}
                require(rejected && history.current()==owned,"Malformed appearance settings mutated published source state");
            }
        }
        std::cout<<argv[map]<<": native model/ground passes="<<models<<" backdrop/UI/effect passes="<<artwork
            <<" retained source effect points="<<captured_points<<'\n';
    }
    require(stereo_frames>0,"Game integration produced identical flat eyes");
    std::cout<<"Calibrated game integration: "<<rendered<<" owned native frames, "<<stereo_frames<<" distinct-eye frames; no per-eye simulation ticks.\n";
    std::cout<<"Finite-floor source correspondence: "<<finite_floors<<" retained floors, "<<matched_floors
        <<" valid static histories and "<<radiance_rejections<<" locally rejected fluid/radiance histories.\n";
    std::cout<<"Frozen cartridge static-ground clock: "<<static_clock_holds<<" exact freshly assembled palette/terrain identities.\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
