#include "starfox/vr/shape_mesh.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>
namespace starfox::vr {
bool shape_face_instances(const assets::Shape& shape,std::vector<ShapeFaceInstance>& output,std::string& error) {
    std::vector<ShapeFaceInstance> pending;
    const auto fail=[&](const char* reason) {error=reason;return false;};
    if(!shape.bsp_root_address) {
        for(std::size_t i=0;i<shape.faces.size();++i) pending.push_back({i,-1});
        output=std::move(pending);error.clear();return true;
    }
    std::unordered_map<uint32_t,const assets::BspNode*> nodes;
    std::unordered_map<uint32_t,const assets::BspLeaf*> leaves;
    std::unordered_map<uint32_t,std::pair<std::size_t,std::size_t>> batches;
    for(const auto& node:shape.bsp_nodes) if(!nodes.emplace(node.address,&node).second) return fail("Duplicate BSP node");
    for(const auto& leaf:shape.bsp_leaves) if(nodes.contains(leaf.address) || !leaves.emplace(leaf.address,&leaf).second) return fail("Duplicate BSP leaf");
    std::size_t first=0;
    for(const auto& batch:shape.face_batches) {
        if(first+batch.faces.size()>shape.faces.size()) return fail("BSP face mapping overflow");
        for(std::size_t i=0;i<batch.faces.size();++i) {
            const auto& a=batch.faces[i];const auto& b=shape.faces[first+i];
            if(a.vertex_indices!=b.vertex_indices || a.visibility_index!=b.visibility_index || a.colour_id!=b.colour_id
                || a.normal!=b.normal || a.sprite!=b.sprite || a.sprite_size!=b.sprite_size
                || a.sprite_visibility_parameter!=b.sprite_visibility_parameter) return fail("BSP face mapping mismatch");
        }
        if(!batches.emplace(batch.address,std::pair{first,batch.faces.size()}).second) return fail("Duplicate BSP face batch");
        first+=batch.faces.size();
    }
    if(first!=shape.faces.size()) return fail("BSP has unmapped faces");
    const auto append=[&](uint32_t address,int visibility) {
        if(!address) return true;
        const auto found=batches.find(address);if(found==batches.end()) return fail("Missing BSP face batch");
        const auto [begin,count]=found->second;
        if(pending.size()+count>65536) return fail("BSP expansion limit exceeded");
        for(std::size_t i=0;i<count;++i) pending.push_back({begin+i,visibility});
        return true;
    };
    std::unordered_set<uint32_t> active;
    std::size_t visits=0;
    std::function<bool(uint32_t,unsigned)> visit=[&](uint32_t address,unsigned depth) {
        if(!address) return true;
        // Shared subgraphs can expand exponentially even with no face batches.
        if(++visits>65536) return fail("BSP traversal limit exceeded");
        if(depth>256 || !active.insert(address).second) return fail("Cyclic or excessive BSP depth");
        if(const auto leaf=leaves.find(address);leaf!=leaves.end()) {
            const bool result=append(leaf->second->face_batch_address,-1);active.erase(address);return result;
        }
        const auto node=nodes.find(address);if(node==nodes.end()) return fail("Missing BSP node");
        const auto& n=*node->second;
        if(!visit(n.fallthrough_address,depth+1)) return false;
        // Invalid source visibility suppresses only this node's batch, not
        // the children, matching the software renderer's default false.
        if(n.visibility_index<shape.visibilities.size()) {
            if(!append(n.face_batch_address,n.visibility_index)) return false;
        }
        if(!visit(n.alternate_address,depth+1)) return false;
        active.erase(address);return true;
    };
    if(!visit(shape.bsp_root_address,0)) return false;
    output=std::move(pending);error.clear();return true;
}
}
