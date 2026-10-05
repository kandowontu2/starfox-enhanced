# Extend only the final private native DSP copy. The unused Gaussian result is
# pure; every phase, decoding/envelope/noise/state operation remains pinned.
file(READ "${STARFOX_3DS_SPC_DSP_SOURCE}" spc_output_source)
string(CONCAT spc_output_anchor
    "\t\tint output = interpolate( v );\n"
    "\t\t\n"
    "\t\t// Noise\n"
    "\t\tif ( m.t_non & v->vbit )\n"
    "\t\t\toutput = (int16_t) (m.noise * 2);")
set(spc_output_replacement [=[		int output = starfox::platform::nintendo_3ds::spc_voice_sample(
			v->env, (m.t_non & v->vbit) != 0, m.noise,
			[&]() { return interpolate( v ); });]=])
string(FIND "${spc_output_source}" "${spc_output_anchor}" position)
if(position EQUAL -1)
    message(FATAL_ERROR "Pinned native DSP output selection needs review")
endif()
string(REPLACE "${spc_output_anchor}" "${spc_output_replacement}" spc_output_source "${spc_output_source}")
string(PREPEND spc_output_source "#include \"starfox/platform/nintendo_3ds/spc_output.hpp\"\n")
starfox_write_spc_header("SPC_DSP_3ds_output.cpp" "${spc_output_source}")
set(STARFOX_3DS_SPC_DSP_SOURCE "${STARFOX_SPC_SOURCE}/SPC_DSP_3ds_output.cpp")
