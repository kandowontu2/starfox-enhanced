// Independent binary64 FORWARD optics supplies known old feature positions.
// This oracle never runs the shader's inverse solver on the CPU.
void reflection_lobe_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=23,h=17,count=w*h;const bool use_srgb=srgb(format);
    using V=std::array<double,3>;
    const auto add=[](V a,V b){return V{a[0]+b[0],a[1]+b[1],a[2]+b[2]};};
    const auto scale=[](V a,double n){return V{a[0]*n,a[1]*n,a[2]*n};};
    const auto dot=[](V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    const auto cross=[](V a,V b){return V{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    const auto unit=[&](V a){return scale(a,1/std::sqrt(dot(a,a)));};
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    const auto feature=[&](double x,double y,double rough,unsigned lobe) {
        const V direction=unit({(x-11.5)/28,(y-8.5)/31,1});const double t=400/direction[2];
        const V normal{0,0,-1},origin=add(scale(direction,t),scale(normal,std::max(.01,t*1.e-5)));
        const V reflected{direction[0],direction[1],-direction[2]};
        const V tangent=unit(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
        V outgoing=unit(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),rough*rough)));
        if(dot(outgoing,normal)<=0) outgoing=reflected;
        const V hit=add(origin,scale(outgoing,(200-origin[2])/outgoing[2]));
        const double u=(hit[0]+1024)/3072,v=(hit[1]+1024)/3072;
        check(u>=0 && v>=0 && u+v<=1,"Independent forward feature left finite fixture triangle");
        const unsigned a=unsigned(std::lround(u*65535)),b=std::min(unsigned(std::lround(v*65535)),65535-a);
        return a|(b<<16);
    };
    const auto linear=[&](unsigned word,unsigned c) {const double v=((word>>(c*8))&255)/255.;return use_srgb?decode(v):v;};
    const auto pack=[&](std::array<double,3> colour) {
        unsigned result=0xff000000;
        for(unsigned c=0;c<3;++c) {double v=std::clamp(colour[c],0.,1.);if(use_srgb) v=encode(v);result|=unsigned(std::lround(v*255))<<(8*c);}
        return result;
    };
    const auto get=[](const Bytes& bytes,unsigned at) {unsigned value;std::memcpy(&value,bytes.data()+at,4);return value;};
    unsigned reused=0,changed=0,protected_count=0,lobe_failures=0;
    for(float rough:{0.F,.04F,.20F,.35F,.62F,1.F}) {
        const unsigned lobes=rough>0?8:1,triangles=lobes+1,vertex_count=triangles*3;
        const auto layout=*render::shadows::native_reflection_history(w,h,{w,h},true,{},lobes);
        Bytes seed(layout.storage_bytes),input(layout.storage_bytes),owners(count*4,255);
        const unsigned mapping=16+vertex_count*16;Bytes vertices(mapping+triangles*4);
        render::shadows::RayReflectionHistory correspondence{16,{w,h},{28,31,11.5,8.5},1,65536,mapping,true};correspondence.model_lobes=lobes;
        // Current primitive order is REVERSED relative to accepted cache IDs.
        for(unsigned primitive=0;primitive<triangles;++primitive) {
            const unsigned accepted=triangles-1-primitive;word(vertices,mapping+primitive*4,accepted);
            const float z=accepted?200.F:400.F;
            const float xyz[3][4]{{-1024,-1024,z,1},{2048,-1024,z,1},{-1024,2048,z,1}};
            std::memcpy(vertices.data()+16+primitive*48,xyz,48);
        }
        const auto response=[](unsigned p,bool now) {
            return now?std::array<float,4>{.35F+float(p%7)*.03F,.766F,.336F,1}:
                std::array<float,4>{.9F,.2F,.1F,1};
        };
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned p=y*w+x;auto* owner=owners.data()+p*4;owner[0]=255;owner[1]=128;owner[2]=2;
            if(x==3 && y>2 && y<h-3) {owner[3]=0;++protected_count;}
            std::array<double,3> sums[2]{};
            for(unsigned frame=0;frame<2;++frame) {
                auto& bytes=frame?input:seed;const auto material=response(p,bool(frame));
                word(bytes,count*4+p*4,frame?triangles-1:0);
                const float depth=frame?550.F:400.F;std::memcpy(bytes.data()+count*8+p*4,&depth,4);
                std::memcpy(bytes.data()+count*12+p*16,material.data(),16);
                for(unsigned lobe=0;lobe<lobes;++lobe) {
                    const unsigned base=count*28+(p*lobes+lobe)*12;
                    unsigned secondary=frame?triangles-2-lobe:lobe+1;
                    // Real per-lobe visibility disagreement. It must not veto
                    // other independent lobes or borrow their colour instead.
                    if(!frame && x==10 && lobe==0) {secondary=UINT32_MAX;++lobe_failures;}
                    word(bytes,base,secondary);word(bytes,base+4,feature(x+(frame?.75:.5),y+(frame?.75:.5),rough,lobe));
                    const unsigned level=frame?((x+y+lobe)%3)*90+35:125+(x%3)*15;
                    const unsigned incident=0xff000000U|level|((frame?220-level/2:105+y*2)<<8)|((45+lobe*17)<<16);
                    word(bytes,base+8,incident);
                    for(unsigned c=0;c<3;++c) sums[frame][c]+=linear(incident,c)/lobes;
                }
                for(unsigned c=0;c<3;++c) sums[frame][c]*=material[c];word(bytes,p*4,pack(sums[frame]));
            }
        }
        render::GpuCalibratedReflectionHistory resolver;check(resolver.initialize(gpu.device),resolver.status().c_str());
        const auto run=[&](const Bytes& bytes,bool old,bool accept,bool reuse) {
            Texture ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);gpu.begin();ownership.upload(owners);
            Buffer source(gpu,bytes),geometry(gpu,vertices);gpu.submit();
            render::shadows::GpuReflectionOutput rays{gpu.device,source.buffer,w,h,w*4,{},layout};
            const render::CalibratedReflectionLobeGeometry guide{geometry.buffer,unsigned(vertices.size()),vertex_count,correspondence,rough};
            gpu.begin();
            if(!old) {
                for(unsigned fault=0;fault<8;++fault) {
                    auto invalid=guide;
                    if(fault==0) invalid.buffer=source.buffer;
                    if(fault==1) invalid.bytes-=4;
                    if(fault==2) invalid.vertex_count-=1;
                    if(fault==3) invalid.history.previous_index_offset=0;
                    if(fault==4) invalid.history.model_lobes=lobes==8?1:8;
                    if(fault==5) invalid.roughness=std::numeric_limits<float>::quiet_NaN();
                    if(fault==6) invalid.history.extent[0]+=1;
                    if(fault==7) invalid.history.model_paths=true;
                    check(!resolver.enqueue(gpu.command,rays,ownership.texture,{42,.85F,use_srgb},&invalid),"Malformed compact geometry accepted");
                }
            }
            check(resolver.enqueue(gpu.command,rays,ownership.texture,{reuse?42U:43U,.85F,use_srgb},&guide),resolver.status().c_str());
            const auto candidate=resolver.output();gpu.submit();const auto actual=read_reflection_history(gpu,candidate);
            auto expected=bytes;
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x;const bool eligible=owners[p*4+3]!=0;
                if(!eligible) word(expected,count*4+p*4,UINT32_MAX);
                std::array<double,3> sum{};bool blended=false;
                for(unsigned lobe=0;lobe<lobes;++lobe) {
                    const unsigned at=count*28+(p*lobes+lobe)*12;const unsigned incident=get(bytes,at+8);
                    std::array<double,3> rgb{};for(unsigned c=0;c<3;++c) rgb[c]=linear(incident,c);
                    if(!eligible || get(bytes,at)==UINT32_MAX) word(expected,at,UINT32_MAX);
                    // Known source centres .75 give a .25/.25 bilinear query.
                    // Packed barycentrics move that point by <.007 pixel here;
                    // RGB tolerance two includes that quantified input rounding.
                    bool valid=!old && reuse && eligible && x<w-1 && y<h-1;
                    std::array<double,3> prior{};
                    for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                        const unsigned q=std::min(y+dy,h-1)*w+std::min(x+dx,w-1),qa=count*28+(q*lobes+lobe)*12;
                        if(owners[q*4+3]==0 || get(seed,qa)!=lobe+1) valid=false;
                        for(unsigned c=0;c<3;++c) prior[c]+=linear(get(seed,qa+8),c)*(dx?.25:.75)*(dy?.25:.75);
                    }
                    if(valid) {
                        auto low=rgb,high=rgb;
                        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                            const unsigned q=unsigned(std::clamp(int(y)+dy,0,int(h)-1))*w+unsigned(std::clamp(int(x)+dx,0,int(w)-1));
                            if(owners[q*4+3]==0) continue;
                            for(unsigned c=0;c<3;++c) {const double value=linear(get(bytes,count*28+(q*lobes+lobe)*12+8),c);low[c]=std::min(low[c],value);high[c]=std::max(high[c],value);}
                        }
                        for(unsigned c=0;c<3;++c) rgb[c]=std::lerp(rgb[c],std::clamp(prior[c],low[c],high[c]),.85F);
                        word(expected,at+8,pack(rgb));++reused;blended=true;
                    }
                    for(unsigned c=0;c<3;++c) sum[c]+=rgb[c]/lobes;
                }
                if(blended) {const auto material=response(p,true);for(unsigned c=0;c<3;++c) sum[c]*=material[c];word(expected,p*4,pack(sum));}
            }
            for(unsigned p=0;p<count;++p) {
                if(actual[p*4]!=bytes[p*4]) ++changed;
                for(unsigned c=0;c<4;++c) check(std::abs(int(actual[p*4+c])-int(expected[p*4+c]))<=(c==3?0:2),"Compact final colour oracle mismatch");
            }
            for(unsigned at=count*4;at<actual.size();++at) {
                const bool colour=at>=count*28 && (at-count*28)%12>=8 && (at-count*28)%12<11;
                if(std::abs(int(actual[at])-int(expected[at]))>(colour?2:0)) {
                    std::cerr<<"Compact lobe oracle rough="<<rough<<" old="<<old<<" byte="<<at
                        <<" actual="<<unsigned(actual[at])<<" expected="<<unsigned(expected[at])<<'\n';
                    throw std::runtime_error("Compact per-lobe identity/current response/incident oracle mismatch");
                }
            }
            if(accept) resolver.commit();else resolver.discard();return actual;
        };
        const auto accepted=run(seed,true,true,true);
        const auto held=resolver.accepted_output();const auto resolved=run(input,false,false,true);
        check(resolved!=input && resolver.accepted_output().buffer==held.buffer && read_reflection_history(gpu,held)==accepted,
            "Compact candidate silently bypassed history or replaced accepted data on discard");
        (void)run(input,false,false,false); // epoch cut: must keep all current colour bytes.
        check(read_reflection_history(gpu,held)==accepted,"Compact epoch cut changed accepted image before commit");
    }
    check(reused>1000 && changed>100 && protected_count && lobe_failures,"Compact resolver exercised insufficient per-lobe colour/rejection coverage");
    std::cout<<"Compact lobe resolver: "<<reused<<" independently known optical samples, "<<changed
        <<" changed RGB pixels; six roughnesses, 1/8 lobes, reordered IDs, per-lobe misses, protected receivers, current response, epoch/discard and malformed geometry passed.\n";
}
