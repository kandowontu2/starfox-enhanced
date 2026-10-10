# PS5 native title toolchain: the pinned Mihawk payload SDK fork for
# compilation, and the PS5_Vulkan RADV title link recipe for executables.
#
# STARFOX_PS5_VULKAN_ROOT names a PS5_Vulkan checkout whose dependencies were
# bootstrapped by platform/ps5/bootstrap.sh (.deps/native holds the
# SDK and the release RADV archive; build/runtime-shim holds ps5-native-tool).
set(STARFOX_PS5_VULKAN_ROOT "$ENV{STARFOX_PS5_VULKAN_ROOT}" CACHE PATH
    "PS5_Vulkan checkout with bootstrapped native dependencies")
if(NOT STARFOX_PS5_VULKAN_ROOT)
    set(STARFOX_PS5_VULKAN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../build/console-sdk/PS5_Vulkan")
endif()
get_filename_component(STARFOX_PS5_VULKAN_ROOT "${STARFOX_PS5_VULKAN_ROOT}" ABSOLUTE)
set(STARFOX_PS5_SDK "${STARFOX_PS5_VULKAN_ROOT}/.deps/native/ps5-payload-sdk")
if(NOT EXISTS "${STARFOX_PS5_SDK}/.ps5-sdk-revision")
    message(FATAL_ERROR "No bootstrapped PS5 SDK under ${STARFOX_PS5_VULKAN_ROOT}; "
        "run platform/ps5/bootstrap.sh")
endif()
# try_compile projects re-read this file without the cache.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES STARFOX_PS5_VULKAN_ROOT)

include("${STARFOX_PS5_SDK}/toolchain/prospero.cmake")

# The title is one statically linked PIE; checks cannot link test programs
# without the title CRT, and an RPATH means nothing on the console.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "" FORCE)

# Executables go through the native title link (CRT, RADV, platform layer,
# libc++ and the title conversion); see platform/ps5/link-title.sh.
set(_starfox_ps5_link
    "/bin/bash \"${CMAKE_CURRENT_LIST_DIR}/../../platform/ps5/link-title.sh\" \"${STARFOX_PS5_VULKAN_ROOT}\" <TARGET> <OBJECTS> <LINK_LIBRARIES>")
set(CMAKE_C_LINK_EXECUTABLE "${_starfox_ps5_link}")
set(CMAKE_CXX_LINK_EXECUTABLE "${_starfox_ps5_link}")
unset(_starfox_ps5_link)
