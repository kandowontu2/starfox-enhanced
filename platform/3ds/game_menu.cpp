#include "starfox/platform/nintendo_3ds/game_menu.hpp"

namespace starfox::platform::nintendo_3ds {
namespace {
using simulation::PregamePage;
constexpr std::array<std::string_view,80> labels{
    "EXPERIENCE","PACE/SPEED","RENDER FPS","DISPLAY","RENDERER","MSU-1 MUSIC","RUMBLE",
    "AA QUALITY","2D FILTER","RENDER UPSCALE","ENHANCED LIGHTING","VSYNC","3D COLOR / STYLE",
    "2D COLOR / STYLE","OPTIONS","START GAME","PREVIEW","3D BLOOM","2D BLOOM","3D SMOOTHING",
    "2D OPTIONS","3D OPTIONS","MODEL EFFECT INTENSITY","BACK","WORLD EFFECT INTENSITY","","",
    "CHROMATIC ABERRATION","CONTRAST","RAY TRACING","DLSS","","REFLECTIVE SURFACES",
    "3D DISTORTION","DISTORTION INTENSITY","3D MATERIAL","ENHANCED GROUND","GROUND MATERIAL",
    "GROUND MOTION","ENHANCED SKY","SKY STYLE","SKY MOTION","AA TYPE","INTEGER SCALING",
    "2D DISTORTION","3D SPECIAL FX","2D SPECIAL FX","GLOBAL ENHANCEMENTS",
    "LIGHT SHAFTS","ANAMORPHIC FLARE","HALATION","SOFT FOCUS","RADIAL BLUR","HEAT HAZE","SHARPEN",
    "VIGNETTE","FILM GRAIN","CRT SCANLINES","PHOSPHOR MASK","CRT CURVATURE","LENS GHOSTS",
    "EXPLOSION SHOCKWAVES","WEAPON LIGHTING","EXHAUST TRAILS","STAGE WEATHER","AMBIENT OCCLUSION",
    "DEPTH OF FIELD","IMPACT SPARKS / DEBRIS","EXHAUST HEAT DISTORTION","CRT PHOSPHOR PERSISTENCE",
    "ADAPTIVE EXPOSURE","WATER CAUSTICS","SHADOW SOFTNESS","IMPACT SHAKE","WEAPON RECOIL",
    "CAMERA BANKING","VOLUMETRIC FOG","MOTION BLUR","DLSS 4.5","3D ASTEROIDS"};
std::string toggle(bool value) {return value?"ON":"OFF";}
bool supported(PregamePage page,unsigned id,bool runtime) {
    switch(page) {
    case PregamePage::main:
        return (id==0 && !runtime) || id==1 || id==2 || id==14 || id==15 || id==16
            || id==20 || id==21 || id==47;
    case PregamePage::options: return id==0 || id==1 || id==3 || id==5 || id==6 || id==7 || id==8 || id==9 || id==11 || id==12;
    case PregamePage::cheats: return true;
    case PregamePage::two_d: case PregamePage::three_d: case PregamePage::global: return id==23;
    case PregamePage::stereo: return id==1 || id==2 || id==4 || id==5;
    }
    return false;
}
GameMenuRow row(const simulation::GameSimulation& game,unsigned id) {
    const auto page=game.pregame_page();
    GameMenuRow result{std::uint8_t(id),{}, {},supported(page,id,game.runtime_options_open())};
    if(page==PregamePage::cheats) {
        constexpr std::array<std::string_view,8> names{"GOD MODE","LEVEL SELECT","DEFAULT LASER",
            "INFINITE BOMBS","INFINITE BOOST","INFINITE LIVES","BACK","PLANET SELECT CHEAT"};
        result.label=names.at(id);
        switch(id) {
        case 0:result.value=toggle(game.god_mode());break;
        case 1:result.value=game.selected_level_name();break;
        case 2:result.value=std::array<std::string_view,3>{"SINGLE","DUAL","BEAM"}.at(game.default_laser());break;
        case 3:result.value=toggle(game.infinite_bombs());break;
        case 4:result.value=toggle(game.infinite_boost());break;
        case 5:result.value=toggle(game.infinite_lives());break;
        case 7:result.value=toggle(game.planet_select_cheat());break;
        default:break;
        }
    } else if(page==PregamePage::options) {
        constexpr std::array<std::string_view,17> names{"CHEATS","ON-SCREEN FPS","CROSSHAIR COLOR",
            "CUSTOMIZE SCREEN","ON-SCREEN BUTTONS","SWAP A/B + Y/X","MUSIC VOLUME","SFX VOLUME",
            "CONTROLLER","STEREOSCOPIC 3D","","BACK","LANGUAGE","CUSTOMIZE BUTTON LAYOUT",
            "FULLSCREEN","GPU BACKEND","RENDERER"};
        result.label=names.at(id);
        switch(id) {
        case 0:case 3:case 8:case 9:result.value="A  OPEN";break;
        case 1:result.value=toggle(game.show_fps());break;
        case 5:result.value=toggle(game.swap_face_buttons());break;
        case 6:result.value=std::to_string(game.music_volume())+"%";break;
        case 7:result.value=std::to_string(game.sfx_volume())+"%";break;
        case 12:result.value=std::array<std::string_view,6>{"ENGLISH","JAPANESE","GERMAN","FRENCH","SPANISH","ENGLISH (EUROPE)"}.at(game.language());break;
        case 14:result.value="3DS LCD";break;
        case 15:result.value="PICA200";break;
        case 16:result.value="3DS GPU";break;
        default:break;
        }
    } else if(page==PregamePage::stereo) {
        constexpr std::array<std::string_view,7> names{"OUTPUT","SEPARATION","CONVERGENCE",
            "RETICLE DEPTH","RESET DEPTH","BACK","NATIVE LEIA SR"};
        result.label=names.at(id);
        if(id==0) result.value="3D SLIDER";
        else if(id==1) result.value=std::to_string(game.stereo_separation());
        else if(id==2) result.value=std::to_string(game.stereo_convergence());
        else if(id==4) result.value="A  RESET";
    } else {
        result.label=labels.at(id);
        switch(id) {
        case 0:result.value=game.runtime_options_open()?"LOCKED":game.experience()==simulation::Experience::original?"ORIGINAL":"STARFOX EX";break;
        case 1:result.value=game.timing_mode()==simulation::TimingMode::original_speed?"ORIGINAL":"UNLOCKED 20 HZ";break;
        case 2:result.value=std::to_string(game.presentation_fps())+" FPS";break;
        case 3:result.value="3DS LCD";break;
        case 4:result.value="3DS GPU";break;
        case 5:result.value="UNAVAILABLE";break;
        case 14:case 20:case 21:case 47:result.value="A  OPEN";break;
        case 15:if(game.runtime_options_open()) result.label="RESUME";break;
        case 16:result.value=toggle(game.preview_requested());break;
        case 30:if(game.fsr1_menu()) result.label="FSR1";break;
        default:break;
        }
    }
    if(result.label.empty()) throw std::logic_error("Unmapped source pre-game menu row");
    if(!result.enabled && result.value.empty()) result.value="UNAVAILABLE";
    return result;
}
std::string title(PregamePage page) {
    switch(page) {
    case PregamePage::main:return "STAR FOX ENHANCED";
    case PregamePage::options:return "OPTIONS";
    case PregamePage::two_d:return "2D OPTIONS";
    case PregamePage::three_d:return "3D OPTIONS";
    case PregamePage::cheats:return "CHEATS";
    case PregamePage::global:return "GLOBAL ENHANCEMENTS";
    case PregamePage::stereo:return "STEREOSCOPIC 3D";
    }
    throw std::logic_error("Unknown source pre-game page");
}
} // namespace
GameMenu::GameMenu(const assets::RomImage& rom,const assets::SymbolMap& symbols):text_(rom,symbols) {
    // Check source-owned page orders without private cartridge data. A new
    // shared row must never pass CI and then crash only when a player opens it.
    for(const auto page:{PregamePage::main,PregamePage::two_d,PregamePage::three_d,PregamePage::global})
        for(const auto id:simulation::pregame_menu_order(page))
            if(id>=labels.size() || labels[id].empty()) throw std::logic_error("Unmapped source menu page row");
    constexpr std::array<Point3,4> corners{{{0,0,0},{400,0,0},{400,240,0},{0,240,0}}};
    constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
    unsigned i=0;for(unsigned corner:{0U,1U,2U,0U,2U,3U}) vertices_[i++]={corners[corner],{1,1,1,1},uv[corner]};
    draws_[0]={0,6,0,pica_identity,PicaSpace::screen,false,false,true};
    draws_[0].source_layer=0; // Source flashes/windows never recolour or erase host UI.
}
GameMenuState GameMenu::capture(const simulation::GameSimulation& game) {
    GameMenuState result;
    result.visible=game.in_setup_menu();
    if(!result.visible) return result;
    result.preview=game.menu_preview();result.page=game.pregame_page();
    result.selection=game.pregame_selection();result.language=game.language();result.title=title(result.page);
    // The source order is authoritative, including long pages and cartridge
    // cheats. Never replace it with a smaller native-only options menu.
    for(auto id:simulation::pregame_menu_order(result.page)) result.rows.push_back(row(game,id));
    return result;
}
input::TickInput GameMenu::filter(const simulation::GameSimulation& game,input::TickInput input) {
    if(!game.in_setup_menu()) return input;
    return filter(game.pregame_page(),game.pregame_selection(),game.runtime_options_open(),input);
}
input::TickInput GameMenu::filter(PregamePage page,unsigned selection,bool runtime,input::TickInput input) {
    if(input.pressed&(input::up|input::down)) {
        // tick_pregame_menu navigates BEFORE applying A/Select/horizontal.
        // Checking only the old row lets a simultaneous navigation+confirm
        // enable an unsupported desktop feature from an enabled neighbour.
        const auto order=simulation::pregame_menu_order(page);
        const auto current=std::size_t(std::find(order.begin(),order.end(),selection)-order.begin());
        if(order.empty() || current==order.size()) throw std::invalid_argument("Invalid source menu selection");
        const auto delta=(input.pressed&input::up)?order.size()-1U:1U;
        selection=order[(current+delta)%order.size()];
    }
    if(supported(page,selection,runtime)) return input;
    auto changes=input::ButtonMask(input::a|input::select|input::left|input::right);
    if(page==PregamePage::main) changes|=input::b;
    input.pressed=static_cast<input::ButtonMask>(input.pressed&~changes);
    return input;
}
bool GameMenu::update(const GameMenuState& state) {
    if(initialized_ && state_==state) return false;
    if(state.visible) {
        if(state.rows.empty() || state.rows.size()>labels.size() || state.language>5
            || std::none_of(state.rows.begin(),state.rows.end(),[&](const auto& row){return row.id==state.selection;}))
            throw std::invalid_argument("Invalid actual pre-game snapshot");
        for(std::size_t i=0;i<state.rows.size();++i) for(std::size_t j=0;j<i;++j)
            if(state.rows[i].id==state.rows[j].id) throw std::invalid_argument("Duplicate actual pre-game row");
    }
    state_=state;initialized_=true;
    if(!state.visible) {rgba_.clear();rgb_.clear();textures_={};return true;}
    text_.set_language(state.language);indexed_.clear(0);
    // Preserve the cartridge menu font at 1x. Extra LCD width lets translated
    // labels and availability text breathe without shrinking the source font.
    constexpr int left=8,right=391,top=20,bottom=234,label_x=25,value_right=382;
    const auto draw_text=[&](std::string_view value,int x,int y,unsigned ink) {
        text_.draw_ascii(value,x,y,indexed_,std::uint8_t(ink));
    };
    draw_text(state.title,int(top_width)/2-text_.measure_ascii(state.title)/2,4,14);
    for(int x=left;x<=right;++x) {indexed_.set(x,top,116);indexed_.set(x,bottom,116);}
    for(int y=top;y<=bottom;++y) {indexed_.set(left,y,116);indexed_.set(right,y,116);}
    const auto selected=std::size_t(std::find_if(state.rows.begin(),state.rows.end(),[&](const auto& row){return row.id==state.selection;})-state.rows.begin());
    constexpr std::size_t visible_rows=14;
    const auto first=state.rows.size()>visible_rows?std::min(selected>12?selected-12:0,state.rows.size()-visible_rows):0;
    const auto count=std::min(visible_rows,state.rows.size());
    for(std::size_t i=first;i<first+count;++i) {
        const auto& row=state.rows[i];const int y=27+int(i-first)*14;
        const unsigned ink=i==selected?14:row.enabled?7:4;
        // Keep percentages only. In particular there are no redundant audio
        // bars, and no separate new/native control semantics.
        draw_text(row.label,label_x,y,ink);
        draw_text(row.value,value_right-text_.measure_ascii(row.value),y,ink);
        if(i==selected) for(int x=0;x<5;++x) for(int dy=-(4-x);dy<=4-x;++dy)
            indexed_.set(14+x,y+5+dy,126);
    }
    const auto arrow=[&](bool up) {for(int y=0;y<4;++y) for(int x=-y;x<=y;++x) indexed_.set(14+x,up?23+y:229-y,126);};
    if(first) arrow(true);
    if(first+count<state.rows.size()) arrow(false);
    rgba_.resize(top_width*screen_height*4);rgb_.resize(top_width*screen_height*3);
    for(unsigned y=0;y<screen_height;++y) for(unsigned x=0;x<top_width;++x) {
        const auto i=std::size_t(y)*top_width+x;const unsigned ink=indexed_.pixels()[i];
        Rgb colour{};unsigned alpha=state.preview && (x<unsigned(left) || x>unsigned(right) || y<unsigned(top) || y>unsigned(bottom))?0:state.preview?190:255;
        if(ink) {alpha=255;colour=(ink&15)==14?Rgb{255,255,255}:(ink&15)==10?Rgb{255,220,64}
            :(ink&15)==4?Rgb{105,124,138}:Rgb{180,200,215};}
        rgba_[i*4]=rgb_[i*3]=colour.r;rgba_[i*4+1]=rgb_[i*3+1]=colour.g;rgba_[i*4+2]=rgb_[i*3+2]=colour.b;rgba_[i*4+3]=std::uint8_t(alpha);
    }
    textures_[0]={rgba_,top_width,screen_height,top_width*4,4};++redraws_;return true;
}
PicaFrame GameMenu::frame(const FramePlan& plan) const {
    return {plan,state_.visible?std::span<const PicaVertex>(vertices_):std::span<const PicaVertex>{},
        state_.visible?std::span<const PicaDraw>(draws_):std::span<const PicaDraw>{},
        state_.visible?std::span<const PicaImage>(textures_):std::span<const PicaImage>{},{0,0,0}};
}
ImageView GameMenu::plain_view() const {
    if(!state_.visible || state_.preview) throw std::logic_error("Plain menu requested for a scene/preview");
    return {rgb_,top_width,screen_height,top_width*3};
}
} // namespace starfox::platform::nintendo_3ds
