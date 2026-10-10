cmake_minimum_required(VERSION 3.22)
project(starfox_metal_factory_link LANGUAGES C CXX OBJC OBJCXX ASM)
if(NOT APPLE OR NOT DEFINED STARFOX_EMBEDDING_DIRECTORY OR
   NOT DEFINED STARFOX_SDL_ARCHIVE)
    message(FATAL_ERROR "Exact Apple SDK, generated complete25 embedding and pinned SDL archive required")
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
include(cmake/SDLMetalExactWorkgroup.cmake)
starfox_patch_metal_exact_workgroup("${sdl3_SOURCE_DIR}")
include(cmake/EmbeddedMetalReflection.cmake)
starfox_add_embedded_metal_reflection_history(starfox_metal_reflection_history
    "${STARFOX_EMBEDDING_DIRECTORY}")
add_executable(starfox_metal_factory_link_check tools/check_link.cpp)
target_compile_features(starfox_metal_factory_link_check PRIVATE cxx_std_20)
target_compile_options(starfox_metal_factory_link_check PRIVATE
    -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wconversion -Werror)
target_include_directories(starfox_metal_factory_link_check PRIVATE
    "${STARFOX_EMBEDDING_DIRECTORY}")
target_compile_definitions(starfox_metal_factory_link_check PRIVATE SDL_MAIN_HANDLED=1)
target_link_libraries(starfox_metal_factory_link_check PRIVATE
    starfox_metal_reflection_history SDL3::SDL3)
add_executable(starfox_metal_pipeline_metadata_check tools/check_metal_pipeline_metadata.cpp)
target_include_directories(starfox_metal_pipeline_metadata_check PRIVATE include)
target_compile_features(starfox_metal_pipeline_metadata_check PRIVATE cxx_std_20)
target_compile_options(starfox_metal_pipeline_metadata_check PRIVATE
    -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -Wconversion -Werror)
target_compile_definitions(starfox_metal_pipeline_metadata_check PRIVATE SDL_MAIN_HANDLED=1)
target_link_libraries(starfox_metal_pipeline_metadata_check PRIVATE SDL3::SDL3)
# This native link consumer creates no SDL/Metal device or window. iOS is
# compiled/linked only; no simulator output is mislabeled as physical evidence.
