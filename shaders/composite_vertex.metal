#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_Settings
{
    uint width;
    uint height;
    uint shadowRowBytes;
    uint flags;
    float shadowStrength;
    float reflectionStrength;
    uint reflectionRowBytes;
    uint material;
    uint materialScale;
    float effectSeconds;
    uint2 padding;
    uint4 postEffects;
};

struct Fullscreen
{
    float4 position;
};

struct composite_vertex_main_out
{
    float4 gl_Position [[position]];
};

vertex composite_vertex_main_out composite_vertex_main(uint gl_VertexIndex [[vertex_id]])
{
    composite_vertex_main_out out = {};
    __attribute__((unused)) uint _50 = gl_VertexIndex;
    bool _52 = gl_VertexIndex == 1u;
    int _48;
    if (_52)
    {
        _48 = 3;
    }
    else
    {
        _48 = -1;
    }
    bool _58 = gl_VertexIndex == 2u;
    int _49;
    if (_58)
    {
        _49 = 3;
    }
    else
    {
        _49 = -1;
    }
    float4 _64 = float4(float(_52 ? 3 : (-1)), float(_58 ? 3 : (-1)), 0.0, 1.0);
    __attribute__((unused)) float4 _47 = _64;
    __attribute__((unused)) float4 _46 = _64;
    out.gl_Position = _64;
    return out;
}

