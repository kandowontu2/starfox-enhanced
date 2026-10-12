#!/usr/bin/env bash
# Links a CMake executable as a PS5 native title ELF (eboot.elf contents).
#
#   link-title.sh <PS5_Vulkan root> <output> <objects and libraries...>
#
# This is CMake's executable link rule (cmake/toolchains/ps5-radv.cmake). It
# follows PS5_Vulkan's tools/build-radv-title.sh exactly: that repository's
# title CRT and C++ allocation runtime, the AGC import stubs, and
# tools/radv-link.sh's recipe for the statically linked RADV archive and the
# SDK fork's platform layer; then ps5-native-tool converts the PIE into the
# title ELF that package-ps5.sh signs. Nothing here is a software renderer:
# the only Vulkan implementation linked is RADV on the console GPU.
set -euo pipefail

[[ $# -ge 3 ]] || { echo "usage: ${0##*/} <PS5_Vulkan root> <output> <inputs...>" >&2; exit 2; }
root=$(cd -- "$1" && pwd)
output=$2
shift 2

sdk_root="$root/.deps/native/ps5-payload-sdk"
archive=${RADV_ARCHIVE:-$root/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a}
native="$root/tooling/native"
tool=${PS5_NATIVE_TOOL:-$root/build/runtime-shim/ps5-native-tool}
work="$output.ps5link"
module_sdk=0x02000009
companion_sdk=0x08050001

for file in "$archive" "$tool" "$sdk_root/bin/prospero-lld" "$native/app_crt.cpp"; do
    [[ -e $file ]] || { echo "missing $file (run platform/ps5/bootstrap.sh)" >&2; exit 2; }
done

inputs=()
for argument in "$@"; do
    case $argument in
        # libc, libm, libpthread and libdl are the console's system modules,
        # imported through the SDK stubs linked below.
        -lc | -lm | -lpthread | -ldl | -lrt | -pthread) ;;
        -Wl,*)
            IFS=, read -r -a flags <<< "${argument#-Wl,}"
            inputs+=("${flags[@]}")
            ;;
        -l* | -L* | --start-group | --end-group | --whole-archive | --no-whole-archive) inputs+=("$argument") ;;
        *.o | *.obj | *.a) inputs+=("$argument") ;;
        -*) echo "${0##*/}: ignoring link flag $argument" >&2 ;;
        *) inputs+=("$argument") ;;
    esac
done

mkdir -p "$work"
cc() { PS5_PAYLOAD_SDK="$sdk_root" sh "$root/tooling/prospero-clang18" "$@"; }
cc -std=c++20 -O2 -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -c "$native/app_crt.cpp" -o "$work/app_crt.o"
cc -std=c++20 -O2 -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -c "$native/app_cpp_runtime.cpp" -o "$work/app_cpp_runtime.o"

# AGC comes from system modules; these host-link stubs only name its imports.
stub() {
    local library=$1 source=$2
    cc -std=c11 -O2 -fPIC -c "$root/$source" -o "$work/${library}_stub.o"
    "$sdk_root/bin/prospero-lld" --shared -soname "${library}.prx" \
        -o "$work/${library}.so" "$work/${library}_stub.o"
}
stub libSceAgc vendor/ps5/sdk/stubs/agc_canary_link_stub.c
stub libSceAgcDriver vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c

# shellcheck source=/dev/null
source "$root/tools/radv-link.sh"
radv_link_recipe "$root" "$sdk_root" "$archive" || exit 2

# Mesa's entry-point tables reference every Vulkan function weakly, reading
# NULL when RADV does not define it; the title converter would demand an SDK
# import for each such dynamic symbol. --no-dynamic-linker keeps them out of
# the dynamic table (PS5_Vulkan docs/M5_PHASE_B.md, tools/check-vulkan-runtime.sh).
"$sdk_root/bin/prospero-lld" "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" \
    --no-dynamic-linker -z nodynamic-undefined-weak \
    --version-script "$native/app-symbols.map" --exclude-libs=ALL \
    -e _start -o "$work/llvm-pie.elf" \
    "$work/app_crt.o" "$work/app_cpp_runtime.o" \
    --start-group "${inputs[@]}" --end-group \
    "$work/libSceAgc.so" "$work/libSceAgcDriver.so" \
    "${radv_link_inputs[@]}" \
    --as-needed "$sdk_root"/target/lib/*.so
"$tool" link --in "$work/llvm-pie.elf" --out "$output" \
    --stub-dir "$sdk_root/target/lib" --stub "$work/libSceAgc.so" \
    --stub "$work/libSceAgcDriver.so" --module-sdk "$module_sdk" \
    --companion-sdk "$companion_sdk" --file-name eboot.elf
