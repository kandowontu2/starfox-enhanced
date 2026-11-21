#include "starfox/assets/bps.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/asteroid_models.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/render/texture_overrides.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Writes every Super FX texture straight from the ROM, so modders need not
// play through the game to collect them. Names and format match MODERN's
// replacement loader: tex_AAAAAA_l.png / tex_AAAAAA_h.png (SNES ROM address,
// low/high nibble plane), RGBA with texel 0 transparent.

namespace {

using Palette = std::array<std::uint16_t, 16>;

struct Edition {
    std::string label;
    starfox::assets::RomImage rom;
    starfox::assets::SymbolMap symbols;
};

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void put_chunk(std::vector<std::uint8_t>& out, const char* type,
    const std::vector<std::uint8_t>& data) {
    put_u32(out, static_cast<std::uint32_t>(data.size()));
    const auto start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put_u32(out, starfox::assets::crc32(
        std::span{out}.subspan(start, data.size() + 4U)));
}

// RGBA to match MODERN's own dump; index 0 becomes alpha 0 unless it is
// opaque art (portraits paint colour 0 as their backing). Uncompressed
// deflate keeps the encoder tiny, since textures are at most 256x128.
void write_png(const std::filesystem::path& path, std::size_t width,
    std::size_t height, const std::vector<std::uint8_t>& indices,
    const Palette& palette, bool opaque = false) {
    std::vector<std::uint8_t> raw;
    raw.reserve((width * 4U + 1U) * height);
    for (std::size_t y = 0; y < height; ++y) {
        raw.push_back(0U);
        for (std::size_t x = 0; x < width; ++x) {
            const auto index = indices[y * width + x] & 0x0fU;
            const auto clear = index == 0U && !opaque;
            const auto word = palette[index];
            // BGR555 to RGB888, replicating the top bits into the low bits.
            for (const auto shift : {0U, 5U, 10U}) {
                const auto channel = static_cast<std::uint8_t>((word >> shift) & 0x1fU);
                raw.push_back(clear ? 0U
                    : static_cast<std::uint8_t>((channel << 3U) | (channel >> 2U)));
            }
            raw.push_back(clear ? 0U : 255U);
        }
    }
    std::vector<std::uint8_t> zlib{0x78U, 0x01U};
    for (std::size_t at = 0; at < raw.size() || at == 0U; at += 65535U) {
        const auto length = std::min<std::size_t>(65535U, raw.size() - at);
        zlib.push_back(at + length == raw.size() ? 1U : 0U);
        zlib.push_back(static_cast<std::uint8_t>(length));
        zlib.push_back(static_cast<std::uint8_t>(length >> 8U));
        zlib.push_back(static_cast<std::uint8_t>(~length));
        zlib.push_back(static_cast<std::uint8_t>(~length >> 8U));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(at),
            raw.begin() + static_cast<std::ptrdiff_t>(at + length));
        if (raw.empty()) break;
    }
    std::uint32_t a = 1U, b = 0U;
    for (const auto byte : raw) {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    put_u32(zlib, (b << 16U) | a);

    std::vector<std::uint8_t> png{0x89U, 'P', 'N', 'G', '\r', '\n', 0x1aU, '\n'};
    std::vector<std::uint8_t> header;
    put_u32(header, static_cast<std::uint32_t>(width));
    put_u32(header, static_cast<std::uint32_t>(height));
    header.insert(header.end(), {8U, 6U, 0U, 0U, 0U});
    put_chunk(png, "IHDR", header);
    put_chunk(png, "IDAT", zlib);
    put_chunk(png, "IEND", {});
    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char*>(png.data()),
        static_cast<std::streamsize>(png.size()));
    if (!file) throw std::runtime_error{"could not write " + path.string()};
}

std::string hex(std::uint32_t value, int digits) {
    std::ostringstream text;
    text << std::hex << std::setw(digits) << std::setfill('0') << value;
    return text.str();
}

std::uint32_t rom_symbol(const starfox::assets::SymbolMap& symbols, const char* name) {
    const auto& values = symbols.find(name);
    if (values.empty()) throw std::runtime_error{std::string{"missing symbol "} + name};
    return values.front();
}

// The 3D view is drawn through CGRAM palette 7. Run the stage's own setup
// rather than guessing which ROM palette table it loads.
Palette stage_palette(const Edition& edition, const std::string& map) {
    starfox::simulation::GameSimulation game{edition.rom, edition.symbols, map};
    for (int tick = 0; tick < 60; ++tick) {
        game.present_frame();
        game.present_frame();
        game.present_frame();
        static_cast<void>(game.tick({}));
    }
    Palette palette{};
    const auto& cgram = game.map().ppu_state().cgram;
    std::copy_n(cgram.begin() + 7 * 16, 16, palette.begin());
    if (std::all_of(palette.begin() + 1, palette.end(), [](auto word) { return word == 0U; })) {
        palette = game.palette_words();
    }
    return palette;
}

// Communications portraits: FACEDATA (and EX's FACEDATA2) holds 32x40 frames
// of 20 SNES 4bpp tiles, column-major (vr::source_portrait_packet), drawn
// through palette 7 with colour 0 as an opaque backing. Each frame runs to
// the next symbol, which ends the table.
std::size_t dump_portraits(const Edition& edition, const std::filesystem::path& directory,
    const Palette& palette) {
    const auto& rom = edition.rom;
    std::vector<std::uint32_t> starts;
    for (const auto& [name, addresses] : edition.symbols.entries()) {
        for (const auto address : addresses) starts.push_back(address);
    }
    std::sort(starts.begin(), starts.end());
    std::ofstream index{directory / "portraits.csv"};
    index << "file,table,frame\n";
    std::size_t written = 0;
    for (const auto* table : {"FACEDATA", "FACEDATA2"}) {
        for (const auto base : edition.symbols.find(table)) {
            if ((base & 0xffffU) < 0x8000U || ((base >> 16U) & 0xffU) >= 0x70U) continue;
            const auto next = std::upper_bound(starts.begin(), starts.end(), base);
            const auto end = std::min<std::uint32_t>((base | 0xffffU) + 1U,
                next == starts.end() ? 0xffffffffU : *next);
            for (std::uint32_t frame = 0; base + (frame + 1U) * 640U <= end && frame < 256U; ++frame) {
                const auto address = base + frame * 640U;
                std::vector<std::uint8_t> pixels(32U * 40U);
                for (std::uint32_t tile = 0; tile < 20U; ++tile) {
                    const auto tile_x = tile / 5U, tile_y = tile % 5U;
                    const auto bytes = address + tile * 32U;
                    for (std::uint32_t row = 0; row < 8U; ++row) {
                        const std::array planes{rom.read8(bytes + row * 2U), rom.read8(bytes + row * 2U + 1U),
                            rom.read8(bytes + 16U + row * 2U), rom.read8(bytes + 17U + row * 2U)};
                        for (std::uint32_t column = 0; column < 8U; ++column) {
                            std::uint8_t ink = 0U;
                            for (std::uint32_t plane = 0; plane < 4U; ++plane) {
                                ink |= static_cast<std::uint8_t>(((planes[plane] >> (7U - column)) & 1U) << plane);
                            }
                            pixels[(tile_y * 8U + row) * 32U + tile_x * 8U + column] = ink;
                        }
                    }
                }
                const auto file = "face_" + hex(address & 0xffffffU, 6) + ".png";
                write_png(directory / "textures" / file, 32U, 40U, pixels, palette, true);
                index << file << ',' << table << ',' << frame << '\n';
                ++written;
            }
            break;
        }
    }
    return written;
}

// 2D OBJ sprites (HUD labels, icons, digits) live in VRAM, so play the stage
// headlessly and keep every sprite it shows, named by MODERN's content key
// (render::sprite_override_key). LIVES is then stepped so the HUD draws
// every digit.
std::size_t dump_sprites(const Edition& edition, const std::filesystem::path& directory,
    const std::string& map) {
    starfox::simulation::GameSimulation game{edition.rom, edition.symbols, map};
    constexpr unsigned sizes[6][2]{{8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}};
    std::set<std::uint64_t> seen;
    std::ofstream index{directory / "sprites.csv"};
    index << "file,size,palette,tile,x,y,tick\n";
    const auto capture = [&](unsigned tick) {
        const auto& ppu = game.map().ppu_state();
        if ((ppu.main_screen & 16U) == 0U) return;
        const auto selection = std::min(unsigned(ppu.object_select >> 5U), 5U);
        for (unsigned object = 0; object < 128U; ++object) {
            const auto low = object * 4U;
            const auto attr = ppu.oam[low + 3U];
            if ((ppu.oam[low] | ppu.oam[low + 1U] | ppu.oam[low + 2U] | attr) == 0U) continue;
            if (ppu.oam[low] == 248U && ppu.oam[low + 1U] == 248U) continue;
            const auto high = ppu.oam[512U + object / 4U] >> ((object % 4U) * 2U);
            const auto size = sizes[selection][(high >> 1U) & 1U];
            const auto tile = static_cast<std::uint16_t>(ppu.oam[low + 2U] | ((attr & 1U) << 8U));
            const unsigned palette = (attr >> 1U) & 7U;
            const auto inks = starfox::render::object_inks(ppu, tile, size);
            if (std::all_of(inks.begin(), inks.end(), [](auto ink) { return ink == 0U; })) continue;
            const auto key = starfox::render::sprite_override_key(inks, size, palette);
            if (!seen.insert(key).second) continue;
            Palette colours{};
            std::copy_n(ppu.cgram.begin() + 128 + palette * 16U, 16, colours.begin());
            const auto file = starfox::render::sprite_override_name(key) + ".png";
            write_png(directory / "textures" / file, size, size, inks, colours);
            int x = ppu.oam[low] + ((high & 1U) << 8U);
            if (x >= 256) x -= 512;
            index << file << ',' << size << ',' << palette << ",$" << hex(tile, 3) << ','
                  << x << ',' << unsigned(ppu.oam[low + 1U]) << ',' << tick << '\n';
        }
    };
    const auto advance = [&game] {
        game.present_frame();
        game.present_frame();
        game.present_frame();
        static_cast<void>(game.tick({}));
    };
    unsigned tick = 0;
    for (; tick < 400U; ++tick) {
        advance();
        if (tick % 10U == 0U) capture(tick);
    }
    if (const auto& lives = edition.symbols.find("LIVES"); !lives.empty()) {
        // The HUD shows LIVES minus one, so 10 draws the 9.
        for (std::uint8_t count = 0; count <= 10U; ++count, ++tick) {
            game.map().write_native_byte(lives.front(), count);
            advance();
            advance();
            capture(tick);
        }
    }
    return seen.size();
}

void dump_edition(const Edition& edition, const std::filesystem::path& root,
    const std::string& map) {
    const auto directory = root / edition.label;
    std::filesystem::create_directories(directory / "atlas");
    std::filesystem::create_directories(directory / "textures");
    const auto palette = stage_palette(edition, map);
    const auto& rom = edition.rom;
    const auto& symbols = edition.symbols;

    // Atlas pages: every bank the texture table points into, as two 4-bit
    // planes. This catches texels no decoded shape references.
    const auto table = rom_symbol(symbols, "TEXTUREADDRTAB");
    const auto texture_count = (rom_symbol(symbols, "TEXTUREXYTAB") - table) / 3U;
    std::set<std::uint8_t> banks;
    for (std::uint32_t index = 0; index < texture_count; ++index) {
        banks.insert(rom.read8(table + index * 3U + 2U));
    }
    std::size_t atlases = 0;
    for (const auto bank : banks) {
        const auto offset = static_cast<std::size_t>(bank & 0x7fU) * 0x8000U;
        if (offset + 0x8000U > rom.bytes().size()) continue;
        for (const auto high : {false, true}) {
            std::vector<std::uint8_t> indices(0x8000U);
            for (std::size_t at = 0; at < indices.size(); ++at) {
                const auto packed = rom.bytes()[offset + at];
                indices[at] = high ? packed >> 4U : packed & 0x0fU;
            }
            write_png(directory / "atlas"
                    / ("bank" + hex(bank, 2) + (high ? "_hi" : "_lo") + ".png"),
                256U, 128U, indices, palette);
            ++atlases;
        }
    }

    // Individual textures, cropped exactly as the shapes that use them sample.
    const starfox::assets::ShapeDecoder decoder{rom, symbols};
    std::map<std::uint16_t, starfox::assets::TextureImage> textures;
    std::map<std::uint16_t, std::set<std::string>> users;
    std::set<std::uint32_t> seen;
    for (const auto& [name, addresses] : symbols.entries()) {
        for (const auto address : addresses) {
            if (!seen.insert(address).second || !decoder.looks_like_shape_header(address)) {
                continue;
            }
            try {
                const auto shape = decoder.decode(address, name);
                for (const auto& texture : shape.textures) {
                    textures.emplace(texture.descriptor, texture);
                    users[texture.descriptor].insert(name);
                }
            } catch (const std::exception&) {
                // shape_coverage reports undecodable headers; skip them here.
            }
        }
    }
    // Some objects swap in their own colour table at runtime (BREAK_METEOR_C
    // supplies the orange asteroid), so no shape header names those textures.
    // Read each *_C table up to the next one, following COLANIM lists.
    const auto colour_bank = rom_symbol(symbols, "TEXTURES_C") & 0xff0000U;
    std::map<std::uint32_t, std::string> tables;
    for (const auto& [name, addresses] : symbols.entries()) {
        if (name.size() < 3U || !name.ends_with("_C")) continue;
        for (const auto address : addresses) {
            if ((address & 0xff0000U) == colour_bank) tables.emplace(address, name);
        }
    }
    const auto add_word = [&](std::uint16_t word, const std::string& owner) {
        try {
            if (auto texture = decoder.texture(word)) {
                textures.emplace(word, std::move(*texture));
                users[word].insert(owner);
            }
        } catch (const std::exception&) {
            // Data after a short table can look like a descriptor.
        }
    };
    for (auto table_it = tables.begin(); table_it != tables.end(); ++table_it) {
        const auto next = std::next(table_it);
        const auto end = std::min(table_it->first + 256U,
            next == tables.end() ? table_it->first + 256U : next->first);
        for (auto at = table_it->first; at + 1U < end; at += 2U) {
            const auto word = rom.read16(at);
            add_word(word, table_it->second);
            if ((word & 0xc000U) == 0x8000U) {
                const auto animation = colour_bank | 0x8000U | (word & 0x3fffU);
                const auto frames = rom.read8(animation);
                for (std::uint32_t frame = 0; frame < frames && frame < 128U; ++frame) {
                    add_word(rom.read16(animation + 1U + frame * 2U), table_it->second);
                }
            }
        }
    }
    // MODERN keys a replacement by ROM address and nibble plane, so several
    // descriptors (different sampled sizes of the same texels) share a file.
    // Write the largest; the CSV lists every size the game samples it at.
    struct Entry {
        const starfox::assets::TextureImage* largest{};
        std::set<std::uint16_t> descriptors;
        std::set<std::string> sizes, shapes;
    };
    std::map<std::string, Entry> files;
    for (const auto& [descriptor, texture] : textures) {
        const auto name = starfox::render::texture_override_name(
            starfox::render::texture_override_key(texture)) + ".png";
        auto& entry = files[name];
        const auto area = [](const auto* image) {
            return (image->u_mask + 1U) * (image->v_mask + 1U);
        };
        if (entry.largest == nullptr || area(&texture) > area(entry.largest)) entry.largest = &texture;
        entry.descriptors.insert(descriptor);
        entry.sizes.insert(std::to_string(texture.u_mask + 1U) + "x" + std::to_string(texture.v_mask + 1U));
        entry.shapes.insert(users[descriptor].begin(), users[descriptor].end());
    }
    const auto joined = [](const auto& values, auto&& format) {
        std::string text;
        for (const auto& value : values) text += (text.empty() ? "" : " ") + format(value);
        return text;
    };
    std::ofstream index{directory / "textures.csv"};
    index << "file,width,height,sampled_sizes,descriptors,shapes,texel_hash\n";
    for (const auto& [name, entry] : files) {
        const auto& texture = *entry.largest;
        const auto width = static_cast<std::size_t>(texture.u_mask) + 1U;
        const auto height = static_cast<std::size_t>(texture.v_mask) + 1U;
        write_png(directory / "textures" / name, width, height, texture.texels, palette);
        index << name << ',' << width << ',' << height << ",\""
              << joined(entry.sizes, [](const auto& size) { return size; }) << "\",\""
              << joined(entry.descriptors, [](auto value) { return "$" + hex(value, 4); }) << "\",\""
              << joined(entry.shapes, [](const auto& shape) { return shape; }) << "\","
              // The content hash 3D stand-ins match on (render::asteroid_texture_hash).
              << hex(starfox::render::asteroid_texture_hash(texture), 8) << '\n';
    }
    const auto portraits = dump_portraits(edition, directory, palette);
    const auto sprites = dump_sprites(edition, directory, map);
    std::cout << edition.label << ": " << files.size() << " textures, " << portraits
              << " portrait frames, " << sprites << " sprites, " << atlases
              << " atlas pages, palette from " << map << " -> " << directory.string() << '\n';
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) throw std::runtime_error{"could not open " + path.string()};
    return {std::istreambuf_iterator<char>{file}, {}};
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> arguments(argv + 1, argv + argc);
        std::string map = "LEVEL1_1";
        if (const auto flag = std::find(arguments.begin(), arguments.end(), "--map");
            flag != arguments.end()) {
            if (std::next(flag) == arguments.end()) throw std::runtime_error{"--map needs a name"};
            map = *std::next(flag);
            arguments.erase(flag, flag + 2);
        }
        std::vector<Edition> editions;
        std::filesystem::path output;
        if (arguments.size() == 2U) {
            // Starfox-Assets.BIN carries both editions. Its manifest only ties
            // it to a game build, so trust the one in its header; the CRCs are
            // still checked.
            const auto bytes = read_file(arguments[0]);
            if (bytes.size() < 12U) throw std::runtime_error{"asset bundle is truncated"};
            const auto manifest = static_cast<std::uint32_t>(bytes[8])
                | (static_cast<std::uint32_t>(bytes[9]) << 8U)
                | (static_cast<std::uint32_t>(bytes[10]) << 16U)
                | (static_cast<std::uint32_t>(bytes[11]) << 24U);
            auto bundle = starfox::assets::decode_runtime_bundle(bytes, manifest);
            editions.push_back({"original", starfox::assets::RomImage{std::move(bundle.original_rom)},
                starfox::assets::SymbolMap::parse(bundle.original_symbols)});
            editions.push_back({"ex", starfox::assets::RomImage{std::move(bundle.starfox_ex_rom)},
                starfox::assets::SymbolMap::parse(bundle.starfox_ex_symbols)});
            output = arguments[1];
        } else if (arguments.size() == 3U) {
            editions.push_back({"rom", starfox::assets::RomImage::load(arguments[0]),
                starfox::assets::SymbolMap::load(arguments[1])});
            output = arguments[2];
        } else {
            std::cerr << "usage: starfox_texture_dump Starfox-Assets.BIN OUTPUT_DIR [--map LEVEL1_1]\n"
                         "       starfox_texture_dump ROM SYMBOLS OUTPUT_DIR [--map LEVEL1_1]\n";
            return 2;
        }
        for (const auto& edition : editions) dump_edition(edition, output, map);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "texture dump failed: " << error.what() << '\n';
        return 1;
    }
}
