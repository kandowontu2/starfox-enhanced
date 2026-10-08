// Scalar current/previous point projection + existing independent centred
// shutter reference. GPU guides/payloads are never read back for the oracle.
void native_joint_particles(Gpu& gpu,SDL_GPUTextureFormat format) {
    constexpr unsigned w=72,h=48;
    constexpr vr::Matrix4 rig{1.F/256,0,0,0,0,-1.F/256,0,0,0,0,-1.F/256,0,0,0,0,1};
    vr::EyeCamera eye;eye.view={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    eye.projection={1,0,0,0,0,1.5F,0,0,.07F,-.04F,-1.005F,-1,0,0,-.10025F,0};
    const double cx=w*(1-eye.projection[8])*.5-.5,cy=h*(1+eye.projection[9])*.5-.5,focal=w*.5;
    Texture source(gpu,w,h,format),underlay(gpu,w,h,format),owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
        surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        result(gpu,w,h,format),retained(gpu,w,h,format);
    Bytes original(w*h*4),background(w*h*4),tags(w*h*4);
    std::vector<std::array<float,4>> depths(w*h),velocities(w*h);
    std::vector<render::MotionBlurGuide> guides(w*h);render::Framebuffer frame(w,h);frame.enable_layer_tags(true);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        const unsigned i=y*w+x;original[i*4]=background[i*4]=15+(x*3)%50;
        original[i*4+1]=background[i*4+1]=30;original[i*4+2]=background[i*4+2]=65;
        original[i*4+3]=background[i*4+3]=255;tags[i*4+2]=1;
        const bool model=x>=28 && x<=36 && y>=12 && y<=36;
        if(model) {tags[i*4+2]=2;original[i*4]=190;original[i*4+1]=45;depths[i]={0,0,-1,256};}
        const bool protected_ink=x==25 || y==4;
        if(protected_ink) tags[i*4+2]=0;
        if(x==3) original[i*4+3]=0;
        guides[i]={model?-8.F:0,0,model?256.F:0,model,!protected_ink};
        velocities[i]={model?-8.F:0,0,model?300.F:0,model?1.F:0};
        frame.set_stored(x,y,0,protected_ink?render::PixelLayer::two_d:model?render::PixelLayer::three_d:render::PixelLayer::background);
    }
    gpu.begin();source.upload(original);underlay.upload(background);owner.upload(tags);surface.upload(words(depths));motion.upload(words(velocities));gpu.submit();
    render::GpuCalibratedSceneFx projection;check(projection.initialize(gpu.device,format),projection.status().c_str());
    render::GpuCalibratedMotionBlur blur;check(blur.initialize(gpu.device,format),blur.status().c_str());
    unsigned responses{};
    for(unsigned type=2;type<=7;++type) for(unsigned variant=0;variant<6;++variant) {
        render::SceneFxWorldFrame world,prior;render::SceneFxFrame expected_points;
        const double x=type==7?38:37,y=23,z=512;
        const std::array<double,3> p{(x-cx)*z/focal,(y-cy)*z/focal,z};
        world.add(p,45,.8F,float(type),.08F,{.3F,.9F,.1F,0},{int(type),13,0,0});
        auto before=p;before[0]-=100;before[1]+=20;before[2]+=32;
        prior.add(before,45,.8F,float(type),.07F,{.3F,.9F,.1F,0},{int(type),13,0,0});
        expected_points.add({float(x),float(y),float(focal*45/z),.8F},{float(type),float(z),.08F,0},{.3F,.9F,.1F,0});
        if(variant==1) prior.points[0].identity[1]++; // New birth, no old velocity.
        if(variant==2) prior.points[prior.count++]=prior.points[0]; // Ambiguous old key.
        if(variant==3) prior.points[0].type=type==7?6:7; // Changed particle primitive.
        if(variant==4) prior.points[0].position[2]=-100; // Previously clipped.
        if(variant==0 || variant==5)
            expected_points.motion_previous[0]={{float(cx+focal*before[0]/before[2]),float(cy+focal*before[1]/before[2]),float(before[2])},true};
        auto settings=render::motion_blur_preset(2);settings.interval_seconds=1./60;
        if(variant==5) settings.paused=true;
        Bytes expected;check(render::render_scene_shutter(expected_points,settings,frame,original,expected,nullptr,0,0,guides,background),
            "Independent joint particle/geometry oracle rejected fixture");
        const render::CalibratedSceneFxPrevious old{&prior,eye,rig,{}};
        gpu.begin();void* points{},*output{};
        check(projection.project_particles(gpu.command,w,h,1,world,eye,rig,{},&old,points),projection.status().c_str());
        check(blur.enqueue(gpu.command,source.texture,underlay.texture,owner.texture,surface.texture,motion.texture,result.texture,
            w,h,settings,true,output,{},{},points,1),blur.status().c_str());
        check(output==result.texture && blur.dispatch_count()>0,"Resident particles silently bypassed the joint exposure");
        // Record another projection/exposure before submission. The first eye
        // must retain its cycled backing, even with reordered/new source keys.
        auto other=world;other.points[0].position[0]+=80;
        check(projection.project_particles(gpu.command,w,h,1,other,eye,rig,{},nullptr,points),projection.status().c_str());
        check(blur.enqueue(gpu.command,source.texture,underlay.texture,owner.texture,surface.texture,motion.texture,retained.texture,
            w,h,settings,true,output,{},{},points,1),blur.status().c_str());gpu.submit();
        const auto actual=result.read();compare(actual,expected,original,tags,srgb(format)?2:1,"native joint particles");
        check(actual!=retained.read(),"Second particle eye/projection overwrote the retained first image");
        responses+=actual!=original;
    }
    check(responses==36,"Native joint exposure lost visible particle types/history cases");
    std::cout<<"Native joint particle format="<<unsigned(format)<<": six types, moving/partial model occlusion, stable/new/duplicate/type-change/clipped prior, pause, protected ink/alpha and same-command eye retention passed.\n";
}
