#include "starfox/render/native_metal_rt_programs.hpp"
#include <cstdio>
#include <stdexcept>

// Deliberately fake read-only bytes: this checks the real provider's selection
// and rejection contract, NOT a native Metal library or GPU execution.
extern "C" {
#if defined(TEST_BAD_PAYLOAD)
extern const std::uint8_t starfox_native_metal_shadow[]={0,0,0,0,11,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
extern const std::uint64_t starfox_native_metal_reflection_size=19;
extern const std::uint64_t starfox_native_metal_empty_shadow_size=1U<<30;
#else
extern const std::uint8_t starfox_native_metal_shadow[]={'M','T','L','B',11,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
extern const std::uint64_t starfox_native_metal_reflection_size=20;
extern const std::uint64_t starfox_native_metal_empty_shadow_size=20;
#endif
extern const std::uint64_t starfox_native_metal_shadow_size=20;
extern const std::uint8_t starfox_native_metal_reflection[]={'M','T','L','B',12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
extern const std::uint8_t starfox_native_metal_empty_shadow[]={'M','T','L','B',13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
extern const std::uint8_t starfox_native_metal_empty_reflection[]={'M','T','L','B',14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
extern const std::uint64_t starfox_native_metal_empty_reflection_size=20;
}
int main() {
    using namespace starfox::render::shadows;
    unsigned checks=0;
    for(unsigned value=0;value<4;++value) {
        const auto bytes=native_metal_rt_program(static_cast<NativeMetalRtProgram>(value));
#if defined(TEST_BAD_PAYLOAD)
        if(value<3) {if(!bytes.empty()) throw std::runtime_error("Invalid provider payload accepted");++checks;continue;}
#endif
        if(bytes.size()!=20 || bytes[4]!=11+value) throw std::runtime_error("Actual provider selected wrong role/extent");
        const std::uint8_t* expected[]={starfox_native_metal_shadow,starfox_native_metal_reflection,
            starfox_native_metal_empty_shadow,starfox_native_metal_empty_reflection};
        if(bytes.data()!=expected[value]) throw std::runtime_error("Actual provider copied/replaced native bytes");
        checks+=2;
    }
    for(unsigned value:{4U,99U}) {
        if(!native_metal_rt_program(static_cast<NativeMetalRtProgram>(value)).empty())
            throw std::runtime_error("Unknown provider role accepted");
        ++checks;
    }
    std::printf("PASS: %u actual provider contract checks; fake bytes, no Metal/GPU acceptance\n",checks);
}
