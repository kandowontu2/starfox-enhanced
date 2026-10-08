// Specialization must preserve raw packed ownership, normals, depth and motion,
// not just the final RGBA presentation. This fixture uploads known independent
// inputs and compares all output bytes, including resized/jittered sampling.
bool composite_pipeline_guides_check(starfox::render::GpuRaster& raster) {
    using namespace starfox::render;
    const auto ensure=[](bool ok){if(!ok) throw std::runtime_error(SDL_GetError());};
    struct RestoreEnvironment {
        std::array<const char*,3> names{"STARFOX_TEST_SPLIT_COMPOSITE_PIPELINES","STARFOX_TEST_UNIFIED_COMPOSITE_PIPELINE",
            "STARFOX_TEST_FAIL_COMPOSITE_INITIALIZE"};
        std::array<std::optional<std::string>,3> values;
        static void set(const char* name,const char* value) {
#if defined(_WIN32)
            _putenv_s(name,value?value:"");
#else
            if(value) setenv(name,value,1);else unsetenv(name);
#endif
        }
        RestoreEnvironment(){for(unsigned i=0;i<names.size();++i) if(const auto* value=std::getenv(names[i])) values[i]=value;}
        ~RestoreEnvironment(){for(unsigned i=0;i<names.size();++i) set(names[i],values[i]?values[i]->c_str():nullptr);}
    } environment;
    RestoreEnvironment::set(environment.names[2],nullptr);
    constexpr unsigned source_width=64,source_height=48,count=source_width*source_height;
    Framebuffer empty(source_width,source_height);
    RasterCommands empty_commands;empty_commands.reset(source_width,source_height);
    struct SelectedDevice {SDL_GPUDevice* device{};~SelectedDevice(){if(device) SDL_DestroyGPUDevice(device);}} selected;
    GpuRaster selected_raster;
    auto* source_raster=&raster;
    if(std::getenv("STARFOX_TEST_LOW_POWER_GPU")) {
        const auto properties=SDL_CreateProperties();ensure(properties);
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,true);
        selected.device=SDL_CreateGPUDeviceWithProperties(properties);SDL_DestroyProperties(properties);ensure(selected.device);
        ensure(selected_raster.render_resident(selected.device,empty_commands,false));source_raster=&selected_raster;
    } else ensure(raster.render(empty_commands,empty,nullptr));
    auto* device=static_cast<SDL_GPUDevice*>(source_raster->resident_output().device);
    std::cout<<"Compositor fixture adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")
        <<" driver="<<SDL_GetGPUDeviceDriver(device)<<'\n';
    struct Resources {
        SDL_GPUDevice* device;
        std::array<SDL_GPUBuffer*,5> buffers{};
        SDL_GPUTexture* msaa{};
        SDL_GPUTransferBuffer *upload{},*download{};
        ~Resources(){SDL_WaitForGPUIdle(device);for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
            if(msaa) SDL_ReleaseGPUTexture(device,msaa);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);}
    } resources{device};
    std::vector<std::uint32_t> packed(count),background(count);
    std::vector<std::array<float,4>> normals(count),motion(count);
    std::vector<float> depth(count);
    for(unsigned y=0;y<source_height;++y) for(unsigned x=0;x<source_width;++x) {
        const unsigned i=y*source_width+x;
        packed[i]=(x+y)%7?((i%253+1)|(((x+y)%5==0?1U:3U)<<8)|0x00010000U):0;
        if(i%13==0) packed[i]|=0x80000000U|(37U<<8);
        background[i]=((x+y)%3?0x04000000U:0U)|((x*7+y*3)%251+1);
        normals[i]={float(x)/64,float(y)/48,-0.5f,float(i+1)};
        depth[i]=float(i+1);
        motion[i]={float(int(x)-32)/16,float(int(y)-24)/16,float(i+1),1};
    }
    const std::array<unsigned,5> sizes{count*4,count*16,count*4,count*16,count*4};
    const std::array<const void*,5> data{packed.data(),normals.data(),depth.data(),motion.data(),background.data()};
    unsigned input_bytes=count*4;
    for(unsigned i=0;i<5;++i) {
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
        resources.buffers[i]=SDL_CreateGPUBuffer(device,&info);ensure(resources.buffers[i]);
        input_bytes+=sizes[i];
    }
    SDL_GPUTextureCreateInfo texture{};texture.type=SDL_GPU_TEXTURETYPE_2D;
    texture.width=source_width;texture.height=source_height;texture.layer_count_or_depth=texture.num_levels=1;
    texture.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;texture.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    resources.msaa=SDL_CreateGPUTexture(device,&texture);ensure(resources.msaa);
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,input_bytes,0};
    resources.upload=SDL_CreateGPUTransferBuffer(device,&transfer);ensure(resources.upload);
    auto* mapped=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device,resources.upload,false));ensure(mapped);
    unsigned offset=0;
    for(unsigned i=0;i<5;++i) {std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
    for(unsigned i=0;i<count;++i) {
        mapped[offset+i*4]=std::uint8_t(i*3);mapped[offset+i*4+1]=std::uint8_t(i*7);
        mapped[offset+i*4+2]=std::uint8_t(i*11);mapped[offset+i*4+3]=std::uint8_t(i%3?128:0);
    }
    SDL_UnmapGPUTransferBuffer(device,resources.upload);
    auto* command=SDL_AcquireGPUCommandBuffer(device);ensure(command);
    auto* copy=SDL_BeginGPUCopyPass(command);ensure(copy);
    offset=0;
    for(unsigned i=0;i<5;++i) {
        SDL_GPUTransferBufferLocation from{resources.upload,offset};
        SDL_GPUBufferRegion to{resources.buffers[i],0,sizes[i]};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);offset+=sizes[i];
    }
    SDL_GPUTextureTransferInfo from{resources.upload,offset,0,0};
    SDL_GPUTextureRegion to{resources.msaa,0,0,0,0,0,source_width,source_height,1};
    SDL_UploadToGPUTexture(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
    ensure(SDL_SubmitGPUCommandBuffer(command));
    Palette256 palette{};
    for(unsigned i=0;i<256;++i) palette[i]={std::uint8_t(i*3),std::uint8_t(i*7),std::uint8_t(i*11),255};
    unsigned cases=0;std::size_t verified=0,retained_guides=0;
    for(unsigned target_scale:{1U,2U,3U}) for(bool antialiased:{false,true}) for(bool world_only:{false,true}) {
        for(unsigned margins=0;margins<3;++margins) {
            Framebuffer cpu(32,24,2);cpu.enable_layer_tags(true);
            std::vector<std::uint8_t> coverage(cpu.pixels().size()),after_late(coverage.size()),early(coverage.size());
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const unsigned i=y*cpu.stored_width()+x;
                cpu.set_stored(x,y,std::uint8_t(x*3+y*7),i%9?PixelLayer::background:PixelLayer::two_d);
                coverage[i]=i%17==0;after_late[i]=i%19==0;early[i]=i%23==0;
            }
            GpuRasterOutput source{};source.device=device;source.pixels=resources.buffers[0];source.surfaces=resources.buffers[1];
            source.geometry_depth=resources.buffers[2];source.motion=resources.buffers[3];
            source.width=source_width;source.height=source_height;source.msaa_color=antialiased?resources.msaa:nullptr;
            auto background_source=source;background_source.pixels=resources.buffers[4];
            GpuCompositeBackground backdrop{background_source,early};
            backdrop.margin_origin=margins?4:0;backdrop.margin_width=24;
            backdrop.repair_transparent_margins=margins==2;backdrop.match_right_margin=world_only;
            LayerCompositeSettings settings;settings.offset_x=-2;settings.offset_y=1;settings.clip_left=1;
            settings.clip_top=2;settings.clip_right=30;settings.clip_bottom=23;
            const std::array<std::uint32_t,4> mapping{source_width,source_height,32*target_scale,24*target_scale};
            const std::array<float,2> jitter{0.25f,-0.375f};
            std::array<std::vector<std::uint8_t>,2> captures;
            for(unsigned mode=0;mode<2;++mode) {
                RestoreEnvironment::set(environment.names[0],mode?"1":nullptr);
                RestoreEnvironment::set(environment.names[1],mode?nullptr:"1");
                GpuComposite compositor;
                ensure(compositor.compose(source,2,cpu,coverage,settings,palette,&background_source,&backdrop,after_late,world_only,mapping,jitter));
                const auto output=compositor.output();const unsigned n=output.width*output.height,bytes=n*44;
                transfer={SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes,0};
                if(resources.download) SDL_ReleaseGPUTransferBuffer(device,resources.download);
                resources.download=SDL_CreateGPUTransferBuffer(device,&transfer);ensure(resources.download);
                command=SDL_AcquireGPUCommandBuffer(device);ensure(command);copy=SDL_BeginGPUCopyPass(command);ensure(copy);
                const std::array<void*,4> buffers{output.packed,output.surfaces,output.geometry_depth,output.motion};
                const std::array<unsigned,4> lengths{n*4,n*16,n*4,n*16};
                unsigned destination=0;
                for(unsigned i=0;i<4;++i) {
                    SDL_GPUBufferRegion buffer_from{static_cast<SDL_GPUBuffer*>(buffers[i]),0,lengths[i]};
                    SDL_GPUTransferBufferLocation buffer_to{resources.download,destination};
                    SDL_DownloadFromGPUBuffer(copy,&buffer_from,&buffer_to);destination+=lengths[i];
                }
                SDL_GPUTextureRegion texture_from{static_cast<SDL_GPUTexture*>(output.rgba),0,0,0,0,0,output.width,output.height,1};
                SDL_GPUTextureTransferInfo texture_to{resources.download,destination,0,0};
                SDL_DownloadFromGPUTexture(copy,&texture_from,&texture_to);SDL_EndGPUCopyPass(copy);
                auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);ensure(fence);
                ensure(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
                const auto* bytes_from=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,resources.download,false));ensure(bytes_from);
                captures[mode].assign(bytes_from,bytes_from+bytes);SDL_UnmapGPUTransferBuffer(device,resources.download);
            }
            if(captures[0]!=captures[1]) throw std::runtime_error("Split compositor raw guide/RGBA mismatch");
            const unsigned output_count=32*24*target_scale*target_scale;
            unsigned populated=0;
            for(unsigned i=0;i<output_count;++i) {
                float visible_depth;
                std::memcpy(&visible_depth,captures[0].data()+output_count*20+i*4,4);
                if(visible_depth>0) ++populated;
            }
            if(!populated) throw std::runtime_error("Compositor fixture did not retain visible model depth");
            retained_guides+=populated;
            verified+=captures[0].size();++cases;
        }
    }
    for(unsigned mode=0;mode<2;++mode) {
        RestoreEnvironment::set(environment.names[0],mode?"1":nullptr);
        RestoreEnvironment::set(environment.names[1],mode?nullptr:"1");
        Framebuffer cpu(32,24,2),actual(32,24,2),expected(32,24,2);cpu.enable_layer_tags(true);
        GpuRasterOutput source{};source.device=device;source.pixels=resources.buffers[0];source.surfaces=resources.buffers[1];
        source.width=source_width;source.height=source_height;
        GpuComposite retry,fresh;const LayerCompositeSettings settings;
        RestoreEnvironment::set(environment.names[2],"1");
        if(retry.compose(source,2,cpu,{},settings,palette) || retry.output().rgba
            || retry.status().find("Injected compositor initialization failure")==std::string::npos)
            throw std::runtime_error("Failed compositor initialization exposed an output");
        RestoreEnvironment::set(environment.names[2],nullptr);
        std::vector<std::uint8_t> actual_rgba,expected_rgba;
        ensure(retry.compose(source,2,cpu,{},settings,palette) && retry.readback(actual,actual_rgba));
        ensure(fresh.compose(source,2,cpu,{},settings,palette) && fresh.readback(expected,expected_rgba));
        if(actual.pixels()!=expected.pixels() || actual.layer_tags()!=expected.layer_tags() || actual_rgba!=expected_rgba)
            throw std::runtime_error("Compositor initialization retry differs from fresh owner");
    }
    std::cout<<"Compositor specialization: "<<cases<<" resized/jittered, world/HUD, margin, MSAA and late-overlay cases; "
        <<verified<<" exact packed/normal/depth/motion/RGBA bytes; "<<retained_guides<<" visible depth guides\n";
    std::cout<<"Compositor initialization: both pipeline modes reject partial owners and recover exactly without a device restart\n";
    return true;
}
