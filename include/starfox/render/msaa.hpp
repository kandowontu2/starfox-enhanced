#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
namespace starfox::render {
struct MsaaPoint {float x{},y{};};
inline constexpr std::array<MsaaPoint,14> msaa_offsets{{
#define SF_MSAA_SAMPLE(x,y) {float(x)/16,float(y)/16},
#include "starfox/render/msaa_samples.inc"
#undef SF_MSAA_SAMPLE
}};
inline bool msaa_sample_count(unsigned count) {return count==2 || count==4 || count==8;}
inline float msaa_edge(MsaaPoint a,MsaaPoint b,MsaaPoint p) {
    // Evaluate a shared edge with identical operands in both triangles, then
    // invert its sign. Different origins otherwise round differently at 4K+.
    const bool flip=a.y>b.y || (a.y==b.y && a.x>b.x);
    if(flip) std::swap(a,b);
    const float value=(b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
    return flip?-value:value;
}
inline bool msaa_top_left(MsaaPoint a,MsaaPoint b) {
    return b.y<a.y || (b.y==a.y && b.x>a.x);
}
// The same top-left rule is used for every sample and winding. Adjacent faces
// therefore own a shared-edge sample exactly once, not via an epsilon overlap.
inline std::uint32_t msaa_coverage(std::array<MsaaPoint,3> vertices,unsigned x,unsigned y,unsigned count) {
    if(!msaa_sample_count(count)) throw std::invalid_argument("MSAA sample count");
    for(const auto& p:vertices) if(!std::isfinite(p.x) || !std::isfinite(p.y)) return 0;
    float area=msaa_edge(vertices[0],vertices[1],vertices[2]);if(area==0 || !std::isfinite(area)) return 0;
    if(area<0) std::swap(vertices[1],vertices[2]);
    std::uint32_t mask=0;
    for(unsigned sample=0;sample<count;++sample) {
        const auto offset=msaa_offsets[count-2+sample];
        const MsaaPoint p{float(x)+.5f+offset.x,float(y)+.5f+offset.y};
        bool covered=true;
        for(unsigned edge=0;edge<3;++edge) {
            const auto a=vertices[edge],b=vertices[(edge+1)%3];const float d=msaa_edge(a,b,p);
            covered&=d>0 || (d==0 && msaa_top_left(a,b));
        }
        if(covered) mask|=1U<<sample;
    }
    return mask;
}
// One shade evaluation per primitive/pixel; only visibility/color storage is
// multisampled. The callback can evaluate the face material at the pixel centre.
struct MsaaRowSpan { float left{},right{}; };
// Repeated-row effects can emit disjoint intervals into one scanline. Keep
// their sample union, not a min/max envelope which would fill the gaps.
inline std::uint32_t msaa_row_coverage(std::span<const MsaaRowSpan> spans,unsigned x,unsigned count) {
    if(!msaa_sample_count(count)) throw std::invalid_argument("MSAA sample count");
    std::uint32_t mask=0;
    for(const auto& span:spans) {
        if(!std::isfinite(span.left) || !std::isfinite(span.right) || span.right<=span.left) continue;
        for(unsigned sample=0;sample<count;++sample) {
            const float at=float(x)+.5f+msaa_offsets[count-2+sample].x;
            if(at>=span.left && at<span.right) mask|=1U<<sample;
        }
    }
    return mask;
}
template<class Shade>
inline void msaa_paint(std::span<std::array<float,4>> samples,std::uint32_t coverage,Shade&& shade) {
    if(!msaa_sample_count(unsigned(samples.size()))) throw std::invalid_argument("MSAA sample storage");
    coverage&=(1U<<samples.size())-1;if(!coverage) return;
    const auto color=shade();
    for(unsigned i=0;i<samples.size();++i) if(coverage&(1U<<i)) samples[i]=color;
}
inline std::array<float,4> msaa_resolve(std::span<const std::array<float,4>> samples) {
    if(!msaa_sample_count(unsigned(samples.size()))) throw std::invalid_argument("MSAA sample storage");
    std::array<float,4> result{};
    for(const auto& s:samples) for(unsigned c=0;c<4;++c) result[c]+=s[c];
    for(auto& c:result) c/=float(samples.size());
    return result;
}
}
