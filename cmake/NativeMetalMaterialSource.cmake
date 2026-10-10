# Public source embedding fragment for the candidate. Apply only after full
# producer/source validation, not as a replacement for the app's CMake graph.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_native_material_coverage.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_material_colour.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_native_material_colour.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/liquid_optics.hlsli"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/metal_liquid_optics.py")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/metal_liquid_optics.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --output "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_liquid_optics.inc"
    RESULT_VARIABLE STARFOX_METAL_LIQUID_RESULT)
if(NOT STARFOX_METAL_LIQUID_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not adapt canonical liquid geometry for native Metal")
endif()
file(READ "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_liquid_optics.inc" STARFOX_LIQUID_OPTICS_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_native_material_coverage.inc"
    STARFOX_METAL_NATIVE_COVERAGE_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_material_colour.inc"
    STARFOX_METAL_MATERIAL_COLOUR_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_native_material_colour.inc"
    STARFOX_METAL_NATIVE_COLOUR_SOURCE)
configure_file(src/render/shaders/metal_native_material_shared.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_native_material_shared.hpp" @ONLY)
