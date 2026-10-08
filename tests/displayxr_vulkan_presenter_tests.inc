// Included after the shared native-eye sources, in the Windows GPU fixture.
// Every device is created through its current XR instance. Reconnect must not
// bind an older ordinary SDL device to a fresh Vulkan2 runtime generation.
bool vulkan_use_integrated{};
struct VulkanFixture {
    DisplayXrRuntime runtime;DisplayXrVulkanBinding creation;SDL_GPUDevice* device{};
    VulkanFixture() {
        check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        mock=Mock{};mock.backend=DisplayXrBackend::vulkan;mock.image_width=128;mock.image_height=72;
        native_mock=NativeMock{};gpu_mock=GpuMock{};gpu_mock.enabled=true;gpu_mock.integrated=vulkan_use_integrated;
        gpu_mock.format=VK_FORMAT_R8G8B8A8_SRGB;
    }
    void create_device(const DisplayXrDesktop* desktop=nullptr,bool rays=false) {
        const auto props=SDL_CreateProperties();check(props,"Vulkan device properties failed");
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
        SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
        SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");
        const bool configured=desktop?desktop->configure_gpu_properties(props):creation.configure_gpu_properties(props);
        check(configured,"Vulkan2 creation properties failed");
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
        if(rays) check(shadows::request_vulkan_ray_query(props),"Native Vulkan ray-query properties failed");
#else
        if(rays) check(shadows::SdlDxrShadows::request_vulkan_interop(props),"Vulkan native-ray properties failed");
#endif
        device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);check(device,SDL_GetError());
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
        // Only the successful required-feature attempt gets this marker. An
        // ordinary XR-created device remains explicitly unavailable for RT.
        check(SDL_SetBooleanProperty(SDL_GetGPUDeviceProperties(device),"starfox.vulkan.ray_query.enabled",rays),"Native ray feature marker failed");
        check(shadows::query_vulkan_ray_query(device).available==rays,"Native Vulkan feature selection/availability disagrees");
#endif
        gpu_mock.vk_native=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(
            SDL_GetGPUDeviceProperties(device),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
        check(gpu_mock.vk_native && gpu_mock.vk_native->device==gpu_mock.vk_device,"Vulkan native binding not preserved");
    }
    void initialize(bool rays=false) {
        const bool discovered=runtime.initialize_with_api(get,DisplayXrBackend::vulkan);
        check(discovered,std::string(runtime.status()).c_str());
        check(creation.initialize(runtime),creation.status().c_str());create_device(nullptr,rays);
    }
    ~VulkanFixture() {
        // Fixture allocations refer to the SDL device, including compositor
        // resources retained past xrDestroySwapchain. Retire them first.
        if(device) SDL_WaitForGPUIdle(device);
        gpu_mock=GpuMock{};creation.close();runtime.close();
        if(device) SDL_DestroyGPUDevice(device);
        SDL_Quit();
    }
};
struct VulkanQueueGate {
    std::unique_ptr<VulkanObject> command;bool released{};
    explicit VulkanQueueGate(SDL_GPUDevice* device) {
        command=vulkan_command();VkEventCreateInfo event{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
        check(vulkan_proc<PFN_vkCreateEvent>("vkCreateEvent")(command->device,&event,nullptr,&command->event)==VK_SUCCESS,"Vulkan stall event failed");
        vulkan_proc<PFN_vkCmdWaitEvents>("vkCmdWaitEvents")(command->command,1,&command->event,
            VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,nullptr,0,nullptr,0,nullptr);
        check(vulkan_proc<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(command->command)==VK_SUCCESS,"Vulkan stall end failed");
        const auto* bridge=static_cast<const StarfoxSdlVulkanXrBridgeV1*>(SDL_GetPointerProperty(
            SDL_GetGPUDeviceProperties(device),STARFOX_SDL_VULKAN_XR_BRIDGE,nullptr));
        const auto submit=vulkan_proc<PFN_vkQueueSubmit>("vkQueueSubmit");
        struct Call {VulkanObject* command;PFN_vkQueueSubmit submit;};Call call{command.get(),submit};
        check(bridge && bridge->with_queue(device,[](void* user,VkQueue queue) {
            auto& c=*static_cast<Call*>(user);VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            info.commandBufferCount=1;info.pCommandBuffers=&c.command->command;
            return c.submit(queue,1,&info,c.command->fence)==VK_SUCCESS;
        },&call),"Vulkan stall submit failed");gpu_blocked=true;
    }
    void unblock() {
        check(vulkan_proc<PFN_vkSetEvent>("vkSetEvent")(command->device,command->event)==VK_SUCCESS,"Vulkan stall release failed");
        gpu_blocked=false;released=true;vulkan_wait(*command);
    }
    ~VulkanQueueGate() {
        if(!released) {
            reinterpret_cast<PFN_vkSetEvent>(command->get(command->device,"vkSetEvent"))(command->device,command->event);
            gpu_blocked=false;
            reinterpret_cast<PFN_vkWaitForFences>(command->get(command->device,"vkWaitForFences"))(
                command->device,1,&command->fence,VK_TRUE,5000000000ULL);
        }
    }
};
void gpu_vulkan_presenter_tests(const char* rom_path,const char* symbols_path) {
    auto* panel_handle=reinterpret_cast<void*>(std::uintptr_t(0x3000));unsigned exact_frames{};
    for(auto format:{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM}) {
        VulkanFixture gpu;gpu.initialize();gpu_mock.format=format;DisplayXrD3D12Presenter presenter;
        check(presenter.initialize(gpu.runtime,gpu.device,panel_handle,&gpu.creation),presenter.status().c_str());
        check(!presenter.running() && presenter.eye_extent(0)==std::array<std::uint32_t,2>{128,72},"Vulkan extent/READY invalid");
        check(presenter.poll_events() && presenter.running(),"Vulkan presenter did not become READY");gpu_mock.eyes[1].next=1;
        for(unsigned i=0;i<4;++i) {
            native_mock.head_x=12+float(i)*7;const auto frame=presenter.begin_frame(native_mock.expected);
            check(frame && frame->calibrated.xr.should_render && frame->cameras[0].projection[5]>0
                && frame->cameras[0].projection[9]==-frame->calibrated.cameras[0].projection[9]
                && frame->cameras[0].view==frame->calibrated.cameras[0].view,"Vulkan calibrated camera/Y changed");
            EyeSources eyes(gpu.device);eyes.initialize(presenter,i);
            check(finish_presenter(presenter,presenter.submit_frame(eyes.textures[0],eyes.textures[1]))==DisplayXrSubmission::submitted,
                presenter.status().c_str());eyes.verify_compositor_pixels();++exact_frames;
        }
        if(format==VK_FORMAT_R8G8B8A8_SRGB || format==VK_FORMAT_R8G8B8A8_UNORM) for(float head:{-30.F,24.F}) {
            native_mock.head_x=head;const auto frame=presenter.begin_frame(native_mock.expected);check(frame.has_value(),"Vulkan geometry frame failed");
            EyeSources eyes(gpu.device);eyes.initialize(presenter);eyes.render_calibrated_geometry(presenter,*frame);
            check(finish_presenter(presenter,presenter.submit_frame(eyes.textures[0],eyes.textures[1]))==DisplayXrSubmission::submitted,
                presenter.status().c_str());eyes.verify_compositor_pixels();
        }
        check(presenter.close() && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys
            && native_mock.session_creates==native_mock.session_destroys,"Vulkan format teardown leaked");
    }
    for(unsigned mode=0;mode<4;++mode) {
        VulkanFixture gpu;gpu.initialize();DisplayXrD3D12Presenter presenter;
        check(presenter.initialize(gpu.runtime,gpu.device,panel_handle,&gpu.creation) && presenter.poll_events(),presenter.status().c_str());
        check(presenter.begin_frame(native_mock.expected).has_value(),"Vulkan wait frame failed");EyeSources eyes(gpu.device);eyes.initialize(presenter);
        if(mode==0) {
            gpu_mock.timeout_right=true;
            for(unsigned i=0;i<3;++i) check(presenter.submit_frame(eyes.textures[0],eyes.textures[1])==DisplayXrSubmission::waiting
                && gpu_mock.eyes[0].acquires==1 && gpu_mock.eyes[1].acquires==1 && !gpu_mock.projections,"Vulkan timeout reacquired/rendered eyes");
            gpu_mock.timeout_right=false;
            check(finish_presenter(presenter,presenter.continue_frame(eyes.textures[0],eyes.textures[1]))==DisplayXrSubmission::submitted,
                presenter.status().c_str());eyes.verify_compositor_pixels();
        } else {
            VulkanQueueGate gate(gpu.device);
            if(mode==3) presenter.retain_scene_submission(nullptr);
            else check(presenter.submit_frame(eyes.textures[0],eyes.textures[1])==DisplayXrSubmission::waiting,"Vulkan queue stall not held");
            const auto started=std::chrono::steady_clock::now();
            if(mode==1) check(presenter.cancel_frame()==DisplayXrSubmission::waiting && presenter.frame_pending(),"Vulkan cancellation freed live copy");
            else for(unsigned i=0;i<3;++i) check(!presenter.try_close() && !gpu_mock.swapchain_destroys,"Vulkan close released uncertain/live work");
            check(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(100)
                && !gpu_mock.projections && !gpu_mock.eyes[0].releases && !gpu_mock.eyes[1].releases,"Vulkan live recovery blocked/submitted early");
            gate.unblock();
            if(mode==1) check(finish_presenter(presenter,DisplayXrSubmission::waiting)==DisplayXrSubmission::submitted
                && !presenter.frame_pending() && native_mock.last_layers==0,"Vulkan cancellation submitted a layer");
            else {
                bool closed=false;for(unsigned i=0;!closed && i<3000;++i) {closed=presenter.try_close();if(!closed) SDL_Delay(1);}
                check(closed && native_mock.last_layers==0 && !gpu_mock.projections,"Vulkan queued teardown did not drain");
            }
        }
        check(presenter.close() && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,"Vulkan wait cleanup leaked");
    }
    for(unsigned failure=0;failure<7;++failure) {
        VulkanFixture gpu;gpu.initialize();DisplayXrD3D12Presenter presenter;
        if(failure==0) mock.missing="xrReleaseSwapchainImage";
        if(failure==1) gpu_mock.fail_create=1;
        if(failure==2) gpu_mock.fail_images=1;
        if(failure==3) gpu_mock.bad_image=true;
        if(failure==4) gpu_mock.format=VK_FORMAT_R32_SFLOAT;
        if(failure==5) native_mock.ignore_request=true;
        check(!presenter.initialize(gpu.runtime,gpu.device,failure==6?nullptr:panel_handle,&gpu.creation)
            && !native_mock.session_open && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,"Vulkan failed init leaked resources");
        mock.missing=nullptr;gpu_mock.fail_create=gpu_mock.fail_images=-1;gpu_mock.bad_image=false;
        gpu_mock.format=VK_FORMAT_R8G8B8A8_SRGB;native_mock.ignore_request=false;
        check(presenter.initialize(gpu.runtime,gpu.device,panel_handle,&gpu.creation) && presenter.close(),"Vulkan init recovery failed");
    }
    for(unsigned failure=0;failure<6;++failure) {
        VulkanFixture gpu;gpu.initialize();DisplayXrD3D12Presenter presenter;
        check(presenter.initialize(gpu.runtime,gpu.device,panel_handle,&gpu.creation) && presenter.poll_events(),presenter.status().c_str());
        if(failure==0) {
            native_mock.bad_raw=true;const auto frame=presenter.begin_frame(native_mock.expected);
            check(frame && !frame->calibrated.xr.should_render && !presenter.frame_pending() && !native_mock.last_layers,"Vulkan invalid tracking rendered");
        } else {
            check(presenter.begin_frame(native_mock.expected).has_value(),"Vulkan failure frame failed");EyeSources eyes(gpu.device);eyes.initialize(presenter,0,failure==1);
            if(failure==2) gpu_mock.bad_index=true;if(failure==3) gpu_mock.fail_release=true;
            if(failure==4) native_mock.active=false;if(failure==5) gpu_mock.fail_end=true;
            check(finish_presenter(presenter,presenter.submit_frame(eyes.textures[0],eyes.textures[1]))==DisplayXrSubmission::failed
                && !presenter.frame_pending(),"Vulkan partial frame accepted");
            if(failure==5) eyes.verify_compositor_pixels();else check(!native_mock.last_layers && !gpu_mock.projections,"Vulkan failed eye submitted a layer");
        }
        check(presenter.close() && gpu_mock.swapchain_creates==gpu_mock.swapchain_destroys,"Vulkan failed frame cleanup leaked");
    }
    for(auto owner_format:{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM}) {
        // Four-format independent synthetic oracles, plus one actual-cartridge
        // ray/environment run. Transport/sample guides cover every format.
        if(rom_path && owner_format!=VK_FORMAT_R8G8B8A8_SRGB) continue;
        const bool ray_fixture=rom_path || native_ground_only || native_taa_water_only || native_taa_liquid_post_only || native_fsr_only || native_taa_reflection_only;
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
        const bool test_rays=ray_fixture; // Both native Vulkan GPUs, no DXR dependency.
#else
        const bool test_rays=ray_fixture && !vulkan_use_integrated;
#endif
        VulkanFixture gpu;gpu_mock.format=owner_format;
        if(native_taa_pattern_only) {mock.image_width=192;mock.image_height=448;}
        if(native_taa_liquid_post_only) {mock.image_width=256;mock.image_height=224;}
        // Runtime discovery caches the recommended extent. Set the pattern
        // fixture's 2x material grid BEFORE discovery, as in D3D12.
        gpu.initialize(test_rays);native_mock.expected=calibrated_game_rig();native_mock.head_x=0;native_mock.pose_scale=1.F/256.F;
        if(native_taa_pattern_only) std::cout<<"Native pattern adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu.device),
            SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<" driver="<<SDL_GetGPUDeviceDriver(gpu.device)<<std::endl;
        DisplayXrGameRenderer renderer;check(renderer.initialize(gpu.runtime,gpu.device,panel_handle,&gpu.creation) && renderer.poll_events(),renderer.status().c_str());
        std::shared_ptr<const CalibratedGameFrame> source;
        if(rom_path && symbols_path) {
            const auto rom=starfox::assets::RomImage::load(rom_path);const auto symbols=starfox::assets::SymbolMap::load(symbols_path);
            starfox::simulation::GameSimulation game(rom,symbols,"LEVEL1_1",{},true);game.set_god_mode(true);starfox::audio::Spc700Audio audio;
            for(unsigned i=0;i<700;++i) {const auto tick=game.tick({});(void)audio.render_logic_tick(tick.audio_port_writes);game.synchronize_apu_output_ports(audio.output_ports());}
            starfox::vr::GameSceneHistory history(game,rom,symbols);CalibratedGameScene assembler(rom,symbols);history.capture();
            CalibratedGameSettings settings;settings.srgb=renderer.srgb_target();
            if(native_scene_fx_only) {settings.scene_enhancements=255;settings.particle_enhancements=15;}
            if(native_fog_only) settings.volumetric_fog=3;
            source=std::make_shared<const CalibratedGameFrame>(assembler.assemble(history.previous(),history.current(),game,1,settings));
            if(native_scene_fx_only) check(source->scene_fx.active(),"Real cartridge did not capture native event emitters");
        } else {
            auto frame=std::make_shared<CalibratedGameFrame>();frame->previous=frame->current=std::make_shared<starfox::vr::GameSceneSnapshot>();
            frame->settings.srgb=renderer.srgb_target();frame->clear={.07F,.11F,.18F,1};starfox::vr::DrawPacket model;
            constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
            // Slanted, asymmetric edges cross many pixel centres, so the
            // intentionally small camera-response rotation remains observable
            // at this fixture's 128x72 extent (an axis-aligned solid quad can
            // quantize to the identical image despite correctly moving).
            for(auto corner:corners) {starfox::vr::SceneVertex v{};
                v.position[0]=corner[0]*.31F+corner[1]*.073F+.17F;
                v.position[1]=corner[1]*.29F+corner[0]*.137F-.08F;
                v.position[2]=-1.6F+corner[0]*.087F;
                v.color[0]=v.color[3]=v.odd_color[0]=v.odd_color[3]=1;model.geometry.vertices.push_back(v);}
            frame->draws.push_back({std::move(model),CalibratedGameLayer::model,starfox::vr::SceneBlend::opaque,true});source=frame;
        }
        const auto render=[&](std::shared_ptr<const CalibratedGameFrame> frame) {
            auto result=renderer.submit(std::move(frame));for(unsigned i=0;result==DisplayXrSubmission::waiting && i<3000;++i) {SDL_Delay(1);result=renderer.poll();}
            check(result==DisplayXrSubmission::submitted,renderer.status().c_str());return vulkan_read_pixels();
        };
        const auto baseline=render(source);check(baseline[0]!=baseline[1],"Vulkan game eyes collapsed to a flat screen");
        if(native_taa_only || native_scene_fx_only || native_fog_only || native_blur_only || native_ground_only || native_fsr_only) {
            std::shared_ptr<const CalibratedGameFrame> effects;
            if(native_fsr_only) effects=native_fsr_owner_oracle(render,renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_ground_only) effects=native_ground_owner_oracle(render,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_blur_only) effects=native_blur_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_fog_only) effects=native_volumetric_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_scene_fx_only) effects=native_scene_fx_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_taa_reflection_only) effects=native_reflection_owner_checks(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_taa_ground_clock_only) native_taa_ground_clock_owner_oracle(render,renderer.srgb_target(),"Vulkan");
            else if(native_taa_local_layers_only) native_taa_local_layers_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_taa_edge_only) native_taa_edge_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_taa_liquid_post_only) effects=native_liquid_post_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),"Vulkan");
            else if(native_taa_reconstructed_post_only) native_reconstructed_post_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),"Vulkan");
            else if(native_taa_pattern_only) native_taa_pattern_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else if(native_taa_palette_only) native_taa_palette_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
            else native_taa_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan",native_taa_water_only);
            auto held=std::make_shared<CalibratedGameFrame>(*source);held->settings.aa_type=4;held->settings.aa_quality=3;
            if(effects) held=std::make_shared<CalibratedGameFrame>(*effects);
            const auto wait_seed=held;
            if(native_blur_only) {held=std::make_shared<CalibratedGameFrame>(*held);held->presentation_seconds+=1./120;held->draws[1].packet.model[12]+=.13F;}
            const auto expected=render(held);
            if(native_blur_only) (void)render(wait_seed);
            {
                VulkanQueueGate gate(gpu.device);check(renderer.submit(held)==DisplayXrSubmission::waiting,"Vulkan TAA GPU wait lost eye targets");
                const auto locates=native_mock.locates;
                check(renderer.poll()==DisplayXrSubmission::waiting && renderer.retained_frame()==held
                    && native_mock.locates==locates,"Vulkan TAA GPU retry reacquired calibrated poses");
                gate.unblock();auto result=renderer.poll();
                for(unsigned attempt=0;result==DisplayXrSubmission::waiting && attempt<10000;++attempt) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::submitted && vulkan_read_pixels()==expected,"Vulkan TAA GPU retry changed held eyes");
            }
            {
                auto cancelled=held;
                if(native_blur_only) {cancelled=std::make_shared<CalibratedGameFrame>(*held);cancelled->presentation_seconds+=1./120;cancelled->draws[1].packet.model[12]+=.13F;}
                VulkanQueueGate gate(gpu.device);check(renderer.submit(cancelled)==DisplayXrSubmission::waiting,"Vulkan native effect close did not retain pending work");
                check(!renderer.try_close() && renderer.retained_frame()==cancelled,"Vulkan native effect close freed live source/history");
                gate.unblock();bool closed=false;
                for(unsigned attempt=0;!closed && attempt<10000;++attempt) {closed=renderer.try_close();if(!closed) SDL_Delay(1);}
                check(closed && !renderer.retained_frame() && !renderer.frame_pending(),"Vulkan TAA cancellation did not retire images");
            }
            std::cout<<"Vulkan TAA owner: real queue stall/retry and in-flight nonblocking cancellation preserved poses/source/history until retirement.\n";
            check(renderer.close(),renderer.status().c_str());continue;
        }
        bool aa_changed=false;
        for(unsigned type:{0U,1U,2U,5U}) for(unsigned quality:{1U,2U,3U}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*source);selected->settings.aa_type=type;selected->settings.aa_quality=quality;
            const auto actual=render(selected);check(actual[0]!=actual[1],"Vulkan AA collapsed calibrated eyes");
            aa_changed|=actual!=baseline;
            for(unsigned e=0;e<2;++e) for(unsigned at=3;at<actual[e].size();at+=4) check(actual[e][at]==baseline[e][at],"Vulkan AA changed alpha");
            check(render(selected)==actual,"Held Vulkan AA resampled/jittered");
            if(type==5 && quality==3) {
                gpu_mock.timeout_right=true;check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Vulkan AA wait lost source");
                check(renderer.poll()==DisplayXrSubmission::waiting,"Vulkan AA wait reacquired eyes");gpu_mock.timeout_right=false;
                auto result=renderer.poll();for(unsigned i=0;result==DisplayXrSubmission::waiting && i<3000;++i) {SDL_Delay(1);result=renderer.poll();}
                check(result==DisplayXrSubmission::submitted && vulkan_read_pixels()==actual,"Vulkan AA retry changed retained output");
                selected->settings.depth_enhancements=15;selected->settings.contrast=1;selected->settings.bloom_model=2;
                (void)render(selected);
            }
            check(render(source)==baseline,"Vulkan AA OFF did not restore exact source");
        }
        if(rom_path && symbols_path) check(aa_changed,"Vulkan native AA never changed the cartridge eye image");
        std::cout<<"Vulkan native AA owner: 4 spatial algorithms/3 qualities, actual independent eyes, exact alpha, retained images/held waits and OFF passed.\n";
        bool ssaa_changed=false;
        for(unsigned quality:{1U,2U,3U}) {
            auto selected=std::make_shared<CalibratedGameFrame>(*source);selected->settings.aa_type=3;selected->settings.aa_quality=quality;
            const auto actual=render(selected);ssaa_changed|=actual!=baseline;
            check(actual[0]!=actual[1],"Vulkan SSAA collapsed calibrated eyes");
            for(unsigned e=0;e<2;++e) for(unsigned at=3;at<actual[e].size();at+=4) check(actual[e][at]==baseline[e][at],"Vulkan SSAA changed alpha");
            check(render(selected)==actual,"Held Vulkan SSAA jittered");
            gpu_mock.timeout_right=true;check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,"Vulkan SSAA wait lost source");
            check(renderer.poll()==DisplayXrSubmission::waiting,"Vulkan SSAA retry reacquired eyes");gpu_mock.timeout_right=false;
            auto result=renderer.poll();for(unsigned i=0;result==DisplayXrSubmission::waiting && i<3000;++i) {SDL_Delay(1);result=renderer.poll();}
            check(result==DisplayXrSubmission::submitted && vulkan_read_pixels()==actual,"Vulkan SSAA retry changed retained output");
            if(quality==2) {
                selected->settings.depth_enhancements=15;selected->settings.contrast=2;selected->settings.bloom_model=2;
                if(test_rays) selected->settings.ray_tracing=selected->settings.reflections=2;
                (void)render(selected);
            }
            check(render(source)==baseline,"Vulkan SSAA OFF failed exact extent/image restoration");
        }
        if(rom_path && symbols_path) check(ssaa_changed,"Vulkan SSAA never changed the cartridge eye image");
        std::cout<<"Vulkan native SSAA owner: 4/9/16 raster samples, actual eyes, alpha, held/image waits, combined depth/appearance and OFF passed.\n";
        native_msaa_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,source,test_rays,"Vulkan");
        native_taa_owner_oracle(render,[]{return vulkan_read_pixels();},renderer,renderer.srgb_target(),test_rays,"Vulkan");
        bool depth_changed=false;
        for(unsigned modes=1;modes<16;++modes) {
            auto selected=std::make_shared<CalibratedGameFrame>(*source);selected->settings.depth_enhancements=modes;
            const auto actual=render(selected);depth_changed|=actual!=baseline;
            check(actual[0]!=actual[1],"Vulkan depth effects collapsed native calibrated eyes");
            for(unsigned e=0;e<2;++e) for(unsigned at=3;at<baseline[e].size();at+=4)
                check(actual[e][at]==baseline[e][at],"Vulkan depth effects changed source alpha");
            check(render(selected)==actual,"Held Vulkan depth effect resampled/jittered");
            if(modes==15) {
                gpu_mock.timeout_right=true;
                check(renderer.submit(selected)==DisplayXrSubmission::waiting && renderer.retained_frame()==selected,
                    "Vulkan depth image wait lost source/guide state");
                check(renderer.poll()==DisplayXrSubmission::waiting,"Vulkan depth image retry reacquired eyes");
                gpu_mock.timeout_right=false;
                auto outcome=renderer.poll();for(unsigned i=0;outcome==DisplayXrSubmission::waiting && i<3000;++i) {SDL_Delay(1);outcome=renderer.poll();}
                check(outcome==DisplayXrSubmission::submitted && vulkan_read_pixels()==actual,"Vulkan depth image wait changed retained output");
            }
            check(render(source)==baseline,"Vulkan depth OFF did not restore exact source output");
        }
        if(rom_path && symbols_path) check(depth_changed,"Vulkan cartridge native depth controls had no visible effect");
        std::cout<<"Vulkan native AO/DOF owner: all 15 active mode pairs, retained actual eyes, exact alpha, held phase/image retries and OFF passed.\n";
        bool camera_changed=false;
        for(unsigned scope:{0U,1U,2U}) for(unsigned quality:{1U,2U,3U}) for(bool combined:{false,true}) {
            CameraResponse tracker;const unsigned modes=quality<<(scope*2);
            (void)tracker.update(0,1,modes,100,0,1);(void)tracker.update(.1,1,modes,68,1,1);
            const auto pose=tracker.update(.15,1,modes,68,1,1);
            auto frame=std::make_shared<CalibratedGameFrame>(*source);
            frame->settings.camera_response_modes=modes;frame->settings.camera_response_pose={pose.pitch,pose.yaw,pose.roll};
            if(combined) {
                frame->settings.bloom_model=3;frame->settings.bloom_world=2;
                frame->settings.contrast=frame->settings.chromatic=2;frame->settings.global_enhancements=global_enhancement_mask;
                if(test_rays) {frame->settings.ray_tracing=frame->settings.reflections=3;frame->settings.material=unsigned(Effect::gold_metal);}
            }
            const auto actual=render(frame);
            check(actual==render(response_reference_frame(frame)),"Vulkan camera response differs from independent world/model Euler oracle");
            check(actual[0]!=actual[1] && render(frame)==actual,"Vulkan camera response collapsed eyes or jittered held pose");
            camera_changed|=!combined && actual!=baseline;
            check(render(source)==baseline,"Vulkan camera response OFF failed exact restoration");
        }
        check(camera_changed,"Vulkan native camera response never moved the world");
        std::cout<<"Vulkan owner camera response: impact/recoil/bank LOW/MED/HIGH, independent native Euler oracle, preserved tracked views/UI, combined appearance/global/bloom and native rays/material/environment, held phase and exact OFF passed.\n";
        if(test_rays) {
            for(unsigned quality:{1U,2U,3U}) {
                auto frame=std::make_shared<CalibratedGameFrame>(*source);frame->settings.ray_tracing=frame->settings.reflections=quality;
                const auto actual=render(frame);check(actual!=baseline && actual[0]!=actual[1],"Vulkan native RT/reflections bypassed models/eyes");
            }
            auto traced=std::make_shared<CalibratedGameFrame>(*source);traced->settings.ray_tracing=traced->settings.reflections=3;
            const auto dielectric=render(traced);
            for(auto material:effect_order) if(starfox::render::material(material)) {
                auto frame=std::make_shared<CalibratedGameFrame>(*traced);frame->settings.material=unsigned(material);
                const auto actual=render(frame);check(actual!=dielectric && actual[0]!=actual[1],"Vulkan native material bypassed actual model");
                for(unsigned eye=0;eye<2;++eye) for(unsigned at=3;at<baseline[eye].size();at+=4)
                    check(actual[eye][at]==dielectric[eye][at],"Vulkan reflective material changed opacity");
            }
            check(render(traced)==dielectric,"Vulkan material OFF failed exact restoration");
            std::cout<<"Vulkan owner native ray/reflection quality and all reflective material tests passed on the real Windows interop queues.\n";
        }
        for(unsigned style=0;style<effect_count;++style) if(calibrated_composite_effect(style)) {
            auto frame=std::make_shared<CalibratedGameFrame>(*source);frame->settings.world_effects={style,65,0,0};frame->settings.model_effects={style,100,0,0};frame->settings.effect_seconds=.375F;
            const auto actual=render(frame);
            if(actual==baseline || actual[0]==actual[1]) std::cerr<<"Vulkan style="<<style<<" unchanged="<<(actual==baseline)<<" flat="<<(actual[0]==actual[1])<<'\n';
            // A solid synthetic red quad can legitimately be a fixed point of
            // a palette transform. The varied real cartridge must change.
            check((!rom_path || actual!=baseline) && actual[0]!=actual[1],"Vulkan style bypassed scene/collapsed eyes");
            for(unsigned eye=0;eye<2;++eye) for(unsigned at=3;at<baseline[eye].size();at+=4)
                check(actual[eye][at]==baseline[eye][at],"Vulkan style changed source alpha");
        }
        check(render(source)==baseline,"Vulkan effect OFF did not restore exact pixels");
        for(unsigned quality:{1U,2U,3U}) {
            auto frame=std::make_shared<CalibratedGameFrame>(*source);frame->settings.contrast=frame->settings.chromatic=quality;
            frame->settings.bloom_model=frame->settings.bloom_world=quality;
            const auto actual=render(frame);check(actual!=baseline && actual[0]!=actual[1],"Vulkan appearance/bloom bypassed native eyes");
        }
        for(unsigned quality:{1U,2U,3U}) {
            auto frame=std::make_shared<CalibratedGameFrame>(*source);frame->settings.exposure=quality;
            frame->settings.history_epoch=100+quality;frame->settings.effect_seconds=0;
            check(render(frame)==baseline,"Vulkan exposure scene cut did not start neutral");
            frame->settings.effect_seconds=.2F;(void)render(frame);frame->settings.effect_seconds=.4F;
            const auto adapted=render(frame);check(adapted!=baseline && adapted[0]!=adapted[1],"Vulkan owner exposure did not adapt independent eyes");
            frame->settings.exposure_paused=true;frame->settings.effect_seconds=.5F;
            check(render(frame)==adapted,"Vulkan owner exposure adapted/reset on pause");
            check(render(source)==baseline,"Vulkan exposure OFF did not restore exact pixels");
        }
        for(unsigned algorithm:{3U,6U}) for(bool image_wait:{false,true}) {
            gpu_mock.timeout_right=image_wait;VulkanQueueGate gate(gpu.device);
            auto exposed=std::make_shared<CalibratedGameFrame>(*source);exposed->settings.exposure=3;
            exposed->settings.aa_type=algorithm;exposed->settings.aa_quality=3;
            check(renderer.submit(exposed)==DisplayXrSubmission::waiting && renderer.retained_frame()==exposed,"Vulkan owner did not retain pending source");
            const auto destroyed=gpu_mock.swapchain_destroys,projections=gpu_mock.projections;const auto started=std::chrono::steady_clock::now();
            for(unsigned i=0;i<3;++i) check(!renderer.try_close() && renderer.retained_frame()==exposed && gpu_mock.swapchain_destroys==destroyed
                && gpu_mock.projections==projections,"Vulkan owner destroyed pending source/image");
            check(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(100),"Vulkan owner live cleanup blocked");gate.unblock();
            bool closed=false;for(unsigned i=0;!closed && i<3000;++i) {closed=renderer.try_close();if(!closed) SDL_Delay(1);}
            check(closed && !renderer.retained_frame() && !native_mock.last_layers && gpu_mock.projections==projections,"Vulkan owner retirement submitted stale frame");
            gpu_mock.timeout_right=false;
            check(renderer.initialize(gpu.runtime,gpu.device,panel_handle,&gpu.creation) && renderer.poll_events(),renderer.status().c_str());
        }
        check(renderer.close(),"Vulkan game cleanup failed");
    }
    // Real desktop HWND placement plus generation-safe runtime/device recovery.
    DisplayXrDesktop desktop;
    for(unsigned generation=0;generation<2;++generation) {
        VulkanFixture gpu;
        struct DesktopCleanup {DisplayXrDesktop& desktop;~DesktopCleanup() {desktop.close();}} cleanup{desktop};
        check(desktop.prepare_with_api(get,DisplayXrBackend::vulkan),desktop.status().c_str());gpu.create_device(&desktop);
        auto* window=SDL_CreateWindow("Leia Vulkan desktop fixture",320,240,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);check(window,SDL_GetError());
        int x{},y{};SDL_GetWindowPosition(window,&x,&y);native_mock.expected=calibrated_game_rig();native_mock.head_x=0;native_mock.pose_scale=1.F/256.F;
        check(desktop.attach(window,gpu.device) && desktop.active() && desktop.generation()==generation+1,desktop.status().c_str());
        auto frame=std::make_shared<CalibratedGameFrame>();frame->previous=frame->current=std::make_shared<starfox::vr::GameSceneSnapshot>();
        frame->settings.srgb=desktop.srgb_target();frame->clear={.25F,.25F,.25F,1};Framebuffer ui(256,224);ui.set(128,112,1);
        std::array<Rgba8,256> palette{};palette[1]={255,220,64,255};append_calibrated_host_ui(*frame,ui,palette,15,std::array<int,4>{12,20,244,223});
        gpu_mock.timeout_right=true;check(desktop.present(frame) && desktop.frame_pending() && !desktop.presented_frames(),desktop.status().c_str());
        check(desktop.present(nullptr) && gpu_mock.eyes[0].acquires==1 && gpu_mock.eyes[1].acquires==1,"Vulkan desktop replaced waiting UI");gpu_mock.timeout_right=false;
        for(unsigned i=0;desktop.frame_pending() && i<3000;++i) {SDL_Delay(1);check(desktop.present(nullptr),desktop.status().c_str());}
        check(!desktop.frame_pending() && desktop.presented_frames()==1,"Vulkan desktop miscounted wait/visible frames");
        const auto pixels=vulkan_read_pixels();check(!pixels[0].empty() && !pixels[1].empty(),"Vulkan desktop UI omitted");
        native_mock.active=false;check(!desktop.present(frame) && desktop.try_close() && !desktop.active(),"Vulkan desktop disconnect failed");
        int rx{},ry{},rw{},rh{};check(SDL_GetWindowPosition(window,&rx,&ry) && SDL_GetWindowSize(window,&rw,&rh)
            && rx==x && ry==y && rw==320 && rh==240,"Vulkan desktop did not restore ordinary window");
        // Reconnect discovers a fresh XR instance. An older SDL device is not
        // legal for that new Vulkan2 binding, even on the same physical GPU.
        check(desktop.prepare_with_api(get,DisplayXrBackend::vulkan),desktop.status().c_str());
        check(!desktop.attach(window,gpu.device) && desktop.generation()==generation+1
            && !desktop.active() && desktop.try_close(),"Vulkan reconnect accepted a foreign-generation device");
        SDL_DestroyWindow(window);
    }
    std::cout<<"Native Vulkan presenter: "<<exact_frames<<" byte-exact RGBA/BGRA linear/sRGB transports, 4 calibrated geometry/compute-ground frames; real queue stalls, image waits, cancellation, uncertain submissions, failure recovery, native game effects and desktop recovery passed. XR compositor/panel mocked, not hardware acceptance.\n";
}
