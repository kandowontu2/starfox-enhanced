#include "evaluate.hpp"
#include "starfox/render/dlss_native.h"
#include <sl.h>
#include <sl_helpers.h>
#include <sl_matrix_helpers.h>
#include <wrl/client.h>
#include <dxgi1_4.h>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT r, const char* operation) {
    if (FAILED(r)) throw std::runtime_error(std::string(operation) + " HRESULT=" + std::to_string(r));
}
void check(sl::Result r, const char* operation) {
    if (r != sl::Result::eOk) throw std::runtime_error(std::string(operation) + ": " + sl::getResultAsStr(r));
}
template<class T> T* api(HMODULE module, const char* name) {
    const auto fn = reinterpret_cast<T*>(GetProcAddress(module, name));
    if (!fn) throw std::runtime_error(std::string("Missing export ") + name);
    return fn;
}
template<class T> T* feature(HMODULE module, const char* name) {
    void* fn{};
    check(api<PFun_slGetFeatureFunction>(module, "slGetFeatureFunction")(sl::kFeatureDLSS, name, fn), name);
    if (!fn) throw std::runtime_error(std::string("Missing feature function ") + name);
    return reinterpret_cast<T*>(fn);
}
ComPtr<ID3D12Resource> buffer(ID3D12Device* device, UINT64 bytes, D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = type;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes; desc.Height = 1; desc.DepthOrArraySize = 1;
    desc.MipLevels = 1; desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> result;
    check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&result)), "Create buffer");
    return result;
}
struct Texture {
    ComPtr<ID3D12Resource> texture, upload;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{};
    D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_COPY_DEST};
    Texture(ID3D12Device* device, UINT w, UINT h, DXGI_FORMAT format, bool output = false) {
        D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = w; desc.Height = h; desc.DepthOrArraySize = 1;
        desc.MipLevels = 1; desc.SampleDesc.Count = 1; desc.Format = format;
        if (output) desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state,
            nullptr, IID_PPV_ARGS(&texture)), "Create texture");
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        upload = buffer(device, bytes, D3D12_HEAP_TYPE_UPLOAD);
    }
    void transition(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES next) {
        if (state == next) return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state, next};
        list->ResourceBarrier(1, &barrier); state = next;
    }
    void fill(ID3D12GraphicsCommandList* list, const void* source, size_t row_bytes) {
        transition(list, D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{}; D3D12_RANGE empty{};
        check(upload->Map(0, &empty, &mapped), "Map upload");
        for (UINT y = 0; y < footprint.Footprint.Height; ++y)
            std::memcpy(static_cast<char*>(mapped) + footprint.Offset + size_t(y) * footprint.Footprint.RowPitch,
                static_cast<const char*>(source) + size_t(y) * row_bytes, row_bytes);
        upload->Unmap(0, nullptr);
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
        src.pResource = upload.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = footprint;
        dst.pResource = texture.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        transition(list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
};
float halton(unsigned index, unsigned base) {
    float value = 0, weight = 1;
    while (index) { weight /= float(base); value += weight * float(index % base); index /= base; }
    return value - 0.5f;
}
sl::float4x4 identity() {
    sl::float4x4 m{};
    for (unsigned r = 0; r < 4; ++r) m[r] = {float(r == 0), float(r == 1), float(r == 2), float(r == 3)};
    return m;
}
}

void evaluate_dlss(HMODULE module, ID3D12Device* device, const std::filesystem::path& directory) {
    if (std::filesystem::exists(directory)) throw std::runtime_error("Evaluation output already exists; use a new directory");
    std::filesystem::create_directories(directory);
    auto free_resources = api<PFun_slFreeResources>(module, "slFreeResources");
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "Create queue");
    // Manual Streamline integration still needs the per-present bookkeeping
    // hook. Use a hidden test window, never a global graphics hook or installer.
    struct Window {
        HWND hwnd{};
        ~Window() { if (hwnd) DestroyWindow(hwnd); }
    } window;
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = instance;
    wc.lpszClassName = L"StarfoxDlssEvaluation";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        throw std::runtime_error("Register evaluation window failed");
    window.hwnd = CreateWindowW(wc.lpszClassName, L"DLSS evaluation", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, instance, nullptr);
    if (!window.hwnd) throw std::runtime_error("Create evaluation window failed");
    ComPtr<IDXGIFactory2> factory;
    check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "Create presentation factory");
    DXGI_SWAP_CHAIN_DESC1 swap_desc{};
    swap_desc.Width = 1280; swap_desc.Height = 720; swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1; swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2; swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> created_swap;
    check(factory->CreateSwapChainForHwnd(queue.Get(), window.hwnd, &swap_desc, nullptr, nullptr, &created_swap), "Create evaluation swapchain");
    void* upgraded = created_swap.Get();
    check(api<PFun_slUpgradeInterface>(module, "slUpgradeInterface")(&upgraded), "Upgrade evaluation swapchain");
    // The proxy retains its own native reference; release our original one.
    created_swap.Reset();
    ComPtr<IDXGISwapChain1> swap;
    swap.Attach(static_cast<IDXGISwapChain1*>(upgraded));
    ComPtr<ID3D12CommandAllocator> allocator;
    check(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator)), "Create allocator");
    ComPtr<ID3D12GraphicsCommandList> list;
    check(device->CreateCommandList(0, q.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "Create list");
    check(list->Close(), "Close initial list");
    ComPtr<ID3D12Fence> fence;
    check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "Create fence");
    struct Event { HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr); ~Event() { if (handle) CloseHandle(handle); } } event;
    if (!event.handle) throw std::runtime_error("Create fence event failed");
    UINT64 serial = 0;
    UINT frame_index = 0;
    std::ofstream report(directory / "evaluation.txt");
    if (!report) throw std::runtime_error("Cannot create evaluation report");
    for (auto mode : {sl::DLSSMode::eMaxQuality, sl::DLSSMode::eBalanced, sl::DLSSMode::eMaxPerformance, sl::DLSSMode::eDLAA}) {
        sl::ViewportHandle viewport{static_cast<UINT>(mode)};
        sl::DLSSOptions options{}; options.mode = mode;
        options.outputWidth = 1280; options.outputHeight = 720;
        options.colorBuffersHDR = sl::Boolean::eFalse;
        options.useAutoExposure = sl::Boolean::eFalse;
        uint32_t w{},h{};char configuration_error[512]{};
        const uint32_t native_mode=mode==sl::DLSSMode::eMaxQuality?1:mode==sl::DLSSMode::eBalanced?2:mode==sl::DLSSMode::eMaxPerformance?3:4;
        if(starfox_dlss_configure_v1(module,UINT(mode),native_mode,1280,720,&w,&h,configuration_error,sizeof(configuration_error)))
            throw std::runtime_error(configuration_error);
        if (!w || !h || w > 4096 || h > 4096) throw std::runtime_error("Invalid input dimensions");
        Texture color(device, w, h, DXGI_FORMAT_R8G8B8A8_UNORM);
        Texture depth(device, w, h, DXGI_FORMAT_R32_FLOAT);
        Texture motion(device, w, h, DXGI_FORMAT_R32G32_FLOAT);
        Texture exposure(device, 1, 1, DXGI_FORMAT_R32_FLOAT);
        Texture output(device, options.outputWidth, options.outputHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true);
        auto readback = buffer(device, output.bytes, D3D12_HEAP_TYPE_READBACK);
        std::vector<UINT> rgb(size_t(w) * h);
        std::vector<float> z(rgb.size());
        std::vector<sl::float2> mv(rgb.size());
        sl::Constants c{};
        c.cameraPos = {0, 0, 0}; c.cameraUp = {0, 1, 0}; c.cameraRight = {1, 0, 0}; c.cameraFwd = {0, 0, 1};
        c.cameraNear = 0.1f; c.cameraFar = 100.f; c.cameraFOV = 1.f; c.cameraAspectRatio = float(w) / h;
        c.cameraViewToClip = identity();
        const float sy = 1.f / std::tan(c.cameraFOV * 0.5f);
        const float a = c.cameraFar / (c.cameraFar - c.cameraNear), b = -c.cameraNear * a;
        c.cameraViewToClip[0] = {sy / c.cameraAspectRatio, 0, 0, 0};
        c.cameraViewToClip[1] = {0, sy, 0, 0};
        c.cameraViewToClip[2] = {0, 0, a, 1}; c.cameraViewToClip[3] = {0, 0, b, 0};
        sl::matrixFullInvert(c.clipToCameraView, c.cameraViewToClip);
        c.clipToPrevClip = c.prevClipToClip = c.clipToLensClip = identity();
        c.mvecScale = {1.f / w, 1.f / h}; c.cameraPinholeOffset = {0, 0};
        c.depthInverted = sl::Boolean::eFalse; c.cameraMotionIncluded = sl::Boolean::eTrue;
        c.motionVectors3D = sl::Boolean::eFalse; c.motionVectorsInvalidValue = -FLT_MAX;
        std::uint64_t last_hash = 0; unsigned changed = 0;
        for (UINT frame = 0; frame < 32; ++frame, ++frame_index) {
            check(allocator->Reset(), "Reset allocator"); check(list->Reset(allocator.Get(), nullptr), "Reset list");
            c.reset = (frame == 0 || frame == 16) ? sl::Boolean::eTrue : sl::Boolean::eFalse;
            c.jitterOffset = {halton(frame % 16 + 1, 2), halton(frame % 16 + 1, 3)};
            // Source is an analytical perspective scene with two frontoparallel
            // planes. Foreground translates exactly one render pixel per frame;
            // at frame 16 it jumps, and the history is explicitly reset.
            const float left = float(w) * 0.25f + float(frame) + (frame >= 16 ? 40.f : 0.f);
            for (UINT y = 0; y < h; ++y) for (UINT x = 0; x < w; ++x) {
                const size_t i = size_t(y) * w + x;
                const float px = float(x) + 0.5f + c.jitterOffset.x, py = float(y) + 0.5f + c.jitterOffset.y;
                const bool front = px >= left && px < left + float(w) * 0.25f && py > h * 0.3f && py < h * 0.7f;
                const UINT grey = ((int(px / 12) + int(py / 12)) & 1) ? 65 : 25;
                rgb[i] = front ? 0xff2050e0u : 0xff000000u | grey | grey << 8 | grey << 16;
                z[i] = a + b / (front ? 4.f : 8.f);
                mv[i] = {front && c.reset == sl::Boolean::eFalse ? -1.f : 0.f, 0.f};
            }
            color.fill(list.Get(), rgb.data(), size_t(w) * 4);
            depth.fill(list.Get(), z.data(), size_t(w) * 4);
            motion.fill(list.Get(), mv.data(), size_t(w) * 8);
            const float one = 1.f; exposure.fill(list.Get(), &one, sizeof(one));
            output.transition(list.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            StarfoxDlssFrameV1 native{};native.size=sizeof(native);native.viewport=UINT(mode);native.frame_index=frame_index;
            native.width=w;native.height=h;native.output_width=1280;native.output_height=720;native.reset=c.reset==sl::Boolean::eTrue;
            native.command=list.Get();native.color=color.texture.Get();native.depth=depth.texture.Get();native.motion=motion.texture.Get();
            native.output=output.texture.Get();native.exposure=exposure.texture.Get();
            native.states[0]=UINT(color.state);native.states[1]=UINT(depth.state);native.states[2]=UINT(motion.state);
            native.states[3]=UINT(output.state);native.states[4]=UINT(exposure.state);
            auto copy_matrix=[](float* dst,const sl::float4x4& m) {
                for(unsigned row=0;row<4;++row) {dst[row*4]=m[row].x;dst[row*4+1]=m[row].y;dst[row*4+2]=m[row].z;dst[row*4+3]=m[row].w;}
            };
            copy_matrix(native.view_to_clip,c.cameraViewToClip);copy_matrix(native.clip_to_view,c.clipToCameraView);
            copy_matrix(native.clip_to_previous,c.clipToPrevClip);copy_matrix(native.previous_to_clip,c.prevClipToClip);
            native.camera_up[1]=1;native.camera_right[0]=1;native.camera_forward[2]=1;
            native.near_plane=c.cameraNear;native.far_plane=c.cameraFar;native.vertical_fov=c.cameraFOV;native.aspect=c.cameraAspectRatio;
            native.jitter[0]=c.jitterOffset.x;native.jitter[1]=c.jitterOffset.y;
            char error[512]{};
            if(frame==0) {
                auto invalid=native;invalid.motion=invalid.depth;
                if(!starfox_dlss_evaluate_v1(module,&invalid,error,sizeof(error))) throw std::runtime_error("Aliased temporal inputs accepted");
                invalid=native;invalid.size=0;
                if(!starfox_dlss_evaluate_v1(module,&invalid,error,sizeof(error))) throw std::runtime_error("Bad frame ABI accepted");
            }
            if(starfox_dlss_evaluate_v1(module,&native,error,sizeof(error))) throw std::runtime_error(error);
            // SDK restores tagged states. No application dispatch follows it;
            // the next operation explicitly supplies its copy state.
            output.transition(list.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
            src.pResource = output.texture.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = readback.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = output.footprint;
            list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            check(list->Close(), "Close evaluation list");
            ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1, lists);
            check(swap->Present(0, 0), "Present evaluation frame");
            check(queue->Signal(fence.Get(), ++serial), "Signal evaluation fence");
            check(fence->SetEventOnCompletion(serial, event.handle), "Arm evaluation fence");
            if (WaitForSingleObject(event.handle, 30000) != WAIT_OBJECT_0)
                throw std::runtime_error("DLSS GPU timeout; output is not verified");
            check(device->GetDeviceRemovedReason(), "Device removed");
            void* data{}; D3D12_RANGE read{0, SIZE_T(output.bytes)};
            check(readback->Map(0, &read, &data), "Map evaluated output");
            std::uint64_t hash = 14695981039346656037ull;
            std::vector<char> ppm; ppm.reserve(1280 * 720 * 3);
            unsigned red_pixels = 0;
            for (UINT y = 0; y < 720; ++y) for (UINT x = 0; x < 1280; ++x) {
                const auto* p = static_cast<const unsigned char*>(data) + output.footprint.Offset + size_t(y) * output.footprint.Footprint.RowPitch + x * 4;
                for (unsigned channel = 0; channel < 3; ++channel) { hash = (hash ^ p[channel]) * 1099511628211ull; ppm.push_back(char(p[channel])); }
                red_pixels += p[0] > 120 && p[0] > unsigned(p[1]) * 2;
            }
            D3D12_RANGE none{}; readback->Unmap(0, &none);
            if (red_pixels < 40000 || red_pixels > 180000) throw std::runtime_error("DLSS output failed foreground-area sanity check");
            changed += frame > 0 && hash != last_hash; last_hash = hash;
            if (frame == 0 || frame == 15 || frame == 16 || frame == 31) {
                std::ofstream image(directory / ("mode-" + std::to_string(unsigned(mode)) + "-frame-" + std::to_string(frame) + ".ppm"), std::ios::binary);
                image << "P6\n1280 720\n255\n"; image.write(ppm.data(), ppm.size());
                if (!image) throw std::runtime_error("Cannot write evaluation image");
            }
            report << "mode=" << unsigned(mode) << " frame=" << frame << " reset=" << (c.reset == sl::Boolean::eTrue)
                << " input=" << w << 'x' << h << " hash=" << hash << " foreground=" << red_pixels << '\n';
        }
        if (changed != 31) throw std::runtime_error("DLSS output is unexpectedly stale");
        check(free_resources(sl::kFeatureDLSS, viewport), "Free evaluated viewport");
        std::cout << "DLSS evaluated: mode=" << unsigned(mode) << " input=" << w << 'x' << h
            << " output=1280x720 frames=32 resets=2 changing-frames=" << changed << '\n';
    }
    report << "PASS: 128 GPU-completed DLSS frames; analytical fixture, not gameplay or DLSS5.\n";
    if (!report) throw std::runtime_error("Cannot finalize evaluation report");
}
