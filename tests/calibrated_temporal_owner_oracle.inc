// Owner-level oracle renders independent single-sample XR FOV offsets, then
// resolves scalar plane motion and centre reconstruction on the CPU. It never
// reads the owner's private motion/history images or calls its jitter/matcher.
template<class Render,class Consume>
void native_taa_owner_oracle(Render submit,Consume read,DisplayXrGameRenderer& renderer,
    bool srgb,bool rays,const char* backend,bool water=false,unsigned palette=0,unsigned intensity=100,
    unsigned world_post=0,bool procedural_water=false,bool pattern=false,bool edge=false) {
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto rgba=[&](Images images) {
        if(bgra) for(auto& eye:images) for(unsigned at=0;at<eye.size();at+=4) std::swap(eye[at],eye[at+2]);
        return images;
    };
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> source){return rgba(submit(std::move(source)));};
    const auto consume=[&]{return rgba(read());};
    constexpr unsigned frames=8;
    constexpr int phases[8][2]{{0,-43},{-64,43},{64,-100},{-96,-14},{32,71},{-32,-71},{0,-43},{-64,43}};
    std::array<std::shared_ptr<CalibratedGameFrame>,frames> sources;
    std::array<Images,frames> samples,labels,centres,centre_labels;
    using EdgeKeys=std::array<std::vector<unsigned>,2>;
    std::array<EdgeKeys,frames> sample_edges,centre_edges;
    const auto add=[&](CalibratedGameFrame& frame,float z,float radius,unsigned key,bool caster,bool after) {
        starfox::vr::DrawPacket packet;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=corner[0]*radius+corner[1]*radius*.19F;
            v.position[1]=corner[1]*radius+corner[0]*radius*.11F;v.position[2]=z;
            v.color[0]=caster?(corner[0]>0?.85F:.15F):1;
            v.color[1]=caster?(corner[1]>0?.65F:.1F):after?1:0;v.color[2]=caster?.3F:0;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
            if(caster) {v.dither_scale=1;v.odd_color[0]*=.35F;v.odd_color[1]*=.6F;}
            packet.geometry.vertices.push_back(v);
        }
        frame.draws.push_back({std::move(packet),after?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
            starfox::vr::SceneBlend::opaque,!after,key,caster,after});
    };
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.04F,.07F,.12F,1};
    if(palette) {check(edge?edge_oracle_style(palette):pattern?calibrated_pattern_effect(palette):calibrated_palette_effect(palette),"TAA reference requires its explicit witnessed family");base->settings.model_effects={palette,intensity,0,0};}
    if(world_post) base->settings.world_effects={world_post,intensity,0,0};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();base->previous=base->current=snapshot;
    snapshot->transforms[1].generation=snapshot->transforms[2].generation=1;
    snapshot->transforms[1].shape=11;snapshot->transforms[2].shape=12;
    add(*base,-2,.59F,1,true,false);add(*base,-1.1F,.1F,2,true,false);
    base->draws[1].packet.model[12]=.24F;base->draws[1].packet.model[13]=.13F;
    add(*base,-.8F,.023F,0,false,false);base->draws[2].packet.model[12]=-.12F;
    add(*base,-.7F,.035F,0,false,true);base->draws[3].packet.model[13]=.14F;
    if(water || world_post || procedural_water) {
        if(water) check(rays,"Liquid TAA owner requires native rays");snapshot->dots_mode=-1;
        base->settings.ray_tracing=water?2:0;base->settings.enhanced_ground=true;
        base->settings.ground_material=world_post?0:5;
        starfox::simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
        ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
        if(world_post) ppu.cgram[2]=15U|(7U<<5)|(5U<<10);
        for(unsigned y=0;y<8;++y) {
            ppu.vram[32+y*2]=world_post && y%2?0:255;
            if(world_post) ppu.vram[33+y*2]=y%2?255:0;
        }
        for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);
        starfox::vr::place_landscape_ground(floor,-.75F,true);
        const auto original_floor=floor;
        CalibratedGroundGradient gradient{{.24F,.19F,.12F},{.12F,.09F,.055F},112,111};
        gradient.material=base->settings.ground_material;gradient.seconds=1.75F;gradient.origin={123,-415};
        check(apply_calibrated_ground(floor,gradient),"Liquid TAA owner floor payload failed");
        base->draws.push_back({std::move(floor),CalibratedGameLayer::world,starfox::vr::SceneBlend::opaque,false});
        base->draws.back().reflection_environment=true;
        const auto wet=render(base);auto off=std::make_shared<CalibratedGameFrame>(*base);off->settings.ray_tracing=0;
        if(world_post) off->settings.world_effects={};
        if(procedural_water) {
            auto changed=gradient;changed.material=0;
            off->draws.back().packet=original_floor;
            check(apply_calibrated_ground(off->draws.back().packet,changed),"RT-OFF water reference ground failed");
        }
        const auto dry=render(off);unsigned changed=0;
        for(unsigned eye=0;eye<2;++eye) for(unsigned at=world_post?0:mock.image_width*mock.image_height*4*3/4;at<wet[eye].size();++at)
            changed+=wet[eye][at]!=dry[eye][at];
        check(changed>64,"TAA local-layer owner lacks visible water/world post coverage");
    }
    const auto label=[](const std::shared_ptr<CalibratedGameFrame>& source) {
        auto result=std::make_shared<CalibratedGameFrame>(*source);result->clear={1,0,0,1};
        result->settings.world_effects=result->settings.model_effects={};
        result->settings.manipulation=0;result->settings.extra_effects={};
        for(auto& draw:result->draws) for(auto& v:draw.packet.geometry.vertices) {
            v.dither_scale=0;v.color[0]=draw.ray_caster?0:1;v.color[1]=draw.source_key==1?1:0;
            v.color[2]=draw.source_key==2?1:0;v.color[3]=1;std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        return result;
    };
    const auto head=[](unsigned frame) {return float(frame)*.75F;};
    const unsigned edge_step=std::max(1U,unsigned(std::lround(double(mock.image_height)/224)));
    const auto branches=[&](const std::shared_ptr<CalibratedGameFrame>& styled) {
        EdgeKeys keys;if(!edge) return keys;
        auto raw=std::make_shared<CalibratedGameFrame>(*styled);raw->settings.model_effects=raw->settings.world_effects={};
        std::erase_if(raw->draws,[](const auto& draw){return draw.after_rays;});
        const auto colours=render(raw);
        auto mask=std::make_shared<CalibratedGameFrame>(*raw);mask->clear={0,0,0,1};
        for(auto& draw:mask->draws) for(auto& v:draw.packet.geometry.vertices) {
            v.dither_scale=0;v.color[0]=draw.layer==CalibratedGameLayer::world?1:0;
            v.color[1]=draw.layer==CalibratedGameLayer::model?1:0;v.color[2]=0;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        const auto classified=render(mask);const unsigned w=mock.image_width,h=mock.image_height;
        for(unsigned eye=0;eye<2;++eye) {
            std::vector<unsigned char> layers(w*h);keys[eye].resize(w*h);
            for(unsigned p=0;p<w*h;++p) layers[p]=classified[eye][p*4+1]==255?2:classified[eye][p*4]==255?1:0;
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned p=y*w+x,layer=layers[p],style=layer==1?world_post:layer==2?palette:0;
                keys[eye][p]=edge_oracle_key(edge_oracle_branch(colours[eye],layers,w,h,x,y,edge_step,style),layer,edge_oracle_style(style)?1:0);
            }
        }
        return keys;
    };
    for(unsigned f=0;f<frames;++f) {
        auto selected=std::make_shared<CalibratedGameFrame>(*base);
        auto state=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);state->revision=f;
        if(f>=3) state->transforms[1].generation=2;
        selected->previous=selected->current=state;selected->settings.effect_seconds=float(f)/60;
        if(f>=6) selected->settings.history_epoch=1;
        selected->draws[0].packet.model[12]=float(f)*.017F;selected->draws[0].packet.model[13]=float(f)*-.005F;
        selected->draws[1].packet.model[12]+=float(f)*-.009F;
        if(f==4) for(auto& v:selected->draws[0].packet.geometry.vertices) v.position[1]+=.023F;
        sources[f]=selected;native_mock.head_x=head(f);native_mock.sample_x=native_mock.sample_y=0;
        centres[f]=render(selected);centre_labels[f]=render(label(selected));
        centre_edges[f]=branches(selected);
        // FOV tangent offsets move the principal point opposite to raster jitter.
        native_mock.sample_x=-phases[f][0]/16.F;native_mock.sample_y=-phases[f][1]/16.F;
        // Native overlays are ordered AFTER reconstruction, not in temporal
        // colour/history. Reference the scene behind them rather than treating
        // their shifted artwork as a current geometry sample.
        auto sample=std::make_shared<CalibratedGameFrame>(*selected);
        std::erase_if(sample->draws,[](const auto& draw){return draw.after_rays;});
        samples[f]=render(sample);labels[f]=render(label(sample));
        sample_edges[f]=branches(sample);
        for(unsigned eye=0;eye<2;++eye) for(const auto* image:{&centres[f][eye],&centre_labels[f][eye],&samples[f][eye],&labels[f][eye]})
            check(image->size()==std::size_t(mock.image_width)*mock.image_height*4,
                "TAA owner fixture changed its extent after runtime discovery");
    }
    native_mock.sample_x=native_mock.sample_y=0;
    const auto eligible=[](const std::vector<unsigned char>& image,unsigned at) {return image[at]==0 && (image[at+1]==255 || image[at+2]==255);};
    const auto decode=[&](double x) {return srgb?(x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4)):x;};
    const auto encode=[&](double x) {return srgb?(x<=.0031308?x*12.92:1.055*std::pow(x,1/2.4)-.055):x;};
    unsigned compared=0,protected_bytes=0,history_changes=0;
    const unsigned pattern_scale=std::max(1U,unsigned(std::lround(double(mock.image_height)/224)));
    const std::array<std::array<unsigned,2>,3> pattern_effects{{{0,pattern?palette:0},{0,0},{0,0}}};
    for(unsigned quality=1;quality<=3;++quality) {
        native_mock.head_x=0;(void)render(base); // OFF destroys both temporal owners.
        std::array<TemporalAa,2> reference;
        for(unsigned f=0;f<frames;++f) {
            auto selected=std::make_shared<CalibratedGameFrame>(*sources[f]);selected->settings.aa_type=4;selected->settings.aa_quality=quality;
            native_mock.head_x=head(f);const auto actual=render(selected);
            check(actual[0]!=actual[1],"Native TAA flattened the calibrated eyes");
            const unsigned w=mock.image_width,h=mock.image_height;
            for(unsigned eye=0;eye<2;++eye) {
                std::vector<TemporalAaGuide> guide(w*h);
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    const unsigned i=y*w+x,at=i*4;auto& g=guide[i];g.eligible=eligible(labels[f][eye],at);if(!g.eligible) continue;
                    g.pattern_phase=pattern_oracle_phase(x,y,pattern_scale,2,pattern_effects);
                    if(edge) g.edge_phase=sample_edges[f][eye][i];
                    const unsigned object=labels[f][eye][at+1]==255?0:1;
                    const double z=object?1.1:2,depth=z+4./256;
                    g.depth=float(depth*256);g.previous_depth=g.depth;
                    g.valid=f && f!=6 && !(object==0 && (f==3 || f==4 || f==5));
                    if(!g.valid) continue;
                    const double l=std::tan(-.5),r=std::tan(.6),d=std::tan(-.5),u=std::tan(.4);
                    const double sx=2/(r-l),sy=2/(u-d),cx=(r+l)/(r-l),cy=(u+d)/(u-d);
                    const double jx=phases[f][0]/256.,jy=phases[f][1]/256.;
                    const auto& now=sources[f]->draws[object].packet.model;const auto& old=sources[f-1]->draws[object].packet.model;
                    const double px=((x+.5)*2/w-1+cx-2*jx/w)*depth/sx+(head(f)+(eye?8:-8))/256.-now[12]+old[12];
                    const double py=(1-(y+.5)*2/h+cy+2*jy/h)*depth/sy+2./256-now[13]+old[13];
                    const double priorx=((px-(head(f-1)+(eye?8:-8))/256.)*sx/depth-cx+2*(phases[f-1][0]/256.)/w+1)*w/2;
                    const double priory=(1-(py-2./256)*sy/depth+cy+2*(phases[f-1][1]/256.)/h)*h/2;
                    g.motion_x=float(priorx-(x+.5));g.motion_y=float(priory-(y+.5));
                }
                constexpr float weights[]{0,.65F,.8F,.9F};
                const auto resolved=reference[eye].resolve(w,h,samples[f][eye],guide,f>=6?2:1,weights[quality]);
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    // Only this dry region is covered by the independent rigid
                    // geometry/history oracle. The liquid component oracle
                    // covers wet rejection and same-depth disocclusion; do not
                    // claim untracked refraction motion or centre-water parity.
                    if((water || world_post || procedural_water) && y>=h/3) continue;
                    const unsigned at=(y*w+x)*4;
                    if(!eligible(centre_labels[f][eye],at)) {
                        check(std::equal(actual[eye].begin()+at,actual[eye].begin()+at+4,centres[f][eye].begin()+at),"Native TAA shifted native centre artwork");protected_bytes+=4;continue;
                    }
                    const double tx=std::clamp(x+phases[f][0]/256.,0.,double(w-1)),ty=std::clamp(y+phases[f][1]/256.,0.,double(h-1));
                    const unsigned ax=unsigned(tx),ay=unsigned(ty);const double fx=tx-ax,fy=ty-ay;
                    const auto phase=pattern_oracle_phase(x,y,pattern_scale,2,pattern_effects);
                    const auto edge_phase=edge?centre_edges[f][eye][y*w+x]:0;
                    bool crossed_phase=false;
                    for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                        const unsigned tapx=std::min(ax+dx,w-1),tapy=std::min(ay+dy,h-1),tap=(tapy*w+tapx)*4;
                        const double share=(dx?fx:1-fx)*(dy?fy:1-fy);
                        crossed_phase|=eligible(labels[f][eye],tap) && share>0
                            && (pattern_oracle_phase(tapx,tapy,pattern_scale,2,pattern_effects)!=phase
                                || (edge && sample_edges[f][eye][tapy*w+tapx]!=edge_phase));
                    }
                    for(unsigned c=0;c<3;++c) {
                        double rgb=0;
                        for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                            const unsigned tap=(std::min(ay+dy,h-1)*w+std::min(ax+dx,w-1))*4;
                            const double share=(dx?fx:1-fx)*(dy?fy:1-fy);
                            rgb+=decode((eligible(labels[f][eye],tap)?resolved[tap+c]:centres[f][eye][at+c])/255.)*share;
                        }
                        const int expected=crossed_phase?centres[f][eye][at+c]:int(std::lround(std::clamp(encode(rgb),0.,1.)*255));
                        if(std::abs(int(actual[eye][at+c])-expected)>2) {
                            throw std::runtime_error(std::string(backend)+" TAA owner oracle mismatch q="+std::to_string(quality)
                                +" frame="+std::to_string(f)+" eye="+std::to_string(eye)+" pixel="+std::to_string(x)+","+std::to_string(y)
                                +" channel="+std::to_string(c)+" actual="+std::to_string(actual[eye][at+c])+" expected="+std::to_string(expected)
                                +" centre="+std::to_string(centres[f][eye][at+c])+" sample="+std::to_string(samples[f][eye][at+c])
                                +" label="+std::to_string(labels[f][eye][at+1]));
                        }
                        ++checks;++compared;
                        history_changes+=f && std::abs(int(resolved[at+c])-int(samples[f][eye][at+c]))>2;
                    }
                    check(actual[eye][at+3]==centres[f][eye][at+3],"Native TAA changed centre alpha");
                }
            }
            check(render(selected)==actual && render(selected)==actual,"Held native TAA accumulated rounding/jitter drift");
            if(palette) {
                auto tick=std::make_shared<CalibratedGameFrame>(*selected);tick->settings.effect_seconds+=1;
                check(render(tick)==actual,"Unused pointwise style clock advanced native TAA phase/history");
            }
            if(f==2) {
                gpu_mock.timeout_right=true;check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"TAA wait lost source");
                check(renderer.poll()==DisplayXrSubmission::waiting,"TAA wait reacquired/jittered eyes");gpu_mock.timeout_right=false;
                auto result=renderer.poll();for(unsigned attempt=0;result==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::submitted && consume()==actual,"TAA wait changed held presentation");
                auto rejected=std::make_shared<CalibratedGameFrame>(*selected);rejected->draws[0].packet.model[12]+=.31F;
                gpu_mock.fail_end=true;auto failure=renderer.submit(rejected);
                for(unsigned attempt=0;renderer.frame_pending() && attempt<10000;++attempt) {SDL_Delay(1);failure=renderer.poll();}
                check(failure==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected TAA frame was accepted/left pending");
                (void)consume(); // Fixture transport copied it before xrEndFrame rejected it.
                check(render(selected)==actual,"Rejected TAA replaced accepted correspondence/colour/phase");
            }
        }
        check(render(base)==render(base),"TAA OFF retained jitter/history");
        auto combined=std::make_shared<CalibratedGameFrame>(*sources[1]);combined->settings.aa_type=4;combined->settings.aa_quality=quality;
        combined->settings.depth_enhancements=15;combined->settings.bloom_model=2;combined->settings.bloom_world=1;
        combined->settings.contrast=2;combined->settings.chromatic=1;combined->settings.global_enhancements=global_enhancement_mask;
        combined->settings.world_effects={unsigned(Effect::sepia),70,0,0};
        if(rays) {combined->settings.ray_tracing=combined->settings.reflections=2;combined->settings.material=unsigned(Effect::gold_metal);}
        const auto enhanced=render(combined);check(render(combined)==enhanced,"Held combined TAA/rays/depth/appearance wobbled");
        for(unsigned eye=0;eye<2;++eye) for(unsigned at=3;at<enhanced[eye].size();at+=4) check(enhanced[eye][at]==255,"Combined TAA changed opaque ink alpha");
    }
    check(compared>1000 && protected_bytes>1000 && (history_changes>100 || (edge && intensity==100)),"TAA owner fixture missed geometry/history/protected coverage");
    native_mock.head_x=0;native_mock.sample_x=native_mock.sample_y=0;
    std::cout<<backend<<" native TAA owner (palette/pattern/edge "<<palette<<", pattern witness "<<pattern<<", edge witness "<<edge<<", intensity "<<intensity<<", world post "<<world_post
        <<", procedural water "<<procedural_water<<"): "<<compared<<" independent motion/resolve/presentation bytes, "<<protected_bytes
        <<" exact centre ink bytes, "<<history_changes<<" history responses; all qualities, two eyes, recycled generations/morph/cut, held/OFF, rejected presentation, XR waits and combined passes passed.\n";
    if(water) std::cout<<backend<<" liquid TAA owner: visible native-water fixture retained independent dry-foreground temporal history; not a liquid-motion/centre-image oracle.\n";
}

template<class Render,class Consume>
void native_taa_edge_owner_oracle(Render render,Consume consume,DisplayXrGameRenderer& renderer,
    bool srgb,bool rays,const char* backend) {
    for(unsigned style:edge_oracle_styles) for(unsigned intensity:{35U,100U})
        native_taa_owner_oracle(render,consume,renderer,srgb,rays,backend,false,style,intensity,0,false,false,true);
    native_taa_owner_oracle(render,consume,renderer,srgb,rays,backend,false,6,35,39,false,false,true);
}

template<class Render,class Consume>
void native_taa_pattern_owner_oracle(Render render,Consume consume,DisplayXrGameRenderer& renderer,
    bool srgb,bool rays,const char* backend) {
    for(unsigned style:{5U,10U,25U,70U})
        native_taa_owner_oracle(render,consume,renderer,srgb,rays,backend,false,style,style==10?35:100,0,false,true);
    // The actual menu's Manipulation/FX slots don't admit point styles. Test
    // the legal mixed WORLD/MODEL selections, not an impossible UI combination.
    native_taa_owner_oracle(render,consume,renderer,srgb,rays,backend,false,10,35,70,false,true);
}
template<class Render,class Consume>
void native_taa_palette_owner_oracle(Render render,Consume consume,DisplayXrGameRenderer& renderer,
    bool srgb,bool rays,const char* backend) {
    for(unsigned palette:{unsigned(Effect::monochrome),unsigned(Effect::negative),unsigned(Effect::topographic)})
        native_taa_owner_oracle(render,consume,renderer,srgb,rays,backend,false,palette,palette==unsigned(Effect::negative)?35:100);
}
template<class Render,class Consume>
void native_taa_local_layers_owner_oracle(Render render,Consume consume,DisplayXrGameRenderer& renderer,
    bool srgb,bool rays,const char* backend) {
    for(unsigned style:{unsigned(Effect::ink),unsigned(Effect::ripple_warp),unsigned(Effect::hologram)})
        native_taa_owner_oracle(render,consume,renderer,srgb,rays,backend,false,0,70,style);
    native_taa_owner_oracle(render,consume,renderer,srgb,rays,backend,false,0,100,0,true);
}
template<class Render>
void native_taa_ground_clock_owner_oracle(Render render,bool srgb,const char* backend) {
    native_mock.head_x=native_mock.sample_x=native_mock.sample_y=0;
    starfox::simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
    ppu.bg2_character_base=0;ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31U<<10;
    for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
    for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
    const auto raw=[&] {
        auto p=starfox::vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);
        starfox::vr::place_landscape_ground(p,-.75F,true);return p;
    };
    unsigned stationary=0,animated=0;
    for(unsigned material:{0U,8U,5U,9U}) for(unsigned quality=1;quality<=3;++quality) {
        auto source=std::make_shared<CalibratedGameFrame>();source->settings.srgb=srgb;
        source->settings.aa_type=4;source->settings.aa_quality=quality;source->settings.effect_seconds=1.375F;
        source->clear={.07F,.11F,.18F,1};auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();
        snapshot->transforms[1].generation=1;snapshot->transforms[1].shape=11;
        source->previous=source->current=snapshot;
        CalibratedGroundGradient gradient{{.31F,.72F,.21F},{.04F,.19F,.035F},112,111};
        gradient.material=material;gradient.origin={123.25F,-415.5F};gradient.seconds=1.375F;
        source->draws.push_back({raw(),CalibratedGameLayer::world,starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        check(apply_calibrated_ground(source->draws.front().packet,gradient),"TAA ground clock producer failed");
        // Static bitmap-style ground is centre-protected by native AA. Include
        // genuine dithered, eligible foreground geometry so a clock-induced
        // sample-phase advance cannot pass by merely restoring centre ground.
        starfox::vr::DrawPacket actor;constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=corner[0]*.43F+corner[1]*.071F;
            v.position[1]=corner[1]*.39F+corner[0]*.053F;v.position[2]=-2;
            v.color[0]=corner[0]>0?.85F:.15F;v.color[1]=corner[1]>0?.65F:.1F;v.color[2]=.3F;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);v.dither_scale=1;
            v.odd_color[0]*=.35F;v.odd_color[1]*=.6F;actor.geometry.vertices.push_back(v);
        }
        source->draws.push_back({std::move(actor),CalibratedGameLayer::model,starfox::vr::SceneBlend::opaque,true,1,true});
        const auto first=render(source);auto tick=std::make_shared<CalibratedGameFrame>(*source);
        gradient.seconds=9.5F;if(material==0) gradient.origin={187.25F,-287.5F};
        tick->settings.effect_seconds=9.5F;tick->draws.front().packet=raw();
        check(apply_calibrated_ground(tick->draws.front().packet,gradient),"TAA fresh clocked terrain failed");
        const auto next=render(tick);
        if(material==0 || material==8) {
            check(first==next && render(source)==first && render(tick)==first,"Fresh static terrain clock advanced native TAA colour/phase");
            auto changed=std::make_shared<CalibratedGameFrame>(*tick);auto altered=gradient;altered.span*=.5F;
            changed->draws.front().packet=raw();check(apply_calibrated_ground(changed->draws.front().packet,altered),"Changed TAA ramp failed");
            check(render(changed)!=next,"Canonical static terrain hid a real palette/ramp change");++stationary;
        } else {
            unsigned changed=0;
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<next[eye].size();++at) changed+=next[eye][at]!=first[eye][at];
            check(changed>64 && render(tick)==next,"Ground canonicalization froze actual water/lava animation or held source drifted");++animated;
        }
        auto off=std::make_shared<CalibratedGameFrame>(*source);off->settings.aa_type=off->settings.aa_quality=0;
        check(render(off)==render(off),"Ground-clock TAA OFF retained sample history");
    }
    std::cout<<backend<<" native TAA ground clock: "<<stationary<<" freshly produced static gradient/red-sand cases stay exact through clock ticks/rewinds; "
        <<animated<<" actual water/lava cases keep animation, changed ramps and OFF are not hidden; all qualities, two eyes.\n";
}
