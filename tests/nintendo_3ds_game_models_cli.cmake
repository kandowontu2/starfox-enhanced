if(NOT DEFINED CHECKER OR NOT EXISTS "${CHECKER}")
    message(FATAL_ERROR "The actual 3DS scene checker is required")
endif()

execute_process(COMMAND "${CHECKER}" --help
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT output MATCHES "BOOT \\(240 frames\\) and LEVEL1_1 \\(1440 frames\\)"
    OR NOT output MATCHES "SOURCE_FRAMES: 1..3600"
    OR NOT output MATCHES "--bundle-original BIN" OR NOT output MATCHES "--bundle-ex BIN"
    OR NOT output MATCHES "MAX_PHASES 1..21600" OR NOT output MATCHES "--fortuna-water"
    OR NOT output MATCHES "MAX_PHASES 1..36000" OR NOT output MATCHES "--fortuna-complete"
    OR NOT output MATCHES "--native-menu-composition"
    OR NOT output MATCHES "--native-effects-flow"
    OR NOT output MATCHES "--all-optics: check every requested frame at strength 2")
    message(FATAL_ERROR "Scene checker help/default scope changed: ${output}${error}")
endif()

foreach(mode IN ITEMS --native-menu-composition --native-effects-flow)
foreach(kind IN ITEMS --bundle-original --bundle-ex)
    execute_process(COMMAND "${CHECKER}" "${kind}" missing-private-BIN "${mode}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 1 OR NOT error MATCHES "Cannot open companion BIN:")
        message(FATAL_ERROR "Native composition mode ${mode} did not reach the verified asset decoder: ${result} / ${output}${error}")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" --bundle-original missing-private-BIN "${mode}" 240
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "Usage: check_3ds_game_models")
    message(FATAL_ERROR "Native composition mode ${mode} accepted an extra frame argument: ${result} / ${output}${error}")
endif()
endforeach()

foreach(frames IN ITEMS 0 36001 -1 1.5 36000x 4294967296)
    execute_process(COMMAND "${CHECKER}" --bundle-original missing-private-BIN LEVEL3_3 "${frames}" --fortuna-complete
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 1 OR NOT error MATCHES "SOURCE_FRAMES must be a whole number from 1 through 36000")
        message(FATAL_ERROR "Incorrect full Fortuna bound rejection for '${frames}': ${result} / ${output}${error}")
    endif()
endforeach()
foreach(kind IN ITEMS --bundle-original --bundle-ex)
    execute_process(COMMAND "${CHECKER}" "${kind}" missing-private-BIN LEVEL1_3 240 --fortuna-complete
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 1 OR NOT error MATCHES "--fortuna-complete requires Original LEVEL3_3")
        message(FATAL_ERROR "Full Fortuna accepted another stage: ${result} / ${output}${error}")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" --bundle-ex missing-private-BIN LEVEL3_3 36000 --fortuna-complete
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "--fortuna-complete requires Original LEVEL3_3")
    message(FATAL_ERROR "Full Fortuna accepted EX: ${result} / ${output}${error}")
endif()
execute_process(COMMAND "${CHECKER}" --bundle-original missing-private-BIN LEVEL3_3 36000 --fortuna-complete
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "Cannot open companion BIN:")
    message(FATAL_ERROR "Valid full Fortuna bound did not reach its validated BIN decoder: ${result} / ${output}${error}")
endif()

# Validate before opening private assets. A generic missing-ROM failure is
# insufficient: every bad bound must identify the SOURCE_FRAMES argument.
foreach(frames IN ITEMS 0 3601 -1 1.5 240x 4294967296)
    foreach(optics IN ITEMS default maximum)
    set(optics_argument)
    if(optics STREQUAL maximum)
        set(optics_argument --all-optics)
    endif()
    execute_process(COMMAND "${CHECKER}" missing-private-ROM missing-private-SYMBOLS LEVEL1_3 "${frames}"
        ${optics_argument}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 1 OR NOT error MATCHES "SOURCE_FRAMES must be a whole number from 1 through 3600")
        message(FATAL_ERROR "Incorrect frame-count rejection for '${frames}': ${result} / ${output}${error}")
    endif()
    endforeach()
endforeach()

foreach(frames IN ITEMS 0 21601 -1 1.5 21600x 4294967296)
    execute_process(COMMAND "${CHECKER}" --bundle-original missing-private-BIN LEVEL3_3 "${frames}" --fortuna-water
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 1 OR NOT error MATCHES "SOURCE_FRAMES must be a whole number from 1 through 21600")
        message(FATAL_ERROR "Incorrect Fortuna bound rejection for '${frames}': ${result} / ${output}${error}")
    endif()
endforeach()
foreach(kind IN ITEMS --bundle-original --bundle-ex)
    execute_process(COMMAND "${CHECKER}" "${kind}" missing-private-BIN LEVEL1_3 240 --fortuna-water
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 1 OR NOT error MATCHES "--fortuna-water requires Original LEVEL3_3")
        message(FATAL_ERROR "Fortuna accepted another stage: ${result} / ${output}${error}")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" --bundle-ex missing-private-BIN LEVEL3_3 21600 --fortuna-water
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "--fortuna-water requires Original LEVEL3_3")
    message(FATAL_ERROR "Fortuna accepted EX: ${result} / ${output}${error}")
endif()
execute_process(COMMAND "${CHECKER}" --bundle-original missing-private-BIN LEVEL3_3 21600 --fortuna-water
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "Cannot open companion BIN:")
    message(FATAL_ERROR "Valid Fortuna bound did not reach its validated BIN decoder: ${result} / ${output}${error}")
endif()

foreach(kind IN ITEMS --bundle-original --bundle-ex)
    execute_process(COMMAND "${CHECKER}" "${kind}" missing-private-BIN LEVEL1_3 240
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 1 OR NOT error MATCHES "Cannot open companion BIN:")
        message(FATAL_ERROR "Companion selection did not reach its validated decoder: ${result} / ${output}${error}")
    endif()
endforeach()
execute_process(COMMAND "${CHECKER}" --bundle-unknown missing-private-BIN LEVEL1_3 240
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "Usage: check_3ds_game_models")
    message(FATAL_ERROR "Unknown companion mode was interpreted as a ROM path: ${result} / ${output}${error}")
endif()

execute_process(COMMAND "${CHECKER}" ROM SYMBOLS LEVEL1_3 240 extra
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "Usage: check_3ds_game_models")
    message(FATAL_ERROR "Unexpected extra-argument handling: ${result} / ${output}${error}")
endif()
execute_process(COMMAND "${CHECKER}" ROM SYMBOLS LEVEL1_3 240 --all-optics extra
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "Usage: check_3ds_game_models")
    message(FATAL_ERROR "Unexpected all-optics extra-argument handling: ${result} / ${output}${error}")
endif()
execute_process(COMMAND "${CHECKER}" ROM SYMBOLS LEVEL3_3 36000 --fortuna-complete extra
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "Usage: check_3ds_game_models")
    message(FATAL_ERROR "Unexpected full-Fortuna extra-argument handling: ${result} / ${output}${error}")
endif()
message(STATUS "Scene checker help/defaults, twenty-four invalid bounds, short/full Fortuna stage/experience gates and extra-argument rejections passed")
