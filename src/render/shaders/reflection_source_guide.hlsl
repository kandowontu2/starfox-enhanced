// A proof-only finite-face/angular guide, not a colour-history consumer.
// Use the existing strict colour-root iteration and full ordered old optics.
#define STARFOX_CURVED_COLOUR_ROOT 1
#include "reflection_curved_path_motion.hlsli"
#define STARFOX_SOURCE_WITNESS_LIBRARY 1
#include "reflection_source_witness.hlsl"

void guide_refuse(uint at,uint reason) {results.Store4(at,uint4(0,reason,0,0));}
[numthreads(64,1,1)]
void feature_guide_main(uint3 id:SV_DispatchThreadID) {
    const uint q=id.x;if(q>=queryCount || q>=64)return;
    const uint rootAt=q*24576,at=rootAt+320,frameAt=q*512;
    [unroll] for(uint n=0;n<4;++n)results.Store4(at+n*16,0);
    if(queryCount>64 || witnessStride!=24576 || witnessOffset!=320 || witnessBytes!=64 || witnessReserved
        || (pathStride!=52 && pathStride!=64) || !width || !height || width>16384 || height>16384
        || (lobes!=1 && lobes!=8) || any(frames.Load4(frameAt+496)!=uint4(1,0,0,0)))
        {guide_refuse(at,2);return;}
    if(results.Load(rootAt)!=1 || results.Load(rootAt+4)!=1 || results.Load(rootAt+12)
        || results.Load(rootAt+192)!=1 || results.Load(rootAt+196)!=31 || results.Load(rootAt+200)
        || results.Load(rootAt+256)!=1 || results.Load(rootAt+260)
        || any(results.Load4(rootAt+144)!=results.Load4(rootAt+208))
        || any(results.Load4(rootAt+144)!=results.Load4(rootAt+272)))
        {guide_refuse(at,1);return;}
    const float4 enclosure=asfloat(results.Load4(rootAt+144));results.Store4(at+16,asuint(enclosure));
    if(!all(isfinite(enclosure)) || any(enclosure.xy>enclosure.zw)
        || any(enclosure.xy<.5) || any(enclosure.zw>float2(width,height)-.5))
        {guide_refuse(at,2);return;}
    const uint4 control=frames.Load4(frameAt+480);
    const uint primary=queries.Load(q*48),lobe=queries.Load(q*48+44);
    if(primary==0xffffffffU || lobe>=lobes || control.x>4 || (control.y>>control.x)!=0
        || control.z>1 || control.w<1 || control.w>3
        || queries.Load(q*48+20)!=((control.w<<8)|(control.y<<4)|control.x)
        || control.z!=(primary==0xfffffffdU?1U:0U)) {guide_refuse(at,2);return;}
    ReflectionRoughFrame receiver;receiver.a=asfloat(frames.Load4(frameAt));receiver.b=asfloat(frames.Load4(frameAt+16));
    receiver.c=asfloat(frames.Load4(frameAt+32));receiver.projection=asfloat(frames.Load4(frameAt+48));
    receiver.extentClip=asfloat(frames.Load4(frameAt+64));receiver.settings=asfloat(frames.Load4(frameAt+80));
    ReflectionSpecularPlane planes[4];
    [unroll] for(uint hop=0;hop<4;++hop) {
        planes[hop].a=asfloat(frames.Load4(frameAt+96+hop*48));planes[hop].b=asfloat(frames.Load4(frameAt+112+hop*48));
        planes[hop].c=asfloat(frames.Load4(frameAt+128+hop*48));
    }
    ReflectionLiquidFrame liquid;liquid.planePoint=asfloat(frames.Load4(frameAt+288));liquid.planeNormal=asfloat(frames.Load4(frameAt+304));
    liquid.rotation0=asfloat(frames.Load4(frameAt+320));liquid.rotation1=asfloat(frames.Load4(frameAt+336));
    liquid.rotation2=asfloat(frames.Load4(frameAt+352));liquid.projection=asfloat(frames.Load4(frameAt+368));
    liquid.extentClip=asfloat(frames.Load4(frameAt+384));liquid.settings=asfloat(frames.Load4(frameAt+400));
    if(!control.z && !control.y) {
        // Shared library validation requires an unused valid liquid frame for
        // purely finite planar paths. No wave or liquid surface is sampled.
        liquid.planePoint=0;liquid.planeNormal=float4(0,1,0,0);
        liquid.rotation0=float4(1,0,0,0);liquid.rotation1=float4(0,1,0,0);liquid.rotation2=float4(0,0,1,0);
        liquid.projection=receiver.projection;liquid.extentClip=receiver.extentClip;liquid.settings=0;
    }
    if(control.z || reflection_rough_analytic(receiver))receiver.settings=0;
    const float4 feature=asfloat(frames.Load4(frameAt+416));float4 target=0;
    if(feature.w==1 && control.w==1) {
        // Preserve the existing colour consumer's compensated barycentric
        // endpoint recipe. Never use an interval target midpoint as a guide.
        const float2 a=curved_dd_sub(curved_dd_sub(float2(1,0),float2(feature.x,0)),float2(feature.y,0));
        const CurvedVector endpoint=curved_vec_add(curved_vec_add(
            curved_vec_scale(curved_vec_float(asfloat(frames.Load4(frameAt+432)).xyz),a),
            curved_vec_scale(curved_vec_float(asfloat(frames.Load4(frameAt+448)).xyz),float2(feature.x,0))),
            curved_vec_scale(curved_vec_float(asfloat(frames.Load4(frameAt+464)).xyz),float2(feature.y,0)));
        target=float4(curved_vec_result(endpoint),1);
    } else if(feature.w==0 && (control.w==2 || control.w==3)) {
        const float3 world=float3(dot(targetCurrentCube[0].xyz,feature.xyz),
            dot(targetCurrentCube[1].xyz,feature.xyz),dot(targetCurrentCube[2].xyz,feature.xyz));
        target=float4(normalize(world.x*targetPreviousCube[0].xyz+world.y*targetPreviousCube[1].xyz
            +world.z*targetPreviousCube[2].xyz),0);
    } else {guide_refuse(at,3);return;}
    if(!all(isfinite(target))) {guide_refuse(at,3);return;}
    // Midpoint is ONLY an initializer; success needs the unchanged converged
    // full solver and finite-face retrace, then containment and angular gates.
    const float2 seed=enclosure.xy+(enclosure.zw-enclosure.xy)*.5;
    const float4 solved=reflection_curved_path_motion(receiver,liquid,planes,control.x,control.y,control.z,target,seed);
    results.Store4(at+32,asuint(solved));
    if(solved.w!=1 || !all(isfinite(solved))) {guide_refuse(at,4);return;}
    if(any(solved.xy<enclosure.xy) || any(solved.xy>enclosure.zw)) {guide_refuse(at,5);return;}
    float2 error;float depth;
    if(!reflection_curved_error(solved.xy,receiver,liquid,planes,control.x,control.y,control.z,target,error,depth)
        || !isfinite(depth) || max(abs(error.x),abs(error.y))>.002) {guide_refuse(at,6);return;}
    results.Store4(at+48,asuint(float4(error,depth,0)));results.Store4(at,uint4(1,0,0,0));
    // Still no old RGB sampling, CURRENT neighbourhood or partial-lobe output.
}
