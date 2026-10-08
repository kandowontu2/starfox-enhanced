#include "starfox/render/gpu_preparation.hpp"

// Count actual joined resource-creation jobs, not inferred shader names. A
// single projection owner switches policies within one unsubmitted command;
// earlier outputs must stay alive and each already prepared stage be reused.
void check_projection_preparation(SDL_GPUDevice* device) {
    using namespace starfox::render;
    struct Resources {
        SDL_GPUDevice* device;
        std::array<SDL_GPUBuffer*,7> inputs{};
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
    NativeProjectionPoint source{0,0,200,0,48,36,0,0};
    NativeTransformVertex native{};
    NativeTransformPose native_pose{};native_pose.translation[2]=200;
    native_pose.vanish[0]=48;native_pose.vanish[1]=36;
    ContinuousTransformVertex continuous{};
    ContinuousTransformPose continuous_pose{};
    continuous_pose.translation[2]=200;continuous_pose.translation[3]=256;
    continuous_pose.vanish[0]=48;continuous_pose.vanish[1]=36;
    std::array<Uint32,4> face{UINT32_MAX,UINT32_MAX,UINT32_MAX,1};
    ContinuousProjectedPoint motion_source{{0,0,200,1},{48,36,200,1}};
    const std::array<const void*,7> data{&source,&native,&native_pose,&continuous,&continuous_pose,&face,&motion_source};
    const std::array<Uint32,7> sizes{sizeof(source),sizeof(native),sizeof(native_pose),sizeof(continuous),sizeof(continuous_pose),sizeof(face),sizeof(motion_source)};
    Uint32 total{};
    for(unsigned i=0;i<sizes.size();++i) {
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
        owned.inputs[i]=SDL_CreateGPUBuffer(device,&info);require(owned.inputs[i]);total+=sizes[i];
    }
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total,0};
    owned.upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(owned.upload);
    transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=1024;
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
    GpuProjection projection;
    PreparationEvents events{[](void*) noexcept {SDL_PumpEvents();},nullptr};
    {
        ScopedPreparationEvents scope(&events);
        // Invalid requests cannot compile even one unused shader.
        require(!projection.enqueue_continuous_visibility(owned.command,owned.inputs[5],1));
        require(events.jobs==0);
        auto* motion=projection.enqueue_motion(device,owned.command,owned.inputs[6],owned.inputs[6],1,1,1);
        require(motion && events.jobs==1);capture(motion,16);
        auto* points=projection.enqueue_continuous(device,owned.command,owned.inputs[3],1,owned.inputs[4],1);
        require(points && events.jobs==2);capture(points,32);
        auto* visible=projection.enqueue_continuous_visibility(owned.command,owned.inputs[5],1);
        require(visible && events.jobs==3);capture(visible,4);
        void* camera{};
        points=projection.enqueue_transformed(device,owned.command,owned.inputs[1],1,owned.inputs[2],1,&camera);
        require(points && camera && events.jobs==5);capture(points,16);
        visible=projection.enqueue_visibility(owned.command,owned.inputs[5],1);
        require(visible && events.jobs==6);capture(visible,4);
        points=projection.enqueue_continuous(device,owned.command,owned.inputs[3],1,owned.inputs[4],1);
        require(points && events.jobs==6);capture(points,32);
        require(!projection.enqueue_visibility(owned.command,owned.inputs[5],1) && events.jobs==6);
        visible=projection.enqueue_continuous_visibility(owned.command,owned.inputs[5],1);
        require(visible && events.jobs==6);capture(visible,4);
        points=projection.enqueue(device,owned.command,owned.inputs[0],1);
        require(points && events.jobs==6);capture(points,16);
        require(!projection.enqueue_continuous_visibility(owned.command,owned.inputs[5],1) && events.jobs==6);
        ScaledTextRenderer::ProjectedFrame blank;
        auto* text=projection.enqueue_text(device,owned.command,blank,8,8,1,3);
        require(text && events.jobs==7);capture(text,8*8*4);
        require(projection.enqueue_motion(device,owned.command,owned.inputs[6],owned.inputs[6],1,1,1) && events.jobs==7);
        // Text must not have destroyed any projected points/pipelines. Likewise,
        // switching back to a word stage cannot retire the queued text/motion.
        require(projection.enqueue_visibility(owned.command,owned.inputs[5],1) && events.jobs==7);
        capture(text,8*8*4);capture(motion,16);
    }
    auto* submitted=owned.command;owned.command=nullptr;
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(fence);
    require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
    const auto* pixels=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(pixels);
    Uint32 read{};
    const auto floats=[&](std::array<float,4> expected) {
        std::array<float,4> actual;std::memcpy(actual.data(),pixels+read,16);read+=16;
        for(unsigned i=0;i<4;++i) require(actual[i]==expected[i]);
    };
    const auto words=[&] {
        std::array<std::int32_t,4> actual;std::memcpy(actual.data(),pixels+read,16);read+=16;
        require(actual==std::array<std::int32_t,4>{48,36,200,1});
    };
    const auto visibility=[&] {Uint32 flag;std::memcpy(&flag,pixels+read,4);read+=4;require(flag==1);};
    floats({0,0,200,1});floats({0,0,200,1});floats({48,36,200,1});visibility();
    words();visibility();floats({0,0,200,1});floats({48,36,200,1});visibility();words();
    for(unsigned i=0;i<8*8*4*2;++i) require(pixels[read+i]==0);
    read+=8*8*4*2;floats({0,0,200,1});require(read==offset);
    SDL_UnmapGPUTransferBuffer(device,owned.download);
    // Cancellation and release must not make a previously compiled stage look
    // warm on a new lifetime. No submission is necessary to compile a stage.
    projection.release_device();
    for(unsigned repeat=0;repeat<2;++repeat) {
        owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
        ScopedPreparationEvents scope(&events);
        require(projection.enqueue(device,owned.command,owned.inputs[0],1) && events.jobs==8+repeat);
        require(SDL_CancelGPUCommandBuffer(owned.command));owned.command=nullptr;
        projection.release_device();
    }
    std::cout<<"Projection preparation: only requested shader stages created; warm policy switches, exact queued motion/word/continuous/visibility/text outputs, invalid requests, cancellation and release/recreate passed\n";
}
