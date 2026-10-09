#include "starfox/vr/cockpit.hpp"
#include <cmath>
#include "starfox/compat/bit_cast.hpp"
#include <stdexcept>
#include <vector>
#include "cockpit_assets.inc"
namespace starfox::vr {
namespace {
// Presentation calibration only: leave the instrument face and lower cabin
// fixed, widening/lowering the window and blending their shared connections.
std::array<float,3> window_frame_position(std::array<float,3> p) {
    const float weight=std::clamp((p[1]+.72F)/.24F,0.F,1.F);
    p[0]*=1.F+.7F*weight;p[1]-=.16F*weight;return p;
}
SceneVertex flat_vertex(std::array<float,3> position,uint32_t rgb,bool srgb,unsigned brightness) {
    SceneVertex vertex{};std::copy(position.begin(),position.end(),vertex.position);
    for(unsigned c=0;c<3;++c) {
        float value=float((rgb>>(16-8*c))&255)/255.F*float(std::min(brightness,15U))/15.F;
        if(srgb)value=value<=.04045F?value/12.92F:std::pow((value+.055F)/1.055F,2.4F);
        vertex.color[c]=value;
    }
    vertex.color[3]=1;return vertex;
}
SceneVertex between(const SceneVertex& a,const SceneVertex& b,float t) {
    auto v=a;
    for(unsigned i=0;i<3;++i)v.position[i]=a.position[i]+(b.position[i]-a.position[i])*t;
    for(unsigned i=0;i<4;++i) {
        v.color[i]=a.color[i]+(b.color[i]-a.color[i])*t;
        v.odd_color[i]=a.odd_color[i]+(b.odd_color[i]-a.odd_color[i])*t;
    }
    for(unsigned i=0;i<2;++i)v.uv[i]=a.uv[i]+(b.uv[i]-a.uv[i])*t;
    return v;
}
// Keeps the part of a convex polygon on one side of an axis plane.
std::vector<SceneVertex> clip_axis(const std::vector<SceneVertex>& polygon,unsigned axis,float value,bool keep_above) {
    std::vector<SceneVertex> out;
    for(size_t i=0;i<polygon.size();++i) {
        const auto& a=polygon[i];const auto& b=polygon[(i+1)%polygon.size()];
        const bool in_a=keep_above?a.position[axis]>=value:a.position[axis]<=value;
        const bool in_b=keep_above?b.position[axis]>=value:b.position[axis]<=value;
        if(in_a)out.push_back(a);
        if(in_a!=in_b)out.push_back(between(a,b,(value-a.position[axis])/(b.position[axis]-a.position[axis])));
    }
    return out;
}
// Pieces lying on a cut plane have no area and would only add slivers.
bool has_area(const std::vector<SceneVertex>& polygon) {
    if(polygon.size()<3)return false;
    float normal[3]{};
    for(size_t i=1;i+1<polygon.size();++i) {
        const auto* a=polygon[0].position;const auto* b=polygon[i].position;const auto* c=polygon[i+1].position;
        const float u[]{b[0]-a[0],b[1]-a[1],b[2]-a[2]},v[]{c[0]-a[0],c[1]-a[1],c[2]-a[2]};
        normal[0]+=u[1]*v[2]-u[2]*v[1];normal[1]+=u[2]*v[0]-u[0]*v[2];normal[2]+=u[0]*v[1]-u[1]*v[0];
    }
    return normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]>1e-14F;
}
// Convex pieces of a convex polygon that lie outside an axis-aligned box.
std::vector<std::vector<SceneVertex>> subtract_box(std::vector<SceneVertex> inside,const CockpitCutout& box) {
    std::vector<std::vector<SceneVertex>> out;
    // Leave polygons that cannot touch the box whole.
    for(unsigned axis=0;axis<3;++axis) {
        float low=inside[0].position[axis],high=low;
        for(const auto& v:inside) {low=std::min(low,v.position[axis]);high=std::max(high,v.position[axis]);}
        if(high<=box.low[axis] || low>=box.high[axis]) {out.push_back(std::move(inside));return out;}
    }
    for(unsigned axis=0;axis<3;++axis)for(bool high:{false,true}) {
        const float value=high?box.high[axis]:box.low[axis];
        auto outside=clip_axis(inside,axis,value,high);
        if(has_area(outside))out.push_back(std::move(outside));
        inside=clip_axis(inside,axis,value,!high);
        if(inside.size()<3)return out;
    }
    return out;
}
void subtract_box(const std::array<SceneVertex,2>& segment,const CockpitCutout& box,std::vector<std::array<SceneVertex,2>>& out) {
    float enter=0,leave=1;
    for(unsigned axis=0;axis<3;++axis) {
        const float a=segment[0].position[axis],d=segment[1].position[axis]-a;
        if(d==0) {if(a<box.low[axis] || a>box.high[axis]) {out.push_back(segment);return;}continue;}
        float t0=(box.low[axis]-a)/d,t1=(box.high[axis]-a)/d;if(t0>t1)std::swap(t0,t1);
        enter=std::max(enter,t0);leave=std::min(leave,t1);
    }
    if(enter>=leave) {out.push_back(segment);return;}
    if(enter>0)out.push_back({segment[0],between(segment[0],segment[1],enter)});
    if(leave<1)out.push_back({between(segment[0],segment[1],leave),segment[1]});
}
SceneVertex pilot_vertex(SceneVertex v) {
    v.position[0]/=256.F;v.position[1]/=-256.F;v.position[2]/=-256.F;
    // The cabin is seen from independent head poses, not the cartridge's eye.
    v.visibility_enabled=v.group_enabled=0;return v;
}
}
Matrix4 cockpit_instrument_mount(bool extended) noexcept {
    // EX's fixed Layout A band is 190 source pixels wide (Original fits 141).
    // Uniform, cartridge-stable fit retains artwork/aspect without health-driven resizing.
    const float pixel=.00305F*(extended?141.F/190.F:1.F);
    const float anchor_x=extended?101.F:76.F;
    return {pixel,0,0,0,0,-pixel,0,0,0,0,1,0,
        -.015F+(extended?.001525F:0.F)-anchor_x*pixel,-.852F+175*pixel,-1.243F,1};
}
void mount_cockpit_instruments(std::span<DrawPacket> packets,bool extended) {
    const auto overlay=overlay_panel_matrix();auto inverse=identity_matrix;
    inverse[0]=1/overlay[0];inverse[5]=1/overlay[5];
    inverse[12]=-overlay[12]/overlay[0];inverse[13]=-overlay[13]/overlay[5];inverse[14]=-overlay[14];
    const auto mount=multiply_matrix(cockpit_instrument_mount(extended),inverse);
    for(auto& packet:packets)packet.model=multiply_matrix(mount,packet.model);
}
DrawPacket cockpit_rear_packet(bool srgb,unsigned brightness) {
    DrawPacket out;
    for(const auto& triangle:cockpit_assets::rear) for(const auto& p:triangle.points)
        out.geometry.vertices.push_back(flat_vertex(window_frame_position(p),triangle.rgb,srgb,brightness));
    return out;
}
DrawPacket cockpit_front_packet(const assets::Shape& shape,bool srgb,unsigned brightness) {
    if(shape.faces.size()!=66)throw std::runtime_error("Unsupported C cockpit face topology");
    std::array<render::Rgba8,256> palette{};
    for(auto& colour:palette)colour={255,255,255,255};
    render::RenderPose pose;pose.z=250;pose.scale=2;
    DrawPacket decoded;std::string error;
    if(!build_draw_packet(shape,pose,palette,112,1,false,256,decoded,error))
        throw std::runtime_error("C cockpit decode: "+error);
    const auto vertices=decoded.geometry.vertex_view();
    if(vertices.size()!=std::size(cockpit_assets::front)*3 || !decoded.geometry.deferred.empty()
        || !decoded.geometry.line_view().empty())throw std::runtime_error("Unsupported C cockpit packet topology");
    // Geometry-only signature verified against both bundled variants. Their
    // palette/descriptor addresses differ, but positions and face ranges match.
    // Reject a different cabin before any index-based material assignment.
    uint64_t signature=14695981039346656037ULL;
    const auto word=[&](uint32_t value) {for(unsigned i=0;i<4;++i) {
        signature^=(value>>(i*8))&255;signature*=1099511628211ULL;
    }};
    for(const auto& v:vertices)for(float value:v.position)word(starfox::bit_cast<uint32_t>(value));
    for(const auto& r:decoded.geometry.ranges) {word(uint32_t(r.source_face));word(r.first_vertex);word(r.vertex_count);}
    if(signature!=0x1a590b6396dbcd5aULL)throw std::runtime_error("Unsupported C cockpit geometry signature");
    DrawPacket out;
    for(const auto& assignment:cockpit_assets::front) {
        const auto range=std::find_if(decoded.geometry.ranges.begin(),decoded.geometry.ranges.end(),
            [&](const auto& r){return r.source_face==assignment.face && assignment.first>=r.first_vertex
                && assignment.first+3<=r.first_vertex+r.vertex_count;});
        if(range==decoded.geometry.ranges.end() || assignment.first!=range->first_vertex+assignment.triangle*3)
            throw std::runtime_error("Unsupported C cockpit face/material mapping");
        for(unsigned i=0;i<3;++i) {
            const auto& v=vertices[assignment.first+i];
            out.geometry.vertices.push_back(flat_vertex(window_frame_position({v.position[0]*.015F,
                -v.position[1]*.015F-.27F,-v.position[2]*.015F-1.4F}),assignment.rgb,srgb,brightness));
        }
    }
    return out;
}
DrawPacket cockpit_ship_packet(const DrawPacket& source,std::optional<std::array<float,2>> keep_x) {
    // The whole live ship surrounds the cabin: nose ahead, wings and tail
    // beside and behind the pilot. The native repair/upgrade wireframe shares
    // the player's source pose, so it uses the same rig with its blink state.
    // Hull inside the cabin cut-outs is removed; the cabin replaces it there.
    DrawPacket out;out.preserve_native_colour=source.preserve_native_colour;out.shading=source.shading;
    out.geometry.texels=source.geometry.texels;out.geometry.shared_texels=source.geometry.shared_texels;
    out.model={cockpit_ship_scale,0,0,0,0,cockpit_ship_scale,0,0,0,0,cockpit_ship_scale,0,
        -cockpit_seat_m[0],-cockpit_seat_m[1],-cockpit_seat_m[2],1};
    std::vector<CockpitCutout> local;
    for(const auto& box:cockpit_hull_cutouts) {
        auto& l=local.emplace_back();
        for(unsigned a=0;a<3;++a) {
            l.low[a]=(box.low[a]+cockpit_seat_m[a])/cockpit_ship_scale;
            l.high[a]=(box.high[a]+cockpit_seat_m[a])/cockpit_ship_scale;
        }
    }
    if(keep_x) {
        constexpr float far=1e6F;
        local.push_back({{-far,-far,-far},{(*keep_x)[0],far,far}});
        local.push_back({{(*keep_x)[1],-far,-far},{far,far,far}});
    }
    const auto vertices=source.geometry.vertex_view();
    if(vertices.size()%3)throw std::runtime_error("Invalid cockpit player triangle packet");
    for(size_t i=0;i<vertices.size();i+=3) {
        std::vector<std::vector<SceneVertex>> pieces{{pilot_vertex(vertices[i]),pilot_vertex(vertices[i+1]),pilot_vertex(vertices[i+2])}};
        for(const auto& box:local) {
            std::vector<std::vector<SceneVertex>> kept;
            for(auto& piece:pieces)for(auto& outside:subtract_box(std::move(piece),box))kept.push_back(std::move(outside));
            pieces=std::move(kept);
        }
        for(const auto& polygon:pieces)for(size_t j=1;j+1<polygon.size();++j)
            for(size_t k:{size_t{0},j,j+1})out.geometry.vertices.push_back(polygon[k]);
    }
    const auto lines=source.geometry.line_view();
    for(size_t i=0;i+1<lines.size();i+=2) {
        std::vector<std::array<SceneVertex,2>> segments{{pilot_vertex(lines[i]),pilot_vertex(lines[i+1])}};
        for(const auto& box:local) {
            std::vector<std::array<SceneVertex,2>> kept;
            for(const auto& segment:segments)subtract_box(segment,box,kept);
            segments=std::move(kept);
        }
        for(const auto& segment:segments)out.geometry.line_vertices.insert(out.geometry.line_vertices.end(),{segment[0],segment[1]});
    }
    return out;
}
CockpitGeometry::CockpitGeometry(const assets::RomImage& rom,const assets::SymbolMap& symbols)
    :decoder_(rom,symbols),symbols_(symbols) {
    const auto& values=symbols.find("FLASHPLAYER_STRAT");if(!values.empty())flash_player_=values.front();
    if(const auto& intact=symbols.find("MYSHIP_4");!intact.empty())intact_x_=x_extent(intact.front());
}
std::array<int,2> CockpitGeometry::x_extent(uint32_t shape) {
    auto found=live_x_.find(shape);
    if(found==live_x_.end()) {
        std::array<int,2> extent{0,0};
        for(const auto& v:decoder_.decode(shape).vertices) {extent[0]=std::min(extent[0],int(v.x));extent[1]=std::max(extent[1],int(v.x));}
        found=live_x_.emplace(shape,extent).first;
    }
    return found->second;
}
// The cutscene hull has no damage variants. When the live ship has lost a
// wing (MYSHIP_L/R/B are narrower than MYSHIP_4), trim it to the live extent.
std::optional<std::array<float,2>> CockpitGeometry::damaged_extent(uint32_t live_shape) {
    if(!intact_x_) return std::nullopt;
    const auto e=x_extent(live_shape);
    if(e==*intact_x_) return std::nullopt;
    return std::array<float,2>{e[0]>(*intact_x_)[0]?e[0]/256.F:-1e6F,e[1]<(*intact_x_)[1]?e[1]/256.F:1e6F};
}
std::vector<DrawPacket> CockpitGeometry::assemble(SourceModelPackets& world,const GameSceneSnapshot& scene,
    const PresentationPreferences& preferences,bool srgb) {
    if(!pilot_view_active(scene,preferences))return {};
    const unsigned key=std::min(unsigned(scene.display_brightness),15U)+(srgb?16:0);
    if(!cabin_[key]) {
        if(!front_)front_=decoder_.decode_by_name(symbols_,"COCKPIT");
        cabin_[key]=std::array{cockpit_front_packet(*front_,srgb,scene.display_brightness),
            cockpit_rear_packet(srgb,scene.display_brightness)};
    }
    std::vector<DrawPacket> out{(*cabin_[key])[0],(*cabin_[key])[1]};
    for(size_t i=0;i<world.handles.size();++i) {
        const bool player=world.handles[i]==scene.player;
        const auto object=std::find_if(scene.objects.begin(),scene.objects.end(),
            [&](const auto& item){return item.handle==world.handles[i];});
        const bool repair=flash_player_ && object!=scene.objects.end() && object->object.strategy_address==flash_player_;
        if(!player && !repair)continue;
        if(std::any_of(world.compute_models.begin(),world.compute_models.end(),
            [&](const auto& model){return model.packet_index==i;}))
            throw std::runtime_error("Cockpit player rig requires source triangle geometry");
        out.push_back(cockpit_ship_packet(world.packets[i],player && object!=scene.objects.end()
            ?damaged_extent(object->object.shape):std::nullopt));
        world.packets[i]=DrawPacket{};
    }
    return out;
}
}
