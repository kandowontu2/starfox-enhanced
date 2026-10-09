#include "starfox/platform/nintendo_3ds/game_scenery.hpp"
#include "starfox/platform/nintendo_3ds/raster_coverage.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include <initializer_list>

namespace starfox::platform::nintendo_3ds {
namespace {
// A nondegenerate rectangle clipped by at most five half-planes has at most
// nine corners; sixteen also covers two added closed/duplicate points per
// clip. A frustum face has at most twelve edge hits followed by seven clips,
// which fits thirty-two even with those duplicates. No corner is simplified.
template<unsigned Dimensions,unsigned Capacity>
class SceneryPolygon {
public:
    using Point=std::array<double,Dimensions>;
    SceneryPolygon()=default;
    SceneryPolygon(std::initializer_list<Point> points) {for(const auto p:points) push_back(p);}
    void clear() noexcept {count_=0;}
    bool empty() const noexcept {return count_==0;}
    unsigned size() const noexcept {return count_;}
    Point& operator[](unsigned i) noexcept {return points_[i];}
    const Point& operator[](unsigned i) const noexcept {return points_[i];}
    const Point& front() const noexcept {return points_[0];}
    const Point& back() const noexcept {return points_[count_-1];}
    const Point* begin() const noexcept {return points_.data();}
    const Point* end() const noexcept {return points_.data()+count_;}
    void push_back(Point point) {
        if(count_==Capacity) throw std::logic_error("3DS receiver clipping exceeded bounded geometry");
        points_[count_++]=point;
    }
private:
    std::array<Point,Capacity> points_{};
    unsigned count_{};
};
using SceneryPolygon2=SceneryPolygon<2,16>;
using SceneryPolygon3=SceneryPolygon<3,32>;
bool valid_corridor(const vr::SourceCorridorBounds& box) noexcept {
    return box.walls && !(box.walls&~15U) && box.left<box.right && box.top<box.bottom;
}
}
bool native_corridor_scene(const GamePresentation& source) noexcept {
    if(!source.current || !source.raster || !source.raster->ppu || source.raster->boss_roll
        || source.raster->ppu->background_mode<1
        || source.raster->ppu->background_mode>2) return false;
    const auto& scene=*source.current;
    if(!scene.background_corridor) return false;
    const auto& box=*scene.background_corridor;
    // The known open-left colony uses WATER/Mode 1, not INATUNNEL. Never
    // infer an enclosed tube from that flag or its asymmetric movement limit.
    if(!source.raster->ppu->tunnel_scene
        && !(box.walls==14 && source.raster->ppu->background_mode==1)) return false;
    // Source entry/exit cameras can lie outside or exactly on a physical face.
    // Signed bounded faces handle that case without clamping the source camera.
    if(!valid_corridor(box)) return false;
    using enum simulation::GameFlowState;
    return scene.flow==gameplay || scene.flow==training || scene.flow==intro || scene.flow==planet_travel
        || scene.flow==stage_results || scene.flow==game_over || scene.flow==finished || scene.flow==credits;
}
namespace {
struct CorridorGeometry {
    std::array<std::array<double,3>,4> normals{};
    std::array<double,4> distances{};
    bool exterior{};
};
CorridorGeometry corridor_geometry(const GamePresentation& source) {
    if(!native_corridor_scene(source) || !std::isfinite(source.interpolation_alpha))
        throw std::invalid_argument("Invalid native 3DS corridor source");
    const auto& now=*source.current;const auto box=*now.background_corridor;
    double alpha=1;
    if(source.previous && source.previous->flow==now.flow && source.previous->scene_epoch==now.scene_epoch
        && source.previous->background_id==now.background_id && source.previous->background_corridor==now.background_corridor
        && !timing::camera_transform_is_discontinuous(source.previous->camera,now.camera))
        alpha=std::clamp(source.interpolation_alpha,0.,1.);
    const auto camera=source.previous?timing::interpolate(source.previous->camera,now.camera,alpha)
        :timing::interpolate(now.camera,now.camera,1);
    const auto view=source.previous?simulation::interpolate_rotation_matrix_q15(source.previous->view_matrix,now.view_matrix,alpha)
        :now.view_matrix;
    std::array<std::array<double,6>,3> inverse{};
    for(unsigned row=0;row<3;++row) {
        for(unsigned col=0;col<3;++col) inverse[row][col]=double(view[col*3+row])/32768.;
        inverse[row][row+3]=1;
    }
    for(unsigned col=0;col<3;++col) {
        unsigned pivot=col;
        for(unsigned row=col+1;row<3;++row) if(std::abs(inverse[row][col])>std::abs(inverse[pivot][col])) pivot=row;
        if(std::abs(inverse[pivot][col])<1.e-9) throw std::invalid_argument("Singular source corridor camera matrix");
        std::swap(inverse[pivot],inverse[col]);const double divisor=inverse[col][col];
        for(auto& value:inverse[col]) value/=divisor;
        for(unsigned row=0;row<3;++row) if(row!=col) {
            const double factor=inverse[row][col];
            for(unsigned k=0;k<6;++k) inverse[row][k]-=factor*inverse[col][k];
        }
    }
    CorridorGeometry result;
    result.distances={camera.x-box.left,box.right-camera.x,camera.y-box.top,box.bottom-camera.y};
    for(unsigned wall=0;wall<4;++wall) {
        if(!(box.walls&(1U<<wall))) continue;
        const unsigned axis=wall/2;const double sign=(wall&1)?1.:-1.;
        result.normals[wall]={sign*inverse[axis][3],-sign*inverse[axis][4],sign*inverse[axis][5]};
        result.exterior|=result.distances[wall]<=0;
    }
    return result;
}
}
std::array<std::array<double,3>,4> source_corridor_planes(const GamePresentation& source) {
    const auto geometry=corridor_geometry(source);
    std::array<std::array<double,3>,4> result{};
    for(unsigned wall=0;wall<4;++wall) {
        const auto normal=geometry.normals[wall];const double d=geometry.distances[wall];
        // A face through the camera has no single reciprocal ray-depth field.
        // Its physical geometry is still generated by signed face clipping.
        if(d==0) continue;
        result[wall]={normal[0]/(d*source.plan.focal_x),-normal[1]/(d*source.plan.focal_y),
            (normal[2]-200*normal[0]/source.plan.focal_x+120*normal[1]/source.plan.focal_y)/d};
    }
    return result;
}
unsigned source_corridor_guard(const GamePresentation& source) {
    static_cast<void>(corridor_geometry(source));
    // Source tunnel margins clamp one authored cross-section's edge pixels.
    // Their finite geometry can extend analytically without allocating tens
    // of thousands of identical decoded columns at large stereo separation.
    return pica_scenery_guard(source.plan);
}
bool native_landscape_scene(const GamePresentation& source) noexcept {
    if(!source.current || !source.raster || !source.raster->ppu || source.raster->boss_roll) return false;
    const auto& s=*source.current;
    // Actual Original Fortuna retains this same landscape during its score
    // tally. Keep that world receiver, not the neighbouring screen-space OBJ.
    return (s.flow==simulation::GameFlowState::gameplay || s.flow==simulation::GameFlowState::training
        || s.flow==simulation::GameFlowState::stage_results)
        && source.raster->ppu->background_mode==2 && !source.raster->ppu->tunnel_scene
        && s.background_landscape && s.landscape_grid_height<0;
}
bool native_water_scene(const GamePresentation& source) noexcept {
    if(!source.current || !source.raster || !source.raster->ppu || source.raster->boss_roll) return false;
    const auto& scene=*source.current;const auto& ppu=*source.raster->ppu;
    using enum simulation::GameFlowState;
    return scene.background_water_surround && ppu.background_mode==1 && !ppu.tunnel_scene
        && (scene.flow==gameplay || scene.flow==training || scene.flow==intro || scene.flow==planet_travel
            || scene.flow==stage_results || scene.flow==game_over || scene.flow==finished || scene.flow==credits);
}
double source_water_height(const GamePresentation& source) {
    if(!native_water_scene(source) || !std::isfinite(source.interpolation_alpha))
        throw std::invalid_argument("Invalid native 3DS water source");
    const auto height=[](const auto& scene){return std::abs(double(scene.camera.y)-scene.shadow_height);};
    const auto& now=*source.current;double result=height(now);
    if(source.previous && source.previous->background_water_surround && source.previous->flow==now.flow
        && height(*source.previous)>0 && !timing::camera_transform_is_discontinuous(source.previous->camera,now.camera))
        result=std::lerp(height(*source.previous),result,std::clamp(source.interpolation_alpha,0.,1.));
    if(result<=0) throw std::invalid_argument("3DS water camera lies on its receiver plane");
    return result;
}
unsigned source_water_guard(const GamePresentation& source) {
    const double distance=source_water_height(source)*source.plan.focal_y;
    return std::max(pica_receiver_guard(source.plan,{0,1/distance,-120/distance}),
        pica_receiver_guard(source.plan,{0,-1/distance,120/distance}));
}
LandscapePlane source_landscape_plane(const GamePresentation& source) {
    if(!native_landscape_scene(source)) throw std::invalid_argument("Not a native 3DS landscape scene");
    const auto& scene=*source.current;const auto& ppu=*source.raster->ppu;
    const auto scroll=scene.background_scroll_override.value_or(std::array{ppu.bg2_scroll_x,ppu.bg2_scroll_y});
    double intercept=scroll[1],slope=0,count=0,sx=0,sy=0,sxx=0,sxy=0;
    int last_raw=0,unwrapped=0;bool previous=false;
    if(ppu.bg2_vertical_offsets_enabled) for(unsigned i=0;i<32;++i) {
        const auto at=(0x2fa0+i)*2;
        const unsigned word=unsigned(ppu.vram[at])|(unsigned(ppu.vram[at+1])<<8);
        if(!(word&0x4000)) continue;
        const int raw=word&0x1fff;
        if(previous) {int delta=(raw-last_raw)&0x1fff;if(delta>4095) delta-=8192;unwrapped+=delta;}
        else unwrapped=raw;
        previous=true;last_raw=raw;
        const double x=i+1,y=unwrapped;++count;sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;
    }
    if(count) {
        const double denominator=count*sxx-sx*sx;
        slope=count>1 && denominator!=0?(count*sxy-sx*sy)/denominator:0;
        intercept=(sy-slope*sx)/count;
    }
    // Exactly the same all-sample line as expanded source BG2. In particular,
    // do not fit a single edge step: the cartridge roll tables are quantized.
    const double offset=intercept+slope*(128+(int(scroll[0])&7))/8.;
    const unsigned period=((ppu.bg2_screen_size&2)?64:32)*(ppu.bg2_tile_size_16?16:8);
    double horizon=double(scene.landscape_atlas_origin)+112-offset+8;
    horizon-=std::round((horizon-120)/period)*period;
    return {horizon,-slope/8.,-double(scene.landscape_grid_height)};
}
unsigned source_landscape_guard(const GamePresentation& source) {
    const auto plane=source_landscape_plane(source);
    const double distance=plane.height*source.plan.focal_y;
    return pica_receiver_guard(source.plan,{-plane.slope/distance,1/distance,
        (200*plane.slope-plane.centre)/distance});
}
std::size_t GameScenery::working_geometry_bytes() const noexcept {
    return (vertices_.capacity()+pending_vertices_.capacity())*sizeof(PicaVertex)
        +(draws_.capacity()+pending_draws_.capacity())*sizeof(PicaDraw)
        +(images_.capacity()+pending_images_.capacity())*sizeof(PicaImage);
}
void GameScenery::ensure_vertex_space(std::size_t needed) {
    if(needed>pica_vertex_limit) throw std::invalid_argument("3DS scenery exceeds complete geometry capacity");
    if(needed>pending_vertices_.capacity())
        pending_vertices_.reserve(std::min<std::size_t>(pica_vertex_limit,
            std::max(needed,std::max<std::size_t>(8,pending_vertices_.capacity()*2))));
}
PicaFrame GameScenery::publish(const GamePresentation& source,const PicaFrame& bg2) {
    validate_pica_group({source.plan,pending_vertices_,pending_draws_,pending_images_,bg2.clear},512U*256U*4U);
    vertices_.swap(pending_vertices_);draws_.swap(pending_draws_);images_.swap(pending_images_);
    return {source.plan,vertices_,draws_,images_,bg2.clear};
}
PicaFrame GameScenery::prepare(const GamePresentation& source,const PicaFrame& bg2,unsigned decoded_guard) {
    const auto plane=source_landscape_plane(source);
    if(!same_pica_plan(source.plan,bg2.plan)) throw std::invalid_argument("3DS terrain belongs to another source eye plan");
    if(bg2.draws.empty()) {vertices_.clear();draws_.clear();images_.clear();return {source.plan,{},{},{},bg2.clear};}
    if(bg2.draws.size()!=bg2.textures.size() || bg2.draws.size()>pica_raster_max_strips
        || bg2.vertices.size()!=bg2.draws.size()*6)
        throw std::invalid_argument("3DS terrain requires an isolated guarded BG2 source raster");
    // Isolated BG2 is identified by its source painter contract, not palette
    // colors or model rectangles. Retain opaque black. Never re-walk its
    // complete provenance image for every slider/eye presentation.
    const double focal_x=source.plan.focal_x,focal_y=source.plan.focal_y;
    const double distance_scale=plane.height*focal_y;
    const auto guard=pica_receiver_guard(source.plan,{-plane.slope/distance_scale,1/distance_scale,
        (200*plane.slope-plane.centre)/distance_scale});
    const double coverage_left=bg2.vertices.front().position[0];
    // Last vertex of each source quad is its left-bottom corner, not right.
    const double right=bg2.vertices[bg2.vertices.size()-4].position[0];
    if(decoded_guard?decoded_guard<guard:coverage_left> -double(guard) || right<top_width+double(guard))
        throw std::invalid_argument("3DS terrain raster does not cover both eye receivers");
    using Point=std::array<double,2>;
    const auto distance=[&](Point point) {return point[1]-plane.centre-plane.slope*(point[0]-200);};
    const auto clip=[&](SceneryPolygon2& polygon,double limit,bool above) {
        const auto old=polygon;polygon.clear();
        if(old.empty()) return;
        auto a=old.back();double da=distance(a)-limit;
        for(const auto b:old) {
            const double db=distance(b)-limit;
            const bool inside_a=above?da>=0:da<=0,inside_b=above?db>=0:db<=0;
            if(inside_a!=inside_b) {
                const double t=da/(da-db);
                polygon.push_back({std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t)});
            }
            if(inside_b) polygon.push_back(b);
            a=b;da=db;
        }
    };
    ensure_vertex_space(bg2.vertices.size());
    auto& next=pending_vertices_;next.assign(bg2.vertices.begin(),bg2.vertices.end());
    auto& images=pending_images_;images.assign(bg2.textures.begin(),bg2.textures.end());
    auto& draws=pending_draws_;draws.assign(bg2.draws.begin(),bg2.draws.end());
    double previous=coverage_left;
    for(unsigned strip=0;strip<images.size();++strip) {
        auto& image=images[strip];auto& sky=draws[strip];
        const auto origin=bg2.vertices[strip*6].position;
        const double left=origin[0],end=left+image.width,top=origin[1],bottom=top+image.height;
        if(sky.first!=strip*6 || sky.count!=6 || sky.texture!=strip || sky.space!=PicaSpace::scenery
            || sky.source_layer!=2 || image.channels!=4 || image.repeat || image.source_layers.empty()
            || (!decoded_guard && (image.height!=screen_height || left!=previous))
            || (decoded_guard && (left< -double(decoded_guard) || end>top_width+decoded_guard || top<0 || bottom>screen_height))
            || origin[2] || bg2.vertices[strip*6+2].position!=Point3{float(end),float(bottom),0})
            throw std::invalid_argument("Invalid 3DS source receiver strip");
        static_cast<void>(pica_texture_layout(image));previous=end;
        image.source_layers={};image.layer_pitch=0;sky.alpha_blend=false;
    }
    // All infinity strips precede every finite strip: interleaving would let
    // a later no-depth sky overwrite an earlier receiver after eye parallax.
    for(unsigned strip=0;strip<images.size();++strip) {
        const auto& image=images[strip];const auto origin=bg2.vertices[strip*6].position;
        const double left=origin[0],end=left+image.width,top=origin[1],bottom=top+image.height;
        SceneryPolygon2 polygon{{left,top},{end,top},{end,bottom},{left,bottom}};
        clip(polygon,distance_scale/source.plan.far_plane,true);
        clip(polygon,distance_scale/source.plan.near_plane,false);
        const unsigned first=unsigned(next.size());
        ensure_vertex_space(first+(polygon.size()>=3?(polygon.size()-2)*3:0));
        for(unsigned corner=1;corner+1<polygon.size();++corner) for(unsigned i:{0U,corner,corner+1}) {
            const auto point=polygon[i];const double z=distance_scale/distance(point);
            next.push_back({{float((point[0]-200)*z/focal_x),float((120-point[1])*z/focal_y),float(z)},
                {1,1,1,1},{float(std::clamp((point[0]-left)/image.width,0.,1.)),float(std::clamp((point[1]-top)/image.height,0.,1.))}});
        }
        if(next.size()>first) {
            PicaDraw ground;ground.first=first;ground.count=unsigned(next.size())-first;ground.texture=strip;
            ground.source_layer=2;ground.projected_uv=true;draws.push_back(ground);
        }
    }
    return publish(source,bg2);
}
std::optional<PicaFrame> GameScenery::prepare_tiles(const GamePresentation& source,const PicaFrame& bg2,
    unsigned available_guard,unsigned vertex_budget) {
    vertex_budget=std::min(vertex_budget,pica_vertex_limit);
    const auto plane=source_landscape_plane(source);
    if(!same_pica_plan(source.plan,bg2.plan)) throw std::invalid_argument("3DS terrain atlas belongs to another eye plan");
    const double scale=plane.height*source.plan.focal_y;
    const auto guard=pica_receiver_guard(source.plan,{-plane.slope/scale,1/scale,
        (200*plane.slope-plane.centre)/scale});
    if(available_guard<guard) throw std::invalid_argument("3DS terrain atlas does not cover both eye receivers");
    if(bg2.draws.empty()) {
        if(!bg2.vertices.empty() || !bg2.textures.empty()) throw std::invalid_argument("Incomplete empty 3DS terrain atlas");
        vertices_.clear();draws_.clear();images_.clear();return PicaFrame{source.plan,{},{},{},bg2.clear};
    }
    if(bg2.draws.size()!=1 || bg2.textures.size()!=1 || bg2.vertices.size()%6)
        throw std::invalid_argument("3DS terrain atlas requires isolated source quads");
    const auto& draw=bg2.draws.front();const auto& image=bg2.textures.front();
    if(draw.first || draw.count!=bg2.vertices.size() || draw.texture || draw.source_layer!=2
        || draw.space!=PicaSpace::scenery || draw.depth_test || draw.depth_write || draw.alpha_blend
        || draw.projected_uv || draw.screen_dither || draw.clip || draw.colour_op
        || draw.model!=pica_identity || image.channels!=4 || image.repeat
        || !image.source_layers.empty()) throw std::invalid_argument("Invalid 3DS terrain tile ownership");
    static_cast<void>(pica_texture_layout(image));
    if(bg2.vertices.size()>vertex_budget) return {};
    using Point=std::array<double,2>;
    const auto distance=[&](Point p){return p[1]-plane.centre-plane.slope*(p[0]-200);};
    // Two half-plane clips can grow a rectangle to six corners. Stack storage
    // avoids an allocating polygon for each of thousands of source tiles.
    const auto clip=[&](std::array<Point,8>& polygon,unsigned count,double limit,bool above) {
        const auto old=polygon;unsigned next=0;
        if(!count) return next;
        auto a=old[count-1];double da=distance(a)-limit;
        const auto add=[&](Point p) {
            if(next==polygon.size()) throw std::logic_error("3DS terrain tile clipping exceeded bounded geometry");
            polygon[next++]=p;
        };
        for(unsigned i=0;i<count;++i) {
            const auto b=old[i];const double db=distance(b)-limit;
            const bool ia=above?da>=0:da<=0,ib=above?db>=0:db<=0;
            if(ia!=ib) {const double t=da/(da-db);add({std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t)});}
            if(ib) add(b);
            a=b;da=db;
        }
        return next;
    };
    ensure_vertex_space(bg2.vertices.size());
    auto& vertices=pending_vertices_;vertices.assign(bg2.vertices.begin(),bg2.vertices.end());
    for(unsigned at=0;at<bg2.vertices.size();at+=6) {
        const auto& a=bg2.vertices[at];const auto& b=bg2.vertices[at+1];const auto& c=bg2.vertices[at+2];
        const auto& d=bg2.vertices[at+5];
        const double x0=a.position[0],y0=a.position[1],x1=c.position[0],y1=c.position[1];
        if(!(x0<x1 && y0<y1) || x0< -double(available_guard) || x1>400+double(available_guard)
            || y0<0 || y1>240 || a.position[2] || b.position!=Point3{float(x1),float(y0),0}
            || c.position[2] || d.position!=Point3{float(x0),float(y1),0}
            || a!=bg2.vertices[at+3] || c!=bg2.vertices[at+4]
            || a.colour!=std::array<float,4>{1,1,1,1} || b.colour!=a.colour || c.colour!=a.colour || d.colour!=a.colour
            || b.uv!=std::array<float,2>{c.uv[0],a.uv[1]} || d.uv!=std::array<float,2>{a.uv[0],c.uv[1]})
            throw std::invalid_argument("3DS terrain atlas changed an axis-aligned source tile");
        std::array<Point,8> polygon{{{x0,y0},{x1,y0},{x1,y1},{x0,y1}}};
        unsigned count=clip(polygon,4,scale/source.plan.far_plane,true);
        count=clip(polygon,count,scale/source.plan.near_plane,false);
        const unsigned extra=count>=3?(count-2)*3:0;
        if(extra>vertex_budget-vertices.size()) return {};
        ensure_vertex_space(vertices.size()+extra);
        // A fan repeats its origin and shared edges. Transform each retained
        // corner once, without changing clipping, arithmetic or fan ordering.
        std::array<PicaVertex,8> transformed;
        if(count>=3) for(unsigned i=0;i<count;++i) {
            const auto p=polygon[i];const double z=scale/distance(p);
            transformed[i]={{float((p[0]-200)*z/source.plan.focal_x),float((120-p[1])*z/source.plan.focal_y),float(z)},
                {1,1,1,1},{float(a.uv[0]+(c.uv[0]-double(a.uv[0]))*(p[0]-x0)/(x1-x0)),
                    float(a.uv[1]+(c.uv[1]-double(a.uv[1]))*(p[1]-y0)/(y1-y0))}};
        }
        for(unsigned corner=1;corner+1<count;++corner) for(unsigned i:{0U,corner,corner+1})
            vertices.push_back(transformed[i]);
    }
    auto& draws=pending_draws_;draws.assign(bg2.draws.begin(),bg2.draws.end());
    // The complete infinity atlas must precede every finite tile, exactly as
    // in the raster receiver. Never interleave far artwork over another tile.
    if(vertices.size()>bg2.vertices.size()) {
        PicaDraw ground;ground.first=unsigned(bg2.vertices.size());ground.count=unsigned(vertices.size())-ground.first;
        ground.texture=0;ground.source_layer=2;ground.projected_uv=true;draws.push_back(ground);
    }
    auto& images=pending_images_;images.assign(bg2.textures.begin(),bg2.textures.end());
    return publish(source,bg2);
}
std::optional<PicaFrame> GameScenery::prepare_water_tiles(const GamePresentation& source,const PicaFrame& bg2,
    unsigned available_guard,unsigned vertex_budget) {
    vertex_budget=std::min(vertex_budget,pica_vertex_limit);
    const double distance=source_water_height(source)*source.plan.focal_y;
    if(!same_pica_plan(source.plan,bg2.plan) || available_guard<source_water_guard(source))
        throw std::invalid_argument("3DS water atlas does not cover its eye receivers");
    if(bg2.draws.empty()) {
        if(!bg2.vertices.empty() || !bg2.textures.empty()) throw std::invalid_argument("Incomplete empty water atlas");
        vertices_.clear();draws_.clear();images_.clear();return PicaFrame{source.plan,{},{},{},bg2.clear};
    }
    if(bg2.draws.size()!=1 || bg2.textures.size()!=1 || bg2.vertices.size()%6)
        throw std::invalid_argument("Water atlas requires isolated source quads");
    const auto& draw=bg2.draws.front();const auto& image=bg2.textures.front();
    if(draw.first || draw.count!=bg2.vertices.size() || draw.texture || draw.source_layer!=2
        || draw.space!=PicaSpace::scenery || draw.depth_test || draw.depth_write || draw.alpha_blend
        || draw.projected_uv || draw.screen_dither || draw.clip || draw.colour_op
        || draw.model!=pica_identity || image.channels!=4 || image.repeat || !image.source_layers.empty())
        throw std::invalid_argument("Invalid water tile ownership");
    static_cast<void>(pica_texture_layout(image));
    for(unsigned at=0;at<bg2.vertices.size();at+=6) {
        const auto& a=bg2.vertices[at];const auto& b=bg2.vertices[at+1];const auto& c=bg2.vertices[at+2];
        const auto& d=bg2.vertices[at+5];
        if(!(a.position[0]<c.position[0] && a.position[1]<c.position[1])
            || a.position[0]< -double(available_guard) || c.position[0]>400+double(available_guard)
            || a.position[1]<0 || c.position[1]>240 || a.position[2] || c.position[2]
            || b.position!=Point3{c.position[0],a.position[1],0} || d.position!=Point3{a.position[0],c.position[1],0}
            || a!=bg2.vertices[at+3] || c!=bg2.vertices[at+4]
            || a.colour!=std::array<float,4>{1,1,1,1} || b.colour!=a.colour || c.colour!=a.colour || d.colour!=a.colour
            || b.uv!=std::array<float,2>{c.uv[0],a.uv[1]} || d.uv!=std::array<float,2>{a.uv[0],c.uv[1]})
            throw std::invalid_argument("Water atlas changed an axis-aligned source tile");
    }
    using Point=std::array<double,2>;
    const auto clip=[](std::array<Point,8>& polygon,unsigned count,double y,bool below) {
        const auto old=polygon;unsigned next=0;
        if(!count) return next;
        auto a=old[count-1];double da=a[1]-y;
        const auto add=[&](Point p) {
            if(next==polygon.size()) throw std::logic_error("Water tile clipping exceeded bounded geometry");
            polygon[next++]=p;
        };
        for(unsigned i=0;i<count;++i) {
            const auto b=old[i];const double db=b[1]-y;const bool ia=below?da>=0:da<=0,ib=below?db>=0:db<=0;
            if(ia!=ib) {const double t=da/(da-db);add({std::lerp(a[0],b[0],t),y});}
            if(ib) add(b);
            a=b;da=db;
        }
        return next;
    };
    auto& vertices=pending_vertices_;vertices.clear();
    auto& draws=pending_draws_;draws.clear();
    // Same two finite planes and narrow far-horizon band as prepare_water.
    // Do not retain the entire old water image underneath the finite surfaces.
    for(int side:{0,-1,1}) {
        const unsigned first=unsigned(vertices.size());
        for(unsigned at=0;at<bg2.vertices.size();at+=6) {
            const auto& a=bg2.vertices[at];const auto& c=bg2.vertices[at+2];
            const double x0=a.position[0],y0=a.position[1],x1=c.position[0],y1=c.position[1];
            std::array<Point,8> polygon{{{x0,y0},{x1,y0},{x1,y1},{x0,y1}}};unsigned count=4;
            if(!side) {
                count=clip(polygon,count,120-distance/source.plan.far_plane,true);
                count=clip(polygon,count,120+distance/source.plan.far_plane,false);
            } else {
                count=clip(polygon,count,120+side*distance/source.plan.far_plane,side>0);
                count=clip(polygon,count,120+side*distance/source.plan.near_plane,side<0);
            }
            const unsigned extra=count>=3?(count-2)*3:0;
            if(extra>vertex_budget-vertices.size()) return {};
            ensure_vertex_space(vertices.size()+extra);
            std::array<PicaVertex,8> transformed;
            if(count>=3) for(unsigned i=0;i<count;++i) {
                const auto p=polygon[i];const double z=side?distance/(side*(p[1]-120)):0;
                const Point3 position=side?Point3{float((p[0]-200)*z/source.plan.focal_x),
                    float((120-p[1])*z/source.plan.focal_y),float(z)}:Point3{float(p[0]),float(p[1]),0};
                transformed[i]={position,{1,1,1,1},{float(a.uv[0]+(c.uv[0]-double(a.uv[0]))*(p[0]-x0)/(x1-x0)),
                    float(a.uv[1]+(c.uv[1]-double(a.uv[1]))*(p[1]-y0)/(y1-y0))}};
            }
            for(unsigned corner=1;corner+1<count;++corner) for(unsigned i:{0U,corner,corner+1})
                vertices.push_back(transformed[i]);
        }
        if(vertices.size()!=first) {
            PicaDraw next;next.first=first;next.count=unsigned(vertices.size())-first;next.texture=0;next.source_layer=2;
            next.space=side?PicaSpace::world:PicaSpace::scenery;next.projected_uv=side!=0;
            next.depth_test=next.depth_write=side!=0;draws.push_back(next);
        }
    }
    auto& images=pending_images_;images.assign(bg2.textures.begin(),bg2.textures.end());
    return publish(source,bg2);
}
PicaFrame GameScenery::prepare_water(const GamePresentation& source,const PicaFrame& bg2,unsigned available_guard) {
    const double height=source_water_height(source),distance=height*source.plan.focal_y;
    if(!same_pica_plan(source.plan,bg2.plan) || available_guard<source_water_guard(source))
        throw std::invalid_argument("3DS water source does not cover both eye receivers");
    if(bg2.draws.size()!=bg2.textures.size() || bg2.draws.size()>pica_raster_max_strips
        || bg2.vertices.size()!=bg2.draws.size()*6)
        throw std::invalid_argument("3DS water requires isolated BG2 source strips");
    const auto clip=[](SceneryPolygon2& polygon,double y,bool below) {
        const auto old=polygon;polygon.clear();if(old.empty()) return;
        auto a=old.back();double da=a[1]-y;
        for(const auto b:old) {
            const double db=b[1]-y;const bool ia=below?da>=0:da<=0,ib=below?db>=0:db<=0;
            if(ia!=ib) {
                const double t=da/(da-db);
                polygon.push_back({std::lerp(a[0],b[0],t),y});
            }
            if(ib) polygon.push_back(b);
            a=b;da=db;
        }
    };
    auto& vertices=pending_vertices_;vertices.clear();auto& draws=pending_draws_;draws.clear();
    auto& images=pending_images_;images.assign(bg2.textures.begin(),bg2.textures.end());
    for(unsigned i=0;i<images.size();++i) {
        auto& image=images[i];const auto& draw=bg2.draws[i];
        if(draw.first!=i*6 || draw.count!=6 || draw.texture!=i || draw.source_layer!=2
            || draw.space!=PicaSpace::scenery || image.channels!=4 || image.repeat || image.source_layers.empty())
            throw std::invalid_argument("Invalid isolated 3DS water artwork");
        static_cast<void>(pica_texture_layout(image));image.source_layers={};image.layer_pitch=0;
    }
    // Only the subpixel far-horizon band remains at infinity. Never leave a
    // complete old planar water/bridge image beneath the finite surfaces.
    for(int side:{0,-1,1}) for(unsigned strip=0;strip<images.size();++strip) {
        const auto& image=images[strip];const auto& origin=bg2.vertices[strip*6].position;
        const double left=origin[0],top=origin[1],right=left+image.width,bottom=top+image.height;
        SceneryPolygon2 polygon{{left,top},{right,top},{right,bottom},{left,bottom}};
        if(side==0) {
            clip(polygon,120-distance/source.plan.far_plane,true);
            clip(polygon,120+distance/source.plan.far_plane,false);
        } else {
            clip(polygon,120+side*distance/source.plan.far_plane,side>0);
            clip(polygon,120+side*distance/source.plan.near_plane,side<0);
        }
        const unsigned first=unsigned(vertices.size());
        ensure_vertex_space(first+(polygon.size()>=3?(polygon.size()-2)*3:0));
        for(unsigned corner=1;corner+1<polygon.size();++corner) for(unsigned k:{0U,corner,corner+1}) {
            const auto point=polygon[k];const double z=side?distance/(side*(point[1]-120)):0;
            const Point3 position=side?Point3{float((point[0]-200)*z/source.plan.focal_x),
                float((120-point[1])*z/source.plan.focal_y),float(z)}:Point3{float(point[0]),float(point[1]),0};
            vertices.push_back({position,{1,1,1,1},
                {float(std::clamp((point[0]-left)/image.width,0.,1.)),float(std::clamp((point[1]-top)/image.height,0.,1.))}});
        }
        if(vertices.size()!=first) {
            PicaDraw draw;draw.first=first;draw.count=unsigned(vertices.size())-first;draw.texture=strip;
            draw.source_layer=2;draw.space=side?PicaSpace::world:PicaSpace::scenery;
            draw.projected_uv=side!=0;draw.depth_test=draw.depth_write=side!=0;draws.push_back(draw);
        }
    }
    return publish(source,bg2);
}
PicaFrame GameScenery::prepare_corridor(const GamePresentation& source,const PicaFrame& bg2,unsigned available_guard) {
    const auto geometry=corridor_geometry(source);
    const auto planes=source_corridor_planes(source);
    if(!same_pica_plan(source.plan,bg2.plan) || available_guard<source_corridor_guard(source))
        throw std::invalid_argument("3DS corridor source does not cover both eyes");
    if(bg2.draws.size()!=bg2.textures.size() || bg2.draws.size()>pica_raster_max_strips
        || bg2.vertices.size()!=bg2.draws.size()*6)
        throw std::invalid_argument("3DS corridor requires isolated BG2 strips");
    using Point=std::array<double,2>;using Plane=std::array<double,3>;
    const auto depth=[](Point p,Plane q){return q[0]*p[0]+q[1]*p[1]+q[2];};
    const auto clip=[&](SceneryPolygon2& polygon,Plane q) {
        const auto old=polygon;polygon.clear();if(old.empty()) return;
        auto a=old.back();double da=depth(a,q);
        for(const auto b:old) {
            const double db=depth(b,q);
            if((da>=0)!=(db>=0)) {
                const double t=da/(da-db);polygon.push_back({std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t)});
            }
            if(db>=0) polygon.push_back(b);
            a=b;da=db;
        }
    };
    auto& vertices=pending_vertices_;vertices.clear();auto& draws=pending_draws_;draws.clear();
    auto& images=pending_images_;images.assign(bg2.textures.begin(),bg2.textures.end());
    for(unsigned i=0;i<images.size();++i) {
        auto& image=images[i];const auto& draw=bg2.draws[i];
        if(draw.first!=i*6 || draw.count!=6 || draw.texture!=i || draw.source_layer!=2
            || draw.space!=PicaSpace::scenery || image.channels!=4 || image.repeat || image.source_layers.empty())
            throw std::invalid_argument("Invalid isolated 3DS corridor artwork");
        static_cast<void>(pica_texture_layout(image));image.source_layers={};image.layer_pitch=0;
    }
    // Partition source rays by their first tube intersection. A single flat
    // image, four unbounded overlapping planes, or a floor/ceiling-only mesh
    // gives incorrect side-wall depth and changes native pixels at the corners.
    // Only the tiny region beyond far remains infinity, before every receiver.
    double mesh_guard=available_guard;
    for(unsigned eye=0;eye<source.plan.eye_count;++eye)
        mesh_guard=std::max(mesh_guard,std::abs(double(source.plan.eyes[eye].projection_offset))
            +std::abs(double(source.plan.focal_x)*source.plan.eyes[eye].x)/source.plan.near_plane);
    if(geometry.exterior) {
        // The positive-distance reciprocal partition below is only valid from
        // inside the tube. Intersect actual signed faces with the guarded view
        // frustum instead: no division by a zero/negative camera-wall distance,
        // no invented wall across the colony opening, and no source-camera cut.
        using SpatialPoint=std::array<double,3>;
        const auto dot=[](SpatialPoint a,SpatialPoint b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
        const auto spatial_clip=[&](SceneryPolygon3& polygon,SpatialPoint normal,double offset) {
            const auto old=polygon;polygon.clear();if(old.empty()) return;
            auto a=old.back();double da=dot(normal,a)+offset;
            for(const auto b:old) {
                const double db=dot(normal,b)+offset;
                if((da>=0)!=(db>=0)) {
                    const double t=da/(da-db);
                    polygon.push_back({std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t),std::lerp(a[2],b[2],t)});
                }
                if(db>=0) polygon.push_back(b);
                a=b;da=db;
            }
        };
        std::array<SpatialPoint,8> frustum{};
        for(unsigned plane=0;plane<2;++plane) for(unsigned corner=0;corner<4;++corner) {
            const double z=plane?source.plan.far_plane:source.plan.near_plane;
            const double x=corner==1 || corner==2?400+mesh_guard:-mesh_guard;
            const double y=corner>=2?240:0;
            frustum[plane*4+corner]={(x-200)*z/source.plan.focal_x,(120-y)*z/source.plan.focal_y,z};
        }
        constexpr std::array<std::array<unsigned,2>,12> edges{{{0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}};
        // The source has no separate authored outside-tube plate. Keep its
        // isolated BG2 far field behind the bounded faces, never a final model
        // image. Missing disoccluded exterior artwork remains a separate gate.
        ensure_vertex_space(bg2.vertices.size());
        vertices.assign(bg2.vertices.begin(),bg2.vertices.end());draws.assign(bg2.draws.begin(),bg2.draws.end());
        for(unsigned wall=0;wall<4;++wall) {
            if(!(source.current->background_corridor->walls&(1U<<wall))) continue;
            SceneryPolygon3 face;
            const auto add=[&](SpatialPoint p) {
                if(std::none_of(face.begin(),face.end(),[&](const auto q) {
                    return std::abs(p[0]-q[0])+std::abs(p[1]-q[1])+std::abs(p[2]-q[2])<1.e-7;
                })) face.push_back(p);
            };
            const auto normal=geometry.normals[wall];const double distance=geometry.distances[wall];
            for(const auto edge:edges) {
                const auto a=frustum[edge[0]],b=frustum[edge[1]];
                const double da=dot(normal,a)-distance,db=dot(normal,b)-distance;
                if(da==0) add(a);
                if(db==0) add(b);
                if((da<0 && db>0) || (da>0 && db<0)) {
                    const double t=da/(da-db);
                    add({std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t),std::lerp(a[2],b[2],t)});
                }
            }
            if(face.size()<3) continue;
            SpatialPoint centre{};
            for(const auto p:face) for(unsigned axis=0;axis<3;++axis) centre[axis]+=p[axis]/face.size();
            SpatialPoint u{};for(unsigned axis=0;axis<3;++axis) u[axis]=face.front()[axis]-centre[axis];
            const SpatialPoint v{normal[1]*u[2]-normal[2]*u[1],normal[2]*u[0]-normal[0]*u[2],normal[0]*u[1]-normal[1]*u[0]};
            const auto angle=[&](SpatialPoint p) {
                for(unsigned axis=0;axis<3;++axis) p[axis]-=centre[axis];
                return std::atan2(dot(p,v),dot(p,u));
            };
            for(unsigned i=1;i<face.size();++i) {
                const auto p=face[i];const double a=angle(p);unsigned at=i;
                while(at && angle(face[at-1])>a) {face[at]=face[at-1];--at;}
                face[at]=p;
            }
            for(unsigned other=0;other<4;++other) if(other!=wall && (source.current->background_corridor->walls&(1U<<other))) {
                auto n=geometry.normals[other];for(auto& value:n) value=-value;
                spatial_clip(face,n,geometry.distances[other]);
            }
            for(unsigned strip=0;strip<images.size();++strip) for(int edge:{-1,0,1}) {
                const auto& image=images[strip];const auto& origin=bg2.vertices[strip*6].position;
                const double left=origin[0],top=origin[1],right=left+image.width;
                if(edge<0 && (strip!=0 || left> -double(available_guard))) continue;
                if(edge>0 && (strip+1!=images.size() || right<400+double(available_guard))) continue;
                const double begin=edge<0?-mesh_guard:edge>0?right:left,end=edge<0?left:edge>0?400+mesh_guard:right;
                if(begin>=end) continue;
                auto polygon=face;
                spatial_clip(polygon,{source.plan.focal_x,0,200-begin},0);
                spatial_clip(polygon,{-source.plan.focal_x,0,end-200},0);
                spatial_clip(polygon,{0,-source.plan.focal_y,120-top},0);
                spatial_clip(polygon,{0,source.plan.focal_y,top+image.height-120},0);
                const unsigned first=unsigned(vertices.size());
                ensure_vertex_space(first+(polygon.size()>=3?(polygon.size()-2)*3:0));
                for(unsigned corner=1;corner+1<polygon.size();++corner) for(unsigned k:{0U,corner,corner+1}) {
                    const auto p=polygon[k];const double x=200+source.plan.focal_x*p[0]/p[2],y=120-source.plan.focal_y*p[1]/p[2];
                    vertices.push_back({{float(p[0]),float(p[1]),float(p[2])},{1,1,1,1},
                        {float(std::clamp((x-left)/image.width,0.,1.)),float(std::clamp((y-top)/image.height,0.,1.))}});
                }
                if(vertices.size()!=first) {
                    PicaDraw draw;draw.first=first;draw.count=unsigned(vertices.size())-first;draw.texture=strip;
                    draw.source_layer=2;draw.projected_uv=true;draws.push_back(draw);
                }
            }
        }
        return publish(source,bg2);
    }
    for(unsigned surface=0;surface<5;++surface) for(unsigned strip=0;strip<images.size();++strip) for(int edge:{-1,0,1}) {
        if(surface && !(source.current->background_corridor->walls&(1U<<(surface-1)))) continue;
        const auto& image=images[strip];const auto& origin=bg2.vertices[strip*6].position;
        const double left=origin[0],top=origin[1],right=left+image.width;
        if(edge<0 && (strip!=0 || left> -double(available_guard))) continue;
        if(edge>0 && (strip+1!=images.size() || right<top_width+double(available_guard))) continue;
        const double begin=edge<0?-mesh_guard:edge>0?right:left,end=edge<0?left:edge>0?top_width+mesh_guard:right;
        if(begin>=end) continue;
        SceneryPolygon2 polygon{{begin,top},{end,top},{end,top+image.height},{begin,top+image.height}};
        if(surface==0) for(const auto q:planes) clip(polygon,{-q[0],-q[1],1./source.plan.far_plane-q[2]});
        else {
            const auto q=planes[surface-1];
            for(unsigned other=0;other<planes.size();++other) if(other!=surface-1)
                clip(polygon,{q[0]-planes[other][0],q[1]-planes[other][1],q[2]-planes[other][2]});
            clip(polygon,{q[0],q[1],q[2]-1./source.plan.far_plane});
            clip(polygon,{-q[0],-q[1],1./source.plan.near_plane-q[2]});
        }
        const unsigned first=unsigned(vertices.size());
        ensure_vertex_space(first+(polygon.size()>=3?(polygon.size()-2)*3:0));
        for(unsigned corner=1;corner+1<polygon.size();++corner) for(unsigned k:{0U,corner,corner+1}) {
            const auto point=polygon[k];const double z=surface?1./depth(point,planes[surface-1]):0;
            const Point3 position=surface?Point3{float((point[0]-200)*z/source.plan.focal_x),
                float((120-point[1])*z/source.plan.focal_y),float(z)}:Point3{float(point[0]),float(point[1]),0};
            vertices.push_back({position,{1,1,1,1},
                {float(std::clamp((point[0]-left)/image.width,0.,1.)),float(std::clamp((point[1]-top)/image.height,0.,1.))}});
        }
        if(vertices.size()!=first) {
            PicaDraw draw;draw.first=first;draw.count=unsigned(vertices.size())-first;draw.texture=strip;draw.source_layer=2;
            draw.space=surface?PicaSpace::world:PicaSpace::scenery;draw.projected_uv=surface!=0;
            draw.depth_test=draw.depth_write=surface!=0;draws.push_back(draw);
        }
    }
    return publish(source,bg2);
}
} // namespace starfox::platform::nintendo_3ds
