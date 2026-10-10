# Public source embedding fragment for the candidate. Apply only after full
# producer/source validation, not as a replacement for the app's CMake graph.
if(NOT DEFINED STARFOX_METAL_SOURCE_ROOT)
    set(STARFOX_METAL_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/metal_native_material_coverage.inc"
    "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/metal_native_material_colour.inc"
    "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/lava_surface.inc"
    "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/water_caustics.inc"
    "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/water_transmission.inc"
    "${STARFOX_METAL_SOURCE_ROOT}/src/render/shaders/liquid_optics.hlsli"
    "${STARFOX_METAL_SOURCE_ROOT}/src/render/shaders/calibrated_colour.hlsli"
    "${STARFOX_METAL_SOURCE_ROOT}/src/vr/shaders/scene_colour.hlsli"
    "${STARFOX_METAL_SOURCE_ROOT}/src/render/shaders/metal_water_shared.hpp.in"
    "${STARFOX_METAL_SOURCE_ROOT}/src/render/shaders/metal_native_material_shared.hpp.in"
    "${STARFOX_METAL_SOURCE_ROOT}/tools/metal_liquid_optics.py"
    "${STARFOX_METAL_SOURCE_ROOT}/tools/metal_material_colour.py")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${STARFOX_METAL_SOURCE_ROOT}/tools/metal_liquid_optics.py"
    --root "${STARFOX_METAL_SOURCE_ROOT}"
    --output "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_liquid_optics.inc"
    RESULT_VARIABLE STARFOX_METAL_LIQUID_RESULT)
if(NOT STARFOX_METAL_LIQUID_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not adapt canonical liquid geometry for native Metal")
endif()
file(READ "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_liquid_optics.inc" STARFOX_LIQUID_OPTICS_SOURCE)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${STARFOX_METAL_SOURCE_ROOT}/tools/metal_material_colour.py"
    --root "${STARFOX_METAL_SOURCE_ROOT}"
    --output "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_material_colour.inc"
    RESULT_VARIABLE STARFOX_METAL_COLOUR_RESULT)
if(NOT STARFOX_METAL_COLOUR_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not adapt canonical colour equations for native Metal")
endif()
file(READ "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/metal_native_material_coverage.inc"
    STARFOX_METAL_NATIVE_COVERAGE_SOURCE)
file(READ "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_material_colour.inc"
    STARFOX_METAL_MATERIAL_COLOUR_SOURCE)
file(READ "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/metal_native_material_colour.inc"
    STARFOX_METAL_NATIVE_COLOUR_SOURCE)
configure_file("${STARFOX_METAL_SOURCE_ROOT}/src/render/shaders/metal_native_material_shared.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_native_material_shared.hpp" @ONLY)
file(READ "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/lava_surface.inc" STARFOX_LAVA_SURFACE_SOURCE)
file(READ "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/water_caustics.inc" STARFOX_WATER_CAUSTICS_SOURCE)
file(READ "${STARFOX_METAL_SOURCE_ROOT}/include/starfox/render/water_transmission.inc" STARFOX_WATER_TRANSMISSION_SOURCE)
configure_file("${STARFOX_METAL_SOURCE_ROOT}/src/render/shaders/metal_water_shared.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_water_shared.hpp" @ONLY)
