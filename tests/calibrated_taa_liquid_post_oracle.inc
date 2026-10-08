// Full-image post-order oracle, not an independent optical water renderer.
// Raw reconstructed radiance is rendered on a separate unstyled timeline. Centre object
// boundaries come from an independent tag raster; the authored floor plane
// and rectangle depths determine which objects are submerged. No private
// owner/history/surface images or production plane helpers are consulted.
template<class Render,class Consume>
std::shared_ptr<const CalibratedGameFrame> native_liquid_post_owner_oracle(
    Render submit,Consume consume,DisplayXrGameRenderer& renderer,bool srgb,const char* backend,
    std::optional<unsigned> sdk_model={}) {
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto rgba=[&](Images images) {
        if(bgra) for(auto& eye:images) for(unsigned at=0;at<eye.size();at+=4) std::swap(eye[at],eye[at+2]);
        return images;
    };
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> f){return rgba(submit(std::move(f)));};
    const unsigned w=mock.image_width,h=mock.image_height;
    check(w==256 && h==224,"Liquid post oracle requires its authored centre grid");
    check(!sdk_model || *sdk_model<=1,"Invalid liquid post SDK model");
    const std::string route=sdk_model?"SDK liquid post model "+std::to_string(*sdk_model):"TAA liquid post";
    struct Restore {
        bool identity;
        ~Restore() {dlss_fixture::identity_dlaa=identity;native_mock.head_x=native_mock.sample_x=native_mock.sample_y=0;}
    } restore{dlss_fixture::identity_dlaa};
    // Run the actual SDK allocation/evaluation/acceptance path. Only its opaque
    // filter is followed by an equal-size identity copy, so this is a native
    // post-order oracle, not a claim about private neural-filter quality.
    if(sdk_model) dlss_fixture::identity_dlaa=true;
    const auto select_reconstruction=[&](CalibratedGameSettings& settings,unsigned quality) {
        if(sdk_model) {
            constexpr unsigned source_aa[]{0,5,6};
            settings.dlss_mode=4;settings.dlss_model=*sdk_model;
            settings.aa_type=source_aa[quality-1];settings.aa_quality=settings.aa_type?2:0;
        } else {settings.aa_type=4;settings.aa_quality=quality;}
    };
    constexpr unsigned frames=4;
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.04F,.07F,.12F,1};
    base->settings.enhanced_ground=true;base->settings.ground_material=5;base->settings.ray_tracing=2;
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();snapshot->dots_mode=-1;
    base->current=base->previous=snapshot;
    for(unsigned key:{1U,2U}) {snapshot->transforms[key].generation=1;snapshot->transforms[key].shape=10+key;}
    const auto add=[&](float z,float radius,float x,float y,unsigned key,bool protected_ink,bool after=false) {
        starfox::vr::DrawPacket packet;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=corner[0]*radius;v.position[1]=corner[1]*radius;v.position[2]=z;
            v.color[0]=protected_ink?1:corner[0]>0?.85F:.15F;
            v.color[1]=protected_ink?.4F:corner[1]>0?.65F:.1F;v.color[2]=.3F;v.color[3]=1;
            v.dither_scale=protected_ink?0:1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
            if(!protected_ink) {v.odd_color[0]*=.35F;v.odd_color[1]*=.6F;}
            packet.geometry.vertices.push_back(v);
        }
        packet.model[12]=x;packet.model[13]=y;packet.preserve_native_colour=protected_ink;
        base->draws.push_back({std::move(packet),protected_ink?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
            starfox::vr::SceneBlend::opaque,!after,key,!protected_ink,after});
    };
    add(-2,1.16F,0,-.15F,1,false); // A large object extending below the water plane.
    add(-1.1F,.12F,.24F,-.3F,2,false); // Nearer opaque model in front of the water.
    add(-.8F,.045F,-.12F,-.29F,0,true);
    add(-.7F,.035F,.1F,.14F,0,true,true);
    starfox::simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
    ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
    for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
    for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
    auto original_floor=starfox::vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);
    starfox::vr::place_landscape_ground(original_floor,-.75F,true);
    auto tag_ppu=ppu;tag_ppu.cgram.fill(31U|(31U<<5));
    auto tag_floor=starfox::vr::landscape_sphere_packet(tag_ppu,{},112,srgb,false,232);
    starfox::vr::place_landscape_ground(tag_floor,-.75F,true);
    const CalibratedGroundGradient tag_gradient{{1,1,0},{1,1,0},112,111};
    check(apply_calibrated_ground(tag_floor,tag_gradient),"Independent centre floor tag failed");
    CalibratedGroundGradient gradient{{.24F,.19F,.12F},{.12F,.09F,.055F},112,111};
    gradient.material=5;gradient.seconds=1.75F;gradient.origin={123,-415};
    auto floor=original_floor;check(apply_calibrated_ground(floor,gradient),"Liquid post floor payload failed");
    // Match game assembly: draw the backdrop first, then the opaque actors.
    // A last, depth-disabled backdrop overwrites actors/protected foreground
    // in both the primary scene and independent label raster, silently removing
    // the submerged-object coverage this oracle is required to exercise.
    base->draws.insert(base->draws.begin(),{std::move(floor),CalibratedGameLayer::world,starfox::vr::SceneBlend::opaque,false,
        calibrated_ground_source_key,false});base->draws.front().reflection_environment=true;
    std::array<std::shared_ptr<CalibratedGameFrame>,frames> sources;
    std::array<Images,frames> tags,overlay_tags,overlay_colours;
    const auto head=[](unsigned f){return float(f)*2.25F;};
    const auto bank=[](unsigned f){return float(f)*.045F;};
    for(unsigned f=0;f<frames;++f) {
        auto source=std::make_shared<CalibratedGameFrame>(*base);
        auto tick=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);tick->revision=f;
        source->current=source->previous=tick;source->settings.effect_seconds=1.75F+float(f)/30;
        source->draws[1].packet.model[12]=float(f)*.017F;source->draws[2].packet.model[12]-=float(f)*.009F;
        auto timed=gradient;timed.seconds=source->settings.effect_seconds;
        source->draws.front().packet=original_floor;
        check(apply_calibrated_ground(source->draws.front().packet,timed),"Moving liquid floor payload failed");
        const float a=bank(f),c=std::cos(a),s=std::sin(a);
        auto& matrix=source->draws.front().packet.model;matrix[0]=c;matrix[1]=s;matrix[4]=-s;matrix[5]=c;
        sources[f]=source;
        auto label=std::make_shared<CalibratedGameFrame>(*source);label->clear={0,0,0,1};
        label->settings.ray_tracing=0;label->settings.ground_material=0;
        const auto tag_matrix=label->draws.front().packet.model;
        label->draws.front().packet=tag_floor;label->draws.front().packet.model=tag_matrix;
        for(auto& draw:label->draws) if(draw.layer!=CalibratedGameLayer::world) for(auto& v:draw.packet.geometry.vertices) {
            v.dither_scale=0;v.color[0]=draw.source_key==1 || draw.after_rays?1:0;
            v.color[1]=draw.source_key==2 || draw.after_rays?1:0;
            v.color[2]=draw.layer==CalibratedGameLayer::native_ui?1:0;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        native_mock.head_x=head(f);overlay_tags[f]=render(label);overlay_colours[f]=render(source);
        std::erase_if(label->draws,[](const auto& d){return d.after_rays;});tags[f]=render(label);
    }
    // Point palettes, patterns, drawn edges, neighbourhood effects, ordered
    // transforms and screen finishing all consume the same physical labels.
    std::vector<CalibratedGameSettings> cases;
    for(auto ids:std::array<std::array<unsigned,2>,6>{{{17,5},{70,17},{6,12},{33,35},{42,7},{17,70}}}) {
        auto s=base->settings;s.model_effects={ids[0],100,0,0};s.world_effects={ids[1],100,0,0};cases.push_back(s);
    }
    auto mixed=base->settings;mixed.model_effects={6,35,0,0};mixed.world_effects={70,100,0,0};
    mixed.manipulation=43;mixed.extra_effects[0]=45;cases.push_back(mixed);
    mixed=base->settings;mixed.model_effects={33,35,0,0};mixed.world_effects={17,70,0,0};
    mixed.extra_effects={53,84,88};cases.push_back(mixed);
    mixed=base->settings;mixed.contrast=2;mixed.chromatic=1;mixed.bloom_model=2;mixed.bloom_world=1;cases.push_back(mixed);
    unsigned compared=0,protected_bytes=0,visible=0,submerged=0,dry_models=0,held=0,rejected=0;
    std::shared_ptr<const CalibratedGameFrame> final_frame;
    for(unsigned quality=1;quality<=3;++quality) {
        native_mock.head_x=0;(void)render(base);
        std::array<Images,frames> raw;
        for(unsigned f=0;f<frames;++f) {
            auto reference=std::make_shared<CalibratedGameFrame>(*sources[f]);select_reconstruction(reference->settings,quality);
            std::erase_if(reference->draws,[](const auto& d){return d.after_rays;});native_mock.head_x=head(f);raw[f]=render(reference);
        }
        for(unsigned selection=0;selection<cases.size();++selection) {
            native_mock.head_x=0;(void)render(base);
            for(unsigned f=0;f<frames;++f) {
                auto selected=std::make_shared<CalibratedGameFrame>(*sources[f]);selected->settings=cases[selection];
                selected->settings.effect_seconds=sources[f]->settings.effect_seconds;
                select_reconstruction(selected->settings,quality);
                native_mock.head_x=head(f);const auto evaluations=dlss_fixture::evaluations.size();const auto actual=render(selected);
                if(sdk_model) check(dlss_fixture::evaluations.size()==evaluations+2,
                    "Liquid post SDK oracle did not evaluate both actual SDK eyes");
                check(actual[0]!=actual[1],"Liquid post lost independent calibrated eyes");
                for(unsigned eye=0;eye<2;++eye) {
                    auto expected=raw[f][eye];Framebuffer ownership(w,h,1);ownership.enable_layer_tags(true);
                    const double l=std::tan(double(-.5F)),r=std::tan(double(.6F)),u=std::tan(double(.4F)),d=std::tan(double(-.5F));
                    const double eye_x=double((head(f)+(eye?8.F:-8.F))*native_mock.pose_scale);
                    const double eye_y=double(2*native_mock.pose_scale),eye_z=double(4*native_mock.pose_scale);
                    // The authored Z rotation maps local Y=-.75 to this plane.
                    const double nx=-double(std::sin(bank(f))),ny=double(std::cos(bank(f)));
                    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                        const unsigned p=y*w+x,at=p*4;const auto& label=tags[f][eye];
                        check((label[at]==0 || label[at]==255) && (label[at+1]==0 || label[at+1]==255)
                            && (label[at+2]==0 || label[at+2]==255),"Nonbinary independent liquid centre tag");
                        const bool world=label[at] && label[at+1];
                        // Clear, like the authored sky, is a world background;
                        // only explicitly tagged native artwork is protected.
                        unsigned layer=label[at+2]?unsigned(PixelLayer::two_d):
                            world || (!label[at] && !label[at+1])?unsigned(PixelLayer::background):unsigned(PixelLayer::three_d);
                        if(layer==unsigned(PixelLayer::three_d)) {
                            const double dx=l+(r-l)*(double(x)+.5)/w,dy=u-(u-d)*(double(y)+.5)/h;
                            const double denom=nx*dx+ny*dy;
                            const double distance=denom<0?-(nx*eye_x+ny*eye_y+.75)/denom:-1;
                            const double model_distance=eye_z+(label[at]?2:double(1.1F));
                            if(distance>0 && distance<model_distance) {layer=unsigned(PixelLayer::background);++submerged;}
                            else ++dry_models;
                        }
                        ownership.layer_tags()[p]=layer;
                    }
                    std::vector<unsigned char> scratch;
                    for(const auto& p:calibrated_game_post_effects(selected->settings))
                        apply_effect(static_cast<Effect>(p.model),ownership,expected,scratch,p.model_intensity,
                            static_cast<Effect>(p.world),p.world_intensity,p.seconds);
                    apply_hdr_effect(ownership,expected,selected->settings.contrast);
                    apply_chromatic_aberration(ownership,expected,scratch,selected->settings.chromatic);
                    BloomPass bloom;bloom.apply(selected->settings.bloom_model,selected->settings.bloom_world,ownership,expected);
                    for(unsigned p=0;p<w*h;++p) {
                        const unsigned at=p*4;const auto& overlay=overlay_tags[f][eye];
                        if(overlay[at]==255 && overlay[at+1]==255 && overlay[at+2]==255) {
                            std::copy_n(overlay_colours[f][eye].begin()+at,4,expected.begin()+at);ownership.layer_tags()[p]=unsigned(PixelLayer::two_d);
                        }
                    }
                    for(unsigned at=0;at<expected.size();++at) {
                        const bool protected_pixel=ownership.layer_tags()[at/4]==unsigned(PixelLayer::two_d);
                        const unsigned tolerance=at%4==3 || protected_pixel?0:srgb || selection>=6?2:0;
                        if(std::abs(int(actual[eye][at])-int(expected[at]))>int(tolerance))
                            throw std::runtime_error(std::string(backend)+" "+route+" mismatch q="+std::to_string(quality)
                                +" selection="+std::to_string(selection)+" frame="+std::to_string(f)+" eye="+std::to_string(eye)
                                +" byte="+std::to_string(at)+" actual="+std::to_string(actual[eye][at])+" expected="+std::to_string(expected[at])
                                +" layer="+std::to_string(ownership.layer_tags()[at/4])
                                +" raw="+std::to_string(raw[f][eye][at])
                                +" centre_tag="+std::to_string(tags[f][eye][at/4*4])+","
                                    +std::to_string(tags[f][eye][at/4*4+1])+","
                                    +std::to_string(tags[f][eye][at/4*4+2]));
                        ++checks;++compared;protected_bytes+=protected_pixel;visible+=actual[eye][at]!=raw[f][eye][at];
                    }
                }
                const auto held_evaluations=dlss_fixture::evaluations.size();
                check(render(selected)==actual,"Held liquid post rotated target pairs or advanced history");
                if(sdk_model) check(dlss_fixture::evaluations.size()==held_evaluations,"Held liquid post reevaluated the SDK");
                ++held;final_frame=selected;
                if(selection==cases.size()-1 && f==frames-1) {
                    gpu_mock.timeout_right=true;check(renderer.submit(selected)==DisplayXrSubmission::waiting,"Liquid wait lost retained eyes");
                    check(renderer.poll()==DisplayXrSubmission::waiting,"Liquid wait reacquired an eye");gpu_mock.timeout_right=false;
                    auto outcome=renderer.poll();for(unsigned i=0;outcome==DisplayXrSubmission::waiting && i<10000;++i) {SDL_Delay(1);outcome=renderer.poll();}
                    check(outcome==DisplayXrSubmission::submitted && rgba(consume())==actual,"Liquid wait changed retained MRTs");
                    auto failed=std::make_shared<CalibratedGameFrame>(*selected);failed->draws[1].packet.model[12]+=.31F;
                    gpu_mock.fail_end=true;outcome=renderer.submit(failed);
                    for(unsigned i=0;renderer.frame_pending() && i<10000;++i) {SDL_Delay(1);outcome=renderer.poll();}
                    check(outcome==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected liquid frame remained pending");
                    (void)consume();check(render(selected)==actual,"Rejected liquid frame replaced accepted radiance/targets");++rejected;
                }
            }
            std::cout<<backend<<" "<<route<<" selection "<<selection<<" quality "<<quality<<": full image CPU order and authored plane oracle passed\n";
        }
    }
    check(compared>100000 && protected_bytes>1000 && visible>1000 && submerged>1000 && dry_models>1000
        && held==cases.size()*frames*3 && rejected==3,
        ("Liquid post missed wet/dry/protected/moving/held/retry coverage: compared="+std::to_string(compared)
            +" protected="+std::to_string(protected_bytes)+" changed="+std::to_string(visible)
            +" submerged="+std::to_string(submerged)+" dry="+std::to_string(dry_models)
            +" held="+std::to_string(held)+" rejected="+std::to_string(rejected)).c_str());
    native_mock.head_x=native_mock.sample_x=native_mock.sample_y=0;
    std::cout<<backend<<" "<<route<<": 9 selections, 3 qualities, "<<compared<<" compared bytes, "<<protected_bytes
        <<" exact protected bytes, "<<submerged<<" submerged centre pixels, "<<dry_models<<" dry model pixels, "
        <<held<<" held frames and "<<rejected<<" rejected/wait recoveries passed\n";
    return final_frame;
}
