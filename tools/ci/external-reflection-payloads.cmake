# Follow-up candidate only: append to the existing DXR overlay after the live
# build retires. Original shader sources, flags and generating commands stay.
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    return()
endif()
enable_language(ASM)
set(STARFOX_CI_REFLECTION_EMITTER "${CMAKE_CURRENT_LIST_DIR}/externalize-reflection-payloads.py")
function(starfox_ci_external_reflection_payloads)
    if(NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT TARGET starfox_displayxr)
        message(FATAL_ERROR "Reflection data candidate requires the original desktop x64 MinGW library")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(external_dir "${CMAKE_BINARY_DIR}/external-reflection-payloads")
    get_target_property(reflection_sources starfox_displayxr SOURCES)
    set(payload_count 0)
    foreach(source IN LISTS reflection_sources)
        if(source MATCHES "/generated/(native_reflection_(index_[A-Za-z_]+|capture)_(dxil|spirv))\\.hpp$")
            set(symbol "${CMAKE_MATCH_1}")
            add_custom_command(OUTPUT "${external_dir}/${symbol}.hpp" "${external_dir}/${symbol}.S" "${external_dir}/${symbol}.bin"
                BYPRODUCTS "${external_dir}/${symbol}.json"
                COMMAND "${Python3_EXECUTABLE}" "${STARFOX_CI_REFLECTION_EMITTER}"
                    --header "${source}" --output "${external_dir}" --symbol "${symbol}"
                DEPENDS "${source}" "${STARFOX_CI_REFLECTION_EMITTER}" VERBATIM)
            target_sources(starfox_displayxr PRIVATE "${external_dir}/${symbol}.hpp" "${external_dir}/${symbol}.S")
            math(EXPR payload_count "${payload_count}+1")
        endif()
    endforeach()
    if(NOT payload_count EQUAL 42)
        message(FATAL_ERROR "Expected every one of the 42 original generated programs; no smaller package is permitted")
    endif()
    set(legacy_outputs "${external_dir}/external_reflection_history_payloads_v2.hpp")
    set(legacy_receipts)
    foreach(stem IN ITEMS reflection_history lobes paths curved_paths)
        foreach(backend IN ITEMS dxil spirv)
            list(APPEND legacy_outputs "${external_dir}/${stem}_${backend}.S" "${external_dir}/${stem}_${backend}.bin")
            list(APPEND legacy_receipts "${external_dir}/${stem}_${backend}.json")
            target_sources(starfox_displayxr PRIVATE "${external_dir}/${stem}_${backend}.S")
        endforeach()
    endforeach()
    add_custom_command(OUTPUT ${legacy_outputs}
        BYPRODUCTS ${legacy_receipts}
        COMMAND "${Python3_EXECUTABLE}" "${STARFOX_CI_REFLECTION_EMITTER}"
            --legacy-root "${CMAKE_SOURCE_DIR}" --output "${external_dir}"
        DEPENDS "${STARFOX_CI_REFLECTION_EMITTER}"
            "${CMAKE_SOURCE_DIR}/src/render/shaders/generated/calibrated_scene_portable.hpp"
            "${CMAKE_SOURCE_DIR}/src/render/shaders/generated/calibrated_reflection_lobes.hpp"
            "${CMAKE_SOURCE_DIR}/src/render/shaders/generated/calibrated_reflection_paths.hpp"
            "${CMAKE_SOURCE_DIR}/src/render/shaders/generated/calibrated_reflection_curved_paths.hpp" VERBATIM)
    target_sources(starfox_displayxr PRIVATE "${external_dir}/external_reflection_history_payloads_v2.hpp")
    target_include_directories(starfox_displayxr BEFORE PRIVATE "${external_dir}")
    target_compile_definitions(starfox_displayxr PRIVATE STARFOX_REFLECTION_HISTORY_EXTERNAL_PAYLOADS=1)
    message(STATUS "Reflection embedding candidate: 42 freshly generated + eight original unchanged payloads")
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL starfox_ci_external_reflection_payloads)
