// Independent physical ray/plane oracle. Never reads GPU geometry to build
// its answer; production hit/barycentric selection remains real hardware RT.
void native_reflected_hit_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d,bool ordered_only=false,bool scene_only=false,bool secondary_lava_only=false,bool indexed_lava=false,bool curved_only=false) {
    using namespace render::shadows;
    const RayReflectionHistory calibrated{64,{173,119},{100,110,83,60},12.8,5120};
    check(calibrated.valid(),"Valid accepted reflection calibration was declined");
    for(unsigned bad=0;bad<8;++bad) {
        auto malformed=calibrated;
        if(bad==0) malformed.previous_vertex_offset=0;
        if(bad==1) malformed.previous_vertex_offset=65;
        if(bad==2) malformed.extent[0]=0;
        if(bad==3) malformed.extent[1]=16385;
        if(bad==4) malformed.projection[1]=0;
        if(bad==5) malformed.projection[2]=std::numeric_limits<double>::infinity();
        if(bad==6) malformed.near_plane=-1;
        if(bad==7) malformed.far_plane=malformed.near_plane;
        check(!malformed.valid(),"Malformed reflected history calibration was accepted");
    }
    check(!native_reflection_history(0,192,{173,119}) && !native_reflection_history(16385,1,{173,119})
        && !native_reflection_history(16384,16384,{173,119}) && !native_reflection_history(256,192,{0,119})
        && native_reflection_history(16384,5041,{173,119}),"Reflected history byte bound overflowed or rejected its last valid extent");
    using V=std::array<double,3>;
    const auto add=[](V a,V b){return V{a[0]+b[0],a[1]+b[1],a[2]+b[2]};};
    const auto sub=[](V a,V b){return V{a[0]-b[0],a[1]-b[1],a[2]-b[2]};};
    const auto scale=[](V a,double t){return V{a[0]*t,a[1]*t,a[2]*t};};
    const auto dot=[](V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    const auto cross=[](V a,V b){return V{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    const auto normal=[&](V a){return scale(a,1/std::sqrt(dot(a,a)));};
    struct Hit {unsigned primitive{};double t{},u{},v{};bool found{},edge{};};
    const auto trace=[&](const std::array<V,12>& vertices,V origin,V direction,double near,double far) {
        Hit hit{};hit.t=far;
        double boundary_t=std::numeric_limits<double>::infinity();
        for(unsigned primitive=0;primitive<4;++primitive) {
            const V a=vertices[primitive*3],ab=sub(vertices[primitive*3+1],a),ac=sub(vertices[primitive*3+2],a);
            const V h=cross(direction,ac),s=sub(origin,a),q=cross(s,ab);
            const double determinant=dot(ab,h);if(std::abs(determinant)<1.e-12) continue;
            const double u=dot(s,h)/determinant,v=dot(direction,q)/determinant,t=dot(ac,q)/determinant;
            // Native intersection rounding can move either side of an edge.
            // Track near-edge misses as well as hits, but never exclude a
            // boundary hidden behind the independently selected interior hit.
            if(t>near && t<far && u>=-.0001 && v>=-.0001 && u+v<=1.0001
                && std::min({std::abs(u),std::abs(v),std::abs(1-u-v)})<.0001)
                boundary_t=std::min(boundary_t,t);
            if(u<0 || v<0 || u+v>1 || t<=near || t>=hit.t) continue;
            hit={primitive,t,u,v,true,false};
        }
        hit.edge=boundary_t<=hit.t+std::max(1.e-9,std::abs(hit.t)*1.e-7);
        return hit;
    };
    {
        std::array<V,12> triangles;
        for(unsigned i=0;i<12;++i) triangles[i]=i%3==0?V{0,0,2}:i%3==1?V{2,0,2}:V{0,2,2};
        const auto boundary_miss=trace(triangles,{},V{-.000016,.5,2},0,4);
        const auto interior=trace(triangles,{},V{.5,.5,2},0,4);
        const auto exterior=trace(triangles,{},V{-.002,.5,2},0,4);
        check(!boundary_miss.found && boundary_miss.edge && interior.found && !interior.edge
            && !exterior.found && !exterior.edge,"Independent ray oracle boundary band hid an interior/exterior sample");
        for(unsigned i=6;i<12;++i) triangles[i]=i%3==0?V{1,1,4}:i%3==1?V{3,1,4}:V{1,3,4};
        const auto hidden_boundary=trace(triangles,{},V{.5,.5,2},0,4);
        check(hidden_boundary.found && !hidden_boundary.edge,"A hidden triangle boundary excluded an interior receiver");
    }
    SdlDxrShadows hardware[2];std::uint64_t checked=0,valid=0,shifted=0,excluded=0;
    if(curved_only) {
        #include "check_reflected_curved_producer.inc"
        return;
    }
    if(secondary_lava_only) {
        #include "check_secondary_lava_producer.inc"
        return;
    }
    if(scene_only) {
        #include "check_reflected_scene_path_producer.inc"
        return;
    }
    if(ordered_only) {
        #include "check_reflected_path_producer.inc"
        return;
    }
    double maximum_pixel_error=0,maximum_depth_error=0;
    for(unsigned variant=0;variant<11;++variant) {
        std::array current{quad(0,0,-2,1.7F,{.3F,.3F,.3F,1}),quad(.94F,.03F,-.6F,.23F,{.8F,.1F,.1F,1})};
        auto accepted=current;
        accepted[0]=quad(0,0,-1.94F,variant==4?.055F:1.7F,{.3F,.3F,.3F,1});
        accepted[1]=quad(variant==5?5.F:.36F,.17F,variant==3?-3.F:-.82F,.23F,{.8F,.1F,.1F,1});
        if(variant==8) for(auto& v:accepted[1]) std::copy_n(accepted[1][0].position,3,v.position);
        if(variant==9) accepted=current; // Explicit rigid matches use the existing source stream.
        std::array<render::CalibratedSceneDraw,2> draws;
        std::array<render::CalibratedSceneMotionDraw,2> matches;
        const float tilt=.12F;
        for(unsigned i=0;i<2;++i) {
            draws[i].vertices=current[i];draws[i].ray_caster=true;
            matches[i].valid=!(variant==1 && i==0) && !(variant==2 && i==1);
            if(matches[i].valid) matches[i].previous_ray_first_triangle=i*2;
            if(matches[i].valid && variant==10) matches[i].previous_ray_first_triangle=(1-i)*2;
            if(matches[i].valid && variant!=9) matches[i].previous_vertices=accepted[i];
            matches[i].previous_model[12]=-.024F;matches[i].previous_model[13]=.018F;
        }
        matches[0].previous_model[0]=matches[0].previous_model[10]=std::cos(tilt);
        matches[0].previous_model[2]=-std::sin(tilt);matches[0].previous_model[8]=std::sin(tilt);
        if(variant==6) {
            matches[0].previous_model[0]*=.88F;matches[0].previous_model[2]*=.88F;
            matches[0].previous_model[8]*=1.13F;matches[0].previous_model[10]*=1.13F;
            matches[0].previous_model[5]=1.07F;
            matches[1].previous_model[0]=.79F;matches[1].previous_model[4]=.12F;matches[1].previous_model[5]=1.12F;
            matches[1].previous_model[12]=.07F;matches[1].previous_model[13]=-.045F;matches[1].previous_model[14]=.035F;
        }
        std::array<XrView,2> eyes{};
        for(auto& eye:eyes) eye.type=XR_TYPE_VIEW;
        auto old_eyes=eyes;
        for(unsigned e=0;e<2;++e) {
            const float yaw=e?.04F:-.03F,old_yaw=e?-.04F:.025F;
            eyes[e].pose.orientation={0,std::sin(yaw/2),0,std::cos(yaw/2)};
            eyes[e].pose.position={e?.034F:-.032F,.015F,.02F};eyes[e].fov={-.57F,.73F,.61F,-.49F};
            old_eyes[e].pose.orientation={0,std::sin(old_yaw/2),0,std::cos(old_yaw/2)};
            old_eyes[e].pose.position={e?.03F:-.028F,-.012F,-.027F};old_eyes[e].fov={-.63F,.68F,.56F,-.54F};
        }
        std::array<render::CalibratedRayGeometryOutput,2> rays;
        std::array<RayReflectionHistory,2> histories;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        for(unsigned e=0;e<2;++e) {
            const auto old=camera_for(old_eyes[e],d3d);
            const render::CalibratedSceneMotion history{old,matches,173,119};
            rays[e]=scene.enqueue_ray_geometry(gpu.command,e,width,height,camera_for(eyes[e],d3d),256,
                variant==9?8U:0U,{.125F,.25F,.5F,1},&history);
            check(rays[e].complete,scene.status().c_str());
            histories[e]={rays[e].previous_vertex_offset,rays[e].previous_extent,rays[e].previous_projection,
                variant==7?600:rays[e].previous_near_plane,rays[e].previous_far_plane.value_or(65536),rays[e].previous_index_offset};
        }
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Reflection fixture source submission lost");
        for(unsigned e=0;e<2;++e) {
            const auto& g=rays[e];const auto& old=histories[e];
            const render::GpuScene::RayGeometryOutput resident{g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
            const Camera camera{width,height,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
            const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
            const std::array<std::uint32_t,256> palette{};
            const bool encoded=hardware[e].render_reflections(gpu.device,camera,resident,palette,0xff306090,0,0,{},g.environment_face_size,
                g.environment_rotation,nullptr,{},0,nullptr,false,range,g.environment_offset,
                g.environment_face_size?1U:0U,false,&old);
            check(encoded,hardware[e].status().c_str());
            const auto output=hardware[e].reflection_output();const auto layout=output.reflection_history;
            check(layout.motion_offset==width*height*4 && layout.storage_bytes==width*height*52
                && layout.identity_offset==width*height*20 && layout.witness_offset==width*height*36
                && layout.extent==old.extent && !output.water_layers.storage_bytes,"Reflection history output lost its explicit layout");
            struct Download {SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;
                ~Download(){if(buffer) SDL_ReleaseGPUTransferBuffer(device,buffer);}};
            const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,layout.storage_bytes,0};
            Download read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&info)};check(read.buffer,SDL_GetError());
            auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
            auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,layout.storage_bytes};
            const SDL_GPUTransferBufferLocation to{read.buffer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
            check(SDL_WaitForGPUFences(gpu.device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(gpu.device,fence);
            const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(bytes,SDL_GetError());
            const auto* motion=reinterpret_cast<const std::array<float,4>*>(bytes+layout.motion_offset);
            const auto* identity=reinterpret_cast<const std::array<unsigned,4>*>(bytes+layout.identity_offset);
            const auto* witness=reinterpret_cast<const std::array<float,4>*>(bytes+layout.witness_offset);
            std::vector<unsigned char> prefix(bytes,bytes+layout.motion_offset);
            std::array<V,12> now{},before{};
            for(unsigned draw=0;draw<2;++draw) for(unsigned v=0;v<6;++v) {
                const auto a=world_eye(eyes[e],model_point(draws[draw].model,current[draw][v]));
                const auto b=world_eye(old_eyes[e],model_point(matches[draw].previous_model,accepted[draw][v]));
                now[draw*6+v]={a[0]*256,-a[1]*256,-a[2]*256};before[draw*6+v]={b[0]*256,-b[1]*256,-b[2]*256};
            }
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const V direction=normal({(x+.5-g.projection[2])/g.projection[0],(y+.5-g.projection[3])/g.projection[1],1});
                const auto primary=trace(now,{},direction,g.near_plane/direction[2],g.far_plane.value_or(65536)/direction[2]);
                std::array<double,4> expected{};bool ambiguous=primary.edge;std::string diagnostic;
                std::array<unsigned,4> expected_ids{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
                std::array<double,4> expected_witness{};
                if(primary.found) {
                    const unsigned p=primary.primitive*3;
                    V n=normal(cross(sub(now[p+1],now[p]),sub(now[p+2],now[p])));
                    if(dot(n,direction)>0) n=scale(n,-1);
                    const double bias=std::max(.01,primary.t*1.e-5);
                    const V reflection=sub(direction,scale(n,2*dot(n,direction)));
                    const auto secondary=trace(now,add(scale(direction,primary.t),scale(n,bias)),reflection,bias,65536);
                    diagnostic=" secondary="+std::to_string(secondary.found)+":"+std::to_string(secondary.primitive)
                        +" bary="+std::to_string(secondary.u)+","+std::to_string(secondary.v);
                    ambiguous=ambiguous || secondary.edge;
                    if(secondary.found) {
                        expected_ids[0]=primary.primitive;expected_ids[1]=secondary.primitive;
                        if(matches[primary.primitive/2].valid)
                            expected_ids[2]=matches[primary.primitive/2].previous_ray_first_triangle+primary.primitive%2;
                        if(matches[secondary.primitive/2].valid && variant!=8)
                            expected_ids[3]=matches[secondary.primitive/2].previous_ray_first_triangle+secondary.primitive%2;
                        expected_witness={secondary.u,secondary.v,primary.t*direction[2],1};
                    }
                    if(secondary.found && matches[primary.primitive/2].valid && matches[secondary.primitive/2].valid) {
                        const unsigned q=secondary.primitive*3;
                        const V qa=before[q],qb=before[q+1],qc=before[q+2],a=before[p],ab=sub(before[p+1],a),ac=sub(before[p+2],a);
                        const V raw=cross(ab,ac),rawQ=cross(sub(qb,qa),sub(qc,qa));
                        if(dot(raw,raw)>1.e-20 && dot(rawQ,rawQ)>1.e-20) {
                            const V oldNormal=normal(raw);
                            const V feature=add(add(scale(qa,1-secondary.u-secondary.v),scale(qb,secondary.u)),scale(qc,secondary.v));
                            const V virtualFeature=sub(feature,scale(oldNormal,2*dot(sub(feature,a),oldNormal)));
                            const double denominator=dot(virtualFeature,oldNormal);
                            const double fraction=denominator!=0?dot(a,oldNormal)/denominator:0;
                            const V receiver=scale(virtualFeature,fraction);
                            std::array<V,12> finiteFace;
                            for(unsigned i=0;i<12;++i) finiteFace[i]=before[p+i%3];
                            const auto inside=trace(finiteFace,{},virtualFeature,0,1.0001);
                            // Ray/triangle membership is independent of the
                            // shader's Gram-matrix barycentric calculation.
                            const bool sameFace=inside.found;
                            const double px=old.projection[0]*virtualFeature[0]/virtualFeature[2]+old.projection[2];
                            const double py=old.projection[1]*virtualFeature[1]/virtualFeature[2]+old.projection[3];
                            diagnostic+=" oldpixel="+std::to_string(px)+","+std::to_string(py)+" fraction="+std::to_string(fraction)
                                +" depth="+std::to_string(receiver[2])+" inside="+std::to_string(inside.found)+" bary="
                                +std::to_string(inside.u)+","+std::to_string(inside.v);
                            if(virtualFeature[2]>0 && fraction>0 && fraction<1 && receiver[2]>=old.near_plane
                                && receiver[2]<=old.far_plane && sameFace && px>=0 && py>=0 && px<old.extent[0] && py<old.extent[1])
                                expected={px,py,receiver[2],1};
                            ambiguous=ambiguous || inside.edge;
                        }
                    }
                }
                if(ambiguous) {++excluded;continue;} // Explicit native boundary-rounding exclusion, never an interior mismatch.
                check(identity[y*width+x]==expected_ids,"Native reflected-hit primitive correspondence mismatch");
                for(unsigned c=0;c<4;++c) check(std::isfinite(witness[y*width+x][c])
                    && std::abs(witness[y*width+x][c]-expected_witness[c])<(c==2?.01:.00001),
                    "Native reflected-hit visibility/barycentric witness mismatch");
                const auto& actual=motion[y*width+x];
                if(actual[3]!=expected[3]) throw std::runtime_error("Reflected motion validity mismatch: variant="+std::to_string(variant)
                    +" eye="+std::to_string(e)+" x="+std::to_string(x)+" y="+std::to_string(y)
                    +" actual="+std::to_string(actual[0])+","+std::to_string(actual[1])+","+std::to_string(actual[2])+","+std::to_string(actual[3])
                    +" expected="+std::to_string(expected[0])+","+std::to_string(expected[1])+","+std::to_string(expected[2])+","+std::to_string(expected[3])
                    +" primary="+std::to_string(primary.primitive)+diagnostic);
                for(unsigned c=0;c<3;++c) {
                    const double error=std::abs(actual[c]-expected[c]);
                    if(!std::isfinite(actual[c]) || error>(c==2?.01:.02)) throw std::runtime_error("Reflected hit motion oracle mismatch: variant="
                        +std::to_string(variant)+" eye="+std::to_string(e)+" x="+std::to_string(x)+" y="+std::to_string(y)
                        +" axis="+std::to_string(c)+" actual="+std::to_string(actual[c])+" expected="+std::to_string(expected[c]));
                    if(c==2) maximum_depth_error=std::max(maximum_depth_error,error);else maximum_pixel_error=std::max(maximum_pixel_error,error);
                }
                ++checked;if(expected[3]) {++valid;if(std::hypot(expected[0]-(x+.5)*old.extent[0]/width,expected[1]-(y+.5)*old.extent[1]/height)>2) ++shifted;}
            }
            SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
            check(hardware[e].render_reflections(gpu.device,camera,resident,palette,0xff306090,0,0,{},g.environment_face_size,
                g.environment_rotation,nullptr,{},0,nullptr,false,range,g.environment_offset,
                g.environment_face_size?1U:0U,false),hardware[e].status().c_str());
            check(!hardware[e].reflection_output().reflection_history.storage_bytes,"Reflection history metadata leaked into OFF");
            std::vector<unsigned char> off;check(hardware[e].readback(off),hardware[e].status().c_str());
            check(prefix==off,"Secondary history changed current reflected RGBA bytes");
            // Refuse malformed/unsupported requests without publishing stale output.
            for(unsigned bad=0;bad<6;++bad) {
                auto invalid=old;
                if(bad==0) invalid.previous_vertex_offset=1;
                if(bad==1) invalid.extent[0]=0;
                if(bad==2) invalid.projection[0]=std::numeric_limits<double>::quiet_NaN();
                if(bad==3) invalid.previous_vertex_offset=g.material_offset;
                const bool rejected=!hardware[e].render_reflections(gpu.device,camera,resident,palette,0xff306090,bad==4?.1F:0,0,{},g.environment_face_size,
                    g.environment_rotation,nullptr,{},0,nullptr,false,range,g.environment_offset,
                    g.environment_face_size?1U:0U,bad==5,&invalid);
                check(rejected && !hardware[e].reflection_output().buffer,"Malformed/rough/multibounce reflected history was published");
            }
        }
    }
    #include "check_reflected_ground_motion.inc"
    #include "check_reflected_liquid_producer.inc"
    #include "check_reflected_lobe_producer.inc"
    if(valid<=1000 || shifted<=1000) throw std::runtime_error("Reflected-hit fixture coverage insufficient: valid="
        +std::to_string(valid)+" shifted="+std::to_string(shifted)+" checked="+std::to_string(checked));
    check(excluded && excluded*100<checked,"Native boundary exclusion consumed too much independent coverage");
    std::cout<<"Native reflected-hit motion: "<<checked<<" physical ray/plane pixel checks, "<<valid<<" accepted finite hits, "<<shifted
        <<" shifted features; maximum pixel/depth errors "<<maximum_pixel_error<<"/"<<maximum_depth_error
        <<"; "<<excluded<<" explicitly bounded edge samples; two eyes, moving/affine mirror and hit, old FOV/extent, rigid source reuse, resident environment cube, unmatched/off-face/behind/clip/degenerate rejection, exact OFF RGBA, malformed recovery passed. Temporal consumers/liquids/multibounce remain separate.\n";
}
