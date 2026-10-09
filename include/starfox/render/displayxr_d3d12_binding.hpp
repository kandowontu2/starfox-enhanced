#pragma once
#include "starfox/render/displayxr_runtime.hpp"
#include <memory>

namespace starfox::render {
// Validated, borrowed same-device binding. Does not create a second device or
// queue, start a session, load a loader, or change the selected GPU. The caller
// synchronizes XR functions that access the queue via the SDL XR queue bridge.
// Runtime, device and queue outlive this object and any session using binding().
class DisplayXrD3D12Binding {
public:
    DisplayXrD3D12Binding();
    ~DisplayXrD3D12Binding();
    DisplayXrD3D12Binding(const DisplayXrD3D12Binding&)=delete;
    DisplayXrD3D12Binding& operator=(const DisplayXrD3D12Binding&)=delete;
    bool initialize(const DisplayXrRuntime&,void* device,void* queue);
    // Borrowed negotiated dispatch for deterministic native-GPU tests. This is
    // graphics validation only: it makes no claim of physical panel discovery.
    bool initialize_with_api(XrInstance,XrSystemId,PFN_xrGetInstanceProcAddr,
                            void* device,void* queue);
    void close() noexcept;
    const void* binding() const noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    struct State;
    std::unique_ptr<State> state_;
    std::string status_{"Leia D3D12 graphics requirements not checked"};
};
}
