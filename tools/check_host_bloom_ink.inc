// Exercise the presentation split, not just the unsplit post-effects image.
// White menu ink can have the same RGB as clipped world bloom beneath it.
static void check_host_bloom_ink(starfox::render::SdlGpuEffects& gpu,SDL_GPUDevice* device) {
    using namespace starfox::render;
    const auto require=[](bool valid,const char* message) {
        if(!valid) throw std::runtime_error(message);
    };
    struct Targets {
        SDL_GPUDevice* device;
        unsigned width,height;
        std::array<SDL_GPUTexture*,2> textures{};
        SDL_GPUTransferBuffer* download{};
        Targets(SDL_GPUDevice* d,unsigned w,unsigned h):device(d),width(w),height(h) {
            SDL_GPUTextureCreateInfo info{};
            info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
            info.width=w;info.height=h;info.layer_count_or_depth=info.num_levels=1;
            for(auto& texture:textures) texture=SDL_CreateGPUTexture(device,&info);
            SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*8,0};
            download=SDL_CreateGPUTransferBuffer(device,&transfer);
            if(!download || !textures[0] || !textures[1]) {
                const std::string error=SDL_GetError();
                for(auto* texture:textures) if(texture) SDL_ReleaseGPUTexture(device,texture);
                if(download) SDL_ReleaseGPUTransferBuffer(device,download);
                throw std::runtime_error(error);
            }
        }
        ~Targets() {
            for(auto* texture:textures) SDL_ReleaseGPUTexture(device,texture);
            SDL_ReleaseGPUTransferBuffer(device,download);
        }
        std::vector<std::uint8_t> read() {
            auto* command=SDL_AcquireGPUCommandBuffer(device);
            if(!command) throw std::runtime_error(SDL_GetError());
            auto* pass=SDL_BeginGPUCopyPass(command);
            if(!pass) {const std::string error=SDL_GetError();SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(error);}
            for(unsigned i=0;i<textures.size();++i) {
                SDL_GPUTextureRegion region{textures[i],0,0,0,0,0,width,height,1};
                SDL_GPUTextureTransferInfo target{download,i*width*height*4,0,0};
                SDL_DownloadFromGPUTexture(pass,&region,&target);
            }
            SDL_EndGPUCopyPass(pass);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            if(!fence) throw std::runtime_error(SDL_GetError());
            const bool waited=SDL_WaitForGPUFences(device,true,&fence,1);
            SDL_ReleaseGPUFence(device,fence);
            if(!waited) throw std::runtime_error(SDL_GetError());
            const auto* bytes=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,download,false));
            if(!bytes) throw std::runtime_error(SDL_GetError());
            std::vector<std::uint8_t> result(bytes,bytes+std::size_t(width)*height*8);
            SDL_UnmapGPUTransferBuffer(device,download);
            return result;
        }
    };
    unsigned coincidences=0;
    for(unsigned scale:{1U,2U,4U}) {
        Framebuffer world(256,224,scale),overlay(256,224);
        world.enable_layer_tags(true);
        std::fill(world.layer_tags().begin(),world.layer_tags().end(),std::uint8_t(PixelLayer::three_d));
        const std::array<std::array<unsigned,3>,3> colours{{{180,200,215},{255,220,64},{255,255,255}}};
        for(unsigned ink=0;ink<3;++ink) for(unsigned y=40;y<44;++y) for(unsigned x=30+ink*40;x<34+ink*40;++x)
            overlay.set(x,y,std::array<std::uint8_t,3>{7,10,14}[ink]);
        const auto size=world.pixels().size()*4;
        std::vector<std::uint8_t> source(size,242);
        for(std::size_t i=3;i<size;i+=4) source[i]=255;
        GpuEffectSettings settings;settings.bloom_model=3;settings.bloom_world=3;
        std::vector<std::uint8_t> base,glow;
        settings.bloom_base=&base;settings.bloom_glow=&glow;
        auto full=source;
        require(gpu.apply(device,world,full,settings),gpu.status().c_str());
        const auto white=(std::size_t(40*scale)*world.stored_width()+110*scale)*4;
        require(base.size()==size && glow.size()==size,"Missing host-bloom reference snapshots");
        require(glow[white]==255 && glow[white+1]==255 && glow[white+2]==255 && base[white]<255,
            "Host-bloom fixture did not exercise white ink/clipped-world coincidence");
        ++coincidences;
        Targets targets(device,world.stored_width(),world.stored_height());
        for(unsigned brightness:{0U,7U,15U}) {
            settings.bloom_base=nullptr;settings.bloom_glow=nullptr;
            settings.setup_overlay=GpuEffectSettings::SetupOverlay{&overlay,12,243,brightness};
            settings.presentation_texture=targets.textures[0];settings.presentation_glow_texture=targets.textures[1];
            auto direct=source;
            require(gpu.apply(device,world,direct,settings),gpu.status().c_str());
            const auto split=targets.read();
            for(unsigned ink=0;ink<3;++ink) for(unsigned y=40*scale;y<44*scale;++y)
                for(unsigned x=(30+ink*40)*scale;x<(34+ink*40)*scale;++x)
                    for(unsigned channel=0;channel<3;++channel) {
                        const auto i=(std::size_t(y)*world.stored_width()+x)*4+channel;
                        require(split[i]==colours[ink][channel]*brightness/15,"Host ink was extracted into world bloom");
                        require(split[size+i]==0,"Host ink retained a separately resampled glow contribution");
                    }
            // Outside the panel the original world bloom must remain, not be disabled.
            require(split[0]==base[0] && split[size]==glow[0]-base[0] && split[size]>0,
                "Protecting host ink disabled or changed unrelated world bloom");
        }
    }
    require(coincidences==3,"Missing host-bloom coincidence controls");
    std::cout<<"Host bloom ownership: exact grey/yellow/white inks at 1x/2x/4x and 0/7/15 brightness; world glow retained\n";
}
