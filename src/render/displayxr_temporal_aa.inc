// Private resident resources and accepted-frame state of the existing owner.
// Allocate only for selected TAA, after pending native/XR work has retired.
struct TaaImages {
    SDL_GPUDevice* device;
    std::array<GpuCalibratedTemporalAa,2> resolve;
    std::array<SDL_GPUTexture*,2> motion{},resolved{},centre_styled{},centre_surface{};
    // Borrowed from the compositor; its four distinct banks keep both eyes'
    // AA/centre atlases alive. Never release these from the temporal owner.
    std::array<void*,2> edge_witness{},centre_edge_witness{};
    std::shared_ptr<const CalibratedGameFrame> accepted,pending;
    std::array<vr::EyeCamera,2> accepted_cameras{},pending_cameras{};
    std::array<float,2> accepted_jitter{},pending_jitter{};
    std::uint64_t accepted_epoch{},pending_epoch{},accepted_sample{},pending_sample{};
    bool held{};
    explicit TaaImages(SDL_GPUDevice* gpu):device(gpu) {}
    void settle(bool ok) noexcept {
        for(auto& eye:resolve) if(ok) eye.commit();else eye.discard();
        if(ok && pending) {
            accepted=std::move(pending);accepted_cameras=pending_cameras;
            accepted_epoch=pending_epoch;accepted_sample=pending_sample;accepted_jitter=pending_jitter;
        } else pending.reset();
    }
    ~TaaImages() {
        for(auto& eye:resolve) eye.release_device();
        for(const auto& group:{motion,resolved,centre_styled,centre_surface})
            for(auto* texture:group) if(texture) SDL_ReleaseGPUTexture(device,texture);
    }
};
std::unique_ptr<TaaImages> taa;
void prepare_taa(const std::array<vr::EyeCamera,2>& cameras) {
    const auto& settings=frame->settings;
    std::uint64_t pixels=0;
    for(const auto size:extent) {
        if(!size[0] || !size[1] || size[0]>16384 || size[1]>16384) throw std::runtime_error("Invalid native TAA extent");
        pixels+=std::uint64_t(size[0])*size[1];
    }
    // Includes guides/centre raster/scratch, accepted history pairs, the three
    // possible retained trail/phosphor histories, exposure and ray outputs.
    // Those owners can retain allocations after their options are disabled.
    // Geometry/AS/driver storage is separate; this is not a free-VRAM promise.
    const bool fog=settings.volumetric_fog!=0;
    if(pixels*(336+(fog?32:0)+(settings.motion_blur?160:0)+(settings.reflections?156:0))+(fog?64ULL*1024*1024:0)>4ULL*1024*1024*1024)
        throw std::runtime_error("Native TAA exceeds the four-GiB eye/history working bound");
    const auto make=[&](unsigned eye,SDL_GPUTextureFormat fmt,SDL_GPUTextureUsageFlags usage) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=fmt;
        info.width=extent[eye][0];info.height=extent[eye][1];info.layer_count_or_depth=info.num_levels=1;info.usage=usage;
        auto* texture=SDL_CreateGPUTexture(device,&info);if(!texture) throw std::runtime_error(SDL_GetError());return texture;
    };
    const auto usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if(!taa) {
        auto next=std::make_unique<TaaImages>(device);
        for(unsigned eye=0;eye<2;++eye) {
            if(!next->resolve[eye].initialize(device,format)) throw std::runtime_error(next->resolve[eye].status());
            next->motion[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,usage);
            next->resolved[eye]=make(eye,format,usage);next->centre_styled[eye]=make(eye,format,usage);
            next->centre_surface[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,usage);
        }
        taa=std::move(next);
    }
    taa->edge_witness.fill(nullptr);taa->centre_edge_witness.fill(nullptr);
    for(unsigned eye=0;eye<2;++eye) {
        if(!native_ink[eye]) native_ink[eye]=make(eye,format,usage);
        if(!native_receiver[eye]) native_receiver[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,usage);
        if(!native_depth[eye]) native_depth[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    }
    auto signature=settings;signature.effect_seconds=0;signature.exposure_paused=false;signature.camera_response_pose={};
    bool reuse=bool(taa->accepted);
    if(reuse) {
        auto prior=taa->accepted->settings;prior.effect_seconds=0;prior.exposure_paused=false;prior.camera_response_pose={};
        const auto& old=*taa->accepted->current;const auto& now=*frame->current;
        reuse=signature==prior && now.scene_epoch==old.scene_epoch && now.flow==old.flow
            && (!calibrated_game_uses_effect_clock(settings) || settings.effect_seconds>=taa->accepted->settings.effect_seconds)
            && !timing::camera_transform_is_discontinuous(old.camera,now.camera);
    }
    taa->pending_epoch=taa->accepted_epoch+unsigned(!reuse);
    taa->held=reuse && same_calibrated_presentation(*frame,*taa->accepted);
    for(unsigned eye=0;eye<2;++eye) taa->held=taa->held
        && cameras[eye].view==taa->accepted_cameras[eye].view
        && cameras[eye].projection==taa->accepted_cameras[eye].projection
        && cameras[eye].effects==taa->accepted_cameras[eye].effects;
    taa->pending_sample=taa->held?taa->accepted_sample:reuse?taa->accepted_sample+1:0;
    taa->pending_jitter=taa->held?taa->accepted_jitter:temporal_jitter(taa->pending_sample);
    taa->pending_cameras=cameras;taa->pending=frame;
}
vr::EyeCamera taa_camera(vr::EyeCamera camera,unsigned eye,bool previous=false) const noexcept {
    const auto jitter=previous?taa->accepted_jitter:taa->pending_jitter;
    camera.projection[8]-=2*jitter[0]/extent[eye][0];camera.projection[9]+=2*jitter[1]/extent[eye][1];
    return camera;
}
