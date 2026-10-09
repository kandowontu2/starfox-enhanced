// Enclose CURRENT raw finite barycentrics or environment direction in the
// accepted eye. Keep operation grouping identical across native consumers.
#ifndef STARFOX_REFLECTION_SOURCE_OPTICAL_TARGET
#define STARFOX_REFLECTION_SOURCE_OPTICAL_TARGET
cbuffer TargetSettings:register(b2,space2) {float4 targetCurrentCube[3],targetPreviousCube[3];};
Interval3 native_target(uint at,float4 feature) {
    if(feature.w!=0) {
        const Interval x=ip(feature.x),y=ip(feature.y),a=isub(ip(1),iadd(x,y));
        return vadd(vadd(vmul(p3(asfloat(frames.Load4(at+432)).xyz),a),
            vmul(p3(asfloat(frames.Load4(at+448)).xyz),x)),vmul(p3(asfloat(frames.Load4(at+464)).xyz),y));
    }
    const Interval3 raw=p3(feature.xyz);
    const Interval3 world=i3(vdot(p3(targetCurrentCube[0].xyz),raw),
        vdot(p3(targetCurrentCube[1].xyz),raw),vdot(p3(targetCurrentCube[2].xyz),raw));
    return vunit(i3(vdot(p3(float3(targetPreviousCube[0].x,targetPreviousCube[1].x,targetPreviousCube[2].x)),world),
        vdot(p3(float3(targetPreviousCube[0].y,targetPreviousCube[1].y,targetPreviousCube[2].y)),world),
        vdot(p3(float3(targetPreviousCube[0].z,targetPreviousCube[1].z,targetPreviousCube[2].z)),world)));
}
#endif
