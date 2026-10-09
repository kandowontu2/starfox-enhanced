# Keep whole draw/dispatch tables in one heap pair. Pinned SDL checked only
# their first slot, causing a multi-resource table to overrun its heap (0x87d).
set(source "${SOURCE_DIR}/src/gpu/d3d12/SDL_gpu_d3d12.c")
file(READ "${source}" code)
set(marker "/* Star Fox whole descriptor transactions v1 */")
string(FIND "${code}" "${marker}" installed)
if(installed GREATER_EQUAL 0)
    return()
endif()
function(replace_exact anchor replacement)
    string(FIND "${code}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Pinned SDL descriptor reservation anchor missing: ${anchor}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" updated "${code}")
    set(code "${updated}" PARENT_SCOPE)
endfunction()
replace_exact("    bool needComputeSamplerBind;"
    "    bool needComputeReadWriteStorageBind;\n    bool needComputeSamplerBind;")
set(set_heaps_anchor [=[    commandBuffer->gpuDescriptorHeaps[1] = samplerHeap;]=])
set(set_heaps_replacement [=[    commandBuffer->gpuDescriptorHeaps[1] = samplerHeap;

    /* Changing either heap invalidates ALL descriptor tables, including UAVs
     * installed by BindComputePipeline and clean tables from previous draws. */
    commandBuffer->needVertexSamplerBind = true;
    commandBuffer->needVertexStorageTextureBind = true;
    commandBuffer->needVertexStorageBufferBind = true;
    commandBuffer->needFragmentSamplerBind = true;
    commandBuffer->needFragmentStorageTextureBind = true;
    commandBuffer->needFragmentStorageBufferBind = true;
    commandBuffer->needComputeSamplerBind = true;
    commandBuffer->needComputeReadOnlyStorageTextureBind = true;
    commandBuffer->needComputeReadOnlyStorageBufferBind = true;
    commandBuffer->needComputeReadWriteStorageBind = true;]=])
replace_exact("${set_heaps_anchor}" "${set_heaps_replacement}")
set(reserve [=[/* Star Fox whole descriptor transactions v1 */
#include "starfox/render/d3d12_descriptor_capacity.h"
static void D3D12_INTERNAL_ReserveGPUDescriptorHeaps(
    D3D12CommandBuffer *commandBuffer, Uint32 viewCount, Uint32 samplerCount)
{
    SDL_assert(viewCount <= VIEW_GPU_DESCRIPTOR_COUNT);
    SDL_assert(samplerCount <= SAMPLER_GPU_DESCRIPTOR_COUNT);
    if (!commandBuffer->gpuDescriptorHeaps[0] || !commandBuffer->gpuDescriptorHeaps[1]
        || Starfox_D3D12DescriptorReservationNeeded(commandBuffer->gpuDescriptorHeaps[0]->currentDescriptorIndex,
            commandBuffer->gpuDescriptorHeaps[0]->maxDescriptors, viewCount)
        || Starfox_D3D12DescriptorReservationNeeded(commandBuffer->gpuDescriptorHeaps[1]->currentDescriptorIndex,
            commandBuffer->gpuDescriptorHeaps[1]->maxDescriptors, samplerCount)) {
        D3D12_INTERNAL_SetGPUDescriptorHeaps(commandBuffer);
    }
}

]=])
replace_exact("static void D3D12_INTERNAL_WriteGPUDescriptors("
    "${reserve}static void D3D12_INTERNAL_WriteGPUDescriptors(")
set(writer_anchor [=[    /* Descriptor overflow, acquire new heaps */
    if (commandBuffer->gpuDescriptorHeaps[heapType]->currentDescriptorIndex >= commandBuffer->gpuDescriptorHeaps[heapType]->maxDescriptors) {
        D3D12_INTERNAL_SetGPUDescriptorHeaps(commandBuffer);
    }

    heap = commandBuffer->gpuDescriptorHeaps[heapType];

    // FIXME: need to error on overflow]=])
set(writer_replacement [=[    /* The caller reserved every table before writing any. Never change
     * heaps mid-transaction and invalidate previously written tables. */
    heap = commandBuffer->gpuDescriptorHeaps[heapType];
    SDL_assert(!Starfox_D3D12DescriptorReservationNeeded(heap->currentDescriptorIndex,
        heap->maxDescriptors, resourceHandleCount));]=])
replace_exact("${writer_anchor}" "${writer_replacement}")
set(graphics_anchor [=[    D3D12GraphicsPipeline *graphicsPipeline = commandBuffer->currentGraphicsPipeline;

    /* Acquire GPU descriptor heaps if we haven't yet */
    if (commandBuffer->gpuDescriptorHeaps[0] == NULL) {
        D3D12_INTERNAL_SetGPUDescriptorHeaps(commandBuffer);
    }]=])
set(graphics_replacement [=[    D3D12GraphicsPipeline *graphicsPipeline = commandBuffer->currentGraphicsPipeline;

    /* Include clean tables: a rollover makes them dirty. */
    D3D12_INTERNAL_ReserveGPUDescriptorHeaps(commandBuffer,
        graphicsPipeline->header.num_vertex_samplers + graphicsPipeline->header.num_fragment_samplers
        + graphicsPipeline->header.num_vertex_storage_textures + graphicsPipeline->header.num_fragment_storage_textures
        + graphicsPipeline->header.num_vertex_storage_buffers + graphicsPipeline->header.num_fragment_storage_buffers,
        graphicsPipeline->header.num_vertex_samplers + graphicsPipeline->header.num_fragment_samplers);]=])
replace_exact("${graphics_anchor}" "${graphics_replacement}")
# Extract the pinned UAV binding body verbatim; reuse it after rollover.
string(FIND "${code}" "    // Bind write-only resources after setting root signature" write_start)
string(FIND "${code}" "static void D3D12_BindComputeSamplers(" next_function)
if(write_start LESS 0 OR next_function LESS write_start)
    message(FATAL_ERROR "Pinned SDL compute UAV binding anchors missing")
endif()
math(EXPR write_length "${next_function} - ${write_start}")
string(SUBSTRING "${code}" ${write_start} ${write_length} old_body)
string(REGEX REPLACE "\n}[ \r\n]*$" "" write_body "${old_body}")
set(write_helper "static void D3D12_INTERNAL_BindComputeWriteResources(D3D12CommandBuffer *d3d12CommandBuffer)\n{\n    D3D12ComputePipeline *pipeline = d3d12CommandBuffer->currentComputePipeline;\n    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandles[MAX_TEXTURE_SAMPLERS_PER_STAGE];\n    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;\n${write_body}\n    d3d12CommandBuffer->needComputeReadWriteStorageBind = false;\n}\n\n")
replace_exact("${old_body}" "    D3D12_INTERNAL_BindComputeWriteResources(d3d12CommandBuffer);\n}\n\n")
replace_exact("static void D3D12_BindComputePipeline(" "${write_helper}static void D3D12_BindComputePipeline(")
set(compute_pipeline_anchor [=[    /* Acquire GPU descriptor heaps if we haven't yet */
    if (d3d12CommandBuffer->gpuDescriptorHeaps[0] == NULL) {
        D3D12_INTERNAL_SetGPUDescriptorHeaps(d3d12CommandBuffer);
    }

    D3D12ComputePipeline *pipeline = (D3D12ComputePipeline *)computePipeline;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandles[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;]=])
set(compute_pipeline_replacement [=[    D3D12ComputePipeline *pipeline = (D3D12ComputePipeline *)computePipeline;
    D3D12_INTERNAL_ReserveGPUDescriptorHeaps(d3d12CommandBuffer,
        pipeline->header.numSamplers + pipeline->header.numReadonlyStorageTextures
        + pipeline->header.numReadonlyStorageBuffers + pipeline->header.numReadWriteStorageTextures
        + pipeline->header.numReadWriteStorageBuffers, pipeline->header.numSamplers);]=])
replace_exact("${compute_pipeline_anchor}" "${compute_pipeline_replacement}")
set(compute_resources_anchor [=[    D3D12ComputePipeline *computePipeline = commandBuffer->currentComputePipeline;

    /* Acquire GPU descriptor heaps if we haven't yet */
    if (commandBuffer->gpuDescriptorHeaps[0] == NULL) {
        D3D12_INTERNAL_SetGPUDescriptorHeaps(commandBuffer);
    }]=])
set(compute_resources_replacement [=[    D3D12ComputePipeline *computePipeline = commandBuffer->currentComputePipeline;

    /* Repeated dispatches can exhaust a heap without another pipeline bind. */
    D3D12_INTERNAL_ReserveGPUDescriptorHeaps(commandBuffer,
        computePipeline->header.numSamplers + computePipeline->header.numReadonlyStorageTextures
        + computePipeline->header.numReadonlyStorageBuffers + computePipeline->header.numReadWriteStorageTextures
        + computePipeline->header.numReadWriteStorageBuffers, computePipeline->header.numSamplers);
    if (commandBuffer->needComputeReadWriteStorageBind) {
        D3D12_INTERNAL_BindComputeWriteResources(commandBuffer);
    }]=])
replace_exact("${compute_resources_anchor}" "${compute_resources_replacement}")
file(WRITE "${source}" "${code}")
