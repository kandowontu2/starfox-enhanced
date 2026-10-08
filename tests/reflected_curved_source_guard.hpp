#pragma once
// Test-only old optical eligibility. Ownership/path features are inputs;
// neither incoming RGB, downloaded inverse guides nor renderer output enters.
#include "reflected_curved_owner_oracle.hpp"
#include <cstdint>

namespace reflected_curved_owner_oracle {
struct SourceTap {unsigned identity{};double depth{};std::array<unsigned,9> path{};};
template<class Read>
inline std::vector<std::array<double,2>> old_visibility_initializers(const Inputs& i,unsigned primary,
    std::span<const unsigned,16> record,Read read) {
    const auto& f=i.liquid;
    if(f.width==0 || f.height==0 || f.width>65536 || f.height>65536
        || std::uint64_t(f.width)*f.height>65536)throw std::runtime_error("Unbounded independent old visibility bank");
    unsigned old_primary=primary;auto matched=std::array<unsigned,6>{};std::copy_n(record.begin(),6,matched.begin());
    const auto mapped=[&](unsigned id)->std::optional<unsigned> {
        if(id>=i.mapping.size() || i.mapping[id]>=i.previous.size())return {};
        return i.mapping[id];
    };
    if(primary!=0xfffffffdU) {const auto id=mapped(primary);if(!id)return {};old_primary=*id;}
    const unsigned hops=record[4]&7U,mask=(record[4]>>4)&15U,kind=record[4]>>8;
    if(hops>4 || kind<1 || kind>3 || (mask>>hops)!=0)return {};
    for(unsigned h=0;h<hops;++h)if(!(mask&(1U<<h))) {
        const auto id=mapped(record[h]);if(!id)return {};matched[h]=*id;
    }
    if(kind==1) {const auto id=mapped(record[5]);if(!id)return {};matched[5]=*id;}
    std::vector<std::array<double,2>> starts;
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x) {
        const auto tap=read(x,y);
        if(!tap || tap->identity!=old_primary || !std::isfinite(tap->depth) || tap->depth<=0
            || !std::equal(matched.begin(),matched.end(),tap->path.begin()))continue;
        // Eligibility/initialization uses ownership, depth and ordered path
        // tags only. The full forward solve and source guard qualify the root;
        // neither old RGB nor a downloaded inverse enters these coordinates.
        starts.push_back({x+.5,y+.5});
    }
    return starts;
}
enum class SourceGuard {accepted,bounds,path,depth,feature,cell_fold,ring_fold,forward,subcell_fold};
template<class Read>
inline SourceGuard source_guard(const Inputs& i,unsigned primary,std::span<const unsigned,16> record,
    double roughness,unsigned lobe,const Solution& root,Read read) {
    const auto& f=i.liquid;
    if(!root.valid || root.x<0 || root.y<0 || root.x>f.width-1 || root.y>f.height-1)return SourceGuard::bounds;
    Path p;p.hops=record[4]&7U;p.liquid_mask=(record[4]>>4)&15U;p.lobe=lobe;p.roughness=roughness;
    const unsigned kind=record[4]>>8;
    if(p.hops>4 || kind<1 || kind>3 || lobe>=8)return SourceGuard::path;
    p.liquid_primary=primary==0xfffffffdU;p.finite_terminal=kind==1;
    auto matched=std::array<unsigned,9>{};std::copy_n(record.begin(),9,matched.begin());
    const auto mapped=[&](unsigned id)->std::optional<unsigned> {
        if(id>=i.mapping.size() || i.mapping[id]>=i.previous.size())return {};
        return i.mapping[id];
    };
    unsigned old_primary=primary;
    if(!p.liquid_primary) {
        const auto id=mapped(primary);if(!id)return SourceGuard::path;
        old_primary=*id;p.receiver=i.previous[*id];
    }
    for(unsigned h=0;h<p.hops;++h)if(!(p.liquid_mask&(1U<<h))) {
        const auto id=mapped(record[h]);if(!id)return SourceGuard::path;
        matched[h]=*id;p.mirrors[h]=i.previous[*id];
    }
    V target;for(unsigned c=0;c<3;++c)target[c]=std::bit_cast<float>(record[6+c]);
    if(!valid_feature(target,kind))return SourceGuard::feature;
    if(p.finite_terminal) {
        const auto id=mapped(record[5]);if(!id)return SourceGuard::path;
        matched[5]=*id;p.terminal_plane=i.previous[*id];
    } else target=old_environment_direction(i,target);
    const auto same=[&](int x,int y)->std::optional<SourceTap> {
        if(x<0 || y<0 || x>=int(f.width) || y>=int(f.height))return {};
        const auto tap=read(unsigned(x),unsigned(y));
        if(!tap || tap->identity!=old_primary
            || !std::equal(matched.begin(),matched.begin()+6,tap->path.begin()))return {};
        if(!valid_feature({std::bit_cast<float>(tap->path[6]),std::bit_cast<float>(tap->path[7]),
            std::bit_cast<float>(tap->path[8])},kind))return {};
        return tap;
    };
    const auto feature=[](const SourceTap& tap) {
        V value;for(unsigned c=0;c<3;++c)value[c]=std::bit_cast<float>(tap.path[6+c]);return value;
    };
    const int bx=int(std::floor(root.x)),by=int(std::floor(root.y));
    const double fx=root.x-bx,fy=root.y-by;double reciprocal=0;
    for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
        const double share=(dx?fx:1-fx)*(dy?fy:1-fy);if(share<=0)continue;
        const int tx=std::min(bx+int(dx),int(f.width)-1),ty=std::min(by+int(dy),int(f.height)-1);
        const auto own=same(tx,ty);if(!own)return SourceGuard::path;
        if(!std::isfinite(own->depth) || own->depth<=0)return SourceGuard::depth;
        reciprocal+=share/own->depth;
        const auto value=feature(*own);V footprint{2.e-6,2.e-6,2.e-6};
        for(unsigned axis=0;axis<2;++axis) {
            V gradient{};
            for(int sign:{-1,1})if(const auto adjacent=same(tx+(axis==0?sign:0),ty+(axis==1?sign:0))) {
                const auto next=feature(*adjacent);
                for(unsigned c=0;c<3;++c)gradient[c]=std::max(gradient[c],std::abs(next[c]-value[c])*1.5);
            }
            for(unsigned c=0;c<3;++c)footprint[c]+=gradient[c]*std::abs((axis==0?root.x:root.y)-(axis==0?tx:ty));
        }
        for(unsigned c=0;c<3;++c)if(std::abs(value[c]-target[c])>std::min(footprint[c],p.finite_terminal?.05:.02))
            return SourceGuard::feature;
    }
    if(!std::isfinite(reciprocal) || reciprocal<=0 || std::abs(1/reciprocal-root.depth)>std::max(.01,root.depth*.005))
        return SourceGuard::depth;
    if(fx<=0 || fy<=0 || bx+1>=int(f.width) || by+1>=int(f.height))return SourceGuard::accepted;
    bool positive=false,negative=false;
    const auto classify=[&](const std::array<V,4>& grid) {
        for(unsigned corner=0;corner<4;++corner) {
            const auto dx=(corner&2U)?sub(grid[3],grid[2]):sub(grid[1],grid[0]),
                dy=(corner&1U)?sub(grid[3],grid[1]):sub(grid[2],grid[0]);
            const double area=p.finite_terminal?dx[0]*dy[1]-dx[1]*dy[0]:dot(grid[corner],cross(dx,dy));
            if(!std::isfinite(area))return false;
            positive|=area>1.e-20;negative|=area<-1.e-20;
        }
        return !(positive && negative);
    };
    std::array<V,4> corners;
    for(unsigned n=0;n<4;++n) {
        const auto tap=same(bx+int(n&1U),by+int(n>>1));if(!tap)return SourceGuard::path;
        corners[n]=feature(*tap);
    }
    if(!classify(corners))return SourceGuard::cell_fold;
    for(int ry=-1;ry<=1;++ry)for(int rx=-1;rx<=1;++rx) {
        if(rx==0 && ry==0)continue;
        std::array<V,4> ring;bool supported=true;
        for(unsigned n=0;n<4;++n) {
            const auto tap=same(bx+rx+int(n&1U),by+ry+int(n>>1));supported&=bool(tap);
            if(tap)ring[n]=feature(*tap);
        }
        if(supported && !classify(ring))return SourceGuard::ring_fold;
    }
    std::array<V,9> grid;
    for(unsigned gy=0;gy<3;++gy)for(unsigned gx=0;gx<3;++gx) {
        const unsigned at=gy*3+gx;
        if((gx%2)==0 && (gy%2)==0)grid[at]=corners[(gy/2)*2+gx/2];
        else {
            const auto ray=forward(i,p,bx+.5+gx*.5,by+.5+gy*.5);
            if(!ray.valid)return SourceGuard::forward;
            if(!p.finite_terminal)grid[at]=ray.outgoing;
            else {
                const auto plane=p.terminal_plane;const auto normal=unit(cross(sub(plane.b,plane.a),sub(plane.c,plane.a)));
                const double den=dot(normal,ray.outgoing),distance=dot(sub(plane.a,ray.origin),normal)/den;
                const auto hit=add(ray.origin,scale(ray.outgoing,distance));
                if(std::abs(den)<=1.e-12 || distance<=ray.bias || distance>=65536 || !inside(plane,hit))return SourceGuard::forward;
                const auto b=bary(plane,hit);grid[at]={b[0],b[1],0};
            }
            for(auto& component:grid[at])component=float(component);
        }
    }
    for(unsigned gy=0;gy<2;++gy)for(unsigned gx=0;gx<2;++gx) {
        const unsigned at=gy*3+gx;
        if(!classify({grid[at],grid[at+1],grid[at+3],grid[at+4]}))return SourceGuard::subcell_fold;
    }
    return SourceGuard::accepted;
}
}
