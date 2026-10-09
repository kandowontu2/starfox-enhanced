// Actual native RT producer, independent binary64 FORWARD ray equations.
// Cache/reprojection colour checks live in the separate consumer fixture.
{
    unsigned checked_lobes=0,finite_lobes=0,edge_lobes=0,exact_prefixes=0;
    constexpr unsigned lw=96,lh=72;
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},
        {.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    for(float rough:{0.F,.04F,.12F,.20F,.35F,.62F,1.F}) for(unsigned metal=0;metal<=3;++metal) {
        const unsigned lobes=rough>0?8:1;
        std::array vertices{quad(0,0,-2,1.7F,{.3F,.3F,.3F,1}),quad(.94F,.03F,-.6F,.23F,{.8F,.1F,.1F,1})};
        std::array<render::CalibratedSceneDraw,2> draws;
        std::array<render::CalibratedSceneMotionDraw,2> matches;
        for(unsigned i=0;i<2;++i) {
            draws[i].vertices=vertices[i];draws[i].ray_caster=true;
            matches[i].previous_model=identity;matches[i].previous_vertices=vertices[i];
            matches[i].valid=true;matches[i].previous_ray_first_triangle=2*i;
        }
        std::array<XrView,2> eyes{};for(auto& eye:eyes) {eye.type=XR_TYPE_VIEW;eye.pose.orientation.w=1;eye.fov={-.57F,.73F,.61F,-.49F};}
        eyes[0].pose.position.x=-.032F;eyes[1].pose.position.x=.034F;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        std::array<render::CalibratedRayGeometryOutput,2> rays;
        for(unsigned eye=0;eye<2;++eye) {
            const auto camera=camera_for(eyes[eye],d3d);const render::CalibratedSceneMotion previous{camera,matches,lw,lh};
            rays[eye]=scene.enqueue_ray_geometry(gpu.command,eye,lw,lh,camera,256,0,{},&previous);
            check(rays[eye].complete,scene.status().c_str());
        }
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Compact lobe source submission lost ownership");
        for(unsigned eye=0;eye<2;++eye) {
            const auto& g=rays[eye];const render::GpuScene::RayGeometryOutput resident{g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
            RayReflectionHistory previous{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
            previous.model_lobes=lobes;
            const Camera camera{lw,lh,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
            const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
            const std::array<std::uint32_t,256> palette{};
            check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,0xff306090,rough,metal,{},0,
                g.environment_rotation,nullptr,{},0,nullptr,false,range,0,1,false,&previous),hardware[eye].status().c_str());
            const auto output=hardware[eye].reflection_output();
            check(output.reflection_history==*native_reflection_history(lw,lh,{lw,lh},true,{},lobes),"Native compact lobe layout mismatch");
            const auto size=output.reflection_history.storage_bytes;const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,size,0};
            auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
            auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
            auto* pass=SDL_BeginGPUCopyPass(command);check(pass,SDL_GetError());
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,size};const SDL_GPUTransferBufferLocation to{transfer,0};
            SDL_DownloadFromGPUBuffer(pass,&from,&to);SDL_EndGPUCopyPass(pass);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
            check(SDL_WaitForGPUFences(gpu.device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(gpu.device,fence);
            const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(bytes,SDL_GetError());
            const unsigned count=lw*lh;const auto* primary_ids=reinterpret_cast<const unsigned*>(bytes+count*4);
            const auto* depth=reinterpret_cast<const float*>(bytes+count*8);
            const auto* response=reinterpret_cast<const std::array<float,4>*>(bytes+count*12);
            const auto* records=reinterpret_cast<const std::array<unsigned,3>*>(bytes+count*28);
            std::vector<unsigned char> prefix(bytes,bytes+count*4);
            std::array<V,12> positions;
            for(unsigned draw=0;draw<2;++draw) for(unsigned v=0;v<6;++v) {
                const auto p=world_eye(eyes[eye],model_point(draws[draw].model,vertices[draw][v]));
                positions[draw*6+v]={p[0]*256,-p[1]*256,-p[2]*256};
            }
            for(unsigned y=0;y<lh;++y) for(unsigned x=0;x<lw;++x) {
                const unsigned index=y*lw+x;const V direction=normal({(x+.5-g.projection[2])/g.projection[0],(y+.5-g.projection[3])/g.projection[1],1});
                const auto primary=trace(positions,{},direction,g.near_plane/direction[2],g.far_plane.value_or(65536)/direction[2]);
                if(primary.edge) {edge_lobes+=lobes;continue;}
                check(primary_ids[index]==(primary.found?primary.primitive:UINT32_MAX),"Native compact primary identity mismatch");
                if(!primary.found) {
                    check(depth[index]==0 && response[index]==std::array<float,4>{},"No receiver retained compact material/depth data");
                    for(unsigned lobe=0;lobe<lobes;++lobe) check(records[index*lobes+lobe]==std::array<unsigned,3>{UINT32_MAX,0,0},"Sky retained uninitialized lobe words");
                    continue;
                }
                check(std::abs(depth[index]-primary.t*direction[2])<.002 && response[index][3]==1,"Compact primary depth/decomposition mismatch");
                const unsigned p=primary.primitive*3;V n=normal(cross(sub(positions[p+1],positions[p]),sub(positions[p+2],positions[p])));
                if(dot(n,direction)>0) n=scale(n,-1);
                const V reflected=sub(direction,scale(n,2*dot(n,direction)));
                const V tangent=normal(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
                const double bias=std::max(.01,primary.t*1.e-5);const V origin=add(scale(direction,primary.t),scale(n,bias));
                const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),n),0.,1.),5);
                const V f0=metal==2?V{1,.766,.336}:metal==3?V{.955,.638,.538}:V{77./255,77./255,77./255};
                for(unsigned c=0;c<3;++c) if(metal!=1 || primary.primitive<2)
                    check(std::abs(response[index][c]-(metal?f0[c]+(1-f0[c])*grazing:1))<.004,
                        "Compact primary conductor response is not current Fresnel");
                for(unsigned lobe=0;lobe<lobes;++lobe) {
                    V outgoing=normal(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),double(rough)*rough)));
                    if(dot(outgoing,n)<=0) outgoing=reflected;
                    const auto hit=trace(positions,origin,outgoing,bias,65536);
                    if(hit.edge) {++edge_lobes;continue;}
                    const auto& actual=records[index*lobes+lobe];
                    check(actual[0]==(hit.found?hit.primitive:UINT32_MAX) && (actual[2]>>24)==255,
                        "Compact rough ray borrowed another lobe or lost incident RGBA");
                    if(hit.found) {
                        const double u=(actual[1]&65535)/65535.,v=(actual[1]>>16)/65535.;
                        check(std::abs(u-hit.u)<=2./65535 && std::abs(v-hit.v)<=2./65535 && u+v<=1.000001,
                            "Compact secondary feature quantization exceeds two UNORM units");++finite_lobes;
                    }
                    ++checked_lobes;
                }
            }
            SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
            check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,0xff306090,rough,metal,{},0,
                g.environment_rotation,nullptr,{},0,nullptr,false,range,0,1,false),hardware[eye].status().c_str());
            std::vector<unsigned char> off;check(hardware[eye].readback(off),hardware[eye].status().c_str());
            check(off==prefix,"Compact lobe producer changed current reflected RGBA versus history OFF");++exact_prefixes;
        }
    }
    check(checked_lobes>100000 && finite_lobes>1000 && edge_lobes*100<checked_lobes,"Compact native lobe oracle coverage inadequate");
    std::cout<<"Compact native lobe producer: "<<checked_lobes<<" binary64 physical rays, "<<finite_lobes
        <<" finite features, "<<edge_lobes<<" bounded edge exclusions, "<<exact_prefixes
        <<" byte-exact OFF prefixes; 1/8 lobes, seven roughnesses, all four conductor codes, two eyes passed. No multibounce/environment history claim.\n";
}
