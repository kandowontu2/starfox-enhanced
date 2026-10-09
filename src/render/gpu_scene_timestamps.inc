// Optional diagnostics only. Include inside the SDL scene implementation.
// Query slots are reused only after their exact SDL submission fence retires,
// never on availability alone (a queued reset can still expose old results).
class SceneGpuTimestamps {
public:
    explicit SceneGpuTimestamps(const char* environment="STARFOX_TRACE_SCENE_GPU_TIMESTAMPS",
        const char* label="scene"):environment_(environment),label_(label) {}
    int begin(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command,std::size_t draws) noexcept {
        if(!std::getenv(environment_)) return -1;
        try {
            if(!state_) state_=std::make_unique<State>(device,label_);
            return state_->begin(command,draws);
        } catch(...) {return -1;} // Diagnostics must never fail rendering.
    }
    void end(int ticket,SDL_GPUCommandBuffer* command) noexcept {
        if(state_ && ticket>=0) state_->end(ticket,command);
    }
    void mark(int ticket,SDL_GPUCommandBuffer* command) noexcept {
        if(state_ && ticket>=0) state_->mark(ticket,command);
    }
    void annotate_draw(int ticket,std::uint64_t batch,unsigned index,unsigned kind) noexcept {
        if(state_ && ticket>=0) state_->slots[unsigned(ticket)].draw={batch,index,kind};
    }
    bool active() const noexcept {return bool(state_);}
    void submitted(int ticket,SDL_GPUFence* fence) noexcept {
        if(state_ && ticket>=0) state_->slots[unsigned(ticket)].fence=fence;
    }
    void cancel(int ticket) noexcept {
        if(state_ && ticket>=0) {state_->slots[unsigned(ticket)]={};++state_->cancelled;}
    }
    // Called AFTER an existing successful SDL fence wait, BEFORE fence release.
    void retired(SDL_GPUFence* fence) noexcept {if(state_) state_->retired(fence);}
    // Called only after all owned submissions retire, while the device lives.
    void release() noexcept {state_.reset();}
private:
    struct Draw {std::uint64_t batch{};unsigned index{},kind{};};
    struct Slot {SDL_GPUFence* fence{};std::uint64_t serial{},draws{};bool busy{},ended{};unsigned stamps{};Draw draw;};
    struct Sample {std::uint64_t serial{},draws{},ticks{};std::array<std::uint64_t,4> phases{};unsigned stamps{};Draw draw;};
    struct State {
        std::array<Slot,8> slots{};
        std::array<Sample,4096> samples{};
        std::size_t count{};
        std::uint64_t serial{},dropped{},cancelled{},errors{};
        const char* unavailable{"Timestamp backend unavailable"};
        const char* label;
        const char* backend;
        const StarfoxSdlD3D12TimestampsV1* d3d{};
        void* d3d_context{};
        double period{};
        unsigned bits{};
#if defined(STARFOX_SCENE_VULKAN_TIMESTAMPS)
        VkDevice device{};
        VkQueryPool pool{};
        PFN_vkDestroyQueryPool destroy{};
        PFN_vkCmdResetQueryPool reset{};
        PFN_vkCmdWriteTimestamp write{};
        PFN_vkGetQueryPoolResults results{};
        const StarfoxSdlVulkanRayBridgeV3* commands{};
#endif
        explicit State(SDL_GPUDevice* sdl,const char* name):label(name),backend(SDL_GetGPUDeviceDriver(sdl)) {
            const auto props=SDL_GetGPUDeviceProperties(sdl);
            if(std::string_view(SDL_GetGPUDeviceDriver(sdl))=="direct3d12") {
                d3d=static_cast<const StarfoxSdlD3D12TimestampsV1*>(SDL_GetPointerProperty(
                    props,STARFOX_SDL_D3D12_TIMESTAMPS,nullptr));
                unavailable="D3D12 timestamp bridge unavailable";
                if(!d3d || d3d->version!=1 || !d3d->create || !d3d->write || !d3d->resolve || !d3d->read || !d3d->destroy) return;
                std::uint64_t hz{};d3d_context=d3d->create(sdl,unsigned(slots.size()*5),&hz);
                unavailable="D3D12 timestamp allocation failed";
                if(!d3d_context || !hz) return;
                period=1e9/double(hz);bits=64;unavailable=nullptr;return;
            }
#if defined(STARFOX_SCENE_VULKAN_TIMESTAMPS)
            const auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(
                SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
            commands=static_cast<const StarfoxSdlVulkanRayBridgeV3*>(
                SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_RAY_BRIDGE,nullptr));
            if(!bridge || bridge->version!=2 || !bridge->device || !bridge->physical_device
                || !bridge->get_instance_proc || !bridge->get_device_proc
                || !commands || commands->version!=3 || !commands->command) return;
            const auto physical=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
                bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties"));
            const auto queues=reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
                bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceQueueFamilyProperties"));
            const auto create=reinterpret_cast<PFN_vkCreateQueryPool>(bridge->get_device_proc(bridge->device,"vkCreateQueryPool"));
            destroy=reinterpret_cast<PFN_vkDestroyQueryPool>(bridge->get_device_proc(bridge->device,"vkDestroyQueryPool"));
            reset=reinterpret_cast<PFN_vkCmdResetQueryPool>(bridge->get_device_proc(bridge->device,"vkCmdResetQueryPool"));
            write=reinterpret_cast<PFN_vkCmdWriteTimestamp>(bridge->get_device_proc(bridge->device,"vkCmdWriteTimestamp"));
            results=reinterpret_cast<PFN_vkGetQueryPoolResults>(bridge->get_device_proc(bridge->device,"vkGetQueryPoolResults"));
            unavailable="Vulkan timestamp entry points unavailable";
            if(!physical || !queues || !create || !destroy || !reset || !write || !results) return;
            std::uint32_t queue_count{};queues(bridge->physical_device,&queue_count,nullptr);
            unavailable="Vulkan queue timestamps unsupported";
            if(!queue_count || queue_count>1024 || bridge->queue_family>=queue_count) return;
            std::vector<VkQueueFamilyProperties> families(queue_count);
            queues(bridge->physical_device,&queue_count,families.data());
            if(bridge->queue_family>=queue_count) return;
            bits=families[bridge->queue_family].timestampValidBits;
            VkPhysicalDeviceProperties properties{};physical(bridge->physical_device,&properties);
            period=properties.limits.timestampPeriod;
            if(!bits || bits>64 || !std::isfinite(period) || period<=0) return;
            VkQueryPoolCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            info.queryType=VK_QUERY_TYPE_TIMESTAMP;info.queryCount=std::uint32_t(slots.size()*5);
            unavailable="Vulkan timestamp pool creation failed";
            device=bridge->device;
            if(create(device,&info,nullptr,&pool)!=VK_SUCCESS) {pool=VK_NULL_HANDLE;return;}
            unavailable=nullptr;
#endif
        }
        int begin(SDL_GPUCommandBuffer* command,std::size_t draws) noexcept {
            const auto next=++serial;
            if(d3d_context && count<samples.size()) {
                for(unsigned i=0;i<slots.size();++i) if(!slots[i].busy) {
                    if(!d3d->write(command,d3d_context,i*5)) {++errors;return -1;}
                    slots[i]={nullptr,next,draws,true,false,1,{}};return int(i);
                }
            }
#if defined(STARFOX_SCENE_VULKAN_TIMESTAMPS)
            if(pool && count<samples.size()) {
                const auto native=commands->command(command);
                if(!native) {++errors;return -1;}
                for(unsigned i=0;i<slots.size();++i) if(!slots[i].busy) {
                    slots[i]={nullptr,next,draws,true,false,1,{}};
                    reset(native,pool,i*5,5);
                    // Match D3D12's bottom-of-pipe boundary. TOP/BOTTOM can
                    // charge preceding submissions to more than one interval;
                    // those intervals must not be summed as exclusive work.
                    write(native,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,pool,i*5);
                    return int(i);
                }
            }
#else
            (void)command;(void)draws;(void)next;
#endif
            ++dropped;return -1;
        }
        void mark(int ticket,SDL_GPUCommandBuffer* command) noexcept {
            auto& slot=slots[unsigned(ticket)];
            if(slot.ended || slot.stamps>=5) {++errors;return;}
            if(d3d_context) {
                if(!d3d->write(command,d3d_context,unsigned(ticket)*5+slot.stamps)) {++errors;return;}
                ++slot.stamps;return;
            }
#if defined(STARFOX_SCENE_VULKAN_TIMESTAMPS)
            const auto native=commands->command(command);
            if(!native) {++errors;return;}
            write(native,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,pool,unsigned(ticket)*5+slot.stamps);
            ++slot.stamps;
#else
            (void)ticket;(void)command;
#endif
        }
        void end(int ticket,SDL_GPUCommandBuffer* command) noexcept {
            mark(ticket,command);auto& slot=slots[unsigned(ticket)];
            if(d3d_context && !d3d->resolve(command,d3d_context,unsigned(ticket)*5,slot.stamps)) {++errors;return;}
            slot.ended=true;
        }
        void retired(SDL_GPUFence* fence) noexcept {
            for(unsigned i=0;i<slots.size();++i) {
                auto& slot=slots[i];if(!slot.busy || slot.fence!=fence) continue;
                std::array<std::uint64_t,5> ticks{};bool valid=false;
                if(d3d_context) valid=d3d->read(d3d_context,i*5,slot.stamps,ticks.data());
#if defined(STARFOX_SCENE_VULKAN_TIMESTAMPS)
                // No WAIT/PARTIAL flag, no added fence wait or device idle.
                if(pool) {
                    std::array<std::uint64_t,10> values{};
                    valid=results(device,pool,i*5,slot.stamps,sizeof(values),values.data(),16,
                        VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)==VK_SUCCESS;
                    for(unsigned q=0;q<slot.stamps;++q) {ticks[q]=values[q*2];valid=valid && values[q*2+1];}
                }
#endif
                if(slot.ended && valid && slot.stamps>=2 && count<samples.size()) {
                    const auto mask=bits==64?UINT64_MAX:(std::uint64_t(1)<<bits)-1;
                    auto& sample=samples[count++];sample.serial=slot.serial;sample.draws=slot.draws;sample.stamps=slot.stamps;
                    sample.draw=slot.draw;
                    sample.ticks=(ticks[slot.stamps-1]-ticks[0])&mask;
                    for(unsigned q=1;q<slot.stamps;++q) sample.phases[q-1]=(ticks[q]-ticks[q-1])&mask;
                } else ++errors;
                slot={};
            }
        }
        ~State() {
            // Defer all output until release: per-frame logging contaminates
            // Windows host submission timings. Owner must retire before here.
            try {
                std::cerr<<label<<"-gpu-timestamps: samples="<<count<<" dropped="<<dropped
                    <<" cancelled="<<cancelled<<" errors="<<errors;
                std::cerr<<" period-ns="<<period<<" valid-bits="<<bits<<" boundary=bottom-bottom backend="<<backend;
                if(unavailable) std::cerr<<" unavailable="<<unavailable;
                std::cerr<<'\n';
                for(std::size_t i=0;i<count;++i) {
                    const auto& sample=samples[i];
                    std::cerr<<label<<"-gpu-time: serial="<<sample.serial<<" draws="<<sample.draws
                        <<" ticks="<<sample.ticks<<" us="<<(double(sample.ticks)*period/1000.0);
                    if(sample.stamps==5 && sample.draw.batch) {
                        std::cerr<<" prepare-us="<<double(sample.phases[0])*period/1000.0
                            <<" clip-us="<<double(sample.phases[1])*period/1000.0
                            <<" spans-us="<<double(sample.phases[2])*period/1000.0
                            <<" paint-us="<<double(sample.phases[3])*period/1000.0;
                    } else if(sample.stamps==4) {
                        std::cerr<<(sample.draw.batch?" prepare-us=":" input-us=")<<double(sample.phases[0])*period/1000.0
                            <<(sample.draw.batch?" spans-us=":" effects-us=")<<double(sample.phases[1])*period/1000.0
                            <<(sample.draw.batch?" paint-us=":" output-us=")<<double(sample.phases[2])*period/1000.0;
                    }
                    if(sample.draw.batch) std::cerr<<" batch="<<sample.draw.batch
                        <<" index="<<sample.draw.index<<" kind="<<sample.draw.kind;
                    std::cerr<<'\n';
                }
            } catch(...) {}
#if defined(STARFOX_SCENE_VULKAN_TIMESTAMPS)
            if(pool) destroy(device,pool,nullptr);
#endif
            if(d3d_context) d3d->destroy(d3d_context);
        }
    };
    std::unique_ptr<State> state_;
    const char* environment_;
    const char* label_;
};
