// Independent CPU effects consume a separately rendered UNSTYLED temporal
// timeline (native TAA or the actual opaque K/M SDK, not an emulated SDK).
// This does not read private colour/history/ownership or use the owner's style
// ordering policy. Geometric labels come from a separate centre-only raster.
template<class Render,class Consume>
void native_reconstructed_post_owner_oracle(Render submit,Consume consume,DisplayXrGameRenderer& renderer,
    bool srgb,const char* backend,bool sdk=false,bool panel_smoke=false) {
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto rgba=[&](Images images) {
        if(bgra) for(auto& eye:images) for(unsigned at=0;at<eye.size();at+=4) std::swap(eye[at],eye[at+2]);
        return images;
    };
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> f){return rgba(submit(std::move(f)));};
    constexpr unsigned count=4;
    const unsigned w=mock.image_width,h=mock.image_height;
    const unsigned scale=std::max(1U,unsigned(std::lround(double(h)/224)));
    check(w%scale==0 && h%scale==0,"Reconstructed post fixture cannot represent its native grid");
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.04F,.07F,.12F,1};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();base->previous=base->current=snapshot;
    snapshot->transforms[1].generation=snapshot->transforms[2].generation=1;
    snapshot->transforms[1].shape=11;snapshot->transforms[2].shape=12;
    const auto add=[&](float z,float radius,unsigned key,CalibratedGameLayer layer,bool after=false) {
        starfox::vr::DrawPacket packet;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=corner[0]*radius+corner[1]*radius*.19F;
            v.position[1]=corner[1]*radius+corner[0]*radius*.11F;v.position[2]=z;
            v.color[0]=corner[0]>0?.85F:.15F;v.color[1]=corner[1]>0?.65F:.1F;v.color[2]=.3F;v.color[3]=1;
            v.dither_scale=layer==CalibratedGameLayer::model?1:0;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);v.odd_color[0]*=.35F;v.odd_color[1]*=.6F;
            packet.geometry.vertices.push_back(v);
        }
        packet.preserve_native_colour=layer==CalibratedGameLayer::native_ui;
        base->draws.push_back({std::move(packet),layer,starfox::vr::SceneBlend::opaque,
            layer==CalibratedGameLayer::model,key,layer==CalibratedGameLayer::model,after});
    };
    add(-4,2,0,CalibratedGameLayer::world);
    add(-2,.59F,1,CalibratedGameLayer::model);
    add(-1.1F,.1F,2,CalibratedGameLayer::model);
    base->draws[2].packet.model[12]=.24F;base->draws[2].packet.model[13]=.13F;
    add(-.8F,.06F,0,CalibratedGameLayer::native_ui);base->draws[3].packet.model[12]=-.12F;
    add(-.7F,.04F,0,CalibratedGameLayer::native_ui,true);base->draws[4].packet.model[13]=.14F;
    std::array<std::shared_ptr<CalibratedGameFrame>,count> sources;
    std::array<Images,count> labels,overlay_labels,overlay_colours;
    const auto head=[](unsigned f){return float(f)*.75F;};
    for(unsigned f=0;f<count;++f) {
        auto source=std::make_shared<CalibratedGameFrame>(*base);
        auto tick=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);tick->revision=f;
        source->previous=source->current=tick;source->settings.effect_seconds=1.75F+float(f)/60;
        source->draws[1].packet.model[12]=float(f)*.017F;source->draws[1].packet.model[13]=float(f)*-.005F;
        source->draws[2].packet.model[12]+=float(f)*-.009F;sources[f]=source;
        auto label=std::make_shared<CalibratedGameFrame>(*source);label->clear={1,0,0,1};
        for(auto& draw:label->draws) for(auto& v:draw.packet.geometry.vertices) {
            v.dither_scale=0;v.color[0]=draw.after_rays || draw.layer==CalibratedGameLayer::world?1:0;
            v.color[1]=draw.after_rays || draw.layer==CalibratedGameLayer::model?1:0;
            v.color[2]=!draw.after_rays && draw.layer==CalibratedGameLayer::native_ui?1:0;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        native_mock.head_x=head(f);overlay_labels[f]=render(label);overlay_colours[f]=render(source);
        std::erase_if(label->draws,[](const auto& draw){return draw.after_rays;});labels[f]=render(label);
    }
    // The authored IDs are independent of the selector under test: five
    // multi-tap styles, twelve static transforms, eight animated shader FX.
    constexpr unsigned ids[]{7,33,35,40,42,43,44,45,48,49,50,51,52,53,58,59,60,84,85,86,87,88,89,90,91};
    std::vector<CalibratedGameSettings> cases;
    for(unsigned id:ids) {
        auto s=base->settings;
        const unsigned strength=id%2?35:100;
        if(id>=84 && id<=89) s.extra_effects={0,id,id};
        else if(id>=43) {s.manipulation=id;s.manipulation_intensity=strength;s.extra_effects[0]=id;}
        else {s.model_effects={id,strength,0,0};s.world_effects={id,strength,0,0};}
        cases.push_back(s);
    }
    // Ordered dependencies, patterns and witnessed edges must stay in the
    // same chain, not straddle reconstruction merely because they are tracked.
    auto mixed=base->settings;mixed.model_effects={33,35,0,0};mixed.world_effects={17,70,0,0};
    mixed.manipulation=50;mixed.manipulation_intensity=70;mixed.extra_effects={53,84,88};cases.push_back(mixed);
    mixed=base->settings;mixed.model_effects={6,35,0,0};mixed.world_effects={70,100,0,0};
    mixed.manipulation=43;mixed.extra_effects[0]=45;cases.push_back(mixed);
    mixed=base->settings;mixed.model_effects={42,100,0,0};mixed.world_effects={5,35,0,0};
    mixed.extra_effects={0,85,0};cases.push_back(mixed);
    // Screen-space finishing also belongs after geometric reconstruction,
    // including a pure finishing selection with no primary style or warp.
    mixed=base->settings;mixed.contrast=2;mixed.chromatic=1;mixed.global_enhancements=global_enhancement_mask;
    mixed.bloom_model=2;mixed.bloom_world=1;cases.push_back(mixed);
    mixed.model_effects={33,35,0,0};mixed.world_effects={17,70,0,0};
    mixed.manipulation=50;mixed.manipulation_intensity=70;cases.push_back(mixed);
    mixed.manipulation=0;mixed.model_effects={6,35,0,0};mixed.world_effects={70,100,0,0};cases.push_back(mixed);
    mixed=base->settings;mixed.manipulation=46;mixed.manipulation_intensity=70;mixed.extra_effects[0]=47;cases.push_back(mixed);
    mixed=base->settings;mixed.manipulation=47;mixed.manipulation_intensity=35;mixed.phosphor=3;cases.push_back(mixed);
    mixed=base->settings;mixed.world_effects={17,70,0,0};mixed.extra_effects={0,85,0};
    mixed.manipulation=46;mixed.manipulation_intensity=35;mixed.phosphor=2;mixed.exposure=3;cases.push_back(mixed);
    unsigned last_temporal_case=unsigned(cases.size()-1),sdk_edge_begin=unsigned(cases.size());
    if(sdk) {
        // Independent authored edge IDs: the SDK has no native TAA edge
        // witnesses. Pure drawn styles and an ordered drawn/coordinate
        // chain must run on the full reconstructed panel, not reject local
        // history on the low-resolution input grid.
        constexpr unsigned edge_ids[]{1,2,3,6,12,13,34,37,39,41,66,67,68};
        for(unsigned id:edge_ids) {
            auto s=base->settings;const unsigned strength=id%2?35:100;
            s.model_effects={id,strength,0,0};s.world_effects={id,strength,0,0};cases.push_back(s);
        }
        mixed=base->settings;mixed.model_effects={1,35,0,0};mixed.world_effects={6,100,0,0};
        mixed.manipulation=43;mixed.manipulation_intensity=70;mixed.extra_effects[0]=53;cases.push_back(mixed);
    }
    if(panel_smoke) {
        check(sdk && w==1920 && h==1080,"Panel memory oracle requires actual full-HD SDK eye targets");
        auto edge=base->settings;edge.model_effects={6,100,0,0};edge.world_effects={6,100,0,0};
        auto history=edge;history.manipulation=46;history.manipulation_intensity=70;history.extra_effects[0]=47;
        history.phosphor=2;history.exposure=2;history.contrast=2;history.chromatic=1;
        history.bloom_model=2;history.bloom_world=1;
        cases={edge,history};last_temporal_case=1;sdk_edge_begin=0;
    }
    unsigned compared=0,protected_bytes=0,visible=0,held_frames=0,rejections=0;
    const unsigned qualities=panel_smoke?2:sdk?14:3;
    const auto select_reconstruction=[&](CalibratedGameSettings& settings,unsigned quality) {
        if(sdk) {
            if(panel_smoke) {settings.dlss_mode=4;settings.dlss_model=quality-1;}
            else if(quality<=8) {settings.dlss_mode=(quality-1)%4+1;settings.dlss_model=(quality-1)/4;}
            else {
                constexpr unsigned grids[]{1,3,6};settings.dlss_mode=2;settings.dlss_model=(quality-9)/3;
                settings.aa_type=grids[(quality-9)%3];settings.aa_quality=2;
            }
        }
        else {settings.aa_type=4;settings.aa_quality=quality;}
    };
    unsigned static_clocks=0,animated_clocks=0;
    for(unsigned quality=1;quality<=qualities;++quality) {
        native_mock.head_x=0;(void)render(base);
        std::array<Images,count> reconstructed;
        for(unsigned f=0;f<count;++f) {
            auto raw=std::make_shared<CalibratedGameFrame>(*sources[f]);select_reconstruction(raw->settings,quality);
            // Post-world artwork is composited only after styles/finishing.
            // It must not enter the independent neighbourhood source or masks.
            std::erase_if(raw->draws,[](const auto& draw){return draw.after_rays;});
            native_mock.head_x=head(f);reconstructed[f]=render(raw);
        }
        for(unsigned c=0;c<cases.size();++c) {
            native_mock.head_x=0;(void)render(base); // Destroy, do not inherit the reference's private history.
            std::array<std::array<FramePersistence,3>,2> trails;
            std::array<AdaptiveExposure,2> exposure;
            for(unsigned f=0;f<count;++f) {
                auto selected=std::make_shared<CalibratedGameFrame>(*sources[f]);selected->settings=cases[c];
                selected->settings.effect_seconds=sources[f]->settings.effect_seconds;select_reconstruction(selected->settings,quality);
                native_mock.head_x=head(f);const auto actual=render(selected);
                check(actual[0]!=actual[1],"Reconstructed styles flattened the calibrated eyes");
                const auto post=calibrated_game_post_effects(selected->settings);
                for(unsigned eye=0;eye<2;++eye) {
                    auto expected=reconstructed[f][eye];Framebuffer ownership(w/scale,h/scale,scale);ownership.enable_layer_tags(true);
                    for(unsigned pixel=0;pixel<w*h;++pixel) {
                        const unsigned at=pixel*4;
                        const auto& label=labels[f][eye];
                        check(label[at]==255 || label[at+1]==255 || label[at+2]==255,"Nonbinary independent centre label");
                        ownership.layer_tags()[pixel]=unsigned(label[at+2]==255?PixelLayer::two_d:label[at+1]==255?PixelLayer::three_d:PixelLayer::background);
                    }
                    std::vector<unsigned char> scratch;
                    for(const auto& p:post) apply_effect(static_cast<Effect>(p.model),ownership,expected,scratch,
                        p.model_intensity,static_cast<Effect>(p.world),p.world_intensity,p.seconds);
                    apply_hdr_effect(ownership,expected,selected->settings.contrast);
                    apply_chromatic_aberration(ownership,expected,scratch,selected->settings.chromatic);
                    apply_global_enhancements(selected->settings.global_enhancements,ownership,expected,scratch,selected->settings.effect_seconds);
                    BloomPass bloom;bloom.apply(selected->settings.bloom_model,selected->settings.bloom_world,ownership,expected);
                    const auto& s=selected->settings;
                    const unsigned world=s.extra_effects[0]?s.extra_effects[0]:s.world_effects[0];
                    const unsigned model=s.manipulation?s.manipulation:s.model_effects[0];
                    trails[eye][0].apply(ownership,expected,static_cast<PersistenceMode>(persistence_mode(static_cast<Effect>(world))),
                        false,true,s.effect_seconds,1,s.extra_effects[0]?100:s.world_effects[1]);
                    trails[eye][1].apply(ownership,expected,static_cast<PersistenceMode>(persistence_mode(static_cast<Effect>(model))),
                        true,false,s.effect_seconds,1,s.manipulation?s.manipulation_intensity:s.model_effects[1]);
                    exposure[eye].apply(ownership,expected,s.exposure,s.effect_seconds,1,s.exposure_paused);
                    trails[eye][2].apply(ownership,expected,s.phosphor?PersistenceMode::phosphor:PersistenceMode::off,
                        true,true,s.effect_seconds,1,100,s.phosphor);
                    for(unsigned pixel=0;pixel<w*h;++pixel) {
                        const unsigned at=pixel*4;const auto& overlay=overlay_labels[f][eye];
                        if(overlay[at]==255 && overlay[at+1]==255 && overlay[at+2]==0) {
                            std::copy_n(overlay_colours[f][eye].begin()+at,4,expected.begin()+at);
                            ownership.layer_tags()[pixel]=unsigned(PixelLayer::two_d);
                        }
                    }
                    for(unsigned at=0;at<expected.size();++at) {
                        const bool protected_pixel=ownership.layer_tags()[at/4]==unsigned(PixelLayer::two_d);
                        const bool combined=panel_smoke?c==1:(c>=17 && c<sdk_edge_begin) || (sdk && c==cases.size()-1);
                        const unsigned tolerance=at%4==3 || protected_pixel?0:srgb || combined?2:0;
                        if(std::abs(int(actual[eye][at])-int(expected[at]))>int(tolerance))
                            throw std::runtime_error(std::string(backend)+" reconstructed post mismatch q="+std::to_string(quality)
                                +" case="+std::to_string(c)+" frame="+std::to_string(f)+" eye="+std::to_string(eye)
                                +" byte="+std::to_string(at)+" actual="+std::to_string(actual[eye][at])+" expected="+std::to_string(expected[at]));
                        ++checks;++compared;protected_bytes+=protected_pixel;
                        visible+=actual[eye][at]!=reconstructed[f][eye][at];
                    }
                }
                check(render(selected)==actual,"Held reconstructed post advanced jitter or fed style pixels into history");++held_frames;
                // All seventeen multi-tap/static transforms ignore the host
                // clock, but animated shader FX must continue on a held game.
                if(f==count-1 && (panel_smoke?c==0:c<25 || (sdk && c>=sdk_edge_begin))) {
                    auto clocked=std::make_shared<CalibratedGameFrame>(*selected);clocked->settings.effect_seconds+=.37F;
                    const auto calls=dlss_fixture::evaluations.size();
                    const auto pixels=render(clocked);
                    if(c<17 || c>=sdk_edge_begin) {
                        check(pixels==actual && (!sdk || dlss_fixture::evaluations.size()==calls),
                            "Static complex effect clock advanced accepted reconstruction or changed pixels");++static_clocks;
                    } else {
                        check(pixels!=actual && (!sdk || dlss_fixture::evaluations.size()==calls+2),
                            "Animated effect was incorrectly frozen by held reconstruction");++animated_clocks;
                    }
                }
                // A rejected SDK evaluation mutates opaque CPU history and
                // forces the next genuine evaluation to reset. Test rejection
                // AFTER the compared timeline, not against an uninterrupted
                // reference's later neural history. Held acceptance is exact.
                if(c==last_temporal_case && f==(sdk?count-1:1)) {
                    gpu_mock.timeout_right=true;check(renderer.submit(selected)==DisplayXrSubmission::waiting,"Reconstructed post wait lost retained eyes");
                    check(renderer.poll()==DisplayXrSubmission::waiting,"Reconstructed post reacquired a waiting image");gpu_mock.timeout_right=false;
                    auto result=renderer.poll();for(unsigned attempt=0;result==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);result=renderer.poll();}
                    check(result==DisplayXrSubmission::submitted && rgba(consume())==actual,"Reconstructed post wait changed retained artwork");
                    auto rejected=std::make_shared<CalibratedGameFrame>(*selected);rejected->draws[1].packet.model[12]+=.31F;
                    gpu_mock.fail_end=true;auto failure=renderer.submit(rejected);
                    for(unsigned attempt=0;renderer.frame_pending() && attempt<10000;++attempt) {SDL_Delay(1);failure=renderer.poll();}
                    check(failure==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected reconstructed post frame remained accepted/pending");
                    (void)consume();check(render(selected)==actual,"Rejected reconstructed post replaced accepted raw history");++rejections;
                }
            }
            std::cout<<backend<<" reconstructed post case "<<c<<" quality "<<quality<<": full CPU style/order oracle, two eyes and held history passed\n";
        }
    }
    check(compared>100000 && protected_bytes>1000 && visible>1000 && held_frames==cases.size()*count*qualities
        && rejections==qualities && static_clocks==(panel_smoke?1:17+(sdk?cases.size()-sdk_edge_begin:0))*qualities
        && animated_clocks==(panel_smoke?0:8)*qualities,
        "Reconstructed post fixture missed effects/protected/history/lifecycle coverage");
    native_mock.head_x=native_mock.sample_x=native_mock.sample_y=0;
    std::cout<<backend<<(panel_smoke?" full-HD panel post: 2 selections at 1920x1080 in each eye, K/M DLAA, ":
        " reconstructed post: 25 effects + 3 ordered chains + 3 finishing + 3 temporal combinations");
    if(!panel_smoke) std::cout<<(sdk?" + 13 drawn effects + 1 ordered drawn chain":"")<<", ";
    std::cout<<compared<<" compared bytes, "<<protected_bytes
        <<" exact protected bytes, "<<visible<<" effect changes, "<<held_frames<<" held frames and "<<rejections<<" rejected/wait recoveries, "
        <<static_clocks<<" static and "<<animated_clocks<<" animated clocks passed\n";
}
