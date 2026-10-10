#pragma once
#include "starfox/vr/stereo_renderer.hpp"
#include "starfox/vr/vulkan_eye_commands.hpp"
#include <chrono>
#include <optional>
namespace starfox::vr {
// Adapter retained for the lifetime of the frame driver. Commands/targets
// outlive it. Only this adapter may submit work on its command object.
class VulkanStereoDraw {
public:
    struct CompletionTiming {
        uint64_t eyes{},pending_polls{},submissions{};
        double total_ms{},maximum_ms{},submit_ms{};
    };
    struct EyeTiming {
        double submit_to_fence_cpu_ms{};
        std::optional<double> gpu_timestamp_ms;
    };
    // CPU elapsed submission-to-fence-observation time, including polling
    // delays. This is intentionally not labelled GPU timestamp time.
    CompletionTiming take_completion_timing() noexcept {auto result=timing_;timing_={};return result;}
    EyeTiming take_last_eye_timing() noexcept {auto result=last_eye_timing_;last_eye_timing_={};return result;}
    bool pending() const noexcept {return submitted_;}
    using Record=std::function<void(VkCommandBuffer,VkExtent2D,const EyeCamera&,XrTime)>;
    VulkanStereoDraw(VulkanEyeCommands& commands,const VulkanEyeTargets& targets)
        :commands_(commands),targets_(targets) {}
    StereoRenderer::EyeResult draw(unsigned eye,uint32_t image,const EyeCamera&,XrTime,
        const VkClearColorValue&,const Record& record={},const Record& before_render={},
        const Record& after_render={},const VulkanEyeCommands::TimelineWait* wait=nullptr);
private:
    VulkanEyeCommands& commands_;
    const VulkanEyeTargets& targets_;
    bool submitted_{};
    unsigned eye_{};uint32_t image_{};
    std::chrono::steady_clock::time_point submitted_at_{};
    CompletionTiming timing_{};
    EyeTiming last_eye_timing_{};
};
}
