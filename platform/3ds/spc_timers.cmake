# Adapt only the private native SPC CPU copy. The pinned upstream DSP, CPU
# instruction ordering, timer state, tempo API and LGPL notices stay intact.
file(READ "${STARFOX_SPC_SOURCE}/SNES_SPC.cpp" spc_timer_source)
set(timer_prescaler_anchor "#define TIMER_DIV( t, n ) ((n) / t->prescaler)")
set(timer_period_anchor "int n = over / t->period;")
foreach(anchor IN ITEMS timer_prescaler_anchor timer_period_anchor)
    string(FIND "${spc_timer_source}" "${${anchor}}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Pinned native SPC timer division extension needs review: ${anchor}")
    endif()
endforeach()
string(REPLACE "${timer_prescaler_anchor}"
    "#define TIMER_DIV( t, n ) starfox::platform::nintendo_3ds::spc_prescaler_divide((n), t->prescaler)"
    spc_timer_source "${spc_timer_source}")
string(REPLACE "${timer_period_anchor}"
    "int n = starfox::platform::nintendo_3ds::spc_period_divide(over, t->period);"
    spc_timer_source "${spc_timer_source}")
string(PREPEND spc_timer_source "#include \"starfox/platform/nintendo_3ds/spc_timers.hpp\"\n")
starfox_write_spc_header("SNES_SPC_3ds_timers.cpp" "${spc_timer_source}")
set(STARFOX_3DS_SPC_TIMERS_SOURCE "${STARFOX_SPC_SOURCE}/SNES_SPC_3ds_timers.cpp")
