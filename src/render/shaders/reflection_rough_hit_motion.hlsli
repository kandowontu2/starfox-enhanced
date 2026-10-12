// One fixed-quadrature ROUGH ray, not the combined eight-ray radiance.
// Invert an accepted old finite receiver and old secondary feature. A valid
// guide still needs per-lobe old visibility/identity/depth/colour validation;
// it is not permission to blend a whole material, Fresnel response or path.
#ifndef STARFOX_REFLECTION_ROUGH_HIT_MOTION
#define STARFOX_REFLECTION_ROUGH_HIT_MOTION
struct ReflectionRoughFrame {
    float4 a,b,c;
    float4 projection; // fx,fy,cx,cy in accepted eye pixels
    float4 extentClip; // width,height,forward near/far
    float4 settings; // accepted roughness, quadrature index 0..7, reserved
};
// W=2 is an explicitly analytic plane: A=point, B=normal, C=zero.
// It is NOT a giant triangle standing in for a curved or finite receiver.
bool reflection_rough_analytic(ReflectionRoughFrame old) {
    return old.a.w==2 && old.b.w==2 && old.c.w==2;
}
float3 reflection_rough_normal(ReflectionRoughFrame old) {
    return normalize(reflection_rough_analytic(old)?old.b.xyz:cross(old.b.xyz-old.a.xyz,old.c.xyz-old.a.xyz));
}
float3 reflection_rough_direction(float3 reflected,float3 normal,float roughness,uint sample) {
    // Exact existing native quadrature, including its tangent-reference branch
    // and below-surface fallback. No random/frame noise or new roughness curve.
    float3 tangent=normalize(cross(reflected,abs(reflected.y)<.95?float3(0,1,0):float3(1,0,0)));
    float3 bitangent=cross(reflected,tangent);
    const float2 taps[8]={float2(.5,0),float2(-.5,0),float2(0,.5),float2(0,-.5),
        float2(.612,.612),float2(-.612,.612),float2(.612,-.612),float2(-.612,-.612)};
    float3 direction=normalize(reflected+roughness*roughness*(tangent*taps[sample].x+bitangent*taps[sample].y));
    return dot(direction,normal)<=0?reflected:direction;
}
bool reflection_rough_frame_valid(ReflectionRoughFrame old) {
    bool analytic=reflection_rough_analytic(old);
    if((!analytic && (old.a.w!=1 || old.b.w!=1 || old.c.w!=1)) || !all(isfinite(old.a)) || !all(isfinite(old.b))
        || !all(isfinite(old.c)) || !all(isfinite(old.projection)) || !all(isfinite(old.extentClip))
        || !all(isfinite(old.settings)) || any(abs(old.a.xyz)>1.e12) || any(abs(old.b.xyz)>1.e12)
        || any(abs(old.c.xyz)>1.e12) || any(old.projection.xy<=0) || any(abs(old.projection)>1.e12)
        || any(old.extentClip.xy<1) || any(old.extentClip.xy>16384) || old.extentClip.z<=0
        || old.extentClip.w<=old.extentClip.z || old.extentClip.w>1.e12
        || old.settings.x<0 || old.settings.x>1 || old.settings.y<0 || old.settings.y>7
        || floor(old.settings.y)!=old.settings.y || any(old.settings.zw!=0)) return false;
    if(analytic && any(old.c.xyz!=0)) return false;
    float3 normal=analytic?old.b.xyz:cross(old.b.xyz-old.a.xyz,old.c.xyz-old.a.xyz);
    float length2=dot(normal,normal);
    return all(isfinite(normal)) && isfinite(length2) && length2>1.e-20;
}
bool reflection_rough_ray(float2 pixel,ReflectionRoughFrame old,out float3 hit,out float3 outgoing,
    out float depth,out float bias) {
    hit=outgoing=0;depth=bias=0;
    if(!all(isfinite(pixel)) || any(pixel<0) || any(pixel>=old.extentClip.xy)) return false;
    float3 normal=reflection_rough_normal(old);
    float3 incident=normalize(float3((pixel-old.projection.zw)/old.projection.xy,1));
    if(dot(normal,incident)>0) normal=-normal;
    float denominator=dot(incident,normal);
    if(!isfinite(denominator) || abs(denominator)<=1.e-12) return false;
    float distance=dot(old.a.xyz,normal)/denominator;depth=distance*incident.z;
    if(!isfinite(distance) || distance<=0 || depth<old.extentClip.z || depth>old.extentClip.w) return false;
    bias=max(reflection_rough_analytic(old)?.05:.01,distance*1.e-5);
    // Native model reflection offsets the origin by the oriented face normal.
    // The guide's Z remains the primary receiver, not this biased origin.
    hit=incident*distance+normal*bias;
    outgoing=reflection_rough_direction(reflect(incident,normal),normal,old.settings.x,uint(old.settings.y));
    return all(isfinite(hit)) && all(isfinite(outgoing));
}
bool reflection_rough_error(float2 pixel,ReflectionRoughFrame old,float3 feature,out float2 error,out float depth) {
    float3 origin,outgoing;float bias;error=0;depth=0;
    if(!reflection_rough_ray(pixel,old,origin,outgoing,depth,bias)) return false;
    float3 travel=feature-origin;float length2=dot(travel,travel);
    if(!isfinite(length2) || length2<=bias*bias) return false;
    travel*=rsqrt(length2);
    if(dot(travel,outgoing)<=0) return false; // Never accept the reverse ray.
    float3 tangent=normalize(cross(outgoing,abs(outgoing.y)<.95?float3(0,1,0):float3(1,0,0)));
    float3 bitangent=cross(outgoing,tangent);
    error=float2(dot(travel,tangent),dot(travel,bitangent))*max(old.projection.x,old.projection.y);
    return all(isfinite(error));
}
bool reflection_rough_finite_receiver(ReflectionRoughFrame old,float2 pixel,float depth) {
    if(reflection_rough_analytic(old)) return true;
    float3 receiver=float3((pixel-old.projection.zw)/old.projection.xy,1)*depth;
    float3 ab=old.b.xyz-old.a.xyz,ac=old.c.xyz-old.a.xyz,relative=receiver-old.a.xyz;
    float aa=dot(ab,ab),bb=dot(ab,ac),cc=dot(ac,ac),ra=dot(relative,ab),rc=dot(relative,ac);
    float determinant=aa*cc-bb*bb;
    if(!isfinite(determinant) || determinant<=1.e-20) return false;
    float2 bary=float2(cc*ra-bb*rc,aa*rc-bb*ra)/determinant;
    return all(isfinite(bary)) && all(bary>=-.00001) && bary.x+bary.y<=1.00001;
}
float4 reflection_rough_result(ReflectionRoughFrame old,float3 feature,float2 pixel,float depth) {
    if(!reflection_rough_finite_receiver(old,pixel,depth)) return 0;
    float3 origin,outgoing;float receiverDepth,bias;
    if(!reflection_rough_ray(pixel,old,origin,outgoing,receiverDepth,bias)) return 0;
    float3 magnitude=max(abs(origin),abs(feature));
    float scale=max(1.f,max(magnitude.x,max(magnitude.y,magnitude.z)));
    // Near-coincident secondary/receiver points have ill-conditioned angular
    // correspondence. The float plane/origin arithmetic can falsely converge
    // there: its small absolute rounding error becomes a large angular error.
    // Keep an origin-rounding allowance (about eight float epsilons) separate
    // from the .002-pixel solver residual. Reject, never snap or blend, when
    // that allowance exceeds the remaining .012 angular-pixel budget.
    float travel=length(feature-origin);
    if(!isfinite(travel) || scale*1.e-6*max(old.projection.x,old.projection.y)>travel*.012) return 0;
    return float4(pixel,depth,1);
}
float4 reflection_rough_hit_motion(ReflectionRoughFrame old,float4 qa,float4 qb,float4 qc,float2 bary) {
    if(!reflection_rough_frame_valid(old) || qa.w!=1 || qb.w!=1 || qc.w!=1
        || !all(isfinite(qa)) || !all(isfinite(qb)) || !all(isfinite(qc))
        || !all(isfinite(bary)) || any(bary<0) || bary.x+bary.y>1.00001) return 0;
    float3 secondaryNormal=cross(qb.xyz-qa.xyz,qc.xyz-qa.xyz);
    float area=dot(secondaryNormal,secondaryNormal);
    if(!all(isfinite(secondaryNormal)) || !isfinite(area) || area<=1.e-20) return 0;
    float3 feature=qa.xyz*(1-bary.x-bary.y)+qb.xyz*bary.x+qc.xyz*bary.y;
    float3 normal=reflection_rough_normal(old);
    float3 virtualFeature=feature-2*normal*dot(feature-old.a.xyz,normal);
    // A rough lobe may enter the viewport when its central mirror ray does
    // not. Use a constrained seed, not the sharp helper's coverage rejection.
    float2 pixel=old.extentClip.xy*.5;
    if(all(isfinite(virtualFeature)) && virtualFeature.z>0)
        pixel=clamp(old.projection.xy*virtualFeature.xy/virtualFeature.z+old.projection.zw,
            0,old.extentClip.xy-.001);
    const float derivativeStep=.25;
    const float trust=max(8.f,min(old.extentClip.x,old.extentClip.y)*.125f);
    [loop] for(uint iteration=0;iteration<16;++iteration) {
        float2 error;float depth;
        if(!reflection_rough_error(pixel,old,feature,error,depth)) return 0;
        float residual=max(abs(error.x),abs(error.y));
        if(residual<=.002) return reflection_rough_result(old,feature,pixel,depth);
        float2 ex,ey;float unused;
        float dx=pixel.x+derivativeStep<old.extentClip.x?derivativeStep:-derivativeStep;
        float dy=pixel.y+derivativeStep<old.extentClip.y?derivativeStep:-derivativeStep;
        if(!reflection_rough_error(pixel+float2(dx,0),old,feature,ex,unused)
            || !reflection_rough_error(pixel+float2(0,dy),old,feature,ey,unused)) return 0;
        float2 jx=(ex-error)/dx,jy=(ey-error)/dy;
        float determinant=jx.x*jy.y-jy.x*jx.y;
        if(!isfinite(determinant) || abs(determinant)<1.e-8) return 0;
        float2 step=float2(jy.y*error.x-jy.x*error.y,-jx.y*error.x+jx.x*error.y)/determinant;
        if(!all(isfinite(step))) return 0;
        step*=min(1.f,trust/max(max(abs(step.x),abs(step.y)),1.e-12));
        bool advanced=false;
        [loop] for(uint backtrack=0;backtrack<6;++backtrack) {
            float2 next=pixel-step,trial;float trialDepth;
            if(reflection_rough_error(next,old,feature,trial,trialDepth)
                && max(abs(trial.x),abs(trial.y))<residual) {pixel=next;advanced=true;break;}
            step*=.5;
        }
        if(!advanced) return 0;
    }
    float2 error;float depth;
    if(reflection_rough_error(pixel,old,feature,error,depth) && max(abs(error.x),abs(error.y))<=.002)
        return reflection_rough_result(old,feature,pixel,depth);
    return 0;
}
#endif
