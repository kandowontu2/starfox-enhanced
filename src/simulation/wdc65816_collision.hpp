// Adapted with permission from Esteban PDN / Starwing 3DS v1.1 (0863e36).
#pragma once
#include "cpu/65816/cpu_65c816.h"
#include "cpu/65816/cpu_65c816_instructions.inl"
#include <algorithm>
#include <array>
#include <span>
#include <cstring>
#include <type_traits>
namespace starfox::simulation::detail {
// The three blocks below use the exact opcode implementations from the pinned
// MIT RetroCPU dependency. Only dispatch and memory access are specialized:
// flags (including the core's raw carry/zero representation), cycles, open bus,
// and every executed PC remain identical to its instruction interpreter.
// Instruction bytes are checked before capture and compared at each entry;
// anything outside these concrete source patterns keeps the ordinary path.
template<std::size_t Size>
struct CollisionCodeBlock {
    std::uint32_t address{};
    std::array<std::uint8_t, Size> bytes;

    template<class Match>
    void capture(std::span<const std::uint8_t> ram,
                 std::uint32_t entry, Match match) {
        address = 0U;
        const auto offset = entry & 0xffffU;
        // These are RAMSTUFF routines copied into the upper half of bank $7e.
        // Their recognized stores target direct-page or lower WRAM scratch,
        // so they cannot change their own instructions during a block.
        if ((entry >> 16U) != 0x7eU || offset < 0x4000U
            || offset + Size > 0x10000U || !match(ram, offset)) return;
        std::copy_n(ram.data() + offset, Size, bytes.data());
        address = entry;
    }

    bool matches(std::span<const std::uint8_t> ram,
                 std::uint32_t pc) const noexcept {
        return address != 0U && pc == address
            && std::memcmp(ram.data() + (address & 0xffffU),
                           bytes.data(), Size) == 0;
    }
};

// Inline the pinned bus rules; combine only ordinary, same-page RAM reads.
struct CollisionMemory {
    static std::uint8_t read8(WDC65C816& cpu,std::uint32_t address) {
        auto& bus=*cpu.sys;
        if(!bus.open_bus_is_data)bus.open_bus=address&255U;
        address &= bus.mem_mask;
        const auto& page=bus.memory.pages[address>>bus.memory.page_shift];
        if((page.io_mask&address)==page.io_eq)
            bus.io_devices.read(bus.io_devices.context,address,&bus.open_bus,1);
        else if(page.ptr)bus.open_bus=page.ptr[address&bus.memory.page_mask];
        cpu.cpu_state.cycle += page.cycles_per_access;
        return bus.open_bus;
    }
    static std::uint16_t read16(WDC65C816& cpu,std::uint32_t address) {
        auto& bus=*cpu.sys;
        const auto masked=address&bus.mem_mask;
        const auto& page=bus.memory.pages[masked>>bus.memory.page_shift];
        const auto offset=masked&bus.memory.page_mask;
        if(offset!=bus.memory.page_mask && page.ptr
            &&(page.io_mask&masked)!=page.io_eq
            &&(page.io_mask&(masked+1U))!=page.io_eq) {
            const auto lo=page.ptr[offset],hi=page.ptr[offset+1U];
            bus.open_bus=hi;
            cpu.cpu_state.cycle += std::uint64_t(page.cycles_per_access)*2U;
            return std::uint16_t(lo)|(std::uint16_t(hi)<<8U);
        }
        const auto lo=read8(cpu,address),hi=read8(cpu,address+1U);
        return std::uint16_t(lo)|(std::uint16_t(hi)<<8U);
    }
    static void write8(WDC65C816& cpu,std::uint32_t address,std::uint8_t value) {
        auto& bus=*cpu.sys;address &= bus.mem_mask;
        const auto& page=bus.memory.pages[address>>bus.memory.page_shift];
        if((page.io_mask&address)==page.io_eq)
            bus.io_devices.write(bus.io_devices.context,address,&value,1);
        else if(!(page.flags&Page::kReadOnly)&&page.ptr)
            page.ptr[address&bus.memory.page_mask]=value;
        bus.open_bus=bus.open_bus_is_data?value:address&255U;
        cpu.cpu_state.cycle += page.cycles_per_access;
    }
    static std::uint32_t program(WDC65C816& cpu) {
        return cpu.cpu_state.code_segment_base|((cpu.cpu_state.ip+1U)&0xffffU);
    }
};
template<class Original,int Index=-1,bool DirectMode=false,bool Immediate=false>
struct CollisionAddress : Original {
    static std::uint32_t CalcEffectiveAddress(WDC65C816* cpu) {
        auto& s=cpu->cpu_state;
        if constexpr(Immediate)return CollisionMemory::program(*cpu);
        else if constexpr(DirectMode) {
            if constexpr(Index>=0)cpu->InternalOp();
            const auto offset=CollisionMemory::read8(*cpu,CollisionMemory::program(*cpu));
            if(s.regs.d.u8[0])cpu->InternalOp();
            unsigned sum=offset;
            if constexpr(Index>=0) {
                sum+=s.regs.reglist[Index].u16;
                if(sum>255U)cpu->InternalOp();
            }
            return s.regs.d.u16+sum;
        } else {
            const auto base=CollisionMemory::read16(*cpu,CollisionMemory::program(*cpu));
            unsigned address=base;
            if constexpr(Index>=0) {
                address+=s.regs.reglist[Index].u16;
                if((address^base)&0xff00U)cpu->InternalOp();
            }
            return address+s.data_segment_base;
        }
    }
    template<class T>static void Read(WDC65C816* cpu,T& value) {
        const auto addr=CalcEffectiveAddress(cpu);
        if constexpr(DirectMode) {
            const auto lo=CollisionMemory::read8(*cpu,addr&0xffffU);
            const auto hi=CollisionMemory::read8(*cpu,(addr+1U)&0xffffU);
            value=std::uint16_t(lo)|(std::uint16_t(hi)<<8U);
        } else value=CollisionMemory::read16(*cpu,addr);
    }
    template<class T>static void Write(WDC65C816* cpu,T value) {
        const auto addr=CalcEffectiveAddress(cpu);
        CollisionMemory::write8(*cpu,DirectMode?addr&0xffffU:addr,value);
        CollisionMemory::write8(*cpu,DirectMode?(addr+1U)&0xffffU:addr+1U,value>>8U);
    }
    template<class F>static void Modify(WDC65C816* cpu,F fn) {
        const auto addr=CalcEffectiveAddress(cpu);
        const auto value=fn(CollisionMemory::read16(*cpu,addr));
        CollisionMemory::write8(*cpu,addr,value);
        CollisionMemory::write8(*cpu,addr+1U,value>>8U);
    }
};
struct CollisionDispatch {
    static bool allowed(const WDC65C816& cpu, bool begins_with_rep,
                        std::size_t remaining) noexcept {
        return remaining >= 48U && !cpu.mode_emulation
            && !cpu.mode_native_6502 && cpu.mode_long_xy
            && (cpu.mode_long_a || begins_with_rep)
            && cpu.cpu_state.regs.d.u16 == 0U
            && cpu.cpu_state.data_segment_base == 0x7e0000U
            && !cpu.has_breakpoints && cpu.tracing.addrs.empty()
            && cpu.cpu_state.pending_interrupts.load(std::memory_order_acquire) == 0U
            && (cpu.cpu_state.other_flags & 8U) == 0U;
    }
    using Word = std::uint16_t;
    using Abs = CollisionAddress<AddrAbs<Word,Word>>;
    using AbsX = CollisionAddress<AddrAbsX<Word,Word>,RX>;
    using AbsY = CollisionAddress<AddrAbsY<Word,Word>,RY>;
    using Direct = CollisionAddress<AddrDirect<Word,Word>,-1,true>;
    using DirectX = CollisionAddress<AddrDirectX<Word,Word>,RX,true>;
    using Imm = CollisionAddress<AddrImm<Word,Word,Word>,-1,false,true>;
    template<class Op, class Record>
    static void step(WDC65C816& cpu, Record& record) {
        auto& state=cpu.cpu_state;
        state.ip &= state.ip_mask;
        const auto pc=cpu.program_address();
        record(pc);
        static_cast<void>(CollisionMemory::read8(cpu,pc));
        Op::Exec(&cpu);
        ++cpu.num_emulated_instructions;
    }
    template<class Record> static void axis(WDC65C816& cpu, Record& record) {
        for(unsigned axis=0;axis<3;++axis) {
            const auto begin=cpu.program_address();
            step<LDA<AbsX>>(cpu,record);step<CLC>(cpu,record);
            step<OpAdc<Abs>>(cpu,record);step<STA<Abs>>(cpu,record);
            step<LDA<Direct>>(cpu,record);step<SEC>(cpu,record);
            step<OpSbc<Direct>>(cpu,record);step<BPL>(cpu,record);
            if(cpu.program_address()==begin+17) {
                step<OpXor<Imm>>(cpu,record);step<INA<Word,Word>>(cpu,record);
            }
            step<SEC>(cpu,record);step<OpSbc<Abs>>(cpu,record);
            step<BMI>(cpu,record);
            if(cpu.program_address()==begin+27) {
                step<JMP_ABS>(cpu,record);return;
            }
        }
    }
    static bool axis_matches(std::span<const std::uint8_t> ram,unsigned base) {
        if(base+90>ram.size())return false;
        for(unsigned axis=0;axis<3;++axis) {
            const auto* p=ram.data()+base+axis*30;
            if(p[0]!=0xbd ||p[3]!=0x18||p[4]!=0x6d||p[7]!=0x8d
                ||p[10]!=0xa5||p[12]!=0x38||p[13]!=0xe5
                ||p[15]!=0x10||p[16]!=4||p[17]!=0x49||p[18]!=0xff
                ||p[19]!=0xff||p[20]!=0x1a||p[21]!=0x38
                ||p[22]!=0xed||p[25]!=0x30||p[26]!=3||p[27]!=0x4c
                ||p[9]>=0x20)return false;
        }
        return true;
    }
    template<class Record> static void next(WDC65C816& cpu, Record& record) {
        const auto begin=cpu.program_address();
        step<REP>(cpu,record);step<LDA<Abs>>(cpu,record);step<CLC>(cpu,record);
        step<OpAdc<Imm>>(cpu,record);step<TAY<Word,Word>>(cpu,record);
        step<OpDecMem<Abs>>(cpu,record);step<BEQ>(cpu,record);
        if(cpu.program_address()==begin+15)step<JMP_ABS>(cpu,record);
    }
    static bool next_matches(std::span<const std::uint8_t> ram,unsigned base) {
        if(base+18>ram.size())return false;
        const auto* p=ram.data()+base;
        return p[0]==0xc2&&p[1]==0x20&&p[2]==0xad&&p[5]==0x18
            &&p[6]==0x69&&p[9]==0xa8&&p[10]==0xce&&p[12]<0x20
            &&p[13]==0xf0&&p[14]==3&&p[15]==0x4c;
    }
    // Original's long conditional branches use BRL; EX emits JMP ABS.
    // Both sequences have identical lengths, but keep their own PC/cycle rules.
    static bool filter_matches(std::span<const std::uint8_t> ram,unsigned base) {
        if(base+99>ram.size())return false;
        constexpr std::array<std::pair<unsigned,std::uint8_t>,49> pattern{{
            {0,0xa4},{2,0xb9},{5,0x29},{8,0xf0},{9,3},{10,0x82},
            {13,0xb9},{16,0x35},{18,0x29},{21,0xf0},{22,3},{23,0x82},
            {26,0x98},{27,0xd5},{29,0xd0},{30,3},{31,0x82},
            {34,0x8a},{35,0xbb},{36,0xd5},{38,0xd0},{39,3},{40,0x82},
            {43,0x9b},{44,0xaa},{45,0xb5},{47,0x85},{49,0xb5},{51,0x85},
            {53,0xb5},{55,0x85},{57,0xb5},{59,0x29},{62,0xf0},{63,13},
            {64,0xb9},{67,0x29},{70,0xf0},{71,5},{72,0xb5},{74,0x9b},
            {75,0x80},{76,11},{77,0xb5},{79,0x9b},{80,0xcd},
            {83,0xd0},{84,3},{85,0x82}}};
        for(auto [offset,value]:pattern)if(ram[base+offset]!=value
            && !(value==0x82&&ram[base+offset]==0x4c))return false;
        return ram[base+88]==0xae &&ram[base+91]==0xbd
            &&ram[base+94]==0xd0&&ram[base+95]==3&&(ram[base+96]==0x82||ram[base+96]==0x4c);
    }
    template<class Record> static void jump_out(WDC65C816& cpu, Record& record) {
        const auto pc=cpu.program_address();
        const auto& memory=cpu.sys->memory;
        const auto opcode=memory.pages[pc>>memory.page_shift].ptr[pc&memory.page_mask];
        if(opcode==0x82)step<BRL>(cpu,record);
        else step<JMP_ABS>(cpu,record);
    }
    template<class Record> static void filter(WDC65C816& cpu, Record& record) {
        const auto begin=cpu.program_address();
        step<LDY<Direct>>(cpu,record);step<LDA<AbsY>>(cpu,record);
        step<OpAnd<Imm>>(cpu,record);step<BEQ>(cpu,record);
        if(cpu.program_address()==begin+10){jump_out(cpu,record);return;}
        step<LDA<AbsY>>(cpu,record);step<OpAnd<DirectX>>(cpu,record);
        step<OpAnd<Imm>>(cpu,record);step<BEQ>(cpu,record);
        if(cpu.program_address()==begin+23){jump_out(cpu,record);return;}
        step<TYA<Word,Word>>(cpu,record);step<OpCmp<DirectX>>(cpu,record);
        step<BNE>(cpu,record);
        if(cpu.program_address()==begin+31){jump_out(cpu,record);return;}
        step<TXA<Word,Word>>(cpu,record);step<TYX<Word,Word>>(cpu,record);
        step<OpCmp<DirectX>>(cpu,record);step<BNE>(cpu,record);
        if(cpu.program_address()==begin+40){jump_out(cpu,record);return;}
        step<TXY<Word,Word>>(cpu,record);step<TAX<Word,Word>>(cpu,record);
        step<LDA<DirectX>>(cpu,record);step<STA<Direct>>(cpu,record);
        step<LDA<DirectX>>(cpu,record);step<STA<Direct>>(cpu,record);
        step<LDA<DirectX>>(cpu,record);step<STA<Direct>>(cpu,record);
        step<LDA<DirectX>>(cpu,record);step<OpAnd<Imm>>(cpu,record);
        step<BEQ>(cpu,record);
        if(cpu.program_address()==begin+64) {
            step<LDA<AbsY>>(cpu,record);step<OpAnd<Imm>>(cpu,record);
            step<BEQ>(cpu,record);
            if(cpu.program_address()==begin+72) {
                step<LDA<DirectX>>(cpu,record);step<TXY<Word,Word>>(cpu,record);
                step<BRA>(cpu,record);
            }
        }
        if(cpu.program_address()==begin+77) {
            step<LDA<DirectX>>(cpu,record);step<TXY<Word,Word>>(cpu,record);
            step<OpCmp<Abs>>(cpu,record);step<BNE>(cpu,record);
            if(cpu.program_address()==begin+85){jump_out(cpu,record);return;}
        }
        step<LDX<Abs>>(cpu,record);step<LDA<AbsX>>(cpu,record);step<BNE>(cpu,record);
        if(cpu.program_address()==begin+96)jump_out(cpu,record);
    }
};
}
