#pragma once
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>

// Optional owner-thread timing; no change to the operation, its result/error,
// or the thread it runs on. Diagnostics never require a writable journal.
template<class Work> decltype(auto) trace_startup_work(const char* phase,Work&& work) {
    struct Trace {
        const char* phase;
        bool active{std::getenv("STARFOX_TRACE_GPU")!=nullptr};
        std::chrono::steady_clock::time_point start{};
        explicit Trace(const char* value) noexcept:phase(value) {
            if(active) try {start=std::chrono::steady_clock::now();std::cerr<<"gpu-startup: begin "<<phase<<'\n';} catch(...) {}
        }
        ~Trace() noexcept {
            if(active) try {
                std::cerr<<"gpu-startup: end "<<phase<<" ms="
                    <<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()<<'\n';
            } catch(...) {}
        }
    } trace{phase};
    return std::forward<Work>(work)();
}

// A small append-only startup journal survives a driver hang/forced close.
// Diagnostics must never prevent launching from a read-only installation.
class StartupTrace {
    std::ofstream file_;
    std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
public:
    explicit StartupTrace(const std::filesystem::path& directory) noexcept {
        try {
            file_.open(directory / "startup.log",std::ios::app);
            file_<<"\nlaunch unix-seconds="<<std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()<<'\n';
            mark("process started");
        } catch (...) {}
    }
    void mark(std::string_view stage) noexcept {
        try {
            file_<<std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-start_).count()<<" ms: "<<stage<<'\n';
            file_.flush();
        } catch (...) {}
    }
};
