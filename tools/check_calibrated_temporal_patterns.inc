// Fixture-only uploads/readbacks and independent scalar history/phase oracle.
void temporal_pattern_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=35,h=25,n=w*h;
    std::uint64_t compared=0,responses=0,crossings=0,protected_pixels=0,presentation_fallbacks=0;
    unsigned recoveries=0,descriptor_resets=0;
    for(unsigned effect:{5U,10U,25U,70U}) for(unsigned scale:{1U,2U,4U})
        for(unsigned scope:{1U,2U,3U}) for(unsigned intensity:{35U,100U}) {
        render::GpuCalibratedTemporalAa taa;check(taa.initialize(gpu.device,format),taa.status().c_str());
        render::TemporalAa reference;render::CalibratedPatternGuide accepted_patterns;bool accepted=false;
        unsigned selected_pattern_responses=0;
        Texture source(gpu,w,h,format),ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
            surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
            motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),target(gpu,w,h,format),
            centre(gpu,w,h,format),centre_ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),presented(gpu,w,h,format);
        std::vector<render::TemporalAaGuide> previous_guides;
        for(unsigned frame=0;frame<12;++frame) {
            std::array<render::CalibratedPostEffects,3> passes{};
            passes[0]={scope&1?effect:0,scope&2?effect:0,intensity,intensity};
            if(frame==6 || frame==10) {passes[0].world_intensity=passes[0].model_intensity=intensity==35?100:35;}
            if(frame==7 || frame==8) {passes[1]={70,25,35,100};passes[2]={10,5,100,35};}
            const auto patterns=render::calibrated_pattern_guide(passes,frame==4?scale+2:frame==8?scale+1:scale);
            std::array<std::array<unsigned,2>,3> effects{};
            for(unsigned j=0;j<3;++j) effects[j]={passes[j].world,passes[j].model};
            auto candidate=reference;
            const bool descriptor_changed=accepted && patterns!=accepted_patterns;
            if(descriptor_changed) {candidate.reset();++descriptor_resets;}
            const auto layout=*render::shadows::native_water_layers(w,h,false);
            Bytes colors(n*4),owners(n*4),liquid_bytes(layout.storage_bytes,0);
            std::vector<std::array<float,4>> surfaces(n,{0,0,1,600}),motions(n);
            std::vector<render::TemporalAaGuide> guides(n);
            std::vector<bool> crossed_history(n);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x,at=p*4,layer=x<w/2?1:2;
                owners[at]=255;owners[at+2]=layer;owners[at+3]=x%17==0?0:255;
                const bool wet=y==h/2 && x%4<2;liquid_bytes[at+3]=wet?253:254;
                // Distinct receiver/grid phases plus a local scalar colour ramp
                // make valid history observable without widening clamp bounds.
                const auto phase=pattern_oracle_phase(x,y,patterns.scale,layer,effects);
                for(unsigned c=0;c<3;++c) colors[at+c]=static_cast<unsigned char>(
                    35+x*2+y+phase%7*2+c*13+(frame%3)*4);
                colors[at+3]=static_cast<unsigned char>(31+p%219);
                float mx=0,my=0;
                if(frame==2) {mx=float((effect==70?3:effect==5?4:1)*patterns.scale);}
                if(frame==3) my=float(patterns.scale); // Intentional phase crossing.
                if(frame==5) {mx=.25F;my=.25F;} // Mixed positive-weight footprint.
                if(frame==9) my=float((effect==70?3:effect==5?4:2)*patterns.scale);
                guides[p]={mx,my,600,600,true,owners[at+3]!=0 && !wet,phase};motions[p]={mx,my,600,1};
                if(accepted && guides[p].eligible && patterns==accepted_patterns) {
                    const float px=x+mx,py=y+my;
                    if(px>=0 && py>=0 && px<w-1 && py<h-1) {
                        const unsigned bx=unsigned(px),by=unsigned(py);bool crossing=false;
                        for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                            const auto& old=previous_guides[(by+dy)*w+bx+dx];
                            const float share=(dx?px-bx:1-(px-bx))*(dy?py-by:1-(py-by));
                            crossing|=share>0 && old.eligible && old.pattern_phase!=phase;
                        }
                        crossings+=crossing;crossed_history[p]=crossing;
                    }
                }
            }
            const auto expected=candidate.resolve(w,h,colors,guides,71,.8F);
            gpu.begin();source.upload(colors);ownership.upload(owners);centre_ownership.upload(owners);centre.upload(colors);
            temporal_float_upload(surface,surfaces);temporal_float_upload(motion,motions);
            Buffer payload(gpu,liquid_bytes);
            const render::shadows::GpuReflectionOutput liquid{gpu.device,payload.buffer,w,h,w*4,layout};
            const render::CalibratedTemporalAaSettings settings{71,.8F,0,patterns};
            if(frame==0) for(unsigned fault=0;fault<4;++fault) {
                auto bad=settings;
                if(fault==0) bad.patterns.scale=0;if(fault==1) bad.patterns.passes[1].model=7;
                if(fault==2) bad.patterns.passes[0].world_intensity=101;
                if(fault==3) bad.patterns.passes[0].model_intensity=0;
                // Force an invalid active/inactive pair irrespective of scope.
                if(fault==3) bad.patterns.passes[0].model=70;
                check(!taa.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,motion.texture,
                    target.texture,w,h,bad,&liquid),"TAA accepted malformed pattern witness");
            }
            check(taa.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,motion.texture,
                target.texture,w,h,settings,&liquid),taa.status().c_str());
            if(frame==4) {check(SDL_CancelGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;taa.discard();++recoveries;continue;}
            gpu.submit();const auto actual=target.read();
            compare(actual,expected,"Point-pattern TAA differs from independent phase/accepted-history oracle",srgb(format)?1:0);
            if(descriptor_changed) compare(actual,colors,"Pattern/grid/intensity change reused incompatible history",0);
            compared+=actual.size();
            for(unsigned p=0;p<n;++p) {
                check(actual[p*4+3]==colors[p*4+3],"Point-pattern TAA changed opacity");
                if(!guides[p].eligible || crossed_history[p]) {
                    check(std::equal(actual.begin()+p*4,actual.begin()+p*4+4,colors.begin()+p*4),"Pattern witness admitted liquid/UI history");++protected_pixels;
                }
                for(unsigned c=0;c<3;++c) {
                    const bool changed=std::abs(int(actual[p*4+c])-int(colors[p*4+c]))>1;
                    responses+=changed;
                    selected_pattern_responses+=changed && guides[p].pattern_phase && frame==1;
                }
            }
            // Independently reconstruct the centre grid. Any positive eligible
            // geometry tap with a different pattern must keep centre RGB exact.
            const std::array jitter{.25F,-.375F};Bytes expected_present=colors;
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x,at=p*4;if(!owners[at+3]) continue;
                const double px=std::clamp(x+double(jitter[0]),0.,double(w-1)),py=std::clamp(y+double(jitter[1]),0.,double(h-1));
                const unsigned bx=unsigned(px),by=unsigned(py);const double fx=px-bx,fy=py-by;
                bool crossed=false;std::array<double,3> rgb{};
                for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                    const unsigned tap=std::min(by+dy,h-1)*w+std::min(bx+dx,w-1);
                    const bool owns=owners[tap*4+3]!=0;const double share=(dx?fx:1-fx)*(dy?fy:1-fy);
                    crossed|=owns && share>0 && guides[tap].pattern_phase!=guides[p].pattern_phase;
                    for(unsigned c=0;c<3;++c) {
                        double v=(owns?expected[tap*4+c]:colors[at+c])/255.;rgb[c]+=(srgb(format)?decode(v):v)*share;
                    }
                }
                if(crossed) {++presentation_fallbacks;continue;}
                for(unsigned c=0;c<3;++c) expected_present[at+c]=static_cast<unsigned char>(std::lround(
                    std::clamp(srgb(format)?encode(rgb[c]):rgb[c],0.,1.)*255));
            }
            gpu.begin();check(taa.enqueue_presentation(gpu.command,target.texture,ownership.texture,surface.texture,
                centre.texture,centre_ownership.texture,presented.texture,w,h,jitter,patterns),taa.status().c_str());gpu.submit();
            compare(presented.read(),expected_present,"Point-pattern de-jitter blurred across a phase/receiver boundary",1);
            if(frame==10) {taa.discard();++recoveries;continue;}
            taa.commit();reference=std::move(candidate);accepted_patterns=patterns;previous_guides=guides;accepted=true;
        }
        if(scale>1) check(selected_pattern_responses>10,"A supported selected point pattern still blanket-rejected history");
    }
    check(responses>1000 && crossings>1000 && protected_pixels>1000 && presentation_fallbacks>1000
        && recoveries==144 && descriptor_resets>100,"Pattern TAA fixture lacks reuse/crossing/recovery/descriptor coverage");
    std::cout<<"Point-pattern TAA format "<<unsigned(format)<<": "<<compared<<" independent history bytes, "<<responses
        <<" same-phase responses, "<<crossings<<" crossed footprints, "<<protected_pixels<<" exact wet/UI pixels, "
        <<presentation_fallbacks<<" centre-pattern fallbacks; 4 effects, 3 scales, both receivers, partial/full intensity,"
        " ordered mixed passes, intensity/grid changes, cancelled/rejected candidates and alpha passed. Component, not frontend.\n";
}
