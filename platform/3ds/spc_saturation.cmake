# Adapt only the private, pinned native DSP copy. Keep every arithmetic,
# temporal/state/voice/filter operation outside CLAMP16 byte-for-byte intact.
file(READ "${STARFOX_SPC_SOURCE}/SPC_DSP.cpp" spc_saturation_source)
string(REPLACE "\r\n" "\n" spc_saturation_source "${spc_saturation_source}")
set(spc_clamp_anchor [=[#define CLAMP16( io )\
{\
	if ( (int16_t) io != io )\
		io = (io >> 31) ^ 0x7FFF;\
}]=])
string(FIND "${spc_saturation_source}" "${spc_clamp_anchor}" spc_clamp_position)
if(spc_clamp_position EQUAL -1)
    message(FATAL_ERROR "Pinned native SPC DSP saturation needs review")
endif()
string(REPLACE "${spc_clamp_anchor}"
    "#define CLAMP16( io ) { io = starfox::platform::nintendo_3ds::spc_saturate16(io); }"
    spc_saturation_source "${spc_saturation_source}")
string(PREPEND spc_saturation_source "#include \"starfox/platform/nintendo_3ds/spc_saturation.hpp\"\n")
starfox_write_spc_header("SPC_DSP_3ds_saturation.cpp" "${spc_saturation_source}")
set(STARFOX_3DS_SPC_DSP_SOURCE "${STARFOX_SPC_SOURCE}/SPC_DSP_3ds_saturation.cpp")
