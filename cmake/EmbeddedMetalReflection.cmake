# Complete SDK-specific shader family; no native-byte aliases or partial graph.
# This component is reusable by an Apple application target. The standalone
# source-only CI consumer proves linking, not game integration or GPU execution.
function(starfox_add_embedded_metal_reflection_history target embedding_directory)
    if(NOT APPLE)
        message(FATAL_ERROR "Embedded Metal reflection history requires an Apple SDK")
    endif()
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR "The original SDL3 GPU implementation is required")
    endif()
    foreach(embedding_file IN ITEMS libraries.S embedding_programs.hpp)
        if(NOT EXISTS "${embedding_directory}/${embedding_file}")
            message(FATAL_ERROR "Complete SDK embedding input is missing: ${embedding_file}")
        endif()
    endforeach()
    get_filename_component(source_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    add_library(${target} STATIC
        "${source_root}/src/render/gpu_calibrated_reflection_history.cpp"
        "${source_root}/src/render/embedded_metal_reflection_programs.cpp"
        "${embedding_directory}/libraries.S")
    target_include_directories(${target} PUBLIC "${source_root}/include"
        PRIVATE "${embedding_directory}")
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PRIVATE
        STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1
        STARFOX_REFLECTION_EMBEDDED_METAL=1)
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-O3;-fno-fast-math;-ffp-contract=off;-Wall;-Wextra;-Wconversion;-Werror>")
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
endfunction()
