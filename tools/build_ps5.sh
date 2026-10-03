#!/usr/bin/env bash
# PS5 homebrew build (platform/ps5/README.md). Everything runs in the
# platform/ps5/Dockerfile image: the pinned native toolchain (Mihawk's payload
# SDK fork, RADV, ps5-native-tool, libc.prx) is bootstrapped into
# build/console-sdk, then the game is built and packaged as a title folder.
set -euo pipefail

source_root="$(cd -- "${1:-$(pwd)}" && pwd)"
build_root="${2:-build/ps5}"
dist_root="${3:-${source_root}/dist/StarFoxEnhanced-ps5}"
image="${STARFOX_PS5_IMAGE:-starfox-ps5}"
title_id="${STARFOX_PS5_TITLE_ID:-PPSA99764}"

docker build -t "${image}" "${source_root}/platform/ps5"
run() {
    docker run --rm --user "$(id -u):$(id -g)" -v "${source_root}:/src:z" \
        -e HOME=/tmp -e LLVM_CONFIG=/usr/bin/llvm-config-18 -w /src "${image}" "$@"
}
run bash platform/ps5/bootstrap.sh build/console-sdk
run cmake -S . -B "${build_root}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchains/ps5-radv.cmake \
    -DSTARFOX_PS5_VULKAN_ROOT=/src/build/console-sdk/PS5_Vulkan \
    -DSTARFOX_PS5_TITLE_ID="${title_id}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARFOX_BUILD_TESTS=OFF \
    -DSTARFOX_BUILD_TOOLS=OFF \
    -DSTARFOX_BUILD_RUNTIME=ON \
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
run cmake --build "${build_root}" --target starfox_ps5_title

# Update the title folder in place: files the player added to it (such as
# Starfox-Assets.BIN) are kept.
mkdir -p "${dist_root}/${title_id}"
cp -r "${source_root}/${build_root}/ps5/${title_id}/." "${dist_root}/${title_id}/"
cp "${source_root}/platform/ps5/README.md" "${dist_root}/README.md"
cp "${source_root}/platform/mobile/ASSET_BUILDER.md" "${dist_root}/ASSET_BUILDER.md"
cp "${source_root}/CREDITS.md" "${source_root}/THIRD_PARTY_NOTICES.md" "${dist_root}/"

printf 'PS5 title folder: %s\n' "${dist_root}/${title_id}"
