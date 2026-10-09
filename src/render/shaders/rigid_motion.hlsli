// Shared by the standalone motion pass and the painter merge. Correspondence
// always uses this foreground object's depth/transform, never a merged scene.
float4 rigidSurfaceMotion(uint index,uint width,uint resetHistory,float z,
    float4 currentProjection,float4 previousProjection,
    float4 previousRow0,float4 previousRow1,float4 previousRow2,
    float2 jitter,float previousNear) {
    float4 result=0;
    if(resetHistory==0 && isfinite(z) && z>0) {
        float2 pixel=float2(index%width,index/width)+.5;
        float2 unjittered=pixel-jitter;
        float4 camera=float4((unjittered-currentProjection.zw)/currentProjection.xy*z,z,1);
        float3 previous=float3(dot(previousRow0,camera),dot(previousRow1,camera),dot(previousRow2,camera));
        if(all(isfinite(previous)) && previous.z>previousNear) {
            float2 delta=previous.xy/previous.z*previousProjection.xy+previousProjection.zw-unjittered;
            if(all(isfinite(delta))) result=float4(delta,z,1);
        }
    }
    return result;
}
