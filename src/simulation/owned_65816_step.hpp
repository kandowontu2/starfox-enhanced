#pragma once

#include "cpu/65816/cpu_65c816.h"

namespace starfox::simulation::detail {

// The cartridge adapter owns this concrete CPU and does not use an external
// EventQueue. Keep SingleStep's exact instruction/interrupt ordering, but avoid
// virtual dispatch and an empty breakpoint-table lookup for every instruction.
// Retain the generic path whenever debugging callbacks are actually installed.
inline void step_owned_65816(WDC65C816& cpu) {
    if (!cpu.breakpoints.empty()) {
        cpu.SingleStep();
        return;
    }
    auto& state = cpu.cpu_state;
    state.ip &= state.ip_mask;
    const auto pending = state.pending_interrupts.load(std::memory_order_acquire);
    if (pending + state.interrupts >= 3U) {
        WDC65C816::Interrupt(&cpu, (pending & 4U) != 0U ? 1U : 0U);
        return;
    }
    WDC65C816::EmulateInstruction(&cpu);
}

} // namespace starfox::simulation::detail
