// Real native scene-path producer. Binary64 forward intersection selects
// each finite model OR the actual analytic plane; no CPU inverse/geometry
// readback supplies the expected path. Full RGBA must equal history OFF.
{
    struct Record {std::array<unsigned,4> mirrors;unsigned control,terminal;
        std::array<float,3> feature;unsigned incoming;std::array<float,3> response;};
    static_assert(sizeof(Record)==52);
    constexpr unsigned pw=96,ph=72,floor_id=0xfffffffeU;
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    unsigned primary_floor=0,primary_model=0,ground_bounces=0,model_bounces=0,prefixes=0,boundaries=0;
    std::array<unsigned,5> hop_counts{};std::uint64_t samples=0;double max_feature=0,max_response=0;
    for(unsigned variant=0;variant<3;++variant) for(unsigned ground_material:{1U,2U})
    for(float rough:{0.F,.12F,.35F}) for(unsigned metal=0;metal<=3;++metal) for(bool transport:{false,true}) {
        std::array vertices{quad(0,0,-2,1.7F,{.3F,.3F,.3F,1}),
            quad(variant?1.15F:.94F,.03F,-.6F,variant?.82F:.23F,{.8F,.1F,.1F,1})};
        std::array<render::CalibratedSceneDraw,2> draws;std::array<render::CalibratedSceneMotionDraw,2> matches;
        for(unsigned i=0;i<2;++i) {
            draws[i].vertices=vertices[i];draws[i].ray_caster=true;
            matches[i].previous_model=identity;matches[i].previous_vertices=vertices[i];
            matches[i].valid=true;matches[i].previous_ray_first_triangle=i*2;
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
            const auto camera=camera_for(eyes[eye],d3d);const render::CalibratedSceneMotion before{camera,matches,pw,ph};
            rays[eye]=scene.enqueue_ray_geometry(gpu.command,eye,pw,ph,camera,256,0,{.125F,.25F,.5F,1},&before);
            check(rays[eye].complete,scene.status().c_str());
        }
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Scene-path geometry lost submission ownership");
        for(unsigned eye=0;eye<2;++eye) {
            const auto& g=rays[eye];const render::GpuScene::RayGeometryOutput resident{
                g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
            const unsigned lobes=rough>0?8:1;
            RayReflectionHistory before{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
            before.model_lobes=lobes;before.model_paths=before.scene_paths=true;
            const float angle=variant*.035F;
            const V floor_point{0,96.+variant*9,0},floor_normal{double(std::sin(angle)),-double(std::cos(angle)),0};
            const ReceiverPlane floor{{floor_point[0],floor_point[1],floor_point[2]},{floor_normal[0],floor_normal[1],floor_normal[2]}};
            before.previous_ground=RayReflectionGround{floor_point,floor_normal};
            RayWater water;water.material=ground_material;water.reflection_strength=.8F;water.mirror_models=transport;
            water.world_to_view={std::cos(angle),-std::sin(angle),0,std::sin(angle),std::cos(angle),0,0,0,1};
            const Camera camera{pw,ph,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
            const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
            const std::array<std::uint32_t,256> palette{};
            check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,0xff306090,rough,metal,{},0,
                g.environment_rotation,nullptr,floor,0,&water,false,range,0,1,transport,&before),hardware[eye].status().c_str());
            const auto output=hardware[eye].reflection_output();const unsigned count=pw*ph;
            check(output.reflection_history==*native_reflection_history(pw,ph,{pw,ph},true,{},lobes,true,true),
                "Native mixed scene prefix/layout mismatch");
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
            const auto* ids=reinterpret_cast<const unsigned*>(bytes+count*4);
            const auto* depths=reinterpret_cast<const float*>(bytes+count*8);
            const auto* response=reinterpret_cast<const std::array<float,4>*>(bytes+count*12);
            const auto* bases=reinterpret_cast<const std::array<float,4>*>(bytes+count*28);
            const auto* records=reinterpret_cast<const Record*>(bytes+count*44);
            std::vector<unsigned char> prefix(bytes,bytes+count*4);std::array<V,12> positions;
            for(unsigned draw=0;draw<2;++draw) for(unsigned v=0;v<6;++v) {
                const auto p=world_eye(eyes[eye],model_point(draws[draw].model,vertices[draw][v]));
                positions[draw*6+v]={p[0]*256,-p[1]*256,-p[2]*256};
            }
            const auto floor_distance=[&](V origin,V direction,double near,double far) {
                const double denominator=dot(direction,floor_normal);
                if(std::abs(denominator)<=1.e-8) return far;
                const double t=dot(sub(floor_point,origin),floor_normal)/denominator;
                return t>near && t<far?t:far;
            };
            const auto f0=[&](unsigned primitive) {
                return metal==2?V{1,.766,.336}:metal==3?V{.955,.638,.538}:
                    primitive<2?V{77./255,77./255,77./255}:V{204./255,26./255,26./255};
            };
            for(unsigned y=0;y<ph;++y) for(unsigned x=0;x<pw;++x) {
                const unsigned index=y*pw+x;
                const V direction=normal({(x+.5-g.projection[2])/g.projection[0],(y+.5-g.projection[3])/g.projection[1],1});
                const double near=g.near_plane/direction[2],far=g.far_plane.value_or(65536)/direction[2];
                const double floor_t=floor_distance({},direction,near,far);
                const auto primary=trace(positions,{},direction,near,floor_t);
                if(primary.edge) {boundaries+=lobes;continue;}
                const bool primary_is_floor=!primary.found && floor_t<far;
                const unsigned expected_id=primary_is_floor?floor_id:primary.found?primary.primitive:UINT32_MAX;
                check(ids[index]==expected_id,"Mixed scene selected a model through its nearer floor or lost primary ownership");
                if(expected_id==UINT32_MAX) {
                    for(unsigned l=0;l<lobes;++l) check(!records[index*lobes+l].control,"No receiver retained mixed path records");
                    continue;
                }
                primary_floor+=primary_is_floor;primary_model+=!primary_is_floor;
                const double distance=primary_is_floor?floor_t:primary.t;
                check(std::abs(depths[index]-distance*direction[2])<.003,"Mixed scene primary depth mismatch");
                check(bases[index][3]==1 && response[index][3]==1,"Current base/response validity was not separated");
                for(unsigned c=0;c<3;++c) {
                    check(std::isfinite(bases[index][c]) && bases[index][c]>=0 && std::isfinite(response[index][c])
                        && response[index][c]>=0 && response[index][c]<=1,"Mixed primary lighting invalid");
                    if(!primary_is_floor) check(bases[index][c]==0,"Model acquired the floor's direct base lighting");
                }
                V n=floor_normal;
                if(!primary_is_floor) {const unsigned p=primary.primitive*3;n=normal(cross(sub(positions[p+1],positions[p]),sub(positions[p+2],positions[p])));}
                if(dot(n,direction)>0) n=scale(n,-1);
                const V reflected=sub(direction,scale(n,2*dot(n,direction)));
                const V tangent=normal(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
                const double bias=std::max(primary_is_floor?.05:.01,distance*1.e-5);
                const V first_origin=add(scale(direction,distance),scale(n,bias));
                for(unsigned l=0;l<lobes;++l) {
                    V outgoing=primary_is_floor?reflected:normal(add(reflected,scale(add(scale(tangent,taps[l][0]),scale(bitangent,taps[l][1])),double(rough)*rough)));
                    if(dot(outgoing,n)<=0) outgoing=reflected;
                    V origin=first_origin,throughput{1,1,1},feature{};double lower=bias;
                    std::array<unsigned,4> mirrors{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
                    unsigned kind=3,hops=0,terminal=UINT32_MAX;bool boundary=false;
                    for(unsigned bounce=0;bounce<4;++bounce) {
                        const double ground_t=floor_distance(origin,outgoing,lower,65536);
                        const auto hit=trace(positions,origin,outgoing,lower,ground_t);
                        if(hit.edge) {boundary=true;break;}
                        if(!hit.found && ground_t==65536) {kind=2;feature=outgoing;break;}
                        if(hit.found && !transport) {kind=1;terminal=hit.primitive;feature={hit.u,hit.v,0};break;}
                        const bool ground=!hit.found;mirrors[bounce]=ground?floor_id:hit.primitive;++hops;
                        ground_bounces+=ground;model_bounces+=!ground;V hn=floor_normal;
                        if(!ground) {const unsigned p=hit.primitive*3;hn=normal(cross(sub(positions[p+1],positions[p]),sub(positions[p+2],positions[p])));}
                        if(dot(hn,outgoing)>0) hn=scale(hn,-1);
                        if((ground && ground_material==2) || (!ground && metal)) {
                            const V tint=ground?V{1,.766,.336}:f0(hit.primitive);
                            const double grazing=std::pow(1-std::clamp(dot(scale(outgoing,-1),hn),0.,1.),5);
                            for(unsigned c=0;c<3;++c) throughput[c]*=tint[c]+(1-tint[c])*grazing;
                        }
                        const double travel=ground?ground_t:hit.t;lower=std::max(.05,travel*1.e-5);
                        origin=add(add(origin,scale(outgoing,travel)),scale(hn,lower));
                        outgoing=sub(outgoing,scale(hn,2*dot(hn,outgoing)));feature=outgoing;
                    }
                    if(boundary) {++boundaries;continue;}
                    const auto& r=records[index*lobes+l];
                    if(r.mirrors!=mirrors || r.control!=((kind<<8)|hops) || r.terminal!=terminal)
                        throw std::runtime_error("Mixed native path order/terminal mismatch: pixel="+std::to_string(index)+" floor="
                            +std::to_string(primary_is_floor)+" rough="+std::to_string(rough));
                    const V difference=sub(feature,V{r.feature[0],r.feature[1],r.feature[2]});
                    const double feature_error=std::sqrt(dot(difference,difference));max_feature=std::max(max_feature,feature_error);
                    check(feature_error<.00008 && (r.incoming>>24)==255,"Mixed terminal feature/direction invalid");
                    for(unsigned c=0;c<3;++c) {const double error=std::abs(throughput[c]-r.response[c]);max_response=std::max(max_response,error);
                        check(error<.012,"Mixed per-hop model/gold response is not separated current throughput");}
                    if(kind!=1) check(r.incoming==0xff306090U,"Mixed environment contains old ground/model colour");
                    else {
                        const V authored=terminal<2?V{.3,.3,.3}:V{.8,.1,.1};
                        for(unsigned c=0;c<3;++c) check(std::abs(double((r.incoming>>(c*8))&255)/255.-authored[c])<=1./255,
                            "Mixed finite terminal contains primary material response or wrong authored colour");
                    }
                    ++hop_counts[hops];++samples;
                }
            }
            SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
            check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,0xff306090,rough,metal,{},0,
                g.environment_rotation,nullptr,floor,0,&water,false,range,0,1,transport),hardware[eye].status().c_str());
            std::vector<unsigned char> off;check(hardware[eye].readback(off),hardware[eye].status().c_str());
            check(off==prefix,"Mixed path producer changed CURRENT RGBA versus history OFF");++prefixes;
        }
    }
    check(samples>100000 && primary_floor>1000 && primary_model>1000 && ground_bounces>1000 && model_bounces>1000
        && boundaries*100<samples,"Mixed native scene lacks real plane/model path coverage");
    for(unsigned count:hop_counts) check(count>100,"Mixed native scene lacks a bounded hop class");
    std::cout<<"Native mixed scene paths: "<<samples<<" binary64 forward samples; primary floor/model "<<primary_floor<<'/'<<primary_model
        <<"; secondary floor/model "<<ground_bounces<<'/'<<model_bounces<<"; hop counts ";
    for(unsigned count:hop_counts) std::cout<<count<<' ';
    std::cout<<"; "<<prefixes<<" byte-exact history-OFF prefixes; max feature/response "<<max_feature<<'/'<<max_response
        <<". Actual analytic mirror/gold plane, finite models, current base/response, 1/8 model lobes and duplicated sharp floor ray; not cached colour or physical panel acceptance.\n";
}
