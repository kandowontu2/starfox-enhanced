#include "native_mono.hpp"
#include "reference/frame_3ds.hpp"
#include "reference/display_3ds.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
namespace starfox::platform::nintendo_3ds {
namespace {
void restore_formats() noexcept {
    gfxSet3D(false);gfxSetScreenFormat(GFX_TOP,GSP_BGR8_OES);gfxSetScreenFormat(GFX_BOTTOM,GSP_BGR8_OES);
}
}
struct NativeMono::Impl {
    platform_3ds::Display3ds display;
    Impl() {
        if(!display.open()) {display.close();restore_formats();throw std::runtime_error("Reference 3DS mono display initialization failed");}
        // Physical slider stereo remains on our separate native-eye path.
        gfxSet3D(false);
    }
    ~Impl() {display.close();restore_formats();}
};
NativeMono::NativeMono():impl_(std::make_unique<Impl>()) {}
NativeMono::~NativeMono()=default;
bool NativeMono::present(GameSession& session,const GamePresentation& source,ImageView dashboard) {
    if(source.plan.stereo || !valid_image(dashboard,bottom_width,screen_height))
        throw std::invalid_argument("Incomplete reference mono frame");
    auto& renderer=session.mono_renderer();
    renderer.set_sprite_selection(source.sprites);
    const auto& game=session.game();const auto& ppu=game.map().ppu_state();
    const bool gpu_bg2=impl_->display.eligible_mode2(ppu)
        && (game.flow_state()==simulation::GameFlowState::gameplay || game.flow_state()==simulation::GameFlowState::training);
    const auto scroll=renderer.current_background_scroll(game);
    if(gpu_bg2) impl_->display.prepare_mode2_plan(ppu,game.map().ppu_vram_revision(),scroll.first,scroll.second);
    const bool reuse=renderer.can_reuse_presentation(game,gpu_bg2,source.interpolation_alpha);
    const auto& indexed=[&]() -> const render::Framebuffer& {
        STARFOX_3DS_FRAME_PHASE(models);
        return reuse?renderer.last_frame():renderer.draw(game,gpu_bg2,source.interpolation_alpha);
    }();
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(ppu.cgram),game.map().display_brightness());
    // Preserve our native lower HUD/layout, not the reference options UI.
    auto* lower=gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,nullptr,nullptr);
    if(!lower)throw std::runtime_error("3DS mono dashboard allocation failed");
    for(unsigned x=0;x<bottom_width;++x) for(unsigned y=0;y<screen_height;++y) {
        const auto* pixel=dashboard.pixels.data()+std::size_t(y)*dashboard.pitch+x*3;
        auto* out=lower+(std::size_t(x)*screen_height+screen_height-1-y)*4;
        out[0]=255;out[1]=pixel[2];out[2]=pixel[1];out[3]=pixel[0];
    }
    if(R_FAILED(GSPGPU_FlushDataCache(lower,bottom_width*screen_height*4)))
        throw std::runtime_error("3DS mono dashboard flush failed");
    STARFOX_3DS_FRAME_PHASE(present);
    return gpu_bg2?impl_->display.present_mode2(ppu,game.map().ppu_vram_revision(),scroll.first,scroll.second,indexed,palette,reuse)
        :impl_->display.present(indexed,palette,reuse);
}
}
