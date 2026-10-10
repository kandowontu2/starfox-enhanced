#pragma once

#include <filesystem>
#include <fstream>

namespace starfox::vr {

// Owns an optional profiling CSV. A default std::ofstream has goodbit set,
// so its bool conversion does not indicate that a file was opened.
class ProfileCsvOutput {
public:
    bool open(const std::filesystem::path& path) {
        stream_.open(path,std::ios::out|std::ios::trunc);
        return enabled() && good();
    }

    bool enabled() const noexcept {return stream_.is_open();}
    bool good() const noexcept {return static_cast<bool>(stream_);}
    std::ostream& stream() noexcept {return stream_;}
    bool flush() {
        stream_.flush();
        return good();
    }

private:
    std::ofstream stream_;
};

}
