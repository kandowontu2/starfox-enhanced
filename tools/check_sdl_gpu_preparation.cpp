#include "starfox/render/sdl_gpu_preparation.hpp"
#include "starfox/render/sdl_gpu_pipeline_preparation.h"
#include "starfox/render/gpu_retirement.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace starfox::render;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Events {
    std::thread::id owner=std::this_thread::get_id();
    unsigned pumps{};
    bool wrong_thread{},nested{};
    static void pump(void* user) noexcept {
        auto& self=*static_cast<Events*>(user);++self.pumps;
        self.wrong_thread|=std::this_thread::get_id()!=self.owner;
        self.nested|=preparation_events!=nullptr;
        SDL_PumpEvents();SDL_SetError("owner pump sentinel");
    }
};
struct Compile {
    std::thread::id thread{};
    unsigned mode{},calls{};
    static bool SDLCALL run(void* user) {
        auto& self=*static_cast<Compile*>(user);++self.calls;self.thread=std::this_thread::get_id();
        require(preparation_events==nullptr,"Preparation context inherited/reentered");
        if(self.mode==1) return SDL_SetError("presenter worker failure sentinel");
        if(self.mode==2) throw std::runtime_error("presenter worker exception sentinel");
        return true;
    }
};
static bool SDLCALL reject(void*,bool (SDLCALL *)(void*),void*) {
    return SDL_SetError("presenter initialization rejected sentinel");
}
struct PipelineObserver {
    std::thread::id owner=std::this_thread::get_id();unsigned calls{},resources{};bool wrong_owner{},wrong_worker{};
    struct Request {PipelineObserver* self;bool (SDLCALL *create)(void*);void* argument;};
    static bool SDLCALL resource(void* argument) {
        auto& request=*static_cast<Request*>(argument);++request.self->resources;
        request.self->wrong_worker|=std::this_thread::get_id()==request.self->owner || preparation_events!=nullptr;
        return request.create(request.argument);
    }
    static bool SDLCALL prepare(void* user,bool (SDLCALL *create)(void*),void* argument) {
        auto& self=*static_cast<PipelineObserver*>(user);++self.calls;self.wrong_owner|=std::this_thread::get_id()!=self.owner;
        Request request{&self,create,argument};const auto* hook=SdlGpuPreparation::pipeline_hook();
        return hook->prepare(hook->user,resource,&request);
    }
};
static bool SDLCALL no_resource(void*,bool (SDLCALL *)(void*),void*) {return true;}
static void check_pixels(SDL_Renderer* renderer,bool textured=false) {
    // Vulkan can present through a native image blit without compiling a blit
    // pipeline. Exercise both solid and textured presenter draws explicitly,
    // rather than relying on a backend-specific number of swapchain pipelines.
    constexpr std::array<Uint8,24> texels{
        239,17,53,255, 31,197,71,255, 47,83,229,255,
        167,59,193,255, 13,149,211,255, 223,181,29,255};
    SDL_Texture* texture{};
    require(SDL_SetRenderDrawColor(renderer,7,11,19,255),"Draw color");require(SDL_RenderClear(renderer),"Clear");
    SDL_FRect rect{10,8,22,17};require(SDL_SetRenderDrawColor(renderer,211,37,89,255),"Rect color");
    require(SDL_RenderFillRect(renderer,&rect),"Fill rect");
    if(textured) {
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,3,2);
        require(texture,"Presenter texture");
        require(SDL_UpdateTexture(texture,nullptr,texels.data(),12),"Presenter texture upload");
        require(SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST),"Presenter texture scale mode");
        require(SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE),"Presenter texture blend mode");
        SDL_FRect destination{38,29,12,8};
        require(SDL_RenderTexture(renderer,texture,nullptr,&destination),"Presenter texture draw");
    }
    auto* surface=SDL_RenderReadPixels(renderer,nullptr);require(surface,"Read presenter pixels");
    auto* pixels=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(surface);require(pixels,"RGBA conversion");
    require(pixels->w==64 && pixels->h==48,"Presenter dimensions changed");
    for(int y=0;y<48;++y) for(int x=0;x<64;++x) {
        const auto* pixel=static_cast<const Uint8*>(pixels->pixels)+y*pixels->pitch+x*4;
        std::array<Uint8,4> expected=x>=10 && x<32 && y>=8 && y<25
            ?std::array<Uint8,4>{211,37,89,255}:std::array<Uint8,4>{7,11,19,255};
        if(textured && x>=38 && x<50 && y>=29 && y<37) {
            const auto offset=((y-29)/4*3+(x-38)/4)*4;
            for(unsigned channel=0;channel<4;++channel) expected[channel]=texels[offset+channel];
        }
        for(unsigned channel=0;channel<4;++channel) require(pixel[channel]==expected[channel],"Presenter shader changed pixels");
    }
    SDL_DestroySurface(pixels);require(SDL_RenderPresent(renderer),"Present on owner");
    SDL_DestroyTexture(texture);
}
int main() try {
    require(SDL_Init(SDL_INIT_VIDEO),"SDL init");
    Events events;PreparationEvents context{Events::pump,&events};
    // Native SDL input polling takes measurable time itself. Give the short
    // callback fixture enough duration to observe repeated real owner pumps;
    // the executable-level eight-second test separately measures WM_NULL.
    SdlGpuPreparation preparation{500,true};const auto* hook=preparation.hook();Compile compile;
    require(hook->prepare(hook->user,Compile::run,&compile),"Unscoped preparation failed");
    require(compile.thread==events.owner && context.jobs==0,"Unscoped preparation moved threads");
    {
        ScopedPreparationEvents scope(&context);
        require(hook->prepare(hook->user,Compile::run,&compile),"Joined preparation failed");
        std::cout<<"Joined callback: worker="<<(compile.thread!=events.owner)<<" jobs="<<context.jobs<<" pumps="<<context.pumps<<'\n';
        require(compile.thread!=events.owner && context.jobs==1 && context.pumps>=10,"Joined worker/pumps missing");
        compile.mode=1;require(!hook->prepare(hook->user,Compile::run,&compile),"Failure accepted");
        require(std::string_view(SDL_GetError())=="presenter worker failure sentinel","Worker TLS error lost");
        compile.mode=2;require(!hook->prepare(hook->user,Compile::run,&compile),"Exception escaped/accepted");
        require(std::string_view(SDL_GetError()).find("presenter worker exception sentinel")!=std::string_view::npos,"Worker exception lost");
        compile.mode=0;require(hook->prepare(hook->user,Compile::run,&compile),"Recovery failed");
    }
    require(compile.calls==5 && !events.wrong_thread && !events.nested,"Callback contract violated");
    auto* window=SDL_CreateWindow("Presenter preparation check",64,48,SDL_WINDOW_HIDDEN);require(window,"Window init");
    const auto props=SDL_CreateProperties();require(props!=0,"Renderer properties");
    SDL_SetPointerProperty(props,SDL_PROP_RENDERER_CREATE_WINDOW_POINTER,window);
    SDL_SetStringProperty(props,SDL_PROP_RENDERER_CREATE_NAME_STRING,"gpu");
    // No hook -> untouched SDL behavior. Bad version -> no opt-in either.
    for(unsigned mode=0;mode<4;++mode) {
        StarfoxSdlGpuPreparation invalid{0,nullptr,reject},failure{1,nullptr,reject};
        SDL_ClearProperty(props,STARFOX_SDL_GPU_PREPARATION);
        if(mode) SDL_SetPointerProperty(props,STARFOX_SDL_GPU_PREPARATION,
            mode==1?&invalid:mode==2?&failure:const_cast<StarfoxSdlGpuPreparation*>(hook));
        const auto jobs=context.jobs;
        SDL_Event queued{};queued.type=SDL_EVENT_USER;queued.user.code=31415;require(SDL_PushEvent(&queued),"Queue owner event");
        SDL_Renderer* renderer{};
        {ScopedPreparationEvents scope(&context);renderer=SDL_CreateRendererWithProperties(props);}
        if(mode==2) {
            require(!renderer,"Rejected presenter unexpectedly started");
            require(std::string_view(SDL_GetError())=="presenter initialization rejected sentinel","Initialization failure lost");
        } else {
            require(renderer,SDL_GetError());
            require(SDL_GetBooleanProperty(SDL_GetRendererProperties(renderer),STARFOX_SDL_GPU_PREPARATION,false)==(mode==3),"SDL preparation hook was ignored/misidentified");
            require(context.jobs==jobs+(mode==3?1U:0U),"Unexpected presenter preparation jobs");
            std::cout<<"Presenter adapter: "<<SDL_GetGPUDeviceDriver(SDL_GetGPURendererDevice(renderer))<<" / "
                <<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(SDL_GetGPURendererDevice(renderer)),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<'\n';
            check_pixels(renderer);SDL_DestroyRenderer(renderer);
        }
        SDL_Event retained{};require(SDL_PeepEvents(&retained,1,SDL_GETEVENT,SDL_EVENT_USER,SDL_EVENT_USER)==1
            && retained.user.code==31415,"Preparation consumed owner input");
    }
    require(!events.wrong_thread && !events.nested,"SDL presenter preparation violated owner contract");
    // This table survives renderer creation AND later draws; unlike the
    // initializer's shader hook it is retained by the actual GPU device.
    SDL_ClearProperty(props,STARFOX_SDL_GPU_PREPARATION);
    for(unsigned mode=0;mode<2;++mode) {
        PipelineObserver observer;StarfoxSdlGpuPreparation pipeline{mode?1U:0U,&observer,PipelineObserver::prepare};
        SDL_SetPointerProperty(props,STARFOX_SDL_GPU_PIPELINE_PREPARATION,&pipeline);
        const auto jobs=context.jobs;
        SDL_Event queued{};queued.type=SDL_EVENT_USER;queued.user.code=27182;require(SDL_PushEvent(&queued),"Queue pipeline event");
        {
            ScopedPreparationEvents scope(&context);
            auto* renderer=SDL_CreateRendererWithProperties(props);require(renderer,SDL_GetError());
            auto* device=SDL_GetGPURendererDevice(renderer);
            require(SDL_GetBooleanProperty(SDL_GetRendererProperties(renderer),STARFOX_SDL_GPU_PIPELINE_PREPARATION,false)==bool(mode),"Pipeline opt-in/version gate failed");
            check_pixels(renderer,true);const auto cached=context.jobs;check_pixels(renderer,true);
            require(context.jobs==cached,"Presenter pipeline cache recompiled a held draw/present");
            if(mode) {
                require(observer.calls>=2 && observer.calls==observer.resources && context.jobs==jobs+observer.calls,
                    "Actual presenter/blit pipeline compilation bypassed the joined hook");
                require(!observer.wrong_owner && !observer.wrong_worker,"Pipeline cache/command or compilation thread changed");
                // Direct helper failures do not replace thread-local errors or
                // poison an already live presenter's next cached draw.
                StarfoxSdlGpuPreparation failed{1,nullptr,reject},missing{1,nullptr,no_resource};
                SDL_SetPointerProperty(SDL_GetGPUDeviceProperties(device),STARFOX_SDL_GPU_PIPELINE_PREPARATION,&failed);
                require(!Starfox_SdlPreparePipeline(device,nullptr) && std::string_view(SDL_GetError())=="presenter initialization rejected sentinel",
                    "Pipeline callback failure/error lost");
                SDL_SetPointerProperty(SDL_GetGPUDeviceProperties(device),STARFOX_SDL_GPU_PIPELINE_PREPARATION,&missing);
                require(!Starfox_SdlPreparePipeline(device,nullptr) && std::string_view(SDL_GetError()).find("produced no resource")!=std::string_view::npos,
                    "Empty pipeline result accepted");
                SDL_SetPointerProperty(SDL_GetGPUDeviceProperties(device),STARFOX_SDL_GPU_PIPELINE_PREPARATION,&pipeline);
                check_pixels(renderer,true);require(context.jobs==cached,"Pipeline error recovery changed cached resources");
                // Acquisition/submission/release stay on this owner. Only the
                // real backend fence wait is joined, with borrowed handles
                // retained until completion and queued input left untouched.
                auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"Retirement command acquisition");
                auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"Retirement fence submission");
                const auto retirement_jobs=context.jobs;
                require(wait_gpu_retirement(device,true,&fence,1),"Joined GPU retirement failed");
                require(context.jobs==retirement_jobs+1 && SDL_QueryGPUFence(device,fence),"Retirement did not join actual GPU completion");
                SDL_ReleaseGPUFence(device,fence);
            } else require(observer.calls==0 && context.jobs==jobs,"Unsupported pipeline hook did not fall back synchronously");
            SDL_DestroyRenderer(renderer);
        }
        SDL_Event retained{};require(SDL_PeepEvents(&retained,1,SDL_GETEVENT,SDL_EVENT_USER,SDL_EVENT_USER)==1
            && retained.user.code==27182,"Pipeline preparation consumed owner input");
    }
    SDL_DestroyProperties(props);SDL_DestroyWindow(window);SDL_Quit();
    std::cout<<"PASS: presenter shader/pipeline workers, joined GPU retirement, owner cache/commands/pumps/input, TLS errors/exceptions, synchronous fallback, opt-in/version, failure/recovery, cache reuse and 24576 exact pixels\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<": "<<SDL_GetError()<<'\n';return 1;}
