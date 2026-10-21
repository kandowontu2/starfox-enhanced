# One lightweight service, shared by the SDL player and standalone tests.
get_filename_component(STARFOX_UPDATE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(READ "${STARFOX_UPDATE_SOURCE_ROOT}/tools/package/apply-update.ps1" STARFOX_UPDATE_SCRIPT)
file(READ "${STARFOX_UPDATE_SOURCE_ROOT}/tools/package/apply-update-linux.py" STARFOX_LINUX_UPDATE_SCRIPT)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${STARFOX_UPDATE_SOURCE_ROOT}/tools/package/apply-update.ps1"
    "${STARFOX_UPDATE_SOURCE_ROOT}/tools/package/apply-update-linux.py")
configure_file("${STARFOX_UPDATE_SOURCE_ROOT}/src/app/update_build.hpp.in"
    "${PROJECT_BINARY_DIR}/generated/update_build.hpp" @ONLY)
add_library(starfox_updates STATIC
    "${STARFOX_UPDATE_SOURCE_ROOT}/src/app/update_release.cpp"
    "${STARFOX_UPDATE_SOURCE_ROOT}/src/app/update_service.cpp")
target_compile_features(starfox_updates PUBLIC cxx_std_20)
target_include_directories(starfox_updates PUBLIC "${STARFOX_UPDATE_SOURCE_ROOT}/include"
    PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(starfox_updates PRIVATE SDL3::SDL3)
if(WIN32 AND NOT WINDOWS_STORE)
    target_link_libraries(starfox_updates PRIVATE winhttp)
    target_compile_definitions(starfox_updates PRIVATE STARFOX_WINDOWS_UPDATES=1)
elseif((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR APPLE) AND NOT IOS AND NOT ANDROID)
    target_compile_definitions(starfox_updates PRIVATE STARFOX_CURL_UPDATE_CHECK=1)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(starfox_updates PRIVATE STARFOX_LINUX_UPDATES=1)
    endif()
endif()
