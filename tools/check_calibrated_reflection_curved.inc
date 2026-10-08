// Component-only cache differential. Authored binary64 FORWARD waves provide
// the known source position; no production inverse math or GPU feature readback
// constructs this oracle. Native closest-hit recording has a separate fixture.
void reflection_curved_cache_tests(Gpu& gpu,SDL_GPUTextureFormat format,bool trace_only=false) {
    namespace lo=reflected_liquid_oracle;
    using V=lo::V;
    // Keep the original 13x9 optical field and protected ink coordinates,
    // then extend it. Its sharp lava/environment cohort has independently
    // ZERO safe samples under the necessary neighboring-fold guard. The
    // added field supplies genuinely regular forward-authored paths without
    // dropping original samples or lowering any positive-coverage gate.
    constexpr unsigned w=33,h=17,count=w*h;
    constexpr unsigned liquid_id=0xfffffffdU;
    struct Record {std::array<unsigned,4> mirrors;unsigned control,terminal;
        std::array<float,3> feature;unsigned incoming;std::array<float,3> response,base;};
    static_assert(sizeof(Record)==64);
    const bool use_srgb=srgb(format);
    const auto linear=[&](unsigned word,unsigned c) {const double v=((word>>(c*8))&255)/255.;return use_srgb?decode(v):v;};
    const auto pack=[&](V value,unsigned alpha=255) {unsigned word=alpha<<24;
        for(unsigned c=0;c<3;++c) {double v=std::clamp(value[c],0.,1.);if(use_srgb)v=encode(v);
            word|=unsigned(std::lround(v*255))<<(c*8);}return word;};
    const auto get=[](const Bytes& b,unsigned at) {unsigned value;std::memcpy(&value,b.data()+at,4);return value;};
    unsigned changed=0,refused=0,current_words=0;
#if defined(STARFOX_CURVED_CACHE_TRACE_AVAILABLE)
    CurvedCacheTrace trace(gpu);
#else
    check(!trace_only,"Curved trace diagnostic was not compiled for this platform");
#endif
    for(unsigned material:{0U,3U}) for(unsigned mode=0;mode<5;++mode) for(unsigned lobes:{1U,8U}) for(unsigned kind:{1U,2U}) {
        if(trace_only && (lobes!=1 || kind!=1 || (mode!=0 && !(material==0 && mode==2))))continue;
        if(material==3 && mode>=3) continue;
        const bool full=mode!=0,liquid_primary=mode>=2;
        const float rough=lobes==8?.04F:0;
        const auto layers=mode>=3?*render::shadows::native_water_layers(w,h,mode==4):render::shadows::NativeWaterLayers{};
        const auto layout=*render::shadows::native_reflection_history(w,h,{w,h},true,layers,lobes,true,false,true,full);
        const unsigned prefix=layout.identity_offset/count;
        const auto record_at=[&](unsigned p,unsigned l) {return layout.incoming_offset+(p*lobes+l)*64;};
        lo::Frame old;old.width=w;old.height=h;old.near=1;old.far=65536;old.time=1.3;old.material=material;
        old.point={0,180,0};old.normal={0,-1,0};old.projection={256,272,6.5,-64.5};old.offset={29,-3,17};
        const render::shadows::RayReflectionLiquid accepted{{old.point,old.normal},old.rotation,old.offset,old.time,material};
        auto fresh=accepted;fresh.time=4.7;
        const auto forward_ray=[&](double x,double y,unsigned lobe) {
            V direction=lo::unit({(x-old.projection[2])/old.projection[0],(y-old.projection[3])/old.projection[1],1}),origin;
            if(liquid_primary) {
                const auto ray=lo::optical(old,x,y);check(ray.valid,"Curved cache authored primary ray missed its liquid");
                origin=lo::add(ray.hit,lo::scale(ray.normal,ray.bias));direction=lo::reflect(ray.direction,ray.normal);
            } else {
                const double primary_t=400/direction[2];const V normal{0,0,-1};
                origin=lo::add(lo::scale(direction,primary_t),lo::scale(normal,std::max(.01,primary_t*1.e-5)));
                const auto reflected=lo::reflect(direction,normal);
                const auto cross=[](V a,V b) {return V{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
                const V tangent=lo::unit(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
                constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
                direction=lo::unit(lo::add(reflected,lo::scale(lo::add(lo::scale(tangent,taps[lobe][0]),lo::scale(bitangent,taps[lobe][1])),double(rough)*rough)));
                if(lo::dot(direction,normal)<=0) direction=reflected;
                const double den=lo::dot(direction,old.normal),distance=lo::dot(lo::add(old.point,lo::scale(origin,-1)),old.normal)/den;
                check(distance>0 && distance<65536,"Curved cache authored secondary liquid went backwards");
                V hit=lo::add(origin,lo::scale(direction,distance)),position=lo::add(lo::row(hit,old),old.offset);
                const double footprint=std::sqrt(lo::dot(hit,hit))/old.projection[0]/std::max(std::abs(den),.04),t=old.time;
                double dx=.055*std::cos(position[0]*.018+position[2]*.011-t*.8)*lo::band(footprint,.022)
                    +.025*std::cos(position[0]*.047-position[2]*.025+t*1.2)*lo::band(footprint,.054);
                double dz=.045*std::cos(position[2]*.022-position[0]*.009-t*.65)*lo::band(footprint,.024)
                    -.020*std::cos(position[0]*.047-position[2]*.025+t*1.2)*lo::band(footprint,.054);
                if(material==3) {
                    auto sample=lo::lava(position[0],position[2],t,footprint);const auto travel=lo::row(direction,old);
                    const double shift=std::clamp(-sample[0]/std::max(travel[1],.12),-distance*.2,distance*.2);
                    hit=lo::add(hit,lo::scale(direction,shift));position=lo::add(position,lo::scale(travel,shift));
                    sample=lo::lava(position[0],position[2],t,footprint);dx=sample[1];dz=sample[2];
                }
                V normal_liquid=lo::unit(lo::col({dx,-1,dz},old));if(lo::dot(normal_liquid,direction)>0)normal_liquid=lo::scale(normal_liquid,-1);
                origin=lo::add(hit,lo::scale(normal_liquid,std::max(.05,distance*1.e-5)));
                direction=lo::reflect(direction,normal_liquid);
            }
            return std::pair{origin,lo::unit(direction)};
        };
        const double terminal_z=liquid_primary?1200:-1200;
        const auto forward=[&](double x,double y,unsigned lobe) {
            const auto [origin,direction]=forward_ray(x,y,lobe);
            if(kind==2)return direction;
            const double distance=(terminal_z-origin[2])/direction[2];
            check(distance>0 && distance<65536,"Curved authored finite terminal went backwards");
            const auto hit=lo::add(origin,lo::scale(direction,distance));
            const V feature{(hit[0]+8192)/24576,(hit[1]+8192)/24576,0};
            check(feature[0]>=0 && feature[1]>=0 && feature[0]+feature[1]<=1,"Curved authored finite terminal left its face");
            return feature;
        };
        // A finite endpoint is stored as binary32 barycentrics, then decoded
        // to a binary32 point. Its optical root need not equal the original
        // unquantized authoring coordinate near a caustic. Diagnose that
        // difference with a local binary64 solve of the independent FORWARD
        // fixture, not the production inverse/search or a GPU guide readback.
        const auto encoded_finite_root=[&](double x,double y,unsigned lobe) {
            const auto authored=forward(x,y,lobe);
            const V target{(double(float(24576*double(float(authored[0]))-8192))+8192)/24576,
                (double(float(24576*double(float(authored[1]))-8192))+8192)/24576,0};
            std::array<double,2> root{x,y};
            constexpr double step=.0001;
            for(unsigned iteration=0;iteration<48;++iteration) {
                const auto value=forward(root[0],root[1],lobe);
                const double ex=value[0]-target[0],ey=value[1]-target[1];
                if(std::max(std::abs(ex),std::abs(ey))<1.e-13)return root;
                const auto px=forward(root[0]+step,root[1],lobe),mx=forward(root[0]-step,root[1],lobe);
                const auto py=forward(root[0],root[1]+step,lobe),my=forward(root[0],root[1]-step,lobe);
                const double ax=(px[0]-mx[0])/(2*step),ay=(px[1]-mx[1])/(2*step);
                const double bx=(py[0]-my[0])/(2*step),by=(py[1]-my[1])/(2*step),det=ax*by-ay*bx;
                check(std::isfinite(det) && std::abs(det)>1.e-20,"Independent encoded finite endpoint is singular");
                root[0]-=(by*ex-bx*ey)/det;root[1]-=(ax*ey-ay*ex)/det;
                check(std::abs(root[0]-x)<.1 && std::abs(root[1]-y)<.1,"Encoded finite root left its independently authored local branch");
            }
            check(false,"Independent encoded finite endpoint did not converge");return root;
        };
        Bytes seed(layout.storage_bytes),input(layout.storage_bytes),owners(count*4,255);
        const auto response=[](unsigned p,bool now) {return now?std::array<float,4>{.42F+float(p%5)*.02F,.7F,.3F,1}:std::array<float,4>{.9F,.1F,.8F,1};};
        const auto base=[&](unsigned p,bool now) {return full?now?std::array<float,4>{.02F+float(p%3)*.006F,.04F,.03F,1}:
            std::array<float,4>{.3F,.01F,.2F,1}:std::array<float,4>{0,0,0,1};};
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
            const unsigned p=y*w+x;owners[p*4]=liquid_primary?1:255;owners[p*4+1]=128;owners[p*4+2]=liquid_primary?1:2;
            if(x==3 && y>1 && y<7)owners[p*4+3]=0;
            for(unsigned frame=0;frame<2;++frame) {
                auto& bytes=frame?input:seed;const auto weight=response(p,frame),light=base(p,frame);
                word(bytes,layout.identity_offset+p*4,liquid_primary?liquid_id:0);
                const float depth=liquid_primary?float(180*old.projection[1]/(y+.5-old.projection[3])):400;
                std::memcpy(bytes.data()+layout.witness_offset+p*4,&depth,4);
                std::memcpy(bytes.data()+layout.weight_offset+p*16,weight.data(),16);
                if(full)std::memcpy(bytes.data()+layout.base_offset+p*16,light.data(),16);
                if(layers.world_offset)word(bytes,layers.world_offset+p*4,0xfdb57a31U+p+frame*256);
                if(layers.surface_offset) {
                    const std::array<unsigned,4> exact{0x3e800000U+p,0x7fc01234U+p,0x3f000000U+p,0x40000000U+frame};
                    std::memcpy(bytes.data()+layers.surface_offset+p*16,exact.data(),16);
                }
                V sum{};
                for(unsigned l=0;l<lobes;++l) {
                    const auto feature=forward(x+(frame?.75:.5),y+(frame?.75:.5),l);
                    Record r{{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX},(kind<<8)|(liquid_primary?0:0x11),kind==1?(liquid_primary?0U:1U):UINT32_MAX,{},0,
                        frame?std::array<float,3>{.65F,.7F,.8F}:std::array<float,3>{.15F,.9F,.2F},
                        frame?std::array<float,3>{.035F,.02F,.015F}:std::array<float,3>{.22F,.01F,.25F}};
                    if(!liquid_primary)r.mirrors[0]=liquid_id;
                    for(unsigned c=0;c<3;++c)r.feature[c]=float(feature[c]);
                    const unsigned level=frame?35+((x+y+(liquid_primary?0:l))%3)*90:115+x%3*20;
                    r.incoming=0xff000000U|level|((frame?220-level/2:105+y*2)<<8)|(67U<<16);
                    // A valid BUT DIFFERENT old hop in a nonzero bilinear tap.
                    // Liquid primaries instead use an off-footprint direction.
                    if(!frame && x==7 && l==0) {
                        if(liquid_primary) {auto wrong=lo::add(feature,V{.15,0,0});if(kind==2)wrong=lo::unit(wrong);for(unsigned c=0;c<3;++c)r.feature[c]=float(wrong[c]);}
                        else {r.control=(kind<<8)|1;r.mirrors[0]=0;}
                    }
                    std::memcpy(bytes.data()+record_at(p,l),&r,sizeof(r));
                    V tinted{};for(unsigned c=0;c<3;++c)tinted[c]=linear(r.incoming,c)*r.response[c]+r.base[c];
                    const unsigned quantized=pack(tinted);for(unsigned c=0;c<3;++c)sum[c]+=linear(quantized,c)/lobes;
                }
                for(unsigned c=0;c<3;++c)sum[c]=light[c]+sum[c]*weight[c];
                word(bytes,p*4,pack(sum,liquid_primary?(material==0?253:254):255));
            }
        }
        render::GpuCalibratedReflectionHistory resolver;check(resolver.initialize(gpu.device),resolver.status().c_str());
        const auto run=[&](const Bytes& bytes,bool old_frame,unsigned epoch,float weight,bool accept,unsigned fault=0) {
            Texture owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);gpu.begin();owner.upload(owners);
            const unsigned triangles=liquid_primary?1:2,vertex_count=triangles*3,previous_vertices=triangles*48,mapping=previous_vertices*2;
            Bytes geometry(mapping+triangles*4);
            for(unsigned primitive=0;primitive<triangles;++primitive) {
                const float z=liquid_primary || primitive==1?float(terminal_z):400;
                const float face[3][4]{{-8192,-8192,z,1},{16384,-8192,z,1},{-8192,16384,z,1}};
                std::memcpy(geometry.data()+primitive*48,face,48);std::memcpy(geometry.data()+previous_vertices+primitive*48,face,48);word(geometry,mapping+primitive*4,primitive);
            }
            Buffer source(gpu,bytes),positions(gpu,geometry);gpu.submit();
            render::shadows::RayReflectionHistory previous{previous_vertices,{w,h},old.projection,1,65536,mapping,true};
            previous.model_lobes=lobes;previous.model_paths=previous.curved_paths=true;previous.curved_receivers=full;
            previous.previous_liquid=accepted;
            if(fault==1)previous.previous_liquid->time+=.01;
            if(fault==2)previous.previous_liquid->offset[0]+=.01;
            if(fault==3)previous.projection[0]+=.01;
            if(fault==4)previous.near_plane+=.01;
            render::CalibratedReflectionLobeGeometry guide{positions.buffer,unsigned(geometry.size()),vertex_count,previous,rough};
            guide.current_liquid=old_frame?accepted:fresh;guide.current_projection=old.projection;guide.current_clip={1,65536};
            const render::shadows::GpuReflectionOutput rays{gpu.device,source.buffer,w,h,w*4,layers,layout};
            gpu.begin();
            check(!resolver.enqueue(gpu.command,rays,owner.texture,{epoch,weight,use_srgb},&guide),"Unqualified curved colour escaped the ordinary-owner guard");
            check(resolver.enqueue(gpu.command,rays,owner.texture,{epoch,weight,use_srgb,true},&guide),resolver.status().c_str());
            check(!resolver.enqueue(gpu.command,rays,owner.texture,{epoch,weight,use_srgb,true},&guide),"Pending curved cache was overwritten");
            auto candidate=resolver.output();gpu.submit();const auto result=read_reflection_history(gpu,candidate);
#if defined(STARFOX_CURVED_CACHE_TRACE_AVAILABLE)
            if(trace_only && !old_frame) {
                const unsigned x=material==0?8:5,y=material==0?(mode==2?3:4):1,p=y*w+x;
                for(unsigned tight:{0U,1U}) {
                    const auto bytes=trace.run(tight,rays,resolver.accepted_output().buffer,owner.texture,guide);
                    const auto four=[&](unsigned offset){std::array<float,4> v;std::memcpy(v.data(),bytes.data()+p*112+offset,16);return v;};
                    const auto optical=four(0),terminal=four(16),delta=four(48),cap=four(64),solve=four(80);
                    std::array<unsigned,4> decision,state;std::memcpy(decision.data(),bytes.data()+p*112+32,16);std::memcpy(state.data(),bytes.data()+p*112+96,16);
                    std::cout<<std::setprecision(9)<<"Curved trace material="<<material<<" mode="<<mode<<" tight="<<tight<<" xy="<<x<<','<<y
                        <<" guide="<<optical[0]<<','<<optical[1]<<','<<optical[2]<<','<<optical[3]
                        <<" terminal="<<terminal[0]<<','<<terminal[1]<<','<<terminal[2]<<','<<terminal[3]
                        <<" refusal="<<decision[0]<<" tap="<<decision[1]<<" delta="<<delta[0]<<','<<delta[1]<<','<<delta[2]
                        <<" cap="<<cap[0]<<','<<cap[1]<<','<<cap[2]<<" solve="<<solve[0]<<','<<solve[1]
                        <<" error="<<solve[2]<<','<<solve[3]<<" state/iteration/backtrack/accept="<<state[0]<<','<<state[1]<<','<<state[2]<<','<<state[3]<<std::endl;
                    if(tight) {
                        const auto incoming=get(result,record_at(p,0)+36),original=get(input,record_at(p,0)+36);
                        check(decision[0]==0 || incoming==original,"Separate trace refusal disagrees with actual color preservation");
                    }
                    if(solve[0]>=0 && solve[1]>=0 && solve[0]<w && solve[1]<h) {
                        const auto [origin,outgoing]=forward_ray(solve[0],solve[1],0);
                        const V target=lo::unit(lo::add(V{terminal[0],terminal[1],terminal[2]},lo::scale(origin,-1)));
                        const auto cross=[](V a,V b){return V{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
                        const V tangent=lo::unit(cross(outgoing,std::abs(outgoing[1])<.95?V{0,1,0}:V{1,0,0}));
                        std::cout<<"Independent forward residual at actual solve="<<lo::dot(target,tangent)*272<<','
                            <<lo::dot(target,cross(outgoing,tangent))*272<<std::endl;
                    }
                    const auto jacobian=[&](double xx,double yy) {
                        constexpr double e=.0001;
                        const auto a=forward(xx+e,yy,0),b=forward(xx-e,yy,0),c=forward(xx,yy+e,0),d=forward(xx,yy-e,0);
                        return ((a[0]-b[0])*(c[1]-d[1])-(a[1]-b[1])*(c[0]-d[0]))/(4*e*e);
                    };
                    double low=1.e30,high=-1.e30;
                    for(unsigned j=0;j<=4;++j)for(unsigned i=0;i<=4;++i) {
                        const auto value=jacobian(x+.5+i*.25,y+.5+j*.25);low=std::min(low,value);high=std::max(high,value);
                    }
                    std::cout<<"Independent finite-feature Jacobian known/actual/cell-range="<<jacobian(x+.75,y+.75)<<','
                        <<jacobian(solve[0],solve[1])<<','<<low<<','<<high<<std::endl;
                }
            }
#endif
            if(accept)resolver.commit();else resolver.discard();return result;
        };
        run(seed,true,42,.85F,true);
        const auto accepted_output=resolver.accepted_output();
        const auto result=run(input,false,42,.85F,false);
        if(trace_only)continue;
        unsigned case_changed=0;
        for(unsigned y=1;y<h-1;++y)for(unsigned x=1;x<w-1;++x) {
            const unsigned p=y*w+x;
            check(std::memcmp(result.data()+layout.witness_offset+p*4,input.data()+layout.witness_offset+p*4,4)==0
                && std::memcmp(result.data()+layout.weight_offset+p*16,input.data()+layout.weight_offset+p*16,16)==0,
                "Curved history transported current primary response/depth");
            if(full)check(std::memcmp(result.data()+layout.base_offset+p*16,input.data()+layout.base_offset+p*16,16)==0,"Curved history transported current primary light");
            V sum{};bool any=false;
            for(unsigned l=0;l<lobes;++l) {
                Record r,actual;std::memcpy(&r,input.data()+record_at(p,l),64);std::memcpy(&actual,result.data()+record_at(p,l),64);
                check(std::memcmp(&actual,&r,36)==0 && std::memcmp(reinterpret_cast<const unsigned char*>(&actual)+40,
                    reinterpret_cast<const unsigned char*>(&r)+40,24)==0,"Curved cache changed CURRENT ordered geometry or light");current_words+=15;
                if(!owners[p*4+3]) {check(get(result,p*4)==get(input,p*4) && actual.incoming==r.incoming
                    && get(result,layout.identity_offset+p*4)==UINT32_MAX,"Curved reuse covered protected ink");continue;}
                bool protected_footprint=false;for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx)protected_footprint|=!owners[((y+dy)*w+x+dx)*4+3];
                bool reject=(l==0 && (x==6 || x==7)) || protected_footprint;
                // Inverting a curved ray is NOT permission to interpolate
                // unrelated old radiance. Independently classify EVERY tap's
                // angular footprint from the authored binary64 forward waves.
                // A rapidly changing lava normal can legitimately exceed the
                // unchanged .02 angular cap even with a known optical root.
                const auto known=forward(x+.75,y+.75,l);
                const auto old_feature=[&](unsigned xx,unsigned yy) {
                    auto f=forward(xx+.5,yy+.5,l);
                    if(liquid_primary && xx==7 && l==0) {f=lo::add(f,V{.15,0,0});if(kind==2)f=lo::unit(f);}
                    return f;
                };
                const auto cross=[](V a,V b) {return V{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
                const std::array features{old_feature(x,y),old_feature(x+1,y),old_feature(x,y+1),old_feature(x+1,y+1)};
                const V dx0=lo::add(features[1],lo::scale(features[0],-1)),dx1=lo::add(features[3],lo::scale(features[2],-1));
                const V dy0=lo::add(features[2],lo::scale(features[0],-1)),dy1=lo::add(features[3],lo::scale(features[1],-1));
                bool positive=false,negative=false;
                for(unsigned tap=0;tap<4;++tap) {
                    const auto dx=tap&2?dx1:dx0,dy=tap&1?dy1:dy0;
                    const double orientation=kind==1?dx[0]*dy[1]-dx[1]*dy[0]:lo::dot(features[tap],cross(dx,dy));
                    positive|=orientation>1.e-20;negative|=orientation<-1.e-20;
                }
                reject|=positive && negative;
                // Forward-authored adjacent cells independently classify a
                // neighboring inverse branch. Do not use the GPU-selected
                // cell/root to manufacture an expected-color exception.
                for(int cy=-1;cy<=1;++cy)for(int cx=-1;cx<=1;++cx) {
                    if(cx==0 && cy==0)continue;
                    const int bx=int(x)+cx,by=int(y)+cy;
                    if(bx<0 || by<0 || bx+1>=int(w) || by+1>=int(h))continue;
                    bool valid=true;std::array<V,4> ring;
                    for(unsigned tap=0;tap<4;++tap) {
                        const unsigned xx=unsigned(bx)+(tap&1),yy=unsigned(by)+(tap>>1),n=yy*w+xx;
                        valid&=owners[n*4+3]!=0 && (liquid_primary || xx!=7 || l!=0);
                        ring[tap]=old_feature(xx,yy);
                    }
                    if(!valid)continue;
                    const V dx0=lo::add(ring[1],lo::scale(ring[0],-1)),dx1=lo::add(ring[3],lo::scale(ring[2],-1));
                    const V dy0=lo::add(ring[2],lo::scale(ring[0],-1)),dy1=lo::add(ring[3],lo::scale(ring[1],-1));
                    for(unsigned tap=0;tap<4;++tap) {
                        const auto dx=tap&2?dx1:dx0,dy=tap&1?dy1:dy0;
                        const double orientation=kind==1?dx[0]*dy[1]-dx[1]*dy[0]:lo::dot(ring[tap],cross(dx,dy));
                        positive|=orientation>1.e-20;negative|=orientation<-1.e-20;
                    }
                }
                reject|=positive && negative;
                // Independent binary64 forward points expose folds hidden
                // inside a coarse cell, without using a GPU guide or inverse
                // to choose the expected color/source branch.
                std::array<V,9> subcell;
                for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)subcell[row*3+col]=forward(x+.5+col*.5,y+.5+row*.5,l);
                subcell[0]=features[0];subcell[2]=features[1];subcell[6]=features[2];subcell[8]=features[3];
                for(unsigned row=0;row<2;++row)for(unsigned col=0;col<2;++col) {
                    const unsigned a=row*3+col;
                    const V dx0=lo::add(subcell[a+1],lo::scale(subcell[a],-1)),dx1=lo::add(subcell[a+4],lo::scale(subcell[a+3],-1));
                    const V dy0=lo::add(subcell[a+3],lo::scale(subcell[a],-1)),dy1=lo::add(subcell[a+4],lo::scale(subcell[a+1],-1));
                    for(unsigned tap=0;tap<4;++tap) {
                        const auto dx=tap&2?dx1:dx0,dy=tap&1?dy1:dy0;
                        const double orientation=kind==1?dx[0]*dy[1]-dx[1]*dy[0]:lo::dot(subcell[a+(tap&1)+(tap>>1)*3],cross(dx,dy));
                        positive|=orientation>1.e-20;negative|=orientation<-1.e-20;
                    }
                }
                reject|=positive && negative;
                for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
                    const unsigned xx=x+dx,yy=y+dy;const auto feature=old_feature(xx,yy);V footprint{2.e-6,2.e-6,2.e-6};
                    for(unsigned axis=0;axis<2;++axis) {
                        V gradient{};
                        for(int side:{-1,1}) {
                            const int nx=int(xx)+(axis==0?side:0),ny=int(yy)+(axis==1?side:0);
                            if(nx<0 || ny<0 || nx>=int(w) || ny>=int(h))continue;
                            const unsigned n=unsigned(ny)*w+unsigned(nx);
                            if(!owners[n*4+3] || (!liquid_primary && nx==7 && l==0))continue;
                            const auto adjacent=old_feature(unsigned(nx),unsigned(ny));
                            for(unsigned c=0;c<3;++c)gradient[c]=std::max(gradient[c],std::abs(adjacent[c]-feature[c])*1.5);
                        }
                        const double at=axis==0?double(x)+.25:double(y)+.25,tap=axis==0?xx:yy;
                        for(unsigned c=0;c<3;++c)footprint[c]+=gradient[c]*std::abs(at-tap);
                    }
                    for(unsigned c=0;c<3;++c)reject|=std::abs(feature[c]-known[c])>std::min(footprint[c],kind==1?.05:.02);
                }
                V incoming{};
                for(unsigned c=0;c<3;++c) {
                    incoming[c]=linear(r.incoming,c);
                    if(!reject) {
                        double prior=0;for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx)
                            prior+=linear(get(seed,record_at((y+dy)*w+x+dx,l)+36),c)*(dx?.25:.75)*(dy?.25:.75);
                        double low=incoming[c],high=incoming[c];
                        for(int yy=-1;yy<=1;++yy)for(int xx=-1;xx<=1;++xx) {
                            const auto n=unsigned(int(y)+yy)*w+unsigned(int(x)+xx);if(!owners[n*4+3])continue;
                            const double sample=linear(get(input,record_at(n,l)+36),c);low=std::min(low,sample);high=std::max(high,sample);
                        }
                        incoming[c]=incoming[c]*.15+std::clamp(prior,low,high)*.85;
                    }
                }
                if(reject) {check(actual.incoming==r.incoming,"Curved cache borrowed an invisible/different old ordered path");++refused;}
                else {const auto expected=pack(incoming);
                    bool mismatch=false;for(unsigned c=0;c<3;++c)mismatch|=std::abs(int((actual.incoming>>(c*8))&255)-int((expected>>(c*8))&255))>1;
                    const auto endpoint_root=kind==1 && mismatch?encoded_finite_root(x+.75,y+.75,l):std::array<double,2>{x+.75,y+.75};
                    for(unsigned c=0;c<3;++c)check(std::abs(int((actual.incoming>>(c*8))&255)-int((expected>>(c*8))&255))<=1,
                        ("Curved incident oracle: material="+std::to_string(material)+" mode="+std::to_string(mode)+" lobes="+std::to_string(lobes)+" kind="+std::to_string(kind)
                            +" xy="+std::to_string(x)+","+std::to_string(y)+" lobe="+std::to_string(l)+" actual="+std::to_string(actual.incoming)+" expected="+std::to_string(expected)
                            +" encoded-root="+std::to_string(endpoint_root[0])+","+std::to_string(endpoint_root[1])).c_str());
                    changed+=actual.incoming!=r.incoming;case_changed+=actual.incoming!=r.incoming;any=true;
                }
                V tinted{};for(unsigned c=0;c<3;++c)tinted[c]=incoming[c]*r.response[c]+r.base[c];const auto quantized=pack(tinted);
                for(unsigned c=0;c<3;++c)sum[c]+=linear(quantized,c)/lobes;
            }
            if(owners[p*4+3]) {
                const auto light=base(p,true),weight=response(p,true);for(unsigned c=0;c<3;++c)sum[c]=light[c]+sum[c]*weight[c];
                const auto expected=any?pack(sum,get(input,p*4)>>24):get(input,p*4);
                check((get(result,p*4)>>24)==(get(input,p*4)>>24),"Curved cache changed opacity/receiver class");
                for(unsigned c=0;c<3;++c)check(std::abs(int((get(result,p*4)>>(c*8))&255)-int((expected>>(c*8))&255))<=1,"Curved recomposition transported additive light or changed per-lobe quantization");
            }
        }
        check(case_changed>0,"A curved material/receiver/lobe/finite-or-environment cohort bypassed incident reuse entirely");
        if(prefix>4)check(std::equal(input.begin()+count*4,input.begin()+count*prefix,result.begin()+count*4),"Curved resolve modified canonical world/surface bytes");
        check(run(input,false,42,.85F,false)==result && resolver.accepted_output().buffer==accepted_output.buffer,"Discard replaced accepted curved banks or liquid clock");
        for(unsigned fault=0;fault<6;++fault) {
            const auto raw=run(input,false,fault==0?43:42,fault==5?0:.85F,false,fault>0 && fault<5?fault:0);
            check(std::equal(input.begin(),input.begin()+count*prefix,raw.begin()),"Curved epoch/OFF/unmatched accepted liquid changed current colour");
        }
        if(material==0 && mode==2 && lobes==1) {
            constexpr unsigned p=5*w+5;
            for(unsigned fault=0;fault<8;++fault) {
                auto malformed=input;const unsigned at=record_at(p,0);
                if(fault==0)word(malformed,at+16,0x210U); // Mask outside zero hops.
                if(fault==1)word(malformed,at,0); // Unused ID is not sentinel.
                if(fault==2)word(malformed,at+52,0x7fc00000U); // NaN current per-hop light.
                if(fault==3)word(malformed,at+56,0xbf800000U); // Negative current per-hop light.
                if(fault==4)word(malformed,at+36,get(input,at+36)&0xffffffU); // Invalid incident alpha.
                if(fault==5)word(malformed,layout.base_offset+p*16+12,0); // Missing current-primary validity.
                if(fault==6)word(malformed,at+16,0x301U); // Budget without four hops.
                if(fault==7)word(malformed,at+40,0x7f800000U); // Infinite current throughput.
                const auto rejected=run(malformed,false,42,.85F,false);
                check(get(rejected,p*4)==get(malformed,p*4) && get(rejected,layout.identity_offset+p*4)==UINT32_MAX,
                    "Malformed curved record contaminated current radiance or remained reusable");
            }
            auto absorbed=input;Record r;std::memcpy(&r,absorbed.data()+record_at(p,0),64);
            r.control=0x411;r.mirrors[0]=liquid_id;r.terminal=UINT32_MAX;r.feature={};r.incoming=0;r.response={1.e-6F,2.e-6F,3.e-6F};
            std::memcpy(absorbed.data()+record_at(p,0),&r,64);
            const auto rejected=run(absorbed,false,42,.85F,false);
            check(get(rejected,p*4)==get(absorbed,p*4)
                && std::memcmp(rejected.data()+record_at(p,0),&r,64)==0,
                "Absorbed current light was discarded, re-encoded or given an invented old incident ray");
        }
        check(resolver.allocation_bytes(w,h,layout.storage_bytes/count)==std::uint64_t(layout.storage_bytes)*2,"Curved preflight lost exact accepted-bank bytes");
        check(run(input,false,42,.85F,true)==result && resolver.accepted_output().buffer!=accepted_output.buffer,
            "Accepting curved radiance did not publish the distinct completed bank");
        const auto stale=run(input,false,42,.85F,false);
        check(std::equal(input.begin(),input.begin()+count*prefix,stale.begin()),
            "New accepted liquid clock was not committed with its colour bank");
        resolver.reset();check(!resolver.accepted_output().buffer,"Reset retained a valid curved colour bank");
        std::cout<<"Curved cache format="<<unsigned(format)<<" material="<<material<<" mode="<<mode<<" lobes="<<lobes<<" kind="<<kind<<" passed"<<std::endl;
    }
    if(trace_only)return;
    check(changed>100 && refused>100 && current_words>10000,"Curved cache fixture did not exercise reuse, rejection and CURRENT radiance");
    std::cout<<"Curved component cache: "<<changed<<" changed incoming records; "<<refused<<" rejected different/tiny-share/protected paths; "<<current_words
        <<" exact current words; sharp/eight lobes, MODEL/liquid primaries, water/lava, canonical layers, cancelled acceptance and stale-clock/transform/epoch/OFF fallbacks. Not native whole-presentation or physical hardware acceptance.\n";
}
