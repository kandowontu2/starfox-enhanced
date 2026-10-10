#pragma once

// Per-frame work counters for the GPU scene path, reported under
// STARFOX_TRACE_SCENE_COST. Counting is gated on one cached environment
// check, so untraced frames pay a predictable branch and nothing else.
// Totals cover every GpuScene encode between two end_frame() calls,
// including stereo eyes and isolated overlay scenes.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <string_view>
#include <vector>

#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL_gpu.h>
#endif

namespace starfox::render::scene_counters {

enum class Counter : std::size_t {
    model_draws,           // GpuModelDraw entries encoded
    compute_passes,        // compute passes begun by scene encoders
    full_frame_dispatches, // raster/merge passes dispatched over the whole output
    bounded_dispatches,    // GPU FAST model rasters limited to their screen box
    raster_commands,       // CPU-recorded RasterCommand entries uploaded
    raster_bytes,          // bytes of those RasterCommand entries
    upload_bytes,          // bytes copied CPU->GPU by scene encoders
    compact_tile_lists,    // GPU FAST compact ordered row-span tile lists built
    count,
};

inline constexpr std::array<std::string_view,std::size_t(Counter::count)> counter_names{
    "models","compute-passes","full-frame-dispatches","bounded-dispatches","raster-commands","raster-bytes","upload-bytes",
    "compact-tile-lists"};

using Totals=std::array<std::uint64_t,std::size_t(Counter::count)>;

[[nodiscard]] inline bool enabled() noexcept {
    static const bool value=std::getenv("STARFOX_TRACE_SCENE_COST")!=nullptr;
    return value;
}

inline std::array<std::atomic<std::uint64_t>,std::size_t(Counter::count)>& frame_totals() noexcept {
    static std::array<std::atomic<std::uint64_t>,std::size_t(Counter::count)> value{};
    return value;
}

inline std::vector<Totals>& measured_frames() {
    static std::vector<Totals> value;
    return value;
}

inline void add(Counter counter,std::uint64_t amount=1) noexcept {
    if(enabled()) frame_totals()[std::size_t(counter)].fetch_add(amount,std::memory_order_relaxed);
}

// Closes the current frame: prints its totals, keeps them for the run
// summary when measured, and starts the next frame from zero.
inline void end_frame(std::ostream& out,std::uint64_t frame,bool measured) {
    if(!enabled()) return;
    Totals totals{};
    for(std::size_t i=0;i<totals.size();++i) totals[i]=frame_totals()[i].exchange(0,std::memory_order_relaxed);
    out<<"scene-counters frame="<<frame;
    for(std::size_t i=0;i<totals.size();++i) out<<' '<<counter_names[i]<<'='<<totals[i];
    out<<'\n';
    if(measured) measured_frames().push_back(totals);
}

// One line per counter over the measured frames, in the same
// median/p95/p99/max form as the frame-work distributions.
inline void print_summary(std::ostream& out) {
    if(!enabled() || measured_frames().empty()) return;
    const auto& frames=measured_frames();
    std::vector<std::uint64_t> values(frames.size());
    for(std::size_t i=0;i<counter_names.size();++i) {
        std::transform(frames.begin(),frames.end(),values.begin(),[i](const Totals& t){return t[i];});
        std::sort(values.begin(),values.end());
        const auto at=[&](std::size_t percent){return values[(values.size()-1)*percent/100];};
        out<<"scene-counters-distribution "<<counter_names[i]<<" median="<<at(50)<<" p95="<<at(95)
            <<" p99="<<at(99)<<" max="<<values.back()<<" frames="<<values.size()<<'\n';
    }
}

#if defined(STARFOX_SDL_GPU_EFFECTS)
// Drop-in replacements for the SDL calls, so each site keeps its shape.
inline SDL_GPUComputePass* begin_compute_pass(SDL_GPUCommandBuffer* command,
    const SDL_GPUStorageTextureReadWriteBinding* textures,Uint32 texture_count,
    const SDL_GPUStorageBufferReadWriteBinding* buffers,Uint32 buffer_count) {
    add(Counter::compute_passes);
    return SDL_BeginGPUComputePass(command,textures,texture_count,buffers,buffer_count);
}

inline void upload_buffer(SDL_GPUCopyPass* copy,const SDL_GPUTransferBufferLocation* source,
    const SDL_GPUBufferRegion* destination,bool cycle) {
    add(Counter::upload_bytes,destination->size);
    SDL_UploadToGPUBuffer(copy,source,destination,cycle);
}
#endif

} // namespace starfox::render::scene_counters
