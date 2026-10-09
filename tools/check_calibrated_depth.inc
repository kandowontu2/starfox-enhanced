// Included by the native scene fixture: real GPU rendering/readback only here,
// never in production. Geometry is checked against independent FOV rays/planes
// before the downloaded guide is used by the shared desktop effect oracle.
void native_depth_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    struct Resources {
        SDL_GPUDevice* device;SDL_GPUTexture *receiver{},*surface{},*output{};
        SDL_GPUTransferBuffer* read{};
        ~Resources() {
            SDL_WaitForGPUIdle(device);
            for(auto* t:{receiver,surface,output}) if(t) SDL_ReleaseGPUTexture(device,t);
            if(read) SDL_ReleaseGPUTransferBuffer(device,read);
        }
    } r{gpu.device};
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
    info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
    info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;r.receiver=SDL_CreateGPUTexture(gpu.device,&info);
    info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;r.surface=SDL_CreateGPUTexture(gpu.device,&info);
    info.format=gpu.format;r.output=SDL_CreateGPUTexture(gpu.device,&info);
    const SDL_GPUTransferBufferCreateInfo read_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*20,0};
    r.read=SDL_CreateGPUTransferBuffer(gpu.device,&read_info);check(r.receiver && r.surface && r.output && r.read,SDL_GetError());
    render::GpuCalibratedDepth effects;check(effects.initialize(gpu.device,gpu.format),effects.status().c_str());
    const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    const auto linear=[&](float value){return !srgb?value:value<=.04045F?value/12.92F:std::pow((value+.055F)/1.055F,2.4F);};
    const auto colour=[&](std::array<float,4> value) {for(unsigned c=0;c<3;++c) value[c]=linear(value[c]);return value;};
    const std::array<std::uint32_t,16> texture{
        0xffa07040,0xff504030,0xffa07040,0xff504030,0xff504030,0xffa07040,0xff504030,0xffa07040,
        0xffa07040,0xff504030,0xffa07040,0xff504030,0xff504030,0xffa07040,0xff504030,0xffa07040};
    std::vector<vr::SceneVertex> wall=quad(0,0,-2.3F,2.F,colour({.4F,.6F,.8F,1}));
    for(auto& v:wall) {v.texture[1]=v.texture[2]=3;v.texture[3]=1U|(srgb?2U:0U);
        v.uv[0]=(v.position[0]+2)*11;v.uv[1]=(v.position[1]+2)*9;}
    auto near=quad(.08F,.03F,-1.9F,.21F,colour({.5F,.4F,.3F,1}));
    // Tilt the wall's true geometry. Derivative normals must follow this, not
    // a generic front-facing normal or the source vertex's unrelated UV fields.
    for(auto& v:wall) v.position[2]+=v.position[0]*.18F+v.position[1]*.11F;
    auto emissive=quad(-.4F,-.2F,-1.5F,.10F,{1,0,0,1});
    auto hud=quad(.45F,-.25F,-1.2F,.08F,{1,1,0,1});
    auto sky=quad(-.8F,.5F,-1.8F,.11F,colour({.2F,.3F,.6F,1}));
    std::array<render::CalibratedSceneDraw,5> draws{{{wall,texture},{near},{emissive},{hud},{sky}}};
    draws[0].effect_layer=draws[1].effect_layer=2;
    draws[2].preserve_native_colour=true;draws[3].preserve_native_colour=true;draws[3].after_rays=true;
    draws[4].effect_layer=1;
    unsigned geometry_checks=0,protected_checks=0,colour_checks=0,changed_ao=0,changed_dof=0;
    std::array<std::vector<unsigned char>,2> eyes;
    for(unsigned e=0;e<2;++e) {
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
        eye.pose.position={e?.037F:-.031F,.018F,.023F};eye.fov={-.61F+e*.025F,.72F,.56F,-.49F};
        const auto camera=camera_for(eye,d3d);
        const auto baseline=render(gpu,scene,draws,camera,{.03F,.07F,.11F,1});eyes[e]=baseline;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
        const auto cost=scene.upload_cost();
        check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::before_rays,nullptr,0,r.surface),
            "Native surface target accepted missing raster ownership");
        auto invalid=camera;invalid.projection[4]=.2F;
        check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,invalid,{},render::CalibratedScenePhase::before_rays,r.receiver,0,r.surface),
            "Native surface target accepted unsupported projective terms");
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{.03F,.07F,.11F,1},
            render::CalibratedScenePhase::before_rays,r.receiver,0,r.surface),scene.status().c_str());
        (void)gpu.finish();check(scene.notify_submitted(token),"Native depth upload confirmation failed");
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},
            render::CalibratedScenePhase::after_rays,r.receiver,token,r.surface),scene.status().c_str());
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        SDL_GPUTextureTransferInfo target{r.read,0,width,height};
        SDL_GPUTextureRegion from{r.receiver,0,0,0,0,0,width,height,1};SDL_DownloadFromGPUTexture(copy,&from,&target);
        target.offset=width*height*4;from.texture=r.surface;SDL_DownloadFromGPUTexture(copy,&from,&target);SDL_EndGPUCopyPass(copy);
        const auto with_guides=gpu.finish();check(with_guides==baseline,"Native surface MRT altered original colour/depth/order");
        check(scene.upload_cost()==cost,"Native surfaces rerendered/uploaded geometry on the CPU");
        const auto* data=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,r.read,false));check(data,SDL_GetError());
        std::vector<unsigned char> tags(data,data+width*height*4);
        std::vector<std::array<float,4>> guide(width*height);std::memcpy(guide.data(),data+width*height*4,width*height*16);
        SDL_UnmapGPUTransferBuffer(gpu.device,r.read);
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto at=std::size_t(y)*width+x;const auto& g=guide[at];
            check(std::all_of(g.begin(),g.end(),[](float v){return std::isfinite(v);}),"Native surface guide became nonfinite");
            if(tags[at*4+2]!=2) {
                check(g==std::array<float,4>{},"Sky/HUD/emissive fragments inherited model surfaces");++protected_checks;continue;
            }
            // Independent eye ray intersected with the authored tilted wall
            // z=-2.3+.18*x+.11*y or the nearer front-facing patch z=-1.9.
            const double rx=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5)/width);
            const double ry=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5)/height);
            const double distance=(2.3+eye.pose.position.z-.18*eye.pose.position.x-.11*eye.pose.position.y)/(1+.18*rx+.11*ry);
            const double near_distance=1.9+eye.pose.position.z;
            const double nx=eye.pose.position.x+rx*near_distance,ny=eye.pose.position.y+ry*near_distance;
            const bool foreground=std::abs(nx-.08)<.21 && std::abs(ny-.03)<.21;
            const double expected=(foreground?near_distance:distance)*256;
            if(std::abs(g[3]-expected)>=.04) throw std::runtime_error("Native raster forward depth differs from independent ray/plane oracle: eye="+std::to_string(e)
                +" xy="+std::to_string(x)+","+std::to_string(y)+" actual="+std::to_string(g[3])+" expected="+std::to_string(expected));
            const double norm=std::sqrt(1+.18*.18+.11*.11);
            const std::array<double,3> normal=foreground?std::array<double,3>{0,0,-1}:std::array<double,3>{-.18/norm,.11/norm,-1/norm};
            for(unsigned c=0;c<3;++c) check(std::abs(g[c]-normal[c])<.002,"Native raster normal differs from independent surface plane");
            ++geometry_checks;
        }
        const auto surface=[&](float sx,float sy,unsigned field) {
            const int x=int(sx),y=int(sy);if(x<0 || y<0 || x>=int(width) || y>=int(height)) return 0.F;
            const auto i=std::size_t(y)*width+x;return tags[i*4+2]?guide[i][field]:0.F;
        };
        for(unsigned scale:{1U,3U}) for(unsigned modes=0;modes<16;++modes) {
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(effects.enqueue(gpu.command,gpu.color,r.receiver,r.surface,r.output,width,height,scale,modes,camera,230),effects.status().c_str());
            std::swap(gpu.color,r.output);const auto actual=gpu.finish();std::swap(gpu.color,r.output);
            render::DepthEnhancements settings;settings.modes=modes;
            settings.camera={float(width)*(1-camera.projection[8])/2-.5F,float(height)*(1+camera.projection[9])/2-.5F,
                float(width)*camera.projection[0]/2,230};settings.focal_y=float(height)*camera.projection[5]/2;
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const auto i=std::size_t(y)*width+x,at=i*4;
                check(actual[at+3]==baseline[at+3],"Native depth effect changed alpha");
                if(!modes || !tags[at+2] || guide[i][3]<=0) {
                    check(std::equal(actual.begin()+at,actual.begin()+at+4,baseline.begin()+at),"Native depth effect changed excluded ink or OFF output");continue;
                }
                const float ambient=render::depth_ambient(surface,float(x),float(y),float(scale),settings);
                for(unsigned c=0;c<3;++c) {
                    const auto colour_sample=[&](float sx,float sy) {
                        sx=std::clamp(sx,0.F,float(width-1));sy=std::clamp(sy,0.F,float(height-1));
                        const unsigned ax=unsigned(sx),ay=unsigned(sy);const float tx=sx-ax,ty=sy-ay;float value=0;
                        for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                            auto n=std::size_t(std::min(ay+dy,height-1))*width+std::min(ax+dx,width-1);if(!tags[n*4+2]) n=i;
                            value+=baseline[n*4+c]/255.F*(dx?tx:1-tx)*(dy?ty:1-ty);
                        }
                        return value;
                    };
                    const int expected=int(render::depth_channel(colour_sample,surface,float(x),float(y),float(scale),settings,ambient)*255.F+.5F);
                    // Golden-angle/sqrt scalar CPU and GPU arithmetic can land
                    // on opposite sides of a final half-code rounding boundary.
                    // Geometry, alpha, excluded pixels and OFF remain exact;
                    // bounded active RGB is allowed one code in either format.
                    if(std::abs(int(actual[at+c])-expected)>1) throw std::runtime_error("Native depth CPU oracle differs: eye="+std::to_string(e)
                        +" mode="+std::to_string(modes)+" scale="+std::to_string(scale)+" xy="+std::to_string(x)+","+std::to_string(y)
                        +" channel="+std::to_string(c)+" actual="+std::to_string(actual[at+c])+" expected="+std::to_string(expected));
                    ++colour_checks;
                }
                if(modes==3) changed_ao+=!std::equal(actual.begin()+at,actual.begin()+at+3,baseline.begin()+at);
                if(modes==12) changed_dof+=!std::equal(actual.begin()+at,actual.begin()+at+3,baseline.begin()+at);
            }
            if(modes==15 && scale==3) {
                gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);
                check(effects.enqueue(gpu.command,gpu.color,r.receiver,r.surface,r.output,width,height,scale,modes,camera,230),effects.status().c_str());
                std::swap(gpu.color,r.output);check(gpu.finish()==actual,"Retained native depth effect jittered on retry");std::swap(gpu.color,r.output);
            }
        }
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);
        check(!effects.enqueue(gpu.command,gpu.color,r.receiver,r.surface,gpu.color,width,height,1,15,camera),"Native depth input/output alias accepted");
        check(!effects.enqueue(gpu.command,gpu.color,r.receiver,r.surface,r.output,width,height,1,16,camera),"Native depth mode overflow accepted");
        check(!effects.enqueue(gpu.command,gpu.color,r.receiver,r.surface,r.output,width,height,1,15,invalid),"Native depth projective camera accepted");
        check(!effects.enqueue(gpu.command,gpu.color,r.receiver,r.surface,r.output,width,height,1,15,camera,NAN),"Native depth nonfinite focus accepted");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        check(render(gpu,scene,draws,camera,{.03F,.07F,.11F,1})==baseline,"Native depth OFF failed original pipeline restoration");
    }
    unsigned specialized_checks=0;
    for(unsigned kind=0;kind<5;++kind) for(unsigned e=0;e<2;++e) {
        XrView eye{XR_TYPE_VIEW};const float yaw=e?.12F:-.09F,roll=e?-.16F:.13F;
        eye.pose.orientation={std::sin(yaw/2)*std::sin(roll/2),std::sin(yaw/2)*std::cos(roll/2),
            std::cos(yaw/2)*std::sin(roll/2),std::cos(yaw/2)*std::cos(roll/2)};
        eye.pose.position={e?.04F:-.033F,.012F,.03F};eye.fov={-.55F,.72F,.52F,-.46F};
        auto camera=camera_for(eye,d3d);
        vr::DrawPacket packet;std::array<std::array<double,3>,3> points;
        if(kind==0) {
            packet.geometry.vertices=quad(.05F,0,-2.1F,.37F,colour({.6F,.3F,.2F,1}));
            for(auto& v:packet.geometry.vertices) {
                v.position[2]+=v.position[0]*.25F+v.position[1]*.17F;
                v.texture[1]=8;v.texture[3]=65536;v.uv[0]=v.uv[1]=3;v.billboard[0]=7;
            }
            auto& words=packet.geometry.texels;words.resize(4+8*24);words[0]=4;words[2]=8;
            for(unsigned y=0;y<8;++y) {const unsigned at=4+y*24;words[at+1]=y;words[at+2]=8;words[at+3]=y+1;}
        } else if(kind==1) {
            packet.geometry.vertices=quad(.05F,.02F,-2.3F,0,colour({.2F,.6F,.3F,1}));
            packet.geometry.texels.assign(16,0xff80a050);
            packet.geometry.texels[0]=packet.geometry.texels[5]=packet.geometry.texels[10]=0; // Real alpha cutouts.
            for(unsigned i=0;i<6;++i) {
                auto& v=packet.geometry.vertices[i];const unsigned corners[]{0,1,2,0,2,3};const auto corner=corners[i];
                v.texture[1]=v.texture[2]=3;v.texture[3]=5U|(srgb?2U:0U);
                v.billboard[0]=corner==1 || corner==2?.28F:-.28F;v.billboard[1]=corner>=2?.28F:-.28F;
                v.uv[0]=corner==1 || corner==2?3:0;v.uv[1]=corner>=2?3:0;
            }
        } else if(kind==2) {
            packet.geometry.vertices=quad(0,0,500,96,colour({.3F,.4F,.7F,1}));
            for(auto& v:packet.geometry.vertices) {
                v.visibility_enabled=2;v.visibility_a[0]=v.visibility_b[1]=v.visibility_c[2]=1;
                v.group_a[0]=32;v.group_a[1]=-24;v.group_b[0]=20;v.group_b[1]=-4;v.group_b[2]=3;
                v.group_c[0]=8.5F;v.group_c[1]=256;v.group_c[2]=1;
            }
        } else if(kind==3) {
            simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
            ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.bg2_character_base=0;ppu.cgram[1]=31;
            for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
            for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
            packet=vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);vr::place_landscape_ground(packet,-.75F,true);
            const float angle=e?.2F:-.17F,c=std::cos(angle),s=std::sin(angle);
            packet.model={c,s,0,0,-s,c,0,0,0,0,1,0,.08F,0,-.12F,1};
            for(unsigned row=0;row<3;++row) {packet.model[row]*=1.2F;packet.model[4+row]*=.92F;packet.model[8+row]*=.85F;}
            camera.projection[10]=-1;camera.projection[14]=-.05F;
        } else {
            packet.geometry.vertices=quad(.05F,0,-1.8F,.35F,colour({.3F,.6F,.7F,1}));
            packet.geometry.texels.assign(1,0xff809040);
            for(auto& v:packet.geometry.vertices) {
                v.position[2]+=v.position[0]*.16F-v.position[1]*.23F;
                v.texture[3]=1U|32768U|(srgb?2U:0U);
            }
        }
        for(unsigned corner=0;corner<3;++corner) {
            vr::SceneVertex floor_vertex{};
            floor_vertex.position[0]=corner==1?1:-1;floor_vertex.position[1]=-.75F;floor_vertex.position[2]=corner==2?-3:-1;
            const auto& v=kind==3?floor_vertex:packet.geometry.vertices[corner];
            const std::array<double,3> world=kind==2?debris_point(v):std::array<double,3>{v.position[0],v.position[1],v.position[2]};
            points[corner]=world_eye(eye,kind==3?model_point(packet.model,v):world);
            if(kind==1) {points[corner][0]+=v.billboard[0];points[corner][1]+=v.billboard[1];}
        }
        const std::array<double,3> a{points[1][0]-points[0][0],points[1][1]-points[0][1],points[1][2]-points[0][2]},
            b{points[2][0]-points[0][0],points[2][1]-points[0][1],points[2][2]-points[0][2]};
        std::array<double,3> normal{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
        const double length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
        for(auto& c:normal) c/=length;
        double constant=0;for(unsigned c=0;c<3;++c) constant+=normal[c]*points[0][c];
        if(constant>0) {constant=-constant;for(auto& c:normal) c=-c;}
        render::CalibratedScenePacket draw{&packet};draw.effect_layer=kind==3?1:2;
        if(kind==3) draw.depth_test=false;
        const auto original=render_packets(gpu,scene,std::span(&draw,1),camera);
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);
        check(scene.upload_packets(gpu.command,std::span(&draw,1)),scene.status().c_str());const auto cost=scene.upload_cost();
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{0,0,0,1},render::CalibratedScenePhase::before_rays,r.receiver,0,r.surface),scene.status().c_str());
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);SDL_GPUTextureTransferInfo target{r.read,0,width,height};
        const SDL_GPUTextureRegion from{r.surface,0,0,0,0,0,width,height,1};SDL_DownloadFromGPUTexture(copy,&from,&target);SDL_EndGPUCopyPass(copy);
        check(gpu.finish()==original,"Native specialized surface MRT changed original source coverage/colour");
        check(scene.upload_cost()==cost,"Native specialized surface changed source upload cost");
        const auto* guide=static_cast<const std::array<float,4>*>(SDL_MapGPUTransferBuffer(gpu.device,r.read,false));check(guide,SDL_GetError());
        unsigned visible=0;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto& g=guide[y*width+x];
            check(std::all_of(g.begin(),g.end(),[](float v){return std::isfinite(v);}),"Native specialized surface guide became nonfinite");
            if(g[3]<=0) continue;
            const double rx=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5)/width);
            const double ry=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5)/height);
            const double distance=constant/(normal[0]*rx+normal[1]*ry-normal[2]);
            // The independent oracle uses double FOV/quaternion rays, whereas
            // uploaded projection/model coefficients are single precision.
            // Near a nearly parallel floor horizon their angular error is
            // amplified as distance^2/plane offset, not a flat relative error.
            const double angular_bound=kind==3?distance*distance/std::abs(constant)*2.e-7*256:0;
            if(std::abs(g[3]-distance*256)>std::max({.05,distance*256*.0001,angular_bound})) throw std::runtime_error("Native specialized depth differs from independent source geometry: kind="+std::to_string(kind)
                +" eye="+std::to_string(e)+" xy="+std::to_string(x)+","+std::to_string(y)+" actual="+std::to_string(g[3])+" expected="+std::to_string(distance*256));
            for(unsigned c=0;c<3;++c) check(std::abs(g[c]-normal[c]*(c==0?1:-1))<.003,
                "Native specialized normal used texture UV/billboard centre instead of true raster point");
            ++visible;++specialized_checks;
        }
        SDL_UnmapGPUTransferBuffer(gpu.device,r.read);
        if(visible<=100) {
            unsigned source_pixels=0;
            for(unsigned at=0;at<original.size();at+=4) source_pixels+=original[at] || original[at+1] || original[at+2];
            throw std::runtime_error("Native specialized guide fixture missed visible geometry: kind="+std::to_string(kind)
                +" eye="+std::to_string(e)+" samples="+std::to_string(visible)+" source_pixels="+std::to_string(source_pixels));
        }
    }
    check(eyes[0]!=eyes[1] && geometry_checks>50000 && protected_checks>1000 && changed_ao>100 && changed_dof>100,
        "Native depth fixture missed independent eyes/surfaces/protection/AO/DOF");
    std::cout<<"Native AO/DOF: "<<geometry_checks<<" independent ray-plane depths/normals, "<<protected_checks
        <<" protected sky/HUD/emissive guides, "<<colour_checks<<" desktop CPU-oracle RGB checks; "<<specialized_checks
        <<" source-span/billboard/destruction/banked-floor/Z-biased decal ray-plane samples; 16 modes/2 scales, asymmetric eyes, MRT exact colour, alpha/retries/OFF/validation passed.\n";
}
