// Actual native water/lava producer, not hand-packed resolver input. Geometry
// and old finite features come from an independent binary64 ray oracle.
{
    namespace liquid_oracle=reflected_liquid_oracle;
    std::array<std::uint64_t,2> liquid_checked{},liquid_valid{},liquid_finite{};
    double liquid_error=0;unsigned prefix_checks=0,lava_reference_crossings=0;
    const auto download=[&](const GpuReflectionOutput& output) {
        const unsigned bytes=output.reflection_history.storage_bytes?output.reflection_history.storage_bytes
            :output.water_layers.storage_bytes?output.water_layers.storage_bytes:output.width*output.height*4;
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes,0};
        auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
        auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
        auto* pass=SDL_BeginGPUCopyPass(command);check(pass,SDL_GetError());
        const SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output.buffer),0,bytes};
        const SDL_GPUTransferBufferLocation target{transfer,0};SDL_DownloadFromGPUBuffer(pass,&source,&target);SDL_EndGPUCopyPass(pass);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
        check(SDL_WaitForGPUFences(gpu.device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(gpu.device,fence);
        const auto* words=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(words,SDL_GetError());
        std::vector<unsigned char> result(words,words+bytes);
        SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);return result;
    };
    for(unsigned kind=0;kind<2;++kind) for(unsigned variant=0;variant<6;++variant) {
        const auto current=std::array{quad(0,.55F,-3,1.1F,{.8F,.1F,.1F,1}),
            quad(-1.6F,.75F,-3.3F,.5F,{.1F,.8F,.2F,1})};
        auto previous=current;previous[0]=quad(.13F,.48F,-2.9F,1.1F,{.8F,.1F,.1F,1});
        std::array<render::CalibratedSceneDraw,2> draws;
        std::array<render::CalibratedSceneMotionDraw,2> matches;
        for(unsigned i=0;i<2;++i) {
            draws[i].vertices=current[i];draws[i].ray_caster=true;matches[i].valid=variant!=2;
            if(matches[i].valid) {
                matches[i].previous_vertices=previous[i];matches[i].previous_ray_first_triangle=variant==3?(1-i)*2:i*2;
            }
        }
        std::array<XrView,2> eyes{};
        for(unsigned e=0;e<2;++e) {
            eyes[e].type=XR_TYPE_VIEW;eyes[e].pose.orientation.w=1;
            eyes[e].pose.position.x=e?.033F:-.031F;eyes[e].fov={-.65F,.72F,.6F,-.57F};
        }
        auto old_eyes=eyes;for(auto& e:old_eyes) {e.pose.position.y=.02F;e.pose.position.z=.025F;}
        std::array<render::CalibratedRayGeometryOutput,2> geometries;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        for(unsigned e=0;e<2;++e) {
            const render::CalibratedSceneMotion motion{camera_for(old_eyes[e],d3d),matches,173,119};
            geometries[e]=scene.enqueue_ray_geometry(gpu.command,e,width,height,camera_for(eyes[e],d3d),256,0,{},&motion);
            check(geometries[e].complete,scene.status().c_str());
        }
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Native liquid source submission lost");
        for(unsigned e=0;e<2;++e) {
            const auto& g=geometries[e];
            std::cout<<"Native liquid producer encode kind="<<kind<<" variant="<<variant<<" eye="<<e<<std::endl;
            const unsigned material=kind?3:0;
            RayReflectionHistory history{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                variant==4?4000:g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
            if(variant!=1) {
                history.previous_liquid=RayReflectionLiquid{};auto& old=*history.previous_liquid;
                old.material=material;old.time=variant==5?7.2F:1.3F;
                old.ground={{0,240,0},{.07F,-1,.025F}};
                old.world_to_view={1.01F,0,.02F,0,.99F,0,-.015F,0,1.03F};old.offset={29,-3,17};
            }
            const ReceiverPlane ground{{0,256,0},{0,-1,0}};
            RayWater water;water.material=material;water.reflection_strength=.85F;water.time=2.4F;
            if(!kind) {
                water.source_colour=std::array<float,3>{.1F,.3F,.4F};
                water.surface_layers=variant%3==1;water.auxiliary_layers=variant%3==2;
            }
            const render::GpuScene::RayGeometryOutput resident{g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
            const Camera camera{width,height,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
            const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
            const std::array<std::uint32_t,256> palette{};
            const auto encode=[&](const RayReflectionHistory* old) {
                return hardware[e].render_reflections(gpu.device,camera,resident,palette,0xff306090,0,0,{},0,
                    g.environment_rotation,nullptr,ground,0,&water,false,range,0,1,false,old);
            };
            check(encode(&history),hardware[e].status().c_str());
            const auto output=hardware[e].reflection_output();const auto layout=output.reflection_history;
            check(layout==*native_reflection_history(width,height,history.extent,true,output.water_layers),
                "Native liquid producer lost combined/separated canonical storage");
            const auto bytes=download(output);
            const auto* motions=reinterpret_cast<const std::array<float,4>*>(bytes.data()+layout.motion_offset);
            const auto* identities=reinterpret_cast<const std::array<unsigned,4>*>(bytes.data()+layout.identity_offset);
            const auto* witnesses=reinterpret_cast<const std::array<float,4>*>(bytes.data()+layout.witness_offset);
            const auto* incoming=reinterpret_cast<const std::uint32_t*>(bytes.data()+layout.incoming_offset);
            const auto* base=reinterpret_cast<const std::array<float,4>*>(bytes.data()+layout.base_offset);
            const auto* response=reinterpret_cast<const std::array<float,4>*>(bytes.data()+layout.weight_offset);
            std::array<V,12> now{},before{};
            for(unsigned i=0;i<2;++i) for(unsigned v=0;v<6;++v) {
                const auto a=world_eye(eyes[e],model_point(draws[i].model,current[i][v]));
                const auto b=world_eye(old_eyes[e],model_point(matches[i].previous_model,previous[i][v]));
                now[i*6+v]={a[0]*256,-a[1]*256,-a[2]*256};before[i*6+v]={b[0]*256,-b[1]*256,-b[2]*256};
            }
            liquid_oracle::Frame current_frame;
            current_frame.point={0,256,0};current_frame.normal={0,-1,0};current_frame.projection=g.projection;
            current_frame.width=width;current_frame.height=height;current_frame.material=material;
            current_frame.near=range.near_depth;current_frame.far=range.far_depth;current_frame.time=water.time;
            liquid_oracle::Frame old_frame;
            if(history.previous_liquid) {
                const auto& old=*history.previous_liquid;
                old_frame.point=old.ground.point;old_frame.normal=old.ground.normal;old_frame.rotation=old.world_to_view;
                old_frame.offset=old.offset;old_frame.time=old.time;old_frame.material=old.material;
                old_frame.projection=history.projection;old_frame.width=history.extent[0];old_frame.height=history.extent[1];
                old_frame.near=history.near_plane;old_frame.far=history.far_plane;
            }
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const unsigned p=y*width+x;
                const auto ray=liquid_oracle::optical(current_frame,x+.5,y+.5);if(!ray.valid) continue;
                const auto primary=trace(now,{},ray.direction,range.near_depth/ray.direction[2],ray.distance);
                if(primary.found || primary.edge) continue;
                const auto secondary=trace(now,add(ray.hit,scale(ray.normal,ray.bias)),
                    liquid_oracle::reflect(ray.direction,ray.normal),ray.bias,65536);
                if(secondary.edge) continue;
                ++liquid_checked[kind];
                std::array<unsigned,4> ids{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
                if(secondary.found) {
                    if(kind) {
                        const V origin=add(ray.hit,scale(ray.normal,ray.bias));
                        const V escape=liquid_oracle::reflect(ray.direction,ray.normal);
                        const double denominator=dot(escape,current_frame.normal);
                        const double plane=denominator>0?dot(sub(current_frame.point,origin),current_frame.normal)/denominator:0;
                        lava_reference_crossings+=plane>ray.bias && plane<secondary.t;
                    }
                    ++liquid_finite[kind];ids[0]=UINT32_MAX-1;ids[1]=secondary.primitive;
                    if(history.previous_liquid) {
                        ids[2]=UINT32_MAX-1;
                        if(matches[secondary.primitive/2].valid)
                            ids[3]=matches[secondary.primitive/2].previous_ray_first_triangle+secondary.primitive%2;
                    }
                    if(witnesses[p][3]!=1 || std::abs(witnesses[p][0]-secondary.u)>=.0001
                        || std::abs(witnesses[p][1]-secondary.v)>=.0001 || std::abs(witnesses[p][2]-ray.depth)>=.02)
                        throw std::runtime_error("Native liquid secondary barycentric/analytic-depth witness mismatch kind="+std::to_string(kind)
                            +" variant="+std::to_string(variant)+" eye="+std::to_string(e)+" pixel="+std::to_string(p)
                            +" actual="+std::to_string(witnesses[p][0])+","+std::to_string(witnesses[p][1])+","
                            +std::to_string(witnesses[p][2])+","+std::to_string(witnesses[p][3])
                            +" expected="+std::to_string(secondary.u)+","+std::to_string(secondary.v)+","+std::to_string(ray.depth)
                            +" primitive="+std::to_string(secondary.primitive)+" actualPrimitive="+std::to_string(identities[p][1]));
                    check((incoming[p]>>24)==255 && base[p][3]==1 && response[p][3]==0,
                        "Finite native liquid hit lost separated radiance");
                    const auto& actor=current[secondary.primitive/2][0];
                    for(unsigned c=0;c<3;++c) {
                        const double incident=double((incoming[p]>>(8*c))&255)/255;
                        check(std::abs(incident-actor.color[c])<1.01/255,"Native liquid incident RGB is not the independently hit actor");
                        const double reconstructed=std::clamp(double(base[p][c])+incident*response[p][c],0.,1.)*255;
                        check(std::abs(reconstructed-bytes[p*4+c])<=1.01,"Native liquid current radiance decomposition does not reconstruct RGB");
                    }
                } else check(witnesses[p][3]==0,"Missing native liquid secondary seeded history");
                check(identities[p]==ids,"Native liquid current/accepted primitive mapping differs from ray oracle");
                if(motions[p][3]) {
                    check(history.previous_liquid && secondary.found && matches[secondary.primitive/2].valid,
                        "Unmatched/native liquid seed frame emitted accepted motion");
                    const unsigned q=secondary.primitive*3;
                    const V feature=add(add(scale(before[q],1-secondary.u-secondary.v),scale(before[q+1],secondary.u)),scale(before[q+2],secondary.v));
                    const auto old_ray=liquid_oracle::optical(old_frame,motions[p][0],motions[p][1]);
                    const double error=liquid_oracle::error(old_frame,feature,old_ray,motions[p][0],motions[p][1]);
                    if(!old_ray.valid || !std::isfinite(error) || error>.03 || std::abs(old_ray.depth-motions[p][2])>.04)
                        throw std::runtime_error("Native liquid old-wave optics mismatch kind="+std::to_string(kind)
                            +" variant="+std::to_string(variant)+" pixel="+std::to_string(p)+" error="+std::to_string(error));
                    liquid_error=std::max(liquid_error,error);++liquid_valid[kind];
                } else check(motions[p]==std::array<float,4>{},"Rejected native liquid motion is not zero");
            }
            check(encode(nullptr),hardware[e].status().c_str());const auto baseline=download(hardware[e].reflection_output());
            check(baseline.size()<=bytes.size() && std::equal(baseline.begin(),baseline.end(),bytes.begin()),
                "Native liquid optional history changed current RGBA/world/surface prefix");++prefix_checks;
        }
    }
    check(liquid_checked[0]>10000 && liquid_checked[1]>10000 && liquid_finite[0]>1000 && liquid_finite[1]>1000
        && liquid_valid[0]>100 && liquid_valid[1]>100 && lava_reference_crossings>10,
        "Native liquid producer lacks finite/current/accepted or displaced-wave escape coverage");
    std::cout<<"Native liquid producer: water/lava "<<liquid_checked[0]<<'/'<<liquid_checked[1]<<" independent physical rays, "
        <<liquid_finite[0]<<'/'<<liquid_finite[1]<<" finite features, "<<liquid_valid[0]<<'/'<<liquid_valid[1]
        <<" accepted old-wave paths; max optical pixel error "<<liquid_error<<"; "<<prefix_checks
        <<" exact OFF RGBA/world/surface comparisons, "<<lava_reference_crossings
        <<" displaced lava rays escaping the reference plane; separate incoming/base/response, two eyes, current/old clocks and transforms, missing/unmatched/reordered/clipped frames passed. Runtime owner/performance separate.\n";
}
