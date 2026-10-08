#include "starfox/assets/runtime_import.hpp"
#include "starfox/assets/embedded.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"unable to open file: " + path.string()};
    }
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{},
    };
}

void write_file(const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes) {
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            throw std::runtime_error{"unable to create output directory: "
                + error.message()};
        }
    }
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        throw std::runtime_error{"unable to create output: " + path.string()};
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error{"unable to write output: " + path.string()};
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3) {
            std::cerr << "usage: starfox_asset_builder RETAIL_ROM "
                         "[Starfox-Assets.BIN]\n";
            return 2;
        }
        const auto input = std::filesystem::path{argv[1]};
        const auto output = argc == 3
            ? std::filesystem::path{argv[2]}
            : std::filesystem::path{"Starfox-Assets.BIN"};
        const auto prepared = starfox::assets::prepare_runtime_input(
            read_file(input), starfox::assets::embedded_asset);
        write_file(output, prepared.bundle);
        std::cout << "Validated " << prepared.source_name << "\nCreated "
                  << std::filesystem::absolute(output).string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Starfox-Assets.BIN generation failed: "
                  << error.what() << '\n';
        return 1;
    }
}
