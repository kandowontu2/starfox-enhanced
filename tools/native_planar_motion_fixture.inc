// Diagnostic-only binary64 geometry, old optical feature and current lighting
// reference. Neither GPU hits nor GPU records feed these expected values.
struct PlanarMotionCounts {
    unsigned cases{},checked{},clear{},ground{},models{},finite{},ground_motion{},model_motion{},invalid_motion{},
        mapped{},unmapped{},multihop{},off{},shadowed{},unshadowed{},edges{},max_rgb{};
    double max_motion{},max_bary{},max_depth{},max_base{},max_response{};
};
std::array<double,4> old_planar_feature(const std::array<Position,12>& old,unsigned secondary,
    std::array<double,2> bary,const RayReflectionHistory& history,bool& edge) {
    if(!history.previous_ground)return {};
    const unsigned first=secondary*3;
    for(unsigned c=0;c<3;++c)for(float value:old[first+c])if(!std::isfinite(value) || old[first+c][3]!=1)return {};
    const V a=position(old[first]),b=position(old[first+1]),c=position(old[first+2]);
    if(dot(cross(sub(b,a),sub(c,a)),cross(sub(b,a),sub(c,a)))<=1.e-20)return {};
    const auto& plane=*history.previous_ground;V point{},normal{};
    for(unsigned i=0;i<3;++i){point[i]=float(plane.point[i]);normal[i]=float(plane.normal[i]);}normal=unit(normal);
    const V feature=add(scale(a,1-bary[0]-bary[1]),add(scale(b,bary[0]),scale(c,bary[1])));
    const V virtual_point=sub(feature,scale(normal,2*dot(sub(feature,point),normal)));
    const double denominator=dot(virtual_point,normal);
    if(virtual_point[2]<=0 || std::abs(denominator)<=1.e-12)return {};
    const double fraction=dot(point,normal)/denominator,depth=virtual_point[2]*fraction;
    edge|=std::min({std::abs(fraction),std::abs(1-fraction),std::abs(depth-history.near_plane),std::abs(depth-history.far_plane)})<1.e-4;
    if(fraction<=0 || fraction>=1 || depth<history.near_plane || depth>history.far_plane)return {};
    // Project a virtual secondary feature, not the old primary ground point.
    const double x=history.projection[0]*virtual_point[0]/virtual_point[2]+history.projection[2];
    const double y=history.projection[1]*virtual_point[1]/virtual_point[2]+history.projection[3];
    edge|=std::min({std::abs(x),std::abs(y),std::abs(x-history.extent[0]),std::abs(y-history.extent[1])})<1.e-4;
    if(x<0 || y<0 || x>=history.extent[0] || y>=history.extent[1])return {};
    return {x,y,depth,1};
}
void verify_planar_motion(const NativeCubeInput& input,const Parameters& p,const reflected_liquid_oracle::Frame& frame,
    const std::array<Position,12>& old,const std::array<unsigned,4>& mapping,const RayReflectionHistory& history,
    const std::vector<unsigned>& actual,PlanarMotionCounts& n) {
    constexpr unsigned count=width*height;check(actual.size()==count*22,"Planar motion is not canonical 88 bytes/pixel");
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const unsigned id=y*width+x;
        const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},incoming=unit(raw);
        const auto primary=trace_native_colour(input.source,{},raw,p.primary_range[0],x,y,p.primary_range[1]);
        const double denominator=dot(raw,frame.normal),floor=std::abs(denominator)>1.e-10?dot(frame.point,frame.normal)/denominator:p.primary_range[1];
        const bool ground=floor>p.primary_range[0] && floor<(primary.found?primary.t:p.primary_range[1]),valid=ground || primary.found;
        const double depth=ground?floor:primary.found?primary.t:0;
        bool edge=primary.edge || std::min(std::abs(floor-p.primary_range[0]),std::abs(floor-(primary.found?primary.t:p.primary_range[1])))<1.e-4;
        V base{},response{},radiance{};ModelPath path;unsigned colour=0,incident=0;
        std::array<unsigned,4> identity{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};std::array<double,4> motion{},witness{};
        if(valid) {
            const V hit=scale(raw,depth);V normal=ground?unit(frame.normal):cube_model_normal(input,primary.primitive,raw);
            if(dot(normal,incoming)>0)normal=scale(normal,-1);
            const double bias=std::max(ground?.05:.01,depth*std::sqrt(dot(raw,raw))*1.e-5);
            const V origin=add(hit,scale(normal,bias));double input_scale=1;
            for(const auto& v:{frame.point,hit})for(double c:v)input_scale=std::max(input_scale,std::abs(c));
            if(!ground)for(unsigned corner=0;corner<3;++corner)for(unsigned c=0;c<3;++c)
                input_scale=std::max(input_scale,std::abs(double(input.source.source.geometry.vertices[primary.primitive*3+corner][c])));
            const double allowance=input_scale*1.e-6*(1+1/std::max(std::abs(dot(normal,incoming)),1.e-12));
            if(!ground || p.water[1]>0) {
                path=model_path(input,p,origin,reflected_liquid_oracle::reflect(incoming,normal),bias,x,y,false,allowance,&frame,ground);
                incident=path.current;edge|=path.edge;
            }
            if(ground) {
                const V light=unit(reflected_liquid_oracle::col({-1,-1,-1},frame));
                const auto shadow=trace_native_colour(input.source,origin,light,bias,x,y);edge|=shadow.edge;
                const double visibility=shadow.found?0:1;n.shadowed+=shadow.found;n.unshadowed+=!shadow.found;
                const V authored=native_bytes(p.environment[0]),tint=unsigned(p.water[3])==2?V{1,.875,.58}:V{.8,.82,.85};
                const V diffuse=scale(native_authored_linear(scale(tint,dot(authored,{.3,.59,.11})),p.material_info[2]),
                    .65+.35*visibility*std::max(0.,dot(normal,light)));
                const double highlight=std::pow(std::max(0.,dot(normal,unit(sub(light,incoming)))),96)*visibility
                    *std::max({authored[0],authored[1],authored[2]})*.55;
                base=add(scale(diffuse,1-p.water[1]),scale(V{1,.95,.82},highlight));
                if(p.water[1]>0) {
                    const V f0=unsigned(p.water[3])==2?V{1,.766,.336}:V{1,1,1};
                    response=scale(add(f0,scale(sub(V{1,1,1},f0),std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5))),p.water[1]);
                }
                radiance=add(base,product(native_linear(incident,p.material_info[2]),response));++n.ground;n.off+=p.water[1]==0;
            } else {response={1,1,1};radiance=native_linear(incident,p.material_info[2]);++n.models;}
            colour=native_pack(radiance,p.material_info[2],ground?254:255);
            const bool finite=(path.control>>8)==1 && (path.control&7)==0;
            if(finite) {
                const unsigned receiver=ground?UINT32_MAX-1:primary.primitive;
                identity[0]=receiver;identity[1]=path.terminal;witness={path.feature[0],path.feature[1],depth,1};
                if(history.previous_index_offset && (!ground || history.previous_ground)) {
                    identity[2]=ground?UINT32_MAX-1:mapping[receiver];identity[3]=mapping[path.terminal];++n.mapped;
                } else ++n.unmapped;
                motion=ground?old_planar_feature(old,path.terminal,{path.feature[0],path.feature[1]},history,edge)
                    :old_feature(old,receiver,path.terminal,{path.feature[0],path.feature[1]},history,edge);++n.finite;
            } else n.multihop+=(path.control&7)>0;
        } else ++n.clear;
        if(edge){++n.edges;continue;}
        check(actual[id]>>24==colour>>24,"Planar motion changed primary source ownership");
        check(actual[count*13+id]>>24==incident>>24,"Planar incoming validity mismatch");
        for(unsigned c=0;c<3;++c)for(const auto pair:{std::pair(actual[id],colour),std::pair(actual[count*13+id],incident)}) {
            const unsigned e=std::abs(int((pair.first>>(c*8))&255)-int((pair.second>>(c*8))&255));n.max_rgb=std::max(n.max_rgb,e);
            check(e<=1,"Planar current/incident colour mismatch");
        }
        for(unsigned c=0;c<4;++c) {
            check(actual[count*5+id*4+c]==identity[c],"Planar motion inferred identity, accepted ground or multihop reuse");
            const double m=std::bit_cast<float>(actual[count+id*4+c]),w=std::bit_cast<float>(actual[count*9+id*4+c]),
                b=std::bit_cast<float>(actual[count*14+id*4+c]),r=std::bit_cast<float>(actual[count*18+id*4+c]);
            check(std::isfinite(m) && std::isfinite(w) && std::isfinite(b) && std::isfinite(r),"Non-finite planar history record");
            if(c==3){check(m==motion[3] && w==witness[3] && b==double(valid) && r==0,"Planar history validity/ABI mismatch");continue;}
            const double me=std::abs(m-motion[c]),we=std::abs(w-witness[c]),be=std::abs(b-base[c]),re=std::abs(r-response[c]);
            n.max_motion=std::max(n.max_motion,me);n.max_base=std::max(n.max_base,be);n.max_response=std::max(n.max_response,re);
            if(c<2)n.max_bary=std::max(n.max_bary,we);else n.max_depth=std::max(n.max_depth,we);
            check(me<=std::max(.002,std::abs(motion[c])*2.e-5),"Planar old optical feature motion mismatch");
            check(we<=(c<2?2.e-5:std::max(.002,std::abs(witness[c])*2.e-5)),"Planar current feature/depth witness mismatch");
            check(be<=2.e-5 && re<=2.e-5,"Planar base or current Fresnel was transported/changed");
        }
        ++n.checked;if(motion[3]){n.ground_motion+=ground;n.model_motion+=!ground;}else ++n.invalid_motion;
    }
    ++n.cases;
}
void run_planar_recovery(Source& source,NativeRayOwner& rays);
void run_planar_motion(Source& source,NativeRayOwner& rays) {
    PlanarMotionCounts n;constexpr std::array<unsigned,4> mapping{17,9,6,12};
    for(unsigned material:{1U,2U})for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})
    for(unsigned context=0;context<5;++context)for(float strength:{0.f,.7f,1.f})for(unsigned pose=0;pose<10;++pose)for(bool matched:{false,true}) {
        auto input=cube_input(16,variant,encoding);auto p=cube_parameters(input,encoding,0,6);
        p.camera={46,39,31.3f,22.7f};p.settings={0,1,0,0};p.environment[0]=context%2?0xff29b50d:0xffa035e0;
        p.point={0,180,0,0};p.normal={0,-1,0,0};p.row0={1,0,0,29};p.row1={0,1,0,-3};p.row2={0,0,1,17};
        p.primary_range={1,65536,1,0};p.water={1.3f,strength,.72f,float(material)};
        if(context==0){p.cube_info={};input.cube.clear();input.size=0;}
        if(context==1){const float c=std::cos(.13f),s=std::sin(.13f);p.normal={s,-c,0,0};p.row0={c,-s,0,29};p.row1={s,c,0,-3};}
        if(context==2){p.row0={.984f,.1f,.02f,29};p.row1={-.12f,.99f,.07f,-3};p.row2={.015f,-.06f,1.012f,17};}
        if(context==3){p.primary_range={250,500,1,0};p.camera[0]=13;p.camera[1]=11;}
        if(context==4){
            // Explicit finite model-to-model mirrors alongside the ground.
            // The banked cube receiver otherwise sends virtually every model
            // ray through a floor hop, correctly invalidating single-hit reuse.
            constexpr std::array<std::array<float,2>,6> corners{{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}}};
            for(unsigned i=0;i<12;++i)input.source.source.geometry.vertices[i]={corners[i%6][0]*(i<6?450.f:1200.f),
                corners[i%6][1]*(i<6?300.f:900.f),i<6?400.f:-200.f,0};
        }
        reflected_liquid_oracle::Frame frame;frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};
        frame.rotation={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};frame.offset={29,-3,17};
        if(context==2){const V point=native_forward(sub(V{0,180,0},frame.offset),frame),normal=unit(reflected_liquid_oracle::col({0,-1,0},frame));
            p.point={float(point[0]),float(point[1]),float(point[2]),0};p.normal={float(normal[0]),float(normal[1]),float(normal[2]),0};
            frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};}
        RayWater water{p.water[0],strength,material};water.brightness=p.water[2];water.camera_position={29,-3,17};
        std::copy(frame.rotation.begin(),frame.rotation.end(),water.world_to_view.begin());
        const ReceiverPlane floor{{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,matched?4U:0U,true};
        history.previous_ground=RayReflectionGround{frame.point,frame.normal};
        auto old=input.source.source.geometry.vertices;for(auto& v:old)v[3]=1;
        if(pose==1){for(auto& v:old){v[0]+=23;v[1]-=11;v[2]+=17;v[2]+=.06f*v[0]-.03f*v[1];}
            history.previous_ground=RayReflectionGround{{12,195,8},{.07,-.994,.027}};}
        if(pose==2)for(auto& v:old)v[3]=0;
        if(pose==3)for(auto& v:old)v=old[0];
        if(pose==4){history.previous_ground.reset();for(unsigned i=0;i<6;++i){old[i][0]*=.33f;old[i][1]*=.33f;}}
        if(pose==5)history.near_plane=500;
        if(pose==6)for(auto& v:old)v[0]=std::numeric_limits<float>::quiet_NaN();
        if(pose==7){history.extent={10,8};history.projection={141,132,0,0};}
        if(pose==8)history.previous_ground=RayReflectionGround{{0,-190,0},{0,-1,0}};
        if(pose==9)for(unsigned i=0;i<6;++i){old[i][0]*=.18f;old[i][1]*=.18f;}
        source.set(input,old,mapping,history);const Camera camera{width,height,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        const PrimaryRayRange range{p.primary_range[0],p.primary_range[1]};
        const std::array<float,9> rotation{p.cube_row0[0],p.cube_row0[1],p.cube_row0[2],p.cube_row1[0],p.cube_row1[1],p.cube_row1[2],p.cube_row2[0],p.cube_row2[1],p.cube_row2[2]};
        check(source.render(rays,input,camera,encoding,&history,0,0,false,range,rotation,&water,floor,p.environment[0]),rays.status().c_str());
        check(rays.reflection_output().reflection_history==*native_reflection_history(width,height,history.extent,true),"Wrong public planar motion layout");
        check(!rays.reflection_output().water_layers.storage_bytes,"Planar motion invented a water prefix");
        const auto actual=source.get(rays.reflection_output());
        try{verify_planar_motion(input,p,frame,old,mapping,history,actual,n);}catch(const std::exception& e){throw std::runtime_error(std::string(e.what())
            +" material/encoding/variant/context/strength/pose/matched="+std::to_string(material)+"/"+std::to_string(encoding)+"/"+std::to_string(variant)
            +"/"+std::to_string(context)+"/"+std::to_string(strength)+"/"+std::to_string(pose)+"/"+std::to_string(matched));}
        check(source.render(rays,input,camera,encoding,nullptr,0,0,false,range,rotation,&water,floor,p.environment[0]),rays.status().c_str());
        const auto plain=source.get(rays.reflection_output());check(std::equal(plain.begin(),plain.end(),actual.begin()),"Planar history changed current RGBA (including boundary lanes)");
        check(!rays.reflection_output().reflection_history.storage_bytes,"Planar motion OFF retained stale layout");
    }
    std::cout<<"Native planar motion: cases="<<n.cases<<" checked/clear="<<n.checked<<'/'<<n.clear<<" ground/models="<<n.ground<<'/'<<n.models
        <<" finite="<<n.finite<<" valid ground/model/invalid motion="<<n.ground_motion<<'/'<<n.model_motion<<'/'<<n.invalid_motion
        <<" explicit mapped/unmapped="<<n.mapped<<'/'<<n.unmapped<<" rejected multihop="<<n.multihop<<" reflection-off floors="<<n.off
        <<" shadowed/unshadowed="<<n.shadowed<<'/'<<n.unshadowed<<" boundary exclusions="<<n.edges
        <<" max RGB/motion/bary/depth/base/response="<<n.max_rgb<<'/'<<n.max_motion<<'/'<<n.max_bary<<'/'<<n.max_depth<<'/'<<n.max_base<<'/'<<n.max_response<<'\n'<<std::flush;
    check(n.cases==3600 && n.checked>1000000 && n.clear>10000 && n.ground>10000 && n.models>10000 && n.finite>10000
        && n.ground_motion>1000 && n.model_motion>1000 && n.invalid_motion>10000 && n.mapped>1000 && n.unmapped>1000 && n.multihop>100
        && n.shadowed>100 && n.unshadowed>1000 && n.off>1000,"Insufficient planar optical-motion witnesses");
    rays.release_device();check(!rays.working_image_bytes() && rays.native_work_complete(),"Planar motion retained images after release");
    std::cout<<"Current RGBA byte-identical ON/OFF; old finite features/actual old plane, CURRENT response/light; NOT liquid/Linux/application reuse/FPS acceptance\n";
    run_planar_recovery(source,rays);
}
void run_planar_recovery(Source& source,NativeRayOwner& rays) {
    constexpr std::array<unsigned,4> mapping{17,9,6,12};unsigned negatives=0;
    auto input=cube_input(16,4,2);auto old=input.source.source.geometry.vertices;for(auto& v:old)v[3]=1;
    const Camera camera{width,height,46,31.3,22.7,39};const ReceiverPlane plane{{0,180,0},{0,-1,0}};
    for(unsigned material:{1U,2U}) {
        RayWater water{1.3f,.7f,material};
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,4,true};
        history.previous_ground=RayReflectionGround{{0,190,0},{0,-1,0}};
        source.set(input,old,mapping,history);
        const auto submit=[&](const RayReflectionHistory& h,const Camera& view,const RayWater* w,std::optional<ReceiverPlane> floor,
            unsigned encoding=2,float rough=0,unsigned metallic=0,bool specular=false){return source.render(rays,input,view,encoding,&h,rough,metallic,specular,
                {1,65536},{1,0,0,0,1,0,0,0,1},w,floor);};
        check(submit(history,camera,&water,plane),rays.status().c_str());const auto reference=source.get(rays.reflection_output());
        for(unsigned failure=0;failure<32;++failure) {
            auto bad=history;auto view=camera;auto surface=water;const RayWater* w=&surface;auto floor=std::optional(plane);
            unsigned encoding=2,metallic=0;float rough=0;bool specular=false;
            switch(failure) {
                case 0:bad.separated=false;break;
                case 1:surface.mirror_models=true;break;
                case 2:surface.material=0;surface.source_colour=std::array<float,3>{.2f,.3f,.4f};break; // Curved liquid cannot accept the old planar frame.
                case 3:surface.material=3;break;
                case 4:bad.previous_liquid=RayReflectionLiquid{{{0,180,0},{0,-1,0}}};break;
                case 5:surface.auxiliary_layers=true;break;
                case 6:surface.surface_layers=true;break;
                case 7:surface.source_colour=std::array<float,3>{.2f,.3f,.4f};break;
                case 8:w=nullptr;break;
                case 9:floor.reset();break;
                case 10:rough=.4f;break;
                case 11:metallic=1;break;
                case 12:specular=true;break;
                case 13:bad.model_lobes=1;break;
                case 14:bad.model_paths=true;break;
                case 15:bad.scene_paths=true;break;
                case 16:bad.previous_ground->normal={};break;
                case 17:bad.previous_ground->point[0]=std::numeric_limits<double>::quiet_NaN();break;
                case 18:bad.previous_ground->normal={std::cos(.003)*1.000000001e-10,std::sin(.003)*1.000000001e-10,0};break; // Valid double collapses to the GPU threshold.
                case 19:bad.projection[0]=1.e-300;break;
                case 20:bad.near_plane=1.e10;bad.far_plane=1.e10+1;break;
                case 21:bad.previous_vertex_offset=0;break;
                case 22:++bad.previous_vertex_offset;break;
                case 23:bad.previous_index_offset=bad.previous_vertex_offset+4;break;
                case 24:bad.previous_index_offset=capacity;break;
                case 25:bad.previous_vertex_offset=capacity;bad.previous_index_offset=0;break;
                case 26:view.width=view.height=16384;break;
                case 27:encoding=0;break;
                case 28:surface.world_to_view={};break;
                case 29:surface.reflection_strength=std::numeric_limits<float>::quiet_NaN();break;
                case 30:bad.extent[1]=16385;break;
                case 31:bad.previous_ground->point[1]=1.e13;break;
            }
            if(submit(bad,view,w,floor,encoding,rough,metallic,specular))throw std::runtime_error("Malformed/unsupported planar history accepted case="+std::to_string(failure));
            const auto output=rays.reflection_output();check(!output.buffer && !output.reflection_history.storage_bytes && !output.water_layers.storage_bytes,
                "Rejected planar history exposed stale output");
            check(submit(history,camera,&water,plane),rays.status().c_str());check(source.get(rays.reflection_output())==reference,"Planar history failed exact recovery");++negatives;
        }
        for(unsigned cycle=0;cycle<2;++cycle) {
            check(rays.native_work_complete() && rays.try_release_device(),"Completed planar history cannot release nonblocking");
            check(!rays.working_image_bytes() && !rays.reflection_output().buffer,"Planar history release retained output/images");
            check(submit(history,camera,&water,plane),rays.status().c_str());check(source.get(rays.reflection_output())==reference,"Planar history reinitialize changed records");
        }
    }
    rays.release_device();check(rays.native_work_complete() && !rays.working_image_bytes(),"Planar recovery retained live images");
    std::cout<<"Malformed/unsupported planar history rejected="<<negatives<<", whole-allocation exact recoveries="<<negatives<<", release/reinitialize cycles=4\n"<<std::flush;
}
