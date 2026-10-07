#include "starfox/platform/nintendo_3ds/game_models.hpp"
#include "starfox/compat/bit_cast.hpp"

namespace starfox::platform::nintendo_3ds {
namespace {
constexpr std::array<double,2> source_origin{112,96}; // Canonical FX viewport, before its 16px guard.
constexpr std::size_t cache_budget=4*1024*1024,cache_entries=128;
std::size_t shape_bytes(const assets::Shape& s) {
    const auto bytes=[](const auto& values) {return values.capacity()*sizeof(values[0]);};
    const auto points=[&](const auto& blocks) {
        std::size_t n=bytes(blocks);for(const auto& block:blocks) n+=bytes(block.source_points);return n;
    };
    const auto faces=[&](const auto& list) {
        std::size_t n=bytes(list);for(const auto& face:list) n+=bytes(face.vertex_indices);return n;
    };
    std::size_t n=sizeof(s)+s.name.capacity()+points(s.point_blocks)+bytes(s.vertices)
        +(s.word_coordinates.capacity()+7)/8+bytes(s.frames)+bytes(s.visibilities)+faces(s.faces)
        +bytes(s.face_batches)+bytes(s.bsp_nodes)+bytes(s.bsp_leaves)+bytes(s.colour_words)
        +bytes(s.colour_materials)+bytes(s.textures);
    for(const auto& frame:s.frames) n+=points(frame.point_blocks)+bytes(frame.vertices)+(frame.word_coordinates.capacity()+7)/8;
    for(const auto& batch:s.face_batches) n+=faces(batch.faces);
    for(const auto& material:s.colour_materials) n+=bytes(material.animation_frames);
    for(const auto& art:s.textures) n+=bytes(art.texels);
    return n;
}
double interpolate_word(std::int16_t a,std::int16_t b,double alpha) {
    auto difference=std::int32_t(b)-a;
    if(difference>32767) difference-=65536;else if(difference< -32768) difference+=65536;
    return a+alpha*difference;
}
render::PreparedShapePrimitives auxiliary(const render::RenderPose& pose) {
    render::PreparedShapePrimitives result;result.pose=pose;result.colour_index_base=112;result.focal_length=256;
    // MPART and projected text use their own source routines, not MDRAWP's
    // object-global alternate solid-polygon scan converter.
    result.pose.wireframe_mode=result.pose.wobble_mode=0;
    result.pose.cel_mode=result.pose.wave_mode=false;
    // MPART/MDSPRITE use the bitmap centre, not EX menu's model vanishing point.
    result.pose.vanish_x=source_origin[0];result.pose.vanish_y=source_origin[1];return result;
}
}
GameModels::GameModels(const assets::RomImage& rom,const assets::SymbolMap& symbols)
    :decoder_(rom,symbols),renderer_([] {render::RenderSettings s;s.colour_index_base=112;return s;}()),text_(rom,symbols) {
    const auto symbol=[&](const char* name) {const auto& list=symbols.find(name);return list.empty()?std::uint32_t{}:list.front();};
    rules_.trail=symbol("TRAIL_ISTRAT");if(rules_.trail) rules_.trail+=0x19;
    rules_.flash_player=symbol("FLASHPLAYER_STRAT");rules_.discrete_rotation_shape=std::uint16_t(symbol("UP_DOOR"));
    if(symbol("M_NANMODE")) rules_.crosshair=symbol("TEST_ISTRAT");
    intro_laser_=std::uint16_t(symbol("RELFASTELASER"));
    constexpr std::array names{"ID_1_C","RED_C","WHITE_C"};
    for(unsigned i=0;i<names.size();++i) {
        for(auto address:symbols.find(names[i])) if((address&0xffff)>=0x8000 && (address>>16)<0x70) {
            colours_[i]=std::uint16_t(address);break;
        }
        if(!colours_[i]) throw std::runtime_error(std::string("Missing 3DS source colour table: ")+names[i]);
    }
}
std::shared_ptr<const assets::Shape> GameModels::shape(std::uint32_t address,std::uint16_t colour,
    const assets::ShapeHeader* parent) {
    // Compact LODs inherit base-header shift/size, so their cache key includes
    // their parent as well as the actual selected header and material table.
    const std::array<std::uint32_t,3> key{parent?parent->address:0,address,colour};
    if(const auto old=shapes_.find(key);old!=shapes_.end()) {old->second.used=epoch_;return old->second.shape;}
    auto decoded=std::make_shared<assets::Shape>(parent?decoder_.decode_lod(*parent,std::uint16_t(address),colour)
        :decoder_.decode(address,{},colour));
    const auto bytes=shape_bytes(*decoded);
    if(bytes>cache_budget) throw std::length_error("3DS decoded model exceeds asset cache budget");
    while(!shapes_.empty() && (shapes_.size()>=cache_entries || cached_bytes_>cache_budget-bytes)) {
        const auto victim=std::min_element(shapes_.begin(),shapes_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        cached_bytes_-=victim->second.bytes;shapes_.erase(victim);
    }
    shapes_.emplace(key,CachedShape{decoded,bytes,epoch_});cached_bytes_+=bytes;return decoded;
}
void GameModels::text(PicaShapes& output,const simulation::GameObject& object,const render::RenderPose& pose,
    const render::Palette256& palette,GameModelCoverage& count,PicaShapeOrder order,std::optional<PicaClip> clip) {
    const auto glyphs=text_.prepare_projected(object.colour_table,object.extended[21],
        starfox::bit_cast<std::int8_t>(object.texture_scroll_x),pose,112);
    if(glyphs.glyphs.empty() || glyphs.character_size<=0) return;
    const auto dimension=std::trunc(glyphs.character_size*256./pose.z);
    if(dimension<=0) return;
    const auto width=dimension*pose.z/256.;
    for(unsigned i=0;i<glyphs.glyphs.size();++i) {
        assets::TextureImage art;art.u_mask=art.v_mask=15;art.texels.resize(256);
        for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x)
            art.texels[y*16+x]=(glyphs.glyphs[i][y]&(0x8000U>>x))?1:0;
        auto prepared=auxiliary(pose);prepared.pose.palette_override=glyphs.colour;
        render::ShapePrimitive primitive;primitive.kind=render::ShapePrimitiveKind::sprite;
        primitive.material.texture=&art;primitive.simple_sprite=true;primitive.sprite_half_extent=width*.5;
        primitive.vertices.push_back({{pose.x+(i+.5-glyphs.glyphs.size()*.5)*width,pose.y,pose.z},{}});
        prepared.primitives.push_back(std::move(primitive));output.append(prepared,palette,source_origin,nullptr,order,clip);
        ++count.text_glyphs;++count.primitives;
    }
}
void GameModels::particles(PicaShapes& output,const vr::GameSceneSnapshot& scene,simulation::ObjectHandle owner,
    const render::RenderPose& pose,double alpha,const render::Palette256& palette,GameModelCoverage& count,
    PicaShapeOrder order,std::optional<PicaClip> clip) {
    auto prepared=auxiliary(pose);
    for(const auto& particle:scene.particles) {
        if(!particle.life || particle.owner!=owner) continue;
        const std::array<double,3> current{pose.x+interpolate_word(particle.previous_x,particle.x,alpha),
            pose.y+interpolate_word(particle.previous_y,particle.y,alpha),pose.z+interpolate_word(particle.previous_z,particle.z,alpha)};
        if(current[2]<256) continue; // Native MPART depth rule; GPU handles per-eye viewport clipping.
        render::ShapePrimitive primitive;primitive.material.colour={particle.colour,particle.colour,false};
        if(particle.flags&4) {
            const std::array<double,3> previous{pose.x+particle.previous_x,pose.y+particle.previous_y,pose.z+particle.previous_z};
            if(previous[2]<256) continue;
            primitive.kind=render::ShapePrimitiveKind::line;primitive.vertices={{{previous},{}},{{current},{}}};
        } else {
            primitive.kind=render::ShapePrimitiveKind::polygon;const auto size=current[2]*2./256;
            for(const auto offset:std::array<std::array<double,2>,4>{{{0,0},{size,0},{size,size},{0,size}}})
                primitive.vertices.push_back({{current[0]+offset[0],current[1]+offset[1],current[2]}, {}});
        }
        prepared.primitives.push_back(std::move(primitive));++count.particles;++count.primitives;
    }
    output.append(prepared,palette,source_origin,nullptr,order,clip);
}
PicaFrame GameModels::prepare(const GamePresentation& frame) {
    if(!frame.current || !frame.previous || !frame.raster || !frame.raster->ppu
        || !std::isfinite(frame.interpolation_alpha)) throw std::invalid_argument("Incomplete 3DS source model snapshot");
    const auto& scene=*frame.current;const auto& previous=*frame.previous;
    auto alpha=std::clamp(frame.interpolation_alpha,0.,1.);
    if(scene.scene_epoch!=previous.scene_epoch) alpha=1;
    auto poses=vr::interpolate_scene_poses(previous,scene,alpha,rules_);
    const auto shadows=scene.shadows_enabled?vr::interpolate_scene_poses(previous,scene,alpha,rules_,true):std::vector<render::RenderPose>{};
    auto words=frame.raster->ppu->cgram;
    std::copy(scene.model_palette.begin(),scene.model_palette.end(),words.begin()+112);
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),frame.raster->brightness);
    auto& next=next_geometry_;next.clear();GameModelCoverage count;++epoch_;
    const auto clip=game_controls_clip(scene.flow);
    const bool controls=clip.has_value();
    // Match the desktop Controls compositor: ordinary demo shadows/models,
    // then the isolated player's shadow/model. Retain BSP face painter order
    // within the late pass, and finite camera vertices for native stereo.
    // High BG2 artwork and source circle colour math remain later groups.
    for(unsigned pass=0;pass<(controls?4U:2U);++pass) for(unsigned i=0;i<scene.objects.size();++i) {
        const bool shadow=(pass&1)==0,late=pass>=2;
        const auto& item=scene.objects[i];const auto& object=item.object;
        if(controls && (item.handle==scene.player)!=late) continue;
        const auto order=late?PicaShapeOrder::painter:PicaShapeOrder::depth;
        const auto flags=object.strategy_flags[0];
        const bool reticle=rules_.crosshair && object.strategy_address==rules_.crosshair;
        if(reticle && scene.flow!=simulation::GameFlowState::gameplay && scene.flow!=simulation::GameFlowState::training) continue;
        if(shadow && (!scene.shadows_enabled || !(flags&0x0c))) continue;
        if(!shadow && (flags&4)) continue;
        auto pose=shadow?shadows[i]:poses[i];
        if(flags&0x40) {if(!shadow) text(next,object,pose,palette,count,order,clip);continue;}
        if(!shadow && (flags&0x10)) {particles(next,scene,item.handle,pose,alpha,palette,count,order,clip);continue;}
        if(!object.shape) continue;
        auto colour=object.colour_table;
        if(scene.colour_table_override && !reticle) colour=*scene.colour_table_override;
        else if((flags&2) && !(flags&0x20)) colour=colours_[(flags&1)?1:2];
        else if(flags&1) colour=colours_[0];
        try {
            const auto base=shape(object.shape,colour);const auto header=base->header;
            const auto selected=shadow?header.shadow_pointer:assets::ShapeDecoder::select_lod_pointer(header,item.source_pose.source_depth);
            const auto model=selected==std::uint16_t(header.address)?base:shape(selected,colour,&header);
            if(reticle && !shadow) {
                pose.palette_override=207;pose.colour_warp=false;pose.wireframe_mode=pose.wobble_mode=0;
                pose.wave_mode=pose.cel_mode=false;pose.wave_offset=0;
            }
            pose.collapse_to_axis_line=scene.flow==simulation::GameFlowState::intro && intro_laser_
                && object.shape==intro_laser_ && pose.z<1024;
            if(pose.simple_scaled_sprite) {
                auto adjustment=std::int16_t(starfox::bit_cast<std::int8_t>(object.texture_scroll_x));
                for(unsigned bit=0;bit<header.shift;++bit) adjustment=simulation::add16(adjustment,adjustment);
                auto diameter=simulation::add16(header.size,adjustment);diameter=simulation::add16(diameter,diameter);
                pose.simple_sprite_world_size=diameter?diameter:1;pose.simple_sprite_colour=object.extended[21];
            }
            const auto prepared=renderer_.prepare_primitives(*model,pose);next.append(prepared,palette,source_origin,&frame.plan,order,clip);
            count.primitives+=prepared.primitives.size();if(shadow) ++count.shadows;else ++count.models;
        } catch(const std::exception& error) {
            throw std::runtime_error("3DS object "+std::to_string(item.handle)+" shape "+std::to_string(object.shape)+": "+error.what());
        }
    }
    // Texture-view allocation is part of preparation, not a possible failure
    // after replacing the last complete published scene.
    const auto completed=next.frame(frame.plan);
    count.cached_shape_bytes=cached_bytes_;std::swap(geometry_,next);coverage_=count;
    return completed;
}
} // namespace starfox::platform::nintendo_3ds
