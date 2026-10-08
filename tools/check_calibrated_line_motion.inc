// Independent endpoint/projection oracle. Production never downloads guides or
// expands raster lines into ribbons; this fixture reads them only for comparison.
void native_line_motion(Gpu& gpu,SDL_GPUTextureFormat format,unsigned variant) {
    constexpr unsigned w=121,h=61;
    constexpr vr::Matrix4 id{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<vr::SceneVertex,2> current{},accepted{};
    for(unsigned i=0;i<2;++i) {
        auto& v=current[i];v.position[0]=i?.85F:-.8F;v.position[2]=i?-3.2F:-1.8F;
        v.color[0]=.9F;v.color[1]=.21F;v.color[2]=.07F;v.color[3]=1;
        std::copy(std::begin(v.color),std::end(v.color),v.odd_color);accepted[i]=v;
        if(variant==1) {
            v.position[0]*=256;v.position[1]*=-256;v.position[2]*=-256;
            v.visibility_enabled=2;v.visibility_a[0]=v.visibility_b[1]=v.visibility_c[2]=1;
            v.group_b[0]=16;v.group_c[0]=8.25F;v.group_c[1]=256;
            accepted[i]=v;accepted[i].group_c[0]=3.5F;
        } else if(variant==2) {
            v.position[0]=0;v.position[2]=-2.4F;v.texture[3]=4U|134217728U;
            v.billboard[0]=(i?1.F:-1.F)/256;v.group_a[0]=280;v.group_a[1]=570;
            accepted[i]=v;accepted[i].group_a[0]=330;accepted[i].group_a[1]=635;
        }
    }
    render::CalibratedSceneDraw draw;draw.vertices=current;draw.topology=vr::SceneTopology::lines;draw.effect_layer=2;
    draw.model=id;draw.model[12]=.08F;draw.model[14]=-.03F;
    render::CalibratedSceneMotionDraw previous_draw;previous_draw.valid=true;
    previous_draw.previous_model=id;previous_draw.previous_model[12]=-.17F;previous_draw.previous_vertices=accepted;
    render::GpuCalibratedScene scene;check(scene.initialize(gpu.device,format),scene.status().c_str());
    Texture colour(gpu,w,h,format),ordinary(gpu,w,h,format),underlay(gpu,w,h,format),result(gpu,w,h,format),
        owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
        motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),depth(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
    const std::array<float,4> clear{.04F,.23F,.5F,1};
    unsigned covered_total{},compared_total{};
    for(unsigned eye=0;eye<2;++eye) {
        vr::EyeCamera camera{id,{.91F,0,0,0,0,1.17F,0,0,-.045F,.033F,-1.005F,-1,0,0,-.10025F,0}};
        camera.view[12]=eye?.065F:-.045F;
        auto prior=camera;prior.view[12]=eye?.035F:-.09F;prior.projection[0]=1.02F;prior.projection[5]=1.08F;
        prior.projection[8]=.018F;prior.projection[9]=-.025F;
        if(variant==3) prior.view[14]=6; // Accepted endpoints are now behind the eye.
        const render::CalibratedSceneMotion history{prior,std::span(&previous_draw,1),w,h};
        gpu.begin();check(scene.upload(gpu.command,std::span(&draw,1)),scene.status().c_str());
        const auto cost=scene.upload_cost();
        check(scene.enqueue_eye(gpu.command,colour.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
            owner.texture,0,surface.texture,false,{},motion.texture,&history),scene.status().c_str());
        check(scene.upload_cost()[0]==cost[0]+accepted.size()*sizeof(vr::SceneVertex),"Line accepted endpoints were not uploaded once");
        check(scene.enqueue_eye(gpu.command,ordinary.texture,depth.texture,w,h,camera,clear),scene.status().c_str());
        check(scene.enqueue_eye(gpu.command,underlay.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::world_only),scene.status().c_str());
        const auto token=scene.upload_token();gpu.submit();check(scene.notify_submitted(token),"Line source token was lost");
        const auto source=colour.read(),background=underlay.read(),tags=owner.read();
        check(source==ordinary.read(),"Line motion changed original raster colour or coverage");
        std::vector<std::array<float,4>> measured(w*h),surfaces(w*h);
        const auto m=motion.read(),s=surface.read();std::memcpy(measured.data(),m.data(),m.size());std::memcpy(surfaces.data(),s.data(),s.size());
        const std::array<Point,2> now{source_eye(current[0],draw.model,camera),source_eye(current[1],draw.model,camera)},
            old{source_eye(accepted[0],previous_draw.previous_model,prior),source_eye(accepted[1],previous_draw.previous_model,prior)};
        const auto projected=[&](Point p,const vr::EyeCamera& c){return std::array<double,2>{
            (p[0]*c.projection[0]/-p[2]-c.projection[8]+1)*w/2,
            (1-p[1]*c.projection[5]/-p[2]+c.projection[9])*h/2};};
        const auto a=projected(now[0],camera),b=projected(now[1],camera);
        check(std::abs(a[1]-b[1])<.00001,"Line oracle requires an authored horizontal projected segment");
        std::vector<render::MotionBlurGuide> guides(w*h);unsigned covered{};
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned i=y*w+x;if(tags[i*4+2]!=2) {
                check(measured[i]==std::array<float,4>{} && surfaces[i]==std::array<float,4>{},"Excluded pixel inherited line history");
                guides[i]={0,0,0,false,tags[i*4+2]==1};continue;
            }
            ++covered;double fraction=(x+.5-a[0])/(b[0]-a[0]);
            check(fraction>=-.01 && fraction<=1.01 && std::abs(y+.5-a[1])<=1,"Raster line escaped its independent projected segment");
            double q0=(1-fraction)/-now[0][2],q1=fraction/-now[1][2],sum=q0+q1;q0/=sum;q1/=sum;
            Point n{},p{};for(unsigned k=0;k<3;++k){n[k]=q0*now[0][k]+q1*now[1][k];p[k]=q0*old[0][k]+q1*old[1][k];}
            const auto current_pixel=projected(n,camera),previous_pixel=projected(p,prior);
            check(surfaces[i][0]==0 && surfaces[i][1]==0 && surfaces[i][2]==0,"Raster line invented a plane normal");
            check(std::abs(surfaces[i][3]+n[2]*256)<.025,"Line depth differs from scalar perspective endpoint interpolation");
            const bool valid=variant!=3 && -p[2]>0;
            if(valid) {
                const std::array<double,4> expected{previous_pixel[0]-current_pixel[0],previous_pixel[1]-current_pixel[1],-p[2]*256,1};
                for(unsigned k=0;k<4;++k) {
                    if(std::abs(measured[i][k]-expected[k])>=(k==2?.025:.003))
                        std::cerr<<"line variant="<<variant<<" eye="<<eye<<" pixel="<<x<<','<<y<<" field="<<k
                            <<" actual="<<measured[i][k]<<" expected="<<expected[k]<<'\n';
                    check(std::abs(measured[i][k]-expected[k])<(k==2?.025:.003),"Line guide differs from independent accepted endpoint projection");++compared_total;
                }
                guides[i]={float(expected[0]),float(expected[1]),float(-n[2]*256),true,true};
            } else {
                check(measured[i]==std::array<float,4>{},"Behind-eye accepted line retained velocity");
                guides[i]={0,0,float(-n[2]*256),false,true};
            }
        }
        check(covered>20,"Line fixture lacks visible source coverage");covered_total+=covered;
        render::GpuCalibratedMotionBlur blur;check(blur.initialize(gpu.device,format),blur.status().c_str());
        auto settings=render::motion_blur_preset(2);settings.interval_seconds=1./60;Bytes expected;
        check(render::reconstruct_motion_blur(w,h,source,background,guides,settings,expected),"Independent line shutter rejected its guides");
        gpu.begin();void* output{};check(blur.enqueue(gpu.command,colour.texture,underlay.texture,owner.texture,surface.texture,motion.texture,
            result.texture,w,h,settings,true,output),blur.status().c_str());gpu.submit();
        compare(result.read(),expected,source,tags,srgb(format)?2:1,"native line raster shutter");
        gpu.begin();check(scene.enqueue_eye(gpu.command,colour.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::after_rays,
            owner.texture,token,surface.texture,false,{},motion.texture,&history),scene.status().c_str());gpu.submit();
        check(scene.upload_cost()[0]==cost[0]+accepted.size()*sizeof(vr::SceneVertex),"Line continuation reuploaded accepted endpoints");
        check(colour.read()==source,"Line continuation changed the retained source colour");
    }
    std::cout<<"Native line motion format="<<unsigned(format)<<" variant="<<variant<<": "<<covered_total
        <<" visible pixels, "<<compared_total<<" independently projected guide fields; exact ordinary colour, zero invented normals, resident shutter and retained continuation passed.\n";
}
