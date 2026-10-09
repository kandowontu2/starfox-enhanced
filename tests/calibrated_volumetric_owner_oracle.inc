// Independent actual-eye/source-geometry reference. The fixture labels raster
// ownership, but never reads private GPU depths, transformed geometry or fog.
template<class Submit,class Consume>
std::shared_ptr<const CalibratedGameFrame> native_volumetric_owner_oracle(Submit submit,Consume consume_raw,
    DisplayXrGameRenderer& renderer,bool srgb,bool rays,const char* backend) {
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto rgba=[&](Images images) {
        if(bgra) for(auto& eye:images) for(unsigned at=0;at<eye.size();at+=4) std::swap(eye[at],eye[at+2]);
        return images;
    };
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> frame){return rgba(submit(std::move(frame)));};
    const auto consume=[&]{return rgba(consume_raw());};
    native_mock.head_x=5;native_mock.sample_x=native_mock.sample_y=0;
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.03F,.05F,.09F,1};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();snapshot->flow=starfox::simulation::GameFlowState::gameplay;
    base->previous=base->current=snapshot;
    const auto quad=[&](float x,float y,float z,float radius,CalibratedGameLayer layer,bool caster,bool after,bool ink) {
        starfox::vr::DrawPacket packet;packet.preserve_native_colour=ink;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=x+corner[0]*radius;v.position[1]=y+corner[1]*radius;v.position[2]=z;
            v.color[0]=ink?1:corner[0]>0?.64F:.11F;v.color[1]=ink?(after?1:0):corner[1]>0?.57F:.13F;
            v.color[2]=ink?0:.21F;v.color[3]=1;
            if(layer==CalibratedGameLayer::world) {v.color[0]=.07F;v.color[1]=.23F;v.color[2]=.41F;}
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);packet.geometry.vertices.push_back(v);
        }
        base->draws.push_back({std::move(packet),layer,starfox::vr::SceneBlend::opaque,!after,0,caster,after});
    };
    quad(0,0,-8,20,CalibratedGameLayer::world,false,false,false);
    quad(0,0,-2,.85F,CalibratedGameLayer::model,true,false,false);
    quad(.21F,.10F,-1.1F,.10F,CalibratedGameLayer::model,true,false,false);
    quad(-.10F,-.05F,-.8F,.04F,CalibratedGameLayer::model,false,false,true);
    quad(.02F,.16F,-.7F,.035F,CalibratedGameLayer::native_ui,false,true,true);
    const auto label=[&](const std::shared_ptr<CalibratedGameFrame>& frame) {
        auto tagged=std::make_shared<CalibratedGameFrame>(*frame);tagged->settings.volumetric_fog=0;tagged->clear={1,0,0,1};
        for(auto& draw:tagged->draws) for(auto& v:draw.packet.geometry.vertices) {
            const bool ink=draw.packet.preserve_native_colour || draw.after_rays;
            v.color[0]=ink?1:0;v.color[1]=!ink && draw.layer==CalibratedGameLayer::world?.5F:!ink && v.position[2]>-1.5F?1:0;
            v.color[2]=!ink && draw.ray_caster && v.position[2]<-1.5F?1:0;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        return render(tagged);
    };
    const auto decode=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto encode=[](double v){return v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;};
    unsigned compared=0,protected_bytes=0,responses=0;
    std::shared_ptr<CalibratedGameFrame> selected;
    for(unsigned variant=0;variant<4;++variant) for(unsigned quality=1;quality<=3;++quality) {
        selected=std::make_shared<CalibratedGameFrame>(*base);selected->settings.volumetric_fog=quality;
        selected->settings.camera_response_pose={0,0,variant==1?.017:0};
        if(variant==2) std::erase_if(selected->draws,[](const auto& d){return d.ray_caster;});
        if(variant==3) selected->source_light={.3,-.1,1};
        auto off=std::make_shared<CalibratedGameFrame>(*selected);off->settings.volumetric_fog=0;
        const auto original=render(off),tags=label(selected),actual=render(selected);
        const unsigned w=mock.image_width,h=mock.image_height;const auto fov=fixture_fov();
        const double l=std::tan(fov.angleLeft),r=std::tan(fov.angleRight),d=std::tan(fov.angleDown),u=std::tan(fov.angleUp);
        const double fx=w/(r-l),fy=h/(u-d),cx=w*(1-(r+l)/(r-l))*.5,cy=h*(1+(u+d)/(u-d))*.5;
        const double angle=selected->settings.camera_response_pose[2],c=std::cos(angle),s=std::sin(angle);
        const auto source_light=selected->source_light;
        const shadows::Vec3 light{c*source_light[0]-s*source_light[1],s*source_light[0]+c*source_light[1],source_light[2]};
        for(unsigned eye=0;eye<2;++eye) {
            shadows::Scene geometry;
            const auto point=[&](const starfox::vr::SceneVertex& v) {
                const double x=256.*v.position[0],y=-256.*v.position[1];
                return shadows::Vec3{c*x-s*y-(native_mock.head_x+(eye?8:-8)),s*x+c*y+2,-256.*v.position[2]+4};
            };
            for(const auto& draw:selected->draws) if(draw.ray_caster)
                for(unsigned at=0;at<draw.packet.geometry.vertices.size();at+=3) {
                    const auto& vertices=draw.packet.geometry.vertices;
                    geometry.add({point(vertices[at]),point(vertices[at+1]),point(vertices[at+2])});
                }
            geometry.build();const auto medium=volumetric_fog_medium(quality);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned at=(y*w+x)*4;const bool ink=tags[eye][at]==255;
                const double z=tags[eye][at+2]==255?516.:tags[eye][at+1]==255?285.6:0;
                const shadows::Vec3 ray{(x+.5-cx)/fx,(y+.5-cy)/fy,1};
                const auto fog=integrate_volumetric_fog(medium,{},ray,z?z*std::sqrt(shadows::dot(ray,ray)):medium.maximum_distance,light,geometry);
                check(bool(fog),"Invalid scalar actual-eye fog reference");
                const double scattering[]{fog->scattering.x,fog->scattering.y,fog->scattering.z};
                for(unsigned channel=0;channel<4;++channel) {
                    int expected=original[eye][at+channel];
                    if(!ink && channel<3) {
                        const double linear=decode(expected/255.)*fog->transmittance+scattering[channel];
                        expected=int(std::lround(encode(std::clamp(linear,0.,1.))*255));
                    }
                    const unsigned tolerance=ink || channel==3?0:srgb?2:1;
                    if(unsigned(std::abs(int(actual[eye][at+channel])-expected))>tolerance)
                        throw std::runtime_error(std::string(backend)+" fog owner oracle variant="+std::to_string(variant)
                            +" quality="+std::to_string(quality)+" eye="+std::to_string(eye)+" byte="+std::to_string(at+channel)
                            +" actual="+std::to_string(actual[eye][at+channel])+" expected="+std::to_string(expected));
                    ++checks;++compared;protected_bytes+=ink;responses+=actual[eye][at+channel]!=original[eye][at+channel];
                }
            }
        }
        check(render(selected)==actual,"Held native fog changed its scattering or source geometry");
        check(render(off)==original,"Fog OFF did not exactly restore the original native scene");
    }
    check(compared>1000 && protected_bytes>100 && responses>100,"Native fog oracle missed geometry/ink/radiance coverage");
    selected=std::make_shared<CalibratedGameFrame>(*base);selected->settings.volumetric_fog=3;
    const auto plain=render(base),tags=label(selected);
    for(unsigned type:{0U,1U,2U,3U,4U,5U,6U}) for(unsigned quality:{1U,2U,3U}) {
        auto aa=std::make_shared<CalibratedGameFrame>(*selected);aa->settings.aa_type=type;aa->settings.aa_quality=quality;
        const auto actual=render(aa);check(render(aa)==actual,"Fog and AA wobbled on a held presentation");
        check(actual[0]!=actual[1],"Native fog collapsed independently calibrated eyes");
        for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
            check(actual[eye][at+3]==plain[eye][at+3],"Fog/AA changed opaque alpha");
            if(tags[eye][at]==255 && plain[eye][at]==255)
                check(std::equal(actual[eye].begin()+at,actual[eye].begin()+at+4,plain[eye].begin()+at),"Fog/AA altered protected centre ink");
        }
        auto combined=std::make_shared<CalibratedGameFrame>(*aa);combined->settings.depth_enhancements=15;
        combined->settings.bloom_model=2;combined->settings.bloom_world=1;combined->settings.contrast=2;combined->settings.chromatic=1;
        combined->settings.global_enhancements=global_enhancement_mask;combined->settings.scene_enhancements=255;
        combined->settings.particle_enhancements=15;combined->scene_fx.add({10,-30,400},100,.8F,0,.18F,{.8F,.6F,.25F,0},{1,1,0,0});
        if(rays) {combined->settings.ray_tracing=combined->settings.reflections=2;combined->settings.material=unsigned(Effect::mirror);}
        const auto enhanced=render(combined);check(render(combined)==enhanced,"Combined native fog/FX/rays did not retain held pixels");
        check(render(base)==plain,"Fog/AA OFF retained altered scene pixels");
    }
    for(unsigned type:{0U,3U,4U,6U}) {
        auto undimmed=std::make_shared<CalibratedGameFrame>(*selected);
        undimmed->settings.aa_type=type;undimmed->settings.aa_quality=type?3:0;
        undimmed->settings.history_epoch=20000;
        const auto finished=render(undimmed);
        auto dimmed=std::make_shared<CalibratedGameFrame>(*undimmed);
        dimmed->settings.history_epoch=20001;
        auto shade=dimmed->draws.back();shade.blend=starfox::vr::SceneBlend::alpha;
        for(auto& v:shade.packet.geometry.vertices) {
            v.position[0]=.02F+(v.position[0]-.02F)*4;
            v.position[1]=.16F+(v.position[1]-.16F)*4;
            v.color[0]=v.color[1]=v.color[2]=0;v.color[3]=.75F;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        dimmed->draws.push_back(std::move(shade));
        auto mask=std::make_shared<CalibratedGameFrame>(*undimmed);
        mask->settings.history_epoch=20002;
        mask->draws.push_back(dimmed->draws.back());mask->draws.back().blend=starfox::vr::SceneBlend::opaque;
        for(auto& v:mask->draws.back().packet.geometry.vertices) {
            v.color[0]=v.color[1]=v.color[2]=1;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        const auto coverage=render(mask),actual=render(dimmed);unsigned dim_pixels=0;
        for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
            const bool ink=coverage[eye][at]==255 && coverage[eye][at+1]==255 && coverage[eye][at+2]==255;
            dim_pixels+=ink;
            for(unsigned channel=0;channel<4;++channel) {
                int target=finished[eye][at+channel];
                if(ink && channel<3) target=int(std::lround((srgb?encode(decode(target/255.)*.25):target/255.*.25)*255));
                if(std::abs(int(actual[eye][at+channel])-target)>int(ink && channel<3))
                    throw std::runtime_error(std::string(backend)+" native fog dimmer type="+std::to_string(type)
                        +" eye="+std::to_string(eye)+" byte="+std::to_string(at+channel)+" mask="+std::to_string(ink)
                        +" actual="+std::to_string(actual[eye][at+channel])+" expected="+std::to_string(target));
                ++checks;
            }
        }
        check(dim_pixels>10,"Native fog UI dimmer had no coverage");
    }
    // TAA fog is reactive: moving occlusion/light cannot inherit fabricated
    // rigid motion. Independently render the two actual XR sample offsets and
    // reconstruct model coverage on the CPU; sky uses the unshifted volume.
    constexpr int phases[2][2]{{0,-43},{-64,43}};
    std::array<Images,2> centres,centre_tags,samples,sample_tags;
    std::array<std::shared_ptr<CalibratedGameFrame>,2> frames;
    for(unsigned f=0;f<2;++f) {
        frames[f]=std::make_shared<CalibratedGameFrame>(*selected);
        frames[f]->source_light=f?std::array<double,3>{.3,-.1,1}:selected->source_light;
        native_mock.sample_x=native_mock.sample_y=0;
        centres[f]=render(frames[f]);centre_tags[f]=label(frames[f]);
        native_mock.sample_x=-phases[f][0]/16.F;native_mock.sample_y=-phases[f][1]/16.F;
        auto sample=std::make_shared<CalibratedGameFrame>(*frames[f]);
        std::erase_if(sample->draws,[](const auto& draw){return draw.after_rays;});
        samples[f]=render(sample);sample_tags[f]=label(sample);
    }
    native_mock.sample_x=native_mock.sample_y=0;
    const auto model=[](const std::vector<unsigned char>& tags,unsigned at) {
        return tags[at]==0 && (tags[at+1]==255 || tags[at+2]==255);
    };
    const unsigned w=mock.image_width,h=mock.image_height;
    for(unsigned quality=1;quality<=3;++quality) {
        (void)render(base);
        for(unsigned f=0;f<2;++f) {
            auto aa=std::make_shared<CalibratedGameFrame>(*frames[f]);aa->settings.aa_type=4;aa->settings.aa_quality=quality;
            const auto actual=render(aa);
            for(unsigned eye=0;eye<2;++eye) for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned at=(y*w+x)*4;
                const double tx=std::clamp(x+phases[f][0]/256.,0.,double(w-1)),ty=std::clamp(y+phases[f][1]/256.,0.,double(h-1));
                const unsigned ax=unsigned(tx),ay=unsigned(ty);const double fx=tx-ax,fy=ty-ay;
                for(unsigned channel=0;channel<4;++channel) {
                    int expected=centres[f][eye][at+channel];
                    if(channel<3 && model(centre_tags[f][eye],at)) {
                        double linear=0;
                        for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                            const unsigned tap=(std::min(ay+dy,h-1)*w+std::min(ax+dx,w-1))*4;
                            const double rgb=(model(sample_tags[f][eye],tap)?samples[f][eye][tap+channel]:centres[f][eye][at+channel])/255.;
                            linear+=(srgb?decode(rgb):rgb)*(dx?fx:1-fx)*(dy?fy:1-fy);
                        }
                        expected=int(std::lround(std::clamp(srgb?encode(linear):linear,0.,1.)*255));
                    }
                    check(std::abs(int(actual[eye][at+channel])-expected)<=int(channel<3 && model(centre_tags[f][eye],at))*2,
                        "Native fog TAA differs from independent reactive XR-sample reconstruction");
                }
            }
            check(render(aa)==actual,"Native fog TAA did not hold the reconstructed centre grid");
        }
    }
    // Independent single-sample XR offsets reference 2/4/8-sample fog, rays,
    // depth, appearance, trails, exposure and rejected/paused presentations.
    native_msaa_owner_oracle(render,consume,renderer,selected,rays,backend);
    selected->settings.aa_type=4;selected->settings.aa_quality=3;
    const auto expected=render(selected);gpu_mock.timeout_right=true;
    check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Fog XR wait lost retained geometry");
    const auto locates=native_mock.locates;native_mock.head_x+=17;
    check(renderer.poll()==DisplayXrSubmission::waiting && native_mock.locates==locates,"Fog XR retry relocated the source eyes");
    gpu_mock.timeout_right=false;auto outcome=renderer.poll();
    for(unsigned i=0;outcome==DisplayXrSubmission::waiting && i<10000;++i) {SDL_Delay(1);outcome=renderer.poll();}
    check(outcome==DisplayXrSubmission::submitted && consume()==expected,"Fog XR retry changed completed integral/images");native_mock.head_x-=17;
    auto rejected=std::make_shared<CalibratedGameFrame>(*selected);rejected->source_light={.3,-.1,1};
    gpu_mock.fail_end=true;outcome=renderer.submit(rejected);
    for(unsigned i=0;renderer.frame_pending() && i<10000;++i) {SDL_Delay(1);outcome=renderer.poll();}
    check(outcome==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected fog presentation remained accepted/pending");(void)consume();
    check(render(selected)==expected,"Rejected fog presentation changed accepted TAA colour/phase");
    auto invalid=std::make_shared<CalibratedGameFrame>(*selected);invalid->settings.volumetric_fog=4;
    const auto before=native_mock.locates;
    check(renderer.submit(invalid)==DisplayXrSubmission::failed && native_mock.locates==before,"Invalid fog quality acquired XR images");
    std::cout<<backend<<" native fog owner: "<<compared<<" independent source/FOV/scattering bytes, "<<protected_bytes
        <<" protected ink bytes, "<<responses<<" responses; all qualities, RT OFF, empty world, camera response/light changes, all AA, combined passes, held/OFF, rejection and XR retries passed.\n";
    native_mock.head_x=0;native_mock.sample_x=native_mock.sample_y=0;
    return selected;
}
