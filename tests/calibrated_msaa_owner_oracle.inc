// Independent owner-level composition oracle: ordinary SINGLE-sample frames
// through independently perturbed mock XR FOVs. It never calls the MSAA sample
// camera/transport/finish helper. Core sample guides have a separate geometric
// CPU oracle. This checks the application ordering and sample/history ownership.
template<class Render,class Consume>
void native_msaa_owner_oracle(Render render,Consume consume,DisplayXrGameRenderer& renderer,
    const std::shared_ptr<const CalibratedGameFrame>& original,bool rays,const char* backend) {
    using Images=std::array<std::vector<unsigned char>,2>;
    constexpr int positions[14][2]{{4,4},{-4,-4},{-2,-6},{6,-2},{-6,2},{2,6},
        {1,-3},{-1,3},{5,1},{-3,-5},{-5,5},{-7,-1},{3,7},{7,-7}};
    auto source=std::make_shared<CalibratedGameFrame>(*original);
    source->settings={};source->settings.srgb=original->settings.srgb;
    source->settings.scene_enhancements=original->settings.scene_enhancements;
    source->settings.particle_enhancements=original->settings.particle_enhancements;
    source->settings.volumetric_fog=original->settings.volumetric_fog;
    std::erase_if(source->draws,[](const auto& draw){return draw.after_rays;});
    if(std::none_of(source->draws.begin(),source->draws.end(),[](const auto& d){return d.ray_caster;}))
        for(auto& d:source->draws) if(d.layer==CalibratedGameLayer::model) {d.ray_caster=true;break;}
    const auto plain=render(source);
    auto labels=std::make_shared<CalibratedGameFrame>(*source);labels->clear={0,1,0,1};
    labels->scene_fx={};labels->settings.scene_enhancements=labels->settings.particle_enhancements=0;
    labels->settings.volumetric_fog=0;
    for(auto& draw:labels->draws) {
        const bool protected_ink=draw.packet.preserve_native_colour
            || (draw.layer!=CalibratedGameLayer::world && !(draw.layer==CalibratedGameLayer::model && draw.ray_caster));
        for(auto& v:draw.packet.geometry.vertices) {
            v.color[0]=v.odd_color[0]=protected_ink?1:0;v.color[1]=v.odd_color[1]=protected_ink?0:1;
            v.color[2]=v.odd_color[2]=0;v.color[3]=v.odd_color[3]=1;
        }
    }
    // Synthetic triangle fixtures have no texture recolouring ambiguity.
    const bool geometric_labels=std::all_of(labels->draws.begin(),labels->draws.end(),[](const auto& d){return d.packet.geometry.texel_view().empty();});
    constexpr unsigned frames=3;
    std::array<Images,frames> center_labels,center_ink;
    for(unsigned frame=0;frame<frames;++frame) {
        auto ink=std::make_shared<CalibratedGameFrame>(*source),label=std::make_shared<CalibratedGameFrame>(*labels);
        if(frame) for(auto* selected:{ink.get(),label.get()}) for(auto& draw:selected->draws) if(draw.ray_caster)
            for(auto& v:draw.packet.geometry.vertices) v.position[0]+=.035F;
        center_ink[frame]=render(ink);if(geometric_labels) center_labels[frame]=render(label);
    }
    const auto decode=[&](double x){return source->settings.srgb?(x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4)):x;};
    const auto encode=[&](double x){return source->settings.srgb?(x<=.0031308?x*12.92:1.055*std::pow(x,1/2.4)-.055):x;};
    unsigned checked=0;bool changed=false;
    for(unsigned quality:{1U,2U,3U}) {
        const unsigned count=1U<<quality;
        for(unsigned combination=0;combination<3;++combination) {
            auto selected=std::make_shared<CalibratedGameFrame>(*source);
            selected->settings.aa_type=6;selected->settings.aa_quality=quality;
            if(combination) {
                selected->settings.depth_enhancements=15;selected->settings.contrast=2;selected->settings.chromatic=1;
                selected->settings.bloom_model=2;selected->settings.bloom_world=1;
                selected->settings.global_enhancements=global_enhancement_mask;
                selected->settings.world_effects={unsigned(Effect::sepia),65,0,0};
                if(rays) {selected->settings.ray_tracing=selected->settings.reflections=2;selected->settings.material=unsigned(Effect::gold_metal);}
            }
            if(combination==2) {
                selected->settings.manipulation=unsigned(Effect::trails);selected->settings.manipulation_intensity=65;
                selected->settings.extra_effects[0]=unsigned(Effect::long_exposure);
                selected->settings.phosphor=2;selected->settings.exposure=2;
            }
            std::array<std::array<Images,frames>,8> references;
            for(unsigned sample=0;sample<count;++sample) {
                native_mock.sample_x=positions[count-2+sample][0];native_mock.sample_y=positions[count-2+sample][1];
                for(unsigned frame=0;frame<frames;++frame) {
                    auto single=std::make_shared<CalibratedGameFrame>(*selected);single->settings.aa_quality=0;
                    // Fresh independent history for EACH reference sample.
                    single->settings.history_epoch=1000+sample;
                    single->settings.effect_seconds=frame==0?0:.09F;
                    single->settings.exposure_paused=frame==2;
                    if(frame) for(auto& draw:single->draws) if(draw.ray_caster)
                        for(auto& v:draw.packet.geometry.vertices) v.position[0]+=.035F;
                    references[sample][frame]=render(single);
                }
            }
            native_mock.sample_x=native_mock.sample_y=0;
            for(unsigned frame=0;frame<frames;++frame) {
                auto multiple=std::make_shared<CalibratedGameFrame>(*selected);multiple->settings.effect_seconds=frame==0?0:.09F;
                multiple->settings.exposure_paused=frame==2;
                if(frame) for(auto& draw:multiple->draws) if(draw.ray_caster)
                    for(auto& v:draw.packet.geometry.vertices) v.position[0]+=.035F;
                const auto actual=render(multiple);changed|=actual!=plain;
                check(actual[0]!=actual[1],"MSAA owner collapsed actual eye geometry");
                if(geometric_labels) for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
                    const bool ink=center_labels[frame][eye][at]==255;
                    for(unsigned c=0;c<4;++c) {
                        double sum=0;for(unsigned sample=0;sample<count;++sample)
                            sum+=c==3?references[sample][frame][eye][at+c]/255.:decode(references[sample][frame][eye][at+c]/255.);
                        const auto expected=ink?center_ink[frame][eye][at+c]:std::lround((c==3?sum/count:encode(sum/count))*255);
                        if(std::abs(int(actual[eye][at+c])-int(expected))>2)
                            throw std::runtime_error(std::string(backend)+" MSAA owner/sample oracle mismatch q="+std::to_string(quality)
                                +" combo="+std::to_string(combination)+" frame="+std::to_string(frame)+" eye="+std::to_string(eye)+" at="+std::to_string(at+c)
                                +" actual="+std::to_string(actual[eye][at+c])+" expected="+std::to_string(expected));
                        ++checked;
                    }
                }
                if(frame==2) check(render(multiple)==actual,"Paused per-sample history/exposure drifted");
            }
            check(render(source)==plain,"MSAA OFF did not restore exact original eye image");
            // A held acquisition cannot encode history or advance a sample.
            auto held=std::make_shared<CalibratedGameFrame>(*selected);
            const auto expected=render(held);gpu_mock.timeout_right=true;
            check(renderer.submit(held)==DisplayXrSubmission::waiting && renderer.retained_frame()==held,"MSAA wait lost sample source");
            check(renderer.poll()==DisplayXrSubmission::waiting,"MSAA image wait resampled history");
            gpu_mock.timeout_right=false;
            auto result=renderer.poll();for(unsigned i=0;result==DisplayXrSubmission::waiting && i<10000;++i) {SDL_Delay(1);result=renderer.poll();}
            check(result==DisplayXrSubmission::submitted,renderer.status().c_str());
            check(consume()==expected,"MSAA wait changed completed sample/history pixels");
            check(render(source)==plain,"MSAA retained acquisition did not recover");
            if(combination==2) {
                (void)render(held);
                auto rejected=std::make_shared<CalibratedGameFrame>(*held);rejected->settings.effect_seconds=.09F;
                for(auto& draw:rejected->draws) if(draw.ray_caster) for(auto& v:draw.packet.geometry.vertices) v.position[0]+=.035F;
                gpu_mock.fail_end=true;result=renderer.submit(rejected);
                for(unsigned i=0;result==DisplayXrSubmission::waiting && i<10000;++i) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::failed && !renderer.frame_pending() && !renderer.retained_frame(),"Rejected MSAA presentation retained/committed a sample history");
                (void)consume(); // inspection of a rejected image is not acceptance
                const auto recovered=render(rejected);
                auto pristine=std::make_shared<CalibratedGameFrame>(*rejected);++pristine->settings.history_epoch;
                check(render(pristine)==recovered,"Rejected MSAA presentation seeded a later sample's history/exposure");
                check(render(source)==plain,"Rejected MSAA history did not recover OFF");
            }
        }
    }
    check(changed,"MSAA owner never changed actual sample coverage");
    std::cout<<backend<<" MSAA owner: 2/4/8 hardware samples, "<<checked
        <<" independent XR single-sample oracle bytes; per-eye/sample ray/depth/global/bloom/appearance and temporal/exposure histories, paused frames, image waits and exact OFF passed.\n";
}
