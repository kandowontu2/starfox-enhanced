#pragma once
#include "starfox/render/packed_projection.hpp"
#include "starfox/render/packed_faces.hpp"
#include "starfox/vr/source_span_layout.hpp"
#include <string>
namespace starfox::vr {
struct SourceAxisSettings {
    uint32_t point_count{},index_count{},fractional{1},has_residuals{};
    std::array<uint32_t,4> ranges{};
    std::array<float,4> projection{};
};
static_assert(sizeof(SourceAxisSettings)==48);
struct SourceAxisInputs {
    std::vector<uint32_t> indices;
    SourceAxisSettings settings;
};
// Indices refer to the original projected vertices, before any face-local
// fragment expansion. Source extrema are selected before GPU transformation.
bool prepare_source_axis_inputs(const assets::Shape&,const render::RenderPose&,
    const render::RenderSettings&,uint32_t projected_vertices,bool residuals,
    SourceAxisInputs&,std::string& error);
// CPU-authored inputs only: projection, visibility, ordering, clipping and
// span generation remain GPU work. All face arrays use bsp.faces indices,
// not triangle indices or the expanded ShapeBatch occurrence order.
struct SourceSpanModel {
    render::PackedProjection projection;
    render::PackedBsp bsp;
    render::PackedFaces faces;
    SourceSpanSettings spans;
    std::array<uint32_t,4> tree{};
    std::vector<render::ContinuousTransformPose> poses;
    std::vector<std::array<float,4>> projection_parameters;
    std::array<uint32_t,4> projection_settings{},visibility_settings{};
    std::array<uint32_t,8> bsp_settings{},clip_settings{};
    uint32_t graphics_wave_phase{};
    // Packed RGBA8; bit 0 enables command palette indices, bit 1 decodes
    // sRGB for an sRGB render target; bit 2 selects occurrence-indexed geometry
    // and materials (requires spans.ordered_mode=2). Low bits zero retain
    // caller material colours.
    std::array<uint32_t,256> graphics_palette{};
    uint32_t graphics_palette_flags{};
    bool graphics_unclipped{};
    // Source topology stays face-indexed; clip/span output is occurrence-indexed.
    // The caller must bind expanded warp geometry/materials before dispatch.
    bool warp_expanded{};
    bool fragmented{};
    uint32_t source_vertex_count{}; // Before fragment-local duplication; seeds source warp PRNG.
};
bool prepare_source_axis_model(const assets::Shape&,const render::RenderPose&,
    const render::RenderSettings&,uint32_t width,uint32_t height,
    SourceSpanModel&,SourceAxisInputs&,std::string& error);

// CPU-authored inputs for the three ordered warp stages. Outputs belong to
// traversal occurrences, while these templates/normals retain source face IDs.
struct SourceWarpInputs {
    render::PackedFaces templates;
    render::PackedWarpTextures textures;
    render::PackedWarpShading shading;
    std::vector<std::array<int32_t,4>> normals;
    decltype(render::RenderPose{}.depth_colour_tables) depth_colours{};
};
// Does not enable live warp dispatch: the caller must bind occurrence outputs
// before replacing the ordinary materials. Transactional on invalid inputs.
bool prepare_source_warp_inputs(const assets::Shape&,const render::RenderPose&,
    const render::RenderSettings&,const render::PackedProjection&,const render::PackedBsp&,
    SourceWarpInputs&,std::string& error,const render::PackedFaces* fragments=nullptr,uint32_t source_vertex_count=0);
enum class SourceSpanRegion : uint8_t {
    vertices,poses,points,residuals,triples,visibility,nodes,face_ids,trees,
    order,results,corners,polygons,projection_parameters,clipped,materials,
    commands,masks,projection_settings,visibility_settings,bsp_settings,
    clip_settings,span_settings,graphics_headers,graphics_lookup,palette,texture_bytes,graphics_polygons,count
};
struct SourceSpanRegionRange {uint64_t offset{},size{};};
enum class SourceAxisRegion : uint8_t {indices,endpoints,residuals,settings,count};
struct SourceAxisArenaLayout {
    std::array<SourceSpanRegionRange,static_cast<size_t>(SourceAxisRegion::count)> regions{};
    uint64_t bytes{};
    const SourceSpanRegionRange& operator[](SourceAxisRegion region) const noexcept {
        return regions[static_cast<size_t>(region)];
    }
};
// Appended after source projection storage; never aliases its input points.
bool layout_source_axis_inputs(const SourceAxisInputs&,uint64_t prefix_bytes,
    uint64_t storage_alignment,uint64_t uniform_alignment,uint64_t max_storage_range,
    uint64_t max_uniform_range,SourceAxisArenaLayout&,std::string& error);
enum class SourceWarpRegion : uint8_t {
    descriptors,result,normals,diffuse,depth_colours,texture_lookup,textures,coordinates,
    decoded,polygons,corners,materials,sequence_settings,material_settings,expand_settings,count
};
struct SourceWarpArenaLayout {
    std::array<SourceSpanRegionRange,static_cast<size_t>(SourceWarpRegion::count)> regions{};
    uint64_t bytes{}; // End offset, including a caller-owned prefix.
    const SourceSpanRegionRange& operator[](SourceWarpRegion region) const noexcept {
        return regions[static_cast<size_t>(region)];
    }
};
// Append warp input/output ranges to a resident arena. The original source
// topology, BSP order and visibility remain in the caller-owned prefix.
bool layout_source_warp_inputs(const SourceWarpInputs&,uint64_t prefix_bytes,
    uint64_t storage_alignment,uint64_t uniform_alignment,uint64_t max_storage_range,
    uint64_t max_uniform_range,SourceWarpArenaLayout&,std::string& error);
inline bool source_span_has_lines(const SourceSpanModel& model) noexcept {
    for(auto primitive:model.faces.primitives) if(primitive==render::PackedPrimitive::line) return true;
    return false;
}
inline bool source_span_has_ordinary_faces(const SourceSpanModel& model) noexcept {
    for(size_t i=0;i<model.faces.primitives.size();++i) {
        const auto primitive=model.faces.primitives[i];
        if(primitive==render::PackedPrimitive::line || primitive==render::PackedPrimitive::sprite
            || (primitive==render::PackedPrimitive::polygon && i<model.faces.materials.size()
                && model.faces.materials[i].textured)) return true;
    }
    return false;
}
inline uint32_t source_span_corner_capacity(const SourceSpanModel& model) noexcept {
    uint32_t count=3;
    for(const auto& polygon:model.faces.polygons) if(polygon[1]>count) count=polygon[1];
    for(const auto& face:model.bsp.faces) if(face.vertex_indices.size()>count) count=static_cast<uint32_t>(face.vertex_indices.size());
    for(auto primitive:model.faces.primitives) if(primitive==render::PackedPrimitive::sprite && count<4) count=4;
    if(!model.graphics_unclipped && source_span_has_ordinary_faces(model) && count<4) count=4;
    return count;
}
struct SourceSpanArenaLayout {
    std::array<SourceSpanRegionRange,static_cast<size_t>(SourceSpanRegion::count)> regions{};
    uint64_t bytes{};
    const SourceSpanRegionRange& operator[](SourceSpanRegion region) const noexcept {
        return regions[static_cast<size_t>(region)];
    }
};
// Limits are taken from the physical device. Every descriptor gets a nonzero
// range; output buffers and alignment padding count against the arena budget.
bool layout_source_span_model(const SourceSpanModel&,uint64_t storage_alignment,
    uint64_t uniform_alignment,uint64_t max_storage_range,uint64_t max_uniform_range,
    SourceSpanArenaLayout&,std::string& error);
// Initial input image, including zeroed GPU outputs/padding. No GPU-generated
// geometry is copied back into it. Upload only after prior GPU use completes.
bool source_span_upload_image(const SourceSpanModel&,const SourceSpanArenaLayout&,
    std::vector<std::byte>&,std::string& error);
// Checks upload compatibility without allocating or filling an arena image.
bool validate_source_span_upload(const SourceSpanModel&,const SourceSpanArenaLayout&,std::string& error);
struct SourceSpanInputWrite {uint64_t offset{};std::vector<std::byte> bytes;};
bool validate_source_axis_upload(const SourceAxisInputs&,const SourceAxisArenaLayout&,std::string& error);
// Input-only updates preserve endpoint/residual GPU outputs. Transactional.
bool source_axis_input_writes(const SourceAxisInputs&,const SourceAxisArenaLayout&,
    std::vector<SourceSpanInputWrite>&,std::string& error);
// Overrides only graphics lookup metadata after source/warp inputs are uploaded.
// Generated geometry remains GPU-owned in the combined arena.
bool source_warp_graphics_write(const SourceSpanModel&,const SourceSpanArenaLayout&,
    const SourceWarpInputs&,const SourceWarpArenaLayout&,SourceSpanInputWrite&,std::string& error);
bool source_warp_input_writes(const SourceWarpInputs&,const SourceWarpArenaLayout&,
    std::vector<SourceSpanInputWrite>&,std::string& error);
bool validate_source_warp_upload(const SourceWarpInputs&,const SourceWarpArenaLayout&,std::string& error);
bool source_span_input_writes(const SourceSpanModel&,const SourceSpanArenaLayout&,
    std::vector<SourceSpanInputWrite>&,std::string& error,std::vector<SourceSpanInputWrite>* recycle=nullptr);
// Transactional. Unsupported transforms/material stages reject explicitly.
bool prepare_source_span_model(const assets::Shape&,const render::RenderPose&,
    const render::RenderSettings&,uint32_t width,uint32_t height,
    SourceSpanModel&,std::string& error,bool unclipped=false);
}
