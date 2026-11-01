#include "starfox/render/backdrop_png.hpp"
#include "lodepng.h"
#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace starfox::render {
std::vector<std::uint32_t> decode_backdrop_png(std::span<const std::uint8_t> bytes, unsigned& width, unsigned& height) {
    struct State {
        LodePNGState value;
        State() {lodepng_state_init(&value);}
        ~State() {lodepng_state_cleanup(&value);}
    } state;
    unsigned w{}, h{};
    const auto inspected = lodepng_inspect(&w, &h, &state.value, bytes.data(), bytes.size());
    if (inspected || !w || !h || w > 8192 || h > 8192
        || (state.value.info_png.color.colortype != LCT_RGB && state.value.info_png.color.colortype != LCT_RGBA)
        || state.value.info_png.color.bitdepth != 8 || state.value.info_png.interlace_method != 0)
        throw std::runtime_error("Unsupported backdrop PNG dimensions or format");
    // Runtime artwork is RGB(A)8. Bound deflate expansion before decoding; do not
    // disable CRC/Adler checks, and never allocate from unchecked PNG sizes.
    const unsigned channels=state.value.info_png.color.colortype==LCT_RGBA?4:3;
    state.value.decoder.zlibsettings.max_output_size = (std::size_t(w) * channels + 1) * h;
    state.value.info_raw.colortype = LCT_RGBA;
    state.value.info_raw.bitdepth = 8;
    unsigned char* raw{};
    const auto error = lodepng_decode(&raw, &w, &h, &state.value, bytes.data(), bytes.size());
    std::unique_ptr<unsigned char, decltype(&std::free)> owned(raw, std::free);
    if (error || !raw) throw std::runtime_error("Invalid or truncated backdrop PNG");
    std::vector<std::uint32_t> pixels(std::size_t(w) * h);
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = std::uint32_t(raw[i * 4]) | (std::uint32_t(raw[i * 4 + 1]) << 8)
            | (std::uint32_t(raw[i * 4 + 2]) << 16) | 0xff000000u;
    width = w; height = h;
    return pixels;
}
}
