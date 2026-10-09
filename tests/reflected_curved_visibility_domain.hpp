#pragma once
// Test-only necessary old-source domains. This is NOT an inverse/root solver
// or a uniqueness certificate. Every qualified source must lie in one of the
// returned regions, but a region need not contain a physical optical root.
// The query uses raw path/feature/visibility only, never colour or GPU guides.
#include "reflected_curved_source_guard.hpp"
#include <limits>

namespace reflected_curved_owner_oracle {
struct VisibilityRegion {
    unsigned x{},y{},span_x{},span_y{};
    std::array<double,2> minimum{},maximum{},initializer{};
    bool contains(double px,double py) const {
        constexpr double enclosure=1.e-10;
        return px>=minimum[0]-enclosure && px<=maximum[0]+enclosure
            && py>=minimum[1]-enclosure && py<=maximum[1]+enclosure;
    }
};
template<class Read>
inline std::vector<VisibilityRegion> old_visibility_domains(const Inputs& i,unsigned primary,
    std::span<const unsigned,16> record,Read read) {
    const auto& f=i.liquid;
    if(!f.width || !f.height || f.width>65536 || f.height>65536
        || std::uint64_t(f.width)*f.height>65536)
        throw std::runtime_error("Unbounded independent old visibility domain");
    const unsigned hops=record[4]&7U,mask=(record[4]>>4)&15U,kind=record[4]>>8;
    if(hops>4 || kind<1 || kind>3 || (mask>>hops)!=0)return {};
    V target;for(unsigned c=0;c<3;++c)target[c]=std::bit_cast<float>(record[6+c]);
    if(!valid_feature(target,kind))return {};
    if(kind!=1)target=old_environment_direction(i,target);
    auto tags=std::array<unsigned,6>{};std::copy_n(record.begin(),6,tags.begin());
    const auto mapped=[&](unsigned id)->std::optional<unsigned> {
        if(id>=i.mapping.size() || i.mapping[id]>=i.previous.size())return {};
        return i.mapping[id];
    };
    unsigned old_primary=primary;
    if(primary!=0xfffffffdU) {const auto id=mapped(primary);if(!id)return {};old_primary=*id;}
    for(unsigned h=0;h<hops;++h)if(!(mask&(1U<<h))) {
        const auto id=mapped(record[h]);if(!id)return {};tags[h]=*id;
    }
    if(kind==1) {const auto id=mapped(record[5]);if(!id)return {};tags[5]=*id;}
    struct Entry {V feature{},dx{},dy{};bool same{},visible{};};
    // One bounded raw-bank read. Depth validity is required for contributing
    // taps, NOT adjacent feature-gradient taps (matching source_guard).
    std::vector<Entry> entries(std::size_t(f.width)*f.height);
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x) {
        const auto tap=read(x,y);if(!tap || tap->identity!=old_primary
            || !std::equal(tags.begin(),tags.end(),tap->path.begin()))continue;
        auto& e=entries[std::size_t(y)*f.width+x];
        for(unsigned c=0;c<3;++c)e.feature[c]=std::bit_cast<float>(tap->path[6+c]);
        e.same=valid_feature(e.feature,kind);
        e.visible=e.same && std::isfinite(tap->depth) && tap->depth>0;
    }
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x) {
        auto& e=entries[std::size_t(y)*f.width+x];if(!e.visible)continue;
        for(unsigned axis=0;axis<2;++axis)for(int side:{-1,1}) {
            const int nx=int(x)+(axis==0?side:0),ny=int(y)+(axis==1?side:0);
            if(nx<0 || ny<0 || nx>=int(f.width) || ny>=int(f.height))continue;
            const auto& n=entries[std::size_t(ny)*f.width+unsigned(nx)];if(!n.same)continue;
            auto& gradient=axis==0?e.dx:e.dy;
            for(unsigned c=0;c<3;++c)gradient[c]=std::max(gradient[c],std::abs(n.feature[c]-e.feature[c])*1.5);
        }
    }
    std::vector<VisibilityRegion> regions;
    constexpr double outward=1.e-12; // query enclosure only, NOT a source-guard tolerance
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x)
        for(unsigned sy=0;sy<2;++sy)for(unsigned sx=0;sx<2;++sx) {
            if(x+sx>=f.width || y+sy>=f.height)continue;
            // Include cells, zero-weight horizontal/vertical edges AND integer
            // vertices. Dropping the latter would miss otherwise valid sources.
            using P=std::array<double,2>;
            std::array<P,16> polygon{},scratch{};unsigned size=0;
            polygon[size++]={0,0};
            if(sx)polygon[size++]={1,0};
            if(sy) {polygon[size++]={double(sx),1};if(sx)polygon[size++]={0,1};}
            bool possible=true;
            for(unsigned dy=0;possible && dy<=sy;++dy)for(unsigned dx=0;possible && dx<=sx;++dx) {
                const auto& e=entries[std::size_t(y+dy)*f.width+x+dx];
                if(!e.visible) {possible=false;break;}
                for(unsigned c=0;possible && c<3;++c) {
                    const double difference=std::abs(e.feature[c]-target[c]);
                    if(difference>(kind==1?.05:.02)+outward) {possible=false;break;}
                    // In this support, |q-tap| is affine. Intersect the EXACT
                    // per-tap feature-footprint inequalities (outward rounded)
                    // instead of selecting a neighbourhood around a known root.
                    const double a=sx?(dx?-e.dx[c]:e.dx[c]):0,
                        b=sy?(dy?-e.dy[c]:e.dy[c]):0,
                        bound=difference-2.e-6-e.dx[c]*dx-e.dy[c]*dy-outward;
                    const auto value=[&](P p){return a*p[0]+b*p[1]-bound;};
                    unsigned next=0;
                    for(unsigned n=0;n<size;++n) {
                        const P first=polygon[n],last=polygon[(n+1)%size];
                        const double vfirst=value(first),vlast=value(last);
                        if(vfirst>=0) {
                            if(next==scratch.size())throw std::runtime_error("Unbounded visibility polygon");
                            scratch[next++]=first;
                        }
                        if((vfirst>=0)!=(vlast>=0)) {
                            const double t=std::clamp(vfirst/(vfirst-vlast),0.,1.);
                            if(next==scratch.size())throw std::runtime_error("Unbounded visibility polygon");
                            scratch[next++]={first[0]+t*(last[0]-first[0]),first[1]+t*(last[1]-first[1])};
                        }
                    }
                    size=next;std::copy_n(scratch.begin(),size,polygon.begin());possible=size!=0;
                }
            }
            if(!possible)continue;
            VisibilityRegion r{x,y,sx,sy,{double(x+sx),double(y+sy)},{double(x),double(y)},{0,0}};
            for(unsigned n=0;n<size;++n)for(unsigned c=0;c<2;++c) {
                const double coordinate=polygon[n][c]+(c==0?x:y);
                r.minimum[c]=std::min(r.minimum[c],coordinate);r.maximum[c]=std::max(r.maximum[c],coordinate);
                r.initializer[c]+=(coordinate+.5)/size;
            }
            regions.push_back(r);
        }
    return regions;
}
}
