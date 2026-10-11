# Attach the five actual graphics factories exactly once. Compilation/linking
# does not install this renderer into the ordinary Apple gameplay path.
function(starfox_add_embedded_metal_composite target embedding_directory)
    if(NOT APPLE OR NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR "An actual Apple SDK and real SDL GPU implementation are required")
    endif()
    foreach(input IN ITEMS libraries.S embedding_programs.hpp)
        if(NOT EXISTS "${embedding_directory}/${input}")
            message(FATAL_ERROR "Complete graphics bundle input missing: ${input}")
        endif()
    endforeach()
    get_filename_component(source_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    # Public API types used by the original calibrated camera; never replace
    # them with local ABI stubs. This is the application's pinned header SDK.
    include(FetchContent)
    FetchContent_Declare(starfox_displayxr_headers
        URL https://codeload.github.com/KhronosGroup/OpenXR-SDK/tar.gz/f2448a8797c85814aa892efc1ab8707900fbcc78
        URL_HASH SHA256=1681e0f6c9633c73117c2fc376d1fe71cbd6fdd2aeb896ed5854ff022aa82eaa
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _starfox_headers_only)
    FetchContent_MakeAvailable(starfox_displayxr_headers)
    add_library(${target} STATIC
        "${source_root}/src/render/gpu_calibrated_ray_composite.cpp"
        "${source_root}/src/render/embedded_metal_composite_programs.cpp"
        "${embedding_directory}/libraries.S")
    target_include_directories(${target} PUBLIC "${source_root}/include"
        PRIVATE "${embedding_directory}")
    target_include_directories(${target} SYSTEM PUBLIC "${starfox_displayxr_headers_SOURCE_DIR}/include")
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PRIVATE STARFOX_CALIBRATED_COMPOSITE_METAL=1)
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-O3;-fno-fast-math;-ffp-contract=off;-Wall;-Wextra;-Wconversion;-Werror>")
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
endfunction()
