// Included after Vulkan, SDL, bridge, and require declarations.
#include "starfox/render/dxr_shadows.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include <vector>
#include <array>

static void check_shadow_copy(SDL_GPUDevice* device,const StarfoxSdlVulkanBridgeV2& bridge,
                              const uint8_t* luid) {
    using namespace starfox::render::shadows;
    std::array<uint8_t,8> identity{};std::memcpy(identity.data(),luid,8);
    DxrShadows producer(identity);require(producer.available(),producer.status().c_str());
    SdlDxrShadows owned;
    auto get=bridge.get_device_proc;auto native=bridge.device;
#define LOAD(name) auto name=reinterpret_cast<PFN_##name>(get(native,#name));require(name,#name " unavailable")
    LOAD(vkCreateBuffer);LOAD(vkDestroyBuffer);LOAD(vkGetBufferMemoryRequirements);
    LOAD(vkGetMemoryWin32HandlePropertiesKHR);LOAD(vkAllocateMemory);LOAD(vkFreeMemory);
    LOAD(vkBindBufferMemory);LOAD(vkCreateSemaphore);LOAD(vkDestroySemaphore);LOAD(vkImportSemaphoreWin32HandleKHR);
#undef LOAD
    for(unsigned frame=0;frame<6;++frame) {
        struct Cleanup {
            SDL_GPUDevice* device;VkDevice native;
            PFN_vkDestroyBuffer destroy_buffer;PFN_vkFreeMemory free_memory;PFN_vkDestroySemaphore destroy_semaphore;
            VkBuffer source{};VkDeviceMemory memory{};VkSemaphore ready{};
            SDL_GPUBuffer* target{};SDL_GPUTransferBuffer* download{};SDL_GPUCommandBuffer* command{};
            ~Cleanup() {
                if(command) SDL_CancelGPUCommandBuffer(command);
                SDL_WaitForGPUIdle(device);
                if(target) SDL_ReleaseGPUBuffer(device,target);
                if(download) SDL_ReleaseGPUTransferBuffer(device,download);
                if(ready) destroy_semaphore(native,ready,nullptr);
                if(source) destroy_buffer(native,source,nullptr);
                if(memory) free_memory(native,memory,nullptr);
            }
        } cleanup{device,native,vkDestroyBuffer,vkFreeMemory,vkDestroySemaphore};
        Scene scene;const double x=double(frame)*2;
        scene.add({{x-15,-12,100},{x+15,-12,100},{x,18,100}});
        const unsigned width=frame%2?400:133,height=frame%2?224:79;
        if(!producer.render_resident(scene,{width,height,128.,width*.5,height*.5},
            {-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}},nullptr,nullptr,true,true))
            throw std::runtime_error(producer.status());
        HANDLE fence=static_cast<HANDLE>(producer.export_ready_fence_handle());
        require(fence,"Producer fence export failed");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};semaphore.pNext=&type;
        auto result=vkCreateSemaphore(native,&semaphore,nullptr,&cleanup.ready);
        if(result!=VK_SUCCESS) {CloseHandle(fence);throw std::runtime_error("Import semaphore creation failed");}
        VkImportSemaphoreWin32HandleInfoKHR sync{VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR};
        sync.semaphore=cleanup.ready;sync.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;sync.handle=fence;
        result=vkImportSemaphoreWin32HandleKHR(native,&sync);CloseHandle(fence);
        require(result==VK_SUCCESS,"Producer semaphore import failed");
        const auto output=producer.resident_output();const uint32_t bytes=output.row_bytes*height;
        VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
        external.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.pNext=&external;
        info.size=bytes;info.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        require(vkCreateBuffer(native,&info,nullptr,&cleanup.source)==VK_SUCCESS,"Native import buffer failed");
        VkMemoryRequirements need{};vkGetBufferMemoryRequirements(native,cleanup.source,&need);
        HANDLE resource=static_cast<HANDLE>(producer.export_resident_handle());require(resource,"Producer resource export failed");
        VkMemoryWin32HandlePropertiesKHR allowed{VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR};
        result=vkGetMemoryWin32HandlePropertiesKHR(native,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,resource,&allowed);
        auto bits=need.memoryTypeBits&allowed.memoryTypeBits;
        if(result!=VK_SUCCESS || !bits) {CloseHandle(resource);throw std::runtime_error("No import memory type");}
        unsigned memory_type=0;while(!(bits&(1U<<memory_type))) ++memory_type;
        VkImportMemoryWin32HandleInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};
        import.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;import.handle=resource;
        VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.pNext=&import;dedicated.buffer=cleanup.source;
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.pNext=&dedicated;
        allocate.allocationSize=need.size;allocate.memoryTypeIndex=memory_type;
        result=vkAllocateMemory(native,&allocate,nullptr,&cleanup.memory);CloseHandle(resource);
        require(result==VK_SUCCESS && vkBindBufferMemory(native,cleanup.source,cleanup.memory,0)==VK_SUCCESS,"Memory import/bind failed");
        SDL_GPUBufferCreateInfo target{};target.size=bytes;target.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
        cleanup.target=SDL_CreateGPUBuffer(device,&target);require(cleanup.target,SDL_GetError());
        SDL_GPUTransferBufferCreateInfo download{};download.size=bytes;download.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        cleanup.download=SDL_CreateGPUTransferBuffer(device,&download);require(cleanup.download,SDL_GetError());
        cleanup.command=SDL_AcquireGPUCommandBuffer(device);require(cleanup.command,SDL_GetError());
        require(!bridge.copy_external(cleanup.command,cleanup.source,bytes,cleanup.target,bytes+4),"Oversized copy accepted");
        require(!bridge.copy_external(cleanup.command,VK_NULL_HANDLE,bytes,cleanup.target,bytes),"Null source accepted");
        require(bridge.wait_timeline(device,cleanup.ready,output.ready_value),SDL_GetError());
        require(bridge.copy_external(cleanup.command,cleanup.source,bytes,cleanup.target,bytes),SDL_GetError());
        auto* pass=SDL_BeginGPUCopyPass(cleanup.command);require(pass,SDL_GetError());
        SDL_GPUBufferRegion source{cleanup.target,0,bytes};SDL_GPUTransferBufferLocation dest{cleanup.download,0};
        SDL_DownloadFromGPUBuffer(pass,&source,&dest);SDL_EndGPUCopyPass(pass);
        auto* done=SDL_SubmitGPUCommandBufferAndAcquireFence(cleanup.command);cleanup.command=nullptr;require(done,SDL_GetError());
        const auto waited=SDL_WaitForGPUFences(device,true,&done,1);SDL_ReleaseGPUFence(device,done);require(waited,SDL_GetError());
        // Readback only after the queued consumer: no producer CPU wait masks a missing dependency.
        std::vector<uint8_t> reference;require(producer.readback_resident(reference),producer.status().c_str());
        auto* pixels=static_cast<const uint8_t*>(SDL_MapGPUTransferBuffer(device,cleanup.download,false));require(pixels,SDL_GetError());
        bool equal=true;unsigned shaded=0;
        for(unsigned y=0;y<height;++y) for(unsigned xpixel=0;xpixel<width;++xpixel) {
            equal&=pixels[y*output.row_bytes+xpixel]==reference[y*width+xpixel];shaded+=reference[y*width+xpixel]!=0;
        }
        SDL_UnmapGPUTransferBuffer(device,cleanup.download);
        require(equal && shaded,"External Vulkan copy differs from DXR output");
        if(!owned.render_resident(device,scene,{width,height,128.,width*.5,height*.5},
            {-1,-1,-1},ReceiverPlane{{0,25,0},{0,1,0}})) throw std::runtime_error(owned.status());
        std::vector<uint8_t> owned_pixels;
        require(owned.readback(owned_pixels) && owned_pixels==reference,"Vulkan shadow owner differs after resize/reuse");
        if(frame==2) owned.release_device();
    }
    std::cout<<"Six animated/resized DXR -> imported Vulkan -> SDL masks exact; producer waits stay on GPU.\n";
}
