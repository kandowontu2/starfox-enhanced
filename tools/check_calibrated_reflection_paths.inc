// Synthetic cache/ownership differential, independent binary64 FORWARD optics.
// The separate native producer fixture verifies real closest-hit RT witnesses.
// Here ordered path domains supply a known guide WITHOUT a CPU inverse solve.
void reflection_path_tests(Gpu& gpu,SDL_GPUTextureFormat format,bool scene_paths=false,bool ground_primary=false) {
    constexpr unsigned w=23,h=17,count=w*h;const bool use_srgb=srgb(format);
    using V=std::array<double,3>;
    const auto add=[](V a,V b){return V{a[0]+b[0],a[1]+b[1],a[2]+b[2]};};
    const auto scale=[](V a,double b){return V{a[0]*b,a[1]*b,a[2]*b};};
    const auto dot=[](V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    const auto cross=[](V a,V b){return V{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    const auto unit=[&](V a){return scale(a,1/std::sqrt(dot(a,a)));};
    // Rotate the ENTIRE analytic-primary scene, including finite endpoint
    // faces. This is a real receding plane, not fake cached-depth noise.
    // Its adjacent tap depths differ by more than the original .5% guard;
    // reciprocal-depth interpolation must still reconstruct the known ray.
    const double tilt=ground_primary?std::atan(3.):0,ct=std::cos(tilt),st=std::sin(tilt);
    const auto orient=[&](V p){return V{p[0],ct*p[1]-st*p[2],st*p[1]+ct*p[2]};};
    const auto unorient=[&](V p){return V{p[0],ct*p[1]+st*p[2],-st*p[1]+ct*p[2]};};
    const V face_normal=orient({0,0,1});
    const auto get=[](const Bytes& b,unsigned at){unsigned v;std::memcpy(&v,b.data()+at,4);return v;};
    const auto linear=[&](unsigned word,unsigned c){const double v=((word>>(c*8))&255)/255.;return use_srgb?decode(v):v;};
    const auto pack=[&](V v){unsigned p=0xff000000;for(unsigned c=0;c<3;++c) {
        double x=std::clamp(v[c],0.,1.);if(use_srgb) x=encode(x);p|=unsigned(std::lround(x*255))<<(8*c);}return p;};
    const auto rotation=[](float angle){const float c=std::cos(angle),s=std::sin(angle);
        return std::array<float,9>{c,0,s,0,1,0,-s,0,c};};
    const auto multiply=[](const std::array<float,9>& m,V d,bool transpose=false){V v{};
        for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) v[r]+=m[transpose?c*3+r:r*3+c]*d[c];return v;};
    constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
    unsigned reused=0,changed=0,refused=0,protected_pixels=0,depth_sensitive=0;
    for(unsigned hops=0;hops<=4;++hops) for(unsigned kind:{1U,2U,3U}) {
        if((hops==4)!=(kind==3)) continue;
        for(float rough:{0.F,.04F,.20F,.35F,.62F,1.F}) {
            const unsigned lobes=rough>0?8:1;
            const auto layout=*render::shadows::native_reflection_history(w,h,{w,h},true,{},lobes,true,scene_paths);
            const unsigned floor_id=0xfffffffeU;
            const render::shadows::RayReflectionGround old_ground{orient({0,0,ground_primary?400.:200.}),scale(face_normal,-1)};
            const render::shadows::RayReflectionGround now_ground{{5,1,550},{.02,-.005,-1}};
            Bytes seed(layout.storage_bytes),input(layout.storage_bytes),owners(count*4,255);
            struct Record {std::array<unsigned,4> mirrors;unsigned control,terminal;
                std::array<float,3> feature;unsigned incoming;std::array<float,3> response;};static_assert(sizeof(Record)==52);
            const auto feature=[&](double x,double y,unsigned lobe,bool current) {
                // This is a crop of a native-size eye, not a 23-pixel image
                // spanning a wide field of view. Keep each bilinear feature
                // footprint within the consumer's unchanged angular guard.
                const V ray=unit({(x-11.5)/256,(y-8.5)/272,1});const double distance=400/dot(ray,face_normal);
                V origin=add(scale(ray,distance),scale(face_normal,-std::max(ground_primary?.05:.01,distance*1.e-5)));
                const V reflected=add(ray,scale(face_normal,-2*dot(ray,face_normal)));
                const V tangent=unit(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
                const double physical_roughness=ground_primary?0:rough;
                V d=unit(add(reflected,scale(add(scale(tangent,taps[lobe][0]),scale(bitangent,taps[lobe][1])),physical_roughness*physical_roughness)));
                if(dot(d,face_normal)>=0) d=reflected;
                for(unsigned hop=0;hop<hops;++hop) {
                    const double z=hop%2?400:200,t=(z-dot(origin,face_normal))/dot(d,face_normal);
                    check(t>0,"Independent ordered path has reverse travel");
                    const double bias=std::max(.05,t*1.e-5),normal=dot(d,face_normal)>0?-1:1;
                    origin=add(add(origin,scale(d,t)),scale(face_normal,normal*bias));d=add(d,scale(face_normal,-2*dot(d,face_normal)));
                }
                if(kind==1) {
                    const double z=hops%2?600:100,t=(z-dot(origin,face_normal))/dot(d,face_normal);const auto p=unorient(add(origin,scale(d,t)));
                    const V bary{(p[0]+8192)/24576,(p[1]+8192)/24576,0};
                    check(t>0 && bary[0]>=0 && bary[1]>=0 && bary[0]+bary[1]<=1,"Independent ordered finite endpoint left its face");return bary;
                }
                return current?unit(multiply(rotation(-.04F),multiply(rotation(.07F),d),true)):d;
            };
            const auto primary_response=[](unsigned p,bool now){return now?std::array<float,4>{.35F+float(p%7)*.03F,.766F,.336F,1}:
                std::array<float,4>{.9F,.2F,.1F,1};};
            const auto secondary_response=[&](unsigned lobe,bool now){if(ground_primary)lobe=0;return now?std::array<float,3>{.7F-float(lobe)*.02F,.6F,.3F}:
                std::array<float,3>{.1F,.9F,.8F};};
            const auto base_light=[&](unsigned p,bool now){return scene_paths && ground_primary
                ?now?std::array<float,4>{.025F+float(p%5)*.006F,.04F,.02F,1}:std::array<float,4>{.16F,.02F,.08F,1}
                :std::array<float,4>{0,0,0,1};};
            const auto record_at=[&](unsigned p,unsigned lobe){return count*(scene_paths?44:28)+(p*lobes+lobe)*52;};
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x;owners[p*4]=ground_primary?1:255;owners[p*4+1]=128;owners[p*4+2]=ground_primary?1:2;
                if(ground_primary && x<w-1 && y<h-1) {
                    const auto depth_at=[&](double xx,double yy){const V ray=unit({(xx-11.5)/256,(yy-8.5)/272,1});return 400/dot(ray,face_normal)*ray[2];};
                    const double guide=depth_at(x+.75,y+.75),tap=depth_at(x+1.5,y+1.5);
                    depth_sensitive+=std::abs(tap-guide)>std::max(.01,guide*.005);
                }
                if(x==3 && y>2 && y<h-3) {owners[p*4+3]=0;++protected_pixels;}
                for(unsigned frame=0;frame<2;++frame) {
                    auto& bytes=frame?input:seed;const auto response=primary_response(p,frame);
                    word(bytes,count*4+p*4,ground_primary?floor_id:frame?0:4);
                    const V ray=unit({(x+.5-11.5)/256,(y+.5-8.5)/272,1});
                    const float depth=frame?550.F:float(400/dot(ray,face_normal)*ray[2]);
                    std::memcpy(bytes.data()+count*8+p*4,&depth,4);std::memcpy(bytes.data()+count*12+p*16,response.data(),16);
                    if(scene_paths) {const auto base=base_light(p,frame);std::memcpy(bytes.data()+count*28+p*16,base.data(),16);}
                    V sum{};
                    for(unsigned lobe=0;lobe<lobes;++lobe) {
                        Record r{{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX},(kind<<8)|hops,kind==1?2U:UINT32_MAX,{},0,secondary_response(lobe,frame)};
                        for(unsigned hop=0;hop<hops;++hop) r.mirrors[hop]=scene_paths && (ground_primary?hop%2==1:hop%2==0)
                            ?floor_id:frame?(hop%2?0:1):(hop%2?4:3);
                        const auto v=feature(x+(frame?.75:.5),y+(frame?.75:.5),lobe,frame);
                        for(unsigned c=0;c<3;++c) r.feature[c]=float(v[c]);
                        const unsigned physical_lobe=ground_primary?0:lobe;
                        const unsigned level=frame?((x+y+physical_lobe)%3)*90+35:125+(x%3)*15;
                        r.incoming=0xff000000U|level|((frame?220-level/2:105+y*2)<<8)|((45+physical_lobe*17)<<16);
                        // Valid but DIFFERENT cached path in a nonzero bilinear
                        // tap. Same final environment/feature is not permission.
                        if(!frame && x==10 && lobe==0) {
                            if(hops) r.mirrors[0]=2;
                            else if(kind==1) r.terminal=1;
                            else {const auto shifted=unit(add(v,V{.15,0,0}));for(unsigned c=0;c<3;++c) r.feature[c]=float(shifted[c]);}
                        }
                        std::memcpy(bytes.data()+record_at(p,lobe),&r,sizeof(r));
                        V tinted{};for(unsigned c=0;c<3;++c) tinted[c]=linear(r.incoming,c)*r.response[c];
                        const auto quantized=pack(tinted);for(unsigned c=0;c<3;++c) sum[c]+=linear(quantized,c)/lobes;
                    }
                    const auto base=base_light(p,frame);
                    for(unsigned c=0;c<3;++c) sum[c]=base[c]+sum[c]*response[c];
                    word(bytes,p*4,(pack(sum)&0xffffffU)|(ground_primary?0xfe000000U:0xff000000U));
                }
            }
            const auto geometry=[&](bool old,bool disguised_finite=false) {
                const unsigned triangles=old?5:3,vertices=triangles*3,mapping=16+vertices*16;
                Bytes b(mapping+triangles*4);
                for(unsigned p=0;p<triangles;++p) {
                    const unsigned accepted=old?p:4-p;word(b,mapping+p*4,accepted);
                    const float z=accepted==4?400.F:accepted==3?200.F:hops%2?600.F:100.F;
                    const float xyz[3][4]{{-8192,-8192,z,1},{16384,-8192,z,1},{-8192,16384,z,1}};
                    for(unsigned v=0;v<3;++v) {
                        const V position=orient({xyz[v][0],xyz[v][1],xyz[v][2]});
                        const std::array<float,4> actual{float(position[0]),float(position[1]),float(position[2]),1};
                        std::memcpy(b.data()+16+p*48+v*16,actual.data(),16);
                    }
                }
                if(disguised_finite) {
                    const float impostor[3][4]{{0,0,400,2},{0,0,-1,2},{0,0,0,2}};
                    std::memcpy(b.data()+16,impostor,48);
                }
                return b;
            };
            render::GpuCalibratedReflectionHistory resolver;check(resolver.initialize(gpu.device),resolver.status().c_str());
            const auto run=[&](const Bytes& bytes,bool old,unsigned epoch,float weight,bool accept,bool wrong_pose=false,bool wrong_floor=false,bool disguised_finite=false) {
                Texture owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);gpu.begin();owner.upload(owners);
                const auto vertices=geometry(old,disguised_finite);Buffer source(gpu,bytes),positions(gpu,vertices);gpu.submit();
                const unsigned triangles=old?5:3,vertex_count=triangles*3,mapping=16+vertex_count*16;
                render::shadows::RayReflectionHistory history{16,{w,h},{256,272,11.5,8.5},1,65536,mapping,true};
                history.model_lobes=lobes;history.model_paths=true;history.scene_paths=scene_paths;
                if(scene_paths) {history.previous_ground=old_ground;if(wrong_floor)history.previous_ground->point[0]+=.1;}
                render::CalibratedReflectionLobeGeometry guide{positions.buffer,unsigned(vertices.size()),vertex_count,history,rough,
                    rotation(old?.07F:-.04F),rotation(wrong_pose?.11F:.07F)};
                if(scene_paths) guide.current_ground=old?old_ground:now_ground;
                const render::shadows::GpuReflectionOutput rays{gpu.device,source.buffer,w,h,w*4,{},layout};
                gpu.begin();
                if(!old) for(unsigned fault=0;fault<6;++fault) {
                    auto bad=guide;
                    if(fault==0) bad.bytes-=4;if(fault==1) bad.buffer=source.buffer;
                    if(fault==2) bad.current_cube[0]=std::numeric_limits<float>::quiet_NaN();
                    if(fault==3) bad.previous_cube[0]=2;
                    if(fault==4) bad.history.model_paths=false;if(fault==5) bad.history.model_lobes=2;
                    check(!resolver.enqueue(gpu.command,rays,owner.texture,{epoch,weight,use_srgb},&bad),"Malformed ordered path calibration accepted");
                }
                if(!old && scene_paths) for(unsigned fault=0;fault<4;++fault) {
                    auto bad=guide;
                    if(fault==0)bad.current_ground.reset();
                    if(fault==1)bad.current_ground->normal={0,0,0};
                    if(fault==2)bad.history.scene_paths=false;
                    if(fault==3)bad.history.previous_ground->normal[1]=std::numeric_limits<double>::quiet_NaN();
                    check(!resolver.enqueue(gpu.command,rays,owner.texture,{epoch,weight,use_srgb},&bad),"Malformed actual analytic plane accepted");
                }
                check(resolver.enqueue(gpu.command,rays,owner.texture,{epoch,weight,use_srgb},&guide),resolver.status().c_str());
                const auto candidate=resolver.output();gpu.submit();auto result=read_reflection_history(gpu,candidate);
                if(accept) resolver.commit();else resolver.discard();return result;
            };
            run(seed,true,42,.85F,true);
            // A preflight must include retained accepted banks on extent or
            // record-layout changes without allocating or disturbing colour.
            const auto accepted_before=resolver.accepted_output();
            const auto resident_bytes=std::uint64_t(layout.storage_bytes)*2;
            const auto stride=layout.storage_bytes/count;
            const auto alternate_stride=scene_paths?(lobes==1?460U:96U):(lobes==1?444U:80U);
            check(resolver.allocation_bytes(w,h,stride)==resident_bytes
                && resolver.allocation_bytes(w+1,h,stride)==std::uint64_t(w+1)*h*stride*2+resident_bytes
                && resolver.allocation_bytes(w,h,alternate_stride)==std::uint64_t(w)*h*alternate_stride*2+resident_bytes
                && resolver.can_allocate(w,h,stride) && !resolver.can_allocate(16384,16384,stride)
                && resolver.accepted_output().buffer==accepted_before.buffer,
                "Reflection allocation preflight omitted retained banks or disturbed accepted colour");
            const auto result=run(input,false,42,.85F,false);
            for(unsigned y=1;y<h-1;++y) for(unsigned x=1;x<w-1;++x) {
                const unsigned p=y*w+x;
                check(std::memcmp(result.data()+count*8+p*4,input.data()+count*8+p*4,4)==0
                    && std::memcmp(result.data()+count*12+p*16,input.data()+count*12+p*16,16)==0,
                    "Ordered reuse filtered current depth/primary material");
                if(scene_paths) check(std::memcmp(result.data()+count*28+p*16,input.data()+count*28+p*16,16)==0,
                    "Mixed reuse filtered CURRENT ground base/direct lighting");
                if(owners[p*4+3]==0) {
                    check(get(result,p*4)==get(input,p*4) && get(result,count*4+p*4)==UINT32_MAX,
                        "Ordered history altered protected receiver");continue;
                }
                V sum{};bool any=false;
                for(unsigned lobe=0;lobe<lobes;++lobe) {
                    const auto at=record_at(p,lobe);Record actual,current;
                    std::memcpy(&actual,result.data()+at,sizeof(actual));std::memcpy(&current,input.data()+at,sizeof(current));
                    check(actual.mirrors==current.mirrors && actual.control==current.control && actual.terminal==current.terminal
                        && actual.feature==current.feature && actual.response==current.response,"Ordered reuse transported old identity/feature/secondary response");
                    bool protected_footprint=false;
                    for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx)
                        protected_footprint|=owners[((y+dy)*w+x+dx)*4+3]==0;
                    const bool reject=(lobe==0 && (x==9 || x==10)) || protected_footprint;
                    // Protected seed x3 removes its footprint. The correct
                    // known guide otherwise has .25/.25 bilinear fractions.
                    V rgb{};
                    for(unsigned c=0;c<3;++c) {
                        rgb[c]=linear(current.incoming,c);
                        if(!reject) {
                            double prior=0;for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx)
                                prior+=linear(get(seed,record_at((y+dy)*w+x+dx,lobe)+36),c)*(dx?.25:.75)*(dy?.25:.75);
                            double low=rgb[c],high=rgb[c];
                            for(int yy=-1;yy<=1;++yy) for(int xx=-1;xx<=1;++xx) {
                                const auto next=unsigned(int(y)+yy)*w+unsigned(int(x)+xx);if(!owners[next*4+3]) continue;
                                const auto v=linear(get(input,record_at(next,lobe)+36),c);low=std::min(low,v);high=std::max(high,v);
                            }
                            rgb[c]=rgb[c]*.15+std::clamp(prior,low,high)*.85;
                        }
                    }
                    if(reject) {check(actual.incoming==current.incoming,"Ordered cache borrowed an invisible/tiny-share path");++refused;}
                    else {
                        const unsigned expected=pack(rgb);for(unsigned c=0;c<3;++c)
                            check(std::abs(int((actual.incoming>>(c*8))&255)-int((expected>>(c*8))&255))<=2,
                                ("Ordered incoming colour differs from independent known guide: hops="+std::to_string(hops)
                                +" kind="+std::to_string(kind)+" rough="+std::to_string(rough)+" xy="+std::to_string(x)+","+std::to_string(y)
                                +" lobe="+std::to_string(lobe)+" channel="+std::to_string(c)+" actual="+std::to_string((actual.incoming>>(c*8))&255)
                                +" expected="+std::to_string((expected>>(c*8))&255)+" current="+std::to_string((current.incoming>>(c*8))&255)).c_str());
                        changed+=actual.incoming!=current.incoming;++reused;any=true;
                    }
                    V tint{};for(unsigned c=0;c<3;++c) tint[c]=rgb[c]*current.response[c];const auto transported=pack(tint);
                    for(unsigned c=0;c<3;++c) sum[c]+=linear(transported,c)/lobes;
                }
                const auto base=base_light(p,true);
                for(unsigned c=0;c<3;++c) sum[c]=base[c]+sum[c]*primary_response(p,true)[c];
                const auto expected=any?(pack(sum)&0xffffffU)|(ground_primary?0xfe000000U:0xff000000U):get(input,p*4);
                check((get(result,p*4)>>24)==(get(input,p*4)>>24),"Mixed path changed current source opacity/receiver class");
                for(unsigned c=0;c<3;++c) check(std::abs(int((get(result,p*4)>>(c*8))&255)-int((expected>>(c*8))&255))<=2,
                    "Ordered final RGB used old material response or wrong transport quantization order");
            }
            // Discard must leave the accepted seed, and epoch/weight cuts must
            // preserve ALL current RGBA without another material re-encode.
            const auto repeated=run(input,false,42,.85F,false);check(repeated==result,"Discard advanced ordered path cache/matrix");
            const auto cut=run(input,false,43,.85F,false),off=run(input,false,42,0,false);
            const auto unmatched_pose=run(input,false,42,.85F,false,true);
            check(std::equal(input.begin(),input.begin()+count*4,cut.begin())
                && std::equal(input.begin(),input.begin()+count*4,off.begin())
                && std::equal(input.begin(),input.begin()+count*4,unmatched_pose.begin()),"Ordered history cut/OFF/unmatched accepted camera changed current RGBA");
            if(scene_paths) {
                const auto unmatched_floor=run(input,false,42,.85F,false,false,true);
                check(std::equal(input.begin(),input.begin()+count*4,unmatched_floor.begin()),"Unmatched accepted floor borrowed incident radiance");
            }
            if(hops==0 && kind==1 && rough==0) for(unsigned fault=0;fault<(scene_paths?14U:8U);++fault) {
                auto malformed=input;constexpr unsigned p=8*w+8;const auto at=record_at(p,0);
                if(fault==0) word(malformed,at+16,0x108); // Too many hops.
                if(fault==1) word(malformed,at+36,get(input,at+36)&0xffffff); // Non-opaque incoming.
                if(fault==2) word(malformed,at,0); // Unused mirror must be sentinel.
                if(fault==3) word(malformed,at+20,3); // Out-of-range terminal.
                if(fault==4) word(malformed,at+24,0x7fc00000); // NaN feature.
                if(fault==5) word(malformed,at+40,0x7f800000); // Infinite response.
                if(fault==6) word(malformed,at+16,0x200); // Escaped with finite terminal.
                if(fault==7) word(malformed,at+32,0x3f800000); // Finite bary feature.z must be zero.
                if(fault==8) word(malformed,count*28+p*16+12,0); // CURRENT base validity.
                if(fault==9) word(malformed,count*28+p*16,0x7fc00000); // CURRENT base NaN.
                if(fault==10) word(malformed,count*28+p*16,0xbf800000); // Negative direct light.
                if(fault==11) word(malformed,count*12+p*16+12,0); // CURRENT response validity.
                if(fault==12) word(malformed,count*4+p*4,3); // Primary identity outside current geometry.
                if(fault==13) word(malformed,p*4,get(input,p*4)&0xffffff); // Receiver must retain its native alpha class.
                const auto rejected=run(malformed,false,42,.85F,false);
                check(get(rejected,p*4)==get(malformed,p*4) && get(rejected,count*4+p*4)==UINT32_MAX
                    && get(rejected,at+16)==0,"Malformed path contaminated composed RGB or future history eligibility");
            }
            if(ground_primary && hops==0 && kind==1 && rough==0) {
                auto wrong_depth=seed;constexpr unsigned sample=8*w+8,tap=8*w+9;
                const float false_depth=123.F;std::memcpy(wrong_depth.data()+count*8+tap*4,&false_depth,4);
                run(wrong_depth,true,42,.85F,true);
                const auto rejected=run(input,false,42,.85F,false);
                check(get(rejected,sample*4)==get(input,sample*4)
                    && get(rejected,record_at(sample,0)+36)==get(input,record_at(sample,0)+36),
                    "Receding floor reused an incorrect bilinear primary depth");
                run(seed,true,42,.85F,true);
            }
            if(!ground_primary && hops==0 && kind==1 && rough==0) {
                const auto impostor=run(input,false,42,.85F,false,false,false,true);
                check(std::equal(input.begin(),input.begin()+count*4,impostor.begin()),
                    "Finite model geometry opted into an analytic plane and bypassed its footprint");
            }
        }
    }
    check(reused>10000 && changed>1000 && refused>1000 && protected_pixels>100,"Ordered path cache coverage inadequate");
    if(ground_primary) check(depth_sensitive>100,"Receding analytic fixture does not distinguish affine depth from per-neighbour rejection");
    std::cout<<(scene_paths?(ground_primary?"Mixed analytic primary path resolver: ":"Mixed model primary path resolver: "):"Ordered path resolver: ")
        <<reused<<" independent known-guide samples, "<<changed<<" changed incoming RGB, "<<refused
        <<" full-path/feature/protected footprint rejections; 0..4 hops, finite/escape/budget terminals, two camera rotations, accepted IDs beyond current triangle count, six roughnesses, 1/8 lobes, current per-hop response, quantization order, discard/cut/OFF and malformed calibration passed. Synthetic cache fixture, not live native owner acceptance.\n";
}
