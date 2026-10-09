#pragma once
#include "starfox/render/framebuffer.hpp"
#include <array>
#include <cmath>

namespace starfox::render {

// CPU reference for the GPU meter. Meter scene pixels only; black borders,
// transparent pixels and HUD must not drive exposure upward. No frame history
// images or allocations are needed. Input/output RGB is display-encoded sRGB.
class AdaptiveExposure {
public:
    void reset() noexcept { valid_=false; stops_=0; }
    [[nodiscard]] float stops() const noexcept { return stops_; }
    static float linear(float v) noexcept {
        return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);
    }
    static float encoded(float v) noexcept {
        return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f;
    }
    void apply(const Framebuffer& frame,std::vector<std::uint8_t>& rgba,
        unsigned quality,double seconds,std::uint64_t epoch,bool paused=false) {
        const auto count=frame.pixels().size();
        if(!quality || quality>3 || !frame.layer_tags_enabled()
            || rgba.size()!=count*4 || !std::isfinite(seconds)) {reset();return;}
        const bool restart=!valid_ || epoch!=epoch_ || quality!=quality_
            || width_!=frame.stored_width() || height_!=frame.stored_height()
            || seconds<time_ || seconds-time_>1.;
        // Histogram permits trimmed log-luminance metering without sorting or
        // per-frame storage. Bright projectiles should not pump the whole scene.
        static const auto decode=[] {
            std::array<float,256> values{};
            for(unsigned i=0;i<256;++i) values[i]=linear(i/255.f);
            return values;
        }();
        std::array<unsigned,64> histogram{};
        unsigned samples=0;
        for(std::size_t i=0;i<count;++i) {
            if(frame.layer_tags()[i]==std::uint8_t(PixelLayer::two_d) || !rgba[i*4+3]) continue;
            const float y=.2126f*decode[rgba[i*4]]+.7152f*decode[rgba[i*4+1]]+.0722f*decode[rgba[i*4+2]];
            if(y<1.f/1024.f) continue;
            const unsigned bin=unsigned(std::clamp(int((std::log2(y)+10.f)*6.4f),0,63));
            ++histogram[bin];++samples;
        }
        const unsigned trim=samples/20;
        unsigned cumulative=0,accepted=0;
        double sum=0;
        for(unsigned b=0;b<64;++b) {
            const unsigned end=cumulative+histogram[b];
            const unsigned first=std::max(cumulative,trim),last=std::min(end,samples-trim);
            if(last>first) {accepted+=last-first;sum+=(last-first)*(-10.+(b+.5)/6.4);}
            cumulative=end;
        }
        const float limit=.5f*quality;
        const float target=accepted?std::clamp(float(std::log2(.18)-sum/accepted),-limit,limit):0.f;
        // Begin at neutral on a cut. Pause does not adapt; elapsed time rather
        // than presentation count gives the same response at 60/120/240 Hz.
        if(restart) stops_=0;
        else if(!paused) {
            const double tau=target<stops_?.25:.8;
            stops_+=(target-stops_)*float(-std::expm1(-(seconds-time_)/tau));
        }
        valid_=true;epoch_=epoch;quality_=quality;time_=seconds;
        width_=frame.stored_width();height_=frame.stored_height();
        if(stops_==0) return;
        const float gain=std::exp2(stops_);
        std::array<std::uint8_t,256> transfer{};
        for(unsigned i=0;i<256;++i)
            transfer[i]=std::uint8_t(std::clamp(std::lround(encoded(std::min(1.f,decode[i]*gain))*255.f),0L,255L));
        for(std::size_t i=0;i<count;++i) {
            if(frame.layer_tags()[i]==std::uint8_t(PixelLayer::two_d) || !rgba[i*4+3]) continue;
            for(unsigned c=0;c<3;++c)
                rgba[i*4+c]=transfer[rgba[i*4+c]];
        }
    }
private:
    bool valid_{};
    float stops_{};
    double time_{};
    std::uint64_t epoch_{};
    unsigned quality_{},width_{},height_{};
};
}
