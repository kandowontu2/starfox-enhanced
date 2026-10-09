#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace starfox::vr {
// ABI of render/shaders/spans_portable.hlsl Settings. Do not reorder fields.
struct SourceSpanSettings {
    // ordered_mode: 0 direct polygons; 1 source-face BSP order;
    // 2 already-expanded occurrence slots, still gated by traversal status/count.
    uint32_t count{};int32_t width{},height{};uint32_t winding_independent{1};
    uint32_t render_scale{1},fractional{},ordered_mode{},polygon_count{};
    uint32_t order_first{},order_tree{},line_thickness{1},padding{};
    uint32_t mask_enabled{},mask_offset{},mask_stride{},mask_padding{};
};
static_assert(sizeof(SourceSpanSettings)==64);
static_assert(offsetof(SourceSpanSettings,mask_enabled)==48);
// Descriptor order: clipped polygons, materials, BSP order, BSP results,
// generated commands, coverage masks, settings. Even unused bindings are valid.
inline std::optional<std::array<uint64_t,7>> source_span_buffer_sizes(const SourceSpanSettings& s) {
    constexpr uint64_t budget=256ULL*1024*1024;
    if(!s.count || !s.polygon_count || s.count>65535U*32U
        || s.width<1 || s.height<1 || s.width>4096 || s.height>4096
        || !s.render_scale || s.render_scale>4 || s.winding_independent>1
        || s.fractional>1 || s.ordered_mode>2 || s.mask_enabled>1
        || s.line_thickness>4 || s.padding || s.mask_padding
        || (s.ordered_mode!=1 && s.count>s.polygon_count)) return std::nullopt;
    const uint64_t rows=uint64_t(s.count)*uint32_t(s.height);
    const uint64_t minimum_stride=((uint64_t(s.width)+31)/32)*4;
    if(s.mask_enabled && (s.mask_stride<minimum_stride || (s.mask_stride&3U) || (s.mask_offset&3U)))
        return std::nullopt;
    if(rows>budget/96 || (s.mask_enabled &&
        (s.mask_offset>budget || rows>(budget-s.mask_offset)/s.mask_stride))) return std::nullopt;
    std::array<uint64_t,7> sizes{
        uint64_t(s.polygon_count)*129*16,uint64_t(s.polygon_count)*96,
        s.ordered_mode==1?(uint64_t(s.order_first)+s.count)*4:4,
        s.ordered_mode?(uint64_t(s.order_tree)+1)*8:8,
        rows*96,s.mask_enabled?uint64_t(s.mask_offset)+rows*s.mask_stride:4,64};
    uint64_t total=0;
    for(auto size:sizes) {
        if(size>budget || total>budget-size) return std::nullopt;
        total+=size;
    }
    return sizes;
}
// Validate CPU-authored/retained payloads before a graphics descriptor upload.
// Resident compute buffers use the producer's separately checked allocation ABI.
inline bool source_span_payload_valid(std::span<const uint32_t> words,uint32_t header,uint32_t width) {
    if(!width || width>4096 || header>words.size() || words.size()-header<4) return false;
    const uint64_t commands=words[header],masks=words[header+1],height=words[header+2];
    if(!height || height>4096 || commands>words.size() || height*24>words.size()-commands
        || (words[header+3]&0x7ff00000U)) return false;
    for(uint64_t y=0;y<height;++y) {
        const auto row=words.subspan(size_t(commands+y*24),24);
        if(row[23]&~7U || row[21]) return false;
        if(int32_t(row[2])<=int32_t(row[0])) continue;
        if(row[1]!=y || row[3]!=y+1) return false;
        if(row[23]&4U) {
            const uint64_t offset=row[8],stride=row[9];
            if((offset&3U) || (stride&3U) || stride<((width+31U)/32U)*4U || row[10]<height) return false;
            const uint64_t end=masks+(offset+y*stride)/4+(width+31U)/32U;
            if(end>words.size()) return false;
        }
    }
    return true;
}
}
