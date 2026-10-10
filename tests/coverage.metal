#include <metal_stdlib>
using namespace metal;
#include "starfox/render/metal_native_material_coverage.inc"
struct CoverageQuery {
    uint primitive;float bary_x,bary_y;uint pixel_x,pixel_y;
    uint count,byte_count,binding_words;
};
kernel void starfox_native_coverage_probe(device const uint* words [[buffer(0)]],
    device const CoverageQuery* queries [[buffer(1)]],device uint* output [[buffer(2)]],
    constant uint& query_count [[buffer(3)]],
    uint id [[thread_position_in_grid]]) {
    if(id>=query_count) return;
    CoverageQuery q=queries[id];
    output[id]=starfox_native_material_coverage(q.primitive,q.bary_x,q.bary_y,q.pixel_x,q.pixel_y,
        words,q.count,q.byte_count,q.binding_words);
}
#include "colour.metal"
