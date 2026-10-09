// Consume actual GPU supports; exclude empty optical boxes over a continuum.
// Remaining hulls are UNRESOLVED, not roots or colour-history certificates.
#include "reflection_source_index_settings.hlsli"
#include "reflection_source_optical_interval.hlsli"
#ifndef STARFOX_SOURCE_OPTICAL_LANES
#define STARFOX_SOURCE_OPTICAL_LANES 64
#endif
cbuffer OpticalSettings:register(b1,space2) {uint opticalBudget,opticalDepth,opticalCapacity,opticalUnused;};
ByteAddressBuffer regions:register(t0,space0),frames:register(t1,space0),queries:register(t2,space0);
#if defined(STARFOX_SOURCE_OPTICAL_STREAM)
ByteAddressBuffer workMap:register(t3,space0);
#endif
RWByteAddressBuffer results:register(u0,space1);
static const uint capacity=128,regionStride=16+capacity*48,resultStride=capacity*32;

#if defined(STARFOX_NATIVE_SOURCE_OPTICAL)
#include "reflection_source_optical_target.hlsli"
#endif

groupshared uint keptCount,excludedCount,hullX0,hullY0,hullX1,hullY1;
float split_axis(float lo,float hi,uint cell,uint count) {
    if(!cell)return lo;if(cell==count)return hi;
    precise float span=hi-lo,fraction=float(cell)/float(count),position=lo+span*fraction;return position;
}
[numthreads(STARFOX_SOURCE_OPTICAL_LANES,1,1)]
void feature_optical_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
#if defined(STARFOX_SOURCE_OPTICAL_STREAM)
    if(group.x>=64*128 || group.y || group.z)return;
    const uint2 task=workMap.Load2(group.x*8);
    const uint q=task.x,r=task.y;if(q>=queryCount || r>=capacity)return;
#else
    const uint q=group.y,r=group.x;if(q>=queryCount || r>=capacity)return;
#endif
    const uint input=q*regionStride,output=q*resultStride+r*32;
    const uint count=regions.Load(input),status=regions.Load(input+4);
    if(!lane) {results.Store4(output,uint4(0,status,0,0));results.Store4(output+16,0);}
    if(status || r>=count)return;
    if(count>capacity || opticalCapacity!=capacity || !opticalBudget || opticalBudget>8192 || opticalDepth>12) {if(!lane)results.Store(output+4,4);return;}
    #if defined(STARFOX_NATIVE_SOURCE_OPTICAL)
    const uint at=q*512;
    if(any(frames.Load4(at+496)!=uint4(1,0,0,0))) {if(!lane)results.Store(output+4,4);return;}
#else
    const uint at=q*448;
#endif
    ReflectionRoughFrame receiver;
    receiver.a=asfloat(frames.Load4(at));receiver.b=asfloat(frames.Load4(at+16));receiver.c=asfloat(frames.Load4(at+32));
    receiver.projection=asfloat(frames.Load4(at+48));receiver.extentClip=asfloat(frames.Load4(at+64));receiver.settings=asfloat(frames.Load4(at+80));
    ReflectionSpecularPlane planes[4];[unroll] for(uint h=0;h<4;++h) {planes[h].a=asfloat(frames.Load4(at+96+h*48));planes[h].b=asfloat(frames.Load4(at+112+h*48));planes[h].c=asfloat(frames.Load4(at+128+h*48));}
    ReflectionLiquidFrame liquid;liquid.planePoint=asfloat(frames.Load4(at+288));liquid.planeNormal=asfloat(frames.Load4(at+304));
    liquid.rotation0=asfloat(frames.Load4(at+320));liquid.rotation1=asfloat(frames.Load4(at+336));liquid.rotation2=asfloat(frames.Load4(at+352));
    liquid.projection=asfloat(frames.Load4(at+368));liquid.extentClip=asfloat(frames.Load4(at+384));liquid.settings=asfloat(frames.Load4(at+400));
    const float4 terminal=asfloat(frames.Load4(at+416));
#if defined(STARFOX_NATIVE_SOURCE_OPTICAL)
    const uint4 control=frames.Load4(at+480);
    const bool needsLiquid=control.z!=0 || control.y!=0;
    bool liquidValid=!needsLiquid || (reflection_liquid_frame_valid(liquid)
        && all(receiver.projection==liquid.projection) && all(receiver.extentClip==liquid.extentClip));
    ReflectionSpecularPlane terminalPlane;terminalPlane.a=asfloat(frames.Load4(at+432));
    terminalPlane.b=asfloat(frames.Load4(at+448));terminalPlane.c=asfloat(frames.Load4(at+464));
    if(!feature_valid(terminal.xyz,control.w) || (terminal.w!=0 && !reflection_specular_plane_valid(terminalPlane)))
        {if(!lane)results.Store(output+4,4);return;}
#else
    const uint4 control=frames.Load4(at+432);
    const bool liquidValid=reflection_liquid_frame_valid(liquid)
        && all(receiver.projection==liquid.projection) && all(receiver.extentClip==liquid.extentClip);
#endif
    const uint queryControl=queries.Load(q*48+20),queryPrimary=queries.Load(q*48),queryLobe=queries.Load(q*48+44);
    if(control.x>4 || (control.y>>control.x)!=0 || control.z>1 || control.z!=(queryPrimary==0xfffffffdU?1U:0U)
        || !all(isfinite(receiver.settings)) || receiver.settings.x<0 || receiver.settings.x>1
        || receiver.settings.y!=float(queryLobe) || queryLobe>7 || any(receiver.settings.zw!=0)
        || control.x!=(queryControl&7U) || control.y!=((queryControl>>4)&15U) || control.w!=(queryControl>>8)
        || !all(isfinite(receiver.projection)) || any(receiver.projection.xy<=0) || !all(isfinite(terminal))
        || terminal.w!=(control.w==1?1:0) || !liquidValid
        || (!control.z && !reflection_rough_frame_valid(receiver))) {if(!lane)results.Store(output+4,4);return;}
    [loop] for(uint h=0;h<control.x;++h)if((control.y&(1U<<h))==0 && !reflection_specular_plane_valid(planes[h])) {if(!lane)results.Store(output+4,4);return;}
    // Feature support coordinates are offset by .5 for the native eye ray.
    const float4 initial=asfloat(regions.Load4(input+16+r*48+16))+.5;
    const float4 extent=float4(interval_down(initial.x),interval_down(initial.y),interval_up(initial.z),interval_up(initial.w));
    if(!all(isfinite(extent)) || any(extent.xy>extent.zw)) {if(!lane)results.Store(output+4,4);return;}
    const uint limit=1U<<((opticalDepth+1)/2);
    const uint2 bins=max(1U,min(limit,uint2(ceil((extent.zw-extent.xy)*64))));const uint cells=bins.x*bins.y;
    if(cells>opticalBudget) {if(!lane)results.Store(output+4,32);return;}
    if(!lane) {keptCount=excludedCount=0;hullX0=hullY0=asuint(1.e30);hullX1=hullY1=0;}
    GroupMemoryBarrierWithGroupSync();
#if defined(STARFOX_NATIVE_SOURCE_OPTICAL)
    intervalFailed=false;const Interval3 enclosedTarget=native_target(at,terminal);
    const bool targetFailed=intervalFailed;
#endif
    // Complete closed-box coverage, evaluated in parallel. This has the same
    // bounded 1/64-pixel endpoint scale as the depth-12 adaptive cover but no
    // thousands-deep per-lane evaluation stream or private traversal stack.
    // Partition the SAME complete closed-cell set by lane modulo group size.
    // Counts and unsigned hull min/max are order-independent integer atomics;
    // every cell retains the identical checked floating-point calculation.
#if defined(STARFOX_SOURCE_OPTICAL_STREAM)
    // Aggregate ONLY integer receipts in each lane. Per-cell shared atomics
    // serialize the full cover, especially when most lava boxes are retained.
    // Counts are bounded by opticalBudget<=8192; unsigned min/max select the
    // identical existing box words, without any floating reduction/reordering.
    uint laneKept=0,laneExcluded=0;
    uint laneX0=asuint(1.e30),laneY0=asuint(1.e30),laneX1=0,laneY1=0;
#endif
    [loop] for(uint cell=lane;cell<cells;cell+=STARFOX_SOURCE_OPTICAL_LANES) {
        const uint2 ij=uint2(cell%bins.x,cell/bins.x);intervalFailed=false;
        const float4 box=float4(interval_down(split_axis(extent.x,extent.z,ij.x,bins.x)),interval_down(split_axis(extent.y,extent.w,ij.y,bins.y)),
            interval_up(split_axis(extent.x,extent.z,ij.x+1,bins.x)),interval_up(split_axis(extent.y,extent.w,ij.y+1,bins.y)));
        #if defined(STARFOX_NATIVE_SOURCE_OPTICAL)
        const bool excluded=!targetFailed && optical_excluded_target(box,receiver,liquid,planes,control,enclosedTarget,terminal.w!=0);
#else
        const bool excluded=optical_excluded(box,receiver,liquid,planes,control,terminal);
#endif
#if defined(STARFOX_SOURCE_OPTICAL_STREAM)
        if(excluded)++laneExcluded;
        else {
            ++laneKept;laneX0=min(laneX0,asuint(box.x));laneY0=min(laneY0,asuint(box.y));
            laneX1=max(laneX1,asuint(box.z));laneY1=max(laneY1,asuint(box.w));
        }
#else
        if(excluded)InterlockedAdd(excludedCount,1);
        else {InterlockedAdd(keptCount,1);InterlockedMin(hullX0,asuint(box.x));InterlockedMin(hullY0,asuint(box.y));InterlockedMax(hullX1,asuint(box.z));InterlockedMax(hullY1,asuint(box.w));}
#endif
    }
#if defined(STARFOX_SOURCE_OPTICAL_STREAM)
    if(laneExcluded)InterlockedAdd(excludedCount,laneExcluded);
    if(laneKept) {
        InterlockedAdd(keptCount,laneKept);InterlockedMin(hullX0,laneX0);InterlockedMin(hullY0,laneY0);
        InterlockedMax(hullX1,laneX1);InterlockedMax(hullY1,laneY1);
    }
#endif
    GroupMemoryBarrierWithGroupSync();
    if(!lane) {results.Store4(output,uint4(keptCount,0,cells,excludedCount));results.Store4(output+16,keptCount?uint4(hullX0,hullY0,hullX1,hullY1):uint4(0,0,0,0));}
    // Zero kept + zero status means only a no-path exclusion. Positive kept,
    // numerical uncertainty, any overflow or partial output NEVER accepts RGB.
}
