#pragma once
#include "starfox/app/atomic_file.hpp"
#include <filesystem>

namespace starfox::app {
enum class GpuFallbackReason { none, journal_unavailable, renderer_unavailable };
// The marker is intentionally NOT removed by the destructor: an exception,
// driver hang, forced close or failed GPU teardown must leave a recovery path.
// Only a successful switch to Software or completed graphics shutdown clears
// it. No assets, preferences or driver caches are deleted during recovery.
class GpuLaunchGuard {
public:
    GpuLaunchGuard(const std::filesystem::path& settings_path,
        const char* marker_name, bool enabled=true, const char* policy_name=nullptr)
        : pending_(settings_path.parent_path()/marker_name),enabled_(enabled) {
        if(policy_name) policy_=settings_path.parent_path()/policy_name;
    }
    [[nodiscard]] bool needs_safe_start() const noexcept {
        if(!enabled_) return false;
        std::error_code error;
        if(!policy_.empty() && (!std::filesystem::exists(policy_,error) || error)) return true;
        const bool pending=std::filesystem::exists(pending_,error);
        return pending || bool(error);
    }
    void record_policy() const noexcept {
        if(!enabled_ || policy_.empty()) return;
        AtomicFile file{policy_};
        if(file.write("1\n")) static_cast<void>(file.commit());
    }
    [[nodiscard]] bool arm() noexcept {
        if(!enabled_) return true;
        if(armed_) return true;
        AtomicFile file{pending_};
        armed_=file.write("GPU session incomplete; use Software recovery on next launch\n")
            && file.commit();
        return armed_;
    }
    void disarm() noexcept {
        if(!enabled_) return;
        std::error_code error;
        // A directory or other unrelated object at the journal path is not
        // our marker. Never remove it to make a failed GPU opt-in appear safe.
        if(!std::filesystem::is_regular_file(pending_,error)) {
            if(!error && !std::filesystem::exists(pending_,error) && !error) armed_=false;
            return;
        }
        std::filesystem::remove(pending_,error);
        if(!error) armed_=false;
    }
    [[nodiscard]] bool armed() const noexcept {return armed_;}
private:
    std::filesystem::path pending_,policy_;
    bool enabled_{},armed_{};
};
}
