// PCVR MODERN capture (fork-local): one game snapshot drawn by both MODERN
// paths offscreen, with no headset:
//   vr   - the PCVR Vulkan renderer as src/vr/application.cpp draws it
//          (vr::ModernFrame with the headset settings, VulkanModernLayers and
//          the VulkanSourceScene world), from one eye at the seat;
//   sdl  - the flat SDL_GPU renderer's headset preview of the same frame
//          (STARFOX_MODERN_HEADSET_PREVIEW in starfox_pc).
// Both use the same eye camera, so the images should match closely; the
// difference report says by how much.
//
// Usage: starfox_vr_modern_capture ROM SYMBOLS LEVEL TICKS OUT_DIR [WIDTH HEIGHT [YAW PITCH [FOV]]]
//   TICKS source ticks after the level's checkpoint; YAW left, PITCH up, FOV
//   horizontal, in degrees (default 0, 0, 100). STARFOX_TEST_ARMADA_APPROACH=1
//   enters Space Armada's corridor approach first, after
//   STARFOX_TEST_PREROLL_TICKS (default 200) so the level's own setup has
//   run, as starfox_pc does.
// Writes OUT_DIR/LEVEL-TICKS-vr.bmp and OUT_DIR/LEVEL-TICKS-sdl.bmp.
//
// STARFOX_MODERN_RT_SHADOWS=1 traces the sun (both paths) and, with
// STARFOX_MODERN_RT_LIGHTS=1, the coloured lights, as the RAY TRACING and
// DYNAMIC LIGHTING rows do; without it the engine glow is the decal.
// STARFOX_VR_EYE=1 draws as the right eye (its own slot in the ray data).
// STARFOX_TEST_DEFEAT=SHAPE,STRATEGY,TICK (starfox_pc's): at that source tick
// the first object of that shape dies through that strategy.
// STARFOX_VR_OBJECTS=1 lists the last tick's objects and world packets.
// STARFOX_VR_FIRST_PERSON=1 presses SELECT once the level starts (the
// first-person view).
// STARFOX_VR_HIDE=key,key,... (hex) leaves those world packets out (finding
// which object draws what).
// STARFOX_VR_TIME=N redraws the Vulkan eye N more times and reports the
// fastest GPU times (timestamps) of the once-per-frame work (ray casters,
// xBRZ layer) and of the eye's pass; the readback is not included.
//
// STARFOX_VR_CLASSIC=1 draws the existing (non-MODERN) PCVR renderer instead,
// to OUT_DIR/LEVEL-TICKS-classic.bmp. MODERN loads replacement artwork from
// <exe>/textures/ for both paths, as starfox_pc and starfox_pcvr do.
#include "starfox/modern/sdl_frame.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/audio/stem_mixer.hpp"
#include "starfox/vr/decal_surface.hpp"
#include "starfox/vr/game_frame_driver.hpp"
#include "starfox/vr/modern_frame.hpp"
#include "starfox/vr/ray_lighting.hpp"
#include "starfox/vr/vulkan_ray_shadows.hpp"
#include "starfox/vr/source_models.hpp"
#include "starfox/vr/vulkan_depth_targets.hpp"
#include "starfox/vr/vulkan_eye_commands.hpp"
#include "starfox/vr/vulkan_eye_targets.hpp"
#include "starfox/vr/vulkan_loader.hpp"
#include "starfox/vr/vulkan_modern_layers.hpp"
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_source_scene.hpp"
#include "starfox/render/texture_override_files.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <set>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace starfox;
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what);}
void check(VkResult result,const char* what) {require(result==VK_SUCCESS,std::string(what)+" ("+std::to_string(int(result))+")");}
template<class F> F function(PFN_vkVoidFunction p) {require(p!=nullptr,"Missing Vulkan entry point");return reinterpret_cast<F>(p);}
// RGBA rows, top first, to a 24-bit BMP.
void write_bmp(const std::filesystem::path& path,const std::vector<unsigned char>& rgba,uint32_t width,uint32_t height) {
    std::ofstream out(path,std::ios::binary);require(bool(out),"Cannot write "+path.string());
    const uint32_t row=width*3,padded=(row+3)&~3U;
    unsigned char header[54]{'B','M'};
    const auto put=[&](unsigned offset,uint32_t value){for(unsigned i=0;i<4;++i) header[offset+i]=uint8_t(value>>(8*i));};
    put(2,54+padded*height);put(10,54);put(14,40);put(18,width);put(22,height);header[26]=1;header[28]=24;put(34,padded*height);
    out.write(reinterpret_cast<const char*>(header),54);
    std::vector<unsigned char> line(padded);
    for(uint32_t y=height;y-->0;) {
        for(uint32_t x=0;x<width;++x) {const auto* p=rgba.data()+(std::size_t(y)*width+x)*4;line[x*3]=p[2];line[x*3+1]=p[1];line[x*3+2]=p[0];}
        out.write(reinterpret_cast<const char*>(line.data()),padded);
    }
}
// modern_runtime.cpp's headset_preview_camera: vr::eye_camera's conventions
// (Vulkan Y down, infinite far, near 0.05 m) for an eye at the seat turned by
// yaw then pitch.
vr::EyeCamera headset_camera(float yaw,float pitch,float fov,float aspect) noexcept {
    constexpr float degrees=3.14159265358979F/180.F;
    const float cy=std::cos(yaw*degrees),sy=std::sin(yaw*degrees);
    const float cp=std::cos(pitch*degrees),sp=std::sin(pitch*degrees);
    const float r[3][3]{{cy,sy*sp,sy*cp},{0,cp,-sp},{-sy,cy*sp,cy*cp}};
    vr::EyeCamera camera{};
    for(unsigned row=0;row<3;++row) for(unsigned column=0;column<3;++column) camera.view[column*4+row]=r[column][row];
    camera.view[15]=1;
    const float x=std::tan(fov*.5F*degrees),y=x/std::max(aspect,.01F);
    auto& m=camera.projection;
    m[0]=1/x;m[5]=-1/y;m[10]=-1;m[11]=-1;m[14]=-.05F;
    return camera;
}
// starfox_pc's STARFOX_TEST_ARMADA_APPROACH: continue LEVEL1_3's map script
// at its corridor background command.
void enter_armada_approach(simulation::GameSimulation& game,const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    const auto entry=symbols.find("LEVEL1_3").at(0);
    const auto background=static_cast<std::uint16_t>(symbols.find("BG_1_3C").at(0)-symbols.find("BGLISTS").at(0));
    auto limit=(entry&0xff0000U)+0xfffeU;
    for(const auto& [name,values]:symbols.entries()) {
        if(!name.starts_with("LEVEL")) continue;
        for(const auto value:values) if(value>entry && value<limit) limit=value;
    }
    std::uint32_t command{};
    for(auto pc=entry;pc+2U<limit;++pc)
        if(rom.read8(pc)==16U && rom.read16(pc+1U)==background) {command=pc;break;}
    require(command!=0,"Armada corridor background command not found");
    game.set_god_mode(true);
    game.map().start(command,game.player());
    game.map().advance_distance(1);
}
// application.cpp's existing renderer: the surrounding stars drawn first,
// outside the world, and gameplay objects 0.25 m further away (the seat).
void split_classic_world(vr::SourceModelPackets& packets,const vr::GameSceneSnapshot& current,
    std::vector<vr::DrawPacket>& stars,std::vector<uint32_t>& keys) {
    const auto star=std::find(packets.handles.begin(),packets.handles.end(),0x20000U);
    require(star!=packets.handles.end(),"Missing surrounding star pass");
    const auto index=std::size_t(star-packets.handles.begin());
    stars.assign(1,packets.packets[index]);keys.assign(1,*star);
    packets.packets.erase(packets.packets.begin()+std::ptrdiff_t(index));packets.handles.erase(star);
    for(auto& compute:packets.compute_models) if(compute.packet_index>index) --compute.packet_index;
    if(current.flow==simulation::GameFlowState::gameplay || current.flow==simulation::GameFlowState::training)
        for(std::size_t i=0;i<packets.packets.size();++i)
            if((packets.handles[i]&0xffffU)!=0 && packets.handles[i]<0x20000U) packets.packets[i].model[14]-=.25F;
}
struct Difference {double mean{};unsigned maximum{};double over_8{};};
Difference compare(const std::vector<unsigned char>& a,const std::vector<unsigned char>& b) {
    Difference d;std::size_t pixels=a.size()/4,over=0;double sum=0;
    for(std::size_t i=0;i<pixels;++i) {
        unsigned worst=0;
        for(unsigned c=0;c<3;++c) worst=std::max(worst,unsigned(std::abs(int(a[i*4+c])-int(b[i*4+c]))));
        sum+=worst;d.maximum=std::max(d.maximum,worst);if(worst>8) ++over;
    }
    d.mean=pixels?sum/double(pixels):0;d.over_8=pixels?100.*double(over)/double(pixels):0;
    return d;
}
}
// STARFOX_VR_STEER=x,y[,from]: hold the steering stick from tick `from`
// (default 0), to see how the scene and its backgrounds follow a turn.
static starfox::vr::VrControls steer_controls(unsigned tick) {
    static const auto steer=[] {
        std::array<float,3> value{0,0,-1};
        if(const char* text=std::getenv("STARFOX_VR_STEER")) {
            value[2]=0;std::sscanf(text,"%f,%f,%f",&value[0],&value[1],&value[2]);
        }
        return value;
    }();
    starfox::vr::VrControls controls;
    if(steer[2]>=0 && float(tick)>=steer[2]) controls.steer={steer[0],steer[1]};
    // STARFOX_VR_FIRE=1: hold the trigger (alternate ticks, so the shots repeat).
    static const bool fire=std::getenv("STARFOX_VR_FIRE")!=nullptr;
    controls.fire=fire && (tick&1U)==0;
    return controls;
}
int main(int argc,char** argv) try {
    if(argc<6) {
        std::cerr<<"Usage: "<<argv[0]<<" ROM SYMBOLS LEVEL TICKS OUT_DIR [WIDTH HEIGHT [YAW PITCH [FOV]]]\n";return 2;
    }
    const auto rom=assets::RomImage::load(argv[1]);
    const auto symbols=assets::SymbolMap::load(argv[2]);
    const std::string level=argv[3];
    const unsigned ticks=unsigned(std::stoul(argv[4]));
    const std::filesystem::path out_dir=argv[5];
    const uint32_t width=argc>7?uint32_t(std::stoul(argv[6])):1024,height=argc>7?uint32_t(std::stoul(argv[7])):1024;
    const float yaw=argc>9?std::stof(argv[8]):0.F,pitch=argc>9?std::stof(argv[9]):0.F,fov=argc>10?std::stof(argv[10]):100.F;
    std::filesystem::create_directories(out_dir);
    const bool classic=std::getenv("STARFOX_VR_CLASSIC") && std::string_view(std::getenv("STARFOX_VR_CLASSIC"))=="1";
    // As application.cpp does before any pipeline exists with MODERN on.
    if(!classic) {
        vr::set_scene_reversed_depth(true);vr::set_inset_decals(true);vr::set_solid_decals(true);
        vr::set_source_ui_distance(vr::headset_ui_distance());
    }
    constexpr bool srgb=true; // The PCVR swapchains are sRGB.

    simulation::GameSimulation game(rom,symbols,level,{},true);
    game.set_timing_mode(simulation::TimingMode::unlocked_20_fps);
    // STARFOX_TEST_GOD_MODE=1 (starfox_pc's): an unattended ship survives a whole level.
    if(const char* god=std::getenv("STARFOX_TEST_GOD_MODE")) game.set_god_mode(std::string_view(god)=="1");
    audio::Spc700Audio spc;audio::Msu1Audio msu;std::vector<int16_t> mixed;
    // Levels wait for their checkpoint; the intro (as application.cpp's
    // LiveGame does) and other screens start at once.
    // STARFOX_VR_FROM_START=1: a level from its first tick (its launch).
    const bool native_intro=!level.starts_with("LEVEL") || (std::getenv("STARFOX_VR_FROM_START")
        && std::string_view(std::getenv("STARFOX_VR_FROM_START"))=="1");
    const auto checkpoint=native_intro?0:symbols.find("MAPRESTART").at(0);
    unsigned warmup=0;
    while(!native_intro && !game.map().peek_ram_word(checkpoint).value() && warmup<3000) {
        const auto tick=game.tick({});
        audio::render_mixed_tick(spc,msu,tick.audio_port_writes,game.map().take_msu_register_writes(),
            game.music_volume(),game.sfx_volume(),mixed);
        game.synchronize_apu_output_ports(spc.output_ports());++warmup;
    }
    require(warmup<3000,"Live game checkpoint did not start");
    if(std::getenv("STARFOX_TEST_ARMADA_APPROACH")) {
        const char* preroll=std::getenv("STARFOX_TEST_PREROLL_TICKS");
        for(unsigned tick=0,count=preroll?unsigned(std::stoul(preroll)):200U;tick<count;++tick) {
            const auto advanced=game.tick({});
            audio::render_mixed_tick(spc,msu,advanced.audio_port_writes,game.map().take_msu_register_writes(),
                game.music_volume(),game.sfx_volume(),mixed);
            game.synchronize_apu_output_ports(spc.output_ports());
        }
        enter_armada_approach(game,rom,symbols);
    }
    // STARFOX_TEST_MAP_JUMP=<label> (starfox_pc's): after the preroll, continue
    // the level's map script at an authored label (god mode on).
    if(const char* label=std::getenv("STARFOX_TEST_MAP_JUMP")) {
        const char* preroll=std::getenv("STARFOX_TEST_PREROLL_TICKS");
        for(unsigned tick=0,count=preroll?unsigned(std::stoul(preroll)):200U;tick<count;++tick) {
            const auto advanced=game.tick({});
            audio::render_mixed_tick(spc,msu,advanced.audio_port_writes,game.map().take_msu_register_writes(),
                game.music_volume(),game.sfx_volume(),mixed);
            game.synchronize_apu_output_ports(spc.output_ports());
        }
        game.set_god_mode(true);
        // LABEL+N continues N bytes into the label's script (a local label, or
        // the command after a mapjsr).
        const std::string_view target(label);const auto plus=target.find('+');
        const auto offset=plus==std::string_view::npos?0U:uint32_t(std::stoul(std::string(target.substr(plus+1))));
        game.map().start(symbols.find(std::string(target.substr(0,plus))).at(0)+offset,game.player());
        game.map().advance_distance(1);
    }
    // STARFOX_TEST_ENDING=1 (starfox_pc's fixture): the real post-Andross
    // ending with a full route history (scores and the six bosses), from
    // FINALMAP_END. TICKS then count from the ending's start.
    if(std::getenv("STARFOX_TEST_ENDING")) {
        const auto symbol=[&](const char* name) {return symbols.find(name).at(0);};
        constexpr std::array<unsigned,6> scores{100,90,80,70,60,100};
        constexpr std::array bosses{"BOSS11","BOSS12","BOSS13","BOSS14","BOSS15","BOSSFINAL"};
        game.map().write_native_word(symbol("SPECPTR"),uint16_t(scores.size()));
        for(std::size_t i=0;i<scores.size();++i) {
            game.map().write_native_byte(symbol("SPECBUF")+uint32_t(i),uint8_t(scores[i]));
            game.map().write_native_word(symbol("BOSS_SEQ")+uint32_t(2*i),uint16_t(symbol(bosses[i])-symbol("ENDSEQBOSS")));
        }
        game.map().write_native_word(symbol("BOSS_PTR"),uint16_t(2*bosses.size()));
        game.set_god_mode(true);
        game.start_map("FINALMAP_END");
    }
    if(std::getenv("STARFOX_VR_FIRST_PERSON")) {
        for(unsigned tick=0;tick<40;++tick) {
            input::TickInput press{};
            if(tick==20) {press.held=press.pressed=input::select;}
            else if(tick==21) press.released=input::select;
            const auto advanced=game.tick(press);
            audio::render_mixed_tick(spc,msu,advanced.audio_port_writes,game.map().take_msu_register_writes(),
                game.music_volume(),game.sfx_volume(),mixed);
            game.synchronize_apu_output_ports(spc.output_ports());
        }
    }
    vr::GameSceneHistory history(game,rom,symbols);
    vr::GameFrameDriver driver(game,[&](auto apu,auto writes) {
        audio::render_mixed_tick(spc,msu,apu,writes,game.music_volume(),game.sfx_volume(),mixed);
        return spc.output_ports();
    },&history);
    vr::SourceModels models(rom,symbols,true,true);
    if(const char* projectiles=std::getenv("STARFOX_TEST_PROJECTILE_MODELS")) models.set_projectile_models(std::string_view(projectiles)=="1");
    if(const char* explosions=std::getenv("STARFOX_TEST_EXPLOSION_MODELS")) models.set_explosion_models(std::string_view(explosions)!="0");
    if(const char* asteroids=std::getenv("STARFOX_TEST_ASTEROID_MODELS"))
        models.set_asteroid_models(static_cast<render::AsteroidModels>(std::clamp(std::atoi(asteroids),0,int(render::asteroid_model_mode_count)-1)));
    render::ScaledTextRenderer dialogue_layout(rom,symbols);
    // application.cpp's MODERN inputs (VR's own cockpit, no flat-screen
    // substitutes); the STARFOX_MODERN_* switches apply as they do there.
    const auto trigonometry=simulation::TrigTables::load(rom,symbols);
    vr::FrameLayerInputs inputs;
    inputs.rom=&rom;inputs.symbols=&symbols;inputs.dialogue_layout=&dialogue_layout;
    inputs.language=game.language();inputs.srgb=srgb;
    inputs.ex_experience=game.experience()==simulation::Experience::starfox_ex;
    const auto enabled=[](const char* name) {const char* value=std::getenv(name);return !(value && std::string_view(value)=="0");};
    if(!classic) {
        inputs.tunnel_corridor=enabled("STARFOX_MODERN_CORRIDOR");
        inputs.landscape_world=enabled("STARFOX_MODERN_LANDSCAPE");
        inputs.classic_ground_scroll=enabled("STARFOX_MODERN_CLASSIC_SCROLL");
        inputs.landscape_ground_dots=enabled("STARFOX_MODERN_GROUND_DOTS");
        inputs.vector_reticle=true;inputs.bg3_behind_bg2=true;
        inputs.trigonometry=&trigonometry; // MHUD wing indicators, as application.cpp
    }
    // Replacement artwork (MODERN only), shared by both paths.
    const auto textures=(std::filesystem::absolute(argv[0]).parent_path()/"textures").string()+"/";
    render::TextureOverrides texture_overrides,portrait_overrides;
    render::SpriteOverrides sprite_overrides;
    if(!classic) {
        texture_overrides=render::load_texture_overrides(textures);
        portrait_overrides=render::load_portrait_overrides(textures);
        sprite_overrides=render::load_sprite_overrides(textures);
        if(!texture_overrides.empty()) models.set_texture_overrides(&texture_overrides);
        if(!portrait_overrides.empty()) inputs.portrait_overrides=&portrait_overrides;
        if(!sprite_overrides.empty()) inputs.sprite_overrides=&sprite_overrides;
    }
    const vr::ModernFrame::Settings seat{true,.25F};
    const auto camera=headset_camera(yaw,pitch,fov,float(width)/float(height));

    // Vulkan: a plain device and one offscreen sRGB eye with reversed depth.
    vr::VulkanLoader loader;require(loader.initialize(),loader.status());
    const auto get=loader.get_instance_proc_addr();
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};application.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};instance_info.pApplicationInfo=&application;
    VkInstance instance{};check(function<PFN_vkCreateInstance>(get(nullptr,"vkCreateInstance"))(&instance_info,nullptr,&instance),"Create Vulkan instance");
#define INSTANCE(name) auto name=function<PFN_##name>(get(instance,#name))
    INSTANCE(vkDestroyInstance);INSTANCE(vkEnumeratePhysicalDevices);INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
    INSTANCE(vkGetPhysicalDeviceMemoryProperties);INSTANCE(vkGetPhysicalDeviceProperties);
    INSTANCE(vkCreateDevice);INSTANCE(vkGetDeviceProcAddr);
    uint32_t count{};check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"Count physical devices");
    std::vector<VkPhysicalDevice> devices(count);check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"Enumerate physical devices");
    // Prefer a discrete GPU (the headset's), then any graphics+compute queue.
    VkPhysicalDevice physical{};uint32_t family{};
    for(const bool discrete:{true,false}) for(auto candidate:devices) {
        if(physical) break;
        VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(candidate,&p);
        if(discrete && p.deviceType!=VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) continue;
        uint32_t families{};vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,queues.data());
        for(uint32_t i=0;i<families;++i)
            if(queues[i].queueCount && (queues[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)) {
                physical=candidate;family=i;break;
            }
    }
    require(physical!=VK_NULL_HANDLE,"No Vulkan graphics queue");
    VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(physical,&properties);
    VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(physical,&memory);
    const float priority=1;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex=family;queue_info.queueCount=1;queue_info.pQueuePriorities=&priority;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device_info.queueCreateInfoCount=1;device_info.pQueueCreateInfos=&queue_info;
    VkDevice device{};check(vkCreateDevice(physical,&device_info,nullptr,&device),"Create Vulkan device");
#define DEVICE(name) auto name=function<PFN_##name>(vkGetDeviceProcAddr(device,#name))
    DEVICE(vkDestroyDevice);DEVICE(vkDeviceWaitIdle);DEVICE(vkGetDeviceQueue);
    DEVICE(vkCreateImage);DEVICE(vkDestroyImage);DEVICE(vkGetImageMemoryRequirements);DEVICE(vkBindImageMemory);
    DEVICE(vkCreateBuffer);DEVICE(vkDestroyBuffer);DEVICE(vkGetBufferMemoryRequirements);DEVICE(vkBindBufferMemory);
    DEVICE(vkAllocateMemory);DEVICE(vkFreeMemory);DEVICE(vkMapMemory);DEVICE(vkUnmapMemory);
    DEVICE(vkCmdPipelineBarrier);DEVICE(vkCmdCopyImageToBuffer);
    DEVICE(vkCreateQueryPool);DEVICE(vkDestroyQueryPool);DEVICE(vkCmdResetQueryPool);DEVICE(vkCmdWriteTimestamp);
    DEVICE(vkGetQueryPoolResults);
    VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
    std::cout<<"Vulkan: "<<properties.deviceName<<" | level "<<level<<" tick "<<ticks<<" | "<<width<<'x'<<height
        <<" yaw "<<yaw<<" pitch "<<pitch<<" fov "<<fov<<'\n';
    const auto allocate=[&](VkMemoryRequirements required,VkMemoryPropertyFlags flags) {
        uint32_t type=memory.memoryTypeCount;
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)
            if((required.memoryTypeBits&(1U<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags) {type=i;break;}
        require(type<memory.memoryTypeCount,"No compatible Vulkan memory");
        VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};info.allocationSize=required.size;info.memoryTypeIndex=type;
        VkDeviceMemory result{};check(vkAllocateMemory(device,&info,nullptr,&result),"Allocate Vulkan memory");return result;
    };
    constexpr VkFormat colour_format=VK_FORMAT_R8G8B8A8_SRGB;
    VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType=VK_IMAGE_TYPE_2D;image_info.format=colour_format;image_info.extent={width,height,1};
    image_info.mipLevels=image_info.arrayLayers=1;image_info.samples=VK_SAMPLE_COUNT_1_BIT;image_info.tiling=VK_IMAGE_TILING_OPTIMAL;
    image_info.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage image{};check(vkCreateImage(device,&image_info,nullptr,&image),"Create eye image");
    VkMemoryRequirements required{};vkGetImageMemoryRequirements(device,image,&required);
    const auto image_memory=allocate(required,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);check(vkBindImageMemory(device,image,image_memory,0),"Bind eye image");
    VkBufferCreateInfo read_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};read_info.size=VkDeviceSize(width)*height*4;read_info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer readback{};check(vkCreateBuffer(device,&read_info,nullptr,&readback),"Create readback buffer");
    vkGetBufferMemoryRequirements(device,readback,&required);
    const auto read_memory=allocate(required,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vkBindBufferMemory(device,readback,read_memory,0),"Bind readback buffer");
    const auto stem=out_dir/(level+"-"+std::to_string(ticks));
    // MODERN's haze, registered before any pipeline exists (as application.cpp does).
    vr::VulkanSceneFog fog;
    if(!classic) {require(fog.initialize(device,vkGetDeviceProcAddr,memory),fog.status());vr::set_scene_fog(&fog);}
    // And its ray tracing, likewise.
    const bool rt=!classic && std::getenv("STARFOX_MODERN_RT_SHADOWS") && std::string_view(std::getenv("STARFOX_MODERN_RT_SHADOWS"))=="1";
    vr::VulkanRayShadows rays;
    if(rt) {require(rays.initialize(device,vkGetDeviceProcAddr,memory,properties.limits),rays.status());vr::set_scene_rays(&rays.binding());}
    {
        const std::array<VkExtent2D,2> extents{VkExtent2D{width,height},VkExtent2D{width,height}};
        vr::VulkanDepthTargets depth;require(depth.initialize(instance,physical,device,get,extents),depth.status());
        vr::VulkanEyeTargets targets;
        const std::array<std::span<const VkImage>,2> images{std::span(&image,1),std::span(&image,1)};
        require(targets.initialize(device,vkGetDeviceProcAddr,colour_format,images,extents,depth.format(),depth.views()),targets.status());
        vr::VulkanEyeCommands commands;require(commands.initialize(device,queue,family,vkGetDeviceProcAddr),commands.status());
        const VkExtent2D extent{width,height};
        // One eye: before_render (compute), the render pass, then the readback.
        const auto draw_eye=[&](std::array<float,4> clear,const vr::VulkanEyeCommands::Record& record,
            const vr::VulkanEyeCommands::Record& before_render) {
            VkClearColorValue clear_value{};std::copy(clear.begin(),clear.end(),clear_value.float32);clear_value.float32[3]=1;
            require(commands.submit(targets,0,0,clear_value,record,before_render,[&](VkCommandBuffer cmd,VkExtent2D) {
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
                barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                barrier.image=image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
                VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={width,height,1};
                vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&region);
            }),"Submit Vulkan eye: "+commands.status());
            while(true) {
                const auto state=commands.poll(1'000'000);
                require(state!=vr::VulkanEyeCommands::Completion::error,"Vulkan eye failed: "+commands.status());
                if(state==vr::VulkanEyeCommands::Completion::complete) break;
            }
            void* mapped{};check(vkMapMemory(device,read_memory,0,VK_WHOLE_SIZE,0,&mapped),"Map readback");
            const auto* bytes=static_cast<const unsigned char*>(mapped);
            std::vector<unsigned char> pixels(bytes,bytes+std::size_t(width)*height*4);vkUnmapMemory(device,read_memory);
            return pixels;
        };
        if(classic) {
            // application.cpp's existing renderer: its world (stars split
            // off, the seat setback), the source 2D layers from
            // vr::assemble_frame_layers with every MODERN option off and
            // frame_background_models; no reversed depth, no MODERN effects.
            vr::VulkanDrawPackets stars,backgrounds,surround,sprites;
            vr::VulkanSourceScene world;
            // (Circle wipes and the shutter are drawn as before and not captured.)
            const auto upload=[&](vr::VulkanDrawPackets& layer,std::span<const vr::DrawPacket> packets,std::span<const uint32_t> keys={}) {
                require(layer.initialize(device,vkGetDeviceProcAddr,memory,targets.render_pass(),packets,keys,false),layer.status());
            };
            const auto render=[&](const vr::GameSceneSnapshot& current,const vr::SourceModelPackets& source,
                const std::vector<vr::DrawPacket>& star_packets,const std::vector<uint32_t>& star_keys,
                const vr::FrameLayers& layers,std::span<const vr::Matrix4> motion) {
                upload(stars,star_packets,star_keys);upload(backgrounds,layers.backgrounds);
                require(backgrounds.update_models(motion),"Classic background motion update failed");
                upload(surround,layers.tunnel_surround);upload(sprites,layers.sprites);
                require(world.initialize(device,vkGetDeviceProcAddr,memory,properties.limits,targets.render_pass(),source),world.status());
                return draw_eye(vr::frame_clear_colour(current,srgb),[&](VkCommandBuffer cmd,VkExtent2D size) {
                    if(!stars.record(cmd,size,camera) || !backgrounds.record(cmd,size,camera) || !surround.record(cmd,size,camera)
                        || !world.record(cmd,size,camera) || !sprites.record(cmd,size,camera))
                        throw std::runtime_error("Vulkan classic recording failed: "+world.status());
                },[&](VkCommandBuffer cmd,VkExtent2D) {
                    if(!world.record_compute(cmd)) throw std::runtime_error("Vulkan world compute failed: "+world.status());
                });
            };
            for(unsigned tick=0;tick<=ticks;++tick) {
                driver.advance(XrTime(tick)*50'000'000,steer_controls(tick),true);
                const auto previous=history.previous(),current=history.current();
                require(previous && current && current->ppu,"Scene history incomplete");
                if(tick!=ticks) continue;
                // Stateless per snapshot, unlike MODERN's plan.
                const auto layers=vr::assemble_frame_layers(*current,inputs);
                const auto motion=vr::frame_background_models(layers.backgrounds,*previous,*current,1.,inputs);
                auto source=models.assemble_world_interpolated(*previous,*current,1.,srgb,true);
                require(source.pending.empty(),"Model assembly pending");
                std::vector<vr::DrawPacket> star_packets;std::vector<uint32_t> star_keys;
                split_classic_world(source,*current,star_packets,star_keys);
                const auto after=render(*current,source,star_packets,star_keys,layers,motion);
                write_bmp(stem.string()+"-classic.bmp",after,width,height);
                std::cout<<"classic: "<<stem.string()<<"-classic.bmp\n";
            }
        } else {
            vr::VulkanModernLayers layers;layers.initialize(device,vkGetDeviceProcAddr,memory,targets.render_pass());
            vr::VulkanSourceScene world;world.set_modern(true);
            vr::ModernFrame frame;frame.set_settings(seat);
            frame.set_image_loader([&](const std::string& name) -> std::optional<vr::ModernImage> {
                auto image=render::load_rgba_image(textures+name);
                if(!image) return {};
                return vr::ModernImage{image->width,image->height,std::move(image->rgba)};
            });
            // SDL_GPU: the flat renderer with the headset preview's settings.
            require(SDL_Init(SDL_INIT_VIDEO),std::string("SDL_Init: ")+SDL_GetError());
            auto* sdl=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,false,"vulkan");
            require(sdl!=nullptr,std::string("SDL_CreateGPUDevice: ")+SDL_GetError());
            constexpr auto sdl_colour=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
            const auto sdl_depth=SDL_GPUTextureSupportsFormat(sdl,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)?SDL_GPU_TEXTUREFORMAT_D32_FLOAT:SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            SDL_GPUTextureCreateInfo colour_info{};
            colour_info.type=SDL_GPU_TEXTURETYPE_2D;colour_info.format=sdl_colour;colour_info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
            colour_info.width=width;colour_info.height=height;colour_info.layer_count_or_depth=1;colour_info.num_levels=1;
            auto* sdl_target=SDL_CreateGPUTexture(sdl,&colour_info);require(sdl_target!=nullptr,"Create SDL colour target");
            auto depth_info=colour_info;depth_info.format=sdl_depth;depth_info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            auto* sdl_depth_target=SDL_CreateGPUTexture(sdl,&depth_info);require(sdl_depth_target!=nullptr,"Create SDL depth target");
            SDL_GPUTransferBufferCreateInfo download_info{};download_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;download_info.size=width*height*4;
            auto* download=SDL_CreateGPUTransferBuffer(sdl,&download_info);require(download!=nullptr,"Create SDL download buffer");
            modern::SdlFrameRenderer flat;
            require(flat.initialize(sdl,sdl_colour,sdl_depth),flat.status());
            // The line brush at the eye's scale (modern_runtime.cpp's headset preview).
            flat.set_frame_settings(seat);flat.set_texture_directory(textures);
            flat.set_target(width,height,std::max(1U,unsigned(std::lround(double(height)*std::abs(camera.projection[5])/512.))));
            std::vector<unsigned char> vr_pixels,sdl_pixels;
            std::function<void()> redraw;
            VkQueryPool timer{};
            {
                VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};info.queryType=VK_QUERY_TYPE_TIMESTAMP;info.queryCount=3;
                check(vkCreateQueryPool(device,&info,nullptr,&timer),"Create timestamp pool");
            }
            const bool rt_lights=std::getenv("STARFOX_MODERN_RT_LIGHTS") && std::string_view(std::getenv("STARFOX_MODERN_RT_LIGHTS"))=="1";
            const vr::RayEmitters emitters(symbols);
            // Both plans see every tick, so their per-level state (horizon votes,
            // retained layers) matches the game's. Source frames: alpha 1.
            const auto symbol_name=[&](std::uint32_t value,bool word) {
                for(const auto& [symbol,values]:symbols.entries())
                    for(const auto v:values) if(word?(v&0xffffU)==value && (v>>16U)<0x70U:v==value) return symbol;
                return std::string{"?"};
            };
            for(unsigned tick=0;tick<=ticks;++tick) {
                if(const char* defeat=std::getenv("STARFOX_TEST_DEFEAT")) {
                    const std::string spec{defeat};
                    const auto a=spec.find(','),b=spec.rfind(',');
                    require(a!=std::string::npos && b!=a,"STARFOX_TEST_DEFEAT needs SHAPE,STRATEGY,TICK");
                    if(tick==std::stoul(spec.substr(b+1))) {
                        std::uint32_t shape{};
                        for(const auto value:symbols.find(spec.substr(0,a))) if(value<0x10000U) {shape=value;break;}
                        const auto strategy=symbols.find(spec.substr(a+1,b-a-1)).at(0);
                        unsigned killed{};
                        for(const auto handle:game.objects().active_handles()) {
                            auto& object=game.objects().at(handle);
                            if(!shape || object.shape!=shape) continue;
                            object.health=0U;object.strategy_flags[1]|=0x01U;object.strategy_address=strategy;++killed;break;
                        }
                        std::cout<<"scheduled-defeat tick="<<tick<<" objects="<<killed<<'\n';
                    }
                }
                driver.advance(XrTime(tick)*50'000'000,steer_controls(tick),true);
                const auto previous=history.previous(),current=history.current();
                require(previous && current && current->ppu,"Scene history incomplete");
                const bool last=tick==ticks;
                if(std::getenv("STARFOX_VR_SCROLL") && tick+std::stoul(std::getenv("STARFOX_VR_SCROLL"))>=ticks)
                    std::cout<<"scroll tick="<<tick<<" bg2="<<current->ppu->bg2_scroll_x<<','<<current->ppu->bg2_scroll_y
                        <<" camera yaw="<<current->camera.yaw<<" pitch="<<current->camera.pitch<<" roll="<<current->camera.roll<<" hofs_en="<<current->ppu->bg2_horizontal_offsets_enabled<<" hofs112="<<current->ppu->bg2_horizontal_offsets[112]<<" vofs_en="<<current->ppu->bg2_vertical_offsets_enabled<<" cam_x="<<current->camera.x<<" cam_y="<<current->camera.y<<" cam_z="<<current->camera.z<<'\n';
                // STARFOX_VR_BG2=1: the last tick's whole BG2 tile map (1024x1024 art
                // pixels from 0,0) as <stem>-bg2.bmp, to see what lies off screen.
                if(last && std::getenv("STARFOX_VR_BG2")) {
                    const auto art=vr::bg2_art_image(*current->ppu,0,0,1024,1024);
                    std::vector<unsigned char> rgba(art.size()*4U);
                    for(std::size_t i=0;i<art.size();++i) for(unsigned c=0;c<4;++c) rgba[i*4U+c]=c==3?255:static_cast<unsigned char>(art[i]>>(c*8U));
                    write_bmp(stem.string()+"-bg2.bmp",rgba,1024,1024);
                    std::cout<<"bg2 scroll="<<current->ppu->bg2_scroll_x<<','<<current->ppu->bg2_scroll_y<<" vanishing="<<current->source_vanishing_point[1]<<std::endl;
                }
                // STARFOX_VR_ALPHA: the last tick drawn between source ticks, as a
                // headset frame is (default 1, the tick itself).
                const double alpha=last && std::getenv("STARFOX_VR_ALPHA")?std::stod(std::getenv("STARFOX_VR_ALPHA")):1.;
                auto world_packets=models.assemble_world_interpolated(*previous,*current,alpha,srgb,true);
                require(world_packets.pending.empty(),"Model assembly pending");
                if(const char* hide=std::getenv("STARFOX_VR_HIDE")) {
                    std::string list{hide};
                    for(std::size_t at=0;at<list.size();) {
                        const auto comma=std::min(list.find(',',at),list.size());
                        const auto key=uint32_t(std::stoul(list.substr(at,comma-at),nullptr,16));
                        at=comma+1;
                        for(std::size_t i=0;i<world_packets.handles.size();++i) if(world_packets.handles[i]==key) {
                            world_packets.packets.erase(world_packets.packets.begin()+std::ptrdiff_t(i));
                            world_packets.handles.erase(world_packets.handles.begin()+std::ptrdiff_t(i));
                            auto& models=world_packets.compute_models;
                            models.erase(std::remove_if(models.begin(),models.end(),[&](const auto& m){return m.packet_index==i;}),models.end());
                            for(auto& model:models) if(model.packet_index>i) --model.packet_index;
                            break;
                        }
                    }
                }
                if(last && std::getenv("STARFOX_VR_OBJECTS")) {
                    const auto& p=game.objects().at(game.player());
                    std::cout<<"objects bg="<<current->background_id<<" flow="<<int(current->flow)<<" camera-player=("<<int(current->camera.x)-int(p.world_x)<<","<<int(current->camera.y)-int(p.world_y)<<","<<int(std::int16_t(std::uint16_t(current->camera.z-p.world_z)))<<") ground="<<int(current->shadow_height)-int(current->camera.y)<<" mhud="<<std::hex<<current->cockpit_hud_roll<<std::dec<<'\n';
                    {const auto& w=current->wipe;const auto& q=*current->ppu;
                    std::cout<<"wipe active="<<w.active<<" logic="<<int(w.logic)<<" rows 0/40/96/150/191 l-r=";
                    for(int row:{0,40,96,150,191}) std::cout<<w.left[size_t(row)]<<'-'<<w.right[size_t(row)]<<' ';
                    std::cout<<"mode="<<int(q.background_mode)<<" bg3hi="<<q.bg3_high_priority<<" tm="<<int(q.main_screen)<<'\n';}
                    for(const auto handle:game.objects().active_handles()) {
                        const auto& o=game.objects().at(handle);
                        std::cout<<"  handle="<<handle<<" shape="<<symbol_name(o.shape,true)<<" strat="<<symbol_name(o.strategy_address,false)
                            <<" flags="<<unsigned(o.flags)<<" rel=("<<o.world_x-p.world_x<<','<<o.world_y-p.world_y<<','
                            <<int(std::int16_t(std::uint16_t(o.world_z-p.world_z)))<<")\n";
                    }
                    for(std::size_t i=0;i<world_packets.packets.size();++i) {
                        const auto& m=world_packets.packets[i].model;
                        std::cout<<"  packet key="<<std::hex<<world_packets.handles[i]<<std::dec<<" at=("<<m[12]<<','<<m[13]<<','<<m[14]
                            <<") tris="<<world_packets.packets[i].geometry.vertex_view().size()/3<<'\n';
                    }
                    {
                        // The world's texel words, as ScenePacketValidator counts them.
                        std::size_t texels=0,largest=0;std::set<const void*> seen;
                        for(const auto& packet:world_packets.packets) {
                            const auto& mesh=packet.geometry;
                            if(mesh.shared_texels && !seen.insert(mesh.shared_texels.get()).second) continue;
                            texels+=mesh.texel_view().size();largest=std::max(largest,mesh.texel_view().size());
                        }
                        std::cout<<"  world texel words "<<texels<<" (largest packet "<<largest<<")\n";
                    }
                    for(const auto& model:world_packets.compute_models)
                        std::cout<<"  compute key="<<std::hex<<model.key<<std::dec<<" packet="<<model.packet_index<<'\n';
                }
                // The engine glow and the ray tracing, as application.cpp
                // enables them: the glow traced with RAY TRACING, else the decal.
                const bool gameplay=current->flow==simulation::GameFlowState::gameplay
                    || current->flow==simulation::GameFlowState::training;
                const auto sun=rt && gameplay?vr::ray_sun(*previous,*current,1.,75.):std::nullopt;
                const bool traced_lights=rt && rt_lights && gameplay;
                {
                    vr::ModernFrame::EngineGlowSettings glow{rt_lights && gameplay && !traced_lights,{}};
                    if(sun) glow.toward_light=std::array<float,3>{float(sun->x),float(sun->y),float(sun->z)};
                    frame.set_engine_glow(glow);flat.set_engine_glow(glow);
                }
                const bool shadowless=rt && gameplay && emitters.shadowless(*current);
                const auto softness=vr::ray_softness(int(game.shadow_softness()&3U));
                const auto caster=[&](uint32_t key) {return emitters.caster(*current,key);};
                const vr::RayEmitterKind emitter=[&](uint32_t key)->unsigned {return emitters.kind(*current,key);};
                auto* command=SDL_AcquireGPUCommandBuffer(sdl);require(command!=nullptr,"Acquire SDL command buffer");
                // STARFOX_VR_FORCE_MHUD=1: MHUD's wing indicators on (HUDROT bit 15), to
                // check both paths draw them without reaching first person.
                auto shown=*current;
                if(std::getenv("STARFOX_VR_FORCE_MHUD")) shown.cockpit_hud_roll|=0x8000U;
                if(!flat.prepare(command,*previous,shown,alpha,world_packets,inputs)) {
                    SDL_CancelGPUCommandBuffer(command);throw std::runtime_error("SDL prepare: "+flat.status());
                }
                frame.prepare(*previous,shown,alpha,std::move(world_packets),inputs);
                if(last && std::getenv("STARFOX_VR_OBJECTS")) {
                    // Each layer set's upload totals, as ScenePacketValidator counts them.
                    const auto totals=[&](const char* name,std::span<const vr::DrawPacket> packets) {
                        std::size_t vertices=0,texels=0;std::set<const void*> seen;
                        for(const auto& packet:packets) {
                            const auto& mesh=packet.geometry;
                            vertices+=mesh.vertex_view().size()+mesh.line_view().size();
                            if(!mesh.shared_texels || seen.insert(mesh.shared_texels.get()).second) texels+=mesh.texel_view().size();
                        }
                        std::cout<<"  layer "<<name<<": packets "<<packets.size()<<", vertices "<<vertices<<", texel words "<<texels<<'\n';
                    };
                    totals("stars",frame.stars);totals("backgrounds",frame.backgrounds);totals("textures",frame.textures);
                    totals("surround",frame.tunnel_surround);totals("sprites",frame.sprites);
                }
                require(layers.upload(frame),layers.status());
                vr::VulkanModernLayers::apply_fog(fog,frame,.05F);
                // STARFOX_VR_FROM_TICK=N: also draw and write every tick from N
                // (<stem>-tTICK-vr.bmp / -sdl.bmp), to follow a short event.
                static const long from_tick=std::getenv("STARFOX_VR_FROM_TICK")?std::stol(std::getenv("STARFOX_VR_FROM_TICK")):-1;
                const bool each=from_tick>=0 && long(tick)>=from_tick && !last;
                if(!last && !each) {require(SDL_SubmitGPUCommandBuffer(command),"Submit SDL prepare");continue;}

                // SDL: modern_runtime.cpp's present() without the window.
                require(flat.record_filtered_layers(command,camera),flat.status());
                if(rt) {
                    modern::SdlFrameRenderer::ShadowSettings settings;
                    settings.radius=softness.radius;settings.samples=softness.samples;
                    settings.cartridge_shadows=current->shadows_enabled;settings.no_shadows=shadowless;
                    const modern::SdlFrameRenderer::LightInputs lights{emitter,vr::ray_bomb_flash(*current),uint32_t(current->revision)};
                    require(flat.record_shadows(command,camera,sun,caster,settings,traced_lights?&lights:nullptr),"SDL shadows: "+flat.status());
                    std::cout<<"sdl rays: objects "<<flat.ray_shadows().object_count()<<", triangles "<<flat.ray_shadows().triangle_count()
                        <<", lights "<<flat.point_light_count()<<", flow "<<int(current->flow)<<", sun "<<bool(sun)<<'\n';
                }
                const auto clear=vr::frame_clear_colour(*current,srgb);
                SDL_GPUColorTargetInfo target{};target.texture=sdl_target;target.load_op=SDL_GPU_LOADOP_CLEAR;
                target.store_op=SDL_GPU_STOREOP_STORE;target.clear_color={clear[0],clear[1],clear[2],1};
                SDL_GPUDepthStencilTargetInfo depth_target{};depth_target.texture=sdl_depth_target;
                depth_target.clear_depth=modern::SdlFrameRenderer::clear_depth();depth_target.load_op=SDL_GPU_LOADOP_CLEAR;
                depth_target.store_op=SDL_GPU_STOREOP_DONT_CARE;
                depth_target.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE;depth_target.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE;
                auto* pass=SDL_BeginGPURenderPass(command,&target,1,&depth_target);require(pass!=nullptr,"Begin SDL pass");
                const bool recorded=flat.record(command,pass,camera);
                SDL_EndGPURenderPass(pass);
                require(recorded,"SDL record: "+flat.status());
                auto* copy=SDL_BeginGPUCopyPass(command);
                SDL_GPUTextureRegion region{};region.texture=sdl_target;region.w=width;region.h=height;region.d=1;
                SDL_GPUTextureTransferInfo destination{download,0,width,height};
                SDL_DownloadFromGPUTexture(copy,&region,&destination);SDL_EndGPUCopyPass(copy);
                auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence!=nullptr,"Submit SDL frame");
                require(SDL_WaitForGPUFences(sdl,true,&fence,1),"Wait for SDL frame");SDL_ReleaseGPUFence(sdl,fence);
                const auto* pixels=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(sdl,download,false));
                require(pixels!=nullptr,"Map SDL download");
                sdl_pixels.assign(pixels,pixels+std::size_t(width)*height*4);SDL_UnmapGPUTransferBuffer(sdl,download);

                // Vulkan: application.cpp's eye recording for MODERN.
                require(world.initialize(device,vkGetDeviceProcAddr,memory,properties.limits,targets.render_pass(),
                    frame.world,nullptr,frame.moving),world.status());
                auto eye=vr::VulkanModernLayers::eye_camera(camera,extent);
                const unsigned eye_index=std::getenv("STARFOX_VR_EYE") && std::string_view(std::getenv("STARFOX_VR_EYE"))=="1"?1:0;
                eye.effects[3]|=eye_index<<4;
                auto model_camera=eye;model_camera.effects={0,0,0,eye.effects[3]};
                model_camera=vr::VulkanModernLayers::fogged(model_camera,1);
                bool hide_shadows=false;
                if(rt) {
                    const bool receivers=std::any_of(frame.backgrounds.begin(),frame.backgrounds.end(),[&](const auto& packet) {
                        return (current->shadows_enabled && vr::is_landscape_ground(packet)) || vr::is_tunnel_corridor(packet)
                            || vr::is_nucleus_room(packet) || vr::is_nucleus_floor(packet);
                    });
                    vr::VulkanRayShadows::Settings settings;settings.radius=softness.radius;settings.samples=softness.samples;
                    const vr::VulkanRayShadows::LightInputs lights{emitter,vr::ray_bomb_flash(*current),uint32_t(current->revision)};
                    require(rays.prepare(world,frame.world,shadowless?std::nullopt:sun,receivers,caster,settings,traced_lights?&lights:nullptr),
                        "Vulkan rays: "+rays.status());
                    rays.set_eye(eye_index,eye,extent);
                    hide_shadows=rays.sun_active() || shadowless;
                    std::cout<<"vr rays: objects "<<rays.object_count()<<", triangles "<<rays.triangle_count()
                        <<", lights "<<rays.point_light_count()<<", sun "<<rays.sun_active()<<'\n';
                }
                redraw=[&,clear,eye,model_camera,hide_shadows] {vr_pixels=draw_eye(clear,[&](VkCommandBuffer cmd,VkExtent2D size) {
                    vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timer,1);
                    if(!layers.record_stars(cmd,size,eye) || !layers.record_backgrounds(cmd,size,eye)
                        || !world.record(cmd,size,model_camera,hide_shadows) || !layers.record_after_world(cmd,size,eye)
                        || !layers.record_below_hud(cmd,size,eye) || !layers.record_hud(cmd,size,eye))
                        throw std::runtime_error("Vulkan MODERN recording failed: "+layers.status()+" / "+world.status());
                    vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timer,2);
                },[&](VkCommandBuffer cmd,VkExtent2D) {
                    vkCmdResetQueryPool(cmd,timer,0,3);
                    if(!world.record_compute(cmd)) throw std::runtime_error("Vulkan world compute failed: "+world.status());
                    vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timer,0);
                    if(rt && !rays.record_casters(cmd)) throw std::runtime_error("Vulkan ray casters failed");
                    if(!layers.record_filter(cmd,eye)) throw std::runtime_error("Vulkan xBRZ layer failed: "+layers.status());
                });};
                redraw();
                if(each) {
                    const auto name=stem.string()+"-t"+std::to_string(tick);
                    write_bmp(name+"-vr.bmp",vr_pixels,width,height);
                    write_bmp(name+"-sdl.bmp",sdl_pixels,width,height);
                }
            }
            if(const char* repeat=std::getenv("STARFOX_VR_TIME")) {
                double fastest=1e9,fastest_frame=1e9;
                for(unsigned i=0,count=unsigned(std::stoul(repeat));i<count;++i) {
                    redraw();
                    std::array<uint64_t,3> stamps{};
                    check(vkGetQueryPoolResults(device,timer,0,3,sizeof(stamps),stamps.data(),sizeof(uint64_t),
                        VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"Read timestamps");
                    fastest_frame=std::min(fastest_frame,double(stamps[1]-stamps[0])*properties.limits.timestampPeriod/1e6);
                    fastest=std::min(fastest,double(stamps[2]-stamps[1])*properties.limits.timestampPeriod/1e6);
                }
                std::cout<<"vr: casters and xBRZ "<<fastest_frame<<" ms, eye pass "<<fastest<<" ms (fastest of "<<repeat<<")\n";
            }
            vkDeviceWaitIdle(device);
            vkDestroyQueryPool(device,timer,nullptr);
            flat.release();
            SDL_ReleaseGPUTransferBuffer(sdl,download);SDL_ReleaseGPUTexture(sdl,sdl_depth_target);SDL_ReleaseGPUTexture(sdl,sdl_target);
            SDL_DestroyGPUDevice(sdl);SDL_Quit();
            write_bmp(stem.string()+"-vr.bmp",vr_pixels,width,height);
            write_bmp(stem.string()+"-sdl.bmp",sdl_pixels,width,height);
            const auto difference=compare(vr_pixels,sdl_pixels);
            std::cout<<"vr vs sdl: mean channel difference "<<difference.mean<<", maximum "<<difference.maximum
                <<", pixels over 8: "<<difference.over_8<<"%\n";
        }
        vkDeviceWaitIdle(device);
    }
    vr::set_scene_rays(nullptr);rays.close();
    vr::set_scene_fog(nullptr);fog.close();
    vkDestroyBuffer(device,readback,nullptr);vkFreeMemory(device,read_memory,nullptr);
    vkDestroyImage(device,image,nullptr);vkFreeMemory(device,image_memory,nullptr);
    vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);
    return 0;
} catch(const std::exception& error) {
    std::cerr<<"vr modern capture failed: "<<error.what()<<'\n';return 1;
}
