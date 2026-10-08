// Accepted finite single-bounce radiance: reflect the old secondary feature
// across the old mirror PLANE, then project that virtual feature in the old
// eye. Moving the primary surface point instead would freeze a sliding image.
// The including native shader supplies previous float4 vertices by primitive.
// The analytic floor's old optical feature uses the OLD plane, not a source
// screen-space horizon or the current floor. The consumer separately verifies
// old coverage, depth and identity; this alone never accepts radiance.
float4 reflection_ground_hit_motion(float3 planePoint,float3 normal,float4 qa,float4 qb,float4 qc,
    float2 bary,float4 projection,float2 extent,float2 clip) {
    if(!all(isfinite(planePoint)) || !all(isfinite(normal)) || dot(normal,normal)<=1.e-20
        || qa.w!=1 || qb.w!=1 || qc.w!=1 || !all(isfinite(qa)) || !all(isfinite(qb)) || !all(isfinite(qc))
        || !all(isfinite(bary)) || any(bary<0) || bary.x+bary.y>1.00001) return 0;
    float3 hitNormal=cross(qb.xyz-qa.xyz,qc.xyz-qa.xyz);
    if(!all(isfinite(hitNormal)) || dot(hitNormal,hitNormal)<=1.e-20) return 0;
    normal=normalize(normal);
    float3 feature=qa.xyz*(1-bary.x-bary.y)+qb.xyz*bary.x+qc.xyz*bary.y;
    float3 virtualFeature=feature-2*normal*dot(feature-planePoint,normal);
    float denominator=dot(virtualFeature,normal);
    if(!all(isfinite(virtualFeature)) || virtualFeature.z<=0 || !isfinite(denominator) || abs(denominator)<=1.e-12) return 0;
    float fraction=dot(planePoint,normal)/denominator;
    float3 receiver=virtualFeature*fraction;
    if(!isfinite(fraction) || fraction<=0 || fraction>=1 || !all(isfinite(receiver)) || receiver.z<clip.x || receiver.z>clip.y) return 0;
    float2 pixel=projection.xy*virtualFeature.xy/virtualFeature.z+projection.zw;
    if(!all(isfinite(pixel)) || any(pixel<0) || any(pixel>=extent)) return 0;
    return float4(pixel,receiver.z,1);
}
float4 reflection_hit_motion(float4 a,float4 b,float4 c,
    float4 qa,float4 qb,float4 qc,float2 secondary_bary,float4 projection,
    float2 extent,float2 clip) {
    if(a.w!=1 || b.w!=1 || c.w!=1 || qa.w!=1 || qb.w!=1 || qc.w!=1
        || !all(isfinite(a)) || !all(isfinite(b)) || !all(isfinite(c))
        || !all(isfinite(qa)) || !all(isfinite(qb)) || !all(isfinite(qc))
        || !all(isfinite(secondary_bary)) || any(secondary_bary<0) || secondary_bary.x+secondary_bary.y>1.00001) return 0;
    float3 ab=b.xyz-a.xyz,ac=c.xyz-a.xyz,normal=cross(ab,ac);
    float length2=dot(normal,normal);
    float3 hitNormal=cross(qb.xyz-qa.xyz,qc.xyz-qa.xyz);
    if(!isfinite(length2) || length2<=1.e-20 || !all(isfinite(hitNormal)) || dot(hitNormal,hitNormal)<=1.e-20) return 0;
    normal*=rsqrt(length2);
    float3 feature=qa.xyz*(1-secondary_bary.x-secondary_bary.y)+qb.xyz*secondary_bary.x+qc.xyz*secondary_bary.y;
    float3 virtualFeature=feature-2*normal*dot(feature-a.xyz,normal);
    if(!all(isfinite(virtualFeature)) || virtualFeature.z<=0) return 0;
    float denominator=dot(virtualFeature,normal);
    if(!isfinite(denominator) || abs(denominator)<=1.e-12) return 0;
    float fraction=dot(a.xyz,normal)/denominator;
    if(!isfinite(fraction) || fraction<=0 || fraction>=1) return 0;
    float3 receiver=virtualFeature*fraction,relative=receiver-a.xyz;
    if(!all(isfinite(receiver)) || receiver.z<clip.x || receiver.z>clip.y) return 0;
    // The old mirror is finite. A virtual feature beyond its accepted face
    // must not borrow a neighbouring or unrelated old receiver's radiance.
    float aa=dot(ab,ab),bb=dot(ab,ac),cc=dot(ac,ac),ra=dot(relative,ab),rc=dot(relative,ac);
    float determinant=aa*cc-bb*bb;
    if(!isfinite(determinant) || determinant<=1.e-20) return 0;
    float2 bary=float2(cc*ra-bb*rc,aa*rc-bb*ra)/determinant;
    if(!all(isfinite(bary)) || any(bary<-.00001) || bary.x+bary.y>1.00001) return 0;
    float2 pixel=projection.xy*virtualFeature.xy/virtualFeature.z+projection.zw;
    if(!all(isfinite(pixel)) || any(pixel<0) || any(pixel>=extent)) return 0;
    // A consumer MUST still verify accepted receiver ownership/depth and the
    // reflected layer's own accepted colour. This is not final image history.
    return float4(pixel,receiver.z,1);
}
