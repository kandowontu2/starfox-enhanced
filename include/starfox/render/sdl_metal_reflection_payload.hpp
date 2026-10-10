#pragma once
#include "starfox/render/embedded_metal_reflection_program.hpp"
#include <SDL3/SDL_gpu.h>
#include <array>
#include <span>
#include <string_view>

namespace starfox::render::embedded_metal {
enum class PayloadSelection { NativeUnchanged, MetalSelected, Refused };
struct Recipe {
    std::string_view name, entry;
    unsigned uniforms, reads, writes, samplers, x, y, z;
};
// The complete original SDL factory recipes, including intentionally reserved
// uniform slots. These are not smaller reflection or compatibility substitutes.
inline constexpr std::array<Recipe,25> reflection_recipes{{
    {"tiles","feature_tiles_main",1,1,1,0,8,8,1},
    {"reduce","feature_reduce_main",1,0,1,0,8,8,1},
    {"query","feature_query_main",1,2,1,0,64,1,1},
    {"mapping","feature_mapping_main",1,2,1,0,64,1,1},
    {"domains","feature_domains_main",2,3,1,0,8,8,1},
    {"frames","feature_frames_main",1,3,1,0,64,1,1},
    {"optical","feature_optical_main",3,3,1,0,64,1,1},
    {"optical_stream","feature_optical_stream_main",3,4,1,0,256,1,1},
    {"optical_schedule","feature_optical_schedule_main",1,1,3,0,128,1,1},
    {"jets","feature_jets_main",3,4,1,0,64,1,1},
    {"jets_stream","feature_jets_stream_main",3,4,1,0,64,1,1},
    {"roots_clear","feature_roots_clear_main",1,0,1,0,64,1,1},
    {"witness","feature_witness_main",3,3,1,0,64,1,1},
    {"folds","feature_folds_main",2,3,1,0,64,1,1},
    {"guide","feature_guide_main",3,2,1,0,64,1,1},
    {"colour","feature_colour_main",2,3,1,0,64,1,1},
    {"compose","feature_compose_main",2,2,1,0,64,1,1},
    {"publish","feature_publish_main",1,1,1,0,64,1,1},
    {"publish_diagnostics","feature_publish_diagnostics_main",1,1,2,0,64,1,1},
    {"admission_clear","feature_admission_clear_main",1,0,1,0,64,1,1},
    {"capture","reflection_path_capture_main",1,1,1,1,8,8,1},
    {"history","reflection_history_main",1,2,1,1,8,8,1},
    {"lobes","reflection_lobes_main",1,3,1,1,8,8,1},
    {"paths","reflection_paths_main",1,3,1,1,8,8,1},
    {"curved_paths","reflection_paths_main",1,3,1,1,8,8,1},
}};

inline bool complete_inventory(std::span<const Program> programs) noexcept {
    if(programs.size()!=reflection_recipes.size()) return false;
    std::array<bool,25> found{};
    for(const auto& program:programs) {
        if(!program.name || !program.entry || !program.bytes || program.size<=92 ||
           program.bytes[0]!='M' || program.bytes[1]!='T' ||
           program.bytes[2]!='L' || program.bytes[3]!='B') return false;
        bool matched=false;
        for(std::size_t n=0;n<reflection_recipes.size();++n) {
            const auto& recipe=reflection_recipes[n];
            if(recipe.name!=program.name) continue;
            if(found[n] || recipe.entry!=program.entry ||
               !matches(program,recipe.uniforms,recipe.reads,recipe.writes,
                        recipe.samplers,recipe.x,recipe.y,recipe.z)) return false;
            matched=found[n]=true;
            break;
        }
        if(!matched) return false;
    }
    return true;
}

// Pure payload selection: no device, pipeline, dispatch, allocation, capability
// claim or history acceptance. The caller still creates and validates the actual
// SDL pipeline. Only format/code/size change, after the ENTIRE bundle qualifies.
// The native DXIL/SPIRV descriptor is otherwise preserved byte for byte.
inline PayloadSelection select_payload(std::span<const Program> programs,
    std::string_view variant, SDL_GPUShaderFormat formats,
    SDL_GPUComputePipelineCreateInfo& info) noexcept {
    if(!(formats&SDL_GPU_SHADERFORMAT_METALLIB))
        return (formats&info.format)?PayloadSelection::NativeUnchanged:PayloadSelection::Refused;
    if(!complete_inventory(programs) || !info.entrypoint ||
       info.num_readonly_storage_textures || info.num_readwrite_storage_textures)
        return PayloadSelection::Refused;
    for(const auto& program:programs) {
        if(variant!=program.name || std::string_view(info.entrypoint)!=program.entry) continue;
        if(!matches(program,info.num_uniform_buffers,info.num_readonly_storage_buffers,
                    info.num_readwrite_storage_buffers,info.num_samplers,
                    info.threadcount_x,info.threadcount_y,info.threadcount_z))
            return PayloadSelection::Refused;
        info.format=SDL_GPU_SHADERFORMAT_METALLIB;
        info.code=program.bytes;
        info.code_size=program.size;
        return PayloadSelection::MetalSelected;
    }
    return PayloadSelection::Refused;
}
}
