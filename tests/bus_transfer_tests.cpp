#include "../src/simulation/owned_bus_transfer.hpp"
#include "starfox/simulation/wdc65816.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace starfox::simulation;
std::uint64_t checks{};
void require(bool value, const char* message) {
    ++checks; if (!value) throw std::runtime_error(message);
}
struct Access {
    std::uint32_t address;
    std::uint8_t before, value;
    bool write;
    bool operator==(const Access&) const = default;
};
struct Machine {
    std::array<std::uint8_t, 4096> memory{};
    std::array<Page, 64> pages{};
    SystemBus bus{};
    std::vector<Access> accesses;
    unsigned variant{}, io_count{};
    Machine(unsigned kind, bool data) : variant(kind) {
        for (unsigned i = 0; i < memory.size(); ++i) memory[i] = std::uint8_t(i * 71U + kind);
        for (unsigned i = 0; i < pages.size(); ++i) {
            pages[i] = {memory.data() + i * 64, 0, 0, 1, i % 5 + 1};
            if (kind == 1 && i % 5 == 0) pages[i].ptr = nullptr;
            if (kind == 2 && i % 3 == 0) pages[i].flags = Page::kReadOnly;
            if (kind == 3) { pages[i].io_mask = 0x18; pages[i].io_eq = 0x10; }
            if (kind == 4 || kind == 5) if (i % 4 == 0) pages[i].io_eq = 0;
            if (kind == 6) {
                pages[i].ptr = i % 5 ? memory.data() + ((i * 5) % 64) * 64 : nullptr;
                pages[i].flags = i % 3 ? 0 : Page::kReadOnly;
                pages[i].io_mask = 0x30; pages[i].io_eq = 0x20;
            }
        }
        bus.Init(6, 12, pages.data()); bus.open_bus_is_data = data; bus.open_bus = 0x69;
        bus.io_devices = {&is_io, &read, &write, nullptr, this};
    }
    static bool is_io(void*, cpuaddr_t) { return true; }
    void side_effect(std::uint32_t address) {
        ++io_count;
        if (variant == 5) {
            // A callback can change the following page's map/readonly/IO mask.
            auto& page = pages[((address >> 6) + 1) % pages.size()];
            page.ptr = memory.data() + ((io_count * 3) % 64) * 64;
            page.flags = io_count % 2 ? Page::kReadOnly : 0;
            page.io_mask = 0; page.io_eq = io_count % 3 ? 1 : 0;
        }
    }
    static void read(void* context, cpuaddr_t address, std::uint8_t* value, std::uint32_t count) {
        require(count == 1, "read callback batched or reordered");
        auto& self = *static_cast<Machine*>(context);
        const auto before = *value;
        *value = std::uint8_t(*value ^ self.memory[address] ^ self.io_count);
        self.accesses.push_back({address, before, *value, false}); self.side_effect(address);
    }
    static void write(void* context, cpuaddr_t address, const std::uint8_t* value, std::uint32_t count) {
        require(count == 1, "write callback batched or reordered");
        auto& self = *static_cast<Machine*>(context);
        self.accesses.push_back({address, self.bus.open_bus, *value, true});
        self.memory[address] = *value; self.side_effect(address);
    }
};
void equal(const Machine& a, const Machine& b) {
    require(a.memory == b.memory, "mapped bytes or alias order changed");
    require(a.bus.open_bus == b.bus.open_bus && a.bus.open_bus_is_data == b.bus.open_bus_is_data,
        "open-bus state changed");
    require(a.accesses == b.accesses && a.io_count == b.io_count, "I/O trace changed");
    for (unsigned i = 0; i < a.pages.size(); ++i) {
        const auto& left = a.pages[i]; const auto& right = b.pages[i];
        require(bool(left.ptr) == bool(right.ptr) && left.flags == right.flags
            && left.io_mask == right.io_mask && left.io_eq == right.io_eq
            && left.cycles_per_access == right.cycles_per_access, "page metadata changed");
        if (left.ptr) require(left.ptr - a.memory.data() == right.ptr - b.memory.data(), "page target changed");
    }
}
void byte_oracle() {
    constexpr std::array<std::uint32_t, 14> addresses{0, 1, 31, 56, 63, 64, 255,
        1021, 4065, 4095, 4096, 0x10000, 0xfffffffa, 0xffffffff};
    constexpr std::array<unsigned, 9> lengths{0, 1, 5, 56, 57, 64, 65, 130, 4097};
    for (unsigned variant = 0; variant < 7; ++variant) for (bool data : {false, true})
        for (auto address : addresses) for (auto length : lengths) {
            Machine old(variant, data), current(variant, data);
            std::vector<std::uint8_t> input(length), left(length + 16, 0xa7), right = left;
            for (unsigned i = 0; i < length; ++i) input[i] = std::uint8_t(i * 39 + length);
            for (unsigned i = 0; i < length; ++i) old.bus.WriteByte(address + i, input[i]);
            detail::write_bus_bytes(current.bus, address, input); equal(old, current);
            for (unsigned i = 0; i < length; ++i) left[i + 8] = old.bus.ReadByte(address + i);
            detail::read_bus_bytes(current.bus, address, std::span{right}.subspan(8, length));
            require(left == right, "read result or buffer guards changed"); equal(old, current);
        }
    for (bool data : {false, true}) for (int shift = -17; shift <= 17; ++shift) {
        Machine old(0, data), current(0, data);
        auto left = std::span{old.memory}.subspan(unsigned(160 + shift), 96);
        auto right = std::span{current.memory}.subspan(unsigned(160 + shift), 96);
        for (unsigned i = 0; i < left.size(); ++i) left[i] = old.bus.ReadByte(160 + i);
        detail::read_bus_bytes(current.bus, 160, right); equal(old, current);
        for (unsigned i = 0; i < left.size(); ++i) old.bus.WriteByte(160 + i, left[i]);
        detail::write_bus_bytes(current.bus, 160, right); equal(old, current);
    }
}
void adapter_oracle() {
    const starfox::assets::RomImage rom(std::vector<std::uint8_t>(0x20000, 0xea));
    Wdc65816 old(rom), current(rom);
    const auto transfer = [&](std::uint32_t address, unsigned length) {
        std::vector<std::uint8_t> input(length), left(length + 16, 0x4f), right = left;
        for (unsigned i = 0; i < length; ++i) input[i] = std::uint8_t(i * 73 + address);
        for (unsigned i = 0; i < length; ++i) old.write8(address + i, input[i]);
        current.write_bytes(address, input);
        require(old.save_state() == current.save_state(), "adapter write complete state changed");
        for (unsigned i = 0; i < length; ++i) left[i + 8] = old.read8(address + i);
        current.read_bytes(address, std::span{right}.subspan(8, length));
        require(left == right && old.save_state() == current.save_state(), "adapter read state/bytes changed");
        require(old.take_apu_port_writes() == current.take_apu_port_writes(), "APU callback queue changed");
        require(old.take_msu_register_writes() == current.take_msu_register_writes(), "MSU callback queue changed");
    };
    for (auto address : {0U, 0x1ffcU, 0x6000U, 0x7e0ffaU, 0x7efffdU, 0x7ffffaU,
            0x800000U, 0x808000U, 0x700ffaU, 0x71fff0U, 0xffffffU, 0xfffffff8U})
        for (unsigned length : {0U, 1U, 57U, 130U}) transfer(address, length);
    for (auto address : {0x2110U, 0x213aU, 0x2140U, 0x802140U, 0x4300U}) transfer(address, 6);
}
void benchmark() {
    std::array<std::uint8_t, 57> bytes{}; for (unsigned i = 0; i < bytes.size(); ++i) bytes[i] = i;
    for (bool bulk : {false, true, true, false}) {
        Machine machine(0, true);
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned record = 0; record < 200000; ++record) {
            if (bulk) {
                detail::write_bus_bytes(machine.bus, 512, bytes);
                detail::read_bus_bytes(machine.bus, 512, bytes);
            } else {
                for (unsigned i = 0; i < bytes.size(); ++i) machine.bus.WriteByte(512 + i, bytes[i]);
                for (unsigned i = 0; i < bytes.size(); ++i) bytes[i] = machine.bus.ReadByte(512 + i);
            }
        }
        require(machine.bus.open_bus == bytes.back() && machine.memory[512] == bytes.front(), "benchmark lost bus effects");
        std::cout << (bulk ? "page" : "byte") << " ms="
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() << '\n';
    }
}
} // namespace
int main(int argc, char** argv) try {
    if (argc == 2 && std::string_view(argv[1]) == "--benchmark") { benchmark(); return 0; }
    byte_oracle(); adapter_oracle();
    std::cout << "PASS " << checks << " pinned-bus byte/page/IO/alias/wrap/complete-adapter-state checks\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
