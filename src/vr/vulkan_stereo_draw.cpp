#include "starfox/vr/vulkan_stereo_draw.hpp"
namespace starfox::vr {
StereoRenderer::EyeResult VulkanStereoDraw::draw(unsigned eye,uint32_t image,
    const EyeCamera& camera,XrTime time,const VkClearColorValue& clear,const Record& record,const Record& before_render,
    const Record& after_render,const VulkanEyeCommands::TimelineWait* wait) {
    using Result=StereoRenderer::EyeResult;
    if(submitted_ && (eye!=eye_ || image!=image_)) return Result::fatal;
    if(!submitted_) {
        submitted_at_=std::chrono::steady_clock::now();
        if(!commands_.submit(targets_,eye,image,clear,[&](VkCommandBuffer command,VkExtent2D extent) {
            if(record) record(command,extent,camera,time);
        },[&](VkCommandBuffer command,VkExtent2D extent) {
            if(before_render) before_render(command,extent,camera,time);
        },[&](VkCommandBuffer command,VkExtent2D extent) {
            if(after_render) after_render(command,extent,camera,time);
        },wait)) {
            return commands_.poll()==VulkanEyeCommands::Completion::complete?Result::failed:Result::fatal;
        }
        ++timing_.submissions;
        timing_.submit_ms+=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-submitted_at_).count();
        submitted_=true;eye_=eye;image_=image;
    }
    switch(commands_.poll(1000000)) {
    case VulkanEyeCommands::Completion::pending:
        ++timing_.pending_polls;
        return Result::pending;
    case VulkanEyeCommands::Completion::error:return Result::fatal;
    case VulkanEyeCommands::Completion::complete: {
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-submitted_at_).count();
        ++timing_.eyes;timing_.total_ms+=elapsed;
        if(elapsed>timing_.maximum_ms) timing_.maximum_ms=elapsed;
        last_eye_timing_.submit_to_fence_cpu_ms=elapsed;
        last_eye_timing_.gpu_timestamp_ms=commands_.take_gpu_duration_ms();
        submitted_=false;return Result::complete;
    }
    }
    return Result::fatal;
}
}
