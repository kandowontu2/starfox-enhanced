// MODERN's per-frame scene planning, shared by the SDL_GPU flat renderer and
// the PCVR application (moved out of src/modern/sdl_frame.cpp).
#include "starfox/vr/modern_frame.hpp"
#include "starfox/compat/bit_cast.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/vr/presentation.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/timing/fixed_step.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
namespace starfox::vr {
namespace {
void require(bool ok,const std::string& what) {if(!ok) throw std::runtime_error(what);}
constexpr uint32_t surrounding_stars_key=0x20000U;
constexpr double pi=3.14159265358979323846;
// make_sky_vertices (background_tiles.cpp): 64 x 32 cells, two triangles each.
constexpr std::size_t sky_sphere_vertex_count=64*32*6;
// Source roll (65536 per turn) to screen rotation for landscape_world_model.
constexpr double roll_sign=-1;
// Source pitch to the landscape's geometric pitch.
constexpr double pitch_sign=-1;
// Wrapped 16-bit source word lerp (the desktop's interpolate_source_word).
int16_t interpolate_word(int16_t previous,int16_t current,double alpha) noexcept {
    const auto difference=int16_t(uint16_t(current)-uint16_t(previous));
    return int16_t(uint16_t(int32_t(previous)+int32_t(std::lround(double(difference)*alpha))));
}
// Remove one world packet (and renumber the compute placeholders after it).
void erase_world_packet(SourceModelPackets& world,std::size_t index,const char* what) {
    world.packets.erase(world.packets.begin()+std::ptrdiff_t(index));
    world.handles.erase(world.handles.begin()+std::ptrdiff_t(index));
    for(auto& compute:world.compute_models) {
        require(compute.packet_index!=index,std::string(what)+" unexpectedly owns a compute model");
        if(compute.packet_index>index) --compute.packet_index;
    }
}
// Flat views: turn the planet art with the camera's world yaw (see
// ModernFrame::sky_turn). STARFOX_MODERN_FIXED_SKY=0 keeps the cartridge's
// own pan, which follows its offset tables at a different rate and slides
// against the 3D ground.
bool world_fixed_sky() noexcept {
    static const bool fixed=[] {
        const char* value=std::getenv("STARFOX_MODERN_FIXED_SKY");
        return !(value && std::string_view(value)=="0");
    }();
    return fixed;
}
// Art columns per radian of yaw: the SNES focal length, the rate world
// objects at the screen centre move, where the planar art mapping matches it.
constexpr double sky_columns_per_radian=256;
bool gameplay_flow(const GameSceneSnapshot& scene) noexcept {
    return scene.flow==simulation::GameFlowState::gameplay || scene.flow==simulation::GameFlowState::training;
}
}
std::optional<std::array<double,2>> mode2_horizon_fit(const simulation::SnesPpuState& ppu) {
    if(ppu.background_mode!=2 || !ppu.bg2_vertical_offsets_enabled) return {};
    // The shader's tile_horizon, in double precision.
    double count=0,sum_x=0,sum_y=0,sum_xx=0,sum_xy=0;
    int previous_raw=0,previous_unwrapped=0;
    for(int i=0;i<32;++i) {
        const auto address=std::size_t((0x2fa0U+unsigned(i))*2U)&0xffffU;
        const unsigned value=ppu.vram[address]|(unsigned(ppu.vram[(address+1)&0xffffU])<<8U);
        if(!(value&0x4000U)) continue;
        const int raw=int(value&8191U);
        int delta=(raw-previous_raw)&8191;if(delta>4095) delta-=8192;
        const int unwrapped=count==0?raw:previous_unwrapped+delta;
        previous_raw=raw;previous_unwrapped=unwrapped;
        const double x=i+1,y=unwrapped;
        count+=1;sum_x+=x;sum_y+=y;sum_xx+=x*x;sum_xy+=x*y;
    }
    if(count==0) return {};
    const double denominator=count*sum_xx-sum_x*sum_x;
    const double slope=count>1 && denominator!=0?(count*sum_xy-sum_x*sum_y)/denominator:0;
    return std::array<double,2>{(sum_y-slope*sum_x)/count,slope};
}
// The background between two ticks. Discontinuities (new background, flow,
// mode or a camera cut) present the current tick unchanged.
BackgroundPresentation interpolated_background(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha) {
    BackgroundPresentation out;
    if(!current.ppu) return out;
    const auto& b=*current.ppu;
    const auto current_horizon=mode2_horizon_fit(b);
    if(current_horizon) out.mode2_horizon=std::array<float,2>{float((*current_horizon)[0]),float((*current_horizon)[1])};
    if(!previous.ppu || !(alpha>0 && alpha<1)) return out;
    const auto& a=*previous.ppu;
    if(previous.background_id!=current.background_id || previous.flow!=current.flow
        || a.background_mode!=b.background_mode || a.tunnel_scene!=b.tunnel_scene
        || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) return out;
    // The cartridge's pitch/roll on Mode 2 ground lives in the VRAM offset
    // table, updated at 20 Hz: move its fitted line at display rate.
    if(const auto previous_horizon=mode2_horizon_fit(a); previous_horizon && current_horizon) {
        const double intercept=(*previous_horizon)[0]
            +std::remainder((*current_horizon)[0]-(*previous_horizon)[0],8192.)*alpha;
        const double slope=std::lerp((*previous_horizon)[1],(*current_horizon)[1],alpha);
        out.mode2_horizon=std::array<float,2>{float(intercept-8192.*std::floor(intercept/8192.)),float(slope)};
    }
    const bool moving=a.bg1_scroll_x!=b.bg1_scroll_x || a.bg1_scroll_y!=b.bg1_scroll_y
        || a.bg2_scroll_x!=b.bg2_scroll_x || a.bg2_scroll_y!=b.bg2_scroll_y
        || a.bg3_scroll_x!=b.bg3_scroll_x || a.bg3_scroll_y!=b.bg3_scroll_y
        || a.bg2_horizontal_offsets!=b.bg2_horizontal_offsets || a.bg2_scanline_scroll_y!=b.bg2_scanline_scroll_y;
    if(!moving) return out;
    auto ppu=std::make_shared<simulation::SnesPpuState>(b);
    // Integer registers for the payload plus the remainder the shader adds
    // between source pixels (a 10x raster otherwise steps 10 pixels at once).
    const auto split=[alpha](int16_t from,int16_t to,uint16_t mask,int16_t& word,float& fraction) {
        const double value=timing::interpolate_fractional_scroll(uint16_t(from),uint16_t(to),alpha,mask);
        const double whole=std::floor(value);
        word=int16_t(uint16_t(uint32_t(whole)&mask));fraction=float(value-whole);
    };
    ppu->bg1_scroll_x=interpolate_word(a.bg1_scroll_x,b.bg1_scroll_x,alpha);
    ppu->bg1_scroll_y=interpolate_word(a.bg1_scroll_y,b.bg1_scroll_y,alpha);
    std::array<float,2> bg2{},bg3{};
    split(a.bg2_scroll_x,b.bg2_scroll_x,0xffffU,ppu->bg2_scroll_x,bg2[0]);
    // BG2's vertical phase wraps at 9 bits (CALCBGSCROLL).
    split(a.bg2_scroll_y,b.bg2_scroll_y,0x1ffU,ppu->bg2_scroll_y,bg2[1]);
    split(a.bg3_scroll_x,b.bg3_scroll_x,0xffffU,ppu->bg3_scroll_x,bg3[0]);
    split(a.bg3_scroll_y,b.bg3_scroll_y,0xffffU,ppu->bg3_scroll_y,bg3[1]);
    out.bg2_fraction=bg2;out.bg3_fraction=bg3;
    for(std::size_t row=0;row<ppu->bg2_horizontal_offsets.size();++row) {
        ppu->bg2_horizontal_offsets[row]=interpolate_word(a.bg2_horizontal_offsets[row],b.bg2_horizontal_offsets[row],alpha);
        // Tunnel scanline values are CHEQUERED page selectors (24/280), not
        // positions: blending them samples rows between the two pages.
        if(!b.tunnel_scene)
            ppu->bg2_scanline_scroll_y[row]=interpolate_word(a.bg2_scanline_scroll_y[row],b.bg2_scanline_scroll_y[row],alpha);
    }
    out.ppu=std::move(ppu);
    return out;
}
EyeCamera reversed_depth_camera(EyeCamera camera) noexcept {
    auto& m=camera.projection;
    // Infinite perspective (clip w = -z): write clip z = near instead of
    // -z - near, so normalized depth is near/distance (1 at the near plane).
    if(m[11]==-1 && m[15]==0 && m[10]==-1) {m[10]=0;m[14]=-m[14];}
    return camera;
}
std::vector<bool> painter_scenery(const SourceModelPackets& source,std::span<const uint16_t> moving) {
    std::vector<const SourceComputeModel*> requests(source.packets.size());
    for(const auto& request:source.compute_models)
        if(request.packet_index<requests.size()) requests[request.packet_index]=&request;
    std::vector<bool> painter(source.packets.size(),false);
    // The ships the camera follows: the player's and the zoom Arwing.
    std::vector<std::size_t> ships;
    for(std::size_t index=0;index<source.packets.size() && index<source.handles.size();++index)
        if(!is_source_shadow_pass(source.handles[index]) && ((source.player && source.handles[index]==source.player)
            || std::find(source.ships.begin(),source.ships.end(),uint16_t(source.handles[index]&0xffffU))!=source.ships.end()))
            ships.push_back(index);
    // Each object's centre and bounding radius in scene metres. Compute
    // models keep their placement in the projection pose (source units, Y
    // down, +Z forward; rows carry the vertex scale), not the packet matrix.
    struct Bounds {std::array<double,3> centre{};double radius{};};
    const auto bounds=[&](std::size_t index) {
        Bounds result;
        if(const auto* request=requests[index]) {
            const auto& poses=request->model.projection.continuous_poses;
            const double units=double(request->units);
            const auto& t=poses[0].translation;
            result.centre={double(t[0])/units,-double(t[1])/units,-double(t[2])/units};
            for(const auto& v:request->model.projection.continuous_vertices) {
                const auto& row=poses[std::min<std::size_t>(v.pose,1)].row0;
                const double scale=std::sqrt(double(row[0])*row[0]+double(row[1])*row[1]+double(row[2])*row[2]);
                result.radius=std::max(result.radius,std::sqrt(double(v.x)*v.x+double(v.y)*v.y+double(v.z)*v.z)*scale/units);
            }
        } else {
            const auto& model=source.packets[index].model;
            result.centre={double(model[12]),double(model[13]),double(model[14])};
            const double scale=std::sqrt(double(model[0])*model[0]+double(model[1])*model[1]+double(model[2])*model[2]);
            for(const auto& v:source.packets[index].geometry.vertex_view())
                result.radius=std::max(result.radius,scale*std::sqrt(double(v.position[0])*v.position[0]
                    +double(v.position[1])*v.position[1]+double(v.position[2])*v.position[2]));
        }
        return result;
    };
    const auto length=[](const std::array<double,3>& v){return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);};
    std::vector<bool> solid(source.packets.size(),false);
    // Bounding radius in model space times the placement scale.
    std::vector<double> reach(source.packets.size());
    for(std::size_t index=0;index<source.packets.size();++index) {
        const auto& model=source.packets[index].model;
        const double scale=std::sqrt(double(model[0])*model[0]+double(model[1])*model[1]+double(model[2])*model[2]);
        double radius=0;
        if(const auto* request=requests[index]) {
            for(const auto& v:request->model.projection.continuous_vertices)
                radius=std::max(radius,std::sqrt(double(v.x)*v.x+double(v.y)*v.y+double(v.z)*v.z)/double(request->units));
        } else for(const auto& v:source.packets[index].geometry.vertex_view())
            radius=std::max(radius,std::sqrt(double(v.position[0])*v.position[0]+double(v.position[1])*v.position[1]
                +double(v.position[2])*v.position[2]));
        reach[index]=radius*scale;
    }
    for(std::size_t index=0;index<source.packets.size() && index<source.handles.size();++index) {
        const auto& model=source.packets[index].model;
        const double distance=std::sqrt(double(model[12])*model[12]+double(model[13])*model[13]+double(model[14])*model[14]);
        // A camera-only test flips between frames as sections pass and swaps
        // winners. Moving objects (ships) never paint over scenery.
        const auto handle=uint16_t(source.handles[index]&0xffffU);
        // Line-only objects (cables, wires) have no faces to order and must
        // stay behind the walls they pass (Corneria's launch base).
        bool lines_only;
        if(const auto* request=requests[index])
            lines_only=std::all_of(request->model.faces.primitives.begin(),request->model.faces.primitives.end(),
                [](auto primitive){return primitive==render::PackedPrimitive::line;});
        else lines_only=source.packets[index].geometry.vertex_view().empty();
        painter[index]=!lines_only && (reach[index]>2. || reach[index]>distance)
            && handle!=0 && source.handles[index]<surrounding_stars_key
            && std::find(moving.begin(),moving.end(),handle)==moving.end();
        solid[index]=!lines_only && handle!=0 && source.handles[index]<surrounding_stars_key
            && !is_source_shadow_pass(source.handles[index]);
    }
    // The Super FX draws whole objects in its sorted draw list (view Z plus
    // the shape's and object's sort offsets, MDRAWLIS.MC) with no depth
    // buffer, the list this native order keeps. An object a followed ship's
    // bounds overlap that the cartridge draws before that ship (the Space
    // Armada carrier's mouth and the blocks inside it, whose sort offsets put
    // them behind it) stays under the ship. Paint both.
    for(const auto ship_index:ships) {
        const auto ship=bounds(ship_index);
        bool overlapped=false;
        for(std::size_t index=0;index<ship_index;++index) if(solid[index]) {
            const auto object=bounds(index);
            const double gap=length({object.centre[0]-ship.centre[0],object.centre[1]-ship.centre[1],object.centre[2]-ship.centre[2]});
            if(gap<object.radius+ship.radius) painter[index]=overlapped=true;
        }
        if(overlapped) painter[ship_index]=true;
    }
    // Parts on a hull (SHIP_5's three HOU_3 turrets) sit on or just inside
    // it; the cartridge draws them after the hull. Paint the pair in its order.
    for(const auto& [part,parent]:source.attachments) {
        std::optional<std::size_t> a,b;
        for(std::size_t index=0;index<source.packets.size() && index<source.handles.size();++index) if(solid[index]) {
            const auto handle=uint16_t(source.handles[index]&0xffffU);
            if(handle==part) a=index;else if(handle==parent) b=index;
        }
        if(!a || !b) continue;
        const auto x=bounds(*a),y=bounds(*b);
        const double gap=length({x.centre[0]-y.centre[0],x.centre[1]-y.centre[1],x.centre[2]-y.centre[2]});
        if(gap<x.radius+y.radius) painter[*a]=painter[*b]=true;
    }
    return painter;
}
void ModernFrame::reset() noexcept {
    stars.clear();star_keys.clear();world={};moving.clear();
    tunnel_surround.clear();sprites.clear();backgrounds.clear();background_models.clear();
    textures.clear();texture_models.clear();texture_mapping=landscape_heading=sky_turn=0;
    sky_yaw_.reset();sky_columns_=0;
    cockpit.reset();reticle.reset();circle.reset();colour_math.reset();shutter.reset();
    layers_rebuilt=backgrounds_rebuilt=textures_rebuilt=false;
    layer_revision_.reset();texture_background_.reset();level_horizon_.reset();horizon_votes_.clear();
    level_background_=0;background_interpolated_=false;
}
void ModernFrame::prepare(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,
    SourceModelPackets source,const FrameLayerInputs& inputs) {
    require(current.ppu!=nullptr,"MODERN frame needs a PPU snapshot");
    world=std::move(source);
    // The surrounding starfield is the distant environment, drawn first
    // and outside the world's depth (application.cpp does the same).
    stars.clear();star_keys.clear();
    if(const auto star=std::find(world.handles.begin(),world.handles.end(),surrounding_stars_key);star!=world.handles.end()) {
        const auto index=std::size_t(star-world.handles.begin());
        stars.push_back(world.packets[index]);star_keys.push_back(*star);
        erase_world_packet(world,index,"Star pass");
    }
    // The cartridge's ground dots (grid packet 0x30000) tie objects to the
    // ground; keep them over the 3D landscape ground unless switched off.
    const bool landscape_world=inputs.landscape_world && current.background_landscape && !current.ppu->tunnel_scene;
    // With dots on, the landscape ground draws them itself (anchored to
    // the 3D ground); the native lattice is dropped either way.
    // STARFOX_MODERN_NATIVE_DOTS=1 keeps the native lattice too (alignment check).
    static const bool native_dots=std::getenv("STARFOX_MODERN_NATIVE_DOTS")!=nullptr;
    if(landscape_world && !current.grid_lines && !native_dots)
        if(const auto grid=std::find(world.handles.begin(),world.handles.end(),0x30000U);grid!=world.handles.end())
            erase_world_packet(world,std::size_t(grid-world.handles.begin()),"Grid pass");
    // Twisting pipe sections (SourceModelPackets::depth_enclosures): keep
    // their draw-list slots but fill them far to near by real distance, so
    // the painter puts the section around the camera last.
    {
        std::vector<std::size_t> slots;std::vector<std::pair<double,std::size_t>> order;
        std::vector<std::size_t> compute(world.packets.size(),SIZE_MAX);
        for(std::size_t i=0;i<world.compute_models.size();++i)
            if(world.compute_models[i].packet_index<compute.size()) compute[world.compute_models[i].packet_index]=i;
        for(std::size_t index=0;index<world.packets.size() && index<world.handles.size();++index) {
            const auto handle=world.handles[index];
            if(is_source_shadow_pass(handle) || std::find(world.depth_enclosures.begin(),world.depth_enclosures.end(),
                uint16_t(handle&0xffffU))==world.depth_enclosures.end()) continue;
            std::array<double,3> centre{};
            if(compute[index]!=SIZE_MAX) {
                const auto& request=world.compute_models[compute[index]];
                const auto& t=request.model.projection.continuous_poses[0].translation;
                centre={double(t[0]),double(t[1]),double(t[2])};
            } else {
                const auto& model=world.packets[index].model;
                centre={double(model[12]),double(model[13]),double(model[14])};
            }
            slots.push_back(index);order.push_back({std::sqrt(centre[0]*centre[0]+centre[1]*centre[1]+centre[2]*centre[2]),index});
        }
        if(slots.size()>1) {
            std::stable_sort(order.begin(),order.end(),[](const auto& a,const auto& b){return a.first>b.first;});
            auto packets=world.packets;auto handles=world.handles;
            for(std::size_t k=0;k<slots.size();++k) {
                const auto from=order[k].second,to=slots[k];
                world.packets[to]=packets[from];world.handles[to]=handles[from];
                if(compute[from]!=SIZE_MAX) world.compute_models[compute[from]].packet_index=to;
            }
        }
    }
    // The cartridge paints objects in its own sorted list order (the native
    // draw list), so where two objects share a surface the later one
    // wins. With a depth buffer those surfaces z-fight. Scale each
    // placement about the eye (the flat camera's view is the identity) by
    // a tiny step per list position: screen positions are unchanged,
    // later objects move nearer by a relative step that exceeds
    // plane-reconstruction rounding and the shader's line/decal biases,
    // and stays far below visible interpenetration. Objects enclosing the
    // camera are drawn as a painter instead (painter scenery in the renderers).
    constexpr float draw_order_step=3.2e-5F;
    for(std::size_t i=0;i<world.packets.size();++i) {
        const float scale=1.F-draw_order_step*float(std::min<std::size_t>(i,1024));
        auto& model=world.packets[i].model;
        for(unsigned column=0;column<4;++column) for(unsigned row=0;row<3;++row) model[column*4+row]*=scale;
    }
    // The PCVR seat: gameplay objects sit further away than the source camera
    // (application.cpp); the ground and corridor below move with them.
    const bool setback=settings_.headset && settings_.seat_setback!=0 && gameplay_flow(current);
    if(setback)
        for(std::size_t i=0;i<world.packets.size();++i)
            if((world.handles[i]&0xffffU)!=0 && world.handles[i]<surrounding_stars_key)
                world.packets[i].model[14]-=settings_.seat_setback;
    Matrix4 seat=identity_matrix;
    if(setback) seat[14]=-settings_.seat_setback;
    // 2D layers change only with the source snapshot (20 Hz).
    layers_rebuilt=!layer_revision_ || *layer_revision_!=current.revision;
    if(layers_rebuilt) {
        auto layers=assemble_frame_layers(current,inputs);
        tunnel_surround=std::move(layers.tunnel_surround);sprites=std::move(layers.sprites);
        layer_revision_=current.revision;
    }
    // Backgrounds follow the source's scroll registers and per-scanline
    // offset tables (how the cartridge fakes roll and turns). Interpolate
    // them at display rate, as the desktop renderer does, instead of
    // stepping at 20 Hz.
    auto presentation=interpolated_background(previous,current,alpha);
    if(current.background_scroll_override) presentation.bg2_fraction.reset();
    const bool interpolated=presentation.ppu || presentation.mode2_horizon;
    backgrounds_rebuilt=layers_rebuilt || interpolated || background_interpolated_;
    if(backgrounds_rebuilt) {
        auto scene=current;
        if(presentation.ppu) scene.ppu=presentation.ppu;
        auto background_inputs=inputs;background_inputs.backgrounds_only=true;
        background_inputs.bg2_scroll_fraction=presentation.bg2_fraction;
        background_inputs.bg3_scroll_fraction=presentation.bg3_fraction;
        background_inputs.mode2_horizon=presentation.mode2_horizon;
        // Level horizon: the art row the cartridge puts on the true horizon,
        // i.e. its art horizon with the pitch shift (-tan(pitch) x 256 rows)
        // removed. Cinematics move the art without matching the camera
        // (Corneria's launch exit and clear), so take the most common value
        // over near-level ticks of this background rather than any one tick.
        if(inputs.landscape_world && scene.background_landscape && scene.ppu) {
            const float art=landscape_art_horizon(*scene.ppu,float(scene.source_vanishing_point[1])+16.F);
            const double pitch=double(int16_t(scene.camera.pitch))*(2*pi/65536.);
            if(level_background_!=scene.background_id || !level_horizon_) {
                horizon_votes_.clear();level_background_=scene.background_id;
                level_horizon_=float(art+std::tan(pitch)*256.);
            }
            if(layers_rebuilt && std::abs(int(int16_t(scene.camera.pitch)))<=1024) {
                const int key=int(std::lround((art+std::tan(pitch)*256.)*4.));
                const auto votes=++horizon_votes_[key];
                if(votes>=horizon_votes_[int(std::lround(*level_horizon_*4.F))]) level_horizon_=float(key)/4.F;
            }
            background_inputs.landscape_horizon=level_horizon_;
            // Venom haze colour from this background's own sky art: half
            // its mean, half its brightest third (the glowing clouds). Once
            // per background, retried until the level has loaded the art.
            if(fog_background_!=scene.background_id) {
                fog_colour_.reset();
                bool venom=false;
                if(inputs.symbols) {
                    const auto& lists=inputs.symbols->find("BGLISTS");
                    for(const char* name:{"BG_1_6A","BG_3_7A"})
                        for(const auto value:inputs.symbols->find(name))
                            if(!lists.empty() && value>lists.front() && (value&0xff0000U)==(lists.front()&0xff0000U)
                                && uint16_t(value-lists.front())==scene.background_id) venom=true;
                }
                if(venom) {
                    // Every opaque pixel of the BG2 sky art (tile map ->
                    // 4bpp characters -> CGRAM); 16-pixel tiles use their
                    // four characters.
                    const auto& ppu=*scene.ppu;
                    const auto palette=render::decode_bgr555_palette(ppu.cgram);
                    const auto word=[&](uint32_t address) {
                        const auto offset=(address&0x7fffU)*2U;
                        return uint16_t(ppu.vram[offset]|(ppu.vram[offset+1U]<<8U));
                    };
                    const unsigned entries=((ppu.bg2_screen_size&1U)?2U:1U)*((ppu.bg2_screen_size&2U)?2U:1U)*1024U;
                    std::vector<std::array<float,4>> band; // r, g, b, luminance
                    for(unsigned entry=0;entry<entries;++entry) {
                        const auto tile=word(ppu.bg2_screen_base+entry);
                        const unsigned characters=ppu.bg2_tile_size_16?4U:1U;
                        for(unsigned part=0;part<characters;++part) {
                            const uint32_t number=((tile&0x3ffU)+(part&1U)+(part>>1)*16U)&0x3ffU;
                            for(unsigned y=0;y<8;++y) {
                                const auto base=(uint32_t(ppu.bg2_character_base)*2U+number*32U+y*2U)&0xffffU;
                                const unsigned p0=ppu.vram[base],p1=ppu.vram[(base+1U)&0xffffU];
                                const unsigned p2=ppu.vram[(base+16U)&0xffffU],p3=ppu.vram[(base+17U)&0xffffU];
                                for(unsigned x=0;x<8;++x) {
                                    const unsigned mask=0x80U>>x;
                                    const unsigned ink=((p0&mask)?1U:0U)|((p1&mask)?2U:0U)|((p2&mask)?4U:0U)|((p3&mask)?8U:0U);
                                    if(!ink) continue;
                                    const auto& c=palette[((tile>>10)&7U)*16U+ink];
                                    band.push_back({float(c.r),float(c.g),float(c.b),.2126F*c.r+.7152F*c.g+.0722F*c.b});
                                }
                            }
                        }
                    }
                    if(band.size()>256) {
                        std::sort(band.begin(),band.end(),[](const auto& a,const auto& b){return a[3]>b[3];});
                        std::array<float,3> mean{},bright{};
                        const std::size_t top=band.size()/3;
                        for(std::size_t i=0;i<band.size();++i) for(unsigned c=0;c<3;++c) {
                            mean[c]+=band[i][c]/float(band.size());
                            if(i<top) bright[c]+=band[i][c]/float(top);
                        }
                        fog_colour_=std::array<float,3>{(mean[0]+bright[0])/510.F,(mean[1]+bright[1])/510.F,(mean[2]+bright[2])/510.F};
                    }
                }
                if(!venom || fog_colour_) fog_background_=scene.background_id;
            }
            static const bool trace_horizon=std::getenv("STARFOX_TRACE_MODERN")!=nullptr;
            if(trace_horizon && layers_rebuilt) std::fprintf(stderr,"horizon-level tick=%llu level=%.2f art=%.2f implied=%.2f pitch=%d camy=%d shadow=%d vp=%d\n",
                static_cast<unsigned long long>(scene.revision),*level_horizon_,art,float(art+std::tan(pitch)*256.),
                int(int16_t(scene.camera.pitch)),int(scene.camera.y),int(scene.shadow_height),int(scene.source_vanishing_point[1]));
        }
        backgrounds=assemble_frame_layers(scene,background_inputs).backgrounds;
        // Headset views look all round: planet art by direction, corridor rows by
        // distance along the corridor.
        if(settings_.headset)
            for(auto& packet:backgrounds)
                if(is_landscape_world(packet) || is_landscape_ground(packet) || is_tunnel_corridor(packet)) set_headset_mapping(packet);
        background_interpolated_=interpolated;
    }
    fog.reset();
    if(fog_colour_ && !settings_.headset && landscape_world && fog_background_==current.background_id
        && gameplay_flow(current)) {
        static const auto distances=[] {
            std::array<float,2> d{1500.F,3300.F};
            if(const char* text=std::getenv("STARFOX_MODERN_FOG")) std::sscanf(text,"%f,%f",&d[0],&d[1]);
            return d;
        }();
        Fog haze;haze.start=distances[0];haze.end=distances[1];
        const float fade=float(std::min(unsigned(current.display_brightness),15U))/15.F;
        for(unsigned c=0;c<3;++c) {
            float v=(*fog_colour_)[c]*fade;
            if(inputs.srgb) v=v<=.04045F?v/12.92F:std::pow((v+.055F)/1.055F,2.4F);
            haze.colour[c]=v;
        }
        haze.colour[3]=haze.end>haze.start?1.F:0.F;
        if(haze.colour[3]>0) fog=haze;
    }
    static const bool trace=std::getenv("STARFOX_TRACE_MODERN")!=nullptr;
    if(trace) std::fprintf(stderr,"modern-frame tick=%llu alpha=%.3f bgid=%x corridor=%d nucleus=%d background=%s vp=%d,%d bg2=%d,%d vscroll=%d grid=%d atlas=%u hudrot=%04x mode=%u flags=%s%s%s%s%s%s roll=%d math=%d:%02x:%u,%u,%u%s%s bright=%u\n",
        static_cast<unsigned long long>(current.revision),alpha,unsigned(current.background_id),int(current.background_corridor.has_value()),int(current.background_nucleus.has_value()),
        presentation.ppu?"interpolated":layers_rebuilt?"tick":"retained",
        int(current.source_vanishing_point[0]),int(current.source_vanishing_point[1]),
        int(current.ppu->bg2_scroll_x),int(current.ppu->bg2_scroll_y),
        int(current.background_vertical_scroll),int(current.landscape_grid_height),unsigned(current.landscape_atlas_origin),
        unsigned(current.cockpit_hud_roll),unsigned(current.ppu->background_mode),
        current.background_star_sphere?"star_sphere ":"",current.background_space_horizon?"space_horizon ":"",
        current.background_unique_space?"unique_space ":"",current.background_landscape?"landscape ":"",
        current.ppu->tunnel_scene?"tunnel ":"",current.background_orbital_planet?"orbital ":"",
        int(int16_t(current.camera.roll)),int(current.colour_math.active),unsigned(current.colour_math.affected_layers),
        unsigned(current.colour_math.red),unsigned(current.colour_math.green),unsigned(current.colour_math.blue),
        current.colour_math.subtract?" sub":"",current.colour_math.half?" half":"",unsigned(current.display_brightness));
    if(trace && layers_rebuilt && current.background_landscape) {
        const auto fit=mode2_horizon_fit(*current.ppu);
        std::fprintf(stderr,"landscape world=%d vofs=%d hofs=%d flow=%d bg=%04x mode=%u shadow=%d dots=%d camera=%d,%d,%d pyr=%u,%u,%u scroll=%d,%d horizon=%.2f,%.4f grid=%d origin=%u\n",
            int(landscape_world),int(current.ppu->bg2_scanline_scroll_enabled),int(current.ppu->bg2_horizontal_offsets_enabled),
            int(current.flow),unsigned(current.background_id),unsigned(current.ppu->background_mode),int(current.shadow_height),int(current.dots_mode),
            int(current.camera.x),int(current.camera.y),int(current.camera.z),
            unsigned(current.camera.pitch),unsigned(current.camera.yaw),unsigned(current.camera.roll),
            int(current.ppu->bg2_scroll_x),int(current.ppu->bg2_scroll_y),fit?(*fit)[0]:0.,fit?(*fit)[1]:0.,
            int(current.landscape_grid_height),unsigned(current.landscape_atlas_origin));
    }
    if(trace && layers_rebuilt && current.ppu->tunnel_scene && current.ppu->bg2_scanline_scroll_enabled) {
        std::fprintf(stderr,"tunnel-table camz=%d camx=%d scrollx=%d hofs=%d:%d flips=",int(current.camera.z),int(current.camera.x),
            int(current.ppu->bg2_scroll_x),int(current.ppu->bg2_horizontal_offsets_enabled),int(current.ppu->bg2_horizontal_offsets[60]));
        for(std::size_t row=1;row<224;++row)
            if(current.ppu->bg2_scanline_scroll_y[row]!=current.ppu->bg2_scanline_scroll_y[row-1])
                std::fprintf(stderr,"%zu:%d,",row,int(current.ppu->bg2_scanline_scroll_y[row]));
        std::fprintf(stderr,"\n");
    }
    // Display-rate background motion (landscape pitch, water height, ...).
    background_models=frame_background_models(backgrounds,previous,current,alpha,inputs);
    // VR's sky spheres (and the landscape ground receiver built on the
    // same mesh) sample the source at 512 pixels per radian / unit
    // tangent, half the SNES camera's focal length of 256, so a flat view
    // showed mountains at half size over a mismatched ground. The shader
    // derives that mapping from model-space positions, so widening the
    // sphere 2x in X and Y (relative to Z) restores the SNES scale: exact
    // vertically at the centre column, close to it horizontally. Headsets
    // keep VR's own scale.
    std::size_t model_index=0;
    if(!settings_.headset)
        for(const auto& packet:backgrounds) {
            const auto vertices=packet.geometry.vertex_view();
            if(vertices.empty() && packet.geometry.line_view().empty()) continue;
            if(vertices.size()==sky_sphere_vertex_count && (vertices.front().texture[3]&8U)) {
                auto& model=background_models.at(model_index);
                for(unsigned row=0;row<4;++row) {model[0*4+row]*=2.F;model[1*4+row]*=2.F;}
            }
            ++model_index;
        }
    // Landscape placement at display rate: roll as geometry, camera height
    // above the ground, and the camera's world X/Z for the ground plane.
    struct {double roll{},pitch{},height{1},x{},z{};Matrix4 rotation{};} landscape;
    {
        const bool continuous=!timing::camera_transform_is_discontinuous(previous.camera,current.camera);
        const double a=continuous?alpha:1.;
        const double roll_delta=double(int16_t(uint16_t(current.camera.roll-previous.camera.roll)));
        landscape.roll=roll_sign*(double(previous.camera.roll)+roll_delta*a)*(2*pi/65536.);
        const double pitch_delta=double(int16_t(uint16_t(current.camera.pitch-previous.camera.pitch)));
        double pitch=double(int16_t(previous.camera.pitch))+pitch_delta*a;
        pitch-=65536.*std::floor((pitch+32768.)/65536.);
        landscape.pitch=pitch_sign*pitch*(2*pi/65536.);
        // The source view rotation the objects are drawn with (yaw
        // included), interpolated like the grid, in model axes.
        const auto view=landscape_scene_view(continuous?previous:current,current,a);
        constexpr int sign[]{1,-1,-1};
        landscape.rotation[15]=1;
        for(unsigned column=0;column<3;++column) for(unsigned row=0;row<3;++row)
            landscape.rotation[column*4+row]=float(view[column*3+row])*float(sign[column]*sign[row])/32768.F;
        landscape.height=std::lerp(double(previous.shadow_height)-previous.camera.y,
            double(current.shadow_height)-current.camera.y,a);
        const auto blend=[&](int32_t before,int32_t now) {
            return double(before)+double(int16_t(uint16_t(now-before)))*a;
        };
        landscape.x=continuous?blend(previous.camera.x,current.camera.x):double(current.camera.x);
        landscape.z=continuous?blend(previous.camera.z,current.camera.z):double(current.camera.z);
    }
    // The source camera's heading in the ground plane's frame (the model's
    // third row is the camera's back axis): a headset's own view would turn
    // the art with the head.
    {
        const double x=-landscape.rotation[2],z=-landscape.rotation[10];
        const double turn=std::hypot(x,z)>1e-6?std::atan2(z,x)/(2*pi):0.;
        landscape_heading=(uint32_t(std::lround((turn-std::floor(turn))*65536.))&0xffffU)|0x10000U;
        sky_turn=0;
        if(!settings_.headset && world_fixed_sky() && landscape_world && std::hypot(x,z)>1e-6) {
            const double yaw=std::atan2(z,x);
            if(sky_yaw_) sky_columns_+=std::remainder(yaw-*sky_yaw_,2*pi)*sky_columns_per_radian;
            sky_yaw_=yaw;
            // 4096 is a whole number of every BG2 art width (256-1024).
            sky_columns_=std::fmod(sky_columns_,4096.);
            const double offset=sky_columns_<0?sky_columns_+4096.:sky_columns_;
            sky_turn=(uint32_t(std::lround(offset*16.))&0xffffU)|0x20000U;
        } else sky_yaw_.reset();
    }
    // Space backgrounds: the real camera roll as geometry around the view
    // axis (the vanishing point), like the planet landscape, instead of
    // the cartridge's 20 Hz Mode 2 column shear.
    if(inputs.space_roll_geometry && space_background_roll(current)) {
        const auto roll=landscape_world_model(landscape.roll,0,256.);
        for(auto& model:background_models) model=multiply_matrix(roll,model);
    }
    // The landscape dome, ground plane and 3D tunnel corridor follow the
    // display-rate camera.
    model_index=0;
    for(const auto& packet:backgrounds) {
        if(packet.geometry.vertex_view().empty() && packet.geometry.line_view().empty()) continue;
        auto& model=background_models.at(model_index);
        if(is_landscape_world(packet)) {
            model=multiply_matrix(seat,landscape_dome_view_model(landscape.rotation,landscape.height));
            // Sky replacement mapping for this frame: horizon row and
            // centre column, 32x fixed point (camera.effects.z).
            const auto texels=packet.geometry.texel_view();
            const float horizon=starfox::bit_cast<float>(texels[272+16384]);
            const float centre=starfox::bit_cast<float>(texels[272+16384+1])+(sky_turn?float(sky_turn&0xffffU)/16.F:0.F);
            texture_mapping=uint32_t(std::clamp(horizon*32.F,0.F,65535.F))
                |(uint32_t(std::clamp(std::fmod(centre,1024.F)+1024.F,0.F,2047.F)*32.F)<<16);
            if(observe_landscape_ && texels.size()>=272+16384+6+112)
                observe_landscape_(current.background_id,*current.ppu,horizon,texels.subspan(272+16384+6,112));
        }
        if(is_landscape_ground(packet))
            model=multiply_matrix(seat,landscape_ground_view_model(landscape.rotation,landscape.height,landscape.x,landscape.z));
        if(is_tunnel_corridor(packet)) {
            const bool continuous=!timing::camera_transform_is_discontinuous(previous.camera,current.camera);
            const auto blend=[&](int32_t before,int32_t now) {
                const double from=continuous?double(before):double(now);
                return from+double(int16_t(uint16_t(now-int32_t(from))))*(continuous?alpha:1.);
            };
            // The authored box's middle (MTUNNEL: x 0, y -60).
            double centre_x=0,centre_y=-60;
            if(const auto& bounds=current.background_corridor; bounds && bounds->right>bounds->left && bounds->bottom>bounds->top) {
                centre_x=(double(bounds->left)+bounds->right)*.5;centre_y=(double(bounds->top)+bounds->bottom)*.5;
            }
            model=multiply_matrix(seat,tunnel_corridor_model(blend(previous.camera.z,current.camera.z),
                blend(previous.camera.x,current.camera.x),blend(previous.camera.y,current.camera.y),centre_y,centre_x));
        }
        if((is_nucleus_room(packet) || is_nucleus_floor(packet)) && current.background_nucleus) {
            const bool continuous=!timing::camera_transform_is_discontinuous(previous.camera,current.camera);
            const auto blend=[&](int32_t before,int32_t now) {
                const double from=continuous?double(before):double(now);
                return from+double(int16_t(uint16_t(now-int32_t(from))))*(continuous?alpha:1.);
            };
            // The ring moves with PLAYER_POSZ, as the panels on it do.
            const int32_t centre=current.background_nucleus->centre_z;
            const int32_t before=previous.background_nucleus?previous.background_nucleus->centre_z:centre;
            model=multiply_matrix(seat,nucleus_room_model(blend(previous.camera.x,current.camera.x),
                blend(previous.camera.y,current.camera.y),blend(previous.camera.z,current.camera.z),blend(before,centre)));
        }
        ++model_index;
    }
    // The engine glow on the landscape ground (set_engine_glow): a round
    // pool soft from its centre, like a laser's coloured light on the
    // ground, about the ship's width across, from the engine at the tail of
    // the ship's shadow; smaller and dimmer as the ship climbs, as a point
    // light's pool would be.
    engine_glow.reset();
    if(engine_glow_settings_.enabled && landscape_world && current.shadows_enabled && world.player)
        for(const auto& request:world.compute_models) {
            if(request.key!=world.player || !(request.units>0)) continue;
            const auto& projection=request.model.projection;
            if(projection.continuous_poses.empty()) break;
            const double units=double(request.units);
            // Pose: source camera space (Y down, +Z forward), model units.
            const auto& pose=projection.continuous_poses[0];
            double half_span=0;
            for(const auto& vertex:projection.continuous_vertices) {
                const auto& p=projection.continuous_poses[std::min<std::size_t>(vertex.pose,projection.continuous_poses.size()-1)];
                const double column=std::sqrt(double(p.row0[0])*p.row0[0]+double(p.row1[0])*p.row1[0]+double(p.row2[0])*p.row2[0]);
                half_span=std::max(half_span,std::abs(double(vertex.x))*column/units);
            }
            using V=std::array<double,3>;
            const auto dot=[](const V& a,const V& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
            const auto unit=[&](V v){const double l=std::sqrt(dot(v,v));if(l>1e-9) for(auto& c:v) c/=l;return v;};
            // Scene metres (+Y up, -Z forward).
            const V forward=unit({double(pose.row0[2]),-double(pose.row1[2]),-double(pose.row2[2])});
            const double back=half_span*.3;
            const V engine{double(pose.translation[0])/units-forward[0]*back,-double(pose.translation[1])/units-forward[1]*back,
                -double(pose.translation[2])/units-forward[2]*back};
            const auto ground=landscape_ground_view_model(landscape.rotation,landscape.height,landscape.x,landscape.z);
            const V up=unit({ground[4],ground[5],ground[6]});
            const V origin{ground[12],ground[13],ground[14]};
            const double height=dot({engine[0]-origin[0],engine[1]-origin[1],engine[2]-origin[2]},up);
            // Gone by about two wingspans up (4.5 half spans), as on the
            // flat view's traced glow.
            const double reach=half_span*4.5;
            if(!(height>0 && height<reach) || !(half_span>0)) break;
            V toward=up;
            if(const auto& sun=engine_glow_settings_.toward_light) {
                const V scene=unit({double((*sun)[0]),double((*sun)[1]),double((*sun)[2])});
                if(dot(scene,up)>.1) toward=scene;
            }
            const double run=height/dot(toward,up);
            // A source unit above the ground, under every object.
            V centre;
            for(unsigned c=0;c<3;++c) centre[c]=engine[c]-toward[c]*run+up[c]/256.;
            V along=unit({forward[0]-up[0]*dot(forward,up),forward[1]-up[1]*dot(forward,up),forward[2]-up[2]*dot(forward,up)});
            if(dot(along,along)<.5) break;
            const V across{up[1]*along[2]-up[2]*along[1],up[2]*along[0]-up[0]*along[2],up[0]*along[1]-up[1]*along[0]};
            // Three times as wide at the top, nearly full while low.
            const double climb=height/reach;
            const double radius=half_span*2.*(1.+2.*climb);
            const double fall=1.-climb*climb*(3.-2.*climb);
            // Red and yellow on alternate pairs of ticks (the cartridge's flicker).
            const bool red=(current.revision/2U)%2U==0U;
            const std::array<float,3> tint=red?std::array<float,3>{1.F,.35F,.1F}:std::array<float,3>{1.F,.75F,.2F};
            const float strength=float(.5*fall);
            DrawPacket packet;
            const std::array<std::array<float,2>,4> corners{{{-1,-1},{1,-1},{1,1},{-1,1}}};
            for(const unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex vertex{};
                const auto [u,v]=corners[corner];
                for(unsigned c=0;c<3;++c)
                    vertex.position[c]=float(centre[c]+along[c]*radius*v+across[c]*radius*u);
                for(unsigned c=0;c<3;++c) vertex.color[c]=tint[c]*strength;
                vertex.color[3]=1;
                vertex.uv[0]=u;vertex.uv[1]=v;
                // A round decal (4096) soft from its centre (texture.y = 1:
                // edge start 0, scene.hlsl), biased onto the ground.
                vertex.texture[1]=1U;vertex.texture[3]=4096U|32768U;
                packet.geometry.vertices.push_back(vertex);
            }
            packet.model=seat;
            engine_glow=Overlay{std::move(packet),SceneBlend::add};
            break;
        }
    // Replacement landscape artwork, loaded once per background.
    textures_rebuilt=false;
    if(landscape_world && load_image_ && texture_background_!=current.background_id) {
        texture_background_=current.background_id;
        textures.clear();textures_rebuilt=true;
        const auto& ppu=*current.ppu;
        const float art_width=float(((ppu.bg2_screen_size&1U)?64U:32U)*(ppu.bg2_tile_size_16?16U:8U));
        char name[32];
        for(const auto kind:{LandscapeTexture::sky,LandscapeTexture::ground}) {
            std::snprintf(name,sizeof name,"bg_%04x_%s.png",unsigned(current.background_id),
                kind==LandscapeTexture::sky?"sky":"ground");
            if(auto image=load_image_(name)) {
                textures.push_back(landscape_texture_packet(kind,image->pixels,image->width,image->height,
                    art_width,112.F,2048.F,inputs.srgb));
                if(settings_.headset) set_headset_mapping(textures.back());
            }
        }
    }
    if(!landscape_world && texture_background_) {
        texture_background_.reset();textures.clear();textures_rebuilt=true;
    }
    texture_models.clear();
    for(const auto& packet:textures)
        texture_models.push_back(multiply_matrix(seat,is_landscape_texture(packet,LandscapeTexture::ground)
            ?landscape_ground_view_model(landscape.rotation,landscape.height,landscape.x,landscape.z)
            :landscape_dome_view_model(landscape.rotation,landscape.height)));
    // MHUD.MC (INIT_STRATS enables it only inside the cockpit): the desktop
    // draws it per frame with interpolated roll, above world models and
    // below the SNES OBJ HUD.
    cockpit.reset();
    if(gameplay_flow(current) && inputs.trigonometry && (current.cockpit_hud_roll&0x8000U)!=0) {
        auto hud=source_cockpit_hud_packet(*inputs.trigonometry,
            timing::interpolate_cockpit_roll(previous.cockpit_hud_roll,current.cockpit_hud_roll,alpha),
            current.cockpit_hud_colour,current.cockpit_hud_flags,current.ppu->cgram,current.display_brightness,inputs.srgb);
        hud.model=source_ui_layer_matrix(float(current.source_vanishing_point[0]),
            float(current.source_vanishing_point[1]),false,true).value();
        cockpit=std::move(hud);
    }
    // The first-person reticle as geometry, moving at display rate.
    reticle.reset();
    if(inputs.vector_reticle && previous.ppu) {
        auto packet=source_vector_reticle_packet(*previous.ppu,*current.ppu,alpha,current.display_brightness,inputs.srgb);
        if(!packet.geometry.vertex_view().empty()) {
            packet.model=source_ui_layer_matrix(float(current.source_vanishing_point[0]),
                float(current.source_vanishing_point[1]),false).value();
            // Gameplay leaves CGSWSEL %10 / CGADSUB %01010000 (IRQ.ASM
            // setwindowpri .noreq): OBJ palettes 4-7 are averaged with the
            // subscreen, so retail's reticle is see-through. A window
            // request (current.colour_math) replaces that setting.
            auto blend=SceneBlend::opaque;
            if(!current.colour_math.active && packet.geometry.vertices.front().texture[1]>=128U+4U*16U) {
                blend=SceneBlend::half_add;
                for(auto& colour:packet.geometry.texels) if(colour>>24) colour=(colour&0xffffffU)|0x80000000U;
            }
            reticle=Overlay{std::move(packet),blend,true};
        }
    }
    circle.reset();
    if(auto packet=source_circle_packet(previous.circle,current.circle,alpha,current.display_brightness,inputs.srgb);
        !packet.geometry.vertices.empty()) {
        const auto flags=current.circle.affected_layers;
        packet.model=source_layer_matrix(float(current.source_vanishing_point[0])+16.F,
            float(current.source_vanishing_point[1])+16.F).value();
        circle=Overlay{std::move(packet),(flags&0x80)?((flags&0x40)?SceneBlend::half_subtract:SceneBlend::subtract)
            :((flags&0x40)?SceneBlend::half_add:SceneBlend::add),(flags&0x10)!=0};
    }
    // Fixed-colour math (the turquoise/red damage flash and scripted
    // full-screen flashes): the whole frame below OBJ unless OBJ is included.
    colour_math.reset();
    if(auto packet=source_colour_math_packet(current.colour_math,inputs.srgb);!packet.geometry.vertices.empty()) {
        const auto& effect=current.colour_math;
        colour_math=Overlay{std::move(packet),effect.subtract?(effect.half?SceneBlend::half_subtract:SceneBlend::subtract)
            :(effect.half?SceneBlend::half_add:SceneBlend::add),(effect.affected_layers&0x10U)!=0};
    }
    shutter.reset();
    if(auto packet=source_shutter_packet(previous.wipe,current.wipe,alpha);!packet.geometry.vertices.empty())
        shutter=std::move(packet);
    // Objects that moved in the world since the previous tick.
    moving.clear();
    for(const auto& object:current.objects) {
        const auto before=std::find_if(previous.objects.begin(),previous.objects.end(),
            [&](const auto& candidate){return candidate.handle==object.handle;});
        if(before!=previous.objects.end() && (before->object.world_x!=object.object.world_x
            || before->object.world_y!=object.object.world_y || before->object.world_z!=object.object.world_z))
            moving.push_back(uint16_t(object.handle));
    }
}
}
