#pragma once
#include <array>
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/scene_camera.hpp"
#include <SDL3/SDL_gpu.h>
#include <map>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <vector>
namespace starfox::modern {
// Collects buffer writes for one frame and flushes them through a single
// persistent (cycled) transfer buffer and one copy pass. Per-write transfer
// buffers are committed resources on D3D12 and cost milliseconds per frame.
class SdlUploadBatch {
public:
    SdlUploadBatch()=default;
    ~SdlUploadBatch();
    SdlUploadBatch(const SdlUploadBatch&)=delete;
    SdlUploadBatch& operator=(const SdlUploadBatch&)=delete;
    // Bytes are copied immediately; the source may change afterwards.
    // `cycle` rewrites a whole buffer that earlier frames may still read:
    // SDL hands back a fresh backing store instead of stalling.
    void add(SDL_GPUBuffer*,uint32_t offset,const void* bytes,uint32_t size,bool cycle=false);
    // One copy pass on `command` (no pass may be open). No-op when empty.
    bool flush(SDL_GPUDevice*,SDL_GPUCommandBuffer*);
    void clear() noexcept {writes_.clear();staging_.clear();}
    std::size_t bytes() const noexcept {return staging_.size();}
    void release() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    struct Write {SDL_GPUBuffer* buffer{};uint32_t offset{},size{},staging{};bool cycle{};};
    std::vector<Write> writes_;
    std::vector<unsigned char> staging_;
    SDL_GPUDevice* device_{};SDL_GPUTransferBuffer* transfer_{};uint32_t capacity_{};
    std::string status_;
};
// SDL_GPU equivalent of vr::SceneTopology/SceneBlend (vulkan_scene_pipeline.hpp).
enum class SceneTopology {triangles,lines};
enum class SceneBlend {opaque,add,subtract,half_add,half_subtract,alpha};
// Graphics pipelines for the shared scene shader, created lazily per state and
// retained for one device and target format pair. Release before the device.
class SdlScenePipelines {
public:
    SdlScenePipelines()=default;
    ~SdlScenePipelines();
    SdlScenePipelines(const SdlScenePipelines&)=delete;
    SdlScenePipelines& operator=(const SdlScenePipelines&)=delete;
    // reversed_depth: clear depth to 0 and pass GREATER (reversed infinite
    // projection, see scene.hlsl reversed_depth); otherwise clear 1, LESS.
    bool initialize(SDL_GPUDevice*,SDL_GPUTextureFormat colour,SDL_GPUTextureFormat depth,bool reversed_depth=false);
    SDL_GPUGraphicsPipeline* get(SceneTopology,SceneBlend,bool depth_test,bool depth_write);
    // Source lines are one source pixel thick (the desktop paints each line
    // point as a scale x scale square). Line draws repeat as instances, each
    // shifted one target pixel along X or Y; 1 pixel or less draws hairlines.
    void set_line_brush(unsigned pixels,unsigned target_width,unsigned target_height) noexcept;
    uint32_t line_instances() const noexcept {return brush_pixels_>1?2*brush_pixels_-1:1;}
    // Vertex uniform slot 1 for the next draws: the brush for line topology,
    // disabled otherwise. Push with every draw's camera constants.
    void push_line_brush(SDL_GPUCommandBuffer*,SceneTopology) const noexcept;
    // Painter mode for the next get() calls: depth-tested pipelines pass
    // every fragment (compare ALWAYS) and still write depth, so later draws
    // cover earlier ones exactly as the cartridge's painter does.
    void set_painter(bool enabled) noexcept {painter_=enabled;}
    // Target sample count (MSAA); a change drops the cached pipelines.
    void set_sample_count(SDL_GPUSampleCount samples) noexcept;
    // Fragment storage slots 1-2 (scene_sdl.hlsl): the ray-traced shadow
    // casters' triangles and objects for the next passes, or null for a small
    // placeholder (no shadow receivers). Draws bind all three slots through
    // bind_fragment_storage(), slot 0 being the draw's own texels.
    void set_shadow_buffers(SDL_GPUBuffer* triangles,SDL_GPUBuffer* objects) noexcept {
        shadow_triangles_=triangles;shadow_objects_=objects;
    }
    void bind_fragment_storage(SDL_GPURenderPass*,SDL_GPUBuffer* texels) const noexcept;
    void release() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    SDL_GPUDevice* device_{};
    SDL_GPUShader* vertex_{};SDL_GPUShader* fragment_{};
    SDL_GPUBuffer* placeholder_{};SDL_GPUBuffer* shadow_triangles_{};SDL_GPUBuffer* shadow_objects_{};
    SDL_GPUTextureFormat colour_{},depth_{};
    bool reversed_depth_{},painter_{};
    SDL_GPUSampleCount samples_{SDL_GPU_SAMPLECOUNT_1};
    unsigned brush_pixels_{1};
    std::array<float,4> brush_{};
    std::map<unsigned,SDL_GPUGraphicsPipeline*> pipelines_;
    std::string status_{"Scene pipelines not initialized"};
};
// SDL_GPU equivalent of vr::VulkanDrawPackets: immutable per-packet geometry
// and texel buffers with replaceable model transforms. Keyed packets reuse
// unchanged geometry across frames, exactly as the Vulkan path does.
class SdlDrawPackets {
public:
    SdlDrawPackets();
    ~SdlDrawPackets();
    SdlDrawPackets(const SdlDrawPackets&)=delete;
    SdlDrawPackets& operator=(const SdlDrawPackets&)=delete;
    // Queues new geometry on `uploads`; flush it on the command buffer before
    // the render pass that draws these packets.
    // `blend` replaces the per-packet choice (opaque, or alpha for backdrop
    // photographs) for every triangle draw, e.g. the circle wipe's add modes.
    bool initialize(SDL_GPUDevice*,SdlUploadBatch& uploads,std::span<const vr::DrawPacket>,
        std::span<const uint32_t> object_keys={},bool depth_test=true,bool depth_write=true,
        std::optional<SceneBlend> blend={});
    // Replace nonempty packets' model matrices in submission order (VR's
    // VulkanDrawPackets::update_models). Geometry stays resident.
    bool update_models(std::span<const vr::Matrix4>) noexcept;
    // Inside a render pass whose targets match `pipelines`' formats.
    bool record(SDL_GPUCommandBuffer*,SDL_GPURenderPass*,SdlScenePipelines&,const vr::EyeCamera&) const;
    // Contiguous range of uploaded (nonempty) items, so resident compute
    // models can interleave without changing painter/pass order.
    bool record_range(SDL_GPUCommandBuffer*,SDL_GPURenderPass*,SdlScenePipelines&,const vr::EyeCamera&,
        std::size_t first,std::size_t count) const;
    std::size_t size() const noexcept;
    std::size_t reused_packets() const noexcept;
    std::size_t rejected_packets() const noexcept;
    void release() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    struct State;
    std::unique_ptr<State> state_;
    std::string status_{"Draw packets not initialized"};
};
// Camera constants for the SDL scene shader: vr::scene_constants with the
// projection's Y negated, since SDL_GPU uses a Y-up clip space everywhere.
vr::SceneConstants sdl_scene_constants(const vr::EyeCamera&) noexcept;
}
