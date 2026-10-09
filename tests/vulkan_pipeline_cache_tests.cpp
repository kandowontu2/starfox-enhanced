#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value) {if(!value) throw std::runtime_error("Pipeline cache regression");}
std::vector<char> payload(40);
size_t supplied{};unsigned creates{},destroys{};
bool reject_initial{},reject_all{},incomplete{};
VkResult VKAPI_PTR create(VkDevice,const VkPipelineCacheCreateInfo* info,const VkAllocationCallbacks*,VkPipelineCache* out) {
    ++creates;supplied=info->initialDataSize;
    if(reject_all || (reject_initial && supplied)) return VK_ERROR_INITIALIZATION_FAILED;
    *out=reinterpret_cast<VkPipelineCache>(uintptr_t(2));return VK_SUCCESS;
}
void VKAPI_PTR destroy(VkDevice,VkPipelineCache,const VkAllocationCallbacks*) {++destroys;}
VkResult VKAPI_PTR data(VkDevice,VkPipelineCache,size_t* size,void* out) {
    if(!out) {*size=payload.size();return VK_SUCCESS;}
    if(incomplete) return VK_INCOMPLETE;
    require(*size>=payload.size());std::memcpy(out,payload.data(),payload.size());*size=payload.size();return VK_SUCCESS;
}
PFN_vkVoidFunction VKAPI_PTR get(VkDevice,const char* name) {
    if(std::strcmp(name,"vkCreatePipelineCache")==0) return reinterpret_cast<PFN_vkVoidFunction>(create);
    if(std::strcmp(name,"vkDestroyPipelineCache")==0) return reinterpret_cast<PFN_vkVoidFunction>(destroy);
    if(std::strcmp(name,"vkGetPipelineCacheData")==0) return reinterpret_cast<PFN_vkVoidFunction>(data);
    return nullptr;
}
}
int main() try {
    using starfox::vr::VulkanPipelineCache;
    const auto directory=std::filesystem::temp_directory_path()/("starfox-cache-test-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path=directory/"vr-pipelines-1-2-3.bin";
    auto device=reinterpret_cast<VkDevice>(uintptr_t(1));
    VkPhysicalDeviceProperties properties{};properties.vendorID=1;properties.deviceID=2;properties.driverVersion=3;
    uint32_t header[]{32,VK_PIPELINE_CACHE_HEADER_VERSION_ONE,1,2};
    std::memcpy(payload.data(),header,sizeof(header));
    {
        VulkanPipelineCache cache;
        require(!cache.initialize(device,nullptr,properties,directory));
        require(cache.initialize(device,get,properties,directory) && supplied==0);
        require(!cache.initialize(device,get,properties,directory));
        require(cache.checkpoint());
        payload.back()=42;require(cache.checkpoint()); // Replace existing cache on Windows too.
        incomplete=true;require(!cache.checkpoint());incomplete=false;
    }
    {
        VulkanPipelineCache cache;require(cache.initialize(device,get,properties,directory));
        require(supplied==payload.size());
    }
    {
        auto changed=properties;changed.pipelineCacheUUID[0]=1;
        VulkanPipelineCache cache;require(cache.initialize(device,get,changed,directory) && supplied==0);
    }
    {
        reject_initial=true;const auto before=creates;
        VulkanPipelineCache cache;require(cache.initialize(device,get,properties,directory));
        require(creates==before+2 && supplied==0);reject_initial=false;
    }
    {
        std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);
        file.seekp(8+39);file.put(7);file.close();
        VulkanPipelineCache cache;require(cache.initialize(device,get,properties,directory) && supplied==0);
    }
    {
        std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"truncated";file.close();
        VulkanPipelineCache cache;require(cache.initialize(device,get,properties,directory) && supplied==0);
    }
    {
        reject_all=true;const auto before=destroys;
        {VulkanPipelineCache cache;require(!cache.initialize(device,get,properties,directory) && !cache.get());}
        require(destroys==before);reject_all=false;
    }
    {
        VulkanPipelineCache cache;require(cache.initialize(device,get,properties,path/"not-a-directory"));
        require(!cache.checkpoint());
    }
    std::filesystem::remove(path);std::filesystem::remove(directory);
    std::cout<<"Cache replacement, reload, mismatch, corruption, fallback and I/O checks passed\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
