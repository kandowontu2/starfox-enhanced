#include "starfox/vr/vulkan_eye_targets.hpp"
#include <stdexcept>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto result=reinterpret_cast<T>(get(device,name));
    if(!result) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return result;
}
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(result));
}
}
VulkanEyeTargets::~VulkanEyeTargets() {close();}
void VulkanEyeTargets::close() noexcept {
    for(auto& eye:frames_) {for(auto frame:eye) destroy_frame_(device_,frame,nullptr);eye.clear();}
    for(auto& eye:views_) {for(auto view:eye) destroy_view_(device_,view,nullptr);eye.clear();}
    if(pass_) destroy_pass_(device_,pass_,nullptr);
    pass_={};device_={};extents_={};depth_=false;
}
VkFramebuffer VulkanEyeTargets::framebuffer(unsigned eye,unsigned image) const noexcept {
    return eye<2 && image<frames_[eye].size()?frames_[eye][image]:VK_NULL_HANDLE;
}
bool VulkanEyeTargets::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkFormat format,
    const std::array<std::span<const VkImage>,2>& images,const std::array<VkExtent2D,2>& extents,
    VkFormat depth_format,const std::array<VkImageView,2>& depth_views) {
    close();
    try {
        if(!device || !get || format==VK_FORMAT_UNDEFINED) throw std::runtime_error("Invalid eye target device/format");
        depth_=depth_format!=VK_FORMAT_UNDEFINED;
        if(depth_ != bool(depth_views[0]) || (!images[1].empty() && depth_ != bool(depth_views[1])))
            throw std::runtime_error("Depth format and both eye depth views must be provided together");
        for(unsigned eye=0;eye<2;++eye) {
            if(eye==1 && images[eye].empty()) continue; // mono composition layer
            if(images[eye].empty() || images[eye].size()>64 || !extents[eye].width || !extents[eye].height)
                throw std::runtime_error("Invalid eye target images/dimensions");
            for(auto image:images[eye]) if(!image) throw std::runtime_error("Null eye image");
        }
        device_=device;
        destroy_view_=entry<PFN_vkDestroyImageView>(get,device,"vkDestroyImageView");
        destroy_frame_=entry<PFN_vkDestroyFramebuffer>(get,device,"vkDestroyFramebuffer");
        destroy_pass_=entry<PFN_vkDestroyRenderPass>(get,device,"vkDestroyRenderPass");
        const auto create_view=entry<PFN_vkCreateImageView>(get,device,"vkCreateImageView");
        const auto create_frame=entry<PFN_vkCreateFramebuffer>(get,device,"vkCreateFramebuffer");
        const auto create_pass=entry<PFN_vkCreateRenderPass>(get,device,"vkCreateRenderPass");
        std::array<VkAttachmentDescription,2> attachments{};
        auto& attachment=attachments[0];
        attachment.format=format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        attachments[1]=attachment;attachments[1].format=depth_format;
        attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const VkAttachmentReference depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;
        subpass.pDepthStencilAttachment=depth_?&depth:nullptr;
        VkSubpassDependency dependency{};dependency.srcSubpass=VK_SUBPASS_EXTERNAL;dependency.dstSubpass=0;
        dependency.srcStageMask=dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if(depth_) {
            dependency.srcStageMask|=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.dstStageMask|=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask|=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask|=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        pass.attachmentCount=depth_?2:1;pass.pAttachments=attachments.data();pass.subpassCount=1;pass.pSubpasses=&subpass;
        pass.dependencyCount=1;pass.pDependencies=&dependency;
        check(create_pass(device,&pass,nullptr,&pass_),"Create eye render pass");
        for(unsigned eye=0;eye<2;++eye) {
            views_[eye].reserve(images[eye].size());frames_[eye].reserve(images[eye].size());
            for(auto image:images[eye]) {
                VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                view.image=image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;
                view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                VkImageView handle{};check(create_view(device,&view,nullptr,&handle),"Create eye image view");
                views_[eye].push_back(handle);
                VkFramebufferCreateInfo frame{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                const std::array<VkImageView,2> frame_views{views_[eye].back(),depth_views[eye]};
                frame.renderPass=pass_;frame.attachmentCount=depth_?2:1;frame.pAttachments=frame_views.data();
                frame.width=extents[eye].width;frame.height=extents[eye].height;frame.layers=1;
                VkFramebuffer target{};check(create_frame(device,&frame,nullptr,&target),"Create eye framebuffer");
                frames_[eye].push_back(target);
            }
        }
        extents_=extents;status_="Vulkan eye targets ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
}
