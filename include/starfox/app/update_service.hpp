#pragma once
#include "starfox/app/update_release.hpp"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>

namespace starfox::app {
enum class UpdatePhase { idle, checking, current, available, downloading, ready, failed };
struct UpdateStatus {
    UpdatePhase phase{};
    UpdateRelease release;
    std::string message="NOT CHECKED";
    unsigned percent{};
};
// No startup requests, credentials, telemetry or main-thread networking.
class UpdateService {
public:
    explicit UpdateService(std::filesystem::path executable_directory);
    ~UpdateService();
    UpdateService(const UpdateService&)=delete;
    UpdateService& operator=(const UpdateService&)=delete;
    static const char* installed_version();
    static bool check_supported();
    bool install_supported() const;
    UpdateStatus status() const;
    void check();
    void download();
    void cancel();
    bool restart_to_install(); // Only call after an explicit user confirmation.
    bool open_release_page() const;
private:
    std::filesystem::path directory_,work_;
    mutable std::mutex mutex_;
    UpdateStatus status_;
    std::thread worker_;
    std::atomic_bool cancel_{};
    std::atomic_bool install_available_{};
    bool determine_install_support() const;
    void set_status(UpdatePhase phase,std::string message);
    void cleanup_work();
};
} // namespace starfox::app
