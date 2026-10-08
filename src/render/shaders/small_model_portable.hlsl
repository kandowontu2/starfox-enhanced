// Bounded one-model group: exact continuous projection, source face visibility
// and authored painter traversal. No cross-group storage dependency, screen
// result sharing between eyes, CPU projection or reduced source geometry.
#define SF_CONTINUOUS_SMALL_MODEL
#include "continuous_portable.hlsl"
#include "source_visibility.hlsli"
struct Node {uint4 links;uint4 batch;};
[[vk::binding(2,0)]] StructuredBuffer<Node> nodes : register(t2,space0);
[[vk::binding(3,0)]] StructuredBuffer<uint4> visibilityFaces : register(t3,space0);
[[vk::binding(4,0)]] StructuredBuffer<uint> faces : register(t4,space0);
[[vk::binding(5,0)]] StructuredBuffer<uint4> trees : register(t5,space0);
[[vk::binding(2,1)]] RWStructuredBuffer<uint> visibility : register(u2,space1);
[[vk::binding(3,1)]] RWStructuredBuffer<uint> ordered : register(u3,space1);
[[vk::binding(4,1)]] RWStructuredBuffer<uint2> results : register(u4,space1);
groupshared uint addresses[64],phases[64];
uint testFace(uint i) {
    uint4 face=visibilityFaces[i];
    if(face.w==1U) return 1;
    if(face.w==3U) return face.x<count?uint(outputPoints[face.x].screen.w>0):0;
    if(any(face.xyz>=count)) return 0;
    return sourceContinuousVisibility(outputPoints[face.x].camera,
        outputPoints[face.y].camera,outputPoints[face.z].camera,face.w);
}
[numthreads(64,1,1)]
void main(uint lane:SV_GroupIndex) {
    // The host admits <=128 points/faces, exactly one tree and one group.
    // Even inactive lanes must reach both barriers.
    for(uint i=lane;i<count;i+=64) transformVertex(i);
    DeviceMemoryBarrierWithGroupSync();
    for(uint i=lane;i<visibilityCount;i+=64) visibility[i]=testFace(i);
    DeviceMemoryBarrierWithGroupSync();
    if(lane!=0) return;
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
