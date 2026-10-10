// Conservative resident feature index (candidate exclusion only). NOT a colour history
// or optical uniqueness certificate. Bounds/bloom collisions may add work;
// they must never delete a qualifying source. No incoming RGB is loaded.
#include "reflection_source_index_settings.hlsli"
#if defined(INDEX_BUILD)
ByteAddressBuffer source:register(t0,space0);
RWByteAddressBuffer atlas:register(u0,space1);
#elif defined(INDEX_REDUCE)
RWByteAddressBuffer atlas:register(u0,space1);
#else
ByteAddressBuffer atlas:register(t0,space0),queries:register(t1,space0);
RWByteAddressBuffer results:register(u0,space1);
#endif
uint node_at(uint level,uint2 pixel,uint lobe) {
    return (lobe*totalNodes+levels[level].x+pixel.y*levels[level].y+pixel.x)*32;
}
uint tag_hash(uint primary,uint4 mirrors,uint control,uint terminal) {
    uint h=2166136261U;h=(h^primary)*16777619U;
    [unroll] for(uint i=0;i<4;++i)h=(h^mirrors[i])*16777619U;
    h=(h^control)*16777619U;return (h^terminal)*16777619U;
}
#if defined(INDEX_BUILD)
groupshared float3 minima[64],maxima[64];
groupshared uint maskA[64],maskB[64];
[numthreads(8,8,1)]
void feature_tiles_main(uint3 group:SV_GroupID,uint3 thread:SV_GroupThreadID,uint lane:SV_GroupIndex) {
    const uint2 pixel=group.xy*8+thread.xy;const uint count=width*height;
    float3 lo=3.402823466e38,hi=-3.402823466e38;uint a=0,b=0;
    if(all(pixel<uint2(width,height)) && group.z<lobes) {
        const uint p=pixel.y*width+pixel.x,primary=source.Load(count*primaryPrefix+p*4);
        const float depth=asfloat(source.Load(count*(primaryPrefix+4)+p*4));
        const uint at=count*recordPrefix+(p*lobes+group.z)*pathStride;
        const uint4 mirrors=source.Load4(at);const uint control=source.Load(at+16),terminal=source.Load(at+20);
        const float3 feature=asfloat(source.Load3(at+24));
        if(primary!=0xffffffffU && isfinite(depth) && depth>0 && control_valid(control) && feature_valid(feature,control>>8)) {
            const float radius=(control>>8)==1?.05001:.02001;
            lo=feature-radius;hi=feature+radius;
            const uint h=tag_hash(primary,mirrors,control,terminal);a=1U<<(h&31U);b=1U<<((h>>8)&31U);
        }
    }
    minima[lane]=lo;maxima[lane]=hi;maskA[lane]=a;maskB[lane]=b;
    GroupMemoryBarrierWithGroupSync();
    [unroll] for(uint stride=32;stride>0;stride>>=1) {
        if(lane<stride) {
            minima[lane]=min(minima[lane],minima[lane+stride]);maxima[lane]=max(maxima[lane],maxima[lane+stride]);
            maskA[lane]|=maskA[lane+stride];maskB[lane]|=maskB[lane+stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if(lane==0) {
        const uint at=node_at(0,group.xy,group.z);
        atlas.Store4(at,uint4(asuint(minima[0]),maskA[0]));atlas.Store4(at+16,uint4(asuint(maxima[0]),maskB[0]));
    }
}
#elif defined(INDEX_REDUCE)
[numthreads(8,8,1)]
void feature_reduce_main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=levels[workLevel].yz) || id.z>=lobes)return;
    float3 lo=3.402823466e38,hi=-3.402823466e38;uint a=0,b=0;
    [unroll] for(uint y=0;y<2;++y)[unroll] for(uint x=0;x<2;++x) {
        const uint2 child=id.xy*2+uint2(x,y);if(any(child>=levels[workLevel-1].yz))continue;
        const uint at=node_at(workLevel-1,child,id.z);const uint4 oldLo=atlas.Load4(at),oldHi=atlas.Load4(at+16);
        lo=min(lo,asfloat(oldLo.xyz));hi=max(hi,asfloat(oldHi.xyz));a|=oldLo.w;b|=oldHi.w;
    }
    const uint at=node_at(workLevel,id.xy,id.z);
    atlas.Store4(at,uint4(asuint(lo),a));atlas.Store4(at+16,uint4(asuint(hi),b));
}
#else
[numthreads(64,1,1)]
void feature_query_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=queryCount)return;
    const uint q=id.x*48,primary=queries.Load(q);const uint4 mirrors=queries.Load4(q+4);
    const uint control=queries.Load(q+20),terminal=queries.Load(q+24),lobe=queries.Load(q+44);
    const float3 target=asfloat(queries.Load3(q+28));const uint outAt=id.x*272;
    uint count=0,status=0,visited=0;
    if(!levelCount || levelCount>12 || lobe>=lobes || leafBudget>64 || !leafBudget || !nodeBudget
        || primary==0xffffffffU || !control_valid(control) || !feature_valid(target,control>>8))status=4;
    else {
        const uint h=tag_hash(primary,mirrors,control,terminal),a=1U<<(h&31U),b=1U<<((h>>8)&31U);
        uint level=levelCount-1;uint2 position=0;bool done=false;
        [loop] while(!done) {
            if(visited>=nodeBudget) {status|=2;break;}
            ++visited;
            const uint at=node_at(level,position,lobe);const uint4 lo=atlas.Load4(at),hi=atlas.Load4(at+16);
            const bool possible=(lo.w&a)!=0 && (hi.w&b)!=0
                && all(target>=asfloat(lo.xyz)) && all(target<=asfloat(hi.xyz));
            if(possible && level>0) {--level;position*=2;continue;}
            if(possible) {
                if(count==leafBudget) {status|=1;break;}
                results.Store(outAt+16+count*4,position.y*levels[0].y+position.x);++count;
            }
            // Stackless dyadic DFS. Partial edge children are skipped; budgets
            // reject incomplete traversals instead of treating them as empty.
            bool advanced=false;
            [loop] while(level<levelCount-1) {
                const uint2 parent=position/2;const uint ordinal=(position.x&1U)+2*(position.y&1U);
                [unroll] for(uint next=1;next<=3;++next) {
                    const uint child=ordinal+next;if(child>3)break;
                    const uint2 candidate=parent*2+uint2(child&1U,child>>1);
                    if(all(candidate<levels[level].yz)) {position=candidate;advanced=true;break;}
                }
                if(advanced)break;
                ++level;position=parent;
            }
            done=!advanced;
        }
    }
    // A reusable packet must not retain leaf IDs from an earlier query in its
    // unused tail. Counts/status and every returned candidate stay unchanged;
    // consumers still refuse the entire packet on nonzero status. Integer-only
    // initialization makes generic and streamed batches byte-deterministic.
    [loop] for(uint unused=count;unused<64;++unused)results.Store(outAt+16+unused*4,0);
    // Nonzero status invalidates EVERY returned leaf, never a partial success.
    results.Store4(outAt,uint4(count,status,visited,0));
}
#endif
