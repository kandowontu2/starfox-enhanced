void ssaa_drawn_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=65,h=37;
    render::GpuCalibratedScene scene;check(scene.initialize(gpu.device,format),scene.status().c_str());
    render::GpuCalibratedAa aa;check(aa.initialize(gpu.device,format),aa.status().c_str());
    Texture native(gpu,w,h,format),native_mask(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),native_depth(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT),target(gpu,w,h,format);
    unsigned long long compared=0;unsigned subpixel_cells=0;
    for(unsigned factor:{2U,3U,4U}) for(unsigned eye:{0U,1U}) for(unsigned frame:{0U,1U}) {
        const unsigned hw=w*factor,hh=h*factor;
        Texture high(gpu,hw,hh,format),high_mask(gpu,hw,hh,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),high_depth(gpu,hw,hh,SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
        std::array<render::CalibratedSceneDraw,2> draws;
        std::array<vr::SceneVertex,3> model{};
        constexpr float positions[3][3]{{-.56F,-.39F,-2},{.64F,.15F,-1.3F},{-.24F,.68F,-2.4F}};
        for(unsigned i=0;i<3;++i) {
            std::copy(positions[i],positions[i]+3,model[i].position);model[i].position[0]+=.021F*frame;
            model[i].color[0]=model[i].odd_color[0]=.8F;model[i].color[1]=model[i].odd_color[1]=.5F;
            model[i].color[2]=model[i].odd_color[2]=.17F;model[i].color[3]=model[i].odd_color[3]=1;
        }
        draws[0].vertices=model;draws[0].ray_caster=true;draws[0].effect_layer=2;
        std::array<vr::SceneVertex,6> ink{};unsigned index=0;
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            auto& v=ink[index++];v.position[0]=.06F+((corner==1 || corner==2)? .075F:-.075F);
            v.position[1]=.05F+(corner>=2?.008F:-.008F);v.position[2]=-.7F;
            v.color[0]=v.odd_color[0]=v.color[3]=v.odd_color[3]=1;
        }
        draws[1].vertices=ink;draws[1].depth_test=false;draws[1].preserve_native_colour=true;
        auto view=camera(eye);const std::array<float,4> clear{.05F,.12F,.22F,1};
        gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());
        check(scene.enqueue_eye(gpu.command,native.texture,native_depth.texture,w,h,view,clear,render::CalibratedScenePhase::before_rays,native_mask.texture),scene.status().c_str());
        check(scene.enqueue_eye(gpu.command,high.texture,high_depth.texture,hw,hh,view,clear,render::CalibratedScenePhase::before_rays,high_mask.texture),scene.status().c_str());
        check(aa.enqueue_supersample(gpu.command,high.texture,high_mask.texture,native.texture,native_mask.texture,target.texture,w,h,factor),aa.status().c_str());
        const auto token=scene.upload_token();gpu.submit();check(scene.notify_submitted(token),"SSAA geometric fixture upload lost ownership");
        const auto low=native.read(),samples=high.read(),actual=target.read();auto expected=low;
        const auto ink_pixel=[](const Bytes& bytes,unsigned at) {return bytes[at]==255 && bytes[at+1]==0 && bytes[at+2]==0;};
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned at=(y*w+x)*4;if(ink_pixel(low,at)) continue;
            std::array<double,3> sum{};unsigned count=0;bool distinct=false;
            const unsigned first=(y*factor*hw+x*factor)*4;
            for(unsigned sy=0;sy<factor;++sy) for(unsigned sx=0;sx<factor;++sx) {
                const unsigned sample=((y*factor+sy)*hw+x*factor+sx)*4;
                if(ink_pixel(samples,sample)) continue;
                distinct|=!std::equal(samples.begin()+sample,samples.begin()+sample+3,samples.begin()+first);
                for(unsigned c=0;c<3;++c) sum[c]+=srgb(format)?decode(samples[sample+c]/255.):samples[sample+c]/255.;
                ++count;
            }
            subpixel_cells+=distinct;
            if(count) for(unsigned c=0;c<3;++c) {
                const double value=sum[c]/count;
                expected[at+c]=static_cast<unsigned char>(std::lround(std::clamp(srgb(format)?encode(value):value,0.,1.)*255));
            }
        }
        for(unsigned at=0;at<actual.size();++at) {
            check(std::abs(int(actual[at])-int(expected[at]))<=unsigned(at%4==3 || ink_pixel(low,(at/4)*4)?0:1),"SSAA actual calibrated raster differs from geometric sample oracle");
            ++compared;
        }
    }
    check(subpixel_cells>0,"Native SSAA geometric reference was only integer enlargement");
    std::cout<<"Native SSAA drawn format "<<unsigned(format)<<": "<<compared<<" sample-oracle checks, "<<subpixel_cells
        <<" cells with distinct true raster samples; perspective triangle, two asymmetric eyes, movement and native thin ink passed.\n";
}
// Independent linear-light box average over deliberately non-upsampled source
// samples. Distinct native ink and high coverage prove protected pixels are
// restored by ownership, not colour matching or an averaged opacity guess.
void ssaa_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    check(render::calibrated_sample_extent_supported(1920,2160,4),"SSAA rejected a bounded native panel extent");
    check(!render::calibrated_sample_extent_supported(3840,2160,4)
        && !render::calibrated_sample_extent_supported(~0U,~0U,4)
        && !render::calibrated_sample_extent_supported(1,1,0),"SSAA extent guard overflowed or accepted unbounded allocations");
    unsigned long long compared=0;unsigned changed=0;
    render::GpuCalibratedAa aa;check(aa.initialize(gpu.device,format),aa.status().c_str());
    for(const auto extent:{std::array{1U,1U},std::array{17U,13U},std::array{65U,37U}}) {
        const auto [w,h]=extent;
        Texture ink(gpu,w,h,format),low_mask(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
        for(unsigned factor:{2U,3U,4U}) {
            const unsigned hw=w*factor,hh=h*factor;
            Texture source(gpu,hw,hh,format),high_mask(gpu,hw,hh,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            Bytes native(w*h*4),low(w*h*4),high(hw*hh*4),coverage(hw*hh*4);
            for(unsigned frame=0;frame<3;++frame) {
                for(unsigned pixel=0;pixel<w*h;++pixel) {
                    for(unsigned c=0;c<4;++c) native[pixel*4+c]=static_cast<unsigned char>(29+(pixel*13+c*43+frame*17)%219);
                    low[pixel*4+2]=frame==2?0:pixel%3;
                }
                for(unsigned pixel=0;pixel<hw*hh;++pixel) {
                    for(unsigned c=0;c<4;++c) high[pixel*4+c]=static_cast<unsigned char>(11+(pixel*37+c*53+frame*29)%241);
                    coverage[pixel*4+2]=frame==1?0:pixel%7?1+pixel%2:0;
                }
                auto expected=native;
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    const unsigned at=(y*w+x)*4;if(!low[at+2]) continue;
                    std::array<double,3> sum{};unsigned count=0;
                    for(unsigned sy=0;sy<factor;++sy) for(unsigned sx=0;sx<factor;++sx) {
                        const unsigned sample=((y*factor+sy)*hw+x*factor+sx)*4;
                        if(!coverage[sample+2]) continue;
                        for(unsigned c=0;c<3;++c) sum[c]+=srgb(format)?decode(high[sample+c]/255.):high[sample+c]/255.;
                        ++count;
                    }
                    if(count) for(unsigned c=0;c<3;++c) {
                        const double value=sum[c]/count;
                        expected[at+c]=static_cast<unsigned char>(std::lround(std::clamp(srgb(format)?encode(value):value,0.,1.)*255));
                    }
                }
                gpu.begin();source.upload(high);high_mask.upload(coverage);ink.upload(native);low_mask.upload(low);
                check(aa.enqueue_supersample(gpu.command,source.texture,high_mask.texture,ink.texture,low_mask.texture,target.texture,w,h,factor),aa.status().c_str());
                gpu.submit();const auto actual=target.read();
                for(unsigned at=0;at<actual.size();++at) {
                    const unsigned tolerance=at%4==3 || !low[(at/4)*4+2] || frame?0:1;
                    check(unsigned(std::abs(int(actual[at])-int(expected[at])))<=tolerance,"Native SSAA differs from linear-light/protected-ink oracle");
                    ++compared;
                }
                changed+=actual!=native;
                gpu.begin();check(aa.enqueue_supersample(gpu.command,source.texture,high_mask.texture,ink.texture,low_mask.texture,target.texture,w,h,factor),aa.status().c_str());
                SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
                gpu.begin();check(aa.enqueue_supersample(gpu.command,source.texture,high_mask.texture,ink.texture,low_mask.texture,target.texture,w,h,factor),aa.status().c_str());
                gpu.submit();check(target.read()==actual,"Native SSAA held/cancelled resolve jittered");
            }
            gpu.begin();
            check(!aa.enqueue_supersample(gpu.command,source.texture,high_mask.texture,ink.texture,low_mask.texture,target.texture,w,h,1),"Native SSAA accepted 1x as supersampling");
            check(!aa.enqueue_supersample(gpu.command,source.texture,high_mask.texture,ink.texture,low_mask.texture,ink.texture,w,h,factor),"Native SSAA accepted aliased final ink");
            check(!aa.enqueue_supersample(gpu.command,source.texture,high_mask.texture,ink.texture,low_mask.texture,target.texture,16384,h,factor),"Native SSAA accepted an overflowing working extent");
            SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        }
    }
    check(changed>0,"SSAA resolve bypassed the high-resolution samples");
    std::cout<<"Native SSAA format "<<unsigned(format)<<": "<<compared<<" linear-light/protected/alpha checks; "<<changed
        <<" changed cases; 4/9/16 samples, odd/tiny extents, exact ink/alpha, no eligible samples, held/cancelled and alias rejection passed.\n";
    ssaa_drawn_tests(gpu,format);
}
