#include "starfox/render/motion_blur.hpp"
#include "starfox/render/scene_motion_history.hpp"
#include "starfox/render/scene_enhancements.hpp"
#include "starfox/render/scene_motion_blur.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace starfox::render;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main() {
    {
        const auto off=motion_blur_preset(0),low=motion_blur_preset(1),medium=motion_blur_preset(2),high=motion_blur_preset(3,10);
        require(off.exposure_seconds==0 && off.maximum_radius==0 && off.samples==1,"Off shutter is not identity");
        require(low.exposure_seconds<medium.exposure_seconds && medium.exposure_seconds<high.exposure_seconds,"shutter levels not ordered");
        require(medium.exposure_seconds==1./120 && medium.samples==9 && medium.maximum_radius==32,"Medium changed validated exposure");
        require(high.samples==13 && high.maximum_radius==128,"high-scale shutter exceeded bounded radius");
        require(motion_blur_preset(99,99).maximum_radius==high.maximum_radius,"invalid shutter preset not bounded");
    }
    {
        Framebuffer frame(9,3);frame.enable_layer_tags(true);
        std::vector<uint8_t> source(9*3*4,0),moving,stationary,visible;
        for(unsigned i=0;i<27;++i) source[i*4+3]=255;
        SceneFxFrame fx;fx.add({4,1,1,1},{6,100,0,0},{1,1,1,0});
        MotionBlurSettings s;s.interval_seconds=s.exposure_seconds=1./60;
        std::vector<MotionBlurGuide> guides(27,{0,0,0,false,true});
        guides[13]={4,0,20,true,true};
        require(render_scene_shutter(fx,s,frame,source,moving,nullptr,0,0,guides),"moving particle occlusion render");
        guides[13].motion_x=0;
        require(render_scene_shutter(fx,s,frame,source,stationary,nullptr,0,0,guides),"stationary particle occlusion render");
        require(render_scene_shutter(fx,s,frame,source,visible),"uncovered particle render");
        require(stationary[13*4]==0&&moving[13*4]>0&&moving[13*4]<visible[13*4],
            "moving occluder did not reveal particle for only part of exposure");
        require(moving[13*4+3]==255,"moving occlusion changed alpha");
        std::vector<uint8_t> background=source,joint,model_only;
        for(unsigned i=0;i<27;++i) background[i*4+2]=100;
        source=background;source[13*4]=255;source[13*4+2]=0;
        guides[13].motion_x=4;
        SceneFxFrame empty;
        require(render_scene_shutter(empty,s,frame,source,model_only,nullptr,0,0,guides,background),"joint model reference");
        std::vector<uint8_t> existing_model;
        require(reconstruct_motion_blur(9,3,source,background,guides,s,existing_model),"existing model reference");
        for(unsigned i=0;i<model_only.size();++i)
            require(std::abs(int(model_only[i])-int(existing_model[i]))<=1,
                "joint reference changed model-only exposure");
        require(render_scene_shutter(fx,s,frame,source,joint,nullptr,0,0,guides,background),"joint particle reference");
        require(model_only[12*4]>0&&model_only[13*4]<255&&model_only[13*4+2]>0,
            "joint reference did not reconstruct foreground colour and revealed background");
        require(joint[13*4+1]>model_only[13*4+1],"joint exposure lost partially revealed particle");
        SurfaceBuffer final_depth(9,3);final_depth.set(4,1,{0,0,-1,20,0,true},0);
        std::vector<uint8_t> sequential;
        require(render_scene_shutter(fx,s,frame,model_only,sequential,&final_depth),"sequential exposure control");
        require(joint[13*4+1]>sequential[13*4+1],
            "joint fixture cannot distinguish sample occlusion from final-frame occlusion");
        source[0]=37;source[1]=211;source[2]=93;
        require(render_scene_shutter(fx,s,frame,source,joint,nullptr,0,0,guides,background)
            &&std::equal(joint.begin(),joint.begin()+4,source.begin()),"joint exposure erased depthless world sprite");
        guides[13].eligible=false;
        require(render_scene_shutter(fx,s,frame,source,joint,nullptr,0,0,guides,background)
            &&std::equal(joint.begin()+52,joint.begin()+56,source.begin()+52),"joint exposure ignored protected guide without HUD tag");
        frame.set_stored(4,1,0,PixelLayer::two_d);guides[13].eligible=false;
        require(render_scene_shutter(fx,s,frame,source,joint,nullptr,0,0,guides,background)
            &&std::equal(joint.begin()+52,joint.begin()+56,source.begin()+52),"joint exposure changed HUD");
    }
    {
        std::vector<MotionBlurGuide> guides(9,{0,0,0,false,true});
        guides[4]={3,0,20,true,true};
        MotionBlurSettings s;s.interval_seconds=s.exposure_seconds=1./60;
        std::vector<SceneShutterOcclusion> samples;
        require(scene_shutter_occlusion(9,1,guides,s,.5,samples)
            &&samples[5].depth==20&&samples[5].coverage==.5f&&samples[6].coverage==.5f
            &&samples[4].coverage==0,"moving occluder stayed at original position or lost fractional coverage");
        guides[5]={0,0,10,false,true};
        std::vector<uint8_t> colours(9*4,0);
        colours[4*4]=255;colours[4*4+3]=255;
        colours[5*4+2]=255;colours[5*4+3]=255;
        require(scene_shutter_occlusion(9,1,guides,s,.5,samples,colours)
            &&samples[5].linear_colour[2]==1&&samples[5].linear_colour[0]==0
            &&samples[6].linear_colour[0]==1&&samples[6].coverage==.5f,
            "foreground sample colour lost depth priority or was premultiplied twice");
        colours[4*4+3]=0;
        require(scene_shutter_occlusion(9,1,guides,s,.5,samples,colours)
            &&samples[6].coverage==0,"transparent foreground occluded particles");
        require(scene_shutter_occlusion(9,1,guides,s,.5,samples)
            &&samples[5].depth==10&&samples[5].coverage==1,"near stationary occluder lost depth priority");
        guides[6].eligible=false;
        require(scene_shutter_occlusion(9,1,guides,s,.5,samples)&&samples[6].coverage==0,"occluder entered HUD");
        s.paused=true;
        require(scene_shutter_occlusion(9,1,guides,s,.5,samples)&&samples[4].coverage==1,"paused occluder moved");
        s.maximum_radius=-1;
        require(!scene_shutter_occlusion(9,1,guides,s,.5,samples)&&samples[4].coverage==1,"invalid occluder settings mutated output");
    }
    {
        SceneFxFrame mixed,lights,particles;mixed.camera={16,8,256,0};
        for(unsigned n=0;n<scene_fx_capacity;++n) {
            mixed.add({float(n),8,2,1},{n%3==0?1.f:6.f,100,0,0},{1,.5f,.1f,0},{6,n+1,0,0});
            mixed.motion_previous[n]={{float(n+2),8,120},true};
        }
        require(split_scene_exposure(mixed,lights,particles)&&lights.camera[3]==16&&particles.camera[3]==32,
            "exposure partition lost emitters");
        unsigned l=0,p=0;
        for(unsigned n=0;n<scene_fx_capacity;++n) {
            auto& out=n%3==0?lights:particles;const auto at=n%3==0?l++:p++;
            require(out.motion_points[at].identity==mixed.motion_points[n].identity
                &&out.motion_previous[at].previous==mixed.motion_previous[n].previous
                &&out.motion_previous[at].valid&&out.data[at*3]==mixed.data[n*3],"partition lost history or ordering");
        }
        auto saved=particles.data;mixed.data[1][0]=8;
        require(!split_scene_exposure(mixed,lights,particles)&&particles.data==saved,"unsupported split mutated output");
    }
    {
        SceneFxFrame particles;particles.camera={16,8,256,0};
        particles.add({16,8,2,1},{6,100,0,0},{1,.5f,.1f,0},{6,1,0,0});
        particles.motion_previous[0]={{24,8,200},true};
        MotionBlurSettings shutter;shutter.interval_seconds=shutter.exposure_seconds=1./60;shutter.samples=9;
        SceneFxFrame sample;
        require(scene_shutter_sample(particles,shutter,.5,sample)
            && sample.data[0][0]==20 && sample.data[1][1]==150
            && std::abs(sample.data[0][2]-4.f/3)<.00001,"particle shutter lost travel/depth/radius");
        Framebuffer frame(32,16);frame.enable_layer_tags(true);
        for(unsigned x=0;x<32;++x) frame.set_stored(x,8,0,PixelLayer::two_d);
        std::vector<uint8_t> source(32*16*4,0),blurred,ordinary,scratch;
        for(unsigned i=0;i<32*16;++i) source[i*4+3]=255;
        ordinary=source;apply_scene_enhancements(particles,frame,ordinary,scratch,nullptr,0,0);
        require(render_scene_shutter(particles,shutter,frame,source,blurred) && blurred!=ordinary,"moving particles did not change exposure");
        require(std::equal(blurred.begin()+8*32*4,blurred.begin()+9*32*4,source.begin()+8*32*4),"particle exposure changed HUD");
        shutter.paused=true;
        require(render_scene_shutter(particles,shutter,frame,source,blurred) && blurred==ordinary,"paused particle shutter changed ordinary appearance");
        shutter.paused=false;particles.motion_previous[0].valid=false;
        require(render_scene_shutter(particles,shutter,frame,source,blurred) && blurred==ordinary,"new particle borrowed shutter movement");
        const auto saved=blurred;shutter.samples=66;
        require(!render_scene_shutter(particles,shutter,frame,source,blurred) && blurred==saved,"invalid particle shutter changed output");
        shutter.samples=9;particles.motion_previous[0].valid=true;
        SurfaceBuffer occluder(32,16);
        for(unsigned y=0;y<16;++y) for(unsigned x=0;x<32;++x) occluder.set(x,y,{0,0,-1,20,0,true},0);
        require(render_scene_shutter(particles,shutter,frame,source,blurred,&occluder)
            && blurred==source,"particle shutter leaked through a foreground surface");
        for(unsigned y=0;y<16;++y) for(unsigned x=0;x<32;++x) occluder.set(x,y,{0,0,-1,75,0,true},0);
        require(render_scene_shutter(particles,shutter,frame,source,blurred,&occluder)
            && blurred!=source,"depth-changing particle was hidden for the whole shutter");
        std::vector<uint8_t> unobstructed;
        require(render_scene_shutter(particles,shutter,frame,source,unobstructed)
            && blurred!=unobstructed,"foreground occlusion was not sampled across exposure");
        particles.motion_previous[0].previous={100000,8,100};shutter.maximum_radius=4;
        require(scene_shutter_sample(particles,shutter,.5,sample)
            && sample.data[0][0]<=20.0001f,"particle shutter travel exceeded radius cap");
    }
    {
        SceneFxTracker tracker;
        std::array<SceneFxEmitter,1> emitters{{{42,{0,0,500},true,false,false}}};
        const auto project=[](const auto& p){return p;};
        const auto points=[](const SceneFxFrame& frame) {
            return std::span(frame.motion_points).first(unsigned(frame.camera[3]));
        };
        auto first=tracker.update(0,0,1,emitters,{0,0,0},0,project,200,112,256,1,3);
        auto next=tracker.update(0,.1,1,emitters,{0,0,0},0,project,200,112,256,1,3);
        require(first.camera[3]==12 && next.camera[3]==12,"particle identity fixture did not emit sparks/debris");
        SceneMotionHistory history;history.commit(points(first),{1,1,400,224},true);
        auto matched=history.prepare(points(next),{2,1,400,224});unsigned moving=0;
        for(unsigned i=0;i<matched.size();++i) {
            require(matched[i].valid,"live spark/debris lost birth identity");
            moving+=matched[i].previous!=next.motion_points[i].projected;
        }
        require(moving>0,"live particle fixture has no motion");
        auto reset=tracker.update(0,.2,2,emitters,{0,0,0},0,project,200,112,256,1,3);
        require(reset.motion_points[0].identity!=first.motion_points[0].identity,"reset recycled particle birth identity");
        SceneFxTracker exhaust;emitters[0].explosion=false;emitters[0].player=true;
        first=exhaust.update(16,0,1,emitters,{0,0,0},0,project,200,112,256,1,4);
        emitters[0].position[0]=10;
        next=exhaust.update(16,.02,1,emitters,{0,0,0},0,project,200,112,256,1,4);
        require(first.camera[3]==1 && next.camera[3]==1
            && first.motion_points[0].identity==next.motion_points[0].identity,"exhaust changed birth identity between emissions");
        next=exhaust.update(16,.04,1,emitters,{0,0,0},0,project,200,112,256,1,4);
        require(next.camera[3]==3,"exhaust/heat fixture did not emit both types");
        for(unsigned i=0;i<3;++i) for(unsigned j=0;j<i;++j)
            require(next.motion_points[i].identity!=next.motion_points[j].identity,"exhaust/heat reused one render identity");
        SceneFxTracker weather;
        const auto weather_project=[](const auto& p){return std::array<double,3>{p[0],p[1],p[2]+1000};};
        first=weather.update(192,0,1,{}, {0,0,0},1,weather_project,200,112,256,1);
        next=weather.update(192,.01,1,{}, {480,0,0},1,weather_project,200,112,256,1);
        require(first.active() && next.active(),"weather identity fixture is empty");
        history.commit(points(first),{1,1,400,224},true);
        matched=history.prepare(points(next),{2,1,400,224});unsigned retained=0,spawned=0;
        for(unsigned i=0;i<matched.size();++i) {
            if(matched[i].valid) ++retained;else ++spawned;
            for(unsigned j=0;j<i;++j) require(next.motion_points[i].identity!=next.motion_points[j].identity,"weather cells share identity");
        }
        require(retained && spawned,"weather grid movement did not retain and introduce cells");
        const auto old=next.motion_points[0];next.eye(8,1024);
        require(next.motion_points[0].identity==old.identity && next.motion_points[0].projected[0]==next.data[0][0],"eye transform detached particle metadata");
        require(next.motion_points[0].projected[0]!=old.projected[0],"eye transform left particle at central camera");
        SceneFxFrame stereo;
        stereo.camera={200,112,256,0};
        stereo.add({25,27,3,1},{6,120,.1f,0},{1,.5f,0,0},{6,42,0,0});
        stereo.motion_previous[0]={{20,30,100},true};
        stereo.eye(-8,1024);
        require(stereo.motion_previous[0].valid
            && std::abs(stereo.motion_previous[0].previous[0]-(20-256*8*(1./1024-1./100)))<.00001,
            "scene eye used current particle depth for previous position");
        require(stereo.motion_points[0].projected[0]==stereo.data[0][0],"scene eye metadata detached from rendered position");
        stereo.camera[3]=0;
        stereo.add({25,27,3,1},{6,120,.1f,0},{1,.5f,0,0},{6,43,0,0});
        require(!stereo.motion_previous[0].valid,"reused effect slot retained previous particle motion");
    }
    {
        SceneMotionHistory history;
        const SceneMotionPoint a{{6,42,0,0},{20,30,100},true};
        const SceneMotionPoint b{{3,-4,5,6},{40,50,200},true};
        std::array<SceneMotionPoint,2> first{a,b},next{b,a};
        next[0].projected={43,55,180};next[1].projected={25,27,120};
        require(!history.prepare(next,{2,1,400,224})[0].valid,"uncommitted particle history used");
        history.commit(first,{1,1,400,224},true);
        const auto motion=history.prepare(next,{2,1,400,224});
        require(motion[0].valid && motion[0].previous==b.projected && motion[1].valid
            && motion[1].previous==a.projected,"particle list reordering mismatched identity");
        require(history.prepare(next,{2,1,400,224})[1].previous==a.projected,"prepare advanced particle history");
        auto duplicate=next;duplicate[0].identity=duplicate[1].identity;
        require(!history.prepare(duplicate,{2,1,400,224})[1].valid,"duplicate current particle identity accepted");
        for(auto frame:{SceneMotionHistory::Frame{3,1,400,224},{2,2,400,224},{2,1,800,448},{2,1,400,224,true}})
            require(!history.prepare(next,frame)[0].valid,"particle cut/resize/pause retained motion");
        auto spawned=next;spawned[0].identity[2]++;
        require(!history.prepare(spawned,{2,1,400,224})[0].valid,"new weather cell borrowed old motion");
        for(double eye:{-8.,8.}) {
            auto point=next[1];auto prior=motion[1];
            require(scene_motion_eye(point,prior,eye,256,1024),"particle eye projection rejected");
            const double expected=(20+256*eye*(1./1024-1./100))-(25+256*eye*(1./1024-1./120));
            require(std::abs((prior.previous[0]-point.projected[0])-expected)<.00001,"particle depth travel has wrong stereo motion");
            auto still=a;SceneMotionSample stable{a.projected,true};
            require(scene_motion_eye(still,stable,eye,256,1024) && still.projected==stable.previous,"stationary particle got stereo velocity");
        }
        auto invalid=next[1];auto prior=motion[1];prior.previous[2]=0;
        require(!scene_motion_eye(invalid,prior,8,256,1024) && invalid.projected==next[1].projected,"bad prior depth partially changed particle");
        history.commit(next,{2,1,400,224},false);
        require(!history.prepare(next,{3,1,400,224})[0].valid,"failed particle presentation committed history");
        history.commit(first,{3,1,400,224,true},true);
        require(!history.prepare(next,{4,1,400,224})[0].valid,"particle resume reused paused history");
        history.commit(duplicate,{4,1,400,224},true);
        require(!history.prepare(next,{5,1,400,224})[1].valid,"duplicate previous particle identity accepted");
    }
    {
        std::vector<MotionBlurGuide> input{{0,0,100,true,true},{8,-2,10,true,true},{7,3,2,true,false}};
        std::vector<MotionBlurGuide> resolved;
        require(resolve_motion_blur_guides(3,1,input,{.25f,0},resolved),"jitter guide resolve failed");
        require(resolved[0].depth==10 && resolved[0].motion_x==8 && resolved[0].motion_y==-2,
            "jitter alignment averaged depth or velocity across silhouette");
        require(resolved[2].depth==2 && !resolved[2].eligible && resolved[2].motion_x==7,
            "jitter alignment changed protected HUD ownership");
        require(resolve_motion_blur_guides(3,1,input,{-1,0},resolved)
            && resolved[0].depth==100 && resolved[1].depth==100 && resolved[1].motion_x==0,
            "negative jitter or edge clamp manufactured motion");
        input[1].valid=false;
        require(resolve_motion_blur_guides(3,1,input,{.5f,0},resolved)
            && resolved[0].depth==10 && !resolved[0].valid && resolved[0].motion_x==0,
            "invalid near surface borrowed valid far velocity");
        const auto preserved=resolved;
        require(!resolve_motion_blur_guides(3,1,input,{2,0},resolved)
            && resolved[0].depth==preserved[0].depth,"invalid jitter mutated output");
        require(!resolve_motion_blur_guides(3,1,input,{0,std::numeric_limits<float>::quiet_NaN()},resolved),
            "NaN jitter accepted");
        require(resolve_motion_blur_guides(3,1,input,{0,0},input) && input[0].depth==100
            && input[1].depth==10,"in-place guide resolve failed");
        for(auto& g:input) g={0,0,30,true,true};
        require(resolve_motion_blur_guides(3,1,input,{.375f,-.25f},resolved)
            && std::all_of(resolved.begin(),resolved.end(),[](auto g){return g.valid && g.motion_x==0 && g.motion_y==0;}),
            "stationary TAA samples acquired blur velocity");
        input={{1,0,50,true,true},{2,0,40,true,true},{3,0,30,true,true},{4,0,20,true,true}};
        require(resolve_motion_blur_guides(2,2,input,{.25f,.25f},resolved)
            && resolved[0].depth==20 && resolved[0].motion_x==4,"diagonal guide sampling missed closest surface");
        input[3].depth=std::numeric_limits<float>::quiet_NaN();
        require(resolve_motion_blur_guides(2,2,input,{.25f,.25f},resolved)
            && resolved[0].depth==30,"invalid depth poisoned guide alignment");
        input[2].motion_y=std::numeric_limits<float>::infinity();
        require(resolve_motion_blur_guides(2,2,input,{.25f,.25f},resolved)
            && resolved[0].depth==30 && !resolved[0].valid && resolved[0].motion_y==0,
            "invalid vector removed occlusion depth or retained motion");
    }
    MotionBlurTimeline timeline;
    require(timeline.interval(1,1,1,false)==0,"uncommitted blur clock valid");
    timeline.commit(1,1,1,false,true);
    require(std::abs(timeline.interval(1.01,2,1,false)-.01)<1e-12,"committed blur interval wrong");
    require(timeline.interval(1.01,2,1,false)==timeline.interval(1.01,2,1,false),"query advances blur clock");
    require(timeline.interval(1.01,2,2,false)==0&&timeline.interval(1.01,3,1,false)==0,"cut or serial gap keeps blur history");
    require(timeline.interval(1,2,1,false)==0&&timeline.interval(.9,2,1,false)==0
        &&timeline.interval(1.3,2,1,false)==0,"invalid elapsed time accepted");
    timeline.commit(1.01,2,1,false,false);
    require(timeline.interval(1.02,3,1,false)==0,"failed presentation advances blur clock");
    timeline.commit(1.02,3,1,true,true);
    require(timeline.interval(1.03,4,1,false)==0,"resume retains paused motion");
    timeline.commit(1.03,4,1,false,true);
    require(timeline.interval(1.04,5,1,true)==0,"paused query has blur interval");
    timeline.reset();require(timeline.interval(1.04,5,1,false)==0,"reset retains blur interval");
    const std::array<std::uint32_t,7> packed{0x0200007f,0x00000180,0x00000203,0x00000304,0x00000405,0x00000506,0x00000607};
    const std::array<float,7> native_depth{10,10,0,20,30,40,50};
    std::array<std::array<float,4>,7> native_motion{{{2,-3,10,1},{2,3,10,1},{1,2,0,1},
        {3,4,21,1},{5,6,30,0},{7,8,40,1},{9,10,50,1}}};
    std::vector<MotionBlurGuide> decoded;
    require(motion_blur_guides(packed,native_depth,native_motion,true,decoded),"native guides rejected");
    require(decoded[0].valid&&decoded[0].motion_x==2&&decoded[0].motion_y==-3,"native motion convention changed");
    require(!decoded[1].eligible&&!decoded[1].valid,"native HUD acquired velocity");
    require(decoded[2].eligible&&!decoded[2].valid&&decoded[2].depth==0,"sky invented surface velocity");
    require(!decoded[3].valid&&decoded[3].depth==20,"stale Z retained velocity or lost occlusion");
    require(!decoded[4].valid&&decoded[5].valid&&!decoded[6].eligible,"native tag/validity decoding failed");
    require(motion_blur_guides(packed,native_depth,native_motion,false,decoded)
        &&std::none_of(decoded.begin(),decoded.end(),[](auto g){return g.valid;}),"scene cut retained native motion");
    const auto prior_size=decoded.size();
    require(!motion_blur_guides(packed,{},native_motion,true,decoded)&&decoded.size()==prior_size,"bad native lengths mutated guides");
    auto world_sprite=packed;world_sprite[1]|=0x10000000U;
    require(motion_blur_guides(world_sprite,native_depth,native_motion,true,decoded)&&decoded[1].valid&&decoded[1].eligible,
        "world sprite mistaken for HUD");
    constexpr unsigned w=9,h=3;
    std::vector<std::uint8_t> source(w*h*4,0),out;
    std::vector<MotionBlurGuide> guides(w*h,{4,0,10,true,true});
    for(unsigned i=0;i<w*h;++i) source[i*4+3]=173;
    for(unsigned y=0;y<h;++y) source[(y*w+4)*4]=255;
    MotionBlurSettings settings;settings.interval_seconds=1./60;settings.exposure_seconds=1./60;
    require(apply_motion_blur(w,h,source,guides,settings,out),"valid blur rejected");
    require(out[(w+4)*4]<255&&out[(w+3)*4]>0&&out[(w+2)*4]>0,"motion did not spread colour along vector");
    for(unsigned i=0;i<w*h;++i) require(out[i*4+3]==173,"blur changed alpha");
    const auto at60=out;
    settings.interval_seconds=1./120;for(auto& g:guides) g.motion_x=2;
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out==at60,"same velocity differs at 120fps");
    for(auto& g:guides) {g.motion_x=-2;}
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out==at60,"centred shutter depends on motion sign");
    auto inplace=source;
    require(apply_motion_blur(w,h,inplace,guides,settings,inplace)&&inplace==out,"in-place blur differs");
    guides[w+4].eligible=false;
    require(apply_motion_blur(w,h,source,guides,settings,out),"HUD test rejected");
    require(out[(w+4)*4]==255&&out[(w+3)*4]==0,"HUD was blurred or leaked into world");
    guides[w+4].eligible=true;guides[w+4].depth=1;
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out[(w+3)*4]==0,"foreground crossed depth boundary");
    for(auto& g:guides) g.valid=false;
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out==source,"cut/invalid history smeared scene");
    for(auto& g:guides) g={2,0,10,true,true};
    settings.paused=true;
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out==source,"pause changes image");
    settings.paused=false;settings.interval_seconds=1;
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out==source,"long gap smears image");
    settings.interval_seconds=1./120;settings.exposure_seconds=0;
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out==source,"zero exposure not identity");
    settings.exposure_seconds=std::numeric_limits<double>::quiet_NaN();
    require(!apply_motion_blur(w,h,source,guides,settings,out)&&out==source,"invalid settings mutate output");
    settings.exposure_seconds=1./60;
    auto vertical=source;
    for(unsigned i=0;i<w*h;++i) {vertical[i*4]=0;guides[i]={0,2,10,true,true};}
    vertical[(w+4)*4]=255;
    require(apply_motion_blur(w,h,vertical,guides,settings,out)
        &&out[4*4]>0&&out[(w+3)*4]==0,"blur direction does not follow vertical velocity");
    for(unsigned i=0;i<w*h;++i) {vertical[i*4]=80;vertical[i*4+1]=110;vertical[i*4+2]=170;}
    require(apply_motion_blur(w,h,vertical,guides,settings,out)&&out==vertical,"uniform image darkens near boundary");
    guides[w+4].motion_x=std::numeric_limits<float>::infinity();
    require(apply_motion_blur(w,h,source,guides,settings,out)&&out[(w+4)*4]==255,"invalid velocity smears pixel");
    std::vector<std::uint8_t> background(w*4,0),foreground;
    for(unsigned i=0;i<w;++i) {background[i*4+2]=255;background[i*4+3]=255;}
    foreground=background;foreground[4*4]=255;foreground[4*4+2]=0;
    std::vector<MotionBlurGuide> moving(w,{0,0,100,true,true});moving[4]={4,0,10,true,true};
    settings.interval_seconds=settings.exposure_seconds=1./60;settings.samples=9;
    require(reconstruct_motion_blur(w,1,foreground,background,moving,settings,out),"silhouette reconstruction failed");
    require(out[3*4]>0&&out[5*4]>0&&out[4*4]<255&&out[4*4+2]>0,"silhouette did not spread/reveal underlay");
    const auto silhouette60=out;
    foreground[1]=255;moving[0].depth=0;
    require(reconstruct_motion_blur(w,1,foreground,background,moving,settings,out)
        &&out[1]==255,"unrelated unknown-depth world sprite erased by moving silhouette");
    foreground[1]=0;moving[0].depth=100;
    moving[4].motion_x=2;settings.interval_seconds=1./120;
    require(reconstruct_motion_blur(w,1,foreground,background,moving,settings,out)&&out==silhouette60,"silhouette depends on FPS");
    foreground[3*4]=0;foreground[3*4+1]=255;foreground[3*4+2]=0;moving[3].depth=1;
    require(reconstruct_motion_blur(w,1,foreground,background,moving,settings,out)
        &&out[3*4]==0&&out[3*4+1]==255&&out[3*4+2]==0,"far trail covers near occluder");
    moving[5].eligible=false;
    require(reconstruct_motion_blur(w,1,foreground,background,moving,settings,out)
        &&out[5*4]==foreground[5*4]&&out[5*4+2]==foreground[5*4+2],"silhouette touches HUD");
    auto alias=foreground;
    require(reconstruct_motion_blur(w,1,alias,background,moving,settings,alias)&&alias==out,"in-place silhouette differs");
    const auto saved=out;
    require(!reconstruct_motion_blur(w,1,foreground,{},moving,settings,out)&&out==saved,"missing underlay accepted");
    for(auto& g:moving) g.valid=false;
    require(reconstruct_motion_blur(w,1,foreground,background,moving,settings,out)&&out==foreground,"cut retains silhouette trails");
    std::cout<<"Velocity motion blur reference tests passed\n";
}
