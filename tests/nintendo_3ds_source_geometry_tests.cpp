#include "starfox/platform/nintendo_3ds/pica_shapes.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
std::atomic<std::size_t> allocations{};
bool count_allocations{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F&& f,const char* message) {
    bool rejected=false;try {f();} catch(const std::exception&) {rejected=true;}
    require(rejected,message);
}
bool near(double a,double b) {return std::abs(a-b)<1.e-5;}
render::RenderPose pose() {render::RenderPose p;p.z=512;p.vanish_x=128;p.vanish_y=112;return p;}
assets::Shape quad() {
    assets::Shape shape;shape.vertices={{-16,-12,0},{16,-12,0},{16,12,0},{-16,12,0}};
    shape.faces.push_back({-1,3,{0,0,-127},{0,1,2,3}});return shape;
}
render::Palette256 palette() {
    render::Palette256 result;
    for(unsigned i=0;i<result.size();++i) result[i]={std::uint8_t(i),std::uint8_t(255-i),std::uint8_t(i^85),255};
    return result;
}
void native_vertices() {
    auto shape=quad();auto p=pose();
    render::SoftwareRenderer renderer;const auto prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives.size()==1 && prepared.primitives[0].vertices.size()==4,"Source polygon boundary retained");
    require(prepared.primitives[0].vertices[0].camera==std::array<double,3>{-16,-12,512},"Not projected screen pixels");
    require(prepared.primitives[0].material.colour.even==3,"Source fallback ink retained");
    PicaShapes output;const auto colours=palette();output.append(prepared,colours);
    const auto stereo=plan_frame(1,true,ScreenUse::world);
    const auto frame=output.frame(stereo);
    require(frame.vertices.size()==6 && frame.draws.size()==1 && frame.textures.empty(),"Actual camera polygon converted to PICA triangles");
    require(frame.vertices[0].position==Point3{-16,12,512},"Y-down source converted to Y-up PICA");
    require(frame.draws[0].model==pica_identity && frame.draws[0].depth_test,"World model, not a displaced mono image");
    const auto left=project(stereo,0,-16,12,512),right=project(stereo,1,-16,12,512);
    require(left && right && (*left)[0]!=(*right)[0],"Real object depth differs between eyes");
    std::vector<std::uint8_t> lower(bottom_width*screen_height*3);
    validate_pica_frame(frame,{lower,bottom_width,screen_height,bottom_width*3});
    p.x=10000;const auto outside=renderer.prepare_primitives(shape,p);
    require(!outside.primitives.empty(),"Mono viewport must not discard potential eye geometry");
    p=pose();p.vanish_x=144;p.vanish_y=104;output.clear();output.append(renderer.prepare_primitives(shape,p),colours);
    const auto shifted=output.frame(stereo);
    require(near(shifted.draws[0].model[0][2],16./256) && near(shifted.draws[0].model[1][2],8./256),"Source vanishing-point shifts retained without flattening depth");
}
void source_geometry() {
    auto shape=quad();auto p=pose();render::SoftwareRenderer renderer;
    shape.header.shift=2;shape.word_coordinates={true,false,false,false};p.scale=2;
    auto prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives[0].vertices[0].camera[0]==-16
        && prepared.primitives[0].vertices[1].camera[0]==128,"Word coordinates bypass byte shift/scale");
    auto frame=assets::ShapeFrame{};frame.vertices={{-3,-2,0},{3,-2,0},{3,2,0},{-3,2,0}};
    frame.word_coordinates={true,true,true,true};shape.frames.push_back(frame);
    prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives[0].vertices[0].camera[0]==-3,"Selected animation geometry retained");
    shape=quad();p=pose();p.use_rotation_matrix=true;
    p.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
    for(bool continuous:{false,true}) for(double phase:{0.,1.,3.5,12.}) {
        p.continuous_geometry=continuous;p.explosion_progress=phase>0?unsigned(std::ceil(phase)):0;
        p.explosion_phase=phase>0?phase:-1;
        render::Framebuffer raster(256,224);render::RenderDiagnostics diagnostics;
        renderer.draw(shape,p,raster,true,nullptr,nullptr,&diagnostics);
        prepared=renderer.prepare_primitives(shape,p);
        require(prepared.primitives.size()==diagnostics.polygons.size(),"Shared source draw/capture has same visible faces");
        for(unsigned i=0;i<prepared.primitives.size();++i) {
            const auto& points=prepared.primitives[i].vertices;
            require(points.size()==diagnostics.polygons[i].camera.size(),"Explosion boundary count matches source renderer");
            for(unsigned j=0;j<points.size();++j)
                require(points[j].camera==diagnostics.polygons[i].camera[j],"Camera/animation/destruction transform is shared exactly");
        }
    }
    shape=quad();shape.vertices[0].z=-600;p=pose();
    prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives.size()==1 && prepared.primitives[0].vertices.size()==5,"Partially behind polygon clipped, not discarded");
    for(const auto& vertex:prepared.primitives[0].vertices) require(vertex.camera[2]>=0,"Near-clipped boundary remains camera geometry");
    shape=quad();shape.faces[0].vertex_indices={0,1};p=pose();
    prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives.size()==1 && prepared.primitives[0].kind==render::ShapePrimitiveKind::line,"Line preserved before source viewport clipping");
    PicaShapes output;output.append(prepared,palette());
    require(output.frame(plan_frame(1,true,ScreenUse::world)).vertices.size()==6,"Source ink line becomes depth-correct ribbon");
    p.collapse_to_axis_line=true;shape.vertices[0].z=-16;shape.vertices[1].z=16;
    prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives.size()==1 && prepared.primitives[0].kind==render::ShapePrimitiveKind::line,"Axis-collapse source path not omitted");
}
void materials_and_sprites() {
    auto shape=quad();auto p=pose();render::SoftwareRenderer renderer;
    const auto colours=palette();PicaShapes output;
    p.force_colour=true;p.forced_colour=0xa3;
    auto prepared=renderer.prepare_primitives(shape,p);output.append(prepared,colours);
    auto frame=output.frame(plan_frame(1,true,ScreenUse::world));
    require(frame.draws[0].screen_dither && frame.textures.size()==1,"Two source inks have native screen-projected texture");
    const auto pixels=frame.textures[0].pixels;
    require(pixels[0]==0 && pixels[4]==255 && pixels[32]==255
        && frame.vertices[0].colour[0]==colours[3].r/255.F
        && frame.draws[0].dither_odd[0]==colours[10].r,"No averaged/flattened source material");
    require(frame.textures[0].repeat && frame.textures[0].width==8,"Native dither samples an 8x8 repeat");
    output.append(prepared,colours);frame=output.frame(frame.plan);
    require(frame.draws.size()==1 && frame.vertices.size()==12 && frame.textures.size()==1,"Adjacent equal passes merge, source inks deduplicate");
    output.clear();p=pose();shape.colour_words={0x4001};shape.faces[0].colour_id=0;
    assets::TextureImage texture;texture.descriptor=0x4001;texture.u_mask=7;texture.v_mask=7;
    texture.coordinates={{{0,0},{7,0},{7,7},{0,7}}};texture.texels.resize(64,3);texture.texels[0]=0;
    shape.textures.push_back(texture);p.texture_scroll_x=8;p.texture_scroll_y=-8;
    prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives[0].vertices[0].uv==std::array<double,2>{8,-8},"Source UV scroll retained");
    output.append(prepared,colours);frame=output.frame(plan_frame(.5F,true,ScreenUse::world));
    require(!frame.draws[0].screen_dither && frame.textures[0].repeat,"Ordinary model texture is not screen-dither sampled");
    require(frame.vertices[0].uv==std::array<float,2>{1,-1},"Wrapping source texel coordinates normalized");
    require(frame.textures[0].pixels[3]==0 && frame.textures[0].pixels[7]==255,"Texel zero is transparent, nonzero is opaque");
    shape.colour_materials={{{}, {0x4001,0xc009}}};p.colour_frame=1;
    prepared=renderer.prepare_primitives(shape,p);
    require(!prepared.primitives[0].material.texture && prepared.primitives[0].material.colour.even==9,"Animated texture-to-ink material is not cached stale");
    shape.colour_materials.clear();p=pose();shape.faces[0].sprite=true;shape.faces[0].vertex_indices={0};
    shape.textures[0].v_mask=3;shape.textures[0].texels.resize(32);
    prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives.size()==1 && prepared.primitives[0].kind==render::ShapePrimitiveKind::sprite,"Embedded model sprite retained");
    output.clear();output.append(prepared,colours);frame=output.frame(plan_frame(1,true,ScreenUse::world));
    require(frame.vertices.size()==6 && !frame.textures[0].repeat,"Billboard art is not wrapped along its edge");
    const auto height=frame.vertices[0].position[1]-frame.vertices[2].position[1];
    const auto width=frame.vertices[1].position[0]-frame.vertices[0].position[0];
    require(near(height,width*.5),"Non-square source sprite clips its art rather than stretching/clamping it");
    p.simple_scaled_sprite=true;p.simple_sprite_colour=0;p.simple_sprite_world_size=32;p.palette_override=12;
    p.effect_clip_left=128;p.effect_clip_right=136;
    prepared=renderer.prepare_primitives(shape,p);
    require(prepared.primitives.size()==1 && prepared.primitives[0].simple_sprite,"Whole-object asteroid/explosion sprite retained");
    output.clear();output.append(prepared,colours);frame=output.frame(plan_frame(1,true,ScreenUse::world));
    require(frame.vertices.size()==6 && frame.vertices[0].uv[0]==0 && frame.vertices[1].uv[0]==1
        && frame.draws[0].clip==PicaClip{200,0,208,240},"Simple sprite must retain both-eye geometry/UVs and use native LCD clipping");
    require(frame.textures[0].pixels[3]==0 && frame.textures[0].pixels[4]==colours[12].r,"Sprite palette override preserves transparent zero");
    p.z=64;require(renderer.prepare_primitives(shape,p).primitives.empty(),"Source simple sprite near no-op matches original");
}
void effect_windows() {
    auto prepared=render::SoftwareRenderer{}.prepare_primitives(quad(),pose());
    const auto plan=plan_frame(1,true,ScreenUse::world);PicaShapes output;
    output.append(prepared,palette());
    auto clipped=prepared;clipped.pose.effect_clip_left=64;clipped.pose.effect_clip_right=160;
    output.append(clipped,palette());
    const auto frame=output.frame(plan);
    require(frame.draws.size()==2 && !frame.draws[0].clip && frame.draws[1].clip==PicaClip{136,0,232,240},
        "Different source windows must not merge into one unclipped native draw");
    require(std::equal(frame.vertices.begin(),frame.vertices.begin()+6,frame.vertices.begin()+6),
        "Source mask pre-cropped mono geometry instead of clipping each native eye");
    require(pica_screen_scissor(*frame.draws[1].clip)==std::array<unsigned,4>{0,168,240,264},
        "LCD horizontal effect clip did not rotate into the actual 240x400 target");
    output.append(clipped,palette());
    require(output.frame(plan).draws.size()==2 && output.frame(plan).draws[1].count==12,
        "Adjacent equal effect windows failed native draw coalescing");
    clipped.pose.effect_clip_left=-2000;clipped.pose.effect_clip_right=-1000;
    output.append(clipped,palette());
    require(output.frame(plan).vertices.size()==18,"Fully outside effect window emitted unmasked geometry");
    clipped.pose.effect_clip_left=-100;clipped.pose.effect_clip_right=128;
    output.append(clipped,palette());
    require(output.frame(plan).draws.back().clip==PicaClip{0,0,200,240},"Partially outside source clip failed LCD clamping");
    std::vector<std::uint8_t> lower(bottom_width*screen_height*3);
    auto malformed=output.frame(plan);std::vector<PicaDraw> bad_draws(malformed.draws.begin(),malformed.draws.end());
    bad_draws[1].clip=PicaClip{200,0,100,240};
    malformed.draws=bad_draws;
    rejects([&]{validate_pica_frame(malformed,{lower,bottom_width,screen_height,bottom_width*3});},
        "Reversed source window accepted by presenter validation");
}
void painter_projection() {
    const auto colours=palette();render::SoftwareRenderer renderer;PicaShapes output;
    auto near_pose=pose();near_pose.z=256;
    auto far_pose=pose();far_pose.z=512;
    auto shape=quad();shape.faces[0].colour_id=3;
    const auto front=renderer.prepare_primitives(shape,near_pose);
    shape.faces[0].colour_id=7;
    const auto player=renderer.prepare_primitives(shape,far_pose);
    output.append(front,colours);
    output.append(player,colours,{128,112},nullptr,PicaShapeOrder::painter);
    output.append(player,colours,{128,112},nullptr,PicaShapeOrder::painter);
    output.append(front,colours);
    const auto plan=plan_frame(1,true,ScreenUse::world);
    const auto frame=output.frame(plan);
    require(frame.draws.size()==3 && frame.draws[1].count==12
        && frame.draws[0].depth_test && !frame.draws[1].depth_test && !frame.draws[1].depth_write
        && frame.draws[2].depth_test && frame.draws[2].depth_write,
        "Painter/depth passes merged across their boundary or leaked into the next source object");
    for(const auto& draw:frame.draws)
        require(draw.space==PicaSpace::world && draw.source_layer==1 && draw.model==pica_identity,
            "Late player lost finite eye projection/source colour ownership");
    // Independently sample the overlapping camera triangles at LCD pixel
    // centres. Reciprocal-Z plus per-draw depth policy, not the production
    // projection helper, determines whether the farther painter is visible.
    const auto sample=[&](unsigned eye,bool force_depth) {
        std::array<float,4> colour{};double nearest=std::numeric_limits<double>::infinity();
        for(unsigned d=0;d<2;++d) {
            const auto& draw=frame.draws[d];
            for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
                std::array<std::array<double,2>,3> p{};
                for(unsigned k=0;k<3;++k) {
                    const auto v=frame.vertices[i+k].position;
                    p[k]={200+plan.focal_x*(double(v[0])-plan.eyes[eye].x)/v[2]+plan.eyes[eye].projection_offset,
                        120-plan.focal_y*double(v[1])/v[2]};
                }
                const auto cross=[](auto a,auto b,auto q){return (b[0]-a[0])*(q[1]-a[1])-(b[1]-a[1])*(q[0]-a[0]);};
                const double area=cross(p[0],p[1],p[2]);
                const std::array point{200.5,120.5};
                const std::array w{cross(p[1],p[2],point)/area,cross(p[2],p[0],point)/area,cross(p[0],p[1],point)/area};
                if(std::any_of(w.begin(),w.end(),[](double value){return value<0;})) continue;
                double reciprocal=0;for(unsigned k=0;k<3;++k) reciprocal+=w[k]/frame.vertices[i+k].position[2];
                const double z=1/reciprocal;
                if((force_depth || draw.depth_test) && z>nearest) continue;
                colour=frame.vertices[i].colour;if(force_depth || draw.depth_write) nearest=z;
            }
        }
        return colour;
    };
    for(unsigned eye=0;eye<2;++eye) {
        require(sample(eye,false)==frame.vertices[frame.draws[1].first].colour,
            "Farther Controls painter is still hidden by a nearer weapons-demo triangle");
        require(sample(eye,true)==frame.vertices[frame.draws[0].first].colour,
            "Occlusion fixture does not distinguish ordinary world depth from painter order");
        require(pica_draw_matrix(plan,eye,frame.draws[1])==PicaProjection(plan,eye).rows(),
            "Painter pass became a flat menu image");
    }
    require(pica_draw_matrix(plan,0,frame.draws[1])!=pica_draw_matrix(plan,1,frame.draws[1]),
        "Late player stereo disparity was removed with its depth test");
    std::vector<std::uint8_t> lower(bottom_width*screen_height*3);
    validate_pica_frame(frame,{lower,bottom_width,screen_height,bottom_width*3});
    auto invalid=player;invalid.primitives.push_back(invalid.primitives.front());
    invalid.primitives.back().vertices[0].camera[0]=std::numeric_limits<double>::quiet_NaN();
    const auto before=std::vector<PicaVertex>(frame.vertices.begin(),frame.vertices.end());
    rejects([&]{output.append(invalid,colours,{128,112},nullptr,PicaShapeOrder::painter);},
        "Malformed painter shape must roll back its new draw and vertices");
    const auto retained=output.frame(plan);
    require(std::equal(before.begin(),before.end(),retained.vertices.begin()) && retained.draws.size()==3
        && retained.draws.back().depth_test && retained.draws.back().count==6,
        "Failed painter append changed the previous complete depth stream");
    rejects([&]{output.append(player,colours,{128,112},nullptr,static_cast<PicaShapeOrder>(99));},
        "Unknown source painter policy accepted");
}
void scene_windows() {
    render::SoftwareRenderer renderer;auto prepared=renderer.prepare_primitives(quad(),pose());
    // Oversized finite camera geometry intersects every LCD edge. A CPU mono
    // reject would hide valid panel ink; no scissor would overwrite its labels.
    const std::array<std::array<double,3>,4> points{{{-600,-400,512},{600,-400,512},
        {600,400,512},{-600,400,512}}};
    for(unsigned i=0;i<4;++i) prepared.primitives[0].vertices[i].camera=points[i];
    const PicaClip panel{96,32,208,120};PicaShapes owner;
    owner.append(prepared,palette());
    owner.append(prepared,palette(),{128,112},nullptr,PicaShapeOrder::painter,panel);
    for(float slider:{0.F,.5F,1.F}) {
        const auto plan=plan_frame(slider,true,ScreenUse::world);const auto frame=owner.frame(plan);
        require(frame.draws.size()==2 && frame.draws[1].clip==panel && !frame.draws[1].depth_test,
            "Scene window lost its isolated painter/depth policy");
        require(std::equal(frame.vertices.begin(),frame.vertices.begin()+6,frame.vertices.begin()+6),
            "Scene window altered finite camera vertices instead of scissoring each eye");
        const auto& draw=frame.draws[1];
        for(unsigned eye=0;eye<plan.eye_count;++eye) {
            std::array<std::array<double,2>,6> projected{};
            for(unsigned i=0;i<6;++i) {
                const auto v=frame.vertices[draw.first+i].position;
                projected[i]={200+plan.focal_x*(double(v[0])-plan.eyes[eye].x)/v[2]+plan.eyes[eye].projection_offset,
                    120-plan.focal_y*double(v[1])/v[2]};
            }
            const auto cross=[](auto a,auto b,auto s){return (b[0]-a[0])*(s[1]-a[1])-(b[1]-a[1])*(s[0]-a[0]);};
            unsigned occupied=0,unclipped=0;
            for(int y=0;y<240;++y) for(int x=0;x<400;++x) {
                const std::array sample{double(x)+.5,double(y)+.5};bool ink=false;
                for(unsigned i:{0U,3U}) {
                    const auto a=projected[i],b=projected[i+1],c=projected[i+2];
                    const double area=cross(a,b,c);
                    ink|=cross(b,c,sample)/area>=0 && cross(c,a,sample)/area>=0 && cross(a,b,sample)/area>=0;
                }
                unclipped+=ink;
                const auto scissor=pica_screen_scissor(*draw.clip);
                // Independent clockwise framebuffer address, sampled at pixel
                // centres; Citro3D's target is 240x400 with exclusive bounds.
                const unsigned rx=239-unsigned(y),ry=399-unsigned(x);
                ink&=rx>=scissor[0] && rx<scissor[2] && ry>=scissor[1] && ry<scissor[3];
                require(ink==(x>=96 && x<208 && y>=32 && y<120),
                    "Native scene scissor misses/overwrites Controls panel pixels in an eye");
                occupied+=ink;
            }
            require(unclipped==400*240 && occupied==112*88,"Scene-window fixture did not exercise all four guard edges");
        }
    }
    auto authored=prepared;authored.pose.effect_clip_left=40;authored.pose.effect_clip_right=80;
    owner.append(authored,palette(),{128,112},nullptr,PicaShapeOrder::depth,panel);
    const auto plan=plan_frame(1,true,ScreenUse::world);
    require(owner.frame(plan).draws.back().clip==PicaClip{112,32,152,120},
        "Scene window replaced/widened the authored horizontal effect clip");
    const auto before=owner.frame(plan).vertices.size();
    authored.pose.effect_clip_left=160;authored.pose.effect_clip_right=180;
    owner.append(authored,palette(),{128,112},nullptr,PicaShapeOrder::depth,panel);
    require(owner.frame(plan).vertices.size()==before,"Disjoint authored/scene windows emitted ink");
    rejects([&]{owner.append(prepared,palette(),{128,112},nullptr,PicaShapeOrder::depth,PicaClip{-1,0,100,120});},
        "Invalid native scene clip was accepted");
    require(owner.frame(plan).vertices.size()==before,"Invalid scene clip damaged previously completed geometry");
    owner.append(prepared,palette());
    require(!owner.frame(plan).draws.back().clip,"Controls window leaked into a subsequent world append");
}
void rollback_and_budget() {
    auto shape=quad();auto p=pose();render::SoftwareRenderer renderer;
    const auto colours=palette();PicaShapes output;auto prepared=renderer.prepare_primitives(shape,p);
    output.append(prepared,colours);
    const auto plan=plan_frame(1,true,ScreenUse::world);
    auto before=output.frame(plan);const std::vector<PicaVertex> saved(before.vertices.begin(),before.vertices.end());
    prepared.primitives.push_back(prepared.primitives[0]);prepared.primitives.back().vertices[0].camera[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{output.append(prepared,colours);},"Malformed later primitive rejects full shape transaction");
    auto after=output.frame(plan);
    require(std::vector<PicaVertex>(after.vertices.begin(),after.vertices.end())==saved && after.draws[0].count==6,"Merged draw count and geometry rolled back together");
    prepared=renderer.prepare_primitives(shape,p);prepared.pose.wave_mode=1;
    rejects([&]{output.append(prepared,colours);},"EX sparse/wave geometry requires its active immutable eye plan");
    prepared.pose.wave_mode=0;
    rejects([&]{output.append(prepared,std::span(colours).first(2));},"Missing palette rejected, not substituted");
    prepared.focal_length=0;rejects([&]{output.append(prepared,colours);},"Invalid focal rejected");
    prepared=renderer.prepare_primitives(shape,p);
    for(unsigned i=0;i<(pica_vertex_limit/6)-1;++i) output.append(prepared,colours);
    before=output.frame(plan);require(before.vertices.size()==pica_vertex_limit,"Native bounded geometry filled exactly");
    rejects([&]{output.append(prepared,colours);},"Over-budget faces fail instead of disappearing");
    require(output.frame(plan).vertices.size()==pica_vertex_limit,"Budget failure retained complete previous frame");
    output.clear();require(output.frame(plan).vertices.empty(),"Explicit frame reset clears geometry");
}
void warmed_geometry_storage() {
    render::SoftwareRenderer renderer;
    auto prepared=renderer.prepare_primitives(quad(),pose());
    auto polygon=prepared.primitives.front();
    polygon.vertices.resize(7);
    for(unsigned i=0;i<7;++i) polygon.vertices[i].camera={double(i*9)-24,double((i*i)%17)-8,512};
    prepared.primitives.assign(128,polygon);
    auto line=polygon;line.kind=render::ShapePrimitiveKind::line;line.vertices.resize(2);
    line.vertices[0].camera={1,2,512};line.vertices[1].camera={1,2,512}; // Collapsed source ribbon.
    prepared.primitives.push_back(line);
    line.vertices[0].camera={-1,-2,-4};line.vertices[1].camera={12,9,128}; // Near-clipped ribbon.
    prepared.primitives.push_back(line);
    const auto colours=palette();const auto plan=plan_frame(1,true,ScreenUse::world);
    std::array<PicaShapes,2> owners;
    for(auto& owner:owners) owner.append(prepared,colours);
    constexpr unsigned count=128*15+12;
    for(auto& owner:owners) {
        const auto frame=owner.frame(plan);
        require(frame.vertices.size()==count && frame.draws.size()==1,"Fan/ribbon warm-up changed geometry or merging");
        unsigned at=0;
        for(unsigned corner=1;corner+1<polygon.vertices.size();++corner)
            for(unsigned index:{0U,corner,corner+1}) {
                const auto& source=polygon.vertices[index].camera;
                require(frame.vertices[at++].position==Point3{float(source[0]),float(-source[1]),float(source[2])},
                    "Direct fan submission changed source vertex order or camera coordinates");
            }
    }
    const std::array addresses{owners[0].frame(plan).vertices.data(),owners[1].frame(plan).vertices.data()};
    allocations=0;count_allocations=true;
    for(unsigned phase=0;phase<180;++phase) {
        prepared.primitives[0].vertices[0].camera[0]=double(phase)*.125;
        auto& owner=owners[phase&1];owner.clear();owner.append(prepared,colours);
        const auto frame=owner.frame(plan);
        require(frame.vertices.size()==count && frame.vertices.data()==addresses[phase&1]
            && frame.vertices[0].position[0]==float(phase)*.125F,
            "Warmed scene storage was replaced, lost geometry or retained old coordinates");
    }
    count_allocations=false;
    std::cout<<"Warmed native fan/ribbon conversion: "<<allocations<<" allocations over 180 changed scenes\n";
    require(allocations==0,"Warmed polygon/fan/ribbon conversion still allocates per face or scene");
}
void texture_resources() {
    const auto colours=palette();render::SoftwareRenderer renderer;PicaShapes output;
    auto prepared=renderer.prepare_primitives(quad(),pose());
    const auto primitive=prepared.primitives.front();prepared.primitives.clear();
    std::vector<assets::TextureImage> artwork(64);
    for(unsigned i=0;i<artwork.size();++i) {
        auto& art=artwork[i];art.u_mask=art.v_mask=7;art.texels.resize(64,std::uint8_t(i+1));
        auto face=primitive;face.material.texture=&art;prepared.primitives.push_back(std::move(face));
    }
    output.append(prepared,colours);const auto plan=plan_frame(1,true,ScreenUse::world);
    auto frame=output.frame(plan);
    require(frame.textures.size()==64 && frame.draws.size()==64 && frame.vertices.size()==384,
        "Source texture stress model retains every distinct texture and ordered draw");
    std::vector<std::uint8_t> lower(bottom_width*screen_height*3);
    validate_pica_frame(frame,{lower,bottom_width,screen_height,bottom_width*3});
    unsigned bytes=512*256*4;
    for(const auto image:frame.textures) bytes+=pica_texture_layout(image).bytes;
    require(bytes<pica_texture_budget,"Texture count does not bypass actual resident-byte budget");
    output.clear();prepared.primitives.clear();
    for(unsigned i=1;i<255;++i) {
        auto face=primitive;face.material.colour={std::uint8_t(i),std::uint8_t(i+1),true};
        prepared.primitives.push_back(std::move(face));
    }
    output.append(prepared,colours);frame=output.frame(plan);
    require(frame.textures.size()==1 && frame.draws.size()==254,
        "Hundreds of animated source ink pairs use one parity mask, not hundreds of textures");
    validate_pica_frame(frame,{lower,bottom_width,screen_height,bottom_width*3});
}
void projected_dither() {
    const auto plan=plan_frame(1,true,ScreenUse::world);
    const std::array<Point3,3> points{{{-96,42,220},{128,-54,750},{-8,72,1900}}};
    // Independently interpolate the homogeneous texture varyings with the
    // GPU's reciprocal-W weights, then apply texture Q projection. A normal
    // UV checker changes density with depth; this must remain LCD aligned.
    for(unsigned eye=0;eye<2;++eye) for(unsigned a=1;a<10;++a) for(unsigned b=1;b<10-a;++b) {
        const std::array<double,3> bary{a/10.,b/10.,1-(a+b)/10.};
        double u=0,v=0,q=0,screen_x=0,screen_y=0;
        for(unsigned i=0;i<3;++i) {
            const auto c=*PicaProjection(plan,eye).clip_position(points[i]);
            const double projected_u=(c[3]-c[1])*25,projected_v=(c[3]+c[0])*15;
            u+=bary[i]*projected_u/c[3];v+=bary[i]*projected_v/c[3];q+=bary[i];
            screen_x+=bary[i]*200*(1-c[1]/c[3]);screen_y+=bary[i]*120*(1-c[0]/c[3]);
        }
        require(near(u/q,screen_x/8) && near(v/q,(screen_height-screen_y)/8),"Projected checker remains one ink per LCD pixel across sloping stereo triangles");
    }
}
} // namespace
// Observe only warmed preparation, not fixture creation or the independent fan oracle.
#if defined(_MSC_VER)
#define STARFOX_ALLOCATION_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define STARFOX_ALLOCATION_NOINLINE __attribute__((noinline))
#else
#define STARFOX_ALLOCATION_NOINLINE
#endif
// Keep test allocation boundaries visible. Inlining these replacement hooks
// also triggers GCC's false malloc/new provenance warning in unrelated fixtures.
STARFOX_ALLOCATION_NOINLINE
void* operator new(std::size_t count) {
    if(count_allocations) ++allocations;
    if(auto* memory=std::malloc(std::max(count,std::size_t{1}))) return memory;
    throw std::bad_alloc{};
}
STARFOX_ALLOCATION_NOINLINE void* operator new[](std::size_t count) {return ::operator new(count);}
STARFOX_ALLOCATION_NOINLINE void operator delete(void* memory) noexcept {std::free(memory);}
STARFOX_ALLOCATION_NOINLINE void operator delete[](void* memory) noexcept {std::free(memory);}
STARFOX_ALLOCATION_NOINLINE void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
STARFOX_ALLOCATION_NOINLINE void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}
#undef STARFOX_ALLOCATION_NOINLINE
int main() try {
    native_vertices();source_geometry();materials_and_sprites();effect_windows();painter_projection();scene_windows();rollback_and_budget();warmed_geometry_storage();texture_resources();projected_dither();
    std::cout<<"3DS shared source geometry/material conversion: "<<checks<<" checks passed\n";
} catch(const std::exception& error) {count_allocations=false;std::cerr<<error.what()<<'\n';return 1;}
