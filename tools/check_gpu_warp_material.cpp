#include <SDL3/SDL.h>
#include "starfox/render/face_material.hpp"
#include "starfox/render/software_renderer.hpp"
#include "../src/render/shaders/generated/warp_material_portable.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using U=Uint32;
void check(bool v){if(!v)throw std::runtime_error(SDL_GetError());}
int main()try {
    check(SDL_Init(SDL_INIT_VIDEO));auto* d=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,true,nullptr);check(d);
    SDL_GPUComputePipelineCreateInfo pi{};pi.code=starfox::render::warp_material_shader::spirv;
    pi.code_size=sizeof(starfox::render::warp_material_shader::spirv);pi.format=SDL_GPU_SHADERFORMAT_SPIRV;pi.entrypoint="main";
    pi.num_readonly_storage_buffers=6;pi.num_readwrite_storage_buffers=1;pi.num_uniform_buffers=1;
    pi.threadcount_x=64;pi.threadcount_y=pi.threadcount_z=1;auto* pipeline=SDL_CreateGPUComputePipeline(d,&pi);check(pipeline);
    constexpr U count=65536;
    const std::array<U,7> sizes{count*4,count*4,16,4*62*10,128,count*4,count*16};
    std::array<SDL_GPUBuffer*,7> buffers{};U uploadSize=0;
    for(unsigned i=0;i<7;++i){SDL_GPUBufferCreateInfo b{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,sizes[i],0};buffers[i]=SDL_CreateGPUBuffer(d,&b);check(buffers[i]);if(i<6)uploadSize+=sizes[i];}
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,uploadSize,0};auto* upload=SDL_CreateGPUTransferBuffer(d,&ti);check(upload);
    ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=sizes[6];auto* download=SDL_CreateGPUTransferBuffer(d,&ti);check(download);
    starfox::assets::Shape shape;starfox::assets::Face face;face.colour_id=255;
    starfox::assets::TextureImage texture;texture.descriptor=0x4567;shape.textures.push_back(texture);
    std::vector<U> descriptors(count),order(count),textures(count,0xffffffffU);textures[0x4567]=0;
    for(U i=0;i<count;++i)descriptors[i]=i;
    std::array<unsigned char,4*62*10> diffuse{};
    for(U b=0;b<4;++b)for(U m=0;m<12;++m)for(U s=0;s<10;++s){auto value=static_cast<unsigned char>(b*53+m*17+s*29);shape.diffuse_shade_tables[b][m][s]=value;diffuse[(b*62+m)*10+s]=value;}
    U compared=0;
    for(U run=0;run<32;++run){
        const U fixture=run%16;const bool axis=run>=16;
        starfox::render::RenderPose pose;pose.has_depth_colour_tables=(fixture&4)!=0;shape.has_diffuse_shade_tables=(fixture&8)!=0;
        for(U b=0;b<4;++b)for(U c=0;c<32;++c)pose.depth_colour_tables[b][c]=static_cast<unsigned char>(b*31+c*7);
        if(fixture==14)pose.palette_override=7;if(fixture==15){pose.palette_override=3;pose.force_colour=true;pose.forced_colour=0xab;}
        if(fixture==13){pose.force_colour=true;pose.forced_colour=0xd2;}
        face.normal={int(fixture)*17-127,63,-91};
        const std::array<std::int8_t,3> light{-93,47,111};
        const U base=fixture%2?250:128,band=fixture%5;
        const std::array<int,4> normal{face.normal.x,face.normal.y,face.normal.z,0};
        const U flags=(pose.palette_override?1U:0U)|(pose.force_colour?2U:0U)|(shape.has_diffuse_shade_tables?4U:0U)|(pose.has_depth_colour_tables?8U:0U)|(axis?16U:0U);
        const std::array<U,16> settings{count,1,band,flags,U(int(light[0])),U(int(light[1])),U(int(light[2])),0,12,12,12,12,base,pose.palette_override.value_or(0),pose.forced_colour,0};
        const void* data[]{descriptors.data(),order.data(),normal.data(),diffuse.data(),pose.depth_colour_tables.data(),textures.data()};
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(d,upload,false));check(mapped);U offset=0;
        for(U i=0;i<6;++i){std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}SDL_UnmapGPUTransferBuffer(d,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(d);check(command);auto* copy=SDL_BeginGPUCopyPass(command);check(copy);offset=0;
        for(U i=0;i<6;++i){SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{buffers[i],0,sizes[i]};SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=sizes[i];}SDL_EndGPUCopyPass(copy);
        SDL_PushGPUComputeUniformData(command,0,settings.data(),sizeof(settings));SDL_GPUStorageBufferReadWriteBinding output{};output.buffer=buffers[6];
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);check(pass);SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,buffers.data(),6);SDL_DispatchGPUCompute(pass,count/64,1,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(command);check(copy);SDL_GPUBufferRegion from{buffers[6],0,sizes[6]};SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence);check(SDL_WaitForGPUFences(d,true,&fence,1));SDL_ReleaseGPUFence(d,fence);
        auto* actual=static_cast<const U*>(SDL_MapGPUTransferBuffer(d,download,false));check(actual);
        for(U word=0;word<count;++word){const auto expected=starfox::render::face_material(shape,face,0,band,light,pose,std::uint16_t(word),std::uint8_t(base));const U textureIndex=expected.texture&&!axis?0U:0xffffffffU;
            if(actual[word*4]!=U(std::uint8_t(base+expected.colour.even)) || actual[word*4+1]!=U(std::uint8_t(base+expected.colour.odd)) || actual[word*4+2]!=U(expected.colour.dither) || actual[word*4+3]!=textureIndex)throw std::runtime_error("material mismatch fixture "+std::to_string(fixture)+" word "+std::to_string(word));++compared;}
        SDL_UnmapGPUTransferBuffer(d,download);
    }
    SDL_ReleaseGPUTransferBuffer(d,upload);SDL_ReleaseGPUTransferBuffer(d,download);for(auto* b:buffers)SDL_ReleaseGPUBuffer(d,b);SDL_ReleaseGPUComputePipeline(d,pipeline);SDL_DestroyGPUDevice(d);SDL_Quit();
    std::cout<<compared<<" GPU material results match face_material across every descriptor and 32 shading/override/axis fixtures\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
