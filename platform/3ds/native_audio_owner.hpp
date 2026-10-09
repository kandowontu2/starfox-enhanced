#pragma once
#include <atomic>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds::detail {
// Both output services share this process owner. Service shutdown precedes
// releasing the lease; never run NDSP and the CSND check simultaneously.
inline std::atomic_flag audio_output_owned=ATOMIC_FLAG_INIT;
struct AudioOutputLease {
    AudioOutputLease() {
        if(audio_output_owned.test_and_set()) throw std::logic_error("3DS audio output already owned");
    }
    ~AudioOutputLease() {audio_output_owned.clear();}
    AudioOutputLease(const AudioOutputLease&)=delete;
    AudioOutputLease& operator=(const AudioOutputLease&)=delete;
};
} // namespace starfox::platform::nintendo_3ds::detail
