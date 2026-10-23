#pragma once
#include "starfox/platform/nintendo_3ds/frontend.hpp"
#include <cstring>

namespace starfox::platform::nintendo_3ds {
inline void validate_rgba_lcd(ImageView source, std::span<std::uint8_t> destination) {
    if (!valid_image(source, bottom_width, screen_height)
        || destination.size() < std::size_t(bottom_width) * screen_height * 4)
        throw std::invalid_argument("Invalid lower RGBA LCD image");
    const auto src = reinterpret_cast<std::uintptr_t>(source.pixels.data());
    const auto dst = reinterpret_cast<std::uintptr_t>(destination.data());
    const auto bytes = std::size_t(bottom_width) * screen_height * 4;
    if ((dst >= src && dst - src < source.pixels.size())
        || (src >= dst && src - dst < bytes))
        throw std::invalid_argument("RGBA LCD conversion requires independent storage");
}

// Small transposition tiles retain both row-major input and rotated LCD lines
// in the ARM11 cache. No temporary frame, rounding or palette changes.
inline void copy_rgba_lcd(ImageView source, std::span<std::uint8_t> destination) {
    validate_rgba_lcd(source, destination);
    constexpr unsigned tile = 16;
    static_assert(bottom_width % tile == 0 && screen_height % tile == 0);
    for (unsigned by = 0; by < screen_height; by += tile)
        for (unsigned bx = 0; bx < bottom_width; bx += tile)
            for (unsigned x = bx; x < bx + tile; ++x)
                for (unsigned y = by; y < by + tile; ++y) {
                    const auto* in = source.pixels.data() + std::size_t(y) * source.pitch + x * 3;
                    auto* out = destination.data() + (std::size_t(x) * screen_height + screen_height - 1 - y) * 4;
                    out[0] = 255; out[1] = in[2]; out[2] = in[1]; out[3] = in[0];
                }
}

// Only this display owner may write its two LCD buffers. Compare actual RGB
// bytes (not a mutable pointer/hash) and certify each physical buffer only
// after the SDK cache flush succeeds. Destruction retires every certificate.
class RgbaLcdCache {
public:
    RgbaLcdCache() : pixels_(std::size_t(bottom_width) * screen_height * 3) {}
    void invalidate() noexcept {
        slots_ = {};pending_ = nullptr;next_slot_ = 0;valid_ = false;
    }
    bool prepare(ImageView source, std::span<std::uint8_t> destination) {
        validate_rgba_lcd(source, destination);
        pending_ = nullptr;
        const auto row_bytes = std::size_t(bottom_width) * 3;
        bool same = valid_;
        if (same && source.pitch == row_bytes)
            same = std::memcmp(source.pixels.data(), pixels_.data(), pixels_.size()) == 0;
        else if (same)
            for (unsigned y = 0; y < screen_height && same; ++y)
                same = std::memcmp(source.pixels.data() + std::size_t(y) * source.pitch,
                    pixels_.data() + std::size_t(y) * row_bytes, row_bytes) == 0;
        if (!same) {
            for (auto& slot : slots_) slot = {};
            for (unsigned y = 0; y < screen_height; ++y)
                std::memcpy(pixels_.data() + std::size_t(y) * row_bytes,
                    source.pixels.data() + std::size_t(y) * source.pitch, row_bytes);
            valid_ = true;
        }
        for (const auto& slot : slots_)
            if (slot == destination.data()) return false;
        copy_rgba_lcd(source, destination);
        pending_ = destination.data();
        return true;
    }
    void commit(std::span<std::uint8_t> destination) {
        if (!pending_ || pending_ != destination.data()
            || destination.size() < std::size_t(bottom_width) * screen_height * 4)
            throw std::logic_error("No completed RGBA LCD transfer to certify");
        slots_[next_slot_] = pending_;
        next_slot_ = (next_slot_ + 1) % slots_.size();
        pending_ = nullptr;
    }
private:
    std::vector<std::uint8_t> pixels_;
    std::array<const std::uint8_t*, 2> slots_{};
    std::uint8_t* pending_{};
    unsigned next_slot_{};
    bool valid_{};
};
}
