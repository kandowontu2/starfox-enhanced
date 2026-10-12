# Optional local tests of the real cartridge VM and native scene owner.
# Paths point to existing owner-provided assets. Nothing is copied or embedded,
# and asset-free CI continues to run without these opt-in inputs.
set(STARFOX_ROM_FILE "" CACHE FILEPATH "Existing Original cartridge for local 3DS regression tests")
set(STARFOX_SYMBOLS_FILE "" CACHE FILEPATH "Existing Original symbols for local 3DS regression tests")
set(STARFOX_EX_ROM_FILE "" CACHE FILEPATH "Existing EX cartridge for local 3DS regression tests")
set(STARFOX_EX_SYMBOLS_FILE "" CACHE FILEPATH "Existing EX symbols for local 3DS regression tests")

function(starfox_3ds_register_cartridge variant rom symbols)
    if(NOT EXISTS "${rom}" OR NOT EXISTS "${symbols}" OR IS_DIRECTORY "${rom}" OR IS_DIRECTORY "${symbols}")
        return()
    endif()
    add_test(NAME starfox_3ds_game_hud_${variant}
        COMMAND starfox_3ds_game_hud_check "${rom}" "${symbols}")
    add_test(NAME starfox_3ds_game_session_${variant}
        COMMAND starfox_3ds_game_session_check "${rom}" "${symbols}")
    add_test(NAME starfox_3ds_parallel_game_session_${variant}
        COMMAND starfox_3ds_parallel_game_session_check "${rom}" "${symbols}")
    foreach(check IN ITEMS capabilities native-depth states runtime-editor-parity editor-chords fps-chords stage-sweep death-source-route continue-yes-source-route continue-no-source-route)
        string(REPLACE "-" "_" name "${check}")
        set(argument "--${check}")
        if(check MATCHES "^(capabilities|native-depth|states)$")
            set(argument "--${check}-only")
        endif()
        add_test(NAME starfox_3ds_game_${name}_${variant}
            COMMAND starfox_3ds_game_session_check "${rom}" "${symbols}" "${argument}")
        set_tests_properties(starfox_3ds_game_${name}_${variant} PROPERTIES TIMEOUT 1800 LABELS "3ds;cartridge;${variant}")
    endforeach()
    add_test(NAME starfox_3ds_game_models_${variant}
        COMMAND starfox_3ds_game_models_check "${rom}" "${symbols}")
    add_test(NAME starfox_3ds_native_model_${variant}
        COMMAND starfox_3ds_native_model_check "${rom}" "${symbols}")
    add_test(NAME starfox_3ds_game_effects_${variant}
        COMMAND starfox_3ds_game_models_check "${rom}" "${symbols}" --native-effects-flow)
    add_test(NAME starfox_3ds_game_menu_composition_${variant}
        COMMAND starfox_3ds_game_models_check "${rom}" "${symbols}" --native-menu-composition)
    add_test(NAME starfox_3ds_game_landscape_${variant}
        COMMAND starfox_3ds_game_models_check "${rom}" "${symbols}" --native-landscape-flow)
    set_tests_properties(starfox_3ds_game_menu_composition_${variant}
        starfox_3ds_game_landscape_${variant} PROPERTIES TIMEOUT 600)
    if(variant STREQUAL "original")
        foreach(check IN ITEMS fortuna corneria attack-carrier)
            string(REPLACE "-" "_" name "${check}")
            add_test(NAME starfox_3ds_game_${name}_source_route_original
                COMMAND starfox_3ds_game_session_check "${rom}" "${symbols}" "--${check}-source-route")
            set_tests_properties(starfox_3ds_game_${name}_source_route_original PROPERTIES TIMEOUT 1800)
        endforeach()
        add_test(NAME starfox_3ds_game_attack_carrier_composition_original
            COMMAND starfox_3ds_game_models_check "${rom}" "${symbols}" LEVEL1_1 36000 --attack-carrier-complete)
        add_test(NAME starfox_3ds_game_fortuna_composition_original
            COMMAND starfox_3ds_game_models_check "${rom}" "${symbols}" LEVEL3_3 36000 --fortuna-complete)
        # Keep the root suite's complete source-phase bounds and deadlines.
        set_tests_properties(starfox_3ds_game_attack_carrier_composition_original
            starfox_3ds_game_fortuna_composition_original PROPERTIES TIMEOUT 7200)
    endif()
endfunction()

starfox_3ds_register_cartridge(original "${STARFOX_ROM_FILE}" "${STARFOX_SYMBOLS_FILE}")
starfox_3ds_register_cartridge(ex "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
