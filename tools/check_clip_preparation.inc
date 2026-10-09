#include "starfox/render/gpu_preparation.hpp"

// Independently authored square/row expectations, not comparison against a
// second GPU implementation. Downloads precede policy switches in the SAME
// unsubmitted command, so stage creation cannot discard earlier queued work.
void check_clip_preparation(SDL_GPUDevice* device,bool fractional_first=false) {
    using namespace starfox::render;
    PreparationEvents events{[](void*) noexcept {SDL_PumpEvents();},nullptr};
    std::string phase="input setup";
    const auto require=[&](bool ok) {
        if(!ok) throw std::runtime_error("Clipping preparation "+std::string(fractional_first?"continuous":"native")
            +" first, "+phase+", jobs="+std::to_string(events.jobs)+": "+SDL_GetError());
    };
    struct Environment {
        const std::array<const char*,3> keys{"STARFOX_TEST_FULL_CLIP","STARFOX_TEST_FULL_SPAN_TRACE","STARFOX_TEST_FULL_SPAN_CLEAR"};
        std::array<std::optional<std::string>,3> saved;
        Environment() {
            for(unsigned i=0;i<keys.size();++i) {
                if(const auto* value=SDL_getenv(keys[i])) saved[i]=value;
                SDL_SetEnvironmentVariable(SDL_GetEnvironment(),keys[i],"1",true);
            }
        }
        ~Environment() {
            for(unsigned i=0;i<keys.size();++i) {
                if(saved[i]) SDL_SetEnvironmentVariable(SDL_GetEnvironment(),keys[i],saved[i]->c_str(),true);
                else SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),keys[i]);
            }
        }
    } environment;
    struct Resources {
        SDL_GPUDevice* device;
        std::array<SDL_GPUBuffer*,6> inputs{};
        SDL_GPUTransferBuffer *upload{},*download{};
        SDL_GPUCommandBuffer* command{};
        ~Resources() {
            if(command) SDL_CancelGPUCommandBuffer(command);
            SDL_WaitForGPUIdle(device);
            for(auto* input:inputs) if(input) SDL_ReleaseGPUBuffer(device,input);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        }
    } owned{device};
    constexpr Uint32 width=40,height=32,clip_bytes=129*16,span_bytes=height*96;
    const std::array<V,4> native{{{8,8,256,1},{24,8,256,1},{24,24,256,1},{8,24,256,1}}};
    const std::array<ContinuousProjectedPoint,4> fractional{{
        {{8,8,256,1},{8,8,256,1}},{{24,8,256,1},{24,8,256,1}},
        {{24,24,256,1},{24,24,256,1}},{{8,24,256,1},{8,24,256,1}}}};
    const std::array<std::array<Uint32,4>,4> corners{{{0,0,0,0},{1,7,0,0},{2,7,7,0},{3,0,7,0}}};
    const std::array<Uint32,4> polygon{0,4,0,0};
    const Uint32 visibility=1;
    RasterCommand material{};material.even=material.odd=7;material.tag=1;
    material.has_surface=1;material.surface={0,0,-1,256};
    const std::array<const void*,6> data{native.data(),fractional.data(),corners.data(),&polygon,&visibility,&material};
    const std::array<Uint32,6> sizes{sizeof(native),sizeof(fractional),sizeof(corners),sizeof(polygon),sizeof(visibility),sizeof(material)};
    Uint32 total{};
    for(unsigned i=0;i<sizes.size();++i) {
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
        owned.inputs[i]=SDL_CreateGPUBuffer(device,&info);require(owned.inputs[i]);total+=sizes[i];
    }
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total,0};
    owned.upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(owned.upload);
    transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=clip_bytes*3+span_bytes*2;
    owned.download=SDL_CreateGPUTransferBuffer(device,&transfer);require(owned.download);
    auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,owned.upload,true));require(mapped);
    Uint32 offset{};
    for(unsigned i=0;i<sizes.size();++i) {std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
    SDL_UnmapGPUTransferBuffer(device,owned.upload);
    owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
    auto* copy=SDL_BeginGPUCopyPass(owned.command);require(copy);offset=0;
    for(unsigned i=0;i<sizes.size();++i) {
        SDL_GPUTransferBufferLocation from{owned.upload,offset};SDL_GPUBufferRegion to{owned.inputs[i],0,sizes[i]};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=sizes[i];
    }
    SDL_EndGPUCopyPass(copy);offset=0;
    const auto capture=[&](void* buffer,Uint32 bytes) {
        require(buffer);auto* pass=SDL_BeginGPUCopyPass(owned.command);require(pass);
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(buffer),0,bytes};SDL_GPUTransferBufferLocation to{owned.download,offset};
        SDL_DownloadFromGPUBuffer(pass,&from,&to);SDL_EndGPUCopyPass(pass);offset+=bytes;
    };
    GpuClip clip;
    const NativeClipSettings settings{1,4,4,1,int(width),int(height)};
    const auto enqueue=[&](bool fractional) {
        return clip.enqueue(device,owned.command,owned.inputs[fractional?1:0],owned.inputs[2],owned.inputs[3],owned.inputs[4],settings,fractional);
    };
    {
        ScopedPreparationEvents scope(&events);
        phase="invalid initial requests";
        require(!clip.enqueue_spans(owned.command,owned.inputs[5]) && events.jobs==0);
        auto invalid=settings;invalid.polygon_count=0;
        require(!clip.enqueue(device,owned.command,owned.inputs[0],owned.inputs[2],owned.inputs[3],owned.inputs[4],invalid) && events.jobs==0);
        phase="first clip";require(enqueue(fractional_first) && events.jobs==1);
        capture(enqueue(fractional_first),clip_bytes);require(events.jobs==1);
        phase="first spans";capture(clip.enqueue_spans(owned.command,owned.inputs[5],true),span_bytes);require(events.jobs==2);
        phase="other clip policy";capture(enqueue(!fractional_first),clip_bytes);require(events.jobs==3);
        phase="warm spans";capture(clip.enqueue_spans(owned.command,owned.inputs[5],true),span_bytes);require(events.jobs==3);
        capture(enqueue(fractional_first),clip_bytes);require(events.jobs==3);
        require(!clip.enqueue_spans(owned.command,owned.inputs[5],true,0) && events.jobs==3);
    }
    phase="submit/download";auto* submitted=owned.command;owned.command=nullptr;
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(fence);
    require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
    const auto* downloaded=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(downloaded);
    Uint32 read{};
    const auto clipped=[&](bool fractional) {
        V header;std::memcpy(&header,downloaded+read,16);
        phase="clipped header "+std::to_string(header[0])+","+std::to_string(header[1]);
        require(header==V{4,0,0,0});
        const std::array<V,4> expected{{{8,8,0,0},{24,8,7,0},{24,24,7,7},{8,24,0,7}}};
        for(unsigned i=0;i<expected.size();++i) {
            phase="clipped corner "+std::to_string(i);
            if(fractional) {
                std::array<float,4> actual;std::memcpy(&actual,downloaded+read+(i+1)*16,16);
                for(unsigned c=0;c<4;++c) require(actual[c]==float(expected[i][c]));
            } else {
                V actual;std::memcpy(&actual,downloaded+read+(i+1)*16,16);require(actual==expected[i]);
            }
        }
        read+=clip_bytes;
    };
    const auto spans=[&] {
        for(unsigned y=0;y<height;++y) {
            RasterCommand row;std::memcpy(&row,downloaded+read+y*sizeof(row),sizeof(row));
            phase="span row "+std::to_string(y)+" bounds "+std::to_string(row.left)+","+std::to_string(row.right)
                +","+std::to_string(row.top)+","+std::to_string(row.bottom);
            if(y>=8 && y<24) {
                // SNES solid spans include the right edge pixel (x2 + 1),
                // while the bottom edge is exclusive. Keep the source rule.
                require(row.left==8 && row.right==25 && row.top==int(y) && row.bottom==int(y+1));
                require(row.even==7 && row.odd==7 && row.tag==1 && row.has_surface==1 && row.surface==material.surface);
            } else require(row.left==0 && row.right==0 && row.top==0 && row.bottom==0);
        }
        read+=span_bytes;
    };
    clipped(fractional_first);spans();clipped(!fractional_first);spans();clipped(fractional_first);
    require(read==offset);SDL_UnmapGPUTransferBuffer(device,owned.download);
    clip.release_device();
    for(unsigned repeat=0;repeat<2;++repeat) {
        phase="cancel/recreate "+std::to_string(repeat);
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
        ScopedPreparationEvents scope(&events);
        require(!clip.enqueue_spans(owned.command,owned.inputs[5]) && events.jobs==3+repeat);
        require(enqueue(fractional_first) && events.jobs==4+repeat);
        require(SDL_CancelGPUCommandBuffer(owned.command));owned.command=nullptr;
        clip.release_device();
    }
    std::cout<<"Clipping preparation: "<<(fractional_first?"continuous":"native")
        <<" first creates one shader; spans/other policy independently lazy; exact queued polygons/row coverage/materials, warm switches, invalid requests and cancel/recreate passed\n";
}
