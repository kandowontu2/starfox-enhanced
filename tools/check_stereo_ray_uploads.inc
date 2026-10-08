// Independent geometry/material oracle plus same-binary upload-policy parity.
// Only this diagnostic reads GPU data back; production retains per-eye results.
void check_stereo_ray_uploads(SDL_GPUDevice* device) {
    using namespace starfox::render;
    struct Environment {
        SDL_Environment* env=SDL_GetEnvironment();
        std::array<const char*,7> names{"STARFOX_TEST_JOINED_STEREO_SUBMISSIONS",
            "STARFOX_TEST_PARALLEL_STEREO_ENCODING","STARFOX_TEST_DUPLICATE_STEREO_RAY_UPLOADS",
            "STARFOX_TEST_FAIL_STEREO_ENCODING_AFTER_LEFT","STARFOX_TEST_FAIL_STEREO_ENCODING_RIGHT",
            "STARFOX_TEST_SPLIT_STEREO_SUBMISSIONS","STARFOX_TEST_SERIAL_STEREO_ENCODING"};
        std::array<std::optional<std::string>,7> saved;
        Environment(){for(unsigned i=0;i<names.size();++i) {
            if(const auto* value=SDL_GetEnvironmentVariable(env,names[i]))saved[i]=value;
            require(SDL_UnsetEnvironmentVariable(env,names[i]),"Ray fixture environment");
        }}
        ~Environment(){for(unsigned i=0;i<names.size();++i) {
            if(saved[i])SDL_SetEnvironmentVariable(env,names[i],saved[i]->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,names[i]);
        }}
        void set(unsigned i,bool value){require(value?SDL_SetEnvironmentVariable(env,names[i],"1",true):
            SDL_UnsetEnvironmentVariable(env,names[i]),"Ray fixture policy");}
        void mode(unsigned value){set(0,value==1);set(1,value==2);}
    } environment;
    struct Transfer {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer{};
        ~Transfer(){SDL_WaitForGPUIdle(device);if(buffer)SDL_ReleaseGPUTransferBuffer(device,buffer);}
    } transfer{device};
    constexpr unsigned slot_size=2048;
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,slot_size*3,0};
    transfer.buffer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer.buffer,"Ray pair download");
    starfox::assets::Shape shape;shape.vertices={{80,60,-360},{-80,60,-440},{0,-70,-400}};
    shape.word_coordinates=std::vector<bool>(3,true);
    shape.faces={{-1,0,{0,0,127},{0,2,1}}};shape.colour_words={0x11};
    GpuModelDraw model;model.shape=&shape;model.pose.use_rotation_matrix=true;
    model.pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};
    model.pose.continuous_geometry=model.pose.subpixel_projection=true;
    model.pose.z=0;model.pose.vanish_x=48;model.pose.vanish_y=36;model.pose.force_colour=true;
    model.pose.forced_colour=0x11;model.ray_geometry=model.ray_materials=true;
    GpuStereoScene shared,duplicate,queued;
    const auto frame=[&] {
        auto second=model;second.pose.x=13;
        return std::array<GpuSceneDraw,2>{model,second};
    };
    const auto expected=[&](bool materials) {
        std::vector<std::uint8_t> bytes;
        for(unsigned eye=0;eye<2;++eye) {
            std::vector<Position> positions;std::vector<RayMaterial> records;
            for(double x:{0.,13.}) {
                // The fixed -32768 matrix negates source coordinates exactly.
                // This never reads prepared/GPU point or connectivity arrays.
                for(const auto& face:shape.faces)for(auto index:face.vertex_indices) {
                    const auto& p=shape.vertices[index];
                    positions.push_back({float(-p.x+x-(eye?3.2:-3.2)),float(-p.y),float(-p.z),1});
                }
                if(materials) {
                    const auto packed=pack_faces(shape,pack_bsp(shape),model.pose,model.settings);
                    std::vector<Triangle> corners;
                    for(unsigned f=0;f<packed.polygons.size();++f) {
                        const auto first=packed.polygons[f][0];corners.push_back({first,first+1,first+2,f});
                    }
                    RayMaterials oracle;require(pack_ray_materials(packed,corners,oracle),"Ray pair material oracle");
                    records.insert(records.end(),oracle.triangles.begin(),oracle.triangles.end());
                }
            }
            const auto append=[&](const void* data,std::size_t size) {
                const auto* first=static_cast<const std::uint8_t*>(data);bytes.insert(bytes.end(),first,first+size);
            };
            append(positions.data(),positions.size()*sizeof(Position));
            if(materials)append(records.data(),records.size()*sizeof(RayMaterial));
        }
        return bytes;
    };
    const auto encode_capture=[&](GpuStereoScene& owner,SDL_GPUCommandBuffer* command,unsigned slot,bool materials) {
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy,"Ray pair copy");
        unsigned offset=slot*slot_size;
        for(unsigned eye=0;eye<2;++eye) {
            const auto rays=owner.ray_geometry_output(eye);
            require(rays.complete && rays.buffer && rays.vertex_count==shape.faces.size()*6,"Independent eye ray geometry");
            require(rays.buffer!=owner.ray_geometry_output(1-eye).buffer,"Eye ray results alias");
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(rays.buffer),0,rays.vertex_count*16};
            SDL_GPUTransferBufferLocation to{transfer.buffer,offset};
            SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=from.size;
            if(materials) {
                require(rays.material_offset && rays.materials,"Independent eye materials absent");
                from.offset=rays.material_offset;from.size=rays.vertex_count/3*64;to.offset=offset;
                SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=from.size;
            } else require(!rays.material_offset,"Shadow-only ray allocated material output");
        }
        require(offset-slot*slot_size<slot_size,"Ray capture extent");SDL_EndGPUCopyPass(copy);
    };
    const auto verify=[&](const std::vector<std::uint8_t>& oracle,unsigned slot,bool materials) {
        const auto* data=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,transfer.buffer,false));
        require(data,"Ray pair mapping");data+=slot*slot_size;
        const auto vertices=unsigned(shape.faces.size()*6);
        const auto eye_stride=vertices*16+(materials?vertices/3*64:0);
        // Continuous eye subtraction has a small float rounding allowance;
        // source materials and their topology/ordering are byte-exact.
        for(unsigned eye=0;eye<2;++eye) {
            for(unsigned p=0;p<vertices;++p)for(unsigned c=0;c<4;++c) {
                float actual,wanted;const auto at=eye*eye_stride+p*16+c*4;
                std::memcpy(&actual,data+at,4);std::memcpy(&wanted,oracle.data()+at,4);
                if(!std::isfinite(actual) || std::abs(actual-wanted)>=.0001f)
                    throw std::runtime_error("Hand-transformed stereo ray vertex mismatch eye="+std::to_string(eye)
                        +" vertex="+std::to_string(p)+" channel="+std::to_string(c)
                        +" actual="+std::to_string(actual)+" expected="+std::to_string(wanted));
            }
            if(materials)require(std::memcmp(data+eye*eye_stride+vertices*16,
                oracle.data()+eye*eye_stride+vertices*16,vertices/3*64)==0,"Stereo ray material CPU oracle mismatch");
        }
        std::vector<std::uint8_t> result(data,data+oracle.size());SDL_UnmapGPUTransferBuffer(device,transfer.buffer);return result;
    };
    const auto capture=[&](GpuStereoScene& owner,const std::vector<std::uint8_t>& oracle,bool materials) {
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"Ray pair command");
        encode_capture(owner,command,0,materials);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"Ray pair submit");
        require(SDL_WaitForGPUFences(device,true,&fence,1),"Ray pair wait");SDL_ReleaseGPUFence(device,fence);
        return verify(oracle,0,materials);
    };
    unsigned pairs=0;
    for(unsigned mode=0;mode<3;++mode)for(unsigned round=0;round<4;++round) {
        environment.mode(mode);const bool materials=round!=3;model.ray_materials=materials;
        shape.vertices[0].x=80+int(round)*2;model.pose.forced_colour=std::uint8_t(0x11+round*0x11);
        shape.faces.resize(round%2?2:1,shape.faces.front());
        const auto draws=frame();const auto oracle=expected(materials);
        environment.set(2,true);require(duplicate.render_resident(device,96,72,draws,6.4,512),"Duplicated ray pair");
        require(duplicate.model_upload_info().ray_shared_bytes==0,"Duplicated ray policy still shared");
        const auto reference=capture(duplicate,oracle,materials);
        environment.set(2,false);require(shared.render_resident(device,96,72,draws,6.4,512),"Shared ray pair");
        const auto stats=shared.model_upload_info();
        require(stats.ray_shared_bytes>0 && stats.ray_shared_buffers==(materials?8U:4U)
            && stats.source_storage_bytes<=16U*1024*1024,"Ray packet accounting/shadow-only budget");
        require(capture(shared,oracle,materials)==reference,"Ray pair source policy changed result bytes");
        // After-left fault injection belongs to the joined/parallel owners;
        // the reference split path has already submitted its first eye.
        if(mode!=0) {
            environment.set(3,true);require(!shared.render_resident(device,96,72,draws,6.4,512),"Cancelled ray pair accepted");
            for(unsigned eye=0;eye<2;++eye)require(!shared.resident_output(eye).pixels
                && !shared.ray_geometry_output(eye).buffer,"Cancelled eye retained ray output");
            environment.set(3,false);require(shared.render_resident(device,96,72,draws,6.4,512),"Ray cancellation retry");
            require(capture(shared,oracle,materials)==reference,"Ray cancellation retry changed result");
        }
        if(mode==2) {
            environment.set(4,true);require(!shared.render_resident(device,96,72,draws,6.4,512),"Right ray failure accepted");
            environment.set(4,false);require(shared.render_resident(device,96,72,draws,6.4,512),"Right ray failure retry");
            require(capture(shared,oracle,materials)==reference,"Right ray failure retry changed result");
        }
        ++pairs;
    }
    // Owned commands: submit three pairs/snapshots before the diagnostic wait,
    // retaining the owner's existing bounded retirement. Reuse the SAME
    // source addresses and vary both topology and palette. Each queued copy
    // must retain the source backing encoded for that pair, not the newest one.
    environment.mode(0);model.ray_materials=true;
    std::array<SDL_GPUFence*,3> fences{};std::array<std::vector<std::uint8_t>,3> oracles;
    for(unsigned round=0;round<3;++round) {
        shape.vertices[0].x=91+int(round)*3;shape.faces.resize(2,shape.faces.front());
        shape.faces[1].vertex_indices=round%2?std::vector<std::uint8_t>{1,2,0}:std::vector<std::uint8_t>{0,2,1};
        model.pose.forced_colour=std::uint8_t(0x44+round*0x11);oracles[round]=expected(true);
        require(queued.render_resident(device,96,72,frame(),6.4,512),"Queued ray pair");
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"Queued ray command");
        require(queued.model_upload_info().ray_shared_buffers==8,"Queued connectivity not shared");
        encode_capture(queued,command,round,true);
        fences[round]=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fences[round],"Queued ray submit");
    }
    require(SDL_WaitForGPUFences(device,true,fences.data(),3),"Queued ray retirement");
    for(unsigned round=0;round<3;++round){verify(oracles[round],round,true);SDL_ReleaseGPUFence(device,fences[round]);}
    model.ray_geometry=false;require(shared.render_resident(device,96,72,frame(),6.4,512),"Ray-OFF pair");
    require(shared.model_upload_info().ray_shared_bytes==0 && shared.model_upload_info().ray_shared_buffers==0,"Ray-OFF extra connectivity");
    std::cout<<"Stereo ray uploads: "<<pairs<<" split/joined/parallel hand-geometry and independent material pairs; shadow-only/OFF, joined/parallel cancellation retry, right-eye failure and 3 queued mutable-source pairs passed\n";
}
