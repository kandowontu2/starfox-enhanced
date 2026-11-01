include_guard(GLOBAL)
set(STARFOX_VITA_PACKAGE_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

function(starfox_add_vita_package runtime_target)
    if(NOT DEFINED ENV{VITASDK} OR "$ENV{VITASDK}" STREQUAL "")
        message(FATAL_ERROR "VITASDK must point to an installed VitaSDK")
    endif()
    include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(STARFOX_VITA_TITLE_ID "SFXE00001" CACHE STRING "Nine-character PS Vita title id")
    set(STARFOX_VITA_APP_VERSION "00.90" CACHE STRING "Two-component PS Vita application version")
    if(NOT STARFOX_VITA_TITLE_ID MATCHES "^[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]$"
        OR NOT STARFOX_VITA_APP_VERSION MATCHES "^[0-9][0-9]\\.[0-9][0-9]$")
        message(FATAL_ERROR "Invalid Vita title id or version")
    endif()
    # Preserve room for vita-elf-create's SCE metadata before read-only data.
    target_link_options(${runtime_target} PRIVATE "-Wl,--defsym=__sce_headroom=0x10000")
    target_sources(${runtime_target} PRIVATE
        "${STARFOX_VITA_PACKAGE_SOURCE_ROOT}/platform/vita/runtime_memory.cpp")
    vita_create_self(eboot.bin ${runtime_target})

    set(root "${STARFOX_VITA_PACKAGE_SOURCE_ROOT}")
    include("${root}/cmake/EnhancedBackdropAssets.cmake")
    starfox_enhanced_backdrop_resources(backdrop_arguments backdrop_files)
    set(backdrop_mappings)
    foreach(backdrop IN LISTS backdrop_files)
        file(RELATIVE_PATH packaged_path "${root}" "${backdrop}")
        list(APPEND backdrop_mappings -a "${backdrop}=${packaged_path}")
    endforeach()
    set(art "${CMAKE_CURRENT_BINARY_DIR}/generated/vita")
    set(icon "${art}/sce_sys/icon0.png")
    set(bg "${art}/sce_sys/livearea/contents/bg.png")
    set(startup "${art}/sce_sys/livearea/contents/startup.png")
    set(template "${root}/platform/vita/sce_sys/livearea/contents/template.xml")
    file(GLOB_RECURSE icon_inputs CONFIGURE_DEPENDS "${root}/assets/icons/vita/*.png")
    add_custom_command(OUTPUT "${icon}" "${bg}" "${startup}"
        COMMAND "${Python3_EXECUTABLE}" "${root}/tools/generate_vita_assets.py" --output "${art}"
        DEPENDS "${root}/tools/generate_vita_assets.py" ${icon_inputs}
        VERBATIM)
    set(sfo "${CMAKE_CURRENT_BINARY_DIR}/StarFoxEnhanced_param.sfo")
    separate_arguments(extra_sfo_flags NATIVE_COMMAND "${VITA_MKSFOEX_FLAGS}")
    separate_arguments(extra_vpk_flags NATIVE_COMMAND "${VITA_PACK_VPK_FLAGS}")
    add_custom_command(OUTPUT "${sfo}"
        COMMAND "${VITA_MKSFOEX}" ${extra_sfo_flags} -s "TITLE_ID=${STARFOX_VITA_TITLE_ID}"
            -s "APP_VER=${STARFOX_VITA_APP_VERSION}" "Star Fox Enhanced" "${sfo}"
        VERBATIM)
    set(vpk "${CMAKE_CURRENT_BINARY_DIR}/StarFoxEnhanced.vpk")
    # Keep each resource mapping a single VERBATIM argument. VitaSDK's FILE
    # macro concatenates unquoted absolute paths, breaking source/build paths
    # containing spaces. Use the same SDK packer directly with explicit deps.
    add_custom_command(OUTPUT "${vpk}"
        COMMAND "${VITA_PACK_VPK}" ${extra_vpk_flags} -s "${sfo}" -b "${CMAKE_CURRENT_BINARY_DIR}/eboot.bin"
            -a "${icon}=sce_sys/icon0.png"
            -a "${bg}=sce_sys/livearea/contents/bg.png"
            -a "${startup}=sce_sys/livearea/contents/startup.png"
            -a "${template}=sce_sys/livearea/contents/template.xml"
            ${backdrop_mappings} "${vpk}"
        COMMAND "${Python3_EXECUTABLE}" "${root}/tools/check_vita_package.py" "${vpk}"
            --title-id "${STARFOX_VITA_TITLE_ID}" --require-backdrops
        DEPENDS "${sfo}" "${CMAKE_CURRENT_BINARY_DIR}/eboot.bin"
            "${icon}" "${bg}" "${startup}" "${template}" ${backdrop_files}
            "${root}/tools/check_vita_package.py"
        VERBATIM)
    add_custom_target(StarFoxEnhanced.vpk-vpk ALL DEPENDS "${vpk}")
    add_dependencies(StarFoxEnhanced.vpk-vpk eboot.bin-self)
endfunction()
