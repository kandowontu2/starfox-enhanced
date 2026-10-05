#include "starfox/platform/nintendo_3ds/game_models.hpp"
#include "starfox/platform/nintendo_3ds/game_layers.hpp"
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include "starfox/platform/nintendo_3ds/pica_raster.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include "starfox/platform/nintendo_3ds/game_dots.hpp"
#include "starfox/platform/nintendo_3ds/pica_window.hpp"
#include "starfox/platform/nintendo_3ds/pica_colour.hpp"
#include <charconv>
#include <iostream>
#include <limits>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
std::string context;
void require(bool value,const char* why) {++checks;if(!value) throw std::runtime_error(context+": "+why);}
bool same_draws(std::span<const PicaDraw> a,std::span<const PicaDraw> b) {
    if(a.size()!=b.size()) return false;
    for(unsigned i=0;i<a.size();++i) {
        const auto& x=a[i];const auto& y=b[i];
        if(x.first!=y.first || x.count!=y.count || x.texture!=y.texture || x.model!=y.model
            || x.space!=y.space || x.depth_test!=y.depth_test || x.depth_write!=y.depth_write
            || x.alpha_blend!=y.alpha_blend || x.screen_dither!=y.screen_dither || x.dither_odd!=y.dither_odd || x.clip!=y.clip
            || x.source_layer!=y.source_layer || x.colour_op!=y.colour_op || x.projected_uv!=y.projected_uv) return false;
    }
    return true;
}
std::array<unsigned,5> source_pixel(const PicaFrame& frame,unsigned x,unsigned y) {
    std::array<unsigned,5> result{};
    for(const auto& draw:frame.draws) {
        if(draw.texture==pica_no_texture) continue;
        const auto& image=frame.textures[draw.texture];
        if(draw.space==PicaSpace::world) continue;
        const auto& origin=frame.vertices[draw.first].position;
        const int ix=int(x)+72-int(origin[0]),iy=int(y)+8-int(origin[1]);
        if(ix<0 || iy<0 || ix>=int(image.width) || iy>=int(image.height)) continue;
        const auto at=std::size_t(iy)*image.pitch+unsigned(ix)*4;
        if(image.pixels[at+3]) result={image.pixels[at],image.pixels[at+1],image.pixels[at+2],255,
            image.source_layers.empty()?draw.source_layer:image.source_layers[std::size_t(iy)*image.layer_pitch+unsigned(ix)]};
    }
    return result;
}
std::vector<std::array<unsigned,5>> flat_painter_pixels(const PicaFrame& frame) {
    // Independent pixel-centre triangle/nearest-texture oracle. The previous
    // raster-only image-offset sampler cannot validate a compact tile atlas.
    // Include the entire mono LCD, not only its canonical 256x224 window.
    std::vector<std::array<unsigned,5>> result(top_width*screen_height);
    for(const auto& draw:frame.draws) {
        require(draw.space!=PicaSpace::world,"Flat source oracle received finite geometry");
        if(draw.texture==pica_no_texture) continue;
        const auto& image=frame.textures[draw.texture];
        for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
            std::array<std::array<double,2>,3> p{};
            for(unsigned k=0;k<3;++k) {
                const auto& v=frame.vertices[i+k];p[k]={v.position[0],v.position[1]};
            }
            const auto cross=[](auto a,auto b,auto s){return (b[0]-a[0])*(s[1]-a[1])-(b[1]-a[1])*(s[0]-a[0]);};
            const double area=cross(p[0],p[1],p[2]);if(std::abs(area)<1.e-12) continue;
            const int x0=std::max(0,int(std::floor(std::min({p[0][0],p[1][0],p[2][0]}))));
            const int x1=std::min(int(top_width),int(std::ceil(std::max({p[0][0],p[1][0],p[2][0]}))));
            const int y0=std::max(0,int(std::floor(std::min({p[0][1],p[1][1],p[2][1]}))));
            const int y1=std::min(int(screen_height),int(std::ceil(std::max({p[0][1],p[1][1],p[2][1]}))));
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                const std::array sample{double(x)+.5,double(y)+.5};
                const std::array weights{cross(p[1],p[2],sample)/area,cross(p[2],p[0],sample)/area,cross(p[0],p[1],sample)/area};
                if(std::any_of(weights.begin(),weights.end(),[](double w){return w< -1.e-5;})) continue;
                double u=0,v=0;
                for(unsigned k=0;k<3;++k) {u+=weights[k]*frame.vertices[i+k].uv[0];v+=weights[k]*frame.vertices[i+k].uv[1];}
                const unsigned tx=unsigned(std::clamp(u*image.width,0.,double(image.width-1)));
                const unsigned ty=unsigned(std::clamp(v*image.height,0.,double(image.height-1)));
                const auto at=std::size_t(ty)*image.pitch+tx*4;
                if(image.pixels[at+3]) result[std::size_t(y)*top_width+unsigned(x)]={
                    image.pixels[at],image.pixels[at+1],image.pixels[at+2],image.pixels[at+3],
                    image.source_layers.empty()?draw.source_layer:image.source_layers[std::size_t(ty)*image.layer_pitch+tx]};
            }
        }
    }
    return result;
}
unsigned remaining_layer_vertices(std::initializer_list<PicaFrame> groups) {
    std::size_t occupied=0;for(const auto& group:groups) occupied+=group.vertices.size();
    require(occupied<=pica_vertex_limit,"Non-layer source geometry exceeded whole-scene vertex limit");
    return pica_vertex_limit-unsigned(occupied);
}
void auxiliary_checks(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    context="Synthetic particle/transaction fixture using actual cartridge assets";
    GameSession session(rom,symbols,[](auto){},"LEVEL1_1");GameModels models(session.rom(),session.symbols());
    auto source=session.presentation(1,true);
    auto scene=std::make_shared<vr::GameSceneSnapshot>(*source.current);
    scene->objects.clear();scene->transforms.clear();scene->particles={};scene->camera={};
    scene->view_matrix={32767,0,0,0,32767,0,0,0,32767};scene->shadows_enabled=false;
    vr::GameSceneObject item;item.handle=1;item.object.strategy_flags[0]=0x10;
    item.presentation.transform.z=512;item.presentation.rotation_matrix=scene->view_matrix;
    item.source_pose.source_depth=512;item.source_pose.use_source_lighting_state=true;
    item.source_pose.source_lighting_matrix=scene->view_matrix;
    scene->objects.push_back(item);scene->transforms.emplace(item.handle,item.presentation);
    auto& dot=scene->particles[0];dot.owner=1;dot.life=8;dot.colour=5;
    dot.previous_x=32760;dot.x=-32760; // Short signed-word crossing, not a 65k jump.
    scene->particles[1]=dot;scene->particles[1].flags=4;
    scene->particles[2]=dot;scene->particles[2].z=scene->particles[2].previous_z=-400;
    scene->particles[3]=dot;scene->particles[3].owner=2;
    source.previous=source.current=scene;source.interpolation_alpha=.5;
    const auto before=session.game().save_state(),before_audio=session.audio().save_state();
    const auto frame=models.prepare(source);validate_pica_frame(frame,source.dashboard);
    require(models.coverage().particles==2 && frame.vertices.size()==12,
        "Owner/depth rules must keep exactly the valid 2x2 dot and one trail");
    float low=std::numeric_limits<float>::max(),high=-low;
    for(const auto& vertex:frame.vertices) {low=std::min(low,vertex.position[0]);high=std::max(high,vertex.position[0]);}
    require(high-low<32 && low>32750,"Particle interpolation took the long path across signed-word wrap");
    const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());
    auto clipped=std::make_shared<vr::GameSceneSnapshot>(*scene);
    clipped->objects[0].source_pose.effect_clip_left=64;clipped->objects[0].source_pose.effect_clip_right=160;
    auto masked=source;masked.current=masked.previous=clipped;
    const auto masked_frame=models.prepare(masked);validate_pica_frame(masked_frame,source.dashboard);
    require(masked_frame.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),masked_frame.vertices.begin())
        && masked_frame.draws.size()==1 && masked_frame.draws[0].clip==PicaClip{152,0,248,240},
        "Particle source window must become hardware eye-space clip, not mono geometry truncation");
    auto invalid=source;invalid.interpolation_alpha=std::numeric_limits<double>::quiet_NaN();
    bool rejected=false;try {static_cast<void>(models.prepare(invalid));} catch(const std::exception&) {rejected=true;}
    require(rejected,"Invalid interpolation must reject a new source frame");
    require(masked_frame.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),masked_frame.vertices.begin())
        && models.coverage().particles==2,"Failed new scene replaced or invalidated the last complete model stream");
    auto raster=std::make_shared<GameRasterSnapshot>(*source.raster);raster->brightness=0;source.raster=raster;
    const auto faded=models.prepare(source);
    for(const auto& vertex:faded.vertices) require(vertex.colour[0]==0 && vertex.colour[1]==0 && vertex.colour[2]==0,
        "Native 60Hz fade was delayed until the next model snapshot");
    require(session.game().save_state()==before && session.audio().save_state()==before_audio,
        "Synthetic source observation or fade modified cartridge/audio state");
}
void fixture(const assets::RomImage& rom,const assets::SymbolMap& symbols,const std::string& map,unsigned requested_phases=0,
    bool all_optics=false) {
    GameSession session(rom,symbols,[](auto){},map);GameModels models(session.rom(),session.symbols());
    PicaRaster background,objects,native_bitmap,priority_oracle;PicaComposite composite;PicaWindow window;
    PicaColourEffects colour;GameLayers cartridge_layers,reference_layers;GameDots dots(session.rom(),session.symbols());
    unsigned frames{},models_seen{},shadows{},glyphs{},particles{},vertices{},draws{},textures{};
    unsigned dust_points{},grid_points{},connected_points{},combined_vertices{},combined_draws{},combined_textures{};
    unsigned panorama_frames{},combined_resident_bytes{},optical_frames{},optical_resident_bytes{};
    unsigned water_frames{},tunnel_frames{},unique_frames{},orbital_frames{},disabled_dot_frames{},intro_frames{};
    // Retain the actual owner graph through scene changes. Recreating it per
    // maximum-optics sample cannot detect growing cached receiver domains.
    GameLayers wide_layers;GameModels wide_models(session.rom(),session.symbols());
    GameDots wide_dots(session.rom(),session.symbols());PicaColourEffects wide_colour;
    PicaWindow wide_window;PicaComposite wide_composite;
    std::array<bool,2> optical_checked{};
    std::array<bool,4> panorama_modes_checked{};
    std::array<bool,2> unique_halves_checked{};
    session.advance(0,0);
    const unsigned phases=requested_phases?requested_phases:map=="BOOT"?240:1440;unsigned outdoor_frames=0;
    for(unsigned phase=1;phase<=phases;++phase) {
        context=map+" native phase "+std::to_string(phase);
        const auto time=(std::int64_t(phase)*1'000'000'000+59)/60;
        const auto held=map=="BOOT" && phase==8?input::start
            :map!="BOOT" && phase>24?input::ButtonMask(input::y|input::right):0;
        session.advance(time,held);
        auto source=session.presentation(1,true);const auto state=session.game().save_state(),apu=session.audio().save_state();
        water_frames+=source.current->background_water_surround;
        tunnel_frames+=source.raster->ppu->tunnel_scene;
        unique_frames+=source.current->background_unique_top_rows!=0
            || source.current->background_landscape_unique_half || source.current->background_landscape_unique_right_half;
        orbital_frames+=source.current->background_orbital_planet;
        intro_frames+=source.current->flow==simulation::GameFlowState::intro;
        const auto frame=models.prepare(source);validate_pica_frame(frame,source.dashboard);
        const auto dot_frame=dots.prepare(source);validate_pica_frame(dot_frame,source.dashboard);
        // Resource/ordering bridge check, NOT the final all-flow compositor:
        // native ground, EX spans and menu host UI still remain.
        PpuBatch bg{{{PpuLayer::bg2}},PicaSpace::scenery,true};
        bg.passes[0].scroll=source.current->background_scroll_override;
        PpuBatch obj{{{PpuLayer::objects}}};obj.passes[0].sprites=source.sprites;
        const auto back=background.prepare(source.raster->ppu,bg,source.plan,source.raster->brightness,
            source.current->background_colour_subtract);
        const auto front=objects.prepare(source.raster->ppu,obj,source.plan,source.raster->brightness);
        PpuBatch bitmap{{{PpuLayer::bg1}}};bitmap.passes[0].guard_inset=16;
        bitmap.passes[0].transparent_black=true;bitmap.passes[0].mosaic_inset=true;
        validate_pica_frame(native_bitmap.prepare(source.raster->ppu,bitmap,source.plan,source.raster->brightness),source.dashboard);
        const auto mask=window.prepare(source.raster->wipe,source.plan,
            source.current->flow==simulation::GameFlowState::gameplay || source.current->flow==simulation::GameFlowState::training
                ?WindowCoverage::full_scene:WindowCoverage::authored);
        const auto effects=colour.prepare(source.raster->circle,source.raster->colour_math,source.raster->brightness,source.plan);
        const auto composed=composite.prepare(source.plan,std::array{back,frame,front,effects,mask},source.dashboard);
        require(composed.vertices.size()==back.vertices.size()+frame.vertices.size()+front.vertices.size()+effects.vertices.size()+mask.vertices.size()
            && std::equal(frame.vertices.begin(),frame.vertices.end(),composed.vertices.begin()+back.vertices.size()),
            "Native layer composition lost/reprojected cartridge model geometry");
        const auto layer_budget=remaining_layer_vertices({frame,dot_frame,effects,mask});
        const auto ordered=cartridge_layers.prepare(source,layer_budget);
        if(native_panorama_scene(source)) {
            ++panorama_frames;
            const auto policy=game_layer_plan(source);std::vector<PpuPass> flattened;
            for(const auto& group:policy.before_model_groups)
                flattened.insert(flattened.end(),group.passes.begin(),group.passes.end());
            require(flattened==policy.before_models.passes,"Actual source panorama reordered interleaved sprite/background priorities");
            for(const auto& draw:ordered.before_models.draws) if(draw.space==PicaSpace::scenery)
                require(draw.source_layer==2 && ordered.before_models.textures[draw.texture].source_layers.empty(),
                    "Actual source panorama pulled mixed sprite ownership into infinity or duplicated A8 residency");
            const auto mode=source.raster->ppu->background_mode;
            if(!panorama_modes_checked[mode] && !ordered.before_models.draws.empty()) {
                const auto mono=reference_layers.prepare(source,0);
                require(flat_painter_pixels(ordered.before_models)==flat_painter_pixels(mono.before_models),
                    "Actual cartridge tile/split scenery changed mono LCD pixels, margins or source ownership");
                panorama_modes_checked[mode]=true;
            }
        }
        if(native_landscape_scene(source)) {
            ++outdoor_frames;
            require(!ordered.before_models.draws.empty() && ordered.before_models.draws[0].space==PicaSpace::scenery,
                "Actual outdoor cartridge background stayed at HUD depth");
            const auto plane=source_landscape_plane(source);
            const double distance=plane.height*source.plan.focal_y;
            unsigned sky_count=0,receiver_count=0;bool receiver_visible=false;
            for(const auto& draw:ordered.before_models.draws) {
                require(draw.source_layer==2,"Actual cartridge terrain lost source ownership");
                if(draw.space==PicaSpace::scenery) {
                    ++sky_count;double low=std::numeric_limits<double>::max(),high=-low;
                    for(unsigned i=draw.first;i<draw.first+draw.count;++i) {
                        const auto& p=ordered.before_models.vertices[i].position;
                        const double d=p[1]-plane.centre-plane.slope*(p[0]-200);
                        low=std::min(low,d);high=std::max(high,d);
                    }
                    receiver_visible|=high>distance/source.plan.far_plane && low<distance/source.plan.near_plane;
                } else {
                    ++receiver_count;
                    require(draw.space==PicaSpace::world && draw.projected_uv && draw.depth_test,
                        "Actual cartridge terrain lost its finite source-owned receiver");
                }
            }
            // A source horizon below the LCD correctly emits sky only; broad
            // guarded receivers can also use more than one borrowed strip.
            // Require finite geometry exactly when the source domain intersects
            // the near/far receiver, not an unconditional two-draw screenshot.
            require(sky_count==ordered.before_models.textures.size() && (receiver_count!=0)==receiver_visible,
                "Actual cartridge finite terrain coverage disagrees with its source plane");
            if(source.current->background_landscape_unique_half || source.current->background_landscape_unique_right_half) {
                const bool right=source.current->background_landscape_unique_right_half;
                auto policy=game_layer_plan(source);
                require(std::any_of(policy.before_models.passes.begin(),policy.before_models.passes.end(),[&](const auto& pass) {
                    return pass.single_occurrence_sky_half==PpuUniqueSkyHalf{right,unsigned(source.current->landscape_atlas_origin)+112};
                }),"Actual unique landscape did not reach its native BG2 decoder policy");
                if(!unique_halves_checked[right]) {
                    for(auto& pass:policy.before_models.passes) pass.single_occurrence_sky_half.reset();
                    const auto authored=priority_oracle.prepare(source.raster->ppu,policy.before_models,source.plan,
                        source.raster->brightness,source.current->background_colour_subtract);
                    for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
                        require(source_pixel(ordered.before_models,x,y)==source_pixel(authored,x,y),
                            "Unique-half margin policy changed actual canonical cartridge pixels or opaque ownership");
                    unique_halves_checked[right]=true;
                }
            }
        }
        const auto ordered_frame=composite.prepare(source.plan,
            std::array{ordered.before_models,dot_frame,frame,ordered.after_models,effects,mask},source.dashboard,ordered.clear);
        require(ordered_frame.vertices.size()==ordered.before_models.vertices.size()+dot_frame.vertices.size()+frame.vertices.size()
            +ordered.after_models.vertices.size()+effects.vertices.size()+mask.vertices.size(),
            "Actual cartridge priority adapter lost a model/PPU/effect painter group");
        require(std::equal(dot_frame.vertices.begin(),dot_frame.vertices.end(),ordered_frame.vertices.begin()+ordered.before_models.vertices.size())
            && std::equal(frame.vertices.begin(),frame.vertices.end(),ordered_frame.vertices.begin()+ordered.before_models.vertices.size()+dot_frame.vertices.size()),
            "Actual cartridge dust/grid must precede models, after source scenery");
        combined_vertices=std::max(combined_vertices,unsigned(ordered_frame.vertices.size()));
        combined_draws=std::max(combined_draws,unsigned(ordered_frame.draws.size()));
        combined_textures=std::max(combined_textures,unsigned(ordered_frame.textures.size()));
        unsigned resident=512U*256U*4U;
        for(const auto& image:ordered_frame.textures) resident+=pica_resident_texture_bytes(image);
        combined_resident_bytes=std::max(combined_resident_bytes,resident);
        require(resident<=pica_texture_budget,"Actual source painter separation exceeded total padded native residency including lower LCD");
        const unsigned optical_kind=native_landscape_scene(source)?1:0;
        if(all_optics || ((native_landscape_scene(source) || native_panorama_scene(source)) && !optical_checked[optical_kind])) {
            auto wide=source;auto settings=session.stereo_settings();
            settings.strength=2;settings.separation=64;settings.convergence=16;
            wide.plan=plan_frame(1,true,ScreenUse::world,settings);
            const auto phase_context=context;
            context+=" maximum-optics source layers";
            context=phase_context+" maximum-optics geometry/grid";
            const auto geometry=wide_models.prepare(wide),ink=wide_dots.prepare(wide);
            const auto tint=wide_colour.prepare(wide.raster->circle,wide.raster->colour_math,wide.raster->brightness,wide.plan);
            const auto wipe=wide_window.prepare(wide.raster->wipe,wide.plan,WindowCoverage::full_scene);
            const auto layers=wide_layers.prepare(wide,remaining_layer_vertices({geometry,ink,tint,wipe}));
            context=phase_context+" maximum-optics complete composition";
            const auto composed_wide=wide_composite.prepare(wide.plan,
                std::array{layers.before_models,ink,geometry,layers.after_models,tint,wipe},wide.dashboard,layers.clear);
            unsigned wide_bytes=512*256*4;
            for(const auto image:composed_wide.textures) {
                require(image.width<=1024,"Actual optical source strip exceeds PICA texture dimensions");
                wide_bytes+=pica_resident_texture_bytes(image);
            }
            require(wide_bytes<=pica_texture_budget && composed_wide.plan.separation==128,
                "Actual wide source composition exceeded residency or silently reduced optics");
            optical_resident_bytes=std::max(optical_resident_bytes,wide_bytes);++optical_frames;
            optical_checked[optical_kind]=true;
            context=phase_context;
        }
        const auto dot_coverage=dots.coverage();dust_points+=dot_coverage.dust;
        grid_points+=dot_coverage.grid;connected_points+=dot_coverage.connections;
        if(source.current->dots_mode==0) {
            ++disabled_dot_frames;
            require(dot_coverage.dust==0 && dot_coverage.grid==0 && dot_coverage.connections==0
                && dot_frame.vertices.empty() && dot_frame.draws.empty() && dot_frame.textures.empty(),
                "Cartridge-disabled dust/grid produced native geometry or retained ink");
        }
        const auto ordered_work=cartridge_layers.work();
        const auto background_work=background.work(),object_work=objects.work();
        const auto effect_builds=colour.builds();
        const auto count=models.coverage();models_seen+=count.models;shadows+=count.shadows;
        glyphs+=count.text_glyphs;particles+=count.particles;
        vertices=std::max(vertices,unsigned(frame.vertices.size()));draws=std::max(draws,unsigned(frame.draws.size()));
        textures=std::max(textures,unsigned(frame.textures.size()));
        require(count.cached_shape_bytes<=4*1024*1024,"Decoded asset cache grew beyond original-hardware budget");
        // Copy before re-preparing: these are explicitly borrowed spans.
        const std::vector<PicaVertex> saved(frame.vertices.begin(),frame.vertices.end());
        const std::vector<PicaDraw> saved_draws(frame.draws.begin(),frame.draws.end());
        std::vector<std::vector<std::uint8_t>> saved_textures;
        for(const auto image:frame.textures) saved_textures.emplace_back(image.pixels.begin(),image.pixels.end());
        const std::vector<PicaVertex> saved_dots(dot_frame.vertices.begin(),dot_frame.vertices.end());
        const std::vector<PicaDraw> saved_dot_draws(dot_frame.draws.begin(),dot_frame.draws.end());
        std::vector<std::vector<std::uint8_t>> saved_dot_textures;
        for(const auto image:dot_frame.textures) saved_dot_textures.emplace_back(image.pixels.begin(),image.pixels.end());
        for(float slider:{0.F,.5F,1.F}) {
            source=session.presentation(slider,true);const auto other=models.prepare(source);
            const auto other_dots=dots.prepare(source);validate_pica_frame(other_dots,source.dashboard);
            require(other_dots.vertices.size()==saved_dots.size() && std::equal(saved_dots.begin(),saved_dots.end(),other_dots.vertices.begin())
                && same_draws(saved_dot_draws,other_dots.draws) && other_dots.textures.size()==saved_dot_textures.size()
                && dots.coverage().ink_updates==dot_coverage.ink_updates,"Slider changed cartridge dust/grid ink or reran its rasterizer");
            for(unsigned i=0;i<saved_dot_textures.size();++i)
                require(std::equal(saved_dot_textures[i].begin(),saved_dot_textures[i].end(),other_dots.textures[i].pixels.begin()),
                    "Slider changed cartridge connected-grid palette/coverage");
            validate_pica_frame(other,source.dashboard);
            require(other.vertices.size()==saved.size() && std::equal(saved.begin(),saved.end(),other.vertices.begin())
                && same_draws(saved_draws,other.draws),"Slider/eye projection rebuilt different world geometry or source order");
            require(other.textures.size()==saved_textures.size(),"Slider changed source artwork count");
            background.prepare(source.raster->ppu,bg,source.plan,source.raster->brightness,source.current->background_colour_subtract);
            objects.prepare(source.raster->ppu,obj,source.plan,source.raster->brightness);
            colour.prepare(source.raster->circle,source.raster->colour_math,source.raster->brightness,source.plan);
            cartridge_layers.prepare(source,layer_budget);
            const auto priority_work=cartridge_layers.work();
            require(priority_work[0].decodes==ordered_work[0].decodes && priority_work[1].decodes==ordered_work[1].decodes
                && priority_work[0].colour_updates==ordered_work[0].colour_updates && priority_work[1].colour_updates==ordered_work[1].colour_updates,
                "Slider reran actual cartridge painter policy rasterization");
            require(colour.builds()==effect_builds,"Slider rebuilt native source colour coverage");
            require(background.work().decodes==background_work.decodes && background.work().colour_updates==background_work.colour_updates
                && objects.work().decodes==object_work.decodes && objects.work().colour_updates==object_work.colour_updates,
                "Slider rerasterized native PPU layers");
            for(unsigned i=0;i<other.textures.size();++i)
                require(std::vector<std::uint8_t>(other.textures[i].pixels.begin(),other.textures[i].pixels.end())==saved_textures[i],
                    "Slider changed source palette/artwork");
        }
        require(session.game().save_state()==state && session.audio().save_state()==apu,"Native model preparation advanced/mutated game or audio");
        ++frames;
    }
    std::cout<<map<<": "<<frames<<" source frames, "<<models_seen<<" models, "<<shadows<<" shadows, "<<glyphs<<" glyphs, "
        <<particles<<" particles, "<<outdoor_frames<<" terrain frames; peak "<<vertices<<" vertices / "<<draws<<" draws / "<<textures<<" textures\n";
    std::cout<<map<<": dust/grid/connected points "<<dust_points<<" / "<<grid_points<<" / "<<connected_points
        <<"; combined peak "<<combined_vertices<<" vertices / "<<combined_draws<<" draws / "<<combined_textures<<" textures\n";
    std::cout<<map<<": "<<panorama_frames<<" panorama frames; padded texture residency peak "<<combined_resident_bytes<<" bytes including lower LCD\n";
    std::cout<<map<<": source policy observations water/tunnel/unique/orbital "<<water_frames<<" / "<<tunnel_frames
        <<" / "<<unique_frames<<" / "<<orbital_frames<<"; cartridge-disabled dot frames "<<disabled_dot_frames
        <<"; counts do not prove visual policy acceptance\n";
    std::cout<<map<<": "<<optical_frames<<" actual strength-2 maximum-menu optical fixtures / "<<optical_resident_bytes
        <<" padded GPU bytes including lower LCD; "<<intro_frames<<" observed intro frames; "
        <<(all_optics?"every requested source frame checked":"first eligible policy samples only")
        <<"; not whole-flow peak RAM or hardware acceptance\n";
    require(models_seen>0 && vertices>0,"Actual fixture never produced source geometry");
    // These mandatory-positive checks belong to the original default fixtures.
    // Other stages can legitimately disable dots, or never contain terrain.
    if(map=="LEVEL1_1") require(outdoor_frames>0,"Corneria check stopped before the outdoor terrain actually appeared");
    if(map=="BOOT" || map=="LEVEL1_1")
        require(dust_points+grid_points+connected_points>0,"Default fixture did not exercise cartridge dust/grid");
}
}
int main(int argc,char** argv) try {
    constexpr auto usage="Usage: check_3ds_game_models ROM SYMBOLS [MAP [SOURCE_FRAMES [--all-optics]]]\n"
        "Default: BOOT (240 frames) and LEVEL1_1 (1440 frames).\n"
        "Optional MAP: an exact cartridge map symbol; SOURCE_FRAMES: 1..3600.\n"
        "--all-optics: check every requested frame at strength 2, separation 64, convergence 16, retaining native owners.\n"
        "Uses private local assets; host policy/resource checks, not native gameplay or hardware acceptance.\n";
    if(argc==2 && std::string_view(argv[1])=="--help") {std::cout<<usage;return 0;}
    if(argc<3 || argc>6 || (argc==6 && std::string_view(argv[5])!="--all-optics"))
        throw std::invalid_argument(usage);
    unsigned phases=0;
    if(argc>=5) {
        const auto value=std::string_view(argv[4]);
        const auto parsed=std::from_chars(value.data(),value.data()+value.size(),phases);
        if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || phases<1 || phases>3600)
            throw std::invalid_argument("SOURCE_FRAMES must be a whole number from 1 through 3600");
    }
    const auto rom=assets::RomImage::load(argv[1]);const auto symbols=assets::SymbolMap::load(argv[2]);
    auxiliary_checks(rom,symbols);
    if(argc>=4) fixture(rom,symbols,argv[3],phases,argc==6);
    else {fixture(rom,symbols,"BOOT");fixture(rom,symbols,"LEVEL1_1");}
    std::cout<<checks<<" native model-stream checks passed; NOT full compositor, ARM gameplay or hardware acceptance\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<context<<": "<<error.what()<<'\n';return 1;}
