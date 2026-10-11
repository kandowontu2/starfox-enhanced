#pragma once
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_error.h>
#include <utility>

namespace starfox::render {
inline constexpr const char* metal_exact_workgroup_property =
    "starfox.gpu.compute.exact_threadgroup.v1";

namespace metal_workgroup_detail {
struct PropertyClone {
    SDL_PropertiesID id{SDL_CreateProperties()};
    bool complete{true};
    PropertyClone() = default;
    PropertyClone(const PropertyClone&) = delete;
    PropertyClone& operator=(const PropertyClone&) = delete;
    ~PropertyClone() { if(id) SDL_DestroyProperties(id); }
};

inline void SDLCALL copy_property(void* userdata, SDL_PropertiesID original,
                                 const char* name) {
    auto& clone=*static_cast<PropertyClone*>(userdata);
    if(!clone.complete) return;
    // SDL_CopyProperties skips properties with cleanup callbacks entirely.
    // Keep all caller values, including borrowed pointer identities, but NEVER
    // copy their ownership/cleanup. The caller keeps its original group alive
    // for this joined creation call, exactly as without this descriptor copy.
    switch(SDL_GetPropertyType(original,name)) {
    case SDL_PROPERTY_TYPE_POINTER:
        clone.complete=SDL_SetPointerProperty(clone.id,name,
            SDL_GetPointerProperty(original,name,nullptr)); break;
    case SDL_PROPERTY_TYPE_STRING:
        clone.complete=SDL_SetStringProperty(clone.id,name,
            SDL_GetStringProperty(original,name,nullptr)); break;
    case SDL_PROPERTY_TYPE_NUMBER:
        clone.complete=SDL_SetNumberProperty(clone.id,name,
            SDL_GetNumberProperty(original,name,0)); break;
    case SDL_PROPERTY_TYPE_FLOAT:
        clone.complete=SDL_SetFloatProperty(clone.id,name,
            SDL_GetFloatProperty(original,name,0)); break;
    case SDL_PROPERTY_TYPE_BOOLEAN:
        clone.complete=SDL_SetBooleanProperty(clone.id,name,
            SDL_GetBooleanProperty(original,name,false)); break;
    default:
        clone.complete=false;
        SDL_SetError("Unknown compute pipeline metadata type"); break;
    }
}
} // namespace metal_workgroup_detail

// This only decorates a descriptor for the privately patched SDL constructor.
// It does not select/alter shader bytes, resources, workgroups, or dispatch.
// The supplied creation callback is joined before temporary properties retire.
template<class Create>
SDL_GPUComputePipeline* with_metal_exact_workgroup(
    const SDL_GPUComputePipelineCreateInfo* original, Create&& create) {
    if(!original) { SDL_SetError("Missing compute pipeline descriptor"); return nullptr; }
    metal_workgroup_detail::PropertyClone properties;
    if(!properties.id) return nullptr;
    if(original->props &&
       (!SDL_EnumerateProperties(original->props,metal_workgroup_detail::copy_property,&properties)
        || !properties.complete)) return nullptr;
    if(!SDL_SetBooleanProperty(properties.id,metal_exact_workgroup_property,true)) return nullptr;
    auto candidate=*original;
    candidate.props=properties.id;
    return std::forward<Create>(create)(&candidate);
}
} // namespace starfox::render
