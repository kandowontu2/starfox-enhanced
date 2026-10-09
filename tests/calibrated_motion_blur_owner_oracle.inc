// Owner-level independent scalar plane motion/shutter oracle. Inputs are plain
// reference scene colours/labels; no private native motion/depth is read back.
template<class Submit,class Consume>
std::shared_ptr<const CalibratedGameFrame> native_blur_owner_oracle(Submit submit,Consume consume_raw,
    DisplayXrGameRenderer& renderer,bool srgb,bool rays,const char* backend) {
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto rgba=[&](Images images){if(bgra) for(auto& e:images) for(unsigned at=0;at<e.size();at+=4) std::swap(e[at],e[at+2]);return images;};
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> f){return rgba(submit(std::move(f)));};
    const auto consume=[&]{return rgba(consume_raw());};
    native_mock.head_x=0;native_mock.sample_x=native_mock.sample_y=0;
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.03F,.05F,.09F,1};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();snapshot->flow=starfox::simulation::GameFlowState::gameplay;
    snapshot->transforms[1].generation=snapshot->transforms[2].generation=1;
    snapshot->transforms[1].shape=11;snapshot->transforms[2].shape=12;base->previous=base->current=snapshot;
    const auto quad=[&](float z,float radius,CalibratedGameLayer layer,unsigned key,bool ink=false,bool after=false) {
        starfox::vr::DrawPacket p;p.preserve_native_colour=ink;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=corner[0]*radius;v.position[1]=corner[1]*radius;v.position[2]=z;
            v.color[0]=ink?1:corner[0]>0?.75F:.12F;v.color[1]=ink?(after?1:0):corner[1]>0?.6F:.13F;
            v.color[2]=ink?0:.24F;v.color[3]=1;
            // Source flat-shaded triangles use their provoking colour. Give
            // models and scenery DIFFERENT authored colours so an identity
            // implementation cannot pass the swept-silhouette oracle.
            if(!ink) {
                v.color[0]=layer==CalibratedGameLayer::world?.04F:key==1?.9F:.1F;
                v.color[1]=layer==CalibratedGameLayer::world?.28F:key==1?.21F:.8F;
                v.color[2]=layer==CalibratedGameLayer::world?.61F:key==1?.06F:.35F;
            }
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);p.geometry.vertices.push_back(v);
        }
        base->draws.push_back({std::move(p),layer,starfox::vr::SceneBlend::opaque,!after,key,key!=0,after});
    };
    quad(-8,20,CalibratedGameLayer::world,0);quad(-2,.50F,CalibratedGameLayer::model,1);
    quad(-1.1F,.12F,CalibratedGameLayer::model,2);base->draws[2].packet.model[12]=.25F;
    quad(-.8F,.035F,CalibratedGameLayer::model,0,true);base->draws[3].packet.model[12]=-.13F;
    quad(-.7F,.04F,CalibratedGameLayer::native_ui,0,true,true);base->draws[4].packet.model[13]=.13F;
    const auto labels=[&](const std::shared_ptr<CalibratedGameFrame>& source,bool overlays) {
        auto f=std::make_shared<CalibratedGameFrame>(*source);f->settings={};f->settings.srgb=srgb;f->clear={0,1,1,1};
        if(!overlays) std::erase_if(f->draws,[](const auto& d){return d.after_rays;});
        for(auto& d:f->draws) for(auto& v:d.packet.geometry.vertices) {
            v.color[0]=d.packet.preserve_native_colour?1:0;
            v.color[1]=d.layer==CalibratedGameLayer::world || d.source_key==1 || d.after_rays?1:0;
            v.color[2]=d.layer==CalibratedGameLayer::world || d.source_key==2?1:0;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        return render(f);
    };
    unsigned compared=0,protected_bytes=0,changed=0;
    const auto moving=[&](double time,unsigned variant=0) {
        auto f=std::make_shared<CalibratedGameFrame>(*base);f->presentation_seconds=time;
        f->draws[1].packet.model[12]=.28F;f->draws[1].packet.model[13]=-.04F;f->draws[1].packet.model[14]=-.07F;
        f->draws[2].packet.model[12]=.19F;
        if(variant==1) {auto s=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);s->transforms[1].generation=2;f->current=s;}
        if(variant==2) for(auto& v:f->draws[1].packet.geometry.vertices) v.position[1]+=.02F;
        if(variant==3) f->settings.history_epoch=1;
        return f;
    };
    const auto primitive=[&](const std::shared_ptr<CalibratedGameFrame>& f,unsigned variant,float phase) {
        if(variant==4) {
            auto state=std::make_shared<starfox::vr::GameSceneSnapshot>(*f->current);
            state->transforms[1].explosion_progress=unsigned(phase);f->current=state;
            for(auto& v:f->draws[1].packet.geometry.vertices) {
                v.position[0]*=256;v.position[1]*=-256;v.position[2]*=-256;
                v.visibility_enabled=2;v.visibility_a[0]=v.visibility_b[1]=v.visibility_c[2]=1;
                v.group_b[0]=16;v.group_c[0]=phase;v.group_c[1]=256;
            }
        } else if(variant==5) {
            for(auto& v:f->draws[1].packet.geometry.vertices) {
                v.billboard[0]=v.position[0];v.billboard[1]=v.position[1];
                v.position[0]=v.position[1]=0;v.texture[3]=4;
            }
        }
    };
    const auto initial=render(base);
    for(unsigned quality=1;quality<=3;++quality) for(unsigned rate:{60U,120U,240U}) for(unsigned variant=0;variant<6;++variant) {
        const double interval=1./rate;
        auto current=moving(10+interval,variant);
        primitive(current,variant,12);
        native_mock.head_x=2;
        const auto tags=labels(current,false),overlay_tags=labels(current,true),with_ui=render(current);
        auto raw=std::make_shared<CalibratedGameFrame>(*current);std::erase_if(raw->draws,[](const auto& d){return d.after_rays;});
        const auto foreground=render(raw);
        auto world=std::make_shared<CalibratedGameFrame>(*current);
        std::erase_if(world->draws,[](const auto& d){return d.layer!=CalibratedGameLayer::world;});
        const auto underlay=render(world);
        auto old=std::make_shared<CalibratedGameFrame>(*base);old->presentation_seconds=10;old->settings.motion_blur=quality;
        primitive(old,variant,4);
        native_mock.head_x=0;(void)render(old);native_mock.head_x=2;current->settings.motion_blur=quality;
        const auto actual=render(current);
        const unsigned w=mock.image_width,h=mock.image_height;
        for(unsigned eye=0;eye<2;++eye) {
            std::vector<MotionBlurGuide> guides(w*h);
            const double l=std::tan(-.5),r=std::tan(.6),d=std::tan(-.5),u=std::tan(.4);
            const double sx=2/(r-l),sy=2/(u-d),cx=(r+l)/(r-l),cy=(u+d)/(u-d);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned i=y*w+x,at=i*4;auto& g=guides[i];
                const auto& label=tags[eye];g.eligible=label[at]==0;
                if(!g.eligible || (label[at+1]==255 && label[at+2]==255)) continue;
                const unsigned object=label[at+1]==255?1:2;
                const auto& now=current->draws[object].packet.model;const auto& before=base->draws[object].packet.model;
                const double z=(object==1?2:1.1)-now[14]+4./256,prior=(object==1?2:1.1)-before[14]+4./256;
                g.depth=float(z*256);g.valid=variant!=3 && !(object==1 && (variant==1 || variant==2));
                if(!g.valid) continue;
                // Floating destruction: x += ((16*phase)>>2)/256.
                // These phases are exact integer words; no GPU transform or
                // private accepted-primitive data supplies this reference.
                const double destruction=object==1 && variant==4?(12.-4.)/64:0;
                const double px=((x+.5)*2/w-1+cx)*z/sx+(2+(eye?8:-8))/256.-now[12]+before[12]-destruction;
                const double py=(1-(y+.5)*2/h+cy)*z/sy+2./256-now[13]+before[13];
                const double priorx=((px-(eye?8:-8)/256.)*sx/prior-cx+1)*w/2;
                const double priory=(1-(py-2./256)*sy/prior+cy)*h/2;
                g.motion_x=float(priorx-(x+.5));g.motion_y=float(priory-(y+.5));
            }
            auto shutter=motion_blur_preset(quality,std::max(1U,unsigned(std::lround(double(h)/224))));shutter.interval_seconds=variant==3?0:interval;
            std::vector<unsigned char> expected;
            check(reconstruct_motion_blur(w,h,foreground[eye],underlay[eye],guides,shutter,expected),"Independent native shutter reference rejected planes");
            for(unsigned at=0;at<actual[eye].size();at+=4) {
                const bool overlay=overlay_tags[eye][at]==255 && overlay_tags[eye][at+1]==255;
                if(overlay) std::copy_n(with_ui[eye].begin()+at,4,expected.begin()+at);
                const bool ink=tags[eye][at]==255 || overlay;
                for(unsigned c=0;c<4;++c) {
                    const int tolerance=ink || c==3?0:2;
                    if(std::abs(int(actual[eye][at+c])-int(expected[at+c]))>tolerance)
                        throw std::runtime_error(std::string(backend)+" native blur owner mismatch q="+std::to_string(quality)+" rate="+std::to_string(rate)
                            +" variant="+std::to_string(variant)+" eye="+std::to_string(eye)+" at="+std::to_string(at+c)
                            +" actual="+std::to_string(actual[eye][at+c])+" expected="+std::to_string(expected[at+c]));
                    ++checks;++compared;if(ink) ++protected_bytes;
                    changed+=actual[eye][at+c]!=with_ui[eye][at+c];
                }
            }
        }
        auto paused=std::make_shared<CalibratedGameFrame>(*current);paused->settings.exposure_paused=true;paused->presentation_seconds+=interval;
        check(render(paused)==with_ui,"Paused native shutter changed colour or retained moving velocity");
        check(render(current)==with_ui,"Resumed native shutter reused paused motion history");
    }
    if(compared<=10000 || protected_bytes<=100 || changed<=100)
        throw std::runtime_error("Native blur owner missed geometry/swept scenery/protected coverage: compared="+std::to_string(compared)
            +" protected="+std::to_string(protected_bytes)+" changed="+std::to_string(changed));
    // A newly born particle has no prior velocity. With held geometry/eyes,
    // joint exposure must equal the ordinary single particle pass, not render
    // it twice. The changed birth identity still opens a physical shutter.
    for(unsigned type=2;type<=7;++type) {
        native_mock.head_x=0;
        auto first=std::make_shared<CalibratedGameFrame>(*base);first->settings.motion_blur=2;
        first->settings.scene_enhancements=type==2?16:type<=5?64:0;
        first->settings.particle_enhancements=type>=6?1:0;first->presentation_seconds=19+type*.1;
        first->scene_fx.add({30,-10,500},40,.7F,float(type),.08F,{.6F,.7F,.2F,0},{int(type),3001,0,0});
        (void)render(first);auto fresh=std::make_shared<CalibratedGameFrame>(*first);
        fresh->presentation_seconds+=1./120;fresh->scene_fx.points[0].identity[1]++;
        const auto actual=render(fresh);
        auto ordinary=std::make_shared<CalibratedGameFrame>(*fresh);ordinary->settings.motion_blur=0;
        const auto expected=render(ordinary);
        for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();++at)
            check(std::abs(int(actual[eye][at])-int(expected[eye][at]))<=(at%4==3?0:2),
                "Native joint shutter duplicates or drops a current-only particle");
    }
    for(unsigned type:{0U,1U,2U,3U,4U,5U,6U}) for(unsigned quality=1;quality<=3;++quality) {
        auto old=std::make_shared<CalibratedGameFrame>(*base);old->settings.motion_blur=quality;
        old->settings.aa_type=type;old->settings.aa_quality=quality;old->presentation_seconds=20;
        old->settings.bloom_model=2;old->settings.bloom_world=1;old->settings.contrast=2;old->settings.chromatic=1;
        old->settings.depth_enhancements=15;old->settings.volumetric_fog=1;
        old->settings.global_enhancements=global_enhancement_mask;old->settings.world_effects={unsigned(Effect::sepia),70,0,0};
        old->settings.exposure=2;old->settings.phosphor=2;
        old->settings.scene_enhancements=64;old->settings.particle_enhancements=1;
        old->scene_fx.add({12,-15,480},35,.7F,3,.08F,{.3F,.5F,.9F,0},{3,1001,0,0});
        old->scene_fx.add({-45,20,650},45,.8F,7,.05F,{.6F,.4F,.2F,0},{7,1002,0,0});
        if(rays) {old->settings.ray_tracing=old->settings.reflections=2;old->settings.material=unsigned(Effect::gold_metal);}
        native_mock.head_x=0;(void)render(old);
        auto current=moving(20+1./120);current->settings=old->settings;native_mock.head_x=2;
        current->scene_fx=old->scene_fx;current->scene_fx.points[0].position[0]+=60;
        current->scene_fx.points[1].position[1]-=30;current->scene_fx.points[1].age+=1.F/120;
        const auto actual=render(current);check(actual[0]!=actual[1],"Combined shutter/AA collapsed calibrated eyes");
        auto no_particles=std::make_shared<CalibratedGameFrame>(*current);no_particles->scene_fx={};
        native_mock.head_x=0;(void)render(old);native_mock.head_x=2;
        check(render(no_particles)!=actual,"Combined native shutter dropped its joint particle layer");
        for(const auto& e:actual) for(unsigned at=3;at<e.size();at+=4) check(e[at]==255,"Combined native shutter changed source alpha");
        auto off=std::make_shared<CalibratedGameFrame>(*current);off->settings.motion_blur=0;const auto no_blur=render(off);
        current->settings.exposure_paused=true;current->presentation_seconds+=1./120;
        const auto paused=render(current);check(render(current)==paused,"Held combined native blur/exposure/history drifted");
        // A changed settings/paused frame must be the same first-frame effect
        // pipeline as OFF, not depend on a stale accepted motion stream.
        auto clean=std::make_shared<CalibratedGameFrame>(*current);clean->settings.motion_blur=0;++clean->settings.history_epoch;
        check(render(clean)==paused,"Paused combined shutter differs from independent OFF effect pipeline");
        (void)no_blur;
    }
    native_mock.head_x=0;
    check(render(base)==initial,"Native shutter OFF did not restore exact original images");
    auto selected=std::make_shared<CalibratedGameFrame>(*base);selected->settings.motion_blur=2;selected->presentation_seconds=30;
    selected->settings.scene_enhancements=64;selected->settings.particle_enhancements=1;
    selected->scene_fx.add({20,-15,480},35,.7F,3,.08F,{.3F,.5F,.9F,0},{3,2001,0,0});
    selected->scene_fx.add({-45,20,650},45,.8F,7,.05F,{.6F,.4F,.2F,0},{7,2002,0,0});
    (void)render(selected);auto pending=moving(30+1./120);pending->settings=selected->settings;native_mock.head_x=2;
    pending->scene_fx=selected->scene_fx;pending->scene_fx.points[0].position[0]+=25;
    gpu_mock.timeout_right=true;
    check(renderer.submit(pending)==DisplayXrSubmission::waiting && renderer.retained_frame()==pending,"Native shutter XR wait lost accepted source/time");
    const auto locates=native_mock.locates;check(renderer.poll()==DisplayXrSubmission::waiting && native_mock.locates==locates,"Native shutter retry reacquired poses");
    gpu_mock.timeout_right=false;auto outcome=renderer.poll();
    for(unsigned i=0;outcome==DisplayXrSubmission::waiting && i<10000;++i) {SDL_Delay(1);outcome=renderer.poll();}
    check(outcome==DisplayXrSubmission::submitted,"Native shutter XR image retry failed");(void)consume();
    auto rejected=moving(30+2./120);rejected->settings=selected->settings;rejected->draws[1].packet.model[12]+=.12F;
    rejected->scene_fx=pending->scene_fx;rejected->scene_fx.points[1].position[1]-=20;
    gpu_mock.fail_end=true;outcome=renderer.submit(rejected);
    for(unsigned i=0;renderer.frame_pending() && i<10000;++i) {SDL_Delay(1);outcome=renderer.poll();}
    check(outcome==DisplayXrSubmission::failed && !renderer.retained_frame(),"Rejected native shutter accepted candidate history");(void)consume();
    const auto recovered=render(rejected);auto pristine=std::make_shared<CalibratedGameFrame>(*rejected);++pristine->settings.history_epoch;
    check(render(pristine)==recovered,"Rejected native shutter seeded later motion");
    std::cout<<backend<<" native blur owner: "<<compared<<" scalar plane/shutter bytes, "<<protected_bytes<<" exact protected bytes, "<<changed
        <<" responses; three qualities, 60/120/240 clocks, current/previous Z, actual destruction/billboard correspondence, swept scenery, recycled/morph/cut, all AA/combined particle effects, pause/OFF and rejected/XR waits passed.\n";
    return pristine;
}
