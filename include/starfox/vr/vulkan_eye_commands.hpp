#pragma once
#include "starfox/vr/vulkan_eye_targets.hpp"
#include <cstdint>
#include <functional>
#include <optional>
namespace starfox::vr {
// One in-flight eye submission. Caller serializes access to the shared queue.
// Never release an XR image until poll() returns complete. Errors require
// device/session teardown, not reuse of an image with uncertain completion.
class VulkanEyeCommands {
public:
    enum class Completion {complete,pending,error};
    using Record=std::function<void(VkCommandBuffer,VkExtent2D)>;
    struct TimelineWait {VkSemaphore semaphore{};uint64_t value{};VkPipelineStageFlags stage{VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};};
    struct TimestampConfig {std::uint32_t valid_bits{};double period_ns{};};
    VulkanEyeCommands()=default;
    ~VulkanEyeCommands();
    VulkanEyeCommands(const VulkanEyeCommands&)=delete;
    VulkanEyeCommands& operator=(const VulkanEyeCommands&)=delete;
    bool initialize(VkDevice,VkQueue,uint32_t queue_family,PFN_vkGetDeviceProcAddr,
        std::optional<TimestampConfig> timestamps=std::nullopt);
    // Standalone producer work, without a render pass or XR image. Poll before
    // handing geometry to another API or reusing this command allocation.
    bool submit_work(VkExtent2D,const Record&,const TimelineWait* wait=nullptr);
    bool submit(const VulkanEyeTargets&,unsigned eye,unsigned image,
        const VkClearColorValue&,const Record& record={},const Record& before_render={},
        const Record& after_render={},const TimelineWait* wait=nullptr);
    // Optional wait requires a timeline-enabled device. Keep semaphore alive
    // until completion. after_render runs outside the pass for ownership release.
    // before_render runs after command begin, outside the render pass. It
    // owns compute-to-graphics barriers; both callbacks share one submission.
    // Optional bounded wait wakes on completion instead of a fixed host sleep.
    Completion poll(uint64_t timeout_ns=0);
    bool gpu_timestamps_available() const noexcept {return query_pool_!=VK_NULL_HANDLE && timestamps_active_;}
    const std::string& timestamp_status() const noexcept {return timestamp_status_;}
    std::optional<double> take_gpu_duration_ms() noexcept {
        auto result=gpu_duration_ms_;gpu_duration_ms_.reset();return result;
    }
    static std::optional<double> timestamp_duration_ms(std::uint64_t begin,std::uint64_t end,
        std::uint32_t valid_bits,double period_ns) noexcept;
    void close() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    VkDevice device_{};VkQueue queue_{};VkCommandPool pool_{};
    VkCommandBuffer command_{};VkFence fence_{};
    VkQueryPool query_pool_{};
    bool pending_{},failed_{};
    bool timestamps_active_{};
    std::uint32_t timestamp_valid_bits_{};
    double timestamp_period_ns_{};
    std::optional<double> gpu_duration_ms_;
    PFN_vkDestroyCommandPool destroy_pool_{};
    PFN_vkDestroyFence destroy_fence_{};
    PFN_vkQueueWaitIdle idle_{};
    PFN_vkResetCommandPool reset_pool_{};
    PFN_vkResetFences reset_fences_{};
    PFN_vkBeginCommandBuffer begin_{};
    PFN_vkEndCommandBuffer end_{};
    PFN_vkCmdBeginRenderPass begin_pass_{};
    PFN_vkCmdEndRenderPass end_pass_{};
    PFN_vkQueueSubmit submit_{};
    PFN_vkGetFenceStatus fence_status_{};
    PFN_vkWaitForFences wait_fences_{};
    PFN_vkDestroyQueryPool destroy_query_pool_{};
    PFN_vkCmdResetQueryPool reset_query_pool_{};
    PFN_vkCmdWriteTimestamp write_timestamp_{};
    PFN_vkGetQueryPoolResults get_query_results_{};
    std::string status_{"Eye commands not initialized"};
    std::string timestamp_status_{"GPU timestamps not requested"};
};
}
