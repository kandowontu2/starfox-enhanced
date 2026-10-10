#include "starfox/vr/openxr_runtime.hpp"
#if defined(__ANDROID__)
#include <jni.h>
#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#include <openxr/openxr_platform.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace starfox::vr {
namespace {
void require(XrResult result,const char* operation) {
    if(result==XR_ERROR_RUNTIME_UNAVAILABLE)
        throw std::runtime_error("No active OpenXR runtime. Install and select your headset's OpenXR runtime, then retry.");
    if(XR_FAILED(result)) throw std::runtime_error(std::string(operation)+": OpenXR result "+std::to_string(result));
}
}
OpenXrRuntime::~OpenXrRuntime() {if(instance_!=XR_NULL_HANDLE) xrDestroyInstance(instance_);}
bool OpenXrRuntime::initialize(const AndroidXrContext* android) {
    if(instance_!=XR_NULL_HANDLE) {xrDestroyInstance(instance_);instance_=XR_NULL_HANDLE;}
    system_=XR_NULL_SYSTEM_ID;views_.clear();vulkan_=false;
    frame_controller_interaction_=false;display_refresh_rate_=false;
    try {
#if defined(__ANDROID__)
        if(!android || !android->java_vm || !android->application_context || !android->activity)
            throw std::runtime_error("Android OpenXR requires a Java VM, application context and Activity");
        PFN_xrVoidFunction initialize_function=nullptr;
        require(xrGetInstanceProcAddr(XR_NULL_HANDLE,"xrInitializeLoaderKHR",&initialize_function),"Find Android OpenXR loader initialization");
        if(!initialize_function) throw std::runtime_error("Android OpenXR loader initialization is unavailable");
        XrLoaderInitInfoAndroidKHR loader_info{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loader_info.applicationVM=android->java_vm;loader_info.applicationContext=android->application_context;
        require(reinterpret_cast<PFN_xrInitializeLoaderKHR>(initialize_function)(
            reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loader_info)),"Initialize Android OpenXR loader");
#else
        if(android) throw std::runtime_error("Android OpenXR context supplied to a desktop build");
#endif
        std::uint32_t count=0;
        require(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr),"Enumerate OpenXR extensions");
        std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
        require(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,extensions.data()),"Read OpenXR extensions");
        vulkan_=std::any_of(extensions.begin(),extensions.end(),[](const auto& extension) {
            return std::strcmp(extension.extensionName,"XR_KHR_vulkan_enable2")==0;
        });
        frame_controller_interaction_=frame_extensions_ && std::any_of(
            extensions.begin(),extensions.end(),[](const auto& extension) {
                return std::strcmp(extension.extensionName,
                    "XR_VALVE_frame_controller_interaction")==0;
            });
        display_refresh_rate_=frame_extensions_ && std::any_of(extensions.begin(),extensions.end(),[](const auto& extension) {
            return std::strcmp(extension.extensionName,XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME)==0;
        });
        std::vector<const char*> enabled_extensions{"XR_KHR_vulkan_enable2"};
        if(display_refresh_rate_) enabled_extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
        if(!vulkan_) throw std::runtime_error("OpenXR runtime lacks XR_KHR_vulkan_enable2");
        if(frame_controller_interaction_)
            enabled_extensions.push_back("XR_VALVE_frame_controller_interaction");
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
#if defined(__ANDROID__)
        if(std::none_of(extensions.begin(),extensions.end(),[](const auto& extension) {
            return std::strcmp(extension.extensionName,XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME)==0;
        })) throw std::runtime_error("OpenXR runtime lacks XR_KHR_android_create_instance");
        enabled_extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
        XrInstanceCreateInfoAndroidKHR android_create{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
        android_create.applicationVM=android->java_vm;android_create.applicationActivity=android->activity;
        create.next=&android_create;
#endif
        std::snprintf(create.applicationInfo.applicationName,XR_MAX_APPLICATION_NAME_SIZE,"Star Fox Enhanced VR");
        std::snprintf(create.applicationInfo.engineName,XR_MAX_ENGINE_NAME_SIZE,"Star Fox Enhanced");
        create.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        create.enabledExtensionCount=static_cast<std::uint32_t>(enabled_extensions.size());create.enabledExtensionNames=enabled_extensions.data();
        require(xrCreateInstance(&create,&instance_),"Create OpenXR instance");
        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        require(xrGetInstanceProperties(instance_,&properties),"Read OpenXR runtime");
        XrSystemGetInfo get{XR_TYPE_SYSTEM_GET_INFO};get.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        require(xrGetSystem(instance_,&get,&system_),"Find VR headset");
        require(xrEnumerateViewConfigurationViews(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"Enumerate stereo views");
        if(count!=2) throw std::runtime_error("VR runtime did not provide two primary stereo views");
        views_.assign(count,{XR_TYPE_VIEW_CONFIGURATION_VIEW});
        require(xrEnumerateViewConfigurationViews(instance_,system_,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,views_.data()),"Read stereo views");
        if(count!=2) throw std::runtime_error("VR runtime changed its primary stereo view count during initialization");
        status_=std::string("OpenXR ready: ")+properties.runtimeName;
        return true;
    } catch(const std::exception& error) {
        status_=error.what();
        if(instance_!=XR_NULL_HANDLE) {xrDestroyInstance(instance_);instance_=XR_NULL_HANDLE;}
        system_=XR_NULL_SYSTEM_ID;views_.clear();vulkan_=false;
        frame_controller_interaction_=false;display_refresh_rate_=false;return false;
    }
}
}
