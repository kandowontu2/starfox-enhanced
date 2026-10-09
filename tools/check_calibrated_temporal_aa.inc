// Fixture-only input uploads/readbacks; production uses resident raster guides.
void temporal_float_upload(Texture& texture,std::span<const std::array<float,4>> pixels) {
    auto& gpu=texture.gpu;check(gpu.command && pixels.size()==texture.width*texture.height,"Bad temporal float fixture extent");
    const unsigned bytes=unsigned(pixels.size_bytes());
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};
    auto* upload=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(upload,SDL_GetError());
    auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,upload,false);check(mapped,SDL_GetError());
    std::memcpy(mapped,pixels.data(),bytes);SDL_UnmapGPUTransferBuffer(gpu.device,upload);
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUTextureTransferInfo from{upload,0,texture.width,texture.height};
    const SDL_GPUTextureRegion to{texture.texture,0,0,0,0,0,texture.width,texture.height,1};
    SDL_UploadToGPUTexture(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);SDL_ReleaseGPUTransferBuffer(gpu.device,upload);
}
void temporal_presentation_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    render::GpuCalibratedTemporalAa taa;check(taa.initialize(gpu.device,format),taa.status().c_str());
    std::uint64_t compared{},stationary{},protected_count{};
    for(const auto extent:{std::array{1U,1U},std::array{17U,13U},std::array{65U,37U},std::array{127U,65U}}) {
        const auto [w,h]=extent;
        Texture resolved(gpu,w,h,format),own(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
            surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),ink(gpu,w,h,format),
            centre(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
        Bytes colors(w*h*4),native(w*h*4),coverage(w*h*4),centre_coverage(w*h*4);
        std::vector<std::array<float,4>> surfaces(w*h);
        for(const auto jitter:{std::array{0.F,0.F},std::array{-.31F,.27F},std::array{.19F,-.13F},std::array{.49F,-.49F}}) {
            const auto ramp=[&](double x,double y,unsigned c) {return 30+x*.8+y*.6+c*17;};
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned i=y*w+x,at=i*4;
                coverage[at+3]=(x%11!=2 && y%13!=3)?255:0;
                // Centre ownership intentionally differs from sample ownership.
                centre_coverage[at+3]=(x%11!=4 && y%13!=5)?255:0;
                surfaces[i]={0,0,1,x%17==0?0.F:x%19==1?NAN:500.F};
                for(unsigned c=0;c<3;++c) {
                    native[at+c]=static_cast<unsigned char>(std::lround(ramp(x,y,c)));
                    colors[at+c]=static_cast<unsigned char>(std::lround(ramp(x-jitter[0],y-jitter[1],c)));
                    if(!centre_coverage[at+3]) native[at+c]=static_cast<unsigned char>(221-c*31);
                }
                native[at+3]=static_cast<unsigned char>(17+i%239);colors[at+3]=255;
            }
            auto expected=native;
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned i=y*w+x,at=i*4;if(!centre_coverage[at+3]) continue;
                const double px=std::clamp(double(x)+jitter[0],0.,double(w-1)),py=std::clamp(double(y)+jitter[1],0.,double(h-1));
                const unsigned bx=unsigned(px),by=unsigned(py);const double fx=px-bx,fy=py-by;
                std::array<double,3> rgb{};bool geometry=false;
                for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                    const unsigned tap=std::min(by+dy,h-1)*w+std::min(bx+dx,w-1);
                    const bool owns=coverage[tap*4+3] && surfaces[tap][3]>0 && std::isfinite(surfaces[tap][3]);
                    const double share=(dx?fx:1-fx)*(dy?fy:1-fy);geometry|=owns && share>0;
                    for(unsigned c=0;c<3;++c) {
                        double value=(owns?colors[tap*4+c]:native[at+c])/255.;if(srgb(format)) value=decode(value);rgb[c]+=value*share;
                    }
                }
                if(geometry) for(unsigned c=0;c<3;++c) expected[at+c]=static_cast<unsigned char>(std::lround(
                    std::clamp(srgb(format)?encode(rgb[c]):rgb[c],0.,1.)*255));
            }
            gpu.begin();resolved.upload(colors);own.upload(coverage);ink.upload(native);centre.upload(centre_coverage);temporal_float_upload(surface,surfaces);
            if(w==1 && jitter[0]==0) for(unsigned fault=0;fault<7;++fault) {
                auto bad=jitter;if(fault==0) bad[0]=NAN;if(fault==1) bad[1]=.51F;
                check(!taa.enqueue_presentation(fault==2?nullptr:gpu.command,resolved.texture,own.texture,surface.texture,
                    ink.texture,fault==3?ink.texture:centre.texture,fault==4?resolved.texture:target.texture,
                    fault==5?0:w,fault==6?16385:h,bad),"Native TAA presentation accepted invalid/aliased inputs");
            }
            check(taa.enqueue_presentation(gpu.command,resolved.texture,own.texture,surface.texture,ink.texture,centre.texture,target.texture,w,h,jitter),taa.status().c_str());gpu.submit();
            const auto actual=target.read();compare(actual,expected,"Native TAA presentation differs from independent jitter/linear-light/centre-ink oracle",1);compared+=actual.size();
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned i=y*w+x,at=i*4;check(actual[at+3]==native[at+3],"Native TAA presentation changed centre opacity");
                if(!centre_coverage[at+3]) {check(std::equal(actual.begin()+at,actual.begin()+at+4,native.begin()+at),"Native TAA presentation shifted/blurred centre ink");++protected_count;}
                if(x<1 || x+1>=w || y<1 || y+1>=h || !centre_coverage[at+3]) continue;
                bool clear=true;
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    const unsigned tap=unsigned(int(y)+dy)*w+unsigned(int(x)+dx);
                    clear&=coverage[tap*4+3]!=0 && surfaces[tap][3]>0 && std::isfinite(surfaces[tap][3]);
                }
                if(clear) for(unsigned c=0;c<3;++c) {check(std::abs(int(actual[at+c])-int(native[at+c]))<=1,"Native TAA exposed sample jitter as stationary-ramp wobble");++stationary;}
            }
        }
    }
    check(stationary>1000 && protected_count>1000,"Native TAA presentation fixture missed stable/ink coverage");
    std::cout<<"Native TAA presentation format "<<unsigned(format)<<": "<<compared<<" independent bilinear/linear-light bytes, "<<stationary
        <<" stationary-ramp channels, "<<protected_count<<" exact centre-ink pixels; signed sample offsets, nonfinite/missing depth, opacity and alias/bounds passed. Not native TAA frontend.\n";
}
// An independent authored liquid mask deliberately shares actor depth/motion.
// Water must neither reuse that flow nor seed a newly dry pixel's history.
void temporal_liquid_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=31,h=17,n=w*h;
    std::uint64_t compared{},wet_samples{},dry_responses{},rejected_samples{};unsigned recovered{};
    for(unsigned layout_case=0;layout_case<3;++layout_case) for(bool models:{false,true})
        for(unsigned initial_mask=0;initial_mask<=3;++initial_mask) {
        const bool world=layout_case==2;
        render::GpuCalibratedTemporalAa taa;check(taa.initialize(gpu.device,format),taa.status().c_str());
        Texture source(gpu,w,h,format),ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
            surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
            motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),target(gpu,w,h,format);
        render::TemporalAa reference;
        const auto layout=layout_case?*render::shadows::native_water_layers(w,h,world):render::shadows::NativeWaterLayers{};
        for(unsigned frame=0;frame<12;++frame) {
            // Clearing the mask must not reuse rejected history at the same Z.
            // Classes also change beneath the same persistent ownership mask.
            const unsigned rejected_layers=frame<5?initial_mask:0;
            Bytes colors(n*4),owners(n*4),layers(layout_case?layout.storage_bytes:n*4,0);
            std::vector<std::array<float,4>> surfaces(n),motions(n);
            std::vector<render::TemporalAaGuide> guides(n);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x,at=p*4,kind=(x+frame)%9;
                owners[at]=kind==5?0:kind==3?1:255;
                owners[at+1]=kind==1 || kind==2?127:0;
                owners[at+2]=kind==6?0:kind==7?3:kind==3?1:2;
                owners[at+3]=kind==8?0:255;
                const unsigned marker=frame==8?255:frame==9?0:kind==1 || kind==2?255:kind==3?254:253;
                layers[at+3]=static_cast<unsigned char>(marker);
                // Invalid surface layers must not make refraction borrow actor
                // motion. The radiance marker is the actual liquid coverage.
                const std::array<float,4> wet_guide{NAN,0,0,-1};
                if(layout_case) std::memcpy(layers.data()+layout.surface_offset+p*16,wet_guide.data(),16);
                // Independently specify what the compositor consumes. The ray
                // tag alone must not invalidate protected or nonreflective RGB.
                const bool receiver=owners[at]>0 && (owners[at+2]==1 || owners[at+2]==2);
                const bool wet=receiver && (marker==253 || (owners[at]==1?marker==254:
                    marker==255 && owners[at+1]>0 && models));
                for(unsigned c=0;c<3;++c) colors[at+c]=static_cast<unsigned char>(
                    ((x*3+y*5+frame)%17<8?35:218)+c*7);
                colors[at+3]=static_cast<unsigned char>(29+(p+frame)%221);
                surfaces[p]={0,0,1,600};motions[p]={frame%2?.25F:0.F,0,600,1};
                const bool rejected=(owners[at+2]==1 && (rejected_layers&1U))
                    || (owners[at+2]==2 && (rejected_layers&2U));
                guides[p]={motions[p][0],0,600,600,true,owners[at+3]!=0 && !wet && !rejected};
                wet_samples+=wet && owners[at+3]!=0;
                rejected_samples+=rejected && owners[at+3]!=0 && !wet;
            }
            auto candidate=reference;
            const auto expected=candidate.resolve(w,h,colors,guides,51,.85F);
            gpu.begin();source.upload(colors);ownership.upload(owners);
            temporal_float_upload(surface,surfaces);temporal_float_upload(motion,motions);
            Buffer payload(gpu,layers);
            const render::shadows::GpuReflectionOutput liquid{gpu.device,payload.buffer,w,h,w*4,layout};
            if(frame==0) check(!taa.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,
                motion.texture,target.texture,w,h,{51,.85F,4},&liquid),"Native TAA accepted an unknown rejection class");
            if(frame==0) for(unsigned fault=0;fault<7;++fault) {
                auto bad=liquid;
                if(fault==0) bad.device=nullptr;
                if(fault==1) bad.buffer=nullptr;
                if(fault==2) --bad.width;
                if(fault==3) ++bad.row_bytes;
                if(fault==4) bad.water_layers.surface_offset+=4;
                if(fault==5) bad.water_layers.storage_bytes-=4;
                if(fault==6) bad.water_layers.world_offset=world?0:4;
                check(!taa.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,motion.texture,
                    target.texture,w,h,{51,.85F},&bad),"Native TAA accepted invalid ray device/layout/extent");
            }
            check(taa.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,motion.texture,
                target.texture,w,h,{51,.85F,rejected_layers,{},{},models},&liquid),taa.status().c_str());
            if(frame==3) {check(SDL_CancelGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;taa.discard();++recovered;continue;}
            gpu.submit();const auto actual=target.read();
            compare(actual,expected,"Native liquid TAA differs from independent wet/dry/previous-ownership oracle",srgb(format)?1:0);
            compared+=actual.size();
            for(unsigned p=0;p<n;++p) {
                if(!guides[p].eligible) check(std::equal(actual.begin()+p*4,actual.begin()+p*4+4,colors.begin()+p*4),
                    "Liquid or protected native sample borrowed actor history/opacity");
                else for(unsigned c=0;c<3;++c) dry_responses+=std::abs(int(actual[p*4+c])-int(colors[p*4+c]))>2;
            }
            taa.commit();reference=std::move(candidate);
        }
    }
    check(wet_samples>1000 && dry_responses>100 && rejected_samples>100 && recovered==24,
        "Liquid/layer TAA fixture missed wet/dry/local rejection/history/cancel coverage");
    std::cout<<"Native liquid TAA format "<<unsigned(format)<<": "<<compared<<" independent history bytes, "<<wet_samples
        <<" reactive liquid samples, "<<dry_responses<<" dry-history responses, "<<rejected_samples
        <<" ownership-local rejected samples; all four rejection masks, rejected-to-clean same-depth transitions, compact/full layouts, wet-to-dry same-depth transitions, "
        "invalid liquid normals, consumed/unconsumed model and floor reflections, reflections OFF, RGBA-only/compact/full layouts, protected classes, opacity, malformed inputs and cancelled work passed.\n";
}
void temporal_aa_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    std::uint64_t compared{},protected_count{};unsigned changed{},recovered{};
    std::array<render::GpuCalibratedTemporalAa,2> taa;
    for(auto& t:taa) check(t.initialize(gpu.device,format),t.status().c_str());
    for(const auto extent:{std::array{1U,1U},std::array{17U,13U},std::array{65U,37U},std::array{127U,65U}}) {
        const auto [w,h]=extent;
        std::array<std::unique_ptr<Texture>,2> source,ownership,surface,motion,target;
        std::array<render::TemporalAa,2> reference;
        std::array<Bytes,2> colors{Bytes(w*h*4),Bytes(w*h*4)},coverage{Bytes(w*h*4),Bytes(w*h*4)};
        std::array<std::vector<render::TemporalAaGuide>,2> guides;
        std::array<std::vector<std::array<float,4>>,2> surfaces,motions;
        std::array<float,2> accepted_depth{};std::array<std::uint64_t,2> epochs{1,71};
        for(unsigned e=0;e<2;++e) {
            source[e]=std::make_unique<Texture>(gpu,w,h,format);target[e]=std::make_unique<Texture>(gpu,w,h,format);
            ownership[e]=std::make_unique<Texture>(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            surface[e]=std::make_unique<Texture>(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
            motion[e]=std::make_unique<Texture>(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
            guides[e].resize(w*h);surfaces[e].resize(w*h);motions[e].resize(w*h);taa[e].reset();
        }
        for(unsigned frame=0;frame<29;++frame) {
            if(frame==7) for(auto& epoch:epochs) ++epoch;
            if(frame==15) for(unsigned e=0;e<2;++e) {taa[e].reset();reference[e].reset();}
            const float weight=frame==11?0.F:frame==12?-2.F:frame==13?2.F:frame%3==0?.65F:frame%3==1?.8F:.9F;
            std::array<Bytes,2> expected;auto candidates=reference;
            std::array<float,2> next_depth{};
            for(unsigned e=0;e<2;++e) {
                next_depth[e]=500.F+frame*11+e*170;
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    const unsigned i=y*w+x,at=i*4;const bool eligible=w==1 || (x+frame+e)%11!=2 && (y+e)%13!=3;
                    coverage[e][at+2]=eligible?2:0;coverage[e][at+3]=eligible?255:0;
                    for(unsigned c=0;c<3;++c) colors[e][at+c]=static_cast<unsigned char>(
                        ((x*3+y*5+frame+e*7)%17<8?34:219)+c*(e?-9:7));
                    if(frame==24) for(unsigned c=0;c<3;++c) colors[e][at+c]=static_cast<unsigned char>(60+c*13);
                    colors[e][at+3]=static_cast<unsigned char>(31+(x*17+y*11+frame+e*3)%225);
                    if(!eligible) {colors[e][at]=255;colors[e][at+1]=17;colors[e][at+2]=3;}
                    auto& g=guides[e][i];g={0,0,next_depth[e],accepted_depth[e],true,eligible};
                    if(frame==2 || frame==3 || frame==14 || frame==27) {g.motion_x=e?-.25F:.25F;g.motion_y=-.5F;}
                    if(frame==4) g.previous_depth+=90; // Current Z cannot substitute for previous-camera depth.
                    if(frame==5 && x%3==0) g.valid=false;
                    if(frame==6 && x%4==0) g.motion_x=NAN;
                    if(frame==9 && x%3==0) g.depth=0;
                    if(frame==10 && x%4==0) g.depth=NAN;
                    if(frame==25) g.motion_x=float(w)*2;
                    if(frame==26 && x%2==0) g.previous_depth+=g.previous_depth*.02F;
                    if(frame==28 && x%3==0) g.previous_depth=INFINITY;
                    surfaces[e][i]={0,0,1,g.depth};motions[e][i]={g.motion_x,g.motion_y,g.previous_depth,g.valid?1.F:0.F};
                }
                expected[e]=candidates[e].resolve(w,h,colors[e],guides[e],epochs[e]+(frame==16 && e==0?111:0),weight);
            }
            gpu.begin();
            for(unsigned e=0;e<2;++e) {
                source[e]->upload(colors[e]);ownership[e]->upload(coverage[e]);
                temporal_float_upload(*surface[e],surfaces[e]);temporal_float_upload(*motion[e],motions[e]);
                const render::CalibratedTemporalAaSettings settings{epochs[e]+(frame==16 && e==0?111:0),weight};
                if(frame==0 && e==0) {
                    for(unsigned fault=0;fault<7;++fault) {
                        auto bad=settings;if(fault==0) bad.weight=NAN;
                        check(!taa[e].enqueue(fault==1?nullptr:gpu.command,source[e]->texture,ownership[e]->texture,
                            surface[e]->texture,fault==2?nullptr:motion[e]->texture,fault==3?source[e]->texture:target[e]->texture,
                            fault==4?0:fault==5?16385:fault==6?16384:w,fault==6?16384:h,bad),"Native TAA accepted invalid/aliased/unbounded inputs");
                    }
                }
                check(taa[e].enqueue(gpu.command,source[e]->texture,ownership[e]->texture,surface[e]->texture,
                    motion[e]->texture,target[e]->texture,w,h,settings),taa[e].status().c_str());
                check(!taa[e].enqueue(gpu.command,source[e]->texture,ownership[e]->texture,surface[e]->texture,
                    motion[e]->texture,target[e]->texture,w,h,settings),"Native TAA overwrote a pending presentation");
            }
            if(frame==18) {SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;for(auto& t:taa) t.discard();continue;}
            if(frame==22) taa[0].reset(); // In-flight old work must not revive a reset history.
            gpu.submit();
            for(unsigned e=0;e<2;++e) {
                const auto actual=target[e]->read();compare(actual,expected[e],"Native TAA differs from independent CPU history/visibility/clipping resolve",srgb(format)?1:0);
                compared+=actual.size();changed+=actual!=colors[e];
                for(unsigned i=0;i<w*h;++i) if(!guides[e][i].eligible) {
                    check(std::equal(actual.begin()+i*4,actual.begin()+i*4+4,colors[e].begin()+i*4),"Native TAA changed protected ink/opacity");++protected_count;
                }
                // Reject only the left eye to expose accidentally shared commit/history.
                if(frame==16 && e==0) {taa[e].discard();++recovered;}
                else {
                    taa[e].commit();if(frame==22 && e==0) reference[e].reset();else reference[e]=std::move(candidates[e]);
                    accepted_depth[e]=next_depth[e];
                }
            }
        }
        // Both cancelled and submitted-but-rejected RESIZES must preserve the
        // old extent's accepted history. Returning to it must not silently reset.
        const unsigned rw=w+2,rh=h+1;
        Texture rs(gpu,rw,rh,format),ro(gpu,rw,rh,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
            rf(gpu,rw,rh,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),rm(gpu,rw,rh,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),rt(gpu,rw,rh,format);
        Bytes resized(rw*rh*4,187),resized_owner(rw*rh*4,255);
        std::vector<std::array<float,4>> resized_surface(rw*rh,{0,0,1,700}),resized_motion(rw*rh,{0,0,700,1});
        for(unsigned rejected=0;rejected<2;++rejected) {
            gpu.begin();rs.upload(resized);ro.upload(resized_owner);temporal_float_upload(rf,resized_surface);temporal_float_upload(rm,resized_motion);
            check(taa[0].enqueue(gpu.command,rs.texture,ro.texture,rf.texture,rm.texture,rt.texture,rw,rh,{epochs[0],.8F}),taa[0].status().c_str());
            if(!rejected) {SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;}
            else {gpu.submit();compare(rt.read(),resized,"Resized first native TAA frame was not exact",0);}
            taa[0].discard();
            for(auto& g:guides[0]) {g.depth=accepted_depth[0]+11;g.previous_depth=accepted_depth[0];g.motion_x=g.motion_y=0;g.valid=true;}
            for(unsigned i=0;i<w*h;++i) {surfaces[0][i]={0,0,1,guides[0][i].depth};motions[0][i]={0,0,guides[0][i].previous_depth,1};}
            const auto expected=reference[0].resolve(w,h,colors[0],guides[0],epochs[0],.8F);
            gpu.begin();source[0]->upload(colors[0]);ownership[0]->upload(coverage[0]);temporal_float_upload(*surface[0],surfaces[0]);temporal_float_upload(*motion[0],motions[0]);
            check(taa[0].enqueue(gpu.command,source[0]->texture,ownership[0]->texture,surface[0]->texture,motion[0]->texture,target[0]->texture,w,h,{epochs[0],.8F}),taa[0].status().c_str());gpu.submit();
            compare(target[0]->read(),expected,"Rejected resize lost/contaminated accepted native TAA history",srgb(format)?1:0);
            taa[0].commit();accepted_depth[0]+=11;++recovered;compared+=expected.size();
        }
        // A completed resize really resets, then supports history at that size.
        gpu.begin();check(taa[0].enqueue(gpu.command,rs.texture,ro.texture,rf.texture,rm.texture,rt.texture,rw,rh,{epochs[0],.8F}),taa[0].status().c_str());gpu.submit();
        compare(rt.read(),resized,"Committed native TAA resize retained incompatible history",0);taa[0].commit();
    }
    check(changed>20 && recovered==12 && protected_count>10000,"Native TAA fixture lacked accumulation/recovery/protected coverage");
    for(auto& t:taa) {t.release_device();check(!t.enqueue(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,1,1,{}),"Released native TAA encoded");}
    std::cout<<"Native TAA resolve format "<<unsigned(format)<<": "<<compared<<" CPU-oracle colour bytes, "<<protected_count
        <<" exact protected pixels; two independent eyes, fractional motion, previous-camera depth, occlusion/invalid/nonfinite rejection, neighbourhood clipping, weight/epoch/reset, cancelled/rejected frames and resizes, alpha and bounds passed. Resolve core, not native TAA frontend.\n";
    temporal_presentation_tests(gpu,format);
}
