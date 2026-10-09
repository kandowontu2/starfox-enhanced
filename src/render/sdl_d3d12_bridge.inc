/* Included inside the pinned SDL D3D12 backend, after its private helpers. */
#include "starfox/render/sdl_d3d12_bridge.h"

static bool Starfox_D3D12_CopyNativeBuffer(void *command, void *source, void *destination, uint32_t bytes)
{
    D3D12CommandBuffer *cb = (D3D12CommandBuffer *)command;
    ID3D12Resource *resource = (ID3D12Resource *)source;
    D3D12BufferContainer *container = (D3D12BufferContainer *)destination;
    ID3D12Device *owner = NULL;
    D3D12_RESOURCE_DESC desc;
    D3D12_RESOURCE_BARRIER barrier;
    D3D12Buffer *buffer;
    HRESULT hr;
    if (!cb || !resource || !container || !bytes || bytes > container->size)
        return SDL_SetError("Invalid Starfox native buffer copy: command=%d source=%d destination=%d bytes=%u capacity=%u",
            cb != NULL, resource != NULL, container != NULL, bytes, container ? container->size : 0);
    hr = ID3D12Resource_GetDevice(resource, D3D_GUID(D3D_IID_ID3D12Device), (void **)&owner);
    if (FAILED(hr)) return SDL_SetError("Cannot identify native buffer device");
    if (owner != cb->renderer->device) {
        ID3D12Device_Release(owner);
        return SDL_SetError("Native buffer must be opened on the SDL device");
    }
    ID3D12Device_Release(owner);
    ID3D12Resource_GetDesc(resource, &desc);
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || desc.Width < bytes)
        return SDL_SetError("Native buffer is too small or not a buffer");
    buffer = D3D12_INTERNAL_PrepareBufferForWrite(cb, container, true, D3D12_RESOURCE_STATE_COPY_DEST);
    SDL_zero(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    ID3D12GraphicsCommandList_ResourceBarrier(cb->graphicsCommandList, 1, &barrier);
    ID3D12GraphicsCommandList_CopyBufferRegion(cb->graphicsCommandList, buffer->handle, 0, resource, 0, bytes);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    ID3D12GraphicsCommandList_ResourceBarrier(cb->graphicsCommandList, 1, &barrier);
    D3D12_INTERNAL_BufferTransitionToDefaultUsage(cb, D3D12_RESOURCE_STATE_COPY_DEST, buffer);
    D3D12_INTERNAL_TrackBuffer(cb, buffer);
    return true;
}
static bool Starfox_D3D12_WaitFence(void *device, void *fence, uint64_t value)
{
    D3D12Renderer *renderer;
    HRESULT hr;
    if (!device || !fence || !value) return SDL_SetError("Invalid native producer fence");
    renderer = (D3D12Renderer *)((SDL_GPUDevice *)device)->driverData;
    SDL_LockMutex(renderer->submitLock);
    hr = ID3D12CommandQueue_Wait(renderer->commandQueue, (ID3D12Fence *)fence, value);
    SDL_UnlockMutex(renderer->submitLock);
    if (FAILED(hr)) return SDL_SetError("Native producer fence wait failed");
    return true;
}
static const StarfoxSdlD3D12BridgeV2 starfox_d3d12_bridge = {2, Starfox_D3D12_CopyNativeBuffer, Starfox_D3D12_WaitFence};

static bool Starfox_D3D12_CopyToExternal(void *command, void *source, void *destination, uint32_t bytes)
{
    D3D12CommandBuffer *cb = (D3D12CommandBuffer *)command;
    D3D12BufferContainer *container = (D3D12BufferContainer *)source;
    ID3D12Resource *resource = (ID3D12Resource *)destination;
    ID3D12Device *owner = NULL;
    D3D12_RESOURCE_DESC desc;
    D3D12_RESOURCE_BARRIER barrier;
    D3D12Buffer *buffer;
    if (!cb || !container || !resource || !bytes || (bytes & 3) || bytes > container->size)
        return SDL_SetError("Invalid Starfox geometry copy");
    if (FAILED(ID3D12Resource_GetDevice(resource, D3D_GUID(D3D_IID_ID3D12Device), (void **)&owner)))
        return SDL_SetError("Cannot identify geometry destination device");
    if (owner != cb->renderer->device) {
        ID3D12Device_Release(owner);
        return SDL_SetError("Geometry destination must be opened on SDL device");
    }
    ID3D12Device_Release(owner);
    ID3D12Resource_GetDesc(resource, &desc);
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || desc.Width < bytes)
        return SDL_SetError("Invalid geometry destination size");
    buffer = container->activeBuffer;
    D3D12_INTERNAL_BufferTransitionFromDefaultUsage(cb, D3D12_RESOURCE_STATE_COPY_SOURCE, buffer);
    SDL_zero(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    ID3D12GraphicsCommandList_ResourceBarrier(cb->graphicsCommandList, 1, &barrier);
    ID3D12GraphicsCommandList_CopyBufferRegion(cb->graphicsCommandList, resource, 0, buffer->handle, 0, bytes);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    ID3D12GraphicsCommandList_ResourceBarrier(cb->graphicsCommandList, 1, &barrier);
    D3D12_INTERNAL_BufferTransitionToDefaultUsage(cb, D3D12_RESOURCE_STATE_COPY_SOURCE, buffer);
    D3D12_INTERNAL_TrackBuffer(cb, buffer);
    return true;
}
static bool Starfox_D3D12_SignalFence(void *device, void *fence, uint64_t value)
{
    D3D12Renderer *renderer;
    HRESULT hr;
    if (!device || !fence || !value) return SDL_SetError("Invalid geometry completion fence");
    renderer = (D3D12Renderer *)((SDL_GPUDevice *)device)->driverData;
    SDL_LockMutex(renderer->submitLock);
    hr = ID3D12CommandQueue_Signal(renderer->commandQueue, (ID3D12Fence *)fence, value);
    SDL_UnlockMutex(renderer->submitLock);
    if (FAILED(hr)) return SDL_SetError("Geometry completion signal failed");
    return true;
}
static const StarfoxSdlD3D12GeometryBridgeV1 starfox_d3d12_geometry_bridge = {1, Starfox_D3D12_CopyToExternal, Starfox_D3D12_SignalFence};

static bool Starfox_D3D12_CopyTexture(void *command, void *sdl_texture, void *native_texture,
                                    bool importing, D3D12_RESOURCE_STATES external_state)
{
    D3D12CommandBuffer *cb = (D3D12CommandBuffer *)command;
    D3D12TextureContainer *container = (D3D12TextureContainer *)sdl_texture;
    ID3D12Resource *external = (ID3D12Resource *)native_texture;
    ID3D12Device *owner = NULL;
    D3D12_RESOURCE_DESC a, b;
    D3D12_RESOURCE_BARRIER barrier;
    D3D12TextureSubresource *sub;
    D3D12_TEXTURE_COPY_LOCATION src, dst;
    D3D12_RESOURCE_STATES state = importing ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_COPY_DEST;
    if (!cb || !container || !external || !container->activeTexture)
        return SDL_SetError("Invalid Starfox texture copy");
    if (FAILED(ID3D12Resource_GetDevice(external, D3D_GUID(D3D_IID_ID3D12Device), (void **)&owner)))
        return SDL_SetError("Cannot identify native texture device");
    if (owner != cb->renderer->device) {
        ID3D12Device_Release(owner);
        return SDL_SetError("Native texture belongs to another device");
    }
    ID3D12Device_Release(owner);
    ID3D12Resource_GetDesc(external, &a);
    ID3D12Resource_GetDesc(container->activeTexture->resource, &b);
    if (external == container->activeTexture->resource ||
        a.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || a.Dimension != b.Dimension ||
        a.Width != b.Width || a.Height != b.Height || a.Format != b.Format ||
        a.DepthOrArraySize != 1 || b.DepthOrArraySize != 1 ||
        a.MipLevels != 1 || b.MipLevels != 1 || a.SampleDesc.Count != 1 || b.SampleDesc.Count != 1 ||
        (external_state == D3D12_RESOURCE_STATE_RENDER_TARGET && !(a.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)))
        return SDL_SetError("Native texture copy requires matching single-mip 2D textures");
    sub = importing ? D3D12_INTERNAL_PrepareTextureSubresourceForWrite(cb, container, 0, 0, true, D3D12_RESOURCE_STATE_COPY_DEST)
                    : D3D12_INTERNAL_FetchTextureSubresource(container, 0, 0);
    if (!importing) D3D12_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(cb, D3D12_RESOURCE_STATE_COPY_SOURCE, sub);
    SDL_zero(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = external;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = external_state;
    barrier.Transition.StateAfter = state;
    ID3D12GraphicsCommandList_ResourceBarrier(cb->graphicsCommandList, 1, &barrier);
    SDL_zero(src); SDL_zero(dst);
    src.Type = dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.pResource = importing ? external : sub->parent->resource;
    dst.pResource = importing ? sub->parent->resource : external;
    ID3D12GraphicsCommandList_CopyTextureRegion(cb->graphicsCommandList, &dst, 0, 0, 0, &src, NULL);
    barrier.Transition.StateBefore = state;
    barrier.Transition.StateAfter = external_state;
    ID3D12GraphicsCommandList_ResourceBarrier(cb->graphicsCommandList, 1, &barrier);
    D3D12_INTERNAL_TextureSubresourceTransitionToDefaultUsage(cb, importing ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COPY_SOURCE, sub);
    D3D12_INTERNAL_TrackTexture(cb, sub->parent);
    return true;
}
static bool Starfox_D3D12_ExportTexture(void *command, void *texture, void *native_texture)
{ return Starfox_D3D12_CopyTexture(command, texture, native_texture, false, D3D12_RESOURCE_STATE_COMMON); }
static bool Starfox_D3D12_ImportTexture(void *command, void *native_texture, void *texture)
{ return Starfox_D3D12_CopyTexture(command, texture, native_texture, true, D3D12_RESOURCE_STATE_COMMON); }
static const StarfoxSdlD3D12TextureBridgeV1 starfox_d3d12_texture_bridge = {1, Starfox_D3D12_ExportTexture, Starfox_D3D12_ImportTexture};

static void *Starfox_D3D12_XrQueue(void *device)
{
    if (!device || SDL_strcmp(SDL_GetGPUDeviceDriver((SDL_GPUDevice *)device), "direct3d12") != 0) {
        SDL_SetError("Invalid D3D12 XR device"); return NULL;
    }
    return ((D3D12Renderer *)((SDL_GPUDevice *)device)->driverData)->commandQueue;
}
static bool Starfox_D3D12_WithXrQueue(void *device, StarfoxD3D12QueueCallback callback, void *user)
{
    D3D12Renderer *renderer;
    bool result;
    if (!callback || !Starfox_D3D12_XrQueue(device)) return SDL_SetError("Invalid D3D12 XR queue callback");
    renderer = (D3D12Renderer *)((SDL_GPUDevice *)device)->driverData;
    SDL_LockMutex(renderer->submitLock);
    result = callback(user, renderer->commandQueue);
    SDL_UnlockMutex(renderer->submitLock);
    return result;
}
static bool Starfox_D3D12_ExportXrColor(void *command, void *texture, void *native_texture)
{ return Starfox_D3D12_CopyTexture(command, texture, native_texture, false, D3D12_RESOURCE_STATE_RENDER_TARGET); }
static bool Starfox_D3D12_ImportXrColor(void *command, void *native_texture, void *texture)
{ return Starfox_D3D12_CopyTexture(command, texture, native_texture, true, D3D12_RESOURCE_STATE_RENDER_TARGET); }
static const StarfoxSdlD3D12XrBridgeV1 starfox_d3d12_xr_bridge = {
    1, Starfox_D3D12_XrQueue, Starfox_D3D12_WithXrQueue,
    Starfox_D3D12_ExportXrColor, Starfox_D3D12_ImportXrColor};

static bool Starfox_D3D12_DispatchNative(void *command, void *const *textures, uint32_t count,
    uint32_t output_index, StarfoxD3D12ComputeCallback callback, void *user)
{
    D3D12CommandBuffer *cb = (D3D12CommandBuffer *)command;
    D3D12TextureSubresource *subresources[8];
    void *resources[8];
    bool result;
    if (!cb || !textures || !callback || !count || count > 8 || output_index >= count)
        return SDL_SetError("Invalid native compute dispatch");
    for (uint32_t i = 0; i < count; ++i) {
        D3D12TextureContainer *container = (D3D12TextureContainer *)textures[i];
        if (!container || !container->activeTexture || container->activeTexture->subresourceCount != 1 ||
            container->header.info.type != SDL_GPU_TEXTURETYPE_2D || container->header.info.sample_count != SDL_GPU_SAMPLECOUNT_1 ||
            (i == output_index && !(container->header.info.usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE)))
            return SDL_SetError("Native compute requires single-mip 2D textures and a writable output");
        for (uint32_t j = 0; j < i; ++j)
            if (textures[j] == textures[i]) return SDL_SetError("Native compute texture alias");
    }
    for (uint32_t i = 0; i < count; ++i) {
        D3D12TextureContainer *container = (D3D12TextureContainer *)textures[i];
        if (i == output_index)
            subresources[i] = D3D12_INTERNAL_PrepareTextureSubresourceForWrite(cb, container, 0, 0, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        else {
            subresources[i] = D3D12_INTERNAL_FetchTextureSubresource(container, 0, 0);
            D3D12_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(cb, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, subresources[i]);
        }
        resources[i] = subresources[i]->parent->resource;
        D3D12_INTERNAL_TrackTexture(cb, subresources[i]->parent);
    }
    result = callback(user, cb->graphicsCommandList, resources, count);
    for (uint32_t i = 0; i < count; ++i)
        D3D12_INTERNAL_TextureSubresourceTransitionToDefaultUsage(cb, i == output_index ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, subresources[i]);
    if (cb->gpuDescriptorHeaps[0] && cb->gpuDescriptorHeaps[1]) {
        ID3D12DescriptorHeap *heaps[] = {cb->gpuDescriptorHeaps[0]->handle, cb->gpuDescriptorHeaps[1]->handle};
        ID3D12GraphicsCommandList_SetDescriptorHeaps(cb->graphicsCommandList, 2, heaps);
    }
    cb->currentComputePipeline = NULL;
    cb->currentGraphicsPipeline = NULL;
    return result;
}
static const StarfoxSdlD3D12ComputeBridgeV1 starfox_d3d12_compute_bridge = {1, Starfox_D3D12_DispatchNative};

static bool Starfox_D3D12_RestorePresentation(void *device)
{
    D3D12Renderer *renderer;
    const StarfoxSdlD3D12PresentHooksV1 *hooks = (const StarfoxSdlD3D12PresentHooksV1 *)SDL_GetPointerProperty(SDL_GetGlobalProperties(), STARFOX_SDL_D3D12_PRESENT_HOOKS, NULL);
    if (!device) return SDL_SetError("Invalid presentation device");
    if (!hooks || hooks->version != 1 || !hooks->swapchain) return true;
    renderer = (D3D12Renderer *)((SDL_GPUDevice *)device)->driverData;
    for (uint32_t i = 0; i < renderer->claimedWindowCount; ++i)
        if (!hooks->swapchain(hooks->user, renderer->device, (void **)&renderer->claimedWindows[i]->swapchain, true)) return false;
    return true;
}
static const StarfoxSdlD3D12PresentBridgeV1 starfox_d3d12_present_bridge = {1, Starfox_D3D12_RestorePresentation};

/* Adapted from agrofubris/starfox-enhanced-stereo 129d917 (GPLv3).
 * Keep the woven image separate from SDL's mode-switch-sensitive backbuffer. */
static bool Starfox_D3D12_OwnedWeave(void *command, void *source, void *target,
    uint32_t width, uint32_t height, StarfoxD3D12OwnedWeaveCallback callback, void *user)
{
    D3D12CommandBuffer *cb = (D3D12CommandBuffer *)command;
    D3D12TextureContainer *src = (D3D12TextureContainer *)source;
    D3D12TextureContainer *dst = (D3D12TextureContainer *)target;
    D3D12TextureSubresource *input, *output;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor;
    ID3D12Device *owner;
    bool result;
    if (!cb || !src || !dst || !callback || !width || !height || (width & 1) || (height & 1) ||
        width > UINT32_MAX / 2 || src == dst || !src->canBeCycled || !dst->canBeCycled || !src->activeTexture || !dst->activeTexture ||
        src->activeTexture->resource == dst->activeTexture->resource ||
        src->header.info.format != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
        dst->header.info.format != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
        src->header.info.width != width * 2 || src->header.info.height != height ||
        dst->header.info.width != width || dst->header.info.height != height ||
        src->header.info.type != SDL_GPU_TEXTURETYPE_2D || dst->header.info.type != SDL_GPU_TEXTURETYPE_2D ||
        src->activeTexture->subresourceCount != 1 || dst->activeTexture->subresourceCount != 1 ||
        src->header.info.sample_count != SDL_GPU_SAMPLECOUNT_1 || dst->header.info.sample_count != SDL_GPU_SAMPLECOUNT_1 ||
        !(src->header.info.usage & SDL_GPU_TEXTUREUSAGE_SAMPLER) ||
        !(dst->header.info.usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
        return SDL_SetError("Native SR weave requires distinct app-owned BGRA 2W/H and W/H textures");
    /* D3D12CreateDevice is a per-adapter singleton: different SDL renderers
     * can share its COM pointer but not descriptor pools or submission queues.
     * Validate SDL ownership before recording any transitions/tracking. */
    if (src->activeTexture->srvHandle.pool != cb->renderer->stagingDescriptorPools[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] ||
        !dst->activeTexture->subresources[0].rtvHandles ||
        dst->activeTexture->subresources[0].rtvHandles[0].pool != cb->renderer->stagingDescriptorPools[D3D12_DESCRIPTOR_HEAP_TYPE_RTV])
        return SDL_SetError("SR textures belong to another SDL renderer");
    for (uint32_t i = 0; i < 2; ++i) {
        ID3D12Resource *resource = i ? dst->activeTexture->resource : src->activeTexture->resource;
        owner = NULL;
        if (FAILED(ID3D12Resource_GetDevice(resource, D3D_GUID(D3D_IID_ID3D12Device), (void **)&owner)))
            return SDL_SetError("Cannot identify SR texture device");
        if (owner != cb->renderer->device) {
            ID3D12Device_Release(owner);return SDL_SetError("SR textures belong to another device");
        }
        ID3D12Device_Release(owner);
    }
    input = D3D12_INTERNAL_FetchTextureSubresource(src, 0, 0);
    D3D12_INTERNAL_TextureSubresourceTransitionFromDefaultUsage(cb, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, input);
    /* Cycling would discard the target that SDL will sample after this call. */
    output = D3D12_INTERNAL_PrepareTextureSubresourceForWrite(cb, dst, 0, 0, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_INTERNAL_TrackTexture(cb, input->parent);
    D3D12_INTERNAL_TrackTexture(cb, output->parent);
    rtv = output->rtvHandles[0].cpuHandle;
    ID3D12GraphicsCommandList_OMSetRenderTargets(cb->graphicsCommandList, 1, &rtv, false, NULL);
    SDL_zero(viewport);viewport.Width = (float)width;viewport.Height = (float)height;viewport.MaxDepth = 1.0f;
    scissor.left = scissor.top = 0;scissor.right = (LONG)width;scissor.bottom = (LONG)height;
    ID3D12GraphicsCommandList_RSSetViewports(cb->graphicsCommandList, 1, &viewport);
    ID3D12GraphicsCommandList_RSSetScissorRects(cb->graphicsCommandList, 1, &scissor);
    result = callback(user, cb->graphicsCommandList, input->parent->resource, output->parent->resource,
        width, height, (uint32_t)DXGI_FORMAT_B8G8R8A8_UNORM);
    ID3D12GraphicsCommandList_OMSetRenderTargets(cb->graphicsCommandList, 0, NULL, false, NULL);
    D3D12_INTERNAL_TextureSubresourceTransitionToDefaultUsage(cb, D3D12_RESOURCE_STATE_RENDER_TARGET, output);
    D3D12_INTERNAL_TextureSubresourceTransitionToDefaultUsage(cb, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, input);
    if (cb->gpuDescriptorHeaps[0] && cb->gpuDescriptorHeaps[1]) {
        ID3D12DescriptorHeap *heaps[] = {cb->gpuDescriptorHeaps[0]->handle, cb->gpuDescriptorHeaps[1]->handle};
        ID3D12GraphicsCommandList_SetDescriptorHeaps(cb->graphicsCommandList, 2, heaps);
    }
    cb->currentGraphicsPipeline = NULL;cb->currentComputePipeline = NULL;
    return result;
}
static const StarfoxSdlD3D12OwnedWeaveV1 starfox_d3d12_owned_weave_bridge = {1, Starfox_D3D12_OwnedWeave};

typedef struct StarfoxD3D12Timestamps {
    D3D12Renderer *renderer;
    ID3D12QueryHeap *heap;
    ID3D12Resource *readback;
    uint32_t count;
} StarfoxD3D12Timestamps;
static void Starfox_D3D12_DestroyTimestamps(void *context)
{
    StarfoxD3D12Timestamps *state = (StarfoxD3D12Timestamps *)context;
    if (!state) return;
    if (state->readback) ID3D12Resource_Release(state->readback);
    if (state->heap) ID3D12QueryHeap_Release(state->heap);
    SDL_free(state);
}
static void *Starfox_D3D12_CreateTimestamps(void *device, uint32_t count, uint64_t *frequency)
{
    static const IID query_iid = {0x0d9658ae,0xed45,0x469e,{0xa6,0x1d,0x97,0x0e,0xc5,0x83,0xca,0xb4}};
    StarfoxD3D12Timestamps *state;
    D3D12_QUERY_HEAP_DESC query;
    D3D12_HEAP_PROPERTIES properties;
    D3D12_RESOURCE_DESC resource;
    UINT64 hz = 0;
    if (!device || !frequency || !count || count > 4096 ||
        SDL_strcmp(SDL_GetGPUDeviceDriver((SDL_GPUDevice *)device), "direct3d12") != 0) return NULL;
    *frequency = 0;
    state = (StarfoxD3D12Timestamps *)SDL_calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->renderer = (D3D12Renderer *)((SDL_GPUDevice *)device)->driverData;
    state->count = count;
    SDL_zero(query); query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP; query.Count = count;
    SDL_zero(properties); properties.Type = D3D12_HEAP_TYPE_READBACK;
    SDL_zero(resource); resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource.Width = (UINT64)count * sizeof(UINT64); resource.Height = 1;
    resource.DepthOrArraySize = resource.MipLevels = resource.SampleDesc.Count = 1;
    resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(ID3D12CommandQueue_GetTimestampFrequency(state->renderer->commandQueue, &hz)) || !hz ||
        FAILED(ID3D12Device_CreateQueryHeap(state->renderer->device, &query, D3D_GUID(query_iid), (void **)&state->heap)) ||
        FAILED(ID3D12Device_CreateCommittedResource(state->renderer->device, &properties, D3D12_HEAP_FLAG_NONE,
            &resource, D3D12_RESOURCE_STATE_COPY_DEST, NULL, D3D_GUID(D3D_IID_ID3D12Resource), (void **)&state->readback))) {
        Starfox_D3D12_DestroyTimestamps(state); return NULL;
    }
    *frequency = hz; return state;
}
static bool Starfox_D3D12_WriteTimestamp(void *command, void *context, uint32_t index)
{
    D3D12CommandBuffer *cb = (D3D12CommandBuffer *)command;
    StarfoxD3D12Timestamps *state = (StarfoxD3D12Timestamps *)context;
    if (!cb || !state || cb->renderer != state->renderer || index >= state->count) return false;
    ID3D12GraphicsCommandList_EndQuery(cb->graphicsCommandList, state->heap, D3D12_QUERY_TYPE_TIMESTAMP, index);
    return true;
}
static bool Starfox_D3D12_ResolveTimestamps(void *command, void *context, uint32_t first, uint32_t count)
{
    D3D12CommandBuffer *cb = (D3D12CommandBuffer *)command;
    StarfoxD3D12Timestamps *state = (StarfoxD3D12Timestamps *)context;
    if (!cb || !state || cb->renderer != state->renderer || !count || first >= state->count || count > state->count-first) return false;
    ID3D12GraphicsCommandList_ResolveQueryData(cb->graphicsCommandList, state->heap, D3D12_QUERY_TYPE_TIMESTAMP,
        first, count, state->readback, (UINT64)first * sizeof(UINT64));
    return true;
}
static bool Starfox_D3D12_ReadTimestamps(void *context, uint32_t first, uint32_t count, uint64_t *ticks)
{
    StarfoxD3D12Timestamps *state = (StarfoxD3D12Timestamps *)context;
    D3D12_RANGE read_range, write_range;
    void *data = NULL;
    if (!state || !ticks || !count || first >= state->count || count > state->count-first) return false;
    read_range.Begin = (SIZE_T)first * sizeof(UINT64); read_range.End = read_range.Begin + (SIZE_T)count * sizeof(UINT64);
    if (FAILED(ID3D12Resource_Map(state->readback, 0, &read_range, &data))) return false;
    SDL_memcpy(ticks, (const Uint8 *)data + read_range.Begin, count * sizeof(UINT64));
    SDL_zero(write_range); ID3D12Resource_Unmap(state->readback, 0, &write_range);
    return true;
}
static const StarfoxSdlD3D12TimestampsV1 starfox_d3d12_timestamps = {1,
    Starfox_D3D12_CreateTimestamps, Starfox_D3D12_WriteTimestamp, Starfox_D3D12_ResolveTimestamps,
    Starfox_D3D12_ReadTimestamps, Starfox_D3D12_DestroyTimestamps};
