#include "starfox/render/sdl_metal_exact_workgroup.hpp"
#include <SDL3/SDL.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <tuple>

namespace {
unsigned cleanup_count=0;
void require(bool value,const char* message) {
    if(!value) { std::fprintf(stderr,"METADATA_FAILURE %s\n",message); std::exit(1); }
}
void SDLCALL cleanup(void*,void*) { ++cleanup_count; }
auto fields(const SDL_GPUComputePipelineCreateInfo& p) {
    return std::tie(p.code_size,p.code,p.entrypoint,p.format,p.num_samplers,
        p.num_readonly_storage_textures,p.num_readonly_storage_buffers,
        p.num_readwrite_storage_textures,p.num_readwrite_storage_buffers,
        p.num_uniform_buffers,p.threadcount_x,p.threadcount_y,p.threadcount_z);
}
}
int main() {
    using starfox::render::metal_exact_workgroup_property;
    using starfox::render::with_metal_exact_workgroup;
    SDL_GPUComputePipelineCreateInfo original{};
    original.code_size=4;
    original.code=reinterpret_cast<const unsigned char*>("MTLB");
    original.entrypoint="held_kernel";
    original.format=SDL_GPU_SHADERFORMAT_METALLIB;
    original.num_samplers=1;original.num_uniform_buffers=3;
    original.num_readonly_storage_textures=2;original.num_readonly_storage_buffers=4;
    original.num_readwrite_storage_textures=5;original.num_readwrite_storage_buffers=6;
    original.threadcount_x=8;original.threadcount_y=8;original.threadcount_z=1;
    const auto props=SDL_CreateProperties();
    require(props!=0,"real SDL properties allocation");
    int borrowed=7;
    require(SDL_SetPointerPropertyWithCleanup(props,"borrowed",&borrowed,cleanup,nullptr),"borrowed pointer");
    require(SDL_SetStringProperty(props,"label","caller label"),"string property");
    require(SDL_SetNumberProperty(props,"number",0x123456789abcLL),"number property");
    require(SDL_SetFloatProperty(props,"float",.125F),"float property");
    require(SDL_SetBooleanProperty(props,"boolean",true),"boolean property");
    require(SDL_SetBooleanProperty(props,metal_exact_workgroup_property,false),"original private flag");
    original.props=props;
    const auto held=original;
    SDL_PropertiesID temporary=0;
    unsigned calls=0;
    auto inspect=[&](const SDL_GPUComputePipelineCreateInfo* p)->SDL_GPUComputePipeline* {
        ++calls;temporary=p->props;
        require(fields(*p)==fields(held),"shader/resources/workgroup fields changed");
        require(p->props!=0 && p->props!=props,"temporary properties isolation");
        require(SDL_GetBooleanProperty(p->props,metal_exact_workgroup_property,false),"exact-group hint missing");
        require(SDL_GetPointerProperty(p->props,"borrowed",nullptr)==&borrowed,"cleanup pointer identity lost");
        require(std::strcmp(SDL_GetStringProperty(p->props,"label",""),"caller label")==0,"string changed");
        require(SDL_GetNumberProperty(p->props,"number",0)==0x123456789abcLL,"number changed");
        require(SDL_GetFloatProperty(p->props,"float",0)==.125F,"float changed");
        require(SDL_GetBooleanProperty(p->props,"boolean",false),"boolean changed");
        require(cleanup_count==0,"original ownership retired early");
        // Sentinel only: this test never creates a GPU or a pipeline.
        return reinterpret_cast<SDL_GPUComputePipeline*>(std::uintptr_t(1));
    };
    require(with_metal_exact_workgroup(&original,inspect)!=nullptr && calls==1,"joined callback");
    require(fields(original)==fields(held) && original.props==props,"caller descriptor changed");
    require(!SDL_GetBooleanProperty(props,metal_exact_workgroup_property,true),"caller flag changed");
    require(cleanup_count==0,"borrowed cleanup duplicated");
    require(!SDL_HasProperty(temporary,metal_exact_workgroup_property),"temporary properties retained");
    require(with_metal_exact_workgroup(&original,[](const auto*)->SDL_GPUComputePipeline* {
        SDL_SetError("held compiler failure");return nullptr;
    })==nullptr,"failed creator accepted");
    require(std::strcmp(SDL_GetError(),"held compiler failure")==0,"actual creator error lost");
    bool threw=false;
    try {with_metal_exact_workgroup(&original,[](const auto*)->SDL_GPUComputePipeline* {
        throw std::runtime_error("held exception");
    });} catch(const std::runtime_error&) {threw=true;}
    require(threw && cleanup_count==0,"throw path borrowed ownership");
    original.props=0;
    require(with_metal_exact_workgroup(&original,[](const auto* p)->SDL_GPUComputePipeline* {
        require(SDL_GetBooleanProperty(p->props,metal_exact_workgroup_property,false),"empty metadata hint");
        return reinterpret_cast<SDL_GPUComputePipeline*>(std::uintptr_t(1));
    })!=nullptr,"empty properties supported");
    require(with_metal_exact_workgroup(nullptr,inspect)==nullptr && calls==1,"null descriptor invoked creator");
    original.props=temporary;
    require(with_metal_exact_workgroup(&original,inspect)==nullptr && calls==1,"retired metadata invoked creator");
    SDL_DestroyProperties(props);
    require(cleanup_count==1,"original cleanup must occur exactly once");
    std::printf("METAL_EXACT_WORKGROUP_METADATA_PASS; real SDL properties; no GPU or pipeline created\n");
}
