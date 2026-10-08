void native_aa_ownership_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    struct Resources {
        SDL_GPUDevice* device;SDL_GPUTexture *receiver{},*surface{};SDL_GPUTransferBuffer* read{};
        ~Resources() {SDL_WaitForGPUIdle(device);for(auto* t:{receiver,surface}) if(t) SDL_ReleaseGPUTexture(device,t);
            if(read) SDL_ReleaseGPUTransferBuffer(device,read);}
    } r{gpu.device};
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=width;info.height=height;
    info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;r.receiver=SDL_CreateGPUTexture(gpu.device,&info);
    info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;r.surface=SDL_CreateGPUTexture(gpu.device,&info);
    const SDL_GPUTransferBufferCreateInfo read{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*4,0};
    r.read=SDL_CreateGPUTransferBuffer(gpu.device,&read);check(r.receiver && r.surface && r.read,SDL_GetError());
    auto solid=quad(0,0,-2,.65F,{.3F,.5F,.7F,1});
    auto textured=quad(-.18F,-.2F,-1.5F,.15F,{1,1,1,1});
    const std::array<std::uint32_t,4> texels{0xff2040ff,0,0xff60d090,0xff60d090};
    for(auto& v:textured) {v.texture[1]=v.texture[2]=1;v.texture[3]=1;v.uv[0]=(v.position[0]+.33F)*6;v.uv[1]=(v.position[1]+.35F)*6;}
    auto emissive=quad(.12F,-.12F,-1.3F,.08F,{1,0,0,1});
    auto hud=quad(.21F,.15F,-1.2F,.06F,{1,1,0,1});
    auto sky=quad(-.3F,.25F,-1.1F,.07F,{0,1,0,1});
    std::array<render::CalibratedSceneDraw,5> draws{{{solid},{textured,texels},{emissive},{hud},{sky}}};
    draws[0].effect_layer=draws[1].effect_layer=2;draws[2].preserve_native_colour=true;
    draws[3].preserve_native_colour=true;draws[3].after_rays=true;draws[4].effect_layer=1;
    unsigned compared=0,eligible=0,protected_ink=0;
    const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    const auto authored=[&](double v) {if(srgb) v=v<=.0031308?12.92*v:1.055*std::pow(v,1./2.4)-.055;return int(std::lround(v*255));};
    for(unsigned eye=0;eye<2;++eye) for(bool with_depth:{false,true}) {
        XrView pose{XR_TYPE_VIEW};pose.pose.orientation.w=1;pose.pose.position.x=eye?.04F:-.03F;pose.fov={-.65F,.75F,.55F,-.5F};
        const auto camera=camera_for(pose,d3d);const auto baseline=render(gpu,scene,draws,camera,{.02F,.07F,.11F,1});
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::before_rays,nullptr,0,nullptr,true),
            "Native AA accepted no ownership target");
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{.02F,.07F,.11F,1},
            render::CalibratedScenePhase::before_rays,r.receiver,0,with_depth?r.surface:nullptr,true),scene.status().c_str());
        (void)gpu.finish();check(scene.notify_submitted(token),"Native AA ownership upload lost confirmation");
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::after_rays,r.receiver,token,
            with_depth?r.surface:nullptr,true),scene.status().c_str());
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        const SDL_GPUTextureRegion from{r.receiver,0,0,0,0,0,width,height,1};const SDL_GPUTextureTransferInfo to{r.read,0,width,height};
        SDL_DownloadFromGPUTexture(copy,&from,&to);SDL_EndGPUCopyPass(copy);const auto actual=gpu.finish();
        check(actual==baseline,"Opt-in AA ownership changed source raster/MRT colour");
        const auto* tags=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,r.read,false));check(tags,SDL_GetError());
        for(unsigned at=0;at<actual.size();at+=4) {
            // Independent visibility/color oracle for this deliberately flat
            // scene. Texture holes expose eligible underlying geometry.
            const bool model=std::abs(int(actual[at])-authored(.3))<=1 && std::abs(int(actual[at+1])-authored(.5))<=1 && std::abs(int(actual[at+2])-authored(.7))<=1;
            check((tags[at+3]==255)==model,"Native AA ownership filtered art or missed visible flat models");
            check(tags[at+3]==0 || tags[at+3]==255,"Native AA ownership became fractional");
            ++compared;if(model) ++eligible;else ++protected_ink;
        }
        SDL_UnmapGPUTransferBuffer(gpu.device,r.read);
    }
    check(eligible && protected_ink,"Native AA mask fixture lacks eligible/protected regions");
    std::cout<<"Native AA raster ownership format "<<unsigned(gpu.format)<<": "<<compared<<" exact visibility checks, "<<eligible
        <<" flat models; protected texture holes/ink/sky, independent eyes, optional depth MRT and exact colour passed.\n";
}
