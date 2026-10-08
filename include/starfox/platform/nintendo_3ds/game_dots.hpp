#pragma once
#include "starfox/platform/nintendo_3ds/game_presentation.hpp"
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/platform/nintendo_3ds/raster_coverage.hpp"
#include "starfox/render/palette.hpp"

namespace starfox::platform::nintendo_3ds {
struct GameDotCoverage {
    unsigned dust{},grid{},connections{},ink_updates{};
};
// Cartridge dust/lattice ink, not a finished scene image. The immutable camera
// geometry and finite grid receiver are shared by both independently projected
// eyes. Current recycled point identities and carried line endpoints are never
// advanced by rendering.
class GameDots {
public:
    GameDots(const assets::RomImage&,const assets::SymbolMap&);
    explicit GameDots(std::array<std::uint8_t,64> star_colours):star_colours_(star_colours) {}
    PicaFrame prepare(const GamePresentation&);
    [[nodiscard]] GameDotCoverage coverage() const noexcept {return coverage_;}
private:
    std::array<std::uint8_t,64> star_colours_{};
    std::vector<PicaVertex> vertices_;
    std::array<PicaDraw,pica_raster_max_strips*2> draws_{};
    std::array<PicaImage,pica_raster_max_strips> image_{};
    unsigned ink_width_{};
    std::vector<std::uint8_t> ink_,rgba_;
    std::optional<std::array<std::int16_t,14>> ink_key_;
    std::optional<std::array<std::uint8_t,4>> ink_colour_;
    GameDotCoverage coverage_{};
};
} // namespace starfox::platform::nintendo_3ds
