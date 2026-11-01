include_guard(GLOBAL)
add_library(starfox_backdrop_codec STATIC
    "${CMAKE_CURRENT_LIST_DIR}/../src/render/backdrop_png.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/lodepng/lodepng.cpp")
target_compile_features(starfox_backdrop_codec PUBLIC cxx_std_20)
target_include_directories(starfox_backdrop_codec PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../include"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../third_party/lodepng")
target_compile_definitions(starfox_backdrop_codec PRIVATE LODEPNG_NO_COMPILE_ENCODER
    LODEPNG_NO_COMPILE_DISK LODEPNG_NO_COMPILE_CPP LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS)
set_target_properties(starfox_backdrop_codec PROPERTIES POSITION_INDEPENDENT_CODE ON)
