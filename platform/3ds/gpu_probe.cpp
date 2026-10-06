// Native PICA/texture/depth/slider check only. This is not the game entry point.
#include "native_display.hpp"
#include "native_gpu.hpp"
#include "starfox/platform/nintendo_3ds/pica_shapes.hpp"
#include "starfox/platform/nintendo_3ds/pica_raster.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include "starfox/platform/nintendo_3ds/pica_window.hpp"
#include "starfox/platform/nintendo_3ds/pica_colour.hpp"
#include "pica_scene_shader.hpp"

namespace {
using namespace starfox::platform::nintendo_3ds;
void quad(std::vector<PicaVertex>& vertices,const std::array<Point3,4>& points,
    std::array<float,4> colour={1,1,1,1}) {
    constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) vertices.push_back({points[corner],colour,uv[corner]});
}
starfox::assets::Shape source_cube() {
    constexpr int s=48;
    starfox::assets::Shape shape;shape.vertices={{-s,-s,-s},{s,-s,-s},{s,s,-s},{-s,s,-s},
        {-s,-s,s},{s,-s,s},{s,s,s},{-s,s,s}};
    constexpr unsigned faces[][4]={{0,1,2,3},{4,7,6,5},{0,4,5,1},{3,2,6,7},{0,3,7,4},{1,5,6,2}};
    shape.colour_words={0x4001,0xc00b,0x0053,0xc004,0xc009,0xc008};
    for(unsigned face=0;face<6;++face) {
        starfox::assets::Face source;source.visibility_index=-1;source.colour_id=face;
        for(auto vertex:faces[face]) source.vertex_indices.push_back(vertex);
        shape.faces.push_back(source);
    }
    starfox::assets::TextureImage art;art.descriptor=0x4001;art.u_mask=art.v_mask=7;
    art.coordinates={{{0,0},{8,0},{8,8},{0,8}}};art.texels.resize(64);
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) art.texels[y*8+x]=((x/2+y/2)&1)?5:15;
    shape.textures.push_back(std::move(art));return shape;
}
starfox::assets::Shape source_span_face() {
    starfox::assets::Shape shape;
    shape.vertices={{-40,-40,-24},{40,-40,24},{40,40,24},{-40,40,-24}};
    shape.faces.push_back({-1,14,{0,0,0},{0,1,2,3}});
    return shape;
}
int diagnostic(NativeDisplay& display) {
    NativeGpu gpu(pica_scene_shader); // Destroy/sync the GPU BEFORE gfxExit.
    Canvas caption(top_width);CockpitDashboard lower;
    const auto shape=source_cube();
    const auto span_face=source_span_face();
    constexpr std::array<const char*,8> span_names{
        "SOLID","WIREFRAME 1","WIREFRAME 2","WOBBLE 1","WOBBLE 2","WOBBLE 3","WAVE","CEL"};
    const std::array<starfox::render::Rgba8,16> colours{{{0,0,0},{48,48,60},{112,28,32},{38,70,150},
        {188,72,24},{24,130,132},{82,20,26},{35,52,120},{118,46,132},{38,110,52},
        {82,82,92},{118,118,128},{156,156,166},{202,202,210},{238,238,242},{255,255,255}}};
    starfox::render::SoftwareRenderer source_renderer;PicaShapes source_geometry;
    PicaRaster source_raster,source_objects;PicaComposite compositor;PicaWindow source_window;
    PicaColourEffects source_colour;
    auto sky=std::make_shared<starfox::simulation::SnesPpuState>();
    sky->main_screen=18;sky->bg2_screen_size=0;
    sky->bg2_character_base=0x4000;sky->bg2_screen_base=0x6000;
    sky->cgram[1]=uint16_t(7|(17<<5)|(28<<10));sky->cgram[2]=uint16_t(25|(28<<5)|(31<<10));
    for(unsigned row=0;row<8;++row) {
        sky->vram[0x8000+row*2]=(row==4)?0:255;
        sky->vram[0x8000+row*2+1]=(row==4)?255:0;
    }
    // Opaque synthetic OBJ overlaps the circle/blackfade on the upper LCD.
    // Those source masks exclude OBJ: its red ink should remain unchanged.
    sky->cgram[129]=31;
    for(unsigned i=0;i<128;++i) sky->oam[i*4+1]=240;
    sky->oam[0]=148;sky->oam[1]=104;sky->oam[2]=7;sky->oam[3]=0x20;
    for(unsigned row=0;row<8;++row) sky->vram[7*32+row*2]=255;
    // Synthetic SNES tiles, not cartridge assets or a gameplay backdrop.
    PpuBatch sky_batch{{{PpuLayer::bg2}},PicaSpace::scenery,true,0,208};
    std::vector<PicaVertex> vertices;
    std::vector<PicaDraw> draws;std::vector<PicaImage> images;
    std::vector<PicaVertex> alpha_vertices;std::vector<PicaDraw> alpha_draws;
    bool setup=true,caption_dirty=true,wipe_demo=false,colour_demo=false;
    unsigned wipe_phase{},colour_phase{},span_mode{},animation_phase{};float x{},y{};
    starfox::input::ButtonMask previous{};
    StereoSettings settings;settings.near_plane=16;
    while(true) {
        const auto input=display.poll();if(!input.running) return 0;
        if((input.held&(starfox::input::select|starfox::input::start))==(starfox::input::select|starfox::input::start)) return 0;
        if((input.held&starfox::input::a) && !(previous&starfox::input::a)) {setup=false;caption_dirty=true;}
        if((input.held&starfox::input::b) && !(previous&starfox::input::b)) {setup=true;caption_dirty=true;}
        if(!setup && (input.held&starfox::input::x) && !(previous&starfox::input::x)) wipe_demo=!wipe_demo;
        if(!setup && (input.held&starfox::input::y) && !(previous&starfox::input::y)) colour_demo=!colour_demo;
        if(!setup && (input.held&starfox::input::right_shoulder) && !(previous&starfox::input::right_shoulder)) {
            span_mode=(span_mode+1)%span_names.size();caption_dirty=true;
        }
        if(!setup && (input.held&starfox::input::left_shoulder) && !(previous&starfox::input::left_shoulder)) {
            span_mode=(span_mode+span_names.size()-1)%span_names.size();caption_dirty=true;
        }
        previous=input.held;
        if(caption_dirty) {
            caption.clear({8,15,28});
            caption.text(12,12,"PICA200 GPU CHECK / NOT THE GAME",{183,224,240});
            if(setup) caption.text(24,56,"A: SOURCE MODELS / DEPTH / SLIDER\n\nB: RETURN TO THIS PAGE\nCIRCLE PAD: MOVE FRONT CUBE\nL/R: EX SPAN MODE\n\nSELECT + START: EXIT\n\nREAL PRE-GAME MENU IS RETAINED\nIN THE SEPARATE GAME PORT",{227,235,242});
            else {
                caption.text(8,211,std::string("EX SPANS: ")+span_names[span_mode],{213,237,244});
                caption.text(8,228,"L/R: SPANS / X: WIPE / Y: COLOUR",{213,237,244});
            }
            caption_dirty=false;
        }
        if(!setup) {
            x=std::clamp(x+2*((input.held&starfox::input::right)!=0)-2*((input.held&starfox::input::left)!=0),-160.F,160.F);
            y=std::clamp(y+2*((input.held&starfox::input::up)!=0)-2*((input.held&starfox::input::down)!=0),-96.F,96.F);
        }
        const auto top=caption.view();
        const auto plan=plan_frame(input.slider,input.stereoscopic_hardware,setup?ScreenUse::setup:ScreenUse::world,settings);
        vertices.clear();draws.clear();images.clear();source_geometry.clear();
        quad(vertices,{{{0,0,0},{float(top_width),0,0},{float(top_width),float(screen_height),0},{0,float(screen_height),0}}});
        draws.push_back({0,6,0,pica_identity,PicaSpace::screen,false,false,false});
        draws.back().source_layer=0; // Synthetic caption is host UI, not a SNES background.
        images.push_back({top.pixels,top.width,top.height,top.pitch,3});
        std::vector<PicaFrame> groups{{plan,vertices,draws,images}};
        if(!setup) {
            groups.push_back(source_raster.prepare(sky,sky_batch,plan));
            starfox::render::RenderPose pose;pose.vanish_x=128;pose.vanish_y=112;
            pose.x=x;pose.y=-y;pose.z=300;pose.continuous_geometry=true;
            source_geometry.append(source_renderer.prepare_primitives(shape,pose),colours);
            pose.x=32;pose.y=-24;pose.z=650;
            source_geometry.append(source_renderer.prepare_primitives(shape,pose),colours);
            pose.x=-84;pose.y=20;pose.z=280;pose.animation_frame=animation_phase++;
            pose.wireframe_mode=std::uint8_t(span_mode==1?1:span_mode==2?2:0);
            pose.wobble_mode=std::uint8_t(span_mode>=3 && span_mode<=5?span_mode-2:0);
            pose.wave_mode=span_mode==6;pose.cel_mode=span_mode==7;
            source_geometry.append(source_renderer.prepare_primitives(span_face,pose),colours,{128,112},&plan);
            const auto converted=source_geometry.frame(plan);
            groups.push_back(converted);
            // The separate translucent pass still exercises depth/alpha state.
            alpha_vertices.clear();alpha_draws.clear();
            quad(alpha_vertices,{{{-75,-60,450},{75,-60,450},{75,60,450},{-75,60,450}}},{.2F,.8F,1,.35F});
            alpha_draws.push_back({0,6,pica_no_texture,pica_identity,PicaSpace::world,true,false,true});
            alpha_draws.back().clip=PicaClip{185,0,225,240}; // Exercise actual per-draw PICA scissor state.
            groups.push_back({plan,alpha_vertices,alpha_draws,{}});
            groups.push_back(source_objects.prepare(sky,PpuBatch{{{PpuLayer::objects}}},plan));
            starfox::simulation::CircleEffectState circle;circle.active=colour_demo;
            circle.centre_x=128;circle.centre_y=112;circle.radius=std::uint16_t(8+(colour_phase%192));
            circle.red=31;circle.green=14;circle.blue=5;circle.affected_layers=3; // BG1 models + BG2 scenery, not OBJ.
            starfox::simulation::ColourMathEffectState math;math.active=colour_demo;
            math.subtract=true;math.affected_layers=0x2f;
            math.red=math.green=math.blue=std::uint8_t((colour_phase++/8)%32);
            groups.push_back(source_colour.prepare(circle,math,15,plan));
            starfox::simulation::WindowWipeState wipe;wipe.active=wipe_demo;wipe.horizontal_opening=true;
            const auto opening=std::abs(192-int(wipe_phase++%384));
            wipe.opening_top=(192-opening)*.5;wipe.opening_bottom=192-wipe.opening_top;
            groups.push_back(source_window.prepare(wipe,plan,WindowCoverage::full_scene));
        }
        HudState hud;hud.lives=2;hud.bombs=3;hud.shield_percent=76;hud.boost_percent=92;
        hud.ally_percent={84,58,95};hud.radio_message="SYNTHETIC GPU CHECK / NO GAME DATA";
        lower.update(hud);
        const auto frame=compositor.prepare(plan,groups,lower.view());gpu.present(frame,lower.view());
    }
}
}
int main() {
    NativeDisplay display;
    try {return diagnostic(display);}
    catch(const std::exception& error) {
        // The failed presenter has already finalized before CPU LCD output.
        Canvas screen(top_width),lower(bottom_width);
        screen.clear({17,25,38});screen.text(16,20,"PICA200 STARTUP / RENDER ERROR",{239,90,99});
        screen.text(16,48,error.what(),{227,235,242},1,368,156);
        screen.text(16,220,"SELECT + START: EXIT",{183,224,240});
        lower.clear({17,25,38});lower.text(12,24,"NATIVE GPU CHECK FAILED\n\nTHIS IS NOT A GAME BUILD",{227,235,242});
        const auto mono=plan_frame(0,false,ScreenUse::setup);
        while(true) {
            const auto input=display.poll();if(!input.running) break;
            if((input.held&(starfox::input::select|starfox::input::start))==(starfox::input::select|starfox::input::start)) break;
            display.present(mono,screen.view(),{},lower.view());
        }
        return 1;
    }
}
