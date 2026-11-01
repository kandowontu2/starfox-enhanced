#include "starfox/modern/sdl_layer_filter.hpp"
#include "modern/shaders/generated/layer_filter.hpp"
#include <algorithm>
#include <stdexcept>
namespace starfox::modern {
namespace {
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what+": "+SDL_GetError());}
namespace code=layer_filter_shader;
}
SdlLayerFilter::~SdlLayerFilter() {release();}
void SdlLayerFilter::release_textures() noexcept {
    if(device_) {
        for(auto* textures:{&source_,&output_}) for(auto*& texture:*textures) if(texture) SDL_ReleaseGPUTexture(device_,texture);
        if(depth_) SDL_ReleaseGPUTexture(device_,depth_);
    }
    source_={};output_={};depth_=nullptr;width_=height_=factor_=0;
}
void SdlLayerFilter::release() noexcept {
    release_textures();
    scalefx_.release_device();
    pipelines_.release();
    if(device_) {
        if(compute_) SDL_ReleaseGPUComputePipeline(device_,compute_);
        if(composite_) SDL_ReleaseGPUGraphicsPipeline(device_,composite_);
        if(vertex_) SDL_ReleaseGPUShader(device_,vertex_);
        if(fragment_) SDL_ReleaseGPUShader(device_,fragment_);
        if(sampler_) SDL_ReleaseGPUSampler(device_,sampler_);
        if(empty_) SDL_ReleaseGPUBuffer(device_,empty_);
    }
    compute_=nullptr;composite_=nullptr;vertex_=fragment_=nullptr;sampler_=nullptr;empty_=nullptr;device_=nullptr;
}
bool SdlLayerFilter::initialize(SDL_GPUDevice* device,SDL_GPUTextureFormat colour,SDL_GPUTextureFormat depth) {
    release();
    try {
        if(!device) throw std::runtime_error("Missing SDL GPU device");
        device_=device;colour_=colour;depth_format_=depth;
        if(!pipelines_.initialize(device,colour,depth,true)) throw std::runtime_error(pipelines_.status());
        const auto formats=SDL_GetGPUShaderFormats(device);
        const bool dxil=(formats&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        if(!dxil && !(formats&SDL_GPU_SHADERFORMAT_SPIRV)) throw std::runtime_error("MODERN layer filter needs a SPIR-V or DXIL backend");
        const auto format=dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_SPIRV;
        SDL_GPUComputePipelineCreateInfo compute{};
        compute.format=format;compute.entrypoint="filter_main";
        compute.code=dxil?code::dxil_compute:code::spirv_compute;
        compute.code_size=dxil?sizeof(code::dxil_compute):sizeof(code::spirv_compute);
        compute.num_readonly_storage_textures=1;compute.num_readonly_storage_buffers=1;
        compute.num_readwrite_storage_textures=1;compute.num_uniform_buffers=1;
        compute.threadcount_x=8;compute.threadcount_y=8;compute.threadcount_z=1;
        compute_=SDL_CreateGPUComputePipeline(device,&compute);
        require(compute_,"Create MODERN layer filter");
        SDL_GPUShaderCreateInfo shader{};
        shader.format=format;shader.stage=SDL_GPU_SHADERSTAGE_VERTEX;shader.entrypoint="composite_vertex";
        shader.code=dxil?code::dxil_vertex:code::spirv_vertex;
        shader.code_size=dxil?sizeof(code::dxil_vertex):sizeof(code::spirv_vertex);
        vertex_=SDL_CreateGPUShader(device,&shader);
        require(vertex_,"Create MODERN layer composite vertex shader");
        shader.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;shader.entrypoint="composite_fragment";shader.num_samplers=1;shader.num_uniform_buffers=1;
        shader.code=dxil?code::dxil_fragment:code::spirv_fragment;
        shader.code_size=dxil?sizeof(code::dxil_fragment):sizeof(code::spirv_fragment);
        fragment_=SDL_CreateGPUShader(device,&shader);
        require(fragment_,"Create MODERN layer composite fragment shader");
        SDL_GPUSamplerCreateInfo sampler{};
        sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_LINEAR;sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler_=SDL_CreateGPUSampler(device,&sampler);
        require(sampler_,"Create MODERN layer sampler");
        SDL_GPUBufferCreateInfo empty{};empty.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;empty.size=16;
        empty_=SDL_CreateGPUBuffer(device,&empty);
        require(empty_,"Create MODERN layer filter buffer");
        status_="MODERN layer filter ready";return true;
    } catch(const std::exception& error) {status_=error.what();release();return false;}
}
bool SdlLayerFilter::resize(unsigned source_width,unsigned source_height,unsigned factor) {
    if(!device_ || !source_width || !source_height || !factor) {status_="Invalid MODERN layer size";return false;}
    if(source_width==width_ && source_height==height_ && factor==factor_) return true;
    release_textures();
    try {
        SDL_GPUTextureCreateInfo info{};
        info.type=SDL_GPU_TEXTURETYPE_2D;info.layer_count_or_depth=1;info.num_levels=1;
        info.sample_count=SDL_GPU_SAMPLECOUNT_1;
        for(unsigned i=0;i<layer_count;++i) {
            info.format=colour_;info.width=source_width;info.height=source_height;
            info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
            source_[i]=SDL_CreateGPUTexture(device_,&info);
            require(source_[i],"Create MODERN layer");
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;info.width=source_width*factor;info.height=source_height*factor;
            info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            output_[i]=SDL_CreateGPUTexture(device_,&info);
            require(output_[i],"Create MODERN filtered layer");
        }
        info.format=depth_format_;info.width=source_width;info.height=source_height;
        info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        depth_=SDL_CreateGPUTexture(device_,&info);
        require(depth_,"Create MODERN layer depth");
        width_=source_width;height_=source_height;factor_=factor;
        pipelines_.set_line_brush(1,source_width,source_height);
        return true;
    } catch(const std::exception& error) {status_=error.what();release_textures();return false;}
}
SDL_GPURenderPass* SdlLayerFilter::begin_layer(SDL_GPUCommandBuffer* command,unsigned index) {
    if(index>=layer_count || !source_[index]) {status_="MODERN layer not sized";return nullptr;}
    SDL_GPUColorTargetInfo target{};target.texture=source_[index];target.load_op=SDL_GPU_LOADOP_CLEAR;
    target.store_op=SDL_GPU_STOREOP_STORE;target.clear_color={0,0,0,0};
    SDL_GPUDepthStencilTargetInfo depth{};depth.texture=depth_;depth.clear_depth=0;
    depth.load_op=SDL_GPU_LOADOP_CLEAR;depth.store_op=SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE;depth.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE;
    auto* pass=SDL_BeginGPURenderPass(command,&target,1,&depth);
    if(!pass) status_=std::string("Begin MODERN layer pass: ")+SDL_GetError();
    return pass;
}
bool SdlLayerFilter::filter(SDL_GPUCommandBuffer* command,unsigned index,unsigned filter) {
    if(index>=layer_count || !source_[index]) {status_="MODERN layer not sized";return false;}
    SDL_GPUBuffer* scalefx=empty_;
    if(filter==5) {
        const auto result=scalefx_.enqueue_texture(device_,command,source_[index],width_,height_);
        // Without the ScaleFX backend, fall back to EDGE like the desktop.
        if(result.buffer) scalefx=static_cast<SDL_GPUBuffer*>(result.buffer);else filter=1;
    }
    // xBRZ's kernels stop at 6x and EDGE's corner table at 10x; larger
    // factors keep the filter's maximum, point-sampled by the composite.
    unsigned factor=factor_;
    if(filter==2) factor=std::min(factor,6U);
    if(filter==1) factor=std::min(factor,10U);
    filled_[index]=float(factor)/float(factor_);
    const uint32_t settings[8]{width_*factor,height_*factor,factor,filter,width_,height_,0,0};
    SDL_GPUStorageTextureReadWriteBinding output{};output.texture=output_[index];output.cycle=true;
    SDL_PushGPUComputeUniformData(command,0,settings,sizeof(settings));
    auto* pass=SDL_BeginGPUComputePass(command,&output,1,nullptr,0);
    if(!pass) {status_=std::string("Begin MODERN layer filter: ")+SDL_GetError();return false;}
    SDL_BindGPUComputePipeline(pass,compute_);
    SDL_BindGPUComputeStorageTextures(pass,0,&source_[index],1);
    SDL_BindGPUComputeStorageBuffers(pass,0,&scalefx,1);
    SDL_DispatchGPUCompute(pass,(settings[0]+7)/8,(settings[1]+7)/8,1);
    SDL_EndGPUComputePass(pass);
    return true;
}
bool SdlLayerFilter::composite(SDL_GPUCommandBuffer* command,SDL_GPURenderPass* pass,unsigned index,SDL_GPUSampleCount samples) {
    if(index>=layer_count || !output_[index]) {status_="MODERN layer not sized";return false;}
    if(composite_ && composite_samples_!=samples) {SDL_ReleaseGPUGraphicsPipeline(device_,composite_);composite_=nullptr;}
    if(!composite_) {
        SDL_GPUColorTargetDescription target{};target.format=colour_;
        auto& blend=target.blend_state;
        blend.enable_blend=true;
        blend.src_color_blendfactor=SDL_GPU_BLENDFACTOR_SRC_ALPHA;blend.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend.color_blend_op=SDL_GPU_BLENDOP_ADD;
        blend.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;blend.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader=vertex_;info.fragment_shader=fragment_;
        info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count=samples;
        info.target_info.color_target_descriptions=&target;info.target_info.num_color_targets=1;
        info.target_info.depth_stencil_format=depth_format_;info.target_info.has_depth_stencil_target=true;
        composite_=SDL_CreateGPUGraphicsPipeline(device_,&info);
        if(!composite_) {status_=std::string("Create MODERN layer composite: ")+SDL_GetError();return false;}
        composite_samples_=samples;
    }
    SDL_BindGPUGraphicsPipeline(pass,composite_);
    const SDL_GPUTextureSamplerBinding binding{output_[index],sampler_};
    SDL_BindGPUFragmentSamplers(pass,0,&binding,1);
    const float extent[4]{filled_[index],filled_[index],0,0};
    SDL_PushGPUFragmentUniformData(command,0,extent,sizeof(extent));
    SDL_DrawGPUPrimitives(pass,3,1,0,0);
    return true;
}
}
