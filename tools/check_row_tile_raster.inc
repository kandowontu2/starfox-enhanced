// Independent forward painter reference plus exact old/new GPU plane parity.
// These fixture-only downloads never enter the production path.
void check_row_tile_raster(SDL_GPUDevice* device,bool metadata=true,bool mask_tiles=false) {
    using namespace starfox::render;
    constexpr unsigned max_width=197,height=13,max_polygons=65;
    struct Owned {
        SDL_GPUDevice* device;
        SDL_GPUBuffer *spans{},*back{},*texels{},*planes{};
        SDL_GPUTransferBuffer *upload{},*download{};
        ~Owned() {
            SDL_WaitForGPUIdle(device);
            for(auto* p:{spans,back,texels,planes}) if(p) SDL_ReleaseGPUBuffer(device,p);
            for(auto* p:{upload,download}) if(p) SDL_ReleaseGPUTransferBuffer(device,p);
        }
    } owned{device};
    const auto make=[&](unsigned bytes) {
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes,0};
        auto* result=SDL_CreateGPUBuffer(device,&info);require(result);return result;
    };
    const unsigned span_bytes=height*max_polygons*sizeof(RasterCommand),back_bytes=height*sizeof(RasterCommand);
    owned.spans=make(span_bytes);owned.back=make(back_bytes);owned.texels=make(64);owned.planes=make(32);
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,span_bytes+back_bytes+96,0};
    owned.upload=SDL_CreateGPUTransferBuffer(device,&info);require(owned.upload);
    info={SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,max_width*height*24*2,0};
    owned.download=SDL_CreateGPUTransferBuffer(device,&info);require(owned.download);
    GpuRaster backgrounds[2],painters[2];
    auto* env=SDL_GetEnvironment();
    struct RestoreMasks {
        SDL_Environment* env;
        std::array<const char*,3> names{"STARFOX_TEST_MASK_TILE_RASTER","STARFOX_TEST_DISABLE_MASK_TILE_RASTER","STARFOX_TEST_OCCUPIED_TILES"};
        std::array<std::optional<std::string>,3> values;
        explicit RestoreMasks(SDL_Environment* e):env(e) {
            for(unsigned i=0;i<names.size();++i) if(const auto* value=SDL_GetEnvironmentVariable(env,names[i])) values[i]=value;
            for(auto* name:names) SDL_UnsetEnvironmentVariable(env,name);
        }
        ~RestoreMasks() {for(unsigned i=0;i<names.size();++i) {
            if(values[i]) SDL_SetEnvironmentVariable(env,names[i],values[i]->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,names[i]);
        }}
    } restore_masks{env};
    const auto* enable=SDL_GetEnvironmentVariable(env,"STARFOX_TEST_ROW_TILE_RASTER");
    const std::optional<std::string> saved_enable=enable?std::optional<std::string>{enable}:std::nullopt;
    const auto* setting=SDL_GetEnvironmentVariable(env,"STARFOX_TEST_DISABLE_ROW_TILE_RASTER");
    const std::optional<std::string> saved=setting?std::optional<std::string>{setting}:std::nullopt;
    const auto* generic=SDL_GetEnvironmentVariable(env,"STARFOX_TEST_GENERIC_RASTER");
    const std::optional<std::string> saved_generic=generic?std::optional<std::string>{generic}:std::nullopt;
    const auto* pixel=SDL_GetEnvironmentVariable(env,"STARFOX_TEST_PIXEL_ONLY_RASTER");
    const std::optional<std::string> saved_pixel=pixel?std::optional<std::string>{pixel}:std::nullopt;
    const auto* single=SDL_GetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_TILED_SPANS");
    const std::optional<std::string> saved_single=single?std::optional<std::string>{single}:std::nullopt;
    const auto* direct=SDL_GetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_RASTER");
    const std::optional<std::string> saved_direct=direct?std::optional<std::string>{direct}:std::nullopt;
    struct Restore {
        SDL_Environment* env;const std::optional<std::string>& saved;const std::optional<std::string>& saved_enable;
        const std::optional<std::string>& saved_generic;
        const std::optional<std::string>& saved_pixel;
        const std::optional<std::string>& saved_single;
        const std::optional<std::string>& saved_direct;
        ~Restore() {if(saved) SDL_SetEnvironmentVariable(env,"STARFOX_TEST_DISABLE_ROW_TILE_RASTER",saved->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_DISABLE_ROW_TILE_RASTER");
            if(saved_enable) SDL_SetEnvironmentVariable(env,"STARFOX_TEST_ROW_TILE_RASTER",saved_enable->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_ROW_TILE_RASTER");
            if(saved_generic) SDL_SetEnvironmentVariable(env,"STARFOX_TEST_GENERIC_RASTER",saved_generic->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_GENERIC_RASTER");
            if(saved_pixel) SDL_SetEnvironmentVariable(env,"STARFOX_TEST_PIXEL_ONLY_RASTER",saved_pixel->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_PIXEL_ONLY_RASTER");
            if(saved_single) SDL_SetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_TILED_SPANS",saved_single->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_TILED_SPANS");
            if(saved_direct) SDL_SetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_RASTER",saved_direct->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_RASTER");}
    } restore{env,saved,saved_enable,saved_generic,saved_pixel,saved_single,saved_direct};
    require(metadata && !mask_tiles?SDL_SetEnvironmentVariable(env,"STARFOX_TEST_ROW_TILE_RASTER","1",true):
        SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_ROW_TILE_RASTER"));
    require(metadata || mask_tiles?SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_PIXEL_ONLY_RASTER"):
        SDL_SetEnvironmentVariable(env,"STARFOX_TEST_PIXEL_ONLY_RASTER","1",true));
    std::uint64_t checked=0;unsigned cases=0,direct_faces=0;
    for(unsigned width:{1U,63U,64U,65U,129U,197U})
    for(unsigned count:{0U,1U,2U,63U,64U,65U})
    for(unsigned policy=0;policy<(metadata?4U:10U);++policy) {
        const bool wave=policy==4 || policy==5;
        const bool remap=policy>=6;
        const unsigned output_width=remap?width+7:width,output_height=remap?height+3:height;
        const std::array<float,2> jitter=policy==8?std::array<float,2>{.75f,-.5f}:
            policy==9?std::array<float,2>{-1.25f,2.f}:std::array<float,2>{};
        constexpr int wave_offset=-32750;constexpr unsigned wave_frame=13;
        const unsigned pixels=output_width*output_height;
        std::vector<RasterCommand> source(count*height),back(height);
        std::array<unsigned char,64> artwork{};
        for(unsigned i=0;i<64;++i) artwork[i]=i%5==0?0:1+i%11;
        const std::array<std::array<float,4>,2> planes{{{0,0,1,900},{0,0,1,250}}};
        struct Pixel {unsigned color=17,tag=2,pair=0,surface_color=17;bool covered=false,pair_valid=false;
            bool valid=true;std::array<float,4> surface{0,1,0,42};float depth=900;};
        std::vector<Pixel> expected(width*height);
        if(!metadata) for(auto& p:expected) {p.valid=false;p.surface_color=0;p.surface={0,0,1,0};p.depth=0;}
        // Independent extents/jitter cannot fuse a source-sized background.
        // Those fixtures start empty, as the public enqueue contract requires.
        if(remap) for(auto& p:expected) p.color=p.tag=0;
        for(unsigned row=0;row<height;++row) {
            auto& b=back[row];b.left=0;b.right=width;b.top=row;b.bottom=row+1;
            b.even=b.odd=17;b.tag=2;b.has_surface=metadata?3:0;b.surface={0,1,0,42};
            for(unsigned p=0;p<count;++p) {
                auto& c=source[p*height+row];c.top=row;c.bottom=row+1;
                c.left=int((p*31+row*7)%(width+10))-12;c.right=c.left+23+int(p%5)*17;
                if(row==4 || (p+row)%13==0) c.right=c.left; // Completely empty row/tiles.
                c.even=p%9==0?0:31+p;c.odd=90+p;c.dither=p%3==0;c.reserved0=2;
                c.tag=p%3;c.has_surface=metadata?(p%4==0?5:4):0;c.surface={1,0,0,float(20+p)};
                if(p%4==1 || (count==1 && (policy&1))) {c.textured=1;c.u_mask=c.v_mask=7;c.colour_base=64;
                    c.u=-257;c.v=int(row)*256;c.du=173;c.dv=-37;c.reserved0=259;c.reserved1=UINT32_MAX-2;}
                if(wave && !c.textured && p%2==0) c.reserved1|=2;
            }
        }
        // Independent forward painter; wave rows move into destination rows,
        // unlike the shader's inverse source-row lookup. Dither uses output Y.
        for(unsigned p=0;p<count;++p) for(unsigned row=0;row<height;++row) {
                const auto& c=source[p*height+row];
                for(int x=std::max(0,c.left);x<std::min(int(width),c.right);++x) {
                    int y=int(row);
                    if(wave && !c.textured && (c.reserved1&2)) {
                        static constexpr int sine[]{0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
                        const auto source_phase=std::int16_t(wave_offset+x);
                        const auto phase=std::int16_t((source_phase>>1)+int(wave_frame&15)-1);
                        y+=sine[unsigned(phase)&15];
                    }
                    if(y<0 || y>=int(height)) continue;
                    unsigned color=c.dither && (((x/2)^(y/2))&1)?c.odd:c.even;
                    if(c.textured) {
                        const unsigned dx=unsigned(x-c.left);
                        const unsigned u=((((unsigned(c.u)+unsigned(c.du)*dx)&65535)>>8)+c.reserved0)&7;
                        const unsigned v=((((unsigned(c.v)+unsigned(c.dv)*dx)&65535)>>8)+c.reserved1)&7;
                        if(!artwork[v*8+u]) continue;
                        color=64+artwork[v*8+u];
                    }
                    auto& e=expected[unsigned(y)*width+unsigned(x)];e.color=color;e.tag=c.tag;e.covered=true;
                    e.pair_valid=!c.textured && !c.tag && c.dither && c.even!=c.odd;
                    e.pair=e.pair_valid?(color==c.even?c.odd:c.even):0;
                    e.depth=metadata?250:0;
                    if(c.has_surface&1) {e.valid=true;e.surface=c.surface;e.surface_color=color;}
                }
        }
        const bool in_place=!remap && (policy&1);const unsigned flags=policy&2?2:1;
        for(auto& p:expected) if(p.covered && flags==2) {p.valid=false;p.surface={0,0,1,0};p.surface_color=0;}
        std::vector<bool> sample_valid(pixels,true);
        if(remap) {
            // Independent rational sample mapping, with no production jitter
            // helper. Nonzero jitter samples pixel centers; ordinary remapping
            // retains the source integer-grid origin.
            const auto sample=[&](unsigned at,unsigned source_size,unsigned output_size,float phase) {
                const std::int64_t numerator=(std::int64_t(at)*256+
                    (jitter!=std::array<float,2>{}?128:0)-std::int64_t(phase*256))*source_size;
                const std::int64_t denominator=std::int64_t(output_size)*256;
                return numerator<0?-((-numerator+denominator-1)/denominator):numerator/denominator;
            };
            std::vector<Pixel> mapped(pixels);
            for(unsigned y=0;y<output_height;++y) for(unsigned x=0;x<output_width;++x) {
                const auto sx=sample(x,width,output_width,jitter[0]),sy=sample(y,height,output_height,jitter[1]);
                const unsigned i=y*output_width+x;
                if(sx>=0 && sx<width && sy>=0 && sy<height) {
                    mapped[i]=expected[unsigned(sy)*width+unsigned(sx)];sample_valid[i]=mapped[i].covered;
                }
                else {sample_valid[i]=false;mapped[i].color=mapped[i].tag=mapped[i].surface_color=0;
                    mapped[i].valid=false;mapped[i].surface={0,0,1,0};mapped[i].depth=0;}
            }
            expected=std::move(mapped);
        }
        auto* bytes=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,owned.upload,true));require(bytes);
        const unsigned actual_bytes=unsigned(source.size()*sizeof(RasterCommand));
        if(actual_bytes) std::memcpy(bytes,source.data(),actual_bytes);
        std::memcpy(bytes+actual_bytes,back.data(),back_bytes);
        std::memcpy(bytes+actual_bytes+back_bytes,artwork.data(),64);
        std::memcpy(bytes+actual_bytes+back_bytes+64,planes.data(),32);
        SDL_UnmapGPUTransferBuffer(device,owned.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        unsigned at=0;
        for(auto [buffer,size]:{std::pair{owned.spans,actual_bytes},std::pair{owned.back,back_bytes},
            std::pair{owned.texels,64U},std::pair{owned.planes,32U}}) {
            if(!size) continue;
            SDL_GPUTransferBufferLocation from{owned.upload,at};SDL_GPUBufferRegion to{buffer,0,size};
            SDL_UploadToGPUBuffer(copy,&from,&to,true);at+=size;
        }
        SDL_EndGPUCopyPass(copy);
        std::array<GpuRasterOutput,2> output;
        for(unsigned path=0;path<2;++path) {
            require(path?SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_GENERIC_RASTER"):
                SDL_SetEnvironmentVariable(env,"STARFOX_TEST_GENERIC_RASTER","1",true));
            require(path && count!=1?SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_DISABLE_ROW_TILE_RASTER"):
                SDL_SetEnvironmentVariable(env,"STARFOX_TEST_DISABLE_ROW_TILE_RASTER","1",true));
            // For single-face cases compare the old tile-list path with the
            // direct row loop, not with the unrelated shared-row experiment.
            require(path?SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_TILED_SPANS"):
                SDL_SetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_TILED_SPANS","1",true));
            require(path && !mask_tiles?SDL_SetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_RASTER","1",true):
                SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_SINGLE_FACE_RASTER"));
            require(mask_tiles?SDL_SetEnvironmentVariable(env,"STARFOX_TEST_MASK_TILE_RASTER","1",true):
                SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_MASK_TILE_RASTER"));
            require(mask_tiles && path?SDL_UnsetEnvironmentVariable(env,"STARFOX_TEST_DISABLE_MASK_TILE_RASTER"):
                SDL_SetEnvironmentVariable(env,"STARFOX_TEST_DISABLE_MASK_TILE_RASTER","1",true));
            const GpuGeometryDepthInput geometry{owned.planes,2,256,256,0,0};
            const auto bg=backgrounds[path].enqueue_row_spans(device,command,owned.back,1,width,height,metadata,nullptr,true,
                nullptr,false,0,0,0,metadata?&geometry:nullptr);
            require(bg.pixels);
            output[path]=painters[path].enqueue_row_spans(device,command,owned.spans,count,width,height,metadata,owned.texels,true,
                remap?nullptr:&bg,wave,wave_offset,wave_frame,64,metadata?&geometry:nullptr,
                remap?std::array<unsigned,2>{output_width,output_height}:std::array<unsigned,2>{},jitter,flags,in_place);
            if(!output[path].pixels) throw std::runtime_error("Row painter width="+std::to_string(width)+
                " count="+std::to_string(count)+" policy="+std::to_string(policy)+" path="+std::to_string(path)+
                ": "+painters[path].status());
            require(output[path].pixels && bool(output[path].surfaces)==metadata && bool(output[path].geometry_depth)==metadata);
            require(output[path].width==output_width && output[path].height==output_height);
            require(painters[path].dispatch_info().row_tile_fused==(!mask_tiles && path==1 && metadata && count>1 && count<=64));
            require(painters[path].dispatch_info().pixel_only==(!mask_tiles && !metadata && path==1 && !(count==1 && !wave)));
            require(painters[path].dispatch_info().single_face_direct==(!mask_tiles && path==1 && count==1 && !wave));
            require(painters[path].dispatch_info().mask_tile==(mask_tiles && path==1 && count>0 && count<=64 && !wave));
            if(painters[path].dispatch_info().single_face_direct) ++direct_faces;
            require(!painters[path].dispatch_info().arguments);
            require(!in_place || output[path].pixels==bg.pixels);
        }
        copy=SDL_BeginGPUCopyPass(command);require(copy);at=0;
        for(const auto& out:output) for(auto [buffer,stride]:{std::pair{out.pixels,4U},std::pair{out.surfaces,16U},std::pair{out.geometry_depth,4U}}) {
            if(!buffer) continue;
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(buffer),0,pixels*stride};
            SDL_GPUTransferBufferLocation to{owned.download,at};SDL_DownloadFromGPUBuffer(copy,&from,&to);at+=pixels*stride;
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* data=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(data);
        const unsigned plane_stride=metadata?24:4;
        require(std::memcmp(data,data+pixels*plane_stride,pixels*plane_stride)==0);
        const auto* packed=reinterpret_cast<const unsigned*>(data);
        const auto* surfaces=reinterpret_cast<const std::array<float,4>*>(data+pixels*4);
        const auto* depth=reinterpret_cast<const float*>(data+pixels*20);
        for(unsigned i=0;i<pixels;++i) {
            const auto& e=expected[i];const unsigned value=packed[i];
            require((value&255)==e.color && ((value>>24)&1)==e.valid);
            require(bool(value&0x04000000U)==sample_valid[i]);
            if(metadata) require(surfaces[i]==e.surface && depth[i]==e.depth);
            // Palette index zero is valid alternate ink, not a missing pair.
            require(bool(value&0x80000000U)==e.pair_valid);
            require(((value>>8)&255)==(e.pair_valid?e.pair:e.tag));
            require(((value>>16)&255)==e.surface_color);
            require(bool(value&0x10000000U)==(flags==1 && e.covered && e.tag==1));
        }
        SDL_UnmapGPUTransferBuffer(device,owned.download);checked+=pixels;++cases;
    }
    require(direct_faces==(mask_tiles?0U:metadata?24U:48U));
    require(!painters[1].enqueue_row_spans(device,nullptr,owned.spans,1,1,1).pixels);
    require(!painters[1].dispatch_info().single_face_direct);
    require(!painters[1].dispatch_info().mask_tile);
    std::cout<<(mask_tiles?"Compact mask-tile painter: ":metadata?"Shared row-tile painter: ":"Colour-only painter: ")<<cases<<" cases / "<<checked
        <<" independently checked pixels and exact "<<(metadata?"packed/surface/depth planes":"packed pixels")
        <<"; partial tiles, 64/65 limits,"
           " black/dither, wrapping texture holes, inherited receivers, emissive/world ownership, copy/in-place,"
        <<(metadata?"":" signed wave rows, custom extents and positive/negative jitter,")
        <<" empty inputs and cache reuse pass; "<<direct_faces<<" direct single-face cases retain old-bin and independent painter parity\n";
}
