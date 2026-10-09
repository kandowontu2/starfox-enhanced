#pragma once
#include "starfox/render/sdl_gpu_preparation.h"
#include "starfox/render/gpu_preparation.hpp"
#include <exception>
#include <algorithm>

namespace starfox::render {

class SdlGpuPreparation {
    unsigned delay_ms_{};
    bool trace_{};
    static bool SDLCALL prepare(void* user,bool (SDLCALL *compile)(void*),void* argument) noexcept {
        const auto& self=*static_cast<const SdlGpuPreparation*>(user);
        const auto start=std::chrono::steady_clock::now();
        if(self.trace_) std::cerr<<"gpu-renderer-prepare: begin presenter-shaders delay-ms="<<self.delay_ms_<<'\n';
        try {
            // Error capture belongs to the worker; SDL's error slot is TLS.
            const bool result=prepare_gpu_resource([&] {
                if(self.delay_ms_) std::this_thread::sleep_for(std::chrono::milliseconds(self.delay_ms_));
                return compile(argument);
            });
            if(self.trace_) std::cerr<<"gpu-renderer-prepare: end presenter-shaders success="<<result<<" ms="
                <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<'\n';
            return result;
        } catch(const std::exception& error) {
            SDL_SetError("Presenter shader preparation: %s",error.what());
        } catch(...) {
            SDL_SetError("Presenter shader preparation failed");
        }
        return false; // Never unwind through SDL's C renderer initializer.
    }
    StarfoxSdlGpuPreparation hook_{1,this,prepare};
    static bool SDLCALL prepare_pipeline(void*,bool (SDLCALL *compile)(void*),void* argument) noexcept {
        const bool trace=std::getenv("STARFOX_TRACE_GPU")!=nullptr;
        // Delay only the first actual presenter/blit resource of a finite
        // diagnostic process, not every format or a normal launch.
        static std::atomic_bool delay_used{};
        const auto* override=std::getenv("STARFOX_TEST_FRAMES")
            ?std::getenv("STARFOX_TEST_RENDERER_PIPELINE_DELAY_MS"):nullptr;
        const auto delay=override && !delay_used.exchange(true)?std::clamp(std::atoi(override),0,20000):0;
        const auto start=std::chrono::steady_clock::now();
        if(trace) std::cerr<<"gpu-renderer-prepare: begin presenter-pipeline delay-ms="<<delay<<'\n';
        try {
            const bool result=prepare_gpu_resource([&] {
                if(delay) std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                return compile(argument);
            });
            if(trace) std::cerr<<"gpu-renderer-prepare: end presenter-pipeline success="<<result<<" ms="
                <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<'\n';
            return result;
        } catch(const std::exception& error) {SDL_SetError("Presenter pipeline preparation: %s",error.what());}
        catch(...) {SDL_SetError("Presenter pipeline preparation failed");}
        return false;
    }
public:
    explicit SdlGpuPreparation(unsigned delay_ms=0,bool trace=false) noexcept
        :delay_ms_(delay_ms),trace_(trace) {}
    SdlGpuPreparation(const SdlGpuPreparation&)=delete;
    SdlGpuPreparation& operator=(const SdlGpuPreparation&)=delete;
    const StarfoxSdlGpuPreparation* hook() const noexcept {return &hook_;}
    static const StarfoxSdlGpuPreparation* pipeline_hook() noexcept {
        // No stack-bound SdlGpuPreparation user pointer is retained by SDL.
        static const StarfoxSdlGpuPreparation hook{1,nullptr,prepare_pipeline};return &hook;
    }
};

} // namespace starfox::render
