#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace starfox::render {
enum class RendererBackend : std::uint8_t { automatic,vulkan,direct3d12,direct3d11,metal,opengles2 };
inline constexpr std::array<std::string_view,6> renderer_backend_names{
    "AUTO","VULKAN","DIRECT3D 12","DIRECT3D 11","METAL","OPENGL ES"};
inline std::span<const RendererBackend> renderer_backend_choices() noexcept {
#if defined(STARFOX_UWP)
    static constexpr std::array choices{RendererBackend::automatic,RendererBackend::direct3d11};
#elif defined(_WIN32)
    static constexpr std::array choices{RendererBackend::automatic,RendererBackend::vulkan,RendererBackend::direct3d12,RendererBackend::direct3d11};
#elif defined(__APPLE__)
    static constexpr std::array choices{RendererBackend::automatic,RendererBackend::metal};
#elif defined(__ANDROID__)
    static constexpr std::array choices{RendererBackend::automatic,RendererBackend::vulkan,RendererBackend::opengles2};
#elif defined(__linux__)
    static constexpr std::array choices{RendererBackend::automatic,RendererBackend::vulkan};
#else
    static constexpr std::array choices{RendererBackend::automatic};
#endif
    return choices;
}
inline bool renderer_backend_supported(RendererBackend backend) noexcept {
    for(auto choice:renderer_backend_choices()) if(choice==backend) return true;
    return false;
}
inline RendererBackend cycle_renderer_backend(RendererBackend backend,bool previous) noexcept {
    const auto choices=renderer_backend_choices();
    for(std::size_t i=0;i<choices.size();++i) if(choices[i]==backend)
        return choices[(i+choices.size()+(previous?-1:1))%choices.size()];
    return RendererBackend::automatic;
}
inline const char* renderer_backend_driver(RendererBackend backend) noexcept {
    switch(backend) {
    case RendererBackend::vulkan:return "vulkan";
    case RendererBackend::direct3d12:return "direct3d12";
    case RendererBackend::metal:return "metal";
    default:return nullptr; // D3D11/GLES are SDL renderers, not SDL GPU drivers.
    }
}
// Desktop Windows AUTO selection. An explicit GPU backend wins; explicit
// SDL_GPU_DRIVER environment overrides still win at SDL_HINT_NORMAL priority.
inline const char* windows_gpu_driver_preference(RendererBackend backend,
    bool sr_platform,bool intel_d3d12,bool neural_d3d12) noexcept {
    if(const auto* explicit_driver=renderer_backend_driver(backend)) return explicit_driver;
    return sr_platform || intel_d3d12 || neural_d3d12?"direct3d12":"vulkan";
}
// A menu selection is transactional: do not abandon a working direct-SDK or
// ordinary stereo output merely because an optional runtime cannot connect.
// Failed initial selections stop retrying; established sessions still use the
// separate reconnect policy after a subsequent tracking/runtime interruption.
struct DisplayXrMenuSelection {
    unsigned stereo_output;
    bool requested;
    bool unavailable;
};
inline constexpr DisplayXrMenuSelection displayxr_menu_selection(unsigned previous_output,
    bool requested,bool connected) noexcept {
    if(!requested) return {previous_output,false,false};
    if(!connected) return {previous_output,false,true};
    return {previous_output==9U?0U:previous_output,true,false};
}
// Record a selection without probing a device or rebuilding the Preview-OFF
// menu. A deferred change is consumed once when a scene actually needs it.
class SrPlatformBackendRequest {
public:
    bool update(bool selected,bool scene_visible,bool may_refresh) noexcept {
        if(selected_!=selected) {selected_=selected;dirty_=true;}
        if(!scene_visible || !dirty_) return false;
        dirty_=false;
        return may_refresh;
    }
    bool selected() const noexcept {return selected_;}
    void renderer_created() noexcept {dirty_=false;}
private:
    bool selected_{},dirty_{};
};
}
