#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_root="${1:-${script_dir}/..}"
source_root="$(cd -- "${source_root}" && pwd)"
if [[ "$(uname -s)" != "Linux" ]]; then
    echo "The Sniper sysroot build requires a Linux host with Clang and LLD." >&2
    exit 2
fi
build_root="${2:-${source_root}/build/steam-frame-cross}"
install_root="${3:-${build_root}/package}"
build_root="$(mkdir -p -- "${build_root}" && cd -- "${build_root}" && pwd)"
diagnostics_root="${build_root}/diagnostics"
sysroot_archive="${STARFOX_STEAMRT_SYSROOT_ARCHIVE:-${build_root}/com.valvesoftware.SteamRuntime.Sdk-arm64-sniper-sysroot.tar.gz}"
sysroot_root="${build_root}/sniper-sysroot"
jobs="${STARFOX_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}"

sysroot_url="https://repo.steampowered.com/steamrt-images-sniper/snapshots/3.0.20260415.224995/com.valvesoftware.SteamRuntime.Sdk-arm64-sniper-sysroot.tar.gz"
sysroot_sha256="8e162d235aeb1e6d283ab028e2c7b933061abc1d59830829c9e311cc73b3dd20"

if [[ -e "${install_root}" ]]; then
    echo "Refusing to reuse existing package directory: ${install_root}" >&2
    exit 2
fi
if [[ -L "${diagnostics_root}" ]]; then
    echo "Refusing to replace a diagnostics symlink: ${diagnostics_root}" >&2
    exit 2
fi
rm -rf -- "${diagnostics_root}"

if [[ ! -f "${sysroot_archive}" ]]; then
    mkdir -p -- "$(dirname -- "${sysroot_archive}")"
    partial="${sysroot_archive}.part"
    rm -f -- "${partial}"
    curl --fail --location --retry 3 "${sysroot_url}" --output "${partial}"
    echo "${sysroot_sha256}  ${partial}" | sha256sum --check --status
    mv -- "${partial}" "${sysroot_archive}"
fi
echo "${sysroot_sha256}  ${sysroot_archive}" | sha256sum --check

sysroot_marker="${sysroot_root}/.starfox-sysroot-sha256"
if [[ ! -f "${sysroot_marker}" ]] \
    || [[ "$(cat -- "${sysroot_marker}")" != "${sysroot_sha256}" ]]; then
    rm -rf -- "${sysroot_root}"
    mkdir -p -- "${sysroot_root}"
    tar -xzf "${sysroot_archive}" -C "${sysroot_root}"
    printf '%s\n' "${sysroot_sha256}" > "${sysroot_marker}"
fi

sysroot=""
while IFS= read -r include_directory; do
    candidate="${include_directory%/usr/include}"
    if [[ -d "${candidate}/usr/lib/aarch64-linux-gnu" \
       || -d "${candidate}/lib/aarch64-linux-gnu" ]]; then
        sysroot="${candidate}"
        break
    fi
done < <(find "${sysroot_root}" -type d -path '*/usr/include' -print)
if [[ -z "${sysroot}" ]]; then
    echo "The pinned archive did not contain an ARM64 Sniper sysroot with usr/include and aarch64 libraries." >&2
    exit 2
fi
sysroot="$(cd -- "${sysroot}" && pwd)"
sysroot_normalization_report="${build_root}/sysroot-link-normalization.json"
python3 "${source_root}/tests/test_steam_frame_sysroot_preparation.py"
python3 "${source_root}/tools/prepare_steam_frame_sysroot.py" \
    --sysroot "${sysroot}" \
    --output "${sysroot_normalization_report}" \
    --expected-absolute-links 38

toolchain="${source_root}/cmake/toolchains/linux-arm64-steamrt-sniper.cmake"
common_cmake_args=(
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_TOOLCHAIN_FILE=${toolchain}"
    "-DSTARFOX_STEAMRT_SYSROOT=${sysroot}"
    -DSTARFOX_BUILD_TESTS=OFF
    -DSTARFOX_BUILD_TOOLS=OFF
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
)
cmake_debug_args=()
if [[ "${STARFOX_CMAKE_DEBUG_TRY_COMPILE:-}" == "1" ]]; then
    # CI failure artifacts retain the generated probe project and command files
    # when a cross-platform compiler check fails.
    cmake_debug_args+=(--debug-trycompile)
fi

flat_build="${build_root}/flat"
cmake "${cmake_debug_args[@]}" -S "${source_root}" -B "${flat_build}" \
    "${common_cmake_args[@]}" \
    -DSTARFOX_BUILD_RUNTIME=ON \
    -DSTARFOX_BUILD_VR=OFF \
    -DSTARFOX_BUILD_STEAM_FRAME=OFF
cmake --build "${flat_build}" --target starfox_pc --parallel "${jobs}"

vr_build="${build_root}/steam-frame-vr"
cmake "${cmake_debug_args[@]}" -S "${source_root}" -B "${vr_build}" \
    "${common_cmake_args[@]}" \
    -DSTARFOX_BUILD_RUNTIME=OFF \
    -DSTARFOX_BUILD_VR=ON \
    -DSTARFOX_BUILD_STEAM_FRAME=ON \
    -DSTARFOX_EMBED_RUNTIME_ASSETS=ON
cmake --build "${vr_build}" --target starfox_steamframe starfox_vr_runtime_check \
    --parallel "${jobs}"
cmake --install "${vr_build}" --prefix "${install_root}" --component steamframe

mkdir -p -- "${diagnostics_root}"
cp -- "${flat_build}/starfox_pc" "${diagnostics_root}/starfox_pc"
cp -- "${vr_build}/starfox_vr_runtime_check" \
    "${diagnostics_root}/starfox_vr_runtime_check"
cp -- "${sysroot_normalization_report}" \
    "${diagnostics_root}/SYSROOT-LINK-NORMALIZATION.json"
cp -- "${source_root}/tools/package/STEAM-FRAME-DIAGNOSTICS.txt" \
    "${diagnostics_root}/STEAM-FRAME-DIAGNOSTICS.txt"
cp -- "${source_root}/THIRD_PARTY_NOTICES.md" "${source_root}/CREDITS.md" \
    "${source_root}/LICENSE-XBRZ.txt" "${diagnostics_root}/"
mkdir -p -- "${diagnostics_root}/licenses/fonts"
cp -- "${source_root}/assets/fonts/README.md" "${source_root}/assets/fonts/misaki.txt" \
    "${diagnostics_root}/licenses/fonts/"

bash -n "${source_root}/tools/package/LAUNCH-STEAM-FRAME.sh"
python3 "${source_root}/tests/test_steam_frame_package_validation.py"
python3 "${source_root}/tools/write_steam_frame_metadata.py" \
    --source-root "${source_root}" \
    --package-root "${install_root}" \
    --sysroot-archive "${sysroot_archive}" \
    --sysroot-normalization-report "${sysroot_normalization_report}"
python3 "${source_root}/tools/validate_steam_frame_elf.py" \
    --sysroot "${sysroot}" \
    --output "${diagnostics_root}/ELF-DEPENDENCIES.json" \
    "starfox_pc=${diagnostics_root}/starfox_pc" \
    "starfox_steamframe=${install_root}/starfox_steamframe" \
    "starfox_vr_runtime_check=${diagnostics_root}/starfox_vr_runtime_check"
python3 "${source_root}/tools/write_steam_frame_metadata.py" \
    --source-root "${source_root}" \
    --package-root "${diagnostics_root}" \
    --sysroot-archive "${sysroot_archive}" \
    --sysroot-normalization-report "${sysroot_normalization_report}" \
    --artifact-set hardware-diagnostics
python3 "${source_root}/tests/test_steam_frame_package.py" "${install_root}"
python3 "${source_root}/tests/test_steam_frame_package.py" \
    --artifact-set hardware-diagnostics "${diagnostics_root}"

echo "Steam Frame package checks completed: ${install_root}"
echo "Hardware diagnostic artifact checks completed: ${diagnostics_root}"
