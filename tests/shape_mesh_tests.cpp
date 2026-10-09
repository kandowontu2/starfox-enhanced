#include "starfox/vr/shape_mesh.hpp"
#include "starfox/vr/shape_batch.hpp"
#include "starfox/vr/source_span_layout.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <iostream>
#include <algorithm>
#include <limits>
#include <stdexcept>
using namespace starfox;
namespace {void require(bool value) {if(!value) throw std::runtime_error("Shape mesh assertion failed");}}
int main(int argc,char** argv) try {
    {
        vr::SourceSpanSettings settings;
        require(!vr::source_span_buffer_sizes(settings));
        settings.count=settings.polygon_count=1;settings.width=settings.height=32;
        auto sizes=vr::source_span_buffer_sizes(settings);require(sizes.has_value());
        require(*sizes==std::array<uint64_t,7>{2064,96,4,8,3072,4,64});
        {
            auto occurrence=settings;occurrence.ordered_mode=2;occurrence.count=2;
            require(!vr::source_span_buffer_sizes(occurrence));
            occurrence.polygon_count=2;occurrence.order_first=UINT32_MAX;
            const auto expanded=vr::source_span_buffer_sizes(occurrence);
            require(expanded && (*expanded)[2]==4); // source face order is not read
            occurrence.ordered_mode=3;require(!vr::source_span_buffer_sizes(occurrence));
        }
        settings.mask_enabled=1;settings.mask_stride=4;
        require((*vr::source_span_buffer_sizes(settings))[5]==128);
        settings.mask_stride=3;require(!vr::source_span_buffer_sizes(settings));
        settings.mask_stride=4;settings.mask_offset=1;require(!vr::source_span_buffer_sizes(settings));
        settings.mask_offset=0;settings.width=33;require(!vr::source_span_buffer_sizes(settings));
        settings.mask_stride=8;require(vr::source_span_buffer_sizes(settings).has_value());
        settings.ordered_mode=1;settings.order_first=UINT32_MAX;
        require(!vr::source_span_buffer_sizes(settings)); // no uint32 wraparound
        settings.order_first=0;settings.order_tree=UINT32_MAX;
        require(!vr::source_span_buffer_sizes(settings));
        settings.order_tree=0;settings.count=settings.polygon_count=65535U*32U;
        require(!vr::source_span_buffer_sizes(settings)); // allocation budget
        std::vector<uint32_t> payload(4+24+1);
        payload[0]=4;payload[1]=28;payload[2]=1;
        payload[4+2]=8;payload[4+3]=1;
        require(vr::source_span_payload_valid(payload,0,8));
        payload[4+23]=4;payload[4+9]=4;payload[4+10]=1;payload[28]=255;
        require(vr::source_span_payload_valid(payload,0,8));
        payload[4+8]=4;require(!vr::source_span_payload_valid(payload,0,8));
        payload[4+8]=0;payload[4+9]=3;require(!vr::source_span_payload_valid(payload,0,8));
        payload[4+9]=4;payload[4+23]=8;require(!vr::source_span_payload_valid(payload,0,8));
        payload[4+23]=0;payload[0]=UINT32_MAX;require(!vr::source_span_payload_valid(payload,0,8));
    }
    assets::Shape shape;shape.header.shift=3;
    shape.vertices={{1,-2,3},{4,5,-6},{7,8,9}};shape.word_coordinates={false,true,false};
    shape.faces={{-1,2,{3,4,5},{0,1,2}},{0,7,{1,2,3},{0,2}},{1,8,{4,5,6},{1},true,9,10}};
    vr::ShapeMesh mesh;std::string error;
    require(vr::decode_shape_mesh(shape,0,2,mesh,error));
    require(mesh.vertices[0].position==std::array<float,3>{16,-32,48});
    require(mesh.vertices[1].position==std::array<float,3>{4,5,-6});
    require(mesh.faces.size()==3 && mesh.faces[1].indices.size()==2 && mesh.faces[2].sprite);
    require(mesh.faces[2].sprite_size==10 && mesh.faces[2].sprite_visibility_parameter==9);
    require(mesh.faces[0].normal==assets::Vec3i{3,4,5} && mesh.faces[0].visibility_index==-1);
    vr::ShapeBatch batch;render::RenderPose pose;render::Palette256 palette{};
    {
        auto positioned=pose;positioned.x=256;positioned.y=128;positioned.z=512;
        const auto identity=vr::game_model_matrix(positioned);
        require(identity && (*identity)[0]==1.F/256 && (*identity)[5]==-1.F/256 && (*identity)[10]==-1.F/256);
        require((*identity)[12]==1 && (*identity)[13]==-.5F && (*identity)[14]==-2);
        positioned.scale=8;require(vr::game_model_matrix(positioned)==identity); // Scale belongs to mesh byte vertices only.
        positioned.use_rotation_matrix=true;
        positioned.rotation_matrix={0,32767,0,-32767,0,0,0,0,32767};
        const auto q15=vr::game_model_matrix(positioned,1);
        require(q15 && (*q15)[1]==-32767.F/32768 && (*q15)[4]==-32767.F/32768);
        positioned.use_rotation_matrix=false;positioned.roll=16384;
        const auto rolled=vr::game_model_matrix(positioned,1);
        require(rolled && std::abs((*rolled)[0])<1e-6 && std::abs((*rolled)[1]+1)<1e-6 && std::abs((*rolled)[4]+1)<1e-6);
        positioned.pitch=positioned.yaw=16384;
        const auto composed=vr::game_model_matrix(positioned,1);
        require(composed && std::abs((*composed)[2]-1)<1e-6 && std::abs((*composed)[5]+1)<1e-6 && std::abs((*composed)[8]-1)<1e-6);
        require(!vr::game_model_matrix(positioned,0));
        positioned.x=std::numeric_limits<double>::infinity();require(!vr::game_model_matrix(positioned));
    }
    palette[2]={255,128,64,255};
    require(vr::build_shape_batch(shape,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
    require(batch.vertices.size()==3 && batch.ranges.size()==1 && batch.deferred.empty() && batch.source_noops.size()==1);
    require(batch.line_vertices.size()==2 && batch.line_ranges.size()==1);
    require(batch.line_vertices[0].position[0]==16 && batch.line_vertices[1].position[0]==112);
    require(batch.vertices[0].color[0]==1 && batch.vertices[0].position[0]==16);
    require(batch.source_noops[0].reason==vr::SourceNoop::untextured_sprite);
    require(batch.polygon_vertices.empty() && batch.polygon_ranges.empty());
    {
        auto quad=shape;
        quad.vertices.push_back({-2,5,8});quad.word_coordinates.push_back(false);
        quad.faces[0].vertex_indices.push_back(3);
        vr::ShapeMesh quad_mesh;vr::ShapeBatch retained;
        require(vr::decode_shape_mesh(quad,0,2,quad_mesh,error));
        require(vr::build_shape_batch(quad,quad_mesh,pose,0,{127,127,127},palette,0,1,false,retained,error,true));
        require(retained.vertices.size()==6 && retained.polygon_vertices.size()==4);
        require(retained.polygon_ranges.size()==1 && retained.polygon_ranges[0].vertex_count==4);
        for(unsigned i=0;i<4;++i) for(unsigned axis=0;axis<3;++axis)
            require(retained.polygon_vertices[i].position[axis]==quad_mesh.vertices[i].position[axis]);
    }
    shape.visibilities={{0,1,2}};shape.faces[0].visibility_index=0;
    require(vr::decode_shape_mesh(shape,0,2,mesh,error));
    require(vr::build_shape_batch(shape,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
    require(batch.vertices[0].visibility_enabled==1 && batch.vertices[0].visibility_a[0]==16);
    require(batch.vertices[0].visibility_b[0]==4); // Word coordinates remain unscaled in visibility too.
    shape.visibilities[0].c=99;
    require(vr::build_shape_batch(shape,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
    require(batch.vertices.empty() && batch.line_vertices.empty());
    require(batch.source_noops[0].reason==vr::SourceNoop::invalid_visibility);
    require(batch.source_noops[1].reason==vr::SourceNoop::invalid_visibility);
    shape.visibilities[0].c=2;
    auto bsp_shape=shape;
    bsp_shape.bsp_root_address=100;
    bsp_shape.face_batches={{200,{shape.faces[0]}},{201,{shape.faces[1],shape.faces[2]}}};
    bsp_shape.bsp_nodes={{100,0,200,101,0}};
    bsp_shape.bsp_leaves={{101,201}};
    std::vector<vr::ShapeFaceInstance> instances;
    require(vr::shape_face_instances(bsp_shape,instances,error));
    require(instances.size()==3 && instances[0].face==1 && instances[0].group_visibility==-1);
    require(instances[2].face==0 && instances[2].group_visibility==0);
    require(vr::build_shape_batch(bsp_shape,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
    require(batch.vertices[0].group_enabled==1 && batch.vertices[0].group_b[0]==4);
    {
        auto occurrences=bsp_shape;
        occurrences.bsp_leaves[0].face_batch_address=200;
        vr::ShapeBatch retained;
        require(vr::build_shape_batch(occurrences,mesh,pose,0,{127,127,127},palette,0,1,false,retained,error,true));
        // The same source face occurs both outside and inside a BSP group.
        // Preserve both occurrences and their independent visibility payloads.
        require(retained.polygon_ranges.size()==2 && retained.polygon_vertices.size()==6);
        require(retained.polygon_ranges[0].source_face==0 && retained.polygon_ranges[1].source_face==0);
        require(retained.polygon_ranges[0].first_vertex==0 && retained.polygon_ranges[1].first_vertex==3);
        require(retained.polygon_vertices[0].group_enabled==0);
        require(retained.polygon_vertices[3].group_enabled==1);
        require(retained.polygon_vertices[3].group_b[0]==4);
        auto invalid_mesh=mesh;invalid_mesh.faces[0].indices[0]=999;
        require(!vr::build_shape_batch(occurrences,invalid_mesh,pose,0,{127,127,127},palette,0,1,false,retained,error,true));
        require(retained.polygon_ranges.size()==2 && retained.polygon_vertices.size()==6);
    }
    bsp_shape.visibilities[0].c=99;
    require(vr::build_shape_batch(bsp_shape,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
    require(batch.vertices.empty() && batch.source_noops[0].reason==vr::SourceNoop::invalid_visibility);
    bsp_shape.visibilities[0].c=2;
    bsp_shape.bsp_nodes[0].visibility_index=99;
    require(vr::shape_face_instances(bsp_shape,instances,error) && instances.size()==2);
    bsp_shape.bsp_nodes[0].alternate_address=100;
    require(!vr::shape_face_instances(bsp_shape,instances,error) && instances.size()==2);
    bsp_shape.bsp_nodes[0].alternate_address=999;
    require(!vr::shape_face_instances(bsp_shape,instances,error));
    bsp_shape.bsp_nodes[0].alternate_address=0;
    bsp_shape.face_batches[0].faces[0].colour_id=99;
    require(!vr::shape_face_instances(bsp_shape,instances,error));
    assets::Shape repeated;repeated.bsp_root_address=1;
    for(unsigned i=1;i<=18;++i) repeated.bsp_nodes.push_back({i,0,0,i==18?0:i+1,i==18?0:i+1});
    require(!vr::shape_face_instances(repeated,instances,error));
    require(error=="BSP traversal limit exceeded");
    auto textured_shape=shape;
    textured_shape.faces={shape.faces[0],shape.faces[0]};
    textured_shape.colour_words={0,0,0x4000};
    assets::TextureImage texture;
    texture.descriptor=0x4000;texture.u_mask=texture.v_mask=1;
    texture.coordinates={{{0,0},{2,0},{0,2},{2,2}}};texture.texels={0,1,2,1};
    textured_shape.textures={texture};
    vr::ShapeMesh textured_mesh;vr::ShapeBatch textured_batch;
    require(vr::decode_shape_mesh(textured_shape,0,2,textured_mesh,error));
    auto texture_pose=pose;texture_pose.texture_scroll_x=-1;texture_pose.texture_scroll_y=3;
    palette[0]={12,34,56,255};palette[255]={0,0,0,255};
    require(vr::build_shape_batch(textured_shape,textured_mesh,texture_pose,0,{127,127,127},palette,254,1,true,textured_batch,error));
    require(textured_batch.vertices.size()==6 && textured_batch.deferred.empty() && textured_batch.texels.size()==4);
    require(textured_batch.texels[0]==0 && textured_batch.texels[1]==0xff000000U && textured_batch.texels[2]==0xff38220cU);
    require(textured_batch.vertices[0].uv[0]==-1 && textured_batch.vertices[1].uv[0]==1 && textured_batch.vertices[0].uv[1]==3);
    require(textured_batch.vertices[0].texture[3]==3 && textured_batch.vertices[3].texture[0]==0);
    for(unsigned effect=0;effect<6;++effect) {
        auto effected=texture_pose;
        if(effect<2) effected.wireframe_mode=effect+1;
        if(effect==2) effected.wobble_mode=1;
        if(effect==3) effected.wobble_mode=2;
        if(effect==4) effected.wave_mode=true;
        if(effect==5) effected.cel_mode=true;
        vr::ShapeBatch unaffected;
        require(vr::build_shape_batch(textured_shape,textured_mesh,effected,0,{127,127,127},palette,254,1,true,unaffected,error));
        require(unaffected.vertices.size()==6 && unaffected.texels==textured_batch.texels);
        require(unaffected.vertices[0].texture[3]==3 && unaffected.vertices[0].uv[0]==-1);
        // A solid polygon must still fail rather than silently lose its effect.
        require(!vr::build_shape_batch(shape,mesh,effected,0,{127,127,127},palette,0,1,false,unaffected,error));
        require(unaffected.vertices.size()==6);
        // Opt-in span preparation preserves the complete solid boundary and
        // unaffected source lines, without an incorrect filled fan beneath it.
        vr::ShapeBatch span_batch;
        require(vr::build_shape_batch(shape,mesh,effected,0,{127,127,127},palette,0,1,false,span_batch,error,true));
        require(span_batch.vertices.empty() && span_batch.ranges.empty());
        require(span_batch.polygon_ranges.size()==1 && span_batch.polygon_vertices.size()==3);
        require(span_batch.polygon_ranges[0].source_face==0);
        require(span_batch.line_vertices.size()==2);
        require(vr::build_shape_batch(textured_shape,textured_mesh,effected,0,{127,127,127},palette,254,1,true,span_batch,error,true));
        require(span_batch.vertices.size()==6 && span_batch.polygon_vertices.empty());
        require(span_batch.texels==textured_batch.texels);
        auto line_shape=shape;line_shape.faces={shape.faces[1]};
        vr::ShapeMesh line_mesh;require(vr::decode_shape_mesh(line_shape,0,2,line_mesh,error));
        require(vr::build_shape_batch(line_shape,line_mesh,effected,0,{127,127,127},palette,0,1,false,unaffected,error));
        require(unaffected.vertices.empty() && unaffected.line_vertices.size()==2);
    }
    textured_shape.textures[0].texels.pop_back();
    require(!vr::build_shape_batch(textured_shape,textured_mesh,texture_pose,0,{127,127,127},palette,254,1,true,textured_batch,error));
    require(textured_batch.vertices.size()==6 && textured_batch.texels.size()==4);
    auto sprite_shape=shape;sprite_shape.faces={shape.faces[2]};
    sprite_shape.colour_words.resize(9);sprite_shape.colour_words[8]=0x4000;
    texture.u_mask=63;texture.v_mask=31;texture.texels.assign(64*32,1);
    sprite_shape.textures={texture};
    require(vr::decode_shape_mesh(sprite_shape,0,2,textured_mesh,error));
    require(vr::build_shape_batch(sprite_shape,textured_mesh,pose,0,{127,127,127},palette,0,1,false,textured_batch,error));
    require(textured_batch.deferred.empty() && textured_batch.vertices.size()==6);
    require(textured_batch.vertices[0].position[0]==4 && textured_batch.vertices[0].billboard[0]==-64);
    require(textured_batch.vertices[2].billboard[1]==-64 && textured_batch.vertices[2].uv[1]==64);
    require(textured_batch.vertices[0].texture[2]==31 && textured_batch.vertices[0].texture[3]==5);
    auto sprite_effects=pose;sprite_effects.wireframe_mode=2;
    sprite_effects.wobble_mode=3;sprite_effects.wave_mode=true;sprite_effects.cel_mode=true;
    require(vr::build_shape_batch(sprite_shape,textured_mesh,sprite_effects,0,{127,127,127},palette,0,1,false,textured_batch,error));
    require(textured_batch.vertices.size()==6 && textured_batch.vertices[0].texture[3]==5);
    require(textured_batch.vertices[0].billboard[0]==-64);
    pose.explosion_progress=1;
    require(vr::build_shape_batch(shape,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
    require(batch.vertices.size()==3 && batch.vertices[0].visibility_enabled==2);pose.explosion_progress=0;
    shape.frames.resize(2);
    shape.frames[0].vertices=shape.vertices;shape.frames[0].word_coordinates=shape.word_coordinates;
    shape.frames[1]=shape.frames[0];shape.frames[1].vertices[0].x=10;
    require(vr::decode_shape_mesh(shape,3,2,mesh,error) && mesh.vertices[0].position[0]==160);
    shape.faces[0].vertex_indices[0]=99;
    require(!vr::decode_shape_mesh(shape,0,2,mesh,error) && !error.empty());
    require(mesh.vertices[0].position[0]==160); // Failed conversion is transactional.
    require(!vr::decode_shape_mesh(shape,0,std::numeric_limits<float>::infinity(),mesh,error));
    if(argc==3) {
        const auto rom=assets::RomImage::load(argv[1]);
        const auto symbols=assets::SymbolMap::load(argv[2]);
        const assets::ShapeDecoder decoder(rom,symbols);
        const auto ship=decoder.decode_by_name(symbols,"SHIP_4");
        require(!ship.vertices.empty() && !ship.faces.empty());
        for(unsigned frame=0;frame<std::max(std::size_t(1),ship.frames.size());++frame) {
            require(vr::decode_shape_mesh(ship,frame,1,mesh,error));
            require(mesh.faces.size()==ship.faces.size());
            for(std::size_t i=0;i<ship.faces.size();++i) {
                require(mesh.faces[i].colour_id==ship.faces[i].colour_id);
                require(mesh.faces[i].indices.size()==ship.faces[i].vertex_indices.size());
                require(mesh.faces[i].normal==ship.faces[i].normal);
            }
        }
        std::cout<<"Cartridge SHIP_4: "<<mesh.vertices.size()<<" vertices, "<<mesh.faces.size()<<" faces preserved\n";
        require(vr::build_shape_batch(ship,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
        require(!batch.vertices.empty() && batch.vertices.size()%3==0);
        require(batch.ranges.size()+batch.line_ranges.size()+batch.deferred.size()+batch.source_noops.size()==ship.faces.size());
        std::cout<<"Cartridge material batch: "<<batch.vertices.size()/3<<" triangles, "<<batch.ranges.size()
            <<" face ranges, "<<batch.deferred.size()<<" explicitly deferred faces\n";
        for(const auto* name:{"ROBOT_0","BOSS_H_2","MY_DEMO"}) {
            const auto demo=decoder.decode_by_name(symbols,name);
            require(demo.bsp_root_address!=0 && !demo.bsp_nodes.empty());
            require(vr::decode_shape_mesh(demo,0,1,mesh,error));
            if(!vr::build_shape_batch(demo,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error))
                throw std::runtime_error(std::string(name)+": "+error);
            require(std::any_of(batch.vertices.begin(),batch.vertices.end(),[](const auto& v){return v.group_enabled!=0;}));
            std::cout<<"Cartridge BSP "<<name<<": "<<batch.vertices.size()/3<<" triangles with GPU group selection\n";
            for(unsigned effect=0;effect<6;++effect) {
                auto span_pose=pose;
                if(effect<2) span_pose.wireframe_mode=effect+1;
                if(effect==2) span_pose.wobble_mode=1;
                if(effect==3) span_pose.wobble_mode=2;
                if(effect==4) span_pose.wave_mode=true;
                if(effect==5) span_pose.cel_mode=true;
                vr::ShapeBatch span_batch;
                require(vr::build_shape_batch(demo,mesh,span_pose,0,{127,127,127},palette,0,1,false,span_batch,error,true));
                require(!span_batch.polygon_ranges.empty());
                require(std::all_of(span_batch.vertices.begin(),span_batch.vertices.end(),
                    [](const auto& vertex){return (vertex.texture[3]&1)!=0;}));
                for(const auto& range:span_batch.polygon_ranges) {
                    require(range.source_face<demo.faces.size());
                    require(range.vertex_count==demo.faces[range.source_face].vertex_indices.size());
                    require(range.first_vertex+range.vertex_count<=span_batch.polygon_vertices.size());
                }
            }
            std::cout<<"Cartridge BSP "<<name<<": six EX span boundary modes prepared without solid fans\n";
        }
        const auto andross=decoder.decode_by_name(symbols,"ANDROSS");
        require(vr::decode_shape_mesh(andross,0,1,mesh,error));
        require(vr::build_shape_batch(andross,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
        require(!batch.texels.empty() && batch.deferred.empty());
        require(std::any_of(batch.vertices.begin(),batch.vertices.end(),[](const auto& vertex){return (vertex.texture[3]&1)!=0;}));
        std::cout<<"Cartridge ANDROSS: "<<batch.texels.size()<<" native GPU texels\n";
        const auto sprite=decoder.decode_by_name(symbols,"LFDIE");
        require(vr::decode_shape_mesh(sprite,0,1,mesh,error));
        require(vr::build_shape_batch(sprite,mesh,pose,0,{127,127,127},palette,0,1,false,batch,error));
        require(batch.deferred.empty() && !batch.vertices.empty());
        require(std::any_of(batch.vertices.begin(),batch.vertices.end(),[](const auto& vertex){return (vertex.texture[3]&4)!=0;}));
    } else require(argc==1);
    std::cout<<"VR decoded shape geometry tests passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
