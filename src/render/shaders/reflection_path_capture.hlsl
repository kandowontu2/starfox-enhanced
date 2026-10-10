// Exact CURRENT-only 52/64-byte path capture, not an optical solve or colour reuse.
// Resident source validation must NEVER index a previously blended CURRENT bank.
// A cold/cut curved bank or zero weight also needs no inverse/colour PSO.
#include "calibrated_ray_history.hlsli"
Texture2D<float4> ownership:register(t0,space0);SamplerState ownerSampler:register(s0,space0);
ByteAddressBuffer current:register(t1,space0);
RWByteAddressBuffer captured:register(u0,space1);
cbuffer Settings:register(b0,space2) {
    uint width,height,lobes,primaryPrefix;
    uint recordPrefix,triangles,curvedReceivers,liquidMaterial;
    uint pathStride,scenePaths,reserved0,reserved1;
};
[numthreads(8,8,1)]
void reflection_path_capture_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height)return;
    const uint count=width*height,p=id.y*width+id.x,word=current.Load(p*4);
    const uint primary=current.Load(count*primaryPrefix+p*4);
    const float depth=asfloat(current.Load(count*(primaryPrefix+4)+p*4));
    const float4 response=asfloat(current.Load4(count*(primaryPrefix+8)+p*16));
    const bool hasBase=curvedReceivers!=0 || scenePaths!=0;
    const float4 base=hasBase?asfloat(current.Load4(count*(primaryPrefix+24)+p*16)):float4(0,0,0,1);
    const float4 owner=ownership.Load(int3(id.xy,0));
    const bool floor=curvedReceivers!=0?primary==0xfffffffdU:scenePaths!=0 && primary==0xfffffffeU;
    const bool eligible=owner.a>.5 && calibrated_secondary_radiance(word,owner,true)
        && (floor?(word>>24)==(curvedReceivers!=0 && liquidMaterial==0?253U:254U):primary<triangles && (word>>24)==255U)
        && isfinite(depth) && depth>0 && response.w==1
        && (!hasBase || (base.w==1 && all(isfinite(base)) && all(base.rgb>=0) && all(base.rgb<=1.e12)))
        && all(isfinite(response)) && all(response.rgb>=0) && all(response.rgb<=1) && any(response.rgb>0);
    captured.Store(p*4,word);
    if(primaryPrefix==24)captured.Store(count*4+p*4,current.Load(count*4+p*4));
    if(primaryPrefix==20 || primaryPrefix==24) {
        const uint surface=count*(primaryPrefix-16)+p*16;captured.Store4(surface,current.Load4(surface));
    }
    captured.Store(count*primaryPrefix+p*4,eligible?primary:0xffffffffU);
    captured.Store(count*(primaryPrefix+4)+p*4,asuint(depth));
    captured.Store4(count*(primaryPrefix+8)+p*16,asuint(response));
    if(hasBase)captured.Store4(count*(primaryPrefix+24)+p*16,asuint(base));
    [loop] for(uint l=0;l<lobes;++l) {
        const uint at=count*recordPrefix+(p*lobes+l)*pathStride;
        [unroll] for(uint n=0;n<3;++n)captured.Store4(at+n*16,current.Load4(at+n*16));
        if(pathStride==64)captured.Store4(at+48,current.Load4(at+48));
        else captured.Store(at+48,current.Load(at+48));
    }
}
