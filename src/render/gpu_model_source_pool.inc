// Included by gpu_model.cpp. This pool shares source uploads, not projected
// geometry. The packet identities expire after the two encoders have joined.
struct GpuModelSourcePool::Impl {
    std::string status{"GPU model source pool unavailable"};
    std::vector<GpuPreparedModelSource> packets;
    GpuModelUploadInfo stats{};
    bool encoded{};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    struct Buffer {SDL_GPUBuffer* value{};std::uint32_t capacity{};};
    struct Input {const void* data{};std::uint32_t bytes{},size{};};
    SDL_GPUDevice* device{};
    std::vector<Buffer> buffers;
    std::vector<std::uint32_t> sizes;
    SDL_GPUTransferBuffer* transfer{};
    std::uint32_t transfer_capacity{};
    ~Impl(){release();}
    static void require(bool ok){if(!ok) throw std::runtime_error(SDL_GetError());}
    void release() noexcept {
        for(auto& buffer:buffers) if(buffer.value) SDL_ReleaseGPUBuffer(device,buffer.value);
        if(transfer) SDL_ReleaseGPUTransferBuffer(device,transfer);
        buffers.clear();sizes.clear();transfer=nullptr;transfer_capacity=0;device=nullptr;
        packets.clear();encoded=false;stats={};
    }
#endif
};
GpuModelSourcePool::GpuModelSourcePool():impl_(std::make_unique<Impl>()){}
GpuModelSourcePool::~GpuModelSourcePool()=default;
void GpuModelSourcePool::end_recording() noexcept {
    impl_->encoded=false;impl_->packets.clear();
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->sizes.clear();
#endif
}
void GpuModelSourcePool::release_device() noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#else
    end_recording();impl_->stats={};
#endif
}
std::span<const GpuPreparedModelSource> GpuModelSourcePool::packets() const noexcept {return impl_->packets;}
GpuModelUploadInfo GpuModelSourcePool::upload_info() const noexcept {
    auto result=impl_->stats;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    for(const auto& buffer:impl_->buffers) result.source_storage_bytes+=buffer.capacity;
    result.source_storage_bytes+=impl_->transfer_capacity;
#endif
    return result;
}
const std::string& GpuModelSourcePool::status() const noexcept {return impl_->status;}
bool GpuModelSourcePool::prepare(void* device,std::span<const GpuModelSourceRequest> requests,std::uint64_t budget) {
    end_recording();impl_->stats={};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!device || !budget || requests.empty() || requests.size()>16384 || budget>8U*1024*1024) return false;
    try {
        auto* next=static_cast<SDL_GPUDevice*>(device);
        if(next!=impl_->device) {impl_->release();impl_->device=next;}
        struct Projection {const PreparedProjectionSource* source;std::array<unsigned,2> indices;};
        struct Topology {const PreparedBspSource* source;std::array<unsigned,2> indices;};
        struct Texture {const assets::TextureImage* source;unsigned index;};
        struct Faces {const PreparedFacesSource* source;std::array<unsigned,4> indices;};
        struct Rays {const PreparedRayTopology* source;unsigned triangles,materials;};
        std::vector<Projection> projections;std::vector<Topology> topologies;std::vector<Texture> textures;
        std::vector<Faces> faces;
        std::vector<Rays> rays;
        std::vector<Impl::Input> inputs;
        std::uint64_t total=0;
        const auto input=[&](const void* data,std::size_t bytes) {
            if(inputs.size()>=2048 || bytes>budget)
                throw std::length_error("Immutable stereo source upload budget exhausted");
            const auto size=(std::max<std::size_t>(4,bytes)+3)&~std::size_t(3);
            if(size>budget-total) throw std::length_error("Immutable stereo source upload budget exhausted");
            const auto index=unsigned(inputs.size());
            inputs.push_back({data,std::uint32_t(bytes),std::uint32_t(size)});
            total+=inputs.back().size;return index;
        };
        impl_->packets.reserve(requests.size());
        // Pointer deduplication is only over these owned immutable packets,
        // never over mutable shapes, animation frames or GPU-produced data.
        for(const auto& request:requests) {
            GpuPreparedModelSource packet;packet.device_=device;packet.source_=request;
            packet.encoded_=&impl_->encoded;
            if(request.projection) {
                auto found=std::find_if(projections.begin(),projections.end(),[&](const auto& p){return p.source==request.projection;});
                if(found==projections.end()) {
                    const auto& source=*request.projection;
                    const auto native=source.native_input();const auto continuous=source.continuous_input();
                    const auto faces=source.visibility_input();
                    const auto vertex=source.continuous()?input(continuous.data(),continuous.size_bytes()):input(native.data(),native.size_bytes());
                    const auto face=input(faces.data(),faces.size_bytes());
                    projections.push_back({request.projection,{vertex,face}});found=std::prev(projections.end());
                }
                for(unsigned i=0;i<2;++i) packet.sizes_[i]=inputs[found->indices[i]].bytes;
            }
            if(request.topology) {
                auto found=std::find_if(topologies.begin(),topologies.end(),[&](const auto& t){return t.source==request.topology;});
                if(found==topologies.end()) {
                    const auto& graph=request.topology->graph();
                    const auto nodes=input(graph.nodes.data(),graph.nodes.size()*sizeof(PackedBspNode));
                    const auto faces=input(graph.face_ids.data(),graph.face_ids.size()*sizeof(std::uint32_t));
                    topologies.push_back({request.topology,{nodes,faces}});found=std::prev(topologies.end());
                }
                for(unsigned i=0;i<2;++i) packet.sizes_[i+2]=inputs[found->indices[i]].bytes;
            }
            if(request.billboard_texture) {
                const auto* texture=request.billboard_texture;
                const auto required=(std::uint64_t(texture->u_mask)+1)*(std::uint64_t(texture->v_mask)+1);
                if(required>texture->texels.size()) throw std::runtime_error("Incomplete immutable billboard texture");
                auto found=std::find_if(textures.begin(),textures.end(),[&](const auto& t){return t.source==texture;});
                if(found==textures.end()) {
                    const auto at=input(texture->texels.data(),texture->texels.size());
                    textures.push_back({texture,at});found=std::prev(textures.end());
                }
                packet.sizes_[4]=inputs[found->index].size;
            }
            if(request.faces) {
                auto found=std::find_if(faces.begin(),faces.end(),[&](const auto& f){return f.source==request.faces;});
                if(found==faces.end()) {
                    const auto& source=request.faces->faces();
                    const auto corners=input(source.corners.data(),source.corners.size()*sizeof(source.corners[0]));
                    const auto polygons=input(source.polygons.data(),source.polygons.size()*sizeof(source.polygons[0]));
                    const auto materials=input(source.materials.data(),source.materials.size()*sizeof(RasterCommand));
                    const auto texels=input(source.texels.data(),source.texels.size());
                    faces.push_back({request.faces,{corners,polygons,materials,texels}});found=std::prev(faces.end());
                }
                for(unsigned i=0;i<4;++i) packet.sizes_[i+5]=i==3?inputs[found->indices[i]].size:inputs[found->indices[i]].bytes;
            }
            if(request.rays) {
                if(!request.faces || !request.rays->matches(*request.faces,false))
                    throw std::runtime_error("Immutable ray upload does not match its face source");
                if(!request.rays->triangles().empty()) {
                    auto found=std::find_if(rays.begin(),rays.end(),[&](const auto& r){return r.source==request.rays;});
                    if(found==rays.end()) {
                        const auto triangles=request.rays->triangles(),materials=request.rays->material_topology();
                        const auto t=input(triangles.data(),triangles.size_bytes());
                        const auto m=materials.empty()?UINT32_MAX:input(materials.data(),materials.size_bytes());
                        rays.push_back({request.rays,t,m});found=std::prev(rays.end());
                    }
                    packet.sizes_[9]=inputs[found->triangles].bytes;
                    if(found->materials!=UINT32_MAX) packet.sizes_[10]=inputs[found->materials].bytes;
                }
            }
            impl_->packets.push_back(packet);
        }
        if(inputs.empty()) {end_recording();return false;}
        for(unsigned i=unsigned(inputs.size());i<impl_->buffers.size();++i)
            if(impl_->buffers[i].value) SDL_ReleaseGPUBuffer(next,impl_->buffers[i].value);
        impl_->buffers.resize(inputs.size());
        std::uint64_t retained=0;
        for(unsigned i=0;i<inputs.size();++i) retained+=std::max(impl_->buffers[i].capacity,inputs[i].size);
        // High-water capacity must not accumulate across changing layouts.
        // Shrink oversized slots when necessary; otherwise reuse allocations.
        if(retained>budget) for(unsigned i=0;i<inputs.size();++i) {
            auto& buffer=impl_->buffers[i];
            if(buffer.capacity>inputs[i].size) {
                SDL_ReleaseGPUBuffer(next,buffer.value);buffer={};
            }
        }
        for(unsigned i=0;i<inputs.size();++i) {
            auto& buffer=impl_->buffers[i];
            if(buffer.capacity<inputs[i].size) {
                SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,inputs[i].size,0};
                auto* replacement=SDL_CreateGPUBuffer(next,&info);Impl::require(replacement);
                if(buffer.value) SDL_ReleaseGPUBuffer(next,buffer.value);
                buffer={replacement,info.size};
            }
        }
        for(auto& packet:impl_->packets) {
            if(packet.source_.projection) {
                const auto found=std::find_if(projections.begin(),projections.end(),[&](const auto& p){return p.source==packet.source_.projection;});
                for(unsigned i=0;i<2;++i) packet.buffers_[i]=impl_->buffers[found->indices[i]].value;
            }
            if(packet.source_.topology) {
                const auto found=std::find_if(topologies.begin(),topologies.end(),[&](const auto& t){return t.source==packet.source_.topology;});
                for(unsigned i=0;i<2;++i) packet.buffers_[i+2]=impl_->buffers[found->indices[i]].value;
            }
            if(packet.source_.billboard_texture) {
                const auto found=std::find_if(textures.begin(),textures.end(),[&](const auto& t){return t.source==packet.source_.billboard_texture;});
                packet.buffers_[4]=impl_->buffers[found->index].value;
            }
            if(packet.source_.faces) {
                const auto found=std::find_if(faces.begin(),faces.end(),[&](const auto& f){return f.source==packet.source_.faces;});
                for(unsigned i=0;i<4;++i) packet.buffers_[i+5]=impl_->buffers[found->indices[i]].value;
            }
            if(packet.source_.rays && packet.sizes_[9]) {
                const auto found=std::find_if(rays.begin(),rays.end(),[&](const auto& r){return r.source==packet.source_.rays;});
                packet.buffers_[9]=impl_->buffers[found->triangles].value;
                if(found->materials!=UINT32_MAX) packet.buffers_[10]=impl_->buffers[found->materials].value;
            }
        }
        if(impl_->transfer_capacity<total || impl_->transfer_capacity>budget) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,std::uint32_t(total),0};
            auto* replacement=SDL_CreateGPUTransferBuffer(next,&info);Impl::require(replacement);
            if(impl_->transfer) SDL_ReleaseGPUTransferBuffer(next,impl_->transfer);
            impl_->transfer=replacement;impl_->transfer_capacity=info.size;
        }
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(next,impl_->transfer,true));Impl::require(mapped);
        std::size_t offset=0;
        for(const auto& value:inputs) {
            if(value.bytes) std::memcpy(mapped+offset,value.data,value.bytes);
            if(value.size>value.bytes) std::memset(mapped+offset+value.bytes,0,value.size-value.bytes);
            offset+=value.size;
        }
        SDL_UnmapGPUTransferBuffer(next,impl_->transfer);
        for(const auto& value:inputs) impl_->sizes.push_back(value.size);
        impl_->stats.input_bytes=total;
        impl_->status="Immutable stereo model sources prepared";return true;
    } catch(const std::exception& error) {end_recording();impl_->stats={};impl_->status=error.what();}
#else
    (void)device;(void)requests;(void)budget;
#endif
    return false;
}
bool GpuModelSourcePool::enqueue(void* command) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!command || impl_->encoded || impl_->sizes.empty()) return false;
    auto* copy=SDL_BeginGPUCopyPass(static_cast<SDL_GPUCommandBuffer*>(command));
    if(!copy) {impl_->status=SDL_GetError();return false;}
    std::uint32_t offset=0;
    for(unsigned i=0;i<impl_->sizes.size();++i) {
        SDL_GPUTransferBufferLocation from{impl_->transfer,offset};
        SDL_GPUBufferRegion to{impl_->buffers[i].value,0,impl_->sizes[i]};
        // Cycle before either eye binds. Both commands subsequently retain the
        // SAME backing version, even when later pairs are already in flight.
        SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=impl_->sizes[i];
    }
    SDL_EndGPUCopyPass(copy);impl_->encoded=true;
    impl_->stats.uploaded_bytes=offset;impl_->stats.uploaded_buffers=unsigned(impl_->sizes.size());
    impl_->status="Immutable stereo model sources uploaded once";return true;
#else
    (void)command;return false;
#endif
}
