#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <limits>
#include <vector>
namespace starfox::render {
struct MotionBlurGuide {
    // Previous-minus-current motion in unjittered output pixels. Interval is
    // supplied separately: a presentation displacement is not a velocity.
    float motion_x{},motion_y{},depth{};
    bool valid{},eligible{};
};
struct MotionBlurSettings {
    double interval_seconds{},exposure_seconds{1./120};
    float maximum_radius{32},relative_depth_tolerance{.02f};
    unsigned samples{9};
    bool paused{};
};
// Exposure is a duration, not a multiple of the presentation interval: an
// identical moving object has the same trail at 60, 120 or 240 FPS. Medium
// retains the validated diagnostic shutter. Off is an exact identity preset.
inline MotionBlurSettings motion_blur_preset(unsigned quality,unsigned scale=1) noexcept {
    MotionBlurSettings settings;
    quality=std::min(quality,3U);scale=std::clamp(scale,1U,10U);
    constexpr std::array<double,4> exposure{0,1./240,1./120,1./60};
    constexpr std::array<unsigned,4> samples{1,5,9,13};
    constexpr std::array<float,4> radius{0,16,32,48};
    settings.exposure_seconds=exposure[quality];settings.samples=samples[quality];
    settings.maximum_radius=std::min(128.f,radius[quality]*scale);
    return settings;
}
// Align jittered native guides to TAA's stable presentation grid. Select the
// closest contributing surface as a complete depth/velocity pair; averaging
// motion across silhouettes invents correspondence. Destination HUD ownership
// remains authoritative. Jitter changes sample location, NEVER velocity.
inline bool resolve_motion_blur_guides(unsigned width,unsigned height,
    std::span<const MotionBlurGuide> source,std::array<float,2> jitter,
    std::vector<MotionBlurGuide>& output) {
    const auto count=std::size_t(width)*height;
    if(!width || !height || source.size()!=count
        || !std::isfinite(jitter[0]) || !std::isfinite(jitter[1])
        || std::abs(jitter[0])>1 || std::abs(jitter[1])>1) return false;
    std::vector<MotionBlurGuide> result(source.begin(),source.end());
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const auto i=std::size_t(y)*width+x;
        if(!source[i].eligible) continue;
        const double sx=std::clamp(double(x)+jitter[0],0.,double(width-1));
        const double sy=std::clamp(double(y)+jitter[1],0.,double(height-1));
        const unsigned ax=unsigned(sx),ay=unsigned(sy);
        const double fx=sx-ax,fy=sy-ay;
        MotionBlurGuide chosen{};chosen.eligible=true;
        double best_weight=-1;
        for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
            const double weight=(dx?fx:1-fx)*(dy?fy:1-fy);
            if(weight<=0) continue;
            const auto& sample=source[std::size_t(std::min(ay+dy,height-1))*width+std::min(ax+dx,width-1)];
            if(!sample.eligible || !std::isfinite(sample.depth) || sample.depth<=0) continue;
            if(chosen.depth==0 || sample.depth<chosen.depth || (sample.depth==chosen.depth && weight>best_weight)) {
                chosen=sample;best_weight=weight;
            }
        }
        if(!chosen.valid || !std::isfinite(chosen.motion_x) || !std::isfinite(chosen.motion_y)) {
            chosen.valid=false;chosen.motion_x=chosen.motion_y=0;
        }
        result[i]=chosen;
    }
    output=std::move(result);return true;
}
// Motion vectors and their elapsed interval must refer to the same committed
// presentation. Merely composing a frame does not advance either history.
class MotionBlurTimeline {
public:
    void reset() noexcept {valid_=false;}
    double interval(double seconds,std::uint64_t serial,std::uint64_t epoch,bool paused) const noexcept {
        if(!valid_||paused||!std::isfinite(seconds)||serial_==UINT64_MAX
            ||serial!=serial_+1||epoch!=epoch_) return 0;
        const double elapsed=seconds-seconds_;
        return elapsed>0&&elapsed<=.25?elapsed:0;
    }
    void commit(double seconds,std::uint64_t serial,std::uint64_t epoch,bool paused,bool presented) noexcept {
        if(!presented||paused||!std::isfinite(seconds)) {reset();return;}
        seconds_=seconds;serial_=serial;epoch_=epoch;valid_=true;
    }
private:
    double seconds_{};
    std::uint64_t serial_{},epoch_{};
    bool valid_{};
};
// Decode the compositor's native buffers. Motion is already unjittered and in
// this buffer's pixel units; adding TAA jitter here would manufacture blur in
// a stationary scene. Reject stale motion whose recorded Z disagrees with the
// visible depth, but retain that depth for stationary occlusion.
inline bool motion_blur_guides(std::span<const std::uint32_t> packed,
    std::span<const float> depth,std::span<const std::array<float,4>> motion,
    bool history_valid,std::vector<MotionBlurGuide>& output) {
    if(packed.empty()||depth.size()!=packed.size()||motion.size()!=packed.size()) return false;
    std::vector<MotionBlurGuide> result(packed.size());
    for(std::size_t i=0;i<packed.size();++i) {
        const unsigned tag=(packed[i]>>8)&255;
        const bool eligible=tag<=5&&(tag!=1||(packed[i]&0x10000000U)!=0);
        const float z=depth[i];const auto& m=motion[i];
        const bool surface=eligible&&std::isfinite(z)&&z>0;
        const bool valid=surface&&history_valid&&m[3]==1
            &&std::isfinite(m[0])&&std::isfinite(m[1])&&std::isfinite(m[2])
            &&std::abs(m[2]-z)<=std::max(.001f,std::abs(z)*.00001f);
        result[i]={valid?m[0]:0,valid?m[1]:0,surface?z:0,valid,eligible};
    }
    output=std::move(result);return true;
}
// Depth-aware, centred-shutter reference. Uses the current colour image only;
// no persistence/history ghosts. The caller invalidates motion at scene cuts.
// Foreground silhouette dilation is a separate integration step, not supplied
// by this surface-local kernel. Protected pixels and alpha are byte-exact.
inline bool apply_motion_blur(unsigned width,unsigned height,
    std::span<const std::uint8_t> source,std::span<const MotionBlurGuide> guides,
    const MotionBlurSettings& settings,std::vector<std::uint8_t>& output) {
    const auto count=std::size_t(width)*height;
    if(!width||!height||source.size()!=count*4||guides.size()!=count
        ||!std::isfinite(settings.interval_seconds)||settings.interval_seconds<0
        ||!std::isfinite(settings.exposure_seconds)||settings.exposure_seconds<0
        ||!std::isfinite(settings.maximum_radius)||settings.maximum_radius<0||settings.maximum_radius>128
        ||!std::isfinite(settings.relative_depth_tolerance)||settings.relative_depth_tolerance<0
        ||settings.samples<1||settings.samples>65) return false;
    std::vector<std::uint8_t> result(source.begin(),source.end());
    // A stopped clock or a long discontinuity must not turn a stale vector
    // into a full-screen smear. Invalid motion is also handled per pixel.
    if(settings.paused||settings.interval_seconds==0||settings.interval_seconds>.25
        ||settings.exposure_seconds==0||settings.maximum_radius==0||settings.samples==1) {
        output=std::move(result);return true;
    }
    std::array<float,256> decode{};
    for(unsigned i=0;i<256;++i) {
        const float v=i/255.f;decode[i]=v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);
    }
    const double shutter=settings.exposure_seconds/settings.interval_seconds;
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const auto i=std::size_t(y)*width+x;const auto& g=guides[i];
        if(!g.valid||!g.eligible||source[i*4+3]==0||!std::isfinite(g.depth)||g.depth<=0
            ||!std::isfinite(g.motion_x)||!std::isfinite(g.motion_y)) continue;
        double vx=g.motion_x*shutter,vy=g.motion_y*shutter;
        const double length=std::hypot(vx,vy);
        if(!std::isfinite(length)||length<.01) continue;
        if(length>2*settings.maximum_radius) {
            const double scale=2*settings.maximum_radius/length;vx*=scale;vy*=scale;
        }
        std::array<double,3> sum{};double weight=0;
        for(unsigned sample=0;sample<settings.samples;++sample) {
            const double t=double(sample)/(settings.samples-1)-.5;
            const double px=x+vx*t,py=y+vy*t;
            if(px<0||py<0||px>width-1||py>height-1) continue;
            const unsigned ax=unsigned(px),ay=unsigned(py);
            const double fx=px-ax,fy=py-ay;
            for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                const auto n=std::size_t(std::min(ay+dy,height-1))*width+std::min(ax+dx,width-1);
                const auto& donor=guides[n];
                if(!donor.eligible||source[n*4+3]==0||!std::isfinite(donor.depth)||donor.depth<=0
                    ||std::abs(donor.depth-g.depth)>std::max(.01f,g.depth*settings.relative_depth_tolerance)) continue;
                const double w=(dx?fx:1-fx)*(dy?fy:1-fy);
                for(unsigned c=0;c<3;++c) sum[c]+=decode[source[n*4+c]]*w;
                weight+=w;
            }
        }
        if(weight<=0) continue;
        for(unsigned c=0;c<3;++c) {
            const double v=std::clamp(sum[c]/weight,0.,1.);
            const double encoded=v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;
            result[i*4+c]=std::uint8_t(std::clamp(std::lround(encoded*255),0L,255L));
        }
    }
    output=std::move(result);return true;
}
// Forward reconstruction reference for moving silhouettes. A world-only
// underlay supplies newly exposed scenery; it must not contain HUD or the
// moving foreground. Each shutter sample resolves nearest depth before colour
// accumulation, so a far object's trail cannot cross a nearer stationary one.
inline bool reconstruct_motion_blur(unsigned width,unsigned height,
    std::span<const std::uint8_t> source,std::span<const std::uint8_t> underlay,
    std::span<const MotionBlurGuide> guides,const MotionBlurSettings& settings,
    std::vector<std::uint8_t>& output) {
    const auto count=std::size_t(width)*height;
    if(!width||!height||source.size()!=count*4||underlay.size()!=source.size()||guides.size()!=count
        ||!std::isfinite(settings.interval_seconds)||settings.interval_seconds<0
        ||!std::isfinite(settings.exposure_seconds)||settings.exposure_seconds<0
        ||!std::isfinite(settings.maximum_radius)||settings.maximum_radius<0||settings.maximum_radius>128
        ||!std::isfinite(settings.relative_depth_tolerance)||settings.relative_depth_tolerance<0
        ||settings.samples<1||settings.samples>65) return false;
    std::vector<std::uint8_t> result(source.begin(),source.end());
    std::vector<std::array<double,2>> velocity(count);
    std::vector<std::uint8_t> affected(count);
    bool moving=false;
    if(!settings.paused&&settings.interval_seconds>0&&settings.interval_seconds<=.25
        &&settings.exposure_seconds>0&&settings.maximum_radius>0&&settings.samples>1) {
        const double shutter=settings.exposure_seconds/settings.interval_seconds;
        for(std::size_t i=0;i<count;++i) {
            const auto& g=guides[i];
            if(!g.eligible||!g.valid||source[i*4+3]==0||!std::isfinite(g.depth)||g.depth<=0
                ||!std::isfinite(g.motion_x)||!std::isfinite(g.motion_y)) continue;
            double x=g.motion_x*shutter,y=g.motion_y*shutter,len=std::hypot(x,y);
            if(!std::isfinite(len)||len<.01) continue;
            if(len>2*settings.maximum_radius) {const double s=2*settings.maximum_radius/len;x*=s;y*=s;}
            velocity[i]={x,y};affected[i]=1;moving=true;
        }
    }
    if(!moving) {output=std::move(result);return true;}
    std::array<double,256> decode{};
    for(unsigned i=0;i<256;++i) {const double v=i/255.;decode[i]=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
    std::vector<double> nearest(count),coverage(count);
    std::vector<std::array<double,3>> colour(count),integral(count);
    for(unsigned sample=0;sample<settings.samples;++sample) {
        const double time=double(sample)/(settings.samples-1)-.5;
        std::fill(nearest.begin(),nearest.end(),std::numeric_limits<double>::infinity());
        std::fill(coverage.begin(),coverage.end(),0);
        std::fill(colour.begin(),colour.end(),std::array<double,3>{});
        const auto splat=[&](auto&& visit) {
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const auto i=std::size_t(y)*width+x;const auto& g=guides[i];
                if(!g.eligible||source[i*4+3]==0||!std::isfinite(g.depth)||g.depth<=0) continue;
                const double px=x+velocity[i][0]*time,py=y+velocity[i][1]*time;
                const int ax=int(std::floor(px)),ay=int(std::floor(py));
                const double fx=px-ax,fy=py-ay;
                for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
                    const int xx=ax+dx,yy=ay+dy;
                    if(xx<0||yy<0||xx>=int(width)||yy>=int(height)) continue;
                    const auto n=std::size_t(yy)*width+xx;
                    if(!guides[n].eligible||source[n*4+3]==0) continue;
                    const double w=(dx?fx:1-fx)*(dy?fy:1-fy);
                    if(w>0) {
                        if(velocity[i][0]!=0||velocity[i][1]!=0) affected[n]=1;
                        visit(i,n,w,double(g.depth));
                    }
                }
            }
        };
        splat([&](auto,auto n,double,double depth){nearest[n]=std::min(nearest[n],depth);});
        splat([&](auto i,auto n,double w,double depth){
            if(depth-nearest[n]>std::max(.01,nearest[n]*settings.relative_depth_tolerance)) return;
            for(unsigned c=0;c<3;++c) colour[n][c]+=decode[source[i*4+c]]*w;
            coverage[n]+=w;
        });
        for(std::size_t i=0;i<count;++i) {
            const double alpha=std::min(coverage[i],1.);
            for(unsigned c=0;c<3;++c)
                integral[i][c]+=(coverage[i]>0?colour[i][c]/coverage[i]*alpha:0)
                    +decode[underlay[i*4+c]]*(1-alpha);
        }
    }
    // Unknown-depth world sprites outside the swept silhouettes are not part
    // of the underlay. Preserve them instead of erasing them globally merely
    // because another object somewhere on screen is moving.
    for(std::size_t i=0;i<count;++i) if(affected[i]&&guides[i].eligible&&source[i*4+3]!=0) for(unsigned c=0;c<3;++c) {
        const double v=std::clamp(integral[i][c]/settings.samples,0.,1.);
        const double encoded=v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;
        result[i*4+c]=std::uint8_t(std::clamp(std::lround(encoded*255),0L,255L));
    }
    output=std::move(result);return true;
}
}
