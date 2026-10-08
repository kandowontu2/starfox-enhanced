// RasterCommand is a 96-byte ABI; its first uint4 is the complete validity
// marker. Do not touch live payload or the source-texel prefix in mask storage.
[[vk::binding(0,1)]] RWByteAddressBuffer commands : register(u0,space1);
[[vk::binding(1,1)]] RWByteAddressBuffer masks : register(u1,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint rowCount, maskWordCount, maskOffset, dispatchRowStride;
};
[numthreads(128,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint index=id.x+id.y*dispatchRowStride;
    if(index<rowCount) commands.Store4(index*96U,uint4(0,0,0,0));
    if(index<maskWordCount) masks.Store(maskOffset+index*4U,0);
}
