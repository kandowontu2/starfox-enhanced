#include "starfox/render/gpu_composite.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include "starfox/render/submission_retirement.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <iostream>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "starfox/render/gpu_preparation.hpp"
#include "starfox/render/gpu_retirement.hpp"
#include "shaders/generated/composite_portable.hpp"
#include "shaders/generated/composite_edge_portable.hpp"
#include "shaders/generated/composite_pixel_portable.hpp"
#endif
namespace starfox::render {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {void checked(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}}
struct GpuComposite::Impl {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{},*edge_pipeline{};
    SDL_GPUBuffer* buffers[8]{};Uint32 capacities[8]{};
    SDL_GPUTransferBuffer *upload{},*download{};Uint32 uploadSize{},downloadSize{};
    SDL_GPUTexture* rgba{},*empty_msaa{};SDL_GPUCommandBuffer* command{};SDL_GPUFence* fence{};
    std::vector<SDL_GPUFence*> retired_fences;
    Uint32 width{},height{};bool valid{},has_depth{},has_motion{},ordered_queue_reuse{};
    std::vector<Uint32> packed;
    std::vector<std::pair<Uint32,Uint32>> dirtySpans;
    std::array<Uint32,256> cachedPalette{};
    Uint32 cachedUniformValue{},cachedUniformBytes{},lastCpuUploadBytes{},lastPaletteUploadBytes{};
    bool cachedUniformValid{},cachedPackedValid{},cachedPaletteValid{};
    struct CpuInputs {
        Uint32 width{},height{},scale{},logicalWidth{};
        bool uniform{},striped{};
        Uint32 value{},stripeValue{};
        std::array<std::pair<Uint32,Uint32>,2> stripes{};
    } cpuInputs;
    bool ownsCpuInputs{};
    bool split_pipelines{},reported_pipeline_mode{},reported_ordered_reuse{};
    std::string status{"GPU composition not initialized"};
    ~Impl() {
        if(!device) return;
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) {wait_gpu_retirement(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);}
        for(auto* previous:retired_fences) {wait_gpu_retirement(device,true,&previous,1);SDL_ReleaseGPUFence(device,previous);}
        if(rgba) SDL_ReleaseGPUTexture(device,rgba);
        if(empty_msaa) SDL_ReleaseGPUTexture(device,empty_msaa);
        for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(edge_pipeline) SDL_ReleaseGPUComputePipeline(device,edge_pipeline);
    }
    void retire(std::size_t limit) {
        checked(retire_submission_queue(retired_fences,limit,
            [&](auto* previous){return SDL_QueryGPUFence(device,previous);},
            [&](auto* previous){return SDL_WaitForGPUFences(device,true,&previous,1);},
            [&](auto* previous){SDL_ReleaseGPUFence(device,previous);}));
    }
    void finish() {
        if(fence) {checked(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);fence=nullptr;}
        retire(0);
    }
    void initialize(SDL_GPUDevice* d) {
        device=d;
        // A partial initialization is not a usable owner. Release only the
        // resources created here and leave a retryable empty device identity.
        // Otherwise the next compose skips initialize and binds null handles.
        struct Initialization {
            Impl& owner;bool committed{};
            ~Initialization() {
                if(committed) return;
                if(owner.empty_msaa) SDL_ReleaseGPUTexture(owner.device,owner.empty_msaa);
                if(owner.edge_pipeline) SDL_ReleaseGPUComputePipeline(owner.device,owner.edge_pipeline);
                if(owner.pipeline) SDL_ReleaseGPUComputePipeline(owner.device,owner.pipeline);
                owner.empty_msaa=nullptr;owner.edge_pipeline=owner.pipeline=nullptr;owner.device=nullptr;
            }
        } initialization{*this};
        split_pipelines=std::getenv("STARFOX_TEST_SPLIT_COMPOSITE_PIPELINES")
            && !std::getenv("STARFOX_TEST_UNIFIED_COMPOSITE_PIPELINE");
        const auto make_pipeline=[&](const auto& spirv,const auto& metal,const auto& dxil) {
            SDL_GPUComputePipelineCreateInfo info{};
            if(SDL_GetGPUShaderFormats(d)&SDL_GPU_SHADERFORMAT_SPIRV) {
                info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=spirv;
                info.code_size=sizeof(spirv);info.entrypoint="main";
            } else if(SDL_GetGPUShaderFormats(d)&SDL_GPU_SHADERFORMAT_MSL) {
                info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(metal);
                info.code_size=sizeof(metal)-1;info.entrypoint="main0";
            } else if(SDL_GetGPUShaderFormats(d)&SDL_GPU_SHADERFORMAT_DXIL) {
                info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=dxil;
                info.code_size=sizeof(dxil);info.entrypoint="main";
            } else throw std::runtime_error("GPU composition requires Vulkan, Metal or D3D12");
            info.num_readonly_storage_buffers=8;info.num_readwrite_storage_buffers=5;
            info.num_readonly_storage_textures=3;
            info.num_readwrite_storage_textures=1;info.num_uniform_buffers=1;
            info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            auto* result=create_gpu_compute_pipeline(d,&info);checked(result);return result;
        };
        if(split_pipelines) {
            pipeline=make_pipeline(composite_pixel_shader::spirv,composite_pixel_shader::metal,composite_pixel_shader::dxil);
            edge_pipeline=make_pipeline(composite_edge_shader::spirv,composite_edge_shader::metal,composite_edge_shader::dxil);
        } else pipeline=make_pipeline(composite_shader::spirv,composite_shader::metal,composite_shader::dxil);
        if(std::getenv("STARFOX_TEST_FAIL_COMPOSITE_INITIALIZE"))
            throw std::runtime_error("Injected compositor initialization failure");
        SDL_GPUTextureCreateInfo t{};t.type=SDL_GPU_TEXTURETYPE_2D;t.width=t.height=t.layer_count_or_depth=t.num_levels=1;
        t.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;t.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
        empty_msaa=SDL_CreateGPUTexture(d,&t);checked(empty_msaa);
        status=std::string("GPU composition: ")+SDL_GetGPUDeviceDriver(d);
        initialization.committed=true;
    }
    void buffer(unsigned i,Uint32 bytes) {
        if(buffers[i] && capacities[i]>=bytes) return;
        if(buffers[i]) SDL_ReleaseGPUBuffer(device,buffers[i]);
        buffers[i]=nullptr;
        if(i==1) cachedPaletteValid=false;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
            |((i>=2 && i<=3) || i>=5?SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE:0U),bytes,0};
        buffers[i]=SDL_CreateGPUBuffer(device,&info);checked(buffers[i]);capacities[i]=bytes;
    }
    void transfer(SDL_GPUTransferBuffer*& b,Uint32& capacity,Uint32 bytes,SDL_GPUTransferBufferUsage usage) {
        if(b && capacity>=bytes) return;
        if(b) SDL_ReleaseGPUTransferBuffer(device,b);
        b=nullptr;
        SDL_GPUTransferBufferCreateInfo info{usage,bytes,0};
        b=SDL_CreateGPUTransferBuffer(device,&info);checked(b);capacity=bytes;
    }
    void compose(const GpuRasterOutput& source,Uint32 sourceScale,const Framebuffer* cpu,
        std::span<const Uint8> foreground,const LayerCompositeSettings& settings,std::span<const Rgba8> palette,
        const GpuRasterOutput* late,const GpuCompositeBackground* background,std::span<const Uint8> afterLate,bool worldOnly,
        std::array<std::uint32_t,4> mapping,std::array<float,2> jitter,const Impl* cpuOwner=nullptr) {
        const bool profile=std::getenv("STARFOX_TRACE_GPU_PASS_COST_ALL")!=nullptr;
        const auto begin=profile?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        if(ordered_queue_reuse) {
            if(fence) {retired_fences.push_back(fence);fence=nullptr;}
            // Queue ordering protects inputs, not prematurely recycled fences.
            // Keep at most two older submissions plus the next encoded frame.
            retire(2);
        } else finish();
        const auto waited=profile?std::chrono::steady_clock::now():begin;
        valid=false;
        ownsCpuInputs=false;
        if(cpuOwner && (cpuOwner==this || cpuOwner->device!=device || !cpuOwner->valid
            || !cpuOwner->ownsCpuInputs || !cpuOwner->buffers[0] || !cpuOwner->buffers[1]))
            throw std::runtime_error("Shared CPU composition inputs are not available");
        if(!cpuOwner && !cpu) throw std::runtime_error("Missing CPU composition inputs");
        const auto cpuWidth=cpuOwner?cpuOwner->cpuInputs.width:cpu->stored_width();
        const auto cpuHeight=cpuOwner?cpuOwner->cpuInputs.height:cpu->stored_height();
        const auto cpuScale=cpuOwner?cpuOwner->cpuInputs.scale:cpu->draw_scale();
        const auto logicalWidth=cpuOwner?cpuOwner->cpuInputs.logicalWidth:cpu->width();
        if(!valid_raster_jitter(jitter)) throw std::runtime_error("Invalid composition jitter");
        const auto count=std::size_t(cpuWidth)*cpuHeight;
        const bool custom=std::any_of(mapping.begin(),mapping.end(),[](auto value){return value!=0;});
        if(custom && std::any_of(mapping.begin(),mapping.end(),[](auto value){return !value || value>8192;}))
            throw std::runtime_error("Invalid independent compositor dimensions");
        const auto sourceReferenceWidth=custom?mapping[0]:source.width;
        const auto sourceReferenceHeight=custom?mapping[1]:source.height;
        const auto outputWidth=custom?mapping[2]:cpuWidth;
        const auto outputHeight=custom?mapping[3]:cpuHeight;
        if(!count || count>UINT32_MAX/24 || (!cpuOwner && (palette.empty() || palette.size()>256))
            || (!foreground.empty() && foreground.size()!=count)
            || (!afterLate.empty() && afterLate.size()!=count) || !sourceScale
            || !source.width || !source.height || sourceReferenceWidth%sourceScale || sourceReferenceHeight%sourceScale)
            throw std::runtime_error("Invalid GPU composition inputs");
        if(late && (late->device!=device || !late->pixels || !late->width || !late->height
            || (!custom && (late->width!=cpuWidth || late->height!=cpuHeight)) || late->pixels==buffers[2]
            || late->pixels==buffers[3])) throw std::runtime_error("Invalid late GPU overlay");
        if(background) {
            const auto& b=background->raster;
            if(b.device!=device || !b.pixels || !b.width || !b.height
                || (!custom && (b.width!=cpuWidth || b.height!=cpuHeight))
                || b.pixels==buffers[2] || b.pixels==buffers[3]
                || (!background->cpu_coverage.empty() && background->cpu_coverage.size()!=count))
                throw std::runtime_error("Invalid resident GPU background");
            if(background->margin_origin && (!background->margin_width
                || background->margin_origin>=logicalWidth
                || background->margin_width>logicalWidth-background->margin_origin))
                throw std::runtime_error("Invalid GPU background margins");
        }
        const Uint32 bytes=Uint32(count*4);
        const Uint32 outputBytes=outputWidth*outputHeight*4;
        if(width!=outputWidth || height!=outputHeight) {
            if(rgba) SDL_ReleaseGPUTexture(device,rgba);
            rgba=nullptr;width=height=0;
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
            info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width=outputWidth;info.height=outputHeight;info.layer_count_or_depth=1;info.num_levels=1;
            rgba=SDL_CreateGPUTexture(device,&info);checked(rgba);width=info.width;height=info.height;
        }
        bool uniform=false,striped=false,gpuUniform=false,gpuStriped=false;
        Uint32 uniformValue=0,stripeValue=0,cpuUploadBytes=0,paletteOffset=0;
        std::array<std::pair<Uint32,Uint32>,2> stripes{};
        std::array<Uint32,256> paletteWords{};
        bool paletteChanged=false;
        dirtySpans.clear();
        if(cpuOwner) {
            const auto& input=cpuOwner->cpuInputs;
            gpuUniform=input.uniform;gpuStriped=input.striped;
            uniformValue=input.value;stripeValue=input.stripeValue;stripes=input.stripes;
            // The next ordinary compose cannot reuse this owner's older host
            // upload cache after a shared-input submission.
            cachedUniformValid=cachedPackedValid=cachedPaletteValid=false;
        } else {
            const auto same_mask=[](std::span<const Uint8> mask,bool expected) {
                return mask.empty()? !expected : std::all_of(mask.begin(),mask.end(),
                    [expected](Uint8 value){return bool(value)==expected;});
            };
            const auto backgroundCoverage=background?background->cpu_coverage:std::span<const Uint8>{};
            const auto packedAt=[&](std::size_t i) {
                return Uint32(cpu->pixels()[i])
                    |(cpu->layer_tags_enabled()?Uint32(cpu->layer_tags()[i])<<8:0U)
                    |(!foreground.empty() && foreground[i]?0x80000000U:0U)
                    |(!backgroundCoverage.empty() && backgroundCoverage[i]?0x40000000U:0U)
                    |(!afterLate.empty() && afterLate[i]?0x20000000U:0U);
            };
            const auto cached=cachedUniformValue;
            const bool allowCpuCache=!std::getenv("STARFOX_DISABLE_UNIFORM_CPU_CACHE")
                && !std::getenv("STARFOX_DISABLE_CPU_UPLOAD_CACHE");
            const bool reuseCpu=allowCpuCache
                && cachedUniformValid && cachedUniformBytes==bytes
                && std::all_of(cpu->pixels().begin(),cpu->pixels().end(),
                    [cached](Uint8 value){return value==Uint8(cached);})
                && (cpu->layer_tags_enabled()
                    ?std::all_of(cpu->layer_tags().begin(),cpu->layer_tags().end(),
                        [cached](Uint8 value){return value==Uint8(cached>>8);})
                    : Uint8(cached>>8)==0)
                && same_mask(foreground,(cached&0x80000000U)!=0)
                && same_mask(backgroundCoverage,(cached&0x40000000U)!=0)
                && same_mask(afterLate,(cached&0x20000000U)!=0);
            uniform=reuseCpu;
            uniformValue=reuseCpu?cached:0U;
            if(!uniform && allowCpuCache && (cachedUniformValid || !cachedPackedValid)) {
                const Uint8 colour=cpu->pixels()[0];
                const Uint8 tag=cpu->layer_tags_enabled()?cpu->layer_tags()[0]:0U;
                const bool front=!foreground.empty() && foreground[0];
                const bool back=!backgroundCoverage.empty() && backgroundCoverage[0];
                const bool last=!afterLate.empty() && afterLate[0];
                uniform=std::all_of(cpu->pixels().begin(),cpu->pixels().end(),
                        [colour](Uint8 value){return value==colour;})
                    && (!cpu->layer_tags_enabled() || std::all_of(cpu->layer_tags().begin(),cpu->layer_tags().end(),
                        [tag](Uint8 value){return value==tag;}))
                    && same_mask(foreground,front) && same_mask(backgroundCoverage,back)
                    && same_mask(afterLate,last);
                if(uniform) uniformValue=Uint32(colour)|(Uint32(tag)<<8)
                    |(front?0x80000000U:0U)|(back?0x40000000U:0U)|(last?0x20000000U:0U);
            }
            // A full-height cartridge border is often two constant vertical
            // strips over an otherwise uniform CPU backing. Describe that exact
            // pattern in constants instead of packing/uploading the whole image.
            if(!uniform && allowCpuCache && cpuWidth>1) {
                const Uint32 rowWidth=cpuWidth;
                const auto base=packedAt(0);
                bool candidate=true;
                unsigned stripeCount=0;
                for(Uint32 x=0;x<rowWidth && candidate;) {
                    const auto value=packedAt(x);
                    if(value==base) {++x;continue;}
                    if(stripeCount>=stripes.size() || (stripeCount && value!=stripeValue)) {
                        candidate=false;break;
                    }
                    stripeValue=value;
                    const auto left=x;
                    while(x<rowWidth && packedAt(x)==stripeValue) ++x;
                    stripes[stripeCount++]={left,x};
                }
                if(candidate && stripeCount) {
                    for(Uint32 y=1;y<cpuHeight && candidate;++y)
                        for(Uint32 x=0;x<rowWidth;++x) {
                            const bool inside=(x>=stripes[0].first && x<stripes[0].second)
                                || (x>=stripes[1].first && x<stripes[1].second);
                            if(packedAt(std::size_t(y)*rowWidth+x)!=(inside?stripeValue:base)) {
                                candidate=false;break;
                            }
                        }
                    if(candidate) {striped=true;uniformValue=base;}
                }
            }
            dirtySpans.clear();
            if(!uniform && !striped) {
                const bool comparePrevious=allowCpuCache && cachedPackedValid && capacities[0]>=bytes
                    && cachedUniformBytes==bytes && packed.size()==count;
                packed.resize(count);
                uniform=true;
                for(Uint32 y=0;y<cpuHeight;++y) {
                    const auto rowStart=std::size_t(y)*cpuWidth;
                    Uint32 firstChanged=cpuWidth,lastChanged=0;
                    for(Uint32 x=0;x<cpuWidth;++x) {
                        const auto i=rowStart+x;
                        const Uint32 value=packedAt(i);
                        if(comparePrevious && packed[i]!=value) {
                            firstChanged=std::min(firstChanged,x);lastChanged=x;
                        }
                        packed[i]=value;
                        if(i==0) uniformValue=value;
                        else if(value!=uniformValue) uniform=false;
                    }
                    if(comparePrevious && firstChanged<cpuWidth) {
                        const auto start=Uint32(rowStart+firstChanged),end=Uint32(rowStart+lastChanged+1);
                        if(!dirtySpans.empty() && (dirtySpans.back().second-1)/cpuWidth==y-1
                            && start-dirtySpans.back().second<=64U)
                            dirtySpans.back().second=end;
                        else dirtySpans.emplace_back(start,end);
                    }
                }
                if(!comparePrevious) dirtySpans.emplace_back(0,Uint32(count));
            }
            gpuUniform=allowCpuCache && uniform;
            gpuStriped=allowCpuCache && striped;
            if(gpuUniform || gpuStriped) dirtySpans.clear();
            // Many tiny copy commands cost more than a contiguous upload. Keep
            // sparse updates for local overlays, but cap command fan-out.
            if(dirtySpans.size()>64) {
                dirtySpans.clear();dirtySpans.emplace_back(0,Uint32(count));
            }
            for(const auto& [start,end]:dirtySpans) cpuUploadBytes+=(end-start)*4;
            buffer(0,(gpuUniform || gpuStriped)?4U:bytes);buffer(1,1024);
            for(unsigned i=0;i<paletteWords.size();++i) {
                const auto c=palette[std::min<std::size_t>(i,palette.size()-1)];
                paletteWords[i]=c.r|(Uint32(c.g)<<8)|(Uint32(c.b)<<16)|(Uint32(c.a)<<24);
            }
            paletteChanged=!cachedPaletteValid || paletteWords!=cachedPalette;
            paletteOffset=dirtySpans.empty()?0U:bytes;
            if(cpuUploadBytes || paletteChanged)
                transfer(upload,uploadSize,paletteOffset+(paletteChanged?1024U:0U),SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        }
        buffer(2,outputBytes);
        buffer(3,outputBytes*4);buffer(4,16);buffer(5,8);
        for(unsigned i=2;i<8;++i)
            if((source.geometry_depth==buffers[i] && source.geometry_depth)
                || (source.motion==buffers[i] && source.motion))
                throw std::runtime_error("Temporal inputs alias compositor output");
        has_depth=source.geometry_depth!=nullptr;has_motion=source.motion!=nullptr;
        buffer(6,has_depth?outputBytes:4);buffer(7,has_motion?outputBytes*4:16);
        // The resident storage buffer survives submissions. Reuse uniform
        // backings without repacking, and upload only changed row runs for
        // nonuniform CPU overlays. The palette remains independent.
        if(!cpuOwner && std::getenv("STARFOX_TRACE_GPU_CPU_UPLOAD")) {
            const auto stripePixels=std::size_t(stripes[0].second-stripes[0].first
                +stripes[1].second-stripes[1].first)*cpuHeight;
            const auto tally=[&](auto predicate) {
                if(gpuUniform) return predicate(uniformValue)?count:std::size_t{0};
                if(gpuStriped) return (predicate(uniformValue)?count-stripePixels:std::size_t{0})
                    +(predicate(stripeValue)?stripePixels:std::size_t{0});
                return std::size_t(std::count_if(packed.begin(),packed.end(),predicate));
            };
            const auto meaningful=tally([](Uint32 value){return value!=0;});
            const auto ink=tally([](Uint32 value){return (value&255U)!=0;});
            const auto covered=tally([](Uint32 value){return (value&0xe0000000U)!=0;});
            const auto [least,greatest]=gpuUniform?std::pair{uniformValue,uniformValue}:gpuStriped?
                std::pair{std::min(uniformValue,stripeValue),std::max(uniformValue,stripeValue)}:
                [&]{const auto [lo,hi]=std::minmax_element(packed.begin(),packed.end());return std::pair{*lo,*hi};}();
            std::cerr<<"gpu-cpu-upload: pixels="<<count<<" meaningful="<<meaningful
                <<" ink="<<ink<<" coverage="<<covered<<" min="<<least
                <<" max="<<greatest<<" bytes="<<cpuUploadBytes<<'\n';
            std::cerr<<"gpu-palette-upload: bytes="<<(paletteChanged?1024U:0U)<<'\n';
            if(gpuStriped) std::cerr<<"gpu-cpu-stripes: "<<stripes[0].first<<'-'<<stripes[0].second
                <<' '<<stripes[1].first<<'-'<<stripes[1].second<<" value="<<stripeValue<<'\n';
            if(!gpuUniform && cpuUploadBytes && ink) {
                Uint32 minX=cpuWidth,minY=cpuHeight,maxX=0,maxY=0;
                for(std::size_t i=0;i<packed.size();++i) if(packed[i]&255U) {
                    const auto x=Uint32(i%cpuWidth),y=Uint32(i/cpuWidth);
                    minX=std::min(minX,x);minY=std::min(minY,y);
                    maxX=std::max(maxX,x);maxY=std::max(maxY,y);
                }
                std::cerr<<"gpu-cpu-content-bounds: "<<minX<<','<<minY<<".."<<maxX<<','<<maxY<<'\n';
            }
        }
        if(cpuUploadBytes || paletteChanged) {
            auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));checked(mapped);
            for(const auto& [start,end]:dirtySpans)
                std::memcpy(mapped+std::size_t(start)*4,packed.data()+start,std::size_t(end-start)*4);
            if(paletteChanged)
                std::memcpy(mapped+paletteOffset,paletteWords.data(),1024);
            SDL_UnmapGPUTransferBuffer(device,upload);
        }
        const auto prepared=profile?std::chrono::steady_clock::now():begin;
        command=SDL_AcquireGPUCommandBuffer(device);checked(command);
        if(cpuUploadBytes || paletteChanged) {
            auto* copy=SDL_BeginGPUCopyPass(command);checked(copy);
            for(const auto& [start,end]:dirtySpans) {
                SDL_GPUTransferBufferLocation from{upload,start*4};
                SDL_GPUBufferRegion to{buffers[0],start*4,(end-start)*4};
                SDL_UploadToGPUBuffer(copy,&from,&to,false);
            }
            if(paletteChanged) {
                SDL_GPUTransferBufferLocation paletteFrom{upload,paletteOffset};
                SDL_GPUBufferRegion paletteTo{buffers[1],0,1024};
                SDL_UploadToGPUBuffer(copy,&paletteFrom,&paletteTo,false);
            }
            SDL_EndGPUCopyPass(copy);
        }
        Sint32 constants[]{Sint32(cpuWidth),Sint32(cpuHeight),Sint32(source.width),Sint32(source.height),
            Sint32(cpuScale),Sint32(sourceScale),
            (settings.mosaic & settings.mosaic_layer_mask)?Sint32((settings.mosaic>>4)+1):1,source.surfaces?1:0,
            settings.offset_x,settings.offset_y,settings.clip_left,settings.clip_top,
            settings.clip_right,settings.clip_bottom,settings.mosaic_origin_x,settings.mosaic_origin_y,
            late?1:0,background?1:0,1,background?Sint32(background->margin_origin):0,
            background?Sint32(background->margin_width):0,background && background->repair_transparent_margins?1:0,has_depth?1:0,has_motion?1:0,
            worldOnly?1:0,std::bit_cast<Sint32>(jitter[0]),std::bit_cast<Sint32>(jitter[1]),background && background->match_right_margin?1:0,Sint32(sourceReferenceWidth),Sint32(sourceReferenceHeight),Sint32(width),Sint32(height),
            late?Sint32(late->width):0,late?Sint32(late->height):0,
            background?Sint32(background->raster.width):0,background?Sint32(background->raster.height):0,
            gpuUniform?1:0,std::bit_cast<Sint32>(uniformValue),gpuStriped?1:0,
            std::bit_cast<Sint32>(stripeValue),Sint32(stripes[0].first),Sint32(stripes[0].second),
            Sint32(stripes[1].first),Sint32(stripes[1].second),source.msaa_color?1:0,
            late && late->msaa_color?1:0,background && background->raster.msaa_color?1:0,0};
        SDL_GPUStorageTextureReadWriteBinding texture{};texture.texture=rgba;
        SDL_GPUStorageBufferReadWriteBinding outputs[5]{};outputs[0].buffer=buffers[2];outputs[1].buffer=buffers[3];outputs[2].buffer=buffers[5];outputs[3].buffer=buffers[6];outputs[4].buffer=buffers[7];
        SDL_GPUBuffer* inputs[]{cpuOwner?cpuOwner->buffers[0]:buffers[0],static_cast<SDL_GPUBuffer*>(source.pixels),
            source.surfaces?static_cast<SDL_GPUBuffer*>(source.surfaces):buffers[4],cpuOwner?cpuOwner->buffers[1]:buffers[1],
            late?static_cast<SDL_GPUBuffer*>(late->pixels):buffers[4],
            background?static_cast<SDL_GPUBuffer*>(background->raster.pixels):buffers[4],
            has_depth?static_cast<SDL_GPUBuffer*>(source.geometry_depth):buffers[4],
            has_motion?static_cast<SDL_GPUBuffer*>(source.motion):buffers[4]};
        for(unsigned phase=constants[19]?0U:1U;phase<2;++phase) {
            constants[18]=Sint32(phase);SDL_PushGPUComputeUniformData(command,0,constants,sizeof(constants));
            auto* pass=SDL_BeginGPUComputePass(command,&texture,1,outputs,5);checked(pass);
            SDL_BindGPUComputePipeline(pass,phase || !split_pipelines?pipeline:edge_pipeline);
            SDL_BindGPUComputeStorageBuffers(pass,0,inputs,8);
            SDL_GPUTexture* aa_inputs[]{source.msaa_color?static_cast<SDL_GPUTexture*>(source.msaa_color):empty_msaa,
                late && late->msaa_color?static_cast<SDL_GPUTexture*>(late->msaa_color):empty_msaa,
                background && background->raster.msaa_color?static_cast<SDL_GPUTexture*>(background->raster.msaa_color):empty_msaa};
            SDL_BindGPUComputeStorageTextures(pass,0,aa_inputs,3);
            SDL_DispatchGPUCompute(pass,phase?(width+7)/8:1,phase?(height+7)/8:1,1);SDL_EndGPUComputePass(pass);
        }
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;checked(fence);valid=true;
        if(ordered_queue_reuse && !reported_ordered_reuse
            && (std::getenv("STARFOX_TRACE_GPU") || std::getenv("STARFOX_TEST_STEREO_RESULT"))) {
            std::cerr<<"composite-ordered-queue: submitted\n";reported_ordered_reuse=true;
        }
        if(!reported_pipeline_mode && std::getenv("STARFOX_TEST_COMPOSITE_RESULT")) {
            std::cerr<<"composite-pipelines: "<<(split_pipelines?"split":"unified")<<'\n';
            reported_pipeline_mode=true;
        }
        if(paletteChanged) {cachedPalette=paletteWords;cachedPaletteValid=true;}
        if(!cpuOwner) {
            cachedPackedValid=!(gpuUniform || gpuStriped);cachedUniformValid=uniform;
            cachedUniformValue=uniformValue;cachedUniformBytes=bytes;
            cpuInputs={cpuWidth,cpuHeight,cpuScale,logicalWidth,gpuUniform,gpuStriped,uniformValue,stripeValue,stripes};
            ownsCpuInputs=true;
        }
        lastCpuUploadBytes=cpuUploadBytes;
        lastPaletteUploadBytes=paletteChanged?1024U:0U;
        if(profile) {
            const auto done=std::chrono::steady_clock::now();
            const auto us=[](auto a,auto b){return std::chrono::duration_cast<std::chrono::microseconds>(b-a).count();};
            std::cerr<<"gpu-composite-cost-us wait="<<us(begin,waited)<<" prepare="<<us(waited,prepared)
                <<" encode-submit="<<us(prepared,done)<<" upload="<<cpuUploadBytes
                <<" uniform="<<gpuUniform<<" stripes="<<gpuStriped<<" shared="<<bool(cpuOwner)<<'\n';
        }
    }
    void read_motion(bool history_valid,std::vector<MotionBlurGuide>& result) {
        if(!valid||!has_depth||!has_motion||!buffers[2]||!buffers[6]||!buffers[7])
            throw std::runtime_error("Native motion/depth is not available");
        const auto count=std::size_t(width)*height;
        if(count>UINT32_MAX/24) throw std::runtime_error("Motion guide readback is too large");
        finish();const Uint32 bytes=Uint32(count*4);
        transfer(download,downloadSize,bytes*6,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        command=SDL_AcquireGPUCommandBuffer(device);checked(command);
        auto* pass=SDL_BeginGPUCopyPass(command);checked(pass);
        const unsigned indices[]{2,6,7};
        for(unsigned i=0;i<3;++i) {
            SDL_GPUBufferRegion source{buffers[indices[i]],0,i==2?bytes*4:bytes};
            SDL_GPUTransferBufferLocation target{download,i*bytes};
            SDL_DownloadFromGPUBuffer(pass,&source,&target);
        }
        SDL_EndGPUCopyPass(pass);fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;checked(fence);finish();
        const auto* mapped=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,download,false));checked(mapped);
        try {
            const bool ok=motion_blur_guides({reinterpret_cast<const Uint32*>(mapped),count},
                {reinterpret_cast<const float*>(mapped+bytes),count},
                {reinterpret_cast<const std::array<float,4>*>(mapped+bytes*2),count},history_valid,result);
            if(!ok) throw std::runtime_error("Invalid native motion guide payload");
        } catch(...) {SDL_UnmapGPUTransferBuffer(device,download);throw;}
        SDL_UnmapGPUTransferBuffer(device,download);
    }
    void read(Framebuffer& target,std::vector<Uint8>& colours,SurfaceBuffer* surfaces) {
        if(!device || !rgba || !buffers[2] || (surfaces && !buffers[3]))
            throw std::runtime_error("GPU composition readback source is missing");
        if(!valid || target.stored_width()!=width || target.stored_height()!=height
            || (surfaces && (surfaces->width()!=width || surfaces->height()!=height)))
            throw std::runtime_error("Invalid composition readback dimensions");
        finish();const Uint32 bytes=width*height*4;
        transfer(download,downloadSize,bytes*6,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        command=SDL_AcquireGPUCommandBuffer(device);checked(command);
        auto* pass=SDL_BeginGPUCopyPass(command);checked(pass);
        SDL_GPUTextureRegion tex{rgba,0,0,0,0,0,width,height,1};SDL_GPUTextureTransferInfo out{download,0,0,0};
        SDL_DownloadFromGPUTexture(pass,&tex,&out);
        for(unsigned i=0;i<(surfaces?2U:1U);++i) {
            SDL_GPUBufferRegion from{buffers[2+i],0,i?bytes*4:bytes};SDL_GPUTransferBufferLocation to{download,i?bytes*2:bytes};
            SDL_DownloadFromGPUBuffer(pass,&from,&to);
        }
        SDL_EndGPUCopyPass(pass);fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;checked(fence);finish();
        const auto* mapped=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,download,false));checked(mapped);
        colours.assign(mapped,mapped+bytes);
        const auto* values=reinterpret_cast<const Uint32*>(mapped+bytes);
        const auto* normals=reinterpret_cast<const float*>(mapped+bytes*2);
        if(surfaces) surfaces->clear();
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto i=std::size_t(y)*width+x;target.set_stored(x,y,Uint8(values[i]),PixelLayer((values[i]>>8)&255));
            if(surfaces && (values[i]&(1U<<24))) surfaces->set(x,y,
                {normals[i*4],normals[i*4+1],normals[i*4+2],normals[i*4+3]},Uint8(values[i]>>16));
        }
        SDL_UnmapGPUTransferBuffer(device,download);
    }
};
#else
struct GpuComposite::Impl {std::string status{"GPU composition unavailable on this build"};};
#endif
GpuComposite::GpuComposite():impl_(std::make_unique<Impl>()) {}
GpuComposite::~GpuComposite()=default;
void GpuComposite::release_device() noexcept {impl_.reset();}
const std::string& GpuComposite::status() const {static const std::string empty{"GPU composition released"};return impl_?impl_->status:empty;}
GpuCompositeOutput GpuComposite::output() const {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(impl_ && impl_->valid) return {impl_->device,impl_->rgba,impl_->buffers[2],impl_->buffers[3],impl_->width,impl_->height,
        impl_->has_depth?impl_->buffers[6]:nullptr,impl_->has_motion?impl_->buffers[7]:nullptr};
#endif
    return {};
}
std::size_t GpuComposite::last_cpu_upload_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_ && impl_->valid ? impl_->lastCpuUploadBytes : 0U;
#else
    return 0U;
#endif
}
std::size_t GpuComposite::last_palette_upload_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_ && impl_->valid ? impl_->lastPaletteUploadBytes : 0U;
#else
    return 0U;
#endif
}
bool GpuComposite::compose(const GpuRasterOutput& source,std::uint32_t scale,const Framebuffer& cpu,
    std::span<const std::uint8_t> foreground,const LayerCompositeSettings& settings,std::span<const Rgba8> palette,
    const GpuRasterOutput* late,const GpuCompositeBackground* background,std::span<const std::uint8_t> afterLate,bool worldOnly,
    std::array<std::uint32_t,4> mapping,std::array<float,2> jitter) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!source.device || !source.pixels) return false;
    if(!impl_ || impl_->device!=source.device) impl_=std::make_unique<Impl>();
    try {if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(source.device));
        impl_->ordered_queue_reuse=ordered_queue_reuse_;
        impl_->compose(source,scale,&cpu,foreground,settings,palette,late,background,afterLate,worldOnly,mapping,jitter);return true;}
    catch(const std::exception& e) {if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->valid=false;impl_->cachedUniformValid=impl_->cachedPackedValid=impl_->cachedPaletteValid=false;
        impl_->status=e.what();return false;}
#else
    (void)source;(void)scale;(void)cpu;(void)foreground;(void)settings;(void)palette;(void)late;(void)background;(void)afterLate;(void)worldOnly;(void)mapping;return false;
#endif
}
bool GpuComposite::compose_with_cpu_inputs(const GpuComposite& owner,const GpuRasterOutput& source,
    std::uint32_t scale,const LayerCompositeSettings& settings,const GpuRasterOutput* late,
    const GpuCompositeBackground* background,bool worldOnly,std::array<std::uint32_t,4> mapping,
    std::array<float,2> jitter) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(&owner==this || !owner.impl_ || !source.device || !source.pixels) return false;
    if(!impl_ || impl_->device!=source.device) impl_=std::make_unique<Impl>();
    try {
        if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(source.device));
        impl_->ordered_queue_reuse=ordered_queue_reuse_;
        impl_->compose(source,scale,nullptr,{},settings,{},late,background,{},worldOnly,mapping,jitter,owner.impl_.get());
        return true;
    } catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->valid=false;impl_->ownsCpuInputs=false;
        impl_->cachedUniformValid=impl_->cachedPackedValid=impl_->cachedPaletteValid=false;
        impl_->status=e.what();return false;
    }
#else
    (void)owner;(void)source;(void)scale;(void)settings;(void)late;(void)background;(void)worldOnly;(void)mapping;(void)jitter;
    return false;
#endif
}
bool GpuComposite::readback_motion_guides(bool history_valid,std::vector<MotionBlurGuide>& guides) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_||!impl_->valid) return false;
    try {impl_->read_motion(history_valid,guides);return true;}
    catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=e.what();return false;
    }
#else
    (void)history_valid;(void)guides;return false;
#endif
}
bool GpuComposite::readback(Framebuffer& target,std::vector<std::uint8_t>& rgba,SurfaceBuffer* surfaces) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_ || !impl_->valid) return false;
    try {impl_->read(target,rgba,surfaces);return true;}
    catch(const std::exception& e) {if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=e.what();return false;}
#else
    (void)target;(void)rgba;(void)surfaces;return false;
#endif
}
}
