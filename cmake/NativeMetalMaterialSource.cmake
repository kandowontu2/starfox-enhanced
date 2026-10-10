# Public source embedding fragment for the candidate. Apply only after full
# producer/source validation, not as a replacement for the app's CMake graph.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_native_material_coverage.inc")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/metal_native_material_coverage.inc"
    STARFOX_METAL_NATIVE_COVERAGE_SOURCE)
configure_file(src/render/shaders/metal_native_material_shared.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/metal_native_material_shared.hpp" @ONLY)
