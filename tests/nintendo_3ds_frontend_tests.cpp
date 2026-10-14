#include "starfox/platform/nintendo_3ds/game_routing.hpp"
#include "starfox/platform/nintendo_3ds/pica_projection.hpp"
#include "starfox/platform/nintendo_3ds/presentation_clock.hpp"
#include "starfox/platform/nintendo_3ds/hardware_profile.hpp"
#include "starfox/platform/nintendo_3ds/lcd_rgba.hpp"
#include "starfox/platform/nintendo_3ds/mono_presentation.hpp"
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
void mono_presentation_tests() {
    using R=MonoTileResult;
    for(auto first:{R::presented,R::busy,R::rejected})
        for(auto second:{R::presented,R::busy,R::rejected})
            for(bool complete_ok:{false,true}) {
                std::string events;unsigned attempts{};
                const bool result=present_mono_tiles_or_complete([&](bool retry) {
                    require(retry==(attempts!=0),"Retry flag did not match its attempt");
                    events+=retry?'R':'T';return attempts++?second:first;
                },[&]{events+='S';},[&]{events+='F';return complete_ok;});
                const bool expected=first==R::presented
                    || (first==R::busy && second==R::presented) || complete_ok;
                const auto order=first==R::presented?"T":first==R::rejected?"TSF"
                    :second==R::presented?"TR":"TRSF";
                require(result==expected,"Mono presentation lost the actual submission result");
                require(events==order,"Mono presentation rerasterized a busy frame, skipped retirement or retried a rejected scene");
                require(attempts<=2,"Busy GPU retry was not bounded");
            }
    // Do not swallow a draw failure, submit a fabricated success, or fall
    // through into another owner after an exception.
    for(unsigned fail_at:{0U,1U,2U}) {
        std::string events;bool caught{};
        try {
            static_cast<void>(present_mono_tiles_or_complete([&](bool retry) {
                events+=retry?'R':'T';
                if(fail_at==0) throw std::runtime_error("tile failure");
                return R::rejected;
            },[&] {events+='S';if(fail_at==1) throw std::runtime_error("sync failure");},
            [&]() -> bool {events+='F';throw std::runtime_error("fallback failure");}));
        } catch(const std::runtime_error&) {caught=true;}
        require(caught && events==(fail_at==0?"T":fail_at==1?"TS":"TSF"),
            "Mono failure escaped its owner or falsely submitted a frame");
    }
}
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
void rgba_lcd_tests() {
    constexpr auto row_bytes=bottom_width*3;
    constexpr auto output_bytes=bottom_width*screen_height*4;
    const unsigned pitch=row_bytes+13;
    std::vector<std::uint8_t> pixels(pitch*screen_height,0xD3);
    for(unsigned y=0;y<screen_height;++y) for(unsigned x=0;x<bottom_width;++x) {
        auto* p=pixels.data()+std::size_t(y)*pitch+x*3;
        p[0]=std::uint8_t(x*7);p[1]=std::uint8_t(y*13);p[2]=std::uint8_t(x+y);
    }
    ImageView source{pixels,bottom_width,screen_height,pitch};
    std::vector<std::uint8_t> first(output_bytes+2,0xA5),second(output_bytes,0x5A);
    const std::span<std::uint8_t> a{first.data()+1,output_bytes},b{second};
    const auto verify=[&](std::span<const std::uint8_t> out) {
        for(unsigned y=0;y<screen_height;++y) for(unsigned x=0;x<bottom_width;++x) {
            const auto* p=pixels.data()+std::size_t(y)*pitch+x*3;
            const auto at=(std::size_t(x)*screen_height+screen_height-1-y)*4;
            require(out[at]==255 && out[at+1]==p[2] && out[at+2]==p[1] && out[at+3]==p[0],
                "Tiled RGBA LCD conversion differs from the independent pixel oracle");
        }
    };
    RgbaLcdCache cache;
    require(cache.prepare(source,a),"Fresh lower framebuffer was assumed resident");verify(a);
    require(cache.prepare(source,a),"Unflushed lower framebuffer was certified");cache.commit(a);
    require(!cache.prepare(source,a),"Unchanged certified lower framebuffer was copied");
    require(cache.prepare(source,b),"Second physical lower framebuffer was assumed resident");cache.commit(b);verify(b);
    require(!cache.prepare(source,a) && !cache.prepare(source,b),"Double-buffer residency was lost");
    // Same input address with different actual content must invalidate BOTH LCDs.
    pixels[(screen_height-1)*pitch+(bottom_width-1)*3]^=0xFF;
    require(cache.prepare(source,a),"Mutable RGB pointer reused stale lower content");cache.commit(a);verify(a);
    require(cache.prepare(source,b),"Changed HUD failed to invalidate the other LCD");cache.commit(b);verify(b);
    // Source row padding is not a displayed pixel dependency.
    pixels[row_bytes]^=0xFF;
    require(!cache.prepare(source,a),"Unused input row padding invalidated the LCD");
    std::vector<std::uint8_t> packed(row_bytes*screen_height);
    for(unsigned y=0;y<screen_height;++y)
        std::copy_n(pixels.data()+std::size_t(y)*pitch,row_bytes,packed.data()+std::size_t(y)*row_bytes);
    const ImageView contiguous{packed,bottom_width,screen_height,row_bytes};
    require(!cache.prepare(contiguous,b),"Equivalent packed RGB content lost residency");
    // A resident presenter returning from the other GPU owner must repaint
    // both physical buffers, even if their pointers and desired RGB are equal.
    cache.invalidate();
    require(cache.prepare(source,a),"Handoff retained old first-buffer LCD certificate");cache.commit(a);
    require(cache.prepare(source,b),"Handoff retained old second-buffer LCD certificate");cache.commit(b);
    require(!cache.prepare(source,a) && !cache.prepare(source,b),"Repainted handoff buffers did not recertify");
    cache.invalidate();cache.invalidate();
    require(cache.prepare(source,a),"Repeated invalidation did not discard LCD certificate");cache.commit(a);
    RgbaLcdCache next_owner;
    require(next_owner.prepare(source,a),"A different display owner inherited old LCD residency");
    const auto before=first;
    auto invalid=source;invalid.height=screen_height-1;
    rejects([&]{copy_rgba_lcd(invalid,a);},"Invalid RGBA source accepted");
    rejects([&]{copy_rgba_lcd(source,a.first(output_bytes-1));},"Short RGBA destination accepted");
    std::vector<std::uint8_t> shared(output_bytes);
    rejects([&]{copy_rgba_lcd({shared,bottom_width,screen_height,row_bytes},shared);},"Overlapping RGBA storage accepted");
    require(first==before && first.front()==0xA5 && first.back()==0xA5,"RGBA bounds rejection changed output or guards");
    bool rejected{};
    try {cache.commit(a);} catch(const std::logic_error&) {rejected=true;}
    require(rejected,"LCD cache accepted a commit without a pending transfer");
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
void canvas_rectangle_oracle_tests() {
    std::uint32_t seed=0x68bf1234U;
    const auto next=[&] {seed=seed*1664525U+1013904223U;return seed;};
    for(const auto size:std::array<std::array<unsigned,2>,5>{{{320,240},{400,240},{74,74},{1,1},{13,7}}}) {
        Canvas canvas(size[0],size[1]);
        std::vector<std::uint8_t> pixels(std::size_t(size[0])*size[1]*3,0),coverage(std::size_t(size[0])*size[1],0);
        bool covered=false;
        for(unsigned step=0;step<320;++step) {
            // Beginning coverage does not clear artwork, and later fills must
            // preserve old ownership outside their actual clipped rectangle.
            if(step==80 || step==210) {canvas.begin_artwork();std::fill(coverage.begin(),coverage.end(),0);covered=true;}
            int x=int(next()%1200)-400,y=int(next()%800)-300;
            int width=int(next()%800)-40,height=int(next()%500)-30;
            switch(step%32) {
            case 0:x=0;y=0;width=int(size[0]);height=int(size[1]);break;
            case 1:x=INT_MIN;y=INT_MIN;width=INT_MAX;height=INT_MAX;break;
            case 2:x=INT_MAX;y=INT_MAX;width=INT_MAX;height=INT_MAX;break;
            case 3:x=-2;y=-2;width=3;height=3;break;
            case 4:x=int(size[0])-1;y=int(size[1])-1;width=INT_MAX;height=INT_MAX;break;
            case 5:x=0;y=0;width=INT_MIN;height=INT_MIN;break;
            case 6:x=0;y=0;width=int(size[0]);height=1;break;
            case 7:x=0;y=0;width=1;height=int(size[1]);break;
            default:break;
            }
            const Rgb colour{std::uint8_t(next()),std::uint8_t(next()),std::uint8_t(next())};
            const auto before=covered?std::span<const std::uint8_t>(coverage):std::span<const std::uint8_t>{};
            require(canvas.coverage().size()==before.size(),"Canvas coverage allocation changed unexpectedly");
            canvas.rectangle(x,y,width,height,colour);
            // Independent old painter: inspect every destination coordinate,
            // not the optimized clip/span/copy implementation.
            if(width>0 && height>0) for(unsigned row=0;row<size[1];++row)
                for(unsigned column=0;column<size[0];++column)
                    if(std::int64_t(column)>=x && std::int64_t(column)<std::int64_t(x)+width
                        && std::int64_t(row)>=y && std::int64_t(row)<std::int64_t(y)+height) {
                        const auto pixel=std::size_t(row)*size[0]+column,at=pixel*3;
                        pixels[at]=colour.r;pixels[at+1]=colour.g;pixels[at+2]=colour.b;
                        if(covered) coverage[pixel]=1;
                    }
            require(std::equal(pixels.begin(),pixels.end(),canvas.view().pixels.begin()),"Clipped RGB span differs from pixel painter");
            if(covered) require(std::equal(coverage.begin(),coverage.end(),canvas.coverage().begin()),"Clipped span changed artwork coverage");
        }
    }
}
void canvas_image_oracle_tests() {
    std::uint32_t seed=0x812e731bU;
    const auto next=[&] {seed=seed*1664525U+1013904223U;return seed;};
    for(const auto size:std::array<std::array<unsigned,2>,5>{{{320,240},{400,240},{74,74},{1,1},{13,7}}}) {
        Canvas canvas(size[0],size[1]);
        std::vector<std::uint8_t> expected(std::size_t(size[0])*size[1]*3,0),coverage(std::size_t(size[0])*size[1],0);
        bool covered=false;
        for(unsigned step=0;step<256;++step) {
            if(step==60 || step==180) {canvas.begin_artwork();std::fill(coverage.begin(),coverage.end(),0);covered=true;}
            const unsigned width=1+next()%size[0],height=1+next()%screen_height,pitch=width*3+next()%23;
            std::vector<std::uint8_t> source(std::size_t(pitch)*height+16,0xCD);
            for(unsigned row=0;row<height;++row) for(unsigned column=0;column<width*3;++column)
                source[std::size_t(row)*pitch+column]=step%16?std::uint8_t(next()):0;
            const auto source_before=source;
            const ImageView image{{source.data(),std::size_t(pitch)*(height-1)+width*3},width,height,pitch};
            int x=int(next()%1000)-350,y=int(next()%700)-250;
            switch(step%16) {
            case 0:x=0;y=0;break;
            case 1:x=-int(width)+1;y=-int(height)+1;break;
            case 2:x=int(size[0])-1;y=int(size[1])-1;break;
            case 3:x=INT_MIN;y=INT_MIN;break;
            case 4:x=INT_MAX;y=INT_MAX;break;
            case 5:x=-1;y=1;break;
            case 6:x=1;y=-1;break;
            default:break;
            }
            canvas.image(x,y,image);
            // Independent destination-coordinate oracle. Padding is never art,
            // and copying black still claims coverage just like pixel().
            for(unsigned row=0;row<size[1];++row) for(unsigned column=0;column<size[0];++column) {
                const auto sx=std::int64_t(column)-x,sy=std::int64_t(row)-y;
                if(sx<0 || sy<0 || sx>=width || sy>=height) continue;
                const auto at=(std::size_t(row)*size[0]+column)*3,src=std::size_t(sy)*pitch+std::size_t(sx)*3;
                for(unsigned channel=0;channel<3;++channel) expected[at+channel]=source[src+channel];
                if(covered) coverage[at/3]=1;
            }
            require(std::equal(expected.begin(),expected.end(),canvas.view().pixels.begin()),"Clipped RGB image spans differ from pixel painter");
            if(covered) require(std::equal(coverage.begin(),coverage.end(),canvas.coverage().begin()),"Image spans changed artwork ownership");
            require(source==source_before,"RGB image draw changed source or padding");
            const auto rgb_before=expected,coverage_before=coverage;
            auto invalid=image;invalid.pixels=invalid.pixels.first(width*3-1);
            rejects([&]{canvas.image(x,y,invalid);},"Invalid image was accepted before clipping");
            require(std::equal(rgb_before.begin(),rgb_before.end(),canvas.view().pixels.begin()),"Invalid image partially drew pixels");
            if(covered) require(std::equal(coverage_before.begin(),coverage_before.end(),canvas.coverage().begin()),"Invalid image partially claimed coverage");
        }
    }
    // Self-images are legal in the existing API. A memcpy/memmove shortcut
    // would change their forward-pixel smearing, especially between rows.
    for(int y:{-1,0,1,3}) for(int x:{-2,0,1,4}) {
        Canvas canvas(13,7);canvas.begin_artwork();
        for(unsigned row=0;row<7;++row) for(unsigned column=0;column<13;++column)
            canvas.rectangle(column,row,1,1,{std::uint8_t(column*11),std::uint8_t(row*31),std::uint8_t(column+row)});
        auto expected=std::vector<std::uint8_t>(canvas.view().pixels.begin(),canvas.view().pixels.end());
        const auto source=canvas.view();
        canvas.image(x,y,{source.pixels.subspan(3),7,3,13*3});
        for(unsigned row=0;row<3;++row) for(unsigned column=0;column<7;++column) {
            const int dx=x+int(column),dy=y+int(row);
            if(dx<0 || dy<0 || dx>=13 || dy>=7) continue;
            const auto src=3+row*13*3+column*3;
            const auto dst=(dy*13+dx)*3;
            const std::array colour{expected[src],expected[src+1],expected[src+2]};
            for(unsigned channel=0;channel<3;++channel) expected[dst+channel]=colour[channel];
        }
        require(std::equal(expected.begin(),expected.end(),canvas.view().pixels.begin()),"Self-image forward-pixel semantics changed");
        require(std::all_of(canvas.coverage().begin(),canvas.coverage().end(),[](auto value){return value==1;}),"Self-image discarded coverage");
    }
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
        require(clock.due(0,rate,0),"Native clock skipped its first presentation");measured.completed(0);
        unsigned rendered{};
        for(unsigned phase=1;phase<=6000;++phase) {
            const auto time=(std::int64_t(phase)*1'000'000'000+59)/60;
            const bool due=clock.due(time,rate,1);
            require(due==(rate==60 || phase%2==0),"Native frame gate drifted away from integer 60 Hz source phases");
            require(!clock.due(time,rate,1),"Duplicate host time submitted a second eye/frame");
            if(due) {++rendered;measured.completed(time);}
            if(phase%60==0) require(measured.fps()==rate,"Measured native rate counted source ticks instead of presentations");
        }
        require(rendered==rate*100,"Native presentation count drifted over 100 seconds");
        require(clock.due(200'000'000'000LL,rate,15) && !clock.due(200'000'000'000LL,rate,15),"Long stall produced catch-up presentations");
        require(clock.due(1,rate,0) && !clock.due(1,rate,0),"Rewound clock retained old credit");
        require(clock.due(1,rate==30?60:30,0),"Changing native target did not rebase presentation");
        clock.reset();require(clock.due(1,rate,0),"Home/editor reset did not restart native presentation");
        const auto end=std::numeric_limits<std::int64_t>::max();
        clock.reset();require(clock.due(end-1'000'000'000LL,rate,0) && clock.due(end,rate,60),"Long uptime overflowed native clock");
        // Vary presentation work after each exact source-clock sample. The old
        // independent gate loses ready frames when a long draw follows a short
        // draw, even though all 60 source phases were completed on time.
        clock.reset();require(clock.due(12'000'000,rate,0),"Jitter fixture did not start");
        unsigned jitter_rendered{};
        for(unsigned phase=1;phase<=6000;++phase) {
            const auto source_time=(std::int64_t(phase)*1'000'000'000+59)/60;
            const auto work=phase%2?2'000'000LL:12'000'000LL;
            if(clock.due(source_time+work,rate,1)) ++jitter_rendered;
            require(!clock.due(source_time+work+1,rate,0),"No source phase replayed the preceding image");
        }
        require(jitter_rendered==rate*100,"Render jitter discarded ready source presentations");
        require(clock.due(end,rate,std::numeric_limits<unsigned>::max()),"Bounded catch-up rejected its final frame");
        require(!clock.due(end,rate,std::numeric_limits<unsigned>::max()),"Catch-up burst submitted more than once");
    }
    PresentationClock clock;
    for(unsigned invalid:{0U,20U,29U,31U,90U,120U,480U}) rejects([&]{static_cast<void>(clock.due(0,invalid,0));},"Unsupported native output target accepted");
    rejects([&]{static_cast<void>(clock.due(-1,60,0));},"Negative native time accepted");
    require(source_phase_wait_needed(0,16'666'666) && !source_phase_wait_needed(0,16'666'667),"Source wait crossed the rational 60 Hz boundary");
    require(source_phase_wait_needed(.5,8'333'333) && !source_phase_wait_needed(.5,8'333'334),"Wait ignored source phase debt");
    require(!source_phase_wait_needed(.999,1'000'000),"Wait slept through a phase completed by source work");
    require(source_phase_wait_needed(0,0) && !source_phase_wait_needed(0,std::numeric_limits<std::int64_t>::max()),"Wait mishandled empty work or long uptime");
    require(source_phase_wait_nanoseconds(0,0)==16'666'667
        && source_phase_wait_nanoseconds(.5,0)==8'333'334
        && source_phase_wait_nanoseconds(.75,4'000'000)==166'667,
        "Source sleep ignored the outstanding partial phase");
    require(source_phase_wait_nanoseconds(.5,8'333'334)==0
        && source_phase_wait_nanoseconds(0,std::numeric_limits<std::int64_t>::max())==0,
        "Completed source work still slept or overflowed");
    for(unsigned eighth=0;eighth<8;++eighth) for(std::int64_t work=0;work<=20'000'000;work+=97'013) {
        const auto fraction=double(eighth)/8.0;
        const auto wait=source_phase_wait_nanoseconds(fraction,work);
        // Exact integer numerator for binary-eighth source fractions. The
        // rational deadline must be rounded up, not stretched to LCD VBlank.
        const auto debt=(8-std::int64_t(eighth))*1'000'000'000-work*480;
        const auto expected=debt>0?(debt+479)/480:0;
        require(wait==expected && wait>=0 && wait<=16'666'667,
            "Source deadline sleep drifted or exceeded one phase");
    }
    for(double fraction:{-1.0,1.0,std::numeric_limits<double>::quiet_NaN()})
        rejects([&]{static_cast<void>(source_phase_wait_needed(fraction,0));},"Invalid source wait fraction accepted");
    rejects([&]{static_cast<void>(source_phase_wait_needed(0,-1));},"Negative source wait time accepted");
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
        hardware_profile_tests();mono_presentation_tests();stereo_tests();pica_projection_tests();stereo_culling_and_clipping_tests();
        routing_and_input_tests();lcd_tests();rgba_lcd_tests();hud_tests();canvas_rectangle_oracle_tests();canvas_image_oracle_tests();dashboard_cache_tests();radio_artwork_cache_tests();presentation_clock_tests();
        if(argc==3 && std::string_view(argv[1])=="--capture") captures(argv[2]);
        else if(argc!=1) throw std::invalid_argument("Usage: frontend_tests [--capture directory]");
        std::cout<<"3DS frontend: "<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"3DS frontend regression: "<<error.what()<<'\n';return 1;
    }
}
