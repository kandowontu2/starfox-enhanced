#include "starfox/vr/cartridge_save.hpp"
#include "starfox/state/files.hpp"
#include <algorithm>
#include <stdexcept>
namespace starfox::vr {
CartridgeSave::CartridgeSave(std::filesystem::path path):path_(std::move(path)) {
    if(path_.empty() || !std::filesystem::exists(path_)) return;
    if(std::filesystem::file_size(path_)!=65536)
        throw std::runtime_error("EX cartridge save must be exactly 65536 bytes; existing file retained");
    persisted_=state::read_file(path_);
    if(persisted_.size()!=65536)
        throw std::runtime_error("EX cartridge save changed while loading; existing file retained");
}
bool CartridgeSave::synchronize(std::span<const uint8_t> bytes) {
    if(path_.empty()) return false;
    if(bytes.size()!=65536) throw std::runtime_error("Invalid EX cartridge save size");
    if(std::ranges::equal(bytes,persisted_)) return false;
    // Allocate before replacing the file, then commit the in-memory baseline.
    std::vector<uint8_t> next(bytes.begin(),bytes.end());
    state::write_atomic(path_,next);
    persisted_=std::move(next);
    return true;
}
}
