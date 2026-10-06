#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/simulation/game_simulation.hpp"

namespace starfox::platform::nintendo_3ds {
enum class WindowCoverage {authored,full_scene,vertical_scene};
// The source colour window masks the composed world, not a mono framebuffer.
// Submit this screen-space black geometry AFTER the ordered upper-LCD scene.
// Actual additive/subtractive circle/colour math is separate, not faked black.
class PicaWindow {
public:
    PicaFrame prepare(const simulation::WindowWipeState& wipe,const FramePlan& plan,WindowCoverage coverage) {
        if(coverage!=WindowCoverage::authored && coverage!=WindowCoverage::full_scene
            && coverage!=WindowCoverage::vertical_scene)
            throw std::invalid_argument("Unknown 3DS source window coverage");
        if(plan.eye_count!=(plan.stereo?2U:1U)) throw std::invalid_argument("Invalid 3DS window eye plan");
        for(unsigned eye=0;eye<plan.eye_count;++eye) static_cast<void>(PicaProjection(plan,eye));
        if(wipe.active && wipe.horizontal_opening && (!std::isfinite(wipe.opening_top)
            || !std::isfinite(wipe.opening_bottom) || wipe.opening_top>wipe.opening_bottom))
            throw std::invalid_argument("Invalid 3DS horizontal source shutter");
        if(!initialized_ || coverage!=coverage_ || !same(wipe,wipe_)) {
            auto& next=next_;next.clear();
            // At most three runs per row. Coalesce unchanged horizontal runs
            // vertically; typical shutters then need just two rectangles.
            std::array<std::array<unsigned,2>,3> old{},runs{};
            unsigned old_count=0,first_row=0;
            for(unsigned y=0;y<=screen_height;++y) {
                unsigned count=0;
                if(y<screen_height) {
                    const bool expanded=coverage==WindowCoverage::full_scene;
                    const bool vertical=coverage!=WindowCoverage::authored;
                    const int sy=vertical?int(y*191/(screen_height-1)):int(y)-24;
                    std::array<unsigned,6> edges{0,top_width,0,0,0,0};
                    if(wipe.horizontal_opening) {
                        if(expanded) edges[2]=(top_width+221)/223;
                        else for(unsigned i=0;i<4;++i) edges[i+2]=boundary(std::array{15,16,17,241}[i],coverage);
                    } else if(sy>=0 && sy<192) {
                        edges[2]=boundary(wipe.left[sy]&255,coverage);
                        edges[3]=boundary((wipe.right[sy]&255)+1,coverage);
                        edges[4]=boundary(16,coverage);edges[5]=boundary(241,coverage);
                    }
                    std::sort(edges.begin(),edges.end());
                    // Logic is constant between its four source-window edges.
                    // Do not traverse/recolour a full 400x240 bitmap per wipe.
                    for(unsigned i=0;i+1<edges.size();++i) {
                        const auto left=edges[i],right=edges[i+1];
                        if(left==right || !masked(wipe,coverage,left,y)) continue;
                        if(count && runs[count-1][1]==left) runs[count-1][1]=right;
                        else {
                            if(count==runs.size()) throw std::logic_error("3DS window run bound exceeded");
                            runs[count++]={left,right};
                        }
                    }
                }
                bool equal=count==old_count;
                for(unsigned i=0;i<count && equal;++i) equal=runs[i]==old[i];
                if(!equal || y==screen_height) {
                    for(unsigned i=0;i<old_count;++i) rectangle(next,old[i][0],first_row,old[i][1],y);
                    old=runs;old_count=count;first_row=y;
                }
            }
            vertices_.swap(next);wipe_=wipe;coverage_=coverage;initialized_=true;++builds_;
        }
        draw_={0,unsigned(vertices_.size()),pica_no_texture,pica_identity,PicaSpace::screen,false,false,false};
        draw_.source_layer=0; // Closed windows must remain black during later colour math.
        return {plan,vertices_,vertices_.empty()?std::span<const PicaDraw>{}:std::span<const PicaDraw>(&draw_,1),{}};
    }
    [[nodiscard]] std::uint64_t builds() const noexcept {return builds_;}
private:
    static unsigned boundary(int source,WindowCoverage coverage) noexcept {
        if(coverage!=WindowCoverage::full_scene) return unsigned(std::clamp(source+int((top_width-256)/2),0,int(top_width)));
        const int value=source-16;
        if(value<=0) return 0;
        return unsigned(std::min(int(top_width),(value*int(top_width-1)+222)/223));
    }
    static bool same(const simulation::WindowWipeState& a,const simulation::WindowWipeState& b) noexcept {
        if(a.active!=b.active) return false;
        if(!a.active) return true;
        return a.logic==b.logic && a.horizontal_opening==b.horizontal_opening && a.left==b.left && a.right==b.right
            && (!a.horizontal_opening || (a.opening_top==b.opening_top && a.opening_bottom==b.opening_bottom));
    }
    static bool masked(const simulation::WindowWipeState& wipe,WindowCoverage coverage,unsigned x,unsigned y) {
        if(!wipe.active) return false;
        const bool expanded=coverage==WindowCoverage::full_scene;
        const bool vertical=coverage!=WindowCoverage::authored;
        const int sx=expanded?16+int(x*223/(top_width-1)):int(x)-int((top_width-256)/2);
        if(wipe.horizontal_opening) {
            const double sy=vertical?(y+.5)*192/screen_height:double(y)+.5-24;
            if(sy<0 || sy>=192) return false;
            if(sy<wipe.opening_top || sy>=wipe.opening_bottom) return true;
            if(expanded) return x<(top_width+221)/223;
            return (!(sx>=15 && sx<=16))!=(sx>=16 && sx<=240);
        }
        const int sy=vertical?int(y*191/(screen_height-1)):int(y)-24;
        if(sy<0 || sy>=192) return false;
        const int left=wipe.left[sy]&255,right=wipe.right[sy]&255;
        const bool inside=left<=right?(sx>=left && sx<=right):(sx>=left || sx<=right);
        const bool a=!inside,b=sx>=16 && sx<=240;
        switch(wipe.logic&3) {
        case 1: return a && b;
        case 2: return a!=b;
        case 3: return a==b;
        default: return a || b;
        }
    }
    static void rectangle(std::vector<PicaVertex>& out,unsigned left,unsigned top,unsigned right,unsigned bottom) {
        if(left>=right || top>=bottom) return;
        if(out.size()>pica_vertex_limit-6) throw std::length_error("3DS source window geometry budget exceeded");
        const std::array<Point3,4> corners{{{float(left),float(top),0},{float(right),float(top),0},
            {float(right),float(bottom),0},{float(left),float(bottom),0}}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) out.push_back({corners[corner],{0,0,0,1},{}});
    }
    simulation::WindowWipeState wipe_{};
    WindowCoverage coverage_{};
    bool initialized_{};
    std::uint64_t builds_{};
    std::vector<PicaVertex> vertices_,next_;
    PicaDraw draw_{};
};
} // namespace starfox::platform::nintendo_3ds
