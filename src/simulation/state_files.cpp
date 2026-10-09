#include "starfox/state/files.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace starfox::state {
namespace {
constexpr std::uintmax_t maximum_file_size = 64U * 1024U * 1024U + 24U;
struct TemporarySlot {
    std::filesystem::path directory, file;
    ~TemporarySlot() {
        std::error_code ignored;
        if (!file.empty()) std::filesystem::remove(file, ignored);
        if (!directory.empty()) std::filesystem::remove(directory, ignored);
    }
};
}

void write_atomic(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    if (bytes.empty() || bytes.size() > maximum_file_size || path.filename().empty())
        throw std::runtime_error{"Invalid save-state file"};
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."};
    std::filesystem::create_directories(parent);
    static std::atomic<std::uint64_t> sequence{};
    TemporarySlot temporary;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        const auto candidate = parent / (".sfe-state-" + std::to_string(stamp)
            + "-" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
        if (std::filesystem::create_directory(candidate)) {
            temporary.directory = candidate;
            temporary.file = candidate / "state.tmp";
            break;
        }
    }
    if (temporary.directory.empty()) throw std::runtime_error{"Cannot reserve save-state temporary file"};
    {
        std::ofstream output{temporary.file, std::ios::binary | std::ios::trunc};
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        output.close();
    }
#ifdef _WIN32
    // std::filesystem::rename cannot replace an existing destination on Windows.
    if (!MoveFileExW(temporary.file.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), "Replace save-state slot"};
#else
    std::filesystem::rename(temporary.file, path);
#endif
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) throw std::runtime_error{"Save-state slot is empty or unreadable"};
    const auto size = input.tellg();
    if (size <= 0 || static_cast<std::uintmax_t>(size) > maximum_file_size)
        throw std::runtime_error{"Invalid save-state file size"};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))
        || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error{"Save-state file changed or could not be read completely"};
    return bytes;
}
} // namespace starfox::state
