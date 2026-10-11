# Source-driven, offline build graph. Does not activate reflection history in
# an application or substitute shader compilation for driver qualification.
function(starfox_build_metal_reflection_bundle target source_root sdk output_root)
    if(NOT APPLE OR NOT sdk MATCHES "^(macosx|iphoneos)$")
        message(FATAL_ERROR "Actual supported Apple SDK required")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT sdk STREQUAL "iphoneos")
        message(FATAL_ERROR "macOS libraries cannot be embedded into an iOS target")
    elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT sdk STREQUAL "macosx")
        message(FATAL_ERROR "iOS libraries cannot be embedded into a macOS target")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    get_filename_component(adapter_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(adapter "${adapter_root}/tools/build_metal_bundle.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${adapter}"
        --source "${source_root}" --sdk "${sdk}" inventory
        RESULT_VARIABLE inventory_status OUTPUT_VARIABLE programs
        ERROR_VARIABLE inventory_error OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT inventory_status EQUAL 0)
        message(FATAL_ERROR "Complete Metal source inventory failed: ${inventory_error}")
    endif()
    list(LENGTH programs program_count)
    if(NOT program_count EQUAL 25)
        message(FATAL_ERROR "Full25 Metal programs required")
    endif()
    # Resolve actual SDK tools for dependency tracking, not compilation launch.
    # compile_metal.py retains its original process identity/resource observer.
    foreach(frontend IN ITEMS metal metallib)
        execute_process(COMMAND xcrun --sdk "${sdk}" --find "${frontend}"
            RESULT_VARIABLE tool_status OUTPUT_VARIABLE tool
            ERROR_VARIABLE tool_error OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT tool_status EQUAL 0 OR NOT EXISTS "${tool}")
            message(FATAL_ERROR "Native ${frontend} unavailable: ${tool_error}")
        endif()
        list(APPEND sdk_tools "${tool}")
        file(SHA256 "${tool}" ${frontend}_sha256)
    endforeach()
    foreach(query IN ITEMS path version)
        execute_process(COMMAND xcrun --sdk "${sdk}" --show-sdk-${query}
            RESULT_VARIABLE sdk_status OUTPUT_VARIABLE sdk_${query}
            ERROR_VARIABLE sdk_error OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT sdk_status EQUAL 0 OR sdk_${query} MATCHES "[\"\n\r\\\\]")
            message(FATAL_ERROR "Native SDK ${query} unavailable/unsafe: ${sdk_error}")
        endif()
    endforeach()
    file(MAKE_DIRECTORY "${output_root}")
    set(context "${output_root}/sdk-context.json")
    # file(CONFIGURE) changes the timestamp only if its actual contents change.
    # Switching SDK/toolchains invalidates all25 compile outputs, even when the
    # logical sdk name and .metal sources are otherwise unchanged.
    file(CONFIGURE OUTPUT "${context}" CONTENT
        "{\"sdk\":\"${sdk}\",\"root\":\"${sdk_path}\",\"version\":\"${sdk_version}\",\"metal_sha256\":\"${metal_sha256}\",\"metallib_sha256\":\"${metallib_sha256}\"}\n")
    set(libraries "${output_root}/libraries")
    set(outputs)
    set(previous)
    file(GLOB all_sources CONFIGURE_DEPENDS "${source_root}/shaders/*.metal")
    set(inputs "${adapter}" "${source_root}/tools/compile_metal.py"
        "${source_root}/tools/check_embedding.py" "${source_root}/shaders.json"
        "${source_root}/tools/embedding-inputs.json" ${all_sources} ${sdk_tools} "${context}")
    foreach(program IN LISTS programs)
        set(library "${libraries}/metal-${sdk}-${program}/${program}.metallib")
        set(receipt "${libraries}/metal-${sdk}-${program}/receipt.json")
        add_custom_command(OUTPUT "${library}" "${receipt}"
            COMMAND "${Python3_EXECUTABLE}" "${adapter}"
                --source "${source_root}" --sdk "${sdk}" --shader "${program}"
                --libraries "${libraries}" --context "${context}" compile
            DEPENDS ${inputs} ${previous} VERBATIM
            COMMENT "Compile original strict Metal program ${program} (${sdk})")
        # Serial dependency chain works with Ninja, Make, and Xcode. Raising
        # the outer build's job count cannot launch 25 memory-heavy compilers.
        set(previous "${library}" "${receipt}")
        list(APPEND outputs "${library}" "${receipt}")
    endforeach()
    set(bundle "${output_root}/bundle")
    add_custom_command(OUTPUT "${bundle}/libraries.S" "${bundle}/embedding_programs.hpp"
        COMMAND "${Python3_EXECUTABLE}" "${adapter}"
            --source "${source_root}" --sdk "${sdk}" --libraries "${libraries}"
            --out "${bundle}" --context "${context}" bundle
        DEPENDS ${outputs} ${inputs} VERBATIM
        COMMENT "Embed complete25 validated ${sdk} programs without network access")
    add_custom_target(${target} DEPENDS "${bundle}/libraries.S" "${bundle}/embedding_programs.hpp")
    set(${target}_DIRECTORY "${bundle}" PARENT_SCOPE)
endfunction()

function(starfox_add_source_built_metal_reflection_history target source_root sdk output_root)
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR "Real SDL3 GPU implementation required")
    endif()
    starfox_build_metal_reflection_bundle(${target}_programs
        "${source_root}" "${sdk}" "${output_root}")
    set(bundle "${${target}_programs_DIRECTORY}")
    set_source_files_properties("${bundle}/libraries.S" PROPERTIES GENERATED TRUE)
    add_library(${target} STATIC
        "${source_root}/src/render/gpu_calibrated_reflection_history.cpp"
        "${source_root}/src/render/embedded_metal_reflection_programs.cpp"
        "${bundle}/libraries.S")
    add_dependencies(${target} ${target}_programs)
    target_include_directories(${target} PUBLIC "${source_root}/include"
        PRIVATE "${bundle}")
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PRIVATE
        STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1
        STARFOX_REFLECTION_EMBEDDED_METAL=1
        STARFOX_REFLECTION_METAL_EXACT_WORKGROUP=1)
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-O3;-fno-fast-math;-ffp-contract=off;-Wall;-Wextra;-Wconversion;-Werror>")
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
endfunction()
