#include "starfox/render/displayxr_desktop.hpp"
#include "starfox/render/displayxr_vulkan_binding.hpp"
#include <windows.h>
#include <d3d12.h>
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>
#include <stdexcept>

namespace starfox::render {
struct DisplayXrDesktop::State {
    DisplayXrRuntime runtime;DisplayXrVulkanBinding vulkan_creation;DisplayXrGameRenderer renderer;
    LUID adapter{};SDL_Window* window{};
    int x{},y{},width{},height{};bool fullscreen{},bordered{},resizable{},placed{},attached{},closing{};
    std::optional<Clock::time_point> ready_since,pending_since;
    void restore() noexcept {
        if(!placed || !window) return;
        SDL_SetWindowBordered(window,bordered);
        SDL_SetWindowResizable(window,resizable);
        SDL_SetWindowPosition(window,x,y);SDL_SetWindowSize(window,width,height);
        SDL_SetWindowFullscreen(window,fullscreen);SDL_SyncWindow(window);placed=false;
    }
};
DisplayXrDesktop::DisplayXrDesktop()=default;
DisplayXrDesktop::~DisplayXrDesktop() {
    // Never unload the runtime beneath a native frame whose completion could
    // not be confirmed. The SDL window/device owner must also retain them.
    if(!close()) (void)state_.release();
}
bool DisplayXrDesktop::close() noexcept {
    if(state_ && !state_->renderer.close()) {status_=state_->renderer.status();return false;}
    if(state_) {
        state_->closing=true;state_->attached=false;state_->vulkan_creation.close();state_->runtime.close();
        if(state_->runtime.instance()!=XR_NULL_HANDLE) {status_=state_->runtime.status();return false;}
        state_->restore();
    }
    state_.reset();return true;
}
bool DisplayXrDesktop::try_close() noexcept {
    if(!state_) return true;
    state_->closing=true;state_->attached=false;
    if(!state_->renderer.try_close()) {status_=state_->renderer.status();return false;}
    state_->vulkan_creation.close();state_->runtime.close();
    if(state_->runtime.instance()!=XR_NULL_HANDLE) {status_=state_->runtime.status();return false;}
    state_->restore();state_.reset();return true;
}
bool DisplayXrDesktop::close_pending() const noexcept {return state_ && state_->closing;}
bool DisplayXrDesktop::prepare(const std::filesystem::path& directory,DisplayXrBackend backend) {
    return prepare_impl([&](DisplayXrRuntime& runtime) {return runtime.initialize(directory,backend);});
}
bool DisplayXrDesktop::prepare_with_api(PFN_xrGetInstanceProcAddr get,DisplayXrBackend backend) {
    return prepare_impl([&](DisplayXrRuntime& runtime) {return runtime.initialize_with_api(get,backend);});
}
bool DisplayXrDesktop::prepare_impl(const std::function<bool(DisplayXrRuntime&)>& discover) {
    if(!try_close()) return false;
    auto next=std::make_unique<State>();
    try {
        if(!discover(next->runtime))
            throw std::runtime_error(std::string(next->runtime.status()));
        if(next->runtime.backend()==DisplayXrBackend::vulkan) {
            if(!next->vulkan_creation.initialize(next->runtime)) throw std::runtime_error(next->vulkan_creation.status());
        } else {
            PFN_xrVoidFunction address{};
            if(XR_FAILED(next->runtime.get_instance_proc()(next->runtime.instance(),"xrGetD3D12GraphicsRequirementsKHR",&address)) || !address)
                throw std::runtime_error("Leia runtime has no D3D12 graphics requirements");
            XrGraphicsRequirementsD3D12KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
            if(XR_FAILED(reinterpret_cast<PFN_xrGetD3D12GraphicsRequirementsKHR>(address)(
                    next->runtime.instance(),next->runtime.system(),&requirements)))
                throw std::runtime_error("Leia D3D12 graphics requirements failed");
            next->adapter=requirements.adapterLuid;
        }
        state_=std::move(next);status_="Leia panel detected; native presentation not started";return true;
    } catch(const std::exception& error) {
        status_=error.what();
        // Failed discovery can itself leave an instance whose destroy failed.
        // Retain the dispatch/module and retry cleanup, never start a second
        // probe over that live instance or lose its only cleanup owner.
        if(next && next->runtime.instance()!=XR_NULL_HANDLE) {
            next->closing=true;state_=std::move(next);
        }
        return false;
    }
}
bool DisplayXrDesktop::configure_gpu_properties(SDL_PropertiesID props) const {
    if(!state_ || state_->closing || !props) return false;
    if(state_->runtime.backend()==DisplayXrBackend::vulkan)
        return state_->vulkan_creation.configure_gpu_properties(props);
    return state_ && props
        && SDL_SetBooleanProperty(props,"starfox.d3d12.require_adapter_luid",true)
        && SDL_SetNumberProperty(props,"starfox.d3d12.adapter_luid_low",state_->adapter.LowPart)
        && SDL_SetNumberProperty(props,"starfox.d3d12.adapter_luid_high",state_->adapter.HighPart);
}
bool DisplayXrDesktop::attach(SDL_Window* window,SDL_GPUDevice* device,CalibratedDlssApi dlss) {
    if(!state_ || state_->attached || !window || !device) {status_="Leia desktop is not ready to attach";return false;}
    try {
        const bool vulkan=state_->runtime.backend()==DisplayXrBackend::vulkan;
        if(std::string_view(SDL_GetGPUDeviceDriver(device))!=(vulkan?"vulkan":"direct3d12"))
            throw std::runtime_error("Native Leia desktop and SDL graphics backend differ");
        auto* hwnd=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
        if(!hwnd) throw std::runtime_error("Leia requires the actual desktop panel HWND");
        auto& s=*state_;s.window=window;
        if(!SDL_GetWindowPosition(window,&s.x,&s.y) || !SDL_GetWindowSize(window,&s.width,&s.height))
            throw std::runtime_error(SDL_GetError());
        const auto flags=SDL_GetWindowFlags(window);s.fullscreen=(flags&SDL_WINDOW_FULLSCREEN)!=0;
        s.bordered=(flags&SDL_WINDOW_BORDERLESS)==0;s.resizable=(flags&SDL_WINDOW_RESIZABLE)!=0;
        s.placed=true;
        if(!SDL_SetWindowFullscreen(window,false) || !SDL_SetWindowBordered(window,false) || !SDL_SetWindowResizable(window,false))
            throw std::runtime_error(SDL_GetError());
        const auto rect=s.runtime.panel().desktop_rect;
        // Update SDL's requested size before its non-resizable WM_GETMINMAXINFO
        // handling. A direct SetWindowPos leaves that size at the old canvas,
        // and Windows then silently clamps the native client area back to it.
        // Win32 SDL coordinates here are physical pixels; verify the resulting
        // HWND bounds rather than assuming the menu's resolution/DPI preset.
        if(!SDL_SetWindowSize(window,rect.extent.width,rect.extent.height)
            || !SDL_SetWindowPosition(window,rect.offset.x,rect.offset.y)
            || !SDL_SyncWindow(window)) throw std::runtime_error("Could not place Leia window on its physical panel");
        RECT client{};POINT origin{};
        if(!GetClientRect(hwnd,&client) || !ClientToScreen(hwnd,&origin)
            || origin.x!=rect.offset.x || origin.y!=rect.offset.y
            || client.right!=rect.extent.width || client.bottom!=rect.extent.height)
            throw std::runtime_error("Leia panel window has incompatible physical bounds/DPI: client="
                +std::to_string(origin.x)+","+std::to_string(origin.y)+","+std::to_string(client.right)+","+std::to_string(client.bottom)
                +" expected="+std::to_string(rect.offset.x)+","+std::to_string(rect.offset.y)+","+std::to_string(rect.extent.width)+","+std::to_string(rect.extent.height));
        if(!s.renderer.initialize(s.runtime,device,hwnd,vulkan?&s.vulkan_creation:nullptr,dlss)) throw std::runtime_error(s.renderer.status());
        s.attached=true;s.ready_since=Clock::now();++generation_;
        status_="Leia calibrated desktop presentation active";return true;
    } catch(const std::exception& error) {
        status_=error.what();
        if(state_->renderer.close()) state_->restore();return false;
    }
}
bool DisplayXrDesktop::active() const noexcept {return state_ && state_->attached;}
bool DisplayXrDesktop::frame_pending() const noexcept {return state_ && state_->renderer.frame_pending();}
bool DisplayXrDesktop::srgb_target() const noexcept {return state_ && state_->renderer.srgb_target();}
std::uint64_t DisplayXrDesktop::presented_frames() const noexcept {return state_?state_->renderer.presented_frames():0;}
bool DisplayXrDesktop::present(std::shared_ptr<const CalibratedGameFrame> frame,Clock::time_point now) {
    if(!active()) return false;
    auto& renderer=state_->renderer;
    // While either native image or SDL queue is waiting, do not locate a new
    // camera or overwrite the accepted frame with a newer simulation snapshot.
    if(renderer.frame_pending()) {
        const auto result=renderer.poll();status_=renderer.status();
        if(result==DisplayXrSubmission::waiting && state_->pending_since
            && now-*state_->pending_since>=std::chrono::seconds(5)) {
            status_="Leia native frame wait expired; pending resources must be drained";return false;
        }
        if(result!=DisplayXrSubmission::waiting) state_->pending_since.reset();
        return result!=DisplayXrSubmission::failed;
    }
    if(!renderer.poll_events() || renderer.exit_requested()) {status_=renderer.status();return false;}
    if(!renderer.running()) {
        if(!state_->ready_since) state_->ready_since=now;
        if(now-*state_->ready_since>=std::chrono::seconds(5)) {
            status_="Leia session did not become READY";return false;
        }
        status_="Waiting for Leia session READY";return true;
    }
    state_->ready_since.reset();
    const auto result=renderer.submit(std::move(frame));status_=renderer.status();
    if(result==DisplayXrSubmission::waiting) state_->pending_since=now;
    return result!=DisplayXrSubmission::failed;
}
}
