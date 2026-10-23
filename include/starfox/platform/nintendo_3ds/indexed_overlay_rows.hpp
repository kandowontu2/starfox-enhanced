#pragma once

#include "starfox/render/framebuffer.hpp"
#include <array>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds {

// A bounded sparse-row index for an immutable cached cartridge overlay.
// Only the original 256x224 source rectangle is transferred. Zero ink remains
// transparent; set_indexed_row preserves ordinary coverage, layer ownership,
// dither erasure, clipping, scaling and ordered point commands.
class IndexedOverlayRows {
public:
    void prepare(const render::Framebuffer& source) {
        validate(source);
        source_width_ = source.stored_width();
        const auto& pixels = source.pixels();
        for (unsigned y = 0; y < rows_.size(); ++y) {
            auto& row = rows_[y];
            row = {256U, 0U};
            const auto offset = std::size_t(y) * source_width_;
            for (unsigned x = 0; x < 256U; ++x) if (pixels[offset + x]) {
                row.first = std::min(row.first, std::uint16_t(x));
                row.last = std::uint16_t(x + 1U);
            }
        }
        valid_ = true;
    }

    void apply(const render::Framebuffer& source,
        render::Framebuffer& destination) const {
        validate(source);
        if (!valid_ || source_width_ != source.stored_width())
            throw std::invalid_argument("Unprepared or resized 3DS overlay source");
        const auto pixels = std::span(source.pixels());
        for (unsigned y = 0; y < rows_.size(); ++y) {
            const auto row = rows_[y];
            if (row.first >= row.last) continue;
            destination.set_indexed_row(row.first, int(y), pixels.subspan(
                std::size_t(y) * source_width_ + row.first, row.last - row.first));
        }
    }

private:
    static void validate(const render::Framebuffer& source) {
        if (source.draw_scale() != 1U || source.width() < 256U || source.height() < 224U)
            throw std::invalid_argument("Incomplete native 3DS overlay source");
    }
    struct Row { std::uint16_t first{}, last{}; };
    std::array<Row, 224U> rows_{};
    std::uint32_t source_width_{};
    bool valid_{};
};
static_assert(sizeof(IndexedOverlayRows) <= 904U);

} // namespace starfox::platform::nintendo_3ds
