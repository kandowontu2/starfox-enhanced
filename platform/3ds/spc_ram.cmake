# Only adapt the native graph's private SPC header. Each instruction, cycle,
# address/data evaluation and special-register access remains on its original
# path. Debug echo checks remain; the more-accurate DSP access mode is untouched.
file(READ "${STARFOX_SPC_SOURCE}/SPC_CPU.h" spc_ram_source)
set(spc_ram_anchor [=[#define CPU_READ( time, offset, addr )\
	cpu_read( addr, time + offset )

#define CPU_WRITE( time, offset, addr, data )\
	cpu_write( data, addr, time + offset )]=])
string(FIND "${spc_ram_source}" "${spc_ram_anchor}" position)
if(position EQUAL -1)
    message(FATAL_ERROR "Pinned native SPC RAM access extension needs review")
endif()
set(spc_ram_replacement [=[#if SPC_MORE_ACCURACY || defined(STARFOX_3DS_SPC_RAM_DISABLE)
#define CPU_READ( time, offset, addr )\
	cpu_read( addr, time + offset )
#define CPU_WRITE( time, offset, addr, data )\
	cpu_write( data, addr, time + offset )
#else
// Small inline RAM-only dispatch, not whole-function forced inlining. Keep
// MEM_ACCESS exactly once even in debug builds; fallback owns it otherwise.
#define CPU_READ( time, offset, addr_ ) ([&]() -> int {\
	const int ram_address = (addr_);\
	const rel_time_t ram_time = (time) + (offset);\
	if (starfox::platform::nintendo_3ds::spc_plain_ram_read(ram_address)) {\
		MEM_ACCESS(ram_time, ram_address)\
		return RAM[ram_address];\
	}\
	return cpu_read(ram_address, ram_time);\
}())
#define CPU_WRITE( time, offset, addr_, data_ ) ([&]() {\
	const int ram_address = (addr_);\
	const int ram_data = (data_);\
	const rel_time_t ram_time = (time) + (offset);\
	if (starfox::platform::nintendo_3ds::spc_plain_ram_write(ram_address)) {\
		MEM_ACCESS(ram_time, ram_address)\
		RAM[ram_address] = static_cast<uint8_t>(ram_data);\
	} else {\
		cpu_write(ram_data, ram_address, ram_time);\
	}\
}())
#endif]=])
string(REPLACE "${spc_ram_anchor}" "${spc_ram_replacement}" spc_ram_source "${spc_ram_source}")
string(PREPEND spc_ram_source "#include \"starfox/platform/nintendo_3ds/spc_ram.hpp\"\n")
starfox_write_spc_header("SPC_CPU.h" "${spc_ram_source}")
