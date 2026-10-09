#include "starfox/platform/nintendo_3ds/pica_shapes.hpp"
#include "starfox/platform/nintendo_3ds/pica_source_spans.hpp"
#include "starfox/render/source_polygon_spans.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
using Screen=std::array<double,2>;
using Camera=std::array<double,3>;
unsigned checks{},frames{};
std::string context;
void require(bool value,const char* why) {
    ++checks;if(!value) throw std::runtime_error(context+": "+why);
}
template<class F> void rejects(F&& f,const char* why) {
    bool rejected=false;try {f();} catch(const std::exception&) {rejected=true;}
    require(rejected,why);
}
render::RenderPose pose() {render::RenderPose p;p.vanish_x=128;p.vanish_y=112;return p;}
constexpr double focal=256;
constexpr std::array<float,4> ink{.25F,.5F,.75F,1};
struct Surface {
    Camera normal{0,0,1};double distance{512};
    double depth(Screen p,const render::RenderPose& view) const {
        return distance/(normal[0]*(p[0]-view.vanish_x)/focal
            +normal[1]*(p[1]-view.vanish_y)/focal+normal[2]);
    }
};
std::vector<render::ShapePrimitiveVertex> face(std::span<const Screen> points,
    const Surface& surface,const render::RenderPose& view) {
    std::vector<render::ShapePrimitiveVertex> result;
    for(auto p:points) {
        const auto z=surface.depth(p,view);
        result.push_back({{(p[0]-view.vanish_x)*z/focal,(p[1]-view.vanish_y)*z/focal,z},{}});
    }
    return result;
}
struct Coverage {
    unsigned width{256},height{224};
    std::vector<double> depth=std::vector<double>(width*height,std::numeric_limits<double>::infinity());
    void put(int x,int y,double z) {
        if(x>=0 && y>=0 && x<int(width) && y<int(height))
            depth[unsigned(y)*width+unsigned(x)]=std::min(z,depth[unsigned(y)*width+unsigned(x)]);
    }
};
// Independent triangle projection and reciprocal-depth interpolation. This
// does not call the native converter's clipping/plane/rectangle functions.
Coverage raster(std::span<const PicaVertex> triangles,const render::RenderPose& view) {
    Coverage out;
    for(unsigned first=0;first<triangles.size();first+=3) {
        std::array<Screen,3> p;std::array<double,3> z;
        for(unsigned i=0;i<3;++i) {
            const auto& v=triangles[first+i];z[i]=v.position[2];
            p[i]={view.vanish_x+focal*v.position[0]/z[i],view.vanish_y-focal*v.position[1]/z[i]};
        }
        const auto edge=[](Screen a,Screen b,Screen c) {
            return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
        };
        const auto area=edge(p[0],p[1],p[2]);if(std::abs(area)<1.e-8) continue;
        const int left=std::max(0,int(std::floor(std::min({p[0][0],p[1][0],p[2][0]}))));
        const int right=std::min(255,int(std::ceil(std::max({p[0][0],p[1][0],p[2][0]}))));
        const int top=std::max(0,int(std::floor(std::min({p[0][1],p[1][1],p[2][1]}))));
        const int bottom=std::min(223,int(std::ceil(std::max({p[0][1],p[1][1],p[2][1]}))));
        for(int y=top;y<=bottom;++y) for(int x=left;x<=right;++x) {
            const Screen sample{x+.5,y+.5};
            const std::array<double,3> w{edge(p[1],p[2],sample)/area,
                edge(p[2],p[0],sample)/area,edge(p[0],p[1],sample)/area};
            if(std::min({w[0],w[1],w[2]})>=-1.e-6)
                out.put(x,y,1/(w[0]/z[0]+w[1]/z[1]+w[2]/z[2]));
        }
    }
    return out;
}
Coverage expected(std::span<const Screen> boundary,const render::RenderPose& view,const Surface& surface) {
    std::vector<render::SourceSpanPoint> rounded;
    for(auto p:boundary) rounded.push_back({int(std::lround(p[0])),int(std::lround(p[1]))});
    const render::SourceSpanModes modes{view.wireframe_mode,view.wobble_mode,view.cel_mode,
        view.wave_mode,true,view.wave_offset,view.animation_frame};
    Coverage out;
    // The shared span contract is separately frozen against 4,608 pre-change
    // source renderer frames. Here it is the ink oracle, never the native
    // geometry/depth oracle: physical depth is calculated independently.
    render::source_polygon_spans(rounded,400,modes,[&](render::SourcePolygonSpan span) {
        for(int x=span.left;x<=span.right;++x)
            out.put(x,span.y,surface.depth({x+.5,span.source_y+.5},view));
    });
    return out;
}
void compare(const Coverage& native,const Coverage& source,bool depths=true) {
    for(unsigned i=0;i<source.depth.size();++i) {
        const bool a=std::isfinite(native.depth[i]),b=std::isfinite(source.depth[i]);
        if(a!=b) throw std::runtime_error(context+": source/native ink differs at "
            +std::to_string(i%256)+","+std::to_string(i/256));
        ++checks;
        if(a && depths) require(std::abs(native.depth[i]-source.depth[i])<source.depth[i]*3.e-6,
            "EX footprint lost authored surface depth or unwarped wave Y");
    }
}
void authored_modes() {
    const std::array<std::vector<Screen>,3> boundaries{{
        {{83,62},{169,62},{169,152},{83,152}},
        {{118,46},{181,100},{157,171},{62,126}},
        {{112,62},{158,76},{181,132},{112,173},{70,131},{89,79}}}};
    const std::array<Surface,3> surfaces{{{{0,0,1},512},{{.55,-.4,1},640},{{-.7,.3,1},1024}}};
    const auto plan=plan_frame(1,true,ScreenUse::world);
    for(unsigned shape=0;shape<boundaries.size();++shape) for(unsigned wire=0;wire<3;++wire)
        for(unsigned wobble=0;wobble<4;++wobble) for(bool wave:{false,true}) for(bool cel:{false,true})
            for(unsigned phase:{0U,7U,15U,31U}) {
                auto view=pose();view.wireframe_mode=std::uint8_t(wire);view.wobble_mode=std::uint8_t(wobble);
                view.wave_mode=wave;view.cel_mode=cel;view.animation_frame=phase;view.wave_offset=-32760;
                context="EX native ink shape="+std::to_string(shape)+" wire="+std::to_string(wire)
                    +" wobble="+std::to_string(wobble)+" wave="+std::to_string(wave)+" cel="+std::to_string(cel)
                    +" phase="+std::to_string(phase);
                const auto camera=face(boundaries[shape],surfaces[shape],view);
                const auto geometry=pica_source_span_geometry(camera,view,focal,{128,112},plan,ink);
                require(geometry.size()%3==0 && geometry.size()<=pica_vertex_limit,"Native EX triangle budget invalid");
                for(const auto& vertex:geometry) require(vertex.colour==ink && vertex.uv==std::array<float,2>{},
                    "Native EX replaced source ink with a bitmap or material");
                compare(raster(geometry,view),expected(boundaries[shape],view,surfaces[shape]));++frames;
            }
}
void nonplanar_depth() {
    context="Non-planar EX surface preserves original fan depth";
    auto view=pose();view.cel_mode=true;
    const std::array<Screen,4> screen{{{80,70},{170,70},{170,160},{80,160}}};
    const std::array<double,4> z{256,384,768,512};
    std::vector<render::ShapePrimitiveVertex> camera;
    for(unsigned i=0;i<4;++i) camera.push_back({{(screen[i][0]-128)*z[i]/focal,
        (screen[i][1]-112)*z[i]/focal,z[i]}, {}});
    const auto geometry=pica_source_span_geometry(camera,view,focal,{128,112},plan_frame(1,true,ScreenUse::world),ink);
    const auto native=raster(geometry,view);
    // Original fan has diagonal (80,70)->(170,160). At strictly interior
    // samples, reciprocal depth is affine in each of its two triangles.
    unsigned above=0,below=0;
    for(int y=72;y<158;++y) for(int x=82;x<168;++x) {
        const double u=(x+.5-80)/90,v=(y+.5-70)/90;
        if(std::abs(u-v)<.025) continue; // Do not conflate quantized pixel-edge regions.
        const double inverse=u>v?(1-u)/z[0]+(u-v)/z[1]+v/z[2]
            :(1-v)/z[0]+u/z[2]+(v-u)/z[3];
        require(std::abs(native.depth[unsigned(y)*256+unsigned(x)]-1/inverse)<.003,
            "Non-planar face was flattened to constant/average depth");
        (u>v?above:below)++;
    }
    require(above>1000 && below>1000,"Non-planar fixture failed to exercise both authored planes");
}
void stereo_and_clip() {
    context="EX eye-union preparation and physical depth clipping";
    auto view=pose();view.cel_mode=true;
    auto settings=StereoSettings{};settings.separation=64;settings.convergence=512;
    const auto plan=plan_frame(1,true,ScreenUse::world,settings);
    // Beyond the cyclopean viewport; finite-depth parallax brings this face
    // into the right LCD while it remains outside the other eye.
    const std::array<Screen,4> boundary{{{330,90},{348,90},{348,135},{330,135}}};
    const Surface surface{{0,0,1},64};const auto camera=face(boundary,surface,view);
    const auto geometry=pica_source_span_geometry(camera,view,focal,{128,112},plan,ink);
    require(!geometry.empty(),"Mono frustum discarded visible right-eye EX ink");
    bool left_visible=false,right_visible=false;
    for(const auto& v:geometry) {
        const auto l=*project(plan,0,v.position[0],v.position[1],v.position[2]);
        const auto r=*project(plan,1,v.position[0],v.position[1],v.position[2]);
        left_visible|=l[0]>=0 && l[0]<400;right_visible|=r[0]>=0 && r[0]<400;
        const auto disparity=plan.focal_x*plan.separation*(1/v.position[2]-1/plan.convergence);
        require(std::abs((l[0]-r[0])-disparity)<.0001,"EX wave/edge lost depth-dependent stereo disparity");
    }
    require(!left_visible && right_visible,"Fixture failed to exercise eye-union instead of mono clipping");
    settings.near_plane=200;settings.far_plane=1000;settings.convergence=512;
    const auto clipped_plan=plan_frame(1,true,ScreenUse::world,settings);
    const std::array<Camera,4> crossing{{{-32,-24,128},{64,-24,320},{64,48,1300},{-32,48,800}}};
    std::vector<render::ShapePrimitiveVertex> crossed;
    for(auto p:crossing) crossed.push_back({p,{}});
    const auto clipped=pica_source_span_geometry(crossed,view,focal,{128,112},clipped_plan,ink);
    require(!clipped.empty(),"Partially near/far-clipped EX face discarded");
    for(const auto& v:clipped) require(v.position[2]>=200-.001 && v.position[2]<=1000+.001,
        "Source footprint divided across an unclipped physical near/far boundary");
    for(double depth:{100.,1500.}) {
        const auto hidden=face(boundary,{{0,0,1},depth},view);
        require(pica_source_span_geometry(hidden,view,focal,{128,112},clipped_plan,ink).empty(),
            "Completely near/far hidden EX face emitted geometry");
    }
}
void bounded_collapsed_rows() {
    context="Near-camera EX collapsed-row union remains bounded without losing ink";
    auto view=pose();view.wireframe_mode=1;view.wobble_mode=1;
    const std::array<Screen,4> boundary{{{-4000,20},{4000,20},{3800,200},{-3800,200}}};
    const Surface surface{{0,0,1},8};
    const auto plan=plan_frame(1,true,ScreenUse::world);
    const auto geometry=pica_source_span_geometry(face(boundary,surface,view),view,focal,{128,112},plan,ink,60);
    require(!geometry.empty() && geometry.size()<=60,"Collapsed chords were emitted once per source visit instead of unioned ink");
    // The wide polygon covers every canonical source X on its deliberately
    // collapsed rows. Independent source conversion above is frozen against
    // the old raster; verify the optimisation did not turn holes into fills.
    compare(raster(geometry,view),expected(boundary,view,surface));
    view.wave_mode=true;view.wireframe_mode=0;
    const auto waved=pica_source_span_geometry(face(boundary,surface,view),view,focal,{128,112},plan,ink);
    compare(raster(waved,view),expected(boundary,view,surface));
    require(waved.size()<4096,"Near-camera collapsed waves retained off-eye/repeated-visit geometry");
}
render::Palette256 palette() {
    render::Palette256 result;
    for(unsigned i=0;i<result.size();++i) result[i]={std::uint8_t(i),std::uint8_t(255-i),std::uint8_t(i^85),255};
    return result;
}
void materials_and_failure() {
    context="EX material/window ordering and transactional resource limits";
    const std::array<Screen,4> boundary{{{90,80},{166,80},{166,144},{90,144}}};
    auto view=pose();view.wave_mode=true;view.vanish_x=136;view.vanish_y=108;
    render::PreparedShapePrimitives source;source.pose=view;source.focal_length=focal;source.colour_index_base=112;
    render::ShapePrimitive primitive;primitive.vertices=face(boundary,{},view);primitive.material.colour={3,10,true};
    source.primitives.push_back(primitive);const auto colours=palette();const auto plan=plan_frame(1,true,ScreenUse::world);
    PicaShapes output;output.append(source,colours,{128,112},&plan);
    auto frame=output.frame(plan);
    require(frame.draws.size()==1 && frame.draws[0].screen_dither && frame.textures.size()==1
        && frame.textures[0].width==8 && frame.textures[0].height==8,"EX allocates only shared source parity, not a mono effect image");
    require(frame.draws[0].model[0][2]==8.F/256 && frame.draws[0].model[1][2]==4.F/256,
        "EX vanished source origin or applied model offset twice");
    require(frame.draws[0].dither_odd==std::array<std::uint8_t,4>{colours[122].r,colours[122].g,colours[122].b,255}
        && frame.vertices[0].colour[0]==colours[115].r/255.F,"EX lost original even/odd palette base");
    const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());
    const auto count=frame.draws[0].count;
    auto broken=source;broken.primitives.push_back(primitive);
    broken.primitives.back().vertices[0].camera[2]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{output.append(broken,colours,{128,112},&plan);},"Invalid later EX face did not reject whole transaction");
    frame=output.frame(plan);
    require(std::vector<PicaVertex>(frame.vertices.begin(),frame.vertices.end())==saved
        && frame.draws.size()==1 && frame.draws[0].count==count && frame.textures.size()==1,
        "EX failure corrupted earlier geometry/merged count/texture ownership");
    source.pose.effect_clip_left=64;source.pose.effect_clip_right=160;
    output.append(source,colours,{128,112},&plan);frame=output.frame(plan);
    require(frame.draws.size()==2 && frame.draws[1].clip==PicaClip{136,0,232,240},"EX source window lost per-eye hardware clipping");
    require(std::equal(saved.begin(),saved.end(),frame.vertices.begin()+count),"EX source window cropped mono geometry before stereo projection");
    std::vector<std::uint8_t> lower(bottom_width*screen_height*3);
    validate_pica_frame(frame,{lower,bottom_width,screen_height,bottom_width*3});
    rejects([&]{pica_source_span_geometry(primitive.vertices,view,focal,{128,112},plan,ink,6);},
        "Geometry budget overflow fell back to a filled fan or dropped ink");
    auto bad_plan=plan;bad_plan.eye_count=3;
    rejects([&]{pica_source_span_geometry(primitive.vertices,view,focal,{128,112},bad_plan,ink);},"Malformed eye count accepted");
    bad_plan=plan;bad_plan.eyes[1].projection_offset=std::numeric_limits<float>::infinity();
    rejects([&]{pica_source_span_geometry(primitive.vertices,view,focal,{128,112},bad_plan,ink);},"Inactive/invalid eye projection ignored");
    rejects([&]{pica_source_span_geometry(primitive.vertices,view,1.e15,{128,112},plan,ink);},"Unbounded scan preparation accepted");
    auto white=ink;white[0]=2;
    rejects([&]{pica_source_span_geometry(primitive.vertices,view,focal,{128,112},plan,white);},"Invalid source colour accepted");
}
} // namespace
int main() try {
    authored_modes();nonplanar_depth();stereo_and_clip();bounded_collapsed_rows();materials_and_failure();
    std::cout<<"3DS native EX spans: "<<checks<<" checks, "<<frames<<" ink/depth fixtures passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
