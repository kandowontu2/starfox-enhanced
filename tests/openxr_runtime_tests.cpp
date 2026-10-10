#include "starfox/vr/openxr_runtime.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
unsigned creates=0,destroys=0;bool fail_system=false,graphics=true,frame_extension=false,refresh_extension=false,frame_enabled=false;unsigned eye_count=2;
bool change_eye_count=false;
void check(bool value){if(!value) throw std::runtime_error("OpenXR runtime lifecycle assertion failed");}
}
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char*,uint32_t capacity,uint32_t* count,XrExtensionProperties* out) {
    // Advertised whether or not the host enables them.
    *count=graphics?1U+(frame_extension?1U:0U)+(refresh_extension?1U:0U):0U;
    if(capacity && graphics) {
        unsigned next=0;
        std::strcpy(out[next++].extensionName,"XR_KHR_vulkan_enable2");
        if(frame_extension) std::strcpy(out[next++].extensionName,"XR_VALVE_frame_controller_interaction");
        if(refresh_extension) std::strcpy(out[next++].extensionName,"XR_FB_display_refresh_rate");
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* info,XrInstance* out) {
    // Only the Frame player asks for the Frame extension.
    const bool frame_on=frame_extension && frame_enabled,refresh_on=refresh_extension && frame_enabled;
    check(info->enabledExtensionCount==1U+(frame_on?1U:0U)+(refresh_on?1U:0U)
        && std::strcmp(info->enabledExtensionNames[0],"XR_KHR_vulkan_enable2")==0);
    bool saw_frame=false,saw_refresh=false;
    for(unsigned i=1;i<info->enabledExtensionCount;++i) {
        saw_frame|=std::strcmp(info->enabledExtensionNames[i],"XR_VALVE_frame_controller_interaction")==0;
        saw_refresh|=std::strcmp(info->enabledExtensionNames[i],"XR_FB_display_refresh_rate")==0;
    }
    check(saw_frame==frame_on && saw_refresh==refresh_on);
    ++creates;*out=reinterpret_cast<XrInstance>(uintptr_t(1));return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(XrInstance instance) {check(instance!=XR_NULL_HANDLE);++destroys;return XR_SUCCESS;}
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(XrInstance,XrInstanceProperties* out) {std::strcpy(out->runtimeName,"test runtime");return XR_SUCCESS;}
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(XrInstance,const XrSystemGetInfo* info,XrSystemId* out) {
    check(info->formFactor==XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY);if(fail_system) return XR_ERROR_FORM_FACTOR_UNAVAILABLE;*out=9;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(XrInstance,XrSystemId,XrViewConfigurationType type,uint32_t capacity,uint32_t* count,XrViewConfigurationView* out) {
    check(type==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);*count=(capacity && change_eye_count)?1:eye_count;
    for(unsigned i=0;i<capacity;++i) out[i].recommendedImageRectWidth=1024;return XR_SUCCESS;
}
}
int main()try {
    using namespace starfox::vr;
    {
        OpenXrRuntime runtime;check(runtime.initialize());check(runtime.views().size()==2 && runtime.system()==9 && runtime.supports_vulkan()
            && !runtime.supports_frame_controller_interaction());
        check(runtime.initialize() && creates==2 && destroys==1);
        fail_system=true;check(!runtime.initialize());check(creates==3 && destroys==3);
        check(runtime.instance()==XR_NULL_HANDLE && runtime.system()==XR_NULL_SYSTEM_ID && runtime.views().empty() && !runtime.supports_vulkan());
        fail_system=false;eye_count=1;check(!runtime.initialize() && creates==4 && destroys==4);
        eye_count=2;graphics=false;check(!runtime.initialize() && creates==4);graphics=true;
        AndroidXrContext context;check(!runtime.initialize(&context) && creates==4);
        check(runtime.initialize());
        change_eye_count=true;check(!runtime.initialize());
        check(runtime.instance()==XR_NULL_HANDLE && runtime.views().empty() && !runtime.supports_vulkan());
        change_eye_count=false;check(runtime.initialize());
        frame_extension=true;
        // Advertised, but not enabled unless the host is the Frame player.
        check(runtime.initialize() && !runtime.supports_frame_controller_interaction());
        refresh_extension=true;
        check(runtime.initialize() && !runtime.supports_frame_controller_interaction()
            && !runtime.supports_display_refresh_rate());
        runtime.set_frame_extensions(true);frame_enabled=true;
        check(runtime.initialize() && runtime.supports_frame_controller_interaction()
            && runtime.supports_display_refresh_rate());
    }
    check(creates==10 && destroys==10);std::cout<<"OpenXR runtime initialization, optional Frame extension, failure cleanup, reinitialization and destruction passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
