#include "starfox/simulation/object_pool.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>

namespace {
using namespace starfox::simulation;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}
template<class F> void rejects(F operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "invalid record/list was accepted");
}
void test_records(ObjectMemoryLayout layout) {
    const std::size_t base_size = layout == ObjectMemoryLayout::starfox_ex ? 57 : 56;
    const std::size_t extended_size = layout == ObjectMemoryLayout::starfox_ex ? 56 : 54;
    std::mt19937 random{0x65816};
    for (unsigned trial = 0; trial < 2048; ++trial) {
        ObjectPool bytewise{3, layout};
        const auto handle = bytewise.allocate_after();
        for (std::uint16_t i = 4; i < base_size; ++i)
            bytewise.write_base_byte(handle, i, static_cast<std::uint8_t>(random()));
        for (std::uint8_t i = 0; i < extended_size; ++i)
            bytewise.write_path_byte(handle, 0x80U + i, static_cast<std::uint8_t>(random()));
        auto& original = bytewise.at(handle);
        original.strategy_address |= std::uint32_t{static_cast<std::uint8_t>(random())} << 24U;
        original.open_al = static_cast<std::uint8_t>(random());
        original.fire_object = static_cast<ObjectHandle>(random());
        original.colour_table = static_cast<std::uint16_t>(random());
        original.sound1 = static_cast<std::uint8_t>(random());
        original.extended[54] = static_cast<std::uint8_t>(random());
        original.extended[55] = static_cast<std::uint8_t>(random());
        ObjectPool bulk = bytewise;

        std::array<std::uint8_t, 59> record;
        record.fill(0xa5);
        bulk.read_base_record(handle, std::span{record}.first(base_size));
        require(std::all_of(record.begin(), record.begin() + 4,
            [](auto byte) { return byte == 0xa5; }), "base export overwrote list pointers");
        require(std::all_of(record.begin() + base_size, record.end(),
            [](auto byte) { return byte == 0xa5; }), "base export overran record");
        for (std::uint16_t i = 4; i < base_size; ++i)
            require(record[i] == bytewise.read_base_byte(handle, i), "base export differs from byte accessor");
        for (auto& byte : record) byte = static_cast<std::uint8_t>(random());
        for (std::uint16_t i = 4; i < base_size; ++i)
            bytewise.write_base_byte(handle, i, record[i]);
        bulk.write_base_record(handle, std::span{record}.first(base_size));
        require(bytewise.save_state() == bulk.save_state(), "base import changed semantic/host fields");
        for (auto& byte : record) byte = static_cast<std::uint8_t>(random());
        for (std::uint8_t i = 0; i < extended_size; ++i)
            bytewise.write_path_byte(handle, 0x80U + i, record[i]);
        bulk.write_extended_record(handle, std::span{record}.first(extended_size));
        require(bytewise.save_state() == bulk.save_state(), "extended import changed unknown/shifted fields");

        if (trial == 0) {
            const auto before = bulk.save_state();
            rejects([&] { bulk.write_base_record(handle, std::span{record}.first(base_size - 1)); });
            rejects([&] { bulk.write_base_record(handle, record); });
            rejects([&] { bulk.write_extended_record(handle, std::span{record}.first(extended_size - 1)); });
            rejects([&] { bulk.write_extended_record(handle, record); });
            rejects([&] { bulk.read_base_record(2, std::span{record}.first(base_size)); });
            require(before == bulk.save_state(), "invalid record mutated pool");
        }
    }
}
void test_lists() {
    ObjectPool pool{8};
    std::array<std::uint64_t, 9> generation{};
    for (unsigned i = 0; i < 4; ++i) {
        const auto handle = pool.allocate_after();
        pool.at(handle).shape = static_cast<std::uint16_t>(100 + handle);
        generation[handle] = pool.generation(handle);
    }
    pool.restore_lists({6, 3, 5, 1}, {8, 4, 7, 2});
    require(pool.active_handles() == std::vector<ObjectHandle>({6, 3, 5, 1}), "active order changed");
    require(pool.free_handles() == std::vector<ObjectHandle>({8, 4, 7, 2}), "free order changed");
    require(pool.generation(1) == generation[1] && pool.generation(3) == generation[3], "retained generation changed");
    require(pool.generation(5) == 5 && pool.generation(6) == 6, "activation generation order changed");
    require(pool.at(1).shape == 101 && pool.at(3).shape == 103, "retained objects cleared");
    require(pool.at(5) == GameObject{} && pool.at(6) == GameObject{}, "free objects gained data");
    const auto saved = pool.save_state();
    pool.restore_lists({6, 3, 5, 1}, {8, 4, 7, 2});
    require(saved == pool.save_state(), "unchanged lists changed generations/state");
    rejects([&] { pool.restore_lists({6, 3, 5, 1}, {8, 4, 7, 6}); });
    rejects([&] { pool.restore_lists({9, 3, 5, 1}, {8, 4, 7, 2}); });
    rejects([&] { pool.restore_lists({0, 3, 5, 1}, {8, 4, 7, 2}); });
    rejects([&] { pool.restore_lists({6, 3, 5, 1}, {8, 4, 7}); });
    require(saved == pool.save_state(), "invalid lists mutated pool");
    const auto reused = pool.allocate_after(3);
    require(reused == 8 && pool.generation(reused) == 7, "free reuse/generation changed");
}
void benchmark() {
    ObjectPool pool{1, ObjectMemoryLayout::starfox_ex};
    const auto handle = pool.allocate_after();
    std::array<std::uint8_t, 57> bytes{};
    std::uint64_t checksum{};
    for (const bool bulk : {false, true, true, false}) {
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned iteration = 0; iteration < 200000; ++iteration) {
            pool.at(handle).count = static_cast<std::uint8_t>(iteration);
            if (bulk) {
                pool.read_base_record(handle, bytes);
                pool.write_base_record(handle, bytes);
                pool.write_extended_record(handle, std::span{bytes}.first(56));
            } else {
                for (std::uint16_t i = 4; i < 57; ++i) bytes[i] = pool.read_base_byte(handle, i);
                for (std::uint16_t i = 4; i < 57; ++i) pool.write_base_byte(handle, i, bytes[i]);
                for (std::uint8_t i = 0; i < 56; ++i) pool.write_path_byte(handle, 0x80U + i, bytes[i]);
            }
            checksum += pool.at(handle).count;
        }
        std::cout << (bulk ? "bulk" : "bytewise") << ','
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count()
            << ',' << checksum << '\n';
    }
}
} // namespace
int main(int argc, char** argv) try {
    test_records(ObjectMemoryLayout::original);
    test_records(ObjectMemoryLayout::starfox_ex);
    test_lists();
    if (argc == 2 && std::string_view{argv[1]} == "--benchmark") benchmark();
    std::cout << "PASS original/EX native object records and list generations\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
