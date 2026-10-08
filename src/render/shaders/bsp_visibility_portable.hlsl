// One model per group. Parallel face tests precede the authored painter walk
// inside one dispatch; all lanes reach the storage barrier before lane zero
// consumes visibility. Shared recursion state avoids per-lane stack spilling.
#include "source_visibility.hlsli"
struct Node {uint4 links;uint4 batch;};
[[vk::binding(0,0)]] StructuredBuffer<Node> nodes : register(t0,space0);
[[vk::binding(1,0)]] ByteAddressBuffer points : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<uint4> visibilityFaces : register(t2,space0);
[[vk::binding(3,0)]] StructuredBuffer<uint> faces : register(t3,space0);
[[vk::binding(4,0)]] StructuredBuffer<uint4> trees : register(t4,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<uint> visibility : register(u0,space1);
[[vk::binding(1,1)]] RWStructuredBuffer<uint> ordered : register(u1,space1);
[[vk::binding(2,1)]] RWStructuredBuffer<uint2> results : register(u2,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint treeCount,nodeCount,visibilityCount,faceCount;
    uint outputCount,pointCount,continuous,padding;
};
groupshared uint addresses[64],phases[64];
uint testFace(uint i) {
    uint4 face=visibilityFaces[i];
    if(face.w==1U) return 1;
    if(face.w==3U) {
        if(face.x>=pointCount) return 0;
        uint bits=points.Load(face.x*(continuous!=0?32U:16U)+(continuous!=0?28U:12U));
        return continuous!=0?uint(asfloat(bits)>0):uint(asint(bits)>0);
    }
    if(any(face.xyz>=pointCount)) return 0;
    uint stride=continuous!=0?32U:16U;
    uint4 a=points.Load4(face.x*stride),b=points.Load4(face.y*stride),c=points.Load4(face.z*stride);
    [branch] if(continuous!=0) return sourceContinuousVisibility(asfloat(a),asfloat(b),asfloat(c),face.w);
    return sourceWordVisibility(asint(a),asint(b),asint(c));
}
[numthreads(64,1,1)]
void main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    for(uint i=lane;i<visibilityCount;i+=64) visibility[i]=testFace(i);
    DeviceMemoryBarrierWithGroupSync();
    if(lane!=0) return;
    // The host only admits a single tree; no global inter-group dependency.
    uint4 tree=trees[0];
    if(tree.y>outputCount || tree.z>outputCount-tree.y) {results[0]=uint2(0,1);return;}
    uint depth=0,written=0,work=0,status=0,next=tree.x;
    while(next!=0xffffffffU || depth!=0) {
        if(work>=tree.w) {status=4;break;}
        ++work;
        if(next!=0xffffffffU) {
            bool active=false;
            for(uint i=0;i<depth;++i) active=active || addresses[i]==next;
            uint address=next;next=0xffffffffU;
            if(active || address>=nodeCount) continue;
            if(depth==64) {status=2;break;}
            addresses[depth]=address;phases[depth]=0;++depth;
        }
        if(depth==0) continue;
        uint top=depth-1;
        Node node=nodes[addresses[top]];
        bool leaf=node.batch.y!=0;
        bool visible=false;
        if(node.links.x<visibilityCount) visible=visibility[node.links.x]!=0;
        uint phase=phases[top];
        if(phase==0 && !leaf) {
            phases[top]=1;next=visible?node.links.y:node.links.z;continue;
        }
        if(phase<=1) {
            phases[top]=2;
            if(leaf || visible) {
                uint first=node.links.w,count=node.batch.x;
                if(first>faceCount || count>faceCount-first) {status=1;break;}
                if(count>tree.z-written) {status=3;break;}
                for(uint f=0;f<count;++f) ordered[tree.y+written++]=faces[first+f];
            }
            if(!leaf) {next=visible?node.links.z:node.links.y;continue;}
        }
        --depth;
    }
    results[0]=uint2(status==0?written:0,status);
}
