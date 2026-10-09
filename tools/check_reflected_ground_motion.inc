// Reuse only the independent CPU ray/triangle oracle, not GPU hit results.
// The native ground is analytic and not part of the model AS.
{
    std::uint64_t ground_checked=0,ground_valid=0;
    for(unsigned variant=0;variant<8;++variant) {
        const std::array current{quad(0,.55F,-3,1.1F,{.8F,.1F,.1F,1}),
            quad(-1.6F,.75F,-3.3F,.5F,{.1F,.8F,.2F,1})};
        auto accepted=current;
        accepted[0]=quad(.13F,.48F,-2.9F,1.1F,{.8F,.1F,.1F,1});
        std::array<render::CalibratedSceneDraw,2> draws;
        std::array<render::CalibratedSceneMotionDraw,2> matches;
        for(unsigned i=0;i<2;++i) {
            draws[i].vertices=current[i];draws[i].ray_caster=true;
            matches[i].valid=variant!=2;
            if(matches[i].valid) {
                matches[i].previous_vertices=accepted[i];
                matches[i].previous_ray_first_triangle=variant==3?(1-i)*2:i*2;
            }
        }
        std::array<XrView,2> eyes{};
        for(unsigned e=0;e<2;++e) {
            eyes[e].type=XR_TYPE_VIEW;eyes[e].pose.orientation.w=1;
            eyes[e].pose.position.x=e?.033F:-.031F;eyes[e].fov={-.65F,.72F,.6F,-.57F};
        }
        auto old_eyes=eyes;
        for(auto& eye:old_eyes) {eye.pose.position.y=.02F;eye.pose.position.z=.025F;}
        std::array<render::CalibratedRayGeometryOutput,2> geometries;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        for(unsigned e=0;e<2;++e) {
            const render::CalibratedSceneMotion motion{camera_for(old_eyes[e],d3d),matches,173,119};
            geometries[e]=scene.enqueue_ray_geometry(gpu.command,e,width,height,camera_for(eyes[e],d3d),256,0,{},&motion);
            check(geometries[e].complete,scene.status().c_str());
        }
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Analytic ground source submission lost");
        for(unsigned e=0;e<2;++e) {
            const auto& g=geometries[e];
            RayReflectionHistory history{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                variant==6?4000:g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
            const double angle=variant==4?.21:-.11;
            const V planePoint{0,variant==5?-300.:240.,0},planeNormal{std::sin(angle),-std::cos(angle),.04};
            if(variant!=1) history.previous_ground=RayReflectionGround{planePoint,planeNormal};
            const ReceiverPlane ground{{0,256,0},{0,-1,0}};
            RayWater water;water.material=variant==7?2:1;water.reflection_strength=.65F;
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
            check(layout==*native_reflection_history(width,height,history.extent,true)
                && !output.water_layers.storage_bytes,"Analytic ground separated-radiance layout lost");
            const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,layout.storage_bytes,0};
            auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
            auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
            auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,info.size};
            const SDL_GPUTransferBufferLocation to{transfer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
            check(SDL_WaitForGPUFences(gpu.device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(gpu.device,fence);
            const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(bytes,SDL_GetError());
            const auto* motions=reinterpret_cast<const std::array<float,4>*>(bytes+layout.motion_offset);
            const auto* identities=reinterpret_cast<const std::array<unsigned,4>*>(bytes+layout.identity_offset);
            const auto* witnesses=reinterpret_cast<const std::array<float,4>*>(bytes+layout.witness_offset);
            const auto* base=reinterpret_cast<const std::array<float,4>*>(bytes+layout.base_offset);
            const auto* response=reinterpret_cast<const std::array<float,4>*>(bytes+layout.weight_offset);
            const auto* incoming=reinterpret_cast<const std::uint32_t*>(bytes+layout.incoming_offset);
            std::vector<unsigned char> prefix(bytes,bytes+width*height*4);
            std::array<V,12> now{},before{};
            for(unsigned i=0;i<2;++i) for(unsigned v=0;v<6;++v) {
                const auto a=world_eye(eyes[e],model_point(draws[i].model,current[i][v]));
                const auto b=world_eye(old_eyes[e],model_point(matches[i].previous_model,accepted[i][v]));
                now[i*6+v]={a[0]*256,-a[1]*256,-a[2]*256};before[i*6+v]={b[0]*256,-b[1]*256,-b[2]*256};
            }
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const unsigned p=y*width+x;
                const V direction=normal({(x+.5-g.projection[2])/g.projection[0],(y+.5-g.projection[3])/g.projection[1],1});
                const double distance=direction[1]>0?256/direction[1]:0;
                if(distance<=g.near_plane/direction[2] || distance>=g.far_plane.value_or(65536)/direction[2]) continue;
                const auto primary=trace(now,{},direction,g.near_plane/direction[2],distance);
                if(primary.found || primary.edge) continue;
                const double bias=std::max(.05,distance*1.e-5);
                const V reflected{direction[0],-direction[1],direction[2]};
                const auto secondary=trace(now,add(scale(direction,distance),V{0,-bias,0}),reflected,bias,65536);
                if(secondary.edge) continue;
                std::array<unsigned,4> ids{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
                std::array<double,4> witness{},motion{};
                if(secondary.found) {
                    ids[0]=UINT32_MAX-1;ids[1]=secondary.primitive;
                    if(variant!=1) {
                        ids[2]=UINT32_MAX-1;
                        if(matches[secondary.primitive/2].valid)
                            ids[3]=matches[secondary.primitive/2].previous_ray_first_triangle+secondary.primitive%2;
                    }
                    witness={secondary.u,secondary.v,distance*direction[2],1};
                    if(variant!=1 && matches[secondary.primitive/2].valid) {
                        const unsigned q=secondary.primitive*3;const V n=normal(planeNormal);
                        const V feature=add(add(scale(before[q],1-secondary.u-secondary.v),scale(before[q+1],secondary.u)),scale(before[q+2],secondary.v));
                        const V virtualFeature=sub(feature,scale(n,2*dot(sub(feature,planePoint),n)));
                        const double denominator=dot(virtualFeature,n);
                        const double fraction=denominator?dot(planePoint,n)/denominator:0;
                        const V receiver=scale(virtualFeature,fraction);
                        const double px=history.projection[0]*virtualFeature[0]/virtualFeature[2]+history.projection[2];
                        const double py=history.projection[1]*virtualFeature[1]/virtualFeature[2]+history.projection[3];
                        if(virtualFeature[2]>0 && fraction>0 && fraction<1 && receiver[2]>=history.near_plane
                            && receiver[2]<=history.far_plane && px>=0 && py>=0 && px<history.extent[0] && py<history.extent[1])
                            motion={px,py,receiver[2],1};
                    }
                    check((incoming[p]>>24)==255 && base[p][3]==1 && response[p][3]==0,
                        "Finite ground reflection lost separated incident/base/response");
                    for(unsigned c=0;c<3;++c) {
                        const float incident=float((incoming[p]>>(8*c))&255)/255;
                        const double final=std::clamp(double(base[p][c])+incident*response[p][c],0.,1.)*255;
                        check(std::abs(final-prefix[p*4+c])<=1.01,"Ground decomposed light did not reconstruct the original native RGB");
                    }
                }
                check(identities[p]==ids,"Analytic ground hit identity/matching oracle mismatch");
                for(unsigned c=0;c<4;++c) {
                    check(std::isfinite(witnesses[p][c]) && std::abs(witnesses[p][c]-witness[c])<(c==2?.02:.00001),
                        "Analytic ground depth/barycentric oracle mismatch");
                    if(!std::isfinite(motions[p][c]) || std::abs(motions[p][c]-motion[c])>(c==2?.025:.02))
                        throw std::runtime_error("Analytic ground motion mismatch variant="+std::to_string(variant)+" eye="+std::to_string(e)
                            +" pixel="+std::to_string(p)+" axis="+std::to_string(c)+" actual="+std::to_string(motions[p][c])+" expected="+std::to_string(motion[c]));
                }
                ++ground_checked;if(motion[3])++ground_valid;
            }
            SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
            check(encode(nullptr),hardware[e].status().c_str());
            std::vector<unsigned char> off;check(hardware[e].readback(off),hardware[e].status().c_str());
            check(off==prefix,"Optional ground correspondence changed unfiltered native RGB");
            // Water cannot borrow the flat-metal producer's previous plane.
            water.material=0;
            check(!encode(&history) && !hardware[e].reflection_output().buffer,"Water history borrowed flat-metal plane metadata");
            auto invalid=history;invalid.previous_ground=RayReflectionGround{{},{}};
            water.material=1;
            check(!encode(&invalid) && !hardware[e].reflection_output().buffer,"Ground history accepted a degenerate old plane");
        }
    }
    check(ground_checked>10000 && ground_valid>1000,"Analytic ground correspondence oracle coverage insufficient");
    std::cout<<"Analytic mirror/gold ground: "<<ground_checked<<" physical ray/plane checks, "<<ground_valid
        <<" accepted finite secondary features; two eyes, old tilted/translated plane, missing/unmatched/reordered/behind/clipped history, current radiance reconstruction, exact OFF RGB and conflicting liquid-history rejection passed\n";
}
