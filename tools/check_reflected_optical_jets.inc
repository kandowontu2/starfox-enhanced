// Independent binary64 forward evaluations validate GPU value/derivative
// enclosures. Sampling is an implementation check, not the certificate's basis.
// The local root classifier itself uses only closed-box interval arithmetic.
struct JetFixture {o::Inputs input;o::Path path;OpticalFrame frame;unsigned family{},hops{},lobe{};};
o::Plane jet_binary32_plane(o::Plane plane) {
    for(auto* vertex:{&plane.a,&plane.b,&plane.c})for(auto& coordinate:*vertex)coordinate=float(coordinate);
    return plane;
}
o::V jet_reference_residual(const JetFixture& f,double x,double y,unsigned axis) {
    const auto ray=o::forward(f.input,f.path,x,y);require(ray.valid,"Differential enclosure admitted an invalid physical ray");
    require(axis<3 && std::abs(ray.outgoing[axis])>1.e-12,"Residual's omitted axis is not stable in the independent map");
    const auto travel=o::unit(f.path.finite_terminal?o::sub(f.path.terminal,ray.origin):f.path.terminal);
    require(o::dot(travel,ray.outgoing)>0,"Differential enclosure admitted a reversed terminal");
    return o::scale(o::cross(travel,ray.outgoing),std::max(f.input.liquid.projection[0],f.input.liquid.projection[1]));
}
std::array<unsigned,2> jet_coordinates(unsigned axis) {return {(axis+1)%3,(axis+2)%3};}
bool jet_contains(const std::array<float,2>& bound,double value) {
    return std::isfinite(bound[0]) && std::isfinite(bound[1]) && bound[0]<=value && value<=bound[1];
}
std::optional<std::array<double,2>> jet_reference_root(const JetFixture& f,double x,double y,unsigned axis) {
    const auto components=jet_coordinates(axis);
    const auto residual=[&](double a,double b){const auto r=jet_reference_residual(f,a,b,axis);return std::array{r[components[0]],r[components[1]]};};
    for(unsigned iteration=0;iteration<12;++iteration) {
        const auto e=residual(x,y);if(std::max(std::abs(e[0]),std::abs(e[1]))<1.e-10)return std::array{x,y};
        constexpr double h=1.e-4;const auto ax=residual(x-h,y),bx=residual(x+h,y),ay=residual(x,y-h),by=residual(x,y+h);
        const double a=(bx[0]-ax[0])/(2*h),b=(by[0]-ay[0])/(2*h),c=(bx[1]-ax[1])/(2*h),d=(by[1]-ay[1])/(2*h),det=a*d-b*c;
        if(!std::isfinite(det) || std::abs(det)<1.e-12)return {};
        x-=(d*e[0]-b*e[1])/det;y-=(-c*e[0]+a*e[1])/det;
        if(!std::isfinite(x) || !std::isfinite(y))return {};
    }
    return {};
}
JetFixture jet_fixture(unsigned family,unsigned variant) {
    JetFixture f;f.family=family;auto& i=f.input;auto& p=f.path;auto& l=i.liquid;
    l.width=17;l.height=13;l.point={0,8,0};l.normal={0,-1,0};l.near=1;l.far=8000;l.projection={48,48,8.5,6.5};
    l.time=float(.9);l.material=family==2?3:0;i.current_projection=l.projection;
    constexpr double sx=7.75,sy=10.75;
    if(family==2) {
        auto flat=l;flat.material=0;const auto base=reflected_liquid_oracle::optical(flat,sx,sy);require(base.valid,"Invalid differential dome base");
        bool placed=false;
        for(int z=-3;z<=3 && !placed;++z)for(int x=-3;x<=3 && !placed;++x) {
            const double seed=reflected_liquid_oracle::hash(x,z),phase=l.time*.14+seed*7-std::floor(l.time*.14+seed*7);
            if(seed>.7 && phase>.2 && phase<.8) {
                l.offset[0]=float(128*(x+.28+.44*reflected_liquid_oracle::hash(x+19,z))-base.hit[0]);
                l.offset[2]=float(128*(z+.28+.44*reflected_liquid_oracle::hash(x,z+29))-base.hit[2]);placed=true;
            }
        }
        require(placed,"Differential fixture has no active dome");
    }
    if(family==0 || variant>=5) {
        p.receiver=jet_binary32_plane(authored_plane({0,0,40},{0,0,1},300));
        p.lobe=family==0?variant?variant-1:0:variant-5;p.roughness=variant?double(float(.15)):0;
        if(family!=0){p.hops=1;p.liquid_mask=1;}
    }else {
        p.liquid_primary=true;
        for(unsigned h=0;h<variant;++h) {
            const auto ray=o::forward(i,p,sx,sy);require(ray.valid,"Incomplete authored differential path");
            if(h%2)p.liquid_mask|=1U<<h;
            else p.mirrors[h]=jet_binary32_plane(authored_plane(o::add(ray.origin,o::scale(ray.outgoing,64)),ray.outgoing,300));
            ++p.hops;
        }
    }
    const auto ray=o::forward(i,p,sx,sy);require(ray.valid,"Authored full differential path invalid");
    p.finite_terminal=p.hops<4 && variant%2;
    p.terminal=p.finite_terminal?o::add(ray.origin,o::scale(ray.outgoing,128)):ray.outgoing;
    for(auto& c:p.terminal)c=float(c);
    f.frame=optical_frame(i,p);if(p.hops==4)f.frame.control[3]=3;
    if(family==0)f.frame.liquid={}; // Planar paths need no fake liquid frame.
    f.hops=p.hops;f.lobe=p.lobe;return f;
}
[[maybe_unused]] void jet_checks(Gpu& gpu) {
    std::vector<JetFixture> fixtures;
    for(unsigned family=0;family<3;++family)for(unsigned variant=0;variant<(family?13U:9U);++variant)fixtures.push_back(jet_fixture(family,variant));
    const unsigned axis_start=unsigned(fixtures.size());
    // Exact raw axis faces, both windings, and a one-ULP nonaxis neighbour:
    // binary64 forward/value/Jacobian checks must still enclose every case.
    // No epsilon axis test or reference normal is uploaded to the shader.
    for(unsigned axis=0;axis<3;++axis)for(bool reversed:{false,true})for(bool tilted:{false,true}) {
        auto f=jet_fixture(0,0);o::V normal{},point{};normal[axis]=1;
        point[axis]=axis==0?-4:axis==1?4:40;
        f.path.receiver=jet_binary32_plane(authored_plane(point,normal,300));
        if(reversed)std::swap(f.path.receiver.b,f.path.receiver.c);
        if(tilted)f.path.receiver.b[axis]=std::nextafter(float(f.path.receiver.b[axis]),INFINITY);
        const auto ray=o::forward(f.input,f.path,7.75,10.75);
        require(ray.valid,"Raw axis/one-ULP-tilt differential fixture invalid");
        f.path.terminal=ray.outgoing;for(auto& c:f.path.terminal)c=float(c);
        f.frame=optical_frame(f.input,f.path);fixtures.push_back(f);
    }
    std::vector<JetInput> boxes;
    // Refine the diagnostic boxes, not the arithmetic or optical tolerances.
    // Retain the original four extents alongside three intermediate/tighter
    // closed boxes. No unresolved box is counted as a positive certificate.
    constexpr std::array<float,7> radii{0.F,1.F/1024,1.F/256,1.F/128,1.F/64,1.F/16,1.F/4};
    for(const auto& f:fixtures)for(float radius:radii)boxes.push_back({f.frame,{7.75F-radius,10.75F-radius,7.75F+radius,10.75F+radius}});
    const auto outputs=gpu.jet_boxes(boxes);require(outputs.size()==boxes.size(),"Incomplete differential output batch");
    unsigned valid=0,unique=0,no_root=0,unknown=0,samples=0,derivatives=0;
    std::array<unsigned,3> family_valid{},family_unique{};std::array<std::array<bool,5>,2> hops{};
    std::array<bool,12> axis_valid{};
    std::array<std::array<bool,8>,3> lobes{};
    for(unsigned n=0;n<outputs.size();++n) {
        const auto& f=fixtures[n/radii.size()];const auto& input=boxes[n];const auto& result=outputs[n];
        if(!result.status[0]) {
            require(!result.status[1],"Unknown differential frame retained a local certificate");++unknown;
            if(n%radii.size()==1)std::cout<<"Unresolved box fixture="<<n/radii.size()<<" family="<<f.family<<" hops="<<f.hops
                <<" lobe="<<f.lobe<<" refusal="<<result.status[3]<<'\n';
            continue;
        }
        require(result.status[0]==1 && result.status[1]<=2 && result.status[2]<3 && !result.status[3] && result.box==input.box,
            "Malformed differential result descriptor");
        ++valid;++family_valid[f.family];if(f.family)hops[f.family-1][f.hops]=true;lobes[f.family][f.lobe]=true;
        if(n/radii.size()>=axis_start)axis_valid[n/radii.size()-axis_start]=true;
        if(n%radii.size()==1)std::cout<<"Local box fixture="<<n/radii.size()<<" family="<<f.family<<" hops="<<f.hops<<" lobe="<<f.lobe
            <<" status="<<result.status[1]<<" contraction="<<result.settings[0]<<" enclosure="
            <<result.enclosure[0]<<','<<result.enclosure[1]<<','<<result.enclosure[2]<<','<<result.enclosure[3]<<'\n';
        const auto coordinates=jet_coordinates(result.status[2]);
        const double span=std::max(double(input.box[2]-input.box[0]),double(input.box[3]-input.box[1]));
        const unsigned steps=span?5:1;
        for(unsigned yy=0;yy<steps;++yy)for(unsigned xx=0;xx<steps;++xx) {
            const double x=input.box[0]+double(input.box[2]-input.box[0])*(steps==1?.5:(double(xx)+.5)/steps);
            const double y=input.box[1]+double(input.box[3]-input.box[1])*(steps==1?.5:(double(yy)+.5)/steps);
            const auto ray=o::forward(f.input,f.path,x,y);require(ray.valid,"Bounded physical branch contradicted independent forward map");
            const std::array<double,8> values{ray.origin[0],ray.origin[1],ray.origin[2],ray.outgoing[0],ray.outgoing[1],ray.outgoing[2],ray.depth,ray.bias};
            for(unsigned c=0;c<8;++c)require(jet_contains(result.ray[c],values[c]),"GPU optical value bounds omitted independent forward geometry");
            const auto r=jet_reference_residual(f,x,y,result.status[2]);
            for(unsigned c=0;c<2;++c)require(jet_contains(result.residual[c],r[coordinates[c]]),"GPU residual enclosure omitted independent forward residual");
            ++samples;
            const double h=span?std::min(1.e-4,span*.002):1.e-4;
            for(unsigned axis=0;axis<2;++axis) {
                const auto eval=[&](double offset){return jet_reference_residual(f,x+(axis==0?offset:0),y+(axis==1?offset:0),result.status[2]);};
                const auto a=eval(-2*h),b=eval(-h),c=eval(h),d=eval(2*h);
                for(unsigned component=0;component<2;++component) {
                    const unsigned coordinate=coordinates[component];
                    const double derivative=(a[coordinate]-8*b[coordinate]+8*c[coordinate]-d[coordinate])/(12*h);
                    if(!jet_contains(result.jacobian[component*2+axis],derivative)) {
                        std::cerr<<"Derivative miss fixture="<<n/radii.size()<<" family="<<f.family<<" hops="<<f.hops<<" lobe="<<f.lobe
                            <<" radius="<<span/2<<" component="<<component<<" axis="<<axis<<" reference="<<derivative
                            <<" bounds="<<result.jacobian[component*2+axis][0]<<','<<result.jacobian[component*2+axis][1]<<'\n';
                        require(false,"GPU Jacobian enclosure omitted independent five-point derivative");
                    }
                    ++derivatives;
                }
            }
        }
        const auto centre=jet_reference_residual(f,result.settings[1],result.settings[2],result.status[2]);
        for(unsigned c=0;c<2;++c)require(jet_contains(result.centre_residual[c],centre[coordinates[c]]),"Midpoint residual enclosure missed reference");
        const auto root=jet_reference_root(f,7.75,10.75,result.status[2]);
        require(root.has_value(),"Independent reference could not refine the authored physical root");
        const bool root_inside=(*root)[0]>=input.box[0] && (*root)[0]<=input.box[2] && (*root)[1]>=input.box[1] && (*root)[1]<=input.box[3];
        if(result.status[1]==1) {
            ++unique;++family_unique[f.family];
            require(std::isfinite(result.settings[0]) && result.settings[0]<1 && result.enclosure[0]>input.box[0]
                && result.enclosure[1]>input.box[1] && result.enclosure[2]<input.box[2] && result.enclosure[3]<input.box[3],
                "Local unique-root classifier lacks strict contraction/inclusion");
            require(root_inside && (*root)[0]>=result.enclosure[0] && (*root)[0]<=result.enclosure[2]
                && (*root)[1]>=result.enclosure[1] && (*root)[1]<=result.enclosure[3],"Local certificate omitted the independently refined physical root");
        }else if(result.status[1]==2){++no_root;require(!root_inside,"Local no-root classifier discarded an independent physical root");}
    }
    for(unsigned family=0;family<3;++family) {
        std::cout<<"Differential family="<<family<<" valid="<<family_valid[family]<<" unique="<<family_unique[family]<<'\n';
        require(family_valid[family] && family_unique[family],"An optical family has no positive differential/local-root case");
        for(unsigned lobe=0;lobe<8;++lobe) {
            if(!lobes[family][lobe])std::cerr<<"Missing physical differential coverage family="<<family<<" lobe="<<lobe<<'\n';
            require(lobes[family][lobe],"Differential fixtures failed to exercise a physical rough lobe");
        }
    }
    for(unsigned family=0;family<2;++family)for(unsigned hop=0;hop<5;++hop) {
        if(!hops[family][hop])std::cerr<<"Missing physical differential coverage family="<<family+1<<" hops="<<hop<<'\n';
        require(hops[family][hop],"Differential fixtures failed to exercise a finite/liquid path depth");
    }
    for(bool covered:axis_valid)require(covered,"Exact-axis, reversed or one-ULP-tilted face lacks complete physical differential coverage");
    const auto stable=gpu.jet_boxes(boxes);require(std::memcmp(outputs.data(),stable.data(),outputs.size()*sizeof(JetResult))==0,"Held optical differential inputs changed outputs");
    auto bad=boxes.front();std::vector<JetInput> faults;
    bad.frame.receiver[0][3]=0;faults.push_back(bad);bad=boxes.front();bad.frame.receiver[5][1]=8;faults.push_back(bad);
    bad=boxes.front();bad.frame.terminal[0]=NAN;faults.push_back(bad);bad=boxes.front();bad.box[0]=NAN;faults.push_back(bad);
    bad=boxes.front();bad.box[2]=-1;faults.push_back(bad);
    bad=boxes[9*radii.size()];bad.frame.liquid[5][0]+=1;faults.push_back(bad);
    bad=boxes[9*radii.size()];bad.box={1,1,16,12};faults.push_back(bad); // Crosses a primary denominator/physical branch.
    const auto refusals=gpu.jet_boxes(faults);for(const auto& result:refusals)require(!result.status[0] && !result.status[1],"Invalid/branch-crossing differential input published a root certificate");
    // These boxes are authored independently of the downloaded certificates.
    // A local existence result must not be substituted for global uniqueness.
    std::vector<JetInput> displaced;
    for(unsigned variant=0;variant<9;++variant) {
        const auto& f=fixtures[variant];
        for(float shift:{-.5F,.5F})displaced.push_back({f.frame,
            {7.75F+shift-1.F/64,10.75F-1.F/64,7.75F+shift+1.F/64,10.75F+1.F/64}});
    }
    const auto absent=gpu.jet_boxes(displaced);unsigned absent_certificates=0;
    for(unsigned n=0;n<absent.size();++n) {
        const auto& result=absent[n];require(result.status[1]!=1,"Displaced physical box manufactured a local root");
        if(result.status[1]==2) {
            ++absent_certificates;require(result.status[0]==1,"No-root status lacks complete physical bounds");
            require(result.enclosure[2]<displaced[n].box[0] || result.enclosure[0]>displaced[n].box[2]
                || result.enclosure[3]<displaced[n].box[1] || result.enclosure[1]>displaced[n].box[3],
                "Local no-root classification lacks a disjoint fixed-point enclosure");
        }
    }
    require(absent_certificates,"No negative local-root case was actually exercised");
    auto boundary=fixtures.front();boundary.path.terminal={0,0,-1};boundary.frame=optical_frame(boundary.input,boundary.path);
    const std::vector<JetInput> shared_edge{{boundary.frame,{8.5F-1.F/64,6.5F-1.F/64,8.5F,6.5F+1.F/64}},
        {boundary.frame,{8.5F,6.5F-1.F/64,8.5F+1.F/64,6.5F+1.F/64}},
        {boundary.frame,{8.5F,6.5F,8.5F,6.5F}}};
    const auto edge_results=gpu.jet_boxes(shared_edge);
    for(const auto& result:edge_results)require(result.status[0]==1 && result.status[1]==0,
        "Shared-boundary/zero-area root was falsely certified or excluded");
    std::cout<<"GPU optical differential/local-root checks: "<<fixtures.size()<<" canonical planar/water/lava paths, 0-4 ordered finite/liquid hops, all eight rough lobes; "
        <<valid<<'/'<<boxes.size()<<" physical boxes, "<<samples<<" independent forward samples, "<<derivatives<<" five-point derivative checks; "
        <<unique<<" local unique-root enclosures, "<<no_root<<" local no-root boxes, "<<unknown<<" unresolved physical branches; "
        <<absent_certificates<<" displaced no-root certificates, three shared-boundary/zero-area refusals, held/malformed checks passed.\n"
        <<"Local certificates ONLY: no complete global source cover, native-owner integration, old RGB reuse or frame-rate qualification.\n";
}
#if defined(STARFOX_OPTICAL_ARITHMETIC_TRACE_ONLY)
unsigned jet_arithmetic_checks(Gpu& gpu) {
    const auto primary=jet_fixture(2,0);std::vector<JetInput> inputs;
    for(float radius:{0.F,1.F/1024,1.F/64})inputs.push_back({primary.frame,
        {7.75F-radius,10.75F-radius,7.75F+radius,10.75F+radius}});
    // Keep the physical frame; vary only its world offset to distinguish a
    // checked arithmetic refusal from a hash/phase boundary. These are not
    // solved/source/colour seeds, and no result is uploaded into the player.
    for(float shift:{-16.F,16.F}) {
        auto frame=primary.frame;frame.liquid[4][3]+=shift;
        inputs.push_back({frame,{7.75F,10.75F,7.75F,10.75F}});
    }
    const auto results=gpu.jet_boxes(inputs);unsigned failures=0;
    for(unsigned n=0;n<results.size();++n) {
        const auto& r=results[n];
        std::cout<<"Primary-lava arithmetic trace case="<<n<<" valid="<<r.status[0]<<" arithmetic="<<r.status[1]
            <<" branch="<<r.status[2]<<" first-site="<<r.status[3]<<" arguments="<<std::hexfloat;
        for(float v:r.settings)std::cout<<v<<',';
        std::cout<<std::defaultfloat<<" bits="<<std::hex;
        for(float v:r.settings)std::cout<<std::bit_cast<unsigned>(v)<<',';
        std::cout<<std::dec<<'\n';
        failures+=unsigned(!r.status[0]);
    }
    std::cout<<"Primary-only arithmetic trace: "<<failures<<" refusals. This does NOT qualify the full map, source history or RGB reuse.\n";
    return failures;
}
#endif
