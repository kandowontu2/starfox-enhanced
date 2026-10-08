// Exercise GpuClip's real host dispatch around SDL's 65535-group X limit.
// Most descriptors are invalid/empty; independently known live sentinels at
// the last two slots detect missing, duplicated or wrapped second-row groups.
void check_cooperative_span_dispatch(SDL_GPUDevice* device) {
    using starfox::render::RasterCommand;
    struct Environment {
        const char* keys[4]{"STARFOX_TEST_COOPERATIVE_SPAN_TRACE","STARFOX_TEST_FULL_SPAN_TRACE",
            "STARFOX_TEST_FULL_SPAN_CLEAR","STARFOX_TEST_PARALLEL_SPAN_CLEAR"};
        std::array<std::optional<std::string>,4> saved;
        Environment() {for(unsigned i=0;i<4;++i)if(const auto* value=SDL_getenv(keys[i]))saved[i]=value;}
        static void set(const char* key,const char* value) {
            if(value) require(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),key,value,true));
            else require(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),key));
        }
        ~Environment() {for(unsigned i=0;i<4;++i) {
            if(saved[i]) SDL_SetEnvironmentVariable(SDL_GetEnvironment(),keys[i],saved[i]->c_str(),true);
            else SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),keys[i]);
        }}
        void select(unsigned path) {
            set(keys[0],path?"1":nullptr);set(keys[1],path!=1?"1":nullptr);
            set(keys[2],"1");set(keys[3],nullptr);
        }
    } environment;
    constexpr unsigned maximum=65536,width=96,height=1;
    const std::array<V,4> points{{{10,0,256,1},{10,1,256,1},{70,1,256,1},{70,0,256,1}}};
    const std::array<V,4> corners{{{0,0,0,0},{1,0,7,0},{2,7,7,0},{3,7,0,0}}};
    const std::array<unsigned,1> visibility{1};
    std::vector<V> descriptors(maximum);
    for(unsigned i:{0U,65534U,65535U}) descriptors[i]={0,4,0,0};
    std::vector<RasterCommand> materials(maximum);
    for(unsigned i=0;i<maximum;++i) {
        materials[i].even=17+i%3;materials[i].odd=34+i%3;materials[i].tag=2;
        materials[i].has_surface=1;materials[i].surface={.25f,.5f,.75f,1};
    }
    struct Owner {
        SDL_GPUDevice* device;
        std::array<SDL_GPUBuffer*,5> buffers{};
        SDL_GPUTransferBuffer *upload{},*download{};
        ~Owner() {
            SDL_WaitForGPUIdle(device);
            for(auto* buffer:buffers) if(buffer)SDL_ReleaseGPUBuffer(device,buffer);
            if(upload)SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download)SDL_ReleaseGPUTransferBuffer(device,download);
        }
    } owner{device};
    const unsigned sizes[]{sizeof(points),sizeof(corners),unsigned(descriptors.size()*sizeof(V)),
        sizeof(visibility),unsigned(materials.size()*sizeof(RasterCommand))};
    std::array<unsigned,5> offsets{};unsigned total=0;
    for(unsigned i=0;i<5;++i) {
        offsets[i]=total;total+=sizes[i];
        SDL_GPUBufferCreateInfo info{};info.size=sizes[i];
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        owner.buffers[i]=SDL_CreateGPUBuffer(device,&info);require(owner.buffers[i]);
    }
    const unsigned poison_offset=total,span_bytes=maximum*sizeof(RasterCommand);
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total+span_bytes,0};
    owner.upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(owner.upload);
    transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=span_bytes;
    owner.download=SDL_CreateGPUTransferBuffer(device,&transfer);require(owner.download);
    starfox::render::GpuClip clips[3];
    unsigned cases=0,live=0;std::uint64_t compared=0;
    for(unsigned count:{65534U,65535U,65536U}) for(unsigned path=0;path<3;++path) {
        environment.select(path);
        auto* uploaded=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device,owner.upload,true));require(uploaded);
        const void* sources[]{points.data(),corners.data(),descriptors.data(),visibility.data(),materials.data()};
        for(unsigned i=0;i<5;++i)std::memcpy(uploaded+offsets[i],sources[i],sizes[i]);
        std::memset(uploaded+poison_offset,0xcd,span_bytes);
        SDL_UnmapGPUTransferBuffer(device,owner.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned i=0;i<5;++i) {
            SDL_GPUTransferBufferLocation source{owner.upload,offsets[i]};
            SDL_GPUBufferRegion target{owner.buffers[i],0,sizes[i]};
            SDL_UploadToGPUBuffer(copy,&source,&target,true);
        }
        SDL_EndGPUCopyPass(copy);
        starfox::render::NativeClipSettings settings{count,4,4,1,width,height};
        if(!clips[path].enqueue(device,command,owner.buffers[0],owner.buffers[1],owner.buffers[2],owner.buffers[3],settings))
            throw std::runtime_error(clips[path].status());
        auto* spans=static_cast<SDL_GPUBuffer*>(clips[path].enqueue_spans(command,owner.buffers[4]));require(spans);
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation seed{owner.upload,poison_offset};
        SDL_GPUBufferRegion target{spans,0,unsigned(count*sizeof(RasterCommand))};
        SDL_UploadToGPUBuffer(copy,&seed,&target,false);SDL_EndGPUCopyPass(copy);
        if(clips[path].enqueue_spans(command,owner.buffers[4],false,1,nullptr,1,nullptr,0,nullptr,{},true)!=spans)
            throw std::runtime_error("Wide span dispatch did not reuse poisoned output");
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation destination{owner.download,0};
        SDL_DownloadFromGPUBuffer(copy,&target,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* output=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,owner.download,false));require(output);
        bool exact=true;
        for(unsigned i=0;i<count;++i) {
            RasterCommand expected{};
            if(i==0 || i>=65534) {
                expected=materials[i];expected.left=10;expected.right=71;expected.bottom=1;
                expected.u=10;expected.v=70;expected.reserved0=1;++live;
            }
            exact &= std::memcmp(output+i*sizeof(expected),&expected,sizeof(expected))==0;
        }
        SDL_UnmapGPUTransferBuffer(device,owner.download);
        if(!exact) throw std::runtime_error("Wide cooperative dispatch differs from independent sentinel/clear oracle");
        ++cases;compared+=count*sizeof(RasterCommand);
    }
    if(cases!=9 || live!=18)throw std::runtime_error("Wide dispatch fixture omitted a route or live boundary sentinel");
    std::cout<<"Cooperative span dispatch: "<<cases<<" reference/candidate/override cases at 65534/65535/65536 slots, "
        <<live<<" independent live sentinels and "<<compared<<" fully checked poisoned command bytes\n";
}
