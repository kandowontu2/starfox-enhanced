#include "native_mono.hpp"
#include "reference/frame_3ds.hpp"
#include "reference/display_3ds.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
#include "starfox/platform/nintendo_3ds/lcd_rgba.hpp"
#include "starfox/platform/nintendo_3ds/mono_presentation.hpp"
namespace starfox::platform::nintendo_3ds {
struct NativeMono::Impl {
    platform_3ds::Display3ds display;
    RgbaLcdCache dashboard;
    Impl() {
        if(!display.open()) {display.close();throw std::runtime_error("Reference 3DS mono display initialization failed");}
        // Physical slider stereo remains on our separate native-eye path.
        gfxSet3D(false);
    }
    ~Impl() {display.close();}
};
NativeMono::NativeMono():impl_(std::make_unique<Impl>()) {}
NativeMono::~NativeMono()=default;
bool NativeMono::present(GameSession& session,const GamePresentation& source,ImageView dashboard) {
    if(source.plan.stereo || !valid_image(dashboard,bottom_width,screen_height))
        throw std::invalid_argument("Incomplete reference mono frame");
    if(impl_->display.activate()) impl_->dashboard.invalidate();
    gfxSet3D(false);
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
    {
        STARFOX_3DS_FRAME_PHASE(lcd_copy);
        auto* lower=gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,nullptr,nullptr);
        if(!lower)throw std::runtime_error("3DS mono dashboard allocation failed");
        const std::span<std::uint8_t> output{lower,bottom_width*screen_height*4};
        if(impl_->dashboard.prepare(dashboard,output)) {
            if(R_FAILED(GSPGPU_FlushDataCache(lower,output.size())))
                throw std::runtime_error("3DS mono dashboard flush failed");
            impl_->dashboard.commit(output);
        }
    }
    STARFOX_3DS_FRAME_PHASE(present);
    const bool presented=!gpu_bg2?impl_->display.present(indexed,palette,reuse,nullptr,true)
        :present_mono_tiles_or_complete([&](bool retry) {
        const auto attempt=[&] {
            if(impl_->display.present_mode2(ppu,game.map().ppu_vram_revision(),
                scroll.first,scroll.second,indexed,palette,reuse,nullptr,retry))
                return MonoTileResult::presented;
            return impl_->display.last_frame_timing().gpu_begin_failed
                ?MonoTileResult::busy:MonoTileResult::rejected;
        };
        if(retry) { STARFOX_3DS_FRAME_PHASE(mono_retry);return attempt(); }
        return attempt();
    },[] { C3D_FrameSync(); },[&] {
        // Rejected atlas/quad draws still require a complete, covered scene.
        // The blocking SDK acquire joins any partial submission before texture
        // writes, without advancing simulation, audio or interpolation.
        STARFOX_3DS_FRAME_PHASE(mono_fallback);
        const auto& complete=renderer.draw(game,false,source.interpolation_alpha);
        return impl_->display.present(complete,palette,false,nullptr,true);
    });
    // The reference GPU links only the upper LCD. Citro3D's queue callback
    // therefore never swaps the lower screen we wrote directly above. Publish
    // its flushed back buffer once per completed frame, including upper-image
    // reuse, without also swapping the GPU-owned top screen.
    if(presented) gfxScreenSwapBuffers(GFX_BOTTOM,false);
    return presented;
}
}
