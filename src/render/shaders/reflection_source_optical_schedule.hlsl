// Integer-only resident compaction. No support, guard, optical box or colour
// is invented or removed: upstream refusals and all unused result slots retain
// precisely the generic writer's default words. The following separate pass
// executes the unchanged optical writer for every potentially active region.
cbuffer ScheduleSettings:register(b0,space2) {uint queryCount,reserved0,reserved1,reserved2;};
ByteAddressBuffer regions:register(t0,space0);
RWByteAddressBuffer results:register(u0,space1),workMap:register(u1,space1),dispatchArgs:register(u2,space1);
groupshared uint workBase,workCount;
static const uint capacity=128,regionStride=16+capacity*48,resultStride=capacity*32;
[numthreads(128,1,1)]
void feature_optical_schedule_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    const uint q=group.x;
    if(!queryCount || queryCount>64 || q>=queryCount || group.y || group.z
        || reserved0 || reserved1 || reserved2)return;
    const uint count=regions.Load(q*regionStride),status=regions.Load(q*regionStride+4);
    const uint output=q*resultStride+lane*32;
    results.Store4(output,uint4(0,status,0,0));results.Store4(output+16,0);
    if(!lane) {
        uint base=0;
        [loop] for(uint previous=0;previous<q;++previous) {
            const uint at=previous*regionStride;
            base+=regions.Load(at+4)?0:min(regions.Load(at),capacity);
        }
        workBase=base;workCount=status?0:min(count,capacity);
        if(q+1==queryCount)dispatchArgs.Store3(0,uint3(base+workCount,1,1));
    }
    GroupMemoryBarrierWithGroupSync();
    if(lane<workCount)workMap.Store2((workBase+lane)*8,uint2(q,lane));
}
