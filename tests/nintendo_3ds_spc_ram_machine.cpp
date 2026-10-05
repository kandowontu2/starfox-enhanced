// Asset-free, real pinned SPC CPU/DSP differential driver. This is compiled
// twice by check_3ds_spc_ram.py, never substituted into the player.
#include "SNES_SPC.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

namespace {
void output(const void* data, std::size_t size) {
    if (std::fwrite(data, 1, size, stdout) != size) throw std::runtime_error("Output failed");
}
void save(unsigned char** cursor, void* source, std::size_t size) {
    std::memcpy(*cursor, source, size); *cursor += size;
}
void load(unsigned char** cursor, void* destination, std::size_t size) {
    std::memcpy(destination, *cursor, size); *cursor += size;
}
void record(SNES_SPC& core, bool restore) {
    std::vector<unsigned char> state(SNES_SPC::state_size);
    auto* cursor = state.data();
    core.copy_state_preserving_machine(&cursor, save);
    const auto count = std::uint32_t(cursor - state.data());
    output(&count, sizeof count); output(state.data(), count);
    std::array<unsigned char, 4> ports{};
    core.save_cpu_input_ports(ports.data()); output(ports.data(), ports.size());
    int clocks{}, carried{};
    std::array<short, SNES_SPC::extra_size> carry{};
    core.save_output_carry(clocks, carried, carry.data());
    output(&clocks, sizeof clocks); output(&carried, sizeof carried);
    output(carry.data(), sizeof carry);
    if (restore) {
        cursor = state.data(); core.copy_state(&cursor, load);
        core.load_cpu_input_ports(ports.data()); core.load_output_carry(clocks, carried, carry.data());
    }
}
std::vector<unsigned char> fixture(unsigned address, bool page, bool rom, unsigned seed) {
    std::vector<unsigned char> file(SNES_SPC::spc_file_size);
    SNES_SPC::init_header(file.data());
    file[0x25] = 0; file[0x26] = 0x20; // PC $2000, outside tested destinations.
    file[0x28] = 1; file[0x29] = 3; file[0x2a] = page ? 0x20 : 0; file[0x2b] = 0xef;
    for (unsigned i = 0; i < 65536; ++i) file[0x100+i] = (i*17+seed*13)&255;
    auto* ram = file.data()+0x100;
    constexpr std::array<unsigned char,4> directory{0,3,0,3};
    constexpr std::array<unsigned char,9> sample{3,0x71,0x32,0x54,0x16,0x27,0x43,0x65,0x10};
    std::copy(directory.begin(),directory.end(),ram+0x200);
    std::copy(sample.begin(),sample.end(),ram+0x300);
    ram[0xf0] = 0x0a; ram[0xf1] = (rom ? 0x80 : 0) | 7;
    ram[0xf2] = 0x6c; ram[0xfa] = 7; ram[0xfb] = 11; ram[0xfc] = 13;
    file[0x10100+0x6c] = 0x3f; // No echo/reset/mute; audible noise.
    std::vector<unsigned char> program;
    const auto emit = [&](std::initializer_list<unsigned char> bytes) { program.insert(program.end(), bytes); };
    const auto dsp = [&](unsigned char reg, unsigned char value) {
        emit({0x8f, reg, 0xf2, 0x8f, value, 0xf3});
    };
    // Page-one MOV dp does not access DSP; explicitly CLR P for this prefix.
    emit({0x20});
    dsp(0x6c,0x3f); dsp(0x0c,0x7f); dsp(0x1c,0x7f); dsp(0x00,0x60); dsp(0x01,0x60);
    dsp(0x04,0); dsp(0x05,0); dsp(0x07,0x7f); dsp(0x5d,2);
    dsp(0x02,0); dsp(0x03,8); dsp(0x3d,seed==7 ? 0 : 1); dsp(0x4c,1);
    emit({static_cast<unsigned char>(page ? 0x40 : 0x20)});
    const unsigned char lo = address&255, hi = address>>8;
    unsigned char value = (seed*29+13)&255;
    if (address == 0xf0) value=0x0a;
    if (address == 0xf1) value=(rom ? 0x80 : 0)|7;
    if (address == 0xf2) value=0x6c;
    if (address == 0xf3) value=0x3f;
    const auto loop = program.size();
    emit({0xe8,value,0xc5,lo,hi,0xe5,lo,hi,0x08,1,
        0xd6,lo,hi,0xf6,lo,hi,0xd5,lo,hi,0xf5,lo,hi,
        0x8f,value,lo,0xe4,lo,0xf8,lo,0xeb,lo,
        0x2c,lo,hi,0x25,lo,hi});
    // Exercise CPU_mem_bit(), which has no cached local RAM pointer.
    emit({0xaa,lo,static_cast<unsigned char>(hi&0x1f),
        0x8a,lo,static_cast<unsigned char>((hi&0x1f)|0x60)});
    emit({0xe8,value,0xc5,0xf4,0,0xc5,0xf5,0,0xc5,0xf6,0,0xc5,0xf7,0});
    // Absolute jump also permits the deliberately longer DSP prefix.
    const auto target = 0x2000+loop;
    emit({0x5f,static_cast<unsigned char>(target),static_cast<unsigned char>(target>>8)});
    std::copy(program.begin(), program.end(), ram+0x2000);
    return file;
}
}

int main() {
    try {
#if defined(_WIN32)
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        constexpr std::array<unsigned, 24> addresses{0,1,0x7f,0xef,0xf0,0xf1,0xf2,0xf3,
            0xf4,0xf7,0xf8,0xf9,0xfa,0xfc,0xfd,0xfe,0xff,0x100,0x1ff,0xfeff,0xffbf,0xffc0,0xfffe,0xffff};
        constexpr std::array<int, 24> cuts{1,2,3,4,5,6,7,8,9,10,11,12,15,16,17,31,32,33,63,64,65,127,128,4096};
        std::array<unsigned char, SNES_SPC::rom_size> rom{};
        // Original test ROM (not Nintendo IPL): loop back through plain RAM.
        for (unsigned i=0;i<rom.size();i+=2) {rom[i]=0x2f;rom[i+1]=0xfe;}
        std::uint64_t cases{}, records{}, samples{}, audible{};
        for (auto address : addresses) for (bool page : {false,true})
            for (bool rom_enabled : {false,true}) for (unsigned seed : {1U,7U}) {
                SNES_SPC core;
                if (core.init()) throw std::runtime_error("SPC initialization failed");
                core.init_rom(rom.data());
                const auto file=fixture(address,page,rom_enabled,seed);
                if (core.load_spc(file.data(),file.size())) throw std::runtime_error("Synthetic SPC rejected");
                core.set_tempo(seed == 1 ? SNES_SPC::tempo_unit : 357);
                record(core,false); ++records; ++cases;
                for (unsigned i=0;i<cuts.size();++i) {
                    std::array<short, 4096> pcm{};
                    core.set_output(pcm.data(),pcm.size());
                    core.write_port(0,i%4,(i*41+seed)&255);
                    core.end_frame(cuts[i]);
                    const auto count=core.sample_count();
                    if (count<0 || count>int(pcm.size())) throw std::runtime_error("PCM output bounds");
                    output(&count,sizeof count); output(pcm.data(),count*sizeof(short));
                    samples += count;
                    for(int j=0;j<count;++j) audible += pcm[j]!=0;
                    record(core,i==18); ++records;
                }
            }
        if (!audible) throw std::runtime_error("Fixture never exercised audible DSP");
        std::fprintf(stderr,"PASS actual SPC RAM machine cases=%llu records=%llu samples=%llu audible=%llu\n",
            static_cast<unsigned long long>(cases),static_cast<unsigned long long>(records),
            static_cast<unsigned long long>(samples),static_cast<unsigned long long>(audible));
    } catch (const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
