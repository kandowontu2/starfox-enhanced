#include "starfox/render/packed_bsp.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace starfox;
namespace {
void require(bool ok) {if(!ok) throw std::runtime_error("Packed BSP assertion failed");}
bool equal(const assets::Face& a,const assets::Face& b) {
    return a.visibility_index==b.visibility_index && a.colour_id==b.colour_id && a.normal==b.normal
        && a.vertex_indices==b.vertex_indices && a.sprite==b.sprite
        && a.sprite_visibility_parameter==b.sprite_visibility_parameter && a.sprite_size==b.sprite_size;
}
void check(const assets::Shape& shape,bool explosion=false) {
    const auto packed=render::pack_bsp(shape,explosion);
    for(unsigned pattern=0;pattern<16;++pattern) {
        std::vector<const assets::Face*> source,actual;
        std::set<std::uint32_t> active;
        const auto visible=[&](std::uint32_t index) {
            return index<shape.visibilities.size() && (pattern==1 || (pattern>1 && ((index*1664525U+pattern*1013904223U)>>(pattern+8))&1));
        };
        const auto batch=[&](std::uint32_t address) {
            for(const auto& b:shape.face_batches) if(b.address==address) {
                for(const auto& face:b.faces) source.push_back(&face);return;
            }
        };
        std::function<void(std::uint32_t)> walk=[&](auto address) {
            if(!address || !active.insert(address).second) return;
            auto leaf=std::find_if(shape.bsp_leaves.begin(),shape.bsp_leaves.end(),[&](const auto& n){return n.address==address;});
            auto node=std::find_if(shape.bsp_nodes.begin(),shape.bsp_nodes.end(),[&](const auto& n){return n.address==address;});
            if(leaf!=shape.bsp_leaves.end()) batch(leaf->face_batch_address);
            else if(node!=shape.bsp_nodes.end()) {
                if(visible(node->visibility_index)) {walk(node->fallthrough_address);batch(node->face_batch_address);walk(node->alternate_address);}
                else {walk(node->alternate_address);walk(node->fallthrough_address);}
            }
            active.erase(address);
        };
        if(!shape.bsp_root_address || explosion) for(const auto& face:shape.faces) source.push_back(&face);
        else walk(shape.bsp_root_address);
        active.clear();
        std::function<void(std::uint32_t)> walkPacked=[&](auto id) {
            if(id==UINT32_MAX || !active.insert(id).second) return;
            const auto& node=packed.nodes.at(id);
            const auto append=[&] {for(unsigned f=0;f<node.batch[0];++f)
                actual.push_back(&packed.faces.at(packed.face_ids.at(node.links[3]+f)));};
            if(node.batch[1]) append();
            else if(visible(node.links[0])) {walkPacked(node.links[1]);append();walkPacked(node.links[2]);}
            else {walkPacked(node.links[2]);walkPacked(node.links[1]);}
            active.erase(id);
        };
        walkPacked(packed.root);require(source.size()==actual.size());require(actual.size()<=packed.output_capacity);
        for(unsigned i=0;i<source.size();++i) require(equal(*source[i],*actual[i]));
    }
}
}
int main(int argc,char** argv)try {
    assets::Shape shape;shape.visibilities.resize(2);shape.bsp_root_address=10;
    for(unsigned i=1;i<=4;++i) {assets::Face face;face.colour_id=std::uint8_t(i);shape.face_batches.push_back({i,{face}});shape.faces.push_back(face);}
    shape.bsp_nodes={{10,0,1,20,30},{20,1,2,30,40}};
    shape.bsp_leaves={{30,3},{40,4}};
    check(shape);check(shape,true);
    // Duplicate-address first entries win; leaves take precedence over nodes.
    shape.bsp_nodes.push_back({10,1,4,0,0});shape.bsp_nodes.push_back({30,1,4,0,0});
    shape.bsp_leaves.push_back({30,4});shape.face_batches.push_back({3,{}});check(shape);
    shape.bsp_nodes[1].fallthrough_address=10;check(shape); // Active-path cycle.
    shape.bsp_nodes[1].alternate_address=999;shape.bsp_nodes[1].face_batch_address=999;check(shape);
    shape.bsp_nodes[0].visibility_index=255;check(shape);
    shape.bsp_root_address=999;check(shape);
    shape.bsp_root_address=0;check(shape);
    assets::Shape deep;deep.bsp_root_address=1;
    for(unsigned i=1;i<=65;++i) deep.bsp_nodes.push_back({i,0,0,i+1,0});
    bool threw=false;try {static_cast<void>(render::pack_bsp(deep));}catch(const std::runtime_error&){threw=true;}require(threw);
    deep.bsp_nodes.pop_back();check(deep);
    unsigned models=0;
    if(argc==3) {
        const auto rom=assets::RomImage::load(argv[1]);const auto symbols=assets::SymbolMap::load(argv[2]);
        const assets::ShapeDecoder decoder(rom,symbols);std::set<std::uint32_t> seen;
        for(const auto& [name,addresses]:symbols.entries()) for(auto address:addresses)
            if(seen.insert(address).second && decoder.looks_like_shape_header(address)) {
                const auto model=decoder.decode(address,name);check(model);check(model,true);++models;
            }
    }else require(argc==1);
    std::cout<<"Packed BSP source-order parity passed; "<<models<<" decoded models, 16 visibility patterns and explosion order each\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
