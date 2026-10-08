// Private members of DisplayXrGameRenderer::State. No new standalone owner,
// submission loop or simulation clock: the existing presenter retains all work.
struct PassPlan {
    unsigned model_mode{},model_strength{},world_mode{},world_strength{};
    bool model_history{},world_history{},phosphor{},globals{},appearance{},bloom{},exposure{},depth{},aa{},fx{},fog{},blur{},liquid_centre{};
    explicit PassPlan(const CalibratedGameSettings& s,bool spatial_aa=false,bool scene_fx=false,bool physical_blur=false,
        bool physical_liquid_centre=false) {
        model_mode=persistence_mode(static_cast<Effect>(s.manipulation?s.manipulation:s.model_effects[0]));
        model_strength=s.manipulation?s.manipulation_intensity:s.model_effects[1];
        world_mode=persistence_mode(static_cast<Effect>(s.extra_effects[0]?s.extra_effects[0]:s.world_effects[0]));
        world_strength=s.extra_effects[0]?100:s.world_effects[1];
        model_history=model_mode && model_strength;world_history=world_mode && world_strength;
        phosphor=s.phosphor!=0;globals=s.global_enhancements!=0;appearance=s.contrast || s.chromatic;
        bloom=s.bloom_model || s.bloom_world;exposure=s.exposure!=0;depth=s.depth_enhancements!=0;aa=spatial_aa;fx=scene_fx;
        fog=s.volumetric_fog!=0;
        blur=physical_blur;liquid_centre=physical_liquid_centre;
    }
    bool temporal() const noexcept {return model_history || world_history || phosphor;}
    bool reconstruct_first(const std::array<CalibratedPostEffects,3>& styles) const noexcept {
        // Pattern/edge phase fallbacks otherwise contain the primary submerged
        // actor's colour, not the liquid's optical radiance. Reconstruct raw
        // water first, then style the independently classified centre image.
        return liquid_centre || fx || depth || appearance || globals || bloom || fog || calibrated_post_after_reconstruction(styles);
    }
    bool finishing() const noexcept {return temporal() || globals || appearance || bloom || exposure || depth || aa || fx || fog;}
    bool active() const noexcept {return finishing() || blur;}
    bool needs_surfaces() const noexcept {return depth || fx || fog || blur;}
    unsigned histories() const noexcept {return unsigned(model_history)+unsigned(world_history)+unsigned(phosphor);}
};
struct PlaneOwners {
    GpuCalibratedPersistence *world{},*model{},*phosphor{};GpuCalibratedExposure* exposure{};
};
struct FinishGrid {
    std::array<std::uint32_t,2> size{};
    SDL_GPUTexture *first{},*second{};
};
struct MsaaSample {
    SDL_GPUDevice* device;SDL_GPUTexture* finished{};
    shadows::SdlDxrShadows shadow{true},reflection{true};
    std::array<GpuCalibratedPersistence,3> history;
    GpuCalibratedExposure exposure;std::array<bool,3> history_ready{};bool exposure_ready{};
    explicit MsaaSample(SDL_GPUDevice* gpu):device(gpu) {}
    void prepare(const PassPlan& plan,SDL_GPUTextureFormat format) {
        const std::array<bool,3> active{plan.world_history,plan.model_history,plan.phosphor};
        for(unsigned i=0;i<3;++i) {
            if(active[i] && !history_ready[i]) {
                const bool ok=history[i].initialize(device,format);if(!ok) throw std::runtime_error(history[i].status());history_ready[i]=true;
            } else if(!active[i] && history_ready[i]) {history[i].release_device();history_ready[i]=false;}
        }
        if(plan.exposure && !exposure_ready) {
            const bool ok=exposure.initialize(device,format);if(!ok) throw std::runtime_error(exposure.status());exposure_ready=true;
        } else if(!plan.exposure && exposure_ready) {exposure.release_device();exposure_ready=false;}
    }
    PlaneOwners owners() noexcept {return {&history[0],&history[1],&history[2],&exposure};}
    ~MsaaSample() {if(finished) SDL_ReleaseGPUTexture(device,finished);}
};
struct MsaaImages {
    SDL_GPUDevice* device;unsigned count{};bool surfaces{},has_motion{};
    std::array<SDL_GPUTexture*,2> color{},receiver{},surface{},depth{},motion{};
    std::array<std::array<std::unique_ptr<MsaaSample>,8>,2> samples;
    GpuCalibratedMsaa transport;
    explicit MsaaImages(SDL_GPUDevice* gpu):device(gpu) {}
    bool native_complete() const noexcept {
        for(const auto& eye:samples) for(const auto& sample:eye) if(sample
            && (!sample->shadow.native_work_complete() || !sample->reflection.native_work_complete())) return false;
        return true;
    }
    void settle_history(bool accepted) noexcept {
        for(auto& eye:samples) for(auto& sample:eye) if(sample) {
            for(auto& trail:sample->history) if(accepted) trail.commit();else trail.cancel();
            if(accepted) sample->exposure.commit();else sample->exposure.cancel();
        }
    }
    std::uint64_t working_image_bytes(const CalibratedPostExtents& extents) const noexcept {
        std::uint64_t bytes=0;
        for(unsigned eye=0;eye<2;++eye) {
            const auto pixels=std::uint64_t(extents[eye][0])*extents[eye][1];
            bytes+=pixels*count*(12+(surfaces?16:0)+(has_motion?16:0));
            for(const auto& sample:samples[eye]) if(sample) {
                if(sample->finished) bytes+=pixels*4;
                bytes+=sample->shadow.working_image_bytes()+sample->reflection.working_image_bytes()
                    +sample->exposure.working_image_bytes();
                for(const auto& history:sample->history) bytes+=history.working_image_bytes();
            }
        }
        return bytes;
    }
    ~MsaaImages() {
        for(const auto& group:{color,receiver,surface,depth,motion}) for(auto* texture:group) if(texture) SDL_ReleaseGPUTexture(device,texture);
    }
};
std::unique_ptr<MsaaImages> msaa;
void prepare_msaa(unsigned count,const PassPlan& plan,bool rays,bool water_layers=false,bool panel_history=false) {
    if(!calibrated_msaa_extent_supported(extent[0][0],extent[0][1],extent[1][0],extent[1][1],count,plan.needs_surfaces(),plan.histories(),rays,plan.fog,plan.blur,water_layers))
        throw std::runtime_error("Native MSAA exceeds the four-GiB resident eye/history working limit");
    if(!scene.supports_multisample(count,true,plan.needs_surfaces())) throw std::runtime_error("Native eye formats do not support the requested MSAA count");
    // OFF/SSAA owners cannot retain another full set of eye-sized histories or
    // ray buffers behind the bounded multisample allocation. No work is pending.
    if(!panel_history && history_ready) {for(auto& h:persistence) h.release_device();history_ready=false;}
    if(!panel_history && exposure_ready) {for(auto& meter:exposure) meter.release_device();exposure_ready=false;}
    if(!msaa) for(unsigned eye=0;eye<2;++eye) {shadow[eye].release_device();reflection[eye].release_device();}
    const auto make_texture=[&](unsigned eye,SDL_GPUTextureFormat texture_format,SDL_GPUTextureUsageFlags usage,
        SDL_GPUSampleCount samples,SDL_PropertiesID props=0) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=texture_format;
        info.width=extent[eye][0];info.height=extent[eye][1];info.layer_count_or_depth=info.num_levels=1;
        info.usage=usage;info.sample_count=samples;info.props=props;
        auto* texture=SDL_CreateGPUTexture(device,&info);if(!texture) throw std::runtime_error(SDL_GetError());return texture;
    };
    if(!msaa || msaa->count!=count || msaa->surfaces!=plan.needs_surfaces() || msaa->has_motion!=plan.blur) {
        auto next=std::make_unique<MsaaImages>(device);next->count=count;next->surfaces=plan.needs_surfaces();next->has_motion=plan.blur;
        struct Properties {SDL_PropertiesID id{SDL_CreateProperties()};~Properties(){if(id) SDL_DestroyProperties(id);}} props;
        if(!props.id || !SDL_SetBooleanProperty(props.id,STARFOX_SDL_MULTISAMPLE_READ,true)) throw std::runtime_error(SDL_GetError());
        const auto sample_count=count==2?SDL_GPU_SAMPLECOUNT_2:count==4?SDL_GPU_SAMPLECOUNT_4:SDL_GPU_SAMPLECOUNT_8;
        const auto usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        for(unsigned eye=0;eye<2;++eye) {
            next->color[eye]=make_texture(eye,format,usage,sample_count,props.id);
            next->receiver[eye]=make_texture(eye,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,usage,sample_count,props.id);
            if(plan.needs_surfaces()) next->surface[eye]=make_texture(eye,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,usage,sample_count,props.id);
            if(plan.blur) next->motion[eye]=make_texture(eye,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,usage,sample_count,props.id);
            next->depth[eye]=make_texture(eye,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,sample_count);
            for(unsigned sample=0;sample<count;++sample) {
                auto owner=std::make_unique<MsaaSample>(device);owner->finished=make_texture(eye,format,usage,SDL_GPU_SAMPLECOUNT_1);
                next->samples[eye][sample]=std::move(owner);
            }
        }
        const bool ok=next->transport.initialize(device,format);if(!ok) throw std::runtime_error(next->transport.status());
        msaa=std::move(next);
    }
    // Centre-resolution ink/depth is shared across samples, never averaged.
    for(unsigned eye=0;eye<2;++eye) {
        if(!native_ink[eye]) native_ink[eye]=make_texture(eye,format,SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER,SDL_GPU_SAMPLECOUNT_1);
        if(!native_receiver[eye]) native_receiver[eye]=make_texture(eye,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER,SDL_GPU_SAMPLECOUNT_1);
        if(!native_depth[eye]) native_depth[eye]=make_texture(eye,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,SDL_GPU_SAMPLECOUNT_1);
        for(unsigned sample=0;sample<count;++sample) msaa->samples[eye][sample]->prepare(plan,format);
    }
}
float player_focus(const vr::EyeCamera& camera,std::span<const CalibratedScenePacket> packets) const {
    float focus=500;
    if(frame->current->player) for(unsigned i=0;i<frame->draws.size();++i) {
        const auto& draw=frame->draws[i];
        if(draw.source_key!=frame->current->player || draw.layer!=CalibratedGameLayer::model || draw.after_rays) continue;
        const auto transformed=vr::model_eye_camera(camera,packets[i].model_override.value_or(draw.packet.model));
        if(transformed && -transformed->view[14]*256>32) focus=-transformed->view[14]*256;break;
    }
    return focus;
}
void finish_plane(SDL_GPUCommandBuffer* command,unsigned eye,const vr::EyeCamera& camera,
    std::span<const CalibratedScenePacket> packets,const PassPlan& p,PlaneOwners owners,
    SDL_GPUTexture* styled,SDL_GPUTexture* final_color,SDL_GPUTexture* ownership=nullptr,SDL_GPUTexture* surface=nullptr,
    const shadows::GpuReflectionOutput* liquid=nullptr,const FinishGrid* grid=nullptr) {
    const auto require=[](bool ok,const std::string& error){if(!ok) throw std::runtime_error(error);};
    const auto& settings=frame->settings;const auto size=grid?grid->size:scene_extent(eye);
    if(!ownership) ownership=receiver[eye];if(!surface) surface=surfaces[eye];
    const unsigned scale=std::max(1U,unsigned(std::lround(double(size[1])/224)));
    const auto first=grid?grid->first:temporal_input[eye],second=grid?grid->second:temporal_middle[eye];
    const auto scratch=[&](SDL_GPUTexture* input){return input==first?second:first;};
    const auto post=calibrated_game_post_effects(settings);
    if(p.aa && settings.aa_type==4 && p.reconstruct_first(post)) {
        // Do not attach rigid-fragment motion to a multi-tap/warped colour.
        // Only geometric/ray/material radiance enters this accepted history.
        // All ordered styles and finishing passes run once on the reconstructed
        // centre grid afterwards, using its independently rasterized ownership
        // and surfaces. The existing liquid rejection and transactional owner
        // still govern history; no feedback of post-effect pixels into it.
        auto* remembered=taa->held?taa->resolve[eye].accepted_colour(size[0],size[1],taa->pending_epoch):nullptr;
        if(!remembered) {
            constexpr float weights[]{0,.65F,.8F,.9F};
            const bool ok=taa->resolve[eye].enqueue(command,styled,ownership,surface,taa->motion[eye],taa->resolved[eye],
                size[0],size[1],{taa->pending_epoch,p.blur?0:weights[settings.aa_quality],0,{},{},settings.reflections!=0},liquid);
            require(ok,taa->resolve[eye].status());remembered=taa->resolved[eye];
        }
        auto* reconstructed=scratch(styled);
        require(taa->resolve[eye].enqueue_presentation(command,remembered,ownership,surface,native_ink[eye],
            native_receiver[eye],reconstructed,size[0],size[1],taa->pending_jitter),taa->resolve[eye].status());
        auto after=p;after.aa=false;
        auto* effected=after.finishing()?scratch(reconstructed):final_color;
        require(composite.enqueue_sequence(command,reconstructed,native_receiver[eye],effected,size[0],size[1],
            nullptr,nullptr,1,0,0,scale,post),composite.status());
        if(after.finishing()) finish_plane(command,eye,taa->pending_cameras[eye],packets,after,owners,
            effected,final_color,native_receiver[eye],taa->centre_surface[eye]);
        return;
    }
    if(p.fx) {
        auto* output=p.depth || p.temporal() || p.globals || p.bloom || p.appearance || p.exposure || p.aa || p.fog?scratch(styled):final_color;
        auto world=frame->scene_fx;
        if(p.blur) {
            // Particles already participate in every joint model shutter tap;
            // keep only lights/shockwaves/heat in the ordinary finish pass.
            unsigned count=0;
            for(unsigned n=0;n<world.count;++n) if(world.points[n].type<2 || world.points[n].type>7)
                world.points[count++]=world.points[n];
            world.count=count;
        }
        const bool ok=scene_effects.enqueue(command,styled,ownership,surface,output,size[0],size[1],scale,
            world,camera,frame->scene_fx_rig(),frame->scene_fx_origin);
        require(ok,scene_effects.status());styled=output;
    }
    if(p.depth) {
        auto* output=p.temporal() || p.globals || p.bloom || p.appearance || p.exposure || p.aa || p.fog?scratch(styled):final_color;
        const bool ok=depth_effect.enqueue(command,styled,ownership,surface,output,size[0],size[1],scale,settings.depth_enhancements,camera,player_focus(camera,packets));
        require(ok,depth_effect.status());styled=output;
    }
    if(p.appearance) {
        auto* output=p.temporal() || p.globals || p.bloom || p.exposure || p.aa || p.fog?scratch(styled):final_color;
        const bool ok=global.enqueue_appearance(command,styled,ownership,output,size[0],size[1],scale,settings.contrast,settings.chromatic);
        require(ok,global.status());styled=output;
    }
    if(p.globals) {
        auto* output=p.temporal() || p.bloom || p.exposure || p.aa || p.fog?scratch(styled):final_color;
        const bool ok=global.enqueue(command,styled,ownership,output,size[0],size[1],scale,settings.global_enhancements,settings.effect_seconds);
        require(ok,global.status());styled=output;
    }
    if(p.bloom) {
        auto* output=p.temporal() || p.exposure || p.aa || p.fog?scratch(styled):final_color;
        const bool ok=bloom.enqueue(command,styled,ownership,output,size[0],size[1],scale,settings.bloom_model,settings.bloom_world);
        require(ok,bloom.status());styled=output;
    }
    if(p.fog) {
        // Geometry is view-space; sample/TAA offsets alter projection centres,
        // not the eye/model transforms. Use this exact raster's camera, guides
        // and directional light, including the unjittered TAA centre plane.
        auto geometry=fog_geometry[eye];const auto& projection=camera.projection;
        geometry.width=size[0];geometry.height=size[1];
        geometry.projection={double(size[0])*projection[0]/2,double(size[1])*projection[5]/2,
            double(size[0])*(1-projection[8])/2,double(size[1])*(1+projection[9])/2};
        const auto light=calibrated_game_light(camera,frame->source_light,settings.camera_response_pose);
        require(bool(light),"Invalid calibrated volumetric source light");
        auto* output=p.temporal() || p.exposure || p.aa?scratch(styled):final_color;
        const bool ok=volumetric.enqueue(command,styled,ownership,surface,output,geometry,
            volumetric_fog_medium(settings.volumetric_fog),{(*light)[0],(*light)[1],(*light)[2]});
        require(ok,volumetric.status());styled=output;
    }
    if(p.aa) {
        auto* output=p.temporal() || p.exposure?scratch(styled):final_color;
        if(settings.aa_type==4) {
            const auto styles=calibrated_game_post_effects(settings);
            const auto patterns=calibrated_pattern_guide(styles,scale);
            const auto edges=calibrated_edge_guide(styles,scale);
            auto* remembered=taa->held?taa->resolve[eye].accepted_colour(size[0],size[1],taa->pending_epoch):nullptr;
            if(!remembered) {
                constexpr float weights[]{0,.65F,.8F,.9F};
                unsigned rejected_layers=0;
                for(const auto& pass:styles) rejected_layers|=calibrated_post_motion_rejection_layers(pass,true,taa->edge_witness[eye]!=nullptr);
                const bool reactive=p.blur && (p.fx || p.fog || std::any_of(styles.begin(),styles.end(),[](const auto& pass){return pass.active();}));
                const bool ok=taa->resolve[eye].enqueue(command,styled,ownership,surface,taa->motion[eye],taa->resolved[eye],
                    size[0],size[1],{taa->pending_epoch,reactive?0:weights[settings.aa_quality],rejected_layers,patterns,edges,settings.reflections!=0},liquid,taa->edge_witness[eye]);
                require(ok,taa->resolve[eye].status());remembered=taa->resolved[eye];
            }
            const bool centre_appearance=p.fx || p.depth || p.appearance || p.globals || p.bloom || p.fog;
            const bool ok=taa->resolve[eye].enqueue_presentation(command,remembered,ownership,surface,
                centre_appearance?native_ink[eye]:taa->centre_styled[eye],native_receiver[eye],output,
                size[0],size[1],taa->pending_jitter,patterns,edges,taa->edge_witness[eye],taa->centre_edge_witness[eye]);
            require(ok,taa->resolve[eye].status());
            // The following screen-space trails/exposure own the reconstructed
            // centre grid, not this frame's shifted raster guides.
            ownership=native_receiver[eye];
        } else {
            const bool ok=aa[eye].enqueue(command,styled,ownership,output,size[0],size[1],settings.aa_type,settings.aa_quality);
            require(ok,aa[eye].status());
        }
        styled=output;
    }
    if(p.world_history) {
        auto* output=p.model_history || p.phosphor || p.exposure?scratch(styled):final_color;
        const bool ok=owners.world->enqueue(command,styled,ownership,output,size[0],size[1],{p.world_mode,p.world_strength,scale,false,true,settings.effect_seconds,history_epoch});
        require(ok,owners.world->status());styled=output;
    }
    if(p.model_history) {
        auto* output=p.phosphor || p.exposure?scratch(styled):final_color;
        const bool ok=owners.model->enqueue(command,styled,ownership,output,size[0],size[1],{p.model_mode,p.model_strength,scale,true,false,settings.effect_seconds,history_epoch});
        require(ok,owners.model->status());styled=output;
    }
    if(p.exposure) {
        auto* output=p.phosphor?scratch(styled):final_color;
        const bool ok=owners.exposure->enqueue(command,styled,ownership,output,size[0],size[1],{settings.exposure,settings.effect_seconds,history_epoch,settings.exposure_paused});
        require(ok,owners.exposure->status());styled=output;
    }
    if(p.phosphor) {
        const bool ok=owners.phosphor->enqueue(command,styled,ownership,final_color,size[0],size[1],{3,100,scale,true,true,settings.effect_seconds,history_epoch,settings.phosphor});
        require(ok,owners.phosphor->status());
    }
}
