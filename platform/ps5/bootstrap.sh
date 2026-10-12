#!/usr/bin/env bash
# Fetches and builds the pinned PS5 native toolchain into DEPS (default
# build/console-sdk): Mihawk's PS5_Vulkan, its Mesa (RADV) and payload SDK
# forks, then the SDK, the release RADV archive, ps5-native-tool and the
# clean-room libc.prx (checked against PS5_Vulkan's recorded digest).
#
# Run inside the platform/ps5/Dockerfile image (tools/build_ps5.sh does).
set -euo pipefail

deps=${1:-build/console-sdk}
# PS5_Vulkan pins the Mesa and SDK fork revisions in tools/build-radv.sh and
# tools/setup-native-dependencies.sh; these two follow those pins.
vulkan_revision=3f3ee69607013b345d2baa6d6a37c86745649a08
mesa_revision=0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8
sdk_revision=95c08f27386fc698f6bbe21dde3030140a41d10b

[[ ${LLVM_CONFIG:-} == /* ]] || {
    echo "LLVM_CONFIG must be an absolute path (the SDK wrappers resolve it)" >&2
    exit 2
}

fetch() {
    local directory=$1 url=$2 revision=$3
    if [[ ! -d $directory/.git ]]; then
        git init -q "$directory"
        git -C "$directory" remote add origin "$url"
    fi
    if ! git -C "$directory" cat-file -e "$revision^{commit}" 2>/dev/null; then
        git -C "$directory" fetch -q --depth 1 origin "$revision"
    fi
    # PS5_Vulkan is used as a working tree; the forks are exported by revision.
    if [[ $directory == */PS5_Vulkan ]]; then
        git -C "$directory" -c advice.detachedHead=false checkout -q "$revision"
    fi
}

mkdir -p "$deps"
deps=$(cd -- "$deps" && pwd)
fetch "$deps/PS5_Vulkan" https://github.com/mihawk-99/PS5_Vulkan.git "$vulkan_revision"
fetch "$deps/PS5_Mesa" https://github.com/mihawk-99/PS5_Mesa.git "$mesa_revision"
fetch "$deps/PS5_PayloadSDK" https://github.com/mihawk-99/PS5_PayloadSDK.git "$sdk_revision"

grep -q "^mesa_revision=$mesa_revision$" "$deps/PS5_Vulkan/tools/build-radv.sh" ||
    { echo "PS5_Vulkan pins another Mesa revision" >&2; exit 2; }
grep -q "^sdk_revision=$sdk_revision$" "$deps/PS5_Vulkan/tools/setup-native-dependencies.sh" ||
    { echo "PS5_Vulkan pins another SDK revision" >&2; exit 2; }

cd "$deps/PS5_Vulkan"
bash tools/setup-native-dependencies.sh
bash tools/build-radv.sh release
bash tools/rebuild-libc.sh
echo "==> PS5 native dependencies ready in $deps/PS5_Vulkan"
