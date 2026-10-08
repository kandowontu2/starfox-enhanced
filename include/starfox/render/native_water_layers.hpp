#pragma once
#include <cstdint>
#include <limits>
#include <optional>
namespace starfox::render::shadows {
// Structure-of-arrays in one retained native/SDL allocation. The first RGBA
// plane keeps the legacy row ABI; optional layers have no independent fence or
// lifetime. Surface = camera-space normal XYZ, positive forward depth W.
// Surface-only consumers omit the hidden-world RGBA plane (world_offset=0).
// storage_bytes describes this canonical prefix, even if a single allocation
// appends NativeReflectionHistory. That descriptor owns the full allocation;
// liquid consumers continue reading only these unchanged prefix offsets.
struct NativeWaterLayers {
    std::uint32_t world_offset{},surface_offset{},storage_bytes{};
    bool operator==(const NativeWaterLayers&) const=default;
};
inline std::optional<NativeWaterLayers> native_water_layers(unsigned width,unsigned height,bool world=true) noexcept {
    const auto count=std::uint64_t(width)*height;
    const unsigned stride=world?24:20;
    if(!count || width>16384 || height>16384 || count>std::numeric_limits<std::uint32_t>::max()/stride)
        return std::nullopt;
    return NativeWaterLayers{world?std::uint32_t(count*4):0,std::uint32_t(count*(world?8:4)),std::uint32_t(count*stride)};
}
} // namespace starfox::render::shadows
