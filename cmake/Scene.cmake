# Graphics-API-free scene data shared by renderers: decoded meshes, BSP order,
# draw packets, interpolation and source sprites/backgrounds/cockpit. No
# Vulkan, OpenXR or SDL headers may be reachable from these sources.
add_library(starfox_scene STATIC
    src/vr/shape_mesh.cpp src/vr/shape_bsp.cpp
    src/vr/shape_batch.cpp src/render/face_material.cpp
    src/vr/scene_camera.cpp src/vr/scene_material.cpp src/vr/game_model_pose.cpp
    src/vr/game_scene.cpp src/vr/draw_packet.cpp src/vr/source_models.cpp
    src/vr/source_span_model.cpp src/vr/scene_interpolation.cpp src/vr/cockpit.cpp
    src/vr/background_tiles.cpp src/vr/source_sprites.cpp src/vr/frame_layers.cpp
    src/vr/scene_packet_validation.cpp src/vr/modern_frame.cpp src/vr/ray_lighting.cpp)
target_include_directories(starfox_scene PUBLIC include)
target_compile_features(starfox_scene PUBLIC cxx_std_20)
target_link_libraries(starfox_scene PUBLIC starfox_core)
if(STARFOX_BUILD_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME starfox_scene_api_free
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/check_scene_api_free.py")
    set_tests_properties(starfox_scene_api_free PROPERTIES LABELS "vr;scene" TIMEOUT 30)
endif()
