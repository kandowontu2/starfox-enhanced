// Fixture-only downloads. Production transports all sample planes on the GPU.
// Independent XR/FOV coverage and eye-ray/plane oracle, not renderer helpers.
struct NativeMsaaGuideTargets {
    SDL_GPUDevice* device;unsigned count;
    SDL_GPUTexture *color{},*receiver{},*surface{},*depth{},*resolved{};
    std::array<SDL_GPUTexture*,8> colors{},receivers{},surfaces{},enhanced{};
    SDL_GPUTransferBuffer* read{};
    NativeMsaaGuideTargets(Gpu& gpu,unsigned samples):device(gpu.device),count(samples) {
        try {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
            info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
            info.sample_count=samples==2?SDL_GPU_SAMPLECOUNT_2:samples==4?SDL_GPU_SAMPLECOUNT_4:SDL_GPU_SAMPLECOUNT_8;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            const auto props=SDL_CreateProperties();check(props,"Could not create MSAA fixture properties");
            check(SDL_SetBooleanProperty(props,STARFOX_SDL_MULTISAMPLE_READ,true),SDL_GetError());info.props=props;
            info.format=gpu.format;color=SDL_CreateGPUTexture(device,&info);
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;receiver=SDL_CreateGPUTexture(device,&info);
            info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;surface=SDL_CreateGPUTexture(device,&info);
            SDL_DestroyProperties(props);info.props=0;
            info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            depth=SDL_CreateGPUTexture(device,&info);check(color && receiver && surface && depth,SDL_GetError());
            info.sample_count=SDL_GPU_SAMPLECOUNT_1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.format=gpu.format;resolved=SDL_CreateGPUTexture(device,&info);check(resolved,SDL_GetError());
            for(unsigned i=0;i<count;++i) {
                info.format=gpu.format;colors[i]=SDL_CreateGPUTexture(device,&info);
                enhanced[i]=SDL_CreateGPUTexture(device,&info);
                info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;receivers[i]=SDL_CreateGPUTexture(device,&info);
                info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;surfaces[i]=SDL_CreateGPUTexture(device,&info);
                check(colors[i] && receivers[i] && surfaces[i] && enhanced[i],SDL_GetError());
            }
            const SDL_GPUTransferBufferCreateInfo download{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*(count*24+8),0};
            read=SDL_CreateGPUTransferBuffer(device,&download);check(read,SDL_GetError());
        } catch(...) {release();throw;}
    }
    void release() {
        SDL_WaitForGPUIdle(device);
        for(auto* t:{color,receiver,surface,depth,resolved}) if(t) SDL_ReleaseGPUTexture(device,t);
        for(auto& group:{colors,receivers,surfaces,enhanced}) for(auto* t:group) if(t) SDL_ReleaseGPUTexture(device,t);
        if(read) SDL_ReleaseGPUTransferBuffer(device,read);
    }
    ~NativeMsaaGuideTargets(){release();}
};
std::vector<unsigned char> native_msaa_guides_render(Gpu& gpu,render::GpuCalibratedScene& scene,
    render::GpuCalibratedMsaa& transport,NativeMsaaGuideTargets& targets,
    std::span<const render::CalibratedSceneDraw> draws,const vr::EyeCamera& camera,bool surfaces=true,bool ordered=false,
    render::GpuCalibratedDepth* depth_effect=nullptr,bool native_ink=false) {
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    const auto scene_check=[&](bool ok){check(ok,scene.status().c_str());};
    const auto transport_check=[&](bool ok){check(ok,transport.status().c_str());};
    scene_check(scene.upload(gpu.command,draws));const auto cost=scene.upload_cost();
    const render::CalibratedSceneMultisample msaa{targets.count,gpu.color,true};
    scene_check(scene.enqueue_eye(gpu.command,targets.color,targets.depth,width,height,camera,{0,0,0,1},
        render::CalibratedScenePhase::before_rays,targets.receiver,0,surfaces?targets.surface:nullptr,true,msaa));
    if(ordered) {
        const auto token=scene.upload_token();(void)gpu.finish();check(scene.notify_submitted(token),"Ordered MSAA guides source not confirmed");
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        scene_check(scene.enqueue_eye(gpu.command,targets.color,targets.depth,width,height,camera,{},
            render::CalibratedScenePhase::after_rays,targets.receiver,token,surfaces?targets.surface:nullptr,true,msaa));
    }
    std::array<void*,8> planes{};
    for(unsigned sample=0;sample<targets.count;++sample) {
        transport_check(transport.enqueue_sample(gpu.command,targets.color,targets.receiver,surfaces?targets.surface:nullptr,
            targets.colors[sample],targets.receivers[sample],surfaces?targets.surfaces[sample]:nullptr,width,height,targets.count,sample));
        planes[sample]=targets.colors[sample];
        if(depth_effect) {
            const auto consumer=render::calibrated_msaa_sample_camera(camera,width,height,targets.count,sample);check(bool(consumer),"Invalid MSAA depth-consumer camera");
            const bool enhanced=depth_effect->enqueue(gpu.command,targets.colors[sample],targets.receivers[sample],targets.surfaces[sample],
                targets.enhanced[sample],width,height,1,15,*consumer,230);
            check(enhanced,depth_effect->status().c_str());planes[sample]=targets.enhanced[sample];
        }
    }
    transport_check(transport.enqueue_resolve(gpu.command,std::span(planes).first(targets.count),targets.resolved,width,height,
        native_ink?targets.colors[0]:nullptr,native_ink?targets.receivers[0]:nullptr));
    check(scene.upload_cost()==cost,"MSAA sample extraction reuploaded native geometry");
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const auto download=[&](SDL_GPUTexture* texture,unsigned offset) {
        const SDL_GPUTextureRegion from{texture,0,0,0,0,0,width,height,1};
        const SDL_GPUTextureTransferInfo to{targets.read,offset,width,height};SDL_DownloadFromGPUTexture(copy,&from,&to);
    };
    constexpr unsigned bytes=width*height;
    for(unsigned sample=0;sample<targets.count;++sample) {
        download(depth_effect?targets.enhanced[sample]:targets.colors[sample],sample*bytes*24);
        download(targets.receivers[sample],sample*bytes*24+bytes*4);
        if(surfaces) download(targets.surfaces[sample],sample*bytes*24+bytes*8);
    }
    download(gpu.color,targets.count*bytes*24);download(targets.resolved,targets.count*bytes*24+bytes*4);
    SDL_EndGPUCopyPass(copy);const auto token=scene.upload_token();(void)gpu.finish();
    check(scene.notify_submitted(token),"MSAA guide upload confirmation failed");
    const auto* mapped=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,targets.read,false));check(mapped,SDL_GetError());
    std::vector<unsigned char> result(mapped,mapped+bytes*(targets.count*24+8));SDL_UnmapGPUTransferBuffer(gpu.device,targets.read);
    if(gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB) {
        for(unsigned sample=0;sample<targets.count+2;++sample) {
            const auto offset=sample<targets.count?sample*bytes*24:targets.count*bytes*24+(sample-targets.count)*bytes*4;
            for(unsigned pixel=0;pixel<bytes;++pixel) std::swap(result[offset+pixel*4],result[offset+pixel*4+2]);
        }
    }
    return result;
}
void native_msaa_guide_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    check(SDL_GetBooleanProperty(SDL_GetGPUDeviceProperties(gpu.device),STARFOX_SDL_MULTISAMPLE_READ,false),
        "Pinned desktop MSAA shader-read capability not published");
    render::GpuCalibratedMsaa transport;check(transport.initialize(gpu.device,gpu.format),transport.status().c_str());
    render::GpuCalibratedDepth depth_effect;const bool ready=depth_effect.initialize(gpu.device,gpu.format);check(ready,depth_effect.status().c_str());
    auto world=quad(-.05F,.02F,-3.2F,.85F,{0,1,0,1});
    auto wall=quad(0,0,-2.3F,.54F,{1,0,0,1});
    for(auto& v:wall) v.position[2]+=.18F*v.position[0]+.11F*v.position[1];
    auto cutout=quad(.18F,-.16F,-1.8F,.23F,{1,1,1,1});
    const std::array<std::uint32_t,16> texels{0U,0xff00ffffU,0U,0xff00ffffU,
        0xff00ffffU,0U,0xff00ffffU,0U,0U,0xff00ffffU,0U,0xff00ffffU,0xff00ffffU,0U,0xff00ffffU,0U};
    for(auto& v:cutout) {v.texture[1]=v.texture[2]=3;v.texture[3]=1|4;
        v.uv[0]=(v.position[0]+.05F)*4/.46F;v.uv[1]=(.07F-v.position[1])*4/.46F;}
    auto protected_ink=quad(-.29F,.14F,-1.4F,.11F,{0,0,1,1});
    auto alpha=quad(.32F,.25F,-1.3F,.09F,{1,1,1,.5F});
    std::array<render::CalibratedSceneDraw,5> draws{{{world},{wall},{cutout,texels},{protected_ink},{alpha}}};
    draws[0].effect_layer=1;draws[1].effect_layer=draws[2].effect_layer=2;
    draws[1].ray_caster=draws[2].ray_caster=true;draws[1].reflective_material=draws[2].reflective_material=true;
    draws[3].preserve_native_colour=true;draws[4].blend=vr::SceneBlend::alpha;
    // Hard-coded standard raster positions, independent of the renderer table.
    constexpr std::array<std::array<int,2>,14> locations{{{4,4},{-4,-4},{-2,-6},{6,-2},{-6,2},{2,6},
        {1,-3},{-1,3},{5,1},{-3,-5},{-5,5},{-7,-1},{3,7},{7,-7}}};
    const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    const auto decode=[&](double value){return !srgb?value:value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);};
    const auto encode=[&](double value){return int(std::lround((!srgb?value:value<=.0031308?12.92*value:1.055*std::pow(value,1./2.4)-.055)*255));};
    const auto edge=[](const auto& a,const auto& b,const auto& p){return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);};
    unsigned checked=0,depth_checks=0,holes=0,edge_pixels=0,qualities=0,effect_checks=0,effect_changes=0;
    for(unsigned count:{2U,4U,8U}) {
        if(!scene.supports_multisample(count,true,true)) {std::cout<<"Native MSAA guides "<<count<<"x unsupported; no substitute.\n";continue;}
        ++qualities;NativeMsaaGuideTargets targets(gpu,count);std::vector<unsigned char> previous;
        for(unsigned eye_index=0;eye_index<2;++eye_index) {
            XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.pose.position={eye_index?.039F:-.032F,.017F,.028F};
            eye.fov={-.61F+eye_index*.023F,.72F,.56F,-.49F};const auto camera=camera_for(eye,d3d);
            for(unsigned sample=0;sample<count;++sample) {
                const auto consumer=render::calibrated_msaa_sample_camera(camera,width,height,count,sample);
                check(bool(consumer) && consumer->view==camera.view && consumer->effects==camera.effects,"MSAA consumer camera changed eye pose/effects");
                const auto offset=locations[count-2+sample];const auto& p=consumer->projection;
                for(unsigned x:{0U,127U,255U}) for(unsigned y:{0U,93U,191U}) {
                    const double rx=(2*(x+.5)/width-1+p[8])/p[0],ry=(1-2*(y+.5)/height+p[9])/p[5];
                    const double expected_x=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5+offset[0]/16.)/width);
                    const double expected_y=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5+offset[1]/16.)/height);
                    check(std::abs(rx-expected_x)<2.e-7 && std::abs(ry-expected_y)<2.e-7,"MSAA ray/depth consumer centres do not match hardware sample rays");
                }
            }
            auto invalid=camera;invalid.projection[4]=.2F;
            check(!render::calibrated_msaa_sample_camera(invalid,width,height,count,0)
                && !render::calibrated_msaa_sample_camera(camera,0,height,count,0)
                && !render::calibrated_msaa_sample_camera(camera,width,height,3,0)
                && !render::calibrated_msaa_sample_camera(camera,width,height,count,count),"Invalid MSAA consumer camera accepted");
            const auto baseline=render(gpu,scene,draws,camera,{0,0,0,1});
            const auto pixels=native_msaa_guides_render(gpu,scene,transport,targets,draws,camera);
            check(native_msaa_guides_render(gpu,scene,transport,targets,draws,camera)==pixels,"Held MSAA sample guides changed on retry");
            const auto receiver_only=native_msaa_guides_render(gpu,scene,transport,targets,draws,camera,false);
            for(unsigned sample=0;sample<count;++sample) {
                const unsigned offset=sample*width*height*24;
                check(std::equal(pixels.begin()+offset,pixels.begin()+offset+width*height*8,receiver_only.begin()+offset),
                    "Receiver-only MSAA extraction differs from surface MRT source/ownership");
            }
            draws[3].after_rays=true;
            const auto ordered=native_msaa_guides_render(gpu,scene,transport,targets,draws,camera,true,true);draws[3].after_rays=false;
            check(ordered==pixels,"Ordered MSAA overlays did not retain exact color/ownership/surface samples");
            const auto enhanced=native_msaa_guides_render(gpu,scene,transport,targets,draws,camera,true,false,&depth_effect);
            const auto ink_resolved=native_msaa_guides_render(gpu,scene,transport,targets,draws,camera,true,false,&depth_effect,true);
            constexpr unsigned image_size=width*height;
            for(unsigned sample=0;sample<count;++sample) {
                const auto consumer=render::calibrated_msaa_sample_camera(camera,width,height,count,sample);const auto& p=consumer->projection;
                render::DepthEnhancements settings;settings.modes=15;settings.focal_y=height*p[5]/2;
                settings.camera={width*(1-p[8])/2-.5F,height*(1+p[9])/2-.5F,width*p[0]/2,230};
                const unsigned offset=sample*image_size*24;
                check(std::equal(pixels.begin()+offset+image_size*4,pixels.begin()+offset+image_size*24,enhanced.begin()+offset+image_size*4),
                    "MSAA depth enhancement changed sample ownership/surfaces");
                const auto surface=[&](float sx,float sy,unsigned field) {
                    const int x=int(sx),y=int(sy);if(x<0 || y<0 || x>=int(width) || y>=int(height)) return 0.F;
                    const unsigned pixel=unsigned(y)*width+unsigned(x);float value;
                    std::memcpy(&value,&pixels[offset+image_size*8+pixel*16+field*4],4);
                    return pixels[offset+image_size*4+pixel*4+2]?value:0.F;
                };
                for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                    const unsigned pixel=y*width+x,at=offset+pixel*4;
                    check(enhanced[at+3]==pixels[at+3],"MSAA depth effect changed sample opacity");
                    if(!pixels[offset+image_size*4+pixel*4+2] || surface(float(x),float(y),3)<=0) {
                        check(std::equal(pixels.begin()+at,pixels.begin()+at+4,enhanced.begin()+at),"MSAA depth effect changed protected sample ink");continue;
                    }
                    const float ambient=render::depth_ambient(surface,float(x),float(y),1,settings);
                    for(unsigned c=0;c<3;++c) {
                        const auto colour=[&](float sx,float sy) {
                            sx=std::clamp(sx,0.F,float(width-1));sy=std::clamp(sy,0.F,float(height-1));
                            const unsigned ax=unsigned(sx),ay=unsigned(sy);const float tx=sx-ax,ty=sy-ay;float value=0;
                            for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                                unsigned n=std::min(ay+dy,height-1)*width+std::min(ax+dx,width-1);
                                if(!pixels[offset+image_size*4+n*4+2]) n=pixel;
                                value+=pixels[offset+n*4+c]/255.F*(dx?tx:1-tx)*(dy?ty:1-ty);
                            }
                            return value;
                        };
                        const int expected=int(render::depth_channel(colour,surface,float(x),float(y),1,settings,ambient)*255.F+.5F);
                        check(std::abs(int(enhanced[at+c])-expected)<=1,"Per-sample native AO/DOF disagrees with sample-camera CPU oracle");++effect_checks;
                    }
                    effect_changes+=!std::equal(pixels.begin()+at,pixels.begin()+at+3,enhanced.begin()+at);
                }
            }
            for(unsigned pixel=0;pixel<image_size;++pixel) for(unsigned c=0;c<3;++c) {
                double average=0;for(unsigned sample=0;sample<count;++sample) average+=decode(enhanced[sample*image_size*24+pixel*4+c]/255.)/count;
                check(std::abs(int(enhanced[count*image_size*24+image_size*4+pixel*4+c])-encode(average))<=1,
                    "Enhanced MSAA resolve did not combine finished AO/DOF sample planes in linear light");
                const unsigned at=count*image_size*24+image_size*4+pixel*4+c;
                const bool ink=pixels[image_size*4+pixel*4+2]==0;
                const int expected=ink?pixels[pixel*4+c]:enhanced[at];
                check(std::abs(int(ink_resolved[at])-expected)<=1,"Native MSAA final resolve blurred protected ink or copied unenhanced scenery");
            }
            if(eye_index) check(pixels!=previous,"MSAA guides collapsed independent eye positions");previous=pixels;
            struct Face {std::array<std::array<double,2>,3> xy;std::array<std::array<double,3>,3> xyz;
                std::array<std::array<double,2>,3> uv;unsigned draw;};
            std::vector<Face> faces;
            for(unsigned draw=0;draw<draws.size();++draw) for(unsigned v=0;v<draws[draw].vertices.size();v+=3) {
                Face f{};f.draw=draw;
                for(unsigned n=0;n<3;++n) {
                    const auto& vertex=draws[draw].vertices[v+n];const auto point=model_point(draws[draw].model,vertex);
                    f.xy[n]=reference(eye,point);for(auto& axis:f.xy[n]) axis=std::round(axis*256)/256;
                    f.xyz[n]=world_eye(eye,point);f.uv[n]={vertex.uv[0],vertex.uv[1]};
                }
                faces.push_back(f);
            }
            constexpr unsigned size=width*height;
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const unsigned pixel=y*width+x;unsigned first_owner=9;bool partial=false;
                std::array<double,3> average{};
                for(unsigned sample=0;sample<count;++sample) {
                    const auto offset=locations[count-2+sample];const std::array p{x+.5+offset[0]/16.,y+.5+offset[1]/16.};
                    const std::array<double,3> ray{std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),p[0]/width),
                        std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),p[1]/height),-1};
                    unsigned owner=9;double forward=std::numeric_limits<double>::infinity();std::array<double,3> normal{},color{};
                    bool ambiguous=false,cutout_hole=false;
                    for(const auto& face:faces) {
                        const double area=edge(face.xy[0],face.xy[1],face.xy[2]);bool covered=true;
                        for(unsigned n=0;n<3;++n) {
                            const auto& a=face.xy[n];const auto& b=face.xy[(n+1)%3];const double value=edge(a,b,p);
                            ambiguous|=std::abs(value)/std::hypot(b[0]-a[0],b[1]-a[1])<1.e-7;covered&=area>0?value>0:value<0;
                        }
                        if(!covered) continue;
                        std::array<double,3> a,b,n;
                        for(unsigned c=0;c<3;++c) {a[c]=face.xyz[1][c]-face.xyz[0][c];b[c]=face.xyz[2][c]-face.xyz[0][c];}
                        n={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
                        double plane=0,denominator=0,norm=0;
                        for(unsigned c=0;c<3;++c) {plane+=n[c]*face.xyz[0][c];denominator+=n[c]*ray[c];norm+=n[c]*n[c];}
                        const double distance=plane/denominator;if(distance>=forward) continue;
                        if(face.draw==2) {
                            const std::array weights{edge(face.xy[1],face.xy[2],p)/area,
                                edge(face.xy[2],face.xy[0],p)/area,edge(face.xy[0],face.xy[1],p)/area};
                            double u=0,v=0;for(unsigned c=0;c<3;++c) {u+=weights[c]*face.uv[c][0];v+=weights[c]*face.uv[c][1];}
                            ambiguous|=std::abs(u-std::round(u))<1.e-5 || std::abs(v-std::round(v))<1.e-5;
                            if(u<0 || v<0 || u>=4 || v>=4 || !texels[unsigned(v)*4+unsigned(u)]) {cutout_hole=true;continue;}
                        }
                        owner=face.draw;forward=distance;
                        for(unsigned c=0;c<3;++c) normal[c]=n[c]/std::sqrt(norm)*(plane>0?-1:1)*(c? -1:1);
                        const auto& value=draws[owner].vertices.front().color;
                        for(unsigned c=0;c<3;++c) color[c]=owner==4?value[c]*value[3]+color[c]*(1-value[3]):owner==2?(c<2?1:0):value[c];
                    }
                    const unsigned at=sample*size*24+pixel*4;const auto* tags=&pixels[sample*size*24+size*4+pixel*4];
                    std::array<float,4> surface;std::memcpy(surface.data(),&pixels[sample*size*24+size*8+pixel*16],16);
                    for(unsigned c=0;c<3;++c) average[c]+=decode(pixels[at+c]/255.)/count;
                    if(ambiguous) continue;
                    const unsigned layer=owner==0 || owner==9?1:owner==1 || owner==2?2:0;
                    const std::array<unsigned,4> expected_tags{owner==1 || owner==2?255U:0U,owner==1 || owner==2?255U:0U,layer,owner==1?255U:0U};
                    for(unsigned c=0;c<4;++c) if(tags[c]!=expected_tags[c]) throw std::runtime_error("Native MSAA sample ownership mismatch: count="+std::to_string(count)
                        +" eye="+std::to_string(eye_index)+" sample="+std::to_string(sample)+" xy="+std::to_string(x)+","+std::to_string(y)
                        +" channel="+std::to_string(c)+" actual="+std::to_string(tags[c])+" expected="+std::to_string(expected_tags[c])+" owner="+std::to_string(owner));
                    for(unsigned c=0;c<3;++c) check(std::abs(int(pixels[at+c])-encode(color[c]))<=1,"Native MSAA sample color disagrees with source/coverage");
                    check(pixels[at+3]==255,"Native MSAA sample extraction changed opacity");
                    if(layer==2) {
                        if(std::abs(surface[3]-forward*256)>.08) throw std::runtime_error("Native MSAA sample depth mismatch: count="+std::to_string(count)
                            +" sample="+std::to_string(sample)+" xy="+std::to_string(x)+","+std::to_string(y)
                            +" actual="+std::to_string(surface[3])+" expected="+std::to_string(forward*256));
                        for(unsigned c=0;c<3;++c) check(std::abs(surface[c]-normal[c])<.003,"MSAA sample normal disagrees with independent plane");
                        ++depth_checks;
                    } else check(surface==std::array<float,4>{},"MSAA excluded sample inherited a model surface");
                    ++checked;holes+=cutout_hole && owner!=2;
                    if(first_owner==9) first_owner=owner;else partial|=first_owner!=owner;
                }
                edge_pixels+=partial;
                const unsigned hardware=count*size*24+pixel*4,resolved=hardware+size*4;
                for(unsigned c=0;c<3;++c) {
                    check(std::abs(int(pixels[resolved+c])-encode(average[c]))<=1,"Enhanced MSAA resolve did not average linear sample colors");
                    check(std::abs(int(pixels[resolved+c])-int(pixels[hardware+c]))<=2,"Extracted sample resolve differs from hardware color resolve");
                }
                check(pixels[resolved+3]==255 && pixels[hardware+3]==255,"MSAA resolve changed opacity");
            }
            check(render(gpu,scene,draws,camera,{0,0,0,1})==baseline,"MSAA guides OFF changed single-sample rendering");
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(!transport.enqueue_sample(gpu.command,targets.color,targets.receiver,targets.surface,
                targets.colors[0],targets.receivers[0],targets.surfaces[0],width,height,count,count),"MSAA sample overflow accepted");
            check(!transport.enqueue_sample(gpu.command,targets.color,targets.receiver,targets.surface,
                targets.color,targets.receivers[0],targets.surfaces[0],width,height,count,0),"MSAA source/output alias accepted");
            check(!transport.enqueue_sample(gpu.command,targets.color,targets.receiver,nullptr,
                targets.colors[0],targets.receivers[0],targets.surfaces[0],width,height,count,0),"MSAA unmatched surface planes accepted");
            const std::array<void*,2> aliased{targets.colors[0],targets.colors[1]};
            check(!transport.enqueue_resolve(gpu.command,aliased,targets.colors[0],width,height),"MSAA resolve/source alias accepted");
            const std::array<void*,2> duplicate{targets.colors[0],targets.colors[0]};
            check(!transport.enqueue_resolve(gpu.command,duplicate,targets.resolved,width,height),"MSAA resolve accepted duplicate sample planes");
            check(!transport.enqueue_resolve(gpu.command,aliased,targets.resolved,width,height,targets.colors[0],nullptr),"Unpaired native MSAA ink accepted");
            check(!transport.enqueue_resolve(gpu.command,aliased,targets.resolved,width,height,targets.resolved,targets.receivers[0]),"MSAA ink/destination alias accepted");
            SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
            check(native_msaa_guides_render(gpu,scene,transport,targets,draws,camera)==pixels,"MSAA guide rejection/cancellation did not recover");
            const auto empty=native_msaa_guides_render(gpu,scene,transport,targets,{},camera);
            for(unsigned sample=0;sample<count;++sample) for(unsigned pixel=0;pixel<size;++pixel) {
                const unsigned at=sample*size*24+pixel*4,tags=sample*size*24+size*4+pixel*4;
                check(empty[at]==0 && empty[at+1]==0 && empty[at+2]==0 && empty[at+3]==255,"Empty MSAA sample kept stale color");
                check(empty[tags]==0 && empty[tags+1]==0 && empty[tags+2]==1 && empty[tags+3]==0,"Empty MSAA sample kept stale ownership");
                std::array<float,4> surface;std::memcpy(surface.data(),&empty[sample*size*24+size*8+pixel*16],16);
                check(surface==std::array<float,4>{},"Empty MSAA sample kept stale surface depth");
            }
            check(native_msaa_guides_render(gpu,scene,transport,targets,draws,camera)==pixels,"Empty MSAA sample transport did not recover");
        }
    }
    check(qualities>=2 && checked>1'000'000 && depth_checks>50'000 && holes>5'000 && edge_pixels>500 && effect_checks>100'000 && effect_changes>5'000,
        "Native MSAA guide fixture missed count/coverage/hole/depth/edge cases");
    std::cout<<"Native MSAA guides format "<<unsigned(gpu.format)<<": "<<checked<<" exact ownership/color samples, "<<depth_checks
        <<" independent sample-plane depths/normals, "<<holes<<" cutout holes, "<<edge_pixels
        <<" mixed-owner edge pixels, "<<effect_checks<<" per-sample AO/DOF oracle channels ("<<effect_changes<<" changed samples); "
        <<"real 2/4/8 samples, 2 independent eyes, linear enhanced/hardware resolve, receiver-only/ordered overlays/empty/held/OFF/alias/cancel recovery passed.\n";
}
