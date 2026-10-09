#include "starfox/render/sdl_d3d12_bridge.h"
#include "../src/app/leia_sr_host.hpp"
#include <SDL3/SDL.h>
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* error) {if(!ok) throw std::runtime_error(error);}
struct Draw {ID3D12Device* device; ID3D12DescriptorHeap* heap;unsigned calls{};bool fail{};};
bool clear(void* opaque,void* commands,void* input,void* output,uint32_t w,uint32_t h,uint32_t format) {
    auto& draw=*static_cast<Draw*>(opaque);
    D3D12_RESOURCE_DESC source{},target{};
#if defined(__MINGW32__)
    static_cast<ID3D12Resource*>(input)->GetDesc(&source);
    static_cast<ID3D12Resource*>(output)->GetDesc(&target);
#else
    source=static_cast<ID3D12Resource*>(input)->GetDesc();target=static_cast<ID3D12Resource*>(output)->GetDesc();
#endif
    if(input==output || source.Width!=uint64_t(w)*2 || source.Height!=h || target.Width!=w || target.Height!=h
        || source.Format!=DXGI_FORMAT_B8G8R8A8_UNORM || target.Format!=source.Format || format!=uint32_t(target.Format)) return false;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
#if defined(__MINGW32__)
    draw.heap->GetCPUDescriptorHandleForHeapStart(&rtv);
#else
    rtv=draw.heap->GetCPUDescriptorHandleForHeapStart();
#endif
    draw.device->CreateRenderTargetView(static_cast<ID3D12Resource*>(output),nullptr,rtv);
    // Avoid half-integer UNORM rounding ties in driver ClearRenderTargetView.
    const float color[]{64.f/255,128.f/255,191.f/255,1};
    static_cast<ID3D12GraphicsCommandList*>(commands)->ClearRenderTargetView(rtv,color,0,nullptr);
    ++draw.calls;return !draw.fail;
}
}
int main(int argc,char** argv) try {
    require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    auto* device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,false,"direct3d12");require(device,SDL_GetError());
    const auto props=SDL_GetGPUDeviceProperties(device);
    auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_DEVICE,nullptr));
    auto* bridge=static_cast<const StarfoxSdlD3D12OwnedWeaveV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_OWNED_WEAVE,nullptr));
    require(native && bridge && bridge->version==1 && bridge->weave,"Owned-weave bridge missing");
    if(argc>1 && std::string_view(argv[1])=="--probe") {
        // Asset-free factory probe on each actual display. Never weave onto
        // the swapchain or claim an initialized factory is working output.
        int count{};auto* displays=SDL_GetDisplays(&count);require(displays && count>0,SDL_GetError());
        unsigned available{};
        for(int i=0;i<count;++i) {
            auto* window=SDL_CreateWindow("SR display probe",64,32,SDL_WINDOW_HIDDEN);require(window,SDL_GetError());
            require(SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED_DISPLAY(displays[i]),SDL_WINDOWPOS_CENTERED_DISPLAY(displays[i])),SDL_GetError());
            auto* hwnd=SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
            {
                LeiaSrHost host;
                const bool initialized=host.ensure(native,hwnd);
                available+=initialized;
                std::cout<<"Display "<<i+1<<" ("<<SDL_GetDisplayName(displays[i])<<"): "
                    <<(initialized?"factory initialized: ":"unavailable: ")<<host.status()<<'\n';
                host.release();
            }
            SDL_DestroyWindow(window);
        }
        SDL_free(displays);require(SDL_WaitForGPUIdle(device),SDL_GetError());SDL_DestroyGPUDevice(device);SDL_Quit();
        std::cout<<"Factory probe only: no game image or native weave was presented; no settings changed.\n";
        return available?0:3;
    }
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;
    require(SUCCEEDED(native->CreateDescriptorHeap(&hd,IID_ID3D12DescriptorHeap,reinterpret_cast<void**>(heap.GetAddressOf()))),"RTV allocation failed");
    Draw draw{native,heap.Get()};unsigned cases{};
    for(const auto extent:{std::array<unsigned,2>{64,32},{128,48}}) {
        const auto w=extent[0],h=extent[1];
        SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        ti.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        ti.width=w*2;ti.height=h;ti.layer_count_or_depth=1;ti.num_levels=1;
        auto* input=SDL_CreateGPUTexture(device,&ti);ti.width=w;
        auto* output=SDL_CreateGPUTexture(device,&ti);ti.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        auto* wrong=SDL_CreateGPUTexture(device,&ti);require(input && output && wrong,SDL_GetError());
        SDL_GPUTransferBufferCreateInfo bi{};bi.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;bi.size=w*h*4;
        auto* readback=SDL_CreateGPUTransferBuffer(device,&bi);require(readback,SDL_GetError());
        for(unsigned iteration=0;iteration<3;++iteration) {
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
            const auto before=draw.calls;
            require(!bridge->weave(command,input,input,w,h,clear,&draw),"Aliased weave accepted");
            require(!bridge->weave(command,input,wrong,w,h,clear,&draw),"RGBA weave accepted");
            require(!bridge->weave(command,input,output,w-1,h,clear,&draw),"Odd extent accepted");
            require(draw.calls==before,"Invalid request reached callback");
            draw.fail=true;
            require(!bridge->weave(command,input,output,w,h,clear,&draw),"Callback failure lost");
            draw.fail=false;
            require(bridge->weave(command,input,output,w,h,clear,&draw),SDL_GetError());
            auto* pass=SDL_BeginGPUCopyPass(command);
            SDL_GPUTextureRegion region{};region.texture=output;region.w=w;region.h=h;region.d=1;
            SDL_GPUTextureTransferInfo dest{};dest.transfer_buffer=readback;dest.pixels_per_row=w;dest.rows_per_layer=h;
            SDL_DownloadFromGPUTexture(pass,&region,&dest);SDL_EndGPUCopyPass(pass);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
            require(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
            const auto* pixels=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device,readback,false));require(pixels,SDL_GetError());
            for(unsigned i=0;i<w*h;++i) if(!(pixels[i*4]==191 && pixels[i*4+1]==128 && pixels[i*4+2]==64 && pixels[i*4+3]==255)) {
                std::cerr<<"extent="<<w<<'x'<<h<<" iteration="<<iteration<<" pixel="<<i<<" BGRA="
                    <<unsigned(pixels[i*4])<<','<<unsigned(pixels[i*4+1])<<','<<unsigned(pixels[i*4+2])<<','<<unsigned(pixels[i*4+3])<<'\n';
                require(false,"Owned BGRA weave pixels/state restoration failed");
            }
            SDL_UnmapGPUTransferBuffer(device,readback);++cases;
        }
        auto* cancel=SDL_AcquireGPUCommandBuffer(device);require(cancel,SDL_GetError());
        require(bridge->weave(cancel,input,output,w,h,clear,&draw),SDL_GetError());require(SDL_CancelGPUCommandBuffer(cancel),SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device,readback);SDL_ReleaseGPUTexture(device,input);SDL_ReleaseGPUTexture(device,output);SDL_ReleaseGPUTexture(device,wrong);
    }
    // Matching formats/extents are not enough: both textures must belong to
    // the exact SDL device whose command buffer records the vendor weave.
    auto* foreign_device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,false,"direct3d12");require(foreign_device,SDL_GetError());
    SDL_GPUTextureCreateInfo foreign_info{};foreign_info.type=SDL_GPU_TEXTURETYPE_2D;
    foreign_info.format=SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    foreign_info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    foreign_info.width=128;foreign_info.height=32;foreign_info.layer_count_or_depth=1;foreign_info.num_levels=1;
    auto* local_source=SDL_CreateGPUTexture(device,&foreign_info);
    auto* foreign_source=SDL_CreateGPUTexture(foreign_device,&foreign_info);foreign_info.width=64;
    auto* local_target=SDL_CreateGPUTexture(device,&foreign_info);
    auto* foreign_target=SDL_CreateGPUTexture(foreign_device,&foreign_info);
    require(local_source && foreign_source && local_target && foreign_target,SDL_GetError());
    auto* foreign_command=SDL_AcquireGPUCommandBuffer(device);require(foreign_command,SDL_GetError());
    const auto before_foreign=draw.calls;
    require(!bridge->weave(foreign_command,local_source,foreign_target,64,32,clear,&draw),"Foreign-device target accepted");
    require(!bridge->weave(foreign_command,foreign_source,local_target,64,32,clear,&draw),"Foreign-device source accepted");
    require(draw.calls==before_foreign,"Foreign-device texture reached callback");
    require(SDL_CancelGPUCommandBuffer(foreign_command),SDL_GetError());
    SDL_ReleaseGPUTexture(device,local_source);SDL_ReleaseGPUTexture(device,local_target);
    SDL_ReleaseGPUTexture(foreign_device,foreign_source);SDL_ReleaseGPUTexture(foreign_device,foreign_target);
    require(SDL_WaitForGPUIdle(foreign_device),SDL_GetError());SDL_DestroyGPUDevice(foreign_device);
    const bool legacy=argc>1 && std::string_view(argv[1])=="--expect-legacy-unavailable";
    if(argc>1 && (std::string_view(argv[1])=="--expect-unavailable" || legacy)) {
        const auto module=LoadLibraryExW(L"starfox_leia_sr.dll",nullptr,LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        require(module,"Adapter did not load; missing-runtime test must exercise the real adapter");
        const auto get=reinterpret_cast<StarfoxLeiaSrGetApi>(GetProcAddress(module,"starfox_leia_sr_get_api"));
        const auto status=reinterpret_cast<StarfoxLeiaSrGetStatus>(GetProcAddress(module,"starfox_leia_sr_status"));
        require(legacy?!status:status && status() && std::string_view(status()).size(),
            "Expected old/no-status or new/diagnostic adapter export contract differs");
        require(get && !get(2) && get(1) && get(1)->version==1 && get(1)->size==sizeof(StarfoxLeiaSrApiV1),"Adapter C ABI/version invalid");
        require(!get(1)->create(nullptr,nullptr) && get(1)->destroy(nullptr),"Adapter null creation/cleanup contract failed");
        auto* window=SDL_CreateWindow("SR optional runtime check",64,32,SDL_WINDOW_HIDDEN);require(window,SDL_GetError());
        auto* hwnd=SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
        LeiaSrHost host;
        for(unsigned i=0;i<3;++i) {
            require(!host.ensure(native,hwnd),"Unexpected SR display/runtime; do not run this mode on SR hardware");
            require(!host.status().empty() && (host.status().find("legacy adapter")!=std::string_view::npos)==legacy,
                "Native/legacy adapter failure reason was not propagated through the loader");
            std::cout<<"SR unavailable reason: "<<host.status()<<'\n';host.release();
        }
        require(!host.available(),"Unavailable SR host retained");SDL_DestroyWindow(window);
        FreeLibrary(module);
        std::cout<<"Optional adapter missing-runtime creation/re-arm passed\n";
    }
    require(SDL_WaitForGPUIdle(device),SDL_GetError());heap.Reset();SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<"Owned SR weave: "<<cases<<" exact BGRA outputs, repeated/resize, rejection (including foreign devices), callback-failure restoration and cancellation passed\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
