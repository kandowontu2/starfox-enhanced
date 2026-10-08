#include "calibrated_colour.hlsli"
Texture2D<float4> faceColour : register(t0,space0);
SamplerState faceSampler : register(s0,space0);
RWByteAddressBuffer rayData : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint size,offset,face,srgb;};
[numthreads(8,8,1)]
void environment_pack_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=size || id.y>=size) return;
    float4 colour=faceColour.Load(int3(id.xy,0));
    if(srgb!=0) colour.rgb=calibrated_encode_srgb(colour.rgb);
    uint4 bytes=uint4(round(saturate(colour)*255.));
    rayData.Store(offset+(face*size*size+id.y*size+id.x)*4,
        bytes.x|(bytes.y<<8)|(bytes.z<<16)|(bytes.w<<24));
}
