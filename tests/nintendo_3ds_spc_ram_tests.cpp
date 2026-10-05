#include "starfox/platform/nintendo_3ds/spc_ram.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
    using namespace starfox::platform::nintendo_3ds;
    std::uint64_t checks = 0;
    const auto check = [&](int address) {
        // Independent interval oracle, not the unsigned optimized predicate.
        const bool ordinary = address >= 0 && (address <= 0xef || address >= 0x100);
        if (spc_plain_ram_read(address) != (ordinary && address <= 0xffff)
            || spc_plain_ram_write(address) != (ordinary && address <= 0xffbf))
            throw std::runtime_error("SPC RAM access classification changed");
        checks += 2;
    };
    for (int address = -0x200; address <= 0x10200; ++address) check(address);
    for (int address : std::array{std::numeric_limits<int>::min(),
            std::numeric_limits<int>::min()+1, std::numeric_limits<int>::max()-1,
            std::numeric_limits<int>::max()}) check(address);
    std::uint32_t random = 0xeac5042bU;
    for (unsigned i = 0; i < 1'048'576; ++i) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        // Avoid implementation-defined unsigned-to-signed conversion.
        const int address = random <= std::uint32_t(std::numeric_limits<int>::max())
            ? int(random) : -1 - int(~random);
        check(address);
    }
    std::cout << "PASS exact SPC ordinary RAM classification: " << checks << " checks\n";
}
