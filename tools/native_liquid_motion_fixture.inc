// Production Vulkan owner; independent binary64 current/old optical geometry.
// Returned motion is checked against INPUT old finite features, not GPU hits.
struct LiquidMotionCounts {
    unsigned cases{},checked{},clear{},models{},finite{},mapped{},unmapped{},multihop{},edges{},max_rgb{},prefixes{},crossings{};
    std::array<unsigned,2> ground{},valid{};double max_optical{},max_bary{},max_depth{},max_base{},max_response{},max_old_depth{};
};
reflected_liquid_oracle::Frame accepted_liquid_frame(const RayReflectionHistory& history) {
    // Evaluate the actual rounded input ABI, never rounded GPU output/hits.
    const auto words=reflection_liquid_frame_words(history);reflected_liquid_oracle::Frame f;
    f.point={words[0],words[1],words[2]};f.normal={words[4],words[5],words[6]};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)f.rotation[r*3+c]=words[8+r*4+c];
    f.offset={words[11],words[15],words[19]};f.projection={words[20],words[21],words[22],words[23]};
    f.width=unsigned(words[24]);f.height=unsigned(words[25]);f.near=words[26];f.far=words[27];f.time=words[28];f.material=unsigned(words[29]);return f;
}
void verify_liquid_motion(const NativeCubeInput& input,const Parameters& p,const reflected_liquid_oracle::Frame& frame,
    const std::array<Position,12>& old,const std::array<unsigned,4>& mapping,const RayReflectionHistory& history,
    const NativeReflectionHistory& layout,const std::vector<unsigned>& actual,LiquidMotionCounts& n) {
    constexpr unsigned count=width*height;const unsigned kind=frame.material==3;
    check(actual.size()==layout.storage_bytes/4,"Liquid history allocation extent mismatch");
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const unsigned id=y*width+x;
        const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},incoming=unit(raw);
        const auto primary=trace_native_colour(input.source,{},raw,p.primary_range[0],x,y,p.primary_range[1]);
        const double denominator=dot(raw,frame.normal),floor=std::abs(denominator)>1.e-10?dot(frame.point,frame.normal)/denominator:p.primary_range[1];
        const bool ground=floor>p.primary_range[0] && floor<(primary.found?primary.t:p.primary_range[1]),valid=ground || primary.found;
        const double depth=ground?floor:primary.found?primary.t:0;
        bool edge=primary.edge || std::min(std::abs(floor-p.primary_range[0]),std::abs(floor-(primary.found?primary.t:p.primary_range[1])))<1.e-4;
        V base{},response{},radiance{};NativeGroundPath path;unsigned colour=0,incident=0;
        std::array<unsigned,4> identity{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};std::array<double,4> witness{},direct_motion{};
        if(valid) {
            V hit=scale(raw,depth),normal=ground?unit(frame.normal):cube_model_normal(input,primary.primitive,raw);
            if(dot(normal,incoming)>0)normal=scale(normal,-1);
            double bias=std::max(ground?.05:.01,depth*std::sqrt(dot(raw,raw))*1.e-5);
            if(ground) {
                if(kind) {const auto lava=native_lava_optical(p,frame,{},incoming,floor*std::sqrt(dot(raw,raw)));
                    hit=lava.hit;normal=lava.normal;bias=lava.bias;base=lava.emission;response={lava.share,lava.share,lava.share};}
                else {const auto water=native_water_expected(input.source,p,frame,{},incoming,floor*std::sqrt(dot(raw,raw)),x,y);
                    normal=water.normal;bias=water.bias;base=water.light;response={water.fresnel,water.fresnel,water.fresnel};edge|=water.edge;}
            } else response={1,1,1};
            if(!ground || response[0]>0) {
                const V origin=add(hit,scale(normal,bias)),direction=reflected_liquid_oracle::reflect(incoming,normal);
                path=native_ground_path(input,p,frame,origin,direction,bias,x,y,ground);incident=path.packed;edge|=path.edge;
                if(ground && kind && path.secondary!=UINT32_MAX) {
                    const double den=dot(direction,frame.normal),crossing=den>0?dot(sub(frame.point,origin),frame.normal)/den:0;
                    const auto finite=trace_native_colour(input.source,origin,direction,bias,x,y);
                    n.crossings+=crossing>bias && crossing<finite.t;
                }
            }
            radiance=add(base,product(native_linear(incident,p.material_info[2]),response));
            colour=native_pack(radiance,p.material_info[2],ground?(kind?254:253):255);
            if(path.secondary!=UINT32_MAX) {
                const unsigned receiver=ground?UINT32_MAX-1:primary.primitive;
                identity[0]=receiver;identity[1]=path.secondary;witness={path.bary[0],path.bary[1],depth,1};
                if(history.previous_index_offset && (!ground || history.previous_liquid)) {
                    identity[2]=ground?UINT32_MAX-1:mapping[receiver];identity[3]=mapping[path.secondary];++n.mapped;
                } else ++n.unmapped;
                if(!ground)direct_motion=old_feature(old,receiver,path.secondary,path.bary,history,edge);
                ++n.finite;
            } else n.multihop+=path.ground>0;
            if(ground)++n.ground[kind];else ++n.models;
        } else ++n.clear;
        if(edge){++n.edges;continue;}
        const unsigned ma=layout.motion_offset/4+id*4,ia=layout.identity_offset/4+id*4,wa=layout.witness_offset/4+id*4;
        const unsigned ba=layout.base_offset/4+id*4,ra=layout.weight_offset/4+id*4;
        check(actual[id]>>24==colour>>24,"Liquid history changed primary source ownership");
        check(actual[layout.incoming_offset/4+id]>>24==incident>>24,"Liquid incident validity mismatch");
        for(unsigned c=0;c<3;++c)for(const auto pair:{std::pair(actual[id],colour),std::pair(actual[layout.incoming_offset/4+id],incident)}) {
            const unsigned e=std::abs(int((pair.first>>(c*8))&255)-int((pair.second>>(c*8))&255));n.max_rgb=std::max(n.max_rgb,e);
            check(e<=1,"Liquid current/incident colour mismatch");
        }
        std::array<float,4> motion{};
        for(unsigned c=0;c<4;++c) {
            check(actual[ia+c]==identity[c],"Liquid inferred accepted identity, hidden receiver or multihop reuse");
            motion[c]=std::bit_cast<float>(actual[ma+c]);const double w=std::bit_cast<float>(actual[wa+c]),b=std::bit_cast<float>(actual[ba+c]),r=std::bit_cast<float>(actual[ra+c]);
            check(std::isfinite(motion[c]) && std::isfinite(w) && std::isfinite(b) && std::isfinite(r),"Non-finite liquid history record");
            if(c==3){check(w==witness[3] && b==double(valid) && r==0,"Liquid history validity/ABI mismatch");continue;}
            const double we=std::abs(w-witness[c]),be=std::abs(b-base[c]),re=std::abs(r-response[c]);
            if(c<2)n.max_bary=std::max(n.max_bary,we);else n.max_depth=std::max(n.max_depth,we);
            n.max_base=std::max(n.max_base,be);n.max_response=std::max(n.max_response,re);
            check(we<=(c<2?.0001:std::max(.02,std::abs(depth)*2.e-5)),"Liquid finite feature/analytic depth mismatch");
            check(be<=2.e-5 && re<=2.e-5,"Liquid current lighting/transmission/Fresnel changed");
        }
        if(!ground) {
            for(unsigned c=0;c<4;++c)check(std::abs(motion[c]-direct_motion[c])<=std::max(.002,std::abs(direct_motion[c])*2.e-5),"Liquid scene direct MODEL motion mismatch");
        } else if(motion[3]) {
            check(motion[3]==1 && history.previous_liquid && history.previous_index_offset && path.secondary!=UINT32_MAX
                && mapping[path.secondary]!=UINT32_MAX,"Unmatched liquid emitted accepted motion");
            const unsigned first=path.secondary*3;for(unsigned corner=0;corner<3;++corner)for(float value:old[first+corner])
                check(std::isfinite(value) && old[first+corner][3]==1,"Invalid old finite feature seeded liquid motion");
            const V a=position(old[first]),b=position(old[first+1]),c=position(old[first+2]);
            check(dot(cross(sub(b,a),sub(c,a)),cross(sub(b,a),sub(c,a)))>1.e-20,"Degenerate old feature seeded liquid motion");
            const V target=add(scale(a,1-path.bary[0]-path.bary[1]),add(scale(b,path.bary[0]),scale(c,path.bary[1])));
            const auto previous=accepted_liquid_frame(history);const auto ray=reflected_liquid_oracle::optical(previous,motion[0],motion[1]);
            const double residual=reflected_liquid_oracle::error(previous,target,ray,motion[0],motion[1]);
            const double depth_error=std::abs(ray.depth-motion[2]);n.max_optical=std::max(n.max_optical,residual);n.max_old_depth=std::max(n.max_old_depth,depth_error);
            check(ray.valid && std::isfinite(residual) && residual<=.03 && depth_error<=.04,"Accepted liquid guide did not invert actual old waves");++n.valid[kind];
        } else check(motion==std::array<float,4>{},"Rejected liquid motion is not exact zero");
        ++n.checked;
    }
    ++n.cases;
}
void run_liquid_recovery(Source& source,NativeRayOwner& rays);
void run_liquid_motion(Source& source,NativeRayOwner& rays) {
    LiquidMotionCounts n;constexpr std::array<unsigned,4> mapping{17,9,6,12};
    for(unsigned material:{0U,3U})for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})
    for(unsigned context=0;context<4;++context)for(float strength:{0.f,.85f})for(unsigned pose=0;pose<8;++pose)for(bool matched:{false,true})
    for(unsigned prefix=0;prefix<(material==0?3U:1U);++prefix) {
        auto input=cube_input(16,variant,encoding);auto p=cube_parameters(input,encoding,0,6);
        p.camera={46,39,31.3f,22.7f};p.settings={0,1,0,0};p.environment[0]=context%2?0xff29b50d:0xffa035e0;
        p.point={0,180,0,0};p.normal={0,-1,0,0};p.row0={1,0,0,29};p.row1={0,1,0,-3};p.row2={0,0,1,17};
        p.primary_range={1,65536,1,0};p.water={2.4f,strength,.72f,float(material)};
        if(material==0)p.source_colour={.2f,.3f,.4f,1};
        if(context==0){p.cube_info={};input.cube.clear();input.size=0;}
        if(context==1){const float c=std::cos(.13f),s=std::sin(.13f);p.normal={s,-c,0,0};p.row0={c,-s,0,29};p.row1={s,c,0,-3};}
        if(context==2){p.row0={.984f,.1f,.02f,29};p.row1={-.12f,.99f,.07f,-3};p.row2={.015f,-.06f,1.012f,17};}
        if(context==3){p.primary_range={250,500,1,0};p.camera[0]=13;p.camera[1]=11;p.water[0]=9.125f;p.water[2]=0;}
        reflected_liquid_oracle::Frame frame;frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};
        frame.rotation={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};frame.offset={29,-3,17};frame.material=material;
        if(context==2){const V point=native_forward(sub(V{0,180,0},frame.offset),frame),normal=unit(reflected_liquid_oracle::col({0,-1,0},frame));
            p.point={float(point[0]),float(point[1]),float(point[2]),0};p.normal={float(normal[0]),float(normal[1]),float(normal[2]),0};
            frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};}
        RayWater water{p.water[0],strength,material};water.brightness=p.water[2];water.camera_position={29,-3,17};
        std::copy(frame.rotation.begin(),frame.rotation.end(),water.world_to_view.begin());
        if(material==0){water.source_colour=std::array<float,3>{.2f,.3f,.4f};water.surface_layers=prefix==1;water.auxiliary_layers=prefix==2;}
        const ReceiverPlane floor{{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,matched?4U:0U,true};
        history.previous_liquid=RayReflectionLiquid{RayReflectionGround{frame.point,frame.normal},frame.rotation,frame.offset,1.3f,material};
        auto old=input.source.source.geometry.vertices;for(auto& v:old)v[3]=1;
        if(pose==1)history.previous_liquid.reset();
        if(pose==2)for(auto& v:old)v[3]=0;
        if(pose==3){for(auto& v:old){v[0]+=23;v[1]-=11;v[2]+=17;}auto& previous=*history.previous_liquid;
            previous.time=7.2f;previous.ground={{12,195,8},{.07f,-.994f,.027f}};previous.world_to_view={1.01f,0,.02f,0,.99f,0,-.015f,0,1.03f};}
        if(pose==4)history.near_plane=500;
        if(pose==5)for(auto& v:old)v=old[0];
        if(pose==6)for(auto& v:old)v[0]=std::numeric_limits<float>::quiet_NaN();
        if(pose==7){history.extent={10,8};history.projection={141,132,0,0};}
        source.set(input,old,mapping,history);const Camera camera{width,height,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        const PrimaryRayRange range{p.primary_range[0],p.primary_range[1]};
        const std::array<float,9> rotation{p.cube_row0[0],p.cube_row0[1],p.cube_row0[2],p.cube_row1[0],p.cube_row1[1],p.cube_row1[2],p.cube_row2[0],p.cube_row2[1],p.cube_row2[2]};
        check(source.render(rays,input,camera,encoding,&history,0,0,false,range,rotation,&water,floor,p.environment[0]),rays.status().c_str());
        const auto output=rays.reflection_output();const auto layout=output.reflection_history;
        check(layout==*native_reflection_history(width,height,history.extent,true,output.water_layers),"Wrong public combined liquid motion ABI");
        check(output.water_layers.storage_bytes==(prefix?native_water_layers(width,height,prefix==2)->storage_bytes:0),"Liquid prefix was fabricated/lost");
        const auto actual=source.get(output);
        try{verify_liquid_motion(input,p,frame,old,mapping,history,layout,actual,n);}catch(const std::exception& e){throw std::runtime_error(std::string(e.what())
            +" material/encoding/variant/context/strength/pose/matched/prefix="+std::to_string(material)+"/"+std::to_string(encoding)+"/"+std::to_string(variant)
            +"/"+std::to_string(context)+"/"+std::to_string(strength)+"/"+std::to_string(pose)+"/"+std::to_string(matched)+"/"+std::to_string(prefix));}
        check(source.render(rays,input,camera,encoding,nullptr,0,0,false,range,rotation,&water,floor,p.environment[0]),rays.status().c_str());
        const auto plain=source.get(rays.reflection_output());check(std::equal(plain.begin(),plain.end(),actual.begin()),"Liquid history changed CURRENT RGBA/world/surface prefix (including boundaries)");++n.prefixes;
        check(!rays.reflection_output().reflection_history.storage_bytes,"Liquid history OFF retained stale layout");
    }
    std::cout<<"Native curved liquid motion: cases="<<n.cases<<" checked/clear="<<n.checked<<'/'<<n.clear<<" water/lava/models="<<n.ground[0]<<'/'<<n.ground[1]<<'/'<<n.models
        <<" finite="<<n.finite<<" valid water/lava="<<n.valid[0]<<'/'<<n.valid[1]<<" explicit mapped/unmapped="<<n.mapped<<'/'<<n.unmapped
        <<" rejected multihop="<<n.multihop<<" displaced lava escaping reference plane="<<n.crossings<<" boundary exclusions="<<n.edges
        <<" max RGB/old optics/bary/analytic depth/base/response/old depth="<<n.max_rgb<<'/'<<n.max_optical<<'/'<<n.max_bary<<'/'<<n.max_depth<<'/'<<n.max_base<<'/'<<n.max_response<<'/'<<n.max_old_depth<<'\n'<<std::flush;
    check(n.cases==3072 && n.checked>1000000 && n.clear>10000 && n.ground[0]>10000 && n.ground[1]>10000 && n.models>10000
        && n.finite>10000 && n.valid[0]>1000 && n.valid[1]>1000 && n.mapped>1000 && n.unmapped>1000 && n.multihop>100 && n.crossings>100,
        "Insufficient actual curved liquid optical-motion witnesses");
    rays.release_device();check(!rays.working_image_bytes() && rays.native_work_complete(),"Liquid history retained images after release");
    std::cout<<"CURRENT prefix byte-identical ON/OFF, actual accepted old waves; producer only, NOT Linux/application reuse/FPS or artistic lava acceptance\n"<<std::flush;
    run_liquid_recovery(source,rays);
}
void run_liquid_recovery(Source& source,NativeRayOwner& rays) {
    constexpr std::array<unsigned,4> mapping{17,9,6,12};unsigned negatives=0;
    auto input=cube_input(16,4,2);auto old=input.source.source.geometry.vertices;for(auto& v:old)v[3]=1;
    const Camera camera{width,height,46,31.3,22.7,39};const ReceiverPlane plane{{0,180,0},{0,-1,0}};
    for(unsigned material:{0U,3U}) {
        RayWater water{2.4f,.85f,material};if(material==0){water.source_colour=std::array<float,3>{.2f,.3f,.4f};water.auxiliary_layers=true;}
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,4,true};
        history.previous_liquid=RayReflectionLiquid{RayReflectionGround{{0,190,0},{0,-1,0}},{1,0,0,0,1,0,0,0,1},{},1.3f,material};
        source.set(input,old,mapping,history);
        const auto submit=[&](const RayReflectionHistory& h,const Camera& view,const RayWater* w,std::optional<ReceiverPlane> floor,
            unsigned encoding=2,float rough=0,unsigned metallic=0,bool specular=false){return source.render(rays,input,view,encoding,&h,rough,metallic,specular,
                {1,65536},{1,0,0,0,1,0,0,0,1},w,floor);};
        check(submit(history,camera,&water,plane),rays.status().c_str());const auto reference=source.get(rays.reflection_output());
        for(unsigned failure=0;failure<24;++failure) {
            auto bad=history;auto view=camera;auto surface=water;const RayWater* w=&surface;auto floor=std::optional(plane);
            unsigned encoding=2,metallic=0;float rough=0;bool specular=false;
            switch(failure) {
                case 0:bad.separated=false;break;
                case 1:surface.mirror_models=true;break;
                case 2:bad.previous_ground=RayReflectionGround{{0,180,0},{0,-1,0}};break;
                case 3:bad.previous_liquid->material=material==0?3:0;break;
                case 4:bad.previous_liquid->world_to_view={1,1,0,1,1+1.e-9,0,0,0,1};break;
                case 5:bad.previous_liquid->ground.normal={};break;
                case 6:bad.previous_liquid->time=-1;break;
                case 7:bad.previous_liquid->offset[0]=std::numeric_limits<double>::quiet_NaN();break;
                case 8:w=nullptr;break;
                case 9:floor.reset();break;
                case 10:rough=.4f;break;
                case 11:metallic=1;break;
                case 12:specular=true;break;
                case 13:bad.model_lobes=1;break;
                case 14:bad.model_paths=true;break;
                case 15:bad.scene_paths=true;break;
                case 16:if(material==0)surface.source_colour.reset();else surface.source_colour=std::array<float,3>{.2f,.3f,.4f};break;
                case 17:bad.projection[0]=1.e-300;break;
                case 18:bad.near_plane=1.e10;bad.far_plane=1.e10+1;break;
                case 19:bad.previous_vertex_offset=0;break;
                case 20:++bad.previous_vertex_offset;break;
                case 21:bad.previous_index_offset=bad.previous_vertex_offset+16;break;
                case 22:bad.extent[1]=16385;break;
                case 23:surface.world_to_view={1,0,0,0,0,0,0,0,1};break;
            }
            if(submit(bad,view,w,floor,encoding,rough,metallic,specular))throw std::runtime_error("Malformed liquid history accepted case="+std::to_string(failure));
            const auto output=rays.reflection_output();check(!output.buffer && !output.reflection_history.storage_bytes && !output.water_layers.storage_bytes,"Rejected liquid exposed stale output");
            check(submit(history,camera,&water,plane),rays.status().c_str());check(source.get(rays.reflection_output())==reference,"Liquid history failed whole-allocation exact recovery");++negatives;
        }
        for(unsigned cycle=0;cycle<2;++cycle) {
            check(rays.native_work_complete() && rays.try_release_device(),"Completed liquid history cannot release nonblocking");
            check(!rays.working_image_bytes() && !rays.reflection_output().buffer,"Liquid release retained output/images");
            check(submit(history,camera,&water,plane),rays.status().c_str());check(source.get(rays.reflection_output())==reference,"Liquid reinitialize changed records");
        }
    }
    rays.release_device();check(rays.native_work_complete() && !rays.working_image_bytes(),"Liquid recovery retained live images");
    std::cout<<"Malformed liquid history rejected="<<negatives<<", whole-allocation exact recoveries="<<negatives<<", release/reinitialize cycles=4\n"<<std::flush;
}
