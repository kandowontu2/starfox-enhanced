// Shared complete forward/residual/local-root writer for authored fixtures and
// native GPU frames. LOCAL classifications never authorize history colour.
#ifndef STARFOX_REFLECTION_SOURCE_OPTICAL_LOCAL_ROOT
#define STARFOX_REFLECTION_SOURCE_OPTICAL_LOCAL_ROOT
#include "reflection_source_optical_jet.hlsli"
// The including wrapper declares its own bounded RWByteAddressBuffer results.
void store_interval(uint at,Interval a) {results.Store2(at,asuint(float2(a.lo,a.hi)));}
void write_optical_local_root(uint outAt,float4 box,ReflectionRoughFrame receiver,
    ReflectionLiquidFrame liquid,ReflectionSpecularPlane planes[4],uint4 control,
    Interval3 target,bool finiteTarget,uint refinements) {
    const float2 centre=(box.xy+box.zw)*.5;
    OpticalJet a=jp(0),b=jp(0);Interval f0=ip(0),f1=ip(0),j00=ip(0),j01=ip(0),j10=ip(0),j11=ip(0);
    uint axis=0;OpticalLocalRoot root;root.status=0;root.enclosure=0;root.contraction=1.e30;
    // One static forward/wave call site for box, midpoint and bounded proved
    // enclosure refinement. No derivative approximation or cloned lava code.
    // J(box) encloses the derivative on EVERY nested enclosure; only a complete
    // unique certificate may start refinement. Original global domain/proof
    // stays intact. Each accepted tighter K is strictly inside its predecessor.
    [loop] for(uint evaluation=0;evaluation<2+min(refinements,2U);++evaluation) {
        if(evaluation>=2 && root.status!=1)break;
        const float4 previousEnclosure=root.enclosure;
        const float2 localCentre=evaluation>=2?(previousEnclosure.xy+previousEnclosure.zw)*.5:centre;
        intervalFailed=false;jetBranchKnown=true;OpticalJetRay ray;
        const float4 extent=evaluation?float4(localCentre,localCentre):box;
        if(!optical_jet_forward(extent,receiver,liquid,planes,control,ray)) {
            if(evaluation>=2)break; // Keep the preceding valid, wider proof.
            results.Store(outAt+12,8U+uint(intervalFailed)+2U*uint(!jetBranchKnown));return;
        }
        if(!evaluation) {
            const float3 midpoint=float3((ray.outgoing.x.v.lo+ray.outgoing.x.v.hi)*.5,
                (ray.outgoing.y.v.lo+ray.outgoing.y.v.hi)*.5,(ray.outgoing.z.v.lo+ray.outgoing.z.v.hi)*.5);
            axis=abs(midpoint.x)>abs(midpoint.y)?0:1;if(abs(midpoint.z)>abs(midpoint[axis]))axis=2;
            store_interval(outAt+32,ray.origin.x.v);store_interval(outAt+40,ray.origin.y.v);store_interval(outAt+48,ray.origin.z.v);
            store_interval(outAt+56,ray.outgoing.x.v);store_interval(outAt+64,ray.outgoing.y.v);store_interval(outAt+72,ray.outgoing.z.v);
            store_interval(outAt+80,ray.depth.v);store_interval(outAt+88,ray.bias.v);
        }
        if(!optical_jet_residual(ray,target,finiteTarget,max(receiver.projection.x,receiver.projection.y),axis,a,b)) {
            if(evaluation>=2)break;
            results.Store(outAt+12,16U+uint(intervalFailed)+2U*uint(!jetBranchKnown));return;
        }
        if(!evaluation) {
            store_interval(outAt+96,a.v);store_interval(outAt+104,b.v);
            j00=a.dx;j01=a.dy;j10=b.dx;j11=b.dy;
            store_interval(outAt+112,j00);store_interval(outAt+120,j01);store_interval(outAt+128,j10);store_interval(outAt+136,j11);
        }else if(evaluation==1) {
            f0=a.v;f1=b.v;
            root=optical_local_root(box,centre,f0,f1,j00,j01,j10,j11);
        }else {
            // Outward Krawczyk inclusion using the SAME complete original
            // derivative hull. No new seed/box from a CPU or source tap.
            const OpticalLocalRoot next=optical_local_root(previousEnclosure,localCentre,a.v,b.v,j00,j01,j10,j11);
            if(intervalFailed || next.status!=1 || any(next.enclosure.xy<previousEnclosure.xy)
                || any(next.enclosure.zw>previousEnclosure.zw))break;
            root.enclosure=next.enclosure;
        }
    }
    results.Store4(outAt,uint4(1,root.status,axis,0));results.Store4(outAt+16,asuint(box));
    results.Store4(outAt+144,asuint(root.enclosure));results.Store4(outAt+160,asuint(float4(root.contraction,centre,0)));
    store_interval(outAt+176,f0);store_interval(outAt+184,f1);
    // LOCAL status 1 never permits old RGB. Complete necessary-domain cover,
    // overlap/branch accounting and all native colour guards are still required.
}
#endif
