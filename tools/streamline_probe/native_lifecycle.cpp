#include "starfox/render/dlss_native.h"
#include <sl_security.h>
#include <sl.h>
#include <sl_helpers.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <unordered_map>
#include <wrl/client.h>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {
std::mutex lifecycle_mutex;
HMODULE active{};
Microsoft::WRL::ComPtr<ID3D12Device> bound;
std::wstring plugin_directory;
std::unordered_map<void*,Microsoft::WRL::ComPtr<IDXGISwapChain3>> swapchains;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
void check(sl::Result r,const char* message) {
    if(r!=sl::Result::eOk) throw std::runtime_error(std::string(message)+": "+sl::getResultAsStr(r));
}
template<class T> T* api(HMODULE module,const char* name) {
    auto fn=reinterpret_cast<T*>(GetProcAddress(module,name));check(fn!=nullptr,name);return fn;
}
template<class F> int guarded(F&& call,char* error,uint32_t capacity) noexcept {
    if(error && capacity) error[0]=0;
    try {std::lock_guard lock(lifecycle_mutex);call();return 0;}
    catch(const std::exception& e) {
        if(error && capacity) {auto n=std::min<size_t>(std::strlen(e.what()),capacity-1);std::memcpy(error,e.what(),n);error[n]=0;}
    } catch(...) {}
    return 1;
}
bool trusted_runtime(const std::filesystem::path& path) {
    WINTRUST_FILE_INFO file{};file.cbStruct=sizeof(file);file.pcwszFilePath=path.c_str();
    WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;
    data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;
    GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    auto result=WinVerifyTrust(nullptr,&policy,&data);
    data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&policy,&data);
    return result==ERROR_SUCCESS;
}
void require_active(void* module) {check(active && module==active,"Invalid DLSS lifecycle handle");}
}
int starfox_dlss_open_v1(const wchar_t* directory,void** result,char* error,uint32_t capacity) {
    if(result) *result=nullptr;
    return guarded([&] {
        check(directory && result && !active,"Invalid or duplicate DLSS initialization");
        auto path=std::filesystem::path(directory);check(path.is_absolute(),"DLSS runtime path must be absolute");
        path=std::filesystem::canonical(path);
        for(const auto* name:{L"sl.interposer.dll",L"sl.common.dll",L"sl.dlss.dll"})
            check(sl::security::verifyEmbeddedSignature((path/name).c_str()),"Rejected Streamline signature");
        check(trusted_runtime(path/L"nvngx_dlss.dll"),"Rejected DLSS runtime signature");
        struct Module {HMODULE value{};~Module(){if(value) FreeLibrary(value);}} loaded;
        loaded.value=LoadLibraryExW((path/L"sl.interposer.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(loaded.value!=nullptr,"Secure DLSS runtime load failed");
        // Resolve shutdown before initializing, so every successful init can unwind.
        api<PFun_slShutdown>(loaded.value,"slShutdown");
        plugin_directory=path.wstring();const wchar_t* paths[]{plugin_directory.c_str()};
        const sl::Feature features[]{sl::kFeatureDLSS};sl::Preferences preferences{};
        preferences.pathsToPlugins=paths;preferences.numPathsToPlugins=1;
        preferences.featuresToLoad=features;preferences.numFeaturesToLoad=1;
        preferences.engine=sl::EngineType::eCustom;preferences.engineVersion="StarFoxEnhanced";
        preferences.projectId="649d1a1a-4051-4280-8b0d-149553bac7c5";
        preferences.renderAPI=sl::RenderAPI::eD3D12;
        preferences.flags=sl::PreferenceFlags::eDisableCLStateTracking|sl::PreferenceFlags::eUseManualHooking|sl::PreferenceFlags::eUseFrameBasedResourceTagging;
        preferences.logLevel=sl::LogLevel::eDefault;
        preferences.logMessageCallback=[](sl::LogType type,const char* message) {
            if(type==sl::LogType::eError || type==sl::LogType::eWarn)
                std::fprintf(stderr,"dlss-sdk-%s: %s\n",type==sl::LogType::eError?"error":"warning",message?message:"");
        };
        check(api<PFun_slInit>(loaded.value,"slInit")(preferences,sl::kSDKVersion),"DLSS initialization");
        active=loaded.value;loaded.value=nullptr;*result=active;
    },error,capacity);
}
int starfox_dlss_bind_device_v1(void* module,void* device,char* error,uint32_t capacity) {
    return guarded([&] {
        require_active(module);check(device!=nullptr,"Null DLSS device");
        auto* native=static_cast<ID3D12Device*>(device);
        check(!bound || bound.Get()==native,"DLSS device change requires shutdown");
        if(bound.Get()==native) return;
        auto luid=native->GetAdapterLuid();sl::AdapterInfo info{};
        info.deviceLUID=reinterpret_cast<uint8_t*>(&luid);info.deviceLUIDSizeInBytes=sizeof(luid);
        check(api<PFun_slIsFeatureSupported>(active,"slIsFeatureSupported")(sl::kFeatureDLSS,info),"DLSS adapter support");
        check(api<PFun_slSetD3DDevice>(active,"slSetD3DDevice")(native),"Bind DLSS device");
        bound=native;
    },error,capacity);
}
int starfox_dlss_release_viewport_v1(void* module,uint32_t viewport,char* error,uint32_t capacity) {
    return guarded([&] {require_active(module);
        check(api<PFun_slFreeResources>(active,"slFreeResources")(sl::kFeatureDLSS,sl::ViewportHandle{viewport}),"Release DLSS viewport");
    },error,capacity);
}
int starfox_dlss_finish_frame_v1(void* module,char* error,uint32_t capacity) {
    return guarded([&] {
        require_active(module);check(bound.Get()!=nullptr,"Bind device before SDK frame-end");
        // Streamline 2.14.1 exports these hooks from sl.common through its
        // normal feature-function gateway. Their signatures and nullptr-safe
        // swapchain handling are defined in commonEntry/commonInterface.cpp.
        // Only the common plugin is requested: no DLSS-G/DXGI presentation,
        // replacement swapchain, private symbol or second SDK is involved.
        using Before=HRESULT(IDXGISwapChain*,UINT,UINT,bool&);
        using After=HRESULT(UINT);
        void *before{},*after{};
        auto get=api<PFun_slGetFeatureFunction>(active,"slGetFeatureFunction");
        check(get(sl::kFeatureCommon,"slHookPresent",before),"SDK common before-present hook");
        check(get(sl::kFeatureCommon,"slHookAfterPresent",after),"SDK common after-present hook");
        check(before && after,"SDK frame-end hooks unavailable");
        bool skip=false;
        check(SUCCEEDED(reinterpret_cast<Before*>(before)(nullptr,0,0,skip)) && !skip,
            "SDK common before-present failed");
        check(SUCCEEDED(reinterpret_cast<After*>(after)(0)),"SDK common after-present failed");
    },error,capacity);
}
int starfox_dlss_close_v1(void* module,char* error,uint32_t capacity) {
    return guarded([&] {require_active(module);
        check(swapchains.empty(),"Restore DLSS swapchains before shutdown");
        check(api<PFun_slShutdown>(active,"slShutdown")(),"DLSS shutdown");
        bound.Reset();FreeLibrary(active);active=nullptr;plugin_directory.clear();
    },error,capacity);
}
int starfox_dlss_swapchain_v1(void* module,void** swap,uint32_t restore,char* error,uint32_t capacity) {
    return guarded([&] {
        require_active(module);check(swap && *swap && restore<=1,"Invalid DLSS swapchain");
        const auto found=swapchains.find(*swap);
        if(restore) {
            if(found==swapchains.end()) return;
            auto* proxy=static_cast<IDXGISwapChain3*>(*swap);
            *swap=found->second.Detach();swapchains.erase(found);proxy->Release();return;
        }
        if(found!=swapchains.end()) return;
        check(bound.Get()!=nullptr,"Bind device before upgrading swapchain");
        // Allocate tracking storage before changing the caller's owned pointer.
        auto [entry,inserted]=swapchains.emplace(*swap,static_cast<IDXGISwapChain3*>(*swap));
        check(inserted,"Swapchain tracking collision");
        void* upgraded=*swap;
        const auto r=api<PFun_slUpgradeInterface>(active,"slUpgradeInterface")(&upgraded);
        if(r!=sl::Result::eOk) {swapchains.erase(entry);check(r,"Upgrade DLSS swapchain");}
        auto node=swapchains.extract(entry);node.key()=upgraded;swapchains.insert(std::move(node));
        // Upgrade adds the proxy's native reference; it does not consume the
        // caller's previous native reference. Tracking owns its separate ref.
        if(upgraded!=*swap) static_cast<IDXGISwapChain3*>(*swap)->Release();
        *swap=upgraded;
    },error,capacity);
}
