// Uses the existing RGBA-normalized fixture, independent flat CPU meter and
// real queue. Readback and uploads occur only in this diagnostic.
void exposure_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    unsigned long long compared{};unsigned changed{};
    for(const auto extent:{std::array{1U,1U},std::array{33U,17U},std::array{65U,37U},std::array{127U,65U}}) {
        const auto [w,h]=extent;Texture source(gpu,w,h,format),ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
        render::GpuCalibratedExposure exposure;const bool initialized=exposure.initialize(gpu.device,format);check(initialized,exposure.status().c_str());
        check(exposure.working_image_bytes()==8,"Exposure initialization omitted or duplicated its two scalar stops");
        render::AdaptiveExposure reference;render::Framebuffer tags(w,h);tags.enable_layer_tags(true);
        Bytes color(w*h*4),coverage(w*h*4);unsigned quality=1;float seconds=10;std::uint64_t epoch=1;
        for(unsigned frame=0;frame<24;++frame) {
            // Dark-to-bright adaptation, pause/resume, cuts, elapsed-time gap,
            // quality change, clock regression, transparent/black exclusions.
            if(frame==8) ++quality;if(frame==16) ++quality;if(frame==6) ++epoch;
            seconds+=frame==20?2.F:frame==22?-.5F:.1F;const bool paused=frame==4 || frame==5;
            for(unsigned at=0;at<color.size();at+=4) {
                const unsigned i=at/4;coverage[at+2]=(i%11==0 && w>1)?0:unsigned(i%2+1);
                const unsigned value=frame<12?23:177;
                for(unsigned c=0;c<3;++c) color[at+c]=coverage[at+2]?static_cast<unsigned char>(std::min(255U,value+(i*13+c*17)%55)):255;
                if(i%29==1) for(unsigned c=0;c<3;++c) color[at+c]=0;
                if(i%37==2) for(unsigned c=0;c<3;++c) color[at+c]=255;
                color[at+3]=i%17==3?0:static_cast<unsigned char>(55+i%201);
                tags.layer_tags()[i]=unsigned(coverage[at+2]?render::PixelLayer::background:render::PixelLayer::two_d);
            }
            const Bytes original=color;Bytes expected=color;reference.apply(tags,expected,quality,seconds,epoch,paused);
            gpu.begin();source.upload(color);ownership.upload(coverage);
            const bool encoded=exposure.enqueue(gpu.command,source.texture,ownership.texture,target.texture,w,h,{quality,seconds,epoch,paused});check(encoded,exposure.status().c_str());
            check(!exposure.enqueue(gpu.command,source.texture,ownership.texture,target.texture,w,h,{quality,seconds,epoch,paused}),"Pending native exposure encode overwritten");
            gpu.submit();const auto actual=target.read();exposure.commit();
            const std::uint64_t tiles=((w+31)/32)*((h+31)/32);
            check(exposure.working_image_bytes()==8+tiles*256,
                "Exposure memory accounting lost the retained tile histogram/stops");
            compare(actual,expected,"Native exposure differs from elapsed-time flat CPU meter",1);compared+=w*h;
            changed+=actual!=original;
            for(unsigned at=0;at<actual.size();at+=4) {
                if(!coverage[at+2] || !color[at+3]) check(std::equal(actual.begin()+at,actual.begin()+at+4,color.begin()+at),"Native exposure changed HUD/emissive/transparent ink");
                check(actual[at+3]==color[at+3],"Native exposure changed source opacity");
            }
        }
        gpu.begin();
        for(unsigned fault=0;fault<8;++fault) {
            render::CalibratedExposureSettings bad{2,seconds,epoch,false};if(fault==0) bad.quality=0;if(fault==1) bad.quality=4;
            if(fault==2) bad.seconds=NAN;if(fault==3) bad.seconds=-1;
            check(!exposure.enqueue(fault==7?nullptr:gpu.command,source.texture,ownership.texture,fault==4?source.texture:target.texture,
                fault==5?0:w,fault==6?16385:h,bad),"Invalid native exposure settings accepted");
        }
        check(exposure.enqueue(gpu.command,source.texture,ownership.texture,target.texture,w,h,{2,seconds,epoch,false}),exposure.status().c_str());
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;exposure.discard();reference.reset();
        gpu.begin();check(exposure.enqueue(gpu.command,source.texture,ownership.texture,target.texture,w,h,{2,seconds,epoch,false}),exposure.status().c_str());gpu.submit();
        compare(target.read(),color,"Cancelled native exposure contaminated neutral restart",0);exposure.commit();
        check(!exposure.enqueue(nullptr,source.texture,ownership.texture,target.texture,w,h,{2,seconds,epoch,false}),"Null native exposure command accepted");
        exposure.release_device();check(!exposure.enqueue(nullptr,nullptr,nullptr,nullptr,w,h,{}),"Released exposure encoded");
        check(!exposure.working_image_bytes(),"Released exposure meter images remained charged");
    }
    // Independent meters in one command. Opposite luminance histories must
    // not share stops, histogram buffers, timing or commit/discard flags.
    {
        constexpr unsigned w=35,h=34;Texture ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        Texture left(gpu,w,h,format),right(gpu,w,h,format),out_left(gpu,w,h,format),out_right(gpu,w,h,format);
        render::GpuCalibratedExposure a,b;check(a.initialize(gpu.device,format) && b.initialize(gpu.device,format),"Independent exposure initialization failed");
        render::AdaptiveExposure ref_a,ref_b;render::Framebuffer tags(w,h);tags.enable_layer_tags(true);std::fill(tags.layer_tags().begin(),tags.layer_tags().end(),unsigned(render::PixelLayer::background));
        Bytes colors[2]{Bytes(w*h*4),Bytes(w*h*4)},coverage(w*h*4);for(unsigned at=0;at<coverage.size();at+=4) {
            coverage[at+2]=1;for(unsigned c=0;c<3;++c) {colors[0][at+c]=32;colors[1][at+c]=210;}colors[0][at+3]=colors[1][at+3]=255;
        }
        gpu.begin();left.upload(colors[0]);right.upload(colors[1]);ownership.upload(coverage);gpu.submit();
        for(unsigned i=0;i<8;++i) {
            const float seconds=i*.1F;Bytes expected_a=colors[0],expected_b=colors[1];ref_a.apply(tags,expected_a,3,seconds,0);ref_b.apply(tags,expected_b,3,seconds,0);
            gpu.begin();check(a.enqueue(gpu.command,left.texture,ownership.texture,out_left.texture,w,h,{3,seconds,0,false})
                && b.enqueue(gpu.command,right.texture,ownership.texture,out_right.texture,w,h,{3,seconds,0,false}),"Independent exposure encode failed");gpu.submit();
            compare(out_left.read(),expected_a,"Right eye overwrote left exposure",1);compare(out_right.read(),expected_b,"Left eye overwrote right exposure",1);
            a.commit();b.commit();compared+=2*w*h;
        }
    }
    // Crossing the histogram's 4096-tile packing boundary, without a texture
    // whose height exceeds GPU limits. Also prove a resize restarts exposure.
    if(format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) {
        render::GpuCalibratedExposure exposure;check(exposure.initialize(gpu.device,format),"Packed exposure initialization failed");
        std::uint64_t retained=8;
        for(auto extent:{std::array{2081U,2049U},std::array{7U,3U}}) {
            const auto [w,h]=extent;Texture source(gpu,w,h,format),ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
            retained+=w==2081?2097152:256; // Actual padded 128x4096 / 64x1 R32 tile allocations.
            Bytes color(w*h*4),coverage(w*h*4);for(unsigned at=0;at<color.size();at+=4) {color[at]=color[at+1]=color[at+2]=48;color[at+3]=255;coverage[at+2]=1;}
            render::AdaptiveExposure reference;render::Framebuffer tags(w,h);tags.enable_layer_tags(true);std::fill(tags.layer_tags().begin(),tags.layer_tags().end(),unsigned(render::PixelLayer::background));
            gpu.begin();source.upload(color);ownership.upload(coverage);gpu.submit();
            for(unsigned i=0;i<2;++i) {
                Bytes expected=color;reference.apply(tags,expected,2,.1F*i,0);gpu.begin();check(exposure.enqueue(gpu.command,source.texture,ownership.texture,target.texture,w,h,{2,.1F*i,0,false}),exposure.status().c_str());gpu.submit();
                compare(target.read(),expected,"Packed histogram or resized exposure differs from CPU meter",1);exposure.commit();compared+=w*h;
                check(exposure.working_image_bytes()==retained,
                    "Exposure accounting omitted packed-tile padding or a retained previous extent");
            }
        }
    }
    check(changed>10,"Exposure fixtures produced no visible adaptation");
    std::cout<<"  Native exposure: "<<compared<<" CPU-oracle pixels, LOW/MED/HIGH, dark/bright trimmed histograms, protected/black/transparent exclusions, pause/cuts/quality/time resets, independent same-command eyes, pending/cancel/release validation and packed tiles passed"<<std::endl;
}
