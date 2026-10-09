#include "starfox/platform/nintendo_3ds/spc_saturation_checks.hpp"
#include <iostream>

int main() {
    try {
        std::cout<<"SPC signed saturation: "<<starfox::platform::nintendo_3ds::check_spc_saturation()
            <<" exact input checks (host path; not native speed/audio acceptance)\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
