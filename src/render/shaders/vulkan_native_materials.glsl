// Shared resident kind2/kind3 coverage for Vulkan shadows and reflections.
// The consumer supplies RayMaterial and a raw uint Materials word[] buffer.
RayMaterial load_ray_material(uint primitive) {
    uint at=primitive*16u;RayMaterial m;
    m.uv0=uintBitsToFloat(uvec2(materials.word[at],materials.word[at+1u]));
    m.uv1=uintBitsToFloat(uvec2(materials.word[at+2u],materials.word[at+3u]));
    m.uv2=uintBitsToFloat(uvec2(materials.word[at+4u],materials.word[at+5u]));
    m.textured=materials.word[at+6u];m.dither=materials.word[at+7u];
    m.even_colour=materials.word[at+8u];m.odd_colour=materials.word[at+9u];
    m.colour_base=materials.word[at+10u];m.face=materials.word[at+11u];
    m.offset=materials.word[at+12u];m.u_mask=materials.word[at+13u];
    m.v_mask=materials.word[at+14u];m.reserved=materials.word[at+15u];return m;
}
uint native_material_sample(uint primitive,vec2 barycentric,uvec2 pixel,uint count,uint bytes,bool coverage_only) {
    if(primitive>=count || count>0x03ffffffu || count*16u>uint(materials.word.length())
        || (bytes&3u)!=0u || bytes<count*64u || bytes/4u>uint(materials.word.length())) return 0u;
    uint at=primitive*16u,kind=materials.word[at+15u];
    // Native solids are already styled by the geometry producer. Do not read
    // UV/atlas/style fields or apply the selected colour effect a second time.
    if(kind==2u) {
        uint scale=materials.word[at+7u];
        if(materials.word[at+6u]!=0u || scale>4096u) return 0u;
        uint divisor=max(scale,1u);
        bool odd=scale!=0u && (((pixel.x/divisor)^(pixel.y/divisor))&1u)!=0u;
        uint packed=materials.word[at+(odd?9u:8u)];return (packed>>24u)==255u?packed:0u;
    }
    if(kind!=3u || materials.word[at+6u]!=1u) return 0u;
    uint flags=materials.word[at+7u],offset=materials.word[at+12u];
    uvec2 mask=uvec2(materials.word[at+13u],materials.word[at+14u]);
    bool indexed=(flags&536870912u)!=0u;
    if((flags&~(7u|536870912u))!=0u || (flags&1u)==0u || any(greaterThan(mask,uvec2(4095)))
        || any(notEqual(mask&(mask+1u),uvec2(0))) || (offset&3u)!=0u || offset<count*64u || offset>bytes) return 0u;
    uint pixels=(mask.x+1u)*(mask.y+1u),words=indexed?256u+(pixels+3u)/4u:pixels;
    if(words>(bytes-offset)/4u) return 0u;
    vec2 a=uintBitsToFloat(uvec2(materials.word[at],materials.word[at+1u]));
    vec2 b=uintBitsToFloat(uvec2(materials.word[at+2u],materials.word[at+3u]));
    vec2 c=uintBitsToFloat(uvec2(materials.word[at+4u],materials.word[at+5u]));
    if(any(isnan(a)) || any(isinf(a)) || any(greaterThan(abs(a),vec2(65536)))
        || any(isnan(b)) || any(isinf(b)) || any(greaterThan(abs(b),vec2(65536)))
        || any(isnan(c)) || any(isinf(c)) || any(greaterThan(abs(c),vec2(65536)))) return 0u;
    vec2 uv=a*(1.0-barycentric.x-barycentric.y)+b*barycentric.x+c*barycentric.y;
    if((flags&4u)!=0u && (any(lessThan(uv,vec2(0))) || any(greaterThanEqual(uv,vec2(mask+1u))))) return 0u;
    uvec2 xy=uvec2(ivec2(floor(uv)))&mask;
    uint texel=xy.y*(mask.x+1u)+xy.x,base=offset/4u;
    if(indexed)texel=(materials.word[base+256u+texel/4u]>>((texel&3u)*8u))&255u;
    uint packed=materials.word[base+texel];
    // Native palette index0 can be opaque black. Fractional alpha is a hole.
    if((packed>>24u)!=255u) return 0u;
    if(coverage_only) return packed;
#if defined(STARFOX_NATIVE_MATERIAL_COLOUR)
    vec3 colour=vec3(uvec3(packed&255u,(packed>>8u)&255u,(packed>>16u)&255u))/255.0;
    if((flags&2u)!=0u)colour=calibrated_decode_srgb(colour);
    uvec4 style=uvec4(materials.word[at+8u],materials.word[at+9u],materials.word[at+10u],materials.word[at+11u]);
    colour=scene_styled_colour(vec4(colour,1.0),style).rgb;
    if((style.z&1u)!=0u)colour=calibrated_encode_srgb(colour);
    uvec3 bytes_rgb=uvec3(round(clamp(colour,0.0,1.0)*255.0));
    return bytes_rgb.x|(bytes_rgb.y<<8u)|(bytes_rgb.z<<16u)|0xff000000u;
#else
    return packed;
#endif
}
