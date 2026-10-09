// Included only by the native-GPU diagnostic, within its fixture namespace.
// CPU mapping is compositor verification, never an application presentation path.
template<class T> T vulkan_proc(const char* name) {
    const auto& api=*gpu_mock.vk_native;auto p=api.get_device_proc(api.device,name);
    check(p,name);return reinterpret_cast<T>(p);
}
std::unique_ptr<VulkanObject> vulkan_object() {
    check(gpu_mock.vk_native,"Vulkan fixture requires the SDL device");
    auto object=std::make_unique<VulkanObject>();object->device=gpu_mock.vk_native->device;object->get=gpu_mock.vk_native->get_device_proc;return object;
}
void vulkan_memory(VulkanObject& object,VkMemoryRequirements need,VkMemoryPropertyFlags flags) {
    const auto& api=*gpu_mock.vk_native;
    auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(api.get_instance_proc(api.instance,"vkGetPhysicalDeviceMemoryProperties"));
    check(properties,"Vulkan fixture memory properties unavailable");VkPhysicalDeviceMemoryProperties memory{};properties(api.physical_device,&memory);
    unsigned type=memory.memoryTypeCount;
    for(unsigned i=0;i<memory.memoryTypeCount;++i) if((need.memoryTypeBits&(1U<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags) {type=i;break;}
    check(type<memory.memoryTypeCount,"Vulkan fixture memory type unavailable");
    VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};info.allocationSize=need.size;info.memoryTypeIndex=type;
    check(vulkan_proc<PFN_vkAllocateMemory>("vkAllocateMemory")(object.device,&info,nullptr,&object.memory)==VK_SUCCESS,"Vulkan fixture memory allocation failed");
}
std::unique_ptr<VulkanObject> vulkan_image(unsigned width,unsigned height,VkFormat format) {
    auto object=vulkan_object();VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;
    info.format=format;info.extent={width,height,1};info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;
    info.tiling=VK_IMAGE_TILING_OPTIMAL;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    info.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    check(vulkan_proc<PFN_vkCreateImage>("vkCreateImage")(object->device,&info,nullptr,&object->image)==VK_SUCCESS,"Vulkan fixture image failed");
    VkMemoryRequirements need{};vulkan_proc<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(object->device,object->image,&need);
    vulkan_memory(*object,need,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vulkan_proc<PFN_vkBindImageMemory>("vkBindImageMemory")(object->device,object->image,object->memory,0)==VK_SUCCESS,"Vulkan fixture image bind failed");return object;
}
std::unique_ptr<VulkanObject> vulkan_buffer(unsigned bytes) {
    auto object=vulkan_object();VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=bytes;
    info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vulkan_proc<PFN_vkCreateBuffer>("vkCreateBuffer")(object->device,&info,nullptr,&object->buffer)==VK_SUCCESS,"Vulkan compositor buffer failed");
    VkMemoryRequirements need{};vulkan_proc<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(object->device,object->buffer,&need);
    vulkan_memory(*object,need,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check(vulkan_proc<PFN_vkBindBufferMemory>("vkBindBufferMemory")(object->device,object->buffer,object->memory,0)==VK_SUCCESS,"Vulkan compositor buffer bind failed");return object;
}
std::unique_ptr<VulkanObject> vulkan_command() {
    auto object=vulkan_object();VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=gpu_mock.vk_native->queue_family;
    check(vulkan_proc<PFN_vkCreateCommandPool>("vkCreateCommandPool")(object->device,&pool,nullptr,&object->pool)==VK_SUCCESS,"Vulkan compositor pool failed");
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};alloc.commandPool=object->pool;alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;alloc.commandBufferCount=1;
    check(vulkan_proc<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(object->device,&alloc,&object->command)==VK_SUCCESS,"Vulkan compositor command failed");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vulkan_proc<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(object->command,&begin)==VK_SUCCESS,"Vulkan compositor begin failed");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    check(vulkan_proc<PFN_vkCreateFence>("vkCreateFence")(object->device,&fence,nullptr,&object->fence)==VK_SUCCESS,"Vulkan compositor fence failed");return object;
}
void vulkan_submit(VulkanObject& object) {
    check(vulkan_proc<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(object.command)==VK_SUCCESS,"Vulkan compositor end failed");
    VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&object.command;
    check(vulkan_proc<PFN_vkQueueSubmit>("vkQueueSubmit")(gpu_mock.vk_queue,1,&info,object.fence)==VK_SUCCESS,"Vulkan compositor submit failed");
}
void vulkan_wait(VulkanObject& object) {
    check(vulkan_proc<PFN_vkWaitForFences>("vkWaitForFences")(object.device,1,&object.fence,VK_TRUE,5000000000ULL)==VK_SUCCESS,"Vulkan compositor fence did not retire");
}
VkImageMemoryBarrier vulkan_barrier(VkImage image) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=image;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};return barrier;
}
void vulkan_prepare_images(const std::vector<std::unique_ptr<VulkanObject>>& images) {
    auto command=vulkan_command();
    for(const auto& image:images) {
        auto barrier=vulkan_barrier(image->image);barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vulkan_proc<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(command->command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,0,nullptr,0,nullptr,1,&barrier);
    }
    vulkan_submit(*command);vulkan_wait(*command);
}
void vulkan_consume_projection() {
    auto& read=gpu_mock.vk_readback;check(!read.pending,"Vulkan compositor reused a pending readback");read={};read.command=vulkan_command();
    for(unsigned eye=0;eye<2;++eye) {
        const auto& image=gpu_mock.eyes[eye];read.buffers[eye]=vulkan_buffer(image.info.width*image.info.height*4);
        auto barrier=vulkan_barrier(image.vk_images[image.submitted_index]->image);
        barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        auto pipeline=vulkan_proc<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        pipeline(read.command->command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={image.info.width,image.info.height,1};
        vulkan_proc<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(read.command->command,barrier.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            read.buffers[eye]->buffer,1,&copy);
        std::swap(barrier.oldLayout,barrier.newLayout);barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.buffer=read.buffers[eye]->buffer;host.size=VK_WHOLE_SIZE;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        pipeline(read.command->command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_HOST_BIT,
            0,0,nullptr,1,&host,1,&barrier);
    }
    vulkan_submit(*read.command);read.pending=true;
}
std::array<std::vector<unsigned char>,2> vulkan_read_pixels() {
    auto& read=gpu_mock.vk_readback;check(read.pending,"Vulkan compositor received no complete projection");vulkan_wait(*read.command);
    std::array<std::vector<unsigned char>,2> pixels;
    for(unsigned eye=0;eye<2;++eye) {
        void* mapped{};auto& buffer=*read.buffers[eye];
        check(vulkan_proc<PFN_vkMapMemory>("vkMapMemory")(buffer.device,buffer.memory,0,VK_WHOLE_SIZE,0,&mapped)==VK_SUCCESS,"Vulkan compositor map failed");
        const auto bytes=gpu_mock.eyes[eye].info.width*gpu_mock.eyes[eye].info.height*4;
        pixels[eye].assign(static_cast<unsigned char*>(mapped),static_cast<unsigned char*>(mapped)+bytes);
        vulkan_proc<PFN_vkUnmapMemory>("vkUnmapMemory")(buffer.device,buffer.memory);
    }
    read={};gpu_mock.retired_vk_images.clear();return pixels;
}
void vulkan_verify_pixels(const std::array<std::vector<unsigned char>,2>& expected) {
    check(vulkan_read_pixels()==expected,"Vulkan compositor received changed/crossed/stale eye pixels");
}
XrResult XRAPI_CALL vulkan_requirements(XrInstance instance,XrSystemId system,XrGraphicsRequirementsVulkan2KHR* out) {
    check(instance==current && system==42 && out->type==XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR,"Bad Vulkan requirements request");
    ++gpu_mock.requirements;out->minApiVersionSupported=XR_MAKE_VERSION(1,1,0);out->maxApiVersionSupported=XR_MAKE_VERSION(1,3,0);return XR_SUCCESS;
}
XrResult XRAPI_CALL vulkan_create_instance(XrInstance instance,const XrVulkanInstanceCreateInfoKHR* info,VkInstance* out,VkResult* result) {
    check(instance==current && info->type==XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR && info->systemId==42 && !info->next
        && !info->createFlags && info->pfnGetInstanceProcAddr && info->vulkanCreateInfo && gpu_mock.requirements,"Bad Vulkan2 creation request");
    gpu_mock.vk_get=info->pfnGetInstanceProcAddr;
    auto create=reinterpret_cast<PFN_vkCreateInstance>(gpu_mock.vk_get(nullptr,"vkCreateInstance"));check(create,"Vulkan creation entry unavailable");
    *result=create(info->vulkanCreateInfo,info->vulkanAllocator,out);if(*result==VK_SUCCESS) {gpu_mock.vk_instance=*out;++gpu_mock.vk_instance_creates;}
    return XR_SUCCESS;
}
XrResult XRAPI_CALL vulkan_get_physical(XrInstance instance,const XrVulkanGraphicsDeviceGetInfoKHR* info,VkPhysicalDevice* out) {
    check(instance==current && info->type==XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR && info->systemId==42 && !info->next
        && info->vulkanInstance==gpu_mock.vk_instance,"Bad Vulkan2 physical-device request");
    auto enumerate=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(gpu_mock.vk_get(info->vulkanInstance,"vkEnumeratePhysicalDevices"));
    auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(gpu_mock.vk_get(info->vulkanInstance,"vkGetPhysicalDeviceProperties"));
    unsigned count{};check(enumerate && properties && enumerate(info->vulkanInstance,&count,nullptr)==VK_SUCCESS && count && count<1024,"Vulkan devices unavailable");
    std::vector<VkPhysicalDevice> devices(count);check(enumerate(info->vulkanInstance,&count,devices.data())==VK_SUCCESS,"Vulkan device enumeration failed");
    *out=VK_NULL_HANDLE;
    for(const auto device:devices) {
        VkPhysicalDeviceProperties prop{};properties(device,&prop);
        if(prop.deviceType==(gpu_mock.integrated?VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)) {*out=device;break;}
    }
    check(*out,"Requested Vulkan fixture adapter unavailable");
    gpu_mock.vk_physical=*out;return XR_SUCCESS;
}
XrResult XRAPI_CALL vulkan_create_device(XrInstance instance,const XrVulkanDeviceCreateInfoKHR* info,VkDevice* out,VkResult* result) {
    check(instance==current && info->type==XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR && info->systemId==42 && !info->next
        && !info->createFlags && info->vulkanPhysicalDevice==gpu_mock.vk_physical && info->pfnGetInstanceProcAddr==gpu_mock.vk_get
        && info->vulkanCreateInfo->queueCreateInfoCount==1,"Bad Vulkan2 device creation request");
    const auto create=reinterpret_cast<PFN_vkCreateDevice>(gpu_mock.vk_get(gpu_mock.vk_instance,"vkCreateDevice"));check(create,"Vulkan device creation entry unavailable");
    *result=create(info->vulkanPhysicalDevice,info->vulkanCreateInfo,info->vulkanAllocator,out);
    if(*result==VK_SUCCESS) {
        gpu_mock.vk_device=*out;gpu_mock.vk_family=info->vulkanCreateInfo->pQueueCreateInfos[0].queueFamilyIndex;++gpu_mock.vk_device_creates;
        auto queue=reinterpret_cast<PFN_vkGetDeviceQueue>(gpu_mock.vk_get(gpu_mock.vk_instance,"vkGetDeviceQueue"));
        check(queue,"Vulkan queue entry unavailable");queue(*out,gpu_mock.vk_family,0,&gpu_mock.vk_queue);
    }
    return XR_SUCCESS;
}
