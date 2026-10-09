#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
namespace starfox::vr {
// Empty path disables persistence (including all model preflights).
class CartridgeSave {
public:
    explicit CartridgeSave(std::filesystem::path path={});
    std::span<const uint8_t> initial() const {return persisted_;}
    bool synchronize(std::span<const uint8_t> bytes);
private:
    std::filesystem::path path_;
    std::vector<uint8_t> persisted_;
};
}
