// Actual source-floor packets and motion matcher -> resident native MRTs.
// Only this fixture downloads images; scalar ray/pose math is independent.
void native_ground_motion_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    struct Resources {
        SDL_GPUDevice* device;SDL_GPUTexture *owner{},*surface{},*motion{};SDL_GPUTransferBuffer* read{};
        ~Resources(){SDL_WaitForGPUIdle(device);for(auto* t:{owner,surface,motion})if(t)SDL_ReleaseGPUTexture(device,t);
            if(read)SDL_ReleaseGPUTransferBuffer(device,read);}
    } r{gpu.device};
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=width;info.height=height;
    info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;r.owner=SDL_CreateGPUTexture(gpu.device,&info);
    info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    r.surface=SDL_CreateGPUTexture(gpu.device,&info);r.motion=SDL_CreateGPUTexture(gpu.device,&info);
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*36,0};
    r.read=SDL_CreateGPUTransferBuffer(gpu.device,&transfer);check(r.owner && r.surface && r.motion && r.read,SDL_GetError());
    simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;ppu.bg2_screen_size=2;
    ppu.bg2_screen_base=0x1000;ppu.bg2_character_base=0;ppu.cgram[1]=31;
    for(unsigned y=0;y<8;++y)ppu.vram[32+y*2]=255;
    for(unsigned tile=0;tile<2048;++tile)ppu.vram[0x2000+tile*2]=1;
    const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
        || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    auto floor=vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);vr::place_landscape_ground(floor,-.75F,true);
    auto snapshot=std::make_shared<vr::GameSceneSnapshot>();snapshot->scene_epoch=17;snapshot->background_id=3;
    std::uint64_t checked=0,protected_pixels=0,rejected=0,distant=0,canonical_colours=0;
    for(unsigned material:{0U,8U,5U,6U,7U,9U,100U})for(unsigned e=0;e<2;++e) {
        render::CalibratedGameFrame old;old.previous=old.current=snapshot;
        old.draws.push_back({floor,render::CalibratedGameLayer::world,vr::SceneBlend::opaque,false,render::calibrated_ground_source_key});
        if(material!=100) {
            render::CalibratedGroundGradient gradient{{.31F,.72F,.21F},{.04F,.19F,.035F},112,111};
            gradient.material=material;gradient.origin={123.25F,-415.5F};gradient.seconds=1.375F;
            check(render::apply_calibrated_ground(old.draws[0].packet,gradient),"Ground motion fixture metadata failed");
        }
        MotionModel then,now;then.angle=-.07;then.scale={.95,1.13,.99};then.offset={.02,-.01,.03};
        now.angle=.12;now.scale={1.1,.86,1.07};now.offset={.08,-.02,-.12};
        old.draws[0].packet.model=then.matrix();auto current=old;current.draws[0].packet.model=now.matrix();
        auto& data=current.draws[0].packet.geometry.texels;
        data[render::calibrated_ground_offset-1]=std::bit_cast<std::uint32_t>(-.95F);
        if(material!=100) {
            data[render::calibrated_ground_offset+9]=std::bit_cast<std::uint32_t>(187.25F);
            data[render::calibrated_ground_offset+10]=std::bit_cast<std::uint32_t>(-287.5F);
            data[render::calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(2.25F);
        }
        XrView eye{XR_TYPE_VIEW};eye.pose.position={e?.072F:-.043F,.027F,.04F};eye.fov={-.64F,.72F,.57F,-.52F};
        eye.pose.orientation={0,float(std::sin(.02)),0,float(std::cos(.02))};
        auto previous_eye=eye;previous_eye.pose.position.x+=.035F;previous_eye.pose.position.y-=.016F;previous_eye.pose.position.z+=.071F;
        previous_eye.pose.orientation={0,float(std::sin(-.0125)),0,float(std::cos(-.0125))};previous_eye.fov={-.61F,.69F,.53F,-.49F};
        const std::array<double,2> jitter{-.31,.27},old_jitter{.19,-.13};
        auto camera=camera_for(eye,d3d),before=camera_for(previous_eye,d3d);
        // The authored sphere/far floor encloses the eye at infinity. A finite
        // 20-metre fixture far plane clips it before any floor guides exist.
        camera.projection[10]=before.projection[10]=-1;
        camera.projection[14]=before.projection[14]=-.05F;
        camera.projection[8]-=float(2*jitter[0]/width);camera.projection[9]+=float(2*jitter[1]/height);
        before.projection[8]-=float(2*old_jitter[0]/width);before.projection[9]+=float(2*old_jitter[1]/height);
        const auto draws=current.packets();const auto baseline=render_packets(gpu,scene,draws,camera);
        if(material==0 || material==8) {
            // Reference retains the old raw ABI values above. Compare the
            // normalized producer payload against it on the actual GPU, not
            // against another invocation of the normalization helper.
            auto canonical=current;auto& words=canonical.draws.front().packet.geometry.texels;
            words[render::calibrated_ground_offset+11]=0;
            if(material==0) words[render::calibrated_ground_offset+9]=words[render::calibrated_ground_offset+10]=0;
            const auto normalized=render_packets(gpu,scene,canonical.packets(),camera);
            check(normalized==baseline,"Static metadata canonicalization changed native ground colour/alpha");
            canonical_colours+=baseline.size();
        }
        const auto prior=render::calibrated_game_motion_draws(current,&old,camera,before);
        check(prior.size()==1 && prior[0].valid==(material==100 || material==0 || material==8),"Ground matcher accepted fluid/radiance or rejected finite static floor");
        render::CalibratedSceneMotion history{before,prior,width,height};
        const auto encode=[&] {
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            const bool uploaded=scene.upload_packets(gpu.command,draws);check(uploaded,scene.status().c_str());const auto token=scene.upload_token();
            const bool encoded=scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{0,0,0,1},render::CalibratedScenePhase::before_rays,
                r.owner,0,r.surface,true,{},r.motion,&history);check(encoded,scene.status().c_str());
            auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
            SDL_GPUTextureTransferInfo target{r.read,0,width,height};SDL_GPUTextureRegion source{r.owner,0,0,0,0,0,width,height,1};
            SDL_DownloadFromGPUTexture(copy,&source,&target);source.texture=r.surface;target.offset=width*height*4;
            SDL_DownloadFromGPUTexture(copy,&source,&target);source.texture=r.motion;target.offset=width*height*20;
            SDL_DownloadFromGPUTexture(copy,&source,&target);SDL_EndGPUCopyPass(copy);
            check(gpu.finish()==baseline,"Native floor motion MRT changed colour/opacity");check(scene.notify_submitted(token),"Ground motion upload retirement lost");
            const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,r.read,false));check(bytes,SDL_GetError());
            std::vector<unsigned char> output(bytes,bytes+width*height*36);SDL_UnmapGPUTransferBuffer(gpu.device,r.read);return output;
        };
        const auto measured=encode();check(encode()==measured,"Held native floor motion jittered");unsigned coverage=0,coloured=0;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned i=y*width+x;std::array<float,4> surface{},motion{};
            coloured+=baseline[i*4]!=0 || baseline[i*4+1]!=0 || baseline[i*4+2]!=0;
            std::memcpy(surface.data(),measured.data()+width*height*4+i*16,16);
            std::memcpy(motion.data(),measured.data()+width*height*20+i*16,16);
            if(surface[3]<=0) {check(motion==std::array<float,4>{},"Infinite sky inherited finite-floor motion");++protected_pixels;continue;}
            ++coverage;check(measured[i*4+2]==1,"Floor guides lost world ownership");
            if(!prior[0].valid) {check(motion==std::array<float,4>{},"Liquid/reflected radiance borrowed rigid floor flow");++rejected;continue;}
            const double rx=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5-jitter[0])/width);
            const double ry=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5-jitter[1])/height);
            const auto origin=now.local({eye.pose.position.x,eye.pose.position.y,eye.pose.position.z});
            const auto direction=now.local(motion_rotate(eye.pose.orientation,{rx,ry,-1}),true);
            const double distance=(-.95-origin[1])/direction[1];std::array<double,3> point{};
            for(unsigned c=0;c<3;++c)point[c]=origin[c]+direction[c]*distance;
            point[1]+=.2;if(material==8){point[0]+=.25;point[2]-=.5;}
            const auto previous_world=then.world(point);auto relative=previous_world;
            relative[0]-=previous_eye.pose.position.x;relative[1]-=previous_eye.pose.position.y;relative[2]-=previous_eye.pose.position.z;
            const auto old_eye=motion_rotate(previous_eye.pose.orientation,relative,true);
            if(old_eye[2]>=0){check(motion==std::array<float,4>{},"Behind-eye floor point retained motion");continue;}
            // At the subpixel horizon, float matrix coefficients and the
            // independent double pose produce ill-conditioned intersections
            // hundreds of metres away. Still check their finite/valid guide,
            // but measure subpixel correspondence on well-conditioned rays.
            if(distance>15 || -old_eye[2]>20) {
                check(motion[3]==1 && std::all_of(motion.begin(),motion.end(),[](float v){return std::isfinite(v);}),
                    "Distant finite floor lost a finite accepted guide");++distant;continue;
            }
            const auto pixel=reference(previous_eye,previous_world);
            const double expected[4]{pixel[0]+old_jitter[0]-(x+.5),pixel[1]+old_jitter[1]-(y+.5),-old_eye[2]*256,1};
            if(std::abs(surface[3]-distance*256)>=.09) throw std::runtime_error("Finite floor depth mismatch: material="
                +std::to_string(material)+" eye="+std::to_string(e)+" xy="+std::to_string(x)+","+std::to_string(y)
                +" actual="+std::to_string(surface[3])+" expected="+std::to_string(distance*256));
            for(unsigned c=0;c<4;++c) {
                if(std::abs(motion[c]-expected[c])>(c==2?.11:.025))throw std::runtime_error("Independent native floor motion mismatch: material="
                    +std::to_string(material)+" eye="+std::to_string(e)+" xy="+std::to_string(x)+","+std::to_string(y)+" channel="+std::to_string(c)
                    +" actual="+std::to_string(motion[c])+" expected="+std::to_string(expected[c]));
                ++checked;
            }
        }
        if(coverage<=500) throw std::runtime_error("Ground motion fixture lacks physical floor coverage: material="
            +std::to_string(material)+" eye="+std::to_string(e)+" pixels="+std::to_string(coverage)
            +" coloured="+std::to_string(coloured)+" layer="+std::to_string(draws[0].effect_layer));
    }
    check(checked>10000 && protected_pixels>10000 && rejected>10000,"Ground motion oracle missed finite/infinite/radiance cases");
    std::cout<<"Native finite-ground motion format "<<unsigned(gpu.format)<<": "<<checked<<" independent components, "
        <<protected_pixels<<" protected sky pixels, "<<rejected<<" fluid/radiance rejections and "<<distant
        <<" finite distant guides; original/gradient/red sand, height/origin changes, "
          "bank/scale, asymmetric tracked eyes, FOV and sample jitter, exact colour/held guides passed; "
        <<canonical_colours<<" byte-exact canonical/raw static colour components.\n";
}
