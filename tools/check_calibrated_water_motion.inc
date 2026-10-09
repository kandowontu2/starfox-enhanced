// Independent uploaded SoA/guide fixture. The reference derives liquid depth
// and visibility from the authored mask, never from downloaded packed guides.
void native_water_motion(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=49,h=29,count=w*h;
    const auto layout=render::shadows::native_water_layers(w,h);
    check(bool(layout),"Liquid fixture layout rejected");
    struct Rays {
        Gpu& gpu;SDL_GPUBuffer* buffer{};
        ~Rays() {if(buffer) SDL_ReleaseGPUBuffer(gpu.device,buffer);}
    } rays{gpu};
    const SDL_GPUBufferCreateInfo buffer_info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,layout->storage_bytes,0};
    rays.buffer=SDL_CreateGPUBuffer(gpu.device,&buffer_info);check(rays.buffer,SDL_GetError());
    Texture source(gpu,w,h,format),underlay(gpu,w,h,format),owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        result(gpu,w,h,format),legacy(gpu,w,h,format);
    Bytes original(count*4),background(count*4),tags(count*4),storage(layout->storage_bytes);
    std::vector<std::array<float,4>> depths(count),velocities(count);
    std::vector<render::MotionBlurGuide> guides(count),old_guides(count);
    unsigned liquid_count{},dry_count{},protected_count{};
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const unsigned i=y*w+x;
        for(unsigned c=0;c<3;++c) original[i*4+c]=background[i*4+c]=25+(x*(5+c*2)+y*3+c*15)%110;
        original[i*4+3]=background[i*4+3]=255;tags[i*4]=255;tags[i*4+2]=1;
        const bool actor=x>=8 && x<=34 && y>=7 && y<=21;
        const bool liquid=x>=22 && x<=34 && y>=7 && y<=21;
        const bool protected_ink=y==11 || x==31;
        if(actor) {
            tags[i*4+2]=2;depths[i]={0,0,-1,1000};velocities[i]={-8,1,1200,1};
            original[i*4]=210;original[i*4+1]=55;original[i*4+2]=40;
        }
        if(liquid) {
            original[i*4]=20+x*3;original[i*4+1]=150;original[i*4+2]=190-y*2;
        }
        if(protected_ink) {tags[i*4]=0;tags[i*4+2]=0;++protected_count;}
        // Unknown ownership remains protected even if the receiver red byte
        // is nonzero. Water metadata is not permission to recolour UI.
        if(x==27 && y>=7 && y<=21) {tags[i*4]=255;tags[i*4+2]=255;}
        if(x==25 && y==15) original[i*4+3]=0;
        const bool eligible=tags[i*4+2]==1 || tags[i*4+2]==2;
        old_guides[i]={actor?-8.F:0,actor?1.F:0,actor?1000.F:0,actor,eligible};
        guides[i]=old_guides[i];
        std::uint32_t primary=liquid?0xFD123456U:0xFE654321U;
        const std::uint32_t world=0xFD102030U;
        std::array<float,4> water_surface{0,-1,0,220};
        // Invalid depth/normal must fall back to the original guide rather
        // than introducing a nonfinite depth or stripping valid actor motion.
        const bool invalid=liquid && x==24;
        if(invalid) water_surface[y%2?0:3]=y%2?std::numeric_limits<float>::quiet_NaN():-1.F;
        if(liquid && !invalid && eligible && tags[i*4]>0) {
            guides[i]={0,0,220,false,true};++liquid_count;
        } else if(actor && eligible) ++dry_count;
        std::memcpy(storage.data()+i*4,&primary,4);
        std::memcpy(storage.data()+layout->world_offset+i*4,&world,4);
        std::memcpy(storage.data()+layout->surface_offset+i*16,water_surface.data(),16);
    }
    check(liquid_count>100 && dry_count>100 && protected_count>50,"Liquid fixture lacks ownership coverage");
    gpu.begin();source.upload(original);underlay.upload(background);owner.upload(tags);
    surface.upload(words(depths));motion.upload(words(velocities));
    const SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,layout->storage_bytes,0};
    auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&transfer_info);check(transfer,SDL_GetError());
    auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());
    std::memcpy(mapped,storage.data(),storage.size());SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUTransferBufferLocation from{transfer,0};const SDL_GPUBufferRegion to{rays.buffer,0,layout->storage_bytes};
    SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);gpu.submit();
    const render::shadows::GpuReflectionOutput water{gpu.device,rays.buffer,w,h,w*4,*layout};
    render::GpuCalibratedMotionBlur blur;check(blur.initialize(gpu.device,format),blur.status().c_str());
    auto settings=render::motion_blur_preset(2);settings.interval_seconds=1./60;
    Bytes expected,old_expected;
    check(render::reconstruct_motion_blur(w,h,original,background,guides,settings,expected),"Liquid reference rejected");
    check(render::reconstruct_motion_blur(w,h,original,background,old_guides,settings,old_expected),"Legacy guide reference rejected");
    unsigned distinct{};for(unsigned i=0;i<expected.size();++i) distinct+=expected[i]!=old_expected[i];
    check(distinct>100,"Liquid reference did not expose submerged rigid-motion bug");
    const auto encode=[&](Texture& target,const render::shadows::GpuReflectionOutput* liquid) {
        void* output{};
        check(blur.enqueue(gpu.command,source.texture,underlay.texture,owner.texture,surface.texture,motion.texture,target.texture,
            w,h,settings,true,output,{},{},nullptr,1,liquid),blur.status().c_str());
        check(output==target.texture && blur.dispatch_count()==2+3*settings.samples,"Liquid shutter dispatch contract changed");
    };
    gpu.begin();encode(result,&water);encode(legacy,nullptr);gpu.submit();
    const auto actual=result.read();compare(actual,expected,original,tags,srgb(format)?2:1,"liquid-aware shutter");
    compare(legacy.read(),old_expected,original,tags,srgb(format)?2:1,"liquid absent shutter");
    gpu.begin();encode(result,&water);gpu.cancel();gpu.begin();encode(result,&water);gpu.submit();
    check(result.read()==actual,"Cancelled liquid shutter changed retained retry");
    for(unsigned fault=0;fault<6;++fault) {
        auto bad=water;
        switch(fault) {case 0:bad.width++;break;case 1:bad.row_bytes+=4;break;case 2:bad.buffer=nullptr;break;
            case 3:bad.device=nullptr;break;case 4:bad.water_layers.surface_offset+=4;break;case 5:bad.water_layers.storage_bytes-=4;break;}
        gpu.begin();void* output=reinterpret_cast<void*>(1);
        check(!blur.enqueue(gpu.command,source.texture,underlay.texture,owner.texture,surface.texture,motion.texture,result.texture,
            w,h,settings,true,output,{},{},nullptr,1,&bad) && !output && blur.dispatch_count()==0,
            "Malformed liquid metadata encoded work or returned an image");gpu.cancel();
    }
    std::cout<<"Native liquid shutter format="<<unsigned(format)<<": independent surface-depth ownership, submerged velocity exclusion, dry motion, protected/unknown ink, nonfinite fallback, OFF, same-command retention and cancellation passed ("
        <<distinct<<" oracle-sensitive bytes).\n";
}
