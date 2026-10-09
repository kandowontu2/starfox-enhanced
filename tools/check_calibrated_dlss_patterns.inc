// Actual SDK evaluation plus an independently derived current/history phase
// footprint oracle. Mapping, uploads and readback exist only in this tool.
void pattern_fixture(Gpu& gpu,Sdk& sdk,SDL_GPUTextureFormat format,unsigned model,unsigned effect,
    unsigned factor,unsigned samples,unsigned& serial) {
    using namespace render;
    constexpr Fsr1Extent output{320,192};constexpr unsigned invalid=0xffffffffU;
    const unsigned mode=effect==5?1:effect==10?2:effect==25?3:4;
    std::array<GpuCalibratedDlss,2> eyes;
    for(unsigned e=0;e<2;++e)check(eyes[e].initialize(gpu.device,format,sdk.api,100+e,mode,model,output),eyes[e].status().c_str());
    const auto input=eyes[0].input_extent();const unsigned n=input.width*input.height;
    const auto cameras=*vr::sbs_eye_cameras(1.1F,float(output.width)/output.height,.064F,4,.05F,80);
    std::array<std::unique_ptr<Texture>,2> scene,owners,surfaces,motion,ink,panel,destination,coverage;
    std::array<Bytes,2> accepted_images,accepted_phase;
    std::array<std::vector<unsigned>,2> accepted_codes;
    CalibratedPatternGuide accepted_descriptor;bool accepted=false;unsigned accepted_grid=factor;
    unsigned matching=0,crossed=0,mixed=0,phase_checks=0,consumed_rays=0,unused_rays=0;
    for(unsigned e=0;e<2;++e) {
        scene[e]=std::make_unique<Texture>(gpu,input,format);owners[e]=std::make_unique<Texture>(gpu,input,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        surfaces[e]=std::make_unique<Texture>(gpu,input,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
        motion[e]=std::make_unique<Texture>(gpu,input,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
        ink[e]=std::make_unique<Texture>(gpu,output,format);panel[e]=std::make_unique<Texture>(gpu,output,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        destination[e]=std::make_unique<Texture>(gpu,output,format);
        coverage[e]=std::make_unique<Texture>(gpu,Fsr1Extent{input.width*factor,input.height*factor},SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,samples);
        check(!eyes[e].resident_inputs().pattern_phase && !eyes[e].resident_inputs().accepted_pattern_phase,
            "Unselected patterns allocated/exposed phase history");
    }
    for(unsigned frame=0;frame<17;++frame) {
        CalibratedPatternGuide descriptor;descriptor.scale=factor*4;
        descriptor.passes[0]={effect,effect,35,100};
        if(frame==4 || frame==5) descriptor.passes[0].world_intensity=100;
        if(frame==6) {descriptor.passes[1]={70,25,100,35};descriptor.passes[2]={10,5,35,100};}
        if(frame==7) descriptor.scale=factor*2;
        if(frame==11 || frame==12) {descriptor.passes[0].model=0;descriptor.passes[0].model_intensity=0;}
        if(frame==13 || frame==14) {descriptor.passes[0].world=0;descriptor.passes[0].world_intensity=0;}
        if(frame>=15) descriptor={};
        const bool active=frame<15;
        // Two rejected descriptors (one deliberately dropped presentation),
        // must not replace the accepted keys/images. Both SDK attempts still
        // consume tokens, and the next real evaluation must reset.
        const bool cancel=frame==4,submitted_reject=frame==6;
        const unsigned grid=frame==8 && samples==1?1:factor;
        const bool changed=accepted && (descriptor!=accepted_descriptor || grid!=accepted_grid);
        const bool reset=!accepted || changed || frame==5 || frame==7;
        const bool history=accepted && !reset && active;
        std::array<Bytes,2> colors,tags,art,panel_tags,coverage_tags;
        std::array<std::vector<std::array<float,4>>,2> depths,vectors;
        std::array<std::vector<unsigned>,2> expected_codes;
        std::array<std::vector<bool>,2> expected_valid;
        std::array<std::vector<bool>,2> expected_patterned;
        std::array<std::vector<std::unique_ptr<Buffer>>,2> water_storage;
        std::array<std::vector<shadows::GpuReflectionOutput>,2> water;
        std::array<std::vector<bool>,2> ray_hits,wet;
        std::array<std::array<unsigned,2>,3> effects;
        for(unsigned p=0;p<3;++p)effects[p]={descriptor.passes[p].world,descriptor.passes[p].model};
        ++serial;gpu.begin();
        for(unsigned e=0;e<2;++e) {
            colors[e].resize(n*4);tags[e].resize(n*4);depths[e].resize(n);vectors[e].resize(n);
            coverage_tags[e].resize(n*factor*factor*4);
            expected_codes[e].resize(n);expected_valid[e].resize(n);expected_patterned[e].resize(n);
            ray_hits[e].resize(n);wet[e].resize(n);
            art[e].resize(output.width*output.height*4);panel_tags[e].resize(art[e].size());
            for(unsigned p=0;p<output.width*output.height;++p) {
                for(unsigned k=0;k<4;++k)art[e][p*4+k]=static_cast<unsigned char>(p*(17+k*11)+e*23+k*43);
                panel_tags[e][p*4+2]=p%7<2?0:e+1;
            }
            for(unsigned p=0;p<n;++p) {
                const unsigned x=p%input.width,y=p/input.width,layer=samples>1?e+1:(x<input.width/2?1:2);
                const bool protected_pixel=p%43==0,missing=p%41==0,untracked=p%47==0;
                const unsigned actual_layer=protected_pixel?0:layer;
                ray_hits[e][p]=(frame==2 || frame==3) && p%37==0 && !protected_pixel;
                wet[e][p]=false;
                tags[e][p*4]=tags[e][p*4+3]=255;tags[e][p*4+2]=actual_layer;
                depths[e][p]={0,0,-1,missing?0.F:512.F};
                // Includes both jitter offsets, deliberately unlike the final
                // SDK vector. Cases include zero-weight neighbouring taps,
                // positive fractional neighbours, phase-period displacement,
                // boundaries, and invalid correspondence.
                const float dx=p%5==0?0:p%5==1?1:p%5==2?.25F:float(descriptor.scale*12)/grid*(p%5==4?-1:1);
                const float dy=p%5==3?.375F:0;
                vectors[e][p]={dx,dy,512.F,untracked?0.F:1.F};
                bool found=false,coherent=true,patterned=pattern_oracle_phase(x*grid,y*grid,descriptor.scale,actual_layer,effects)!=0;unsigned code=0;
                for(unsigned yy=0;yy<grid;++yy)for(unsigned xx=0;xx<grid;++xx)for(unsigned sample=0;sample<samples;++sample) {
                    // A last contributing SSAA corner changes receiver; class
                    // zero SSAA taps are omitted, not averaged into the proof.
                    const unsigned tag=samples>1?layer:grid!=factor?actual_layer:(p%29==0 && xx+1==grid && yy+1==grid?3-layer
                        :p%31==0 && xx==0 && yy==0?0:actual_layer);
                    const unsigned at=((y*factor+yy)*input.width*factor+x*factor+xx)*4;
                    coverage_tags[e][at]=tag==1 || tag==2?255:0;
                    coverage_tags[e][at+2]=tag;coverage_tags[e][at+3]=255;
                    // The ray exists only on the last actual source corner
                    // and sample. Keep rays on omitted class-zero coverage in
                    // the input, but do not let them replace primary depth or
                    // make the resolved RGB secondary. This oracle derives
                    // consumption from authored coverage, not the shader helper.
                    if(ray_hits[e][p] && sample+1==samples && xx+1==grid && yy+1==grid
                        && (tag==1 || tag==2))wet[e][p]=true;
                    if(samples==1 && tag==0)continue;
                    const unsigned tap=pattern_oracle_phase(x*grid+xx,y*grid+yy,descriptor.scale,tag,effects);
                    patterned=patterned || tap!=0;
                    coherent=coherent && tag==actual_layer && (tag==1 || tag==2) && (!found || code==tap);
                    if(!found)code=tap;found=true;
                }
                consumed_rays+=wet[e][p];unused_rays+=ray_hits[e][p] && !wet[e][p];
                const unsigned witness=!protected_pixel && (!missing || wet[e][p]) && !wet[e][p] && found && coherent?code:invalid;
                expected_codes[e][p]=witness;
                bool matches=history && witness!=invalid && !untracked;
                const double old_x=x+double(dx),old_y=y+double(dy);
                if(matches && (old_x<0 || old_y<0 || old_x>input.width-1 || old_y>input.height-1)) matches=false;
                if(matches) {
                    const unsigned bx=unsigned(std::floor(old_x)),by=unsigned(std::floor(old_y));
                    const double fx=old_x-bx,fy=old_y-by;
                    for(unsigned yy=0;yy<2;++yy)for(unsigned xx=0;xx<2;++xx) {
                        const double share=(xx?fx:1-fx)*(yy?fy:1-fy);
                        const unsigned q=std::min(by+yy,input.height-1)*input.width+std::min(bx+xx,input.width-1);
                        if(share>0 && accepted_codes[e][q]!=code)matches=false;
                    }
                }
                expected_valid[e][p]=patterned || history?matches:!protected_pixel && !missing && !untracked && !wet[e][p];
                expected_patterned[e][p]=patterned;
                matching+=matches;crossed+=history && witness!=invalid && !untracked && !matches;
                mixed+=!coherent;
                // A screen-patterned input of changing radiance (not a flat
                // colour token) exercises actual neural reconstruction too.
                const unsigned phase=pattern_oracle_phase(x*grid,y*grid,descriptor.scale,layer,effects);
                colors[e][p*4]=30+(x*3+phase*11+frame*7+e*19)%190;
                colors[e][p*4+1]=24+(y*2+phase*5+frame*13)%190;
                colors[e][p*4+2]=20+(x+y+phase*3+e*31)%180;colors[e][p*4+3]=255;
            }
            scene[e]->upload(colors[e]);owners[e]->upload(tags[e]);surfaces[e]->upload(floats(depths[e]));motion[e]->upload(floats(vectors[e]));
            ink[e]->upload(art[e]);panel[e]->upload(panel_tags[e]);
            if(samples>1)coverage[e]->clear_ownership(e+1,255);
            else if(grid==factor)coverage[e]->upload(coverage_tags[e]);
            else {
                // The layout transition uses the native ownership image, not
                // an oversized grid silently rebound with a different stride.
                coverage_tags[e]=tags[e];
            }
            if(frame==2 || frame==3) {
                const unsigned w=input.width*grid,h=input.height*grid;
                const auto layout=*shadows::native_water_layers(w,h,true);
                for(unsigned sample=0;sample<samples;++sample) {
                    Bytes bytes(layout.storage_bytes,0);
                    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
                        const unsigned p=(y/grid)*input.width+x/grid,index=y*w+x;
                        if(!ray_hits[e][p] || sample+1!=samples || x%grid+1!=grid || y%grid+1!=grid)continue;
                        bytes[index*4+3]=253;
                        const std::array<float,4> hit{0,-.8F,.6F,300};
                        std::memcpy(bytes.data()+layout.surface_offset+index*16,hit.data(),16);
                    }
                    water_storage[e].push_back(std::make_unique<Buffer>(gpu,bytes));
                    water[e].push_back({gpu.device,water_storage[e].back()->value,w,h,w*4,layout});
                }
            }
            auto f=*calibrated_dlss_frame(cameras[e],cameras[e],{.23F,-.31F},false);
            f.frame_index=serial;f.width=input.width;f.height=input.height;f.output_width=output.width;f.output_height=output.height;
            const CalibratedDlssCoverage footprint{grid==factor?coverage[e]->value:owners[e]->value,grid,samples};
            if(frame==0) {
                for(unsigned bad=0;bad<4;++bad) {
                    auto malformed=descriptor;
                    if(bad==0)malformed.scale=0;if(bad==1)malformed.passes[0].model=6;
                    if(bad==2)malformed.passes[0].world_intensity=101;if(bad==3)malformed.passes[1].model_intensity=35;
                    const auto before=evaluations;
                    check(!eyes[e].enqueue(gpu.command,scene[e]->value,owners[e]->value,surfaces[e]->value,motion[e]->value,
                        ink[e]->value,panel[e]->value,destination[e]->value,f,{-.11F,.07F},{},0,footprint,malformed),
                        "Malformed SDK pattern descriptor accepted");
                    check(evaluations==before,"Malformed pattern descriptor reached real SDK evaluation");
                }
            }
            check(eyes[e].enqueue(gpu.command,scene[e]->value,owners[e]->value,surfaces[e]->value,motion[e]->value,
                ink[e]->value,panel[e]->value,destination[e]->value,f,{-.11F,.07F},water[e],0,footprint,descriptor),eyes[e].status().c_str());
            check(recorded.back().viewport==100+e && recorded.back().frame_index==serial,
                "Pattern guide evaluated a shared/mono viewport or reused an attempted frame token");
            check(bool(recorded.back().reset)==reset,"SDK pattern/grid/attempt history reset mismatch");
        }
        // Even a dropped presentation must drain commands once the SDK has
        // advanced its private native state. Neither case commits app banks.
        for(auto& eye:eyes)check(eye.has_recorded_sdk_work(),"SDK pattern work witness missing");
        gpu.submit();sdk.finish_frame();
        if(cancel || submitted_reject) {
            for(unsigned e=0;e<2;++e) {
                eyes[e].discard();auto inputs=eyes[e].resident_inputs();
                check(gpu.read(static_cast<SDL_GPUTexture*>(inputs.accepted_pattern_phase),input.width,input.height,4)==accepted_phase[e],
                    "Rejected candidate overwrote accepted pattern metadata");
            }
        } else {
            for(unsigned e=0;e<2;++e) {
                const auto inputs=eyes[e].resident_inputs();
                check(bool(inputs.pattern_phase)==active,"Pattern-OFF did not retain the ordinary no-phase path");
                const auto codes=active?gpu.read(static_cast<SDL_GPUTexture*>(inputs.pattern_phase),input.width,input.height,4):Bytes{};
                const auto bias=gpu.read(static_cast<SDL_GPUTexture*>(inputs.current_color_bias),input.width,input.height,1);
                const auto vectors_out=gpu.read(static_cast<SDL_GPUTexture*>(inputs.motion),input.width,input.height,8);
                const auto depth_out=gpu.read(static_cast<SDL_GPUTexture*>(inputs.depth),input.width,input.height,4);
                const auto color_out=gpu.read(static_cast<SDL_GPUTexture*>(inputs.color),input.width,input.height,4);
                for(unsigned p=0;p<n;++p) {
                    ++guide_pixels;
                    const unsigned layer=tags[e][p*4+2];
                    if(active) {check(scalar(codes,p)==(expected_codes[e][p]==invalid?-1.F:float(expected_codes[e][p])),
                        "Current SDK AA pattern phase differs from independent footprint oracle");++phase_checks;}
                    // Centre-protected MSAA cells can still have contributing
                    // patterned coverage; their guide is rejected, while final
                    // presentation restores their untouched protected artwork.
                    const bool valid=expected_valid[e][p];
                    const bool visible=layer!=0 && (depths[e][p][3]>0 || wet[e][p]);
                    const unsigned expected_bias=!valid && (visible || layer==2 || expected_patterned[e][p])?255:0;
                    if(bias[p]!=expected_bias)throw std::runtime_error("SDK pattern bias mismatch frame="+std::to_string(frame)
                        +" eye="+std::to_string(e)+" pixel="+std::to_string(p%input.width)+","+std::to_string(p/input.width)
                        +" layer="+std::to_string(layer)+" factor="+std::to_string(grid)+" samples="+std::to_string(samples)
                        +" phase="+std::to_string(expected_codes[e][p])+" reset="+std::to_string(reset)
                        +" valid="+std::to_string(valid)+" patterned="+std::to_string(expected_patterned[e][p])
                        +" actual="+std::to_string(bias[p])+" expected="+std::to_string(expected_bias));
                    ++checks;
                    const double z=wet[e][p]?300:depths[e][p][3];
                    const double expected_depth=layer!=0 && z>0?-double(cameras[e].projection[10])+double(cameras[e].projection[14])*256/z:1;
                    if(std::abs(scalar(depth_out,p)-expected_depth)>=2.e-6)
                        throw std::runtime_error("Pattern liquid/rigid depth mismatch frame="+std::to_string(frame)
                            +" eye="+std::to_string(e)+" pixel="+std::to_string(p%input.width)+","+std::to_string(p/input.width)
                            +" layer="+std::to_string(layer)+" factor="+std::to_string(grid)+" samples="+std::to_string(samples)
                            +" wet="+std::to_string(wet[e][p])+" primary_z="+std::to_string(depths[e][p][3])
                            +" actual="+std::to_string(scalar(depth_out,p))+" expected="+std::to_string(expected_depth));
                    ++checks;
                    for(unsigned k=0;k<2;++k) {
                        const float expected=valid?vectors[e][p][k]+(k?-.38F:.34F):-std::numeric_limits<float>::max();
                        check(valid?std::abs(scalar(vectors_out,p*2+k)-expected)<2.e-5:scalar(vectors_out,p*2+k)==expected,
                            "Pattern witness erased genuine motion or reused a different screen phase");
                    }
                    for(unsigned k=0;k<4;++k)check(color_out[p*4+k]==colors[e][p*4+k],"Pattern witness changed RGB/pass order/opacity before SDK");
                }
                accepted_images[e]=destination[e]->read();
                for(unsigned p=0;p<output.width*output.height;++p)for(unsigned k=0;k<4;++k) {
                    const bool protected_pixel=panel_tags[e][p*4+2]==0 || art[e][p*4+3]==0;
                    if(protected_pixel || k==3) {
                        check(accepted_images[e][p*4+k]==art[e][p*4+k],"SDK pattern reconstruction changed protected ink/opacity");++protected_bytes;
                    } else world_changes+=accepted_images[e][p*4+k]!=art[e][p*4+k];
                }
                eyes[e].commit();accepted_phase[e]=codes;accepted_codes[e]=expected_codes[e];
                check(bool(eyes[e].resident_inputs().accepted_pattern_phase)==active,"SDK pattern-OFF committed a patterned history key");
            }
            accepted_descriptor=descriptor;accepted_grid=grid;accepted=true;
            check(accepted_images[0]!=accepted_images[1],"Pattern SDK collapsed independent eyes");
        }
        const auto calls=evaluations;gpu.begin();
        for(unsigned e=0;e<2;++e)check(eyes[e].enqueue_accepted(gpu.command,ink[e]->value,panel[e]->value,destination[e]->value),eyes[e].status().c_str());
        gpu.submit();sdk.finish_frame();
        // The current full-panel protected ink may change independently of a
        // rejected candidate; here it is held, so the entire pair is exact.
        for(unsigned e=0;e<2;++e){check(destination[e]->read()==accepted_images[e],"Held/rejected pattern SDK eye drifted");++held;}
        check(evaluations==calls,"Held pattern image invoked SDK evaluation");
    }
    check(matching && crossed && (factor==1 || mixed),"Pattern oracle did not exercise positive reuse/cross-phase/AA mixing");
    check(consumed_rays && (factor!=1 || samples!=1 || unused_rays),
        "Pattern liquid oracle missed visible rays or omitted-coverage negative controls");
    for(auto& eye:eyes)check(eye.retire(),eye.status().c_str());
    std::cout<<"pattern format="<<unsigned(format)<<" model="<<model<<" effect="<<effect<<" mode="<<mode
        <<" footprint="<<factor<<" samples="<<samples<<" phases="<<phase_checks<<" matches="<<matching
        <<" rejected="<<crossed<<" mixed="<<mixed<<" consumed_rays="<<consumed_rays<<" unused_rays="<<unused_rays
        <<" real SDK/oracle/accepted metadata/held images passed"<<std::endl;
}
