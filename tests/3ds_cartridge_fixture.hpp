#pragma once
#include "starfox/platform/nintendo_3ds/game_assets.hpp"
#include "companion_manifest.hpp"
#include <fstream>
#include <string_view>

// Read the user's existing companion in memory. Never unpack or duplicate ROMs.
inline starfox::platform::nintendo_3ds::GameCartridge test_3ds_cartridge(int argc,char** argv) {
    using namespace starfox;
    using namespace platform::nintendo_3ds;
    if(argc==4 && std::string_view(argv[1])=="--companion") {
        const std::string_view mode=argv[3];
        if(mode!="original" && mode!="ex") throw std::invalid_argument("Expected original or ex");
        std::ifstream input(argv[2],std::ios::binary);
        return read_game_cartridge(input,companion_manifest,
            mode=="ex"?simulation::Experience::starfox_ex:simulation::Experience::original);
    }
    if(argc!=3) throw std::invalid_argument("Use ROM SYMBOLS or --companion EXISTING_BIN original|ex");
    return {assets::RomImage::load(argv[1]),assets::SymbolMap::load(argv[2])};
}
