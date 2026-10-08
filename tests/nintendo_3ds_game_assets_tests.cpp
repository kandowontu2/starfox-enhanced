#include "starfox/platform/nintendo_3ds/game_assets.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include "companion_manifest.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace starfox;
using namespace starfox::platform::nintendo_3ds;
namespace {
unsigned checks{};
void check(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F&& run) {
    try {run();} catch(const std::exception&) {++checks;return;}
    throw std::runtime_error("Malformed companion accepted");
}
std::istringstream stream(const std::vector<std::uint8_t>& bytes) {
    return std::istringstream(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()),std::ios::binary);
}
void generated_manifest(const std::filesystem::path& root) {
    constexpr std::array files{
        "assets/patches/ultrastarfox-v12.bps","assets/symbols/ultrastarfox.txt",
        "assets/patches/starfox-ex-v12.bps","assets/symbols/starfox-ex.txt",
        "assets/patches/retail-japan-v10-to-usa-v12.bps","assets/patches/retail-japan-v11-to-usa-v12.bps",
        "assets/patches/retail-usa-v10-to-v12.bps","assets/patches/retail-usa-v11-to-v12.bps",
        "assets/patches/retail-europe-v10-to-usa-v12.bps","assets/patches/retail-europe-v11-to-usa-v12.bps",
        "assets/patches/retail-germany-v10-to-usa-v12.bps"};
    std::array<std::vector<std::uint8_t>,files.size()> owned;
    std::array<assets::RuntimeManifestResource,files.size()> resources;
    for(std::size_t index=0;index<files.size();++index) {
        std::ifstream input(root/files[index],std::ios::binary);
        check(static_cast<bool>(input),"Missing public manifest resource");
        owned[index]={std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        resources[index]={owned[index],index==1 || index==3};
    }
    check(assets::runtime_asset_manifest(resources)==companion_manifest,
        "Generated manifest differs from shared runtime C++ checksum");
}
class Oversized final:public std::streambuf {
    pos_type seekoff(off_type,std::ios_base::seekdir,std::ios_base::openmode) override {
        return pos_type(maximum_companion_bytes+1);
    }
};
}
int main(int argc,char** argv) try {
    if(argc!=2) throw std::invalid_argument("Usage: game_assets_tests SOURCE_ROOT (public assets only)");
    generated_manifest(argv[1]);
    assets::RuntimeBundlePayload payload;
    payload.original_rom.assign(0x8000,0x31);payload.starfox_ex_rom.assign(0x10000,0x62);
    payload.original_symbols="VIEWPOSX $0000b4\r\n";
    payload.starfox_ex_symbols="VIEWPOSX $0000c2\nSPECWEPCNTONE $000123\n";
    const auto encoded=assets::encode_runtime_bundle(payload,companion_manifest);
    for(const auto experience:{simulation::Experience::original,simulation::Experience::starfox_ex}) {
        auto input=stream(encoded);input.seekg(7); // Reader owns its whole-file position.
        const auto cartridge=read_game_cartridge(input,companion_manifest,experience);
        const bool ex=experience==simulation::Experience::starfox_ex;
        check(cartridge.rom.bytes()==(ex?payload.starfox_ex_rom:payload.original_rom),"Wrong selected cartridge");
        check(cartridge.symbols.find("VIEWPOSX").front()==(ex?0x00c2U:0x00b4U),"Wrong selected symbols");
    }
    auto input=stream(encoded);
    rejects([&]{read_game_cartridge(input,companion_manifest^1U,simulation::Experience::original);});
    auto bad=encoded;bad[0]^=1;
    input=stream(bad);rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::original);});
    bad=encoded;bad[bad.size()-10]^=1; // Corruption of UNSELECTED EX symbols also rejects Original.
    input=stream(bad);rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::original);});
    bad=encoded;bad.resize(40);input=stream(bad);
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::original);});
    bad=encoded;bad.pop_back();input=stream(bad);
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::starfox_ex);});
    bad=encoded;bad.push_back(0);input=stream(bad);
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::starfox_ex);});
    auto malformed=payload;malformed.original_symbols="NO_CAMERA $0000b4\n";
    input=stream(assets::encode_runtime_bundle(malformed,companion_manifest));
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::original);});
    malformed=payload;malformed.original_symbols=payload.starfox_ex_symbols;
    input=stream(assets::encode_runtime_bundle(malformed,companion_manifest));
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::original);});
    malformed=payload;malformed.starfox_ex_rom.clear();
    input=stream(assets::encode_runtime_bundle(malformed,companion_manifest));
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::starfox_ex);});
    malformed=payload;malformed.original_rom.push_back(0); // Copier-header/misaligned ROM.
    input=stream(assets::encode_runtime_bundle(malformed,companion_manifest));
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::original);});
    Oversized oversized;std::istream limit(&oversized);
    rejects([&]{read_game_cartridge(limit,companion_manifest,simulation::Experience::original);});
    input=stream(encoded);input.setstate(std::ios::badbit);
    rejects([&]{read_game_cartridge(input,companion_manifest,simulation::Experience::original);});
    std::cout<<"3DS companion: "<<checks<<" checks passed; synthetic bundle / shared public manifest\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
