# Asset-free installer regression only; native ordering is tested separately.
cmake_minimum_required(VERSION 3.20)
if(NOT DEFINED SOURCE_DIR OR NOT DEFINED TEST_BINARY_DIR)
    message(FATAL_ERROR "SOURCE_DIR and TEST_BINARY_DIR are required")
endif()
set(patch "${SOURCE_DIR}/cmake/sdl3-d3d12-copy-ordering.cmake")
set(anchor [=[static void D3D12_INTERNAL_BufferTransitionToDefaultUsage(
    D3D12CommandBuffer *commandBuffer,
    D3D12_RESOURCE_STATES sourceState,
    D3D12Buffer *buffer)
{
    D3D12_INTERNAL_BufferBarrier(
        commandBuffer,
        sourceState,
        D3D12_INTERNAL_DefaultBufferResourceState(buffer),
        buffer);
}]=])
set(expected [=[static void D3D12_INTERNAL_BufferTransitionToDefaultUsage(
    D3D12CommandBuffer *commandBuffer,
    D3D12_RESOURCE_STATES sourceState,
    D3D12Buffer *buffer)
{
    const D3D12_RESOURCE_STATES defaultState = D3D12_INTERNAL_DefaultBufferResourceState(buffer);
    /* Star Fox copy-read to future UAV write ordering v1 */
    /* Returning a copy source through another read-only state can lose its
     * copy-read dependency before a following compute write on some drivers.
     * Complete that read-to-write transition explicitly, then restore the
     * same public default state. No CPU wait, extra submission or copy. */
    if (sourceState == D3D12_RESOURCE_STATE_COPY_SOURCE
        && (buffer->container->usage & SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE)
        && defaultState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_INTERNAL_BufferBarrier(commandBuffer, sourceState,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, buffer);
        sourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    D3D12_INTERNAL_BufferBarrier(commandBuffer, sourceState, defaultState, buffer);
}]=])
set(marker "/* Star Fox copy-read to future UAV write ordering v1 */")
set(prefix "// unrelated prefix: do not change\n")
set(suffix "\n// unrelated suffix: do not change\n")

function(run_patch case_name input should_pass wanted)
    set(root "${TEST_BINARY_DIR}/${case_name}")
    set(backend "${root}/src/gpu/d3d12/SDL_gpu_d3d12.c")
    file(MAKE_DIRECTORY "${root}/src/gpu/d3d12")
    file(WRITE "${backend}" "${input}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${root}" -P "${patch}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(should_pass AND NOT result EQUAL 0)
        message(FATAL_ERROR "${case_name}: installer failed: ${output}${error}")
    elseif(NOT should_pass AND result EQUAL 0)
        message(FATAL_ERROR "${case_name}: installer accepted an invalid backend")
    endif()
    file(READ "${backend}" actual)
    if(NOT actual STREQUAL wanted)
        message(FATAL_ERROR "${case_name}: unexpected backend mutation")
    endif()
    if(NOT should_pass AND NOT error MATCHES "Pinned SDL copy-ordering")
        message(FATAL_ERROR "${case_name}: wrong failure reason: ${error}")
    endif()
    message(STATUS "${case_name}: passed")
endfunction()

set(original "${prefix}${anchor}${suffix}")
set(patched "${prefix}${expected}${suffix}")
run_patch(fresh "${original}" TRUE "${patched}")
run_patch(idempotent "${patched}" TRUE "${patched}")
run_patch(unknown "${prefix}new upstream backend${suffix}" FALSE "${prefix}new upstream backend${suffix}")
run_patch(duplicate_anchor "${original}${anchor}" FALSE "${original}${anchor}")
run_patch(marker_only "${prefix}${marker}${suffix}" FALSE "${prefix}${marker}${suffix}")
run_patch(marker_with_old_anchor "${original}${marker}" FALSE "${original}${marker}")
string(REPLACE "sourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;"
    "sourceState = D3D12_RESOURCE_STATE_COMMON;" altered "${patched}")
run_patch(altered_installed "${altered}" FALSE "${altered}")
run_patch(duplicate_installed "${patched}${expected}" FALSE "${patched}${expected}")
run_patch(duplicate_marker "${patched}${marker}" FALSE "${patched}${marker}")
# An additional definition need not match the old anchor or carry the marker.
# Counting only exact old/installed functions would silently accept it.
string(REPLACE "D3D12_INTERNAL_DefaultBufferResourceState(buffer)"
    "D3D12_RESOURCE_STATE_COMMON" other_definition "${anchor}")
run_patch(duplicate_unknown_definition "${original}${other_definition}"
    FALSE "${original}${other_definition}")
run_patch(installed_with_unknown_definition "${patched}${other_definition}"
    FALSE "${patched}${other_definition}")
message(STATUS "Eleven asset-free SDL copy-ordering installer cases passed; not native GPU/source/colour/FPS acceptance")
