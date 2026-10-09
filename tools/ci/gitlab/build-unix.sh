#!/usr/bin/env bash
set -euo pipefail

target="${1:?expected linux, switch, or vita}"
cd "${CI_PROJECT_DIR:?CI_PROJECT_DIR is required}"
mkdir -p release-out
version="${CI_COMMIT_TAG:-${CI_RELEASE_TAG:-v0.0.8}}"
version="${version#v}"

case "${target}" in
linux)
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates git build-essential cmake ninja-build pkg-config curl file unzip \
        libasound2-dev libpulse-dev libx11-dev libxext-dev \
        libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
        libxss-dev libxtst-dev libxkbcommon-dev libgl1-mesa-dev \
        libgles2-mesa-dev libegl1-mesa-dev libdbus-1-dev libudev-dev
    curl --fail --location --retry 3 \
        https://raw.githubusercontent.com/neomody77/sdl3-switch/182e511214d7600e4bdab8606d7caf0ef744afd6/sdl3-switch.patch \
        --output /tmp/sdl3-switch.patch
    python3 tests/test_switch_audio_backend.py /tmp/sdl3-switch.patch c++
    bash tools/build_linux.sh "$CI_PROJECT_DIR" \
        "$CI_PROJECT_DIR/build/gitlab-linux-x64" \
        "$CI_PROJECT_DIR/dist/StarFoxEnhanced-linux-x64"
    binary=dist/StarFoxEnhanced-linux-x64/starfox_pc
    test -x "$binary"
    test -x dist/StarFoxEnhanced-linux-x64/starfox_asset_builder
    file "$binary" | grep -q 'ELF 64-bit.*x86-64'
    ! ldd "$binary" | grep -q 'not found'
    cmake -E tar cf "release-out/StarFoxEnhanced-${version}-linux-x64.zip" \
        --format=zip dist/StarFoxEnhanced-linux-x64
    ;;
switch)
    bash tools/build_switch.sh "$CI_PROJECT_DIR"
    nro=dist/StarFoxEnhanced-switch/switch/StarFoxEnhanced/StarFoxEnhanced.nro
    test -s "$nro"
    test -f dist/StarFoxEnhanced-switch/package_switch_nsp.ps1
    cmake -E tar cf "release-out/StarFoxEnhanced-${version}-switch-homebrew.zip" \
        --format=zip dist/StarFoxEnhanced-switch
    ;;
vita)
    bash tools/build_vita.sh "$CI_PROJECT_DIR"
    vpk=dist/StarFoxEnhanced-vita/StarFoxEnhanced.vpk
    test -s "$vpk"
    cmake -E tar tf "$vpk" > build/vita-contents.txt
    grep -Fxq eboot.bin build/vita-contents.txt
    cmake -E tar cf "release-out/StarFoxEnhanced-${version}-vita.zip" \
        --format=zip dist/StarFoxEnhanced-vita
    ;;
*)
    echo "Unknown target: $target" >&2
    exit 2
    ;;
esac
