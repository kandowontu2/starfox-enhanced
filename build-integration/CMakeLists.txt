cmake_minimum_required(VERSION 3.22)
project(starfox_metal_source_build LANGUAGES C CXX OBJC OBJCXX ASM)
if(NOT APPLE OR NOT DEFINED STARFOX_BUILD_SDK OR NOT DEFINED STARFOX_SDL_ARCHIVE)
    message(FATAL_ERROR "Actual Apple SDK and pinned real SDL archive required")
endif()
set(SDL_SHARED OFF CACHE BOOL "Use actual statically linked SDL" FORCE)
set(SDL_STATIC ON CACHE BOOL "Use actual statically linked SDL" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "No unrelated SDL test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "No unrelated SDL tests" FORCE)
include(FetchContent)
FetchContent_Declare(SDL3 URL "${STARFOX_SDL_ARCHIVE}"
    URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)
get_filename_component(source_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include("${source_root}/cmake/BuildMetalReflection.cmake")
starfox_add_source_built_metal_reflection_history(starfox_metal_reflection_history
    "${source_root}" "${STARFOX_BUILD_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/generated-metal")
add_executable(starfox_metal_factory_link_check "${source_root}/tools/check_link.cpp")
target_compile_features(starfox_metal_factory_link_check PRIVATE cxx_std_20)
target_compile_options(starfox_metal_factory_link_check PRIVATE
    -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wconversion -Werror)
target_include_directories(starfox_metal_factory_link_check PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated-metal/bundle")
target_compile_definitions(starfox_metal_factory_link_check PRIVATE SDL_MAIN_HANDLED=1)
target_link_libraries(starfox_metal_factory_link_check PRIVATE
    starfox_metal_reflection_history SDL3::SDL3)
