// Shared stereo source storage and owned uploads are separate identities.
// Reuse only owned bytes in the current recording; never seed owned storage
// from a borrowed source. Compare each queued draw to independent Software.
void check_shared_model_memo(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr unsigned width=96,height=72,steps=16;
    GpuModel model;GpuModelSourcePool pool;
    unsigned images=0;std::uint64_t avoided=0;
    for(unsigned mode=0;mode<3;++mode) for(unsigned scale:{1U,2U,4U}) {
        starfox::assets::Shape shape;shape.name="shared-owned-memo";
        shape.vertices={{-16,-10,0},{16,-10,0},{0,14,0}};
        shape.faces={{-1,0,{0,0,1},{2,1,0}}};shape.colour_words={0x4000};
        starfox::assets::TextureImage texture;texture.descriptor=0x4000;texture.u_mask=texture.v_mask=3;
        texture.coordinates={{{0,0},{3,0},{3,3},{0,3}}};
        texture.texels={1,2,0,4,5,0,7,8,9,10,11,0,0,13,14,15};shape.textures={texture};
        RenderPose pose;pose.z=200;pose.vanish_x=width/2;pose.vanish_y=height/2;
        pose.continuous_geometry=pose.subpixel_projection=mode!=0;
        pose.use_rotation_matrix=mode!=2;
        pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
        RenderSettings settings;settings.render_scale=scale;
        const PreparedProjectionSource projection(shape,pose,settings);
        const PreparedBspSource topology(shape,false);
        const std::array<GpuModelSourceRequest,1> requests{{{&projection,&topology}}};
        require(pool.prepare(device,requests));
        const unsigned pixels=width*height*scale*scale;
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,pixels*4*steps,0};
        auto* download=SDL_CreateGPUTransferBuffer(device,&info);require(download);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        require(pool.enqueue(command));model.begin_upload_batch();
        std::vector<std::vector<std::uint32_t>> expected;
        std::uint64_t shared_bytes=0;
        for(unsigned step=0;step<steps;++step) {
            if(step==4) {pose.x=2.25;pose.y=-1.125;}
            if(step==7) {pose.simple_scaled_sprite=true;pose.simple_sprite_world_size=32;}
            if(step==8) pose.simple_scaled_sprite=false;
            if(step==9) shape.textures[0].texels[1]=8; // Same pointer, changed artwork.
            if(step==13) pool.end_recording();
            if(step==15) {model.end_upload_batch();model.begin_upload_batch();}
            const bool shared=step>=2 && step<13 && step!=5 && step!=6 && step!=7;
            const bool metadata=step!=11;
            const auto output=model.enqueue(device,command,shape,pose,settings,width,height,metadata,
                nullptr,nullptr,false,nullptr,nullptr,{},{},nullptr,8,nullptr,0,false,&topology,&projection,
                shared?&pool.packets()[0]:nullptr);
            if(!output.pixels) throw std::runtime_error(model.status());
            const auto stats=model.upload_info();
            const auto check=[&](bool value,const char* message) {
                if(!value) throw std::runtime_error("shared memo mode="+std::to_string(mode)+" scale="+std::to_string(scale)
                    +" step="+std::to_string(step)+": "+message);
            };
            check(stats.snapshot_bytes<=1024U*1024,"snapshot budget");
            const auto storage_input=stats.input_bytes-stats.pose_uniform_input_bytes;
            check(stats.uploaded_bytes<=storage_input-stats.shared_bytes,"owned upload accounting");
            if(stats.pose_uniform_input_bytes) check(stats.pose_uniform_pushes==1 && stats.pose_uniform_bytes>=stats.pose_uniform_input_bytes,"command-owned pose constants");
            if(shared) {check(stats.shared_buffers==4 && stats.shared_bytes>0,"expected borrowed source");shared_bytes=stats.shared_bytes;}
            else check(stats.shared_bytes==0,"unexpected borrowed source");
            if(step==0 || step==15) check(stats.uploaded_bytes==storage_input,"cold scope must upload all owned storage inputs");
            if(step==1 || step==2 || step==3 || step==6 || step==10 || step==11 || step==12 || step==14)
                check(stats.uploaded_bytes==0,"unchanged owned inputs should remain resident");
            if(step==4 || step==9) check(stats.uploaded_bytes+stats.pose_uniform_input_bytes>0 && stats.uploaded_bytes<storage_input-stats.shared_bytes,
                "pose/artwork mutation must update changed owned slots or command constants");
            if(step==5 || step==13) check(stats.uploaded_bytes==shared_bytes,"borrowed -> owned must restore every borrowed slot");
            if(step==8) check(stats.uploaded_bytes==storage_input-stats.shared_bytes,"billboard overwrite must invalidate owned snapshot");
            avoided+=storage_input-stats.shared_bytes-stats.uploaded_bytes;
            Framebuffer cpu(width,height,scale);cpu.enable_layer_tags(true);cpu.begin_write_coverage();
            SurfaceBuffer surfaces(width*scale,height*scale);
            SoftwareRenderer(settings).draw(shape,pose,cpu,false,metadata?&surfaces:nullptr);
            std::vector<std::uint32_t> frame(pixels);
            for(unsigned i=0;i<pixels;++i) frame[i]=cpu.pixels()[i]|(unsigned(cpu.layer_tags()[i])<<8)
                |(cpu.write_coverage()[i]?0x04000000U:0)
                |(metadata && surfaces.samples()[i].valid?(unsigned(surfaces.samples()[i].palette_index)<<16)|0x01000000U:0);
            expected.push_back(std::move(frame));
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,pixels*4};
            SDL_GPUTransferBufferLocation to{download,step*pixels*4};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        }
        model.end_upload_batch();pool.end_recording();
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* actual=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,download,false));require(actual);
        for(unsigned step=0;step<steps;++step) for(unsigned i=0;i<pixels;++i) {
            if((actual[step*pixels+i]&0x05ffffffU)!=expected[step][i])
                throw std::runtime_error("shared memo image mode="+std::to_string(mode)+" scale="+std::to_string(scale)
                    +" step="+std::to_string(step)+" pixel="+std::to_string(i));
        }
        images+=steps;SDL_UnmapGPUTransferBuffer(device,download);SDL_ReleaseGPUTransferBuffer(device,download);
        // Begin a new source/owned scope on a cancelled command. A later scope
        // cannot assume those unsubmitted input writes ever reached the GPU.
        require(pool.prepare(device,requests));command=SDL_AcquireGPUCommandBuffer(device);require(command);
        require(pool.enqueue(command));model.begin_upload_batch();
        const auto cancelled=model.enqueue(device,command,shape,pose,settings,width,height,false,
            nullptr,nullptr,false,nullptr,nullptr,{},{},nullptr,8,nullptr,0,false,&topology,&projection,&pool.packets()[0]);
        require(cancelled.pixels && model.upload_info().uploaded_bytes>0);
        model.end_upload_batch();pool.end_recording();require(SDL_CancelGPUCommandBuffer(command));
        command=SDL_AcquireGPUCommandBuffer(device);require(command);model.begin_upload_batch();
        require(model.enqueue(device,command,shape,pose,settings,width,height,false).pixels);
        require(model.upload_info().uploaded_bytes==model.upload_info().input_bytes-model.upload_info().pose_uniform_input_bytes);
        model.end_upload_batch();require(SDL_CancelGPUCommandBuffer(command));
    }
    model.release_device();pool.release_device();require(avoided>0 && model.upload_info().snapshot_bytes==0);
    std::cout<<"Shared/owned model memo: "<<images<<" independent Software images; "<<avoided
        <<" owned bytes avoided; native/matrix/Euler, 1x/2x/4x, source ownership switches, pose/artwork mutation, "
          "billboard overwrite, metadata changes, cold scopes and cancellation recovery pass\n";
}
