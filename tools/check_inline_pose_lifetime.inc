// Host pose arrays may be reused immediately: uniforms belong to each recorded
// dispatch, including unsubmitted/cancelled commands and queued GPU versions.
void check_inline_pose_lifetime(SDL_GPUDevice* device) {
    using namespace starfox::render;
    struct Owned {
        SDL_GPUDevice* device;
        SDL_GPUBuffer *native{},*continuous{};
        SDL_GPUTransferBuffer* upload{};
        std::array<SDL_GPUTransferBuffer*,3> downloads{};
        std::array<SDL_GPUFence*,3> fences{};
        SDL_GPUCommandBuffer* command{};
        ~Owned() {
            if(command) SDL_CancelGPUCommandBuffer(command);
            SDL_WaitForGPUIdle(device);
            for(auto* fence:fences) if(fence) SDL_ReleaseGPUFence(device,fence);
            for(auto* download:downloads) if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(native) SDL_ReleaseGPUBuffer(device,native);
            if(continuous) SDL_ReleaseGPUBuffer(device,continuous);
        }
    } owned{device};
    const NativeTransformVertex native{4,-2,1,0};
    const ContinuousTransformVertex continuous{4,-2,1,0};
    SDL_GPUBufferCreateInfo buffer{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,16,0};
    owned.native=SDL_CreateGPUBuffer(device,&buffer);require(owned.native);
    owned.continuous=SDL_CreateGPUBuffer(device,&buffer);require(owned.continuous);
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,32,0};
    owned.upload=SDL_CreateGPUTransferBuffer(device,&transfer);require(owned.upload);
    auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,owned.upload,true));require(mapped);
    std::memcpy(mapped,&native,16);std::memcpy(mapped+16,&continuous,16);
    SDL_UnmapGPUTransferBuffer(device,owned.upload);
    owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
    auto* copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
    SDL_GPUTransferBufferLocation from{owned.upload,0};SDL_GPUBufferRegion to{owned.native,0,16};
    SDL_UploadToGPUBuffer(copy,&from,&to,true);from.offset=16;to={owned.continuous,0,16};
    SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
    GpuProjection projection;
    NativeTransformPose native_pose{};
    native_pose.row0[0]=native_pose.row1[1]=native_pose.row2[2]=-32768;
    native_pose.vanish[0]=48;native_pose.vanish[1]=36;
    ContinuousTransformPose continuous_pose{};
    continuous_pose.row0[0]=continuous_pose.row1[1]=continuous_pose.row2[2]=1;
    continuous_pose.translation[3]=256;continuous_pose.vanish[0]=48;continuous_pose.vanish[1]=36;
    constexpr unsigned steps=6,bytes_per_step=48;
    transfer.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;transfer.size=steps*bytes_per_step;
    for(auto& download:owned.downloads){download=SDL_CreateGPUTransferBuffer(device,&transfer);require(download);}
    for(unsigned pair=0;pair<3;++pair) {
        if(pair) {owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);}
        for(unsigned step=0;step<steps;++step) {
            const int x=int(pair)*11+int(step)*3-9,z=200+int(pair)*20+int(step);
            native_pose.translation[0]=x;native_pose.translation[2]=z;
            void* camera{};
            auto* projected=projection.enqueue_transformed(device,owned.command,owned.native,1,nullptr,1,&camera,
                std::span<const NativeTransformPose>(&native_pose,1));require(projected && camera);
            native_pose.translation[0]=9999;native_pose.translation[2]=-9999;
            copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(projected),0,16};
            SDL_GPUTransferBufferLocation target{owned.downloads[pair],step*bytes_per_step};
            SDL_DownloadFromGPUBuffer(copy,&source,&target);SDL_EndGPUCopyPass(copy);
            continuous_pose.translation[0]=float(x)+.25f;continuous_pose.translation[2]=float(z);
            projected=projection.enqueue_continuous(device,owned.command,owned.continuous,1,nullptr,1,nullptr,false,
                std::span<const ContinuousTransformPose>(&continuous_pose,1));require(projected);
            continuous_pose.translation[0]=9999;continuous_pose.translation[2]=-9999;
            copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
            source={static_cast<SDL_GPUBuffer*>(projected),0,32};target.offset+=16;
            SDL_DownloadFromGPUBuffer(copy,&source,&target);SDL_EndGPUCopyPass(copy);
        }
        auto* submitted=owned.command;owned.command=nullptr;
        owned.fences[pair]=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(owned.fences[pair]);
    }
    require(SDL_WaitForGPUFences(device,true,owned.fences.data(),owned.fences.size()));
    for(unsigned pair=0;pair<3;++pair) {
        const auto* bytes=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,owned.downloads[pair],false));require(bytes);
        for(unsigned step=0;step<steps;++step) {
            const int x=int(pair)*11+int(step)*3-9,z=200+int(pair)*20+int(step);
            std::array<int,4> actual{};std::memcpy(actual.data(),bytes+step*bytes_per_step,16);
            NativeProjectionPoint expected{x-4,2,z-1,0,48,36,0,0};require(actual==reference(expected));
            ContinuousProjectedPoint fractional{};std::memcpy(&fractional,bytes+step*bytes_per_step+16,32);
            require(fractional.camera[0]==float(x)+4.25f && fractional.camera[1]==-2 && fractional.camera[2]==float(z)+1 && fractional.camera[3]==1);
            const double sx=48+(double(x)+4.25)*256/(z+1),sy=36-2.*256/(z+1);
            require(std::abs(fractional.screen[0]-sx)<.00002 && std::abs(fractional.screen[1]-sy)<.00002 && fractional.screen[3]==1);
        }
        SDL_UnmapGPUTransferBuffer(device,owned.downloads[pair]);
    }
    // Cancel a different constant payload and retry using the same CPU object.
    owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
    require(projection.enqueue_transformed(device,owned.command,owned.native,1,nullptr,1,nullptr,std::span<const NativeTransformPose>(&native_pose,1)));
    require(SDL_CancelGPUCommandBuffer(owned.command));owned.command=nullptr;
    owned.command=SDL_AcquireGPUCommandBuffer(device);require(owned.command);
    native_pose.translation[0]=8;native_pose.translation[2]=201;
    auto* output=projection.enqueue_transformed(device,owned.command,owned.native,1,nullptr,1,nullptr,std::span<const NativeTransformPose>(&native_pose,1));require(output);
    native_pose.translation[0]=-9999;
    copy=SDL_BeginGPUCopyPass(owned.command);require(copy);
    SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output),0,16};SDL_GPUTransferBufferLocation target{owned.downloads[0],0};
    SDL_DownloadFromGPUBuffer(copy,&source,&target);SDL_EndGPUCopyPass(copy);
    auto* submitted=owned.command;owned.command=nullptr;
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(submitted);require(fence);
    require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
    const auto* actual=static_cast<const std::array<int,4>*>(SDL_MapGPUTransferBuffer(device,owned.downloads[0],false));require(actual);
    require(*actual==reference(NativeProjectionPoint{4,2,200,0,48,36,0,0}));
    SDL_UnmapGPUTransferBuffer(device,owned.downloads[0]);projection.release_device();
    std::cout<<"Inline model poses: 36 independent queued native/continuous outputs, immediate host mutation, policy switching and cancelled-command retry passed\n";
}
