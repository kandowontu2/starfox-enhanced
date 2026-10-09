// Isolate the SAME primary-lava map, not a lower-fidelity reflection path.
// The complete verifier remains separately mandatory. Trace first checked
// arithmetic refusal; do not turn that refusal into a usable certificate.
#include "../../src/render/shaders/reflection_source_optical_jet.hlsli"
ByteAddressBuffer inputs:register(t0,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer TraceSettings:register(b0,space2) {uint traceCount,traceUnused0,traceUnused1,traceUnused2;};
void trace_interval(uint at,Interval a) {results.Store2(at,asuint(float2(a.lo,a.hi)));}
[numthreads(64,1,1)]
void feature_arithmetic_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=traceCount)return;
    const uint at=id.x*464,outAt=id.x*192;
    [unroll] for(uint n=0;n<12;++n)results.Store4(outAt+n*16,0);
    ReflectionRoughFrame receiver=(ReflectionRoughFrame)0;
    receiver.projection=asfloat(inputs.Load4(at+48));receiver.extentClip=asfloat(inputs.Load4(at+64));
    ReflectionLiquidFrame liquid;liquid.planePoint=asfloat(inputs.Load4(at+288));liquid.planeNormal=asfloat(inputs.Load4(at+304));
    liquid.rotation0=asfloat(inputs.Load4(at+320));liquid.rotation1=asfloat(inputs.Load4(at+336));liquid.rotation2=asfloat(inputs.Load4(at+352));
    liquid.projection=asfloat(inputs.Load4(at+368));liquid.extentClip=asfloat(inputs.Load4(at+384));liquid.settings=asfloat(inputs.Load4(at+400));
    if(!reflection_liquid_frame_valid(liquid) || any(receiver.projection!=liquid.projection)
        || any(receiver.extentClip!=liquid.extentClip) || liquid.settings.y!=3)return;
    const float4 box=asfloat(inputs.Load4(at+448));OpticalJetRay ray;
    ray.origin=ray.outgoing=jp3(0);ray.depth=ray.bias=jp(0);
    intervalFailed=false;jetBranchKnown=true;jetFailureSite=0;jetFailureArguments=0;
    if(!all(isfinite(box)) || any(box.xy>box.zw) || any(box.xy<0)
        || box.z>=receiver.extentClip.x || box.w>=receiver.extentClip.y)return;
    // Copy only the primary-curved prefix's operation grouping, using the SAME
    // jet/wave/normal helpers. A literal zero-hop call to the full loop still
    // retained the full stage graph in SPIR-V. This trace is not a replacement
    // for that loop's verification, and contains no simplified/flat wave.
    const OpticalJet x=jet(iv(box.x,box.z),ip(1),ip(0)),y=jet(iv(box.y,box.w),ip(0),ip(1));
    const OpticalJet3 outgoing=junit(j3(jd(js(x,jp(receiver.projection.z)),jp(receiver.projection.x)),
        jd(js(y,jp(receiver.projection.w)),jp(receiver.projection.y)),jp(1)));
    const OpticalJet3 origin=jp3(0);OpticalJet3 normal=jp3(liquid.planeNormal.xyz);
    const OpticalJet denominator=jdot(outgoing,normal);
    const OpticalJet distance=jd(jdot(jsub3(jp3(liquid.planePoint.xyz),origin),normal),denominator);
    ray.depth=jm(distance,outgoing.z);
    if(iabs(denominator.v).lo<=ic(1.e-12).hi || distance.v.lo<=0
        || ray.depth.v.lo<receiver.extentClip.z || ray.depth.v.hi>receiver.extentClip.w)return;
    OpticalJet3 hit=jadd3(origin,jscale(outgoing,distance));
    const OpticalJet footprint=jd(jd(distance,jp(max(liquid.projection.x,1))),jmax(jabs(denominator),jr(4,100)));
    liquid.settings.y=3; // Already required exactly three above, not a substituted material.
    normal=jet_liquid_normal(liquid,outgoing,distance,footprint,hit);
    ray.bias=jmax(jr(5,100),jm(distance,jr(1,100000)));
    ray.origin=jadd3(hit,jscale(normal,ray.bias));ray.outgoing=jreflect(outgoing,normal);
    const bool valid=!intervalFailed && jetBranchKnown;
    results.Store4(outAt,uint4(uint(valid),uint(intervalFailed),uint(!jetBranchKnown),jetFailureSite));
    results.Store4(outAt+16,asuint(box));results.Store4(outAt+160,asuint(jetFailureArguments));
    trace_interval(outAt+32,ray.origin.x.v);trace_interval(outAt+40,ray.origin.y.v);trace_interval(outAt+48,ray.origin.z.v);
    trace_interval(outAt+56,ray.outgoing.x.v);trace_interval(outAt+64,ray.outgoing.y.v);trace_interval(outAt+72,ray.outgoing.z.v);
    trace_interval(outAt+80,ray.depth.v);trace_interval(outAt+88,ray.bias.v);
}
