#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

namespace starfox::render {
// Issue #49 isolates Windows Radeon Vulkan device loss to the resident
// cartridge passes. Keep GPU geometry/effects, but decode cartridge artwork
// on the host on that combination. Do not apply a Windows driver workaround
// to Mesa, other vendors, or AMD's independent D3D12 implementation.
inline constexpr bool host_cartridge_composition(bool windows_desktop,
    std::uint32_t vendor,std::string_view driver) noexcept {
    return windows_desktop && vendor==0x1002U && driver=="vulkan";
}

// An idle boundary is needed only when a live configuration actually changes,
// never on each frame or the first scene. The caller commits after retirement
// succeeds so a failed wait cannot authorize reuse of the old resources.
class GpuConfigurationBoundary {
public:
    bool changed(std::uint64_t signature) const noexcept {
        return previous_ && *previous_!=signature;
    }
    void commit(std::uint64_t signature) noexcept {previous_=signature;}
    void reset() noexcept {previous_.reset();}
private:
    std::optional<std::uint64_t> previous_;
};
}
