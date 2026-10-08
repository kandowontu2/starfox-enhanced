# Extend only the private, pinned native DSP copy, after the exact saturation
# extension. Counter update/order/state, offsets and rates remain unchanged.
file(READ "${STARFOX_3DS_SPC_DSP_SOURCE}" spc_counter_source)
set(spc_counter_anchor "return ((unsigned) m.counter + counter_offsets [rate]) % counter_rates [rate];")
string(FIND "${spc_counter_source}" "${spc_counter_anchor}" spc_counter_position)
if(spc_counter_position EQUAL -1)
    message(FATAL_ERROR "Pinned native DSP counter remainder needs review")
endif()
string(REPLACE "${spc_counter_anchor}"
    "return starfox::platform::nintendo_3ds::spc_dsp_counter_remainder((unsigned) m.counter + counter_offsets [rate], counter_rates [rate], starfox::platform::nintendo_3ds::spc_dsp_counter_reciprocals[rate]);"
    spc_counter_source "${spc_counter_source}")
string(PREPEND spc_counter_source "#include \"starfox/platform/nintendo_3ds/spc_counters.hpp\"\n")
starfox_write_spc_header("SPC_DSP_3ds_counters.cpp" "${spc_counter_source}")
set(STARFOX_3DS_SPC_DSP_SOURCE "${STARFOX_SPC_SOURCE}/SPC_DSP_3ds_counters.cpp")
