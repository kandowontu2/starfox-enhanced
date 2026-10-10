#pragma once
#include <cstddef>
#include <string_view>
namespace starfox::render::embedded_metal {
struct Program {
    const char* name;
    const char* entry;
    const unsigned char* bytes;
    std::size_t size;
    unsigned uniforms, readonly_buffers, writable_buffers, samplers;
    unsigned threads_x, threads_y, threads_z;
};
// A caller must retain its full existing descriptor and dispatch recipe.
// Finding a library does not qualify numerical precision or device support.
inline bool matches(const Program& p, unsigned uniforms, unsigned readonly_buffers,
                    unsigned writable_buffers, unsigned samplers,
                    unsigned x, unsigned y, unsigned z) noexcept {
    return p.uniforms == uniforms && p.readonly_buffers == readonly_buffers &&
           p.writable_buffers == writable_buffers && p.samplers == samplers &&
           p.threads_x == x && p.threads_y == y && p.threads_z == z;
}
}
