// Adapted with permission from Esteban PDN / Starwing 3DS v1.1 (0863e36).
#pragma once

#include "starfox/simulation/wdc65816.hpp"
#include "cpu/65816/cpu_65c816.h"

namespace starfox::simulation::detail {

// Match the pinned RetroCPU SetRegister semantics without its debugger's
// case-insensitive string lookup on every bounded cartridge routine entry.
// In A8/I8 mode the high byte is retained, including the hidden B accumulator.
inline void set_cartridge_call_registers(
    WDC65C816& cpu, const Wdc65816Registers& registers) {
    cpu.SetStatusRegister(registers.status);
    auto& state = cpu.cpu_state;
    if (cpu.mode_long_a) state.regs.a.u16 = registers.a;
    else state.regs.a.u8[0] = static_cast<std::uint8_t>(registers.a);
    if (cpu.mode_long_xy) {
        state.regs.x.u16 = registers.x;
        state.regs.y.u16 = registers.y;
    } else {
        state.regs.x.u8[0] = static_cast<std::uint8_t>(registers.x);
        state.regs.y.u8[0] = static_cast<std::uint8_t>(registers.y);
    }
    if (!cpu.mode_native_6502) {
        state.regs.d.u16 = registers.direct;
        state.data_segment_base = std::uint32_t(registers.data_bank) << 16U;
    }
    state.regs.sp.u16 = cpu.mode_emulation
        ? static_cast<std::uint16_t>(0x100U | (registers.stack & 0xffU))
        : registers.stack;
}

inline void set_cartridge_program_address(WDC65C816& cpu, std::uint32_t address) {
    // Equivalent to SetRegister("pb", address >> 16), followed by "pc".
    if (!cpu.mode_native_6502) cpu.cpu_state.code_segment_base = address & 0xffff0000U;
    cpu.cpu_state.ip = address & 0xffffU;
    if (address > 0xffffU) cpu.cpu_state.code_segment_base = address & 0xff0000U;
}

} // namespace starfox::simulation::detail
