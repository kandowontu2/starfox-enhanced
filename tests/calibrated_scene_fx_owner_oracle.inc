// Owner-level reference: independently render geometric labels, analytically
// project source points/plane depths from the mock's actual XR FOV/eye pose,
// and evaluate the shared scalar radiance equations. Never read private owner
// guides, projected buffers or submit an uploaded 2D replacement scene.
template<class Submit,class Consume>
std::shared_ptr<const CalibratedGameFrame> native_scene_fx_owner_oracle(Submit submit,Consume consume_raw,
    DisplayXrGameRenderer& renderer,bool srgb,bool rays,const char* backend) {
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto rgba=[&](Images images) {if(bgra) for(auto& eye:images) for(unsigned at=0;at<eye.size();at+=4)
        std::swap(eye[at],eye[at+2]);return images;};
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> frame){return rgba(submit(std::move(frame)));};
    const auto consume=[&]{return rgba(consume_raw());};
    native_mock.head_x=5;native_mock.sample_x=native_mock.sample_y=0;
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.03F,.05F,.09F,1};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();snapshot->flow=starfox::simulation::GameFlowState::gameplay;
    base->previous=base->current=snapshot;
    const auto quad=[&](float x,float y,float z,float radius,CalibratedGameLayer layer,bool caster,bool after,bool protected_ink) {
        starfox::vr::DrawPacket packet;packet.preserve_native_colour=protected_ink;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto corner:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=x+corner[0]*radius;v.position[1]=y+corner[1]*radius;v.position[2]=z;
            v.color[0]=protected_ink?1:corner[0]>0?.64F:.11F;
            v.color[1]=protected_ink?(after?1:0):corner[1]>0?.57F:.13F;
            v.color[2]=protected_ink?0:.21F;v.color[3]=1;
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
    const auto labels=[&](const std::shared_ptr<CalibratedGameFrame>& source) {
        auto label=std::make_shared<CalibratedGameFrame>(*source);label->clear={1,0,0,1};label->scene_fx={};
        label->settings.scene_enhancements=label->settings.particle_enhancements=0;
        for(unsigned d=0;d<label->draws.size();++d) for(auto& v:label->draws[d].packet.geometry.vertices) {
            const bool ink=d>=3;v.color[0]=ink?1:0;v.color[1]=d==0?.5F:d==2?1:0;v.color[2]=d==1?1:0;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        }
        return render(label);
    };
    unsigned compared=0,protected_bytes=0,responses=0,occluded=0;
    const auto compare=[&](const Images& actual,const Images& expected,const Images& original,const Images& tags) {
        for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();++at) {
            const bool ink=tags[eye][(at/4)*4]==255;
            const unsigned tolerance=ink || at%4==3?0:srgb?2:1;
            if(unsigned(std::abs(int(actual[eye][at])-int(expected[eye][at])))>tolerance)
                throw std::runtime_error(std::string(backend)+" native FX owner byte="+std::to_string(at)+" eye="+std::to_string(eye)
                    +" actual="+std::to_string(actual[eye][at])+" expected="+std::to_string(expected[eye][at]));
            ++checks;++compared;protected_bytes+=ink;responses+=actual[eye][at]!=original[eye][at];
        }
    };
    const auto oracle=[&](const std::shared_ptr<CalibratedGameFrame>& source,const Images& colors,const Images& tags) {
        auto expected=colors;const unsigned w=mock.image_width,h=mock.image_height;
        const auto fov=fixture_fov();const double l=std::tan(fov.angleLeft),r=std::tan(fov.angleRight),d=std::tan(fov.angleDown),u=std::tan(fov.angleUp);
        const float fx=float(w/(r-l)),fy=float(h/(u-d)),cx=float(w*(1-(r+l)/(r-l))*.5-.5),cy=float(h*(1+(u+d)/(u-d))*.5-.5);
        const float ratio=fy/fx;const double roll=source->settings.camera_response_pose[2],c=std::cos(roll),s=std::sin(roll);
        for(unsigned eye=0;eye<2;++eye) {
            SceneFxFrame points;points.camera={cx,cy,fx,0};
            const auto project=[&](std::array<double,3> p) {
                for(unsigned a=0;a<3;++a) {p[a]=std::fmod(p[a]-source->scene_fx_origin[a],65536.);
                    if(p[a]>32767) p[a]-=65536.;else if(p[a]<-32768) p[a]+=65536.;}
                return std::array<float,3>{float(c*p[0]-s*p[1]-(native_mock.head_x+(eye?8:-8))),float(s*p[0]+c*p[1]+2),float(p[2]+4)};
            };
            const float focus=source->scene_fx.player?project(*source->scene_fx.player)[2]:0;
            for(unsigned n=0;n<source->scene_fx.count;++n) {
                const auto& point=source->scene_fx.points[n];const auto p=project(point.position);
                if(p[2]<32 || p[2]>10000 || (point.type==8 && p[2]<std::max(32.F,focus*.6F))) continue;
                const float radius=std::clamp(fx*point.radius/p[2],.5F,point.type==8?30.F:180.F);auto extra=point.extra;
                if(point.type==1) extra={p[0],p[1],p[2],point.radius};
                if(point.type==8) extra={focus,std::max(64.F,focus*.5F),0,0};
                points.add({cx+fx*p[0]/p[2],cy+fx*p[1]/p[2],radius,point.strength},{point.type,p[2],point.age,0},extra,point.identity);
            }
            const auto depth=[&](unsigned pixel) {return tags[eye][pixel*4+2]==255?516.F:tags[eye][pixel*4+1]==255?285.6F:0.F;};
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned pixel=y*w+x,at=pixel*4;if(tags[eye][at]==255) continue;
                for(unsigned channel=0;channel<3;++channel) {
                    const auto sample=[&](float px,float py,float focus_depth=0,float range=0,float plume=0) {
                        px=std::clamp(px,0.F,float(w-1));py=std::clamp(cy+(py-cy)*ratio,0.F,float(h-1));
                        const unsigned ax=unsigned(px),ay=unsigned(py);const float tx=px-ax,ty=py-ay;float value=0;
                        for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                            unsigned tap=std::min(ay+dy,h-1)*w+std::min(ax+dx,w-1);const float z=depth(tap);
                            if(tags[eye][tap*4]==255 || (range>0 && z>0 && (std::abs(z-focus_depth)<range || z+8<plume))) tap=pixel;
                            value+=colors[eye][tap*4+channel]*(dx?tx:1-tx)*(dy?ty:1-ty);
                        }
                        return value/255.F;
                    };
                    const float z=depth(pixel);
                    expected[eye][at+channel]=static_cast<unsigned char>(scene_channel(sample,points,float(x),cy+(y-cy)/ratio,channel,1,z,0,0,z>0?-1.F:0.F)*255.F+.5F);
                    if(z>0 && source->scene_fx.count && z+8<project(source->scene_fx.points[0].position)[2]) ++occluded;
                }
            }
        }
        return expected;
    };
    std::shared_ptr<CalibratedGameFrame> combined;
    for(unsigned mode=0;mode<11;++mode) {
        auto selected=std::make_shared<CalibratedGameFrame>(*base);
        selected->settings.scene_enhancements=255;selected->settings.particle_enhancements=15;
        selected->settings.camera_response_pose={0,0,mode==10?.017:0};
        selected->scene_fx_origin={999998000.25,-999998000.75,999998000.125};
        const auto put=[&](unsigned type,double z) {
            auto p=selected->scene_fx_origin;p[0]+=type==8?170:10;p[1]-=30;p[2]+=z;
            selected->scene_fx.add(p,type==1?600:type==0?100:type==8?90:40,.8F,float(type),.18F,{.8F,.6F,.25F,0},{int(type),1,0,0});
        };
        if(mode<9) put(mode,400);else for(unsigned type=0;type<9;++type) put(type,type==7?650:400);
        auto player=selected->scene_fx_origin;player[2]+=300;selected->scene_fx.player=player;
        auto plain_source=std::make_shared<CalibratedGameFrame>(*selected);plain_source->scene_fx={};
        const auto original=render(plain_source),tags=labels(selected),expected=oracle(selected,original,tags),actual=render(selected);
        compare(actual,expected,original,tags);check(render(selected)==actual,"Held raw native FX aged/spawned or changed projection");
        check(actual[0]!=actual[1],"Native FX flattened tracked eyes");
        if(mode<9 && actual==original) throw std::runtime_error("Native owner FX type "+std::to_string(mode)+" had no observable response");
        check(render(plain_source)==original,"Native FX OFF did not restore the original native image");
        combined=selected;
    }
    check(compared>1000 && responses>100 && protected_bytes>100 && occluded>100,"Owner FX oracle missed response/ink/occlusion coverage");
    const auto labels_combined=labels(combined),plain=render(base);
    for(unsigned type:{0U,1U,2U,3U,4U,5U,6U}) for(unsigned quality:{1U,2U,3U}) {
        auto selected=std::make_shared<CalibratedGameFrame>(*combined);selected->settings.aa_type=type;selected->settings.aa_quality=quality;
        const auto actual=render(selected);
        check(render(selected)==actual,"Native AA and FX changed held output");
        for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
            check(actual[eye][at+3]==plain[eye][at+3],"Native FX/AA changed opaque alpha");
            if(labels_combined[eye][at]==255 && plain[eye][at]==255)
                check(std::equal(actual[eye].begin()+at,actual[eye].begin()+at+4,plain[eye].begin()+at),"Native FX/AA altered UI/emissive ink");
        }
        if(type==4 && quality==3) {
            gpu_mock.timeout_right=true;const auto locates=native_mock.locates;
            check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Native FX wait lost raw event frame");
            const auto retained_locates=native_mock.locates;native_mock.head_x+=17;
            check(retained_locates==locates+1 && renderer.poll()==DisplayXrSubmission::waiting
                && native_mock.locates==retained_locates,"Native FX retry resampled tracked eye cameras");
            gpu_mock.timeout_right=false;auto result=renderer.poll();
            for(unsigned attempt=0;result==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);result=renderer.poll();}
            check(result==DisplayXrSubmission::submitted && consume()==actual,"Native FX retry changed retained event/camera output");native_mock.head_x-=17;
            auto rejected=std::make_shared<CalibratedGameFrame>(*selected);rejected->scene_fx.points[0].position[0]+=77;
            gpu_mock.fail_end=true;result=renderer.submit(rejected);
            for(unsigned attempt=0;result==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);result=renderer.poll();}
            check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected FX frame remained pending/accepted");(void)consume();
            check(render(selected)==actual,"Rejected FX frame seeded accepted TAA events/history");
        }
        auto enhanced=std::make_shared<CalibratedGameFrame>(*selected);enhanced->settings.depth_enhancements=15;
        enhanced->settings.bloom_model=2;enhanced->settings.bloom_world=1;enhanced->settings.contrast=2;enhanced->settings.chromatic=1;
        enhanced->settings.global_enhancements=global_enhancement_mask;
        if(rays) {enhanced->settings.ray_tracing=enhanced->settings.reflections=2;enhanced->settings.material=unsigned(Effect::mirror);}
        const auto appearance=render(enhanced);check(render(enhanced)==appearance,"Combined native effects did not retain a held presentation");
    }
    auto invalid=std::make_shared<CalibratedGameFrame>(*combined);invalid->settings.scene_enhancements=0;
    const auto locates=native_mock.locates;check(renderer.submit(invalid)==DisplayXrSubmission::failed && native_mock.locates==locates
        && !renderer.frame_pending(),"Disabled raw effect acquired a new XR frame");
    // Independently resolve per-sample single-eye renders: all 2/4/8 samples,
    // depths, post effects, history/exposure and partial XR presentation.
    native_msaa_owner_oracle(render,consume,renderer,combined,rays,backend);
    std::cout<<backend<<" native FX owner: "<<compared<<" independent source/FOV/radiance bytes, "<<protected_bytes
        <<" protected ink bytes, "<<responses<<" responses; nine types, occlusion, fractional wrapped origins, camera response, all AA qualities, held/OFF, typed rejection and XR retries passed.\n";
    native_mock.sample_x=native_mock.sample_y=0;native_mock.head_x=0;
    combined->settings.aa_type=4;combined->settings.aa_quality=3;
    return combined;
}
