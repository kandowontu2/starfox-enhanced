// Isolated CPU and resident/byte-tag GPU oracles for the slim upscale resolve.
// Readbacks and waits here are fixtures only, not production frame work.
bool check_dedither(starfox::render::SdlGpuEffects& gpu,SDL_GPUDevice* device) {
    using namespace starfox::render;
    const auto require=[](bool ok){if(!ok) throw std::runtime_error(SDL_GetError());};
    struct ResidentFixture {
        SDL_GPUDevice* device{};
        SDL_GPUTexture *source{},*target{},*glow{},*model{};
        SDL_GPUBuffer *packed{},*surfaces{};
        SDL_GPUTransferBuffer* transfer{};
        ~ResidentFixture() {
            if(source)SDL_ReleaseGPUTexture(device,source);
            if(target)SDL_ReleaseGPUTexture(device,target);
            if(glow)SDL_ReleaseGPUTexture(device,glow);
            if(model)SDL_ReleaseGPUTexture(device,model);
            if(packed)SDL_ReleaseGPUBuffer(device,packed);
            if(surfaces)SDL_ReleaseGPUBuffer(device,surfaces);
            if(transfer)SDL_ReleaseGPUTransferBuffer(device,transfer);
        }
    };
    std::uint64_t checked_bytes{},changed_pixels{};
    unsigned cases{},capture_cases{};
    for(unsigned scale=1;scale<=10;++scale) for(unsigned pattern=0;pattern<4;++pattern) {
        Framebuffer frame(33,25,scale);frame.enable_layer_tags(true);
        const auto w=frame.stored_width(),h=frame.stored_height();
        std::vector<std::uint8_t> original(std::size_t(w)*h*4);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const auto i=std::size_t(y)*w+x;
            // Distinct flat/HUD/background/textured/world/terrain tags, cutouts
            // and model-only interiors. Alpha and unused packed bits vary too.
            const auto tag=x<w/3?((x/5+y/7)%6):pattern==3 && (x+y)%11==0?4:0;
            frame.layer_tags()[i]=std::uint8_t(tag);frame.pixels()[i]=std::uint8_t(i*19+pattern*3);
            const auto parity=pattern==2?((x/scale+y/scale)&1):((x+y)&1);
            original[i*4]=std::uint8_t(pattern==0?(i*73+x*29):parity?240:17);
            original[i*4+1]=std::uint8_t(pattern==0?(i*31+y*13):parity?23:190);
            original[i*4+2]=std::uint8_t(pattern==0?(i*97+x*y):parity?162:31);
            original[i*4+3]=std::uint8_t(i*7+pattern*31);
        }
        auto expected=original;std::vector<std::uint8_t> scratch;
        smooth_models(4,frame,expected,scratch);
        std::size_t changed{};
        for(std::size_t i=0;i<frame.pixels().size();++i) {
            if(expected[i*4+3]!=original[i*4+3]) throw std::runtime_error("CPU dedither changed alpha");
            changed+=std::memcmp(expected.data()+i*4,original.data()+i*4,3)!=0;
        }
        if(pattern>0 && scale>1 && !changed) throw std::runtime_error("Dedither fixture did not resolve a checkerboard");
        changed_pixels+=changed;
        GpuEffectSettings settings;settings.smoothing=4;
        auto bytes=original;
        if(!gpu.apply(device,frame,bytes,settings)) throw std::runtime_error(gpu.status());
        if(bytes!=expected) throw std::runtime_error("Byte-tag dedither differs from CPU oracle");
        checked_bytes+=bytes.size();

        ResidentFixture fixture;fixture.device=device;
        SDL_GPUTextureCreateInfo texture{};texture.type=SDL_GPU_TEXTURETYPE_2D;
        texture.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        texture.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        texture.width=w;texture.height=h;texture.layer_count_or_depth=texture.num_levels=1;
        fixture.source=SDL_CreateGPUTexture(device,&texture);require(fixture.source);
        fixture.target=SDL_CreateGPUTexture(device,&texture);require(fixture.target);
        const auto size=Uint32(original.size());
        SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,size,0};
        fixture.packed=SDL_CreateGPUBuffer(device,&buffer);require(fixture.packed);
        buffer.size=size*4;fixture.surfaces=SDL_CreateGPUBuffer(device,&buffer);require(fixture.surfaces);
        SDL_GPUTransferBufferCreateInfo staging{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,size*6,0};
        fixture.transfer=SDL_CreateGPUTransferBuffer(device,&staging);require(fixture.transfer);
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,fixture.transfer,false));require(mapped);
        std::memcpy(mapped,original.data(),size);
        auto* words=reinterpret_cast<Uint32*>(mapped+size);
        for(std::size_t i=0;i<frame.pixels().size();++i)
            words[i]=frame.pixels()[i]|(Uint32(frame.layer_tags()[i])<<8)|(Uint32(frame.pixels()[i])<<16)
                |(i%5?0x01000000U:0U)|Uint32((i%7)<<28);
        auto* surface_data=reinterpret_cast<float*>(mapped+size*2);
        for(std::size_t i=0;i<frame.pixels().size();++i) {
            surface_data[i*4]=surface_data[i*4+1]=0;
            surface_data[i*4+2]=-1;surface_data[i*4+3]=700+float(i%23);
        }
        SDL_UnmapGPUTransferBuffer(device,fixture.transfer);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* pass=SDL_BeginGPUCopyPass(command);
        if(!pass){SDL_CancelGPUCommandBuffer(command);require(false);}
        SDL_GPUTextureTransferInfo from{fixture.transfer,0,0,0};
        SDL_GPUTextureRegion to{fixture.source,0,0,0,0,0,w,h,1};SDL_UploadToGPUTexture(pass,&from,&to,false);
        SDL_GPUTransferBufferLocation origin{fixture.transfer,size};SDL_GPUBufferRegion region{fixture.packed,0,size};
        SDL_UploadToGPUBuffer(pass,&origin,&region,false);
        origin.offset=size*2;region={fixture.surfaces,0,size*4};SDL_UploadToGPUBuffer(pass,&origin,&region,false);
        SDL_EndGPUCopyPass(pass);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        const bool waited=SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);require(waited);
        const GpuCompositeOutput resident{device,fixture.source,fixture.packed,fixture.surfaces,w,h};
        for(bool presented:{false,true}) {
            settings.presentation_texture=presented?fixture.target:nullptr;
            bytes=original;
            if(!gpu.apply_resident(resident,frame,bytes,settings)) throw std::runtime_error(gpu.status());
            if(presented && !gpu.readback(bytes)) throw std::runtime_error(gpu.status());
            if(bytes!=expected) throw std::runtime_error("Resident/captured dedither differs from CPU oracle");
            checked_bytes+=bytes.size();
        }
        // Reconsume the held original with no effect: the preceding pass must
        // not have modified the borrowed source or mistaken palette/flag bits
        // for ownership. OFF is exact after both direct/captured paths.
        settings={};bytes.clear();
        if(!gpu.apply_resident(resident,frame,bytes,settings)) throw std::runtime_error(gpu.status());
        if(bytes!=original) throw std::runtime_error("Dedither overwrote its borrowed source or OFF output");
        checked_bytes+=bytes.size();
        settings.presentation_texture=fixture.target;bytes.clear();
        if(!gpu.apply_resident(resident,frame,bytes,settings) || !gpu.readback(bytes))
            throw std::runtime_error(gpu.status());
        if(bytes!=original) throw std::runtime_error("Captured OFF output retained a stale ping-pong image");
        auto repeated=bytes;
        if(!gpu.readback(repeated) || repeated!=original)
            throw std::runtime_error("Repeated deferred capture changed the held image");
        checked_bytes+=bytes.size()*2;++capture_cases;
        if(pattern==0 && (scale==1 || scale==2 || scale==4)) {
            {
                SdlGpuEffects held;GpuEffectSettings off;off.presentation_texture=fixture.target;
                auto captured=original;
                if(!held.apply_resident(resident,frame,captured,off)) throw std::runtime_error(held.status());
                if(held.texture_payload_bytes()!=std::uint64_t(size)*2+32)
                    throw std::runtime_error("Plain deferred capture allocated a redundant full-frame snapshot");
                const auto overwrite_source=[&](bool restore) {
                    auto* upload_data=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,fixture.transfer,true));require(upload_data);
                    if(restore) std::memcpy(upload_data,original.data(),size);
                    else std::memset(upload_data,0,size);
                    SDL_UnmapGPUTransferBuffer(device,fixture.transfer);
                    auto* queued=SDL_AcquireGPUCommandBuffer(device);require(queued);
                    auto* copy_pass=SDL_BeginGPUCopyPass(queued);
                    if(!copy_pass){SDL_CancelGPUCommandBuffer(queued);require(false);}
                    SDL_GPUTextureTransferInfo upload_from{fixture.transfer,0,0,0};
                    SDL_GPUTextureRegion upload_to{fixture.source,0,0,0,0,0,w,h,1};
                    SDL_UploadToGPUTexture(copy_pass,&upload_from,&upload_to,false);SDL_EndGPUCopyPass(copy_pass);
                    require(SDL_SubmitGPUCommandBuffer(queued));
                };
                // A screenshot requested after the producer reuses its image
                // must read our held output, never the now-overwritten source.
                overwrite_source(false);
                if(!held.readback(captured) || captured!=original)
                    throw std::runtime_error("OFF capture borrowed a subsequently overwritten compositor image");
                checked_bytes+=captured.size();++capture_cases;
                overwrite_source(true);
            }
            fixture.glow=SDL_CreateGPUTexture(device,&texture);require(fixture.glow);
            fixture.model=SDL_CreateGPUTexture(device,&texture);require(fixture.model);
            for(unsigned split=0;split<4;++split) {
                settings={};settings.hdr=2;settings.chromatic=1;settings.bloom_model=2;settings.bloom_world=1;
                auto reference=original;
                if(!gpu.apply_resident(resident,frame,reference,settings)) throw std::runtime_error(gpu.status());
                settings.presentation_texture=fixture.target;
                settings.presentation_glow_texture=(split&1)?fixture.glow:nullptr;
                settings.presentation_model_texture=(split&2)?fixture.model:nullptr;
                bytes.clear();
                if(!gpu.apply_resident(resident,frame,bytes,settings) || !gpu.readback(bytes))
                    throw std::runtime_error(gpu.status());
                if(bytes!=reference) throw std::runtime_error("Deferred capture lost its pre-extraction styled image");
                checked_bytes+=bytes.size();++capture_cases;
            }
        }
        ++cases;
    }
    std::cout<<"Dedither: "<<cases<<" scale/pattern cases, "<<checked_bytes
        <<" exact byte comparisons and "<<changed_pixels<<" resolved model pixels; "
        <<"CPU/byte-tag/resident/captured/OFF, alpha, protected layers and held source pass; "
        <<capture_cases<<" deferred OFF/styled/split captures pass\n";
    return true;
}
