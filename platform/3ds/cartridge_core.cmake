# The cartridge VM/SPC/HUD graph only: no SDL, desktop GPU, MSU codecs or
# OpenXR dependency. The same pinned CPU and state-enabled SPC sources as the
# main project are used, not a substitute emulator or a diagnostic stub.
include(FetchContent)
FetchContent_Declare(retro_cpu
    GIT_REPOSITORY https://github.com/achaulk/retro_cpu.git
    GIT_TAG ea9049ab25084334f7cc1907b3a98bf1c2604a03
    SOURCE_SUBDIR _starfox_no_upstream_project)
FetchContent_Declare(snes_spc
    GIT_REPOSITORY https://github.com/blarggs-audio-libraries/snes_spc.git
    GIT_TAG ec8ee2bbe30451614c1d02a83f7af1c97d497d45
    SOURCE_SUBDIR _starfox_no_upstream_project)
FetchContent_MakeAvailable(retro_cpu snes_spc)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/SpcState.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/spc_timers.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/spc_saturation.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/spc_counters.cmake")
add_library(starfox_3ds_spc STATIC
    "${STARFOX_SPC_SOURCE}/spc.cpp" "${STARFOX_3DS_SPC_TIMERS_SOURCE}"
    "${STARFOX_SPC_SOURCE}/SNES_SPC_misc.cpp" "${STARFOX_SPC_SOURCE}/SNES_SPC_state.cpp"
    "${STARFOX_3DS_SPC_DSP_SOURCE}" "${STARFOX_SPC_SOURCE}/SPC_Filter.cpp")
target_include_directories(starfox_3ds_spc SYSTEM PUBLIC "${STARFOX_SPC_SOURCE}")
target_include_directories(starfox_3ds_spc PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")
add_library(starfox_3ds_cpu STATIC "${retro_cpu_SOURCE_DIR}/cpu.cc"
    "${retro_cpu_SOURCE_DIR}/cpu/65816/cpu_65c816.cc")
# Compile the pinned CPU and its paged memory bus together so operand accesses
# can inline without whole-program LTO. No opcode, IO ordering or cycle changes;
# the independent pre-unity state/bus trace guards the resulting code.
set_target_properties(starfox_3ds_cpu PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 2)
target_include_directories(starfox_3ds_cpu SYSTEM PUBLIC "${retro_cpu_SOURCE_DIR}")
target_compile_features(starfox_3ds_cpu PUBLIC cxx_std_17)
if(MINGW)
    target_compile_definitions(starfox_3ds_cpu PUBLIC __FUNCSIG__=__PRETTY_FUNCTION__)
endif()
set(cartridge_root "${CMAKE_CURRENT_LIST_DIR}/../..")
set(cartridge_sources
    src/audio/spc700_audio.cpp
    src/assets/bps.cpp src/assets/decrunch.cpp src/assets/runtime_bundle.cpp src/assets/shape_decoder.cpp
    src/render/framebuffer.cpp src/render/palette.cpp src/render/scaled_text_renderer.cpp
    src/render/dust_renderer.cpp
    src/render/pixel_filter.cpp src/render/scalefx.cpp src/render/row_workers.cpp
    src/simulation/dust_system.cpp src/simulation/game_simulation.cpp src/simulation/game_state.cpp
    src/simulation/map_vm.cpp src/simulation/object_pool.cpp src/simulation/state_container.cpp
    src/simulation/state_files.cpp src/simulation/particle_system.cpp src/simulation/path_vm.cpp
    src/simulation/prng.cpp src/simulation/strategy_scheduler.cpp src/simulation/wdc65816.cpp
    src/timing/fixed_step.cpp src/vr/game_scene.cpp src/vr/source_pose_interpolation.cpp)
list(TRANSFORM cartridge_sources PREPEND "${cartridge_root}/")
add_library(starfox_3ds_cartridge_core STATIC ${cartridge_sources}
    game_assets.cpp game_hud.cpp game_session.cpp game_session_state.cpp game_state.cpp
    game_models.cpp game_dots.cpp game_layers.cpp game_scenery.cpp game_menu.cpp game_storage.cpp game_state_storage.cpp)
target_include_directories(starfox_3ds_cartridge_core PUBLIC "${cartridge_root}/include")
target_compile_features(starfox_3ds_cartridge_core PUBLIC cxx_std_20)
target_compile_definitions(starfox_3ds_cartridge_core PUBLIC STARFOX_SCENE_SOURCE_ONLY=1)
target_link_libraries(starfox_3ds_cartridge_core PUBLIC starfox_3ds_frontend
    starfox_3ds_source_geometry starfox_3ds_raster PRIVATE starfox_3ds_cpu starfox_3ds_spc)
# Section GC is important for the old console: linked diagnostic bring-up must
# not retain unused desktop pixel filters just because their capability helper
# lives in the same object. Unsupported runtime options are a separate host
# policy; this build does not claim those effects work on PICA.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(starfox_3ds_cartridge_core PRIVATE -ffunction-sections -fdata-sections)
endif()
