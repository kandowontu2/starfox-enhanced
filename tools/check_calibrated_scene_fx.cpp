// Fixture-only uploads/readbacks. Production consumes raster-owned native
// images and never submits, waits, downloads or uploads CPU screen pixels.
#include "starfox/render/gpu_calibrated_scene_fx.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace {
using namespace starfox;
using Bytes=std::vector<unsigned char>;
unsigned long long checks{},responses{};
void check(bool ok,const char* error) {++checks;if(!ok) throw std::runtime_error(error);}
constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
constexpr vr::Matrix4 source_rig{1.F/256,0,0,0,0,-1.F/256,0,0,0,0,-1.F/256,0,0,0,0,1};
bool bgra(SDL_GPUTextureFormat f) {return f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
bool srgb(SDL_GPUTextureFormat f) {return f==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || f==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;}
struct Gpu {
    SDL_GPUDevice* device{};SDL_GPUCommandBuffer* command{};
    ~Gpu() {if(device) {if(command) SDL_CancelGPUCommandBuffer(command);SDL_WaitForGPUIdle(device);SDL_DestroyGPUDevice(device);}SDL_Quit();}
    void begin() {check(!command,"Fixture command already open");command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());}
    void submit() {const bool ok=SDL_SubmitGPUCommandBuffer(command);command=nullptr;check(ok,SDL_GetError());}
};
struct ProjectReadback {
    Gpu& gpu;SDL_GPUTransferBuffer* download{};
    unsigned rows;
    explicit ProjectReadback(Gpu& g,bool particles=false):gpu(g),rows(particles?194:146) {
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,rows*16,0};
        download=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(download,SDL_GetError());
    }
    ~ProjectReadback() {SDL_ReleaseGPUTransferBuffer(gpu.device,download);}
    void capture(void* points) {
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(points),0,rows*16};
        const SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    }
    void compare(const render::SceneFxFrame& expected,float ratio,bool particles=false) {
        check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
        auto* mapped=static_cast<const std::array<float,4>*>(SDL_MapGPUTransferBuffer(gpu.device,download,false));check(mapped,SDL_GetError());
        std::array<std::array<float,4>,194> actual{};std::memcpy(actual.data(),mapped,rows*16);SDL_UnmapGPUTransferBuffer(gpu.device,download);
        for(unsigned row=0;row<2+unsigned(expected.camera[3])*3;++row) for(unsigned c=0;c<4;++c) {
            const float reference=row==0?expected.camera[c]:row==1?(c==0?ratio:0):expected.data[row-2][c];
            if(std::abs(actual[row][c]-reference)>1.e-3F) {
                std::cerr<<"Projected point row="<<row<<" channel="<<c<<" actual="<<actual[row][c]<<" expected="<<reference<<'\n';
                check(false,"GPU source wrapping/eye projection differs from independent reference");
            }
            ++checks;
        }
        if(particles) for(unsigned n=0;n<unsigned(expected.camera[3]);++n) for(unsigned c=0;c<4;++c) {
            const auto& before=expected.motion_previous[n];
            const float value=before.valid?(c==3?1:before.previous[c]):0;
            check(std::abs(actual[146+n][c]-value)<1.e-3F,"Native particle prior identity/eye projection differs from scalar reference");
        }
    }
};
struct Texture {
    Gpu& gpu;SDL_GPUTexture* texture{};unsigned width,height,stride;SDL_GPUTextureFormat format;
    Texture(Gpu& g,unsigned w,unsigned h,SDL_GPUTextureFormat f):gpu(g),width(w),height(h),stride(f==SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT?16:4),format(f) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=f;info.width=w;info.height=h;
        info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        texture=SDL_CreateGPUTexture(gpu.device,&info);check(texture,SDL_GetError());
    }
    ~Texture() {if(texture) SDL_ReleaseGPUTexture(gpu.device,texture);}
    Texture(const Texture&)=delete;
    void upload(Bytes bytes) {
        check(gpu.command && bytes.size()==width*height*stride,"Bad fixture upload");
        if(bgra(format)) for(unsigned i=0;i<bytes.size();i+=4) std::swap(bytes[i],bytes[i+2]);
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,unsigned(bytes.size()),0};
        auto* upload=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(upload,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,upload,false);check(mapped,SDL_GetError());
        std::memcpy(mapped,bytes.data(),bytes.size());SDL_UnmapGPUTransferBuffer(gpu.device,upload);
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        SDL_GPUTextureTransferInfo from{};from.transfer_buffer=upload;from.pixels_per_row=width;from.rows_per_layer=height;
        const SDL_GPUTextureRegion to{texture,0,0,0,0,0,width,height,1};SDL_UploadToGPUTexture(copy,&from,&to,false);
        SDL_EndGPUCopyPass(copy);SDL_ReleaseGPUTransferBuffer(gpu.device,upload);
    }
    Bytes read() {
        check(!gpu.command,"Readback would interfere with unsubmitted scene");gpu.begin();
        const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*stride,0};
        auto* download=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(download,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        const SDL_GPUTextureRegion from{texture,0,0,0,0,0,width,height,1};
        SDL_GPUTextureTransferInfo to{};to.transfer_buffer=download;to.pixels_per_row=width;to.rows_per_layer=height;
        SDL_DownloadFromGPUTexture(copy,&from,&to);SDL_EndGPUCopyPass(copy);gpu.submit();check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
        const auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,download,false));check(mapped,SDL_GetError());
        Bytes bytes(mapped,mapped+width*height*stride);SDL_UnmapGPUTransferBuffer(gpu.device,download);SDL_ReleaseGPUTransferBuffer(gpu.device,download);
        if(bgra(format)) for(unsigned i=0;i<bytes.size();i+=4) std::swap(bytes[i],bytes[i+2]);return bytes;
    }
};
// Independent double-precision source wrapping/matrix/perspective reference.
// It never calls the flat projection helper or native shader packing helpers.
render::SceneFxFrame project(const render::SceneFxWorldFrame& world,const vr::EyeCamera& eye,
    const vr::Matrix4& rig,std::array<double,3> origin,unsigned w,unsigned h,unsigned scale) {
    render::SceneFxFrame result;const auto& p=eye.projection;
    const float cx=w*(1-p[8])*.5F-.5F,cy=h*(1+p[9])*.5F-.5F,focal=w*p[0]*.5F;
    result.camera={cx,cy,focal,0};
    const auto transform=[&](std::array<double,3> position) {
        std::array<double,4> delta{},rigged{},transformed{};delta[3]=1;
        for(unsigned axis=0;axis<3;++axis) {
            delta[axis]=std::fmod(position[axis]-origin[axis],65536.);
            if(delta[axis]>32767) delta[axis]-=65536;
            else if(delta[axis]<-32768) delta[axis]+=65536;
        }
        for(unsigned row=0;row<4;++row) for(unsigned col=0;col<4;++col) rigged[row]+=rig[col*4+row]*delta[col];
        for(unsigned row=0;row<4;++row) for(unsigned col=0;col<4;++col) transformed[row]+=eye.view[col*4+row]*rigged[col];
        return std::array<float,3>{float(transformed[0]*256),float(-transformed[1]*256),float(-transformed[2]*256)};
    };
    const float focus=world.player?transform(*world.player)[2]:0;
    for(unsigned n=0;n<world.count;++n) {
        const auto& point=world.points[n];const auto position=transform(point.position);
        const float z=position[2];if(z<32 || z>10000 || (point.type==8 && z<std::max(32.F,focus*.6F))) continue;
        const float radius=std::clamp(focal*point.radius/z,.5F*scale,(point.type==8?30.F:180.F)*scale);
        auto extra=point.extra;
        if(point.type==1) extra={position[0],position[1],z,point.radius};
        if(point.type==8) extra={focus,std::max(64.F,focus*.5F),0,0};
        result.add({cx+focal*position[0]/z,cy+focal*position[1]/z,radius,point.strength},{point.type,z,point.age,0},extra,point.identity);
    }
    return result;
}
Bytes oracle(const Bytes& colors,const Bytes& tags,const std::vector<std::array<float,4>>& surfaces,
    const render::SceneFxFrame& points,const vr::EyeCamera& eye,unsigned w,unsigned h,unsigned scale) {
    auto expected=colors;const float ratio=float(h)*eye.projection[5]/(w*eye.projection[0]),cy=points.camera[1];
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const unsigned i=y*w+x;if(tags[i*4+2]==0 || !points.active()) continue;
        const auto& s=surfaces[i];
        for(unsigned c=0;c<3;++c) {
            const auto sample=[&](float sx,float sy,float focus=0,float range=0,float plume=0) {
                sy=cy+(sy-cy)*ratio;sx=std::clamp(sx,0.F,float(w-1));sy=std::clamp(sy,0.F,float(h-1));
                unsigned ax=unsigned(sx),ay=unsigned(sy);const float tx=sx-ax,ty=sy-ay;float value=0;
                for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                    unsigned at=std::min(ay+dy,h-1)*w+std::min(ax+dx,w-1);
                    const float depth=surfaces[at][3];
                    if(tags[at*4+2]==0 || (range>0 && depth>0 && (depth+8<plume || std::abs(depth-focus)<range))) at=i;
                    value+=colors[at*4+c]*(dx?tx:1-tx)*(dy?ty:1-ty);
                }
                return value/255.F;
            };
            expected[i*4+c]=static_cast<unsigned char>(render::scene_channel(sample,points,float(x),cy+(float(y)-cy)/ratio,c,float(scale),s[3],s[0],s[1],s[2])*255.F+.5F);
        }
    }
    return expected;
}
void compare(const Bytes& actual,const Bytes& expected,const Bytes& original,const Bytes& tags,unsigned tolerance) {
    check(actual.size()==expected.size(),"Native FX extent changed");
    for(unsigned i=0;i<actual.size();++i) {
        const unsigned limit=i%4==3 || tags[(i/4)*4+2]==0?0:tolerance;
        if(unsigned(std::abs(int(actual[i])-int(expected[i])))>limit) {
            std::cerr<<"Scene FX byte "<<i<<" actual="<<unsigned(actual[i])<<" expected="<<unsigned(expected[i])<<" tolerance="<<limit<<'\n';
            check(false,"Native FX differs from independent source/eye projection and shared CPU radiance equations");
        }
        ++checks;if(i%4!=3 && actual[i]!=original[i]) ++responses;
    }
}
void run(Gpu& gpu,SDL_GPUTextureFormat format,unsigned w,unsigned h) {
    Texture source(gpu,w,h,format),ownership(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),left(gpu,w,h,format),right(gpu,w,h,format);
    render::GpuCalibratedSceneFx fx;check(fx.initialize(gpu.device,format),fx.status().c_str());
    Bytes colors(w*h*4),tags(w*h*4),surface_bytes(w*h*16);std::vector<std::array<float,4>> surfaces(w*h);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        unsigned i=y*w+x;
        for(unsigned c=0;c<3;++c) colors[i*4+c]=static_cast<unsigned char>(20+(x*17+y*29+c*53)%131);
        colors[i*4+3]=static_cast<unsigned char>((x*43+y*71)%256);
        tags[i*4+2]=x<3 || x>w-4 || (x>24 && x<33 && y>12 && y<21)?0:y<10?1:2;
        surfaces[i]={0,0,-1,y<12 || tags[i*4+2]==0?0.F:x%7==0?150.F:x%4?900.F:420.F};
    }
    std::memcpy(surface_bytes.data(),surfaces.data(),surface_bytes.size());
    gpu.begin();source.upload(colors);ownership.upload(tags);surface.upload(surface_bytes);gpu.submit();
    vr::EyeCamera eye{identity,{1.4F,0,0,0,0,1.7F,0,0,.12F,-.16F,-1, -1,0,0,-.1F,0},{}};
    for(unsigned mode=0;mode<21;++mode) {
        std::cout<<"Checking native scene FX format="<<unsigned(format)<<" size="<<w<<'x'<<h<<" case="<<mode<<std::endl;
        render::SceneFxWorldFrame world;std::array<double,3> origin{};auto rig=source_rig;
        if(mode==12 || mode==13) origin={999999000.375,-999999020.625,999999010.875};
        const auto put=[&](std::array<double,3> p,float radius,float strength,float type,float age,std::array<float,4> extra) {
            for(unsigned a=0;a<3;++a) p[a]+=origin[a];world.add(p,radius,strength,type,age,extra,{int(type),1,0,0});
        };
        if(mode<9) {put({-20,-30,500},mode==1?600:mode==0?220:mode==8?130:80,.8F,float(mode),.18F,{.8F,.6F,.25F,0});}
        else if(mode==19) {
            for(const auto p:{std::array<double,3>{32767.25,0,600},{0,32767.5,600},{0,-32768.5,600}})
                put(p,45,.7F,3,0,{.3F,.7F,1,0});
        } else if(mode==20) {
            for(double z:{32767.5,-32768.5}) put({0,0,z},45,.7F,3,0,{.3F,.7F,1,0});
        } else if(mode==16) {
            for(double z:{31.9,32.,10000.,10000.1}) put({0,0,z},260,.7F,1,0,{});
        } else if(mode==17) {
            for(double z:{31.,179.,180.,10001.}) put({0,0,z},120,.8F,8,.2F,{});
        } else if(mode==15) {
            for(unsigned n=0;n<84;++n) put({0,0,-10},40,.3F,6,.1F,{1,.2F,.1F,0});
            for(unsigned n=0;n<12;++n) put({double(int(n%6)*38-95),double(int(n/6)*100-50),600.},45,.7F,3,.1F,{.3F,.7F,1,0});
        } else if(mode!=9) {
            for(unsigned n=0;n<34;++n) put({0,0,mode==10?-10.:500.},40,.3F,6,.1F,{1,.2F,.1F,0});
            for(unsigned n=0;n<27;++n) put({double(int(n%9)*38-152),double(int(n/9)*100-100),600.},45,.7F,3,.1F,{.3F,.7F,1,0});
            put({20,0,800},130,.9F,8,.1F,{});
        }
        world.player=std::array<double,3>{origin[0],origin[1],origin[2]+300};
        const unsigned scale=mode==11?6:mode==14?10:mode==13?2:1;
        if(mode==14) {rig[0]*=.8F;rig[8]=.2F/256;rig[2]=-.2F/256;rig[10]*=.8F;}
        if(mode==18) {
            const float c=std::cos(.17F),s=std::sin(.17F);eye.view[0]=eye.view[10]=c;eye.view[2]=-s;eye.view[8]=s;
            eye.view[13]=.11F;
        }
        if(mode>=19) eye.view=identity;
        if(mode==20) rig[10]*=.25F;
        auto other=eye;other.view[12]=.27F;other.view[13]=-.15F;other.projection[8]=-.07F;other.projection[9]=.23F;
        // Both eyes are encoded BEFORE submitting. Later buffer cycles must
        // not replace the first eye's projected points/gather focal ratio.
        gpu.begin();
        void* projected{};ProjectReadback left_points(gpu),right_points(gpu);
        check(fx.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,w,h,scale,world,eye,rig,origin,&projected),fx.status().c_str());left_points.capture(projected);
        check(fx.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,right.texture,w,h,scale,world,other,rig,origin,&projected),fx.status().c_str());right_points.capture(projected);
        gpu.submit();
        left_points.compare(project(world,eye,rig,origin,w,h,scale),float(h)*eye.projection[5]/(w*eye.projection[0]));
        right_points.compare(project(world,other,rig,origin,w,h,scale),float(h)*other.projection[5]/(w*other.projection[0]));
        const auto expected_left=oracle(colors,tags,surfaces,project(world,eye,rig,origin,w,h,scale),eye,w,h,scale);
        const auto expected_right=oracle(colors,tags,surfaces,project(world,other,rig,origin,w,h,scale),other,w,h,scale);
        compare(left.read(),expected_left,colors,tags,srgb(format)?2:1);
        compare(right.read(),expected_right,colors,tags,srgb(format)?2:1);
        if(mode<9) check(expected_left!=colors,"A requested effect had no observable response in its fixture");
        if(mode==9) check(left.read()==colors && right.read()==colors,"OFF altered original pixels/alpha");
        if(mode==10) check(project(world,eye,rig,origin,w,h,scale).camera[3]==28,"Near clipping happened after visible-cap compaction");
        if(mode==15) check(world.count==96 && project(world,eye,rig,origin,w,h,scale).camera[3]==12,"Full raw capacity/second portable uniform chunk was truncated");
        if(mode==16) check(project(world,eye,rig,origin,w,h,scale).camera[3]==2,"Native near/far boundary policy changed");
        if(mode==17) check(project(world,eye,rig,origin,w,h,scale).camera[3]==1,"Player-depth exhaust clipping changed");
        if(mode==20) check(project(world,eye,rig,origin,w,h,scale).camera[3]==1,"Fractional signed-word wrapping changed");
        // Cancellation cannot leave a cached screen/projection for the retry.
        if(mode==12) {
            gpu.begin();check(fx.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,w,h,scale,world,eye,rig,origin),fx.status().c_str());
            SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
            gpu.begin();check(fx.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,w,h,scale,world,other,rig,origin),fx.status().c_str());gpu.submit();
            compare(left.read(),expected_right,colors,tags,srgb(format)?2:1);
        }
    }
    // Independent previous-source matching, NOT previous compacted indices.
    // Current and previous eyes have different wrapping, asymmetric FOVs,
    // clipping, ordering, population caps and rejected/reused birth identities.
    for(unsigned variant=0;variant<7;++variant) {
        render::SceneFxWorldFrame world,prior,selected;const std::array<double,3> origin{1.e8,123,-1.e8};
        for(unsigned n=0;n<90;++n) {
            const float type=n%9;const std::array<std::int64_t,4> id{int(type),std::int64_t(n+1),7,1};
            auto position=origin;position[0]+=int(n%13)*7-40;position[1]+=int(n%5)*5-10;position[2]+=n<8?-100.:400.+n*9;
            world.add(position,9,.7F,type,.05F,{.8F,.3F,.1F,0},id);
        }
        for(unsigned n=world.count;n-->0;) {
            auto point=world.points[n];point.position[0]-=11;point.position[1]+=4;point.position[2]+=30;
            prior.points[prior.count++]=point;
        }
        if(variant==1) prior.points[12].identity={};
        if(variant==2) prior.points[12].identity=prior.points[13].identity;
        if(variant==3) world.points[20].identity=world.points[21].identity;
        if(variant==4) prior.points[12].type=1;
        if(variant==5) prior.points[12].position[2]=origin[2]-100;
        for(unsigned n=0;n<world.count;++n) if(world.points[n].type>=2 && world.points[n].type<=7)
            selected.points[selected.count++]=world.points[n];
        auto old_eye=eye;old_eye.view[12]+=.08F;old_eye.view[13]-=.03F;old_eye.projection[8]+=.04F;
        auto expected=project(selected,eye,source_rig,origin,w,h,1);
        auto old_origin=origin;old_origin[0]-=3;old_origin[2]+=5;
        for(unsigned n=0;n<unsigned(expected.camera[3]);++n) {
            const auto id=expected.motion_points[n].identity;
            const auto same=[&](const auto& p){return p.identity==id;};
            if(variant==6 || std::count_if(world.points.begin(),world.points.begin()+world.count,same)!=1
                || std::count_if(prior.points.begin(),prior.points.begin()+prior.count,same)!=1) continue;
            const auto found=std::find_if(prior.points.begin(),prior.points.begin()+prior.count,same);
            if(found->type!=expected.data[n*3+1][0]) continue;
            render::SceneFxWorldFrame one;one.points[one.count++]=*found;
            const auto old=project(one,old_eye,source_rig,old_origin,w,h,1);
            if(!old.active()) continue;
            const float ratio=float(h)*old_eye.projection[5]/(w*old_eye.projection[0]);
            expected.motion_previous[n].valid=true;
            expected.motion_previous[n].previous={old.data[0][0],old.camera[1]+(old.data[0][1]-old.camera[1])*ratio,old.data[1][1]};
        }
        const render::CalibratedSceneFxPrevious before{&prior,old_eye,source_rig,old_origin};
        ProjectReadback readback(gpu,true);gpu.begin();void* points{};
        const bool ok=fx.project_particles(gpu.command,w,h,1,world,eye,source_rig,origin,variant==6?nullptr:&before,points);
        check(ok,fx.status().c_str());
        check(points,"Native particle projection lost its resident buffer");readback.capture(points);gpu.submit();
        readback.compare(expected,float(h)*eye.projection[5]/(w*eye.projection[0]),true);
    }
    render::SceneFxWorldFrame empty;
    for(unsigned fault=0;fault<13;++fault) {
        auto world=empty;auto broken=eye;auto rig=source_rig;std::array<double,3> origin{};unsigned width=w,scale=1;
        switch(fault) {
        case 0:world.count=97;break;
        case 1:world.add({0,0,500},5,.7F,9,0,{},{});break;
        case 2:broken.projection[0]=0;break;
        case 3:broken.view[2]=std::numeric_limits<float>::quiet_NaN();break;
        case 4:rig[15]=0;break;
        case 5:origin[0]=std::numeric_limits<double>::infinity();break;
        case 6:width=16385;break;
        case 7:scale=0;break;
        case 8:broken.projection[5]=1.e-40F;break;
        case 9:world.add({0,0,500},5,.7F,3,-1,{},{});break;
        case 10:world.add({0,0,500},0,.7F,1,0,{},{});break;
        case 11:broken.projection[4]=.1F;break;
        case 12:world.player=std::array<double,3>{0,0,std::numeric_limits<double>::quiet_NaN()};break;
        }
        gpu.begin();check(!fx.enqueue(gpu.command,source.texture,ownership.texture,surface.texture,left.texture,width,h,scale,world,broken,rig,origin),"Invalid native scene FX payload/view encoded");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        gpu.begin();void* output=reinterpret_cast<void*>(1);
        check(!fx.project_particles(gpu.command,width,h,scale,world,broken,rig,origin,nullptr,output) && !output,
            "Invalid native particle projection encoded/retained output");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    std::cout<<"Native world FX format "<<unsigned(format)<<" size="<<w<<'x'<<h<<": independent asymmetric eyes, all nine types, clipped/over-cap world populations, large wrapped origins, scale, protected ink, exact alpha/OFF and cancellation passed.\n";
}
}
int main(int argc,char** argv) try {
    const char* backend=argc>1?argv[1]:"direct3d12";
    check(std::string_view(backend)=="direct3d12" || std::string_view(backend)=="vulkan","Use direct3d12|vulkan");
    Gpu gpu;check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());gpu.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,true,backend);check(gpu.device,SDL_GetError());
    for(auto format:{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB}) {
        run(gpu,format,64,40);run(gpu,format,127,79);
    }
    std::cout<<"Native scene FX "<<backend<<": "<<checks<<" checks, "<<responses<<" changed authored colour bytes. GPU component fixture only; owner/frontend and physical Leia validation are separate.\n";return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
