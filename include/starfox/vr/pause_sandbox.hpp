#pragma once
#include "starfox/vr/source_models.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace starfox::vr {
// Pause-only editing. Keep simulation frozen; commit offsets once on resume.
// Bounds are captured from the CPU geometry path only when entering pause.
class PauseSandbox {
    using V=std::array<float,3>;
    struct Item {uint32_t key{};uint64_t generation{};V low{},high{},offset{};};
    struct Grab {size_t index{};float distance{};V start{},offset{};};
    std::vector<Item> items_;
    std::array<std::optional<Grab>,2> grabs_{};
    std::array<bool,2> armed_{};
    simulation::MatrixQ15 view_{};
    bool active_{};
    static V add(V a,V b) {for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
    static V point(V o,V d,float t) {for(unsigned i=0;i<3;++i)o[i]+=d[i]*t;return o;}
public:
    bool active() const noexcept {return active_;}
    void begin(const SourceModelPackets& packets,const GameSceneSnapshot& snapshot) {
        items_.clear();grabs_={};armed_={};view_=snapshot.view_matrix;active_=true;
        for(size_t i=0;i<packets.packets.size();++i) {
            const auto key=packets.handles[i];const auto& packet=packets.packets[i];
            if(!key || key>=0x10000 || packet.preserve_native_colour) continue;
            const auto found=snapshot.transforms.find(static_cast<uint16_t>(key));
            if(found==snapshot.transforms.end()) continue;
            Item item;item.key=key;item.generation=found->second.generation;
            item.low.fill(std::numeric_limits<float>::infinity());item.high.fill(-std::numeric_limits<float>::infinity());
            const auto include=[&](const auto& vertices) {for(const auto& vertex:vertices) {
                for(unsigned row=0;row<3;++row) {
                    const auto& m=packet.model;
                    float v=m[12+row];for(unsigned col=0;col<3;++col)v+=m[col*4+row]*vertex.position[col];
                    if(row==2) v-=.25F; // Same gameplay setback as application.
                    item.low[row]=std::min(item.low[row],v);item.high[row]=std::max(item.high[row],v);
                }
            }};
            include(packet.geometry.vertex_view());include(packet.geometry.line_view());
            if(!std::isfinite(item.low[0])) continue;
            for(unsigned axis=0;axis<3;++axis) {item.low[axis]-=.015F;item.high[axis]+=.015F;}
            items_.push_back(item);
        }
    }
    DrawPacket update(const std::array<std::optional<XrPosef>,2>& poses,const std::array<bool,2>& held) {
        DrawPacket pointer;pointer.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        if(!active_) return pointer;
        for(unsigned hand=0;hand<2;++hand) {
            if(!poses[hand]) {grabs_[hand].reset();armed_[hand]=false;continue;}
            const auto& pose=*poses[hand];const auto q=pose.orientation;
            const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
            V origin{pose.position.x,pose.position.y,pose.position.z};
            if(!std::isfinite(norm) || norm<.0001F || !std::all_of(origin.begin(),origin.end(),[](float v){return std::isfinite(v);})) {
                grabs_[hand].reset();armed_[hand]=false;continue;
            }
            V direction{-2*(q.x*q.z+q.w*q.y)/norm,-2*(q.y*q.z-q.w*q.x)/norm,
                -1+2*(q.x*q.x+q.y*q.y)/norm};
            float distance=128;std::optional<size_t> hit;
            for(size_t i=0;i<items_.size();++i) {
                if(grabs_[1-hand] && grabs_[1-hand]->index==i) continue;
                const auto low=add(items_[i].low,items_[i].offset),high=add(items_[i].high,items_[i].offset);
                float near=.02F,far=distance;
                for(unsigned axis=0;axis<3;++axis) {
                    if(std::abs(direction[axis])<.000001F) {if(origin[axis]<low[axis] || origin[axis]>high[axis]) far=-1;}
                    else {float a=(low[axis]-origin[axis])/direction[axis],b=(high[axis]-origin[axis])/direction[axis];
                        if(a>b) std::swap(a,b);near=std::max(near,a);far=std::min(far,b);}
                }
                if(near<=far) {distance=near;hit=i;}
            }
            if(!held[hand]) {grabs_[hand].reset();armed_[hand]=true;}
            else if(armed_[hand]) {
                armed_[hand]=false;
                if(hit) grabs_[hand]=Grab{*hit,distance,point(origin,direction,distance),items_[*hit].offset};
            }
            if(grabs_[hand]) {
                auto& grab=*grabs_[hand];distance=grab.distance;
                const auto target=point(origin,direction,distance);
                for(unsigned axis=0;axis<3;++axis)
                    items_[grab.index].offset[axis]=std::clamp(grab.offset[axis]+target[axis]-grab.start[axis],-100.F,100.F);
            }
            const auto end=point(origin,direction,distance);
            for(const auto& position:{origin,end}) {
                SceneVertex vertex{};std::copy(position.begin(),position.end(),vertex.position);
                vertex.color[0]=grabs_[hand]?1.F:0.F;vertex.color[1]=1;vertex.color[2]=grabs_[hand]?0.F:1.F;vertex.color[3]=1;
                pointer.geometry.line_vertices.push_back(vertex);
            }
        }
        return pointer;
    }
    void apply(SourceModelPackets& packets) const {
        for(size_t i=0;i<packets.packets.size();++i) for(const auto& item:items_)
            if(packets.handles[i]==item.key || packets.handles[i]==(item.key|source_shadow_pass))
                for(unsigned axis=0;axis<3;++axis) packets.packets[i].model[12+axis]+=item.offset[axis];
    }
    void commit(simulation::ObjectPool& pool) {
        if(!active_) return;
        for(const auto& item:items_) {
            const auto key=static_cast<uint16_t>(item.key);
            if(!pool.is_active(key) || pool.generation(key)!=item.generation) continue;
            const V camera_delta{item.offset[0]*256,-item.offset[1]*256,-item.offset[2]*256};
            auto& object=pool.at(key);int16_t* fields[]{&object.world_x,&object.world_y,&object.world_z};
            for(unsigned axis=0;axis<3;++axis) {
                double delta=0;for(unsigned row=0;row<3;++row)delta+=camera_delta[row]*view_[axis*3+row]/32768.;
                *fields[axis]=static_cast<int16_t>(std::clamp(std::lround(*fields[axis]+delta),-32768L,32767L));
            }
        }
        cancel();
    }
    void cancel() {active_=false;items_.clear();grabs_={};armed_={};}
};
}
