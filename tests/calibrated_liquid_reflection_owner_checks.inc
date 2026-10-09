// Same real GPU owner as the ordinary/metal checks above. Require an actual
// history-only RGB difference with AA OFF and SSAA; no primary TAA can account
// for it. Physical current/old optical correspondence is checked separately.
{
    std::array<unsigned,2> liquid_stabilized{},liquid_protected{};
    for(unsigned kind=0;kind<2;++kind) for(unsigned moving_plane=0;moving_plane<2;++moving_plane)
        for(unsigned sampled=0;sampled<2;++sampled) {
        const unsigned material=kind?9:5,stabilized_before=liquid_stabilized[kind];
        auto world=std::make_shared<CalibratedGameFrame>(*base);world->draws.erase(world->draws.begin());
        for(auto& draw:world->draws) if(draw.source_key==2) for(auto& v:draw.packet.geometry.vertices) {
            v.position[0]=(v.position[0]-1.4F)*2;v.position[1]=(v.position[1]-.03F)*2+.55F;v.position[2]-=2.4F;
        }
        starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,false,false,232);
        starfox::vr::place_landscape_ground(floor,-.5F,true);
        CalibratedGroundGradient gradient{{.2F,.3F,.1F},{.4F,.5F,.2F}};
        gradient.material=material;gradient.seconds=2;gradient.origin={23,-31};
        check(apply_calibrated_ground(floor,gradient),"Native liquid owner lost retained metadata");
        world->draws.insert(world->draws.begin(),{std::move(floor),CalibratedGameLayer::world,
            starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        world->settings.ray_tracing=world->settings.reflections=2;world->settings.enhanced_ground=true;
        world->settings.ground_material=material;world->settings.history_epoch=800+kind*100+moving_plane*10+sampled;
        world->settings.aa_type=sampled?3:0;world->settings.aa_quality=sampled?1:0;
        Images latest;
        for(unsigned frame=0;frame<6;++frame) {
            auto next=std::make_shared<CalibratedGameFrame>(*world);next->presentation_seconds=4+frame/120.;
            auto now=std::make_shared<starfox::vr::GameSceneSnapshot>(*tick);now->revision=frame;
            next->previous=next->current=now;
            for(auto& draw:next->draws) {
                if(draw.source_key==2) draw.packet.model[12]=frame*-.021F;
                if(draw.source_key==calibrated_ground_source_key) {
                    draw.packet.geometry.texels[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(2+frame/120.F);
                    draw.packet.geometry.texels[calibrated_ground_offset+9]=std::bit_cast<std::uint32_t>(23+frame*.75F);
                    if(moving_plane) {
                        const auto angle=frame*.011F;draw.packet.model[0]=draw.packet.model[5]=std::cos(angle);
                        draw.packet.model[1]=std::sin(angle);draw.packet.model[4]=-std::sin(angle);
                        draw.packet.model[13]=frame*-.004F;
                    }
                }
            }
            std::cout<<backend<<" native liquid owner material="<<material<<" moving="<<moving_plane
                <<" SSAA="<<sampled<<" frame="<<frame<<std::endl;
            const auto actual=render(next);
            check(render(next)==actual,"Held native liquid changed its clock/radiance or blended again");
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
                check(actual[eye][at+3]==plain[eye][at+3],"Native liquid reflection changed source alpha");
                if(plain[eye][at]==255 && plain[eye][at+2]==0 && (plain[eye][at+1]==0 || plain[eye][at+1]==255)) {
                    check(std::equal(actual[eye].begin()+at,actual[eye].begin()+at+4,plain[eye].begin()+at),
                        "Native liquid history covered protected emissive/UI pixels");++liquid_protected[kind];
                }
            }
            if(frame==2) {
                auto rejected=std::make_shared<CalibratedGameFrame>(*next);
                rejected->draws.front().packet.model[13]-=.08F;
                rejected->draws.front().packet.geometry.texels[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(4.5F);
                auto changed=std::make_shared<starfox::vr::GameSceneSnapshot>(*now);changed->revision=99;rejected->current=changed;
                gpu_mock.fail_end=true;auto result=renderer.submit(rejected);
                for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected native wave remained pending");
                (void)read();check(render(next)==actual,"Rejected liquid clock/plane/radiance replaced accepted history");
            }
            if(frame==3) {
                auto cut=std::make_shared<CalibratedGameFrame>(*next);cut->settings.history_epoch+=10000;
                const auto raw=render(cut);
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<raw[eye].size();at+=4)
                    for(unsigned c=0;c<3;++c) liquid_stabilized[kind]+=raw[eye][at+c]!=actual[eye][at+c];
            }
            latest=actual;last=next;
        }
        if(liquid_stabilized[kind]<=stabilized_before) throw std::runtime_error(
            "Native liquid owner bypassed incident-light history material="+std::to_string(material)
            +" moving="+std::to_string(moving_plane)+" SSAA="+std::to_string(sampled));
        auto wave_only=std::make_shared<CalibratedGameFrame>(*last);wave_only->presentation_seconds+=.01;
        wave_only->draws.front().packet.geometry.texels[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(2.6F);
        check(render(wave_only)!=latest,"Advancing only retained wave time froze native liquid radiance");
        auto rewind=std::make_shared<CalibratedGameFrame>(*wave_only);
        rewind->draws.front().packet.geometry.texels[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(1.9F);
        const auto rewound=render(rewind);
        auto cut=std::make_shared<CalibratedGameFrame>(*rewind);cut->settings.history_epoch+=20000;
        check(render(cut)==rewound,"Wave clock rewind failed to cut old incoming radiance");last=cut;
    }
    check(liquid_protected[0]>50 && liquid_protected[1]>50,"Native liquid owner lacked protected foreground coverage");
    std::cout<<backend<<" native water/lava owner: "<<liquid_stabilized[0]<<'/'<<liquid_stabilized[1]
        <<" actually stabilized RGB bytes, "<<liquid_protected[0]<<'/'<<liquid_protected[1]
        <<" exact protected pixels; AA OFF/2x SSAA, current wave clock/origin, static/banked moving planes, two eyes, held/rejected wave metadata and wave-only advance/rewind passed. Mocked XR, no FPS claim.\n";
}
