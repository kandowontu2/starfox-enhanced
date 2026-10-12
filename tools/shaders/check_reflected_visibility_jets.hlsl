// Diagnostic boxes are inputs only. The GPU computes all derivatives and
// local fixed-point certificates; no downloaded solve/root/certificate feeds it.
ByteAddressBuffer inputs:register(t0,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer JetSettings:register(b0,space2) {uint jetCount,jetRefinements,jetUnused1,jetUnused2;};
#include "../../src/render/shaders/reflection_source_optical_local_root.hlsli"
[numthreads(64,1,1)]
void feature_jets_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=jetCount)return;
    const uint at=id.x*464,outAt=id.x*192;
    [unroll] for(uint n=0;n<12;++n)results.Store4(outAt+n*16,0);
    if(jetUnused1==1) {
        // A separate arithmetic contract, not a physical/source fixture:
        // F(x,y)=(x-.5,y-.5) has its exact root at (.5,.5).
        // The second valid K image deliberately extends one parent edge;
        // its status is unresolved although it still encloses that root.
        OpticalLocalRoot proved=optical_local_root(float4(0,0,1,1),float2(.5,.5),
            iv(-.125,.125),iv(-.125,.125),ip(1),ip(0),ip(0),ip(1));
        const float4 before=proved.enclosure;
        OpticalLocalRoot next=optical_local_root(before,float2(.5,.5),
            iv(0,.25),iv(-.05,.05),ip(1),ip(0),ip(0),ip(1));
        if(id.x==1)next=optical_local_root(before,float2(.5,.5),
            iv(-.01,.01),iv(-.01,.01),ip(1),ip(0),ip(0),ip(1));
        if(id.x==2)next=optical_local_root(before,float2(.5,.5),
            ip(0),ip(0),ip(0),ip(0),ip(0),ip(0)); // Invalid preconditioner, status0.
        if(id.x==3)next=optical_local_root(before,float2(.5,.5),
            ip(4),ip(4),ip(1),ip(0),ip(0),ip(1)); // Checked disjoint/no-root.
        if(id.x==4)jetBranchKnown=false;
        if(id.x==5)intervalFailed=true;
        if(id.x==6)proved.status=0; // A merely unresolved first result.
        if(id.x==7)proved.evaluated=false; // No inherited checked proof.
        const bool accepted=optical_intersect_inherited_root(proved,next);
        results.Store4(outAt,uint4(proved.status,next.status,uint(next.evaluated),uint(accepted)));
        results.Store4(outAt+16,asuint(before));
        results.Store4(outAt+144,asuint(proved.enclosure));
        return;
    }
    if(jetUnused1 || jetUnused2)return;
    ReflectionRoughFrame receiver;
    receiver.a=asfloat(inputs.Load4(at));receiver.b=asfloat(inputs.Load4(at+16));receiver.c=asfloat(inputs.Load4(at+32));
    receiver.projection=asfloat(inputs.Load4(at+48));receiver.extentClip=asfloat(inputs.Load4(at+64));receiver.settings=asfloat(inputs.Load4(at+80));
    ReflectionSpecularPlane planes[4];[unroll] for(uint h=0;h<4;++h) {
        planes[h].a=asfloat(inputs.Load4(at+96+h*48));planes[h].b=asfloat(inputs.Load4(at+112+h*48));planes[h].c=asfloat(inputs.Load4(at+128+h*48));
    }
    ReflectionLiquidFrame liquid;liquid.planePoint=asfloat(inputs.Load4(at+288));liquid.planeNormal=asfloat(inputs.Load4(at+304));
    liquid.rotation0=asfloat(inputs.Load4(at+320));liquid.rotation1=asfloat(inputs.Load4(at+336));liquid.rotation2=asfloat(inputs.Load4(at+352));
    liquid.projection=asfloat(inputs.Load4(at+368));liquid.extentClip=asfloat(inputs.Load4(at+384));liquid.settings=asfloat(inputs.Load4(at+400));
    const float4 terminal=asfloat(inputs.Load4(at+416)),box=asfloat(inputs.Load4(at+448));const uint4 control=inputs.Load4(at+432);
    if(jetRefinements>2 || control.x>4 || (control.y>>control.x)!=0 || control.z>1 || control.w<1 || control.w>3
        || (control.w==3?control.x!=4:control.x==4) || !all(isfinite(receiver.settings))
        || receiver.settings.x<0 || receiver.settings.x>1 || receiver.settings.y<0 || receiver.settings.y>7
        || receiver.settings.y!=floor(receiver.settings.y) || any(receiver.settings.zw!=0)
        || !all(isfinite(receiver.projection)) || any(receiver.projection.xy<=0)
        || !all(isfinite(receiver.extentClip)) || any(receiver.extentClip.xy<1) || any(receiver.extentClip.xy>16384)
        || receiver.extentClip.z<=0 || receiver.extentClip.w<=receiver.extentClip.z
        || !all(isfinite(terminal)) || terminal.w!=(control.w==1?1:0)
        || (!control.z && !reflection_rough_frame_valid(receiver)))return;
    if(control.z || control.y)if(!reflection_liquid_frame_valid(liquid)
        || any(receiver.projection!=liquid.projection) || any(receiver.extentClip!=liquid.extentClip))return;
    [loop] for(uint h=0;h<control.x;++h)if(!(control.y&(1U<<h)) && !reflection_specular_plane_valid(planes[h]))return;
    write_optical_local_root(outAt,box,receiver,liquid,planes,control,p3(terminal.xyz),terminal.w!=0,jetRefinements);
}
