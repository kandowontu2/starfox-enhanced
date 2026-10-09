#!/usr/bin/env bash
set -euo pipefail

# Platform diagnostics and real cartridge-core bring-up, not the completed game.
# Run from a devkitPro shell; no WSL or Docker environment is required.
: "${DEVKITPRO:?Set DEVKITPRO to the devkitPro installation}"
source_root="${1:-$(pwd)}"
build_root="${2:-${source_root}/build/3ds-frontend}"
test_player="${STARFOX_3DS_TEST_PLAYER:-OFF}"
profile_frames="${STARFOX_3DS_PROFILE_FRAMES:-OFF}"
enable_ipo="${STARFOX_3DS_ENABLE_IPO:-OFF}"
if [[ "$test_player" != ON && "$test_player" != OFF ]]; then
    printf '%s\n' 'STARFOX_3DS_TEST_PLAYER must be ON or OFF.' >&2
    exit 1
fi
if [[ "$profile_frames" != ON && "$profile_frames" != OFF ]]; then
    printf '%s\n' 'STARFOX_3DS_PROFILE_FRAMES must be ON or OFF.' >&2
    exit 1
fi
if [[ "$enable_ipo" != ON && "$enable_ipo" != OFF ]]; then
    printf '%s\n' 'STARFOX_3DS_ENABLE_IPO must be ON or OFF.' >&2
    exit 1
fi
toolchain="${DEVKITPRO}/cmake/3DS.cmake"

if [[ ! -f "${toolchain}" || ! -f "${DEVKITPRO}/libctru/include/3ds.h" ]]; then
    printf '%s\n' 'Missing devkitPro 3DS SDK. Install devkitARM, libctru, 3ds-tools and 3ds-cmake.' >&2
    exit 1
fi
cmake -S "${source_root}/platform/3ds" -B "${build_root}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${toolchain}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DSTARFOX_3DS_BUILD_NATIVE=ON \
    -DSTARFOX_3DS_BUILD_TEST_PLAYER="$test_player" \
    -DSTARFOX_3DS_PROFILE_FRAMES="$profile_frames" \
    -DSTARFOX_3DS_ENABLE_IPO="$enable_ipo" \
    -DSTARFOX_3DS_BUILD_HOST_TESTS=OFF
# Report all independent native compilation failures in one pass. Ninja still
# returns failure; this never turns a partial build into an accepted package.
cmake --build "${build_root}" --parallel 2 -- -k 0
printf 'Frontend diagnostic (NOT the game): %s\n' "${build_root}/starfox_3ds_frontend_check.3dsx"
printf 'PICA GPU diagnostic (NOT the game): %s\n' "${build_root}/starfox_3ds_gpu_check.3dsx"
printf 'Actual VM/SPC/HUD bring-up (renderer incomplete): %s\n' "${build_root}/starfox_3ds_game_core_check.3dsx"
if [[ "$test_player" == ON ]]; then
    printf 'Experimental New 3DS stereo / original-model mono test player (NOT hardware-accepted): %s\n' "${build_root}/starfox_3ds_test_player.3dsx"
fi
