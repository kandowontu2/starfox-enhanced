// PCVR MODERN's xBRZ fallback (vr::VulkanLayerFilter): the filtered 2D layer
// drawn as one quad where its sprites would be. clip: the eye's projection x
// view x the sprite layer's placement (canvas pixels to clip space); extent:
// canvas size in pixels (xy), the filled part of the texture (zw); decode: x
// nonzero when the eye target is sRGB (the filter works on encoded colours).
struct HudLayerConstants {column_major float4x4 clip;float4 extent;float4 decode;};
[[vk::push_constant]] ConstantBuffer<HudLayerConstants> hud;
[[vk::binding(0,0)]] Texture2D<float4> hud_layer;
[[vk::binding(1,0)]] SamplerState hud_sampler;
struct HudQuad {float4 position : SV_Position;float2 uv : TEXCOORD0;};
HudQuad hud_layer_vertex_main(uint id : SV_VertexID) {
    // Two triangles over the canvas.
    static const float2 corners[6]={float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
    HudQuad output;
    const float2 corner=corners[id];
    output.uv=corner*hud.extent.zw;
    output.position=mul(hud.clip,float4(corner*hud.extent.xy,0,1));
    return output;
}
float4 hud_layer_fragment_main(HudQuad input) : SV_Target0 {
    float4 colour=hud_layer.Sample(hud_sampler,input.uv);
    if(hud.decode.x!=0) colour.rgb=select(colour.rgb<=.04045,colour.rgb/12.92,pow((colour.rgb+.055)/1.055,2.4));
    return colour;
}
