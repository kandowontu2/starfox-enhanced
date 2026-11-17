#include "starfox/compat/bit_cast.hpp"
#include "starfox/vr/vulkan_ray_shadows.hpp"
#include "starfox/vr/source_ray_topology.hpp"
#include "../modern/shaders/generated/ray_shadow.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <numeric>
#include <stdexcept>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto* function=reinterpret_cast<T>(get(device,name));
    if(!function) throw std::runtime_error(std::string("Missing Vulkan entry point ")+name);
    return function;
}
void check(VkResult result,const char* what) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(what)+" failed: "+std::to_string(int(result)));
}
// ray_shadow.hlsl Settings (as modern::SdlRayShadows).
struct CasterSettings {
    uint32_t count,points,corners,mode;
    uint32_t points_word,residuals_word,corners_word,first_triangle;
    uint32_t object,pad[3];
    std::array<std::array<float,4>,3> rows;
    std::array<float,4> light;
};
static_assert(sizeof(CasterSettings)==112);
// scene.hlsl SceneRays, in float4s: shadow_params, shadow_light,
// shadow_target, ray_flags, ray_targets, ray_eyes[2], shadow_unproject[8],
// light_params, point_lights[64].
constexpr std::size_t uniform_floats=80*4;
constexpr std::size_t targets_at=4*4,eyes_at=5*4,unproject_at=7*4,lights_at=15*4;
using render::shadows::Vec3;
}
VulkanRayShadows::~VulkanRayShadows() {close();}
bool VulkanRayShadows::create_buffer(Buffer& output,VkDeviceSize size,VkBufferUsageFlags usage,bool host) {
    Buffer next;next.size=size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=usage;
    check(entry<PFN_vkCreateBuffer>(get_,device_,"vkCreateBuffer")(device_,&info,nullptr,&next.buffer),"Create ray shadow buffer");
    try {
        VkMemoryRequirements required{};
        entry<PFN_vkGetBufferMemoryRequirements>(get_,device_,"vkGetBufferMemoryRequirements")(device_,next.buffer,&required);
        const VkMemoryPropertyFlags wanted=host?VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            :VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        uint32_t type=memory_.memoryTypeCount;
        for(uint32_t i=0;i<memory_.memoryTypeCount;++i)
            if((required.memoryTypeBits&(1U<<i)) && (memory_.memoryTypes[i].propertyFlags&wanted)==wanted) {type=i;break;}
        if(type==memory_.memoryTypeCount) throw std::runtime_error("No memory type for ray shadow buffer");
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=required.size;allocate.memoryTypeIndex=type;
        check(entry<PFN_vkAllocateMemory>(get_,device_,"vkAllocateMemory")(device_,&allocate,nullptr,&next.memory),"Allocate ray shadow memory");
        check(entry<PFN_vkBindBufferMemory>(get_,device_,"vkBindBufferMemory")(device_,next.buffer,next.memory,0),"Bind ray shadow memory");
        if(host) check(entry<PFN_vkMapMemory>(get_,device_,"vkMapMemory")(device_,next.memory,0,VK_WHOLE_SIZE,0,&next.mapped),"Map ray shadow memory");
    } catch(...) {destroy_buffer(next);throw;}
    destroy_buffer(output);output=next;return true;
}
void VulkanRayShadows::destroy_buffer(Buffer& buffer) noexcept {
    if(device_ && get_) {
        if(buffer.buffer) reinterpret_cast<PFN_vkDestroyBuffer>(get_(device_,"vkDestroyBuffer"))(device_,buffer.buffer,nullptr);
        if(buffer.memory) reinterpret_cast<PFN_vkFreeMemory>(get_(device_,"vkFreeMemory"))(device_,buffer.memory,nullptr);
    }
    buffer={};
}
bool VulkanRayShadows::grow(Buffer& buffer,VkDeviceSize size,VkBufferUsageFlags usage,bool host) {
    if(size<=buffer.size) return false;
    if(size>256ULL*1024*1024) throw std::runtime_error("MODERN shadow casters exceed the buffer budget");
    create_buffer(buffer,std::max(size,buffer.size*2),usage,host);
    return true;
}
void VulkanRayShadows::write_bindings() {
    // The scene's set 2 (uniform, triangles, objects) and the casters' set 1
    // (triangles, objects) and set 2 (Settings, one dynamic offset per dispatch).
    const VkDescriptorBufferInfo uniform{uniforms_.buffer,0,VK_WHOLE_SIZE};
    const VkDescriptorBufferInfo triangles{triangles_.buffer,0,VK_WHOLE_SIZE};
    const VkDescriptorBufferInfo objects{objects_.buffer,0,VK_WHOLE_SIZE};
    const VkDescriptorBufferInfo settings{settings_.buffer,0,sizeof(CasterSettings)};
    std::array<VkWriteDescriptorSet,6> writes{};
    const auto write=[&](unsigned i,VkDescriptorSet set,uint32_t binding,VkDescriptorType type,const VkDescriptorBufferInfo& info) {
        writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};writes[i].dstSet=set;writes[i].dstBinding=binding;
        writes[i].descriptorCount=1;writes[i].descriptorType=type;writes[i].pBufferInfo=&info;
    };
    write(0,scene_set_,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,uniform);
    write(1,scene_set_,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,triangles);
    write(2,scene_set_,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,objects);
    write(3,output_set_,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,triangles);
    write(4,output_set_,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,objects);
    write(5,settings_set_,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,settings);
    entry<PFN_vkUpdateDescriptorSets>(get_,device_,"vkUpdateDescriptorSets")(device_,uint32_t(writes.size()),writes.data(),0,nullptr);
}
bool VulkanRayShadows::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory,
    const VkPhysicalDeviceLimits& limits) {
    close();
    try {
        if(!device || !get) throw std::runtime_error("Missing ray shadow device");
        device_=device;get_=get;memory_=memory;limits_=limits;
        const auto create_layout=entry<PFN_vkCreateDescriptorSetLayout>(get,device,"vkCreateDescriptorSetLayout");
        const auto layout=[&](std::initializer_list<VkDescriptorType> types,VkShaderStageFlags stages,VkDescriptorSetLayout& output) {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for(auto type:types) bindings.push_back({uint32_t(bindings.size()),type,1,stages,nullptr});
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.bindingCount=uint32_t(bindings.size());info.pBindings=bindings.data();
            check(create_layout(device,&info,nullptr,&output),"Create ray shadow layout");
        };
        layout({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
            VK_SHADER_STAGE_FRAGMENT_BIT,scene_layout_);
        layout({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},VK_SHADER_STAGE_COMPUTE_BIT,input_layout_);
        layout({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},VK_SHADER_STAGE_COMPUTE_BIT,output_layout_);
        layout({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},VK_SHADER_STAGE_COMPUTE_BIT,settings_layout_);
        const std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets=3;pool.poolSizeCount=uint32_t(sizes.size());pool.pPoolSizes=sizes.data();
        check(entry<PFN_vkCreateDescriptorPool>(get,device,"vkCreateDescriptorPool")(device,&pool,nullptr,&scene_pool_),"Create ray shadow pool");
        const std::array<VkDescriptorSetLayout,3> layouts{scene_layout_,output_layout_,settings_layout_};
        std::array<VkDescriptorSet,3> sets{};
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool=scene_pool_;allocation.descriptorSetCount=3;allocation.pSetLayouts=layouts.data();
        check(entry<PFN_vkAllocateDescriptorSets>(get,device,"vkAllocateDescriptorSets")(device,&allocation,sets.data()),"Allocate ray shadow sets");
        scene_set_=sets[0];output_set_=sets[1];settings_set_=sets[2];
        // The casters' pipeline: MODERN's ray_shadow.hlsl, whose SDL_GPU
        // compute sets (read-only, read-write, uniforms) match these.
        const std::array<VkDescriptorSetLayout,3> compute_sets{input_layout_,output_layout_,settings_layout_};
        VkPipelineLayoutCreateInfo pipeline_layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_layout.setLayoutCount=3;pipeline_layout.pSetLayouts=compute_sets.data();
        check(entry<PFN_vkCreatePipelineLayout>(get,device,"vkCreatePipelineLayout")(device,&pipeline_layout,nullptr,&pipeline_layout_),
            "Create ray shadow pipeline layout");
        std::vector<uint32_t> code((sizeof(modern::ray_shadow_shader::spirv_compute)+3)/4);
        std::memcpy(code.data(),modern::ray_shadow_shader::spirv_compute,sizeof(modern::ray_shadow_shader::spirv_compute));
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=sizeof(modern::ray_shadow_shader::spirv_compute);module.pCode=code.data();
        VkShaderModule shader{};
        check(entry<PFN_vkCreateShaderModule>(get,device,"vkCreateShaderModule")(device,&module,nullptr,&shader),"Create ray shadow shader");
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        info.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;info.stage.module=shader;info.stage.pName="main";
        info.layout=pipeline_layout_;
        const auto created=entry<PFN_vkCreateComputePipelines>(get,device,"vkCreateComputePipelines")(device,VK_NULL_HANDLE,1,&info,nullptr,&pipeline_);
        entry<PFN_vkDestroyShaderModule>(get,device,"vkDestroyShaderModule")(device,shader,nullptr);
        check(created,"Create ray shadow pipeline");
        settings_stride_=std::lcm(VkDeviceSize(sizeof(CasterSettings)),std::max<VkDeviceSize>(limits.minUniformBufferOffsetAlignment,1));
        create_buffer(uniforms_,uniform_floats*sizeof(float),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
        create_buffer(triangles_,64*64,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,false);
        create_buffer(objects_,64*32,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false);
        create_buffer(settings_,settings_stride_*64,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
        create_buffer(topology_,64*1024,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memset(uniforms_.mapped,0,uniform_floats*sizeof(float));
        write_bindings();
        binding_={scene_layout_,scene_set_};
        status_="Ray shadows ready";return true;
    } catch(const std::exception& error) {status_=error.what();const auto message=status_;close();status_=message;return false;}
}
void VulkanRayShadows::close() noexcept {
    if(device_ && get_) {
        if(pipeline_) reinterpret_cast<PFN_vkDestroyPipeline>(get_(device_,"vkDestroyPipeline"))(device_,pipeline_,nullptr);
        if(pipeline_layout_) reinterpret_cast<PFN_vkDestroyPipelineLayout>(get_(device_,"vkDestroyPipelineLayout"))(device_,pipeline_layout_,nullptr);
        const auto destroy_pool=reinterpret_cast<PFN_vkDestroyDescriptorPool>(get_(device_,"vkDestroyDescriptorPool"));
        for(auto pool:{scene_pool_,dispatch_pool_}) if(pool) destroy_pool(device_,pool,nullptr);
        const auto destroy_layout=reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(get_(device_,"vkDestroyDescriptorSetLayout"));
        for(auto layout:{scene_layout_,input_layout_,output_layout_,settings_layout_}) if(layout) destroy_layout(device_,layout,nullptr);
        for(auto* buffer:{&uniforms_,&triangles_,&objects_,&topology_,&settings_}) destroy_buffer(*buffer);
    }
    pipeline_={};pipeline_layout_={};scene_pool_=dispatch_pool_={};dispatch_capacity_=0;
    scene_layout_=input_layout_=output_layout_=settings_layout_={};scene_set_=output_set_=settings_set_={};
    binding_={};dispatches_.clear();object_seed_.clear();point_lights_.clear();object_index_.clear();
    object_count_=triangle_count_=0;active_=sun_active_=false;device_={};get_={};
    status_="Ray shadows not initialized";
}
bool VulkanRayShadows::prepare(const VulkanSourceScene& world,const SourceModelPackets& packets,
    std::optional<render::shadows::Vec3> toward_light,bool receivers,const std::function<bool(uint32_t)>& caster,
    const Settings& settings,const LightInputs* lights) {
    active_=sun_active_=false;object_count_=triangle_count_=0;dispatches_.clear();object_seed_.clear();
    point_lights_.clear();object_index_.clear();
    auto* uniform=static_cast<float*>(uniforms_.mapped);
    if(uniform) uniform[0]=0; // shadow_params.x: nothing traced
    if(!device_) {status_="Ray shadows not initialized";return false;}
    try {
        settings_values_=settings;
        settings_values_.brightness=std::clamp(settings.brightness,0.F,1.F);
        settings_values_.samples=std::clamp(settings.samples,1U,16U);
        const bool sun=toward_light && receivers;
        if(!sun && !lights) {status_="MODERN ray shadows off";return true;}
        // Emissive lights need the casters for their own shadow rays, sun or
        // not; any direction serves the casters' sun-plane bounds then.
        auto light=sun?*toward_light:Vec3{0,-1,0};
        // The seat: the world's own axes, the eyes' shared space.
        EyeCamera seat;
        seat.view={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};seat.projection=seat.view;
        light=ray_eye_direction(seat,light);
        const double length=std::sqrt(render::shadows::dot(light,light));
        if(!std::isfinite(length) || length<=1e-9) throw std::runtime_error("Invalid MODERN shadow light");
        light_=light*(1/length);
        // Casters: each physical compute-route model, its topology built
        // here and its points read from its own arena.
        const VkDeviceSize alignment=std::lcm(std::max<VkDeviceSize>(limits_.minStorageBufferOffsetAlignment,1),VkDeviceSize(16));
        struct Item {VkBuffer arena{};VkDeviceSize topology_offset{},topology_bytes{};CasterSettings settings{};};
        std::vector<Item> items;std::vector<std::array<uint32_t,4>> topology;
        std::vector<std::array<uint32_t,4>> all_topology;
        for(const auto& model:packets.compute_models) {
            if(is_source_shadow_pass(model.key) || (caster && !caster(model.key))) continue;
            if(model.axis || model.warp) continue; // not drawn by the ray source
            VulkanSourceScene::RaySource source;
            if(!world.ray_source(model.key,source)) continue;
            const auto rows=ray_model_rows(seat,source.placement,source.units);
            if(!rows) continue;
            const uint32_t points=model.model.projection_settings[0];
            std::string error;topology.clear();
            if(!source_ray_topology(model.model.faces,points,topology,error) || topology.empty()) continue;
            bool aligned=true;
            for(const auto& range:source.ranges) aligned=aligned && range.buffer==source.ranges[0].buffer && range.offset%4==0;
            if(!aligned) continue;
            Item item;item.arena=source.ranges[0].buffer;
            item.topology_offset=((all_topology.size()*16+alignment-1)/alignment)*alignment;
            item.topology_bytes=topology.size()*16;
            all_topology.resize(std::size_t(item.topology_offset/16));
            all_topology.insert(all_topology.end(),topology.begin(),topology.end());
            const uint32_t count=uint32_t(topology.size());
            item.settings={count,points,uint32_t(model.model.faces.corners.size()),model.model.projection_settings[2],
                uint32_t(source.ranges[0].offset/4),uint32_t(source.ranges[1].offset/4),uint32_t(source.ranges[2].offset/4),
                triangle_count_,uint32_t(items.size()),{},*rows,{float(light_.x),float(light_.y),float(light_.z),0.F}};
            // Count bit 31 marks large scenery (vr::painter_scenery): the
            // fragments ignore it for points within its bounds.
            object_index_[model.key]=uint32_t(items.size());
            object_seed_.push_back({~0U,~0U,~0U,triangle_count_,0,0,0,count|(world.painter(model.packet_index)?0x80000000U:0U)});
            triangle_count_+=count;
            items.push_back(item);
        }
        object_count_=uint32_t(items.size());
        bool rebind=grow(triangles_,VkDeviceSize(triangle_count_)*64,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,false);
        rebind=grow(objects_,VkDeviceSize(object_count_)*32,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false) || rebind;
        rebind=grow(settings_,settings_stride_*std::max<std::size_t>(items.size(),1),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true) || rebind;
        grow(topology_,std::max<VkDeviceSize>(all_topology.size()*16,16),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        if(rebind) write_bindings();
        if(!all_topology.empty()) std::memcpy(topology_.mapped,all_topology.data(),all_topology.size()*16);
        // One set 0 per dispatch: its model's arena and topology.
        if(items.size()>dispatch_capacity_) {
            if(dispatch_pool_) reinterpret_cast<PFN_vkDestroyDescriptorPool>(get_(device_,"vkDestroyDescriptorPool"))(device_,dispatch_pool_,nullptr);
            dispatch_pool_={};dispatch_capacity_=0;
            const uint32_t capacity=uint32_t(std::max<std::size_t>(items.size()*2,64));
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,capacity*2};
            VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool.maxSets=capacity;pool.poolSizeCount=1;pool.pPoolSizes=&size;
            check(entry<PFN_vkCreateDescriptorPool>(get_,device_,"vkCreateDescriptorPool")(device_,&pool,nullptr,&dispatch_pool_),
                "Create ray caster pool");
            dispatch_capacity_=capacity;
        }
        if(dispatch_pool_) check(entry<PFN_vkResetDescriptorPool>(get_,device_,"vkResetDescriptorPool")(device_,dispatch_pool_,0),
            "Reset ray caster pool");
        if(!items.empty()) {
            std::vector<VkDescriptorSetLayout> layouts(items.size(),input_layout_);
            std::vector<VkDescriptorSet> sets(items.size());
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool=dispatch_pool_;allocation.descriptorSetCount=uint32_t(items.size());allocation.pSetLayouts=layouts.data();
            check(entry<PFN_vkAllocateDescriptorSets>(get_,device_,"vkAllocateDescriptorSets")(device_,&allocation,sets.data()),
                "Allocate ray caster sets");
            std::vector<VkDescriptorBufferInfo> infos(items.size()*2);
            std::vector<VkWriteDescriptorSet> writes(items.size()*2,{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});
            for(std::size_t i=0;i<items.size();++i) {
                infos[i*2]={items[i].arena,0,VK_WHOLE_SIZE};
                infos[i*2+1]={topology_.buffer,items[i].topology_offset,items[i].topology_bytes};
                for(unsigned b=0;b<2;++b) {
                    auto& write=writes[i*2+b];write.dstSet=sets[i];write.dstBinding=b;write.descriptorCount=1;
                    write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&infos[i*2+b];
                }
                std::memcpy(static_cast<char*>(settings_.mapped)+i*settings_stride_,&items[i].settings,sizeof(CasterSettings));
                dispatches_.push_back({sets[i],uint32_t(i*settings_stride_),(items[i].settings.count+63)/64});
            }
            entry<PFN_vkUpdateDescriptorSets>(get_,device_,"vkUpdateDescriptorSets")(device_,uint32_t(writes.size()),writes.data(),0,nullptr);
        }
        sun_active_=sun && object_count_;
        if(lights) point_lights_=ray_point_lights(seat,light_,object_index_,packets.compute_models,packets.packets,packets.handles,
            lights->emitter,lights->bomb,max_point_lights,settings_values_.light_reach,lights->tick);
        active_=sun_active_ || !point_lights_.empty();
        // The frame's part of SceneRays; the eyes fill their own (set_eye).
        std::fill(uniform,uniform+targets_at,0.F);
        std::fill(uniform+lights_at,uniform+uniform_floats,0.F);
        if(active_) {
            uniform[0]=1;uniform[1]=float(object_count_);uniform[2]=settings_values_.brightness;uniform[3]=float(settings_values_.samples);
            uniform[4]=float(light_.x);uniform[5]=float(light_.y);uniform[6]=float(light_.z);uniform[7]=2e-4F;
            uniform[10]=settings_values_.radius;
            // ray_flags: sun shadows on the background (fog mode 2), the models (1).
            uniform[12]=sun_active_?1.F:0.F;uniform[13]=sun_active_ && settings_values_.models?1.F:0.F;
            float* light_params=uniform+lights_at;
            light_params[0]=float(point_lights_.size());light_params[1]=float(settings_values_.light_steps);
            light_params[2]=settings_values_.light_strength;light_params[3]=sun_active_?1.F:0.F;
            for(std::size_t i=0;i<point_lights_.size();++i) {
                const auto& point=point_lights_[i];
                float* out=light_params+4+i*16;
                out[0]=point.position[0];out[1]=point.position[1];out[2]=point.position[2];out[3]=point.radius;
                out[4]=point.colour[0];out[5]=point.colour[1];out[6]=point.colour[2];out[7]=starfox::bit_cast<float>(point.object);
                out[8]=point.axis[0];out[9]=point.axis[1];out[10]=point.axis[2];out[11]=point.aspect;
                // Projected lights: look.y = spread (nonzero), look.w = lift.
                out[12]=point.hardness;out[13]=point.projected?std::max(point.spread,1e-3F):0.F;out[14]=point.fade_height;
                out[15]=point.projected?point.lift:(point.background_only?1.F:0.F);
            }
        }
        status_=active_?"MODERN ray shadows prepared":"MODERN ray shadows: nothing to trace";return true;
    } catch(const std::exception& error) {
        if(uniform) uniform[0]=0;
        active_=sun_active_=false;object_count_=triangle_count_=0;dispatches_.clear();point_lights_.clear();
        status_=error.what();return false;
    }
}
bool VulkanRayShadows::record_casters(VkCommandBuffer command) const {
    if(!active_ || dispatches_.empty()) return true;
    const auto barrier=reinterpret_cast<PFN_vkCmdPipelineBarrier>(get_(device_,"vkCmdPipelineBarrier"));
    const auto update=reinterpret_cast<PFN_vkCmdUpdateBuffer>(get_(device_,"vkCmdUpdateBuffer"));
    const auto bind=reinterpret_cast<PFN_vkCmdBindPipeline>(get_(device_,"vkCmdBindPipeline"));
    const auto bind_sets=reinterpret_cast<PFN_vkCmdBindDescriptorSets>(get_(device_,"vkCmdBindDescriptorSets"));
    const auto dispatch=reinterpret_cast<PFN_vkCmdDispatch>(get_(device_,"vkCmdDispatch"));
    if(!barrier || !update || !bind || !bind_sets || !dispatch) return false;
    // Seed the object table (open bounds, triangle ranges).
    const auto bytes=object_seed_.size()*32;
    for(std::size_t at=0;at<bytes;at+=65536)
        update(command,objects_.buffer,at,std::min<std::size_t>(65536,bytes-at),reinterpret_cast<const char*>(object_seed_.data())+at);
    // The models' compute output and the seed, then the casters.
    VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    before.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;
    before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    barrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,1,&before,0,nullptr,0,nullptr);
    bind(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);
    bind_sets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout_,1,1,&output_set_,0,nullptr);
    for(const auto& item:dispatches_) {
        bind_sets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout_,0,1,&item.set,0,nullptr);
        bind_sets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout_,2,1,&settings_set_,1,&item.offset);
        dispatch(command,item.groups,1,1);
    }
    record_read_barrier(command);
    return true;
}
void VulkanRayShadows::record_read_barrier(VkCommandBuffer command) const {
    if(!active_ || dispatches_.empty()) return;
    const auto barrier=reinterpret_cast<PFN_vkCmdPipelineBarrier>(get_(device_,"vkCmdPipelineBarrier"));
    VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    barrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,1,&after,0,nullptr,0,nullptr);
}
void VulkanRayShadows::set_eye(unsigned eye,const EyeCamera& camera,VkExtent2D extent) noexcept {
    auto* uniform=static_cast<float*>(uniforms_.mapped);
    if(!uniform || eye>1) return;
    uniform[targets_at+eye*2]=float(extent.width);uniform[targets_at+eye*2+1]=float(extent.height);
    // The eye in the seat's native axes (+Y down, +Z forward, 256 per metre).
    const auto& v=camera.view;
    std::array<double,3> position{};
    for(unsigned k=0;k<3;++k) position[k]=-(v[k*4]*v[12]+v[k*4+1]*v[13]+v[k*4+2]*v[14]);
    float* at=uniform+eyes_at+eye*4;
    at[0]=float(position[0]*256);at[1]=float(-position[1]*256);at[2]=float(-position[2]*256);at[3]=0;
    // Clip to world: inverse(projection x view), as rows.
    std::array<double,32> m{};
    for(unsigned row=0;row<4;++row) {
        for(unsigned column=0;column<4;++column) {
            double value=0;
            for(unsigned k=0;k<4;++k) value+=double(camera.projection[k*4+row])*v[column*4+k];
            m[row*8+column]=value;
        }
        m[row*8+4+row]=1;
    }
    for(unsigned column=0;column<4;++column) {
        unsigned pivot=column;
        for(unsigned row=column+1;row<4;++row) if(std::abs(m[row*8+column])>std::abs(m[pivot*8+column])) pivot=row;
        if(std::abs(m[pivot*8+column])<1e-30) {uniform[0]=0;return;}
        for(unsigned k=0;k<8;++k) std::swap(m[column*8+k],m[pivot*8+k]);
        const double scale=1/m[column*8+column];
        for(unsigned k=0;k<8;++k) m[column*8+k]*=scale;
        for(unsigned row=0;row<4;++row) if(row!=column) {
            const double factor=m[row*8+column];
            for(unsigned k=0;k<8;++k) m[row*8+k]-=factor*m[column*8+k];
        }
    }
    for(unsigned row=0;row<4;++row) for(unsigned column=0;column<4;++column)
        uniform[unproject_at+eye*16+row*4+column]=float(m[row*8+4+column]);
}
}
