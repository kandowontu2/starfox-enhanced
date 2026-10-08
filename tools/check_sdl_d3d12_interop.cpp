#include "starfox/render/dxr_shadows.hpp"
#include "starfox/render/sdl_d3d12_bridge.h"
#include "starfox/render/sdl_gpu_effects.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include <SDL3/SDL.h>
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

static void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
static bool native_copy_callback(void* user,void* command,void* const* resources,uint32_t count) {
    if(count!=2) return false;
    auto* list=static_cast<ID3D12GraphicsCommandList*>(command);
    auto* source=static_cast<ID3D12Resource*>(resources[0]);auto* target=static_cast<ID3D12Resource*>(resources[1]);
    D3D12_RESOURCE_BARRIER barriers[2]{};
    for(unsigned i=0;i<2;++i) {
        barriers[i].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[i].Transition.pResource=i?target:source;barriers[i].Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[i].Transition.StateBefore=i?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[i].Transition.StateAfter=i?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    list->ResourceBarrier(2,barriers);list->CopyResource(target,source);
    for(auto& barrier:barriers) std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);
    list->ResourceBarrier(2,barriers);
    *static_cast<bool*>(user)=true;
    return true;
}
static void check_texture_bridge(SDL_GPUDevice* device, ID3D12Device* native, SDL_PropertiesID props) {
    auto* bridge=static_cast<const StarfoxSdlD3D12TextureBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_TEXTURE_BRIDGE,nullptr));
    require(bridge && bridge->version==1,"Texture bridge unavailable");
    auto* compute=static_cast<const StarfoxSdlD3D12ComputeBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_COMPUTE_BRIDGE,nullptr));
    require(compute && compute->version==1,"Native compute bridge unavailable");
    for(unsigned format=0;format<3;++format) for(unsigned width : {37u,128u,259u}) {
        const unsigned height=23, bytes=width*height*(format==2?8:4);
        SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        if(format) ti.format=format==1?SDL_GPU_TEXTUREFORMAT_R32_FLOAT:SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
        ti.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;ti.width=width;ti.height=height;ti.layer_count_or_depth=1;ti.num_levels=1;
        auto* source=SDL_CreateGPUTexture(device,&ti);auto* target=SDL_CreateGPUTexture(device,&ti);
        require(source && target,SDL_GetError());
        SDL_GPUTransferBufferCreateInfo bi{};bi.size=bytes;bi.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        auto* upload=SDL_CreateGPUTransferBuffer(device,&bi);bi.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        auto* download=SDL_CreateGPUTransferBuffer(device,&bi);require(upload && download,SDL_GetError());
        std::vector<unsigned char> expected(bytes);for(unsigned i=0;i<bytes;++i) expected[i]=static_cast<unsigned char>((i*73+i/width)%251);
        auto* mapped=SDL_MapGPUTransferBuffer(device,upload,false);require(mapped,SDL_GetError());
        std::memcpy(mapped,expected.data(),bytes);SDL_UnmapGPUTransferBuffer(device,upload);
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;
        desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=format==0?DXGI_FORMAT_R8G8B8A8_UNORM:format==1?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R32G32_FLOAT;desc.SampleDesc.Count=1;
        Microsoft::WRL::ComPtr<ID3D12Resource> external,wrong;
        require(SUCCEEDED(native->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_ID3D12Resource,reinterpret_cast<void**>(external.GetAddressOf()))),"Native texture allocation failed");
        ++desc.Width;
        require(SUCCEEDED(native->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_ID3D12Resource,reinterpret_cast<void**>(wrong.GetAddressOf()))),"Mismatch texture allocation failed");
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        auto* pass=SDL_BeginGPUCopyPass(command);
        SDL_GPUTextureTransferInfo transfer{};transfer.transfer_buffer=upload;transfer.pixels_per_row=width;transfer.rows_per_layer=height;
        SDL_GPUTextureRegion region{};region.texture=source;region.w=width;region.h=height;region.d=1;
        SDL_UploadToGPUTexture(pass,&transfer,&region,false);SDL_EndGPUCopyPass(pass);
        require(!bridge->copy_to_external(command,source,wrong.Get()),"Mismatched texture accepted");
        require(!bridge->copy_from_external(command,nullptr,target),"Null texture accepted");
        require(bridge->copy_to_external(command,source,external.Get()),SDL_GetError());
        require(bridge->copy_from_external(command,external.Get(),target),SDL_GetError());
        bool called=false;void* aliases[]{source,source};void* native_inputs[]{source,target};
        require(!compute->dispatch(command,aliases,2,1,native_copy_callback,&called) && !called,"Aliased native compute accepted");
        require(!compute->dispatch(command,native_inputs,2,2,native_copy_callback,&called) && !called,"Invalid output index accepted");
        require(compute->dispatch(command,native_inputs,2,1,native_copy_callback,&called) && called,SDL_GetError());
        pass=SDL_BeginGPUCopyPass(command);region.texture=target;transfer.transfer_buffer=download;
        SDL_DownloadFromGPUTexture(pass,&region,&transfer);SDL_EndGPUCopyPass(pass);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
        require(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
        mapped=SDL_MapGPUTransferBuffer(device,download,false);require(mapped,SDL_GetError());
        const bool equal=std::memcmp(mapped,expected.data(),bytes)==0;SDL_UnmapGPUTransferBuffer(device,download);
        SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
        SDL_ReleaseGPUTexture(device,source);SDL_ReleaseGPUTexture(device,target);
        require(equal,"Native texture round trip changed pixels");
    }
    std::cout << "Native texture/compute bridges: 9 exact RGBA/R32/RG32 round trips, native commands, and invalid-input checks passed\n";
}
static void check_timestamp_bridge(SDL_GPUDevice* device,SDL_PropertiesID props) {
    const auto* timing=static_cast<const StarfoxSdlD3D12TimestampsV1*>(SDL_GetPointerProperty(
        props,STARFOX_SDL_D3D12_TIMESTAMPS,nullptr));
    require(timing && timing->version==1,"Timestamp bridge unavailable");
    std::uint64_t frequency{};
    require(!timing->create(device,0,&frequency) && !timing->create(device,4097,&frequency)
        && !timing->create(device,8,nullptr),"Invalid timestamp allocation accepted");
    auto* context=timing->create(device,8,&frequency);
    require(context && frequency,"Timestamp allocation/frequency failed");
    unsigned resolved=0;
    // A cancelled list must not consume/reuse a submitted slot or resolve an
    // unwritten index. Next lists rewrite all queried indices before resolve.
    auto* cancelled=SDL_AcquireGPUCommandBuffer(device);require(cancelled,SDL_GetError());
    require(timing->write(cancelled,context,0),"Cancelled timestamp write failed");
    require(!timing->write(cancelled,context,8) && !timing->write(nullptr,context,0)
        && !timing->resolve(cancelled,context,7,2) && !timing->resolve(cancelled,context,0,0),
        "Invalid timestamp command/range accepted");
    SDL_CancelGPUCommandBuffer(cancelled);
    for(unsigned trial=0;trial<16;++trial) {
        std::array<SDL_GPUFence*,2> fences{};
        for(unsigned pair=0;pair<2;++pair) {
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
            for(unsigned q=0;q<4;++q) require(timing->write(command,context,pair*4+q),"Timestamp write failed");
            require(timing->resolve(command,context,pair*4,4),"Timestamp resolve failed");
            fences[pair]=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fences[pair],SDL_GetError());
        }
        require(SDL_WaitForGPUFences(device,true,fences.data(),2),SDL_GetError());
        for(unsigned pair=0;pair<2;++pair) {
            std::array<std::uint64_t,4> ticks{};
            require(timing->read(context,pair*4,4,ticks.data()),"Fence-retired timestamp read failed");
            require(ticks[1]>=ticks[0] && ticks[2]>=ticks[1] && ticks[3]>=ticks[2],"Timestamp query indices mixed or unordered");
            require(ticks[3]-ticks[0]==(ticks[1]-ticks[0])+(ticks[2]-ticks[1])+(ticks[3]-ticks[2]),"Timestamp phases do not sum to command time");
            SDL_ReleaseGPUFence(device,fences[pair]);++resolved;
        }
    }
    std::uint64_t tick{};
    require(!timing->read(context,8,1,&tick) && !timing->read(context,7,2,&tick)
        && !timing->read(context,0,1,nullptr),"Invalid timestamp read accepted");
    timing->destroy(context);
    std::cout<<"D3D12 timestamp bridge: "<<resolved<<" retired query sets, cancellation/reuse and invalid ranges passed; frequency="<<frequency<<"\n";
}
int main() {
    using namespace starfox::render::shadows;
    SDL_GPUDevice* device{};SDL_GPUBuffer* buffer{};SDL_GPUTransferBuffer* download{};
    int result=0;
    try {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,true,"direct3d12");
        require(device,SDL_GetError());
        const auto props=SDL_GetGPUDeviceProperties(device);
        check_timestamp_bridge(device,props);
        auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_DEVICE,nullptr));
        auto* bridge=static_cast<const StarfoxSdlD3D12BridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_BRIDGE,nullptr));
        require(native && bridge && bridge->version==2 && bridge->wait_fence,"Pinned native SDL bridge is unavailable");
        check_texture_bridge(device,native,props);
        LUID luid{};
#if defined(__MINGW32__)
        native->GetAdapterLuid(&luid);
#else
        luid=native->GetAdapterLuid();
#endif
        std::array<std::uint8_t,8> identity{};std::memcpy(identity.data(),&luid,8);
        DxrShadows producer(identity);
        SdlDxrShadows owned;
        require(!producer.working_image_bytes() && !owned.working_image_bytes(),
            "Unused ray metadata query allocated or invented output images");
        require(producer.available(),producer.status().c_str());
        {
            Scene casters;casters.add({{-12,-12,20},{12,-12,20},{0,12,20}});casters.build();
            const ReceiverPlane plane{{0,0,40},{0,0,1}};
            std::vector<std::uint8_t> actual,expected;
            const Camera camera{37,23,30,18.5,11.5,26};
            render_mask(casters,camera,{0,0,-1},plane,expected,nullptr,true,true);
            require(owned.render_resident(device,casters,camera,{0,0,-1},plane,nullptr,true),owned.status().c_str());
            require(owned.readback(actual) && actual==expected,"SDL DXR underlay lost plane shadows");
            require(!owned.render_resident(device,casters,camera,{0,0,-1},{},nullptr,true)
                && !owned.output().buffer,"SDL DXR underlay retained stale no-plane output");
            require(owned.render_resident(device,casters,camera,{0,0,-1},plane,nullptr,true)
                && owned.readback(actual) && actual==expected,"SDL DXR underlay failed recovery");
            std::cout<<"SDL DXR ground-only receiver transfer and invalidation passed\n";
        }
        std::uint64_t owned_native_capacity=1024; // 40-byte rows x 23, aligned to 256 above.
        for(unsigned frame=0;frame<12;++frame) {
            const unsigned width=frame%3==0?133:frame%3==1?400:129,height=frame%3==1?224:79;
            Scene scene;
            const double x=double(frame)-6;
            scene.add({{x-15,-12,100},{x+15,-12,100},{x,18,100}});
            Camera camera{width,height,128.,width*.5,height*.5};
            require(producer.render_resident(scene,camera,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},nullptr,nullptr,true),producer.status().c_str());
            const auto output=producer.resident_output();
            const auto bytes=output.row_bytes*output.height;
            HANDLE shared=static_cast<HANDLE>(producer.export_resident_handle());
            require(shared,"DXR shared output export failed");
            Microsoft::WRL::ComPtr<ID3D12Resource> imported;
            const auto opened=native->OpenSharedHandle(shared,IID_ID3D12Resource,reinterpret_cast<void**>(imported.GetAddressOf()));
            CloseHandle(shared);
            require(SUCCEEDED(opened),"SDL device could not open DXR output");
            SDL_GPUBufferCreateInfo info{};info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;info.size=bytes;
            buffer=SDL_CreateGPUBuffer(device,&info);require(buffer,SDL_GetError());
            SDL_GPUTransferBufferCreateInfo transfer{};transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=bytes;
            download=SDL_CreateGPUTransferBuffer(device,&transfer);require(download,SDL_GetError());
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
            require(!bridge->copy_buffer(command,imported.Get(),buffer,bytes+4),"Oversized native copy was accepted");
            require(!bridge->copy_buffer(command,nullptr,buffer,bytes),"Null native resource was accepted");
            require(!bridge->copy_buffer(command,imported.Get(),buffer,0),"Empty native copy was accepted");
            require(bridge->copy_buffer(command,imported.Get(),buffer,bytes),SDL_GetError());
            // The transfer below is verification only. Runtime consumers bind
            // the SDL storage buffer without mapping either producer or result.
            auto* pass=SDL_BeginGPUCopyPass(command);
            SDL_GPUBufferRegion source{buffer,0,bytes};
            SDL_GPUTransferBufferLocation destination{download,0};
            SDL_DownloadFromGPUBuffer(pass,&source,&destination);SDL_EndGPUCopyPass(pass);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,SDL_GetError());
            require(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());SDL_ReleaseGPUFence(device,fence);
            std::vector<std::uint8_t> reference;
            require(producer.readback_resident(reference),producer.status().c_str());
            D3D12_RESOURCE_DESC retained{};
#if defined(__MINGW32__)
            static_cast<ID3D12Resource*>(output.resource)->GetDesc(&retained);
#else
            retained=static_cast<ID3D12Resource*>(output.resource)->GetDesc();
#endif
            require(producer.working_image_bytes()==retained.Width*2,
                "Native image accounting omitted aligned/grown output or diagnostic readback");
            auto* pixels=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,download,false));require(pixels,SDL_GetError());
            bool equal=true;unsigned shaded=0;
            for(unsigned y=0;y<height;++y) for(unsigned xpixel=0;xpixel<width;++xpixel) {
                equal&=pixels[y*output.row_bytes+xpixel]==reference[y*width+xpixel];
                shaded+=reference[y*width+xpixel]!=0;
            }
            SDL_UnmapGPUTransferBuffer(device,download);
            require(equal && shaded,"Native SDL copy differs from DXR mask or test scene is blank");
            starfox::render::Framebuffer frame_buffer(width,height+4);
            frame_buffer.enable_layer_tags(true);
            for(unsigned y=0;y<frame_buffer.height();++y) for(unsigned px=0;px<width;++px)
                frame_buffer.set_stored(px,y,1,static_cast<starfox::render::PixelLayer>(px%5));
            std::vector<std::uint8_t> expected(frame_buffer.pixels().size()*4,200),actual=expected;
            starfox::render::GpuEffectSettings settings;
            settings.shadow_width=width;settings.shadow_height=height;settings.shadow_offset_y=int(frame%5)-2;
            settings.shadow_mask=reference;
            starfox::render::SdlGpuEffects effects;
            require(effects.apply(device,frame_buffer,expected,settings),effects.status().c_str());
            settings.shadow_mask={};settings.resident_shadow={device,buffer,width,height,output.row_bytes};
            require(effects.apply(device,frame_buffer,actual,settings),effects.status().c_str());
            require(actual==expected,"Packed resident shadow blend differs from upload path");
            settings.resident_shadow.packed_row_bytes+=4;
            require(!effects.apply(device,frame_buffer,actual,settings),"Invalid packed row stride was accepted");
            effects.release_device();
            if(!owned.render_resident(device,scene,camera,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}}))
                throw std::runtime_error("Owned frame "+std::to_string(frame)+": "+owned.status());
            owned_native_capacity=std::max(owned_native_capacity,(std::uint64_t(bytes)+255)&~std::uint64_t(255));
            // All adjacent cases change the SDL copy extent, retiring its old
            // optional download while retaining the producer's grown output.
            require(owned.working_image_bytes()==owned_native_capacity+bytes,
                "SDL image accounting omitted native capacity or double-counted an imported alias");
            std::vector<std::uint8_t> owned_mask;
            // Consume before any diagnostic readback: a CPU wait here would
            // conceal a missing producer -> SDL queue dependency.
            settings.resident_shadow=owned.output();actual.assign(expected.size(),200);
            require(effects.apply(device,frame_buffer,actual,settings) && actual==expected,"Owned mask effects mismatch");
            require(owned.readback(owned_mask) && owned_mask==reference,"Owned native mask differs after reuse/resize");
            require(owned.working_image_bytes()==owned_native_capacity+2*bytes,
                "SDL image accounting omitted the optional diagnostic download");
            if(frame==5) {
                owned.release_device();
                owned_native_capacity=0;
                require(!owned.working_image_bytes(),"Released native/SDL image storage remained charged");
                require(!owned.output().buffer && SDL_GetGPUShaderFormats(device),"Owner release destroyed borrowed device or retained output");
                require(!owned.readback(owned_mask) && owned_mask.empty(),"Released mask remained readable");
            }
            if(frame==8) {
                auto invalid_camera=camera;invalid_camera.width=0;
                require(!owned.render_resident(device,scene,invalid_camera,{-1,-1,-1},std::nullopt)
                    && !owned.output().buffer,"Invalid render retained an owned mask");
                owned_native_capacity=0;
                require(!owned.working_image_bytes(),"Failed legacy ray owner retained charged image storage");
            }
            effects.release_device();
            SDL_ReleaseGPUTransferBuffer(device,download);download=nullptr;
            SDL_ReleaseGPUBuffer(device,buffer);buffer=nullptr;
        }
        // Replace an unread submission, then destroy with work still pending.
        // Neither path may release an imported source before SDL's copy ends.
        for(unsigned frame=0;frame<8;++frame) {
            Scene scene;scene.add({{-15,-12,100},{15,-12,100},{0,18,100}});
            Camera camera{133+frame*4,79,128.,66.5+frame*2,39.5};
            if(!owned.render_resident(device,scene,camera,{-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}}))
                throw std::runtime_error("Unread replacement: "+owned.status());
            if(frame==3) owned.release_device();
        }
        owned.release_device();
        std::cout<<"DXR -> native shared resource -> SDL storage buffer: 12 animated/resized masks and effects blends exact; invalid copies/strides rejected. Diagnostic readback only.\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';result=1;}
    if(device) {
        SDL_WaitForGPUIdle(device);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        SDL_DestroyGPUDevice(device);
    }
    SDL_Quit();return result;
}
