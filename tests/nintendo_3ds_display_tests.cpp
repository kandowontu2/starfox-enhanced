#include "native_display.hpp"
#include <3ds.h>
#include <iostream>

namespace {
using namespace starfox::platform::nintendo_3ds;
unsigned checks{};
void require(bool condition,const char* message) {
    ++checks;if(!condition) throw std::runtime_error(message);
}
struct Services {
    u8 model{};
    Result initialize{},query{};
    unsigned queries{},exits{},slider_reads{},input_reads{},right_reads{},flushes{},gfx_exits{};
    float slider{1};bool running{true},stereo{};
    aptHookCookie* cookie{};
    std::optional<APT_HookType> event;
    std::vector<bool> speedup;
    std::array<u8,400*240*3> left{},right{};
    std::array<u8,320*240*3> lower{};
} service;
void test_model(u8 model,Result initialize=0,Result query=0) {
    service=Services{};service.model=model;service.initialize=initialize;service.query=query;
    const bool queried=initialize>=0;
    const bool detected=queried && query>=0;
    const bool stereo=detected && (model==2 || model==4);
    const bool speedup=detected && (model==2 || model==4 || model==5);
    {
        NativeDisplay display;
        require(display.profile()==HardwareProfile{speedup,stereo},"Actual display adapter chose wrong model policy");
        require(service.queries==unsigned(queried) && service.exits==unsigned(queried),"CFGU service lease was not balanced");
        require(service.speedup==std::vector<bool>{speedup},"Wrong initial speedup request");
        require(service.cookie && !service.stereo,"Startup hook/LCD state missing");
        for(auto event:{APTHOOK_ONRESTORE,APTHOOK_ONWAKEUP}) {
            service.event=event;
            const auto controls=display.poll();
            require(controls.running && controls.stereoscopic_hardware==stereo,"Poll changed hardware capability");
            require(controls.slider==(stereo?1.F:0.F),"Original/2DS slider was not ignored");
            require(controls.physical==starfox::input::a && (controls.held&starfox::input::right)
                && controls.touching && controls.touch_x==31 && controls.touch_y==47,"Hardware policy changed mapped input/touch");
        }
        require(service.speedup==std::vector<bool>{speedup,speedup,speedup},"Home/wake did not restore the speedup policy");
        require(service.slider_reads==(stereo?2U:0U),"Mono hardware read the unused slider");
        std::vector<u8> left(top_width*screen_height*3,19),right(left.size(),73),bottom(bottom_width*screen_height*3,41);
        const ImageView l{left,top_width,screen_height,top_width*3};
        const ImageView r{right,top_width,screen_height,top_width*3};
        const ImageView b{bottom,bottom_width,screen_height,bottom_width*3};
        display.present(plan_frame(1,stereo,ScreenUse::world),l,stereo?r:ImageView{},b);
        require(service.stereo==stereo && service.right_reads==unsigned(stereo),"Mono display requested a second framebuffer");
        require(service.left.front()==19 && service.lower.front()==41 && service.flushes==1,"Display omitted a required LCD");
        if(stereo) require(service.right.front()==73,"Independent right eye was not published");
        else {
            bool rejected=false;
            try {display.present(plan_frame(1,true,ScreenUse::world),l,r,b);}
            catch(const std::invalid_argument&) {rejected=true;}
            require(rejected && !service.stereo && service.right_reads==0 && service.flushes==1,
                "An accidental stereo frame bypassed the mono policy");
        }
        service.running=false;
        const auto input_reads=service.input_reads,slider_reads=service.slider_reads;
        const auto stopped=display.poll();
        require(!stopped.running && service.input_reads==input_reads && service.slider_reads==slider_reads,
            "Closed display still sampled input/slider");
    }
    require(!service.cookie && service.speedup==std::vector<bool>{speedup,speedup,speedup,false}
        && service.gfx_exits==1,"Exit retained APT callback or CPU speedup");
}
}
Result cfguInit() {return service.initialize;}
Result CFGU_GetSystemModel(u8* model) {++service.queries;*model=service.model;return service.query;}
void cfguExit() {++service.exits;}
void osSetSpeedupEnable(bool value) {service.speedup.push_back(value);}
float osGet3DSliderState() {++service.slider_reads;return service.slider;}
void aptHook(aptHookCookie* cookie,void (*callback)(APT_HookType,void*),void* pointer) {
    cookie->callback=callback;cookie->parameter=pointer;service.cookie=cookie;
}
void aptUnhook(aptHookCookie* cookie) {require(cookie==service.cookie,"Unhooked a different APT owner");service.cookie=nullptr;}
bool aptMainLoop() {
    if(service.event) {service.cookie->callback(*service.event,service.cookie->parameter);service.event.reset();}
    return service.running;
}
void gfxInitDefault() {}
void gfxSet3D(bool value) {service.stereo=value;}
void gfxExit() {++service.gfx_exits;require(!service.cookie && !service.speedup.back(),"Speedup lease outlived graphics teardown");}
void hidScanInput() {++service.input_reads;}
void hidCircleRead(circlePosition* position) {*position={100,0};}
u32 hidKeysHeld() {return 1U|KEY_TOUCH;}
void hidTouchRead(touchPosition* position) {*position={31,47};}
u8* gfxGetFramebuffer(gfxScreen_t screen,gfx3dSide_t eye,void*,void*) {
    if(screen==GFX_BOTTOM) return service.lower.data();
    if(eye==GFX_RIGHT) {++service.right_reads;return service.right.data();}
    return service.left.data();
}
void gfxFlushBuffers() {++service.flushes;}
void gfxSwapBuffers() {}
void gspWaitForVBlank() {}
int main() try {
    for(unsigned model=0;model<256;++model) test_model(u8(model));
    for(u8 model:{u8(0),u8(2),u8(4),u8(5)}) {
        test_model(model,-1);test_model(model,0,-1);
    }
    std::cout<<"3DS display adapter: "<<checks<<" host service-double checks passed; not device/ABI acceptance\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
