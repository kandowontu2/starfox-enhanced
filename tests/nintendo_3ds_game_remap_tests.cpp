#include "starfox/platform/nintendo_3ds/game_remap.hpp"
#include <filesystem>
#include <iostream>

using namespace starfox;
using namespace platform::nintendo_3ds;
namespace {
unsigned checks{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F action) {bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid mapping/editor accepted");}
void tap(GameRemap& editor,input::ButtonMask button) {editor.update({button});editor.update({});}
void choose(GameRemap& editor,unsigned row) {
    for(unsigned i=0;editor.selection()!=row && i<13;++i) tap(editor,input::down);
    require(editor.selection()==row,"Fixed navigation lost selected action");
}
void capture(GameRemap& editor,PadSample pad) {
    tap(editor,input::a);require(editor.capturing(),"Physical A did not open assignment capture");
    editor.update(pad);require(editor.capturing(),"Binding committed before release");
    editor.update({});require(!editor.capturing(),"Binding did not commit on release");
}
void mapping_contracts() {
    GameBindings map;require(map.valid(),"Default Nintendo map invalid");
    // Independent native printed-position oracle, not the production arrays.
    constexpr std::array<input::ButtonMask,12> expected{input::up,input::down,input::left,input::right,
        input::a,input::b,input::x,input::y,input::left_shoulder,input::right_shoulder,input::select,input::start};
    for(auto physical:expected) require(mapped_buttons(map,{physical})==physical,"Default mapping changed a printed Nintendo action");
    for(int direction=0;direction<4;++direction) {
        PadSample pad;if(direction==0) pad.circle_y=80;if(direction==1) pad.circle_y=-80;
        if(direction==2) pad.circle_x=-80;
        if(direction==3) pad.circle_x=80;
        require(mapped_buttons(map,pad)==expected[direction],"Default Circle Pad orientation changed");
        require(fixed_menu_buttons(pad)==expected[direction],"Fixed Circle Pad menu orientation changed");
    }
    require(mapped_buttons(map,{0,40,-40})==0,"Circle Pad threshold no longer strict");
    map.sources[0]=8;require(mapped_buttons(map,{input::a})==(input::a|input::up),"Duplicate physical binding lost an action");
    require(mapped_buttons(map,{0,0,80})==0,"Rebound direction retained hidden old Circle Pad source");
    map.sources[0]=12;require(mapped_buttons(map,{0,0,80})==input::up,"Explicit Circle Pad binding failed");
    map.sources[4]=GameBindings::unbound;require(mapped_buttons(map,{input::a})==0,"Cleared A binding still fired");
    require(fixed_menu_buttons({input::a})==input::a,"Cleared gameplay A removed fixed menu recovery");
    map.sources[8]=8;map.sources[9]=0;
    const auto custom=mapped_buttons(map,{input::a|input::b});
    require((custom&(input::left_shoulder|input::right_shoulder))==(input::left_shoulder|input::right_shoulder),"Physical face buttons could not map to in-game L+R");
    map.deadzone=80;require(!mapped_buttons(map,{0,79,79}),"Configured deadzone ignored");
    map.deadzone=157;require(!map.valid(),"Unbounded deadzone accepted");rejects([&]{mapped_buttons(map,{});});
    map=GameBindings{};map.sources[0]=16;require(!map.valid(),"Unknown physical source accepted");rejects([&]{mapped_buttons(map,{});});
    rejects([]{physical_sources({},157);});rejects([]{binding_name(16);});
    require(binding_name(255)=="UNBOUND","Cleared-source label wrong");
}
void editor_contracts(const std::filesystem::path& captures) {
    GameRemap editor;require(!editor.active() && editor.frame().vertices.empty(),"Inactive editor drew a replacement menu");
    GameBindings invalid;invalid.sources[0]=16;rejects([&]{editor.open(invalid);});
    editor.open({});require(editor.active() && editor.selection()==0 && !editor.capturing(),"Controller editor did not start at Up");
    editor.update({input::a});editor.update({input::a});require(!editor.capturing(),"Opening A leaked into first binding");editor.update({});
    const auto first=editor.redraws();for(unsigned i=0;i<60;++i) require(!editor.update({}),"Idle controller editor redrew every frame");
    require(editor.redraws()==first,"Idle controller texture regenerated");
    for(unsigned row=0;row<12;++row) {
        choose(editor,row);require(editor.highlighted_action()==game_action_bits[row],"Diagram highlights physical binding rather than logical SNES action");
        const auto frame=editor.frame();validate_pica_frame(frame,editor.lower_view());
        require(frame.plan.eye_count==1 && !frame.plan.stereo && frame.draws[0].source_layer==0
            && frame.draws[0].space==PicaSpace::screen && !frame.draws[0].depth_write,"Controller UI entered world stereo/death/color math");
        require(pica_resident_texture_bytes(frame.textures[0])==512*256*4,"Controller texture exceeded expected native size");
        if(!captures.empty()) {
            std::filesystem::create_directories(captures);Canvas upper(top_width);upper.image(0,0,editor.upper_view());
            upper.write_bmp((captures/("controller-"+std::to_string(row)+".bmp")).string());
        }
    }
    choose(editor,12);require(!editor.highlighted_action(),"Non-SNES deadzone row highlights a fake controller button");
    tap(editor,input::right);require(editor.bindings().deadzone==50,"Deadzone did not change");
    for(unsigned i=0;i<20;++i) tap(editor,input::right);
    require(editor.bindings().deadzone==100,"Deadzone upper UI bound broken");
    for(unsigned i=0;i<20;++i) tap(editor,input::left);
    require(editor.bindings().deadzone==10,"Deadzone lower UI bound broken");
    choose(editor,8);capture(editor,{input::a});require(editor.bindings().sources[8]==8,"In-game L did not map to physical A");
    choose(editor,9);capture(editor,{input::b});require(editor.bindings().sources[9]==0,"Physical B was reserved and could not be assigned");
    choose(editor,10);capture(editor,{input::select});require(editor.bindings().sources[10]==2,"Physical Select was reserved and could not be assigned");
    choose(editor,11);capture(editor,{input::start});require(editor.bindings().sources[11]==3,"Physical Start was reserved and could not be assigned");
    choose(editor,0);capture(editor,{0,0,80});require(editor.bindings().sources[0]==12,"Circle Pad movement could not be assigned");
    const auto before=editor.bindings();tap(editor,input::a);editor.update({input::b});
    editor.update({input::b|input::start});require(!editor.capturing() && editor.bindings()==before,"Capture cancel rebound physical B before chord completed");
    editor.update({});tap(editor,input::a);editor.update({input::a|input::x});
    editor.update({input::a});require(editor.capturing() && editor.bindings()==before,"Ambiguous assignment became a partial binding");
    editor.update({});editor.update({input::x});editor.update({});require(editor.bindings().sources[0]==9,"Ambiguous capture could not retry after release");
    const auto committed=editor.bindings();tap(editor,input::a);editor.update({input::y});editor.suspend();
    require(!editor.capturing() && editor.bindings()==committed,"Home/sleep committed an unfinished candidate");
    editor.update({input::a});require(!editor.capturing(),"Resume held A reopened capture");editor.update({});
    tap(editor,input::x);require(editor.bindings().sources[0]==255,"Clear did not unbind selected gameplay action");
    tap(editor,input::y);require(editor.bindings()==GameBindings{},"Defaults did not recover all mappings/deadzone");
    if(!captures.empty()) {Canvas lower;lower.image(0,0,editor.lower_view());lower.write_bmp((captures/"controller-list.bmp").string());}
    tap(editor,input::b);require(!editor.active() && editor.frame().textures.empty(),"Fixed B did not return from controller editor");
    editor.open({});editor.close();require(!editor.active(),"Failed source reload kept a stale editor active");
}
}
int main(int argc,char** argv) try {
    if(argc!=1 && !(argc==3 && std::string_view(argv[1])=="--capture")) throw std::invalid_argument("Usage: game_remap_tests [--capture DIRECTORY]");
    mapping_contracts();editor_contracts(argc==3?std::filesystem::path(argv[2]):std::filesystem::path{});
    std::cout<<"3DS native controller remapping: "<<checks<<" checks passed; public host UI/input contracts, not console acceptance\n";
} catch(const std::exception& error) {std::cerr<<"3DS controller remapping: "<<error.what()<<'\n';return 1;}
