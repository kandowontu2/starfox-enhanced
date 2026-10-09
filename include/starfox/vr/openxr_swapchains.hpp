#pragma once
#include "starfox/vr/openxr_session.hpp"
namespace starfox::vr {
struct SwapchainApi {
    PFN_xrEnumerateSwapchainFormats formats{xrEnumerateSwapchainFormats};
    PFN_xrCreateSwapchain create{xrCreateSwapchain};
    PFN_xrDestroySwapchain destroy{xrDestroySwapchain};
    PFN_xrEnumerateSwapchainImages images{xrEnumerateSwapchainImages};
    PFN_xrAcquireSwapchainImage acquire{xrAcquireSwapchainImage};
    PFN_xrWaitSwapchainImage wait{xrWaitSwapchainImage};
    PFN_xrReleaseSwapchainImage release{xrReleaseSwapchainImage};
};
enum class ImageWait {ready,waiting,error};
class OpenXrSwapchains {
public:
    explicit OpenXrSwapchains(SwapchainApi api={}):api_(api) {}
    ~OpenXrSwapchains();
    OpenXrSwapchains(const OpenXrSwapchains&)=delete;
    OpenXrSwapchains& operator=(const OpenXrSwapchains&)=delete;
    // Session/device outlive this object. Caller must finish GPU work before
    // close/destruction. Preference values use the selected graphics API.
    bool initialize(XrSession,std::span<const XrViewConfigurationView>,
        std::span<const int64_t> preferred_formats,XrSwapchainUsageFlags additional_usage=0);
    void close() noexcept;
    bool start_frame(const StereoFrame&,XrSpace);
    // Timeout preserves the acquired image; retry waits rather than acquiring
    // another image. Never write an image until ready is returned.
    ImageWait acquire_eye(unsigned eye,XrDuration timeout=0);
    bool release_eye(unsigned eye); // only after GPU work is submitted/completed as required by the graphics binding
    // Suppress submission and return acquired images. Caller first completes
    // any GPU work. A timeout/error retains ownership; retry before next frame.
    ImageWait cancel_frame(XrDuration timeout=0);
    // Caller allocates/initializes graphics-specific image structures.
    bool enumerate_images(unsigned eye,uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader*);
    const XrCompositionLayerProjection* projection() const noexcept;
    XrSwapchain handle(unsigned eye) const noexcept {return eye<2?eyes_[eye].handle:XR_NULL_HANDLE;}
    uint32_t image_count(unsigned eye) const noexcept {return eye<2?eyes_[eye].count:0;}
    std::optional<uint32_t> image_index(unsigned eye) const noexcept;
    int64_t format() const noexcept {return format_;}
    const std::string& status() const noexcept {return status_;}
private:
    struct Eye {
        XrSwapchain handle{XR_NULL_HANDLE};
        uint32_t count{},index{};
        bool acquired{},ready{},released{};
    };
    SwapchainApi api_;
    std::array<Eye,2> eyes_{};
    std::array<XrCompositionLayerProjectionView,2> views_{{
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
    XrCompositionLayerProjection layer_{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    int64_t format_{};
    bool frame_{};
    std::string status_{"OpenXR swapchains not initialized"};
};
// A separate, mono UI swapchain. Its image is released only after the caller's
// GPU fence completes; timeouts retain ownership across frame-loop retries.
class OpenXrQuad {
public:
    explicit OpenXrQuad(SwapchainApi api={}):api_(api) {}
    ~OpenXrQuad() {close();}
    OpenXrQuad(const OpenXrQuad&)=delete;
    OpenXrQuad& operator=(const OpenXrQuad&)=delete;
    bool initialize(XrSession,int64_t format,uint32_t width=1024,uint32_t height=896);
    void close() noexcept;
    bool enumerate_images(uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader*);
    ImageWait acquire(XrDuration timeout=0);
    bool release();
    ImageWait cancel(XrDuration timeout=0);
    uint32_t image_count() const noexcept {return count_;}
    std::optional<uint32_t> image_index() const noexcept {return ready_?std::optional<uint32_t>(index_):std::nullopt;}
    const XrCompositionLayerQuad* layer(XrSpace,XrPosef,float width=1.15F) noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    SwapchainApi api_;
    XrCompositionLayerQuad layer_{XR_TYPE_COMPOSITION_LAYER_QUAD};
    uint32_t count_{},index_{};
    bool acquired_{},ready_{},released_{};
    std::string status_;
};

}
