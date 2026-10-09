namespace {
// Host packets and their GPU borrowing are tied to this pair, not to a shape
// address across frames. Destruction joins/cancellation in the caller precedes
// this scope's teardown; only reusable SDL allocations survive it.
struct StereoModelSourceRecording {
    GpuModelSourcePool* pool{};
    std::vector<GpuModelDraw*> models;
    StereoModelSourceRecording(std::unique_ptr<GpuModelSourcePool>& owner,void* device,
        std::span<GpuSceneDraw> left,std::span<GpuSceneDraw> right) {
        for(auto draws:{left,right}) for(auto& draw:draws)
            if(auto* model=std::get_if<GpuModelDraw>(&draw)) model->prepared_gpu=nullptr;
#if defined(STARFOX_SDL_GPU_EFFECTS)
        if(owner) owner->prepare(device,{}); // Clear last pair's counters/identities, not its allocations.
        if(SDL_getenv("STARFOX_TEST_DUPLICATE_STEREO_SOURCE_UPLOADS")) return;
        std::vector<GpuModelSourceRequest> requests;
        models.reserve(left.size()*2);
        for(std::size_t index=0;index<left.size();++index) if(auto* model=std::get_if<GpuModelDraw>(&left[index])) {
            if(model->pose.collapse_to_axis_line || !model->shape) continue;
            if(model->pose.simple_scaled_sprite) {
                auto* other=std::get_if<GpuModelDraw>(&right[index]);
                const auto* texture=texture_for_colour(*model->shape,model->pose.simple_sprite_colour,model->pose.colour_frame);
                if(!other || !other->shape || !other->pose.simple_scaled_sprite || !texture
                    || texture!=texture_for_colour(*other->shape,other->pose.simple_sprite_colour,other->pose.colour_frame)) continue;
                // Empty/clipped billboard helpers do not consume their texture.
                if(model->pose.z<128 || other->pose.z<128 || model->pose.simple_sprite_world_size<=0
                    || other->pose.simple_sprite_world_size<=0) continue;
                requests.push_back({nullptr,nullptr,texture});models.push_back(model);models.push_back(other);
                continue;
            }
            // Empty helper shapes perform no model-source uploads. Do not add
            // a pool pass for their unused vertex/BSP descriptors.
            if(model->prepared_topology && model->prepared_topology->graph().faces.empty()) continue;
            if(model->prepared_projection && model->prepared_projection->native_input().empty()
                && model->prepared_projection->continuous_input().empty()) continue;
            // Explosion vertex/visibility expansion is mutable and eye-owned.
            // Its already-flattened immutable BSP source can still be shared.
            const auto* projection=model->pose.explosion_progress?nullptr:model->prepared_projection;
            if(!projection && !model->prepared_topology) continue;
            auto* other=std::get_if<GpuModelDraw>(&right[index]);
            const auto* faces=other && other->prepared_faces==model->prepared_faces?model->prepared_faces:nullptr;
            const auto* rays=faces && other && other->prepared_rays==model->prepared_rays
                && !SDL_getenv("STARFOX_TEST_DUPLICATE_STEREO_RAY_UPLOADS")?model->prepared_rays:nullptr;
            requests.push_back({projection,model->prepared_topology,nullptr,faces,rays});models.push_back(model);
            models.push_back(other);
        }
        if(requests.empty()) return;
        if(!owner) owner=std::make_unique<GpuModelSourcePool>();
        if(!owner->prepare(device,requests)) {models.clear();return;}
        pool=owner.get();const auto packets=pool->packets();
        for(unsigned i=0;i<packets.size();++i) {
            models[i*2]->prepared_gpu=models[i*2+1]->prepared_gpu=&packets[i];
        }
#else
        (void)owner;(void)device;
#endif
    }
    ~StereoModelSourceRecording() {
        for(auto* model:models) model->prepared_gpu=nullptr;
        if(pool) pool->end_recording();
    }
    void enqueue(void* command) {
        if(pool && !pool->enqueue(command)) throw std::runtime_error(pool->status());
    }
};
GpuModelUploadInfo add_model_uploads(GpuModelUploadInfo a,const GpuModelUploadInfo& b) noexcept {
    a.input_bytes+=b.input_bytes;a.uploaded_bytes+=b.uploaded_bytes;
    a.uploaded_buffers+=b.uploaded_buffers;a.reused_buffers+=b.reused_buffers;
    a.shared_bytes+=b.shared_bytes;a.shared_buffers+=b.shared_buffers;
    a.snapshot_bytes+=b.snapshot_bytes;a.source_storage_bytes+=b.source_storage_bytes;
    a.ray_shared_bytes+=b.ray_shared_bytes;a.ray_shared_buffers+=b.ray_shared_buffers;
    a.pose_uniform_bytes+=b.pose_uniform_bytes;a.pose_uniform_pushes+=b.pose_uniform_pushes;
    a.pose_uniform_input_bytes+=b.pose_uniform_input_bytes;
    return a;
}
}
