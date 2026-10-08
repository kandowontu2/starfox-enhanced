// Fixture-only scalar ray/footprint oracle. Never used by production shading.
void native_ground_surface_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
    ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
    for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
    for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
    auto original=vr::landscape_sphere_packet(ppu,{},112,false,false,232);
    vr::place_landscape_ground(original,-.75F,true);
    unsigned compared=0,sky=0,changed=0;
    for(unsigned material:{0U,5U,8U,9U}) for(float seconds:{1.375F,2.25F})
        for(float bank:{0.F,.21F}) for(unsigned e=0;e<2;++e)
        for(unsigned motion=0;motion<(material==5?4U:1U);++motion) {
        render::CalibratedGroundGradient gradient{{.31F,.72F,.21F},{.04F,.19F,.035F},112,111};
        gradient.material=material;gradient.origin={123.25F,-415.5F};
        gradient.seconds=seconds;gradient.brightness=.65F;
        gradient.motion=material==5?motion:material==0?3:material==8?2:0;
        auto enhanced=original;check(render::apply_calibrated_ground(enhanced,gradient),"Native surface payload failed");
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
        eye.pose.position={e?.035F:-.028F,.027F,.04F};eye.fov={-.64F,.72F,.57F,-.52F};
        auto camera=camera_for(eye,d3d);camera.projection[10]=-1;camera.projection[14]=-.05F;
        const float cosine=std::cos(bank),sine=std::sin(bank);
        enhanced.model={cosine,sine,0,0,-sine,cosine,0,0,0,0,1,0,.08F,0,-.12F,1};
        original.model=enhanced.model;
        render::CalibratedScenePacket off{&original,vr::SceneBlend::opaque,false};
        const auto baseline=render_packets(gpu,scene,std::span(&off,1),camera);
        render::CalibratedScenePacket draw{&enhanced,vr::SceneBlend::opaque,false};
        const auto pixels=render_packets(gpu,scene,std::span(&draw,1),camera);
        const double ox=eye.pose.position.x-.08,oy=eye.pose.position.y,oz=eye.pose.position.z+.12;
        const double localEyeX=cosine*ox+sine*oy,localEyeY=-sine*ox+cosine*oy;
        const auto hit=[&](unsigned x,unsigned y) {
            const double dx=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5)/width);
            const double dy=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5)/height);
            const double localRayY=-sine*dx+cosine*dy;
            const double distance=(-.75-localEyeY)/localRayY;
            return std::array<double,4>{localEyeX+(cosine*dx+sine*dy)*distance,oz-distance,distance,localRayY};
        };
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto p=hit(x,y);const auto at=(std::size_t(y)*width+x)*4;
            check(pixels[at+3]==255,"Native surface changed opacity");
            if(p[3]>=.015) {
                for(unsigned c=0;c<4;++c) check(pixels[at+c]==baseline[at+c],"Native surface crossed the banked sky boundary");
                ++sky;continue;
            }
            if(p[3]>-.02 || p[2]<.2 || p[2]>15 || p[1]>-.3) continue;
            const float row=float(112+.75*512/std::hypot(p[0],p[1]));
            const float fraction=std::clamp((row-112)/111,0.F,1.F);
            std::array<float,3> expected{};
            for(unsigned c=0;c<3;++c) expected[c]=std::lerp(gradient.far[c],gradient.near[c],fraction);
            const float wx=float(p[0])*256+gradient.origin[0],wz=-float(p[1])*256+gradient.origin[1];
            if(material==5 || material==9) {
                // HLSL fine derivatives are the paired 2x2 lane differences.
                // Intersect those rays independently instead of reading the
                // GPU's surface plane or borrowing its projection helpers.
                const auto px=hit(x^1U,y),py=hit(x,y^1U);
                const float footprint=float(std::max(std::hypot(px[0]-p[0],px[1]-p[1]),
                    std::hypot(py[0]-p[0],py[1]-p[1]))*256);
                if(material==5) {
                    // Independent desktop water equations, with analytic
                    // pixel footprint. Do not call the production shader or
                    // obtain UVs/normal/clock from a downloaded guide plane.
                    float u=wx/64,z=wz/64;
                    if(gradient.motion==1) u+=std::sin(z*.17F+seconds)*.6F;
                    if(gradient.motion==2) z+=std::sin(u*.2F+seconds*1.5F)*.8F;
                    const auto band=[&](float frequency) {
                        const float f=footprint/64*frequency;
                        return 1.F/(1+f*f*f*f);
                    };
                    const float wave=std::sin(z*.42F+std::sin(u*.09F)*2-seconds*.8F)*band(.46F);
                    const float glint=std::pow(std::max(0.F,std::sin(u*.035F+wave*.23F)),16.F)*band(.40F);
                    const float ripple=std::sin(u*2.7F+z*1.3F)*std::sin(z*3.1F-u*.9F)*band(6);
                    float detail=wave*.10F+ripple*.025F*std::clamp((row-112)/100,0.F,1.F)+glint*.30F;
                    if(gradient.motion==3) detail+=std::sin(seconds*1.5F+z*.1F)*.08F;
                    const float fade=std::clamp((row-112)/32,0.F,1.F),tint[]{.86F,1.06F,1.18F};
                    for(unsigned c=0;c<3;++c) expected[c]=std::clamp(expected[c]*(1+(tint[c]-1)*fade+detail*fade),0.F,1.F);
                } else {
                    const auto wave=render::lava_surface(wx,wz,seconds,footprint);
                    const auto molten=render::lava_shade(wave,float(localEyeX-p[0]),float(-localEyeY-.75),float(p[1]-oz));
                    const float radiance[]{molten.r,molten.g,molten.b};
                    float edge=std::clamp((row-112)/8,0.F,1.F);edge=edge*edge*(3-2*edge);
                    for(unsigned c=0;c<3;++c) expected[c]=std::lerp(expected[c],std::clamp(radiance[c]*gradient.brightness,0.F,1.F),edge);
                }
            } else {
                float u=wx/64,z=wz/64;
                if(gradient.motion==1) u+=std::sin(z*.17F+seconds)*.6F;
                if(gradient.motion==2) z+=std::sin(u*.2F+seconds*1.5F)*.8F;
                float detail=material==8?(render::lava_detail::lava_noise(u*.10F,z*.10F)*2-1)*.025F:0;
                if(gradient.motion==3) detail+=std::sin(seconds*1.5F+z*.1F)*.08F;
                for(auto& c:expected) c=std::clamp(c*(1+detail*std::clamp((row-112)/32,0.F,1.F)),0.F,1.F);
            }
            for(unsigned c=0;c<3;++c) {
                const int encoded=int(std::lround(expected[c]*255));
                if(std::abs(int(pixels[at+c])-encoded)>3)
                    throw std::runtime_error("Native ground surface oracle: material="+std::to_string(material)
                        +" bank="+std::to_string(bank)+" eye="+std::to_string(e)+" pixel="+std::to_string(x)+","+std::to_string(y)
                        +" channel="+std::to_string(c)+" actual="+std::to_string(pixels[at+c])+" expected="+std::to_string(encoded));
                changed+=pixels[at+c]!=baseline[at+c];
            }
            ++compared;
        }
        check(render_packets(gpu,scene,std::span(&draw,1),camera)==pixels,"Retained native surface moved during a retry");
        check(render_packets(gpu,scene,std::span(&off,1),camera)==baseline,"Native surface OFF failed exact source restoration");
        vr::DrawPacket ink;ink.geometry.vertices=quad(0,-.15F,-1,.16F,{.2F,.6F,.9F,1});ink.preserve_native_colour=true;
        const render::CalibratedScenePacket overlay{&ink,vr::SceneBlend::opaque,false};
        const std::array on_layers{draw,overlay},off_layers{off,overlay};
        const auto on=render_packets(gpu,scene,on_layers,camera),off_image=render_packets(gpu,scene,off_layers,camera);
        const auto coverage=render_packets(gpu,scene,std::span(&overlay,1),camera);
        // A native foreground remains opaque and unmodified by lava/materials.
        unsigned ink_pixels=0;
        for(unsigned at=0;at<coverage.size();at+=4) if(coverage[at] || coverage[at+1] || coverage[at+2]) {
            for(unsigned c=0;c<4;++c) check(on[at+c]==off_image[at+c],"Ground surface painted protected foreground");
            ++ink_pixels;
        }
        check(ink_pixels>400,"Native surface fixture missed its foreground occluder");
    }
    check(compared>300000 && sky>300000 && changed>300000,"Native surface oracle lacked independent coverage");
    auto valid=original;render::CalibratedGroundGradient gradient;gradient.material=9;
    check(render::apply_calibrated_ground(valid,gradient),"Native lava validation fixture failed");
    for(unsigned word:{8U,9U,11U,12U,13U}) {
        auto invalid=valid;
        invalid.geometry.texels[render::calibrated_ground_offset+word]=word==8?10:word==13?4:
            std::bit_cast<std::uint32_t>(std::numeric_limits<float>::infinity());
        render::CalibratedScenePacket bad{&invalid,vr::SceneBlend::opaque,false};
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload_packets(gpu.command,std::span(&bad,1)),"Malformed native liquid metadata reached the GPU");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    for(bool first:{false,true}) {
        auto invalid=valid;const auto source=invalid.geometry.vertex_view();
        invalid.geometry.vertices.assign(source.begin(),source.end());invalid.geometry.shared_vertices.reset();
        auto& vertex=first?invalid.geometry.vertices.front():invalid.geometry.vertices.back();
        vertex.texture[3]&=~render::calibrated_ground_flag;
        render::CalibratedScenePacket bad{&invalid,vr::SceneBlend::opaque,false};
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.upload_packets(gpu.command,std::span(&bad,1)),"Mixed native ground/source packet selected an incomplete shader variant");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    // The reflected environment uses the same retained surface payload. No
    // original-floor snapshot or screen-space image may substitute for it.
    constexpr unsigned cube_size=16,cube_bytes=cube_size*cube_size*6*4;
    valid.model=identity;
    vr::DrawPacket caster_packet;caster_packet.geometry.vertices=quad(0,0,-2,.4F,{1,1,1,1});
    render::CalibratedScenePacket environment{&valid,vr::SceneBlend::opaque,false};environment.reflection_environment=true;
    render::CalibratedScenePacket caster{&caster_packet};caster.ray_caster=true;
    const std::array cube_draws{environment,caster};
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.fov={-.7F,.7F,.7F,-.7F};
    const auto camera=camera_for(eye,d3d);
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload_packets(gpu.command,cube_draws),scene.status().c_str());
    const auto ray=scene.enqueue_ray_geometry(gpu.command,0,8,8,camera,256,cube_size,{0,0,0,1});
    check(ray.complete && ray.environment_face_size==cube_size,scene.status().c_str());
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,cube_bytes,0};
    auto* read=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(read,SDL_GetError());
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUBufferRegion region{static_cast<SDL_GPUBuffer*>(ray.buffer),ray.material_offset+ray.environment_offset,cube_bytes};
    const SDL_GPUTransferBufferLocation target{read,0};SDL_DownloadFromGPUBuffer(copy,&region,&target);SDL_EndGPUCopyPass(copy);
    (void)gpu.finish();
    const auto* cube=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read,false));check(cube,SDL_GetError());
    const auto cube_hit=[](unsigned x,unsigned y) {
        const double u=(x+.5)/cube_size*2-1,v=(y+.5)/cube_size*2-1;
        const double distance=-.75/v;
        return std::array<double,2>{u*distance,-distance};
    };
    for(unsigned y=0;y<cube_size/2;++y) for(unsigned x=0;x<cube_size;++x) {
        const auto p=cube_hit(x,y),px=cube_hit(x^1U,y),py=cube_hit(x,y^1U);
        const float footprint=float(std::max(std::hypot(px[0]-p[0],px[1]-p[1]),std::hypot(py[0]-p[0],py[1]-p[1]))*256);
        const auto wave=render::lava_surface(float(p[0])*256,-float(p[1])*256,0,footprint);
        const auto molten=render::lava_shade(wave,-float(p[0]),-.75F,float(p[1]));
        const float radiance[]{molten.r,molten.g,molten.b};
        const auto at=(4*cube_size*cube_size+y*cube_size+x)*4;
        for(unsigned c=0;c<3;++c) {
            const int expected=int(std::lround(std::clamp(radiance[c],0.F,1.F)*255));
            check(std::abs(int(cube[at+c])-expected)<=3,"Reflection environment substituted an original/flat lava floor");
        }
        check(cube[at+3]==255,"Native lava environment changed opacity");
    }
    SDL_UnmapGPUTransferBuffer(gpu.device,read);SDL_ReleaseGPUTransferBuffer(gpu.device,read);
    std::cout<<"Native world-space surfaces: "<<compared<<" independent rays, "<<sky
        <<" protected sky pixels, water/red sand/pulse/lava, per-eye view/footprint, retained clock, opacity/OFF/validation and enhanced reflection environment passed.\n";
}
// Exercise every optional ground entry on real graphics pipelines, including
// sample-frequency MRTs. Common wrappers must not alter ground radiance.
void native_ground_surface_binding_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
    ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
    for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
    for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
    auto plain=vr::landscape_sphere_packet(ppu,{},112,false,false,232);vr::place_landscape_ground(plain,-.75F,true);
    XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.pose.position.x=.025F;eye.fov={-.65F,.72F,.57F,-.52F};
    auto camera=camera_for(eye,d3d);camera.projection[10]=-1;camera.projection[14]=-.05F;
    render::GpuCalibratedMsaa transport;check(transport.initialize(gpu.device,gpu.format),transport.status().c_str());
    struct ExtraImages {
        SDL_GPUDevice* device;std::vector<SDL_GPUTexture*> textures;
        ~ExtraImages(){SDL_WaitForGPUIdle(device);for(auto* image:textures) SDL_ReleaseGPUTexture(device,image);}
        SDL_GPUTexture* create(SDL_GPUTextureCreateInfo info) {
            auto* image=SDL_CreateGPUTexture(device,&info);check(image,SDL_GetError());textures.push_back(image);return image;
        }
    };
    for(unsigned material:{5U,9U}) for(unsigned samples:{1U,2U,4U,8U}) {
        auto liquid=plain;
        render::CalibratedGroundGradient gradient;gradient.material=material;gradient.seconds=3.25F;gradient.origin={41,-57};
        gradient.far={.12F,.4F,.65F};gradient.near={.03F,.1F,.2F};
        check(render::apply_calibrated_ground(liquid,gradient),"Liquid binding fixture did not create a receiver");
        render::CalibratedScenePacket draw{&liquid,vr::SceneBlend::opaque,false};draw.effect_layer=1;
        NativeMsaaGuideTargets targets(gpu,samples==1?2:samples);ExtraImages extra{gpu.device};
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=width;info.height=height;
        info.layer_count_or_depth=info.num_levels=1;info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        auto* depth=samples==1?extra.create(info):targets.depth;
        info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        auto* sample_motion=extra.create(info);
        info.sample_count=samples==1?SDL_GPU_SAMPLECOUNT_1:samples==2?SDL_GPU_SAMPLECOUNT_2:
            samples==4?SDL_GPU_SAMPLECOUNT_4:SDL_GPU_SAMPLECOUNT_8;
        const auto props=SDL_CreateProperties();check(props,SDL_GetError());
        check(SDL_SetBooleanProperty(props,STARFOX_SDL_MULTISAMPLE_READ,samples>1),SDL_GetError());info.props=props;
        auto* motion=extra.create(info);SDL_DestroyProperties(props);
        auto* color=samples==1?gpu.color:targets.color;
        auto* receiver=samples==1?targets.receivers[0]:targets.receiver;
        auto* surface=samples==1?targets.surfaces[0]:targets.surface;
        std::vector<unsigned char> reference;
        for(unsigned kind=0;kind<4;++kind) {
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(scene.upload_packets(gpu.command,std::span(&draw,1)),scene.status().c_str());
            const std::array<render::CalibratedSceneMotionDraw,1> old{};
            const render::CalibratedSceneMotion previous{camera,old,width,height};
            const render::CalibratedSceneMultisample msaa{samples, samples>1 && kind==0?gpu.color:nullptr,samples>1 && kind!=0};
            const bool encoded=scene.enqueue_eye(gpu.command,color,depth,width,height,camera,{0,0,0,1},
                kind?render::CalibratedScenePhase::before_rays:render::CalibratedScenePhase::all,
                kind?receiver:nullptr,0,kind>=2?surface:nullptr,false,msaa,
                kind==3?motion:nullptr,kind==3?&previous:nullptr);
            check(encoded,scene.status().c_str());
            if(samples>1 && kind) {
                const bool extracted=transport.enqueue_sample(gpu.command,color,receiver,kind>=2?surface:nullptr,
                    gpu.color,targets.receivers[0],kind>=2?targets.surfaces[0]:nullptr,width,height,samples,0,
                    kind==3?motion:nullptr,kind==3?sample_motion:nullptr);
                check(extracted,transport.status().c_str());
            }
            const auto image=gpu.finish();
            if(kind==(samples==1?0:1)) reference=image;
            else if(kind) check(image==reference,"Ground MRT wrapper changed its same-sample radiance");
            unsigned liquid_pixels=0;
            for(unsigned at=0;at<image.size();at+=4) {
                check(image[at+3]==255,"Ground sample entry changed opacity");
                liquid_pixels+=material==9?image[at]>image[at+1]:image[at+2]>image[at];
            }
            check(liquid_pixels>10000,"Ground entry discarded its actual liquid floor");
        }
    }
    std::cout<<"Native water/lava bindings: colour/receiver/surface/motion x 1/2/4/8 samples; exact same-sample radiance and opacity passed.\n";
}
