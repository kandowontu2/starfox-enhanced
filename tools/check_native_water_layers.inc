// Fixture readback only. Production keeps all planes in the same resident
// allocation and submits the underlay composite on its existing SDL queue.
Bytes read_ray_layers(Gpu& gpu,const render::shadows::GpuReflectionOutput& output) {
    check(!gpu.command && output.water_layers.storage_bytes,"Invalid water-layer fixture readback");
    const auto bytes=output.water_layers.storage_bytes;
    const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes,0};
    auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&info);check(transfer,SDL_GetError());
    gpu.begin();auto* pass=SDL_BeginGPUCopyPass(gpu.command);check(pass,SDL_GetError());
    const SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output.buffer),0,bytes};
    const SDL_GPUTransferBufferLocation target{transfer,0};
    SDL_DownloadFromGPUBuffer(pass,&source,&target);SDL_EndGPUCopyPass(pass);gpu.submit();
    check(SDL_WaitForGPUIdle(gpu.device),SDL_GetError());
    auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,transfer,false));check(mapped,SDL_GetError());
    Bytes result(mapped,mapped+bytes);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
    return result;
}
void native_water_layer_tests(Gpu& gpu,SDL_GPUTextureFormat format,
    const render::CalibratedRayGeometryOutput& source) {
    using namespace render::shadows;
    const render::GpuScene::RayGeometryOutput geometry{source.device,source.buffer,source.vertex_count,true,
        source.materials,source.material_offset,source.material_bytes};
    Camera camera{source.width,source.height,source.projection[0],source.projection[2],source.projection[3],source.projection[1]};
    const PrimaryRayRange range{source.near_plane,source.far_plane.value_or(65536)};
    const ReceiverPlane plane{{0,128,0},{0,-1,0}};
    RayWater water;water.time=1.75F;water.source_colour=std::array<float,3>{.2F,.3F,.4F};
    const std::array<unsigned,256> palette{};
    SdlDxrShadows traced;Bytes ordinary,world;
    const auto trace=[&](bool layers,bool ground) {
        water.auxiliary_layers=layers;
        check(traced.render_reflections(gpu.device,camera,geometry,palette,0xffb07841U,0,0,{},0,
            {1,0,0,0,1,0,0,0,1},nullptr,plane,0,&water,ground,range,0,srgb(format)?2:1),traced.status().c_str());
        return traced.reflection_output();
    };
    check(!trace(false,false).water_layers.storage_bytes,"Disabled water layers allocated an auxiliary plane");
    check(traced.readback(ordinary),traced.status().c_str());
    const auto output=trace(true,false);
    check(output.water_layers==*native_water_layers(source.width,source.height),"Native-to-SDL water layer metadata changed");
    const auto layered=read_ray_layers(gpu,output);
    check(std::equal(ordinary.begin(),ordinary.end(),layered.begin()),"Auxiliary water layers changed the primary ray image");
    const auto layout=output.water_layers;
    water.surface_layers=true;
    const auto compact_output=trace(false,false);
    check(compact_output.water_layers==*native_water_layers(source.width,source.height,false),
        "Surface-only native-to-SDL water metadata changed");
    const auto compact=read_ray_layers(gpu,compact_output);
    check(std::equal(ordinary.begin(),ordinary.end(),compact.begin()),"Surface-only guides changed primary transport");
    check(std::equal(compact.begin()+compact_output.water_layers.surface_offset,compact.end(),
        layered.begin()+layout.surface_offset),"Surface-only water guides differ from full hidden-world trace");
    water.surface_layers=false;
    unsigned wet=0,dry=0,hidden=0;
    for(unsigned y=0;y<source.height;++y) for(unsigned x=0;x<source.width;++x) {
        const unsigned pixel=y*source.width+x,at=pixel*4;
        std::array<float,4> surface{};std::memcpy(surface.data(),layered.data()+layout.surface_offset+pixel*16,16);
        const bool liquid=layered[at+3]==253;
        if(liquid) {
            // Independent plane intersection from camera pixel centres, not
            // another read of the GPU surface/depth guide.
            const double dx=(x+.5-camera.center_x)/camera.focal_length;
            const double dy=(y+.5-camera.center_y)/camera.vertical_focal_length();
            const double expected=128/dy,length=std::sqrt(dx*dx+dy*dy+1);
            check(std::isfinite(surface[3]) && surface[3]>0
                && std::abs(surface[3]-expected)<std::max(.01,expected*2.e-5),"Water depth retained the submerged primary model");
            const double normal_length=std::sqrt(double(surface[0])*surface[0]+double(surface[1])*surface[1]+double(surface[2])*surface[2]);
            check(std::abs(normal_length-1)<1.e-5 && (surface[0]*dx+surface[1]*dy+surface[2])/length<=0,
                "Native water normal is non-finite, unnormalized or reversed");
            ++wet;
        } else {
            check(surface==std::array<float,4>{},"Dry geometry inherited a water depth guide");++dry;
        }
        if(layered[layout.world_offset+at+3]==253 && !liquid) ++hidden;
    }
    (void)trace(false,true);check(traced.readback(world),traced.status().c_str());
    check(std::equal(world.begin(),world.end(),layered.begin()+layout.world_offset),
        "Hidden water underlay omitted actual secondary scene transport");
    check(wet>100 && dry>100 && hidden>10,"Water layer fixture missed liquid/dry/hidden-ground ownership");
    (void)trace(true,false);
    check(!traced.render_reflections(gpu.device,camera,geometry,palette,0,0,0,{},0,
        {1,0,0,0,1,0,0,0,1},nullptr,plane,0,&water,false,PrimaryRayRange{0,0},0,srgb(format)?2:1),
        "Malformed water ray range accepted");
    const auto invalid=traced.reflection_output();
    check(!invalid.buffer && !invalid.device && !invalid.width && !invalid.height && !invalid.row_bytes
        && invalid.water_layers==NativeWaterLayers{},"Failed water request exposed stale auxiliary metadata");
    std::cout<<"  Hardware water layers: "<<wet<<" analytic depth/normal, "<<dry<<" dry exclusions, "<<hidden
        <<" hidden water pixels; primary unchanged, resident offsets and world-only secondary transport passed\n";
}
