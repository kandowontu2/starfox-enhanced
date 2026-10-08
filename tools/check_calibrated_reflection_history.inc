// Fixture-only buffer download and independent double-precision colour oracle.
// The production resolver does not read back, wait or submit commands.
struct ReflectionRecord {
    std::array<unsigned char,4> colour{};
    std::array<float,4> motion{};
    std::array<unsigned,4> identity{};
    std::array<float,4> witness{};
    std::array<unsigned char,4> incoming{};
    std::array<float,4> base{},response{};
    std::array<unsigned char,4> world{};
    std::array<std::uint32_t,4> surface{}; // Raw current words, including invalid/NaN payloads.
};
struct ReflectionFixture {
    unsigned width,height;std::vector<ReflectionRecord> records;Bytes owners;bool separated{};
    unsigned prefix{4};
    ReflectionFixture(unsigned w,unsigned h):width(w),height(h),records(w*h),owners(w*h*4) {}
    Bytes pack() const {
        const auto count=width*height;Bytes bytes(count*(prefix+(separated?84:48)));
        for(unsigned p=0;p<count;++p) {
            std::memcpy(bytes.data()+p*4,records[p].colour.data(),4);
            if(prefix==24) std::memcpy(bytes.data()+count*4+p*4,records[p].world.data(),4);
            if(prefix==20 || prefix==24) std::memcpy(bytes.data()+count*(prefix-16)+p*16,records[p].surface.data(),16);
            std::memcpy(bytes.data()+count*prefix+p*16,records[p].motion.data(),16);
            std::memcpy(bytes.data()+count*(prefix+16)+p*16,records[p].identity.data(),16);
            std::memcpy(bytes.data()+count*(prefix+32)+p*16,records[p].witness.data(),16);
            if(separated) {
                std::memcpy(bytes.data()+count*(prefix+48)+p*4,records[p].incoming.data(),4);
                std::memcpy(bytes.data()+count*(prefix+52)+p*16,records[p].base.data(),16);
                std::memcpy(bytes.data()+count*(prefix+68)+p*16,records[p].response.data(),16);
            }
        }
        return bytes;
    }
    render::shadows::NativeWaterLayers liquid() const {
        return prefix==4?render::shadows::NativeWaterLayers{}:
            *render::shadows::native_water_layers(width,height,prefix==24);
    }
};
bool reflected_record_eligible(const ReflectionFixture& frame,unsigned p) {
    const auto& r=frame.records[p];const auto* owner=frame.owners.data()+p*4;
    const bool consumer=owner[0]>0 && (r.colour[3]==253 || (owner[0]==1?r.colour[3]==254:r.colour[3]==255 && owner[1]>0));
    if(frame.separated && (r.incoming[3]!=255 || r.base[3]!=1 || r.response[3]!=0
        || !std::all_of(r.base.begin(),r.base.end(),[](float v){return std::isfinite(v);})
        || !std::all_of(r.response.begin(),r.response.end(),[](float v){return std::isfinite(v);})
        || std::any_of(r.base.begin(),r.base.begin()+3,[](float v){return v<0 || v>65504;})
        || std::any_of(r.response.begin(),r.response.begin()+3,[](float v){return v<0 || v>1;})
        || std::none_of(r.response.begin(),r.response.begin()+3,[](float v){return v>0;}))) return false;
    return owner[3]>127 && consumer && (owner[2]==1 || owner[2]==2)
        && (frame.separated || r.colour[3]==255) && r.witness[3]==1 && r.witness[2]>0
        && std::all_of(r.witness.begin(),r.witness.end(),[](float x){return std::isfinite(x);})
        && r.identity[0]!=UINT32_MAX && r.identity[1]!=UINT32_MAX
        && r.witness[0]>=0 && r.witness[1]>=0 && r.witness[0]+r.witness[1]<=1.00001;
}
ReflectionFixture reflection_seed(unsigned w,unsigned h) {
    ReflectionFixture frame(w,h);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const unsigned p=y*w+x;auto& r=frame.records[p];
        r.colour={static_cast<unsigned char>(70+(p%3)*40),static_cast<unsigned char>(170-(p%3)*30),100,255};
        r.identity={2,6,UINT32_MAX,UINT32_MAX};r.witness={.15F+.002F*x,.2F+.003F*y,500,1};
        auto* owner=frame.owners.data()+p*4;owner[0]=255;owner[1]=128;owner[2]=2;owner[3]=255;
        switch(p%37) {
        case 0:owner[3]=0;break;
        case 1:r.witness[2]=600;break;
        case 2:r.witness[3]=0;break;
        case 3:r.witness[0]+=.15F;break;
        case 4:r.colour[3]=0;break;
        case 5:r.identity[1]=8;break;
        default:break;
        }
    }
    return frame;
}
ReflectionFixture reflection_current(unsigned w,unsigned h,unsigned old_width,unsigned old_height) {
    ReflectionFixture frame(w,h);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const unsigned p=y*w+x;auto& r=frame.records[p];
        const float old_x=float(x%(old_width-2))+.25F,old_y=float(y%(old_height-2))+.25F;
        // Current draw order differs from the accepted order, and the feature
        // is not at the primary receiver's old/current screen coordinate.
        r.identity={9,3,2,6};r.motion={old_x+.5F,old_y+.5F,500,1};
        r.witness={.15F+.002F*old_x,.2F+.003F*old_y,530,1};
        const unsigned level=(x+y)%3;r.colour={static_cast<unsigned char>(50+level*90),
            static_cast<unsigned char>(230-level*90),static_cast<unsigned char>(50+level*90),255};
        auto* owner=frame.owners.data()+p*4;owner[0]=255;owner[1]=128;owner[2]=2;owner[3]=255;
        switch(p%23) {
        case 1:r.identity[2]=7;break;
        case 2:r.identity[3]=7;break;
        case 3:r.motion[2]=400;break;
        case 4:r.witness[0]+=.12F;break;
        case 5:r.identity[3]=UINT32_MAX;break;
        case 6:r.motion[0]=std::numeric_limits<float>::quiet_NaN();break;
        case 7:r.motion[0]=-1;break;
        case 8:r.motion[3]=0;break;
        case 9:owner[3]=0;break;
        case 10:r.colour[3]=253;break;
        case 11:r.identity[0]=UINT32_MAX;break;
        case 12:r.witness[3]=0;break;
        case 13:r.colour[3]=254;break;
        case 14:owner[2]=0;break;
        case 15:owner[1]=0;break;
        case 16:owner[0]=1;break;
        default:break;
        }
    }
    return frame;
}
ReflectionFixture reflection_components(ReflectionFixture frame,bool use_srgb,bool current,unsigned kind=254) {
    frame.separated=true;
    for(unsigned p=0;p<frame.records.size();++p) {
        auto& r=frame.records[p];auto* owner=frame.owners.data()+p*4;
        r.incoming=r.colour;r.incoming[3]=255;r.colour[3]=static_cast<unsigned char>(kind);
        r.base={current?.19F:.03F,current?.04F:.28F,float(p%5)*.013F,1};
        r.response={current?.35F:.85F,.6F,float(p%7)/8,0};
        if(kind==254) {
            owner[0]=1;r.identity[0]=UINT32_MAX-1;
            if(current && r.identity[2]==2) r.identity[2]=UINT32_MAX-1;
        }
        for(unsigned c=0;c<3;++c) {
            const double incoming=use_srgb?decode(r.incoming[c]/255.):r.incoming[c]/255.;
            double value=r.base[c]+incoming*r.response[c];
            value=std::clamp(value,0.,1.);if(use_srgb)value=encode(value);
            r.colour[c]=static_cast<unsigned char>(std::lround(value*255));
        }
        // Descriptor validation is not permission to trust malformed GPU data.
        // These current samples must remain bit-exact and cannot seed history.
        switch(p%31) {
        case 0:r.incoming[3]=0;break;
        case 1:r.base[0]=std::numeric_limits<float>::quiet_NaN();break;
        case 2:r.response[1]=std::numeric_limits<float>::infinity();break;
        case 3:r.base[0]=-.01F;break;
        case 4:r.response[0]=1.01F;break;
        case 5:r.response={0,0,0,0};break;
        case 6:r.base[3]=0;break;
        case 7:r.response[3]=1;break;
        case 8:r.base[1]=70000;break;
        default:break;
        }
    }
    return frame;
}
ReflectionFixture reflection_oracle(const ReflectionFixture& input,const ReflectionFixture* old,
    double weight,bool use_srgb,unsigned& reused) {
    auto output=input;
    const auto colour=[&](const ReflectionRecord& r,unsigned c) {
        const double value=(input.separated?r.incoming[c]:r.colour[c])/255.;return use_srgb?decode(value):value;
    };
    for(unsigned y=0;y<input.height;++y) for(unsigned x=0;x<input.width;++x) {
        const unsigned p=y*input.width+x;const auto& r=input.records[p];auto& result=output.records[p];
        const bool eligible=reflected_record_eligible(input,p);result.witness[3]=eligible?1.F:0.F;
        if(!old || !eligible || weight==0 || r.motion[3]!=1 || r.motion[2]<=0
            || !std::all_of(r.motion.begin(),r.motion.end(),[](float v){return std::isfinite(v);})
            || r.identity[2]==UINT32_MAX || r.identity[3]==UINT32_MAX) continue;
        const double ox=double(r.motion[0])-.5,oy=double(r.motion[1])-.5;
        if(ox<0 || oy<0 || ox>old->width-1 || oy>old->height-1) continue;
        const int bx=int(std::floor(ox)),by=int(std::floor(oy));const double fx=ox-bx,fy=oy-by;
        std::array<double,3> prior{};bool valid=true;
        const bool analytic_ground=input.separated && (r.colour[3]==254 || r.colour[3]==253)
            && r.identity[0]==UINT32_MAX-1 && r.identity[2]==UINT32_MAX-1;
        double inverse_depth=0;
        for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
            const double share=(dx?fx:1-fx)*(dy?fy:1-fy);if(share<=0) continue;
            const int tx=std::min(bx+dx,int(old->width)-1),ty=std::min(by+dy,int(old->height)-1);
            const auto& tap=old->records[unsigned(ty)*old->width+unsigned(tx)];
            if(tap.identity[0]!=r.identity[2] || tap.identity[1]!=r.identity[3] || tap.witness[3]!=1
                || (input.separated?tap.incoming[3]:tap.colour[3])!=255
                || !std::all_of(tap.witness.begin(),tap.witness.end(),[](float v){return std::isfinite(v);})
                || tap.witness[2]<=0 || (!analytic_ground
                    && std::abs(double(tap.witness[2])-r.motion[2])>std::max(.01,double(r.motion[2])*.005))) {valid=false;continue;}
            if(analytic_ground) inverse_depth+=share/double(tap.witness[2]);
            std::array<double,2> footprint{.002,.002};
            for(unsigned axis=0;axis<2;++axis) {
                std::array<double,2> gradient{};
                for(int side:{-1,1}) {
                    const int nx=tx+(axis==0?side:0),ny=ty+(axis==1?side:0);
                    if(nx<0 || ny<0 || nx>=int(old->width) || ny>=int(old->height)) continue;
                    const auto& neighbour=old->records[unsigned(ny)*old->width+unsigned(nx)];
                    if(neighbour.identity[0]!=r.identity[2] || neighbour.identity[1]!=r.identity[3] || neighbour.witness[3]!=1
                        || !std::all_of(neighbour.witness.begin(),neighbour.witness.end(),[](float v){return std::isfinite(v);})) continue;
                    for(unsigned c=0;c<2;++c) gradient[c]=std::max(gradient[c],1.5*std::abs(double(neighbour.witness[c])-tap.witness[c]));
                }
                const double distance=std::abs(axis==0?ox-tx:oy-ty);
                for(unsigned c=0;c<2;++c) footprint[c]+=gradient[c]*distance;
            }
            for(unsigned c=0;c<2;++c) if(std::abs(double(tap.witness[c])-r.witness[c])>std::min(.05,footprint[c])) valid=false;
            for(unsigned c=0;c<3;++c) prior[c]+=share*colour(tap,c);
        }
        if(!valid || (analytic_ground && (inverse_depth<=0 || !std::isfinite(inverse_depth)
            || std::abs(1/inverse_depth-r.motion[2])>std::max(.01,double(r.motion[2])*.005)))) continue;
        ++reused;
        for(unsigned c=0;c<3;++c) {
            double low=colour(r,c),high=low;
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                const unsigned n=unsigned(std::clamp(int(y)+dy,0,int(input.height)-1))*input.width
                    +unsigned(std::clamp(int(x)+dx,0,int(input.width)-1));
                const auto& tap=input.records[n];
                if(tap.identity[0]!=r.identity[0] || tap.identity[1]!=r.identity[1] || !reflected_record_eligible(input,n)) continue;
                low=std::min(low,colour(tap,c));high=std::max(high,colour(tap,c));
            }
            const double resolved=std::lerp(colour(r,c),std::clamp(prior[c],low,high),weight);
            double value=resolved;
            if(input.separated) {
                const double encoded=use_srgb?encode(std::clamp(resolved,0.,1.)):resolved;
                result.incoming[c]=static_cast<unsigned char>(std::lround(std::clamp(encoded,0.,1.)*255));
                value=r.base[c]+resolved*r.response[c];
            }
            value=std::clamp(value,0.,1.);if(use_srgb) value=encode(value);
            result.colour[c]=static_cast<unsigned char>(std::lround(std::clamp(value,0.,1.)*255));
        }
    }
    return output;
}
Bytes read_reflection_history(Gpu& gpu,const render::shadows::GpuReflectionOutput& output) {
    check(!gpu.command && output.buffer,"Reflection fixture downloaded pending/unavailable command");gpu.begin();
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,output.reflection_history.storage_bytes,0};
    auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,info.size};
    const SDL_GPUTransferBufferLocation to{transfer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);gpu.submit();
    check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
    auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(mapped,SDL_GetError());
    Bytes result(mapped,mapped+info.size);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);return result;
}
void compare_reflection_history(const Bytes& actual,const ReflectionFixture& expected) {
    const auto packed=expected.pack();const unsigned colour_bytes=expected.width*expected.height*4;
    check(actual.size()==packed.size(),"Reflection history output extent changed");
    for(unsigned i=0;i<actual.size();++i) {
        const unsigned incident_start=expected.width*expected.height*(expected.prefix+48);
        const bool incoming=expected.separated && i>=incident_start && i<incident_start+colour_bytes;
        const unsigned tolerance=(i<colour_bytes || incoming) && i%4!=3?1:0;
        if(unsigned(std::abs(int(actual[i])-int(packed[i])))>tolerance) {
            std::cerr<<"Reflection oracle byte="<<i<<" actual="<<unsigned(actual[i])<<" expected="<<unsigned(packed[i])<<'\n';
            throw std::runtime_error("Reflected colour/identity/witness oracle mismatch");
        }
    }
}
void reflection_history_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=17,h=13;const bool use_srgb=srgb(format);
    render::GpuCalibratedReflectionHistory resolver;check(resolver.initialize(gpu.device),resolver.status().c_str());
    const auto seed=reflection_seed(w,h);const auto input=reflection_current(w,h,w,h);
    unsigned reused=0,liquid_stabilized_bytes=0,liquid_guide_checks=0,liquid_world_checks=0;
    auto accepted=reflection_oracle(seed,nullptr,.85F,use_srgb,reused);
    const auto run=[&](const ReflectionFixture& frame,std::array<unsigned,2> old_extent,
        render::CalibratedReflectionHistorySettings settings,const ReflectionFixture* old,bool commit) {
        const auto held_before=resolver.accepted_output();
        const auto held_bytes=held_before.buffer?read_reflection_history(gpu,held_before):Bytes{};
        Texture ownership(gpu,frame.width,frame.height,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        std::unique_ptr<render::GpuCalibratedRayComposite> liquid_consumer;
        std::unique_ptr<Texture> original_receiver,resolved_receiver,original_surface,resolved_surface,
            world_source,original_world,resolved_world;
        if(frame.prefix!=4) {
            liquid_consumer=std::make_unique<render::GpuCalibratedRayComposite>();
            check(liquid_consumer->initialize(gpu.device,format),liquid_consumer->status().c_str());
            original_receiver=std::make_unique<Texture>(gpu,frame.width,frame.height,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            resolved_receiver=std::make_unique<Texture>(gpu,frame.width,frame.height,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            original_surface=std::make_unique<Texture>(gpu,frame.width,frame.height,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
            resolved_surface=std::make_unique<Texture>(gpu,frame.width,frame.height,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
            if(frame.prefix==24) {
                world_source=std::make_unique<Texture>(gpu,frame.width,frame.height,format);
                original_world=std::make_unique<Texture>(gpu,frame.width,frame.height,format);
                resolved_world=std::make_unique<Texture>(gpu,frame.width,frame.height,format);
            }
        }
        gpu.begin();ownership.upload(frame.owners);const auto packed=frame.pack();Buffer source(gpu,packed);
        render::shadows::GpuReflectionOutput rays{gpu.device,source.buffer,frame.width,frame.height,frame.width*4};
        rays.water_layers=frame.liquid();
        rays.reflection_history=*render::shadows::native_reflection_history(frame.width,frame.height,old_extent,frame.separated,rays.water_layers);
        check(resolver.enqueue(gpu.command,rays,ownership.texture,settings),resolver.status().c_str());
        check(!resolver.enqueue(gpu.command,rays,ownership.texture,settings),"Reflection history accepted overlapping candidates");
        const auto result=resolver.output();
        if(liquid_consumer) {
            check(liquid_consumer->enqueue_water_guides(gpu.command,ownership.texture,nullptr,
                original_receiver->texture,original_surface->texture,frame.width,frame.height,rays),liquid_consumer->status().c_str());
            check(liquid_consumer->enqueue_water_guides(gpu.command,ownership.texture,nullptr,
                resolved_receiver->texture,resolved_surface->texture,frame.width,frame.height,result),liquid_consumer->status().c_str());
            if(world_source) {
                Bytes background(frame.width*frame.height*4);
                for(unsigned p=0;p<frame.width*frame.height;++p) {
                    background[p*4]=33;background[p*4+1]=77;background[p*4+2]=99;
                    background[p*4+3]=static_cast<unsigned char>(p%2?255:121);
                }
                world_source->upload(background);
                check(liquid_consumer->enqueue(gpu.command,world_source->texture,ownership.texture,original_world->texture,
                    frame.width,frame.height,nullptr,&rays,1,0,0,1,{},true),liquid_consumer->status().c_str());
                check(liquid_consumer->enqueue(gpu.command,world_source->texture,ownership.texture,resolved_world->texture,
                    frame.width,frame.height,nullptr,&result,1,0,0,1,{},true),liquid_consumer->status().c_str());
            }
        }
        gpu.submit();
        check(result.water_layers==rays.water_layers && result.reflection_history==rays.reflection_history,
            "Resolver changed current liquid prefix or full history descriptor");
        if(liquid_consumer) {
            check(original_receiver->read()==resolved_receiver->read() && original_surface->read()==resolved_surface->read(),
                "Combined liquid history changed the real consumer's current ownership/surface output");
            ++liquid_guide_checks;
            if(world_source) check(original_world->read()==resolved_world->read(),
                "Combined liquid history changed the real consumer's hidden-world alpha/colour output");
            liquid_world_checks+=bool(world_source);
        }
        const auto expected=reflection_oracle(frame,old,settings.weight,settings.srgb,reused);
        const auto actual=read_reflection_history(gpu,result);
        compare_reflection_history(actual,expected);
        if(frame.prefix!=4 && old && settings.weight>0) {
            unsigned changed=0;
            for(unsigned i=0;i<frame.width*frame.height*4;++i)
                changed+=i%4!=3 && std::abs(int(actual[i])-int(packed[i]))>1;
            check(changed>10,"Combined liquid path silently bypassed visible reflected RGB stabilization");
            liquid_stabilized_bytes+=changed;
        }
        if(commit) resolver.commit();else resolver.discard();
        check(!resolver.output().buffer,"Settled reflection output still exposed a candidate");
        const auto held_after=resolver.accepted_output();
        if(commit) {
            check(held_after.buffer==result.buffer,"Committed secondary RGB is unavailable for held presentation");
            compare_reflection_history(read_reflection_history(gpu,held_after),expected);
        } else {
            check(held_after.buffer==held_before.buffer,"Discarded candidate replaced held secondary RGB");
            if(held_after.buffer) check(read_reflection_history(gpu,held_after)==held_bytes,
                "Discarded candidate modified accepted held colour/identity/witness bytes");
        }
        return expected;
    };
    run(seed,{w,h},{42,.85F,use_srgb},nullptr,true);
    const auto current_expected=run(input,{w,h},{42,.85F,use_srgb},&accepted,false);
    check(reused>10 && current_expected.pack()!=reflection_oracle(input,nullptr,0,use_srgb,reused).pack(),
        "Reflection fixture did not exercise actual colour history");
    // A discarded candidate must leave the old colour AND old identities intact.
    run(input,{w,h},{42,.85F,use_srgb},&accepted,false);
    run(input,{w,h},{42,0,use_srgb},&accepted,false);
    run(input,{w,h},{43,.85F,use_srgb},nullptr,false);
    run(input,{w,h},{42,.85F,!use_srgb},nullptr,false);
    run(input,{w+1,h},{42,.85F,use_srgb},nullptr,false);
    const auto resized=reflection_current(23,15,w,h);
    run(resized,{w,h},{42,.85F,use_srgb},&accepted,false);
    run(input,{w,h},{42,.85F,use_srgb},&accepted,false);
    // Inspect and decline malformed descriptors before any dispatch. These
    // faults cannot destroy a previously accepted frame.
    Texture ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    gpu.begin();ownership.upload(input.owners);const auto packed=input.pack();Buffer source(gpu,packed);
    render::shadows::GpuReflectionOutput rays{gpu.device,source.buffer,w,h,w*4};
    rays.reflection_history=*render::shadows::native_reflection_history(w,h,{w,h});
    gpu.submit();gpu.begin();
    for(unsigned fault=0;fault<8;++fault) {
        auto bad=rays;render::CalibratedReflectionHistorySettings settings{42,.85F,use_srgb};
        if(fault==0) bad.reflection_history.identity_offset+=4;
        if(fault==1) bad.reflection_history.storage_bytes-=4;
        if(fault==2) bad.row_bytes+=4;
        if(fault==3) bad.device=nullptr;
        if(fault==4) bad.water_layers.storage_bytes=4;
        if(fault==5) settings.weight=1;
        if(fault==6) settings.weight=std::numeric_limits<float>::quiet_NaN();
        if(fault==7) bad.reflection_history.extent={0,h};
        check(!resolver.enqueue(gpu.command,bad,ownership.texture,settings),"Malformed reflected history was encoded");
        check(!resolver.output().buffer,"Malformed request published stale reflected colour");
    }
    check(resolver.enqueue(gpu.command,rays,ownership.texture,{42,.85F,use_srgb}),resolver.status().c_str());
    const auto cancelled=resolver.output();
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;resolver.discard();
    run(input,{w,h},{42,.85F,use_srgb},&accepted,false);
    // No input may alias either retained image, even after cancellation.
    gpu.begin();auto aliased=rays;aliased.buffer=cancelled.buffer;
    check(!resolver.enqueue(gpu.command,aliased,ownership.texture,{42,.85F,use_srgb}),"Accepted reflection buffer aliased a new input");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    gpu.begin();check(resolver.enqueue(gpu.command,rays,ownership.texture,{42,.85F,use_srgb}),resolver.status().c_str());
    gpu.submit();check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());resolver.reset();resolver.commit();
    check(!resolver.accepted_output().buffer,"Reset-during-pending exposed invalid held secondary RGB");
    run(input,{w,h},{42,.85F,use_srgb},nullptr,false);
    resolver.reset();run(seed,{w,h},{42,.85F,use_srgb},nullptr,true);
    const auto accepted_resized=run(resized,{w,h},{42,.85F,use_srgb},&accepted,true);
    auto after_resize=resized;
    for(unsigned y=0;y<after_resize.height;++y) for(unsigned x=0;x<after_resize.width;++x) {
        auto& r=after_resize.records[y*after_resize.width+x];
        r.identity[2]=9;r.identity[3]=3;r.motion={x+.5F,y+.5F,530,1};
        r.colour[0]=static_cast<unsigned char>(r.colour[0]+10);
    }
    run(after_resize,{resized.width,resized.height},{42,.85F,use_srgb},&accepted_resized,false);
    // Switch layout without an epoch change: old whole-radiance words are
    // never interpreted as incident-light words (nor vice versa).
    const auto separated_seed=reflection_components(seed,use_srgb,false);
    const auto separated_input=reflection_components(input,use_srgb,true);
    run(separated_input,{w,h},{42,.85F,use_srgb},nullptr,false);
    run(separated_seed,{w,h},{42,.85F,use_srgb},nullptr,true);
    unsigned component_reused=0;
    const auto component_accepted=reflection_oracle(separated_seed,nullptr,.85F,use_srgb,component_reused);
    const auto component_expected=run(separated_input,{w,h},{42,.85F,use_srgb},&component_accepted,false);
    check(component_expected.pack()!=reflection_oracle(separated_input,nullptr,0,use_srgb,component_reused).pack(),
        "Separated-light fixture never accepted reflected colour");
    run(separated_input,{w,h},{43,.85F,use_srgb},nullptr,false);
    run(separated_input,{w,h},{42,0,use_srgb},&component_accepted,false);
    run(reflection_components(resized,use_srgb,true),{w,h},{42,.85F,use_srgb},&component_accepted,false);
    // Independent analytic plane Z=n.P/(n.D). Its neighbours differ by far
    // more than the point-depth tolerance, including tiny subpixel Y shares.
    // Valid footprint reconstruction must work without weakening the old
    // entity, secondary feature or disocclusion checks.
    auto receding_seed=separated_seed,receding_input=separated_input;
    const auto plane_depth=[](double x,double y){return 800./(2.+x*.17+y*.35);};
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        auto& old=receding_seed.records[y*w+x];auto& now=receding_input.records[y*w+x];
        if(old.witness[2]==500) old.witness[2]=float(plane_depth(x,y));
        if(now.motion[2]==500) now.motion[2]=float(plane_depth(now.motion[0]-.5,now.motion[1]-.5));
    }
    run(receding_seed,{w,h},{45,.85F,use_srgb},nullptr,true);
    const auto receding_accepted=reflection_oracle(receding_seed,nullptr,.85F,use_srgb,component_reused);
    const auto receding_expected=run(receding_input,{w,h},{45,.85F,use_srgb},&receding_accepted,false);
    check(receding_expected.pack()!=reflection_oracle(receding_input,nullptr,0,use_srgb,component_reused).pack(),
        "Receding analytic receiver never accepted incoming colour");
    for(auto& now:receding_input.records) if(std::isfinite(now.motion[1]) && now.motion[2]>0) {
        now.motion[1]=std::floor(now.motion[1]-.5F)+.5F+1.e-5F;
        if(now.motion[2]!=400) now.motion[2]=float(plane_depth(now.motion[0]-.5,now.motion[1]-.5));
    }
    run(receding_input,{w,h},{45,.85F,use_srgb},&receding_accepted,false);
    auto diagonal_seed=separated_seed,diagonal_input=separated_input;
    const auto diagonal_feature=[](double x,double y){return std::array<float,2>{
        float(.3-.01*x+.0135*y),float(.15+.006*x+.005*y)};};
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const unsigned p=y*w+x;auto& old=diagonal_seed.records[p];auto& now=diagonal_input.records[p];
        const auto before=diagonal_feature(x,y);
        old.witness[0]=before[0]+float(p%37==3)*.15F;old.witness[1]=before[1];
        // The light feature moves horizontally while nearly exactly aligned
        // in Y. A tiny upper-row tap still requires the combined 2-D witness
        // footprint; dropping that tap or relaxing the .05 ceiling is wrong.
        now.motion[0]=float(x%(w-2))+.5F+.72F;
        now.motion[1]=float(y%(h-2))+.5F+1.e-5F;
        const auto after=diagonal_feature(now.motion[0]-.5,now.motion[1]-.5);
        now.witness[0]=after[0]+float(p%23==4)*.12F;now.witness[1]=after[1];
    }
    run(diagonal_seed,{w,h},{46,.85F,use_srgb},nullptr,true);
    const auto diagonal_accepted=reflection_oracle(diagonal_seed,nullptr,.85F,use_srgb,component_reused);
    const auto diagonal_expected=run(diagonal_input,{w,h},{46,.85F,use_srgb},&diagonal_accepted,false);
    check(diagonal_expected.pack()!=reflection_oracle(diagonal_input,nullptr,0,use_srgb,component_reused).pack(),
        "Diagonal feature footprint never accepted incident colour");
    for(auto& now:diagonal_input.records) now.motion[2]*=.8F;
    const auto disoccluded=run(diagonal_input,{w,h},{46,.85F,use_srgb},&diagonal_accepted,false);
    for(unsigned p=0;p<w*h;++p) check(disoccluded.records[p].colour==diagonal_input.records[p].colour
        && disoccluded.records[p].incoming==diagonal_input.records[p].incoming,
        "Planar footprint accepted a mismatched optical depth");
    run(input,{w,h},{42,.85F,use_srgb},nullptr,false);
    // Water's current transmission/specular base follows the receiver, not
    // the reflected feature. This is an isolated radiance contract test,
    // not proof that the native curved-water producer is implemented.
    const auto liquid_seed=reflection_components(seed,use_srgb,false,253);
    const auto liquid_input=reflection_components(input,use_srgb,true,253);
    run(liquid_seed,{w,h},{44,.85F,use_srgb},nullptr,true);
    const auto liquid_accepted=reflection_oracle(liquid_seed,nullptr,.85F,use_srgb,component_reused);
    run(liquid_input,{w,h},{44,.85F,use_srgb},&liquid_accepted,false);
    // The real combined ABI retains this frame's water layers in the SAME
    // allocation. Give them distinct, changing bytes: copying old layers or
    // accidentally filtering the underlay/surface must fail the full oracle.
    const auto layered=[&](ReflectionFixture frame,unsigned prefix,unsigned phase) {
        check(frame.separated && (prefix==20 || prefix==24),"Invalid layered reflection fixture");
        frame.prefix=prefix;
        for(unsigned y=0;y<frame.height;++y) for(unsigned x=0;x<frame.width;++x) {
            const unsigned p=y*frame.width+x;auto& r=frame.records[p];
            r.world={static_cast<unsigned char>(17+(x*3+phase*19)%230),
                static_cast<unsigned char>(11+(y*7+phase*43)%230),static_cast<unsigned char>(phase*37),
                static_cast<unsigned char>((p+phase)%2?253:254)};
            r.surface={std::bit_cast<std::uint32_t>(float(x+phase)*.021F),
                std::bit_cast<std::uint32_t>(-1.F),std::bit_cast<std::uint32_t>(float(y+phase)*.017F),
                std::bit_cast<std::uint32_t>(float(phase*31)+200+p*.5F)};
            if(p%7==0) r.surface[0]=0x7fc12345U; // Preserve raw invalid/NaN payload, not canonicalized float.
            if(p%11==0) r.surface[3]=0;
            if(r.identity[0]!=UINT32_MAX) r.identity[0]=UINT32_MAX-1;
            if(r.identity[2]==2) r.identity[2]=UINT32_MAX-1;
            if(phase && r.motion[2]==500) r.motion[2]=float(800./(2.+(r.motion[0]-.5)*.17+(r.motion[1]-.5)*.35));
            if(!phase && r.witness[2]==500) r.witness[2]=float(800./(2.+x*.17+y*.35));
        }
        return frame;
    };
    unsigned layered_reused{};
    for(unsigned prefix:{20U,24U}) {
        const auto old=layered(liquid_seed,prefix,0),now=layered(liquid_input,prefix,1);
        // Change 4->20 or 20->24 without an epoch cut: no incompatible plane
        // can be sampled even if both cases are separated-radiance layouts.
        run(now,{w,h},{44,.85F,use_srgb},nullptr,false);
        const auto old_accepted=run(old,{w,h},{44,.85F,use_srgb},nullptr,true);
        const auto expected=run(now,{w,h},{44,.85F,use_srgb},&old_accepted,false);
        auto unfiltered=reflection_oracle(now,nullptr,0,use_srgb,layered_reused);
        check(expected.pack()!=unfiltered.pack(),"Combined liquid ABI never stabilized visible incident RGB");
        reflection_oracle(now,&old_accepted,.85F,use_srgb,layered_reused);
        run(now,{w,h},{44,0,use_srgb},&old_accepted,false);
        run(now,{w,h},{45,.85F,use_srgb},nullptr,false);
        run(now,{w,h},{44,.85F,!use_srgb},nullptr,false);
        const auto larger=layered(reflection_components(resized,use_srgb,true,253),prefix,2);
        run(larger,{w,h},{44,.85F,use_srgb},&old_accepted,false);
        // Rejected resize leaves the accepted liquid prefix and optical image
        // byte-exact; the next regular frame can still use the same old data.
        run(now,{w,h},{44,.85F,use_srgb},&old_accepted,false);
    }
    check(layered_reused>10,"Combined liquid ABI lacks accepted finite-feature coverage");
    // Switching back to a plain separated layout also cuts reuse.
    run(separated_input,{w,h},{44,.85F,use_srgb},nullptr,false);
    // All optional offsets must match a canonical allocation, before encode.
    Texture component_owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    gpu.begin();component_owner.upload(separated_input.owners);Buffer component_source(gpu,separated_input.pack());
    auto components=rays;components.buffer=component_source.buffer;
    components.reflection_history=*render::shadows::native_reflection_history(w,h,{w,h},true);
    for(unsigned fault=0;fault<5;++fault) {
        auto bad=components;
        if(fault==0) bad.reflection_history.incoming_offset=0;
        if(fault==1) bad.reflection_history.base_offset+=4;
        if(fault==2) bad.reflection_history.weight_offset-=4;
        if(fault==3) bad.reflection_history.storage_bytes-=4;
        if(fault==4) bad.reflection_history.witness_offset=bad.reflection_history.base_offset;
        check(!resolver.enqueue(gpu.command,bad,component_owner.texture,{44,.85F,use_srgb}),
            "Overlapping/incomplete separated-radiance layout was encoded");
    }
    for(unsigned prefix:{20U,24U}) {
        const auto liquid_frame=layered(liquid_input,prefix,1);
        Buffer liquid_source(gpu,liquid_frame.pack());
        auto combined=components;combined.buffer=liquid_source.buffer;combined.water_layers=liquid_frame.liquid();
        combined.reflection_history=*render::shadows::native_reflection_history(w,h,{w,h},true,combined.water_layers);
        for(unsigned fault=0;fault<7;++fault) {
            auto bad=combined;
            if(fault==0) bad.water_layers.surface_offset+=4;
            if(fault==1) bad.water_layers.world_offset=prefix==24?0:4;
            if(fault==2) bad.water_layers.storage_bytes=bad.reflection_history.storage_bytes;
            if(fault==3) bad.reflection_history.motion_offset-=4;
            if(fault==4) bad.reflection_history.storage_bytes=bad.water_layers.storage_bytes;
            if(fault==5) bad.reflection_history.incoming_offset=0;
            if(fault==6) bad.reflection_history=*render::shadows::native_reflection_history(w,h,{w,h},true);
            check(!resolver.enqueue(gpu.command,bad,component_owner.texture,{44,.85F,use_srgb}) && !resolver.output().buffer,
                "Malformed combined liquid prefix/append allocation encoded work or exposed a stale candidate");
        }
    }
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    std::cout<<"  Reflected RGB format "<<unsigned(format)<<": "<<reused
        <<" accepted colour samples; reordered primitives, depth/barycentric disocclusion, protected/unconsumed pixels, full old footprint, receding plane and diagonal-feature/tiny-share rejection regressions, linear-light clamp, epoch/colour/extent changes, resize, cancellation, held resident RGB, alias and reset-during-pending; separated ground/liquid incoming RGB, bit-exact current base/response, malformed components and layout cuts; combined 104/108-byte liquid ABI retained current world/surface including raw NaNs, "
        <<layered_reused<<" layered incoming samples, "<<liquid_stabilized_bytes<<" actual stabilized RGB bytes, "
        <<liquid_guide_checks<<" real guide and "<<liquid_world_checks<<" real hidden-world consumer comparisons, mode cuts and 14 malformed combined allocations passed\n";
}
