// Poison recycled span storage before every emission. Comparing only fresh
// allocations would miss stale live bounds/materials and accidental cycling.
void check_span_clear_reuse(SDL_GPUDevice* device,bool colour_trace=false,bool cooperative_trace=false) {
    using starfox::render::RasterCommand;
    struct Buffers {
        SDL_GPUDevice* device;
        std::vector<SDL_GPUBuffer*> values;
        SDL_GPUTransferBuffer *upload{},*download{};
        ~Buffers() {
            SDL_WaitForGPUIdle(device);
            for(auto* b:values) SDL_ReleaseGPUBuffer(device,b);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        }
        SDL_GPUBuffer* make(unsigned bytes) {
            SDL_GPUBufferCreateInfo info{};info.size=bytes;
            info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
            auto* result=SDL_CreateGPUBuffer(device,&info);require(result);values.push_back(result);return result;
        }
    } buffers{device};
    struct Environment {
        std::optional<std::string> bounds,full,parallel,colour,full_trace,cooperative;
        static std::optional<std::string> get(const char* key) {
            if(const auto* value=std::getenv(key)) return std::string(value);
            return {};
        }
        static void set(const char* key,const char* value) {
            if(value)SDL_SetEnvironmentVariable(SDL_GetEnvironment(),key,value,true);
            else SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),key);
#if defined(_WIN32)
            _putenv_s(key,value?value:"");
#else
            if(value) setenv(key,value,1);else unsetenv(key);
#endif
        }
        Environment():bounds(get("STARFOX_TEST_SPAN_BOUNDS_CLEAR")),full(get("STARFOX_TEST_FULL_SPAN_CLEAR")) {
            if(const auto* value=SDL_getenv("STARFOX_TEST_PARALLEL_SPAN_CLEAR"))parallel=value;
            if(const auto* value=SDL_getenv("STARFOX_TEST_COLOUR_SPAN_TRACE"))colour=value;
            if(const auto* value=SDL_getenv("STARFOX_TEST_FULL_SPAN_TRACE"))full_trace=value;
            if(const auto* value=SDL_getenv("STARFOX_TEST_COOPERATIVE_SPAN_TRACE"))cooperative=value;
        }
        ~Environment() {
            set("STARFOX_TEST_SPAN_BOUNDS_CLEAR",bounds?bounds->c_str():nullptr);
            set("STARFOX_TEST_FULL_SPAN_CLEAR",full?full->c_str():nullptr);
            if(parallel)SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"STARFOX_TEST_PARALLEL_SPAN_CLEAR",parallel->c_str(),true);
            else SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"STARFOX_TEST_PARALLEL_SPAN_CLEAR");
            set("STARFOX_TEST_COLOUR_SPAN_TRACE",colour?colour->c_str():nullptr);
            set("STARFOX_TEST_FULL_SPAN_TRACE",full_trace?full_trace->c_str():nullptr);
            set("STARFOX_TEST_COOPERATIVE_SPAN_TRACE",cooperative?cooperative->c_str():nullptr);
        }
        void select(unsigned path,bool compare_colour=false,bool compare_cooperative=false) {
            if(compare_colour) {
                set("STARFOX_TEST_COLOUR_SPAN_TRACE",path>=4 && !compare_cooperative?"1":nullptr);
                set("STARFOX_TEST_COOPERATIVE_SPAN_TRACE",path>=4 && compare_cooperative?"1":nullptr);
                set("STARFOX_TEST_FULL_SPAN_TRACE",path==8?"1":nullptr);
            }
            path%=4;
            set("STARFOX_TEST_SPAN_BOUNDS_CLEAR",path==1?"1":nullptr);
            set("STARFOX_TEST_FULL_SPAN_CLEAR",path==0?"1":nullptr);
            if(path==2)require(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"STARFOX_TEST_PARALLEL_SPAN_CLEAR","1",true));
            else require(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"STARFOX_TEST_PARALLEL_SPAN_CLEAR"));
        }
    } environment;
    // Use the same full command/mask oracle for both experimental tracers.
    colour_trace=colour_trace || cooperative_trace;
    constexpr unsigned polygons=4,width=96,max_height=256;
    std::array<V,16> points{};std::array<V,16> corners{};std::array<V,4> descriptors{};
    std::array<unsigned,4> visibility{};std::array<RasterCommand,4> materials{};
    std::array<std::uint8_t,64> texels{};
    for(unsigned i=0;i<64;++i) texels[i]=i%7==0?0:std::uint8_t(40+i%15);
    for(unsigned i=0;i<polygons;++i) {
        descriptors[i]={int(i*4),4,int(i),i==2?1:0};
        for(unsigned j=0;j<4;++j) corners[i*4+j]={int(i*4+j),j>=2?7:0,j==1 || j==2?7:0,0};
        auto& m=materials[i];m.even=11+i;m.odd=18+i;m.dither=i&1;m.tag=2+i;
        m.has_surface=i!=1;m.surface={.25f,.5f,.75f,1};m.textured=i==2;
        m.u_mask=m.v_mask=7;m.reserved0=1;m.reserved1=2;
    }
    auto* point_buffer=buffers.make(sizeof(points));auto* corner_buffer=buffers.make(sizeof(corners));
    auto* polygon_buffer=buffers.make(sizeof(descriptors));auto* visibility_buffer=buffers.make(sizeof(visibility));
    auto* material_buffer=buffers.make(sizeof(materials));auto* texel_buffer=buffers.make(sizeof(texels));
    constexpr unsigned max_span_bytes=polygons*max_height*4*sizeof(RasterCommand);
    constexpr unsigned max_pixels=width*max_height*16;
    constexpr unsigned max_mask_bytes=64+polygons*max_height*4*((width*4+31)/32)*4*9;
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,max_span_bytes+4096,0};
    buffers.upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(buffers.upload);
    transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=max_span_bytes+max_pixels*20+max_mask_bytes;
    buffers.download=SDL_CreateGPUTransferBuffer(device,&transfer);require(buffers.download);
    starfox::render::GpuClip clips[9];starfox::render::GpuRaster rasters[9];
    unsigned cases=0,live=0,empty=0,automatic_full=0,automatic_parallel=0;std::uint64_t compared=0;
    for(unsigned height:{64U,max_height}) for(unsigned samples:{0U,2U,4U,8U})
    for(unsigned scale:{1U,2U,4U}) for(unsigned variant=0;variant<(colour_trace?16U:8U);++variant) {
        const unsigned rows=height*scale,span_bytes=polygons*rows*sizeof(RasterCommand),pixels=width*height*scale*scale;
        const bool masks=samples || variant>=4;
        const unsigned mask_bytes=masks?64+polygons*rows*((width*scale+31)/32)*4*(samples+1):0;
        for(auto& material:materials)material.reserved1=masks && material.textured==0?262144U:0;
        if(colour_trace) for(unsigned i=0;i<polygons;++i) if(!materials[i].textured) {
            materials[i].reserved1|=((variant+i)%3)*2U;
            if(variant>=8)materials[i].reserved1|=1U;
            if((variant+i)%5==0)materials[i].reserved1|=65536U;
            if((variant+i)%7==0)materials[i].reserved1|=131072U;
            // Irrelevant solid UVs deliberately exercise wrapped negative and
            // endpoint values in the reference tracer, not only zero UVs.
            for(unsigned j=0;j<4;++j) {
                corners[i*4+j][1]=word(int((variant+1)*7919U+i*3571U+j*16383U));
                corners[i*4+j][2]=word(int((variant+3)*12347U+i*8191U+j*32767U));
            }
        }
        for(unsigned i=0;i<polygons;++i) {
            int x=4+int(i*19),y=3+int(i*9);
            if(variant>=4){x+=int(variant*11);y-=int(variant*6);}
            points[i*4+0]={x,y,256,1};points[i*4+1]={x+23,y,256,1};
            points[i*4+2]={x+20,y+19,256,1};points[i*4+3]={x-3,y+19,256,1};
            visibility[i]=(variant==1 || variant==5)?0:variant==2?unsigned(i==1):1;
        }
        const auto automatic_mode=starfox::render::span_clear_mode(polygons*rows,samples?(mask_bytes-64)/4:0);
        if(automatic_mode==starfox::render::SpanClearMode::parallel)++automatic_parallel;
        else ++automatic_full;
        std::array<std::vector<std::uint8_t>,9> results;
        const unsigned paths=colour_trace?9U:4U;
        for(unsigned path=0;path<paths;++path) {
            environment.select(path,colour_trace,cooperative_trace);
            auto* upload=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device,buffers.upload,true));require(upload);
            unsigned offset=0;
            const auto pack=[&](const auto& value) {const auto begin=offset;std::memcpy(upload+offset,&value,sizeof(value));offset+=sizeof(value);return begin;};
            const auto point_offset=pack(points),corner_offset=pack(corners),polygon_offset=pack(descriptors),visibility_offset=pack(visibility);
            const auto material_offset=pack(materials),texel_offset=pack(texels),poison_offset=offset;
            RasterCommand poison{};poison.left=-123;poison.right=555;poison.top=-77;poison.bottom=444;
            poison.even=0xde;poison.odd=0xad;poison.dither=1;poison.tag=127;poison.textured=0;
            poison.has_surface=1;poison.surface={-9,-8,-7,123};poison.u=987;poison.v=654;
            poison.reserved1=6;
            for(unsigned row=0;row<polygons*rows;++row) std::memcpy(upload+poison_offset+row*sizeof(poison),&poison,sizeof(poison));
            SDL_UnmapGPUTransferBuffer(device,buffers.upload);
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
            for(auto [buffer,from,bytes]:{
                std::tuple{point_buffer,point_offset,unsigned(sizeof(points))},
                std::tuple{corner_buffer,corner_offset,unsigned(sizeof(corners))},
                std::tuple{polygon_buffer,polygon_offset,unsigned(sizeof(descriptors))},
                std::tuple{visibility_buffer,visibility_offset,unsigned(sizeof(visibility))},
                std::tuple{material_buffer,material_offset,unsigned(sizeof(materials))},
                std::tuple{texel_buffer,texel_offset,unsigned(sizeof(texels))}}) {
                SDL_GPUTransferBufferLocation source{buffers.upload,from};SDL_GPUBufferRegion target{buffer,0,bytes};
                SDL_UploadToGPUBuffer(copy,&source,&target,true);
            }
            SDL_EndGPUCopyPass(copy);
            starfox::render::NativeClipSettings settings{polygons,16,16,polygons,width,std::int32_t(height)};
            if(!clips[path].enqueue(device,command,point_buffer,corner_buffer,polygon_buffer,visibility_buffer,settings))
                throw std::runtime_error(clips[path].status());
            auto* seed=static_cast<SDL_GPUBuffer*>(clips[path].enqueue_spans(command,material_buffer,true,scale));require(seed);
            copy=SDL_BeginGPUCopyPass(command);require(copy);
            SDL_GPUTransferBufferLocation source{buffers.upload,poison_offset};SDL_GPUBufferRegion target{seed,0,span_bytes};
            SDL_UploadToGPUBuffer(copy,&source,&target,false);SDL_EndGPUCopyPass(copy);
            void* masked_texels=nullptr;
            auto* spans=static_cast<SDL_GPUBuffer*>(clips[path].enqueue_spans(command,material_buffer,true,scale,
                nullptr,1,masks?texel_buffer:nullptr,masks?64U:0U,masks?&masked_texels:nullptr,{},true,samples));require(spans);
            if(masks && (!masked_texels || clips[path].mask_buffer_bytes()<mask_bytes))
                throw std::runtime_error("Span mask fixture lacks complete storage");
            if(spans!=seed) throw std::runtime_error("Span reuse fixture replaced the poisoned target");
            const auto output=rasters[path].enqueue_row_spans(device,command,spans,polygons,width*scale,rows,true,masks?masked_texels:texel_buffer,true,
                nullptr,variant==3 || variant==7,-21,9,masks?clips[path].mask_buffer_bytes():0);
            if(!output.pixels || !output.surfaces) throw std::runtime_error(rasters[path].status());
            copy=SDL_BeginGPUCopyPass(command);require(copy);
            for(auto [buffer,from,bytes]:{
                std::tuple{spans,0U,span_bytes},
                std::tuple{static_cast<SDL_GPUBuffer*>(output.pixels),span_bytes,pixels*4},
                std::tuple{static_cast<SDL_GPUBuffer*>(output.surfaces),span_bytes+pixels*4,pixels*16}}) {
                SDL_GPUBufferRegion region{buffer,0,bytes};SDL_GPUTransferBufferLocation destination{buffers.download,from};
                SDL_DownloadFromGPUBuffer(copy,&region,&destination);
            }
            if(masks) {
                SDL_GPUBufferRegion region{static_cast<SDL_GPUBuffer*>(masked_texels),0,mask_bytes};
                SDL_GPUTransferBufferLocation destination{buffers.download,span_bytes+pixels*20};
                SDL_DownloadFromGPUBuffer(copy,&region,&destination);
            }
            SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
            require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
            const auto* downloaded=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,buffers.download,false));require(downloaded);
            results[path].assign(downloaded,downloaded+span_bytes+pixels*20+mask_bytes);SDL_UnmapGPUTransferBuffer(device,buffers.download);
        }
        for(unsigned path=1;path<paths;++path) {
        for(unsigned row=0;row<polygons*rows;++row) {
            RasterCommand reference{},candidate{};
            std::memcpy(&reference,results[0].data()+row*sizeof(reference),sizeof(reference));
            std::memcpy(&candidate,results[path].data()+row*sizeof(candidate),sizeof(candidate));
            // Sparse wobble can emit a zero-width command with authored row
            // bounds. It must still match in full, not be mistaken for a slot
            // that the clear pass left completely empty.
            if(!reference.left && !reference.top && !reference.right && !reference.bottom) {
                const bool minimal=path%4!=0 && (path%4!=3 || automatic_mode!=starfox::render::SpanClearMode::full);
                if(candidate.left || candidate.top || candidate.right || candidate.bottom
                    || (minimal?(candidate.even!=0xde || candidate.surface!=std::array<float,4>{-9,-8,-7,123})
                                :bool(std::memcmp(&reference,&candidate,sizeof(reference)))))
                    throw std::runtime_error("Empty span bounds or poisoned payload differs");
                ++empty;
            } else {
                if(std::memcmp(&reference,&candidate,sizeof(reference))) throw std::runtime_error("Live recycled span differs");
                ++live;compared+=sizeof(reference);
            }
        }
        if(std::memcmp(results[0].data()+span_bytes,results[path].data()+span_bytes,pixels*20))
            throw std::runtime_error("Recycled span raster color/surface differs");
        if(masks && (std::memcmp(results[0].data()+span_bytes+pixels*20,results[path].data()+span_bytes+pixels*20,mask_bytes)
            || std::memcmp(results[path].data()+span_bytes+pixels*20,texels.data(),64)))
            throw std::runtime_error("Recycled native/MSAA span masks or copied texel prefix differs");
        compared+=pixels*20+mask_bytes;
        }
        ++cases;
    }
    if(!live || !empty || !automatic_full || !automatic_parallel)
        throw std::runtime_error("Recycled span fixture lacks live/empty rows or both automatic routes");
    std::cout<<(cooperative_trace?"Cooperative span tracing (reference + four clear modes + full override): ":colour_trace?"Colour XY span tracing (reference + four clear modes + full override): ":"Span clear reuse (bounds + parallel + automatic): ")<<cases<<" cases, "<<live<<" live / "<<empty<<" poisoned empty rows; "
        <<compared<<" exact live-command/color/surface bytes; automatic "<<automatic_full<<" full / "<<automatic_parallel<<" parallel cases\n";
}
