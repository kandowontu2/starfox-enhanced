// Component-only diagnostic. Game owners never allocate, submit or accept
// these separate 112-byte records; transfers/waits are confined to this tool.
#if defined(STARFOX_CURVED_CACHE_TRACE_AVAILABLE)
struct CurvedCacheTrace {
    Gpu& gpu;SDL_GPUComputePipeline* pipelines[2]{};SDL_GPUSampler* sampler{};
    explicit CurvedCacheTrace(Gpu& g):gpu(g) {}
    ~CurvedCacheTrace() {
        for(auto* pipeline:pipelines)if(pipeline)SDL_ReleaseGPUComputePipeline(gpu.device,pipeline);
        if(sampler)SDL_ReleaseGPUSampler(gpu.device,sampler);
    }
    Bytes run(unsigned tight,const render::shadows::GpuReflectionOutput& rays,void* accepted,
        SDL_GPUTexture* owner,const render::CalibratedReflectionLobeGeometry& geometry) {
        check(tight<2 && !gpu.command && accepted && geometry.valid(),"Invalid bounded curved trace input");
        check(std::string_view(SDL_GetGPUDeviceDriver(gpu.device))=="direct3d12","Curved trace currently requires D3D12");
        if(!pipelines[tight]) {
            SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="reflection_curved_trace_main";
            info.format=SDL_GPU_SHADERFORMAT_DXIL;info.num_samplers=info.num_readwrite_storage_buffers=info.num_uniform_buffers=1;
            info.num_readonly_storage_buffers=3;info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            info.code=tight?calibrated_curved_trace_1_dxil:calibrated_curved_trace_0_dxil;
            info.code_size=tight?sizeof(calibrated_curved_trace_1_dxil):sizeof(calibrated_curved_trace_0_dxil);
            pipelines[tight]=SDL_CreateGPUComputePipeline(gpu.device,&info);check(pipelines[tight],SDL_GetError());
        }
        if(!sampler) {
            SDL_GPUSamplerCreateInfo info{};info.min_filter=info.mag_filter=SDL_GPU_FILTER_NEAREST;
            info.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            info.address_mode_u=info.address_mode_v=info.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampler=SDL_CreateGPUSampler(gpu.device,&info);check(sampler,SDL_GetError());
        }
        const auto& old=geometry.history;const unsigned lobes=old.model_lobes;
        const auto bytes=std::uint64_t(rays.width)*rays.height*lobes*112;
        check(bytes && bytes<=16*1024*1024,"Trace exceeded its component-only bound");
        const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,unsigned(bytes),0};
        auto* output=SDL_CreateGPUBuffer(gpu.device,&info);check(output,SDL_GetError());
        struct OutputScope {SDL_GPUDevice* device;SDL_GPUBuffer* buffer;~OutputScope(){SDL_ReleaseGPUBuffer(device,buffer);}} release{gpu.device,output};
        std::array<unsigned,112> u{}; // EXACT same 448-byte uniform ABI as consumer.
        const auto f=[&](unsigned at,double value){u[at]=std::bit_cast<unsigned>(float(value));};
        u[0]=u[2]=rays.width;u[1]=u[3]=rays.height;u[4]=lobes;u[5]=1;
        u[6]=old.previous_vertex_offset;u[7]=old.previous_index_offset;
        u[8]=u[20]=geometry.vertex_count/3;u[9]=geometry.bytes;f(10,.85);f(11,geometry.roughness);
        for(unsigned c=0;c<4;++c)f(12+c,old.projection[c]);f(16,old.near_plane);f(17,old.far_plane);
        u[22]=rays.reflection_history.identity_offset/(rays.width*rays.height);u[23]=unsigned(old.curved_receivers);
        for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){f(24+r*4+c,geometry.current_cube[r*3+c]);f(36+r*4+c,geometry.previous_cube[r*3+c]);}
        const auto liquid=render::shadows::reflection_liquid_frame_words(old);
        for(unsigned c=0;c<32;++c)u[64+c]=std::bit_cast<unsigned>(liquid[c]);
        for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c)f(96+r*4+c,geometry.current_liquid->world_to_view[r*3+c]);f(99+r*4,geometry.current_liquid->offset[r]);}
        for(unsigned c=0;c<4;++c)f(108+c,geometry.current_projection[c]);
        gpu.begin();const SDL_GPUStorageBufferReadWriteBinding target{output,false,0,0,0};
        auto* pass=SDL_BeginGPUComputePass(gpu.command,nullptr,0,&target,1);check(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipelines[tight]);
        const SDL_GPUTextureSamplerBinding ownership{owner,sampler};SDL_BindGPUComputeSamplers(pass,0,&ownership,1);
        SDL_GPUBuffer* bindings[]{static_cast<SDL_GPUBuffer*>(rays.buffer),static_cast<SDL_GPUBuffer*>(accepted),static_cast<SDL_GPUBuffer*>(geometry.buffer)};
        SDL_BindGPUComputeStorageBuffers(pass,0,bindings,3);SDL_PushGPUComputeUniformData(gpu.command,0,u.data(),sizeof(u));
        SDL_DispatchGPUCompute(pass,(rays.width+7)/8,(rays.height+7)/8,1);SDL_EndGPUComputePass(pass);gpu.submit();
        auto download=rays;download.buffer=output;download.reflection_history.storage_bytes=unsigned(bytes);
        return read_reflection_history(gpu,download);
    }
};
#endif
