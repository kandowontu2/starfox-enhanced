// Whole-owner integration/lifetime checks. The component fixtures separately
// prove physical hit correspondence and exact reflected-colour acceptance.
// These checks do not substitute a lifecycle assertion for a colour oracle.
template<class Render,class Consume>
std::shared_ptr<const CalibratedGameFrame> native_reflection_owner_checks(Render submit,Consume read,
    DisplayXrGameRenderer& renderer,bool srgb,bool native_rays,const char* backend) {
    check(native_rays,"Reflected-radiance owner fixture requires actual native rays");
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto rgba=[&](Images images) {
        if(bgra) for(auto& eye:images) for(unsigned at=0;at<eye.size();at+=4) std::swap(eye[at],eye[at+2]);
        return images;
    };
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> frame){return rgba(submit(std::move(frame)));};
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.04F,.07F,.12F,1};
    auto tick=std::make_shared<starfox::vr::GameSceneSnapshot>();base->current=base->previous=tick;
    tick->dots_mode=-1;
    for(unsigned key:{1U,2U}) {tick->transforms[key].generation=1;tick->transforms[key].shape=10+key;}
    const auto add=[&](unsigned key,float x,float y,float z,float radius,bool caster,bool after,std::array<float,4> colour) {
        starfox::vr::DrawPacket packet;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=x+corner[0]*radius;v.position[1]=y+corner[1]*radius;v.position[2]=z;
            std::copy(colour.begin(),colour.end(),v.color);
            if(key==2) {
                v.uv[0]=float(corner[0]+1)*32;v.uv[1]=float(corner[1]+1)*32;
                v.texture[1]=v.texture[2]=63;v.texture[3]=1;
            }
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);packet.geometry.vertices.push_back(v);
        }
        if(key==2) for(unsigned ty=0;ty<64;++ty) for(unsigned tx=0;tx<64;++tx)
            packet.geometry.texels.push_back((32+192*tx/63)|((32+192*ty/63)<<8)|((32+128*((tx+ty)&15)/15)<<16)|0xff000000U);
        base->draws.push_back({std::move(packet),after?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
            starfox::vr::SceneBlend::opaque,!after,key,caster,after});
    };
    add(1,0,0,-2,1.7F,true,false,{.3F,.3F,.3F,1});
    // At the fixture's small eye grid, a tiny reflected actor has barycentric
    // tap spacing greater than the conservative .05 witness bound, rejecting
    // every old bilinear footprint. This larger off-forward actor provides
    // interior reflected hits without relaxing that production bound.
    add(2,1.4F,.03F,-.6F,.75F,true,false,{.8F,.1F,.1F,1});
    add(0,-.08F,-.05F,-.8F,.025F,false,false,{1,0,0,1});
    add(0,.04F,.1F,-.7F,.035F,false,true,{1,1,0,1});
    native_mock.head_x=0;native_mock.sample_x=native_mock.sample_y=0;
    const auto plain=render(base);
    std::shared_ptr<CalibratedGameFrame> last;unsigned protected_pixels=0,moving_bytes=0,stabilized_bytes=0;
    Images previous;
    std::array<unsigned,3> rough_stabilized{};
    if(native_msaa_reflection_only) {
#include "calibrated_msaa_reflection_owner_checks.inc"
        (void)render(last);return last;
    }
    // Primary AA OFF, each spatial AA, two actual SSAA source grids, and TAA.
    // All use real rays and the independent accepted secondary timeline, not
    // a primary-image proxy. MSAA has a separate native-sample oracle below;
    // SDK reconstruction has its own real-SDK source-jitter/sample oracle.
    struct Mode {unsigned aa,quality,material;};
    constexpr Mode modes[]{{0,0,0},{0,3,0},{1,3,0},{2,3,0},{5,3,0},{3,1,0},{3,3,0},{4,3,0},
        {0,0,unsigned(starfox::render::Effect::amethyst)},{4,3,unsigned(starfox::render::Effect::amethyst)},
        {0,0,unsigned(starfox::render::Effect::marble)},{4,3,unsigned(starfox::render::Effect::marble)},
        {0,0,unsigned(starfox::render::Effect::graphite)},{4,3,unsigned(starfox::render::Effect::graphite)}};
    for(unsigned pass=0;pass<std::size(modes);++pass) for(unsigned frame=0;frame<6;++frame) {
        auto selected=std::make_shared<CalibratedGameFrame>(*base);
        selected->settings.aa_type=modes[pass].aa;selected->settings.aa_quality=modes[pass].quality;
        selected->settings.material=modes[pass].material;
        selected->settings.ray_tracing=selected->settings.reflections=2;
        auto now=std::make_shared<starfox::vr::GameSceneSnapshot>(*tick);now->revision=frame;
        if(frame>=4) now->transforms[2].generation=2;
        selected->previous=selected->current=now;selected->presentation_seconds=frame/120.;
        for(auto& draw:selected->draws) {
            if(draw.source_key==1) draw.packet.model[12]=frame*.013F;
            if(draw.source_key==2) draw.packet.model[12]=frame*-.037F;
        }
        if(frame>=3) std::swap(selected->draws[0],selected->draws[1]);
        if(frame==5) selected->settings.history_epoch=1;
        const auto actual=render(selected);
        check(actual!=plain,"Native reflection owner did not visibly change the image");
        check(render(selected)==actual && render(selected)==actual,"Held reflected radiance drifted or resampled");
        for(unsigned eye=0;eye<2;++eye) {
            check(actual[eye].size()==plain[eye].size(),"Native reflection owner changed its physical eye extent");
            for(unsigned at=0;at<actual[eye].size();at+=4) {
                check(actual[eye][at+3]==plain[eye][at+3],"Reflected owner changed source alpha");
                if(plain[eye][at]==255 && plain[eye][at+2]==0 && (plain[eye][at+1]==0 || plain[eye][at+1]==255)) {
                    check(std::equal(actual[eye].begin()+at,actual[eye].begin()+at+4,plain[eye].begin()+at),
                        "Reflected history covered protected emissive/UI pixels");++protected_pixels;
                }
                if(frame) for(unsigned c=0;c<3;++c) moving_bytes+=actual[eye][at+c]!=previous[eye][at+c];
            }
        }
        if(frame==1) {
            gpu_mock.timeout_right=true;
            check(renderer.submit(selected)==DisplayXrSubmission::waiting,"Reflected owner failed to retain image wait");
            const auto locates=native_mock.locates;
            check(renderer.poll()==DisplayXrSubmission::waiting && native_mock.locates==locates,
                "Reflected image wait reacquired eye poses");
            gpu_mock.timeout_right=false;auto status=renderer.poll();
            for(unsigned n=0;status==DisplayXrSubmission::waiting && n<10000;++n) {SDL_Delay(1);status=renderer.poll();}
            check(status==DisplayXrSubmission::submitted && rgba(read())==actual,"Reflected image wait changed accepted colour");
        }
        if(frame==2) {
            auto rejected=std::make_shared<CalibratedGameFrame>(*selected);
            rejected->draws[1].packet.model[12]+=.31F;
            auto bad_tick=std::make_shared<starfox::vr::GameSceneSnapshot>(*now);bad_tick->revision=99;rejected->current=bad_tick;
            gpu_mock.fail_end=true;auto result=renderer.submit(rejected);
            for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
            check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected reflected frame remained accepted/pending");
            (void)read();check(render(selected)==actual,"Rejected reflected frame replaced the accepted eye/history");
        }
        if((!pass || (modes[pass].material && !modes[pass].aa)) && frame==3) {
            // Same physical image/rays with AA OFF. A history-only signature
            // cut exposes current raw RGB, independently of the component
            // oracle. The reflected texture supplies a genuine neighbourhood
            // colour range: flat per-triangle RGB would be clamped to current
            // RGB by design, making bypass indistinguishable from good history.
            auto reset=std::make_shared<CalibratedGameFrame>(*selected);reset->settings.history_epoch+=100;
            const auto raw=render(reset);
            unsigned changed=0;
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<raw[eye].size();at+=4)
                for(unsigned c=0;c<3;++c) changed+=raw[eye][at+c]!=actual[eye][at+c];
            if(modes[pass].material) rough_stabilized[(pass-8)/2]+=changed;
            else stabilized_bytes+=changed;
            check(changed>0,"AA-OFF native renderer bypassed accepted secondary RGB filtering");
        }
        previous=actual;last=selected;
    }
    check(protected_pixels>50 && moving_bytes>50,"Native reflection owner missed protected/moving scene coverage");
    check(render(base)==plain,"Reflection/TAA OFF did not exactly restore native source");
    unsigned ground_stabilized=0,ground_protected=0;
    for(unsigned material:{6U,7U}) for(unsigned moving_plane=0;moving_plane<2;++moving_plane) {
        const auto stabilized_before=ground_stabilized;
        auto world=std::make_shared<CalibratedGameFrame>(*base);
        world->draws.erase(world->draws.begin()); // analytic floor, not a finite model mirror
        for(auto& draw:world->draws) if(draw.source_key==2) for(auto& v:draw.packet.geometry.vertices) {
            v.position[0]=(v.position[0]-1.4F)*2;v.position[1]=(v.position[1]-.03F)*2+.55F;v.position[2]-=2.4F;
        }
        starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
        auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,false,false,232);
        starfox::vr::place_landscape_ground(floor,-.5F,true);
        CalibratedGroundGradient gradient{{.2F,.3F,.1F},{.4F,.5F,.2F}};gradient.material=material;
        check(apply_calibrated_ground(floor,gradient),"Reflected owner ground fixture metadata failed");
        check(bool(calibrated_ground_motion_source(floor)),"Reflected owner ground lost its unique source-plane metadata");
        world->draws.insert(world->draws.begin(),{std::move(floor),CalibratedGameLayer::world,
            starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
        world->settings.ray_tracing=world->settings.reflections=2;world->settings.enhanced_ground=true;
        world->settings.history_epoch=500+material+moving_plane*10;
        for(unsigned frame=0;frame<6;++frame) {
            auto next=std::make_shared<CalibratedGameFrame>(*world);next->presentation_seconds=2+frame/120.;
            auto now=std::make_shared<starfox::vr::GameSceneSnapshot>(*tick);now->revision=frame;
            next->previous=next->current=now;
            for(auto& draw:next->draws) {
                if(draw.source_key==2) draw.packet.model[12]=frame*-.021F;
                if(moving_plane && draw.source_key==calibrated_ground_source_key) {
                    const auto angle=frame*.011F;draw.packet.model[0]=draw.packet.model[5]=std::cos(angle);
                    draw.packet.model[1]=std::sin(angle);draw.packet.model[4]=-std::sin(angle);
                    draw.packet.model[13]=frame*-.004F;
                }
            }
            const auto actual=render(next);
            check(render(next)==actual,"Held analytic ground reflected light drifted");
            if(frame==2) {
                auto rejected=std::make_shared<CalibratedGameFrame>(*next);
                for(auto& draw:rejected->draws) if(draw.source_key==calibrated_ground_source_key) draw.packet.model[13]-=.08F;
                auto bad_tick=std::make_shared<starfox::vr::GameSceneSnapshot>(*now);bad_tick->revision=99;rejected->current=bad_tick;
                gpu_mock.fail_end=true;auto result=renderer.submit(rejected);
                for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected analytic ground presentation stayed pending");
                (void)read();check(render(next)==actual,"Rejected ground plane or radiance replaced accepted history");
            }
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
                check(actual[eye][at+3]==plain[eye][at+3],"Analytic ground reflection changed native alpha");
                if(plain[eye][at]==255 && plain[eye][at+2]==0 && (plain[eye][at+1]==0 || plain[eye][at+1]==255)) {
                    check(std::equal(actual[eye].begin()+at,actual[eye].begin()+at+4,plain[eye].begin()+at),
                        "Analytic ground reflection history covered protected native/emissive pixels");++ground_protected;
                }
            }
            if(frame==3) {
                auto cut=std::make_shared<CalibratedGameFrame>(*next);cut->settings.history_epoch+=100;
                const auto raw=render(cut);
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<raw[eye].size();at+=4)
                    for(unsigned c=0;c<3;++c) ground_stabilized+=raw[eye][at+c]!=actual[eye][at+c];
            }
            last=next;
        }
        check(ground_stabilized>stabilized_before,moving_plane
            ?"Moving analytic mirror/gold plane bypassed accepted incident-light filtering"
            :"Static analytic mirror/gold plane bypassed accepted incident-light filtering");
    }
    if(!ground_stabilized || ground_protected<=50) throw std::runtime_error(
        "Native analytic ground owner missed actual colour stabilization or protected pixels: stabilized="
        +std::to_string(ground_stabilized)+" protected="+std::to_string(ground_protected));
#include "calibrated_ordered_reflection_owner_checks.inc"
#include "calibrated_msaa_reflection_owner_checks.inc"
#include "calibrated_liquid_reflection_owner_checks.inc"
    // Return a traced frame for the existing outer real-GPU stall/close checks.
    (void)render(last);
    std::cout<<backend<<" native reflected-radiance owner: "<<moving_bytes<<" moving bytes, "<<stabilized_bytes
        <<" AA-OFF stabilized bytes, "<<protected_pixels
        <<" exact protected pixels; independent AA OFF/spatial/2x+4x SSAA/TAA, held resident RGB, two eyes, accepted/reordered/recycled sources, held/cut/OFF, rejected presentation and XR waits passed (lifecycle integration, not a whole-image ray oracle).\n";
    std::cout<<backend<<" analytic mirror/gold owner: "<<ground_stabilized<<" stabilized RGB bytes, "<<ground_protected
        <<" bit-exact protected pixels, static/moving retained planes, held/rejected resident radiance and history-only cuts passed (mocked XR, not physical panel).\n";
    std::cout<<backend<<" rough model owner: amethyst/marble/graphite "<<rough_stabilized[0]<<'/'
        <<rough_stabilized[1]<<'/'<<rough_stabilized[2]
        <<" AA-OFF stabilized bytes; actual 8-lobe ordered-path producer/resolver, AA OFF/TAA, two eyes, held/reordered/recycled/cut/rejected presentations passed.\n";
    return last;
}
