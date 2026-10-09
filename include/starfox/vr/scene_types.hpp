#pragma once
#include <cstdint>
namespace starfox::vr {
// Shared native scene ABI. These are graphics-independent data: desktop
// calibrated displays must not need a Vulkan device just to prepare a packet.
struct SceneVertex {
    bool operator==(const SceneVertex&) const = default;
    float position[3];float color[4];
    float odd_color[4]{};
    std::uint32_t dither_scale{};
    float visibility_a[3]{},visibility_b[3]{},visibility_c[3]{};
    // 0 disabled, 1 visibility; 2 destruction uses these as source rotation
    // rows and group_a/b/c as translation, normal and phase/units/Q15.
    std::uint32_t visibility_enabled{};
    float group_a[3]{},group_b[3]{},group_c[3]{};
    std::uint32_t group_enabled{};
    float uv[2]{};
    // Offset, U/V masks and flags. Includes ordinary RGBA textures, source
    // tile/font/span payloads, photographs and GPU shadow coverage. Each
    // renderer must validate the payload for the flags it actually supports.
    std::uint32_t texture[4]{};
    // Eye-facing model-space offset (flag 4). Source span packets instead
    // carry projection depth and the source-to-world unit scale here.
    float billboard[2]{};
};
static_assert(sizeof(SceneVertex)==160);
enum class SceneTopology {triangles,lines};
enum class SceneBlend {opaque,add,subtract,half_add,half_subtract,shadow,alpha};
inline constexpr std::uint32_t gpu_connected_grid_flag=512U|4194304U;
// Graphics-independent ABI shared by the headset and calibrated-display
// compute producers: row headers, primitive records, row lists, projections.
inline constexpr std::uint32_t connected_grid_output_words=384+225*15+192*675+225*4;
}
