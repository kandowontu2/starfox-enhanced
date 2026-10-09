#ifndef STARFOX_REFLECTION_PATH_RECORD
#define STARFOX_REFLECTION_PATH_RECORD
// Opt-in native producer witness. Not visibility acceptance or reusable colour.
// No old material response or pre-mirror sample belongs to incident radiance.
// Invalid records are fully initialized; kind 2 and 3 are deliberately distinct
// (real environment escape versus bounded specular budget exhaustion).
struct ReflectionPathRecord {
    uint4 mirrors;
    uint control; // low 3 bits count; kind at bit8. Curved ABI: liquid mask bits4..7, kind4=absorbed.
    uint terminal;
    float3 feature; // finite barycentric XY/zero, or terminal eye-view direction XYZ.
    uint incoming;
    float3 response; // CURRENT per-hop Fresnel, never filtered/transported.
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    float3 base; // CURRENT accumulated emission/transmission/direct light per lobe.
#endif
};
ReflectionPathRecord reflection_path_empty() {
    ReflectionPathRecord r;
    r.mirrors=0xffffffffU;r.control=0;r.terminal=0xffffffffU;
    r.feature=0;r.incoming=0;r.response=0;
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    r.base=0;
#endif
    return r;
}
void reflection_path_terminal(inout ReflectionPathRecord r,uint kind,uint count,
    uint terminal,float3 feature,uint incoming,float3 response) {
    r.control=(kind<<8)|count
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
        |(r.control&0xf0U)
#endif
        ;r.terminal=terminal;r.feature=feature;
    r.incoming=incoming;r.response=response;
}
void reflection_path_store(RWByteAddressBuffer output,uint at,ReflectionPathRecord r) {
    output.Store4(at,r.mirrors);
    output.Store2(at+16,uint2(r.control,r.terminal));
    output.Store3(at+24,asuint(r.feature));output.Store(at+36,r.incoming);
    output.Store3(at+40,asuint(r.response));
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    output.Store3(at+52,asuint(r.base));
#endif
}
ReflectionPathRecord reflection_path_load(ByteAddressBuffer source,uint at) {
    ReflectionPathRecord r;r.mirrors=source.Load4(at);uint2 key=source.Load2(at+16);
    r.control=key.x;r.terminal=key.y;r.feature=asfloat(source.Load3(at+24));
    r.incoming=source.Load(at+36);r.response=asfloat(source.Load3(at+40));
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    r.base=asfloat(source.Load3(at+52));
#endif
    return r;
}
#endif
