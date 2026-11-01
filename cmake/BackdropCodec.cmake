include_guard(GLOBAL)
add_library(starfox_backdrop_codec STATIC
    "${CMAKE_CURRENT_LIST_DIR}/../src/render/backdrop_png.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/lodepng/lodepng.cpp")
target_compile_features(starfox_backdrop_codec PUBLIC cxx_std_20)
target_include_directories(starfox_backdrop_codec PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../include"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../third_party/lodepng")
target_compile_definitions(starfox_backdrop_codec PRIVATE LODEPNG_NO_COMPILE_ENCODER
    LODEPNG_NO_COMPILE_DISK LODEPNG_NO_COMPILE_CPP LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS)
if(VITA)
    # Vita's SELF converter cannot encode the R_ARM_BASE_PREL relocations
    # produced by -fPIC. This codec is linked into its executable, not a DSO.
    set_target_properties(starfox_backdrop_codec PROPERTIES POSITION_INDEPENDENT_CODE OFF)
else()
    # Android and the other shared-library hosts need PIC static objects.
    set_target_properties(starfox_backdrop_codec PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
