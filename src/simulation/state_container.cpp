#include "starfox/state/container.hpp"
#include "starfox/state/archive.hpp"
#include "starfox/assets/bps.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace starfox::state {
namespace {
constexpr std::array<std::uint8_t,8> magic{'S','F','E','S','T','A','T','E'};
constexpr std::size_t header_size=20, trailer_size=4;
constexpr std::size_t maximum_payload=64U*1024U*1024U;
}
std::vector<std::uint8_t> pack(std::uint32_t schema, std::uint32_t rom_crc,
    std::span<const std::uint8_t> payload) {
    if (payload.size()>maximum_payload) throw std::runtime_error{"save state is too large"};
    Writer header;
    header(magic,schema,rom_crc,static_cast<std::uint32_t>(payload.size()));
    auto result=header.bytes();
    result.insert(result.end(),payload.begin(),payload.end());
    Writer trailer;
    trailer(assets::crc32(result));
    result.insert(result.end(),trailer.bytes().begin(),trailer.bytes().end());
    return result;
}
std::span<const std::uint8_t> unpack(std::span<const std::uint8_t> file,
    std::uint32_t expected_schema, std::uint32_t expected_rom_crc) {
    if (file.size()<header_size+trailer_size || file.size()>maximum_payload+header_size+trailer_size)
        throw std::runtime_error{"invalid save-state length"};
    Reader header{file.first(header_size)};
    std::array<std::uint8_t,8> signature{};
    std::uint32_t schema{},rom_crc{},length{},checksum{};
    header(signature,schema,rom_crc,length);
    header.finish();
    if (signature!=magic || schema!=expected_schema || rom_crc!=expected_rom_crc)
        throw std::runtime_error{"save state belongs to another format or cartridge"};
    if (length!=file.size()-header_size-trailer_size)
        throw std::runtime_error{"truncated or extended save state"};
    Reader trailer{file.last(trailer_size)};
    trailer(checksum);
    if (checksum!=assets::crc32(file.first(file.size()-trailer_size)))
        throw std::runtime_error{"save-state checksum mismatch"};
    return file.subspan(header_size,length);
}
} // namespace starfox::state
