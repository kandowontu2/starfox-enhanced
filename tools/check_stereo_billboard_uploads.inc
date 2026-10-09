// Independent software pixels plus actual GPU source/depth readbacks. This is
// fixture-only; production never reads, waits for or uploads a screen image.
void check_stereo_billboard_uploads(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr unsigned width=96,height=72,max_count=width*height*16;
    struct Resources {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* download{};SDL_GPUCommandBuffer* command{};
        ~Resources(){if(command) SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);}
    } owned{device};
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,max_count*32,0};
    owned.download=SDL_CreateGPUTransferBuffer(device,&transfer);require(owned.download);
    GpuModelSourcePool pool;GpuModel model;
    starfox::assets::Shape shape;shape.colour_words={0x4000};shape.colour_materials.resize(1);
    shape.colour_materials[0].animation_frames={0x4000,0x4001};shape.textures.resize(2);
    for(unsigned i=0;i<2;++i) {
        auto& t=shape.textures[i];t.descriptor=0x4000+i;t.u_mask=t.v_mask=7;
        t.texels.resize(67+i*4); // Exercise non-word-aligned payloads and padding.
    }
    unsigned images=0,distinct=0;std::uint64_t avoided=0;
    for(unsigned scale:{1U,2U,4U}) for(bool depth:{false,true}) for(unsigned round=0;round<5;++round) {
        for(unsigned i=0;i<2;++i) for(unsigned j=0;j<shape.textures[i].texels.size();++j)
            shape.textures[i].texels[j]=(j+round)%7==0?0:1+(j+round+i*5)%15;
        RenderSettings settings;settings.render_scale=scale;settings.colour_index_base=0x30;
        RenderPose left;left.simple_scaled_sprite=true;left.simple_sprite_world_size=80;
        left.z=200+round*9;left.x=-3;left.y=2;left.vanish_x=width/2;left.vanish_y=height/2;
        left.continuous_geometry=left.subpixel_projection=true;left.colour_frame=round;
        left.effect_clip_left=-8;left.effect_clip_right=width+8;
        if(round==4) left.palette_override=0x3e;
        auto right=left;right.x+=12;right.z+=7;
        const auto* texture=&shape.textures[round%2];
        const auto bytes=unsigned((texture->texels.size()+3)&~std::size_t(3));
        const std::array<GpuModelSourceRequest,3> requests{{{nullptr,nullptr,texture},
            {nullptr,nullptr,texture},{nullptr,nullptr,texture}}};
        require(!pool.prepare(device,requests,bytes-1) && pool.packets().empty());
        require(pool.prepare(device,requests) && pool.packets().size()==3);
        require(pool.upload_info().source_storage_bytes<=16U*1024*1024);
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
        const unsigned count=width*height*scale*scale;
        const auto draw=[&](unsigned slot,const RenderPose& pose,const GpuPreparedModelSource* source) {
            const auto output=model.enqueue(device,owned.command,shape,pose,settings,width,height,false,
                nullptr,nullptr,depth,nullptr,nullptr,{},{},nullptr,8,nullptr,0,false,nullptr,nullptr,source);
            if(!output.pixels) throw std::runtime_error(model.status());
            auto* copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,count*4};
            const SDL_GPUTransferBufferLocation to{owned.download,slot*count*4};
            SDL_DownloadFromGPUBuffer(copy,&from,&to);
            if(depth) {
                require(output.geometry_depth);
                const SDL_GPUBufferRegion z_from{static_cast<SDL_GPUBuffer*>(output.geometry_depth),0,count*4};
                const SDL_GPUTransferBufferLocation z_to{owned.download,(slot+4)*count*4};
                SDL_DownloadFromGPUBuffer(copy,&z_from,&z_to);
            }
            SDL_EndGPUCopyPass(copy);return model.upload_info();
        };
        const auto unready=draw(0,left,&pool.packets()[0]);require(unready.shared_bytes==0);
        require(pool.enqueue(owned.command) && !pool.enqueue(owned.command));
        require(pool.upload_info().uploaded_buffers==1 && pool.upload_info().uploaded_bytes==bytes);
        const auto shared=draw(1,left,&pool.packets()[1]);
        require(shared.shared_buffers==1 && shared.shared_bytes==bytes);
        require(shared.uploaded_bytes==(depth?16U:0U) && shared.uploaded_buffers==(depth?1U:0U));
        require(shared.input_bytes==unready.input_bytes && unready.uploaded_bytes-shared.uploaded_bytes==bytes);
        avoided+=unready.uploaded_bytes-shared.uploaded_bytes;
        // A selected animation texture different from the packet MUST fall back.
        if(round==2) ++right.colour_frame;
        const auto other=draw(2,right,&pool.packets()[2]);
        require(other.shared_bytes==(round==2?0U:bytes));
        pool.end_recording();require(pool.packets().empty());
        const auto ordinary=draw(3,right,nullptr);require(ordinary.shared_bytes==0);
        auto* command=owned.command;owned.command=nullptr;
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        auto* data=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(data);
        Framebuffer expected_left(width,height,scale),expected_right(width,height,scale);
        SoftwareRenderer renderer(settings);renderer.draw(shape,left,expected_left,false);renderer.draw(shape,right,expected_right,false);
        for(unsigned slot=0;slot<4;++slot) for(unsigned pixel=0;pixel<count;++pixel) {
            const auto& expected=slot<2?expected_left:expected_right;
            require((data[slot*count+pixel]&255U)==expected.pixels()[pixel]);
            require(data[slot*count+pixel]==data[(slot<2?0:3)*count+pixel]);
            if(depth) {
                require(data[(slot+4)*count+pixel]==data[((slot<2?0:3)+4)*count+pixel]);
                if((data[slot*count+pixel]&(1U<<26))!=0) {
                    const auto z=std::bit_cast<float>(data[(slot+4)*count+pixel]);
                    require(std::isfinite(z) && std::abs(z-float(slot<2?left.z:right.z))<1.e-3F);
                }
            }
            if(slot==2 && data[slot*count+pixel]!=data[pixel]) ++distinct;
        }
        images+=4;SDL_UnmapGPUTransferBuffer(device,owned.download);
        // Uploaded-but-cancelled versions cannot become a cross-frame cache.
        require(pool.prepare(device,requests));owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
        require(pool.enqueue(owned.command));pool.end_recording();
        require(SDL_CancelGPUCommandBuffer(owned.command));owned.command=nullptr;
    }
    require(distinct>0);
    // Record all four changing pairs before submitting any of them. Each eye
    // must retain its own source backing version, not the final active texture.
    struct Pending {
        SDL_GPUDevice* device;
        std::vector<SDL_GPUCommandBuffer*> commands;
        std::vector<SDL_GPUFence*> fences;
        ~Pending(){for(auto* c:commands) if(c) SDL_CancelGPUCommandBuffer(c);
            if(!fences.empty()) SDL_WaitForGPUFences(device,true,fences.data(),unsigned(fences.size()));
            for(auto* f:fences) SDL_ReleaseGPUFence(device,f);}
    } pending{device};
    std::vector<std::vector<std::uint8_t>> expected_queued;
    constexpr unsigned queued_count=width*height;
    for(unsigned frame=0;frame<4;++frame) {
        auto& texture=shape.textures[0];
        for(unsigned j=0;j<texture.texels.size();++j) texture.texels[j]=(j+frame)%7==0?0:1+(j+frame*3)%15;
        const std::array<GpuModelSourceRequest,1> request{{{nullptr,nullptr,&texture}}};
        require(pool.prepare(device,request));
        for(unsigned eye=0;eye<2;++eye) {
            owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
            if(eye==0) require(pool.enqueue(owned.command));
            RenderPose pose;pose.simple_scaled_sprite=true;pose.simple_sprite_world_size=60;
            pose.z=220+frame*13;pose.x=int(eye)*11-5;pose.y=2;pose.vanish_x=width/2;pose.vanish_y=height/2;
            const auto output=model.enqueue(device,owned.command,shape,pose,{},width,height,false,
                nullptr,nullptr,false,nullptr,nullptr,{},{},nullptr,8,nullptr,0,false,nullptr,nullptr,&pool.packets()[0]);
            require(output.pixels && model.upload_info().shared_buffers==1 && model.upload_info().uploaded_bytes==0);
            auto* copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,queued_count*4};
            const SDL_GPUTransferBufferLocation to{owned.download,(frame*2+eye)*queued_count*4};
            SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
            Framebuffer reference(width,height);SoftwareRenderer{}.draw(shape,pose,reference,false);
            expected_queued.emplace_back(reference.pixels().begin(),reference.pixels().end());
            pending.commands.push_back(owned.command);owned.command=nullptr;
        }
        pool.end_recording();
    }
    require(expected_queued[0]!=expected_queued[2]);
    for(auto*& command:pending.commands) {
        auto* submitted=command;command=nullptr;
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(fence);pending.fences.push_back(fence);
    }
    require(SDL_WaitForGPUFences(device,true,pending.fences.data(),unsigned(pending.fences.size())));
    const auto* queued=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(queued);
    for(unsigned image=0;image<8;++image) for(unsigned pixel=0;pixel<queued_count;++pixel)
        require((queued[image*queued_count+pixel]&255U)==expected_queued[image][pixel]);
    SDL_UnmapGPUTransferBuffer(device,owned.download);images+=8;
    auto malformed=shape.textures[0];malformed.texels.resize(3);
    const std::array<GpuModelSourceRequest,1> bad{{{nullptr,nullptr,&malformed}}};
    require(!pool.prepare(device,bad) && pool.packets().empty());
    pool.release_device();require(pool.upload_info().source_storage_bytes==0);
    std::cout<<"Stereo billboard uploads: "<<images<<" independent Software images, "<<distinct
        <<" distinct-eye pixels, "<<avoided<<" per-draw upload bytes avoided; 1/2/4x, own depth/palette, "
          "animation mismatch, live texel mutation, aligned budget, deduplicated copies, unready/ordinary recovery, "
          "malformed payload, uploaded cancellation and eight queued changing-eye images passed\n";
}
