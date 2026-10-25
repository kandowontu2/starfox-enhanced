#include "starfox/modern/frame_policy.hpp"
#include "starfox/modern/sdl_render_state.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
}
int main() try {
    using namespace starfox::modern;
    for(unsigned stereo=0;stereo<=9;++stereo)
        for(bool setup:{false,true}) for(bool options:{false,true})
            require(flat_frame_supported(setup,options,std::uint8_t(stereo))
                ==(!setup && !options && stereo!=stereo_sr_platform),"Flat presenter replaced a Leia SR/host frame");
    {
        // Stereo eyes: depth C keeps the mono position, infinity shifts by
        // the backdrop's disparity, and the eyes mirror each other.
        starfox::vr::EyeCamera mono{};
        mono.view[0]=mono.view[5]=mono.view[10]=mono.view[15]=1;
        mono.projection[0]=1.5F;mono.projection[5]=-2;mono.projection[8]=.1F;
        mono.projection[11]=-1;mono.projection[14]=-1.F/256;
        const auto ndc_x=[](const starfox::vr::EyeCamera& camera,double x,double depth) {
            const auto& m=camera.projection;
            return (m[0]*x+m[8]*-depth+m[12])/depth;
        };
        const auto left=stereo_eye_cameras(mono,0,16,1024,512),right=stereo_eye_cameras(mono,1,16,1024,512);
        const auto close=[](double a,double b) {return std::abs(a-b)<1e-5;};
        require(close(ndc_x(left.world,.3,4),ndc_x(mono,.3,4)) && close(ndc_x(right.world,.3,4),ndc_x(mono,.3,4)),
            "Stereo eyes disagree at the convergence distance");
        const double far=ndc_x(right.world,0,1e7)-ndc_x(mono,0,1e7);
        require(far>0 && close(far,ndc_x(right.backdrop,0,2)-ndc_x(mono,0,2))
            && close(ndc_x(left.backdrop,0,2)-ndc_x(mono,0,2),-far),"Backdrop disparity is not the eyes' infinity");
        require(close(ndc_x(right.reticle,0,2)-ndc_x(mono,0,2),ndc_x(right.world,0,2)-ndc_x(mono,0,2)),
            "Reticle disparity is not its crosshair depth");
        require(left.world.view==mono.view && right.world.view==mono.view,"Stereo eyes moved the shared world view");
        const auto unset=stereo_eye_cameras(mono,1,16,1024,0);
        require(unset.reticle.projection==mono.projection,"Reticle at depth 0 left the screen plane");
    }
    std::array<starfox::vr::SourceComputeModel,3> models;
    require(source_models_supported({}) && source_models_supported(models),"Ordinary scene was rejected");
    for(auto& model:models) {
        model.warp=std::make_shared<starfox::vr::SourceWarpInputs>();
        require(!source_models_supported(models),"Warp scene would lose a model instead of falling back");
        model.warp.reset();model.axis=std::make_shared<starfox::vr::SourceAxisInputs>();
        require(!source_models_supported(models),"Axis scene would lose a model instead of falling back");
        model.axis.reset();
    }
    auto* surface=SDL_CreateSurface(640,480,SDL_PIXELFORMAT_RGBA32);
    require(surface!=nullptr,"Create renderer test surface");
    auto* renderer=SDL_CreateSoftwareRenderer(surface);
    require(renderer!=nullptr,"Create renderer state fixture");
    for(bool custom:{false,true}) {
        require(SDL_SetRenderLogicalPresentation(renderer,320,224,SDL_LOGICAL_PRESENTATION_LETTERBOX),"Set logical canvas");
        require(SDL_SetRenderScale(renderer,1.25F,1.5F),"Set scale");
        const SDL_Rect viewport{7,9,300,200},clip{5,6,100,90};
        require(SDL_SetRenderViewport(renderer,custom?&viewport:nullptr),"Set viewport");
        require(SDL_SetRenderClipRect(renderer,custom?&clip:nullptr),"Set clip");
        for(bool failure:{false,true}) {
            try {
                const SdlRenderStateGuard guard{renderer};
                require(guard.logical_mode()==SDL_LOGICAL_PRESENTATION_LETTERBOX,"Captured wrong presentation");
                SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED);
                SDL_SetRenderScale(renderer,2,2);SDL_SetRenderViewport(renderer,nullptr);SDL_SetRenderClipRect(renderer,nullptr);
                if(failure) throw 1;
            } catch(int) {}
            int w=0,h=0;SDL_RendererLogicalPresentation mode{};float sx=0,sy=0;
            require(SDL_GetRenderLogicalPresentation(renderer,&w,&h,&mode) && w==320 && h==224
                && mode==SDL_LOGICAL_PRESENTATION_LETTERBOX,"Fallback lost logical presentation");
            require(SDL_GetRenderScale(renderer,&sx,&sy) && sx==1.25F && sy==1.5F,"Fallback lost scale");
            require(SDL_RenderViewportSet(renderer)==custom && SDL_RenderClipEnabled(renderer)==custom,"Fallback lost state flags");
            if(custom) {
                SDL_Rect actual{};
                require(SDL_GetRenderViewport(renderer,&actual) && SDL_RectsEqual(&actual,&viewport),"Fallback lost viewport");
                require(SDL_GetRenderClipRect(renderer,&actual) && SDL_RectsEqual(&actual,&clip),"Fallback lost clip");
            }
        }
    }
    SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);
    std::cout<<"MODERN complete-frame fallback and renderer-state recovery passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<": "<<SDL_GetError()<<'\n';return 1;}
