#include "starfox/vr/shape_batch.hpp"
#include <algorithm>
#include <unordered_map>
#include "starfox/vr/decal_surface.hpp"
namespace starfox::vr {
bool build_shape_batch(const assets::Shape& shape,const ShapeMesh& mesh,const render::RenderPose& pose,
    std::size_t depth,const std::array<int8_t,3>& light,std::span<const render::Rgba8> palette,
    uint8_t base,uint32_t scale,bool srgb,ShapeBatch& output,std::string& error,bool retain_polygon_boundaries) {
    const auto fail=[&](const char* reason) {error=reason;return false;};
    if(pose.colour_warp && !pose.force_colour && !pose.palette_override) return fail("Native colour-warp stage pending");
    if(pose.simple_scaled_sprite) return fail("Native simple-scaled-sprite stage pending");
    if(pose.collapse_to_axis_line) return fail("Native axis-collapse stage pending");
    ShapeBatch pending;
    std::unordered_map<const assets::TextureImage*,uint32_t> textures;
    if(mesh.faces.size()!=shape.faces.size()) return fail("Mesh/source face count mismatch");
    std::vector<render::FaceMaterial> materials;materials.reserve(shape.faces.size());
    bool has_textures=false;
    for(std::size_t i=0;i<mesh.faces.size();++i) {
        if(mesh.faces[i].source_face!=i) return fail("Mesh/source face order mismatch");
        materials.push_back(render::face_material(shape,shape.faces[i],pose.colour_frame,depth,light,pose,std::nullopt,base));
        has_textures|=materials.back().texture!=nullptr;
    }
    // Classify authored coplanar overlays before either packet path projects them.
    std::vector<DecalSurface> solid_surfaces;
    const auto surface=[&](const ShapeMeshFace& face) {
        DecalSurface points;
        for(auto index:face.indices) points.push_back({mesh.vertices[index].position});
        return points;
    };
    if(has_textures && !pose.explosion_progress) for(const auto& face:mesh.faces) {
        if(face.sprite || face.indices.size()<3 || materials[face.source_face].texture) continue;
        for(auto index:face.indices) if(index>=mesh.vertices.size()) return fail("Invalid mesh face vertex");
        solid_surfaces.push_back(surface(face));
    }
    std::vector<ShapeFaceInstance> instances;
    if(pose.explosion_progress) {
        for(std::size_t face=0;face<shape.faces.size();++face) instances.push_back({face,-1});
    } else if(!shape_face_instances(shape,instances,error)) return false;
    for(const auto& instance:instances) {
        if(instance.face>=mesh.faces.size()) return fail("Invalid BSP face instance");
        const auto& face=mesh.faces[instance.face];
        if(face.source_face!=instance.face) return fail("Mesh/source face order mismatch");
        if(face.source_face>=shape.faces.size()) return fail("Missing source material face");
        const auto& source=shape.faces[face.source_face];
        if(face.sprite && face.indices.size()!=1) return fail("Invalid sprite centre count");
        for(auto index:face.indices) if(index>=mesh.vertices.size()) return fail("Invalid mesh face vertex");
        if(!face.sprite && face.indices.size()<2) {
            pending.source_noops.push_back({face.source_face,SourceNoop::degenerate});continue;
        }
        if(!pose.explosion_progress) {
            const auto invalid_visibility=[&](int index) {
                if(index<0 || static_cast<std::size_t>(index)>=shape.visibilities.size()) return false;
                const auto& v=shape.visibilities[index];
                return v.a>=mesh.vertices.size() || v.b>=mesh.vertices.size() || v.c>=mesh.vertices.size();
            };
            // Match the software renderer: an in-range visibility record with
            // absent vertices suppresses this face/batch, not the whole scene.
            // HYPER4 contains such a record in the original cartridge source.
            if(invalid_visibility(face.visibility_index) || invalid_visibility(instance.group_visibility)) {
                pending.source_noops.push_back({face.source_face,SourceNoop::invalid_visibility});continue;
            }
        }
        auto material=materials[face.source_face];
        // EX's alternate span routines are selected only for solid polygons.
        // Textures, sprites and two-point lines take separate source paths.
        // Do not reject those unaffected primitives merely because the object
        // carries an effect flag. Explicit boundary consumers own affected
        // solids; the default triangle-only path still rejects them.
        const bool source_span_face=!face.sprite && face.indices.size()>=3 && !material.texture &&
            (pose.wireframe_mode || pose.wobble_mode || pose.wave_mode || pose.cel_mode);
        if(source_span_face && !retain_polygon_boundaries) {
            if(pose.wireframe_mode) return fail("Native EX wireframe mode pending");
            if(pose.wobble_mode || pose.wave_mode || pose.cel_mode)
                return fail("Native EX scanline effect pending");
        }
        if(face.sprite && !material.texture) {pending.source_noops.push_back({face.source_face,SourceNoop::untextured_sprite});continue;}
        // MOBJ's two-point primitive uses draw_line/material.colour, even
        // when the material descriptor also resolves to a texture. Neither
        // upload nor sample that art for source lines (sprites still do).
        if(!face.sprite && face.indices.size()==2) material.texture=nullptr;
        SceneVertex prototype{};
        if(material.texture) {
            const auto& texture=*material.texture;
            const auto size=(uint32_t(texture.u_mask)+1)*(uint32_t(texture.v_mask)+1);
            if(texture.texels.size()!=size) return fail("Invalid source texture dimensions");
            auto found=textures.find(&texture);
            if(found==textures.end()) {
                if(pending.texels.size()+size>4'000'000) return fail("Shape texel batch exceeds upload limit");
                const auto offset=static_cast<uint32_t>(pending.texels.size());
                for(auto index:texture.texels) {
                    if(!index) {pending.texels.push_back(0);continue;}
                    const auto translated=static_cast<uint8_t>(base+index);
                    if(translated>=palette.size()) return fail("Invalid texture palette index");
                    const auto c=palette[translated];
                    pending.texels.push_back(uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)|0xff000000U);
                }
                found=textures.emplace(&texture,offset).first;
            }
            prototype.texture[0]=found->second;prototype.texture[1]=texture.u_mask;
            prototype.texture[2]=texture.v_mask;prototype.texture[3]=1U|(srgb?2U:0U);
            if(!face.sprite && !pose.explosion_progress) {
                const auto overlay=surface(face);
                if(std::any_of(solid_surfaces.begin(),solid_surfaces.end(),[&](const auto& solid) {
                    return decal_surface_contains(solid,overlay);
                })) prototype.texture[3]|=32768U;
            }
        } else if(!apply_scene_material(prototype,material,palette,base,scale,srgb)) return fail("Invalid GPU material palette/dither scale");
        if(pose.explosion_progress) {
            // Visibility/BSP are bypassed by the source explosion routine.
            // The remaining transform payload is filled by build_draw_packet.
            prototype.visibility_enabled=2;
            prototype.group_b[0]=-float(source.normal.x);
            prototype.group_b[1]=float(source.normal.y);
            prototype.group_b[2]=-float(source.normal.z);
        }
        if(!pose.explosion_progress && face.visibility_index>=0 && static_cast<std::size_t>(face.visibility_index)<shape.visibilities.size()) {
            const auto& visibility=shape.visibilities[face.visibility_index];
            if(visibility.a>=mesh.vertices.size() || visibility.b>=mesh.vertices.size() || visibility.c>=mesh.vertices.size())
                return fail("Invalid source visibility triple");
            const auto copy=[&](auto index,float* target) {
                std::copy(mesh.vertices[index].position.begin(),mesh.vertices[index].position.end(),target);
            };
            copy(visibility.a,prototype.visibility_a);copy(visibility.b,prototype.visibility_b);copy(visibility.c,prototype.visibility_c);
            prototype.visibility_enabled=1;
        }
        if(instance.group_visibility>=0) {
            const auto& visibility=shape.visibilities[instance.group_visibility];
            if(visibility.a>=mesh.vertices.size() || visibility.b>=mesh.vertices.size() || visibility.c>=mesh.vertices.size())
                return fail("Invalid BSP group visibility triple");
            const auto copy=[&](auto index,float* target) {
                std::copy(mesh.vertices[index].position.begin(),mesh.vertices[index].position.end(),target);
            };
            copy(visibility.a,prototype.group_a);copy(visibility.b,prototype.group_b);copy(visibility.c,prototype.group_c);
            prototype.group_enabled=1;
        }
        if(face.sprite) {
            if(pending.vertices.size()+6>4'000'000) return fail("Shape sprite batch exceeds upload limit");
            const auto width=uint32_t(material.texture->u_mask)+1;
            // Source mshowspr uses width/2 world units, doubled for 64px sheets.
            const float half=float(width)*(width==64?1.F:.5F);
            const float corners[4][2]{{-1,1},{1,1},{1,-1},{-1,-1}};
            pending.ranges.push_back({face.source_face,static_cast<uint32_t>(pending.vertices.size()),6,face.visibility_index});
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                auto vertex=prototype;vertex.texture[3]|=4;
                std::copy(mesh.vertices[face.indices[0]].position.begin(),mesh.vertices[face.indices[0]].position.end(),vertex.position);
                vertex.billboard[0]=corners[corner][0]*half;vertex.billboard[1]=corners[corner][1]*half;
                vertex.uv[0]=corners[corner][0]>0?float(width):0;
                // The source uses a square destination and clips non-square art.
                vertex.uv[1]=corners[corner][1]<0?float(width):0;
                pending.vertices.push_back(vertex);
            }
            continue;
        }
        if(face.indices.size()==2) {
            if(pending.line_vertices.size()+2>4'000'000) return fail("Shape line batch exceeds upload limit");
            pending.line_ranges.push_back({face.source_face,static_cast<uint32_t>(pending.line_vertices.size()),2,face.visibility_index});
            for(std::size_t corner=0;corner<face.indices.size();++corner) {
                const auto index=face.indices[corner];
                auto vertex=prototype;
                if(material.texture) {
                    const auto uv=material.texture->coordinates[corner%4];
                    vertex.uv[0]=float(uv.u)+pose.texture_scroll_x;vertex.uv[1]=float(uv.v)+pose.texture_scroll_y;
                }
                std::copy(mesh.vertices[index].position.begin(),mesh.vertices[index].position.end(),vertex.position);
                pending.line_vertices.push_back(vertex);
            }
            continue;
        }
        if(retain_polygon_boundaries && !material.texture) {
            if(pending.polygon_vertices.size()+face.indices.size()>4'000'000)
                return fail("Shape polygon boundary batch exceeds upload limit");
            pending.polygon_ranges.push_back({face.source_face,
                static_cast<uint32_t>(pending.polygon_vertices.size()),
                static_cast<uint32_t>(face.indices.size()),face.visibility_index});
            for(const auto index:face.indices) {
                auto vertex=prototype;
                std::copy(mesh.vertices[index].position.begin(),mesh.vertices[index].position.end(),vertex.position);
                pending.polygon_vertices.push_back(vertex);
            }
        }
        // Never draw an ordinary fan underneath a sparse/wavy source span.
        // Textures, sprites and lines have already taken their own paths.
        if(source_span_face) continue;
        const auto vertex_count=(face.indices.size()-2)*3;
        if(pending.vertices.size()+vertex_count>4'000'000) return fail("Shape triangle batch exceeds upload limit");
        pending.ranges.push_back({face.source_face,static_cast<uint32_t>(pending.vertices.size()),
            static_cast<uint32_t>(vertex_count),face.visibility_index});
        for(std::size_t corner=1;corner+1<face.indices.size();++corner) {
            for(auto face_corner:{std::size_t(0),corner,corner+1}) {
                const auto index=face.indices[face_corner];
                auto vertex=prototype;
                if(material.texture) {
                    const auto uv=material.texture->coordinates[face_corner%4];
                    vertex.uv[0]=float(uv.u)+pose.texture_scroll_x;vertex.uv[1]=float(uv.v)+pose.texture_scroll_y;
                }
                std::copy(mesh.vertices[index].position.begin(),mesh.vertices[index].position.end(),vertex.position);
                pending.vertices.push_back(vertex);
            }
        }
    }
    output=std::move(pending);error.clear();return true;
}
}
