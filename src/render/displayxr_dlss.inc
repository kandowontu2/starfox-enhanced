// Private state of the existing native game owner. The host's single trusted
// SDK outlives successful retirement; physical eyes never share viewport 99.
struct DlssImages {
    SDL_GPUDevice* device;
    std::array<GpuCalibratedDlss,2> resolve;
    std::array<SDL_GPUTexture*,2> motion{},guide_color{},ownership{},surface{},depth{};
    std::shared_ptr<const CalibratedGameFrame> accepted,pending;
    std::array<vr::EyeCamera,2> accepted_cameras{},pending_cameras{};
    std::array<float,2> accepted_jitter{},pending_jitter{};
    std::uint64_t accepted_sample{},pending_sample{};
    unsigned mode{},model{};bool held{},reuse{};
    explicit DlssImages(SDL_GPUDevice* gpu):device(gpu) {}
    bool retire() noexcept {
        for(auto& eye:resolve) if(!eye.retire()) return false;
        return true;
    }
    void settle(bool ok) noexcept {
        for(auto& eye:resolve) if(ok) eye.commit();else eye.discard();
        if(ok && pending) {
            accepted=std::move(pending);accepted_cameras=pending_cameras;
            accepted_sample=pending_sample;accepted_jitter=pending_jitter;
        } else pending.reset();
    }
    ~DlssImages() {
        for(const auto& group:{motion,guide_color,ownership,surface,depth})
            for(auto* texture:group) if(texture) SDL_ReleaseGPUTexture(device,texture);
    }
};
CalibratedDlssApi dlss_api;
std::unique_ptr<DlssImages> dlss;
// SDK frame tokens advance on attempts, not accepted presentations. A cancelled
// command may already have changed SDK CPU state. IDs survive quality changes.
unsigned dlss_frame_index{};
bool dlss_frame_end_pending{};
std::string dlss_frame_end_error;
bool finish_dlss_frame() noexcept {
    if(!dlss_frame_end_pending) return true;
    char error[512]{};
    if(!dlss_api.finish_frame || dlss_api.finish_frame(dlss_api.module,error,sizeof(error))) {
        dlss_frame_end_error=error[0]?error:"Native SDK frame-end unavailable";return false;
    }
    dlss_frame_end_pending=false;dlss_frame_end_error.clear();return true;
}
bool retire_dlss() noexcept {
    if(!dlss) return true;
    if(!dlss->retire()) return false;
    dlss.reset();return true;
}
void configure_dlss(unsigned mode,unsigned model) {
    if(dlss && dlss->mode==mode && dlss->model==model) return;
    if(!retire_dlss()) throw std::runtime_error("Native DLSS SDK viewport retirement failed");
    if(!mode) return;
    auto next=std::make_unique<DlssImages>(device);next->mode=mode;next->model=model;
    for(unsigned eye=0;eye<2;++eye) {
        const auto full=output_extent[eye];
        if(!next->resolve[eye].initialize(device,format,dlss_api,100+eye,mode,model,{full[0],full[1]}))
            throw std::runtime_error(next->resolve[eye].status());
        const auto input=next->resolve[eye].input_extent();
        const auto make=[&](SDL_GPUTextureFormat fmt,SDL_GPUTextureUsageFlags usage) {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=fmt;
            info.width=input.width;info.height=input.height;info.layer_count_or_depth=info.num_levels=1;info.usage=usage;
            auto* texture=SDL_CreateGPUTexture(device,&info);if(!texture) throw std::runtime_error(SDL_GetError());return texture;
        };
        const auto usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        next->guide_color[eye]=make(format,usage);
        next->ownership[eye]=make(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,usage);
        next->surface[eye]=make(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,usage);
        next->motion[eye]=make(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,usage);
        next->depth[eye]=make(SDL_GPU_TEXTUREFORMAT_D32_FLOAT,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    }
    dlss=std::move(next);
}
void prepare_dlss(const std::array<vr::EyeCamera,2>& cameras) {
    // Includes held presentations; polls must not run this bookkeeping twice.
    // Failed encoding retains the obligation until submitted work is drained.
    dlss_frame_end_pending=true;
    auto signature=frame->settings;signature.effect_seconds=0;signature.exposure_paused=false;signature.camera_response_pose={};
    dlss->reuse=bool(dlss->accepted);
    if(dlss->reuse) {
        auto prior=dlss->accepted->settings;prior.effect_seconds=0;prior.exposure_paused=false;prior.camera_response_pose={};
        const auto& old=*dlss->accepted->current;const auto& now=*frame->current;
        dlss->reuse=signature==prior && now.scene_epoch==old.scene_epoch && now.flow==old.flow
            && (!calibrated_game_uses_effect_clock(frame->settings)
                || frame->settings.effect_seconds>=dlss->accepted->settings.effect_seconds)
            && !timing::camera_transform_is_discontinuous(old.camera,now.camera);
    }
    dlss->held=dlss->reuse && same_calibrated_presentation(*frame,*dlss->accepted);
    for(unsigned eye=0;eye<2;++eye) dlss->held=dlss->held
        && cameras[eye].view==dlss->accepted_cameras[eye].view
        && cameras[eye].projection==dlss->accepted_cameras[eye].projection
        && cameras[eye].effects==dlss->accepted_cameras[eye].effects;
    dlss->pending_sample=dlss->held?dlss->accepted_sample:dlss->reuse?dlss->accepted_sample+1:0;
    dlss->pending_jitter=dlss->held?dlss->accepted_jitter:temporal_jitter(dlss->pending_sample);
    dlss->pending_cameras=cameras;dlss->pending=frame;
    if(!dlss->held) {
        const auto token=next_calibrated_dlss_frame_index();
        if(!token) throw std::runtime_error("Native DLSS frame token exhausted");
        dlss_frame_index=*token;
    }
}
vr::EyeCamera dlss_camera(vr::EyeCamera camera,unsigned eye,bool previous=false) const noexcept {
    const auto jitter=previous?dlss->accepted_jitter:dlss->pending_jitter;
    camera.projection[8]-=2*jitter[0]/extent[eye][0];camera.projection[9]+=2*jitter[1]/extent[eye][1];
    return diagnostic_source_camera(camera,eye);
}
