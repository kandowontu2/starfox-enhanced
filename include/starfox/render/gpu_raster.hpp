#pragma once
#include "starfox/render/software_renderer.hpp"
#include <memory>
#include <string>
namespace starfox::render {
// Optional GPU-only diagnostic handles for the latest occupied-tile enqueue.
// Borrowed until the next enqueue (including a failed one) or release. A caller
// must order/fence any readback itself; production does not read these on CPU.
struct GpuRasterDispatchInfo {
    void* device{};
    void* arguments{}; // Four uint32s: group counts x/y/z, zero padding.
    void* tiles{}; // uint32 count, then unique 64-pixel row-tile IDs.
    std::uint32_t tile_capacity{};
    bool row_tile_fused{}; // Last enqueue used the single-pass shared row painter.
    bool pixel_only{}; // No receiver/depth outputs requested; same colour painter.
    bool single_face_direct{}; // Exact single-face row loop, without a tile-list pass.
    bool mask_tile{}; // Ordered two-word coverage masks for <=64 row-span faces.
};
// Bit 31 carries an authored flat-face dither pair. Byte 1 then stores the
// alternate palette index, not a layer tag. The selected index stays in byte 0
// so original-resolution indexed readback is unchanged.
inline constexpr std::uint8_t gpu_pixel_layer(std::uint32_t value) noexcept {
    return (value&0x80000000U)?0:std::uint8_t(value>>8);
}
// Highest render scale the GPU scene path accepts (GPU FAST 10x).
inline constexpr std::uint32_t max_gpu_render_scale = 10U;
void replay_raster_commands(const RasterCommands&,Framebuffer&,SurfaceBuffer*,bool clear_target=true);
// Borrowed SDL GPU buffers, valid until the next render or device release.
// pixels packs index/tag/surface-palette/valid into four bytes; surfaces is
// an optional float4 normal/depth buffer. Consumers must use the same device.
// Bit 27 denotes explicitly classified terrain in a background draw; it must
// follow visible ownership, never be inherited through model/HUD overwrites.
// Bit 28 marks a world sprite that retains its 2D styling tag but must not be
// excluded from temporal reconstruction or restored as screen-space HUD.
struct GpuRasterOutput {
    void* device{};
    void* pixels{};
    void* surfaces{};
    std::uint32_t width{},height{};
    std::uint64_t generation{};
    // Optional positive camera-space Z per visible pixel, zero where unknown.
    // Separate from the historical face-mean effects depth in surfaces.w.
    void* geometry_depth{};
    // Optional float4 previous-current XY pixels, camera Z, validity. Follows
    // visible colour ownership; unknown motion is not valid zero motion.
    void* motion{};
    // Optional premultiplied RGBA multisample coverage, same extent/device.
    // Indexed data remains authoritative for metadata and original readback.
    void* msaa_color{};
    // Fully initialized row-span painter result: unused packed bits are clear
    // and invalid receiver normals have the canonical {0,0,1,0} value. Only
    // this representation may be preserved by sparse in-place row-span draws.
    bool row_span_canonical{};
};
struct GpuGeometryDepthInput {
    void* planes{}; // float4 camera-space plane (normal.xyz, dot(normal, point)).
    std::uint32_t count{};
    float focal_x{},focal_y{},center_x{},center_y{};
    bool screen_aligned{}; // Constant-Z sprite planes tolerate sample remapping.
};
class GpuRaster {
public:
    GpuRaster();
    ~GpuRaster();
    bool render(RasterCommands&,Framebuffer&,SurfaceBuffer*);
    // Binning may switch per frame; the compute pipeline is created lazily.
    bool render_resident(void* device,RasterCommands&,bool surface_metadata,bool gpu_binning=false);
    // Consumes GpuClip's row-span layout directly.
    // A zero polygon count accepts null spans and produces cleared output,
    // including empty coverage/surface ownership, without uploading commands.
    // Texels must be a valid storage buffer for textured commands;
    // null is only valid for solid or empty input.
    // No upload,
    // submission, fence or readback. Caller owns command lifetime and must
    // cancel it on failure. Return buffers are borrowed until next operation.
    // Does not populate resident_output()/wait_for_completion(); caller submits.
    // Optional background fuses painter composition into rasterization. It must
    // match dimensions/device and must not alias this raster's output buffers,
    // except for explicit in_place_background. That opt-in consumes/mutates the
    // canonical background on the ordered command and returns the SAME buffers;
    // callers must not retain an immutable view of its previous contents. Missing
    // metadata storage or a noncanonical background uses the normal copy path.
    // Background surface ownership survives non-surface foreground writes.
    // wave_rows checks the inverse-wave source row plus the unshifted row.
    // All wave spans in this call must share wave_offset/wave_frame;
    // ordinary/textured spans still use their destination row.
    // texel_bytes is the bound buffer size for repeated-row mask bounds checks;
    // zero rejects mask reads. Ordinary texture commands retain their UV rules.
    // painter_flags reproduces GpuScene's foreground policy in this dispatch:
    // bit 0 marks covered 2D-tagged sprites as world geometry; bit 1 clears
    // covered receiver metadata for emissive geometry. No other bits allowed.
    GpuRasterOutput enqueue_row_spans(void* device,void* command,void* spans,
        std::uint32_t polygon_count,std::uint32_t width,std::uint32_t height,bool surface_metadata=false,void* texels=nullptr,bool pixel_coverage=false,
        const GpuRasterOutput* background=nullptr,bool wave_rows=false,
        std::int16_t wave_offset=0,std::uint32_t wave_frame=0,std::uint32_t texel_bytes=0,
        const GpuGeometryDepthInput* geometry_depth=nullptr,std::array<std::uint32_t,2> raster_size={},std::array<float,2> raster_jitter={},std::uint32_t painter_flags=0,bool in_place_background=false,
        bool bounded_in_place=false,bool compact_tiles=false);
    // Upload legacy raster commands onto a caller-owned command buffer, for
    // ordered interleaving with GpuModel/GpuScene. No submit/readback/wait.
    // Result has explicit write coverage; consume before the next operation.
    // Caller cancels the command on failure. Do not mix with pending submits.
    // prepared_row_bins skips CPU bin rebuilding: the caller must have called
    // bin_rows() and keep every source array immutable through this call.
    // GPU binning does not mutate source row tables and ignores that flag.
    GpuRasterOutput enqueue_commands(void* device,void* command,RasterCommands&,
        bool surface_metadata=false,bool gpu_binning=false,std::array<std::uint32_t,2> raster_size={},std::array<float,2> raster_jitter={},bool prepared_row_bins=false);
    // With pixel_coverage=true, packed bit 26 marks every geometry write,
    // including palette index zero. Used when merging separate model layers.
    [[nodiscard]] GpuRasterOutput resident_output() const;
    // Whether a working result uses this raster's cached output storage. Scene
    // owners must avoid overwriting a lent sparse backing with an unfused draw
    // before the painter merge has consumed it. This does not wait or submit.
    [[nodiscard]] bool output_aliases(const GpuRasterOutput&) const noexcept;
    [[nodiscard]] GpuRasterDispatchInfo dispatch_info() const noexcept;
    bool readback(Framebuffer&,SurfaceBuffer*);
    // Fence-only diagnostic synchronization; retains resident data, no readback.
    bool wait_for_completion();
    // Required before destroying a borrowed renderer/device.
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
