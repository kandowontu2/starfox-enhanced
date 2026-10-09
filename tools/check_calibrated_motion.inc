// Independent XR-pose/local-plane oracle. Downloads are fixture-only.
struct MotionModel {
    double angle{};std::array<double,3> scale{1,1,1},offset{};
    vr::Matrix4 matrix() const {
        const double c=std::cos(angle),s=std::sin(angle);auto m=identity;
        m[0]=float(c*scale[0]);m[1]=float(s*scale[0]);
        m[4]=float(-s*scale[1]);m[5]=float(c*scale[1]);m[10]=float(scale[2]);
        for(unsigned i=0;i<3;++i) m[12+i]=float(offset[i]);return m;
    }
    std::array<double,3> local(std::array<double,3> p,bool direction=false) const {
        if(!direction) for(unsigned i=0;i<3;++i) p[i]-=offset[i];
        const double c=std::cos(angle),s=std::sin(angle);
        return {(c*p[0]+s*p[1])/scale[0],(-s*p[0]+c*p[1])/scale[1],p[2]/scale[2]};
    }
    std::array<double,3> world(std::array<double,3> p) const {
        const double c=std::cos(angle),s=std::sin(angle);
        return {c*scale[0]*p[0]-s*scale[1]*p[1]+offset[0],
            s*scale[0]*p[0]+c*scale[1]*p[1]+offset[1],scale[2]*p[2]+offset[2]};
    }
};
void native_temporal_geometry_tests(Gpu&,render::GpuCalibratedScene&,bool);
std::array<double,3> motion_rotate(XrQuaternionf q,std::array<double,3> p,bool inverse=false) {
    const double sign=inverse?-1:1,x=q.x*sign,y=q.y*sign,z=q.z*sign,w=q.w;
    return {(1-2*(y*y+z*z))*p[0]+2*(x*y-z*w)*p[1]+2*(x*z+y*w)*p[2],
        2*(x*y+z*w)*p[0]+(1-2*(x*x+z*z))*p[1]+2*(y*z-x*w)*p[2],
        2*(x*z-y*w)*p[0]+2*(y*z+x*w)*p[1]+(1-2*(x*x+y*y))*p[2]};
}
void native_motion_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    struct Resources {
        SDL_GPUDevice* device;SDL_GPUTexture *receiver{},*surface{},*motion{};SDL_GPUTransferBuffer* read{};
        ~Resources() {SDL_WaitForGPUIdle(device);for(auto* t:{receiver,surface,motion}) if(t) SDL_ReleaseGPUTexture(device,t);
            if(read) SDL_ReleaseGPUTransferBuffer(device,read);}
    } r{gpu.device};
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=width;info.height=height;
    info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;r.receiver=SDL_CreateGPUTexture(gpu.device,&info);
    info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    r.surface=SDL_CreateGPUTexture(gpu.device,&info);r.motion=SDL_CreateGPUTexture(gpu.device,&info);
    const SDL_GPUTransferBufferCreateInfo read_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*36,0};
    r.read=SDL_CreateGPUTransferBuffer(gpu.device,&read_info);check(r.receiver && r.surface && r.motion && r.read,SDL_GetError());
    auto wall=quad(.04F,-.09F,-2.3F,.67F,{.7F,.2F,.1F,1});
    for(auto& v:wall) v.position[2]=-2.3F+.18F*v.position[0]+.11F*v.position[1];
    auto ink=quad(-.08F,.02F,-1.5F,.13F,{.1F,.8F,.4F,1});
    auto alpha=quad(.28F,-.08F,-1.4F,.10F,{.8F,.7F,.1F,.5F});
    std::array<render::CalibratedSceneDraw,3> draws{};
    draws[0].vertices=wall;draws[0].effect_layer=2;
    draws[1].vertices=ink;draws[1].preserve_native_colour=true;draws[1].after_rays=true;
    draws[2].vertices=alpha;draws[2].blend=vr::SceneBlend::alpha;draws[2].after_rays=true;
    const std::array<std::uint32_t,4> cutout{0xffbb2244U,0,0,0xff449966U};
    std::array<unsigned,2> plain_coverage{};unsigned holes{};
    std::uint64_t checked{},protected_pixels{},changed{};
    for(unsigned variant=0;variant<10;++variant) for(unsigned e=0;e<2;++e) {
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
        eye.pose.position={e?.075F:-.055F,.013F,-.018F};eye.fov={-.56F,.63F,.45F,-.51F};
        auto prior_eye=eye;MotionModel now,then;
        if(variant==1 || variant==3 || variant==4 || variant==5) {
            now.offset={.12,-.07,-.24};then.offset={-.09,.03,.06};
        }
        if(variant==2 || variant==3 || variant==4 || variant==5) {
            now.angle=.12;now.scale={1.14,.82,1.07};then.angle=-.08;then.scale={.91,1.19,.98};
        }
        if(variant>=3 && variant<=5) {
            eye.pose.orientation={0,float(std::sin(.03)),0,float(std::cos(.03))};
            prior_eye.pose.orientation={0,float(std::sin(-.0175)),0,float(std::cos(-.0175))};
            prior_eye.pose.position.x+=.041F;prior_eye.pose.position.y-=.023F;prior_eye.pose.position.z+=.081F;
            prior_eye.fov={-.59F,.61F,.49F,-.44F};
        }
        if(variant==7) then.offset[2]=6; // Previous surface lies behind its eye.
        if(variant==8) {
            constexpr unsigned uv[6][2]{{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
            for(unsigned i=0;i<wall.size();++i) {
                wall[i].texture[1]=wall[i].texture[2]=1;wall[i].texture[3]=1;
                wall[i].uv[0]=float(uv[i][0])*2;wall[i].uv[1]=float(uv[i][1])*2;
            }
            draws[0].texels=cutout;
        }
        std::array<double,2> jitter{},previous_jitter{};
        if(variant==9) {
            // Explicit pixel-sized sample offsets, not an altered XR pose.
            jitter={-.31,.27};previous_jitter={.19,-.13};
            for(auto& v:wall) std::fill(std::begin(v.texture),std::end(v.texture),0U);
            draws[0].texels={};
        }
        auto camera=camera_for(eye,d3d),old_camera=camera_for(prior_eye,d3d);
        camera.projection[8]-=float(2*jitter[0]/width);camera.projection[9]+=float(2*jitter[1]/height);
        old_camera.projection[8]-=float(2*previous_jitter[0]/width);old_camera.projection[9]+=float(2*previous_jitter[1]/height);
        draws[0].model=now.matrix();const auto baseline=render(gpu,scene,draws,camera,{.03F,.07F,.11F,1});
        std::array<render::CalibratedSceneMotionDraw,3> prior{};
        prior[0].previous_model=then.matrix();prior[0].valid=variant!=6;
        render::CalibratedSceneMotion history{old_camera,prior,variant==5?width*2:width,variant==5?height*3/2:height};
        const auto encode=[&] {
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(scene.upload(gpu.command,draws),scene.status().c_str());const auto token=scene.upload_token();
            // Reject incomplete/aliased input transactionally, then encode valid guides.
            if(variant==0 && e==0) {
                auto wrong=history;wrong.draws=std::span(prior).first(2);
                check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::before_rays,
                    r.receiver,0,r.surface,true,{},r.motion,&wrong),"Native motion accepted wrong draw history count");
                check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::before_rays,
                    r.receiver,0,r.surface,true,{},r.surface,&history),"Native motion accepted aliased targets");
                check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::before_rays,
                    r.receiver,0,r.surface,true,{},nullptr,&history),"Native motion accepted history without target");
                auto singular=prior;singular[0].previous_model[0]=0;singular[0].previous_model[5]=0;
                wrong.draws=singular;
                check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::before_rays,
                    r.receiver,0,r.surface,true,{},r.motion,&wrong),"Native motion accepted singular previous model");
                wrong=history;wrong.previous_camera.projection[4]=.2F;
                check(!scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::before_rays,
                    r.receiver,0,r.surface,true,{},r.motion,&wrong),"Native motion accepted a noncanonical previous projection");
            }
            const auto cost=scene.upload_cost();
            check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{.03F,.07F,.11F,1},render::CalibratedScenePhase::before_rays,
                r.receiver,0,r.surface,true,{},r.motion,&history),scene.status().c_str());
            (void)gpu.finish();check(scene.notify_submitted(token),"Native motion upload ownership was lost");
            gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
            check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::after_rays,
                r.receiver,token,r.surface,true,{},r.motion,&history),scene.status().c_str());
            check(scene.upload_cost()==cost,"Native motion transferred source geometry a second time");
            auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
            SDL_GPUTextureTransferInfo target{r.read,0,width,height};
            SDL_GPUTextureRegion from{r.receiver,0,0,0,0,0,width,height,1};SDL_DownloadFromGPUTexture(copy,&from,&target);
            from.texture=r.surface;target.offset=width*height*4;SDL_DownloadFromGPUTexture(copy,&from,&target);
            from.texture=r.motion;target.offset=width*height*20;SDL_DownloadFromGPUTexture(copy,&from,&target);SDL_EndGPUCopyPass(copy);
            check(gpu.finish()==baseline,"Native motion MRT changed source colour/depth/alpha/order");
            const auto* bytes=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,r.read,false));check(bytes,SDL_GetError());
            std::vector<unsigned char> data(bytes,bytes+width*height*36);SDL_UnmapGPUTransferBuffer(gpu.device,r.read);return data;
        };
        const auto data=encode();check(encode()==data,"Held native motion guides jittered");
        unsigned coverage=0;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto i=y*width+x;std::array<float,4> surface{},motion{};
            std::memcpy(surface.data(),data.data()+width*height*4+i*16,16);
            std::memcpy(motion.data(),data.data()+width*height*20+i*16,16);
            check(std::all_of(motion.begin(),motion.end(),[](float value){return std::isfinite(value);}),"Native motion guide became nonfinite");
            coverage+=data[i*4+2]==2;
            if(data[i*4+2]!=2 || variant==6 || variant==7) {
                check(motion==std::array<float,4>{},"Excluded/invalid/behind-eye surface inherited motion");++protected_pixels;continue;
            }
            const double rx=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5-jitter[0])/width);
            const double ry=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5-jitter[1])/height);
            const auto direction=now.local(motion_rotate(eye.pose.orientation,{rx,ry,-1}),true);
            const auto origin=now.local({eye.pose.position.x,eye.pose.position.y,eye.pose.position.z});
            const double distance=(-2.3+.18*origin[0]+.11*origin[1]-origin[2])/(direction[2]-.18*direction[0]-.11*direction[1]);
            std::array<double,3> point{};for(unsigned c=0;c<3;++c) point[c]=origin[c]+direction[c]*distance;
            const auto old_world=then.world(point);auto relative=old_world;
            relative[0]-=prior_eye.pose.position.x;relative[1]-=prior_eye.pose.position.y;relative[2]-=prior_eye.pose.position.z;
            const auto old_eye=motion_rotate(prior_eye.pose.orientation,relative,true);
            const auto pixel=reference(prior_eye,old_world);
            const std::array<double,4> expected{(pixel[0]+previous_jitter[0])*history.previous_width/width-(x+.5),
                (pixel[1]+previous_jitter[1])*history.previous_height/height-(y+.5),-old_eye[2]*256,1};
            check(std::abs(surface[3]-distance*256)<.07,"Native motion current depth differs from independent transformed plane");
            for(unsigned c=0;c<4;++c) if(std::abs(motion[c]-expected[c])>(c==2?.08:.012))
                throw std::runtime_error("Native rigid motion differs from independent XR/local-plane oracle: variant="+std::to_string(variant)
                    +" eye="+std::to_string(e)+" xy="+std::to_string(x)+","+std::to_string(y)+" component="+std::to_string(c)
                    +" actual="+std::to_string(motion[c])+" expected="+std::to_string(expected[c]));
            changed+=std::abs(motion[0])+std::abs(motion[1])>.05;checked+=4;
        }
        if(variant==0) plain_coverage[e]=coverage;
        if(variant==8) {
            check(coverage>500 && plain_coverage[e]>coverage+500,"Native motion fixture did not expose texture cutout holes");
            holes+=plain_coverage[e]-coverage;
        }
        check(render(gpu,scene,draws,camera,{.03F,.07F,.11F,1})==baseline,"Native motion OFF altered source pixels");
    }
    check(checked>10000 && changed>1000 && protected_pixels>10000,"Native motion oracle missed coverage/movement/protected ink");
    std::cout<<"Native motion format "<<unsigned(gpu.format)<<": "<<checked<<" independent XR/rigid-plane components, "<<protected_pixels
        <<" excluded/invalid pixels; two asymmetric eyes, current/previous model translation/rotation/nonuniform scale, camera/FOV/resolution changes and pixel-sized sample jitter, "
          "previous depth, "<<holes<<" texture cutout holes, ordered ink/alpha, held/OFF and alias/layout/singular/projection guards passed. Motion core, not native TAA frontend.\n";
    native_temporal_geometry_tests(gpu,scene,d3d);
}
// Real resident connection: calibrated rigid geometry -> the same raster's
// ownership/depth/motion -> temporal resolve. CPU transfers are fixture-only.
void native_temporal_geometry_tests(Gpu& gpu,render::GpuCalibratedScene& scene,bool d3d) {
    struct Resources {
        SDL_GPUDevice* device;SDL_GPUTexture *receiver{},*surface{},*motion{},*resolved{};SDL_GPUTransferBuffer* read{};
        ~Resources() {SDL_WaitForGPUIdle(device);for(auto* t:{receiver,surface,motion,resolved}) if(t) SDL_ReleaseGPUTexture(device,t);
            if(read) SDL_ReleaseGPUTransferBuffer(device,read);}
    } r{gpu.device};
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.width=width;info.height=height;
    info.layer_count_or_depth=info.num_levels=1;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.format=gpu.format;r.resolved=SDL_CreateGPUTexture(gpu.device,&info);
    info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;r.receiver=SDL_CreateGPUTexture(gpu.device,&info);
    info.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;r.surface=SDL_CreateGPUTexture(gpu.device,&info);r.motion=SDL_CreateGPUTexture(gpu.device,&info);
    const SDL_GPUTransferBufferCreateInfo read_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*40,0};
    r.read=SDL_CreateGPUTransferBuffer(gpu.device,&read_info);check(r.resolved && r.receiver && r.surface && r.motion && r.read,SDL_GetError());
    auto wall=quad(.04F,-.09F,-2.3F,.67F,{.7F,.2F,.1F,1});
    for(auto& v:wall) {v.position[2]=-2.3F+.18F*v.position[0]+.11F*v.position[1];v.dither_scale=1;v.odd_color[0]=.1F;v.odd_color[1]=.5F;v.odd_color[2]=.8F;}
    auto ink=quad(-.08F,.02F,-1.5F,.13F,{.1F,.8F,.4F,1});
    auto alpha=quad(.28F,-.08F,-1.4F,.10F,{.8F,.7F,.1F,.5F});
    std::array<render::CalibratedSceneDraw,3> draws{};draws[0].vertices=wall;draws[0].effect_layer=2;
    draws[1].vertices=ink;draws[1].preserve_native_colour=true;draws[1].after_rays=true;
    draws[2].vertices=alpha;draws[2].blend=vr::SceneBlend::alpha;draws[2].after_rays=true;
    std::array<render::GpuCalibratedTemporalAa,2> taa;std::array<render::TemporalAa,2> temporal_reference;
    std::array<XrView,2> accepted_eye{};std::array<MotionModel,2> accepted_model{};
    std::array<vr::EyeCamera,2> accepted_camera{};std::array<std::array<double,2>,2> accepted_jitter{};
    for(auto& t:taa) check(t.initialize(gpu.device,gpu.format),t.status().c_str());
    std::uint64_t compared{},ink_count{},motion_count{};unsigned changed{};
    for(unsigned frame=0;frame<10;++frame) for(unsigned e=0;e<2;++e) {
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;eye.pose.position={e?.075F:-.055F,.013F,-.018F};eye.fov={-.56F,.63F,.45F,-.51F};
        eye.pose.orientation={0,float(std::sin(frame*.0004)),0,float(std::cos(frame*.0004))};
        MotionModel now;now.offset={frame*.006,-double(frame)*.001,0};now.angle=frame*.0003;now.scale={1+frame*.001,1-frame*.001,1};
        const std::array<double,2> jitter{frame%2?-.31:.19,frame%2?.27:-.13};
        auto camera=camera_for(eye,d3d);camera.projection[8]-=float(2*jitter[0]/width);camera.projection[9]+=float(2*jitter[1]/height);
        draws[0].model=now.matrix();std::array<render::CalibratedSceneMotionDraw,3> prior{};
        prior[0].previous_model=accepted_model[e].matrix();prior[0].valid=frame!=0 && frame!=4;
        render::CalibratedSceneMotion history{frame?accepted_camera[e]:camera,prior,width,height};
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());check(scene.upload(gpu.command,draws),scene.status().c_str());
        const auto token=scene.upload_token();
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{.03F,.07F,.11F,1},render::CalibratedScenePhase::before_rays,
            r.receiver,0,r.surface,true,{},r.motion,&history),scene.status().c_str());(void)gpu.finish();check(scene.notify_submitted(token),"Native TAA geometry lost source upload token");
        gpu.command=SDL_AcquireGPUCommandBuffer(gpu.device);check(gpu.command,SDL_GetError());
        check(scene.enqueue_eye(gpu.command,gpu.color,gpu.depth,width,height,camera,{},render::CalibratedScenePhase::after_rays,
            r.receiver,token,r.surface,true,{},r.motion,&history),scene.status().c_str());
        check(taa[e].enqueue(gpu.command,gpu.color,r.receiver,r.surface,r.motion,r.resolved,width,height,{91,.8F}),taa[e].status().c_str());
        if(frame==5) {SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;taa[e].discard();continue;}
        auto* copy=SDL_BeginGPUCopyPass(gpu.command);check(copy,SDL_GetError());
        SDL_GPUTextureTransferInfo target{r.read,0,width,height};SDL_GPUTextureRegion from{r.receiver,0,0,0,0,0,width,height,1};
        SDL_DownloadFromGPUTexture(copy,&from,&target);from.texture=r.surface;target.offset=width*height*4;SDL_DownloadFromGPUTexture(copy,&from,&target);
        from.texture=r.motion;target.offset=width*height*20;SDL_DownloadFromGPUTexture(copy,&from,&target);
        from.texture=r.resolved;target.offset=width*height*36;SDL_DownloadFromGPUTexture(copy,&from,&target);SDL_EndGPUCopyPass(copy);
        const auto raw=gpu.finish();auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(gpu.device,r.read,false));check(mapped,SDL_GetError());
        std::vector<unsigned char> data(mapped,mapped+width*height*40);SDL_UnmapGPUTransferBuffer(gpu.device,r.read);
        std::vector<unsigned char> actual(data.begin()+width*height*36,data.end());
        if(gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || gpu.format==SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
            for(unsigned at=0;at<actual.size();at+=4) std::swap(actual[at],actual[at+2]);
        std::vector<render::TemporalAaGuide> guides(width*height);
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const unsigned i=y*width+x;std::array<float,4> surface{},motion{};
            std::memcpy(surface.data(),data.data()+width*height*4+i*16,16);std::memcpy(motion.data(),data.data()+width*height*20+i*16,16);
            guides[i]={motion[0],motion[1],surface[3],motion[2],motion[3]>0,data[i*4+3]!=0};
            if(!guides[i].eligible || !prior[0].valid) {check(motion==std::array<float,4>{},"Native TAA unknown/protected geometry retained correspondence");continue;}
            const double rx=std::lerp(std::tan(double(eye.fov.angleLeft)),std::tan(double(eye.fov.angleRight)),(x+.5-jitter[0])/width);
            const double ry=std::lerp(std::tan(double(eye.fov.angleUp)),std::tan(double(eye.fov.angleDown)),(y+.5-jitter[1])/height);
            const auto direction=now.local(motion_rotate(eye.pose.orientation,{rx,ry,-1}),true);
            const auto origin=now.local({eye.pose.position.x,eye.pose.position.y,eye.pose.position.z});
            const double distance=(-2.3+.18*origin[0]+.11*origin[1]-origin[2])/(direction[2]-.18*direction[0]-.11*direction[1]);
            std::array<double,3> point{};for(unsigned c=0;c<3;++c) point[c]=origin[c]+direction[c]*distance;
            const auto old_world=accepted_model[e].world(point);auto relative=old_world;
            relative[0]-=accepted_eye[e].pose.position.x;relative[1]-=accepted_eye[e].pose.position.y;relative[2]-=accepted_eye[e].pose.position.z;
            const auto old_eye=motion_rotate(accepted_eye[e].pose.orientation,relative,true);const auto projected=reference(accepted_eye[e],old_world);
            check(std::abs(motion[0]-(projected[0]+accepted_jitter[e][0]-(x+.5)))<.012
                && std::abs(motion[1]-(projected[1]+accepted_jitter[e][1]-(y+.5)))<.012 && std::abs(motion[2]+old_eye[2]*256)<.08,
                "Resident native TAA input differs from accepted rigid/XR oracle");motion_count+=3;
        }
        auto next=temporal_reference[e];const auto expected=next.resolve(width,height,raw,guides,91,.8F);
        for(unsigned at=0;at<actual.size();++at) {
            const unsigned tolerance=(at%4==3 || !guides[at/4].eligible)?0:1;
            if(unsigned(std::abs(int(actual[at])-int(expected[at])))>tolerance) throw std::runtime_error("Resident calibrated raster -> TAA differs from CPU history oracle");
            if(!guides[at/4].eligible) {check(actual[at]==raw[at],"Resident native TAA altered protected ink");++ink_count;}
        }
        compared+=actual.size();changed+=actual!=raw;
        if(frame==6) {taa[e].discard();continue;}
        taa[e].commit();temporal_reference[e]=std::move(next);accepted_eye[e]=eye;accepted_model[e]=now;accepted_camera[e]=camera;accepted_jitter[e]=jitter;
    }
    if(!(compared>100000 && ink_count>100000 && motion_count>10000 && changed>5))
        throw std::runtime_error("Resident native TAA fixture lacked coverage/history response: colour="+std::to_string(compared)
            +" ink="+std::to_string(ink_count)+" motion="+std::to_string(motion_count)+" changed="+std::to_string(changed));
    std::cout<<"Resident native TAA geometry format "<<unsigned(gpu.format)<<": "<<compared<<" colour bytes, "<<ink_count<<" exact protected bytes, "<<motion_count
        <<" independent accepted-XR/rigid motion components; two eyes, actual jittered raster guides, prior depth, moving dithered models, ordered alpha/ink and cancelled/rejected frame recovery passed. No native frontend/physical-display claim.\n";
}
