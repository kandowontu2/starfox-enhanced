#pragma once
#include "starfox/render/scene_enhancements.hpp"
#include "starfox/render/motion_blur.hpp"

namespace starfox::render {
struct SceneShutterOcclusion {
    float depth{};
    float coverage{};
    std::array<double,3> linear_colour{}; // Conditional foreground colour, not premultiplied.
};
// Reference moving-foreground coverage at one exposure instant. Coverage is
// retained separately: a fractionally covered edge must not fully hide a
// particle. Missing depth is uncovered, not a fabricated stationary surface.
inline bool scene_shutter_occlusion(unsigned width,unsigned height,
    std::span<const MotionBlurGuide> guides,const MotionBlurSettings& settings,
    double time,std::vector<SceneShutterOcclusion>& output,std::span<const std::uint8_t> colour={}) {
    const auto count=std::size_t(width)*height;
    if(!width||!height||width>16384||height>16384||guides.size()!=count
        ||(!colour.empty()&&colour.size()!=count*4)||!std::isfinite(time)||time<-.5||time>.5
        ||!std::isfinite(settings.interval_seconds)||settings.interval_seconds<0
        ||!std::isfinite(settings.exposure_seconds)||settings.exposure_seconds<0
        ||!std::isfinite(settings.maximum_radius)||settings.maximum_radius<0||settings.maximum_radius>128
        ||!std::isfinite(settings.relative_depth_tolerance)||settings.relative_depth_tolerance<0) return false;
    const bool moving=!settings.paused&&settings.interval_seconds>0&&settings.interval_seconds<=.25
        &&settings.exposure_seconds>0&&settings.maximum_radius>0;
    const double ratio=moving?settings.exposure_seconds/settings.interval_seconds:0;
    if(!std::isfinite(ratio)) return false;
    std::vector<SceneShutterOcclusion> result(count);
    std::vector<double> weights(count);
    const auto visit=[&](auto&& emit) {
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto& g=guides[std::size_t(y)*width+x];
            if(!g.eligible||!std::isfinite(g.depth)||g.depth<=0) continue;
            if(!colour.empty()&&colour[(std::size_t(y)*width+x)*4+3]==0) continue;
            double vx=0,vy=0;
            if(g.valid&&std::isfinite(g.motion_x)&&std::isfinite(g.motion_y)) {
                vx=g.motion_x*ratio;vy=g.motion_y*ratio;
                const double length=std::hypot(vx,vy);
                if(!std::isfinite(length)||length<.01) vx=vy=0;
                else if(length>2*settings.maximum_radius) {
                    const double bound=2*settings.maximum_radius/length;vx*=bound;vy*=bound;
                }
            }
            const double px=x+vx*time,py=y+vy*time;
            const int ax=int(std::floor(px)),ay=int(std::floor(py));
            for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
                const int xx=ax+dx,yy=ay+dy;
                if(xx<0||yy<0||xx>=int(width)||yy>=int(height)) continue;
                const auto at=std::size_t(yy)*width+xx;
                if(!guides[at].eligible) continue;
                const float weight=float((dx?px-ax:1-(px-ax))*(dy?py-ay:1-(py-ay)));
                if(weight>0) emit(std::size_t(y)*width+x,at,g.depth,weight);
            }
        }
    };
    visit([&](auto,auto at,float depth,float) {
        if(result[at].depth==0||depth<result[at].depth) result[at].depth=depth;
    });
    visit([&](auto from,auto at,float depth,float weight) {
        if(depth-result[at].depth<=std::max(.01f,result[at].depth*settings.relative_depth_tolerance)) {
            result[at].coverage=std::min(1.f,result[at].coverage+weight);
            weights[at]+=weight;
            if(!colour.empty()) for(unsigned c=0;c<3;++c) {
                const double v=colour[from*4+c]/255.;
                result[at].linear_colour[c]+=weight*(v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4));
            }
        }
    });
    for(std::size_t i=0;i<count;++i) if(weights[i]>0)
        for(auto& channel:result[i].linear_colour) channel/=weights[i];
    output=std::move(result);return true;
}
// Stable partition for separate radiance and coverage passes. Preserve birth
// identity and previous projections; add() alone intentionally clears history.
// Unsupported optical effects reject the entire split transactionally.
inline bool split_scene_exposure(const SceneFxFrame& source,SceneFxFrame& lights,SceneFxFrame& particles) {
    const float count=source.camera[3];
    if(!std::isfinite(count)||count<0||count>scene_fx_capacity||std::floor(count)!=count
        || &lights==&particles) return false;
    SceneFxFrame l,p;l.camera=p.camera=source.camera;l.camera[3]=p.camera[3]=0;
    for(unsigned n=0;n<unsigned(count);++n) {
        const float type=source.data[n*3+1][0];
        if(!std::isfinite(type)||type<1||type>7||std::floor(type)!=type) return false;
        auto& dest=type==1?l:p;const unsigned at=unsigned(dest.camera[3]);
        dest.add(source.data[n*3],source.data[n*3+1],source.data[n*3+2]);
        dest.motion_points[at]=source.motion_points[n];dest.motion_previous[at]=source.motion_previous[n];
    }
    lights=std::move(l);particles=std::move(p);return true;
}
// Reference shutter for coverage-changing particles. Surface lighting and
// optical distortions retain their current samples; they are separate passes.
inline bool scene_shutter_sample(const SceneFxFrame& source,const MotionBlurSettings& settings,
    double time,SceneFxFrame& result) {
    if(!std::isfinite(time) || time<-.5 || time>.5 || !std::isfinite(settings.interval_seconds)
        || settings.interval_seconds<0 || !std::isfinite(settings.exposure_seconds) || settings.exposure_seconds<0
        || !std::isfinite(settings.maximum_radius) || settings.maximum_radius<0 || settings.maximum_radius>128
        || !std::isfinite(source.camera[3]) || source.camera[3]<0 || source.camera[3]>scene_fx_capacity
        || std::floor(source.camera[3])!=source.camera[3]) return false;
    auto sampled=source;
    if(!settings.paused && settings.interval_seconds>0 && settings.interval_seconds<=.25
        && settings.exposure_seconds>0 && settings.maximum_radius>0) {
        const double shutter=settings.exposure_seconds/settings.interval_seconds;
        if(!std::isfinite(shutter)) return false;
        for(unsigned n=0;n<unsigned(source.camera[3]);++n) {
            const unsigned at=n*3;const float type=source.data[at+1][0];
            if(type<2 || type>7 || !source.motion_previous[n].valid) continue;
            const auto& prior=source.motion_previous[n].previous;
            const auto& now=source.data[at];const float z=source.data[at+1][1];
            if(!std::isfinite(prior[0]) || !std::isfinite(prior[1]) || !std::isfinite(prior[2]) || prior[2]<=0
                || !std::isfinite(now[0]) || !std::isfinite(now[1]) || !std::isfinite(z) || z<=0) continue;
            double dx=(double(prior[0])-now[0])*shutter,dy=(double(prior[1])-now[1])*shutter;
            double dz=(double(prior[2])-z)*shutter;
            if(!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) continue;
            const double travel=std::hypot(dx,dy);
            const double bound=travel>2*settings.maximum_radius?2*settings.maximum_radius/travel:1.;
            const double sample_z=z+dz*time*bound;
            if(sample_z<32 || sample_z>10000) {sampled.data[at][3]=0;continue;}
            sampled.data[at][0]=float(now[0]+dx*time*bound);
            sampled.data[at][1]=float(now[1]+dy*time*bound);
            sampled.data[at][2]=float(now[2]*double(z)/sample_z);
            sampled.data[at+1][1]=float(sample_z);
            if(type==7) sampled.data[at+1][2]=float(source.data[at+1][2]-time*settings.exposure_seconds*bound);
            sampled.motion_points[n].projected={sampled.data[at][0],sampled.data[at][1],float(sample_z)};
        }
    }
    result=std::move(sampled);return true;
}

// CPU reference, not the production frame loop. Integrate rendered shutter
// samples in linear light. Each sample independently tests foreground depth;
// HUD and alpha retain the ordinary scene-effects renderer's protection.
inline bool render_scene_shutter(const SceneFxFrame& effects,const MotionBlurSettings& settings,
    const Framebuffer& frame,const std::vector<std::uint8_t>& source,std::vector<std::uint8_t>& output,
    const SurfaceBuffer* surfaces=nullptr,int ox=0,int oy=0,
    std::span<const MotionBlurGuide> moving_occluders={},
    std::span<const std::uint8_t> world_underlay={}) {
    if(source.size()!=frame.pixels().size()*4 || settings.samples<1 || settings.samples>65) return false;
    if(!moving_occluders.empty() && (moving_occluders.size()!=frame.pixels().size() || ox || oy)) return false;
    if(!world_underlay.empty() && (world_underlay.size()!=source.size() || moving_occluders.empty())) return false;
    SceneFxFrame sampled;
    if(!scene_shutter_sample(effects,settings,0,sampled)) return false;
    const unsigned samples=settings.paused || settings.interval_seconds==0 || settings.interval_seconds>.25
        || settings.exposure_seconds==0 || settings.maximum_radius==0?1:settings.samples;
    std::vector<std::array<double,3>> sum(frame.pixels().size());
    std::array<double,256> decode{};
    for(unsigned i=0;i<256;++i) {const double v=i/255.;decode[i]=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
    std::vector<std::uint8_t> scratch;
    for(unsigned tap=0;tap<samples;++tap) {
        const double time=samples==1?0:double(tap)/(samples-1)-.5;
        if(!scene_shutter_sample(effects,settings,time,sampled)) return false;
        if(!moving_occluders.empty()) {
            SceneFxFrame lights,particles;
            if(!split_scene_exposure(sampled,lights,particles)||lights.active()) return false;
            std::vector<SceneShutterOcclusion> occlusion;
            const auto w=frame.stored_width(),h=frame.stored_height();
            if(!scene_shutter_occlusion(w,h,moving_occluders,settings,time,occlusion,
                world_underlay.empty()?std::span<const std::uint8_t>{}:std::span<const std::uint8_t>{source})) return false;
            SurfaceBuffer moving_depth(w,h);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const auto i=std::size_t(y)*w+x;
                moving_depth.set(x,y,{0,0,-1,occlusion[i].depth,frame.pixels()[i],occlusion[i].depth>0},frame.pixels()[i]);
            }
            auto covered=source,uncovered=source;
            if(!world_underlay.empty()) {
                std::copy(world_underlay.begin(),world_underlay.end(),uncovered.begin());
                for(std::size_t i=0;i<sum.size();++i) {
                    // A depthless world sprite cannot be reconstructed as a
                    // model. Keep it in the uncovered layer rather than erase
                    // it globally when an unrelated model starts moving.
                    if(!std::isfinite(moving_occluders[i].depth)||moving_occluders[i].depth<=0)
                        std::copy_n(source.begin()+i*4,4,uncovered.begin()+i*4);
                    if(!moving_occluders[i].eligible || (frame.layer_tags_enabled()&&frame.layer_tags()[i]==1)) {
                        std::copy_n(source.begin()+i*4,4,uncovered.begin()+i*4);
                        continue;
                    }
                    for(unsigned c=0;c<3;++c) {
                        const double v=std::clamp(occlusion[i].linear_colour[c],0.,1.);
                        const double encoded=v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;
                        covered[i*4+c]=std::uint8_t(std::clamp(std::lround(encoded*255),0L,255L));
                    }
                }
            }
            apply_scene_enhancements(sampled,frame,covered,scratch,&moving_depth,0,0);
            apply_scene_enhancements(sampled,frame,uncovered,scratch,nullptr,0,0);
            for(std::size_t i=0;i<sum.size();++i) for(unsigned c=0;c<3;++c)
                sum[i][c]+=decode[covered[i*4+c]]*occlusion[i].coverage
                    +decode[uncovered[i*4+c]]*(1-occlusion[i].coverage);
            continue;
        }
        auto colour=source;apply_scene_enhancements(sampled,frame,colour,scratch,surfaces,ox,oy);
        for(std::size_t i=0;i<sum.size();++i) for(unsigned c=0;c<3;++c) sum[i][c]+=decode[colour[i*4+c]];
    }
    auto result=source;
    for(std::size_t i=0;i<sum.size();++i) for(unsigned c=0;c<3;++c) {
        if(!moving_occluders.empty()&&(!moving_occluders[i].eligible||source[i*4+3]==0)) continue;
        const double v=std::clamp(sum[i][c]/samples,0.,1.);
        const double encoded=v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;
        result[i*4+c]=std::uint8_t(std::clamp(std::lround(encoded*255),0L,255L));
    }
    output=std::move(result);return true;
}
}
