#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/simulation/game_simulation.hpp"

namespace starfox::platform::nintendo_3ds {
// Native fixed-function post colour math. Only screen-space coverage is
// generated here; the presenter's winning-layer stencil gates each eye's
// already-rendered geometry/artwork. The lower LCD is never part of this pass.
// PICA blends in RGBA8, so 5-bit source re-quantization/half rounding still
// require physical pixel acceptance; this is not claimed bit-exact to SNES.
class PicaColourEffects {
public:
    PicaFrame prepare(const simulation::CircleEffectState& circle,
        const simulation::ColourMathEffectState& math,unsigned brightness,
        const FramePlan& plan,std::optional<PicaClip> circle_clip={}) {
        if(brightness>15 || plan.eye_count!=(plan.stereo?2U:1U))
            throw std::invalid_argument("Invalid 3DS source colour frame");
        for(unsigned eye=0;eye<plan.eye_count;++eye) static_cast<void>(PicaProjection(plan,eye));
        if(circle_clip) static_cast<void>(pica_screen_scissor(*circle_clip));
        Key key;
        if(circle.active && circle.radius && (circle.affected_layers&63)) {
            key.disk={true,int(circle.centre_x)+72,int(circle.centre_y)+8,circle.radius,
                {bool(circle.affected_layers&128),bool(circle.affected_layers&64),std::uint8_t(circle.affected_layers&63)},
                {circle_fixed(circle.red,brightness),circle_fixed(circle.green,brightness),circle_fixed(circle.blue,brightness)},circle_clip};
        }
        if(math.active && (math.affected_layers&63))
            key.full={true,0,0,0,{math.subtract,math.half,std::uint8_t(math.affected_layers&63)},
                {expand(math.red),expand(math.green),expand(math.blue)},{}};
        if(!initialized_ || key!=key_) {
            auto& vertices=next_vertices_;auto& draws=next_draws_;vertices.clear();draws.clear();
            if(key.disk.active) {
                const auto first=unsigned(vertices.size());
                const auto bounds=key.disk.clip.value_or(PicaClip{});
                int old_left=0,old_right=0,top=bounds.top;
                const std::int64_t radius=key.disk.radius,r2=radius*radius;
                // Integer source disk, not a polygon approximation. Coalesce
                // identical scanline spans; no per-pixel/world texture build.
                for(int y=bounds.top;y<=bounds.bottom;++y) {
                    int left=0,right=0;
                    if(y<bounds.bottom) {
                        const std::int64_t dy=std::int64_t(y)-key.disk.y;
                        if(dy*dy<=r2) {
                            const auto remaining=r2-dy*dy;
                            auto extent=std::int64_t(std::sqrt(double(remaining)));
                            while((extent+1)*(extent+1)<=remaining) ++extent;
                            while(extent*extent>remaining) --extent;
                            left=int(std::clamp(std::int64_t(key.disk.x)-extent,std::int64_t(bounds.left),std::int64_t(bounds.right)));
                            right=int(std::clamp(std::int64_t(key.disk.x)+extent+1,std::int64_t(bounds.left),std::int64_t(bounds.right)));
                        }
                    }
                    if(left!=old_left || right!=old_right || y==bounds.bottom) {
                        rectangle(vertices,old_left,top,old_right,y,key.disk.fixed);
                        top=y;old_left=left;old_right=right;
                    }
                }
                submit(draws,first,unsigned(vertices.size())-first,key.disk);
            }
            if(key.full.active) {
                const auto first=unsigned(vertices.size());
                rectangle(vertices,0,0,top_width,screen_height,key.full.fixed);
                submit(draws,first,6,key.full);
            }
            vertices_.swap(vertices);draws_.swap(draws);key_=key;initialized_=true;++builds_;
        }
        return {plan,vertices_,draws_,{}};
    }
    [[nodiscard]] std::uint64_t builds() const noexcept {return builds_;}
private:
    struct Region {
        bool active{};int x{},y{};unsigned radius{};
        PicaColourOp op;Rgb fixed;std::optional<PicaClip> clip;
        bool operator==(const Region&) const=default;
    };
    struct Key {Region disk,full;bool operator==(const Key&) const=default;};
    static std::uint8_t expand(unsigned value) noexcept {value&=31;return std::uint8_t((value<<3)|(value>>2));}
    static std::uint8_t circle_fixed(unsigned value,unsigned brightness) noexcept {
        // The desktop/source circle scales the fixed colour by master
        // brightness before its five-bit arithmetic, unlike global math.
        return expand((unsigned(expand(value))*brightness/15)>>3);
    }
    static void rectangle(std::vector<PicaVertex>& vertices,int left,int top,int right,int bottom,Rgb fixed) {
        if(left>=right || top>=bottom) return;
        if(vertices.size()>pica_vertex_limit-6) throw std::length_error("3DS colour coverage budget exceeded");
        const std::array<float,4> colour{fixed.r/255.F,fixed.g/255.F,fixed.b/255.F,1};
        const std::array<Point3,4> corners{{{float(left),float(top),0},{float(right),float(top),0},
            {float(right),float(bottom),0},{float(left),float(bottom),0}}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) vertices.push_back({corners[corner],colour,{}});
    }
    static void submit(std::vector<PicaDraw>& draws,unsigned first,unsigned count,const Region& region) {
        if(!count) return;
        PicaDraw draw{first,count,pica_no_texture,pica_identity,PicaSpace::screen,false,false,false};
        draw.source_layer=0;draw.colour_op=region.op;draw.clip=region.clip;draws.push_back(draw);
    }
    Key key_;bool initialized_{};std::uint64_t builds_{};
    std::vector<PicaVertex> vertices_,next_vertices_;
    std::vector<PicaDraw> draws_,next_draws_;
};
} // namespace starfox::platform::nintendo_3ds
