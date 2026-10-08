// Fixture-only readbacks: independent PRE-effect branches and scalar history.
void temporal_edge_fault_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=16,h=8,n=w*h;
    render::GpuCalibratedTemporalAa taa;check(taa.initialize(gpu.device,format),taa.status().c_str());
    Texture source(gpu,w,h,format),own(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),target(gpu,w,h,format),atlas(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32_FLOAT);
    const std::array<render::CalibratedPostEffects,3> passes{{{3,3,100,100},{},{}}};
    const render::CalibratedTemporalAaSettings settings{17,.8F,0,{},render::calibrated_edge_guide(passes,1)};
    Bytes base(n*4),owners(n*4),witness(n*4);std::vector<std::array<float,4>> surfaces(n,{0,0,1,600}),motions(n,{0,0,600,1});
    for(unsigned p=0;p<n;++p) {for(unsigned c=0;c<3;++c) base[p*4+c]=static_cast<unsigned char>(20+p%w*7+c*3);
        base[p*4+3]=255;owners[p*4]=owners[p*4+3]=255;owners[p*4+2]=p<w*h/2?1:2;}
    for(float invalid:{NAN,INFINITY,-1.F,512.F,.5F,0.F,2.F,9.F}) {
        taa.reset();
        for(unsigned frame=0;frame<3;++frame) {
            auto colours=base;for(unsigned p=0;p<n;++p) for(unsigned c=0;c<3;++c) colours[p*4+c]+=static_cast<unsigned char>(frame*5);
            const float code=frame==0?invalid:1.F;for(unsigned p=0;p<n;++p) std::memcpy(witness.data()+p*4,&code,4);
            gpu.begin();source.upload(colours);own.upload(owners);atlas.upload(witness);temporal_float_upload(surface,surfaces);temporal_float_upload(motion,motions);
            if(frame==1) check(!taa.enqueue(gpu.command,source.texture,own.texture,surface.texture,motion.texture,target.texture,w,h,settings,nullptr,
                taa.accepted_colour(w,h,17)),"TAA edge input aliased accepted colour");
            check(taa.enqueue(gpu.command,source.texture,own.texture,surface.texture,motion.texture,target.texture,w,h,settings,nullptr,atlas.texture),taa.status().c_str());gpu.submit();
            const auto actual=target.read();if(frame<2) compare(actual,colours,"Malformed edge atlas reused/seeded same-depth history",0);
            else check(actual!=colours,"Valid edge atlas never recovered genuine history after invalid candidate");
            taa.commit();
        }
    }
    render::GpuCalibratedRayComposite compositor;check(compositor.initialize(gpu.device,format),compositor.status().c_str());
    std::array<void*,4> banks{};std::array<Bytes,4> captured;
    for(unsigned bank=0;bank<4;++bank) {
        auto colours=base;for(unsigned p=0;p<n;++p) if(p%4==bank) for(unsigned c=0;c<3;++c) colours[p*4+c]=240;
        gpu.begin();source.upload(colours);check(compositor.enqueue_sequence(gpu.command,source.texture,own.texture,target.texture,w,h,nullptr,nullptr,1,1,0,1,passes,&banks[bank],bank),compositor.status().c_str());gpu.submit();
        captured[bank]=atlas.read(static_cast<SDL_GPUTexture*>(banks[bank]));
        for(unsigned prior=0;prior<bank;++prior) check(banks[prior]!=banks[bank] && atlas.read(static_cast<SDL_GPUTexture*>(banks[prior]))==captured[prior],
            "Equal-sized eyes' centre/AA atlas banks alias or overwrite retained witness");
    }
    gpu.begin();void* cleared=banks[0];check(!compositor.enqueue_sequence(gpu.command,source.texture,own.texture,target.texture,w,h,nullptr,nullptr,1,1,0,1,passes,&cleared,4)
        && !cleared,"Invalid witness bank retained borrowed output");
    check(compositor.enqueue_sequence(gpu.command,source.texture,own.texture,target.texture,w,h,nullptr,nullptr,1,1,0,1,{},&cleared) && !cleared,
        "Effects OFF returned stale witness");gpu.submit();
}
void temporal_edge_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    temporal_edge_fault_tests(gpu,format);
    constexpr unsigned w=37,h=29,n=w*h;
    std::uint64_t bytes=0,responses=0,crossings=0,centre_fallbacks=0;unsigned recoveries=0;
    for(unsigned style:edge_oracle_styles) for(unsigned scale:{1U,3U}) for(unsigned scope:{1U,2U,3U}) for(unsigned intensity:{35U,100U}) {
        render::GpuCalibratedRayComposite compositor;check(compositor.initialize(gpu.device,format),compositor.status().c_str());
        render::GpuCalibratedTemporalAa taa;check(taa.initialize(gpu.device,format),taa.status().c_str());
        Texture raw(gpu,w,h,format),plain(gpu,w,h,format),intermediate(gpu,w,h,format),styled(gpu,w,h,format),
            own(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
            motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),resolved(gpu,w,h,format),centre_raw(gpu,w,h,format),
            centre(gpu,w,h,format),centre_own(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),presented(gpu,w,h,format),
            atlas_read(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32_FLOAT);
        render::TemporalAa reference;render::CalibratedEdgeGuide accepted_edges;render::CalibratedPatternGuide accepted_patterns;
        bool accepted=false;unsigned selected_responses=0;std::vector<render::TemporalAaGuide> prior_guides;
        for(unsigned frame=0;frame<12;++frame) {
            std::array<render::CalibratedPostEffects,3> passes{};
            // Empty original slots MUST NOT be compacted in the witness.
            passes[frame==6?2:0]={scope&1?style:0,scope&2?style:0,intensity,intensity};
            if(frame==7 || frame==8) {passes[1]={6,39,35,100};passes[2]={25,3,100,35};}
            if(frame==9) passes[1]={50,0,100,0}; // WORLD warp stays reactive.
            if(frame==10) passes[0].world_intensity=passes[0].model_intensity=intensity==35?100:35;
            const unsigned step=frame==4?scale+1:scale;
            const auto edges=render::calibrated_edge_guide(passes,step);const auto patterns=render::calibrated_pattern_guide(passes,step);
            const auto masks=edges.masks();const unsigned rejected=frame==9?1:0;
            std::array<std::array<unsigned,2>,3> pattern_effects{};
            for(unsigned j=0;j<3;++j) pattern_effects[j]={passes[j].world_intensity?passes[j].world:0,passes[j].model_intensity?passes[j].model:0};
            Bytes colors(n*4),owners(n*4),layers(n);std::vector<std::array<float,4>> surfaces(n,{0,0,1,600}),motions(n);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x,at=p*4,layer=y==0 || x%19==0?0:x<w/2?1:2;
                layers[p]=static_cast<unsigned char>(layer);owners[at]=255;owners[at+2]=layers[p];owners[at+3]=layer && x%17?255:0;
                for(unsigned c=0;c<3;++c) colors[at+c]=static_cast<unsigned char>(15+x*3+y*2+c*9+(frame%3)*6+(x%11<3?75:0));
                colors[at+3]=static_cast<unsigned char>(31+p%219);motions[p]={frame==2?.25F:frame==3?float(step):0,frame==2?.375F:0,600,1};
            }
            gpu.begin();raw.upload(colors);own.upload(owners);centre_own.upload(owners);temporal_float_upload(surface,surfaces);temporal_float_upload(motion,motions);gpu.submit();
            // Accumulate each ordered pass from its plain PRE-effect RGB.
            auto input=colors;std::vector<unsigned> bits(n);void* plain_input=raw.texture;
            render::Framebuffer tags(w,h,1);tags.enable_layer_tags(true);
            for(unsigned p=0;p<n;++p) tags.layer_tags()[p]=unsigned(layers[p]==1?render::PixelLayer::background:layers[p]==2?render::PixelLayer::three_d:render::PixelLayer::two_d);
            for(unsigned j=0;j<3;++j) if(passes[j].active()) {
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    const unsigned p=y*w+x,layer=layers[p];const auto& pass=passes[j];
                    const unsigned effect=layer==1?pass.world:layer==2?pass.model:0,amount=layer==1?pass.world_intensity:pass.model_intensity;
                    bits[p]|=(amount?edge_oracle_branch(input,layers,w,h,x,y,step,effect):0)<<(j*3);
                }
                void* output=plain_input==plain.texture?intermediate.texture:plain.texture;
                gpu.begin();check(compositor.enqueue(gpu.command,plain_input,own.texture,output,w,h,nullptr,nullptr,1,1,0,step,passes[j]),compositor.status().c_str());gpu.submit();
                auto actual=output==plain.texture?plain.read():intermediate.read();
                if(step==1) {auto expected=input;Bytes scratch;render::apply_effect(static_cast<render::Effect>(passes[j].model),tags,expected,scratch,
                    passes[j].model_intensity,static_cast<render::Effect>(passes[j].world),passes[j].world_intensity);
                    compare(actual,expected,"Edge style differs from independent flat RGB",srgb(format)?1:0);}
                input=std::move(actual);plain_input=output;
            }
            void* witness=nullptr;
            gpu.begin();check(compositor.enqueue_sequence(gpu.command,raw.texture,own.texture,styled.texture,w,h,nullptr,nullptr,1,1,0,step,passes,&witness,0),compositor.status().c_str());gpu.submit();
            check(bool(witness)==edges.active(),"Unselected edge sequence retained/allocated a witness");
            const auto styled_bytes=styled.read();compare(styled_bytes,input,"MRT edge capture changed original style RGB/alpha",0);
            if(witness) {const auto actual_bits=atlas_read.read(static_cast<SDL_GPUTexture*>(witness));
                for(unsigned p=0;p<n;++p) {float value;std::memcpy(&value,actual_bits.data()+p*4,4);check(value==float(bits[p]),"MRT edge/grid witness differs from PRE-style branch oracle");}}
            auto candidate=reference;const bool descriptor_changed=accepted && (edges!=accepted_edges || patterns!=accepted_patterns);if(descriptor_changed) candidate.reset();
            const auto layout=*render::shadows::native_water_layers(w,h,false);Bytes wet_bytes(layout.storage_bytes,0);
            std::vector<render::TemporalAaGuide> guides(n);std::vector<bool> crossed(n);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x,layer=layers[p];const bool wet=y==h/2 && x%4<2;wet_bytes[p*4+3]=wet?253:254;
                guides[p]={motions[p][0],motions[p][1],600,600,true,owners[p*4+3]!=0 && !wet && !(layer==1 && rejected),
                    pattern_oracle_phase(x,y,step,layer,pattern_effects),edge_oracle_key(bits[p],layer,layer==1?masks[0]:masks[1])};
                if(accepted && !descriptor_changed && guides[p].eligible) {
                    const float px=x+motions[p][0],py=y+motions[p][1];
                    if(px>=0 && py>=0 && px<w-1 && py<h-1) for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                        const auto& old=prior_guides[(unsigned(py)+dy)*w+unsigned(px)+dx];
                        const float share=(dx?px-unsigned(px):1-(px-unsigned(px)))*(dy?py-unsigned(py):1-(py-unsigned(py)));
                        crossed[p]=crossed[p] || (share>0 && old.eligible && (old.edge_phase!=guides[p].edge_phase || old.pattern_phase!=guides[p].pattern_phase));
                    }
                }
                crossings+=crossed[p];
            }
            const auto expected=candidate.resolve(w,h,styled_bytes,guides,71,.8F);
            gpu.begin();Buffer payload(gpu,wet_bytes);const render::shadows::GpuReflectionOutput liquid{gpu.device,payload.buffer,w,h,w*4,layout};
            const render::CalibratedTemporalAaSettings settings{71,.8F,rejected,patterns,edges};
            if(frame==0) {
                check(!taa.enqueue(gpu.command,styled.texture,own.texture,surface.texture,motion.texture,resolved.texture,w,h,settings,&liquid,nullptr),"TAA accepted missing edge atlas");
                check(!taa.enqueue(gpu.command,styled.texture,own.texture,surface.texture,motion.texture,resolved.texture,w,h,settings,&liquid,styled.texture),"TAA accepted aliased edge atlas");
                auto bad=settings;bad.edges.passes[0].model=7;
                check(!taa.enqueue(gpu.command,styled.texture,own.texture,surface.texture,motion.texture,resolved.texture,w,h,bad,&liquid,witness),"TAA accepted malformed edge descriptor");
            }
            check(taa.enqueue(gpu.command,styled.texture,own.texture,surface.texture,motion.texture,resolved.texture,w,h,settings,&liquid,witness),taa.status().c_str());
            if(frame==4) {check(SDL_CancelGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;taa.discard();++recoveries;continue;}
            gpu.submit();const auto actual=resolved.read();compare(actual,expected,"Edge TAA differs from independent branch/history oracle",srgb(format)?1:0);bytes+=actual.size();
            if(descriptor_changed) compare(actual,styled_bytes,"Edge pass/grid/intensity change reused incompatible history",0);
            for(unsigned p=0;p<n;++p) {
                if(!guides[p].eligible || crossed[p]) check(std::equal(actual.begin()+p*4,actual.begin()+p*4+4,styled_bytes.begin()+p*4),"Changed edge or wet/UI pixel reused history");
                for(unsigned c=0;c<3;++c) {const bool changed=std::abs(int(actual[p*4+c])-int(styled_bytes[p*4+c]))>1;responses+=changed;selected_responses+=changed && frame==1;}
            }
            // Independently styled CENTRE differs from AA: compare actual
            // positive taps against centre branches, not the AA's own centre.
            auto centre_colors=colors;for(unsigned y=3;y<h;y+=5) for(unsigned x=2;x<w;x+=7) for(unsigned c=0;c<3;++c) centre_colors[(y*w+x)*4+c]=240;
            gpu.begin();centre_raw.upload(centre_colors);gpu.submit();
            auto centre_input=centre_colors;std::vector<unsigned> centre_bits(n);plain_input=centre_raw.texture;
            for(unsigned j=0;j<3;++j) if(passes[j].active()) {
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    const unsigned p=y*w+x,layer=layers[p];const auto& pass=passes[j];
                    const unsigned effect=layer==1?pass.world:layer==2?pass.model:0,amount=layer==1?pass.world_intensity:pass.model_intensity;
                    centre_bits[p]|=(amount?edge_oracle_branch(centre_input,layers,w,h,x,y,step,effect):0)<<(j*3);
                }
                void* output=plain_input==plain.texture?intermediate.texture:plain.texture;
                gpu.begin();check(compositor.enqueue(gpu.command,plain_input,own.texture,output,w,h,nullptr,nullptr,1,1,0,step,passes[j]),compositor.status().c_str());gpu.submit();
                centre_input=output==plain.texture?plain.read():intermediate.read();plain_input=output;
            }
            gpu.begin();void* centre_witness=nullptr;
            check(compositor.enqueue_sequence(gpu.command,centre_raw.texture,own.texture,centre.texture,w,h,nullptr,nullptr,1,1,0,step,passes,&centre_witness,2),compositor.status().c_str());gpu.submit();
            const auto centre_bytes=centre.read();const auto centre_atlas=centre_witness?atlas_read.read(static_cast<SDL_GPUTexture*>(centre_witness)):Bytes(n*4);
            compare(centre_bytes,centre_input,"Centre MRT capture changed original RGB/alpha",0);
            if(centre_witness) for(unsigned p=0;p<n;++p) {float value;std::memcpy(&value,centre_atlas.data()+p*4,4);
                check(value==float(centre_bits[p]),"Independent centre PRE-style branch differs from witness");}
            const std::array jitter{.25F,-.375F};auto expected_present=centre_bytes;
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x,at=p*4;if(!owners[at+3]) continue;
                const auto key=edge_oracle_key(centre_bits[p],layers[p],layers[p]==1?masks[0]:masks[1]);
                const float px=std::clamp(x+jitter[0],0.F,float(w-1)),py=std::clamp(y+jitter[1],0.F,float(h-1));
                const unsigned ax=unsigned(px),ay=unsigned(py);const float fx=px-ax,fy=py-ay;bool changed=false;std::array<double,3> rgb{};
                for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                    const unsigned tap=std::min(ay+dy,h-1)*w+std::min(ax+dx,w-1);const bool owns=owners[tap*4+3]!=0;const double share=(dx?fx:1-fx)*(dy?fy:1-fy);
                    changed|=owns && share>0 && (guides[tap].edge_phase!=key || guides[tap].pattern_phase!=guides[p].pattern_phase);
                    for(unsigned c=0;c<3;++c) {const double value=(owns?expected[tap*4+c]:centre_bytes[at+c])/255.;rgb[c]+=(srgb(format)?decode(value):value)*share;}
                }
                if(changed) {++centre_fallbacks;continue;}
                for(unsigned c=0;c<3;++c) expected_present[at+c]=static_cast<unsigned char>(std::lround(std::clamp(srgb(format)?encode(rgb[c]):rgb[c],0.,1.)*255));
            }
            gpu.begin();check(taa.enqueue_presentation(gpu.command,resolved.texture,own.texture,surface.texture,centre.texture,own.texture,presented.texture,w,h,jitter,patterns,edges,witness,centre_witness)==false,"Presentation accepted aliased AA/centre ownership");
            check(taa.enqueue_presentation(gpu.command,resolved.texture,own.texture,surface.texture,centre.texture,centre_own.texture,presented.texture,w,h,jitter,patterns,edges,witness,centre_witness),taa.status().c_str());gpu.submit();
            compare(presented.read(),expected_present,"Edge de-jitter blurred across independent centre/AA branch",1);
            if(frame==10) {taa.discard();++recoveries;continue;}
            taa.commit();reference=std::move(candidate);accepted_edges=edges;accepted_patterns=patterns;prior_guides=guides;accepted=true;
        }
        if(intensity==35) check(selected_responses>3,"Selected edge style still blanket-rejected history");
    }
    check(responses>1000 && crossings>1000 && centre_fallbacks>1000 && recoveries==312,"Edge fixture missed history/changed branch/centre/recovery coverage");
    std::cout<<"Edge TAA format "<<unsigned(format)<<": "<<bytes<<" independent history bytes, "<<responses<<" stable responses, "<<crossings
        <<" crossed footprints, "<<centre_fallbacks<<" centre-branch fallbacks; 13 styles, two grids, both receivers, partial/full intensity, original ordered slots,"
        " unsupported WORLD warp, cancelled/rejected candidates, wet/UI/alpha passed. Component, not physical panel.\n";
}
