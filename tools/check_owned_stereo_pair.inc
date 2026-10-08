// Verify joint ownership independently of the display-format packer. The split
// path is a same-shader oracle for all guide bits; SoftwareRenderer separately
// establishes the eye projection/painter coverage. Only this test reads back.
void check_owned_stereo_pair(SDL_GPUDevice* device,bool parallel=false) {
    using namespace starfox::render;
    auto* environment=SDL_GetEnvironment();
    struct Restore {
        SDL_Environment* env;
        std::array<const char*,7> names{"STARFOX_TEST_JOINED_STEREO_SUBMISSIONS",
            "STARFOX_TEST_SPLIT_STEREO_SUBMISSIONS","STARFOX_TEST_FAIL_STEREO_ENCODING_AFTER_LEFT",
            "STARFOX_TEST_PARALLEL_STEREO_ENCODING","STARFOX_TEST_SERIAL_STEREO_ENCODING",
            "STARFOX_TEST_FAIL_STEREO_ENCODING_RIGHT","STARFOX_TEST_DUPLICATE_STEREO_SOURCE_UPLOADS"};
        std::array<std::optional<std::string>,7> saved;
        explicit Restore(SDL_Environment* value):env(value) {
            for(unsigned i=0;i<names.size();++i)
                if(const auto* text=SDL_GetEnvironmentVariable(env,names[i])) saved[i]=text;
        }
        ~Restore(){for(unsigned i=0;i<names.size();++i) {
            if(saved[i]) SDL_SetEnvironmentVariable(env,names[i],saved[i]->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,names[i]);
        }}
    } restore{environment};
    if(parallel) {
        require(SDL_UnsetEnvironmentVariable(environment,restore.names[0]));
        require(SDL_SetEnvironmentVariable(environment,restore.names[3],"1",true));
    } else require(SDL_SetEnvironmentVariable(environment,restore.names[0],"1",true));
    require(SDL_UnsetEnvironmentVariable(environment,restore.names[1]));
    require(SDL_UnsetEnvironmentVariable(environment,restore.names[2]));
    require(SDL_UnsetEnvironmentVariable(environment,restore.names[4]));
    require(SDL_UnsetEnvironmentVariable(environment,restore.names[5]));
    struct Transfer {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer{};
        ~Transfer(){SDL_WaitForGPUIdle(device);if(buffer) SDL_ReleaseGPUTransferBuffer(device,buffer);}
    } transfer{device};
    constexpr unsigned max_width=128,height=72,max_count=max_width*height;
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,max_count*40*2,0};
    transfer.buffer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer.buffer);
    starfox::assets::Shape shape;
    shape.vertices={{-14,-10,0},{14,-10,0},{0,14,0}};
    shape.faces={{-1,0,{0,0,1},{2,1,0}}};shape.colour_words={0x11};
    GpuModelDraw model;model.shape=&shape;model.pose.z=180;
    model.pose.force_colour=true;model.pose.forced_colour=0x11;
    model.surface_metadata=true;model.geometry_depth=true;
    model.pose.continuous_geometry=model.pose.subpixel_projection=true;
    model.pose.vanish_y=height/2;
    GpuStereoScene joint,split;
    std::uint64_t verified=0;unsigned images=0;
    const auto capture=[&](GpuStereoScene& owner,unsigned width,bool metadata=true) {
        const unsigned count=width*height,stride=count*(metadata?40:4);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned eye=0;eye<2;++eye) {
            const auto output=owner.resident_output(eye);require(output.pixels);
            const std::array<void*,4> buffers{output.pixels,output.surfaces,output.geometry_depth,output.motion};
            const std::array<unsigned,4> bytes{count*4,count*16,count*4,count*16};
            unsigned offset=eye*stride;
            for(unsigned field=0;field<(metadata?4U:1U);++field) {
                require(buffers[field]);
                SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(buffers[field]),0,bytes[field]};
                SDL_GPUTransferBufferLocation target{transfer.buffer,offset};
                SDL_DownloadFromGPUBuffer(copy,&source,&target);offset+=bytes[field];
            }
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* mapped=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,transfer.buffer,false));require(mapped);
        std::vector<std::uint8_t> result(mapped,mapped+stride*2);
        SDL_UnmapGPUTransferBuffer(device,transfer.buffer);return result;
    };
    for(unsigned iteration=0;iteration<10;++iteration) {
        const unsigned width=iteration%3?96:128,count=width*height;
        model.pose.vanish_x=width/2;
        model.pose.x=int(iteration)*2-8;
        model.pose.z=iteration%2?180:260;
        model.pose.forced_colour=iteration%4?0x11:0;
        model.previous_pose=model.pose;model.previous_pose->x-=2;
        const std::array<GpuSceneDraw,1> frame{model};
        require(SDL_SetEnvironmentVariable(environment,restore.names[1],"1",true));
        require(SDL_SetEnvironmentVariable(environment,restore.names[4],"1",true));
        require(SDL_SetEnvironmentVariable(environment,restore.names[6],"1",true));
        require(split.render_resident(device,width,height,frame,6.4,200));
        const auto duplicate_uploads=split.model_upload_info();
        require(duplicate_uploads.shared_bytes==0 && duplicate_uploads.uploaded_bytes==duplicate_uploads.input_bytes-duplicate_uploads.pose_uniform_input_bytes);
        const auto expected=capture(split,width);
        require(SDL_UnsetEnvironmentVariable(environment,restore.names[1]));
        require(SDL_UnsetEnvironmentVariable(environment,restore.names[4]));
        require(SDL_UnsetEnvironmentVariable(environment,restore.names[6]));
        require(joint.render_resident(device,width,height,frame,6.4,200));
        const auto shared_uploads=joint.model_upload_info();
        // Four geometry descriptors plus four ordinary face inputs per eye.
        require(shared_uploads.shared_bytes>0 && shared_uploads.shared_buffers==16);
        require(shared_uploads.uploaded_bytes<duplicate_uploads.uploaded_bytes);
        require(shared_uploads.source_storage_bytes<=16U*1024*1024);
        const auto actual=capture(joint,width);
        require(actual==expected);verified+=actual.size();images+=2;
        for(unsigned eye=0;eye<2;++eye) {
            const auto projected=stereo_scene_eye(frame,eye,6.4,200);require(projected.has_value());
            const auto& draw=std::get<GpuModelDraw>((*projected)[0]);
            Framebuffer reference(width,height);reference.enable_layer_tags(true);reference.begin_write_coverage();
            SoftwareRenderer(draw.settings).draw(shape,draw.pose,reference,false);
            unsigned coverage=0;
            for(unsigned pixel=0;pixel<count;++pixel) {
                std::uint32_t word;
                std::memcpy(&word,actual.data()+eye*count*40+pixel*4,4);
                require((word&255U)==reference.pixels()[pixel]);
                require(bool(word&0x04000000U)==bool(reference.write_coverage()[pixel]));
                coverage+=(word&0x04000000U)!=0;
            }
            require(coverage>20);
        }
        // A cancelled second eye must invalidate the whole pair and every
        // published ray/guide output; retry uploads and renders the changed pose.
        require(SDL_SetEnvironmentVariable(environment,restore.names[2],"1",true));
        require(!joint.render_resident(device,width,height,frame,6.4,200));
        for(unsigned eye=0;eye<2;++eye)
            require(!joint.resident_output(eye).pixels && !joint.ray_geometry_output(eye).complete);
        require(SDL_UnsetEnvironmentVariable(environment,restore.names[2]));
        require(joint.render_resident(device,width,height,frame,6.4,200));
        require(capture(joint,width)==expected);
        if(parallel) {
            require(SDL_SetEnvironmentVariable(environment,restore.names[5],"1",true));
            require(!joint.render_resident(device,width,height,frame,6.4,200));
            for(unsigned eye=0;eye<2;++eye)
                require(!joint.resident_output(eye).pixels && !joint.ray_geometry_output(eye).complete);
            require(SDL_UnsetEnvironmentVariable(environment,restore.names[5]));
            require(joint.render_resident(device,width,height,frame,6.4,200));
            require(capture(joint,width)==expected);
            // Switching this SAME owner back to main-thread recording must
            // retire its worker submission and retain the exact guides.
            if(iteration==6) {
                require(SDL_SetEnvironmentVariable(environment,restore.names[4],"1",true));
                require(joint.render_resident(device,width,height,frame,6.4,200));
                require(capture(joint,width)==expected);
                require(SDL_UnsetEnvironmentVariable(environment,restore.names[4]));
                require(joint.render_resident(device,width,height,frame,6.4,200));
                require(capture(joint,width)==expected);
            }
            if(iteration==4) joint.release_device(); // Stop/recreate the idle worker.
        }
    }
    // Borrowed enqueue remains forbidden until both owners have retired the
    // shared submission, even if the last diagnostic download already finished.
    const std::array<GpuSceneDraw,1> frame{model};
    auto* borrowed=SDL_AcquireGPUCommandBuffer(device);require(borrowed);
    require(!joint.enqueue(device,borrowed,128,height,frame,6.4,200));SDL_CancelGPUCommandBuffer(borrowed);
    require(joint.wait_for_completion());
    borrowed=SDL_AcquireGPUCommandBuffer(device);require(borrowed);
    require(joint.enqueue(device,borrowed,128,height,frame,6.4,200).has_value());SDL_CancelGPUCommandBuffer(borrowed);
    require(joint.render_resident(device,128,height,{},6.4,200));
    const auto cleared=capture(joint,128,false);
    require(std::all_of(cleared.begin(),cleared.end(),[](auto byte){return byte==0;}));
    // Submit changing pairs and their snapshots without a per-pair wait. The
    // final fence covers all preceding copies on this queue. Reusing either
    // eye's backing too early would make an earlier snapshot show a later pose.
    constexpr unsigned queued_width=96,queued_count=queued_width*height,queued_frames=12;
    RasterCommands queued_underlay;queued_underlay.reset(queued_width,height);
    RasterCommand underlay;underlay.right=queued_width;underlay.bottom=height;
    queued_underlay.commands.push_back(underlay);
    std::vector<std::uint8_t> queued_reference(queued_frames*queued_count*2);
    std::vector<SDL_GPUFence*> snapshots;
    struct RetireSnapshots {
        SDL_GPUDevice* device;std::vector<SDL_GPUFence*>& fences;
        ~RetireSnapshots(){if(!fences.empty()) {
            SDL_WaitForGPUFences(device,true,fences.data(),Uint32(fences.size()));
            for(auto* fence:fences) SDL_ReleaseGPUFence(device,fence);
        }}
    } retire{device,snapshots};
    snapshots.reserve(queued_frames);
    for(unsigned sample=0;sample<queued_frames;++sample) {
        // Change actual source vertices/topology at the SAME shape address,
        // not just the eye pose. Earlier queued snapshots must retain their
        // prior immutable backing, including resize/reallocated source buffers.
        shape.vertices[0].x=-14-int(sample%5);
        if(sample%3==1) shape.faces.push_back(shape.faces[0]);
        else if(shape.faces.size()>1) shape.faces.pop_back();
        model.pose.vanish_x=queued_width/2;model.pose.x=int(sample)*2-10;
        model.pose.forced_colour=0x20+sample;model.previous_pose=model.pose;model.previous_pose->x-=2;
        queued_underlay.commands[0].even=queued_underlay.commands[0].odd=0x40+sample;
        const std::array<GpuSceneDraw,2> queued_frame{GpuRasterDraw{&queued_underlay},model};
        require(joint.render_resident(device,queued_width,height,queued_frame,6.4,200));
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned eye=0;eye<2;++eye) {
            const auto output=joint.resident_output(eye);require(output.pixels);
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,queued_count*4};
            SDL_GPUTransferBufferLocation to{transfer.buffer,(sample*2+eye)*queued_count*4};
            SDL_DownloadFromGPUBuffer(copy,&from,&to);
            const auto projected=stereo_scene_eye(queued_frame,eye,6.4,200);require(projected.has_value());
            const auto& draw=std::get<GpuModelDraw>((*projected)[1]);
            Framebuffer reference(queued_width,height);
            replay_raster_commands(queued_underlay,reference,nullptr);
            SoftwareRenderer(draw.settings).draw(shape,draw.pose,reference,false);
            std::copy(reference.pixels().begin(),reference.pixels().end(),queued_reference.begin()+(sample*2+eye)*queued_count);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);snapshots.push_back(fence);
    }
    require(SDL_WaitForGPUFences(device,true,snapshots.data(),Uint32(snapshots.size())));
    const auto* queued=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,transfer.buffer,false));require(queued);
    for(unsigned pixel=0;pixel<queued_reference.size();++pixel) require((queued[pixel]&255U)==queued_reference[pixel]);
    SDL_UnmapGPUTransferBuffer(device,transfer.buffer);
    joint.release_device();split.release_device();
    require(joint.model_upload_info().shared_bytes==0 && joint.model_upload_info().source_storage_bytes==0
        && joint.model_upload_info().snapshot_bytes==0);
    std::cout<<(parallel?"Parallel":"Joined")<<" stereo ownership: "<<images<<" independent CPU eye images, "<<verified
        <<" exact duplicate-upload color/surface/depth/motion bytes; fewer immutable source copies; resize, covered black ink, partial cancellation/retry, empty clear, borrowed retirement and 12 queued source-mutating pairs passed\n";
    if(parallel) std::cout<<"Worker right-eye failure/retry, same-owner serial/parallel switch and worker release/recreation passed\n";
}
