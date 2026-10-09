// Production RT path witnesses, checked by independent binary64 forward
// intersection/rough-lobe/per-hop Fresnel equations. No CPU inverse guide and
// no geometry readback is used to construct the expected optical path.
{
    struct Record {std::array<unsigned,4> mirrors;unsigned control,terminal;
        std::array<float,3> feature;unsigned incoming;std::array<float,3> response;};
    static_assert(sizeof(Record)==52);
    constexpr unsigned pw=96,ph=72;
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},
        {.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    std::array<unsigned,5> hop_counts{};unsigned finite_count=0,escape_count=0,budget_count=0,prefixes=0;
    std::uint64_t samples=0,edges=0;double max_direction=0,max_response=0;
    for(unsigned variant=0;variant<3;++variant) for(float rough:{0.F,.12F,.35F,1.F})
    for(unsigned metal=0;metal<=3;++metal) for(bool transport:{false,true}) {
        const unsigned lobes=rough>0?8:1;
        std::array vertices{quad(0,0,-2,1.7F,{.3F,.3F,.3F,1}),
            quad(variant?1.15F:.94F,.03F,-.6F,variant?.82F:.23F,{.8F,.1F,.1F,1})};
        std::array<render::CalibratedSceneDraw,2> draws;
        std::array<render::CalibratedSceneMotionDraw,2> matches;
        for(unsigned i=0;i<2;++i) {
            draws[i].vertices=vertices[i];draws[i].ray_caster=true;
            matches[i].previous_model=identity;matches[i].previous_vertices=vertices[i];
            matches[i].valid=true;matches[i].previous_ray_first_triangle=2*i;
        }
        std::array<XrView,2> eyes{};
        for(unsigned eye=0;eye<2;++eye) {
            auto& e=eyes[eye];e.type=XR_TYPE_VIEW;e.pose.orientation.w=1;
            e.pose.position={eye?.034F:-.032F,.015F,.02F};e.fov={-.57F,.73F,.61F,-.49F};
        }
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        std::array<render::CalibratedRayGeometryOutput,2> rays;
        for(unsigned eye=0;eye<2;++eye) {
            const auto camera=camera_for(eyes[eye],d3d);const render::CalibratedSceneMotion previous{camera,matches,pw,ph};
            rays[eye]=scene.enqueue_ray_geometry(gpu.command,eye,pw,ph,camera,256,variant==2?8:0,
                {.125F,.25F,.5F,1},&previous);
            check(rays[eye].complete,scene.status().c_str());
        }
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Ordered path source submission lost ownership");
        for(unsigned eye=0;eye<2;++eye) {
            const auto& g=rays[eye];const render::GpuScene::RayGeometryOutput resident{
                g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
            RayReflectionHistory previous{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
            previous.model_lobes=lobes;previous.model_paths=true;
            const Camera camera{pw,ph,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
            const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
            const std::array<std::uint32_t,256> palette{};
            check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,0xff306090,rough,metal,{},g.environment_face_size,
                g.environment_rotation,nullptr,{},0,nullptr,false,range,g.environment_offset,1,transport,&previous),hardware[eye].status().c_str());
            const auto output=hardware[eye].reflection_output();
            check(output.reflection_history==*native_reflection_history(pw,ph,{pw,ph},true,{},lobes,true),
                "Native ordered path layout mismatch");
            const auto size=output.reflection_history.storage_bytes;
            const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,size,0};
            auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
            auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
            auto* pass=SDL_BeginGPUCopyPass(command);check(pass,SDL_GetError());
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,size};const SDL_GPUTransferBufferLocation to{transfer,0};
            SDL_DownloadFromGPUBuffer(pass,&from,&to);SDL_EndGPUCopyPass(pass);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
            check(SDL_WaitForGPUFences(gpu.device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(gpu.device,fence);
            const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(bytes,SDL_GetError());
            const unsigned count=pw*ph;const auto* ids=reinterpret_cast<const unsigned*>(bytes+count*4);
            const auto* depths=reinterpret_cast<const float*>(bytes+count*8);
            const auto* records=reinterpret_cast<const Record*>(bytes+count*28);
            std::vector<unsigned char> prefix(bytes,bytes+count*4);
            std::array<V,12> positions;
            for(unsigned draw=0;draw<2;++draw) for(unsigned v=0;v<6;++v) {
                const auto p=world_eye(eyes[eye],model_point(draws[draw].model,vertices[draw][v]));
                positions[draw*6+v]={p[0]*256,-p[1]*256,-p[2]*256};
            }
            const auto f0=[&](unsigned primitive) {
                return metal==2?V{1,.766,.336}:metal==3?V{.955,.638,.538}:
                    primitive<2?V{77./255,77./255,77./255}:V{204./255,26./255,26./255};
            };
            for(unsigned y=0;y<ph;++y) for(unsigned x=0;x<pw;++x) {
                const unsigned index=y*pw+x;
                const V direction=normal({(x+.5-g.projection[2])/g.projection[0],(y+.5-g.projection[3])/g.projection[1],1});
                const auto primary=trace(positions,{},direction,g.near_plane/direction[2],g.far_plane.value_or(65536)/direction[2]);
                if(primary.edge) {edges+=lobes;continue;}
                check(ids[index]==(primary.found?primary.primitive:UINT32_MAX),"Ordered path primary identity mismatch");
                if(!primary.found) {
                    for(unsigned lobe=0;lobe<lobes;++lobe) {
                        const auto& r=records[index*lobes+lobe];
                        check(r.mirrors==std::array<unsigned,4>{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}
                            && !r.control && r.terminal==UINT32_MAX && r.feature==std::array<float,3>{}
                            && !r.incoming && r.response==std::array<float,3>{},"No receiver retained ordered path data");
                    }
                    continue;
                }
                check(std::abs(depths[index]-primary.t*direction[2])<.002,"Ordered path lost current primary depth");
                const unsigned p=primary.primitive*3;
                V n=normal(cross(sub(positions[p+1],positions[p]),sub(positions[p+2],positions[p])));
                if(dot(n,direction)>0) n=scale(n,-1);
                const V reflected=sub(direction,scale(n,2*dot(n,direction)));
                const V tangent=normal(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
                const double bias=std::max(.01,primary.t*1.e-5);
                const V first_origin=add(scale(direction,primary.t),scale(n,bias));
                for(unsigned lobe=0;lobe<lobes;++lobe) {
                    V outgoing=normal(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),double(rough)*rough)));
                    if(dot(outgoing,n)<=0) outgoing=reflected;
                    V origin=first_origin,throughput{1,1,1};double near=bias;
                    std::array<unsigned,4> mirrors{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
                    unsigned kind=3,hops=0,terminal=UINT32_MAX;V feature{};bool boundary=false;
                    for(unsigned bounce=0;bounce<4;++bounce) {
                        const auto hit=trace(positions,origin,outgoing,near,65536);
                        if(hit.edge) {boundary=true;break;}
                        if(!hit.found) {kind=2;feature=outgoing;break;}
                        if(!transport) {kind=1;terminal=hit.primitive;feature={hit.u,hit.v,0};break;}
                        mirrors[bounce]=hit.primitive;++hops;
                        const unsigned h=hit.primitive*3;
                        V hn=normal(cross(sub(positions[h+1],positions[h]),sub(positions[h+2],positions[h])));
                        if(dot(hn,outgoing)>0) hn=scale(hn,-1);
                        if(metal) {
                            const auto tint=f0(hit.primitive);const double grazing=std::pow(1-std::clamp(dot(scale(outgoing,-1),hn),0.,1.),5);
                            for(unsigned c=0;c<3;++c) throughput[c]*=tint[c]+(1-tint[c])*grazing;
                        }
                        near=std::max(.05,hit.t*1.e-5);
                        origin=add(add(origin,scale(outgoing,hit.t)),scale(hn,near));
                        outgoing=sub(outgoing,scale(hn,2*dot(hn,outgoing)));feature=outgoing;
                    }
                    if(boundary) {++edges;continue;}
                    const auto& actual=records[index*lobes+lobe];
                    check(actual.mirrors==mirrors && actual.control==((kind<<8)|hops) && actual.terminal==terminal,
                        "Native reflection path identity/order/terminal kind mismatch");
                    const double error=std::sqrt(dot(sub(feature,V{actual.feature[0],actual.feature[1],actual.feature[2]}),
                        sub(feature,V{actual.feature[0],actual.feature[1],actual.feature[2]})));
                    max_direction=std::max(max_direction,error);
                    check(error<.00008 && (actual.incoming>>24)==255,"Native ordered terminal feature/direction invalid");
                    for(unsigned c=0;c<3;++c) {
                        const auto error=std::abs(throughput[c]-actual.response[c]);max_response=std::max(max_response,error);
                        check(error<.012,"Native per-hop conductor throughput is not current separated response");
                    }
                    if(kind!=1) {
                        bool same=actual.incoming==0xff306090U;
                        if(variant==2) {
                            // Native UNORM capture can resolve the exact .5
                            // halfway clear value to 127 OR 128. Compare against
                            // the authored float with half an input quantum,
                            // not a guessed tie direction. OFF is still exact.
                            const V authored{.125,.25,.5};same=(actual.incoming>>24)==255;
                            for(unsigned c=0;c<3;++c) same=same &&
                                std::abs(double((actual.incoming>>(c*8))&255)/255.-authored[c])<=.5/255.+1.e-6;
                        }
                        if(!same)
                            throw std::runtime_error("Incident environment mismatch: variant="+std::to_string(variant)
                                +" metal="+std::to_string(metal)+" transport="+std::to_string(transport)
                                +" eye="+std::to_string(eye)+" pixel="+std::to_string(index)
                                +" actual="+std::to_string(actual.incoming)+" expected="+std::to_string(variant==2?0xff804020U:0xff306090U));
                    }
                    else {
                        const V raw=terminal<2?V{.3,.3,.3}:V{.8,.1,.1};
                        for(unsigned c=0;c<3;++c) check(std::abs(double((actual.incoming>>(c*8))&255)/255.-raw[c])<=1./255,
                            "Incident finite feature contains primary material response or wrong source colour");
                    }
                    ++hop_counts[hops];finite_count+=kind==1;escape_count+=kind==2;budget_count+=kind==3;++samples;
                }
            }
            SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
            check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,0xff306090,rough,metal,{},g.environment_face_size,
                g.environment_rotation,nullptr,{},0,nullptr,false,range,g.environment_offset,1,transport),hardware[eye].status().c_str());
            std::vector<unsigned char> off;check(hardware[eye].readback(off),hardware[eye].status().c_str());
            check(off==prefix,"Ordered witness producer altered reflected RGBA versus history OFF");++prefixes;
        }
    }
    check(samples>100000 && finite_count>1000 && escape_count>1000 && budget_count>1000
        && edges*100<samples,"Native ordered path coverage insufficient");
    for(unsigned count:hop_counts) check(count>100,"Native ordered path hop class lacks positive coverage");
    std::cout<<"Native ordered path producer: "<<samples<<" binary64 physical paths; finite/escape/budget "
        <<finite_count<<'/'<<escape_count<<'/'<<budget_count<<"; hop counts ";
    for(auto count:hop_counts) std::cout<<count<<' ';
    std::cout<<"; "<<edges<<" bounded edge exclusions, "<<prefixes<<" exact OFF RGBA prefixes; max feature/response error "
        <<max_direction<<'/'<<max_response<<". Two eyes, 1/8 lobes, four roughnesses/conductor codes, constant and resident cube environment; no cached colour/visibility acceptance.\n";
}
