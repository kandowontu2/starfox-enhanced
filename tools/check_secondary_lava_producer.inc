// Actual native MODEL -> animated LAVA producer. Independent binary64 source
// intersections select every hop. RGB deliberately shares the scalar surface
// recipe, not GPU geometry/colour readback; this is not a neural/filter oracle.
{
    namespace lo=reflected_liquid_oracle;
    constexpr unsigned pw=96,ph=72;
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    const bool srgb=!indexed_lava && (gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
        || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB);
    const auto decode=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    const auto encode=[](double c){return c<=.0031308?c*12.92:1.055*std::pow(c,1/2.4)-.055;};
    const auto linear=[&](std::uint32_t word) {
        V result;for(unsigned c=0;c<3;++c) {result[c]=double((word>>(c*8))&255)/255.;
            if(srgb) result[c]=decode(result[c]);else if(indexed_lava) result[c]*=result[c];}return result;
    };
    const auto pack=[&](V rgb) {
        std::uint32_t word=0xff000000;
        for(unsigned c=0;c<3;++c) {double value=std::clamp(rgb[c],0.,1.);if(srgb) value=encode(value);else if(indexed_lava) value=std::sqrt(value);
            word|=std::uint32_t(value*255+.5)<<(c*8);}return word;
    };
    unsigned positive=0,ground_hops=0,model_hops=0,loop_paths=0,boundaries=0,time_changes=0,sentinel_pairs=0;
    unsigned max_error=0;std::array<unsigned,5> hop_counts{};
    // No source texture readback or pre-packed reflection/history records.
    for(unsigned variant=0;variant<3;++variant) {
        std::cout<<"Secondary lava indexed="<<indexed_lava<<" native format="<<gpu.format<<" variant="<<variant<<std::endl;
        std::array vertices{quad(0,0,-2,1.1F,{.3F,.3F,.3F,1}),quad(.6F,.5F,-.55F,.24F,{.8F,.1F,.1F,1})};
        for(auto& v:vertices[0]) v.position[2]+=.7F*v.position[1]+.08F*v.position[0];
        std::array<render::CalibratedSceneDraw,2> draws;
        for(unsigned i=0;i<2;++i) {draws[i].vertices=vertices[i];draws[i].ray_caster=true;}
        std::array<XrView,2> eyes{};
        for(unsigned eye=0;eye<2;++eye) {auto& e=eyes[eye];e.type=XR_TYPE_VIEW;e.pose.orientation.w=1;
            e.pose.position={eye?.034F:-.032F,.015F,.02F};e.fov={-.57F,.73F,.61F,-.49F};}
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        std::array<render::CalibratedRayGeometryOutput,2> rays;
        for(unsigned eye=0;eye<2;++eye) {
            rays[eye]=scene.enqueue_ray_geometry(gpu.command,eye,pw,ph,camera_for(eyes[eye],d3d),256,0,{});
            check(rays[eye].complete,scene.status().c_str());
        }
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Secondary lava geometry lost submission ownership");
        for(unsigned eye=0;eye<2;++eye) {
            const auto& g=rays[eye];render::GpuScene::RayGeometryOutput resident{
                g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
            render::RayMaterials indexed;
            if(indexed_lava) {
                indexed.triangles.resize(4);
                for(unsigned p=0;p<4;++p) indexed.triangles[p].even=indexed.triangles[p].odd=p<2?1:2;
                // Same genuine resident GPU source positions, with the regular
                // game's supported CPU palette metadata, never native RGBA.
                resident.materials=&indexed;resident.material_offset=resident.material_bytes=0;
            }
            const Camera camera{pw,ph,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
            const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
            std::array<std::uint32_t,256> palette{};palette[1]=0xff4d4d4d;palette[2]=0xff1a1acc;
            const float angle=variant==2?.13F:0.F;
            const V floor_point{0,180,0},floor_normal{double(std::sin(angle)),-double(std::cos(angle)),0};
            const ReceiverPlane floor{{floor_point[0],floor_point[1],floor_point[2]},
                {floor_normal[0],floor_normal[1],floor_normal[2]}};
            std::array<V,12> positions;
            for(unsigned draw=0;draw<2;++draw) for(unsigned v=0;v<6;++v) {
                const auto p=world_eye(eyes[eye],model_point(draws[draw].model,vertices[draw][v]));
                positions[draw*6+v]={p[0]*256,-p[1]*256,-p[2]*256};
            }
            const auto floor_distance=[&](V origin,V direction,double near,double far) {
                const double denominator=dot(direction,floor_normal);if(std::abs(denominator)<=1.e-8) return far;
                const double t=dot(sub(floor_point,origin),floor_normal)/denominator;return t>near && t<far?t:far;
            };
            const float rough=variant==2?.25F:0,reflection_strength=variant==0?0.F:.8F;
            const unsigned metallic=variant==1?2:0,lobes=rough>0?8:1;
            std::array<std::vector<unsigned char>,2> first;
            for(unsigned phase=0;phase<2;++phase) for(unsigned sentinel=0;sentinel<2;++sentinel) {
                const unsigned environment=sentinel?0xff29b50dU:0xffa035e0U;
                RayWater water;water.material=3;water.time=phase?4.7F:1.3F;water.reflection_strength=reflection_strength;
                water.brightness=.72F;water.camera_position={29,-3,17};water.mirror_models=true;
                water.world_to_view={std::cos(angle),-std::sin(angle),0,std::sin(angle),std::cos(angle),0,0,0,1};
                check(hardware[eye].render_reflections(gpu.device,camera,resident,palette,environment,rough,metallic,{},0,
                    g.environment_rotation,nullptr,floor,0,&water,false,range,0,indexed_lava?0:srgb?2:1,variant==1),hardware[eye].status().c_str());
                std::vector<unsigned char> bytes;check(hardware[eye].readback(bytes),hardware[eye].status().c_str());
                check(bytes.size()==pw*ph*4,"Secondary lava image has wrong extent");
                lo::Frame frame;frame.point=floor_point;frame.normal=floor_normal;
                std::copy(water.world_to_view.begin(),water.world_to_view.end(),frame.rotation.begin());
                std::copy(water.camera_position.begin(),water.camera_position.end(),frame.offset.begin());
                frame.time=water.time;frame.material=3;frame.projection=g.projection;
                for(unsigned y=0;y<ph;++y) for(unsigned x=0;x<pw;++x) {
                    const unsigned index=y*pw+x;
                    const V direction=normal({(x+.5-g.projection[2])/g.projection[0],(y+.5-g.projection[3])/g.projection[1],1});
                    const double near=g.near_plane/direction[2],far=g.far_plane.value_or(65536)/direction[2];
                    const auto primary=trace(positions,{},direction,near,floor_distance({},direction,near,far));
                    if(primary.edge) {++boundaries;continue;}
                    if(!primary.found) continue;
                    const unsigned p=primary.primitive*3;V n=normal(cross(sub(positions[p+1],positions[p]),sub(positions[p+2],positions[p])));
                    if(dot(n,direction)>0) n=scale(n,-1);
                    const V reflected=lo::reflect(direction,n),tangent=normal(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0}));
                    const V bitangent=cross(reflected,tangent);
                    const double initial_bias=std::max(.01,primary.t*1.e-5);
                    const V first_origin=add(scale(direction,primary.t),scale(n,initial_bias));
                    V sum{};bool boundary=false,has_lava=false,repeated_lava=false;unsigned path_lava=0,path_models=0;
                    for(unsigned lobe=0;lobe<lobes;++lobe) {
                        V outgoing=rough>0?normal(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),double(rough)*rough))):reflected;
                        if(dot(outgoing,n)<=0) outgoing=reflected;
                        V origin=first_origin,throughput{1,1,1},accumulated{};double lower=initial_bias;
                        unsigned hops=0,lobe_lava=0;
                        for(unsigned bounce=0;bounce<4;++bounce) {
                            const double ground_t=floor_distance(origin,outgoing,lower,65536);
                            const auto hit=trace(positions,origin,outgoing,lower,ground_t);
                            if(hit.edge) {boundary=true;break;}
                            if(!hit.found && ground_t==65536) break;
                            ++hops;
                            if(hit.found) {
                                ++path_models;const unsigned hp=hit.primitive*3;
                                V hn=normal(cross(sub(positions[hp+1],positions[hp]),sub(positions[hp+2],positions[hp])));
                                if(dot(hn,outgoing)>0) hn=scale(hn,-1);
                                if(metallic) {const V f0{1,.766,.336};const double grazing=std::pow(1-std::clamp(dot(scale(outgoing,-1),hn),0.,1.),5);
                                    for(unsigned c=0;c<3;++c) throughput[c]*=f0[c]+(1-f0[c])*grazing;}
                                lower=std::max(.05,hit.t*1.e-5);origin=add(add(origin,scale(outgoing,hit.t)),scale(hn,lower));
                                outgoing=lo::reflect(outgoing,hn);continue;
                            }
                            ++path_lava;++lobe_lava;has_lava=true;repeated_lava|=lobe_lava>1;
                            V hp=add(origin,scale(outgoing,ground_t)),world=add(lo::row(hp,frame),frame.offset);
                            const double footprint=std::sqrt(dot(hp,hp))/std::max(g.projection[0],1.)/std::max(std::abs(dot(outgoing,floor_normal)),.04);
                            const auto geometry=lo::lava(world[0],world[2],water.time,footprint);const V travel=lo::row(outgoing,frame);
                            const double shift=std::clamp(-geometry[0]/std::max(travel[1],.12),-ground_t*.2,ground_t*.2);
                            hp=add(hp,scale(outgoing,shift));world=add(world,scale(travel,shift));
                            const auto waved=lo::lava(world[0],world[2],water.time,footprint);
                            V hn=normal(lo::col({waved[1],-1,waved[2]},frame));if(dot(hn,outgoing)>0) hn=scale(hn,-1);
                            const auto surface=render::lava_surface(float(world[0]),float(world[2]),water.time,float(footprint));
                            const V viewer=lo::row(scale(outgoing,-1),frame);const auto colour=render::lava_shade(surface,float(viewer[0]),float(viewer[1]),float(viewer[2]));
                            V molten{double(colour.r*water.brightness),double(colour.g*water.brightness),double(colour.b*water.brightness)};
                            for(unsigned c=0;c<3;++c) {if(srgb) molten[c]=decode(molten[c]);else if(indexed_lava) molten[c]*=molten[c];
                                accumulated[c]+=throughput[c]*molten[c];}
                            const double share=(.035+.40*std::pow(1-std::clamp(dot(scale(outgoing,-1),hn),0.,1.),5))
                                *water.reflection_strength*(.3+.7*surface.crust);
                            throughput=scale(throughput,share);
                            if(*std::max_element(throughput.begin(),throughput.end())<1.e-5) {throughput={};break;}
                            lower=std::max(.05,ground_t*1.e-5);origin=add(hp,scale(hn,lower));outgoing=lo::reflect(outgoing,hn);
                        }
                        if(boundary) break;
                        ++hop_counts[hops];
                        const V env=linear(environment);for(unsigned c=0;c<3;++c) accumulated[c]+=throughput[c]*env[c];
                        // Every native reflected path packs once before the rough-lobe mean.
                        sum=add(sum,linear(pack(accumulated)));
                    }
                    if(boundary) {++boundaries;continue;}
                    if(!has_lava) continue;
                    ground_hops+=path_lava;model_hops+=path_models;loop_paths+=repeated_lava;
                    sum=scale(sum,1./lobes);
                    if(metallic) {const V f0{1,.766,.336};const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),n),0.,1.),5);
                        for(unsigned c=0;c<3;++c) sum[c]*=f0[c]+(1-f0[c])*grazing;}
                    const unsigned expected=pack(sum);check(bytes[index*4+3]==255,"Model -> lava reflection changed opaque model alpha");
                    for(unsigned c=0;c<3;++c) {
                        const auto error=unsigned(std::abs(int(bytes[index*4+c])-int((expected>>(c*8))&255)));max_error=std::max(max_error,error);
                        if(error>2) throw std::runtime_error("Secondary lava is not the current animated enhanced surface: variant="+std::to_string(variant)
                            +" eye="+std::to_string(eye)+" phase="+std::to_string(phase)+" pixel="+std::to_string(index)+" channel="+std::to_string(c)
                            +" actual/expected="+std::to_string(bytes[index*4+c])+"/"+std::to_string((expected>>(c*8))&255));
                    }
                    if(variant==0 && sentinel==1) {for(unsigned c=0;c<4;++c) check(bytes[index*4+c]==first[phase][index*4+c],
                        "Emission-only enhanced lava retained original flat ground colour");++sentinel_pairs;}
                    if(sentinel==0 && phase==1) for(unsigned c=0;c<3;++c) time_changes+=std::abs(int(bytes[index*4+c])-int(first[0][index*4+c]))>2;
                    ++positive;
                }
                if(sentinel==0) first[phase]=bytes;
            }
        }
    }
    check(positive>10000 && ground_hops>10000 && model_hops>100 && time_changes>100 && sentinel_pairs>1000,
        "Secondary lava fixture lacks actual animated/model/ground positive coverage");
    std::cout<<"Native secondary lava indexed="<<indexed_lava<<" format="<<gpu.format<<": "<<positive<<" independent source-geometry samples; ground/model hops "
        <<ground_hops<<'/'<<model_hops<<"; repeated-lava paths "<<loop_paths<<"; animated RGB changes "<<time_changes
        <<"; original-floor-independent pairs "<<sentinel_pairs<<"; max RGB error "<<max_error<<"; boundaries "<<boundaries
        <<"; bounded hop classes ";for(unsigned count:hop_counts) std::cout<<count<<' ';
    std::cout<<". Actual hardware intersections and shared scalar RGB recipe; not cached history, neural filtering or native Linux/Metal acceptance."<<std::endl;
}
