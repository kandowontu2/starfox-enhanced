#include "starfox/render/leia_sr_api.h"
#include <windows.h>
#include <d3d12.h>
#include "sr/management/srcontext.h"
#include "sr/weaver/dx12weaver.h"
#define SRDISPLAY_LAZYBINDING
#include "sr/world/display/display.h"
#include "sr/sense/display/switchablehint.h"
#include <cstdio>
#include <exception>
#include <limits>
#include <vector>

namespace {
char status_text[512]{"SR adapter not requested"};
void status(const char* message,unsigned long error=0) noexcept {
    if(error) std::snprintf(status_text,sizeof(status_text),"%s (Windows error %lu)",message,error);
    else std::snprintf(status_text,sizeof(status_text),"%s",message?message:"Unknown SR error");
}
struct Host {
    SR::SRContext* context{};
    SR::IDX12Weaver1* sr{};
    HWND window{};
    HMONITOR monitor{};
    SR::SwitchableLensHint* lens{}; // context-owned; never delete directly
    bool lens_tried{},lens_enabled{};
};
struct RuntimePreflight {
    HMODULE directx{},core{};
    ~RuntimePreflight() {if(core) FreeLibrary(core);if(directx) FreeLibrary(directx);}
};
// Calls are serialized on the present thread. If a partially initialized
// SDK context cannot be destroyed, keep it reachable and reject new hosts.
Host* failed_create{};
HMONITOR compatible_monitor(HWND window) {
    const auto target=MonitorFromWindow(window,MONITOR_DEFAULTTONULL);
    if(!target) {status("Game window is not on an active monitor");return nullptr;}
    // Do not pre-qualify the panel against the SDK's EDID/model utility list.
    // It can reject panels supported by the installed service, and imports
    // extra utilities missing from older runtimes. Let the real SDK factory
    // and (when available) the runtime DisplayManager qualify hardware.
    MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(target,&monitor)) {
        UINT32 path_count{},mode_count{};
        if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&path_count,&mode_count)==ERROR_SUCCESS) {
            std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
            std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
            if(QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&path_count,paths.data(),&mode_count,modes.data(),nullptr)==ERROR_SUCCESS) {
                unsigned targets{};
                for(UINT32 i=0;i<path_count;++i) {
                    DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
                    source.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                    source.header.size=sizeof(source);source.header.adapterId=paths[i].sourceInfo.adapterId;
                    source.header.id=paths[i].sourceInfo.id;
                    if(DisplayConfigGetDeviceInfo(&source.header)==ERROR_SUCCESS
                        && wcscmp(source.viewGdiDeviceName,monitor.szDevice)==0) ++targets;
                }
                if(targets>1) {status("SR panel is in Windows duplicate/clone mode");return nullptr;}
            }
        }
    }
    return target;
}
bool runtime_display_matches(Host& host) {
    // Optional/lazy interface: an older runtime follows the same successful
    // factory path as SR-lib, rather than failing on a newer missing export.
    auto* displays=SR::TryGetDisplayManagerInstance(*host.context);
    if(!displays) return true;
    auto* display=displays->getPrimaryActiveSRDisplay();
    if(!display || !display->isValid()) {status("SR service reports no active compatible display");return false;}
    const auto area=display->getLocation();
    const auto minimum=std::numeric_limits<LONG>::min(),maximum=std::numeric_limits<LONG>::max();
    if(area.left<minimum || area.top<minimum || area.right>maximum || area.bottom>maximum
        || area.right<=area.left || area.bottom<=area.top) {status("SR service returned invalid display coordinates");return false;}
    const RECT rectangle{LONG(area.left),LONG(area.top),LONG(area.right),LONG(area.bottom)};
    if(MonitorFromRect(&rectangle,MONITOR_DEFAULTTONULL)!=host.monitor) {
        status("Game window is not on the runtime's active SR panel");return false;
    }
    return true;
}
int destroy(void* opaque) noexcept {
    auto* host=opaque?static_cast<Host*>(opaque):failed_create;
    if(!host) return 1;
    if(host->lens && host->lens_enabled) {
        // A failed disable must not prevent context-owned preference cleanup.
        try {host->lens->disable();host->lens_enabled=false;} catch(...) {}
    }
    if(host->sr) {try {host->sr->destroy();host->sr=nullptr;} catch(...) {return 0;}}
    if(host->context) {try {SR::SRContext::deleteSRContext(host->context);host->context=nullptr;} catch(...) {return 0;}}
    if(host==failed_create) failed_create=nullptr;
    delete host;
    return 1;
}
void discard_failed(Host*& host) noexcept {
    if(host && !destroy(host)) failed_create=host;
    host=nullptr;
}
void* create(void* device,void* hwnd) noexcept {
    if(failed_create) {status("Prior SR factory teardown is still pending");return nullptr;}
    if(!device || !hwnd) {status("SR factory needs an actual D3D12 device and window");return nullptr;}
    DWORD previous{};
    SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,&previous);
    Host* host=nullptr;
    try {
        // Match SR-lib's preflight before touching any delay-loaded SDK entry.
        // DirectX loads its Core/Displays/FaceTrackers/OpenCV dependency chain.
        RuntimePreflight runtime;
        runtime.directx=LoadLibraryW(L"SimulatedRealityDirectX.dll");
        if(!runtime.directx) {
            status("Cannot load SimulatedRealityDirectX.dll or its runtime dependencies",GetLastError());
            SetThreadErrorMode(previous,nullptr);return nullptr;
        }
        runtime.core=LoadLibraryW(L"SimulatedRealityCore.dll");
        if(!runtime.core) {
            status("Cannot load SimulatedRealityCore.dll or its runtime dependencies",GetLastError());
            SetThreadErrorMode(previous,nullptr);return nullptr;
        }
        // Factory/context order matches the working SR-lib integration.
        if(const auto monitor=compatible_monitor(static_cast<HWND>(hwnd))) {
            host=new Host;
            host->window=static_cast<HWND>(hwnd);host->monitor=monitor;
            host->context=SR::SRContext::create();
            if(!host->context) {status("SR runtime/service returned no context");discard_failed(host);}
            else {
                const auto result=SR::CreateDX12Weaver(host->context,static_cast<ID3D12Device*>(device),host->window,&host->sr);
                if(result!=WeaverSuccess || !host->sr) {
                    std::snprintf(status_text,sizeof(status_text),"SR D3D12 weaver creation failed (SDK result %d)",int(result));
                    discard_failed(host);SetThreadErrorMode(previous,nullptr);return nullptr;
                }
                host->sr->setLatencyInFrames(1);host->sr->enableLateLatching(true);
                host->sr->setShaderSRGBConversion(false,false);
                // Eye tracking must start after constructing the weaver.
                host->context->initialize();
                if(!runtime_display_matches(*host)) discard_failed(host);
                else status("SR weaver/context initialized; presentation not yet confirmed");
            }
        }
    } catch(const std::exception& error) {
        std::snprintf(status_text,sizeof(status_text),"SR initialization exception: %s",error.what());discard_failed(host);
    } catch(...) {status("SR initialization threw a vendor/delay-load exception");discard_failed(host);}
    SetThreadErrorMode(previous,nullptr);
    return host;
}
int weave(void* opaque,void* command,void* source,uint32_t width,uint32_t height,uint32_t format) noexcept {
    auto* host=static_cast<Host*>(opaque);
    if(!host || !command || !source || !width || !height || (width&1) || (height&1)
        || format!=DXGI_FORMAT_B8G8R8A8_UNORM) {status("SR weave received incompatible host/list/texture extent or format");return 0;}
    // Re-arm on a different panel rather than weaving with stale calibration.
    if(MonitorFromWindow(host->window,MONITOR_DEFAULTTONULL)!=host->monitor) {status("Game window moved away from the initialized SR panel");return 0;}
    try {
        const auto desc=static_cast<ID3D12Resource*>(source)->GetDesc();
        if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM || desc.Width!=uint64_t(width)*2 || desc.Height!=height) {
            status("SR input must be the app-owned BGRA8 full stereo pair");return 0;
        }
        // Switchable panels need an app-owned 3D preference. Fixed-lens panels
        // normally throw here; try once after initialize(), not each frame.
        if(!host->lens_tried) {
            host->lens_tried=true;
            try {host->lens=SR::SwitchableLensHint::create(*host->context);} catch(...) {}
        }
        if(host->lens && !host->lens_enabled) {host->lens->enable();host->lens_enabled=true;}
        host->sr->setInputViewTexture(static_cast<ID3D12Resource*>(source),int(width*2),int(height),DXGI_FORMAT_B8G8R8A8_UNORM);
        host->sr->setOutputFormat(DXGI_FORMAT_B8G8R8A8_UNORM);
        const D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};
        const D3D12_RECT scissor{0,0,LONG(width),LONG(height)};
        host->sr->setCommandList(static_cast<ID3D12GraphicsCommandList*>(command));
        host->sr->setViewport(viewport);host->sr->setScissorRect(scissor);host->sr->weave();
        status("SR weave recorded; SDL presentation not yet confirmed");
        return 1;
    } catch(const std::exception& error) {
        std::snprintf(status_text,sizeof(status_text),"SR weave exception: %s",error.what());return 0;
    } catch(...) {status("SR weave threw a vendor/delay-load exception");return 0;}
}
const StarfoxLeiaSrApiV1 api{1,sizeof(StarfoxLeiaSrApiV1),create,weave,destroy};
}
extern "C" __declspec(dllexport) const StarfoxLeiaSrApiV1* starfox_leia_sr_get_api(uint32_t version) noexcept {
    return version==1?&api:nullptr;
}
extern "C" __declspec(dllexport) const char* starfox_leia_sr_status() noexcept {return status_text;}
