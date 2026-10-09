# Shared presentation/input standard used by native VR and the desktop stereo
# renderer's shared menu packets. Keep one target when both are enabled.
if(TARGET starfox_sfvr)
    return()
endif()
enable_language(C)
add_library(starfox_sfvr STATIC
    third_party/sfvr/src/sfvr_haptics.c third_party/sfvr/src/sfvr_perf.c
    third_party/sfvr/src/sfvr_room.c third_party/sfvr/src/sfvr_settings.c
    third_party/sfvr/src/sfvr_turn.c third_party/sfvr/src/sfvr_view.c)
target_include_directories(starfox_sfvr PUBLIC third_party/sfvr/include)
set_target_properties(starfox_sfvr PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_link_libraries(starfox_sfvr PUBLIC m)
endif()
