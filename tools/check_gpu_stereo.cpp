#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/gpu_model.hpp"
#include "starfox/render/stereo_output.hpp"
#include "starfox/render/gpu_composite.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <source_location>
#include <cstring>
#include <bit>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void check_no_reshade_injector() {
#if defined(_WIN32)
    if(GetModuleHandleW(L"ReShade64.dll") || GetModuleHandleW(L"ReShade32.dll"))
        throw std::runtime_error("Native stereo checker loaded ReShade; disable its layer for this test process");
#endif
}

void require(bool ok,const std::source_location where=std::source_location::current()) {
    if(!ok) throw std::runtime_error("Stereo check line "+std::to_string(where.line())+": "+SDL_GetError());
}
void check_packing(SDL_GPUDevice* device) {
    using namespace starfox::render;
    constexpr unsigned width=8,height=4;
    SDL_GPUTextureCreateInfo info{};
    info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width=width;info.height=height;info.layer_count_or_depth=1;info.num_levels=1;
    std::array<SDL_GPUTexture*,2> eyes{};
    for(auto& eye:eyes) {eye=SDL_CreateGPUTexture(device,&info);require(eye);}
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*8,0};
    auto* download=SDL_CreateGPUTransferBuffer(device,&transfer);require(download);
    for(const auto mode:{StereoOutput::off,StereoOutput::half_sbs,StereoOutput::full_sbs,
        StereoOutput::half_top_bottom,StereoOutput::full_top_bottom,StereoOutput::crossview}) {
        const auto layout=stereo_output_layout(mode,width,height).value();
        info.width=layout.width;info.height=layout.height;
        auto* destination=SDL_CreateGPUTexture(device,&info);require(destination);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        for(unsigned eye=0;eye<2;++eye) {
            SDL_GPUColorTargetInfo target{};target.texture=eyes[eye];
            target.load_op=SDL_GPU_LOADOP_CLEAR;target.store_op=SDL_GPU_STOREOP_STORE;
            target.clear_color=eye?SDL_FColor{0,0,1,1}:SDL_FColor{1,0,0,1};
            auto* pass=SDL_BeginGPURenderPass(command,&target,1,nullptr);require(pass);SDL_EndGPURenderPass(pass);
        }
        require(enqueue_stereo_texture_pack(command,eyes[0],eyes[1],destination,mode,width,height));
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTextureRegion region{destination,0,0,0,0,0,layout.width,layout.height,1};
        SDL_GPUTextureTransferInfo target{download,0,0,0};
        SDL_DownloadFromGPUTexture(copy,&region,&target);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* pixels=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device,download,false));require(pixels);
        bool exact=true;
        for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
            const auto i=(y*layout.width+x)*4;
            const auto& viewport=layout.eyes[1];
            const bool right=layout.eye_count==2 && x>=viewport.x && x<viewport.x+viewport.width
                && y>=viewport.y && y<viewport.y+viewport.height;
            exact &= pixels[i]==(right?0:255) && pixels[i+1]==0
                && pixels[i+2]==(right?255:0) && pixels[i+3]==255;
        }
        SDL_UnmapGPUTransferBuffer(device,download);require(exact);
        SDL_ReleaseGPUTexture(device,destination);
        std::cout<<"GPU packing mode "<<unsigned(mode)<<": "<<layout.width<<"x"<<layout.height<<" exact pixels\n";
    }
    for(auto* eye:eyes) SDL_ReleaseGPUTexture(device,eye);
    SDL_ReleaseGPUTransferBuffer(device,download);
}
void check_overlay_packing(bool gpu=false) {
    using namespace starfox::render;
    auto* surface=SDL_CreateSurface(12,9,SDL_PIXELFORMAT_RGBA32);require(surface);
    auto* window=gpu?SDL_CreateWindow("Stereo packing check",12,9,SDL_WINDOW_HIDDEN):nullptr;
    if(gpu) require(window);
    auto* renderer=gpu?SDL_CreateRenderer(window,"gpu"):SDL_CreateSoftwareRenderer(surface);require(renderer);
    std::array<SDL_Texture*,2> eyes{};
    const std::array<std::array<unsigned char,4>,2> colors{{{210,80,40,255},{30,140,220,255}}};
    for(unsigned eye=0;eye<2;++eye) {
        eyes[eye]=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,1,1);require(eyes[eye]);
        require(SDL_UpdateTexture(eyes[eye],nullptr,colors[eye].data(),4));
        require(SDL_SetTextureColorMod(eyes[eye],128,64,32));
        require(SDL_SetTextureBlendMode(eyes[eye],SDL_BLENDMODE_BLEND));
    }
    for(auto mode:{StereoOutput::interlaced,StereoOutput::interlaced_reversed,StereoOutput::anaglyph_red_cyan}) {
        require(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer));
        require(render_stereo_overlay(renderer,eyes[0],eyes[1],mode,12,9,std::array<float,4>{2,1,8,7}));
        require(SDL_FlushRenderer(renderer));
        auto* captured=SDL_RenderReadPixels(renderer,nullptr);require(captured);
        for(unsigned y=0;y<9;++y) for(unsigned x=0;x<12;++x) {
            Uint8 r,g,b,a;require(SDL_ReadSurfacePixel(captured,x,y,&r,&g,&b,&a));
            std::array<unsigned char,4> expected{0,0,0,255};
            if(x>=2 && x<10 && y>=1 && y<8) {
                if(mode==StereoOutput::anaglyph_red_cyan) expected={210,140,220,255};
                else expected=colors[(y&1U)^(mode==StereoOutput::interlaced_reversed?1U:0U)];
            }
            require((std::array<unsigned char,4>{r,g,b,a}==expected));
        }
        SDL_DestroySurface(captured);
        for(auto* eye:eyes) {
            Uint8 r,g,b;SDL_BlendMode blend;
            require(SDL_GetTextureColorMod(eye,&r,&g,&b) && r==128 && g==64 && b==32);
            require(SDL_GetTextureBlendMode(eye,&blend) && blend==SDL_BLENDMODE_BLEND);
        }
        std::cout<<(gpu?"GPU":"Software")<<" overlay packing "<<unsigned(mode)<<": exact channels, row parity, bars and state restoration\n";
    }
    require(!render_stereo_overlay(renderer,eyes[0],eyes[1],StereoOutput::off,12,9));
    require(!render_stereo_overlay(renderer,eyes[0],eyes[0],StereoOutput::interlaced,12,9));
    for(auto* eye:eyes) SDL_DestroyTexture(eye);
    SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);if(window) SDL_DestroyWindow(window);
}
#include "check_row_tile_raster.inc"
#include "check_model_uploads.inc"
#include "check_owned_stereo_pair.inc"
#include "check_stereo_source_uploads.inc"
#include "check_shared_model_memo.inc"
#include "check_stereo_billboard_uploads.inc"
#include "check_stereo_msaa.inc"
#include "check_projection_preparation.inc"
int main() try {
    using namespace starfox::render;
    require(SDL_Init(SDL_INIT_VIDEO));
    check_overlay_packing();
    check_overlay_packing(true);
    check_no_reshade_injector();
    const auto properties=SDL_CreateProperties();require(properties!=0);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,
        SDL_getenv("STARFOX_TEST_LOW_POWER_GPU")!=nullptr);
    auto* device=SDL_CreateGPUDeviceWithProperties(properties);SDL_DestroyProperties(properties);
    require(device);
    check_no_reshade_injector();
    std::cout<<"GPU stereo adapter: "
        <<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")
        <<" driver="<<SDL_GetGPUDeviceDriver(device)<<std::endl;
    check_projection_preparation(device);
    if(SDL_getenv("STARFOX_TEST_PROJECTION_PREPARATION_ONLY")) {
        SDL_DestroyGPUDevice(device);SDL_Quit();return 0;
    }
    if(SDL_getenv("STARFOX_TEST_BILLBOARD_SOURCE_ONLY")) {
        check_stereo_billboard_uploads(device);SDL_DestroyGPUDevice(device);SDL_Quit();return 0;
    }
    if(SDL_getenv("STARFOX_TEST_MASK_TILE_ONLY")) {
        check_row_tile_raster(device,true,true);check_row_tile_raster(device,false,true);
        SDL_DestroyGPUDevice(device);SDL_Quit();return 0;
    }
    check_packing(device);
    check_row_tile_raster(device);
    check_row_tile_raster(device,false);
    check_row_tile_raster(device,true,true);check_row_tile_raster(device,false,true);
    check_model_uploads(device);
    check_prepared_model_source(device);
    check_stereo_source_uploads(device);
    check_shared_model_memo(device);
    check_stereo_billboard_uploads(device);
    check_owned_stereo_pair(device);
    check_owned_stereo_pair(device,true);
    check_stereo_msaa(device);
    {
        constexpr unsigned width=224,height=192,bytes=width*height*4;
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes*2,0};
        auto* download=SDL_CreateGPUTransferBuffer(device,&info);require(download);
        GpuStereoScene scene;
        starfox::assets::Shape shape;
        shape.vertices={{-20,-20,0},{20,-20,0},{0,20,0}};
        // Source span filling requires the cartridge's clockwise face order.
        shape.faces={{-1,0,{0,0,1},{2,1,0}}};shape.colour_words={0x11};
        GpuModelDraw model;model.shape=&shape;
        model.pose.force_colour=true;model.pose.forced_colour=0x11;
        // Exercise reuse and both sides of the zero-disparity plane.
        for(const double depth:{100.,200.,400.,100.}) {
            model.pose.z=depth;
            model.geometry_depth=true;
            model.previous_pose=model.pose;
            model.previous_pose->x-=depth==400?0:2;
            const std::array<GpuSceneDraw,1> frame{model};
            Framebuffer reference(width,height);
            const auto reference_eye=stereo_scene_eye(frame,0,6.4,200);
            const auto& reference_model=std::get<GpuModelDraw>((*reference_eye)[0]);
            SoftwareRenderer(reference_model.settings).draw(shape,reference_model.pose,reference);
            std::cout<<"CPU coverage "<<std::count_if(reference.pixels().begin(),reference.pixels().end(),[](auto p){return p!=0;})<<'\n';
            // Separate instances exercise owned/fenced submissions without
            // mixing them into the caller-owned enqueue instance's lifetime.
            GpuStereoScene owned;
            require(owned.render_resident(device,width,height,frame,6.4,200));
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
            const auto output=depth==200
                ? std::optional<std::array<GpuRasterOutput,2>>{{owned.resident_output(0),owned.resident_output(1)}}
                : scene.enqueue(device,command,width,height,frame,6.4,200);
            if(!output) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error("Stereo enqueue failed");}
            require((*output)[0].pixels!=(*output)[1].pixels);
            auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
            for(unsigned eye=0;eye<2;++eye) {
                SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>((*output)[eye].pixels),0,bytes};
                SDL_GPUTransferBufferLocation to{download,eye*bytes};
                SDL_DownloadFromGPUBuffer(pass,&from,&to);
            }
            SDL_EndGPUCopyPass(pass);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
            require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
            const auto* pixels=static_cast<const uint32_t*>(SDL_MapGPUTransferBuffer(device,download,false));require(pixels);
            std::array<double,2> centroid{};
            std::array<unsigned,2> count{};
            bool exact=true;
            for(unsigned eye=0;eye<2;++eye) {
                const auto eye_frame=stereo_scene_eye(frame,eye,6.4,200);
                const auto& draw=std::get<GpuModelDraw>((*eye_frame)[0]);
                SoftwareRenderer(draw.settings).draw(shape,draw.pose,reference,true);
                for(unsigned i=0;i<width*height;++i)
                    exact &= (pixels[eye*width*height+i]&255U)==reference.pixels()[i];
            }
            for(unsigned eye=0;eye<2;++eye) for(unsigned i=0;i<width*height;++i)
                if((pixels[eye*width*height+i]&255U)!=0) {centroid[eye]+=i%width;++count[eye];}
            SDL_UnmapGPUTransferBuffer(device,download);
            require(exact);
            std::cout<<"GPU coverage "<<count[0]<<" / "<<count[1]<<'\n';
            require(count[0]>20 && count[1]>20);
            for(unsigned eye=0;eye<2;++eye) centroid[eye]/=count[eye];
            const double disparity=centroid[0]-centroid[1];
            const double expected=256*6.4*(1/depth-1/200.);
            if(std::abs(disparity-expected)>1) throw std::runtime_error("Native stereo disparity incorrect");
            std::cout<<"Depth "<<depth<<": disparity "<<disparity<<", expected "<<expected<<"; both outputs retained\n";
            // History is transformed into each eye before native projection.
            // Pure lateral travel has equal screen velocity in both eyes;
            // a stationary object must not acquire the eye separation as motion.
            Palette256 palette{};palette[0x11]={255,255,255,255};
            Framebuffer background(width,height);
            for(unsigned eye=0;eye<2;++eye) {
                GpuComposite composite;
                require(composite.compose((*output)[eye],1,background,{}, {},palette));
                std::vector<MotionBlurGuide> guides;
                require(composite.readback_motion_guides(true,guides));
                unsigned valid=0;
                const double velocity=depth==400?0:-512./depth;
                for(const auto& guide:guides) if(guide.valid) {
                    ++valid;
                    require(std::abs(guide.motion_x-velocity)<.002);
                    require(std::abs(guide.motion_y)<.002);
                    require(std::abs(guide.depth-depth)<.002);
                }
                require(valid>20);
                std::cout<<"Eye "<<eye<<": "<<valid<<" native motion guides, expected velocity "<<velocity<<'\n';
            }
        }
        // Regress the sparse-to-temporal producer transition. Four helper
        // models lend producer zero's target; the next unfused model must NOT
        // clear that same buffer before merging. Odd/even helper counts and
        // frame reuse vary which producer is selected. A colored underlay makes
        // covered black ink observable instead of hiding it against a clear.
        RasterCommands underlay;underlay.reset(width,height);
        RasterCommand fill;fill.right=width;fill.bottom=height;fill.even=fill.odd=0x40;
        underlay.commands.push_back(fill);
        // Shared CPU-binned chunks used to rebuild rows concurrently in both
        // encoders. Repeated, changing underlays make that race observable;
        // a one-off successful pair is not sufficient ownership evidence.
        for(unsigned round=0;round<16;++round) for(const unsigned helpers:{0U,1U,3U,4U,5U,4U}) {
            underlay.commands[0].even=underlay.commands[0].odd=0x40+round%8;
            underlay.commands.resize(1);
            auto stripe=fill;stripe.left=int(round%7)*9;stripe.right=stripe.left+31;
            stripe.top=int(round%5)*17;stripe.bottom=stripe.top+23;
            stripe.even=stripe.odd=0x50+round%8;underlay.commands.push_back(stripe);
            std::vector<GpuSceneDraw> frame{GpuRasterDraw{&underlay}};
            auto moving=model;moving.pose.z=256;moving.pose.x=0;moving.pose.y=0;
            moving.previous_pose=moving.pose;moving.previous_pose->x-=2;
            for(unsigned n=0;n<helpers;++n) {
                auto helper=moving;helper.previous_pose.reset();helper.surface_metadata=true;
                helper.pose.x=(int(n)-int(helpers)/2)*45;
                helper.pose.y=n%2?-30:30;helper.pose.forced_colour=n?0x31+n:0;
                frame.emplace_back(helper);
            }
            frame.emplace_back(moving);
            require(scene.render_resident(device,width,height,frame,6.4,200));
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
            for(unsigned eye=0;eye<2;++eye) {
                const auto output=scene.resident_output(eye);require(output.pixels && output.motion);
                SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,bytes};
                SDL_GPUTransferBufferLocation to{download,eye*bytes};SDL_DownloadFromGPUBuffer(copy,&from,&to);
            }
            SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
            require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
            const auto* pixels=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,download,false));require(pixels);
            bool exact=true;
            for(unsigned eye=0;eye<2;++eye) {
                Framebuffer reference(width,height);replay_raster_commands(underlay,reference,nullptr);
                const auto eye_frame=stereo_scene_eye(frame,eye,6.4,200);require(eye_frame.has_value());
                for(const auto& item:*eye_frame) if(const auto* draw=std::get_if<GpuModelDraw>(&item))
                    SoftwareRenderer(draw->settings).draw(shape,draw->pose,reference,false);
                for(unsigned i=0;i<width*height;++i)
                    exact&=(pixels[eye*width*height+i]&255U)==reference.pixels()[i];
            }
            SDL_UnmapGPUTransferBuffer(device,download);
            if(!exact) throw std::runtime_error("Sparse-to-motion painter transition lost earlier ink, round="
                +std::to_string(round)+" helpers="+std::to_string(helpers));
        }
        std::cout<<"Sparse-to-motion transition: 96 changing odd/even helper batches match independent CPU eyes, including black ink and shared CPU-binned underlays\n";
        scene.release_device();SDL_ReleaseGPUTransferBuffer(device,download);
    }
    check_no_reshade_injector();
    std::cout<<"Native stereo checker: known ReShade injector absent\n";
    SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<"Native GPU stereo submission passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
