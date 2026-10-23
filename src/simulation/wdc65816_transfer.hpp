// Adapted with permission from Esteban PDN / Starwing 3DS v1.1 (0863e36).
#pragma once

#include "wdc65816_collision.hpp"

namespace starfox::simulation::detail {

// These ROM blocks still execute the pinned instruction implementations,
// including individual PPU/APU bus callbacks. Grouping removes the outer
// interpreter dispatch and transfer-request polling between instructions that
// cannot produce either transfer request. The caller retains instruction
// budgets and the complete recent-PC journal.
template<std::size_t Size>
struct TransferCodeBlock {
    std::uint32_t address{};
    std::array<std::uint8_t, Size> bytes{};

    static const std::uint8_t* mapped_rom(const WDC65C816& cpu,
                                         std::uint32_t pc) noexcept {
        const auto& memory = cpu.sys->memory;
        if (pc > cpu.sys->mem_mask) return nullptr;
        const auto& page = memory.pages[pc >> memory.page_shift];
        const auto offset = pc & memory.page_mask;
        if (!page.ptr || !(page.flags & Page::kReadOnly)
            || offset + Size > memory.page_mask + 1U
            // Reject any page with a possible I/O match, including a match
            // strictly between the first and last instruction bytes.
            || (page.io_eq & ~page.io_mask) == 0U)
            return nullptr;
        return page.ptr + offset;
    }

    void capture(const WDC65C816& cpu, std::uint32_t entry,
                 const std::array<std::uint8_t, Size>& expected) noexcept {
        address = 0U;
        const auto* source = mapped_rom(cpu, entry);
        if (entry == 0U || !source
            || std::memcmp(source, expected.data(), Size) != 0) return;
        bytes = expected;
        address = entry;
    }

    bool matches(const WDC65C816& cpu, std::uint32_t pc) const noexcept {
        if (address == 0U || address != pc) return false;
        const auto* source = mapped_rom(cpu, pc);
        return source && std::memcmp(source, bytes.data(), Size) == 0;
    }
};

struct TransferDispatch {
    using Byte = std::uint8_t;
    using Word = std::uint16_t;
    struct ModeCache {
        const WDC65C816::instruction_exec_fn* short_a{};
        const WDC65C816::instruction_exec_fn* long_a{};
    };
    // A captured block is wholly inside one mapped, read-only ROM page.
    // Opcode/operand reads in that block cannot be I/O or cross a page, so
    // retain the ROM value, open bus and access clocks without routing every
    // byte through the general bus decoder. Data and all PPU/APU callbacks
    // still use the ordinary pinned memory rules.
    static Byte read_program8(WDC65C816& cpu, std::uint32_t address) {
        auto& bus = *cpu.sys;
        const auto& page = bus.memory.pages[address >> bus.memory.page_shift];
        bus.open_bus = page.ptr[address & bus.memory.page_mask];
        cpu.cpu_state.cycle += page.cycles_per_access;
        return bus.open_bus;
    }
    static Word read_program16(WDC65C816& cpu, std::uint32_t address) {
        auto& bus = *cpu.sys;
        const auto& page = bus.memory.pages[address >> bus.memory.page_shift];
        const auto* source = page.ptr + (address & bus.memory.page_mask);
        bus.open_bus = source[1];
        cpu.cpu_state.cycle += std::uint64_t(page.cycles_per_access) * 2U;
        return Word(source[0]) | (Word(source[1]) << 8U);
    }
    // The code guard fixes these operands, and D=0 keeps the indirect pointer
    // in ordinary low WRAM. Preserve the pinned bus rules while avoiding the
    // out-of-line generic CPU accessors for each byte of each operand.
    struct AbsByte : AddrAbs<Byte, Word> {
        template<class T> static void Read(WDC65C816* cpu, T& value) {
            const auto address = read_program16(*cpu,
                CollisionMemory::program(*cpu)) + cpu->cpu_state.data_segment_base;
            value = CollisionMemory::read8(*cpu, address);
        }
    };
    template<class Index> struct AbsStore : AddrAbs<Word, Index> {
        template<class T> static void Write(WDC65C816* cpu, T value) {
            const auto address = read_program16(*cpu,
                CollisionMemory::program(*cpu)) + cpu->cpu_state.data_segment_base;
            CollisionMemory::write8(*cpu, address, Byte(value));
            CollisionMemory::write8(*cpu, address + 1U, Byte(value >> 8U));
        }
    };
    using AbsWord = AbsStore<Word>;
    using AbsShortIndexWord = AbsStore<Byte>;
    struct IndirectY : AddrDirectIndirectLongY<Byte, Word> {
        template<class T> static void Read(WDC65C816* cpu, T& value) {
            const auto operand = read_program8(*cpu,
                CollisionMemory::program(*cpu));
            const auto pointer = std::uint16_t(cpu->cpu_state.regs.d.u16 + operand);
            const auto low = CollisionMemory::read16(*cpu, pointer);
            const auto bank = CollisionMemory::read8(*cpu,
                std::uint16_t(pointer + 2U));
            const auto address = (std::uint32_t(bank) << 16U | low)
                + cpu->cpu_state.regs.y.u16;
            value = CollisionMemory::read8(*cpu, address);
        }
    };
    struct BranchNotEqual : BNE {
        static void Exec(WDC65C816* cpu) {
            const auto offset = read_program8(*cpu,
                CollisionMemory::program(*cpu));
            if (BneImpl::DoBranch(cpu)) {
                cpu->InternalOp();
                cpu->cpu_state.ip += GetOffset(offset);
            }
            cpu->cpu_state.ip += kBytes;
        }
    };
    template<bool LongAccumulator> struct AccumulatorMode {
        static void Exec(WDC65C816* cpu, ModeCache& cache) {
            // Exactly REP/SEP #$20, guarded in both complete ROM patterns.
            // Keep the intermediate mode/dispatch table correct, including
            // if an individual APU callback throws before the following SEP.
            static_cast<void>(read_program8(*cpu,
                CollisionMemory::program(*cpu)));
            cpu->cpu_state.ip += 2U;
            if constexpr (LongAccumulator) cpu->cpu_state.other_flags &= ~0x20U;
            else cpu->cpu_state.other_flags |= 0x20U;
            cpu->mode_long_a = LongAccumulator;
            if (!cpu->mode_native_6502 && !cpu->mode_emulation
                && cpu->mode_long_xy) {
                // Only the two guarded native A8/A16, I16 tables are cached.
                // Capture the owning core's actual table once; preserve the
                // intermediate table even if the next I/O callback throws.
                auto& table = LongAccumulator ? cache.long_a : cache.short_a;
                if (table) {
                    cpu->cpu_state.mode = LongAccumulator ? 3 : 2;
                    cpu->current_instruction_set = table;
                } else {
                    cpu->OnUpdateMode();
                    table = cpu->current_instruction_set;
                }
            } else cpu->OnUpdateMode();
        }
    };
    template<bool LongAccumulator, class Record>
    static void mode_step(WDC65C816& cpu, Record& record, ModeCache& cache) {
        cpu.cpu_state.ip &= cpu.cpu_state.ip_mask;
        const auto pc = cpu.program_address();
        record(pc);
        static_cast<void>(read_program8(cpu, pc));
        AccumulatorMode<LongAccumulator>::Exec(&cpu, cache);
        ++cpu.num_emulated_instructions;
    }
    template<class Op, class Record>
    static void step(WDC65C816& cpu, Record& record) {
        auto& state = cpu.cpu_state;
        state.ip &= state.ip_mask;
        const auto pc = cpu.program_address();
        record(pc);
        static_cast<void>(read_program8(cpu, pc));
        Op::Exec(&cpu);
        ++cpu.num_emulated_instructions;
    }

    static bool allowed(const WDC65C816& cpu, bool long_a, bool long_xy,
                        std::size_t remaining, std::size_t block_limit) noexcept {
        return remaining >= block_limit && !cpu.mode_emulation
            && !cpu.mode_native_6502 && cpu.mode_long_a == long_a
            && cpu.mode_long_xy == long_xy && cpu.cpu_state.regs.d.u16 == 0U
            && (cpu.cpu_state.data_segment_base == 0U
                || cpu.cpu_state.data_segment_base == 0x800000U)
            && !cpu.has_breakpoints && cpu.tracing.addrs.empty()
            && cpu.cpu_state.pending_interrupts.load(std::memory_order_acquire) == 0U
            && (cpu.cpu_state.other_flags & 8U) == 0U;
    }

    template<class Record, class Execute>
    static void repeat(WDC65C816& cpu, Record& record,
                       std::size_t remaining, std::size_t block_limit,
                       Execute execute) {
        // Used only by the owning CPU's transfer call after allowed() and a
        // complete ROM guard. These blocks write PPU/APU ports, not transfer
        // flags, instruction pages or their CPU mode/address configuration.
        // Avoid the outer transfer poll/code decoder between successive
        // iterations, while keeping its exact budget and IRQ boundary. The
        // counter/branch exit and SBOOT bank rollover return to that loop.
        const auto begin = cpu.program_address();
        auto bounded_record = [&](std::uint32_t at) {
            record(at);
            --remaining;
        };
        for (unsigned block = 0U; block < 256U; ++block) {
            execute(bounded_record);
            if (cpu.program_address() != begin || remaining < block_limit
                || cpu.cpu_state.pending_interrupts.load(
                    std::memory_order_acquire) != 0U) break;
        }
    }

    static constexpr std::array<std::uint8_t, 6> word_fill_bytes{
        0x8d, 0x18, 0x21, 0xca, 0xd0, 0xfa};
    static constexpr std::array<std::uint8_t, 12> short_index_fill_bytes{
        0x8d, 0x18, 0x21, 0x8d, 0x19, 0x21, 0x88, 0xd0, 0xf7,
        0xca, 0xd0, 0xf2};

    static std::array<std::uint8_t, 21> boot_bytes(std::uint8_t pointer) {
        return {0xeb,0xb7,pointer,0xc8,0xeb,0xcd,0x40,0x21,0xd0,0xfb,
            0x1a,0xc2,0x20,0x8d,0x40,0x21,0xe2,0x20,0xca,0xd0,0xeb};
    }
    static std::array<std::uint8_t, 32> sboot_bytes(std::uint8_t pointer) {
        return {0xeb,0xb7,pointer,0xc8,0xd0,0x09,0xe6,Byte(pointer+2U),
            0x64,Byte(pointer+1U),0x64,pointer,0xa0,0x00,0x80,0xeb,
            0xcd,0x40,0x21,0xd0,0xfb,0x1a,0xc2,0x20,0x8d,0x40,0x21,
            0xe2,0x20,0xca,0xd0,0xe0};
    }

    template<class Record>
    static void word_fill(WDC65C816& cpu, Record& record) {
        step<STA<AbsWord>>(cpu, record);
        step<DEX<Word, Word>>(cpu, record);
        step<BranchNotEqual>(cpu, record);
    }

    template<class Record>
    static void short_index_fill(WDC65C816& cpu, Record& record) {
        const auto begin = cpu.program_address();
        step<STA<AbsShortIndexWord>>(cpu, record);
        step<STA<AbsShortIndexWord>>(cpu, record);
        step<DEY<Word, Byte>>(cpu, record);
        step<BranchNotEqual>(cpu, record);
        if (cpu.program_address() == begin) return;
        step<DEX<Word, Byte>>(cpu, record);
        step<BranchNotEqual>(cpu, record);
    }

    template<bool BankCrossing, class Record>
    static void upload(WDC65C816& cpu, Record& record, ModeCache& cache) {
        const auto begin = cpu.program_address();
        step<XBA>(cpu, record);
        step<LDA<IndirectY>>(cpu, record);
        step<INY<Byte, Word>>(cpu, record);
        if constexpr (BankCrossing) {
            step<BranchNotEqual>(cpu, record);
            // The rare address rollover rejoins the interpreter at INC.
            if (cpu.program_address() == begin + 6U) return;
        }
        step<XBA>(cpu, record);
        const auto wait = cpu.program_address();
        step<OpCmp<AbsByte>>(cpu, record);
        step<BranchNotEqual>(cpu, record);
        // A non-acknowledging APU retains the original polling/budget path.
        if (cpu.program_address() == wait) return;
        step<INA<Byte, Word>>(cpu, record);
        mode_step<true>(cpu, record, cache);
        step<STA<AbsWord>>(cpu, record);
        mode_step<false>(cpu, record, cache);
        step<DEX<Byte, Word>>(cpu, record);
        step<BranchNotEqual>(cpu, record);
    }
};
} // namespace starfox::simulation::detail
