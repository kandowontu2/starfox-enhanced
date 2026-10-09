// Actual native MODEL and full WATER/LAVA receiver witnesses. Every path is
// independently traced from authored inputs in binary64. The water/lava RGB
// recipe is intentionally shared, not reconstructed from GPU colour or hits.
{
    namespace lo=reflected_liquid_oracle;
    namespace nf=native_reflection_fixture;
    struct Record {std::array<unsigned,4> mirrors;unsigned control,terminal;
        std::array<float,3> feature;unsigned incoming;std::array<float,3> response,base;};
    static_assert(sizeof(Record)==64);
    constexpr unsigned pw=96,ph=72,count=pw*ph;
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    const unsigned encoding=(gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
        || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)?2:1;
    const auto linear=[&](unsigned word){return nf::native_linear(word,encoding);};
    const auto pack=[&](V value){return nf::native_pack(value,encoding,255);};
    const auto download=[&](const GpuReflectionOutput& output,unsigned size) {
        struct Download {SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;
            ~Download(){if(buffer)SDL_ReleaseGPUTransferBuffer(device,buffer);}};
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,size,0};
        Download read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&info)};check(read.buffer,SDL_GetError());
        auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
        const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,size};
        const SDL_GPUTransferBufferLocation to{read.buffer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
        check(SDL_WaitForGPUFences(gpu.device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(gpu.device,fence);
        const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(bytes,SDL_GetError());
        std::vector<unsigned char> storage(bytes,bytes+size);SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);return storage;
    };
    std::array<unsigned,5> hop_counts{},kinds{};
    std::array<unsigned,2> liquid_hops{},animated{};
    std::uint64_t samples=0,edges=0,model_hops=0,empty=0,exact_prefixes=0;
    std::array<std::uint64_t,2> liquid_primaries{};
    std::uint64_t canonical_prefix_bytes=0,visible_guides=0,hidden_guides=0;
    std::uint64_t current_caustic_changes=0;
    double max_feature=0,max_response=0,max_base=0;unsigned max_rgb=0;
    // 0 retains the accepted MODEL-only mode; 1 full receivers; 2 visible
    // surface-only guides; 3 canonical hidden-world + visible surface planes.
    for(unsigned receiver=0;receiver<4;++receiver)
    for(unsigned material:{0U,3U})for(unsigned variant=0;variant<3;++variant) {
        if(material==3 && receiver>1) continue;
        std::cout<<"Curved producer format="<<gpu.format<<" receivers="<<receiver<<" liquid="<<material<<" variant="<<variant<<std::endl;
        // Author exact encoded byte centres in the negotiated LINEAR domain.
        // .3*255 is a halfway round-to-even case; for sRGB, .3 linear also
        // denotes a different encoded colour. Neither is an exact 0x4d input.
        const auto gray=linear(0xff4d4d4dU),red=linear(0xff1a1accU);
        std::array vertices{quad(0,0,-2,1.1F,{float(gray[0]),float(gray[1]),float(gray[2]),1}),
            quad(.6F,.5F,-.55F,.24F,{float(red[0]),float(red[1]),float(red[2]),1})};
        for(auto& v:vertices[0])v.position[2]+=.7F*v.position[1]+.08F*v.position[0];
        std::array<render::CalibratedSceneDraw,2> draws;
        std::array<render::CalibratedSceneMotionDraw,2> matches;
        for(unsigned i=0;i<2;++i) {draws[i].vertices=vertices[i];draws[i].ray_caster=true;
            matches[i].previous_model=identity;matches[i].previous_vertices=vertices[i];
            matches[i].valid=true;matches[i].previous_ray_first_triangle=2*i;}
        std::array<XrView,2> eyes{};
        for(unsigned e=0;e<2;++e) {eyes[e].type=XR_TYPE_VIEW;eyes[e].pose.orientation.w=1;
            eyes[e].pose.position={e?.034F:-.032F,.015F,.02F};eyes[e].fov={-.57F,.73F,.61F,-.49F};}
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        std::array<render::CalibratedRayGeometryOutput,2> rays;
        for(unsigned e=0;e<2;++e) {const auto camera=camera_for(eyes[e],d3d);
            const render::CalibratedSceneMotion previous{camera,matches,pw,ph};
            rays[e]=scene.enqueue_ray_geometry(gpu.command,e,pw,ph,camera,256,0,{},&previous);
            check(rays[e].complete,scene.status().c_str());}
        check(SDL_SubmitGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        check(scene.notify_submitted(token),"Curved source lost submission ownership");
        const float angle=variant==2?.13F:0,rough=variant==2?.25F:0;
        const unsigned lobes=rough>0?8:1,metal=variant==1?2:variant==2?3:0;
        const bool transport=variant!=0;
        for(unsigned e=0;e<2;++e) {
            const auto& g=rays[e];const render::GpuScene::RayGeometryOutput resident{
                g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
            const Camera camera{pw,ph,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
            const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
            const std::array<std::uint32_t,256> palette{};
            const V floor_point{0,180,0},floor_normal{double(std::sin(angle)),-double(std::cos(angle)),0};
            const ReceiverPlane floor{{floor_point[0],floor_point[1],floor_point[2]},{floor_normal[0],floor_normal[1],floor_normal[2]}};
            std::array<V,12> positions;nf::NativeReflectedSource colour_source{};
            for(unsigned draw=0;draw<2;++draw)for(unsigned v=0;v<6;++v) {
                const auto p=world_eye(eyes[e],model_point(draws[draw].model,vertices[draw][v]));
                const unsigned i=draw*6+v;positions[i]={p[0]*256,-p[1]*256,-p[2]*256};
                // Match the input stream's float storage, not its GPU output.
                for(unsigned c=0;c<3;++c)positions[i][c]=float(positions[i][c]);
                colour_source.source.geometry.vertices[i]={float(positions[i][0]),float(positions[i][1]),float(positions[i][2]),1};
            }
            for(unsigned p=0;p<4;++p)colour_source.solid[p].fill(p<2?0xff4d4d4dU:0xff1a1accU);
            const auto floor_distance=[&](V origin,V direction,double near,double far) {
                const double d=dot(direction,floor_normal);if(std::abs(d)<=1.e-8)return far;
                const double t=dot(sub(floor_point,origin),floor_normal)/d;return t>near && t<far?t:far;
            };
            const auto f0=[&](unsigned primitive) {return metal==2?V{1,.766,.336}:metal==3?V{.955,.638,.538}:
                linear(primitive<2?0xff4d4d4dU:0xff1a1accU);};
            std::array<std::vector<unsigned char>,2> previous_prefix;
            std::vector<unsigned char> plain_prefix;
            for(unsigned phase=0;phase<2;++phase)for(float strength:{0.F,.8F})for(unsigned caustics:{0U,3U}) {
                if(material==3 && caustics)continue;
                const unsigned environment=0xff29b50dU;
                RayWater water;water.material=material;water.time=phase?4.7F:1.3F;water.reflection_strength=strength;
                water.brightness=.72F;water.camera_position={29,-3,17};water.mirror_models=transport;
                water.caustics=caustics;
                water.world_to_view={std::cos(angle),-std::sin(angle),0,std::sin(angle),std::cos(angle),0,0,0,1};
                if(material==0)water.source_colour=std::array<float,3>{.2F,.3F,.4F};
                water.surface_layers=receiver==2;water.auxiliary_layers=receiver==3;
                RayReflectionHistory previous{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                    g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
                previous.model_lobes=lobes;previous.model_paths=true;previous.curved_paths=true;
                previous.curved_receivers=receiver!=0;
                previous.previous_liquid=RayReflectionLiquid{{floor_point,floor_normal},{},{},water.time,material};
                std::copy(water.world_to_view.begin(),water.world_to_view.end(),previous.previous_liquid->world_to_view.begin());
                std::copy(water.camera_position.begin(),water.camera_position.end(),previous.previous_liquid->offset.begin());
                check(hardware[e].render_reflections(gpu.device,camera,resident,palette,environment,rough,metal,{},0,
                    g.environment_rotation,nullptr,floor,0,&water,false,range,0,encoding,transport,&previous),hardware[e].status().c_str());
                const auto output=hardware[e].reflection_output();const auto layout=output.reflection_history;
                const auto water_layers=receiver>1?*native_water_layers(pw,ph,receiver==3):NativeWaterLayers{};
                check(layout==*native_reflection_history(pw,ph,{pw,ph},true,water_layers,lobes,true,false,true,receiver!=0)
                    && output.water_layers==water_layers,"Native curved record aliases a different ABI");
                auto storage=download(output,layout.storage_bytes);
                const auto* bytes=storage.data();const auto* ids=reinterpret_cast<const unsigned*>(bytes+layout.identity_offset);
                const auto* depths=reinterpret_cast<const float*>(bytes+layout.witness_offset);
                const auto* primary_response=reinterpret_cast<const std::array<float,4>*>(bytes+layout.weight_offset);
                const auto* primary_base=layout.base_offset?reinterpret_cast<const std::array<float,4>*>(bytes+layout.base_offset):nullptr;
                const auto* records=reinterpret_cast<const Record*>(bytes+layout.incoming_offset);
                std::vector<unsigned char> prefix(bytes,bytes+count*4);
                lo::Frame frame;frame.point=floor_point;frame.normal=floor_normal;frame.time=water.time;frame.material=material;frame.projection=g.projection;
                std::copy(water.world_to_view.begin(),water.world_to_view.end(),frame.rotation.begin());
                std::copy(water.camera_position.begin(),water.camera_position.end(),frame.offset.begin());
                nf::Parameters wp{};wp.camera[0]=float(g.projection[0]);wp.material_info[2]=encoding;
                wp.water={water.time,strength,water.brightness,float(material|(transport?16:0)|(caustics<<5))};
                wp.source_colour={.2F,.3F,.4F,1};
                struct LiquidSample {V hit{},normal{},base{};double share{},bias{};bool edge{};};
                const auto sample_liquid=[&](V origin,V outgoing,double distance,unsigned x,unsigned y,bool primary=false) {
                    LiquidSample sample;sample.hit=add(origin,scale(outgoing,distance));sample.bias=std::max(.05,distance*1.e-5);
                    if(material==0) {
                        const auto expected=nf::native_water_expected(colour_source,wp,frame,origin,outgoing,distance,x,y);
                        sample.normal=expected.normal;sample.base=expected.light;sample.share=expected.fresnel;sample.edge=expected.edge;
                    } else {
                        V world=add(lo::row(sample.hit,frame),frame.offset);
                        const double footprint=(primary?distance:std::sqrt(dot(sample.hit,sample.hit)))/std::max(g.projection[0],1.)
                            /std::max(std::abs(dot(outgoing,floor_normal)),.04);
                        const auto geometry=lo::lava(world[0],world[2],water.time,footprint);const V travel=lo::row(outgoing,frame);
                        const double shift=std::clamp(-geometry[0]/std::max(travel[1],.12),-distance*.2,distance*.2);
                        sample.hit=add(sample.hit,scale(outgoing,shift));world=add(world,scale(travel,shift));
                        const auto wave=lo::lava(world[0],world[2],water.time,footprint);
                        sample.normal=normal(lo::col({wave[1],-1,wave[2]},frame));if(dot(sample.normal,outgoing)>0)sample.normal=scale(sample.normal,-1);
                        const auto surface=render::lava_surface(float(world[0]),float(world[2]),water.time,float(footprint));const V viewer=lo::row(scale(outgoing,-1),frame);
                        const auto colour=render::lava_shade(surface,float(viewer[0]),float(viewer[1]),float(viewer[2]));
                        sample.base={double(colour.r*water.brightness),double(colour.g*water.brightness),double(colour.b*water.brightness)};
                        if(encoding==2)for(auto& c:sample.base)c=c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);
                        sample.share=(.035+.40*std::pow(1-std::clamp(dot(scale(outgoing,-1),sample.normal),0.,1.),5))*strength*(.3+.7*surface.crust);
                    }
                    return sample;
                };
                for(unsigned y=0;y<ph;++y)for(unsigned x=0;x<pw;++x) {
                    const unsigned index=y*pw+x;const V direction=normal({(x+.5-g.projection[2])/g.projection[0],(y+.5-g.projection[3])/g.projection[1],1});
                    const double near=g.near_plane/direction[2],far=g.far_plane.value_or(65536)/direction[2];
                    const double floor_t=floor_distance({},direction,near,far);
                    const auto primary=trace(positions,{},direction,near,floor_t);
                    const bool liquid_primary=receiver && !primary.found && floor_t<far;
                    if(primary.edge){edges+=lobes;continue;}
                    check(ids[index]==(primary.found?primary.primitive:liquid_primary?0xfffffffdU:UINT32_MAX),"Curved nearer primary ownership mismatch");
                    if(receiver>1 && !liquid_primary) {
                        const auto* surface=reinterpret_cast<const std::array<float,4>*>(bytes+water_layers.surface_offset);
                        check(surface[index]==std::array<float,4>{},"Dry MODEL/sky inherited visible liquid guide");++hidden_guides;
                    }
                    if(!primary.found && !liquid_primary) {
                        for(unsigned c=0;c<4;++c)check(prefix[index*4+c]==0,"Curved missing MODEL receiver retained RGBA");
                        check(depths[index]==0 && primary_response[index]==std::array<float,4>{},"Curved miss retained current primary response");
                        if(primary_base)check(primary_base[index]==std::array<float,4>{},"Curved miss retained primary base radiance");
                        for(unsigned lobe=0;lobe<lobes;++lobe) {const auto& r=records[index*lobes+lobe];
                            check(r.mirrors==std::array<unsigned,4>{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}
                                && !r.control && r.terminal==UINT32_MAX && r.feature==std::array<float,3>{}
                                && !r.incoming && r.response==std::array<float,3>{} && r.base==std::array<float,3>{},"Curved miss retained ordered light/history");++empty;}
                        continue;
                    }
                    const double primary_distance=liquid_primary?floor_t:primary.t;
                    check(std::abs(depths[index]-primary_distance*direction[2])<.002,"Curved current primary depth mismatch");
                    V n{},base_primary{};double bias=std::max(.01,primary_distance*1.e-5);V first_origin{},primary_weight{1,1,1};
                    if(liquid_primary) {
                        const auto sample=sample_liquid({},direction,floor_t,x,y,true);
                        if(sample.edge){edges+=lobes;continue;}
                        n=sample.normal;bias=sample.bias;base_primary=sample.base;primary_weight.fill(sample.share);
                        first_origin=add(sample.hit,scale(n,bias));++liquid_primaries[material==3];
                        if(receiver>1) {
                            const auto* surface=reinterpret_cast<const std::array<float,4>*>(bytes+water_layers.surface_offset);
                            const V observed{surface[index][0],surface[index][1],surface[index][2]};
                            check(std::sqrt(dot(sub(n,observed),sub(n,observed)))<2.e-5
                                && std::abs(surface[index][3]-floor_t*direction[2])<.002,"Visible canonical water guide changed optical geometry");++visible_guides;
                        }
                    } else {
                        const unsigned p=primary.primitive*3;n=normal(cross(sub(positions[p+1],positions[p]),sub(positions[p+2],positions[p])));
                        if(dot(n,direction)>0)n=scale(n,-1);
                        first_origin=add(scale(direction,primary.t),scale(n,bias));
                        if(metal) {const auto tint=f0(primary.primitive);const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),n),0.,1.),5);
                            for(unsigned c=0;c<3;++c)primary_weight[c]=tint[c]+(1-tint[c])*grazing;}
                    }
                    const V reflected=lo::reflect(direction,n),tangent=normal(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
                    for(unsigned c=0;c<3;++c)check(std::abs(primary_response[index][c]-primary_weight[c])<2.e-5,"Curved current primary material mismatch");
                    check(primary_response[index][3]==1,"Curved primary validity mismatch");
                    if(primary_base) {
                        for(unsigned c=0;c<3;++c)check(std::isfinite(primary_base[index][c]) && std::abs(primary_base[index][c]-base_primary[c])<.0004,
                            "Curved CURRENT primary transmission/emission mismatch");
                        check(primary_base[index][3]==1,"Curved current primary base validity mismatch");
                    }
                    V reconstructed{};bool pixel_edge=false;
                    for(unsigned lobe=0;lobe<lobes;++lobe) {
                        V outgoing=liquid_primary?reflected:normal(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),double(rough)*rough)));
                        if(dot(outgoing,n)<=0)outgoing=reflected;
                        V origin=first_origin,throughput{1,1,1},base{},feature{};double lower=bias;
                        std::array<unsigned,4> mirrors{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
                        unsigned kind=3,hops=0,mask=0,terminal=UINT32_MAX,incoming=environment;
                        const bool path_enabled=!liquid_primary || primary_weight[0]>0;
                        if(!path_enabled){kind=0;incoming=0;throughput={};}
                        for(unsigned bounce=0;path_enabled && bounce<4;++bounce) {
                            const double ground_t=liquid_primary && material==3 && !bounce && dot(outgoing,floor_normal)>0?65536
                                :floor_distance(origin,outgoing,lower,65536);
                            const auto hit=trace(positions,origin,outgoing,lower,ground_t);
                            if(hit.edge){pixel_edge=true;break;}
                            if(!hit.found && ground_t==65536){kind=2;feature=outgoing;break;}
                            if(hit.found) {
                                if(!transport){kind=1;terminal=hit.primitive;feature={hit.u,hit.v,0};incoming=terminal<2?0xff4d4d4dU:0xff1a1accU;break;}
                                mirrors[bounce]=hit.primitive;++model_hops;++hops;
                                const unsigned h=hit.primitive*3;V hn=normal(cross(sub(positions[h+1],positions[h]),sub(positions[h+2],positions[h])));
                                if(dot(hn,outgoing)>0)hn=scale(hn,-1);
                                if(metal){const auto tint=f0(hit.primitive);const double grazing=std::pow(1-std::clamp(dot(scale(outgoing,-1),hn),0.,1.),5);
                                    for(unsigned c=0;c<3;++c)throughput[c]*=tint[c]+(1-tint[c])*grazing;}
                                lower=std::max(.05,hit.t*1.e-5);origin=add(add(origin,scale(outgoing,hit.t)),scale(hn,lower));outgoing=lo::reflect(outgoing,hn);feature=outgoing;continue;
                            }
                            mirrors[bounce]=0xfffffffdU;mask|=1U<<bounce;++hops;++liquid_hops[material==3];
                            const auto surface=sample_liquid(origin,outgoing,ground_t,x,y);
                            if(surface.edge){pixel_edge=true;break;}
                            for(unsigned c=0;c<3;++c)base[c]+=throughput[c]*surface.base[c];throughput=scale(throughput,surface.share);
                            if(*std::max_element(throughput.begin(),throughput.end())<1.e-5){kind=4;incoming=0;feature={};break;}
                            lower=surface.bias;origin=add(surface.hit,scale(surface.normal,lower));outgoing=lo::reflect(outgoing,surface.normal);feature=outgoing;
                        }
                        if(pixel_edge)break;
                        const auto& actual=records[index*lobes+lobe];
                        const std::string where=" receivers="+std::to_string(receiver)+" liquid="+std::to_string(material)+" variant="+std::to_string(variant)+" eye="+std::to_string(e)+" phase="+std::to_string(phase)+" strength="+std::to_string(strength)+" caustics="+std::to_string(caustics)+" pixel="+std::to_string(index)+" lobe="+std::to_string(lobe);
                        if(actual.mirrors!=mirrors || actual.control!=((kind<<8)|(mask<<4)|hops) || actual.terminal!=terminal)
                            throw std::runtime_error("Curved actual ordered identity/terminal mismatch"+where+" actual/expected control="+std::to_string(actual.control)+"/"+std::to_string((kind<<8)|(mask<<4)|hops));
                        const V observed_feature{actual.feature[0],actual.feature[1],actual.feature[2]};
                        const double feature_error=std::sqrt(dot(sub(feature,observed_feature),sub(feature,observed_feature)));max_feature=std::max(max_feature,feature_error);
                        if(!std::isfinite(feature_error) || feature_error>=.00008 || actual.incoming!=incoming)
                            throw std::runtime_error("Curved terminal feature/incident colour mismatch"+where+" feature error="+std::to_string(feature_error)
                                +" incident="+std::to_string(actual.incoming)+"/"+std::to_string(incoming));
                        V current_base{},current_response{};
                        for(unsigned c=0;c<3;++c) {
                            const double re=std::abs(actual.response[c]-throughput[c]),be=std::abs(actual.base[c]-base[c]);max_response=std::max(max_response,re);max_base=std::max(max_base,be);
                            if(!std::isfinite(actual.response[c]) || !std::isfinite(actual.base[c]) || re>=.0002 || be>=.0004)
                                throw std::runtime_error("Curved current per-hop light/response mismatch"+where+" component="+std::to_string(c)+" errors="+std::to_string(re)+"/"+std::to_string(be));
                            current_base[c]=actual.base[c];current_response[c]=actual.response[c];
                        }
                        // Native packing occurs PER LOBE before the rough mean.
                        const auto incident=linear(actual.incoming);for(unsigned c=0;c<3;++c)current_base[c]+=incident[c]*current_response[c];
                        reconstructed=add(reconstructed,linear(pack(current_base)));++hop_counts[hops];++kinds[kind];++samples;
                    }
                    if(pixel_edge){edges+=lobes;continue;}
                    reconstructed=scale(reconstructed,1./lobes);for(unsigned c=0;c<3;++c)reconstructed[c]*=primary_response[index][c];
                    if(primary_base)for(unsigned c=0;c<3;++c)reconstructed[c]+=primary_base[index][c];
                    const unsigned expected=pack(reconstructed);check(prefix[index*4+3]==(liquid_primary?material==0?253:254:255),"Curved primary reflection alpha changed");
                    for(unsigned c=0;c<3;++c){const unsigned error=std::abs(int(prefix[index*4+c])-int((expected>>(c*8))&255));max_rgb=std::max(max_rgb,error);
                        check(error<=1,"Separated current-base/path records do not reconstruct native reflected RGBA");}
                    const auto& old=previous_prefix[caustics!=0];
                    if(phase && strength>.5F && old.size()==prefix.size())for(unsigned c=0;c<3;++c)
                        animated[material==3]+=std::abs(int(prefix[index*4+c])-int(old[index*4+c]))>2;
                }
                check(hardware[e].render_reflections(gpu.device,camera,resident,palette,environment,rough,metal,{},0,
                    g.environment_rotation,nullptr,floor,0,&water,false,range,0,encoding,transport),hardware[e].status().c_str());
                const auto off_output=hardware[e].reflection_output();
                auto off=download(off_output,water_layers.storage_bytes?water_layers.storage_bytes:count*4);
                if(receiver) {
                    const unsigned canonical_bytes=water_layers.storage_bytes?water_layers.storage_bytes:count*4;
                    check(off_output.water_layers==water_layers,"Full curved witnesses changed canonical water layout");
                    const auto mismatch=std::mismatch(off.begin(),off.end(),storage.begin());
                    if(mismatch.first!=off.end()) {
                        const auto at=std::size_t(mismatch.first-off.begin());
                        throw std::runtime_error("Full curved witnesses changed ordinary RGBA/canonical hidden-world/surface bytes: receivers="
                            +std::to_string(receiver)+" liquid="+std::to_string(material)+" variant="+std::to_string(variant)
                            +" eye="+std::to_string(e)+" phase="+std::to_string(phase)+" strength="+std::to_string(strength)
                            +" byte="+std::to_string(at)+" actual/off="+std::to_string(storage[at])+"/"+std::to_string(off[at]));
                    }
                    canonical_prefix_bytes+=canonical_bytes;
                }
                for(unsigned index=0;index<count;++index)if(ids[index]!=UINT32_MAX) {
                    for(unsigned c=0;c<4;++c)if(prefix[index*4+c]!=off[index*4+c])
                        throw std::runtime_error("Curved witness mode changed ordinary receiver RGBA: liquid="+std::to_string(material)
                            +" variant="+std::to_string(variant)+" eye="+std::to_string(e)+" phase="+std::to_string(phase)
                            +" strength="+std::to_string(strength)+" pixel="+std::to_string(index)+" channel="+std::to_string(c)
                            +" actual/off="+std::to_string(prefix[index*4+c])+"/"+std::to_string(off[index*4+c]));++exact_prefixes;}
                if(!phase && strength>.5F)previous_prefix[caustics!=0]=prefix;
                if(!caustics)plain_prefix=prefix;
                else {
                    check(plain_prefix.size()==prefix.size(),"Missing same-frame uncaustic reference");
                    for(unsigned index=0;index<count;++index)for(unsigned c=0;c<3;++c)
                        current_caustic_changes+=prefix[index*4+c]!=plain_prefix[index*4+c];
                }
            }
        }
    }
    check(samples>100000 && empty>10000 && liquid_hops[0]>1000 && liquid_hops[1]>1000 && model_hops>100
        && kinds[1]>100 && kinds[2]>1000 && kinds[4]>1000 && exact_prefixes>10000
        && animated[0]>100 && animated[1]>100 && liquid_primaries[0]>10000 && liquid_primaries[1]>10000
        && visible_guides>10000 && hidden_guides>10000 && canonical_prefix_bytes>1000000,
        "Curved native producer lacks positive optical/light/primary/canonical ownership coverage");
    check(current_caustic_changes>100,"Curved records ignored CURRENT transmitted caustic illumination");
    std::cout<<"Native curved MODEL/FULL producer format="<<gpu.format<<": paths="<<samples<<" empty="<<empty<<" boundaries="<<edges
        <<" water/lava/model hops="<<liquid_hops[0]<<'/'<<liquid_hops[1]<<'/'<<model_hops<<" kinds=";
    for(unsigned c:kinds)std::cout<<c<<' ';std::cout<<" hops=";for(unsigned c:hop_counts)std::cout<<c<<' ';
    std::cout<<" exact ordinary MODEL prefixes="<<exact_prefixes<<" current animated water/lava="<<animated[0]<<'/'<<animated[1]
        <<" max feature/response/base/RGB="<<max_feature<<'/'<<max_response<<'/'<<max_base<<'/'<<max_rgb
        <<" liquid primaries="<<liquid_primaries[0]<<'/'<<liquid_primaries[1]<<" visible/dry guides="<<visible_guides<<'/'<<hidden_guides
        <<" exact canonical prefix bytes="<<canonical_prefix_bytes
        <<" current caustic changes="<<current_caustic_changes
        <<". Staged 64-byte full producer, no cached colour, native Linux/Metal or performance acceptance."<<std::endl;
}
