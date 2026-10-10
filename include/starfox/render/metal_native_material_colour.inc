// Native material colour is resolved only after candidate opacity succeeds.
// The canonical equations below come from metal_material_colour.inc, generated
// from unchanged calibrated_colour.hlsli and scene_colour.hlsli.
uint starfox_native_material_colour(uint primitive,float bary_x,float bary_y,
    uint pixel_x,uint pixel_y,device const uint* words,uint count,uint byte_count,
    uint binding_words) {
    uint packed=starfox_native_material_coverage(primitive,bary_x,bary_y,pixel_x,pixel_y,
        words,count,byte_count,binding_words);
    if(packed==0u) return 0u;
    uint at=primitive*16u;
    // Kind2 colours were already styled by geometry production.
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
