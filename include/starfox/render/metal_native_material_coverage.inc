// Same resident64-byte kind2/kind3 coverage contract as Vulkan and DXR.
// This decoder is coverage only: it must not apply colour styles or sRGB twice.
// binding_words is the actual bound allocation, byte_count the declared payload.
uint starfox_native_material_coverage(uint primitive,float bary_x,float bary_y,
    uint pixel_x,uint pixel_y,device const uint* words,uint count,uint byte_count,
    uint binding_words) {
    if(primitive>=count || count>0x03ffffffu || count*16u>binding_words
        || (byte_count&3u)!=0u || byte_count<count*64u
        || byte_count/4u>binding_words || !isfinite(bary_x) || !isfinite(bary_y)) return 0u;
    uint at=primitive*16u,kind=words[at+15u];
    if(kind==2u) {
        uint scale=words[at+7u];
        if(words[at+6u]!=0u || scale>4096u) return 0u;
        uint divisor=scale==0u?1u:scale;
        bool odd=scale!=0u && (((pixel_x/divisor)^(pixel_y/divisor))&1u)!=0u;
        uint packed=words[at+(odd?9u:8u)];
        return (packed>>24u)==255u?packed:0u;
    }
    if(kind!=3u || words[at+6u]!=1u) return 0u;
    uint flags=words[at+7u],offset=words[at+12u];
    uint mask_x=words[at+13u],mask_y=words[at+14u];
    bool indexed=(flags&536870912u)!=0u;
    if((flags&~(7u|536870912u))!=0u || (flags&1u)==0u
        || mask_x>4095u || mask_y>4095u || (mask_x&(mask_x+1u))!=0u
        || (mask_y&(mask_y+1u))!=0u || (offset&3u)!=0u
        || offset<count*64u || offset>byte_count) return 0u;
    uint pixels=(mask_x+1u)*(mask_y+1u);
    uint atlas_words=indexed?256u+(pixels+3u)/4u:pixels;
    if(atlas_words>(byte_count-offset)/4u) return 0u;
    float ax=as_type<float>(words[at]),ay=as_type<float>(words[at+1u]);
    float bx=as_type<float>(words[at+2u]),by=as_type<float>(words[at+3u]);
    float cx=as_type<float>(words[at+4u]),cy=as_type<float>(words[at+5u]);
    if(!isfinite(ax) || !isfinite(ay) || !isfinite(bx) || !isfinite(by)
        || !isfinite(cx) || !isfinite(cy) || abs(ax)>65536.0f || abs(ay)>65536.0f
        || abs(bx)>65536.0f || abs(by)>65536.0f || abs(cx)>65536.0f || abs(cy)>65536.0f) return 0u;
    float u=ax*(1.0f-bary_x-bary_y)+bx*bary_x+cx*bary_y;
    float v=ay*(1.0f-bary_x-bary_y)+by*bary_x+cy*bary_y;
    if(!isfinite(u) || !isfinite(v) || abs(u)>65536.0f || abs(v)>65536.0f) return 0u;
    if((flags&4u)!=0u && (u<0.0f || v<0.0f || u>=float(mask_x+1u) || v>=float(mask_y+1u))) return 0u;
    uint x=uint(int(floor(u)))&mask_x,y=uint(int(floor(v)))&mask_y;
    uint texel=y*(mask_x+1u)+x,base=offset/4u;
    if(indexed) texel=(words[base+256u+texel/4u]>>((texel&3u)*8u))&255u;
    uint packed=words[base+texel];
    // Native index0 may be opaque black. Only fully opaque alpha casts a ray.
    return (packed>>24u)==255u?packed:0u;
}
