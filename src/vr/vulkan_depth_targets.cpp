#include "starfox/vr/vulkan_depth_targets.hpp"
#include <stdexcept>
namespace starfox::vr {
namespace {
template<class T> T function(PFN_vkVoidFunction fn) {
    if(!fn) throw std::runtime_error("Missing Vulkan depth entry point");return reinterpret_cast<T>(fn);
}
void check(VkResult value,const char* operation) {
    if(value!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(value));
}
}
VulkanDepthTargets::~VulkanDepthTargets() {close();}
void VulkanDepthTargets::close() noexcept {
    for(unsigned eye=0;eye<2;++eye) {
        if(views_[eye]) destroy_view_(device_,views_[eye],nullptr);
        if(images_[eye]) destroy_image_(device_,images_[eye],nullptr);
        if(memory_[eye]) free_(device_,memory_[eye],nullptr);
    }
    views_={};images_={};memory_={};device_={};format_=VK_FORMAT_UNDEFINED;
}
bool VulkanDepthTargets::initialize(VkInstance instance,VkPhysicalDevice physical,VkDevice device,
    PFN_vkGetInstanceProcAddr get,const std::array<VkExtent2D,2>& extents) {
    close();
    try {
        if(!instance || !physical || !device || !get) throw std::runtime_error("Missing depth device");
        const auto get_device=function<PFN_vkGetDeviceProcAddr>(get(instance,"vkGetDeviceProcAddr"));
        const auto get_format=function<PFN_vkGetPhysicalDeviceFormatProperties>(get(instance,"vkGetPhysicalDeviceFormatProperties"));
        const auto get_memory=function<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance,"vkGetPhysicalDeviceMemoryProperties"));
        const auto get_properties=function<PFN_vkGetPhysicalDeviceProperties>(get(instance,"vkGetPhysicalDeviceProperties"));
        VkPhysicalDeviceProperties properties{};get_properties(physical,&properties);
        for(auto extent:extents) if(!extent.width || !extent.height || extent.width>properties.limits.maxImageDimension2D
            || extent.height>properties.limits.maxImageDimension2D) throw std::runtime_error("Invalid eye depth dimensions");
        for(auto candidate:{VK_FORMAT_D32_SFLOAT,VK_FORMAT_D16_UNORM}) {
            VkFormatProperties support{};get_format(physical,candidate,&support);
            if(support.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {format_=candidate;break;}
        }
        if(format_==VK_FORMAT_UNDEFINED) throw std::runtime_error("No supported eye depth format");
        VkPhysicalDeviceMemoryProperties memory_properties{};get_memory(physical,&memory_properties);
        if(memory_properties.memoryTypeCount>VK_MAX_MEMORY_TYPES) throw std::runtime_error("Invalid memory type count");
        device_=device;
#define DEVICE(type,name) function<type>(get_device(device,name))
        destroy_view_=DEVICE(PFN_vkDestroyImageView,"vkDestroyImageView");
        destroy_image_=DEVICE(PFN_vkDestroyImage,"vkDestroyImage");
        free_=DEVICE(PFN_vkFreeMemory,"vkFreeMemory");
        const auto create=DEVICE(PFN_vkCreateImage,"vkCreateImage");
        const auto requirements=DEVICE(PFN_vkGetImageMemoryRequirements,"vkGetImageMemoryRequirements");
        const auto allocate=DEVICE(PFN_vkAllocateMemory,"vkAllocateMemory");
        const auto bind=DEVICE(PFN_vkBindImageMemory,"vkBindImageMemory");
        const auto create_view=DEVICE(PFN_vkCreateImageView,"vkCreateImageView");
#undef DEVICE
        for(unsigned eye=0;eye<2;++eye) {
            VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            image.imageType=VK_IMAGE_TYPE_2D;image.format=format_;image.extent={extents[eye].width,extents[eye].height,1};
            image.mipLevels=image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;
            image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            check(create(device,&image,nullptr,&images_[eye]),"Create eye depth image");
            VkMemoryRequirements required{};requirements(device,images_[eye],&required);
            uint32_t type=VK_MAX_MEMORY_TYPES;
            for(uint32_t i=0;i<memory_properties.memoryTypeCount;++i) if(required.memoryTypeBits&(1U<<i)) {
                type=i;if(memory_properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) break;
            }
            if(type==VK_MAX_MEMORY_TYPES || !required.size) throw std::runtime_error("No compatible eye depth memory");
            VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memory.allocationSize=required.size;memory.memoryTypeIndex=type;
            check(allocate(device,&memory,nullptr,&memory_[eye]),"Allocate eye depth memory");
            check(bind(device,images_[eye],memory_[eye],0),"Bind eye depth memory");
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view.image=images_[eye];view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format_;
            view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
            check(create_view(device,&view,nullptr,&views_[eye]),"Create eye depth view");
        }
        status_="Per-eye Vulkan depth targets ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
}
