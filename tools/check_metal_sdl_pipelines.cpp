#include "starfox/render/sdl_metal_reflection_payload.hpp"
#include "starfox/render/embedded_metal_reflection_programs.hpp"
#include "starfox/render/gpu_calibrated_reflection_history.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <tuple>

#if !defined(STARFOX_REFLECTION_METAL_EXACT_WORKGROUP)
#error The actual patched SDL exact-workgroup candidate is required.
#endif

using namespace starfox::render;
using namespace starfox::render::embedded_metal;

namespace {
auto fields(const SDL_GPUComputePipelineCreateInfo& p) {
    return std::tie(p.code_size,p.code,p.entrypoint,p.format,p.num_samplers,
        p.num_readonly_storage_textures,p.num_readonly_storage_buffers,
        p.num_readwrite_storage_textures,p.num_readwrite_storage_buffers,
        p.num_uniform_buffers,p.threadcount_x,p.threadcount_y,p.threadcount_z,p.props);
}
struct Resources {
    SDL_GPUDevice* device{};
    SDL_PropertiesID props{};
    std::array<SDL_GPUComputePipeline*,25> pipelines{};
    ~Resources() {
        for(auto* pipeline:pipelines) if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(props) SDL_DestroyProperties(props);
        if(device) SDL_DestroyGPUDevice(device);
        SDL_Quit();
    }
};
}

int main(int argc,char** argv) {
    // A source-only build never executes this. Runtime admission belongs to the
    // original native identity/resource observer after the old driver retires.
    if(argc!=2 || std::string_view(argv[1])!="--run-gpu") {
        std::fprintf(stderr,"Explicit --run-gpu and external retirement/resource admission required\n");
        return 64;
    }
    Resources resources;
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr,"SDL_VIDEO_UNAVAILABLE: %s\n",SDL_GetError());return 77;
    }
    resources.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_METALLIB,true,"metal");
    if(!resources.device) {
        std::fprintf(stderr,"SDL_METAL_DEVICE_UNAVAILABLE: %s\n",SDL_GetError());return 77;
    }
    const char* driver=SDL_GetGPUDeviceDriver(resources.device);
    const auto formats=SDL_GetGPUShaderFormats(resources.device);
    if(!driver || std::strcmp(driver,"metal")!=0 || !(formats&SDL_GPU_SHADERFORMAT_METALLIB)) {
        std::fprintf(stderr,"ACTUAL_METAL_DRIVER_REQUIRED\n");return 1;
    }
    const auto programs=programs_for_sdk();
    if(programs.size()!=resources.pipelines.size()) {
        std::fprintf(stderr,"COMPLETE25_SDK_PROGRAMS_REQUIRED\n");return 1;
    }
    resources.props=SDL_CreateProperties();
    if(!resources.props || !SDL_SetStringProperty(resources.props,"starfox.probe.phase","full25-pipeline-only")
        || !SDL_SetBooleanProperty(resources.props,metal_exact_workgroup_property,false)) {
        std::fprintf(stderr,"CALLER_METADATA_ALLOCATION_FAILED: %s\n",SDL_GetError());return 1;
    }
    unsigned attempted=0,selected=0,created=0,unchanged=0;
    for(std::size_t i=0;i<programs.size();++i) {
        const auto& program=programs[i];
        ++attempted;
        std::printf("SDL_PIPELINE_BEGIN %s / %s threads=%ux%ux%u\n",program.name,program.entry,
            program.threads_x,program.threads_y,program.threads_z);
        std::fflush(stdout);
        SDL_GPUComputePipelineCreateInfo info{};
        info.entrypoint=program.entry;info.format=SDL_GPU_SHADERFORMAT_DXIL;
        info.num_uniform_buffers=program.uniforms;
        info.num_readonly_storage_buffers=program.readonly_buffers;
        info.num_readwrite_storage_buffers=program.writable_buffers;
        info.num_samplers=program.samplers;
        info.threadcount_x=program.threads_x;info.threadcount_y=program.threads_y;info.threadcount_z=program.threads_z;
        info.props=resources.props;
        if(select_payload(programs,program.name,formats,info)!=PayloadSelection::MetalSelected) {
            std::fprintf(stderr,"SDL_PAYLOAD_REFUSED %s\n",program.name);continue;
        }
        ++selected;
        const auto saved=info;
        SDL_ClearError();
        // The actual production helper decorates metadata, joins preparation
        // and returns the pinned SDL constructor's real resource/error.
        resources.pipelines[i]=create_gpu_compute_pipeline(resources.device,&info);
        if(fields(info)!=fields(saved)
            || SDL_GetBooleanProperty(resources.props,metal_exact_workgroup_property,true)
            || std::strcmp(SDL_GetStringProperty(resources.props,"starfox.probe.phase","missing"),
                           "full25-pipeline-only")!=0) {
            std::fprintf(stderr,"SDL_CALLER_MUTATED %s\n",program.name);continue;
        }
        ++unchanged;
        if(!resources.pipelines[i]) {
            std::fprintf(stderr,"SDL_PIPELINE_FAILED %s: %s\n",program.name,SDL_GetError());continue;
        }
        ++created;
        std::printf("SDL_PIPELINE_PASS %s / %s threads=%ux%ux%u\n",program.name,program.entry,
            program.threads_x,program.threads_y,program.threads_z);
        std::fflush(stdout);
    }
    std::printf("SDL_PIPELINE_SUMMARY attempted=%u selected=%u created=%u unchanged=%u failed=%u\n",
        attempted,selected,created,unchanged,attempted-created);
    std::fflush(stdout);
    if(attempted!=25 || selected!=25 || created!=25 || unchanged!=25) return 1;
    {
        // Connect one genuine factory-owner initialization after the complete
        // native constructor sweep. This is not all lazy dispatches or a game.
        GpuCalibratedReflectionHistory owner;
        if(!owner.initialize(resources.device)) {
            std::fprintf(stderr,"ACTUAL_HISTORY_OWNER_FAILED: %s\n",owner.status().c_str());return 1;
        }
        owner.release_device();
        std::printf("ACTUAL_HISTORY_OWNER_INITIALIZATION_PASS\n");
    }
    std::printf("ALL25_ACTUAL_SDL_METAL_PIPELINES_PASS; no dispatch/numerical/frame-cost/game acceptance\n");
    return 0;
}
