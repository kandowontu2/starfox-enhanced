// Actual per-eye samples, not a selected AA setting or identical no-op image.
// A hand-projected triangle gives an independent sample-coverage/color oracle.
#include <bit>
void check_stereo_msaa(SDL_GPUDevice* device) {
    using namespace starfox::render;
    struct Environment {
        SDL_Environment* env=SDL_GetEnvironment();
        std::array<const char*,6> names{"STARFOX_TEST_JOINED_STEREO_SUBMISSIONS",
            "STARFOX_TEST_SPLIT_STEREO_SUBMISSIONS","STARFOX_TEST_PARALLEL_STEREO_ENCODING",
            "STARFOX_TEST_SERIAL_STEREO_ENCODING","STARFOX_TEST_FAIL_STEREO_ENCODING_AFTER_LEFT",
            "STARFOX_TEST_FAIL_STEREO_MSAA_RESOLVE_RIGHT"};
        std::array<std::optional<std::string>,6> saved;
        Environment() {
            for(unsigned i=0;i<names.size();++i) {
                if(const auto* value=SDL_GetEnvironmentVariable(env,names[i]))saved[i]=value;
                require(SDL_UnsetEnvironmentVariable(env,names[i]));
            }
        }
        ~Environment(){for(unsigned i=0;i<names.size();++i) {
            if(saved[i])SDL_SetEnvironmentVariable(env,names[i],saved[i]->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,names[i]);
        }}
        void select(unsigned mode) {
            for(unsigned i=0;i<4;++i)require(SDL_UnsetEnvironmentVariable(env,names[i]));
            if(mode==1)require(SDL_SetEnvironmentVariable(env,names[0],"1",true));
            if(mode==2)require(SDL_SetEnvironmentVariable(env,names[2],"1",true));
        }
    } environment;
    constexpr unsigned max_width=128,height=72,max_bytes=max_width*height*4*2;
    struct Transfer {
        SDL_GPUDevice* device;SDL_GPUTransferBuffer* buffer{};
        ~Transfer(){SDL_WaitForGPUIdle(device);if(buffer)SDL_ReleaseGPUTransferBuffer(device,buffer);}
    } transfer{device};
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,max_bytes,0};
    transfer.buffer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer.buffer);
    const auto capture=[&](const std::array<GpuRasterOutput,2>& output,unsigned width,SDL_GPUCommandBuffer* command=nullptr) {
        if(!command)command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned eye=0;eye<2;++eye) {
            require(output[eye].msaa_color);
            SDL_GPUTextureRegion from{static_cast<SDL_GPUTexture*>(output[eye].msaa_color),0,0,0,0,0,width,height,1};
            SDL_GPUTextureTransferInfo to{transfer.buffer,eye*width*height*4,0,0};
            SDL_DownloadFromGPUTexture(copy,&from,&to);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* data=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,transfer.buffer,false));require(data);
        std::vector<std::uint8_t> result(data,data+width*height*8);
        SDL_UnmapGPUTransferBuffer(device,transfer.buffer);return result;
    };
    starfox::assets::Shape shape;
    shape.vertices={{-14,-11,0},{17,-9,0},{-2,18,0}};
    shape.faces={{-1,0,{0,0,1},{2,1,0}}};shape.colour_words={0x11};
    GpuModelDraw model;model.shape=&shape;model.pose.force_colour=true;
    model.pose.vanish_y=height/2;model.geometry_depth=model.surface_metadata=true;
    model.pose.continuous_geometry=model.pose.subpixel_projection=true;
    const auto check=[&](const std::vector<std::uint8_t>& pixels,unsigned width,unsigned samples,const Palette256& palette) {
        unsigned covered=0,partial=0;
        for(unsigned eye=0;eye<2;++eye) {
            // This does not call stereo_scene_eye or read projected GPU points.
            const double eye_x=(eye?1:-1)*6.4*.5,focal=model.settings.focal_length;
            std::array<MsaaPoint,3> triangle;
            for(unsigned v=0;v<3;++v) triangle[v]={
                float(model.pose.vanish_x+focal*eye_x/200.+focal*(model.pose.x-eye_x+shape.vertices[v].x)/model.pose.z),
                float(model.pose.vanish_y+focal*(model.pose.y+shape.vertices[v].y)/model.pose.z)};
            // Forced source materials store two four-bit palette indices;
            // 0x11 is solid index 1, not the host palette index 17.
            const auto color=palette[model.pose.forced_colour&15];
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                const auto n=std::popcount(msaa_coverage(triangle,x,y,samples));
                covered+=n!=0;partial+=n!=0 && n!=int(samples);
                const auto at=(eye*width*height+y*width+x)*4;
                const std::array<unsigned,4> channels{color.r,color.g,color.b,255};
                for(unsigned c=0;c<4;++c) {
                    const int expected=int(std::lround(double(channels[c])*n/samples));
                    if(std::abs(int(pixels[at+c])-expected)>1)
                        throw std::runtime_error("Stereo MSAA numeric mismatch eye="+std::to_string(eye)
                            +" x="+std::to_string(x)+" y="+std::to_string(y)+" channel="+std::to_string(c)
                            +" actual="+std::to_string(pixels[at+c])+" expected="+std::to_string(expected));
                }
            }
        }
        require(covered>20 && partial>0);
        require(std::memcmp(pixels.data(),pixels.data()+width*height*4,width*height*4)!=0);
    };
    unsigned cases=0,resolves=0;std::uint64_t checked=0;
    GpuStereoScene owned;
    const Palette256 pending{};
    for(unsigned mode=0;mode<3;++mode)for(unsigned samples:{2U,4U,8U})for(unsigned iteration=0;iteration<6;++iteration) {
        environment.select(mode);
        const unsigned width=iteration%2?96:128;
        model.pose.vanish_x=width/2;model.pose.x=.3125+iteration*.75;model.pose.y=.1875;
        model.pose.z=iteration%2?256:320;model.pose.forced_colour=iteration==4?0:0x11;
        const std::array<GpuSceneDraw,1> frame{model};
        const GpuScene::MsaaSettings settings{nullptr,samples,pending,true};
        require(owned.render_resident(device,width,height,frame,6.4,200,&settings));
        require(owned.msaa_samples()==samples);
        const std::array<GpuRasterOutput,2> original{owned.resident_output(0),owned.resident_output(1)};
        require(original[0].pixels && original[1].pixels && original[0].pixels!=original[1].pixels);
        require(!original[0].msaa_color && !original[1].msaa_color);
        std::vector<std::uint8_t> preceding;
        for(unsigned phase=0;phase<3;++phase) {
            Palette256 palette{};
            palette[model.pose.forced_colour&15]={std::uint8_t(211-phase*61),std::uint8_t(37+phase*73),std::uint8_t(93+phase*31),255};
            require(owned.resolve_msaa_palette(palette));
            const std::array<GpuRasterOutput,2> output{owned.resident_output(0),owned.resident_output(1)};
            require(output[0].msaa_color!=output[1].msaa_color && owned.msaa_samples()==samples);
            for(unsigned eye=0;eye<2;++eye)require(output[eye].pixels==original[eye].pixels
                && output[eye].geometry_depth==original[eye].geometry_depth && output[eye].surfaces==original[eye].surfaces);
            const auto actual=capture(output,width);check(actual,width,samples,palette);
            if(!preceding.empty())require(preceding!=actual);
            preceding=actual;checked+=actual.size();++resolves;
        }
        Palette256 final{};final[model.pose.forced_colour&15]={80,160,240,255};
        require(SDL_SetEnvironmentVariable(environment.env,environment.names[5],"1",true));
        require(!owned.resolve_msaa_palette(final));
        require(!owned.msaa_samples() && !owned.resident_output(0).pixels && !owned.resident_output(1).pixels);
        require(SDL_UnsetEnvironmentVariable(environment.env,environment.names[5]));
        require(owned.render_resident(device,width,height,frame,6.4,200,&settings));
        require(owned.resolve_msaa_palette(final));check(capture({owned.resident_output(0),owned.resident_output(1)},width),width,samples,final);
        if(mode) {
            require(SDL_SetEnvironmentVariable(environment.env,environment.names[4],"1",true));
            require(!owned.render_resident(device,width,height,frame,6.4,200,&settings));
            require(!owned.msaa_samples() && !owned.resident_output(0).pixels && !owned.resident_output(1).pixels);
            require(SDL_UnsetEnvironmentVariable(environment.env,environment.names[4]));
            require(owned.render_resident(device,width,height,frame,6.4,200,&settings));
            require(owned.resolve_msaa_palette(final));
            check(capture({owned.resident_output(0),owned.resident_output(1)},width),width,samples,final);
        }
        require(owned.render_resident(device,width,height,frame,6.4,200));
        require(!owned.msaa_samples() && !owned.resident_output(0).msaa_color && !owned.resident_output(1).msaa_color);
        require(owned.render_resident(device,width,height,{},6.4,200,&settings));
        require(owned.resolve_msaa_palette(final));
        const auto empty=capture({owned.resident_output(0),owned.resident_output(1)},width);
        require(std::all_of(empty.begin(),empty.end(),[](auto byte){return byte==0;}));
        ++cases;
    }
    // Borrowed one-command encoding uses the same optional sample contract.
    require(owned.wait_for_completion());
    GpuStereoScene borrowed;Palette256 palette{};palette[model.pose.forced_colour&15]={100,190,250,255};
    const std::array<GpuSceneDraw,1> frame{model};
    const GpuScene::MsaaSettings settings{nullptr,4,palette};
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
    const auto output=borrowed.enqueue(device,command,96,height,frame,6.4,200,&settings);require(output.has_value());
    check(capture(*output,96,command),96,4,palette);
    std::cout<<"Stereo MSAA: "<<cases<<" real 2x/4x/8x split/joined/parallel cases; "<<resolves
        <<" retained palette resolves, "<<checked<<" numeric RGBA bytes; independent eyes, OFF, empty, cancellation and borrowed encoding passed\n";
}
