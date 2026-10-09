// Readbacks are independent fixture oracles, never used by the renderer.
#include "starfox/render/gpu_calibrated_ray_composite.hpp"
#include "starfox/render/gpu_calibrated_scene.hpp"
#include "starfox/render/effects.hpp"
#include "starfox/render/frame_persistence.hpp"
#include "starfox/render/gpu_calibrated_persistence.hpp"
#include "starfox/render/gpu_calibrated_global.hpp"
#include "starfox/render/global_enhancements.hpp"
#include "starfox/render/gpu_calibrated_bloom.hpp"
#include "starfox/render/bloom.hpp"
#include "starfox/render/hdr_effect.hpp"
#include "starfox/render/chromatic_aberration.hpp"
#include "starfox/render/gpu_calibrated_exposure.hpp"
#include "starfox/render/adaptive_exposure.hpp"
#include "starfox/render/gpu_calibrated_aa.hpp"
#include "starfox/render/gpu_calibrated_fsr1.hpp"
#include "starfox/render/gpu_smaa.hpp"
#include "starfox/render/gpu_calibrated_temporal_aa.hpp"
#include "starfox/render/gpu_calibrated_reflection_history.hpp"
#include "starfox/render/temporal_aa.hpp"
#include "../tests/calibrated_pattern_oracle.hpp"
#include "../tests/calibrated_edge_oracle.hpp"
#include "starfox/render/gpu_calibrated_depth.hpp"
#include "starfox/render/depth_enhancements.hpp"
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "reflected_liquid_oracle.hpp"
#if defined(STARFOX_CURVED_CACHE_TRACE_AVAILABLE)
#include "calibrated_curved_trace_0_dxil.hpp"
#include "calibrated_curved_trace_1_dxil.hpp"
#endif
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace {
using namespace starfox;
using Bytes=std::vector<unsigned char>;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUCommandBuffer* command{};
    ~Gpu() {if(device) {if(command) SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);SDL_DestroyGPUDevice(device);}SDL_Quit();}
    void begin() {check(!command,"Fixture command already open");command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());}
    void submit() {const bool ok=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(ok,SDL_GetError());}
};
struct Texture {
    Gpu& gpu;SDL_GPUTexture* texture{};unsigned width,height;SDL_GPUTextureFormat format;
    Texture(Gpu& g,unsigned w,unsigned h,SDL_GPUTextureFormat f):gpu(g),width(w),height(h),format(f) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=f;info.width=w;info.height=h;
        info.layer_count_or_depth=info.num_levels=1;
        info.usage=f==SDL_GPU_TEXTUREFORMAT_D32_FLOAT?SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
            :SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        texture=SDL_CreateGPUTexture(gpu.device,&info);check(texture,SDL_GetError());
    }
    ~Texture() {if(texture) SDL_ReleaseGPUTexture(gpu.device,texture);}
    Texture(const Texture&)=delete;Texture& operator=(const Texture&)=delete;
    Bytes read(SDL_GPUTexture* borrowed=nullptr) {
        check(!gpu.command,"Readback would interfere with unsubmitted scene");gpu.begin();
        const unsigned bytes=width*height*(format==SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT?16:4);
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes,0};
        auto* download=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(download,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        const SDL_GPUTextureRegion region{borrowed?borrowed:texture,0,0,0,0,0,width,height,1};
        SDL_GPUTextureTransferInfo target{};target.transfer_buffer=download;target.pixels_per_row=width;target.rows_per_layer=height;
        SDL_DownloadFromGPUTexture(copy,&region,&target);SDL_EndGPUCopyPass(copy);gpu.submit();check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,download,false));check(mapped,SDL_GetError());
        Bytes result(mapped,mapped+bytes);SDL_UnmapGPUTransferBuffer(gpu.device,download);SDL_ReleaseGPUTransferBuffer(gpu.device,download);
        if(format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
            for(unsigned i=0;i<result.size();i+=4) std::swap(result[i],result[i+2]);
        return result;
    }
    void upload(Bytes bytes) {
        check(bytes.size()==width*height*(format==SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT?16:4),"Bad fixture texture extent");
        if(format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
            for(unsigned i=0;i<bytes.size();i+=4) std::swap(bytes[i],bytes[i+2]);
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,unsigned(bytes.size()),0};
        auto* upload=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(upload,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,upload,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,bytes.data(),bytes.size());SDL_UnmapGPUTransferBuffer(gpu.device,upload);
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        SDL_GPUTextureTransferInfo from{};from.transfer_buffer=upload;from.pixels_per_row=width;from.rows_per_layer=height;
        const SDL_GPUTextureRegion to{texture,0,0,0,0,0,width,height,1};SDL_UploadToGPUTexture(copy,&from,&to,false);
        SDL_EndGPUCopyPass(copy);SDL_ReleaseGPUTransferBuffer(gpu.device,upload);
    }
};
struct Buffer {
    Gpu& gpu;SDL_GPUBuffer* buffer{};
    Buffer(Gpu& g,std::span<const unsigned char> bytes):gpu(g) {
        const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,unsigned(bytes.size()),0};
        buffer=SDL_CreateGPUBuffer(gpu.device,&info);check(buffer,SDL_GetError());
        const SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,unsigned(bytes.size()),0};
        auto* upload=SDL_CreateGPUTransferBuffer(gpu.device,&transfer);check(upload,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,upload,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,bytes.data(),bytes.size());SDL_UnmapGPUTransferBuffer(gpu.device,upload);
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{upload,0};const SDL_GPUBufferRegion to{buffer,0,unsigned(bytes.size())};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);SDL_ReleaseGPUTransferBuffer(gpu.device,upload);
    }
    ~Buffer() {SDL_ReleaseGPUBuffer(gpu.device,buffer);}
};
bool srgb(SDL_GPUTextureFormat format) {return format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
double decode(double c) {return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);}
double encode(double c) {return c<=.0031308?12.92*c:1.055*std::pow(c,1/2.4)-.055;}
Bytes oracle(const Bytes& source,const Bytes& coverage,unsigned width,unsigned height,
    std::span<const unsigned char> shadow,unsigned shadow_row,std::span<const unsigned char> reflection,
    unsigned reflection_row,SDL_GPUTextureFormat format,float shadow_strength,float reflection_strength) {
    auto expected=source;
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const unsigned at=(y*width+x)*4;if(!coverage[at]) continue;
        const unsigned amount=shadow.empty()?0:shadow_row?shadow[y*shadow_row+x]:
            [&] {unsigned value{};std::memcpy(&value,shadow.data()+at,4);return std::min(255U,value);}();
        const auto* reflected=reflection.empty()?nullptr:reflection.data()+y*reflection_row+x*4;
        for(unsigned c=0;c<3;++c) {
            double value=source[at+c]/255.;if(srgb(format)) value=decode(value);
            value*=1-amount/255.*shadow_strength;
            if(reflected && reflected[3]==255) {
                double target=reflected[c]/255.;if(srgb(format)) target=decode(target);
                value=std::lerp(value,target,reflection_strength*coverage[at+1]/255.);
            }
            if(srgb(format)) value=encode(value);
            expected[at+c]=static_cast<unsigned char>(std::lround(std::clamp(value,0.,1.)*255));
        }
    }
    return expected;
}
void compare(const Bytes& actual,const Bytes& expected,const char* error,unsigned tolerance=1) {
    check(actual.size()==expected.size(),"Composite oracle extent changed");
    for(unsigned i=0;i<actual.size();++i) if(unsigned(std::abs(int(actual[i])-int(expected[i])))>(i%4==3?0:tolerance)) {
        std::cerr<<error<<" byte="<<i<<" actual="<<unsigned(actual[i])<<" expected="<<unsigned(expected[i])<<'\n';throw std::runtime_error(error);
    }
}
void word(Bytes& bytes,unsigned at,unsigned value) {std::memcpy(bytes.data()+at,&value,4);}
#include "check_calibrated_reflection_history.inc"
#include "check_calibrated_reflection_lobes.inc"
#include "check_calibrated_reflection_paths.inc"
#include "check_calibrated_curved_trace.inc"
#include "check_calibrated_reflection_curved.inc"
#include "check_calibrated_water_guides.inc"
void ground_transport_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=12,h=8;
    Texture source(gpu,w,h,format),receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
    render::GpuCalibratedRayComposite compositor;check(compositor.initialize(gpu.device,format),compositor.status().c_str());
    Bytes color(w*h*4),coverage(w*h*4),reflected(w*h*4);
    for(unsigned p=0;p<w*h;++p) {
        const unsigned at=p*4;const bool ground=p%3==0,model=p%3==1;
        for(unsigned c=0;c<3;++c) {color[at+c]=static_cast<unsigned char>(31+c*47);reflected[at+c]=static_cast<unsigned char>(191-c*41);}
        color[at+3]=static_cast<unsigned char>(81+p);reflected[at+3]=p%4==0?253:p%2?255:254;
        coverage[at]=ground?1:model?255:0;coverage[at+1]=255;coverage[at+2]=ground?1:model?2:0;
    }
    gpu.begin();source.upload(color);receiver.upload(coverage);Buffer reflection(gpu,reflected);gpu.submit();
    const render::shadows::GpuReflectionOutput output{gpu.device,reflection.buffer,w,h,w*4};
    for(float strength:{0.F,.5F,1.F}) {
        gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
            nullptr,&output,1,strength),compositor.status().c_str());gpu.submit();
        auto expected=color;
        for(unsigned p=0;p<w*h;++p) {
            const unsigned at=p*4;
            for(unsigned c=0;c<3;++c) {
                if((coverage[at] && reflected[at+3]==253) || (coverage[at]==1 && reflected[at+3]==254)) expected[at+c]=reflected[at+c];
                if(coverage[at]==255 && reflected[at+3]==255) {
                    double a=color[at+c]/255.,b=reflected[at+c]/255.;
                    if(srgb(format)) {a=decode(a);b=decode(b);}
                    double value=std::lerp(a,b,strength);if(srgb(format)) value=encode(value);
                    expected[at+c]=static_cast<unsigned char>(std::lround(value*255));
                }
            }
        }
        compare(target.read(),expected,"Ground transport blended twice/leaked into foreground or changed alpha",srgb(format)?1:0);
    }
    std::cout<<"  Distinct ground/model/protected ray words, exact ground replacement, quality applied once and source alpha passed\n";
    const auto layout=*render::shadows::native_water_layers(w,h);
    Bytes layered(layout.storage_bytes,0);
    std::copy(reflected.begin(),reflected.end(),layered.begin());
    auto expected=color;
    for(unsigned p=0;p<w*h;++p) {
        const unsigned at=p*4,world=layout.world_offset+at;
        const bool liquid=p%4!=0;
        layered[world]=23;layered[world+1]=117;layered[world+2]=209;layered[world+3]=liquid?253:0;
        if(liquid && coverage[at]) std::copy_n(layered.begin()+world,3,expected.begin()+at);
    }
    gpu.begin();Buffer layers(gpu,layered);gpu.submit();
    render::shadows::GpuReflectionOutput underlay{gpu.device,layers.buffer,w,h,w*4,layout};
    gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
        nullptr,&underlay,1,0,0,1,{},true),compositor.status().c_str());gpu.submit();
    compare(target.read(),expected,"Hidden water underlay read primary model rays or changed sky/protected alpha",srgb(format)?1:0);
    gpu.begin();
    for(unsigned fault=0;fault<4;++fault) {
        auto bad=underlay;
        if(fault==0) bad.water_layers.world_offset+=4;
        if(fault==1) bad.water_layers.surface_offset-=4;
        if(fault==2) bad.water_layers.storage_bytes-=4;
        if(fault==3) bad.water_layers={};
        check(!compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
            nullptr,&bad,1,0,0,1,{},true),"Malformed native water layers accepted");
    }
    check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
        nullptr,&underlay,1,0,0,1,{},true),compositor.status().c_str());
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h),compositor.status().c_str());gpu.submit();
    compare(target.read(),color,"Cancelled hidden water pass contaminated exact OFF restoration",0);
    std::cout<<"  Separate water underlay, protected sky/alpha, bounded layer offsets, malformed input and cancellation passed\n";
}
void strided_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=9,h=3,packed_row=16,reflection_row=40;
    Texture source(gpu,w,h,format),receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
    render::GpuCalibratedRayComposite compositor;check(compositor.initialize(gpu.device,format),compositor.status().c_str());
    Bytes color(w*h*4),coverage(w*h*4),packed(packed_row*h,0xed),unpacked(w*h*4),reflected(reflection_row*h,0xa7);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        unsigned at=(y*w+x)*4;
        for(unsigned c=0;c<4;++c) color[at+c]=static_cast<unsigned char>(17+(x*23+y*31+c*59)%229);
        coverage[at]=x%3?255:0;coverage[at+1]=static_cast<unsigned char>(x*29+y*7);
        packed[y*packed_row+x]=static_cast<unsigned char>((x*37+y*51)%256);
        word(unpacked,at,packed[y*packed_row+x]);
        word(reflected,y*reflection_row+x*4,(x%4?0xff000000U:0x7f000000U)|((x*59+y*73)&255U)|(((x*41+y*29)&255U)<<8)|(((x*13+y*97)&255U)<<16));
    }
    gpu.begin();source.upload(color);receiver.upload(coverage);
    Buffer packed_buffer(gpu,packed),unpacked_buffer(gpu,unpacked),reflection_buffer(gpu,reflected);gpu.submit();
    const render::shadows::GpuShadowOutput packed_output{gpu.device,packed_buffer.buffer,w,h,packed_row},uint_output{gpu.device,unpacked_buffer.buffer,w,h,0};
    const render::shadows::GpuReflectionOutput reflection{gpu.device,reflection_buffer.buffer,w,h,reflection_row};
    for(unsigned mode=0;mode<6;++mode) {
        const auto* shadow=mode==1 || mode==3?&packed_output:mode==2 || mode==4?&uint_output:nullptr;
        const auto* reflection_input=mode>=3?&reflection:nullptr;
        gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,shadow,reflection_input,.6F,.8F),compositor.status().c_str());gpu.submit();
        const auto expected=oracle(color,coverage,w,h,shadow?(shadow->packed_row_bytes?std::span(packed):std::span(unpacked)):std::span<const unsigned char>{},
            shadow?shadow->packed_row_bytes:0,reflection_input?std::span(reflected):std::span<const unsigned char>{},reflection_row,format,.6F,.8F);
        compare(target.read(),expected,"Strided composite differs from independent colour oracle",mode?1:0);
    }
    unsigned finishes=0;
    for(auto material:render::effect_order) {
        if(!render::decorative_material(material)) continue;
        ++finishes;
        for(unsigned step:{1U,3U}) for(unsigned mode:{0U,1U}) {
            const auto* shadow=mode?&packed_output:nullptr;
            const auto* reflected_input=mode?&reflection:nullptr;
            auto expected=oracle(color,coverage,w,h,mode?std::span(packed):std::span<const unsigned char>{},
                packed_row,mode?std::span(reflected):std::span<const unsigned char>{},reflection_row,format,.6F,.8F);
            render::Framebuffer ownership(w/step,h/step,step);ownership.enable_layer_tags(true);
            for(unsigned pixel=0;pixel<w*h;++pixel)
                ownership.layer_tags()[pixel]=unsigned(coverage[pixel*4]?render::PixelLayer::three_d:render::PixelLayer::two_d);
            Bytes scratch;render::apply_effect(material,ownership,expected,scratch);
            gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
                shadow,reflected_input,.6F,.8F,unsigned(material),step),compositor.status().c_str());gpu.submit();
            const auto message=std::string("Native decorative finish ")+std::string(render::effect_names[unsigned(material)])
                +" mode="+std::to_string(mode)+" step="+std::to_string(step)+" differs from flat surface-finish oracle";
            // The negotiated sRGB target performs hardware conversion, whose
            // rounding is permitted one code just like the composition oracle
            // above. Linear targets and alpha must remain byte-exact.
            compare(target.read(),expected,message.c_str(),srgb(format)?1:0);
        }
    }
    check(finishes==17,"Decorative native fixture omitted a material");
    std::cout<<"  17 native surface finishes, composed-ray/no-ray inputs, step 1/3, flat colour/edge oracle (linear exact, sRGB <=1 code), exact alpha passed"<<std::endl;
    // Mixed ownership, including covered protected pixels and world fragments
    // with NO ray receiver. Same-colour neighbours have different ownership.
    for(unsigned pixel=0;pixel<w*h;++pixel) {
        const unsigned layer=pixel%3;coverage[pixel*4+2]=layer;
        if(layer==1) coverage[pixel*4]=0;
        if(layer==0 && pixel%4==0) coverage[pixel*4]=255;
    }
    gpu.begin();receiver.upload(coverage);gpu.submit();
    // Check ray blending independently before the authored 8-bit effect
    // oracle. The strided tests above cover halfway ray blends with their
    // one-code tolerance. Avoid exact half ties here, so a neighbourhood
    // threshold is not ambiguously tipped by UNORM hardware conversion.
    constexpr float style_shadow=.61F,style_reflection=.73F;
    std::array<Bytes,2> composed;
    for(unsigned mode:{0U,1U}) {
        gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
            mode?&packed_output:nullptr,mode?&reflection:nullptr,style_shadow,style_reflection),compositor.status().c_str());gpu.submit();
        composed[mode]=target.read();
        const auto expected=oracle(color,coverage,w,h,mode?std::span(packed):std::span<const unsigned char>{},
            packed_row,mode?std::span(reflected):std::span<const unsigned char>{},reflection_row,format,style_shadow,style_reflection);
        compare(composed[mode],expected,"Mixed ownership ray blending differs from independent oracle",mode?1:0);
    }
    unsigned styles=0;
    for(unsigned effect=0;effect<render::effect_count;++effect) {
        if(!render::calibrated_composite_effect(effect)) continue;
        ++styles;
        for(unsigned step:{1U,3U}) for(unsigned mode:{0U,1U}) for(unsigned strength:{0U,35U,100U})
            for(unsigned selection:{0U,1U,2U,3U}) {
            const unsigned world=selection==1?0:effect;
            const unsigned model=selection==2?0:selection==3?unsigned(render::Effect::emboss):effect;
            const auto* shadow=mode?&packed_output:nullptr;const auto* ray=mode?&reflection:nullptr;
            auto expected=composed[mode];
            render::Framebuffer ownership(w/step,h/step,step);ownership.enable_layer_tags(true);
            for(unsigned pixel=0;pixel<w*h;++pixel) {
                auto kind=coverage[pixel*4+2];
                ownership.layer_tags()[pixel]=unsigned(kind==1?render::PixelLayer::background:kind==2?render::PixelLayer::three_d:render::PixelLayer::two_d);
            }
            Bytes scratch;render::apply_effect(static_cast<render::Effect>(model),ownership,expected,scratch,
                strength,static_cast<render::Effect>(world),strength);
            gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,shadow,ray,style_shadow,style_reflection,0,step,
                {world,model,strength,strength}),compositor.status().c_str());gpu.submit();
            const auto message=std::string("Native neighbourhood ")+std::string(render::effect_names[effect])+" mode="+std::to_string(mode)
                +" step="+std::to_string(step)+" strength="+std::to_string(strength)+" selection="+std::to_string(selection)+" differs from flat oracle";
            compare(target.read(),expected,message.c_str(),srgb(format) || effect>=unsigned(render::Effect::energy_shield)?1:0);
        }
    }
    check(styles==67,"Native style/warp/animated fixture omitted a selection");
    std::cout<<"  67 native style/warp/animated selections, independent world/model choices, protected and non-ray coverage, 0/35/100 intensity, steps 1/3 and composed rays passed"<<std::endl;
    for(unsigned step:{1U,3U}) for(unsigned mode:{0U,1U}) {
        const render::CalibratedPostEffects post{unsigned(render::Effect::watercolour),unsigned(render::Effect::cel_drawn),35,100};
        const auto* shadow=mode?&packed_output:nullptr;const auto* ray=mode?&reflection:nullptr;
        for(auto finish:render::effect_order) if(render::decorative_material(finish)) {
            gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,shadow,ray,
                style_shadow,style_reflection,unsigned(finish),step),compositor.status().c_str());gpu.submit();
            auto expected=target.read();Bytes scratch;
            render::Framebuffer ownership(w/step,h/step,step);ownership.enable_layer_tags(true);
            for(unsigned pixel=0;pixel<w*h;++pixel) {
                const auto kind=coverage[pixel*4+2];
                ownership.layer_tags()[pixel]=unsigned(kind==1?render::PixelLayer::background:kind==2?render::PixelLayer::three_d:render::PixelLayer::two_d);
            }
            render::apply_effect(static_cast<render::Effect>(post.model),ownership,expected,scratch,post.model_intensity,
                static_cast<render::Effect>(post.world),post.world_intensity);
            gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,shadow,ray,
                style_shadow,style_reflection,unsigned(finish),step,post),compositor.status().c_str());gpu.submit();
            compare(target.read(),expected,"Decorative material did not precede independent model/world styles",srgb(format)?1:0);
        }
    }
    std::cout<<"  All 17 decorative finishes precede independent world/model styles; alpha and finished neighbours passed"<<std::endl;
    gpu.begin();
    for(unsigned fault=0;fault<8;++fault) {
        auto s=packed_output;auto r=reflection;
        if(fault==0) s.width--;
        if(fault==1) s.device=nullptr;
        if(fault==2) s.packed_row_bytes=w;
        if(fault==3) r.height++;
        if(fault==4) r.row_bytes=w*4-4;
        if(fault==5) r.buffer=nullptr;
        check(!compositor.enqueue(gpu.command,source.texture,receiver.texture,fault==6?source.texture:target.texture,w,h,&s,&r,
            fault==7?NAN:1,1),"Malformed ray composite inputs accepted");
    }
    for(unsigned fault=0;fault<3;++fault)
        check(!compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,1,1,
            unsigned(fault==0?render::Effect::mirror:fault==1?render::Effect::cel_drawn:render::Effect::glass),fault==2?0:1),
            "Invalid decorative material/scale was encoded");
    check(!compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,1,1,0,1,{1,0,101,0})
        && !compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,1,1,0,1,{unsigned(render::Effect::trails),0,100,100}),
        "Invalid native post style/intensity accepted");
    check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h),compositor.status().c_str());gpu.submit();
    compare(target.read(),color,"Validation failure retained stale ray composition",0);
}
void warp_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    for(unsigned step:{1U,3U}) {
        const unsigned w=65*step,h=97*step;
        Texture source(gpu,w,h,format),receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
        render::GpuCalibratedRayComposite compositor;check(compositor.initialize(gpu.device,format),compositor.status().c_str());
        render::Framebuffer ownership(65,97,step);ownership.enable_layer_tags(true);
        Bytes color(w*h*4),coverage(w*h*4);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned at=(y*w+x)*4,nx=x/step,ny=y/step;
            for(unsigned c=0;c<3;++c) color[at+c]=static_cast<unsigned char>((nx*23+ny*41+c*67+x%step*11)%256);
            // Stable-sort ties with different RGB: the integer luminance is
            // identical, but reordering equal keys would change red pixels.
            if(nx>=32 && nx<48 && ny>=8 && ny<22) {
                color[at]=static_cast<unsigned char>(100+nx%3);color[at+1]=color[at+2]=100;
            }
            color[at+3]=static_cast<unsigned char>(11+(x*17+y*31)%245);
            const unsigned kind=ny<3 || (nx>=29 && nx<=31)?0:nx<24?1:2;
            coverage[at+2]=kind;coverage[at]=kind && nx%4?255:0;coverage[at+1]=255;
            ownership.layer_tags()[y*w+x]=unsigned(kind==1?render::PixelLayer::background:kind==2?render::PixelLayer::three_d:render::PixelLayer::two_d);
        }
        gpu.begin();source.upload(color);receiver.upload(coverage);gpu.submit();unsigned effects=0;
        for(unsigned effect=0;effect<render::effect_count;++effect) {
            if(!render::calibrated_composite_effect(effect) || render::calibrated_post_effect(effect) || render::calibrated_palette_effect(effect)) continue;
            ++effects;const bool animated=effect>=unsigned(render::Effect::energy_shield);
            Bytes first,last;
            for(float time:{0.F,.375F,1.25F}) {
                if(!animated && time!=0) continue;
                for(unsigned strength:{0U,35U,100U}) for(unsigned selection:{0U,1U,2U}) {
                    const unsigned world=selection==1?0:effect,model=selection==2?0:effect;
                    auto expected=color;Bytes scratch;
                    render::apply_effect(static_cast<render::Effect>(model),ownership,expected,scratch,strength,static_cast<render::Effect>(world),strength,time);
                    gpu.begin();check(compositor.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,
                        1,1,0,step,{world,model,strength,strength,time}),compositor.status().c_str());gpu.submit();auto actual=target.read();
                    const auto detail="Native warp "+std::string(render::effect_names[effect])+" time="+std::to_string(time)
                        +" step="+std::to_string(step)+" intensity="+std::to_string(strength)+" selection="+std::to_string(selection);
                    compare(actual,expected,detail.c_str(),animated || srgb(format)?1:0);
                    for(unsigned at=0;at<actual.size();at+=4) if(!coverage[at+2])
                        check(std::equal(color.begin()+at,color.begin()+at+4,actual.begin()+at),"Warp sampled over protected native ink");
                    if(strength==100 && selection==0) {
                        check(actual!=color,"Spatial/animated native selection was bypassed");
                        if(time==0) first=actual;if(time==1.25F) last=actual;
                    }
                }
            }
            if(animated) check(first!=last,("Animated native selection "+std::string(render::effect_names[effect])+" did not advance at a new presentation time").c_str());
        }
        check(effects==20,"Native coordinate/animated fixture omitted an effect");
        const std::array<render::CalibratedPostEffects,3> sequence{{
            {unsigned(render::Effect::sepia),unsigned(render::Effect::posterized),35,100,.375F},
            {unsigned(render::Effect::barrel_warp),unsigned(render::Effect::pixel_sort),100,35,.375F},
            {unsigned(render::Effect::energy_shield),unsigned(render::Effect::arc_lightning),100,100,.375F}}};
        auto expected=color;Bytes scratch;
        for(const auto& pass:sequence) render::apply_effect(static_cast<render::Effect>(pass.model),ownership,expected,scratch,
            pass.model_intensity,static_cast<render::Effect>(pass.world),pass.world_intensity,pass.seconds);
        gpu.begin();check(compositor.enqueue_sequence(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,
            1,1,0,step,sequence),compositor.status().c_str());gpu.submit();const auto held=target.read();
        compare(held,expected,"Primary/manipulation/special-FX sequence changed the flat effect order",1);
        gpu.begin();check(compositor.enqueue_sequence(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,
            1,1,0,step,sequence),compositor.status().c_str());gpu.submit();
        check(target.read()==held,"Same native animation time jittered instead of holding a stable frame");
        gpu.begin();auto invalid=sequence;invalid[2].seconds=NAN;
        check(!compositor.enqueue_sequence(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,
            1,1,0,step,invalid),"Malformed late sequence pass was accepted");
        check(compositor.enqueue_sequence(gpu.command,source.texture,receiver.texture,target.texture,w,h,nullptr,nullptr,
            1,1,0,step,{}),compositor.status().c_str());gpu.submit();compare(target.read(),color,"Sequence OFF failed exact native restoration",0);
    }
    std::cout<<"  12 static warps + 8 animated FX: flat CPU oracle, 65x97 source grid, 1x/3x, sort ties, ownership/edges, 0/35/100 intensity, three times, ordered resident passes, paused stability and exact OFF passed"<<std::endl;
}
void global_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    render::GpuCalibratedGlobal native;check(native.initialize(gpu.device,format),native.status().c_str());
    for(unsigned step:{1U,3U}) {
        const unsigned w=45*step,h=33*step;
        Texture source(gpu,w,h,format),receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
        Bytes color(w*h*4),coverage(w*h*4);render::Framebuffer ownership(45,33,step);ownership.enable_layer_tags(true);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned pixel=y*w+x,at=pixel*4;
            const unsigned layer=(y<2*step || (x/step+y/step)%13==0)?0:1+(x/step)%2;
            coverage[at+2]=layer;
            ownership.layer_tags()[pixel]=unsigned(layer==0?render::PixelLayer::two_d:layer==1?render::PixelLayer::background:render::PixelLayer::three_d);
            for(unsigned c=0;c<3;++c) color[at+c]=static_cast<unsigned char>((x*29+y*17+c*83)%256);
            color[at+3]=static_cast<unsigned char>((x*13+y*19)%256);
        }
        gpu.begin();source.upload(color);receiver.upload(coverage);gpu.submit();
        for(unsigned choice=0;choice<=render::global_enhancement_count+1;++choice) for(unsigned quality:{1U,2U,3U})
            for(float seconds:{0.F,.317F,1.73F}) {
            const unsigned packed=choice<render::global_enhancement_count?quality<<(choice*2)
                :choice==render::global_enhancement_count?quality*0x01555555U:render::global_enhancement_mask;
            auto expected=color;Bytes scratch;render::apply_global_enhancements(packed,ownership,expected,scratch,seconds);
            gpu.begin();check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,step,packed,seconds),native.status().c_str());gpu.submit();
            const auto actual=target.read();
            const auto message="Native global enhancement "+std::to_string(choice)+" quality="+std::to_string(quality)
                +" step="+std::to_string(step)+" time="+std::to_string(seconds)+" differs from shared flat oracle";
            compare(actual,expected,message.c_str(),1); // Float/trig and hardware sRGB conversion permit one RGB code.
            for(unsigned at=0;at<actual.size();at+=4) if(!coverage[at+2])
                check(std::equal(color.begin()+at,color.begin()+at+4,actual.begin()+at),"Native global enhancements changed protected UI/emissive ink");
        }
        gpu.begin();
        for(unsigned fault=0;fault<5;++fault) check(!native.enqueue(gpu.command,source.texture,receiver.texture,
            fault==4?source.texture:target.texture,w,h,fault==1?0:step,fault==0?1U<<26:1U,
            fault==2?NAN:fault==3?-1.F:0.F),"Malformed native global settings accepted");
        check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,step,0,0),native.status().c_str());gpu.submit();
        compare(target.read(),color,"Global enhancements OFF failed exact restoration",0);
        gpu.begin();check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,step,render::global_enhancement_mask,0),native.status().c_str());
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        gpu.begin();check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,step,0,0),native.status().c_str());gpu.submit();
        compare(target.read(),color,"Cancelled global pass contaminated a later frame",0);
    }
    std::cout<<"  All 13 global enhancements: shared flat oracle, LOW/MED/HIGH and combined selections, 45x33 grid, 1x/3x, three times, protected samples/ink/alpha, malformed/cancelled inputs and exact OFF passed"<<std::endl;
}
void appearance_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    render::GpuCalibratedGlobal native;check(native.initialize(gpu.device,format),native.status().c_str());
    unsigned compared=0,changed=0;
    for(const auto dimensions:{std::array<unsigned,3>{45,33,1},{45,33,3},{1,1,1},{47,31,2}}) {
        const auto nw=dimensions[0],nh=dimensions[1],scale=dimensions[2],w=nw*scale,h=nh*scale;
        Texture source(gpu,w,h,format),receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format),second(gpu,w,h,format);
        Bytes color(w*h*4),coverage(w*h*4);render::Framebuffer ownership(nw,nh,scale);ownership.enable_layer_tags(true);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned pixel=y*w+x,at=pixel*4;
            const unsigned layer=w==1?2:(y<2*scale || (x/scale+y/scale)%13==0)?0:1+(x/scale)%2;
            coverage[at+2]=layer;
            ownership.layer_tags()[pixel]=unsigned(layer==0?render::PixelLayer::two_d:layer==1?render::PixelLayer::background:render::PixelLayer::three_d);
            for(unsigned c=0;c<3;++c) color[at+c]=static_cast<unsigned char>((x*29+y*17+c*83)%256);
            color[at+3]=static_cast<unsigned char>((x*13+y*19+43)%256);
        }
        const auto oracle=[&](unsigned contrast,unsigned chromatic) {
            auto expected=color;Bytes scratch;
            render::apply_hdr_effect(ownership,expected,static_cast<unsigned char>(contrast));
            render::apply_chromatic_aberration(ownership,expected,scratch,static_cast<unsigned char>(chromatic));
            return expected;
        };
        gpu.begin();source.upload(color);receiver.upload(coverage);gpu.submit();
        for(unsigned contrast=0;contrast<4;++contrast) for(unsigned chromatic=0;chromatic<4;++chromatic) {
            gpu.begin();check(native.enqueue_appearance(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,contrast,chromatic),native.status().c_str());gpu.submit();
            const auto actual=target.read(),expected=oracle(contrast,chromatic);
            const auto message="Native appearance contrast="+std::to_string(contrast)+" chromatic="+std::to_string(chromatic)+" scale="+std::to_string(scale);
            compare(actual,expected,message.c_str(),contrast || chromatic?1:0);
            for(unsigned at=0;at<actual.size();at+=4) {
                if(!coverage[at+2]) check(std::equal(color.begin()+at,color.begin()+at+4,actual.begin()+at),"Native appearance changed protected UI/emissive ink");
                if(chromatic && !contrast && coverage[at+2]!=2)
                    check(std::equal(color.begin()+at,color.begin()+at+4,actual.begin()+at),"Model chromatic separation altered native world art");
                check(actual[at+3]==color[at+3],"Native appearance changed source alpha");
                changed+=actual[at]!=color[at];++compared;
            }
        }
        gpu.begin();
        for(unsigned fault=0;fault<8;++fault) check(!native.enqueue_appearance(gpu.command,source.texture,fault==7?source.texture:receiver.texture,
            fault==4?source.texture:fault==5?receiver.texture:target.texture,fault==0?0:w,h,fault==1?0:fault==6?32769:scale,
            fault==2?4:1,fault==3?4:0),"Malformed native appearance inputs accepted");
        // Encoding a pass does not own its source or commit any history.
        check(native.enqueue_appearance(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,3,3),native.status().c_str());
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        gpu.begin();check(native.enqueue_appearance(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,0,0),native.status().c_str());gpu.submit();
        compare(target.read(),color,"Cancelled appearance pass contaminated exact OFF restoration",0);
        gpu.begin();
        check(native.enqueue_appearance(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,1,3),native.status().c_str());
        check(native.enqueue_appearance(gpu.command,source.texture,receiver.texture,second.texture,w,h,scale,3,0),native.status().c_str());gpu.submit();
        compare(target.read(),oracle(1,3),"Second appearance eye overwrote the first eye",1);
        compare(second.read(),oracle(3,0),"Second appearance eye inherited the first eye's controls",1);
    }
    check(changed>100,"Native appearance fixtures produced no visible effect");
    std::cout<<"  Native appearance: "<<compared<<" flat CPU-oracle pixels, all 16 contrast/chromatic quality pairs, byte-rounded order, protected neighbourhood/world/UI/alpha, 1x/2x/3x and single-pixel/odd extents, independent eyes, cancellation, invalid inputs and exact OFF passed"<<std::endl;
}
void bloom_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    render::GpuCalibratedBloom native;check(native.initialize(gpu.device,format),native.status().c_str());
    unsigned comparisons=0,changed=0;
    for(const auto dimensions:{std::array<unsigned,3>{45,33,1},{45,33,3},{1,1,1},{47,31,2}}) {
        const auto nw=dimensions[0],nh=dimensions[1],scale=dimensions[2],w=nw*scale,h=nh*scale;
        Texture source(gpu,w,h,format),receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format),second(gpu,w,h,format);
        Bytes color(w*h*4),coverage(w*h*4);render::Framebuffer ownership(nw,nh,scale);ownership.enable_layer_tags(true);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned pixel=y*w+x,at=pixel*4;
            const unsigned layer=w==1?2:(y<2*scale || (x/scale+y/scale)%13==0)?0:1+(x/scale)%2;
            coverage[at+2]=layer;
            ownership.layer_tags()[pixel]=unsigned(layer==0?render::PixelLayer::two_d:layer==1?render::PixelLayer::background:render::PixelLayer::three_d);
            for(unsigned c=0;c<3;++c) color[at+c]=static_cast<unsigned char>((x*29+y*17+c*83+217)%256);
            color[at+3]=static_cast<unsigned char>((x*13+y*19+43)%256);
        }
        gpu.begin();source.upload(color);receiver.upload(coverage);gpu.submit();
        for(unsigned model=0;model<4;++model) for(unsigned world=0;world<4;++world) {
            auto expected=color;render::BloomPass oracle;oracle.apply(model,world,ownership,expected);
            gpu.begin();check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,model,world),native.status().c_str());gpu.submit();
            const auto actual=target.read();
            const auto message="Native bloom model="+std::to_string(model)+" world="+std::to_string(world)+" scale="+std::to_string(scale);
            compare(actual,expected,message.c_str(),model || world?1:0);
            for(unsigned at=0;at<actual.size();at+=4) {
                if(!coverage[at+2]) check(std::equal(color.begin()+at,color.begin()+at+4,actual.begin()+at),"Native bloom changed protected UI/emissive colour or opacity");
                check(actual[at+3]==color[at+3],"Native bloom changed source alpha");
                changed+=actual[at]!=color[at];++comparisons;
            }
        }
        gpu.begin();
        for(unsigned fault=0;fault<7;++fault) check(!native.enqueue(gpu.command,source.texture,receiver.texture,
            fault==4?source.texture:fault==5?receiver.texture:target.texture,fault==0?0:w,h,fault==1?0:fault==6?32769:scale,
            fault==2?4:1,fault==3?4:0),"Malformed native bloom settings accepted");
        check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,3,1),native.status().c_str());
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        gpu.begin();check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,0,0),native.status().c_str());gpu.submit();
        compare(target.read(),color,"Cancelled bloom pass contaminated later OFF image",0);
        // Both eyes may encode in one command. Shared temporary halos must
        // never overwrite the first eye's completed colour destination.
        gpu.begin();
        check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,scale,1,3),native.status().c_str());
        check(native.enqueue(gpu.command,source.texture,receiver.texture,second.texture,w,h,scale,3,0),native.status().c_str());gpu.submit();
        auto firstExpected=color,secondExpected=color;render::BloomPass oracle;
        oracle.apply(1,3,ownership,firstExpected);oracle.apply(3,0,ownership,secondExpected);
        compare(target.read(),firstExpected,"Second bloom eye overwrote the first eye",1);
        compare(second.read(),secondExpected,"Second bloom eye inherited the first eye's source strengths",1);
    }
    check(changed>100,"Native bloom fixtures produced no visible enhancement");
    std::cout<<"  Native 2D/3D bloom: "<<comparisons<<" flat CPU-oracle pixels; all 16 quality pairs, half-native tight/broad halos, 1x/2x/3x, partial/single-pixel extents, shared-command independent eyes, protected ink/alpha, resize, cancellation/invalid inputs and exact OFF passed"<<std::endl;
}
void persistence_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    for(unsigned step:{1U,3U}) for(unsigned mode:{1U,2U,3U}) for(unsigned scope:{0U,1U,2U}) for(unsigned hz:{60U,240U})
        for(unsigned quality:{1U,2U,3U}) {
        if(mode!=3 && quality!=2) continue;
        const unsigned w=12*step,h=8*step;
        Texture source(gpu,w,h,format),receiver(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
        render::GpuCalibratedPersistence native;
        check(!native.working_image_bytes(),"Unused persistence owner invented retained float images");
        check(native.initialize(gpu.device,format),native.status().c_str());render::FramePersistence oracle;
        check(!native.working_image_bytes(),"Persistence initialization allocated an unselected history extent");
        render::Framebuffer ownership(12,8,step);ownership.enable_layer_tags(true);
        bool observed=false;
        for(unsigned frame=0;frame<15;++frame) {
            Bytes color(w*h*4),coverage(w*h*4);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned at=(y*w+x)*4,nx=x/step,ny=y/step;
                unsigned layer=1;
                if(ny==0 || (frame==3 && nx==2 && ny==3)) layer=0;
                else if(frame<3 && nx==2+frame && ny==3) layer=2;
                color[at]=layer==2?245:static_cast<unsigned char>(10+(nx+ny+frame)%20);
                color[at+1]=layer==2?155:static_cast<unsigned char>(15+(nx*3+ny)%20);
                color[at+2]=layer==2?85:static_cast<unsigned char>(20+(nx+ny*3)%20);
                color[at+3]=static_cast<unsigned char>(20+(x*11+y*17)%235);
                if(layer==0) {color[at]=210;color[at+1]=225;color[at+2]=250;}
                coverage[at+2]=layer;
                ownership.layer_tags()[y*w+x]=unsigned(layer==0?render::PixelLayer::two_d:layer==1?render::PixelLayer::background:render::PixelLayer::three_d);
            }
            const unsigned selected_mode=mode==3?3:frame>=5?3-mode:mode,epoch=frame>=8?2:1,intensity=frame>=6?35:100;
            const unsigned selected_quality=frame>=6?4-quality:quality;
            double time=double(frame)/hz;
            if(frame==4) time=double(3)/hz; // Paused phase is identical.
            if(frame==9) time+=2; // Long gap must reset.
            if(frame==10) time=.01; // Rebased clock must reset.
            if(frame==12) {
                gpu.begin();source.upload(color);receiver.upload(coverage);
                check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
                    {selected_mode,intensity,step,scope!=1,scope!=0,time,epoch,selected_quality}),native.status().c_str());
                check(!native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
                    {selected_mode,intensity,step,scope!=1,scope!=0,time,epoch,selected_quality}),"Pending native history was overwritten");
                SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;native.discard();oracle.reset();
            }
            auto expected=color;oracle.apply(ownership,expected,static_cast<render::PersistenceMode>(selected_mode),scope!=1,scope!=0,time,epoch,intensity,selected_quality);
            gpu.begin();source.upload(color);receiver.upload(coverage);
            check(native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,
                {selected_mode,intensity,step,scope!=1,scope!=0,time,epoch,selected_quality}),native.status().c_str());gpu.submit();native.commit();
            auto actual=target.read();
            check(native.working_image_bytes()==std::uint64_t(w)*h*32,
                "Persistence memory accounting omitted or duplicated its two float histories");
            compare(actual,expected,"Native temporal history differs from the independent time-based float RGB oracle",srgb(format)?1:0);
            observed=observed || actual!=color;
            for(unsigned at=0;at<actual.size();at+=4) if(!coverage[at+2]) check(std::equal(color.begin()+at,color.begin()+at+4,actual.begin()+at),
                "Native persistence captured or changed protected UI ink");
        }
        check(observed,"Native temporal selection did not retain any history");
        // New eye extent/scale cannot inherit the previous eye's cached pair.
        Texture resized(gpu,w+3,h,format),resized_mask(gpu,w+3,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),resized_output(gpu,w+3,h,format);
        Bytes fresh((w+3)*h*4,13),mask(fresh.size(),0);for(unsigned at=0;at<mask.size();at+=4) mask[at+2]=1;
        gpu.begin();resized.upload(fresh);resized_mask.upload(mask);
        check(native.enqueue(gpu.command,resized.texture,resized_mask.texture,resized_output.texture,w+3,h,
            {mode,100,step+1,true,false,.4,2,quality}),native.status().c_str());gpu.submit();native.commit();
        compare(resized_output.read(),fresh,"Resized native eye inherited stale temporal history",0);
        check(native.working_image_bytes()==std::uint64_t(2*w+3)*h*32,
            "Persistence accounting lost the retained previous extent during resize");
        gpu.begin();auto invalid=render::CalibratedPersistenceSettings{mode,100,step,true,false,NAN,2};
        check(!native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,invalid),"Non-finite native temporal time accepted");
        invalid.seconds=.5;invalid.mode=0;
        check(!native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,invalid),"OFF native temporal mode allocated/encoded history");
        invalid.mode=3;invalid.quality=0;
        check(!native.enqueue(gpu.command,source.texture,receiver.texture,target.texture,w,h,invalid),"Invalid phosphor quality accepted");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        native.release_device();check(!native.working_image_bytes(),"Released persistence images remained charged");
    }
    std::cout<<"  Native trails/long exposure/CRT phosphor: time-based float CPU oracle, models/world/both, 60/240 Hz, all phosphor qualities/RGB decay, protected ink/alpha, pause/gap/backward time, mode/quality/epoch/scale/extent resets and transactional cancellation passed"<<std::endl;
}
std::vector<vr::SceneVertex> quad(float x,float y,float z,float radius,std::array<float,4> color) {
    std::vector<vr::SceneVertex> result(6);constexpr int corners[][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
    for(unsigned i=0;i<6;++i) {auto& v=result[i];v.position[0]=x+corners[i][0]*radius;v.position[1]=y+corners[i][1]*radius;v.position[2]=z;
        std::copy(color.begin(),color.end(),v.color);std::copy(color.begin(),color.end(),v.odd_color);}
    return result;
}
vr::EyeCamera camera(unsigned eye) {
    XrView view{XR_TYPE_VIEW};view.pose.orientation={0,0,0,1};view.pose.position={eye?.038F:-.027F,.01F,.02F};
    view.fov={-.56F+eye*.03F,.72F,.6F,-.51F};auto result=vr::eye_camera(view,1,.05F,20.F);check(bool(result),"Bad calibrated fixture camera");
    for(unsigned column=0;column<4;++column) result->projection[column*4+1]*=-1;return *result;
}
#include "check_calibrated_exposure.inc"
#include "check_calibrated_aa.inc"
#include "check_calibrated_temporal_aa.inc"
#include "check_calibrated_temporal_patterns.inc"
#include "check_calibrated_temporal_edges.inc"
#include "check_native_water_layers.inc"
#include "check_native_curved_cache.inc"
void phase_tests(Gpu& gpu,SDL_GPUTextureFormat format,bool hardware) {
    constexpr unsigned w=256,h=192;
    Texture source0(gpu,w,h,format),source1(gpu,w,h,format),receiver0(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        receiver1(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target0(gpu,w,h,format),target1(gpu,w,h,format),
        depth0(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT),depth1(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
    std::array<Texture*,2> sources{&source0,&source1},receivers{&receiver0,&receiver1},targets{&target0,&target1},depths{&depth0,&depth1};
    render::GpuCalibratedScene scene;render::GpuCalibratedRayComposite compositor;
    check(scene.initialize(gpu.device,format),scene.status().c_str());check(compositor.initialize(gpu.device,format),compositor.status().c_str());
    auto early=quad(-.1F,-.1F,-.9F,.32F,{0,.6F,0,1}),surface=quad(0,0,-1,.46F,{.3F,.5F,.6F,1}),
        light=quad(-.06F,.03F,-.65F,.055F,{1,0,0,1}),cutout=quad(.27F,0,-.8F,.08F,{1,1,1,1}),
        ui=quad(.13F,.14F,-.5F,.07F,{1,1,0,1}),hidden=quad(0,-.16F,-2,.05F,{1,0,1,1});
    const std::array<unsigned,4> texels{0xff0044ffU,0,0,0xff0044ffU};
    for(unsigned i=0;i<cutout.size();++i) {auto& v=cutout[i];v.texture[1]=v.texture[2]=1;v.texture[3]=1;
        v.uv[0]=(i==1 || i==2 || i==4)?1.5F:.5F;v.uv[1]=(i==2 || i==4 || i==5)?1.5F:.5F;}
    std::array<render::CalibratedSceneDraw,6> draws{{{early},{surface},{light},{cutout,texels},{ui},{hidden}}};
    draws[0].depth_test=false;draws[1].ray_caster=draws[3].ray_caster=true;
    draws[4].depth_test=false;draws[4].after_rays=draws[5].after_rays=true;
    draws[0].effect_layer=1;draws[1].effect_layer=draws[3].effect_layer=2;
    std::array<Bytes,2> reference,base,coverage;std::array<render::CalibratedRayGeometryOutput,2> rays;
    gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());
    for(unsigned e=0;e<2;++e) check(scene.enqueue_eye(gpu.command,targets[e]->texture,depths[e]->texture,w,h,camera(e),{.03F,.06F,.09F,.7F}),scene.status().c_str());
    gpu.submit();for(unsigned e=0;e<2;++e) reference[e]=targets[e]->read();
    gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
    for(unsigned e=0;e<2;++e) {
        check(scene.enqueue_eye(gpu.command,sources[e]->texture,depths[e]->texture,w,h,camera(e),{.03F,.06F,.09F,.7F},
            render::CalibratedScenePhase::before_rays,receivers[e]->texture),scene.status().c_str());
        rays[e]=scene.enqueue_ray_geometry(gpu.command,e,w,h,camera(e));check(rays[e].complete,scene.status().c_str());
    }
    gpu.submit();check(scene.notify_submitted(token),"Composite producer token not acknowledged");
    for(unsigned e=0;e<2;++e) {base[e]=sources[e]->read();coverage[e]=receivers[e]->read();}
    check(base[0]!=base[1] && coverage[0]!=coverage[1],"Composite fixture lost independent calibrated eyes");
    for(unsigned e=0;e<2;++e) {
        unsigned selected{},excluded_light{},holes{};
        for(unsigned at=0;at<base[e].size();at+=4) {
            selected+=coverage[e][at]!=0;
            if(base[e][at]==255 && base[e][at+1]==0 && base[e][at+2]==0) {
                check(coverage[e][at]==0 && coverage[e][at+2]==0,"Emissive foreground inherited ray/effect coverage");++excluded_light;
            }
            if(base[e][at]==0 && base[e][at+1]>90 && base[e][at+2]==0) {
                check(coverage[e][at]==0 && coverage[e][at+2]==1,"World foreground lost effect-only coverage");++holes;
            }
            if(coverage[e][at]) check(coverage[e][at+2]==2,"Native receiver lost independent model effect coverage");
        }
        check(selected>1000 && excluded_light>50 && holes>50,"Receiver coverage fixture failed to exercise selected/excluded fragments");
    }
    // The second command may load original depth only for a submitted token.
    gpu.begin();check(!scene.enqueue_eye(gpu.command,target0.texture,depth0.texture,w,h,camera(0),{},render::CalibratedScenePhase::after_rays,nullptr,token+1),
        "Stale continuation token accepted");
    for(unsigned e=0;e<2;++e) {
        check(compositor.enqueue(gpu.command,sources[e]->texture,receivers[e]->texture,targets[e]->texture,w,h),compositor.status().c_str());
        check(scene.enqueue_eye(gpu.command,targets[e]->texture,depths[e]->texture,w,h,camera(e),{},render::CalibratedScenePhase::after_rays,nullptr,token),scene.status().c_str());
    }
    gpu.submit();for(unsigned e=0;e<2;++e) compare(targets[e]->read(),reference[e],"Split native source order/depth/alpha changed",0);
    // Selecting a deferred neighbourhood style must not tint the base raster
    // with the shared scene shader's earlier approximation before composition.
    for(unsigned effect=0;effect<render::effect_count;++effect) if(render::calibrated_composite_effect(effect)) {
        auto selected=draws;
        for(auto& draw:selected) draw.effects_override=std::array<unsigned,4>{effect,100,0,0};
        gpu.begin();check(scene.upload(gpu.command,selected),scene.status().c_str());
        const auto style_token=scene.upload_token();
        for(unsigned e=0;e<2;++e) check(scene.enqueue_eye(gpu.command,sources[e]->texture,depths[e]->texture,w,h,camera(e),{.03F,.06F,.09F,.7F},
            render::CalibratedScenePhase::before_rays,receivers[e]->texture),scene.status().c_str());
        gpu.submit();check(scene.notify_submitted(style_token),"Deferred style lost source ownership");
        for(unsigned e=0;e<2;++e) {
            compare(sources[e]->read(),base[e],"Deferred style also tinted the source raster",0);
            compare(receivers[e]->read(),coverage[e],"Deferred style changed source ray/effect coverage",0);
        }
    }
    // Restore the submitted raw source/token used by the actual native RT test.
    gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());
    const auto ray_token=scene.upload_token();
    for(unsigned e=0;e<2;++e) {
        check(scene.enqueue_eye(gpu.command,sources[e]->texture,depths[e]->texture,w,h,camera(e),{.03F,.06F,.09F,.7F},
            render::CalibratedScenePhase::before_rays,receivers[e]->texture),scene.status().c_str());
        rays[e]=scene.enqueue_ray_geometry(gpu.command,e,w,h,camera(e));check(rays[e].complete,scene.status().c_str());
    }
    gpu.submit();check(scene.notify_submitted(ray_token),"Native ray source token not restored after style checks");
    // Real DXR output remains resident; this tests the actual GPU composition,
    // not a CPU replacement of the native ray result.
    if(hardware) {
        for(unsigned eye=0;eye<2;++eye) native_water_layer_tests(gpu,format,rays[eye]);
        std::array<render::shadows::SdlDxrShadows,2> traced_shadows,traced_reflections;
        std::array<Bytes,2> mask,reflected;std::array<render::shadows::GpuShadowOutput,2> shadow_outputs;
        std::array<render::shadows::GpuReflectionOutput,2> reflection_outputs;
        const std::array<unsigned,256> palette{};
        for(unsigned e=0;e<2;++e) {
            const auto& ray=rays[e];const render::GpuScene::RayGeometryOutput geometry{ray.device,ray.buffer,ray.vertex_count,true,ray.materials,ray.material_offset,ray.material_bytes};
            render::shadows::Camera ray_camera{w,h,ray.projection[0],ray.projection[2],ray.projection[3],ray.projection[1]};
            const render::shadows::PrimaryRayRange range{ray.near_plane,*ray.far_plane};
            check(traced_shadows[e].render_resident(gpu.device,{},ray_camera,{.3,-.2,-1},std::nullopt,&geometry,false,range),traced_shadows[e].status().c_str());
            check(traced_reflections[e].render_reflections(gpu.device,ray_camera,geometry,palette,0xffd07030U,0,0,{},0,{1,0,0,0,1,0,0,0,1},nullptr,{},0,nullptr,false,range),traced_reflections[e].status().c_str());
            shadow_outputs[e]=traced_shadows[e].output();reflection_outputs[e]=traced_reflections[e].reflection_output();
            check(traced_shadows[e].readback(mask[e]),traced_shadows[e].status().c_str());check(traced_reflections[e].readback(reflected[e]),traced_reflections[e].status().c_str());
        }
        gpu.begin();
        for(unsigned e=0;e<2;++e) {
            check(compositor.enqueue(gpu.command,sources[e]->texture,receivers[e]->texture,targets[e]->texture,w,h,&shadow_outputs[e],&reflection_outputs[e]),compositor.status().c_str());
            check(scene.enqueue_eye(gpu.command,targets[e]->texture,depths[e]->texture,w,h,camera(e),{},render::CalibratedScenePhase::after_rays,nullptr,ray_token),scene.status().c_str());
        }
        gpu.submit();
        for(unsigned e=0;e<2;++e) {
            auto expected=oracle(base[e],coverage[e],w,h,mask[e],w,reflected[e],w*4,format,1,1);
            for(unsigned at=0;at<expected.size();at+=4) if(!std::equal(reference[e].begin()+at,reference[e].begin()+at+4,base[e].begin()+at))
                std::copy_n(reference[e].begin()+at,4,expected.begin()+at);
            const auto actual=targets[e]->read();compare(actual,expected,"Resident DXR -> native eye composite differs from independent oracle");
            check(actual!=reference[e],"Native ray composite had no visible effect");
        }
        check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
    }
    // Cancelled uploads must not permit post-ray reuse of a previous frame.
    gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());const auto cancelled=scene.upload_token();
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;gpu.begin();
    check(!scene.enqueue_eye(gpu.command,target0.texture,depth0.texture,w,h,camera(0),{},render::CalibratedScenePhase::after_rays,nullptr,token)
        && !scene.enqueue_eye(gpu.command,target0.texture,depth0.texture,w,h,camera(0),{},render::CalibratedScenePhase::after_rays,nullptr,cancelled),
        "Cancelled/unsubmitted source allowed ray continuation");SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    // Selected post-ray packets must reject; line-only packets must not disappear
    // from the caster batch while the rest is reported complete.
    gpu.begin();auto bad=draws;bad[4].ray_caster=true;
    check(!scene.upload(gpu.command,bad) && !scene.upload_token(),"Post-ray source accepted as a caster");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    for(unsigned fault=0;fault<4;++fault) {
        auto invalid=draws;invalid[2].effect_layer=fault==0?3:1;
        if(fault==1) invalid[2].preserve_native_colour=true;
        if(fault==2) invalid[2].camera_override=camera(0);
        if(fault==3) invalid[2].after_rays=true;
        gpu.begin();check(!scene.upload(gpu.command,invalid) && !scene.upload_token(),"Invalid native effect ownership was encoded");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    vr::DrawPacket lines;lines.geometry.line_vertices={surface[0],surface[1]};
    render::CalibratedScenePacket selected_lines{&lines};selected_lines.ray_caster=true;
    gpu.begin();check(scene.upload_packets(gpu.command,std::span(&selected_lines,1)),scene.status().c_str());
    const auto line_rays=scene.enqueue_ray_geometry(gpu.command,0,w,h,camera(0));
    check(line_rays.complete && line_rays.vertex_count==6,
        "Selected line-only packet was dropped instead of producing two finite ray triangles");
    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
}
}
// Included beside the spatial reference; this is a fixture-only readback path.
#include "check_calibrated_ssaa.inc"
#include "check_calibrated_fsr1.inc"
int main(int argc,char** argv) try {
    const char* backend=argc>1?argv[1]:"direct3d12";check(std::string_view(backend)=="direct3d12" || std::string_view(backend)=="vulkan","Use direct3d12|vulkan");
    const bool curved_paths_only=argc>2 && std::string_view(argv[2])=="--curved-paths-only";
    const bool curved_trace_only=argc>2 && std::string_view(argv[2])=="--curved-trace-only";
    const bool native_shift_only=argc>2 && std::string_view(argv[2])=="--native-curved-shift-only";
    const bool native_curved_only=native_shift_only || (argc>2 && std::string_view(argv[2])=="--native-curved-cache-only");
    check(!curved_trace_only || std::string_view(backend)=="direct3d12","Curved diagnostic trace currently requires direct3d12");
    check(!native_curved_only || std::string_view(backend)=="direct3d12","Native curved coupling currently requires direct3d12");
    Gpu gpu;check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    if(curved_paths_only && std::string_view(backend)=="vulkan") {
        const auto props=SDL_CreateProperties();check(props,SDL_GetError());
        SDL_GPUVulkanOptions options{};options.vulkan_api_version=(1U<<22)|(2U<<12); // Vulkan 1.2, not the default 1.0 request.
        const bool ok=SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,backend)
            && SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true)
            && SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true)
            && SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true)
            && SDL_SetPointerProperty(props,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,&options);
        if(ok)gpu.device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);
    } else gpu.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,true,backend);
    check(gpu.device,SDL_GetError());
    std::cout<<"Calibrated component adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu.device),
        SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<" driver="<<SDL_GetGPUDeviceDriver(gpu.device)<<std::endl;
    const bool aa_only=argc>2 && std::string_view(argv[2])=="--aa-only";
    const bool ssaa_only=argc>2 && std::string_view(argv[2])=="--ssaa-only";
    const bool temporal_only=argc>2 && std::string_view(argv[2])=="--temporal-only";
    const bool patterns_only=argc>2 && std::string_view(argv[2])=="--temporal-pattern-only";
    const bool edges_only=argc>2 && std::string_view(argv[2])=="--temporal-edge-only";
    const bool fsr_only=argc>2 && std::string_view(argv[2])=="--fsr-only";
    const bool ground_only=argc>2 && std::string_view(argv[2])=="--ground-only";
    const bool native_rays_only=argc>2 && std::string_view(argv[2])=="--native-rays-only";
    const bool history_images_only=argc>2 && std::string_view(argv[2])=="--history-images-only";
    const bool ray_history_only=argc>2 && std::string_view(argv[2])=="--ray-history-only";
    const bool reflection_history_only=argc>2 && std::string_view(argv[2])=="--reflection-history-only";
    const bool reflection_paths_only=argc>2 && std::string_view(argv[2])=="--reflection-paths-only";
    const bool water_guides_only=argc>2 && std::string_view(argv[2])=="--water-guides-only";
    const bool panel_water_only=argc>2 && std::string_view(argv[2])=="--water-panel-guides-only";
    for(const auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
        if(native_curved_only) {native_curved_cache_tests(gpu,format,native_shift_only);continue;}
        if(curved_trace_only) {reflection_curved_cache_tests(gpu,format,true);break;}
        if(curved_paths_only) {reflection_curved_cache_tests(gpu,format);continue;}
        if(reflection_paths_only) {
            reflection_path_tests(gpu,format);
            reflection_path_tests(gpu,format,true,false);
            reflection_path_tests(gpu,format,true,true);continue;
        }
        if(reflection_history_only) {reflection_history_tests(gpu,format);reflection_lobe_tests(gpu,format);continue;}
        if(panel_water_only) {water_panel_guide_tests(gpu,format);continue;}
        if(ray_history_only) {temporal_liquid_tests(gpu,format);continue;}
        if(history_images_only) {persistence_tests(gpu,format);exposure_tests(gpu,format);continue;}
        if(aa_only) {aa_tests(gpu,format);continue;}
        if(fsr_only) {native_fsr_tests(gpu,format);continue;}
        if(ssaa_only) {ssaa_tests(gpu,format);continue;}
        if(patterns_only) {temporal_pattern_tests(gpu,format);continue;}
        if(edges_only) {temporal_edge_tests(gpu,format);continue;}
        if(temporal_only) {temporal_aa_tests(gpu,format);temporal_liquid_tests(gpu,format);temporal_pattern_tests(gpu,format);continue;}
        if(native_rays_only) {phase_tests(gpu,format,std::string_view(backend)=="direct3d12");continue;}
        water_guide_tests(gpu,format);water_panel_guide_tests(gpu,format);
        if(water_guides_only) continue;
        ground_transport_tests(gpu,format);
        if(ground_only) continue;
        std::cout<<"Format "<<unsigned(format)<<": strided compositor"<<std::endl;strided_tests(gpu,format);
        warp_tests(gpu,format);
        global_tests(gpu,format);
        appearance_tests(gpu,format);
        bloom_tests(gpu,format);
        persistence_tests(gpu,format);
        exposure_tests(gpu,format);
        aa_tests(gpu,format);
        ssaa_tests(gpu,format);
        temporal_aa_tests(gpu,format);
        temporal_liquid_tests(gpu,format);
        reflection_history_tests(gpu,format);
        reflection_lobe_tests(gpu,format);
        reflection_path_tests(gpu,format);
        reflection_path_tests(gpu,format,true,false);
        reflection_path_tests(gpu,format,true,true);
        temporal_pattern_tests(gpu,format);
        temporal_edge_tests(gpu,format);
        native_fsr_tests(gpu,format);
        std::cout<<"Format "<<unsigned(format)<<": receiver/phases/native rays"<<std::endl;phase_tests(gpu,format,std::string_view(backend)=="direct3d12");
    }
    if(history_images_only) {std::cout<<"Native retained history/meter images "<<backend<<": four formats and independent temporal/meter pixel oracles passed.\n";return 0;}
    if(reflection_history_only) {std::cout<<"Native reflected-radiance resolver "<<backend<<": four formats, independent identity/visibility/colour oracles and accepted-frame lifetime passed.\n";return 0;}
    if(reflection_paths_only) {std::cout<<"Ordered reflection-path consumer "<<backend<<": four formats passed. Synthetic GPU cache/forward-optics fixture only; no native RT producer, live owner or physical display acceptance in this run.\n";return 0;}
    if(curved_paths_only) {std::cout<<"Curved reflection-path consumer "<<backend<<": four component formats passed; native producer/live owner and physical display acceptance are separate.\n";return 0;}
    if(curved_trace_only) {std::cout<<"Curved trace captured; this is NOT a color, native-owner or performance acceptance.\n";return 0;}
    if(native_curved_only) {std::cout<<"Native curved producer/consumer coupling direct3d12: four authored-source/contract formats passed (shifted="<<native_shift_only<<"); arbitrary geometry/liquid motion, general old visibility, live owner/backend and performance qualification remain separate.\n";return 0;}
    if(ray_history_only) {std::cout<<"Native ray history "<<backend<<": four formats and independent consumed-ray/previous-eligibility pixel oracles passed.\n";return 0;}
    if(panel_water_only) {std::cout<<"Native panel liquid guides "<<backend<<": four formats passed; no physical Leia claim.\n";return 0;}
    if(ground_only || native_rays_only || water_guides_only) {std::cout<<"Native ground/ray compositor "<<backend<<": four formats passed; no physical Leia claim.\n";return 0;}
    if(aa_only || ssaa_only || temporal_only || patterns_only || edges_only || fsr_only) {std::cout<<"Native AA/upscale "<<backend<<": four formats passed; no physical Leia claim.\n";return 0;}
    std::cout<<"Calibrated ray composite "<<backend<<": four runtime formats, packed/uint masks, strided/partial rows, alpha/colour oracle, binary cutouts, emissive exclusion, independent calibrated eyes, original depth/source-order and stale/cancelled continuation passed. "
        <<(std::string_view(backend)=="direct3d12"?"Resident native DXR shadows/reflections composited and checked.":"Raster/composite checks only; no native Vulkan DXR claim.")<<" Fixture, not physical Leia validation.\n";return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
