// MODERN: the VR scene shader under SDL_GPU's binding model.
// Vertex stage: storage buffers t#/space0, uniform buffers b#/space1.
// Fragment stage: storage buffers t#/space2, uniform buffers b#/space3.
// SPIR-V takes set = space, binding = register; DXIL uses the registers as-is.
// The camera arrives as a uniform block (SDL_PushGPU*UniformData, slot 0)
// instead of a Vulkan push constant. The caller flips projection Y because
// SDL_GPU presents a Y-up clip space on every backend.
// scene.hlsl declares those bindings for STARFOX_SDL_SCENE_VERTEX/FRAGMENT
// (shared with the calibrated DisplayXR renderer).
#if defined(STARFOX_SDL_FRAGMENT)
#define STARFOX_SDL_SCENE_FRAGMENT
#else
#define STARFOX_SDL_SCENE_VERTEX
#endif
#define STARFOX_SCENE_RESOLVE_DITHER
#include "../../vr/shaders/scene.hlsl"

// SDL_GPU's D3D12 backend names every vertex attribute TEXCOORD<location>.
struct SdlVertex {
    [[vk::location(0)]] float3 position : TEXCOORD0;
    [[vk::location(1)]] float4 color : TEXCOORD1;
    [[vk::location(2)]] float4 odd_color : TEXCOORD2;
    [[vk::location(3)]] uint dither_scale : TEXCOORD3;
    [[vk::location(4)]] float3 visibility_a : TEXCOORD4;
    [[vk::location(5)]] float3 visibility_b : TEXCOORD5;
    [[vk::location(6)]] float3 visibility_c : TEXCOORD6;
    [[vk::location(7)]] uint visibility_enabled : TEXCOORD7;
    [[vk::location(8)]] float3 group_a : TEXCOORD8;
    [[vk::location(9)]] float3 group_b : TEXCOORD9;
    [[vk::location(10)]] float3 group_c : TEXCOORD10;
    [[vk::location(11)]] uint group_enabled : TEXCOORD11;
    [[vk::location(12)]] float2 uv : TEXCOORD12;
    [[vk::location(13)]] uint4 texture : TEXCOORD13;
    [[vk::location(14)]] float2 billboard : TEXCOORD14;
};
Vertex sdl_vertex(SdlVertex input) {
    Vertex output;
    output.position=input.position;output.color=input.color;output.odd_color=input.odd_color;
    output.dither_scale=input.dither_scale;
    output.visibility_a=input.visibility_a;output.visibility_b=input.visibility_b;output.visibility_c=input.visibility_c;
    output.visibility_enabled=input.visibility_enabled;
    output.group_a=input.group_a;output.group_b=input.group_b;output.group_c=input.group_c;
    output.group_enabled=input.group_enabled;
    output.uv=input.uv;output.texture=input.texture;output.billboard=input.billboard;
    return output;
}
#if !defined(STARFOX_SDL_FRAGMENT)
// Vertex uniform slot 1 (SdlScenePipelines::push_line_brush): x = brush
// pixels, yz = clip offset per target pixel, w = enabled (line draws only).
cbuffer SdlLineBrush : register(b1, space1) { float4 line_brush; };
Fragment sdl_vertex_textured_main(SdlVertex input,uint instance : SV_InstanceID) {
    Fragment output=vertex_textured_main(sdl_vertex(input));
    // Thick source lines: instance i < N shifts i pixels right, later ones
    // shift down, sweeping the desktop's N x N square brush along the line.
    if(line_brush.w!=0 && instance!=0) {
        uint pixels=uint(line_brush.x);
        float2 shift=instance<pixels?float2(instance,0):float2(0,instance-pixels+1);
        output.position.xy+=shift*line_brush.yz*output.position.w;
    }
    return output;
}
#endif
#if defined(STARFOX_SDL_FRAGMENT)
// Fragment uniform slot 1 (SdlFrameRenderer): distance haze (scene.hlsl's
// scene_fog), w = the mode (0 off, 1 every fragment, 2 the landscape).
// The same block carries the ray-traced sun shadow (SdlRayShadows):
// shadow_params x = receivers (0 none, 1 world models, 2 the landscape
// ground plane), y = caster objects, z = brightness kept in shadow, w = sun
// samples (1 hard, more for soft); shadow_light xyz = toward the sun in
// native eye space (+Y down, +Z forward, source units), w = ray start
// offset per unit of distance; shadow_target xy = target size, z = the
// sun's angular radius (soft shadows); shadow_unproject = inverse
// projection rows (SDL clip space, Y up).
// Emissive point lights (SdlRayShadows::lights): light_params x = count,
// y = brightness steps (0 smooth), z = strength, w = 1 when the sun shadows
// these receivers; point_lights, four per light (SdlRayShadows::PointLight):
// position (native eye space) and radius; colour and its own caster object
// (asuint, ~0 none); pool axis and aspect; hardness, projected, fade height,
// background only (lights the ground, corridor and room, not models).
cbuffer SdlFog : register(b1, space3) {
    float4 fog_colour; float4 fog_params;
    float4 shadow_params; float4 shadow_light; float4 shadow_target; float4 shadow_unproject[4];
    float4 light_params; float4 point_lights[64];
};
// Casters (ray_shadow.hlsl): three float4 corners per triangle and its
// bounds across the sun's rays (min u,v, max u,v on shadow_basis); per object
// ordered-bits bounds plus its triangle range (count bit 31: large scenery).
StructuredBuffer<float4> shadow_triangles : register(t1, space2);
ByteAddressBuffer shadow_objects : register(t2, space2);
float4 ray_clip(float4 position) {
    // SDL_GPU clip space is Y up.
    return float4(position.x/shadow_target.x*2.-1.,1.-position.y/shadow_target.y*2.,position.z,1);
}
float4 ray_unproject_row(uint row) {return shadow_unproject[row];}
float3 ray_eye() {return 0;}
#include "../../vr/shaders/scene_rt.hlsli"
float4 sdl_fragment_main(Fragment input) : SV_Target0 {
    // Model receivers need the face's plane before anything may discard:
    // flat faces, so screen derivatives of the rebuilt position give it.
    float3 surface=0,normal=0;
    if(shadow_params.x!=0) {
        surface=shadow_native_position(input.position);
        normal=cross(ddx(surface),ddy(surface));
    }
    float4 colour=fragment_textured_main(input);
    colour=ray_lighting(input,colour,surface,normal,uint(shadow_params.x),light_params.w!=0);
    return scene_fog(input,colour,uint(fog_params.w),fog_colour,fog_params);
}
#endif
