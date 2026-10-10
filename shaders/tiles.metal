#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

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

kernel void feature_tiles_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& source [[buffer(1)]], device type_RWByteAddressBuffer& atlas [[buffer(2)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]], uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]])
{
    threadgroup spvUnsafeArray<float3, 64> minima;
    threadgroup spvUnsafeArray<float3, 64> maxima;
    threadgroup spvUnsafeArray<uint, 64> maskA;
    threadgroup spvUnsafeArray<uint, 64> maskB;
    uint2 _19 = (gl_WorkGroupID.xy * uint2(8u)) + gl_LocalInvocationID.xy;
    uint _20 = Settings.width * Settings.height;
    bool _153;
    if (all(_19 < uint2(Settings.width, Settings.height)))
    {
        _153 = gl_WorkGroupID.z < Settings.lobes;
    }
    else
    {
        _153 = false;
    }
    uint _295;
    uint _296;
    float3 _297;
    float3 _298;
    if (_153)
    {
        uint _22 = (_19.y * Settings.width) + _19.x;
        uint _24 = _22 * 4u;
        uint _160 = ((_20 * Settings.primaryPrefix) + _24) >> 2u;
        float _166 = as_type<float>(source._m0[((_20 * (Settings.primaryPrefix + 4u)) + _24) >> 2u]);
        uint _33 = (_20 * Settings.recordPrefix) + (((_22 * Settings.lobes) + gl_WorkGroupID.z) * Settings.pathStride);
        uint _174 = _33 >> 2u;
        uint _183 = (_33 + 16u) >> 2u;
        uint _189 = (_33 + 24u) >> 2u;
        float3 _197 = as_type<float3>(uint3(source._m0[_189], source._m0[_189 + 1u], source._m0[_189 + 2u]));
        bool _205;
        if (source._m0[_160] != 4294967295u)
        {
            _205 = !(isnan(_166) || isinf(_166));
        }
        else
        {
            _205 = false;
        }
        bool _209;
        if (_205)
        {
            _209 = _166 > 0.0;
        }
        else
        {
            _209 = false;
        }
        bool _230;
        if (_209)
        {
            uint _212 = source._m0[_183] & 7u;
            uint _215 = source._m0[_183] >> 8u;
            bool _221;
            if (_212 <= 4u)
            {
                _221 = (((source._m0[_183] >> 4u) & 15u) >> _212) == 0u;
            }
            else
            {
                _221 = false;
            }
            bool _225;
            if (_221)
            {
                _225 = _215 >= 1u;
            }
            else
            {
                _225 = false;
            }
            bool _229;
            if (_225)
            {
                _229 = _215 <= 3u;
            }
            else
            {
                _229 = false;
            }
            _230 = _229;
        }
        else
        {
            _230 = false;
        }
        bool _272;
        if (_230)
        {
            uint _233 = source._m0[_183] >> 8u;
            bool _271;
            do
            {
                bool3 _236 = isnan(_197);
                bool3 _237 = isinf(_197);
                if (!all(not(bool3(_236.x || _237.x, _236.y || _237.y, _236.z || _237.z))))
                {
                    _271 = false;
                    break;
                }
                if (_233 == 1u)
                {
                    bool _254;
                    if (_197.z == 0.0)
                    {
                        _254 = all(_197.xy >= float2(0.0));
                    }
                    else
                    {
                        _254 = false;
                    }
                    bool _260;
                    if (_254)
                    {
                        _260 = spvFAdd(_197.x, _197.y) <= 1.0;
                    }
                    else
                    {
                        _260 = false;
                    }
                    _271 = _260;
                    break;
                }
                bool _265;
                if (_233 != 2u)
                {
                    _265 = _233 == 3u;
                }
                else
                {
                    _265 = true;
                }
                bool _270;
                if (_265)
                {
                    _270 = abs(spvFSub(dot(_197, _197), 1.0)) < 0.00010099999781232327222824096679688;
                }
                else
                {
                    _270 = false;
                }
                _271 = _270;
                break;
            } while(false);
            _272 = _271;
        }
        else
        {
            _272 = false;
        }
        uint _291;
        uint _292;
        float3 _293;
        float3 _294;
        if (_272)
        {
            float3 _278 = float3(((source._m0[_183] >> 8u) == 1u) ? 0.0500099994242191314697265625 : 0.0200099982321262359619140625);
            uint _58 = (((((((((((((2166136261u ^ source._m0[_160]) * 16777619u) ^ source._m0[_174]) * 16777619u) ^ source._m0[_174 + 1u]) * 16777619u) ^ source._m0[_174 + 2u]) * 16777619u) ^ source._m0[_174 + 3u]) * 16777619u) ^ source._m0[_183]) * 16777619u) ^ source._m0[(_33 + 20u) >> 2u]) * 16777619u;
            _291 = 1u << ((_58 >> 8u) & 31u);
            _292 = 1u << (_58 & 31u);
            _293 = spvFAdd(_197, _278);
            _294 = spvFSub(_197, _278);
        }
        else
        {
            _291 = 0u;
            _292 = 0u;
            _293 = float3(-3.4028234663852885981170418348452e+38);
            _294 = float3(3.4028234663852885981170418348452e+38);
        }
        _295 = _291;
        _296 = _292;
        _297 = _293;
        _298 = _294;
    }
    else
    {
        _295 = 0u;
        _296 = 0u;
        _297 = float3(-3.4028234663852885981170418348452e+38);
        _298 = float3(3.4028234663852885981170418348452e+38);
    }
    minima[gl_LocalInvocationIndex] = _298;
    maxima[gl_LocalInvocationIndex] = _297;
    maskA[gl_LocalInvocationIndex] = _296;
    maskB[gl_LocalInvocationIndex] = _295;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint _304 = 32u; _304 > 0u; _304 = _304 >> 1u)
    {
        if (gl_LocalInvocationIndex < _304)
        {
            uint _44 = gl_LocalInvocationIndex + _304;
            minima[gl_LocalInvocationIndex] = precise::min(minima[gl_LocalInvocationIndex], minima[_44]);
            maxima[gl_LocalInvocationIndex] = precise::max(maxima[gl_LocalInvocationIndex], maxima[_44]);
            maskA[gl_LocalInvocationIndex] |= maskA[_44];
            maskB[gl_LocalInvocationIndex] |= maskB[_44];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    if (gl_LocalInvocationIndex == 0u)
    {
        uint _64 = ((((gl_WorkGroupID.z * Settings.totalNodes) + Settings.levels[0u].x) + (gl_WorkGroupID.y * Settings.levels[0u].y)) + gl_WorkGroupID.x) * 32u;
        uint _341 = _64 >> 2u;
        uint3 _344 = as_type<uint3>(minima[0]);
        atlas._m0[_341] = _344.x;
        atlas._m0[_341 + 1u] = _344.y;
        atlas._m0[_341 + 2u] = _344.z;
        atlas._m0[_341 + 3u] = maskA[0];
        uint _354 = (_64 + 16u) >> 2u;
        uint3 _357 = as_type<uint3>(maxima[0]);
        atlas._m0[_354] = _357.x;
        atlas._m0[_354 + 1u] = _357.y;
        atlas._m0[_354 + 2u] = _357.z;
        atlas._m0[_354 + 3u] = maskB[0];
    }
}

