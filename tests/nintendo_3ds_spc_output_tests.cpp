#include "starfox/platform/nintendo_3ds/spc_output_checks.hpp"
#include <iostream>
int main() {
    std::cout << "PASS exact SPC voice sample selection: "
        << starfox::platform::nintendo_3ds::check_spc_output() << " checks\n";
}
