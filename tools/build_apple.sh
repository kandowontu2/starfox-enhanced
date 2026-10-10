#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-$(pwd)}"
platform="${2:-macos}"
case "${platform}" in
macos)
    build_root="${source_root}/build/macos-universal"
    cmake -S "${source_root}" -B "${build_root}" -G Xcode \
        -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
        -DSTARFOX_BUILD_TESTS=ON \
        -DSTARFOX_EMBED_RUNTIME_ASSETS=ON \
        -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
    ;;
ios)
    build_root="${source_root}/build/ios"
    cmake -S "${source_root}" -B "${build_root}" -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
        -DCMAKE_OSX_SYSROOT=iphoneos \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO \
        -DSTARFOX_METAL_INTEROP=ON \
        -DSTARFOX_BUILD_TESTS=OFF \
        -DSTARFOX_EMBED_RUNTIME_ASSETS=ON \
        -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
    ;;
*)
    echo "usage: $0 [source-root] [macos|ios]" >&2
    exit 2
    ;;
esac
metal_sdk=macosx
if [[ "${platform}" == "ios" ]]; then metal_sdk=iphoneos; fi
python3 "${source_root}/tools/check_metal_rt_shaders.py" --sdk "${metal_sdk}" \
    --generated-dir "${build_root}/generated"
cmake --build "${build_root}" --config Release --target starfox_pc

if [[ "${platform}" == "macos" ]]; then
    cmake --build "${build_root}" --config Release --target starfox_native_metal_check
    # A hosted Mac may lack ray intersections. Treat that ONLY as a reported
    # skip, never a runtime pass; real shader/image failures fail this job.
    set +e
    "${build_root}/Release/starfox_native_metal_check" \
        "${build_root}/generated/native-metal-check/reflection.metal" \
        "${build_root}/generated/native-metal-check/shadow.metal"
    native_metal_exit=$?
    set -e
    if [[ ${native_metal_exit} -eq 2 ]]; then
        echo "::warning::Native Metal runtime check skipped: no capable GPU (not runtime acceptance)."
    elif [[ ${native_metal_exit} -ne 0 ]]; then
        exit "${native_metal_exit}"
    fi
    cmake --build "${build_root}" --config Release --target starfox_asset_builder
    cmake --build "${build_root}" --config Release --target starfox_core_tests
    ctest --test-dir "${build_root}" -C Release \
        -R '^starfox_core_tests$' --output-on-failure
fi
