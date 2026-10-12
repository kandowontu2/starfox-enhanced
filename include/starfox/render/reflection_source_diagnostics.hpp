#pragma once
#include <cstdint>

namespace starfox::render {
// Read-only diagnostic markers between passes in the SAME unsubmitted command.
// Never upload optical inputs, submit/wait/read results, or modify proof/RGB.
enum class ReflectionSourceStage : unsigned {
    begin,queries,optical,roots,witness,folds,guide,colour,composition,publication
};
struct ReflectionSourceStageEvent {
    void* device{};void* command{};
    std::uint64_t first{},total{};
    std::uint32_t count{},eye{},sample{};
    ReflectionSourceStage stage{};
    // Read-only SAME-command diagnostic inputs, present only at composition.
    // They expire before the next streamed batch. A callback may encode a
    // diagnostic download, but must not modify/read/submit/wait these buffers
    // or feed a downloaded receipt back into the rendering pipeline.
    void* queries{};
    void* proofs{};
    std::uint32_t query_stride{},proof_stride{},proof_bytes{};
};
using ReflectionSourceStageObserver=void(*)(void*,const ReflectionSourceStageEvent&);
} // namespace starfox::render
