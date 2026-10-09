// Read-only diagnostic bridge linked with an unmodified local Snes9x build.
// No instruction, interrupt, timing, RAM or cartridge state is changed here.
#include "snes9x.h"

extern "C" __declspec(dllexport) uint32 retro_debug_cpu_word(unsigned field) {
    switch(field) {
    case 0: return Registers.PBPC & 0xffffffU;
    case 1: return Registers.A.W;
    case 2: return Registers.X.W;
    case 3: return Registers.Y.W;
    case 4: return Registers.P.W;
    case 5: return Registers.S.W;
    case 6: return CPU.NMIPending;
    case 7: return CPU.IRQLine;
    case 8: return CPU.WaitingForInterrupt;
    case 9: return uint32(CPU.V_Counter);
    case 10: return CPU.Flags;
    default: return 0xffffffffU;
    }
}
