# Guarded pinned-backend candidate for COPY_SOURCE -> future compute writes.
# Keep the public default resource state and the source/copy bytes unchanged.
set(backend "${SOURCE_DIR}/src/gpu/d3d12/SDL_gpu_d3d12.c")
file(READ "${backend}" code)
set(marker "/* Star Fox copy-read to future UAV write ordering v1 */")
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
set(replacement [=[static void D3D12_INTERNAL_BufferTransitionToDefaultUsage(
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
function(count_exact text fragment output)
    string(LENGTH "${text}" text_length)
    string(LENGTH "${fragment}" fragment_length)
    string(REPLACE "${fragment}" "" remainder "${text}")
    string(LENGTH "${remainder}" remainder_length)
    math(EXPR occurrences "(${text_length} - ${remainder_length}) / ${fragment_length}")
    set(${output} "${occurrences}" PARENT_SCOPE)
endfunction()
# An additional altered definition can match neither the old anchor nor the
# installed replacement. Refuse it before either replacement or early return.
string(REGEX MATCHALL "static[ \t\r\n]+void[ \t\r\n]+D3D12_INTERNAL_BufferTransitionToDefaultUsage[ \t\r\n]*\\("
    definitions "${code}")
list(LENGTH definitions definition_count)
if(NOT definition_count EQUAL 1)
    message(FATAL_ERROR "Pinned SDL copy-ordering function missing or duplicated")
endif()
count_exact("${code}" "${marker}" markers)
count_exact("${code}" "${anchor}" anchors)
if(markers GREATER 0)
    count_exact("${code}" "${replacement}" replacements)
    if(NOT markers EQUAL 1 OR NOT replacements EQUAL 1 OR NOT anchors EQUAL 0)
        message(FATAL_ERROR "Pinned SDL copy-ordering patch is partial or ambiguous")
    endif()
    return()
endif()
if(NOT anchors EQUAL 1)
    message(FATAL_ERROR "Pinned SDL copy-ordering anchor missing or duplicated")
endif()
string(REPLACE "${anchor}" "${replacement}" code "${code}")
file(WRITE "${backend}" "${code}")
