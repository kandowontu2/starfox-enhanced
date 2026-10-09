#pragma once
#include "starfox/render/msaa.hpp"
#include <memory>
#include <string>
namespace starfox::render {
struct GpuSpanOrder;
struct GpuMsaaTriangle {
    MsaaPoint a,b,c;
    std::uint32_t color0{},color1{0xffffffffU};
    MsaaPoint uv_a{},uv_b{},uv_c{};
    std::uint32_t texture_offset{},u_mask{},v_mask{},color_base{};
    std::uint32_t scroll_x{},scroll_y{},textured{};
    // Packed block active count; primitive kind (1 = quad stroke, uv_a is
    // fourth corner; 2 = cel polygon horizontal inset); reserved.
    // Ordinary caller-supplied triangles use zero.
    std::array<std::uint32_t,3> padding{};
};
static_assert(sizeof(GpuMsaaTriangle)==96);
struct GpuMsaaFaces {
    void* triangles{};
    void* kinds{}; // uint per input: 0 clipped, 1 polygon, 2 special/invalid.
    unsigned triangle_count{};
    void* texels{};
    unsigned texel_bytes{};
};
struct GpuMsaaSamples {
    void* device{};
    void* buffer{}; // RGBA16F or indexed material tokens; never resolved color.
    unsigned width{},height{},count{};
    bool indexed{};
};
struct GpuMsaaLayer {
    void* pixels{}; // Native packed pixels with explicit bit-26 coverage.
    void* kinds{}; // Optional packed-face classifications; special faces fall back together.
    unsigned face_count{};
    bool resolve_only{};
};
// Polygon coverage/resolve core with flat and affine indexed materials.
// Special primitives must keep their own evaluators, not become flat triangles.
class GpuMsaa {
public:
    GpuMsaa();~GpuMsaa();
    // GpuClip 129-int4 blocks and native 96-byte materials. Optional order is
    // an already validated painter list (not a BSP tree). Borrowed outputs;
    // caller must dispatch kind 2 through the appropriate material evaluator.
    GpuMsaaFaces pack_faces(void* device,void* command,void* clipped,void* materials,
        void* order,unsigned count,unsigned polygon_count,bool fractional,
        std::array<float,2> scale={1,1},const GpuSpanOrder* bsp=nullptr,unsigned line_thickness=1,
        std::array<unsigned,4> repeated_masks={}); // byte prefix, row stride, height, samples
    void* enqueue(void* device,void* command,void* background,void* triangles,void* palette,
        unsigned width,unsigned height,unsigned triangle_count,unsigned samples,
        const GpuMsaaSamples* previous=nullptr,bool retain_samples=false,
        void* texels=nullptr,unsigned texel_bytes=0,bool packed_faces=false,
        const GpuMsaaLayer* layer=nullptr);
    // Enables painter continuation across separate model submissions without
    // resolving one object's edge into another's coverage. Background-based
    // draws alternate instances. Scene layers may continue in place: each GPU
    // invocation exclusively owns its pixel's samples and skips untouched pixels.
    [[nodiscard]] GpuMsaaSamples sample_output() const;
    // Recolor retained scene coverage without rerunning transforms/rasterization.
    void* resolve_scene(void* command,void* palette);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
