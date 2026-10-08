// Diagnostic-only per-draw intervals reuse the same exact-fence query owner.
// Work is measured through its normal painter merge/MSAA/ray-input packing;
// these intervals do not include the separate hardware-ray queue.
class SceneDrawTimestamps {
public:
    [[nodiscard]] std::uint64_t batch_id() const noexcept {return batch_;}
    void begin_batch(bool owned) noexcept {
        enabled_=owned && std::getenv("STARFOX_TRACE_SCENE_GPU_DRAWS");
        if(enabled_) ++batch_;
    }
    int begin(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command,unsigned index,
        const GpuSceneDraw& draw) noexcept {
        if(!enabled_) return -1;
        if(index>=256) {++oversized_;return -1;}
        try {
            while(recorders_.size()<=index) recorders_.push_back(std::make_unique<Recorder>());
            const auto units=std::visit([](const auto& item)->std::size_t {
                using T=std::decay_t<decltype(item)>;
                if constexpr(std::is_same_v<T,GpuModelDraw>) return item.shape?item.shape->faces.size():0;
                else if constexpr(std::is_same_v<T,GpuRasterDraw> || std::is_same_v<T,GpuIndexedLayerDraw>)
                    return item.commands?item.commands->commands.size():0;
                else if constexpr(std::is_same_v<T,GpuGridDraw>) return item.lines?675:225;
                else if constexpr(std::is_same_v<T,GpuDustDraw>) return item.frame.points.size();
                else if constexpr(std::is_same_v<T,GpuParticleDraw>) return item.frame.particles.size();
                else if constexpr(std::is_same_v<T,GpuTextDraw>) return item.frame.glyphs.size();
                else return 1;
            },draw);
            auto& recorder=*recorders_[index];
            recorder.ticket=recorder.queries.begin(device,command,units);
            recorder.queries.annotate_draw(recorder.ticket,batch_,index,unsigned(draw.index()));
            return recorder.ticket>=0?int(index):-1;
        } catch(...) {++oversized_;return -1;}
    }
    void end(int index,SDL_GPUCommandBuffer* command) noexcept {
        if(index>=0) {
            auto& recorder=*recorders_[unsigned(index)];
            recorder.queries.end(recorder.ticket,command);
        }
    }
    void mark(int index,SDL_GPUCommandBuffer* command) noexcept {
        if(index>=0) {
            auto& recorder=*recorders_[unsigned(index)];
            recorder.queries.mark(recorder.ticket,command);
        }
    }
    void submitted(SDL_GPUFence* fence) noexcept {
        for(auto& recorder:recorders_) {
            recorder->queries.submitted(recorder->ticket,fence);recorder->ticket=-1;
        }
    }
    void cancel() noexcept {
        for(auto& recorder:recorders_) {
            recorder->queries.cancel(recorder->ticket);recorder->ticket=-1;
        }
    }
    void retired(SDL_GPUFence* fence) noexcept {
        for(auto& recorder:recorders_) recorder->queries.retired(fence);
    }
    void release() noexcept {
        if(batch_) {
            try {std::cerr<<"scene-draw-profile: batches="<<batch_<<" owners="<<recorders_.size()
                <<" oversized="<<oversized_<<'\n';} catch(...) {}
        }
        recorders_.clear();batch_=oversized_=0;enabled_=false;
    }
    ~SceneDrawTimestamps(){release();}
private:
    struct Recorder {
        SceneGpuTimestamps queries{"STARFOX_TRACE_SCENE_GPU_DRAWS","scene-draw"};
        int ticket{-1};
    };
    std::vector<std::unique_ptr<Recorder>> recorders_;
    std::uint64_t batch_{},oversized_{};
    bool enabled_{};
};
