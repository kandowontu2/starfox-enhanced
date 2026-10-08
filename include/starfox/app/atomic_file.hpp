#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace starfox::app {

// The I/O seam lets tests exercise short writes and late flush/close/rename
// errors using real temporary files. Production always uses SDL's functions.
struct AtomicFileIo {
    decltype(&SDL_IOFromFile) open{SDL_IOFromFile};
    decltype(&SDL_WriteIO) write{SDL_WriteIO};
    decltype(&SDL_FlushIO) flush{SDL_FlushIO};
    decltype(&SDL_CloseIO) close{SDL_CloseIO};
    decltype(&SDL_RenamePath) rename{SDL_RenamePath};
};

// Write beside the destination, flush and close, then atomically replace it.
// A failed/abandoned transaction never truncates the previous file. Only the
// temporary file exclusively created by this instance can be cleaned up.
// This protects process interruption, not arbitrary filesystem/power failure.
class AtomicFile {
public:
    explicit AtomicFile(const std::filesystem::path& path,
        AtomicFileIo io = {}) noexcept : io_(io) {
        try {
            if (path.empty()) return;
            std::error_code error;
            destination_ = std::filesystem::absolute(path, error);
            if (error || !regular_or_missing(destination_)) return;
            std::filesystem::create_directories(destination_.parent_path(), error);
            if (error) return;
            destination_utf8_ = utf8(destination_);
            const auto stamp = std::to_string(SDL_GetTicksNS());
            for (unsigned attempt = 0; attempt < 64; ++attempt) {
                temporary_ = destination_;
                temporary_ += ".sfe-tmp-" + stamp + "-" +
                    std::to_string(sequence_.fetch_add(1, std::memory_order_relaxed));
                temporary_utf8_ = utf8(temporary_);
                stream_ = io_.open(temporary_utf8_.c_str(), "wbx");
                if (stream_) {
                    owns_temporary_ = true;
                    return;
                }
                // A collision is not ours to overwrite or remove. Other open
                // failures (permissions, full storage, etc.) fail immediately.
                const auto status = std::filesystem::symlink_status(temporary_, error);
                if (error || !std::filesystem::exists(status)) return;
            }
        } catch (...) {
            // Path conversion/allocation failure must not terminate a noexcept
            // settings save or touch an existing configuration.
        }
    }

    AtomicFile(const AtomicFile&) = delete;
    AtomicFile& operator=(const AtomicFile&) = delete;
    ~AtomicFile() {
        if (stream_) io_.close(stream_);
        discard_temporary();
    }

    [[nodiscard]] bool write(std::string_view bytes) noexcept {
        if (!stream_ || failed_) return false;
        if (!bytes.empty() && io_.write(stream_, bytes.data(), bytes.size()) != bytes.size())
            failed_ = true;
        return !failed_;
    }

    [[nodiscard]] bool commit() noexcept {
        if (!stream_) return false;
        const bool flushed = !failed_ && io_.flush(stream_);
        const bool closed = io_.close(stream_);
        stream_ = nullptr;
        if (flushed && closed && regular_or_missing(destination_)
            && io_.rename(temporary_utf8_.c_str(), destination_utf8_.c_str())) {
            owns_temporary_ = false;
            return true;
        }
        discard_temporary();
        return false;
    }

private:
    static std::string utf8(const std::filesystem::path& path) {
        const auto bytes = path.u8string();
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    static bool regular_or_missing(const std::filesystem::path& path) noexcept {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error && error != std::errc::no_such_file_or_directory) return false;
        return !std::filesystem::exists(status) || std::filesystem::is_regular_file(status);
    }

    void discard_temporary() noexcept {
        if (!owns_temporary_) return;
        std::error_code error;
        std::filesystem::remove(temporary_, error);
        owns_temporary_ = false;
    }

    inline static std::atomic<std::uint64_t> sequence_{};
    AtomicFileIo io_;
    std::filesystem::path destination_, temporary_;
    std::string destination_utf8_, temporary_utf8_;
    SDL_IOStream* stream_{};
    bool owns_temporary_{}, failed_{};
};

} // namespace starfox::app
