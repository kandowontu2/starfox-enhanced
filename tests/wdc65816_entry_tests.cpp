// Adapted with permission from Esteban PDN / Starwing 3DS v1.1 (0863e36).
#include "../src/simulation/wdc65816_entry.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using starfox::simulation::Wdc65816Registers;
namespace detail = starfox::simulation::detail;

static void reference(WDC65C816& cpu, const Wdc65816Registers& r, std::uint32_t address) {
    cpu.SetRegister("p", r.status);
    cpu.SetRegister("a", r.a);
    cpu.SetRegister("x", r.x);
    cpu.SetRegister("y", r.y);
    cpu.SetRegister("d", r.direct);
    cpu.SetRegister("sp", r.stack);
    cpu.SetRegister("db", r.data_bank);
    cpu.SetRegister("pb", address >> 16U);
    cpu.SetRegister("pc", address);
}
static void optimized(WDC65C816& cpu, const Wdc65816Registers& r, std::uint32_t address) {
    detail::set_cartridge_call_registers(cpu,r);
    detail::set_cartridge_program_address(cpu,address);
}
static void compare(WDC65C816& a, WDC65C816& b) {
    const auto& x=a.cpu_state; const auto& y=b.cpu_state;
    if(a.a()!=b.a() || a.x()!=b.x() || a.y()!=b.y()
        || x.regs.d.u16!=y.regs.d.u16 || x.regs.sp.u16!=y.regs.sp.u16
        || x.code_segment_base!=y.code_segment_base || x.data_segment_base!=y.data_segment_base
        || x.ip!=y.ip || x.zero!=y.zero || x.negative!=y.negative
        || x.carry!=y.carry || x.other_flags!=y.other_flags
        || a.GetStatusRegister()!=b.GetStatusRegister()
        || a.mode_long_a!=b.mode_long_a || a.mode_long_xy!=b.mode_long_xy
        || a.current_instruction_set!=b.current_instruction_set)
        throw std::runtime_error("typed entry differs from pinned RetroCPU register API");
}
int main() try {
    SystemBus bus{}; WDC65C816 baseline{&bus}, candidate{&bus};
    std::uint32_t random=0x98ac321f;
    auto next=[&] { random^=random<<13; random^=random>>17; random^=random<<5; return random; };
    unsigned cases{};
    for(unsigned mode=0;mode<3;++mode) {
        baseline.mode_native_6502=candidate.mode_native_6502=mode==2;
        baseline.mode_emulation=candidate.mode_emulation=mode!=0;
        baseline.OnUpdateMode();candidate.OnUpdateMode();
        for(unsigned iteration=0;iteration<65536;++iteration) {
            Wdc65816Registers r;
            r.status=iteration&255U; r.a=next();r.x=next();r.y=next();
            r.direct=next();r.stack=next();r.data_bank=next();
            const auto address=next() & (iteration%2 ? 0xffffffU : 0xffffU);
            reference(baseline,r,address); optimized(candidate,r,address);compare(baseline,candidate);++cases;
        }
    }
    // Report bridge cost separately from game/console FPS.
    baseline.mode_native_6502=candidate.mode_native_6502=false;
    baseline.mode_emulation=candidate.mode_emulation=false;
    baseline.OnUpdateMode();candidate.OnUpdateMode();
    constexpr unsigned repeats=1000000;
    const auto bench=[&](auto call,WDC65C816& cpu) {
        auto begin=std::chrono::steady_clock::now();
        for(unsigned i=0;i<repeats;++i) {
            Wdc65816Registers r; r.status=(i&1)?0x24:0x04;
            r.a=i;r.x=i+1;r.y=i+2;r.direct=i+3;r.stack=0x1ff;r.data_bank=0x7e;
            call(cpu,r,0x7e8100+(i&255));
        }
        return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    };
    for(unsigned i=0;i<3;++i) {
        const auto before=bench(reference,baseline),after=bench(optimized,candidate);
        compare(baseline,candidate);
        std::cout<<"host bridge baseline_ms="<<before<<" typed_ms="<<after<<" calls="<<repeats<<'\n';
    }
    std::cout<<"PASS "<<cases<<" stateful entries, all 256 statuses, native/emulation/6502, bank-zero/nonzero\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
