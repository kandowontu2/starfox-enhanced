#if TEST_SAMPLERS
Texture2D<float4> tex0 : register(t0, space0);
Texture2D<float4> tex1 : register(t1, space0);
SamplerState sampler0 : register(s0, space0);
SamplerState sampler1 : register(s1, space0);
ByteAddressBuffer input0 : register(t2, space0);
ByteAddressBuffer input1 : register(t3, space0);
#else
ByteAddressBuffer input0 : register(t0, space0);
ByteAddressBuffer input1 : register(t1, space0);
#endif
RWByteAddressBuffer output : register(u0, space1);
cbuffer Constants : register(b0, space2) {uint index; uint3 padding;};
[numthreads(1,1,1)]
void main()
{
    uint result = input0.Load(0) ^ input1.Load(0) ^ index;
#if TEST_SAMPLERS
    result ^= uint(round(tex0.SampleLevel(sampler0, float2(.5,.5), 0).r * 255));
    result ^= uint(round(tex1.SampleLevel(sampler1, float2(.5,.5), 0).r * 255)) << 8;
#endif
    output.Store(index * 4, result);
}
