// Native secondary witnesses are produced by RT first. Solve only their old
// wave paths in a separate bounded compute pass: no RayQuery state/registers,
// scene traversal, readback, submission or queue wait belongs in this helper.
#include "reflection_liquid_hit_motion.hlsli"
ByteAddressBuffer coverage : register(t1);
ByteAddressBuffer triangleVertices : register(t2);
RWByteAddressBuffer outputMask : register(u0);
cbuffer Settings : register(b0) {
    float4 camera,options,groundPoint,groundNormal,lights[16],primaryRange;
};
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint width=uint(camera.x),height=uint(camera.y);
    if(id.x>=width || id.y>=height || (coverage.Load(12)&8192U)==0) return;
    uint data=coverage.Load(4),count=width*height,index=id.y*width+id.x;
    uint offset=coverage.Load(data+1172);
    uint4 identity=outputMask.Load4(offset+count*16+index*16);
    if(identity.x!=0xfffffffeU || identity.y==0xffffffffU || identity.w==0xffffffffU) return;
    float4 witness=asfloat(outputMask.Load4(offset+count*32+index*16));
    if(witness.w!=1) return;
    uint hit=coverage.Load(data+1168)+identity.y*48;
    ReflectionLiquidFrame old;
    old.planePoint=asfloat(coverage.Load4(data+1216));old.planeNormal=asfloat(coverage.Load4(data+1232));
    old.rotation0=asfloat(coverage.Load4(data+1248));old.rotation1=asfloat(coverage.Load4(data+1264));
    old.rotation2=asfloat(coverage.Load4(data+1280));old.projection=asfloat(coverage.Load4(data+1296));
    old.extentClip=asfloat(coverage.Load4(data+1312));old.settings=asfloat(coverage.Load4(data+1328));
    // Current native receiver Z is analytic, distinct from displaced lava Z.
    // This pose-transported point is only a seed; the resulting motion still
    // solves the actual accepted optical path to the matched old triangle.
    float3 currentHit=float3(float((double(id.x)+.5-double(camera.w))/double(camera.z))*witness.z,
        float((double(id.y)+.5-double(options.x))/double(options.z))*witness.z,witness.z);
    float2 seed=reflection_liquid_receiver_seed(old,currentHit,asfloat(coverage.Load4(data+1104)),
        asfloat(coverage.Load4(data+1120)),asfloat(coverage.Load4(data+1136)));
    float4 motion=reflection_liquid_hit_motion(old,asfloat(triangleVertices.Load4(hit)),
        asfloat(triangleVertices.Load4(hit+16)),asfloat(triangleVertices.Load4(hit+32)),witness.xy,seed);
    // Current RGB/world/surface/incident/base/response and all identities stay
    // untouched. The first producer already zeroed missing/rejected motion.
    outputMask.Store4(offset+index*16,asuint(motion));
}
