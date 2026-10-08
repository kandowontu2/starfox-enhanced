// Fixture-only CPU references/downloads. Production keeps the ray result and
// environment resolve resident on the caller's existing GPU command/queue.
void check_environment_reflection(starfox::render::SdlGpuEffects& gpu,SDL_GPUDevice* device) {
    using namespace starfox::render;
    const auto check=[](bool ok) {if(!ok) throw std::runtime_error("Environment/ray fusion: "+std::string(SDL_GetError()));};
    auto* env=SDL_GetEnvironment();
    constexpr auto key="STARFOX_TEST_SEPARATE_ENVIRONMENT_REFLECTION";
    constexpr auto force_key="STARFOX_TEST_FUSE_ENVIRONMENT_REFLECTION";
    const auto* previous=SDL_GetEnvironmentVariable(env,key);
    struct Restore {
        SDL_Environment* env;const char* key;std::optional<std::string> saved;
        ~Restore(){if(saved) SDL_SetEnvironmentVariable(env,key,saved->c_str(),true);else SDL_UnsetEnvironmentVariable(env,key);}
    } restore{env,key,previous?std::optional<std::string>{previous}:std::nullopt};
    const auto* previous_force=SDL_GetEnvironmentVariable(env,force_key);
    Restore restore_force{env,force_key,previous_force?std::optional<std::string>{previous_force}:std::nullopt};
    check(SDL_SetEnvironmentVariable(env,force_key,"1",true));
    struct Owned {
        SDL_GPUDevice* device;SDL_GPUBuffer* rays{};SDL_GPUTransferBuffer* upload{};
        ~Owned(){SDL_WaitForGPUIdle(device);if(rays) SDL_ReleaseGPUBuffer(device,rays);if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);}
    } owned{device};
    constexpr unsigned maximum=32*13*16;
    SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,maximum*4,0};
    owned.rays=SDL_CreateGPUBuffer(device,&bi);check(owned.rays);
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,maximum*4,0};
    owned.upload=SDL_CreateGPUTransferBuffer(device,&ti);check(owned.upload);
    BackdropImage sky;sky.width=16;sky.height=8;
    for(unsigned i=0;i<128;++i) sky.pixels.push_back(0xff000000u|((i*17)&255u)|(((i*29)&255u)<<8)|(((i*31)&255u)<<16));
    std::uint64_t bytes=0,hits=0,protected_pixels=0,fallbacks=0;unsigned cases=0;
    for(unsigned scale:{1U,2U,4U}) for(unsigned material:{1U,6U,7U,8U,10U})
    for(unsigned variant=0;variant<3;++variant) {
        Framebuffer f(32,13,scale);f.enable_layer_tags(true);
        const unsigned w=f.stored_width(),h=f.stored_height();
        const unsigned rw=w-(variant==1?3:0),rh=h-(variant==2?2:0);
        const int offset=variant==1?2:variant==2?-2:0;
        std::vector<std::uint8_t> input(w*h*4);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const auto i=y*w+x,lx=x/scale;
            f.pixels()[i]=(lx+y/scale)%8;
            f.layer_tags()[i]=lx>=4 && lx<7?1:lx>=8 && lx<11?0:lx>=20 && lx<24?5:lx>=27?4:2;
            input[i*4]=(i*19+37)&255;input[i*4+1]=(i*23+59)&255;
            input[i*4+2]=(i*31+71)&255;input[i*4+3]=(i*7+3)&255;
        }
        std::vector<unsigned> rays(rw*rh);
        for(unsigned i=0;i<rays.size();++i) {
            const unsigned marker=i%7==0?0:i%7==1?255:i%7==2?253:254;
            rays[i]=(marker<<24)|((i*13+11)&255)|(((i*17+29)&255)<<8)|(((i*37+41)&255)<<16);
        }
        auto* mapped=SDL_MapGPUTransferBuffer(device,owned.upload,true);check(mapped);
        std::memcpy(mapped,rays.data(),rays.size()*4);SDL_UnmapGPUTransferBuffer(device,owned.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command);
        auto* copy=SDL_BeginGPUCopyPass(command);check(copy);
        SDL_GPUTransferBufferLocation from{owned.upload,0};SDL_GPUBufferRegion to{owned.rays,0,unsigned(rays.size()*4)};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);check(SDL_SubmitGPUCommandBuffer(command));
        GpuEffectSettings settings;auto& e=settings.environment;
        e.modes={material,variant,1,0};e.motion={5.f,17.f,29.f,.75f};
        e.plane={variant==1?.35f:variant==2?-.30f:0.f,5,0,1};
        e.classes[1]=5;e.classes[2]=1;e.classes[3]=2;e.classes[4]=3;e.classes[5]=4;e.classes[6]=6;e.classes[7]=7;
        e.backdrop=&sky;e.ray_water=true;e.scroll_fraction={.37f,-.23f,0,0};
        settings.resident_reflection={device,owned.rays,rw,rh,rw*4};settings.reflection_offset_y=offset;
        std::array<std::vector<std::uint8_t>,2> results{input,input};
        for(unsigned path=0;path<2;++path) {
            check(path?SDL_UnsetEnvironmentVariable(env,key):SDL_SetEnvironmentVariable(env,key,"1",true));
            check(gpu.apply(device,f,results[path],settings));
        }
        check(results[0]==results[1]);bytes+=results[0].size();
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const auto i=y*w+x;
            const auto layer=f.layer_tags()[i];
            unsigned kind=layer==5?5:e.classes[f.pixels()[i]];
            if(material==10 && layer==2 && (kind==0 || kind==6)
                && (float(y)+.5f)/scale>=5.f+e.plane[0]*((float(x)+.5f)/scale-float(w)/scale*.5f)) kind=5;
            const bool receiver=(layer==2 || layer==5) && kind>=1 && kind<=5 && (material!=1 || kind==5);
            const int sy=int(y)-offset;
            const bool valid=receiver && x<rw && sy>=0 && sy<int(rh);
            const unsigned traced=valid?rays[unsigned(sy)*rw+x]:0;
            if(valid && (traced>>24)==254) {
                for(unsigned c=0;c<3;++c) check(results[1][i*4+c]==((traced>>(c*8))&255));
                ++hits;
            } else if(receiver) ++fallbacks;
            if(layer!=2 && layer!=5) {
                for(unsigned c=0;c<4;++c) check(results[1][i*4+c]==input[i*4+c]);
                ++protected_pixels;
            }
            check(results[1][i*4+3]==input[i*4+3]);
        }
        if(variant==2) gpu.release_device(); // Same owner, resize/recreate/toggle.
        ++cases;
    }
    check(hits && fallbacks && protected_pixels);
    std::cout<<"Environment/ray fusion: "<<cases<<" cases, "<<bytes<<" exact separate-pass RGBA bytes; "
        <<hits<<" independently checked liquid hits, "<<fallbacks<<" missing/bounded/model-marker fallbacks and "
        <<protected_pixels<<" protected pixels; Auto/water/mirror/gold/lava, banks, fractional scrolling, alpha and owner reuse pass\n";
}
