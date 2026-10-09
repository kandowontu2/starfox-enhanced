#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace starfox::render {
// Reference resolve for the native previous-minus-current pixel-motion
// convention. previous_depth is the current surface transformed into the
// previous camera, not current camera Z (which changes during camera travel).
struct TemporalAaGuide {
    float motion_x{},motion_y{},depth{},previous_depth{};
    bool valid{},eligible{};
    std::uint32_t pattern_phase{}; // Optional independently supplied point-pattern witness.
    std::uint32_t edge_phase{}; // Optional independently supplied pre-style edge/grid witness.
};
class TemporalAa {
public:
    void reset() noexcept { width_=height_=0;history_.clear();depth_.clear();eligible_.clear();phases_.clear();edges_.clear(); }
    std::vector<std::uint8_t> resolve(unsigned width,unsigned height,
        std::span<const std::uint8_t> current,std::span<const TemporalAaGuide> guides,
        std::uint64_t epoch,float history_weight=.85f) {
        const auto size=std::size_t(width)*height;
        if(!width || !height || current.size()!=size*4 || guides.size()!=size)
            throw std::invalid_argument("TAA input dimensions");
        if(!std::isfinite(history_weight)) throw std::invalid_argument("TAA history weight");
        std::vector<std::uint8_t> output(current.begin(),current.end());
        const bool reuse=width_==width && height_==height && epoch_==epoch;
        history_weight=std::clamp(history_weight,0.f,.95f);
        if(reuse) for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto i=std::size_t(y)*width+x;const auto& g=guides[i];
            if(!g.valid || !g.eligible || !(g.depth>0) || !(g.previous_depth>0)
                || !std::isfinite(g.motion_x) || !std::isfinite(g.motion_y)
                || !std::isfinite(g.depth) || !std::isfinite(g.previous_depth)) continue;
            const float hx=x+g.motion_x,hy=y+g.motion_y;
            if(hx<0 || hy<0 || hx>width-1 || hy>height-1) continue;
            const unsigned ax=unsigned(hx),ay=unsigned(hy);
            const float fx=hx-ax,fy=hy-ay;
            std::array<float,3> prior{};float sum=0;bool mixed_phase=false;
            for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                const auto n=std::size_t(std::min(ay+dy,height-1))*width+std::min(ax+dx,width-1);
                const float w=(dx?fx:1-fx)*(dy?fy:1-fy);
                if(!eligible_[n] || !std::isfinite(depth_[n])
                    || std::abs(depth_[n]-g.previous_depth)>std::max(.01f,g.previous_depth*.01f)) continue;
                if(w>0 && (phases_[n]!=g.pattern_phase || edges_[n]!=g.edge_phase)) {mixed_phase=true;continue;}
                for(unsigned c=0;c<3;++c) prior[c]+=history_[n*4+c]*w;
                sum+=w;
            }
            // Do not stretch one surviving tap across an occlusion edge.
            if(mixed_phase || sum<.75f) continue;
            std::array<float,3> lo{255,255,255},hi{};
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                const auto n=std::size_t(std::clamp(int(y)+dy,0,int(height)-1))*width
                    +std::clamp(int(x)+dx,0,int(width)-1);
                if(!guides[n].eligible || guides[n].pattern_phase!=g.pattern_phase || guides[n].edge_phase!=g.edge_phase) continue;
                for(unsigned c=0;c<3;++c) {
                    lo[c]=std::min(lo[c],float(current[n*4+c]));
                    hi[c]=std::max(hi[c],float(current[n*4+c]));
                }
            }
            for(unsigned c=0;c<3;++c) {
                const float clipped=std::clamp(prior[c]/sum,lo[c],hi[c]);
                output[i*4+c]=std::uint8_t(std::clamp(std::lround(
                    current[i*4+c]*(1-history_weight)+clipped*history_weight),0L,255L));
            }
        }
        width_=width;height_=height;epoch_=epoch;history_=output;
        depth_.resize(size);eligible_.resize(size);phases_.resize(size);edges_.resize(size);
        for(std::size_t i=0;i<size;++i) {depth_[i]=guides[i].depth;eligible_[i]=guides[i].eligible;phases_[i]=guides[i].pattern_phase;edges_[i]=guides[i].edge_phase;}
        return output;
    }
private:
    unsigned width_{},height_{};std::uint64_t epoch_{};
    std::vector<std::uint8_t> history_,eligible_;std::vector<float> depth_;
    std::vector<std::uint32_t> phases_,edges_;
};
}
