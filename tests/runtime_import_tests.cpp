#include "starfox/assets/runtime_import.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include "starfox/assets/bps.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace {
std::map<int, std::vector<std::uint8_t>> resources;
std::span<const std::uint8_t> resource(int id) { return resources.at(id); }
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void rejects(Action action) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Invalid import accepted");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2 && argc != 4) throw std::runtime_error("usage: import_tests SOURCE_ROOT [USA_REV2_ROM OUTPUT_BIN]");
        const std::filesystem::path root(argv[1]);
        const std::map<int, const char*> paths{
            {101,"assets/patches/ultrastarfox-v12.bps"}, {102,"assets/symbols/ultrastarfox.txt"},
            {108,"assets/patches/starfox-ex-v12.bps"}, {109,"assets/symbols/starfox-ex.txt"},
            {120,"assets/patches/retail-japan-v10-to-usa-v12.bps"},
            {121,"assets/patches/retail-japan-v11-to-usa-v12.bps"},
            {122,"assets/patches/retail-usa-v10-to-v12.bps"},
            {123,"assets/patches/retail-usa-v11-to-v12.bps"},
            {124,"assets/patches/retail-europe-v10-to-usa-v12.bps"},
            {125,"assets/patches/retail-europe-v11-to-usa-v12.bps"},
            {126,"assets/patches/retail-germany-v10-to-usa-v12.bps"}};
        for (const auto& [id, path] : paths) resources[id] = read(root / path);
        using namespace starfox::assets;
        const auto manifest = runtime_companion_manifest(resource);
        const RuntimeBundlePayload fixture{{1,2,3}, "original", {4,5,6}, "ex"};
        const auto bundle = encode_runtime_bundle(fixture, manifest);
        require(prepare_runtime_input(bundle, resource).bundle == bundle, "BIN changed during import");
        auto damaged = bundle; damaged.back() ^= 1;
        rejects([&] { (void)prepare_runtime_input(damaged, resource); });
        const auto stale = encode_runtime_bundle(fixture, manifest ^ 1);
        rejects([&] { (void)prepare_runtime_input(stale, resource); });
        rejects([&] { (void)prepare_runtime_input({}, resource); });
        const std::vector<std::uint8_t> unknown(1048576);
        rejects([&] { (void)prepare_runtime_input(unknown, resource); });
        const std::vector<std::uint8_t> archive{'P','K',3,4};
        rejects([&] { (void)prepare_runtime_input(archive, resource); });
        rejects([&] { (void)prepare_runtime_input(bundle, nullptr); });
        if (argc == 4) {
            const auto rom = read(argv[2]);
            require(rom.size() == 1048576 && crc32(rom) == 0x8fc4e6d0, "Expected USA Rev 2 test input");
            const auto result = prepare_runtime_input(rom, resource);
            const auto decoded = decode_runtime_bundle(result.bundle, manifest);
            require(decoded.original_rom == apply_bps_patch(rom, resource(101)), "Original differs from source patch");
            require(decoded.starfox_ex_rom == apply_bps_patch(rom, resource(108)), "EX differs from source patch");
            require(std::vector<std::uint8_t>(decoded.original_symbols.begin(), decoded.original_symbols.end()) == resources[102], "Original symbols changed");
            require(std::vector<std::uint8_t>(decoded.starfox_ex_symbols.begin(), decoded.starfox_ex_symbols.end()) == resources[109], "EX symbols changed");
            auto headered = rom; headered.insert(headered.begin(), 512, 0xa5);
            require(prepare_runtime_input(headered, resource).bundle == result.bundle, "Copier header changes output");
            require(prepare_runtime_input(result.bundle, resource).bundle == result.bundle, "Reimport changes bundle");
            auto modified = rom; modified[12345] ^= 1;
            rejects([&] { (void)prepare_runtime_input(modified, resource); });
            auto broken_patch = resources[101]; resources[101].back() ^= 1;
            rejects([&] { (void)prepare_runtime_input(rom, resource); });
            resources[101] = std::move(broken_patch);
            std::ofstream output(argv[3], std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(result.bundle.data()), result.bundle.size());
            output.close(); require(bool(output), "Cannot write test bundle");
            std::cout << "Retail/headered ROM, Original + EX patch parity and BIN reimport passed\n";
        }
        std::cout << "Quest runtime import validation passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
