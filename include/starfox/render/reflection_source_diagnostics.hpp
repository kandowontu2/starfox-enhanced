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
};
using ReflectionSourceStageObserver=void(*)(void*,const ReflectionSourceStageEvent&);
} // namespace starfox::render
