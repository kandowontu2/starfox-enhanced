#include "../src/simulation/owned_65816_step.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error{message};
}

struct Access {
    std::uint32_t address;
    std::uint8_t value;
    bool write;
    bool operator==(const Access&) const = default;
};

struct Machine {
    std::array<std::uint8_t, 65536> memory{};
    std::array<Page, 4096> pages{};
    SystemBus bus{};
    WDC65C816 cpu{&bus};
    std::vector<Access> accesses;
    std::vector<std::uint32_t> interrupts;
    unsigned breakpoint_calls{};
    bool record_accesses{true};

    explicit Machine(bool trace_bus = true) {
        for (std::size_t index = 0; index < pages.size(); ++index) {
            pages[index] = Page{memory.data() + (index % 16U) * 4096U,
                0U, 0U, trace_bus ? 0U : 1U,
                static_cast<std::uint32_t>(index % 3U + 1U)};
        }
        bus.Init(12U, 24U, pages.data());
        bus.io_devices = {&is_io, &read, &write, &irq, this};
        cpu.tracing.addrs.resize(16U);
    }

    static bool is_io(void*, cpuaddr_t) { return true; }
    static void read(void* context, cpuaddr_t address, std::uint8_t* value,
        std::uint32_t size) {
        auto& self = *static_cast<Machine*>(context);
        require(size == 1U, "unexpected bus read size");
        *value = self.memory[address & 0xffffU];
        if (self.record_accesses) self.accesses.push_back({address, *value, false});
    }
    static void write(void* context, cpuaddr_t address,
        const std::uint8_t* value, std::uint32_t size) {
        auto& self = *static_cast<Machine*>(context);
        require(size == 1U, "unexpected bus write size");
        self.memory[address & 0xffffU] = *value;
        if (self.record_accesses) self.accesses.push_back({address, *value, true});
    }
    static void irq(void* context, std::uint32_t type) {
        auto& self = *static_cast<Machine*>(context);
        self.interrupts.push_back(type);
        self.cpu.cpu_state.ClearInterruptSource(type);
    }

    void reset(unsigned mode, unsigned status, std::uint32_t ip) {
        memory.fill(0xeaU);
        // Deterministic reset/interrupt vectors and operands across PC wrap.
        for (std::size_t address = 0xffe0U; address != 0x10000U; ++address)
            memory[address] = static_cast<std::uint8_t>(address & 1U ? 0x70U : 0x00U);
        cpu.PowerOn();
        cpu.mode_emulation = mode == 4U;
        cpu.mode_native_6502 = false;
        const auto widths = mode == 4U ? 0x30U
            : (mode & 1U ? 0U : 0x20U) | (mode & 2U ? 0U : 0x10U);
        cpu.SetStatusRegister(static_cast<std::uint8_t>(widths | status));
        cpu.OnUpdateMode();
        cpu.cpu_state.regs.a.u16 = 3U; // bounded MVN/MVP work
        cpu.cpu_state.regs.x.u16 = 0x32U;
        cpu.cpu_state.regs.y.u16 = 0x74U;
        cpu.cpu_state.regs.d.u16 = 0x1200U;
        cpu.cpu_state.regs.sp.u16 = mode == 4U ? 0x1feU : 0x2feU;
        cpu.cpu_state.code_segment_base = 0x20000U;
        cpu.cpu_state.data_segment_base = 0x30000U;
        cpu.cpu_state.ip = ip;
        cpu.cpu_state.ip_mask = 0xffffU;
        cpu.cpu_state.cycle = 17U;
        cpu.cpu_state.event_cycle = 101U;
        cpu.cpu_state.cycle_stop = 10000U;
        cpu.cpu_state.pending_interrupts.store(0U);
        cpu.num_emulated_instructions = 0U;
        cpu.tracing.write = 0U;
        std::fill(cpu.tracing.addrs.begin(), cpu.tracing.addrs.end(), 0U);
        cpu.breakpoints.clear();
        cpu.has_breakpoints = false;
        bus.open_bus = 0x12U;
        accesses.clear();
        interrupts.clear();
        breakpoint_calls = 0U;
    }

    void instruction(std::uint8_t opcode) {
        const auto pc = cpu.cpu_state.ip & cpu.cpu_state.ip_mask;
        memory[pc] = opcode;
        memory[(pc + 1U) & 0xffffU] = 0x12U;
        memory[(pc + 2U) & 0xffffU] = 0x03U;
        memory[(pc + 3U) & 0xffffU] = 0x02U;
    }
};

auto state(const Machine& machine) {
    const auto& cpu = machine.cpu;
    const auto& s = cpu.cpu_state;
    return std::tuple{s.regs.a.u16, s.regs.x.u16, s.regs.y.u16, s.regs.d.u16,
        s.regs.sp.u16, s.data_segment_base, s.code_segment_base, s.ip, s.ip_mask,
        s.cycle, s.cycle_stop, s.event_cycle, s.mode, s.zero, s.negative,
        s.interrupts, s.pending_interrupts.load(), s.carry, s.other_flags,
        cpu.mode_native_6502, cpu.mode_emulation, cpu.mode_long_a, cpu.mode_long_xy,
        cpu.num_emulated_instructions, cpu.tracing.write, machine.bus.open_bus,
        machine.breakpoint_calls};
}

void number(std::uint64_t& hash, std::uint64_t value) {
    for (unsigned byte = 0; byte < 8; ++byte) {
        hash ^= std::uint8_t(value >> (byte * 8U)); hash *= 1099511628211ULL;
    }
}
void snapshot(std::uint64_t& hash, const Machine& machine) {
    std::apply([&](const auto&... value) { (number(hash, std::uint64_t(value)), ...); }, state(machine));
    for (const auto byte : machine.memory) { hash ^= byte; hash *= 1099511628211ULL; }
    number(hash, machine.accesses.size());
    for (const auto& access : machine.accesses) {
        number(hash, access.address); number(hash, access.value); number(hash, access.write);
    }
    number(hash, machine.interrupts.size());
    for (const auto interrupt : machine.interrupts) number(hash, interrupt);
    number(hash, machine.cpu.tracing.addrs.size());
    for (const auto address : machine.cpu.tracing.addrs) number(hash, address);
}

void equal(const Machine& generic, const Machine& owned) {
    require(state(generic) == state(owned), "CPU state/cycles differ");
    require(generic.memory == owned.memory, "memory differs");
    require(generic.accesses == owned.accesses, "bus accesses/order differ");
    require(generic.interrupts == owned.interrupts, "interrupt callbacks differ");
    require(generic.cpu.tracing.addrs == owned.cpu.tracing.addrs, "debug trace differs");
    require(generic.cpu.current_instruction_set == owned.cpu.current_instruction_set,
        "instruction mode differs");
}

void step(Machine& generic, Machine& owned, std::uint64_t* trace = nullptr) {
    generic.cpu.SingleStep();
    starfox::simulation::detail::step_owned_65816(owned.cpu);
    equal(generic, owned);
    if (trace) snapshot(*trace, generic);
}

std::size_t opcode_cases(Machine& generic, Machine& owned, std::uint64_t* trace = nullptr) {
    std::size_t count{};
    for (unsigned mode = 0; mode < 5U; ++mode) {
        // The pinned core does not implement BCD ADC/SBC. Exercise C/Z/V/N/I
        // without invoking those unrelated diagnostic-only panic stubs.
        for (const auto status : {0U, 0x01U, 0x04U, 0xc3U}) {
            for (const auto ip : {0x7000U, 0xfffeU, 0xffffU, 0x17000U}) {
                for (unsigned opcode = 0; opcode < 256U; ++opcode) {
                    for (const bool fast_blocks : {false, true}) {
                        for (auto* machine : {&generic, &owned}) {
                            machine->reset(mode, status, ip);
                            machine->cpu.fast_block_moves = fast_blocks;
                            machine->instruction(static_cast<std::uint8_t>(opcode));
                        }
                        step(generic, owned, trace);
                        ++count;
                    }
                }
            }
        }
    }
    return count;
}

std::size_t interrupt_cases(Machine& generic, Machine& owned, std::uint64_t* trace = nullptr) {
    std::size_t count{};
    for (unsigned mode = 0; mode < 5U; ++mode) {
        for (unsigned pending = 0; pending < 8U; ++pending) {
            for (const bool enabled : {false, true}) {
                for (const auto mask : {0xffU, 0xfffU, 0xffffU}) {
                    for (auto* machine : {&generic, &owned}) {
                        machine->reset(mode, enabled ? 0U : 4U, 0x17000U);
                        machine->cpu.cpu_state.ip_mask = mask;
                        machine->instruction(0xeaU);
                        machine->cpu.cpu_state.pending_interrupts.store(pending);
                    }
                    step(generic, owned, trace);
                    ++count;
                }
            }
        }
    }
    return count;
}

void breakpoint_cases(Machine& generic, Machine& owned, std::uint64_t* trace = nullptr) {
    for (const bool inconsistent_flag : {false, true}) {
        for (const bool pending_nmi : {false, true}) {
            for (auto* machine : {&generic, &owned}) {
                machine->reset(0U, 0U, 0x7000U);
                machine->instruction(0xeaU);
                machine->cpu.AddBreakpoint(machine->cpu.program_address(),
                    [machine](EmulatedCpu* cpu) {
                        require(cpu == &machine->cpu, "wrong breakpoint CPU");
                        ++machine->breakpoint_calls;
                        cpu->SetRegister("a", 0x56U);
                        cpu->SetRegister("p", 0x09U);
                    });
                // The map, not the advisory flag, is SingleStep's authority.
                if (inconsistent_flag) machine->cpu.has_breakpoints = false;
                machine->cpu.cpu_state.pending_interrupts.store(pending_nmi ? 4U : 0U);
            }
            step(generic, owned, trace);
            require(generic.breakpoint_calls == (pending_nmi ? 0U : 1U),
                "interrupt must precede breakpoint");
            for (auto* machine : {&generic, &owned}) {
                machine->cpu.RemoveBreakpoint(0x27000U);
                machine->instruction(0xeaU);
            }
            step(generic, owned, trace);
        }
    }
}

void sequence(Machine& generic, Machine& owned, std::uint64_t* trace = nullptr) {
    constexpr std::array<std::uint8_t, 18> program{
        0x18U, 0xfbU, 0xc2U, 0x30U, 0xa9U, 0x34U, 0x12U, 0xe2U, 0x30U,
        0xa9U, 0x56U, 0x48U, 0x68U, 0xcbU, 0xdbU, 0xeaU, 0x80U, 0xeeU};
    for (auto* machine : {&generic, &owned}) {
        machine->reset(4U, 0U, 0x7000U);
        std::copy(program.begin(), program.end(), machine->memory.begin() + 0x7000U);
    }
    for (unsigned index = 0; index < 256U; ++index) step(generic, owned, trace);
}

void cpu_workloads() {
    // Indexed 16-bit RAM reads, arithmetic, RAM writes, flag/loop updates and
    // branches. Not the previous NOP-only dispatch microbenchmark.
    constexpr std::array<std::uint8_t, 27> program{
        0xc2,0x30,0xa2,0x00,0x00,0xbd,0x00,0x20,0x18,0x69,0x37,0x01,
        0x9d,0x00,0x30,0xe8,0xe8,0xe0,0x00,0x08,0xd0,0xef,
        0xa2,0x00,0x00,0x80,0xea};
    constexpr unsigned instructions = 4000000;
    std::cout << "workload,instructions,milliseconds,complete_state_digest\n";
    for (unsigned workload = 0; workload < 3; ++workload) {
        auto machine = std::make_unique<Machine>(false);
        machine->reset(3, 0, 0x7000); machine->cpu.tracing.addrs.clear();
        machine->record_accesses = false;
        std::copy(program.begin(), program.end(), machine->memory.begin() + 0x7000);
        for (unsigned i = 0; i < 2048; ++i) machine->memory[0x2000 + i] = std::uint8_t(i * 37U);
        for (unsigned bank = 0; bank < 256; ++bank) {
            if (workload == 1) {
                for (unsigned page : {2U, 3U}) {
                    machine->pages[bank * 16 + page].io_mask = 0;
                    machine->pages[bank * 16 + page].io_eq = 0;
                }
            } else if (workload == 2) machine->pages[bank * 16 + 3].flags = Page::kReadOnly;
        }
        const auto start = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < instructions; ++i)
            starfox::simulation::detail::step_owned_65816(machine->cpu);
        const auto milliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        std::uint64_t hash = 14695981039346656037ULL; snapshot(hash, *machine);
#ifndef STARFOX_CPU_LEGACY_ORACLE
        // Pinned BEFORE the CPU/bus unity edit, using the separate old library.
        // The opcode oracle records IO callbacks; additionally guard direct
        // paged RAM and ignored read-only writes, where inlining matters most.
        constexpr std::array<std::uint64_t, 3> expected{
            0xb7691115198386cdULL, 0xb7691115198386cdULL, 0x191d33089eb792cdULL};
        require(hash == expected[workload], "CPU compilation changed mixed RAM/IO/read-only state");
#endif
        std::cout << workload << ',' << instructions << ',' << milliseconds
            << ',' << std::hex << hash << std::dec << '\n';
    }
}

void benchmark() {
    auto generic = std::make_unique<Machine>(false);
    auto owned = std::make_unique<Machine>(false);
    constexpr std::uint32_t steps = 2000000U;
    const auto time = [](Machine& machine, bool fast) {
        machine.reset(0U, 0U, 0U);
        machine.cpu.tracing.addrs.clear();
        // Entire mirrored address space contains NOPs; no reset vectors/operands.
        machine.memory.fill(0xeaU);
        const auto start = std::chrono::steady_clock::now();
        for (std::uint32_t index = 0; index < steps; ++index) {
            if (fast) starfox::simulation::detail::step_owned_65816(machine.cpu);
            else machine.cpu.SingleStep();
        }
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
    };
    for (unsigned trial = 0; trial < 5U; ++trial) {
        double baseline{}, specialized{};
        if (trial & 1U) {
            specialized = time(*owned, true);
            baseline = time(*generic, false);
        } else {
            baseline = time(*generic, false);
            specialized = time(*owned, true);
        }
        equal(*generic, *owned);
        std::cout << "NOP dispatch trial " << trial << ": generic=" << baseline
            << "ms owned=" << specialized << "ms (host microbenchmark, not game FPS)\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string{argv[1]} == "--cpu-workload-benchmark") {
            cpu_workloads(); return EXIT_SUCCESS;
        }
        auto generic = std::make_unique<Machine>();
        auto owned = std::make_unique<Machine>();
        std::uint64_t trace = 14695981039346656037ULL;
        const auto opcodes = opcode_cases(*generic, *owned, &trace);
        const auto interrupts = interrupt_cases(*generic, *owned, &trace);
        breakpoint_cases(*generic, *owned, &trace);
        sequence(*generic, *owned, &trace);
        // Filled from the separate pre-unity library/executable. It covers
        // complete registers/flags/cycles/RAM, ordered IO/IRQs and debug trace,
        // not two wrappers compiled against the same optimized bus routines.
#ifndef STARFOX_CPU_LEGACY_ORACLE
        constexpr std::uint64_t expected = 0xc0500dd32658dff0ULL;
        require(trace == expected, "CPU compilation changed independent state/bus trace");
#endif
        std::cout << "Independent CPU state/bus trace digest " << std::hex << trace << std::dec << '\n';
        std::cout << "Owned 65C816 step: " << opcodes << " opcode fixtures, "
            << interrupts << " interrupt fixtures, breakpoint fallback and mode sequence PASS\n";
        if (argc == 2 && std::string{argv[1]} == "--benchmark") benchmark();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Owned 65C816 step: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
