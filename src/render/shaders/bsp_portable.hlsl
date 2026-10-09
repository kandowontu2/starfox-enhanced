// Source painter traversal. One lane owns each model's output segment.
// Nodes: (visibility, fallthrough, alternate, firstFace),
//        (faceCount, leaf, reserved, reserved). Missing links use UINT_MAX.
// Trees: (root, outputFirst, outputCapacity, workLimit).
// Face IDs are already flattened in source batch order. Visibility is produced
// by the word or continuous visibility stage; no camera readback is required.
struct Node { uint4 links; uint4 batch; };
[[vk::binding(0,0)]] StructuredBuffer<Node> nodes : register(t0, space0);
[[vk::binding(1,0)]] StructuredBuffer<uint> visibility : register(t1, space0);
[[vk::binding(2,0)]] StructuredBuffer<uint> faces : register(t2, space0);
[[vk::binding(3,0)]] StructuredBuffer<uint4> trees : register(t3, space0);
[[vk::binding(0,1)]] RWStructuredBuffer<uint> ordered : register(u0, space1);
[[vk::binding(1,1)]] RWStructuredBuffer<uint2> results : register(u1, space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0, space2) {
    uint treeCount, nodeCount, visibilityCount, faceCount;
    uint outputCount; uint3 padding;
};
// Status: 0 success, 1 invalid range, 2 depth, 3 capacity, 4 work limit.
// Failed trees publish count zero: partial output must never be rendered.
[numthreads(32,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=treeCount) return;
    uint4 tree=trees[id.x];
    if(tree.y>outputCount || tree.z>outputCount-tree.y) {
        results[id.x]=uint2(0,1); return;
    }
    uint addresses[64], phases[64];
    uint depth=0, written=0, work=0, status=0, next=tree.x;
    // Explicit recursion stack retains active addresses until both branches
    // return, matching the CPU's active-path set rather than a global visited
    // set. Shared subtrees can legitimately be visited more than once.
    while(next!=0xffffffffU || depth!=0) {
        if(work>=tree.w) {status=4;break;}
        ++work;
        if(next!=0xffffffffU) {
            bool active=false;
            for(uint i=0;i<depth;++i) active=active || addresses[i]==next;
            uint address=next; next=0xffffffffU;
            if(active || address>=nodeCount) continue;
            if(depth==64) {status=2;break;}
            addresses[depth]=address; phases[depth]=0; ++depth;
        }
        if(depth==0) continue;
        uint top=depth-1;
        Node node=nodes[addresses[top]];
        bool leaf=node.batch.y!=0;
        bool visible=false;
        if(node.links.x<visibilityCount) visible=visibility[node.links.x]!=0;
        uint phase=phases[top];
        if(phase==0 && !leaf) {
            phases[top]=1;
            next=visible?node.links.y:node.links.z;
            continue;
        }
        if(phase<=1) {
            phases[top]=2;
            if(leaf || visible) {
                uint first=node.links.w, count=node.batch.x;
                if(first>faceCount || count>faceCount-first) {status=1;break;}
                if(count>tree.z-written) {status=3;break;}
                for(uint f=0;f<count;++f) ordered[tree.y+written++]=faces[first+f];
            }
            if(!leaf) {
                next=visible?node.links.z:node.links.y;
                continue;
            }
        }
        --depth;
    }
    results[id.x]=uint2(status==0?written:0,status);
}
