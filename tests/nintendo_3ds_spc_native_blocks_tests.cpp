// Adapted from Esteban PDN's Starwing 3DS v1.1 native-block equivalence test.
#include <SNES_SPC.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
struct Copy {
    std::vector<std::uint8_t> bytes;
    static void fields(unsigned char** io,void* data,std::size_t size) {
        auto& self=*reinterpret_cast<Copy*>(*io);
        auto* first=static_cast<std::uint8_t*>(data);
        self.bytes.insert(self.bytes.end(),first,first+size);
    }
};
auto state(SNES_SPC& spc) {
    Copy result;auto* io=reinterpret_cast<unsigned char*>(&result);
    spc.copy_state_preserving_machine(&io,Copy::fields);return result.bytes;
}
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
}
int main() try {
    std::mt19937 random(0x5f58b);
    unsigned long long accelerated{};unsigned comparisons{};
    for(unsigned fixture=0;fixture<96;++fixture) {
        std::vector<std::uint8_t> snapshot(SNES_SPC::spc_file_size);
        SNES_SPC::init_header(snapshot.data());
        snapshot[0x25]=0x24;snapshot[0x26]=6;
        snapshot[0x28]=std::array<std::uint8_t,4>{0,1,2,255}[fixture%4];
        snapshot[0x29]=std::uint8_t(random());snapshot[0x2a]=(fixture&4)?0x20:0;
        snapshot[0x2b]=0xff;
        auto* ram=snapshot.data()+0x100;
        for(unsigned i=0;i<65536;++i)ram[i]=std::uint8_t(random());
        std::memset(ram+0xf0,0,16);snapshot[0x10100+0x6c]=0x20;
        const std::array<std::uint8_t,34> program{
            0xaa,0x20,0x40,0x8a,0x21,0xa0,0x2b,0x20,0x2b,0x21,
            0xe4,0x20,0x25,0x22,0x00,0x08,0x3f,0xd6,0x00,0x30,
            0xfc,0xe4,0x21,0x08,0x50,0xd6,0x00,0x31,0xfc,0x1d,
            0xd0,0xe0,0x2f,0xfe};
        std::copy(program.begin(),program.end(),ram+0x624);
        const unsigned rejection=fixture/16;
        if(rejection==1)ram[0x633]=0x48;
        if(rejection==2){ram[0x636]=0xf4;ram[0x637]=0;}
        if(rejection==3){ram[0x636]=0x24;ram[0x637]=6;snapshot[0x29]=0;}
        if(rejection==4)ram[0x631]=0xf4;
        if(rejection==5){ram[0x636]=0xc0;ram[0x637]=0xff;}
        SNES_SPC reference,candidate;require(!reference.init()&&!candidate.init(),"SPC init failed");
        require(!reference.load_spc(snapshot.data(),snapshot.size())
            && !candidate.load_spc(snapshot.data(),snapshot.size()),"SPC load failed");
        candidate.enable_native_blocks(true);
        for(unsigned slice=0;slice<128;++slice) {
            std::array<short,1024> expected{},actual{};
            reference.set_output(expected.data(),expected.size());candidate.set_output(actual.data(),actual.size());
            const int clocks=slice<64 ? int(slice+1) : int(1+random()%512);
            reference.end_frame(clocks);candidate.end_frame(clocks);
            require(reference.sample_count()==candidate.sample_count()&&expected==actual,"Native block changed PCM");
            require(state(reference)==state(candidate),"Native block changed CPU, DSP, RAM or cycle state");
            ++comparisons;
        }
        if(rejection)require(candidate.native_block_count()==0,"Unsafe block did not fall back");
        accelerated+=candidate.native_block_count();
    }
    require(accelerated>0,"Native block was never exercised");
    std::cout<<comparisons<<" exact PCM/core comparisons; "<<accelerated
        <<" native iterations; short deadlines, wrap, flags, I/O, ROM and mutation guards checked\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
