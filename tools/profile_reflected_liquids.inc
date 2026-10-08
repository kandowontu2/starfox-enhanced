// Explicit native-producer diagnostic, not an in-game FPS benchmark. The four
// finite triangles are fixed across paired OFF/history blocks; only accepted
// source positions and wave clocks advance. No eye image/ROM dump is written.
void profile_reflected_liquids(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d,
    unsigned w,unsigned h,unsigned frames) {
    using namespace render::shadows;
    check(std::getenv("STARFOX_PROFILE_DXR_TIMINGS")!=nullptr,"Set STARFOX_PROFILE_DXR_TIMINGS for the explicit native timing diagnostic");
    check(w>=256 && w<=1920 && h>=192 && h<=1080 && frames>=12 && frames<=120,
        "Native timing dimensions/frames outside bounded 256..1920 x 192..1080, 12..120");
    std::array<SdlDxrShadows,2> hardware;
    std::array<XrView,2> eyes{};
    for(unsigned eye=0;eye<2;++eye) {
        eyes[eye].type=XR_TYPE_VIEW;eyes[eye].pose.orientation.w=1;
        eyes[eye].pose.position.x=eye?.033F:-.031F;eyes[eye].fov={-.65F,.72F,.6F,-.57F};
    }
    const std::array<std::uint32_t,256> palette{};
    for(unsigned kind=0;kind<2;++kind) {
        const unsigned material=kind?3:0;
        const auto original=std::array{quad(0,.55F,-3,1.1F,{.8F,.1F,.1F,1}),
            quad(-1.6F,.75F,-3.3F,.5F,{.1F,.8F,.2F,1})};
        for(unsigned block=0;block<4;++block) {
            const bool history_on=block==1 || block==2;
            std::cout<<"liquid-profile-block: material="<<material<<" block="<<block
                <<" history="<<history_on<<" width="<<w<<" height="<<h
                <<" frames="<<frames<<" warmup=8 scope=four-triangle-native-producer-not-game-fps"<<std::endl;
            for(unsigned frame=0;frame<frames;++frame) {
                // Repeat the identical source/wave sequence in every ABBA
                // block. A continuously advancing sequence would confound
                // history cost with a different visible optical workload.
                const unsigned step=frame+1;
                auto current=original,old=original;
                for(auto& v:current[0]) v.position[0]+=float(step)*.002F;
                for(auto& v:old[0]) v.position[0]+=float(step-1)*.002F;
                std::array<render::CalibratedSceneDraw,2> draws;
                std::array<render::CalibratedSceneMotionDraw,2> matches;
                for(unsigned i=0;i<2;++i) {
                    draws[i].vertices=current[i];draws[i].ray_caster=true;
                    matches[i].valid=true;matches[i].previous_vertices=old[i];matches[i].previous_ray_first_triangle=i*2;
                }
                std::array<render::CalibratedRayGeometryOutput,2> geometries;
                gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
                check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
                for(unsigned eye=0;eye<2;++eye) {
                    const auto camera=camera_for(eyes[eye],d3d);
                    const render::CalibratedSceneMotion previous{camera,matches,w,h};
                    geometries[eye]=scene.enqueue_ray_geometry(gpu.command,eye,w,h,camera,256,0,{},history_on?&previous:nullptr);
                    check(geometries[eye].complete,scene.status().c_str());
                }
                check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
                check(scene.notify_submitted(token),"Native timing source submission lost");
                for(unsigned eye=0;eye<2;++eye) {
                    const auto& g=geometries[eye];
                    RayWater water;water.material=material;water.reflection_strength=.85F;
                    water.time=2+float(step)/120;
                    if(!kind) water.source_colour=std::array<float,3>{.1F,.3F,.4F};
                    RayReflectionHistory history;
                    if(history_on) {
                        history={g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                            g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
                        history.previous_liquid=RayReflectionLiquid{};
                        history.previous_liquid->material=material;history.previous_liquid->time=2+double(step-1)/120;
                        history.previous_liquid->ground={{0,256,0},{0,-1,0}};
                    }
                    const render::GpuScene::RayGeometryOutput resident{g.device,g.buffer,g.vertex_count,true,
                        g.materials,g.material_offset,g.material_bytes};
                    const Camera camera{w,h,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
                    const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
                    std::cout<<"liquid-profile-frame: material="<<material<<" block="<<block
                        <<" frame="<<frame<<" eye="<<eye<<std::endl;
                    check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,0xff306090,0,0,{},0,
                        g.environment_rotation,nullptr,ReceiverPlane{{0,256,0},{0,-1,0}},0,&water,false,
                        range,0,1,false,history_on?&history:nullptr),hardware[eye].status().c_str());
                    const auto result=hardware[eye].reflection_output();
                    check(result.buffer && result.width==w && result.height==h
                        && (result.reflection_history.storage_bytes!=0)==history_on,"Native timing output lost its requested layout");
                    // Fixture synchronization outside native query intervals:
                    // permit the next geometry upload to reuse resident storage.
                    // This is not a production extra wait or a frame-time sample.
                    check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
                }
            }
        }
    }
    std::cout<<"Native liquid ABBA timing completed: real GPU timestamps; component only, four finite triangles, "
        "no resolver/bank/frame-rate/device-performance acceptance. Concurrent projects remain running.\n";
}
