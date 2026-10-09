// Input-only binary64 MODEL + actual mirror/gold-plane path reference.
// Included by the pinned-owner diagnostic, never by the application.
struct SceneHistoryCounts {
    ModelCounts paths;
    unsigned ground{},models{},ground_hops{},zero_strength{},shadowed{},unshadowed{},duplicates{};
    double max_base{};
};
void verify_scene_history(const NativeCubeInput& input,const Parameters& p,const reflected_liquid_oracle::Frame& frame,
    const RayReflectionHistory& history,const std::vector<unsigned>& actual,SceneHistoryCounts& n) {
    constexpr unsigned count=width*height;const unsigned lobes=history.model_lobes;auto& totals=n.paths;
    check(actual.size()==count*(11+lobes*13),"SCENE path history is not its canonical 96/460-byte layout");
    const bool transport=p.material_info[3] || (unsigned(p.water[3])&16);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const unsigned id=y*width+x;
        const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1},incoming=unit(raw);
        const auto model=trace_native_colour(input.source,{},raw,p.primary_range[0],x,y,p.primary_range[1]);
        const double denominator=dot(raw,frame.normal),floor=std::abs(denominator)>1.e-10?dot(frame.point,frame.normal)/denominator:p.primary_range[1];
        const bool ground=floor>p.primary_range[0] && floor<(model.found?model.t:p.primary_range[1]),valid=ground || model.found;
        const unsigned identity=ground?UINT32_MAX-1:model.found?model.primitive:UINT32_MAX;
        const double depth=ground?floor:model.found?model.t:0;
        bool boundary=model.edge || std::min(std::abs(floor-p.primary_range[0]),std::abs(floor-(model.found?model.t:p.primary_range[1])))<1.e-4;
        V response{},base{},radiance{};std::array<ModelPath,8> paths{};
        if(ground) {
            const V hit=scale(raw,floor);V normal=unit(frame.normal);if(dot(normal,incoming)>0)normal=scale(normal,-1);
            const double distance=floor*std::sqrt(dot(raw,raw)),bias=std::max(.05,distance*1.e-5);
            const V light=unit(reflected_liquid_oracle::col({-1,-1,-1},frame));
            const auto shadow=trace_native_colour(input.source,add(hit,scale(normal,bias)),light,bias,x,y);
            boundary|=shadow.edge;const double visibility=shadow.found?0:1;
            const V authored=native_bytes(p.environment[0]),tint=(unsigned(p.water[3])&15)==2?V{1,.875,.58}:V{.8,.82,.85};
            const V diffuse=scale(native_authored_linear(scale(tint,dot(authored,{.3,.59,.11})),p.material_info[2]),
                .65+.35*visibility*std::max(0.,dot(normal,light)));
            const double highlight=std::pow(std::max(0.,dot(normal,unit(sub(light,incoming)))),96)*visibility
                *std::max({authored[0],authored[1],authored[2]})*.55;
            base=add(scale(diffuse,1-p.water[1]),scale(V{1,.95,.82},highlight));
            if(p.water[1]>0) {
                const double grazing=std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5);
                const V f0=(unsigned(p.water[3])&15)==2?V{1,.766,.336}:V{1,1,1};
                response=scale(add(f0,scale(sub(V{1,1,1},f0),grazing)),p.water[1]);
                double input_scale=1;for(const auto& v:{frame.point,hit})for(double component:v)input_scale=std::max(input_scale,std::abs(component));
                const double allowance=input_scale*1.e-6*(1+1/std::max(std::abs(dot(normal,incoming)),1.e-12));
                paths[0]=model_path(input,p,add(hit,scale(normal,bias)),reflected_liquid_oracle::reflect(incoming,normal),bias,x,y,transport,allowance,&frame,true);
                boundary|=paths[0].edge;
            }
            for(unsigned lobe=1;lobe<lobes;++lobe)paths[lobe]=paths[0];
            radiance=add(base,product(native_linear(paths[0].current,p.material_info[2]),response));
            ++n.ground;n.zero_strength+=p.water[1]==0;n.shadowed+=shadow.found;n.unshadowed+=!shadow.found;
        } else if(model.found) {
            const V normal=cube_model_normal(input,model.primitive,raw),hit=scale(raw,model.t);
            const double bias=std::max(.01,model.t*std::sqrt(dot(raw,raw))*1.e-5);const V origin=add(hit,scale(normal,bias));
            const auto directions=native_model_lobes(incoming,normal,p.settings[0]);
            check(directions.count==lobes,"SCENE MODEL lobe contract mismatch");totals.clips+=directions.hemisphere_clips;
            double input_scale=1;for(unsigned corner=0;corner<3;++corner)for(unsigned c=0;c<3;++c)
                input_scale=std::max(input_scale,std::abs(double(input.source.source.geometry.vertices[model.primitive*3+corner][c])));
            for(double component:origin)input_scale=std::max(input_scale,std::abs(component));
            const double allowance=input_scale*1.e-6*(1+1/std::max(std::abs(dot(normal,incoming)),1.e-12));
            response={1,1,1};if(p.dimensions[3]) {const V f0=native_model_f0(model.colour,p.dimensions[3],p.material_info[2]);
                response=add(f0,scale(sub(V{1,1,1},f0),std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5)));}
            V sum{};for(unsigned lobe=0;lobe<lobes;++lobe) {
                paths[lobe]=model_path(input,p,origin,directions.direction[lobe],bias,x,y,transport,allowance,&frame);
                boundary|=paths[lobe].edge;sum=add(sum,scale(native_linear(paths[lobe].current,p.material_info[2]),1./lobes));
            }
            radiance=product(sum,response);++n.models;
        }
        if(boundary){++totals.edges;continue;}
        check(actual[count+id]==identity,"SCENE primary plane/model/clear identity mismatch");
        const double actual_depth=std::bit_cast<float>(actual[count*2+id]),de=std::abs(actual_depth-depth);
        totals.max_depth=std::max(totals.max_depth,de);
        check(std::isfinite(actual_depth) && de<=std::max(.002,std::abs(depth)*2.e-5),"SCENE primary depth mismatch");
        for(unsigned c=0;c<4;++c) {
            const double weight=std::bit_cast<float>(actual[count*3+id*4+c]),light=std::bit_cast<float>(actual[count*7+id*4+c]);
            check(std::isfinite(weight) && std::isfinite(light),"SCENE current response/base is not finite");
            if(c==3)check(weight==double(valid) && light==double(valid),"SCENE current decomposition validity mismatch");
            else {const double we=std::abs(weight-response[c]),be=std::abs(light-base[c]);
                totals.max_response=std::max(totals.max_response,we);n.max_base=std::max(n.max_base,be);
                check(we<=2.e-5 && be<=2.e-5,"SCENE transported or changed current response/base light");}
        }
        const unsigned expected=valid?native_pack(radiance,p.material_info[2],ground?254:255):0;
        check(actual[id]>>24==expected>>24,"SCENE plane/model/clear source ownership mismatch");
        for(unsigned c=0;c<3;++c) {const unsigned e=std::abs(int((actual[id]>>(8*c))&255)-int((expected>>(8*c))&255));
            totals.max_rgb=std::max(totals.max_rgb,e);check(e<=1,"SCENE current RGB mismatch");}
        for(unsigned lobe=0;lobe<lobes;++lobe) {
            const unsigned at=count*11+(id*lobes+lobe)*13;const auto& path=paths[lobe];const unsigned kind=path.control>>8;
            if(!valid || (ground && p.water[1]==0))for(unsigned c=0;c<13;++c)
                check(actual[at+c]==((c<4 || c==5)?UINT32_MAX:0),"SCENE inactive path retained a stale word");
            for(unsigned c=0;c<4;++c) {check(actual[at+c]==path.mirrors[c],"SCENE lost/reordered a model or ground hop");
                n.ground_hops+=path.mirrors[c]==UINT32_MAX-1;}
            check(actual[at+4]==path.control && actual[at+5]==path.terminal,"SCENE path kind/count/terminal mismatch");
            for(unsigned c=0;c<3;++c) {
                const double feature=std::bit_cast<float>(actual[at+6+c]),weight=std::bit_cast<float>(actual[at+10+c]);
                const double fe=std::abs(feature-path.feature[c]),we=std::abs(weight-path.response[c]);
                check(std::isfinite(feature) && std::isfinite(weight),"SCENE path feature/throughput not finite");
                totals.max_feature=std::max(totals.max_feature,fe);totals.max_response=std::max(totals.max_response,we);
                check(fe<=(kind==1 && c<2?path.bary_allowance:2.e-5) && we<=2.e-5,"SCENE path feature/current throughput mismatch");
            }
            const unsigned incident=actual[at+9];check(incident>>24==path.incoming>>24,"SCENE incident source validity mismatch");
            for(unsigned c=0;c<3;++c) {const unsigned e=std::abs(int((incident>>(8*c))&255)-int((path.incoming>>(8*c))&255));
                totals.max_rgb=std::max(totals.max_rgb,e);check(e<=1,"SCENE incident radiance is tinted or stale");}
            if(ground && lobe) {const unsigned first=count*11+id*lobes*13;
                for(unsigned c=0;c<13;++c)check(actual[at+c]==actual[first+c],"SCENE invented a rough floor ray");++n.duplicates;}
            ++totals.records;totals.finite+=kind==1;totals.escape+=kind==2;totals.budget+=kind==3;totals.hops+=path.control&7;
            if(kind==1)totals.max_bary_allowance=std::max(totals.max_bary_allowance,path.bary_allowance);
        }
        ++totals.checked;if(!valid)++totals.clear;else {++totals.conductors[p.dimensions[3]];totals.rough+=lobes==8;}
    }
    ++totals.cases;
}
void run_scene_recovery(Source& source,NativeRayOwner& rays);
void run_scene_history(Source& source,NativeRayOwner& rays) {
    SceneHistoryCounts n;constexpr std::array<unsigned,4> mapping{17,9,6,12};
    for(unsigned material:{1U,2U})for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})for(float roughness:{0.f,.65f})
    for(unsigned metallic=0;metallic<4;++metallic)for(unsigned context=0;context<6;++context)for(float strength:{0.f,.7f,1.f})for(unsigned mode=0;mode<3;++mode) {
        auto input=cube_input(16,variant,encoding);auto p=cube_parameters(input,encoding,0,6);
        p.camera={context==5?13.f:46.f,context==5?11.f:39.f,31.3f,22.7f};p.dimensions={width,height,3,metallic};
        p.environment[0]=context%2?0xff29b50d:0xffa035e0;p.settings={roughness,1,0,0};p.material_info[3]=mode==2;
        p.primary_range={1,65536,1,0};if(context==3)p.primary_range={250,500,1,0};if(context==4)p.primary_range={450,800,1,0};
        p.point={0,180,0,0};p.normal={0,-1,0,0};p.row0={1,0,0,29};p.row1={0,1,0,-3};p.row2={0,0,1,17};
        if(context==0){p.cube_info={};input.cube.clear();input.size=0;}
        if(context==1) {const float c=std::cos(.13f),s=std::sin(.13f);p.normal={s,-c,0,0};p.row0={c,-s,0,29};p.row1={s,c,0,-3};}
        if(context==2) {p.row0={.984f,.1f,.02f,29};p.row1={-.12f,.99f,.07f,-3};p.row2={.015f,-.06f,1.012f,17};}
        if(context==5) {for(unsigned i=0;i<12;++i)input.source.source.geometry.vertices[i][2]=i<6?400.f:-200.f;
            constexpr std::array<std::array<float,2>,6> rect{{{-300,-220},{300,-220},{300,220},{-300,-220},{300,220},{-300,220}}};
            for(unsigned i=0;i<6;++i){auto& v=input.source.source.geometry.vertices[i+6];v[0]=rect[i][0];v[1]=rect[i][1];}}
        reflected_liquid_oracle::Frame frame;frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};
        frame.rotation={p.row0[0],p.row0[1],p.row0[2],p.row1[0],p.row1[1],p.row1[2],p.row2[0],p.row2[1],p.row2[2]};frame.offset={29,-3,17};
        if(context==2) {frame.point=native_forward(sub(V{0,180,0},frame.offset),frame);frame.normal=unit(reflected_liquid_oracle::col({0,-1,0},frame));
            p.point={float(frame.point[0]),float(frame.point[1]),float(frame.point[2]),0};p.normal={float(frame.normal[0]),float(frame.normal[1]),float(frame.normal[2]),0};
            frame.point={p.point[0],p.point[1],p.point[2]};frame.normal={p.normal[0],p.normal[1],p.normal[2]};}
        p.water={1.3f,strength,.72f,float(material|(mode==1?16:0))};
        RayWater water{p.water[0],strength,material,mode==1};water.brightness=p.water[2];
        std::copy(frame.rotation.begin(),frame.rotation.end(),water.world_to_view.begin());water.camera_position={29,-3,17};
        ReceiverPlane plane{{p.point[0],p.point[1],p.point[2]},{p.normal[0],p.normal[1],p.normal[2]}};
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,4,true};
        history.model_lobes=roughness>0?8:1;history.model_paths=history.scene_paths=true;
        history.previous_ground=RayReflectionGround{{0,190,0},{0,-1,0}};
        auto old=input.source.source.geometry.vertices;for(auto& vertex:old)vertex[3]=context%2?0:1;
        source.set(input,old,mapping,history);const Camera camera{width,height,p.camera[0],p.camera[2],p.camera[3],p.camera[1]};
        const PrimaryRayRange range{p.primary_range[0],p.primary_range[1]};
        const std::array<float,9> rotation{p.cube_row0[0],p.cube_row0[1],p.cube_row0[2],p.cube_row1[0],p.cube_row1[1],p.cube_row1[2],p.cube_row2[0],p.cube_row2[1],p.cube_row2[2]};
        check(source.render(rays,input,camera,encoding,&history,roughness,metallic,mode==2,range,rotation,&water,plane,p.environment[0]),rays.status().c_str());
        check(rays.reflection_output().reflection_history==*native_reflection_history(width,height,history.extent,true,{},history.model_lobes,true,true),"Wrong SCENE output layout");
        const auto actual=source.get(rays.reflection_output());
        try {verify_scene_history(input,p,frame,history,actual,n);}catch(const std::exception& e){throw std::runtime_error(std::string(e.what())
            +" material/encoding/variant/roughness/metal/context/strength/mode="+std::to_string(material)+"/"+std::to_string(encoding)+"/"+std::to_string(variant)
            +"/"+std::to_string(roughness)+"/"+std::to_string(metallic)+"/"+std::to_string(context)+"/"+std::to_string(strength)+"/"+std::to_string(mode));}
        check(source.render(rays,input,camera,encoding,nullptr,roughness,metallic,mode==2,range,rotation,&water,plane,p.environment[0]),rays.status().c_str());
        const auto ordinary=source.get(rays.reflection_output());
        const auto changed=std::mismatch(ordinary.begin(),ordinary.end(),actual.begin());
        if(changed.first!=ordinary.end())throw std::runtime_error("SCENE history changed current RGBA pixel/case="+std::to_string(changed.first-ordinary.begin())
            +"/"+std::to_string(n.paths.cases)+" plain/history="+std::to_string(*changed.first)+"/"+std::to_string(*changed.second));
        check(!rays.reflection_output().reflection_history.storage_bytes,"SCENE history OFF retained stale records");
    }
    const auto& t=n.paths;
    std::cout<<"Native SCENE history: cases="<<t.cases<<" checked/clear="<<t.checked<<'/'<<t.clear<<" ground/models="<<n.ground<<'/'<<n.models
        <<" path records="<<t.records<<" finite/escape/budget="<<t.finite<<'/'<<t.escape<<'/'<<t.budget<<" hops/ground="<<t.hops<<'/'<<n.ground_hops
        <<" zero-reflection floors="<<n.zero_strength<<" shadowed/unshadowed="<<n.shadowed<<'/'<<n.unshadowed<<" exact floor duplicates="<<n.duplicates
        <<" boundary exclusions="<<t.edges<<" max RGB/feature/response/base/depth="<<t.max_rgb<<'/'<<t.max_feature<<'/'<<t.max_response<<'/'<<n.max_base<<'/'<<t.max_depth
        <<" max input-derived bary allowance="<<t.max_bary_allowance<<'\n'<<std::flush;
    check(t.cases==5184 && t.checked>1000000 && t.clear>100000 && n.ground>100000 && n.models>100000 && n.ground_hops>10000
        && t.finite>10000 && t.escape>10000 && t.budget>1000 && n.shadowed>100 && n.unshadowed>10000 && n.zero_strength>10000 && n.duplicates>100000,
        "Insufficient actual planar/finite ordered history witnesses");
    std::cout<<"SCENE current RGBA byte-identical ON/OFF, actual planar hops and current base/response separated; NOT Linux/application/reuse/FPS acceptance\n";
    run_scene_recovery(source,rays);
}
void run_scene_recovery(Source& source,NativeRayOwner& rays) {
    constexpr std::array<unsigned,4> mapping{17,9,6,12};unsigned negatives=0,cycles=0;
    auto input=cube_input(16,4,2);auto old=input.source.source.geometry.vertices;for(auto& v:old)v[3]=1;
    const Camera camera{width,height,46,31.3,22.7,39};const ReceiverPlane plane{{0,180,0},{0,-1,0}};
    for(unsigned material:{1U,2U})for(unsigned lobes:{1U,8U}) {
        RayWater water{1.3f,.7f,material,true};
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,4,true};
        history.model_lobes=lobes;history.model_paths=history.scene_paths=true;history.previous_ground=RayReflectionGround{{0,190,0},{0,-1,0}};
        source.set(input,old,mapping,history);const float roughness=lobes==8?.65f:0;
        const auto submit=[&](const RayReflectionHistory& records,const Camera& view,const RayWater* surface,
            std::optional<ReceiverPlane> floor,float rough,unsigned encoding=2,unsigned metal=2) {
            return source.render(rays,input,view,encoding,&records,rough,metal,true,{1,65536},{1,0,0,0,1,0,0,0,1},surface,floor);
        };
        check(submit(history,camera,&water,plane,roughness),rays.status().c_str());const auto reference=source.get(rays.reflection_output());
        for(unsigned failure=0;failure<24;++failure) {
            auto bad=history;auto view=camera;auto surface=water;auto floor=std::optional(plane);float rough=roughness;
            const RayWater* surface_ptr=&surface;unsigned encoding=2,metal=2;
            switch(failure) {
                case 0:bad.model_paths=false;break;
                case 1:bad.separated=false;break;
                case 2:bad.model_lobes=4;break;
                case 3:bad.model_lobes=lobes==8?1:8;break;
                case 4:bad.scene_paths=false;break;
                case 5:surface.material=0;break;
                case 6:surface.material=3;break;
                case 7:surface.source_colour=std::array<float,3>{.2f,.3f,.4f};break;
                case 8:surface.auxiliary_layers=true;break;
                case 9:surface.surface_layers=true;break;
                case 10:surface.reflection_strength=std::numeric_limits<float>::quiet_NaN();break;
                case 11:floor.reset();break;
                case 12:surface_ptr=nullptr;break;
                case 13:bad.previous_index_offset=capacity;break;
                case 14:bad.previous_vertex_offset=0;break;
                case 15:view.width=view.height=16384;break;
                case 16:surface.world_to_view={};break;
                case 17:bad.projection[0]=1.e-300;break;
                case 18:bad.previous_ground->normal={};break;
                case 19:view.focal_length=0;break;
                case 20:encoding=0;break;
                case 21:encoding=3;break;
                case 22:metal=4;break;
                case 23:surface.caustics=4;break;
            }
            check(!submit(bad,view,surface_ptr,floor,rough,encoding,metal),"Malformed/unsupported SCENE history accepted");
            check(!rays.reflection_output().buffer && !rays.reflection_output().reflection_history.storage_bytes,"Rejected SCENE history exposed stale borrowed output");
            check(submit(history,camera,&water,plane,roughness),rays.status().c_str());
            check(source.get(rays.reflection_output())==reference,"SCENE decline recovery changed complete records");++negatives;
        }
        check(rays.native_work_complete() && rays.try_release_device(),"Completed SCENE history cannot retire nonblocking");
        check(!rays.working_image_bytes() && !rays.reflection_output().buffer,"SCENE release retained images");
        check(submit(history,camera,&water,plane,roughness),rays.status().c_str());
        check(source.get(rays.reflection_output())==reference,"SCENE reinitialize changed complete records");++cycles;
    }
    std::cout<<"SCENE malformed/unsupported/range declines="<<negatives<<", exact record recoveries="<<negatives<<", release/reinitialize cycles="<<cycles<<'\n';
    rays.release_device();check(!rays.working_image_bytes() && rays.native_work_complete(),"SCENE final release retained resources");
    std::cout<<"SCENE exact malformed-input recovery and cleanup; NOT a substitute for the full scene matrix/application acceptance\n";
}
