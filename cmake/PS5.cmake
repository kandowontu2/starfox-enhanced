# PS5 homebrew target (platform/ps5/README.md): compiled with Mihawk's payload
# SDK fork and linked against RADV through cmake/toolchains/ps5-radv.cmake.
if(NOT STARFOX_PS5_VULKAN_ROOT)
    message(FATAL_ERROR "Configure the PS5 with cmake/toolchains/ps5-radv.cmake")
endif()
set(STARFOX_BUILD_TESTS OFF CACHE BOOL "Host tests cannot execute on consoles")
set(STARFOX_PACKAGE_MSU1_MUSIC OFF CACHE BOOL "Keep the optional music separate")
if(STARFOX_BUILD_VR)
    message(FATAL_ERROR "The PS5 homebrew backend does not provide OpenXR")
endif()
add_compile_definitions(STARFOX_PS5=1)

function(starfox_prepare_ps5_sdl)
    # Keep SDL3 GPU/Vulkan, mixer/resampler, pthreads and virtual pads.
    # Native video/audio are attached below; avoid probing host desktop drivers.
    foreach(feature ALSA JACK PIPEWIRE PULSEAUDIO SNDIO OSS X11 WAYLAND
            KMSDRM OPENGL OPENGLES HIDAPI LIBUSB UDEV DBUS IBUS
            LIBURING SENSOR CAMERA HAPTIC DUMMYVIDEO DUMMYAUDIO OFFSCREEN
            RENDER_VULKAN)
        set(SDL_${feature} OFF CACHE BOOL "Disabled on the PS5" FORCE)
    endforeach()
    set(SDL_RENDER_GPU ON CACHE BOOL "Hardware GPU rendering" FORCE)
    set(SDL_GPU ON CACHE BOOL "Native GPU API" FORCE)
    set(SDL_VULKAN ON CACHE BOOL "Native Vulkan" FORCE)
    set(SDL_RENDER ON CACHE BOOL "SDL GPU renderer" FORCE)
    set(HAVE_SDL_AUDIO TRUE)
    set(HAVE_SDL_VIDEO TRUE)
    set(SDL_VIRTUAL_JOYSTICK ON CACHE BOOL "Native pads use SDL virtual devices" FORCE)
    set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "No desktop window system" FORCE)
    # The SDK carries FreeBSD's usbhid headers but the console has no such
    # module; pads come from ScePad (gamepad.c), not SDL's BSD USB driver.
    set(LIBUSBHID FALSE CACHE INTERNAL "No usbhid on the PS5")
    # Configure checks compile static libraries and cannot see link failures.
    # These FreeBSD functions are absent from the console's libc module; SDL
    # then uses its own implementations.
    foreach(function WCSNLEN WCSLCPY WCSLCAT)
        set(LIBC_HAS_${function} FALSE CACHE INTERNAL "Not exported by the PS5 libc")
    endforeach()
    # Patch the pinned SDL source for the console (tools/prepare_ps5_sdl.py):
    # driver registration and two build gates, never its API.
    FetchContent_GetProperties(SDL3)
    if(NOT sdl3_POPULATED)
        FetchContent_Populate(SDL3)
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/prepare_ps5_sdl.py" "${sdl3_SOURCE_DIR}"
        COMMAND_ERROR_IS_FATAL ANY)
    add_subdirectory("${sdl3_SOURCE_DIR}" "${sdl3_BINARY_DIR}")
    set(STARFOX_CONSOLE_SDL_SOURCE "${sdl3_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(starfox_attach_ps5_sdl)
    # Vulkan comes from SDL's bundled headers and the RADV archive that
    # platform/ps5/link-title.sh links; system modules (VideoOut,
    # AudioOut, Pad, UserService) are imported through the SDK stubs there.
    set(backend "${PROJECT_SOURCE_DIR}/platform/ps5/runtime")
    target_sources(SDL3-static PRIVATE
        "${backend}/sdl_video.c" "${backend}/sdl_audio.c"
        "${backend}/sdl_filesystem.c" "${backend}/gamepad.c"
        "${backend}/libc_compat.c" "${backend}/process.c")
    target_include_directories(SDL3-static PRIVATE "${backend}"
        "${STARFOX_CONSOLE_SDL_SOURCE}/src")
endfunction()

set(STARFOX_PS5_TITLE_ID "PPSA99764" CACHE STRING
    "PS5 title id (homebrew range used by the PS5_Vulkan titles)")
set(STARFOX_PS5_CONTENT_ID "" CACHE STRING
    "PS5 content id (default: derived from STARFOX_PS5_TITLE_ID)")

# Writes <build>/ps5/<TITLE_ID>/: eboot.bin, sce_sys and sce_module/libc.prx.
function(starfox_add_ps5_title target)
    string(SUBSTRING "${STARFOX_PS5_TITLE_ID}" 4 5 STARFOX_PS5_CONCEPT_ID)
    if(NOT STARFOX_PS5_CONTENT_ID)
        # The UP9000 prefix is the one the console-validated PS5_Vulkan and
        # ProsperoEden titles use.
        set(STARFOX_PS5_CONTENT_ID "UP9000-${STARFOX_PS5_TITLE_ID}_00-STARFOXENHANCED0")
    endif()
    set(STARFOX_PS5_CONTENT_VERSION "01.000.000")
    set(param "${CMAKE_CURRENT_BINARY_DIR}/ps5-param.json")
    configure_file("${PROJECT_SOURCE_DIR}/platform/ps5/param.json.in" "${param}" @ONLY)
    add_custom_target(starfox_ps5_title ALL
        COMMAND /bin/bash "${PROJECT_SOURCE_DIR}/platform/ps5/package.sh"
            "${STARFOX_PS5_VULKAN_ROOT}" "$<TARGET_FILE:${target}>" "${param}"
            "${CMAKE_CURRENT_BINARY_DIR}/ps5"
        DEPENDS ${target}
        COMMENT "Packaging the PS5 title ${STARFOX_PS5_TITLE_ID}"
        VERBATIM)
endfunction()
