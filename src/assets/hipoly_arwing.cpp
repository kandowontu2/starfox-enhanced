#include "starfox/assets/hipoly_arwing.hpp"

#include "starfox/assets/hipoly_arwing_data.hpp"

#include <algorithm>

namespace starfox::assets {

bool HipolyArwings::replaces(std::uint32_t shape) const noexcept {
    return rom && shape <= 0xffffU && std::find(shapes.begin(), shapes.end(), shape) != shapes.end();
}

HipolyArwings build_hipoly_arwings(const RomImage& rom, const SymbolMap& symbols) {
    HipolyArwings result;
    constexpr std::size_t bank_size = 0x8000U;
    const std::size_t bank_offset = std::size_t{hipoly_arwing::bank} * bank_size;
    // The streams need a bank no cartridge data occupies.
    if (rom.size() > bank_offset) return result;

    std::vector<std::uint8_t> bytes = rom.bytes();
    std::vector<std::uint16_t> shapes;
    std::vector<std::pair<std::uint16_t, const hipoly_arwing::Replacement*>> headers;
    for (const auto& replacement : hipoly_arwing::replacements) {
        // Shape headers live in bank $00; the symbol file also lists each
        // name at its section base in the data bank.
        std::uint32_t header = 0U;
        for (const auto address : symbols.find(replacement.shape)) {
            if ((address >> 16U) == 0U && (address & 0xffffU) >= 0x8000U) header = address;
        }
        if (!header) return result;
        headers.emplace_back(static_cast<std::uint16_t>(header), &replacement);
    }

    bytes.resize(bank_offset + bank_size, 0U);
    std::copy(hipoly_arwing::streams.begin(), hipoly_arwing::streams.end(),
        bytes.begin() + static_cast<std::ptrdiff_t>(bank_offset));
    for (const auto& [header, replacement] : headers) {
        const auto offset = rom.lorom_offset(header);
        bytes[offset + 0U] = static_cast<std::uint8_t>(replacement->points);
        bytes[offset + 1U] = static_cast<std::uint8_t>(replacement->points >> 8U);
        bytes[offset + 2U] = hipoly_arwing::bank;
        bytes[offset + 3U] = static_cast<std::uint8_t>(replacement->faces);
        bytes[offset + 4U] = static_cast<std::uint8_t>(replacement->faces >> 8U);
        shapes.push_back(header);
    }
    result.rom = std::make_shared<const RomImage>(std::move(bytes));
    result.shapes = std::move(shapes);
    return result;
}

} // namespace starfox::assets
