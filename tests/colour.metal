// Included after the unchanged coverage decoder/probe. Colour work occurs
// only after an opaque sample is selected, never for query-candidate opacity.
#include "starfox/render/metal_material_colour.inc"
uint starfox_native_material_colour(CoverageQuery q,device const uint* words) {
    uint packed=starfox_native_material_coverage(q.primitive,q.bary_x,q.bary_y,q.pixel_x,q.pixel_y,
        words,q.count,q.byte_count,q.binding_words);
    if(packed==0u) return 0u;
    uint at=q.primitive*16u;
    // Native kind2 solids have already been styled by geometry production.
    if(words[at+15u]==2u) return packed;
    float3 colour=float3(packed&255u,(packed>>8u)&255u,(packed>>16u)&255u)/255.0f;
    uint flags=words[at+7u];
    if((flags&2u)!=0u) colour=calibrated_decode_srgb(colour);
    uint4 style=uint4(words[at+8u],words[at+9u],words[at+10u],words[at+11u]);
    colour=scene_styled_colour(float4(colour,1.0f),style).rgb;
    if((style.z&1u)!=0u) colour=calibrated_encode_srgb(colour);
    uint3 rgb=uint3(round(clamp(colour,0.0f,1.0f)*255.0f));
    return rgb.x|(rgb.y<<8u)|(rgb.z<<16u)|0xff000000u;
}
kernel void starfox_native_colour_probe(device const uint* words [[buffer(0)]],
    device const CoverageQuery* queries [[buffer(1)]],device uint* output [[buffer(2)]],
    constant uint& query_count [[buffer(3)]],uint id [[thread_position_in_grid]]) {
    if(id>=query_count) return;
    output[id]=starfox_native_material_colour(queries[id],words);
}
