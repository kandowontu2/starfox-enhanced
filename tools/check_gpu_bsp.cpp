#include <SDL3/SDL.h>
#include "starfox/render/gpu_bsp.hpp"
#include "starfox/render/gpu_clip.hpp"
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/packed_bsp.hpp"
#include "starfox/render/packed_projection.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <source_location>
#include <vector>
using U=Uint32;
using Four=std::array<U,4>;
struct Node {Four links,batch;};
void require(bool value,std::source_location where=std::source_location::current()) {
    if(!value) throw std::runtime_error("BSP check line "+std::to_string(where.line())+": "+SDL_GetError());
}
#include "check_projected_bsp.inc"
#include "check_small_model_stage.inc"
int main(int argc,char** argv)try {
    require(SDL_Init(SDL_INIT_VIDEO));
    const auto properties=SDL_CreateProperties();require(properties!=0);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,SDL_getenv("STARFOX_TEST_LOW_POWER_GPU")!=nullptr);
    auto* device=SDL_CreateGPUDeviceWithProperties(properties);SDL_DestroyProperties(properties);require(device);
    std::cout<<"BSP GPU adapter: "<<SDL_GetGPUDeviceDriver(device)<<" / "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<'\n';
    check_projected_bsp(device);
    check_small_model_stage(device);
    starfox::render::GpuBsp bsp;
    require(!bsp.enqueue(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,{}).order);
    std::vector<Node> nodes;std::vector<U> visibility,faces;std::vector<Four> trees;
    std::vector<std::vector<U>> expected;
    U seed=19891;
    for(U t=0;t<128;++t) {
        U base=U(nodes.size());
        for(U i=0;i<31;++i) {
            seed=seed*1664525U+1013904223U;
            visibility.push_back(seed>>31);
            U first=U(faces.size());faces.push_back(t*100+i*2);faces.push_back(t*100+i*2+1);
            nodes.push_back({{base+i,i<15?base+i*2+1:UINT32_MAX,i<15?base+i*2+2:UINT32_MAX,first},{2,i>=15?1U:0U,0,0}});
        }
        // Exercise a cycle and a shared subtree without suppressing later visits.
        if(t%3==0) nodes[base+7].links[1]=base;
        if(t%5==0) nodes[base+2].links[2]=base+3;
        if(t%7==0) nodes[base+4].links[0]=UINT32_MAX;
        trees.push_back({base,t*256,256,4096});
        std::vector<U> out,active;
        std::function<void(U)> walk=[&](U index) {
            if(index>=nodes.size()) return;
            for(U prior:active) if(prior==index) return;
            active.push_back(index);
            const auto& n=nodes[index];
            const bool visible=n.links[0]<visibility.size() && visibility[n.links[0]];
            if(n.batch[1]) for(U f=0;f<n.batch[0];++f) out.push_back(faces[n.links[3]+f]);
            else if(visible) {
                walk(n.links[1]);
                for(U f=0;f<n.batch[0];++f) out.push_back(faces[n.links[3]+f]);
                walk(n.links[2]);
            } else {walk(n.links[2]);walk(n.links[1]);}
            active.pop_back();
        };
        walk(base);expected.push_back(out);
    }
    U realModels=0;
    if(argc==3) {
        const auto rom=starfox::assets::RomImage::load(argv[1]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
        const starfox::assets::ShapeDecoder decoder(rom,symbols);std::set<U> seen;
        for(const auto& [name,addresses]:symbols.entries()) for(auto address:addresses)
            if(seen.insert(address).second && decoder.looks_like_shape_header(address)) {
                const auto model=decoder.decode(address,name);const auto packed=starfox::render::pack_bsp(model);
                require(packed.output_capacity<=256);++realModels;
                for(U pattern=0;pattern<16;++pattern) {
                    const U nodeBase=U(nodes.size()),faceBase=U(faces.size()),visBase=U(visibility.size());
                    for(U i=0;i<model.visibilities.size();++i)
                        visibility.push_back(pattern==1 || (pattern>1 && ((i*1664525U+pattern*1013904223U)>>(pattern+8))&1));
                    faces.insert(faces.end(),packed.face_ids.begin(),packed.face_ids.end());
                    for(const auto& n:packed.nodes) {
                        Node relocated{n.links,n.batch};
                        if(relocated.links[0]!=UINT32_MAX) relocated.links[0]+=visBase;
                        for(U axis=1;axis<=2;++axis) if(relocated.links[axis]!=UINT32_MAX) relocated.links[axis]+=nodeBase;
                        relocated.links[3]+=faceBase;nodes.push_back(relocated);
                    }
                    U root=packed.root==UINT32_MAX?UINT32_MAX:packed.root+nodeBase;
                    trees.push_back({root,U(trees.size())*256,256,packed.work_limit});
                    std::vector<U> out;std::set<U> active;
                    std::function<void(U)> walk=[&](U id) {
                        if(id==UINT32_MAX || !active.insert(id).second) return;
                        const auto& n=nodes[id];bool visible=n.links[0]<visibility.size() && visibility[n.links[0]];
                        const auto append=[&]{for(U f=0;f<n.batch[0];++f) out.push_back(faces[n.links[3]+f]);};
                        if(n.batch[1]) append();
                        else if(visible) {walk(n.links[1]);append();walk(n.links[2]);}
                        else {walk(n.links[2]);walk(n.links[1]);}
                        active.erase(id);
                    };
                    walk(root);expected.push_back(out);
                }
            }
    }else require(argc==1);
    // Fail closed on insufficient output, exhausted work, bad batch ranges,
    // and recursion deeper than the explicitly supported stack.
    const U failFirst=U(trees.size());
    trees.push_back({0,failFirst*256,0,4096});
    trees.push_back({0,(failFirst+1)*256,256,0});
    U bad=U(nodes.size());nodes.push_back({{0,UINT32_MAX,UINT32_MAX,UINT32_MAX},{2,1,0,0}});
    trees.push_back({bad,(failFirst+2)*256,256,4096});
    U chain=U(nodes.size());
    for(U i=0;i<65;++i) nodes.push_back({{UINT32_MAX,UINT32_MAX,chain+i+1,0},{0,0,0,0}});
    trees.push_back({chain,(failFirst+3)*256,256,4096});
    const U outputCount=U(trees.size())*256;
    std::array<U,6> sizes{U(nodes.size()*sizeof(Node)),U(visibility.size()*4),U(faces.size()*4),U(trees.size()*16),outputCount*4,U(trees.size()*8)};
    std::array<SDL_GPUBuffer*,6> buffers{};
    for(U i=0;i<4;++i) {
        SDL_GPUBufferCreateInfo b{ i<4?SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ:SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,sizes[i],0};
        buffers[i]=SDL_CreateGPUBuffer(device,&b);require(buffers[i]);
    }
    U uploadSize=sizes[0]+sizes[1]+sizes[2]+sizes[3];
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,uploadSize,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&ti);require(upload);
    ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=sizes[4]+sizes[5];
    auto* download=SDL_CreateGPUTransferBuffer(device,&ti);require(download);
    auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,false));require(mapped);
    std::array<const void*,4> data{nodes.data(),visibility.data(),faces.data(),trees.data()};
    U offset=0;for(U i=0;i<4;++i) {std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
    SDL_UnmapGPUTransferBuffer(device,upload);
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
    auto* copy=SDL_BeginGPUCopyPass(command);offset=0;
    for(U i=0;i<4;++i) {
        SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{buffers[i],0,sizes[i]};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);offset+=sizes[i];
    }
    SDL_EndGPUCopyPass(copy);
    starfox::render::GpuBspSettings settings{U(trees.size()),U(nodes.size()),U(visibility.size()),U(faces.size()),outputCount,{}};
    const auto output=bsp.enqueue(device,command,buffers[0],buffers[1],buffers[2],buffers[3],settings);
    require(output.order && output.results);
    buffers[4]=static_cast<SDL_GPUBuffer*>(output.order);buffers[5]=static_cast<SDL_GPUBuffer*>(output.results);
    copy=SDL_BeginGPUCopyPass(command);offset=0;
    for(U i=4;i<6;++i) {
        SDL_GPUBufferRegion from{buffers[i],0,sizes[i]};SDL_GPUTransferBufferLocation to{download,offset};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=sizes[i];
    }
    SDL_EndGPUCopyPass(copy);auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
    require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
    const auto* result=static_cast<const U*>(SDL_MapGPUTransferBuffer(device,download,false));require(result);
    const U* headers=result+outputCount;
    for(U t=0;t<failFirst;++t) {
        if(headers[t*2]!=expected[t].size() || headers[t*2+1]) throw std::runtime_error("BSP count/status mismatch");
        for(U i=0;i<expected[t].size();++i) if(result[t*256+i]!=expected[t][i]) throw std::runtime_error("BSP painter order mismatch");
    }
    const U statuses[]{3,4,1,2};
    for(U i=0;i<4;++i) if(headers[(failFirst+i)*2] || headers[(failFirst+i)*2+1]!=statuses[i]) throw std::runtime_error("BSP failure did not fail closed");
    SDL_UnmapGPUTransferBuffer(device,download);
    // Consume resident order directly through clipping, spans and raster. All
    // faces overlap, so the final index must be the last source-ordered face.
    // Failed BSP trees must clear reused row scratch instead of leaking pixels.
    std::array<Four,4> points{{{1,1,256,1},{1,7,256,1},{7,7,256,1},{7,1,256,1}}};
    std::array<Four,4> corners{{{0,0,0,0},{1,0,0,0},{2,0,0,0},{3,0,0,0}}};
    std::array<Four,64> polygons{};
    std::array<starfox::render::RasterCommand,64> materials{};
    U on=1;
    starfox::render::Framebuffer coverage(8,8);
    starfox::assets::Shape shape;starfox::assets::Face face;face.visibility_index=-1;face.normal={0,0,127};
    for(U i=0;i<4;++i) {
        shape.vertices.push_back({-int(points[i][0]),-int(points[i][1]),-256});
        shape.word_coordinates.push_back(true);face.vertex_indices.push_back(std::uint8_t(i));
    }
    shape.faces.push_back(face);shape.colour_words={0x3f99};shape.colour_materials={{0x3f99,{}}};
    starfox::render::RenderPose pose;pose.x=pose.y=pose.z=pose.vanish_x=pose.vanish_y=0;
    pose.use_rotation_matrix=true;pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};
    pose.force_colour=true;pose.forced_colour=0x11;
    starfox::render::SoftwareRenderer renderer;renderer.draw(shape,pose,coverage);
    for(U i=0;i<64;++i) {
        polygons[i]={0,4,0,0};materials[i].even=materials[i].odd=i+1;materials[i].tag=7;
    }
    std::array<U,5> geoSizes{sizeof(points),sizeof(corners),sizeof(polygons),sizeof(materials),4};
    std::array<const void*,5> geoData{points.data(),corners.data(),polygons.data(),materials.data(),&on};
    std::array<SDL_GPUBuffer*,5> geometry{};
    mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require(mapped);offset=0;
    for(U i=0;i<5;++i) {
        SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,geoSizes[i],0};
        geometry[i]=SDL_CreateGPUBuffer(device,&bi);require(geometry[i]);
        std::memcpy(mapped+offset,geoData[i],geoSizes[i]);offset+=geoSizes[i];
    }
    SDL_UnmapGPUTransferBuffer(device,upload);
    starfox::render::GpuClip clip;starfox::render::GpuRaster raster;
    for(U tree:{0U,failFirst,failFirst+1,failFirst+2,failFirst+3,0U}) {
        command=SDL_AcquireGPUCommandBuffer(device);require(command);
        copy=SDL_BeginGPUCopyPass(command);offset=0;
        for(U i=0;i<5;++i) {
            SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{geometry[i],0,geoSizes[i]};
            SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=geoSizes[i];
        }
        SDL_EndGPUCopyPass(copy);
        starfox::render::NativeClipSettings cs{64,4,4,1,8,8,{}};
        require(clip.enqueue(device,command,geometry[0],geometry[1],geometry[2],geometry[4],cs));
        starfox::render::GpuSpanOrder order{output.order,output.results,tree*256,256,tree};
        auto* spans=clip.enqueue_spans(command,geometry[3],true,1,&order);require(spans);
        auto image=raster.enqueue_row_spans(device,command,spans,256,8,8);require(image.pixels);
        copy=SDL_BeginGPUCopyPass(command);
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(image.pixels),0,64*4};
        SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* pixels=static_cast<const U*>(SDL_MapGPUTransferBuffer(device,download,false));require(pixels);
        U drawn=0;
        for(U i=0;i<64;++i) {
            if(bool(pixels[i]&255U)!=(tree==0 && coverage.get(int(i%8),int(i/8))!=0))
                throw std::runtime_error("Resident BSP coverage differs from SoftwareRenderer");
            if(pixels[i]&255U) {
            ++drawn;
            if(tree!=0 || (pixels[i]&255U)!=expected[0].back()+1 || ((pixels[i]>>8)&255U)!=7)
                throw std::runtime_error("Resident BSP-to-raster painter mismatch");
            }
        }
        if(tree==0 && !drawn) throw std::runtime_error("Resident BSP test drew nothing");
        SDL_UnmapGPUTransferBuffer(device,download);
    }
    clip.release_device();raster.release_device();
    for(auto* b:geometry) SDL_ReleaseGPUBuffer(device,b);
    for(U i=0;i<4;++i) SDL_ReleaseGPUBuffer(device,buffers[i]);
    SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
    bsp.release_device();SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<failFirst<<" GPU BSP trees match traversal ("<<realModels<<" decoded models x16 patterns); 4 failure cases pass; resident order-to-raster pixels and reuse pass\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
