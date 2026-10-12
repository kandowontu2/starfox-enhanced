# Temporary source-only native build overlay. The original DXC custom commands
# and headers are unmodified. Only the C++ representation of their exact bytes
# changes, with fixed-size declarations and COFF incbin object members.
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    return()
endif()
# CMAKE_PROJECT_INCLUDE runs at root file scope after compiler setup. CMake
# forbids enabling a language from a deferred call; only target attachment is
# deferred until the unchanged root has declared its existing library.
enable_language(ASM)
set(STARFOX_CI_DXR_EMITTER "${CMAKE_CURRENT_LIST_DIR}/externalize-dxr-payload.py")
function(starfox_ci_external_dxr_payloads)
    if(NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT TARGET starfox_core)
        message(FATAL_ERROR "External DXR payload candidate requires desktop x64 MinGW core")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(external_dir "${CMAKE_BINARY_DIR}/external-dxr-payloads")
    get_target_property(core_sources starfox_core SOURCES)
    set(payload_count 0)
    foreach(source IN LISTS core_sources)
        if(source MATCHES "/generated/(shadow_dxr_[A-Za-z0-9_]+|reflection_liquid_history_shader)\\.hpp$")
            set(symbol "${CMAKE_MATCH_1}")
            set(declaration "${external_dir}/${symbol}.hpp")
            set(assembly "${external_dir}/${symbol}.S")
            set(binary "${external_dir}/${symbol}.bin")
            add_custom_command(OUTPUT "${declaration}" "${assembly}" "${binary}"
                BYPRODUCTS "${external_dir}/${symbol}.json"
                COMMAND "${Python3_EXECUTABLE}" "${STARFOX_CI_DXR_EMITTER}"
                    --header "${source}" --output "${external_dir}" --symbol "${symbol}"
                DEPENDS "${source}" "${STARFOX_CI_DXR_EMITTER}"
                VERBATIM)
            target_sources(starfox_core PRIVATE "${declaration}" "${assembly}")
            math(EXPR payload_count "${payload_count}+1")
        endif()
    endforeach()
    if(NOT payload_count EQUAL 22)
        message(FATAL_ERROR "Expected all 22 DXR payloads, found ${payload_count}; never drop a program to fit memory")
    endif()
    target_include_directories(starfox_core BEFORE PRIVATE "${external_dir}")
    message(STATUS "Native diagnostic candidate: all ${payload_count} unchanged DXR payloads through COFF data objects")
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL starfox_ci_external_dxr_payloads)
