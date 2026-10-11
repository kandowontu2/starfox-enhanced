#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T>
[[clang::optnone]] T spvFMul(T l, T r)
{
    return fma(l, r, T(0));
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Cols> spvFMulVectorMatrix(vec<T, Rows> v, matrix<T, Cols, Rows> m)
{
    vec<T, Cols> res = vec<T, Cols>(0);
    for (uint i = Rows; i > 0; --i)
    {
        vec<T, Cols> tmp(0);
        for (uint j = 0; j < Cols; ++j)
        {
            tmp[j] = m[j][i - 1];
        }
        res = fma(tmp, vec<T, Cols>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Rows> spvFMulMatrixVector(matrix<T, Cols, Rows> m, vec<T, Cols> v)
{
    vec<T, Rows> res = vec<T, Rows>(0);
    for (uint i = Cols; i > 0; --i)
    {
        res = fma(m[i - 1], vec<T, Rows>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int LCols, int LRows, int RCols, int RRows>
[[clang::optnone]] matrix<T, RCols, LRows> spvFMulMatrixMatrix(matrix<T, LCols, LRows> l, matrix<T, RCols, RRows> r)
{
    static_assert(LCols == RRows, "column-row configuration mismatch");
    matrix<T, RCols, LRows> res;
    for (uint i = 0; i < RCols; i++)
    {
        vec<T, LRows> tmp(0);
        for (uint j = 0; j < LCols; j++)
        {
            tmp = fma(vec<T, LRows>(r[i][j]), l[j], tmp);
        }
        res[i] = tmp;
    }
    return res;
}

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_Settings
{
    uint width;
    uint height;
    uint surfaceOffset;
    uint hasSurface;
};

struct Fullscreen
{
    float4 position;
};

struct Guides
{
    float4 ownership;
    float4 surface;
};

struct water_guides_fragment_main_out
{
    float4 out_var_SV_Target0 [[color(0)]];
    float4 out_var_SV_Target1 [[color(1)]];
};

fragment water_guides_fragment_main_out water_guides_fragment_main(const device type_ByteAddressBuffer& water [[buffer(1)]], constant type_Settings& Settings [[buffer(0)]], texture2d<float> ownership [[texture(0)]], texture2d<float> surfaces [[texture(1)]], float4 gl_FragCoord [[position]])
{
    water_guides_fragment_main_out out = {};
    __attribute__((unused)) float4 _81 = gl_FragCoord;
    uint2 _86 = uint2(gl_FragCoord.xy);
    bool _87 = true;
    bool _116;
    if (_86.x < Settings.width)
    {
        bool _115 = _86.y >= Settings.height;
        _87 = _115;
        _116 = _115;
    }
    else
    {
        _116 = true;
    }
    if (_116)
    {
        discard_fragment();
    }
    float4 _85 = ownership.read(uint2(int3(int(_86.x), int(_86.y), 0).xy), 0);
    float4 _88;
    float4 _143;
    if (Settings.hasSurface != 0u)
    {
        float4 _142 = surfaces.read(uint2(int3(int(_86.x), int(_86.y), 0).xy), 0);
        _88 = _142;
        _143 = _142;
    }
    else
    {
        _88 = float4(0.0);
        _143 = float4(0.0);
    }
    float4 _84 = _143;
    uint _147 = uint(rint(spvFMul(_85.z, 255.0)));
    __attribute__((unused)) uint _89 = _147;
    uint _21 = (_86.y * Settings.width) + _86.x;
    __attribute__((unused)) uint _90 = _21;
    bool _91 = true;
    bool _156;
    if (_147 != 1u)
    {
        bool _155 = _147 == 2u;
        _91 = _155;
        _156 = _155;
    }
    else
    {
        _156 = true;
    }
    bool _92 = false;
    bool _162;
    if (_156)
    {
        bool _161 = _85.x > 0.0;
        _92 = _161;
        _162 = _161;
    }
    else
    {
        _162 = false;
    }
    bool _93 = false;
    bool _170;
    if (_162)
    {
        bool _169 = (water._m0[(_21 * 4u) >> 2u] >> 24u) == 253u;
        _93 = _169;
        _170 = _169;
    }
    else
    {
        _170 = false;
    }
    float4 _217;
    if (_170)
    {
        uint _175 = (Settings.surfaceOffset + (_21 * 16u)) >> 2u;
        float4 _94 = as_type<float4>(uint4(water._m0[_175], water._m0[_175 + 1u], water._m0[_175 + 2u], water._m0[_175 + 3u]));
        float _28 = dot(_94.xyz, _94.xyz);
        __attribute__((unused)) float _95 = _28;
        bool4 _191 = isnan(_94);
        bool4 _192 = isinf(_94);
        bool _96 = false;
        bool _201;
        if (all(not(bool4(_191.x || _192.x, _191.y || _192.y, _191.z || _192.z, _191.w || _192.w))))
        {
            bool _200 = _94.w > 0.0;
            _96 = _200;
            _201 = _200;
        }
        else
        {
            _201 = false;
        }
        bool _97 = false;
        bool _205;
        if (_201)
        {
            bool _204 = _28 > 0.5;
            _97 = _204;
            _205 = _204;
        }
        else
        {
            _205 = false;
        }
        bool _98 = false;
        bool _209;
        if (_205)
        {
            bool _208 = _28 < 1.5;
            _98 = _208;
            _209 = _208;
        }
        else
        {
            _209 = false;
        }
        float4 _216;
        if (_209)
        {
            _85 = float4(0.0039215688593685626983642578125, 1.0, 0.0039215688593685626983642578125, _85.w);
            _84 = _94;
            _216 = _94;
        }
        else
        {
            _216 = _143;
        }
        _217 = _216;
    }
    else
    {
        _217 = _143;
    }
    __attribute__((unused)) float4 _83 = _85;
    __attribute__((unused)) float4 _82 = _217;
    out.out_var_SV_Target0 = _85;
    out.out_var_SV_Target1 = _217;
    return out;
}

