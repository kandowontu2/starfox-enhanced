// Exercises the actual native owner, fixed panel images and changing *raster*
// sizes. Independent scalar filtering is checked in the component fixture;
// here the full-resolution source oracle checks excluded ink and final overlays.
template<class Render>
std::shared_ptr<const CalibratedGameFrame> native_fsr_owner_oracle(Render render,DisplayXrGameRenderer& renderer,
    bool srgb,bool rays,const char* backend) {
    auto source=std::make_shared<CalibratedGameFrame>();source->settings.srgb=srgb;source->clear={.07F,.11F,.18F,1};
    source->current=source->previous=std::make_shared<starfox::vr::GameSceneSnapshot>();
    const auto add=[&](float x,float y,float z,float radius,bool model,bool after){
        starfox::vr::DrawPacket p;constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners){starfox::vr::SceneVertex v{};v.position[0]=x+corner[0]*radius+corner[1]*radius*.17F;
            v.position[1]=y+corner[1]*radius+corner[0]*radius*.13F;v.position[2]=z;
            v.color[0]=model?(corner[0]>0?.8F:.18F):1;v.color[1]=model?(corner[1]>0?.6F:.12F):after?1:0;
            v.color[2]=model?.35F:0;v.color[3]=1;std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
            p.geometry.vertices.push_back(v);}
        source->draws.push_back({std::move(p),after?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
            starfox::vr::SceneBlend::opaque,!after,0,model,after});};
    add(0,0,-2,.7F,true,false);add(.13F,.06F,-1.1F,.07F,true,false);
    add(-.08F,-.05F,-.8F,.025F,false,false);add(.04F,.1F,-.7F,.035F,false,true);
    const auto rgba=[&](auto images){
        // The D3D12 readback callback already normalizes BGRA. Vulkan's raw
        // transfer callback still needs the same conversion as other oracles.
        const bool bgra=mock.backend==DisplayXrBackend::vulkan
            && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
        if(bgra)for(auto& eye:images)for(unsigned at=0;at<eye.size();at+=4)std::swap(eye[at],eye[at+2]);return images;};
    const auto baseline=rgba(render(source));unsigned protected_bytes=0,changes=0,cases=0;
    std::shared_ptr<CalibratedGameFrame> final;
    for(unsigned mode=1;mode<=4;++mode)for(unsigned aa:{0U,1U,3U,4U,6U}) {
        auto next=std::make_shared<CalibratedGameFrame>(*source);next->settings.fsr1_mode=mode;
        next->settings.aa_type=aa;next->settings.aa_quality=aa?2:0;
        next->settings.ray_tracing=rays?2:0;next->settings.reflections=rays?2:0;
        if(aa==1){next->settings.bloom_model=2;next->settings.contrast=1;next->settings.chromatic=1;}
        const auto output=rgba(render(next));check(output[0]!=output[1],"Native FSR flattened calibrated stereo");
        const auto input=starfox::render::fsr1_input_extent({mock.image_width,mock.image_height},static_cast<starfox::render::Fsr1Mode>(mode));
        const unsigned factor=aa==3?3:1;
        for(unsigned eye=0;eye<2;++eye){check(renderer.scene_extent(eye)==std::array<std::uint32_t,2>{input.width*factor,input.height*factor},
                "Native FSR did not rasterize at its real input extent");
            check(output[eye].size()==baseline[eye].size(),"Native FSR resized the runtime panel image");
            for(unsigned at=0;at<output[eye].size();at+=4){
                bool protected_pixel=baseline[eye][at]>250 && baseline[eye][at+2]<2
                    && (baseline[eye][at+1]<2 || baseline[eye][at+1]>250);
                for(unsigned k=0;k<4;++k){if(protected_pixel){check(output[eye][at+k]==baseline[eye][at+k],"Native FSR filtered or misplaced original ink/overlay");++protected_bytes;}
                    else changes+=output[eye][at+k]!=baseline[eye][at+k];}
            }
        }
        // One accepted source repeated: no jitter, stale opposite eye or
        // quality-transition history may move a static native presentation.
        check(rgba(render(next))==output,"Native FSR repeated source is unstable");
        check(rgba(render(source))==baseline,"Native FSR OFF did not restore the exact full-resolution source");
        final=next;++cases;
    }
    check(changes && protected_bytes,"Native FSR fixture lacks visible upscaled scene or protected ink");
    auto bad=std::make_shared<CalibratedGameFrame>(*source);bad->settings.fsr1_mode=5;
    check(renderer.submit(bad)==DisplayXrSubmission::failed && !renderer.frame_pending(),"Invalid native FSR selection retained a frame");
    std::cout<<backend<<" native FSR owner: "<<cases<<" mode/AA/ray/effect combinations; "<<protected_bytes
        <<" exact protected full-panel bytes; real lower raster extents, stereo, held-source and OFF/invalid transitions passed"<<std::endl;
    return final;
}
