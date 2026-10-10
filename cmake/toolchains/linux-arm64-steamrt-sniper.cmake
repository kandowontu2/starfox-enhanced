# Clang/LLD cross-toolchain for the immutable Steam Runtime Sniper ARM64 SDK.
# Python, CMake, Ninja and code generators remain host programs; all target
# includes, libraries and pkg-config files are resolved from this sysroot.
if(NOT DEFINED STARFOX_STEAMRT_SYSROOT
   OR NOT IS_DIRECTORY "${STARFOX_STEAMRT_SYSROOT}")
    message(FATAL_ERROR
        "Set STARFOX_STEAMRT_SYSROOT to the extracted Sniper ARM64 sysroot")
endif()
if(NOT IS_DIRECTORY "${STARFOX_STEAMRT_SYSROOT}/usr/include")
    message(FATAL_ERROR
        "STARFOX_STEAMRT_SYSROOT does not contain usr/include: ${STARFOX_STEAMRT_SYSROOT}")
endif()

get_filename_component(STARFOX_STEAMRT_SYSROOT
    "${STARFOX_STEAMRT_SYSROOT}" REALPATH)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSROOT "${STARFOX_STEAMRT_SYSROOT}")

set(CMAKE_C_COMPILER clang CACHE FILEPATH "Host Clang C compiler")
set(CMAKE_CXX_COMPILER clang++ CACHE FILEPATH "Host Clang C++ compiler")
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)

# A multiarch Sniper sysroot may ship its matching GCC headers/runtime in one
# of these layouts. Point Clang into the SDK explicitly so its libstdc++
# discovery cannot fall back to the Linux host's native GCC installation.
set(_starfox_gcc_candidates)
foreach(_starfox_gcc_pattern
    "${STARFOX_STEAMRT_SYSROOT}/usr/lib/gcc/aarch64-linux-gnu/*"
    "${STARFOX_STEAMRT_SYSROOT}/usr/lib/gcc-cross/aarch64-linux-gnu/*"
    "${STARFOX_STEAMRT_SYSROOT}/lib/gcc/aarch64-linux-gnu/*")
    file(GLOB _starfox_matches LIST_DIRECTORIES TRUE "${_starfox_gcc_pattern}")
    list(APPEND _starfox_gcc_candidates ${_starfox_matches})
endforeach()
list(FILTER _starfox_gcc_candidates INCLUDE REGEX "/[0-9][^/]*$")
if(_starfox_gcc_candidates)
    list(SORT _starfox_gcc_candidates COMPARE NATURAL ORDER DESCENDING)
    list(GET _starfox_gcc_candidates 0 _starfox_gcc_install_dir)
    set(_starfox_clang_gcc "--gcc-install-dir=${_starfox_gcc_install_dir}")
else()
    # Clang still receives the sysroot, target triple and target-only CMake
    # lookup policy below. This fallback supports SDK layouts with no GCC tree.
    set(_starfox_clang_gcc "--gcc-toolchain=${STARFOX_STEAMRT_SYSROOT}/usr")
endif()
set(CMAKE_C_FLAGS_INIT "${_starfox_clang_gcc}")
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libstdc++ ${_starfox_clang_gcc}")
unset(_starfox_gcc_candidates)
unset(_starfox_gcc_pattern)
unset(_starfox_matches)
unset(_starfox_gcc_install_dir)
unset(_starfox_clang_gcc)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_AR llvm-ar CACHE FILEPATH "LLVM archiver")
set(CMAKE_RANLIB llvm-ranlib CACHE FILEPATH "LLVM ranlib")
set(CMAKE_NM llvm-nm CACHE FILEPATH "LLVM nm")

set(CMAKE_FIND_ROOT_PATH "${STARFOX_STEAMRT_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${STARFOX_STEAMRT_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "${STARFOX_STEAMRT_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${STARFOX_STEAMRT_SYSROOT}/usr/lib/pkgconfig:${STARFOX_STEAMRT_SYSROOT}/usr/share/pkgconfig")
unset(ENV{PKG_CONFIG_PATH})

# CMake invokes this toolchain file again for compiler ABI checks. Preserve the
# external sysroot input across those nested configure operations.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES STARFOX_STEAMRT_SYSROOT)
