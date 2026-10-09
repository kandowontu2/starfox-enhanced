// Native owner queries, generated ONLY from GPU current path/geometry inputs.
// Downloads and authored expectations below are diagnostic assertions, not a
// production query upload, inverse seed, colour choice or optical certificate.
struct QueryCapture {
    Gpu& gpu;unsigned capacity,used{};bool optical{},local_roots{};SDL_GPUTransferBuffer* transfer{};
    ~QueryCapture(){if(transfer)SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);}
    QueryCapture(Gpu& g,unsigned n,bool optics=false,bool roots=false):gpu(g),capacity(n),optical(optics),local_roots(roots) {
        check(!local_roots || optical,"Local-root capture requires native optical inputs");
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,n*((optical?11088U:6480U)+(local_roots?24576U:0U)),0};
        transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
    }
    unsigned record(SDL_GPUCommandBuffer* cb,const GpuReflectionSourceQueries& q,const GpuReflectionSourceOptical* optics=nullptr,
        const GpuReflectionSourceLocalRoots* roots=nullptr) {
        check(q.count && used+q.count<=capacity,"Diagnostic query capture overflow");
        check(optical==bool(optics),"Optical capture missing native borrowed buffers");
        check(local_roots==bool(roots),"Local-root capture missing native borrowed buffer");
        auto* pass=SDL_BeginGPUCopyPass(cb);check(pass,SDL_GetError());
        const unsigned sizes[]{48,272,6160,512,4096,24576};
        void* buffers[]{q.queries,q.leaves,q.regions,optics?optics->frames:nullptr,optics?optics->results:nullptr,roots?roots->results:nullptr};unsigned base=0;
        for(unsigned n=0;n<(local_roots?6U:optical?5U:3U);++n) {
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(buffers[n]),0,q.count*sizes[n]};
            const SDL_GPUTransferBufferLocation to{transfer,base+used*sizes[n]};
            SDL_DownloadFromGPUBuffer(pass,&from,&to);base+=capacity*sizes[n];
        }
        SDL_EndGPUCopyPass(pass);const auto first=used;used+=q.count;return first;
    }
    Words read() {
        const auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());
        Words words(capacity*((optical?2772U:1620U)+(local_roots?6144U:0U)));std::memcpy(words.data(),mapped,words.size()*4);
        SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
        for(unsigned query=0;query<used;++query) {
            const unsigned at=capacity*12+query*68,count=words[at];
            check(count<=64,"Native leaf packet exceeded its complete 64-candidate capacity");
            for(unsigned leaf=count;leaf<64;++leaf)check(words[at+4+leaf]==0,
                "Native unused candidate tail retained an earlier query's leaf ID");
            const unsigned domain=capacity*80+query*1540,regions=words[domain];
            check(regions<=128,"Native support packet exceeded its complete 128-region capacity");
            for(unsigned region=regions;region<128;++region)for(unsigned word=0;word<12;++word)
                check(words[domain+4+region*12+word]==0,
                    "Native unused support tail retained an earlier query's region");
        }
        return words;
    }
};
Words query_fixture(unsigned w,unsigned h,unsigned lobes,bool curved,unsigned material,bool current,bool faults,unsigned colour) {
    const auto layout=native_layout(w,h,lobes,curved);const unsigned count=w*h;
    Words words(layout.storage_bytes/4,0);
    const auto primitive=[&](unsigned id){return current?count-1-id:id;};
    for(unsigned p=0;p<count;++p) {
        const bool liquid=curved && p%w==4;
        words[p]=(liquid?(material==0?0xfd000000U:0xfe000000U):0xff000000U)|0x336699U|colour;
        words[layout.identity_offset/4+p]=liquid?0xfffffffdU:primitive(p==8?7:p);
        words[layout.witness_offset/4+p]=std::bit_cast<unsigned>(400.F);
        for(unsigned c=0;c<4;++c)words[layout.weight_offset/4+p*4+c]=std::bit_cast<unsigned>(c==3?1.F:.5F);
        if(curved)words[layout.base_offset/4+p*4+3]=std::bit_cast<unsigned>(1.F);
        for(unsigned l=0;l<lobes;++l) {
            const unsigned at=layout.incoming_offset/4+(p*lobes+l)*(curved?16:13);
            unsigned hops=p%5,kind=hops==4?3:p%3==0?1:2,mask=0;
            if(p==7 || p==8){hops=0;kind=1;}
            for(unsigned n=0;n<4;++n) {
                const bool liquidHop=curved && n<hops && (p+n)%2;
                words[at+n]=n>=hops?UINT32_MAX:liquidHop?0xfffffffdU:primitive(3+n);
                if(liquidHop)mask|=1U<<n;
            }
            words[at+4]=(kind<<8)|(mask<<4)|hops;words[at+5]=kind==1?primitive(17):UINT32_MAX;
            const std::array<float,3> feature=kind==1?std::array{.25F+float(l)/128,.25F,0.F}:
                current?std::array{.8F,-.6F,0.F}:std::array{.6F,.8F,0.F};
            for(unsigned c=0;c<3;++c)words[at+6+c]=std::bit_cast<unsigned>(feature[c]);
            words[at+9]=0xff123456U|colour;for(unsigned c=10;c<13;++c)words[at+c]=std::bit_cast<unsigned>(.5F);
            if(faults) {
                if(p==5)words[at+4]|=8; // Reserved control bit.
                if(p==6)words[at+5]=count+3; // Unmatched finite terminal.
                if(p==10)words[at+5]=0; // Environment must have no terminal.
                if(p==11)words[at+6]=0x7fc01234U; // Nonfinite feature.
                if(p==13)words[at+4]=4U<<8; // Absorption has no source query.
            }
        }
        if(faults) {
            if(p==1)words[layout.witness_offset/4+p]=0x7fc01234U;
            if(p==2)words[layout.witness_offset/4+p]=std::bit_cast<unsigned>(-1.F);
            if(p==3)words[layout.weight_offset/4+p*4+3]=0;
        }
    }
    // A physically consistent accepted finite-receiver -> environment path,
    // independently formed from the eye ray and native rough quadrature. No
    // expected root is sent to the owner: only authored native path records.
    for(unsigned l=0;l<lobes;++l) {
        const unsigned at=layout.incoming_offset/4+(22*lobes+l)*(curved?16:13);
        for(unsigned n=0;n<4;++n)words[at+n]=UINT32_MAX;
        words[at+4]=2U<<8;words[at+5]=UINT32_MAX;
        const auto unit=[](std::array<double,3> v){const double n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(auto& c:v)c/=n;return v;};
        const auto cross=[](std::array<double,3> a,std::array<double,3> b){return std::array{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
        auto ray=unit({(5.5-8)/256,(1.5-4)/272,1});ray[2]=-ray[2];
        if(lobes==8) {
            const auto tangent=unit(cross(ray,{0,1,0})),bitangent=cross(ray,tangent);
            constexpr std::array<std::array<double,2>,8> taps{{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}}};
            for(unsigned c=0;c<3;++c)ray[c]+=.25*(tangent[c]*taps[l][0]+bitangent[c]*taps[l][1]);
            ray=unit(ray);
        }
        const std::array<float,3> feature=current?std::array{float(ray[1]),float(-ray[0]),float(ray[2])}:
            std::array{float(ray[0]),float(ray[1]),float(ray[2])};
        for(unsigned c=0;c<3;++c)words[at+6+c]=std::bit_cast<unsigned>(feature[c]);
    }
    return words;
}
void exercise_queries(Gpu& gpu,unsigned lobes,bool curved,unsigned material,bool optical=false) {
    constexpr unsigned w=17,h=9,count=w*h;const unsigned total=count*lobes;
    const auto layout=native_layout(w,h,lobes,curved);const auto old_authored=query_fixture(w,h,lobes,curved,material,false,false,0);
    Words geometry(count*25,0);
    const std::array<float,12> vertices{-100,-100,400,1,200,-100,400,1,-100,200,400,1};
    for(unsigned t=0;t<count;++t) {
        std::memcpy(geometry.data()+t*12,vertices.data(),48);
        std::memcpy(geometry.data()+count*12+t*12,vertices.data(),48);
        for(unsigned v=0;v<3;++v) {
            geometry[t*12+v*4+2]=std::bit_cast<unsigned>(800.F+float(t));
            geometry[count*12+t*12+v*4]=std::bit_cast<unsigned>(vertices[v*4]+float(t)/64);
        }
        geometry[count*24+t]=count-1-t;
    }
    // One GPU-native correspondence deliberately has no accepted primitive.
    geometry[count*24+count-1-12]=UINT32_MAX;Buffer positions(gpu,geometry);
    shadows::RayReflectionHistory previous{count*48,{w,h},{256,272,8,4},1,65536,count*96,true};
    previous.model_lobes=lobes;previous.model_paths=true;previous.curved_paths=previous.curved_receivers=curved;
    if(curved){previous.previous_liquid=shadows::RayReflectionLiquid{{{0,180,0},{0,-1,0}}};previous.previous_liquid->material=material;}
    CalibratedReflectionLobeGeometry guide{positions.value,unsigned(geometry.size()*4),count*3,previous,lobes==8?.5F:0.F};
    if(curved)guide.current_liquid=previous.previous_liquid;
    if(curved || optical){guide.current_projection=previous.projection;guide.current_clip={1,65536};}
    const auto water=curved?*shadows::native_water_layers(w,h,true):shadows::NativeWaterLayers{};
    OwnerTexture ink(gpu,w,h,curved);GpuCalibratedReflectionHistory owner;check(owner.initialize(gpu.device),owner.status().c_str());
    const auto stage=[&](const Words& authored,unsigned epoch,bool indexed=true) {
        Buffer source(gpu,authored);const shadows::GpuReflectionOutput rays{gpu.device,source.value,w,h,w*4,water,layout};
        auto* cb=SDL_AcquireGPUCommandBuffer(gpu.device);check(cb,SDL_GetError());
        check(owner.enqueue(cb,rays,ink.value,{epoch,0,false,curved,indexed},&guide),owner.status().c_str());
        return cb;
    };
    auto* cold=stage(old_authored,42);check(!owner.enqueue_source_queries(cold,0,1) && !owner.source_queries().queries,
        "Cold capture produced accepted-source queries");
    check(!owner.enqueue_source_optical(cold) && !owner.source_optical().frames,
        "Cold capture produced usable native optics");gpu.submit(cold);owner.commit();
    const auto accepted=owner.accepted_source_index();const auto accepted_bank=gpu.read(accepted.source_buffer,layout.storage_bytes);
    const auto accepted_nodes=gpu.read(accepted.buffer,accepted.layout.storage_bytes);const auto resident=owner.working_image_bytes();
    guide.current_cube={0,-1,0,1,0,0,0,0,1};
    if(optical && lobes==8)guide.roughness=.85F; // Old bank still used .5.
    Words reference_queries,reference_frames;
    for(unsigned round=0;round<3;++round) {
        const auto round_authored=query_fixture(w,h,lobes,curved,material,true,round==2,round*256);
        auto* cb=stage(round_authored,42);
        check(!owner.enqueue_source_optical(cb) && !owner.source_optical().frames,
            "Native optics without a pending query borrowed another batch");
        const auto preflight=owner.source_query_allocation_bytes();
        const auto query_bytes=resident+GpuReflectionSourceQueries::working_bytes;
        const auto complete_bytes=query_bytes+(optical?GpuReflectionSourceOptical::working_bytes:0);
        check(preflight==query_bytes+(optical && round?GpuReflectionSourceOptical::working_bytes:0),"Query scratch preflight changed across frames");
        if(optical)check(owner.source_optical_allocation_bytes()==complete_bytes,"Optical preflight omitted retained/query scratch");
        QueryCapture capture(gpu,total+(optical?5U:4U),optical);
        for(unsigned first=0;first<total;first+=64) {
            const unsigned n=std::min(64U,total-first);
            check(owner.enqueue_source_queries(cb,first,n),owner.status().c_str());const auto queries=owner.source_queries();
            check(!owner.source_optical().frames,"A new query retained the preceding optical descriptor");
            check(queries.first==first && queries.count==n && queries.current_buffer==owner.output().buffer
                && queries.source.buffer==accepted.buffer && queries.source.source_buffer==accepted.source_buffer,
                "Query borrowed a wrong current/accepted native bank");
            check(owner.working_image_bytes()==query_bytes+(optical && (round || first)?GpuReflectionSourceOptical::working_bytes:0),
                "Query workspace was allocated per batch/lobe/frame");
            if(optical) {
                check(owner.enqueue_source_optical(cb),owner.status().c_str());const auto optics=owner.source_optical();
                check(optics.queries.queries==queries.queries && optics.queries.source.buffer==accepted.buffer
                    && owner.working_image_bytes()==complete_bytes && owner.source_optical_allocation_bytes()==complete_bytes,
                    "Optical workspace/source pairing changed per batch/lobe/frame");capture.record(cb,queries,&optics);
            } else {
                if(!curved)check(!owner.enqueue_source_optical(cb) && !owner.source_optical().frames,
                    "Absent optional planar eye calibration was silently guessed");
                capture.record(cb,queries);
            }
        }
        for(unsigned fault=0;fault<4;++fault) {
            ReflectionSourceQueryLimits limits;
            if(fault==0)limits.leaves=1;
            if(fault==1)limits.nodes=1;
            if(fault==2)limits.regions=1;
            if(fault==3)limits.supports=1;
            check(owner.enqueue_source_queries(cb,7*lobes,1,limits),owner.status().c_str());
            if(optical){check(owner.enqueue_source_optical(cb),owner.status().c_str());const auto optics=owner.source_optical();capture.record(cb,owner.source_queries(),&optics);}
            else capture.record(cb,owner.source_queries());
        }
        if(optical) {
            check(owner.enqueue_source_queries(cb,7*lobes,1),owner.status().c_str());
            check(owner.enqueue_source_optical(cb,{1,12}),owner.status().c_str());const auto optics=owner.source_optical();
            capture.record(cb,owner.source_queries(),&optics);
            for(unsigned fault=0;fault<5;++fault) {
                const ReflectionSourceOpticalLimits limits{fault==0?0U:fault==1?8193U:8192U,fault==2?13U:12U};
                void* command=fault==3?nullptr:fault==4?static_cast<void*>(&owner):cb;
                check(!owner.enqueue_source_optical(command,limits) && !owner.source_optical().frames,
                    "Invalid optical quota/command retained usable scratch");
            }
        }
        for(unsigned fault=0;fault<8;++fault) {
            const std::uint64_t first=fault==0?total:fault==1?UINT64_MAX:0;
            const unsigned n=fault==2?0:fault==3?65:1;
            ReflectionSourceQueryLimits limits;if(fault==4)limits.nodes=4097;if(fault==5)limits.regions=129;
            void* command=fault==6?nullptr:fault==7?static_cast<void*>(&owner):cb;
            check(!owner.enqueue_source_queries(command,first,n,limits) && !owner.source_queries().queries,
                "Invalid/wrong-command query exposed stale scratch");
        }
        gpu.submit(cb);const auto words=capture.read();
        if(round==0)reference_queries=Words(words.begin(),words.begin()+total*12);
        if(round==1)check(std::equal(reference_queries.begin(),reference_queries.end(),words.begin()),"RGB altered native mapped queries");
        if(optical && round==0)reference_frames=Words(words.begin()+capture.capacity*1620,words.begin()+capture.capacity*1620+total*128);
        if(optical && round==1)check(std::equal(reference_frames.begin(),reference_frames.end(),words.begin()+capture.capacity*1620),"RGB altered native optical frame construction");
        for(unsigned sample=0;sample<total;++sample) {
            const unsigned p=sample/lobes,l=sample%lobes,q=sample*12,leaf=capture.capacity*12+sample*68;
            const unsigned domain=capture.capacity*80+sample*1540;
            const bool valid=p%w!=0 && !(curved && p==9) && p!=12
                && !(round==2 && (p==1 || p==2 || p==3 || p==5 || p==6 || p==10 || p==11 || p==13));
            check((words[q]!=UINT32_MAX)==valid,"Current GPU primitive/protected/malformed query eligibility changed");
            if(!valid){check(words[leaf+1]==4 && words[domain+1]==4,"Invalid mapping retained usable supports");
                if(optical)check(!words[capture.capacity*1620+sample*128+124],"Invalid path published a valid optical frame");
                continue;}
            const unsigned at=layout.incoming_offset/4+sample*(curved?16:13);
            check(words[q]==old_authored[layout.identity_offset/4+p] && words[q+11]==l && words[q+10]==0,
                "GPU primitive reorder or native lobe identity was lost");
            for(unsigned c=0;c<6;++c)check(words[q+1+c]==old_authored[at+c],"Ordered finite/liquid path correspondence changed");
            for(unsigned c=0;c<3;++c)check(std::abs(std::bit_cast<float>(words[q+7+c])-std::bit_cast<float>(old_authored[at+6+c]))<1.e-7F,
                "Current sky direction was not transformed into the accepted cube");
            check(!words[leaf+1] && words[leaf]>0 && !words[domain+1] && words[domain]>0,
                "Valid mapped path lost its necessary accepted supports");
            bool contained=false;
            for(unsigned n=0;n<words[domain];++n) {
                const unsigned region=domain+4+n*12;const float x=float(p%w),y=float(p/w);
                contained|=std::bit_cast<float>(words[region+4])<=x && std::bit_cast<float>(words[region+5])<=y
                    && std::bit_cast<float>(words[region+6])>=x && std::bit_cast<float>(words[region+7])>=y;
            }
            check(contained,"Known necessary native sample omitted by the resident support cover");
            if(optical) {
                const unsigned frame=capture.capacity*1620+sample*128,results=capture.capacity*1748+sample*1024;
                check(words[frame+124]==1 && !words[frame+125] && !words[frame+126] && !words[frame+127],"Valid query lost its native optical frame");
                check(std::bit_cast<float>(words[frame+20])==(lobes==8?.5F:0.F) && std::bit_cast<float>(words[frame+21])==float(l),
                    "Native optics used CURRENT roughness or another lobe instead of accepted metadata");
                if(words[q]!=0xfffffffdU)for(unsigned c=0;c<12;++c)
                    check(words[frame+c]==geometry[count*12+words[q]*12+c],"Native receiver read current or reordered geometry instead of accepted vertices");
                for(unsigned c=0;c<3;++c)check(words[frame+104+c]==round_authored[at+6+c],"Optical target was rounded/remapped instead of retaining raw terminal features");
                bool physical_retained=false;
                for(unsigned n=0;n<words[domain];++n) {
                    const unsigned output=results+n*8,region=domain+4+n*12;
                    check(!words[output+1] && words[output]+words[output+3]==words[output+2] && words[output+2]>0,
                        "Optical continuum coverage was partial, invalid or dropped accounting");
                    if(p==22 && words[output]) {
                        const float x=5.5F,y=1.5F;
                        physical_retained|=std::bit_cast<float>(words[output+4])<=x && std::bit_cast<float>(words[output+5])<=y
                            && std::bit_cast<float>(words[output+6])>=x && std::bit_cast<float>(words[output+7])>=y;
                    }
                    if(curved)for(unsigned c=0;c<32;++c)check(words[frame+72+c]==std::bit_cast<unsigned>(shadows::reflection_liquid_frame_words(previous)[c]),"Native optical frame changed accepted liquid bytes");
                    check(words[region+4]<=words[region+6],"Malformed support bounds");
                }
                if(p==22)check(physical_retained,"Native optical exclusion removed a known accepted physical path");
            }
        }
        for(unsigned fault=0;fault<4;++fault) {
            const unsigned domain=capture.capacity*80+(total+fault)*1540,flags[]{1,2,16,32};
            check((words[domain+1]&flags[fault])!=0,"Partial query/support quota failed to invalidate the whole result");
            if(optical)for(unsigned n=0;n<128;++n)check(words[capture.capacity*1748+(total+fault)*1024+n*8+1]&flags[fault],
                "Upstream query/support refusal was dropped by optical consumption");
        }
        if(optical) {
            const unsigned domain=capture.capacity*80+(total+4)*1540,results=capture.capacity*1748+(total+4)*1024;
            bool refused=false;for(unsigned n=0;n<words[domain];++n)refused|=(words[results+n*8+1]&32U)!=0;
            check(refused,"Incomplete optical box budget was treated as a complete cover");
        }
        check(gpu.read(accepted.source_buffer,layout.storage_bytes)==accepted_bank
            && gpu.read(accepted.buffer,accepted.layout.storage_bytes)==accepted_nodes,"Queries mutated accepted native colour/index");
        owner.discard();check(!owner.source_queries().queries && !owner.source_optical().frames && owner.accepted_source_index().buffer==accepted.buffer,
            "Discard retained a query or detached the accepted bank");
    }
    if(optical) {
        // The public guide bounds can be valid while individual GPU old faces
        // are malformed, or the accepted bank has more primitives than the
        // current producer's old-vertex span. Refuse on GPU, never read the
        // mapping payload as vertices or allow a partial frame to be consumed.
        for(unsigned fault=0;fault<6;++fault) {
            Words broken=geometry;unsigned p=7;
            if(fault==0)broken[count*12+7*12+3]=0; // Receiver homogeneous W.
            if(fault==1)broken[count*12+7*12]=0x7fc01234U;
            if(fault==2)for(unsigned v=1;v<3;++v)for(unsigned c=0;c<4;++c)
                broken[count*12+7*12+v*4+c]=broken[count*12+7*12+c];
            if(fault==3)broken[count*12+17*12+11]=0; // Finite terminal W.
            if(fault==4){p=26;broken[count*12+3*12+7]=0;} // Finite hop W.
            auto authored=query_fixture(w,h,lobes,curved,material,true,false,0);
            const auto original_guide=guide;
            if(fault==5) {
                p=22;guide.vertex_count=3;guide.history.previous_index_offset=count*48+48;
                broken[count*12+12]=22;authored[layout.identity_offset/4+p]=0;
            }
            Buffer malformed_positions(gpu,broken);guide.buffer=malformed_positions.value;
            check(guide.valid(),"GPU-malformed fixture accidentally failed host guide preflight");
            auto* command=stage(authored,42);QueryCapture capture(gpu,1,true);
            check(owner.enqueue_source_queries(command,p*lobes,1),owner.status().c_str());
            check(owner.enqueue_source_optical(command),owner.status().c_str());const auto optics=owner.source_optical();
            capture.record(command,owner.source_queries(),&optics);gpu.submit(command);const auto words=capture.read();
            check(words[0]!=UINT32_MAX && !words[81] && words[80]>0,
                "Malformed old-face fixture did not reach valid necessary supports");
            check(!words[1620+124],"Malformed/out-of-span old geometry published a valid optical frame");
            for(unsigned n=0;n<words[80];++n)check(words[1748+n*8+1]==4,
                "Malformed old geometry was not refused by every supported optical region");
            owner.discard();guide=original_guide;
            check(!owner.source_optical().frames,"Malformed-frame discard retained borrowed optics");
        }
        if(!curved)for(unsigned fault=0;fault<2;++fault) {
            const auto original_guide=guide;
            if(fault==0)guide.history.projection[0]+=1;else guide.history.near_plane+=1;
            auto* command=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42);
            check(owner.enqueue_source_queries(command,22*lobes,1),owner.status().c_str());
            check(!owner.enqueue_source_optical(command) && !owner.source_optical().frames,
                "Mismatched accepted planar projection/clip enabled optical exclusions");
            gpu.submit(command);owner.discard();guide=original_guide;
        }
    }
    auto* cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),43);
    check(!owner.enqueue_source_queries(cut,0,1) && !owner.enqueue_source_optical(cut),
        "Epoch cut reused stale accepted-source queries/optics");gpu.submit(cut);owner.discard();
    guide.previous_cube=guide.current_cube;
    cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42);
    check(!owner.enqueue_source_queries(cut,0,1),"Mismatched accepted cube reused source queries");gpu.submit(cut);owner.discard();
    guide.previous_cube={1,0,0,0,1,0,0,0,1};
    if(curved) {
        guide.history.previous_liquid->time+=.25;
        cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42);
        check(!owner.enqueue_source_queries(cut,0,1),"Mismatched accepted liquid phase reused source queries");gpu.submit(cut);owner.discard();
        guide.history.previous_liquid->time-=.25;
        guide.history.projection[0]+=1;
        cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42);
        check(!owner.enqueue_source_queries(cut,0,1),"Mismatched accepted liquid projection reused source queries");gpu.submit(cut);owner.discard();
        guide.history.projection[0]-=1;
    }
    cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42);
    check(owner.enqueue_source_queries(cut,20*lobes,1),owner.status().c_str());const auto published=owner.source_index();
    if(optical)check(owner.enqueue_source_optical(cut),owner.status().c_str());
    gpu.submit(cut);owner.commit();
    check(!owner.source_queries().queries && !owner.source_optical().frames && !owner.enqueue_source_optical(cut)
        && owner.accepted_source_index().buffer==published.buffer
        && owner.accepted_source_index().source_buffer==published.source_buffer,"Commit detached query source or retained borrowed scratch descriptor");
    guide.previous_cube=guide.current_cube;
    cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42,false);
    check(!owner.enqueue_source_queries(cut,0,1) && !owner.enqueue_source_optical(cut),
        "Disabled source staging exposed query/optical scratch");gpu.submit(cut);owner.commit();
    check(owner.working_image_bytes()==std::uint64_t(layout.storage_bytes)*2 && !owner.accepted_source_index().buffer,
        "Disabling staging retained source indices or query scratch");
    cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42);
    check(!owner.enqueue_source_queries(cut,0,1),"Re-enabled cold index borrowed absent accepted sources");gpu.submit(cut);owner.commit();
    cut=stage(query_fixture(w,h,lobes,curved,material,true,false,0),42);owner.reset();
    check(!owner.enqueue_source_queries(cut,0,1) && !owner.source_queries().queries
        && !owner.enqueue_source_optical(cut) && !owner.source_optical().frames,"Pending reset retained accepted-source queries/optics");
    gpu.submit(cut);owner.commit();check(!owner.accepted_source_index().buffer,"Cut query bank was committed");
    owner.release_device();check(!owner.working_image_bytes() && !owner.source_queries().queries && !owner.source_optical().frames
        && !owner.enqueue_source_optical(nullptr),"Released owner retained query/optical scratch");
    std::cout<<"GPU-native source queries lobes="<<lobes<<" curved="<<curved<<" liquid="<<material
        <<": all "<<total<<" native samples streamed; reordered/unmatched IDs, 0-4 ordered hops, cube rotation, protected/malformed inputs, RGB-independent queries, quotas/metadata/commit/cut/discard/disable and bounded scratch passed.\n";
    if(optical)std::cout<<"Native GPU optical frames/continuum consumption: all "<<total
        <<" samples, accepted (not current) vertices/roughness, raw targets, physical path retained, exact liquid bytes, malformed/out-of-span geometry, calibration/lifetime and upstream/box-budget refusal, RGB-independent frames passed. No root uniqueness or colour acceptance.\n";
}
