// Native color images stand in for acquired OpenXR swapchain images. No XR
// runtime/panel is needed; this tests the actual SDL device, queue and barriers.
// CPU transfer below is fixture verification only, never a presentation path.
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include "starfox/render/sdl_d3d12_bridge.h"
#include "starfox/render/sdl_vulkan_bridge.h"
#ifdef STARFOX_DISPLAYXR_BINDING
#include "starfox/render/displayxr_d3d12_binding.hpp"
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>
#endif
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Gpu {
    SDL_GPUDevice* device{};
    SDL_GPUTexture *source{},*target{};
    SDL_GPUTransferBuffer *upload{},*download{};
    SDL_GPUCommandBuffer* command{};
    SDL_GPUFence* fence{};
    void release_frame() {
        if(command) {SDL_CancelGPUCommandBuffer(command);command=nullptr;}
        if(fence) {SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);fence=nullptr;}
        SDL_WaitForGPUIdle(device);
        if(source) SDL_ReleaseGPUTexture(device,source);
        if(target) SDL_ReleaseGPUTexture(device,target);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        source=target=nullptr;upload=download=nullptr;
    }
    ~Gpu() {if(device) {release_frame();SDL_DestroyGPUDevice(device);} SDL_Quit();}
};
struct VulkanImage {
    VkDevice device{};VkImage image{};VkDeviceMemory memory{};
    PFN_vkDestroyImage destroy{};PFN_vkFreeMemory free{};
    ~VulkanImage() {if(image) destroy(device,image,nullptr);if(memory) free(device,memory,nullptr);}
};
std::unique_ptr<VulkanImage> create_vulkan_image(const StarfoxSdlVulkanBridgeV2& api,VkFormat format,
                                                unsigned width,unsigned height) {
    auto get=api.get_device_proc;auto device=api.device;
#define LOAD(name) auto name=reinterpret_cast<PFN_##name>(get(device,#name));require(name,#name " missing")
    LOAD(vkCreateImage);LOAD(vkDestroyImage);LOAD(vkGetImageMemoryRequirements);
    LOAD(vkAllocateMemory);LOAD(vkFreeMemory);LOAD(vkBindImageMemory);
#undef LOAD
    auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        api.get_instance_proc(api.instance,"vkGetPhysicalDeviceMemoryProperties"));require(properties,"Memory properties missing");
    auto result=std::make_unique<VulkanImage>();result->device=device;result->destroy=vkDestroyImage;result->free=vkFreeMemory;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={width,height,1};
    info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;
    info.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    require(vkCreateImage(device,&info,nullptr,&result->image)==VK_SUCCESS,"Native color image creation failed");
    VkMemoryRequirements need{};vkGetImageMemoryRequirements(device,result->image,&need);
    VkPhysicalDeviceMemoryProperties memory{};properties(api.physical_device,&memory);
    unsigned type=memory.memoryTypeCount;
    for(unsigned i=0;i<memory.memoryTypeCount;++i)
        if((need.memoryTypeBits&(1U<<i)) && (memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {type=i;break;}
    require(type<memory.memoryTypeCount,"No native image memory type");
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize=need.size;allocation.memoryTypeIndex=type;
    require(vkAllocateMemory(device,&allocation,nullptr,&result->memory)==VK_SUCCESS,"Native color memory allocation failed");
    require(vkBindImageMemory(device,result->image,result->memory,0)==VK_SUCCESS,"Native color memory bind failed");
    return result;
}
struct QueueCheck {void* expected{};unsigned calls{};bool accept{true},valid{true};};
bool d3d_queue(void* user,void* queue) {
    auto& check=*static_cast<QueueCheck*>(user);++check.calls;
    check.valid&=queue==check.expected;return check.accept && check.valid;
}
bool vulkan_queue(void* user,VkQueue queue) {
    auto& check=*static_cast<QueueCheck*>(user);++check.calls;
    check.valid&=reinterpret_cast<void*>(queue)==check.expected;return check.accept && check.valid;
}
struct QueueWork {
    void* expected{};ID3D12Fence* fence{};PFN_vkQueueSubmit submit{};unsigned value{};
};
bool signal_d3d(void* user,void* queue) {
    auto& work=*static_cast<QueueWork*>(user);
    return queue==work.expected && SUCCEEDED(static_cast<ID3D12CommandQueue*>(queue)->Signal(work.fence,++work.value));
}
bool submit_vulkan(void* user,VkQueue queue) {
    auto& work=*static_cast<QueueWork*>(user);
    if(reinterpret_cast<void*>(queue)!=work.expected) return false;
    ++work.value;return work.submit(queue,0,nullptr,VK_NULL_HANDLE)==VK_SUCCESS;
}
void check_concurrent_queue(Gpu& gpu,ID3D12Device* device,const StarfoxSdlD3D12XrBridgeV1* dx,
                            const StarfoxSdlVulkanBridgeV2* api,const StarfoxSdlVulkanXrBridgeV1* vk) {
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    struct Completion {SDL_GPUDevice* device;~Completion() {SDL_WaitForGPUIdle(device);}} completion{gpu.device};
    QueueWork work;
    if(dx) {
        require(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_ID3D12Fence,
            reinterpret_cast<void**>(fence.GetAddressOf()))),"Native fence creation failed");
        work.expected=dx->queue(gpu.device);work.fence=fence.Get();
    } else {
        work.expected=reinterpret_cast<void*>(vk->queue(gpu.device));
        work.submit=reinterpret_cast<PFN_vkQueueSubmit>(api->get_device_proc(api->device,"vkQueueSubmit"));
        require(work.submit,"Native submit entry missing");
    }
    std::atomic<bool> valid{true};
    {
        std::jthread producer([&] {
            for(unsigned i=0;i<128;++i)
                if(!(dx?dx->with_queue(gpu.device,signal_d3d,&work):vk->with_queue(gpu.device,submit_vulkan,&work))) valid=false;
        });
        for(unsigned i=0;i<128;++i) {
            auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);require(command,SDL_GetError());
            require(SDL_SubmitGPUCommandBuffer(command),SDL_GetError());
        }
    }
    require(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
    require(valid && work.value==128 && (!fence || fence->GetCompletedValue()==128),
        "Concurrent native/SDL queue submissions failed");
}
#ifdef STARFOX_DISPLAYXR_BINDING
struct RequirementsMock {
    LUID adapter{};
    D3D_FEATURE_LEVEL level{D3D_FEATURE_LEVEL_11_0};
    XrResult result{XR_SUCCESS};
    bool missing{},null_proc{};
    unsigned queries{};
} requirements_mock;
const auto xr_instance=reinterpret_cast<XrInstance>(std::uintptr_t(0x100));
XrResult XRAPI_CALL requirements(XrInstance instance,XrSystemId system,XrGraphicsRequirementsD3D12KHR* out) {
    ++requirements_mock.queries;
    if(instance!=xr_instance || system!=77 || !out || out->type!=XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR || out->next)
        return XR_ERROR_VALIDATION_FAILURE;
    out->adapterLuid=requirements_mock.adapter;out->minFeatureLevel=requirements_mock.level;
    return requirements_mock.result;
}
XrResult XRAPI_CALL requirements_get(XrInstance instance,const char* name,PFN_xrVoidFunction* out) {
    *out=nullptr;
    if(instance!=xr_instance || requirements_mock.missing || std::string_view(name)!="xrGetD3D12GraphicsRequirementsKHR")
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    if(!requirements_mock.null_proc) *out=reinterpret_cast<PFN_xrVoidFunction>(requirements);
    return XR_SUCCESS;
}
void check_binding(ID3D12Device* device,void* queue) {
    using starfox::render::DisplayXrD3D12Binding;
    starfox::render::DisplayXrRuntime unavailable;
#if defined(__MINGW32__)
    device->GetAdapterLuid(&requirements_mock.adapter);
#else
    requirements_mock.adapter=device->GetAdapterLuid();
#endif
    DisplayXrD3D12Binding binding;
    require(!binding.initialize(unavailable,device,queue) && !binding.binding(),"Unconfirmed physical runtime accepted");
    require(binding.initialize_with_api(xr_instance,77,requirements_get,device,queue),binding.status().c_str());
    auto* selected=static_cast<const XrGraphicsBindingD3D12KHR*>(binding.binding());
    require(selected && selected->type==XR_TYPE_GRAPHICS_BINDING_D3D12_KHR && !selected->next
        && selected->device==device && selected->queue==queue && requirements_mock.queries==1,"Invalid negotiated graphics binding");
    ++requirements_mock.adapter.LowPart;
    require(!binding.initialize_with_api(xr_instance,77,requirements_get,device,queue) && !binding.binding(),"Wrong adapter accepted or stale binding retained");
    --requirements_mock.adapter.LowPart;requirements_mock.level=static_cast<D3D_FEATURE_LEVEL>(0x7fffffff);
    require(!binding.initialize_with_api(xr_instance,77,requirements_get,device,queue) && !binding.binding(),"Invalid feature level accepted");
    requirements_mock.level=D3D_FEATURE_LEVEL_11_0;requirements_mock.result=XR_ERROR_GRAPHICS_DEVICE_INVALID;
    require(!binding.initialize_with_api(xr_instance,77,requirements_get,device,queue) && !binding.binding(),"Runtime requirements failure accepted");
    requirements_mock.result=XR_SUCCESS;requirements_mock.missing=true;
    require(!binding.initialize_with_api(xr_instance,77,requirements_get,device,queue),"Missing requirement interface accepted");
    requirements_mock.missing=false;requirements_mock.null_proc=true;
    require(!binding.initialize_with_api(xr_instance,77,requirements_get,device,queue),"Null requirement dispatch accepted");
    requirements_mock.null_proc=false;
    D3D12_COMMAND_QUEUE_DESC description{};description.Type=D3D12_COMMAND_LIST_TYPE_COPY;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> copy_queue;
    require(SUCCEEDED(device->CreateCommandQueue(&description,IID_ID3D12CommandQueue,reinterpret_cast<void**>(copy_queue.GetAddressOf()))),"Copy queue allocation failed");
    require(!binding.initialize_with_api(xr_instance,77,requirements_get,device,copy_queue.Get()) && !binding.binding(),"Non-graphics queue accepted");
    require(!binding.initialize_with_api(xr_instance,77,requirements_get,device,nullptr)
        && !binding.initialize_with_api(XR_NULL_HANDLE,77,requirements_get,device,queue)
        && !binding.initialize_with_api(xr_instance,XR_NULL_SYSTEM_ID,requirements_get,device,queue),"Incomplete graphics input accepted");
    require(binding.initialize_with_api(xr_instance,77,requirements_get,device,queue),binding.status().c_str());
    binding.close();require(!binding.binding() && device->GetNodeCount()>0,"Close retained binding or released borrowed device");
    std::cout<<"D3D12 runtime requirements checked on the actual SDL device/queue; wrong adapter/feature/queue, missing dispatch and recovery checks passed\n";
}
#endif
}
int main(int argc,char** argv) {
    try {
        require(argc==2 && (std::string_view(argv[1])=="direct3d12" || std::string_view(argv[1])=="vulkan"),
            "Usage: starfox_sdl_xr_color_check direct3d12|vulkan");
        const bool d3d=std::string_view(argv[1])=="direct3d12";
        Gpu gpu;require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        gpu.device=SDL_CreateGPUDevice(d3d?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_SPIRV,true,argv[1]);
        require(gpu.device,SDL_GetError());const auto props=SDL_GetGPUDeviceProperties(gpu.device);
        auto* dx=static_cast<const StarfoxSdlD3D12XrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_XR_BRIDGE,nullptr));
        auto* vk=static_cast<const StarfoxSdlVulkanXrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_XR_BRIDGE,nullptr));
        auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_DEVICE,nullptr));
        auto* api=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
        auto* ray=static_cast<const StarfoxSdlVulkanRayBridgeV3*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_RAY_BRIDGE,nullptr));
        QueueCheck queue;
        if(d3d) {
            require(native && dx && dx->version==1 && dx->queue && dx->with_queue && dx->copy_to_color && dx->copy_from_color,"D3D12 XR bridge missing");
            auto* native_queue=static_cast<ID3D12CommandQueue*>(dx->queue(gpu.device));require(native_queue,"Missing D3D12 queue");
            Microsoft::WRL::ComPtr<ID3D12Device> owner;
            require(SUCCEEDED(native_queue->GetDevice(IID_ID3D12Device,reinterpret_cast<void**>(owner.GetAddressOf()))) && owner.Get()==native,
                "XR queue is not the SDL device's queue");
            D3D12_COMMAND_QUEUE_DESC desc{};
#if defined(__MINGW32__)
            native_queue->GetDesc(&desc);
#else
            desc=native_queue->GetDesc();
#endif
            require(desc.Type==D3D12_COMMAND_LIST_TYPE_DIRECT,"XR queue is not graphics capable");
            queue.expected=native_queue;
#ifdef STARFOX_DISPLAYXR_BINDING
            check_binding(native,native_queue);
#endif
            require(!dx->queue(nullptr) && !dx->with_queue(nullptr,d3d_queue,&queue)
                && !dx->with_queue(gpu.device,nullptr,&queue),"Invalid D3D12 queue input accepted");
            require(dx->with_queue(gpu.device,d3d_queue,&queue),"Queue callback failed");
            queue.accept=false;require(!dx->with_queue(gpu.device,d3d_queue,&queue),"Callback rejection lost");
            queue.accept=true;require(dx->with_queue(gpu.device,d3d_queue,&queue) && queue.calls==3,"Queue lock not restored");
        } else {
            require(api && api->version==2 && ray && ray->version==3 && vk && vk->version==1
                && vk->queue && vk->with_queue && vk->copy_to_color && vk->copy_from_color,"Vulkan XR bridge missing");
            auto get_queue=reinterpret_cast<PFN_vkGetDeviceQueue>(api->get_device_proc(api->device,"vkGetDeviceQueue"));
            require(get_queue,"Queue lookup missing");VkQueue expected{};get_queue(api->device,api->queue_family,0,&expected);
            require(expected && vk->queue(gpu.device)==expected,"XR queue is not the SDL queue");
            queue.expected=reinterpret_cast<void*>(expected);
            require(!vk->queue(nullptr) && !vk->with_queue(nullptr,vulkan_queue,&queue)
                && !vk->with_queue(gpu.device,nullptr,&queue),"Invalid Vulkan queue input accepted");
            require(vk->with_queue(gpu.device,vulkan_queue,&queue),"Queue callback failed");
            queue.accept=false;require(!vk->with_queue(gpu.device,vulkan_queue,&queue),"Callback rejection lost");
            queue.accept=true;require(vk->with_queue(gpu.device,vulkan_queue,&queue) && queue.calls==3,"Queue lock not restored");
        }
        check_concurrent_queue(gpu,native,dx,api,vk);
        struct Format {SDL_GPUTextureFormat sdl;DXGI_FORMAT dxgi;VkFormat vk;};
        const std::array<Format,4> formats{{
            {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_UNORM},
            {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,VK_FORMAT_R8G8B8A8_SRGB},
            {SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM},
            {SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,VK_FORMAT_B8G8R8A8_SRGB}}};
        unsigned checked{};
        for(const auto& format:formats) for(unsigned width:{37u,128u,259u}) {
            const unsigned height=23,bytes=width*height*4;
            Microsoft::WRL::ComPtr<ID3D12Resource> dx_color;
            std::unique_ptr<VulkanImage> vk_color;
            // Even a failed assertion after submission must finish GPU access
            // before the native image/memory owners above are destroyed.
            struct NativeLifetime {Gpu& gpu;~NativeLifetime() {gpu.release_frame();}} lifetime{gpu};
            if(d3d) {
                D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
                D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                desc.Width=width;desc.Height=height;desc.DepthOrArraySize=desc.MipLevels=1;
                desc.Format=format.dxgi;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
                require(SUCCEEDED(native->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_RENDER_TARGET,
                    nullptr,IID_ID3D12Resource,reinterpret_cast<void**>(dx_color.GetAddressOf()))),"Native D3D12 color allocation failed");
            } else vk_color=create_vulkan_image(*api,format.vk,width,height);
            // Two distinct eye payloads reuse each native image. Restoring the
            // required state/layout is necessary for the second eye to work.
            for(unsigned eye=0;eye<2;++eye) {
                SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=format.sdl;
                info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
                info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
                gpu.source=SDL_CreateGPUTexture(gpu.device,&info);gpu.target=SDL_CreateGPUTexture(gpu.device,&info);
                require(gpu.source && gpu.target,SDL_GetError());
                SDL_GPUTransferBufferCreateInfo transfer_info{};transfer_info.size=bytes;transfer_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                gpu.upload=SDL_CreateGPUTransferBuffer(gpu.device,&transfer_info);transfer_info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
                gpu.download=SDL_CreateGPUTransferBuffer(gpu.device,&transfer_info);require(gpu.upload && gpu.download,SDL_GetError());
                std::vector<unsigned char> expected(bytes);
                for(unsigned i=0;i<bytes;++i) expected[i]=static_cast<unsigned char>((i*73+i/width+eye*109)%251);
                auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,gpu.upload,false);require(mapped,SDL_GetError());
                std::memcpy(mapped,expected.data(),bytes);SDL_UnmapGPUTransferBuffer(gpu.device,gpu.upload);
                gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);require(gpu.command,SDL_GetError());
                auto* pass=SDL_BeginGPUCopyPass(gpu.command);require(pass,SDL_GetError());
                SDL_GPUTextureTransferInfo transfer{};transfer.transfer_buffer=gpu.upload;transfer.pixels_per_row=width;transfer.rows_per_layer=height;
                SDL_GPUTextureRegion region{};region.texture=gpu.source;region.w=width;region.h=height;region.d=1;
                SDL_UploadToGPUTexture(pass,&transfer,&region,false);SDL_EndGPUCopyPass(pass);
                if(d3d) {
                    require(!dx->copy_to_color(gpu.command,gpu.source,nullptr) && !dx->copy_from_color(gpu.command,nullptr,gpu.target),"Null color image accepted");
                    require(dx->copy_to_color(gpu.command,gpu.source,dx_color.Get()),SDL_GetError());
                    require(dx->copy_from_color(gpu.command,dx_color.Get(),gpu.target),SDL_GetError());
                } else {
                    if(!eye) {
                        auto barrier_fn=reinterpret_cast<PFN_vkCmdPipelineBarrier>(api->get_device_proc(api->device,"vkCmdPipelineBarrier"));require(barrier_fn,"Barrier entry missing");
                        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=vk_color->image;
                        barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                        barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                        barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                        barrier_fn(ray->command(gpu.command),VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            0,0,nullptr,0,nullptr,1,&barrier);
                    }
                    require(!vk->copy_to_color(gpu.command,gpu.source,VK_NULL_HANDLE,format.vk,width,height)
                        && !vk->copy_to_color(gpu.command,gpu.source,vk_color->image,format.vk,width+1,height)
                        && !vk->copy_to_color(gpu.command,gpu.source,vk_color->image,VK_FORMAT_R32_SFLOAT,width,height),"Invalid native color metadata accepted");
                    require(vk->copy_to_color(gpu.command,gpu.source,vk_color->image,format.vk,width,height),SDL_GetError());
                    require(vk->copy_from_color(gpu.command,vk_color->image,format.vk,width,height,gpu.target),SDL_GetError());
                }
                pass=SDL_BeginGPUCopyPass(gpu.command);require(pass,SDL_GetError());region.texture=gpu.target;transfer.transfer_buffer=gpu.download;
                SDL_DownloadFromGPUTexture(pass,&region,&transfer);SDL_EndGPUCopyPass(pass);
                gpu.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(gpu.command);gpu.command=nullptr;require(gpu.fence,SDL_GetError());
                require(SDL_WaitForGPUFences(gpu.device,true,&gpu.fence,1),SDL_GetError());
                mapped=SDL_MapGPUTransferBuffer(gpu.device,gpu.download,false);require(mapped,SDL_GetError());
                const bool exact=std::memcmp(mapped,expected.data(),bytes)==0;SDL_UnmapGPUTransferBuffer(gpu.device,gpu.download);
                require(exact,"Native color handoff changed an eye's pixels");++checked;
                gpu.release_frame();
            }
        }
        std::cout<<argv[1]<<": "<<checked<<" exact RGBA/BGRA linear/sRGB eye handoffs; queue identity, 128 concurrent native/SDL submissions, rejected callback recovery and invalid-input checks passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
