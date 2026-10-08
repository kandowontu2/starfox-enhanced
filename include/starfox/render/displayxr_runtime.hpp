#pragma once
#include <openxr/openxr.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace starfox::render {
enum class DisplayXrBackend {direct3d12,vulkan};
struct DisplayXrPanel {
    std::uint32_t pixel_width{},pixel_height{};
    XrExtent2Df physical_size{};
    XrVector3f nominal_viewer{};
    std::array<float,2> view_scale{};
    XrRect2Di desktop_rect{};
    std::string device_name;
    bool primary{};
};
// Optional display discovery, separate from the ordinary renderer and PCVR.
// Loads ONLY the explicitly selected/installed DisplayXR runtime through the
// public OpenXR runtime-negotiation ABI. Does not mutate XR_RUNTIME_JSON,
// loader globals, registry values, PATH, or the system's active VR runtime.
// initialize() detects hardware; it does NOT prove session/presentation support.
class DisplayXrRuntime {
public:
    DisplayXrRuntime()=default;
    ~DisplayXrRuntime();
    DisplayXrRuntime(const DisplayXrRuntime&)=delete;
    DisplayXrRuntime& operator=(const DisplayXrRuntime&)=delete;
    bool initialize(const std::filesystem::path& runtime_directory={},
        DisplayXrBackend backend=DisplayXrBackend::direct3d12);
    // Borrowed negotiated dispatch, for deterministic tests/embedding. It must
    // remain callable until close/destruction; no implicit loader is invoked.
    bool initialize_with_api(PFN_xrGetInstanceProcAddr,
        DisplayXrBackend backend=DisplayXrBackend::direct3d12);
    void close() noexcept;
    bool detected() const noexcept {return instance_!=XR_NULL_HANDLE && system_!=XR_NULL_SYSTEM_ID;}
    XrInstance instance() const noexcept {return instance_;}
    XrSystemId system() const noexcept {return system_;}
    DisplayXrBackend backend() const noexcept {return backend_;}
    PFN_xrGetInstanceProcAddr get_instance_proc() const noexcept {return get_;}
    const DisplayXrPanel& panel() const noexcept {return panel_;}
    const std::array<XrViewConfigurationView,2>& views() const noexcept {return views_;}
    std::string_view status() const noexcept {
        if(!status_.empty()) return status_;
        return cleanup_pending_?"Leia SR unavailable: runtime cleanup pending":"Leia SR unavailable: not checked";
    }
    const std::string& runtime_name() const noexcept {return runtime_name_;}
    const std::string& system_name() const noexcept {return system_name_;}
private:
    bool discover(DisplayXrBackend);
    void* module_{};
    PFN_xrGetInstanceProcAddr get_{};
    PFN_xrDestroyInstance destroy_{};
    XrInstance instance_{XR_NULL_HANDLE};
    XrSystemId system_{XR_NULL_SYSTEM_ID};
    DisplayXrPanel panel_;
    std::array<XrViewConfigurationView,2> views_{};
    DisplayXrBackend backend_{DisplayXrBackend::direct3d12};
    bool cleanup_pending_{};
    std::string status_,runtime_name_,system_name_;
};
}
