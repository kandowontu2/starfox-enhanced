#pragma once
#include <cstdint>
#include <span>
#include <string>
struct SDL_AudioStream;
namespace starfox::vr {
class PcmOutput {
public:
    ~PcmOutput();
    PcmOutput()=default;
    PcmOutput(const PcmOutput&)=delete;
    PcmOutput& operator=(const PcmOutput&)=delete;
    bool open();
    bool set_active(bool);
    bool push(std::span<const int16_t>);
    void close() noexcept;
    int queued_bytes() const noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    SDL_AudioStream* stream_{};bool initialized_{},active_{};
    std::string status_;
};
}
