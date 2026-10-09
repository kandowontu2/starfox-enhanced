#include "starfox/platform/nintendo_3ds/game_dots.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include <iostream>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
void require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(why);}
void close(double a,double b,const char* why,double tolerance=.005) {
    ++checks;if(std::abs(a-b)>tolerance) throw std::runtime_error(std::string(why)+": "+std::to_string(a)+" versus "+std::to_string(b)
        +" (tolerance "+std::to_string(tolerance)+")");
}
std::int16_t word(std::int64_t x) {auto u=x%65536;if(u<0) u+=65536;return std::int16_t(u>=32768?u-65536:u);}
std::int16_t mul(int a,int b) {return word(std::int64_t(std::floor(double(a)*b/32768)));}
constexpr simulation::MatrixQ15 identity{32767,0,0,0,32767,0,0,0,32767};
struct Fixture {
    std::shared_ptr<vr::GameSceneSnapshot> scene=std::make_shared<vr::GameSceneSnapshot>();
    std::shared_ptr<simulation::SnesPpuState> ppu=std::make_shared<simulation::SnesPpuState>();
    std::shared_ptr<GameRasterSnapshot> raster=std::make_shared<GameRasterSnapshot>();
    Canvas dashboard;
    GamePresentation source;
    std::array<std::uint8_t,64> colours{};
    Fixture() {
        scene->flow=simulation::GameFlowState::gameplay;scene->view_matrix=identity;
        scene->source_vanishing_point={112,96};scene->camera.y=-512;
        for(unsigned i=0;i<16;++i) scene->model_palette[i]=std::uint16_t(i|((31-i)<<5)|((i*2%32)<<10));
        for(unsigned i=0;i<64;++i) colours[i]=std::uint8_t((i/16+i%16)%16);
        raster->ppu=ppu;raster->brightness=15;
        source.current=source.previous=scene;source.raster=raster;source.interpolation_alpha=1;
        source.plan=plan_frame(1,true,ScreenUse::world);source.dashboard=dashboard.view();
    }
};
struct Lattice {std::array<std::int16_t,3> origin,x,z;};
Lattice lattice(const timing::TransformSnapshot& camera,const simulation::MatrixQ15& matrix) {
    const std::array<std::int16_t,3> initial{word(255-(std::uint16_t(camera.x)&255)-1920),word(-camera.y),
        word(255-(std::uint16_t(camera.z)&255)-1920)};
    Lattice result;
    for(unsigned axis=0;axis<3;++axis) {
        result.origin[axis]=word(mul(initial[0],matrix[axis])+mul(initial[1],matrix[3+axis])+mul(initial[2],matrix[6+axis]));
        result.x[axis]=word(std::int64_t(std::floor(double(matrix[axis])/128)));
        result.z[axis]=word(std::int64_t(std::floor(double(matrix[6+axis])/128)));
    }
    return result;
}
struct Dot {int x,y,z;};
std::vector<Dot> points(const Lattice& field) {
    std::vector<Dot> result;auto row=field.origin;
    for(unsigned z=0;z<15;++z) {
        auto p=row;
        for(unsigned x=0;x<15;++x) {
            if(p[2]>256) result.push_back({p[0],p[1],p[2]});
            for(unsigned axis=0;axis<3;++axis) p[axis]=word(p[axis]+field.x[axis]);
        }
        for(unsigned axis=0;axis<3;++axis) row[axis]=word(row[axis]+field.z[axis]);
    }
    return result;
}
// Independent cartridge MSHOWGRID2 oracle: integer reciprocal, visible-only
// sequence, asymmetric leftward walk and its carried M_PREVX/Y endpoint.
std::pair<std::vector<std::uint8_t>,unsigned> ink(const Lattice& field,std::array<std::int16_t,2> carry) {
    std::vector<std::uint8_t> result(224*192);unsigned visible=0;
    const auto plot=[&](int x,int y) {if(x>=0 && x<224 && y>=0 && y<192) result[y*224+x]=1;};
    for(auto p:points(field)) {
        const int depth=std::min(p.z,12287),reciprocal=32767*256/(depth&~1);
        const int x=word(mul(p.x,reciprocal)+112),y=word(mul(p.y,reciprocal)+96);
        if(x<0 || x>=224 || y<0 || y>=192) continue;
        ++visible;const int adjusted=x-1;plot(adjusted,y+2);
        const int dx=adjusted-carry[0],adx=std::abs(dx),ady=std::abs(y-carry[1]);
        const int step=y<carry[1]?1:-1;int error=adx,xx=adjusted,yy=y,remaining=dx;
        do {plot(xx-2,yy);--xx;error-=ady;if(error<0) {yy+=step;error+=adx;}--remaining;} while(remaining>=0);
        carry={std::int16_t(adjusted),std::int16_t(y)};
        if(p.z<512) plot(adjusted-1,y+1);
    }
    return {std::move(result),visible};
}
std::array<double,2> screen(const FramePlan& plan,unsigned eye,Point3 p) {
    auto clip=PicaProjection(plan,eye).clip_position(p);require(bool(clip),"Nonfinite eye geometry");
    return {200*(1-(*clip)[1]/(*clip)[3]),120*(1-(*clip)[0]/(*clip)[3])};
}
void dust() {
    Fixture f;f.scene->dots_mode=-1;f.scene->camera={};f.scene->dust_point_count=5;
    f.scene->dust_points[0]={10,20,255};f.scene->dust_points[1]={10,20,257};
    f.scene->dust_points[2]={-45,-20,1100};f.scene->dust_points[3]={120,75,5000};f.scene->dust_points[4]={0,0,-600};
    GameDots owner(f.colours);auto frame=owner.prepare(f.source);validate_pica_frame(frame,f.source.dashboard);
    require(owner.coverage().dust==3 && frame.vertices.size()==24 && frame.draws.size()==1,"Dust clipping/near ink count changed");
    require(frame.textures.empty() && frame.draws[0].source_layer==1 && frame.draws[0].depth_test && frame.draws[0].depth_write,
        "Dust must be native BG1 world geometry with real depth");
    unsigned first=0;
    for(unsigned i:{1U,2U,3U}) {
        auto p=f.scene->dust_points[i];const double cz=p.z*32767./32768,depth=std::min(cz,4095.);
        const auto v=frame.vertices[first];close(v.position[0],p.x*32767./32768,"Dust X matrix");
        close(v.position[1],-p.y*32767./32768,"Dust Y inversion");close(v.position[2],depth,"Dust reciprocal depth clamp");
        const auto shade=f.colours[((5-i)&3)*16+(unsigned(depth)>>8)];const auto word_colour=f.scene->model_palette[shade];
        const auto palette=render::decode_bgr555_palette(f.scene->model_palette);
        close(v.colour[0],palette[shade].r/255.,"Dust STAR_COLS remaining-count phase");
        require(word_colour!=0,"Fixture star palette must be nontrivial");
        const auto left=screen(frame.plan,0,v.position),right=screen(frame.plan,1,v.position);
        close(right[0]-left[0],frame.plan.focal_x*(frame.plan.eyes[1].x-frame.plan.eyes[0].x)*(1/1024.-1/depth),
            "Dust uses finite off-axis stereo depth");
        close(frame.vertices[first+1].position[0]-v.position[0],depth/256,"Dust angular pixel width");
        if(cz<1024) {
            close(frame.vertices[first+6].position[0]-v.position[0],-depth/256,"Near second dot X");
            close(frame.vertices[first+6].position[1]-v.position[1],-depth/256,"Near second dot Y");
            first+=12;
        } else first+=6;
    }
    f.scene->dust_point_count=1;f.scene->dust_points[0]={-32760,0,1100};f.scene->camera.x=32760;
    frame=owner.prepare(f.source);close(frame.vertices[0].position[0],16*32767./32768,"Signed-word dust wrap");
    auto previous=std::make_shared<vr::GameSceneSnapshot>(*f.scene);
    previous->camera.x=0;previous->dust_points[0]={30000,0,1100};f.scene->camera.x=100;f.scene->dust_points[0]={200,0,1100};
    f.source.previous=previous;f.source.interpolation_alpha=.25;
    frame=owner.prepare(f.source);close(frame.vertices[0].position[0],175*32767./32768,"Camera interpolation must use CURRENT recycled identity");
    f.scene->paused=true;frame=owner.prepare(f.source);
    close(frame.vertices[0].position[0],100*32767./32768,"Pause must not jitter between cameras");f.scene->paused=false;
    ++f.scene->scene_epoch;frame=owner.prepare(f.source);
    close(frame.vertices[0].position[0],100*32767./32768,"Scene cut must not blend old stars");
    f.source.previous=f.scene;f.scene->flow=simulation::GameFlowState::controls_choice;f.scene->source_vanishing_point={90,64};
    frame=owner.prepare(f.source);const auto depth=frame.vertices[0].position[2];
    close(frame.vertices[0].position[0],100*32767./32768-22*depth/256,"Controls dust viewport X");
    close(frame.vertices[0].position[1],32*depth/256,"Controls dust viewport Y");
    const auto controls_vertices=std::vector<PicaVertex>(frame.vertices.begin(),frame.vertices.end());
    for(auto flow:{simulation::GameFlowState::controls_type,simulation::GameFlowState::controls_choice}) {
        f.scene->flow=flow;
        for(float slider:{0.F,.5F,1.F}) {
            f.source.plan=plan_frame(slider,true,ScreenUse::world);frame=owner.prepare(f.source);
            require(frame.draws[0].clip==PicaClip{96,32,208,120},"Controls dust escaped its 112x88 source flight panel");
            require(std::equal(controls_vertices.begin(),controls_vertices.end(),frame.vertices.begin()),
                "Controls dust clip/eye change rebuilt or pre-cropped camera geometry");
            validate_pica_frame(frame,f.source.dashboard);
            PicaComposite composite;const auto combined=composite.prepare(f.source.plan,std::array{frame,frame},f.source.dashboard);
            require(combined.draws.size()==2 && combined.draws[0].clip==frame.draws[0].clip
                && combined.draws[1].clip==frame.draws[0].clip,"Composition stripped the Controls dust window");
        }
    }
    f.scene->flow=simulation::GameFlowState::gameplay;frame=owner.prepare(f.source);
    require(!frame.draws[0].clip,"Controls dust clip leaked into gameplay");
    f.raster->brightness=0;frame=owner.prepare(f.source);
    for(auto v:frame.vertices) require(v.colour==std::array<float,4>{0,0,0,1},"Live raster fade lost opaque star ink");
    for(auto flow:{simulation::GameFlowState::planet_select,simulation::GameFlowState::planet_travel,simulation::GameFlowState::continue_choice}) {
        f.scene->flow=flow;frame=owner.prepare(f.source);require(frame.vertices.empty() && frame.draws.empty(),"Map/Continue must suppress source dust");
    }
}
void ground() {
    const std::array<simulation::MatrixQ15,4> rotations{identity,
        simulation::MatrixQ15{30000,12000,0,-12000,30000,0,0,0,32767},
        simulation::MatrixQ15{32767,0,0,0,30000,12000,0,-12000,30000},
        simulation::MatrixQ15{23000,0,-23000,0,32767,0,23000,0,23000}};
    Fixture f;GameDots owner(f.colours);f.scene->dots_mode=1;
    for(const auto& matrix:rotations) for(int phase:{0,255,-32760,32760}) {
        f.scene->view_matrix=matrix;f.scene->camera.x=phase;f.scene->camera.z=-phase;
        const auto expected=points(lattice(f.scene->camera,matrix));auto frame=owner.prepare(f.source);
        validate_pica_frame(frame,f.source.dashboard);require(owner.coverage().grid==expected.size(),"Q15 wrapped 15x15 lattice count");
        unsigned first=0;
        for(auto p:expected) {
            close(frame.vertices[first].position[0],p.x,"Ground dot X source lattice");
            close(frame.vertices[first].position[1],-p.y,"Ground dot Y source lattice");
            close(frame.vertices[first].position[2],std::min(p.z,12287),"Ground dot Z source lattice");
            first+=p.z<512?12:6;
        }
        require(first==frame.vertices.size() && frame.textures.empty(),"Ground uses individual depth-tested point ink");
    }
    f.scene->dots_mode=0;require(owner.prepare(f.source).vertices.empty(),"Disabled dots retained old geometry");
}
void connected() {
    Fixture f;GameDots owner(f.colours);f.scene->dots_mode=1;f.scene->grid_lines=true;f.scene->grid_line_start={87,142};
    unsigned expected_updates=0;
    for(const auto matrix:{identity,simulation::MatrixQ15{30000,12000,0,-12000,30000,0,0,0,32767},
            simulation::MatrixQ15{32767,0,0,0,30000,12000,0,-12000,30000}})
        for(int y:{-512,-1200,0,256}) {
            f.scene->view_matrix=matrix;f.scene->camera.y=y;f.scene->camera.x=y/3;
            const auto field=lattice(f.scene->camera,matrix);const auto [expected,count]=ink(field,f.scene->grid_line_start);
            const auto frame=owner.prepare(f.source);validate_pica_frame(frame,f.source.dashboard);
            require(owner.coverage().connections==count && owner.coverage().ink_updates==++expected_updates,"Connected ink source count/cache key");
            const unsigned width=frame.textures[0].pitch/4,canonical_left=(width-224)/2;
            require(frame.textures.size()==(width+1023)/1024 && width>=464,
                "Connected ink must cover finite parallax with bounded borrowed strips");
            for(unsigned strip=0;strip<frame.textures.size();++strip)
                require(frame.textures[strip].width<=1024 && frame.textures[strip].pixels.data()==frame.textures[0].pixels.data()+strip*4096,
                    "Connected grid strips duplicated source pixels or exceeded native texture dimensions");
            const auto image=frame.textures[0];unsigned occupied=0;
            for(unsigned yy=0;yy<192;++yy) for(unsigned x=0;x<224;++x) {
                const auto at=(std::size_t(yy+24)*width+x+canonical_left)*4;
                require(image.pixels[at+3]==(expected[yy*224+x]?255:0),"Connected canonical source ink was altered by native guard expansion");
                occupied+=expected[yy*224+x];
            }
            if(y==-512 && matrix==identity) require(occupied>0,"Ground oracle failed to exercise visible source lines");
            const std::array<double,3> n{double(field.x[1])*field.z[2]-double(field.x[2])*field.z[1],
                double(field.x[2])*field.z[0]-double(field.x[0])*field.z[2],double(field.x[0])*field.z[1]-double(field.x[1])*field.z[0]};
            const double d=n[0]*field.origin[0]+n[1]*field.origin[1]+n[2]*field.origin[2];
            unsigned finite=0;
            for(auto draw:frame.draws) {
                require(draw.source_layer==1 && draw.texture<frame.textures.size() && draw.alpha_blend,"Grid ink source provenance/opacity policy");
                if(draw.space==PicaSpace::scenery) {require(!draw.depth_test && !draw.depth_write,"Far carry must be scenery, not HUD/finite depth");continue;}
                ++finite;require(draw.projected_uv && draw.depth_test && draw.depth_write,"Connected grid needs finite depth and homogeneous source UV");
                for(unsigned i=draw.first;i<draw.first+draw.count;++i) {
                    auto v=frame.vertices[i];const auto p=v.position;
                    // Bound float vertex rounding by the absolute terms, not
                    // their cancellation near a tilted far-plane horizon.
                    const double roundoff=2*std::numeric_limits<float>::epsilon()
                        *(std::abs(n[0]*p[0])+std::abs(n[1]*p[1])+std::abs(n[2]*p[2]));
                    close(n[0]*p[0]-n[1]*p[1]+n[2]*p[2],d,"Grid receiver is not the actual Q15 source ground plane",std::max(1.,roundoff));
                    require(p[2]>=frame.plan.near_plane-.005 && p[2]<=frame.plan.far_plane+.005,"Receiver crosses near/far clip");
                    const double strip_left=(400.-width)/2+draw.texture*1024;
                    close(v.uv[0],(200+frame.plan.focal_x*p[0]/p[2]-strip_left)/frame.textures[draw.texture].width,"Ground source UV registration X",.00001);
                    close(v.uv[1],(120-frame.plan.focal_y*p[1]/p[2])/240.,"Ground source UV registration Y",.00001);
                    const auto left=screen(frame.plan,0,p),right=screen(frame.plan,1,p);
                    close(right[0]-left[0],frame.plan.focal_x*(frame.plan.eyes[1].x-frame.plan.eyes[0].x)*(1/1024.-1/p[2]),"Grid stereo disparity",.02);
                }
            }
            if(y<0) require(finite>=1,"Visible ground must not fall back to mono screen depth");
            const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());
            const std::vector<std::uint8_t> pixels(image.pixels.begin(),image.pixels.end());
            const auto saved_carry=f.scene->grid_line_start;const auto saved_points=f.scene->dust_points;
            for(float slider:{0.F,.5F,1.F}) {
                f.source.plan=plan_frame(slider,true,ScreenUse::world);const auto other=owner.prepare(f.source);
                require(other.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),other.vertices.begin()),"Slider rebuilt connected source geometry");
                require(std::equal(pixels.begin(),pixels.end(),other.textures[0].pixels.begin()) && owner.coverage().ink_updates==expected_updates,
                    "Unchanged camera/slider rerasterized source grid ink");
            }
            require(f.scene->grid_line_start==saved_carry,"Rendering advanced canonical connected-grid history");
            for(unsigned i=0;i<saved_points.size();++i) require(saved_points[i].x==f.scene->dust_points[i].x && saved_points[i].y==f.scene->dust_points[i].y
                && saved_points[i].z==f.scene->dust_points[i].z,"Rendering mutated/recycled source dust");
        }
    f.scene->camera={0,-512,0};f.scene->view_matrix=identity;f.source.plan=plan_frame(1,true,ScreenUse::world);
    owner.prepare(f.source);const auto updates=owner.coverage().ink_updates;f.raster->brightness=0;
    auto frame=owner.prepare(f.source);require(owner.coverage().ink_updates==updates+1,"60Hz brightness must update cached grid ink");
    unsigned black=0;
    for(unsigned i=0;i<frame.textures[0].pixels.size();i+=4) if(frame.textures[0].pixels[i+3]) {
        ++black;require(frame.textures[0].pixels[i]==0 && frame.textures[0].pixels[i+1]==0 && frame.textures[0].pixels[i+2]==0,
            "Black ink must be opaque, not a transparent-hole colour key");
    }
    require(black>0,"Black ink test did not cover occupied pixels");
    const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());const auto work=owner.coverage().ink_updates;
    for(unsigned invalid:{0U,1U,2U,3U}) {
        auto bad=f.source;
        if(invalid==0) bad.interpolation_alpha=std::numeric_limits<double>::quiet_NaN();
        if(invalid==1) bad.current.reset();
        if(invalid==2) {bad.plan.eyes[0].projection_offset=2000;}
        if(invalid==3) {auto malformed=std::make_shared<vr::GameSceneSnapshot>(*f.scene);malformed->dust_point_count=512;bad.current=malformed;}
        bool rejected=false;try {owner.prepare(bad);} catch(const std::exception&) {rejected=true;}
        require(rejected && owner.coverage().ink_updates==work && std::equal(saved.begin(),saved.end(),frame.vertices.begin()),
            "Invalid grid input damaged last complete borrowed frame");
    }
    PicaComposite composite;const auto composed=composite.prepare(f.source.plan,std::array{frame,frame},f.source.dashboard);
    require(composed.vertices.size()==frame.vertices.size()*2 && composed.textures.size()==2
        && composed.draws.back().texture==1,"Native composition lost dust/grid draw and texture order");
    f.scene->dots_mode=0;frame=owner.prepare(f.source);
    require(frame.draws.empty() && frame.vertices.empty() && frame.textures.empty(),"Scene change retained old connected grid");
    f.scene->dots_mode=1;owner.prepare(f.source);
    require(owner.coverage().ink_updates==work+1,"Leaving connected mode must release and rebuild its cache");
}
void connected_optics() {
    for(float convergence:{16.F,32.F,1024.F}) for(const auto matrix:{identity,
        simulation::MatrixQ15{30000,12000,0,-12000,30000,0,0,0,32767}}) {
        Fixture f;GameDots owner(f.colours);f.scene->dots_mode=1;f.scene->grid_lines=true;
        f.scene->camera={0,-512,0};f.scene->view_matrix=matrix;f.scene->grid_line_start={87,142};
        StereoSettings settings;settings.strength=2;settings.separation=64;settings.convergence=convergence;
        f.source.plan=plan_frame(1,true,ScreenUse::world,settings);
        const auto frame=owner.prepare(f.source);validate_pica_frame(frame,f.source.dashboard);
        const auto width=frame.textures[0].pitch/4,canonical_left=(width-224)/2;
        const auto [expected,count]=ink(lattice(f.scene->camera,matrix),f.scene->grid_line_start);
        require(frame.textures.size()==(width+1023)/1024 && owner.coverage().connections==count,
            "High separation grid lost its native strip/count contract");
        for(unsigned y=0;y<192;++y) for(unsigned x=0;x<224;++x)
            require(frame.textures[0].pixels[(std::size_t(y+24)*width+canonical_left+x)*4+3]==(expected[y*224+x]?255:0),
                "High separation altered the exact carried source grid ink");
        bool finite=false;
        for(const auto& draw:frame.draws) {
            if(draw.space==PicaSpace::world) finite=true;
            else require(!finite,"High separation grid sky overwrote an earlier finite strip");
            require(draw.texture<frame.textures.size(),"Grid strip references an omitted texture");
        }
        const auto* pixels=frame.textures[0].pixels.data();const auto updates=owner.coverage().ink_updates;
        for(float slider:{.5F,0.F,1.F}) {
            f.source.plan=plan_frame(slider,true,ScreenUse::world,settings);
            const auto cached=owner.prepare(f.source);validate_pica_frame(cached,f.source.dashboard);
            require(cached.textures[0].pixels.data()==pixels && owner.coverage().ink_updates==updates,
                "Slider-only changes decoded/copied already sufficient wide source grid artwork");
        }
    }
}
}
int main() try {
    dust();ground();connected();connected_optics();std::cout<<checks<<" native dust/grid checks passed (host geometry, not device pixels)\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
