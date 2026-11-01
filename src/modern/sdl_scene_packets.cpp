#include "starfox/modern/sdl_scene_packets.hpp"
#include "starfox/vr/scene_types.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/source_models.hpp"
#include "modern/shaders/generated/scene_sdl.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
namespace starfox::modern {
namespace {
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what);}
std::string sdl_error(const char* what) {return std::string(what)+": "+SDL_GetError();}
SDL_GPUBuffer* create_buffer(SDL_GPUDevice* device,SDL_GPUBufferUsageFlags usage,uint32_t size) {
    SDL_GPUBufferCreateInfo info{};info.usage=usage;info.size=size;
    auto* buffer=SDL_CreateGPUBuffer(device,&info);
    require(buffer,sdl_error("Create scene buffer"));
    return buffer;
}
struct BufferDeleter {
    SDL_GPUDevice* device{};
    void operator()(SDL_GPUBuffer* buffer) const noexcept {if(device && buffer) SDL_ReleaseGPUBuffer(device,buffer);}
};
using Buffer=std::shared_ptr<SDL_GPUBuffer>;
Buffer owned(SDL_GPUDevice* device,SDL_GPUBuffer* buffer) {return Buffer(buffer,BufferDeleter{device});}
}
SdlUploadBatch::~SdlUploadBatch() {release();}
void SdlUploadBatch::release() noexcept {
    if(device_ && transfer_) SDL_ReleaseGPUTransferBuffer(device_,transfer_);
    device_=nullptr;transfer_=nullptr;capacity_=0;clear();
}
void SdlUploadBatch::add(SDL_GPUBuffer* buffer,uint32_t offset,const void* bytes,uint32_t size,bool cycle) {
    if(!buffer || !size) return;
    const auto at=uint32_t(staging_.size());
    staging_.resize(staging_.size()+size);
    std::memcpy(staging_.data()+at,bytes,size);
    writes_.push_back({buffer,offset,size,at,cycle});
}
bool SdlUploadBatch::flush(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command) {
    if(writes_.empty()) return true;
    try {
        require(device && command,"Missing upload device/command");
        if(device_!=device || capacity_<staging_.size()) {
            if(device_ && transfer_) SDL_ReleaseGPUTransferBuffer(device_,transfer_);
            transfer_=nullptr;device_=device;
            // Grow geometrically so steady-state frames reuse one buffer.
            capacity_=std::max<uint32_t>(uint32_t(staging_.size()),std::max<uint32_t>(capacity_*2,1U<<20));
            SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;info.size=capacity_;
            transfer_=SDL_CreateGPUTransferBuffer(device,&info);
            require(transfer_,sdl_error("Create upload batch"));
        }
        // Cycling hands back a fresh backing store while earlier frames still read the old one.
        void* mapped=SDL_MapGPUTransferBuffer(device,transfer_,true);
        require(mapped,sdl_error("Map upload batch"));
        std::memcpy(mapped,staging_.data(),staging_.size());
        SDL_UnmapGPUTransferBuffer(device,transfer_);
        auto* copy=SDL_BeginGPUCopyPass(command);
        require(copy,sdl_error("Begin upload batch"));
        for(const auto& write:writes_) {
            SDL_GPUTransferBufferLocation source{transfer_,write.staging};
            SDL_GPUBufferRegion destination{write.buffer,write.offset,write.size};
            SDL_UploadToGPUBuffer(copy,&source,&destination,write.cycle);
        }
        SDL_EndGPUCopyPass(copy);
        clear();status_.clear();return true;
    } catch(const std::exception& error) {status_=error.what();clear();return false;}
}
vr::SceneConstants sdl_scene_constants(const vr::EyeCamera& camera) noexcept {
    auto flipped=camera;
    for(unsigned column=0;column<4;++column) flipped.projection[column*4+1]=-flipped.projection[column*4+1];
    return vr::scene_constants(flipped);
}
SdlScenePipelines::~SdlScenePipelines() {release();}
void SdlScenePipelines::release() noexcept {
    if(device_) {
        for(auto& [key,pipeline]:pipelines_) SDL_ReleaseGPUGraphicsPipeline(device_,pipeline);
        if(vertex_) SDL_ReleaseGPUShader(device_,vertex_);
        if(fragment_) SDL_ReleaseGPUShader(device_,fragment_);
        if(placeholder_) SDL_ReleaseGPUBuffer(device_,placeholder_);
    }
    pipelines_.clear();vertex_=fragment_=nullptr;placeholder_=shadow_triangles_=shadow_objects_=nullptr;device_=nullptr;
}
void SdlScenePipelines::bind_fragment_storage(SDL_GPURenderPass* pass,SDL_GPUBuffer* texels) const noexcept {
    SDL_GPUBuffer* buffers[]{texels,shadow_triangles_?shadow_triangles_:placeholder_,shadow_objects_?shadow_objects_:placeholder_};
    SDL_BindGPUFragmentStorageBuffers(pass,0,buffers,3);
}
void SdlScenePipelines::set_sample_count(SDL_GPUSampleCount samples) noexcept {
    if(samples==samples_) return;
    // Callers change targets between frames, after the last pass using them.
    if(device_) for(auto& [key,pipeline]:pipelines_) SDL_ReleaseGPUGraphicsPipeline(device_,pipeline);
    pipelines_.clear();samples_=samples;
}
void SdlScenePipelines::set_line_brush(unsigned pixels,unsigned target_width,unsigned target_height) noexcept {
    brush_pixels_=std::clamp(pixels,1U,64U);
    // Clip-space offset per target pixel; SDL_GPU clip space is Y-up, so a
    // pixel down the target is negative Y.
    brush_=brush_pixels_>1 && target_width && target_height
        ?std::array<float,4>{float(brush_pixels_),2.F/float(target_width),-2.F/float(target_height),1.F}
        :std::array<float,4>{};
    if(!brush_[3]) brush_pixels_=1;
}
void SdlScenePipelines::push_line_brush(SDL_GPUCommandBuffer* command,SceneTopology topology) const noexcept {
    const std::array<float,4> off{};
    const auto& data=topology==SceneTopology::lines?brush_:off;
    SDL_PushGPUVertexUniformData(command,1,data.data(),sizeof(data));
}
bool SdlScenePipelines::initialize(SDL_GPUDevice* device,SDL_GPUTextureFormat colour,SDL_GPUTextureFormat depth,bool reversed_depth) {
    release();reversed_depth_=reversed_depth;
    try {
        require(device!=nullptr,"Missing SDL GPU device");
        const auto formats=SDL_GetGPUShaderFormats(device);
        const bool dxil=(formats&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        require(dxil || (formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0,"MODERN scene shaders need a SPIR-V or DXIL backend");
        const auto create=[&](SDL_GPUShaderStage stage) {
            const bool vertex=stage==SDL_GPU_SHADERSTAGE_VERTEX;
            SDL_GPUShaderCreateInfo info{};
            if(dxil) {
                info.code=vertex?shader::dxil_vertex:shader::dxil_fragment;
                info.code_size=vertex?sizeof(shader::dxil_vertex):sizeof(shader::dxil_fragment);
                info.format=SDL_GPU_SHADERFORMAT_DXIL;
            } else {
                info.code=vertex?shader::spirv_vertex:shader::spirv_fragment;
                info.code_size=vertex?sizeof(shader::spirv_vertex):sizeof(shader::spirv_fragment);
                info.format=SDL_GPU_SHADERFORMAT_SPIRV;
            }
            info.entrypoint=vertex?shader::vertex_entry:shader::fragment_entry;
            // Fragment: slot 0 texels, 1-2 shadow casters; uniform slot 1 fog and shadow.
            info.stage=stage;info.num_storage_buffers=vertex?1:3;info.num_uniform_buffers=2;
            auto* created=SDL_CreateGPUShader(device,&info);
            require(created,sdl_error(vertex?"Create MODERN vertex shader":"Create MODERN fragment shader"));
            return created;
        };
        device_=device;colour_=colour;depth_=depth;
        vertex_=create(SDL_GPU_SHADERSTAGE_VERTEX);
        fragment_=create(SDL_GPU_SHADERSTAGE_FRAGMENT);
        placeholder_=create_buffer(device,SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,64);
        status_="MODERN scene pipelines ready";return true;
    } catch(const std::exception& error) {status_=error.what();release();return false;}
}
SDL_GPUGraphicsPipeline* SdlScenePipelines::get(SceneTopology topology,SceneBlend blend,bool depth_test,bool depth_write) {
    if(!device_) return nullptr;
    const bool painter=painter_ && depth_test;
    const unsigned key=unsigned(topology)|(unsigned(depth_test)<<1)|(unsigned(depth_test && depth_write)<<2)|(unsigned(blend)<<3)
        |(unsigned(painter)<<7);
    if(const auto found=pipelines_.find(key);found!=pipelines_.end()) return found->second;
    using V=vr::SceneVertex;
    const SDL_GPUVertexAttribute attributes[]{
        {0,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,uint32_t(offsetof(V,position))},
        {1,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,uint32_t(offsetof(V,color))},
        {2,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,uint32_t(offsetof(V,odd_color))},
        {3,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT,uint32_t(offsetof(V,dither_scale))},
        {4,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,uint32_t(offsetof(V,visibility_a))},
        {5,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,uint32_t(offsetof(V,visibility_b))},
        {6,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,uint32_t(offsetof(V,visibility_c))},
        {7,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT,uint32_t(offsetof(V,visibility_enabled))},
        {8,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,uint32_t(offsetof(V,group_a))},
        {9,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,uint32_t(offsetof(V,group_b))},
        {10,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,uint32_t(offsetof(V,group_c))},
        {11,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT,uint32_t(offsetof(V,group_enabled))},
        {12,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,uint32_t(offsetof(V,uv))},
        {13,0,SDL_GPU_VERTEXELEMENTFORMAT_UINT4,uint32_t(offsetof(V,texture))},
        {14,0,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,uint32_t(offsetof(V,billboard))}};
    SDL_GPUVertexBufferDescription binding{0,sizeof(V),SDL_GPU_VERTEXINPUTRATE_VERTEX,0};
    SDL_GPUColorTargetDescription target{};target.format=colour_;
    auto& state=target.blend_state;
    state.color_write_mask=SDL_GPU_COLORCOMPONENT_R|SDL_GPU_COLORCOMPONENT_G|SDL_GPU_COLORCOMPONENT_B|SDL_GPU_COLORCOMPONENT_A;
    state.enable_color_write_mask=true;
    if(blend!=SceneBlend::opaque) { // Same factors as vulkan_scene_pipeline.cpp.
        const bool half=blend==SceneBlend::half_add || blend==SceneBlend::half_subtract;
        const bool subtract=blend==SceneBlend::subtract || blend==SceneBlend::half_subtract;
        state.enable_blend=true;
        state.src_color_blendfactor=SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor=half?SDL_GPU_BLENDFACTOR_SRC_ALPHA:SDL_GPU_BLENDFACTOR_ONE;
        state.color_blend_op=subtract?SDL_GPU_BLENDOP_REVERSE_SUBTRACT:SDL_GPU_BLENDOP_ADD;
        state.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ZERO;
        state.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;state.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
        if(blend==SceneBlend::alpha) {
            state.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            state.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
            state.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        }
    }
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader=vertex_;info.fragment_shader=fragment_;
    info.vertex_input_state.vertex_buffer_descriptions=&binding;info.vertex_input_state.num_vertex_buffers=1;
    info.vertex_input_state.vertex_attributes=attributes;info.vertex_input_state.num_vertex_attributes=15;
    info.primitive_type=topology==SceneTopology::lines?SDL_GPU_PRIMITIVETYPE_LINELIST:SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.enable_depth_clip=true; // Vulkan default: clip, never clamp.
    info.multisample_state.sample_count=samples_;
    info.depth_stencil_state.compare_op=painter?SDL_GPU_COMPAREOP_ALWAYS
        :reversed_depth_?SDL_GPU_COMPAREOP_GREATER:SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test=depth_test;
    info.depth_stencil_state.enable_depth_write=depth_test && depth_write;
    info.target_info.color_target_descriptions=&target;info.target_info.num_color_targets=1;
    info.target_info.depth_stencil_format=depth_;info.target_info.has_depth_stencil_target=true;
    auto* pipeline=SDL_CreateGPUGraphicsPipeline(device_,&info);
    if(!pipeline) {status_=sdl_error("Create MODERN scene pipeline");return nullptr;}
    pipelines_.emplace(key,pipeline);
    return pipeline;
}
struct SdlDrawPackets::State {
    struct Geometry {vr::DrawPacket source;Buffer triangles,lines,texels;uint32_t triangle_count{},line_count{};bool photograph{};};
    struct Item {vr::Matrix4 model;std::shared_ptr<Geometry> geometry;uint32_t key{};};
    SDL_GPUDevice* device{};
    std::vector<Item> items;
    bool keyed{},depth_test{true},depth_write{true};
    std::optional<SceneBlend> blend;
    std::size_t reused{},rejected{};
};
SdlDrawPackets::SdlDrawPackets()=default;
SdlDrawPackets::~SdlDrawPackets()=default;
void SdlDrawPackets::release() noexcept {state_.reset();}
std::size_t SdlDrawPackets::size() const noexcept {return state_?state_->items.size():0;}
std::size_t SdlDrawPackets::reused_packets() const noexcept {return state_?state_->reused:0;}
std::size_t SdlDrawPackets::rejected_packets() const noexcept {return state_?state_->rejected:0;}
bool SdlDrawPackets::update_models(std::span<const vr::Matrix4> models) noexcept {
    if(!state_ || models.size()!=state_->items.size()) return false;
    for(const auto& model:models) if(!vr::model_eye_camera(vr::EyeCamera{},model)) return false;
    for(std::size_t i=0;i<models.size();++i) state_->items[i].model=models[i];
    return true;
}
bool SdlDrawPackets::initialize(SDL_GPUDevice* device,SdlUploadBatch& uploads,
    std::span<const vr::DrawPacket> packets,std::span<const uint32_t> object_keys,bool depth_test,bool depth_write,
    std::optional<SceneBlend> blend) {
    try {
        require(device && packets.size()<=4096,"Invalid MODERN scene upload");
        if(!object_keys.empty()) {
            require(object_keys.size()==packets.size(),"MODERN scene key count mismatch");
            std::unordered_set<uint32_t> unique;
            for(auto key:object_keys) require(unique.insert(key).second,"Duplicate MODERN scene object key");
        }
        for(const auto& packet:packets) {
            const auto& mesh=packet.geometry;
            require(mesh.deferred.empty() && mesh.vertex_view().size()%3==0 && mesh.line_view().size()%2==0
                && vr::model_eye_camera(vr::EyeCamera{},packet.model).has_value(),"Invalid MODERN scene packet");
        }
        auto next=std::make_unique<State>();
        next->device=device;next->keyed=!object_keys.empty();next->depth_test=depth_test;next->depth_write=depth_write;
        next->blend=blend;
        const bool compatible=state_ && state_->device==device;
        std::unordered_map<uint32_t,const State::Item*> prior;
        if(compatible && next->keyed && state_->keyed) for(const auto& item:state_->items) prior.emplace(item.key,&item);
        std::unordered_map<const std::vector<uint32_t>*,Buffer> shared_texels;
        if(compatible) for(const auto& item:state_->items)
            if(item.geometry->source.geometry.shared_texels)
                shared_texels.emplace(item.geometry->source.geometry.shared_texels.get(),item.geometry->texels);
        const auto upload=[&](SDL_GPUBufferUsageFlags usage,const void* bytes,uint32_t size) {
            auto buffer=owned(device,create_buffer(device,usage,size));
            uploads.add(buffer.get(),0,bytes,size);
            return buffer;
        };
        const uint32_t transparent=0;
        for(std::size_t index=0;index<packets.size();++index) {
            const auto& packet=packets[index];
            const auto& mesh=packet.geometry;
            const auto vertices=mesh.vertex_view();
            const auto lines=mesh.line_view();
            if(vertices.empty() && lines.empty()) continue;
            const uint32_t key=next->keyed?object_keys[index]:0;
            // Connected-grid packets need the grid compute stage; not ported yet.
            // Keep an undrawn item so item indices still match nonempty packets.
            if(!vertices.empty() && vertices.front().texture[3]==vr::gpu_connected_grid_flag) {
                auto empty=std::make_shared<State::Geometry>();empty->source=packet;
                next->items.push_back({packet.model,std::move(empty),key});++next->rejected;continue;
            }
            const State::Item* candidate=nullptr;
            if(next->keyed) {
                if(const auto found=prior.find(key);found!=prior.end()) candidate=found->second;
            } else if(compatible && !state_->keyed && next->items.size()<state_->items.size()) candidate=&state_->items[next->items.size()];
            if(candidate && vr::same_draw_geometry(std::span(&candidate->geometry->source,1),std::span(&packet,1))) {
                next->items.push_back({packet.model,candidate->geometry,key});++next->reused;continue;
            }
            auto geometry=std::make_shared<State::Geometry>();geometry->source=packet;
            geometry->photograph=!vertices.empty()
                && (vertices.front().texture[3]&vr::backdrop_texture_flag)==vr::backdrop_texture_flag;
            const auto words=mesh.texel_view();
            // Same mesh, changed payload (e.g. display-rate background
            // scroll): rewrite the existing buffers instead of allocating new
            // ones, which is slow on D3D12 at display rate.
            const auto& prior_mesh=candidate?candidate->geometry->source.geometry:mesh;
            const bool same_vertices=candidate && candidate->geometry->triangles
                && prior_mesh.vertex_view().size()==vertices.size()
                && (prior_mesh.vertex_view().data()==vertices.data()
                    || std::equal(vertices.begin(),vertices.end(),prior_mesh.vertex_view().begin()));
            const bool same_lines=candidate && candidate->geometry->lines
                && prior_mesh.line_view().size()==lines.size()
                && (prior_mesh.line_view().data()==lines.data()
                    || std::equal(lines.begin(),lines.end(),prior_mesh.line_view().begin()));
            const bool rewrite_texels=candidate && candidate->geometry->texels
                && !mesh.shared_texels && !prior_mesh.shared_texels
                && !words.empty() && prior_mesh.texel_view().size()==words.size();
            if(rewrite_texels) {
                geometry->texels=candidate->geometry->texels;
                const auto bytes=std::as_bytes(words);
                uploads.add(geometry->texels.get(),0,bytes.data(),uint32_t(bytes.size()),true);
            }
            else if(mesh.shared_texels && shared_texels.contains(mesh.shared_texels.get()))
                geometry->texels=shared_texels.at(mesh.shared_texels.get());
            else {
                const auto bytes=words.empty()?std::as_bytes(std::span(&transparent,1)):std::as_bytes(words);
                geometry->texels=upload(SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,bytes.data(),uint32_t(bytes.size()));
                if(mesh.shared_texels) shared_texels.emplace(mesh.shared_texels.get(),geometry->texels);
            }
            if(same_vertices) {
                geometry->triangles=candidate->geometry->triangles;geometry->triangle_count=candidate->geometry->triangle_count;
            } else if(!vertices.empty()) {
                geometry->triangles=upload(SDL_GPU_BUFFERUSAGE_VERTEX,vertices.data(),uint32_t(vertices.size_bytes()));
                geometry->triangle_count=uint32_t(vertices.size());
            }
            if(same_lines) {
                geometry->lines=candidate->geometry->lines;geometry->line_count=candidate->geometry->line_count;
            } else if(!lines.empty()) {
                geometry->lines=upload(SDL_GPU_BUFFERUSAGE_VERTEX,lines.data(),uint32_t(lines.size_bytes()));
                geometry->line_count=uint32_t(lines.size());
            }
            next->items.push_back({packet.model,std::move(geometry),key});
        }
        state_=std::move(next);status_="MODERN scene uploaded";return true;
    } catch(const std::exception& error) {status_=error.what();return false;}
}
bool SdlDrawPackets::record(SDL_GPUCommandBuffer* command,SDL_GPURenderPass* pass,SdlScenePipelines& pipelines,
    const vr::EyeCamera& camera) const {
    return record_range(command,pass,pipelines,camera,0,size());
}
bool SdlDrawPackets::record_range(SDL_GPUCommandBuffer* command,SDL_GPURenderPass* pass,SdlScenePipelines& pipelines,
    const vr::EyeCamera& camera,std::size_t first,std::size_t count) const {
    if(!state_ || !command || !pass || first>state_->items.size() || count>state_->items.size()-first) return false;
    for(const auto& item:std::span(state_->items).subspan(first,count)) {
        const auto& geometry=*item.geometry;
        auto draw_camera=camera;
        if(geometry.source.preserve_native_colour || (state_->keyed && vr::is_source_shadow_pass(item.key))) draw_camera.effects={0,0,0,camera.effects[3]};
        const auto combined=vr::model_eye_camera(draw_camera,item.model);
        if(!combined) return false;
        const auto constants=sdl_scene_constants(*combined);
        SDL_GPUBuffer* texels=geometry.texels.get();
        const auto draw=[&](SceneTopology topology,SceneBlend blend,const Buffer& vertices,uint32_t count) {
            auto* pipeline=pipelines.get(topology,blend,state_->depth_test,state_->depth_write);
            if(!pipeline) return false;
            SDL_BindGPUGraphicsPipeline(pass,pipeline);
            SDL_GPUBufferBinding binding{vertices.get(),0};
            SDL_BindGPUVertexBuffers(pass,0,&binding,1);
            SDL_BindGPUVertexStorageBuffers(pass,0,&texels,1);
            pipelines.bind_fragment_storage(pass,texels);
            SDL_PushGPUVertexUniformData(command,0,&constants,sizeof(constants));
            SDL_PushGPUFragmentUniformData(command,0,&constants,sizeof(constants));
            pipelines.push_line_brush(command,topology);
            SDL_DrawGPUPrimitives(pass,count,topology==SceneTopology::lines?pipelines.line_instances():1,0,0);
            return true;
        };
        if(geometry.triangles && !draw(SceneTopology::triangles,state_->blend?*state_->blend:geometry.photograph?SceneBlend::alpha:SceneBlend::opaque,
            geometry.triangles,geometry.triangle_count)) return false;
        if(geometry.lines && !draw(SceneTopology::lines,SceneBlend::opaque,geometry.lines,geometry.line_count)) return false;
    }
    return true;
}
}
