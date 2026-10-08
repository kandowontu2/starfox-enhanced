// Explicit accepted primitive stream for secondary-hit motion. Readback is
// fixture-only; production evaluates the previous pose/animation on the GPU.
void native_ray_history_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    using Position=std::array<float,4>;
    constexpr unsigned vertex_count=54,vertex_bytes=vertex_count*16;
    unsigned comparisons=0;
    for(unsigned variant=0;variant<6;++variant) {
        std::array<std::vector<vr::SceneVertex>,6> current,accepted;
        current[0]=quad(-.16F,.07F,-2,.2F,{.7F,.2F,.1F,1});
        current[1]=quad(0,0,550,80,{.1F,.7F,.2F,1});
        current[2]=quad(0,0,-2.7F,.1F,{.2F,.1F,.7F,1});
        current[3].resize(10);
        current[4]=quad(.4F,0,-3,.1F,{1,1,1,1}); // New actor: never infer history by order.
        current[5]=quad(0,0,-1,.05F,{1,1,0,1}); // Protected/noncaster draw.
        const float endpoints[10][3]{{-.3F,.1F,-2},{.4F,.1F,-2.4F},
            {-.1F,-.3F,-1.8F},{-.1F,.4F,-2.1F},{-.1F,0,.2F},{.3F,.1F,-2},
            {.1F,.1F,-2},{.1F,.1F,-2},{-.1F,.1F,.3F},{.1F,.15F,.2F}};
        for(unsigned i=0;i<10;++i) {
            std::copy_n(endpoints[i],3,current[3][i].position);
            current[3][i].color[3]=current[3][i].odd_color[3]=1;
        }
        constexpr int corners[][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(unsigned i=0;i<6;++i) {
            auto& debris=current[1][i];debris.visibility_enabled=2;
            debris.visibility_a[0]=debris.visibility_c[2]=.97F;
            debris.visibility_a[2]=.24F;debris.visibility_b[1]=1;debris.visibility_c[0]=-.24F;
            debris.group_a[0]=24;debris.group_a[1]=-12;debris.group_a[2]=36;
            debris.group_b[0]=-11;debris.group_b[1]=7;debris.group_b[2]=18;
            debris.group_c[0]=8.25F;debris.group_c[1]=256;debris.group_c[2]=variant&1;
            auto& sprite=current[2][i];sprite.position[0]=-.1F;sprite.position[1]=.03F;
            sprite.texture[3]=1|4|134217728U;sprite.group_a[0]=280;sprite.group_a[1]=570;
            sprite.billboard[0]=corners[i][0]/256.F;sprite.billboard[1]=corners[i][1]/256.F;
            current[0][i].texture[3]=1; // Two one-texel materials force previous-stream alignment padding.
        }
        accepted=current;
        for(unsigned i=0;i<6;++i) {
            auto& debris=accepted[1][i];debris.visibility_a[0]=debris.visibility_c[2]=.99F;
            debris.visibility_a[2]=.14F;debris.visibility_c[0]=-.14F;
            debris.group_a[0]=-9;debris.group_a[1]=8;debris.group_a[2]=-15;debris.group_c[0]=3.5F;
            accepted[2][i].group_a[0]=330;accepted[2][i].group_a[1]=variant==1?120.F:635.F;
        }
        accepted[3][1].position[1]=.21F;accepted[3][1].position[2]=-1.9F;
        if(variant==2) for(unsigned i=0;i<3;++i)
            std::copy_n(accepted[0][0].position,3,accepted[0][i].position);
        const std::array<std::uint32_t,1> texels{0xff348ac7U};
        std::array<render::CalibratedSceneDraw,6> draws;
        std::array<render::CalibratedSceneMotionDraw,6> matches;
        for(unsigned i=0;i<draws.size();++i) {
            draws[i].vertices=current[i];draws[i].ray_caster=i<5;
            draws[i].model[12]=.06F;draws[i].model[13]=-.02F;
            matches[i].valid=i<4 && variant!=4;
            matches[i].previous_model[12]=-.04F;matches[i].previous_model[13]=.05F;
            if(matches[i].valid && (i!=0 || variant==2)) matches[i].previous_vertices=accepted[i];
        }
        draws[0].texels=texels;
        draws[2].texels=texels; // Billboard uses the same valid RGBA-source contract as rasterization.
        draws[3].topology=vr::SceneTopology::lines;
        if(variant==3) {matches[3].valid=false;matches[3].previous_vertices={};}
        // Non-uniform model scales make billboard width depend on the old
        // model too, not merely the accepted centre or current model scale.
        draws[2].model[0]=1.13F;draws[2].model[5]=.91F;
        matches[2].previous_model[0]=.93F;matches[2].previous_model[5]=1.07F;
        std::array<XrView,2> eyes{};
        for(auto& eye:eyes) eye.type=XR_TYPE_VIEW;
        auto old_eyes=eyes;
        for(unsigned e=0;e<2;++e) {
            const float yaw=e?.075F:-.045F,roll=e?-.06F:.04F;
            eyes[e].pose.orientation={std::sin(yaw/2)*std::sin(roll/2),std::sin(yaw/2)*std::cos(roll/2),
                std::cos(yaw/2)*std::sin(roll/2),std::cos(yaw/2)*std::cos(roll/2)};
            eyes[e].pose.position={e?.044F:-.033F,.017F,.061F};eyes[e].fov={-.57F,.73F,.61F,-.49F};
            const float old_yaw=e?-.025F:.035F;
            old_eyes[e].pose.orientation={0,std::sin(old_yaw/2),0,std::cos(old_yaw/2)};
            old_eyes[e].pose.position={e?.038F:-.03F,-.013F,-.027F};old_eyes[e].fov={-.63F,.68F,.56F,-.54F};
        }
        std::array<render::CalibratedRayGeometryOutput,2> rays;
        std::array<std::vector<Position>,2> expected;
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());
        const auto initial_upload=scene.upload_cost()[0];
        for(unsigned e=0;e<2;++e) {
            const auto now=camera_for(eyes[e],d3d),old=camera_for(old_eyes[e],d3d);
            const unsigned old_width=173,old_height=119,cube_size=variant==5?8U:0U;
            const render::CalibratedSceneMotion history{old,matches,old_width,old_height};
            rays[e]=scene.enqueue_ray_geometry(gpu.command,e,width,height,now,256,cube_size,{0,0,0,1},&history);
            check(rays[e].complete && rays[e].vertex_count==vertex_count,scene.status().c_str());
            check(rays[e].previous_vertex_offset%16==0 && rays[e].previous_vertex_offset>=
                rays[e].material_offset+rays[e].material_bytes+cube_size*cube_size*6*4,
                "Previous ray geometry overlaps current material/texel/environment data");
            check(rays[e].previous_extent==std::array{old_width,old_height},"Previous ray extent replaced by current extent");
            const auto& f=old_eyes[e].fov;const auto& p=rays[e].previous_projection;
            const double l=std::tan(f.angleLeft),r=std::tan(f.angleRight),u=std::tan(f.angleUp),d=std::tan(f.angleDown);
            const double fx=old_width/(r-l),fy=old_height/(u-d),near=rays[e].previous_near_plane;
            check(std::abs(p[0]-fx)<.0001 && std::abs(p[1]-fy)<.0001
                && std::abs(p[2]+old_width*l/(r-l))<.0001 && std::abs(p[3]-old_height*u/(u-d))<.0001,
                "Previous ray calibration ignored accepted FOV/extent");
            check(std::abs(near-.05*256)<.00001 && rays[e].previous_far_plane
                && std::abs(*rays[e].previous_far_plane-20*256)<.2,"Previous ray clipping lost source units");
            const auto native=[&](unsigned draw,const vr::SceneVertex& v) {
                auto point=draw==1?debris_point(v):std::array<double,3>{v.position[0],v.position[1],v.position[2]};
                std::array<double,3> world{};const auto& m=matches[draw].previous_model;
                for(unsigned axis=0;axis<3;++axis) world[axis]=double(m[axis])*point[0]+double(m[4+axis])*point[1]+double(m[8+axis])*point[2]+m[12+axis];
                auto eye=world_eye(old_eyes[e],world);
                if(draw==2) {
                    const double dimension=std::clamp(std::trunc(v.group_a[0]*256./v.group_a[1]),0.,240.);
                    eye[0]+=v.billboard[0]*dimension*v.group_a[1]/512*std::hypot(m[0],m[1],m[2]);
                    eye[1]+=v.billboard[1]*dimension*v.group_a[1]/512*std::hypot(m[4],m[5],m[6]);
                }
                return std::array<double,3>{eye[0]*256,-eye[1]*256,-eye[2]*256};
            };
            for(unsigned draw=0;draw<5;++draw) {
                if(draw!=3) {
                    for(unsigned i=0;i<6;++i) {
                        const bool visible=matches[draw].valid && !(draw==2 && variant==1) && !(draw==0 && variant==2 && i<3);
                        const auto position=native(draw,draw==0 && variant!=2?current[draw][i]:accepted[draw][i]);
                        expected[e].push_back(visible?Position{float(position[0]),float(position[1]),float(position[2]),1}:Position{});
                    }
                } else for(unsigned i=0;i<10;i+=2) {
                    auto a=native(draw,accepted[draw][i]),b=native(draw,accepted[draw][i+1]);
                    bool visible=matches[draw].valid && std::max(a[2],b[2])>=near;
                    if(visible && a[2]<near) {const double t=(near-a[2])/(b[2]-a[2]);for(unsigned c=0;c<3;++c) a[c]=std::lerp(a[c],b[c],t);}
                    else if(visible && b[2]<near) {const double t=(near-b[2])/(a[2]-b[2]);for(unsigned c=0;c<3;++c) b[c]=std::lerp(b[c],a[c],t);}
                    const double dx=visible?fx*(b[0]/b[2]-a[0]/a[2]):0,dy=visible?fy*(b[1]/b[2]-a[1]/a[2]):0;
                    const double length=std::hypot(dx,dy);visible=visible && length>1.e-10;
                    const double nx=visible?-dy/(length*2*fx):0,ny=visible?dx/(length*2*fy):0;
                    const std::array<Position,4> corners{{
                        {float(a[0]-nx*a[2]),float(a[1]-ny*a[2]),float(a[2]),1},
                        {float(b[0]-nx*b[2]),float(b[1]-ny*b[2]),float(b[2]),1},
                        {float(b[0]+nx*b[2]),float(b[1]+ny*b[2]),float(b[2]),1},
                        {float(a[0]+nx*a[2]),float(a[1]+ny*a[2]),float(a[2]),1}}};
                    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) expected[e].push_back(visible?corners[corner]:Position{});
                }
            }
        }
        check(rays[0].buffer!=rays[1].buffer,"Previous ray streams aliased between eyes");
        std::uint64_t expected_upload=initial_upload;
        for(unsigned i=0;i<4;++i) expected_upload+=matches[i].previous_vertices.size_bytes();
        check(scene.upload_cost()[0]==expected_upload,"Accepted primitives uploaded again for the second eye or transformed on CPU");
        const unsigned stride=rays[0].previous_vertex_offset+vertex_bytes;
        check(rays[1].previous_vertex_offset+vertex_bytes==stride,"Per-eye ray layouts differ unexpectedly");
        struct Download {SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer;
            ~Download(){if(buffer) SDL_ReleaseGPUTransferBuffer(device,buffer);}};
        const SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,stride*2,0};
        Download read{gpu.device,SDL_CreateGPUTransferBuffer(gpu.device,&transfer)};check(read.buffer,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        for(unsigned e=0;e<2;++e) {
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(rays[e].buffer),0,stride};
            const SDL_GPUTransferBufferLocation to{read.buffer,e*stride};SDL_DownloadFromGPUBuffer(copy,&from,&to);
        }
        SDL_EndGPUCopyPass(copy);gpu.finish();
        const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(bytes,SDL_GetError());
        std::array<std::vector<unsigned char>,2> current_prefix;
        for(unsigned e=0;e<2;++e) {
            const auto* positions=reinterpret_cast<const Position*>(bytes+e*stride+rays[e].previous_vertex_offset);
            check(expected[e].size()==vertex_count,"Previous ray fixture lost primitive order");
            for(unsigned i=0;i<vertex_count;++i) for(unsigned c=0;c<4;++c) {
                if(!std::isfinite(positions[i][c]) || std::abs(positions[i][c]-expected[e][i][c])>(c==3?0:.002F))
                    throw std::runtime_error("Previous ray oracle mismatch: variant="+std::to_string(variant)+" eye="+std::to_string(e)
                        +" vertex="+std::to_string(i)+" axis="+std::to_string(c)+" actual="+std::to_string(positions[i][c])+" expected="+std::to_string(expected[e][i][c]));
                ++comparisons;
            }
            current_prefix[e].assign(bytes+e*stride,bytes+e*stride+rays[e].material_offset+rays[e].material_bytes);
            std::uint32_t texel;std::memcpy(&texel,bytes+e*stride+rays[e].material_offset+vertex_count/3*64,4);
            check(texel==texels[0],"Previous geometry corrupted live texels");
        }
        SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
        // OFF must have exactly the same current triangles/materials, and no
        // previous publication/upload. Larger retained capacity is not history.
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.upload(gpu.command,draws),scene.status().c_str());const auto off_upload=scene.upload_cost()[0];
        for(unsigned e=0;e<2;++e) {
            const auto off=scene.enqueue_ray_geometry(gpu.command,e,width,height,camera_for(eyes[e],d3d));
            check(off.complete && !off.previous_vertex_offset && off.previous_extent==std::array<unsigned,2>{},"Previous ray state leaked into OFF");
            copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
            const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(off.buffer),0,unsigned(current_prefix[e].size())};
            const SDL_GPUTransferBufferLocation to{read.buffer,e*stride};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        }
        check(scene.upload_cost()[0]==off_upload,"Ray OFF uploaded previous geometry");gpu.finish();
        bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,read.buffer,false));check(bytes,SDL_GetError());
        for(unsigned e=0;e<2;++e) check(std::equal(current_prefix[e].begin(),current_prefix[e].end(),bytes+e*stride),"Previous ray export changed current geometry/material bytes");
        SDL_UnmapGPUTransferBuffer(gpu.device,read.buffer);
        // Malformed matched data must fail before uploading previous geometry,
        // then cancellation/retry must recover with the exact accepted source.
        for(unsigned bad=0;bad<6;++bad) {
            auto invalid=matches;auto invalid_vertices=accepted[1];auto old=camera_for(old_eyes[0],d3d);
            if(bad==0) invalid[1].previous_vertices=std::span(invalid_vertices).first(3);
            if(bad==1) {invalid_vertices[0].position[0]=std::numeric_limits<float>::quiet_NaN();invalid[1].previous_vertices=invalid_vertices;invalid[1].valid=true;}
            if(bad==2) {invalid[3].valid=true;invalid[3].previous_vertices={};}
            if(bad==3) {auto wrong=old;wrong.projection[8]+=.01F;invalid[0].valid=true;invalid[0].previous_camera_override=wrong;}
            if(bad==4) old.view[0]=old.view[4]=old.view[8]=0; // Singular previous eye.
            if(bad==5) {invalid_vertices[0].texture[3]=4096;invalid[1].previous_vertices=invalid_vertices;invalid[1].valid=true;}
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(scene.upload(gpu.command,draws),scene.status().c_str());const auto before=scene.upload_cost()[0];
            const render::CalibratedSceneMotion history{old,invalid,173,119};
            check(!scene.enqueue_ray_geometry(gpu.command,0,width,height,camera_for(eyes[0],d3d),256,0,{0,0,0,1},&history).complete,
                "Invalid accepted ray correspondence was published");
            check(scene.upload_cost()[0]==before,"Invalid ray correspondence uploaded partially validated history");
            SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        }
    }
    std::cout<<"Native secondary-hit geometry: "<<comparisons<<" independent previous XYZ/validity comparisons; two eyes, old FOV/extent/model, rigid/debris/billboard/line source, hidden/new/degenerate rejection, aligned texels/cube, OFF bytes, cancellation/validation passed. RGB transport remains separate.\n";
}
