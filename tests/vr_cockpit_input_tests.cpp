#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/game_frame_driver.hpp"
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace starfox;using namespace starfox::vr;
namespace {
void require(bool value,const std::source_location& where=std::source_location::current()) {
    if(!value) throw std::runtime_error("Cockpit steering regression at "+std::to_string(where.line()));
}
constexpr simulation::MatrixQ15 identity{32767,0,0,0,32767,0,0,0,32767};
constexpr input::ButtonMask directions=input::left|input::right|input::up|input::down;
GameSceneSnapshot snapshot(simulation::MatrixQ15 rotation=identity) {
    GameSceneSnapshot scene;scene.flow=simulation::GameFlowState::gameplay;scene.pilot_tracking=true;scene.view_matrix=identity;
    scene.pilot_reference.emplace();scene.pilot_reference->rotation_matrix=rotation;return scene;
}
input::TickInput consume(VrControls controls,const GameSceneSnapshot& scene,const PresentationPreferences& prefs) {
    VrGameInput input;input.sample(controls);return input.consume(cockpit_steering_matrix(scene,prefs));
}
void cartridge(const char* rom_path,const char* symbols_path) {
    const auto rom=assets::RomImage::load(rom_path);const auto symbols=assets::SymbolMap::load(symbols_path);
    simulation::GameSimulation base(rom,symbols,"LEVEL1_1");
    base.set_timing_mode(simulation::TimingMode::unlocked_20_fps);
    const auto strategy=symbols.find("PLAYERONPLANET_STRAT").at(0);
    for(unsigned i=0;i<2400 && base.objects().at(base.player()).strategy_address!=strategy;++i) (void)base.tick({});
    require(base.objects().at(base.player()).strategy_address==strategy);(void)base.tick({});
    const auto saved=base.save_state();
    PresentationPreferences prefs;prefs.cockpit=prefs.follow_ship_rotation=true;
    const auto controls_at=[](unsigned segment) {
        VrControls controls;controls.steer=segment<4?XrVector2f{-.8F,0}:segment<8?XrVector2f{0,.8F}:XrVector2f{.8F,-.8F};
        controls.roll_right=segment==1 || (segment>=3 && segment<=5);
        controls.roll_left=segment==7 || segment>=9;
        return controls;
    };
    // The native pace derives its required raster phases from source state;
    // do not assume the unlocked mode's two logic ticks per 100 ms segment.
    for(auto timing:{simulation::TimingMode::unlocked_20_fps,simulation::TimingMode::original_speed}) {
        auto paced=base.restored_state(saved);paced->set_timing_mode(timing);const auto initial=paced->save_state();
        std::vector<uint8_t> reference;unsigned reference_audio=0,reference_ticks=0;
        for(unsigned hz:{72U,90U,120U,10U}) {
            auto game=base.restored_state(initial);GameSceneHistory history(*game,rom,symbols);unsigned blocks=0,ticks=0;
            GameFrameDriver driver(*game,[&](auto,auto) {++blocks;return std::array<uint8_t,4>{};},&history);
            (void)driver.advance(0,{},true,prefs);unsigned frame=1;
            for(unsigned segment=0;segment<12;++segment) {
                const auto controls=controls_at(segment);
                const int64_t end=(segment+1)*100'000'000LL;
                while(int64_t(frame)*1'000'000'000/hz<end) {
                    ticks+=driver.advance(int64_t(frame++)*1'000'000'000/hz,controls,true,prefs).logic_ticks;
                }
                ticks+=driver.advance(end,controls,true,prefs).logic_ticks;
                if(int64_t(frame)*1'000'000'000/hz==end) ++frame;
            }
            if(reference.empty()) {reference=game->save_state();reference_audio=blocks;reference_ticks=ticks;}
            else require(game->save_state()==reference && blocks==reference_audio && ticks==reference_ticks);
        }
        require(reference_audio==24);
        if(timing==simulation::TimingMode::unlocked_20_fps)require(reference_ticks==24);
        // Independently clock the source at its raster boundary, record adapted
        // logical controls only when the native simulation consumes them, then
        // replay through the flat simulation with no VR mapper.
        auto recorder=base.restored_state(initial);GameSceneHistory recorded_history(*recorder,rom,symbols);
        VrGameInput recorded_input;std::vector<input::TickInput> recording;unsigned changed_bases=0,source_audio=0;
        for(unsigned raster=0;raster<72;++raster) {
            recorded_input.sample(controls_at(raster/6));recorder->present_frame();
            if(recorder->logic_tick_ready()) {
                const auto before=recorded_history.current()->pilot_reference->rotation_matrix;
                recording.push_back(recorded_input.consume(cockpit_steering_matrix(*recorded_history.current(),prefs)));
                (void)recorder->tick(recording.back());(void)recorder->map().take_msu_register_writes();
                recorded_history.capture();
                changed_bases+=before!=recorded_history.current()->pilot_reference->rotation_matrix;
            }
            if(raster%3==2) {recorder->synchronize_apu_output_ports({});++source_audio;}
        }
        require(changed_bases>0 && recording.size()==reference_ticks && source_audio==reference_audio);
        require(recorder->save_state()==reference);
        auto replay=base.restored_state(initial);unsigned replay_tick=0;
        for(unsigned raster=0;raster<72;++raster) {
            replay->present_frame();
            if(replay->logic_tick_ready()) {
                require(replay_tick<recording.size());
                (void)replay->tick(recording[replay_tick++]);(void)replay->map().take_msu_register_writes();
            }
            if(raster%3==2)replay->synchronize_apu_output_ports({});
        }
        require(replay_tick==recording.size() && replay->save_state()==reference);
        std::cout<<(timing==simulation::TimingMode::original_speed?"Native":"Unlocked 20 Hz")
            <<" pace: flat replay and 72/90/120Hz/batched full-state parity passed ("
            <<reference_ticks<<" source ticks, "<<changed_bases<<" changing bases, "<<reference_audio<<" audio blocks)\n";
    }
    // Runtime/setup navigation must bypass adaptation even if the preview
    // retains an active, banked pilot behind the menu.
    for(bool runtime:{false,true}) {
        std::vector<uint8_t> menu_reference;
        for(bool follow:{false,true}) {
            auto game=base.restored_state(saved);game->objects().at(game->player()).rotation_z=64;
            if(runtime) require(game->toggle_runtime_options());else game->enable_menu_preview();
            GameSceneHistory history(*game,rom,symbols);
            GameFrameDriver driver(*game,[](auto,auto) {return std::array<uint8_t,4>{};},&history);
            auto menu_prefs=prefs;menu_prefs.follow_ship_rotation=follow;
            (void)driver.advance(0,{},true,menu_prefs);
            for(unsigned frame=1;frame<=30;++frame) {
                VrControls controls;controls.steer=frame<15?XrVector2f{0,1}:XrVector2f{-1,0};
                (void)driver.advance(int64_t(frame)*1'000'000'000/90,controls,true,menu_prefs);
            }
            if(menu_reference.empty())menu_reference=game->save_state();else require(game->save_state()==menu_reference);
        }
    }
    // Confirm each cartridge's authoritative direction/inversion conventions.
    const auto c_type=symbols.find("C_TYPE").at(0);
    for(unsigned type:{0U,2U}) for(int bank:{0,64,-64,128}) {
        auto game=base.restored_state(saved);game->map().write_native_byte(c_type,uint8_t(type));
        game->objects().at(game->player()).rotation_z=uint8_t(bank);
        GameSceneHistory history(*game,rom,symbols);const auto scene=*history.current();
        require(scene.control_type==type && pilot_view_active(scene,prefs));
        VrControls left;left.steer.x=-1;auto tick=consume(left,scene,prefs);
        const auto initial=game->objects().at(game->player());
        for(unsigned i=0;i<3;++i) {(void)game->tick(tick);tick.pressed=0;}
        const auto final=game->objects().at(game->player());
        const float dx=float(final.world_x-initial.world_x),dy=-float(final.world_y-initial.world_y);
        auto view=presentation_scene_matrix(scene,scene,1,prefs);auto source=identity_matrix;
        for(unsigned c=0;c<3;++c) for(unsigned r=0;r<3;++r)
            source[c*4+r]=float(scene.view_matrix[c*3+r])/32768*((c==0)==(r==0)?1:-1);
        view=multiply_matrix(view,source);
        require(view[0]*dx+view[4]*dy<0); // Physical left moves left in the displayed ship frame.
    }
    std::cout<<"Cartridge movement and menu bypass passed\n";
}
}
int main(int argc,char** argv) try {
    PresentationPreferences prefs;prefs.cockpit=prefs.follow_ship_rotation=true;
    const std::array<simulation::MatrixQ15,4> banks{{identity,
        {0,32767,0,-32767,0,0,0,0,32767},{0,-32767,0,32767,0,0,0,0,32767},
        {-32767,0,0,0,-32767,0,0,0,32767}}};
    const input::ButtonMask left_expected[]{input::left,input::down,input::up,input::right};
    for(unsigned bank=0;bank<4;++bank) for(unsigned type=0;type<4;++type) {
        auto scene=snapshot(banks[bank]);scene.control_type=type;
        VrControls left;left.steer.x=-.4F;
        auto expected=left_expected[bank];
        if(type&2U) {if(expected==input::up)expected=input::down;else if(expected==input::down)expected=input::up;}
        require(consume(left,scene,prefs).held==expected);
        const auto transform=*cockpit_steering_matrix(scene,prefs);
        for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y) {
            VrControls c;c.steer={float(x)*.4F,float(y)*.4F};
            c.fire=c.bomb=c.boost=c.brake=c.roll_left=c.roll_right=true;
            const auto tick=consume(c,scene,prefs);
            const input::ButtonMask buttons=input::y|input::a|input::x|input::b|input::left_shoulder|input::right_shoulder;
            require((tick.held&~directions)==buttons && tick.held==tick.pressed);
            const float a=float(bool(tick.held&input::right))-bool(tick.held&input::left);
            const float b=float(bool(tick.held&input::up))-bool(tick.held&input::down);
            require(std::abs(transform[0]*a+transform[2]*b-x)<.001F);
            require(std::abs(transform[1]*a+transform[3]*b-y)<.001F);
        }
        for(float amount:{0.F,.1F,.35F}) {left.steer.x=amount;require(consume(left,scene,prefs).held==0);}
    }
    auto scene=snapshot(banks[1]);
    for(unsigned inactive=0;inactive<6;++inactive) {
        auto s=scene;auto p=prefs;
        if(inactive==0)p.follow_ship_rotation=false;if(inactive==1)p.cockpit=false;
        if(inactive==2)s.pilot_tracking=false;if(inactive==3)s.flow=simulation::GameFlowState::title;if(inactive==4)s.paused=true;
        if(inactive==5)s.pilot_reference.reset();
        require(!cockpit_steering_matrix(s,p));VrControls left;left.steer.x=-1;require(consume(left,s,p).held==input::left);
    }
    // Source-view rotation cancels consistently, while pitch/yaw retain a valid
    // projected movement plane. Full analog diagonals are quantized only once.
    scene.pilot_reference->rotation_matrix={28378,8192,-14189,0,28378,16384,16384,-14189,24576};
    const auto fixed=cockpit_steering_matrix(scene,prefs);scene.view_matrix=banks[1];
    const auto changed=cockpit_steering_matrix(scene,prefs);require(fixed && changed);
    for(unsigned i=0;i<4;++i) require(std::abs((*fixed)[i]-(*changed)[i])<.001F);
    for(float amount:{.36F,.4F,1.F}) for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y) {
        if(!x && !y)continue;
        VrControls c;c.steer={amount*x,amount*y};const auto tick=consume(c,scene,prefs);
        require(tick.held && !(tick.held&~directions));
        const float a=float(bool(tick.held&input::right))-bool(tick.held&input::left);
        const float b=float(bool(tick.held&input::up))-bool(tick.held&input::down);
        const float dx=(*changed)[0]*a+(*changed)[2]*b,dy=(*changed)[1]*a+(*changed)[3]*b;
        require((dx*x+dy*y)/(std::hypot(dx,dy)*std::hypot(float(x),float(y)))>.9F);
    }
    // An edge-on movement plane cannot supply both view axes.
    scene.pilot_reference->rotation_matrix={32767,0,0,0,0,32767,0,-32767,0};
    require(!cockpit_steering_matrix(scene,prefs));
    // Latched quick taps and held-stick changes across bank remain proper edges.
    VrGameInput input;VrControls left;left.steer.x=-1;input.sample(left);input.sample({});
    auto tap=input.consume(cockpit_steering_matrix(snapshot(banks[1]),prefs));
    require(tap.held==0 && tap.pressed==input::down && tap.released==input::down);
    input.sample(left);auto first=input.consume(cockpit_steering_matrix(snapshot(),prefs));require(first.held==input::left);
    input.sample(left);auto turn=input.consume(cockpit_steering_matrix(snapshot(banks[1]),prefs));
    require(turn.held==input::down && turn.pressed==input::down && turn.released==input::left);
    if(argc==3)cartridge(argv[1],argv[2]);else require(argc==1);
    std::cout<<"Cockpit source steering, eight-way/deadzone, inversion and edge tests passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
