#include "starfox/render/packed_bsp.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <unordered_map>
namespace starfox::render {
PreparedBspSource::PreparedBspSource(const assets::Shape& shape,bool explosion)
    :source_(&shape),explosion_(explosion),graph_(pack_bsp(shape,explosion)) {
    normals_.reserve(graph_.faces.size());
    for(const auto& face:graph_.faces) normals_.push_back({face.normal.x,face.normal.y,face.normal.z,0});
}
std::uint64_t PreparedBspSource::storage_bytes() const noexcept {
    std::uint64_t bytes=graph_.nodes.capacity()*sizeof(PackedBspNode)
        +graph_.faces.capacity()*sizeof(assets::Face)+graph_.face_ids.capacity()*sizeof(std::uint32_t)
        +normals_.capacity()*sizeof(normals_[0]);
    for(const auto& face:graph_.faces) bytes+=face.vertex_indices.capacity();
    return bytes;
}
PackedBsp pack_bsp(const assets::Shape& shape,bool explosion) {
    PackedBsp packed;
    const auto append=[&](const std::vector<assets::Face>& faces) {
        if(faces.size()>65536 || packed.faces.size()>65536-faces.size())
            throw std::runtime_error("GPU BSP model exceeds 65536 faces");
        auto first=std::uint32_t(packed.faces.size());
        packed.faces.insert(packed.faces.end(),faces.begin(),faces.end());
        for(std::uint32_t i=0;i<faces.size();++i) packed.face_ids.push_back(first+i);
        return std::array<std::uint32_t,2>{first,std::uint32_t(faces.size())};
    };
    if(!shape.bsp_root_address || explosion) {
        const auto batch=append(shape.faces);
        packed.nodes.push_back({{UINT32_MAX,UINT32_MAX,UINT32_MAX,batch[0]},{batch[1],1,0,0}});
        packed.root=0;packed.output_capacity=batch[1];packed.work_limit=1;packed.maximum_depth=1;
        return packed;
    }
    std::unordered_map<std::uint32_t,std::array<std::uint32_t,2>> batches;
    for(const auto& batch:shape.face_batches)
        if(!batches.contains(batch.address)) batches.emplace(batch.address,append(batch.faces));
    std::unordered_map<std::uint32_t,std::uint32_t> addresses;
    const auto allocate=[&](std::uint32_t address) {
        if(!address || addresses.contains(address)) return false;
        if(packed.nodes.size()>=65536) throw std::runtime_error("GPU BSP model exceeds 65536 nodes");
        addresses.emplace(address,std::uint32_t(packed.nodes.size()));packed.nodes.push_back({});return true;
    };
    // The software traversal looks for a leaf before a node at the same address.
    for(const auto& leaf:shape.bsp_leaves) allocate(leaf.address);
    for(const auto& node:shape.bsp_nodes) allocate(node.address);
    const auto index=[&](std::uint32_t address) {
        const auto found=addresses.find(address);return found==addresses.end()?UINT32_MAX:found->second;
    };
    const auto batch=[&](std::uint32_t address) {
        const auto found=batches.find(address);
        return found==batches.end()?std::array<std::uint32_t,2>{0,0}:found->second;
    };
    std::vector<bool> initialized(packed.nodes.size());
    for(const auto& leaf:shape.bsp_leaves) {
        const auto id=index(leaf.address);if(id==UINT32_MAX || initialized[id]) continue;
        initialized[id]=true;const auto range=batch(leaf.face_batch_address);
        packed.nodes[id]={{UINT32_MAX,UINT32_MAX,UINT32_MAX,range[0]},{range[1],1,0,0}};
    }
    for(const auto& node:shape.bsp_nodes) {
        const auto id=index(node.address);if(id==UINT32_MAX || initialized[id]) continue;
        initialized[id]=true;const auto range=batch(node.face_batch_address);
        const auto visibility=node.visibility_index<shape.visibilities.size()?std::uint32_t(node.visibility_index):UINT32_MAX;
        packed.nodes[id]={{visibility,index(node.fallthrough_address),index(node.alternate_address),range[0]},{range[1],0,0,0}};
    }
    packed.root=index(shape.bsp_root_address);
    // Count both branches and every batch conservatively. This bounds either
    // visibility decision without CPU per-frame sorting. Only the active path
    // suppresses cycles; shared subtrees contribute again on each visit.
    std::vector<bool> active(packed.nodes.size());
    std::uint32_t visits=0;
    std::function<void(std::uint32_t,std::uint32_t)> walk=[&](auto id,auto depth) {
        if(++visits>262144) throw std::runtime_error("GPU BSP traversal exceeds work bound");
        if(id==UINT32_MAX || active[id]) return;
        if(depth>64) throw std::runtime_error("GPU BSP traversal exceeds depth 64");
        packed.maximum_depth=std::max(packed.maximum_depth,depth);
        active[id]=true;const auto& node=packed.nodes[id];
        if(node.batch[0]>65536-packed.output_capacity) throw std::runtime_error("GPU BSP traversal exceeds 65536 output faces");
        packed.output_capacity+=node.batch[0];
        if(!node.batch[1]) {walk(node.links[1],depth+1);walk(node.links[2],depth+1);}
        active[id]=false;
    };
    walk(packed.root,1);packed.work_limit=visits*4+1;
    return packed;
}
}
