// Part of the existing retained game owner, not another submission/render loop.
// Physical shutter reconstruction precedes screen-space styles/AA/exposure:
// those then process the same blurred scene, rather than using distorted colour
// with undisplaced geometry guides. Ray/material finishes precede the shutter.
struct BlurImages {
    SDL_GPUDevice* device;
    CalibratedMotionHistory history;
    GpuCalibratedMotionBlur reconstruct;
    std::array<SDL_GPUTexture*,2> motion{},raw{},finished{},world{},world_receiver{},world_depth{},sample_world{},sample_receiver{},ray_world{};
    unsigned count{},factor{};
    bool ready{};
    explicit BlurImages(SDL_GPUDevice* gpu):device(gpu) {}
    ~BlurImages() {
        reconstruct.release_device();
        for(const auto& group:{motion,raw,finished,world,world_receiver,world_depth,sample_world,sample_receiver,ray_world})
            for(auto* t:group) if(t) SDL_ReleaseGPUTexture(device,t);
    }
};
std::unique_ptr<BlurImages> blur;
void prepare_blur(unsigned count,bool water_layers=false) {
    std::uint64_t pixels=0;
    for(unsigned eye=0;eye<2;++eye) {
        const auto size=scene_extent(eye);
        const auto area=std::uint64_t(size[0])*size[1];
        if(!size[0] || !size[1] || size[0]>16384 || size[1]>16384 || area>1024ULL*1024*1024/88)
            throw std::runtime_error("Native blur exceeds its per-eye one-GiB scratch working bound");
        pixels+=area;
    }
    // Existing histories/ray/depth/scratch plus shutter/underlay. Actual MSAA
    // sample storage gets its stronger combined bound in prepare_msaa().
    if(pixels*(492+(water_layers?20+40*count:0)+(frame->settings.volumetric_fog?32:0))+64ULL*1024*1024>4ULL*1024*1024*1024)
        throw std::runtime_error("Native blur exceeds the four-GiB resident eye/history working bound");
    if(blur->ready) return;
    struct Properties {SDL_PropertiesID id{SDL_CreateProperties()};~Properties(){if(id) SDL_DestroyProperties(id);}} props;
    if(count>1 && (!props.id || !SDL_SetBooleanProperty(props.id,STARFOX_SDL_MULTISAMPLE_READ,true)))
        throw std::runtime_error(SDL_GetError());
    const auto make=[&](unsigned eye,SDL_GPUTextureFormat fmt,SDL_GPUTextureUsageFlags usage,unsigned samples=1) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=fmt;
        const auto size=scene_extent(eye);info.width=size[0];info.height=size[1];
        info.layer_count_or_depth=info.num_levels=1;info.usage=usage;
        info.sample_count=samples==2?SDL_GPU_SAMPLECOUNT_2:samples==4?SDL_GPU_SAMPLECOUNT_4:samples==8?SDL_GPU_SAMPLECOUNT_8:SDL_GPU_SAMPLECOUNT_1;
        info.props=samples>1?props.id:0;
        auto* t=SDL_CreateGPUTexture(device,&info);if(!t) throw std::runtime_error(SDL_GetError());return t;
    };
    const auto usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    // A failure leaves the owner intact for normal retained teardown; ready
    // is set only once both eyes and the reconstruction adapter exist.
    for(unsigned eye=0;eye<2;++eye) {
        if(!blur->motion[eye]) blur->motion[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,usage);
        if(!blur->raw[eye]) blur->raw[eye]=make(eye,format,usage);
        if(!blur->finished[eye]) blur->finished[eye]=make(eye,format,usage);
        if(!blur->ray_world[eye]) blur->ray_world[eye]=make(eye,format,usage);
        if(!blur->world[eye]) blur->world[eye]=make(eye,format,usage,count);
        if(!blur->world_receiver[eye]) blur->world_receiver[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,usage,count);
        if(!blur->world_depth[eye]) blur->world_depth[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,count);
        if(count>1) {
            if(!blur->sample_world[eye]) blur->sample_world[eye]=make(eye,format,usage);
            if(!blur->sample_receiver[eye]) blur->sample_receiver[eye]=make(eye,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,usage);
        }
    }
    if(!blur->reconstruct.initialize(device,format)) throw std::runtime_error(blur->reconstruct.status());
    blur->count=count;blur->factor=sample_factor;blur->ready=true;
}
