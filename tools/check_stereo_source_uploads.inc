void check_stereo_source_uploads(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr unsigned width=96,height=72,count=width*height;
    struct Resources {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* download{};SDL_GPUCommandBuffer* command{};
        ~Resources(){if(command) SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);}
    } owned{device};
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,count*4*3,0};
    owned.download=SDL_CreateGPUTransferBuffer(device,&transfer);require(owned.download);
    GpuModelSourcePool pool;GpuModel model;
    starfox::assets::Shape shape;
    shape.vertices={{-18,-12,0},{18,-12,0},{0,16,0}};
    shape.faces={{-1,0,{0,0,1},{2,1,0}}};shape.colour_words={0x11};
    starfox::assets::TextureImage texture;texture.descriptor=0x4000;texture.u_mask=texture.v_mask=1;
    texture.coordinates={{{0,0},{1,0},{0,1},{1,1}}};texture.texels={0,1,2,3};shape.textures={texture};
    unsigned images=0;std::uint64_t avoided=0;
    for(const bool share_faces:{false,true}) for(unsigned mode=0;mode<3;++mode) for(unsigned round=0;round<5;++round) {
        RenderPose pose;pose.z=200;pose.vanish_x=width/2;pose.vanish_y=height/2;
        pose.force_colour=true;pose.forced_colour=round%2?0x11:0;
        if(share_faces) {
            shape.colour_words[0]=round%2?0x3e01:0x4000;pose.force_colour=false;
            pose.has_depth_colour_tables=true;pose.depth_colour_tables[0][1]=std::uint8_t(0x11+round);
            pose.texture_scroll_x=int(round)-2;pose.texture_scroll_y=2-int(round);
            shape.textures[0].texels[2]=std::uint8_t(2+round);
        }
        pose.continuous_geometry=pose.subpixel_projection=mode!=0;
        pose.use_rotation_matrix=mode!=2;
        pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
        pose.scale=.75+round*.05;pose.x=int(round)-2;
        shape.vertices[0].x=-18-int(round)*2;
        if(round%2) shape.faces.push_back(shape.faces[0]);
        else if(shape.faces.size()>1) shape.faces.pop_back();
        const PreparedProjectionSource projection(shape,pose,{});
        const PreparedBspSource topology(shape,false);
        std::optional<PreparedFacesSource> faces;
        if(share_faces) faces.emplace(shape,topology,pose,RenderSettings{});
        const std::array<GpuModelSourceRequest,2> requests{{{&projection,&topology,nullptr,faces?&*faces:nullptr},
            {&projection,&topology,nullptr,faces?&*faces:nullptr}}};
        require(!pool.prepare(device,requests,1) && pool.packets().empty());
        require(pool.prepare(device,requests) && pool.packets().size()==2);
        require(pool.upload_info().source_storage_bytes<=16U*1024*1024);
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
        const auto draw=[&](unsigned slot,const GpuPreparedModelSource* source) {
            const auto output=model.enqueue(device,owned.command,shape,pose,{},width,height,false,
                nullptr,nullptr,false,nullptr,nullptr,{},{},nullptr,8,nullptr,0,false,&topology,&projection,source,
                source && faces?&*faces:nullptr);
            if(!output.pixels) throw std::runtime_error(model.status());
            auto* copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,count*4};
            SDL_GPUTransferBufferLocation to{owned.download,slot*count*4};
            SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
            return model.upload_info();
        };
        // Unencoded packets are deliberately unusable, not valid stale data.
        const auto unready=draw(0,&pool.packets()[0]);require(unready.shared_bytes==0);
        require(pool.enqueue(owned.command) && !pool.enqueue(owned.command));
        const auto shared=draw(1,&pool.packets()[1]);
        require(shared.shared_buffers==(share_faces?8U:4U) && shared.shared_bytes>0 && shared.uploaded_bytes<unready.uploaded_bytes);
        require(pool.upload_info().uploaded_buffers==(share_faces?8U:4U)); // Duplicate requests upload each source once.
        avoided+=unready.uploaded_bytes-shared.uploaded_bytes;
        pool.end_recording();require(pool.packets().empty());
        const auto ordinary=draw(2,nullptr);require(ordinary.shared_bytes==0 && ordinary.uploaded_bytes==unready.uploaded_bytes);
        auto* submitted=owned.command;owned.command=nullptr;
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* data=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(data);
        Framebuffer expected(width,height);SoftwareRenderer{}.draw(shape,pose,expected,false);
        for(unsigned slot=0;slot<3;++slot) for(unsigned pixel=0;pixel<count;++pixel) {
            require(data[slot*count+pixel]==data[pixel]);
            require((data[slot*count+pixel]&255U)==expected.pixels()[pixel]);
        }
        images+=3;SDL_UnmapGPUTransferBuffer(device,owned.download);
        // Cancel an uploaded packet, destroy its source identities, and retry
        // later at identical addresses. No submitted-data cache is consulted.
        require(pool.prepare(device,requests));owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
        require(pool.enqueue(owned.command));pool.end_recording();
        require(SDL_CancelGPUCommandBuffer(owned.command));owned.command=nullptr;
    }
    // Tiny payloads may still create excessive driver allocations. The handle
    // cap rejects before allocation/encoding and preserves the ordinary path.
    std::deque<PreparedProjectionSource> many_sources;
    std::vector<GpuModelSourceRequest> many_requests;
    RenderPose source_pose;source_pose.z=200;source_pose.continuous_geometry=true;
    const PreparedBspSource shared_topology(shape,false);
    for(unsigned i=0;i<1025;++i) {
        many_sources.emplace_back(shape,source_pose,RenderSettings{});
        many_requests.push_back({&many_sources.back(),&shared_topology});
    }
    require(!pool.prepare(device,many_requests) && pool.packets().empty() && pool.upload_info().uploaded_bytes==0);
    require(pool.upload_info().source_storage_bytes<=16U*1024*1024);
    pool.release_device();require(pool.upload_info().source_storage_bytes==0);
    std::cout<<"Stereo source uploads: "<<images<<" independent Software images; "<<avoided
        <<" model upload bytes avoided; native/continuous/Euler sources, ordinary face/material/UV/texture sources, source mutation, bounded pool, budget fallback, "
          "handle-cap fallback, unready packets, duplicate requests, ordinary recovery and uploaded cancellation passed\n";
}
