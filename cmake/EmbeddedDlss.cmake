# Supply an already packaged, verified production runtime; never download DLLs
# implicitly or embed developer/ReShade/neural-rendering binaries.
set(STARFOX_DLSS_RUNTIME_DIRECTORY "" CACHE PATH
    "Verified Windows x64 DLSS runtime folder to embed inside starfox_pc")
if(STARFOX_DLSS_RUNTIME_DIRECTORY)
    if(NOT WIN32 OR WINDOWS_STORE OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Embedded DLSS requires desktop Windows x64")
    endif()
    find_program(STARFOX_PWSH NAMES pwsh REQUIRED)
    get_filename_component(_dlss_dir "${STARFOX_DLSS_RUNTIME_DIRECTORY}" ABSOLUTE)
    get_filename_component(_dlss_install "${_dlss_dir}" DIRECTORY)
    get_filename_component(_dlss_name "${_dlss_dir}" NAME)
    if(NOT _dlss_name STREQUAL "dlss")
        message(FATAL_ERROR "Use the dlss subfolder produced by tools/package_dlss.ps1")
    endif()
    execute_process(COMMAND "${STARFOX_PWSH}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_dlss_package.ps1"
        -Installation "${_dlss_install}" COMMAND_ERROR_IS_FATAL ANY)
    enable_language(RC)
    set(_dlss_files starfox_dlss_native.dll sl.interposer.dll sl.common.dll
        sl.dlss.dll nvngx_dlss.dll Streamline-LICENSE.txt Streamline-THIRD-PARTY.md
        nvngx_dlss.license.txt DLSS-RUNTIME.txt runtime-manifest.json)
    set(_dlss_rc "")
    set(_dlss_header "#pragma once\n#include <array>\nnamespace starfox::app::embedded_dlss {\nstruct File { const char* name; unsigned id; };\ninline constexpr std::array<File,10> files{{\n")
    set(_dlss_digests "")
    set(_dlss_inputs "")
    set(_dlss_id 700)
    foreach(_file IN LISTS _dlss_files)
        set(_path "${_dlss_dir}/${_file}")
        file(SHA256 "${_path}" _digest)
        string(APPEND _dlss_digests "${_digest}")
        file(TO_CMAKE_PATH "${_path}" _path)
        string(APPEND _dlss_rc "${_dlss_id} RCDATA \"${_path}\"\n")
        string(APPEND _dlss_header "{\"${_file}\",${_dlss_id}},\n")
        list(APPEND _dlss_inputs "${_path}")
        math(EXPR _dlss_id "${_dlss_id}+1")
    endforeach()
    string(SHA256 _dlss_package_id "${_dlss_digests}")
    string(APPEND _dlss_header "}};\ninline constexpr char package_id[]=\"${_dlss_package_id}\";\n}\n")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_dlss_manifest.hpp"
        CONTENT "${_dlss_header}" @ONLY)
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/starfox_dlss.rc"
        CONTENT "${_dlss_rc}" @ONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_dlss_inputs})
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/starfox_dlss.rc"
        PROPERTY OBJECT_DEPENDS "${_dlss_inputs}")
    add_library(starfox_dlss_resources OBJECT "${CMAKE_CURRENT_BINARY_DIR}/starfox_dlss.rc")
    target_sources(starfox_pc PRIVATE $<TARGET_OBJECTS:starfox_dlss_resources>)
    target_include_directories(starfox_pc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(starfox_pc PRIVATE STARFOX_EMBEDDED_DLSS=1)
    target_link_libraries(starfox_pc PRIVATE shell32 ole32)
    install(FILES "${_dlss_dir}/Streamline-LICENSE.txt"
        "${_dlss_dir}/Streamline-THIRD-PARTY.md" "${_dlss_dir}/nvngx_dlss.license.txt"
        "${_dlss_dir}/DLSS-RUNTIME.txt" DESTINATION licenses/dlss)
    message(STATUS "Embedding verified standard DLSS runtime (no DLSS5/ReShade)")
endif()
