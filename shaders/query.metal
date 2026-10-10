#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T>
[[clang::optnone]] T spvFAdd(T l, T r)
{
    return fma(T(1), l, r);
}

template<typename T>
[[clang::optnone]] T spvFSub(T l, T r)
{
    return fma(T(-1), r, l);
}

struct type_Settings
{
    uint width;
    uint height;
    uint lobes;
    uint primaryPrefix;
    uint recordPrefix;
    uint pathStride;
    uint totalNodes;
    uint levelCount;
    uint workLevel;
    uint queryCount;
    uint leafBudget;
    uint nodeBudget;
    uint4 levels[12];
};

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

kernel void feature_query_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& atlas [[buffer(1)]], device type_ByteAddressBuffer& queries [[buffer(2)]], device type_RWByteAddressBuffer& results [[buffer(3)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        if (gl_GlobalInvocationID.x >= Settings.queryCount)
        {
            break;
        }
        uint _13 = gl_GlobalInvocationID.x * 48u;
        uint _142 = _13 >> 2u;
        uint _145 = (_13 + 4u) >> 2u;
        uint _154 = (_13 + 20u) >> 2u;
        uint _160 = (_13 + 44u) >> 2u;
        uint _163 = (_13 + 28u) >> 2u;
        float3 _171 = as_type<float3>(uint3(queries._m0[_163], queries._m0[_163 + 1u], queries._m0[_163 + 2u]));
        uint _24 = gl_GlobalInvocationID.x * 272u;
        bool _178;
        if (Settings.levelCount != 0u)
        {
            _178 = Settings.levelCount > 12u;
        }
        else
        {
            _178 = true;
        }
        bool _185;
        if (!_178)
        {
            _185 = queries._m0[_160] >= Settings.lobes;
        }
        else
        {
            _185 = true;
        }
        bool _192;
        if (!_185)
        {
            _192 = Settings.leafBudget > 64u;
        }
        else
        {
            _192 = true;
        }
        bool _199;
        if (!_192)
        {
            _199 = Settings.leafBudget == 0u;
        }
        else
        {
            _199 = true;
        }
        bool _206;
        if (!_199)
        {
            _206 = Settings.nodeBudget == 0u;
        }
        else
        {
            _206 = true;
        }
        bool _211;
        if (!_206)
        {
            _211 = queries._m0[_142] == 4294967295u;
        }
        else
        {
            _211 = true;
        }
        bool _234;
        if (!_211)
        {
            uint _215 = queries._m0[_154] & 7u;
            uint _218 = queries._m0[_154] >> 8u;
            bool _224;
            if (_215 <= 4u)
            {
                _224 = (((queries._m0[_154] >> 4u) & 15u) >> _215) == 0u;
            }
            else
            {
                _224 = false;
            }
            bool _228;
            if (_224)
            {
                _228 = _218 >= 1u;
            }
            else
            {
                _228 = false;
            }
            bool _232;
            if (_228)
            {
                _232 = _218 <= 3u;
            }
            else
            {
                _232 = false;
            }
            _234 = !_232;
        }
        else
        {
            _234 = true;
        }
        bool _278;
        if (!_234)
        {
            uint _238 = queries._m0[_154] >> 8u;
            bool _276;
            do
            {
                bool3 _241 = isnan(_171);
                bool3 _242 = isinf(_171);
                if (!all(not(bool3(_241.x || _242.x, _241.y || _242.y, _241.z || _242.z))))
                {
                    _276 = false;
                    break;
                }
                if (_238 == 1u)
                {
                    bool _259;
                    if (_171.z == 0.0)
                    {
                        _259 = all(_171.xy >= float2(0.0));
                    }
                    else
                    {
                        _259 = false;
                    }
                    bool _265;
                    if (_259)
                    {
                        _265 = spvFAdd(_171.x, _171.y) <= 1.0;
                    }
                    else
                    {
                        _265 = false;
                    }
                    _276 = _265;
                    break;
                }
                bool _270;
                if (_238 != 2u)
                {
                    _270 = _238 == 3u;
                }
                else
                {
                    _270 = true;
                }
                bool _275;
                if (_270)
                {
                    _275 = abs(spvFSub(dot(_171, _171), 1.0)) < 0.00010099999781232327222824096679688;
                }
                else
                {
                    _275 = false;
                }
                _276 = _275;
                break;
            } while(false);
            _278 = !_276;
        }
        else
        {
            _278 = true;
        }
        uint _420;
        uint _421;
        uint _422;
        if (_278)
        {
            _420 = 0u;
            _421 = 4u;
            _422 = 0u;
        }
        else
        {
            uint _63 = (((((((((((((2166136261u ^ queries._m0[_142]) * 16777619u) ^ queries._m0[_145]) * 16777619u) ^ queries._m0[_145 + 1u]) * 16777619u) ^ queries._m0[_145 + 2u]) * 16777619u) ^ queries._m0[_145 + 3u]) * 16777619u) ^ queries._m0[_154]) * 16777619u) ^ queries._m0[(_13 + 24u) >> 2u]) * 16777619u;
            uint _290 = 1u << (_63 & 31u);
            uint _293 = 1u << ((_63 >> 8u) & 31u);
            uint _25 = Settings.levelCount - 1u;
            uint _295;
            uint2 _298;
            _295 = 0u;
            _298 = uint2(0u);
            uint _26;
            uint _296;
            uint2 _299;
            uint _301;
            bool _304;
            uint _418;
            uint _419;
            uint _300 = _25;
            uint _302 = 0u;
            bool _303 = false;
            for (;;)
            {
                if (!_303)
                {
                    if (_302 >= Settings.nodeBudget)
                    {
                        _418 = _302;
                        _419 = 2u;
                        break;
                    }
                    _26 = _302 + 1u;
                    uint _317 = _298.y;
                    uint _320 = _298.x;
                    uint _69 = ((((queries._m0[_160] * Settings.totalNodes) + Settings.levels[_300].x) + (_317 * Settings.levels[_300].y)) + _320) * 32u;
                    uint _321 = _69 >> 2u;
                    uint _29 = _321 + 3u;
                    uint _331 = (_69 + 16u) >> 2u;
                    uint _33 = _331 + 3u;
                    bool _347;
                    if ((atlas._m0[_29] & _290) != 0u)
                    {
                        _347 = (atlas._m0[_33] & _293) != 0u;
                    }
                    else
                    {
                        _347 = false;
                    }
                    bool _354;
                    if (_347)
                    {
                        _354 = all(_171 >= as_type<float3>(uint4(atlas._m0[_321], atlas._m0[_321 + 1u], atlas._m0[_321 + 2u], atlas._m0[_29]).xyz));
                    }
                    else
                    {
                        _354 = false;
                    }
                    bool _361;
                    if (_354)
                    {
                        _361 = all(_171 <= as_type<float3>(uint4(atlas._m0[_331], atlas._m0[_331 + 1u], atlas._m0[_331 + 2u], atlas._m0[_33]).xyz));
                    }
                    else
                    {
                        _361 = false;
                    }
                    bool _365;
                    if (_361)
                    {
                        _365 = _300 > 0u;
                    }
                    else
                    {
                        _365 = false;
                    }
                    if (_365)
                    {
                        _296 = _295;
                        _299 = _298 * uint2(2u);
                        _301 = _300 - 1u;
                        _304 = _303;
                        _295 = _296;
                        _298 = _299;
                        _300 = _301;
                        _302 = _26;
                        _303 = _304;
                        continue;
                    }
                    uint _379;
                    if (_361)
                    {
                        if (_295 == Settings.leafBudget)
                        {
                            _418 = _26;
                            _419 = 1u;
                            break;
                        }
                        results._m0[((_24 + 16u) + (_295 * 4u)) >> 2u] = (_317 * Settings.levels[0].y) + _320;
                        _379 = _295 + 1u;
                    }
                    else
                    {
                        _379 = _295;
                    }
                    uint2 _384;
                    uint _385;
                    _384 = _298;
                    _385 = _300;
                    uint2 _42;
                    uint _49;
                    bool _382;
                    uint2 _415;
                    bool _416;
                    bool _381 = false;
                    for (;;)
                    {
                        if (_385 < _25)
                        {
                            _42 = _384 / uint2(2u);
                            uint _44 = (_384.x & 1u) + (2u * (_384.y & 1u));
                            uint2 _412;
                            uint _394 = 1u;
                            for (;;)
                            {
                                if (_394 <= 3u)
                                {
                                    uint _45 = _44 + _394;
                                    if (_45 > 3u)
                                    {
                                        _412 = _384;
                                        _382 = _381;
                                        break;
                                    }
                                    uint2 _47 = (_42 * uint2(2u)) + uint2(_45 & 1u, _45 >> 1u);
                                    if (all(_47 < Settings.levels[_385].yz))
                                    {
                                        _412 = _47;
                                        _382 = true;
                                        break;
                                    }
                                    _394++;
                                    continue;
                                }
                                else
                                {
                                    _412 = _384;
                                    _382 = _381;
                                    break;
                                }
                            }
                            if (_382)
                            {
                                _415 = _412;
                                _416 = _382;
                                break;
                            }
                            _49 = _385 + 1u;
                            _381 = _382;
                            _384 = _42;
                            _385 = _49;
                            continue;
                        }
                        else
                        {
                            _415 = _384;
                            _416 = _381;
                            break;
                        }
                    }
                    _296 = _379;
                    _299 = _415;
                    _301 = _385;
                    _304 = !_416;
                    _295 = _296;
                    _298 = _299;
                    _300 = _301;
                    _302 = _26;
                    _303 = _304;
                    continue;
                }
                else
                {
                    _418 = _302;
                    _419 = 0u;
                    break;
                }
            }
            _420 = _418;
            _421 = _419;
            _422 = _295;
        }
        for (uint _424 = _422; _424 < 64u; )
        {
            results._m0[((_24 + 16u) + (_424 * 4u)) >> 2u] = 0u;
            _424++;
            continue;
        }
        uint _430 = _24 >> 2u;
        results._m0[_430] = _422;
        results._m0[_430 + 1u] = _421;
        results._m0[_430 + 2u] = _420;
        results._m0[_430 + 3u] = 0u;
        break;
    } while(false);
}

