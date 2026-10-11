#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>
#include <metal_atomic>

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

struct type_DomainSettings
{
    uint regionBudget;
    uint supportBudget;
    uint regionCapacity;
    uint domainUnused;
};

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct Point
{
    float2 x;
    float2 y;
};

__attribute__((unused)) constant int _1801 = {};

kernel void feature_domains_main(constant type_Settings& Settings [[buffer(0)]], constant type_DomainSettings& DomainSettings [[buffer(1)]], device type_ByteAddressBuffer& source [[buffer(2)]], device type_ByteAddressBuffer& queries [[buffer(3)]], device type_ByteAddressBuffer& leaves [[buffer(4)]], device type_RWByteAddressBuffer& regions [[buffer(5)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]], uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]])
{
    threadgroup uint regionCount;
    threadgroup uint regionStatus;
    threadgroup uint workCount;
    threadgroup uint errorBits;
    do
    {
        if (gl_WorkGroupID.x >= Settings.queryCount)
        {
            break;
        }
        uint _23 = gl_WorkGroupID.x * 272u;
        uint _24 = gl_WorkGroupID.x * 6160u;
        uint _25 = gl_WorkGroupID.x * 48u;
        uint _1824 = _23 >> 2u;
        uint _1826 = leaves._m0[_1824];
        bool _1827 = gl_LocalInvocationIndex == 0u;
        if (_1827)
        {
            errorBits = 0u;
            workCount = 0u;
            regionCount = 0u;
            regionStatus = leaves._m0[(_23 + 4u) >> 2u];
            bool _1841;
            if (DomainSettings.regionCapacity == 128u)
            {
                _1841 = DomainSettings.regionBudget == 0u;
            }
            else
            {
                _1841 = true;
            }
            bool _1848;
            if (!_1841)
            {
                _1848 = DomainSettings.regionBudget > 128u;
            }
            else
            {
                _1848 = true;
            }
            bool _1855;
            if (!_1848)
            {
                _1855 = DomainSettings.supportBudget == 0u;
            }
            else
            {
                _1855 = true;
            }
            bool _1862;
            if (!_1855)
            {
                _1862 = DomainSettings.supportBudget > 16384u;
            }
            else
            {
                _1862 = true;
            }
            bool _1867;
            if (!_1862)
            {
                _1867 = _1826 > 64u;
            }
            else
            {
                _1867 = true;
            }
            if (_1867)
            {
                regionStatus |= 4u;
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        uint _1872 = _25 >> 2u;
        uint _1874 = queries._m0[_1872];
        uint _1875 = (_25 + 4u) >> 2u;
        uint _1877 = queries._m0[_1875];
        uint _1879 = queries._m0[_1875 + 1u];
        uint _1881 = queries._m0[_1875 + 2u];
        uint _1883 = queries._m0[_1875 + 3u];
        uint4 _1884 = uint4(_1877, _1879, _1881, _1883);
        uint _1885 = (_25 + 20u) >> 2u;
        uint _1887 = queries._m0[_1885];
        uint _1888 = (_25 + 24u) >> 2u;
        uint _1890 = queries._m0[_1888];
        uint _1891 = (_25 + 28u) >> 2u;
        uint _1893 = queries._m0[_1891];
        uint _1895 = queries._m0[_1891 + 1u];
        uint _1897 = queries._m0[_1891 + 2u];
        float3 _1899 = as_type<float3>(uint3(_1893, _1895, _1897));
        uint _1900 = (_25 + 44u) >> 2u;
        uint _1902 = queries._m0[_1900];
        spvUnsafeArray<Point, 16> _1809;
        spvUnsafeArray<Point, 16> _1810;
        spvUnsafeArray<float2, 3> _1811;
        spvUnsafeArray<float2, 3> _1812;
        int _1907;
        int _1906;
        for (uint _1904 = 0u; _1904 < _1826; _1904++, _1906 = _1907)
        {
            if (regionStatus != 0u)
            {
                break;
            }
            uint _38 = _1904 * 4u;
            uint _1915 = ((_23 + 16u) + _38) >> 2u;
            uint _1917 = leaves._m0[_1915];
            bool _1925;
            if (_1827)
            {
                _1925 = _1917 >= (Settings.levels[0].y * Settings.levels[0].z);
            }
            else
            {
                _1925 = false;
            }
            if (_1925)
            {
                __attribute__((unused)) uint _1928 = atomic_fetch_or_explicit((threadgroup atomic_uint*)&regionStatus, 128u, memory_order_relaxed);
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (regionStatus != 0u)
            {
                break;
            }
            uint2 _44 = (uint2(_1917 % Settings.levels[0].y, _1917 / Settings.levels[0].y) * uint2(8u)) + gl_LocalInvocationID.xy;
            int2 _1937 = int2(_44);
            float3 _2072;
            bool _2073;
            do
            {
                bool _1954;
                if (!any(_1937 < int2(0)))
                {
                    _1954 = any(_1937 >= int2(int(Settings.width), int(Settings.height)));
                }
                else
                {
                    _1954 = true;
                }
                if (_1954)
                {
                    _2072 = float3(0.0);
                    _2073 = false;
                    break;
                }
                uint _85 = (uint(_1937.y) * Settings.width) + uint(_1937.x);
                uint _86 = Settings.width * Settings.height;
                uint _88 = _85 * 4u;
                if (source._m0[((_86 * Settings.primaryPrefix) + _88) >> 2u] != _1874)
                {
                    _2072 = float3(0.0);
                    _2073 = false;
                    break;
                }
                uint _94 = (_86 * Settings.recordPrefix) + (((_85 * Settings.lobes) + _1902) * Settings.pathStride);
                uint _1979 = _94 >> 2u;
                bool _1998;
                if (!any(uint4(source._m0[_1979], source._m0[_1979 + 1u], source._m0[_1979 + 2u], source._m0[_1979 + 3u]) != _1884))
                {
                    _1998 = source._m0[(_94 + 16u) >> 2u] != _1887;
                }
                else
                {
                    _1998 = true;
                }
                bool _2006;
                if (!_1998)
                {
                    _2006 = source._m0[(_94 + 20u) >> 2u] != _1890;
                }
                else
                {
                    _2006 = true;
                }
                if (_2006)
                {
                    _2072 = float3(0.0);
                    _2073 = false;
                    break;
                }
                uint _2009 = (_94 + 24u) >> 2u;
                float3 _2017 = as_type<float3>(uint3(source._m0[_2009], source._m0[_2009 + 1u], source._m0[_2009 + 2u]));
                uint _2018 = _1887 >> 8u;
                bool _2056;
                do
                {
                    bool3 _2021 = isnan(_2017);
                    bool3 _2022 = isinf(_2017);
                    if (!all(not(bool3(_2021.x || _2022.x, _2021.y || _2022.y, _2021.z || _2022.z))))
                    {
                        _2056 = false;
                        break;
                    }
                    if (_2018 == 1u)
                    {
                        bool _2039;
                        if (_2017.z == 0.0)
                        {
                            _2039 = all(_2017.xy >= float2(0.0));
                        }
                        else
                        {
                            _2039 = false;
                        }
                        bool _2045;
                        if (_2039)
                        {
                            _2045 = spvFAdd(_2017.x, _2017.y) <= 1.0;
                        }
                        else
                        {
                            _2045 = false;
                        }
                        _2056 = _2045;
                        break;
                    }
                    bool _2050;
                    if (_2018 != 2u)
                    {
                        _2050 = _2018 == 3u;
                    }
                    else
                    {
                        _2050 = true;
                    }
                    bool _2055;
                    if (_2050)
                    {
                        _2055 = abs(spvFSub(dot(_2017, _2017), 1.0)) < 0.00010099999781232327222824096679688;
                    }
                    else
                    {
                        _2055 = false;
                    }
                    _2056 = _2055;
                    break;
                } while(false);
                float _2060 = as_type<float>(source._m0[((_86 * (Settings.primaryPrefix + 4u)) + _88) >> 2u]);
                bool _2067;
                if (_2056)
                {
                    _2067 = !(isnan(_2060) || isinf(_2060));
                }
                else
                {
                    _2067 = false;
                }
                bool _2071;
                if (_2067)
                {
                    _2071 = _2060 > 0.0;
                }
                else
                {
                    _2071 = false;
                }
                _2072 = _2017;
                _2073 = _2071;
                break;
            } while(false);
            uint _2074 = _1887 >> 8u;
            bool _2075 = _2074 == 1u;
            uint2 _2081 = uint2(Settings.width, Settings.height);
            bool _2091;
            if (all(_44 < _2081) ? _2073 : false)
            {
                _2091 = all(abs(spvFSub(_2072, _1899)) <= float3(_2075 ? 0.0500009991228580474853515625 : 0.02000099979341030120849609375));
            }
            else
            {
                _2091 = false;
            }
            if (_2091)
            {
                int _2097;
                _2097 = _1906;
                int _2098;
                for (uint _2095 = 0u; _2095 < 2u; _2095++, _2097 = _2098)
                {
                    _2098 = _2097;
                    int _2103;
                    for (uint _2105 = 0u; _2105 < 2u; _2098 = _2103, _2105++)
                    {
                        if (any((_44 + uint2(_2105, _2095)) >= _2081))
                        {
                            _2103 = _2098;
                            continue;
                        }
                        uint _2114 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&workCount, 1u, memory_order_relaxed);
                        if (_2114 >= DomainSettings.supportBudget)
                        {
                            __attribute__((unused)) uint _2120 = atomic_fetch_or_explicit((threadgroup atomic_uint*)&regionStatus, 32u, memory_order_relaxed);
                            _2103 = _2098;
                            continue;
                        }
                        float3 _1807 = _1899;
                        float2 _3086;
                        float4 _3087;
                        int _3088;
                        do
                        {
                            _1809[0u] = Point{ float2(0.0), float2(0.0) };
                            bool _2124 = _2105 != 0u;
                            if (_2124)
                            {
                                _1809[1u] = Point{ float2(1.0, 0.0), float2(0.0) };
                            }
                            uint _2128 = _2124 ? 2u : 1u;
                            bool _2129 = _2095 != 0u;
                            uint _2144;
                            if (_2129)
                            {
                                __attribute__((unused)) uint _2135 = 3u;
                                __attribute__((unused)) uint _2136 = 2u;
                                uint _109 = _2124 ? 3u : 2u;
                                _1809[_2128] = Point{ float2(float(_2105), 0.0), float2(1.0, 0.0) };
                                uint _2143;
                                if (_2124)
                                {
                                    __attribute__((unused)) uint _2140 = 4u;
                                    __attribute__((unused)) uint _2141 = 3u;
                                    _1809[_109] = Point{ float2(0.0), float2(1.0, 0.0) };
                                    _2143 = _2124 ? 4u : 3u;
                                }
                                else
                                {
                                    _2143 = _109;
                                }
                                _2144 = _2143;
                            }
                            else
                            {
                                _2144 = _2128;
                            }
                            float _2145 = _2075 ? 9.9999994396249292094580596312881e-11 : 1.9999998812636476941406726837158e-07;
                            float2 _2146 = float2(_2145, 0.0);
                            bool _2149;
                            uint _2152;
                            int _2155;
                            int _2857;
                            uint _2858;
                            bool _2859;
                            bool _2148 = false;
                            uint _2151 = _2144;
                            uint _2153 = 0u;
                            int _2154 = _2098;
                            for (;;)
                            {
                                if (_2153 <= _2095)
                                {
                                    bool _2161;
                                    uint _2164;
                                    int _2167;
                                    bool _2160 = _2148;
                                    uint _2163 = _2151;
                                    uint _2165 = 0u;
                                    int _2166 = _2154;
                                    for (;;)
                                    {
                                        if (_2165 <= _2105)
                                        {
                                            int2 _2172 = int2(_44 + uint2(_2165, _2153));
                                            float3 _2297;
                                            bool _2298;
                                            do
                                            {
                                                bool _2185;
                                                if (!any(_2172 < int2(0)))
                                                {
                                                    _2185 = any(_2172 >= int2(int(Settings.width), int(Settings.height)));
                                                }
                                                else
                                                {
                                                    _2185 = true;
                                                }
                                                if (_2185)
                                                {
                                                    _2297 = float3(0.0);
                                                    _2298 = false;
                                                    break;
                                                }
                                                uint _129 = (uint(_2172.y) * Settings.width) + uint(_2172.x);
                                                uint _130 = Settings.width * Settings.height;
                                                uint _132 = _129 * 4u;
                                                if (source._m0[((_130 * Settings.primaryPrefix) + _132) >> 2u] != _1874)
                                                {
                                                    _2297 = float3(0.0);
                                                    _2298 = false;
                                                    break;
                                                }
                                                uint _138 = (_130 * Settings.recordPrefix) + (((_129 * Settings.lobes) + _1902) * Settings.pathStride);
                                                uint _2206 = _138 >> 2u;
                                                bool _2225;
                                                if (!any(uint4(source._m0[_2206], source._m0[_2206 + 1u], source._m0[_2206 + 2u], source._m0[_2206 + 3u]) != _1884))
                                                {
                                                    _2225 = source._m0[(_138 + 16u) >> 2u] != _1887;
                                                }
                                                else
                                                {
                                                    _2225 = true;
                                                }
                                                bool _2233;
                                                if (!_2225)
                                                {
                                                    _2233 = source._m0[(_138 + 20u) >> 2u] != _1890;
                                                }
                                                else
                                                {
                                                    _2233 = true;
                                                }
                                                if (_2233)
                                                {
                                                    _2297 = float3(0.0);
                                                    _2298 = false;
                                                    break;
                                                }
                                                uint _2236 = (_138 + 24u) >> 2u;
                                                float3 _2244 = as_type<float3>(uint3(source._m0[_2236], source._m0[_2236 + 1u], source._m0[_2236 + 2u]));
                                                bool _2281;
                                                do
                                                {
                                                    bool3 _2247 = isnan(_2244);
                                                    bool3 _2248 = isinf(_2244);
                                                    if (!all(not(bool3(_2247.x || _2248.x, _2247.y || _2248.y, _2247.z || _2248.z))))
                                                    {
                                                        _2281 = false;
                                                        break;
                                                    }
                                                    if (_2075)
                                                    {
                                                        bool _2264;
                                                        if (_2244.z == 0.0)
                                                        {
                                                            _2264 = all(_2244.xy >= float2(0.0));
                                                        }
                                                        else
                                                        {
                                                            _2264 = false;
                                                        }
                                                        bool _2270;
                                                        if (_2264)
                                                        {
                                                            _2270 = spvFAdd(_2244.x, _2244.y) <= 1.0;
                                                        }
                                                        else
                                                        {
                                                            _2270 = false;
                                                        }
                                                        _2281 = _2270;
                                                        break;
                                                    }
                                                    bool _2275;
                                                    if (_2074 != 2u)
                                                    {
                                                        _2275 = _2074 == 3u;
                                                    }
                                                    else
                                                    {
                                                        _2275 = true;
                                                    }
                                                    bool _2280;
                                                    if (_2275)
                                                    {
                                                        _2280 = abs(spvFSub(dot(_2244, _2244), 1.0)) < 0.00010099999781232327222824096679688;
                                                    }
                                                    else
                                                    {
                                                        _2280 = false;
                                                    }
                                                    _2281 = _2280;
                                                    break;
                                                } while(false);
                                                float _2285 = as_type<float>(source._m0[((_130 * (Settings.primaryPrefix + 4u)) + _132) >> 2u]);
                                                bool _2292;
                                                if (_2281)
                                                {
                                                    _2292 = !(isnan(_2285) || isinf(_2285));
                                                }
                                                else
                                                {
                                                    _2292 = false;
                                                }
                                                bool _2296;
                                                if (_2292)
                                                {
                                                    _2296 = _2285 > 0.0;
                                                }
                                                else
                                                {
                                                    _2296 = false;
                                                }
                                                _2297 = _2244;
                                                _2298 = _2296;
                                                break;
                                            } while(false);
                                            float3 _1808 = _2297;
                                            if (!_2298)
                                            {
                                                _2155 = 0;
                                                _2152 = _2163;
                                                _2149 = true;
                                                break;
                                            }
                                            _1812[0u] = float2(0.0);
                                            _1811[0u] = float2(0.0);
                                            _1812[1u] = float2(0.0);
                                            _1811[1u] = float2(0.0);
                                            _1812[2u] = float2(0.0);
                                            _1811[2u] = float2(0.0);
                                            for (uint _2309 = 0u; _2309 < 2u; _2309++)
                                            {
                                                for (int _2315 = -1; _2315 <= 1; _2315 += 2)
                                                {
                                                    bool _2320 = _2309 == 0u;
                                                    int2 _112 = _2172 + int2(_2320 ? _2315 : 0, (_2309 == 1u) ? _2315 : 0);
                                                    float3 _2434;
                                                    bool _2435;
                                                    do
                                                    {
                                                        bool _2337;
                                                        if (!any(_112 < int2(0)))
                                                        {
                                                            _2337 = any(_112 >= int2(int(Settings.width), int(Settings.height)));
                                                        }
                                                        else
                                                        {
                                                            _2337 = true;
                                                        }
                                                        if (_2337)
                                                        {
                                                            _2434 = float3(0.0);
                                                            _2435 = false;
                                                            break;
                                                        }
                                                        uint _154 = (uint(_112.y) * Settings.width) + uint(_112.x);
                                                        uint _155 = Settings.width * Settings.height;
                                                        if (source._m0[((_155 * Settings.primaryPrefix) + (_154 * 4u)) >> 2u] != _1874)
                                                        {
                                                            _2434 = float3(0.0);
                                                            _2435 = false;
                                                            break;
                                                        }
                                                        uint _163 = (_155 * Settings.recordPrefix) + (((_154 * Settings.lobes) + _1902) * Settings.pathStride);
                                                        uint _2358 = _163 >> 2u;
                                                        bool _2377;
                                                        if (!any(uint4(source._m0[_2358], source._m0[_2358 + 1u], source._m0[_2358 + 2u], source._m0[_2358 + 3u]) != _1884))
                                                        {
                                                            _2377 = source._m0[(_163 + 16u) >> 2u] != _1887;
                                                        }
                                                        else
                                                        {
                                                            _2377 = true;
                                                        }
                                                        bool _2385;
                                                        if (!_2377)
                                                        {
                                                            _2385 = source._m0[(_163 + 20u) >> 2u] != _1890;
                                                        }
                                                        else
                                                        {
                                                            _2385 = true;
                                                        }
                                                        if (_2385)
                                                        {
                                                            _2434 = float3(0.0);
                                                            _2435 = false;
                                                            break;
                                                        }
                                                        uint _2388 = (_163 + 24u) >> 2u;
                                                        float3 _2396 = as_type<float3>(uint3(source._m0[_2388], source._m0[_2388 + 1u], source._m0[_2388 + 2u]));
                                                        bool _2433;
                                                        do
                                                        {
                                                            bool3 _2399 = isnan(_2396);
                                                            bool3 _2400 = isinf(_2396);
                                                            if (!all(not(bool3(_2399.x || _2400.x, _2399.y || _2400.y, _2399.z || _2400.z))))
                                                            {
                                                                _2433 = false;
                                                                break;
                                                            }
                                                            if (_2075)
                                                            {
                                                                bool _2416;
                                                                if (_2396.z == 0.0)
                                                                {
                                                                    _2416 = all(_2396.xy >= float2(0.0));
                                                                }
                                                                else
                                                                {
                                                                    _2416 = false;
                                                                }
                                                                bool _2422;
                                                                if (_2416)
                                                                {
                                                                    _2422 = spvFAdd(_2396.x, _2396.y) <= 1.0;
                                                                }
                                                                else
                                                                {
                                                                    _2422 = false;
                                                                }
                                                                _2433 = _2422;
                                                                break;
                                                            }
                                                            bool _2427;
                                                            if (_2074 != 2u)
                                                            {
                                                                _2427 = _2074 == 3u;
                                                            }
                                                            else
                                                            {
                                                                _2427 = true;
                                                            }
                                                            bool _2432;
                                                            if (_2427)
                                                            {
                                                                _2432 = abs(spvFSub(dot(_2396, _2396), 1.0)) < 0.00010099999781232327222824096679688;
                                                            }
                                                            else
                                                            {
                                                                _2432 = false;
                                                            }
                                                            _2433 = _2432;
                                                            break;
                                                        } while(false);
                                                        _2434 = _2396;
                                                        _2435 = _2433;
                                                        break;
                                                    } while(false);
                                                    if (_2435)
                                                    {
                                                        float2 _175 = -float2(_1808.x, 0.0);
                                                        float _2442 = _175.x;
                                                        float _2443 = _175.y;
                                                        float _176 = spvFAdd(_2434.x, _2442);
                                                        float _177 = spvFSub(_176, _2434.x);
                                                        float _182 = spvFAdd(0.0, _2443);
                                                        float _183 = spvFSub(_182, 0.0);
                                                        float _188 = spvFAdd(spvFAdd(spvFSub(_2434.x, spvFSub(_176, _177)), spvFSub(_2442, _177)), _182);
                                                        float _189 = spvFAdd(_176, _188);
                                                        float _192 = spvFAdd(spvFSub(_188, spvFSub(_189, _176)), spvFAdd(spvFSub(0.0, spvFSub(_182, _183)), spvFSub(_2443, _183)));
                                                        float _193 = spvFAdd(_189, _192);
                                                        float _195 = spvFSub(_192, spvFSub(_193, _189));
                                                        float2 _2444 = float2(_193, _195);
                                                        bool _2453;
                                                        if ((isunordered(_193, 0.0) || _193 >= 0.0))
                                                        {
                                                            bool _2452;
                                                            if (_193 == 0.0)
                                                            {
                                                                _2452 = _195 < 0.0;
                                                            }
                                                            else
                                                            {
                                                                _2452 = false;
                                                            }
                                                            _2453 = _2452;
                                                        }
                                                        else
                                                        {
                                                            _2453 = true;
                                                        }
                                                        float2 _2457;
                                                        if (_2453)
                                                        {
                                                            _2457 = -_2444;
                                                        }
                                                        else
                                                        {
                                                            _2457 = _2444;
                                                        }
                                                        float _197 = spvFMul(_2457.x, 1.5);
                                                        float _198 = spvFMul(_2457.x, 4097.0);
                                                        float _200 = spvFSub(_198, spvFSub(_198, _2457.x));
                                                        float _201 = spvFSub(_2457.x, _200);
                                                        float _202 = spvFMul(1.5, 4097.0);
                                                        float _204 = spvFSub(_202, spvFSub(_202, 1.5));
                                                        float _205 = spvFSub(1.5, _204);
                                                        float _219 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_200, _204), _197), spvFMul(_200, _205)), spvFMul(_201, _204)), spvFMul(_201, _205)), spvFMul(_2457.x, 0.0)), spvFMul(_2457.y, 1.5)), spvFMul(_2457.y, 0.0));
                                                        float _220 = spvFAdd(_197, _219);
                                                        float _222 = spvFSub(_219, spvFSub(_220, _197));
                                                        float2 _2460 = float2(_220, _222);
                                                        if (_2320)
                                                        {
                                                            bool _2475;
                                                            if ((isunordered(_1811[0u].x, _220) || _1811[0u].x >= _220))
                                                            {
                                                                bool _2474;
                                                                if (_1811[0u].x == _220)
                                                                {
                                                                    _2474 = _1811[0u].y < _222;
                                                                }
                                                                else
                                                                {
                                                                    _2474 = false;
                                                                }
                                                                _2475 = _2474;
                                                            }
                                                            else
                                                            {
                                                                _2475 = true;
                                                            }
                                                            _1811[0u] = select(_1811[0u], _2460, bool2(_2475));
                                                        }
                                                        else
                                                        {
                                                            bool _2489;
                                                            if ((isunordered(_1812[0u].x, _220) || _1812[0u].x >= _220))
                                                            {
                                                                bool _2488;
                                                                if (_1812[0u].x == _220)
                                                                {
                                                                    _2488 = _1812[0u].y < _222;
                                                                }
                                                                else
                                                                {
                                                                    _2488 = false;
                                                                }
                                                                _2489 = _2488;
                                                            }
                                                            else
                                                            {
                                                                _2489 = true;
                                                            }
                                                            _1812[0u] = select(_1812[0u], _2460, bool2(_2489));
                                                        }
                                                        float2 _1621 = -float2(_1808.y, 0.0);
                                                        float _2496 = _1621.x;
                                                        float _2497 = _1621.y;
                                                        float _1622 = spvFAdd(_2434.y, _2496);
                                                        float _1623 = spvFSub(_1622, _2434.y);
                                                        float _1628 = spvFAdd(0.0, _2497);
                                                        float _1629 = spvFSub(_1628, 0.0);
                                                        float _1634 = spvFAdd(spvFAdd(spvFSub(_2434.y, spvFSub(_1622, _1623)), spvFSub(_2496, _1623)), _1628);
                                                        float _1635 = spvFAdd(_1622, _1634);
                                                        float _1638 = spvFAdd(spvFSub(_1634, spvFSub(_1635, _1622)), spvFAdd(spvFSub(0.0, spvFSub(_1628, _1629)), spvFSub(_2497, _1629)));
                                                        float _1639 = spvFAdd(_1635, _1638);
                                                        float _1641 = spvFSub(_1638, spvFSub(_1639, _1635));
                                                        float2 _2498 = float2(_1639, _1641);
                                                        bool _2507;
                                                        if ((isunordered(_1639, 0.0) || _1639 >= 0.0))
                                                        {
                                                            bool _2506;
                                                            if (_1639 == 0.0)
                                                            {
                                                                _2506 = _1641 < 0.0;
                                                            }
                                                            else
                                                            {
                                                                _2506 = false;
                                                            }
                                                            _2507 = _2506;
                                                        }
                                                        else
                                                        {
                                                            _2507 = true;
                                                        }
                                                        float2 _2511;
                                                        if (_2507)
                                                        {
                                                            _2511 = -_2498;
                                                        }
                                                        else
                                                        {
                                                            _2511 = _2498;
                                                        }
                                                        float _1643 = spvFMul(_2511.x, 1.5);
                                                        float _1644 = spvFMul(_2511.x, 4097.0);
                                                        float _1646 = spvFSub(_1644, spvFSub(_1644, _2511.x));
                                                        float _1647 = spvFSub(_2511.x, _1646);
                                                        float _1661 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1646, _204), _1643), spvFMul(_1646, _205)), spvFMul(_1647, _204)), spvFMul(_1647, _205)), spvFMul(_2511.x, 0.0)), spvFMul(_2511.y, 1.5)), spvFMul(_2511.y, 0.0));
                                                        float _1662 = spvFAdd(_1643, _1661);
                                                        float _1664 = spvFSub(_1661, spvFSub(_1662, _1643));
                                                        float2 _2514 = float2(_1662, _1664);
                                                        if (_2320)
                                                        {
                                                            bool _2543;
                                                            if ((isunordered(_1811[1u].x, _1662) || _1811[1u].x >= _1662))
                                                            {
                                                                bool _2542;
                                                                if (_1811[1u].x == _1662)
                                                                {
                                                                    _2542 = _1811[1u].y < _1664;
                                                                }
                                                                else
                                                                {
                                                                    _2542 = false;
                                                                }
                                                                _2543 = _2542;
                                                            }
                                                            else
                                                            {
                                                                _2543 = true;
                                                            }
                                                            _1811[1u] = select(_1811[1u], _2514, bool2(_2543));
                                                        }
                                                        else
                                                        {
                                                            bool _2529;
                                                            if ((isunordered(_1812[1u].x, _1662) || _1812[1u].x >= _1662))
                                                            {
                                                                bool _2528;
                                                                if (_1812[1u].x == _1662)
                                                                {
                                                                    _2528 = _1812[1u].y < _1664;
                                                                }
                                                                else
                                                                {
                                                                    _2528 = false;
                                                                }
                                                                _2529 = _2528;
                                                            }
                                                            else
                                                            {
                                                                _2529 = true;
                                                            }
                                                            _1812[1u] = select(_1812[1u], _2514, bool2(_2529));
                                                        }
                                                        float2 _1665 = -float2(_1808.z, 0.0);
                                                        float _2550 = _1665.x;
                                                        float _2551 = _1665.y;
                                                        float _1666 = spvFAdd(_2434.z, _2550);
                                                        float _1667 = spvFSub(_1666, _2434.z);
                                                        float _1672 = spvFAdd(0.0, _2551);
                                                        float _1673 = spvFSub(_1672, 0.0);
                                                        float _1678 = spvFAdd(spvFAdd(spvFSub(_2434.z, spvFSub(_1666, _1667)), spvFSub(_2550, _1667)), _1672);
                                                        float _1679 = spvFAdd(_1666, _1678);
                                                        float _1682 = spvFAdd(spvFSub(_1678, spvFSub(_1679, _1666)), spvFAdd(spvFSub(0.0, spvFSub(_1672, _1673)), spvFSub(_2551, _1673)));
                                                        float _1683 = spvFAdd(_1679, _1682);
                                                        float _1685 = spvFSub(_1682, spvFSub(_1683, _1679));
                                                        float2 _2552 = float2(_1683, _1685);
                                                        bool _2561;
                                                        if ((isunordered(_1683, 0.0) || _1683 >= 0.0))
                                                        {
                                                            bool _2560;
                                                            if (_1683 == 0.0)
                                                            {
                                                                _2560 = _1685 < 0.0;
                                                            }
                                                            else
                                                            {
                                                                _2560 = false;
                                                            }
                                                            _2561 = _2560;
                                                        }
                                                        else
                                                        {
                                                            _2561 = true;
                                                        }
                                                        float2 _2565;
                                                        if (_2561)
                                                        {
                                                            _2565 = -_2552;
                                                        }
                                                        else
                                                        {
                                                            _2565 = _2552;
                                                        }
                                                        float _1687 = spvFMul(_2565.x, 1.5);
                                                        float _1688 = spvFMul(_2565.x, 4097.0);
                                                        float _1690 = spvFSub(_1688, spvFSub(_1688, _2565.x));
                                                        float _1691 = spvFSub(_2565.x, _1690);
                                                        float _1705 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1690, _204), _1687), spvFMul(_1690, _205)), spvFMul(_1691, _204)), spvFMul(_1691, _205)), spvFMul(_2565.x, 0.0)), spvFMul(_2565.y, 1.5)), spvFMul(_2565.y, 0.0));
                                                        float _1706 = spvFAdd(_1687, _1705);
                                                        float _1708 = spvFSub(_1705, spvFSub(_1706, _1687));
                                                        float2 _2568 = float2(_1706, _1708);
                                                        if (_2320)
                                                        {
                                                            bool _2597;
                                                            if ((isunordered(_1811[2u].x, _1706) || _1811[2u].x >= _1706))
                                                            {
                                                                bool _2596;
                                                                if (_1811[2u].x == _1706)
                                                                {
                                                                    _2596 = _1811[2u].y < _1708;
                                                                }
                                                                else
                                                                {
                                                                    _2596 = false;
                                                                }
                                                                _2597 = _2596;
                                                            }
                                                            else
                                                            {
                                                                _2597 = true;
                                                            }
                                                            _1811[2u] = select(_1811[2u], _2568, bool2(_2597));
                                                        }
                                                        else
                                                        {
                                                            bool _2583;
                                                            if ((isunordered(_1812[2u].x, _1706) || _1812[2u].x >= _1706))
                                                            {
                                                                bool _2582;
                                                                if (_1812[2u].x == _1706)
                                                                {
                                                                    _2582 = _1812[2u].y < _1708;
                                                                }
                                                                else
                                                                {
                                                                    _2582 = false;
                                                                }
                                                                _2583 = _2582;
                                                            }
                                                            else
                                                            {
                                                                _2583 = true;
                                                            }
                                                            _1812[2u] = select(_1812[2u], _2568, bool2(_2583));
                                                        }
                                                    }
                                                }
                                            }
                                            _2164 = _2163;
                                            bool _2602;
                                            uint _2604;
                                            int _2607;
                                            bool _2601 = _2160;
                                            uint _2605 = 0u;
                                            int _2606 = _2166;
                                            for (;;)
                                            {
                                                if (_2605 < 3u)
                                                {
                                                    float2 _223 = -float2(_1807[_2605], 0.0);
                                                    float _2616 = _223.x;
                                                    float _2617 = _223.y;
                                                    float _224 = spvFAdd(_1808[_2605], _2616);
                                                    float _225 = spvFSub(_224, _1808[_2605]);
                                                    float _230 = spvFAdd(0.0, _2617);
                                                    float _231 = spvFSub(_230, 0.0);
                                                    float _236 = spvFAdd(spvFAdd(spvFSub(_1808[_2605], spvFSub(_224, _225)), spvFSub(_2616, _225)), _230);
                                                    float _237 = spvFAdd(_224, _236);
                                                    float _240 = spvFAdd(spvFSub(_236, spvFSub(_237, _224)), spvFAdd(spvFSub(0.0, spvFSub(_230, _231)), spvFSub(_2617, _231)));
                                                    float _241 = spvFAdd(_237, _240);
                                                    float _243 = spvFSub(_240, spvFSub(_241, _237));
                                                    float2 _2618 = float2(_241, _243);
                                                    bool _2627;
                                                    if ((isunordered(_241, 0.0) || _241 >= 0.0))
                                                    {
                                                        bool _2626;
                                                        if (_241 == 0.0)
                                                        {
                                                            _2626 = _243 < 0.0;
                                                        }
                                                        else
                                                        {
                                                            _2626 = false;
                                                        }
                                                        _2627 = _2626;
                                                    }
                                                    else
                                                    {
                                                        _2627 = true;
                                                    }
                                                    float2 _2631;
                                                    if (_2627)
                                                    {
                                                        _2631 = -_2618;
                                                    }
                                                    else
                                                    {
                                                        _2631 = _2618;
                                                    }
                                                    float _2633 = float(_2075 ? 20 : 50);
                                                    float _245 = 1.0 / _2633;
                                                    float _253 = spvFMul(_245, _2633);
                                                    float _254 = spvFMul(_245, 4097.0);
                                                    float _256 = spvFSub(_254, spvFSub(_254, _245));
                                                    float _257 = spvFSub(_245, _256);
                                                    float _258 = spvFMul(_2633, 4097.0);
                                                    float _260 = spvFSub(_258, spvFSub(_258, _2633));
                                                    float _261 = spvFSub(_2633, _260);
                                                    float _274 = spvFMul(0.0, 0.0);
                                                    float _275 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_256, _260), _253), spvFMul(_256, _261)), spvFMul(_257, _260)), spvFMul(_257, _261)), spvFMul(_245, 0.0)), spvFMul(0.0, _2633)), _274);
                                                    float _276 = spvFAdd(_253, _275);
                                                    float _246 = -_276;
                                                    float _247 = -spvFSub(_275, spvFSub(_276, _253));
                                                    float _279 = spvFAdd(1.0, _246);
                                                    float _280 = spvFSub(_279, 1.0);
                                                    float _285 = spvFAdd(0.0, _247);
                                                    float _286 = spvFSub(_285, 0.0);
                                                    float _291 = spvFAdd(spvFAdd(spvFSub(1.0, spvFSub(_279, _280)), spvFSub(_246, _280)), _285);
                                                    float _292 = spvFAdd(_279, _291);
                                                    float _248 = spvFAdd(_292, spvFAdd(spvFSub(_291, spvFSub(_292, _279)), spvFAdd(spvFSub(0.0, spvFSub(_285, _286)), spvFSub(_247, _286)))) / _2633;
                                                    float _297 = spvFAdd(_245, _248);
                                                    float _298 = spvFSub(_297, _245);
                                                    float _303 = spvFAdd(0.0, 0.0);
                                                    float _304 = spvFSub(_303, 0.0);
                                                    float _308 = spvFAdd(spvFSub(0.0, spvFSub(_303, _304)), spvFSub(0.0, _304));
                                                    float _309 = spvFAdd(spvFAdd(spvFSub(_245, spvFSub(_297, _298)), spvFSub(_248, _298)), _303);
                                                    float _310 = spvFAdd(_297, _309);
                                                    float _313 = spvFAdd(spvFSub(_309, spvFSub(_310, _297)), _308);
                                                    float _314 = spvFAdd(_310, _313);
                                                    float _316 = spvFSub(_313, spvFSub(_314, _310));
                                                    float _317 = spvFMul(_314, _2633);
                                                    float _318 = spvFMul(_314, 4097.0);
                                                    float _320 = spvFSub(_318, spvFSub(_318, _314));
                                                    float _321 = spvFSub(_314, _320);
                                                    float _335 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_320, _260), _317), spvFMul(_320, _261)), spvFMul(_321, _260)), spvFMul(_321, _261)), spvFMul(_314, 0.0)), spvFMul(_316, _2633)), spvFMul(_316, 0.0));
                                                    float _336 = spvFAdd(_317, _335);
                                                    float _249 = -_336;
                                                    float _250 = -spvFSub(_335, spvFSub(_336, _317));
                                                    float _339 = spvFAdd(1.0, _249);
                                                    float _340 = spvFSub(_339, 1.0);
                                                    float _345 = spvFAdd(0.0, _250);
                                                    float _346 = spvFSub(_345, 0.0);
                                                    float _351 = spvFAdd(spvFAdd(spvFSub(1.0, spvFSub(_339, _340)), spvFSub(_249, _340)), _345);
                                                    float _352 = spvFAdd(_339, _351);
                                                    float _355 = spvFAdd(spvFSub(_351, spvFSub(_352, _339)), spvFAdd(spvFSub(0.0, spvFSub(_345, _346)), spvFSub(_250, _346)));
                                                    float _356 = spvFAdd(_352, _355);
                                                    float _252 = spvFAdd(_356, spvFSub(_355, spvFSub(_356, _352))) / _2633;
                                                    float _359 = spvFAdd(_314, _252);
                                                    float _360 = spvFSub(_359, _314);
                                                    float _365 = spvFAdd(_316, 0.0);
                                                    float _366 = spvFSub(_365, _316);
                                                    float _371 = spvFAdd(spvFAdd(spvFSub(_314, spvFSub(_359, _360)), spvFSub(_252, _360)), _365);
                                                    float _372 = spvFAdd(_359, _371);
                                                    float _375 = spvFAdd(spvFSub(_371, spvFSub(_372, _359)), spvFAdd(spvFSub(_316, spvFSub(_365, _366)), spvFSub(0.0, _366)));
                                                    float _376 = spvFAdd(_372, _375);
                                                    float _378 = spvFSub(_375, spvFSub(_376, _372));
                                                    float _379 = spvFAdd(_376, _2145);
                                                    float _380 = spvFSub(_379, _376);
                                                    float _385 = spvFAdd(_378, 0.0);
                                                    float _386 = spvFSub(_385, _378);
                                                    float _391 = spvFAdd(spvFAdd(spvFSub(_376, spvFSub(_379, _380)), spvFSub(_2145, _380)), _385);
                                                    float _392 = spvFAdd(_379, _391);
                                                    float _395 = spvFAdd(spvFSub(_391, spvFSub(_392, _379)), spvFAdd(spvFSub(_378, spvFSub(_385, _386)), spvFSub(0.0, _386)));
                                                    float _396 = spvFAdd(_392, _395);
                                                    bool _2644;
                                                    if ((isunordered(_396, _2631.x) || _396 >= _2631.x))
                                                    {
                                                        bool _2643;
                                                        if (_396 == _2631.x)
                                                        {
                                                            _2643 = spvFSub(_395, spvFSub(_396, _392)) < _2631.y;
                                                        }
                                                        else
                                                        {
                                                            _2643 = false;
                                                        }
                                                        _2644 = _2643;
                                                    }
                                                    else
                                                    {
                                                        _2644 = true;
                                                    }
                                                    if (_2644)
                                                    {
                                                        _2167 = 0;
                                                        _2161 = true;
                                                        break;
                                                    }
                                                    float2 _2659;
                                                    if (_2124)
                                                    {
                                                        float2 _2658;
                                                        if (_2165 != 0u)
                                                        {
                                                            _2658 = -_1811[_2605];
                                                        }
                                                        else
                                                        {
                                                            _2658 = _1811[_2605];
                                                        }
                                                        _2659 = _2658;
                                                    }
                                                    else
                                                    {
                                                        _2659 = float2(0.0);
                                                    }
                                                    float2 _2672;
                                                    if (_2129)
                                                    {
                                                        float2 _2671;
                                                        if (_2153 != 0u)
                                                        {
                                                            _2671 = -_1812[_2605];
                                                        }
                                                        else
                                                        {
                                                            _2671 = _1812[_2605];
                                                        }
                                                        _2672 = _2671;
                                                    }
                                                    else
                                                    {
                                                        _2672 = float2(0.0);
                                                    }
                                                    float _399 = 2.0 / 1000000.0;
                                                    float _407 = spvFMul(_399, 1000000.0);
                                                    float _408 = spvFMul(_399, 4097.0);
                                                    float _410 = spvFSub(_408, spvFSub(_408, _399));
                                                    float _411 = spvFSub(_399, _410);
                                                    float _412 = spvFMul(1000000.0, 4097.0);
                                                    float _414 = spvFSub(_412, spvFSub(_412, 1000000.0));
                                                    float _415 = spvFSub(1000000.0, _414);
                                                    float _428 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_410, _414), _407), spvFMul(_410, _415)), spvFMul(_411, _414)), spvFMul(_411, _415)), spvFMul(_399, 0.0)), spvFMul(0.0, 1000000.0)), _274);
                                                    float _429 = spvFAdd(_407, _428);
                                                    float _400 = -_429;
                                                    float _401 = -spvFSub(_428, spvFSub(_429, _407));
                                                    float _432 = spvFAdd(2.0, _400);
                                                    float _433 = spvFSub(_432, 2.0);
                                                    float _438 = spvFAdd(0.0, _401);
                                                    float _439 = spvFSub(_438, 0.0);
                                                    float _444 = spvFAdd(spvFAdd(spvFSub(2.0, spvFSub(_432, _433)), spvFSub(_400, _433)), _438);
                                                    float _445 = spvFAdd(_432, _444);
                                                    float _402 = spvFAdd(_445, spvFAdd(spvFSub(_444, spvFSub(_445, _432)), spvFAdd(spvFSub(0.0, spvFSub(_438, _439)), spvFSub(_401, _439)))) / 1000000.0;
                                                    float _450 = spvFAdd(_399, _402);
                                                    float _451 = spvFSub(_450, _399);
                                                    float _456 = spvFAdd(spvFAdd(spvFSub(_399, spvFSub(_450, _451)), spvFSub(_402, _451)), _303);
                                                    float _457 = spvFAdd(_450, _456);
                                                    float _460 = spvFAdd(spvFSub(_456, spvFSub(_457, _450)), _308);
                                                    float _461 = spvFAdd(_457, _460);
                                                    float _463 = spvFSub(_460, spvFSub(_461, _457));
                                                    float _464 = spvFMul(_461, 1000000.0);
                                                    float _465 = spvFMul(_461, 4097.0);
                                                    float _467 = spvFSub(_465, spvFSub(_465, _461));
                                                    float _468 = spvFSub(_461, _467);
                                                    float _482 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_467, _414), _464), spvFMul(_467, _415)), spvFMul(_468, _414)), spvFMul(_468, _415)), spvFMul(_461, 0.0)), spvFMul(_463, 1000000.0)), spvFMul(_463, 0.0));
                                                    float _483 = spvFAdd(_464, _482);
                                                    float _403 = -_483;
                                                    float _404 = -spvFSub(_482, spvFSub(_483, _464));
                                                    float _486 = spvFAdd(2.0, _403);
                                                    float _487 = spvFSub(_486, 2.0);
                                                    float _492 = spvFAdd(0.0, _404);
                                                    float _493 = spvFSub(_492, 0.0);
                                                    float _498 = spvFAdd(spvFAdd(spvFSub(2.0, spvFSub(_486, _487)), spvFSub(_403, _487)), _492);
                                                    float _499 = spvFAdd(_486, _498);
                                                    float _502 = spvFAdd(spvFSub(_498, spvFSub(_499, _486)), spvFAdd(spvFSub(0.0, spvFSub(_492, _493)), spvFSub(_404, _493)));
                                                    float _503 = spvFAdd(_499, _502);
                                                    float _406 = spvFAdd(_503, spvFSub(_502, spvFSub(_503, _499))) / 1000000.0;
                                                    float _506 = spvFAdd(_461, _406);
                                                    float _507 = spvFSub(_506, _461);
                                                    float _512 = spvFAdd(_463, 0.0);
                                                    float _513 = spvFSub(_512, _463);
                                                    float _518 = spvFAdd(spvFAdd(spvFSub(_461, spvFSub(_506, _507)), spvFSub(_406, _507)), _512);
                                                    float _519 = spvFAdd(_506, _518);
                                                    float _522 = spvFAdd(spvFSub(_518, spvFSub(_519, _506)), spvFAdd(spvFSub(_463, spvFSub(_512, _513)), spvFSub(0.0, _513)));
                                                    float _523 = spvFAdd(_519, _522);
                                                    float2 _526 = -float2(_523, spvFSub(_522, spvFSub(_523, _519)));
                                                    float _2675 = _526.x;
                                                    float _2676 = _526.y;
                                                    float _527 = spvFAdd(_2631.x, _2675);
                                                    float _528 = spvFSub(_527, _2631.x);
                                                    float _533 = spvFAdd(_2631.y, _2676);
                                                    float _534 = spvFSub(_533, _2631.y);
                                                    float _539 = spvFAdd(spvFAdd(spvFSub(_2631.x, spvFSub(_527, _528)), spvFSub(_2675, _528)), _533);
                                                    float _540 = spvFAdd(_527, _539);
                                                    float _543 = spvFAdd(spvFSub(_539, spvFSub(_540, _527)), spvFAdd(spvFSub(_2631.y, spvFSub(_533, _534)), spvFSub(_2676, _534)));
                                                    float _544 = spvFAdd(_540, _543);
                                                    float _546 = spvFSub(_543, spvFSub(_544, _540));
                                                    float _2679 = float(_2165);
                                                    float _547 = spvFMul(_1811[_2605].x, _2679);
                                                    float _548 = spvFMul(_1811[_2605].x, 4097.0);
                                                    float _550 = spvFSub(_548, spvFSub(_548, _1811[_2605].x));
                                                    float _551 = spvFSub(_1811[_2605].x, _550);
                                                    float _552 = spvFMul(_2679, 4097.0);
                                                    float _554 = spvFSub(_552, spvFSub(_552, _2679));
                                                    float _555 = spvFSub(_2679, _554);
                                                    float _569 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_550, _554), _547), spvFMul(_550, _555)), spvFMul(_551, _554)), spvFMul(_551, _555)), spvFMul(_1811[_2605].x, 0.0)), spvFMul(_1811[_2605].y, _2679)), spvFMul(_1811[_2605].y, 0.0));
                                                    float _570 = spvFAdd(_547, _569);
                                                    float2 _573 = -float2(_570, spvFSub(_569, spvFSub(_570, _547)));
                                                    float _2683 = _573.x;
                                                    float _2684 = _573.y;
                                                    float _574 = spvFAdd(_544, _2683);
                                                    float _575 = spvFSub(_574, _544);
                                                    float _580 = spvFAdd(_546, _2684);
                                                    float _581 = spvFSub(_580, _546);
                                                    float _586 = spvFAdd(spvFAdd(spvFSub(_544, spvFSub(_574, _575)), spvFSub(_2683, _575)), _580);
                                                    float _587 = spvFAdd(_574, _586);
                                                    float _590 = spvFAdd(spvFSub(_586, spvFSub(_587, _574)), spvFAdd(spvFSub(_546, spvFSub(_580, _581)), spvFSub(_2684, _581)));
                                                    float _591 = spvFAdd(_587, _590);
                                                    float _593 = spvFSub(_590, spvFSub(_591, _587));
                                                    float _2687 = float(_2153);
                                                    float _594 = spvFMul(_1812[_2605].x, _2687);
                                                    float _595 = spvFMul(_1812[_2605].x, 4097.0);
                                                    float _597 = spvFSub(_595, spvFSub(_595, _1812[_2605].x));
                                                    float _598 = spvFSub(_1812[_2605].x, _597);
                                                    float _599 = spvFMul(_2687, 4097.0);
                                                    float _601 = spvFSub(_599, spvFSub(_599, _2687));
                                                    float _602 = spvFSub(_2687, _601);
                                                    float _616 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_597, _601), _594), spvFMul(_597, _602)), spvFMul(_598, _601)), spvFMul(_598, _602)), spvFMul(_1812[_2605].x, 0.0)), spvFMul(_1812[_2605].y, _2687)), spvFMul(_1812[_2605].y, 0.0));
                                                    float _617 = spvFAdd(_594, _616);
                                                    float2 _620 = -float2(_617, spvFSub(_616, spvFSub(_617, _594)));
                                                    float _2691 = _620.x;
                                                    float _2692 = _620.y;
                                                    float _621 = spvFAdd(_591, _2691);
                                                    float _622 = spvFSub(_621, _591);
                                                    float _627 = spvFAdd(_593, _2692);
                                                    float _628 = spvFSub(_627, _593);
                                                    float _633 = spvFAdd(spvFAdd(spvFSub(_591, spvFSub(_621, _622)), spvFSub(_2691, _622)), _627);
                                                    float _634 = spvFAdd(_621, _633);
                                                    float _637 = spvFAdd(spvFSub(_633, spvFSub(_634, _621)), spvFAdd(spvFSub(_593, spvFSub(_627, _628)), spvFSub(_2692, _628)));
                                                    float _638 = spvFAdd(_634, _637);
                                                    float _640 = spvFSub(_637, spvFSub(_638, _634));
                                                    float2 _641 = -_2146;
                                                    float _2693 = _641.x;
                                                    float _2694 = _641.y;
                                                    float _642 = spvFAdd(_638, _2693);
                                                    float _643 = spvFSub(_642, _638);
                                                    float _648 = spvFAdd(_640, _2694);
                                                    float _649 = spvFSub(_648, _640);
                                                    float _654 = spvFAdd(spvFAdd(spvFSub(_638, spvFSub(_642, _643)), spvFSub(_2693, _643)), _648);
                                                    float _655 = spvFAdd(_642, _654);
                                                    float _658 = spvFAdd(spvFSub(_654, spvFSub(_655, _642)), spvFAdd(spvFSub(_640, spvFSub(_648, _649)), spvFSub(_2694, _649)));
                                                    float _659 = spvFAdd(_655, _658);
                                                    float2 _2695 = float2(_659, spvFSub(_658, spvFSub(_659, _655)));
                                                    uint _117;
                                                    uint _2698;
                                                    uint _2697 = 0u;
                                                    uint _2700 = 0u;
                                                    for (;;)
                                                    {
                                                        if (_2700 < _2164)
                                                        {
                                                            _117 = _2700 + 1u;
                                                            uint _118 = _117 % _2164;
                                                            float _662 = spvFMul(_2659.x, _1809[_2700].x.x);
                                                            float _663 = spvFMul(_2659.x, 4097.0);
                                                            float _665 = spvFSub(_663, spvFSub(_663, _2659.x));
                                                            float _666 = spvFSub(_2659.x, _665);
                                                            float _667 = spvFMul(_1809[_2700].x.x, 4097.0);
                                                            float _669 = spvFSub(_667, spvFSub(_667, _1809[_2700].x.x));
                                                            float _670 = spvFSub(_1809[_2700].x.x, _669);
                                                            float _684 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_665, _669), _662), spvFMul(_665, _670)), spvFMul(_666, _669)), spvFMul(_666, _670)), spvFMul(_2659.x, _1809[_2700].x.y)), spvFMul(_2659.y, _1809[_2700].x.x)), spvFMul(_2659.y, _1809[_2700].x.y));
                                                            float _685 = spvFAdd(_662, _684);
                                                            float _687 = spvFSub(_684, spvFSub(_685, _662));
                                                            float _688 = spvFMul(_2672.x, _1809[_2700].y.x);
                                                            float _689 = spvFMul(_2672.x, 4097.0);
                                                            float _691 = spvFSub(_689, spvFSub(_689, _2672.x));
                                                            float _692 = spvFSub(_2672.x, _691);
                                                            float _693 = spvFMul(_1809[_2700].y.x, 4097.0);
                                                            float _695 = spvFSub(_693, spvFSub(_693, _1809[_2700].y.x));
                                                            float _696 = spvFSub(_1809[_2700].y.x, _695);
                                                            float _710 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_691, _695), _688), spvFMul(_691, _696)), spvFMul(_692, _695)), spvFMul(_692, _696)), spvFMul(_2672.x, _1809[_2700].y.y)), spvFMul(_2672.y, _1809[_2700].y.x)), spvFMul(_2672.y, _1809[_2700].y.y));
                                                            float _711 = spvFAdd(_688, _710);
                                                            float _713 = spvFSub(_710, spvFSub(_711, _688));
                                                            float _714 = spvFAdd(_685, _711);
                                                            float _715 = spvFSub(_714, _685);
                                                            float _720 = spvFAdd(_687, _713);
                                                            float _721 = spvFSub(_720, _687);
                                                            float _726 = spvFAdd(spvFAdd(spvFSub(_685, spvFSub(_714, _715)), spvFSub(_711, _715)), _720);
                                                            float _727 = spvFAdd(_714, _726);
                                                            float _730 = spvFAdd(spvFSub(_726, spvFSub(_727, _714)), spvFAdd(spvFSub(_687, spvFSub(_720, _721)), spvFSub(_713, _721)));
                                                            float _731 = spvFAdd(_727, _730);
                                                            float _733 = spvFSub(_730, spvFSub(_731, _727));
                                                            float2 _734 = -_2695;
                                                            float _2720 = _734.x;
                                                            float _2721 = _734.y;
                                                            float _735 = spvFAdd(_731, _2720);
                                                            float _736 = spvFSub(_735, _731);
                                                            float _741 = spvFAdd(_733, _2721);
                                                            float _742 = spvFSub(_741, _733);
                                                            float _747 = spvFAdd(spvFAdd(spvFSub(_731, spvFSub(_735, _736)), spvFSub(_2720, _736)), _741);
                                                            float _748 = spvFAdd(_735, _747);
                                                            float _751 = spvFAdd(spvFSub(_747, spvFSub(_748, _735)), spvFAdd(spvFSub(_733, spvFSub(_741, _742)), spvFSub(_2721, _742)));
                                                            float _752 = spvFAdd(_748, _751);
                                                            float _754 = spvFSub(_751, spvFSub(_752, _748));
                                                            float2 _2722 = float2(_752, _754);
                                                            float _755 = spvFMul(_2659.x, _1809[_118].x.x);
                                                            float _756 = spvFMul(_1809[_118].x.x, 4097.0);
                                                            float _758 = spvFSub(_756, spvFSub(_756, _1809[_118].x.x));
                                                            float _759 = spvFSub(_1809[_118].x.x, _758);
                                                            float _773 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_665, _758), _755), spvFMul(_665, _759)), spvFMul(_666, _758)), spvFMul(_666, _759)), spvFMul(_2659.x, _1809[_118].x.y)), spvFMul(_2659.y, _1809[_118].x.x)), spvFMul(_2659.y, _1809[_118].x.y));
                                                            float _774 = spvFAdd(_755, _773);
                                                            float _776 = spvFSub(_773, spvFSub(_774, _755));
                                                            float _777 = spvFMul(_2672.x, _1809[_118].y.x);
                                                            float _778 = spvFMul(_1809[_118].y.x, 4097.0);
                                                            float _780 = spvFSub(_778, spvFSub(_778, _1809[_118].y.x));
                                                            float _781 = spvFSub(_1809[_118].y.x, _780);
                                                            float _795 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_691, _780), _777), spvFMul(_691, _781)), spvFMul(_692, _780)), spvFMul(_692, _781)), spvFMul(_2672.x, _1809[_118].y.y)), spvFMul(_2672.y, _1809[_118].y.x)), spvFMul(_2672.y, _1809[_118].y.y));
                                                            float _796 = spvFAdd(_777, _795);
                                                            float _798 = spvFSub(_795, spvFSub(_796, _777));
                                                            float _799 = spvFAdd(_774, _796);
                                                            float _800 = spvFSub(_799, _774);
                                                            float _805 = spvFAdd(_776, _798);
                                                            float _806 = spvFSub(_805, _776);
                                                            float _811 = spvFAdd(spvFAdd(spvFSub(_774, spvFSub(_799, _800)), spvFSub(_796, _800)), _805);
                                                            float _812 = spvFAdd(_799, _811);
                                                            float _815 = spvFAdd(spvFSub(_811, spvFSub(_812, _799)), spvFAdd(spvFSub(_776, spvFSub(_805, _806)), spvFSub(_798, _806)));
                                                            float _816 = spvFAdd(_812, _815);
                                                            float _818 = spvFSub(_815, spvFSub(_816, _812));
                                                            float _819 = spvFAdd(_816, _2720);
                                                            float _820 = spvFSub(_819, _816);
                                                            float _825 = spvFAdd(_818, _2721);
                                                            float _826 = spvFSub(_825, _818);
                                                            float _831 = spvFAdd(spvFAdd(spvFSub(_816, spvFSub(_819, _820)), spvFSub(_2720, _820)), _825);
                                                            float _832 = spvFAdd(_819, _831);
                                                            float _835 = spvFAdd(spvFSub(_831, spvFSub(_832, _819)), spvFAdd(spvFSub(_818, spvFSub(_825, _826)), spvFSub(_2721, _826)));
                                                            float _836 = spvFAdd(_832, _835);
                                                            float _838 = spvFSub(_835, spvFSub(_836, _832));
                                                            float2 _2727 = float2(_836, _838);
                                                            bool2 _2728 = isnan(_2722);
                                                            bool2 _2729 = isinf(_2722);
                                                            bool _2741;
                                                            if (all(not(bool2(_2728.x || _2729.x, _2728.y || _2729.y))))
                                                            {
                                                                bool2 _2735 = isnan(_2727);
                                                                bool2 _2736 = isinf(_2727);
                                                                _2741 = !all(not(bool2(_2735.x || _2736.x, _2735.y || _2736.y)));
                                                            }
                                                            else
                                                            {
                                                                _2741 = true;
                                                            }
                                                            if (_2741)
                                                            {
                                                                _2607 = -2;
                                                                _2604 = _2697;
                                                                _2602 = true;
                                                                break;
                                                            }
                                                            bool _2752;
                                                            if ((isunordered(_752, 0.0) || _752 >= 0.0))
                                                            {
                                                                bool _2751;
                                                                if (_752 == 0.0)
                                                                {
                                                                    _2751 = _754 < 0.0;
                                                                }
                                                                else
                                                                {
                                                                    _2751 = false;
                                                                }
                                                                _2752 = _2751;
                                                            }
                                                            else
                                                            {
                                                                _2752 = true;
                                                            }
                                                            bool _2753 = !_2752;
                                                            bool _2762;
                                                            if ((isunordered(_836, 0.0) || _836 >= 0.0))
                                                            {
                                                                bool _2761;
                                                                if (_836 == 0.0)
                                                                {
                                                                    _2761 = _838 < 0.0;
                                                                }
                                                                else
                                                                {
                                                                    _2761 = false;
                                                                }
                                                                _2762 = _2761;
                                                            }
                                                            else
                                                            {
                                                                _2762 = true;
                                                            }
                                                            uint _2770;
                                                            if (_2753)
                                                            {
                                                                if (_2697 == 16u)
                                                                {
                                                                    _2607 = -1;
                                                                    _2604 = _2697;
                                                                    _2602 = true;
                                                                    break;
                                                                }
                                                                _1810[_2697] = _1809[_2700];
                                                                _2770 = _2697 + 1u;
                                                            }
                                                            else
                                                            {
                                                                _2770 = _2697;
                                                            }
                                                            if (_2753 != (!_2762))
                                                            {
                                                                float2 _839 = -_2727;
                                                                float _2774 = _839.x;
                                                                float _2775 = _839.y;
                                                                float _840 = spvFAdd(_752, _2774);
                                                                float _841 = spvFSub(_840, _752);
                                                                float _846 = spvFAdd(_754, _2775);
                                                                float _847 = spvFSub(_846, _754);
                                                                float _852 = spvFAdd(spvFAdd(spvFSub(_752, spvFSub(_840, _841)), spvFSub(_2774, _841)), _846);
                                                                float _853 = spvFAdd(_840, _852);
                                                                float _856 = spvFAdd(spvFSub(_852, spvFSub(_853, _840)), spvFAdd(spvFSub(_754, spvFSub(_846, _847)), spvFSub(_2775, _847)));
                                                                float _857 = spvFAdd(_853, _856);
                                                                float _859 = spvFSub(_856, spvFSub(_857, _853));
                                                                float _860 = _752 / _857;
                                                                float _868 = spvFMul(_860, _857);
                                                                float _869 = spvFMul(_860, 4097.0);
                                                                float _871 = spvFSub(_869, spvFSub(_869, _860));
                                                                float _872 = spvFSub(_860, _871);
                                                                float _873 = spvFMul(_857, 4097.0);
                                                                float _875 = spvFSub(_873, spvFSub(_873, _857));
                                                                float _876 = spvFSub(_857, _875);
                                                                float _890 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_871, _875), _868), spvFMul(_871, _876)), spvFMul(_872, _875)), spvFMul(_872, _876)), spvFMul(_860, _859)), spvFMul(0.0, _857)), spvFMul(0.0, _859));
                                                                float _891 = spvFAdd(_868, _890);
                                                                float _861 = -_891;
                                                                float _862 = -spvFSub(_890, spvFSub(_891, _868));
                                                                float _894 = spvFAdd(_752, _861);
                                                                float _895 = spvFSub(_894, _752);
                                                                float _900 = spvFAdd(_754, _862);
                                                                float _901 = spvFSub(_900, _754);
                                                                float _906 = spvFAdd(spvFAdd(spvFSub(_752, spvFSub(_894, _895)), spvFSub(_861, _895)), _900);
                                                                float _907 = spvFAdd(_894, _906);
                                                                float _863 = spvFAdd(_907, spvFAdd(spvFSub(_906, spvFSub(_907, _894)), spvFAdd(spvFSub(_754, spvFSub(_900, _901)), spvFSub(_862, _901)))) / _857;
                                                                float _912 = spvFAdd(_860, _863);
                                                                float _913 = spvFSub(_912, _860);
                                                                float _918 = spvFAdd(spvFAdd(spvFSub(_860, spvFSub(_912, _913)), spvFSub(_863, _913)), _303);
                                                                float _919 = spvFAdd(_912, _918);
                                                                float _922 = spvFAdd(spvFSub(_918, spvFSub(_919, _912)), _308);
                                                                float _923 = spvFAdd(_919, _922);
                                                                float _925 = spvFSub(_922, spvFSub(_923, _919));
                                                                float _926 = spvFMul(_923, _857);
                                                                float _927 = spvFMul(_923, 4097.0);
                                                                float _929 = spvFSub(_927, spvFSub(_927, _923));
                                                                float _930 = spvFSub(_923, _929);
                                                                float _944 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_929, _875), _926), spvFMul(_929, _876)), spvFMul(_930, _875)), spvFMul(_930, _876)), spvFMul(_923, _859)), spvFMul(_925, _857)), spvFMul(_925, _859));
                                                                float _945 = spvFAdd(_926, _944);
                                                                float _864 = -_945;
                                                                float _865 = -spvFSub(_944, spvFSub(_945, _926));
                                                                float _948 = spvFAdd(_752, _864);
                                                                float _949 = spvFSub(_948, _752);
                                                                float _954 = spvFAdd(_754, _865);
                                                                float _955 = spvFSub(_954, _754);
                                                                float _960 = spvFAdd(spvFAdd(spvFSub(_752, spvFSub(_948, _949)), spvFSub(_864, _949)), _954);
                                                                float _961 = spvFAdd(_948, _960);
                                                                float _964 = spvFAdd(spvFSub(_960, spvFSub(_961, _948)), spvFAdd(spvFSub(_754, spvFSub(_954, _955)), spvFSub(_865, _955)));
                                                                float _965 = spvFAdd(_961, _964);
                                                                float _867 = spvFAdd(_965, spvFSub(_964, spvFSub(_965, _961))) / _857;
                                                                float _968 = spvFAdd(_923, _867);
                                                                float _969 = spvFSub(_968, _923);
                                                                float _974 = spvFAdd(_925, 0.0);
                                                                float _975 = spvFSub(_974, _925);
                                                                float _980 = spvFAdd(spvFAdd(spvFSub(_923, spvFSub(_968, _969)), spvFSub(_867, _969)), _974);
                                                                float _981 = spvFAdd(_968, _980);
                                                                float _984 = spvFAdd(spvFSub(_980, spvFSub(_981, _968)), spvFAdd(spvFSub(_925, spvFSub(_974, _975)), spvFSub(0.0, _975)));
                                                                float _985 = spvFAdd(_981, _984);
                                                                float _987 = spvFSub(_984, spvFSub(_985, _981));
                                                                float2 _2776 = float2(_985, _987);
                                                                bool2 _2777 = isnan(_2776);
                                                                bool2 _2778 = isinf(_2776);
                                                                if (!all(not(bool2(_2777.x || _2778.x, _2777.y || _2778.y))))
                                                                {
                                                                    _2607 = -3;
                                                                    _2604 = _2770;
                                                                    _2602 = true;
                                                                    break;
                                                                }
                                                                bool _2793;
                                                                if ((isunordered(_985, (-1.0000000133514319600180897396058e-10)) || _985 >= (-1.0000000133514319600180897396058e-10)))
                                                                {
                                                                    bool _2792;
                                                                    if (_985 == (-1.0000000133514319600180897396058e-10))
                                                                    {
                                                                        _2792 = _987 < 0.0;
                                                                    }
                                                                    else
                                                                    {
                                                                        _2792 = false;
                                                                    }
                                                                    _2793 = _2792;
                                                                }
                                                                else
                                                                {
                                                                    _2793 = true;
                                                                }
                                                                if (_2793)
                                                                {
                                                                    _2607 = -4;
                                                                    _2604 = _2770;
                                                                    _2602 = true;
                                                                    break;
                                                                }
                                                                bool _2796 = (isunordered(1.0, _985) || 1.0 >= _985);
                                                                bool _2804;
                                                                if (_2796)
                                                                {
                                                                    bool _2803;
                                                                    if (1.0 == _985)
                                                                    {
                                                                        _2803 = 1.0000000133514319600180897396058e-10 < _987;
                                                                    }
                                                                    else
                                                                    {
                                                                        _2803 = false;
                                                                    }
                                                                    _2804 = _2803;
                                                                }
                                                                else
                                                                {
                                                                    _2804 = true;
                                                                }
                                                                if (_2804)
                                                                {
                                                                    _2607 = -5;
                                                                    _2604 = _2770;
                                                                    _2602 = true;
                                                                    break;
                                                                }
                                                                bool _2814;
                                                                if (_2796)
                                                                {
                                                                    bool _2813;
                                                                    if (1.0 == _985)
                                                                    {
                                                                        _2813 = 0.0 < _987;
                                                                    }
                                                                    else
                                                                    {
                                                                        _2813 = false;
                                                                    }
                                                                    _2814 = _2813;
                                                                }
                                                                else
                                                                {
                                                                    _2814 = true;
                                                                }
                                                                float2 _2816 = select(_2776, float2(1.0, 0.0), bool2(_2814));
                                                                float _2817 = _2816.x;
                                                                bool _2827;
                                                                if ((isunordered(0.0, _2817) || 0.0 >= _2817))
                                                                {
                                                                    bool _2826;
                                                                    if (0.0 == _2817)
                                                                    {
                                                                        _2826 = 0.0 < _2816.y;
                                                                    }
                                                                    else
                                                                    {
                                                                        _2826 = false;
                                                                    }
                                                                    _2827 = _2826;
                                                                }
                                                                else
                                                                {
                                                                    _2827 = true;
                                                                }
                                                                float2 _2829 = select(float2(0.0), _2816, bool2(_2827));
                                                                if (_2770 == 16u)
                                                                {
                                                                    _2607 = -1;
                                                                    _2604 = _2770;
                                                                    _2602 = true;
                                                                    break;
                                                                }
                                                                float2 _988 = -_1809[_2700].x;
                                                                float _2833 = _988.x;
                                                                float _2834 = _988.y;
                                                                float _989 = spvFAdd(_1809[_118].x.x, _2833);
                                                                float _990 = spvFSub(_989, _1809[_118].x.x);
                                                                float _995 = spvFAdd(_1809[_118].x.y, _2834);
                                                                float _996 = spvFSub(_995, _1809[_118].x.y);
                                                                float _1001 = spvFAdd(spvFAdd(spvFSub(_1809[_118].x.x, spvFSub(_989, _990)), spvFSub(_2833, _990)), _995);
                                                                float _1002 = spvFAdd(_989, _1001);
                                                                float _1005 = spvFAdd(spvFSub(_1001, spvFSub(_1002, _989)), spvFAdd(spvFSub(_1809[_118].x.y, spvFSub(_995, _996)), spvFSub(_2834, _996)));
                                                                float _1006 = spvFAdd(_1002, _1005);
                                                                float _1008 = spvFSub(_1005, spvFSub(_1006, _1002));
                                                                float _2835 = _2829.x;
                                                                float _2836 = _2829.y;
                                                                float _1009 = spvFMul(_2835, _1006);
                                                                float _1010 = spvFMul(_2835, 4097.0);
                                                                float _1012 = spvFSub(_1010, spvFSub(_1010, _2835));
                                                                float _1013 = spvFSub(_2835, _1012);
                                                                float _1014 = spvFMul(_1006, 4097.0);
                                                                float _1016 = spvFSub(_1014, spvFSub(_1014, _1006));
                                                                float _1017 = spvFSub(_1006, _1016);
                                                                float _1031 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1012, _1016), _1009), spvFMul(_1012, _1017)), spvFMul(_1013, _1016)), spvFMul(_1013, _1017)), spvFMul(_2835, _1008)), spvFMul(_2836, _1006)), spvFMul(_2836, _1008));
                                                                float _1032 = spvFAdd(_1009, _1031);
                                                                float _1034 = spvFSub(_1031, spvFSub(_1032, _1009));
                                                                float _1035 = spvFAdd(_1809[_2700].x.x, _1032);
                                                                float _1036 = spvFSub(_1035, _1809[_2700].x.x);
                                                                float _1041 = spvFAdd(_1809[_2700].x.y, _1034);
                                                                float _1042 = spvFSub(_1041, _1809[_2700].x.y);
                                                                float _1047 = spvFAdd(spvFAdd(spvFSub(_1809[_2700].x.x, spvFSub(_1035, _1036)), spvFSub(_1032, _1036)), _1041);
                                                                float _1048 = spvFAdd(_1035, _1047);
                                                                float _1051 = spvFAdd(spvFSub(_1047, spvFSub(_1048, _1035)), spvFAdd(spvFSub(_1809[_2700].x.y, spvFSub(_1041, _1042)), spvFSub(_1034, _1042)));
                                                                float _1052 = spvFAdd(_1048, _1051);
                                                                float2 _1055 = -_1809[_2700].y;
                                                                float _2838 = _1055.x;
                                                                float _2839 = _1055.y;
                                                                float _1056 = spvFAdd(_1809[_118].y.x, _2838);
                                                                float _1057 = spvFSub(_1056, _1809[_118].y.x);
                                                                float _1062 = spvFAdd(_1809[_118].y.y, _2839);
                                                                float _1063 = spvFSub(_1062, _1809[_118].y.y);
                                                                float _1068 = spvFAdd(spvFAdd(spvFSub(_1809[_118].y.x, spvFSub(_1056, _1057)), spvFSub(_2838, _1057)), _1062);
                                                                float _1069 = spvFAdd(_1056, _1068);
                                                                float _1072 = spvFAdd(spvFSub(_1068, spvFSub(_1069, _1056)), spvFAdd(spvFSub(_1809[_118].y.y, spvFSub(_1062, _1063)), spvFSub(_2839, _1063)));
                                                                float _1073 = spvFAdd(_1069, _1072);
                                                                float _1075 = spvFSub(_1072, spvFSub(_1073, _1069));
                                                                float _1076 = spvFMul(_2835, _1073);
                                                                float _1077 = spvFMul(_1073, 4097.0);
                                                                float _1079 = spvFSub(_1077, spvFSub(_1077, _1073));
                                                                float _1080 = spvFSub(_1073, _1079);
                                                                float _1094 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1012, _1079), _1076), spvFMul(_1012, _1080)), spvFMul(_1013, _1079)), spvFMul(_1013, _1080)), spvFMul(_2835, _1075)), spvFMul(_2836, _1073)), spvFMul(_2836, _1075));
                                                                float _1095 = spvFAdd(_1076, _1094);
                                                                float _1097 = spvFSub(_1094, spvFSub(_1095, _1076));
                                                                float _1098 = spvFAdd(_1809[_2700].y.x, _1095);
                                                                float _1099 = spvFSub(_1098, _1809[_2700].y.x);
                                                                float _1104 = spvFAdd(_1809[_2700].y.y, _1097);
                                                                float _1105 = spvFSub(_1104, _1809[_2700].y.y);
                                                                float _1110 = spvFAdd(spvFAdd(spvFSub(_1809[_2700].y.x, spvFSub(_1098, _1099)), spvFSub(_1095, _1099)), _1104);
                                                                float _1111 = spvFAdd(_1098, _1110);
                                                                float _1114 = spvFAdd(spvFSub(_1110, spvFSub(_1111, _1098)), spvFAdd(spvFSub(_1809[_2700].y.y, spvFSub(_1104, _1105)), spvFSub(_1097, _1105)));
                                                                float _1115 = spvFAdd(_1111, _1114);
                                                                _1810[_2770] = Point{ float2(_1052, spvFSub(_1051, spvFSub(_1052, _1048))), float2(_1115, spvFSub(_1114, spvFSub(_1115, _1111))) };
                                                                _2698 = _2770 + 1u;
                                                            }
                                                            else
                                                            {
                                                                _2698 = _2770;
                                                            }
                                                            _2697 = _2698;
                                                            _2700 = _117;
                                                            continue;
                                                        }
                                                        else
                                                        {
                                                            _2607 = _2606;
                                                            _2604 = _2697;
                                                            _2602 = _2601;
                                                            break;
                                                        }
                                                    }
                                                    if (_2602)
                                                    {
                                                        _2167 = _2607;
                                                        _2161 = _2602;
                                                        break;
                                                    }
                                                    if (_2604 == 0u)
                                                    {
                                                        _2167 = 0;
                                                        _2161 = true;
                                                        break;
                                                    }
                                                    for (uint _2848 = 0u; _2848 < _2604; )
                                                    {
                                                        _1809[_2848] = _1810[_2848];
                                                        _2848++;
                                                        continue;
                                                    }
                                                    _2601 = _2602;
                                                    _2164 = _2604;
                                                    _2605++;
                                                    _2606 = _2607;
                                                    continue;
                                                }
                                                else
                                                {
                                                    _2167 = _2606;
                                                    _2161 = _2601;
                                                    break;
                                                }
                                            }
                                            if (_2161)
                                            {
                                                _2155 = _2167;
                                                _2152 = _2164;
                                                _2149 = _2161;
                                                break;
                                            }
                                            _2160 = _2161;
                                            _2163 = _2164;
                                            _2165++;
                                            _2166 = _2167;
                                            continue;
                                        }
                                        else
                                        {
                                            _2155 = _2166;
                                            _2152 = _2163;
                                            _2149 = _2160;
                                            break;
                                        }
                                    }
                                    if (_2149)
                                    {
                                        _2857 = _2155;
                                        _2858 = _2152;
                                        _2859 = _2149;
                                        break;
                                    }
                                    _2148 = _2149;
                                    _2151 = _2152;
                                    _2153++;
                                    _2154 = _2155;
                                    continue;
                                }
                                else
                                {
                                    _2857 = _2154;
                                    _2858 = _2151;
                                    _2859 = _2148;
                                    break;
                                }
                            }
                            if (_2859)
                            {
                                _3086 = float2(0.0);
                                _3087 = float4(0.0);
                                _3088 = _2857;
                                break;
                            }
                            float _2861 = float(_2105);
                            float2 _2862 = float2(_2861, 0.0);
                            float _2863 = float(_2095);
                            float2 _2864 = float2(_2863, 0.0);
                            float2 _2866;
                            float2 _2869;
                            float2 _2871;
                            float2 _2873;
                            float2 _2875;
                            float2 _2877;
                            _2866 = float2(0.0);
                            _2869 = float2(0.0);
                            _2871 = float2(0.0);
                            _2873 = float2(0.0);
                            _2875 = _2864;
                            _2877 = _2862;
                            float2 _2867;
                            float2 _2870;
                            float2 _2872;
                            float2 _2874;
                            float2 _2876;
                            float2 _2878;
                            for (uint _2879 = 0u; _2879 < _2858; _2866 = _2867, _2869 = _2870, _2871 = _2872, _2873 = _2874, _2875 = _2876, _2877 = _2878, _2879++)
                            {
                                bool _2897;
                                if ((isunordered(_2877.x, _1809[_2879].x.x) || _2877.x >= _1809[_2879].x.x))
                                {
                                    bool _2896;
                                    if (_2877.x == _1809[_2879].x.x)
                                    {
                                        _2896 = _2877.y < _1809[_2879].x.y;
                                    }
                                    else
                                    {
                                        _2896 = false;
                                    }
                                    _2897 = _2896;
                                }
                                else
                                {
                                    _2897 = true;
                                }
                                _2878 = select(_1809[_2879].x, _2877, bool2(_2897));
                                bool _2913;
                                if ((isunordered(_2875.x, _1809[_2879].y.x) || _2875.x >= _1809[_2879].y.x))
                                {
                                    bool _2912;
                                    if (_2875.x == _1809[_2879].y.x)
                                    {
                                        _2912 = _2875.y < _1809[_2879].y.y;
                                    }
                                    else
                                    {
                                        _2912 = false;
                                    }
                                    _2913 = _2912;
                                }
                                else
                                {
                                    _2913 = true;
                                }
                                _2876 = select(_1809[_2879].y, _2875, bool2(_2913));
                                bool _2928;
                                if ((isunordered(_2873.x, _1809[_2879].x.x) || _2873.x >= _1809[_2879].x.x))
                                {
                                    bool _2927;
                                    if (_2873.x == _1809[_2879].x.x)
                                    {
                                        _2927 = _2873.y < _1809[_2879].x.y;
                                    }
                                    else
                                    {
                                        _2927 = false;
                                    }
                                    _2928 = _2927;
                                }
                                else
                                {
                                    _2928 = true;
                                }
                                _2874 = select(_2873, _1809[_2879].x, bool2(_2928));
                                bool _2943;
                                if ((isunordered(_2871.x, _1809[_2879].y.x) || _2871.x >= _1809[_2879].y.x))
                                {
                                    bool _2942;
                                    if (_2871.x == _1809[_2879].y.x)
                                    {
                                        _2942 = _2871.y < _1809[_2879].y.y;
                                    }
                                    else
                                    {
                                        _2942 = false;
                                    }
                                    _2943 = _2942;
                                }
                                else
                                {
                                    _2943 = true;
                                }
                                _2872 = select(_2871, _1809[_2879].y, bool2(_2943));
                                float _1118 = spvFAdd(_2869.x, _1809[_2879].x.x);
                                float _1119 = spvFSub(_1118, _2869.x);
                                float _1124 = spvFAdd(_2869.y, _1809[_2879].x.y);
                                float _1125 = spvFSub(_1124, _2869.y);
                                float _1130 = spvFAdd(spvFAdd(spvFSub(_2869.x, spvFSub(_1118, _1119)), spvFSub(_1809[_2879].x.x, _1119)), _1124);
                                float _1131 = spvFAdd(_1118, _1130);
                                float _1134 = spvFAdd(spvFSub(_1130, spvFSub(_1131, _1118)), spvFAdd(spvFSub(_2869.y, spvFSub(_1124, _1125)), spvFSub(_1809[_2879].x.y, _1125)));
                                float _1135 = spvFAdd(_1131, _1134);
                                _2870 = float2(_1135, spvFSub(_1134, spvFSub(_1135, _1131)));
                                float _1138 = spvFAdd(_2866.x, _1809[_2879].y.x);
                                float _1139 = spvFSub(_1138, _2866.x);
                                float _1144 = spvFAdd(_2866.y, _1809[_2879].y.y);
                                float _1145 = spvFSub(_1144, _2866.y);
                                float _1150 = spvFAdd(spvFAdd(spvFSub(_2866.x, spvFSub(_1138, _1139)), spvFSub(_1809[_2879].y.x, _1139)), _1144);
                                float _1151 = spvFAdd(_1138, _1150);
                                float _1154 = spvFAdd(spvFSub(_1150, spvFSub(_1151, _1138)), spvFAdd(spvFSub(_2866.y, spvFSub(_1144, _1145)), spvFSub(_1809[_2879].y.y, _1145)));
                                float _1155 = spvFAdd(_1151, _1154);
                                _2867 = float2(_1155, spvFSub(_1154, spvFSub(_1155, _1151)));
                            }
                            float2 _1158 = -float2(1.0000000133514319600180897396058e-10, 0.0);
                            float _2957 = _1158.x;
                            float _2958 = _1158.y;
                            float _1159 = spvFAdd(_2877.x, _2957);
                            float _1160 = spvFSub(_1159, _2877.x);
                            float _1165 = spvFAdd(_2877.y, _2958);
                            float _1166 = spvFSub(_1165, _2877.y);
                            float _1171 = spvFAdd(spvFAdd(spvFSub(_2877.x, spvFSub(_1159, _1160)), spvFSub(_2957, _1160)), _1165);
                            float _1172 = spvFAdd(_1159, _1171);
                            float _1175 = spvFAdd(spvFSub(_1171, spvFSub(_1172, _1159)), spvFAdd(spvFSub(_2877.y, spvFSub(_1165, _1166)), spvFSub(_2958, _1166)));
                            float _1176 = spvFAdd(_1172, _1175);
                            float _1178 = spvFSub(_1175, spvFSub(_1176, _1172));
                            bool _2968;
                            if ((isunordered(0.0, _1176) || 0.0 >= _1176))
                            {
                                bool _2967;
                                if (0.0 == _1176)
                                {
                                    _2967 = 0.0 < _1178;
                                }
                                else
                                {
                                    _2967 = false;
                                }
                                _2968 = _2967;
                            }
                            else
                            {
                                _2968 = true;
                            }
                            float2 _2970 = select(float2(0.0), float2(_1176, _1178), bool2(_2968));
                            float _1179 = spvFAdd(_2875.x, _2957);
                            float _1180 = spvFSub(_1179, _2875.x);
                            float _1185 = spvFAdd(_2875.y, _2958);
                            float _1186 = spvFSub(_1185, _2875.y);
                            float _1191 = spvFAdd(spvFAdd(spvFSub(_2875.x, spvFSub(_1179, _1180)), spvFSub(_2957, _1180)), _1185);
                            float _1192 = spvFAdd(_1179, _1191);
                            float _1195 = spvFAdd(spvFSub(_1191, spvFSub(_1192, _1179)), spvFAdd(spvFSub(_2875.y, spvFSub(_1185, _1186)), spvFSub(_2958, _1186)));
                            float _1196 = spvFAdd(_1192, _1195);
                            float _1198 = spvFSub(_1195, spvFSub(_1196, _1192));
                            bool _2982;
                            if ((isunordered(0.0, _1196) || 0.0 >= _1196))
                            {
                                bool _2981;
                                if (0.0 == _1196)
                                {
                                    _2981 = 0.0 < _1198;
                                }
                                else
                                {
                                    _2981 = false;
                                }
                                _2982 = _2981;
                            }
                            else
                            {
                                _2982 = true;
                            }
                            float2 _2984 = select(float2(0.0), float2(_1196, _1198), bool2(_2982));
                            float _1199 = spvFAdd(_2873.x, 1.0000000133514319600180897396058e-10);
                            float _1200 = spvFSub(_1199, _2873.x);
                            float _1205 = spvFAdd(_2873.y, 0.0);
                            float _1206 = spvFSub(_1205, _2873.y);
                            float _1211 = spvFAdd(spvFAdd(spvFSub(_2873.x, spvFSub(_1199, _1200)), spvFSub(1.0000000133514319600180897396058e-10, _1200)), _1205);
                            float _1212 = spvFAdd(_1199, _1211);
                            float _1215 = spvFAdd(spvFSub(_1211, spvFSub(_1212, _1199)), spvFAdd(spvFSub(_2873.y, spvFSub(_1205, _1206)), spvFSub(0.0, _1206)));
                            float _1216 = spvFAdd(_1212, _1215);
                            float _1218 = spvFSub(_1215, spvFSub(_1216, _1212));
                            bool _2996;
                            if ((isunordered(_2861, _1216) || _2861 >= _1216))
                            {
                                bool _2995;
                                if (_2861 == _1216)
                                {
                                    _2995 = 0.0 < _1218;
                                }
                                else
                                {
                                    _2995 = false;
                                }
                                _2996 = _2995;
                            }
                            else
                            {
                                _2996 = true;
                            }
                            float2 _2998 = select(float2(_1216, _1218), _2862, bool2(_2996));
                            float _1219 = spvFAdd(_2871.x, 1.0000000133514319600180897396058e-10);
                            float _1220 = spvFSub(_1219, _2871.x);
                            float _1225 = spvFAdd(_2871.y, 0.0);
                            float _1226 = spvFSub(_1225, _2871.y);
                            float _1231 = spvFAdd(spvFAdd(spvFSub(_2871.x, spvFSub(_1219, _1220)), spvFSub(1.0000000133514319600180897396058e-10, _1220)), _1225);
                            float _1232 = spvFAdd(_1219, _1231);
                            float _1235 = spvFAdd(spvFSub(_1231, spvFSub(_1232, _1219)), spvFAdd(spvFSub(_2871.y, spvFSub(_1225, _1226)), spvFSub(0.0, _1226)));
                            float _1236 = spvFAdd(_1232, _1235);
                            float _1238 = spvFSub(_1235, spvFSub(_1236, _1232));
                            bool _3010;
                            if ((isunordered(_2863, _1236) || _2863 >= _1236))
                            {
                                bool _3009;
                                if (_2863 == _1236)
                                {
                                    _3009 = 0.0 < _1238;
                                }
                                else
                                {
                                    _3009 = false;
                                }
                                _3010 = _3009;
                            }
                            else
                            {
                                _3010 = true;
                            }
                            float2 _3012 = select(float2(_1236, _1238), _2864, bool2(_3010));
                            float _3014 = float(_44.x);
                            float _3015 = _2970.x;
                            float _3016 = _2970.y;
                            float _1239 = spvFAdd(_3014, _3015);
                            float _1240 = spvFSub(_1239, _3014);
                            float _1245 = spvFAdd(0.0, _3016);
                            float _1246 = spvFSub(_1245, 0.0);
                            float _1251 = spvFAdd(spvFAdd(spvFSub(_3014, spvFSub(_1239, _1240)), spvFSub(_3015, _1240)), _1245);
                            float _1252 = spvFAdd(_1239, _1251);
                            float _1255 = spvFAdd(spvFSub(_1251, spvFSub(_1252, _1239)), spvFAdd(spvFSub(0.0, spvFSub(_1245, _1246)), spvFSub(_3016, _1246)));
                            float _1256 = spvFAdd(_1252, _1255);
                            float _3029;
                            do
                            {
                                if (_1256 <= 0.0)
                                {
                                    _3029 = 0.0;
                                    break;
                                }
                                float _3028;
                                if (spvFSub(_1255, spvFSub(_1256, _1252)) < 0.0)
                                {
                                    _3028 = as_type<float>(as_type<uint>(_1256) - 1u);
                                }
                                else
                                {
                                    _3028 = _1256;
                                }
                                _3029 = _3028;
                                break;
                            } while(false);
                            float _3031 = float(_44.y);
                            float _3032 = _2984.x;
                            float _3033 = _2984.y;
                            float _1260 = spvFAdd(_3031, _3032);
                            float _1261 = spvFSub(_1260, _3031);
                            float _1266 = spvFAdd(0.0, _3033);
                            float _1267 = spvFSub(_1266, 0.0);
                            float _1272 = spvFAdd(spvFAdd(spvFSub(_3031, spvFSub(_1260, _1261)), spvFSub(_3032, _1261)), _1266);
                            float _1273 = spvFAdd(_1260, _1272);
                            float _1276 = spvFAdd(spvFSub(_1272, spvFSub(_1273, _1260)), spvFAdd(spvFSub(0.0, spvFSub(_1266, _1267)), spvFSub(_3033, _1267)));
                            float _1277 = spvFAdd(_1273, _1276);
                            float _3046;
                            do
                            {
                                if (_1277 <= 0.0)
                                {
                                    _3046 = 0.0;
                                    break;
                                }
                                float _3045;
                                if (spvFSub(_1276, spvFSub(_1277, _1273)) < 0.0)
                                {
                                    _3045 = as_type<float>(as_type<uint>(_1277) - 1u);
                                }
                                else
                                {
                                    _3045 = _1277;
                                }
                                _3046 = _3045;
                                break;
                            } while(false);
                            float _3047 = _2998.x;
                            float _3048 = _2998.y;
                            float _1281 = spvFAdd(_3014, _3047);
                            float _1282 = spvFSub(_1281, _3014);
                            float _1287 = spvFAdd(0.0, _3048);
                            float _1288 = spvFSub(_1287, 0.0);
                            float _1293 = spvFAdd(spvFAdd(spvFSub(_3014, spvFSub(_1281, _1282)), spvFSub(_3047, _1282)), _1287);
                            float _1294 = spvFAdd(_1281, _1293);
                            float _1297 = spvFAdd(spvFSub(_1293, spvFSub(_1294, _1281)), spvFAdd(spvFSub(0.0, spvFSub(_1287, _1288)), spvFSub(_3048, _1288)));
                            float _1298 = spvFAdd(_1294, _1297);
                            float _3055;
                            if (spvFSub(_1297, spvFSub(_1298, _1294)) > 0.0)
                            {
                                _3055 = as_type<float>(as_type<uint>(_1298) + 1u);
                            }
                            else
                            {
                                _3055 = _1298;
                            }
                            float _3056 = _3012.x;
                            float _3057 = _3012.y;
                            float _1302 = spvFAdd(_3031, _3056);
                            float _1303 = spvFSub(_1302, _3031);
                            float _1308 = spvFAdd(0.0, _3057);
                            float _1309 = spvFSub(_1308, 0.0);
                            float _1314 = spvFAdd(spvFAdd(spvFSub(_3031, spvFSub(_1302, _1303)), spvFSub(_3056, _1303)), _1308);
                            float _1315 = spvFAdd(_1302, _1314);
                            float _1318 = spvFAdd(spvFSub(_1314, spvFSub(_1315, _1302)), spvFAdd(spvFSub(0.0, spvFSub(_1308, _1309)), spvFSub(_3057, _1309)));
                            float _1319 = spvFAdd(_1315, _1318);
                            float _3064;
                            if (spvFSub(_1318, spvFSub(_1319, _1315)) > 0.0)
                            {
                                _3064 = as_type<float>(as_type<uint>(_1319) + 1u);
                            }
                            else
                            {
                                _3064 = _1319;
                            }
                            float4 _3065 = float4(_3029, _3046, _3055, _3064);
                            float _126 = spvFAdd(_3014, 0.5);
                            float _3066 = float(_2858);
                            float _1323 = _2869.x / _3066;
                            float _1331 = spvFMul(_1323, _3066);
                            float _1332 = spvFMul(_1323, 4097.0);
                            float _1334 = spvFSub(_1332, spvFSub(_1332, _1323));
                            float _1335 = spvFSub(_1323, _1334);
                            float _1336 = spvFMul(_3066, 4097.0);
                            float _1338 = spvFSub(_1336, spvFSub(_1336, _3066));
                            float _1339 = spvFSub(_3066, _1338);
                            float _1350 = spvFMul(0.0, _3066);
                            float _1352 = spvFMul(0.0, 0.0);
                            float _1353 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1334, _1338), _1331), spvFMul(_1334, _1339)), spvFMul(_1335, _1338)), spvFMul(_1335, _1339)), spvFMul(_1323, 0.0)), _1350), _1352);
                            float _1354 = spvFAdd(_1331, _1353);
                            float _1324 = -_1354;
                            float _1325 = -spvFSub(_1353, spvFSub(_1354, _1331));
                            float _1357 = spvFAdd(_2869.x, _1324);
                            float _1358 = spvFSub(_1357, _2869.x);
                            float _1363 = spvFAdd(_2869.y, _1325);
                            float _1364 = spvFSub(_1363, _2869.y);
                            float _1369 = spvFAdd(spvFAdd(spvFSub(_2869.x, spvFSub(_1357, _1358)), spvFSub(_1324, _1358)), _1363);
                            float _1370 = spvFAdd(_1357, _1369);
                            float _1326 = spvFAdd(_1370, spvFAdd(spvFSub(_1369, spvFSub(_1370, _1357)), spvFAdd(spvFSub(_2869.y, spvFSub(_1363, _1364)), spvFSub(_1325, _1364)))) / _3066;
                            float _1375 = spvFAdd(_1323, _1326);
                            float _1376 = spvFSub(_1375, _1323);
                            float _1381 = spvFAdd(0.0, 0.0);
                            float _1382 = spvFSub(_1381, 0.0);
                            float _1386 = spvFAdd(spvFSub(0.0, spvFSub(_1381, _1382)), spvFSub(0.0, _1382));
                            float _1387 = spvFAdd(spvFAdd(spvFSub(_1323, spvFSub(_1375, _1376)), spvFSub(_1326, _1376)), _1381);
                            float _1388 = spvFAdd(_1375, _1387);
                            float _1391 = spvFAdd(spvFSub(_1387, spvFSub(_1388, _1375)), _1386);
                            float _1392 = spvFAdd(_1388, _1391);
                            float _1394 = spvFSub(_1391, spvFSub(_1392, _1388));
                            float _1395 = spvFMul(_1392, _3066);
                            float _1396 = spvFMul(_1392, 4097.0);
                            float _1398 = spvFSub(_1396, spvFSub(_1396, _1392));
                            float _1399 = spvFSub(_1392, _1398);
                            float _1413 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1398, _1338), _1395), spvFMul(_1398, _1339)), spvFMul(_1399, _1338)), spvFMul(_1399, _1339)), spvFMul(_1392, 0.0)), spvFMul(_1394, _3066)), spvFMul(_1394, 0.0));
                            float _1414 = spvFAdd(_1395, _1413);
                            float _1327 = -_1414;
                            float _1328 = -spvFSub(_1413, spvFSub(_1414, _1395));
                            float _1417 = spvFAdd(_2869.x, _1327);
                            float _1418 = spvFSub(_1417, _2869.x);
                            float _1423 = spvFAdd(_2869.y, _1328);
                            float _1424 = spvFSub(_1423, _2869.y);
                            float _1429 = spvFAdd(spvFAdd(spvFSub(_2869.x, spvFSub(_1417, _1418)), spvFSub(_1327, _1418)), _1423);
                            float _1430 = spvFAdd(_1417, _1429);
                            float _1433 = spvFAdd(spvFSub(_1429, spvFSub(_1430, _1417)), spvFAdd(spvFSub(_2869.y, spvFSub(_1423, _1424)), spvFSub(_1328, _1424)));
                            float _1434 = spvFAdd(_1430, _1433);
                            float _1330 = spvFAdd(_1434, spvFSub(_1433, spvFSub(_1434, _1430))) / _3066;
                            float _1437 = spvFAdd(_1392, _1330);
                            float _1438 = spvFSub(_1437, _1392);
                            float _1443 = spvFAdd(_1394, 0.0);
                            float _1444 = spvFSub(_1443, _1394);
                            float _1449 = spvFAdd(spvFAdd(spvFSub(_1392, spvFSub(_1437, _1438)), spvFSub(_1330, _1438)), _1443);
                            float _1450 = spvFAdd(_1437, _1449);
                            float _1453 = spvFAdd(spvFSub(_1449, spvFSub(_1450, _1437)), spvFAdd(spvFSub(_1394, spvFSub(_1443, _1444)), spvFSub(0.0, _1444)));
                            float _1454 = spvFAdd(_1450, _1453);
                            float _1456 = spvFSub(_1453, spvFSub(_1454, _1450));
                            float _1457 = spvFAdd(_126, _1454);
                            float _1458 = spvFSub(_1457, _126);
                            float _1463 = spvFAdd(0.0, _1456);
                            float _1464 = spvFSub(_1463, 0.0);
                            float _1469 = spvFAdd(spvFAdd(spvFSub(_126, spvFSub(_1457, _1458)), spvFSub(_1454, _1458)), _1463);
                            float _1470 = spvFAdd(_1457, _1469);
                            float _1473 = spvFAdd(spvFSub(_1469, spvFSub(_1470, _1457)), spvFAdd(spvFSub(0.0, spvFSub(_1463, _1464)), spvFSub(_1456, _1464)));
                            float _1474 = spvFAdd(_1470, _1473);
                            float _127 = spvFAdd(_3031, 0.5);
                            float _1478 = _2866.x / _3066;
                            float _1486 = spvFMul(_1478, _3066);
                            float _1487 = spvFMul(_1478, 4097.0);
                            float _1489 = spvFSub(_1487, spvFSub(_1487, _1478));
                            float _1490 = spvFSub(_1478, _1489);
                            float _1502 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1489, _1338), _1486), spvFMul(_1489, _1339)), spvFMul(_1490, _1338)), spvFMul(_1490, _1339)), spvFMul(_1478, 0.0)), _1350), _1352);
                            float _1503 = spvFAdd(_1486, _1502);
                            float _1479 = -_1503;
                            float _1480 = -spvFSub(_1502, spvFSub(_1503, _1486));
                            float _1506 = spvFAdd(_2866.x, _1479);
                            float _1507 = spvFSub(_1506, _2866.x);
                            float _1512 = spvFAdd(_2866.y, _1480);
                            float _1513 = spvFSub(_1512, _2866.y);
                            float _1518 = spvFAdd(spvFAdd(spvFSub(_2866.x, spvFSub(_1506, _1507)), spvFSub(_1479, _1507)), _1512);
                            float _1519 = spvFAdd(_1506, _1518);
                            float _1481 = spvFAdd(_1519, spvFAdd(spvFSub(_1518, spvFSub(_1519, _1506)), spvFAdd(spvFSub(_2866.y, spvFSub(_1512, _1513)), spvFSub(_1480, _1513)))) / _3066;
                            float _1524 = spvFAdd(_1478, _1481);
                            float _1525 = spvFSub(_1524, _1478);
                            float _1530 = spvFAdd(spvFAdd(spvFSub(_1478, spvFSub(_1524, _1525)), spvFSub(_1481, _1525)), _1381);
                            float _1531 = spvFAdd(_1524, _1530);
                            float _1534 = spvFAdd(spvFSub(_1530, spvFSub(_1531, _1524)), _1386);
                            float _1535 = spvFAdd(_1531, _1534);
                            float _1537 = spvFSub(_1534, spvFSub(_1535, _1531));
                            float _1538 = spvFMul(_1535, _3066);
                            float _1539 = spvFMul(_1535, 4097.0);
                            float _1541 = spvFSub(_1539, spvFSub(_1539, _1535));
                            float _1542 = spvFSub(_1535, _1541);
                            float _1556 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1541, _1338), _1538), spvFMul(_1541, _1339)), spvFMul(_1542, _1338)), spvFMul(_1542, _1339)), spvFMul(_1535, 0.0)), spvFMul(_1537, _3066)), spvFMul(_1537, 0.0));
                            float _1557 = spvFAdd(_1538, _1556);
                            float _1482 = -_1557;
                            float _1483 = -spvFSub(_1556, spvFSub(_1557, _1538));
                            float _1560 = spvFAdd(_2866.x, _1482);
                            float _1561 = spvFSub(_1560, _2866.x);
                            float _1566 = spvFAdd(_2866.y, _1483);
                            float _1567 = spvFSub(_1566, _2866.y);
                            float _1572 = spvFAdd(spvFAdd(spvFSub(_2866.x, spvFSub(_1560, _1561)), spvFSub(_1482, _1561)), _1566);
                            float _1573 = spvFAdd(_1560, _1572);
                            float _1576 = spvFAdd(spvFSub(_1572, spvFSub(_1573, _1560)), spvFAdd(spvFSub(_2866.y, spvFSub(_1566, _1567)), spvFSub(_1483, _1567)));
                            float _1577 = spvFAdd(_1573, _1576);
                            float _1485 = spvFAdd(_1577, spvFSub(_1576, spvFSub(_1577, _1573))) / _3066;
                            float _1580 = spvFAdd(_1535, _1485);
                            float _1581 = spvFSub(_1580, _1535);
                            float _1586 = spvFAdd(_1537, 0.0);
                            float _1587 = spvFSub(_1586, _1537);
                            float _1592 = spvFAdd(spvFAdd(spvFSub(_1535, spvFSub(_1580, _1581)), spvFSub(_1485, _1581)), _1586);
                            float _1593 = spvFAdd(_1580, _1592);
                            float _1596 = spvFAdd(spvFSub(_1592, spvFSub(_1593, _1580)), spvFAdd(spvFSub(_1537, spvFSub(_1586, _1587)), spvFSub(0.0, _1587)));
                            float _1597 = spvFAdd(_1593, _1596);
                            float _1599 = spvFSub(_1596, spvFSub(_1597, _1593));
                            float _1600 = spvFAdd(_127, _1597);
                            float _1601 = spvFSub(_1600, _127);
                            float _1606 = spvFAdd(0.0, _1599);
                            float _1607 = spvFSub(_1606, 0.0);
                            float _1612 = spvFAdd(spvFAdd(spvFSub(_127, spvFSub(_1600, _1601)), spvFSub(_1597, _1601)), _1606);
                            float _1613 = spvFAdd(_1600, _1612);
                            float _1616 = spvFAdd(spvFSub(_1612, spvFSub(_1613, _1600)), spvFAdd(spvFSub(0.0, spvFSub(_1606, _1607)), spvFSub(_1599, _1607)));
                            float _1617 = spvFAdd(_1613, _1616);
                            float2 _3071 = float2(spvFAdd(_1474, spvFSub(_1473, spvFSub(_1474, _1470))), spvFAdd(_1617, spvFSub(_1616, spvFSub(_1617, _1613))));
                            bool4 _3072 = isnan(_3065);
                            bool4 _3073 = isinf(_3065);
                            bool _3084;
                            if (all(not(bool4(_3072.x || _3073.x, _3072.y || _3073.y, _3072.z || _3073.z, _3072.w || _3073.w))))
                            {
                                bool2 _3079 = isnan(_3071);
                                bool2 _3080 = isinf(_3071);
                                _3084 = all(not(bool2(_3079.x || _3080.x, _3079.y || _3080.y)));
                            }
                            else
                            {
                                _3084 = false;
                            }
                            _3086 = _3071;
                            _3087 = _3065;
                            _3088 = _3084 ? 1 : (-6);
                            break;
                        } while(false);
                        if (_3088 < 0)
                        {
                            __attribute__((unused)) uint _3092 = atomic_fetch_or_explicit((threadgroup atomic_uint*)&regionStatus, 64u, memory_order_relaxed);
                            __attribute__((unused)) uint _3096 = atomic_fetch_or_explicit((threadgroup atomic_uint*)&errorBits, 1u << (uint((-1) - _3088) & 31u), memory_order_relaxed);
                            _2103 = _3088;
                            continue;
                        }
                        if (_3088 == 0)
                        {
                            _2103 = _3088;
                            continue;
                        }
                        uint _3100 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&regionCount, 1u, memory_order_relaxed);
                        if (_3100 >= DomainSettings.regionBudget)
                        {
                            __attribute__((unused)) uint _3106 = atomic_fetch_or_explicit((threadgroup atomic_uint*)&regionStatus, 16u, memory_order_relaxed);
                            _2103 = _3088;
                            continue;
                        }
                        uint _50 = (_24 + 16u) + (_3100 * 48u);
                        uint _3107 = _50 >> 2u;
                        regions._m0[_3107] = _44.x;
                        regions._m0[_3107 + 1u] = _44.y;
                        regions._m0[_3107 + 2u] = _2105;
                        regions._m0[_3107 + 3u] = _2095;
                        uint _3114 = (_50 + 16u) >> 2u;
                        uint4 _3115 = as_type<uint4>(_3087);
                        regions._m0[_3114] = _3115.x;
                        regions._m0[_3114 + 1u] = _3115.y;
                        regions._m0[_3114 + 2u] = _3115.z;
                        regions._m0[_3114 + 3u] = _3115.w;
                        uint _3124 = (_50 + 32u) >> 2u;
                        uint2 _3125 = as_type<uint2>(_3086);
                        regions._m0[_3124] = _3125.x;
                        regions._m0[_3124 + 1u] = _3125.y;
                        regions._m0[_3124 + 2u] = 0u;
                        regions._m0[_3124 + 3u] = 0u;
                        _2103 = _3088;
                    }
                }
                _1907 = _2097;
            }
            else
            {
                _1907 = _1906;
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
        }
        uint _3135 = min(regionCount, DomainSettings.regionBudget);
        uint _65 = _3135 + gl_LocalInvocationIndex;
        for (uint _3137 = _65; _3137 < 128u; )
        {
            uint _68 = (_24 + 16u) + (_3137 * 48u);
            uint _3141 = _68 >> 2u;
            regions._m0[_3141] = 0u;
            regions._m0[_3141 + 1u] = 0u;
            regions._m0[_3141 + 2u] = 0u;
            regions._m0[_3141 + 3u] = 0u;
            uint _3146 = (_68 + 16u) >> 2u;
            regions._m0[_3146] = 0u;
            regions._m0[_3146 + 1u] = 0u;
            regions._m0[_3146 + 2u] = 0u;
            regions._m0[_3146 + 3u] = 0u;
            uint _3151 = (_68 + 32u) >> 2u;
            regions._m0[_3151] = 0u;
            regions._m0[_3151 + 1u] = 0u;
            regions._m0[_3151 + 2u] = 0u;
            regions._m0[_3151 + 3u] = 0u;
            _3137 += 64u;
            continue;
        }
        if (_1827)
        {
            uint _3158 = _24 >> 2u;
            regions._m0[_3158] = _3135;
            regions._m0[_3158 + 1u] = regionStatus;
            regions._m0[_3158 + 2u] = min(workCount, DomainSettings.supportBudget);
            regions._m0[_3158 + 3u] = errorBits;
        }
        break;
    } while(false);
}

