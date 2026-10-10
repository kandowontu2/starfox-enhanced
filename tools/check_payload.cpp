#include "starfox/render/sdl_metal_reflection_payload.hpp"
#include "embedding_programs.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <tuple>

using namespace starfox::render::embedded_metal;
static unsigned refusals=0;
static void require(bool value, const char* message) {
    if(!value) {std::fprintf(stderr,"PAYLOAD_FAILURE %s\n",message);std::exit(1);}
}
static auto fields(const SDL_GPUComputePipelineCreateInfo& p) {
    return std::tie(p.code_size,p.code,p.entrypoint,p.format,p.num_samplers,
        p.num_readonly_storage_textures,p.num_readonly_storage_buffers,
        p.num_readwrite_storage_textures,p.num_readwrite_storage_buffers,
        p.num_uniform_buffers,p.threadcount_x,p.threadcount_y,p.threadcount_z,p.props);
}
static SDL_GPUComputePipelineCreateInfo descriptor(const Program& p) {
    SDL_GPUComputePipelineCreateInfo result{};
    result.code_size=4;
    result.code=reinterpret_cast<const unsigned char*>("DXIL");
    result.entrypoint=p.entry;
    result.format=SDL_GPU_SHADERFORMAT_DXIL;
    result.num_uniform_buffers=p.uniforms;
    result.num_readonly_storage_buffers=p.readonly_buffers;
    result.num_readwrite_storage_buffers=p.writable_buffers;
    result.num_samplers=p.samplers;
    result.threadcount_x=p.threads_x;
    result.threadcount_y=p.threads_y;
    result.threadcount_z=p.threads_z;
    result.props=39; // Opaque extension identity must not be changed by selection.
    return result;
}
static void refuse(std::span<const Program> programs, std::string_view variant,
                   SDL_GPUComputePipelineCreateInfo info) {
    const auto saved=info;
    require(select_payload(programs,variant,SDL_GPU_SHADERFORMAT_METALLIB,info)==PayloadSelection::Refused,
            "incorrect descriptor/inventory was accepted");
    require(fields(saved)==fields(info),"refusal changed caller descriptor");
    ++refusals;
}
int main() {
    std::array<Program,25> programs{};
    std::transform(std::begin(embedding_programs),std::end(embedding_programs),programs.begin(),[](const auto& p) {
        return Program{p.name,p.entry,p.bytes,p.size,p.uniforms,p.readonly_buffers,p.writable_buffers,p.samplers,p.x,p.y,p.z};
    });
    require(complete_inventory(programs),"actual entire SDK library family missing");
    for(std::size_t n=0;n<programs.size();++n) {
        const auto& program=programs[n];
        auto original=descriptor(program), selected=original, expected=original;
        expected.format=SDL_GPU_SHADERFORMAT_METALLIB;
        expected.code=program.bytes;
        expected.code_size=program.size;
        require(select_payload(programs,program.name,SDL_GPU_SHADERFORMAT_METALLIB|SDL_GPU_SHADERFORMAT_MSL,selected)==PayloadSelection::MetalSelected,
                "actual original SDK program not selected");
        require(fields(selected)==fields(expected),"selection changed original resources/workgroup/extension");
        require(reinterpret_cast<std::uintptr_t>(selected.code)%16==0,"actual embedded payload alignment lost");
        for(const auto format:{SDL_GPU_SHADERFORMAT_DXIL,SDL_GPU_SHADERFORMAT_SPIRV}) {
            auto native=original;native.format=format;const auto saved=native;
            require(select_payload(programs,program.name,format,native)==PayloadSelection::NativeUnchanged,
                    "native backend was changed by Metal selector");
            require(fields(native)==fields(saved),"native descriptor changed");
        }
        auto wrong=original;wrong.entrypoint="missing_kernel";refuse(programs,program.name,wrong);
        wrong.entrypoint=nullptr;refuse(programs,program.name,wrong);
        refuse(programs,"missing_variant",original);
        for(auto member:{&SDL_GPUComputePipelineCreateInfo::num_uniform_buffers,
                         &SDL_GPUComputePipelineCreateInfo::num_readonly_storage_buffers,
                         &SDL_GPUComputePipelineCreateInfo::num_readwrite_storage_buffers,
                         &SDL_GPUComputePipelineCreateInfo::num_samplers,
                         &SDL_GPUComputePipelineCreateInfo::num_readonly_storage_textures,
                         &SDL_GPUComputePipelineCreateInfo::num_readwrite_storage_textures,
                         &SDL_GPUComputePipelineCreateInfo::threadcount_x,
                         &SDL_GPUComputePipelineCreateInfo::threadcount_y,
                         &SDL_GPUComputePipelineCreateInfo::threadcount_z}) {
            wrong=original;++(wrong.*member);refuse(programs,program.name,wrong);
        }
        for(auto member:{&Program::uniforms,&Program::readonly_buffers,&Program::writable_buffers,&Program::samplers,
                         &Program::threads_x,&Program::threads_y,&Program::threads_z}) {
            auto changed=programs;++(changed[n].*member);refuse(changed,program.name,original);
        }
        auto changed=programs;changed[n].size=92;refuse(changed,program.name,original);
        changed=programs;changed[n].bytes=nullptr;refuse(changed,program.name,original);
        changed=programs;changed[n].name="unknown";refuse(changed,program.name,original);
        changed=programs;changed[n].entry="unknown";refuse(changed,program.name,original);
        changed=programs;changed[n]=programs[(n+1)%programs.size()];refuse(changed,program.name,original);
        refuse(std::span(programs).first(24),program.name,original);
        std::printf("PAYLOAD_PASS %s / %s bytes=%zu\n",program.name,program.entry,program.size);
    }
    const auto paths=std::find_if(programs.begin(),programs.end(),[](const Program& p){return std::string_view(p.name)=="paths";});
    const auto curved=std::find_if(programs.begin(),programs.end(),[](const Program& p){return std::string_view(p.name)=="curved_paths";});
    require(paths!=programs.end() && curved!=programs.end() && std::string_view(paths->entry)==curved->entry,
            "original shared-entry variants missing");
    require(paths->bytes!=curved->bytes && paths->size!=curved->size,"shared-entry variants were conflated");
    std::printf("ALL25_SDL_PAYLOAD_SELECTION_PASS refusals=%u; no GPU dispatch or production acceptance\n",refusals);
}
