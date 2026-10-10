// Refine resident candidate leaves into necessary source supports on the GPU.
// This is NOT an optical-root/uniqueness certificate or colour reuse. Inputs
// are the native old ownership/depth/path/features and the GPU leaf result.
#include "reflection_source_index_settings.hlsli"
#define STARFOX_CURVED_SCALAR_ONLY 1
#include "reflection_curved_precision.hlsli"
cbuffer DomainSettings:register(b1,space2) {
    uint regionBudget,supportBudget,regionCapacity,domainUnused;
};
ByteAddressBuffer source:register(t0,space0),queries:register(t1,space0),leaves:register(t2,space0);
RWByteAddressBuffer regions:register(u0,space1);
static const uint capacity=128,stride=16+capacity*48;
struct Query {uint primary;uint4 mirrors;uint control,terminal,lobe;float3 target;};
struct Entry {float3 feature;bool same,visible;};
struct Point {float2 x,y;};
Point domain_point(float2 x,float2 y) {Point p;p.x=x;p.y=y;return p;}
Entry entry(int2 pixel,Query q) {
    Entry e;e.feature=0;e.same=e.visible=false;
    if(any(pixel<0) || any(pixel>=int2(width,height)))return e;
    const uint p=uint(pixel.y)*width+uint(pixel.x),count=width*height;
    if(source.Load(count*primaryPrefix+p*4)!=q.primary)return e;
    const uint at=count*recordPrefix+(p*lobes+q.lobe)*pathStride;
    if(any(source.Load4(at)!=q.mirrors) || source.Load(at+16)!=q.control || source.Load(at+20)!=q.terminal)return e;
    e.feature=asfloat(source.Load3(at+24));e.same=feature_valid(e.feature,q.control>>8);
    const float depth=asfloat(source.Load(count*(primaryPrefix+4)+p*4));
    e.visible=e.same && isfinite(depth) && depth>0;return e;
}
float2 plane_value(Point p,float2 a,float2 b,float2 bound) {
    return curved_dd_sub(curved_dd_add(curved_dd_mul(a,p.x),curved_dd_mul(b,p.y)),bound);
}
float lower_float(float2 value) {
    // All encoded coordinates are nonnegative. Directed binary32 enclosure
    // prevents storage rounding from cutting away an independently valid root.
    if(value.x<=0)return 0;
    return value.y<0?asfloat(asuint(value.x)-1):value.x;
}
float upper_float(float2 value) {
    return value.y>0?asfloat(asuint(value.x)+1):value.x;
}
// Return 0 for no necessary region, 1 for an enclosure, -1 for arithmetic or
// bounded polygon storage failure (which invalidates the entire query).
int support(uint2 pixel,uint2 span,Query q,out float4 bounds,out float2 initializer) {
    bounds=0;initializer=0;
    Point polygon[16],scratch[16];uint size=0;
    polygon[size++]=domain_point(0,0);
    if(span.x)polygon[size++]=domain_point(float2(1,0),0);
    if(span.y) {polygon[size++]=domain_point(float2(span.x,0),float2(1,0));if(span.x)polygon[size++]=domain_point(0,float2(1,0));}
    const uint kind=q.control>>8;
    // Arithmetic/query enclosures only. Old-eye target upload rounds to
    // binary32; finite barycentric targets already have that exact encoding.
    // No source/optical/colour acceptance tolerance is altered.
    const float2 outward=float2(kind==1?1.e-10:2.e-7,0);
    [loop] for(uint dy=0;dy<=span.y;++dy)[loop] for(uint dx=0;dx<=span.x;++dx) {
        const int2 tapPixel=int2(pixel+uint2(dx,dy));const Entry tap=entry(tapPixel,q);
        if(!tap.visible)return 0;
        float2 gradientX[3],gradientY[3];
        [unroll] for(uint c=0;c<3;++c)gradientX[c]=gradientY[c]=0;
        [unroll] for(uint axis=0;axis<2;++axis)[unroll] for(int side=-1;side<=1;side+=2) {
            const Entry neighbour=entry(tapPixel+int2(axis==0?side:0,axis==1?side:0),q);
            // Noncontributing neighbour depth intentionally does NOT gate the
            // feature gradient, matching the independent source guard.
            if(neighbour.same)[unroll] for(uint c=0;c<3;++c) {
                const float2 delta=curved_dd_mul(curved_dd_abs(curved_dd_sub(float2(neighbour.feature[c],0),float2(tap.feature[c],0))),float2(1.5,0));
                if(axis==0)gradientX[c]=curved_dd_max(gradientX[c],delta);else gradientY[c]=curved_dd_max(gradientY[c],delta);
            }
        }
        [loop] for(uint c=0;c<3;++c) {
            const float2 difference=curved_dd_abs(curved_dd_sub(float2(tap.feature[c],0),float2(q.target[c],0)));
            const float2 cap=curved_dd_add(curved_dd_ratio(1,kind==1?20:50),outward);
            if(curved_dd_less(cap,difference))return 0;
            const float2 a=span.x?(dx?-gradientX[c]:gradientX[c]):0,b=span.y?(dy?-gradientY[c]:gradientY[c]):0;
            float2 bound=curved_dd_sub(difference,curved_dd_ratio(2,1000000));
            bound=curved_dd_sub(curved_dd_sub(curved_dd_sub(bound,curved_dd_mul(gradientX[c],float2(dx,0))),
                curved_dd_mul(gradientY[c],float2(dy,0))),outward);
            uint next=0;
            [loop] for(uint n=0;n<size;++n) {
                const Point first=polygon[n],last=polygon[(n+1)%size];
                const float2 vfirst=plane_value(first,a,b,bound),vlast=plane_value(last,a,b,bound);
                if(!all(isfinite(vfirst)) || !all(isfinite(vlast)))return -2;
                const bool insideFirst=!curved_dd_less(vfirst,0),insideLast=!curved_dd_less(vlast,0);
                if(insideFirst) {if(next==16)return -1;scratch[next++]=first;}
                if(insideFirst!=insideLast) {
                    float2 t=curved_dd_div(vfirst,curved_dd_sub(vfirst,vlast));
                    if(!all(isfinite(t)))return -3;
                    // Opposite half-plane signs imply the exact fraction lies
                    // in [0,1]. Compensated division can round just beyond an
                    // endpoint; match the independent clipper's endpoint clamp
                    // only inside the existing local query enclosure. Larger
                    // excursions still invalidate the entire candidate query.
                    if(curved_dd_less(t,float2(-1.e-10,0)))return -4;
                    if(curved_dd_less(float2(1,1.e-10),t))return -5;
                    t=curved_dd_max(0,curved_dd_min(float2(1,0),t));
                    if(next==16)return -1;
                    scratch[next++]=domain_point(curved_dd_add(first.x,curved_dd_mul(t,curved_dd_sub(last.x,first.x))),
                        curved_dd_add(first.y,curved_dd_mul(t,curved_dd_sub(last.y,first.y))));
                }
            }
            if(!next)return 0;
            size=next;[loop] for(uint n=0;n<size;++n)polygon[n]=scratch[n];
        }
    }
    float2 minX=float2(span.x,0),minY=float2(span.y,0),maxX=0,maxY=0,sumX=0,sumY=0;
    [loop] for(uint n=0;n<size;++n) {
        minX=curved_dd_min(minX,polygon[n].x);minY=curved_dd_min(minY,polygon[n].y);
        maxX=curved_dd_max(maxX,polygon[n].x);maxY=curved_dd_max(maxY,polygon[n].y);
        sumX=curved_dd_add(sumX,polygon[n].x);sumY=curved_dd_add(sumY,polygon[n].y);
    }
    const float2 padding=float2(1.e-10,0);
    minX=curved_dd_max(0,curved_dd_sub(minX,padding));minY=curved_dd_max(0,curved_dd_sub(minY,padding));
    maxX=curved_dd_min(float2(span.x,0),curved_dd_add(maxX,padding));maxY=curved_dd_min(float2(span.y,0),curved_dd_add(maxY,padding));
    bounds=float4(lower_float(curved_dd_add(float2(pixel.x,0),minX)),lower_float(curved_dd_add(float2(pixel.y,0),minY)),
        upper_float(curved_dd_add(float2(pixel.x,0),maxX)),upper_float(curved_dd_add(float2(pixel.y,0),maxY)));
    initializer=float2(curved_dd_float(curved_dd_add(float2(pixel.x+.5,0),curved_dd_div(sumX,float2(size,0)))),
        curved_dd_float(curved_dd_add(float2(pixel.y+.5,0),curved_dd_div(sumY,float2(size,0)))));
    return all(isfinite(bounds)) && all(isfinite(initializer))?1:-6;
}
groupshared uint regionCount,regionStatus,workCount,errorBits;
[numthreads(8,8,1)]
void feature_domains_main(uint3 group:SV_GroupID,uint3 thread:SV_GroupThreadID,uint lane:SV_GroupIndex) {
    if(group.x>=queryCount)return;
    const uint resultAt=group.x*272,outputAt=group.x*stride,qAt=group.x*48;
    const uint leafCount=leaves.Load(resultAt);
    if(lane==0) {
        regionCount=workCount=errorBits=0;regionStatus=leaves.Load(resultAt+4);
        if(regionCapacity!=capacity || !regionBudget || regionBudget>capacity || !supportBudget || supportBudget>16384 || leafCount>64)
            regionStatus|=4;
    }
    GroupMemoryBarrierWithGroupSync();
    Query q;q.primary=queries.Load(qAt);q.mirrors=queries.Load4(qAt+4);q.control=queries.Load(qAt+20);
    q.terminal=queries.Load(qAt+24);q.target=asfloat(queries.Load3(qAt+28));q.lobe=queries.Load(qAt+44);
    [loop] for(uint leaf=0;leaf<leafCount;++leaf) {
        if(regionStatus)break;
        const uint tile=leaves.Load(resultAt+16+leaf*4);
        if(lane==0 && tile>=levels[0].y*levels[0].z)InterlockedOr(regionStatus,128);
        GroupMemoryBarrierWithGroupSync();if(regionStatus)break;
        const uint2 pixel=uint2(tile%levels[0].y,tile/levels[0].y)*8+thread.xy;
        const Entry anchor=entry(int2(pixel),q);
        // Every support anchored here necessarily obeys this per-tap cap.
        // A cheap outward-rounded test prevents expensive polygon clipping
        // for unrelated features inside a conservative/bloom-positive leaf.
        // It does not replace the tighter compensated half-plane clipping.
        const float coarseCap=(q.control>>8)==1?.050001:.020001;
        if(all(pixel<uint2(width,height)) && anchor.visible && all(abs(anchor.feature-q.target)<=coarseCap)) {
            [unroll] for(uint sy=0;sy<2;++sy)[unroll] for(uint sx=0;sx<2;++sx) {
                if(any(pixel+uint2(sx,sy)>=uint2(width,height)))continue;
                uint work;InterlockedAdd(workCount,1,work);
                if(work>=supportBudget) {InterlockedOr(regionStatus,32);continue;}
                float4 bounds;float2 initializer;const int outcome=support(pixel,uint2(sx,sy),q,bounds,initializer);
                if(outcome<0) {InterlockedOr(regionStatus,64);InterlockedOr(errorBits,1U<<uint(-outcome-1));continue;}
                if(!outcome)continue;
                uint at;InterlockedAdd(regionCount,1,at);
                if(at>=regionBudget) {InterlockedOr(regionStatus,16);continue;}
                at=outputAt+16+at*48;
                regions.Store4(at,uint4(pixel,sx,sy));regions.Store4(at+16,asuint(bounds));
                regions.Store4(at+32,uint4(asuint(initializer),0,0));
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    const uint emitted=min(regionCount,regionBudget);
    // This reusable packet is copied before the next batch overwrites it.
    // Initialize its entire inactive tail even for empty/upstream-refused
    // queries; old support coordinates are not part of the current cover.
    // Active records, their atomic order and all refusal/count words stay
    // unchanged. Each lane clears disjoint records beyond the emitted count.
    [loop] for(uint unused=emitted+lane;unused<capacity;unused+=64) {
        const uint at=outputAt+16+unused*48;
        regions.Store4(at,0);regions.Store4(at+16,0);regions.Store4(at+32,0);
    }
    if(lane==0)regions.Store4(outputAt,uint4(emitted,regionStatus,min(workCount,supportBudget),errorBits));
    // ANY nonzero status makes all emitted regions unusable, including budget
    // and arithmetic failures. No branch or old RGB is accepted here.
}
