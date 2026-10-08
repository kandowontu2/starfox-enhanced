// Same retained 160-byte SceneVertex input as rasterization. Raw loads avoid
// backend-specific StructuredBuffer float3 padding. CPU only composes matrices.
ByteAddressBuffer vertices : register(t0,space0);
RWByteAddressBuffer triangles : register(u0,space1);
cbuffer Settings : register(b0,space2) {
    float4 view_rows[3];
    uint count,output_first;
    float units_per_metre;
    uint materials_offset;
    uint4 effects;
    uint texture_offset,line_mode;
    float2 focal;
    float near_plane;
    uint previous_mode; // 0 current; 1 accepted previous; 2 unmatched previous.
    uint previous_index_offset,previous_first;
};
struct Vertex {
    float3 position;
    float3 visibility_a,visibility_b,visibility_c;
    uint visibility_enabled;
    float3 group_a,group_b,group_c;
    uint group_enabled;
    uint4 texture;
    float2 billboard;
};
float4x4 view_matrix() {return float4x4(view_rows[0],view_rows[1],view_rows[2],float4(0,0,0,1));}
#include "../../vr/shaders/scene_geometry.hlsli"
#define STARFOX_CALIBRATED_PALETTE
#include "calibrated_colour.hlsli"
#include "../../vr/shaders/scene_colour.hlsli"
uint native_ray_colour(float4 colour) {
    float3 rgb=scene_styled_colour(colour,effects).rgb;
    if((effects.z&1U)!=0) rgb=calibrated_encode_srgb(rgb);
    uint3 packed=uint3(round(saturate(rgb)*255.));
    return packed.x|(packed.y<<8)|(packed.z<<16)|0xff000000U;
}
Vertex load_vertex(uint index) {
    uint at=index*160;Vertex v;
    v.position=asfloat(vertices.Load3(at));
    v.visibility_a=asfloat(vertices.Load3(at+48));
    v.visibility_b=asfloat(vertices.Load3(at+60));
    v.visibility_c=asfloat(vertices.Load3(at+72));
    v.visibility_enabled=vertices.Load(at+84);
    v.group_a=asfloat(vertices.Load3(at+88));
    v.group_b=asfloat(vertices.Load3(at+100));
    v.group_c=asfloat(vertices.Load3(at+112));
    v.group_enabled=vertices.Load(at+124);
    v.texture=vertices.Load4(at+136);
    v.billboard=asfloat(vertices.Load2(at+152));
    return v;
}
void store_previous_index(uint primitive,uint visible) {
    if(previous_mode==0) return;
    uint current=(output_first-materials_offset/16)/3+primitive;
    uint accepted=visible!=0 && previous_first!=0xffffffffU?previous_first+primitive:0xffffffffU;
    triangles.Store(previous_index_offset+current*4,accepted);
}
void store_material(uint vertex,uint triangle_index,float2 uv0,float2 uv1,float2 uv2) {
    if(previous_mode!=0) return; // Previous geometry never overwrites live colour/UV records.
    // Native solid material kind 2: packed authored RGBA colours rather than
    // 8-bit palette indices. All material records stay in the same GPU buffer.
    uint at=materials_offset+triangle_index*64;
    Vertex material_vertex=load_vertex(vertex/160);
    source_billboard_visible(material_vertex); // Also preserves EX aiming-station wrap flags.
    uint4 texture=material_vertex.texture;
    if((texture.w&1U)!=0) {
        // Kind 3 keeps source UVs/flags and layer styling. RGBA words live
        // after all triangle records, copied from the retained raster texels.
        triangles.Store2(at,asuint(uv0));
        triangles.Store2(at+8,asuint(uv1));
        triangles.Store2(at+16,asuint(uv2));
        triangles.Store2(at+24,uint2(1,texture.w));
        triangles.Store4(at+32,effects);
        triangles.Store4(at+48,uint4(texture_offset+texture.x*4,texture.y,texture.z,3));
        return;
    }
    triangles.Store4(at,0);triangles.Store4(at+16,0);
    triangles.Store(at+28,vertices.Load(vertex+44)); // Actual dither pixel scale.
    triangles.Store4(at+32,uint4(native_ray_colour(asfloat(vertices.Load4(vertex+12))),
        native_ray_colour(asfloat(vertices.Load4(vertex+28))),0,0));
    triangles.Store4(at+48,uint4(0,0,0,2));
}
// Source LINELIST primitives have a one-raster-pixel width, not a zero-area
// triangle. Preserve them as eye-facing ribbons of that projected width.
// Endpoint widths vary with depth and both focal axes: no central-eye/X-only
// approximation. Offscreen endpoints survive; hidden/behind/degenerate lines
// produce degenerate records, just like hidden ordinary source faces.
void store_line(uint primitive) {
    if(previous_mode==2) {
        for(uint i=0;i<6;++i) triangles.Store4((output_first+primitive*6+i)*16,0);
        store_previous_index(primitive*2,0);store_previous_index(primitive*2+1,0);
        return;
    }
    uint first=primitive*2,visible=1;
    float3 eye[2];float2 uv[2];
    for(uint i=0;i<2;++i) {
        Vertex input=load_vertex(first+i);float3 source;uint shown;
        uint billboard_visible=source_billboard_visible(input);
        eye[i]=scene_eye_position(input,source,shown).xyz*float3(1,-1,-1)*units_per_metre;
        uv[i]=asfloat(vertices.Load2((first+i)*160+128));
        if(i==0) visible=shown&billboard_visible;
        visible&=all(isfinite(eye[i]));
    }
    if(max(eye[0].z,eye[1].z)<near_plane) visible=0;
    if(visible!=0) {
        // Clip on GPU before dividing by depth; never turn a camera-crossing
        // line into an infinite ribbon. Retain the source segment's UV phase.
        if(eye[0].z<near_plane) {
            float t=(near_plane-eye[0].z)/(eye[1].z-eye[0].z);
            eye[0]=lerp(eye[0],eye[1],t);uv[0]=lerp(uv[0],uv[1],t);
        } else if(eye[1].z<near_plane) {
            float t=(near_plane-eye[1].z)/(eye[0].z-eye[1].z);
            eye[1]=lerp(eye[1],eye[0],t);uv[1]=lerp(uv[1],uv[0],t);
        }
    }
    float2 direction=visible!=0?focal*(eye[1].xy/eye[1].z-eye[0].xy/eye[0].z):float2(0,0);
    float squared=dot(direction,direction);
    visible&=all(isfinite(direction)) && squared>1.e-20;
    float2 halfWidth=visible!=0?float2(-direction.y,direction.x)*(.5*rsqrt(squared))/focal:float2(0,0);
    float3 corners[4];
    corners[0]=eye[0]-float3(halfWidth*eye[0].z,0);
    corners[1]=eye[1]-float3(halfWidth*eye[1].z,0);
    corners[2]=eye[1]+float3(halfWidth*eye[1].z,0);
    corners[3]=eye[0]+float3(halfWidth*eye[0].z,0);
    const uint indices[6]={0,1,2,0,2,3};
    uint output=output_first+primitive*6;
    for(uint i=0;i<6;++i) triangles.Store4((output+i)*16,asuint(float4(visible!=0?corners[indices[i]]:float3(0,0,0),previous_mode!=0?float(visible!=0):1)));
    store_previous_index(primitive*2,visible);store_previous_index(primitive*2+1,visible);
    store_material(first*160,output/3,uv[0],uv[1],uv[1]);
    store_material(first*160,output/3+1,uv[0],uv[1],uv[0]);
}
[numthreads(64,1,1)]
void ray_geometry_main(uint3 id:SV_DispatchThreadID) {
    if(line_mode!=0) {if(id.x<count/2) store_line(id.x);return;}
    if(id.x>=count/3) return;
    if(previous_mode==2) {
        for(uint i=0;i<3;++i) triangles.Store4((output_first+id.x*3+i)*16,0);
        store_previous_index(id.x,0);
        return;
    }
    float3 eye[3];uint visible=1;
    for(uint i=0;i<3;++i) {
        Vertex input=load_vertex(id.x*3+i);float3 source;uint shown;
        uint billboard_visible=source_billboard_visible(input);
        eye[i]=scene_eye_position(input,source,shown).xyz*float3(1,-1,-1)*units_per_metre;
        if(i==0) visible=shown&billboard_visible; // Same provoking vertex as raster visibility.
        visible&=all(isfinite(eye[i]));
    }
    if(previous_mode!=0) {
        float3 normal=cross(eye[1]-eye[0],eye[2]-eye[0]);
        visible&=all(isfinite(normal)) && dot(normal,normal)>1.e-20;
    }
    store_previous_index(id.x,visible);
    for(uint i=0;i<3;++i) {
        // Only a coordinate/unit conversion after the complete tracked pose.
        float3 position=visible?eye[i]:float3(0,0,0);
        triangles.Store4((output_first+id.x*3+i)*16,asuint(float4(position,previous_mode!=0?float(visible!=0):1)));
    }
    uint vertex=id.x*3*160;
    store_material(vertex,output_first/3+id.x,asfloat(vertices.Load2(vertex+128)),
        asfloat(vertices.Load2(vertex+160+128)),asfloat(vertices.Load2(vertex+320+128)));
}
