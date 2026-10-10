// Consume real GPU optical frames and complete retained support hulls. No host
// endpoint, authored box, inverse seed, old colour or downloaded root input.
#include "reflection_source_index_settings.hlsli"
ByteAddressBuffer frames:register(t0,space0),regions:register(t1,space0),queries:register(t2,space0),optical:register(t3,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer RootSettings:register(b1,space2) {uint rootCapacity,rootFrameStride,rootResultStride,rootMode;};
#include "reflection_source_optical_local_root.hlsli"
#include "reflection_source_optical_target.hlsli"
#if defined(STARFOX_SOURCE_STREAM_ROOTS)
void stream_root_reject(uint at) {
    // The generic writer reports upstream refusal in EVERY region slot,
    // even in whole-hull mode. Preserve that complete diagnostic ABI.
    [loop] for(uint region=0;region<128;++region)results.Store(at+region*192+12,32);
}
#endif
[numthreads(64,1,1)]
void feature_jets_main(uint3 id:SV_DispatchThreadID) {
#if defined(STARFOX_SOURCE_STREAM_ROOTS)
    // The complete-frame stream needs ONE whole-union hull per query, not
    // 128 mostly inactive region lanes. All optical math below is shared.
    const uint q=id.x,r=0;
#define sourceRootMode 1U
#else
    const uint q=id.y,r=id.x;
#define sourceRootMode rootMode
#endif
    if(q>=queryCount || q>=64 || r>=128)return;
    const uint outAt=(q*128+r)*192,input=q*6160,at=q*512;
#if defined(STARFOX_SOURCE_STREAM_ROOTS)
    // The owner zeroes all unused region records ONCE before the full stream.
    // Each batch resets the hull and every subsequent proof/colour packet.
    [unroll] for(uint n=0;n<36;++n)results.Store4(outAt+n*16,0);
    // Unused region records are otherwise zero, except their upstream refusal
    // word. Reset that word on reuse after a preceding rejected query.
    [loop] for(uint region=3;region<128;++region)results.Store(outAt+region*192+12,0);
    if(rootMode!=1) {stream_root_reject(outAt);return;}
#else
    [unroll] for(uint n=0;n<12;++n)results.Store4(outAt+n*16,0);
#endif
    const uint count=regions.Load(input),status=regions.Load(input+4);
    if(queryCount>64 || rootCapacity!=128 || rootFrameStride!=512 || rootResultStride!=192 || rootMode>1
        || count>128 || status) {
#if defined(STARFOX_SOURCE_STREAM_ROOTS)
        stream_root_reject(outAt);
#else
        results.Store(outAt+12,32);
#endif
        return;
    }
    // Mode 1 reuses exactly this checked forward writer on ONE superset of the
    // complete necessary-source union. Min/max selects existing outward bounds;
    // it never drops singleton/shared supports or guesses equality from root
    // distance/overlapping enclosures. Hull holes still need the source guard.
    if(sourceRootMode && r)return;
    if(!sourceRootMode && r>=count)return;
    bool retained=false;float4 box=float4(1.e30,1.e30,-1.e30,-1.e30);
    [loop] for(uint n=sourceRootMode?0:r;n<(sourceRootMode?count:r+1);++n) {
        const uint4 receipt=optical.Load4((q*128+n)*32);
        if(receipt.y || !receipt.z || receipt.z>8192 || receipt.x>receipt.z || receipt.w!=receipt.z-receipt.x)
            {results.Store(outAt+12,32);return;}
        if(!receipt.x)continue;
        const float4 region=asfloat(optical.Load4((q*128+n)*32+16));
        if(!all(isfinite(region)) || any(region.xy>region.zw)) {results.Store(outAt+12,64);return;}
        box=float4(min(box.xy,region.xy),max(box.zw,region.zw));retained=true;
    }
    if(any(frames.Load4(at+496)!=uint4(1,0,0,0))) {results.Store(outAt+12,64);return;}
    ReflectionRoughFrame receiver;
    receiver.a=asfloat(frames.Load4(at));receiver.b=asfloat(frames.Load4(at+16));receiver.c=asfloat(frames.Load4(at+32));
    receiver.projection=asfloat(frames.Load4(at+48));receiver.extentClip=asfloat(frames.Load4(at+64));receiver.settings=asfloat(frames.Load4(at+80));
    ReflectionSpecularPlane planes[4];[unroll] for(uint h=0;h<4;++h) {
        planes[h].a=asfloat(frames.Load4(at+96+h*48));planes[h].b=asfloat(frames.Load4(at+112+h*48));planes[h].c=asfloat(frames.Load4(at+128+h*48));
    }
    ReflectionLiquidFrame liquid;liquid.planePoint=asfloat(frames.Load4(at+288));liquid.planeNormal=asfloat(frames.Load4(at+304));
    liquid.rotation0=asfloat(frames.Load4(at+320));liquid.rotation1=asfloat(frames.Load4(at+336));liquid.rotation2=asfloat(frames.Load4(at+352));
    liquid.projection=asfloat(frames.Load4(at+368));liquid.extentClip=asfloat(frames.Load4(at+384));liquid.settings=asfloat(frames.Load4(at+400));
    const float4 terminal=asfloat(frames.Load4(at+416));const uint4 control=frames.Load4(at+480);
    const uint queryControl=queries.Load(q*48+20),queryPrimary=queries.Load(q*48),queryLobe=queries.Load(q*48+44);
    ReflectionSpecularPlane terminalPlane;terminalPlane.a=asfloat(frames.Load4(at+432));
    terminalPlane.b=asfloat(frames.Load4(at+448));terminalPlane.c=asfloat(frames.Load4(at+464));
    if(control.x>4 || (control.y>>control.x)!=0 || control.z>1 || control.w<1 || control.w>3
        || (control.w==3?control.x!=4:control.x==4) || queryPrimary==0xffffffffU
        || control.z!=(queryPrimary==0xfffffffdU?1U:0U) || queryLobe>=lobes || queryLobe>7
        || queryControl!=((control.w<<8)|(control.y<<4)|control.x)
        || !all(isfinite(receiver.settings)) || receiver.settings.x<0 || receiver.settings.x>1
        || receiver.settings.y!=float(queryLobe) || any(receiver.settings.zw!=0)
        || !all(isfinite(receiver.projection)) || any(receiver.projection.xy<=0)
        || !all(isfinite(receiver.extentClip)) || any(receiver.extentClip.xy!=float2(width,height))
        || any(receiver.extentClip.xy<1) || any(receiver.extentClip.xy>16384)
        || receiver.extentClip.z<=0 || receiver.extentClip.w<=receiver.extentClip.z
        || !all(isfinite(terminal)) || terminal.w!=(control.w==1?1:0)
        || !feature_valid(terminal.xyz,control.w)
        || (terminal.w!=0 && !reflection_specular_plane_valid(terminalPlane))
        || (!control.z && !reflection_rough_frame_valid(receiver))) {results.Store(outAt+12,64);return;}
    if(control.z || control.y)if(!reflection_liquid_frame_valid(liquid)
        || any(receiver.projection!=liquid.projection) || any(receiver.extentClip!=liquid.extentClip))
        {results.Store(outAt+12,64);return;}
    [loop] for(uint h=0;h<control.x;++h)if(!(control.y&(1U<<h)) && !reflection_specular_plane_valid(planes[h]))
        {results.Store(outAt+12,64);return;}
    // Complete upstream optical exclusion is kept distinct from a LOCAL root
    // classification. Neither excludes a different source/branch or accepts RGB.
    if(!retained) {results.Store(outAt+12,4096);return;}
    if(!all(isfinite(box)) || any(box.xy>box.zw)) {results.Store(outAt+12,64);return;}
    intervalFailed=false;const Interval3 target=native_target(at,terminal);
    // Do not lose target-construction failure when the forward writer resets
    // per-evaluation arithmetic state.
    if(intervalFailed) {results.Store(outAt+12,128);return;}
    // A necessary-source hull can be arbitrarily thin. A valid but unresolved
    // fixed-point enclosure is not colour permission. For WHOLE-source mode
    // only, retry once on a checked outward SUPerset: no original support is
    // removed and the same full path/finite-face/contraction proof must hold
    // over the larger box. Local/per-region diagnostics remain exact as before.
    // The extra work is bounded; failed arithmetic/branches are never retried
    // as though they were a complete physical map, and no guide is uploaded.
    [loop] for(uint attempt=0;attempt<2;++attempt) {
        [unroll] for(uint n=0;n<12;++n)results.Store4(outAt+n*16,0);
        write_optical_local_root(outAt,box,receiver,liquid,planes,control,target,terminal.w!=0,sourceRootMode?2U:0U);
        if(!sourceRootMode || attempt || results.Load(outAt)!=1 || results.Load(outAt+4)!=0 || results.Load(outAt+12))return;
        intervalFailed=false;
        const Interval x=isub(ip(box.x),ip(.125)),y=isub(ip(box.y),ip(.125)),
            z=iadd(ip(box.z),ip(.125)),w=iadd(ip(box.w),ip(.125));
        if(intervalFailed)return;
        // Clipping must never compress the original necessary-source hull.
        const float4 expanded=float4(min(box.xy,max(float2(.5,.5),float2(x.lo,y.lo))),
            max(box.zw,min(receiver.extentClip.xy-.5,float2(z.hi,w.hi))));
        if(!all(isfinite(expanded)) || any(expanded.xy>box.xy) || any(expanded.zw<box.zw)
            || all(expanded==box))return;
        box=expanded;
    }
}
