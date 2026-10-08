// Fixture-only screen/geometry uploads and readbacks. The production component
// consumes native resident allocations and encodes into its caller's command.
#include "starfox/render/gpu_calibrated_volumetric.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using namespace starfox::render;
using Bytes=std::vector<unsigned char>;
unsigned long long checks{},responses{};float worst{};
void check(bool ok,const char* message) {++checks;if(!ok) throw std::runtime_error(message);}
bool bgra(SDL_GPUTextureFormat f) {return f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM||f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
bool srgb(SDL_GPUTextureFormat f) {return f==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB||f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUCommandBuffer* command{};
    ~Gpu(){if(device){if(command)SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);SDL_DestroyGPUDevice(device);}SDL_Quit();}
    void begin(){check(!command,"Command already open");command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());}
    void submit(){bool ok=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(ok,SDL_GetError());check(SDL_WaitForGPUIdle(device),SDL_GetError());}
};
struct Buffer {
    Gpu& gpu;SDL_GPUBuffer* buffer{};unsigned bytes;
    Buffer(Gpu& g,unsigned n):gpu(g),bytes(n){const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,n,0};buffer=SDL_CreateGPUBuffer(g.device,&info);check(buffer,SDL_GetError());}
    ~Buffer(){SDL_ReleaseGPUBuffer(gpu.device,buffer);}
    void upload(const void* bytes){const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,this->bytes,0};auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());std::memcpy(mapped,bytes,this->bytes);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());const SDL_GPUTransferBufferLocation from{transfer,0};const SDL_GPUBufferRegion to{buffer,0,this->bytes};SDL_UploadToGPUBuffer(pass,&from,&to,true);SDL_EndGPUCopyPass(pass);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);}
};
struct Texture {
    Gpu& gpu;SDL_GPUTexture* texture{};unsigned w,h,stride;SDL_GPUTextureFormat format;
    Texture(Gpu& g,unsigned width,unsigned height,SDL_GPUTextureFormat f):gpu(g),w(width),h(height),stride(f==SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT?16:4),format(f){SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=f;info.width=w;info.height=h;info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;texture=SDL_CreateGPUTexture(g.device,&info);check(texture,SDL_GetError());}
    ~Texture(){SDL_ReleaseGPUTexture(gpu.device,texture);}
    void upload(Bytes bytes){check(bytes.size()==w*h*stride,"Wrong texture upload size");if(bgra(format))for(unsigned i=0;i<bytes.size();i+=4)std::swap(bytes[i],bytes[i+2]);const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,unsigned(bytes.size()),0};auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());std::memcpy(mapped,bytes.data(),bytes.size());SDL_UnmapGPUTransferBuffer(gpu.device,transfer);auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());SDL_GPUTextureTransferInfo from{};from.transfer_buffer=transfer;from.pixels_per_row=w;from.rows_per_layer=h;const SDL_GPUTextureRegion to{texture,0,0,0,0,0,w,h,1};SDL_UploadToGPUTexture(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);}
    Bytes read(){gpu.begin();const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*stride,0};auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());const SDL_GPUTextureRegion from{texture,0,0,0,0,0,w,h,1};SDL_GPUTextureTransferInfo to{};to.transfer_buffer=transfer;to.pixels_per_row=w;to.rows_per_layer=h;SDL_DownloadFromGPUTexture(pass,&from,&to);SDL_EndGPUCopyPass(pass);gpu.submit();auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(mapped,SDL_GetError());Bytes bytes(mapped,mapped+w*h*stride);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);if(bgra(format))for(unsigned i=0;i<bytes.size();i+=4)std::swap(bytes[i],bytes[i+2]);return bytes;}
};
struct IntegralRead {
    Gpu& gpu;SDL_GPUTransferBuffer* transfer{};unsigned count;
    IntegralRead(Gpu& g,unsigned n):gpu(g),count(n){const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,count*16,0};transfer=SDL_CreateGPUTransferBuffer(g.device,&info);check(transfer,SDL_GetError());}
    ~IntegralRead(){SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);}
    void capture(void* volume){auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(volume),0,count*16};const SDL_GPUTransferBufferLocation to{transfer,0};SDL_DownloadFromGPUBuffer(pass,&from,&to);SDL_EndGPUCopyPass(pass);}
    std::vector<std::array<float,4>> read(){auto* mapped=static_cast<std::array<float,4>*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(mapped,SDL_GetError());std::vector<std::array<float,4>> result(mapped,mapped+count);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);return result;}
};
float decode(float v){return v<=.04045F?v/12.92F:std::pow((v+.055F)/1.055F,2.4F);}
float encode(float v){return v<=.0031308F?v*12.92F:1.055F*std::pow(v,1.F/2.4F)-.055F;}
void run(Gpu& gpu,SDL_GPUTextureFormat format,unsigned w,unsigned h){
    Texture source(gpu,w,h,format),ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),left(gpu,w,h,format),right(gpu,w,h,format);
    Bytes colors(w*h*4),tags(w*h*4),depth(w*h*16);std::vector<std::array<float,4>> guides(w*h);
    for(unsigned i=0;i<w*h;++i){colors[i*4]=32+i*13%160;colors[i*4+1]=45+i*7%180;colors[i*4+2]=70+i*3%140;colors[i*4+3]=i%13?255:0;tags[i*4+2]=i%7?std::uint8_t(i%2+1):0;guides[i]={0,0,-1,i%3?float(2+i%93):0};}
    std::memcpy(depth.data(),guides.data(),depth.size());gpu.begin();source.upload(colors);ownership.upload(tags);surface.upload(depth);gpu.submit();
    GpuCalibratedVolumetric fog;check(fog.initialize(gpu.device,format),fog.status().c_str());IntegralRead read(gpu,w*h);
    for(unsigned kind=0;kind<6;++kind)for(unsigned eye=0;eye<2;++eye){
        using shadows::Vec3;using shadows::Triangle;std::vector<Triangle> triangles;
        if(kind){triangles.push_back({{-2,-2,5},{2,-2,5},{0,2,5}});triangles.push_back({{-4,-5,-100},{4,-5,-100},{4,-5,100}});triangles.push_back({{-4,-5,-100},{4,-5,100},{-4,-5,100}});}
        // Exercise multiple hierarchy levels and inactive/degenerate padding.
        if(kind==5)for(unsigned i=0;i<130;++i)triangles.push_back({{double(i+80),0,30},{double(i+81),0,30},{double(i+80),1,30}});
        const float eye_offset=eye?.7F:-.7F;
        for(auto& t:triangles){t.a.x-=eye_offset;t.b.x-=eye_offset;t.c.x-=eye_offset;}
        shadows::Scene reference;
        for(unsigned i=0;i<triangles.size();++i){if((kind==2||kind==3||kind==4)&&(i==1||i==2))continue;reference.add(triangles[i]);}
        if(kind==2){reference.add({{-4-eye_offset,-5,-100},{-eye_offset,-5,-100},{-eye_offset,-5,100}});reference.add({{-4-eye_offset,-5,-100},{-eye_offset,-5,100},{-4-eye_offset,-5,100}});}
        reference.build();
        const unsigned n=unsigned(triangles.size()),material_offset=n*48,material_bytes=n*64+1028;
        Bytes payload(std::max(16U,material_offset+material_bytes));
        for(unsigned i=0;i<n;++i){const auto& t=triangles[i];const std::array<float,4> vertices[]{{float(t.a.x),float(t.a.y),float(t.a.z),1},{float(t.b.x),float(t.b.y),float(t.b.z),1},{float(t.c.x),float(t.c.y),float(t.c.z),1}};std::memcpy(payload.data()+i*48,vertices,sizeof(vertices));std::uint32_t words[16]{};words[8]=words[9]=0xffabcdef;words[15]=2;
            if((kind==2||kind==3||kind==4)&&(i==1||i==2)){float uv[6]{0,0,2,0,2,0};if(i==2){uv[2]=2;uv[4]=0;}if(kind==3)for(unsigned c=0;c<6;++c)uv[c]=0;std::memcpy(words,uv,sizeof(uv));words[6]=1;words[7]=kind==3?536870913U:kind==4?5U:1U;if(kind==4){float outside[6]{-1,0,-1,0,-1,0};std::memcpy(words,outside,sizeof(outside));}words[12]=n*64;words[13]=kind==2?1:0;words[14]=0;words[15]=3;}
            std::memcpy(payload.data()+material_offset+i*64,words,sizeof(words));}
        const std::uint32_t opaque=0xffffffff;std::memcpy(payload.data()+material_offset+n*64,&opaque,4);
        // Indexed mode's sole texel is palette index 1 with transparent alpha.
        if(kind==3){const std::uint32_t index=1;std::memcpy(payload.data()+material_offset+n*64+1024,&index,4);}
        Buffer geometry(gpu,unsigned(payload.size()));gpu.begin();geometry.upload(payload.data());gpu.submit();
        CalibratedRayGeometryOutput output;output.device=gpu.device;output.buffer=n?geometry.buffer:nullptr;output.vertex_count=n*3;output.complete=true;output.width=w;output.height=h;output.projection={w*.61,h*.73,w*(eye?.43:.57)+.375,h*.52-.125};output.material_offset=material_offset;output.material_bytes=n?material_bytes:0;
        for(unsigned mode=0;mode<7;++mode){
            auto medium=volumetric_fog_medium(std::min(mode,3U));medium.maximum_distance=100;
            if(mode>3)medium.extinction=.015;
            medium.samples=std::array<unsigned,7>{1,8,16,32,1,64,128}[mode];
            medium.anisotropy=mode==4?-.4:.35;if(mode==5)medium.sunlight={};const Vec3 light{-.2,-1,.1};
            gpu.begin();void* volume=reinterpret_cast<void*>(1);check(fog.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,eye?right.texture:left.texture,output,medium,light,&volume),fog.status().c_str());if(mode) {check(volume,"Missing fog integral");read.capture(volume);}else check(!volume,"OFF retained stale integral");gpu.submit();const auto integral=mode?read.read():std::vector<std::array<float,4>>{};auto actual=(eye?right:left).read();
            for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned at=y*w+x;std::array<double,4> expected{0,0,0,1};if(tags[at*4+2]&&mode){const Vec3 ray{(x+.5-output.projection[2])/output.projection[0],(y+.5-output.projection[3])/output.projection[1],1};const double distance=guides[at][3]>0?std::min(medium.maximum_distance,guides[at][3]*std::sqrt(shadows::dot(ray,ray))):medium.maximum_distance;const auto value=integrate_volumetric_fog(medium,{},ray,distance,light,reference);check(bool(value),"CPU fog reference invalid");expected={value->scattering.x,value->scattering.y,value->scattering.z,value->transmittance};}
                for(unsigned c=0;c<4;++c){if(mode){const float error=float(std::abs(integral[at][c]-expected[c]));worst=std::max(worst,error);if(!std::isfinite(integral[at][c])||error>1.e-4){std::cerr<<"Fog integral kind="<<kind<<" eye="<<eye<<" mode="<<mode<<" pixel="<<at<<" channel="<<c<<" got="<<integral[at][c]<<" expected="<<expected[c]<<'\n';check(false,"Native fog differs from double-precision geometry/light integral");}++checks;}
                    int target=colors[at*4+c];if(mode&&tags[at*4+2]&&colors[at*4+3]&&c<3){const double linear=decode(colors[at*4+c]/255.F)*expected[3]+expected[c];target=int(std::lround(encode(float(std::clamp(linear,0.,1.)))*255));}const unsigned tolerance=mode&&tags[at*4+2]&&colors[at*4+3]&&c<3?(srgb(format)?2:1):0;check(unsigned(std::abs(int(actual[at*4+c])-target))<=tolerance,"Native fog colour/HUD/alpha/OFF mismatch");if(actual[at*4+c]!=colors[at*4+c])++responses;}
            }
        }
        // Multiple eyes in one unsubmitted command must keep distinct backing.
        VolumetricMedium medium;medium.extinction=.015;medium.maximum_distance=100;gpu.begin();void* first{},*second{};check(fog.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,output,medium,{0,-1,0},&first),fog.status().c_str());read.capture(first);auto changed=output;changed.projection[2]+=2;check(fog.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,right.texture,changed,medium,{0,-1,0},&second),fog.status().c_str());gpu.submit();auto before=left.read(),after=right.read();check(kind==0||before!=after,"Eye/sample projection changes produced identical fog");
        gpu.begin();check(fog.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,output,medium,{0,-1,0}),fog.status().c_str());SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;gpu.begin();check(fog.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,changed,medium,{0,-1,0}),fog.status().c_str());gpu.submit();check(left.read()==after,"Cancelled native fog contaminated retry");
        for(unsigned fault=0;fault<10;++fault){auto broken=output;auto bad=medium;switch(fault){case 0:broken.complete=false;break;case 1:broken.vertex_count=1;break;case 2:broken.width=16385;break;case 3:broken.projection[0]=0;break;case 4:bad.anisotropy=1;break;case 5:bad.extinction=-1;break;case 6:bad.samples=129;break;case 7:broken.material_offset=1;broken.material_bytes=64;break;case 8:bad.maximum_distance=std::numeric_limits<double>::infinity();break;case 9:broken.projection[1]=1.e-300;break;}gpu.begin();void* stale=reinterpret_cast<void*>(1);check(!fog.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,broken,bad,{0,-1,0},&stale)&&!stale,"Invalid fog input retained stale output");SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;}
    }
    std::cout<<"Native volumetric format="<<unsigned(format)<<" extent="<<w<<'x'<<h<<": actual eye projection, binary/indexed/clamped cutouts, hierarchy levels, all qualities/custom densities and 1/8/16/32/64/128 samples, protected ink/alpha/OFF, retained eyes and cancellation passed.\n";
}
}
int main(int argc,char** argv)try{const char* backend=argc>1?argv[1]:"direct3d12";Gpu gpu;check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());gpu.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,true,backend);check(gpu.device,SDL_GetError());for(auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}){run(gpu,format,19,13);run(gpu,format,37,21);}check(responses>100,"Fog produced no observable image response");std::cout<<"Native volumetric component "<<backend<<": "<<checks<<" checks, "<<responses<<" changed bytes, max integral error="<<worst<<". Game owner/frontend/physical Leia acceptance remain separate.\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
