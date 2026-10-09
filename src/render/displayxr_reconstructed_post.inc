// Full-panel post-reconstruction resources. Owned by the existing presenter,
// never a separate submission/history loop. Layout changes occur only after
// its retained GPU/native/XR work has drained.
struct ReconstructedPostImages {
    SDL_GPUDevice* device;
    std::array<SDL_GPUTexture*,2> raw{},first{},second{},surface{};
    std::array<SDL_GPUTexture*,2> liquid_owner{},liquid_surface{};
    bool has_surface{},has_liquid{};
    explicit ReconstructedPostImages(SDL_GPUDevice* gpu):device(gpu) {}
    ~ReconstructedPostImages() {
        for(const auto& group:{raw,first,second,surface,liquid_owner,liquid_surface}) for(auto* image:group)
            if(image) SDL_ReleaseGPUTexture(device,image);
    }
};
std::unique_ptr<ReconstructedPostImages> reconstructed_post;
void prepare_reconstructed_post(bool selected,const PassPlan& plan,unsigned samples,bool rays,bool liquid_guides,bool panel_liquid) {
    if(!selected) {reconstructed_post.reset();return;}
    const CalibratedPostExtents source{scene_extent(0),scene_extent(1)};
    std::uint64_t source_bytes=0,transition_bytes=0;
    if(samples>1) {
        const auto bytes=calibrated_msaa_working_bytes(source[0][0],source[0][1],source[1][0],source[1][1],
            samples,plan.needs_surfaces(),0,rays,plan.fog,plan.blur,liquid_guides);
        if(!bytes) throw std::runtime_error("Invalid reconstructed post sample working extent");
        source_bytes=*bytes;
        if(msaa && (msaa->count!=samples || msaa->surfaces!=plan.needs_surfaces() || msaa->has_motion!=plan.blur)) {
            // Charge the actual retained output capacities, not just this
            // frame's selected ray flags. A former liquid layout can be much
            // larger than the ordinary reflection output and stays resident
            // until the complete replacement sample owner is installed.
            transition_bytes=msaa->working_image_bytes(source);
        }
    }
    if(!calibrated_reconstructed_post_extent_supported(source,output_extent,source_bytes,transition_bytes))
        throw std::runtime_error("Native reconstructed post exceeds the resident source/panel working bound");
    const bool has_surface=plan.needs_surfaces() || panel_liquid;
    if(reconstructed_post && reconstructed_post->has_surface==has_surface && reconstructed_post->has_liquid==panel_liquid) return;
    auto next=std::make_unique<ReconstructedPostImages>(device);next->has_surface=has_surface;next->has_liquid=panel_liquid;
    const auto make=[&](unsigned eye,SDL_GPUTextureFormat texture_format) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=texture_format;
        info.width=output_extent[eye][0];info.height=output_extent[eye][1];
        info.layer_count_or_depth=info.num_levels=1;
        info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        auto* image=SDL_CreateGPUTexture(device,&info);
        if(!image) throw std::runtime_error(SDL_GetError());return image;
    };
    for(unsigned eye=0;eye<2;++eye) {
        next->raw[eye]=make(eye,format);next->first[eye]=make(eye,format);next->second[eye]=make(eye,format);
        if(next->has_surface) next->surface[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
        if(panel_liquid) {
            next->liquid_owner[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            next->liquid_surface[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
        }
    }
    reconstructed_post=std::move(next);
}
void finish_reconstructed_post(SDL_GPUCommandBuffer* command,unsigned eye,const vr::EyeCamera& camera,
    std::span<const CalibratedScenePacket> packets,const PassPlan& plan,
    const std::array<CalibratedPostEffects,3>& styles) {
    const auto size=output_extent[eye];auto after=plan;after.aa=false;
    auto* ownership=fsr_receiver[eye];auto* surface=reconstructed_post->surface[eye];
    if(reconstructed_post->has_liquid) {
        std::optional<CalibratedGroundRayReceiver> ground;
        for(const auto& p:packets) if(p.ground_receiver && calibrated_ground_material(*p.packet)==5) {
            if(ground) throw std::runtime_error("Multiple reconstructed liquid receivers");
            ground=calibrated_ground_ray_receiver(*p.packet,camera,float(frame->settings.reflections)/3,
                false,p.model_override?&*p.model_override:nullptr,frame->settings.water_caustics);
        }
        if(!ground) throw std::runtime_error("Reconstructed liquid lost its retained ground plane");
        if(!composite.enqueue_water_panel_guides(command,ownership,surface,reconstructed_post->liquid_owner[eye],
            reconstructed_post->liquid_surface[eye],size[0],size[1],camera,*ground)) throw std::runtime_error(composite.status());
        ownership=reconstructed_post->liquid_owner[eye];surface=reconstructed_post->liquid_surface[eye];
    }
    auto* styled=after.finishing()?reconstructed_post->first[eye]:color[eye];
    const auto scale=std::max(1U,unsigned(std::lround(double(size[1])/224)));
    if(!composite.enqueue_sequence(command,reconstructed_post->raw[eye],ownership,styled,
        size[0],size[1],nullptr,nullptr,1,0,0,scale,styles)) throw std::runtime_error(composite.status());
    if(after.finishing()) {
        const FinishGrid grid{size,reconstructed_post->first[eye],reconstructed_post->second[eye]};
        const PlaneOwners owners{&persistence[eye],&persistence[2+eye],&persistence[4+eye],&exposure[eye]};
        finish_plane(command,eye,camera,packets,after,owners,styled,color[eye],ownership,surface,nullptr,&grid);
    }
}
