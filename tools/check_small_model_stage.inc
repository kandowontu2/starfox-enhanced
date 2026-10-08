// The bounded producer must preserve the separate transform's raw point and
// residual bytes. Visibility and authored painter order have independent CPU
// references; full model checks separately compare pixels to SoftwareRenderer.
void check_small_model_stage(SDL_GPUDevice* device) {
    using namespace starfox::render;
    GpuProjection projection;GpuBsp fused;
    std::uint64_t transformed_bytes=0,visibility_flags=0,painter_entries=0;
    unsigned cases=0;
    for(unsigned mode=0;mode<3;++mode) for(bool lossless:{false,true})
    for(U count:{1U,31U,63U,64U,65U,127U,128U}) {
        const auto context="small stage mode "+std::to_string(mode)+" count "+std::to_string(count)
            +" lossless "+std::to_string(lossless);
        auto check=[&](bool value,const char* what) {
            if(!value) throw std::runtime_error(context+": "+what+": "+SDL_GetError());
        };
        starfox::assets::Shape shape;shape.header.shift=1;
        for(U i=0;i<count;++i) {
            shape.vertices.push_back({int((i*197U)%511)-255,int((i*83U)%313)-156,int((i*109U)%601)-300});
            shape.word_coordinates.push_back(i%3==0);
        }
        RenderPose pose;pose.continuous_geometry=pose.subpixel_projection=true;
        pose.use_rotation_matrix=mode!=2;pose.rotation_matrix={-30123,5211,8017,-7109,-30991,3951,7011,-5301,-31013};
        pose.x=16777216.125;pose.y=-16777215.875;pose.z=64.123456789;
        pose.scale=1.23456789;pose.pitch=17391.123;pose.yaw=23901.456;pose.roll=1309.75;
        pose.vanish_x=111.8125;pose.vanish_y=97.1875;
        auto packed=pack_projection(shape,pose,{});
        auto vertices=packed.continuous_vertices;
        std::array<ContinuousTransformPose,6> poses{};
        std::copy(packed.continuous_poses.begin(),packed.continuous_poses.end(),poses.begin());
        std::copy(packed.euler_operands.begin(),packed.euler_operands.end(),poses.begin()+4);
        if(mode==0) { // Original FP32 path, not coefficient-residual arithmetic.
            poses[0].vanish[3]=poses[1].vanish[3]=0;
            poses[0].translation[0]=poses[1].translation[0]=-.3125f;
            poses[0].translation[1]=poses[1].translation[1]=.1875f;
        } else if(mode==2) poses[0].vanish[3]=poses[1].vanish[3]=2;
        if(count>1) vertices.back().pose=99; // Invalid pose must not bypass a group barrier.
        std::vector<Four> visibility_faces(count);
        for(U i=0;i<count;++i) {
            auto& face=visibility_faces[i];
            face={(i*29U)%count,(i*31U+1)%count,(i*37U+2)%count,
                i%9==0?1U:i%9==1?3U:i%9==2?2U:0U};
            if(i%9==3) face[1]=face[2]=face[0];
            if(i%9==4) face[0]=count; // Invalid visibility index.
        }
        std::vector<Node> nodes;std::vector<U> face_ids;
        for(U i=0;i<31;++i) {
            nodes.push_back({{i%count,i<15?i*2+1:UINT32_MAX,i<15?i*2+2:UINT32_MAX,U(face_ids.size())},
                {2,i>=15?1U:0U,0,0}});
            face_ids.push_back(i*2+101);face_ids.push_back(i*2+102);
        }
        nodes[7].links[1]=0;nodes[2].links[2]=3;nodes[4].links[0]=UINT32_MAX;
        const U bad=U(nodes.size());nodes.push_back({{0,UINT32_MAX,UINT32_MAX,UINT32_MAX},{2,1,0,0}});
        const U chain=U(nodes.size());
        for(U i=0;i<65;++i) nodes.push_back({{UINT32_MAX,UINT32_MAX,chain+i+1,0},{0,0,0,0}});
        constexpr U output_count=512;
        const std::array<Four,7> trees{{{0,0,output_count,4096},{0,0,0,4096},
            {bad,0,output_count,4096},{0,0,output_count,0},{chain,0,output_count,4096},
            {UINT32_MAX,0,output_count,4096},{0,output_count+1,0,4096}}};
        const std::array<U,7> statuses{0,3,1,4,2,0,1};
        const std::array<U,6> sizes{count*16,U(sizeof(poses)),U(nodes.size()*sizeof(Node)),count*16,U(face_ids.size()*4),16};
        std::array<SDL_GPUBuffer*,6> inputs{};U upload_size=0;
        for(unsigned i=0;i<inputs.size();++i) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
            inputs[i]=SDL_CreateGPUBuffer(device,&info);require(inputs[i]);upload_size+=sizes[i];
        }
        SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,upload_size,0};
        auto* upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(upload);
        const U row_bytes=count*64+count*8+output_count*12+24;
        transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=count*64+row_bytes*U(trees.size());
        auto* download=SDL_CreateGPUTransferBuffer(device,&transfer);require(download);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);U offset=0;
        auto capture=[&](std::initializer_list<std::pair<void*,U>> outputs) {
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
            for(const auto& [buffer,bytes]:outputs) {
                SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(buffer),0,bytes};
                SDL_GPUTransferBufferLocation to{download,offset};SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=bytes;
            }
            SDL_EndGPUCopyPass(copy);
        };
        GpuBspSettings config{1,U(nodes.size()),count,U(face_ids.size()),output_count,{}};
        void* separate_points=nullptr;void* separate_residuals=nullptr;
        for(unsigned scenario=0;scenario<trees.size();++scenario) {
            const std::array<const void*,6> data{vertices.data(),poses.data(),nodes.data(),visibility_faces.data(),face_ids.data(),trees[scenario].data()};
            auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require(mapped);
            U position=0;for(unsigned i=0;i<inputs.size();++i) {std::memcpy(mapped+position,data[i],sizes[i]);position+=sizes[i];}
            SDL_UnmapGPUTransferBuffer(device,upload);
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);position=0;
            for(unsigned i=0;i<inputs.size();++i) {
                SDL_GPUTransferBufferLocation from{upload,position};SDL_GPUBufferRegion to{inputs[i],0,sizes[i]};
                SDL_UploadToGPUBuffer(copy,&from,&to,true);position+=sizes[i];
            }
            SDL_EndGPUCopyPass(copy);
            if(scenario==0) {
                separate_points=projection.enqueue_continuous(device,command,inputs[0],count,inputs[1],6,&separate_residuals,lossless);
                require(separate_points && separate_residuals);
                capture({{separate_points,count*32},{separate_residuals,count*32}});
            }
            const auto result=fused.enqueue_continuous_model(device,command,inputs[0],inputs[1],6,inputs[2],inputs[3],inputs[4],inputs[5],config,count,lossless);
            require(result.points && result.residuals && result.visibility && result.order && result.results);
            capture({{result.points,count*32},{result.residuals,count*32},{result.visibility,count*4},{result.order,output_count*4},{result.results,8}});
            // The same owner switches all three producer layouts within one
            // command. Captured cycled buffers must retain their prior version.
            const auto projected=fused.enqueue_projected(device,command,inputs[2],separate_points,inputs[3],inputs[4],inputs[5],config,count,true);
            require(projected.visibility && projected.order && projected.results);
            capture({{projected.visibility,count*4},{projected.order,output_count*4},{projected.results,8}});
            const auto generic=fused.enqueue(device,command,inputs[2],projected.visibility,inputs[4],inputs[5],config);
            require(generic.order && generic.results);capture({{generic.order,output_count*4},{generic.results,8}});
            for(void* alias:{result.points,result.residuals,result.visibility,result.order,result.results})
                require(!fused.enqueue_continuous_model(device,command,alias,inputs[1],6,inputs[2],inputs[3],inputs[4],inputs[5],config,count,lossless).order);
            auto invalid=config;invalid.tree_count=2;
            require(!fused.enqueue_continuous_model(device,command,inputs[0],inputs[1],6,inputs[2],inputs[3],inputs[4],inputs[5],invalid,count,lossless).order);
            invalid=config;invalid.visibility_count=129;
            require(!fused.enqueue_continuous_model(device,command,inputs[0],inputs[1],6,inputs[2],inputs[3],inputs[4],inputs[5],invalid,count,lossless).order);
            for(U bad_count:{0U,129U}) require(!fused.enqueue_continuous_model(device,command,inputs[0],inputs[1],6,inputs[2],inputs[3],inputs[4],inputs[5],config,bad_count,lossless).order);
            require(!fused.enqueue_continuous_model(device,command,inputs[0],inputs[1],0,inputs[2],inputs[3],inputs[4],inputs[5],config,count,lossless).order);
        }
        require(offset==transfer.size);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* raw=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,download,false));require(raw);
        const auto* points=reinterpret_cast<const ContinuousProjectedPoint*>(raw);
        auto visible=[&](const Four& face)->U {
            if(face[3]==1) return 1;
            if(face[3]==3) return face[0]<count && points[face[0]].screen[3]>0;
            for(unsigned i=0;i<3;++i) if(face[i]>=count || points[face[i]].camera[3]<0) return 0;
            if(face[3]==2) return 1;
            const auto& a=points[face[0]].camera;const auto& b=points[face[1]].camera;const auto& c=points[face[2]].camera;
            const double determinant=double(a[0])*b[1]*c[2]-double(a[0])*b[2]*c[1]-double(a[1])*b[0]*c[2]
                +double(a[1])*b[2]*c[0]+double(a[2])*b[0]*c[1]-double(a[2])*b[1]*c[0];
            double scale=1;for(const auto* p:{&a,&b,&c}) for(unsigned i=0;i<3;++i) scale=std::max(scale,std::abs(double((*p)[i])));
            return determinant<=scale*scale*scale*1e-12;
        };
        std::vector<U> expected_visibility;for(const auto& face:visibility_faces) expected_visibility.push_back(visible(face));
        std::vector<U> expected_order;std::set<U> active;
        std::function<void(U)> walk=[&](U index) {
            if(index>=nodes.size() || !active.insert(index).second) return;
            const auto& node=nodes[index];const bool vis=node.links[0]<count && expected_visibility[node.links[0]];
            auto append=[&]{for(U i=0;i<node.batch[0];++i) expected_order.push_back(face_ids[node.links[3]+i]);};
            if(node.batch[1]) append();else if(vis) {walk(node.links[1]);append();walk(node.links[2]);}else {walk(node.links[2]);walk(node.links[1]);}
            active.erase(index);
        };walk(0);
        offset=count*64;
        for(unsigned scenario=0;scenario<trees.size();++scenario) {
            check(std::memcmp(raw,raw+offset,count*64)==0,"raw transform/residual mismatch");offset+=count*64;
            for(unsigned producer=0;producer<3;++producer) {
                if(producer<2) {
                    const auto* flags=reinterpret_cast<const U*>(raw+offset);
                    for(U i=0;i<count;++i) check(flags[i]==expected_visibility[i],"independent visibility mismatch");
                    visibility_flags+=count;offset+=count*4;
                }
                const auto* order=reinterpret_cast<const U*>(raw+offset);offset+=output_count*4;
                const auto* header=reinterpret_cast<const U*>(raw+offset);offset+=8;
                check(header[1]==statuses[scenario],"failure status mismatch");
                const U expected_count=scenario==0?U(expected_order.size()):0;
                check(header[0]==expected_count,"painter count mismatch");
                for(U i=0;i<expected_count;++i) check(order[i]==expected_order[i],"independent painter mismatch");
                painter_entries+=expected_count;
            }
            transformed_bytes+=count*64;++cases;
        }
        SDL_UnmapGPUTransferBuffer(device,download);
        auto* cancelled=SDL_AcquireGPUCommandBuffer(device);require(cancelled);
        require(fused.enqueue_continuous_model(device,cancelled,inputs[0],inputs[1],6,inputs[2],inputs[3],inputs[4],inputs[5],config,count,lossless).order);
        SDL_CancelGPUCommandBuffer(cancelled);
        // Recreate resources on the next group after cancelled recording.
        fused.release_device();projection.release_device();
        for(auto* input:inputs) SDL_ReleaseGPUBuffer(device,input);
        SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
    }
    std::cout<<"Bounded small-model stage: "<<cases<<" cases, "<<transformed_bytes<<" exact separate-stage point/residual bytes, "
        <<visibility_flags<<" independent visibility flags, "<<painter_entries<<" independent painter entries; FP32/Q15/Euler, both residual formats, "
        <<"1/31/63/64/65/127/128 counts, invalid poses, cycles, statuses 0..4, alias/bounds, mixed producers, cancellation/recreation pass\n";
}
