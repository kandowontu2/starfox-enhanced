#include "starfox/platform/nintendo_3ds/game_hud_editor.hpp"
#include <filesystem>
#include <iostream>

using namespace starfox;
using namespace platform::nintendo_3ds;
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F f) {bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid HUD request accepted");}
Rgb at(ImageView v,unsigned x,unsigned y) {
    const auto offset=std::size_t(y)*v.pitch+x*3;
    return {v.pixels[offset],v.pixels[offset+1],v.pixels[offset+2]};
}
bool same(ImageView a,ImageView b) {return a.width==b.width && a.height==b.height && a.pitch==b.pitch && std::ranges::equal(a.pixels,b.pixels);}
void tap(GameHudEditor& editor,input::ButtonMask button) {editor.update({button});editor.update({});}
void choose(GameHudEditor& editor,unsigned id) {
    for(unsigned i=0;editor.selection()!=id && i<hud_widget_count;++i) tap(editor,input::right_shoulder);
    require(editor.selection()==id,"Cannot select every native HUD widget");
}
HudState sample() {
    HudState s;s.shield_percent=76;s.boost_percent=92;s.lives=2;s.bombs=3;s.boss_percent=67;
    s.ally_percent={84,58,95};s.second_shield_percent=61;s.second_counters=HudCounters{6,2};
    s.radio_message="SAMPLE RADIO\nHUD LAYOUT PREVIEW";return s;
}
void layout_contract() {
    constexpr std::array<std::array<unsigned,4>,13> expected{{
        {8,8,304,70},{123,99,74,74},{17,100,62,9},{241,100,62,9},{128,181,64,9},
        {12,200,96,21},{212,200,96,21},{119,214,82,21},{12,182,108,8},{212,182,108,8},
        {12,224,96,11},{12,164,108,8},{212,164,108,8}}};
    CockpitLayout layout;require(layout.valid() && hud_widget_count==13,"Default layout invalid or widgets omitted");
    for(unsigned id=0;id<13;++id) {
        const auto p=layout.widgets[id];
        require(p.x==expected[id][0] && p.y==expected[id][1] && p.quarters==4 && p.visible,"Default placement changed");
        for(unsigned q=2;q<=8;++q) {
            auto candidate=layout;auto& edit=candidate.widgets[id];edit={0,0,std::uint8_t(q),true};
            const unsigned w=(expected[id][2]*q+3)/4,h=(expected[id][3]*q+3)/4;
            require(candidate.valid()==(w<=320 && h<=240),"Layout accepted non-fitting resized panel");
            if(w>320 || h>240) continue;
            require(hud_widget_extent(id,edit)==std::array<unsigned,2>{w,h},"Panel extent rounding differs from independent oracle");
            edit.x=std::uint16_t(320-w);edit.y=std::uint16_t(240-h);
            require(candidate.valid(),"Exact right/bottom LCD edge rejected");
            ++edit.x;require(!candidate.valid(),"One-pixel horizontal overflow accepted");--edit.x;
            ++edit.y;edit.visible=false;require(!candidate.valid(),"Hidden panel bypassed LCD bounds");
        }
        for(unsigned q:{0U,1U,9U,255U}) {
            auto bad=layout;bad.widgets[id].quarters=std::uint8_t(q);require(!bad.valid(),"Invalid scale accepted");
        }
        HudPlacement bounded{65535,65535,255,false};bound_hud_placement(id,bounded);
        const auto extent=hud_widget_extent(id,bounded);
        auto valid=layout;valid.widgets[id]=bounded;
        require(valid.valid() && bounded.x==320-extent[0] && bounded.y==240-extent[1] && !bounded.visible,"Clamping changed visibility or failed at LCD edges");
    }
    rejects([&]{static_cast<void>(hud_widget_extent(13,{}));});
}
void masked_scaling_oracle() {
    constexpr unsigned width=4,height=3,pitch=16;
    std::vector<std::uint8_t> source(pitch*height,201);
    constexpr std::array<std::uint8_t,12> mask{1,0,1,1,0,1,0,1,1,1,0,0};
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const auto i=y*pitch+x*3;source[i]=std::uint8_t(x*17);source[i+1]=std::uint8_t(y*23);source[i+2]=std::uint8_t((x+y)*31);
    }
    const ImageView image{source,width,height,pitch};constexpr Rgb base{71,83,97};
    // Include clipping on every edge, noninteger quarters, padded source rows,
    // black opaque pixels and untouched holes. No production scale helper is
    // used to construct the expected pixel.
    for(unsigned q=2;q<=8;++q) for(int offset:{-3,-1,0,3,7}) {
        Canvas output(8,8);output.clear(base);output.scaled_artwork(offset,offset,image,mask,q);
        const int w=int((width*q+3)/4),h=int((height*q+3)/4);
        for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x) {
            Rgb expected=base;const int dx=int(x)-offset,dy=int(y)-offset;
            if(dx>=0 && dy>=0 && dx<w && dy<h) {
                const unsigned sx=unsigned(dx)*4/q,sy=unsigned(dy)*4/q;
                if(mask[sy*width+sx]) expected=at(image,sx,sy);
            }
            require(at(output.view(),x,y)==expected,"Masked nearest HUD scaling changed coverage/black pixel/padding");
        }
    }
    Canvas output(8,8);output.clear(base);const auto stable=std::vector(output.view().pixels.begin(),output.view().pixels.end());
    for(unsigned q:{0U,1U,9U}) rejects([&]{output.scaled_artwork(0,0,image,mask,q);});
    rejects([&]{output.scaled_artwork(0,0,image,std::span(mask).first(11),4);});
    auto short_image=image;short_image.pixels=std::span(source).first(5);rejects([&]{output.scaled_artwork(0,0,short_image,mask,4);});
    require(std::ranges::equal(stable,output.view().pixels),"Rejected artwork partly changed destination");
    Canvas panel(4,3);panel.begin_artwork();panel.rectangle(0,0,1,1,{0,0,0});
    require(panel.coverage()[0]==1 && panel.coverage()[1]==0,"Black HUD pixels mistaken for transparent colour key");
}
void isolated_widget_rendering() {
    CockpitWidgets widgets;require(widgets.bytes()==0,"Default HUD eagerly allocated custom widget panels");
    Canvas legacy,isolated;
    for(unsigned mode=0;mode<8;++mode) {
        auto s=sample();s.second_player_view=(mode&1)!=0;s.meters_enabled=(mode&2)==0;s.counters_enabled=(mode&4)==0;
        draw_cockpit(legacy,s);widgets.draw(isolated,s);
        require(same(legacy.view(),isolated.view()),"Isolated default panels changed original cockpit pixels/painter order");
    }
    const auto resident=widgets.bytes();std::size_t expected{};
    for(const auto size:hud_widget_sizes) expected+=size[0]*size[1]*4;
    require(resident==expected && resident==154'856,"Isolated RGB+coverage panels exceed exact fixed-widget storage");
    std::vector<std::uint8_t> portrait(196*64),radio(856*56);
    for(std::size_t i=0;i<portrait.size();++i) portrait[i]=std::uint8_t(i*71);
    for(std::size_t i=0;i<radio.size();++i) radio[i]=std::uint8_t(i*53);
    auto artwork=sample();artwork.portrait={portrait,64,64,196};artwork.radio_artwork={radio,284,56,856};
    draw_cockpit(legacy,artwork);widgets.draw(isolated,artwork);
    require(same(legacy.view(),isolated.view()),"Isolated panels changed opaque cartridge-style artwork/padded rows");
    auto s=sample();s.layout.widgets[unsigned(HudWidget::portrait)].x=100;
    CockpitDashboard dashboard;require(dashboard.update(s) && !dashboard.update(s),"Custom HUD ignored placement or redraw cache");
    const auto stable=std::vector(dashboard.view().pixels.begin(),dashboard.view().pixels.end());
    auto bad=s;bad.layout.widgets[0].x=320;rejects([&]{dashboard.update(bad);});
    require(std::ranges::equal(stable,dashboard.view().pixels),"Invalid layout partly changed published dashboard");
    bad=s;bad.portrait={{},65,64,195};rejects([&]{dashboard.update(bad);});
    require(std::ranges::equal(stable,dashboard.view().pixels),"Invalid portrait partly changed published dashboard");
    for(auto& p:s.layout.widgets) p.visible=false;
    widgets.draw(isolated,s);const auto background=std::vector(isolated.view().pixels.begin(),isolated.view().pixels.end());
    for(unsigned id=0;id<hud_widget_count;++id) {
        s.layout.widgets[id]={0,0,2,true};widgets.draw(isolated,s);
        require(!std::ranges::equal(background,isolated.view().pixels),"A movable HUD panel does not render independently");
        require(widgets.bytes()==resident,"Widget movement allocated another set of panels");
        s.layout.widgets[id].visible=false;
    }
    widgets.draw(isolated,s);require(std::ranges::equal(background,isolated.view().pixels),"Hidden HUD left ghost widget pixels");
}
void editor_controls() {
    GameHudEditor editor;require(!editor.active() && editor.frame().draws.empty(),"Inactive editor published UI");
    editor.open({});const auto initial=editor.layout();const auto redraws=editor.redraws();
    editor.update({input::start},{true,8,8});require(editor.active() && editor.layout()==initial,"Initiating press/touch leaked into editor");
    editor.update({});
    for(unsigned i=0;i<50;++i) editor.update({});
    require(editor.redraws()==redraws,"Idle editor regenerated preview");
    for(unsigned id=0;id<hud_widget_count;++id) {
        choose(editor,id);const auto before=editor.layout().widgets[id];
        tap(editor,input::left);tap(editor,input::up);
        require(editor.layout().widgets[id].x==std::max(0,int(before.x)-4)
            && editor.layout().widgets[id].y==std::max(0,int(before.y)-4),"D-pad did not move selected widget only");
        tap(editor,input::a|input::down);require(editor.layout().widgets[id].quarters==3,"A+Down did not resize quarter step");
        tap(editor,input::x);require(!editor.layout().widgets[id].visible,"X did not hide selected panel");
        tap(editor,input::x);require(editor.layout().widgets[id].visible && editor.layout().valid(),"X show or LCD extent invalid");
    }
    tap(editor,input::y);require(editor.layout()==initial,"Y did not restore all native placements");
    choose(editor,0);tap(editor,input::select|input::left);require(editor.layout().widgets[0].x==7,"Fine move did not use one LCD pixel");
    tap(editor,input::y);
    editor.update({}, {true,9,9});editor.update({}, {true,319,239});
    require(editor.layout().widgets[0].x==16 && editor.layout().widgets[0].y==170,"Touch drag failed to cover/clamp full LCD");
    editor.update({input::right_shoulder},{true,300,220});require(editor.selection()==1,"Shoulder could not select another widget during drag");
    const auto stopped=editor.layout();editor.update({}, {true,150,150});
    require(editor.layout()==stopped && editor.selection()==1,"Old drag anchor transferred to new selection");
    editor.update({});editor.update({}, {true,124,100});editor.update({}, {true,164,140});
    require(editor.layout().widgets[1].x==163 && editor.layout().widgets[1].y==139,"Fresh portrait drag did not preserve touch offset");
    editor.update({}, {true,-1,400});const auto outside=editor.layout();
    require(outside.valid(),"Outside-LCD touch produced invalid placement");
    editor.suspend();editor.update({input::start},{true,150,150});
    require(editor.active() && editor.layout()==outside,"Home resume accepted held Apply/touch");editor.update({});
    auto invalid=editor.layout();invalid.widgets[0].quarters=9;rejects([&]{editor.open(invalid);});
    require(editor.active() && editor.layout()==outside,"Malformed open replaced active editor");
    validate_pica_frame(editor.frame(),editor.lower_view());
    require(!editor.frame().plan.stereo && editor.frame().plan.eye_count==1 && editor.frame().draws[0].source_layer==0
        && !editor.frame().draws[0].depth_test && !editor.frame().draws[0].depth_write,"Editor acquired world depth/colour effects");
    tap(editor,input::start);require(!editor.active() && editor.applied() && editor.frame().draws.empty(),"Apply did not close editor");
    editor.open(outside);editor.update({});tap(editor,input::y);tap(editor,input::b);
    require(!editor.active() && !editor.applied(),"Cancel was reported as a layout commit");
}
}
int main(int argc,char** argv) try {
    layout_contract();masked_scaling_oracle();isolated_widget_rendering();editor_controls();
    if(argc==2) {
        const std::filesystem::path directory(argv[1]);std::filesystem::create_directories(directory);
        GameHudEditor editor;editor.open({});editor.update({});
        Canvas upper(top_width),lower;upper.image(0,0,editor.upper_view());lower.image(0,0,editor.lower_view());
        upper.write_bmp((directory/"hud-editor-upper.bmp").string());lower.write_bmp((directory/"hud-editor-lower.bmp").string());
        choose(editor,unsigned(HudWidget::shield));tap(editor,input::a|input::up);tap(editor,input::left);tap(editor,input::up);
        upper.image(0,0,editor.upper_view());lower.image(0,0,editor.lower_view());
        upper.write_bmp((directory/"hud-editor-custom-upper.bmp").string());lower.write_bmp((directory/"hud-editor-custom-lower.bmp").string());
    }
    std::cout<<"3DS native HUD layout/editor: "<<checks<<" checks passed; LCD pixel/control/storage policies, not device acceptance\n";
} catch(const std::exception& error) {std::cerr<<"3DS HUD editor: "<<error.what()<<'\n';return 1;}
