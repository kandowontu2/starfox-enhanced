cmake_minimum_required(VERSION 3.22)

project(starfox_enhanced VERSION 0.0.8 LANGUAGES CXX)

# The PS5 payload SDK toolchain (cmake/toolchains/ps5-radv.cmake) sets PROSPERO.
if(PROSPERO)
    set(STARFOX_PS5 TRUE)
    include(cmake/PS5.cmake)
endif()

if(WINDOWS_STORE)
    # Apply the UWP ABI to the entire dependency graph, not only the SDL entry
    # point. The redistributable UWP framework still uses the pre-17.10 mutex
    # constructor ABI; ask it to initialize mutexes instead of inlining the
    # newer desktop runtime's layout.
    add_compile_definitions(WINAPI_FAMILY=WINAPI_FAMILY_APP
        _CRT_APP=1 _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR=1)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:/Zi>")
    # Ninja links with the caller's LIB environment. A later incremental build
    # from a desktop shell must not silently replace *_app.dll imports with
    # desktop CRT imports (which fail before WinMain on Xbox).
    get_filename_component(STARFOX_MSVC_X64_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(STARFOX_MSVC_ROOT "${STARFOX_MSVC_X64_BIN}/../../.." ABSOLUTE)
    set(STARFOX_UWP_CRT_LIB_DIR "${STARFOX_MSVC_ROOT}/lib/x64/store")
    if(NOT EXISTS "${STARFOX_UWP_CRT_LIB_DIR}/msvcprt.lib")
        message(FATAL_ERROR "The MSVC x64 UWP/Store libraries are required")
    endif()
    link_directories(BEFORE "${STARFOX_UWP_CRT_LIB_DIR}")
endif()

# devkitPro's current toolchain identifies Horizon as NintendoSwitch but does
# not define the legacy SWITCH convenience variable used by projects and SDL.
if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(SWITCH TRUE)
endif()

if(WINDOWS_STORE OR ANDROID OR IOS OR SWITCH OR VITA OR STARFOX_PS5)
    set(STARFOX_BUILD_TOOLS_DEFAULT OFF)
else()
    set(STARFOX_BUILD_TOOLS_DEFAULT ON)
endif()

option(STARFOX_BUILD_TESTS "Build deterministic timing tests" ON)
option(STARFOX_BUILD_RUNTIME "Build the SDL3 native PC runtime" ON)
option(STARFOX_BUILD_VR "Build the OpenXR VR integration and runtime diagnostic" OFF)
option(STARFOX_ENABLE_DISPLAYXR "Include optional Leia SR integration components in desktop Windows" ON)
option(STARFOX_ENABLE_LEIASR "Include optional direct SR Platform adapter loader in desktop Windows" ON)
option(STARFOX_BUILD_STEAM_FRAME
    "Build the native Linux ARM64 Steam Frame OpenXR player" OFF)
option(STARFOX_BUILD_TOOLS "Build host-side trace and asset tools"
    ${STARFOX_BUILD_TOOLS_DEFAULT})
option(STARFOX_EMBED_RUNTIME_ASSETS
    "Embed source-built BPS deltas and symbol tables into the Windows runtime" ON)
option(STARFOX_PACKAGE_MSU1_MUSIC
    "Build and install the optional Starfox-MSU1.PAK companion" ON)
# xBRZ is enabled by default; minimal builds may disable the backend.
option(STARFOX_ENABLE_XBRZ
    "Fetch and compile the GPLv3 xBRZ backend for the 2D presentation filter"
    ON)
if(STARFOX_BUILD_STEAM_FRAME)
    if(NOT STARFOX_BUILD_VR)
        message(FATAL_ERROR "STARFOX_BUILD_STEAM_FRAME requires STARFOX_BUILD_VR=ON")
    endif()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "STARFOX_BUILD_STEAM_FRAME requires a Linux target")
    endif()
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$"
       OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "STARFOX_BUILD_STEAM_FRAME requires a 64-bit ARM target")
    endif()
endif()
if(ANDROID AND STARFOX_BUILD_VR AND CMAKE_GENERATOR MATCHES "Ninja")
    # The Quest runner has limited RAM; compiling several large simulation/VR
    # translation units at once makes the hosted runner terminate mid-build.
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS starfox_quest_compile=1)
    set(CMAKE_JOB_POOL_COMPILE starfox_quest_compile)
    # These two translation units are unusually large. Clang's O2/O3
    # optimizer has repeatedly made the hosted Quest runner terminate while
    # compiling them, even with a single Ninja worker. Keep the rest of the
    # headset renderer at O2 and use the bounded O1 pass only for simulation.
    set_source_files_properties(
        src/simulation/game_simulation.cpp
        src/simulation/wdc65816.cpp
        PROPERTIES COMPILE_OPTIONS "-O1")
endif()
set(STARFOX_UWP_PACKAGE_CERTIFICATE "" CACHE FILEPATH
    "Development PFX used to sign the Xbox UWP package")
set(STARFOX_ROM_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
    CACHE FILEPATH "Assembled UltraStarFox ROM used by the native runtime")
set(STARFOX_SYMBOLS_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/symbols/ultrastarfox.txt"
    CACHE FILEPATH "Pinned UltraStarFox symbol table used by the native runtime")
set(STARFOX_EX_ROM_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-star-fox-ex/SFES/SFES.SFC"
    CACHE FILEPATH "Assembled Star Fox EX ROM used by the native runtime")
set(STARFOX_EX_SYMBOLS_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/symbols/starfox-ex.txt"
    CACHE FILEPATH "Pinned Star Fox EX symbol table used by the native runtime")
set(STARFOX_ORIGINAL_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/ultrastarfox-v12.bps"
    CACHE FILEPATH "UltraStarFox BPS delta for a clean Star Fox USA v1.2 ROM")
set(STARFOX_EX_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/starfox-ex-v12.bps"
    CACHE FILEPATH "Star Fox EX BPS delta for a clean Star Fox USA v1.2 ROM")
if(WIN32)
    set(STARFOX_RETAIL_ROM_DIR "C:/NTSC-US Super Nintendo System Roms"
        CACHE PATH "Directory searched for clean retail validation ROMs")
else()
    set(STARFOX_RETAIL_ROM_DIR "$ENV{HOME}/NTSC-US Super Nintendo System Roms"
        CACHE PATH "Directory searched for clean retail validation ROMs")
endif()
set(STARFOX_RETAIL_V12_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (USA) (Rev 2).sfc"
    CACHE FILEPATH "Clean Star Fox USA v1.2 ROM used only for local validation")
set(STARFOX_RETAIL_JAPAN_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (Japan).sfc")
set(STARFOX_RETAIL_JAPAN_V11_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (Japan) (Rev 1).sfc")
set(STARFOX_RETAIL_USA_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (USA).sfc")
set(STARFOX_RETAIL_USA_V11_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (USA) (Rev 1).sfc")
set(STARFOX_RETAIL_EUROPE_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Starwing (Europe).sfc")
set(STARFOX_RETAIL_EUROPE_V11_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Starwing (Europe) (Rev 1).sfc")
set(STARFOX_RETAIL_GERMANY_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Starwing (Germany).sfc")
set(STARFOX_RETAIL_JAPAN_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-japan-v10-to-usa-v12.bps")
set(STARFOX_RETAIL_JAPAN_V11_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-japan-v11-to-usa-v12.bps")
set(STARFOX_RETAIL_USA_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-usa-v10-to-v12.bps")
set(STARFOX_RETAIL_USA_V11_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-usa-v11-to-v12.bps")
set(STARFOX_RETAIL_EUROPE_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-europe-v10-to-usa-v12.bps")
set(STARFOX_RETAIL_EUROPE_V11_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-europe-v11-to-usa-v12.bps")
set(STARFOX_RETAIL_GERMANY_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-germany-v10-to-usa-v12.bps")

include(FetchContent)
if(APPLE AND (STARFOX_BUILD_RUNTIME OR STARFOX_BUILD_VR))
    # SDL enables Objective-C++ for its Apple sources. Enable it in the root
    # project first so newer CMake versions publish compiler rules to this
    # directory before SDL's nested project configures.
    enable_language(OBJCXX)
endif()
# Standalone desktop VR needs SDL for gamepad/audio support even when the flat
# runtime is not built. Use the same pinned source archive without applying the
# flat runtime's SDL interop patches. Android continues to consume its official
# AAR/Prefab target below.
if(STARFOX_BUILD_VR AND NOT STARFOX_BUILD_RUNTIME AND NOT ANDROID)
    set(SDL_SHARED OFF CACHE BOOL "Avoid a runtime SDL shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Link SDL into the standalone VR executable" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build the SDL test library" FORCE)
    FetchContent_Declare(
        SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
        URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()
if(STARFOX_BUILD_VR)
    include(cmake/OpenXR.cmake)
endif()
FetchContent_Declare(
    retro_cpu
    GIT_REPOSITORY https://github.com/achaulk/retro_cpu.git
    GIT_TAG ea9049ab25084334f7cc1907b3a98bf1c2604a03
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(retro_cpu)

FetchContent_Declare(
    snes_spc
    GIT_REPOSITORY https://github.com/blarggs-audio-libraries/snes_spc.git
    GIT_TAG ec8ee2bbe30451614c1d02a83f7af1c97d497d45
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(snes_spc)

FetchContent_Declare(
    dr_libs
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
    GIT_TAG b55a0d9a30b91ad8901f89ecf05f76a33186c185
    # Only the standalone decoding headers are used. Its test-only miniaudio
    # submodule is large and unnecessary for any Star Fox target.
    GIT_SUBMODULES ""
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(dr_libs)

if(STARFOX_ENABLE_XBRZ)
    FetchContent_Declare(
        xbrz
        GIT_REPOSITORY https://github.com/janisozaur/xbrz.git
        GIT_TAG 93c54433fa0df37c689c919e8152fb0b9136584a
        SOURCE_SUBDIR _starfox_no_upstream_project
    )
    FetchContent_MakeAvailable(xbrz)
    add_library(xbrz_filter STATIC "${xbrz_SOURCE_DIR}/xbrz.cpp")
    target_include_directories(xbrz_filter SYSTEM PUBLIC "${xbrz_SOURCE_DIR}")
    target_compile_features(xbrz_filter PUBLIC cxx_std_20)
    # Upstream's debug-only pixel breakpoint uses MSVC intrinsics even on
    # other compilers. Keep this optimized third-party filter in release mode.
    target_compile_definitions(xbrz_filter PRIVATE NDEBUG)
    set_target_properties(xbrz_filter PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # The pinned xBRZ revision calls std::sqrt without including <cmath>; it
    # only ever compiled because older standard libraries leaked the
    # declaration through another header. Force the include rather than
    # patching a third-party source tree.
    if(MSVC)
        target_compile_options(xbrz_filter PRIVATE /FI cmath)
    else()
        target_compile_options(xbrz_filter PRIVATE -include cmath)
    endif()
endif()

include(cmake/SpcState.cmake)
add_library(snes_spc_core STATIC
    "${STARFOX_SPC_SOURCE}/spc.cpp"
    "${STARFOX_SPC_SOURCE}/SNES_SPC.cpp"
    "${STARFOX_SPC_SOURCE}/SNES_SPC_misc.cpp"
    "${STARFOX_SPC_SOURCE}/SNES_SPC_state.cpp"
    "${STARFOX_SPC_SOURCE}/SPC_DSP.cpp"
    "${STARFOX_SPC_SOURCE}/SPC_Filter.cpp"
)
target_include_directories(snes_spc_core SYSTEM PUBLIC
    "${STARFOX_SPC_SOURCE}"
)

# RetroCPU's top-level build includes an unused legacy host layer that does
# not compile on modern MinGW. Build only the MIT CPU/bus sources we use.
add_library(retro_cpu_core STATIC
    "${retro_cpu_SOURCE_DIR}/cpu.cc"
)
target_include_directories(retro_cpu_core SYSTEM PUBLIC "${retro_cpu_SOURCE_DIR}")
target_compile_features(retro_cpu_core PUBLIC cxx_std_17)
add_library(retro_cpu_65816 STATIC
    "${retro_cpu_SOURCE_DIR}/cpu/65816/cpu_65c816.cc"
)
target_include_directories(retro_cpu_65816 SYSTEM PUBLIC "${retro_cpu_SOURCE_DIR}")
target_link_libraries(retro_cpu_65816 PUBLIC retro_cpu_core)
target_compile_features(retro_cpu_65816 PUBLIC cxx_std_17)
if(MINGW)
    target_compile_definitions(retro_cpu_core PUBLIC __FUNCSIG__=__PRETTY_FUNCTION__)
    target_compile_definitions(retro_cpu_65816 PUBLIC __FUNCSIG__=__PRETTY_FUNCTION__)
endif()

add_library(starfox_core
    src/audio/msu1_audio.cpp
    src/audio/msu1_pack.cpp
    src/audio/spc700_audio.cpp
    src/assets/bps.cpp
    src/assets/decrunch.cpp
    src/assets/rom.cpp
    src/assets/runtime_bundle.cpp
    src/assets/runtime_import.cpp
    src/assets/shape_decoder.cpp
    src/render/background_renderer.cpp
    src/render/dust_renderer.cpp
    src/render/dxr_shadows.cpp
    src/render/gpu_projection.cpp
    src/render/gpu_clip.cpp
    src/render/gpu_bsp.cpp
    src/render/gpu_ray_geometry.cpp
    src/render/gpu_background.cpp
    src/render/gpu_colour_warp.cpp
    src/render/packed_bsp.cpp
    src/render/packed_projection.cpp
    src/render/packed_faces.cpp
    src/render/gpu_model.cpp
    src/render/gpu_scene.cpp
    src/render/gpu_effects.cpp
    src/render/sdl_gpu_effects.cpp
    src/render/portable_shadows.cpp
    src/render/gpu_volumetric_fog.cpp
    src/render/gpu_motion_blur.cpp
    src/render/gpu_scene_shutter.cpp
    src/render/sdl_dxr_shadows.cpp
    src/render/vulkan_ray_support.cpp
    src/render/vulkan_hardware_rt.cpp
    src/render/gpu_raster.cpp
    src/render/gpu_composite.cpp
    src/render/gpu_temporal_inputs.cpp
    src/render/gpu_temporal_aa.cpp
    src/render/gpu_smaa.cpp
    src/render/gpu_msaa.cpp
    src/render/gpu_scalefx.cpp
    src/render/gpu_fsr1.cpp
    src/render/d3d11_fsr1.cpp
    src/render/scalefx.cpp
    src/render/framebuffer.cpp
    src/render/palette.cpp
    src/render/particle_renderer.cpp
    src/render/pixel_filter.cpp
    src/render/presentation_history.cpp
    src/render/row_workers.cpp
    src/render/scaled_text_renderer.cpp
    src/render/software_renderer.cpp
    src/render/face_material.cpp
    src/render/asteroid_models.cpp
    src/render/sprite_renderer.cpp
    src/simulation/math.cpp
    src/simulation/dust_system.cpp
    src/simulation/game_simulation.cpp
    src/simulation/game_state.cpp
    src/simulation/map_vm.cpp
    src/simulation/object_pool.cpp
    src/simulation/state_container.cpp
    src/simulation/state_files.cpp
    src/simulation/particle_system.cpp
    src/simulation/path_vm.cpp
    src/simulation/prng.cpp
    src/simulation/rumble_sequencer.cpp
    src/simulation/strategy_scheduler.cpp
    src/simulation/wdc65816.cpp
    src/timing/fixed_step.cpp
    src/vr/source_pose_interpolation.cpp
)

target_include_directories(starfox_core PUBLIC include)
if(IOS)
    target_compile_definitions(starfox_core PUBLIC STARFOX_IOS_RUNTIME)
endif()
target_include_directories(starfox_core SYSTEM PRIVATE "${dr_libs_SOURCE_DIR}")
target_compile_features(starfox_core PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(starfox_core PRIVATE
    retro_cpu_65816 snes_spc_core Threads::Threads)
if(STARFOX_ENABLE_XBRZ)
    target_link_libraries(starfox_core PRIVATE xbrz_filter)
    target_compile_definitions(starfox_core PRIVATE STARFOX_ENABLE_XBRZ)
endif()

# Ordinary effects use SM5 compute, not RT cores. Compile DXBC at build time.
# CPU-only host/3DS test graphs do not use desktop shaders. In particular,
# they have no runtime DXC discovery, so finding an installed SDK FXC must
# not add a DXR custom command with an empty compiler executable.
if(WIN32 AND (STARFOX_BUILD_RUNTIME OR STARFOX_BUILD_VR OR STARFOX_BUILD_TOOLS))
    file(GLOB STARFOX_SDK_FXC "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/fxc.exe")
    list(SORT STARFOX_SDK_FXC COMPARE NATURAL ORDER DESCENDING)
    find_program(STARFOX_FXC NAMES fxc)
    if(NOT STARFOX_FXC AND STARFOX_SDK_FXC)
        list(GET STARFOX_SDK_FXC 0 STARFOX_FXC)
    endif()
    if(STARFOX_FXC)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shadow_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3
                -D STARFOX_DXR_SHADOW_ONLY=1 -D STARFOX_DXR_NATIVE_MATERIALS=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shadow_shader.hpp"
                -Vn shadow_dxr_shadow_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS src/render/shaders/liquid_optics.hlsli src/render/shaders/shadow_dxr.hlsl src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc include/starfox/render/water_caustics.inc VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shadow_shader.hpp")
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr1_d3d11_shader.hpp"
            COMMAND "${STARFOX_FXC}" /nologo /T cs_5_0 /E main /O3 /D STARFOX_FSR1_D3D11=1
                /Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr1_d3d11_shader.hpp" /Vn fsr1_d3d11_shader
                "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/fsr1_portable.hlsl"
            DEPENDS src/render/shaders/fsr1_portable.hlsl third_party/fsr1/ffx_a.h third_party/fsr1/ffx_fsr1.h VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/fsr1_d3d11_shader.hpp")
        target_include_directories(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_core PUBLIC STARFOX_D3D11_FSR1)
        if(STARFOX_BUILD_TOOLS AND NOT WINDOWS_STORE)
            add_executable(starfox_d3d11_fsr1_check tools/check_d3d11_fsr1.cpp)
            target_link_libraries(starfox_d3d11_fsr1_check PRIVATE starfox_core d3d11)
        endif()
        if(NOT WINDOWS_STORE)
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/effects_compute_shader.hpp"
            COMMAND "${STARFOX_FXC}" /nologo /T cs_5_0 /E main /O3
                /D "STARFOX_ENABLE_XBRZ=$<BOOL:${STARFOX_ENABLE_XBRZ}>"
                /Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/effects_compute_shader.hpp"
                /Vn effects_compute_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/effects_compute.hlsl"
            DEPENDS src/render/shaders/effects_compute.hlsl include/starfox/render/special_fx.inc include/starfox/render/global_enhancements.inc include/starfox/render/scene_enhancements.inc include/starfox/render/depth_enhancements.inc include/starfox/render/ambient_occlusion.inc src/render/shaders/xbrz_compute.hlsli src/render/shaders/environment_material.hlsli
                src/render/shaders/backdrop_sample.hlsli
                include/starfox/render/lava_surface.inc
                include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc
                src/render/shaders/xbrz_weights.hlsli src/render/shaders/edge_corners.hlsli VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/effects_compute_shader.hpp")
        target_include_directories(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_core PRIVATE STARFOX_GPU_EFFECTS)
        endif()
    endif()
endif()

# DXR shaders are compiled at build time; no compiler DLL is shipped or loaded
# on user machines. Other platforms retain the CPU shadow implementation.
if(WIN32 AND NOT WINDOWS_STORE AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND STARFOX_BUILD_RUNTIME)
    file(GLOB STARFOX_SDK_DXC "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
    list(SORT STARFOX_SDK_DXC COMPARE NATURAL ORDER DESCENDING)
    find_program(STARFOX_DXC NAMES dxc)
    if(NOT STARFOX_DXC AND STARFOX_SDK_DXC)
        list(GET STARFOX_SDK_DXC 0 STARFOX_DXC)
    endif()
    if(STARFOX_DXC)
        set(STARFOX_DXR_ENABLED TRUE)
        set(STARFOX_DXR_OPTICAL_DEPENDENCIES src/render/shaders/liquid_optics.hlsli)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -D STARFOX_DXR_REFLECTION_ONLY=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shader.hpp"
                -Vn shadow_dxr_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc include/starfox/render/water_caustics.inc VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shader.hpp")
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -D STARFOX_DXR_NATIVE_MATERIALS=1 -D STARFOX_DXR_REFLECTION_ONLY=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_shader.hpp"
                -Vn shadow_dxr_native_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc include/starfox/render/water_caustics.inc VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_shader.hpp")
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_water_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -D STARFOX_DXR_NATIVE_MATERIALS=1
                -D STARFOX_DXR_NATIVE_WATER=1 -D STARFOX_DXR_REFLECTION_ONLY=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_water_shader.hpp"
                -Vn shadow_dxr_native_water_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc include/starfox/render/water_caustics.inc VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_water_shader.hpp")
        foreach(model_shader IN ITEMS models native_models)
            set(model_definitions -D STARFOX_DXR_MODELS_ONLY=1 -D STARFOX_DXR_REFLECTION_ONLY=1)
            if(model_shader STREQUAL "native_models")
                list(APPEND model_definitions -D STARFOX_DXR_NATIVE_MATERIALS=1)
            endif()
            add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_${model_shader}_shader.hpp"
                COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 ${model_definitions}
                    -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_${model_shader}_shader.hpp"
                    -Vn shadow_dxr_${model_shader}_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
                DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc include/starfox/render/water_caustics.inc VERBATIM)
            target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_${model_shader}_shader.hpp")
        endforeach()
        # Accepted secondary-hit motion is an explicit optional producer, not
        # extra registers or driver compilation in ordinary reflection PSOs.
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_history_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -D STARFOX_DXR_NATIVE_MATERIALS=1
                -D STARFOX_DXR_REFLECTION_ONLY=1 -D STARFOX_DXR_REFLECTION_HISTORY=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_history_shader.hpp"
                -Vn shadow_dxr_native_history_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl src/render/shaders/reflection_hit_motion.hlsli
                src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli
                src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli
                include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc
                include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc
                include/starfox/render/water_caustics.inc VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_native_history_shader.hpp")
        # Model lobe history cannot contain a liquid/analytic ground receiver.
        # Compile it as a separate lazy model-only PSO, rather than adding its
        # ray loop to every legacy model and water/lava history pipeline.
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_model_lobes_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -D STARFOX_DXR_NATIVE_MATERIALS=1
                -D STARFOX_DXR_REFLECTION_ONLY=1 -D STARFOX_DXR_MODELS_ONLY=1
                -D STARFOX_DXR_REFLECTION_HISTORY=1 -D STARFOX_DXR_MODEL_LOBE_HISTORY=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_model_lobes_shader.hpp"
                -Vn shadow_dxr_model_lobes_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS src/render/shaders/shadow_dxr.hlsl src/render/shaders/reflection_hit_motion.hlsli
                src/render/shaders/reflection_rough_hit_motion.hlsli src/render/shaders/reflection_lobe_record.hlsli
                src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli
                src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli
                include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc
                include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc
                include/starfox/render/water_caustics.inc ${STARFOX_DXR_OPTICAL_DEPENDENCIES} VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_model_lobes_shader.hpp")
        # Full ordered witnesses and separated per-hop response are deliberately
        # a separate lazy PSO: no register/compile cost in the installed legacy
        # single-hit/liquid/ordinary producer paths.
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_model_paths_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -WX -D STARFOX_DXR_NATIVE_MATERIALS=1
                -D STARFOX_DXR_REFLECTION_ONLY=1 -D STARFOX_DXR_MODELS_ONLY=1
                -D STARFOX_DXR_REFLECTION_HISTORY=1 -D STARFOX_DXR_MODEL_LOBE_HISTORY=1
                -D STARFOX_DXR_MODEL_PATH_HISTORY=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_model_paths_shader.hpp"
                -Vn shadow_dxr_model_paths_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS src/render/shaders/shadow_dxr.hlsl src/render/shaders/reflection_path_record.hlsli
                src/render/shaders/reflection_hit_motion.hlsli src/render/shaders/reflection_rough_hit_motion.hlsli
                src/render/shaders/reflection_lobe_record.hlsli src/render/shaders/calibrated_colour.hlsli
                src/vr/shaders/scene_colour.hlsli src/render/shaders/environment_material.hlsli
                src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc
                include/starfox/render/ex_city_moons.inc include/starfox/render/lava_surface.inc
                include/starfox/render/water_transmission.inc include/starfox/render/water_caustics.inc
                ${STARFOX_DXR_OPTICAL_DEPENDENCIES} VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_model_paths_shader.hpp")
        # Mixed analytic mirror/gold + finite model paths have a distinct
        # CURRENT-base-light prefix. No liquid is approximated by this PSO.
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_scene_paths_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -WX -D STARFOX_DXR_NATIVE_MATERIALS=1
                -D STARFOX_DXR_REFLECTION_ONLY=1 -D STARFOX_DXR_REFLECTION_HISTORY=1
                -D STARFOX_DXR_MODEL_LOBE_HISTORY=1 -D STARFOX_DXR_MODEL_PATH_HISTORY=1
                -D STARFOX_DXR_SCENE_PATH_HISTORY=1
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_scene_paths_shader.hpp"
                -Vn shadow_dxr_scene_paths_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS src/render/shaders/shadow_dxr.hlsl src/render/shaders/reflection_path_record.hlsli
                src/render/shaders/reflection_hit_motion.hlsli src/render/shaders/reflection_rough_hit_motion.hlsli
                src/render/shaders/reflection_lobe_record.hlsli src/render/shaders/calibrated_colour.hlsli
                src/vr/shaders/scene_colour.hlsli src/render/shaders/environment_material.hlsli
                src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc
                include/starfox/render/ex_city_moons.inc include/starfox/render/lava_surface.inc
                include/starfox/render/water_transmission.inc include/starfox/render/water_caustics.inc
                ${STARFOX_DXR_OPTICAL_DEPENDENCIES} VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_scene_paths_shader.hpp")
        # Staged MODEL -> real water/lava witnesses have a distinct 64-byte
        # current-additive-light ABI. Lazy producer PSOs only; no legacy history
        # consumer may reinterpret them or enable reuse before qualification.
        foreach(curved_liquid IN ITEMS water lava)
        foreach(curved_primary IN ITEMS model receivers)
            set(curved_definitions -D STARFOX_DXR_NATIVE_MATERIALS=1 -D STARFOX_DXR_REFLECTION_ONLY=1
                -D STARFOX_DXR_REFLECTION_HISTORY=1
                -D STARFOX_DXR_MODEL_LOBE_HISTORY=1 -D STARFOX_DXR_MODEL_PATH_HISTORY=1
                -D STARFOX_DXR_CURVED_PATH_HISTORY=1)
            set(curved_name "shadow_dxr_curved_${curved_liquid}_paths_shader")
            if(curved_primary STREQUAL "model")
                list(APPEND curved_definitions -D STARFOX_DXR_MODELS_ONLY=1)
            else()
                list(APPEND curved_definitions -D STARFOX_DXR_CURVED_RECEIVER_HISTORY=1)
                set(curved_name "shadow_dxr_curved_${curved_liquid}_receivers_shader")
            endif()
            if(curved_liquid STREQUAL "water")
                list(APPEND curved_definitions -D STARFOX_DXR_NATIVE_WATER=1)
            endif()
            add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/${curved_name}.hpp"
                COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 -WX ${curved_definitions}
                    -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/${curved_name}.hpp"
                    -Vn "${curved_name}" "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
                DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl
                    src/render/shaders/reflection_path_record.hlsli src/render/shaders/reflection_hit_motion.hlsli
                    src/render/shaders/reflection_rough_hit_motion.hlsli src/render/shaders/reflection_lobe_record.hlsli
                    src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli
                    src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli
                    include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc
                    include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc
                    include/starfox/render/water_caustics.inc VERBATIM)
            target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/${curved_name}.hpp")
        endforeach()
        endforeach()
        foreach(liquid_history IN ITEMS water lava)
            set(liquid_history_definitions -D STARFOX_DXR_NATIVE_MATERIALS=1
                -D STARFOX_DXR_REFLECTION_ONLY=1 -D STARFOX_DXR_REFLECTION_HISTORY=1 -D STARFOX_DXR_LIQUID_HISTORY=1)
            if(liquid_history STREQUAL "water")
                list(APPEND liquid_history_definitions -D STARFOX_DXR_NATIVE_WATER=1)
            endif()
            add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_${liquid_history}_history_shader.hpp"
                COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 ${liquid_history_definitions}
                    -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_${liquid_history}_history_shader.hpp"
                    -Vn "shadow_dxr_${liquid_history}_history_shader" "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
                DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl
                    src/render/shaders/reflection_hit_motion.hlsli
                    src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli
                    src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli
                    include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc
                    include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc
                    include/starfox/render/water_caustics.inc VERBATIM)
            target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_${liquid_history}_history_shader.hpp")
        endforeach()
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/reflection_liquid_history_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E main -O3
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/reflection_liquid_history_shader.hpp"
                -Vn reflection_liquid_history_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/reflection_liquid_history.hlsl"
            DEPENDS src/render/shaders/reflection_liquid_history.hlsl src/render/shaders/reflection_liquid_hit_motion.hlsli
                ${STARFOX_DXR_OPTICAL_DEPENDENCIES} include/starfox/render/lava_surface.inc
                include/starfox/render/water_caustics.inc VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/reflection_liquid_history_shader.hpp")
        # Keep the heavier opt-in hit refinement out of ordinary ray PSOs:
        # disabled experiments must not raise register pressure or cold-start
        # driver compilation cost. All variants retain the same resource ABI.
        foreach(stable_shader IN ITEMS reflection native native_water models native_models)
            set(stable_definitions -D STARFOX_DXR_REFLECTION_ONLY=1 -D STARFOX_DXR_STABLE_HITS=1)
            if(stable_shader MATCHES "^native")
                list(APPEND stable_definitions -D STARFOX_DXR_NATIVE_MATERIALS=1)
            endif()
            if(stable_shader STREQUAL "native_water")
                list(APPEND stable_definitions -D STARFOX_DXR_NATIVE_WATER=1)
            endif()
            if(stable_shader MATCHES "models$")
                list(APPEND stable_definitions -D STARFOX_DXR_MODELS_ONLY=1)
            endif()
            add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_stable_${stable_shader}_shader.hpp"
                COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3 ${stable_definitions}
                    -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_stable_${stable_shader}_shader.hpp"
                    -Vn shadow_dxr_stable_${stable_shader}_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
                DEPENDS ${STARFOX_DXR_OPTICAL_DEPENDENCIES} src/render/shaders/shadow_dxr.hlsl src/render/shaders/geometry_fp64.hlsli
                    src/render/shaders/calibrated_colour.hlsli src/vr/shaders/scene_colour.hlsli
                    src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli
                    include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc
                    include/starfox/render/lava_surface.inc include/starfox/render/water_transmission.inc
                    include/starfox/render/water_caustics.inc VERBATIM)
            target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_stable_${stable_shader}_shader.hpp")
        endforeach()
        target_include_directories(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_core PRIVATE STARFOX_DXR)
        if(STARFOX_BUILD_TOOLS)
            add_executable(starfox_dxr_check tools/benchmark_dxr.cpp)
            target_link_libraries(starfox_dxr_check PRIVATE starfox_core)
        endif()
    else()
        message(STATUS "DXC not found: hardware DXR disabled")
    endif()
endif()

if(MSVC)
    target_compile_options(starfox_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(starfox_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(STARFOX_BUILD_TOOLS)
    add_executable(starfox_timing_demo src/app/timing_demo.cpp)
    target_link_libraries(starfox_timing_demo PRIVATE starfox_core)

    add_executable(starfox_shape_preview src/app/shape_preview.cpp)
    target_link_libraries(starfox_shape_preview PRIVATE starfox_core)

    add_executable(starfox_shape_coverage src/app/shape_coverage.cpp)
    target_link_libraries(starfox_shape_coverage PRIVATE starfox_core)

    add_executable(starfox_map_trace src/app/map_trace.cpp)
    target_link_libraries(starfox_map_trace PRIVATE starfox_core)

    add_executable(starfox_strategy_trace src/app/strategy_trace.cpp)
    target_link_libraries(starfox_strategy_trace PRIVATE starfox_core)

    add_executable(starfox_game_init_trace src/app/game_init_trace.cpp)
    target_link_libraries(starfox_game_init_trace PRIVATE starfox_core)

    add_executable(starfox_stage_trace src/app/stage_trace.cpp)
    target_link_libraries(starfox_stage_trace PRIVATE starfox_core)

    add_executable(starfox_stage_preview src/app/stage_preview.cpp)
    target_link_libraries(starfox_stage_preview PRIVATE starfox_core)

    add_executable(starfox_planet_probe src/app/planet_probe.cpp)
    target_link_libraries(starfox_planet_probe PRIVATE starfox_core)
endif()

if(STARFOX_BUILD_RUNTIME)
    if(ANDROID AND NOT TARGET SDL3::SDL3)
        # The Android application consumes SDL's official AAR. Its Prefab
        # package supplies both headers and libSDL3.so to this CMake build.
        find_package(SDL3 REQUIRED CONFIG)
    endif()
    if(NOT TARGET SDL3::SDL3)
        set(SDL_SHARED OFF CACHE BOOL "Avoid a runtime SDL DLL" FORCE)
        set(SDL_STATIC ON CACHE BOOL "Link SDL into the native executable" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build the SDL test library" FORCE)
        if(SWITCH)
            # Public SDL3 does not ship its NDA-gated Nintendo backend. Apply a
            # pinned, zlib-licensed libnx backend for the open devkitPro build.
            # Both the backend revision and its bytes are fixed here so the
            # console build remains reproducible and cannot drift silently.
            set(STARFOX_SDL3_SWITCH_PATCH
                "${CMAKE_CURRENT_BINARY_DIR}/downloads/sdl3-switch.patch")
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/downloads")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/neomody77/sdl3-switch/182e511214d7600e4bdab8606d7caf0ef744afd6/sdl3-switch.patch"
                "${STARFOX_SDL3_SWITCH_PATCH}"
                EXPECTED_HASH
                    "SHA256=9f694599723d78b08c09d99f8203ddea52ee5178575a807263fe79f8ab1f28dc"
                TLS_VERIFY ON
                STATUS STARFOX_SDL3_SWITCH_PATCH_STATUS)
            list(GET STARFOX_SDL3_SWITCH_PATCH_STATUS 0
                STARFOX_SDL3_SWITCH_PATCH_RESULT)
            if(NOT STARFOX_SDL3_SWITCH_PATCH_RESULT EQUAL 0)
                list(GET STARFOX_SDL3_SWITCH_PATCH_STATUS 1
                    STARFOX_SDL3_SWITCH_PATCH_ERROR)
                message(FATAL_ERROR
                    "Unable to download the pinned SDL3 Switch backend: "
                    "${STARFOX_SDL3_SWITCH_PATCH_ERROR}")
            endif()
            find_package(Git REQUIRED)
            set(STARFOX_SDL3_PATCH_COMMAND
                PATCH_COMMAND "${GIT_EXECUTABLE}" apply
                    --whitespace=nowarn -p3
                    "${STARFOX_SDL3_SWITCH_PATCH}"
                COMMAND "${CMAKE_COMMAND}"
                    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                    "-DSOURCE_DIR=<SOURCE_DIR>"
                    "-DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-switch-audio-pipeline.patch"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_git_patch_once.cmake")
        elseif(APPLE OR STARFOX_METAL_INTEROP)
            set(STARFOX_SDL3_PATCH_COMMAND "")
        else()
            set(STARFOX_SDL3_PATCH_COMMAND "")
        endif()
        if(WINDOWS_STORE)
            # Upstream SDL dropped UWP. This maintained SDL 3.4-compatible
            # fork keeps WinRT video/audio, Windows Gaming Input and Xbox Dev
            # Mode.
            # Its SDL3 forward-port retained a desktop-only D3D11 window lookup
            # and omitted thread IDs needed by WASAPI's reentrancy checks.
            # Apply the local UWP runtime fixes reproducibly before configuring
            # the dependency. The helper tolerates CMake reconfiguration after
            # the patch is already present in the fetched source tree.
            find_package(Git REQUIRED)
            FetchContent_Declare(
                SDL3
                GIT_REPOSITORY https://github.com/SternXD/SDL3-uwp.git
                GIT_TAG 8fd8db768df139e9bb4c7a7869fb913a0e29ea89
                GIT_SHALLOW TRUE
                PATCH_COMMAND "${CMAKE_COMMAND}"
                    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                    "-DSOURCE_DIR=<SOURCE_DIR>"
                    "-DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-uwp-d3d11.patch"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_git_patch_once.cmake")
        else()
            FetchContent_Declare(
                SDL3
                URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
                URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                ${STARFOX_SDL3_PATCH_COMMAND})
        endif()
        if(STARFOX_PS5)
            starfox_prepare_ps5_sdl()
        endif()
        FetchContent_MakeAvailable(SDL3)
        if(STARFOX_PS5)
            starfox_attach_ps5_sdl()
        endif()
        if(APPLE OR STARFOX_METAL_INTEROP)
            # URL-based FetchContent patch hooks did not update iOS's SDL
            # source in CI. Inject the narrow native-handle functions into
            # this private pinned SDL copy, before its Metal guard closes.
            set(STARFOX_SDL3_METAL_SOURCE
                "${SDL3_SOURCE_DIR}/src/gpu/metal/SDL_gpu_metal.m")
            if(NOT EXISTS "${STARFOX_SDL3_METAL_SOURCE}")
                message(FATAL_ERROR "SDL Metal backend source missing: ${STARFOX_SDL3_METAL_SOURCE}")
            endif()
            file(READ "${STARFOX_SDL3_METAL_SOURCE}" STARFOX_SDL3_METAL_TEXT)
            string(FIND "${STARFOX_SDL3_METAL_TEXT}" "SDL_StarfoxMetalDevice" STARFOX_SDL3_METAL_SYMBOL_POS)
            string(FIND "${STARFOX_SDL3_METAL_TEXT}" "starfox_metal_interop.inc"
                STARFOX_SDL3_METAL_INCLUDE_POS)
            file(COPY_FILE
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-metal-ray-interop.inc"
                "${SDL3_SOURCE_DIR}/src/gpu/metal/starfox_metal_interop.inc")
            if(STARFOX_SDL3_METAL_SYMBOL_POS EQUAL -1
                AND STARFOX_SDL3_METAL_INCLUDE_POS EQUAL -1)
                set(STARFOX_SDL3_METAL_END "#endif // SDL_GPU_METAL")
                string(FIND "${STARFOX_SDL3_METAL_TEXT}" "${STARFOX_SDL3_METAL_END}"
                    STARFOX_SDL3_METAL_END_POS)
                if(STARFOX_SDL3_METAL_END_POS EQUAL -1)
                    message(FATAL_ERROR "SDL Metal backend guard changed upstream")
                endif()
                string(REPLACE "${STARFOX_SDL3_METAL_END}"
                    "#include \"starfox_metal_interop.inc\"\n${STARFOX_SDL3_METAL_END}"
                    STARFOX_SDL3_METAL_TEXT "${STARFOX_SDL3_METAL_TEXT}")
                file(WRITE "${STARFOX_SDL3_METAL_SOURCE}" "${STARFOX_SDL3_METAL_TEXT}")
            endif()
        endif()
        if(NOT WINDOWS_STORE)
            execute_process(COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-adapter-vendor.cmake"
                COMMAND_ERROR_IS_FATAL ANY)
        endif()
        if((STARFOX_DXR_ENABLED OR (UNIX AND NOT APPLE AND STARFOX_BUILD_RUNTIME))
                AND NOT WINDOWS_STORE)
            if(STARFOX_DXR_ENABLED)
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                    "cmake/sdl3-d3d12-interop.cmake")
                execute_process(COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-d3d12-interop.cmake"
                    COMMAND_ERROR_IS_FATAL ANY)
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "cmake/sdl3-vulkan-interop.cmake" "src/render/sdl_vulkan_bridge.inc"
                "src/render/sdl_vulkan_submit_timing.inc"
                "src/render/sdl_vulkan_xr_create.inc")
            execute_process(COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-vulkan-interop.cmake"
                COMMAND_ERROR_IS_FATAL ANY)
            get_target_property(STARFOX_SDL_NATIVE_TARGET SDL3::SDL3 ALIASED_TARGET)
            target_include_directories(starfox_core PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
            target_include_directories(${STARFOX_SDL_NATIVE_TARGET} PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/render")
            if(STARFOX_BUILD_TOOLS)
                # Run the exact Linux GLSL/SPIR-V without a DXR bridge. Windows
                # verifies the shader only, not Linux SDL ownership/lifetimes.
                add_executable(starfox_native_rayquery_check
                    tools/check_native_rayquery.cpp src/vr/vulkan_loader.cpp)
                target_compile_features(starfox_native_rayquery_check PRIVATE cxx_std_20)
                target_include_directories(starfox_native_rayquery_check PRIVATE
                    include "${sdl3_SOURCE_DIR}/src/video/khronos")
                target_link_libraries(starfox_native_rayquery_check PRIVATE ${CMAKE_DL_LIBS})
                # Exercise the ACTUAL VulkanHardwareRt owner and pinned SDL
                # bridge without enabling this producer in Windows players.
                # A Windows run is not Linux/Deck platform acceptance.
                add_executable(starfox_vulkan_shadow_owner_check
                    tools/check_vulkan_shadow_owner.cpp src/render/vulkan_hardware_rt.cpp
                    src/render/vulkan_ray_support.cpp)
                target_compile_features(starfox_vulkan_shadow_owner_check PRIVATE cxx_std_20)
                target_compile_definitions(starfox_vulkan_shadow_owner_check PRIVATE
                    STARFOX_SDL_GPU_EFFECTS STARFOX_NATIVE_VULKAN_OWNER_PROBE)
                target_include_directories(starfox_vulkan_shadow_owner_check PRIVATE
                    include "${sdl3_SOURCE_DIR}/src/video/khronos")
                target_link_libraries(starfox_vulkan_shadow_owner_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_vulkan_reflection_history_check
                    tools/check_vulkan_reflection_history.cpp src/render/vulkan_hardware_rt.cpp
                    src/render/vulkan_ray_support.cpp)
                target_compile_features(starfox_vulkan_reflection_history_check PRIVATE cxx_std_20)
                target_compile_definitions(starfox_vulkan_reflection_history_check PRIVATE
                    STARFOX_SDL_GPU_EFFECTS STARFOX_NATIVE_VULKAN_OWNER_PROBE)
                target_include_directories(starfox_vulkan_reflection_history_check PRIVATE
                    include "${sdl3_SOURCE_DIR}/src/video/khronos")
                target_link_libraries(starfox_vulkan_reflection_history_check PRIVATE starfox_core SDL3::SDL3)
                # Reuse the exact fixtures/oracles through the live calibrated
                # adapter, rather than a test-only availability switch. Probe
                # definitions remain local; installed Windows players keep DXR.
                foreach(owner_fixture IN ITEMS shadow_owner reflection_history)
                    add_executable(starfox_vulkan_adapter_${owner_fixture}_check
                        tools/check_vulkan_${owner_fixture}.cpp src/render/sdl_dxr_shadows.cpp
                        src/render/vulkan_hardware_rt.cpp src/render/vulkan_ray_support.cpp)
                    target_compile_features(starfox_vulkan_adapter_${owner_fixture}_check PRIVATE cxx_std_20)
                    target_compile_definitions(starfox_vulkan_adapter_${owner_fixture}_check PRIVATE
                        STARFOX_SDL_GPU_EFFECTS STARFOX_NATIVE_VULKAN_OWNER_PROBE STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
                    target_include_directories(starfox_vulkan_adapter_${owner_fixture}_check PRIVATE
                        include "${sdl3_SOURCE_DIR}/src/video/khronos")
                    target_link_libraries(starfox_vulkan_adapter_${owner_fixture}_check PRIVATE starfox_core SDL3::SDL3)
                endforeach()
                if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
                    # Faults affect only this diagnostic's native owner calls.
                    # Linker wrapping is never installed into player builds.
                    add_executable(starfox_vulkan_ray_lifetime_check
                        tools/check_vulkan_ray_lifetime.cpp src/render/vulkan_hardware_rt.cpp
                        src/render/vulkan_ray_support.cpp)
                    target_compile_features(starfox_vulkan_ray_lifetime_check PRIVATE cxx_std_20)
                    target_compile_definitions(starfox_vulkan_ray_lifetime_check PRIVATE
                        STARFOX_SDL_GPU_EFFECTS STARFOX_NATIVE_VULKAN_OWNER_PROBE)
                    target_include_directories(starfox_vulkan_ray_lifetime_check PRIVATE
                        include "${sdl3_SOURCE_DIR}/src/video/khronos")
                    target_link_libraries(starfox_vulkan_ray_lifetime_check PRIVATE starfox_core SDL3::SDL3)
                    target_link_options(starfox_vulkan_ray_lifetime_check PRIVATE
                        "-Wl,--wrap=SDL_SubmitGPUCommandBufferAndAcquireFence"
                        "-Wl,--wrap=SDL_WaitForGPUFences" "-Wl,--wrap=SDL_WaitForGPUIdle"
                        "-Wl,--wrap=SDL_QueryGPUFence" "-Wl,--wrap=SDL_CancelGPUCommandBuffer")
                    add_executable(starfox_vulkan_adapter_ray_lifetime_check
                        tools/check_vulkan_ray_lifetime.cpp src/render/sdl_dxr_shadows.cpp
                        src/render/vulkan_hardware_rt.cpp src/render/vulkan_ray_support.cpp)
                    target_compile_features(starfox_vulkan_adapter_ray_lifetime_check PRIVATE cxx_std_20)
                    target_compile_definitions(starfox_vulkan_adapter_ray_lifetime_check PRIVATE
                        STARFOX_SDL_GPU_EFFECTS STARFOX_NATIVE_VULKAN_OWNER_PROBE STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
                    target_include_directories(starfox_vulkan_adapter_ray_lifetime_check PRIVATE
                        include "${sdl3_SOURCE_DIR}/src/video/khronos")
                    target_link_libraries(starfox_vulkan_adapter_ray_lifetime_check PRIVATE starfox_core SDL3::SDL3)
                    target_link_options(starfox_vulkan_adapter_ray_lifetime_check PRIVATE
                        "-Wl,--wrap=SDL_SubmitGPUCommandBufferAndAcquireFence"
                        "-Wl,--wrap=SDL_WaitForGPUFences" "-Wl,--wrap=SDL_WaitForGPUIdle"
                        "-Wl,--wrap=SDL_QueryGPUFence" "-Wl,--wrap=SDL_CancelGPUCommandBuffer")
                endif()
            endif()
            if(STARFOX_BUILD_TOOLS AND UNIX AND NOT APPLE)
                add_executable(starfox_vulkan_ray_support_check
                    tools/check_vulkan_ray_support.cpp)
                target_link_libraries(starfox_vulkan_ray_support_check PRIVATE starfox_core SDL3::SDL3)
                target_compile_definitions(starfox_vulkan_ray_support_check PRIVATE STARFOX_SDL_GPU_EFFECTS)
                target_include_directories(starfox_vulkan_ray_support_check PRIVATE
                    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${sdl3_SOURCE_DIR}/src/video/khronos")
            endif()
            if(STARFOX_BUILD_TOOLS AND STARFOX_DXR_ENABLED)
                add_executable(starfox_sdl_vulkan_interop_check tools/check_sdl_vulkan_interop.cpp)
                target_link_libraries(starfox_sdl_vulkan_interop_check PRIVATE starfox_core SDL3::SDL3)
                target_include_directories(starfox_sdl_vulkan_interop_check PRIVATE
                    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${sdl3_SOURCE_DIR}/src/video/khronos")
                add_executable(starfox_sdl_d3d12_interop_check tools/check_sdl_d3d12_interop.cpp)
                add_executable(starfox_leia_sr_check tools/check_leia_sr.cpp)
                target_compile_definitions(starfox_leia_sr_check PRIVATE STARFOX_LEIASR_HOST=1)
                target_link_libraries(starfox_leia_sr_check PRIVATE SDL3::SDL3)
                target_include_directories(starfox_leia_sr_check PRIVATE include)
                if(MINGW)
                    target_link_options(starfox_leia_sr_check PRIVATE -static -static-libgcc -static-libstdc++)
                endif()
                target_link_libraries(starfox_sdl_d3d12_interop_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_sdl_xr_color_check tools/check_sdl_xr_color.cpp)
                target_link_libraries(starfox_sdl_xr_color_check PRIVATE SDL3::SDL3)
                target_include_directories(starfox_sdl_xr_color_check PRIVATE
                    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${sdl3_SOURCE_DIR}/src/video/khronos")
                if(MINGW)
                    target_include_directories(starfox_leia_sr_check BEFORE PRIVATE
                        "${sdl3_SOURCE_DIR}/src/video/directx")
                    target_include_directories(starfox_sdl_d3d12_interop_check BEFORE PRIVATE
                        "${sdl3_SOURCE_DIR}/src/video/directx")
                    target_include_directories(starfox_sdl_xr_color_check BEFORE PRIVATE
                        "${sdl3_SOURCE_DIR}/src/video/directx")
                endif()
            endif()
        endif()
    endif()

    if(WIN32 AND NOT WINDOWS_STORE AND DEFINED sdl3_SOURCE_DIR)
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-d3d12-descriptor-reservation.cmake"
            COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-d3d12-copy-ordering.cmake"
            COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-responsive-renderer.cmake"
            COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-responsive-pipelines.cmake"
            COMMAND_ERROR_IS_FATAL ANY)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "cmake/sdl3-d3d12-descriptor-reservation.cmake" "include/starfox/render/d3d12_descriptor_capacity.h"
            "cmake/sdl3-d3d12-copy-ordering.cmake"
            "cmake/sdl3-responsive-renderer.cmake" "include/starfox/render/sdl_gpu_preparation.h"
            "cmake/sdl3-responsive-pipelines.cmake" "include/starfox/render/sdl_gpu_pipeline_preparation.h")
        get_target_property(STARFOX_SDL_NATIVE_TARGET SDL3::SDL3 ALIASED_TARGET)
        target_include_directories(${STARFOX_SDL_NATIVE_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_compile_definitions(starfox_core PUBLIC STARFOX_SDL_RESPONSIVE_RENDERER)
        if(STARFOX_BUILD_TOOLS)
            add_executable(starfox_sdl_gpu_preparation_check tools/check_sdl_gpu_preparation.cpp)
            target_link_libraries(starfox_sdl_gpu_preparation_check PRIVATE starfox_core SDL3::SDL3)
            if(STARFOX_DXR_ENABLED)
                add_executable(starfox_sdl_d3d12_descriptors_check tools/check_sdl_d3d12_descriptors.cpp)
                target_link_libraries(starfox_sdl_d3d12_descriptors_check PRIVATE starfox_core SDL3::SDL3)
                foreach(descriptor_samplers IN ITEMS 0 1)
                    set(descriptor_shader "${CMAKE_CURRENT_BINARY_DIR}/generated/check_d3d12_descriptors_${descriptor_samplers}.dxil")
                    add_custom_command(OUTPUT "${descriptor_shader}"
                        COMMAND "${STARFOX_DXC}" -T cs_6_0 -E main -WX
                            -D "TEST_SAMPLERS=${descriptor_samplers}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tools/shaders/check_d3d12_descriptors.hlsl"
                            -Fo "${descriptor_shader}"
                        DEPENDS tools/shaders/check_d3d12_descriptors.hlsl VERBATIM)
                    list(APPEND descriptor_shaders "${descriptor_shader}")
                endforeach()
                add_custom_target(starfox_d3d12_descriptor_regression_shaders DEPENDS ${descriptor_shaders})
                add_dependencies(starfox_sdl_d3d12_descriptors_check starfox_d3d12_descriptor_regression_shaders)
                add_test(NAME starfox_sdl_d3d12_descriptors_native
                    COMMAND starfox_sdl_d3d12_descriptors_check ${descriptor_shaders})
                set_tests_properties(starfox_sdl_d3d12_descriptors_native PROPERTIES
                    RUN_SERIAL TRUE TIMEOUT 120 LABELS native_gpu)
                if(MINGW)
                    target_include_directories(starfox_sdl_d3d12_descriptors_check BEFORE PRIVATE
                        "${sdl3_SOURCE_DIR}/src/video/directx")
                endif()
            endif()
        endif()
    endif()

    # Native calibrated MSAA needs exact per-sample ownership/depth reads.
    # Keep this private extension on the two pinned desktop GPU backends.
    if(STARFOX_DXR_ENABLED AND NOT WINDOWS_STORE AND DEFINED sdl3_SOURCE_DIR)
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-msaa-read.cmake"
            COMMAND_ERROR_IS_FATAL ANY)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "cmake/sdl3-msaa-read.cmake")
    endif()
    set(STARFOX_RUNTIME_SOURCES
        src/app/starfox_pc.cpp
        src/app/runtime_input.cpp)
    if(STARFOX_DXR_ENABLED)
        # SDL's pinned DirectX headers include DXR interfaces missing from
        # older MinGW installations. MSVC uses its Windows SDK headers.
        if(MINGW AND EXISTS "${sdl3_SOURCE_DIR}/src/video/directx/d3d12.h")
            target_include_directories(starfox_core BEFORE PRIVATE "${sdl3_SOURCE_DIR}/src/video/directx")
        endif()
    endif()
    if(ANDROID)
        # SDLActivity loads libmain.so. SDL_main.h exports SDL_main from the
        # otherwise-identical portable runtime entry point.
        add_library(starfox_pc SHARED ${STARFOX_RUNTIME_SOURCES})
        set_target_properties(starfox_pc PROPERTIES OUTPUT_NAME main)
    else()
        add_executable(starfox_pc ${STARFOX_RUNTIME_SOURCES})
    endif()
    if(WIN32 AND NOT WINDOWS_STORE AND STARFOX_ENABLE_DISPLAYXR)
        include(cmake/DisplayXR.cmake)
    endif()
    if(WIN32 AND NOT WINDOWS_STORE AND STARFOX_ENABLE_LEIASR)
        target_compile_definitions(starfox_pc PRIVATE STARFOX_LEIASR_HOST=1)
    endif()
    if(IOS)
        enable_language(OBJCXX)
        target_sources(starfox_pc PRIVATE src/app/ios_runtime_input_picker.mm)
        set_source_files_properties(src/app/ios_runtime_input_picker.mm
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        target_link_libraries(starfox_pc PRIVATE
            "-framework UIKit" "-framework UniformTypeIdentifiers")
    endif()
    if(WINDOWS_STORE)
        # The Ninja generator's WindowsStore platform adds /APPCONTAINER at
        # link time but does not consistently publish WINAPI_FAMILY to
        # application translation units.  SDL then compiles its library as
        # WinRT while SDL_main.h in the executable selects the desktop Win32
        # entry point.  That mixed startup path packages successfully but is
        # rejected/terminated during Xbox activation.  Make both halves select
        # the same CoreApplication implementation explicitly.
        target_compile_definitions(starfox_pc PRIVATE
            STARFOX_UWP=1
            WINAPI_FAMILY=WINAPI_FAMILY_APP)
        add_library(starfox_uwp_main OBJECT src/app/uwp_main.cpp)
        target_link_libraries(starfox_uwp_main PRIVATE SDL3::Headers)
        target_compile_definitions(starfox_uwp_main PRIVATE
            WINAPI_FAMILY=WINAPI_FAMILY_APP)
        target_compile_options(starfox_uwp_main PRIVATE /ZW)
        set_target_properties(starfox_uwp_main PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_sources(starfox_pc PRIVATE
            $<TARGET_OBJECTS:starfox_uwp_main>)
        # SDL's WinRT entry point expects the blank-cursor resource to be part
        # of the application binary.  The Visual Studio UWP template adds
        # these files automatically, but the Ninja package path does not.
        # Without them SDL can fail during activation before the first frame.
        target_sources(starfox_pc PRIVATE
            "${SDL3_SOURCE_DIR}/src/main/winrt/SDL3-WinRTResources.rc"
            "${SDL3_SOURCE_DIR}/src/main/winrt/SDL3-WinRTResource_BlankCursor.cur")
        target_link_options(starfox_pc PRIVATE
            /APPCONTAINER
            /NODEFAULTLIB:kernel32.lib
            /NODEFAULTLIB:user32.lib
            /DEBUG:FULL
            /OPT:REF /OPT:ICF
            /WINMD
            "/WINMDFILE:${CMAKE_CURRENT_BINARY_DIR}/starfox_pc.winmd")

        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(STARFOX_UWP_ASSET_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/uwp-assets")
        set(STARFOX_UWP_VISUAL_ASSETS
            "${STARFOX_UWP_ASSET_DIRECTORY}/StoreLogo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/Square44x44Logo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/Square150x150Logo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/Wide310x150Logo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/SplashScreen.png")
        add_custom_command(
            OUTPUT ${STARFOX_UWP_VISUAL_ASSETS}
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_uwp_assets.py"
                --output "${STARFOX_UWP_ASSET_DIRECTORY}"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_uwp_assets.py"
            COMMENT "Generating Xbox UWP visual assets"
            VERBATIM)
        set(STARFOX_UWP_MANIFEST
            "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest")
        if(PROJECT_VERSION_TWEAK STREQUAL "")
            set(PROJECT_VERSION_TWEAK 0)
        endif()
        set(STARFOX_UWP_BUILD_REVISION "${PROJECT_VERSION_TWEAK}" CACHE STRING
            "Fourth component of the Xbox UWP package version")
        if(NOT STARFOX_UWP_BUILD_REVISION MATCHES "^[0-9]+$")
            message(FATAL_ERROR
                "STARFOX_UWP_BUILD_REVISION must be an integer from 0 to 65535")
        elseif(STARFOX_UWP_BUILD_REVISION GREATER 65535)
            message(FATAL_ERROR
                "STARFOX_UWP_BUILD_REVISION must be an integer from 0 to 65535")
        endif()
        configure_file(platform/uwp/Package.appxmanifest.in
            "${STARFOX_UWP_MANIFEST}" @ONLY)
        target_sources(starfox_pc PRIVATE
            "${STARFOX_UWP_MANIFEST}" ${STARFOX_UWP_VISUAL_ASSETS})
        set_source_files_properties("${STARFOX_UWP_MANIFEST}" PROPERTIES
            VS_DEPLOYMENT_CONTENT 1)
        set_source_files_properties(${STARFOX_UWP_VISUAL_ASSETS} PROPERTIES
            VS_DEPLOYMENT_CONTENT 1
            VS_DEPLOYMENT_LOCATION "Assets")
        set_target_properties(starfox_pc PROPERTIES
            VS_GLOBAL_AppContainerApplication true
            VS_GLOBAL_ApplicationType "Windows Store"
            VS_GLOBAL_ApplicationTypeRevision "10.0"
            VS_GLOBAL_AppxPackage true
            VS_GLOBAL_AppxPackageSigningEnabled true
            VS_GLOBAL_GenerateAppxPackageOnBuild true
            VS_GLOBAL_UapAppxPackageBuildMode SideloadOnly
            VS_GLOBAL_DefaultLanguage en-US)
        if(STARFOX_UWP_PACKAGE_CERTIFICATE)
            set_target_properties(starfox_pc PROPERTIES
                VS_GLOBAL_PackageCertificateKeyFile
                    "${STARFOX_UWP_PACKAGE_CERTIFICATE}")
        endif()
    endif()
    if(WIN32)
        # The packaged runtime reports failures through a native message box;
        # it should not open a second console window beside the game.
        set_target_properties(starfox_pc PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    if(SWITCH)
        # The public libnx SDL3 backend uses switch-mesa's EGL/GLES renderer.
        # Keep the static graphics libraries in one rescan group because Mesa
        # contains circular references.
        target_link_libraries(starfox_pc PRIVATE starfox_core
            "-Wl,--start-group" SDL3::SDL3 EGL GLESv2 glapi drm_nouveau
            "-Wl,--end-group" nx m)
    else()
        target_link_libraries(starfox_pc PRIVATE starfox_core SDL3::SDL3)
        if(NOT SWITCH AND NOT VITA AND NOT WINDOWS_STORE)
            # Checked-in portable shaders must match their shared source.
            set(STARFOX_EFFECT_SOURCE_TEXT "")
            foreach(shader effects_compute.hlsl xbrz_compute.hlsli xbrz_weights.hlsli edge_corners.hlsli environment_material.hlsli backdrop_sample.hlsli ../../../include/starfox/render/ex_face_planet_regions.inc ../../../include/starfox/render/ex_city_moons.inc ../../../include/starfox/render/lava_surface.inc ../../../include/starfox/render/special_fx.inc ../../../include/starfox/render/global_enhancements.inc ../../../include/starfox/render/scene_enhancements.inc ../../../include/starfox/render/depth_enhancements.inc ../../../include/starfox/render/ambient_occlusion.inc)
                get_filename_component(shader_source "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/${shader}" ABSOLUTE)
                file(READ "${shader_source}" shader_text)
                string(REPLACE "\r\n" "\n" shader_text "${shader_text}")
                string(APPEND STARFOX_EFFECT_SOURCE_TEXT "${shader_text}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${shader_source}")
            endforeach()
            string(SHA256 STARFOX_EFFECT_SOURCE_HASH "${STARFOX_EFFECT_SOURCE_TEXT}")
            foreach(variant 0 1)
                file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/generated/effects_portable_${variant}.hpp"
                    shader_stamp REGEX "^// Source SHA-256:" LIMIT_COUNT 1)
                if(NOT shader_stamp STREQUAL "// Source SHA-256: ${STARFOX_EFFECT_SOURCE_HASH}")
                    message(FATAL_ERROR "Portable effects shaders are stale. Run tools/generate_portable_effects.py; see docs/PORTABLE-GPU.md.")
                endif()
            endforeach()
            target_link_libraries(starfox_core PRIVATE SDL3::SDL3)
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "third_party/fsr1/ffx_a.h" "third_party/fsr1/ffx_fsr1.h")
            # Discover the entire portable family so newly migrated passes cannot
            # accidentally bypass source/header verification. raster_bins predates
            # the naming convention; effects_compute has its combined check above.
            file(GLOB native_shader_sources CONFIGURE_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/*_portable.hlsl")
            list(APPEND native_shader_sources
                "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/raster_bins.hlsl")
            foreach(native_shader_source IN LISTS native_shader_sources)
            get_filename_component(native_shader "${native_shader_source}" NAME_WE)
            file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/${native_shader}.hlsl" shadow_text)
            string(REPLACE "\r\n" "\n" shadow_text "${shadow_text}")
            string(SHA256 shadow_hash "${shadow_text}")
            if(shadow_text MATCHES "#[ \t]*include")
                find_package(Python3 REQUIRED COMPONENTS Interpreter)
                execute_process(COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/portable_shader_source.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/${native_shader}.hlsl"
                    OUTPUT_VARIABLE shadow_hash OUTPUT_STRIP_TRAILING_WHITESPACE
                    COMMAND_ERROR_IS_FATAL ANY)
                file(GLOB_RECURSE shader_helpers CONFIGURE_DEPENDS
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/*.hlsli")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                    ${shader_helpers} "${CMAKE_CURRENT_SOURCE_DIR}/tools/portable_shader_source.py")
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "src/render/shaders/${native_shader}.hlsl"
                "src/render/shaders/generated/${native_shader}.hpp")
            file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/generated/${native_shader}.hpp"
                shadow_stamp REGEX "^// Source SHA-256:" LIMIT_COUNT 1)
            if(NOT shadow_stamp STREQUAL "// Source SHA-256: ${shadow_hash}")
                message(FATAL_ERROR "Portable shader ${native_shader} is stale. Regenerate with tools/generate_portable_shadows.py, or tools/generate_scalefx.py for ScaleFX.")
            endif()
            endforeach()
            target_compile_definitions(starfox_core PRIVATE STARFOX_SDL_GPU_EFFECTS)
            target_compile_definitions(starfox_pc PRIVATE STARFOX_SDL_GPU_EFFECTS)
            if(STARFOX_BUILD_TOOLS)
                add_executable(starfox_gpu_effects_check tools/benchmark_gpu_effects.cpp)
                add_executable(starfox_present_check tools/check_present_pacing.cpp)
                target_link_libraries(starfox_present_check PRIVATE SDL3::SDL3)
                if(MINGW)
                    target_link_options(starfox_present_check PRIVATE -static -static-libgcc -static-libstdc++)
                endif()
                add_executable(starfox_gpu_fsr1_check tools/check_gpu_fsr1.cpp)
                add_executable(starfox_gpu_volumetric_check tools/check_gpu_volumetric.cpp)
                target_link_libraries(starfox_gpu_volumetric_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_fsr1_check PRIVATE starfox_core SDL3::SDL3)
                target_compile_definitions(starfox_gpu_effects_check PRIVATE STARFOX_TEST_PORTABLE_GPU)
                target_link_libraries(starfox_gpu_effects_check PRIVATE starfox_core SDL3::SDL3)
                if(WIN32)
                    target_link_libraries(starfox_gpu_effects_check PRIVATE d3d11)
                endif()
                add_executable(starfox_portable_shadows_check tools/benchmark_dxr.cpp)
                target_compile_definitions(starfox_portable_shadows_check PRIVATE STARFOX_TEST_PORTABLE_SHADOWS)
                target_link_libraries(starfox_portable_shadows_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_raster_check tools/benchmark_gpu_raster.cpp)
                target_link_libraries(starfox_gpu_raster_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_sprites_check tools/check_gpu_sprites.cpp)
                target_link_libraries(starfox_gpu_sprites_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_projection_check tools/check_gpu_projection.cpp)
                target_link_libraries(starfox_gpu_projection_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_bsp_check tools/check_gpu_bsp.cpp)
                add_executable(starfox_gpu_model_check tools/check_gpu_model.cpp)
                add_executable(starfox_gpu_terrain_batches_check tools/check_gpu_terrain_batches.cpp)
                target_link_libraries(starfox_gpu_terrain_batches_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_model_batches_check tools/check_gpu_model_batches.cpp)
                target_link_libraries(starfox_gpu_model_batches_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_depth_check tools/check_gpu_depth.cpp)
                target_link_libraries(starfox_gpu_depth_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_ray_geometry_check tools/check_gpu_ray_geometry.cpp)
                add_executable(starfox_gpu_background_check tools/check_gpu_background.cpp)
                target_link_libraries(starfox_gpu_background_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_ray_geometry_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_colour_warp_check tools/check_gpu_colour_warp.cpp)
                add_executable(starfox_gpu_fp64_check tools/check_gpu_fp64.cpp)
                target_compile_features(starfox_gpu_fp64_check PRIVATE cxx_std_20)
                target_link_libraries(starfox_gpu_fp64_check PRIVATE SDL3::SDL3)
                add_executable(starfox_gpu_warp_material_check tools/check_gpu_warp_material.cpp)
                add_executable(starfox_gpu_warp_expand_check tools/check_gpu_warp_expand.cpp)
                add_executable(starfox_gpu_warp_chain_check tools/check_gpu_warp_chain.cpp)
                target_link_libraries(starfox_gpu_warp_chain_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_warp_expand_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_warp_material_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_colour_warp_check PRIVATE SDL3::SDL3)
                add_executable(starfox_euler_precision_check tools/check_euler_precision.cpp)
                target_link_libraries(starfox_euler_precision_check PRIVATE starfox_core)
                target_link_libraries(starfox_gpu_model_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_bsp_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_clip_check tools/check_gpu_clip.cpp)
                target_link_libraries(starfox_gpu_clip_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_composite_check tools/check_gpu_composite.cpp)
                add_executable(starfox_gpu_temporal_aa_check tools/check_gpu_temporal_aa.cpp)
                target_link_libraries(starfox_gpu_temporal_aa_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_stereo_check tools/check_gpu_stereo.cpp)
                target_link_libraries(starfox_gpu_stereo_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_composite_check PRIVATE starfox_core SDL3::SDL3)
                if(WIN32)
                    target_link_libraries(starfox_gpu_composite_check PRIVATE dxgi)
                endif()
                add_executable(starfox_scalefx_check tools/check_scalefx.cpp)
                target_link_libraries(starfox_scalefx_check PRIVATE starfox_core SDL3::SDL3)
            endif()
        endif()
    endif()
    if(APPLE)
        enable_language(OBJCXX)
        include(cmake/NativeMetalMaterialSource.cmake)
        if(STARFOX_BUILD_TOOLS AND NOT IOS)
            find_package(Python3 COMPONENTS Interpreter REQUIRED)
            set(STARFOX_NATIVE_METAL_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/native-metal-check")
            add_custom_command(OUTPUT "${STARFOX_NATIVE_METAL_SOURCE_DIR}/reflection.metal"
                    "${STARFOX_NATIVE_METAL_SOURCE_DIR}/shadow.metal"
                    "${STARFOX_NATIVE_METAL_SOURCE_DIR}/empty_reflection.metal"
                    "${STARFOX_NATIVE_METAL_SOURCE_DIR}/empty_shadow.metal"
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_metal_rt_shaders.py"
                    --emit-only "${STARFOX_NATIVE_METAL_SOURCE_DIR}"
                    --generated-dir "${CMAKE_CURRENT_BINARY_DIR}/generated"
                DEPENDS tools/check_metal_rt_shaders.py src/render/metal_hardware_rt.mm
                    src/render/shaders/metal_water_shared.hpp.in
                    src/render/shaders/metal_native_material_shared.hpp.in
                    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_water_shared.hpp"
                    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_native_material_shared.hpp"
                    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_liquid_optics.inc"
                    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_material_colour.inc"
                    include/starfox/render/lava_surface.inc include/starfox/render/water_caustics.inc
                    include/starfox/render/water_transmission.inc
                VERBATIM)
            add_custom_target(starfox_native_metal_source DEPENDS
                "${STARFOX_NATIVE_METAL_SOURCE_DIR}/reflection.metal"
                "${STARFOX_NATIVE_METAL_SOURCE_DIR}/shadow.metal"
                "${STARFOX_NATIVE_METAL_SOURCE_DIR}/empty_reflection.metal"
                "${STARFOX_NATIVE_METAL_SOURCE_DIR}/empty_shadow.metal")
            add_executable(starfox_native_metal_check tools/check_native_metal.mm)
            add_dependencies(starfox_native_metal_check starfox_native_metal_source)
            target_compile_features(starfox_native_metal_check PRIVATE cxx_std_20)
            target_include_directories(starfox_native_metal_check PRIVATE include)
            set_source_files_properties(tools/check_native_metal.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
            target_link_libraries(starfox_native_metal_check PRIVATE "-framework Metal" "-framework Foundation")
        endif()
        target_include_directories(starfox_pc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_sources(starfox_pc PRIVATE src/render/metal_hardware_rt.mm)
        set_source_files_properties(src/render/metal_hardware_rt.mm
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        target_link_libraries(starfox_pc PRIVATE "-framework Metal")
        set_target_properties(starfox_pc PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME "StarFoxEnhanced"
            MACOSX_BUNDLE_BUNDLE_NAME "Star Fox Enhanced"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.starfox.enhanced"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_INFO_PLIST
                "${CMAKE_CURRENT_SOURCE_DIR}/platform/apple/Info.plist.in")
    endif()
    if(IOS)
        set_target_properties(starfox_pc PROPERTIES
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    endif()
    if(SWITCH)
        set(STARFOX_SWITCH_NACP
            "${CMAKE_CURRENT_BINARY_DIR}/StarFoxEnhanced.nacp")
        if(COMMAND nx_generate_nacp AND COMMAND nx_create_nro)
            nx_generate_nacp(
                OUTPUT "${STARFOX_SWITCH_NACP}"
                NAME "Star Fox Enhanced"
                AUTHOR "Star Fox Enhanced team"
                VERSION "${PROJECT_VERSION}")
            nx_create_nro(starfox_pc_nro
                TARGET starfox_pc
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/StarFoxEnhanced.nro"
                NACP "${STARFOX_SWITCH_NACP}")
        elseif(COMMAND nx_generate_nro)
            nx_generate_nro(starfox_pc
                NAME "Star Fox Enhanced"
                AUTHOR "Star Fox Enhanced team")
        else()
            message(FATAL_ERROR
                "The devkitPro Switch CMake helpers are unavailable")
        endif()
    endif()

    if(STARFOX_PACKAGE_MSU1_MUSIC)
        FetchContent_Declare(
            starfox_msu_pack
            GIT_REPOSITORY https://github.com/Sunlitspace542/star-fox-msu1-pcm.git
            GIT_TAG 7a86296d7f62fd26bc2c51a7ed43eda8e4b21588
            SOURCE_SUBDIR _starfox_no_upstream_project
        )
        FetchContent_MakeAvailable(starfox_msu_pack)
        set(STARFOX_MSU_TRACK_FILES
            titledemo title controls trainingmode map blackholemap
            briefing briefingshort scramble level1_1 level1_2 level1_3
            level1_4 level1_6 level2_2 level2_3 level2_4 level2_6
            level3_3 level3_5 boss1_1 boss1_2 boss1_3 boss1_4 boss2_3
            boss3_3 boss3_5 boss1_5a boss1_5b boss1_5c bossandross
            bossootd levelblackhole levelootd clearband clearorch
            clearootd downband downorch gameover continue secretactivated
            blackholewarp blackholeexit dangerous lastbossclear
            gamecleardemo bossroll staffroll enterhyperspace1
            enterhyperspace2 exithyperspace)
        set(STARFOX_MSU_TRACK_PATHS "")
        foreach(STARFOX_MSU_TRACK IN LISTS STARFOX_MSU_TRACK_FILES)
            set(STARFOX_MSU_TRACK_PATH
                "${starfox_msu_pack_SOURCE_DIR}/${STARFOX_MSU_TRACK}.flac")
            if(NOT EXISTS "${STARFOX_MSU_TRACK_PATH}")
                message(FATAL_ERROR
                    "MSU-1 pack is missing ${STARFOX_MSU_TRACK}.flac")
            endif()
            list(APPEND STARFOX_MSU_TRACK_PATHS "${STARFOX_MSU_TRACK_PATH}")
        endforeach()
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(STARFOX_MSU1_PACK
            "${CMAKE_CURRENT_BINARY_DIR}/Starfox-MSU1.PAK")
        add_custom_command(
            OUTPUT "${STARFOX_MSU1_PACK}"
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_msu1.py"
                --output "${STARFOX_MSU1_PACK}"
                ${STARFOX_MSU_TRACK_PATHS}
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_msu1.py"
                ${STARFOX_MSU_TRACK_PATHS}
            COMMENT "Packing optional MSU-1 soundtrack companion"
            VERBATIM)
        add_custom_target(starfox_msu1_pack ALL
            DEPENDS "${STARFOX_MSU1_PACK}")
        add_dependencies(starfox_pc starfox_msu1_pack)
        install(FILES "${STARFOX_MSU1_PACK}" DESTINATION .)
    endif()

    set(STARFOX_EMBEDDED_INPUTS_AVAILABLE FALSE)
    if(STARFOX_EMBED_RUNTIME_ASSETS
       AND EXISTS "${STARFOX_ORIGINAL_PATCH_FILE}"
       AND EXISTS "${STARFOX_SYMBOLS_FILE}"
       AND EXISTS "${STARFOX_EX_PATCH_FILE}"
       AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
        set(STARFOX_EMBEDDED_INPUTS_AVAILABLE TRUE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(STARFOX_GENERATED_EMBEDDED_ASSETS
            "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_assets.cpp")
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnhancedBackdropAssets.cmake")
        starfox_enhanced_backdrop_resources(
            STARFOX_EMBED_BACKDROP_ARGS STARFOX_EMBED_BACKDROP_FILES)
        list(LENGTH STARFOX_EMBED_BACKDROP_FILES STARFOX_EMBED_BACKDROP_COUNT)
        math(EXPR STARFOX_EMBED_BACKDROP_LAST_ID "199 + ${STARFOX_EMBED_BACKDROP_COUNT}")
        set(STARFOX_EMBED_CHUNK_ARGS)
        set(STARFOX_EMBED_SPLIT_OUTPUTS)
        if(ANDROID OR WINDOWS_STORE)
            # MSVC's compiler heap cannot concatenate a multi-megabyte BMP
            # literal; mobile compilers also need bounded literal sizes.
            list(APPEND STARFOX_EMBED_CHUNK_ARGS --chunk-bytes 8192)
        endif()
        if(ANDROID)
            # A single source file containing every backdrop exhausts mobile
            # runner RAM. Compile each resource in a bounded translation unit.
            list(APPEND STARFOX_EMBED_CHUNK_ARGS --split-dir
                "${CMAKE_CURRENT_BINARY_DIR}/generated")
            foreach(resource_id IN ITEMS 101 102 108 109 120 121 122 123 124 125 126)
                list(APPEND STARFOX_EMBED_SPLIT_OUTPUTS
                    "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_resource_${resource_id}.cpp")
            endforeach()
            foreach(resource_id RANGE 200 ${STARFOX_EMBED_BACKDROP_LAST_ID})
                list(APPEND STARFOX_EMBED_SPLIT_OUTPUTS
                    "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_resource_${resource_id}.cpp")
            endforeach()
        endif()
        add_custom_command(
            OUTPUT "${STARFOX_GENERATED_EMBEDDED_ASSETS}"
                ${STARFOX_EMBED_SPLIT_OUTPUTS}
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime_assets.py"
                --output "${STARFOX_GENERATED_EMBEDDED_ASSETS}"
                ${STARFOX_EMBED_CHUNK_ARGS}
                --resource "101=${STARFOX_ORIGINAL_PATCH_FILE}"
                --resource "102=${STARFOX_SYMBOLS_FILE}"
                --resource "108=${STARFOX_EX_PATCH_FILE}"
                --resource "109=${STARFOX_EX_SYMBOLS_FILE}"
                --resource "120=${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                --resource "121=${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                --resource "122=${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                --resource "123=${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                --resource "124=${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                --resource "125=${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                --resource "126=${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}"
                ${STARFOX_EMBED_BACKDROP_ARGS}
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime_assets.py"
                ${STARFOX_EMBED_BACKDROP_FILES}
                "${STARFOX_ORIGINAL_PATCH_FILE}"
                "${STARFOX_SYMBOLS_FILE}"
                "${STARFOX_EX_PATCH_FILE}"
                "${STARFOX_EX_SYMBOLS_FILE}"
                "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}"
            COMMENT "Embedding portable runtime patch resources"
            VERBATIM)
        add_library(starfox_embedded_assets STATIC
            "${STARFOX_GENERATED_EMBEDDED_ASSETS}"
            ${STARFOX_EMBED_SPLIT_OUTPUTS})
        target_include_directories(starfox_embedded_assets PRIVATE include)
        target_compile_features(starfox_embedded_assets PRIVATE cxx_std_20)
        if(STARFOX_EMBED_SPLIT_OUTPUTS AND CMAKE_GENERATOR MATCHES "Ninja"
           AND NOT STARFOX_BUILD_VR)
            set_property(GLOBAL APPEND PROPERTY JOB_POOLS starfox_embed_compile=2)
            set_property(TARGET starfox_embedded_assets PROPERTY
                JOB_POOL_COMPILE starfox_embed_compile)
        endif()

        if(NOT ANDROID AND NOT IOS AND NOT WINDOWS_STORE
           AND NOT SWITCH AND NOT VITA AND NOT STARFOX_PS5)
            add_executable(starfox_asset_builder src/app/asset_builder.cpp)
            target_link_libraries(starfox_asset_builder PRIVATE
                starfox_core starfox_embedded_assets)
            if(STARFOX_BUILD_VR)
                install(TARGETS starfox_asset_builder
                    RUNTIME DESTINATION . COMPONENT pcvr)
            endif()
        endif()
    endif()

    if(WIN32 AND NOT WINDOWS_STORE AND STARFOX_EMBED_RUNTIME_ASSETS)
        if(EXISTS "${STARFOX_ORIGINAL_PATCH_FILE}"
           AND EXISTS "${STARFOX_SYMBOLS_FILE}"
           AND EXISTS "${STARFOX_EX_PATCH_FILE}"
           AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}"
           AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
            enable_language(RC)
            file(TO_CMAKE_PATH
                "${STARFOX_ORIGINAL_PATCH_FILE}" STARFOX_EMBEDDED_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_SYMBOLS_FILE}" STARFOX_EMBEDDED_SYMBOLS)
            file(TO_CMAKE_PATH
                "${STARFOX_EX_PATCH_FILE}" STARFOX_EX_EMBEDDED_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_EX_SYMBOLS_FILE}" STARFOX_EX_EMBEDDED_SYMBOLS)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                STARFOX_RETAIL_JAPAN_V10_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                STARFOX_RETAIL_JAPAN_V11_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                STARFOX_RETAIL_USA_V10_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                STARFOX_RETAIL_USA_V11_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                STARFOX_RETAIL_EUROPE_V10_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                STARFOX_RETAIL_EUROPE_V11_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}"
                STARFOX_RETAIL_GERMANY_V10_PATCH)
            configure_file(
                src/app/starfox_assets.rc.in
                "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc"
                @ONLY
            )
            # Keep the resource compiler isolated from the C++ target's
            # include path. GNU windres forwards quoted -I paths through its
            # preprocessor incorrectly when the workspace contains spaces.
            add_library(starfox_runtime_resources OBJECT
                "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc")
            set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc"
                PROPERTY OBJECT_DEPENDS
                    "${STARFOX_ORIGINAL_PATCH_FILE};${STARFOX_SYMBOLS_FILE};${STARFOX_EX_PATCH_FILE};${STARFOX_EX_SYMBOLS_FILE};${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE};${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE};${STARFOX_RETAIL_USA_V10_PATCH_FILE};${STARFOX_RETAIL_USA_V11_PATCH_FILE};${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE};${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE};${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
            target_sources(starfox_pc PRIVATE
                $<TARGET_OBJECTS:starfox_runtime_resources>)
            set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc" APPEND PROPERTY OBJECT_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/alpine-day-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/macbeth-dusk-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-night-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/desert-horizon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-volcanic-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/city-night-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/nebula-red-blue-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cloud-plains-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/gold-storm-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dune-horizon-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/distant-snow-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/titania-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ocean-clouds-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/night-coast-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/day-coast-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/red-cloud-band-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/rocky-green-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dense-city-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/asteroid-belt-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/fine-debris-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-nebula-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/comet-corona-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cratered-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/banded-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/blue-cloud-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-sky-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/jade-planet-horizon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dimension-vortex-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/amber-blue-cloud-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/inferno-band-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/spectral-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/face-moon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/deep-space-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-ocean-surface-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-lava-surface-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/kazaru-volcanic-hills-v1.bmp")
            target_compile_definitions(starfox_pc PRIVATE
                STARFOX_HAS_EMBEDDED_ASSETS=1)
            set(STARFOX_RUNTIME_ASSETS_EMBEDDED TRUE)
        else()
            message(WARNING
                "Embedded runtime assets requested, but an Original or Star Fox EX "
                "BPS/symbol pair is missing. starfox_pc will require external assets.")
        endif()
    endif()
    include(cmake/EmbeddedDlss.cmake)
    if((NOT WIN32 OR WINDOWS_STORE) AND STARFOX_EMBED_RUNTIME_ASSETS)
        if(STARFOX_EMBEDDED_INPUTS_AVAILABLE)
            target_link_libraries(starfox_pc PRIVATE starfox_embedded_assets)
            target_compile_definitions(starfox_pc PRIVATE
                STARFOX_HAS_EMBEDDED_ASSETS=1)
            set(STARFOX_RUNTIME_ASSETS_EMBEDDED TRUE)
        else()
            message(WARNING
                "Portable embedded assets requested, but a BPS/symbol pair "
                "is missing. starfox_pc will require external assets.")
        endif()
    endif()
    if(MINGW)
        target_link_options(starfox_pc PRIVATE -static -static-libgcc -static-libstdc++)
        if(TARGET starfox_asset_builder)
            # The builder is distributed as a single standalone executable.
            # Do not leave users dependent on whichever MinGW runtime happened
            # to be installed on the CI worker that produced the release.
            target_link_options(starfox_asset_builder PRIVATE
                -static -static-libgcc -static-libstdc++)
        endif()
    endif()
    if(VITA)
        if(NOT DEFINED ENV{VITASDK} OR "$ENV{VITASDK}" STREQUAL "")
            message(FATAL_ERROR "VITASDK must point to an installed VitaSDK")
        endif()
        include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
        set(STARFOX_VITA_TITLE_ID "SFXE00001" CACHE STRING
            "Nine-character PS Vita title id")
        set(STARFOX_VITA_APP_VERSION "00.07" CACHE STRING
            "Two-component PS Vita application version")
        # SCE metadata is appended to the executable segment by vita-elf-create.
        # A separate adjacent read-only segment leaves no room for that data.
        target_link_options(starfox_pc PRIVATE
            "-Wl,--defsym=__sce_headroom=0x10000")
        vita_create_self(eboot.bin starfox_pc)
        vita_create_vpk(StarFoxEnhanced.vpk
            "${STARFOX_VITA_TITLE_ID}" eboot.bin
            VERSION "${STARFOX_VITA_APP_VERSION}"
            NAME "Star Fox Enhanced")
    endif()

    if(STARFOX_PS5)
        starfox_add_ps5_title(starfox_pc)
    endif()

    install(TARGETS starfox_pc
        RUNTIME DESTINATION .
        LIBRARY DESTINATION .
        BUNDLE DESTINATION .)
    if(WIN32 AND NOT WINDOWS_STORE AND STARFOX_ENABLE_LEIASR)
        # Built separately with MSVC so MinGW never imports SDK C++ symbols.
        # The helper installs the module/notices beside this build's executable.
        install(FILES "${CMAKE_BINARY_DIR}/starfox_leia_sr.dll" DESTINATION . OPTIONAL)
        install(DIRECTORY "${CMAKE_BINARY_DIR}/licenses/leia-sr" DESTINATION licenses OPTIONAL)
        if(TARGET starfox_leia_sr_check)
            install(TARGETS starfox_leia_sr_check RUNTIME DESTINATION .)
            install(FILES tools/package/Diagnose-Leia-SR.cmd tools/package/LEIA-SR.txt DESTINATION .)
        endif()
    endif()
    if(TARGET starfox_asset_builder)
        install(TARGETS starfox_asset_builder RUNTIME DESTINATION .)
    endif()
    install(FILES README.md CREDITS.md THIRD_PARTY_NOTICES.md LICENSE-XBRZ.txt DESTINATION .)
    install(FILES assets/fonts/README.md assets/fonts/misaki.txt
        DESTINATION licenses/fonts)
    # Player packages contain runtime files and notices only. Development
    # audits, architecture notes and release-work checkpoints stay in source.

endif()

if(STARFOX_BUILD_TESTS)
    add_executable(starfox_sr_platform_policy_tests tests/sr_platform_policy_tests.cpp)
    target_include_directories(starfox_sr_platform_policy_tests PRIVATE include)
    target_compile_features(starfox_sr_platform_policy_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_sr_platform_policy_tests COMMAND starfox_sr_platform_policy_tests)
    # Headless smoke coverage is independent of local saved graphics options.
    set(STARFOX_SMOKE_BASELINE "STARFOX_TEST_RENDERER=SOFTWARE")
    foreach(setting RAY_TRACING SOFTWARE_SHADOWS REFLECTIVE_SURFACES RTX_LIGHTING
            BLOOM BLOOM_2D EFFECT WORLD_EFFECT MATERIAL MANIPULATION HDR_EFFECT
            CHROMATIC_ABERRATION MODEL_SMOOTHING ANTI_ALIASING 2D_FILTER
            DLSS_SELECTION DLSS45_SELECTION FSR1_SELECTION STEREO_OUTPUT CAMERA_RESPONSE
            GLOBAL_ENHANCEMENTS SCENE_ENHANCEMENTS DEPTH_ENHANCEMENTS PARTICLE_ENHANCEMENTS
            PHOSPHOR_PERSISTENCE ADAPTIVE_EXPOSURE WATER_CAUSTICS VOLUMETRIC_FOG MOTION_BLUR_QUALITY ENVIRONMENT_0
            ENVIRONMENT_1 ENVIRONMENT_2 ENVIRONMENT_3 ENVIRONMENT_4 ENVIRONMENT_5)
        list(APPEND STARFOX_SMOKE_BASELINE "STARFOX_TEST_${setting}=0")
    endforeach()
    enable_testing()
    add_test(NAME starfox_sdl_d3d12_copy_ordering_patch_tests
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/sdl-copy-ordering-fixtures"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdl_d3d12_copy_ordering.cmake")
    add_test(NAME starfox_reflection_shader_dependency_tests
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reflection_shader_dependencies.cmake")
    # This host ABI must compile without optional OpenXR/VR include paths.
    add_executable(starfox_calibrated_dlss_api_tests tests/calibrated_dlss_api_tests.cpp)
    target_include_directories(starfox_calibrated_dlss_api_tests PRIVATE include)
    target_compile_features(starfox_calibrated_dlss_api_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_calibrated_dlss_api_tests COMMAND starfox_calibrated_dlss_api_tests)
    # The original-3DS frontend contracts are testable without a Nintendo SDK
    # or proprietary assets. Native packaging remains a separate platform build.
    add_library(starfox_3ds_frontend STATIC platform/3ds/frontend.cpp platform/3ds/cockpit_layout.cpp
        platform/3ds/game_hud_editor.cpp platform/3ds/game_remap.cpp platform/3ds/game_quick_menu.cpp)
    target_include_directories(starfox_3ds_frontend PUBLIC include)
    target_compile_features(starfox_3ds_frontend PUBLIC cxx_std_20)
    add_executable(starfox_3ds_frontend_tests tests/nintendo_3ds_frontend_tests.cpp)
    target_link_libraries(starfox_3ds_frontend_tests PRIVATE starfox_3ds_frontend)
    add_test(NAME starfox_3ds_frontend_tests COMMAND starfox_3ds_frontend_tests)
    add_executable(starfox_3ds_game_remap_tests tests/nintendo_3ds_game_remap_tests.cpp)
    target_link_libraries(starfox_3ds_game_remap_tests PRIVATE starfox_3ds_frontend)
    add_test(NAME starfox_3ds_game_remap_tests COMMAND starfox_3ds_game_remap_tests)
    add_executable(starfox_3ds_game_quick_menu_tests tests/nintendo_3ds_game_quick_menu_tests.cpp)
    target_link_libraries(starfox_3ds_game_quick_menu_tests PRIVATE starfox_3ds_frontend)
    add_test(NAME starfox_3ds_game_quick_menu_tests COMMAND starfox_3ds_game_quick_menu_tests)
    add_executable(starfox_3ds_audio_tests tests/nintendo_3ds_audio_tests.cpp)
    target_link_libraries(starfox_3ds_audio_tests PRIVATE starfox_3ds_frontend)
    add_test(NAME starfox_3ds_audio_tests COMMAND starfox_3ds_audio_tests)
    # Link the real NDSP adapter to explicit test doubles. Native SDK/firmware
    # and physical audio acceptance remain separate from these host contracts.
    add_executable(starfox_3ds_native_audio_tests tests/nintendo_3ds_native_audio_tests.cpp
        platform/3ds/native_audio.cpp)
    target_include_directories(starfox_3ds_native_audio_tests PRIVATE tests/3ds_audio_stubs include)
    target_compile_features(starfox_3ds_native_audio_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_3ds_native_audio_tests COMMAND starfox_3ds_native_audio_tests)
    add_executable(starfox_3ds_native_stem_executor_tests
        tests/nintendo_3ds_native_stem_executor_tests.cpp platform/3ds/native_stem_executor.cpp)
    target_include_directories(starfox_3ds_native_stem_executor_tests PRIVATE
        tests/3ds_stem_stubs include platform/3ds)
    target_compile_features(starfox_3ds_native_stem_executor_tests PRIVATE cxx_std_20)
    target_link_libraries(starfox_3ds_native_stem_executor_tests PRIVATE Threads::Threads)
    add_test(NAME starfox_3ds_native_stem_executor_tests COMMAND starfox_3ds_native_stem_executor_tests)
    # Host libctru doubles are confined to these regression targets. Never
    # substitute them for the real SDK in the separately packaged ARM player.
    add_library(starfox_3ds_stem_test_runtime STATIC
        tests/nintendo_3ds_native_stem_executor_tests.cpp platform/3ds/native_stem_executor.cpp)
    target_include_directories(starfox_3ds_stem_test_runtime PRIVATE
        tests/3ds_stem_stubs include platform/3ds)
    target_compile_features(starfox_3ds_stem_test_runtime PRIVATE cxx_std_20)
    target_compile_definitions(starfox_3ds_stem_test_runtime PRIVATE
        main=worker_api_fixture_main STARFOX_STEM_TEST_NEW_MODEL=1)
    target_link_libraries(starfox_3ds_stem_test_runtime PRIVATE Threads::Threads)
    add_executable(starfox_spc_stems_tests tests/spc_stems_tests.cpp)
    target_link_libraries(starfox_spc_stems_tests PRIVATE starfox_core)
    add_test(NAME starfox_spc_stems_tests COMMAND starfox_spc_stems_tests)
    add_executable(starfox_spc_parallel_stems_tests tests/spc_stems_tests.cpp)
    target_compile_definitions(starfox_spc_parallel_stems_tests PRIVATE STARFOX_SPC_PARALLEL_ORACLE=1)
    target_include_directories(starfox_spc_parallel_stems_tests PRIVATE platform/3ds)
    target_link_libraries(starfox_spc_parallel_stems_tests PRIVATE
        starfox_core starfox_3ds_stem_test_runtime Threads::Threads)
    add_test(NAME starfox_spc_parallel_stems_tests COMMAND starfox_spc_parallel_stems_tests)
    add_executable(starfox_3ds_pica_tests tests/nintendo_3ds_pica_tests.cpp)
    target_link_libraries(starfox_3ds_pica_tests PRIVATE starfox_3ds_frontend)
    add_test(NAME starfox_3ds_pica_tests COMMAND starfox_3ds_pica_tests)
    add_library(starfox_3ds_source_geometry STATIC platform/3ds/pica_shapes.cpp platform/3ds/pica_source_spans.cpp)
    target_link_libraries(starfox_3ds_source_geometry PUBLIC starfox_3ds_frontend starfox_core)
    add_executable(starfox_3ds_source_geometry_tests tests/nintendo_3ds_source_geometry_tests.cpp)
    target_link_libraries(starfox_3ds_source_geometry_tests PRIVATE starfox_3ds_source_geometry)
    add_test(NAME starfox_3ds_source_geometry_tests COMMAND starfox_3ds_source_geometry_tests)
    add_executable(starfox_3ds_source_spans_tests tests/nintendo_3ds_source_spans_tests.cpp)
    target_link_libraries(starfox_3ds_source_spans_tests PRIVATE starfox_3ds_source_geometry)
    add_test(NAME starfox_3ds_source_spans_tests COMMAND starfox_3ds_source_spans_tests)
    add_executable(starfox_source_polygon_spans_tests tests/source_polygon_spans_tests.cpp)
    target_link_libraries(starfox_source_polygon_spans_tests PRIVATE starfox_3ds_source_geometry)
    add_test(NAME starfox_source_polygon_spans_tests COMMAND starfox_source_polygon_spans_tests)
    add_library(starfox_3ds_raster STATIC platform/3ds/pica_raster.cpp platform/3ds/game_layers.cpp platform/3ds/game_scenery.cpp
        platform/3ds/bg2_tile_plan.cpp platform/3ds/bg2_rolled_tile_plan.cpp platform/3ds/pica_bg2_tiles.cpp)
    target_link_libraries(starfox_3ds_raster PUBLIC starfox_3ds_frontend starfox_core)
    add_executable(starfox_3ds_raster_tests tests/nintendo_3ds_raster_tests.cpp)
    target_link_libraries(starfox_3ds_raster_tests PRIVATE starfox_3ds_raster)
    add_test(NAME starfox_3ds_raster_tests COMMAND starfox_3ds_raster_tests)
    add_executable(starfox_3ds_game_layers_tests tests/nintendo_3ds_game_layers_tests.cpp)
    target_link_libraries(starfox_3ds_game_layers_tests PRIVATE starfox_3ds_raster starfox_3ds_game_session)
    add_test(NAME starfox_3ds_game_layers_tests COMMAND starfox_3ds_game_layers_tests)
    add_executable(starfox_3ds_source_models_check tools/check_3ds_source_models.cpp)
    target_link_libraries(starfox_3ds_source_models_check PRIVATE starfox_3ds_source_geometry)
    add_executable(starfox_3ds_hud_routing_tests tests/nintendo_3ds_hud_routing_tests.cpp
        src/render/sprite_renderer.cpp)
    target_include_directories(starfox_3ds_hud_routing_tests PRIVATE include)
    target_compile_features(starfox_3ds_hud_routing_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_3ds_hud_routing_tests COMMAND starfox_3ds_hud_routing_tests)
    add_library(starfox_3ds_game_hud STATIC platform/3ds/game_hud.cpp)
    target_link_libraries(starfox_3ds_game_hud PUBLIC starfox_3ds_frontend starfox_core)
    add_library(starfox_3ds_game_session STATIC platform/3ds/game_session.cpp platform/3ds/game_session_state.cpp
        platform/3ds/game_state.cpp platform/3ds/game_state_storage.cpp platform/3ds/game_menu.cpp platform/3ds/game_storage.cpp src/vr/game_scene.cpp)
    # presentation() uses the shared native panorama classifier in game_layers.
    # Export that archive dependency so session-only consumers link too.
    target_link_libraries(starfox_3ds_game_session PUBLIC starfox_3ds_game_hud starfox_3ds_raster)
    target_compile_definitions(starfox_3ds_game_session PRIVATE STARFOX_SCENE_SOURCE_ONLY=1)
    add_library(starfox_3ds_game_models STATIC platform/3ds/game_models.cpp platform/3ds/game_dots.cpp
        src/vr/source_pose_interpolation.cpp)
    target_link_libraries(starfox_3ds_game_models PUBLIC starfox_3ds_game_session starfox_3ds_source_geometry)
    # The host checker uses the same bounded companion reader/public checksum
    # as the native player. Generate its header here too; a stale header copied
    # from a separate native build must not be required by this target.
    include(platform/3ds/companion_manifest.cmake)
    add_executable(starfox_3ds_game_models_check tools/check_3ds_game_models.cpp
        platform/3ds/game_assets.cpp "${companion_header}")
    target_include_directories(starfox_3ds_game_models_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(starfox_3ds_game_models_check PRIVATE starfox_3ds_game_models starfox_3ds_raster)
    add_executable(starfox_3ds_game_session_check tools/check_3ds_game_session.cpp)
    target_link_libraries(starfox_3ds_game_session_check PRIVATE starfox_3ds_game_models starfox_3ds_raster)
    add_executable(starfox_3ds_parallel_game_session_check tools/check_3ds_game_session.cpp)
    target_compile_definitions(starfox_3ds_parallel_game_session_check PRIVATE STARFOX_3DS_CHECK_PARALLEL_AUDIO=1)
    target_include_directories(starfox_3ds_parallel_game_session_check PRIVATE platform/3ds)
    target_link_libraries(starfox_3ds_parallel_game_session_check PRIVATE
        starfox_3ds_game_models starfox_3ds_raster starfox_3ds_stem_test_runtime Threads::Threads)
    add_executable(starfox_3ds_native_model_check tools/check_3ds_native_model.cpp)
    target_link_libraries(starfox_3ds_native_model_check PRIVATE starfox_3ds_game_models)
    add_executable(starfox_3ds_game_menu_tests tests/nintendo_3ds_game_menu_tests.cpp)
    target_link_libraries(starfox_3ds_game_menu_tests PRIVATE starfox_3ds_game_session)
    add_test(NAME starfox_3ds_game_menu_tests COMMAND starfox_3ds_game_menu_tests)
    add_executable(starfox_3ds_game_dots_tests tests/nintendo_3ds_game_dots_tests.cpp)
    target_link_libraries(starfox_3ds_game_dots_tests PRIVATE starfox_3ds_game_models)
    add_test(NAME starfox_3ds_game_dots_tests COMMAND starfox_3ds_game_dots_tests)
    add_executable(starfox_3ds_game_storage_tests tests/nintendo_3ds_game_storage_tests.cpp)
    target_link_libraries(starfox_3ds_game_storage_tests PRIVATE starfox_3ds_game_session)
    add_test(NAME starfox_3ds_game_storage_tests COMMAND starfox_3ds_game_storage_tests)
    add_executable(starfox_3ds_game_state_tests tests/nintendo_3ds_game_state_tests.cpp)
    target_link_libraries(starfox_3ds_game_state_tests PRIVATE starfox_3ds_game_session)
    add_test(NAME starfox_3ds_game_state_tests COMMAND starfox_3ds_game_state_tests)
    add_executable(starfox_3ds_game_hud_check tools/check_3ds_game_hud.cpp)
    target_link_libraries(starfox_3ds_game_hud_check PRIVATE starfox_3ds_game_hud)
    add_test(NAME starfox_3ds_game_hud_status COMMAND starfox_3ds_game_hud_check)
    if(EXISTS "${STARFOX_ROM_FILE}" AND EXISTS "${STARFOX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_hud_original COMMAND starfox_3ds_game_hud_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_session_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_parallel_game_session_original COMMAND starfox_3ds_parallel_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_capabilities_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --capabilities-only)
        add_test(NAME starfox_3ds_game_stage_sweep_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --stage-sweep)
        set_tests_properties(starfox_3ds_game_stage_sweep_original PROPERTIES TIMEOUT 1800)
        add_test(NAME starfox_3ds_game_fortuna_source_route_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --fortuna-source-route)
        set_tests_properties(starfox_3ds_game_fortuna_source_route_original PROPERTIES TIMEOUT 1800)
        add_test(NAME starfox_3ds_game_corneria_source_route_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --corneria-source-route)
        set_tests_properties(starfox_3ds_game_corneria_source_route_original PROPERTIES TIMEOUT 1800)
        add_test(NAME starfox_3ds_game_attack_carrier_source_route_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --attack-carrier-source-route)
        set_tests_properties(starfox_3ds_game_attack_carrier_source_route_original PROPERTIES TIMEOUT 1800)
        add_test(NAME starfox_3ds_game_death_source_route_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --death-source-route)
        set_tests_properties(starfox_3ds_game_death_source_route_original PROPERTIES TIMEOUT 1800)
        foreach(continue_choice IN ITEMS yes no)
            add_test(NAME starfox_3ds_game_continue_${continue_choice}_source_route_original COMMAND starfox_3ds_game_session_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --continue-${continue_choice}-source-route)
            set_tests_properties(starfox_3ds_game_continue_${continue_choice}_source_route_original PROPERTIES TIMEOUT 1800)
        endforeach()
        add_test(NAME starfox_3ds_game_models_original COMMAND starfox_3ds_game_models_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_native_model_original COMMAND starfox_3ds_native_model_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_attack_carrier_composition_original COMMAND starfox_3ds_game_models_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" LEVEL1_1 36000 --attack-carrier-complete)
        add_test(NAME starfox_3ds_game_fortuna_composition_original COMMAND starfox_3ds_game_models_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" LEVEL3_3 36000 --fortuna-complete)
        # This offline oracle re-prepares every source phase at maximum optics
        # and checks slider/source pixels, not the player's realtime frame rate.
        # Keep its 36000 source-phase bound; allow the measured ~.22s/phase host
        # work to complete. This does not change any GPU completion deadline.
        set_tests_properties(starfox_3ds_game_attack_carrier_composition_original starfox_3ds_game_fortuna_composition_original PROPERTIES TIMEOUT 7200)
        add_test(NAME starfox_3ds_game_effects_original COMMAND starfox_3ds_game_models_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --native-effects-flow)
        add_test(NAME starfox_3ds_game_menu_composition_original COMMAND starfox_3ds_game_models_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --native-menu-composition)
        add_test(NAME starfox_3ds_game_landscape_original COMMAND starfox_3ds_game_models_check "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}" --native-landscape-flow)
        set_tests_properties(starfox_3ds_game_menu_composition_original starfox_3ds_game_landscape_original PROPERTIES TIMEOUT 600)
    endif()
    if(EXISTS "${STARFOX_EX_ROM_FILE}" AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_hud_ex COMMAND starfox_3ds_game_hud_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_session_ex COMMAND starfox_3ds_game_session_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_parallel_game_session_ex COMMAND starfox_3ds_parallel_game_session_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_capabilities_ex COMMAND starfox_3ds_game_session_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" --capabilities-only)
        add_test(NAME starfox_3ds_game_stage_sweep_ex COMMAND starfox_3ds_game_session_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" --stage-sweep)
        set_tests_properties(starfox_3ds_game_stage_sweep_ex PROPERTIES TIMEOUT 1800)
        add_test(NAME starfox_3ds_game_death_source_route_ex COMMAND starfox_3ds_game_session_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" --death-source-route)
        set_tests_properties(starfox_3ds_game_death_source_route_ex PROPERTIES TIMEOUT 1800)
        foreach(continue_choice IN ITEMS yes no)
            add_test(NAME starfox_3ds_game_continue_${continue_choice}_source_route_ex COMMAND starfox_3ds_game_session_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" --continue-${continue_choice}-source-route)
            set_tests_properties(starfox_3ds_game_continue_${continue_choice}_source_route_ex PROPERTIES TIMEOUT 1800)
        endforeach()
        add_test(NAME starfox_3ds_game_models_ex COMMAND starfox_3ds_game_models_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_native_model_ex COMMAND starfox_3ds_native_model_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_3ds_game_effects_ex COMMAND starfox_3ds_game_models_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" --native-effects-flow)
        add_test(NAME starfox_3ds_game_menu_composition_ex COMMAND starfox_3ds_game_models_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" --native-menu-composition)
        add_test(NAME starfox_3ds_game_landscape_ex COMMAND starfox_3ds_game_models_check "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" --native-landscape-flow)
        set_tests_properties(starfox_3ds_game_menu_composition_ex starfox_3ds_game_landscape_ex PROPERTIES TIMEOUT 600)
    endif()
    add_executable(starfox_bit_cast_compat_tests tests/bit_cast_compat_tests.cpp)
    target_include_directories(starfox_bit_cast_compat_tests PRIVATE include)
    target_compile_features(starfox_bit_cast_compat_tests PRIVATE cxx_std_20)
    target_compile_definitions(starfox_bit_cast_compat_tests
        PRIVATE STARFOX_FORCE_BIT_CAST_BUILTIN=1)
    add_test(NAME starfox_bit_cast_compat_tests COMMAND starfox_bit_cast_compat_tests)
    if(STARFOX_BUILD_VR AND NOT ANDROID)
        add_executable(starfox_desktop_paths_tests
            tests/desktop_paths_tests.cpp src/vr/desktop_paths.cpp)
        target_include_directories(starfox_desktop_paths_tests PRIVATE src/vr)
        target_compile_features(starfox_desktop_paths_tests PRIVATE cxx_std_20)
        add_test(NAME starfox_desktop_paths_tests COMMAND starfox_desktop_paths_tests)
        set_tests_properties(starfox_desktop_paths_tests PROPERTIES LABELS "vr;paths")
    endif()
    add_executable(starfox_irq_palette_tests tests/irq_palette_tests.cpp)
    target_include_directories(starfox_irq_palette_tests PRIVATE include)
    target_compile_features(starfox_irq_palette_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_irq_palette_tests COMMAND starfox_irq_palette_tests)
    add_executable(starfox_ps5_port_tests tests/ps5_port_tests.cpp)
    target_link_libraries(starfox_ps5_port_tests PRIVATE starfox_core)
    add_test(NAME starfox_ps5_port_tests COMMAND starfox_ps5_port_tests)
    # These seven macro configurations simulate target policies on a host.
    # Native/cross console compilers already define their target macros and
    # must not be asked to pretend to be another platform at the same time.
    if(NOT CMAKE_CROSSCOMPILING AND NOT WINDOWS_STORE AND NOT ANDROID
        AND NOT IOS AND NOT SWITCH AND NOT VITA AND NOT STARFOX_PS5)
        foreach(policy IN ITEMS desktop ios android switch uwp vita ps5)
            set(target starfox_render_scale_policy_${policy}_tests)
            add_executable(${target} tests/render_scale_policy_tests.cpp)
            target_include_directories(${target} PRIVATE include)
            target_compile_features(${target} PRIVATE cxx_std_20)
            if(policy STREQUAL desktop)
                target_compile_definitions(${target} PRIVATE STARFOX_TEST_SCALE_LIMIT=10)
            elseif(policy STREQUAL ios)
                target_compile_definitions(${target} PRIVATE STARFOX_IOS_RUNTIME=1 STARFOX_TEST_SCALE_LIMIT=2)
            else()
                target_compile_definitions(${target} PRIVATE STARFOX_TEST_SCALE_LIMIT=4)
                if(policy STREQUAL android)
                    target_compile_definitions(${target} PRIVATE __ANDROID__=1)
                elseif(policy STREQUAL switch)
                    target_compile_definitions(${target} PRIVATE __SWITCH__=1)
                elseif(policy STREQUAL uwp)
                    target_compile_definitions(${target} PRIVATE STARFOX_UWP=1)
                elseif(policy STREQUAL vita)
                    target_compile_definitions(${target} PRIVATE SDL_PLATFORM_VITA=1)
                elseif(policy STREQUAL ps5)
                    target_compile_definitions(${target} PRIVATE STARFOX_PS5=1)
                endif()
            endif()
            add_test(NAME ${target} COMMAND ${target})
        endforeach()
    endif()
    add_executable(starfox_comms_timing_check tools/check_comms_timing.cpp)
    target_link_libraries(starfox_comms_timing_check PRIVATE starfox_core)
    add_executable(starfox_bps_tests tests/bps_tests.cpp)
    target_link_libraries(starfox_bps_tests PRIVATE starfox_core)
    add_test(NAME starfox_bps_tests COMMAND starfox_bps_tests)
    add_executable(starfox_runtime_import_tests tests/runtime_import_tests.cpp)
    target_link_libraries(starfox_runtime_import_tests PRIVATE starfox_core)
    add_test(NAME starfox_runtime_import_tests COMMAND starfox_runtime_import_tests "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(starfox_pixel_filter_tests tests/pixel_filter_tests.cpp)
    add_executable(starfox_enhanced_terrain_tests tests/enhanced_terrain_tests.cpp)
    target_link_libraries(starfox_enhanced_terrain_tests PRIVATE starfox_core)
    add_test(NAME starfox_enhanced_terrain_tests COMMAND starfox_enhanced_terrain_tests)
    add_executable(starfox_backdrop_image_tests tests/backdrop_image_tests.cpp)
    target_include_directories(starfox_backdrop_image_tests PRIVATE include)
    target_compile_features(starfox_backdrop_image_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_backdrop_image_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/alpine-day-v2.bmp")
    add_test(NAME starfox_face_moon_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/face-moon-v1.bmp")
    add_test(NAME starfox_deep_space_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/deep-space-v1.bmp")
    add_test(NAME starfox_macbeth_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/macbeth-dusk-v1.bmp")
    add_test(NAME starfox_storm_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-night-v1.bmp")
    add_test(NAME starfox_desert_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/desert-horizon-v1.bmp")
    foreach(backdrop IN ITEMS orbital-clouds-v1 orbital-volcanic-v1 city-night-v1 nebula-red-blue-v1 cloud-plains-v1 gold-storm-v1 dune-horizon-v2 distant-snow-v1 titania-clouds-v1 ocean-clouds-v1 ocean-clouds-v2 night-coast-v1 red-cloud-band-v1 rocky-green-v1 dense-city-v1 asteroid-belt-v1 fine-debris-v1 ember-nebula-v2 lava-cavern-v1 comet-corona-v1 cratered-planet-v1 storm-planet-v1 banded-planet-v1 blue-cloud-v1 ember-sky-v1 jade-planet-horizon-v1 dimension-vortex-v1 amber-blue-cloud-v1 inferno-band-v1 spectral-clouds-v1)
        add_test(NAME starfox_${backdrop}_tests COMMAND starfox_backdrop_image_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/${backdrop}.bmp")
    endforeach()
    add_executable(starfox_frame_persistence_tests tests/frame_persistence_tests.cpp)
    add_test(NAME starfox_kazaru_volcanic_hills_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/kazaru-volcanic-hills-v1.bmp")
    target_link_libraries(starfox_frame_persistence_tests PRIVATE starfox_core)
    add_test(NAME starfox_frame_persistence_tests COMMAND starfox_frame_persistence_tests)
    target_link_libraries(starfox_pixel_filter_tests PRIVATE starfox_core)
    if(STARFOX_ENABLE_XBRZ)
        target_compile_definitions(
            starfox_pixel_filter_tests PRIVATE STARFOX_ENABLE_XBRZ)
    endif()
    add_test(
        NAME starfox_pixel_filter_tests COMMAND starfox_pixel_filter_tests)
    if(EXISTS "${STARFOX_RETAIL_V12_FILE}"
       AND EXISTS "${STARFOX_ORIGINAL_PATCH_FILE}"
       AND EXISTS "${STARFOX_ROM_FILE}"
       AND EXISTS "${STARFOX_EX_PATCH_FILE}"
       AND EXISTS "${STARFOX_EX_ROM_FILE}")
        add_test(
            NAME starfox_retail_v12_reconstruction
            COMMAND starfox_bps_tests
                "${STARFOX_RETAIL_V12_FILE}"
                "${STARFOX_ORIGINAL_PATCH_FILE}"
                "${STARFOX_ROM_FILE}"
                "${STARFOX_EX_PATCH_FILE}"
                "${STARFOX_EX_ROM_FILE}")
    endif()
    if(EXISTS "${STARFOX_RETAIL_V12_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V11_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_FILE}"
       AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
        add_test(
            NAME starfox_retail_variant_canonicalization
            COMMAND starfox_bps_tests --verify-canonicalizations
                "${STARFOX_RETAIL_V12_FILE}"
                "${STARFOX_RETAIL_JAPAN_V10_FILE}"
                "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_JAPAN_V11_FILE}"
                "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V10_FILE}"
                "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V11_FILE}"
                "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V10_FILE}"
                "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V11_FILE}"
                "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_GERMANY_V10_FILE}"
                "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
    endif()
    add_executable(starfox_core_tests tests/timing_tests.cpp)
    # Compile the unsupported backend independently, including on DXR hosts.
    # Do not link starfox_core, whose private STARFOX_DXR definition selects DXR.
    add_executable(starfox_dxr_stub_tests tools/check_dxr_stub.cpp src/render/dxr_shadows.cpp src/render/vulkan_hardware_rt.cpp)
    target_include_directories(starfox_dxr_stub_tests PRIVATE include)
    target_compile_features(starfox_dxr_stub_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_dxr_stub_tests COMMAND starfox_dxr_stub_tests)

    add_executable(starfox_state_tests tests/state_tests.cpp)
    add_executable(starfox_title_logo_check tests/title_logo_tests.cpp)
    target_link_libraries(starfox_title_logo_check PRIVATE starfox_core)
    add_executable(starfox_background_transition_check tools/check_background_transitions.cpp)
    target_link_libraries(starfox_background_transition_check PRIVATE starfox_core)
    target_link_libraries(starfox_state_tests PRIVATE starfox_core)
    add_test(NAME starfox_state_tests COMMAND starfox_state_tests)
    add_executable(starfox_shadow_geometry_tests tests/shadow_geometry_tests.cpp)
    add_executable(starfox_camera_response_tests tests/camera_response_tests.cpp)
    add_executable(starfox_responsive_preparation_tests tests/responsive_preparation_tests.cpp)
    add_executable(starfox_d3d12_descriptor_capacity_tests tests/d3d12_descriptor_capacity_tests.cpp)
    target_include_directories(starfox_d3d12_descriptor_capacity_tests PRIVATE include)
    target_compile_features(starfox_d3d12_descriptor_capacity_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_d3d12_descriptor_capacity_tests COMMAND starfox_d3d12_descriptor_capacity_tests)
    target_include_directories(starfox_responsive_preparation_tests PRIVATE include)
    target_compile_features(starfox_responsive_preparation_tests PRIVATE cxx_std_20)
    target_link_libraries(starfox_responsive_preparation_tests PRIVATE Threads::Threads)
    add_test(NAME starfox_responsive_preparation_tests COMMAND starfox_responsive_preparation_tests)
    add_executable(starfox_volumetric_fog_tests tests/volumetric_fog_tests.cpp)
    add_executable(starfox_motion_blur_tests tests/motion_blur_tests.cpp)
    add_executable(starfox_scene_world_fx_tests tests/scene_world_fx_tests.cpp)
    target_include_directories(starfox_scene_world_fx_tests PRIVATE include)
    target_compile_features(starfox_scene_world_fx_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_scene_world_fx_tests COMMAND starfox_scene_world_fx_tests)
    target_link_libraries(starfox_motion_blur_tests PRIVATE starfox_core)
    add_test(NAME starfox_motion_blur_tests COMMAND starfox_motion_blur_tests)
    target_link_libraries(starfox_volumetric_fog_tests PRIVATE starfox_core)
    add_test(NAME starfox_volumetric_fog_tests COMMAND starfox_volumetric_fog_tests)
    target_include_directories(starfox_camera_response_tests PRIVATE include)
    target_compile_features(starfox_camera_response_tests PRIVATE cxx_std_20)
    target_link_libraries(starfox_camera_response_tests PRIVATE starfox_core)
    add_test(NAME starfox_camera_response_tests COMMAND starfox_camera_response_tests)
    target_include_directories(starfox_shadow_geometry_tests PRIVATE include)
    target_compile_features(starfox_shadow_geometry_tests PRIVATE cxx_std_20)
    target_link_libraries(starfox_shadow_geometry_tests PRIVATE starfox_core)
    add_test(NAME starfox_shadow_geometry_tests COMMAND starfox_shadow_geometry_tests)
    add_executable(starfox_software_reflection_tests tests/software_reflection_tests.cpp)
    target_link_libraries(starfox_software_reflection_tests PRIVATE starfox_core)
    add_test(NAME starfox_software_reflection_tests COMMAND starfox_software_reflection_tests)
    add_executable(starfox_raster_commands_tests tools/benchmark_gpu_raster.cpp)
    add_executable(starfox_geometry_fp64_tests tests/geometry_fp64_tests.cpp)
    target_compile_features(starfox_geometry_fp64_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_geometry_fp64_tests COMMAND starfox_geometry_fp64_tests)
    foreach(curved_table IN ITEMS 0 1)
        set(curved_scalar_target starfox_reflected_curved_scalar_${curved_table}_tests)
        add_executable(${curved_scalar_target} tests/reflected_curved_scalar_tests.cpp)
        target_compile_features(${curved_scalar_target} PRIVATE cxx_std_20)
        target_compile_definitions(${curved_scalar_target} PRIVATE STARFOX_CURVED_TRIG_TABLE=${curved_table})
        if(MSVC)
            target_compile_options(${curved_scalar_target} PRIVATE /fp:strict)
        else()
            target_compile_options(${curved_scalar_target} PRIVATE -fno-fast-math -ffp-contract=off)
        endif()
        add_test(NAME ${curved_scalar_target} COMMAND ${curved_scalar_target})
    endforeach()
    add_executable(starfox_face_material_tests tests/face_material_tests.cpp)
    add_executable(starfox_packed_bsp_tests tests/packed_bsp_tests.cpp)
    add_executable(starfox_packed_projection_tests tests/packed_projection_tests.cpp)
    add_executable(starfox_temporal_projection_tests tests/temporal_projection_tests.cpp)
    target_include_directories(starfox_temporal_projection_tests PRIVATE include)
    target_compile_features(starfox_temporal_projection_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_temporal_projection_tests COMMAND starfox_temporal_projection_tests)
    add_executable(starfox_packed_faces_tests tests/packed_faces_tests.cpp)
    target_link_libraries(starfox_packed_faces_tests PRIVATE starfox_core)
    add_test(NAME starfox_packed_faces_tests COMMAND starfox_packed_faces_tests)
    target_link_libraries(starfox_packed_projection_tests PRIVATE starfox_core)
    add_test(NAME starfox_packed_projection_tests COMMAND starfox_packed_projection_tests)
    target_link_libraries(starfox_packed_bsp_tests PRIVATE starfox_core)
    add_test(NAME starfox_packed_bsp_tests COMMAND starfox_packed_bsp_tests)
    target_link_libraries(starfox_face_material_tests PRIVATE starfox_core)
    add_test(NAME starfox_face_material_tests COMMAND starfox_face_material_tests)
    target_compile_definitions(starfox_raster_commands_tests PRIVATE STARFOX_TEST_RASTER_CPU)
    target_link_libraries(starfox_raster_commands_tests PRIVATE starfox_core)
    add_test(NAME starfox_raster_commands_tests COMMAND starfox_raster_commands_tests)
    target_link_libraries(starfox_core_tests PRIVATE starfox_core)
    add_test(NAME starfox_core_tests COMMAND starfox_core_tests)

    add_executable(starfox_rumble_sequencer_tests tests/rumble_sequencer_tests.cpp)
    target_link_libraries(starfox_rumble_sequencer_tests PRIVATE starfox_core)
    add_test(NAME starfox_rumble_sequencer_tests COMMAND starfox_rumble_sequencer_tests)

    add_executable(starfox_msu1_pack_tests tests/msu1_pack_tests.cpp)
    target_link_libraries(starfox_msu1_pack_tests PRIVATE starfox_core)
    add_test(NAME starfox_msu1_pack_tests COMMAND starfox_msu1_pack_tests)

    add_executable(starfox_msu1_audio_tests tests/msu1_audio_tests.cpp)
    target_link_libraries(starfox_msu1_audio_tests PRIVATE starfox_core)
    add_test(NAME starfox_msu1_audio_tests COMMAND starfox_msu1_audio_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/msu-tone-320.flac")

    add_executable(starfox_shape_decoder_tests tests/shape_decoder_tests.cpp)
    target_link_libraries(starfox_shape_decoder_tests PRIVATE starfox_core)
    add_test(NAME starfox_shape_decoder_tests COMMAND starfox_shape_decoder_tests)

    add_executable(starfox_simulation_tests tests/simulation_tests.cpp)
    target_link_libraries(starfox_simulation_tests PRIVATE starfox_core)
    # The cartridge-wide integration fixture intentionally keeps several full
    # GameSimulation snapshots alive in nested parity sections. PE/COFF's
    # small default stack can overflow after earlier sections begin reaching
    # their now-correct transition endpoints, masking the actual assertions.
    if(MINGW)
        target_link_options(
            starfox_simulation_tests PRIVATE "-Wl,--stack,16777216")
    elseif(MSVC)
        target_link_options(starfox_simulation_tests PRIVATE "/STACK:16777216")
    endif()
    add_test(NAME starfox_simulation_tests COMMAND starfox_simulation_tests)

    add_executable(starfox_object_record_tests tests/object_record_tests.cpp)
    target_link_libraries(starfox_object_record_tests PRIVATE starfox_core)
    add_test(NAME starfox_object_record_tests COMMAND starfox_object_record_tests)
    add_executable(starfox_bus_transfer_tests tests/bus_transfer_tests.cpp)
    target_link_libraries(starfox_bus_transfer_tests PRIVATE starfox_core retro_cpu_core)
    add_test(NAME starfox_bus_transfer_tests COMMAND starfox_bus_transfer_tests)

    add_executable(starfox_tunnel_scanline_tests tests/tunnel_scanline_tests.cpp)
    target_link_libraries(starfox_tunnel_scanline_tests PRIVATE starfox_core)
    add_test(NAME starfox_tunnel_scanline_tests COMMAND starfox_tunnel_scanline_tests)

    add_executable(starfox_direct_audio_tests tests/direct_audio_tests.cpp)
    target_link_libraries(starfox_direct_audio_tests PRIVATE starfox_core)

    add_executable(starfox_transition_parity_tests
        tests/transition_parity_tests.cpp)
    target_link_libraries(starfox_transition_parity_tests PRIVATE starfox_core)

    add_executable(starfox_ending_tests tests/ending_tests.cpp)
    target_link_libraries(starfox_ending_tests PRIVATE starfox_core)
    add_executable(starfox_intro_pacing_tests tests/intro_pacing_tests.cpp)
    add_executable(starfox_geometry_pacing_tests tests/geometry_pacing_tests.cpp)
    target_link_libraries(starfox_geometry_pacing_tests PRIVATE starfox_core)
    target_link_libraries(starfox_intro_pacing_tests PRIVATE starfox_core)
    if(MINGW)
        target_link_options(starfox_intro_pacing_tests PRIVATE "-Wl,--stack,16777216")
    elseif(MSVC)
        target_link_options(starfox_intro_pacing_tests PRIVATE "/STACK:16777216")
    endif()

    add_executable(starfox_asteroid_models_tests tests/asteroid_models_tests.cpp)
    target_link_libraries(starfox_asteroid_models_tests PRIVATE starfox_core)
    add_test(NAME starfox_asteroid_models_tests COMMAND starfox_asteroid_models_tests)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_asteroid_models
            COMMAND starfox_asteroid_models_tests
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
    endif()
    add_executable(starfox_stereo_output_tests tests/stereo_output_tests.cpp)
    add_executable(starfox_temporal_aa_tests tests/temporal_aa_tests.cpp)
    add_executable(starfox_msaa_tests tests/msaa_tests.cpp)
    target_link_libraries(starfox_msaa_tests PRIVATE starfox_core)
    add_test(NAME starfox_msaa_tests COMMAND starfox_msaa_tests)
    target_link_libraries(starfox_temporal_aa_tests PRIVATE starfox_core)
    add_test(NAME starfox_temporal_aa_tests COMMAND starfox_temporal_aa_tests)
    target_link_libraries(starfox_stereo_output_tests PRIVATE starfox_core)
    add_test(NAME starfox_stereo_output_tests COMMAND starfox_stereo_output_tests)
    add_executable(starfox_hitlist_tests tests/hitlist_tests.cpp)
    add_executable(starfox_level_clear_tests tests/level_clear_tests.cpp)
    target_link_libraries(starfox_level_clear_tests PRIVATE starfox_core)
    target_link_libraries(starfox_hitlist_tests PRIVATE starfox_core)

    if(STARFOX_BUILD_RUNTIME)
        add_executable(starfox_model_host_clock_tests tests/model_host_clock_tests.cpp)
        target_compile_features(starfox_model_host_clock_tests PRIVATE cxx_std_20)
        target_link_libraries(starfox_model_host_clock_tests PRIVATE Threads::Threads)
        add_test(NAME starfox_model_host_clock_tests COMMAND starfox_model_host_clock_tests)
        add_executable(starfox_gpu_preparation_tests tests/gpu_preparation_tests.cpp)
        target_include_directories(starfox_gpu_preparation_tests PRIVATE include)
        target_compile_features(starfox_gpu_preparation_tests PRIVATE cxx_std_20)
        target_link_libraries(starfox_gpu_preparation_tests PRIVATE SDL3::SDL3 Threads::Threads)
        add_test(NAME starfox_gpu_preparation_tests COMMAND starfox_gpu_preparation_tests)
        add_executable(starfox_reflection_source_stage_timing_tests tests/reflection_source_stage_timing_tests.cpp)
        target_include_directories(starfox_reflection_source_stage_timing_tests PRIVATE include)
        target_compile_features(starfox_reflection_source_stage_timing_tests PRIVATE cxx_std_20)
        target_link_libraries(starfox_reflection_source_stage_timing_tests PRIVATE SDL3::SDL3)
        add_test(NAME starfox_reflection_source_stage_timing_tests COMMAND starfox_reflection_source_stage_timing_tests)
        add_executable(starfox_reflection_source_proof_receipt_tests tests/reflection_source_proof_receipt_tests.cpp)
        target_include_directories(starfox_reflection_source_proof_receipt_tests PRIVATE include)
        target_compile_features(starfox_reflection_source_proof_receipt_tests PRIVATE cxx_std_20)
        target_link_libraries(starfox_reflection_source_proof_receipt_tests PRIVATE SDL3::SDL3)
        add_test(NAME starfox_reflection_source_proof_receipt_tests COMMAND starfox_reflection_source_proof_receipt_tests)
        if(WIN32 AND NOT WINDOWS_STORE)
            # Requires a visible Windows desktop; run explicitly, not in headless CTest.
            add_executable(starfox_renderer_switch_tests tests/renderer_switch_tests.cpp)
            target_link_libraries(starfox_renderer_switch_tests PRIVATE SDL3::SDL3)
        endif()
        add_executable(starfox_runtime_input_tests
            tests/runtime_input_tests.cpp
            src/app/runtime_input.cpp)
        target_link_libraries(
            starfox_runtime_input_tests PRIVATE starfox_core SDL3::SDL3)
        add_test(NAME starfox_runtime_input_tests COMMAND starfox_runtime_input_tests)
        if(NOT WINDOWS_STORE AND NOT ANDROID AND NOT SWITCH AND NOT VITA AND NOT IOS)
            add_executable(starfox_controller_check tools/check_controller_route.cpp src/app/runtime_input.cpp)
            target_link_libraries(starfox_controller_check PRIVATE starfox_core SDL3::SDL3)
        endif()
        add_executable(starfox_audio_queue_tests tests/audio_queue_tests.cpp)
        target_link_libraries(starfox_audio_queue_tests PRIVATE starfox_core SDL3::SDL3)
        add_test(NAME starfox_audio_queue_tests COMMAND starfox_audio_queue_tests)
        set_tests_properties(starfox_audio_queue_tests PROPERTIES
            ENVIRONMENT "SDL_AUDIODRIVER=dummy")
        set_tests_properties(starfox_runtime_input_tests PROPERTIES
            ENVIRONMENT "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1"
            TIMEOUT 10)
        if(WIN32 AND NOT WINDOWS_STORE)
            # Exercise UWP's pre-initialization configuration on the host as
            # well; ordinary desktop tests can pass via XInput without WGI.
            add_executable(starfox_uwp_input_tests
                tests/runtime_input_tests.cpp src/app/runtime_input.cpp)
            target_compile_definitions(starfox_uwp_input_tests PRIVATE STARFOX_UWP=1)
            target_link_libraries(starfox_uwp_input_tests PRIVATE starfox_core SDL3::SDL3)
            add_test(NAME starfox_uwp_input_tests COMMAND starfox_uwp_input_tests)
            set_tests_properties(starfox_uwp_input_tests PROPERTIES
                ENVIRONMENT "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1" TIMEOUT 10)
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(
            NAME starfox_upstream_shape_coverage
            COMMAND starfox_shape_coverage
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(
            NAME starfox_upstream_simulation_data
            COMMAND starfox_simulation_tests
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(NAME starfox_upstream_state_data COMMAND starfox_state_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_comms_timing COMMAND starfox_comms_timing_check
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(
            NAME starfox_upstream_direct_audio
            COMMAND starfox_direct_audio_tests
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(
            NAME starfox_upstream_transition_parity
            COMMAND starfox_transition_parity_tests
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(NAME starfox_upstream_ending COMMAND starfox_ending_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_intro_pacing COMMAND starfox_intro_pacing_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_geometry_pacing COMMAND starfox_geometry_pacing_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_hitlist COMMAND starfox_hitlist_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_level_clear COMMAND starfox_level_clear_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(
            NAME starfox_upstream_map_trace
            COMMAND starfox_map_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    MAP1_1A
        )
        add_test(
            NAME starfox_upstream_strategy_trace
            COMMAND starfox_strategy_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    MAP1_1A
                    1
        )
        add_test(
            NAME starfox_upstream_game_init
            COMMAND starfox_game_init_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    20
        )
        add_test(
            NAME starfox_upstream_stage_trace
            COMMAND starfox_stage_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    LEVEL1_1
                    100
        )
        add_test(
            NAME starfox_upstream_planet_probe
            COMMAND starfox_planet_probe
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(
            NAME starfox_upstream_route_lifecycle
            COMMAND starfox_stage_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    LEVEL1_3
                    500
        )
        if(STARFOX_BUILD_RUNTIME)
            add_test(
                NAME starfox_runtime_smoke
                COMMAND starfox_pc
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                        LEVEL1_1
            )
            set_tests_properties(starfox_runtime_smoke PROPERTIES
                ENVIRONMENT
                    "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_FRAMES=6;STARFOX_TEST_RENDER_SCALE=1"
                TIMEOUT 10
            )
            add_test(
                NAME starfox_exit_confirmation_smoke
                COMMAND starfox_pc
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                        BOOT
            )
            set_tests_properties(starfox_exit_confirmation_smoke PROPERTIES
                ENVIRONMENT
                    "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_EXIT_CONFIRMATION=1;STARFOX_TEST_FRAMES=1000000;STARFOX_TEST_PRESSES=1:0x0200,5:0x0080;STARFOX_TEST_UNPACED=1;STARFOX_TEST_SKIP_PREROLL=1;STARFOX_TEST_RENDER_SCALE=10"
                # The scripted LEFT, then A must select YES and terminate long
                # before the large frame sentinel. This also exercises the
                # confirmation compositor at the maximum upscale setting.
                TIMEOUT 10
            )
            if(STARFOX_RUNTIME_ASSETS_EMBEDDED)
                # Embedded resources are patches, not a cartridge. A clean
                # first launch requires a user-supplied retail ROM; otherwise
                # this headless fixture waits forever in the asset picker.
                if(EXISTS "${STARFOX_RETAIL_V12_FILE}")
                    file(MAKE_DIRECTORY
                        "${CMAKE_CURRENT_BINARY_DIR}/embedded-runtime-smoke")
                    add_test(
                        NAME starfox_embedded_runtime_smoke
                        COMMAND "$<TARGET_FILE:starfox_pc>"
                    )
                    set_tests_properties(starfox_embedded_runtime_smoke PROPERTIES
                        WORKING_DIRECTORY
                        "${CMAKE_CURRENT_BINARY_DIR}/embedded-runtime-smoke"
                        ENVIRONMENT
                        "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_RETAIL_ROM=${STARFOX_RETAIL_V12_FILE};STARFOX_TEST_FRAMES=6;STARFOX_TEST_RENDER_SCALE=1"
                        TIMEOUT 10
                    )
                endif()
                if(EXISTS "${STARFOX_EX_ROM_FILE}"
                   AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}")
                    add_test(
                        NAME starfox_embedded_asset_lifecycle
                        COMMAND "${CMAKE_COMMAND}"
                            "-DRUNTIME=$<TARGET_FILE:starfox_pc>"
                            "-DVERIFIER=$<TARGET_FILE:starfox_bps_tests>"
                            "-DRETAIL=${STARFOX_RETAIL_V12_FILE}"
                            "-DRETAIL_JAPAN_V10=${STARFOX_RETAIL_JAPAN_V10_FILE}"
                            "-DRETAIL_JAPAN_V11=${STARFOX_RETAIL_JAPAN_V11_FILE}"
                            "-DRETAIL_USA_V10=${STARFOX_RETAIL_USA_V10_FILE}"
                            "-DRETAIL_USA_V11=${STARFOX_RETAIL_USA_V11_FILE}"
                            "-DRETAIL_EUROPE_V10=${STARFOX_RETAIL_EUROPE_V10_FILE}"
                            "-DRETAIL_EUROPE_V11=${STARFOX_RETAIL_EUROPE_V11_FILE}"
                            "-DRETAIL_GERMANY_V10=${STARFOX_RETAIL_GERMANY_V10_FILE}"
                            "-DORIGINAL_ROM=${STARFOX_ROM_FILE}"
                            "-DORIGINAL_SYMBOLS=${STARFOX_SYMBOLS_FILE}"
                            "-DEX_ROM=${STARFOX_EX_ROM_FILE}"
                            "-DEX_SYMBOLS=${STARFOX_EX_SYMBOLS_FILE}"
                            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/asset-lifecycle"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_asset_lifecycle.cmake"
                    )
                    set_tests_properties(
                        starfox_embedded_asset_lifecycle PROPERTIES TIMEOUT 90)
                endif()
            endif()
        endif()
    endif()

    if(EXISTS "${STARFOX_EX_ROM_FILE}"
       AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(
            NAME starfox_ex_direct_audio
            COMMAND starfox_direct_audio_tests
                    "${STARFOX_EX_ROM_FILE}"
                    "${STARFOX_EX_SYMBOLS_FILE}"
        )
        add_test(
            NAME starfox_ex_transition_parity
            COMMAND starfox_transition_parity_tests
                    "${STARFOX_EX_ROM_FILE}"
                    "${STARFOX_EX_SYMBOLS_FILE}"
        )
        add_test(NAME starfox_ex_ending COMMAND starfox_ending_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_intro_pacing COMMAND starfox_intro_pacing_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_geometry_pacing COMMAND starfox_geometry_pacing_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_hitlist COMMAND starfox_hitlist_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_level_clear COMMAND starfox_level_clear_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(
            NAME starfox_ex_simulation_data
            COMMAND starfox_simulation_tests
                    "${STARFOX_EX_ROM_FILE}"
                    "${STARFOX_EX_SYMBOLS_FILE}"
        )
        add_test(NAME starfox_ex_state_data COMMAND starfox_state_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_comms_timing COMMAND starfox_comms_timing_check
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        set_tests_properties(starfox_ex_simulation_data PROPERTIES
            ENVIRONMENT "STARFOX_EX_ROUTE_AUDIT_TICKS=2000"
            # The audit boots every shipped EX stage and advances each for
            # 2,000 source ticks. Renderer/route regressions have expanded
            # its coverage enough that Windows debug-host load can exceed
            # four minutes without a hang; retain the full audit and give it
            # a deterministic margin instead of shortening stage coverage.
            TIMEOUT 420)
        if(STARFOX_BUILD_RUNTIME AND STARFOX_RUNTIME_ASSETS_EMBEDDED)
            file(MAKE_DIRECTORY
                "${CMAKE_CURRENT_BINARY_DIR}/embedded-ex-runtime-smoke")
            add_test(
                NAME starfox_ex_intro_runtime_smoke
                COMMAND "$<TARGET_FILE:starfox_pc>"
            )
            set_tests_properties(starfox_ex_intro_runtime_smoke PROPERTIES
                WORKING_DIRECTORY
                    "${CMAKE_CURRENT_BINARY_DIR}/embedded-ex-runtime-smoke"
                ENVIRONMENT
                    "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_EXPERIENCE=EX;STARFOX_TEST_FRAMES=900;STARFOX_TEST_PRESSES=6:0x1000;STARFOX_TEST_UNPACED=1;STARFOX_TEST_RENDER_SCALE=1"
                # The embedded EX boot renders 900 full frames. Sanitizer and
                # busy Windows hosts can exceed 20 seconds without a hang.
                TIMEOUT 45
            )
        endif()
    endif()

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME starfox_metal_source_assembly
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_metal_shader_sources.py")
        add_test(NAME starfox_vulkan_material_colour_adapter
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_vulkan_material_colour.py")
        add_test(NAME starfox_dlss_preview_artwork_checker
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dlss_display_preview.py")
        set_tests_properties(starfox_dlss_preview_artwork_checker PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME starfox_steam_frame_cmake_guards
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_steam_frame_cmake_guards.py"
                --cmake "${CMAKE_COMMAND}"
                --generator "${CMAKE_GENERATOR}")
        # The Linux package fixture checks POSIX executable permission bits.
        # Windows cannot represent those bits for its extensionless fixtures.
        if(UNIX)
            add_test(NAME starfox_steam_frame_package_validation
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_steam_frame_package_validation.py")
        endif()
        add_test(NAME starfox_backdrop_catalog
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_backdrop_catalog.py")
        add_test(NAME starfox_enhanced_palette_trace
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_enhanced_palette_trace.py")
        add_test(NAME starfox_backdrop_region_inks
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_backdrop_region_inks.py")
        add_test(NAME starfox_android_package_oracle
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_android_package.py")
        add_test(NAME starfox_ps5_sdl_patch
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ps5_sdl_patch.py")
        add_test(NAME starfox_quest_package_oracle
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_quest_package.py")
        if(UNIX AND NOT CMAKE_CROSSCOMPILING)
            add_test(NAME starfox_embedded_assets_roundtrip
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_embed_runtime_assets.py"
                    --compiler "${CMAKE_CXX_COMPILER}")
        endif()
        add_test(NAME starfox_asteroid_models_generated
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_asteroid_models.py" --check)
        add_test(NAME starfox_scramble_sequence_oracle
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scramble_sequence.py")
        if(STARFOX_BUILD_RUNTIME AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC")
            add_test(NAME starfox_isolated_effects
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_isolated_effects.py"
                    "$<TARGET_FILE:starfox_pc>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
            set_tests_properties(starfox_isolated_effects PROPERTIES TIMEOUT 240)
        endif()
        add_test(
            NAME starfox_dialogue_catalog_tests
            COMMAND "${Python3_EXECUTABLE}" -m unittest discover
                    -s "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                    -p "test_dialogue_catalog.py"
        )
        set_tests_properties(starfox_dialogue_catalog_tests PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}"
        )
        add_test(
            NAME starfox_shape_audit_tests
            COMMAND "${Python3_EXECUTABLE}" -m unittest discover
                    -s "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                    -p "test_shape_audit.py"
        )
        set_tests_properties(starfox_shape_audit_tests PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()
