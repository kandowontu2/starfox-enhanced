// Adapted with permission from Esteban PDN / Starwing 3DS v1.1 (0863e36).
#include "starfox/assets/rom.hpp"
#include "3ds_cartridge_fixture.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "../src/simulation/wdc65816_collision.hpp"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>
using namespace starfox;
struct Machine {
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(0x20000);
    std::vector<Page> pages=std::vector<Page>(4096);
    SystemBus bus{};WDC65C816 cpu{&bus};
    Machine(){
        for(auto& p:pages){p={};p.io_eq=1;p.cycles_per_access=1;}
        bus.Init(12,24,pages.data());bus.Map(0,ram.data(),0x2000);
        bus.Map(0x7e0000,ram.data(),ram.size());
        cpu.mode_native_6502=false;cpu.mode_emulation=false;
        cpu.mode_long_a=true;cpu.mode_long_xy=true;cpu.OnUpdateMode();
        cpu.cpu_state.event_cycle=0;cpu.cpu_state.pending_interrupts.store(0);
    }
};
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
void same(Machine& a,Machine& b){
 const auto& x=a.cpu.cpu_state;const auto& y=b.cpu.cpu_state;
 require(a.ram==b.ram,"RAM diverged");
 require(a.bus.open_bus==b.bus.open_bus,"open bus diverged");
 require(x.cycle==y.cycle,"cycles diverged");
 require(x.ip==y.ip &&x.code_segment_base==y.code_segment_base,"PC diverged");
 require(x.regs.a.u16==y.regs.a.u16&&x.regs.x.u16==y.regs.x.u16&&x.regs.y.u16==y.regs.y.u16,"registers diverged");
 require(x.zero==y.zero&&x.negative==y.negative&&x.carry==y.carry&&x.other_flags==y.other_flags,"raw flags diverged");
 require(x.data_segment_base==y.data_segment_base&&x.regs.d.u16==y.regs.d.u16&&x.regs.sp.u16==y.regs.sp.u16,"address registers diverged");
 require(a.cpu.num_emulated_instructions==b.cpu.num_emulated_instructions,"instruction count diverged");
 require(a.cpu.mode_long_a==b.cpu.mode_long_a&&a.cpu.mode_long_xy==b.cpu.mode_long_xy,"mode diverged");
}
int main(int argc,char** argv)try{
 const auto cartridge=test_3ds_cartridge(argc,argv);
 const auto& rom=cartridge.rom;const auto& symbols=cartridge.symbols;
 const auto sym=[&](const char* s){return symbols.find(s).at(0);};
 const auto layout=sym("NUMBER_AL")==0x50?simulation::ObjectMemoryLayout::starfox_ex:simulation::ObjectMemoryLayout::original;
 simulation::ObjectPool objects(sym("NUMBER_AL"),layout);
 simulation::MapVm map(rom,simulation::MapDatabase(rom,symbols),objects,&symbols);
 simulation::Wdc65816Registers regs{};regs.status=0x24;
 map.call_native_routine(sym("COPY_TO_0101_L"),regs,5'000'000);
 std::vector<std::uint8_t> ram(131072);
 for(unsigned i=0;i<ram.size();++i)ram[i]=map.read_native_byte(0x7e0000+i);
 using D=simulation::detail::CollisionDispatch;
 const std::array<unsigned,3> pcs{sym("CHKCOLL0")+13U,sym("CHKCOLLNXT"),sym("NORMALCOL")};
 require(D::filter_matches(ram,pcs[0]&65535),"filter byte guard rejected source");
 require(D::next_matches(ram,pcs[1]&65535),"next byte guard rejected source");
 require(D::axis_matches(ram,pcs[2]&65535),"axis byte guard rejected source");
 // Any opcode OR operand edit after capture must reject the fast path.
 simulation::detail::CollisionCodeBlock<99U> guarded;
 guarded.capture(ram,pcs[0],D::filter_matches);
 require(guarded.matches(ram,pcs[0]),"captured source rejected");
 for(unsigned i=0;i<99;++i) {
    ram[(pcs[0]&65535)+i]^=1;
    require(!guarded.matches(ram,pcs[0]),"modified code accepted");
    ram[(pcs[0]&65535)+i]^=1;
 }
 ram[pcs[0]&65535]^=1;
 guarded.capture(ram,pcs[0],D::filter_matches);
 require(!guarded.matches(ram,pcs[0]),"foreign opcode accepted");
 ram[pcs[0]&65535]^=1;
 std::mt19937 random(0x3d50c070);
 auto fast=std::make_unique<Machine>(),slow=std::make_unique<Machine>();
 // These are the exact production eligibility predicates, not a test mirror.
 {
   auto& c=fast->cpu;c.cpu_state.data_segment_base=0x7e0000;
   c.cpu_state.regs.d.u16=0;c.cpu_state.other_flags=4;
   const auto allowed=[&](bool rep=false,std::size_t budget=48){return D::allowed(c,rep,budget);};
   require(allowed(),"valid native state rejected");
   require(!allowed(false,47),"short instruction budget accepted");
   c.cpu_state.regs.d.u16=1;require(!allowed(),"nonzero direct page accepted");c.cpu_state.regs.d.u16=0;
   c.cpu_state.data_segment_base=0x7f0000;require(!allowed(),"foreign data bank accepted");c.cpu_state.data_segment_base=0x7e0000;
   c.mode_long_a=false;require(!allowed(),"A8 accepted by A16 block");require(allowed(true),"REP block rejected A8");c.mode_long_a=true;
   c.mode_long_xy=false;require(!allowed(),"I8 accepted");c.mode_long_xy=true;
   c.mode_emulation=true;require(!allowed(),"emulation accepted");c.mode_emulation=false;
   c.mode_native_6502=true;require(!allowed(),"6502 accepted");c.mode_native_6502=false;
   c.has_breakpoints=true;require(!allowed(),"breakpoints bypassed");c.has_breakpoints=false;
   c.tracing.addrs.push_back(1);require(!allowed(),"trace bypassed");c.tracing.addrs.clear();
   c.cpu_state.pending_interrupts.store(2);require(!allowed(),"pending IRQ bypassed");c.cpu_state.pending_interrupts.store(0);
   c.cpu_state.other_flags|=8;require(!allowed(),"decimal accepted");c.cpu_state.other_flags&=~8U;
 }
 for(unsigned block=0;block<3;++block)for(unsigned trial=0;trial<4096;++trial){
   fast->ram=ram;
   for(unsigned i=0;i<0x3400;++i)fast->ram[i]=random();
   const auto handle=[&]{return sym("ALBLKS")+(random()%sym("NUMBER_AL"))*sym("AL_SIZE");};
   const std::uint16_t other=handle();
   const auto tmpx=ram[(pcs[0]&65535)+1];fast->ram[tmpx]=other;fast->ram[tmpx+1]=other>>8;
   slow->ram=fast->ram;
   const std::array<std::uint32_t,8> initial{random(),random(),random(),random(),random(),random(),random(),random()};
   const auto x=block==0?handle():random()%65000; const auto y=handle();
   for(auto* m:{fast.get(),slow.get()}){
      auto& c=m->cpu;auto& s=c.cpu_state;c.SetStatusRegister(block==1&&trial%2?0x24:4);
      s.regs.a.u16=initial[0];s.regs.x.u16=x;s.regs.y.u16=y;
      s.regs.d.u16=0;s.regs.sp.u16=0x1ff;s.ip=pcs[block]&65535;s.code_segment_base=0x7e0000;s.data_segment_base=0x7e0000;
      s.zero=initial[1];s.negative=initial[2];s.carry=initial[3];s.other_flags=initial[4]&0xc7;
      s.cycle=(std::uint64_t(initial[5])<<32)|initial[6];m->bus.open_bus=initial[7];c.num_emulated_instructions=100;
   }
   std::vector<unsigned> trace;auto record=[&](unsigned pc){trace.push_back(pc);};
   if(block==0)D::filter(fast->cpu,record);else if(block==1)D::next(fast->cpu,record);else D::axis(fast->cpu,record);
   require(trace.size()<=48U,"block exceeds instruction budget guard");
   for(auto pc:trace){require(slow->cpu.program_address()==pc,"instruction trace diverged");slow->cpu.SingleStep();}
   try{same(*fast,*slow);}catch(const std::exception& e){std::fprintf(stderr,"block%u trial%u: %s\n",block,trial,e.what());return 1;}
 }
 std::printf("Collision blocks (%u objects): 12288 randomized states and 99 byte guards matched; RAM, raw flags, cycles, open bus, every PC\n",sym("NUMBER_AL"));
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
