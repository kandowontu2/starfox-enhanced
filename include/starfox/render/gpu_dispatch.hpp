#pragma once
#include <cstdint>

namespace starfox::render {
struct LinearGpuDispatch {std::uint32_t x{},y{},row_stride{};};
// Flatten a 64-lane linear pass over legal two-dimensional dispatch groups.
// Balance rows rather than launching a mostly empty second 65535-group row.
constexpr LinearGpuDispatch linear_gpu_dispatch64(std::uint32_t count) noexcept {
    if(!count) return {};
    const auto groups=count/64U+(count%64U!=0);
    const auto rows=groups/65535U+(groups%65535U!=0);
    const auto columns=groups/rows+(groups%rows!=0);
    return {columns,rows,columns*64U};
}
// The minimal span/mask clearer has 128 lanes, with the same balanced-row
// contract. Pass row_stride to the shader; a fixed 65535-group stride leaves
// holes when the host balances a dispatch over more than one row.
constexpr LinearGpuDispatch linear_gpu_dispatch128(std::uint32_t count) noexcept {
    if(!count) return {};
    const auto groups=count/128U+(count%128U!=0);
    const auto rows=groups/65535U+(groups%65535U!=0);
    const auto columns=groups/rows+(groups%rows!=0);
    return {columns,rows,columns*128U};
}
}
