#pragma once
#include "sfvr/sfvr_perf.h"
#include <array>
#include <optional>
#include <string>

namespace starfox::vr {
// The standard `[vr-perf]` line (steam-frame-vr-port standard, section 8),
// formatted by the shared sfvr_perf. One frame per submitted stereo frame:
// the four CPU stages are the existing per-frame profile timings (logic,
// model assembly, upload, layer composition), the eye values are each eye's
// CPU submit-to-fence milliseconds, and `gpu` is the left plus right eye GPU
// timestamp milliseconds when both exist, otherwise n/a. It runs alongside
// --profile-csv and does not replace it.
class PerfLog {
public:
    struct Frame {
        bool missed{};
        std::optional<double> logic_ms,model_ms,upload_ms,layer_ms;
        std::array<std::optional<double>,2> eye_ms;
        std::array<std::optional<double>,2> gpu_ms;
    };
    explicit PerfLog(double window_seconds=SFVR_PERF_DEFAULT_WINDOW_S) {
        constexpr const char* stages[]{"logic","model","upload","layer"};
        sfvr_perf_init(&perf_,window_seconds,stages,4);
    }
    // Unmeasured values count as zero; an unknown GPU time is excluded.
    void add_frame(double now_seconds,const Frame& frame) noexcept {
        const float stages[4]{value(frame.logic_ms),value(frame.model_ms),
            value(frame.upload_ms),value(frame.layer_ms)};
        const float eyes[2]{value(frame.eye_ms[0]),value(frame.eye_ms[1])};
        const float gpu=frame.gpu_ms[0]&&frame.gpu_ms[1]
            ?static_cast<float>(*frame.gpu_ms[0]+*frame.gpu_ms[1]):-1.F;
        sfvr_perf_add_frame(&perf_,now_seconds,frame.missed?1:0,stages,eyes,gpu);
    }
    // The finished window's line, once per window, else nothing.
    [[nodiscard]] std::optional<std::string> poll(double now_seconds) noexcept {
        char line[256];
        if(!sfvr_perf_poll(&perf_,now_seconds,line,sizeof line)) return std::nullopt;
        return std::string(line);
    }
private:
    static float value(const std::optional<double>& ms) noexcept {
        return ms?static_cast<float>(*ms):0.F;
    }
    sfvr_perf perf_{};
};
}
