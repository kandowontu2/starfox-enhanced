// Actual game owner: GPU-captured environment -> native ordered liquid records
// -> incident resolver -> foreground compositor -> complete XR acceptance.
// Static optics or an authored fractional lens shift supplies the independent
// SAME-pixel/bilinear old source. The opt-in rigid/clock/eye follow-up uses test-only
// binary64 forward optics; no downloaded inverse guide is oracle input.
// Diagnostic readbacks never become renderer input. This is not arbitrary
// motion, other-backend, physical-panel or practical performance acceptance.
template<class Render,class Consume>
std::shared_ptr<const CalibratedGameFrame> native_curved_reflection_owner_checks(
    Render render,Consume read,DisplayXrGameRenderer& renderer,bool srgb,unsigned quality=0,bool reconstruct=false,bool shifted=false,bool motion=false,bool camera_motion=false,bool disocclusion=false,bool source_validation=false,
    ReflectionSourceStageTimings* source_timings=nullptr) {
    check(quality<=3,"Invalid ordered-liquid MSAA diagnostic quality");
    check(!reconstruct || quality,"Ordered-liquid reconstruction requires genuine hardware samples");
    check(!shifted || (!quality && !reconstruct),"Fractional-lens owner qualification currently requires AA OFF");
    check(!motion || (!quality && !reconstruct && !shifted),"Moving owner qualification currently requires AA OFF and a fixed lens");
    check(!camera_motion || motion,"Eye-motion qualification requires the independent moving-owner optics");
    check(!disocclusion || camera_motion,"Disocclusion qualification requires real moving eyes and independent optics");
    check(!source_validation || (!quality && !reconstruct),
        "Source-frame owner qualification currently requires AA OFF; MSAA remains a separate gate");
    check(!native_source_lava_raw_input_only || (motion && !source_validation && !quality && !reconstruct
        && !shifted && !camera_motion && !disocclusion && srgb),
        "Raw-input capture requires the exact fixed-eye sRGB moving lava producer fixture");
    const auto enable=[&](bool enabled) {return source_validation
        ?renderer.enable_source_reflection_validation(enabled):renderer.enable_curved_reflection_validation(enabled);};
    const bool detailed=shifted || motion;
    const unsigned samples=quality?1U<<quality:1;
    const unsigned phases=detailed?3:2;
    constexpr unsigned probe_faces=24;
    const unsigned model_triangles=detailed?8+probe_faces*2:4;
    check(!enable(true),"Curved/source owner did not require explicit diagnostic opt-in");
    check(renderer.enable_reflection_diagnostics(true) && enable(true),
        "Could not stage the actual ordered-liquid owner");
    ReflectionSourceProofReceipts proof_receipts;
    const bool proof_diagnostics=source_validation && motion
        && (native_source_lava_motion_proof_only || SDL_getenv("STARFOX_TRACE_SOURCE_PROOF_RECEIPTS"));
    struct SourceObserver {
        ReflectionSourceStageTimings* timings;ReflectionSourceProofReceipts* proofs;
        static void observe(void* context,const ReflectionSourceStageEvent& event) {
            auto& state=*static_cast<SourceObserver*>(context);
            if(state.timings && state.timings->enabled())ReflectionSourceStageTimings::observe(state.timings,event);
            state.proofs->observe(event);
        }
    } source_observer{source_timings,&proof_receipts};
    struct RemoveSourceObserver {
        DisplayXrGameRenderer& renderer;
        ~RemoveSourceObserver(){renderer.set_source_stage_observer(nullptr);}
    } remove_observer{renderer};
    if(proof_diagnostics || (source_timings && source_timings->enabled()))check(source_validation
        && renderer.set_source_stage_observer(SourceObserver::observe,&source_observer),
        "Could not bind explicit source-stage diagnostics");
    struct Bank {shadows::GpuReflectionOutput output;std::vector<unsigned> words;};
    const auto download=[&](shadows::GpuReflectionOutput output) {
        check(output.device && output.buffer && output.reflection_history.storage_bytes,"Missing real owner ray bank");
        Bank result{output,{}};auto* device=static_cast<SDL_GPUDevice*>(output.device);
        const auto bytes=output.reflection_history.storage_bytes;
        SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=bytes;
        auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);check(transfer,SDL_GetError());
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
        const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,bytes};
        const SDL_GPUTransferBufferLocation to{transfer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
        const bool completed=SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);check(completed,SDL_GetError());
        const auto* words=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(device,transfer,false));check(words,SDL_GetError());
        result.words.assign(words,words+bytes/4);SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);
        return result;
    };
    const auto rgb=[&](unsigned word) {
        std::array<double,3> result{};
        for(unsigned c=0;c<3;++c) {double x=((word>>(c*8))&255)/255.;
            result[c]=srgb?(x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4)):x;}
        return result;
    };
    const auto download_admission=[&](const GpuReflectionSourceAdmission& mask) {
        check(mask.device && mask.buffer && mask.width && mask.height && mask.bytes==mask.width*mask.height*4,
            "Source diagnostics lost the exact accepted per-pixel lobe mask");
        // Describe a read-only test download; this never becomes ray input.
        shadows::GpuReflectionOutput described{};described.device=mask.device;described.buffer=mask.buffer;
        described.width=mask.width;described.height=mask.height;described.reflection_history.storage_bytes=mask.bytes;
        return download(described).words;
    };
    const auto pack=[&](std::array<double,3> value) {
        unsigned result=0xff000000U;
        for(unsigned c=0;c<3;++c) {double x=std::clamp(value[c],0.,1.);
            if(srgb)x=x<=.0031308?x*12.92:1.055*std::pow(x,1/2.4)-.055;
            result|=unsigned(std::floor(x*255+.5))<<(c*8);}
        return result;
    };
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
    const auto trace_direct=[&](const shadows::GpuReflectionOutput& current,const shadows::GpuReflectionOutput& previous,
        ReflectionLiquidDiagnosticInputs inputs,const starfox::vr::EyeCamera& camera) {
        // Separate diagnostic output. All inputs are the real borrowed GPU
        // handles/encoded CPU metadata, not downloaded/re-uploaded records.
        check(inputs.device==current.device && inputs.vertices && inputs.ownership && previous.buffer,
            "Owner refusal trace lost its actual borrowed inputs");
        auto* device=static_cast<SDL_GPUDevice*>(current.device);
        check(std::string_view(SDL_GetGPUDeviceDriver(device))=="direct3d12","Owner refusal trace requires D3D12");
        const unsigned count=current.width*current.height,lobes=current.reflection_history.model_lobes;
        const auto trace_bytes=std::uint64_t(count)*lobes*112;
        check(trace_bytes>0 && trace_bytes<=16*1024*1024,"Owner refusal trace exceeded its diagnostic bound");
        struct TraceResources {
            SDL_GPUDevice* device;SDL_GPUBuffer* output{};SDL_GPUComputePipeline* pipeline{};SDL_GPUSampler* sampler{};
            ~TraceResources() {
                if(output)SDL_ReleaseGPUBuffer(device,output);
                if(pipeline)SDL_ReleaseGPUComputePipeline(device,pipeline);
                if(sampler)SDL_ReleaseGPUSampler(device,sampler);
            }
        } resources{device};
        SDL_GPUComputePipelineCreateInfo pipeline{};pipeline.entrypoint="reflection_curved_trace_main";
        pipeline.format=SDL_GPU_SHADERFORMAT_DXIL;pipeline.num_samplers=pipeline.num_readwrite_storage_buffers=pipeline.num_uniform_buffers=1;
        pipeline.num_readonly_storage_buffers=3;pipeline.threadcount_x=pipeline.threadcount_y=8;pipeline.threadcount_z=1;
        pipeline.code=calibrated_curved_trace_1_dxil;pipeline.code_size=sizeof(calibrated_curved_trace_1_dxil);
        resources.pipeline=SDL_CreateGPUComputePipeline(device,&pipeline);check(resources.pipeline,SDL_GetError());
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        resources.sampler=SDL_CreateGPUSampler(device,&sampler);check(resources.sampler,SDL_GetError());
        const SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,unsigned(trace_bytes),0};
        resources.output=SDL_CreateGPUBuffer(device,&buffer);check(resources.output,SDL_GetError());
        std::array<unsigned,112> uniform{};
        const auto f=[&](unsigned at,double value){uniform[at]=std::bit_cast<unsigned>(float(value));};
        uniform[0]=uniform[2]=current.width;uniform[1]=uniform[3]=current.height;uniform[4]=lobes;uniform[5]=1U|(srgb?2U:0U);
        uniform[6]=inputs.previous_vertex_offset;uniform[7]=inputs.previous_vertex_offset+inputs.vertex_count*16;
        uniform[8]=uniform[20]=inputs.vertex_count/3;uniform[9]=uniform[7]+uniform[8]*4;
        f(10,.85);f(11,0); // This bounded trace only runs the sharp mirror cohort.
        for(unsigned c=0;c<4;++c)f(12+c,inputs.frame[20+c]);f(16,inputs.frame[26]);f(17,inputs.frame[27]);
        uniform[22]=current.reflection_history.identity_offset/count;uniform[23]=1;
        // Read-only actual encoded calibration, used ONLY by this diagnostic.
        // The independent oracle still starts from raw located views/geometry;
        // neither these transforms nor the GPU trace select its source.
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c) {
            f(24+r*4+c,inputs.current_cube[r*3+c]);f(36+r*4+c,inputs.previous_cube[r*3+c]);
        }
        for(unsigned c=0;c<32;++c)f(64+c,inputs.frame[c]);
        for(unsigned c=0;c<12;++c)f(96+c,inputs.current_liquid_rotation[c]);
        for(unsigned c=0;c<4;++c)f(108+c,inputs.current_projection[c]);
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
        const SDL_GPUStorageBufferReadWriteBinding target{resources.output,false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&target,1);check(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,resources.pipeline);
        const SDL_GPUTextureSamplerBinding owner{static_cast<SDL_GPUTexture*>(inputs.ownership),resources.sampler};
        SDL_BindGPUComputeSamplers(pass,0,&owner,1);
        SDL_GPUBuffer* buffers[]{static_cast<SDL_GPUBuffer*>(current.buffer),static_cast<SDL_GPUBuffer*>(previous.buffer),static_cast<SDL_GPUBuffer*>(inputs.vertices)};
        SDL_BindGPUComputeStorageBuffers(pass,0,buffers,3);SDL_PushGPUComputeUniformData(command,0,uniform.data(),sizeof(uniform));
        SDL_DispatchGPUCompute(pass,(current.width+7)/8,(current.height+7)/8,1);SDL_EndGPUComputePass(pass);
        check(SDL_SubmitGPUCommandBuffer(command),SDL_GetError());
        auto output=current;output.buffer=resources.output;output.reflection_history.storage_bytes=unsigned(trace_bytes);
        return download(output).words;
    };
#endif
    std::uint64_t changed=0,current_words=0,protected_pixels=0,canonical_words=0,protected_paths=0;
    std::uint64_t cut_current_words=0,cut_ownership_refusals=0;
    std::uint64_t reconstructed_bytes=0,protected_image_bytes=0,finished_reuse_bytes=0;
    std::uint64_t count_transitions=0,transition_current_words=0,retired_history_bytes=0;
    std::uint64_t refused_old_taps=0,protected_old_taps=0,recomposed_channels=0;
    std::uint64_t moving_geometry_words=0,advanced_liquid_clocks=0,moving_root_witnesses=0;
    std::uint64_t changed_eye_views=0,changed_ownership_pixels=0;
    std::uint64_t uncovered_primary_pixels=0,covered_primary_pixels=0;
    std::uint64_t independently_ambiguous_refusals=0,independent_bank_fallbacks=0;
    std::uint64_t independently_bounded_seeds=0;
    std::shared_ptr<CalibratedGameFrame> last;
    for(unsigned ground_material:{5U,9U})for(auto material:{Effect::mirror,Effect::marble}) {
        if((native_source_lava_motion_proof_only || native_source_lava_raw_input_only)
            && (ground_material!=9 || material!=Effect::mirror))continue;
        check(enable(false) && enable(true),
            "Curved owner could not cut/reseed between native cohorts");
        auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.03F,.06F,.09F,1};
        auto tick=std::make_shared<starfox::vr::GameSceneSnapshot>();tick->dots_mode=-1;
        for(unsigned key:{1U,2U}) {tick->transforms[key].generation=1;tick->transforms[key].shape=key+10;}
        if(detailed) {tick->transforms[3].generation=1;tick->transforms[3].shape=13;}
        if(detailed) {tick->transforms[4].generation=1;tick->transforms[4].shape=14;}
        if(detailed)for(unsigned key=5;key<5+probe_faces;++key) {
            tick->transforms[key].generation=1;tick->transforms[key].shape=key+10;
        }
        base->previous=base->current=tick;
        const auto add=[&](unsigned key,float x,float y,float z,float radius,std::array<float,4> colour,bool caster,bool after) {
            starfox::vr::DrawPacket packet;
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                starfox::vr::SceneVertex v{};v.position[0]=x+((corner==1 || corner==2)?radius:-radius);
                v.position[1]=y+(corner>=2?radius:-radius);v.position[2]=z;
                if(key==1)v.position[2]+=.7F*v.position[1]+.08F*v.position[0];
                if(key==3)v.position[2]-=.7F*(v.position[1]-y)+.08F*(v.position[0]-x);
                if(key==4)v.position[2]+=.35F*(v.position[1]-y)+.03F*(v.position[0]-x);
                const bool probe=key>=5 && key<5+probe_faces;
                if(probe)v.position[2]+=(.4F+.25F*((key-5)%4))*(v.position[1]-y)
                    +.02F*(int((key-5)%3)-1)*(v.position[0]-x);
                std::copy(colour.begin(),colour.end(),v.color);std::copy(colour.begin(),colour.end(),v.odd_color);
                if(detailed && (key==1 || key==3 || key==4 || probe)) {
                    // Native, phase-invariant grayscale artwork spans the
                    // real marble finish's integer luminance bands. Uniform
                    // grey can legitimately erase sub-band dielectric changes
                    // even when old incident lookup/recomposition is correct.
                    std::fill(std::begin(v.color),std::end(v.color),1.F);
                    std::fill(std::begin(v.odd_color),std::end(v.odd_color),1.F);
                    v.uv[0]=(v.position[0]-x+radius)*32/(2*radius);
                    v.uv[1]=(v.position[1]-y+radius)*32/(2*radius);
                    v.texture[1]=v.texture[2]=31;v.texture[3]=1;
                }
                packet.geometry.vertices.push_back(v);
            }
            if(detailed && (key==1 || key==3 || key==4 || (key>=5 && key<5+probe_faces)))for(unsigned ty=0;ty<32;++ty)for(unsigned tx=0;tx<32;++tx) {
                const unsigned light=30+tx*7;
                packet.geometry.texels.push_back(light|(light<<8)|(light<<16)|0xff000000U);
            }
            base->draws.push_back({std::move(packet),after?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
                starfox::vr::SceneBlend::opaque,!after,key,caster,after});
        };
        add(1,0,0,-2,1.1F,{.3F,.3F,.3F,1},true,false);
        add(2,.6F,.5F,-.55F,.24F,{.8F,.1F,.1F,1},true,false);
        // Keep the original downward model/liquid optical chain. A separate
        // upward model provides a visible direct-sky finishing witness without
        // raising dielectric Fresnel, removing roughness or weakening reuse.
        if(detailed)add(3,-.28F,.30F,-1.F,.24F,{.3F,.3F,.3F,1},true,false);
        // Broaden finite-receiver angle/depth coverage without replacing the
        // original receding face or waiving its folded/absorbed paths. A
        // closer, shallower face samples a different genuine liquid region.
        if(detailed)add(4,.45F,-.40F,-.50F,.18F,{.3F,.3F,.3F,1},true,false);
        if(detailed)for(unsigned i=0;i<probe_faces;++i)
            add(5+i,-.60F+.24F*(i%6),-.35F+.20F*(i/6),
                -.60F-.20F*(i%3),.065F,{.3F,.3F,.3F,1},true,false);
        add(0,-.1F,.03F,-.65F,.07F,{1,0,0,1},false,false);
        add(0,.16F,.16F,-.5F,.05F,{1,1,0,1},false,true);
        // Actual world artwork captured on the GPU, not the component's CPU
        // cube or a screenshot containing models/native text.
        const auto scenery_begin=base->draws.size();
        for(float z:{3.F,-12.F}) {
            add(0,0,0,z,16,{1,1,1,1},false,false);auto& draw=base->draws.back();
            draw.layer=CalibratedGameLayer::world;draw.reflection_environment=true;draw.depth_test=false;
            for(auto& v:draw.packet.geometry.vertices) {
                v.uv[0]=(v.position[0]+16)*2;v.uv[1]=(v.position[1]+16)*2;
                v.texture[1]=v.texture[2]=63;v.texture[3]=1;
            }
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
                // Brightness contrast makes wrong fractional old taps and
                // final marble's integer luminance bands distinguishable.
                // This is authored scenery uploaded/captured by the real
                // owner, never a diagnostic inverse or RGB output injection.
                const unsigned light=40+(x*19+y*23)%128;
                draw.packet.geometry.texels.push_back(detailed
                    ?light|((light+7)<<8)|((light+13)<<16)|0xff000000U
                    :(45+x*2)|((40+y*2)<<8)|((70+x+y)<<16)|0xff000000U);
            }
        }
        // The actual game emits distant BG artwork BEFORE the BG2 floor and
        // models, without writing depth. Rendering the sky last overwrites the
        // floor's raster ownership wherever no model wrote depth. Water can
        // still replace the model there; opaque lava correctly cannot. Preserve
        // the real source order, rather than weakening that foreground guard.
        std::rotate(base->draws.begin(),base->draws.begin()+scenery_begin,base->draws.end());
        starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
        // An opaque lava receiver needs the source BG2 coverage, unlike water
        // which can replace nearer model RGB. Use actual opaque source tiles,
        // as in the independent native coupling fixture, not a blank PPU whose
        // absent raster ground correctly refuses every lava-primary footprint.
        ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
        for(unsigned y=0;y<8;++y)ppu.vram[32+y*2]=255;
        for(unsigned tile=0;tile<2048;++tile)ppu.vram[0x2000+tile*2]=1;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,false,false,232);
        starfox::vr::place_landscape_ground(floor,.01F-180.F/256,true);
        CalibratedGroundGradient gradient{{.2F,.3F,.4F},{.2F,.3F,.4F},112,111};
        gradient.material=ground_material;gradient.origin={29,17};gradient.seconds=1.3F;gradient.brightness=.72F;
        check(apply_calibrated_ground(floor,gradient),"Owner fixture lost actual liquid packet metadata");
        base->draws.insert(base->draws.begin()+2,{std::move(floor),CalibratedGameLayer::world,
            starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        base->settings.enhanced_ground=true;base->settings.ray_tracing=base->settings.reflections=2;
        base->settings.material=unsigned(material);
        base->settings.aa_type=quality?6:0;base->settings.aa_quality=quality;
        native_mock.head_x=native_mock.head_yaw=native_mock.sample_x=native_mock.sample_y=0;
        const auto source=[&](unsigned phase) {
            auto frame=std::make_shared<CalibratedGameFrame>(*base);
            auto now=std::make_shared<starfox::vr::GameSceneSnapshot>(*tick);now->revision=phase;
            frame->previous=frame->current=now;frame->presentation_seconds=20+phase/120.;
            if(phase)for(auto& draw:frame->draws)if(draw.reflection_environment)
                // Keep the static cohort unchanged. The fractional out/return
                // cohort changes authored incident light enough to survive
                // the real water Fresnel/rough-lobe/final 8-bit compositor,
                // rather than treating sub-quantum output as finishing proof.
                for(auto& word:draw.packet.geometry.texels)word+=phase*(detailed
                    ?24+(24U<<8)+(24U<<16):3-(2U<<8)+(1U<<16));
            if(shifted) {
                // Translate the REAL located XR FOV in tangent space. These
                // sixteenth-pixel offsets give current (+.25,-.375) raster
                // principal-point displacement, then return to the centre.
                // View, liquid clock, geometry and rough lobe remain fixed.
                native_mock.sample_x=phase==1?-4.F:0.F;
                native_mock.sample_y=phase==1?6.F:0.F;
            }
            if(camera_motion) {
                // Actual located XR translation AND quaternion yaw, out and
                // back. Projection and topology remain fixed; geometry and
                // wave clock continue the independent rigid/clock fixture.
                native_mock.head_x=phase==1?(disocclusion?.22F:.075F):0.F;
                native_mock.head_yaw=phase==1?(disocclusion?.012F:.0005F):0.F;
            }
            if(motion)for(auto& draw:frame->draws) {
                if(draw.source_key==calibrated_ground_source_key) {
                    // Active authored clock metadata, not a CPU-rendered wave.
                    draw.packet.geometry.texels[calibrated_ground_offset+11]=std::bit_cast<unsigned>(1.3F+phase*.015F);
                } else if(draw.ray_caster) {
                    // Real rigid geometry changes, then returns; the liquid
                    // clock keeps advancing. Generation/topology stay stable.
                    const float angle=phase==1?(disocclusion?.015F:.002F):0.F;
                    draw.packet.model[0]=draw.packet.model[10]=std::cos(angle);
                    draw.packet.model[2]=-std::sin(angle);draw.packet.model[8]=std::sin(angle);
                    draw.packet.model[12]=phase==1?(disocclusion?.025F:.0015F):0.F;
                    draw.packet.model[14]=phase==1?(disocclusion?.012F:.0008F):0.F;
                } else if(disocclusion && !draw.after_rays && draw.layer==CalibratedGameLayer::model) {
                    // Sweep actual opaque, non-reflective foreground ink across
                    // the receiver, then return. The real raster owner must
                    // expose and cover primary samples; do not inject a fake
                    // ownership image or cut history to avoid those transitions.
                    draw.packet.model[12]=phase==1?.22F:0.F;
                }
            }
            return frame;
        };
        using Images=decltype(render(base));
        std::array<Images,3> center_ink,phase_images;
        std::array<std::array<Images,2>,8> references;
        if(reconstruct) {
            // Independent located XR FOVs, in sixteenths of a pixel. Do not
            // call the production sample-camera/transport/resolve helpers or
            // use their diagnostic finished sample textures as references.
            constexpr int positions[14][2]{{4,4},{-4,-4},{-2,-6},{6,-2},{-6,2},{2,6},
                {1,-3},{-1,3},{5,1},{-3,-5},{-5,5},{-7,-1},{3,7},{7,-7}};
            for(unsigned phase=0;phase<2;++phase) {
                auto ink=source(phase);ink->settings.ray_tracing=ink->settings.reflections=ink->settings.aa_quality=0;
                center_ink[phase]=render(ink);
            }
            for(unsigned sample=0;sample<samples;++sample) {
                native_mock.sample_x=positions[samples-2+sample][0];
                native_mock.sample_y=positions[samples-2+sample][1];
                for(unsigned phase=0;phase<2;++phase) {
                    auto single=source(phase);single->settings.aa_type=single->settings.aa_quality=0;
                    single->settings.history_epoch=40000+ground_material*10000+unsigned(material)*100+quality*10+sample;
                    // The opaque yellow native overlay is drawn on the centre
                    // grid AFTER reconstruction. Remove it from the independent
                    // sample images, then restore its independently rendered
                    // centre ink below. No shifted UI-edge pixels are waived.
                    std::erase_if(single->draws,[](const auto& draw){return draw.after_rays;});
                    references[sample][phase]=render(single);
                    check(render(single)==references[sample][phase],"Held independent ordered-liquid XR sample changed its finished image");
                }
            }
            native_mock.sample_x=native_mock.sample_y=0;
        }
        std::array<std::array<Bank,2>,8> accepted{};unsigned cohort_changed=0,liquid_pixels=0,model_pixels=0;
        unsigned source_unrounded_compositions=0;
        unsigned source_admitted_lobes=0,source_composed_pixels=0,source_independent_footprints=0;
        struct AdmissionBank {GpuReflectionSourceAdmission output;std::vector<unsigned> words;};
        std::array<std::array<AdmissionBank,2>,8> accepted_admissions{};
        struct Coverage {
            unsigned changed{},liquid{},model{},surface{},curved_model{},direct_model{};
            unsigned direct_wave_changes{};
            unsigned current_direct{},matched_direct{},different_direct_source{},different_clamped_direct{};
            unsigned current_curved{},matched_curved{},different_curved_source{},different_clamped_curved{};
            std::array<unsigned,5> curved_kinds{};
            std::array<unsigned,24> curved_refusals{};
        };
        std::array<std::array<Coverage,2>,8> coverage{};
        std::array<std::array<std::array<Coverage,2>,8>,3> phase_reuse{};
        std::array<std::optional<starfox::vr::EyeCamera>,2> previous_cameras{};
        for(unsigned phase=0;phase<phases;++phase) {
            auto frame=source(phase);
            std::vector<unsigned> diagnostic_proofs;
            proof_receipts.selected=proof_diagnostics && ground_material==9 && material==Effect::mirror && phase==1;
            if(source_validation)std::cerr<<"Source owner phase begin ground="<<ground_material
                <<" model="<<unsigned(material)<<" samples="<<samples<<" phase="<<phase
                <<" srgb="<<srgb<<" extent="<<mock.image_width<<'x'<<mock.image_height<<std::endl;
            const auto images=render(frame);
            if(proof_receipts.selected) {
                auto receipts=proof_receipts.retired(!renderer.frame_pending());
                const unsigned records=mock.image_width*mock.image_height,proof_begin=records*12;
                std::array<std::unordered_map<unsigned,unsigned>,5> reasons;
                unsigned curved=0;
                for(unsigned r=0;r<records;++r) {
                    const unsigned query=r*12,control=receipts[query+5];
                    if(receipts[query]>=model_triangles || !((control>>4)&15U))continue;
                    ++curved;const unsigned proof=proof_begin+r*144;
                    ++reasons[0][(receipts[proof]<<24)|(receipts[proof+1]<<16)|receipts[proof+3]];
                    ++reasons[1][(receipts[proof+48]<<24)|receipts[proof+50]];
                    ++reasons[2][(receipts[proof+64]<<24)|receipts[proof+65]];
                    ++reasons[3][(receipts[proof+80]<<24)|receipts[proof+81]];
                    ++reasons[4][(receipts[proof+96]<<24)|receipts[proof+97]];
                }
                std::cout<<"Source curved proof receipts srgb="<<srgb<<" phase="<<phase
                    <<" eye=0 complete-records="<<records<<" curved-queries="<<curved<<'\n';
                for(unsigned stage=0;stage<reasons.size();++stage) {
                    std::vector<std::pair<unsigned,unsigned>> sorted(reasons[stage].begin(),reasons[stage].end());
                    std::sort(sorted.begin(),sorted.end());
                    std::cout<<"Source proof stage="<<stage<<" valid/class/reason:count=";
                    for(const auto& [key,count]:sorted)std::cout<<(key>>24)<<'/'<<((key>>16)&255)<<'/'<<(key&65535)<<':'<<count<<',';
                    std::cout<<std::endl;
                }
                diagnostic_proofs=std::move(receipts);
            }
            if(source_validation)std::cerr<<"Source owner phase GPU/readback complete ground="<<ground_material
                <<" model="<<unsigned(material)<<" samples="<<samples<<" phase="<<phase<<std::endl;
            const auto old_cameras=previous_cameras;
            phase_images[phase]=images;
            check(images[0]!=images[1],"Ordered liquid owner collapsed native eye projections");
            if(detailed) for(unsigned eye=0;eye<2;++eye) {
                const auto camera=renderer.source_raster_diagnostic_camera(eye);
                check(bool(camera),"Fractional owner did not record an actual located raster camera");
                if(phase) {
                    check((camera_motion?camera->view!=previous_cameras[eye]->view:camera->view==previous_cameras[eye]->view)
                        && (motion?camera->projection==previous_cameras[eye]->projection:camera->projection!=previous_cameras[eye]->projection),
                        "Owner failed to change the selected actual located eye/lens calibration");
                    if(camera_motion) {
                        check(camera->view[0]!=previous_cameras[eye]->view[0]
                            && camera->view[12]!=previous_cameras[eye]->view[12],
                            "Eye-motion owner missed real rotation or translation");
                        ++changed_eye_views;
                    }
                }
                previous_cameras[eye]=camera;
            }
            if(reconstruct)for(unsigned eye=0;eye<2;++eye)for(unsigned at=0;at<images[eye].size();at+=4) {
                const auto& ink=center_ink[phase][eye];
                const bool protected_ink=ink[at]==255 && ink[at+2]==0 && (ink[at+1]==0 || ink[at+1]==255);
                std::array<double,3> linear{};double alpha=0;
                for(unsigned sample=0;sample<samples;++sample) {
                    const auto& image=references[sample][phase][eye];
                    unsigned word{};std::memcpy(&word,image.data()+at,4);
                    const auto value=rgb(word);for(unsigned c=0;c<3;++c)linear[c]+=value[c]/samples;
                    alpha+=double(image[at+3])/samples;
                }
                const auto color=pack(linear);
                for(unsigned c=0;c<4;++c) {
                    const unsigned expected=protected_ink?ink[at+c]:c==3?unsigned(std::lround(alpha)):(color>>(c*8))&255;
                    const unsigned limit=protected_ink || c==3?0:2;
                    if(std::abs(int(images[eye][at+c])-int(expected))>int(limit))throw std::runtime_error(
                        "Ordered-liquid independent XR reconstruction mismatch ground="+std::to_string(ground_material)
                        +" material="+std::to_string(unsigned(material))+" samples="+std::to_string(samples)
                        +" phase="+std::to_string(phase)+" eye="+std::to_string(eye)+" xy="
                        +std::to_string((at/4)%mock.image_width)+","+std::to_string((at/4)/mock.image_width)
                        +" channel="+std::to_string(c)+" actual="+std::to_string(images[eye][at+c])
                        +" expected="+std::to_string(expected)+" centre-ink="+std::to_string(protected_ink));
                    ++reconstructed_bytes;protected_image_bytes+=protected_ink;
                }
            }
            std::array<std::array<Bank,2>,8> candidate{};
            std::array<std::array<AdmissionBank,2>,8> candidate_admissions{};
            std::unordered_set<void*> retained_buffers,current_buffers;
            for(unsigned sample=0;sample<samples;++sample)for(unsigned eye=0;eye<2;++eye) {
                auto raw=download(renderer.current_reflection_sample(eye,sample));
                candidate[sample][eye]=download(renderer.accepted_reflection_sample(eye,sample));
                const auto& out=candidate[sample][eye];const auto& h=out.output.reflection_history;
                auto& witness=coverage[sample][eye];
                const unsigned count=out.output.width*out.output.height,lobes=h.model_lobes;
                if(!diagnostic_proofs.empty() && !sample && !eye) {
                    // Correlate the complete GPU receipts with the ACTUAL old
                    // bank, for diagnosis only. These roots/targets/taps never
                    // seed the independent forward-optics acceptance oracle
                    // or become renderer input. Bound verbose printing only.
                    check(phase && lobes==1 && diagnostic_proofs.size()==count*156,
                        "Proof correlation lost its complete previous-bank layout");
                    const auto& old=accepted[sample][eye];unsigned unique{},printed{};
                    for(unsigned p=0;p<count;++p) {
                        const unsigned q=p*12,proof=count*12+p*144;
                        if(diagnostic_proofs[q]>=model_triangles || !((diagnostic_proofs[q+5]>>4)&15U)
                            || diagnostic_proofs[proof]!=1 || diagnostic_proofs[proof+1]!=1)continue;
                        ++unique;if(printed++>=32)continue;
                        std::array<float,4> root{};
                        for(unsigned n=0;n<4;++n)root[n]=std::bit_cast<float>(diagnostic_proofs[proof+36+n]);
                        std::cout<<"Source unique proof diagnostic pixel="<<p%out.output.width<<','<<p/out.output.width
                            <<" primary="<<diagnostic_proofs[q]<<" control="<<diagnostic_proofs[q+5]
                            <<" terminal="<<diagnostic_proofs[q+6]<<" target=";
                        for(unsigned n=0;n<3;++n)std::cout<<std::bit_cast<float>(diagnostic_proofs[q+7+n])<<',';
                        std::cout<<" enclosure=";for(const auto value:root)std::cout<<value<<',';
                        std::cout<<" witness="<<diagnostic_proofs[proof+48]<<'/'<<diagnostic_proofs[proof+50]<<'\n';
                        if(!std::all_of(root.begin(),root.end(),[](float v){return std::isfinite(v);})
                            || root[0]<.5F || root[1]<.5F || root[2]>out.output.width-.5F
                            || root[3]>out.output.height-.5F || root[0]>root[2] || root[1]>root[3])continue;
                        const int x0=int(std::floor(root[0]-.5F))-1,y0=int(std::floor(root[1]-.5F))-1,
                            x1=int(std::floor(root[2]-.5F))+2,y1=int(std::floor(root[3]-.5F))+2;
                        if(x1-x0>5 || y1-y0>5)continue;
                        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
                            if(x<0 || y<0 || x>=int(out.output.width) || y>=int(out.output.height))continue;
                            const unsigned np=unsigned(y)*out.output.width+unsigned(x),at=h.incoming_offset/4+np*16;
                            std::cout<<"Source old tap diagnostic xy="<<x<<','<<y
                                <<" primary="<<old.words[h.identity_offset/4+np]<<" path=";
                            for(unsigned n=0;n<6;++n)std::cout<<old.words[at+n]<<',';
                            std::cout<<" feature=";for(unsigned n=0;n<3;++n)std::cout<<std::bit_cast<float>(old.words[at+6+n])<<',';
                            std::cout<<" depth="<<std::bit_cast<float>(old.words[h.witness_offset/4+np])<<'\n';
                        }
                    }
                    std::cout<<"Source unique proof diagnostic total="<<unique<<" printed="<<std::min(unique,32U)
                        <<"; observation only, never an optical/colour acceptance oracle"<<std::endl;
                }
                auto& admission=candidate_admissions[sample][eye];
                admission.output=renderer.accepted_reflection_source_admission(eye,sample);
                if(source_validation) {
                    check(admission.output.device==out.output.device && admission.output.width==out.output.width
                        && admission.output.height==out.output.height && admission.output.buffer!=out.output.buffer
                        && admission.output.buffer!=raw.output.buffer,"Source mask aliases colour or has a foreign extent/device");
                    admission.words=download_admission(admission.output);
                    if(phase)check(download_admission(accepted_admissions[sample][eye].output)==accepted_admissions[sample][eye].words,
                        "Current presentation overwrote the previous accepted lobe mask");
                } else check(!admission.output.buffer,"Ordinary reflection diagnostics allocated a source lobe mask");
                std::optional<reflected_curved_owner_oracle::Inputs> motion_inputs;
                if(motion && phase) {
                    const auto inputs=renderer.reflection_liquid_inputs(eye,sample);
                    check(inputs.device==raw.output.device && inputs.vertices && inputs.vertex_count==model_triangles*3,
                        "Moving owner lost actual current/accepted native geometry");
                    const auto camera=renderer.source_raster_diagnostic_camera(eye);
                    check(bool(camera),"Moving owner lost its encoded optical camera");
                    auto optical=raw.output;optical.buffer=inputs.vertices;
                    optical.reflection_history.storage_bytes=inputs.previous_vertex_offset+inputs.vertex_count*16+model_triangles*4;
                    const auto words=download(optical).words;
                    motion_inputs=reflected_curved_owner_oracle::decode(words,inputs.previous_vertex_offset,inputs.vertex_count,
                        inputs.frame,{out.output.width*camera->projection[0]/2,out.output.height*camera->projection[5]/2,
                            out.output.width*(1.-camera->projection[8])/2,out.output.height*(1.+camera->projection[9])/2});
                    motion_inputs->current_liquid_transform=inputs.current_liquid_rotation;
                    if(camera_motion)reflected_curved_owner_oracle::eye_views(*motion_inputs,camera->view,old_cameras[eye]->view);
                    unsigned differences=0;
                    for(unsigned i=0;i<inputs.vertex_count*4;++i)differences+=words[i]!=words[inputs.previous_vertex_offset/4+i];
                    check(differences>model_triangles,"Moving owner did not actually change the native finite geometry");
                    check(inputs.frame[28]==1.3F+(phase-1)*.015F && inputs.frame[28]!=1.3F+phase*.015F,
                        "Moving owner lost the accepted OLD liquid clock or failed to advance it");
                    for(unsigned i=0;i<model_triangles;++i)check(motion_inputs->mapping[i]==i,
                        "Moving owner fixture lost explicit stable primitive correspondence");
                    moving_geometry_words+=differences;++advanced_liquid_clocks;
                }
                if(phase)check(download(accepted[sample][eye].output).words==accepted[sample][eye].words,
                    "Current presentation overwrote the prior accepted ordered-liquid bank");
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
                std::vector<unsigned> refusal_trace;unsigned traced_samples=0;
                if(!native_source_lava_raw_input_only && detailed && material==Effect::mirror
                    && ((phase==1 && eye==0) || (motion && phase))) {
                    const auto inputs=renderer.reflection_liquid_inputs(eye,sample);
                    refusal_trace=trace_direct(raw.output,accepted[sample][eye].output,inputs,*renderer.source_raster_diagnostic_camera(eye));
                }
#endif
                check(h.curved_paths && h.curved_receivers && h.model_paths && lobes==(material==Effect::mirror?1U:8U),
                    "Game owner did not request complete native ordered liquid records");
                check(out.output.buffer!=raw.output.buffer,"Accepted ordered-liquid bank aliases CURRENT native output");
                check(retained_buffers.insert(out.output.buffer).second && current_buffers.insert(raw.output.buffer).second,
                    "Ordered-liquid eyes/hardware samples alias another sample's bank");
                check(std::equal(raw.words.begin()+count,raw.words.begin()+h.identity_offset/4,out.words.begin()+count),
                    "Owner transported canonical CURRENT surface/hidden-world words");
                canonical_words+=h.identity_offset/4-count;
                if(quality && ground_material==5) {
                    const auto expected=shadows::native_water_layers(out.output.width,out.output.height,false);
                    check(expected && out.output.water_layers==*expected && raw.output.water_layers==*expected
                        && h.identity_offset==expected->storage_bytes,
                        "Sampled ordered water lost its genuine CURRENT surface prefix");
                } else check(out.output.water_layers.storage_bytes==0,
                    "Unrequested ordered-liquid canonical layers changed the native ABI");
                for(unsigned p=0;p<count;++p) {
                    const auto id=out.words[h.identity_offset/4+p];
                    const unsigned admitted=source_validation?admission.words[p]:0;
                    check(!source_validation || (admitted<(1U<<lobes) && (!admitted || (phase && id!=UINT32_MAX))),
                        "Cold/protected source mask invented lobe admission");
                    if(source_validation && !admitted)check(out.words[p]==raw.words[p],
                        "Unpublished source pixel changed fresh CURRENT radiance");
                    if(camera_motion && phase) {
                        const auto old_id=accepted[sample][eye].words[h.identity_offset/4+p];
                        changed_ownership_pixels+=id!=old_id;
                        uncovered_primary_pixels+=id!=UINT32_MAX && old_id==UINT32_MAX;
                        covered_primary_pixels+=id==UINT32_MAX && old_id!=UINT32_MAX;
                    }
                    // Raster ownership intentionally invalidates the cached
                    // primary behind protected ink; native RT still sees the
                    // underlying surface. Every eligible identity stays exact.
                    check(id==UINT32_MAX || id==raw.words[h.identity_offset/4+p],"Owner changed an eligible CURRENT primary identity");
                    model_pixels+=id<model_triangles;liquid_pixels+=id==0xfffffffdU;
                    witness.model+=id<model_triangles;witness.liquid+=id==0xfffffffdU;
                    if(quality && ground_material==5 && id==0xfffffffdU) {
                        const auto at=out.output.water_layers.surface_offset/4+p*4;
                        double length=0;
                        for(unsigned c=0;c<3;++c) {const auto v=std::bit_cast<float>(out.words[at+c]);length+=double(v)*v;}
                        const auto depth=std::bit_cast<float>(out.words[at+3]);
                        check(std::isfinite(length) && length>.5 && length<1.5 && std::isfinite(depth) && depth>0,
                            "Sampled water prefix has no physical CURRENT normal/depth");
                        ++witness.surface;
                    }
                    if(id==UINT32_MAX) {check(raw.words[p]==out.words[p],"Owner changed protected/unowned CURRENT radiance");++protected_pixels;}
                    for(auto plane:{h.witness_offset,h.weight_offset,h.base_offset})if(plane) {
                        const unsigned stride=plane==h.witness_offset?1:4;
                        check(std::equal(raw.words.begin()+plane/4+p*stride,raw.words.begin()+plane/4+(p+1)*stride,
                            out.words.begin()+plane/4+p*stride),"Owner transported CURRENT primary depth/light");
                    }
                    std::array<double,3> composed_sum{};
                    std::array<double,3> source_composed_sum{};
                    for(unsigned l=0;l<lobes;++l) {
                        const unsigned at=h.incoming_offset/4+(p*lobes+l)*16;
                        const unsigned control=raw.words[at+4];
                        const bool direct_model=id<model_triangles && (control&7U)==0 && (control>>8)==2;
                        const bool curved_model_path=id<model_triangles && ((control>>4)&15U)!=0;
                        auto& phase_witness=phase_reuse[phase][sample][eye];
                        if(motion && phase && id==0xfffffffdU && (control&7U)==0 && (control>>8)==2) {
                            const auto& old=accepted[sample][eye];
                            phase_witness.direct_wave_changes+=old.words[h.identity_offset/4+p]==id
                                && std::equal(raw.words.begin()+at,raw.words.begin()+at+6,old.words.begin()+at)
                                && !std::equal(raw.words.begin()+at+6,raw.words.begin()+at+9,old.words.begin()+at+6);
                        }
                        phase_witness.current_direct+=direct_model;
                        if(curved_model_path) {
                            ++phase_witness.current_curved;
                            if((control>>8)<phase_witness.curved_kinds.size())++phase_witness.curved_kinds[control>>8];
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
                            if(!refusal_trace.empty()) {
                                const unsigned reason=refusal_trace[(p*lobes+l)*28+8];
                                if(reason<phase_witness.curved_refusals.size())++phase_witness.curved_refusals[reason];
                            }
#endif
                        }
                        protected_paths+=id==UINT32_MAX && raw.words[at]!=UINT32_MAX;
                        for(unsigned n=0;n<16;++n)if(n!=9) {
                            if(raw.words[at+n]!=out.words[at+n])throw std::runtime_error(
                                "Owner CURRENT ordered-word mismatch ground="+std::to_string(ground_material)
                                +" material="+std::to_string(unsigned(material))+" phase="+std::to_string(phase)
                                +" sample="+std::to_string(sample)+" eye="+std::to_string(eye)
                                +" xy="+std::to_string(p%out.output.width)+","+std::to_string(p/out.output.width)
                                +" lobe="+std::to_string(l)+" field="+std::to_string(n)+" id="+std::to_string(id)
                                +" raw="+std::to_string(raw.words[at+n])+" accepted="+std::to_string(out.words[at+n])
                                +" stride="+std::to_string(h.storage_bytes/count)+" prefix="+std::to_string(h.identity_offset/count));
                            ++current_words;
                        }
                        if(detailed && id!=UINT32_MAX) {
                            const auto incident=rgb(out.words[at+9]);
                            std::array<double,3> transported{};
                            for(unsigned c=0;c<3;++c) transported[c]=incident[c]
                                *std::bit_cast<float>(raw.words[at+10+c])+std::bit_cast<float>(raw.words[at+13+c]);
                            // Native transport quantizes EACH lobe before
                            // quadrature. Never average old primary RGB or
                            // carry its per-hop/current material response.
                            const auto quantized=rgb(pack(transported));
                            for(unsigned c=0;c<3;++c)composed_sum[c]+=quantized[c]/lobes;
                        }
                        std::array<double,3> prior{};bool old_valid=phase!=0 && !motion,old_ink=false;
                        double independently_solved_x=0,independently_solved_y=0;
                        const bool liquid_ambiguity_witness=disocclusion && ground_material==5 && material==Effect::mirror
                            && phase==1 && eye==1 && p==58*out.output.width+26 && l==0;
                        const bool model_return_ambiguity_witness=disocclusion && ground_material==5 && material==Effect::mirror
                            && phase==2 && eye==0 && p==25*out.output.width+125 && l==0;
                        const bool model_out_ambiguity_witness=disocclusion && ground_material==5 && material==Effect::mirror
                            && phase==1 && eye==1 && p==32*out.output.width+84 && l==0;
                        const bool ambiguity_witness=liquid_ambiguity_witness || model_return_ambiguity_witness || model_out_ambiguity_witness;
                        const auto emit_optical_replay=[&] {
                            if(!motion_inputs || !motion_inputs->current_liquid_transform)return;
                            // Complete bounded raw optics and old visibility only.
                            // Separate inverse/colour diagnostics are outside this block.
                            const auto& in=*motion_inputs;const auto& f=in.liquid;
                            const auto liquid_abi=renderer.reflection_liquid_inputs(eye,sample).frame;
                            check(count<=65536,"Synthetic owner replay exceeds its bounded old visibility budget");
                            std::cout<<std::setprecision(17)<<"BEGIN_CURVED_OWNER_SOURCE_V4\n"
                                <<out.output.width<<' '<<out.output.height<<' '<<id<<' '<<l<<' '
                                <<(material==Effect::mirror?0:double(.55F))<<' '<<p%out.output.width<<' '<<p/out.output.width<<'\n';
                            const auto vector=[&](const auto& values){for(auto value:values)std::cout<<value<<' ';std::cout<<'\n';};
                            vector(in.current_projection);vector(in.current_eye_view);vector(in.previous_eye_view);
                            vector(f.point);vector(f.normal);vector(f.rotation);vector(f.offset);vector(f.projection);
                            std::cout<<f.width<<' '<<f.height<<' '<<f.material<<' '<<liquid_abi[26]<<' '<<liquid_abi[27]<<' '<<f.time<<'\n';
                            vector(*in.current_liquid_transform);
                            std::cout<<std::bit_cast<float>(raw.words[h.witness_offset/4+p])<<'\n'<<in.current.size()<<'\n';
                            for(unsigned n=0;n<in.current.size();++n) {
                                vector(in.current[n].a);vector(in.current[n].b);vector(in.current[n].c);
                                vector(in.previous[n].a);vector(in.previous[n].b);vector(in.previous[n].c);
                                std::cout<<in.mapping[n]<<'\n';
                            }
                            vector(std::span<const unsigned,16>(raw.words.data()+at,16));
                            const auto& old=accepted[sample][eye];std::cout<<count<<'\n';
                            for(unsigned np=0;np<count;++np) {
                                const unsigned na=h.incoming_offset/4+(np*lobes+l)*16;
                                std::cout<<np%out.output.width<<' '<<np/out.output.width<<' '<<old.words[h.identity_offset/4+np]<<' '
                                    <<std::bit_cast<float>(old.words[h.witness_offset/4+np])<<' ';
                                vector(std::span<const unsigned,9>(old.words.data()+na,9));
                            }
                            std::cout<<"END_CURVED_OWNER_SOURCE_V4\n"<<std::flush;
                        };
                        if(native_source_lava_raw_input_only && phase==1 && sample==0 && eye==0
                            && p==29*out.output.width+107 && l==0) {
                            // Predetermined CURRENT raster coordinate from the
                            // recorded refusal, not a downloaded source root.
                            // Export only the already bounded raw geometry,
                            // calibration/path/depth and full old visibility.
                            check(out.output.width==128 && out.output.height==72 && lobes==1
                                && id==40 && control==546 && motion_inputs
                                && motion_inputs->current_liquid_transform,
                                "Raw lava capture no longer matches the observed producer-input case");
                            emit_optical_replay();
                            std::cout<<"Raw lava input capture: eye0/sample0/phase1 CURRENT pixel107,29; "
                                <<"all "<<count<<" old visibility inputs retained. No GPU root, guide, source choice "
                                <<"or old incident RGB exported. Diagnostic only, no qualification claim.\n";
                            return frame;
                        }
                        // A same-byte incident can still be an admitted old
                        // source lookup before quantization. Qualify EVERY
                        // admitted footprint independently, not only changes
                        // visible in the rounded diagnostic incident word.
                        if(phase && id!=UINT32_MAX && (!motion || raw.words[at+9]!=out.words[at+9]
                            || (source_validation && (admitted&(1U<<l))) || ambiguity_witness)) {
                            // Authored lens displacement, NOT the consumer's
                            // inverse feature/flow/guide or downloaded camera.
                            // Phase two must use the accepted SHIFTED view,
                            // proving the owner advanced its old calibration.
                            const double dx=shifted?(phase==1?.25:-.25):0;
                            const double dy=shifted?(phase==1?-.375:.375):0;
                            const unsigned x=p%out.output.width,y=p/out.output.width;
                            double sx=x-dx,sy=y-dy,old_depth=0;
                            if(motion) {
                                std::optional<reflected_curved_owner_oracle::Solution> attempted_root,attempted_seed;
                                const auto qualified_root=[&](const auto& root) {
                                        if(root.bounded_seed)attempted_seed=root;else attempted_root=root;
                                        if(root.x<0 || root.y<0 || root.x>out.output.width-1 || root.y>out.output.height-1)return false;
                                        const auto& old=accepted[sample][eye];double reciprocal=0;
                                        for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
                                            const double fx=root.x-std::floor(root.x),fy=root.y-std::floor(root.y);
                                            const double share=(dx?fx:1-fx)*(dy?fy:1-fy);if(!share)continue;
                                            const unsigned nx=std::min(unsigned(std::floor(root.x))+dx,out.output.width-1),
                                                ny=std::min(unsigned(std::floor(root.y))+dy,out.output.height-1),np=ny*out.output.width+nx;
                                            const unsigned na=h.incoming_offset/4+(np*lobes+l)*16;
                                            const double depth=std::bit_cast<float>(old.words[h.witness_offset/4+np]);
                                            if(old.words[h.identity_offset/4+np]!=id || !std::isfinite(depth) || depth<=0
                                                || !std::equal(raw.words.begin()+at,raw.words.begin()+at+6,old.words.begin()+na))return false;
                                            reciprocal+=share/depth;
                                        }
                                        if(!std::isfinite(reciprocal) || reciprocal<=0
                                            || std::abs(1/reciprocal-root.depth)>std::max(.01,root.depth*.005))return false;
                                        return reflected_curved_owner_oracle::source_guard(*motion_inputs,id,
                                            std::span<const unsigned,16>(raw.words.data()+at,16),
                                            material==Effect::mirror?0:double(.55F),l,root,[&](unsigned nx,unsigned ny)
                                                ->std::optional<reflected_curved_owner_oracle::SourceTap> {
                                                if(nx>=out.output.width || ny>=out.output.height)return {};
                                                const unsigned np=ny*out.output.width+nx,na=h.incoming_offset/4+(np*lobes+l)*16;
                                                reflected_curved_owner_oracle::SourceTap tap;
                                                tap.identity=old.words[h.identity_offset/4+np];
                                                tap.depth=std::bit_cast<float>(old.words[h.witness_offset/4+np]);
                                                std::copy_n(old.words.begin()+na,9,tap.path.begin());return tap;
                                            })==reflected_curved_owner_oracle::SourceGuard::accepted;
                                    };
                                auto solution=reflected_curved_owner_oracle::previous_source(*motion_inputs,id,
                                    std::span<const unsigned,16>(raw.words.data()+at,16),
                                    material==Effect::mirror?0:double(.55F),l,x,y,qualified_root,
                                    std::bit_cast<float>(raw.words[h.witness_offset/4+p]));
                                if(!solution.valid && !solution.ambiguous) {
                                    ++independent_bank_fallbacks;
                                    const auto& old=accepted[sample][eye];
                                    const auto starts=reflected_curved_owner_oracle::old_visibility_initializers(*motion_inputs,id,
                                        std::span<const unsigned,16>(raw.words.data()+at,16),[&](unsigned nx,unsigned ny)
                                            ->std::optional<reflected_curved_owner_oracle::SourceTap> {
                                            const unsigned np=ny*out.output.width+nx,na=h.incoming_offset/4+(np*lobes+l)*16;
                                            reflected_curved_owner_oracle::SourceTap tap;
                                            tap.identity=old.words[h.identity_offset/4+np];
                                            tap.depth=std::bit_cast<float>(old.words[h.witness_offset/4+np]);
                                            std::copy_n(old.words.begin()+na,9,tap.path.begin());return tap;
                                        });
                                    solution=reflected_curved_owner_oracle::previous_source(*motion_inputs,id,
                                        std::span<const unsigned,16>(raw.words.data()+at,16),
                                        material==Effect::mirror?0:double(.55F),l,x,y,qualified_root,
                                        std::bit_cast<float>(raw.words[h.witness_offset/4+p]),starts);
                                }
                                if(ambiguity_witness) {
                                    check(id==(liquid_ambiguity_witness?0xfffffffdU:model_return_ambiguity_witness?43U:0U)
                                        && control==(liquid_ambiguity_witness?546U:529U) && !solution.valid && solution.ambiguous,
                                        "Disocclusion regression lost its independently visible distinct optical roots");
                                    check(raw.words[at+9]==out.words[at+9],
                                        ("Owner reused an ambiguous multi-hop liquid colour footprint phase="+std::to_string(phase)
                                        +" eye="+std::to_string(eye)+" xy="+std::to_string(x)+","+std::to_string(y)).c_str());
                                    check(!source_validation || !(admitted&(1U<<l)),
                                        "Source admission published an independently ambiguous optical footprint");
                                    ++independently_ambiguous_refusals;
                                    continue;
                                }
                                if(!solution.valid) {
                                    emit_optical_replay();
                                    std::cout<<"Independent source ambiguity="<<solution.ambiguous<<'\n';
                                    for(const auto& attempt:{attempted_root,attempted_seed})if(attempt)
                                        std::cout<<"Independent source attempt xy="<<attempt->x<<','<<attempt->y
                                            <<" depth="<<attempt->depth<<" error="<<attempt->error<<" seed="<<attempt->bounded_seed<<'\n';
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
                                    if(!refusal_trace.empty()) {
                                        const unsigned tr=(p*lobes+l)*28;
                                        std::cout<<"Separate GPU diagnostic (not replay input): guide=";
                                        for(unsigned n=0;n<4;++n)std::cout<<std::bit_cast<float>(refusal_trace[tr+n])<<',';
                                        std::cout<<" target=";
                                        for(unsigned n=4;n<8;++n)std::cout<<std::bit_cast<float>(refusal_trace[tr+n])<<',';
                                        std::cout<<" residual="<<std::bit_cast<float>(refusal_trace[tr+22])<<','
                                            <<std::bit_cast<float>(refusal_trace[tr+23])<<" refusal="<<refusal_trace[tr+8]<<'\n';
                                    }
#endif
                                    std::cout<<std::flush;
                                    throw std::runtime_error("Moving owner reused without an independently qualified old source ground="
                                    +std::to_string(ground_material)+" material="+std::to_string(unsigned(material))+" phase="
                                    +std::to_string(phase)+" eye="+std::to_string(eye)+" xy="+std::to_string(x)+","+std::to_string(y)
                                    +" id="+std::to_string(id)+" control="+std::to_string(control)+" lobe="+std::to_string(l));
                                }
                                sx=solution.x;sy=solution.y;old_depth=solution.depth;++moving_root_witnesses;
                                independently_solved_x=sx;independently_solved_y=sy;
                                independently_bounded_seeds+=solution.bounded_seed;
                            }
                            old_valid=sx>=0 && sy>=0 && sx<=out.output.width-1 && sy<=out.output.height-1;
                            const auto& old=accepted[sample][eye];
                            double inverse_depth=0;
                            if(old_valid) for(unsigned oy=0;oy<2;++oy) for(unsigned ox=0;ox<2;++ox) {
                                const double fx=sx-std::floor(sx),fy=sy-std::floor(sy);
                                const double share=(ox?fx:1-fx)*(oy?fy:1-fy);if(!share)continue;
                                const unsigned nx=std::min(unsigned(std::floor(sx))+ox,out.output.width-1),
                                    ny=std::min(unsigned(std::floor(sy))+oy,out.output.height-1),np=ny*out.output.width+nx;
                                const unsigned na=h.incoming_offset/4+(np*lobes+l)*16;
                                const auto old_id=old.words[h.identity_offset/4+np];old_ink|=old_id==UINT32_MAX;
                                old_valid=old_valid && old_id==id
                                    && std::equal(raw.words.begin()+at,raw.words.begin()+at+6,old.words.begin()+na);
                                if(motion) {
                                    const double depth=std::bit_cast<float>(old.words[h.witness_offset/4+np]);
                                    old_valid=old_valid && std::isfinite(depth) && depth>0;
                                    inverse_depth+=share/depth;
                                }
                                const auto value=rgb(old.words[na+9]);for(unsigned c=0;c<3;++c)prior[c]+=value[c]*share;
                                if(!detailed && raw.words[at+9]!=out.words[at+9])
                                    for(unsigned n=0;n<9;++n)check(old.words[na+n]==raw.words[at+n],"Static authored old source changed its actual optical path");
                            }
                            if(motion)old_valid=old_valid && std::isfinite(inverse_depth) && inverse_depth>0
                                && std::abs(1/inverse_depth-old_depth)<=std::max(.01,old_depth*.005);
                            if(!old_valid) {
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
                                if(motion && raw.words[at+9]!=out.words[at+9] && !refusal_trace.empty()) {
                                    const unsigned tr=(p*lobes+l)*28;
                                    std::cout<<"Moving owner diagnostic only: independently solved source="<<std::setprecision(12)<<sx<<','<<sy
                                        <<" actual guide="<<std::bit_cast<float>(refusal_trace[tr])<<','<<std::bit_cast<float>(refusal_trace[tr+1])
                                        <<" actual depth="<<std::bit_cast<float>(refusal_trace[tr+2])
                                        <<" refusal="<<refusal_trace[tr+8]<<" solve residual="<<std::bit_cast<float>(refusal_trace[tr+22])
                                        <<','<<std::bit_cast<float>(refusal_trace[tr+23])<<std::endl;
                                }
#endif
                                if(raw.words[at+9]!=out.words[at+9])throw std::runtime_error(
                                    "Owner reused a different/missing independent old footprint ground="+std::to_string(ground_material)
                                    +" material="+std::to_string(unsigned(material))+" phase="+std::to_string(phase)
                                    +" eye="+std::to_string(eye)+" xy="+std::to_string(x)+","+std::to_string(y)
                                    +" id="+std::to_string(id)+" control="+std::to_string(control)+" lobe="+std::to_string(l)
                                    +" independent-source="+std::to_string(sx)+","+std::to_string(sy)
                                    +" old-depth="+std::to_string(old_depth)+" tap-depth="+std::to_string(1/inverse_depth));
                                ++refused_old_taps;protected_old_taps+=old_ink;
                            }
                        }
                        const auto expected_unrounded_incident=[&] {
                            auto value=rgb(raw.words[at+9]),low=value,high=value;
                            const unsigned x=p%out.output.width,y=p/out.output.width;
                            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                                const unsigned nx=unsigned(std::clamp(int(x)+dx,0,int(out.output.width)-1)),ny=unsigned(std::clamp(int(y)+dy,0,int(out.output.height)-1));
                                const unsigned np=ny*out.output.width+nx,na=h.incoming_offset/4+(np*lobes+l)*16;
                                if(out.words[h.identity_offset/4+np]!=id || !std::equal(raw.words.begin()+at,raw.words.begin()+at+6,raw.words.begin()+na))continue;
                                const auto neighbour=rgb(raw.words[na+9]);for(unsigned c=0;c<3;++c) {low[c]=std::min(low[c],neighbour[c]);high[c]=std::max(high[c],neighbour[c]);}
                            }
                            for(unsigned c=0;c<3;++c)value[c]=value[c]*.15+std::clamp(prior[c],low[c],high[c])*.85;
                            return value;
                        };
                        const auto expected_incident=[&] {return pack(expected_unrounded_incident());};
                        if((direct_model || curved_model_path) && old_valid) {
                            if(direct_model) {
                                ++phase_witness.matched_direct;
                                phase_witness.different_direct_source+=pack(prior)!=raw.words[at+9];
                                phase_witness.different_clamped_direct+=expected_incident()!=raw.words[at+9];
                            } else {
                                ++phase_witness.matched_curved;
                                phase_witness.different_curved_source+=((pack(prior)^raw.words[at+9])&0xffffffU)!=0;
                                phase_witness.different_clamped_curved+=((expected_incident()^raw.words[at+9])&0xffffffU)!=0;
                            }
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
                            if(shifted && !refusal_trace.empty() && traced_samples<8 && ((expected_incident()^raw.words[at+9])&0xffffffU)!=0
                                && (ground_material==5?direct_model:curved_model_path)) {
                                const unsigned tr=(p*lobes+l)*28;
                                const auto scalar=[&](unsigned offset){return std::bit_cast<float>(refusal_trace[tr+offset]);};
                                std::array<double,3> footprint{2.e-6,2.e-6,2.e-6};
                                const auto& previous=accepted[sample][eye];
                                const unsigned refused=refusal_trace[tr+9];
                                double authored_inverse_depth=0;
                                const double sx=double(p%out.output.width)-.25,sy=double(p/out.output.width)+.375;
                                for(unsigned oy=0;oy<2;++oy)for(unsigned ox=0;ox<2;++ox) {
                                    const unsigned nx=unsigned(std::floor(sx))+ox,ny=unsigned(std::floor(sy))+oy;
                                    const double fx=sx-std::floor(sx),fy=sy-std::floor(sy);
                                    const double share=(ox?fx:1-fx)*(oy?fy:1-fy);
                                    authored_inverse_depth+=share/std::bit_cast<float>(previous.words[h.witness_offset/4+ny*out.output.width+nx]);
                                }
                                if(refused<count) {
                                    const int tx=refused%out.output.width,ty=refused/out.output.width;
                                    const double at_xy[]{double(p%out.output.width)-.25,double(p/out.output.width)+.375};
                                    const unsigned old_at=h.incoming_offset/4+(refused*lobes+l)*16;
                                    for(unsigned axis=0;axis<2;++axis) {
                                        std::array<double,3> gradient{};
                                        for(int side:{-1,1}) {
                                            const int nx=tx+(axis==0?side:0),ny=ty+(axis==1?side:0);
                                            if(nx<0 || ny<0 || nx>=int(out.output.width) || ny>=int(out.output.height))continue;
                                            const unsigned np=ny*out.output.width+nx,na=h.incoming_offset/4+(np*lobes+l)*16;
                                            if(previous.words[h.identity_offset/4+np]!=id || !std::equal(raw.words.begin()+at,raw.words.begin()+at+6,previous.words.begin()+na))continue;
                                            for(unsigned c=0;c<3;++c)gradient[c]=std::max(gradient[c],1.5*std::abs(
                                                double(std::bit_cast<float>(previous.words[na+6+c]))-std::bit_cast<float>(previous.words[old_at+6+c])));
                                        }
                                        for(unsigned c=0;c<3;++c)footprint[c]+=gradient[c]*std::abs(at_xy[axis]-(axis==0?tx:ty));
                                    }
                                    for(auto& value:footprint)value=std::min(value,.02);
                                }
                                std::cout<<"Actual owner "<<(curved_model_path?"curved":"direct")<<" decision xy="<<p%out.output.width<<','<<p/out.output.width
                                    <<" primary="<<id<<" control="<<control<<" refusal="<<refusal_trace[tr+8]
                                    <<" refused-tap="<<refusal_trace[tr+9]
                                    <<" delta="<<scalar(12)<<','<<scalar(13)<<','<<scalar(14)
                                    <<" cap="<<scalar(16)<<','<<scalar(17)<<','<<scalar(18)
                                    <<" independent-footprint="<<footprint[0]<<','<<footprint[1]<<','<<footprint[2]
                                    <<" gpu-neighbour-checks="<<std::hex<<refusal_trace[tr+15]<<std::dec
                                    <<" gpu-rechecked-cap-x="<<scalar(19)
                                    <<" refused-depth="<<(refused<count?std::bit_cast<float>(previous.words[h.witness_offset/4+refused]):0)
                                    <<" authored-depth="<<1/authored_inverse_depth
                                    <<" guide="<<scalar(0)<<','<<scalar(1)<<','<<scalar(2)<<','<<scalar(3)
                                    <<" solve="<<scalar(20)<<','<<scalar(21)<<" error="<<scalar(22)<<','<<scalar(23)
                                    <<" state/iteration/backtrack/accept="<<refusal_trace[tr+24]<<','<<refusal_trace[tr+25]<<','<<refusal_trace[tr+26]<<','<<refusal_trace[tr+27]<<std::endl;
                                ++traced_samples;
                            }
#endif
                        }
                        if(source_validation) {
                            const bool reused=(admitted&(1U<<l))!=0;
                            check(raw.words[at+9]==out.words[at+9] || reused,
                                "Changed incident missing its discrete admission witness");
                            if(reused) {
                                check(phase && id!=UINT32_MAX && old_valid,
                                    "Admitted source lobe has no independent valid old footprint");
                                ++source_independent_footprints;
                            }
                            if(admitted) {
                                auto transported=reused?expected_unrounded_incident():rgb(raw.words[at+9]);
                                for(unsigned c=0;c<3;++c)transported[c]=transported[c]
                                    *std::bit_cast<float>(raw.words[at+10+c])+std::bit_cast<float>(raw.words[at+13+c]);
                                const auto quantized=rgb(pack(transported));
                                for(unsigned c=0;c<3;++c)source_composed_sum[c]+=quantized[c]/lobes;
                            }
                            source_admitted_lobes+=reused;
                        }
                        if(raw.words[at+9]==out.words[at+9])continue;
                        check(phase && id!=UINT32_MAX,"Cold/protected owner invented old incident RGB");
                        check(old_valid,"Owner reused invalid independently authored old bilinear taps");
                        const auto expected=expected_incident();for(unsigned c=0;c<3;++c)
                            if(std::abs(int((out.words[at+9]>>(c*8))&255)-int((expected>>(c*8))&255))>1) {
                                emit_optical_replay();
#if defined(STARFOX_CURVED_OWNER_TRACE_AVAILABLE)
                                if(!refusal_trace.empty()) {
                                    const unsigned tr=(p*lobes+l)*28;
                                    std::cout<<std::setprecision(12)<<"RGB failure separate GPU trace (not reference input): guide=";
                                    for(unsigned n=0;n<4;++n)std::cout<<std::bit_cast<float>(refusal_trace[tr+n])<<',';
                                    std::cout<<" terminal=";
                                    for(unsigned n=4;n<8;++n)std::cout<<std::bit_cast<float>(refusal_trace[tr+n])<<',';
                                    std::cout<<" refusal="<<refusal_trace[tr+8]<<" solve=";
                                    for(unsigned n=20;n<24;++n)std::cout<<std::bit_cast<float>(refusal_trace[tr+n])<<',';
                                    std::cout<<std::endl;
                                }
#endif
                                throw std::runtime_error("Actual owner incident RGB disagrees with independent authored old-source oracle ground="
                                    +std::to_string(ground_material)+" material="+std::to_string(unsigned(material))
                                    +" phase="+std::to_string(phase)+" eye="+std::to_string(eye)
                                    +" xy="+std::to_string(p%out.output.width)+","+std::to_string(p/out.output.width)
                                    +" id="+std::to_string(id)+" control="+std::to_string(control)+" lobe="+std::to_string(l)
                                    +" source="+std::to_string(independently_solved_x)+","+std::to_string(independently_solved_y)
                                    +" channel="+std::to_string(c)+" current="+std::to_string((raw.words[at+9]>>(c*8))&255)
                                    +" actual="+std::to_string((out.words[at+9]>>(c*8))&255)
                                    +" expected="+std::to_string((expected>>(c*8))&255));
                            }
                        if(source_validation && lobes==1) {
                            // Independently finish EVERY changed sharp pixel
                            // from unrounded admitted RGB and RAW CURRENT
                            // material. Never decode the rounded diagnostic
                            // incident word as the compositor's colour input.
                            // Keep this changed-sharp check alongside the full
                            // discrete-mask all-lobe composition check below.
                            auto transported=expected_unrounded_incident();
                            for(unsigned c=0;c<3;++c)transported[c]=transported[c]
                                *std::bit_cast<float>(raw.words[at+10+c])
                                +std::bit_cast<float>(raw.words[at+13+c]);
                            auto composed=rgb(pack(transported));
                            for(unsigned c=0;c<3;++c)composed[c]
                                =std::bit_cast<float>(raw.words[h.base_offset/4+p*4+c])
                                +composed[c]*std::bit_cast<float>(raw.words[h.weight_offset/4+p*4+c]);
                            const unsigned expected_pixel=pack(composed);
                            check((out.words[p]&0xff000000U)==(raw.words[p]&0xff000000U),
                                "Source composition changed RAW CURRENT opacity");
                            for(unsigned c=0;c<3;++c)check(std::abs(int((out.words[p]>>(c*8))&255)
                                -int((expected_pixel>>(c*8))&255))<=1,
                                "Sharp source composition missed independent unrounded RGB/CURRENT material oracle");
                            ++source_unrounded_compositions;
                        }
                        ++cohort_changed;
                        ++witness.changed;
                        ++phase_witness.changed;
                        phase_witness.model+=id<model_triangles;phase_witness.liquid+=id==0xfffffffdU;
                        // Native record: mirrors[0..3], control[4], terminal[5].
                        // Kind 2 is an actual environment escape; kind 3 is
                        // four-hop budget exhaustion, never a direct sky ray.
                        phase_witness.curved_model+=id<model_triangles && ((control>>4)&15U)!=0;
                        phase_witness.direct_model+=direct_model;
                    }
                    if(source_validation && admitted) {
                        for(unsigned c=0;c<3;++c)source_composed_sum[c]
                            =std::bit_cast<float>(raw.words[h.base_offset/4+p*4+c])
                            +source_composed_sum[c]*std::bit_cast<float>(raw.words[h.weight_offset/4+p*4+c]);
                        const auto expected=pack(source_composed_sum);
                        for(unsigned c=0;c<3;++c)check(std::abs(int((out.words[p]>>(c*8))&255)
                            -int((expected>>(c*8))&255))<=1,
                            "Complete source composition missed independent unrounded lobe/CURRENT material oracle");
                        check((out.words[p]&0xff000000U)==(raw.words[p]&0xff000000U),
                            "Complete source composition changed CURRENT opacity");
                        ++source_composed_pixels;
                    }
                    if(detailed && id!=UINT32_MAX) {
                        for(unsigned c=0;c<3;++c)composed_sum[c]=std::bit_cast<float>(raw.words[h.base_offset/4+p*4+c])
                            +composed_sum[c]*std::bit_cast<float>(raw.words[h.weight_offset/4+p*4+c]);
                        const auto expected=pack(composed_sum);
                        for(unsigned c=0;c<3;++c) {
                            if(std::abs(int((out.words[p]>>(c*8))&255)-int((expected>>(c*8))&255))>2)
                                throw std::runtime_error("Fractional owner radiance missed independently validated incident/CURRENT recomposition ground="
                                    +std::to_string(ground_material)+" material="+std::to_string(unsigned(material))
                                    +" phase="+std::to_string(phase)+" eye="+std::to_string(eye)
                                    +" xy="+std::to_string(p%out.output.width)+","+std::to_string(p/out.output.width)
                                    +" channel="+std::to_string(c)+" actual="+std::to_string((out.words[p]>>(c*8))&255)
                                    +" expected="+std::to_string((expected>>(c*8))&255));
                            ++recomposed_channels;
                        }
                        check((out.words[p]&0xff000000U)==(raw.words[p]&0xff000000U),"Fractional owner changed CURRENT radiance opacity");
                    }
                }
            }
            for(auto* buffer:current_buffers)check(!retained_buffers.contains(buffer),"Ordered native input aliases another eye/sample's accepted history");
            check(render(frame)==images,"Held complete ordered-liquid owner retraced/jittered");
            for(unsigned sample=0;sample<samples;++sample)for(unsigned eye=0;eye<2;++eye)
                check(download(renderer.accepted_reflection_sample(eye,sample)).words==candidate[sample][eye].words,
                "Held ordered-liquid presentation mutated accepted records");
            if(source_validation)for(unsigned sample=0;sample<samples;++sample)for(unsigned eye=0;eye<2;++eye) {
                const auto held=renderer.accepted_reflection_source_admission(eye,sample);
                check(held.buffer==candidate_admissions[sample][eye].output.buffer
                    && download_admission(held)==candidate_admissions[sample][eye].words,
                    "Held source presentation replaced or mutated its accepted lobe mask");
            }
            accepted_admissions=std::move(candidate_admissions);
            accepted=std::move(candidate);last=frame;
        }
        if(!cohort_changed || model_pixels<=100 || liquid_pixels<=100)throw std::runtime_error(
            "Actual ordered-liquid cohort bypassed reuse or missed receiver ownership ground="+std::to_string(ground_material)
            +" model="+std::to_string(unsigned(material))+" changed="+std::to_string(cohort_changed)
            +" model/liquid="+std::to_string(model_pixels)+"/"+std::to_string(liquid_pixels));
        changed+=cohort_changed;
        if(source_validation) {
            check(source_admitted_lobes>=cohort_changed && source_composed_pixels>0
                && source_independent_footprints==source_admitted_lobes,
                "Complete unrounded source oracle omitted admitted lobes/pixels");
            std::cout<<"Actual full unrounded source composition pixels="<<source_composed_pixels
                <<" admitted-lobes="<<source_admitted_lobes
                <<" independent-footprints="<<source_independent_footprints
                <<" changed-incidents="<<cohort_changed<<std::endl;
        }
        if(source_validation && material==Effect::mirror) {
            check(source_unrounded_compositions==cohort_changed,
                "Sharp source composition oracle omitted an admitted incident");
            std::cout<<"Actual sharp unrounded composition witnesses="<<source_unrounded_compositions<<std::endl;
        }
        if(detailed) for(unsigned phase=1;phase<phases;++phase) for(unsigned eye=0;eye<2;++eye) {
            const auto& witness=phase_reuse[phase][0][eye];
            if(motion)check(witness.direct_wave_changes>0,
                "Moving owner did not change native liquid-to-sky optics with its actual advancing clock");
            if(ground_material==9) {
                std::cout<<"Lava curved model diagnostic phase="<<phase<<" eye="<<eye
                    <<" current/matched/different-source/clamped="<<witness.current_curved<<'/'<<witness.matched_curved
                    <<'/'<<witness.different_curved_source<<'/'<<witness.different_clamped_curved<<" kinds=";
                for(unsigned kind=0;kind<witness.curved_kinds.size();++kind)std::cout<<kind<<':'<<witness.curved_kinds[kind]<<',';
                std::cout<<" refusals=";
                for(unsigned reason=0;reason<witness.curved_refusals.size();++reason)if(witness.curved_refusals[reason])
                    std::cout<<reason<<':'<<witness.curved_refusals[reason]<<',';
                std::cout<<std::endl;
            }
            if(!witness.model || !witness.liquid || !witness.curved_model || !witness.direct_model) throw std::runtime_error(
                "Fractional owner missed model/liquid reuse ground="+std::to_string(ground_material)
                +" material="+std::to_string(unsigned(material))+" phase="+std::to_string(phase)
                +" eye="+std::to_string(eye)+" changed model/liquid="+std::to_string(witness.model)+"/"+std::to_string(witness.liquid)
                +" curved/direct model="+std::to_string(witness.curved_model)+"/"+std::to_string(witness.direct_model)
                +" direct current/matched/different-source/clamped="+std::to_string(witness.current_direct)+"/"
                +std::to_string(witness.matched_direct)+"/"+std::to_string(witness.different_direct_source)+"/"
                +std::to_string(witness.different_clamped_direct));
        }
        for(unsigned sample=0;sample<samples;++sample)for(unsigned eye=0;eye<2;++eye) {
            const auto& witness=coverage[sample][eye];
            if(!witness.changed || witness.model<=50 || witness.liquid<=50
                || (quality && ground_material==5 && witness.surface<=50))throw std::runtime_error(
                "Ordered liquid missed positive per-eye/sample coverage ground="+std::to_string(ground_material)
                +" model="+std::to_string(unsigned(material))+" samples="+std::to_string(samples)
                +" sample="+std::to_string(sample)+" eye="+std::to_string(eye)+" changed="+std::to_string(witness.changed)
                +" model/liquid/surface="+std::to_string(witness.model)+"/"+std::to_string(witness.liquid)+"/"+std::to_string(witness.surface));
        }
        auto rejected=std::make_shared<CalibratedGameFrame>(*last);rejected->presentation_seconds+=1./120;
        for(auto& draw:rejected->draws)if(draw.source_key==1)draw.packet.model[12]+=.04F;
        gpu_mock.fail_end=true;auto outcome=renderer.submit(rejected);
        for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);outcome=renderer.poll();}
        check(outcome==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected ordered-liquid presentation was accepted/pending");
        (void)read();for(unsigned sample=0;sample<samples;++sample)for(unsigned eye=0;eye<2;++eye) {
            const auto bank=download(renderer.accepted_reflection_sample(eye,sample));
            check(bank.output.buffer==accepted[sample][eye].output.buffer && bank.words==accepted[sample][eye].words,
                "Rejected whole presentation replaced ordered-liquid accepted history");
        }
        if(quality) {
            auto resized=std::make_shared<CalibratedGameFrame>(*last);resized->settings.aa_quality=quality==3?1:3;
            gpu_mock.fail_end=true;auto result=renderer.submit(resized);
            for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
            check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected ordered-liquid sample-count change stayed pending");
            (void)read();for(unsigned sample=0;sample<samples;++sample)for(unsigned eye=0;eye<2;++eye) {
                const auto bank=download(renderer.accepted_reflection_sample(eye,sample));
                check(bank.output.buffer==accepted[sample][eye].output.buffer && bank.words==accepted[sample][eye].words,
                    "Rejected ordered-liquid count transition replaced/released accepted sample banks");
            }
        }
        check(render(last)==phase_images[phases-1],"Rejected ordered-liquid frame/count change altered the restored finished image");
        auto cut=std::make_shared<CalibratedGameFrame>(*last);cut->settings.history_epoch+=100;
        const auto cut_images=render(cut);
        if(reconstruct || detailed)for(unsigned eye=0;eye<2;++eye) {
            unsigned changed_models=0,changed_liquids=0;
            for(unsigned pixel=0;pixel<cut_images[eye].size()/4;++pixel) {
                bool model=false,liquid=false;
                for(unsigned sample=0;sample<samples;++sample) {
                    const auto& bank=accepted[sample][eye];
                    const auto id=bank.words[bank.output.reflection_history.identity_offset/4+pixel];
                    model|=id<model_triangles;liquid|=id==0xfffffffdU;
                }
                for(unsigned c=0;c<3;++c)if(cut_images[eye][pixel*4+c]!=phase_images[phases-1][eye][pixel*4+c]) {
                    ++finished_reuse_bytes;changed_models+=model;changed_liquids+=liquid;
                }
            }
            if(!changed_models || !changed_liquids)throw std::runtime_error(
                "Ordered incident reuse missed finished model/liquid RGB ground="+std::to_string(ground_material)
                +" material="+std::to_string(unsigned(material))+" samples="+std::to_string(samples)
                +" lens="+std::to_string(shifted)+" eye="+std::to_string(eye)
                +" model/liquid bytes="+std::to_string(changed_models)+"/"+std::to_string(changed_liquids));
        }
        for(unsigned sample=0;sample<samples;++sample)for(unsigned eye=0;eye<2;++eye) {
            const auto retained=download(renderer.accepted_reflection_sample(eye,sample));
            const auto current=download(renderer.current_reflection_sample(eye,sample));
            const auto& layout=retained.output.reflection_history;
            check(layout==current.output.reflection_history && retained.words.size()==current.words.size()
                && retained.words.size()==accepted[sample][eye].words.size(),"History-only owner cut changed native layout");
            const unsigned identity=layout.identity_offset/4,count=retained.output.width*retained.output.height;
            for(unsigned word=0;word<current.words.size();++word) {
                const bool ownership=word>=identity && word<identity+count;
                // A history cut changes neither raster ownership nor authored
                // optics. Native RT's underlying primary behind protected ink
                // is NOT an accepted old footprint. Keep the exact pre-cut
                // ownership map, and compare EVERY other word with CURRENT.
                // In particular, no radiance/path/light/canonical word may be
                // waived merely because its pixel is protected or unowned.
                const auto expected=ownership?accepted[sample][eye].words[word]:current.words[word];
                if(retained.words[word]!=expected)throw std::runtime_error(
                    "History-only owner cut "+std::string(ownership?"ownership":"CURRENT")
                    +" mismatch ground="+std::to_string(ground_material)+" material="+std::to_string(unsigned(material))
                    +" sample="+std::to_string(sample)+" eye="+std::to_string(eye)
                    +" word="+std::to_string(word)+" expected="+std::to_string(expected)
                    +" accepted="+std::to_string(retained.words[word])+" raw="+std::to_string(current.words[word]));
                if(ownership) {
                    check(expected==current.words[word] || expected==UINT32_MAX,"Cut invented a different native primary");
                    cut_ownership_refusals+=expected!=current.words[word];
                } else ++cut_current_words;
            }
        }
        last=cut;
        if(quality) {
            // Change the genuine hardware sample count in BOTH directions.
            // Across qualities this covers every directed 2/4/8 transition.
            // Do not cut the history manually: the production timeline must
            // reject correspondence between different sample grids itself.
            for(unsigned next_quality:{quality%3+1,quality}) {
                const auto before_bytes=renderer.reflection_history_resident_bytes();
                auto resized=std::make_shared<CalibratedGameFrame>(*last);
                resized->settings.aa_quality=next_quality;
                const unsigned next_samples=1U<<next_quality;
                const auto resized_images=render(resized);
                check(resized_images[0]!=resized_images[1],"Accepted count transition collapsed eye projections");
                std::unordered_set<void*> buffers;
                std::uint64_t expected_bytes=0;
                for(unsigned sample=0;sample<next_samples;++sample) for(unsigned eye=0;eye<2;++eye) {
                    const auto retained=download(renderer.accepted_reflection_sample(eye,sample));
                    const auto current=download(renderer.current_reflection_sample(eye,sample));
                    const auto& layout=retained.output.reflection_history;
                    check(layout==current.output.reflection_history && retained.words.size()==current.words.size(),
                        "Accepted count transition changed CURRENT native layout");
                    check(buffers.insert(retained.output.buffer).second && buffers.insert(current.output.buffer).second,
                        "Accepted count transition aliases an eye/sample CURRENT or history bank");
                    expected_bytes+=std::uint64_t(layout.storage_bytes)*2;
                    const unsigned identity=layout.identity_offset/4,count=retained.output.width*retained.output.height;
                    unsigned models=0,liquids=0;
                    for(unsigned word=0;word<current.words.size();++word) {
                        const bool ownership=word>=identity && word<identity+count;
                        if(ownership) {
                            const auto id=retained.words[word];
                            check(id==UINT32_MAX || id==current.words[word],"New sample grid invented a CURRENT primary");
                            models+=id<model_triangles;liquids+=id==0xfffffffdU;
                        } else {
                            check(retained.words[word]==current.words[word],
                                "Accepted count transition reused stale RGB/path/light/canonical data from another sample grid");
                            ++transition_current_words;
                        }
                    }
                    check(models>50 && liquids>50,"Accepted count transition lost real model/liquid coverage in an eye/sample");
                }
                const auto after_bytes=renderer.reflection_history_resident_bytes();
                check(after_bytes==expected_bytes,"Accepted count transition retained inactive or replacement history allocations");
                if(before_bytes>after_bytes) retired_history_bytes+=before_bytes-after_bytes;
                for(unsigned sample=next_samples;sample<8;++sample) for(unsigned eye=0;eye<2;++eye)
                    check(!renderer.accepted_reflection_sample(eye,sample).buffer && !renderer.current_reflection_sample(eye,sample).buffer,
                        "Accepted count reduction exposed a retired sample bank");
                check(render(resized)==resized_images && renderer.reflection_history_resident_bytes()==after_bytes,
                    "Held accepted count transition jittered or allocated more history");
                ++count_transitions;last=resized;
            }
        }
        std::cout<<"D3D12 ordered liquid owner ground="<<ground_material<<" model="<<unsigned(material)
            <<" samples="<<samples<<" complete-source-stream="<<source_validation
            <<" independent incident witnesses="<<cohort_changed<<" model/liquid="<<model_pixels<<'/'<<liquid_pixels<<std::endl;
    }
    check(protected_paths>0,"Ordered owner fixture missed nonempty CURRENT paths behind protected raster ink");
    check(!quality || canonical_words>0,"Sampled ordered water never exercised a nonzero canonical prefix");
    if(shifted) {
        check(refused_old_taps>100 && protected_old_taps>0 && finished_reuse_bytes>0 && recomposed_channels>10000,
            "Fractional owner lacked positive old-footprint/ink refusal and finished-radiance witnesses");
        std::cout<<"D3D12 ordered-liquid located-lens out/return: independent bilinear incident changes="<<changed
            <<" refused old footprints="<<refused_old_taps<<" protected old footprints="<<protected_old_taps
            <<" finished reuse RGB bytes="<<finished_reuse_bytes<<" recomposed radiance channels="<<recomposed_channels
            <<"; geometry/liquid clock/view fixed, actual XR FOV displaced then restored, no downloaded inverse oracle.\n";
    }
    if(motion) {
        check(moving_geometry_words>100 && advanced_liquid_clocks==16 && moving_root_witnesses>100
            && finished_reuse_bytes>0 && recomposed_channels>10000,
            "Moving owner lacked real geometry/clock/independent root/finished-radiance coverage");
        std::cout<<"D3D12 ordered-liquid rigid-model out/return + advancing wave clock: changed geometry words="<<moving_geometry_words
            <<" accepted-old clock checks="<<advanced_liquid_clocks<<" independent old forward roots="<<moving_root_witnesses
            <<" independently visibility-qualified bounded initializers="<<independently_bounded_seeds
            <<" finished reuse RGB bytes="<<finished_reuse_bytes<<" recomposed channels="<<recomposed_channels
            <<(camera_motion?"; real eye translation/yaw, fixed lens, no downloaded inverse as oracle.\n":"; fixed eye/lens, no downloaded inverse as oracle.\n");
    }
    if(camera_motion) {
        check(changed_eye_views==16 && changed_ownership_pixels>0,
            "Camera owner missed actual eye/ownership changes");
        std::cout<<"D3D12 ordered-liquid located-eye out/return: changed eye views="<<changed_eye_views
            <<" changed primary ownership pixels="<<changed_ownership_pixels
            <<"; binary64 current-view/world/old-view sky transport, no output-colour source selection.\n";
    }
    if(disocclusion) {
        check(uncovered_primary_pixels>20 && covered_primary_pixels>20 && changed_ownership_pixels>1000
            && independently_ambiguous_refusals==3,
            "Disocclusion owner missed real exposed/covered primary samples");
        std::cout<<"D3D12 ordered-liquid visibility out/return: uncovered primary pixels="<<uncovered_primary_pixels
            <<" covered primary pixels="<<covered_primary_pixels
            <<" independently ambiguous CURRENT-only footprints="<<independently_ambiguous_refusals
            <<"; larger located-eye/rigid motion and moving opaque raster occluder, no history cuts or relaxed optical guards.\n";
    }
    if(reconstruct) {
        check(reconstructed_bytes>10000 && protected_image_bytes>50 && finished_reuse_bytes>0,
            "Ordered-liquid final reconstruction lacked independent colour/ink/reuse coverage");
        std::cout<<"D3D12 ordered-liquid final reconstruction samples="<<samples<<": "<<reconstructed_bytes
            <<" independent external-XR averaged image bytes, "<<protected_image_bytes<<" exact protected centre bytes, "
            <<finished_reuse_bytes<<" history-only changed finished RGB bytes; no shifted native-overlay edge exclusions.\n";
    }
    if(quality) {
        check(count_transitions==8 && transition_current_words>0 && retired_history_bytes>0,
            "Ordered-liquid accepted sample transitions lacked positive retirement/CURRENT coverage");
        std::cout<<"D3D12 ordered-liquid accepted count transitions starting samples="<<samples
            <<": transitions="<<count_transitions<<" exact cold CURRENT words="<<transition_current_words
            <<" retired history bytes="<<retired_history_bytes
            <<"; all resident slots counted, both directions, no manual history reset.\n";
    }
    std::cout<<"D3D12 ordered liquid owner samples="<<samples<<": changed="<<changed<<" exact CURRENT path/light words="<<current_words
        <<" protected="<<protected_pixels<<" protected nonempty paths="<<protected_paths<<" canonical words="<<canonical_words
        <<" cut CURRENT words="<<cut_current_words<<" cut ownership refusals="<<cut_ownership_refusals
        <<"; resident authored GPU environment, sharp/eight lobes, water/lava, actual whole-presentation acceptance/discard/held/cut passed. "
        <<(camera_motion?"Bounded real eye/rigid/clock motion with independent old forward optics; ":motion?"Bounded real rigid/clock motion with independent old forward optics; ":shifted?"Authored fractional located-lens out/return bilinear old-source oracle only; ":"Static authored old-source oracle only; ")
        <<"independent visible-bank fallback cases="<<independent_bank_fallbacks<<"; "
        <<"arbitrary motion/visibility, other backends, physical panel and cost remain unqualified; complete-source-stream="<<source_validation<<".\n";
    return last;
}
