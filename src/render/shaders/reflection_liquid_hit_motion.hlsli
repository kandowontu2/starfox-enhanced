// Invert the PREVIOUS liquid's optical path for one matched secondary feature.
// This is not a primary-plane motion proxy, nor permission to accept RGB.
// The history consumer must still verify the old receiver/secondary identity,
// coverage, depth and feature footprint. A failed solve returns an invalid guide.
#ifndef STARFOX_REFLECTION_LIQUID_HIT_MOTION
#define STARFOX_REFLECTION_LIQUID_HIT_MOTION
#include "liquid_optics.hlsli"
#ifndef STARFOX_LIQUID_SOLVE_TRACE
#define STARFOX_LIQUID_SOLVE_TRACE(iteration,slot,value)
#endif

struct ReflectionLiquidFrame {
    float4 planePoint,planeNormal;
    // Same inverse-transpose convention as native_water_sample: row-vector
    // positions, column-vector normals; w stores the world-space offset.
    float4 rotation0,rotation1,rotation2;
    float4 projection; // fx,fy,cx,cy
    float4 extentClip; // width,height,near,far (forward camera depths)
    float4 settings; // accepted time, material (0 water / 3 lava), reserved
};
bool reflection_liquid_frame_valid(ReflectionLiquidFrame old) {
    float normalLength=dot(old.planeNormal.xyz,old.planeNormal.xyz);
    if(!all(isfinite(old.planePoint)) || !all(isfinite(old.planeNormal))
        || !all(isfinite(old.rotation0)) || !all(isfinite(old.rotation1)) || !all(isfinite(old.rotation2))
        || !all(isfinite(old.projection)) || !all(isfinite(old.extentClip)) || !all(isfinite(old.settings))
        || any(abs(old.planePoint.xyz)>1.e12) || !isfinite(normalLength) || normalLength<=1.e-20
        || any(abs(old.rotation0)>1.e12) || any(abs(old.rotation1)>1.e12) || any(abs(old.rotation2)>1.e12)
        || any(old.projection.xy<=0) || any(old.projection.xy>1.e12)
        || any(old.extentClip.xy<1) || any(old.extentClip.xy>16384)
        || old.extentClip.z<=0 || old.extentClip.w<=old.extentClip.z || old.extentClip.w>1.e12
        || old.settings.x<0 || old.settings.x>1.e12
        || (old.settings.y!=0 && old.settings.y!=3)) return false;
    float determinant=dot(old.rotation0.xyz,cross(old.rotation1.xyz,old.rotation2.xyz));
    return isfinite(determinant) && abs(determinant)>1.e-12;
}
bool reflection_liquid_ray(float2 pixel,ReflectionLiquidFrame old,
    out float3 hit,out float3 normal,out float depth,out float bias) {
    hit=normal=0;depth=bias=0;
    if(!all(isfinite(pixel)) || any(pixel<0) || any(pixel>=old.extentClip.xy)) return false;
    float3 direction=normalize(float3((pixel-old.projection.zw)/old.projection.xy,1));
    float denominator=dot(direction,old.planeNormal.xyz);
    if(!isfinite(denominator) || abs(denominator)<=1.e-12) return false;
    float distance=dot(old.planePoint.xyz,old.planeNormal.xyz)/denominator;
    depth=distance*direction.z;
    if(!isfinite(distance) || distance<=0 || depth<old.extentClip.z || depth>old.extentClip.w) return false;
    hit=direction*distance;
    float3x3 rotation=float3x3(old.rotation0.xyz,old.rotation1.xyz,old.rotation2.xyz);
    float footprint=distance/max(old.projection.x,1.f)/max(abs(denominator),.04f);
    LiquidOpticalSample sample=liquid_optical_sample(hit,direction,distance,footprint,rotation,
        float3(old.rotation0.w,old.rotation1.w,old.rotation2.w),old.settings.x,uint(old.settings.y));
    hit=sample.hit;normal=sample.normal;
    bias=max(.05,distance*1.e-5);
    // The witness records the analytic receiver depth, not the displaced lava
    // origin. Both values are kept distinct when solving the optical path.
    return all(isfinite(hit)) && all(isfinite(normal)) && isfinite(bias);
}
bool reflection_liquid_error(float2 pixel,ReflectionLiquidFrame old,float3 feature,
    out float2 error,out float depth) {
    float3 hit,normal;float bias;error=0;depth=0;
    if(!reflection_liquid_ray(pixel,old,hit,normal,depth,bias)) return false;
    float3 toFeature=feature-(hit+normal*bias);
    float length2=dot(toFeature,toFeature);
    if(!isfinite(length2) || length2<=bias*bias) return false;
    // Reversing reflection recovers the incoming eye ray. Include the actual
    // ray bias; dropping it can move a near reflected feature by a whole pixel.
    float3 incoming=reflect(toFeature*rsqrt(length2),normal);
    if(!all(isfinite(incoming)) || incoming.z<=0) return false;
    error=old.projection.xy*incoming.xy/incoming.z+old.projection.zw-pixel;
    return all(isfinite(error));
}
// Transport the CURRENT primary receiver point into the accepted eye solely
// to initialize the old-wave inversion. Invert the real matrix, not a rigid
// transpose approximation (source transforms can contain quantized scale).
float2 reflection_liquid_receiver_seed(ReflectionLiquidFrame old,float3 currentHit,
    float4 current0,float4 current1,float4 current2) {
    if(!reflection_liquid_frame_valid(old) || !all(isfinite(currentHit))
        || !all(isfinite(current0)) || !all(isfinite(current1)) || !all(isfinite(current2))) return float2(-1,-1);
    float3 world=mul(currentHit,float3x3(current0.xyz,current1.xyz,current2.xyz))
        +float3(current0.w,current1.w,current2.w);
    float3 delta=world-float3(old.rotation0.w,old.rotation1.w,old.rotation2.w);
    float3 a=cross(old.rotation1.xyz,old.rotation2.xyz),b=cross(old.rotation2.xyz,old.rotation0.xyz),c=cross(old.rotation0.xyz,old.rotation1.xyz);
    float determinant=dot(old.rotation0.xyz,a);
    float3 previous=float3(dot(delta,a),dot(delta,b),dot(delta,c))/determinant;
    if(!all(isfinite(previous)) || previous.z<=0) return float2(-1,-1);
    float2 pixel=old.projection.xy*previous.xy/previous.z+old.projection.zw;
    return all(isfinite(pixel))?pixel:float2(-1,-1);
}
float4 reflection_liquid_hit_motion(ReflectionLiquidFrame old,float4 qa,float4 qb,float4 qc,float2 bary,float2 receiverSeed) {
    if(!reflection_liquid_frame_valid(old) || qa.w!=1 || qb.w!=1 || qc.w!=1
        || !all(isfinite(qa)) || !all(isfinite(qb)) || !all(isfinite(qc))
        || !all(isfinite(bary)) || any(bary<0) || bary.x+bary.y>1.00001) return 0;
    float3 triangleNormal=cross(qb.xyz-qa.xyz,qc.xyz-qa.xyz);
    float triangleLength=dot(triangleNormal,triangleNormal);
    if(!all(isfinite(triangleNormal)) || !isfinite(triangleLength) || triangleLength<=1.e-20) return 0;
    float3 feature=qa.xyz*(1-bary.x-bary.y)+qb.xyz*bary.x+qc.xyz*bary.y;
    float3 n=normalize(old.planeNormal.xyz);
    float3 virtualFeature=feature-2*n*dot(feature-old.planePoint.xyz,n);
    if(!all(isfinite(virtualFeature)) || virtualFeature.z<=0) return 0;
    float2 pixel=old.projection.xy*virtualFeature.xy/virtualFeature.z+old.projection.zw;
    // The current receiver is a local starting guess only. The accepted guide
    // still has to invert the actual OLD displaced surface and matched finite
    // feature; a primary-plane flow is never substituted for that solution.
    float2 seedError=0;float seedDepth=0;
    bool seeded=all(isfinite(receiverSeed)) && reflection_liquid_error(receiverSeed,old,feature,seedError,seedDepth);
    if(seeded) pixel=receiverSeed;
    // A bounded deterministic solve; no random samples or per-pixel ray queries.
    // Line search never crosses invalid coverage/clip boundaries. If the old
    // wave folds or a root is not reachable locally, decline history outright.
    const float derivativeStep=.25;
    const float trust=max(8.f,min(old.extentClip.x,old.extentClip.y)*.0625f);
    [loop] for(uint iteration=0;iteration<12;++iteration) {
        // Reuse the just-evaluated starting ray: do not sample the complete
        // wave/bubble surface twice at the same pixel on every finite hit.
        float2 error=seedError;float depth=seedDepth;
        bool valid=true;
        [branch] if(iteration!=0 || !seeded) valid=reflection_liquid_error(pixel,old,feature,error,depth);
        if(!valid) {
            STARFOX_LIQUID_SOLVE_TRACE(iteration,3,float4(pixel,-1,0));return 0;
        }
        STARFOX_LIQUID_SOLVE_TRACE(iteration,0,float4(pixel,error));
        float residual=max(abs(error.x),abs(error.y));
        if(residual<=.002) {
            STARFOX_LIQUID_SOLVE_TRACE(iteration,3,float4(pixel,1,residual));return float4(pixel,depth,1);
        }
        float2 ex,ey;float unused;
        if(!reflection_liquid_error(pixel+float2(derivativeStep,0),old,feature,ex,unused)
            || !reflection_liquid_error(pixel+float2(0,derivativeStep),old,feature,ey,unused)) return 0;
        float2 jx=(ex-error)/derivativeStep,jy=(ey-error)/derivativeStep;
        STARFOX_LIQUID_SOLVE_TRACE(iteration,1,float4(jx,jy));
        float determinant=jx.x*jy.y-jy.x*jx.y;
        if(!isfinite(determinant) || abs(determinant)<1.e-5) return 0;
        float2 step=float2(jy.y*error.x-jy.x*error.y,-jx.y*error.x+jx.x*error.y)/determinant;
        if(!all(isfinite(step))) return 0;
        step*=min(1.f,trust/max(max(abs(step.x),abs(step.y)),1.e-12));
        STARFOX_LIQUID_SOLVE_TRACE(iteration,2,float4(step,residual,determinant));
        bool advanced=false;
        [loop] for(uint backtrack=0;backtrack<5;++backtrack) {
            float2 next=pixel-step,trial;float trialDepth;
            if(reflection_liquid_error(next,old,feature,trial,trialDepth)
                && max(abs(trial.x),abs(trial.y))<residual) {pixel=next;advanced=true;break;}
            step*=.5;
        }
        STARFOX_LIQUID_SOLVE_TRACE(iteration,3,float4(pixel,advanced?2:-2,0));
        if(!advanced) return 0;
    }
    float2 error;float depth;
    if(reflection_liquid_error(pixel,old,feature,error,depth) && max(abs(error.x),abs(error.y))<=.002)
        return float4(pixel,depth,1);
    return 0;
}
float4 reflection_liquid_hit_motion(ReflectionLiquidFrame old,float4 qa,float4 qb,float4 qc,float2 bary) {
    return reflection_liquid_hit_motion(old,qa,qb,qc,bary,float2(-1,-1));
}
#endif
