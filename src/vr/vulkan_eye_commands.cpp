#include "starfox/vr/vulkan_eye_commands.hpp"
#include <cmath>
#include <stdexcept>
#include <array>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
template<class T> T optional_entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    return reinterpret_cast<T>(get(device,name));
}
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(result));
}
}
VulkanEyeCommands::~VulkanEyeCommands() {close();}
void VulkanEyeCommands::close() noexcept {
    if((pending_ || failed_) && queue_ && idle_) idle_(queue_);
    if(query_pool_ && destroy_query_pool_) destroy_query_pool_(device_,query_pool_,nullptr);
    if(fence_) destroy_fence_(device_,fence_,nullptr);
    if(pool_) destroy_pool_(device_,pool_,nullptr);
    device_={};queue_={};pool_={};command_={};fence_={};query_pool_={};pending_=failed_=false;
    timestamps_active_=false;timestamp_valid_bits_=0;timestamp_period_ns_=0.;gpu_duration_ms_.reset();
    destroy_query_pool_=nullptr;reset_query_pool_=nullptr;write_timestamp_=nullptr;get_query_results_=nullptr;
    timestamp_status_="GPU timestamps not initialized";
}
bool VulkanEyeCommands::initialize(VkDevice device,VkQueue queue,uint32_t family,PFN_vkGetDeviceProcAddr get,
    std::optional<TimestampConfig> timestamps) {
    close();
    try {
        if(!device || !queue || !get) throw std::runtime_error("Missing eye command device/queue");
        device_=device;queue_=queue;
#define LOAD(member,type,name) member=entry<type>(get,device,name)
        LOAD(destroy_pool_,PFN_vkDestroyCommandPool,"vkDestroyCommandPool");
        LOAD(destroy_fence_,PFN_vkDestroyFence,"vkDestroyFence");
        LOAD(idle_,PFN_vkQueueWaitIdle,"vkQueueWaitIdle");
        LOAD(reset_pool_,PFN_vkResetCommandPool,"vkResetCommandPool");
        LOAD(reset_fences_,PFN_vkResetFences,"vkResetFences");
        LOAD(begin_,PFN_vkBeginCommandBuffer,"vkBeginCommandBuffer");
        LOAD(end_,PFN_vkEndCommandBuffer,"vkEndCommandBuffer");
        LOAD(begin_pass_,PFN_vkCmdBeginRenderPass,"vkCmdBeginRenderPass");
        LOAD(end_pass_,PFN_vkCmdEndRenderPass,"vkCmdEndRenderPass");
        LOAD(submit_,PFN_vkQueueSubmit,"vkQueueSubmit");
        LOAD(fence_status_,PFN_vkGetFenceStatus,"vkGetFenceStatus");
        LOAD(wait_fences_,PFN_vkWaitForFences,"vkWaitForFences");
#undef LOAD
        auto create_pool=entry<PFN_vkCreateCommandPool>(get,device,"vkCreateCommandPool");
        auto allocate=entry<PFN_vkAllocateCommandBuffers>(get,device,"vkAllocateCommandBuffers");
        auto create_fence=entry<PFN_vkCreateFence>(get,device,"vkCreateFence");
        VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool.queueFamilyIndex=family;pool.flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        check(create_pool(device,&pool,nullptr,&pool_),"Create eye command pool");
        VkCommandBufferAllocateInfo buffer{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        buffer.commandPool=pool_;buffer.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;buffer.commandBufferCount=1;
        check(allocate(device,&buffer,&command_),"Allocate eye command buffer");
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        check(create_fence(device,&fence,nullptr,&fence_),"Create eye fence");
        if(!timestamps) timestamp_status_="GPU timestamps disabled";
        else if(timestamps->valid_bits==0) timestamp_status_="unavailable: queue timestampValidBits is zero";
        else if(timestamps->valid_bits>64 || !std::isfinite(timestamps->period_ns) || timestamps->period_ns<=0.)
            timestamp_status_="unavailable: invalid queue timestamp properties";
        else {
            destroy_query_pool_=optional_entry<PFN_vkDestroyQueryPool>(get,device,"vkDestroyQueryPool");
            reset_query_pool_=optional_entry<PFN_vkCmdResetQueryPool>(get,device,"vkCmdResetQueryPool");
            write_timestamp_=optional_entry<PFN_vkCmdWriteTimestamp>(get,device,"vkCmdWriteTimestamp");
            get_query_results_=optional_entry<PFN_vkGetQueryPoolResults>(get,device,"vkGetQueryPoolResults");
            const auto create_query_pool=optional_entry<PFN_vkCreateQueryPool>(get,device,"vkCreateQueryPool");
            if(!destroy_query_pool_ || !reset_query_pool_ || !write_timestamp_ || !get_query_results_ || !create_query_pool) {
                timestamp_status_="unavailable: Vulkan timestamp query entry point missing";
            } else {
                VkQueryPoolCreateInfo query{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
                query.queryType=VK_QUERY_TYPE_TIMESTAMP;query.queryCount=2;
                const auto created=create_query_pool(device,&query,nullptr,&query_pool_);
                if(created!=VK_SUCCESS) {
                    query_pool_={};
                    timestamp_status_="unavailable: vkCreateQueryPool failed ("+std::to_string(created)+")";
                } else {
                    timestamp_valid_bits_=timestamps->valid_bits;timestamp_period_ns_=timestamps->period_ns;
                    timestamps_active_=true;timestamp_status_="available: reusable Vulkan timestamp query pool";
                }
            }
        }
        status_="Vulkan eye commands ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
bool VulkanEyeCommands::submit(const VulkanEyeTargets& targets,unsigned eye,unsigned image,
    const VkClearColorValue& color,const Record& record,const Record& before_render,
    const Record& after_render,const TimelineWait* wait) {
    if(!targets.framebuffer(eye,image)) {
        status_="Eye framebuffer is not ready";return false;
    }
    return submit_work(targets.extent(eye),[&](VkCommandBuffer command,VkExtent2D extent) {
        if(before_render) before_render(command,extent);
        std::array<VkClearValue,2> clear{};clear[0].color=color;clear[1].depthStencil={1.0F,0};
        VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        pass.renderPass=targets.render_pass();pass.framebuffer=targets.framebuffer(eye,image);
        pass.renderArea.extent=extent;pass.clearValueCount=targets.has_depth()?2:1;pass.pClearValues=clear.data();
        begin_pass_(command,&pass,VK_SUBPASS_CONTENTS_INLINE);
        if(record) record(command,extent);
        end_pass_(command);
        if(after_render) after_render(command,extent);
    },wait);
}
bool VulkanEyeCommands::submit_work(VkExtent2D extent,const Record& record,const TimelineWait* wait) {
    if(!command_ || pending_ || failed_ || !record) {
        status_="Eye command submission is not ready";return false;
    }
    if(wait && (!wait->semaphore || !wait->value || !wait->stage)) {status_="Invalid eye timeline wait";return false;}
    try {
        check(reset_pool_(device_,pool_,0),"Reset eye commands");
        check(reset_fences_(device_,1,&fence_),"Reset eye fence");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(begin_(command_,&begin),"Begin eye commands");
        if(timestamps_active_) {
            reset_query_pool_(command_,query_pool_,0,2);
            write_timestamp_(command_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,query_pool_,0);
        }
        record(command_,extent);
        if(timestamps_active_)
            write_timestamp_(command_,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,query_pool_,1);
        check(end_(command_),"End eye commands");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command_;
        VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
        if(wait) {
            timeline.waitSemaphoreValueCount=1;timeline.pWaitSemaphoreValues=&wait->value;
            submit.pNext=&timeline;submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&wait->semaphore;
            submit.pWaitDstStageMask=&wait->stage;
        }
        // Treat a failed submit as uncertain; only teardown may recover it.
        failed_=true;
        check(submit_(queue_,1,&submit,fence_),"Submit eye commands");
        failed_=false;pending_=true;return true;
    } catch(const std::exception& e) {status_=e.what();return false;}
    catch(...) {status_="Eye recording callback failed";return false;}
}
VulkanEyeCommands::Completion VulkanEyeCommands::poll(uint64_t timeout_ns) {
    if(!device_ || failed_) return Completion::error;
    if(!pending_) return Completion::complete;
    const auto result=timeout_ns?wait_fences_(device_,1,&fence_,VK_TRUE,
        timeout_ns>1000000?1000000:timeout_ns):fence_status_(device_,fence_);
    if(result==VK_NOT_READY || result==VK_TIMEOUT) return Completion::pending;
    if(result==VK_SUCCESS) {
        pending_=false;gpu_duration_ms_.reset();
        if(timestamps_active_) {
            std::array<std::uint64_t,4> values{};
            const auto query_result=get_query_results_(device_,query_pool_,0,2,sizeof(values),values.data(),
                2*sizeof(std::uint64_t),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
            if(query_result==VK_SUCCESS && values[1] && values[3])
                gpu_duration_ms_=timestamp_duration_ms(values[0],values[2],timestamp_valid_bits_,timestamp_period_ns_);
            else if(query_result!=VK_NOT_READY) {
                timestamps_active_=false;
                timestamp_status_="unavailable: vkGetQueryPoolResults failed ("+std::to_string(query_result)+")";
            }
        }
        return Completion::complete;
    }
    failed_=true;status_="Eye fence failed: "+std::to_string(result);return Completion::error;
}
std::optional<double> VulkanEyeCommands::timestamp_duration_ms(std::uint64_t begin,std::uint64_t end,
    std::uint32_t valid_bits,double period_ns) noexcept {
    if(valid_bits==0 || valid_bits>64 || !std::isfinite(period_ns) || period_ns<=0.) return std::nullopt;
    auto ticks=end-begin;
    if(valid_bits<64) ticks&=(std::uint64_t{1}<<valid_bits)-1U;
    const auto milliseconds=static_cast<double>(ticks)*period_ns/1'000'000.;
    return std::isfinite(milliseconds)?std::optional<double>(milliseconds):std::nullopt;
}
}
