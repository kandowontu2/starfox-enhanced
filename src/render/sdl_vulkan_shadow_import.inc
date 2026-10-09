// Windows-only native Vulkan import, owned by SdlDxrShadows. Caller finishes
// all copies before reset; no graphics-pipeline or CPU pixel storage is needed.
struct SdlVulkanShadowImport {
    const StarfoxSdlVulkanBridgeV2* bridge{};
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    VkSemaphore ready{};
    VkSemaphore done{};
    ~SdlVulkanShadowImport() {reset();}
    void reset() {
        if(!bridge) return;
        const auto device=bridge->device;const auto get=bridge->get_device_proc;
        if(ready) reinterpret_cast<PFN_vkDestroySemaphore>(get(device,"vkDestroySemaphore"))(device,ready,nullptr);
        if(done) reinterpret_cast<PFN_vkDestroySemaphore>(get(device,"vkDestroySemaphore"))(device,done,nullptr);
        if(buffer) reinterpret_cast<PFN_vkDestroyBuffer>(get(device,"vkDestroyBuffer"))(device,buffer,nullptr);
        if(memory) reinterpret_cast<PFN_vkFreeMemory>(get(device,"vkFreeMemory"))(device,memory,nullptr);
        ready={};done={};buffer={};memory={};
    }
    void open(DxrShadows& producer,uint64_t bytes,bool geometry=false) {
        reset();const auto device=bridge->device;const auto get=bridge->get_device_proc;
        auto require=[](bool value,const char* message){if(!value) throw std::runtime_error(message);};
#define LOAD(name) auto name=reinterpret_cast<PFN_##name>(get(device,#name));require(name,#name " unavailable")
        LOAD(vkCreateBuffer);LOAD(vkGetBufferMemoryRequirements);LOAD(vkGetMemoryWin32HandlePropertiesKHR);
        LOAD(vkAllocateMemory);LOAD(vkBindBufferMemory);LOAD(vkCreateSemaphore);LOAD(vkImportSemaphoreWin32HandleKHR);
#undef LOAD
        VkExternalMemoryBufferCreateInfo external{};external.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
        external.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
        VkBufferCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;info.pNext=&external;
        info.size=bytes;info.usage=geometry?VK_BUFFER_USAGE_TRANSFER_DST_BIT:VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        require(vkCreateBuffer(device,&info,nullptr,&buffer)==VK_SUCCESS,"Vulkan external buffer creation failed");
        VkMemoryRequirements need{};vkGetBufferMemoryRequirements(device,buffer,&need);
        struct Handle {HANDLE value;~Handle(){if(value) CloseHandle(value);}};
        Handle resource{static_cast<HANDLE>(geometry?producer.export_geometry_handle():producer.export_resident_handle())};require(resource.value,"DXR resource export failed");
        VkMemoryWin32HandlePropertiesKHR allowed{};allowed.sType=VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
        require(vkGetMemoryWin32HandlePropertiesKHR(device,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
            resource.value,&allowed)==VK_SUCCESS,"Vulkan external memory query failed");
        const auto bits=need.memoryTypeBits&allowed.memoryTypeBits;require(bits,"No Vulkan import memory type");
        unsigned type=0;while(!(bits&(1U<<type))) ++type;
        VkImportMemoryWin32HandleInfoKHR import{};import.sType=VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
        import.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;import.handle=resource.value;
        VkMemoryDedicatedAllocateInfo dedicated{};dedicated.sType=VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;dedicated.pNext=&import;dedicated.buffer=buffer;
        VkMemoryAllocateInfo allocate{};allocate.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;allocate.pNext=&dedicated;
        allocate.allocationSize=need.size;allocate.memoryTypeIndex=type;
        require(vkAllocateMemory(device,&allocate,nullptr,&memory)==VK_SUCCESS
            && vkBindBufferMemory(device,buffer,memory,0)==VK_SUCCESS,"Vulkan memory import failed");
        for(unsigned index=0;index<(geometry?2U:1U);++index) {
        auto& imported_fence=index?done:ready;
        Handle fence{static_cast<HANDLE>(index?producer.export_geometry_completion_handle():producer.export_ready_fence_handle())};require(fence.value,"DXR fence export failed");
        VkSemaphoreTypeCreateInfo timeline{};timeline.sType=VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;timeline.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo semaphore{};semaphore.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;semaphore.pNext=&timeline;
        require(vkCreateSemaphore(device,&semaphore,nullptr,&imported_fence)==VK_SUCCESS,"Vulkan timeline creation failed");
        VkImportSemaphoreWin32HandleInfoKHR sync{};sync.sType=VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
        sync.semaphore=imported_fence;sync.handleType=VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;sync.handle=fence.value;
        require(vkImportSemaphoreWin32HandleKHR(device,&sync)==VK_SUCCESS,"Vulkan fence import failed");
        }
    }
};
