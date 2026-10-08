#pragma once
#include "starfox/assets/rom.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <istream>

namespace starfox::platform::nintendo_3ds {
struct GameCartridge { assets::RomImage rom; assets::SymbolMap symbols; };
// Standard companion, checked against the build's public-resource manifest.
// Never trust the file's own manifest as the expected value. Bounded before
// allocation for original 3DS RAM; unselected cartridge/storage die before the
// much larger GameSession is constructed. No filesystem or SDK dependency.
inline constexpr std::size_t maximum_companion_bytes=12U*1024U*1024U;
GameCartridge read_game_cartridge(std::istream&,std::uint32_t expected_manifest,
    simulation::Experience);
} // namespace starfox::platform::nintendo_3ds
