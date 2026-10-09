// Independent source visibility and recursive painter references. No generated
// shader math or GPU-reported visibility is reused as the expected result.
void check_projected_bsp(SDL_GPUDevice* device) {
    using namespace starfox::render;
    struct ContinuousPoint {std::array<float,4> camera,screen;};
    std::array<std::array<std::int32_t,4>,128> native{};
    std::array<ContinuousPoint,128> continuous{};
    U seed=719;
    for(U i=0;i<native.size();++i) {
        for(unsigned axis=0;axis<3;++axis) {
            seed=seed*1664525U+1013904223U;
            native[i][axis]=std::int32_t((seed>>8)&65535)-32768;
            continuous[i].camera[axis]=float(native[i][axis])/8;
        }
        native[i][3]=i%11==0?-1:1;
        continuous[i].camera[3]=float(native[i][3]);
        continuous[i].screen[3]=i%7==0?-1.f:1.f;
    }
    auto reference_visibility=[&](const Four& face,bool fractional) -> U {
        if(face[3]==1) return 1;
        if(face[3]==3) return face[0]<native.size() && (fractional
            ?continuous[face[0]].screen[3]>0:native[face[0]][3]>0);
        for(unsigned i=0;i<3;++i) if(face[i]>=native.size()) return 0;
        if(!fractional) {
            const auto& a=native[face[0]];const auto& b=native[face[1]];const auto& c=native[face[2]];
            if(a[3]<0 || b[3]<0 || c[3]<0) return 0;
            auto word_difference=[](std::int32_t b,std::int32_t a) {
                const auto bits=std::uint16_t(std::uint32_t(b)-std::uint32_t(a));
                return std::int32_t(std::bit_cast<std::int16_t>(bits));
            };
            const auto bx=word_difference(b[0],a[0]),by=word_difference(b[1],a[1]);
            const auto cx=word_difference(c[0],a[0]),cy=word_difference(c[1],a[1]);
            const auto area=std::bit_cast<std::int32_t>(U(bx)*U(cy)-U(by)*U(cx));
            const bool odd=(a[2]<0)!=((b[2]<0)!=(c[2]<0));
            return (area<0)!=odd;
        }
        const auto& a=continuous[face[0]].camera;const auto& b=continuous[face[1]].camera;const auto& c=continuous[face[2]].camera;
        if(a[3]<0 || b[3]<0 || c[3]<0) return 0;
        if(face[3]==2) return 1;
        const double det=double(a[0])*b[1]*c[2]-double(a[0])*b[2]*c[1]
            -double(a[1])*b[0]*c[2]+double(a[1])*b[2]*c[0]
            +double(a[2])*b[0]*c[1]-double(a[2])*b[1]*c[0];
        double scale=1;
        for(const auto* p:{&a,&b,&c}) for(unsigned axis=0;axis<3;++axis) scale=std::max(scale,std::abs(double((*p)[axis])));
        return det<=scale*scale*scale*1e-12;
    };
    GpuBsp fused,baseline;
    std::uint64_t checked_visibility=0,checked_order=0;
    unsigned cases=0;
    for(bool fractional:{false,true}) for(U count:{1U,31U,63U,64U,65U,127U,4096U}) {
        std::vector<Four> visibility_faces(count);std::vector<U> expected_visibility(count);
        for(U i=0;i<count;++i) {
            auto& face=visibility_faces[i];
            for(unsigned axis=0;axis<3;++axis) {seed=seed*1664525U+1013904223U;face[axis]=(seed>>16)%130;}
            face[3]=i%16==0?1U:i%16==1?3U:i%16==2?2U:0U;
            if(i%16==3) face[1]=face[2]=face[0]; // Tangent determinant exactly zero.
            expected_visibility[i]=reference_visibility(face,fractional);
        }
        std::vector<Node> nodes;std::vector<U> face_ids;
        for(U i=0;i<31;++i) {
            nodes.push_back({{i%count,i<15?i*2+1:UINT32_MAX,i<15?i*2+2:UINT32_MAX,U(face_ids.size())},{2,i>=15?1U:0U,0,0}});
            face_ids.push_back(i*2+101);face_ids.push_back(i*2+102);
        }
        nodes[7].links[1]=0;nodes[2].links[2]=3; // Active cycle + legal shared subtree.
        nodes[4].links[0]=UINT32_MAX;
        const U bad=U(nodes.size());nodes.push_back({{0,UINT32_MAX,UINT32_MAX,UINT32_MAX},{2,1,0,0}});
        const U chain=U(nodes.size());
        for(U i=0;i<65;++i) nodes.push_back({{UINT32_MAX,UINT32_MAX,chain+i+1,0},{0,0,0,0}});
        constexpr U output_count=512;
        const std::array<Four,7> trees{{{0,0,output_count,4096},{0,0,0,4096},
            {bad,0,output_count,4096},{0,0,output_count,0},{chain,0,output_count,4096},
            {UINT32_MAX,0,output_count,4096},{0,output_count+1,0,4096}}};
        const std::array<U,7> statuses{0,3,1,4,2,0,1};
        std::vector<U> expected;std::set<U> active;
        std::function<void(U)> walk=[&](U index) {
            if(index>=nodes.size() || !active.insert(index).second) return;
            const auto& node=nodes[index];const bool visible=node.links[0]<count && expected_visibility[node.links[0]];
            const auto append=[&]{for(U i=0;i<node.batch[0];++i) expected.push_back(face_ids[node.links[3]+i]);};
            if(node.batch[1]) append();
            else if(visible) {walk(node.links[1]);append();walk(node.links[2]);}
            else {walk(node.links[2]);walk(node.links[1]);}
            active.erase(index);
        };walk(0);
        std::array<U,6> sizes{U(nodes.size()*sizeof(Node)),U(fractional?sizeof(continuous):sizeof(native)),
            count*16,U(face_ids.size()*4),16,count*4};
        std::array<const void*,6> data{nodes.data(),fractional?static_cast<const void*>(continuous.data()):native.data(),
            visibility_faces.data(),face_ids.data(),nullptr,expected_visibility.data()};
        std::array<SDL_GPUBuffer*,6> inputs{};
        U upload_size=0;for(U size:sizes) upload_size+=size;
        for(unsigned i=0;i<inputs.size();++i) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
            inputs[i]=SDL_CreateGPUBuffer(device,&info);require(inputs[i]);
        }
        SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,upload_size,0};
        auto* upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(upload);
        transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=count*4+(output_count*4+8)*2;
        auto* download=SDL_CreateGPUTransferBuffer(device,&transfer);require(download);
        for(unsigned scenario=0;scenario<trees.size();++scenario) {
            auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require(mapped);
            U offset=0;
            for(unsigned i=0;i<inputs.size();++i) {std::memcpy(mapped+offset,i==4?trees[scenario].data():data[i],sizes[i]);offset+=sizes[i];}
            SDL_UnmapGPUTransferBuffer(device,upload);
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);offset=0;
            for(unsigned i=0;i<inputs.size();++i) {
                SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{inputs[i],0,sizes[i]};
                SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=sizes[i];
            }
            SDL_EndGPUCopyPass(copy);
            GpuBspSettings config{1,U(nodes.size()),count,U(face_ids.size()),output_count,{}};
            const auto result=fused.enqueue_projected(device,command,inputs[0],inputs[1],inputs[2],inputs[3],inputs[4],config,128,fractional);
            require(result.visibility && result.order && result.results);
            const auto original=baseline.enqueue(device,command,inputs[0],inputs[5],inputs[3],inputs[4],config);
            require(original.order && original.results);
            auto bad_config=config;bad_config.tree_count=2;
            require(!fused.enqueue_projected(device,command,inputs[0],inputs[1],inputs[2],inputs[3],inputs[4],bad_config,128,fractional).order);
            bad_config=config;bad_config.visibility_count=4097;
            require(!fused.enqueue_projected(device,command,inputs[0],inputs[1],inputs[2],inputs[3],inputs[4],bad_config,128,fractional).order);
            require(!fused.enqueue_projected(device,command,inputs[0],inputs[1],result.visibility,inputs[3],inputs[4],config,128,fractional).order);
            copy=SDL_BeginGPUCopyPass(command);require(copy);offset=0;
            const std::array<std::pair<void*,U>,5> outputs{{{result.visibility,count*4},{result.order,output_count*4},
                {result.results,8},{original.order,output_count*4},{original.results,8}}};
            for(const auto& [buffer,bytes]:outputs) {
                SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(buffer),0,bytes};SDL_GPUTransferBufferLocation to{download,offset};
                SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=bytes;
            }
            SDL_EndGPUCopyPass(copy);auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
            require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
            const auto* values=static_cast<const U*>(SDL_MapGPUTransferBuffer(device,download,false));require(values);
            for(U i=0;i<count;++i) require(values[i]==expected_visibility[i]);
            const auto* order=values+count;const auto* header=order+output_count;
            const auto* old_order=header+2;const auto* old_header=old_order+output_count;
            const auto expected_count=scenario==0?expected.size():0;
            require(header[0]==expected_count && header[1]==statuses[scenario]);
            require(std::memcmp(header,old_header,8)==0);
            for(U i=0;i<expected_count;++i) require(order[i]==expected[i] && order[i]==old_order[i]);
            checked_visibility+=count;checked_order+=expected_count;++cases;
            SDL_UnmapGPUTransferBuffer(device,download);
        }
        // A producer may use either pipeline in one recording. Consume each
        // cycled order before the next enqueue; generic BSP may read the
        // producer's resident visibility without recreating its buffers.
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require(mapped);
        std::memcpy(mapped,trees[0].data(),16);SDL_UnmapGPUTransferBuffer(device,upload);
        auto* switching=SDL_AcquireGPUCommandBuffer(device);require(switching);
        auto* copy=SDL_BeginGPUCopyPass(switching);require(copy);
        SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{inputs[4],0,16};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        GpuBspSettings config{1,U(nodes.size()),count,U(face_ids.size()),output_count,{}};
        const auto projected=fused.enqueue_projected(device,switching,inputs[0],inputs[1],inputs[2],inputs[3],inputs[4],config,128,fractional);
        require(projected.visibility && projected.order && projected.results);
        copy=SDL_BeginGPUCopyPass(switching);require(copy);U offset=0;
        for(const auto& [buffer,bytes]:std::array<std::pair<void*,U>,3>{{{projected.visibility,count*4},{projected.order,output_count*4},{projected.results,8}}}) {
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(buffer),0,bytes};SDL_GPUTransferBufferLocation destination{download,offset};
            SDL_DownloadFromGPUBuffer(copy,&source,&destination);offset+=bytes;
        }
        SDL_EndGPUCopyPass(copy);
        const auto generic=fused.enqueue(device,switching,inputs[0],projected.visibility,inputs[3],inputs[4],config);
        require(generic.order && generic.results);
        copy=SDL_BeginGPUCopyPass(switching);require(copy);
        for(const auto& [buffer,bytes]:std::array<std::pair<void*,U>,2>{{{generic.order,output_count*4},{generic.results,8}}}) {
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(buffer),0,bytes};SDL_GPUTransferBufferLocation destination{download,offset};
            SDL_DownloadFromGPUBuffer(copy,&source,&destination);offset+=bytes;
        }
        SDL_EndGPUCopyPass(copy);auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(switching);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* values=static_cast<const U*>(SDL_MapGPUTransferBuffer(device,download,false));require(values);
        for(U i=0;i<count;++i) require(values[i]==expected_visibility[i]);
        for(U start:{count,count+output_count+2}) {
            const auto* order=values+start;const auto* header=order+output_count;
            require(header[0]==expected.size() && header[1]==0);
            for(U i=0;i<expected.size();++i) require(order[i]==expected[i]);
        }
        checked_visibility+=count;checked_order+=expected.size();++cases;
        SDL_UnmapGPUTransferBuffer(device,download);
        auto* cancelled=SDL_AcquireGPUCommandBuffer(device);require(cancelled);
        require(fused.enqueue_projected(device,cancelled,inputs[0],inputs[1],inputs[2],inputs[3],inputs[4],config,128,fractional).order);
        SDL_CancelGPUCommandBuffer(cancelled);
        for(auto* input:inputs) SDL_ReleaseGPUBuffer(device,input);
        SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
    }
    fused.release_device();baseline.release_device();
    std::cout<<"Projected visibility/BSP: "<<cases<<" cases, "<<checked_visibility<<" independent visibility flags, "
        <<checked_order<<" exact source-order entries; native/continuous, cycles/shared subtrees, failure statuses, alias/bounds, same-recording pipeline switching and cancellation reuse pass\n";
}
