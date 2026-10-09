// Exercise every pixel across the old 65535-group ceiling, not just allocation.
void check_wide_motion(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr Uint32 w=8192,h=512,n=w*h,bytes=n*4;
    SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes,0};
    auto* depth=SDL_CreateGPUBuffer(device,&bi);auto* pixels=SDL_CreateGPUBuffer(device,&bi);
    require(depth && pixels,"wide motion inputs");
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes*2,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&ti);ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=n*48;
    auto* read=SDL_CreateGPUTransferBuffer(device,&ti);require(upload && read,"wide motion transfers");
    auto* input=static_cast<Uint32*>(SDL_MapGPUTransferBuffer(device,upload,false));require(input,"wide motion upload");
    const auto z_at=[](Uint32 i){return i%997?100.f+float(i%5)*4.f:0.f;};
    for(Uint32 i=0;i<n;++i) {
        const float z=z_at(i);std::memcpy(input+i,&z,4);
        input[n+i]=(1U<<26)|(unsigned(PixelLayer::three_d)<<8)|(i%251+1);
    }
    SDL_UnmapGPUTransferBuffer(device,upload);
    GpuProjection projection;GpuScene separate,integrated;
    float largest_error=0;
    for(bool reset:{false,true}) {
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"wide motion command");
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,"wide motion copy");
        SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{depth,0,bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);from.offset=bytes;to.buffer=pixels;
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        GpuProjection::MotionSurfaceSettings s;s.width=w;s.height=h;s.reset_history=reset;
        s.current_projection[0]=1024;s.current_projection[1]=768;
        s.current_projection[2]=4096;s.current_projection[3]=256;
        std::copy_n(s.current_projection,4,s.previous_projection);
        s.previous_row0[3]=1;s.previous_row1[3]=-2;s.jitter_x=.375f;s.jitter_y=-.125f;
        const auto motion=projection.enqueue_motion_surface(device,command,depth,s);require(motion,projection.status().c_str());
        auto invalid=s;invalid.width=8193;
        require(!projection.enqueue_motion_surface(device,command,depth,invalid),"wide motion axis budget ignored");
        invalid=s;invalid.height=2049;
        require(!projection.enqueue_motion_surface(device,command,depth,invalid),"wide motion pixel budget ignored");
        GpuRasterOutput source{device,pixels,nullptr,w,h};source.geometry_depth=depth;source.motion=motion;
        const auto reference=separate.enqueue(command,source);require(reference.pixels && reference.motion,separate.status().c_str());
        source.motion=nullptr;
        const auto inline_result=integrated.enqueue(command,source,nullptr,false,false,nullptr,0,0,&s);
        require(inline_result.pixels && inline_result.motion,integrated.status().c_str());
        copy=SDL_BeginGPUCopyPass(command);require(copy,"wide motion download");
        Uint32 offset=0;
        for(const auto& output:{reference,inline_result}) {
            const std::array<void*,3> buffers{output.pixels,output.geometry_depth,output.motion};
            const std::array<Uint32,3> sizes{bytes,bytes,bytes*4};
            for(unsigned j=0;j<3;++j) {
                require(buffers[j],"wide motion missing plane");
                to={static_cast<SDL_GPUBuffer*>(buffers[j]),0,sizes[j]};
                SDL_GPUTransferBufferLocation destination{read,offset};SDL_DownloadFromGPUBuffer(copy,&to,&destination);
                offset+=sizes[j];
            }
        }
        SDL_EndGPUCopyPass(copy);auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"wide motion submit");
        require(SDL_WaitForGPUFences(device,true,&fence,1),"wide motion wait");SDL_ReleaseGPUFence(device,fence);
        const auto* result=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,read,false));require(result,"wide motion map");
        require(std::memcmp(result,result+n*6,n*24)==0,"wide inline/separate color/depth/motion mismatch");
        const auto* result_depth=reinterpret_cast<const float*>(result+n);
        const auto* result_motion=reinterpret_cast<const std::array<float,4>*>(result+n*2);
        for(Uint32 i=0;i<n;++i) {
            const float z=z_at(i);const bool valid=z>0 && !reset;
            const Uint32 expected=(1U<<26)|(unsigned(PixelLayer::three_d)<<8)|(i%251+1);
            require(result[i]==expected && result_depth[i]==z,"wide scene missing/repeated color or depth");
            const auto& m=result_motion[i];require(m[3]==float(valid) && m[2]==(valid?z:0),"wide motion validity/depth");
            if(valid) {
                const float error=std::max(std::abs(m[0]-1024.f/z),std::abs(m[1]+1536.f/z));
                largest_error=std::max(largest_error,error);
                require(error<=4*std::numeric_limits<float>::epsilon()*w,"wide analytical motion differs");
            } else require(m[0]==0 && m[1]==0,"wide invalid/reset motion not zero");
        }
        SDL_UnmapGPUTransferBuffer(device,read);
    }
    projection.release_device();separate.release_device();integrated.release_device();
    SDL_ReleaseGPUBuffer(device,depth);SDL_ReleaseGPUBuffer(device,pixels);
    SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,read);
    std::cout<<"Wide motion/scene dispatch: "<<n*2U<<" independently checked pixels across 65535 groups, exact inline/separate planes, max motion error="<<largest_error<<" passed\n";
}
