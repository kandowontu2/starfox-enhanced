#include "starfox/render/gpu_clip.hpp"
#include "starfox/render/gpu_projection.hpp"
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/raster_commands.hpp"
#include "starfox/render/span_clear_policy.hpp"
#include "starfox/render/software_renderer.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <optional>
#include <limits>
#include <tuple>
namespace {
using V=std::array<int,4>;
int word(int v){return std::bit_cast<std::int16_t>(std::uint16_t(v));}
void require(bool ok){if(!ok) throw std::runtime_error(SDL_GetError());}
std::vector<V> reference(std::vector<V> input,int width,int height) {
    for(unsigned plane=0;plane<4 && !input.empty();++plane) {
        const unsigned axis=plane/2;const int boundary=plane%2?(axis?height:width):0;
        const auto inside=[&](const V& v){return plane%2?v[axis]<boundary:v[axis]>=boundary;};
        std::vector<V> output;auto previous=input.back();
        for(const auto& current:input) {
            const bool p=inside(previous),c=inside(current);
            if(p!=c) {
                const auto& anchor=p?previous:current;const auto& outside=p?current:previous;
                V intersection=anchor;
                for(unsigned k=0;k<4;++k) {
                    if(k==axis) {intersection[k]=boundary;continue;}
                    const int denominator=word(outside[axis]-anchor[axis]);
                    if(denominator) intersection[k]=word(anchor[k]+int(std::int64_t(word(boundary-anchor[axis]))
                        *word(outside[k]-anchor[k])/denominator));
                }
                output.push_back(intersection);
            }
            if(c) output.push_back(current);
            previous=current;
        }
        input=std::move(output);
    }
    return input;
}
struct Resources {
    SDL_GPUDevice* device{};std::array<SDL_GPUBuffer*,8> buffers{};
    SDL_GPUTransferBuffer *upload{},*download{};
    ~Resources(){if(device){SDL_WaitForGPUIdle(device);for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        SDL_DestroyGPUDevice(device);}SDL_Quit();}
};
using Fractional=std::array<double,4>;
std::vector<Fractional> fractional_reference(std::vector<Fractional> input,int width,int height) {
    for(unsigned plane=0;plane<4 && !input.empty();++plane) {
        const unsigned axis=plane/2;const double boundary=plane%2?(axis?height:width):0;
        const auto inside=[&](const Fractional& v){return plane%2?v[axis]<boundary:v[axis]>=boundary;};
        std::vector<Fractional> output;auto previous=input.back();
        for(const auto& current:input) {
            if(inside(previous)!=inside(current)) {
                const double denominator=current[axis]-previous[axis];
                const double amount=denominator==0?0:(boundary-previous[axis])/denominator;
                Fractional intersection;
                for(unsigned k=0;k<4;++k) intersection[k]=previous[k]+(current[k]-previous[k])*amount;
                output.push_back(intersection);
            }
            if(inside(current)) output.push_back(current);
            previous=current;
        }
        input=std::move(output);
    }
    return input;
}
}
#include "check_span_clear_reuse.inc"
#include "check_span_clear_dispatch.inc"
#include "check_cooperative_span_dispatch.inc"
#include "check_projection_cache_clip.inc"
#include "check_clip_preparation.inc"
int main(int argc,char** argv)try {
    Resources r;require(SDL_Init(SDL_INIT_VIDEO));
    const auto properties=SDL_CreateProperties();require(properties!=0);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,std::getenv("STARFOX_TEST_LOW_POWER_GPU")!=nullptr);
    r.device=SDL_CreateGPUDeviceWithProperties(properties);SDL_DestroyProperties(properties);require(r.device);
    std::cout<<"GPU adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(r.device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")
        <<" driver="<<SDL_GetGPUDeviceDriver(r.device)<<std::endl;
    check_clip_preparation(r.device);check_clip_preparation(r.device,true);
    if(argc==2 && std::string(argv[1])=="--preparation") return 0;
    if(argc==2 && std::string(argv[1])=="--span-clear-reuse") {
        check_span_clear_reuse(r.device);check_span_clear_dispatch(r.device);return 0;
    }
    if(argc==2 && std::string(argv[1])=="--colour-spans") {
        check_span_clear_reuse(r.device,true);return 0;
    }
    if(argc==2 && std::string(argv[1])=="--cooperative-spans") {
        check_span_clear_reuse(r.device,false,true);return 0;
    }
    if(argc==2 && std::string(argv[1])=="--cooperative-dispatch") {
        check_cooperative_span_dispatch(r.device);return 0;
    }
    if(argc==2 && std::string(argv[1])=="--projection-cache") {check_projection_cache_clip(r.device);return 0;}
    if(argc==2 && std::string(argv[1])=="--radix16") {check_projection_cache_clip(r.device,true);return 0;}
    if(argc==2 && std::string(argv[1])=="--interior") {check_projection_cache_clip(r.device,false,true);return 0;}
    const bool small=argc==2 && std::string(argv[1])=="--small-clip";
    if(argc!=1 && !small) throw std::runtime_error("usage: starfox_gpu_clip_check [--preparation|--span-clear-reuse|--colour-spans|--cooperative-spans|--cooperative-dispatch|--small-clip|--projection-cache|--radix16|--interior]");
    if(small && (!std::getenv("STARFOX_TEST_SMALL_CLIP") || std::getenv("STARFOX_TEST_FULL_CLIP")))
        throw std::runtime_error("Small-clip oracle requires the opt-in and no full-clip override");
    if(small && !(SDL_GetGPUShaderFormats(r.device)&SDL_GPU_SHADERFORMAT_SPIRV))
        throw std::runtime_error("Small clipping is restricted to Vulkan after D3D12 reflected-water device loss");
    starfox::render::GpuClip clip;
    starfox::render::GpuProjection projection;
    starfox::render::GpuRaster raster;
    constexpr unsigned n=4097;
    std::vector<V> points,corners,polygons;std::vector<Uint32> visibility(n,1);
    std::vector<std::vector<V>> source(n);
    std::uint32_t random=71893;
    const auto next=[&](){random=random*1664525U+1013904223U;return word(int(random&65535));};
    for(unsigned i=0;i<n;++i) {
        const unsigned count=small?4:3+i%30;polygons.push_back({int(corners.size()),int(count),int(i),0});
        for(unsigned j=0;j<count;++j) {
            V v{next(),next(),next(),next()};
            if(i%2) {v[0]%=600;v[1]%=400;}
            corners.push_back({int(points.size()),v[2],v[3],0});points.push_back({v[0],v[1],256,1});source[i].push_back(v);
        }
        if(i%13==0) visibility[i]=0;
    }
    // Exact exclusive boundaries, zero length edges, and asymmetric UV division.
    source[1]={{-1,0,-32768,32767},{224,0,3,7},{224,192,11,13},{0,192,17,19}};
    for(unsigned j=0;j<4;++j) {
        const auto index=unsigned(polygons[1][0])+j;const auto& v=source[1][j];
        points[index]={v[0],v[1],256,1};corners[index]={int(index),v[2],v[3],0};
    }
    polygons[2][0]=-1; // Wrapped offsets must fail before indexing.
    polygons[3][1]=33;
    polygons[4][2]=int(n);
    source[31]={{-50,-10,0,0},{8,120,0,7},{290,135,7,7},{150,8,7,0}};
    source[32]={{10,10,0,0},{10,100,0,0},{100,100,0,0},{100,10,0,0},{10,10,0,0}};
    if(small) source[32].pop_back();
    for(unsigned i:{31U,32U}) for(unsigned j=0;j<source[i].size();++j) {
        const auto index=unsigned(polygons[i][0])+j;const auto& v=source[i][j];
        points[index]={v[0],v[1],256,1};corners[index]={int(index),v[2],v[3],0};
    }
    std::vector<starfox::render::NativeProjectionPoint> projection_points;
    for(const auto& p:points) projection_points.push_back({p[0],p[1],256,0,0,0,0,0});
    auto projected_source=source;
    for(auto& polygon:projected_source) for(auto& v:polygon) {
        const int dominant=std::max(std::abs(v[0]),std::abs(v[1]));
        if(dominant>=16384) {v[0]=v[0]*16383/dominant;v[1]=v[1]*16383/dominant;}
    }
    std::vector<V> faces;std::vector<Uint32> projected_visibility(n);
    unsigned first=0;
    for(unsigned i=0;i<n;++i) {
        faces.push_back({int(first),int(first+1),int(first+2),0});first+=unsigned(source[i].size());
        const auto& p=projected_source[i];
        const auto area=std::int64_t(word(p[1][0]-p[0][0]))*word(p[2][1]-p[0][1])
            -std::int64_t(word(p[1][1]-p[0][1]))*word(p[2][0]-p[0][0]);
        projected_visibility[i]=std::bit_cast<std::int32_t>(std::uint32_t(area))<0;
    }
    std::vector<starfox::render::RasterCommand> materials(n);
    for(unsigned i=0;i<n;++i) {
        auto& material=materials[i];material.even=1+i%15;material.odd=1+(i*7)%15;
        material.dither=material.even!=material.odd;material.tag=1;
        material.has_surface=i%3!=0;
        material.surface={float(i)/64,0,-1,float(256+i)};
    }
    materials[32].has_surface=0;
    materials[31].textured=1;materials[31].tag=4;
    materials[31].u_mask=materials[31].v_mask=7;
    materials[31].reserved0=2;materials[31].reserved1=Uint32(-1);
    std::vector<std::uint8_t> texels(64);
    for(unsigned i=0;i<64;++i) texels[i]=i%5==0?0:std::uint8_t(1+i%15);
    const Uint32 sizes[]{Uint32(points.size()*16),Uint32(corners.size()*16),Uint32(polygons.size()*16),n*4,
        Uint32(projection_points.size()*32),n*16,n*96,64};
    Uint32 total=0;for(unsigned i=0;i<8;++i) {SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
        r.buffers[i]=SDL_CreateGPUBuffer(r.device,&info);require(r.buffers[i]);total+=sizes[i];}
    SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total,0},download_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,16*1024*1024,0};
    r.upload=SDL_CreateGPUTransferBuffer(r.device,&upload_info);require(r.upload);
    r.download=SDL_CreateGPUTransferBuffer(r.device,&download_info);require(r.download);
    auto* data=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(r.device,r.upload,true));require(data);
    const void* sources[]{points.data(),corners.data(),polygons.data(),visibility.data(),projection_points.data(),faces.data(),materials.data(),texels.data()};
    Uint32 offset=0;for(unsigned i=0;i<8;++i){std::memcpy(data+offset,sources[i],sizes[i]);offset+=sizes[i];}
    SDL_UnmapGPUTransferBuffer(r.device,r.upload);
    auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
    offset=0;for(unsigned i=0;i<8;++i){SDL_GPUTransferBufferLocation from{r.upload,offset};SDL_GPUBufferRegion to{r.buffers[i],0,sizes[i]};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=sizes[i];}SDL_EndGPUCopyPass(copy);
    require(SDL_SubmitGPUCommandBuffer(command));
    std::size_t checked=0;
    std::size_t span_pixels_checked=0;
    std::size_t span_pixels_drawn=0;
    std::size_t preserved_surface_pixels=0;
    for(bool chained:{false,true}) for(unsigned count:{1U,31U,32U,33U,n,33U}) for(const auto dimensions:{std::array<int,2>{224,192},{398,192},{796,448}}) {
        command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        auto* projected=r.buffers[0];auto* visible=r.buffers[3];
        if(chained) {
            projected=static_cast<SDL_GPUBuffer*>(projection.enqueue(r.device,command,r.buffers[4],Uint32(points.size())));
            if(!projected){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(projection.status());}
            visible=static_cast<SDL_GPUBuffer*>(projection.enqueue_visibility(command,r.buffers[5],n));
            if(!visible){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(projection.status());}
        }
        starfox::render::NativeClipSettings settings{count,Uint32(points.size()),Uint32(corners.size()),n,dimensions[0],dimensions[1]};
        auto* output=static_cast<SDL_GPUBuffer*>(clip.enqueue(r.device,command,projected,r.buffers[1],r.buffers[2],visible,settings,false,nullptr,0,nullptr,0,small?4U:0U));
        if(!output){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(clip.status());}
        SDL_GPUBuffer* spans=nullptr;
        starfox::render::GpuRasterOutput rasterized;
        if(chained && count==33) {
            spans=static_cast<SDL_GPUBuffer*>(clip.enqueue_spans(command,r.buffers[6]));
            if(!spans){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(clip.status());}
            rasterized=raster.enqueue_row_spans(r.device,command,spans,count,dimensions[0],dimensions[1],true,r.buffers[7]);
            if(!rasterized.pixels){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(raster.status());}
        }
        copy=SDL_BeginGPUCopyPass(command);require(copy);SDL_GPUBufferRegion from{output,0,count*129*16};SDL_GPUTransferBufferLocation to{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);
        if(spans) {from={spans,0,count*Uint32(dimensions[1])*96};to.offset=count*129*16;SDL_DownloadFromGPUBuffer(copy,&from,&to);}
        const Uint32 raster_offset=count*129*16+count*Uint32(dimensions[1])*96;
        if(rasterized.pixels) {
            from={static_cast<SDL_GPUBuffer*>(rasterized.pixels),0,Uint32(dimensions[0]*dimensions[1]*4)};
            to.offset=raster_offset;SDL_DownloadFromGPUBuffer(copy,&from,&to);
            from={static_cast<SDL_GPUBuffer*>(rasterized.surfaces),0,Uint32(dimensions[0]*dimensions[1]*16)};
            to.offset=raster_offset+Uint32(dimensions[0]*dimensions[1]*4);SDL_DownloadFromGPUBuffer(copy,&from,&to);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* values=static_cast<const V*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(values);
        for(unsigned i=0;i<count;++i) {
            const auto header=values[i*129];
            if(i>=2 && i<=4) {if(header[0]!=0 || header[1]!=1) throw std::runtime_error("Invalid clipping descriptor accepted");continue;}
            const auto expected=(chained?projected_visibility[i]:visibility[i])
                ?reference(chained?projected_source[i]:source[i],dimensions[0],dimensions[1]):std::vector<V>{};
            if(header[1]!=0 || header[0]!=int(expected.size())) throw std::runtime_error("Clipped count mismatch at "+std::to_string(i));
            for(unsigned j=0;j<expected.size();++j) if(values[i*129+1+j]!=expected[j])
                throw std::runtime_error("Clipped corner mismatch at "+std::to_string(i));
        }
        if(spans) {
            const auto* emitted=reinterpret_cast<const starfox::render::RasterCommand*>(values+count*129);
            starfox::render::SoftwareRenderer renderer;
            std::vector<std::uint8_t> combined(std::size_t(dimensions[0])*dimensions[1]);
            std::vector<Uint32> tags(combined.size());
            std::vector<Uint32> surface_palette(combined.size());
            std::vector<std::array<float,4>> surface_values(combined.size(),{0,0,1,0});
            starfox::render::RenderPose pose;pose.x=pose.y=pose.z=0;pose.vanish_x=pose.vanish_y=0;
            pose.use_rotation_matrix=true;pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};
            pose.force_colour=true;
            for(unsigned i=0;i<count;++i) {
                pose.forced_colour=std::uint8_t(materials[i].even|(materials[i].odd<<4));
                pose.force_colour=i!=31;
                pose.texture_scroll_x=2;pose.texture_scroll_y=-1;
                starfox::render::Framebuffer cpu(dimensions[0],dimensions[1]),gpu(dimensions[0],dimensions[1]);
                if(!(i>=2 && i<=4) && projected_visibility[i]) {
                    starfox::assets::Shape shape;starfox::assets::Face face;
                    face.visibility_index=-1;face.normal={0,0,127};
                    for(unsigned j=0;j<source[i].size();++j) {
                        const auto& p=source[i][j];shape.vertices.push_back({-p[0],-p[1],-256});
                        shape.word_coordinates.push_back(true);face.vertex_indices.push_back(std::uint8_t(j));
                    }
                    shape.faces.push_back(face);shape.colour_words={0x3f99};shape.colour_materials={{0x3f99,{}}};
                    if(i==31) {
                        shape.colour_words={0x4000};shape.colour_materials={{0x4000,{}}};
                        starfox::assets::TextureImage texture;
                        texture.descriptor=0x4000;texture.u_mask=texture.v_mask=7;texture.texels=texels;
                        texture.coordinates={{{0,0},{0,7},{7,7},{7,0}}};shape.textures.push_back(texture);
                    }
                    renderer.draw(shape,pose,cpu);
                }
                for(int row=0;row<dimensions[1];++row) {
                    const auto& c=emitted[i*dimensions[1]+row];
                    for(int y=c.top;y<c.bottom;++y) for(int x=c.left;x<c.right;++x) {
                        auto colour=std::uint8_t(c.dither && ((x^y)&1)?c.odd:c.even);
                        if(c.textured) {
                            const auto u=((((Uint32(c.u)+Uint32(c.du)*Uint32(x-c.left))&65535U)>>8)+c.reserved0)&7U;
                            const auto v=((((Uint32(c.v)+Uint32(c.dv)*Uint32(x-c.left))&65535U)>>8)+c.reserved1)&7U;
                            colour=texels[v*8+u];if(!colour) continue;
                        }
                        gpu.set(x,y,colour);
                    }
                }
                if(!std::equal(cpu.pixels().begin(),cpu.pixels().end(),gpu.pixels().begin()))
                    throw std::runtime_error("GPU solid span pixels differ from SoftwareRenderer at polygon "+std::to_string(i));
                span_pixels_checked+=cpu.pixels().size();
                span_pixels_drawn+=std::count_if(cpu.pixels().begin(),cpu.pixels().end(),[](auto pixel){return pixel!=0;});
                for(std::size_t pixel=0;pixel<combined.size();++pixel)
                    if(cpu.pixels()[pixel]!=0) {
                        combined[pixel]=cpu.pixels()[pixel];
                        tags[pixel]=materials[i].tag;
                        if(materials[i].has_surface) {
                            surface_palette[pixel]=(Uint32(cpu.pixels()[pixel])<<16)|(1U<<24);
                            surface_values[pixel]=materials[i].surface;
                        }
                    }
            }
            const auto* packed=reinterpret_cast<const Uint32*>(reinterpret_cast<const std::uint8_t*>(values)+raster_offset);
            const auto* normals=reinterpret_cast<const std::array<float,4>*>(packed+combined.size());
            for(std::size_t pixel=0;pixel<combined.size();++pixel) {
                const Uint32 expected=(combined[pixel]?Uint32(combined[pixel])|(tags[pixel]<<8):0U)|surface_palette[pixel];
                if(packed[pixel]!=expected) throw std::runtime_error("Resident GPU final pixels differ from source painter");
                if(normals[pixel]!=surface_values[pixel]) throw std::runtime_error("Resident GPU surface owner differs from source painter");
                preserved_surface_pixels+=surface_palette[pixel]!=0 && ((surface_palette[pixel]>>16)&255U)!=combined[pixel];
            }
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);checked+=count;
    }
    std::cout<<clip.status()<<": "<<checked<<" exact polygons; resident projection/visibility chain, XY/UV, viewport boundaries, invalid descriptors and allocation reuse passed\n";
    if(!span_pixels_drawn) throw std::runtime_error("Solid span fixture rendered no pixels");
    std::cout<<"GPU solid spans matched SoftwareRenderer across "<<span_pixels_checked<<" pixels ("<<span_pixels_drawn<<" drawn)\n";
    if(!preserved_surface_pixels) throw std::runtime_error("Fixture did not separate pixel and surface ownership");
    std::cout<<"Resident GPU raster colours/dither/tags/surfaces match; "<<preserved_surface_pixels
        <<" pixels retain metadata beneath later non-surface spans\n";
    // Exercise fractional clipping itself, not merely pipeline creation. Reuse
    // the 32-byte projection input buffer after the native chain has completed.
    std::vector<starfox::render::ContinuousProjectedPoint> fractional_points(points.size());
    for(std::size_t i=0;i<points.size();++i) {
        auto& p=fractional_points[i];
        p.camera[0]=float(points[i][0])+.25f;p.camera[1]=float(points[i][1])-.125f;p.camera[2]=256;p.camera[3]=1;
        std::copy_n(p.camera,4,p.screen);
    }
    fractional_points[polygons[5][0]].camera[2]=-1;
    fractional_points[polygons[6][0]].camera[3]=-1;
    fractional_points[polygons[7][0]].camera[2]=-1;polygons[7][3]=1;
    for(int j=0;j<polygons[8][1];++j) fractional_points[polygons[8][0]+j].camera[2]=-1;
    std::vector<std::array<float,4>> projection_parameters(n,{0,0,256,0});
    data=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(r.device,r.upload,true));require(data);
    std::memcpy(data,fractional_points.data(),fractional_points.size()*32);
    const Uint32 parameter_offset=Uint32(fractional_points.size()*32);
    std::memcpy(data+parameter_offset,projection_parameters.data(),n*16);
    std::memcpy(data+parameter_offset+n*16,polygons.data(),n*16);
    SDL_UnmapGPUTransferBuffer(r.device,r.upload);
    command=SDL_AcquireGPUCommandBuffer(r.device);require(command);copy=SDL_BeginGPUCopyPass(command);require(copy);
    SDL_GPUTransferBufferLocation fractional_from{r.upload,0};
    SDL_GPUBufferRegion fractional_to{r.buffers[4],0,Uint32(fractional_points.size()*32)};
    SDL_UploadToGPUBuffer(copy,&fractional_from,&fractional_to,true);
    fractional_from.offset=parameter_offset;fractional_to={r.buffers[0],0,n*16};
    SDL_UploadToGPUBuffer(copy,&fractional_from,&fractional_to,true);
    fractional_from.offset=parameter_offset+n*16;fractional_to={r.buffers[2],0,n*16};
    SDL_UploadToGPUBuffer(copy,&fractional_from,&fractional_to,true);SDL_EndGPUCopyPass(copy);
    require(SDL_SubmitGPUCommandBuffer(command));
    std::size_t fractional_checked=0;double maximum_error=0;
    std::size_t scaled_pixels_checked=0;
    std::size_t near_corners_checked=0;
    for(bool custom_raster:{false,true}) for(bool near_enabled:{false,true}) for(unsigned count:{1U,31U,32U,33U,n,33U}) for(const auto dimensions:{std::array<int,2>{224,192},{398,192},{796,448}}) {
        command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        starfox::render::NativeClipSettings settings{count,Uint32(points.size()),Uint32(corners.size()),n,dimensions[0],dimensions[1]};
        auto* output=static_cast<SDL_GPUBuffer*>(clip.enqueue(r.device,command,r.buffers[4],r.buffers[1],r.buffers[2],r.buffers[3],settings,true,
            near_enabled?r.buffers[0]:nullptr,near_enabled?n:0,nullptr,0,small?4U:0U));
        if(!output){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(clip.status());}
        if(clip.enqueue_spans(command,r.buffers[6],false,0)) {
            SDL_CancelGPUCommandBuffer(command);throw std::runtime_error("Span emitter accepted zero render scale");
        }
        const unsigned scale=dimensions[0]==224?1U:dimensions[0]==398?2U:4U;
        const Uint32 raster_width=custom_raster?Uint32(dimensions[0])*3/2:Uint32(dimensions[0])*scale;
        const Uint32 raster_height=custom_raster?Uint32(dimensions[1])*3/2:Uint32(dimensions[1])*scale;
        SDL_GPUBuffer* scaled_spans=nullptr;
        if(count==33) {
            if(clip.enqueue_spans(command,r.buffers[6],true,scale,nullptr,1,nullptr,0,nullptr,{1,0}))
                throw std::runtime_error("Span emitter accepted partial raster dimensions");
            scaled_spans=static_cast<SDL_GPUBuffer*>(clip.enqueue_spans(command,r.buffers[6],custom_raster || scale>1,scale,nullptr,1,nullptr,0,nullptr,
                custom_raster?std::array<Uint32,2>{raster_width,raster_height}:std::array<Uint32,2>{}));
            if(!scaled_spans){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(clip.status());}
        }
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion from{output,0,count*129*16};SDL_GPUTransferBufferLocation to{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);
        if(scaled_spans) {
            from={scaled_spans,0,count*raster_height*96};to.offset=count*129*16;
            SDL_DownloadFromGPUBuffer(copy,&from,&to);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* words=static_cast<const V*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(words);
        const auto* values=reinterpret_cast<const std::array<float,4>*>(words);
        for(unsigned i=0;i<count;++i) {
            const auto header=words[i*129];
            if(i>=2 && i<=6 && !(i==5 && near_enabled)) {
                const int status=i==5?3:1;
                if(header[0]!=0 || header[1]!=status) throw std::runtime_error("Fractional invalid/near classification mismatch");
                continue;
            }
            std::vector<Fractional> input;
            for(const auto& v:source[i]) input.push_back({v[0]+.25,v[1]-.125,double(v[2]),double(v[3])});
            if(i==5 && near_enabled) {
                std::vector<std::array<double,3>> camera;
                for(int j=0;j<polygons[i][1];++j) {
                    const auto& p=fractional_points[polygons[i][0]+j];
                    camera.push_back({p.camera[0],p.camera[1],p.camera[2]});
                }
                std::vector<std::array<double,3>> clipped_camera;
                for(std::size_t j=0;j<camera.size();++j) {
                    const auto& a=camera[j];const auto& b=camera[(j+1)%camera.size()];
                    if(a[2]>=0) clipped_camera.push_back(a);
                    if((a[2]>=0)!=(b[2]>=0)) {
                        const double amount=a[2]/(a[2]-b[2]);
                        clipped_camera.push_back({a[0]+(b[0]-a[0])*amount,a[1]+(b[1]-a[1])*amount,0});
                    }
                }
                input.clear();
                for(const auto& p:clipped_camera) {
                    const double depth=p[2]==0?1:p[2];
                    input.push_back({p[0]*256/depth,p[1]*256/depth,0,0});
                }
            }
            if(i==7 || i==8) input.clear();
            const auto expected=visibility[i]?fractional_reference(input,dimensions[0],dimensions[1]):std::vector<Fractional>{};
            if(i==5 && near_enabled) near_corners_checked+=expected.size();
            if(header[1]!=0 || header[0]!=int(expected.size()))
                throw std::runtime_error("Fractional clipped count mismatch at "+std::to_string(i));
            for(unsigned j=0;j<expected.size();++j) for(unsigned k=0;k<4;++k) {
                const double error=std::abs(double(values[i*129+1+j][k])-expected[j][k]);
                maximum_error=std::max(maximum_error,error);
                if(!std::isfinite(values[i*129+1+j][k]) || error>(k<2?0.0001:0.001))
                    throw std::runtime_error("Fractional clip mismatch at "+std::to_string(i)+" error "+std::to_string(error));
            }
        }
        if(scaled_spans) {
            const auto* emitted=reinterpret_cast<const starfox::render::RasterCommand*>(words+count*129);
            starfox::render::RenderSettings render_settings;render_settings.render_scale=custom_raster?1:scale;
            if(custom_raster) render_settings.focal_length=384;
            starfox::render::SoftwareRenderer renderer(render_settings);
            starfox::render::RenderPose pose;pose.x=.25;pose.y=-.125;pose.z=0;pose.vanish_x=pose.vanish_y=0;
            pose.use_rotation_matrix=true;pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};
            pose.subpixel_projection=true;pose.continuous_geometry=true;
            pose.texture_scroll_x=2;pose.texture_scroll_y=-1;
            for(unsigned i:{31U,32U}) {
                starfox::assets::Shape shape;starfox::assets::Face face;face.visibility_index=-1;face.normal={0,0,127};
                for(unsigned j=0;j<source[i].size();++j) {
                    const auto& p=source[i][j];shape.vertices.push_back({-p[0],-p[1],-256});
                    shape.word_coordinates.push_back(true);face.vertex_indices.push_back(std::uint8_t(j));
                }
                shape.faces.push_back(face);shape.colour_words={0x4000};shape.colour_materials={{0x4000,{}}};
                starfox::assets::TextureImage texture;texture.descriptor=0x4000;texture.u_mask=texture.v_mask=7;
                texture.texels=texels;texture.coordinates={{{0,0},{0,7},{7,7},{7,0}}};shape.textures.push_back(texture);
                pose.force_colour=i==32;pose.forced_colour=std::uint8_t(materials[i].even|(materials[i].odd<<4));
                starfox::render::Framebuffer cpu(custom_raster?raster_width:dimensions[0],custom_raster?raster_height:dimensions[1],custom_raster?1:scale),gpu(raster_width,raster_height);
                renderer.draw(shape,pose,cpu);
                starfox::render::RasterCommands batch;batch.reset(gpu.width(),gpu.height());batch.texels=texels;
                const auto rows=raster_height;
                batch.commands.assign(emitted+i*rows,emitted+(i+1)*rows);
                starfox::render::replay_raster_commands(batch,gpu,nullptr);
                if(cpu.pixels()!=gpu.pixels()) throw std::runtime_error("Fractional scaled span mismatch at scale "+std::to_string(scale));
                scaled_pixels_checked+=cpu.pixels().size();
            }
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);fractional_checked+=count;
    }
    std::cout<<"Fractional clipping: "<<fractional_checked<<" polygons, maximum XYUV error "<<maximum_error<<'\n';
    std::cout<<"Fractional spans 1x/2x/4x matched SoftwareRenderer across "<<scaled_pixels_checked<<" pixels\n";
    if(!near_corners_checked) throw std::runtime_error("Near-plane fixture produced no visible geometry");
    std::cout<<"Near-plane source order/projection matched "<<near_corners_checked<<" clipped corners; textured/fully-behind rejection passed\n";
    for(unsigned variant=0;variant<15;++variant) {
        std::array<starfox::render::ContinuousProjectedPoint,3> p{},tails{};
        const float tail=variant==0?-std::ldexp(1.f,-30):variant==1?std::ldexp(1.f,-30):0.f;
        for(unsigned i=0;i<3;++i) {
            p[i].camera[2]=256;p[i].camera[3]=1;
            p[i].screen[0]=float(16+i*16);p[i].screen[1]=i==1?100.f:72.125f;p[i].screen[2]=256;p[i].screen[3]=1;
            tails[i].camera[3]=1;tails[i].screen[0]=p[i].screen[0];tails[i].screen[1]=p[i].screen[1];tails[i].screen[3]=tail;
        }
        if(variant==3) tails[0].camera[3]=-1;
        if(variant>=7 && variant<=9) {
            for(unsigned i=0;i<3;++i) {
                tails[i].camera[3]=2;
                for(unsigned c=0;c<2;++c) {
                    auto bits=std::bit_cast<std::uint64_t>(double(p[i].screen[c]));
                    // A valid double payload may contain a NaN-looking low word.
                    if(variant==8)bits=(bits&0xffffffff00000000ULL)|0x7fc00000U;
                    if(variant==9 && i==0 && c==0)bits=0x7ff8000000000000ULL;
                    tails[i].screen[c*2]=std::bit_cast<float>(std::uint32_t(bits));
                    tails[i].screen[c*2+1]=std::bit_cast<float>(std::uint32_t(bits>>32));
                }
            }
        }
        if(variant==5) {
            const float xyz[3][3]{{-.125f,0,-1},{.125f,0,1},{0,.125f,1}};
            for(unsigned i=0;i<3;++i) for(unsigned c=0;c<3;++c) p[i].camera[c]=xyz[i][c];
            tails[0].camera[0]=1e-8f;
        }
        const std::array<V,3> indices{{{0,0,0,0},{1,0,0,0},{2,0,0,0}}};
        if(variant==6 || variant==11) {
            const double first=-224.0*std::sqrt(.5);
            for(unsigned i=0;i<2;++i) {
                const double camera[]{(i==0?first:0)+.25,(i==0?first:0)-.125,384};
                for(unsigned c=0;c<3;++c) {
                    p[i].camera[c]=float(camera[c]);tails[i].camera[c]=float(camera[c]-double(p[i].camera[c]));
                }
                for(unsigned c=0;c<2;++c) {
                    const double screen=(c==0?112.:96.)+camera[c]*256./384.;
                    p[i].screen[c]=float(screen);tails[i].screen[c]=float(screen);
                    tails[i].screen[c+2]=float(screen-double(float(screen)));
                }
            }
        }
        if(variant==10) {
            const float camera[3][3]{{-21,105,236},{-17.0001220703125f,89,256},{-56.9989013671875f,97,200}};
            for(unsigned i=0;i<3;++i) {
                for(unsigned c=0;c<3;++c)p[i].camera[c]=camera[i][c];
                p[i].screen[0]=float(112.+double(camera[i][0])*256./camera[i][2]);
                p[i].screen[1]=float(96.+double(camera[i][1])*256./camera[i][2]);
            }
        }
        if(variant>=12) {
            for(unsigned i=0;i<3;++i) {
                tails[i].camera[3]=3;
                double camera[]{double(p[i].screen[0])-112.,double(p[i].screen[1])-96.,256.};
                if(variant==14) {
                    const double nearCamera[3][3]{{-.125,0,-1},{.125,0,1},{0,-.125,1}};
                    for(unsigned c=0;c<3;++c)camera[c]=nearCamera[i][c];
                }
                for(unsigned c=0;c<3;++c) {
                    p[i].camera[c]=float(camera[c]);
                    auto bits=std::bit_cast<std::uint64_t>(camera[c]);
                    if(variant==13 && i==0 && c==0)bits=0x7ff8000000000000ULL;
                    tails[i].camera[c]=std::bit_cast<float>(std::uint32_t(bits));
                    tails[i].screen[c]=std::bit_cast<float>(std::uint32_t(bits>>32));
                }
            }
        }
        const V polygon{0,(variant==6 || variant==11)?2:3,0,(variant==6 || variant==11)?2:variant>=12?8:0};const Uint32 visible=1;
        data=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(r.device,r.upload,true));require(data);
        std::memcpy(data,p.data(),96);std::memcpy(data+96,tails.data(),96);
        std::memcpy(data+192,indices.data(),48);std::memcpy(data+240,&polygon,16);std::memcpy(data+256,&visible,4);
        const float parameters[]{112,96,256,variant>=10?1.f:0.f};std::memcpy(data+260,parameters,16);
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        command=SDL_AcquireGPUCommandBuffer(r.device);require(command);copy=SDL_BeginGPUCopyPass(command);require(copy);
        const Uint32 ids[]{4,0,1,2,3,5},offsets[]{0,96,192,240,256,260},lengths[]{96,96,48,16,4,16};
        for(unsigned i=0;i<6;++i) {
            SDL_GPUTransferBufferLocation from{r.upload,offsets[i]};SDL_GPUBufferRegion to{r.buffers[ids[i]],0,lengths[i]};
            SDL_UploadToGPUBuffer(copy,&from,&to,true);
        }
        SDL_EndGPUCopyPass(copy);
        const starfox::render::NativeClipSettings settings{1,3,3,1,224,192};
        auto* output=static_cast<SDL_GPUBuffer*>(clip.enqueue(r.device,command,r.buffers[4],r.buffers[1],r.buffers[2],r.buffers[3],settings,true,(variant==5 || variant>=10)?r.buffers[5]:nullptr,(variant==5 || variant>=10)?1:0,variant==10?nullptr:r.buffers[0],variant==10?0:(variant==4?2:3),small?4U:0U));
        require(output);copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion from{output,0,129*16};SDL_GPUTransferBufferLocation to{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* words=static_cast<const V*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(words);
        if(variant>=12) {
            if(variant==13) {
                if(words[0][0]!=0 || words[0][1]!=1)throw std::runtime_error("Invalid binary64 camera accepted");
            } else if(words[0][0]!=(variant==14?4:3) || words[0][1]!=0)throw std::runtime_error("Valid binary64 camera rejected");
            if(variant==14) {
                const float expected[4][2]{{112,96},{144,96},{112,64},{96,80}};
                for(unsigned i=0;i<4;++i)
                    if(std::bit_cast<float>(words[i+1][0])!=expected[i][0] || std::bit_cast<float>(words[i+1][1])!=expected[i][1])
                        throw std::runtime_error("Binary64 near camera intersection/order mismatch");
            }
        } else if(variant==10) {
            bool found=false;
            for(int i=1;i<=words[0][0];++i) {
                const float x=std::bit_cast<float>(words[i][0]),y=std::bit_cast<float>(words[i][1]);
                if(y==192 && std::abs(x-93.375f)<.001f) {
                    found=true;if(std::lround(double(x)*4)!=373)throw std::runtime_error("TREE intersection lost 4x boundary side");
                }
            }
            if(words[0][1]!=0 || !found)throw std::runtime_error("TREE boundary fixture missing intersection");
        } else if(variant>=7 && variant<=9) {
            if(variant==9) {
                if(words[0][0]!=0 || words[0][1]!=1)throw std::runtime_error("Invalid binary64 screen accepted");
            } else if(words[0][0]!=3 || words[0][1]!=0)throw std::runtime_error("Valid binary64 screen rejected");
        } else if(variant==6 || variant==11) {
            const float x=std::bit_cast<float>(words[1][0]);
            if(words[0][0]!=2 || words[0][1]!=0 || std::lround(x*2)!=33)
                throw std::runtime_error("Clipped destruction line lost 2x half-pixel boundary");
        } else if(variant==5) {
            if(words[0][0]!=4 || words[0][1]!=0) throw std::runtime_error("Residual near triangle rejected");
            if(!(std::bit_cast<float>(words[1][0])>112.f) || !(std::bit_cast<float>(words[4][0])>96.f))
                throw std::runtime_error("Near intersection lost camera residual");
        } else if(variant>=3) {
            if(words[0][0]!=0 || words[0][1]!=1) throw std::runtime_error("Invalid residual input accepted");
        } else {
            if(words[0][0]!=3 || words[0][1]!=0) throw std::runtime_error("Valid residual triangle rejected");
            const float y=std::bit_cast<float>(words[1][1]);
            if((variant==0 && !(y<72.125f)) || (variant==1 && !(y>72.125f)) || (variant==2 && y!=72.125f))
                throw std::runtime_error("Clipping lost residual rounding side");
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"Fifteen residual clipping boundary/invalid-input/near-intersection/line/binary64 fixtures passed\n";
    std::size_t word_near_cases=0,word_near_drawn=0;
    for(int back:{-32768,-257,-2,-1}) for(int front:{-1,0,1,2,256,32767}) {
        const std::array<starfox::render::NativeProjectionPoint,3> camera{{
            {-20,-10,back,0,112,96,0,0},{0,30,front,0,112,96,0,0},{20,-10,front,0,112,96,0,0}}};
        const std::array<V,3> indices{{{0,0,0,0},{1,0,0,0},{2,0,0,0}}};
        const V polygon{0,3,0,0};const Uint32 visible=1;
        data=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(r.device,r.upload,true));require(data);
        std::memcpy(data,camera.data(),96);std::memcpy(data+96,indices.data(),48);
        std::memcpy(data+144,&polygon,16);std::memcpy(data+160,&visible,4);
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        command=SDL_AcquireGPUCommandBuffer(r.device);require(command);copy=SDL_BeginGPUCopyPass(command);require(copy);
        const Uint32 buffer_ids[]{4,1,2,3},offsets[]{0,96,144,160},lengths[]{96,48,16,4};
        for(unsigned i=0;i<4;++i) {
            SDL_GPUTransferBufferLocation from{r.upload,offsets[i]};SDL_GPUBufferRegion to{r.buffers[buffer_ids[i]],0,lengths[i]};
            SDL_UploadToGPUBuffer(copy,&from,&to,true);
        }
        SDL_EndGPUCopyPass(copy);
        auto* projected=static_cast<SDL_GPUBuffer*>(projection.enqueue(r.device,command,r.buffers[4],3));
        if(!projected){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(projection.status());}
        starfox::render::NativeClipSettings settings{1,3,3,1,224,192};
        if(!clip.enqueue(r.device,command,projected,r.buffers[1],r.buffers[2],r.buffers[3],settings,false,r.buffers[4],3,nullptr,0,small?4U:0U)) {
            SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(clip.status());
        }
        auto* spans=clip.enqueue_spans(command,r.buffers[6]);
        if(!spans){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(clip.status());}
        const auto output=raster.enqueue_row_spans(r.device,command,spans,1,224,192);
        if(!output.pixels){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(raster.status());}
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,224*192*4};SDL_GPUTransferBufferLocation to{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        starfox::assets::Shape shape;starfox::assets::Face face;face.visibility_index=-1;face.normal={0,0,127};face.vertex_indices={0,1,2};
        shape.faces.push_back(face);shape.colour_words={0x3f11};shape.colour_materials={{0x3f11,{}}};
        for(const auto& p:camera) {shape.vertices.push_back({-p.x,-p.y,-p.z});shape.word_coordinates.push_back(true);}
        starfox::render::RenderPose pose;pose.z=0;pose.use_rotation_matrix=true;
        pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};pose.force_colour=true;pose.forced_colour=0x11;
        starfox::render::Framebuffer cpu(224,192);starfox::render::SoftwareRenderer renderer;
        renderer.draw(shape,pose,cpu);
        const auto* pixels=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(pixels);
        for(std::size_t i=0;i<cpu.pixels().size();++i) {
            if(std::uint8_t(pixels[i])!=cpu.pixels()[i])
                throw std::runtime_error("Word near-plane raster mismatch back="+std::to_string(back)+" front="+std::to_string(front));
            word_near_drawn+=cpu.pixels()[i]!=0;
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);++word_near_cases;
    }
    if(!word_near_drawn) throw std::runtime_error("Word near fixture produced no pixels");
    std::cout<<"Word near-plane GPU chain: "<<word_near_cases<<" SoftwareRenderer comparisons passed, "<<word_near_drawn<<" drawn pixels\n";
    raster.release_device();clip.release_device();projection.release_device();return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
