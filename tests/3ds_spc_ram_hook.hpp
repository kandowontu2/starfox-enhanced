#pragma once
#include <cstdint>
#include <cstdio>

// Bounded, official core callbacks. The machine differential compares these
// independently of serialized state, including callback time and ordering.
struct SpcRamHooks {
    std::uint64_t digest = 14695981039346656037ULL, opcodes{}, ports{}, dsp{};
    void add(std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) {
            digest ^= (value >> shift) & 255U; digest *= 1099511628211ULL;
        }
    }
    ~SpcRamHooks() {
        std::fprintf(stderr, "hooks opcodes=%llu ports=%llu dsp=%llu digest=%016llx\n",
            static_cast<unsigned long long>(opcodes), static_cast<unsigned long long>(ports),
            static_cast<unsigned long long>(dsp), static_cast<unsigned long long>(digest));
    }
};
inline SpcRamHooks spc_ram_hooks;
#define SPC_CPU_OPCODE_HOOK(address, opcode) do { \
    ++spc_ram_hooks.opcodes; spc_ram_hooks.add(1); \
    spc_ram_hooks.add(static_cast<std::uint32_t>(address)); spc_ram_hooks.add(opcode); \
} while (0)
#define SPC_PORT_WRITE_HOOK(time, port, data, ports_) do { \
    ++spc_ram_hooks.ports; spc_ram_hooks.add(2); spc_ram_hooks.add(time); \
    spc_ram_hooks.add(port); spc_ram_hooks.add(data); \
    for (unsigned i = 0; i < 4; ++i) spc_ram_hooks.add((ports_)[i]); \
} while (0)
#define SPC_DSP_WRITE_HOOK(time, reg, data) do { \
    ++spc_ram_hooks.dsp; spc_ram_hooks.add(3); spc_ram_hooks.add(time); \
    spc_ram_hooks.add(reg); spc_ram_hooks.add(data); \
} while (0)
