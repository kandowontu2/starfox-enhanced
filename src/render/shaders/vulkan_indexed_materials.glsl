// Shared indexed coverage for native Vulkan reflection and shadow candidates.
// RayMaterial and the packed-byte texels SSBO are declared by each producer.
// 256 is the rejected/cutout sentinel, never a valid source palette index.
uint indexed_source_index(RayMaterial m,vec2 barycentric,uvec2 pixel,uint texel_count) {
    if(m.reserved!=0u || m.textured>1u || m.colour_base>255u) return 256u;
    uint index=m.dither!=0u && ((pixel.x+pixel.y)&1u)!=0u?m.odd_colour:m.even_colour;
    if(m.textured!=0u) {
        if(m.u_mask>4095u || m.v_mask>4095u || (m.u_mask&(m.u_mask+1u))!=0u
            || (m.v_mask&(m.v_mask+1u))!=0u || m.offset>texel_count
            || (m.u_mask+1u)*(m.v_mask+1u)>texel_count-m.offset) return 256u;
        vec2 uv=m.uv0*(1.0-barycentric.x-barycentric.y)+m.uv1*barycentric.x+m.uv2*barycentric.y;
        if(any(isnan(uv)) || any(isinf(uv)) || any(greaterThan(abs(uv),vec2(1e8)))) return 256u;
        uvec2 tile=uvec2(ivec2(floor(uv)))&uvec2(m.u_mask,m.v_mask);
        uint at=m.offset+tile.y*(m.u_mask+1u)+tile.x;
        uint texel=(texels.word[at>>2u]>>((at&3u)*8u))&255u;
        // MDSPRITE/source texel zero is a hole BEFORE palette-base wrapping.
        // Opaque texture or solid inks selecting palette zero stay opaque.
        if(texel==0u) return 256u;
        index=(texel+m.colour_base)&255u;
    }
    return index<=255u?index:256u;
}
