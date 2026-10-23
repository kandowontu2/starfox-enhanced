#include "starfox/simulation/wdc65816.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace starfox;
void require(bool ok,const char* what) {if(!ok)throw std::runtime_error(what);}
int main() try {
    assets::RomImage rom(std::vector<std::uint8_t>(32768));
    simulation::Wdc65816 cpu(rom),other(rom);
    auto memory=cpu.ppu_memory_revision(),vram=cpu.ppu_vram_revision();
    require(memory!=other.ppu_memory_revision(),"CPU instances have separate cache epochs");
    const std::array<std::uint8_t,2> bytes{12,34};
    cpu.write_vram(65535,bytes);
    require(cpu.ppu_vram_revision()!=vram && cpu.ppu_memory_revision()!=memory,"Wrapped VRAM batch invalidates both caches");
    vram=cpu.ppu_vram_revision();memory=cpu.ppu_memory_revision();cpu.write_vram(65535,bytes);
    require(vram==cpu.ppu_vram_revision() && memory==cpu.ppu_memory_revision(),"Identical VRAM upload is reusable");
    cpu.write_cgram(2,std::array<std::uint16_t,1>{123});
    require(memory!=cpu.ppu_memory_revision() && vram==cpu.ppu_vram_revision(),"Palette does not invalidate tile decode");
    memory=cpu.ppu_memory_revision();cpu.write_cgram(2,std::array<std::uint16_t,1>{123});
    require(memory==cpu.ppu_memory_revision(),"Identical palette upload is reusable");
    cpu.write8(0x7e0000,89);cpu.upload_oam(0x7e0000,1);
    require(memory!=cpu.ppu_memory_revision(),"OAM changes invalidate foreground");
    memory=cpu.ppu_memory_revision();cpu.upload_oam(0x7e0000,1);
    require(memory==cpu.ppu_memory_revision(),"Identical OAM batch is reusable");
    cpu.write8(0x002116,2);cpu.write8(0x002117,0);cpu.write8(0x002118,55);
    require(vram!=cpu.ppu_vram_revision(),"VRAM register writes invalidate cached tiles");
    memory=cpu.ppu_memory_revision();cpu.write8(0x002102,4);cpu.write8(0x002103,0);cpu.write8(0x002104,76);
    require(memory!=cpu.ppu_memory_revision(),"OAM register writes invalidate foreground");
    memory=cpu.ppu_memory_revision();cpu.write8(0x002121,4);cpu.write8(0x002122,88);
    require(memory!=cpu.ppu_memory_revision(),"CGRAM register writes invalidate palette");
    const auto state=cpu.save_state();memory=cpu.ppu_memory_revision();vram=cpu.ppu_vram_revision();
    cpu.load_state(state);
    require(memory!=cpu.ppu_memory_revision() && vram!=cpu.ppu_vram_revision(),"State restore invalidates old-instance caches");
    require(cpu.save_state()==state,"Cache revisions do not change serialized source state");
    std::cout<<"Presentation PPU cache revisions: batch/register/restore checks passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
