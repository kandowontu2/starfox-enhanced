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
// Fragment uniform slot 1 (SdlFrameRenderer): distance haze. colour in the
// target encoding (a = strength); params x/y = start/end distance (source
// units), z = near plane x 256 (reversed depth: distance = z / depth),
// w = 0 off, 1 every fragment, 2 the landscape: its ground by distance and
// its sky dome by height above the horizon (full at it, clear 56 art rows
// up), so the haze hangs over the horizon as well as in front of it.
cbuffer SdlFog : register(b1, space3) { float4 fog_colour; float4 fog_params; };
float4 sdl_fragment_main(Fragment input) : SV_Target0 {
    float4 colour=fragment_textured_main(input);
    if(fog_params.w==0 || colour.a==0) return colour;
    float amount;
    if(fog_params.w==2) {
        uint flags=texels[input.texture.x+7];
        if((input.texture.w&8U)==0 || (flags&4096U)==0) return colour;
        if((flags&16384U)==0) {
            // The sky dome (landscape_world_sample's art rows above the horizon).
            float3 d=input.surface_position;
            float rows=d.y/max(-d.z,length(d)*.02)*256.;
            amount=pow(1.-saturate(rows/56.),1.5)*fog_colour.a;
            colour.rgb=lerp(colour.rgb,fog_colour.rgb,amount);
            return colour;
        }
    }
    float distance=fog_params.z/max(input.position.z,1e-9);
    amount=saturate((distance-fog_params.x)/max(fog_params.y-fog_params.x,1.))*fog_colour.a;
    colour.rgb=lerp(colour.rgb,fog_colour.rgb,amount);
    return colour;
}
#endif
