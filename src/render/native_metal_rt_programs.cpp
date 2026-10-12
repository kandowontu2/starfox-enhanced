#include "starfox/render/native_metal_rt_programs.hpp"
#include <cstddef>
#include <cstring>

extern "C" {
extern const std::uint8_t starfox_native_metal_shadow[];
extern const std::uint64_t starfox_native_metal_shadow_size;
extern const std::uint8_t starfox_native_metal_reflection[];
extern const std::uint64_t starfox_native_metal_reflection_size;
extern const std::uint8_t starfox_native_metal_empty_shadow[];
extern const std::uint64_t starfox_native_metal_empty_shadow_size;
extern const std::uint8_t starfox_native_metal_empty_reflection[];
extern const std::uint64_t starfox_native_metal_empty_reflection_size;
}
namespace starfox::render::shadows {
namespace {
std::span<const std::uint8_t> checked(const std::uint8_t* data,std::uint64_t size) noexcept {
    if(size<20 || size>(1U<<29) || std::memcmp(data,"MTLB",4)!=0) return {};
    return {data,static_cast<std::size_t>(size)};
}
}
std::span<const std::uint8_t> native_metal_rt_program(NativeMetalRtProgram program) noexcept {
    switch(program) {
    case NativeMetalRtProgram::shadow:return checked(starfox_native_metal_shadow,starfox_native_metal_shadow_size);
    case NativeMetalRtProgram::reflection:return checked(starfox_native_metal_reflection,starfox_native_metal_reflection_size);
    case NativeMetalRtProgram::empty_shadow:return checked(starfox_native_metal_empty_shadow,starfox_native_metal_empty_shadow_size);
    case NativeMetalRtProgram::empty_reflection:return checked(starfox_native_metal_empty_reflection,starfox_native_metal_empty_reflection_size);
    }
    return {};
}
}
