// Same-recording input reuse must never outlive cancellation or borrow a
// texture upload performed by the independent whole-object billboard path.
void check_model_uploads(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr unsigned width=96,height=72;
    struct Owned {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* download{};SDL_GPUCommandBuffer* command{};
        ~Owned(){if(command) SDL_CancelGPUCommandBuffer(command);
            SDL_WaitForGPUIdle(device);if(download) SDL_ReleaseGPUTransferBuffer(device,download);}
    } owned{device};
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*16*4,0};
    owned.download=SDL_CreateGPUTransferBuffer(device,&transfer);require(owned.download);
    GpuModel model;
    std::uint64_t verified=0,reused=0,avoided=0;unsigned images=0;
    for(unsigned scale:{1U,2U,4U}) {
        RenderSettings settings;settings.render_scale=scale;
        starfox::assets::Shape shape;shape.name="input-reuse";
        shape.vertices={{-16,-10,0},{16,-10,0},{0,14,0}};
        shape.faces={{-1,0,{0,0,1},{2,1,0}}};shape.colour_words={0x4000};
        starfox::assets::TextureImage texture;texture.descriptor=0x4000;texture.u_mask=texture.v_mask=3;
        texture.coordinates={{{0,0},{3,0},{3,3},{0,3}}};
        texture.texels={1,2,0,4,5,0,7,8,9,10,11,0,0,13,14,15};shape.textures={texture};
        RenderPose pose;pose.z=200;pose.vanish_x=width/2;pose.vanish_y=height/2;
        pose.continuous_geometry=pose.subpixel_projection=true;
        const auto draw=[&](bool metadata,bool all_uploads,bool no_uploads) {
            const auto output=model.enqueue(device,owned.command,shape,pose,settings,width,height,metadata);
            if(!output.pixels) throw std::runtime_error(model.status());
            require(bool(output.surfaces)==metadata);
            const auto stats=model.upload_info();require(stats.input_bytes!=0);
            const auto storage_input=stats.input_bytes-stats.pose_uniform_input_bytes;
            require(stats.uploaded_bytes<=storage_input);
            if(all_uploads) require(stats.uploaded_bytes==storage_input && stats.reused_buffers==0);
            if(no_uploads) require(stats.uploaded_bytes==0 && stats.uploaded_buffers==0 && stats.reused_buffers>0);
            if(stats.pose_uniform_input_bytes) require(stats.pose_uniform_pushes==1 && stats.pose_uniform_bytes>=stats.pose_uniform_input_bytes);
            reused+=stats.reused_buffers;avoided+=storage_input-stats.uploaded_bytes;
            auto* copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
            const unsigned pixels=width*height*scale*scale;
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,pixels*4};
            SDL_GPUTransferBufferLocation to{owned.download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);
            SDL_EndGPUCopyPass(copy);
            // Each fixture ends this scope BEFORE submitting. The following
            // fixture deliberately uses a fresh scope, except the multi-draw
            // recording below which captures every borrowed result in order.
            return output;
        };
        const auto reference=[&](bool metadata) {
            Framebuffer cpu(width,height,scale);cpu.enable_layer_tags(true);cpu.begin_write_coverage();
            SurfaceBuffer surfaces(width*scale,height*scale);
            // GpuModel clears storage without claiming an opaque background.
            // Track model writes only, not SoftwareRenderer's optional clear.
            SoftwareRenderer(settings).draw(shape,pose,cpu,false,metadata?&surfaces:nullptr);
            std::vector<std::uint32_t> packed(cpu.pixels().size());
            for(unsigned i=0;i<packed.size();++i) packed[i]=cpu.pixels()[i]|(unsigned(cpu.layer_tags()[i])<<8)
                |(cpu.write_coverage()[i]?0x04000000U:0)
                |(metadata && surfaces.samples()[i].valid?
                    (unsigned(surfaces.samples()[i].palette_index)<<16)|0x01000000U:0);
            return packed;
        };
        const auto finish=[&](const std::vector<std::uint32_t>& expected) {
            auto* submitted=owned.command;owned.command=nullptr;
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(fence);
            require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
            const auto* actual=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(actual);
            for(unsigned i=0;i<expected.size();++i)
                require((actual[i]&0x05ffffffU)==expected[i]);
            SDL_UnmapGPUTransferBuffer(device,owned.download);verified+=expected.size();++images;
        };
        // Capture several successive draws in one recording. Reusing the same
        // host input bytes must not erase a prior draw's borrowed GPU version.
        constexpr unsigned steps=13;
        SDL_GPUTransferBufferCreateInfo sequence_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*scale*scale*4*steps,0};
        auto* sequence=SDL_CreateGPUTransferBuffer(device,&sequence_info);require(sequence);
        std::vector<std::vector<std::uint32_t>> expected;
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);model.begin_upload_batch();
        std::uint64_t snapshot_peak=0;
        for(unsigned step=0;step<steps;++step) {
            if(step==2){pose.x=2.25;pose.y=-1.125;}
            if(step==3) shape.textures[0].texels[1]=8; // Mutate artwork at the same address.
            if(step==4){pose.force_colour=true;pose.forced_colour=0;} // Opaque black.
            if(step==5){pose.force_colour=false;shape.vertices[0].x+=2;}
            if(step==6){pose.simple_scaled_sprite=true;pose.simple_sprite_world_size=32;}
            if(step==7) pose.simple_scaled_sprite=false; // Shared buffer 9 was overwritten.
            if(step==8) shape.faces.push_back(shape.faces[0]); // Same Shape, new input layout.
            if(step==11) shape.faces.pop_back(); // Shrink without freeing retained capacity.
            if(step==12) shape.faces.push_back(shape.faces[0]); // Regrow without allocating again.
            const bool metadata=step!=9;
            // Texture -> forced flat material also changes the upload layout.
            const auto raster=draw(metadata,step==0 || step==4 || step==5 || step==7 || step==8 || step>=11,step==1 || step==9 || step==10);
            const unsigned pixels=raster.width*raster.height;
            const auto stats=model.upload_info();require(stats.snapshot_bytes<=1024U*1024);
            if(step==4 || step>=11) require(stats.snapshot_bytes==snapshot_peak);
            snapshot_peak=std::max(snapshot_peak,stats.snapshot_bytes);
            if(step==2 || step==3) {
                require(stats.uploaded_bytes+stats.pose_uniform_input_bytes>0 && stats.uploaded_bytes<stats.input_bytes-stats.pose_uniform_input_bytes);
            }
            expected.push_back(reference(metadata));
            auto* copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
            // Preserve this borrowed result before another model enqueue reuses it.
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(raster.pixels),0,pixels*4};
            SDL_GPUTransferBufferLocation to{sequence,step*pixels*4};SDL_DownloadFromGPUBuffer(copy,&from,&to);
            SDL_EndGPUCopyPass(copy);
        }
        model.end_upload_batch();auto* submitted=owned.command;owned.command=nullptr;
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* data=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,sequence,false));require(data);
        unsigned captured_step=0;
        for(const auto& frame:expected) {for(unsigned i=0;i<frame.size();++i) {
                if((data[i]&0x05ffffffU)!=frame[i])
                    throw std::runtime_error("Model reuse mismatch: scale="+std::to_string(scale)
                        +" step="+std::to_string(captured_step)+" pixel="+std::to_string(i)
                        +" actual="+std::to_string(data[i]&0x05ffffffU)+" expected="+std::to_string(frame[i]));
            }
            ++captured_step;
            data+=frame.size();verified+=frame.size();++images;}
        SDL_UnmapGPUTransferBuffer(device,sequence);SDL_ReleaseGPUTransferBuffer(device,sequence);
        // Cancel a fully encoded cached recording, then reacquire a command
        // (SDL may recycle its address). The retry MUST upload all inputs.
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);model.begin_upload_batch();
        draw(true,true,false);draw(true,false,true);model.end_upload_batch();
        require(SDL_CancelGPUCommandBuffer(owned.command));owned.command=nullptr;
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);model.begin_upload_batch();
        draw(true,true,false);model.end_upload_batch();finish(reference(true));
        // Outside an explicit recording scope, byte-identical inputs still upload.
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
        draw(false,true,false);finish(reference(false));
    }
    model.release_device();require(model.upload_info().snapshot_bytes==0 && reused>0 && avoided>0);
    std::cout<<"Model input reuse: "<<images<<" images / "<<verified<<" independent Software pixels; "
        <<reused<<" read-only copies and "<<avoided<<" bytes avoided; artwork/pose/material/layout mutations, "
          "billboard interleave, metadata switches, bounded shrink/regrow reuse, cancellation and standalone recovery pass\n";
}
void check_prepared_model_source(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr unsigned width=96,height=72;
    starfox::assets::Shape shape;
    shape.vertices={{-16,-10,0},{16,-10,0},{0,14,0}};
    shape.faces={{-1,0,{0,0,1},{2,1,0}}};shape.colour_words={0x11};
    auto other=shape;
    const PreparedBspSource wrong_source(other,false),wrong_policy(shape,true);
    GpuModelDraw model;model.shape=&shape;model.pose.z=200;
    model.pose.vanish_x=width/2;model.pose.vanish_y=height/2;
    model.pose.continuous_geometry=model.pose.subpixel_projection=true;
    model.pose.force_colour=true;model.pose.forced_colour=0x11;
    const PreparedProjectionSource wrong_projection(other,model.pose,model.settings);
    auto native_pose=model.pose;native_pose.continuous_geometry=native_pose.subpixel_projection=false;
    native_pose.use_rotation_matrix=true;
    const PreparedProjectionSource wrong_projection_mode(shape,native_pose,model.settings);
    GpuScene scene;
    for(const auto* bad:{&wrong_source,&wrong_policy}) {
        model.prepared_topology=bad;
        const std::array<GpuSceneDraw,1> frame{model};
        require(!scene.render_resident(device,width,height,frame) && !scene.resident_output().pixels);
        require(scene.status().find("different source/policy")!=std::string::npos);
    }
    model.prepared_topology=nullptr;
    for(const auto* bad:{&wrong_projection,&wrong_projection_mode}) {
        model.prepared_projection=bad;
        const std::array<GpuSceneDraw,1> frame{model};
        require(!scene.render_resident(device,width,height,frame) && !scene.resident_output().pixels);
        require(scene.status().find("different source/frame/policy")!=std::string::npos);
    }
    for(unsigned mode=0;mode<3;++mode) for(unsigned round=0;round<3;++round) {
        model.pose.continuous_geometry=model.pose.subpixel_projection=mode!=0;
        model.pose.use_rotation_matrix=mode!=2;
        model.pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
        model.pose.scale=.75+round*.125;
        shape.vertices[0].x-=3;shape.faces[0].normal.x=int(round);
        if(round==1) shape.faces.push_back(shape.faces[0]);
        if(round==2) shape.faces.pop_back();
        {
            // CPU source preparation expires before GPU completion/readback.
            // Submitted work must own its uploaded bytes, not this packet.
            const PreparedBspSource source(shape,false);
            const PreparedProjectionSource projection(shape,model.pose,model.settings);
            model.prepared_topology=&source;
            model.prepared_projection=&projection;
            const std::array<GpuSceneDraw,1> frame{model};
            require(scene.render_resident(device,width,height,frame));
        }
        Framebuffer actual(width,height),expected(width,height);
        require(scene.readback(actual,nullptr));
        SoftwareRenderer(model.settings).draw(shape,model.pose,expected,false);
        require(actual.pixels()==expected.pixels());
    }
    std::cout<<"Prepared model source: topology/projection source-policy rejection, native/continuous/Euler mutation and upload-owned retirement passed\n";
}
