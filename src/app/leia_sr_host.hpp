#pragma once
#include "starfox/render/leia_sr_api.h"
#include <cstdint>
#include <cstdio>
#include <string_view>
#if defined(STARFOX_LEIASR_HOST)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#include <filesystem>
#endif

class LeiaSrHost {
public:
    LeiaSrHost()=default;
    LeiaSrHost(const LeiaSrHost&)=delete;
    LeiaSrHost& operator=(const LeiaSrHost&)=delete;
    ~LeiaSrHost() {release();
#if defined(STARFOX_LEIASR_HOST)
        if(!host_ && !unavailable_) unload();
#endif
    }
    bool ensure(void* device,void* hwnd) noexcept {
#if defined(STARFOX_LEIASR_HOST)
        if(host_) return !unavailable_;
        if(unavailable_ || !device || !hwnd) return false;
        unavailable_=true;
        try {
            if(!module_) {
                // Never search the working directory or PATH for our adapter.
                wchar_t path[32768];
                const auto count=GetModuleFileNameW(nullptr,path,32768);
                if(!count || count>=32768) {set_status("Cannot locate executable directory",GetLastError());return false;}
                const auto module_path=std::filesystem::path(path).parent_path()/L"starfox_leia_sr.dll";
                module_=LoadLibraryExW(module_path.c_str(),nullptr,
                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
                if(!module_) {set_status("Cannot load starfox_leia_sr.dll or its dependencies",GetLastError());return false;}
                const auto get=reinterpret_cast<StarfoxLeiaSrGetApi>(GetProcAddress(module_,"starfox_leia_sr_get_api"));
                api_=get?get(1):nullptr;
                get_status_=reinterpret_cast<StarfoxLeiaSrGetStatus>(GetProcAddress(module_,"starfox_leia_sr_status"));
                if(!api_ || api_->version!=1 || api_->size<sizeof(StarfoxLeiaSrApiV1)
                    || !api_->create || !api_->weave || !api_->destroy) {set_status("Incompatible SR adapter C ABI");unload();return false;}
            }
            host_=api_->create(device,hwnd);
            if(!host_) {set_status(get_status_?get_status_():"SR adapter initialization failed (legacy adapter has no diagnostics)");return false;}
            unavailable_=false;
            set_status("SR weaver initialized; presentation not yet confirmed");
            return true;
        } catch(...) {set_status("SR adapter loader/factory threw an exception");unload();return false;}
#else
        (void)device;(void)hwnd;return false;
#endif
    }
    std::string_view status() const noexcept {
#if defined(STARFOX_LEIASR_HOST)
        return status_;
#else
        return "SR Platform not included in this platform/build";
#endif
    }
    bool available() const noexcept {
#if defined(STARFOX_LEIASR_HOST)
        return host_!=nullptr && !unavailable_;
#else
        return false;
#endif
    }
    bool weave(void* command,void* source,std::uint32_t width,std::uint32_t height,std::uint32_t format) noexcept {
#if defined(STARFOX_LEIASR_HOST)
        if(!host_ || unavailable_) return false;
        if(api_->weave(host_,command,source,width,height,format)!=0) return true;
        set_status(get_status_?get_status_():"SR weave failed (legacy adapter has no diagnostics)");
        return false;
#else
        (void)command;(void)source;(void)width;(void)height;(void)format;return false;
#endif
    }
    void release() noexcept {
#if defined(STARFOX_LEIASR_HOST)
        if(api_ && !api_->destroy(host_)) {set_status("SR vendor teardown failed; resources retained");unavailable_=true;return;}
        // Retain the optional adapter until shutdown. Repeated retries must
        // not reload its delay imports; contexts/weavers are still destroyed.
        host_=nullptr;unavailable_=false;
#endif
    }
    void disable() noexcept {release();
#if defined(STARFOX_LEIASR_HOST)
        unavailable_=true;
#endif
    }
private:
#if defined(STARFOX_LEIASR_HOST)
    void set_status(const char* text,unsigned long error=0) noexcept {
        if(error) std::snprintf(status_,sizeof(status_),"%s (Windows error %lu)",text?text:"Unknown SR error",error);
        else std::snprintf(status_,sizeof(status_),"%s",text?text:"Unknown SR error");
    }
    void unload() noexcept {api_=nullptr;get_status_=nullptr;if(module_) FreeLibrary(module_);module_=nullptr;}
    HMODULE module_{};
    const StarfoxLeiaSrApiV1* api_{};
    StarfoxLeiaSrGetStatus get_status_{};
    void* host_{};
    bool unavailable_{};
    char status_[512]{"SR Platform not requested"};
#endif
};
