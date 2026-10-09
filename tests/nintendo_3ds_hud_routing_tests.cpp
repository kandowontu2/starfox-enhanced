#include "starfox/render/sprite_renderer.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace starfox;
void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
void object(simulation::SnesPpuState& ppu,unsigned number,int x,unsigned y,unsigned tile,unsigned priority=2) {
    const auto low=number*4,high=512+number/4,shift=(number%4)*2;
    ppu.oam[low]=std::uint8_t(x);ppu.oam[low+1]=std::uint8_t(y);ppu.oam[low+2]=std::uint8_t(tile);
    ppu.oam[low+3]=std::uint8_t((tile>>8)|(priority<<4));
    ppu.oam[high]=std::uint8_t((ppu.oam[high]&~(3U<<shift))|((x<0?1U:0U)<<shift));
    const unsigned base=(tile&255U)*32;
    for(unsigned row=0;row<8;++row) ppu.vram[base+row*2]=0xFF; // Opaque 4bpp colour 1.
}
void test(unsigned scale,bool commands) {
    simulation::SnesPpuState ppu;ppu.object_select=0;
    object(ppu,0,10,10,189);    // Retail lives.
    object(ppu,1,20,178,226);   // EX relocated lives, not shield.
    object(ppu,2,40,170,4);     // Shield.
    object(ppu,3,190,176,5);    // Bombs/boost.
    object(ppu,4,24,138,6);     // Comms source artwork.
    object(ppu,5,200,3,0xF1);  // Enemy label, real tile bank retained.
    object(ppu,6,180,85,7);    // World OBJ / explosion in the same source pass.
    object(ppu,7,100,180,0x61); // Reticle legitimately inside bottom HUD band.
    object(ppu,8,119,184,0x3E); // Vertical warning, not a HUD panel.
    const auto original=ppu;
    render::Framebuffer all(256,224,scale),world(256,224,scale),hud(256,224,scale),legacy(256,224,scale);
    render::RasterCommands all_commands,world_commands,hud_commands,legacy_commands;
    if(commands) {
        all_commands.reset(256*scale,224*scale);all.record_to(&all_commands);
        world_commands.reset(256*scale,224*scale);world.record_to(&world_commands);
        hud_commands.reset(256*scale,224*scale);hud.record_to(&hud_commands);
        legacy_commands.reset(256*scale,224*scale);legacy.record_to(&legacy_commands);
    }
    const render::SpriteRenderer renderer;
    renderer.draw_objects(ppu,all);
    renderer.draw_objects(ppu,world,std::nullopt,0,true,false,nullptr,false,nullptr,render::SpriteSelection::world_only);
    renderer.draw_objects(ppu,hud,std::nullopt,0,true,false,nullptr,false,nullptr,render::SpriteSelection::configurable_hud_only);
    renderer.draw_objects(ppu,legacy,std::nullopt,0,true,false,nullptr,true);
    require(ppu==original,"HUD routing modified cartridge VRAM/OAM");
    if(commands) {
        require(world_commands.commands.size()==3 && hud_commands.commands.size()==6,"Recorded OBJ split lost sprites");
        require(world_commands.commands.size()+hud_commands.commands.size()==all_commands.commands.size(),"Recorded source pass not partitioned");
        require(world_commands.commands.size()==legacy_commands.commands.size(),"Existing suppression path changed");
        return;
    }
    for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x) {
        require(!(world.get(x,y) && hud.get(x,y)),"Nonoverlapping test sprites duplicated between LCDs");
        require(all.get(x,y)==std::max(world.get(x,y),hud.get(x,y)),"HUD partition lost source pixels");
        require(world.get(x,y)==legacy.get(x,y),"Existing HUD suppression changed");
    }
    for(const auto coordinate:{std::array<unsigned,2>{180,85},{100,180},{119,184}})
        require(world.get(coordinate[0],coordinate[1]) && !hud.get(coordinate[0],coordinate[1]),"World explosion/reticle moved into HUD");
    for(const auto coordinate:{std::array<unsigned,2>{10,10},{20,178},{40,170},{190,176},{24,138},{200,4}})
        require(hud.get(coordinate[0],coordinate[1]) && !world.get(coordinate[0],coordinate[1]),"HUD source artwork retained on top");
    render::Framebuffer wrong_priority(256,224,scale);
    renderer.draw_objects(ppu,wrong_priority,0,0,true,false,nullptr,false,nullptr,render::SpriteSelection::configurable_hud_only);
    require(std::all_of(wrong_priority.pixels().begin(),wrong_priority.pixels().end(),[](auto c){return !c;}),"HUD extraction ignored OBJ priority");
}
} // namespace
int main() {
    try {
        for(unsigned scale:{1U,2U}) {test(scale,false);test(scale,true);}
        std::cout<<"3DS HUD routing: source pixels, recorded commands, priorities, reticles and world effects passed\n";
    } catch(const std::exception& error) {
        std::cerr<<"3DS HUD routing regression: "<<error.what()<<'\n';return 1;
    }
}
