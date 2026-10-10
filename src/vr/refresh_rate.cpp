#include "starfox/vr/refresh_rate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace starfox::vr {
RefreshApi RefreshApi::from_instance(XrInstance instance) {
    RefreshApi api;
    if(instance==XR_NULL_HANDLE) return api;
    const auto load=[&](const char* name,auto& target) {
        PFN_xrVoidFunction function{};
        if(XR_FAILED(xrGetInstanceProcAddr(instance,name,&function))) function=nullptr;
        target=reinterpret_cast<std::remove_reference_t<decltype(target)>>(function);
    };
    load("xrEnumerateDisplayRefreshRatesFB",api.enumerate);
    load("xrGetDisplayRefreshRateFB",api.get);
    load("xrRequestDisplayRefreshRateFB",api.request);
    if(!api.available()) api={};
    return api;
}

std::optional<float> choose_refresh_rate(std::span<const float> offered,float target) noexcept {
    std::optional<float> best_at_or_below,lowest;
    for(const float rate:offered) {
        if(!std::isfinite(rate) || rate<=0.F) continue;
        if(!lowest || rate<*lowest) lowest=rate;
        if(rate<=target && (!best_at_or_below || rate>*best_at_or_below)) best_at_or_below=rate;
    }
    return best_at_or_below?best_at_or_below:lowest;
}

bool RefreshRate::request(XrSession session,float target) {
    if(!api_.available() || session==XR_NULL_HANDLE) {
        status_="XR_FB_display_refresh_rate unavailable";return false;
    }
    session_=session;
    uint32_t count=0;
    if(XR_FAILED(api_.enumerate(session,0,&count,nullptr)) || count==0 || count>64) {
        status_="Enumerate display refresh rates failed";return false;
    }
    std::vector<float> rates(count);
    if(XR_FAILED(api_.enumerate(session,count,&count,rates.data())) || count>rates.size()) {
        status_="Read display refresh rates failed";return false;
    }
    rates.resize(count);
    offered_=rates;
    const auto chosen=choose_refresh_rate(offered_,target);
    if(!chosen) {status_="Runtime offered no usable display refresh rate";return false;}
    const auto result=api_.request(session,*chosen);
    if(XR_FAILED(result)) {
        status_="Request display refresh rate "+std::to_string(*chosen)+": OpenXR result "+std::to_string(result);
        return false;
    }
    requested_=*chosen;
    float actual=0.F;
    current_=XR_SUCCEEDED(api_.get(session,&actual)) && actual>0.F?actual:*chosen;
    // Restart the governor: a new target is measured from scratch.
    started_=tainted_=false;frames_=0;low_windows_=0;
    status_="Display refresh rate requested";
    return true;
}

std::optional<float> RefreshRate::observe(double now,bool focused) noexcept {
    if(!requested_ || fell_back_) return std::nullopt;
    if(!started_) {started_=true;window_start_=now;frames_=0;tainted_=!focused;}
    if(!focused) tainted_=true;
    ++frames_;
    if(now-window_start_<window_seconds) return std::nullopt;
    const double fps=static_cast<double>(frames_)/(now-window_start_);
    if(api_.get && session_!=XR_NULL_HANDLE) {
        float actual=0.F;
        if(XR_SUCCEEDED(api_.get(session_,&actual)) && actual>0.F) current_=actual;
    }
    const double reference=current_?*current_:*requested_;
    // A window with any unfocused frame says nothing about performance.
    if(tainted_) low_windows_=0;
    else if(fps<reference*low_fraction) ++low_windows_;
    else low_windows_=0;
    window_start_=now;frames_=0;tainted_=false;
    if(low_windows_<low_windows_before_fallback) return std::nullopt;
    // Step to the next lower offered rate, never below the floor.
    std::optional<float> lower;
    for(const float rate:offered_)
        if(std::isfinite(rate) && rate>=fallback_rate-.5F && rate<reference-.5 && (!lower || rate>*lower)) lower=rate;
    low_windows_=0;
    if(!lower) return std::nullopt; // Already at the lowest usable rate.
    if(*lower<=fallback_rate+.5F) fell_back_=true;
    return lower;
}
}
