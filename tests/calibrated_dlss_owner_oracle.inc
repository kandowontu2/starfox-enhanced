template<class Render,class Drain>
std::shared_ptr<const CalibratedGameFrame> native_dlss_owner_oracle(Render render,Drain drain,DisplayXrGameRenderer& renderer,bool srgb) {
    auto source=std::make_shared<CalibratedGameFrame>();source->settings.srgb=srgb;source->clear={.07F,.11F,.18F,1};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();source->current=source->previous=snapshot;
    const auto add=[&](float x,float y,float z,float radius,bool model,bool after) {
        starfox::vr::DrawPacket p;constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=x+corner[0]*radius+corner[1]*radius*.17F;
            v.position[1]=y+corner[1]*radius+corner[0]*radius*.13F;v.position[2]=z;
            v.color[0]=model?(corner[0]>0?.8F:.18F):1;v.color[1]=model?(corner[1]>0?.6F:.12F):after?1:0;
            v.color[2]=model?.35F:0;v.color[3]=1;std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
            p.geometry.vertices.push_back(v);
        }
        const unsigned key=unsigned(source->draws.size()+1);
        source->draws.push_back({std::move(p),after?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
            starfox::vr::SceneBlend::opaque,!after,key,model,after});
        auto& transform=snapshot->transforms[starfox::simulation::ObjectHandle(key)];
        transform.generation=1;transform.shape=key;transform.strategy_address=key;
    };
    add(0,0,-2,.7F,true,false);add(.13F,.06F,-1.1F,.07F,true,false);
    add(-.08F,-.05F,-.8F,.025F,false,false);add(.04F,.1F,-.7F,.035F,false,true);
    const auto baseline=render(source);unsigned protected_bytes=0,changes=0,cases=0;
    std::shared_ptr<CalibratedGameFrame> final;
    if(!native_dlss_failure_only) {
    for(unsigned model:{0U,1U})for(unsigned mode=1;mode<=4;++mode)for(unsigned aa:{0U,1U,3U,4U,6U}) {
        auto next=std::make_shared<CalibratedGameFrame>(*source);next->settings.dlss_mode=mode;next->settings.dlss_model=model;
        next->settings.aa_type=aa;next->settings.aa_quality=aa?2:0;
        next->settings.ray_tracing=2;next->settings.reflections=2;
        if(aa==1) {next->settings.bloom_model=2;next->settings.contrast=1;}
        const auto before=dlss_fixture::evaluations.size();const auto frame_ends=dlss_fixture::finished_frames;const auto output=render(next);
        check(dlss_fixture::evaluations.size()==before+2,"Native owner did not evaluate exactly two real SDK eyes");
        check(dlss_fixture::finished_frames==frame_ends+1,"SDK bookkeeping did not finish exactly once for the two-eye pair");
        const auto left=dlss_fixture::evaluations[before],right=dlss_fixture::evaluations[before+1];
        check(left.viewport==100 && right.viewport==101 && left.frame_index==right.frame_index && left.reset && right.reset,
            "Native owner shared a mono viewport/token or retained incompatible quality history");
        check(!std::equal(std::begin(left.view_to_clip),std::end(left.view_to_clip),std::begin(right.view_to_clip))
            || !std::equal(std::begin(left.camera_position),std::end(left.camera_position),std::begin(right.camera_position)),
            "Native SDK eye constants collapsed calibrated cameras");
        const unsigned factor=aa==3?3:1;
        for(unsigned eye=0;eye<2;++eye) {
            const auto& evaluated=dlss_fixture::evaluations[before+eye];
            check(evaluated.output_width==mock.image_width && evaluated.output_height==mock.image_height,
                "Native DLSS changed runtime panel dimensions");
            check(renderer.scene_extent(eye)==std::array<std::uint32_t,2>{evaluated.width*factor,evaluated.height*factor},
                "Native DLSS did not rasterize at actual SDK input size");
            for(unsigned sample=0;sample<(aa==6?4U:1U);++sample) {
                const auto bank=renderer.accepted_reflection_sample(eye,sample);
                check(bank.buffer && bank.reflection_history.storage_bytes
                    && bank.width==evaluated.width*factor && bank.height==evaluated.height*factor,
                    "Native SDK secondary cache used panel dimensions, omitted a real sample or ignored the SSAA raster");
            }
            if(mode==4) check(evaluated.width==mock.image_width && evaluated.height==mock.image_height,
                "Native DLAA forced a super-resolution factor");
            else check(evaluated.width<mock.image_width && evaluated.height<mock.image_height,
                "Native DLSS merely downscaled an already full-resolution raster");
            check(output[eye].size()==baseline[eye].size(),"Native DLSS resized the compositor image");
            for(unsigned at=0;at<output[eye].size();at+=4) {
                const bool ink=baseline[eye][at]>250 && baseline[eye][at+2]<2
                    && (baseline[eye][at+1]<2 || baseline[eye][at+1]>250);
                check(output[eye][at+3]==baseline[eye][at+3],"Native SDK owner changed panel opacity");
                for(unsigned k=0;k<4;++k) if(ink) {
                    check(output[eye][at+k]==baseline[eye][at+k],"Native SDK owner blurred or displaced protected ink");++protected_bytes;
                } else changes+=output[eye][at+k]!=baseline[eye][at+k];
            }
        }
        check(output[0]!=output[1],"Native DLSS flattened calibrated stereo");
        const auto held=dlss_fixture::evaluations.size();
        check(render(next)==output && render(next)==output && dlss_fixture::evaluations.size()==held,
            "Held native SDK owner wobbled or reevaluated a frozen preview");
        check(dlss_fixture::finished_frames==frame_ends+3,"Held SDK presentations omitted or repeated frame-end cleanup");
        auto moved=std::make_shared<CalibratedGameFrame>(*next);moved->draws[1].packet.model[12]+=.025F;
        moved->presentation_seconds+=1./120;const auto moving=render(moved);
        check(dlss_fixture::evaluations.size()==held+2 && dlss_fixture::evaluations[held].frame_index>left.frame_index
            && !dlss_fixture::evaluations[held].reset && !dlss_fixture::evaluations[held+1].reset,
            "Moving native SDK eyes did not advance genuine accepted history");
        check(moving!=output && render(moved)==moving,"Native SDK moving/held source image was stale or unstable");
        auto topology=std::make_shared<CalibratedGameFrame>(*moved);
        auto spawned=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);
        ++spawned->transforms[2].generation;topology->current=topology->previous=spawned;
        for(auto& vertex:topology->draws[1].packet.geometry.vertices)vertex.position[0]+=.011F;
        topology->presentation_seconds+=1./120;
        const auto topology_calls=dlss_fixture::evaluations.size();const auto replacement=render(topology);
        check(dlss_fixture::evaluations.size()==topology_calls+2 && !dlss_fixture::evaluations[topology_calls].reset
            && !dlss_fixture::evaluations[topology_calls+1].reset,
            "New/untracked model reset both entire SDK eye histories instead of using local current-colour bias");
        check(render(topology)==replacement && dlss_fixture::evaluations.size()==topology_calls+2,
            "Held reactive geometry reevaluated or drifted");
        check(render(source)==baseline,"Native DLSS OFF failed exact full-resolution restoration");
        final=next;++cases;
    }
    check(changes && protected_bytes,"Native DLSS oracle lacks scene changes or protected ink");
    unsigned static_floors=0;
    for(unsigned material:{100U,0U,8U})for(unsigned model:{0U,1U})for(unsigned aa:{0U,3U,6U}) {
        auto terrain=std::make_shared<CalibratedGameFrame>(*source);
        terrain->settings.dlss_mode=2;terrain->settings.dlss_model=model;
        terrain->settings.aa_type=aa;terrain->settings.aa_quality=aa?2:0;
        starfox::simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
        ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.bg2_character_base=0;ppu.cgram[1]=31;
        for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
        for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);
        starfox::vr::place_landscape_ground(floor,-.75F,true);
        const auto original_floor=floor;
        CalibratedGroundGradient gradient{{.24F,.39F,.12F},{.12F,.19F,.055F},112,111};
        gradient.material=material;gradient.origin={123,-415};gradient.seconds=1.75F;
        if(material!=100) check(apply_calibrated_ground(floor,gradient),"Static SDK floor payload failed");
        terrain->draws.insert(terrain->draws.begin(),{std::move(floor),CalibratedGameLayer::world,
            starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        terrain->draws.front().reflection_environment=true;
        const auto first=render(terrain);const auto calls=dlss_fixture::evaluations.size();
        if(material==0 || material==8) {
            auto tick=std::make_shared<CalibratedGameFrame>(*terrain);tick->settings.effect_seconds=9;
            auto changed=gradient;changed.seconds=9;
            if(material==0) changed.origin={187,-287};
            tick->draws.front().packet=original_floor;
            check(apply_calibrated_ground(tick->draws.front().packet,changed),"Fresh SDK clocked terrain failed");
            check(render(tick)==first && render(terrain)==first && dlss_fixture::evaluations.size()==calls,
                "Fresh static terrain metadata reevaluated SDK eyes or advanced held sample phase");
        }
        auto moving=std::make_shared<CalibratedGameFrame>(*terrain);moving->presentation_seconds+=1./120;
        auto& words=moving->draws.front().packet.geometry.texels;
        words[calibrated_ground_offset-1]=std::bit_cast<std::uint32_t>(-.83F);
        if(material!=100) {
            words[calibrated_ground_offset+9]=std::bit_cast<std::uint32_t>(187.F);
            words[calibrated_ground_offset+10]=std::bit_cast<std::uint32_t>(-287.F);
            words[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(3.F);
        }
        moving->draws.front().packet.model[12]=.015F;
        moving->draws[2].packet.model[12]+=.013F;
        const auto moved=render(moving);
        check(dlss_fixture::evaluations.size()==calls+2 && !dlss_fixture::evaluations[calls].reset
            && !dlss_fixture::evaluations[calls+1].reset,"Static finite floor reset the two native SDK histories");
        check(moved!=first && moved[0]!=moved[1] && render(moving)==moved && dlss_fixture::evaluations.size()==calls+2,
            "Native SDK finite floor was stale, flattened or its held image jittered");
        ++static_floors;
    }
    std::cout<<"Native SDK finite-floor correspondence: "<<static_floors
        <<" original/gradient/red-sand, K/M, centre/SSAA/MSAA owner cases; fresh static clocks stay held, height/origin/model movement preserves global history and exact held eyes.\n";
    unsigned palette_cases=0;
    {
        // A visible finite floor exercises WORLD palettes as well as MODEL
        // palettes. Do not substitute an empty world pass or a flat HUD image.
        auto terrain=std::make_shared<CalibratedGameFrame>(*source);
        starfox::simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
        // Blue source sky keeps the fixture's red/yellow protected-ink marker
        // unambiguous. WORLD palettes correctly colour the sky; red sky is NOT
        // emissive/HUD ink merely because its RGB matches that marker.
        ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.bg2_character_base=0;ppu.cgram[1]=31U<<10;
        for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
        for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);
        starfox::vr::place_landscape_ground(floor,-.75F,true);
        CalibratedGroundGradient gradient{{.24F,.39F,.12F},{.12F,.19F,.055F},112,111};
        check(apply_calibrated_ground(floor,gradient),"Palette SDK finite-floor payload failed");
        terrain->draws.insert(terrain->draws.begin(),{std::move(floor),CalibratedGameLayer::world,
            starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        terrain->draws.front().reflection_environment=true;
        const auto unstyled=render(terrain);
        constexpr unsigned styles[]{4,8,9,11,14,15,16,17,18,19,20,21,22,23,24,31,32,36,38,64,65,69,71,72,73};
        for(unsigned style:styles) for(unsigned model:{0U,1U}) for(unsigned selection=0;selection<3;++selection) {
            auto selected=std::make_shared<CalibratedGameFrame>(*terrain);
            constexpr unsigned intensities[]{35,100,70},aa_types[]{0,3,6};
            selected->settings.dlss_mode=2;selected->settings.dlss_model=model;
            selected->settings.aa_type=aa_types[selection];selected->settings.aa_quality=selection?2:0;
            if(selection!=1) selected->settings.world_effects={style,intensities[selection],0,0};
            if(selection!=0) selected->settings.model_effects={style,intensities[selection],0,0};
            const auto first_call=dlss_fixture::evaluations.size();const auto styled=render(selected);
            check(dlss_fixture::evaluations.size()==first_call+2 && dlss_fixture::evaluations[first_call].reset
                && dlss_fixture::evaluations[first_call+1].reset,"Palette/quality change did not reset both SDK eyes");
            check(styled!=unstyled && styled[0]!=styled[1],"Native SDK palette was invisible or flattened stereo");
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<styled[eye].size();at+=4) {
                check(styled[eye][at+3]==unstyled[eye][at+3],"Palette SDK changed panel opacity");
                const bool ink=unstyled[eye][at]>250 && unstyled[eye][at+2]<2
                    && (unstyled[eye][at+1]<2 || unstyled[eye][at+1]>250);
                if(ink) {
                    if(!std::equal(styled[eye].begin()+at,styled[eye].begin()+at+4,unstyled[eye].begin()+at))
                        throw std::runtime_error("Palette SDK protected ink mismatch style="+std::to_string(style)
                            +" model="+std::to_string(model)+" selection="+std::to_string(selection)+" eye="+std::to_string(eye)
                            +" pixel="+std::to_string(at/4%mock.image_width)+","+std::to_string(at/4/mock.image_width)
                            +" baseline="+std::to_string(unstyled[eye][at])+","+std::to_string(unstyled[eye][at+1])+","+std::to_string(unstyled[eye][at+2])
                            +" actual="+std::to_string(styled[eye][at])+","+std::to_string(styled[eye][at+1])+","+std::to_string(styled[eye][at+2]));
                    ++checks;protected_bytes+=4;
                }
            }
            auto tick=std::make_shared<CalibratedGameFrame>(*selected);tick->settings.effect_seconds=2;
            const auto held=dlss_fixture::evaluations.size();
            check(render(tick)==styled && render(selected)==styled && dlss_fixture::evaluations.size()==held,
                "Unused static palette clock advanced SDK evaluations/phase or reset a held preview");
            auto moved=std::make_shared<CalibratedGameFrame>(*selected);moved->draws[2].packet.model[12]+=.035F;
            moved->presentation_seconds+=1./120;moved->settings.effect_seconds=1.F/60;
            const auto moving=render(moved);
            check(dlss_fixture::evaluations.size()==held+2 && !dlss_fixture::evaluations[held].reset
                && !dlss_fixture::evaluations[held+1].reset,"Static pointwise palette reset moving SDK history every frame");
            check(moving!=styled && render(moved)==moving && dlss_fixture::evaluations.size()==held+2,
                "Moving palette SDK image was stale or held history wobbled");
            if(selection==2) {
                auto changed=std::make_shared<CalibratedGameFrame>(*moved);++changed->settings.model_effects[1];
                const auto change_call=dlss_fixture::evaluations.size();(void)render(changed);
                check(dlss_fixture::evaluations.size()==change_call+2 && dlss_fixture::evaluations[change_call].reset
                    && dlss_fixture::evaluations[change_call+1].reset,"Palette intensity change reused incompatible SDK colour history");
            }
            ++palette_cases;
        }
        unsigned local_post_cases=0;
        for(unsigned style:{unsigned(Effect::ink),unsigned(Effect::ripple_warp),unsigned(Effect::hologram),unsigned(Effect::kaleidoscope),
                unsigned(Effect::dithered),unsigned(Effect::night_vision),unsigned(Effect::scanlines),unsigned(Effect::crt_phosphor)})
            for(unsigned model:{0U,1U})for(unsigned selection=0;selection<3;++selection) {
            auto warped=std::make_shared<CalibratedGameFrame>(*terrain);warped->settings.dlss_mode=2;warped->settings.dlss_model=model;
            constexpr unsigned aa_types[]{0,3,6};warped->settings.aa_type=aa_types[selection];warped->settings.aa_quality=selection?2:0;
            warped->settings.world_effects={selection==1?unsigned(Effect::monochrome):style,70,0,0};
            warped->settings.model_effects={selection==0?unsigned(Effect::monochrome):style,70,0,0};
            const auto first=render(warped);const auto calls=dlss_fixture::evaluations.size();
            check(first!=unstyled && first[0]!=first[1],"SDK ownership-local post fixture lacks visible style/stereo coverage");
            check(render(warped)==first && dlss_fixture::evaluations.size()==calls,"Held mixed distortion did not retain exact SDK eyes");
            if(calibrated_pattern_effect(style)) {
                auto clocked=std::make_shared<CalibratedGameFrame>(*warped);clocked->settings.effect_seconds=9;
                check(render(clocked)==first && render(warped)==first && dlss_fixture::evaluations.size()==calls,
                    "Static point-pattern clock reevaluated/jittered an accepted SDK preview");
            }
            auto moved=std::make_shared<CalibratedGameFrame>(*warped);moved->draws[2].packet.model[12]+=.035F;
            moved->settings.effect_seconds=1.F/60;const auto moving=render(moved);
            check(dlss_fixture::evaluations.size()==calls+2 && !dlss_fixture::evaluations[calls].reset
                && !dlss_fixture::evaluations[calls+1].reset,"Ownership-local post discarded both SDK eye histories");
            check(moving!=first && render(moved)==moving && dlss_fixture::evaluations.size()==calls+2,
                "Moving post source was stale or held ownership-local SDK image drifted");
            if(calibrated_pattern_effect(style)) {
                auto intensity=std::make_shared<CalibratedGameFrame>(*moved);++intensity->settings.model_effects[1];
                const auto change=dlss_fixture::evaluations.size();(void)render(intensity);
                check(dlss_fixture::evaluations.size()==change+2 && dlss_fixture::evaluations[change].reset
                    && dlss_fixture::evaluations[change+1].reset,"Pattern intensity change reused incompatible SDK history");
            }
            ++local_post_cases;
        }
        auto global=std::make_shared<CalibratedGameFrame>(*terrain);global->settings.dlss_mode=2;global->settings.chromatic=1;
        (void)render(global);auto moved_global=std::make_shared<CalibratedGameFrame>(*global);moved_global->draws[2].packet.model[12]+=.035F;
        const auto global_calls=dlss_fixture::evaluations.size();(void)render(moved_global);
        check(dlss_fixture::evaluations.size()==global_calls+2 && !dlss_fixture::evaluations[global_calls].reset
            && !dlss_fixture::evaluations[global_calls+1].reset,"Post-reconstruction chromatic effect discarded raw SDK history");
        auto reflected=std::make_shared<CalibratedGameFrame>(*terrain);reflected->settings.dlss_mode=2;
        reflected->settings.ray_tracing=reflected->settings.reflections=2;
        reflected->settings.material=unsigned(Effect::mirror);reflected->settings.world_effects={unsigned(Effect::ripple_warp),70,0,0};
        (void)render(reflected);auto moved_reflection=std::make_shared<CalibratedGameFrame>(*reflected);
        moved_reflection->draws[2].packet.model[12]+=.035F;const auto reflection_calls=dlss_fixture::evaluations.size();(void)render(moved_reflection);
        check(dlss_fixture::evaluations.size()==reflection_calls+2 && !dlss_fixture::evaluations[reflection_calls].reset
            && !dlss_fixture::evaluations[reflection_calls+1].reset,"Locally witnessed secondary RGB discarded both entire SDK eye histories");
        std::cout<<"Native SDK ownership-local posts: "<<local_post_cases
            <<" neighbour/warp/animated/mixed plus four phase-guided point patterns, world/model/both, K/M and centre/SSAA/MSAA; moving layers preserve global history, exact held eyes/static pattern clocks/intensity reset, chromatic runs after reconstruction and secondary reflection is guarded locally.\n";
        check(render(terrain)==unstyled,"Palette SDK OFF did not restore the exact original floor/model/ink pair");
    }
    std::cout<<"Native SDK static palettes: "<<palette_cases
        <<" cases across all 25 pointwise styles, K/M, visible world/model/both, 35/100/70 intensity and centre/SSAA/MSAA; "
        <<"moving history survives, unused clock stays held, intensity resets, local mixed distortion rejection and exact protected/OFF passed.\n";
    unsigned liquids=0,panel_liquids=0;
    for(unsigned material:{5U,9U})for(unsigned model:{0U,1U})for(unsigned aa:{0U,3U,6U}) {
        auto liquid=std::make_shared<CalibratedGameFrame>(*source);
        auto tick=std::make_shared<starfox::vr::GameSceneSnapshot>(*snapshot);tick->dots_mode=-1;
        liquid->previous=liquid->current=tick;liquid->settings.dlss_mode=2;liquid->settings.dlss_model=model;
        liquid->settings.enhanced_ground=true;liquid->settings.ground_material=material;
        liquid->settings.ray_tracing=material==5?2:0;liquid->settings.aa_type=aa;liquid->settings.aa_quality=aa?2:0;
        starfox::simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
        ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=31;
        for(unsigned y=0;y<8;++y)ppu.vram[32+y*2]=255;
        for(unsigned tile=0;tile<1024;++tile)ppu.vram[0x1000+tile*2]=1;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);
        starfox::vr::place_landscape_ground(floor,-.75F,true);
        const auto unshaded_floor=floor;
        CalibratedGroundGradient gradient{{.24F,.19F,.12F},{.12F,.09F,.055F},112,111};
        gradient.material=material;gradient.seconds=1.75F;gradient.origin={123,-415};
        check(apply_calibrated_ground(floor,gradient),"Native DLSS liquid floor payload failed");
        liquid->draws.push_back({std::move(floor),CalibratedGameLayer::world,starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        liquid->draws.back().reflection_environment=true;
        const auto first=render(liquid);const auto calls=dlss_fixture::evaluations.size();
        auto moving_liquid=std::make_shared<CalibratedGameFrame>(*liquid);
        moving_liquid->presentation_seconds+=1./120;moving_liquid->settings.effect_seconds+=1.F/120;
        gradient.seconds+=1.F/120;moving_liquid->draws.back().packet=unshaded_floor;
        check(apply_calibrated_ground(moving_liquid->draws.back().packet,gradient),"Animated liquid metadata failed");
        moving_liquid->draws[1].packet.model[12]+=.013F;
        const auto wet=render(moving_liquid);
        check(dlss_fixture::evaluations.size()==calls+2 && !dlss_fixture::evaluations[calls].reset
            && !dlss_fixture::evaluations[calls+1].reset,"Animated liquid reset the entire native SDK image");
        check(wet!=first && render(moving_liquid)==wet && dlss_fixture::evaluations.size()==calls+2,
            "Native SDK liquid source was stale or its held image jittered");
        check(wet[0]!=wet[1],"Native SDK liquid flattened calibrated stereo");++liquids;
        if(material==5) {
            // Actual owner route, not just the guide component: reconstruct
            // first, then use the full-panel liquid classification for world
            // styles and the physical surface for AO/DOF. K/M and all three
            // readable raster layouts must preserve held and moving timelines.
            auto post=std::make_shared<CalibratedGameFrame>(*moving_liquid);
            post->settings.contrast=1;post->settings.depth_enhancements=5;
            post->settings.bloom_world=1;post->settings.model_effects={1,70,0,0};post->settings.world_effects={8,70,0,0};
            const auto styled=render(post);const auto post_calls=dlss_fixture::evaluations.size();
            check(styled!=wet && styled[0]!=styled[1] && render(post)==styled
                && dlss_fixture::evaluations.size()==post_calls,"Panel liquid finishing was missing, mono, jittery or reevaluated while held");
            for(unsigned eye=0;eye<2;++eye)for(unsigned at=3;at<styled[eye].size();at+=4)
                check(styled[eye][at]==wet[eye][at],"Panel liquid finishing changed original opacity");
            auto moved=std::make_shared<CalibratedGameFrame>(*post);moved->draws[1].packet.model[12]+=.019F;
            moved->presentation_seconds+=1./120;
            const auto finished=render(moved);
            check(finished!=styled && render(moved)==finished && dlss_fixture::evaluations.size()==post_calls+2
                && !dlss_fixture::evaluations[post_calls].reset && !dlss_fixture::evaluations[post_calls+1].reset,
                "Moving panel liquid discarded global raw history or retained stale finishing");
            ++panel_liquids;
        }
    }
    std::cout<<"Native SDK local reactivity: "<<liquids<<" actual water/lava, K/M, centre/SSAA/MSAA owner cases; "
        <<"moving liquid and topology changes preserve global history, held source stays exact.\n";
    check(panel_liquids==6,"Missing K/M/centre/SSAA/MSAA actual reconstructed liquid finishing");
    std::cout<<"Native SDK panel liquids: "<<panel_liquids<<" actual K/M centre/SSAA/MSAA owners, drawn/world styles + contrast + bloom + AO/DOF; stereo/opacity, exact held and moving accepted history passed.\n";
    }
    auto bad=std::make_shared<CalibratedGameFrame>(*source);bad->settings.dlss_mode=5;
    check(renderer.submit(bad)==DisplayXrSubmission::failed && !renderer.frame_pending(),"Invalid native DLSS mode retained a frame");
    bad->settings.dlss_mode=1;bad->settings.fsr1_mode=1;
    check(renderer.submit(bad)==DisplayXrSubmission::failed && !renderer.frame_pending(),"Two native upscalers were silently stacked");
    // A later-eye SDK failure happens after the left SDK has already mutated
    // CPU state. Cancellation must retain the previously accepted pair, and
    // the next genuine evaluation must reset BOTH independent histories.
    final=std::make_shared<CalibratedGameFrame>(*source);final->settings.dlss_mode=1;
    final->settings.ray_tracing=final->settings.reflections=2;final->settings.material=unsigned(Effect::mirror);
    const auto accepted=render(final);const auto accepted_count=dlss_fixture::evaluations.size();
    const std::array accepted_secondary{renderer.accepted_reflection_sample(0),renderer.accepted_reflection_sample(1)};
    check(accepted_secondary[0].buffer && accepted_secondary[1].buffer,"SDK failure fixture omitted accepted secondary banks");
    const auto validate=[&](const char* phase) {
        // Retrieval-only checkpoint: never clear/filter away SDK errors.
        auto* gpu=static_cast<SDL_GPUDevice*>(accepted_secondary[0].device);
        check(SDL_WaitForGPUIdle(gpu),SDL_GetError());
        auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(gpu),STARFOX_SDL_D3D12_DEVICE,nullptr));
        check(native,"Missing SDK validation device");
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> queue;
        check(SUCCEEDED(native->QueryInterface(IID_ID3D12InfoQueue,reinterpret_cast<void**>(queue.GetAddressOf()))),"SDK validation queue unavailable");
        D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
        D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
        check(SUCCEEDED(queue->PushRetrievalFilter(&filter)),"SDK validation filter failed");
        const auto count=queue->GetNumStoredMessagesAllowedByRetrievalFilter();
        for(UINT64 i=0;i<std::min<UINT64>(count,16);++i) {
            SIZE_T bytes=0;check(SUCCEEDED(queue->GetMessage(i,nullptr,&bytes)),"SDK validation message size unavailable");
            std::vector<unsigned char> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            check(SUCCEEDED(queue->GetMessage(i,message,&bytes)),"SDK validation message unavailable");
            std::cerr<<"SDK "<<phase<<" validation ID "<<message->ID<<": "<<message->pDescription<<'\n';
        }
        queue->PopRetrievalFilter();
        check(count==0,(std::string("SDK resource-state failure at ")+phase).c_str());
    };
    validate("accepted-before-failure");
    const auto accepted_ends=dlss_fixture::finished_frames;
    const auto accepted_releases=dlss_fixture::release_count;
    auto failed=std::make_shared<CalibratedGameFrame>(*final);failed->draws[1].packet.model[12]+=.04F;
    failed->presentation_seconds+=1./120;dlss_fixture::fail_call=unsigned(accepted_count+2);
    check(renderer.submit(failed)==DisplayXrSubmission::failed,"Right-eye SDK failure was not propagated");
    dlss_fixture::fail_call=0;
    for(unsigned attempt=0;renderer.frame_pending() && attempt<10000;++attempt) {
        SDL_Delay(1);check(renderer.poll()!=DisplayXrSubmission::submitted,"Cancelled SDK frame became a visible accepted pair");
    }
    check(!renderer.frame_pending(),"Cancelled SDK owner did not retire its pending source commands");
    validate("failed-pair-drained");
    check(dlss_fixture::finished_frames==accepted_ends+1,"Cancelled SDK pair omitted or repeated frame-end cleanup");
    const auto cancelled_count=dlss_fixture::evaluations.size();
    check(cancelled_count==accepted_count+2 && render(final)==accepted
        && dlss_fixture::evaluations.size()==cancelled_count,"SDK cancellation overwrote or reevaluated accepted eyes");
    for(unsigned eye=0;eye<2;++eye) check(renderer.accepted_reflection_sample(eye).buffer==accepted_secondary[eye].buffer,
        "Later-eye SDK failure committed or released the preceding accepted secondary bank");
    check(dlss_fixture::release_count==accepted_releases,"Held accepted SDK pair needlessly recreated private history");
    const auto recovered=render(failed);
    validate("recovered-pair");
    check(dlss_fixture::release_count==accepted_releases,"SDK recovery needlessly recreated private eye histories instead of draining recorded work");
    check(dlss_fixture::evaluations.size()==cancelled_count+2 && dlss_fixture::evaluations[cancelled_count].reset
        && dlss_fixture::evaluations[cancelled_count+1].reset,"SDK cancellation did not reset both next evaluations");
    check(recovered!=accepted && render(failed)==recovered,"Recovered SDK source remained stale or jittery");
    // A failed frame-end must retain the immutable pair and retry only the
    // cleanup, not reevaluate, commit early or begin another source frame.
    auto delayed=std::make_shared<CalibratedGameFrame>(*failed);delayed->draws[1].packet.model[12]+=.02F;
    delayed->presentation_seconds+=1./120;
    const auto ends_before_retry=dlss_fixture::finished_frames;
    const auto evals_before_retry=dlss_fixture::evaluations.size();
    dlss_fixture::fail_finish_call=dlss_fixture::finish_attempts+1;
    auto outcome=renderer.submit(delayed);
    for(unsigned attempt=0;dlss_fixture::finish_attempts<dlss_fixture::fail_finish_call && attempt<10000;++attempt) {
        SDL_Delay(1);outcome=renderer.poll();
    }
    check(outcome==DisplayXrSubmission::waiting && renderer.retained_frame()==delayed && renderer.frame_pending()
        && dlss_fixture::finished_frames==ends_before_retry,"SDK frame-end failure lost or prematurely settled its pair");
    check(renderer.submit(final)==DisplayXrSubmission::failed,"Unfinished SDK cleanup accepted a second source frame");
    dlss_fixture::fail_finish_call=0;
    for(unsigned attempt=0;renderer.frame_pending() && attempt<10000;++attempt) {SDL_Delay(1);outcome=renderer.poll();}
    check(outcome==DisplayXrSubmission::submitted && !renderer.frame_pending() && !renderer.retained_frame()
        && dlss_fixture::finished_frames==ends_before_retry+1 && dlss_fixture::evaluations.size()==evals_before_retry+2,
        "SDK frame-end retry repeated evaluation or failed exact-once cleanup");
    const auto delayed_pixels=drain();
    validate("frame-end-retry");
    check(render(delayed)==delayed_pixels && dlss_fixture::evaluations.size()==evals_before_retry+2,
        "SDK frame-end retry failed to retain the exact accepted output");
    final=delayed;
    if(native_dlss_failure_only) std::cout<<"D3D12 native SDK failure sequence: accepted secondary banks, right-eye failure, exact held recovery and frame-end retry passed.\n";
    else std::cout<<"D3D12 native DLSS owner: "<<cases<<" real K/M quality/AA/ray/effect cases, "<<protected_bytes
        <<" exact full-panel protected bytes; actual input raster, stereo, opacity, held/moving source, OFF and later-eye failure/reset passed"<<std::endl;
    return final;
}
