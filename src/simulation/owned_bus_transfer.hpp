#pragma once

#include "cpu.h"

#include <algorithm>
#include <cstdint>
#include <span>

namespace starfox::simulation::detail {

// Adapter record transfers, not CPU instructions: the pinned SystemBus byte
// functions return an access cost which Wdc65816::read8/write8 do not charge.
// Never bypass an I/O mask that can change within the page. Such accesses and
// unmapped holes keep the exact byte callback/order/open-bus behavior.
inline void read_bus_bytes(SystemBus& bus, std::uint32_t address,
    std::span<std::uint8_t> output) {
    while (!output.empty()) {
        const auto masked = address & bus.mem_mask;
        const auto& page = bus.memory.pages[masked >> bus.memory.page_shift];
        if (!page.ptr || (page.io_mask & bus.memory.page_mask)
            || (page.io_mask & masked) == page.io_eq) {
            output.front() = bus.ReadByte(address);
            ++address; output = output.subspan(1);
            continue;
        }
        const auto offset = masked & bus.memory.page_mask;
        const auto count = std::min(output.size(), std::size_t(bus.memory.page_size - offset));
        // Use sequential byte semantics, not memcpy/memmove: a caller's
        // buffer may alias mapped memory (including a later source byte).
        for (std::size_t i = 0; i < count; ++i) {
            bus.open_bus = page.ptr[offset + i];
            output[i] = bus.open_bus;
        }
        address += static_cast<std::uint32_t>(count);
        output = output.subspan(count);
    }
}

inline void write_bus_bytes(SystemBus& bus, std::uint32_t address,
    std::span<const std::uint8_t> input) {
    while (!input.empty()) {
        const auto masked = address & bus.mem_mask;
        const auto& page = bus.memory.pages[masked >> bus.memory.page_shift];
        if (!page.ptr || (page.io_mask & bus.memory.page_mask)
            || (page.io_mask & masked) == page.io_eq) {
            bus.WriteByte(address, input.front());
            ++address; input = input.subspan(1);
            continue;
        }
        const auto offset = masked & bus.memory.page_mask;
        const auto count = std::min(input.size(), std::size_t(bus.memory.page_size - offset));
        for (std::size_t i = 0; i < count; ++i) {
            const auto value = input[i];
            if (!(page.flags & Page::kReadOnly)) page.ptr[offset + i] = value;
            bus.open_bus = bus.open_bus_is_data ? value : std::uint8_t(masked + i);
        }
        address += static_cast<std::uint32_t>(count);
        input = input.subspan(count);
    }
}

} // namespace starfox::simulation::detail
