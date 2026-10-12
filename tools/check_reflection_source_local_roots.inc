// Real owner chain: native path -> GPU mapping/supports -> GPU frame/optics ->
// shared complete differential/local-root writer. The independent binary64
// forward oracle authors raw source inputs and checks outputs, never a GPU
// box, root, inverse seed or history-colour choice.
struct CurrentBankCapture {
    Gpu& gpu;unsigned bytes;SDL_GPUTransferBuffer* transfer{};
    CurrentBankCapture(Gpu& g,unsigned n):gpu(g),bytes(n) {
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,n,0};
        transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
    }
    ~CurrentBankCapture(){if(transfer)SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);}
    void record(SDL_GPUCommandBuffer* cb,void* current) {
        auto* pass=SDL_BeginGPUCopyPass(cb);check(pass,SDL_GetError());
        const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(current),0,bytes};
        const SDL_GPUTransferBufferLocation to{transfer,0};
        SDL_DownloadFromGPUBuffer(pass,&from,&to);SDL_EndGPUCopyPass(pass);
    }
    Words read() {
        const auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());
        Words words(bytes/4);std::memcpy(words.data(),mapped,bytes);
        SDL_UnmapGPUTransferBuffer(gpu.device,transfer);return words;
    }
};
void exercise_local_roots(Gpu& gpu,unsigned lobes,bool curved,unsigned material,bool finite,bool whole_cover=false,bool boundary=false,
    bool witness=false,bool bad_depth=false,bool bad_feature=false,bool folds=false,unsigned bad_fold=0,unsigned ordered_hops=0,bool guides=false,
    bool colours=false,bool srgb=false,bool compose=false,bool mixed_reject=false,bool publish=false,bool admission_diagnostics=false) {
    namespace o=reflected_curved_owner_oracle;
    constexpr unsigned w=17,h=9,count=w*h;
    check(!guides || folds,"Native guide diagnostics require complete preceding fold proofs");
    check(!colours || guides,"Native colour diagnostics require preceding complete guide proofs");
    check(!compose || colours,"Whole-pixel composition diagnostics require preceding incident colours");
    check(!admission_diagnostics || publish,"Admission diagnostics require the whole publication suite");
    check(!publish || (compose && whole_cover && !boundary && !bad_depth && !bad_feature && !bad_fold && !ordered_hops),
        "Full-frame publication probe requires a complete positive composition fixture");
    check(!mixed_reject || (compose && lobes==8 && finite && bad_fold==1),"Mixed-lobe fixture requires eight-lobe finite fold composition");
    check(ordered_hops<=4 && (!ordered_hops || (folds && !curved && !finite && !bad_depth && !bad_feature && !bad_fold)),
        "Ordered native fold fixture requires a complete planar environment path");
    const unsigned pixel=curved?106U:22U;
    // CURRENT raw targets move inside an old cell rather than sitting on its
    // shared grid boundary. The old bank itself remains ordinary half-pixel
    // samples; no expected root coordinate or optical box is uploaded.
    const double offset=boundary?.5:.625;
    const double sx=double(pixel%w)+offset,sy=double(pixel/w)+offset;
    const auto layout=native_layout(w,h,lobes,curved);
    Words geometry(count*25,0);const std::array<float,12> vertices{-100,-100,400,1,200,-100,400,1,-100,200,400,1};
    for(unsigned t=0;t<count;++t) {
        std::memcpy(geometry.data()+t*12,vertices.data(),48);std::memcpy(geometry.data()+count*12+t*12,vertices.data(),48);
        if(ordered_hops && t>=3 && t<7) {
            // Independently trace alternating, finite, noncoincident faces.
            // Large complete faces retain all eight real rough quadrature rays;
            // these are raw GPU old vertices, not a root/box/certificate.
            constexpr float hop_z[]{100,600,50,700};
            const std::array<float,12> hop_vertices{-4096,-4096,hop_z[t-3],1,8192,-4096,hop_z[t-3],1,
                -4096,8192,hop_z[t-3],1};
            std::memcpy(geometry.data()+count*12+t*12,hop_vertices.data(),48);
        }
        for(unsigned v=0;v<3;++v) {
            geometry[t*12+v*4+2]=std::bit_cast<unsigned>(800.F+float(t));
            // Retain the authored finite hop face while applying the same raw
            // old-vertex translation as the ordinary receiver fixtures.
            const unsigned old_x=count*12+t*12+v*4;
            geometry[old_x]=std::bit_cast<unsigned>(std::bit_cast<float>(geometry[old_x])+float(t)/64);
            if(finite && t==17)geometry[count*12+t*12+v*4+2]=std::bit_cast<unsigned>(100.F);
        }
        geometry[count*24+t]=count-1-t;
    }
    Buffer positions(gpu,geometry);
    shadows::RayReflectionHistory previous{count*48,{w,h},{256,272,8,4},1,65536,count*96,true};
    previous.model_lobes=lobes;previous.model_paths=true;previous.curved_paths=previous.curved_receivers=curved;
    if(curved) {previous.previous_liquid=shadows::RayReflectionLiquid{{{0,180,0},{0,-1,0}}};previous.previous_liquid->material=material;}
    auto independent=o::decode(geometry,previous.previous_vertex_offset,count*3,
        shadows::reflection_liquid_frame_words(previous),previous.projection);
    // Planar records intentionally have no liquid plane. The independent eye
    // is still known from the real accepted native projection/clip metadata.
    if(!curved) {
        independent.liquid.width=w;independent.liquid.height=h;independent.liquid.projection=previous.projection;
        independent.liquid.near=previous.near_plane;independent.liquid.far=previous.far_plane;
    }
    std::vector<o::ForwardRoot> expected;
    auto old_inputs=query_fixture(w,h,lobes,curved,material,false,false,0);
    auto current_inputs=query_fixture(w,h,lobes,curved,material,true,false,256);
    for(unsigned l=0;l<lobes;++l) {
        o::Path path;path.receiver=independent.previous[pixel];path.liquid_primary=curved;
        path.hops=ordered_hops;
        for(unsigned hop=0;hop<ordered_hops;++hop)path.mirrors[hop]=independent.previous[3+hop];
        path.lobe=l;path.roughness=lobes==8?.5:0;path.finite_terminal=finite;path.terminal_plane=independent.previous[17];
        const auto ray=o::forward(independent,path,sx,sy);check(ray.valid,"Authored native local-root forward input invalid");
        std::array<float,3> feature{};
        if(finite) {
            const double distance=(100-ray.origin[2])/ray.outgoing[2];check(distance>0,"Finite terminal is behind the reflected ray");
            const auto hit=o::add(ray.origin,o::scale(ray.outgoing,distance));const auto bary=o::bary(path.terminal_plane,hit);
            check(bary[0]>=0 && bary[1]>=0 && bary[0]+bary[1]<=1,"Authored terminal missed its accepted finite face");
            feature={float(bary[0]),float(bary[1]),0};path.terminal=o::point(path.terminal_plane,{feature[0],feature[1]});
        } else {
            feature={float(ray.outgoing[0]),float(ray.outgoing[1]),float(ray.outgoing[2])};
            path.terminal=o::unit({feature[0],feature[1],feature[2]});
        }
        const auto root=o::solve_forward(independent,path,sx,sy);check(root.valid,"Independent raw-feature local root did not solve");
        expected.push_back(root);
        // Populate a continuous 3x3 accepted source patch with the SAME physical
        // receiver/path identity and independently retraced per-tap features.
        // Isolated taps have only zero-area supports and cannot establish a
        // continuum local-root inclusion. Do not invent support boxes to fix it.
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
            const unsigned p=unsigned(int(pixel)+dy*int(w)+dx);
            const auto old_ray=o::forward(independent,path,double(p%w)+.5,double(p/w)+.5);
            check(old_ray.valid,"Independent neighbouring source tap invalid");
            std::array<float,3> old_feature{};
            if(finite) {
                const double distance=(100-old_ray.origin[2])/old_ray.outgoing[2];
                const auto hit=o::add(old_ray.origin,o::scale(old_ray.outgoing,distance));
                const auto bary=o::bary(path.terminal_plane,hit);
                check(distance>0 && bary[0]>=0 && bary[1]>=0 && bary[0]+bary[1]<=1,
                    "Independent neighbouring source missed the finite terminal");
                old_feature={float(bary[0]),float(bary[1]),0};
            } else old_feature={float(old_ray.outgoing[0]),float(old_ray.outgoing[1]),float(old_ray.outgoing[2])};
            if(bad_feature) {
                check(finite && !curved && !boundary && !bad_depth,"Source-footprint hole fixture requires a finite interior target");
                // A diagonal necessary feature half-plane has a rectangular
                // enclosure containing the physical root, while the root lies
                // OUTSIDE the half-plane. Real raw old features, not an authored
                // GPU box/root/certificate, create this hole. The full-hull
                // root must remain provable, but source admission must refuse.
                old_feature=feature;old_feature[0]+=dx==0 && dy==0?.03F:0.F;
                check(old_feature[0]>=0 && old_feature[0]+old_feature[1]<=1,"Source-hole raw barycentric fixture invalid");
            }
            if(bad_fold && (!mixed_reject || l+1==lobes)) {
                check(folds && finite && !curved && !boundary && !bad_depth && !bad_feature && bad_fold<=2,
                    "Raw source-fold fixture requires a finite interior target");
                old_feature=feature;
                old_feature[0]+=.001F*float(bad_fold==1?dx*(dy==1?-1:1):std::abs(dx));
                old_feature[1]+=.001F*float(dy);
                check(old_feature[0]>=0 && old_feature[1]>=0 && old_feature[0]+old_feature[1]<=1,
                    "Raw source-fold barycentric fixture invalid");
            }
            old_inputs[p]=(curved?(material==0?0xfd000000U:0xfe000000U):0xff000000U)|0x336699U;
            old_inputs[layout.identity_offset/4+p]=curved?0xfffffffdU:pixel;
            old_inputs[layout.witness_offset/4+p]=std::bit_cast<unsigned>(float(old_ray.depth)*(bad_depth?2.F:1.F));
            const unsigned at=layout.incoming_offset/4+(p*lobes+l)*(curved?16:13);
            for(unsigned n=0;n<4;++n)old_inputs[at+n]=n<ordered_hops?3+n:UINT32_MAX;
            const unsigned kind=finite?1U:ordered_hops==4?3U:2U;
            old_inputs[at+4]=(kind<<8)|ordered_hops;old_inputs[at+5]=finite?17U:UINT32_MAX;
            for(unsigned c=0;c<3;++c)old_inputs[at+6+c]=std::bit_cast<unsigned>(old_feature[c]);
        }
        const unsigned at=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13);
        for(unsigned n=0;n<4;++n)current_inputs[at+n]=n<ordered_hops?count-1-(3+n):UINT32_MAX;
        const unsigned kind=finite?1U:ordered_hops==4?3U:2U;
        current_inputs[at+4]=(kind<<8)|ordered_hops;current_inputs[at+5]=finite?count-1-17:UINT32_MAX;
        const std::array<float,3> raw=!finite?std::array{feature[1],-feature[0],feature[2]}:feature;
        for(unsigned c=0;c<3;++c)current_inputs[at+6+c]=std::bit_cast<unsigned>(raw[c]);
    }
    if(colours)for(unsigned l=0;l<lobes;++l)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        const unsigned p=unsigned(int(pixel)+dy*int(w)+dx),at=layout.incoming_offset/4+(p*lobes+l)*(curved?16:13);
        const unsigned here=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13);
        const bool excluded=(dx==-1 && dy==-1) || (dx==-1 && dy==0) || (dx==1 && dy==1);
        old_inputs[at+9]=0xff000000U|unsigned(210+dx*7+l)|(unsigned(10+dy+dx+2)<<8)|(unsigned(170+dy*5+l)<<16);
        // Raw CURRENT path/incident/base/throughput, not a host colour choice,
        // inverse seed or GPU certificate. Matching neighbours vary in RGB;
        // deliberately bright mismatched/invalid neighbours must NOT widen it.
        std::copy_n(current_inputs.begin()+here,6,current_inputs.begin()+at);
        for(unsigned c=0;c<3;++c) {
            auto f=std::bit_cast<float>(old_inputs[at+6+c]);
            if(!finite)f=c==0?std::bit_cast<float>(old_inputs[at+7]):c==1?-std::bit_cast<float>(old_inputs[at+6]):f;
            if(p!=pixel)current_inputs[at+6+c]=std::bit_cast<unsigned>(f); // Keep the raw physical .625 target exact.
            current_inputs[at+10+c]=std::bit_cast<unsigned>(.2F+float(c)*.2F);
            if(curved)current_inputs[at+13+c]=std::bit_cast<unsigned>(.025F*float(c+1));
        }
        current_inputs[layout.identity_offset/4+p]=current_inputs[layout.identity_offset/4+pixel];
        current_inputs[p]=current_inputs[pixel];
        current_inputs[at+9]=excluded?0xfffefdfcU:0xff000000U|unsigned(40+(dx+1)*20+(dy+1)*7+l)
            |(unsigned(60+(dy+1)*15+l)<<8)|(unsigned(35+(dx+1)*9+(dy+1)*11+l)<<16);
        if(curved)for(unsigned c=0;c<3;++c)current_inputs[layout.base_offset/4+p*4+c]=std::bit_cast<unsigned>(.03F*float(c+1));
        if(dx==-1 && dy==-1)current_inputs[layout.weight_offset/4+p*4+3]=0;
        if(dx==-1 && dy==0)current_inputs[at+4]|=8; // Invalid reserved bit: receiver itself remains eligible.
        if(dx==1 && dy==1)current_inputs[layout.identity_offset/4+p]=count-1-1; // Different primary.
    }
    if(compose) {
        // Distinct CURRENT/old material response and non-byte-aligned CURRENT
        // throughput/base expose old-material use or pack-before-transport.
        for(unsigned l=0;l<lobes;++l)for(unsigned c=0;c<3;++c) {
            const unsigned at=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13);
            current_inputs[at+10+c]=std::bit_cast<unsigned>(.137F+float(c)*.281F+float(l)*.005F);
            if(curved)current_inputs[at+13+c]=std::bit_cast<unsigned>(.021F*float(c+1)+float(l)*.003F);
        }
        for(unsigned c=0;c<3;++c)current_inputs[layout.weight_offset/4+pixel*4+c]=std::bit_cast<unsigned>(.31F+float(c)*.16F);
    }
    CalibratedReflectionLobeGeometry guide{positions.value,unsigned(geometry.size()*4),count*3,previous,lobes==8?.5F:0.F};
    guide.current_projection=previous.projection;guide.current_clip={previous.near_plane,previous.far_plane};
    if(curved)guide.current_liquid=previous.previous_liquid;
    const auto water=curved?*shadows::native_water_layers(w,h,true):shadows::NativeWaterLayers{};
    OwnerTexture ink(gpu,w,h,curved,curved);GpuCalibratedReflectionHistory owner;check(owner.initialize(gpu.device),owner.status().c_str());
    const auto stage=[&](const Words& authored,bool indexed=true) {
        Buffer source(gpu,authored);const shadows::GpuReflectionOutput rays{gpu.device,source.value,w,h,w*4,water,layout};
        auto* cb=SDL_AcquireGPUCommandBuffer(gpu.device);check(cb,SDL_GetError());
        check(owner.enqueue(cb,rays,ink.value,{70,indexed && colours?.85F:0.F,srgb,curved,indexed,indexed && admission_diagnostics},&guide),owner.status().c_str());
        check(!owner.accepted_source_admission().buffer,"Pending source mask escaped its accepted presentation lifetime");return cb;
    };
    const auto encode=[&](void* command) {return whole_cover?owner.enqueue_source_root_hull(command):owner.enqueue_source_local_roots(command);};
    const auto active=[&]() {
        if(!whole_cover)return owner.source_local_roots();
        const auto hull=owner.source_root_hull();return GpuReflectionSourceLocalRoots{hull.optical,hull.results};
    }; // Diagnostic copy adapter only: never label a hull as independent local regions.
    const auto cleared=[&]() {return !owner.source_local_roots().results && !owner.source_root_hull().results
        && !owner.source_witness().results && !owner.source_folds().results && !owner.source_guide().results
        && !owner.source_colour().results && !owner.source_composition().results;};
    auto* cb=stage(old_inputs);check(!encode(cb) && cleared(),
        "Cold capture published native local roots");
    if(publish)check(owner.enqueue_source_frame(cb) && !owner.source_publication_complete()
        && owner.output().buffer==owner.fresh_current_output().buffer,
        "Cold complete stream invented old radiance or published a partial image");
    gpu.submit(cb);owner.commit();
    const auto accepted=owner.accepted_source_index();const auto bank=gpu.read(accepted.source_buffer,layout.storage_bytes);
    const auto cold_admission=owner.accepted_source_admission();
    const Words zero_admission(count);
    if(admission_diagnostics)check(cold_admission.device==gpu.device && cold_admission.width==w && cold_admission.height==h
        && cold_admission.bytes==count*4 && cold_admission.buffer!=accepted.source_buffer && cold_admission.buffer!=accepted.buffer
        && gpu.read(cold_admission.buffer,count*4)==zero_admission,"Cold accepted mask lost its pair or invented admission");
    else check(!cold_admission.buffer,"Ordinary publication allocated a diagnostic admission mask");
    const auto resident=owner.working_image_bytes();const auto complete=resident+GpuReflectionSourceQueries::working_bytes
        +GpuReflectionSourceOptical::working_bytes+GpuReflectionSourceLocalRoots::working_bytes;
    check(owner.source_local_root_allocation_bytes()==complete,"Native local-root preflight omitted lazy complete scratch");
    guide.current_cube={0,-1,0,1,0,0,0,0,1};if(lobes==8)guide.roughness=.85F;
    std::vector<o::SourceGuard> independent_guards;
    if(witness) {
        // Independent located-eye matrices reproduce the diagnostic's actual
        // CURRENT rotation; the old calibration/roughness stays accepted.
        o::eye_views(independent,std::array<float,16>{0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1},
            std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
        for(unsigned l=0;l<lobes;++l) {
            std::array<unsigned,16> raw{};const unsigned at=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13);
            std::copy_n(current_inputs.begin()+at,9,raw.begin());
            const auto read=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
                const unsigned p=y*w+x;o::SourceTap tap;tap.identity=bank[layout.identity_offset/4+p];
                tap.depth=std::bit_cast<float>(bank[layout.witness_offset/4+p]);
                std::copy_n(bank.begin()+layout.incoming_offset/4+(p*lobes+l)*(curved?16:13),9,tap.path.begin());return tap;
            };
            const o::Solution source{expected[l].x-.5,expected[l].y-.5,expected[l].depth,expected[l].error,true,false};
            const auto guard=o::source_guard(independent,current_inputs[layout.identity_offset/4+pixel],raw,
                lobes==8?.5:0,l,source,read);
            independent_guards.push_back(guard);
            if(bad_feature)check(guard==o::SourceGuard::feature,"Independent raw source-hole predicate did not refuse its physical root");
            else if(bad_depth)check(guard==o::SourceGuard::depth,"Independent raw source-depth predicate did not refuse its physical root");
            else if(bad_fold && (!mixed_reject || l+1==lobes))check(guard==(bad_fold==1?o::SourceGuard::cell_fold:o::SourceGuard::ring_fold),
                "Independent actual raw source-fold predicate did not reach its intended fold guard");
            else check(guard==o::SourceGuard::accepted || guard==o::SourceGuard::cell_fold || guard==o::SourceGuard::ring_fold
                || guard==o::SourceGuard::forward || guard==o::SourceGuard::subcell_fold,
                "Positive native source fixture fails the independent path/depth/feature predicate");
            // A later fold refusal is NOT waved through as colour eligibility.
            // This stage/fixture qualifies the source footprint only; full
            // native fold/current-colour acceptance remains explicitly open.
        }
    }
    Words previous_results,previous_optical_inputs;unsigned unique=0;
    for(unsigned round=0;round<2;++round) {
        cb=stage(current_inputs);check(!encode(cb),"Native roots without a pending optical batch were accepted");
        check(owner.enqueue_source_queries(cb,pixel*lobes,lobes),owner.status().c_str());
        check(!active().results,"New query retained a preceding local-root descriptor");
        check(owner.enqueue_source_optical(cb),owner.status().c_str());
        if(whole_cover) {
            check(owner.enqueue_source_local_roots(cb) && owner.source_local_roots().results && !owner.source_root_hull().results,
                "Local encoding exposed a whole-source hull descriptor");
        }
        check(encode(cb),owner.status().c_str());
        const auto roots=active();const auto queries=owner.source_queries();const auto optics=owner.source_optical();
        check(roots.results && roots.optical.frames==optics.frames && roots.optical.queries.current_buffer==owner.output().buffer
            && roots.optical.queries.source.source_buffer==accepted.source_buffer && roots.optical.queries.first==pixel*lobes
            && owner.working_image_bytes()==complete && owner.source_local_root_allocation_bytes()==complete
            && (whole_cover?!owner.source_local_roots().results:!owner.source_root_hull().results),
            "Native local roots borrowed another source/batch or omitted retained memory");
        QueryCapture capture(gpu,lobes,true,true);capture.record(cb,queries,&optics,&roots);
        // Every refusal clears the borrowed root descriptor, not its storage.
        check(!encode(nullptr) && cleared(),"Null root command retained a usable descriptor");
        check(!encode(&owner) && cleared(),"Foreign root command retained a usable descriptor");
        check(encode(cb) && active().results,owner.status().c_str());
        check(owner.enqueue_source_optical(cb) && cleared(),
            "Next optical encoding retained a previous usable local-root descriptor");
        gpu.submit(cb);const auto words=capture.read();
        const unsigned base=capture.capacity*2772;const Words results(words.begin()+base,words.end());
        if(round) {
            if(results!=previous_results) {
                std::cerr<<"Held native local-root mismatch family="<<(curved?material==3?"lava":"water":finite?"finite":"sky")
                    <<" lobes="<<lobes<<" whole-cover="<<whole_cover<<" boundary="<<boundary<<'\n';
                unsigned changed=0,printed=0;
                for(unsigned n=0;n<results.size();++n)if(results[n]!=previous_results[n]) {
                    ++changed;
                    if(printed++<32)std::cerr<<" lobe="<<n/6144<<" slot="<<(n%6144)/48<<" field="<<n%48
                        <<" before=0x"<<std::hex<<previous_results[n]<<" after=0x"<<results[n]<<std::dec
                        <<" floats="<<std::bit_cast<float>(previous_results[n])<<','<<std::bit_cast<float>(results[n])<<'\n';
                }
                std::cerr<<" changed-root-words="<<changed<<'\n';
                changed=printed=0;
                for(unsigned n=0;n<base;++n)if(words[n]!=previous_optical_inputs[n]) {
                    ++changed;
                    if(printed++<32)std::cerr<<" input-word="<<n<<" before=0x"<<std::hex<<previous_optical_inputs[n]
                        <<" after=0x"<<words[n]<<std::dec<<'\n';
                }
                std::cerr<<" changed-query-domain-frame-words="<<changed<<'\n';
            }
            check(results==previous_results,"Held native optical frames changed local classifications");
        }else {
            previous_results=results;
            previous_optical_inputs.assign(words.begin(),words.begin()+base);
        }
        for(unsigned l=0;l<lobes;++l) {
            const unsigned domain=capture.capacity*80+l*1540,frame=capture.capacity*1620+l*128;
            check(!words[domain+1] && words[domain]>0 && words[frame+124]==1
                && std::bit_cast<float>(words[frame+21])==float(l),"Known physical native lobe lost its supports/frame");
            if(whole_cover && results[l*6144])for(unsigned region=0;region<words[domain];++region) {
                const unsigned receipt=capture.capacity*1748+l*1024+region*8,at=l*6144;
                if(!words[receipt])continue;
                check(std::bit_cast<float>(results[at+4])<=std::bit_cast<float>(words[receipt+4])
                    && std::bit_cast<float>(results[at+5])<=std::bit_cast<float>(words[receipt+5])
                    && std::bit_cast<float>(results[at+6])>=std::bit_cast<float>(words[receipt+6])
                    && std::bit_cast<float>(results[at+7])>=std::bit_cast<float>(words[receipt+7]),
                    "Whole-source hull omitted a retained region/boundary");
            }
            bool retained=false,certified=false;
            for(unsigned region=0;region<128;++region) {
                const unsigned at=l*6144+region*48;
                if(region>=(whole_cover?1U:words[domain])) {for(unsigned n=0;n<48;++n)check(!results[at+n],"Unused root slot retained an older batch");continue;}
                check(results[at]<=1 && results[at+1]<=2,"Native root stage published an unknown classification");
                if(!results[at])continue;
                const double x=expected[l].x,y=expected[l].y;
                const bool contains=std::bit_cast<float>(results[at+4])<=x && std::bit_cast<float>(results[at+5])<=y
                    && std::bit_cast<float>(results[at+6])>=x && std::bit_cast<float>(results[at+7])>=y;
                retained|=contains;
                if(contains)check(results[at+1]!=2,"Local no-root certificate contradicted the independent raw-target root");
                if(results[at+1]==1) {
                    check(contains && std::bit_cast<float>(results[at+36])<=x && std::bit_cast<float>(results[at+37])<=y
                        && std::bit_cast<float>(results[at+38])>=x && std::bit_cast<float>(results[at+39])>=y,
                        "Native local unique enclosure contradicted the independent forward root");certified=true;
                }
            }
            if(!retained || !certified) {
                std::cerr<<"Native local-root diagnostic family="<<(curved?material==3?"lava":"water":finite?"finite":"sky")
                    <<" lobe="<<l<<" supports="<<words[domain]<<" independent="<<expected[l].x<<','<<expected[l].y<<'\n';
                for(unsigned region=0;region<words[domain];++region) {
                    const unsigned at=l*6144+region*48;
                    std::cerr<<" region="<<region<<" valid="<<results[at]<<" class="<<results[at+1]<<" reason="<<results[at+3]
                        <<" box="<<std::bit_cast<float>(results[at+4])<<','<<std::bit_cast<float>(results[at+5])<<','
                        <<std::bit_cast<float>(results[at+6])<<','<<std::bit_cast<float>(results[at+7])
                        <<" contraction="<<std::bit_cast<float>(results[at+40])
                        <<" enclosure="<<std::bit_cast<float>(results[at+36])<<','<<std::bit_cast<float>(results[at+37])
                        <<','<<std::bit_cast<float>(results[at+38])<<','<<std::bit_cast<float>(results[at+39])
                        <<" centre-residual="<<std::bit_cast<float>(results[at+44])<<','<<std::bit_cast<float>(results[at+45])
                        <<','<<std::bit_cast<float>(results[at+46])<<','<<std::bit_cast<float>(results[at+47])<<'\n';
                }
            }
            check(retained,"Native complete map failed to evaluate a retained known physical root");
            check(certified,"Native complete map did not locally enclose its known physical root");if(!round)++unique;
        }
        check(gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,"Native local roots mutated accepted RGB/path input");
        owner.discard();check(cleared() && owner.working_image_bytes()==complete,
            "Discard retained a usable root descriptor or lost reusable bounded scratch");
    }
    if(witness) {
        Words held;unsigned proved=0,fold_proved=0,fold_refused=0,guide_proved=0,guide_refused=0,colour_proved=0,colour_refused=0,composed_pixels=0;
        for(unsigned round=0;round<2;++round) {
            cb=stage(current_inputs);check(!owner.enqueue_source_witness(cb) && cleared(),
                "Source witness without a whole-source hull exposed a usable descriptor");
            std::unique_ptr<CurrentBankCapture> current_capture;
            if(colours) {
                current_capture=std::make_unique<CurrentBankCapture>(gpu,layout.storage_bytes);
                current_capture->record(cb,owner.output().buffer);
            }
            check(owner.enqueue_source_queries(cb,pixel*lobes,lobes) && owner.enqueue_source_optical(cb)
                && owner.enqueue_source_root_hull(cb),owner.status().c_str());
            const auto hull=owner.source_root_hull();const GpuReflectionSourceLocalRoots before{hull.optical,hull.results};
            // Keep independent transfer owners for each proof snapshot. This
            // isolates same-command proof writes from packed-transfer copies;
            // aggregate download storage is unchanged and no proof is uploaded.
            QueryCapture root_capture(gpu,lobes,true,true),footprint_capture(gpu,lobes,true,true);
            std::unique_ptr<QueryCapture> fold_capture;
            if(folds)fold_capture=std::make_unique<QueryCapture>(gpu,lobes,true,true);
            std::unique_ptr<QueryCapture> guide_capture;
            if(guides)guide_capture=std::make_unique<QueryCapture>(gpu,lobes,true,true);
            std::unique_ptr<QueryCapture> colour_capture;
            if(colours)colour_capture=std::make_unique<QueryCapture>(gpu,lobes,true,true);
            std::unique_ptr<QueryCapture> compose_capture;
            if(compose)compose_capture=std::make_unique<QueryCapture>(gpu,lobes,true,true);
            root_capture.record(cb,owner.source_queries(),&hull.optical,&before);
            if(folds) {
                check(!owner.enqueue_source_folds(cb) && cleared(),"Native folds without a preceding footprint exposed a descriptor");
                check(owner.enqueue_source_root_hull(cb),owner.status().c_str());
            }
            check(owner.enqueue_source_witness(cb),owner.status().c_str());const auto source=owner.source_witness();
            check(source.results==hull.results && source.hull.optical.frames==hull.optical.frames
                && !owner.source_root_hull().results && !owner.source_local_roots().results
                && owner.working_image_bytes()==complete,"Source witness aliased an older descriptor or allocated another scratch bank");
            const GpuReflectionSourceLocalRoots after{source.hull.optical,source.results};
            footprint_capture.record(cb,owner.source_queries(),&source.hull.optical,&after);
            check(!owner.enqueue_source_witness(nullptr) && cleared(),"Null witness command retained usable root/source evidence");
            check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb),owner.status().c_str());
            check(!owner.enqueue_source_witness(&owner) && cleared(),"Foreign witness command retained usable source evidence");
            check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb),owner.status().c_str());
            GpuReflectionSourceFolds fold;
            if(colours) {
                check(!owner.enqueue_source_colour(cb) && cleared(),"Colour without a preceding guide exposed RGB");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb),owner.status().c_str());
            }
            if(guides) {
                check(!owner.enqueue_source_guide(cb) && cleared(),"Native guide without preceding folds exposed evidence");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb),owner.status().c_str());
            }
            if(folds) {
                check(owner.enqueue_source_folds(cb),owner.status().c_str());fold=owner.source_folds();
                check(fold.results==source.results && fold.witness.results==source.results
                    && !owner.source_witness().results && !owner.source_root_hull().results && !owner.source_local_roots().results
                    && owner.working_image_bytes()==complete,"Native fold stage retained an older descriptor or allocated another scratch bank");
                check(!owner.enqueue_source_folds(nullptr) && cleared(),"Null fold command retained usable source proof");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb),owner.status().c_str());
                check(!owner.enqueue_source_folds(&owner) && cleared(),"Foreign fold command retained usable source proof");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb),owner.status().c_str());
                fold=owner.source_folds();
                const GpuReflectionSourceLocalRoots guarded{fold.witness.hull.optical,fold.results};
                // Separate before/after readbacks prove the native stage leaves
                // root AND footprint bytes unchanged. No certificate upload.
                fold_capture->record(cb,owner.source_queries(),&fold.witness.hull.optical,&guarded);
            }
            if(guides) {
                check(owner.enqueue_source_guide(cb),owner.status().c_str());const auto guided=owner.source_guide();
                check(guided.results==fold.results && guided.folds.results==fold.results
                    && !owner.source_folds().results && !owner.source_witness().results && !owner.source_root_hull().results
                    && owner.working_image_bytes()==complete,"Native guide retained older evidence or allocated another scratch bank");
                check(!owner.enqueue_source_guide(nullptr) && cleared(),"Null guide command retained usable evidence");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                    && owner.enqueue_source_guide(cb),owner.status().c_str());
                check(!owner.enqueue_source_guide(&owner) && cleared(),"Foreign guide command retained usable evidence");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                    && owner.enqueue_source_guide(cb),owner.status().c_str());
                const GpuReflectionSourceLocalRoots copy{fold.witness.hull.optical,fold.results};
                guide_capture->record(cb,owner.source_queries(),&fold.witness.hull.optical,&copy);
            }
            if(colours) {
                if(compose) {
                    check(!owner.enqueue_source_composition(cb) && cleared(),"Composition without incident colours exposed a partial pixel");
                    check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                        && owner.enqueue_source_guide(cb),owner.status().c_str());
                }
                check(owner.enqueue_source_colour(cb),owner.status().c_str());const auto rgb=owner.source_colour();
                check(rgb.results==fold.results && rgb.guide.results==fold.results && !owner.source_guide().results
                    && owner.working_image_bytes()==complete,"Colour borrowed wrong bank/guide or allocated another scratch bank");
                check(!owner.enqueue_source_colour(nullptr) && cleared(),"Null colour command retained usable RGB");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                    && owner.enqueue_source_guide(cb) && owner.enqueue_source_colour(cb),owner.status().c_str());
                check(!owner.enqueue_source_colour(&owner) && cleared(),"Foreign colour command retained usable RGB");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                    && owner.enqueue_source_guide(cb) && owner.enqueue_source_colour(cb),owner.status().c_str());
                const GpuReflectionSourceLocalRoots copy{fold.witness.hull.optical,fold.results};
                colour_capture->record(cb,owner.source_queries(),&fold.witness.hull.optical,&copy);
            }
            if(compose) {
                check(owner.enqueue_source_composition(cb),owner.status().c_str());const auto composed=owner.source_composition();
                check(composed.results==fold.results && composed.colour.results==fold.results && !owner.source_colour().results
                    && owner.working_image_bytes()==complete,"Whole-pixel composition lost its colours or allocated another scratch bank");
                check(!owner.enqueue_source_composition(nullptr) && cleared(),"Null composition command retained a partial pixel");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                    && owner.enqueue_source_guide(cb) && owner.enqueue_source_colour(cb) && owner.enqueue_source_composition(cb),owner.status().c_str());
                check(!owner.enqueue_source_composition(&owner) && cleared(),"Foreign composition command retained a partial pixel");
                check(owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                    && owner.enqueue_source_guide(cb) && owner.enqueue_source_colour(cb) && owner.enqueue_source_composition(cb),owner.status().c_str());
                if(lobes==8 && !round) {
                    check(owner.enqueue_source_queries(cb,pixel*lobes+1,lobes-1) && owner.enqueue_source_optical(cb)
                        && owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                        && owner.enqueue_source_guide(cb) && owner.enqueue_source_colour(cb),owner.status().c_str());
                    check(!owner.enqueue_source_composition(cb) && cleared() && owner.working_image_bytes()==complete,
                        "Partial/misaligned lobe batch exposed a composed pixel or allocated");
                    check(owner.enqueue_source_queries(cb,pixel*lobes,lobes) && owner.enqueue_source_optical(cb)
                        && owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb)
                        && owner.enqueue_source_guide(cb) && owner.enqueue_source_colour(cb) && owner.enqueue_source_composition(cb),owner.status().c_str());
                }
                const GpuReflectionSourceLocalRoots copy{fold.witness.hull.optical,fold.results};
                compose_capture->record(cb,owner.source_queries(),&fold.witness.hull.optical,&copy);
            }
            gpu.submit(cb);const auto original_words=root_capture.read(),footprint_words=footprint_capture.read();
            const auto fold_words=folds?fold_capture->read():footprint_words;
            const auto guide_words=guides?guide_capture->read():fold_words;
            const auto colour_words=colours?colour_capture->read():guide_words;
            const auto words=compose?compose_capture->read():colour_words;const unsigned start=lobes*2772;
            const Words observed(words.begin()+start,words.end());
            if(round)check(held==observed,"Held source-footprint witness changed between native frames");else held=observed;
            for(unsigned l=0;l<lobes;++l) {
                const unsigned original=start+l*6144,at=original,header=at+48;
                if(!std::equal(original_words.begin()+original,original_words.begin()+original+48,words.begin()+at)) {
                    // Post-submit diagnostic only: distinguish an actual lost
                    // resident packet from a same-command snapshot/copy fault.
                    // Never upload this readback, replace either snapshot or
                    // relax the original full-packet retention assertion.
                    const auto resident_words=gpu.read(after.results,lobes*24576);
                    const unsigned resident_at=l*6144;
                    std::cerr<<"Native proof retention diagnostic family="<<(curved?material==3?"lava":"water":finite?"finite":"sky")
                        <<" lobe="<<l<<" boundary="<<boundary<<" original="<<original<<" final="<<at
                        <<" independent-transfer-capacity="<<lobes<<'\n';
                    for(unsigned field=0;field<48;++field)if(original_words[original+field]!=words[at+field])
                        std::cerr<<" field="<<field<<" before=0x"<<std::hex<<original_words[original+field]<<" final=0x"<<words[at+field]
                            <<" after-footprint=0x"<<footprint_words[at+field]
                            <<" fresh-resident=0x"<<resident_words[resident_at+field]<<std::dec<<'\n';
                    std::cerr<<" fresh-witness-header=";
                    for(unsigned field=0;field<4;++field)std::cerr<<resident_words[resident_at+48+field]<<',';
                    std::cerr<<'\n';
#if defined(_WIN32)
                    if(gpu.validation)std::cerr<<" stored-critical="<<gpu.validation->GetNumStoredMessagesAllowedByRetrievalFilter()
                        <<" discarded="<<gpu.validation->GetNumMessagesDiscardedByMessageCountLimit()<<'\n';
#endif
                }
                check(original_words[original]==1 && original_words[original+1]==1 && !original_words[original+3],
                    "Initial full-hull snapshot omitted a certified root; downstream proof retention is not established");
                check(std::equal(original_words.begin()+original,original_words.begin()+original+48,words.begin()+at),
                    "Source witness rewrote its full-hull root certificate");
                if(bad_feature)check(words[header]==0 && words[header+2]==5,
                    "Physical root in a necessary bounding-box hole bypassed the actual source-feature guard");
                else if(bad_depth)check(words[header]==0 && words[header+2]==4,
                    "Actual inconsistent accepted depth bypassed the source-footprint guard");
                else {
                    if(words[header]!=1)std::cerr<<"Source witness diagnostic family="<<(curved?material==3?"lava":"water":finite?"finite":"sky")
                        <<" lobe="<<l<<" boundary="<<boundary<<" reason="<<words[header+2]<<" bits="<<words[header+1]
                        <<" cells="<<words[header+3]<<" root="<<std::bit_cast<float>(words[header+4])<<','<<std::bit_cast<float>(words[header+5])
                        <<','<<std::bit_cast<float>(words[header+6])<<','<<std::bit_cast<float>(words[header+7])<<'\n';
                    check(words[header]==1 && words[header+1]==31 && !words[header+2] && words[header+3]>0,
                        "Native source-footprint witness refused a physically valid retained source");if(!round)++proved;
                }
                if(folds) {
                    check(std::equal(footprint_words.begin()+at,footprint_words.begin()+at+64,words.begin()+at),
                        "Native fold stage rewrote its root/footprint input proofs");
                    const unsigned f=at+64;const auto guard=independent_guards[l];
                    if(bad_feature || bad_depth)check(!words[f] && words[f+1]==1,"Upstream footprint refusal bypassed native fold stage");
                    else if(bad_fold && (!mixed_reject || l+1==lobes))check(!words[f] && words[f+1]==(bad_fold==1?3U:4U),
                        "Mandatory raw cell/ring fold bypassed native guard or failed before its intended stage");
                    else if(guard!=o::SourceGuard::accepted)check(!words[f],"Native fold stage accepted an independently refused source");
                    else if(!boundary) {
                        if(!words[f])std::cerr<<"Native fold diagnostic family="<<(curved?material==3?"lava":"water":finite?"finite":"sky")
                            <<" lobe="<<l<<" reason="<<words[f+1]<<" cells="<<words[f+2]<<" traces="<<words[f+3]<<'\n';
                        check(words[f]==1 && !words[f+1],"Native fold stage refused an independently accepted interior source");
                    }
                    check(words[f]<=1 && words[f+1]<=8,"Native fold stage published an unknown proof/reason");
                    if(!round) {fold_proved+=words[f];fold_refused+=!words[f];}
                    check(std::equal(original_words.begin()+original,original_words.begin()+original+48,words.begin()+at),
                        "Native fold stage rewrote its certified root");
                }
                if(guides) {
                    check(std::equal(fold_words.begin()+at,fold_words.begin()+at+80,words.begin()+at),
                        "Native guide rewrote root/footprint/fold input proofs");
                    const unsigned g=at+80;check(words[g]<=1 && words[g+1]<=6 && !words[g+2] && !words[g+3],
                        "Native guide published unknown validity/refusal/reserved words");
                    if(!words[at+64])check(!words[g] && words[g+1]==1,"Upstream fold refusal bypassed native guide");
                    else if(!boundary) {
                        if(!words[g])std::cerr<<"Native guide diagnostic family="<<(curved?material==3?"lava":"water":finite?"finite":"sky")
                            <<" lobe="<<l<<" reason="<<words[g+1]<<'\n';
                        check(words[g]==1 && !words[g+1],"Native angular/finite-face guide refused a proved interior source");
                    }
                    if(words[g]) {
                        check(std::equal(words.begin()+at+36,words.begin()+at+40,words.begin()+g+4),
                            "Native guide did not retain its exact certified enclosure");
                        const float x=std::bit_cast<float>(words[g+8]),y=std::bit_cast<float>(words[g+9]);
                        check(x>=std::bit_cast<float>(words[g+4]) && x<=std::bit_cast<float>(words[g+6])
                            && y>=std::bit_cast<float>(words[g+5]) && y<=std::bit_cast<float>(words[g+7])
                            && std::abs(x-expected[l].x)<=1./4096 && std::abs(y-expected[l].y)<=1./4096
                            && words[g+11]==std::bit_cast<unsigned>(1.F)
                            && std::abs(std::bit_cast<float>(words[g+12]))<=.002F
                            && std::abs(std::bit_cast<float>(words[g+13]))<=.002F && !words[g+15],
                            "Native guide escaped proof or contradicted independent old path/angular gate");
                    }
                    if(!round){guide_proved+=words[g];guide_refused+=!words[g];}
                }
                if(colours) {
                    check(std::equal(guide_words.begin()+at,guide_words.begin()+at+96,words.begin()+at),"Colour stage rewrote its root/source/fold/angular proof");
                    const unsigned c=at+96;check(words[c]<=1 && words[c+1]<=6 && !words[c+3],"Unknown native colour validity/refusal/reserved words");
                    if(!words[at+80])check(!words[c] && words[c+1]==1,"Upstream guide refusal bypassed colour admission");
                    else {
                        if(!words[c])std::cerr<<"Native colour refused lobe="<<l<<" reason="<<words[c+1]<<'\n';
                        check(words[c]==1 && !words[c+1] && words[c+2]==6,"Colour admitted invalid CURRENT neighbours or refused complete input");
                        const auto linear=[&](unsigned rgba,unsigned channel) {
                            const double x=double((rgba>>(channel*8))&255)/255;
                            return srgb?(x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4)):x;
                        };
                        const double x=std::bit_cast<float>(words[at+88])-.5,y=std::bit_cast<float>(words[at+89])-.5;
                        const int sx=int(std::floor(x)),sy=int(std::floor(y));const double fx=x-sx,fy=y-sy;
                        for(unsigned channel=0;channel<3;++channel) {
                            const auto raw=[&](const Words& image,unsigned p){return image[layout.incoming_offset/4+(p*lobes+l)*(curved?16:13)+9];};
                            const double incoming=linear(raw(current_inputs,pixel),channel);double low=incoming,high=incoming,prior=0;
                            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                                if((dx==-1 && dy==-1) || (dx==-1 && dy==0) || (dx==1 && dy==1))continue;
                                const auto value=linear(raw(current_inputs,unsigned(int(pixel)+dy*int(w)+dx)),channel);
                                low=std::min(low,value);high=std::max(high,value);
                            }
                            for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
                                const double share=(dx?fx:1-fx)*(dy?fy:1-fy);
                                const unsigned p=unsigned(std::min(sy+int(dy),int(h)-1))*w+unsigned(std::min(sx+int(dx),int(w)-1));
                                prior+=linear(raw(bank,p),channel)*share;
                            }
                            const double admitted=incoming+(std::clamp(prior,low,high)-incoming)*double(.85F);
                            const double expected[]{incoming,low,high,prior,admitted};
                            for(unsigned slot=0;slot<5;++slot)check(std::abs(std::bit_cast<float>(words[c+4+slot*4+channel])-expected[slot])<=1.e-5,
                                "Native incident colour differs from independent fresh-neighbour/linear bilinear clamp");
                        }
                    }
                    if(!round){colour_proved+=words[c];colour_refused+=!words[c];}
                }
                if(compose) {
                    check(std::equal(colour_words.begin()+at,colour_words.begin()+at+128,words.begin()+at),
                        "Whole-pixel composition overwrote its optical/incident-colour proofs");
                    const auto decode=[&](unsigned rgba,unsigned c) {
                        const double x=double((rgba>>(c*8))&255)/255;
                        return srgb?(x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4)):x;
                    };
                    const auto pack=[&](const std::array<double,3>& colour,unsigned alpha) {
                        unsigned rgba=alpha;
                        for(unsigned c=0;c<3;++c) {
                            double value=std::clamp(colour[c],0.,1.);
                            if(srgb)value=value<=.0031308?12.92*value:1.055*std::pow(value,1/2.4)-.055;
                            rgba|=unsigned(std::floor(value*255+.5))<<(c*8);
                        }
                        return rgba;
                    };
                    std::array<unsigned,8> incident{},transport{};std::array<double,3> sum{},pixel_rgb{};unsigned mask=0;
                    for(unsigned sample=0;sample<lobes;++sample) {
                        const unsigned path=layout.incoming_offset/4+(pixel*lobes+sample)*(curved?16:13);
                        const unsigned raw=current_inputs[path+9];std::array<double,3> incoming{},light{};
                        const bool use_old=!bad_depth && !bad_feature && !(bad_fold && (!mixed_reject || sample+1==lobes));
                        if(use_old)mask|=1U<<sample;
                        for(unsigned c=0;c<3;++c) {
                            incoming[c]=decode(raw,c);
                            if(use_old) {
                                const unsigned proof=start+sample*6144;
                                const double x=std::bit_cast<float>(colour_words[proof+88])-.5,y=std::bit_cast<float>(colour_words[proof+89])-.5;
                                const int sx=int(std::floor(x)),sy=int(std::floor(y));const double fx=x-sx,fy=y-sy;
                                double prior=0,low=incoming[c],high=incoming[c];
                                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                                    if((dx==-1 && dy==-1) || (dx==-1 && dy==0) || (dx==1 && dy==1))continue;
                                    const unsigned neighbour=unsigned(int(pixel)+dy*int(w)+dx);
                                    const auto value=decode(current_inputs[layout.incoming_offset/4+(neighbour*lobes+sample)*(curved?16:13)+9],c);
                                    low=std::min(low,value);high=std::max(high,value);
                                }
                                for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
                                    const unsigned p=unsigned(std::min(sy+int(dy),int(h)-1))*w+unsigned(std::min(sx+int(dx),int(w)-1));
                                    const double share=(dx?fx:1-fx)*(dy?fy:1-fy);
                                    prior+=decode(bank[layout.incoming_offset/4+(p*lobes+sample)*(curved?16:13)+9],c)*share;
                                }
                                incoming[c]+=(std::clamp(prior,low,high)-incoming[c])*double(.85F);
                            }
                            const double throughput=std::bit_cast<float>(current_inputs[path+10+c]);
                            const double base=curved?std::bit_cast<float>(current_inputs[path+13+c]):0;
                            light[c]=incoming[c]*throughput+base;
                        }
                        incident[sample]=use_old?pack(incoming,raw&0xff000000U):raw;
                        transport[sample]=pack(light,0xff000000U);
                        for(unsigned c=0;c<3;++c)sum[c]+=decode(transport[sample],c);
                    }
                    for(unsigned c=0;c<3;++c) {
                        const double base=curved?std::bit_cast<float>(current_inputs[layout.base_offset/4+pixel*4+c]):0;
                        const double response=std::bit_cast<float>(current_inputs[layout.weight_offset/4+pixel*4+c]);
                        pixel_rgb[c]=base+(sum[c]/lobes)*response;
                    }
                    const unsigned canonical=mask?pack(pixel_rgb,current_inputs[pixel]&0xff000000U):current_inputs[pixel];
                    const unsigned packet=at+128;
                    check(words[packet]==unsigned(mask!=0) && words[packet+1]==unsigned(mask==0)
                        && words[packet+2]==pixel && words[packet+3]==mask,
                        "Whole-pixel packet dropped a CURRENT/refused lobe or admitted an unproven sample");
                    check(words[packet+4]==canonical && words[packet+5]==incident[l] && words[packet+6]==(mask?transport[l]:0)
                        && !words[packet+7],"Complete pixel/native quantization or per-lobe incident/transport differs from independent CURRENT-material recipe");
                    for(unsigned c=0;c<3;++c)check(std::abs(std::bit_cast<float>(words[packet+8+c])-(mask?pixel_rgb[c]:0))<=1.e-5,
                        "Whole-pixel linear composition used stale material/base or averaged unquantized lobes");
                    for(unsigned n=11;n<16;++n)check(!words[packet+n],"Whole-pixel packet reserved fields changed");
                    if(!round && !l)composed_pixels+=unsigned(mask!=0);
                }
                if(compose)for(unsigned n=120;n<128;++n)check(!words[at+n],"Composition overwrote the reserved pre-packet gap");
                for(unsigned n=compose?144U:colours?120U:guides?96U:folds?80U:64U;n<6144;++n)check(!words[at+n],"Source proof retained a stale local-root slot");
            }
            check(gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,"Source witness modified its immutable accepted bank");
            if(colours)check(gpu.read(owner.output().buffer,layout.storage_bytes)==current_capture->read(),
                "Native colour/proof chain modified fresh CURRENT canonical/incident RGB, base, throughput or paths");
            owner.discard();check(cleared() && owner.working_image_bytes()==complete,"Discard retained a witness or lost reusable scratch");
        }
        std::cout<<"Native source footprint family="<<(curved?material==3?"lava":"water":finite?"finite-target":"sky-target")
            <<" lobes="<<lobes<<" boundary="<<boundary<<" bad-depth="<<bad_depth<<" source-hole="<<bad_feature<<" proved="<<proved
            <<" fold-mode="<<folds<<" raw-fold="<<bad_fold<<" fold-proved="<<fold_proved<<" fold-refused="<<fold_refused
            <<" ordered-hops="<<ordered_hops
            <<" guide-mode="<<guides<<" guide-proved="<<guide_proved<<" guide-refused="<<guide_refused
            <<" colour-mode="<<colours<<" srgb="<<srgb<<" colour-proved="<<colour_proved<<" colour-refused="<<colour_refused
            <<" compose-mode="<<compose<<" mixed-reject="<<mixed_reject<<" composed-pixels="<<composed_pixels
            <<" held/refused-command/immutability passed; complete-colour/FPS unqualified.\n";
    }
    if(compose && !publish && lobes==8 && !boundary && !bad_depth && !bad_feature && !bad_fold && !ordered_hops) {
        // Refuse the WHOLE pixel even when earlier lobes already authorized
        // old radiance. Corrupt only raw CURRENT material, never a GPU proof,
        // source box, root seed or history-colour choice. Keep raw NaN bits.
        for(unsigned fault=0;fault<(curved?4U:3U);++fault) {
            auto broken=current_inputs;
            const unsigned last=layout.incoming_offset/4+(pixel*lobes+lobes-1)*(curved?16:13);
            const bool primary_fault=fault==2;
            if(fault==0)broken[last+10]=std::bit_cast<unsigned>(-.125F);
            else if(fault==1)broken[last+12]=0x7fc01234U;
            else if(primary_fault)broken[layout.weight_offset/4+pixel*4]=std::bit_cast<unsigned>(1.125F);
            else broken[last+14]=0x7fc05678U;
            cb=stage(broken);CurrentBankCapture fresh(gpu,layout.storage_bytes);fresh.record(cb,owner.output().buffer);
            check(owner.enqueue_source_queries(cb,pixel*lobes,lobes) && owner.enqueue_source_optical(cb)
                && owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb)
                && owner.enqueue_source_folds(cb) && owner.enqueue_source_guide(cb)
                && owner.enqueue_source_colour(cb),owner.status().c_str());
            const auto colour=owner.source_colour();const auto& optics=colour.guide.folds.witness.hull.optical;
            const GpuReflectionSourceLocalRoots roots{optics,colour.results};
            QueryCapture before(gpu,lobes,true,true),after(gpu,lobes,true,true);
            before.record(cb,owner.source_queries(),&optics,&roots);
            check(owner.enqueue_source_composition(cb),owner.status().c_str());
            after.record(cb,owner.source_queries(),&optics,&roots);gpu.submit(cb);
            const auto prior=before.read(),words=after.read();const unsigned start=lobes*2772;
            for(unsigned l=0;l<lobes;++l) {
                const unsigned at=start+l*6144,packet=at+128,path=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13);
                const bool admitted=!primary_fault && l+1<lobes;
                // An invalid PRIMARY is already protected by fresh capture;
                // every optical/colour stage must refuse upstream. In contrast
                // raw late-lobe material faults reach seven admitted colours.
                const unsigned colour_reason=admitted?0U:primary_fault?1U:3U;
                check(prior[at+96]==unsigned(admitted) && prior[at+97]==colour_reason,
                    ("Late CURRENT material admission mismatch fault="+std::to_string(fault)+" lobe="+std::to_string(l)
                    +" colour="+std::to_string(prior[at+96])+" reason="+std::to_string(prior[at+97])
                    +" root="+std::to_string(prior[at])+" footprint="+std::to_string(prior[at+48])
                    +" fold="+std::to_string(prior[at+64])+" guide="+std::to_string(prior[at+80])).c_str());
                if(primary_fault)check(!prior[at] && !prior[at+48] && !prior[at+64] && !prior[at+80],
                    "Invalid primary bypassed protected capture and retained an optical/colour certificate");
                check(std::equal(prior.begin()+at,prior.begin()+at+128,words.begin()+at),
                    "Invalid material composition overwrote preceding optical/colour proofs");
                check(!words[packet] && words[packet+1]==3 && words[packet+2]==pixel && !words[packet+3]
                    && words[packet+4]==broken[pixel] && words[packet+5]==broken[path+9],
                    "Late invalid CURRENT material published a partial pixel instead of exact CURRENT fallback");
                for(unsigned n=6;n<16;++n)check(!words[packet+n],"Invalid material retained partial incident/transport/pixel fields");
            }
            check(gpu.read(owner.output().buffer,layout.storage_bytes)==fresh.read(),
                "Material refusal modified fresh CURRENT data or raw NaN payloads");
            check(gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,
                "Material refusal modified its immutable accepted bank");
            owner.discard();check(cleared() && owner.working_image_bytes()==complete,
                "Material refusal retained a pixel descriptor or allocated another scratch bank");
        }
        std::cout<<"Native whole-pixel material refusal family="<<(curved?material==3?"lava":"water":finite?"finite-target":"sky-target")
            <<" negative/nonfinite final-lobe material and invalid primary response retain exact CURRENT; no partial publication.\n";
    }
    if(ordered_hops>=2) {
        auto reordered=current_inputs;
        for(unsigned l=0;l<lobes;++l) {
            const unsigned at=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13);
            std::swap(reordered[at],reordered[at+1]);
        }
        cb=stage(reordered);
        check(owner.enqueue_source_queries(cb,pixel*lobes,lobes) && owner.enqueue_source_optical(cb)
            && owner.enqueue_source_root_hull(cb),owner.status().c_str());
        const auto hull=owner.source_root_hull();const GpuReflectionSourceLocalRoots roots{hull.optical,hull.results};
        check(owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb),owner.status().c_str());
        if(guides)check(owner.enqueue_source_guide(cb),owner.status().c_str());
        if(colours)check(owner.enqueue_source_colour(cb),owner.status().c_str());
        if(compose)check(owner.enqueue_source_composition(cb),owner.status().c_str());
        QueryCapture capture(gpu,lobes,true,true);capture.record(cb,owner.source_queries(),&hull.optical,&roots);
        gpu.submit(cb);const auto words=capture.read();
        for(unsigned l=0;l<lobes;++l) {
            const unsigned domain=lobes*80+l*1540,at=lobes*2772+l*6144;
            check(!words[domain] && !words[domain+1],"Native source cover ignored ordered mirror identity");
            check(!words[at] && words[at+3]==4096 && !words[at+48] && words[at+50]==1
                && !words[at+64] && words[at+65]==1,"Reordered complete path acquired a root/source/fold proof");
            if(guides)check(!words[at+80] && words[at+81]==1,"Reordered path bypassed native guide upstream gate");
            if(colours)check(!words[at+96] && words[at+97]==1,"Reordered path bypassed native colour upstream gate");
            if(compose) {
                const unsigned path=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13),packet=at+128;
                check(!words[packet] && words[packet+1]==1 && words[packet+2]==pixel && !words[packet+3]
                    && words[packet+4]==reordered[pixel] && words[packet+5]==reordered[path+9],
                    "Reordered path changed the complete CURRENT fallback");
                for(unsigned n=6;n<16;++n)check(!words[packet+n],"Reordered path retained a partial composition");
            }
        }
        check(gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,"Reordered path mutated its accepted source bank");
        owner.discard();check(cleared(),"Reordered-path refusal retained a usable source descriptor");
        std::cout<<"Native ordered-path refusal lobes="<<lobes<<" hops="<<ordered_hops<<" complete order retained.\n";
    }
    // Quota failure must remain a refusal in the downstream stage, even when
    // a previous valid batch left certificates in the same reusable allocation.
    cb=stage(current_inputs);ReflectionSourceQueryLimits limits;limits.supports=1;
    check(owner.enqueue_source_queries(cb,7*lobes,1,limits),owner.status().c_str());
    check(owner.enqueue_source_optical(cb) && encode(cb),owner.status().c_str());
    QueryCapture refused(gpu,1,true,true);const auto q=owner.source_queries();const auto optical=owner.source_optical();const auto roots=active();
    refused.record(cb,q,&optical,&roots);gpu.submit(cb);const auto refusal=refused.read();
    check(refusal[81]!=0,"Native support-quota fixture unexpectedly produced a complete cover");
    for(unsigned n=0;n<128;++n)check(!refusal[2772+n*48] && refusal[2772+n*48+3]==32,
        "Incomplete native support cover reused a previous local certificate");owner.discard();
    if(witness) {
        const unsigned queries=compose?lobes:1,root_start=queries*2772;
        cb=stage(current_inputs);check(owner.enqueue_source_queries(cb,7*lobes,queries,limits) && owner.enqueue_source_optical(cb)
            && owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb),owner.status().c_str());
        const auto source=owner.source_witness();const GpuReflectionSourceLocalRoots copy{source.hull.optical,source.results};
        if(folds)check(owner.enqueue_source_folds(cb),owner.status().c_str());
        if(guides)check(owner.enqueue_source_guide(cb),owner.status().c_str());
        if(colours)check(owner.enqueue_source_colour(cb),owner.status().c_str());
        if(compose)check(owner.enqueue_source_composition(cb),owner.status().c_str());
        QueryCapture capture(gpu,queries,true,true);capture.record(cb,owner.source_queries(),&source.hull.optical,&copy);gpu.submit(cb);
        const auto words=capture.read();check(!words[root_start+48] && (words[root_start+50]==1 || words[root_start+50]==8),
            "Quota-refused hull retained a successful source-footprint witness");
        if(folds)check(!words[root_start+64] && words[root_start+65]==1,"Quota-refused upstream proof bypassed native fold guard");
        if(guides)check(!words[root_start+80] && words[root_start+81]==1,"Quota-refused upstream proof bypassed native guide");
        if(colours)check(!words[root_start+96] && words[root_start+97]==1,"Quota-refused upstream proof bypassed native colour");
        if(compose)for(unsigned l=0;l<lobes;++l) {
            const unsigned at=root_start+l*6144+128,path=layout.incoming_offset/4+(7*lobes+l)*(curved?16:13);
            check(!words[at] && words[at+1] && words[at+2]==7 && !words[at+3]
                && words[at+4]==current_inputs[7] && words[at+5]==current_inputs[path+9],
                "Quota refusal changed a CURRENT complete-pixel fallback");
            for(unsigned n=6;n<16;++n)check(!words[at+n],"Quota refusal exposed partial composition");
        }
        owner.discard();
    }
    if(!curved)for(unsigned fault=0;fault<(finite?2U:1U+ordered_hops);++fault) {
        const unsigned broken_face=fault?(ordered_hops?2+fault:17):pixel;
        auto broken=geometry;broken[count*12+broken_face*12+3]=0;
        Buffer malformed(gpu,broken);guide.buffer=malformed.value;cb=stage(current_inputs);
        const unsigned queries=compose?lobes:1,root_start=queries*2772;
        check(owner.enqueue_source_queries(cb,pixel*lobes,queries) && owner.enqueue_source_optical(cb)
            && encode(cb),owner.status().c_str());
        QueryCapture capture(gpu,queries,true,true);const auto query=owner.source_queries();const auto optics=owner.source_optical();const auto local=active();
        if(folds)check(owner.enqueue_source_witness(cb) && owner.enqueue_source_folds(cb),owner.status().c_str());
        if(guides)check(owner.enqueue_source_guide(cb),owner.status().c_str());
        if(colours)check(owner.enqueue_source_colour(cb),owner.status().c_str());
        if(compose)check(owner.enqueue_source_composition(cb),owner.status().c_str());
        capture.record(cb,query,&optics,&local);gpu.submit(cb);const auto words=capture.read();
        check(words[queries*80]>0 && !words[queries*80+1] && !words[queries*1620+124],"Malformed accepted-face fixture did not reach frame refusal");
        for(unsigned n=0;n<(whole_cover?1U:words[queries*80]);++n)check(!words[root_start+n*48] && words[root_start+n*48+3]==32,
            "Malformed native optical frame retained a usable local certificate");
        if(folds)check(!words[root_start+64] && (words[root_start+65]==1 || words[root_start+65]==8),"Malformed frame bypassed native fold guard");
        if(guides)check(!words[root_start+80] && (words[root_start+81]==1 || words[root_start+81]==2),"Malformed frame bypassed native guide");
        if(colours)check(!words[root_start+96] && words[root_start+97]==1,"Malformed frame bypassed native colour");
        if(compose)for(unsigned l=0;l<lobes;++l) {
            const unsigned at=root_start+l*6144+128,path=layout.incoming_offset/4+(pixel*lobes+l)*(curved?16:13);
            check(!words[at] && words[at+1]==1 && words[at+2]==pixel && !words[at+3]
                && words[at+4]==current_inputs[pixel] && words[at+5]==current_inputs[path+9],
                "Malformed geometry changed complete CURRENT fallback");
            for(unsigned n=6;n<16;++n)check(!words[at+n],"Malformed geometry exposed partial composition");
        }
        owner.discard();guide.buffer=positions.value;
    }
    if(publish) {
        const auto chain=[&](SDL_GPUCommandBuffer* command,unsigned first,unsigned batch) {
            check(owner.enqueue_source_queries(command,first,batch) && owner.enqueue_source_optical(command)
                && owner.enqueue_source_root_hull(command) && owner.enqueue_source_witness(command)
                && owner.enqueue_source_folds(command) && owner.enqueue_source_guide(command)
                && owner.enqueue_source_colour(command) && owner.enqueue_source_composition(command),owner.status().c_str());
        };
        auto predicted=complete+layout.storage_bytes+(admission_diagnostics?count*4:0);
        // A gap cannot start a hidden image or allocate a third bank.
        cb=stage(current_inputs);chain(cb,pixel*lobes,lobes);
        check(owner.source_publication_allocation_bytes()==predicted && !owner.enqueue_source_publication(cb)
            && !owner.source_publication_complete() && owner.output().buffer==owner.fresh_current_output().buffer
            && owner.working_image_bytes()==complete,"Publication accepted a nonzero starting batch or allocated on refusal");
        gpu.submit(cb);owner.discard();
        // Abandon a real started frame, not only a rejected first batch.
        cb=stage(current_inputs);const auto partial_fresh=owner.fresh_current_output();
        CurrentBankCapture partial_original(gpu,layout.storage_bytes);partial_original.record(cb,partial_fresh.buffer);
        chain(cb,0,64);check(owner.enqueue_source_publication(cb) && !owner.source_publication_complete()
            && owner.output().buffer==partial_fresh.buffer && owner.working_image_bytes()==predicted,
            "Started incomplete frame exposed a separate image");
        gpu.submit(cb);check(gpu.read(partial_fresh.buffer,layout.storage_bytes)==partial_original.read(),
            "Started incomplete frame changed fresh CURRENT");owner.discard();
        check(!owner.source_publication_complete() && !owner.output().buffer && owner.accepted_output().buffer==accepted.source_buffer
            && gpu.read(accepted.source_buffer,layout.storage_bytes)==bank && owner.working_image_bytes()==predicted,
            "Discard of started partial publication replaced accepted history or leaked storage");
        void* published=nullptr;
        for(unsigned round=0;round<2;++round) {
            cb=stage(current_inputs);const auto fresh=owner.fresh_current_output();
            CurrentBankCapture original(gpu,layout.storage_bytes);original.record(cb,fresh.buffer);
            std::vector<std::unique_ptr<QueryCapture>> captures;std::vector<unsigned> counts,starts;
            void* resident_packets=nullptr;
            for(unsigned first=0;first<count*lobes;) {
                const unsigned batch=std::min(64U,count*lobes-first);chain(cb,first,batch);
                const auto composed=owner.source_composition();const auto& optics=composed.colour.guide.folds.witness.hull.optical;
                resident_packets=composed.results;
                const GpuReflectionSourceLocalRoots roots{optics,composed.results};
                auto capture=std::make_unique<QueryCapture>(gpu,batch,true,true);
                capture->record(cb,owner.source_queries(),&optics,&roots);
                if(!first)check(!owner.enqueue_source_publication(nullptr) && !owner.enqueue_source_publication(&owner)
                    && owner.source_composition().results==composed.results && !owner.source_publication_complete(),
                    "Foreign/null publication exposed a frame or invalidated a retriable whole-pixel packet");
                check(owner.enqueue_source_publication(cb),owner.status().c_str());first+=batch;
                check(owner.working_image_bytes()==predicted && owner.source_publication_allocation_bytes()==predicted,
                    "Publication omitted its complete third bank or allocated per batch");
                check(owner.accepted_output().buffer==accepted.source_buffer && owner.accepted_source_index().buffer==accepted.buffer,
                    "Partial publication accepted an image/index before presentation");
                if(first<count*lobes)check(!owner.source_publication_complete() && owner.output().buffer==fresh.buffer
                    && owner.source_index().source_buffer==fresh.buffer,"Partial composed image escaped before complete frame coverage");
                else check(owner.source_publication_complete() && owner.output().buffer!=fresh.buffer
                    && owner.source_index().source_buffer==owner.output().buffer,"Complete publication detached image/index or overwrote fresh bank");
                captures.push_back(std::move(capture));counts.push_back(batch);starts.push_back(first-batch);
            }
            published=owner.output().buffer;
            check(!owner.enqueue_source_queries(cb,0,lobes) && owner.source_publication_complete()
                && owner.output().buffer==published,"A completed frame allowed another query to mutate its published image");
            gpu.submit(cb);auto expected_image=original.read();Words expected_admission(count);unsigned changed=0;
            for(unsigned batch=0;batch<captures.size();++batch) {
                const auto words=captures[batch]->read();const unsigned root_start=counts[batch]*2772;
                for(unsigned q=0;q<counts[batch];q+=lobes) {
                    const unsigned at=root_start+q*6144+128,p=(starts[batch]+q)/lobes;
                    if(!words[at])continue;
                    check(words[at+1]==0 && words[at+2]==p && words[at+3],"Publication packet lost its whole-pixel identity");
                    ++changed;expected_image[p]=words[at+4];expected_admission[p]=words[at+3];
                    for(unsigned l=0;l<lobes;++l) {
                        const unsigned path=layout.incoming_offset/4+((starts[batch]+q+l)*(curved?16:13));
                        expected_image[path+9]=words[at+l*6144+5];
                    }
                }
            }
            check(changed && expected_image[pixel]!=original.read()[pixel],"Full-frame fixture did not exercise a changed certified pixel");
            check(gpu.read(published,layout.storage_bytes)==expected_image,"Published complete image differs from complete staged packets/CURRENT fallbacks");
            check(gpu.read(fresh.buffer,layout.storage_bytes)==original.read(),"Publication polluted fresh CURRENT evidence across batches");
            check(gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,"Publication changed an accepted source before commit");
            if(admission_diagnostics)check(gpu.read(cold_admission.buffer,count*4)==zero_admission,
                "Pending publication mutated the old accepted diagnostic mask");
            if(!round) {
                owner.discard();check(!owner.output().buffer && !owner.fresh_current_output().buffer && !owner.source_publication_complete()
                    && owner.accepted_output().buffer==accepted.source_buffer && owner.working_image_bytes()==predicted,
                    "Discard accepted a partial image or lost bounded reusable third storage");
                // The real front-end convenience entry must encode exactly the
                // same COMPLETE stream as the independently inspected batches.
                // No diagnostic readback is re-uploaded or used as an input.
                cb=stage(current_inputs);const auto streamed_fresh=owner.fresh_current_output();
                CurrentBankCapture streamed_original(gpu,layout.storage_bytes);streamed_original.record(cb,streamed_fresh.buffer);
                check(!owner.enqueue_source_frame(nullptr) && !owner.enqueue_source_frame(&owner)
                    && !owner.enqueue_source_frame(cb,nullptr,&owner)
                    && owner.working_image_bytes()==predicted && !owner.source_publication_complete(),
                    "Null/foreign complete stream changed cached state");
                check(owner.source_frame_allocation_bytes()==predicted+GpuReflectionSourceOptical::stream_working_bytes,
                    "Complete-frame preflight omitted lazy GPU-only optical scheduling scratch");
                predicted+=GpuReflectionSourceOptical::stream_working_bytes;
                struct StreamBatchCaptures {
                    Gpu& gpu;const std::vector<unsigned>& starts;const std::vector<unsigned>& counts;
                    std::vector<std::unique_ptr<QueryCapture>> packets;
                } stream_captures{gpu,starts,counts,{}};
                const auto observe=+[](void* context,void* command,const GpuReflectionSourceComposition& composed) {
                    auto& trace=*static_cast<StreamBatchCaptures*>(context);
                    const auto& optics=composed.colour.guide.folds.witness.hull.optical;
                    const auto& q=optics.queries;const auto batch=trace.packets.size();
                    check(batch<trace.counts.size() && q.first==trace.starts[batch] && q.count==trace.counts[batch],
                        "Complete stream omitted/reordered/resized an independently captured manual batch");
                    const GpuReflectionSourceLocalRoots roots{optics,composed.results};
                    auto capture=std::make_unique<QueryCapture>(trace.gpu,q.count,true,true);
                    capture->record(static_cast<SDL_GPUCommandBuffer*>(command),q,&optics,&roots);
                    trace.packets.push_back(std::move(capture));
                };
                check(owner.enqueue_source_frame(cb,observe,&stream_captures) && owner.source_publication_complete()
                    && owner.output().buffer!=streamed_fresh.buffer
                    && owner.source_index().source_buffer==owner.output().buffer
                    && owner.working_image_bytes()==predicted,owner.status().c_str());
                check(owner.source_frame_allocation_bytes()==predicted,
                    "Complete-frame cache forecast duplicated or lost resident optical work map/arguments");
                const auto streamed=owner.output();
                // Publication deliberately seals/clears its stage descriptors.
                // The bounded scratch itself is still the same resident buffer:
                // borrow that diagnostic handle, never replay old descriptors
                // as input or expose a sealed proof packet through the owner.
                const auto streamed_optics=owner.source_optical();
                check(resident_packets && streamed_optics.queries.first==starts.back()
                    && streamed_optics.queries.count==counts.back() && !owner.source_composition().results,
                    "Stream did not retain the final native scratch or seal its proof descriptors");
                const GpuReflectionSourceLocalRoots streamed_roots{streamed_optics,resident_packets};
                QueryCapture streamed_packets(gpu,counts.back(),true,true);
                streamed_packets.record(cb,owner.source_queries(),&streamed_optics,&streamed_roots);
                check(!owner.enqueue_source_frame(cb) && owner.output().buffer==streamed.buffer,
                    "Completed convenience stream restarted or replaced its output");
                gpu.submit(cb);
                check(stream_captures.packets.size()==captures.size(),
                    "Full stream packet oracle did not capture EVERY complete-frame batch");
                for(unsigned batch=0;batch<captures.size();++batch) {
                    const auto actual=stream_captures.packets[batch]->read(),expected=captures[batch]->read();
                    if(actual!=expected) {
                        unsigned differences=0;
                        for(unsigned n=0;n<actual.size();++n)if(actual[n]!=expected[n]) {
                            if(differences++<16) {
                                std::cout<<"Native all-batch stream difference batch="<<batch
                                    <<" first="<<starts[batch]<<" count="<<counts[batch]<<" word="<<n
                                    <<" stream/manual="<<std::hex<<actual[n]<<'/'<<expected[n]<<std::dec;
                                const auto domain_begin=counts[batch]*80,domain_end=counts[batch]*1620;
                                if(n>=domain_begin && n<domain_end) {
                                    const auto query=(n-domain_begin)/1540,word=(n-domain_begin)%1540;
                                    const auto header=domain_begin+query*1540;
                                    std::cout<<" support-query="<<query<<" support-word="<<word
                                        <<" emitted="<<actual[header]<<'/'<<expected[header]
                                        <<" status="<<actual[header+1]<<'/'<<expected[header+1];
                                }
                                std::cout<<'\n';
                            }
                        }
                        std::cout<<"Native all-batch stream differences="<<differences<<std::endl;
                    }
                    check(actual==expected,"Complete stream changed raw packet words in an earlier or final batch");
                }
                check(gpu.read(streamed.buffer,layout.storage_bytes)==expected_image
                    && gpu.read(streamed_fresh.buffer,layout.storage_bytes)==streamed_original.read()
                    && gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,
                    "Convenience stream diverged from whole-pixel packets or changed fresh/accepted evidence");
                const auto streamed_words=streamed_packets.read(),manual_words=captures.back()->read();
                if(streamed_words!=manual_words) {
                    unsigned differences=0;const unsigned root_start=counts.back()*2772;
                    for(unsigned n=0;n<streamed_words.size();++n)if(streamed_words[n]!=manual_words[n]) {
                        if(differences++<16)std::cout<<"Native stream packet difference word="<<n
                            <<" root-query="<<(n>=root_start?int((n-root_start)/6144):-1)
                            <<" root-word="<<(n>=root_start?int((n-root_start)%6144):-1)
                            <<" stream/manual="<<std::hex<<streamed_words[n]<<'/'<<manual_words[n]<<std::dec
                            <<" float="<<std::bit_cast<float>(streamed_words[n])<<'/'<<std::bit_cast<float>(manual_words[n])<<'\n';
                    }
                    std::cout<<"Native stream packet differences="<<differences<<std::endl;
                }
                check(streamed_words==manual_words,
                    "Stream scheduling changed raw query/optics/root/footprint/fold/guide/colour/material packet words");
                owner.discard();check(owner.accepted_output().buffer==accepted.source_buffer
                    && !owner.output().buffer && owner.working_image_bytes()==predicted,
                    "Discard of convenience stream accepted or leaked its complete image");
                if(admission_diagnostics)check(owner.accepted_source_admission().buffer==cold_admission.buffer
                    && gpu.read(cold_admission.buffer,count*4)==zero_admission,"Discard accepted or mutated a published diagnostic mask");
            } else {
                owner.commit();check(owner.accepted_output().buffer==published && owner.accepted_source_index().source_buffer==published
                    && gpu.read(published,layout.storage_bytes)==expected_image && !owner.output().buffer && !owner.source_publication_complete()
                    && owner.working_image_bytes()==predicted,"Presentation commit failed the exact complete image/index pair");
                if(admission_diagnostics) {
                    const auto admitted=owner.accepted_source_admission();
                    check(admitted.buffer && admitted.buffer!=cold_admission.buffer && admitted.buffer!=published
                        && admitted.bytes==count*4 && gpu.read(admitted.buffer,count*4)==expected_admission,
                        "Full commit failed to pair every certified/refused lobe mask with the exact published image");
                }
            }
        }
        guide.previous_cube=guide.current_cube;cb=stage(current_inputs);
        const auto reuse=owner.allocation_bytes(w,h,layout.storage_bytes/count,true,admission_diagnostics);
        check(reuse==predicted && owner.source_publication_allocation_bytes()==predicted
            && owner.source_frame_allocation_bytes()==predicted,
            "Same-layout preflight omitted cached publication or allocated an extra complete bank");
        gpu.submit(cb);owner.discard();
        // Even an explicit commit of an incomplete stream accepts only RAW
        // CURRENT, never the partly composed separate image.
        cb=stage(current_inputs);const auto incomplete=owner.fresh_current_output();
        CurrentBankCapture incomplete_original(gpu,layout.storage_bytes);incomplete_original.record(cb,incomplete.buffer);
        chain(cb,0,64);check(owner.enqueue_source_publication(cb) && !owner.source_publication_complete()
            && owner.output().buffer==incomplete.buffer,"Incomplete stream exposed partial publication before commit");
        gpu.submit(cb);owner.commit();check(owner.accepted_output().buffer==incomplete.buffer
            && owner.accepted_source_index().source_buffer==incomplete.buffer
            && gpu.read(incomplete.buffer,layout.storage_bytes)==incomplete_original.read(),
            "Incomplete stream commit accepted a partly composed image instead of full fresh CURRENT");
        if(admission_diagnostics)check(gpu.read(owner.accepted_source_admission().buffer,count*4)==zero_admission,
            "Incomplete commit published partial lobe admission instead of fresh CURRENT mask");
        guide.previous_cube=guide.current_cube;cb=stage(current_inputs,false);gpu.submit(cb);owner.commit();
        check(owner.working_image_bytes()==std::uint64_t(layout.storage_bytes)*2 && !owner.source_publication_allocation_bytes()
            && !owner.source_frame_allocation_bytes(),
            "Disabling staging retained publication storage or unaccounted scratch");
        check(!owner.accepted_source_admission().buffer,"Disabled staging retained an accepted diagnostic mask");
        cb=stage(current_inputs);owner.reset();check(!owner.output().buffer && !owner.fresh_current_output().buffer
            && !owner.source_publication_complete() && !owner.enqueue_source_publication(cb),"Reset exposed a publication image");
        gpu.submit(cb);owner.discard();owner.release_device();check(!owner.working_image_bytes(),"Release retained publication resources");
        check(!owner.accepted_source_admission().buffer,"Release exposed an orphan diagnostic mask");
        std::cout<<"Native complete frame publication family="<<(curved?material==3?"lava":"water":finite?"finite-target":"sky-target")
            <<" lobes="<<lobes<<" full-records="<<count*lobes<<" admission-diagnostics="<<admission_diagnostics
            <<" manual/convenience fresh/accepted/batch/commit/discard/cached-bound/off/reset passed; no player-FPS qualification.\n";
        return;
    }
    cb=stage(current_inputs);check(owner.enqueue_source_queries(cb,pixel*lobes,lobes) && owner.enqueue_source_optical(cb)
        && encode(cb),owner.status().c_str());gpu.submit(cb);owner.commit();
    check(cleared() && !encode(cb),"Commit retained roots borrowed from a submitted command");
    guide.previous_cube=guide.current_cube;cb=stage(current_inputs,false);gpu.submit(cb);owner.commit();
    check(owner.working_image_bytes()==std::uint64_t(layout.storage_bytes)*2 && cleared(),
        "Disabling staging retained local-root scratch or accepted index");
    cb=stage(current_inputs);gpu.submit(cb);owner.commit();cb=stage(current_inputs);owner.reset();
    check(!encode(cb) && cleared(),"Reset exposed roots from a pending presentation");
    gpu.submit(cb);owner.discard();owner.release_device();
    check(!owner.working_image_bytes() && cleared() && !owner.source_local_root_allocation_bytes(),
        "Released owner retained native local-root scratch/descriptor");
    std::cout<<"Native local-root chain family="<<(curved?material==3?"lava":"water":finite?"finite-target":"sky-target")
        <<" lobes="<<lobes<<" whole-cover="<<whole_cover<<" boundary="<<boundary<<" unique="<<unique
        <<" ordered-hops="<<ordered_hops<<" held/quotas/lifecycle passed; local only, no global/RGB acceptance.\n";
}
