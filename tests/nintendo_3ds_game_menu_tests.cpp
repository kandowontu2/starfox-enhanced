#include "starfox/platform/nintendo_3ds/game_menu.hpp"
#include "starfox/platform/nintendo_3ds/settings_reset.hpp"
#include <iostream>
#include <limits>

using namespace starfox;
using namespace starfox::platform::nintendo_3ds;
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F run) {
    bool rejected=false;try {run();} catch(const std::exception&) {rejected=true;}
    require(rejected,"Malformed menu/ownership request accepted");
}
assets::RomImage public_font_fixture() {
    // Deliberately synthetic glyphs, not copied cartridge assets. The actual
    // ScaledTextRenderer still decodes its ordinary ROM width/translation data.
    std::vector<std::uint8_t> bytes(0x8000);
    for(unsigned i=0;i<96;++i) {
        bytes[0x100+i]=6;bytes[0x200+i]=std::uint8_t(i);
        for(unsigned row=0;row<12;++row) {bytes[0x300+i*24+row*2]=0xfc;bytes[0x300+i*24+row*2+1]=0;}
    }
    return assets::RomImage(std::move(bytes));
}
void settings_reset_contract() {
    constexpr auto chord=input::ButtonMask(input::left_shoulder|input::right_shoulder);
    SettingsResetHold hold;
    require(!hold.update(true,chord,0) && hold.active(),"Zero uptime did not arm mapped L+R");
    require(!hold.update(true,chord,4'999'999'999LL),"Settings reset fired before five seconds");
    require(hold.update(true,chord,5'000'000'000LL),"Settings reset did not fire at five seconds");
    require(!hold.update(true,chord,5'000'000'000LL) && !hold.update(true,chord,15'000'000'000LL),"Held chord repeated settings reset");
    require(!hold.update(true,input::left_shoulder,15'000'000'001LL) && !hold.active(),"One shoulder release failed to cancel");
    require(!hold.update(true,chord,16'000'000'000LL) && hold.update(true,chord,21'000'000'000LL),"Released chord could not be rearmed");
    hold.cancel();require(!hold.active() && hold.elapsed()==0,"Home/sleep cancellation retained reset progress");
    for(unsigned fps:{20U,30U,60U,120U,240U,480U}) {
        hold.cancel();require(!hold.update(true,chord,0),"Frame-rate fixture fired on first sample");
        for(unsigned frame=1;frame<5*fps;++frame)
            require(!hold.update(true,chord,std::int64_t(frame)*1'000'000'000/fps),"Frame count changed the real-time reset threshold");
        require(hold.update(true,chord,SettingsResetHold::duration),"Frame rate delayed the real-time reset threshold");
    }
    hold.cancel();hold.update(true,chord,0);
    require(!hold.update(false,chord,9'000'000'000LL) && !hold.active(),"Gameplay allowed menu settings reset");
    hold.update(true,chord,10'000'000'000LL);hold.update(false,chord,14'999'999'999LL);
    require(!hold.update(true,chord,15'000'000'000LL) && hold.elapsed()==0,"Leaving/re-entering setup retained an old hold");
    require(!hold.update(true,0,16'000'000'000LL) && !hold.active(),"Mapped release failed to clear hold");
    hold.update(true,chord,20'000'000'000LL);
    require(!hold.update(true,chord,0) && !hold.active(),"Rewound host clock completed an old hold");
    require(!hold.update(true,chord,-1) && !hold.active(),"Invalid clock sample armed reset");
    constexpr auto physical_shoulders=(1U<<8)|(1U<<9);
    require(!hold.update(true,buttons(physical_shoulders),0)
        && hold.update(true,buttons(physical_shoulders),SettingsResetHold::duration),"Nintendo shoulders did not reach mapped in-game L+R");
    for(auto wrong:{input::ButtonMask(input::a|input::b),input::ButtonMask(input::x|input::y),input::ButtonMask(input::select|input::start)}) {
        hold.cancel();hold.update(true,wrong,0);
        require(!hold.update(true,wrong,SettingsResetHold::duration) && !hold.active(),"Unrelated actions triggered settings reset");
    }
    hold.cancel();const auto far=std::numeric_limits<std::int64_t>::max();
    hold.update(true,chord,far-SettingsResetHold::duration);
    require(hold.update(true,chord,far),"Long uptime overflowed the hold timer");
}
void menu_capability_chords() {
    using simulation::PregamePage;
    const auto enabled=[](PregamePage page,unsigned id,bool runtime) {
        const auto in=[&](std::initializer_list<unsigned> ids){return std::find(ids.begin(),ids.end(),id)!=ids.end();};
        switch(page) {
        case PregamePage::main:return (id==0 && !runtime) || in({1,2,14,15,16,20,21,47});
        case PregamePage::options:return in({0,1,3,5,6,7,8,9,11,12});
        case PregamePage::cheats:return true;
        case PregamePage::two_d:case PregamePage::three_d:case PregamePage::global:return id==23;
        case PregamePage::stereo:return in({1,2,4,5});
        }
        throw std::runtime_error("Invalid menu test page");
    };
    // Every source-owned row and wrap, both runtime/setup contexts, all action
    // buttons, and Up+Down priority. Held/released/navigation/Start must remain
    // unchanged; a disabled target cannot mutate ignored graphics settings.
    unsigned blocked=0,allowed=0;
    constexpr std::array<input::ButtonMask,4> navigation{0,input::up,input::down,input::ButtonMask(input::up|input::down)};
    constexpr std::array<input::ButtonMask,6> actions{input::a,input::select,input::left,input::right,input::b,
        input::ButtonMask(input::a|input::select|input::left|input::right|input::b)};
    for(auto page:{PregamePage::main,PregamePage::options,PregamePage::cheats,PregamePage::two_d,
        PregamePage::three_d,PregamePage::global,PregamePage::stereo}) {
        const auto order=simulation::pregame_menu_order(page);
        for(bool runtime:{false,true})for(std::size_t row=0;row<order.size();++row)
            for(auto nav:navigation)for(auto action:actions) {
                    const auto target=nav?order[(row+((nav&input::up)?order.size()-1U:1U))%order.size()]:order[row];
                    input::TickInput original;original.held=input::ButtonMask(nav|action|input::start);
                    original.pressed=original.held;original.released=input::x;
                    auto expected=original;
                    if(!enabled(page,target,runtime)) {
                        auto mask=input::ButtonMask(input::a|input::select|input::left|input::right);
                        if(page==PregamePage::main)mask|=input::b;
                        expected.pressed=input::ButtonMask(expected.pressed&~mask);++blocked;
                    }else ++allowed;
                    const auto actual=GameMenu::filter(page,order[row],runtime,original);
                    require(actual.held==expected.held && actual.pressed==expected.pressed
                        && actual.released==expected.released,
                        "Same-tick native menu navigation bypassed capability gate or changed source input semantics");
                }
    }
    require(blocked && allowed,"Menu capability test lacks disabled and enabled destination coverage");
}
}
int main() try {
    settings_reset_contract();
    menu_capability_chords();
    const auto rom=public_font_fixture();
    const auto symbols=assets::SymbolMap::parse("MSCALECHARS $008000\nMARIOMSGS $008020\nFONT0WID $008100\nFONT0TRN $008200\nFONT0FON $008300\nFACEDATA $009000\n");
    GameMenu menu(rom,symbols);
    std::vector<std::uint8_t> lower(bottom_width*screen_height*3);
    const ImageView dashboard{lower,bottom_width,screen_height,bottom_width*3};
    const auto plan=plan_frame(0,false,ScreenUse::setup);
    GameMenuState state;state.visible=true;state.title="STAR FOX ENHANCED";
    for(auto id:simulation::pregame_menu_order(simulation::PregamePage::main)) state.rows.push_back({id,"ROW","VALUE",true});
    state.selection=state.rows.front().id;
    require(menu.update(state),"Initial actual menu was not drawn");
    require(!menu.update(state) && menu.redraws()==1,"Unchanged setup redrew every frame");
    require(valid_image(menu.plain_view(),top_width,screen_height),"Plain setup LCD image invalid");
    Canvas upper(top_width);upper.image(0,0,menu.plain_view());
    require(std::equal(upper.view().pixels.begin(),upper.view().pixels.end(),menu.plain_view().pixels.begin()),"Upper menu capture changed source font pixels");
    Canvas lower_canvas;rejects([&]{lower_canvas.image(0,0,menu.plain_view());});
    auto frame=menu.frame(plan);validate_pica_frame(frame,dashboard);
    require(frame.vertices.size()==6 && frame.draws.size()==1 && frame.textures.size()==1,"Plain setup prepared a world scene");
    require(frame.draws[0].space==PicaSpace::screen && !frame.draws[0].depth_test
        && !frame.draws[0].depth_write && frame.draws[0].source_layer==0,"Setup joined source depth/colour math");
    require(pica_resident_texture_bytes(frame.textures[0])==512*256*4,"Setup texture size exceeds native budget");
    for(std::size_t i=3;i<frame.textures[0].pixels.size();i+=4) require(frame.textures[0].pixels[i]==255,"Plain setup leaked background scene pixels");
    const auto pixels=std::vector<std::uint8_t>(frame.textures[0].pixels.begin(),frame.textures[0].pixels.end());
    for(float slider:{0.F,.25F,1.F,0.F}) {
        const auto next=menu.frame(plan_frame(slider,true,ScreenUse::menu_preview));
        validate_pica_frame(next,dashboard);
        require(std::equal(next.textures[0].pixels.begin(),next.textures[0].pixels.end(),pixels.begin()),"Slider changed menu pixels");
        for(unsigned eye=0;eye<next.plan.eye_count;++eye)
            require(pica_draw_matrix(next.plan,eye,next.draws[0])==pica_screen_matrix(top_width),"Screen menu acquired stereo disparity");
    }
    // Preserve the full shared 80-label range, including the merged asteroid
    // row. Constructor validation above also covers the real source page IDs.
    const auto main_state=state;state.page=simulation::PregamePage::three_d;state.rows.clear();
    for(unsigned id=0;id<80;++id) state.rows.push_back({std::uint8_t(id),"ROW","UNAVAILABLE",false});
    state.selection=79;require(menu.update(state),"Full source label range rejected");
    validate_pica_frame(menu.frame(plan),dashboard);
    require(menu.state().selection==79 && menu.state().rows.size()==80,"Merged source row was dropped or relabeled");
    auto too_many=state;too_many.rows.push_back({80,"INVALID","",false});
    rejects([&]{menu.update(too_many);});require(menu.state()==state,"Oversized source page partly replaced menu");
    state=main_state;menu.update(state);
    state.preview=true;require(menu.update(state),"Preview UI opacity did not update");
    frame=menu.frame(plan_frame(1,true,ScreenUse::menu_preview));
    validate_pica_frame(frame,dashboard);rejects([&]{static_cast<void>(menu.plain_view());});
    const auto at=[&](unsigned x,unsigned y){return frame.textures[0].pixels[(std::size_t(y)*top_width+x)*4+3];};
    require(at(0,239)==0 && at(200,233)==190 && at(8,20)==255,"Preview panel/text/background coverage wrong");
    for(const auto page:{simulation::PregamePage::main,simulation::PregamePage::options,simulation::PregamePage::two_d,
        simulation::PregamePage::three_d,simulation::PregamePage::cheats,simulation::PregamePage::global,simulation::PregamePage::stereo}) {
        state.page=page;state.rows.clear();state.preview=false;
        for(auto id:simulation::pregame_menu_order(page)) state.rows.push_back({id,"ROW","UNAVAILABLE",false});
        for(const auto& row:state.rows) {
            state.selection=row.id;menu.update(state);validate_pica_frame(menu.frame(plan),dashboard);
            require(menu.state().rows==state.rows && menu.state().selection==row.id,"Scrolling replaced/reordered the actual source menu");
        }
    }
    for(unsigned language=0;language<6;++language) {
        state.language=std::uint8_t(language);state.title="OPTIONS";
        menu.update(state);validate_pica_frame(menu.frame(plan),dashboard);
    }
    const auto saved=menu.state();auto bad=saved;bad.selection=255;
    rejects([&]{menu.update(bad);});require(menu.state()==saved,"Invalid snapshot partly replaced menu");
    bad=saved;bad.rows.push_back(bad.rows.front());rejects([&]{menu.update(bad);});
    bad=saved;bad.language=6;rejects([&]{menu.update(bad);});
    state.visible=false;menu.update(state);
    frame=menu.frame(plan);
    require(frame.vertices.empty() && frame.draws.empty() && frame.textures.empty(),"Menu remained over cartridge flow");
    rejects([&]{static_cast<void>(menu.plain_view());});
    std::cout<<"3DS actual menu renderer: "<<checks<<" checks passed (synthetic public font/layout, not cartridge/device acceptance)\n";
} catch(const std::exception& error) {std::cerr<<"3DS actual menu renderer: "<<error.what()<<'\n';return 1;}
