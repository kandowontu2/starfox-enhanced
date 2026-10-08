// Independent authored liquid/geometry guides; readback is fixture-only.
void water_guide_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=19,h=7,n=w*h;
    Texture receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        merged(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        liquid(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),source(gpu,w,h,format),styled(gpu,w,h,format);
    render::GpuCalibratedRayComposite compositor;check(compositor.initialize(gpu.device,format),compositor.status().c_str());
    Bytes ownership(n*4),raw(n*16),color(n*4);
    for(unsigned p=0;p<n;++p) {
        ownership[p*4]=255;ownership[p*4+1]=17;ownership[p*4+2]=2;ownership[p*4+3]=p%2?255:0;
        if(p%12==1) {ownership[p*4]=1;ownership[p*4+2]=1;}
        if(p%12==3) {ownership[p*4]=0;ownership[p*4+2]=0;}
        if(p%12==4) {ownership[p*4]=0;ownership[p*4+2]=1;}
        if(p%12==5) ownership[p*4+2]=3;
        const std::array<float,4> geometry{1,0,0,6000.F+p};std::memcpy(raw.data()+p*16,geometry.data(),16);
        color[p*4]=static_cast<unsigned char>(43+p%13);color[p*4+1]=static_cast<unsigned char>(117+p%31);
        color[p*4+2]=static_cast<unsigned char>(202-p%23);color[p*4+3]=static_cast<unsigned char>(91+p);
    }
    gpu.begin();receiver.upload(ownership);surface.upload(raw);source.upload(color);gpu.submit();
    for(bool world:{false,true}) for(bool has_surface:{false,true}) {
        const auto layout=*render::shadows::native_water_layers(w,h,world);
        Bytes layers(layout.storage_bytes,0),expected_owner=ownership,expected_surface=has_surface?raw:Bytes(n*16,0);
        for(unsigned p=0;p<n;++p) {
            layers[p*4+3]=p%12==2?255:p%12==10?254:253;
            std::array<float,4> guide{0,-.8F,.6F,321.F+p};
            if(p%12==6) guide[0]=std::numeric_limits<float>::quiet_NaN();
            if(p%12==7) guide[3]=-1;
            if(p%12==8) guide={0,0,0,321};
            if(p%12==9) guide[3]=std::numeric_limits<float>::infinity();
            if(p%12==11) guide={0,-2,0,321};
            std::memcpy(layers.data()+layout.surface_offset+p*16,guide.data(),16);
            if(p%12<2) {
                expected_owner[p*4]=1;expected_owner[p*4+1]=255;expected_owner[p*4+2]=1;
                std::memcpy(expected_surface.data()+p*16,guide.data(),16);
            }
        }
        gpu.begin();Buffer payload(gpu,layers);gpu.submit();
        const render::shadows::GpuReflectionOutput output{gpu.device,payload.buffer,w,h,w*4,layout};
        const auto encode=[&] {return compositor.enqueue_water_guides(gpu.command,receiver.texture,has_surface?surface.texture:nullptr,
            merged.texture,liquid.texture,w,h,output);};
        gpu.begin();check(encode(),compositor.status().c_str());gpu.submit();
        compare(merged.read(),expected_owner,"Liquid guide merge changed dry/protected/unknown/AA ownership",0);
        check(liquid.read()==expected_surface,"Liquid guide merge retained submerged depth/normal or changed dry guides");
        render::Framebuffer labels(w,h,1);labels.enable_layer_tags(true);
        for(unsigned p=0;p<n;++p) labels.layer_tags()[p]=unsigned(expected_owner[p*4+2]==1?render::PixelLayer::background:
            expected_owner[p*4+2]==2?render::PixelLayer::three_d:render::PixelLayer::two_d);
        const render::CalibratedPostEffects post{unsigned(render::Effect::sepia),unsigned(render::Effect::negative),100,100};
        auto expected_color=color;Bytes scratch;
        render::apply_effect(static_cast<render::Effect>(post.model),labels,expected_color,scratch,post.model_intensity,
            static_cast<render::Effect>(post.world),post.world_intensity);
        gpu.begin();check(compositor.enqueue(gpu.command,source.texture,merged.texture,styled.texture,w,h,
            nullptr,nullptr,1,1,0,1,post),compositor.status().c_str());gpu.submit();
        compare(styled.read(),expected_color,"Actual liquid selected submerged model style instead of world style",srgb(format)?1:0);
        render::GpuCalibratedDepth depth;check(depth.initialize(gpu.device,format),depth.status().c_str());
        vr::EyeCamera camera{};camera.view=identity;camera.projection[0]=1.3F;camera.projection[5]=1.6F;
        camera.projection[8]=.1F;camera.projection[9]=-.2F;camera.projection[10]=camera.projection[11]=-1;camera.projection[14]=-.1F;
        unsigned different_depth=0;
        for(unsigned modes:{1U,3U,4U,12U,15U}) {
            const render::DepthEnhancements settings{modes,{w*(1-.1F)/2-.5F,h*(1-.2F)/2-.5F,w*1.3F/2,500},h*1.6F/2};
            const auto reference=[&](const Bytes& guides) {
                auto result=color;
                const auto at_surface=[&](float xx,float yy,unsigned field) {
                    const int x=int(xx),y=int(yy);if(x<0 || y<0 || x>=int(w) || y>=int(h) || !expected_owner[(y*w+x)*4+2]) return 0.F;
                    float value{};std::memcpy(&value,guides.data()+(y*w+x)*16+field*4,4);return value;
                };
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                    const unsigned origin=(y*w+x)*4;
                    if(!expected_owner[origin+2] || at_surface(float(x),float(y),3)<=0) continue;
                    const auto ambient=render::depth_ambient(at_surface,float(x),float(y),1.F,settings);
                    for(unsigned c=0;c<3;++c) {
                        const auto at_colour=[&](float xx,float yy) {
                            xx=std::clamp(xx,0.F,float(w-1));yy=std::clamp(yy,0.F,float(h-1));
                            const unsigned bx=unsigned(xx),by=unsigned(yy);const float tx=xx-bx,ty=yy-by;float value=0;
                            for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                                unsigned at=(std::min(by+dy,h-1)*w+std::min(bx+dx,w-1))*4;
                                if(!expected_owner[at+2]) at=origin;
                                value+=color[at+c]/255.F*(dx?tx:1-tx)*(dy?ty:1-ty);
                            }
                            return value;
                        };
                        result[origin+c]=static_cast<unsigned char>(render::depth_channel(at_colour,at_surface,float(x),float(y),1.F,settings,ambient)*255.F+.5F);
                    }
                }
                return result;
            };
            const auto expected_depth=reference(expected_surface),wrong_depth=reference(raw);
            different_depth+=expected_depth!=wrong_depth;
            gpu.begin();check(depth.enqueue(gpu.command,source.texture,merged.texture,liquid.texture,styled.texture,w,h,1,modes,camera),depth.status().c_str());gpu.submit();
            compare(styled.read(),expected_depth,"Liquid AO/DOF used submerged model guides instead of authored surface reference",1);
        }
        check(different_depth>0,"Liquid depth oracle did not distinguish submerged geometry from its actual surface");
        gpu.begin();
        for(unsigned fault=0;fault<6;++fault) {
            auto bad=output;
            if(fault==0) bad.water_layers.surface_offset+=4;
            if(fault==1) bad.water_layers.storage_bytes-=4;
            if(fault==2) bad.water_layers.world_offset=world?0:4;
            if(fault==3) bad.width--;
            if(fault==4) bad.row_bytes+=4;
            if(fault==5) bad.water_layers={};
            check(!compositor.enqueue_water_guides(gpu.command,receiver.texture,surface.texture,merged.texture,liquid.texture,w,h,bad),
                "Malformed liquid guide layout accepted");
        }
        check(!compositor.enqueue_water_guides(gpu.command,receiver.texture,surface.texture,receiver.texture,liquid.texture,w,h,output),
            "Aliased liquid guide target accepted");
        check(encode(),compositor.status().c_str());check(SDL_CancelGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        gpu.begin();check(encode(),compositor.status().c_str());gpu.submit();
        compare(merged.read(),expected_owner,"Cancelled liquid guide retry changed ownership",0);
        check(liquid.read()==expected_surface,"Cancelled liquid guide retry changed depth");
    }
    std::cout<<"  Liquid MRT: full/compact, optional source depth, independent normal/depth/style + AO/DOF oracle, dry/protected/unknown/invalid guides, AA alpha, malformed/alias and cancellation passed\n";
}

// Independent double-precision geometric/normal oracle. No source-grid ray
// buffer is supplied: full-panel classification must use its own asymmetric
// eye projection and retained world plane, including non-unit Q15 transforms.
void water_panel_guide_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=127,h=71,n=w*h;
    Texture receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        merged(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),liquid(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
    render::GpuCalibratedRayComposite compositor;check(compositor.initialize(gpu.device,format),compositor.status().c_str());
    unsigned wet_total=0,dry_total=0,protected_total=0;
    for(unsigned test=0;test<12;++test) {
        auto camera=(*vr::sbs_eye_cameras(1.1F,float(w)/h,.064F,4,.05F,80))[test%2];
        // Native SDL presenters convert OpenXR's downward projection to their
        // retained positive-height native eye convention before encoding.
        camera.projection[5]=-camera.projection[5];camera.projection[9]=-camera.projection[9];
        camera.projection[8]+=.11F;camera.projection[9]-=.13F;
        render::CalibratedGroundRayReceiver ground;
        const double angle=(int(test%3)-1)*.27,c=std::cos(angle),s=std::sin(angle);
        ground.transport.world_to_view={float(c*1.001),float(-s*.997),0,float(s*1.003),float(c*.999),0,0,0,1.002F};
        const auto& matrix=ground.transport.world_to_view;
        ground.transport.camera_position={float(test*29),-12,float(test*37)};ground.transport.time=float(test)/7;
        ground.plane.point={0,test<10?72.:test==10?-.1:100000.,0};
        ground.plane.normal={-matrix[1],-matrix[4],-matrix[7]};
        ground.plane.normal=ground.plane.normal*(1/std::sqrt(render::shadows::dot(ground.plane.normal,ground.plane.normal)));
        // The authored shader plane is single precision. Evaluate those exact
        // inputs in double, not a different pre-conversion double plane whose
        // near-parallel horizon intersection can be materially different.
        ground.plane.normal={float(ground.plane.normal.x),float(ground.plane.normal.y),float(ground.plane.normal.z)};
        Bytes ownership(n*4),raw(n*16),expected_owner;
        std::vector<std::array<float,4>> primary(n),expected(n);
        for(unsigned p=0;p<n;++p) {
            ownership[p*4]=p%7==0?0:255;ownership[p*4+1]=17;ownership[p*4+2]=p%7==1?0:p%7==2?3:2;
            ownership[p*4+3]=static_cast<unsigned char>(p%256);
            primary[p]={0,0,-1,p%4==0?0.F:p%4==1?64.F:p%4==2?512.F:6000.F};
            std::memcpy(raw.data()+p*16,primary[p].data(),16);
        }
        expected_owner=ownership;expected=primary;
        const auto& projection=camera.projection;
        const double fx=float(w*projection[0]/2),fy=float(h*projection[5]/2),
            cx=float(w*(1-projection[8])/2),cy=float(h*(1+projection[9])/2);
        for(unsigned p=0;p<n;++p) {
            const auto layer=ownership[p*4+2];
            if(!ownership[p*4] || (layer!=1 && layer!=2)) {++protected_total;continue;}
            render::shadows::Vec3 ray{(p%w+.5-cx)/fx,(p/w+.5-cy)/fy,1};
            ray=ray*(1/std::sqrt(render::shadows::dot(ray,ray)));
            const double denominator=render::shadows::dot(ray,ground.plane.normal);
            const double distance=std::abs(denominator)>1.e-8?render::shadows::dot(ground.plane.point,ground.plane.normal)/denominator:-1;
            const double z=distance*ray.z,ndc=-double(projection[10])+double(projection[14])*256/z;
            if(distance<=0 || z>=65536 || ndc<0 || ndc>=1 || (primary[p][3]>0 && z>primary[p][3])) {++dry_total;continue;}
            const auto hit=ray*distance;
            const double wx=hit.x*matrix[0]+hit.y*matrix[3]+hit.z*matrix[6]+ground.transport.camera_position[0],
                wz=hit.x*matrix[2]+hit.y*matrix[5]+hit.z*matrix[8]+ground.transport.camera_position[2],t=ground.transport.time;
            const double footprint=distance/std::max(fx,1.)/std::max(std::abs(denominator),.04);
            const auto band=[&](double frequency){return 1/(1+std::pow(footprint*frequency,4));};
            const double dx=.055*std::cos(wx*.018+wz*.011-t*.8)*band(.022)+.025*std::cos(wx*.047-wz*.025+t*1.2)*band(.054),
                dz=.045*std::cos(wz*.022-wx*.009-t*.65)*band(.024)-.020*std::cos(wx*.047-wz*.025+t*1.2)*band(.054);
            render::shadows::Vec3 normal{matrix[0]*dx-matrix[1]+matrix[2]*dz,matrix[3]*dx-matrix[4]+matrix[5]*dz,
                matrix[6]*dx-matrix[7]+matrix[8]*dz};
            normal=normal*(1/std::sqrt(render::shadows::dot(normal,normal)));if(render::shadows::dot(normal,ray)>0)normal=normal*(-1);
            expected[p]={float(normal.x),float(normal.y),float(normal.z),float(z)};
            expected_owner[p*4]=1;expected_owner[p*4+1]=255;expected_owner[p*4+2]=1;++wet_total;
        }
        gpu.begin();receiver.upload(ownership);surface.upload(raw);
        const auto encode=[&]{return compositor.enqueue_water_panel_guides(gpu.command,receiver.texture,surface.texture,
            merged.texture,liquid.texture,w,h,camera,ground);};
        const auto checked_encode=[&]{if(!encode()) throw std::runtime_error("Panel liquid encode case="+std::to_string(test)+": "+compositor.status());};
        checked_encode();gpu.submit();
        check(merged.read()==expected_owner,"Panel liquid lost actual visibility/AA/protected ownership");
        const auto actual=liquid.read();
        for(unsigned p=0;p<n;++p)for(unsigned k=0;k<4;++k) {
            float value;std::memcpy(&value,actual.data()+p*16+k*4,4);
            const double tolerance=expected_owner[p*4]==1?(k==3?std::max(2.e-4,std::abs(double(expected[p][k]))*2.e-6):2.e-5):0;
            if(!std::isfinite(value) || std::abs(double(value)-expected[p][k])>tolerance)
                throw std::runtime_error("Panel water guide mismatch case="+std::to_string(test)+" pixel="+std::to_string(p)
                    +" field="+std::to_string(k)+" actual="+std::to_string(value)+" expected="+std::to_string(expected[p][k]));
        }
        gpu.begin();
        check(!compositor.enqueue_water_panel_guides(gpu.command,receiver.texture,surface.texture,receiver.texture,liquid.texture,w,h,camera,ground),
            "Aliased panel liquid guide accepted");
        for(unsigned fault=0;fault<5;++fault) {
            auto bad=ground;auto bad_camera=camera;
            if(fault==0)bad.transport.time=-1;if(fault==1)bad.transport.world_to_view={};
            if(fault==2)bad.plane.normal={};if(fault==3)bad.transport.material=1;if(fault==4)bad_camera.projection[0]=0;
            check(!compositor.enqueue_water_panel_guides(gpu.command,receiver.texture,surface.texture,merged.texture,liquid.texture,w,h,bad_camera,bad),
                "Invalid panel liquid geometry/projection/time accepted");
        }
        checked_encode();check(SDL_CancelGPUCommandBuffer(gpu.command),SDL_GetError());gpu.command=nullptr;
        gpu.begin();checked_encode();gpu.submit();check(merged.read()==expected_owner,"Panel liquid cancellation changed retained retry");
    }
    check(wet_total>1000 && dry_total>1000 && protected_total>1000,"Panel water oracle lacked positive/occluded/protected evidence");
    std::cout<<"Panel liquid format "<<unsigned(format)<<": 12 independent asymmetric/affine/clock/near/far geometry cases; "
        <<wet_total<<" wet, "<<dry_total<<" dry/occluded, "<<protected_total<<" protected; exact ownership/alpha, normals/depth and invalid/cancelled retries passed\n";
}
