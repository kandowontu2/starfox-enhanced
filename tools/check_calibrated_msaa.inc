// Actual native hardware MSAA. The oracle projects source vertices from the
// independent XR pose/FOV above and tests standard raster sample positions;
// it never consumes the renderer's projection constants or resolved image.
struct NativeMsaaTargets {
    SDL_GPUDevice* device;unsigned samples;
    SDL_GPUTexture *colour[2]{},*depth[2]{},*second{};
    SDL_GPUTransferBuffer* read{};
    NativeMsaaTargets(Gpu& gpu,unsigned count):device(gpu.device),samples(count) {
        try {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
        info.width=width;info.height=height;info.layer_count_or_depth=info.num_levels=1;
        info.format=gpu.format;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.sample_count=count==2?SDL_GPU_SAMPLECOUNT_2:count==4?SDL_GPU_SAMPLECOUNT_4:SDL_GPU_SAMPLECOUNT_8;
        for(auto*& target:colour) {target=SDL_CreateGPUTexture(device,&info);check(target,SDL_GetError());}
        info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        for(auto*& target:depth) {target=SDL_CreateGPUTexture(device,&info);check(target,SDL_GetError());}
        info.sample_count=SDL_GPU_SAMPLECOUNT_1;info.format=gpu.format;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        second=SDL_CreateGPUTexture(device,&info);check(second,SDL_GetError());
        const SDL_GPUTransferBufferCreateInfo download{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*4,0};
        read=SDL_CreateGPUTransferBuffer(device,&download);check(read,SDL_GetError());
        } catch(...) {release();throw;}
    }
    ~NativeMsaaTargets() {release();}
    void release() noexcept {
        SDL_WaitForGPUIdle(device);
        for(auto* t:colour) if(t) SDL_ReleaseGPUTexture(device,t);
        for(auto* t:depth) if(t) SDL_ReleaseGPUTexture(device,t);
        if(second) SDL_ReleaseGPUTexture(device,second);
        if(read) SDL_ReleaseGPUTransferBuffer(device,read);
    }
};
std::array<std::vector<unsigned char>,2> native_msaa_render(Gpu& gpu,render::GpuCalibratedScene& scene,
    NativeMsaaTargets& targets,std::span<const render::CalibratedSceneDraw> draws,
    const std::array<vr::EyeCamera,2>& cameras,bool ordered=false) {
    gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
    check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
    const auto enqueue=[&](render::CalibratedScenePhase phase,bool retain) {
        for(unsigned eye=0;eye<2;++eye) {
            const render::CalibratedSceneMultisample msaa{targets.samples,eye?targets.second:gpu.color,retain};
            check(scene.enqueue_eye(gpu.command,targets.colour[eye],targets.depth[eye],width,height,cameras[eye],
                {0,0,1,1},phase,nullptr,phase==render::CalibratedScenePhase::after_rays?token:0,nullptr,false,msaa),scene.status().c_str());
        }
    };
    if(ordered) {
        enqueue(render::CalibratedScenePhase::before_rays,true);(void)gpu.finish();
        check(scene.notify_submitted(token),"MSAA source submission was not confirmed");
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        enqueue(render::CalibratedScenePhase::after_rays,false);
    } else enqueue(render::CalibratedScenePhase::all,false);
    auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
    const SDL_GPUTextureRegion from{targets.second,0,0,0,0,0,width,height,1};
    const SDL_GPUTextureTransferInfo to{targets.read,0,width,height};
    SDL_DownloadFromGPUTexture(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    std::array<std::vector<unsigned char>,2> pixels;pixels[0]=gpu.finish();
    const auto* mapped=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,targets.read,false));check(mapped,SDL_GetError());
    pixels[1].assign(mapped,mapped+width*height*4);SDL_UnmapGPUTransferBuffer(gpu.device,targets.read);
    if(gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
        for(unsigned i=0;i<pixels[1].size();i+=4) std::swap(pixels[1][i],pixels[1][i+2]);
    return pixels;
}
struct NativeMsaaOracleStats {unsigned pixels{},partial{},ambiguous{};};
NativeMsaaOracleStats native_msaa_oracle(const Gpu& gpu,const std::vector<unsigned char>& pixels,
    std::span<const render::CalibratedSceneDraw> draws,const XrView& eye,unsigned count) {
    // D3D standard / Vulkan standardSampleLocations, deliberately independent
    // of the compute renderer's MSAA helpers/sample table.
    constexpr std::array<std::array<int,2>,14> locations{{{4,4},{-4,-4},{-2,-6},{6,-2},{-6,2},{2,6},
        {1,-3},{-1,3},{5,1},{-3,-5},{-5,5},{-7,-1},{3,7},{7,-7}}};
    struct Face {std::array<std::array<double,2>,3> xy;double depth;std::array<double,4> colour;
        vr::SceneBlend blend;bool test;};
    std::vector<Face> faces;
    for(const auto& draw:draws) for(unsigned at=0;at<draw.vertices.size();at+=3) {
        Face face{};face.test=draw.depth_test;face.blend=draw.blend;
        for(unsigned v=0;v<3;++v) {
            const auto world=model_point(draw.model,draw.vertices[at+v]);face.xy[v]=reference(eye,world);
            // Fixed-function rasterization snaps vertices to 8-bit subpixels.
            for(auto& axis:face.xy[v]) axis=std::round(axis*256)/256;
            if(v==0) face.depth=-world_eye(eye,world)[2];
        }
        const auto& vertex=draw.vertices[at];
        for(unsigned c=0;c<4;++c) face.colour[c]=vertex.dither_scale && draw.effect_layer==2
            ?(double(vertex.color[c])+vertex.odd_color[c])*.5:vertex.color[c];
        if(vertex.texture[3]&1) {
            // Fully opaque/transparent single-texel fixtures. A separate
            // interior test below checks the mixed texture's alpha holes.
            const auto packed=draw.texels[vertex.texture[0]];
            if(!(packed>>24)) continue;
            for(unsigned c=0;c<4;++c) face.colour[c]=double((packed>>(c*8))&255U)/255;
        }
        faces.push_back(face);
    }
    const bool srgb=gpu.format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    const auto encode=[&](double value,unsigned channel) {
        value=std::clamp(value,0.,1.);
        if(srgb && channel<3) value=value<=.0031308?12.92*value:1.055*std::pow(value,1./2.4)-.055;
        return int(std::lround(value*255));
    };
    NativeMsaaOracleStats stats;
    const auto edge=[](const auto& a,const auto& b,const auto& p) {return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);};
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        std::array<double,4> sum{};unsigned changed=0;bool ambiguous=false;
        for(unsigned sample=0;sample<count;++sample) {
            const auto offset=locations[count-2+sample];const std::array p{x+.5+offset[0]/16.,y+.5+offset[1]/16.};
            std::array<double,4> colour{0,0,1,1};double depth=std::numeric_limits<double>::infinity();
            for(const auto& face:faces) {
                const double area=edge(face.xy[0],face.xy[1],face.xy[2]);bool covered=true;
                for(unsigned v=0;v<3;++v) {
                    const auto& a=face.xy[v];const auto& b=face.xy[(v+1)%3];const double distance=edge(a,b,p);
                    // Avoid only the exactly shared/subpixel ambiguous sample,
                    // not a whole edge pixel or every partially covered pixel.
                    if(std::abs(distance)/std::hypot(b[0]-a[0],b[1]-a[1])<1.e-7) ambiguous=true;
                    covered&=area>0?distance>0:distance<0;
                }
                if(!covered || (face.test && face.depth>=depth)) continue;
                const auto& source=face.colour;
                for(unsigned c=0;c<3;++c) {
                    switch(face.blend) {
                    case vr::SceneBlend::opaque:colour[c]=source[c];break;
                    case vr::SceneBlend::alpha:colour[c]=source[c]*source[3]+colour[c]*(1-source[3]);break;
                    case vr::SceneBlend::add:colour[c]+=source[c]*source[3];break;
                    case vr::SceneBlend::half_add:colour[c]=(colour[c]+source[c])*source[3];break;
                    case vr::SceneBlend::subtract:colour[c]-=source[c]*source[3];break;
                    case vr::SceneBlend::half_subtract:colour[c]=(colour[c]-source[c])*source[3];break;
                    case vr::SceneBlend::shadow:colour[c]*=1-source[3];break;
                    }
                    colour[c]=std::clamp(colour[c],0.,1.);
                }
                if(face.blend==vr::SceneBlend::opaque) colour[3]=source[3];
                else if(face.blend==vr::SceneBlend::alpha) colour[3]=source[3]+colour[3]*(1-source[3]);
                if(face.test) depth=face.depth;
            }
            if(colour!=std::array<double,4>{0,0,1,1}) ++changed;
            for(unsigned c=0;c<4;++c) sum[c]+=colour[c]/count;
        }
        if(ambiguous) {++stats.ambiguous;continue;}
        for(unsigned c=0;c<4;++c) {
            const int wanted=encode(sum[c],c),actual=pixels[(y*width+x)*4+c];
            if(std::abs(wanted-actual)>2) throw std::runtime_error("Native MSAA coverage/oracle mismatch: samples="+std::to_string(count)
                +" format="+std::to_string(unsigned(gpu.format))+" pixel="+std::to_string(x)+","+std::to_string(y)
                +" channel="+std::to_string(c)+" actual="+std::to_string(actual)+" expected="+std::to_string(wanted));
        }
        ++stats.pixels;if(changed && changed<count) ++stats.partial;
    }
    check(stats.ambiguous<200,"MSAA oracle skipped too many ambiguous samples");return stats;
}
void native_msaa_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    std::array<XrView,2> eyes{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};std::array<vr::EyeCamera,2> cameras;
    for(unsigned e=0;e<2;++e) {
        eyes[e].pose.orientation.w=1;eyes[e].pose.position={e?.035F:-.029F,.022F,.031F};
        eyes[e].fov={-.64F,.72F,.57F,-.52F};cameras[e]=camera_for(eyes[e],d3d);
    }
    auto red=quad(-.08F,.04F,-1.7F,.57F,{1,0,0,1});
    // Non-axis-aligned silhouette and both winding directions.
    red.resize(3);red[0].position[0]=-.67F;red[0].position[1]=.43F;
    red[1].position[0]=.56F;red[1].position[1]=.29F;red[2].position[0]=-.19F;red[2].position[1]=-.51F;
    auto green=quad(.16F,-.09F,-2.4F,.64F,{0,1,0,1});
    auto pale=quad(.08F,.12F,-1.35F,.29F,{.75F,.25F,.5F,.5F});
    auto dither=quad(-.02F,.06F,-1.4F,.38F,{1,0,0,1});
    for(auto& v:dither) {v.odd_color[0]=0;v.odd_color[1]=1;v.dither_scale=1;}
    auto texture=quad(.12F,-.12F,-1.2F,.25F,{1,1,1,1});
    for(auto& v:texture) v.texture[3]=1;
    const std::array<uint32_t,1> opaque{0xff00ff00U},transparent{0U};
    std::array<render::CalibratedSceneDraw,2> base{{{red},{green}}};base[0].depth_test=base[1].depth_test=false;
    const auto off=render(gpu,scene,base,cameras[0],{0,0,1,1});
    unsigned pixels=0,partial=0,qualities=0;
    check(!scene.supports_multisample(0) && !scene.supports_multisample(3) && !scene.supports_multisample(16),"Invalid native MSAA count supported");
    for(unsigned count:{2U,4U,8U}) {
        if(!scene.supports_multisample(count)) {std::cout<<"Native hardware MSAA "<<count<<"x unsupported by target formats; no fallback.\n";continue;}
        NativeMsaaTargets targets(gpu,count);++qualities;
        // Clamp UVs to the exact quad. Partially covered edge pixels whose
        // centre is outside it must shade a covered centroid, not disappear.
        for(auto& v:texture) {v.texture[1]=v.texture[2]=0;v.texture[3]=1|4;
            v.uv[0]=(v.position[0]+.13F)*2;v.uv[1]=(.13F-v.position[1])*2;}
        const auto verify=[&](std::span<const render::CalibratedSceneDraw> draws) {
            const auto images=native_msaa_render(gpu,scene,targets,draws,cameras);
            for(unsigned e=0;e<2;++e) {const auto stats=native_msaa_oracle(gpu,images[e],draws,eyes[e],count);pixels+=stats.pixels;partial+=stats.partial;}
            check(images[0]!=images[1],"Hardware MSAA collapsed independent native eye poses");return images;
        };
        const render::CalibratedSceneDraw triangle{red};(void)verify(std::span(&triangle,1));
        auto reversed=red;std::swap(reversed[1],reversed[2]);const render::CalibratedSceneDraw opposite{reversed};
        check(verify(std::span(&opposite,1))==native_msaa_render(gpu,scene,targets,std::span(&triangle,1),cameras),"MSAA winding changed coverage");
        (void)verify(base);base[0].depth_test=base[1].depth_test=true;
        const auto depth_order=verify(base);std::swap(base[0],base[1]);check(verify(base)==depth_order,"MSAA depth depended on painter order");
        std::swap(base[0],base[1]);base[0].depth_test=base[1].depth_test=false;
        for(auto blend:{vr::SceneBlend::alpha,vr::SceneBlend::add,vr::SceneBlend::half_add,
            vr::SceneBlend::subtract,vr::SceneBlend::half_subtract,vr::SceneBlend::shadow}) {
            const std::array<render::CalibratedSceneDraw,2> draws{{{red,{},identity,vr::SceneTopology::triangles,vr::SceneBlend::opaque,false},
                {pale,{},identity,vr::SceneTopology::triangles,blend,false}}};(void)verify(draws);
        }
        render::CalibratedSceneDraw pair{dither};pair.effect_layer=2;(void)verify(std::span(&pair,1));
        const std::array<render::CalibratedSceneDraw,2> solid_texture{{{red},{texture,opaque}}};(void)verify(solid_texture);
        const std::array<render::CalibratedSceneDraw,2> hole_texture{{{red},{texture,transparent}}};
        check(verify(hole_texture)==native_msaa_render(gpu,scene,targets,std::span(&triangle,1),cameras),"Transparent texture acquired MSAA colour/depth");
        // A live mixed texture must discard interior alpha holes in each eye.
        const std::array<uint32_t,4> checker{0U,0xff00ff00U,0xff00ff00U,0U};
        for(auto& v:texture) {v.texture[1]=v.texture[2]=1;v.uv[0]=(v.position[0]+.13F)*4;v.uv[1]=(.13F-v.position[1])*4;}
        const std::array<render::CalibratedSceneDraw,2> cutout{{{red},{texture,checker}}};
        const auto mixed=native_msaa_render(gpu,scene,targets,cutout,cameras);
        const auto underlay=native_msaa_render(gpu,scene,targets,std::span(&triangle,1),cameras);
        for(unsigned e=0;e<2;++e) for(const auto point:{std::array<double,3>{-.065,.065,-1.2},std::array<double,3>{.31,-.31,-1.2}}) {
            const auto xy=reference(eyes[e],point);const auto at=(unsigned(xy[1])*width+unsigned(xy[0]))*4;
            for(unsigned c=0;c<4;++c) check(mixed[e][at+c]==underlay[e][at+c],"Native MSAA filled a transparent texture hole");
        }
        base[0].depth_test=base[1].depth_test=true;base[1].after_rays=true;
        const auto together=native_msaa_render(gpu,scene,targets,base,cameras);
        check(native_msaa_render(gpu,scene,targets,base,cameras,true)==together,"Retained MSAA samples lost ordered native overlays");
        base[1].after_rays=false;base[0].depth_test=base[1].depth_test=false;
        check(native_msaa_render(gpu,scene,targets,base,cameras)==native_msaa_render(gpu,scene,targets,base,cameras),"Held MSAA eye changed between presentations");
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());check(scene.upload(gpu.command,base),scene.status().c_str());
        const auto cancelled=scene.upload_token();
        for(const auto invalid:{render::CalibratedSceneMultisample{3,gpu.color},render::CalibratedSceneMultisample{1,gpu.color},
            render::CalibratedSceneMultisample{count,targets.colour[0]},render::CalibratedSceneMultisample{count,targets.depth[0]}})
            check(!scene.enqueue_eye(gpu.command,targets.colour[0],targets.depth[0],width,height,cameras[0],{},
                render::CalibratedScenePhase::all,nullptr,0,nullptr,false,invalid),"Invalid MSAA resolve/count reached a render pass");
        const render::CalibratedSceneMultisample valid{count,gpu.color};
        check(!scene.enqueue_eye(gpu.command,targets.colour[0],targets.depth[0],width,height,cameras[0],{},
            render::CalibratedScenePhase::before_rays,nullptr,0,nullptr,false,valid),"MSAA continuation accepted discarded samples");
        check(!scene.enqueue_eye(gpu.command,targets.colour[0],targets.depth[0],width,height,cameras[0],{},
            render::CalibratedScenePhase::before_rays,targets.second,0,nullptr,false,valid),"MSAA guides accepted discarded colour samples");
        check(scene.enqueue_eye(gpu.command,targets.colour[0],targets.depth[0],width,height,cameras[0],{0,0,1,1},
            render::CalibratedScenePhase::all,nullptr,0,nullptr,false,valid),"Invalid MSAA request poisoned a valid same-command retry");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(!scene.enqueue_eye(gpu.command,targets.colour[0],targets.depth[0],width,height,cameras[0],{},
            render::CalibratedScenePhase::after_rays,nullptr,cancelled,nullptr,false,valid),"Cancelled MSAA upload continued on a recycled command");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
        const auto empty=native_msaa_render(gpu,scene,targets,{},cameras);
        for(const auto& image:empty) for(unsigned at=0;at<image.size();at+=4)
            check(image[at]==0 && image[at+1]==0 && image[at+2]==255 && image[at+3]==255,"Empty MSAA eye retained stale geometry");
        check(render(gpu,scene,base,cameras[0],{0,0,1,1})==off,"MSAA OFF did not restore exact single-sample rendering");
    }
    check(qualities>=2 && pixels>2'000'000 && partial>500,"Native hardware MSAA fixture missed quality/edge coverage");
    std::cout<<"Native hardware MSAA format "<<unsigned(gpu.format)<<": "<<pixels<<" analytic pixel checks, "<<partial
        <<" partial edges, "<<qualities<<" supported counts; two resident eyes, both windings, painter/depth, six blends, dither pair, "
        "alpha holes, retained overlays, held/cancel/OFF/alias/ownership guards passed. Geometry core, not frontend parity.\n";
}
