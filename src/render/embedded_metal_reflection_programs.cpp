#include "starfox/render/embedded_metal_reflection_programs.hpp"
#include "embedding_programs.hpp"
#include <array>
namespace starfox::render::embedded_metal {
std::span<const Program> programs_for_sdk() noexcept {
    static const auto programs=[] {
        std::array<Program,25> result{};
        for(std::size_t n=0;n<result.size();++n) {
            const auto& p=embedding_programs[n];
            result[n]={p.name,p.entry,p.bytes,p.size,p.uniforms,p.readonly_buffers,
                p.writable_buffers,p.samplers,p.x,p.y,p.z};
        }
        return result;
    }();
    return programs;
}
}
