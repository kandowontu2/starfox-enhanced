# The ordinary PC executable owns these optional integration components. No
# runtime DLL or proprietary SR library is linked; normal launches do not invoke
# them. Release packaging supplies upstream installers separately and never
# runs them automatically (tools/package_displayxr.ps1).
FetchContent_Declare(starfox_displayxr_headers
    URL https://codeload.github.com/KhronosGroup/OpenXR-SDK/tar.gz/f2448a8797c85814aa892efc1ab8707900fbcc78
    URL_HASH SHA256=1681e0f6c9633c73117c2fc376d1fe71cbd6fdd2aeb896ed5854ff022aa82eaa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _starfox_headers_only)
FetchContent_MakeAvailable(starfox_displayxr_headers)
add_library(starfox_displayxr STATIC src/render/displayxr_runtime.cpp
    src/render/displayxr_session.cpp src/render/displayxr_d3d12_binding.cpp src/render/displayxr_vulkan_binding.cpp
    src/render/displayxr_d3d12_presenter.cpp src/render/gpu_calibrated_scene.cpp src/render/gpu_calibrated_ray_composite.cpp
    src/render/calibrated_game_scene.cpp src/render/displayxr_game_renderer.cpp src/render/displayxr_desktop.cpp
    src/render/gpu_calibrated_persistence.cpp src/render/gpu_calibrated_global.cpp src/render/gpu_calibrated_bloom.cpp src/render/gpu_calibrated_exposure.cpp
    src/render/gpu_calibrated_depth.cpp src/render/gpu_calibrated_aa.cpp src/render/gpu_calibrated_msaa.cpp
    src/render/gpu_calibrated_temporal_aa.cpp src/render/gpu_calibrated_reflection_history.cpp src/render/gpu_calibrated_scene_fx.cpp src/render/gpu_calibrated_volumetric.cpp
    src/render/gpu_calibrated_motion_blur.cpp src/render/gpu_calibrated_fsr1.cpp src/render/gpu_calibrated_dlss.cpp
    src/vr/game_scene.cpp src/vr/source_models.cpp
    src/vr/scene_interpolation.cpp src/vr/source_span_model.cpp src/vr/openxr_swapchains.cpp
    src/vr/openxr_session.cpp src/vr/eye_camera.cpp
    src/vr/game_model_pose.cpp src/vr/draw_packet.cpp src/vr/shape_mesh.cpp
    src/vr/shape_bsp.cpp src/vr/shape_batch.cpp src/vr/scene_material.cpp
    src/vr/scene_packet_validation.cpp src/vr/background_tiles.cpp src/vr/source_sprites.cpp)
target_include_directories(starfox_displayxr PUBLIC include)
target_include_directories(starfox_displayxr PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
target_include_directories(starfox_displayxr SYSTEM PUBLIC "${starfox_displayxr_headers_SOURCE_DIR}/include")
target_compile_features(starfox_displayxr PUBLIC cxx_std_20)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_reflection_lobes.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_reflection_lobes.py" --paths --check
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_calibrated_scene.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_calibrated_scene_fx.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_calibrated_volumetric.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_calibrated_motion_blur.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_calibrated_dlss.py" --check
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "src/render/shaders/calibrated_scene_portable.hlsl"
    "src/render/shaders/calibrated_colour.hlsli"
    "src/render/shaders/calibrated_materials.hlsli"
    "src/render/shaders/calibrated_post_effects.hlsli"
    "src/render/shaders/calibrated_persistence.hlsl"
    "src/render/shaders/calibrated_global.hlsl"
    "src/render/shaders/calibrated_bloom.hlsl"
    "src/render/shaders/calibrated_exposure.hlsl"
    "src/render/shaders/calibrated_depth.hlsl"
    "src/render/shaders/calibrated_water_guides.hlsl"
    "src/render/shaders/calibrated_water_surface.hlsli"
    "src/render/shaders/calibrated_aa.hlsl" "third_party/smaa/SMAA.hlsl"
    "src/render/shaders/calibrated_msaa.hlsl"
    "src/render/shaders/calibrated_temporal_aa.hlsl"
    "src/render/shaders/calibrated_reflection_history.hlsl"
    "src/render/shaders/calibrated_scene_fx.hlsl" "src/render/shaders/generated/calibrated_scene_fx.hpp"
    "src/render/shaders/calibrated_volumetric.hlsl" "src/render/shaders/generated/calibrated_volumetric.hpp"
    "src/render/shaders/calibrated_motion_blur.hlsl" "src/render/shaders/generated/calibrated_motion_blur.hpp"
    "src/render/shaders/calibrated_dlss.hlsl" "src/render/shaders/generated/calibrated_dlss.hpp"
    "include/starfox/render/scene_enhancements.inc"
    "include/starfox/render/ambient_occlusion.inc"
    "include/starfox/render/depth_enhancements.inc"
    "include/starfox/render/global_enhancements.inc"
    "include/starfox/render/special_fx.inc"
    "src/render/shaders/calibrated_connected_grid.hlsl"
    "src/render/shaders/calibrated_ray_geometry.hlsl"
    "src/render/shaders/calibrated_ray_composite.hlsl"
    "src/render/shaders/generated/calibrated_scene_portable.hpp"
    "src/vr/shaders/scene.hlsl" "src/vr/shaders/scene_geometry.hlsli" "src/vr/shaders/scene_colour.hlsli" "src/vr/shaders/connected_grid.hlsli"
    "include/starfox/render/ex_face_planet_regions.inc")
target_link_libraries(starfox_displayxr PRIVATE SDL3::SDL3 starfox_core)
if(STARFOX_DXC)
    # Resident index/mapping/support/optical companions, opt-in staging only.
    # Continuum exclusion is NOT root uniqueness or old-colour acceptance.
    foreach(source_stage IN ITEMS tiles reduce query mapping domains frames optical optical_stream optical_schedule jets jets_stream roots_clear witness folds guide colour compose publish publish_diagnostics admission_clear)
    foreach(source_format IN ITEMS dxil spirv)
        set(source_flags -Gis -O3)
        set(source_extra_dependencies)
        if(source_stage STREQUAL "tiles")
            list(APPEND source_flags -DINDEX_BUILD=1)
        elseif(source_stage STREQUAL "reduce")
            list(APPEND source_flags -DINDEX_REDUCE=1)
        endif()
        if(source_stage STREQUAL "optical" OR source_stage STREQUAL "optical_stream")
            list(APPEND source_flags -DSTARFOX_OPTICAL_SCALAR_INLINE=1 -DSTARFOX_NATIVE_SOURCE_OPTICAL=1)
            list(APPEND source_extra_dependencies src/render/shaders/reflection_source_optical_target.hlsli)
            if(source_stage STREQUAL "optical_stream")
                list(APPEND source_extra_dependencies src/render/shaders/reflection_source_optical.hlsl)
            endif()
        elseif(source_stage STREQUAL "jets" OR source_stage STREQUAL "jets_stream")
            list(APPEND source_flags -DSTARFOX_OPTICAL_SCALAR_INLINE=1)
            list(APPEND source_extra_dependencies src/render/shaders/reflection_source_optical_target.hlsli
                src/render/shaders/reflection_source_optical_local_root.hlsli src/render/shaders/reflection_source_optical_jet.hlsli
                src/render/shaders/reflection_source_jets.hlsl)
        elseif(source_stage STREQUAL "witness")
            list(APPEND source_flags -DSTARFOX_OPTICAL_SCALAR_INLINE=1)
            list(APPEND source_extra_dependencies src/render/shaders/reflection_source_optical_target.hlsli)
        elseif(source_stage STREQUAL "folds" OR source_stage STREQUAL "guide")
            list(APPEND source_flags -DSTARFOX_OPTICAL_SCALAR_INLINE=1)
            list(APPEND source_extra_dependencies src/render/shaders/reflection_source_witness.hlsl
                src/render/shaders/reflection_source_optical_target.hlsli src/render/shaders/reflection_curved_path_motion.hlsli)
        elseif(source_stage STREQUAL "colour" OR source_stage STREQUAL "compose")
            list(APPEND source_extra_dependencies src/render/shaders/calibrated_colour.hlsli)
        elseif(source_stage STREQUAL "publish_diagnostics")
            list(APPEND source_extra_dependencies src/render/shaders/reflection_source_publish.hlsl)
        endif()
        if(source_format STREQUAL "spirv")
            list(APPEND source_flags -spirv -fspv-target-env=vulkan1.2)
            if(source_stage STREQUAL "jets" OR source_stage STREQUAL "jets_stream")
                # Strict storage/SSA cleanup: the complete shared differential
                # suite passes NVIDIA and Intel Vulkan. Native owner frames
                # still require their separate execution checks; staging is
                # opt-in and this is not a colour-history/FPS qualification.
                list(REMOVE_ITEM source_flags -O3)
                list(APPEND source_flags -Oconfig=--inline-entry-points-exhaustive,--eliminate-dead-functions,--private-to-local,--scalar-replacement=100,--convert-local-access-chains,--eliminate-local-single-block,--eliminate-local-single-store,--ssa-rewrite,--ccp,--eliminate-dead-branches,--eliminate-dead-code-aggressive,--simplify-instructions,--eliminate-dead-code-aggressive,--compact-ids)
            elseif(source_stage STREQUAL "optical" OR source_stage STREQUAL "optical_stream")
                list(REMOVE_ITEM source_flags -O3)
                list(APPEND source_flags -Oconfig=--inline-entry-points-exhaustive,--eliminate-dead-functions,--ccp,--eliminate-dead-branches,--eliminate-dead-code-aggressive,--simplify-instructions,--eliminate-dead-code-aggressive,--compact-ids)
            elseif(source_stage STREQUAL "folds")
                # Preserve bounded shared helpers instead of cloning the full
                # precise wave/quarter-cell program at each caller. DXIL keeps
                # its validated scalarized inline form; all math gates stay.
                list(APPEND source_flags -DSTARFOX_FOLD_HELPER_NOINLINE=1)
            endif()
        endif()
        set(source_name "native_reflection_index_${source_stage}_${source_format}")
        set(source_file src/render/shaders/reflection_source_index.hlsl)
        if(source_stage STREQUAL "mapping")
            set(source_file src/render/shaders/reflection_source_queries.hlsl)
        elseif(source_stage STREQUAL "domains")
            set(source_file src/render/shaders/reflection_source_domains.hlsl)
        elseif(source_stage STREQUAL "frames")
            set(source_file src/render/shaders/reflection_source_frames.hlsl)
        elseif(source_stage STREQUAL "optical")
            set(source_file src/render/shaders/reflection_source_optical.hlsl)
        elseif(source_stage STREQUAL "optical_stream")
            set(source_file src/render/shaders/reflection_source_optical_stream.hlsl)
        elseif(source_stage STREQUAL "optical_schedule")
            set(source_file src/render/shaders/reflection_source_optical_schedule.hlsl)
        elseif(source_stage STREQUAL "jets")
            set(source_file src/render/shaders/reflection_source_jets.hlsl)
        elseif(source_stage STREQUAL "jets_stream")
            set(source_file src/render/shaders/reflection_source_jets_stream.hlsl)
        elseif(source_stage STREQUAL "roots_clear")
            set(source_file src/render/shaders/reflection_source_roots_clear.hlsl)
        elseif(source_stage STREQUAL "witness")
            set(source_file src/render/shaders/reflection_source_witness.hlsl)
        elseif(source_stage STREQUAL "folds")
            set(source_file src/render/shaders/reflection_source_folds.hlsl)
        elseif(source_stage STREQUAL "guide")
            set(source_file src/render/shaders/reflection_source_guide.hlsl)
        elseif(source_stage STREQUAL "colour")
            set(source_file src/render/shaders/reflection_source_colour.hlsl)
        elseif(source_stage STREQUAL "compose")
            set(source_file src/render/shaders/reflection_source_compose.hlsl)
        elseif(source_stage STREQUAL "publish")
            set(source_file src/render/shaders/reflection_source_publish.hlsl)
        elseif(source_stage STREQUAL "publish_diagnostics")
            set(source_file src/render/shaders/reflection_source_publish_diagnostics.hlsl)
        elseif(source_stage STREQUAL "admission_clear")
            set(source_file src/render/shaders/reflection_source_admission_clear.hlsl)
        endif()
        set(source_binary "${CMAKE_CURRENT_BINARY_DIR}/generated/${source_name}.bin")
        set(source_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${source_name}.hpp")
        add_custom_command(OUTPUT "${source_header}" BYPRODUCTS "${source_binary}"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E "feature_${source_stage}_main" -WX ${source_flags}
                -Fo "${source_binary}" "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}"
            COMMAND "${CMAKE_COMMAND}" "-DINPUT=${source_binary}" "-DOUTPUT=${source_header}"
                "-DVARIABLE=${source_name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderBinary.cmake"
            DEPENDS cmake/EmbedShaderBinary.cmake "${source_file}" ${source_extra_dependencies} src/render/shaders/reflection_source_index.hlsl
                src/render/shaders/reflection_source_index_settings.hlsli src/render/shaders/reflection_source_feature.hlsli
                src/render/shaders/reflection_source_optical_interval.hlsli src/render/shaders/reflection_source_optical_reciprocals.inc
                src/render/shaders/reflection_specular_path_motion.hlsli src/render/shaders/reflection_rough_hit_motion.hlsli
                src/render/shaders/reflection_liquid_hit_motion.hlsli src/render/shaders/liquid_optics.hlsli
                src/render/shaders/reflection_curved_precision.hlsli VERBATIM)
        target_sources(starfox_displayxr PRIVATE "${source_header}")
    endforeach()
    endforeach()
    target_include_directories(starfox_displayxr PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(starfox_displayxr PRIVATE STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1)
    foreach(capture_format IN ITEMS dxil spirv)
        set(capture_flags -Gis -O3)
        if(capture_format STREQUAL "spirv")
            list(APPEND capture_flags -spirv -fspv-target-env=vulkan1.2)
        endif()
        set(capture_name "native_reflection_capture_${capture_format}")
        set(capture_binary "${CMAKE_CURRENT_BINARY_DIR}/generated/${capture_name}.bin")
        set(capture_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${capture_name}.hpp")
        add_custom_command(OUTPUT "${capture_header}" BYPRODUCTS "${capture_binary}"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E reflection_path_capture_main -WX ${capture_flags}
                -Fo "${capture_binary}" "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/reflection_path_capture.hlsl"
            COMMAND "${CMAKE_COMMAND}" "-DINPUT=${capture_binary}" "-DOUTPUT=${capture_header}"
                "-DVARIABLE=${capture_name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderBinary.cmake"
            DEPENDS cmake/EmbedShaderBinary.cmake src/render/shaders/reflection_path_capture.hlsl
                src/render/shaders/calibrated_ray_history.hlsli VERBATIM)
        target_sources(starfox_displayxr PRIVATE "${capture_header}")
    endforeach()
endif()
if(MINGW AND EXISTS "${sdl3_SOURCE_DIR}/src/video/directx/d3d12.h")
    target_include_directories(starfox_displayxr BEFORE PRIVATE "${sdl3_SOURCE_DIR}/src/video/directx")
endif()
if(STARFOX_BUILD_TOOLS AND STARFOX_DXR_ENABLED)
    add_executable(starfox_reflection_source_owner_check tools/check_reflection_source_owner.cpp)
    target_link_libraries(starfox_reflection_source_owner_check PRIVATE starfox_displayxr SDL3::SDL3)
    target_include_directories(starfox_reflection_source_owner_check PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
    if(STARFOX_DXC)
        target_include_directories(starfox_reflection_source_owner_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_reflection_source_owner_check PRIVATE STARFOX_SOURCE_SCHEDULE_CHECK_AVAILABLE=1)
    endif()
    if(MINGW)
        target_include_directories(starfox_reflection_source_owner_check BEFORE PRIVATE "${sdl3_SOURCE_DIR}/src/video/directx")
        target_link_options(starfox_reflection_source_owner_check PRIVATE -static -static-libgcc -static-libstdc++)
        # Same bounded native Intel interval-compiler HOST reservation as the
        # standalone optical diagnostic. Never changes a player executable.
        target_link_options(starfox_reflection_source_owner_check PRIVATE "-Wl,--stack,16777216")
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(starfox_reflection_source_owner_check PRIVATE -Wall -Wextra -Wconversion -Werror)
    endif()
    # Resident conservative old-feature index, diagnostic staging only. These
    # pipelines are not linked to a player or permission to reuse curved colour.
    set(visibility_index_headers)
    set(visibility_nonjet_headers)
    set(visibility_arithmetic_headers)
    foreach(index_stage IN ITEMS tiles reduce query domains optical jets arithmetic)
    foreach(index_format IN ITEMS dxil spirv)
        set(index_flags -Gis -O3)
        if(index_stage STREQUAL "tiles")
            list(APPEND index_flags -DINDEX_BUILD=1)
        elseif(index_stage STREQUAL "reduce")
            list(APPEND index_flags -DINDEX_REDUCE=1)
        endif()
        if(index_stage STREQUAL "optical" OR index_stage STREQUAL "jets" OR index_stage STREQUAL "arithmetic")
            # Native Intel drivers fault on the preserved-call variants. Keep
            # checked arithmetic but inline bounds before native compilation.
            # Unified stage/wave loops and prechecked fixed reciprocals keep
            # this diagnostic small enough for DXC's normal optimizer.
            list(APPEND index_flags -DSTARFOX_OPTICAL_SCALAR_INLINE=1)
        endif()
        if(index_format STREQUAL "spirv")
            list(APPEND index_flags -spirv -fspv-target-env=vulkan1.2)
            if(index_stage STREQUAL "jets")
                # Same strict recipe verified against the complete standalone
                # physical/derivative/local-root suite on both Vulkan devices.
                list(REMOVE_ITEM index_flags -O3)
                list(APPEND index_flags -Oconfig=--inline-entry-points-exhaustive,--eliminate-dead-functions,--private-to-local,--scalar-replacement=100,--convert-local-access-chains,--eliminate-local-single-block,--eliminate-local-single-store,--ssa-rewrite,--ccp,--eliminate-dead-branches,--eliminate-dead-code-aggressive,--simplify-instructions,--eliminate-dead-code-aggressive,--compact-ids)
            elseif(index_stage STREQUAL "optical" OR index_stage STREQUAL "arithmetic")
                # Flatten the checked interval call graph before the native
                # compiler, then prune constants/dead code. Avoid DXC's costly
                # default exhaustive optimization cycle; no relaxed-math pass.
                list(REMOVE_ITEM index_flags -O3)
                list(APPEND index_flags -Oconfig=--inline-entry-points-exhaustive,--eliminate-dead-functions,--ccp,--eliminate-dead-branches,--eliminate-dead-code-aggressive,--simplify-instructions,--eliminate-dead-code-aggressive,--compact-ids)
            endif()
        endif()
        set(index_name "reflected_visibility_${index_stage}_${index_format}")
        set(index_source tools/shaders/check_reflected_visibility_index.hlsl)
        if(index_stage STREQUAL "domains")
            set(index_source tools/shaders/check_reflected_visibility_domains.hlsl)
        elseif(index_stage STREQUAL "optical")
            set(index_source tools/shaders/check_reflected_visibility_optical.hlsl)
        elseif(index_stage STREQUAL "jets")
            set(index_source tools/shaders/check_reflected_visibility_jets.hlsl)
        elseif(index_stage STREQUAL "arithmetic")
            set(index_source tools/shaders/check_reflected_visibility_arithmetic.hlsl)
        endif()
        set(index_extra_dependencies)
        if(index_stage STREQUAL "jets" OR index_stage STREQUAL "arithmetic")
            list(APPEND index_extra_dependencies src/render/shaders/reflection_source_optical_jet.hlsli
                src/render/shaders/reflection_source_optical_local_root.hlsli)
        endif()
        set(index_binary "${CMAKE_CURRENT_BINARY_DIR}/generated/${index_name}.bin")
        set(index_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${index_name}.hpp")
        add_custom_command(OUTPUT "${index_header}" BYPRODUCTS "${index_binary}"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E "feature_${index_stage}_main" -WX ${index_flags}
                -Fo "${index_binary}" "${CMAKE_CURRENT_SOURCE_DIR}/${index_source}"
            COMMAND "${CMAKE_COMMAND}" "-DINPUT=${index_binary}" "-DOUTPUT=${index_header}"
                "-DVARIABLE=${index_name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderBinary.cmake"
            DEPENDS cmake/EmbedShaderBinary.cmake "${index_source}" ${index_extra_dependencies} tools/shaders/reflected_visibility_settings.hlsli
                src/render/shaders/reflection_source_index.hlsl src/render/shaders/reflection_source_index_settings.hlsli
                src/render/shaders/reflection_source_feature.hlsli src/render/shaders/reflection_source_domains.hlsl
                src/render/shaders/reflection_source_optical.hlsl src/render/shaders/reflection_source_optical_interval.hlsli
                src/render/shaders/reflection_source_optical_reciprocals.inc
                tools/shaders/reflected_optical_interval.hlsli
                tools/shaders/reflected_optical_reciprocals.inc
                src/render/shaders/reflection_specular_path_motion.hlsli
                src/render/shaders/reflection_liquid_hit_motion.hlsli
                src/render/shaders/reflection_curved_precision.hlsli VERBATIM)
        if(index_stage STREQUAL "arithmetic")
            list(APPEND visibility_arithmetic_headers "${index_header}")
        else()
            list(APPEND visibility_index_headers "${index_header}")
            if(NOT index_stage STREQUAL "jets")
                list(APPEND visibility_nonjet_headers "${index_header}")
            endif()
        endif()
    endforeach()
    endforeach()
    foreach(index_check IN ITEMS starfox_reflected_visibility_index_check starfox_reflected_optical_jet_arithmetic_check)
        if(index_check STREQUAL "starfox_reflected_optical_jet_arithmetic_check")
            add_executable(${index_check} tools/check_reflected_visibility_index.cpp ${visibility_nonjet_headers} ${visibility_arithmetic_headers})
            target_compile_definitions(${index_check} PRIVATE STARFOX_OPTICAL_ARITHMETIC_TRACE_ONLY=1)
        else()
            add_executable(${index_check} tools/check_reflected_visibility_index.cpp ${visibility_index_headers})
        endif()
        target_include_directories(${index_check} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
            "${sdl3_SOURCE_DIR}/src/video/khronos")
        target_compile_features(${index_check} PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${index_check} PRIVATE -Wall -Wextra -Wconversion -Werror)
        endif()
        target_link_libraries(${index_check} PRIVATE SDL3::SDL3)
        if(MINGW)
            target_link_options(${index_check} PRIVATE -static -static-libgcc -static-libstdc++)
            target_include_directories(${index_check} BEFORE PRIVATE "${sdl3_SOURCE_DIR}/src/video/directx")
        endif()
        # Intel's native Vulkan compiler overflows the MinGW 2 MiB default
        # HOST stack on this interval diagnostic. The same source/payloads pass
        # with 16 MiB reserved, still only 4 KiB initially committed. This does
        # not alter a shader quota or ordinary player executable.
        if(MINGW)
            target_link_options(${index_check} PRIVATE "-Wl,--stack,16777216")
        elseif(MSVC)
            target_link_options(${index_check} PRIVATE /STACK:16777216)
        endif()
    endforeach()
    # Small actual-driver scalar diagnosis before spending minutes compiling
    # the complete curved inverse. It includes the exact shared shader header.
    set(curved_scalar_headers)
    foreach(scalar_format IN ITEMS dxil spirv)
        set(scalar_flags)
        set(scalar_optimization -O3)
        if(scalar_format STREQUAL "spirv")
            # Whole-program IEEE strictness includes preservation of signed
            # zero/Inf/NaN. NoContraction on the scalar helper alone is not
            # sufficient for the nested quotient residual on tested Vulkan.
            # The diagnostic queries the required binary32 capability first.
            set(scalar_flags -spirv -fspv-target-env=vulkan1.2 -Gis)
            set(scalar_optimization "-Oconfig=--simplify-instructions,--eliminate-dead-code-aggressive,--eliminate-dead-functions,--compact-ids")
        endif()
        set(scalar_name "reflected_curved_scalar_${scalar_format}")
        set(scalar_binary "${CMAKE_CURRENT_BINARY_DIR}/generated/${scalar_name}.bin")
        set(scalar_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${scalar_name}.hpp")
        add_custom_command(OUTPUT "${scalar_header}" BYPRODUCTS "${scalar_binary}"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E main ${scalar_optimization} -WX ${scalar_flags}
                -DSTARFOX_CURVED_TRIG_TABLE=1 -Fo "${scalar_binary}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/shaders/check_reflected_curved_scalar.hlsl"
            COMMAND "${CMAKE_COMMAND}" "-DINPUT=${scalar_binary}" "-DOUTPUT=${scalar_header}"
                "-DVARIABLE=${scalar_name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderBinary.cmake"
            DEPENDS cmake/EmbedShaderBinary.cmake tools/shaders/check_reflected_curved_scalar.hlsl
                src/render/shaders/reflection_curved_precision.hlsli VERBATIM)
        list(APPEND curved_scalar_headers "${scalar_header}")
    endforeach()
    add_executable(starfox_reflected_curved_scalar_gpu_check tools/check_reflected_curved_scalar_gpu.cpp ${curved_scalar_headers})
    target_include_directories(starfox_reflected_curved_scalar_gpu_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_include_directories(starfox_reflected_curved_scalar_gpu_check PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
    target_compile_features(starfox_reflected_curved_scalar_gpu_check PRIVATE cxx_std_20)
    target_link_libraries(starfox_reflected_curved_scalar_gpu_check PRIVATE SDL3::SDL3)
    if(MINGW)
        target_link_options(starfox_reflected_curved_scalar_gpu_check PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    # Staged genuine curved mixed-path optics. Not enabled cached radiance:
    # retain the old full-path guards until native producer/visibility acceptance.
    set(curved_shader_headers)
    foreach(curved_phase IN ITEMS guide witness)
    foreach(curved_format IN ITEMS dxil spirv)
        set(curved_flags)
        set(curved_witness 0)
        if(curved_phase STREQUAL "witness")
            set(curved_witness 1)
        endif()
        set(curved_name "reflected_curved_path_${curved_phase}_${curved_format}")
        set(curved_binary "${CMAKE_CURRENT_BINARY_DIR}/generated/${curved_name}.bin")
        set(curved_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${curved_name}.hpp")
        set(curved_optimization -O3)
        if(curved_format STREQUAL "spirv")
            set(curved_flags -spirv -fspv-target-env=vulkan1.2 -Gis)
            # Default exhaustive inlining expands the shared precision kernel
            # enormously. Preserve its bounded loops/functions and optimize
            # dead code/IDs explicitly; arithmetic and strict oracle are intact.
            set(curved_optimization "-Oconfig=--simplify-instructions,--eliminate-dead-code-aggressive,--eliminate-dead-functions,--compact-ids")
        endif()
        add_custom_command(OUTPUT "${curved_header}" BYPRODUCTS "${curved_binary}"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E main ${curved_optimization} -WX ${curved_flags}
                "-DSTARFOX_CURVED_WITNESS_PHASE=${curved_witness}" -DSTARFOX_CURVED_TRIG_TABLE=1 -Fo "${curved_binary}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/shaders/check_reflected_curved_path.hlsl"
            COMMAND "${CMAKE_COMMAND}" "-DINPUT=${curved_binary}" "-DOUTPUT=${curved_header}"
                "-DVARIABLE=${curved_name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderBinary.cmake"
            DEPENDS cmake/EmbedShaderBinary.cmake tools/shaders/check_reflected_curved_path.hlsl
                src/render/shaders/reflection_curved_path_motion.hlsli
                src/render/shaders/reflection_curved_precision.hlsli
                src/render/shaders/reflection_specular_path_motion.hlsli
                src/render/shaders/reflection_rough_hit_motion.hlsli
                src/render/shaders/reflection_liquid_hit_motion.hlsli
                src/render/shaders/liquid_optics.hlsli
                include/starfox/render/lava_surface.inc include/starfox/render/water_caustics.inc VERBATIM)
        list(APPEND curved_shader_headers "${curved_header}")
    endforeach()
    endforeach()
    add_executable(starfox_reflected_curved_path_check tools/check_reflected_curved_path.cpp
        ${curved_shader_headers})
    target_include_directories(starfox_reflected_curved_path_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_include_directories(starfox_reflected_curved_path_check PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
    target_compile_features(starfox_reflected_curved_path_check PRIVATE cxx_std_20)
    target_link_libraries(starfox_reflected_curved_path_check PRIVATE SDL3::SDL3)
    if(MINGW)
        target_link_options(starfox_reflected_curved_path_check PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    # Full ordered planar path optics, including infinite terminal directions.
    # This is a staged optical check, not enabled multibounce colour history.
    foreach(path_format IN ITEMS dxil spirv)
        set(path_flags)
        if(path_format STREQUAL "spirv")
            set(path_flags -spirv -fspv-target-env=vulkan1.1)
        endif()
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_specular_path_${path_format}.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E main -O3 -WX ${path_flags}
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_specular_path_${path_format}.hpp"
                -Vn "reflected_specular_path_${path_format}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/shaders/check_reflected_specular_path.hlsl"
            DEPENDS tools/shaders/check_reflected_specular_path.hlsl
                src/render/shaders/reflection_specular_path_motion.hlsli
                src/render/shaders/reflection_rough_hit_motion.hlsli VERBATIM)
    endforeach()
    add_executable(starfox_reflected_specular_path_check tools/check_reflected_specular_path.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_specular_path_dxil.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_specular_path_spirv.hpp")
    target_include_directories(starfox_reflected_specular_path_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_features(starfox_reflected_specular_path_check PRIVATE cxx_std_20)
    target_link_libraries(starfox_reflected_specular_path_check PRIVATE SDL3::SDL3)
    if(MINGW)
        target_link_options(starfox_reflected_specular_path_check PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    # Per-lobe rough reflection inverse optics, not enabled radiance history.
    # The independent forward oracle includes the real native origin bias.
    foreach(rough_format IN ITEMS dxil spirv)
        set(rough_flags)
        if(rough_format STREQUAL "spirv")
            set(rough_flags -spirv -fspv-target-env=vulkan1.1)
        endif()
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_rough_motion_${rough_format}.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E main -O3 -WX ${rough_flags}
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_rough_motion_${rough_format}.hpp"
                -Vn "reflected_rough_motion_${rough_format}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/shaders/check_reflected_rough_motion.hlsl"
            DEPENDS tools/shaders/check_reflected_rough_motion.hlsl src/render/shaders/reflection_rough_hit_motion.hlsli VERBATIM)
    endforeach()
    add_executable(starfox_reflected_rough_motion_check tools/check_reflected_rough_motion.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_rough_motion_dxil.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_rough_motion_spirv.hpp")
    target_include_directories(starfox_reflected_rough_motion_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_features(starfox_reflected_rough_motion_check PRIVATE cxx_std_20)
    target_link_libraries(starfox_reflected_rough_motion_check PRIVATE SDL3::SDL3)
    if(MINGW)
        target_link_options(starfox_reflected_rough_motion_check PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    # Standalone optical proof: no game, scene library or shipped EXE relink.
    # The runtime liquid owner shares these optics. Its bounded inverse-wave
    # solve runs separately from RT, without entering ordinary reflection PSOs.
    foreach(liquid_format IN ITEMS dxil spirv)
        set(liquid_flags)
        if(liquid_format STREQUAL "spirv")
            set(liquid_flags -spirv -fspv-target-env=vulkan1.1)
        endif()
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_liquid_motion_${liquid_format}.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_0 -E main -O3 ${liquid_flags}
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_liquid_motion_${liquid_format}.hpp"
                -Vn "reflected_liquid_motion_${liquid_format}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/shaders/check_reflected_liquid_motion.hlsl"
            DEPENDS tools/shaders/check_reflected_liquid_motion.hlsl src/render/shaders/reflection_liquid_hit_motion.hlsli
                src/render/shaders/liquid_optics.hlsli
                include/starfox/render/lava_surface.inc include/starfox/render/water_caustics.inc VERBATIM)
    endforeach()
    add_executable(starfox_reflected_liquid_motion_check tools/check_reflected_liquid_motion.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_liquid_motion_dxil.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/reflected_liquid_motion_spirv.hpp")
    target_include_directories(starfox_reflected_liquid_motion_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_features(starfox_reflected_liquid_motion_check PRIVATE cxx_std_20)
    target_link_libraries(starfox_reflected_liquid_motion_check PRIVATE SDL3::SDL3)
    if(MINGW)
        target_link_options(starfox_reflected_liquid_motion_check PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_executable(starfox_displayxr_vulkan_creation_check tools/check_displayxr_vulkan_creation.cpp)
    target_link_libraries(starfox_displayxr_vulkan_creation_check PRIVATE starfox_displayxr SDL3::SDL3)
    target_include_directories(starfox_displayxr_vulkan_creation_check PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
    add_executable(starfox_calibrated_scene_check tools/check_calibrated_scene.cpp)
    target_link_libraries(starfox_calibrated_scene_check PRIVATE starfox_displayxr SDL3::SDL3)
    add_executable(starfox_calibrated_ray_composite_check tools/check_calibrated_ray_composite.cpp)
    target_link_libraries(starfox_calibrated_ray_composite_check PRIVATE starfox_displayxr SDL3::SDL3)
    if(WIN32)
        # Separate diagnostic PSOs/output, never linked into the game owner.
        # Compare the actual tight color solve and unchanged geometric guide.
        foreach(trace_root IN ITEMS 0 1)
            set(trace_name "calibrated_curved_trace_${trace_root}_dxil")
            set(trace_binary "${CMAKE_CURRENT_BINARY_DIR}/generated/${trace_name}.bin")
            set(trace_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${trace_name}.hpp")
            add_custom_command(OUTPUT "${trace_header}" BYPRODUCTS "${trace_binary}"
                COMMAND "${STARFOX_DXC}" -T cs_6_0 -E reflection_curved_trace_main -O3 -WX -Gis
                    -DSTARFOX_DXR_CURVED_PATH_HISTORY=1 -DSTARFOX_CURVED_CACHE_TRACE=1
                    "-DSTARFOX_CURVED_COLOUR_ROOT=${trace_root}" -Fo "${trace_binary}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/calibrated_reflection_paths.hlsl"
                COMMAND "${CMAKE_COMMAND}" "-DINPUT=${trace_binary}" "-DOUTPUT=${trace_header}"
                    "-DVARIABLE=${trace_name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaderBinary.cmake"
                DEPENDS cmake/EmbedShaderBinary.cmake src/render/shaders/calibrated_reflection_paths.hlsl
                    src/render/shaders/reflection_curved_path_motion.hlsli src/render/shaders/reflection_curved_precision.hlsli
                    src/render/shaders/reflection_specular_path_motion.hlsli src/render/shaders/reflection_rough_hit_motion.hlsli
                    src/render/shaders/reflection_liquid_hit_motion.hlsli src/render/shaders/liquid_optics.hlsli
                    include/starfox/render/lava_surface.inc include/starfox/render/water_caustics.inc VERBATIM)
            target_sources(starfox_calibrated_ray_composite_check PRIVATE "${trace_header}")
        endforeach()
        target_include_directories(starfox_calibrated_ray_composite_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_calibrated_ray_composite_check PRIVATE STARFOX_CURVED_CACHE_TRACE_AVAILABLE=1)
    endif()
    add_executable(starfox_calibrated_scene_fx_check tools/check_calibrated_scene_fx.cpp)
    target_link_libraries(starfox_calibrated_scene_fx_check PRIVATE starfox_displayxr SDL3::SDL3)
    add_executable(starfox_calibrated_volumetric_check tools/check_calibrated_volumetric.cpp)
    target_link_libraries(starfox_calibrated_volumetric_check PRIVATE starfox_displayxr SDL3::SDL3)
    add_executable(starfox_calibrated_motion_blur_check tools/check_calibrated_motion_blur.cpp)
    target_link_libraries(starfox_calibrated_motion_blur_check PRIVATE starfox_displayxr SDL3::SDL3)
    add_executable(starfox_calibrated_dlss_check tools/check_calibrated_dlss.cpp)
    target_link_libraries(starfox_calibrated_dlss_check PRIVATE starfox_displayxr SDL3::SDL3)
    add_executable(starfox_calibrated_game_scene_check tools/check_calibrated_game_scene.cpp)
    target_link_libraries(starfox_calibrated_game_scene_check PRIVATE starfox_displayxr starfox_core SDL3::SDL3)
    add_executable(starfox_displayxr_gpu_presenter_check tests/displayxr_runtime_tests.cpp)
    target_link_libraries(starfox_displayxr_gpu_presenter_check PRIVATE starfox_displayxr SDL3::SDL3)
    target_compile_definitions(starfox_displayxr_gpu_presenter_check PRIVATE STARFOX_DISPLAYXR_GPU_FIXTURE=1)
    target_include_directories(starfox_displayxr_gpu_presenter_check PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
    if(WIN32)
        # Separate read-only refusal trace in the diagnostic executable only.
        # Neither trace shader nor its output is linked into a player owner.
        target_sources(starfox_displayxr_gpu_presenter_check PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/generated/calibrated_curved_trace_1_dxil.hpp")
        target_include_directories(starfox_displayxr_gpu_presenter_check PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_displayxr_gpu_presenter_check PRIVATE STARFOX_CURVED_OWNER_TRACE_AVAILABLE=1)
    endif()
    # The same live two-eye renderer, acceptance banks and XR presentation
    # fixture, with the Linux-style native Vulkan adapter instead of Win32 DXR
    # sharing. Target-local only; never changes a shipped Windows backend.
    add_executable(starfox_displayxr_native_vulkan_check tests/displayxr_runtime_tests.cpp
        src/render/sdl_dxr_shadows.cpp src/render/vulkan_hardware_rt.cpp src/render/vulkan_ray_support.cpp)
    target_link_libraries(starfox_displayxr_native_vulkan_check PRIVATE starfox_displayxr SDL3::SDL3)
    target_compile_definitions(starfox_displayxr_native_vulkan_check PRIVATE STARFOX_DISPLAYXR_GPU_FIXTURE=1
        STARFOX_SDL_GPU_EFFECTS STARFOX_NATIVE_VULKAN_OWNER_PROBE STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
    target_include_directories(starfox_displayxr_native_vulkan_check PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
    if(MINGW)
        target_include_directories(starfox_displayxr_gpu_presenter_check BEFORE PRIVATE
            "${sdl3_SOURCE_DIR}/src/video/directx")
        target_include_directories(starfox_displayxr_native_vulkan_check BEFORE PRIVATE
            "${sdl3_SOURCE_DIR}/src/video/directx")
        target_include_directories(starfox_calibrated_dlss_check BEFORE PRIVATE
            "${sdl3_SOURCE_DIR}/src/video/directx")
    endif()
endif()
if(TARGET starfox_sdl_xr_color_check)
    target_link_libraries(starfox_sdl_xr_color_check PRIVATE starfox_displayxr)
    target_compile_definitions(starfox_sdl_xr_color_check PRIVATE STARFOX_DISPLAYXR_BINDING=1)
endif()
target_link_libraries(starfox_displayxr PRIVATE advapi32)
target_link_libraries(starfox_pc PRIVATE starfox_displayxr)
target_compile_definitions(starfox_pc PRIVATE STARFOX_DISPLAYXR=1)
install(FILES third_party/displayxr/LICENSE.txt third_party/displayxr/NOTICE.txt
    DESTINATION licenses/displayxr COMPONENT DisplayXRNotices)
if(STARFOX_BUILD_TESTS)
    add_executable(starfox_curved_owner_source_check tools/check_curved_owner_source.cpp)
    target_compile_features(starfox_curved_owner_source_check PRIVATE cxx_std_20)
    if(MINGW)
        target_link_options(starfox_curved_owner_source_check PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_executable(starfox_reflected_curved_owner_oracle_tests tests/reflected_curved_owner_oracle_tests.cpp)
    target_compile_features(starfox_reflected_curved_owner_oracle_tests PRIVATE cxx_std_20)
    if(MINGW)
        target_link_options(starfox_reflected_curved_owner_oracle_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME starfox_reflected_curved_owner_oracle_tests COMMAND starfox_reflected_curved_owner_oracle_tests)
    set_tests_properties(starfox_reflected_curved_owner_oracle_tests PROPERTIES TIMEOUT 30)
    add_executable(starfox_calibrated_reflection_history_tests tests/calibrated_reflection_history_tests.cpp)
    target_link_libraries(starfox_calibrated_reflection_history_tests PRIVATE starfox_displayxr starfox_core)
    add_test(NAME starfox_calibrated_reflection_history_tests COMMAND starfox_calibrated_reflection_history_tests)
    set_tests_properties(starfox_calibrated_reflection_history_tests PROPERTIES TIMEOUT 30)
    add_executable(starfox_calibrated_scene_fx_tests tests/calibrated_scene_fx_tests.cpp)
    target_link_libraries(starfox_calibrated_scene_fx_tests PRIVATE starfox_displayxr starfox_core)
    add_test(NAME starfox_calibrated_scene_fx_tests COMMAND starfox_calibrated_scene_fx_tests)
    set_tests_properties(starfox_calibrated_scene_fx_tests PROPERTIES TIMEOUT 30)
    add_executable(starfox_displayxr_mesh_regression_tests tests/shape_mesh_tests.cpp)
    target_link_libraries(starfox_displayxr_mesh_regression_tests PRIVATE starfox_displayxr starfox_core)
    add_test(NAME starfox_displayxr_mesh_regression_tests COMMAND starfox_displayxr_mesh_regression_tests)
    set_tests_properties(starfox_displayxr_mesh_regression_tests PROPERTIES TIMEOUT 30)
    add_executable(starfox_displayxr_tests tests/displayxr_runtime_tests.cpp)
    target_link_libraries(starfox_displayxr_tests PRIVATE starfox_displayxr)
    add_test(NAME starfox_displayxr_tests COMMAND starfox_displayxr_tests)
    set_tests_properties(starfox_displayxr_tests PROPERTIES TIMEOUT 30)
    # Exercise the unchanged headset defaults after sharing the locate-chain
    # plumbing. Fully mocked APIs: no loader, installed runtime or HMD needed.
    add_executable(starfox_displayxr_headset_regression_tests tests/openxr_session_tests.cpp
        src/vr/stereo_renderer.cpp)
    target_link_libraries(starfox_displayxr_headset_regression_tests PRIVATE starfox_displayxr)
    add_test(NAME starfox_displayxr_headset_regression_tests COMMAND starfox_displayxr_headset_regression_tests)
    set_tests_properties(starfox_displayxr_headset_regression_tests PROPERTIES TIMEOUT 30)
endif()
