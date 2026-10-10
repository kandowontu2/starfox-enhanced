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

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct type_Settings
{
    uint width;
    uint height;
    uint lobes;
    uint primaryPrefix;
    uint recordPrefix;
    uint triangles;
    uint curvedReceivers;
    uint liquidMaterial;
    uint pathStride;
    uint scenePaths;
    uint reserved0;
    uint reserved1;
};

kernel void reflection_path_capture_main(device type_ByteAddressBuffer& current [[buffer(0)]], device type_RWByteAddressBuffer& captured [[buffer(1)]], constant type_Settings& Settings [[buffer(2)]], texture2d<float> ownership [[texture(0)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _147;
        if (gl_GlobalInvocationID.x < Settings.width)
        {
            _147 = gl_GlobalInvocationID.y >= Settings.height;
        }
        else
        {
            _147 = true;
        }
        if (_147)
        {
            break;
        }
        uint _13 = Settings.width * Settings.height;
        uint _15 = (gl_GlobalInvocationID.y * Settings.width) + gl_GlobalInvocationID.x;
        uint _16 = _15 * 4u;
        uint _153 = _16 >> 2u;
        uint _158 = ((_13 * Settings.primaryPrefix) + _16) >> 2u;
        uint _160 = current._m0[_158];
        uint _161 = ((_13 * (Settings.primaryPrefix + 4u)) + _16) >> 2u;
        uint _163 = current._m0[_161];
        float _164 = as_type<float>(_163);
        uint _24 = _15 * 16u;
        uint _165 = ((_13 * (Settings.primaryPrefix + 8u)) + _24) >> 2u;
        uint _167 = current._m0[_165];
        uint _26 = _165 + 1u;
        uint _169 = current._m0[_26];
        uint _27 = _165 + 2u;
        uint _171 = current._m0[_27];
        uint _28 = _165 + 3u;
        uint _173 = current._m0[_28];
        float4 _175 = as_type<float4>(uint4(_167, _169, _171, _173));
        bool _178 = Settings.curvedReceivers != 0u;
        bool _185;
        if (!_178)
        {
            _185 = Settings.scenePaths != 0u;
        }
        else
        {
            _185 = true;
        }
        float4 _200;
        if (_185)
        {
            uint _189 = ((_13 * (Settings.primaryPrefix + 24u)) + _24) >> 2u;
            _200 = as_type<float4>(uint4(current._m0[_189], current._m0[_189 + 1u], current._m0[_189 + 2u], current._m0[_189 + 3u]));
        }
        else
        {
            _200 = float4(0.0, 0.0, 0.0, 1.0);
        }
        float4 _206 = ownership.read(uint2(int3(int(gl_GlobalInvocationID.x), int(gl_GlobalInvocationID.y), 0).xy), 0);
        bool _218;
        if (_178)
        {
            _218 = _160 == 4294967293u;
        }
        else
        {
            bool _217;
            if (Settings.scenePaths != 0u)
            {
                _217 = _160 == 4294967294u;
            }
            else
            {
                _217 = false;
            }
            _218 = _217;
        }
        bool _255;
        if (_206.w > 0.5)
        {
            bool _254;
            do
            {
                uint _227 = uint(rint(spvFMul(_206.z, 255.0)));
                float _228 = _206.x;
                bool _237;
                if ((isunordered(_228, 0.0) || _228 > 0.0))
                {
                    bool _236;
                    if (_227 != 1u)
                    {
                        _236 = _227 != 2u;
                    }
                    else
                    {
                        _236 = false;
                    }
                    _237 = _236;
                }
                else
                {
                    _237 = true;
                }
                if (_237)
                {
                    _254 = false;
                    break;
                }
                uint _240 = current._m0[_153] >> 24u;
                if (_240 == 253u)
                {
                    _254 = true;
                    break;
                }
                if (_228 == 0.0039215688593685626983642578125)
                {
                    _254 = _240 == 254u;
                    break;
                }
                bool _253;
                if (_240 == 255u)
                {
                    _253 = _206.y > 0.0;
                }
                else
                {
                    _253 = false;
                }
                _254 = _253;
                break;
            } while(false);
            _255 = _254;
        }
        else
        {
            _255 = false;
        }
        bool _279;
        if (_255)
        {
            bool _278;
            if (_218)
            {
                bool _267;
                if (_178)
                {
                    _267 = Settings.liquidMaterial == 0u;
                }
                else
                {
                    _267 = false;
                }
                _278 = (current._m0[_153] >> 24u) == (_267 ? 253u : 254u);
            }
            else
            {
                bool _277;
                if (_160 < Settings.triangles)
                {
                    _277 = (current._m0[_153] >> 24u) == 255u;
                }
                else
                {
                    _277 = false;
                }
                _278 = _277;
            }
            _279 = _278;
        }
        else
        {
            _279 = false;
        }
        bool _286;
        if (_279)
        {
            _286 = !(isnan(_164) || isinf(_164));
        }
        else
        {
            _286 = false;
        }
        bool _290;
        if (_286)
        {
            _290 = _164 > 0.0;
        }
        else
        {
            _290 = false;
        }
        bool _295;
        if (_290)
        {
            _295 = _175.w == 1.0;
        }
        else
        {
            _295 = false;
        }
        bool _323;
        if (_295)
        {
            bool _322;
            if (_185)
            {
                bool _309;
                if (_200.w == 1.0)
                {
                    bool4 _304 = isnan(_200);
                    bool4 _305 = isinf(_200);
                    _309 = all(not(bool4(_304.x || _305.x, _304.y || _305.y, _304.z || _305.z, _304.w || _305.w)));
                }
                else
                {
                    _309 = false;
                }
                bool _315;
                if (_309)
                {
                    _315 = all(_200.xyz >= float3(0.0));
                }
                else
                {
                    _315 = false;
                }
                bool _321;
                if (_315)
                {
                    _321 = all(_200.xyz <= float3(999999995904.0));
                }
                else
                {
                    _321 = false;
                }
                _322 = _321;
            }
            else
            {
                _322 = true;
            }
            _323 = _322;
        }
        else
        {
            _323 = false;
        }
        bool _331;
        if (_323)
        {
            bool4 _326 = isnan(_175);
            bool4 _327 = isinf(_175);
            _331 = all(not(bool4(_326.x || _327.x, _326.y || _327.y, _326.z || _327.z, _326.w || _327.w)));
        }
        else
        {
            _331 = false;
        }
        bool _337;
        if (_331)
        {
            _337 = all(_175.xyz >= float3(0.0));
        }
        else
        {
            _337 = false;
        }
        bool _343;
        if (_337)
        {
            _343 = all(_175.xyz <= float3(1.0));
        }
        else
        {
            _343 = false;
        }
        bool _349;
        if (_343)
        {
            _349 = any(_175.xyz > float3(0.0));
        }
        else
        {
            _349 = false;
        }
        captured._m0[_153] = current._m0[_153];
        bool _351 = Settings.primaryPrefix == 24u;
        if (_351)
        {
            uint _354 = ((_13 * 4u) + _16) >> 2u;
            captured._m0[_354] = current._m0[_354];
        }
        if ((Settings.primaryPrefix != 20u) ? _351 : true)
        {
            uint _362 = ((_13 * (Settings.primaryPrefix - 16u)) + _24) >> 2u;
            uint _40 = _362 + 1u;
            uint _366 = current._m0[_40];
            uint _41 = _362 + 2u;
            uint _368 = current._m0[_41];
            uint _42 = _362 + 3u;
            uint _370 = current._m0[_42];
            captured._m0[_362] = current._m0[_362];
            captured._m0[_40] = _366;
            captured._m0[_41] = _368;
            captured._m0[_42] = _370;
        }
        captured._m0[_158] = _349 ? _160 : 4294967295u;
        captured._m0[_161] = _163;
        captured._m0[_165] = _167;
        captured._m0[_26] = _169;
        captured._m0[_27] = _171;
        captured._m0[_28] = _173;
        if (_185)
        {
            uint _384 = ((_13 * (Settings.primaryPrefix + 24u)) + _24) >> 2u;
            uint4 _385 = as_type<uint4>(_200);
            captured._m0[_384] = _385.x;
            captured._m0[_384 + 1u] = _385.y;
            captured._m0[_384 + 2u] = _385.z;
            captured._m0[_384 + 3u] = _385.w;
        }
        for (uint _395 = 0u; _395 < Settings.lobes; _395++)
        {
            uint _53 = (_13 * Settings.recordPrefix) + (((_15 * Settings.lobes) + _395) * Settings.pathStride);
            uint _406 = _53 >> 2u;
            uint _54 = _406 + 1u;
            uint _410 = current._m0[_54];
            uint _55 = _406 + 2u;
            uint _412 = current._m0[_55];
            uint _56 = _406 + 3u;
            uint _414 = current._m0[_56];
            captured._m0[_406] = current._m0[_406];
            captured._m0[_54] = _410;
            captured._m0[_55] = _412;
            captured._m0[_56] = _414;
            uint _419 = (_53 + 16u) >> 2u;
            uint _65 = _419 + 1u;
            uint _423 = current._m0[_65];
            uint _66 = _419 + 2u;
            uint _425 = current._m0[_66];
            uint _67 = _419 + 3u;
            uint _427 = current._m0[_67];
            captured._m0[_419] = current._m0[_419];
            captured._m0[_65] = _423;
            captured._m0[_66] = _425;
            captured._m0[_67] = _427;
            uint _432 = (_53 + 32u) >> 2u;
            uint _69 = _432 + 1u;
            uint _436 = current._m0[_69];
            uint _70 = _432 + 2u;
            uint _438 = current._m0[_70];
            uint _71 = _432 + 3u;
            uint _440 = current._m0[_71];
            captured._m0[_432] = current._m0[_432];
            captured._m0[_69] = _436;
            captured._m0[_70] = _438;
            captured._m0[_71] = _440;
            if (Settings.pathStride == 64u)
            {
                uint _449 = (_53 + 48u) >> 2u;
                uint _58 = _449 + 1u;
                uint _453 = current._m0[_58];
                uint _59 = _449 + 2u;
                uint _455 = current._m0[_59];
                uint _60 = _449 + 3u;
                uint _457 = current._m0[_60];
                captured._m0[_449] = current._m0[_449];
                captured._m0[_58] = _453;
                captured._m0[_59] = _455;
                captured._m0[_60] = _457;
            }
            else
            {
                uint _462 = (_53 + 48u) >> 2u;
                captured._m0[_462] = current._m0[_462];
            }
        }
        break;
    } while(false);
}

