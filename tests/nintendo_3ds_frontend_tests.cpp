#include "starfox/platform/nintendo_3ds/game_routing.hpp"
#include "starfox/platform/nintendo_3ds/pica_projection.hpp"
#include "starfox/platform/nintendo_3ds/presentation_clock.hpp"
#include "starfox/platform/nintendo_3ds/hardware_profile.hpp"
#include <algorithm>
#include <climits>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace starfox::platform::nintendo_3ds;
unsigned checks{};
void require(bool value,const char* message) {
    ++checks;
    if(!value) throw std::runtime_error(message);
}
template<class F> void rejects(F operation,const char* message) {
    bool rejected=false;
    try {operation();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,message);
}
bool close(float a,float b) {return std::abs(a-b)<.0001F;}
bool near(double a,double b,double tolerance=1.e-5) {return std::abs(a-b)<tolerance;}
Rgb pixel(ImageView view,unsigned x,unsigned y) {
    const auto at=std::size_t(y)*view.pitch+x*3;
    return {view.pixels[at],view.pixels[at+1],view.pixels[at+2]};
}
void hardware_profile_tests() {
    require(hardware_profile(std::nullopt)==HardwareProfile{},"Failed model query enabled New-only features");
    for(unsigned model=0;model<256;++model) {
        const auto profile=hardware_profile(std::uint8_t(model));
        const bool new_cpu=model==2 || model==4 || model==5;
        const bool stereo=model==2 || model==4;
        require(profile.cpu_speedup==new_cpu,"CPU speedup does not match New hardware");
        require(profile.stereo==stereo,"Original/2DS/unknown model enabled stereo");
        for(float slider:{0.F,.125F,.5F,1.F,2.F,-1.F,std::numeric_limits<float>::quiet_NaN()})
            for(auto use:{ScreenUse::setup,ScreenUse::front_end,ScreenUse::menu_preview,ScreenUse::world}) {
                const auto frame=plan_frame(slider,profile.stereo,use);
                const bool world=use==ScreenUse::menu_preview || use==ScreenUse::world;
                const bool two_eyes=stereo && world && std::isfinite(slider) && slider>0;
                require(frame.stereo==two_eyes && frame.eye_count==(two_eyes?2U:1U),
                    "Hardware policy did not remove the inactive eye");
                if(!two_eyes) require(frame.slider==0 && frame.eyes[0].x==0 && frame.eyes[0].projection_offset==0,
                    "Mono fallback retained stereo displacement");
            }
    }
}
void stereo_tests() {
    for(float slider:{0.F,.25F,.5F,1.F}) {
        const auto frame=plan_frame(slider,true,ScreenUse::world);
        require(frame.eye_count==(slider?2U:1U),"Slider zero must skip the second eye");
        require(close(frame.separation,12*slider),"Slider separation is not continuous");
        for(float z:{16.F,256.F,512.F,1024.F,4096.F,65536.F}) {
            const auto left=project(frame,0,17,-23,z);
            require(left.has_value(),"Valid world point rejected");
            if(!frame.stereo) {
                require(close((*left)[0],200+256*17/z),"Mono projection changed");
                continue;
            }
            const auto right=project(frame,1,17,-23,z);
            require(right.has_value(),"Valid right-eye point rejected");
            // Independently derived parallel-camera disparity. A shifted mono
            // image cannot pass both the near and far depth cases.
            const auto expected=256*frame.separation*(1/z-1/1024.F);
            require(close((*left)[0]-(*right)[0],expected),"Incorrect stereo disparity");
            require(close((*left)[1],(*right)[1]),"Stereo introduced vertical disparity");
            if(z==1024) require(close((*left)[0],(*right)[0]),"Convergence plane must have zero disparity");
        }
        if(frame.stereo) {
            const auto far_left=project(frame,0,0,0,65536);
            const auto far_right=project(frame,1,0,0,65536);
            require((*far_left)[0]<(*far_right)[0],"Distant scene must sit behind the screen");
            require(background_offset(frame,0)<background_offset(frame,1),"Background must sit at infinity");
            require(close(background_offset(frame,1)-background_offset(frame,0),256*frame.separation/1024),
                "Incorrect infinite background separation");
        } else rejects([&]{project(frame,1,0,0,512);},"Mono code requested a right eye");
    }
    for(auto screen:{ScreenUse::setup,ScreenUse::front_end})
        require(!plan_frame(1,true,screen).stereo,"Menu text must remain mono");
    require(plan_frame(1,true,ScreenUse::menu_preview).stereo,"Menu world preview may be stereo");
    require(!plan_frame(1,false,ScreenUse::world).stereo,"2DS fallback must stay mono");
    require(!plan_frame(-1,true,ScreenUse::world).stereo,"Negative slider not clamped");
    require(!plan_frame(std::numeric_limits<float>::quiet_NaN(),true,ScreenUse::world).stereo,"NaN slider not safe");
    require(close(plan_frame(10,true,ScreenUse::world).slider,1),"Slider upper bound not clamped");
    auto settings=StereoSettings{};settings.strength=0;
    require(!plan_frame(1,true,ScreenUse::world,settings).stereo,"Disabled depth must avoid right-eye work");
    settings.convergence=1;
    rejects([&]{plan_frame(1,true,ScreenUse::world,settings);},"Invalid convergence accepted");
    settings=StereoSettings{};settings.strength=3;
    rejects([&]{plan_frame(1,true,ScreenUse::world,settings);},"Unbounded stereo strength accepted");
    settings=StereoSettings{};settings.focal_x=std::numeric_limits<float>::infinity();
    rejects([&]{plan_frame(1,true,ScreenUse::world,settings);},"Non-finite settings accepted");
    const auto frame=plan_frame(1,true,ScreenUse::world);
    for(float z:{0.F,-1.F,65537.F,std::numeric_limits<float>::quiet_NaN()})
        require(!project(frame,0,0,0,z),"Out-of-frustum point accepted");
    require(!project(frame,0,std::numeric_limits<float>::max(),0,1),"Overflowing projected point accepted");
}
void pica_projection_tests() {
    for(float slider:{0.F,.125F,.5F,1.F}) {
        const auto frame=plan_frame(slider,true,ScreenUse::world);
        for(unsigned eye=0;eye<frame.eye_count;++eye) {
            const PicaProjection projection(frame,eye);
            for(float z:{1.F,16.F,128.F,512.F,1024.F,4096.F,65536.F}) {
                for(float x:{-83.F,0.F,37.F}) for(float y:{-47.F,0.F,61.F}) {
                    const auto clip=*projection.clip_position({x,y,z});
                    require(clip[3]>0,"PICA clip W must be positive in front of the camera");
                    // Independent top-left pixel formula; do not use project()
                    // or the GPU matrix itself to derive the expected result.
                    const double eye_x=frame.separation*(eye?.5:-.5);
                    const double expected_x=200+256*(x-eye_x)/z+256*eye_x/1024;
                    const double expected_y=120-256*double(y)/z;
                    require(near(200*(1-clip[1]/clip[3]),expected_x,.001),"PICA stereo X or LCD rotation is wrong");
                    require(near(120*(1-clip[0]/clip[3]),expected_y,.001),"PICA projection introduced vertical disparity");
                    const double expected_depth=(1-65536/double(z))/65535;
                    require(near(clip[2]/clip[3],expected_depth,1.e-6),"PICA depth must use reversed [-1,0] range");
                }
            }
            const auto near_clip=*projection.clip_position({0,0,1});
            const auto far_clip=*projection.clip_position({0,0,65536});
            require(near(near_clip[2]/near_clip[3],-1,1.e-6),"PICA near-plane depth is incorrect");
            require(near(far_clip[2]/far_clip[3],0,1.e-6),"PICA far-plane depth is incorrect");
            require(!projection.clip_position({std::numeric_limits<float>::quiet_NaN(),0,1}),"Non-finite PICA vertex accepted");
        }
    }
    auto settings=StereoSettings{};
    settings.focal_x=183;settings.focal_y=219;settings.convergence=777;settings.near_plane=7;settings.far_plane=9000;
    const auto frame=plan_frame(.75F,true,ScreenUse::world,settings);
    for(unsigned eye=0;eye<2;++eye) {
        const PicaProjection projection(frame,eye);
        const auto clip=*projection.clip_position({21,-13,777});
        require(near(200*(1-clip[1]/clip[3]),200+183*21./777),"Non-default convergence did not remain at screen depth");
        require(near(120*(1-clip[0]/clip[3]),120+219*13./777),"Independent X/Y focal lengths were lost");
        require(near(projection.clip_position({0,0,7})->at(2)/7,-1,1.e-6),"Custom near depth incorrect");
        require(near(projection.clip_position({0,0,9000})->at(2)/9000,0,1.e-6),"Custom far depth incorrect");
    }
    rejects([&]{PicaProjection projection(frame,2);},"Inactive PICA eye accepted");
    auto malformed=frame;malformed.eye_count=0;
    rejects([&]{StereoFrustum frustum(malformed);},"Malformed GPU stereo frame accepted");
    malformed=frame;malformed.far_plane=malformed.near_plane;
    rejects([&]{PicaProjection projection(malformed,0);},"Zero PICA depth range accepted");
    malformed=frame;malformed.eyes[0].projection_offset=std::numeric_limits<float>::infinity();
    rejects([&]{PicaProjection projection(malformed,0);},"Non-finite PICA eye offset accepted");
}
void stereo_culling_and_clipping_tests() {
    const auto stereo=plan_frame(1,true,ScreenUse::world);
    const auto mono=plan_frame(0,true,ScreenUse::world);
    const PicaProjection left(stereo,0),right(stereo,1),single(mono,0);
    const StereoFrustum both(stereo),one(mono);
    const Bounds3 right_edge{{104,0,128},{104,0,128}};
    require(!left.intersects(right_edge) && right.intersects(right_edge),"Right-only stereo geometry test is not valid");
    require(both.intersects(right_edge) && !one.intersects(right_edge),"Shared stereo culling discarded the right eye's edge");
    const Bounds3 left_edge{{-104,0,128},{-104,0,128}};
    require(left.intersects(left_edge) && !right.intersects(left_edge) && both.intersects(left_edge),
        "Shared stereo culling discarded the left eye's edge");
    for(auto bounds:{Bounds3{{-1,-1,-5},{1,1,-2}},Bounds3{{-1,-1,.1F},{1,1,.5F}},
                    Bounds3{{-1,-1,70000},{1,1,71000}},Bounds3{{-1,1000,16},{1,1001,17}},
                    Bounds3{{-1001,-1,16},{-1000,1,17}}})
        require(!both.intersects(bounds),"Fully out-of-frustum 3DS bound survived culling");
    for(auto bounds:{Bounds3{{-10,-1,.5F},{10,1,2}},Bounds3{{-1,-1,65535},{1,1,65537}},
                    Bounds3{{-100,-100,-100},{100,100,100}},Bounds3{{-1,-1,1024},{1,1,1025}}})
        require(both.intersects(bounds),"Intersecting 3DS bound incorrectly culled");
    // Independently sample frustum points across both eyes. Any box containing
    // a definitely visible point must survive shared culling; the converse is
    // deliberately not required for this conservative broad-phase test.
    for(unsigned i=0;i<600;++i) {
        const float z=2.F+float((i*977U)%64000U);
        const float x=(int((i*613U)%601U)-300)*z/256;
        const float y=(int((i*157U)%361U)-180)*z/256;
        bool visible=false;
        for(unsigned eye=0;eye<2;++eye) {
            const double eye_x=eye?6:-6;
            const double px=200+256*(x-eye_x)/z+256*eye_x/1024;
            const double py=120-256*double(y)/z;
            visible|=px>0 && px<400 && py>0 && py<240;
        }
        if(visible) require(both.intersects({{x-.01F,y-.01F,z-.01F},{x+.01F,y+.01F,z+.01F}}),
            "3DS culling lost independently visible geometry");
    }
    rejects([&]{(void)both.intersects({{1,0,1},{0,1,2}});},"Inverted 3DS bounds accepted");
    rejects([&]{(void)both.intersects({{0,0,1},{std::numeric_limits<float>::infinity(),1,2}});},"Non-finite 3DS bounds accepted");
    const auto near_crossing=single.project_segment({0,0,-2},{1,0,8});
    require(near_crossing.has_value(),"Near-plane crossing discarded a visible diagnostic segment");
    require(near((*near_crossing)[0][0],276.8,.001) && near((*near_crossing)[1][0],232),
        "Near-plane segment was clipped after perspective division");
    const auto horizontal=single.project_segment({-1000,0,512},{1000,0,512});
    require(horizontal && near((*horizontal)[0][0],0) && near((*horizontal)[1][0],400),"LCD side clipping incorrect");
    const auto vertical=single.project_segment({0,1000,512},{0,-1000,512});
    require(vertical && near((*vertical)[0][1],0) && near((*vertical)[1][1],240),"LCD vertical clipping incorrect");
    const auto far_crossing=single.project_segment({0,0,70000},{1,0,65500});
    require(far_crossing.has_value(),"Far-plane crossing discarded a visible segment");
    require(!single.project_segment({0,0,-10},{0,0,-1}),"Entirely behind-camera line survived clipping");
    require(!single.project_segment({0,1000,16},{1,1000,20}),"Entirely offscreen line survived clipping");
    require(!single.project_segment({std::numeric_limits<float>::quiet_NaN(),0,16},{0,0,20}),"Invalid line survived clipping");
    for(const auto* eye:{&left,&right}) {
        const auto line=eye->project_segment({17,-23,512},{31,9,1024});
        require(line.has_value(),"In-frustum stereo segment discarded");
        const unsigned index=eye==&left?0:1;
        const auto a=*project(stereo,index,17,-23,512),b=*project(stereo,index,31,9,1024);
        require(near((*line)[0][0],a[0],.001) && near((*line)[0][1],a[1],.001)
            && near((*line)[1][0],b[0],.001) && near((*line)[1][1],b[1],.001),"Clipped PICA and CPU projection disagree");
    }
}
void routing_and_input_tests() {
    using enum starfox::simulation::GameFlowState;
    require(game_routing(pregame_menu).screen==ScreenUse::setup,"Pre-game setup lost");
    require(game_routing(pregame_menu,true).screen==ScreenUse::menu_preview,"Pre-game preview lost");
    require(!game_routing(ex_pregame_menu).move_hud,"EX menu sprites treated as gameplay HUD");
    for(auto state:{training,gameplay})
        require(game_routing(state).move_hud && game_routing(state).screen==ScreenUse::world,"Gameplay HUD not routed");
    require(game_routing(intro).screen==ScreenUse::world && !game_routing(intro).move_hud,"Intro art must remain on top");
    for(auto state:{title,controls_type,controls_choice,planet_select,planet_travel,stage_results,
            game_over,continue_choice,credits,finished})
        require(game_routing(state).screen==ScreenUse::front_end && !game_routing(state).move_hud,"Front-end art moved as HUD");
    using namespace starfox::input;
    const std::array<ButtonMask,12> expected{a,b,starfox::input::select,start,right,left,up,down,right_shoulder,left_shoulder,x,y};
    for(unsigned i=0;i<expected.size();++i) require(buttons(1U<<i)==expected[i],"Nintendo printed button mapping changed");
    require(buttons((1U<<8)|(1U<<9))==(left_shoulder|right_shoulder),"L+R mapping broken");
    require(buttons(1U<<31)==0,"Unrelated libctru flags mapped to SNES");
    require(buttons(0,40,-40)==0,"Circle Pad deadzone boundary broken");
    require(buttons(0,41,-41)==(right|down),"Circle Pad diagonal broken");
    require(buttons(0,-41,41)==(left|up),"Circle Pad orientation broken");
    rejects([]{buttons(0,0,0,-1);},"Negative Circle Pad deadzone accepted");
}
void lcd_tests() {
    for(unsigned width:{top_width,bottom_width}) {
        const unsigned pitch=width*3+12;
        std::vector<std::uint8_t> source(pitch*screen_height,0xD3);
        for(unsigned y=0;y<screen_height;++y) for(unsigned x=0;x<width;++x) {
            const auto at=std::size_t(y)*pitch+x*3;
            source[at]=std::uint8_t(x);source[at+1]=std::uint8_t(y);source[at+2]=std::uint8_t(x+y);
        }
        const ImageView view{source,width,screen_height,pitch};
        std::vector<std::uint8_t> destination(width*screen_height*3+16,0xA5);
        copy_lcd(view,destination);
        for(unsigned y=0;y<screen_height;++y) for(unsigned x=0;x<width;++x) {
            const auto at=(std::size_t(x)*screen_height+239-y)*3;
            require(destination[at]==std::uint8_t(x+y) && destination[at+1]==y && destination[at+2]==std::uint8_t(x),
                "LCD rotation, padding, or BGR conversion is wrong");
        }
        require(std::all_of(destination.end()-16,destination.end(),[](auto b){return b==0xA5;}),"LCD copy overflowed");
        auto invalid=view;invalid.height=239;
        const auto unchanged=destination;
        rejects([&]{copy_lcd(invalid,destination);},"Invalid LCD accepted");
        require(destination==unchanged,"Invalid LCD partially published");
        rejects([&]{copy_lcd(view,{destination.data(),4});},"Short destination accepted");
        rejects([&]{copy_lcd(view,source);},"Overlapping LCD storage accepted");
        invalid=view;invalid.pixels=invalid.pixels.first(100);
        require(!valid_image(invalid,width,240),"Short source accepted");
    }
    require(!valid_image({},0,0),"Empty image accepted");
    require(!valid_image({},UINT_MAX,UINT_MAX),"Overflowing image extent accepted");
}
void hud_tests() {
    rejects([]{Canvas canvas(UINT_MAX);},"Unbounded canvas allocation allowed");
    Canvas canvas;canvas.clear({1,2,3});
    canvas.rectangle(INT_MAX,INT_MAX,INT_MAX,INT_MAX,{255,0,0});
    canvas.rectangle(INT_MIN,INT_MIN,1,1,{255,0,0});
    require(pixel(canvas.view(),0,0)==Rgb{1,2,3},"Out-of-bounds rectangle wrote pixels");
    canvas.rectangle(-2,-2,3,3,{4,5,6});
    require(pixel(canvas.view(),0,0)==Rgb{4,5,6} && pixel(canvas.view(),1,0)==Rgb{1,2,3},"Rectangle clipping broken");
    rejects([&]{canvas.text(0,0,"A",{},4);},"Unbounded font scale accepted");
    HudState state;state.shield_percent=200;state.boost_percent=0;state.boss_percent=50;
    state.radio_message="SHORT MESSAGE";draw_cockpit(canvas,state);
    require(pixel(canvas.view(),105,216)==Rgb{239,90,99},"Shield percentage not clamped");
    require(pixel(canvas.view(),214,216)==Rgb{6,18,28},"Zero boost meter still filled");
    const auto short_view=std::vector<std::uint8_t>(canvas.view().pixels.begin(),canvas.view().pixels.end());
    const auto long_message=std::string(4000,'W');state.radio_message=long_message;draw_cockpit(canvas,state);
    const auto view=canvas.view();
    require(std::equal(view.pixels.begin()+view.pitch*78,view.pixels.end(),short_view.begin()+view.pitch*78),
        "Long radio message drew over lower cockpit panels");
    const std::array<std::uint8_t,12> portrait{255,10,0,0,255,10,10,0,255,80,90,100};
    state.portrait={portrait,2,2,6};draw_cockpit(canvas,state);
    require(pixel(canvas.view(),128,104)==Rgb{255,10,0} && pixel(canvas.view(),129,105)==Rgb{80,90,100},"Portrait RGB copy broken");
    state.portrait.width=65;
    rejects([&]{draw_cockpit(canvas,state);},"Oversized radio portrait accepted");
    Canvas top(top_width);rejects([&]{draw_cockpit(top,{});},"Lower-screen HUD drawn onto upper screen");
}
void dashboard_cache_tests() {
    CockpitDashboard dashboard;HudState state;
    std::string message="ALL SHIPS CHECK IN";state.radio_message=message;
    std::array<std::uint8_t,18> portrait{255,10,0,0,255,10,99,99,99,10,0,255,80,90,100,99,99,99};
    state.portrait={portrait,2,2,9};
    require(dashboard.update(state),"First dashboard update did not render");
    const auto initial=std::vector<std::uint8_t>(dashboard.view().pixels.begin(),dashboard.view().pixels.end());
    for(unsigned frame=0;frame<120;++frame) {
        const auto plan=plan_frame(float(frame)/119,true,ScreenUse::world);
        require(plan.eye_count>=1 && !dashboard.update(state),"Slider movement redrew unchanged lower HUD");
    }
    std::string relocated_message=message;state.radio_message=relocated_message;
    require(!dashboard.update(state),"Same message at a different address invalidated the dashboard");
    portrait[6]=0;portrait[17]=0;
    require(!dashboard.update(state),"Portrait padding invalidated the dashboard");
    portrait[0]=42;
    require(dashboard.update(state) && pixel(dashboard.view(),128,104)==Rgb{42,10,0},
        "In-place portrait animation was not detected");
    relocated_message[0]='B';
    require(dashboard.update(state),"In-place radio text change was not detected");
    state.shield_percent=43;state.boss_percent=51;state.ally_percent[1]=27;
    require(dashboard.update(state) && !dashboard.update(state),"Changed meter state was not cached");
    const auto valid=std::vector<std::uint8_t>(dashboard.view().pixels.begin(),dashboard.view().pixels.end());
    auto invalid=state;invalid.portrait.pixels=invalid.portrait.pixels.first(4);
    rejects([&]{dashboard.update(invalid);},"Short radio portrait accepted by dashboard cache");
    require(std::equal(valid.begin(),valid.end(),dashboard.view().pixels.begin()),"Invalid portrait partially redrew dashboard");
    invalid=state;invalid.portrait.width=65;
    rejects([&]{dashboard.update(invalid);},"Oversized cached portrait accepted");
    require(!dashboard.update(state),"Rejected portrait invalidated retained source state");
    state.portrait={};
    require(dashboard.update(state) && !dashboard.update(state),"Portrait removal not cached");
    require(pixel(dashboard.view(),128,104)==Rgb{6,18,28},"Removed portrait left stale artwork");
    require(initial!=valid,"Changing dashboard source did not affect artwork");
}
void radio_artwork_cache_tests() {
    CockpitDashboard dashboard;HudState state;
    std::array<std::uint8_t,18> artwork{1,2,3,4,5,6,99,99,99,7,8,9,10,11,12,99,99,99};
    state.radio_artwork={artwork,2,2,9};state.radio_message="FALLBACK";
    require(dashboard.update(state),"Cartridge radio artwork did not render");
    require(pixel(dashboard.view(),18,16)==Rgb{1,2,3}
        && pixel(dashboard.view(),19,17)==Rgb{10,11,12},"Radio artwork pitch/position changed");
    const auto original=std::vector<std::uint8_t>(dashboard.view().pixels.begin(),dashboard.view().pixels.end());
    auto relocated=artwork;state.radio_artwork.pixels=relocated;
    require(!dashboard.update(state),"Radio address change invalidated the owned cache");
    relocated[6]=0;relocated[17]=0;
    require(!dashboard.update(state),"Radio padding change invalidated the owned cache");
    relocated[9]=42;
    require(dashboard.update(state) && pixel(dashboard.view(),18,17)==Rgb{42,8,9},
        "In-place radio artwork change was not detected");
    const auto retained=std::vector<std::uint8_t>(dashboard.view().pixels.begin(),dashboard.view().pixels.end());
    auto invalid=state;invalid.radio_artwork.pixels=invalid.radio_artwork.pixels.first(4);
    rejects([&]{dashboard.update(invalid);},"Short radio artwork accepted");
    invalid=state;invalid.radio_artwork.width=285;
    rejects([&]{dashboard.update(invalid);},"Radio artwork overflowed message panel horizontally");
    invalid=state;invalid.radio_artwork.height=57;
    rejects([&]{dashboard.update(invalid);},"Radio artwork overflowed message panel vertically");
    require(std::equal(retained.begin(),retained.end(),dashboard.view().pixels.begin())
        && !dashboard.update(state),"Invalid radio artwork changed retained dashboard/cache");
    Canvas direct;direct.clear({1,2,3});
    rejects([&]{draw_cockpit(direct,invalid);},"Direct cockpit drawing accepted oversized radio artwork");
    require(pixel(direct.view(),0,0)==Rgb{1,2,3},"Invalid radio artwork partially drew cockpit");
    state.radio_artwork={};state.radio_message=" "; // No text pixels, without the standby fallback.
    require(dashboard.update(state) && !dashboard.update(state),"Radio artwork removal was not cached");
    require(pixel(dashboard.view(),18,17)==Rgb{6,18,28},"Removed radio artwork left stale pixels");
    state.meters_enabled=false;state.counters_enabled=false;
    require(dashboard.update(state),"Disabled source meters did not redraw");
    require(pixel(dashboard.view(),50,216)==Rgb{15,29,42},"Disabled shield still showed source meter");
    state.meters_enabled=true;state.boost_enabled=false;state.second_shield_percent=50;
    require(dashboard.update(state) && !dashboard.update(state),"Player-two health/boost gating was not cached");
    require(pixel(dashboard.view(),35,230)==Rgb{86,208,168},"Player-two shield not shown");
    require(pixel(dashboard.view(),250,216)==Rgb{15,29,42},"Unavailable boost still showed meter");
    state.counters_enabled=true;state.second_counters=HudCounters{6,2};
    require(dashboard.update(state) && !dashboard.update(state),"P2 counters were not part of the dashboard cache key");
    auto with_p2=std::vector<std::uint8_t>(dashboard.view().pixels.begin(),dashboard.view().pixels.end());
    state.second_counters->bombs=3;
    require(dashboard.update(state) && !dashboard.update(state),"P2 bomb changes did not update exactly once");
    require(!std::equal(with_p2.begin(),with_p2.end(),dashboard.view().pixels.begin()),"P2 bomb change was invisible");
    state.second_counters.reset();
    require(dashboard.update(state) && !dashboard.update(state),"Retiring P2 counters did not clear cached artwork");
    state.second_player_view=true;state.lives=6;state.bombs=2;
    require(dashboard.update(state) && !dashboard.update(state),"Active P2 counter labels were not cached");
    state.counters_enabled=false;
    require(dashboard.update(state) && !dashboard.update(state),"Disabling P2 counters left cached labels");
    require(original!=retained,"Radio mutation did not change visible pixels");
}
void presentation_clock_tests() {
    for(unsigned rate:{30U,60U}) {
        PresentationClock clock;PresentationRate measured;
        require(clock.due(0,rate),"Native clock skipped its first presentation");measured.completed(0);
        unsigned rendered{};
        for(unsigned phase=1;phase<=6000;++phase) {
            const auto time=(std::int64_t(phase)*1'000'000'000+59)/60;
            const bool due=clock.due(time,rate);
            require(due==(rate==60 || phase%2==0),"Native frame gate drifted away from integer 60 Hz source phases");
            require(!clock.due(time,rate),"Duplicate host time submitted a second eye/frame");
            if(due) {++rendered;measured.completed(time);}
            if(phase%60==0) require(measured.fps()==rate,"Measured native rate counted source ticks instead of presentations");
        }
        require(rendered==rate*100,"Native presentation count drifted over 100 seconds");
        require(clock.due(200'000'000'000LL,rate) && !clock.due(200'000'000'000LL,rate),"Long stall produced catch-up presentations");
        require(clock.due(1,rate) && !clock.due(1,rate),"Rewound clock retained old credit");
        require(clock.due(1,rate==30?60:30),"Changing native target did not rebase presentation");
        clock.reset();require(clock.due(1,rate),"Home/editor reset did not restart native presentation");
        const auto end=std::numeric_limits<std::int64_t>::max();
        clock.reset();require(clock.due(end-1'000'000'000LL,rate) && clock.due(end,rate),"Long uptime overflowed native clock");
    }
    PresentationClock clock;
    for(unsigned invalid:{0U,20U,29U,31U,90U,120U,480U}) rejects([&]{static_cast<void>(clock.due(0,invalid));},"Unsupported native output target accepted");
    rejects([&]{static_cast<void>(clock.due(-1,60));},"Negative native time accepted");
    PresentationRate measured;
    measured.completed(0);measured.completed(3'000'000'000LL);
    require(measured.fps()==0,"Long loading stall was advertised as a valid measured rate");
    measured.reset();for(unsigned phase=0;phase<=60;++phase) measured.completed(std::int64_t(phase)*1'000'000'000/60);
    require(measured.fps()==60,"Actual presentation count was not reported");
    measured.reset();require(measured.fps()==0,"APT/editor reset retained a stale FPS display");
    measured.completed(-1);require(measured.fps()==0,"Invalid measurement time retained rate");
}
void captures(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    Canvas lower;HudState hud;hud.shield_percent=76;hud.boost_percent=92;
    hud.lives=2;hud.bombs=3;hud.ally_percent={84,58,95};hud.boss_percent=67;
    hud.radio_message="ALL SHIPS CHECK IN\nSLIDER DEPTH / HUD BELOW";
    draw_cockpit(lower,hud);lower.write_bmp((directory/"lower-hud.bmp").string());
    hud.second_counters=HudCounters{6,2};
    draw_cockpit(lower,hud);lower.write_bmp((directory/"lower-hud-two-player.bmp").string());
    hud.second_counters.reset();hud.second_player_view=true;hud.lives=6;hud.bombs=2;
    draw_cockpit(lower,hud);lower.write_bmp((directory/"lower-hud-p2-view.bmp").string());
    const auto plan=plan_frame(1,true,ScreenUse::world);
    for(unsigned eye=0;eye<2;++eye) {
        Canvas top(top_width);top.clear({8,15,28});
        top.text(20,12,"3DS PROJECTION CHECK - NOT GAMEPLAY",{213,237,244});
        for(float depth:{256.F,512.F,1024.F,3072.F}) {
            const std::array<std::array<float,2>,4> corners{{{-64,48},{64,48},{64,-48},{-64,-48}}};
            for(unsigned i=0;i<4;++i) {
                const auto a=*project(plan,eye,corners[i][0],corners[i][1],depth);
                const auto b=*project(plan,eye,corners[(i+1)%4][0],corners[(i+1)%4][1],depth);
                top.line(int(a[0]),int(a[1]),int(b[0]),int(b[1]),depth<1024?Rgb{94,204,229}:Rgb{240,181,86});
            }
        }
        top.write_bmp((directory/(eye?"top-right.bmp":"top-left.bmp")).string());
    }
}
} // namespace
int main(int argc,char** argv) {
    try {
        hardware_profile_tests();stereo_tests();pica_projection_tests();stereo_culling_and_clipping_tests();
        routing_and_input_tests();lcd_tests();hud_tests();dashboard_cache_tests();radio_artwork_cache_tests();presentation_clock_tests();
        if(argc==3 && std::string_view(argv[1])=="--capture") captures(argv[2]);
        else if(argc!=1) throw std::invalid_argument("Usage: frontend_tests [--capture directory]");
        std::cout<<"3DS frontend: "<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"3DS frontend regression: "<<error.what()<<'\n';return 1;
    }
}
