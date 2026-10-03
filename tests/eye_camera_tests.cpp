#include "starfox/vr/eye_camera.hpp"
#include "starfox/vr/presentation.hpp"
#include "starfox/vr/shadow_camera.hpp"
#include "starfox/vr/source_shadow_environment.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace starfox::vr;
namespace {
void require(bool ok) {if(!ok) throw std::runtime_error("Eye camera assertion failed");}
void close(float a,float b) {require(std::abs(a-b)<1e-4f);}
std::array<float,4> transform(const Matrix4& m,std::array<float,4> p) {
    std::array<float,4> out{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) out[r]+=m[c*4+r]*p[c];
    return out;
}
std::array<float,4> project(const Matrix4& m,std::array<float,4> p) {
    auto result=transform(m,p);const auto w=result[3];
    for(auto& v:result) v/=w;return result;
}
}
int main() {
    try {
        {
            const auto panel=panel_matrix();close(panel[0]*256,1.15F);close(panel[14],-1.75F);
            std::array<XrView,2> views{};
            for(unsigned eye=0;eye<2;++eye) {
                views[eye].pose.orientation={0,std::sqrt(.5F),0,std::sqrt(.5F)};
                views[eye].pose.position={2,1,eye?.032F:-.032F};
            }
            WorldPanelAnchor anchor;const auto pose=anchor.pose(views);
            close(pose.position.x,.25F);close(pose.position.y,1);close(pose.position.z,0);
            views[0].pose.position.x+=1;views[1].pose.position.x+=1;
            close(anchor.pose(views).position.x,.25F);anchor.reset();close(anchor.pose(views).position.x,1.25F);
            PositionAnchor head;head.reset(true);const auto raw=views;require(head.apply(views));
            close(views[0].pose.orientation.y,0);close(views[0].pose.orientation.w,1);
            close(views[1].pose.position.x-views[0].pose.position.x,-.064F);
            close(raw[0].pose.position.x,3); // runtime poses were copied, not rewritten
        }
        {
            EyeCamera camera{};
            for(unsigned i=0;i<16;++i) {camera.view[i]=float(i);camera.projection[i]=float(32+i);}
            camera.effects={8,75,0,0};
            const auto constants=scene_constants(camera);
            for(unsigned row=0;row<3;++row) for(unsigned col=0;col<4;++col)
                close(constants.view_rows[row*4+col],camera.view[col*4+row]);
            require(constants.projection==camera.projection && constants.effects==camera.effects);
        }
        {
            using namespace starfox::render::shadows;
            const std::array<int16_t,9> view{32760,81,19,-70,32761,53,14,-42,32760};
            for(double camera_y:{-32000.,-200.,500.,32768.}) {
                const auto environment=source_shadow_environment(view,camera_y,0,true,-.25);
                require(environment && environment->ground);
                double delta=std::fmod(-camera_y,65536.);if(delta>32767) delta-=65536;else if(delta< -32768) delta+=65536;
                for(double x:{-1200.,0.,1500.}) for(double z:{-900.,3000.}) {
                    const Vec3 point{(x*view[0]+delta*view[3]+z*view[6])/32768./256.,
                        -(x*view[1]+delta*view[4]+z*view[7])/32768./256.,
                        -(x*view[2]+delta*view[5]+z*view[8])/32768./256.-.25};
                    require(std::abs(dot(point-environment->ground->point,environment->ground->normal))<1e-10);
                }
            }
            require(!source_shadow_environment({},0,0,true));
            require(!source_shadow_environment(view,std::numeric_limits<double>::quiet_NaN(),0,true));
            require(!source_shadow_environment(view,0,0,false)->ground);
        }
        {
            const auto pair=sbs_eye_cameras(1.2F,16.F/9.F,6.4F,100,1,1000);
            require(pair.has_value());
            const auto point=[&](unsigned eye,float depth) {
                return project((*pair)[eye].projection,
                    transform((*pair)[eye].view,{12,4,-depth,1}));
            };
            close(point(0,100)[0],point(1,100)[0]);
            for(float depth:{50.F,100.F,200.F}) {
                close(point(0,depth)[1],point(1,depth)[1]);
                close(point(0,depth)[2],point(1,depth)[2]);
            }
            require(point(0,50)[0]>point(1,50)[0]);
            require(point(0,200)[0]<point(1,200)[0]);
            require(!sbs_eye_cameras(0,1,6.4F,100,1));
            require(!sbs_eye_cameras(1.2F,0,6.4F,100,1));
            require(!sbs_eye_cameras(1.2F,1,6.4F,0,1));
        }
        {
            const auto ppu=source_layer_matrix(128,112).value();
            require(source_ui_layer_matrix(76,92,true).value()==ppu);
            require(source_ui_layer_matrix(172,172,true).value()==ppu);
            require(source_ui_layer_matrix(76,92,false).value()==source_layer_matrix(92,108).value());
            require(source_ui_layer_matrix(172,172,true,true).value()==source_layer_matrix(112,96).value());
            const auto fx=source_layer_matrix(112,96).value();
            const auto sprite=transform(ppu,{24,194,0,1});
            const auto meter=transform(fx,{8,178,0,1});
            for(unsigned i=0;i<4;++i) close(sprite[i],meter[i]);
        }
        {
            PositionAnchor anchor;
            std::array<XrView,2> views{};
            views[0].pose.position={2.F-.032F,1.7F,-3.F};
            views[1].pose.position={2.F+.032F,1.7F,-3.F};
            auto moved=views;
            require(anchor.apply(views));
            close(views[0].pose.position.x,-.032F);close(views[1].pose.position.x,.032F);
            close(views[0].pose.position.y,0);close(views[1].pose.position.z,0);
            XrPosef hand{};hand.orientation.w=1;hand.position={2.3F,1.5F,-3.5F};
            const auto anchored_hand=anchor.anchored(hand);
            close(anchored_hand.position.x,.3F);close(anchored_hand.position.y,-.2F);close(anchored_hand.position.z,-.5F);
            for(auto& view:moved) {view.pose.position.x+=.2F;view.pose.position.y-=.1F;}
            require(anchor.apply(moved));
            close(moved[0].pose.position.x,.168F);close(moved[1].pose.position.x,.232F);
            close(moved[0].pose.position.y,-.1F);
            const auto raw=views;
            for(float factor:{0.F,.5F,1.F,1.5F,2.F}) {
                PositionAnchor calibrated;auto initial=raw;require(calibrated.apply(initial,factor));
                auto translated=raw;
                for(auto& eye:translated) {eye.pose.position.x+=.4F;eye.pose.position.z-=.2F;}
                require(calibrated.apply(translated,factor));
                close((translated[0].pose.position.x+translated[1].pose.position.x)*.5F,.4F*factor);
                close(translated[1].pose.position.x-translated[0].pose.position.x,.064F);
                close(translated[0].pose.position.z,-.2F*factor);
                require(translated[0].fov.angleLeft==raw[0].fov.angleLeft);
            }
            PositionAnchor invalid;
            auto bad=views;bad[1].pose.position.x=std::numeric_limits<float>::quiet_NaN();
            require(!invalid.apply(bad));
            require(invalid.apply(views));close(views[0].pose.position.x,-.032F);
        }
        {
            // Recentre keeps standing height; recalibration (3 s hold) takes it.
            PositionAnchor anchor;
            std::array<XrView,2> views{};
            views[0].pose.position={-.032F,1.7F,0.F};views[1].pose.position={.032F,1.7F,0.F};
            require(anchor.apply(views));close(views[0].pose.position.y,0);
            const auto moved=[&](float x,float y,float z) {
                std::array<XrView,2> out{};
                out[0].pose.position={x-.032F,y,z};out[1].pose.position={x+.032F,y,z};return out;
            };
            anchor.reset(true,true);
            auto crouched=moved(.5F,1.2F,-1.F);require(anchor.apply(crouched));
            close(crouched[0].pose.position.x,-.032F);close(crouched[0].pose.position.z,0);
            close(crouched[0].pose.position.y,-.5F); // Horizontal re-centred, height kept.
            anchor.reset(true,true);anchor.reset(true,true); // Repeated requests before apply.
            auto again=moved(-.25F,1.2F,.5F);require(anchor.apply(again));
            close(again[1].pose.position.x,.032F);close(again[0].pose.position.z,0);
            close(again[0].pose.position.y,-.5F);
            anchor.reset(true,true);anchor.reset(true,false); // Height recalibration wins.
            auto calibrated=moved(0.F,1.2F,0.F);require(anchor.apply(calibrated));
            close(calibrated[0].pose.position.y,0);
            PositionAnchor first;first.reset(true,true); // Nothing to keep yet: live height.
            auto fresh=moved(1.F,1.3F,1.F);require(first.apply(fresh));close(fresh[0].pose.position.y,0);
        }
        XrView eye{XR_TYPE_VIEW};eye.pose.orientation.w=1;
        eye.fov={-0.7f,0.9f,0.8f,-0.6f};
        auto camera=eye_camera(eye,100,1,1000);require(camera.has_value());
        close(project(camera->projection,{0,0,-1,1})[2],0);
        close(project(camera->projection,{0,0,-1000,1})[2],1);
        close(project(camera->projection,{std::tan(eye.fov.angleLeft)*10,0,-10,1})[0],-1);
        close(project(camera->projection,{std::tan(eye.fov.angleRight)*10,0,-10,1})[0],1);
        close(project(camera->projection,{0,std::tan(eye.fov.angleUp)*10,-10,1})[1],-1);
        close(project(camera->projection,{0,std::tan(eye.fov.angleDown)*10,-10,1})[1],1);
        const auto infinite=eye_camera(eye,100,1);require(infinite.has_value());
        close(project(infinite->projection,{0,0,-1,1})[2],0);
        close(project(infinite->projection,{0,0,-1e8f,1})[2],1);
        eye.pose.position={1,2,3};
        // A 90-degree yaw: local forward (-Z) points toward world -X.
        eye.pose.orientation={0,std::sqrt(0.5f),0,std::sqrt(0.5f)};
        camera=eye_camera(eye,100,1,1000);require(camera.has_value());
        const auto at_eye=transform(camera->view,{100,200,300,1});
        for(unsigned i=0;i<3;++i) close(at_eye[i],0);
        const auto ahead=transform(camera->view,{90,200,300,1});
        close(ahead[0],0);close(ahead[1],0);close(ahead[2],-10);
        // Rotation, nonuniform scale and translation do not commute with the
        // tracked eye transform: compare the composition to two independent
        // matrix/vector evaluations rather than just identity fixtures.
        Matrix4 model{0,2,0,0,-3,0,0,0,0,0,4,0,90,200,300,1};
        const auto object_camera=model_eye_camera(*camera,model);require(object_camera.has_value());
        const std::array<float,4> local{1,2,3,1};
        const auto expected=transform(camera->view,transform(model,local));
        const auto actual=transform(object_camera->view,local);
        for(unsigned i=0;i<4;++i) close(expected[i],actual[i]);
        require(object_camera->projection==camera->projection);
        model[15]=0;require(!model_eye_camera(*camera,model));model[15]=1;
        model[0]=std::numeric_limits<float>::infinity();require(!model_eye_camera(*camera,model));
        eye.pose.orientation={0,0,0,1};eye.pose.position={-0.032f,0,0};
        auto left=eye_camera(eye,1,0.1f,100);
        eye.pose.position.x=0.032f;auto right=eye_camera(eye,1,0.1f,100);
        auto lp=project(left->projection,transform(left->view,{0,0,-1,1}));
        auto rp=project(right->projection,transform(right->view,{0,0,-1,1}));
        require(lp[0]>rp[0]);close(lp[0]-rp[0],0.064f*left->projection[0]);
        require(!eye_camera(eye,0,1));require(!eye_camera(eye,1,0));
        require(!eye_camera(eye,1,1,1));require(!eye_camera(eye,1,1,0.5f));
        eye.pose.orientation={0,0,0,0};require(!eye_camera(eye,1,1));
        eye.pose.orientation.w=1;eye.fov.angleLeft=eye.fov.angleRight;
        require(!eye_camera(eye,1,1));
        eye.fov.angleLeft=std::numeric_limits<float>::quiet_NaN();require(!eye_camera(eye,1,1));
        for(unsigned side=0;side<2;++side) {
            XrView asymmetric{XR_TYPE_VIEW};asymmetric.pose.orientation.w=1;
            asymmetric.fov={side?-.7F:-1.1F,side?1.05F:.65F,.85F,-.6F};
            auto asymmetric_camera=eye_camera(asymmetric,1,.1F,100.F);require(asymmetric_camera.has_value());
            for(auto size:{std::array<uint32_t,2>{133,79},std::array<uint32_t,2>{1024,1200}}) {
                const auto rays=shadow_camera(*asymmetric_camera,size[0],size[1]);require(rays.has_value());
                for(unsigned y=0;y<size[1];y+=7) for(unsigned x=0;x<size[0];x+=11) {
                    const float rx=float((x+.5-rays->center_x)/rays->focal_length);
                    const float ry=float((y+.5-rays->center_y)/rays->vertical_focal_length());
                    const auto clip=project(asymmetric_camera->projection,{rx,-ry,-1,1});
                    require(std::abs((clip[0]+1)*size[0]*.5-(x+.5))<.001);
                    require(std::abs((clip[1]+1)*size[1]*.5-(y+.5))<.001);
                }
                require(!shadow_camera(*asymmetric_camera,0,size[1]));
            }
            asymmetric_camera->projection[15]=1;require(!shadow_camera(*asymmetric_camera,100,100));
        }
        for(unsigned pose=0;pose<12;++pose) {
            const float angle=(float(pose)-6)*.08F;
            XrView moving{XR_TYPE_VIEW};moving.pose.orientation={0,std::sin(angle/2),0,std::cos(angle/2)};
            moving.pose.position={.1F*pose,.5F+.03F*pose,.02F*pose};
            moving.fov={-.9F,.7F,.8F,-.6F};
            const auto c=eye_camera(moving,1,.1F,100.F);require(c.has_value());
            const auto view=ShadowView::from_eye(*c);require(view.has_value());
            const auto ground=view->receiver({{0,0,0},{0,1,0}});
            const auto rays=shadow_camera(*c,1024,1200);require(rays.has_value());
            const Matrix4 placement{0,2,0,0,-3,0,0,0,0,0,4,0,.4F,-.2F,-3.F,1};
            for(float units:{128.F,256.F,512.F}) {
                const auto rows=shadow_model_transform(*c,placement,units);require(rows.has_value());
                for(float x:{-100.F,0.F,100.F}) {
                    const std::array<float,4> native{x,30,200,1};
                    const auto world=transform(placement,{x/units,-30/units,-200/units,1});
                    const auto expected=view->point({world[0],world[1],world[2]});
                    std::array<float,3> actual{};
                    for(unsigned r=0;r<3;++r) for(unsigned k=0;k<4;++k) actual[r]+=(*rows)[r][k]*native[k];
                    require(std::abs(actual[0]-expected.x)<.001 && std::abs(actual[1]-expected.y)<.001
                        && std::abs(actual[2]-expected.z)<.001);
                    const auto screen=project(c->projection,transform(c->view,world));
                    require(std::abs(rays->focal_length*actual[0]/actual[2]+rays->center_x-(screen[0]+1)*512)<.001);
                    require(std::abs(rays->vertical_focal_length()*actual[1]/actual[2]+rays->center_y-(screen[1]+1)*600)<.001);
                }
            }
            require(!shadow_model_transform(*c,placement,0));
            require(!shadow_model_transform(*c,placement,256,0));
            auto invalid_placement=placement;invalid_placement[15]=0;
            require(!shadow_model_transform(*c,invalid_placement,256));
            for(double x:{-1.,0.,1.}) {
                const auto p=view->point({x,0,-5});
                require(std::abs(starfox::render::shadows::dot(p-ground.point,ground.normal))<1e-4);
                const auto screen=project(c->projection,transform(c->view,{float(x),0,-5,1}));
                require(std::abs(rays->focal_length*p.x/p.z+rays->center_x-(screen[0]+1)*512)<.001);
                require(std::abs(rays->vertical_focal_length()*p.y/p.z+rays->center_y-(screen[1]+1)*600)<.001);
            }
            const auto light=view->direction({-1,1,1});
            require(std::abs(starfox::render::shadows::dot(light,ground.normal)-1)<1e-5);
            auto invalid=*c;invalid.view[0]*=2;require(!ShadowView::from_eye(invalid));
            require(!ShadowView::from_eye(*c,0));
        }
        std::cout<<"Per-eye shadow projection and moving ground/light transform tests passed (not headset validation)\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
