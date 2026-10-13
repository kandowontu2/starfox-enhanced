#pragma once
#include <cstdint>
#if defined(__3DS__)
#include <3ds.h>
#else
#include <chrono>
#endif
namespace starfox::platform_3ds {
#if defined(__3DS__)
inline constexpr std::uint64_t reference_clock_frequency=SYSCLOCK_ARM11;
inline std::uint64_t reference_system_tick() noexcept {return svcGetSystemTick();}
#else
// Host component tests use only wall-time diagnostics, never an emulated CPU.
inline constexpr std::uint64_t reference_clock_frequency=1'000'000'000ULL;
inline std::uint64_t reference_system_tick() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
#endif
}
