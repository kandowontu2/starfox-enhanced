// Asset-free ARM arithmetic execution check. No cartridge, DSP service or PCM
// output substitute. The game still runs both complete accurate SPC stems.
#include "native_display.hpp"
#include "starfox/platform/nintendo_3ds/spc_saturation_checks.hpp"
#include "starfox/platform/nintendo_3ds/spc_counter_checks.hpp"
#include "starfox/platform/nintendo_3ds/spc_output_checks.hpp"
#include <3ds.h>
#include <fstream>

int main() {
    using namespace starfox::platform::nintendo_3ds;
    NativeDisplay display;
    Canvas upper(top_width),lower;
    upper.clear({8,15,28});upper.text(20,80,"CHECKING NATIVE SPC MATH...",{240,181,86},2);
    lower.clear({8,15,28});lower.text(12,24,"ASSET-FREE INTEGER TEST\nNO GAME OR DSP SERVICE",{183,224,240});
    display.present(plan_frame(0,false,ScreenUse::setup),upper.view(),{},lower.view());
    const auto began=svcGetSystemTick();
    std::string result;
    try {
#if defined(STARFOX_3DS_OUTPUT_CHECK)
        const auto count=check_spc_output();
#elif defined(STARFOX_3DS_COUNTER_CHECK)
        const auto count=check_spc_counters();
#else
        const auto count=check_spc_saturation();
#endif
        result="PASS / "+std::to_string(count)+" EXACT INPUTS";
    } catch(const std::exception& error) {result=std::string("FAIL / ")+error.what();}
    const auto elapsed=svcGetSystemTick()-began;
    // A uniquely named, small bounded report, never a settings/save overwrite.
#if defined(STARFOX_3DS_OUTPUT_CHECK)
    constexpr auto reportPrefix="sdmc:/starfox-spc-output-";
    constexpr auto reportTitle="Original 3DS native voice sample selection check";
#elif defined(STARFOX_3DS_COUNTER_CHECK)
    constexpr auto reportPrefix="sdmc:/starfox-spc-counters-";
    constexpr auto reportTitle="Original 3DS native DSP counter remainder check";
#else
    constexpr auto reportPrefix="sdmc:/starfox-spc-saturation-";
    constexpr auto reportTitle="Original 3DS native signed-saturation check";
#endif
    std::ofstream report(reportPrefix+std::to_string(began)+".txt");
    if(report) {
        report<<reportTitle<<"\n"<<result<<"\nclock_hz="<<SYSCLOCK_ARM11
            <<"\nelapsed_ticks="<<elapsed
            <<"\nscope=ARM arithmetic execution; not physical FPS, streaming audio or whole-game acceptance\n";
        report.flush();
    }
    upper.clear({8,15,28});upper.text(20,28,"NATIVE SPC INTEGER CHECK",{183,224,240},2);
    upper.text(20,78,result,{227,235,242},1,360,108);
    lower.text(12,78,report?"REPORT WRITTEN TO SD ROOT":"NO SD REPORT; SEE TOP SCREEN",{227,235,242});
    lower.text(12,126,"ARITHMETIC ONLY, NOT GAME FPS\nSELECT + START: EXIT",{183,224,240});
    while(true) {
        const auto input=display.poll();
        if(!input.running || (input.held&(starfox::input::select|starfox::input::start))
            ==(starfox::input::select|starfox::input::start)) break;
        display.present(plan_frame(0,false,ScreenUse::setup),upper.view(),{},lower.view());
    }
}
