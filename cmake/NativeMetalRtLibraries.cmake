# Compile the exact four actual application kernels, including both empty-scene
# specializations. The ordinary MetalHardwareRt owner loads these bytes directly.
# Only the dedicated RT path needs their macOS14/iOS17 library minimum; the app
# keeps its existing macOS11/iOS15 minimum and unsupported-hardware behavior.
function(starfox_embed_native_metal_rt target)
    if(NOT APPLE OR NOT TARGET ${target})
        message(FATAL_ERROR "Actual Apple application target required for native Metal RT libraries")
    endif()
    enable_language(ASM)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    find_program(STARFOX_NATIVE_METAL_XCRUN xcrun REQUIRED)
    if(IOS)
        set(sdk iphoneos)
        set(deployment air64-apple-ios17.0)
    else()
        set(sdk macosx)
        set(deployment air64-apple-macosx14.0)
    endif()
    set(STARFOX_NATIVE_METAL_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/native-metal-rt")
    set(source_outputs)
    set(libraries)
    foreach(name IN ITEMS shadow reflection empty_shadow empty_reflection)
        list(APPEND source_outputs "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.metal")
        list(APPEND libraries "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.metallib")
    endforeach()
    add_custom_command(OUTPUT ${source_outputs}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_metal_rt_shaders.py"
            --generated-dir "${CMAKE_CURRENT_BINARY_DIR}/generated"
            --emit-only "${STARFOX_NATIVE_METAL_DIRECTORY}"
        DEPENDS tools/check_metal_rt_shaders.py src/render/metal_hardware_rt.mm
            src/render/shaders/metal_water_shared.hpp.in src/render/shaders/metal_native_material_shared.hpp.in
            "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_water_shared.hpp"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_native_material_shared.hpp"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_liquid_optics.inc"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_material_colour.inc"
            include/starfox/render/lava_surface.inc include/starfox/render/water_caustics.inc
            include/starfox/render/water_transmission.inc include/starfox/render/metal_native_material_coverage.inc
            include/starfox/render/metal_native_material_colour.inc
        VERBATIM)
    foreach(name IN ITEMS shadow reflection empty_shadow empty_reflection)
        add_custom_command(OUTPUT "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.metallib"
            COMMAND "${STARFOX_NATIVE_METAL_XCRUN}" --sdk "${sdk}" metal
                -std=metal3.0 -target "${deployment}" -O3 -fno-fast-math -ffp-contract=off
                -c "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.metal"
                -o "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.air"
            COMMAND "${STARFOX_NATIVE_METAL_XCRUN}" --sdk "${sdk}" metallib
                "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.air"
                -o "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.metallib"
            DEPENDS ${source_outputs}
            BYPRODUCTS "${STARFOX_NATIVE_METAL_DIRECTORY}/${name}.air"
            VERBATIM)
    endforeach()
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/native_metal_rt_payloads.S.in"
        "${STARFOX_NATIVE_METAL_DIRECTORY}/native_metal_rt_payloads.S" @ONLY)
    add_custom_target(starfox_native_metal_rt_libraries DEPENDS ${libraries})
    add_dependencies(${target} starfox_native_metal_rt_libraries)
    set_source_files_properties("${STARFOX_NATIVE_METAL_DIRECTORY}/native_metal_rt_payloads.S"
        PROPERTIES OBJECT_DEPENDS "${libraries}")
    target_sources(${target} PRIVATE src/render/native_metal_rt_programs.cpp
        "${STARFOX_NATIVE_METAL_DIRECTORY}/native_metal_rt_payloads.S")
    target_compile_definitions(${target} PRIVATE STARFOX_EMBED_NATIVE_METAL_RT=1)
endfunction()
