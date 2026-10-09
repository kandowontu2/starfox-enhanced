// Ordered source material generation. Output is indexed by traversal slot,
// not face ID: shared BSP subtrees may emit the same face more than once.
StructuredBuffer<uint> ordered : register(t0,space0);
StructuredBuffer<uint2> traversal : register(t1,space0);
StructuredBuffer<uint4> polygons : register(t2,space0);
StructuredBuffer<uint> visibility : register(t3,space0);
RWStructuredBuffer<uint> descriptors : register(u0,space1);
RWStructuredBuffer<uint2> result : register(u1,space1);
cbuffer Settings : register(b0,space2) {
    uint capacity,polygonCount,visibilityCount,seed;
};
uint nextWord(inout uint state,inout uint carry) {
    uint word=0;
    for(uint hop=0;hop<32;++hop) {
        uint swapped=((state<<8)|(state>>8))&65535U;
        uint rotated=(carry<<15)|(swapped>>1);
        uint first=rotated+state;
        uint second=(first&65535U)+state+uint(first>65535U);
        carry=uint(second>65535U);
        state=(second+1U)&65535U;
        word=state;
        if((word&49152U)!=32768U) return word;
    }
    return word&~32768U;
}
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id!=0)) return;
    result[0]=uint2(0,1);
    // Later resident stages dispatch capacity threads, not a CPU-read count.
    // Clear the unused tail and all failure paths before validating traversal.
    for(uint i=0;i<capacity;++i) descriptors[i]=0xffffffffU;
    uint2 list=traversal[0];
    if(list.y!=0 || list.x>capacity) return;
    // Validate before emitting: callers must never consume partial output.
    for(uint i=0;i<list.x;++i) if(ordered[i]>=polygonCount) return;
    uint state=seed&65535U,carry=0;
    for(uint i=0;i<list.x;++i) {
        uint face=ordered[i],v=polygons[face].z;
        // Destruction consumes a word for every source face, before sprite
        // centre visibility is tested. Bit 31 is a mode bit, not seed state.
        bool visible=(seed&0x80000000U)!=0 || (v<visibilityCount && visibility[v]!=0);
        uint descriptor=0xffffffffU;
        if(visible) descriptor=nextWord(state,carry);
        descriptors[i]=descriptor;
    }
    result[0]=uint2(list.x,0);
}
