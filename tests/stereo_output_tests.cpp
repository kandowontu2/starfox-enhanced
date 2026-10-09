#include "starfox/render/stereo_output.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/model_motion_history.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/particle_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include <cstdlib>
#include <iostream>
#include <source_location>

namespace {
void require(bool condition,const std::source_location where=std::source_location::current()) {
    if(!condition) {std::cerr<<"Stereo output regression at line "<<where.line()<<'\n';std::exit(1);}
}
}
int main() {
    {
        using namespace starfox::render;
        starfox::assets::Shape shape;shape.vertices={{-1,-1,0},{1,-1,0},{0,1,0},{2,2,0}};
        shape.faces={{-1,0,{0,0,1},{0,1,2,3}}, {-1,0,{0,0,1},{0,1}}};shape.colour_words={0x11};
        GpuModelDraw caster;caster.shape=&shape;caster.pose.continuous_geometry=true;caster.ray_geometry=true;
        auto reflected=caster;reflected.ray_materials=true;
        auto emissive=reflected;emissive.emissive=true;
        auto exploded=reflected;exploded.pose.explosion_progress=1;
        std::vector<GpuSceneDraw> frame{caster,reflected,emissive,exploded};
        auto left=*stereo_scene_eye(frame,0,16,1024),right=*stereo_scene_eye(frame,1,16,1024);
        const auto packet=[](auto& draws,unsigned i){return std::get<GpuModelDraw>(draws[i]).prepared_rays;};
        std::uint64_t base_budget{};
        {
            StereoSourceTopologies sources(left,right,8U*1024*1024,true,true,false);
            base_budget=sources.storage_bytes();require(!sources.ray_source_count() && !sources.ray_model_count());
            for(unsigned i=0;i<frame.size();++i) require(!packet(left,i) && !packet(right,i));
        }
        {
            StereoSourceTopologies sources(left,right);
            require(sources.ray_source_count()==1 && sources.ray_model_count()==4);
            require(packet(left,0)==packet(right,1) && packet(left,0)->triangles().size()==2);
            require(packet(left,0)->material_topology().size()==2);
            require(!packet(left,2) && !packet(right,2) && !packet(left,3) && !packet(right,3));
        }
        {
            StereoSourceTopologies bounded(left,right,base_budget);
            require(bounded.storage_bytes()<=base_budget && !bounded.ray_source_count());
            require(!packet(left,0) && !packet(right,1));
        }
        for(auto draws:{std::span(left),std::span(right)}) for(auto& draw:draws) {
            auto& model=std::get<GpuModelDraw>(draw);model.ray_materials=false;
        }
        {
            StereoSourceTopologies shadow_only(left,right);
            require(shadow_only.ray_source_count()==1 && packet(left,0)->material_topology().empty());
        }
        shape.faces[0].vertex_indices={3,2,1}; // Fresh packet at the same mutable shape address.
        {
            StereoSourceTopologies fresh(left,right);
            require(packet(right,0)->triangles().size()==1 && packet(right,0)->triangles()[0][0]==3);
        }
        for(auto draws:{std::span(left),std::span(right)}) for(auto& draw:draws)
            std::get<GpuModelDraw>(draw).ray_geometry=false;
        {
            StereoSourceTopologies no_rays(left,right);
            require(!no_rays.ray_source_count() && !no_rays.ray_model_count());
            for(unsigned i=0;i<frame.size();++i) require(!packet(left,i) && !packet(right,i));
        }
    }
    {
        using namespace starfox::render;
        starfox::assets::Shape shape;shape.vertices={{-1,-1,0},{1,-1,0},{0,1,0}};
        shape.faces={{-1,0,{0,0,1},{0,1,2}}};shape.colour_words={0x11};
        GpuModelDraw base;base.shape=&shape;base.pose.continuous_geometry=true;
        auto repeated=base;repeated.pose.x=10;
        auto coloured=base;coloured.pose.force_colour=true;coloured.pose.forced_colour=0x32;
        auto exploded=base;exploded.pose.explosion_progress=1;
        std::vector<GpuSceneDraw> frame{base,repeated,coloured,exploded};
        auto left=*stereo_scene_eye(frame,0,16,1024),right=*stereo_scene_eye(frame,1,16,1024);
        const auto packet=[](auto& draws,unsigned index){return std::get<GpuModelDraw>(draws[index]).prepared_faces;};
        {
            StereoSourceTopologies sources(left,right);
            require(sources.face_source_count()==2 && sources.face_model_count()==6);
            require(packet(left,0)==packet(right,1) && packet(left,0)!=packet(left,2));
            require(!packet(left,3) && !packet(right,3));
            require(packet(left,2)->faces().materials[0].even==2 && packet(left,2)->faces().materials[0].odd==3);
        }
        // Eye-specific material selection must never be flattened into the
        // left eye's material, even while immutable geometry is pair-shared.
        std::get<GpuModelDraw>(right[0]).pose.palette_override=201;
        {
            StereoSourceTopologies sources(left,right);
            require(sources.face_source_count()==3 && sources.face_model_count()==6);
            require(packet(left,0)!=packet(right,0) && packet(right,0)->faces().materials[0].even==201);
        }
        shape.colour_words[0]=0x55; // Same address, new recording, fresh source bytes.
        {
            StereoSourceTopologies sources(left,right);
            require(packet(left,0)->faces().materials[0].even==5);
        }
        {
            StereoSourceTopologies reference(left,right,8U*1024*1024,true,false);
            require(!reference.face_source_count() && !reference.face_model_count());
            for(unsigned i=0;i<frame.size();++i) require(!packet(left,i) && !packet(right,i));
        }
        const auto budget=PreparedBspSource(shape,false).storage_bytes()
            +PreparedBspSource(shape,true).storage_bytes()
            +PreparedProjectionSource(shape,base.pose,base.settings).storage_bytes();
        {
            StereoSourceTopologies bounded(left,right,budget);
            require(bounded.storage_bytes()<=budget && !bounded.face_source_count());
            for(unsigned i=0;i<frame.size();++i) require(!packet(left,i) && !packet(right,i));
        }
    }
    {
        // Source topology, never visibility/order, is immutable for both eyes.
        // Repeated instances share preparation but retain distinct poses.
        starfox::assets::Shape shape;
        shape.vertices={{-1,-1,0},{1,-1,0},{0,1,0}};
        shape.faces={{-1,1,{0,0,1},{0,1,2}}};
        shape.face_batches={{200,{{-1,2,{1,2,3},{2,1,0}}}}};
        shape.bsp_leaves={{100,200}};shape.bsp_root_address=100;
        auto distinct=shape;
        starfox::render::GpuModelDraw model;model.shape=&shape;model.pose.continuous_geometry=true;
        auto interpolated=model;interpolated.pose.explosion_phase=.5;
        auto exploded=model;exploded.pose.explosion_progress=1;exploded.pose.explosion_phase=.5;
        auto axis=model;axis.pose.collapse_to_axis_line=true;
        auto sprite=model;sprite.pose.simple_scaled_sprite=true;
        auto other=model;other.shape=&distinct;
        // Fractional presentation phase alone must NOT flatten the authored BSP;
        // destruction mode is selected by the original source byte counter.
        std::vector<starfox::render::GpuSceneDraw> frame{model,interpolated,exploded,axis,sprite,other};
        const auto pointer=[](auto& draws,std::size_t i) {
            return std::get<starfox::render::GpuModelDraw>(draws[i]).prepared_topology;
        };
        auto left=*starfox::render::stereo_scene_eye(frame,0,16,1024);
        auto right=*starfox::render::stereo_scene_eye(frame,1,16,1024);
        {
            starfox::render::StereoSourceTopologies sources(left,right);
            require(sources.source_count()==3 && sources.model_count()==8 && sources.storage_bytes()>0);
            require(sources.projection_source_count()==2 && sources.projection_model_count()==8);
            require(pointer(left,0)==pointer(right,0) && pointer(left,0)==pointer(left,1));
            require(pointer(left,2)!=pointer(left,0) && pointer(left,2)==pointer(right,2));
            require(!pointer(left,3) && !pointer(right,4) && pointer(left,5)!=pointer(left,0));
            require(pointer(left,0)->matches(shape,false) && !pointer(left,0)->matches(shape,true)
                && !pointer(left,0)->matches(distinct,false));
            require(pointer(left,0)->graph().faces[0].colour_id==2
                && pointer(left,2)->graph().faces[0].colour_id==1);
            require(pointer(left,0)->normals()[0]==std::array<std::int32_t,4>{1,2,3,0});
            require(std::get<starfox::render::GpuModelDraw>(left[0]).pose.x
                !=std::get<starfox::render::GpuModelDraw>(right[0]).pose.x);
            for(auto& draw:frame) require(!std::get<starfox::render::GpuModelDraw>(draw).prepared_topology
                && !std::get<starfox::render::GpuModelDraw>(draw).prepared_projection);
            const auto* projection=std::get<starfox::render::GpuModelDraw>(left[0]).prepared_projection;
            require(projection==std::get<starfox::render::GpuModelDraw>(right[1]).prepared_projection
                && projection==std::get<starfox::render::GpuModelDraw>(left[2]).prepared_projection);
        }
        // Fresh binding clears expired borrowed fields without reading them;
        // in-place source/address reuse must pack the changed topology again.
        shape.face_batches[0].faces[0].normal={7,8,9};
        {
            starfox::render::StereoSourceTopologies fresh(left,right);
            require(pointer(left,0)->normals()[0]==std::array<std::int32_t,4>{7,8,9,0});
        }
        for(auto budget:{std::uint64_t(0),std::uint64_t(1)}) {
            starfox::render::StereoSourceTopologies bounded(left,right,budget);
            require(!bounded.source_count() && !bounded.model_count() && !bounded.storage_bytes());
            for(std::size_t i=0;i<left.size();++i) require(!pointer(left,i) && !pointer(right,i)
                && !std::get<starfox::render::GpuModelDraw>(left[i]).prepared_projection
                && !std::get<starfox::render::GpuModelDraw>(right[i]).prepared_projection);
        }
        const auto source_bytes=starfox::render::PreparedBspSource(shape,false).storage_bytes();
        {
            starfox::render::StereoSourceTopologies bounded(left,right,source_bytes);
            require(bounded.source_count()==1 && bounded.model_count()==4 && bounded.storage_bytes()<=source_bytes);
            require(pointer(left,0) && !pointer(left,2) && !pointer(right,5));
        }
        // Failure after earlier successful binds must clear every borrowed
        // pointer, not leave a dangling partial preparation for a fallback.
        std::get<starfox::render::GpuModelDraw>(right[5]).pose.explosion_progress=1;
        bool rejected=false;
        try {starfox::render::StereoSourceTopologies bad(left,right);}
        catch(const std::exception&) {rejected=true;}
        require(rejected);
        for(std::size_t i=0;i<left.size();++i) require(!pointer(left,i) && !pointer(right,i)
            && !std::get<starfox::render::GpuModelDraw>(left[i]).prepared_projection
            && !std::get<starfox::render::GpuModelDraw>(right[i]).prepared_projection);
        std::get<starfox::render::GpuModelDraw>(right[5]).pose.explosion_progress=0;
        {
            starfox::render::StereoSourceTopologies duplicate(left,right,8U*1024*1024,false);
            require(duplicate.source_count()==3 && !duplicate.projection_source_count()
                && !duplicate.projection_model_count());
        }
    }
    {
        // Canonical animation frame, continuous/native representation and the
        // native prescale are the complete immutable vertex-source key.
        starfox::assets::Shape animated;animated.header.shift=2;
        animated.frames={{{},{{1,2,3},{4,5,6}},{false,true}},
            {{},{{7,8,9},{10,11,12}},{true,false}}};
        animated.faces={{-1,1,{0,0,1},{0,1}}};
        starfox::render::GpuModelDraw base;base.shape=&animated;base.pose.continuous_geometry=true;
        auto modulo=base;modulo.pose.animation_frame=2;
        auto next=base;next.pose.animation_frame=1;
        auto native=base;native.pose.continuous_geometry=false;native.pose.use_rotation_matrix=true;
        auto scaled=native;scaled.pose.scale=2;
        auto upscale=base;upscale.pose.scale=3;upscale.settings.render_scale=4;
        std::vector<starfox::render::GpuSceneDraw> frame{base,modulo,next,native,scaled,upscale};
        // Ordinary SBS deliberately promotes native poses to continuous ones
        // so integer projection cannot discard eye separation. Verify that
        // policy separately, then exercise native source keys with native
        // encoder recordings rather than changing the production eye policy.
        {
            auto l=*starfox::render::stereo_scene_eye(frame,0,16,1024);
            auto r=*starfox::render::stereo_scene_eye(frame,1,16,1024);
            starfox::render::StereoSourceTopologies ordinary(l,r);
            require(ordinary.projection_source_count()==2 && ordinary.projection_model_count()==12);
        }
        auto left=frame,right=frame;
        for(unsigned i=0;i<frame.size();++i) {
            std::get<starfox::render::GpuModelDraw>(left[i]).pose.x-=8;
            std::get<starfox::render::GpuModelDraw>(right[i]).pose.x+=8;
        }
        const auto packet=[](const auto& draws,unsigned i) {
            return std::get<starfox::render::GpuModelDraw>(draws[i]).prepared_projection;
        };
        {
            starfox::render::StereoSourceTopologies sources(left,right);
            require(sources.source_count()==1 && sources.model_count()==12
                && sources.projection_source_count()==4 && sources.projection_model_count()==12);
            require(packet(left,0)==packet(right,1) && packet(left,0)==packet(left,5)
                && packet(left,0)!=packet(right,2) && packet(left,3)!=packet(left,4));
            require(packet(left,3)->native_input()[0].x==4
                && packet(left,4)->native_input()[0].x==8
                && packet(left,3)->native_input()[1].x==4);
        }
        const auto budget=starfox::render::PreparedBspSource(animated,false).storage_bytes()
            +starfox::render::PreparedProjectionSource(animated,base.pose,base.settings).storage_bytes();
        {
            starfox::render::StereoSourceTopologies partial(left,right,budget);
            require(partial.source_count()==1 && partial.projection_source_count()==1
                && partial.projection_model_count()==6 && partial.storage_bytes()==budget);
            require(packet(left,0) && packet(right,1) && packet(left,5)
                && !packet(left,2) && !packet(right,3) && !packet(left,4));
        }
        std::get<starfox::render::GpuModelDraw>(right[4]).pose.scale=3;
        bool rejected=false;
        try{starfox::render::StereoSourceTopologies mismatch(left,right);}
        catch(const std::runtime_error&){rejected=true;}require(rejected);
        for(unsigned i=0;i<frame.size();++i) require(!packet(left,i) && !packet(right,i)
            && !std::get<starfox::render::GpuModelDraw>(left[i]).prepared_topology
            && !std::get<starfox::render::GpuModelDraw>(right[i]).prepared_topology);
    }
    using namespace starfox::render;
    require(stereo_ordered_layer_reuse(true,false));
    require(!stereo_ordered_layer_reuse(true,true));
    require(!stereo_ordered_layer_reuse(false,false));
    require(!stereo_ordered_layer_reuse(false,true));
    {
        // Pair-shared inputs must never include projected producers, sky
        // parallax or a marked first-person reticle. Mixed lists fail closed.
        RasterCommands commands;commands.reset(256,224);
        RasterCommand c;c.left=10;c.right=20;c.top=10;c.bottom=20;
        commands.add(c);
        auto ppu=std::make_shared<starfox::simulation::SnesPpuState>();
        GpuBackgroundDraw bg;bg.ppu=ppu;bg.settings.layer=3;
        const std::array<GpuSceneDraw,3> fixed{GpuRasterDraw{&commands},
            GpuIndexedLayerDraw{&commands},bg};
        require(stereo_screen_fixed_layer(fixed,true,true));
        require(stereo_screen_fixed_layer({}));
        for(const GpuSceneDraw projected:{GpuSceneDraw{GpuModelDraw{}},GpuSceneDraw{GpuGridDraw{}},
                GpuSceneDraw{GpuDustDraw{}},GpuSceneDraw{GpuParticleDraw{}},GpuSceneDraw{GpuTextDraw{}}}) {
            const std::array<GpuSceneDraw,2> mixed{fixed[0],projected};
            require(!stereo_screen_fixed_layer(mixed));
        }
        const std::array<GpuSceneDraw,1> no_commands{GpuRasterDraw{}};
        require(!stereo_screen_fixed_layer(no_commands));
        const std::array<GpuSceneDraw,1> no_indexed{GpuIndexedLayerDraw{}};
        require(!stereo_screen_fixed_layer(no_indexed));
        const std::array<GpuSceneDraw,1> no_ppu{GpuBackgroundDraw{}};
        require(!stereo_screen_fixed_layer(no_ppu));
        bg.settings.layer=2;bg.settings.tag=PixelLayer::background;
        std::array<GpuSceneDraw,1> sky{bg};
        require(stereo_screen_fixed_layer(sky,false,true));
        require(!stereo_screen_fixed_layer(sky,true,false));
        bg.settings.tag=PixelLayer::two_d;sky[0]=bg;
        require(stereo_screen_fixed_layer(sky,true,true));
        commands.commands[0].textured=4;commands.commands[0].reserved1=4;
        require(stereo_screen_fixed_layer(fixed,true,false));
        require(!stereo_screen_fixed_layer(fixed,false,true));
        commands.commands[0].textured=1; // Texture flags alone do not mark a reticle.
        require(stereo_screen_fixed_layer(fixed,true,true));
    }
    {
        RasterCommands source;source.reset(800,448);
        for(unsigned i=0;i<5;++i) {
            RasterCommand c;c.left=20+int(i)*32;c.right=c.left+32;c.top=40;c.bottom=72;
            c.u=c.left;c.v=c.top;c.du=2;c.dv=16;c.textured=4;c.reserved1=i<4?4U|(i&3U):0U;
            source.add(c);
        }
        for(double displacement:{-8.25,-2.,0.,2.,8.25}) {
            auto eye=source;require(stereo_translate_crosshair(eye,displacement));
            for(unsigned i=0;i<5;++i) {
                const auto& c=eye.commands[i];const auto& original=source.commands[i];
                const auto offset=i<4?std::llround(displacement*2):0;
                require(c.left==original.left+offset && c.right==original.right+offset && c.u==original.u+offset);
                require(c.top==original.top && c.v==original.v && c.reserved1==original.reserved1);
            }
        }
        auto invalid=source;invalid.commands[2].du=0;
        require(!stereo_translate_crosshair(invalid,8) && invalid.commands[0].left==source.commands[0].left);
        require(!stereo_translate_crosshair(source,std::numeric_limits<double>::infinity()));
        invalid=source;invalid.commands[2].u=std::numeric_limits<std::int32_t>::max();
        require(!stereo_translate_crosshair(invalid,1) && invalid.commands[0].left==source.commands[0].left);
        invalid=source;invalid.commands[2].left=std::numeric_limits<std::int32_t>::min();
        require(!stereo_translate_crosshair(invalid,-1) && invalid.commands[0].left==source.commands[0].left);
        for(int scale=1;scale<=10;++scale) {
            auto scaled=source;
            for(auto& c:scaled.commands) c.du=scale;
            require(stereo_translate_crosshair(scaled,2.5));
            require(scaled.commands[0].u==source.commands[0].u+std::llround(2.5*scale));
            require(scaled.commands[4].u==source.commands[4].u);
        }
    }
    {
        // TV rig: a ship at 512 must not sit at zero disparity as it did
        // with the old convergence plane. Distant models go behind the
        // screen, with no vertical disparity or toe-in distortion.
        GpuModelDraw model;
        model.settings.focal_length=256;
        model.pose.y=24;
        model.pose.vanish_y=96;
        for(const double depth:{512.,kSbsConvergence,2048.}) {
            model.pose.z=depth;
            const std::array<GpuSceneDraw,1> frame{model};
            const auto left=stereo_scene_eye(frame,0,kSbsEyeSeparation,kSbsConvergence);
            const auto right=stereo_scene_eye(frame,1,kSbsEyeSeparation,kSbsConvergence);
            require(left && right);
            const auto& l=std::get<GpuModelDraw>((*left)[0]);
            const auto& r=std::get<GpuModelDraw>((*right)[0]);
            const double disparity=(l.pose.x-r.pose.x)*256/depth+l.pose.vanish_x-r.pose.vanish_x;
            require(std::abs(disparity-256*kSbsEyeSeparation*(1/depth-1/kSbsConvergence))<1e-10);
            if(depth==512) require(disparity>=4);
            if(depth==kSbsConvergence) require(std::abs(disparity)<1e-10);
            if(depth==2048) require(disparity<0);
            require(l.pose.y==r.pose.y && l.pose.vanish_y==r.pose.vanish_y);
            require(l.pose.z==r.pose.z && l.pose.yaw==r.pose.yaw);
        }
        require(sbs_eye_x(0)==-sbs_eye_x(1));
    }
    {
        starfox::assets::Shape shape;
        GpuModelDraw draw{&shape};
        draw.identity=GpuModelIdentity{42,3,7,11,2};
        draw.pose.x=12;
        ModelMotionHistory history;
        const std::array<GpuSceneDraw,1> frame{draw};
        history.commit(frame,{10,1,800,448});
        auto next=draw;next.pose.x=16;
        const auto* old=history.previous(next,{11,1,800,448});
        require(old && old->x==12);
        require(!history.previous(next,{12,1,800,448})); // skipped presentation
        require(!history.previous(next,{11,2,800,448})); // scene/save-state epoch
        require(!history.previous(next,{11,1,1600,448}));
        ++next.identity->generation;
        require(!history.previous(next,{11,1,800,448}));
        next=draw;++next.pose.animation_frame;
        require(history.previous(next,{11,1,800,448})); // static geometry ignores global tick
        starfox::assets::Shape animated;animated.frames.resize(2);
        auto animated_draw=draw;animated_draw.shape=&animated;
        ModelMotionHistory animated_history;
        const std::array<GpuSceneDraw,1> animated_frame{animated_draw};
        animated_history.commit(animated_frame,{10,1,800,448});
        animated_draw.pose.animation_frame=1;
        require(!animated_history.previous(animated_draw,{11,1,800,448}));
        animated_draw.pose.animation_frame=2;
        require(animated_history.previous(animated_draw,{11,1,800,448})); // same wrapped geometry
        next=draw;++next.pose.explosion_progress;
        require(!history.previous(next,{11,1,800,448}));
        next=draw;next.settings.focal_length=128;
        require(!history.previous(next,{11,1,800,448}));
        const std::array<GpuSceneDraw,2> duplicate{draw,draw};
        const auto prepared=history.prepare(frame,{11,1,800,448});
        require(std::get<GpuModelDraw>(prepared[0]).previous_pose.has_value());
        const auto ambiguous=history.prepare(duplicate,{11,1,800,448});
        require(!std::get<GpuModelDraw>(ambiguous[0]).previous_pose);
        require(!std::get<GpuModelDraw>(ambiguous[1]).previous_pose);
        require(history.previous(draw,{11,1,800,448}));
        history.commit(duplicate,{11,1,800,448});
        require(!history.previous(draw,{12,1,800,448}));
        history.commit(frame,{12,1,800,448});
        history.commit({}, {13,1,800,448});
        require(!history.previous(draw,{14,1,800,448})); // disappeared
        history.commit(frame,{14,1,800,448});
        history.reset();
        require(!history.previous(draw,{15,1,800,448}));
    }
    {
        ScaledTextRenderer::ProjectedFrame text;
        text.pose.z=256;text.character_size=16;text.colour=114;
        std::array<std::uint16_t,16> glyph{};glyph[0]=0x8000;glyph[15]=1;
        text.glyphs.push_back(glyph);
        for(unsigned scale:{1U,2U,4U}) {
            Framebuffer pixels(224*scale,192*scale);pixels.set_draw_scale(scale);
            ScaledTextRenderer::draw_projected(text,pixels);
            require(pixels.draw_scale()==scale);
            require(pixels.pixels()[(88*scale)*(224*scale)+104*scale]==114);
            require(pixels.pixels()[(103*scale)*(224*scale)+119*scale]==114);
            require(pixels.pixels()[(96*scale)*(224*scale)+112*scale]==0);
        }
    }
    {
        ParticleRenderer::OwnerFrame frame;
        frame.pose.z=0;
        frame.alpha=.5;
        starfox::simulation::ParticleState trail;
        trail.life=1;trail.flags=4;trail.colour=2;trail.z=trail.previous_z=512;
        trail.previous_x=-10;
        frame.particles.push_back(trail);
        Framebuffer pixels(224,192);
        ParticleRenderer::draw_frame(frame,pixels);
        for(unsigned x=107;x<=110;++x) require(pixels.pixels()[96*224+x]==114);
        require(pixels.pixels()[96*224+111]==0);
        frame.particles[0].flags=0;frame.particles[0].previous_x=0;
        pixels.clear();ParticleRenderer::draw_frame(frame,pixels);
        require(pixels.pixels()[96*224+112]==114 && pixels.pixels()[97*224+113]==114);
        frame.pose.effect_clip_left=113;frame.pose.effect_clip_right=114;
        pixels.clear();ParticleRenderer::draw_frame(frame,pixels);
        require(pixels.pixels()[96*224+112]==0 && pixels.pixels()[96*224+113]==114);
        GpuSceneRecording recording;recording.reset(448,384);
        RasterCommands pending;pending.reset(448,384);
        auto inactive=trail;inactive.life=0;frame.particles.push_back(inactive);
        recording.append_particles(pending,{frame,2});recording.finish(pending);
        frame.particles.clear();
        require(recording.draws().size()==1);
        const auto& stored=std::get<GpuParticleDraw>(recording.draws()[0]);
        require(stored.frame.particles.size()==1);
        Framebuffer replay(448,384),expected(448,384);expected.set_draw_scale(2);
        ParticleRenderer::draw_frame(stored.frame,expected);recording.replay(replay,nullptr);
        require(replay.pixels()==expected.pixels() && replay.draw_scale()==1);
        const auto left=stereo_scene_eye(recording.draws(),0,6.4,512);
        const auto right=stereo_scene_eye(recording.draws(),1,6.4,512);
        require(left && right && std::get<GpuParticleDraw>((*left)[0]).eye_x<0
            && std::get<GpuParticleDraw>((*right)[0]).eye_x>0 && stored.eye_x==0);
        recording.reset(448,384);recording.append_particles(pending,{frame,2});
        require(recording.draws().empty());
    }
    {
        GpuDustDraw dust;
        dust.frame.points={{0,0,512},{10,10,2048}};
        dust.frame.matrix={32767,0,0,0,32767,0,0,0,32767};
        dust.scale=2;
        GpuSceneRecording recording;recording.reset(448,384);
        RasterCommands pending;pending.reset(448,384);
        recording.append_dust(pending,dust);recording.finish(pending);
        dust.frame.points.clear();
        Framebuffer frame(448,384);recording.replay(frame,nullptr);
        require(frame.pixels()[192*448+224]==112);
        const auto left=stereo_scene_eye(recording.draws(),0,6.4,512);
        const auto right=stereo_scene_eye(recording.draws(),1,6.4,512);
        require(left && right);
        require(std::get<GpuDustDraw>((*left)[0]).eye_x<0 && std::get<GpuDustDraw>((*right)[0]).eye_x>0);
        require(std::get<GpuDustDraw>((*left)[0]).frame.points.size()==2);
        require(std::get<GpuDustDraw>(recording.draws()[0]).eye_x==0);
    }
    {
        GpuGridDraw grid;
        grid.camera.y=512;grid.matrix={32767,0,0,0,32767,0,0,0,32767};
        grid.scale=2;
        const std::array<GpuSceneDraw,1> source{grid};
        const auto left=stereo_scene_eye(source,0,6.4,512);
        const auto right=stereo_scene_eye(source,1,6.4,512);
        require(left && right);
        require(std::get<GpuGridDraw>((*left)[0]).eye_x<0);
        require(std::get<GpuGridDraw>((*right)[0]).eye_x>0);
        require(std::get<GpuGridDraw>(source[0]).eye_x==0);
        GpuSceneRecording recording;recording.reset(800,448);
        RasterCommands pending;pending.reset(800,448);
        recording.append_grid(pending,grid);recording.finish(pending);
        Framebuffer frame(800,448);recording.replay(frame,nullptr);
        require(frame.draw_scale()==1);
        require(recording.draws().size()==1);
        const auto points=project_source_grid(grid.camera,grid.matrix,400,224);
        require(points.count>0);
        for(std::size_t i=0;i<points.count;++i) {
            const auto p=points.points[i];
            require(frame.pixels()[unsigned(p.y)*2*800+unsigned(p.x)*2]==126);
        }
        grid.lines=true;grid.line_start={-20,40};
        recording.reset(800,448);pending.reset(800,448);
        recording.append_grid(pending,grid);recording.finish(pending);
        recording.replay(frame,nullptr);
        Framebuffer expected(800,448);expected.set_draw_scale(2);
        DustRenderer::draw_grid_lines_frame({points,grid.line_start},expected);
        require(frame.pixels()==expected.pixels());
        const auto line_eye=stereo_scene_eye(recording.draws(),1,6.4,512);
        require(line_eye && std::get<GpuGridDraw>((*line_eye)[0]).line_start==grid.line_start);
    }
    {
        RasterCommands raster;
        GpuModelDraw model;
        require(!model.identity);
        model.identity=GpuModelIdentity{42,9,123,456,2};
        auto recycled=*model.identity;
        ++recycled.generation;
        require(recycled!=*model.identity);
        auto reshaped=*model.identity;
        ++reshaped.shape;
        require(reshaped!=*model.identity);
        model.pose.x=10;model.pose.z=100;model.pose.animation_frame=7;
        model.pose.source_depth=123;model.pose.use_source_lighting_state=true;
        model.previous_pose=model.pose;model.previous_pose->x=8;
        const std::array<GpuSceneDraw,3> frame{GpuRasterDraw{&raster},model,GpuRasterDraw{&raster}};
        const auto left=stereo_scene_eye(frame,0,6.4,100);
        const auto right=stereo_scene_eye(frame,1,6.4,100);
        require(left && right && left->size()==3 && right->size()==3);
        const auto& l=std::get<GpuModelDraw>((*left)[1]);
        const auto& r=std::get<GpuModelDraw>((*right)[1]);
        require(l.identity==model.identity && r.identity==model.identity);
        require(l.pose.x>model.pose.x && r.pose.x<model.pose.x);
        require(std::abs(l.pose.x*256/100+l.pose.vanish_x-r.pose.x*256/100-r.pose.vanish_x)<1e-12);
        require(l.pose.animation_frame==7 && r.pose.source_depth==123 && r.pose.use_source_lighting_state);
        require(l.pose.continuous_geometry && r.pose.subpixel_projection);
        require(l.previous_pose && r.previous_pose);
        require(std::abs((l.pose.x-l.previous_pose->x)-2)<1e-12);
        require(std::abs((r.pose.x-r.previous_pose->x)-2)<1e-12);
        require(l.previous_pose->vanish_x==l.pose.vanish_x && r.previous_pose->vanish_x==r.pose.vanish_x);
        require(l.previous_pose->continuous_geometry && r.previous_pose->subpixel_projection);
        require(std::get<GpuRasterDraw>((*left)[0]).commands==&raster);
        require(std::get<GpuRasterDraw>((*right)[2]).commands==&raster);
        require(std::get<GpuModelDraw>(frame[1]).pose.x==10);
        require(!stereo_scene_eye(frame,2,6.4,100));
        require(!stereo_scene_eye(frame,0,6.4,0));
    }
    for (const auto size : {std::array<unsigned,2>{1920,1080}, {3840,1080}, {256,224}}) {
        const auto off = stereo_output_layout(StereoOutput::off,size[0],size[1]);
        const auto half = stereo_output_layout(StereoOutput::half_sbs,size[0],size[1]);
        const auto full = stereo_output_layout(StereoOutput::full_sbs,size[0],size[1]);
        require(off && half && full);
        require(off->eye_count == 1 && half->eye_count == 2 && full->eye_count == 2);
        require(half->width == size[0] && full->width == size[0]*2);
        require(half->height == size[1] && full->height == size[1]);
        require(half->scene_aspect == full->scene_aspect && off->scene_aspect == full->scene_aspect);
        for (const auto& layout : {*half,*full}) {
            require(layout.eyes[0].x == 0 && layout.eyes[1].x == layout.eyes[0].width);
            require(layout.eyes[0].width == layout.eyes[1].width);
            require(layout.eyes[1].x + layout.eyes[1].width == layout.width);
        }
    }
    require(!stereo_output_layout(StereoOutput::half_sbs,1919,1080));
    require(!stereo_output_layout(StereoOutput::full_sbs,0xffffffffU,1080));
    require(!stereo_output_layout(StereoOutput::off,1920,0));
    require(!stereo_output_layout(static_cast<StereoOutput>(10),1920,1080));
    const auto sr=stereo_output_layout(StereoOutput::sr_platform,1920,1080);
    require(sr && sr->width==3840 && sr->eyes[0].width==1920 && sr->eyes[1].x==1920);
    for(const auto size:{std::array<unsigned,2>{256,224},{1921,1080},{3840,2160}}) {
        const auto full=stereo_output_layout(StereoOutput::full_sbs,size[0],size[1]);
        const auto cross=stereo_output_layout(StereoOutput::crossview,size[0],size[1]);
        require(full && cross && cross->width==full->width && cross->height==full->height);
        require(cross->scene_aspect==full->scene_aspect && cross->eye_count==2);
        require(cross->eyes[0].x==size[0] && cross->eyes[1].x==0);
        for(const auto& eye:cross->eyes) require(eye.width==size[0] && eye.height==size[1] && eye.y==0);
        require(!stereo_overlay(StereoOutput::crossview) && stereo_double_width(StereoOutput::crossview));
    }
    require(!stereo_output_layout(StereoOutput::crossview,0xffffffffU,1080));
    for(auto mode:{StereoOutput::interlaced,StereoOutput::interlaced_reversed,StereoOutput::anaglyph_red_cyan}) {
        const auto layout=stereo_output_layout(mode,1920,1081);
        require(layout && layout->width==1920 && layout->height==1081 && layout->eye_count==2);
        require(layout->eyes[0].width==1920 && layout->eyes[1].height==1081);
    }
    for(const auto mode:{StereoOutput::half_top_bottom,StereoOutput::full_top_bottom}) {
        const auto layout=stereo_output_layout(mode,1920,1080);
        require(layout && layout->eye_count==2 && layout->width==1920);
        require(layout->scene_aspect==1920./1080);
        require(layout->height==(mode==StereoOutput::half_top_bottom?1080U:2160U));
        require(layout->eyes[0].x==0 && layout->eyes[1].x==0);
        require(layout->eyes[0].width==1920 && layout->eyes[1].width==1920);
        require(layout->eyes[0].y==0 && layout->eyes[1].y==layout->eyes[0].height);
        require(layout->eyes[1].y+layout->eyes[1].height==layout->height);
    }
    require(!stereo_output_layout(StereoOutput::half_top_bottom,1920,1079));
    require(!stereo_output_layout(StereoOutput::full_top_bottom,1920,0xffffffffU));
    const auto eyes = stereo_eye_projections(6.4,100,1.5);
    require(eyes.has_value());
    const auto project = [&](unsigned eye, double x, double z) {
        return 1.5*(x-(*eyes)[eye].eye_x)/z+(*eyes)[eye].projection_offset_x;
    };
    for (double x : {-50.,0.,50.})
        require(std::abs(project(0,x,100)-project(1,x,100)) < 1e-12);
    require(project(0,0,50) > project(1,0,50));
    require(project(0,0,200) < project(1,0,200));
    require(!stereo_eye_projections(0,100,1.5));
    for(const double separation:{1.,16.,64.,512.}) for(const double convergence:{16.,1024.,4096.,65535.}) {
        const auto rig=stereo_eye_projections(separation,convergence,256.);
        require(rig.has_value());
        const auto disparity=[&](double depth) {
            return -256.*(*rig)[0].eye_x/depth+(*rig)[0].projection_offset_x
                +256.*(*rig)[1].eye_x/depth-(*rig)[1].projection_offset_x;
        };
        require(std::abs(disparity(convergence))<1e-10);
        require(disparity(convergence*.5)>0 && disparity(convergence*2)<0);
        require(std::abs(disparity(1e12)+256.*separation/convergence)<1e-6);
        const auto far_left=stereo_layer_displacement(0,separation,convergence,256.);
        const auto far_right=stereo_layer_displacement(1,separation,convergence,256.);
        require(far_left && far_right && *far_left<0 && *far_right>0);
        require(std::abs((*far_left-*far_right)-disparity(1e12))<1e-6);
        for(const double depth:{convergence*.5,convergence,convergence*2}) {
            const auto l=stereo_layer_displacement(0,separation,convergence,256.,depth);
            const auto r=stereo_layer_displacement(1,separation,convergence,256.,depth);
            require(l && r && std::abs((*l-*r)-disparity(depth))<1e-10);
        }
    }
    require(!stereo_layer_displacement(2,16,1024,256));
    require(!stereo_layer_displacement(0,16,1024,256,0));
    require(!stereo_layer_displacement(0,16,1024,256,std::numeric_limits<double>::infinity()));
    require(!stereo_layer_displacement(0,16,1024,std::numeric_limits<double>::quiet_NaN()));
    require(!stereo_eye_projections(6.4,-100,1.5));
    require(!stereo_eye_projections(6.4,100,std::numeric_limits<double>::infinity()));
    std::cout << "SBS layout and off-axis stereo projection checks pass\n";
}
