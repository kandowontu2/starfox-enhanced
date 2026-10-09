#include "starfox/render/gpu_volumetric_fog.hpp"
#include "starfox/render/sdl_gpu_effects.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <iostream>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#include "starfox/render/sdl_d3d12_bridge.h"
#include <d3d12.h>
#include <wrl/client.h>
#endif
using namespace starfox::render;
void require(bool ok,const std::string& message) {if(!ok) throw std::runtime_error(message);}
#ifdef _WIN32
struct FogQueueGate {
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    explicit FogQueueGate(SDL_GPUDevice* device) {
        const auto props=SDL_GetGPUDeviceProperties(device);
        auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_DEVICE,nullptr));
        const auto* bridge=static_cast<const StarfoxSdlD3D12XrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_XR_BRIDGE,nullptr));
        require(native && bridge && bridge->version==1 && bridge->with_queue,"Fog lifetime fixture requires the native queue bridge");
        require(SUCCEEDED(native->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_ID3D12Fence,
            reinterpret_cast<void**>(fence.GetAddressOf()))),"Fog queue gate fence allocation failed");
        require(bridge->with_queue(device,[](void* user,void* queue) {
            return SUCCEEDED(static_cast<ID3D12CommandQueue*>(queue)->Wait(static_cast<ID3D12Fence*>(user),1));
        },fence.Get()),"Fog queue gate wait failed");
    }
    void unblock() {require(SUCCEEDED(fence->Signal(1)),"Fog queue gate release failed");}
    ~FogQueueGate() {if(fence) fence->Signal(1);}
};
#endif
int main(int argc,char** argv) {
    SDL_GPUDevice* device=nullptr;
    try {
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_MSL,
            true,argc>1?argv[1]:nullptr);
        require(device,SDL_GetError());
        GpuVolumetricFog gpu;
        shadows::Scene empty;empty.build();
        shadows::Scene scene;
        scene.add({{-2,-2,5},{2,-2,5},{0,2,5}});
        scene.add({{-100,-5,-100},{100,-5,-100},{100,-5,50}});
        scene.add({{-100,-5,-100},{100,-5,50},{-100,-5,50}});scene.build();
        VolumetricMedium medium;medium.extinction=.01;medium.maximum_distance=100;medium.anisotropy=.35;
        const shadows::Vec3 light{0,-1,0};
        GpuVolumetricFog shared[2],resident;
        const VolumetricProjection centre{4,5,4.5,3.5};
        for(unsigned geometry=0;geometry<2;++geometry)
            require(shared[geometry].render(device,geometry?scene:empty,centre,9,7,medium,light,std::nullopt),shared[geometry].status());
        float worst=0;unsigned cases=0;
        for(bool background_only:{false,true}) for(double eye:{-1.,0.,1.}) for(bool geometry:{false,true}) for(bool ground:{false,true}) for(unsigned samples:{1U,16U,64U,128U}) {
            medium.samples=samples;
            shadows::Scene current;
            const shadows::Vec3 eye_offset{-eye,0,0};
            for(const auto& t:(geometry?scene:empty).triangles()) current.add({t.a+eye_offset,t.b+eye_offset,t.c+eye_offset});
            current.build();
            const unsigned width=9,height=7;
            const VolumetricProjection projection{4,5,4.5+4*eye/20,3.5};
            const auto plane=ground?std::optional<VolumetricGround>{{{-eye,0,10},{0,1,-1}}}:std::nullopt;
            require(gpu.render(device,current,projection,width,height,medium,light,plane,background_only),gpu.status());
            const auto output=gpu.output();require(output.buffer&&output.width==width&&output.height==height,"Missing resident fog output");
            std::vector<std::array<float,4>> actual;
            require(gpu.readback(actual),gpu.status());
            const auto source=shared[geometry].scene_output();
            require(source.complete && source.nodes && source.triangles,"Missing borrowed source BVH");
            const auto source_plane=ground?std::optional<VolumetricGround>{{{0,0,10},{0,1,-1}}}:std::nullopt;
            require(resident.render_resident(device,source,projection,width,height,medium,light,source_plane,
                background_only,{eye,0,0}),resident.status());
            require(resident.scene_output().nodes==source.nodes && resident.scene_output().triangles==source.triangles
                && resident.output().buffer!=gpu.output().buffer,"Fog eyes did not share only source geometry");
            std::vector<std::array<float,4>> shared_actual;
            require(resident.readback(shared_actual),resident.status());
            std::vector<std::uint8_t> coverage(width*height,1);
            std::vector<VolumetricPixel> guides;
            require(build_volumetric_guides(current,projection,width,height,coverage,medium.maximum_distance,plane,guides,background_only),"CPU guide failure");
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const auto i=y*width+x;
                const shadows::Vec3 ray{(x+.5-projection.center_x)/projection.focal_x,(y+.5-projection.center_y)/projection.focal_y,1};
                const auto distance=guides[i].view_depth==0?medium.maximum_distance:
                    guides[i].view_depth*std::sqrt(shadows::dot(ray,ray));
                const auto expected=integrate_volumetric_fog(medium,{},ray,distance,light,current);
                require(bool(expected),"CPU integral failure");
                const double values[]{expected->scattering.x,expected->scattering.y,expected->scattering.z,expected->transmittance};
                for(unsigned c=0;c<4;++c) {
                    const float delta=float(std::abs(actual[i][c]-values[c]));worst=std::max(worst,delta);
                    require(std::isfinite(actual[i][c])&&delta<.0001f,"GPU fog differs from CPU: case="+std::to_string(cases)
                        +" pixel="+std::to_string(i)+" channel="+std::to_string(c)+" delta="+std::to_string(delta)
                        +" expected="+std::to_string(values[c])+" actual="+std::to_string(actual[i][c])
                        +" depth="+std::to_string(guides[i].view_depth)+" T="+std::to_string(actual[i][3]));
                    const float shared_delta=float(std::abs(shared_actual[i][c]-values[c]));worst=std::max(worst,shared_delta);
                    require(std::isfinite(shared_actual[i][c]) && shared_delta<.0001f,
                        "Shared source fog differs from independently translated CPU geometry: case="+std::to_string(cases)
                        +" pixel="+std::to_string(i)+" channel="+std::to_string(c)+" delta="+std::to_string(shared_delta));
                }
            }
            ++cases;
        }
        {
            // The general source-space origin includes vertical/depth offsets,
            // not just the two horizontally separated SBS cameras.
            const shadows::Vec3 origin{1.25,-.5,2};
            shadows::Scene translated;
            for(const auto& t:scene.triangles()) translated.add({t.a-origin,t.b-origin,t.c-origin});translated.build();
            const VolumetricGround plane{{0,1,10},{.25,1,-1}};
            const VolumetricGround shifted{plane.point-origin,plane.normal};
            for(bool underlay:{false,true}) {
                require(gpu.render(device,translated,centre,9,7,medium,light,shifted,underlay),gpu.status());
                require(resident.render_resident(device,shared[1].scene_output(),centre,9,7,medium,light,plane,underlay,origin),resident.status());
                std::vector<std::array<float,4>> a,b;require(gpu.readback(a)&&resident.readback(b),"Translated fog readback failed");
                for(unsigned i=0;i<a.size();++i) for(unsigned c=0;c<4;++c)
                    require(std::abs(a[i][c]-b[i][c])<.0001f,"Shared 3-axis source/ground origin mismatch");
            }
        }
        {
            GpuVolumetricFog producer,left,right;
            require(producer.render(device,scene,centre,9,7,medium,light,std::nullopt),producer.status());
            const auto source=producer.scene_output();
            require(left.render_resident(device,source,centre,9,7,medium,light,std::nullopt),left.status());
            require(right.render_resident(device,source,{4,5,4.7,3.5},9,7,medium,light,std::nullopt,false,{1,0,0}),right.status());
            // Re-publish the donor only AFTER both consumers were submitted.
            // Their independent output must retain the old scene, even though
            // the donor now uploads a different (empty) source allocation.
            require(producer.render(device,empty,centre,9,7,medium,light,std::nullopt),producer.status());
            producer.release_device();
            std::vector<std::array<float,4>> a,b,expected;
            require(left.readback(a)&&right.readback(b),"Pending shared source consumption lost output");
            require(gpu.render(device,scene,centre,9,7,medium,light,std::nullopt)&&gpu.readback(expected),gpu.status());
            require(a==expected && a!=b,"Shared source output was overwritten or stereo origin was ignored");
            shadows::Scene translated;for(const auto& t:scene.triangles()) translated.add({t.a-shadows::Vec3{1,0,0},t.b-shadows::Vec3{1,0,0},t.c-shadows::Vec3{1,0,0}});translated.build();
            require(gpu.render(device,translated,{4,5,4.7,3.5},9,7,medium,light,std::nullopt)&&gpu.readback(expected),gpu.status());
            for(unsigned i=0;i<b.size();++i) for(unsigned c=0;c<4;++c)
                require(std::abs(b[i][c]-expected[i][c])<.0001f,"Right eye shared-source retirement mismatch");
        }
        {
            const auto source=shared[1].scene_output();
            for(unsigned malformed=0;malformed<8;++malformed) {
                auto bad=source;
                switch(malformed) {
                case 0:bad.complete=false;break;
                case 1:bad.device=nullptr;break;
                case 2:bad.nodes=nullptr;break;
                case 3:bad.triangles=bad.nodes;break;
                case 4:bad.node_bytes=0;break;
                case 5:bad.triangle_bytes=0;break;
                case 6:bad.node_count=16777217;break;
                case 7:bad.triangle_count=0;break;
                }
                require(!resident.render_resident(device,bad,centre,9,7,medium,light,std::nullopt)
                    && !resident.output().buffer && !resident.scene_output().complete,"Malformed source retained stale fog");
                require(shared[1].scene_output().nodes==source.nodes,"Rejected consumer mutated source owner");
                require(resident.render_resident(device,source,centre,9,7,medium,light,std::nullopt),resident.status());
            }
            auto alias=source;alias.nodes=resident.output().buffer;
            require(!resident.render_resident(device,alias,centre,9,7,medium,light,std::nullopt),"Fog output accepted as its source BVH");
            for(auto origin:{shadows::Vec3{INFINITY,0,0},shadows::Vec3{1e300,0,0}}) {
                require(!resident.render_resident(device,source,centre,9,7,medium,light,std::nullopt,false,origin)
                    && !resident.output().buffer,"Invalid source origin retained stale fog");
                require(!gpu.render(device,scene,centre,9,7,medium,light,std::nullopt,false,origin)
                    && !gpu.output().buffer,"Invalid uploaded-scene origin retained stale fog");
            }
        }
#ifdef _WIN32
        if(std::string_view(SDL_GetGPUDeviceDriver(device))=="direct3d12") {
            GpuVolumetricFog producer,left,right;
            require(producer.render(device,scene,centre,9,7,medium,light,std::nullopt),producer.status());
            std::vector<std::array<float,4>> expected,actual;
            require(producer.readback(expected),producer.status());
            const auto source=producer.scene_output();
            // The source upload is finished; readers are then deliberately
            // queued behind an unsignalled native fence. Destroying the donor
            // must retain SDL's pending read references, not wait for readers
            // or delete their live native buffers. Gate RAII releases on error.
            FogQueueGate gate(device);
            require(left.render_resident(device,source,centre,9,7,medium,light,std::nullopt),left.status());
            require(right.render_resident(device,source,centre,9,7,medium,light,std::nullopt),right.status());
            producer.release_device();
            gate.unblock();
            require(left.readback(actual) && actual==expected,"Stalled left fog lost released donor geometry");
            require(right.readback(actual) && actual==expected,"Stalled right fog lost released donor geometry");
            std::cout<<"D3D12 shared fog source: donor release with two genuinely stalled GPU readers passed.\n";
        }
#endif
        {
            Framebuffer frame(9,7);frame.enable_layer_tags(true);
            std::vector<std::uint8_t> source(9*7*4);
            for(unsigned i=0;i<63;++i) {
                source[i*4]=std::uint8_t(i*11);source[i*4+1]=std::uint8_t(i*7);source[i*4+2]=std::uint8_t(i*3);
                source[i*4+3]=i%7?200:0;
                if(i%5==0) frame.layer_tags()[i]=std::uint8_t(PixelLayer::two_d);
            }
            const VolumetricProjection projection{4,5,4.5,3.5};
            auto expected=source,actual=source;
            require(apply_volumetric_fog(medium,projection,frame,scene,light,std::nullopt,expected),"CPU composite failed");
            require(gpu.render(device,scene,projection,9,7,medium,light,std::nullopt),gpu.status());
            GpuEffectSettings settings;settings.volumetric=gpu.output();
            SdlGpuEffects compositor;
            require(compositor.apply(device,frame,actual,settings),compositor.status());
            for(unsigned i=0;i<actual.size();++i) {
                const bool protected_pixel=frame.layer_tags()[i/4]==std::uint8_t(PixelLayer::two_d)||source[(i/4)*4+3]==0;
                require(std::abs(int(actual[i])-int(expected[i]))<=(protected_pixel||i%4==3?0:1),
                    "GPU fog composite/HUD mismatch byte="+std::to_string(i));
            }
            actual=source;settings.volumetric={};
            require(compositor.apply(device,frame,actual,settings)&&actual==source,"Fog toggle retained stale composite binding");
            compositor.release_device();
        }
        medium.extinction=0;
        require(gpu.render(device,scene,{1,1,.5,.5},1,1,medium,light,std::nullopt),gpu.status());
        std::vector<std::array<float,4>> identity;
        require(gpu.readback(identity)&&identity.size()==1&&identity[0]==std::array<float,4>{0,0,0,1},"Zero density not identity");
        require(!gpu.render(device,scene,{0,1,.5,.5},1,1,medium,light,std::nullopt)&&!gpu.output().buffer,"Invalid input retained stale fog");
        require(!gpu.render(device,scene,{1,1,1e300,.5},1,1,medium,light,std::nullopt)&&!gpu.output().buffer,"Float overflow accepted");
        require(gpu.render(device,empty,{1,1,.5,.5},2,2,medium,light,std::nullopt),"Could not recover after invalid input");
        gpu.release_device();require(!gpu.output().buffer,"Release retained output");
        require(gpu.render(device,empty,{1,1,.5,.5},1,1,medium,light,std::nullopt),"Could not reacquire device");
        gpu.release_device();
        resident.release_device();for(auto& owner:shared) owner.release_device();
        std::cout<<"GPU volumetric parity passed: "<<SDL_GetGPUDeviceDriver(device)<<", cases="<<cases<<", max error="<<worst<<'\n';
        SDL_DestroyGPUDevice(device);SDL_Quit();return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<'\n';if(device) SDL_DestroyGPUDevice(device);SDL_Quit();return 1;
    }
}
