#include "starfox/modern/sdl_source_scene.hpp"
#include "starfox/modern/frame_policy.hpp"
#include "starfox/vr/modern_frame.hpp"
#include "starfox/vr/source_span_model.hpp"
#include "starfox/vr/source_ray_topology.hpp"
#include "render/shaders/generated/spans_portable.hpp"
#include "render/shaders/generated/clip_continuous_portable.hpp"
#include "render/shaders/generated/continuous_portable.hpp"
#include "render/shaders/generated/continuous_visibility_portable.hpp"
#include "render/shaders/generated/bsp_portable.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
namespace starfox::modern {
namespace {
using R=vr::SourceSpanRegion;
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what);}
std::string sdl_error(const char* what) {return std::string(what)+": "+SDL_GetError();}
// Region bindings per stage, exactly as vr::VulkanSourceBindings: read-only
// inputs, then read-write outputs, then the settings uniform block.
constexpr std::array<std::array<R,8>,5> stage_regions{{
    {R::clipped,R::materials,R::order,R::results,R::commands,R::masks,R::span_settings},
    {R::points,R::corners,R::polygons,R::visibility,R::projection_parameters,R::residuals,R::clipped,R::clip_settings},
    {R::vertices,R::poses,R::points,R::residuals,R::projection_settings},
    {R::points,R::triples,R::visibility,R::visibility_settings},
    {R::nodes,R::visibility,R::face_ids,R::trees,R::order,R::results,R::bsp_settings}}};
constexpr std::array<unsigned,5> stage_counts{7,8,5,4,7},stage_inputs{4,6,2,2,4},stage_lanes{32,32,64,64,32};
// Dispatch order (vr::VulkanSourceModel::record): projection, visibility,
// BSP, clip, spans. Ordinary (unclipped) models stop after BSP.
constexpr std::array<unsigned,5> stage_order{2,3,4,1,0};
SDL_GPUBuffer* create_buffer(SDL_GPUDevice* device,SDL_GPUBufferUsageFlags usage,uint64_t size) {
    require(size>0 && size<=0xffffffffULL,"Invalid MODERN source buffer size");
    SDL_GPUBufferCreateInfo info{};info.usage=usage;info.size=uint32_t(size);
    auto* buffer=SDL_CreateGPUBuffer(device,&info);
    require(buffer,sdl_error("Create source buffer"));
    return buffer;
}
}
SdlSourceStages::~SdlSourceStages() {release();}
void SdlSourceStages::release() noexcept {
    if(device_) for(auto*& pipeline:pipelines_) if(pipeline) SDL_ReleaseGPUComputePipeline(device_,pipeline);
    pipelines_={};device_=nullptr;
}
SDL_GPUComputePipeline* SdlSourceStages::get(SDL_GPUDevice* device,unsigned stage) {
    if(stage>=5 || !device) return nullptr;
    if(device_!=device) {release();device_=device;}
    if(pipelines_[stage]) return pipelines_[stage];
    const auto formats=SDL_GetGPUShaderFormats(device);
    const bool spirv=(formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
    const bool dxil=!spirv && (formats&SDL_GPU_SHADERFORMAT_DXIL)!=0;
    if(!spirv && !dxil) {status_="MODERN source stages need a SPIR-V or DXIL backend";return nullptr;}
    // Same shader selection as desktop gpu_clip.cpp, including Intel's
    // compact continuous-clip DXIL.
    const bool intel=dxil && SDL_GetNumberProperty(SDL_GetGPUDeviceProperties(device),"starfox.gpu.vendor_id",0)==0x8086;
    SDL_GPUComputePipelineCreateInfo info{};
    info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:SDL_GPU_SHADERFORMAT_DXIL;
    const auto pick=[&](const unsigned char* spirv_code,std::size_t spirv_size,const unsigned char* dxil_code,std::size_t dxil_size) {
        info.code=spirv?spirv_code:dxil_code;info.code_size=spirv?spirv_size:dxil_size;
    };
    switch(stage) {
#if defined(_WIN32)
    case 0: pick(render::spans_shader::spirv,sizeof(render::spans_shader::spirv),render::spans_shader::dxil,sizeof(render::spans_shader::dxil));break;
    case 1: pick(render::clip_continuous_shader::spirv,sizeof(render::clip_continuous_shader::spirv),
        intel?render::clip_continuous_shader::intel_dxil:render::clip_continuous_shader::dxil,
        intel?sizeof(render::clip_continuous_shader::intel_dxil):sizeof(render::clip_continuous_shader::dxil));break;
#else
    case 0: pick(render::spans_shader::spirv,sizeof(render::spans_shader::spirv),nullptr,0);break;
    case 1: pick(render::clip_continuous_shader::spirv,sizeof(render::clip_continuous_shader::spirv),nullptr,0);break;
#endif
    case 2: pick(render::continuous_shader::spirv,sizeof(render::continuous_shader::spirv),render::continuous_shader::dxil,sizeof(render::continuous_shader::dxil));break;
    case 3: pick(render::continuous_visibility_shader::spirv,sizeof(render::continuous_visibility_shader::spirv),
        render::continuous_visibility_shader::dxil,sizeof(render::continuous_visibility_shader::dxil));break;
    default: pick(render::bsp_shader::spirv,sizeof(render::bsp_shader::spirv),render::bsp_shader::dxil,sizeof(render::bsp_shader::dxil));break;
    }
    if(!info.code || !info.code_size) {status_="MODERN source stage shader unavailable on this platform";return nullptr;}
    info.entrypoint="main";
    info.num_readonly_storage_buffers=stage_inputs[stage];
    info.num_readwrite_storage_buffers=stage_counts[stage]-stage_inputs[stage]-1;
    info.num_uniform_buffers=1;
    info.threadcount_x=stage_lanes[stage];info.threadcount_y=info.threadcount_z=1;
    pipelines_[stage]=SDL_CreateGPUComputePipeline(device,&info);
    if(!pipelines_[stage]) status_=sdl_error("Create MODERN source stage");
    return pipelines_[stage];
}

struct SdlSourceModel::State {
    SDL_GPUDevice* device{};
    vr::SourceSpanArenaLayout layout;
    std::vector<std::byte> image; // CPU mirror of the arena's inputs (settings are pushed from here).
    std::array<SDL_GPUBuffer*,size_t(R::count)> regions{};
    SDL_GPUBuffer* arena{};
    SDL_GPUBuffer* triangles{};SDL_GPUBuffer* lines{};
    // Shadow caster topology (vr::source_ray_topology), uploaded on change.
    SDL_GPUBuffer* ray_topology{};uint32_t ray_capacity{};
    std::vector<std::array<uint32_t,4>> ray_triangles;
    uint32_t ray_points{},ray_corners{},ray_mode{};
    uint32_t triangle_count{},line_count{};
    std::array<uint32_t,5> counts{};
    uint32_t corner_capacity{3};
    float units{};
    bool unclipped{},has_lines{},mixed{},compute_dirty{true};
    std::vector<vr::SourceSpanInputWrite> inputs,scratch;
    ~State() {
        if(!device) return;
        for(auto* buffer:regions) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        for(auto* buffer:{arena,triangles,lines,ray_topology}) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
    }
    bool active(unsigned stage) const noexcept {return !unclipped || stage>=2;}
    // A model without polygons (or with unusable topology) casts nothing.
    void update_ray_topology(SdlUploadBatch& uploads,const vr::SourceSpanModel& model) {
        std::vector<std::array<uint32_t,4>> next;std::string error;
        ray_points=model.projection_settings[0];ray_corners=uint32_t(model.faces.corners.size());
        ray_mode=model.projection_settings[2];
        if(!vr::source_ray_topology(model.faces,ray_points,next,error)) next.clear();
        if(next==ray_triangles) return;
        ray_triangles=std::move(next);
        if(ray_triangles.empty()) return;
        const auto bytes=uint32_t(ray_triangles.size()*sizeof(ray_triangles[0]));
        if(bytes>ray_capacity) {
            if(ray_topology) SDL_ReleaseGPUBuffer(device,ray_topology);
            ray_topology=create_buffer(device,SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes);ray_capacity=bytes;
        }
        uploads.add(ray_topology,0,ray_triangles.data(),bytes,true);
    }
};
SdlSourceModel::SdlSourceModel()=default;
SdlSourceModel::~SdlSourceModel()=default;
float SdlSourceModel::units() const noexcept {return state_?state_->units:0;}
bool SdlSourceModel::unclipped() const noexcept {return state_ && state_->unclipped;}
bool SdlSourceModel::initialize(SDL_GPUDevice* device,SdlUploadBatch& uploads,const vr::SourceSpanModel& model,float units) {
    try {
        require(device!=nullptr,"Missing MODERN source device");
        require(!model.warp_expanded,"MODERN source warp stage not ported yet");
        require(std::isfinite(units) && units>0,"Invalid MODERN source units");
        auto next=std::make_unique<State>();
        next->device=device;next->units=units;
        next->counts={model.spans.count,model.spans.polygon_count,model.projection_settings[0],model.visibility_settings[0],1};
        require(next->counts[2]==model.projection.continuous_vertices.size() && next->counts[3]==model.projection.visibility_faces.size()
            && model.projection_settings[1]==model.poses.size(),"Inconsistent MODERN source dispatch counts");
        for(unsigned stage=0;stage<5;++stage) {
            const uint64_t groups=(uint64_t(next->counts[stage])+stage_lanes[stage]-1)/stage_lanes[stage];
            require(next->counts[stage] && groups<=65535,"MODERN source model exceeds dispatch limits");
        }
        std::string error;
        // SDL binds whole region buffers, so only the arena's word offsets matter.
        require(vr::layout_source_span_model(model,16,16,256ULL*1024*1024,65536,next->layout,error),error);
        require(vr::source_span_upload_image(model,next->layout,next->image,error),error);
        next->unclipped=model.graphics_unclipped;
        next->has_lines=vr::source_span_has_lines(model);
        next->mixed=!next->unclipped && vr::source_span_has_ordinary_faces(model);
        next->corner_capacity=vr::source_span_corner_capacity(model);
        require(!next->unclipped || next->corner_capacity<=32,"Source polygon template exceeds corner limit");
        next->arena=create_buffer(device,SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,next->layout.bytes);
        uploads.add(next->arena,0,next->image.data(),uint32_t(next->image.size()));
        for(unsigned stage=0;stage<5;++stage) if(next->active(stage))
            for(unsigned binding=0;binding+1<stage_counts[stage];++binding) {
                const auto region=stage_regions[stage][binding];
                auto& buffer=next->regions[size_t(region)];
                if(buffer) continue;
                const auto& range=next->layout[region];
                buffer=create_buffer(device,SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,range.size);
                uploads.add(buffer,0,next->image.data()+range.offset,uint32_t(range.size));
            }
        // Cover templates (vr::VulkanSourceModel::cover_geometry, non-axis).
        const uint32_t triangles=(next->unclipped || next->mixed)?next->corner_capacity-2:2U;
        require(next->counts[0]<=4'000'000/(triangles*3),"Source template budget exceeded");
        std::vector<vr::SceneVertex> vertices;vertices.reserve(size_t(next->counts[0])*triangles*3);
        const auto lookup=uint32_t(next->layout[R::graphics_lookup].offset/4);
        for(uint32_t slot=0;slot<next->counts[0];++slot) for(uint32_t triangle=0;triangle<triangles;++triangle)
            for(uint32_t corner:{0U,triangle+1,triangle+2}) {
                vr::SceneVertex vertex{};vertex.color[3]=1;
                vertex.texture[0]=lookup;vertex.texture[1]=slot;vertex.texture[2]=corner;
                vertex.texture[3]=131072U|(next->unclipped?(262144U|4194304U):524288U);
                if(next->mixed) vertex.texture[3]|=33554432U;
                vertex.billboard[1]=units;vertices.push_back(vertex);
            }
        next->triangle_count=uint32_t(vertices.size());
        next->triangles=create_buffer(device,SDL_GPU_BUFFERUSAGE_VERTEX,vertices.size()*sizeof(vr::SceneVertex));
        uploads.add(next->triangles,0,vertices.data(),uint32_t(vertices.size()*sizeof(vr::SceneVertex)));
        std::vector<vr::SceneVertex> lines;
        if(next->has_lines) {
            lines.reserve(size_t(next->counts[0])*2);
            for(uint32_t slot=0;slot<next->counts[0];++slot) for(uint32_t corner=0;corner<2;++corner) {
                auto vertex=vertices.front();vertex.texture[1]=slot;vertex.texture[2]=corner;
                vertex.texture[3]|=16777216U;lines.push_back(vertex);
            }
            next->line_count=uint32_t(lines.size());
            next->lines=create_buffer(device,SDL_GPU_BUFFERUSAGE_VERTEX,lines.size()*sizeof(vr::SceneVertex));
            uploads.add(next->lines,0,lines.data(),uint32_t(lines.size()*sizeof(vr::SceneVertex)));
        }
        next->update_ray_topology(uploads,model);
        state_=std::move(next);status_="MODERN source model ready";return true;
    } catch(const std::exception& error) {status_=error.what();return false;}
}
bool SdlSourceModel::update(SdlUploadBatch& uploads,const vr::SourceSpanModel& model) {
    if(!state_) return false;
    auto& s=*state_;
    try {
        const std::array<uint32_t,5> counts{model.spans.count,model.spans.polygon_count,
            model.projection_settings[0],model.visibility_settings[0],1};
        if(model.warp_expanded || model.graphics_unclipped!=s.unclipped || vr::source_span_has_lines(model)!=s.has_lines
            || (!model.graphics_unclipped && vr::source_span_has_ordinary_faces(model))!=s.mixed
            || vr::source_span_corner_capacity(model)!=s.corner_capacity || counts!=s.counts
            || counts[2]!=model.projection.continuous_vertices.size() || counts[3]!=model.projection.visibility_faces.size()
            || model.projection_settings[1]!=model.poses.size()) {
            status_="MODERN source update requires compatible dispatch counts";return false;
        }
        std::string error;
        if(!vr::validate_source_span_upload(model,s.layout,error)) {status_=error;return false;}
        if(!vr::source_span_input_writes(model,s.layout,s.inputs,error,&s.scratch)) {status_=error;return false;}
        bool changed=false;
        for(size_t i=0;i<s.inputs.size();++i) {
            const auto& write=s.inputs[i];
            // scratch holds the previous snapshot after the transactional swap;
            // unchanged static inputs need no GPU rewrite (as VR).
            if(i<s.scratch.size() && write.offset==s.scratch[i].offset && write.bytes==s.scratch[i].bytes) continue;
            const uint64_t begin=write.offset,end=write.offset+write.bytes.size();
            require(end<=s.image.size(),"MODERN source write outside arena");
            std::memcpy(s.image.data()+begin,write.bytes.data(),write.bytes.size());
            uploads.add(s.arena,uint32_t(begin),s.image.data()+begin,uint32_t(write.bytes.size()));changed=true;
            for(size_t region=0;region<s.regions.size();++region) {
                if(!s.regions[region]) continue;
                const auto& range=s.layout.regions[region];
                const uint64_t overlap_begin=std::max(begin,range.offset),overlap_end=std::min(end,range.offset+range.size);
                if(overlap_begin<overlap_end)
                    uploads.add(s.regions[region],uint32_t(overlap_begin-range.offset),
                        s.image.data()+overlap_begin,uint32_t(overlap_end-overlap_begin));
            }
        }
        if(changed) s.compute_dirty=true;
        s.update_ray_topology(uploads,model);
        status_="MODERN source model updated";return true;
    } catch(const std::exception& error) {status_=error.what();s.inputs.clear();s.scratch.clear();return false;}
}
bool SdlSourceModel::ray_source(RaySource& output) const noexcept {
    if(!state_ || state_->ray_triangles.empty() || !state_->ray_topology) return false;
    const auto& s=*state_;
    output={s.arena,s.ray_topology,uint32_t(s.ray_triangles.size()),s.ray_points,s.ray_corners,s.ray_mode,
        uint32_t(s.layout[R::points].offset/4),uint32_t(s.layout[R::residuals].offset/4),uint32_t(s.layout[R::corners].offset/4)};
    return true;
}
bool SdlSourceModel::record_compute(SDL_GPUCommandBuffer* command,SdlSourceStages& stages,bool& publish) {
    publish=false;
    if(!state_ || !command) return false;
    auto& s=*state_;
    if(!s.compute_dirty) return true;
    for(unsigned stage:stage_order) {
        if(!s.active(stage)) continue;
        auto* pipeline=stages.get(s.device,stage);
        if(!pipeline) {status_=stages.status();return false;}
        const unsigned inputs=stage_inputs[stage],outputs=stage_counts[stage]-inputs-1;
        std::array<SDL_GPUStorageBufferReadWriteBinding,2> writable{};
        for(unsigned i=0;i<outputs;++i) writable[i].buffer=s.regions[size_t(stage_regions[stage][inputs+i])];
        std::array<SDL_GPUBuffer*,6> readable{};
        for(unsigned i=0;i<inputs;++i) readable[i]=s.regions[size_t(stage_regions[stage][i])];
        const auto& settings=s.layout[stage_regions[stage][stage_counts[stage]-1]];
        SDL_PushGPUComputeUniformData(command,0,s.image.data()+settings.offset,uint32_t(settings.size));
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,writable.data(),outputs);
        if(!pass) {status_=sdl_error("Begin MODERN source stage");return false;}
        SDL_BindGPUComputePipeline(pass,pipeline);
        SDL_BindGPUComputeStorageBuffers(pass,0,readable.data(),inputs);
        SDL_DispatchGPUCompute(pass,(s.counts[stage]+stage_lanes[stage]-1)/stage_lanes[stage],1,1);
        SDL_EndGPUComputePass(pass);
    }
    s.compute_dirty=false;publish=true;
    return true;
}
void SdlSourceModel::publish_outputs(SDL_GPUCopyPass* copy) const {
    // Publish every stage output into the graphics arena at VR's offsets.
    if(!state_ || !copy) return;
    const auto& s=*state_;
    for(unsigned stage=0;stage<5;++stage) if(s.active(stage))
        for(unsigned i=stage_inputs[stage];i+1<stage_counts[stage];++i) {
            const auto region=stage_regions[stage][i];
            const auto& range=s.layout[region];
            SDL_GPUBufferLocation source{s.regions[size_t(region)],0};
            SDL_GPUBufferLocation destination{s.arena,uint32_t(range.offset)};
            SDL_CopyGPUBufferToBuffer(copy,&source,&destination,uint32_t(range.size),false);
        }
}
bool SdlSourceModel::record_graphics(SDL_GPUCommandBuffer* command,SDL_GPURenderPass* pass,SdlScenePipelines& pipelines,
    const vr::EyeCamera& camera) const {
    if(!state_ || !command || !pass) return false;
    const auto& s=*state_;
    const auto constants=sdl_scene_constants(camera);
    auto* triangles=pipelines.get(SceneTopology::triangles,SceneBlend::opaque,true,true);
    auto* lines=s.has_lines?pipelines.get(SceneTopology::lines,SceneBlend::opaque,true,true):nullptr;
    if(!triangles || (s.has_lines && !lines)) return false;
    const auto bind=[&](SDL_GPUGraphicsPipeline* pipeline,SDL_GPUBuffer* vertices) {
        SDL_BindGPUGraphicsPipeline(pass,pipeline);
        SDL_GPUBufferBinding binding{vertices,0};
        SDL_BindGPUVertexBuffers(pass,0,&binding,1);
        SDL_GPUBuffer* arena=s.arena;
        SDL_BindGPUVertexStorageBuffers(pass,0,&arena,1);
        pipelines.bind_fragment_storage(pass,arena);
        SDL_PushGPUVertexUniformData(command,0,&constants,sizeof(constants));
        SDL_PushGPUFragmentUniformData(command,0,&constants,sizeof(constants));
        pipelines.push_line_brush(command,pipeline==lines?SceneTopology::lines:SceneTopology::triangles);
    };
    if(!s.has_lines) {
        bind(triangles,s.triangles);
        SDL_DrawGPUPrimitives(pass,s.triangle_count,1,0,0);return true;
    }
    // Lines and polygons interleave per slot to keep source painter order.
    const uint32_t per_slot=(s.corner_capacity-2)*3;
    for(uint32_t slot=0;slot<s.counts[0];++slot) {
        bind(triangles,s.triangles);SDL_DrawGPUPrimitives(pass,per_slot,1,slot*per_slot,0);
        bind(lines,s.lines);SDL_DrawGPUPrimitives(pass,2,pipelines.line_instances(),slot*2,0);
    }
    return true;
}

struct SdlSourceScene::State {
    struct Draw {std::size_t legacy_first{},legacy_count{};std::shared_ptr<SdlSourceModel> model;
        vr::Matrix4 placement{};std::size_t packet_index{};};
    SDL_GPUDevice* device{};
    std::vector<Draw> draws;
    std::vector<vr::DrawPacket> source_packets;
    std::vector<uint32_t> handles;
    std::unique_ptr<SdlDrawPackets> packets;
    // Per source packet: the object surrounds the camera (tunnel sections,
    // hangars). Those draw first as a painter, in native order.
    std::vector<bool> enclosing;
};
SdlSourceScene::SdlSourceScene()=default;
SdlSourceScene::~SdlSourceScene() {release();}
void SdlSourceScene::release() noexcept {state_.reset();stages_.release();uploads_.release();}
bool SdlSourceScene::initialize(SDL_GPUDevice* device,SDL_GPUCommandBuffer* command,const vr::SourceModelPackets& source,
    std::span<const uint16_t> moving) {
    try {
        require(device && command,"Missing MODERN scene device/command");
        require(source.pending.empty() && source.handles.size()==source.packets.size() && source.packets.size()<=4096,
            "Incomplete ordered source scene");
        require(source_models_supported(source.compute_models),"Unsupported MODERN warp/axis model: use the complete fallback frame");
        std::vector<const vr::SourceComputeModel*> requests(source.packets.size());
        for(const auto& request:source.compute_models) {
            require(request.packet_index<source.packets.size() && !requests[request.packet_index]
                && source.handles[request.packet_index]==request.key,"Invalid ordered compute placeholder");
            requests[request.packet_index]=&request;
        }
        std::unordered_map<uint32_t,std::shared_ptr<SdlSourceModel>> previous;
        if(state_ && state_->device==device) {
            for(const auto& draw:state_->draws) if(draw.model) previous.emplace(state_->handles[draw.packet_index],draw.model);
        }
        auto next=std::make_unique<State>();
        next->device=device;next->source_packets=source.packets;next->handles=source.handles;
        // Retain packet geometry across frames (keyed reuse inside SdlDrawPackets).
        next->packets=state_ && state_->device==device?std::move(state_->packets):std::make_unique<SdlDrawPackets>();
        std::size_t legacy=0,first=0,unsupported=0;
        for(std::size_t index=0;index<source.packets.size();++index) {
            const auto* request=requests[index];
            if(request) {
                std::shared_ptr<SdlSourceModel> model;
                if(const auto found=previous.find(request->key);found!=previous.end()
                    && found->second->units()==request->units && found->second->unclipped()==request->model.graphics_unclipped
                    && found->second->update(uploads_,request->model)) model=found->second;
                if(!model) {
                    model=std::make_shared<SdlSourceModel>();
                    require(model->initialize(device,uploads_,request->model,request->units),model->status());
                }
                next->draws.push_back({first,legacy-first,std::move(model),source.packets[index].model,index});first=legacy;
            } else {
                const auto& mesh=source.packets[index].geometry;
                if(!mesh.vertex_view().empty() || !mesh.line_view().empty()) ++legacy;
            }
        }
        next->draws.push_back({first,legacy-first,{}});
        // Painter objects (vr::painter_scenery, shared with the PCVR app).
        next->enclosing=vr::painter_scenery(source,moving);
        if(!next->packets->initialize(device,uploads_,source.packets,source.handles)) {
            const auto reason=next->packets->status();
            if(state_) state_->packets=std::move(next->packets); // Keep retained geometry for the next attempt.
            throw std::runtime_error(reason);
        }
        require(legacy==next->packets->size(),"Ordered MODERN item count mismatch");
        require(uploads_.flush(device,command),uploads_.status());
        unsupported_=unsupported;compute_models_=source.compute_models.size();
        state_=std::move(next);status_="MODERN ordered source scene ready";return true;
    } catch(const std::exception& error) {uploads_.clear();status_=error.what();return false;}
}
std::vector<SdlSourceScene::RayCaster> SdlSourceScene::ray_casters() const {
    std::vector<RayCaster> casters;
    if(!state_) return casters;
    for(const auto& draw:state_->draws) if(draw.model)
        casters.push_back({state_->handles[draw.packet_index],draw.placement,draw.model->units(),draw.model.get(),
            draw.packet_index<state_->enclosing.size() && state_->enclosing[draw.packet_index]});
    return casters;
}
bool SdlSourceScene::record_compute(SDL_GPUCommandBuffer* command) {
    if(!state_ || !command) return false;
    std::vector<const SdlSourceModel*> published;
    for(auto& draw:state_->draws) {
        bool publish=false;
        if(draw.model && !draw.model->record_compute(command,stages_,publish)) {status_=draw.model->status();return false;}
        if(publish) published.push_back(draw.model.get());
    }
    if(published.empty()) return true;
    // One copy pass publishes every model's outputs into its arena.
    auto* copy=SDL_BeginGPUCopyPass(command);
    if(!copy) {status_=sdl_error("Begin MODERN arena publish");return false;}
    for(const auto* model:published) model->publish_outputs(copy);
    SDL_EndGPUCopyPass(copy);
    return true;
}
bool SdlSourceScene::record(SDL_GPUCommandBuffer* command,SDL_GPURenderPass* pass,SdlScenePipelines& pipelines,
    const vr::EyeCamera& camera) const {
    if(!state_) return false;
    // Pass 0: enclosing objects as a painter in native order (the cartridge
    // overwrites nested tunnel sections regardless of depth). Pass 1: every
    // other object, depth-tested against them.
    struct Restore {SdlScenePipelines& p;~Restore() {p.set_painter(false);}} restore{pipelines};
    for(const bool painter:{true,false}) {
        pipelines.set_painter(painter);
        std::size_t legacy_packet=0;
        for(const auto& draw:state_->draws) {
            // Legacy items in this range belong to the packets before draw.packet_index.
            for(std::size_t item=0;item<draw.legacy_count;++item) {
                while(legacy_packet<state_->source_packets.size()) {
                    const auto& mesh=state_->source_packets[legacy_packet].geometry;
                    const bool compute=std::any_of(state_->draws.begin(),state_->draws.end(),
                        [&](const auto& d) {return d.model && d.packet_index==legacy_packet;});
                    if(!compute && (!mesh.vertex_view().empty() || !mesh.line_view().empty())) break;
                    ++legacy_packet;
                }
                const bool enclosing=legacy_packet<state_->enclosing.size() && state_->enclosing[legacy_packet];
                const bool hidden=hide_source_shadows_ && legacy_packet<state_->handles.size()
                    && vr::is_source_shadow_pass(state_->handles[legacy_packet]);
                if(enclosing==painter && !hidden && !state_->packets->record_range(command,pass,pipelines,camera,draw.legacy_first+item,1))
                    return false;
                ++legacy_packet;
            }
            if(!draw.model) continue;
            if(state_->enclosing[draw.packet_index]!=painter) continue;
            if(hide_source_shadows_ && vr::is_source_shadow_pass(state_->handles[draw.packet_index])) continue;
            auto draw_camera=camera;
            if(state_->source_packets[draw.packet_index].preserve_native_colour
                || vr::is_source_shadow_pass(state_->handles[draw.packet_index])) draw_camera.effects={0,0,0,camera.effects[3]};
            const auto combined=vr::model_eye_camera(draw_camera,draw.placement);
            if(!combined || !draw.model->record_graphics(command,pass,pipelines,*combined)) return false;
        }
    }
    return true;
}
}
