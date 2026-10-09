// Replay only bounded raw INPUT geometry/eye calibration/path/visibility.
// Independent forward searches are assertions after the native owner executes;
// neither their starts/roots, source boxes nor downloaded RGB enter the GPU.
void exercise_root_hull_replay(Gpu& gpu,const char* filename,bool witness=false,bool folds=false,bool guides=false,bool colours=false,bool srgb=false,bool compose=false) {
    namespace o=reflected_curved_owner_oracle;
    const auto replay=load_full_curved_owner_replay(filename);const auto& input=replay.inputs;
    const auto& liquid=input.liquid;const unsigned w=replay.width,h=replay.height,count=w*h;
    check(replay.roughness==0 && replay.lobe==0 && replay.primary==0xfffffffdU
        && replay.x<w && replay.y<h && input.current.size()==input.previous.size()
        && !input.previous.empty(),"Ambiguous native replay requires its complete sharp liquid-primary input");
    const unsigned triangles=unsigned(input.previous.size()),pixel=replay.y*w+replay.x;
    Words geometry(triangles*25,0);
    for(unsigned n=0;n<triangles;++n)for(unsigned old=0;old<2;++old) {
        const auto& face=old?input.previous[n]:input.current[n];const std::array<o::V,3> vertices{face.a,face.b,face.c};
        for(unsigned v=0;v<3;++v) {
            const unsigned at=(old*triangles+n)*12+v*4;
            for(unsigned c=0;c<3;++c)geometry[at+c]=std::bit_cast<unsigned>(float(vertices[v][c]));
            geometry[at+3]=std::bit_cast<unsigned>(1.F);
        }
    }
    for(unsigned n=0;n<triangles;++n)geometry[triangles*24+n]=input.mapping[n];
    Buffer positions(gpu,geometry);const auto layout=native_layout(w,h,1,true);
    Words old_input(layout.storage_bytes/4,0),owners(count,0);
    for(unsigned p=0;p<count;++p) {
        const auto& tap=replay.taps[p];const bool wet=tap.identity==0xfffffffdU;
        old_input[p]=(wet?(liquid.material==3?0xfe000000U:0xfd000000U):0xff000000U)|0x336699U;
        old_input[layout.identity_offset/4+p]=tap.identity;
        old_input[layout.witness_offset/4+p]=std::bit_cast<unsigned>(float(tap.depth));
        old_input[layout.base_offset/4+p*4+3]=std::bit_cast<unsigned>(1.F);
        for(unsigned c=0;c<4;++c)old_input[layout.weight_offset/4+p*4+c]=std::bit_cast<unsigned>(c==3?1.F:.5F);
        const unsigned at=layout.incoming_offset/4+p*16;std::copy(tap.path.begin(),tap.path.end(),old_input.begin()+at);
        old_input[at+9]=0xff336699U;for(unsigned c=10;c<13;++c)old_input[at+c]=std::bit_cast<unsigned>(.5F);
        if(tap.identity!=UINT32_MAX)owners[p]=wet?0xff010001U:0xff0280ffU;
    }
    auto current_input=old_input;current_input[layout.identity_offset/4+pixel]=replay.primary;
    current_input[layout.witness_offset/4+pixel]=std::bit_cast<unsigned>(float(replay.current_depth));
    std::copy(replay.record.begin(),replay.record.end(),current_input.begin()+layout.incoming_offset/4+pixel*16);
    owners[pixel]=0xff010001U;OwnerTexture ink(gpu,w,h,false,false,&owners);
    shadows::RayReflectionHistory history{triangles*48,{w,h},liquid.projection,liquid.near,liquid.far,triangles*96,true};
    history.model_lobes=1;history.model_paths=history.curved_paths=history.curved_receivers=true;
    shadows::RayReflectionLiquid old_liquid;old_liquid.ground={liquid.point,liquid.normal};
    old_liquid.world_to_view=liquid.rotation;old_liquid.offset=liquid.offset;old_liquid.time=liquid.time;old_liquid.material=liquid.material;
    history.previous_liquid=old_liquid;
    CalibratedReflectionLobeGeometry guide{positions.value,unsigned(geometry.size()*4),triangles*3,history,0};
    guide.current_liquid=old_liquid;guide.current_projection=liquid.projection;guide.current_clip={liquid.near,liquid.far};
    constexpr double sign[3]{1,-1,-1};
    for(unsigned row=0;row<3;++row)for(unsigned c=0;c<3;++c)
        guide.current_cube[row*3+c]=guide.previous_cube[row*3+c]=float(input.previous_eye_view[row*4+c]*sign[c]);
    GpuCalibratedReflectionHistory owner;check(owner.initialize(gpu.device),owner.status().c_str());
    const auto water=*shadows::native_water_layers(w,h,true);
    const auto stage=[&](const Words& words) {
        Buffer source(gpu,words);const shadows::GpuReflectionOutput rays{gpu.device,source.value,w,h,w*4,water,layout};
        auto* cb=SDL_AcquireGPUCommandBuffer(gpu.device);check(cb,SDL_GetError());
        check(owner.enqueue(cb,rays,ink.value,{80,colours?.85F:0.F,srgb,true,true},&guide),owner.status().c_str());return cb;
    };
    auto* cb=stage(old_input);gpu.submit(cb);owner.commit();
    const auto accepted=owner.accepted_source_index();const auto bank=gpu.read(accepted.source_buffer,layout.storage_bytes);
    // Recover exactly the accepted visibility fields for independent assertions,
    // never re-upload them or use their incoming colour as a choice/seed.
    const auto read=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
        const unsigned p=y*w+x,at=layout.incoming_offset/4+p*16;o::SourceTap tap;
        tap.identity=bank[layout.identity_offset/4+p];tap.depth=std::bit_cast<float>(bank[layout.witness_offset/4+p]);
        std::copy_n(bank.begin()+at,9,tap.path.begin());return tap;
    };
    o::Path path;path.liquid_primary=true;path.hops=replay.record[4]&7U;path.liquid_mask=(replay.record[4]>>4)&15U;
    path.finite_terminal=(replay.record[4]>>8)==1;
    for(unsigned n=0;n<path.hops;++n)if(!(path.liquid_mask&(1U<<n)))path.mirrors[n]=input.previous.at(input.mapping.at(replay.record[n]));
    for(unsigned c=0;c<3;++c)path.terminal[c]=std::bit_cast<float>(replay.record[6+c]);
    if(path.finite_terminal) {
        path.terminal_plane=input.previous.at(input.mapping.at(replay.record[5]));
        path.terminal=o::point(path.terminal_plane,{path.terminal[0],path.terminal[1]});
    }else path.terminal=o::old_environment_direction(input,path.terminal);
    const auto domains=o::old_visibility_domains(input,replay.primary,replay.record,read);
    std::vector<o::Solution> roots;
    for(const auto& domain:domains) {
        const auto solved=o::solve_forward(input,path,domain.initializer[0],domain.initializer[1]);if(!solved.valid)continue;
        const o::Solution root{double(float(solved.x))-.5,double(float(solved.y))-.5,double(float(solved.depth)),solved.error,true,false};
        if(o::source_guard(input,replay.primary,replay.record,0,0,root,read)!=o::SourceGuard::accepted)continue;
        // Deduplication is ONLY a diagnostic count, never the GPU certificate.
        if(std::none_of(roots.begin(),roots.end(),[&](const auto& other){return std::abs(root.x-other.x)<=1./4096
            && std::abs(root.y-other.y)<=1./4096;}))roots.push_back(root);
    }
    check(roots.size()>=2,"Ambiguous raw replay no longer reproduces two independently qualified branches");
    for(unsigned row=0;row<3;++row)for(unsigned c=0;c<3;++c)
        guide.current_cube[row*3+c]=float(input.current_eye_view[row*4+c]*sign[c]);
    guide.current_projection=input.current_projection;
    // The exact recorded OLD liquid frame is the forward proof's input.
    // The log records a CURRENT transform/depth but not a separate CURRENT
    // clock: the same-clock diagnostic capture calibration below is NOT a
    // claim of complete producer/current-colour replay. It cannot change the
    // OLD forward proof or the raw target and is never an inverse/old root.
    check(input.current_liquid_transform.has_value(),"Ambiguous native replay lacks raw CURRENT liquid calibration");
    auto now_liquid=old_liquid;const auto& transform=*input.current_liquid_transform;
    for(unsigned row=0;row<3;++row) {
        now_liquid.offset[row]=transform[row*4+3];
        for(unsigned c=0;c<3;++c)now_liquid.world_to_view[row*3+c]=transform[row*4+c];
    }
    // The captured old and current normals are horizontal in this saved raw
    // replay. Do not invent a differently oriented current receiver.
    check(liquid.normal==o::V{0,-1,0} && transform[1]==0 && transform[4]==0 && transform[6]==0 && transform[9]==0
        && transform[5]==1,"Ambiguous fixture's plane transport requires unsupported nonhorizontal raw calibration");
    now_liquid.ground.point[1]=liquid.point[1]+liquid.offset[1]-now_liquid.offset[1];
    guide.current_liquid=now_liquid;
    cb=stage(current_input);check(owner.enqueue_source_queries(cb,pixel,1) && owner.enqueue_source_optical(cb)
        && owner.enqueue_source_root_hull(cb),owner.status().c_str());
    const auto hull=owner.source_root_hull();const GpuReflectionSourceLocalRoots copy_view{hull.optical,hull.results};
    check(hull.results && !owner.source_local_roots().results,"Ambiguous hull aliased a local descriptor");
    QueryCapture capture(gpu,1,true,true);const auto query=owner.source_queries();const auto optical=owner.source_optical();
    capture.record(cb,query,&optical,&copy_view);gpu.submit(cb);const auto result=capture.read();
    check(result[80]>0 && !result[81] && result[1620+124]==1,"Ambiguous raw replay did not reach a complete native optical frame");
    for(const auto& root:roots) {
        bool kept=false;
        for(unsigned region=0;region<result[80];++region) {
            const unsigned at=1748+region*8;
            check(!result[at+1] && result[at+2]>0 && result[at]+result[at+3]==result[at+2],"Ambiguous replay has partial optical coverage");
            if(result[at])kept|=std::bit_cast<float>(result[at+4])<=root.x+.5 && std::bit_cast<float>(result[at+5])<=root.y+.5
                && std::bit_cast<float>(result[at+6])>=root.x+.5 && std::bit_cast<float>(result[at+7])>=root.y+.5;
        }
        check(kept,"Native necessary/optical cover discarded an independently qualified ambiguous branch");
    }
    check(!result[2772] || result[2773]==0,"Whole-source hull falsely accepted uniqueness/no-root for two qualified branches");
    for(unsigned region=1;region<128;++region)for(unsigned word=0;word<48;++word)
        check(!result[2772+region*48+word],"Whole-source replay left a stale local-region certificate");
    check(gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,"Ambiguous hull modified accepted incident RGB/path fields");
    if(witness) {
        owner.discard();cb=stage(current_input);
        check(owner.enqueue_source_queries(cb,pixel,1) && owner.enqueue_source_optical(cb)
            && owner.enqueue_source_root_hull(cb) && owner.enqueue_source_witness(cb),owner.status().c_str());
        const auto source=owner.source_witness();const GpuReflectionSourceLocalRoots copy{source.hull.optical,source.results};
        if(folds)check(owner.enqueue_source_folds(cb),owner.status().c_str());
        if(guides)check(owner.enqueue_source_guide(cb),owner.status().c_str());
        if(colours)check(owner.enqueue_source_colour(cb),owner.status().c_str());
        if(compose)check(owner.enqueue_source_composition(cb),owner.status().c_str());
        QueryCapture next(gpu,1,true,true);next.record(cb,owner.source_queries(),&source.hull.optical,&copy);gpu.submit(cb);
        const auto words=next.read();check(!words[2820] && words[2822]==1,
            "Ambiguous two-root replay bypassed native source witness's whole-hull requirement");
        if(folds)check(!words[2836] && words[2837]==1,"Ambiguous two-root replay bypassed native fold guard's upstream requirement");
        if(guides)check(!words[2852] && words[2853]==1,"Ambiguous two-root replay bypassed native guide uniqueness requirement");
        if(colours)check(!words[2868] && words[2869]==1,"Ambiguous two-root replay acquired old RGB permission");
        if(compose) {
            const unsigned at=2900,incoming=layout.incoming_offset/4+pixel*16+9;
            check(!words[at] && words[at+1]==1 && words[at+2]==pixel && !words[at+3]
                && words[at+4]==current_input[pixel] && words[at+5]==current_input[incoming],
                "Ambiguous two-root replay changed complete CURRENT pixel/incident light");
            for(unsigned n=6;n<16;++n)check(!words[at+n],"Ambiguous composition retained a partial packet");
        }
        check(gpu.read(accepted.source_buffer,layout.storage_bytes)==bank,"Ambiguous witness modified accepted RGB/path input");
    }
    owner.discard();check(!owner.source_root_hull().results && !owner.source_local_roots().results
        && !owner.source_witness().results && !owner.source_folds().results && !owner.source_guide().results && !owner.source_colour().results
        && !owner.source_composition().results,
        "Ambiguous discard retained a certificate");
    owner.release_device();check(!owner.working_image_bytes(),"Ambiguous replay retained resident scratch");
    std::cout<<"Native whole-source ambiguous replay: "<<roots.size()<<" independently qualified raw-input branches retained; "
        <<"unique/no-root acceptance refused, accepted bank unchanged. Numerical search checks the implementation, not global completeness.\n";
}
