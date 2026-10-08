// GPU-written dispatch/list oracle plus exact packed-pixel comparison. Includes
// changing extents on one cache and the native dispatch-X limit on real GPUs.
namespace {
void check_occupied_tiles(SDL_GPUDevice* device) {
    using namespace starfox::render;
    struct Scratch {
        SDL_GPUDevice* device;
        std::vector<SDL_GPUBuffer*> buffers;
        SDL_GPUTransferBuffer *upload{},*download{};
        ~Scratch() {
            SDL_WaitForGPUIdle(device);
            for(auto* b:buffers) SDL_ReleaseGPUBuffer(device,b);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        }
    } scratch{device};
    const auto check=[](bool ok) {if(!ok) throw std::runtime_error("Occupied-tile GPU oracle: "+std::string(SDL_GetError()));};
    constexpr unsigned max_width=2048,max_height=2049,max_pixels=max_width*max_height;
    constexpr unsigned span_bytes=max_height*2*sizeof(RasterCommand);
    const auto make_buffer=[&](unsigned bytes) {
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes,0};
        auto* result=SDL_CreateGPUBuffer(device,&info);check(result);scratch.buffers.push_back(result);return result;
    };
    auto* background_spans=make_buffer(span_bytes);
    auto* foreground_spans=make_buffer(span_bytes);
    SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,span_bytes*2,0};
    scratch.upload=SDL_CreateGPUTransferBuffer(device,&transfer);check(scratch.upload);
    transfer={SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,max_pixels*8+(32*max_height+1)*4+16,0};
    scratch.download=SDL_CreateGPUTransferBuffer(device,&transfer);check(scratch.download);
    GpuRaster first_background,second_background,copy_raster,occupied_raster;
    unsigned long long exact_pixels=0,total_tiles=0;
    for(unsigned fixture=0;fixture<7;++fixture) {
        const unsigned width=fixture==3 || fixture>=5?2048:fixture==2?197:129;
        const unsigned height=fixture==3?2049:fixture>=5?2048:fixture==2?13:7;
        const unsigned pixels=width*height,columns=(width+63)/64,capacity=columns*height;
        const bool indirect=!SDL_getenv("STARFOX_TEST_DISABLE_OCCUPIED_TILES")
            && !SDL_getenv("STARFOX_TEST_DISABLE_TILED_SPANS")
            && SDL_getenv("STARFOX_TEST_OCCUPIED_TILES");
        std::vector<RasterCommand> back(height),front(height*2);
        std::vector<unsigned> expected(pixels,0x0400025bU);
        std::vector<bool> occupied(capacity);
        for(unsigned row=0;row<height;++row) {
            auto& b=back[row];b.left=0;b.right=width;b.top=row;b.bottom=row+1;b.even=b.odd=91;b.tag=2;
            for(unsigned polygon=0;polygon<2;++polygon) {
                auto& c=front[polygon*height+row];c.top=row;c.bottom=row+1;c.tag=polygon?1:2;
                c.even=polygon?0:31;c.odd=polygon?0:67;c.dither=polygon?0:1;
                if(fixture==1 && row==1 && polygon==0) {c.left=64;c.right=65;}
                if(fixture==2) {
                    c.left=int((row*37+polygon*11)%width)-20;c.right=c.left+int(polygon?23:83);
                }
                if((fixture==3 || fixture>=5) && polygon==0) {
                    c.right=fixture==5 && row==height-1?1984:width;
                }
                const int left=std::max(0,c.left),right=std::min(int(width),c.right);
                for(int x=left;x<right;++x) {
                    const unsigned colour=c.dither && ((x^row)&1)?c.odd:c.even;
                    expected[row*width+x]=0x04000000U|(c.tag<<8)|colour;
                    occupied[row*columns+unsigned(x)/64]=true;
                }
            }
        }
        unsigned count=0;for(bool tile:occupied) count+=tile;
        auto* data=static_cast<std::byte*>(SDL_MapGPUTransferBuffer(device,scratch.upload,true));check(data);
        const auto back_bytes=unsigned(back.size()*sizeof(back[0])),front_bytes=unsigned(front.size()*sizeof(front[0]));
        std::memcpy(data,back.data(),back_bytes);std::memcpy(data+back_bytes,front.data(),front_bytes);
        SDL_UnmapGPUTransferBuffer(device,scratch.upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(device);check(cmd);
        auto* copy=SDL_BeginGPUCopyPass(cmd);check(copy);
        SDL_GPUTransferBufferLocation from{scratch.upload,0};SDL_GPUBufferRegion to{background_spans,0,back_bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);from.offset=back_bytes;to={foreground_spans,0,front_bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        const auto a=first_background.enqueue_row_spans(device,cmd,background_spans,1,width,height,false,nullptr,true);
        const auto b=second_background.enqueue_row_spans(device,cmd,background_spans,1,width,height,false,nullptr,true);
        check(a.pixels && b.pixels && a.row_span_canonical && b.row_span_canonical);
        const auto reference=copy_raster.enqueue_row_spans(device,cmd,foreground_spans,2,width,height,false,nullptr,true,&a);
        const auto actual=occupied_raster.enqueue_row_spans(device,cmd,foreground_spans,2,width,height,false,nullptr,true,&b,false,0,0,0,nullptr,{},{},0,true);
        check(reference.pixels && actual.pixels==b.pixels && actual.row_span_canonical);
        const auto dispatch=occupied_raster.dispatch_info();
        check(bool(dispatch.arguments)==indirect && bool(dispatch.tiles)==indirect);
        copy=SDL_BeginGPUCopyPass(cmd);check(copy);
        unsigned offset=0;
        for(void* result:{reference.pixels,actual.pixels}) {
            to={static_cast<SDL_GPUBuffer*>(result),0,pixels*4};SDL_GPUTransferBufferLocation dest{scratch.download,offset};
            SDL_DownloadFromGPUBuffer(copy,&to,&dest);offset+=pixels*4;
        }
        if(indirect) {
            check(dispatch.device==device && dispatch.tile_capacity==capacity);
            to={static_cast<SDL_GPUBuffer*>(dispatch.arguments),0,16};SDL_GPUTransferBufferLocation dest{scratch.download,offset};
            SDL_DownloadFromGPUBuffer(copy,&to,&dest);offset+=16;
            to={static_cast<SDL_GPUBuffer*>(dispatch.tiles),0,(capacity+1)*4};dest.offset=offset;
            SDL_DownloadFromGPUBuffer(copy,&to,&dest);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);check(fence);
        check(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* words=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(device,scratch.download,false));check(words);
        check(std::memcmp(words,expected.data(),pixels*4)==0 && std::memcmp(words+pixels,expected.data(),pixels*4)==0);
        if(indirect) {
            const auto* args=words+pixels*2;
            check(args[0]==std::max(1U,std::min(count,65535U)) && args[1]==std::max(1U,(count+65534)/65535)
                && args[2]==1 && args[3]==0 && args[4]==count);
            std::vector<bool> seen(capacity);
            for(unsigned i=0;i<count;++i) {
                const auto tile=args[5+i];check(tile<capacity && occupied[tile] && !seen[tile]);seen[tile]=true;
            }
            check(seen==occupied);
        }
        SDL_UnmapGPUTransferBuffer(device,scratch.download);
        exact_pixels+=pixels;total_tiles+=count;
        std::cout<<"Occupied tiles fixture="<<fixture<<" active="<<count<<" capacity="<<capacity
            <<" indirect="<<indirect<<" passed\n";
    }
    occupied_raster.release_device();check(!occupied_raster.dispatch_info().arguments);
    std::cout<<"Occupied-tile raster: "<<exact_pixels<<" exact pixels, "<<total_tiles
        <<" unique occupied IDs; empty, single tile, overlaps, odd extents, cache reuse, 65535/65536/65568 group boundaries pass\n";
}
}
