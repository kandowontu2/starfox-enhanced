#include "starfox/platform/nintendo_3ds/game_assets.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include <limits>

namespace starfox::platform::nintendo_3ds {
GameCartridge read_game_cartridge(std::istream& input,std::uint32_t manifest,
    simulation::Experience experience) {
    if(experience!=simulation::Experience::original && experience!=simulation::Experience::starfox_ex)
        throw std::invalid_argument("Unsupported 3DS cartridge experience");
    input.seekg(0,std::ios::end);
    const auto size=input.tellg();
    if(!input || size<48 || size>static_cast<std::streamoff>(maximum_companion_bytes))
        throw std::runtime_error("Starfox-Assets.BIN is missing, truncated or too large for original 3DS");
    input.seekg(0,std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!input || input.gcount()!=static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("Unable to read complete Starfox-Assets.BIN from SD card");
    // Shared decoder verifies all payload CRCs, lengths and the complete file,
    // even for the cartridge that is not currently selected.
    auto payload=assets::decode_runtime_bundle(bytes,manifest);
    const bool ex=experience==simulation::Experience::starfox_ex;
    auto symbols=assets::SymbolMap::parse(ex?payload.starfox_ex_symbols:payload.original_symbols);
    // BOOT is a host flow entry, not a label in either cartridge's symbols.
    if(symbols.find("VIEWPOSX").empty()) throw std::runtime_error("Companion has no camera RAM symbols");
    const bool ex_symbols=!symbols.find("SPECWEPCNTONE").empty();
    if(ex_symbols!=ex) throw std::runtime_error("Companion cartridge/symbol experience mismatch");
    return {assets::RomImage(std::move(ex?payload.starfox_ex_rom:payload.original_rom)),std::move(symbols)};
}
} // namespace starfox::platform::nintendo_3ds
