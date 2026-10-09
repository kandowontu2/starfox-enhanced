#include "starfox/platform/nintendo_3ds/spc_counter_checks.hpp"
#include <iostream>

int main() try {
    const auto checks = starfox::platform::nintendo_3ds::check_spc_counters();
    std::cout << "Exact SPC DSP envelope/noise counter remainders: " << checks << " checks PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
