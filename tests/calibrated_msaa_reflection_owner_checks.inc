// Whole native MSAA/history oracle: render each sample independently through
// an externally shifted XR FOV, then average its finished radiance in binary64.
// The reference does not call the production sample camera/transport helpers,
// cannot borrow another reference sample's cache, and cannot use resolved
// centre ownership as a substitute for genuine hardware sample coverage.
{
    using Effect=starfox::render::Effect;
    if(native_msaa_reflection_lava_only) check(renderer.enable_reflection_diagnostics(true),"Could not enable exact liquid diagnostic inputs");
    struct Selection {unsigned ground;Effect model;};
    constexpr Selection selections[]{{0,Effect::mirror},{6,Effect::gold_metal},
        {7,Effect::metallic},{5,Effect::off},{9,Effect::off}};
    constexpr int positions[14][2]{{4,4},{-4,-4},{-2,-6},{6,-2},{-6,2},{2,6},
        {1,-3},{-1,3},{5,1},{-3,-5},{-5,5},{-7,-1},{3,7},{7,-7}};
    const auto decode=[&](double x){return srgb?(x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4)):x;};
    const auto encode=[&](double x){return srgb?(x<=.0031308?x*12.92:1.055*std::pow(x,1/2.4)-.055):x;};
    unsigned checked=0,protected_ink=0,stabilized=0;
    for(const auto selection:selections) for(unsigned quality:{1U,2U,3U}) {
        if(native_msaa_reflection_water_only && (selection.ground!=5 || quality!=2)) continue;
        if(native_msaa_reflection_lava_only && selection.ground!=9) continue;
        // The explicit lava mode additionally captures exact native inputs.
        // Ordinary coverage includes lava too, without diagnostic readbacks
        // or any diagnostic flag enabling a different runtime history path.
        const unsigned count=1U<<quality;
        auto fixture=std::make_shared<CalibratedGameFrame>(*base);
        if(selection.ground==5 || selection.ground==9) {
            // Real finite reflected liquid hits, rather than only environment
            // misses (which the legacy curved-hit cache correctly rejects).
            std::erase_if(fixture->draws,[](const auto& draw){return draw.source_key==1;});
            for(auto& draw:fixture->draws) if(draw.source_key==2) for(auto& v:draw.packet.geometry.vertices) {
                v.position[0]=(v.position[0]-1.4F)*2;v.position[1]=(v.position[1]-.03F)*2+.55F;v.position[2]-=2.4F;
            }
        } else {
            for(auto& v:fixture->draws[0].packet.geometry.vertices) {v.position[0]*=7;v.position[1]*=7;}
            for(auto& v:fixture->draws[1].packet.geometry.vertices) {
                v.position[0]=6.15F+(v.position[0]-1.4F)/.75F*5.85F;
                v.position[1]=(v.position[1]-.03F)/.75F*5.85F;
            }
        }
        const auto scenery=[&](float z) {
            starfox::vr::DrawPacket packet;
            constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
            for(auto corner:corners) {
                starfox::vr::SceneVertex v{};v.position[0]=corner[0]*16.F;v.position[1]=corner[1]*16.F;v.position[2]=z;
                v.color[0]=v.color[1]=v.color[2]=v.color[3]=1;std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
                v.uv[0]=float(corner[0]+1)*64;v.uv[1]=float(corner[1]+1)*64;
                v.texture[1]=v.texture[2]=127;v.texture[3]=1;packet.geometry.vertices.push_back(v);
            }
            for(unsigned y=0;y<128;++y) for(unsigned x=0;x<128;++x)
                packet.geometry.texels.push_back((32+x*192/127)|((32+y*192/127)<<8)
                    |((32+((x*3+y*5)%128)*192/127)<<16)|0xff000000U);
            fixture->draws.insert(fixture->draws.begin(),{std::move(packet),CalibratedGameLayer::world,starfox::vr::SceneBlend::opaque,false});
            fixture->draws.front().reflection_environment=true;
        };
        scenery(3);
        if(selection.ground) {
            for(auto& draw:fixture->draws) if(draw.source_key==1)
                for(auto& v:draw.packet.geometry.vertices) v.position[0]*=.07F;
            starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
            auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,false,false,232);
            starfox::vr::place_landscape_ground(floor,-.5F,true);
            CalibratedGroundGradient gradient{{.2F,.3F,.1F},{.4F,.5F,.2F}};
            gradient.material=selection.ground;gradient.seconds=1;
            check(apply_calibrated_ground(floor,gradient),"MSAA reflection fixture lost its native ground");
            fixture->draws.insert(fixture->draws.begin(),{std::move(floor),CalibratedGameLayer::world,
                starfox::vr::SceneBlend::opaque,false,calibrated_ground_source_key});
            fixture->settings.enhanced_ground=true;scenery(-12);
        }
        fixture->settings.aa_type=6;fixture->settings.aa_quality=quality;
        fixture->settings.material=unsigned(selection.model);
        fixture->settings.ray_tracing=fixture->settings.reflections=2;
        const auto source=[&](unsigned frame) {
            auto next=std::make_shared<CalibratedGameFrame>(*fixture);
            auto now=std::make_shared<starfox::vr::GameSceneSnapshot>(*tick);now->revision=frame;
            if(frame>=4) now->transforms[2].generation=2;
            next->previous=next->current=now;next->presentation_seconds=20+frame/120.;
            for(auto& draw:next->draws) {
                if(draw.source_key==1) draw.packet.model[12]=frame*.013F;
                if(draw.source_key==2) draw.packet.model[12]=frame*-.019F;
                if(draw.source_key==calibrated_ground_source_key) {
                    const float bank=frame*.006F;draw.packet.model[0]=draw.packet.model[5]=std::cos(bank);
                    draw.packet.model[1]=std::sin(bank);draw.packet.model[4]=-std::sin(bank);draw.packet.model[13]=frame*-.002F;
                    if(selection.ground==5 || selection.ground==9) {
                        const auto at=std::size_t(draw.packet.geometry.vertex_view().front().texture[0])+calibrated_ground_offset;
                        draw.packet.geometry.texels[at+11]=std::bit_cast<std::uint32_t>(1+frame/120.F);
                    }
                }
            }
            if(frame>=3) {
                const auto a=std::find_if(next->draws.begin(),next->draws.end(),[](const auto& d){return d.source_key==1;});
                const auto b=std::find_if(next->draws.begin(),next->draws.end(),[](const auto& d){return d.source_key==2;});
                if(a!=next->draws.end() && b!=next->draws.end()) std::iter_swap(a,b);
            }
            native_mock.head_x=frame*.27F;native_mock.head_yaw=frame*.004F;
            return next;
        };
        // The final positive colour comparison is AFTER the recycled actor
        // has seeded a fresh generation. Its first recycled frame must refuse
        // old finite hits, not demonstrate reuse of those stale identities.
        constexpr unsigned frames=6;
        struct Record {
            shadows::GpuReflectionOutput layout;std::vector<std::uint32_t> words;
            ReflectionLiquidDiagnosticInputs inputs;std::vector<std::uint32_t> old_vertices;
        };
        using Records=std::array<Record,2>;
        const auto records=[&](unsigned sample) {
            Records result;
            if(!native_msaa_reflection_lava_only) return result;
            for(unsigned eye=0;eye<2;++eye) {
                auto& out=result[eye];out.layout=renderer.accepted_reflection_sample(eye,sample);
                check(out.layout.buffer && out.layout.reflection_history.storage_bytes,"Missing accepted lava diagnostic sample");
                auto* device=static_cast<SDL_GPUDevice*>(out.layout.device);
                const auto bytes=out.layout.reflection_history.storage_bytes;
                SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=bytes;
                auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);check(transfer,SDL_GetError());
                auto* command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
                auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
                const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(out.layout.buffer),0,bytes};
                const SDL_GPUTransferBufferLocation to{transfer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
                auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
                check(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
                const auto* data=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,transfer,false));check(data,SDL_GetError());
                out.words.assign(data,data+bytes/4);SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);
                out.inputs=renderer.reflection_liquid_inputs(eye,sample);
                if(out.inputs.vertices) {
                    const auto vertex_bytes=out.inputs.vertex_count*16;
                    info.size=vertex_bytes;transfer=SDL_CreateGPUTransferBuffer(device,&info);check(transfer,SDL_GetError());
                    command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
                    copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
                    const SDL_GPUBufferRegion verts{static_cast<SDL_GPUBuffer*>(out.inputs.vertices),out.inputs.previous_vertex_offset,vertex_bytes};
                    const SDL_GPUTransferBufferLocation dest{transfer,0};SDL_DownloadFromGPUBuffer(copy,&verts,&dest);SDL_EndGPUCopyPass(copy);
                    fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
                    check(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
                    data=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,transfer,false));check(data,SDL_GetError());
                    out.old_vertices.assign(data,data+vertex_bytes/4);SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);
                }
            }
            return result;
        };
        const auto print_record=[](const Record& record,unsigned pixel,const char* name) {
            const auto& h=record.layout.reflection_history;const auto& words=record.words;
            const auto floats=[&](unsigned offset){for(unsigned c=0;c<4;++c) std::cout<<' '<<std::bit_cast<float>(words[offset/4+pixel*4+c]);};
            std::cout<<name<<" pixel="<<pixel<<" RGBA="<<std::hex<<words[pixel]<<" incoming="<<words[h.incoming_offset/4+pixel]<<std::dec;
            std::cout<<" guide=";floats(h.motion_offset);std::cout<<" ids="<<std::hex;
            for(unsigned c=0;c<4;++c) std::cout<<' '<<words[h.identity_offset/4+pixel*4+c];
            std::cout<<std::dec<<" witness=";floats(h.witness_offset);std::cout<<std::endl;
            const auto primitive=words[h.identity_offset/4+pixel*4+1];
            if(record.old_vertices.size()>=(std::uint64_t(primitive)+1)*12) {
                // 208-byte standalone helper ABI, printed as exact uint bits.
                // Only this explicit diagnostic performs the vertex readback.
                std::cout<<"case-bits "<<std::hex;
                for(float value:record.inputs.frame) std::cout<<std::bit_cast<std::uint32_t>(value)<<' ';
                for(unsigned i=0;i<12;++i) std::cout<<record.old_vertices[primitive*12+i]<<' ';
                for(unsigned i=0;i<4;++i) std::cout<<words[h.witness_offset/4+pixel*4+i]<<' ';
                std::cout<<std::bit_cast<std::uint32_t>(float(pixel%record.layout.width)+.5F)<<' '
                    <<std::bit_cast<std::uint32_t>(float(pixel/record.layout.width)+.5F)<<" 0 0"<<std::dec<<std::endl;
            }
        };
        std::array<std::array<Records,frames>,8> reference_records,actual_records;
        std::array<Images,frames> center_ink;
        native_mock.sample_x=native_mock.sample_y=0;
        for(unsigned frame=0;frame<frames;++frame) {
            auto ink=source(frame);ink->settings.ray_tracing=ink->settings.reflections=ink->settings.aa_quality=0;
            center_ink[frame]=render(ink);
        }
        Images covered,empty;
        if(selection.ground) {
            // Independent CURRENT hardware-raster coverage, not a shader
            // path ID or cached colour, separates model and floor acceptance.
            auto labels=source(frames-1);labels->settings.ray_tracing=labels->settings.reflections=0;
            covered=render(labels);
            auto uncovered=std::make_shared<CalibratedGameFrame>(*labels);
            std::erase_if(uncovered->draws,[](const auto& draw){return draw.source_key==1 || draw.source_key==2;});
            empty=render(uncovered);
        }
        std::array<std::array<Images,frames>,8> references;
        for(unsigned sample=0;sample<count;++sample) {
            native_mock.sample_x=positions[count-2+sample][0];native_mock.sample_y=positions[count-2+sample][1];
            for(unsigned frame=0;frame<frames;++frame) {
                auto single=source(frame);single->settings.aa_quality=0;
                // Each independent XR camera starts a fresh secondary bank.
                single->settings.history_epoch=20000+selection.ground*100+quality*10+sample;
                references[sample][frame]=render(single);
                reference_records[sample][frame]=records(0);
                check(render(single)==references[sample][frame],"Independent held sample reflection drifted");
            }
        }
        native_mock.sample_x=native_mock.sample_y=0;
        unsigned case_stabilized=0,model_stabilized=0,floor_stabilized=0;
        for(unsigned frame=0;frame<frames;++frame) {
            auto multiple=source(frame);multiple->settings.history_epoch=30000+selection.ground*100+quality;
            const auto actual=render(multiple);
            if(native_msaa_reflection_lava_only) for(unsigned sample=0;sample<count;++sample) actual_records[sample][frame]=records(sample);
            check(render(multiple)==actual,"Held MSAA secondary samples retraced/reblended");
            if(frame==2) {
                auto rejected=std::make_shared<CalibratedGameFrame>(*multiple);rejected->draws[0].packet.model[12]+=.2F;
                gpu_mock.fail_end=true;auto result=renderer.submit(rejected);
                for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected native MSAA reflection remained pending");
                (void)read();check(render(multiple)==actual,"Rejected MSAA frame replaced a sample's accepted RGB/pose/path");
                auto resized=std::make_shared<CalibratedGameFrame>(*multiple);resized->settings.aa_quality=quality==3?1:3;
                gpu_mock.fail_end=true;result=renderer.submit(resized);
                for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::failed && !renderer.frame_pending(),"Rejected MSAA count transition remained pending");
                (void)read();check(render(multiple)==actual,"Rejected count transition released/replaced accepted sample banks");
                gpu_mock.timeout_right=true;
                check(renderer.submit(multiple)==DisplayXrSubmission::waiting && renderer.retained_frame()==multiple,
                    "MSAA reflection acquisition wait lost its exact source");
                check(renderer.poll()==DisplayXrSubmission::waiting,"MSAA reflection wait advanced a sample history");
                gpu_mock.timeout_right=false;result=renderer.poll();
                for(unsigned n=0;renderer.frame_pending() && n<10000;++n) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::submitted && rgba(read())==actual,"Held MSAA wait changed accepted sample radiance");
            }
            if(frame==frames-1) {
                auto cut=std::make_shared<CalibratedGameFrame>(*multiple);cut->settings.history_epoch+=100000;
                const auto raw=render(cut);
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
                    const bool actor=!selection.ground
                        || !std::equal(covered[eye].begin()+at,covered[eye].begin()+at+4,empty[eye].begin()+at);
                    for(unsigned c=0;c<3;++c) if(actual[eye][at+c]!=raw[eye][at+c]) {
                        ++case_stabilized;(actor?model_stabilized:floor_stabilized)++;
                    }
                }
            }
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=0;at<actual[eye].size();at+=4) {
                const bool ink=center_ink[frame][eye][at]==255 && center_ink[frame][eye][at+2]==0
                    && (center_ink[frame][eye][at+1]==0 || center_ink[frame][eye][at+1]==255);
                for(unsigned c=0;c<4;++c) {
                    double sum=0;for(unsigned sample=0;sample<count;++sample)
                        sum+=c==3?references[sample][frame][eye][at+c]/255.:decode(references[sample][frame][eye][at+c]/255.);
                    const auto expected=ink?center_ink[frame][eye][at+c]:std::lround((c==3?sum/count:encode(sum/count))*255);
                    // Ordinary native UI is composed on the centre grid;
                    // skip an edge where the sample references see that ink
                    // but the centre does not. Interiors remain byte-exact.
                    bool sample_ink=false;
                    for(unsigned sample=0;sample<count;++sample) {
                        const auto& ref=references[sample][frame][eye];
                        sample_ink|=ref[at]==255 && ref[at+2]==0 && (ref[at+1]==0 || ref[at+1]==255);
                    }
                    if(!sample_ink || ink) {
                        if(std::abs(int(actual[eye][at+c])-int(expected))>(ink?0:2)) {
                            if(native_msaa_reflection_lava_only) for(unsigned sample=0;sample<count;++sample) {
                                std::cout<<"Lava sample "<<sample<<" frame "<<frame<<" eye "<<eye<<std::endl;
                                const auto& actual_record=actual_records[sample][frame][eye];
                                print_record(actual_record,at/4,"actual");print_record(reference_records[sample][frame][eye],at/4,"reference");
                                if(frame) {
                                    const auto& h=actual_record.layout.reflection_history;
                                    const auto gx=std::bit_cast<float>(actual_record.words[h.motion_offset/4+at])-.5F;
                                    const auto gy=std::bit_cast<float>(actual_record.words[h.motion_offset/4+at+1])-.5F;
                                    if(std::isfinite(gx) && std::isfinite(gy) && gx>=0 && gy>=0
                                        && gx<actual_record.layout.width-1 && gy<actual_record.layout.height-1)
                                        for(unsigned y=0;y<2;++y) for(unsigned x=0;x<2;++x) {
                                            const auto tap=(unsigned(gy)+y)*actual_record.layout.width+unsigned(gx)+x;
                                            print_record(actual_records[sample][frame-1][eye],tap,"actual prior tap");
                                            print_record(reference_records[sample][frame-1][eye],tap,"reference prior tap");
                                        }
                                }
                            }
                            if(native_msaa_reflection_water_only || native_msaa_reflection_lava_only) {
                                std::cout<<"Diagnostic cached single samples:";
                                for(unsigned sample=0;sample<count;++sample) std::cout<<' '<<unsigned(references[sample][frame][eye][at+c]);
                                std::cout<<"; raw single samples:";
                                double raw_sum=0;
                                for(unsigned sample=0;sample<count;++sample) {
                                    native_mock.sample_x=positions[count-2+sample][0];native_mock.sample_y=positions[count-2+sample][1];
                                    auto single=source(frame);single->settings.aa_quality=0;single->settings.history_epoch=60000+sample;
                                    const auto raw_single=render(single);std::cout<<' '<<unsigned(raw_single[eye][at+c]);
                                    raw_sum+=c==3?raw_single[eye][at+c]/255.:decode(raw_single[eye][at+c]/255.);
                                }
                                native_mock.sample_x=native_mock.sample_y=0;
                                auto raw_source=source(frame);raw_source->settings.history_epoch=61000;
                                const auto raw_msaa=render(raw_source);
                                std::cout<<"; raw mean="<<std::lround((c==3?raw_sum/count:encode(raw_sum/count))*255)
                                    <<" raw MSAA="<<unsigned(raw_msaa[eye][at+c])<<std::endl;
                            }
                            throw std::runtime_error(std::string(backend)+" MSAA secondary history oracle mismatch ground="
                                +std::to_string(selection.ground)+" quality="+std::to_string(quality)+" frame="+std::to_string(frame)
                                +" eye="+std::to_string(eye)+" byte="+std::to_string(at+c)+" actual="
                                +std::to_string(actual[eye][at+c])+" expected="+std::to_string(expected));
                        }
                        ++checked;if(ink) ++protected_ink;
                    }
                }
            }
            last=multiple;
        }
        check(case_stabilized>0,("Native MSAA reflection history bypassed actual incoming RGB: ground="
            +std::to_string(selection.ground)+" quality="+std::to_string(quality)).c_str());
        if(selection.ground) check(floor_stabilized>0 && ((selection.ground!=6 && selection.ground!=7) || model_stabilized>0),
            ("Native MSAA lacks independent model/floor reflection reuse: ground="+std::to_string(selection.ground)
                +" quality="+std::to_string(quality)+" model/floor="+std::to_string(model_stabilized)+"/"+std::to_string(floor_stabilized)).c_str());
        stabilized+=case_stabilized;
        std::cout<<backend<<" MSAA reflection ground="<<selection.ground<<" samples="<<count
            <<" history-only changed RGB bytes="<<case_stabilized<<" independently classified model/floor="
            <<model_stabilized<<'/'<<floor_stabilized<<std::endl;
    }
    native_mock.head_x=native_mock.head_yaw=native_mock.sample_x=native_mock.sample_y=0;
    check(checked>10000 && protected_ink>50 && stabilized>0,"MSAA reflection oracle lacked colour/coverage acceptance");
    std::cout<<backend<<" MSAA reflection owner: "<<checked<<" independent XR sample oracle bytes, "
        <<protected_ink<<" protected bytes, "<<stabilized<<" history-only changed RGB bytes; "
        <<(native_msaa_reflection_water_only?"isolated 4-sample native water, ":native_msaa_reflection_lava_only?"isolated 2/4/8-sample native lava, ":
            "2/4/8 genuine hardware samples, sharp/rough finite paths, mixed mirror/gold ground and native water/lava, ")
        <<"moving located eyes/banked planes, "
        "held/rejected/reordered/recycled source acceptance passed. Mocked XR, not physical panel or performance acceptance.\n";
}
