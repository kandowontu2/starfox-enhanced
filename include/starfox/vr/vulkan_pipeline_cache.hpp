#pragma once
#include <vulkan/vulkan.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace starfox::vr {
// Render-thread owned. The device must outlive this cache. Cache failure is
// optional: rendering continues with ordinary pipeline compilation.
class VulkanPipelineCache {
public:
    ~VulkanPipelineCache() {if(cache_) destroy_(device_,cache_,nullptr);}
    VulkanPipelineCache()=default;
    VulkanPipelineCache(const VulkanPipelineCache&)=delete;
    VulkanPipelineCache& operator=(const VulkanPipelineCache&)=delete;
    VkPipelineCache get() const noexcept {return cache_;}
    bool initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
        const VkPhysicalDeviceProperties& properties,const std::filesystem::path& directory) {
        if(cache_ || !device || !get) return false;
        path_.clear();
        device_=device;
        auto create=reinterpret_cast<PFN_vkCreatePipelineCache>(get(device,"vkCreatePipelineCache"));
        destroy_=reinterpret_cast<PFN_vkDestroyPipelineCache>(get(device,"vkDestroyPipelineCache"));
        data_=reinterpret_cast<PFN_vkGetPipelineCacheData>(get(device,"vkGetPipelineCacheData"));
        if(!create || !destroy_ || !data_) return false;
        std::vector<char> initial;
        try {
            if(!directory.empty()) {
                path_=directory/("vr-pipelines-"+std::to_string(properties.vendorID)+"-"
                    +std::to_string(properties.deviceID)+"-"+std::to_string(properties.driverVersion)+".bin");
                std::ifstream file(path_,std::ios::binary|std::ios::ate);
                const auto size=file.tellg();
                if(size>=40 && size<=maximum_size+8) {
                    std::uint64_t expected{};
                    file.seekg(0);file.read(reinterpret_cast<char*>(&expected),sizeof(expected));
                    initial.resize(static_cast<size_t>(size)-8);
                    file.read(initial.data(),initial.size());
                    if(!file || hash(initial)!=expected || !compatible(initial,properties)) initial.clear();
                }
            }
        } catch(...) {initial.clear();}
        VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        info.initialDataSize=initial.size();info.pInitialData=initial.empty()?nullptr:initial.data();
        auto result=create(device,&info,nullptr,&cache_);
        if(result!=VK_SUCCESS && !initial.empty()) {
            info.initialDataSize=0;info.pInitialData=nullptr;
            result=create(device,&info,nullptr,&cache_);
        }
        if(result!=VK_SUCCESS) cache_=VK_NULL_HANDLE;
        return result==VK_SUCCESS;
    }
    // Called only after newly compiled pipelines, not every frame. A failed
    // replacement leaves the previous valid cache intact.
    bool checkpoint() noexcept {
        if(!cache_ || path_.empty()) return false;
        try {
            size_t size=0;
            if(data_(device_,cache_,&size,nullptr)!=VK_SUCCESS || size>maximum_size) return false;
            std::vector<char> bytes(size);
            if(data_(device_,cache_,&size,bytes.data())!=VK_SUCCESS) return false;
            bytes.resize(size);
            std::filesystem::create_directories(path_.parent_path());
            auto temporary=path_;temporary+=".tmp";
            std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
            const auto checksum=hash(bytes);
            file.write(reinterpret_cast<const char*>(&checksum),sizeof(checksum));
            file.write(bytes.data(),bytes.size());file.close();
            if(!file) return false;
#ifdef _WIN32
            return MoveFileExW(temporary.c_str(),path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
            std::error_code error;
            std::filesystem::rename(temporary,path_,error);
            return !error;
#endif
        } catch(...) {return false;}
    }
private:
    static constexpr std::streamoff maximum_size=64*1024*1024;
    static std::uint64_t hash(const std::vector<char>& bytes) noexcept {
        std::uint64_t value=14695981039346656037ULL;
        for(unsigned char byte:bytes) {value^=byte;value*=1099511628211ULL;}
        return value;
    }
    static bool compatible(const std::vector<char>& bytes,const VkPhysicalDeviceProperties& p) noexcept {
        if(bytes.size()<32) return false;
        std::uint32_t header[4];std::memcpy(header,bytes.data(),sizeof(header));
        return header[0]>=32 && header[0]<=bytes.size() && header[1]==VK_PIPELINE_CACHE_HEADER_VERSION_ONE
            && header[2]==p.vendorID && header[3]==p.deviceID
            && std::memcmp(bytes.data()+16,p.pipelineCacheUUID,VK_UUID_SIZE)==0;
    }
    VkDevice device_{};VkPipelineCache cache_{};
    PFN_vkDestroyPipelineCache destroy_{};PFN_vkGetPipelineCacheData data_{};
    std::filesystem::path path_;
};
}
