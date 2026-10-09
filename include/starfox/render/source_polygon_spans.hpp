#pragma once
#include "starfox/simulation/math.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace starfox::render {
struct SourceSpanPoint {std::int32_t x{},y{};};
struct SourceSpanModes {
    std::uint8_t wireframe{},wobble{};
    bool cel{},wave{},winding_independent{};
    std::int16_t wave_offset{};
    std::uint32_t animation_frame{};
};
struct SourcePolygonSpan {
    std::int32_t left{},right{},y{},source_y{}; // Inclusive horizontal ink; unwarped Y retained.
};
// MDRAWP's authored solid/EX span rules, not triangle edge coverage. The caller
// supplies the already clipped, rounded source boundary. No framebuffer, pixel
// array, material substitution, depth flattening or second-eye simulation is
// performed here. Consumers retain source_y when reconstructing displaced
// wave coverage onto the original camera-space surface.
template<class Emit>
void source_polygon_spans(std::span<const SourceSpanPoint> points,unsigned width,
    const SourceSpanModes& modes,Emit&& emit) {
    if(points.size()<3) return;
    auto minimum=std::size_t{};
    auto maximum_y=points.front().y;
    for(std::size_t index=1;index<points.size();++index) {
        if(points[index].y<points[minimum].y) minimum=index;
        maximum_y=std::max(maximum_y,points[index].y);
    }
    auto y=points[minimum].y;
    if(y==maximum_y) return;
    const bool extended_x=width>224;
    const auto fixed_x=[extended_x](std::int32_t value) {
        return extended_x?value*256:std::int32_t(simulation::wrap16(std::int64_t(value)*256));
    };
    const auto integer_x=[extended_x](std::int32_t value) {
        return extended_x?simulation::arithmetic_shift_right(value,8):std::int32_t(std::uint16_t(value)>>8);
    };
    const auto rounded_x=[extended_x,&integer_x](std::int32_t value) {
        return extended_x?simulation::arithmetic_shift_right(value+127,8)
            :integer_x(simulation::add16(std::int16_t(value),127));
    };
    const auto advance_x=[extended_x](std::int32_t value,std::int32_t increment) {
        if(extended_x) return value+increment;
        auto advanced=simulation::add16(std::int16_t(value),std::int16_t(increment));
        // Only the source's narrow -8..0 wrapped interval is corrected.
        if(advanced<0 && simulation::add16(advanced,0x0800)>=0) advanced=0;
        return std::int32_t(advanced);
    };
    const auto edge_increment=[extended_x](std::int32_t difference,std::int32_t scanlines) {
        if(scanlines<=0) return std::int32_t{};
        const auto reciprocal=scanlines==1?32767:32768/scanlines;
        const auto value=simulation::arithmetic_shift_right(difference*reciprocal,7);
        return extended_x?value:std::int32_t(simulation::wrap16(value));
    };
    struct Tracer {
        std::size_t vertex{};int direction{};
        std::int32_t x{},increment{},remaining{};
    };
    const auto initial_x=fixed_x(points[minimum].x);
    Tracer left{minimum,1,initial_x,0,0},right{minimum,-1,initial_x,0,0};
    const auto begin_segment=[&](Tracer& tracer) {
        auto rounded=rounded_x(tracer.x);
        for(std::size_t guard=0;guard<points.size();++guard) {
            tracer.vertex=tracer.direction>0?(tracer.vertex+1)%points.size()
                :(tracer.vertex+points.size()-1)%points.size();
            const auto& endpoint=points[tracer.vertex];
            const auto scanlines=endpoint.y-y;
            if(scanlines<0) return false;
            if(!scanlines) {rounded=endpoint.x;tracer.x=fixed_x(rounded);continue;}
            tracer.x=fixed_x(rounded);
            tracer.increment=edge_increment(endpoint.x-rounded,scanlines);
            tracer.remaining=scanlines;return true;
        }
        return false;
    };
    constexpr std::array<std::int8_t,32> wave_sine{
        0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1,
        0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
    const auto wave_y=[&](std::int32_t x) {
        auto phase=simulation::wrap16(std::int32_t(modes.wave_offset)+x);
        phase=simulation::wrap16(simulation::arithmetic_shift_right(phase,1)
            +std::int32_t(modes.animation_frame&15)-1);
        auto index=std::int32_t(phase)%std::int32_t(wave_sine.size());
        if(index<0) index+=std::int32_t(wave_sine.size());
        return y+wave_sine[std::size_t(index)];
    };
    const auto span=[&](std::int32_t x1,std::int32_t x2,std::int32_t plotted_y) {
        if(x1<=x2) emit(SourcePolygonSpan{x1,x2,plotted_y,y});
    };
    bool mode2_edge_continuation=false,has_previous_wobble_left=false;
    std::int32_t previous_wobble_left{};
    while(y<maximum_y) {
        const bool left_starts_segment=left.remaining==0,right_starts_segment=right.remaining==0;
        if(left_starts_segment && !begin_segment(left)) return;
        if(right_starts_segment && !begin_segment(right)) return;
        auto x1=integer_x(left.x),x2=integer_x(right.x);
        if(modes.winding_independent && x2<x1) std::swap(x1,x2);
        if(x2>=x1) {
            if(modes.wobble&2) {
                if(!right_starts_segment && has_previous_wobble_left)
                    span(previous_wobble_left,previous_wobble_left,y);
            } else if((modes.wireframe==1 && !left_starts_segment && !right_starts_segment)
                || (modes.wireframe==2 && mode2_edge_continuation
                    && !left_starts_segment && !right_starts_segment)) {
                span(x1,x1,y);if(x2!=x1) span(x2,x2,y);
            } else if(modes.cel && !modes.wireframe) {
                span(x1+1,x2-1,y);
            } else if(modes.wave && !modes.wireframe) {
                // Group only adjacent pixels with the same source displacement.
                // Parity is still evaluated at each final X/Y by the consumer.
                auto first=x1,plotted_y=wave_y(x1);
                for(auto x=x1+1;x<=x2;++x) {
                    const auto next_y=wave_y(x);
                    if(next_y!=plotted_y) {span(first,x-1,plotted_y);first=x;plotted_y=next_y;}
                }
                span(first,x2,plotted_y);
            } else span(x1,x2,y);
        }
        if(modes.wobble&2) {previous_wobble_left=x1;has_previous_wobble_left=true;}
        if(modes.wireframe==2) {
            if(right_starts_segment) mode2_edge_continuation=false;
            if(left_starts_segment && !right_starts_segment) mode2_edge_continuation=true;
        }
        left.x=advance_x(left.x,left.increment);right.x=advance_x(right.x,right.increment);
        --left.remaining;--right.remaining;
        if(modes.wobble&1) {
            if(left.remaining && right.remaining) continue;
            ++y;
        }
        ++y;
    }
}
} // namespace starfox::render
