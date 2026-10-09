#pragma once
#include <cstddef>
#include <vector>

namespace starfox::render {

// Submitted work and its fence are separate lifetimes. Releasing an acquired
// SDL fence before completion can return it to the backend's reusable pool
// while a command buffer still depends on it. Retain every pending fence;
// wait only for completed work (backend cleanup) or bounded queue pressure.
// A failed wait retains the entry so the owner can recover/tear down safely.
template<class Fence,class Query,class Wait,class Release>
bool retire_submission_queue(std::vector<Fence>& pending,std::size_t limit,
    Query completed,Wait wait,Release release) {
    while(!pending.empty() && (pending.size()>limit || completed(pending.front()))) {
        const auto oldest=pending.front();
        if(!wait(oldest)) return false;
        release(oldest);
        pending.erase(pending.begin());
    }
    return true;
}

} // namespace starfox::render
