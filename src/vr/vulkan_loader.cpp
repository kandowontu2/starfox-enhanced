#include "starfox/vr/vulkan_loader.hpp"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace starfox::vr {
VulkanLoader::~VulkanLoader() {close();}
void VulkanLoader::close() noexcept {
    get_=nullptr;
    if(!library_) return;
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(library_));
#else
    dlclose(library_);
#endif
    library_=nullptr;
}
bool VulkanLoader::initialize() {
    close();
#if defined(_WIN32)
    // Load the installed driver loader, never an accidental working-dir DLL.
    library_=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(library_) get_=reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(static_cast<HMODULE>(library_),"vkGetInstanceProcAddr"));
#elif defined(__ANDROID__)
    library_=dlopen("libvulkan.so",RTLD_NOW|RTLD_LOCAL);
    if(library_) get_=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library_,"vkGetInstanceProcAddr"));
#else
    library_=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);
    if(library_) get_=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library_,"vkGetInstanceProcAddr"));
#endif
    if(!library_ || !get_) {status_="System Vulkan loader is unavailable or missing vkGetInstanceProcAddr";close();return false;}
    status_="System Vulkan loader ready";return true;
}
}
