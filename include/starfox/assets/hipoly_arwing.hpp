#pragma once

#include "starfox/assets/rom.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace starfox::assets {

// UltraStarFox's hipolyarwing build replaces every player and teammate Arwing
// with the ending cutscene model (MYDEMO), including broken-wing and
// wireframe variants. Retail ROMs lack those streams, so this copies the ROM,
// maps the assembled streams into one unused LoROM bank and points each
// Arwing header at them. Only renderers read the copy; the simulation keeps
// the cartridge's own headers, so collision and timing are unchanged.
struct HipolyArwings {
    std::shared_ptr<const RomImage> rom;
    std::vector<std::uint16_t> shapes; // Retail header addresses now redirected.

    [[nodiscard]] bool replaces(std::uint32_t shape) const noexcept;
};

// Empty (no rom) when the symbols do not name the retail Arwing headers, for
// example a ROM that was already assembled with hipolyarwing on.
[[nodiscard]] HipolyArwings build_hipoly_arwings(const RomImage& rom, const SymbolMap& symbols);

} // namespace starfox::assets
