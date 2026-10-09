// Actual native model-path producer + GPU resolver + complete accepted eye
// presentation. The separate binary64 fixtures prove each hit/guide/colour.
// An AA-OFF history-only cut must visibly change incoming reflection colour;
// primary TAA, old material responses and lifecycle-only checks cannot count.
{
    for(unsigned ground_material:{0U,6U,7U}) {
    using Effect=starfox::render::Effect;
    constexpr Effect selected_materials[]{Effect::mirror,Effect::metallic,Effect::gold_metal,
        Effect::copper_metal,Effect::silver,Effect::titanium};
    std::array<unsigned,std::size(selected_materials)> path_stabilized{};
    std::array<unsigned,std::size(selected_materials)> floor_stabilized{},model_stabilized{};
    auto path_base=std::make_shared<CalibratedGameFrame>(*base);
    // Parallel plates with a forward-view gap. Reflected rays can hit the
    // off-forward second plate and continue between real finite model faces.
    // These are not synthetic cached records or a replacement background.
    for(auto& v:path_base->draws[0].packet.geometry.vertices) {
        v.position[0]*=7;v.position[1]*=7;
    }
    for(auto& v:path_base->draws[1].packet.geometry.vertices) {
        v.position[0]=6.15F+(v.position[0]-1.4F)/.75F*5.85F;
        v.position[1]=(v.position[1]-.03F)/.75F*5.85F;
    }
    starfox::vr::DrawPacket scenery;
    constexpr int env_corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
    for(auto corner:env_corners) {
        starfox::vr::SceneVertex v{};v.position[0]=corner[0]*16.F;v.position[1]=corner[1]*16.F;v.position[2]=3;
        v.color[0]=v.color[1]=v.color[2]=v.color[3]=1;std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        v.uv[0]=float(corner[0]+1)*64;v.uv[1]=float(corner[1]+1)*64;
        v.texture[1]=v.texture[2]=127;v.texture[3]=1;scenery.geometry.vertices.push_back(v);
    }
    for(unsigned y=0;y<128;++y) for(unsigned x=0;x<128;++x)
        scenery.geometry.texels.push_back((32+x*192/127)|((32+y*192/127)<<8)
            |((32+((x*3+y*5)%128)*192/127)<<16)|0xff000000U);
    path_base->draws.push_back({std::move(scenery),CalibratedGameLayer::world,
        starfox::vr::SceneBlend::opaque,false});
    path_base->draws.back().reflection_environment=true;
    if(ground_material) {
        // Retain finite model paths in the centre and genuine visible floor
        // receivers at the sides, instead of hiding the entire floor behind
        // the original screen-filling test plate.
        for(auto& v:path_base->draws[0].packet.geometry.vertices) v.position[0]*=.07F;
        starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,false,false,232);
        starfox::vr::place_landscape_ground(floor,-.5F,true);
        CalibratedGroundGradient gradient{{.2F,.3F,.1F},{.4F,.5F,.2F}};gradient.material=ground_material;
        check(apply_calibrated_ground(floor,gradient),"Mixed owner lost its actual analytic ground payload");
        path_base->draws.insert(path_base->draws.begin(),{std::move(floor),CalibratedGameLayer::world,
            starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        // Ground rays escape forward/up, whereas the vertical model mirrors
        // see the original rear scenery. Supply authored textured scenery in
        // BOTH directions; a constant clear has no incoming-colour change
        // to stabilize and cannot prove that ground history is enabled.
        auto forward_scenery=path_base->draws.back();
        for(auto& v:forward_scenery.packet.geometry.vertices) v.position[2]=-12;
        path_base->draws.insert(path_base->draws.begin(),std::move(forward_scenery));
        path_base->settings.enhanced_ground=true;
    }
    native_mock.head_x=0;native_mock.head_yaw=0;
    const auto path_plain=render(path_base);
    unsigned path_protected=0;
    for(unsigned material=0;material<std::size(selected_materials);++material) for(unsigned aa:{0U,4U}) {
        for(unsigned frame=0;frame<6;++frame) {
            auto selected=std::make_shared<CalibratedGameFrame>(*path_base);
            selected->settings.material=unsigned(selected_materials[material]);
            selected->settings.ray_tracing=selected->settings.reflections=2;
            selected->settings.aa_type=aa;selected->settings.aa_quality=aa?3:0;
            selected->settings.history_epoch=1600+material*10+aa+ground_material*100;
            selected->presentation_seconds=8+frame/120.;
            auto now=std::make_shared<starfox::vr::GameSceneSnapshot>(*tick);now->revision=frame;
            if(frame>=4) now->transforms[2].generation=2;
            selected->previous=selected->current=now;
            for(auto& draw:selected->draws) {
                if(draw.source_key==1) draw.packet.model[12]=frame*.013F;
                if(draw.source_key==2) draw.packet.model[12]=frame*-.019F;
                if(ground_material && draw.source_key==calibrated_ground_source_key) {
                    const float bank=frame*.006F;draw.packet.model[0]=draw.packet.model[5]=std::cos(bank);
                    draw.packet.model[1]=std::sin(bank);draw.packet.model[4]=-std::sin(bank);
                    draw.packet.model[13]=frame*-.002F;
                }
            }
            if(frame>=3) {
                const auto first=std::find_if(selected->draws.begin(),selected->draws.end(),[](const auto& d){return d.source_key==1;});
                const auto second=std::find_if(selected->draws.begin(),selected->draws.end(),[](const auto& d){return d.source_key==2;});
                check(first!=selected->draws.end() && second!=selected->draws.end(),"Ordered fixture lost a finite actor");
                std::iter_swap(first,second);
            }
            native_mock.head_x=frame*.27F;native_mock.head_yaw=frame*.004F;
            std::cout<<backend<<" ordered native owner material="<<unsigned(selected_materials[material])
                <<" floor="<<ground_material<<" AA="<<aa<<" frame="<<frame<<std::endl;
            const auto actual=render(selected);
            check(actual!=path_plain,"Ordered native model material did not change real reflected radiance");
            check(render(selected)==actual,"Held native ordered reflection resampled or drifted");
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
                check(actual[eye][at+3]==path_plain[eye][at+3],"Ordered native reflection changed source opacity");
                // Use the CURRENT ray-off view under these located eye poses;
                // a stationary old image cannot identify rotated protected ink.
            }
            if(frame==2) {
                auto rejected=std::make_shared<CalibratedGameFrame>(*selected);
                rejected->draws[0].packet.model[12]+=.2F;gpu_mock.fail_end=true;
                auto result=renderer.submit(rejected);
                for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected ordered model presentation remained pending");
                (void)read();check(render(selected)==actual,"Rejected ordered paths replaced accepted colour/camera/IDs");
            }
            if(frame==3 && !aa) {
                auto cut=std::make_shared<CalibratedGameFrame>(*selected);cut->settings.history_epoch+=10000;
                const auto raw=render(cut);
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<raw[eye].size();at+=4)
                    for(unsigned c=0;c<3;++c) path_stabilized[material]+=raw[eye][at+c]!=actual[eye][at+c];
                check(path_stabilized[material]>0,("Ordered native material bypassed AA-OFF incoming-light history: "
                    +std::to_string(unsigned(selected_materials[material]))).c_str());
                if(ground_material) {
                    // A CURRENT ray-off image with and without the opaque
                    // actors classifies the source owner independently of
                    // the ray/cache shader. Changes at unchanged source
                    // pixels are floor history; changes at actor coverage
                    // are model history. Primary AA is OFF in every probe.
                    auto source=std::make_shared<CalibratedGameFrame>(*selected);
                    source->settings.ray_tracing=source->settings.reflections=0;
                    const auto covered=render(source);
                    auto uncovered=std::make_shared<CalibratedGameFrame>(*source);
                    std::erase_if(uncovered->draws,[](const auto& d){return d.source_key==1 || d.source_key==2;});
                    const auto empty=render(uncovered);
                    for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<raw[eye].size();at+=4) {
                        const bool actor=!std::equal(covered[eye].begin()+at,covered[eye].begin()+at+4,empty[eye].begin()+at);
                        for(unsigned c=0;c<3;++c) if(raw[eye][at+c]!=actual[eye][at+c])
                            (actor?model_stabilized[material]:floor_stabilized[material])++;
                    }
                    check(model_stabilized[material]>0 && floor_stabilized[material]>0,
                        ("Mixed native owner lacks separate AA-OFF model AND ground colour reuse: material="
                            +std::to_string(unsigned(selected_materials[material]))+" ground="+std::to_string(ground_material)
                            +" model/floor="+std::to_string(model_stabilized[material])+"/"+std::to_string(floor_stabilized[material])).c_str());
                }
            }
            if(frame==5) {
                auto ray_off=std::make_shared<CalibratedGameFrame>(*selected);
                ray_off->settings.ray_tracing=ray_off->settings.reflections=0;
                ray_off->settings.aa_type=ray_off->settings.aa_quality=0;
                const auto current_plain=render(ray_off);
                // A history cut/AA-OFF isolates secondary RGB from primary AA.
                auto current_raw=std::make_shared<CalibratedGameFrame>(*selected);
                current_raw->settings.history_epoch+=10000;current_raw->settings.aa_type=current_raw->settings.aa_quality=0;
                const auto current_reflected=render(current_raw);
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<current_plain[eye].size();at+=4) {
                    const bool protected_ink=current_plain[eye][at]==255 && current_plain[eye][at+2]==0
                        && (current_plain[eye][at+1]==0 || current_plain[eye][at+1]==255);
                    if(protected_ink) {
                        check(std::equal(current_plain[eye].begin()+at,current_plain[eye].begin()+at+4,
                            current_reflected[eye].begin()+at),"Ordered reflection covered current protected emissive/UI pixels");
                        ++path_protected;
                    }
                }
            }
            last=selected;
        }
    }
    native_mock.head_x=0;native_mock.head_yaw=0;
    check(path_protected>50,"Ordered native model owner missed current-view protected ink");
    std::cout<<backend<<" ordered model owner: mirror/metallic/gold/copper/silver/titanium AA-OFF changed RGB bytes";
    for(unsigned changed:path_stabilized) std::cout<<' '<<changed;
    std::cout<<"; actual native finite-plate/cubemap transport, AA OFF/TAA, located-eye rotation/translation, held/rejected/reordered/recycled"
        " sources and current protected ink passed. Component tests separately prove hop/feature/colour optics; mocked XR, not physical headset/panel.\n";
    if(ground_material) {
        std::cout<<backend<<" mixed ground="<<ground_material<<" separate model/floor AA-OFF changed RGB bytes";
        for(unsigned material=0;material<std::size(selected_materials);++material)
            std::cout<<' '<<model_stabilized[material]<<'/'<<floor_stabilized[material];
        std::cout<<"; actual banked analytic plane, finite model reflectors and resident textured environment.\n";
    }
    }
}
