#pragma once
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "starfox/render/calibrated_effects.hpp"
#include "starfox/render/calibrated_ground_receiver.hpp"
#include <array>
#include <memory>
#include <string>

namespace starfox::render {
// Optional R32_FLOAT MRT. Each pass stores three exact bits: present, local
// edge branch, screen-grid branch. The caller owns both non-aliased atlases.
struct CalibratedEdgeCapture {void* prior{};void* destination{};unsigned slot{};};
// Native eye composition, not a flat-screen reprojection. The receiver texture
// comes from the calibrated raster's exact visible fragments. Source/destination
// use the negotiated runtime format; the receiver is RGBA8_UNORM. All textures
// must have the supplied extent and belong to the initialized device.
class GpuCalibratedRayComposite {
public:
    GpuCalibratedRayComposite();~GpuCalibratedRayComposite();
    GpuCalibratedRayComposite(const GpuCalibratedRayComposite&)=delete;
    GpuCalibratedRayComposite& operator=(const GpuCalibratedRayComposite&)=delete;
    bool initialize(void* device,int color_format);
    // Encodes into the caller's command: no submission, GPU wait or readback.
    // Borrowed inputs must remain alive until that command completes. Null ray
    // inputs with material/effects OFF give an exact source copy (including alpha).
    // Traced reflection words use RGBA byte order in the target's declared
    // colour encoding, regardless of its RGBA/BGRA storage channel order.
    // water_world selects the separately traced hidden liquid plane. It needs
    // validated auxiliary layers and a rasterized world-only source/receiver;
    // it preserves world sky and alpha, and accepts no model/material styles.
    bool enqueue(void* command,void* source,void* receiver,void* destination,
        unsigned width,unsigned height,const shadows::GpuShadowOutput* shadow=nullptr,
        const shadows::GpuReflectionOutput* reflection=nullptr,
        float shadow_strength=1,float reflection_strength=1,
        unsigned material=0,unsigned material_scale=1,CalibratedPostEffects effects={},bool water_world=false,
        CalibratedEdgeCapture edge_capture={});
    // Same order as flat presentation: primary style, spatial manipulation,
    // special FX. Ping-pong images stay GPU-resident and owned across retries.
    bool enqueue_sequence(void* command,void* source,void* receiver,void* destination,
        unsigned width,unsigned height,const shadows::GpuShadowOutput* shadow,
        const shadows::GpuReflectionOutput* reflection,float shadow_strength,float reflection_strength,
        unsigned material,unsigned material_scale,const std::array<CalibratedPostEffects,3>& effects,
        void** edge_witness=nullptr,unsigned witness_bank=0);
    // The optional sequence atlas is compositor-owned and only valid until the
    // next same-bank sequence. Four banks retain AA and centre witnesses for
    // both eyes, including equal extents. The caller assigns banks 0..3.
    // OFF/non-edge selections allocate no atlas. No additional render pass.
    // Replace only actual visible liquid ownership/normal/depth. Dry and
    // protected guides remain exact. Destinations never alias inputs; surfaces
    // may be absent if only ownership is needed. No submission or readback.
    bool enqueue_water_guides(void* command,void* receiver,void* surfaces,
        void* destination_receiver,void* destination_surfaces,unsigned width,unsigned height,
        const shadows::GpuReflectionOutput&);
    // Unjittered full-panel liquid guides after reconstruction. Evaluate the
    // retained analytic plane and the SAME native ray-water normal field, not
    // a nearest/bilinear upscale of source-grid guides. Opaque nearer models,
    // protected artwork, dry sky and alpha remain unchanged. No trace/readback.
    bool enqueue_water_panel_guides(void* command,void* receiver,void* surfaces,
        void* destination_receiver,void* destination_surfaces,unsigned width,unsigned height,
        const vr::EyeCamera&,const CalibratedGroundRayReceiver&);
    // Optional decorative surface finish samples the composed native eye,
    // including shadows/reflections, before model/world styles and post-ray UI.
    // With both rays/materials and styles, a resident target-format resolve
    // preserves the flat renderer's quantization and discrete edge thresholds.
    // The scratch texture remains owned until this compositor is released.
    // Only selected opaque
    // receivers are styled; source alpha and excluded foreground stay exact.
    // Scale is the native eye's source-pixel neighbourhood step (>=1).
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
