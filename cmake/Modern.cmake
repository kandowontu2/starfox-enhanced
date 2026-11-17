# MODERN renderer: the shared VR scene (starfox_scene) drawn through SDL_GPU
# for flat-screen targets. Needs SDL3::SDL3 from the runtime build. Desktop
# Windows/Linux, Android and PS5 (all present through SDL_GPU with DXIL or
# SPIR-V); other targets keep their existing renderers.
if((WIN32 AND NOT WINDOWS_STORE) OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID OR STARFOX_PS5)
    set(STARFOX_MODERN_DEFAULT ON)
else()
    set(STARFOX_MODERN_DEFAULT OFF)
endif()
option(STARFOX_ENABLE_MODERN "Build the MODERN renderer (VR scene code on SDL_GPU)" ${STARFOX_MODERN_DEFAULT})
if(NOT STARFOX_ENABLE_MODERN)
    return()
endif()
if(NOT TARGET starfox_scene)
    include("${CMAKE_CURRENT_LIST_DIR}/Scene.cmake")
endif()
add_library(starfox_modern STATIC src/modern/sdl_scene_packets.cpp src/modern/sdl_layer_filter.cpp src/modern/sdl_source_scene.cpp src/modern/sdl_frame.cpp src/modern/sdl_ray_shadows.cpp src/modern/modern_runtime.cpp
    src/modern/texture_dump.cpp)
target_include_directories(starfox_modern PUBLIC include PRIVATE src)
target_compile_features(starfox_modern PUBLIC cxx_std_20)
target_link_libraries(starfox_modern PUBLIC starfox_scene SDL3::SDL3)
if(NOT TARGET starfox_texture_files)
    # Replacement artwork files (MODERN's <exe>/textures/), for the flat
    # renderer and the PCVR app.
    add_library(starfox_texture_files STATIC src/render/texture_override_files.cpp)
    target_include_directories(starfox_texture_files PUBLIC include)
    target_compile_features(starfox_texture_files PUBLIC cxx_std_20)
    target_link_libraries(starfox_texture_files PUBLIC starfox_core PRIVATE SDL3::SDL3)
endif()
target_link_libraries(starfox_modern PUBLIC starfox_texture_files)
if(STARFOX_BUILD_TESTS)
    add_executable(starfox_modern_safety_tests tests/modern_safety_tests.cpp)
    target_include_directories(starfox_modern_safety_tests PRIVATE include)
    target_compile_features(starfox_modern_safety_tests PRIVATE cxx_std_20)
    target_link_libraries(starfox_modern_safety_tests PRIVATE SDL3::SDL3)
    add_test(NAME starfox_modern_safety_tests COMMAND starfox_modern_safety_tests)
endif()
if(STARFOX_BUILD_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME starfox_modern_shader_freshness
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/generate_modern_shaders.py" --check)
    set_tests_properties(starfox_modern_shader_freshness PROPERTIES LABELS modern TIMEOUT 30)
endif()
if(TARGET starfox_vr_game)
    # M1 harness (fork-local): SDL_GPU packets vs starfox_vr_scene_route_bench captures.
    add_executable(starfox_modern_scene_check EXCLUDE_FROM_ALL tools/check_modern_scene.cpp)
    target_link_libraries(starfox_modern_scene_check PRIVATE starfox_modern starfox_vr_game SDL3::SDL3)
    # PCVR MODERN without a headset: one snapshot through the VR Vulkan path
    # and the flat renderer's headset preview, with a difference report.
    add_executable(starfox_vr_modern_capture EXCLUDE_FROM_ALL tools/check_vr_modern_capture.cpp)
    target_link_libraries(starfox_vr_modern_capture PRIVATE starfox_modern starfox_vr_game SDL3::SDL3)
endif()
