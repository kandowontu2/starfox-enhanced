#include "starfox/render/camera_response.hpp"
#include "starfox/render/camera_world.hpp"
#include <stdexcept>
#include <limits>
#include <iostream>
using namespace starfox::render;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
bool equal(CameraResponsePose a,CameraResponsePose b) {
    return std::abs(a.pitch-b.pitch)<1e-12 && std::abs(a.yaw-b.yaw)<1e-12 && std::abs(a.roll-b.roll)<1e-12;
}
int main() {
    const std::array<float,16> native_identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    require(camera_response_world_transform({})==native_identity,"native identity response changed the world");
    for(CameraResponsePose pose:std::array<CameraResponsePose,5>{{{.009,0,0},{0,-.009,0},{0,0,.024},{-.012,.009,-.024},{.009,-.007,.024}}}) {
        const auto native=camera_response_world_transform(pose);require(bool(native),"native response rotation rejected");
        require((*native)[3]==0 && (*native)[7]==0 && (*native)[11]==0 && (*native)[12]==0
            && (*native)[13]==0 && (*native)[14]==0 && (*native)[15]==1,"native response translated the rig/eye baseline");
        for(unsigned a=0;a<3;++a) for(unsigned b=0;b<3;++b) {
            double dot=0;for(unsigned k=0;k<3;++k) dot+=double((*native)[a*4+k])*(*native)[b*4+k];
            require(std::abs(dot-double(a==b))<1.e-7,"native response introduced scale or shear");
        }
        for(double fx:{80.,256.,1024.}) for(double fy:{90.,333.})
            for(double x:{-1.,0.,.9}) for(double y:{-.8,0.,.7}) {
            // Geometry transformation must be the inverse of the flat ray
            // sample, including its +Y-down/+Z-forward coordinate conversion.
            const double source[]{x,-y,-3};double dest[3]{};
            for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) dest[r]+=(*native)[c*4+r]*source[c];
            const auto recovered=camera_response_source(camera_response_matrix(pose),
                200+fx*dest[0]/-dest[2],112+fy*-dest[1]/-dest[2],200,112,fx,fy);
            require(recovered && std::abs((*recovered)[0]-(200+fx*x/3))<.00005
                && std::abs((*recovered)[1]-(112+fy*y/3))<.00005,"native response disagrees with flat camera rotation/asymmetric focal axes");
        }
    }
    for(CameraResponsePose invalid:std::array<CameraResponsePose,4>{{{NAN,0,0},{0,INFINITY,0},{0,0,.051},{-.051,0,0}}})
        require(!camera_response_world_transform(invalid),"unbounded/non-finite native response accepted");
    require(!camera_response_moves_world({}),"identity camera scheduled world rendering");
    require(camera_response_moves_world({1e-12,0,0}),"small camera response discarded");
    require(camera_response_moves_world({0,-.001,0}),"yaw response discarded");
    require(camera_response_moves_world({0,0,.001}),"bank response discarded");
    require(!camera_response_moves_world({std::numeric_limits<double>::quiet_NaN(),0,0}),"invalid camera scheduled rendering");
    for(unsigned scale:{1U,2U,4U}) for(unsigned mosaic:{0U,0x31U}) {
        Framebuffer source(12,10,scale),world(20,14,scale);
        source.enable_layer_tags(true);world.enable_layer_tags(true);
        std::fill(world.pixels().begin(),world.pixels().end(),7);
        std::fill(world.layer_tags().begin(),world.layer_tags().end(),std::uint8_t(PixelLayer::background));
        source.enable_dither_pairs(true);
        for(std::size_t i=0;i<source.pixels().size();++i) {
            source.pixels()[i]=std::uint8_t(i%3?7:12);
            source.layer_tags()[i]=std::uint8_t(i%6);
            source.set_dither_alternate(i,13);
        }
        auto reference=world,clean=source;
        clean.clear_dither_pairs();
        for(std::size_t i=0;i<clean.pixels().size();++i) {
            if(clean.layer_tags()[i]==std::uint8_t(PixelLayer::two_d)) clean.pixels()[i]=0;
            else clean.set_dither_alternate(i,13);
        }
        LayerCompositeSettings placement;
        placement.offset_x=-2;placement.offset_y=3;
        placement.clip_left=1;placement.clip_right=9;placement.clip_bottom=11;
        placement.mosaic=std::uint8_t(mosaic);placement.mosaic_layer_mask=1;
        composite_transparent_layer(clean,reference,placement);
        const auto original=source.pixels();
        require(composite_camera_world(source,world,placement),"tagged camera world rejected");
        require(world.pixels()==reference.pixels() && world.layer_tags()==reference.layer_tags(),"camera world clipping/mosaic/HUD mismatch");
        require(std::equal(world.dither_pairs().begin(),world.dither_pairs().end(),reference.dither_pairs().begin(),reference.dither_pairs().end()),"camera world dither provenance mismatch");
        require(source.pixels()==original,"camera extraction mutated source");
        source.enable_layer_tags(false);
        const auto unchanged=world.pixels();
        require(!composite_camera_world(source,world,placement) && world.pixels()==unchanged,"untagged source guessed HUD ownership");
    }
    CameraShotTracker shots;
    const std::array<std::uint64_t,2> volley{0x10001,0x10002};
    require(shots.observe(1,volley)==0,"existing shots triggered initial recoil");
    require(shots.observe(1,volley)==0,"repeated presentation retriggered volley");
    require(shots.observe(1,{})==0,"projectile removal triggered recoil");
    require(shots.observe(1,volley)==1,"dual volley generated wrong recoil count");
    const std::array<std::uint64_t,2> reused{0x20001,0x10002};
    require(shots.observe(1,reused)==2,"reused projectile slot failed recoil");
    require(shots.observe(2,reused)==0,"scene transition retained recoil serial");
    const auto matrix=camera_response_matrix({.01,-.007,.024});
    std::array<double,9> inverse{};
    for(unsigned y=0;y<3;++y) for(unsigned x=0;x<3;++x) inverse[y*3+x]=matrix[x*3+y];
    for(double scale:{1.,2.,4.}) for(double px:{0.,200.,400.}) for(double py:{0.,112.,224.}) {
        const auto original=camera_response_source(camera_response_matrix({}),px,py,200,112,256,256);
        require(original && std::abs((*original)[0]-px)<1e-10 && std::abs((*original)[1]-py)<1e-10,"identity reprojection changed pixels");
        const auto source=camera_response_source(matrix,px*scale,py*scale,200*scale,112*scale,256*scale,256*scale);
        const auto unit=camera_response_source(matrix,px,py,200,112,256,256);
        require(source && unit,"valid response projection rejected");
        require(std::abs((*source)[0]/scale-(*unit)[0])<1e-10 && std::abs((*source)[1]/scale-(*unit)[1])<1e-10,"render upscale changed response angle");
        const auto restored=camera_response_source(inverse,(*unit)[0],(*unit)[1],200,112,256,256);
        require(restored && std::abs((*restored)[0]-px)<1e-10 && std::abs((*restored)[1]-py)<1e-10,"camera reprojection failed inverse");
    }
    require(!camera_response_source(matrix,0,0,0,0,0,256),"invalid projection accepted");
    for(unsigned width:{64U,128U,224U}) {
        constexpr unsigned height=64;
        std::vector<std::uint8_t> world(width*height*4,255),out;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto i=(y*width+x)*4;world[i]=std::uint8_t(32+x/2);world[i+1]=std::uint8_t(32+y*2);world[i+2]=64;
        }
        require(apply_camera_response(world,out,width,height,80,80,{} ) && out==world,"disabled camera pass changed image");
        for(double bank:{-.024,.024}) for(double pitch:{-.018,.009}) {
            require(apply_camera_response(world,out,width,height,80,80,{pitch,.009,bank}),"bounded camera pose failed edge coverage");
            require(out!=world,"camera pass did not move image");
            for(std::size_t i=0;i<out.size();i+=4) require(out[i]>=32 && out[i+1]>=32 && out[i+2]==64 && out[i+3]==255,"camera pass exposed border or damaged alpha");
            auto inplace=world;
            require(apply_camera_response(inplace,inplace,width,height,80,80,{pitch,.009,bank}) && inplace==out,"in-place camera response corrupted source");
        }
        const auto before=out;
        require(!apply_camera_response(world,out,width,height,80,80,{0,2,0}) && out==before,"invalid camera destroyed output");
        auto final=world;
        std::vector<std::uint8_t> coverage(width*height);
        for(unsigned y=20;y<30;++y) for(unsigned x=12;x<40;++x) {
            const auto i=y*width+x;coverage[i]=1;
            // Include opaque black and ink indistinguishable from source world.
            if(x%2) {final[i*4]=0;final[i*4+1]=0;final[i*4+2]=0;}
        }
        const CameraResponsePose pose{.009,-.007,.024};
        std::vector<std::uint8_t> warped,combined;
        require(apply_camera_response(world,warped,width,height,80,80,pose),"world warp failed");
        require(composite_camera_response(world,final,coverage,combined,width,height,80,80,pose),"camera HUD composite failed");
        for(std::size_t i=0;i<coverage.size();++i) for(unsigned c=0;c<4;++c)
            require(combined[i*4+c]==(coverage[i]?final[i*4+c]:warped[i*4+c]),"HUD moved or left stale ink in world");
        auto in_place=final;
        require(composite_camera_response(world,in_place,coverage,in_place,width,height,80,80,pose) && in_place==combined,"HUD source alias corrupted composition");
    }
    CameraResponsePose reference;
    for(unsigned fps:{60U,120U,240U}) {
        CameraResponse camera;
        require(equal(camera.update(0,1,63,100,0,1),{}),"first sample kicked camera");
        CameraResponsePose p;
        for(unsigned i=1;i<=fps;++i) p=camera.update(double(i)/fps,1,63,i>=fps/2?68:100,i>=fps/2?1:0,1);
        if(fps==60) reference=p;else require(equal(reference,p),"response depends on presentation rate");
        require(p.roll>0 && p.roll<=.024,"bank escaped limits");
        require(equal(camera.update(1,1,63,68,1,1),p),"repeated presentation retriggered event");
        require(equal(camera.update(1.5,1,63,68,1,1,true),p),"pause changed response");
        require(equal(camera.update(1.5,1,63,68,1,1),p),"resume advanced response ages");
        require(equal(camera.update(6.5,1,63,68,1,1,true),p),"long pause cleared camera response");
        require(equal(camera.update(6.5,1,63,68,1,1),p),"long pause resume changed response");
        require(equal(camera.update(1.6,2,63,1,8,1),{}),"scene cut retained response");
        require(equal(camera.update(1.7,2,0,0,9,1),{}),"disabled response moved camera");
    }
    CameraResponse camera;
    camera.update(0,1,3,100,0,0);
    camera.update(.1,1,3,68,0,0);
    auto impact=camera.update(.15,1,3,68,0,0);
    require(impact.pitch!=0 && impact.yaw!=0 && impact.roll==0,"impact did not shake presentation axes");
    require(std::abs(impact.pitch)<=.009 && std::abs(impact.yaw)<=.009,"impact exceeded angular bound");
    CameraResponse healed=camera;
    require(equal(camera.update(.2,1,3,68,0,0),healed.update(.2,1,3,100,0,0)),"healing retriggered damage shake");
    camera={};
    camera.update(0,1,12,100,0,0);
    auto recoil=camera.update(.1,1,12,100,1,0);
    require(recoil.pitch<0 && recoil.yaw==0 && recoil.roll==0,"recoil affected wrong axes");
    require(equal(camera.update(0,1,12,100,1,0),{}),"rewind did not clear response");
    require(equal(camera.update(std::numeric_limits<double>::quiet_NaN(),1,63,1,1,0),{}),"invalid time retained pose");
    std::cout<<"Camera response tests passed\n";
}
