// Coupled native-DXR -> curved-cache -> ordered compositor contract check.
// Geometry, materials, raster ownership and path records are produced by their
// real GPU owners. Readbacks below are diagnostics ONLY, never renderer input.
// Exact same geometry/liquid/eye/lobe calibration makes the authored old source
// of each current ray its SAME pixel/lobe, independently of a GPU inverse guide.
// The optional principal-point shift changes actual raster/RT projection and
// supplies an independently authored FRACTIONAL old source for every ray.
// Neither mode qualifies arbitrary moving geometry/liquid clocks, general old
// visibility, a live owner or practical frame cost.
void native_curved_cache_tests(Gpu& gpu,SDL_GPUTextureFormat format,bool shifted=false) {
    using namespace render::shadows;
    struct Record {
        std::array<unsigned,4> mirrors;unsigned control,terminal;
        std::array<float,3> feature;unsigned incoming;
        std::array<float,3> response,base;
    };
    static_assert(sizeof(Record)==64);
    constexpr unsigned w=96,h=72,count=w*h,cube_size=32;
    // Read status AFTER the operation, avoiding unspecified argument order and
    // a stale/dangling c_str() if the operation replaces its status string.
    const auto verify=[](bool ok,const auto& owner) {check(ok,owner.status().c_str());};
    const unsigned encoding=srgb(format)?2:1;
    const auto get=[](const Bytes& b,unsigned at) {unsigned v;std::memcpy(&v,b.data()+at,4);return v;};
    const auto linear=[&](unsigned v) {
        std::array<double,3> result{};
        for(unsigned c=0;c<3;++c) {result[c]=((v>>(c*8))&255)/255.;if(srgb(format))result[c]=decode(result[c]);}
        return result;
    };
    const auto pack=[&](std::array<double,3> rgb,unsigned alpha=255) {
        unsigned result=alpha<<24;
        for(unsigned c=0;c<3;++c) {double value=std::clamp(rgb[c],0.,1.);if(srgb(format))value=encode(value);
            result|=unsigned(std::floor(value*255+.5))<<(c*8);}
        return result;
    };
    const auto cube=[](unsigned phase) {
        std::vector<unsigned> result(6*cube_size*cube_size);
        for(unsigned f=0;f<6;++f)for(unsigned y=0;y<cube_size;++y)for(unsigned x=0;x<cube_size;++x)
            result[(f*cube_size+y)*cube_size+x]=0xff000000U
                |(45+f*7+x*3+phase*3)|((40+y*3+f*5-phase*2)<<8)|((70+x+y+f*2+phase)<<16);
        return result;
    };
    std::uint64_t total_changed=0,total_current=0,total_protected=0,total_prefix=0,total_identity=0,total_blocked=0,total_old_ink=0;
    for(unsigned full:{0U,1U})for(unsigned material:{0U,3U})for(float roughness:{0.F,.25F}) {
        std::cout<<"Native curved coupling format="<<format<<" full="<<full<<" liquid="<<material
            <<" rough="<<roughness<<" shifted="<<shifted<<std::endl;
        const unsigned lobes=roughness>0?8:1;
        Texture source0(gpu,w,h,format),source1(gpu,w,h,format),target0(gpu,w,h,format),target1(gpu,w,h,format),
            reference0(gpu,w,h,format),reference1(gpu,w,h,format),
            owner0(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),owner1(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
            depth0(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT),depth1(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
        const std::array<Texture*,2> sources{&source0,&source1},targets{&target0,&target1},
            references{&reference0,&reference1},owners{&owner0,&owner1},depths{&depth0,&depth1};
        render::GpuCalibratedScene scene;render::GpuCalibratedRayComposite compositor;
        std::array<render::GpuCalibratedReflectionHistory,2> caches;
        std::array<SdlDxrShadows,2> hardware;
        verify(scene.initialize(gpu.device,format),scene);
        verify(compositor.initialize(gpu.device,format),compositor);
        for(auto& cache:caches)verify(cache.initialize(gpu.device),cache);
        const auto gray=linear(0xff4d4d4dU),red=linear(0xff1a1accU);
        std::array vertices{quad(0,0,-2,1.1F,{float(gray[0]),float(gray[1]),float(gray[2]),1}),
            quad(.6F,.5F,-.55F,.24F,{float(red[0]),float(red[1]),float(red[2]),1}),
            quad(-.1F,.03F,-.65F,.07F,{1,0,0,1}),quad(.16F,.16F,-.5F,.05F,{1,1,0,1}),
            quad(0,0,-1,1,{.15F,.25F,.35F,1})};
        for(auto& v:vertices[0])v.position[2]+=.7F*v.position[1]+.08F*v.position[0];
        // Use the actual authored BG2 producer and native ground extension,
        // not an ordinary quad with fabricated ground receiver flags.
        constexpr float floor_y=.01F-180.F/256;
        simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
        ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
        for(unsigned y=0;y<8;++y)ppu.vram[32+y*2]=255;
        for(unsigned tile=0;tile<2048;++tile)ppu.vram[0x2000+tile*2]=1;
        auto ground=vr::landscape_sphere_packet(ppu,{},112,false,false,232);
        vr::place_landscape_ground(ground,floor_y,true);
        render::CalibratedGroundGradient gradient{{.2F,.3F,.4F},{.2F,.3F,.4F},112,111};
        gradient.material=material?9:5;gradient.origin={29,17};gradient.seconds=1.3F;gradient.brightness=.72F;
        check(render::apply_calibrated_ground(ground,gradient),"Coupled authored floor rejected native enhancement metadata");
        std::array<vr::DrawPacket,5> packets{};
        std::array<render::CalibratedScenePacket,5> draws{};
        std::array<render::CalibratedSceneMotionDraw,5> matches{};
        for(unsigned i=0;i<draws.size();++i) {
            packets[i].geometry.vertices=vertices[i];draws[i].packet=&packets[i];
            matches[i].valid=true;matches[i].previous_model=identity;matches[i].previous_vertices=packets[i].geometry.vertex_view();
            if(i<2) {draws[i].ray_caster=draws[i].reflective_material=true;draws[i].effect_layer=2;
                matches[i].previous_ray_first_triangle=i*2;}
        }
        draws[3].after_rays=true;draws[3].depth_test=false;
        packets[4]=ground;matches[4].previous_vertices=packets[4].geometry.vertex_view();
        draws[4].ground_receiver=true;draws[4].effect_layer=1;
        std::array<render::CalibratedRayGeometryOutput,2> rays{};
        std::array<vr::EyeCamera,2> cameras{};
        std::uint64_t token=0;
        std::array<Bytes,2> base,coverage,old_coverage,reference;
        std::array<render::CalibratedReflectionLobeGeometry,2> guides{};
        const std::array<unsigned,256> palette{};
        std::array<render::CalibratedGroundRayReceiver,2> terrain{};
        const auto upload_frame=[&](unsigned phase) {
          gpu.begin();verify(scene.upload_packets(gpu.command,draws),scene);token=scene.upload_token();
          for(unsigned e=0;e<2;++e) {
            cameras[e]=camera(e);
            if(shifted && phase) {
                // Authored current (+.25,-.375) pixel centre offset. View,
                // focal scale, geometry, rough lobe and liquid clock stay fixed;
                // old source is therefore (x+.5-.25,y+.5+.375).
                cameras[e].projection[8]-=float(2*.25/w);
                cameras[e].projection[9]+=float(2*-.375/h);
            }
            verify(scene.enqueue_eye(gpu.command,references[e]->texture,depths[e]->texture,w,h,cameras[e],{.03F,.06F,.09F,.7F}),scene);
            verify(scene.enqueue_eye(gpu.command,sources[e]->texture,depths[e]->texture,w,h,cameras[e],{.03F,.06F,.09F,.7F},
                render::CalibratedScenePhase::before_rays,owners[e]->texture,0,nullptr,true),scene);
            const render::CalibratedSceneMotion previous{camera(e),matches,w,h};
            rays[e]=scene.enqueue_ray_geometry(gpu.command,e,w,h,cameras[e],256,0,{},&previous);
            check(rays[e].complete,scene.status().c_str());
          }
          gpu.submit();check(scene.notify_submitted(token),"Coupled source lost actual submission ownership");
          for(unsigned e=0;e<2;++e) {base[e]=sources[e]->read();coverage[e]=owners[e]->read();reference[e]=references[e]->read();}
          check(coverage[0]!=coverage[1],"Native coupled ownership lost independent eye projections");
          for(unsigned e=0;e<2;++e) {
            const auto receiver=render::calibrated_ground_ray_receiver(ground,cameras[e],.8F);
            check(bool(receiver),"Authored native floor lost its ray calibration");terrain[e]=*receiver;
            terrain[e].transport.auxiliary_layers=full && !material;
            const auto& plane=terrain[e].plane;const auto& water=terrain[e].transport;
            RayReflectionLiquid liquid{{{plane.point.x,plane.point.y,plane.point.z},{plane.normal.x,plane.normal.y,plane.normal.z}},
                {},{},water.time,material};
            std::copy(water.world_to_view.begin(),water.world_to_view.end(),liquid.world_to_view.begin());
            std::copy(water.camera_position.begin(),water.camera_position.end(),liquid.offset.begin());
            const auto& g=rays[e];RayReflectionHistory history{g.previous_vertex_offset,g.previous_extent,g.previous_projection,
                g.previous_near_plane,g.previous_far_plane.value_or(65536),g.previous_index_offset,true};
            history.model_lobes=lobes;history.model_paths=history.curved_paths=true;history.curved_receivers=full;
            history.previous_liquid=liquid;
            guides[e]={g.buffer,g.previous_index_offset+g.vertex_count/3*4,g.vertex_count,history,roughness};
            guides[e].current_liquid=liquid;guides[e].current_projection=g.projection;
            guides[e].current_clip={g.near_plane,g.far_plane.value_or(65536)};
            check(guides[e].valid(),"Native curved producer correspondence cannot be consumed");
            check(!shifted || !phase || guides[e].current_projection!=guides[e].history.projection,
                "Shifted native fixture did not change actual current/accepted projection");
          }
        };
        std::array<GpuReflectionOutput,2> native{};
        std::array<Bytes,2> current{},accepted{};
        const auto resolve=[&](unsigned epoch,float weight,bool accept) {
            std::array<GpuReflectionOutput,2> candidates{};
            gpu.begin();
            for(unsigned e=0;e<2;++e) {
                check(!caches[e].enqueue(gpu.command,native[e],owners[e]->texture,{epoch,weight,srgb(format)},&guides[e]),
                    "Unqualified native curved history escaped its ordinary-owner guard");
                verify(caches[e].enqueue(gpu.command,native[e],owners[e]->texture,{epoch,weight,srgb(format),true},&guides[e]),caches[e]);
                candidates[e]=caches[e].output();
                verify(compositor.enqueue(gpu.command,sources[e]->texture,owners[e]->texture,targets[e]->texture,w,h,nullptr,&candidates[e]),compositor);
                verify(scene.enqueue_eye(gpu.command,targets[e]->texture,depths[e]->texture,w,h,cameras[e],{},
                    render::CalibratedScenePhase::after_rays,nullptr,token),scene);
            }
            gpu.submit(); // Whole ordered presentation, not just history dispatch.
            std::array<Bytes,2> result;
            for(unsigned e=0;e<2;++e) {
                result[e]=read_reflection_history(gpu,candidates[e]);
                Bytes words(result[e].begin(),result[e].begin()+count*4);
                auto expected=oracle(base[e],coverage[e],w,h,{},0,words,w*4,format,1,1);
                // Nearer WATER replaces any selected receiver, including a
                // submerged model. LAVA/mirror words replace only ground.
                for(unsigned p=0;p<count;++p)if((coverage[e][p*4] && (get(words,p*4)>>24)==253)
                    || (coverage[e][p*4]==1 && (get(words,p*4)>>24)==254))
                    for(unsigned c=0;c<3;++c)expected[p*4+c]=words[p*4+c];
                for(unsigned p=0;p<count;++p)if(reference[e][p*4]==255 && reference[e][p*4+1]==255 && reference[e][p*4+2]==0)
                    std::copy_n(reference[e].begin()+p*4,4,expected.begin()+p*4);
                compare(targets[e]->read(),expected,"Real native curved cache crossed ordered compositor/foreground ownership");
            }
            for(auto& cache:caches) {if(accept)cache.commit();else cache.discard();}
            return result;
        };
        for(unsigned phase=0;phase<2;++phase) {
            upload_frame(phase);
            const auto environment=cube(phase);
            for(unsigned e=0;e<2;++e) {
                const auto& g=rays[e];const render::GpuScene::RayGeometryOutput resident{
                    g.device,g.buffer,g.vertex_count,true,g.materials,g.material_offset,g.material_bytes};
                const Camera ray_camera{w,h,g.projection[0],g.projection[2],g.projection[3],g.projection[1]};
                const PrimaryRayRange range{g.near_plane,g.far_plane.value_or(65536)};
                verify(hardware[e].render_reflections(gpu.device,ray_camera,resident,palette,0xff29b50dU,roughness,0,
                    environment,cube_size,{1,0,0,0,1,0,0,0,1},nullptr,terrain[e].plane,0,&terrain[e].transport,false,range,0,encoding,false,&guides[e].history),
                    hardware[e]);
                native[e]=hardware[e].reflection_output();current[e]=read_reflection_history(gpu,native[e]);
                check(native[e].reflection_history.curved_paths && native[e].reflection_history.curved_receivers==bool(full),
                    "Native coupled producer selected a different path ABI");
            }
            const auto result=resolve(42,.85F,phase==0);
            if(!phase) {accepted=result;old_coverage=coverage;check(caches[0].accepted_output().buffer!=caches[1].accepted_output().buffer,"Eyes shared a reflection acceptance bank");}
            unsigned cohort_changed=0,cohort_model=0,cohort_liquid=0,cohort_protected=0,cohort_blocked=0,cohort_old_ink=0;
            for(unsigned e=0;e<2;++e) {
                const auto& layout=native[e].reflection_history;const unsigned prefix=layout.identity_offset/count;
                if(prefix>4) {
                    check(std::equal(current[e].begin()+count*4,current[e].begin()+count*prefix,result[e].begin()+count*4),
                        "Coupled cache transported canonical CURRENT hidden-water/surface planes");
                    total_prefix+=count*(prefix-4);
                }
                for(unsigned p=0;p<count;++p) {
                    const unsigned id=get(result[e],layout.identity_offset+p*4);
                    check(std::memcmp(result[e].data()+layout.witness_offset+p*4,current[e].data()+layout.witness_offset+p*4,4)==0
                        && std::memcmp(result[e].data()+layout.weight_offset+p*16,current[e].data()+layout.weight_offset+p*16,16)==0,
                        "Native coupling transported CURRENT primary depth/response");
                    if(layout.base_offset)check(std::memcmp(result[e].data()+layout.base_offset+p*16,current[e].data()+layout.base_offset+p*16,16)==0,
                        "Native coupling transported CURRENT primary transmission/emission");
                    if(!coverage[e][p*4] || !coverage[e][p*4+3]) {
                        check(get(result[e],p*4)==get(current[e],p*4) && id==UINT32_MAX,"Native curved cache reused protected/unowned raster pixels");
                        ++cohort_protected;
                    }
                    if(id==UINT32_MAX)continue;
                    cohort_model+=id<rays[e].vertex_count/3;cohort_liquid+=id==0xfffffffdU;
                    std::array<double,3> sum{};bool changed=false;
                    for(unsigned l=0;l<lobes;++l) {
                        const unsigned at=layout.incoming_offset+(p*lobes+l)*64;
                        Record a,r;std::memcpy(&a,result[e].data()+at,64);std::memcpy(&r,current[e].data()+at,64);
                        check(a.mirrors==r.mirrors && a.control==r.control && a.terminal==r.terminal && a.feature==r.feature
                            && a.response==r.response && a.base==r.base,"Coupled cache changed CURRENT ordered path/throughput/additive light");
                        total_current+=15;changed|=a.incoming!=r.incoming;
                        const unsigned x=p%w,y=p/w;
                        std::array<double,3> prior{};bool old_valid=true,old_ink=false;
                        if(phase) {
                            // Known source from authored lens change, NOT an
                            // inverse guide/feature/flow read back from the GPU.
                            const double sx=x-(shifted?.25:0),sy=y+(shifted?.375:0);
                            old_valid=sx>=0 && sx<=w-1 && sy>=0 && sy<=h-1;
                            if(old_valid)for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
                                const double fx=sx-std::floor(sx),fy=sy-std::floor(sy),share=(dx?fx:1-fx)*(dy?fy:1-fy);
                                if(!share)continue;
                                const unsigned nx=std::min(unsigned(std::floor(sx))+dx,w-1),ny=std::min(unsigned(std::floor(sy))+dy,h-1),n=ny*w+nx;
                                Record old;std::memcpy(&old,accepted[e].data()+layout.incoming_offset+(n*lobes+l)*64,64);
                                old_valid=old_valid && get(accepted[e],layout.identity_offset+n*4)==id
                                    && old.mirrors==r.mirrors && old.control==r.control && old.terminal==r.terminal;
                                old_ink=old_ink || !old_coverage[e][n*4] || old_coverage[e][n*4+3]<=127;
                                const auto rgb=linear(old.incoming);for(unsigned c=0;c<3;++c)prior[c]+=rgb[c]*share;
                                if(!shifted)check(old.feature==r.feature,"Identity-source fixture changed its authored optical path");
                            }
                            if(!old_valid) {
                                check(a.incoming==r.incoming,"Native shifted history crossed a different/missing old visibility footprint");
                                ++cohort_blocked;cohort_old_ink+=old_ink;
                            }
                        }
                        if(a.incoming!=r.incoming) {
                            ++cohort_changed;check(phase && (a.incoming>>24)==255,"Cold native coupling invented historical incident RGB");
                            check(old_valid,"Native cache reused an invalid known old source");
                            auto expected=linear(r.incoming),low=expected,high=expected;
                            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                                const unsigned nx=unsigned(std::clamp(int(x)+dx,0,int(w)-1)),ny=unsigned(std::clamp(int(y)+dy,0,int(h)-1)),n=ny*w+nx;
                                if(get(current[e],layout.identity_offset+n*4)!=id || !coverage[e][n*4] || coverage[e][n*4+3]<=127)continue;
                                Record neighbour;std::memcpy(&neighbour,current[e].data()+layout.incoming_offset+(n*lobes+l)*64,64);
                                if(neighbour.control!=r.control || neighbour.terminal!=r.terminal || neighbour.mirrors!=r.mirrors)continue;
                                const auto rgb=linear(neighbour.incoming);
                                for(unsigned c=0;c<3;++c) {low[c]=std::min(low[c],rgb[c]);high[c]=std::max(high[c],rgb[c]);}
                            }
                            for(unsigned c=0;c<3;++c)expected[c]=expected[c]*.15+std::clamp(prior[c],low[c],high[c])*.85;
                            const unsigned expected_word=pack(expected);
                            for(unsigned c=0;c<3;++c) {
                                if(std::abs(int((a.incoming>>(c*8))&255)-int((expected_word>>(c*8))&255))>1)
                                    throw std::runtime_error("Native authored-source incident oracle mismatch shifted="+std::to_string(shifted)+" format="+std::to_string(format)
                                        +" full="+std::to_string(full)+" liquid="+std::to_string(material)+" rough="+std::to_string(roughness)
                                        +" eye="+std::to_string(e)+" xy="+std::to_string(x)+","+std::to_string(y)+" lobe="+std::to_string(l)
                                        +" actual="+std::to_string(a.incoming)+" expected="+std::to_string(expected_word));
                            }
                            ++total_identity;
                        }
                        auto rgb=linear(a.incoming);
                        for(unsigned c=0;c<3;++c)rgb[c]=rgb[c]*r.response[c]+r.base[c];
                        const auto quantized=linear(pack(rgb));for(unsigned c=0;c<3;++c)sum[c]+=quantized[c]/lobes;
                    }
                    if(!phase || !changed)check(get(result[e],p*4)==get(current[e],p*4),"Native coupling changed CURRENT colour without incident reuse");
                    if(changed) {
                        std::array<float,4> response{},light{};
                        std::memcpy(response.data(),current[e].data()+layout.weight_offset+p*16,16);
                        if(layout.base_offset)std::memcpy(light.data(),current[e].data()+layout.base_offset+p*16,16);
                        for(unsigned c=0;c<3;++c)sum[c]=light[c]+sum[c]*response[c];
                        const unsigned expected=pack(sum,get(current[e],p*4)>>24),actual=get(result[e],p*4);
                        check((actual>>24)==(expected>>24),"Native coupling changed receiver opacity/class");
                        for(unsigned c=0;c<3;++c)check(std::abs(int((actual>>(c*8))&255)-int((expected>>(c*8))&255))<=1,
                            "Coupled native recomposition did not retain CURRENT lighting/lobe quantization");
                    }
                }
            }
            check(cohort_model>100 && cohort_protected>100 && (!full || cohort_liquid>100),"Coupled native fixture lacked model/liquid/protected ownership");
            if(phase) {
                check(cohort_changed>0,"A native material/lobe/receiver cohort completely bypassed cache reuse");
                for(unsigned e=0;e<2;++e)check(read_reflection_history(gpu,caches[e].accepted_output())==accepted[e],
                    "Discarded native ordered presentation replaced accepted history");
                check(resolve(42,.85F,false)==result,"Native resident retry changed after a discarded complete presentation");
                for(unsigned fault=0;fault<2;++fault) {
                    const auto fresh=resolve(fault?42:43,fault?0:.85F,false);
                    for(unsigned e=0;e<2;++e)check(std::equal(current[e].begin(),current[e].begin()+count*4,fresh[e].begin()),
                        "Native stale-epoch/OFF resolve changed current RGBA");
                }
                check(resolve(42,.85F,true)==result,"Accepting complete native presentation changed its candidate");
                if(shifted) {
                    // The accepted metadata is NOW the shifted projection.
                    // Replaying correspondence to the older unshifted frame
                    // must cut history, even with the same extent/epoch/liquid.
                    const auto stale=resolve(42,.85F,false);
                    for(unsigned e=0;e<2;++e) {
                        const unsigned prefix=native[e].reflection_history.identity_offset/count;
                        check(std::equal(current[e].begin(),current[e].begin()+count*prefix,stale[e].begin()),
                            "Accepted shifted projection reused stale old calibration");
                    }
                }
            }
            total_changed+=cohort_changed;total_protected+=cohort_protected;total_blocked+=cohort_blocked;total_old_ink+=cohort_old_ink;
            std::cout<<"  phase="<<phase<<" retained model/liquid="<<cohort_model<<'/'<<cohort_liquid
                <<" protected="<<cohort_protected<<" changed incident="<<cohort_changed<<" blocked old="<<cohort_blocked
                <<" blocked old ink="<<cohort_old_ink<<std::endl;
        }
        check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
    }
    check(total_changed>100 && total_current>10000 && total_protected>1000 && total_prefix>0,"Insufficient native coupling contract coverage");
    check(!shifted || (total_blocked>100 && total_old_ink>0),"Shifted native cache did not exercise missing/different/protected old taps");
    std::cout<<"Native curved coupling: changed="<<total_changed<<" exact current words="<<total_current
        <<" protected="<<total_protected<<" canonical bytes="<<total_prefix
        <<" authored-source colour witnesses="<<total_identity<<" blocked old="<<total_blocked<<" old ink="<<total_old_ink<<" shifted="<<shifted
        <<"; sharp/eight lobes, water/lava, model/full receivers, real GPU source/ownership, ordered composition and acceptance/discard gates passed. "
        <<"No arbitrary moving geometry/liquid clock/general old-visibility, live-game/performance or other-backend acceptance.\n";
}
