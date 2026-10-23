// Adapted with permission from Esteban PDN / Starwing 3DS v1.1 (0863e36).
#include "starfox/assets/rom.hpp"
#include "3ds_cartridge_fixture.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "../src/simulation/wdc65816_transfer.hpp"
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
    std::vector<std::uint8_t> code=std::vector<std::uint8_t>(0x10000);
    std::array<std::uint8_t,4096> io{};
    struct Write { unsigned address; std::uint8_t value; std::uint64_t cycle; bool operator==(const Write&) const = default; };
    std::vector<Write> writes;
    unsigned throw_on_write{};
    unsigned interrupt_on_write{};
    SystemBus bus{};WDC65C816 cpu{&bus};
    static void read(void* context,cpuaddr_t address,std::uint8_t* value,std::uint32_t) {
        auto& self=*static_cast<Machine*>(context);*value=self.io[address&4095];
    }
    static void write(void* context,cpuaddr_t address,const std::uint8_t* value,std::uint32_t) {
        auto& self=*static_cast<Machine*>(context);self.io[address&4095]=*value;
        self.writes.push_back({address,*value,self.cpu.cpu_state.cycle});
        if(self.interrupt_on_write && self.writes.size()==self.interrupt_on_write)
            self.cpu.cpu_state.pending_interrupts.store(1U);
        if(self.throw_on_write && self.writes.size()==self.throw_on_write)
            throw std::runtime_error{"injected I/O callback failure"};
    }
    Machine(){
        for(auto& p:pages){p={};p.io_eq=1;p.cycles_per_access=1;}
        bus.Init(12,24,pages.data());bus.Map(0,ram.data(),0x2000);
        bus.Map(0x7e0000,ram.data(),ram.size());bus.Map(0x400000,code.data(),code.size(),true);
        pages[2].io_eq=0;pages[2].io_mask=0;pages[2].cycles_per_access=7;
        pages[0x802].io_eq=0;pages[0x802].io_mask=0;pages[0x802].cycles_per_access=7;
        bus.io_devices.context=this;bus.io_devices.read=read;bus.io_devices.write=write;
        cpu.mode_native_6502=false;cpu.mode_emulation=false;
        cpu.mode_long_a=true;cpu.mode_long_xy=true;cpu.OnUpdateMode();
        cpu.cpu_state.event_cycle=0;cpu.cpu_state.pending_interrupts.store(0);
    }
};
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
void same(Machine& a,Machine& b){
 const auto& x=a.cpu.cpu_state;const auto& y=b.cpu.cpu_state;
 require(a.ram==b.ram,"RAM diverged");
 require(a.io==b.io && a.writes==b.writes,"I/O callback values/order/cycles diverged");
 require(a.bus.open_bus==b.bus.open_bus,"open bus diverged");
 require(x.cycle==y.cycle,"cycles diverged");
 require(x.ip==y.ip &&x.code_segment_base==y.code_segment_base,"PC diverged");
 require(x.regs.a.u16==y.regs.a.u16&&x.regs.x.u16==y.regs.x.u16&&x.regs.y.u16==y.regs.y.u16,"registers diverged");
 require(x.zero==y.zero&&x.negative==y.negative&&x.carry==y.carry&&x.other_flags==y.other_flags,"raw flags diverged");
 require(x.data_segment_base==y.data_segment_base&&x.regs.d.u16==y.regs.d.u16&&x.regs.sp.u16==y.regs.sp.u16,"address registers diverged");
 require(a.cpu.num_emulated_instructions==b.cpu.num_emulated_instructions,"instruction count diverged");
 require(a.cpu.mode_long_a==b.cpu.mode_long_a&&a.cpu.mode_long_xy==b.cpu.mode_long_xy,"mode diverged");
 require(x.mode==y.mode&&a.cpu.current_instruction_set==b.cpu.current_instruction_set,"mode dispatch table diverged");
 require(x.pending_interrupts.load()==y.pending_interrupts.load(),"IRQ boundary diverged");
}

int main(int argc,char** argv)try{
 const auto cartridge=test_3ds_cartridge(argc,argv);
 const auto& rom=cartridge.rom;const auto& symbols=cartridge.symbols;
 const auto sym=[&](const char* s){return symbols.find(s).at(0);};
 using D=simulation::detail::TransferDispatch;
 const auto pointer=std::uint8_t(sym("ADDRESS"));
 const auto boot=D::boot_bytes(pointer);const auto sboot=D::sboot_bytes(pointer);
 for(unsigned i=0;i<boot.size();++i)require(rom.read8(sym("BOOT_LOOP")+i)==boot[i],"ROM boot pattern mismatch");
 for(unsigned i=0;i<sboot.size();++i)require(rom.read8(sym("SBOOT_LOOP")+i)==sboot[i],"ROM sboot pattern mismatch");
 std::array<std::vector<std::uint8_t>,4> patterns{{
    {boot.begin(),boot.end()},{sboot.begin(),sboot.end()},
    {D::word_fill_bytes.begin(),D::word_fill_bytes.end()},
    {D::short_index_fill_bytes.begin(),D::short_index_fill_bytes.end()}}};
 std::mt19937 random(0x3d507172);
 auto fast=std::make_unique<Machine>(),slow=std::make_unique<Machine>();
 D::ModeCache modes;
 // All current ABI/raw flag state, including hidden B, is compared below.
 for(unsigned block=0;block<4;++block)for(unsigned trial=0;trial<4096;++trial){
   const unsigned pc=trial%17U==0U
       ?0x410000U-static_cast<unsigned>(patterns[block].size()):0x408100U;
   for(unsigned i=0;i<0x20000;++i)fast->ram[i]=std::uint8_t(i*13U+trial);
   std::copy(patterns[block].begin(),patterns[block].end(),fast->code.begin()+(pc&65535));
   const auto source=trial==0?0xffffffU:trial==1?0xfffff0U:std::uint32_t(0x7e0000U+(random()%0x10000U));
   for(unsigned i=0;i<3;++i)fast->ram[pointer+i]=source>>(i*8);
   slow->ram=fast->ram;slow->code=fast->code;
   const std::array<std::uint32_t,8> initial{random(),random(),random(),random(),random(),random(),random(),random()};
   const auto x=trial<3?trial:random()&65535;
   const auto y=trial==0?1U:trial==1?32U:trial==2?65535U:trial==3?0U:random()&65535;
   fast->io.fill(0);fast->io[0x140]=(trial%2||trial==4)?std::uint8_t(initial[0]):std::uint8_t(initial[0]+1);
   slow->io=fast->io;
   for(auto* m:{fast.get(),slow.get()}){
      m->writes.clear();m->throw_on_write=block<2&&trial>=4&&trial<=5?trial-3:0;
      auto& c=m->cpu;auto& st=c.cpu_state;
      c.mode_native_6502=false;c.mode_emulation=false;c.mode_long_a=block>=2;c.mode_long_xy=block!=3;c.OnUpdateMode();
      st.regs.a.u16=initial[0];st.regs.x.u16=block==3?x&255:x;st.regs.y.u16=block==3?y&255:y;
      st.regs.d.u16=0;st.regs.sp.u16=0x1ff;st.ip=pc&65535;st.code_segment_base=pc&0xff0000;st.data_segment_base=trial==6?0x800000:trial%3?0:0x7e0000;
      st.zero=initial[1];st.negative=initial[2];st.carry=initial[3];st.other_flags=initial[4]&0xc7;
      st.cycle=(std::uint64_t(initial[5])<<32)|initial[6];m->bus.open_bus=initial[7];m->bus.open_bus_is_data=trial%2;c.num_emulated_instructions=100;
      m->pages[pc>>12].cycles_per_access=1U+trial%11U;
   }
   std::vector<unsigned> trace;auto record=[&](unsigned at){trace.push_back(at);};
   bool fast_threw=false,slow_threw=false;
   try {
     if(block==0)D::upload<false>(fast->cpu,record,modes);else if(block==1)D::upload<true>(fast->cpu,record,modes);
     else if(block==2)D::word_fill(fast->cpu,record);else D::short_index_fill(fast->cpu,record);
   } catch(const std::runtime_error&){fast_threw=true;}
   require(trace.size()<=(block<2?13U:block==2?3U:6U),"block exceeded guard budget");
   try {for(auto at:trace){require(slow->cpu.program_address()==at,"instruction trace diverged");slow->cpu.SingleStep();}}
   catch(const std::runtime_error&){slow_threw=true;}
   require(fast_threw==slow_threw,"callback exception diverged");
   if(block<2 && trial>=4 && trial<=5)require(fast_threw,"callback failure case was not exercised");
   try{same(*fast,*slow);}catch(const std::exception& e){std::fprintf(stderr,"block%u trial%u: %s\n",block,trial,e.what());return 1;}
 }
 // Repeated blocks retain the prior block boundary for budgets, polling
 // exits, bank rollover, callback exceptions and a newly requested IRQ.
 for(unsigned block=0;block<4;++block)for(unsigned trial=0;trial<128;++trial){
   fast=std::make_unique<Machine>();slow=std::make_unique<Machine>();
   constexpr unsigned pc=0x408100;
   const auto minimum=block<2?13U:block==2?3U:6U;
   const std::size_t budget=minimum+trial%137U;
   for(auto* m:{fast.get(),slow.get()}){
     std::copy(patterns[block].begin(),patterns[block].end(),m->code.begin()+(pc&65535));
     auto& c=m->cpu;auto& s=c.cpu_state;
     c.mode_long_a=block>=2;c.mode_long_xy=block!=3;c.OnUpdateMode();
     s.ip=pc&65535;s.code_segment_base=pc&0xff0000;
     s.data_segment_base=trial%2?0x800000U:0U;
     s.regs.d.u16=0U;s.regs.sp.u16=0x1ffU;
     s.zero=2U;s.negative=0U;s.carry=2U;s.other_flags=4U;
     s.interrupts=0U;s.cycle=std::uint64_t(trial)*7919U;
     s.regs.a.u16=0x5732U;s.regs.x.u16=trial%7U;
     s.regs.y.u16=block==3?trial%9U:trial%3?0xfffcU:32U;
     const unsigned source=0x7e2000U;
     for(unsigned i=0;i<3;++i)m->ram[pointer+i]=source>>(i*8);
     m->io[0x140]=trial%3?0x32U:0x33U;
     m->throw_on_write=trial%4?1U+trial%19U:0U;
     m->interrupt_on_write=trial%5?0U:1U+trial%23U;
     m->pages[pc>>12].cycles_per_access=1U+trial%11U;
     m->bus.open_bus_is_data=trial%2;
   }
   D::ModeCache fast_modes,slow_modes;
   const auto execute=[&](Machine& m,auto& record,D::ModeCache& cache){
     if(block==0)D::upload<false>(m.cpu,record,cache);
     else if(block==1)D::upload<true>(m.cpu,record,cache);
     else if(block==2)D::word_fill(m.cpu,record);
     else D::short_index_fill(m.cpu,record);
   };
   std::vector<unsigned> fast_trace,slow_trace;
   auto fast_record=[&](unsigned at){fast_trace.push_back(at);};
   bool fast_threw=false,slow_threw=false;
   try {D::repeat(fast->cpu,fast_record,budget,minimum,[&](auto& r){execute(*fast,r,fast_modes);});}
   catch(const std::runtime_error&){fast_threw=true;}
   auto remaining=budget;
   auto slow_record=[&](unsigned at){slow_trace.push_back(at);--remaining;};
   try {
     for(unsigned b=0;b<256U && slow->cpu.program_address()==pc
         && D::allowed(slow->cpu,block>=2,block!=3,remaining,minimum);++b)
       execute(*slow,slow_record,slow_modes);
   }catch(const std::runtime_error&){slow_threw=true;}
   require(fast_threw==slow_threw && fast_trace==slow_trace,"repeated block exit diverged");
   same(*fast,*slow);
 }
 // The exact production guards reject low budgets, mode changes and debugger/IRQ use.
 auto& c=fast->cpu;c.mode_long_a=false;c.mode_long_xy=true;c.OnUpdateMode();c.cpu_state.regs.d.u16=0;c.cpu_state.other_flags=4;c.cpu_state.data_segment_base=0;
 const auto allowed=[&](std::size_t budget=13){return D::allowed(c,false,true,budget,13);};
 require(allowed(),"valid mode rejected");require(!allowed(12),"short budget accepted");
 c.mode_long_a=true;require(!allowed(),"A16 upload accepted");c.mode_long_a=false;
 c.mode_long_xy=false;require(!allowed(),"I8 upload accepted");c.mode_long_xy=true;
 c.mode_emulation=true;require(!allowed(),"emulation accepted");c.mode_emulation=false;
 c.mode_native_6502=true;require(!allowed(),"6502 accepted");c.mode_native_6502=false;
 c.cpu_state.regs.d.u16=1;require(!allowed(),"nonzero direct accepted");c.cpu_state.regs.d.u16=0;
 c.cpu_state.data_segment_base=0x7e0000;require(!allowed(),"WRAM data bank accepted");c.cpu_state.data_segment_base=0x800000;require(allowed(),"FastROM mirror rejected");c.cpu_state.data_segment_base=0;
 c.cpu_state.other_flags|=8;require(!allowed(),"decimal accepted");c.cpu_state.other_flags&=~8U;
 c.has_breakpoints=true;require(!allowed(),"debugger accepted");c.has_breakpoints=false;
 c.tracing.addrs.push_back(1);require(!allowed(),"trace accepted");c.tracing.addrs.clear();
 c.cpu_state.pending_interrupts.store(2);require(!allowed(),"IRQ accepted");c.cpu_state.pending_interrupts.store(0);
 const auto guard_test=[&]<std::size_t N>(const std::array<std::uint8_t,N>& pattern) {
   constexpr unsigned pc=0x408100;std::copy(pattern.begin(),pattern.end(),fast->code.begin()+(pc&65535));
   simulation::detail::TransferCodeBlock<N> code;code.capture(c,pc,pattern);require(code.matches(c,pc),"code capture failed");
   for(unsigned i=0;i<N;++i){fast->code[(pc&65535)+i]^=1;require(!code.matches(c,pc),"mutated code accepted");simulation::detail::TransferCodeBlock<N> bad;bad.capture(c,pc,pattern);require(bad.address==0,"foreign pattern captured");fast->code[(pc&65535)+i]^=1;}
   fast->pages[pc>>12].flags=0;require(!code.matches(c,pc),"writable code accepted");fast->pages[pc>>12].flags=Page::kReadOnly;
   auto& page=fast->pages[pc>>12];page.io_mask=0xffffffU;page.io_eq=pc+3U;
   require(!code.matches(c,pc),"interior I/O byte accepted");page.io_mask=0;page.io_eq=1;
   const auto crossing=pc+(4096U-(pc&4095U))-N+1U;
   std::copy(pattern.begin(),pattern.end(),fast->code.begin()+(crossing&65535U));
   simulation::detail::TransferCodeBlock<N> edge;edge.capture(c,crossing,pattern);
   require(edge.address==0U,"cross-page code accepted");
   const auto ending=crossing-1U;
   std::copy(pattern.begin(),pattern.end(),fast->code.begin()+(ending&65535U));
   edge.capture(c,ending,pattern);require(edge.matches(c,ending),"page-ending block rejected");
 };
 guard_test(boot);guard_test(sboot);guard_test(D::word_fill_bytes);guard_test(D::short_index_fill_bytes);
 std::printf("Transfer blocks: 16384 randomized states, 512 repeated-block exits and all 71 byte guards exact; raw flags, cycles, every PC, I/O callbacks/order/cycles, IRQ boundaries and open bus\n");
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
