#include "starfox/render/displayxr_runtime.hpp"
#include "starfox/render/displayxr_abi.hpp"
#include <openxr/openxr_loader_negotiation.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace starfox::render {
namespace {
void require(XrResult result,const char* operation) {
    if(XR_FAILED(result)) throw std::runtime_error(std::string(operation)+": OpenXR result "+std::to_string(result));
}
template<class Function>
Function function(PFN_xrGetInstanceProcAddr get,XrInstance instance,const char* name) {
    PFN_xrVoidFunction pointer{};
    require(get(instance,name,&pointer),name);
    if(!pointer) throw std::runtime_error(std::string("Missing runtime entry point: ")+name);
    return reinterpret_cast<Function>(pointer);
}
template<std::size_t Size>
std::string text(const char (&buffer)[Size]) {
    const auto end=std::find(buffer,buffer+Size,'\0');
    if(end==buffer+Size) throw std::runtime_error("Unterminated runtime identity");
    return {buffer,end};
}
bool positive(float value) {return std::isfinite(value) && value>0;}
#if defined(_WIN32)
std::filesystem::path installed_runtime() {
    HKEY key{};
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"Software\\DisplayXR\\Runtime",0,
        KEY_QUERY_VALUE|KEY_WOW64_64KEY,&key)!=ERROR_SUCCESS) return {};
    struct CloseKey { HKEY key;~CloseKey() {RegCloseKey(key);} } close{key};
    DWORD bytes{},type{};
    if(RegGetValueW(key,nullptr,L"InstallPath",RRF_RT_REG_SZ,&type,nullptr,&bytes)!=ERROR_SUCCESS
        || bytes<sizeof(wchar_t) || bytes>65536 || bytes%sizeof(wchar_t)) return {};
    std::vector<wchar_t> buffer(bytes/sizeof(wchar_t));
    if(RegGetValueW(key,nullptr,L"InstallPath",RRF_RT_REG_SZ,&type,buffer.data(),&bytes)!=ERROR_SUCCESS
        || !bytes || bytes>buffer.size()*sizeof(wchar_t) || bytes%sizeof(wchar_t)
        || buffer[bytes/sizeof(wchar_t)-1]!=L'\0') return {};
    return std::filesystem::path(buffer.data());
}
#endif
}

DisplayXrRuntime::~DisplayXrRuntime() {close();}
void DisplayXrRuntime::close() noexcept {
    // A broken dispatch/destroy must never cause a live instance's DLL to be
    // unloaded. Retain it for a later cleanup attempt (or process termination),
    // and prevent discovery from overwriting the still-owned handle/dispatch.
    // All normal and failed-discovery paths with a conforming runtime release.
    system_=XR_NULL_SYSTEM_ID;panel_={};views_={};runtime_name_.clear();system_name_.clear();status_.clear();
    if(instance_!=XR_NULL_HANDLE) {
        if(!destroy_ && get_) {
            PFN_xrVoidFunction pointer{};
            if(XR_SUCCEEDED(get_(instance_,"xrDestroyInstance",&pointer)) && pointer)
                destroy_=reinterpret_cast<PFN_xrDestroyInstance>(pointer);
        }
        if(!destroy_ || XR_FAILED(destroy_(instance_))) {cleanup_pending_=true;return;}
    }
    cleanup_pending_=false;
    instance_=XR_NULL_HANDLE;system_=XR_NULL_SYSTEM_ID;get_=nullptr;destroy_=nullptr;
#if defined(_WIN32)
    if(module_) FreeLibrary(static_cast<HMODULE>(module_));
#endif
    module_=nullptr;
}
bool DisplayXrRuntime::initialize(const std::filesystem::path& directory,DisplayXrBackend backend) {
    close();
    if(cleanup_pending_) return false;
    try {
#if defined(_WIN32)
        const auto base=directory.empty()?installed_runtime():directory;
        if(base.empty()) throw std::runtime_error("DisplayXR is not installed");
        const auto path=std::filesystem::absolute(base/"DisplayXRClient.dll");
        if(!std::filesystem::is_regular_file(path)) throw std::runtime_error("DisplayXRClient.dll not found in the selected runtime directory");
        module_=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!module_) throw std::runtime_error("Cannot load DisplayXR or its dependencies (Windows error "+std::to_string(GetLastError())+")");
        const auto negotiate=reinterpret_cast<PFN_xrNegotiateLoaderRuntimeInterface>(
            GetProcAddress(static_cast<HMODULE>(module_),"xrNegotiateLoaderRuntimeInterface"));
        if(!negotiate) throw std::runtime_error("DisplayXR lacks OpenXR runtime negotiation");
        XrNegotiateLoaderInfo loader{XR_LOADER_INTERFACE_STRUCT_LOADER_INFO,XR_LOADER_INFO_STRUCT_VERSION,sizeof(XrNegotiateLoaderInfo),
            1,XR_CURRENT_LOADER_RUNTIME_VERSION,XR_MAKE_VERSION(1,0,0),XR_MAKE_VERSION(1,1,0)};
        XrNegotiateRuntimeRequest request{XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST,XR_RUNTIME_INFO_STRUCT_VERSION,sizeof(XrNegotiateRuntimeRequest)};
        require(negotiate(&loader,&request),"Negotiate DisplayXR runtime");
        if(request.runtimeInterfaceVersion!=XR_CURRENT_LOADER_RUNTIME_VERSION || !request.getInstanceProcAddr
            || XR_VERSION_MAJOR(request.runtimeApiVersion)!=1 || request.runtimeApiVersion<loader.minApiVersion)
            throw std::runtime_error("Incompatible OpenXR runtime negotiation");
        get_=request.getInstanceProcAddr;
        return discover(backend);
#else
        (void)directory;(void)backend;
        throw std::runtime_error("Leia SR display integration currently targets desktop Windows");
#endif
    } catch(const std::exception& error) {
        const auto failure=std::string("Leia SR unavailable: ")+error.what();close();status_=failure;return false;
    }
}
bool DisplayXrRuntime::initialize_with_api(PFN_xrGetInstanceProcAddr get,DisplayXrBackend backend) {
    close();if(cleanup_pending_) return false;get_=get;return discover(backend);
}
bool DisplayXrRuntime::discover(DisplayXrBackend backend) {
    try {
        if(!get_) throw std::runtime_error("No DisplayXR dispatch");
        if(backend!=DisplayXrBackend::direct3d12 && backend!=DisplayXrBackend::vulkan)
            throw std::runtime_error("Unknown display graphics backend");
        const auto enumerate=function<PFN_xrEnumerateInstanceExtensionProperties>(get_,XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties");
        std::uint32_t count{};
        require(enumerate(nullptr,0,&count,nullptr),"Enumerate DisplayXR extensions");
        if(!count || count>4096) throw std::runtime_error("Invalid extension count");
        std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
        require(enumerate(nullptr,count,&count,extensions.data()),"Read DisplayXR extensions");
        if(!count || count>extensions.size()) throw std::runtime_error("Extension count changed during discovery");
        extensions.resize(count);
        const auto version=[&](const char* name) {
            for(const auto& extension:extensions) if(text(extension.extensionName)==name) return extension.extensionVersion;
            return std::uint32_t{};
        };
        const auto graphics=backend==DisplayXrBackend::direct3d12?"XR_KHR_D3D12_enable":"XR_KHR_vulkan_enable2";
        if(version(displayxr::display_info_extension)<18 || version(displayxr::view_rig_extension)<3
            || version(displayxr::window_extension)<8)
            throw std::runtime_error("DisplayXR lacks the required native display/rig/window interfaces");
        if(!version(graphics)) throw std::runtime_error(std::string("Runtime lacks ")+graphics);
        const std::array<const char*,4> enabled{displayxr::display_info_extension,displayxr::view_rig_extension,displayxr::window_extension,graphics};
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(create.applicationInfo.applicationName,"Star Fox Enhanced Leia SR");
        std::strcpy(create.applicationInfo.engineName,"Star Fox Enhanced");
        create.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        create.enabledExtensionCount=enabled.size();create.enabledExtensionNames=enabled.data();
        require(function<PFN_xrCreateInstance>(get_,XR_NULL_HANDLE,"xrCreateInstance")(&create,&instance_),"Create DisplayXR instance");
        if(instance_==XR_NULL_HANDLE) throw std::runtime_error("Runtime returned a null instance");
        destroy_=function<PFN_xrDestroyInstance>(get_,instance_,"xrDestroyInstance");
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        require(function<PFN_xrGetInstanceProperties>(get_,instance_,"xrGetInstanceProperties")(instance_,&properties),"Read DisplayXR identity");
        runtime_name_=text(properties.runtimeName);
        // Official builds append their git tag to CMAKE_PROJECT_DESCRIPTION.
        // Do not require the short project name: that rejects the real runtime.
        if(!runtime_name_.starts_with("DisplayXR Runtime "))
            throw std::runtime_error("Selected runtime is not DisplayXR");
        XrSystemGetInfo system{XR_TYPE_SYSTEM_GET_INFO};system.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        require(function<PFN_xrGetSystem>(get_,instance_,"xrGetSystem")(instance_,&system,&system_),"Find Leia SR display");
        if(system_==XR_NULL_SYSTEM_ID) throw std::runtime_error("Runtime returned a null display system");
        displayxr::DisplayInfo physical;
        displayxr::DesktopInfo desktop;
        physical.next=&desktop;
        XrSystemProperties display{XR_TYPE_SYSTEM_PROPERTIES};display.next=&physical;
        require(function<PFN_xrGetSystemProperties>(get_,instance_,"xrGetSystemProperties")(instance_,system_,&display),"Read physical display properties");
        system_name_=text(display.systemName);
        if(system_name_!="DisplayXR: Leia 3D Display" || desktop.panel_confirmed!=XR_TRUE)
            throw std::runtime_error("No confirmed physical Leia SR panel (simulated displays are not compatible hardware)");
        if(display.systemId!=system_ || !physical.pixel_width || !physical.pixel_height
            || physical.pixel_width>32768 || physical.pixel_height>32768
            || !positive(physical.display_size_meters.width) || !positive(physical.display_size_meters.height)
            || !positive(physical.view_scale_x) || !positive(physical.view_scale_y)
            || physical.view_scale_x>8 || physical.view_scale_y>8
            || !std::isfinite(physical.nominal_viewer.x) || !std::isfinite(physical.nominal_viewer.y)
            || !positive(physical.nominal_viewer.z) || desktop.rect.extent.width<=0 || desktop.rect.extent.height<=0)
            throw std::runtime_error("Invalid physical display dimensions or viewer geometry");
        const auto monitor=text(desktop.device_name);
        if(monitor.empty()) throw std::runtime_error("Physical panel has no OS monitor identity");
        const auto views=function<PFN_xrEnumerateViewConfigurationViews>(get_,instance_,"xrEnumerateViewConfigurationViews");
        require(views(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"Enumerate display eyes");
        if(count!=2) throw std::runtime_error("Display does not provide two stereo eyes");
        views_.fill({XR_TYPE_VIEW_CONFIGURATION_VIEW});
        require(views(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,views_.data()),"Read display eyes");
        if(count!=2) throw std::runtime_error("Stereo eye count changed during discovery");
        for(const auto& view:views_) if(!view.recommendedImageRectWidth || !view.recommendedImageRectHeight
            || view.recommendedImageRectWidth>32768 || view.recommendedImageRectHeight>32768
            || view.recommendedImageRectWidth>view.maxImageRectWidth || view.recommendedImageRectHeight>view.maxImageRectHeight
            || !view.recommendedSwapchainSampleCount || view.recommendedSwapchainSampleCount>view.maxSwapchainSampleCount)
            throw std::runtime_error("Invalid stereo swapchain recommendations");
        panel_={physical.pixel_width,physical.pixel_height,physical.display_size_meters,physical.nominal_viewer,
            {physical.view_scale_x,physical.view_scale_y},desktop.rect,monitor,desktop.primary==XR_TRUE};
        backend_=backend;
        status_="Leia SR physical display detected; graphics session/presentation not yet validated";
        return true;
    } catch(const std::exception& error) {
        const auto failure=std::string("Leia SR unavailable: ")+error.what();close();status_=failure;return false;
    }
}
}
