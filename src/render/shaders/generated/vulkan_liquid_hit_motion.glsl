// Generated from reflection_liquid_hit_motion.hlsli; do not edit.
bool isfinite(float v) {return !isnan(v) && !isinf(v);}
bvec2 isfinite(vec2 v) {return bvec2(isfinite(v.x),isfinite(v.y));}
bvec3 isfinite(vec3 v) {return bvec3(isfinite(v.x),isfinite(v.y),isfinite(v.z));}
bvec4 isfinite(vec4 v) {return bvec4(isfinite(v.x),isfinite(v.y),isfinite(v.z),isfinite(v.w));}
// Invert the PREVIOUS liquid's optical path for one matched secondary feature.
// This is not a primary-plane motion proxy, nor permission to accept RGB.
// The history consumer must still verify the old receiver/secondary identity,
// coverage, depth and feature footprint. A failed solve returns an invalid guide.
#ifndef STARFOX_REFLECTION_LIQUID_HIT_MOTION
#define STARFOX_REFLECTION_LIQUID_HIT_MOTION
#include "vulkan_liquid_optics.glsl"
#ifndef STARFOX_LIQUID_SOLVE_TRACE
#define STARFOX_LIQUID_SOLVE_TRACE(iteration,slot,value)
#endif

struct ReflectionLiquidFrame {
    vec4 planePoint,planeNormal;
    // Same inverse-transpose convention as native_water_sample: row-vector
    // positions, column-vector normals; w stores the world-space offset.
    vec4 rotation0,rotation1,rotation2;
    vec4 projection; // fx,fy,cx,cy
    vec4 extentClip; // width,height,near,far (forward camera depths)
    vec4 settings; // accepted time, material (0 water / 3 lava), reserved
};
bool reflection_liquid_frame_valid(ReflectionLiquidFrame old) {
    float normalLength=dot(old.planeNormal.xyz,old.planeNormal.xyz);
    if(!all(isfinite(old.planePoint)) || !all(isfinite(old.planeNormal))
        || !all(isfinite(old.rotation0)) || !all(isfinite(old.rotation1)) || !all(isfinite(old.rotation2))
        || !all(isfinite(old.projection)) || !all(isfinite(old.extentClip)) || !all(isfinite(old.settings))
        || any(greaterThan(abs(old.planePoint.xyz),vec3(1.e12))) || !isfinite(normalLength) || normalLength<=1.e-20
        || any(greaterThan(abs(old.rotation0),vec4(1.e12))) || any(greaterThan(abs(old.rotation1),vec4(1.e12))) || any(greaterThan(abs(old.rotation2),vec4(1.e12)))
        || any(lessThanEqual(old.projection.xy,vec2(0))) || any(greaterThan(old.projection.xy,vec2(1.e12)))
        || any(lessThan(old.extentClip.xy,vec2(1))) || any(greaterThan(old.extentClip.xy,vec2(16384)))
        || old.extentClip.z<=0 || old.extentClip.w<=old.extentClip.z || old.extentClip.w>1.e12
        || old.settings.x<0 || old.settings.x>1.e12
        || (old.settings.y!=0 && old.settings.y!=3)) return false;
    float determinant=dot(old.rotation0.xyz,cross(old.rotation1.xyz,old.rotation2.xyz));
    return isfinite(determinant) && abs(determinant)>1.e-12;
}
bool reflection_liquid_ray(vec2 pixel,ReflectionLiquidFrame old,
    out vec3 hit,out vec3 normal,out float depth,out float bias) {
    hit=normal=vec3(0);depth=bias=0;
    if(!all(isfinite(pixel)) || any(lessThan(pixel,vec2(0))) || any(greaterThanEqual(pixel,old.extentClip.xy))) return false;
    vec3 direction=normalize(vec3((pixel-old.projection.zw)/old.projection.xy,1));
    float denominator=dot(direction,old.planeNormal.xyz);
    if(!isfinite(denominator) || abs(denominator)<=1.e-12) return false;
    float distance=dot(old.planePoint.xyz,old.planeNormal.xyz)/denominator;
    depth=distance*direction.z;
    if(!isfinite(distance) || distance<=0 || depth<old.extentClip.z || depth>old.extentClip.w) return false;
    hit=direction*distance;
    mat3 rotation=transpose(mat3(old.rotation0.xyz,old.rotation1.xyz,old.rotation2.xyz));
    float footprint=distance/max(old.projection.x,1.f)/max(abs(denominator),.04f);
    LiquidOpticalSample optical=liquid_optical_sample(hit,direction,distance,footprint,rotation,
        vec3(old.rotation0.w,old.rotation1.w,old.rotation2.w),old.settings.x,uint(old.settings.y));
    hit=optical.hit;normal=optical.normal;
    bias=max(.05,distance*1.e-5);
    // The witness records the analytic receiver depth, not the displaced lava
    // origin. Both values are kept distinct when solving the optical path.
    return all(isfinite(hit)) && all(isfinite(normal)) && isfinite(bias);
}
bool reflection_liquid_error(vec2 pixel,ReflectionLiquidFrame old,vec3 feature,
    out vec2 error,out float depth) {
    vec3 hit,normal;float bias;error=vec2(0);depth=0;
    if(!reflection_liquid_ray(pixel,old,hit,normal,depth,bias)) return false;
    vec3 toFeature=feature-(hit+normal*bias);
    float length2=dot(toFeature,toFeature);
    if(!isfinite(length2) || length2<=bias*bias) return false;
    // Reversing reflection recovers the incoming eye ray. Include the actual
    // ray bias; dropping it can move a near reflected feature by a whole pixel.
    vec3 incoming=reflect(toFeature*inversesqrt(length2),normal);
    if(!all(isfinite(incoming)) || incoming.z<=0) return false;
    error=old.projection.xy*incoming.xy/incoming.z+old.projection.zw-pixel;
    return all(isfinite(error));
}
// Transport the CURRENT primary receiver point into the accepted eye solely
// to initialize the old-wave inversion. Invert the real matrix, not a rigid
// transpose approximation (source transforms can contain quantized scale).
vec2 reflection_liquid_receiver_seed(ReflectionLiquidFrame old,vec3 currentHit,
    vec4 current0,vec4 current1,vec4 current2) {
    if(!reflection_liquid_frame_valid(old) || !all(isfinite(currentHit))
        || !all(isfinite(current0)) || !all(isfinite(current1)) || !all(isfinite(current2))) return vec2(-1,-1);
    vec3 world=(currentHit*transpose(mat3(current0.xyz,current1.xyz,current2.xyz)))
        +vec3(current0.w,current1.w,current2.w);
    vec3 delta=world-vec3(old.rotation0.w,old.rotation1.w,old.rotation2.w);
    vec3 a=cross(old.rotation1.xyz,old.rotation2.xyz),b=cross(old.rotation2.xyz,old.rotation0.xyz),c=cross(old.rotation0.xyz,old.rotation1.xyz);
    float determinant=dot(old.rotation0.xyz,a);
    vec3 previous=vec3(dot(delta,a),dot(delta,b),dot(delta,c))/determinant;
    if(!all(isfinite(previous)) || previous.z<=0) return vec2(-1,-1);
    vec2 pixel=old.projection.xy*previous.xy/previous.z+old.projection.zw;
    return all(isfinite(pixel))?pixel:vec2(-1,-1);
}
vec4 reflection_liquid_hit_motion(ReflectionLiquidFrame old,vec4 qa,vec4 qb,vec4 qc,vec2 bary,vec2 receiverSeed) {
    if(!reflection_liquid_frame_valid(old) || qa.w!=1 || qb.w!=1 || qc.w!=1
        || !all(isfinite(qa)) || !all(isfinite(qb)) || !all(isfinite(qc))
        || !all(isfinite(bary)) || any(lessThan(bary,vec2(0))) || bary.x+bary.y>1.00001) return vec4(0);
    vec3 triangleNormal=cross(qb.xyz-qa.xyz,qc.xyz-qa.xyz);
    float triangleLength=dot(triangleNormal,triangleNormal);
    if(!all(isfinite(triangleNormal)) || !isfinite(triangleLength) || triangleLength<=1.e-20) return vec4(0);
    vec3 feature=qa.xyz*(1-bary.x-bary.y)+qb.xyz*bary.x+qc.xyz*bary.y;
    vec3 n=normalize(old.planeNormal.xyz);
    vec3 virtualFeature=feature-2*n*dot(feature-old.planePoint.xyz,n);
    if(!all(isfinite(virtualFeature)) || virtualFeature.z<=0) return vec4(0);
    vec2 pixel=old.projection.xy*virtualFeature.xy/virtualFeature.z+old.projection.zw;
    // The current receiver is a local starting guess only. The accepted guide
    // still has to invert the actual OLD displaced surface and matched finite
    // feature; a primary-plane flow is never substituted for that solution.
    vec2 seedError=vec2(0);float seedDepth=0;
    bool seeded=all(isfinite(receiverSeed)) && reflection_liquid_error(receiverSeed,old,feature,seedError,seedDepth);
    if(seeded) pixel=receiverSeed;
    // A bounded deterministic solve; no random samples or per-pixel ray queries.
    // Line search never crosses invalid coverage/clip boundaries. If the old
    // wave folds or a root is not reachable locally, decline history outright.
    const float derivativeStep=.25;
    const float trust=max(8.f,min(old.extentClip.x,old.extentClip.y)*.0625f);
    for(uint iteration=0;iteration<12;++iteration) {
        // Reuse the just-evaluated starting ray: do not optical the complete
        // wave/bubble surface twice at the same pixel on every finite hit.
        vec2 error=seedError;float depth=seedDepth;
        bool valid=true;
        if(iteration!=0 || !seeded) valid=reflection_liquid_error(pixel,old,feature,error,depth);
        if(!valid) {
            STARFOX_LIQUID_SOLVE_TRACE(iteration,3,vec4(pixel,-1,0));return vec4(0);
        }
        STARFOX_LIQUID_SOLVE_TRACE(iteration,0,vec4(pixel,error));
        float residual=max(abs(error.x),abs(error.y));
        if(residual<=.002) {
            STARFOX_LIQUID_SOLVE_TRACE(iteration,3,vec4(pixel,1,residual));return vec4(pixel,depth,1);
        }
        vec2 ex,ey;float unused;
        if(!reflection_liquid_error(pixel+vec2(derivativeStep,0),old,feature,ex,unused)
            || !reflection_liquid_error(pixel+vec2(0,derivativeStep),old,feature,ey,unused)) return vec4(0);
        vec2 jx=(ex-error)/derivativeStep,jy=(ey-error)/derivativeStep;
        STARFOX_LIQUID_SOLVE_TRACE(iteration,1,vec4(jx,jy));
        float determinant=jx.x*jy.y-jy.x*jx.y;
        if(!isfinite(determinant) || abs(determinant)<1.e-5) return vec4(0);
        vec2 step=vec2(jy.y*error.x-jy.x*error.y,-jx.y*error.x+jx.x*error.y)/determinant;
        if(!all(isfinite(step))) return vec4(0);
        step*=min(1.f,trust/max(max(abs(step.x),abs(step.y)),1.e-12));
        STARFOX_LIQUID_SOLVE_TRACE(iteration,2,vec4(step,residual,determinant));
        bool advanced=false;
        for(uint backtrack=0;backtrack<5;++backtrack) {
            vec2 next=pixel-step,trial;float trialDepth;
            if(reflection_liquid_error(next,old,feature,trial,trialDepth)
                && max(abs(trial.x),abs(trial.y))<residual) {pixel=next;advanced=true;break;}
            step*=.5;
        }
        STARFOX_LIQUID_SOLVE_TRACE(iteration,3,vec4(pixel,advanced?2:-2,0));
        if(!advanced) return vec4(0);
    }
    vec2 error;float depth;
    if(reflection_liquid_error(pixel,old,feature,error,depth) && max(abs(error.x),abs(error.y))<=.002)
        return vec4(pixel,depth,1);
    return vec4(0);
}
vec4 reflection_liquid_hit_motion(ReflectionLiquidFrame old,vec4 qa,vec4 qb,vec4 qc,vec2 bary) {
    return reflection_liquid_hit_motion(old,qa,qb,qc,bary,vec2(-1,-1));
}
#endif
