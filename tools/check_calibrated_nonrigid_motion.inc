// Diagnostic-only scalar geometry oracle. Production evaluates both source
// payloads on GPU; these equations intentionally do not call its transformer
// or obtain reference velocities/depths by reading the GPU guide textures.
using Point=std::array<double,3>;
Point difference(Point a,Point b) {for(unsigned i=0;i<3;++i) a[i]-=b[i];return a;}
Point cross(Point a,Point b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double dot(Point a,Point b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Point affine(const vr::Matrix4& matrix,Point p) {
    Point result{};
    for(unsigned row=0;row<3;++row) result[row]=matrix[row]*p[0]+matrix[4+row]*p[1]+matrix[8+row]*p[2]+matrix[12+row];
    return result;
}
Point source_eye(const vr::SceneVertex& vertex,const vr::Matrix4& model,const vr::EyeCamera& camera) {
    Point point{vertex.position[0],vertex.position[1],vertex.position[2]};
    const auto word=[](int value) {return int(std::uint32_t(value)&65535U)-((std::uint32_t(value)&32768U)?65536:0);};
    const auto rotate=[&](Point value) {
        Point rotated{};const float* rows[]{vertex.visibility_a,vertex.visibility_b,vertex.visibility_c};
        for(unsigned axis=0;axis<3;++axis) {
            if(vertex.group_c[2]) {
                int sum=0;
                for(unsigned c=0;c<3;++c) {
                    const auto product=std::int64_t(std::lround(rows[axis][c]*32768.))*word(int(std::lround(value[c])));
                    // Mathematical floor implements signed Q15 shift, including
                    // negative terms, without implementation-defined shifts.
                    sum+=int(std::floor(double(product)/32768));
                }
                rotated[axis]=word(sum);
            } else for(unsigned c=0;c<3;++c) rotated[axis]+=rows[axis][c]*value[c];
        }
        return rotated;
    };
    if(vertex.visibility_enabled==2) {
        point=rotate(point);
        auto normal=rotate({vertex.group_b[0],vertex.group_b[1],vertex.group_b[2]});normal[1]=-std::abs(normal[1]);
        const double phase=vertex.group_c[0],fraction=phase-std::floor(phase);
        for(unsigned axis=0;axis<3;++axis) {
            point[axis]=vertex.group_c[2]?word(int(point[axis])+int(std::lround(vertex.group_a[axis]))):point[axis]+vertex.group_a[axis];
            const double n=std::lround(normal[axis]);
            point[axis]+=std::lerp(std::floor(n*std::floor(phase)/4),std::floor(n*std::ceil(phase)/4),fraction);
            point[axis]*=(axis? -1.:1.)/vertex.group_c[1];
        }
    }
    Point eye=affine(camera.view,affine(model,point));
    double dx=vertex.billboard[0],dy=vertex.billboard[1];
    if(vertex.texture[3]&134217728U) {
        const double dimension=std::clamp(std::trunc(vertex.group_a[0]*256./vertex.group_a[1]),0.,240.);
        dx*=dimension*vertex.group_a[1]/512.;dy*=dimension*vertex.group_a[1]/512.;
    }
    if(vertex.texture[3]&4U) {
        for(unsigned axis=0;axis<2;++axis) {
            Point basis{};basis[axis]=1;
            auto transformed=affine(camera.view,affine(model,basis));
            auto origin=affine(camera.view,affine(model,{}));
            const auto vector=difference(transformed,origin);
            const double scale=std::sqrt(dot(vector,vector))/(vertex.visibility_enabled==2?vertex.group_c[1]:1);
            eye[axis]+=(axis?dy:dx)*scale;
        }
    }
    return eye;
}
// Intersect the current eye ray with the current deformed triangle, then use
// its actual perspective barycentrics to reconstruct the accepted position.
bool triangle_hit(Point ray,const std::array<Point,3>& triangle,Point& barycentric,double& distance) {
    const auto e1=difference(triangle[1],triangle[0]),e2=difference(triangle[2],triangle[0]);
    const auto p=cross(ray,e2);const double determinant=dot(e1,p);
    if(std::abs(determinant)<1.e-12) return false;
    const auto from=difference({},triangle[0]);const double u=dot(from,p)/determinant;
    const auto q=cross(from,e1);const double v=dot(ray,q)/determinant;
    distance=dot(e2,q)/determinant;barycentric={1-u-v,u,v};
    return distance>0;
}
void native_nonrigid(Gpu& gpu,SDL_GPUTextureFormat format,unsigned variant,unsigned samples=1) {
    constexpr unsigned w=88,h=54;
    constexpr vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    constexpr int corners[][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
    std::vector<vr::SceneVertex> current(6),accepted(6),world(6),ink(6);
    std::vector<std::uint32_t> texels;
    for(unsigned i=0;i<6;++i) {
        auto& v=current[i];v.color[0]=.8F;v.color[1]=.27F;v.color[2]=.08F;v.color[3]=1;
        std::copy(std::begin(v.color),std::end(v.color),v.odd_color);
        if(variant<2) {
            v.position[0]=corners[i][0]*135.F;v.position[1]=corners[i][1]*84.F;v.position[2]=550;
            v.visibility_enabled=2;v.visibility_a[0]=.97F;v.visibility_a[2]=.24F;
            v.visibility_b[1]=1;v.visibility_c[0]=-.24F;v.visibility_c[2]=.97F;
            v.group_a[0]=24;v.group_a[1]=-12;v.group_a[2]=36;
            v.group_b[0]=-11;v.group_b[1]=7;v.group_b[2]=18;
            v.group_c[0]=8.25F;v.group_c[1]=256;v.group_c[2]=variant;
            accepted[i]=v;auto& old=accepted[i];old.visibility_a[0]=old.visibility_c[2]=.99F;
            old.visibility_a[2]=.14F;old.visibility_c[0]=-.14F;
            old.group_a[0]=-9;old.group_a[1]=8;old.group_a[2]=-15;old.group_c[0]=3.5F;
        } else {
            v.position[0]=-.1F;v.position[1]=.03F;v.position[2]=-2.1F;
            v.texture[3]=4;v.billboard[0]=corners[i][0]*.55F;v.billboard[1]=corners[i][1]*.38F;
            if(variant>=3 && variant<=5) {
                v.texture[3]|=134217728U;v.group_a[0]=280;v.group_a[1]=570;
                v.billboard[0]=corners[i][0]/256.F;v.billboard[1]=corners[i][1]/256.F;
            }
            accepted[i]=v;
            if(variant>=3 && variant<=5) {accepted[i].group_a[0]=330;accepted[i].group_a[1]=variant==4?120.F:635.F;}
        }
        if(variant==6) {
            v.texture[3]|=1;v.texture[1]=v.texture[2]=3;
            v.uv[0]=corners[i][0]>0?4.F:0.F;v.uv[1]=corners[i][1]>0?4.F:0.F;accepted[i]=v;
        }
        auto& back=world[i];back.position[0]=corners[i][0]*5.F;back.position[1]=corners[i][1]*3.F;back.position[2]=-6;
        back.color[0]=.12F;back.color[1]=.37F;back.color[2]=.55F;back.color[3]=1;std::copy(std::begin(back.color),std::end(back.color),back.odd_color);
        auto& protected_ink=ink[i];protected_ink=back;protected_ink.position[0]=.5F+corners[i][0]*.14F;
        protected_ink.position[1]=-.1F+corners[i][1]*.12F;protected_ink.position[2]=-1;
        protected_ink.color[0]=.2F;protected_ink.color[1]=.85F;protected_ink.color[2]=.15F;
        std::copy(std::begin(protected_ink.color),std::end(protected_ink.color),protected_ink.odd_color);
    }
    if(variant==6) for(unsigned i=0;i<16;++i) texels.push_back(i%3?0xff186cd4U:0);
    std::array<render::CalibratedSceneDraw,3> draws{};draws[0].vertices=current;draws[0].texels=texels;draws[0].effect_layer=2;
    draws[1].vertices=world;draws[1].effect_layer=1;draws[2].vertices=ink;draws[2].preserve_native_colour=true;
    if(variant>=2) {draws[0].model[0]=1.13F;draws[0].model[5]=.91F;draws[0].model[12]=.14F;draws[0].model[14]=.08F;}
    std::array<render::CalibratedSceneMotionDraw,3> history{};history[0].valid=true;history[0].previous_vertices=accepted;
    history[0].previous_model=identity;history[0].previous_model[0]=.93F;history[0].previous_model[5]=1.07F;
    history[0].previous_model[12]=-.19F;history[0].previous_model[13]=.07F;history[0].previous_model[14]=-.15F;
    render::GpuCalibratedScene scene;check(scene.initialize(gpu.device,format),scene.status().c_str());
    check(scene.supports_multisample(samples,true,true),"Required native sample count unsupported; no substitute");
    Texture source0(gpu,w,h,format,samples),source1(gpu,w,h,format,samples),baseline(gpu,w,h,format,samples),underlay(gpu,w,h,format,samples),
        owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,samples),surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,samples),
        motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,samples),result(gpu,w,h,format),depth(gpu,w,h,SDL_GPU_TEXTUREFORMAT_D32_FLOAT,samples);
    struct SamplePlanes {
        Texture color,owner,surface,motion,baseline,underlay;
        SamplePlanes(Gpu& gpu,unsigned w,unsigned h,SDL_GPUTextureFormat format):color(gpu,w,h,format),
            owner(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),surface(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
            motion(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),baseline(gpu,w,h,format),underlay(gpu,w,h,format) {}
    };
    std::array<std::unique_ptr<SamplePlanes>,8> planes;
    std::array<std::array<std::unique_ptr<Texture>,8>,2> retained;
    render::GpuCalibratedMsaa transport;
    if(samples>1) {
        check(transport.initialize(gpu.device,format),transport.status().c_str());
        for(unsigned sample=0;sample<samples;++sample) {
            planes[sample]=std::make_unique<SamplePlanes>(gpu,w,h,format);
            for(unsigned eye=0;eye<2;++eye) retained[eye][sample]=std::make_unique<Texture>(gpu,w,h,format);
        }
    }
    const render::CalibratedSceneMultisample multisample{samples,nullptr,samples>1};
    const auto extract=[&](SDL_GPUTexture* raw,Texture& output,unsigned sample) {
        auto& p=*planes[sample];check(transport.enqueue_sample(gpu.command,raw,owner.texture,surface.texture,
            output.texture,p.owner.texture,p.surface.texture,w,h,samples,sample,motion.texture,p.motion.texture),transport.status().c_str());
    };
    const std::array<float,4> clear{.06F,.17F,.24F,1};
    const auto camera_for=[&](unsigned eye) {
        vr::EyeCamera camera{identity,{.88F,0,0,0,0,1.16F,0,0,-.06F,.035F,-1.005F,-1,0,0,-.10025F,0}};
        const float angle=.08F,cs=std::cos(angle),sn=std::sin(angle);
        camera.view[0]=camera.view[10]=cs;camera.view[2]=-sn;camera.view[8]=sn;
        camera.view[12]=eye?.04F:-.04F;camera.view[13]=.02F;camera.view[14]=-.1F;return camera;
    };
    gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());const auto source_cost=scene.upload_cost();
    std::array<std::array<Bytes,8>,2> first_colour;
    for(unsigned eye=0;eye<2;++eye) {
        const auto camera=camera_for(eye);auto previous=camera;previous.view=identity;
        previous.view[12]=eye?.065F:-.025F;previous.view[13]=-.03F;previous.view[14]=variant==5?4.F:.04F;
        previous.projection[0]=.96F;previous.projection[5]=1.04F;previous.projection[8]=.025F;previous.projection[9]=-.02F;
        const render::CalibratedSceneMotion prior{previous,history,w,h};
        auto* source=eye?source1.texture:source0.texture;
        check(scene.enqueue_eye(gpu.command,source,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
            owner.texture,0,surface.texture,false,multisample,motion.texture,&prior),scene.status().c_str());
        check(scene.upload_cost()[0]==source_cost[0]+accepted.size()*sizeof(vr::SceneVertex),"Accepted primitive upload was duplicated between eyes");
        if(samples>1 && eye==0 && variant==1 && format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) {
            auto& p=*planes[0];
            for(unsigned fault=0;fault<6;++fault) {
                void* old_motion=motion.texture;void* new_motion=p.motion.texture;
                void* old_surface=surface.texture;void* new_surface=p.surface.texture;
                if(fault==0) old_motion=nullptr;
                if(fault==1) new_motion=nullptr;
                if(fault==2) old_surface=new_surface=nullptr;
                if(fault==3) old_motion=old_surface;
                if(fault==4) new_motion=new_surface;
                if(fault==5) new_motion=source;
                check(!transport.enqueue_sample(gpu.command,source,owner.texture,old_surface,p.color.texture,p.owner.texture,new_surface,
                    w,h,samples,0,old_motion,new_motion),"Native MSAA motion accepted missing/aliased source or output planes");
            }
        }
        if(samples>1) for(unsigned sample=0;sample<samples;++sample) extract(source,*retained[eye][sample],sample);
    }
    const auto token=scene.upload_token();gpu.submit();check(scene.notify_submitted(token),"Accepted primitive submission token rejected");
    for(unsigned eye=0;eye<2;++eye) for(unsigned sample=0;sample<samples;++sample)
        first_colour[eye][sample]=samples>1?retained[eye][sample]->read():(eye?source1:source0).read();
    check(first_colour[0][0]!=first_colour[1][0],"Nonrigid motion collapsed the calibrated eye poses");
    // Re-raster each eye with explicit new source commands to compare exact
    // ordinary-colour coverage, guide payload and the resident blur consumer.
    for(unsigned eye=0;eye<2;++eye) {
        const auto camera=camera_for(eye);auto previous=camera;previous.view=identity;
        previous.view[12]=eye?.065F:-.025F;previous.view[13]=-.03F;previous.view[14]=variant==5?4.F:.04F;
        previous.projection[0]=.96F;previous.projection[5]=1.04F;previous.projection[8]=.025F;previous.projection[9]=-.02F;
        const render::CalibratedSceneMotion prior{previous,history,w,h};
        gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());
        check(scene.enqueue_eye(gpu.command,source0.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
            owner.texture,0,surface.texture,false,multisample,motion.texture,&prior),scene.status().c_str());
        check(scene.enqueue_eye(gpu.command,baseline.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
            owner.texture,0,surface.texture,false,multisample),scene.status().c_str());
        check(scene.enqueue_eye(gpu.command,underlay.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::world_only,
            nullptr,0,nullptr,false,multisample),scene.status().c_str());
        if(samples>1) for(unsigned sample=0;sample<samples;++sample) {
            extract(source0.texture,planes[sample]->color,sample);extract(baseline.texture,planes[sample]->baseline,sample);
            extract(underlay.texture,planes[sample]->underlay,sample);
        }
        gpu.submit();
        for(unsigned sample=0;sample<samples;++sample) {
        auto& source_plane=samples>1?planes[sample]->color:source0;
        auto& underlay_plane=samples>1?planes[sample]->underlay:underlay;
        auto& owner_plane=samples>1?planes[sample]->owner:owner;
        auto& surface_plane=samples>1?planes[sample]->surface:surface;
        auto& motion_plane=samples>1?planes[sample]->motion:motion;
        auto& baseline_plane=samples>1?planes[sample]->baseline:baseline;
        const auto original=source_plane.read(),background=underlay_plane.read(),tags=owner_plane.read();
        check(original==baseline_plane.read() && original==first_colour[eye][sample],"Accepted nonrigid producer changed source colour/coverage or retained eye/sample");
        const auto motion_bytes=motion_plane.read(),surface_bytes=surface_plane.read();std::vector<std::array<float,4>> measured(w*h),measured_surface(w*h);
        std::memcpy(measured.data(),motion_bytes.data(),motion_bytes.size());std::memcpy(measured_surface.data(),surface_bytes.data(),surface_bytes.size());
        std::vector<render::MotionBlurGuide> guides(w*h);unsigned model_pixels{},invalid_pixels{};
        constexpr std::array<std::array<int,2>,14> locations{{
#define SF_MSAA_SAMPLE(x,y) {x,y},
#include "starfox/render/msaa_samples.inc"
#undef SF_MSAA_SAMPLE
        }};
        const double dx=samples>1?locations[samples-2+sample][0]/16.:0;
        const double dy=samples>1?locations[samples-2+sample][1]/16.:0;
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
            const unsigned pixel=y*w+x,layer=tags[pixel*4+2];
            if(layer!=2) {guides[pixel]={0,0,0,false,layer==1};check(measured[pixel]==std::array<float,4>{},"Excluded world/ink inherited nonrigid motion");continue;}
            ++model_pixels;Point ray{(2*(x+.5+dx)/w-1+camera.projection[8])/camera.projection[0],
                (1-2*(y+.5+dy)/h+camera.projection[9])/camera.projection[5],-1};
            Point weights{};double z{};unsigned triangle=0;bool found=false;
            for(;triangle<2;++triangle) {
                std::array<Point,3> points;
                for(unsigned i=0;i<3;++i) points[i]=source_eye(current[triangle*3+i],draws[0].model,camera);
                if(!triangle_hit(ray,points,weights,z)) continue;
                std::array<std::array<double,2>,3> projected;
                for(unsigned i=0;i<3;++i) projected[i]={
                    (points[i][0]*camera.projection[0]/-points[i][2]-camera.projection[8]+1)*w/2,
                    (1-points[i][1]*camera.projection[5]/-points[i][2]+camera.projection[9])*h/2};
                const auto edge=[](auto a,auto b,auto p) {return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);};
                const double orientation=edge(projected[0],projected[1],projected[2])>0?1:-1;
                bool covered=true;
                for(unsigned i=0;i<3;++i) {
                    const auto a=projected[i],b=projected[(i+1)%3];
                    // Native raster coverage snaps edges to a 1/256-pixel
                    // grid, while attributes describe the original plane.
                    // Allow that ONE geometric grid unit, not arbitrary
                    // barycentric/guide error or skipped boundary pixels.
                    covered&=orientation*edge(a,b,std::array{x+.5+dx,y+.5+dy})/std::hypot(b[0]-a[0],b[1]-a[1])>=-1./256;
                }
                if(covered) {found=true;break;}
            }
            if(!found) std::cerr<<"source hit format="<<unsigned(format)<<" variant="<<variant<<" samples="<<samples
                <<" sample="<<sample<<" eye="<<eye<<" pixel="<<x<<','<<y<<" weights="<<weights[0]<<','<<weights[1]<<','<<weights[2]<<" depth="<<z<<'\n';
            check(found,"Owned nonrigid pixel has no scalar source-triangle intersection");
            Point old{};
            for(unsigned i=0;i<3;++i) {
                const auto point=source_eye(accepted[triangle*3+i],history[0].previous_model,previous);
                for(unsigned c=0;c<3;++c) old[c]+=weights[i]*point[c];
            }
            const bool valid=variant!=4 && -old[2]>0;
            const double ox=(old[0]*previous.projection[0]/-old[2]-previous.projection[8]+1)*w/2;
            const double oy=(1-old[1]*previous.projection[5]/-old[2]+previous.projection[9])*h/2;
            check(std::abs(measured_surface[pixel][3]-z*256)<.025,"Current nonrigid surface depth differs from scalar source triangle");
            if(valid) {
                const std::array<double,4> expected{ox-(x+.5+dx),oy-(y+.5+dy),-old[2]*256,1};
                for(unsigned c=0;c<4;++c) check(std::abs(measured[pixel][c]-expected[c])<(c==2?.025:.003),
                    "Nonrigid guide differs from accepted Q15/billboard perspective-barycentric reference");
                guides[pixel]={float(expected[0]),float(expected[1]),float(z*256),true,true};
            } else {check(measured[pixel]==std::array<float,4>{},"Invisible/behind-eye accepted primitive inherited velocity");guides[pixel]={0,0,float(z*256),false,true};++invalid_pixels;}
        }
        check(model_pixels>40,"Nonrigid fixture lacks visible source geometry");
        if(variant==4 || variant==5) check(invalid_pixels==model_pixels,"Rejected accepted primitive retained motion");
        render::GpuCalibratedMotionBlur blur;check(blur.initialize(gpu.device,format),blur.status().c_str());
        auto settings=render::motion_blur_preset(2);settings.interval_seconds=1./60;Bytes expected;
        check(render::reconstruct_motion_blur(w,h,original,background,guides,settings,expected),"Nonrigid centred shutter reference failed");
        gpu.begin();void* output{};check(blur.enqueue(gpu.command,source_plane.texture,underlay_plane.texture,owner_plane.texture,surface_plane.texture,motion_plane.texture,
            result.texture,w,h,settings,true,output),blur.status().c_str());gpu.submit();
        compare(result.read(),expected,original,tags,srgb(format)?2:1,"native nonrigid raster shutter");
        }
    }
    if(samples==1 && format==SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM && variant==0) {
        const auto camera=camera_for(0);const render::CalibratedSceneMotion prior{camera,history,w,h};
        gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());const auto before=scene.upload_cost();
        for(unsigned fault=0;fault<6;++fault) {
            auto bad_history=history;auto bad_vertices=accepted;
            if(fault==0) bad_vertices.pop_back();
            if(fault==1) bad_history[0].valid=false;
            if(fault==2) bad_vertices[0].group_c[0]=NAN;
            if(fault==3) bad_vertices[0].texture[3]=65536U;
            if(fault==4) bad_vertices[0].group_c[1]=0;
            if(fault==5) bad_vertices[0].group_c[0]=256;
            bad_history[0].previous_vertices=bad_vertices;const render::CalibratedSceneMotion invalid{camera,bad_history,w,h};
            check(!scene.enqueue_eye(gpu.command,source0.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
                owner.texture,0,surface.texture,false,{},motion.texture,&invalid),"Malformed accepted primitive request was accepted");
            check(scene.upload_cost()==before,"Malformed accepted primitive request encoded a partial old-source copy");
        }
        check(scene.enqueue_eye(gpu.command,source0.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
            owner.texture,0,surface.texture,false,{},motion.texture,&prior),scene.status().c_str());gpu.cancel();
        // The cancelled source command must not validate the cached old stream.
        gpu.begin();check(scene.upload(gpu.command,draws),scene.status().c_str());const auto fresh=scene.upload_cost();
        check(scene.enqueue_eye(gpu.command,source0.texture,depth.texture,w,h,camera,clear,render::CalibratedScenePhase::before_rays,
            owner.texture,0,surface.texture,false,{},motion.texture,&prior),scene.status().c_str());
        check(scene.upload_cost()[0]==fresh[0]+accepted.size()*sizeof(vr::SceneVertex),"Cancelled accepted stream was reused without re-encoding");
        const auto submitted=scene.upload_token();gpu.submit();check(scene.notify_submitted(submitted),"Re-encoded accepted stream token rejected");
        gpu.begin();const auto cost=scene.upload_cost();
        check(scene.enqueue_eye(gpu.command,source0.texture,depth.texture,w,h,camera,{},render::CalibratedScenePhase::after_rays,
            owner.texture,submitted,surface.texture,false,{},motion.texture,&prior),scene.status().c_str());
        check(scene.upload_cost()==cost,"Ordered continuation re-uploaded accepted primitives");gpu.submit();
        auto wrong=accepted;wrong[0].group_a[0]++;auto mismatch=history;mismatch[0].previous_vertices=wrong;
        const render::CalibratedSceneMotion changed_prior{camera,mismatch,w,h};gpu.begin();
        check(!scene.enqueue_eye(gpu.command,source0.texture,depth.texture,w,h,camera,{},render::CalibratedScenePhase::after_rays,
            owner.texture,submitted,surface.texture,false,{},motion.texture,&changed_prior),"Continuation introduced an unowned previous source stream");
        check(scene.upload_cost()==cost,"Rejected continuation encoded accepted primitives");gpu.cancel();
    }
    std::cout<<"Accepted nonrigid format="<<unsigned(format)<<" variant="<<variant<<" samples="<<samples
        <<": two-eye exact colour, scalar triangle/Q15/billboard/current-vs-previous Z and actual resident shutter passed.\n";
}
