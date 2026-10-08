// Integer scheduling oracle only; none of these authored diagnostic buffers
// enters a real optical/colour chain or becomes history/expected render input.
void exercise_optical_schedule(Gpu& gpu) {
#if defined(STARFOX_SOURCE_SCHEDULE_CHECK_AVAILABLE)
    const bool spirv=(SDL_GetGPUShaderFormats(gpu.device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
    SDL_GPUComputePipelineCreateInfo info{};info.entrypoint="feature_optical_schedule_main";
    info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
    info.code=spirv?native_reflection_index_optical_schedule_spirv:native_reflection_index_optical_schedule_dxil;
    info.code_size=spirv?sizeof(native_reflection_index_optical_schedule_spirv):sizeof(native_reflection_index_optical_schedule_dxil);
    info.num_readonly_storage_buffers=1;info.num_readwrite_storage_buffers=3;info.num_uniform_buffers=1;
    info.threadcount_x=128;info.threadcount_y=info.threadcount_z=1;
    auto* pipeline=create_gpu_compute_pipeline(gpu.device,&info);check(pipeline,SDL_GetError());
    struct Release {Gpu& gpu;SDL_GPUComputePipeline* pipeline;~Release(){SDL_ReleaseGPUComputePipeline(gpu.device,pipeline);}} release{gpu,pipeline};
    constexpr unsigned queries=64,capacity=128,regionWords=6160/4,resultWords=4096/4,sentinel=0xa5c37e19;
    for(unsigned qcount:{1U,2U,63U,64U})for(unsigned pattern=0;pattern<4;++pattern) {
        Words regions(queries*regionWords,sentinel),expected_results(queries*resultWords,sentinel);
        Words expected_map(queries*capacity*2,sentinel);
        unsigned total=0;
        for(unsigned q=0;q<qcount;++q) {
            const unsigned counts[]{0,1,2,127,128,129,UINT32_MAX};
            const unsigned count=pattern==0?0:pattern==1?128:counts[q%7];
            const unsigned status=pattern==3?32:pattern==2 && q%5==4?17:0;
            regions[q*regionWords]=count;regions[q*regionWords+1]=status;
            for(unsigned r=0;r<capacity;++r) {
                const auto at=q*resultWords+r*8;
                std::fill_n(expected_results.begin()+at,8,0U);expected_results[at+1]=status;
            }
            if(!status)for(unsigned r=0;r<std::min(count,capacity);++r) {
                expected_map[total*2]=q;expected_map[total*2+1]=r;++total;
            }
        }
        Buffer input(gpu,regions),results(gpu,Words(queries*resultWords,sentinel)),map(gpu,Words(queries*capacity*2,sentinel));
        Buffer args(gpu,Words(3,sentinel),SDL_GPU_BUFFERUSAGE_INDIRECT);
        auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
        SDL_GPUStorageBufferReadWriteBinding outputs[]{
            {results.value,false,0,0,0},{map.value,false,0,0,0},{args.value,false,0,0,0}};
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,outputs,3);check(pass,SDL_GetError());
        SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,&input.value,1);
        const std::array<unsigned,4> uniform{qcount,0,0,0};
        SDL_PushGPUComputeUniformData(command,0,uniform.data(),sizeof(uniform));
        SDL_DispatchGPUCompute(pass,qcount,1,1);SDL_EndGPUComputePass(pass);gpu.submit(command);
        check(gpu.read(results.value,unsigned(expected_results.size()*4))==expected_results,
            "Optical schedule changed an empty/refused/default result or a query outside the batch");
        check(gpu.read(map.value,unsigned(expected_map.size()*4))==expected_map,
            "Optical schedule lost/duplicated/reordered an active region or wrote beyond its prefix");
        check(gpu.read(args.value,12)==Words{total,1,1},
            "Optical indirect count included inactive/refused regions or lost capacity overflow refusal work");
    }
    std::cout<<"Native integer optical schedule PASS: batch1/2/63/64, empty/refused/mixed/full counts, capacity overflow, exact defaults/map/indirect bounds.\n";
#else
    (void)gpu;check(false,"Native optical scheduling oracle not compiled");
#endif
}
