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

struct type_OpticalSettings
{
    uint opticalBudget;
    uint opticalDepth;
    uint opticalCapacity;
    uint opticalUnused;
};

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct type_TargetSettings
{
    float4 targetCurrentCube[3];
    float4 targetPreviousCube[3];
};

struct ReflectionRoughFrame
{
    float4 a;
    float4 b;
    float4 c;
    float4 projection;
    float4 extentClip;
    float4 settings;
};

struct ReflectionSpecularPlane
{
    float4 a;
    float4 b;
    float4 c;
};

struct ReflectionLiquidFrame
{
    float4 planePoint;
    float4 planeNormal;
    float4 rotation0;
    float4 rotation1;
    float4 rotation2;
    float4 projection;
    float4 extentClip;
    float4 settings;
};

struct Interval
{
    float lo;
    float hi;
};

struct Interval3
{
    Interval x;
    Interval y;
    Interval z;
};

struct CurvedScalar
{
    float high;
    float low;
};

constant spvUnsafeArray<int2, 8> _2253 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2254 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float normalLength = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3097 = isnan(old.planePoint);
    bool4 _3098 = isinf(old.planePoint);
    bool temp_var_logical = true;
    if (all(not(bool4(_3097.x || _3098.x, _3097.y || _3098.y, _3097.z || _3098.z, _3097.w || _3098.w))))
    {
        bool4 _3104 = isnan(old.planeNormal);
        bool4 _3105 = isinf(old.planeNormal);
        temp_var_logical = !all(not(bool4(_3104.x || _3105.x, _3104.y || _3105.y, _3104.z || _3105.z, _3104.w || _3105.w)));
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        bool4 _3114 = isnan(old.rotation0);
        bool4 _3115 = isinf(old.rotation0);
        temp_var_logical_1 = !all(not(bool4(_3114.x || _3115.x, _3114.y || _3115.y, _3114.z || _3115.z, _3114.w || _3115.w)));
    }
    bool temp_var_logical_2 = true;
    if (!temp_var_logical_1)
    {
        bool4 _3124 = isnan(old.rotation1);
        bool4 _3125 = isinf(old.rotation1);
        temp_var_logical_2 = !all(not(bool4(_3124.x || _3125.x, _3124.y || _3125.y, _3124.z || _3125.z, _3124.w || _3125.w)));
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical_2)
    {
        bool4 _3134 = isnan(old.rotation2);
        bool4 _3135 = isinf(old.rotation2);
        temp_var_logical_3 = !all(not(bool4(_3134.x || _3135.x, _3134.y || _3135.y, _3134.z || _3135.z, _3134.w || _3135.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3144 = isnan(old.projection);
        bool4 _3145 = isinf(old.projection);
        temp_var_logical_4 = !all(not(bool4(_3144.x || _3145.x, _3144.y || _3145.y, _3144.z || _3145.z, _3144.w || _3145.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3154 = isnan(old.extentClip);
        bool4 _3155 = isinf(old.extentClip);
        temp_var_logical_5 = !all(not(bool4(_3154.x || _3155.x, _3154.y || _3155.y, _3154.z || _3155.z, _3154.w || _3155.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3164 = isnan(old.settings);
        bool4 _3165 = isinf(old.settings);
        temp_var_logical_6 = !all(not(bool4(_3164.x || _3165.x, _3164.y || _3165.y, _3164.z || _3165.z, _3164.w || _3165.w)));
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        temp_var_logical_7 = any(abs(old.planePoint.xyz) > float3(999999995904.0));
    }
    bool temp_var_logical_8 = true;
    if (!temp_var_logical_7)
    {
        temp_var_logical_8 = isnan(normalLength) || isinf(normalLength);
    }
    bool temp_var_logical_9 = true;
    if (!temp_var_logical_8)
    {
        temp_var_logical_9 = normalLength <= 9.9999996826552253889678874634872e-21;
    }
    bool temp_var_logical_10 = true;
    if (!temp_var_logical_9)
    {
        temp_var_logical_10 = any(abs(old.rotation0) > float4(999999995904.0));
    }
    bool temp_var_logical_11 = true;
    if (!temp_var_logical_10)
    {
        temp_var_logical_11 = any(abs(old.rotation1) > float4(999999995904.0));
    }
    bool temp_var_logical_12 = true;
    if (!temp_var_logical_11)
    {
        temp_var_logical_12 = any(abs(old.rotation2) > float4(999999995904.0));
    }
    bool temp_var_logical_13 = true;
    if (!temp_var_logical_12)
    {
        temp_var_logical_13 = any(old.projection.xy <= float2(0.0));
    }
    bool temp_var_logical_14 = true;
    if (!temp_var_logical_13)
    {
        temp_var_logical_14 = any(old.projection.xy > float2(999999995904.0));
    }
    bool temp_var_logical_15 = true;
    if (!temp_var_logical_14)
    {
        temp_var_logical_15 = any(old.extentClip.xy < float2(1.0));
    }
    bool temp_var_logical_16 = true;
    if (!temp_var_logical_15)
    {
        temp_var_logical_16 = any(old.extentClip.xy > float2(16384.0));
    }
    bool temp_var_logical_17 = true;
    if (!temp_var_logical_16)
    {
        temp_var_logical_17 = old.extentClip.z <= 0.0;
    }
    bool temp_var_logical_18 = true;
    if (!temp_var_logical_17)
    {
        temp_var_logical_18 = old.extentClip.w <= old.extentClip.z;
    }
    bool temp_var_logical_19 = true;
    if (!temp_var_logical_18)
    {
        temp_var_logical_19 = old.extentClip.w > 999999995904.0;
    }
    bool temp_var_logical_20 = true;
    if (!temp_var_logical_19)
    {
        temp_var_logical_20 = old.settings.x < 0.0;
    }
    bool temp_var_logical_21 = true;
    if (!temp_var_logical_20)
    {
        temp_var_logical_21 = old.settings.x > 999999995904.0;
    }
    bool temp_var_logical_22 = true;
    if (!temp_var_logical_21)
    {
        bool temp_var_logical_23 = false;
        if (old.settings.y != 0.0)
        {
            temp_var_logical_23 = old.settings.y != 3.0;
        }
        temp_var_logical_22 = temp_var_logical_23;
    }
    if (temp_var_logical_22)
    {
        return false;
    }
    float determinant = dot(old.rotation0.xyz, cross(old.rotation1.xyz, old.rotation2.xyz));
    bool temp_var_logical_24 = false;
    if (!(isnan(determinant) || isinf(determinant)))
    {
        temp_var_logical_24 = abs(determinant) > 9.9999999600419720025001879548654e-13;
    }
    return temp_var_logical_24;
}

static inline __attribute__((always_inline))
bool feature_valid(thread const float3& f, thread const uint& kind)
{
    bool3 _3302 = isnan(f);
    bool3 _3303 = isinf(f);
    if (!all(not(bool3(_3302.x || _3303.x, _3302.y || _3303.y, _3302.z || _3303.z))))
    {
        return false;
    }
    if (kind == 1u)
    {
        bool temp_var_logical = false;
        if (f.z == 0.0)
        {
            temp_var_logical = all(f.xy >= float2(0.0));
        }
        bool temp_var_logical_1 = false;
        if (temp_var_logical)
        {
            temp_var_logical_1 = spvFAdd(f.x, f.y) <= 1.0;
        }
        return temp_var_logical_1;
    }
    bool temp_var_logical_2 = true;
    if (kind != 2u)
    {
        temp_var_logical_2 = kind == 3u;
    }
    bool temp_var_logical_3 = false;
    if (temp_var_logical_2)
    {
        temp_var_logical_3 = abs(spvFSub(dot(f, f), 1.0)) < 0.00010099999781232327222824096679688;
    }
    return temp_var_logical_3;
}

static inline __attribute__((always_inline))
bool reflection_specular_plane_valid(thread const ReflectionSpecularPlane& plane)
{
    ReflectionSpecularPlane param_var_p = plane;
    bool _3334 = false;
    if (param_var_p.a.w == 2.0)
    {
        _3334 = param_var_p.b.w == 2.0;
    }
    bool _3335 = false;
    if (_3334)
    {
        _3335 = param_var_p.c.w == 2.0;
    }
    bool _3336 = _3335;
    bool analytic = _3336;
    bool temp_var_logical = false;
    if (!analytic)
    {
        bool temp_var_logical_1 = true;
        if ((isunordered(plane.a.w, 1.0) || plane.a.w == 1.0))
        {
            temp_var_logical_1 = plane.b.w != 1.0;
        }
        bool temp_var_logical_2 = true;
        if (!temp_var_logical_1)
        {
            temp_var_logical_2 = plane.c.w != 1.0;
        }
        temp_var_logical = temp_var_logical_2;
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical)
    {
        bool4 _3378 = isnan(plane.a);
        bool4 _3379 = isinf(plane.a);
        temp_var_logical_3 = !all(not(bool4(_3378.x || _3379.x, _3378.y || _3379.y, _3378.z || _3379.z, _3378.w || _3379.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3388 = isnan(plane.b);
        bool4 _3389 = isinf(plane.b);
        temp_var_logical_4 = !all(not(bool4(_3388.x || _3389.x, _3388.y || _3389.y, _3388.z || _3389.z, _3388.w || _3389.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3398 = isnan(plane.c);
        bool4 _3399 = isinf(plane.c);
        temp_var_logical_5 = !all(not(bool4(_3398.x || _3399.x, _3398.y || _3399.y, _3398.z || _3399.z, _3398.w || _3399.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        temp_var_logical_6 = any(abs(plane.a.xyz) > float3(999999995904.0));
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        temp_var_logical_7 = any(abs(plane.b.xyz) > float3(999999995904.0));
    }
    bool temp_var_logical_8 = true;
    if (!temp_var_logical_7)
    {
        temp_var_logical_8 = any(abs(plane.c.xyz) > float3(999999995904.0));
    }
    if (temp_var_logical_8)
    {
        return false;
    }
    bool temp_var_logical_9 = false;
    if (analytic)
    {
        temp_var_logical_9 = any(plane.c.xyz != float3(0.0));
    }
    if (temp_var_logical_9)
    {
        return false;
    }
    float3 temp_var_ternary;
    if (analytic)
    {
        temp_var_ternary = plane.b.xyz;
    }
    else
    {
        temp_var_ternary = cross(spvFSub(plane.b.xyz, plane.a.xyz), spvFSub(plane.c.xyz, plane.a.xyz));
    }
    float3 n = temp_var_ternary;
    float length2 = dot(n, n);
    bool3 _3457 = isnan(n);
    bool3 _3458 = isinf(n);
    bool temp_var_logical_10 = false;
    if (all(not(bool3(_3457.x || _3458.x, _3457.y || _3458.y, _3457.z || _3458.z))))
    {
        temp_var_logical_10 = !(isnan(length2) || isinf(length2));
    }
    bool temp_var_logical_11 = false;
    if (temp_var_logical_10)
    {
        temp_var_logical_11 = length2 > 9.9999996826552253889678874634872e-21;
    }
    return temp_var_logical_11;
}

static inline __attribute__((always_inline))
bool reflection_rough_frame_valid(thread const ReflectionRoughFrame& old)
{
    ReflectionRoughFrame param_var_old = old;
    bool _3471 = false;
    if (param_var_old.a.w == 2.0)
    {
        _3471 = param_var_old.b.w == 2.0;
    }
    bool _3472 = false;
    if (_3471)
    {
        _3472 = param_var_old.c.w == 2.0;
    }
    bool _3473 = _3472;
    bool analytic = _3473;
    bool temp_var_logical = false;
    if (!analytic)
    {
        bool temp_var_logical_1 = true;
        if ((isunordered(old.a.w, 1.0) || old.a.w == 1.0))
        {
            temp_var_logical_1 = old.b.w != 1.0;
        }
        bool temp_var_logical_2 = true;
        if (!temp_var_logical_1)
        {
            temp_var_logical_2 = old.c.w != 1.0;
        }
        temp_var_logical = temp_var_logical_2;
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical)
    {
        bool4 _3515 = isnan(old.a);
        bool4 _3516 = isinf(old.a);
        temp_var_logical_3 = !all(not(bool4(_3515.x || _3516.x, _3515.y || _3516.y, _3515.z || _3516.z, _3515.w || _3516.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3525 = isnan(old.b);
        bool4 _3526 = isinf(old.b);
        temp_var_logical_4 = !all(not(bool4(_3525.x || _3526.x, _3525.y || _3526.y, _3525.z || _3526.z, _3525.w || _3526.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3535 = isnan(old.c);
        bool4 _3536 = isinf(old.c);
        temp_var_logical_5 = !all(not(bool4(_3535.x || _3536.x, _3535.y || _3536.y, _3535.z || _3536.z, _3535.w || _3536.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3545 = isnan(old.projection);
        bool4 _3546 = isinf(old.projection);
        temp_var_logical_6 = !all(not(bool4(_3545.x || _3546.x, _3545.y || _3546.y, _3545.z || _3546.z, _3545.w || _3546.w)));
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        bool4 _3555 = isnan(old.extentClip);
        bool4 _3556 = isinf(old.extentClip);
        temp_var_logical_7 = !all(not(bool4(_3555.x || _3556.x, _3555.y || _3556.y, _3555.z || _3556.z, _3555.w || _3556.w)));
    }
    bool temp_var_logical_8 = true;
    if (!temp_var_logical_7)
    {
        bool4 _3565 = isnan(old.settings);
        bool4 _3566 = isinf(old.settings);
        temp_var_logical_8 = !all(not(bool4(_3565.x || _3566.x, _3565.y || _3566.y, _3565.z || _3566.z, _3565.w || _3566.w)));
    }
    bool temp_var_logical_9 = true;
    if (!temp_var_logical_8)
    {
        temp_var_logical_9 = any(abs(old.a.xyz) > float3(999999995904.0));
    }
    bool temp_var_logical_10 = true;
    if (!temp_var_logical_9)
    {
        temp_var_logical_10 = any(abs(old.b.xyz) > float3(999999995904.0));
    }
    bool temp_var_logical_11 = true;
    if (!temp_var_logical_10)
    {
        temp_var_logical_11 = any(abs(old.c.xyz) > float3(999999995904.0));
    }
    bool temp_var_logical_12 = true;
    if (!temp_var_logical_11)
    {
        temp_var_logical_12 = any(old.projection.xy <= float2(0.0));
    }
    bool temp_var_logical_13 = true;
    if (!temp_var_logical_12)
    {
        temp_var_logical_13 = any(abs(old.projection) > float4(999999995904.0));
    }
    bool temp_var_logical_14 = true;
    if (!temp_var_logical_13)
    {
        temp_var_logical_14 = any(old.extentClip.xy < float2(1.0));
    }
    bool temp_var_logical_15 = true;
    if (!temp_var_logical_14)
    {
        temp_var_logical_15 = any(old.extentClip.xy > float2(16384.0));
    }
    bool temp_var_logical_16 = true;
    if (!temp_var_logical_15)
    {
        temp_var_logical_16 = old.extentClip.z <= 0.0;
    }
    bool temp_var_logical_17 = true;
    if (!temp_var_logical_16)
    {
        temp_var_logical_17 = old.extentClip.w <= old.extentClip.z;
    }
    bool temp_var_logical_18 = true;
    if (!temp_var_logical_17)
    {
        temp_var_logical_18 = old.extentClip.w > 999999995904.0;
    }
    bool temp_var_logical_19 = true;
    if (!temp_var_logical_18)
    {
        temp_var_logical_19 = old.settings.x < 0.0;
    }
    bool temp_var_logical_20 = true;
    if (!temp_var_logical_19)
    {
        temp_var_logical_20 = old.settings.x > 1.0;
    }
    bool temp_var_logical_21 = true;
    if (!temp_var_logical_20)
    {
        temp_var_logical_21 = old.settings.y < 0.0;
    }
    bool temp_var_logical_22 = true;
    if (!temp_var_logical_21)
    {
        temp_var_logical_22 = old.settings.y > 7.0;
    }
    bool temp_var_logical_23 = true;
    if (!temp_var_logical_22)
    {
        temp_var_logical_23 = floor(old.settings.y) != old.settings.y;
    }
    bool temp_var_logical_24 = true;
    if (!temp_var_logical_23)
    {
        temp_var_logical_24 = any(old.settings.zw != float2(0.0));
    }
    if (temp_var_logical_24)
    {
        return false;
    }
    bool temp_var_logical_25 = false;
    if (analytic)
    {
        temp_var_logical_25 = any(old.c.xyz != float3(0.0));
    }
    if (temp_var_logical_25)
    {
        return false;
    }
    float3 temp_var_ternary;
    if (analytic)
    {
        temp_var_ternary = old.b.xyz;
    }
    else
    {
        temp_var_ternary = cross(spvFSub(old.b.xyz, old.a.xyz), spvFSub(old.c.xyz, old.a.xyz));
    }
    float3 normal = temp_var_ternary;
    float length2 = dot(normal, normal);
    bool3 _3714 = isnan(normal);
    bool3 _3715 = isinf(normal);
    bool temp_var_logical_26 = false;
    if (all(not(bool3(_3714.x || _3715.x, _3714.y || _3715.y, _3714.z || _3715.z))))
    {
        temp_var_logical_26 = !(isnan(length2) || isinf(length2));
    }
    bool temp_var_logical_27 = false;
    if (temp_var_logical_26)
    {
        temp_var_logical_27 = length2 > 9.9999996826552253889678874634872e-21;
    }
    return temp_var_logical_27;
}

static inline __attribute__((always_inline))
float interval_down(thread const float& x, thread bool& intervalFailed)
{
    if (isnan(x) || isinf(x))
    {
        intervalFailed = true;
        return x;
    }
    if (x == 0.0)
    {
        return -as_type<float>(8388608u);
    }
    uint temp_var_ternary;
    if (x < 0.0)
    {
        temp_var_ternary = 1u;
    }
    else
    {
        temp_var_ternary = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + temp_var_ternary);
}

static inline __attribute__((always_inline))
float interval_up(thread const float& x, thread bool& intervalFailed)
{
    if (isnan(x) || isinf(x))
    {
        intervalFailed = true;
        return x;
    }
    if (x == 0.0)
    {
        return as_type<float>(8388608u);
    }
    uint temp_var_ternary;
    if (x > 0.0)
    {
        temp_var_ternary = 1u;
    }
    else
    {
        temp_var_ternary = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + temp_var_ternary);
}

static inline __attribute__((always_inline))
bool interval_exact_point(thread const Interval& a, thread const float& x)
{
    if (x == 0.0)
    {
        return ((as_type<uint>(a.lo) | as_type<uint>(a.hi)) & 2147483647u) == 0u;
    }
    bool temp_var_logical = false;
    if (as_type<uint>(a.lo) == as_type<uint>(x))
    {
        temp_var_logical = as_type<uint>(a.hi) == as_type<uint>(x);
    }
    return temp_var_logical;
}

static inline __attribute__((always_inline))
float optical_add_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    uint aa = as_type<uint>(a) & 2147483647u;
    uint bb = as_type<uint>(b) & 2147483647u;
    bool temp_var_logical = false;
    if (aa < 2139095040u)
    {
        temp_var_logical = bb < 2139095040u;
    }
    if (temp_var_logical)
    {
        if (aa == 0u)
        {
            return b;
        }
        if (bb == 0u)
        {
            return a;
        }
        bool temp_var_logical_1 = true;
        if (aa >= 8388608u)
        {
            temp_var_logical_1 = bb < 8388608u;
        }
        if (temp_var_logical_1)
        {
            intervalFailed = true;
        }
        bool temp_var_logical_2 = false;
        if (aa >= 562036736u)
        {
            temp_var_logical_2 = aa <= 1568669696u;
        }
        bool temp_var_logical_3 = false;
        if (temp_var_logical_2)
        {
            temp_var_logical_3 = bb >= 562036736u;
        }
        bool temp_var_logical_4 = false;
        if (temp_var_logical_3)
        {
            temp_var_logical_4 = bb <= 1568669696u;
        }
        if (temp_var_logical_4)
        {
            float temp_var_ternary;
            if (aa >= bb)
            {
                temp_var_ternary = a;
            }
            else
            {
                temp_var_ternary = b;
            }
            float large = temp_var_ternary;
            float temp_var_ternary_1;
            if (aa >= bb)
            {
                temp_var_ternary_1 = b;
            }
            else
            {
                temp_var_ternary_1 = a;
            }
            float small = temp_var_ternary_1;
            float sum = spvFAdd(large, small);
            float recovered = spvFSub(sum, large);
            float error = spvFSub(small, recovered);
            if (upper)
            {
                float temp_var_ternary_2;
                if (error > 0.0)
                {
                    float param_var_x = sum;
                    float _19942 = interval_up(param_var_x, intervalFailed);
                    temp_var_ternary_2 = _19942;
                }
                else
                {
                    temp_var_ternary_2 = sum;
                }
                return temp_var_ternary_2;
            }
            float temp_var_ternary_3;
            if (error < 0.0)
            {
                float param_var_x_1 = sum;
                float _19948 = interval_down(param_var_x_1, intervalFailed);
                temp_var_ternary_3 = _19948;
            }
            else
            {
                temp_var_ternary_3 = sum;
            }
            return temp_var_ternary_3;
        }
    }
    float sum_1 = spvFAdd(a, b);
    float temp_var_ternary_4;
    if (upper)
    {
        float param_var_x_2 = sum_1;
        float _19955 = interval_up(param_var_x_2, intervalFailed);
        temp_var_ternary_4 = _19955;
    }
    else
    {
        float param_var_x_3 = sum_1;
        float _19957 = interval_down(param_var_x_3, intervalFailed);
        temp_var_ternary_4 = _19957;
    }
    return temp_var_ternary_4;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    Interval param_var_a = a;
    bool _16155 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _16155 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _16156 = false;
    if (_16155)
    {
        _16156 = param_var_a.lo <= param_var_a.hi;
    }
    bool _16157 = _16156;
    bool temp_var_logical = false;
    if (_16157)
    {
        Interval param_var_a_1 = b;
        bool _16152 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _16152 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _16153 = false;
        if (_16152)
        {
            _16153 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _16154 = _16153;
        temp_var_logical = _16154;
    }
    if (temp_var_logical)
    {
        Interval param_var_a_2 = a;
        float param_var_x = 0.0;
        if (interval_exact_point(param_var_a_2, param_var_x))
        {
            return b;
        }
        Interval param_var_a_3 = b;
        float param_var_x_1 = 0.0;
        if (interval_exact_point(param_var_a_3, param_var_x_1))
        {
            return a;
        }
    }
    float param_var_a_4 = a.lo;
    float param_var_b = b.lo;
    bool param_var_upper = false;
    float _16219 = optical_add_bound(param_var_a_4, param_var_b, param_var_upper, intervalFailed);
    float param_var_lo = _16219;
    float param_var_a_5 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _16224 = optical_add_bound(param_var_a_5, param_var_b_1, param_var_upper_1, intervalFailed);
    float param_var_hi = _16224;
    Interval _16150;
    _16150.lo = param_var_lo;
    _16150.hi = param_var_hi;
    Interval _16151 = _16150;
    return _16151;
}

static inline __attribute__((always_inline))
float optical_product_bounds(thread const float& a, thread const float& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    uint aa = as_type<uint>(a) & 2147483647u;
    uint bb = as_type<uint>(b) & 2147483647u;
    bool temp_var_logical = false;
    if (aa < 2139095040u)
    {
        temp_var_logical = bb < 2139095040u;
    }
    if (temp_var_logical)
    {
        bool temp_var_logical_1 = true;
        if (aa != 0u)
        {
            temp_var_logical_1 = bb == 0u;
        }
        if (temp_var_logical_1)
        {
            optical_product_upper = 0.0;
            return 0.0;
        }
        if (as_type<uint>(a) == 1065353216u)
        {
            optical_product_upper = b;
            return b;
        }
        if (as_type<uint>(b) == 1065353216u)
        {
            optical_product_upper = a;
            return a;
        }
        if (as_type<uint>(a) == 3212836864u)
        {
            float result = as_type<float>(as_type<uint>(b) ^ 2147483648u);
            optical_product_upper = result;
            return result;
        }
        if (as_type<uint>(b) == 3212836864u)
        {
            float result_1 = as_type<float>(as_type<uint>(a) ^ 2147483648u);
            optical_product_upper = result_1;
            return result_1;
        }
        bool temp_var_logical_2 = true;
        if (aa >= 8388608u)
        {
            temp_var_logical_2 = bb < 8388608u;
        }
        if (temp_var_logical_2)
        {
            intervalFailed = true;
        }
        bool temp_var_logical_3 = false;
        if (aa >= 813694976u)
        {
            temp_var_logical_3 = aa <= 1317011456u;
        }
        bool temp_var_logical_4 = false;
        if (temp_var_logical_3)
        {
            temp_var_logical_4 = bb >= 813694976u;
        }
        bool temp_var_logical_5 = false;
        if (temp_var_logical_4)
        {
            temp_var_logical_5 = bb <= 1317011456u;
        }
        if (temp_var_logical_5)
        {
            float product = spvFMul(a, b);
            uint am = (aa & 8388607u) | 8388608u;
            uint bm = (bb & 8388607u) | 8388608u;
            uint al = am & 65535u;
            uint ah = am >> 16u;
            uint bl = bm & 65535u;
            uint bh = bm >> 16u;
            uint base = al * bl;
            uint _cross = (al * bh) + (ah * bl);
            uint exactLow = base + (_cross << 16u);
            uint temp_var_ternary;
            if (exactLow < base)
            {
                temp_var_ternary = 1u;
            }
            else
            {
                temp_var_ternary = 0u;
            }
            uint exactHigh = ((ah * bh) + (_cross >> 16u)) + temp_var_ternary;
            uint rounded = as_type<uint>(product) & 2147483647u;
            uint shift = (((rounded >> 23u) - (aa >> 23u)) - (bb >> 23u)) + 150u;
            bool temp_var_logical_6 = true;
            if (shift >= 23u)
            {
                temp_var_logical_6 = shift > 24u;
            }
            if (temp_var_logical_6)
            {
                intervalFailed = true;
                float param_var_x = product;
                float _20067 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _20067;
                float param_var_x_1 = product;
                float _20069 = interval_down(param_var_x_1, intervalFailed);
                return _20069;
            }
            uint rm = (rounded & 8388607u) | 8388608u;
            uint roundedLow = rm << (shift & 31u);
            uint roundedHigh = rm >> ((32u - shift) & 31u);
            bool temp_var_logical_7 = true;
            if (exactHigh <= roundedHigh)
            {
                bool temp_var_logical_8 = false;
                if (exactHigh == roundedHigh)
                {
                    temp_var_logical_8 = exactLow > roundedLow;
                }
                temp_var_logical_7 = temp_var_logical_8;
            }
            bool greater = temp_var_logical_7;
            bool temp_var_logical_9 = true;
            if (exactHigh >= roundedHigh)
            {
                bool temp_var_logical_10 = false;
                if (exactHigh == roundedHigh)
                {
                    temp_var_logical_10 = exactLow < roundedLow;
                }
                temp_var_logical_9 = temp_var_logical_10;
            }
            bool less = temp_var_logical_9;
            bool negative = ((as_type<uint>(a) ^ as_type<uint>(b)) & 2147483648u) != 0u;
            bool temp_var_ternary_2;
            if (negative)
            {
                temp_var_ternary_2 = less;
            }
            else
            {
                temp_var_ternary_2 = greater;
            }
            float temp_var_ternary_1;
            if (temp_var_ternary_2)
            {
                float param_var_x_2 = product;
                float _20115 = interval_up(param_var_x_2, intervalFailed);
                temp_var_ternary_1 = _20115;
            }
            else
            {
                temp_var_ternary_1 = product;
            }
            optical_product_upper = temp_var_ternary_1;
            bool temp_var_ternary_4;
            if (negative)
            {
                temp_var_ternary_4 = greater;
            }
            else
            {
                temp_var_ternary_4 = less;
            }
            float temp_var_ternary_3;
            if (temp_var_ternary_4)
            {
                float param_var_x_3 = product;
                float _20123 = interval_down(param_var_x_3, intervalFailed);
                temp_var_ternary_3 = _20123;
            }
            else
            {
                temp_var_ternary_3 = product;
            }
            return temp_var_ternary_3;
        }
    }
    float product_1 = spvFMul(a, b);
    float param_var_x_4 = product_1;
    float _20129 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _20129;
    float param_var_x_5 = product_1;
    float _20131 = interval_down(param_var_x_5, intervalFailed);
    return _20131;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval param_var_a = a;
    bool _18105 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _18105 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _18106 = false;
    if (_18105)
    {
        _18106 = param_var_a.lo <= param_var_a.hi;
    }
    bool _18107 = _18106;
    bool temp_var_logical = false;
    if (_18107)
    {
        Interval param_var_a_1 = b;
        bool _18102 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _18102 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _18103 = false;
        if (_18102)
        {
            _18103 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _18104 = _18103;
        temp_var_logical = _18104;
    }
    if (temp_var_logical)
    {
        Interval param_var_a_2 = a;
        float param_var_x = 0.0;
        bool temp_var_logical_1 = true;
        if (!interval_exact_point(param_var_a_2, param_var_x))
        {
            Interval param_var_a_3 = b;
            float param_var_x_1 = 0.0;
            temp_var_logical_1 = interval_exact_point(param_var_a_3, param_var_x_1);
        }
        if (temp_var_logical_1)
        {
            float param_var_x_2 = 0.0;
            float _18099 = param_var_x_2;
            float _18100 = param_var_x_2;
            Interval _18097;
            _18097.lo = _18099;
            _18097.hi = _18100;
            Interval _18098 = _18097;
            Interval _18101 = _18098;
            return _18101;
        }
        Interval param_var_a_4 = a;
        float param_var_x_3 = 1.0;
        if (interval_exact_point(param_var_a_4, param_var_x_3))
        {
            return b;
        }
        Interval param_var_a_5 = b;
        float param_var_x_4 = 1.0;
        if (interval_exact_point(param_var_a_5, param_var_x_4))
        {
            return a;
        }
        Interval param_var_a_6 = a;
        float param_var_x_5 = -1.0;
        if (interval_exact_point(param_var_a_6, param_var_x_5))
        {
            Interval param_var_a_7 = b;
            float _18094 = as_type<float>(as_type<uint>(param_var_a_7.hi) ^ 2147483648u);
            float _18095 = as_type<float>(as_type<uint>(param_var_a_7.lo) ^ 2147483648u);
            Interval _18092;
            _18092.lo = _18094;
            _18092.hi = _18095;
            Interval _18093 = _18092;
            Interval _18096 = _18093;
            return _18096;
        }
        Interval param_var_a_8 = b;
        float param_var_x_6 = -1.0;
        if (interval_exact_point(param_var_a_8, param_var_x_6))
        {
            Interval param_var_a_9 = a;
            float _18089 = as_type<float>(as_type<uint>(param_var_a_9.hi) ^ 2147483648u);
            float _18090 = as_type<float>(as_type<uint>(param_var_a_9.lo) ^ 2147483648u);
            Interval _18087;
            _18087.lo = _18089;
            _18087.hi = _18090;
            Interval _18088 = _18087;
            Interval _18091 = _18088;
            return _18091;
        }
    }
    Interval param_var_a_10 = a;
    bool _18084 = false;
    if (!(isnan(param_var_a_10.lo) || isinf(param_var_a_10.lo)))
    {
        _18084 = !(isnan(param_var_a_10.hi) || isinf(param_var_a_10.hi));
    }
    bool _18085 = false;
    if (_18084)
    {
        _18085 = param_var_a_10.lo <= param_var_a_10.hi;
    }
    bool _18086 = _18085;
    bool temp_var_logical_2 = false;
    if (_18086)
    {
        Interval param_var_a_11 = b;
        bool _18081 = false;
        if (!(isnan(param_var_a_11.lo) || isinf(param_var_a_11.lo)))
        {
            _18081 = !(isnan(param_var_a_11.hi) || isinf(param_var_a_11.hi));
        }
        bool _18082 = false;
        if (_18081)
        {
            _18082 = param_var_a_11.lo <= param_var_a_11.hi;
        }
        bool _18083 = _18082;
        temp_var_logical_2 = _18083;
    }
    bool finiteCorners = temp_var_logical_2;
    bool temp_var_logical_3 = false;
    if (finiteCorners)
    {
        temp_var_logical_3 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    bool sameA = temp_var_logical_3;
    bool temp_var_logical_4 = false;
    if (finiteCorners)
    {
        temp_var_logical_4 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    bool sameB = temp_var_logical_4;
    float param_var_a_12 = a.lo;
    float param_var_b = b.lo;
    float _18293 = optical_product_bounds(param_var_a_12, param_var_b, intervalFailed, optical_product_upper);
    float p = _18293;
    float u = optical_product_upper;
    float q = p;
    float v = u;
    if (!sameB)
    {
        float param_var_a_13 = a.lo;
        float param_var_b_1 = b.hi;
        float _18303 = optical_product_bounds(param_var_a_13, param_var_b_1, intervalFailed, optical_product_upper);
        q = _18303;
        v = optical_product_upper;
    }
    float r = p;
    float w = u;
    if (!sameA)
    {
        float param_var_a_14 = a.hi;
        float param_var_b_2 = b.lo;
        float _18313 = optical_product_bounds(param_var_a_14, param_var_b_2, intervalFailed, optical_product_upper);
        r = _18313;
        w = optical_product_upper;
    }
    float s = q;
    float x = v;
    if (!sameA)
    {
        if (sameB)
        {
            s = r;
            x = w;
        }
        else
        {
            float param_var_a_15 = a.hi;
            float param_var_b_3 = b.hi;
            float _18326 = optical_product_bounds(param_var_a_15, param_var_b_3, intervalFailed, optical_product_upper);
            s = _18326;
            x = optical_product_upper;
        }
    }
    float param_var_lo = precise::min(precise::min(p, q), precise::min(r, s));
    float param_var_hi = precise::max(precise::max(u, v), precise::max(w, x));
    Interval _18079;
    _18079.lo = param_var_lo;
    _18079.hi = param_var_hi;
    Interval _18080 = _18079;
    return _18080;
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float q = precise::sqrt(a);
    bool temp_var_ternary;
    float temp_var_ternary_1;
    CurvedScalar _20166;
    for (uint n = 0u; n < 8u; n++)
    {
        float param_var_ax = q;
        float param_var_ay = 0.0;
        float param_var_bx = q;
        float param_var_by = 0.0;
        float _20168 = spvFMul(param_var_ax, param_var_bx);
        float _20169 = spvFMul(param_var_ax, 4097.0);
        float _20170 = spvFSub(_20169, spvFSub(_20169, param_var_ax));
        float _20171 = spvFSub(param_var_ax, _20170);
        float _20172 = spvFMul(param_var_bx, 4097.0);
        float _20173 = spvFSub(_20172, spvFSub(_20172, param_var_bx));
        float _20174 = spvFSub(param_var_bx, _20173);
        float _20175 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_20170, _20173), _20168), spvFMul(_20170, _20174)), spvFMul(_20171, _20173)), spvFMul(_20171, _20174)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _20176 = spvFAdd(_20168, _20175);
        float _20177 = spvFSub(_20175, spvFSub(_20176, _20168));
        float _20178 = _20176;
        float _20179 = _20177;
        _20166.high = _20178;
        _20166.low = _20179;
        CurvedScalar _20167 = _20166;
        CurvedScalar _20180 = _20167;
        CurvedScalar square = _20180;
        bool temp_var_logical = true;
        if (!(isnan(square.high) || isinf(square.high)))
        {
            temp_var_logical = isnan(square.low) || isinf(square.low);
        }
        if (temp_var_logical)
        {
            intervalFailed = true;
            return q;
        }
        bool temp_var_logical_1 = true;
        if ((isunordered(square.high, a) || square.high >= a))
        {
            bool temp_var_logical_2 = false;
            if (square.high == a)
            {
                temp_var_logical_2 = square.low < 0.0;
            }
            temp_var_logical_1 = temp_var_logical_2;
        }
        bool below = temp_var_logical_1;
        bool temp_var_logical_3 = true;
        if ((isunordered(a, square.high) || a >= square.high))
        {
            bool temp_var_logical_4 = false;
            if (a == square.high)
            {
                temp_var_logical_4 = 0.0 < square.low;
            }
            temp_var_logical_3 = temp_var_logical_4;
        }
        bool above = temp_var_logical_3;
        if (upper)
        {
            temp_var_ternary = !below;
        }
        else
        {
            temp_var_ternary = !above;
        }
        if (temp_var_ternary)
        {
            return q;
        }
        if (upper)
        {
            float param_var_x = q;
            float _20278 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_1 = _20278;
        }
        else
        {
            float param_var_x_1 = q;
            float _20280 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_1 = precise::max(0.0, _20280);
        }
        q = temp_var_ternary_1;
    }
    intervalFailed = true;
    return q;
}

static inline __attribute__((always_inline))
Interval isqrt(thread const Interval& a, thread bool& intervalFailed)
{
    bool temp_var_logical = true;
    if ((isunordered(a.hi, 0.0) || a.hi >= 0.0))
    {
        temp_var_logical = a.hi > 1000000015047466219876688855040.0;
    }
    if (temp_var_logical)
    {
        intervalFailed = true;
        float param_var_lo = 0.0;
        float param_var_hi = 1000000015047466219876688855040.0;
        Interval _20134;
        _20134.lo = param_var_lo;
        _20134.hi = param_var_hi;
        Interval _20135 = _20134;
        return _20135;
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _20152 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _20152;
    float _20153 = interval_down(param_var_x, intervalFailed);
    float param_var_lo_1 = precise::max(0.0, _20153);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _20158 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _20158;
    float _20159 = interval_up(param_var_x_1, intervalFailed);
    float param_var_hi_1 = _20159;
    Interval _20132;
    _20132.lo = param_var_lo_1;
    _20132.hi = param_var_hi_1;
    Interval _20133 = _20132;
    return _20133;
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float q = a / b;
    bool temp_var_ternary;
    bool temp_var_ternary_1;
    float temp_var_ternary_2;
    CurvedScalar _19726;
    for (uint n = 0u; n < 8u; n++)
    {
        bool temp_var_logical = true;
        if (!(isnan(q) || isinf(q)))
        {
            temp_var_logical = abs(q) > 1000000015047466219876688855040.0;
        }
        if (temp_var_logical)
        {
            intervalFailed = true;
            return q;
        }
        float param_var_ax = q;
        float param_var_ay = 0.0;
        float param_var_bx = b;
        float param_var_by = 0.0;
        float _19728 = spvFMul(param_var_ax, param_var_bx);
        float _19729 = spvFMul(param_var_ax, 4097.0);
        float _19730 = spvFSub(_19729, spvFSub(_19729, param_var_ax));
        float _19731 = spvFSub(param_var_ax, _19730);
        float _19732 = spvFMul(param_var_bx, 4097.0);
        float _19733 = spvFSub(_19732, spvFSub(_19732, param_var_bx));
        float _19734 = spvFSub(param_var_bx, _19733);
        float _19735 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_19730, _19733), _19728), spvFMul(_19730, _19734)), spvFMul(_19731, _19733)), spvFMul(_19731, _19734)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _19736 = spvFAdd(_19728, _19735);
        float _19737 = spvFSub(_19735, spvFSub(_19736, _19728));
        float _19738 = _19736;
        float _19739 = _19737;
        _19726.high = _19738;
        _19726.low = _19739;
        CurvedScalar _19727 = _19726;
        CurvedScalar _19740 = _19727;
        CurvedScalar product = _19740;
        bool temp_var_logical_1 = true;
        if (!(isnan(product.high) || isinf(product.high)))
        {
            temp_var_logical_1 = isnan(product.low) || isinf(product.low);
        }
        if (temp_var_logical_1)
        {
            intervalFailed = true;
            return q;
        }
        bool temp_var_logical_2 = true;
        if ((isunordered(product.high, a) || product.high >= a))
        {
            bool temp_var_logical_3 = false;
            if (product.high == a)
            {
                temp_var_logical_3 = product.low < 0.0;
            }
            temp_var_logical_2 = temp_var_logical_3;
        }
        bool below = temp_var_logical_2;
        bool temp_var_logical_4 = true;
        if ((isunordered(a, product.high) || a >= product.high))
        {
            bool temp_var_logical_5 = false;
            if (a == product.high)
            {
                temp_var_logical_5 = 0.0 < product.low;
            }
            temp_var_logical_4 = temp_var_logical_5;
        }
        bool above = temp_var_logical_4;
        if (b < 0.0)
        {
            if (upper)
            {
                temp_var_ternary = !above;
            }
            else
            {
                temp_var_ternary = !below;
            }
            if (temp_var_ternary)
            {
                return q;
            }
        }
        else
        {
            if (upper)
            {
                temp_var_ternary_1 = !below;
            }
            else
            {
                temp_var_ternary_1 = !above;
            }
            if (temp_var_ternary_1)
            {
                return q;
            }
        }
        if (upper)
        {
            float param_var_x = q;
            float _19857 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_2 = _19857;
        }
        else
        {
            float param_var_x_1 = q;
            float _19859 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_2 = _19859;
        }
        q = temp_var_ternary_2;
    }
    intervalFailed = true;
    return q;
}

static inline __attribute__((always_inline))
Interval idiv(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    Interval param_var_a = b;
    bool _16307 = false;
    if (param_var_a.lo <= 0.0)
    {
        _16307 = param_var_a.hi >= 0.0;
    }
    bool _16308 = _16307;
    bool temp_var_logical = true;
    if (!_16308)
    {
        temp_var_logical = precise::max(abs(b.lo), abs(b.hi)) > 1000000015047466219876688855040.0;
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        temp_var_logical_1 = precise::max(abs(a.lo), abs(a.hi)) > 1000000015047466219876688855040.0;
    }
    if (temp_var_logical_1)
    {
        intervalFailed = true;
        float param_var_lo = -1000000015047466219876688855040.0;
        float param_var_hi = 1000000015047466219876688855040.0;
        Interval _16305;
        _16305.lo = param_var_lo;
        _16305.hi = param_var_hi;
        Interval _16306 = _16305;
        return _16306;
    }
    Interval param_var_a_1 = a;
    bool _16302 = false;
    if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
    {
        _16302 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
    }
    bool _16303 = false;
    if (_16302)
    {
        _16303 = param_var_a_1.lo <= param_var_a_1.hi;
    }
    bool _16304 = _16303;
    bool temp_var_logical_2 = false;
    if (_16304)
    {
        Interval param_var_a_2 = b;
        bool _16299 = false;
        if (!(isnan(param_var_a_2.lo) || isinf(param_var_a_2.lo)))
        {
            _16299 = !(isnan(param_var_a_2.hi) || isinf(param_var_a_2.hi));
        }
        bool _16300 = false;
        if (_16299)
        {
            _16300 = param_var_a_2.lo <= param_var_a_2.hi;
        }
        bool _16301 = _16300;
        temp_var_logical_2 = _16301;
    }
    if (temp_var_logical_2)
    {
        Interval param_var_a_3 = a;
        float param_var_x = 0.0;
        if (interval_exact_point(param_var_a_3, param_var_x))
        {
            float param_var_x_1 = 0.0;
            float _16296 = param_var_x_1;
            float _16297 = param_var_x_1;
            Interval _16294;
            _16294.lo = _16296;
            _16294.hi = _16297;
            Interval _16295 = _16294;
            Interval _16298 = _16295;
            return _16298;
        }
        Interval param_var_a_4 = b;
        float param_var_x_2 = 1.0;
        if (interval_exact_point(param_var_a_4, param_var_x_2))
        {
            return a;
        }
        Interval param_var_a_5 = b;
        float param_var_x_3 = -1.0;
        if (interval_exact_point(param_var_a_5, param_var_x_3))
        {
            Interval param_var_a_6 = a;
            float _16291 = as_type<float>(as_type<uint>(param_var_a_6.hi) ^ 2147483648u);
            float _16292 = as_type<float>(as_type<uint>(param_var_a_6.lo) ^ 2147483648u);
            Interval _16289;
            _16289.lo = _16291;
            _16289.hi = _16292;
            Interval _16290 = _16289;
            Interval _16293 = _16290;
            return _16293;
        }
    }
    float param_var_alo = a.lo;
    float param_var_ahi = a.hi;
    float param_var_blo = b.lo;
    float param_var_bhi = b.hi;
    float _16244 = param_var_alo;
    float _16245 = param_var_ahi;
    Interval _16241;
    _16241.lo = _16244;
    _16241.hi = _16245;
    Interval _16242 = _16241;
    Interval _16246 = _16242;
    bool _16238 = false;
    if (!(isnan(_16246.lo) || isinf(_16246.lo)))
    {
        _16238 = !(isnan(_16246.hi) || isinf(_16246.hi));
    }
    bool _16239 = false;
    if (_16238)
    {
        _16239 = _16246.lo <= _16246.hi;
    }
    bool _16240 = _16239;
    bool _16247 = false;
    if (_16240)
    {
        float _16248 = param_var_blo;
        float _16249 = param_var_bhi;
        Interval _16236;
        _16236.lo = _16248;
        _16236.hi = _16249;
        Interval _16237 = _16236;
        Interval _16250 = _16237;
        bool _16233 = false;
        if (!(isnan(_16250.lo) || isinf(_16250.lo)))
        {
            _16233 = !(isnan(_16250.hi) || isinf(_16250.hi));
        }
        bool _16234 = false;
        if (_16233)
        {
            _16234 = _16250.lo <= _16250.hi;
        }
        bool _16235 = _16234;
        _16247 = _16235;
    }
    bool _16243 = _16247;
    bool _16252 = false;
    if (_16243)
    {
        _16252 = as_type<uint>(param_var_alo) == as_type<uint>(param_var_ahi);
    }
    bool _16251 = _16252;
    bool _16254 = false;
    if (_16243)
    {
        _16254 = as_type<uint>(param_var_blo) == as_type<uint>(param_var_bhi);
    }
    bool _16253 = _16254;
    float _16256 = param_var_alo;
    float _16257 = param_var_blo;
    bool _16258 = false;
    float _16526 = quotient_bound(_16256, _16257, _16258, intervalFailed);
    float _16255 = _16526;
    float _16260 = param_var_alo;
    float _16261 = param_var_blo;
    bool _16262 = true;
    float _16529 = quotient_bound(_16260, _16261, _16262, intervalFailed);
    float _16259 = _16529;
    float _16263 = _16255;
    float _16264 = _16259;
    if (!_16253)
    {
        float _16265 = param_var_alo;
        float _16266 = param_var_bhi;
        bool _16267 = false;
        float _16538 = quotient_bound(_16265, _16266, _16267, intervalFailed);
        _16263 = _16538;
        float _16268 = param_var_alo;
        float _16269 = param_var_bhi;
        bool _16270 = true;
        float _16541 = quotient_bound(_16268, _16269, _16270, intervalFailed);
        _16264 = _16541;
    }
    float _16271 = _16255;
    float _16272 = _16259;
    if (!_16251)
    {
        float _16273 = param_var_ahi;
        float _16274 = param_var_blo;
        bool _16275 = false;
        float _16550 = quotient_bound(_16273, _16274, _16275, intervalFailed);
        _16271 = _16550;
        float _16276 = param_var_ahi;
        float _16277 = param_var_blo;
        bool _16278 = true;
        float _16553 = quotient_bound(_16276, _16277, _16278, intervalFailed);
        _16272 = _16553;
    }
    float _16279 = _16263;
    float _16280 = _16264;
    if (!_16251)
    {
        if (_16253)
        {
            _16279 = _16271;
            _16280 = _16272;
        }
        else
        {
            float _16281 = param_var_ahi;
            float _16282 = param_var_bhi;
            bool _16283 = false;
            float _16568 = quotient_bound(_16281, _16282, _16283, intervalFailed);
            _16279 = _16568;
            float _16284 = param_var_ahi;
            float _16285 = param_var_bhi;
            bool _16286 = true;
            float _16571 = quotient_bound(_16284, _16285, _16286, intervalFailed);
            _16280 = _16571;
        }
    }
    float _16287 = precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16255), _16263), _16271), _16279);
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16259), _16264), _16272), _16280);
    float _16288 = _16287;
    float lo = _16288;
    float param_var_x_4 = lo;
    float _16591 = interval_down(param_var_x_4, intervalFailed);
    float param_var_lo_1 = _16591;
    float param_var_x_5 = interval_divide_upper;
    float _16593 = interval_up(param_var_x_5, intervalFailed);
    float param_var_hi_1 = _16593;
    Interval _16231;
    _16231.lo = param_var_lo_1;
    _16231.hi = param_var_hi_1;
    Interval _16232 = _16231;
    return _16232;
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, device type_ByteAddressBuffer& frames, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        float param_var_x = feature.x;
        float _4213 = param_var_x;
        float _4214 = param_var_x;
        Interval _4211;
        _4211.lo = _4213;
        _4211.hi = _4214;
        Interval _4212 = _4211;
        Interval _4215 = _4212;
        Interval x = _4215;
        float param_var_x_1 = feature.y;
        float _4208 = param_var_x_1;
        float _4209 = param_var_x_1;
        Interval _4206;
        _4206.lo = _4208;
        _4206.hi = _4209;
        Interval _4207 = _4206;
        Interval _4210 = _4207;
        Interval y = _4210;
        float param_var_x_2 = 1.0;
        float _4203 = param_var_x_2;
        float _4204 = param_var_x_2;
        Interval _4201;
        _4201.lo = _4203;
        _4201.hi = _4204;
        Interval _4202 = _4201;
        Interval _4205 = _4202;
        Interval param_var_a = _4205;
        Interval param_var_a_1 = x;
        Interval param_var_b = y;
        Interval _4252 = iadd(param_var_a_1, param_var_b, intervalFailed);
        Interval param_var_b_1 = _4252;
        Interval _4197 = param_var_a;
        Interval _4198 = param_var_b_1;
        float _4194 = as_type<float>(as_type<uint>(_4198.hi) ^ 2147483648u);
        float _4195 = as_type<float>(as_type<uint>(_4198.lo) ^ 2147483648u);
        Interval _4192;
        _4192.lo = _4194;
        _4192.hi = _4195;
        Interval _4193 = _4192;
        Interval _4196 = _4193;
        Interval _4199 = _4196;
        Interval _4272 = iadd(_4197, _4199, intervalFailed);
        Interval _4200 = _4272;
        Interval a = _4200;
        uint _4275 = (at + 432u) >> 2u;
        float3 param_var_v = as_type<float4>(uint4(frames._m0[_4275], frames._m0[_4275 + 1u], frames._m0[_4275 + 2u], frames._m0[_4275 + 3u])).xyz;
        float _4185 = param_var_v.x;
        float _4182 = _4185;
        float _4183 = _4185;
        Interval _4180;
        _4180.lo = _4182;
        _4180.hi = _4183;
        Interval _4181 = _4180;
        Interval _4184 = _4181;
        Interval _4186 = _4184;
        float _4187 = param_var_v.y;
        float _4177 = _4187;
        float _4178 = _4187;
        Interval _4175;
        _4175.lo = _4177;
        _4175.hi = _4178;
        Interval _4176 = _4175;
        Interval _4179 = _4176;
        Interval _4188 = _4179;
        float _4189 = param_var_v.z;
        float _4172 = _4189;
        float _4173 = _4189;
        Interval _4170;
        _4170.lo = _4172;
        _4170.hi = _4173;
        Interval _4171 = _4170;
        Interval _4174 = _4171;
        Interval _4190 = _4174;
        Interval3 _4168;
        _4168.x = _4186;
        _4168.y = _4188;
        _4168.z = _4190;
        Interval3 _4169 = _4168;
        Interval3 _4191 = _4169;
        Interval3 param_var_a_2 = _4191;
        Interval param_var_b_2 = a;
        Interval _4158 = param_var_a_2.x;
        Interval _4159 = param_var_b_2;
        Interval _4333 = imul(_4158, _4159, intervalFailed, optical_product_upper);
        Interval _4160 = _4333;
        Interval _4161 = param_var_a_2.y;
        Interval _4162 = param_var_b_2;
        Interval _4337 = imul(_4161, _4162, intervalFailed, optical_product_upper);
        Interval _4163 = _4337;
        Interval _4164 = param_var_a_2.z;
        Interval _4165 = param_var_b_2;
        Interval _4341 = imul(_4164, _4165, intervalFailed, optical_product_upper);
        Interval _4166 = _4341;
        Interval3 _4156;
        _4156.x = _4160;
        _4156.y = _4163;
        _4156.z = _4166;
        Interval3 _4157 = _4156;
        Interval3 _4167 = _4157;
        Interval3 param_var_a_3 = _4167;
        uint _4352 = (at + 448u) >> 2u;
        float3 param_var_v_1 = as_type<float4>(uint4(frames._m0[_4352], frames._m0[_4352 + 1u], frames._m0[_4352 + 2u], frames._m0[_4352 + 3u])).xyz;
        float _4149 = param_var_v_1.x;
        float _4146 = _4149;
        float _4147 = _4149;
        Interval _4144;
        _4144.lo = _4146;
        _4144.hi = _4147;
        Interval _4145 = _4144;
        Interval _4148 = _4145;
        Interval _4150 = _4148;
        float _4151 = param_var_v_1.y;
        float _4141 = _4151;
        float _4142 = _4151;
        Interval _4139;
        _4139.lo = _4141;
        _4139.hi = _4142;
        Interval _4140 = _4139;
        Interval _4143 = _4140;
        Interval _4152 = _4143;
        float _4153 = param_var_v_1.z;
        float _4136 = _4153;
        float _4137 = _4153;
        Interval _4134;
        _4134.lo = _4136;
        _4134.hi = _4137;
        Interval _4135 = _4134;
        Interval _4138 = _4135;
        Interval _4154 = _4138;
        Interval3 _4132;
        _4132.x = _4150;
        _4132.y = _4152;
        _4132.z = _4154;
        Interval3 _4133 = _4132;
        Interval3 _4155 = _4133;
        Interval3 param_var_a_4 = _4155;
        Interval param_var_b_3 = x;
        Interval _4122 = param_var_a_4.x;
        Interval _4123 = param_var_b_3;
        Interval _4410 = imul(_4122, _4123, intervalFailed, optical_product_upper);
        Interval _4124 = _4410;
        Interval _4125 = param_var_a_4.y;
        Interval _4126 = param_var_b_3;
        Interval _4414 = imul(_4125, _4126, intervalFailed, optical_product_upper);
        Interval _4127 = _4414;
        Interval _4128 = param_var_a_4.z;
        Interval _4129 = param_var_b_3;
        Interval _4418 = imul(_4128, _4129, intervalFailed, optical_product_upper);
        Interval _4130 = _4418;
        Interval3 _4120;
        _4120.x = _4124;
        _4120.y = _4127;
        _4120.z = _4130;
        Interval3 _4121 = _4120;
        Interval3 _4131 = _4121;
        Interval3 param_var_b_4 = _4131;
        Interval _4110 = param_var_a_3.x;
        Interval _4111 = param_var_b_4.x;
        Interval _4432 = iadd(_4110, _4111, intervalFailed);
        Interval _4112 = _4432;
        Interval _4113 = param_var_a_3.y;
        Interval _4114 = param_var_b_4.y;
        Interval _4437 = iadd(_4113, _4114, intervalFailed);
        Interval _4115 = _4437;
        Interval _4116 = param_var_a_3.z;
        Interval _4117 = param_var_b_4.z;
        Interval _4442 = iadd(_4116, _4117, intervalFailed);
        Interval _4118 = _4442;
        Interval3 _4108;
        _4108.x = _4112;
        _4108.y = _4115;
        _4108.z = _4118;
        Interval3 _4109 = _4108;
        Interval3 _4119 = _4109;
        Interval3 param_var_a_5 = _4119;
        uint _4453 = (at + 464u) >> 2u;
        float3 param_var_v_2 = as_type<float4>(uint4(frames._m0[_4453], frames._m0[_4453 + 1u], frames._m0[_4453 + 2u], frames._m0[_4453 + 3u])).xyz;
        float _4101 = param_var_v_2.x;
        float _4098 = _4101;
        float _4099 = _4101;
        Interval _4096;
        _4096.lo = _4098;
        _4096.hi = _4099;
        Interval _4097 = _4096;
        Interval _4100 = _4097;
        Interval _4102 = _4100;
        float _4103 = param_var_v_2.y;
        float _4093 = _4103;
        float _4094 = _4103;
        Interval _4091;
        _4091.lo = _4093;
        _4091.hi = _4094;
        Interval _4092 = _4091;
        Interval _4095 = _4092;
        Interval _4104 = _4095;
        float _4105 = param_var_v_2.z;
        float _4088 = _4105;
        float _4089 = _4105;
        Interval _4086;
        _4086.lo = _4088;
        _4086.hi = _4089;
        Interval _4087 = _4086;
        Interval _4090 = _4087;
        Interval _4106 = _4090;
        Interval3 _4084;
        _4084.x = _4102;
        _4084.y = _4104;
        _4084.z = _4106;
        Interval3 _4085 = _4084;
        Interval3 _4107 = _4085;
        Interval3 param_var_a_6 = _4107;
        Interval param_var_b_5 = y;
        Interval _4074 = param_var_a_6.x;
        Interval _4075 = param_var_b_5;
        Interval _4511 = imul(_4074, _4075, intervalFailed, optical_product_upper);
        Interval _4076 = _4511;
        Interval _4077 = param_var_a_6.y;
        Interval _4078 = param_var_b_5;
        Interval _4515 = imul(_4077, _4078, intervalFailed, optical_product_upper);
        Interval _4079 = _4515;
        Interval _4080 = param_var_a_6.z;
        Interval _4081 = param_var_b_5;
        Interval _4519 = imul(_4080, _4081, intervalFailed, optical_product_upper);
        Interval _4082 = _4519;
        Interval3 _4072;
        _4072.x = _4076;
        _4072.y = _4079;
        _4072.z = _4082;
        Interval3 _4073 = _4072;
        Interval3 _4083 = _4073;
        Interval3 param_var_b_6 = _4083;
        Interval _4062 = param_var_a_5.x;
        Interval _4063 = param_var_b_6.x;
        Interval _4533 = iadd(_4062, _4063, intervalFailed);
        Interval _4064 = _4533;
        Interval _4065 = param_var_a_5.y;
        Interval _4066 = param_var_b_6.y;
        Interval _4538 = iadd(_4065, _4066, intervalFailed);
        Interval _4067 = _4538;
        Interval _4068 = param_var_a_5.z;
        Interval _4069 = param_var_b_6.z;
        Interval _4543 = iadd(_4068, _4069, intervalFailed);
        Interval _4070 = _4543;
        Interval3 _4060;
        _4060.x = _4064;
        _4060.y = _4067;
        _4060.z = _4070;
        Interval3 _4061 = _4060;
        Interval3 _4071 = _4061;
        return _4071;
    }
    float3 param_var_v_3 = feature.xyz;
    float _4053 = param_var_v_3.x;
    float _4050 = _4053;
    float _4051 = _4053;
    Interval _4048;
    _4048.lo = _4050;
    _4048.hi = _4051;
    Interval _4049 = _4048;
    Interval _4052 = _4049;
    Interval _4054 = _4052;
    float _4055 = param_var_v_3.y;
    float _4045 = _4055;
    float _4046 = _4055;
    Interval _4043;
    _4043.lo = _4045;
    _4043.hi = _4046;
    Interval _4044 = _4043;
    Interval _4047 = _4044;
    Interval _4056 = _4047;
    float _4057 = param_var_v_3.z;
    float _4040 = _4057;
    float _4041 = _4057;
    Interval _4038;
    _4038.lo = _4040;
    _4038.hi = _4041;
    Interval _4039 = _4038;
    Interval _4042 = _4039;
    Interval _4058 = _4042;
    Interval3 _4036;
    _4036.x = _4054;
    _4036.y = _4056;
    _4036.z = _4058;
    Interval3 _4037 = _4036;
    Interval3 _4059 = _4037;
    Interval3 raw = _4059;
    float3 param_var_v_4 = TargetSettings.targetCurrentCube[0].xyz;
    float _4029 = param_var_v_4.x;
    float _4026 = _4029;
    float _4027 = _4029;
    Interval _4024;
    _4024.lo = _4026;
    _4024.hi = _4027;
    Interval _4025 = _4024;
    Interval _4028 = _4025;
    Interval _4030 = _4028;
    float _4031 = param_var_v_4.y;
    float _4021 = _4031;
    float _4022 = _4031;
    Interval _4019;
    _4019.lo = _4021;
    _4019.hi = _4022;
    Interval _4020 = _4019;
    Interval _4023 = _4020;
    Interval _4032 = _4023;
    float _4033 = param_var_v_4.z;
    float _4016 = _4033;
    float _4017 = _4033;
    Interval _4014;
    _4014.lo = _4016;
    _4014.hi = _4017;
    Interval _4015 = _4014;
    Interval _4018 = _4015;
    Interval _4034 = _4018;
    Interval3 _4012;
    _4012.x = _4030;
    _4012.y = _4032;
    _4012.z = _4034;
    Interval3 _4013 = _4012;
    Interval3 _4035 = _4013;
    Interval3 param_var_a_7 = _4035;
    Interval3 param_var_b_7 = raw;
    Interval _4001 = param_var_a_7.x;
    Interval _4002 = param_var_b_7.x;
    Interval _4648 = imul(_4001, _4002, intervalFailed, optical_product_upper);
    Interval _4003 = _4648;
    Interval _4004 = param_var_a_7.y;
    Interval _4005 = param_var_b_7.y;
    Interval _4653 = imul(_4004, _4005, intervalFailed, optical_product_upper);
    Interval _4006 = _4653;
    Interval _4654 = iadd(_4003, _4006, intervalFailed);
    Interval _4007 = _4654;
    Interval _4008 = param_var_a_7.z;
    Interval _4009 = param_var_b_7.z;
    Interval _4659 = imul(_4008, _4009, intervalFailed, optical_product_upper);
    Interval _4010 = _4659;
    Interval _4660 = iadd(_4007, _4010, intervalFailed);
    Interval _4011 = _4660;
    Interval param_var_x_3 = _4011;
    float3 param_var_v_5 = TargetSettings.targetCurrentCube[1].xyz;
    float _3994 = param_var_v_5.x;
    float _3991 = _3994;
    float _3992 = _3994;
    Interval _3989;
    _3989.lo = _3991;
    _3989.hi = _3992;
    Interval _3990 = _3989;
    Interval _3993 = _3990;
    Interval _3995 = _3993;
    float _3996 = param_var_v_5.y;
    float _3986 = _3996;
    float _3987 = _3996;
    Interval _3984;
    _3984.lo = _3986;
    _3984.hi = _3987;
    Interval _3985 = _3984;
    Interval _3988 = _3985;
    Interval _3997 = _3988;
    float _3998 = param_var_v_5.z;
    float _3981 = _3998;
    float _3982 = _3998;
    Interval _3979;
    _3979.lo = _3981;
    _3979.hi = _3982;
    Interval _3980 = _3979;
    Interval _3983 = _3980;
    Interval _3999 = _3983;
    Interval3 _3977;
    _3977.x = _3995;
    _3977.y = _3997;
    _3977.z = _3999;
    Interval3 _3978 = _3977;
    Interval3 _4000 = _3978;
    Interval3 param_var_a_8 = _4000;
    Interval3 param_var_b_8 = raw;
    Interval _3966 = param_var_a_8.x;
    Interval _3967 = param_var_b_8.x;
    Interval _4713 = imul(_3966, _3967, intervalFailed, optical_product_upper);
    Interval _3968 = _4713;
    Interval _3969 = param_var_a_8.y;
    Interval _3970 = param_var_b_8.y;
    Interval _4718 = imul(_3969, _3970, intervalFailed, optical_product_upper);
    Interval _3971 = _4718;
    Interval _4719 = iadd(_3968, _3971, intervalFailed);
    Interval _3972 = _4719;
    Interval _3973 = param_var_a_8.z;
    Interval _3974 = param_var_b_8.z;
    Interval _4724 = imul(_3973, _3974, intervalFailed, optical_product_upper);
    Interval _3975 = _4724;
    Interval _4725 = iadd(_3972, _3975, intervalFailed);
    Interval _3976 = _4725;
    Interval param_var_y = _3976;
    float3 param_var_v_6 = TargetSettings.targetCurrentCube[2].xyz;
    float _3959 = param_var_v_6.x;
    float _3956 = _3959;
    float _3957 = _3959;
    Interval _3954;
    _3954.lo = _3956;
    _3954.hi = _3957;
    Interval _3955 = _3954;
    Interval _3958 = _3955;
    Interval _3960 = _3958;
    float _3961 = param_var_v_6.y;
    float _3951 = _3961;
    float _3952 = _3961;
    Interval _3949;
    _3949.lo = _3951;
    _3949.hi = _3952;
    Interval _3950 = _3949;
    Interval _3953 = _3950;
    Interval _3962 = _3953;
    float _3963 = param_var_v_6.z;
    float _3946 = _3963;
    float _3947 = _3963;
    Interval _3944;
    _3944.lo = _3946;
    _3944.hi = _3947;
    Interval _3945 = _3944;
    Interval _3948 = _3945;
    Interval _3964 = _3948;
    Interval3 _3942;
    _3942.x = _3960;
    _3942.y = _3962;
    _3942.z = _3964;
    Interval3 _3943 = _3942;
    Interval3 _3965 = _3943;
    Interval3 param_var_a_9 = _3965;
    Interval3 param_var_b_9 = raw;
    Interval _3931 = param_var_a_9.x;
    Interval _3932 = param_var_b_9.x;
    Interval _4778 = imul(_3931, _3932, intervalFailed, optical_product_upper);
    Interval _3933 = _4778;
    Interval _3934 = param_var_a_9.y;
    Interval _3935 = param_var_b_9.y;
    Interval _4783 = imul(_3934, _3935, intervalFailed, optical_product_upper);
    Interval _3936 = _4783;
    Interval _4784 = iadd(_3933, _3936, intervalFailed);
    Interval _3937 = _4784;
    Interval _3938 = param_var_a_9.z;
    Interval _3939 = param_var_b_9.z;
    Interval _4789 = imul(_3938, _3939, intervalFailed, optical_product_upper);
    Interval _3940 = _4789;
    Interval _4790 = iadd(_3937, _3940, intervalFailed);
    Interval _3941 = _4790;
    Interval param_var_z = _3941;
    Interval3 _3929;
    _3929.x = param_var_x_3;
    _3929.y = param_var_y;
    _3929.z = param_var_z;
    Interval3 _3930 = _3929;
    Interval3 world = _3930;
    float3 param_var_v_7 = float3(TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[2].x);
    float _3922 = param_var_v_7.x;
    float _3919 = _3922;
    float _3920 = _3922;
    Interval _3917;
    _3917.lo = _3919;
    _3917.hi = _3920;
    Interval _3918 = _3917;
    Interval _3921 = _3918;
    Interval _3923 = _3921;
    float _3924 = param_var_v_7.y;
    float _3914 = _3924;
    float _3915 = _3924;
    Interval _3912;
    _3912.lo = _3914;
    _3912.hi = _3915;
    Interval _3913 = _3912;
    Interval _3916 = _3913;
    Interval _3925 = _3916;
    float _3926 = param_var_v_7.z;
    float _3909 = _3926;
    float _3910 = _3926;
    Interval _3907;
    _3907.lo = _3909;
    _3907.hi = _3910;
    Interval _3908 = _3907;
    Interval _3911 = _3908;
    Interval _3927 = _3911;
    Interval3 _3905;
    _3905.x = _3923;
    _3905.y = _3925;
    _3905.z = _3927;
    Interval3 _3906 = _3905;
    Interval3 _3928 = _3906;
    Interval3 param_var_a_10 = _3928;
    Interval3 param_var_b_10 = world;
    Interval _3894 = param_var_a_10.x;
    Interval _3895 = param_var_b_10.x;
    Interval _4860 = imul(_3894, _3895, intervalFailed, optical_product_upper);
    Interval _3896 = _4860;
    Interval _3897 = param_var_a_10.y;
    Interval _3898 = param_var_b_10.y;
    Interval _4865 = imul(_3897, _3898, intervalFailed, optical_product_upper);
    Interval _3899 = _4865;
    Interval _4866 = iadd(_3896, _3899, intervalFailed);
    Interval _3900 = _4866;
    Interval _3901 = param_var_a_10.z;
    Interval _3902 = param_var_b_10.z;
    Interval _4871 = imul(_3901, _3902, intervalFailed, optical_product_upper);
    Interval _3903 = _4871;
    Interval _4872 = iadd(_3900, _3903, intervalFailed);
    Interval _3904 = _4872;
    Interval param_var_x_4 = _3904;
    float3 param_var_v_8 = float3(TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[2].y);
    float _3887 = param_var_v_8.x;
    float _3884 = _3887;
    float _3885 = _3887;
    Interval _3882;
    _3882.lo = _3884;
    _3882.hi = _3885;
    Interval _3883 = _3882;
    Interval _3886 = _3883;
    Interval _3888 = _3886;
    float _3889 = param_var_v_8.y;
    float _3879 = _3889;
    float _3880 = _3889;
    Interval _3877;
    _3877.lo = _3879;
    _3877.hi = _3880;
    Interval _3878 = _3877;
    Interval _3881 = _3878;
    Interval _3890 = _3881;
    float _3891 = param_var_v_8.z;
    float _3874 = _3891;
    float _3875 = _3891;
    Interval _3872;
    _3872.lo = _3874;
    _3872.hi = _3875;
    Interval _3873 = _3872;
    Interval _3876 = _3873;
    Interval _3892 = _3876;
    Interval3 _3870;
    _3870.x = _3888;
    _3870.y = _3890;
    _3870.z = _3892;
    Interval3 _3871 = _3870;
    Interval3 _3893 = _3871;
    Interval3 param_var_a_11 = _3893;
    Interval3 param_var_b_11 = world;
    Interval _3859 = param_var_a_11.x;
    Interval _3860 = param_var_b_11.x;
    Interval _4934 = imul(_3859, _3860, intervalFailed, optical_product_upper);
    Interval _3861 = _4934;
    Interval _3862 = param_var_a_11.y;
    Interval _3863 = param_var_b_11.y;
    Interval _4939 = imul(_3862, _3863, intervalFailed, optical_product_upper);
    Interval _3864 = _4939;
    Interval _4940 = iadd(_3861, _3864, intervalFailed);
    Interval _3865 = _4940;
    Interval _3866 = param_var_a_11.z;
    Interval _3867 = param_var_b_11.z;
    Interval _4945 = imul(_3866, _3867, intervalFailed, optical_product_upper);
    Interval _3868 = _4945;
    Interval _4946 = iadd(_3865, _3868, intervalFailed);
    Interval _3869 = _4946;
    Interval param_var_y_1 = _3869;
    float3 param_var_v_9 = float3(TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[2].z);
    float _3852 = param_var_v_9.x;
    float _3849 = _3852;
    float _3850 = _3852;
    Interval _3847;
    _3847.lo = _3849;
    _3847.hi = _3850;
    Interval _3848 = _3847;
    Interval _3851 = _3848;
    Interval _3853 = _3851;
    float _3854 = param_var_v_9.y;
    float _3844 = _3854;
    float _3845 = _3854;
    Interval _3842;
    _3842.lo = _3844;
    _3842.hi = _3845;
    Interval _3843 = _3842;
    Interval _3846 = _3843;
    Interval _3855 = _3846;
    float _3856 = param_var_v_9.z;
    float _3839 = _3856;
    float _3840 = _3856;
    Interval _3837;
    _3837.lo = _3839;
    _3837.hi = _3840;
    Interval _3838 = _3837;
    Interval _3841 = _3838;
    Interval _3857 = _3841;
    Interval3 _3835;
    _3835.x = _3853;
    _3835.y = _3855;
    _3835.z = _3857;
    Interval3 _3836 = _3835;
    Interval3 _3858 = _3836;
    Interval3 param_var_a_12 = _3858;
    Interval3 param_var_b_12 = world;
    Interval _3824 = param_var_a_12.x;
    Interval _3825 = param_var_b_12.x;
    Interval _5008 = imul(_3824, _3825, intervalFailed, optical_product_upper);
    Interval _3826 = _5008;
    Interval _3827 = param_var_a_12.y;
    Interval _3828 = param_var_b_12.y;
    Interval _5013 = imul(_3827, _3828, intervalFailed, optical_product_upper);
    Interval _3829 = _5013;
    Interval _5014 = iadd(_3826, _3829, intervalFailed);
    Interval _3830 = _5014;
    Interval _3831 = param_var_a_12.z;
    Interval _3832 = param_var_b_12.z;
    Interval _5019 = imul(_3831, _3832, intervalFailed, optical_product_upper);
    Interval _3833 = _5019;
    Interval _5020 = iadd(_3830, _3833, intervalFailed);
    Interval _3834 = _5020;
    Interval param_var_z_1 = _3834;
    Interval3 _3822;
    _3822.x = param_var_x_4;
    _3822.y = param_var_y_1;
    _3822.z = param_var_z_1;
    Interval3 _3823 = _3822;
    Interval3 param_var_a_13 = _3823;
    Interval3 _3815 = param_var_a_13;
    float _3816 = 1.0;
    float _3812 = _3816;
    float _3813 = _3816;
    Interval _3810;
    _3810.lo = _3812;
    _3810.hi = _3813;
    Interval _3811 = _3810;
    Interval _3814 = _3811;
    Interval _3817 = _3814;
    Interval3 _3818 = param_var_a_13;
    Interval _3801 = _3818.x;
    bool _3794 = false;
    if (_3801.lo <= 0.0)
    {
        _3794 = _3801.hi >= 0.0;
    }
    float _3793;
    if (_3794)
    {
        _3793 = 0.0;
    }
    else
    {
        _3793 = precise::min(abs(_3801.lo), abs(_3801.hi));
    }
    float _3792 = _3793;
    float _3795 = precise::max(abs(_3801.lo), abs(_3801.hi));
    float _3796 = spvFMul(_3792, _3792);
    float _5072 = interval_down(_3796, intervalFailed);
    float _3797 = precise::max(0.0, _5072);
    float _3798 = spvFMul(_3795, _3795);
    float _5076 = interval_up(_3798, intervalFailed);
    float _3799 = _5076;
    Interval _3790;
    _3790.lo = _3797;
    _3790.hi = _3799;
    Interval _3791 = _3790;
    Interval _3800 = _3791;
    Interval _3802 = _3800;
    Interval _3803 = _3818.y;
    bool _3783 = false;
    if (_3803.lo <= 0.0)
    {
        _3783 = _3803.hi >= 0.0;
    }
    float _3782;
    if (_3783)
    {
        _3782 = 0.0;
    }
    else
    {
        _3782 = precise::min(abs(_3803.lo), abs(_3803.hi));
    }
    float _3781 = _3782;
    float _3784 = precise::max(abs(_3803.lo), abs(_3803.hi));
    float _3785 = spvFMul(_3781, _3781);
    float _5115 = interval_down(_3785, intervalFailed);
    float _3786 = precise::max(0.0, _5115);
    float _3787 = spvFMul(_3784, _3784);
    float _5119 = interval_up(_3787, intervalFailed);
    float _3788 = _5119;
    Interval _3779;
    _3779.lo = _3786;
    _3779.hi = _3788;
    Interval _3780 = _3779;
    Interval _3789 = _3780;
    Interval _3804 = _3789;
    Interval _5127 = iadd(_3802, _3804, intervalFailed);
    Interval _3805 = _5127;
    Interval _3806 = _3818.z;
    bool _3772 = false;
    if (_3806.lo <= 0.0)
    {
        _3772 = _3806.hi >= 0.0;
    }
    float _3771;
    if (_3772)
    {
        _3771 = 0.0;
    }
    else
    {
        _3771 = precise::min(abs(_3806.lo), abs(_3806.hi));
    }
    float _3770 = _3771;
    float _3773 = precise::max(abs(_3806.lo), abs(_3806.hi));
    float _3774 = spvFMul(_3770, _3770);
    float _5159 = interval_down(_3774, intervalFailed);
    float _3775 = precise::max(0.0, _5159);
    float _3776 = spvFMul(_3773, _3773);
    float _5163 = interval_up(_3776, intervalFailed);
    float _3777 = _5163;
    Interval _3768;
    _3768.lo = _3775;
    _3768.hi = _3777;
    Interval _3769 = _3768;
    Interval _3778 = _3769;
    Interval _3807 = _3778;
    Interval _5171 = iadd(_3805, _3807, intervalFailed);
    Interval _3808 = _5171;
    Interval _5172 = isqrt(_3808, intervalFailed);
    Interval _3809 = _5172;
    Interval _3819 = _3809;
    Interval _5174 = idiv(_3817, _3819, intervalFailed, interval_divide_upper);
    Interval _3820 = _5174;
    Interval _3758 = _3815.x;
    Interval _3759 = _3820;
    Interval _5178 = imul(_3758, _3759, intervalFailed, optical_product_upper);
    Interval _3760 = _5178;
    Interval _3761 = _3815.y;
    Interval _3762 = _3820;
    Interval _5182 = imul(_3761, _3762, intervalFailed, optical_product_upper);
    Interval _3763 = _5182;
    Interval _3764 = _3815.z;
    Interval _3765 = _3820;
    Interval _5186 = imul(_3764, _3765, intervalFailed, optical_product_upper);
    Interval _3766 = _5186;
    Interval3 _3756;
    _3756.x = _3760;
    _3756.y = _3763;
    _3756.z = _3766;
    Interval3 _3757 = _3756;
    Interval3 _3767 = _3757;
    Interval3 _3821 = _3767;
    return _3821;
}

static inline __attribute__((always_inline))
float split_axis(thread const float& lo, thread const float& hi, thread const uint& cell, thread const uint& count)
{
    if (cell == 0u)
    {
        return lo;
    }
    if (cell == count)
    {
        return hi;
    }
    float span = spvFSub(hi, lo);
    float fraction = float(cell) / float(count);
    float position = spvFAdd(lo, spvFMul(span, fraction));
    return position;
}

static inline __attribute__((always_inline))
Interval3 plane_normal(thread const ReflectionSpecularPlane& p, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = p;
    bool _16979 = false;
    if (param_var_p.a.w == 2.0)
    {
        _16979 = param_var_p.b.w == 2.0;
    }
    bool _16980 = false;
    if (_16979)
    {
        _16980 = param_var_p.c.w == 2.0;
    }
    bool _16981 = _16980;
    if (_16981)
    {
        float3 param_var_v = p.b.xyz;
        float _16972 = param_var_v.x;
        float _16969 = _16972;
        float _16970 = _16972;
        Interval _16967;
        _16967.lo = _16969;
        _16967.hi = _16970;
        Interval _16968 = _16967;
        Interval _16971 = _16968;
        Interval _16973 = _16971;
        float _16974 = param_var_v.y;
        float _16964 = _16974;
        float _16965 = _16974;
        Interval _16962;
        _16962.lo = _16964;
        _16962.hi = _16965;
        Interval _16963 = _16962;
        Interval _16966 = _16963;
        Interval _16975 = _16966;
        float _16976 = param_var_v.z;
        float _16959 = _16976;
        float _16960 = _16976;
        Interval _16957;
        _16957.lo = _16959;
        _16957.hi = _16960;
        Interval _16958 = _16957;
        Interval _16961 = _16958;
        Interval _16977 = _16961;
        Interval3 _16955;
        _16955.x = _16973;
        _16955.y = _16975;
        _16955.z = _16977;
        Interval3 _16956 = _16955;
        Interval3 _16978 = _16956;
        Interval3 param_var_a = _16978;
        Interval3 _16948 = param_var_a;
        float _16949 = 1.0;
        float _16945 = _16949;
        float _16946 = _16949;
        Interval _16943;
        _16943.lo = _16945;
        _16943.hi = _16946;
        Interval _16944 = _16943;
        Interval _16947 = _16944;
        Interval _16950 = _16947;
        Interval3 _16951 = param_var_a;
        Interval _16934 = _16951.x;
        bool _16927 = false;
        if (_16934.lo <= 0.0)
        {
            _16927 = _16934.hi >= 0.0;
        }
        float _16926;
        if (_16927)
        {
            _16926 = 0.0;
        }
        else
        {
            _16926 = precise::min(abs(_16934.lo), abs(_16934.hi));
        }
        float _16925 = _16926;
        float _16928 = precise::max(abs(_16934.lo), abs(_16934.hi));
        float _16929 = spvFMul(_16925, _16925);
        float _17089 = interval_down(_16929, intervalFailed);
        float _16930 = precise::max(0.0, _17089);
        float _16931 = spvFMul(_16928, _16928);
        float _17093 = interval_up(_16931, intervalFailed);
        float _16932 = _17093;
        Interval _16923;
        _16923.lo = _16930;
        _16923.hi = _16932;
        Interval _16924 = _16923;
        Interval _16933 = _16924;
        Interval _16935 = _16933;
        Interval _16936 = _16951.y;
        bool _16916 = false;
        if (_16936.lo <= 0.0)
        {
            _16916 = _16936.hi >= 0.0;
        }
        float _16915;
        if (_16916)
        {
            _16915 = 0.0;
        }
        else
        {
            _16915 = precise::min(abs(_16936.lo), abs(_16936.hi));
        }
        float _16914 = _16915;
        float _16917 = precise::max(abs(_16936.lo), abs(_16936.hi));
        float _16918 = spvFMul(_16914, _16914);
        float _17132 = interval_down(_16918, intervalFailed);
        float _16919 = precise::max(0.0, _17132);
        float _16920 = spvFMul(_16917, _16917);
        float _17136 = interval_up(_16920, intervalFailed);
        float _16921 = _17136;
        Interval _16912;
        _16912.lo = _16919;
        _16912.hi = _16921;
        Interval _16913 = _16912;
        Interval _16922 = _16913;
        Interval _16937 = _16922;
        Interval _17144 = iadd(_16935, _16937, intervalFailed);
        Interval _16938 = _17144;
        Interval _16939 = _16951.z;
        bool _16905 = false;
        if (_16939.lo <= 0.0)
        {
            _16905 = _16939.hi >= 0.0;
        }
        float _16904;
        if (_16905)
        {
            _16904 = 0.0;
        }
        else
        {
            _16904 = precise::min(abs(_16939.lo), abs(_16939.hi));
        }
        float _16903 = _16904;
        float _16906 = precise::max(abs(_16939.lo), abs(_16939.hi));
        float _16907 = spvFMul(_16903, _16903);
        float _17176 = interval_down(_16907, intervalFailed);
        float _16908 = precise::max(0.0, _17176);
        float _16909 = spvFMul(_16906, _16906);
        float _17180 = interval_up(_16909, intervalFailed);
        float _16910 = _17180;
        Interval _16901;
        _16901.lo = _16908;
        _16901.hi = _16910;
        Interval _16902 = _16901;
        Interval _16911 = _16902;
        Interval _16940 = _16911;
        Interval _17188 = iadd(_16938, _16940, intervalFailed);
        Interval _16941 = _17188;
        Interval _17189 = isqrt(_16941, intervalFailed);
        Interval _16942 = _17189;
        Interval _16952 = _16942;
        Interval _17191 = idiv(_16950, _16952, intervalFailed, interval_divide_upper);
        Interval _16953 = _17191;
        Interval _16891 = _16948.x;
        Interval _16892 = _16953;
        Interval _17195 = imul(_16891, _16892, intervalFailed, optical_product_upper);
        Interval _16893 = _17195;
        Interval _16894 = _16948.y;
        Interval _16895 = _16953;
        Interval _17199 = imul(_16894, _16895, intervalFailed, optical_product_upper);
        Interval _16896 = _17199;
        Interval _16897 = _16948.z;
        Interval _16898 = _16953;
        Interval _17203 = imul(_16897, _16898, intervalFailed, optical_product_upper);
        Interval _16899 = _17203;
        Interval3 _16889;
        _16889.x = _16893;
        _16889.y = _16896;
        _16889.z = _16899;
        Interval3 _16890 = _16889;
        Interval3 _16900 = _16890;
        Interval3 _16954 = _16900;
        return _16954;
    }
    float3 param_var_v_1 = p.b.xyz;
    float _16882 = param_var_v_1.x;
    float _16879 = _16882;
    float _16880 = _16882;
    Interval _16877;
    _16877.lo = _16879;
    _16877.hi = _16880;
    Interval _16878 = _16877;
    Interval _16881 = _16878;
    Interval _16883 = _16881;
    float _16884 = param_var_v_1.y;
    float _16874 = _16884;
    float _16875 = _16884;
    Interval _16872;
    _16872.lo = _16874;
    _16872.hi = _16875;
    Interval _16873 = _16872;
    Interval _16876 = _16873;
    Interval _16885 = _16876;
    float _16886 = param_var_v_1.z;
    float _16869 = _16886;
    float _16870 = _16886;
    Interval _16867;
    _16867.lo = _16869;
    _16867.hi = _16870;
    Interval _16868 = _16867;
    Interval _16871 = _16868;
    Interval _16887 = _16871;
    Interval3 _16865;
    _16865.x = _16883;
    _16865.y = _16885;
    _16865.z = _16887;
    Interval3 _16866 = _16865;
    Interval3 _16888 = _16866;
    Interval3 param_var_a_1 = _16888;
    float3 param_var_v_2 = p.a.xyz;
    float _16858 = param_var_v_2.x;
    float _16855 = _16858;
    float _16856 = _16858;
    Interval _16853;
    _16853.lo = _16855;
    _16853.hi = _16856;
    Interval _16854 = _16853;
    Interval _16857 = _16854;
    Interval _16859 = _16857;
    float _16860 = param_var_v_2.y;
    float _16850 = _16860;
    float _16851 = _16860;
    Interval _16848;
    _16848.lo = _16850;
    _16848.hi = _16851;
    Interval _16849 = _16848;
    Interval _16852 = _16849;
    Interval _16861 = _16852;
    float _16862 = param_var_v_2.z;
    float _16845 = _16862;
    float _16846 = _16862;
    Interval _16843;
    _16843.lo = _16845;
    _16843.hi = _16846;
    Interval _16844 = _16843;
    Interval _16847 = _16844;
    Interval _16863 = _16847;
    Interval3 _16841;
    _16841.x = _16859;
    _16841.y = _16861;
    _16841.z = _16863;
    Interval3 _16842 = _16841;
    Interval3 _16864 = _16842;
    Interval3 param_var_b = _16864;
    Interval3 _16832 = param_var_a_1;
    Interval _16833 = param_var_b.x;
    float _16829 = as_type<float>(as_type<uint>(_16833.hi) ^ 2147483648u);
    float _16830 = as_type<float>(as_type<uint>(_16833.lo) ^ 2147483648u);
    Interval _16827;
    _16827.lo = _16829;
    _16827.hi = _16830;
    Interval _16828 = _16827;
    Interval _16831 = _16828;
    Interval _16834 = _16831;
    Interval _16835 = param_var_b.y;
    float _16824 = as_type<float>(as_type<uint>(_16835.hi) ^ 2147483648u);
    float _16825 = as_type<float>(as_type<uint>(_16835.lo) ^ 2147483648u);
    Interval _16822;
    _16822.lo = _16824;
    _16822.hi = _16825;
    Interval _16823 = _16822;
    Interval _16826 = _16823;
    Interval _16836 = _16826;
    Interval _16837 = param_var_b.z;
    float _16819 = as_type<float>(as_type<uint>(_16837.hi) ^ 2147483648u);
    float _16820 = as_type<float>(as_type<uint>(_16837.lo) ^ 2147483648u);
    Interval _16817;
    _16817.lo = _16819;
    _16817.hi = _16820;
    Interval _16818 = _16817;
    Interval _16821 = _16818;
    Interval _16838 = _16821;
    Interval3 _16815;
    _16815.x = _16834;
    _16815.y = _16836;
    _16815.z = _16838;
    Interval3 _16816 = _16815;
    Interval3 _16839 = _16816;
    Interval _16805 = _16832.x;
    Interval _16806 = _16839.x;
    Interval _17374 = iadd(_16805, _16806, intervalFailed);
    Interval _16807 = _17374;
    Interval _16808 = _16832.y;
    Interval _16809 = _16839.y;
    Interval _17379 = iadd(_16808, _16809, intervalFailed);
    Interval _16810 = _17379;
    Interval _16811 = _16832.z;
    Interval _16812 = _16839.z;
    Interval _17384 = iadd(_16811, _16812, intervalFailed);
    Interval _16813 = _17384;
    Interval3 _16803;
    _16803.x = _16807;
    _16803.y = _16810;
    _16803.z = _16813;
    Interval3 _16804 = _16803;
    Interval3 _16814 = _16804;
    Interval3 _16840 = _16814;
    Interval3 param_var_a_2 = _16840;
    float3 param_var_v_3 = p.c.xyz;
    float _16796 = param_var_v_3.x;
    float _16793 = _16796;
    float _16794 = _16796;
    Interval _16791;
    _16791.lo = _16793;
    _16791.hi = _16794;
    Interval _16792 = _16791;
    Interval _16795 = _16792;
    Interval _16797 = _16795;
    float _16798 = param_var_v_3.y;
    float _16788 = _16798;
    float _16789 = _16798;
    Interval _16786;
    _16786.lo = _16788;
    _16786.hi = _16789;
    Interval _16787 = _16786;
    Interval _16790 = _16787;
    Interval _16799 = _16790;
    float _16800 = param_var_v_3.z;
    float _16783 = _16800;
    float _16784 = _16800;
    Interval _16781;
    _16781.lo = _16783;
    _16781.hi = _16784;
    Interval _16782 = _16781;
    Interval _16785 = _16782;
    Interval _16801 = _16785;
    Interval3 _16779;
    _16779.x = _16797;
    _16779.y = _16799;
    _16779.z = _16801;
    Interval3 _16780 = _16779;
    Interval3 _16802 = _16780;
    Interval3 param_var_a_3 = _16802;
    float3 param_var_v_4 = p.a.xyz;
    float _16772 = param_var_v_4.x;
    float _16769 = _16772;
    float _16770 = _16772;
    Interval _16767;
    _16767.lo = _16769;
    _16767.hi = _16770;
    Interval _16768 = _16767;
    Interval _16771 = _16768;
    Interval _16773 = _16771;
    float _16774 = param_var_v_4.y;
    float _16764 = _16774;
    float _16765 = _16774;
    Interval _16762;
    _16762.lo = _16764;
    _16762.hi = _16765;
    Interval _16763 = _16762;
    Interval _16766 = _16763;
    Interval _16775 = _16766;
    float _16776 = param_var_v_4.z;
    float _16759 = _16776;
    float _16760 = _16776;
    Interval _16757;
    _16757.lo = _16759;
    _16757.hi = _16760;
    Interval _16758 = _16757;
    Interval _16761 = _16758;
    Interval _16777 = _16761;
    Interval3 _16755;
    _16755.x = _16773;
    _16755.y = _16775;
    _16755.z = _16777;
    Interval3 _16756 = _16755;
    Interval3 _16778 = _16756;
    Interval3 param_var_b_1 = _16778;
    Interval3 _16746 = param_var_a_3;
    Interval _16747 = param_var_b_1.x;
    float _16743 = as_type<float>(as_type<uint>(_16747.hi) ^ 2147483648u);
    float _16744 = as_type<float>(as_type<uint>(_16747.lo) ^ 2147483648u);
    Interval _16741;
    _16741.lo = _16743;
    _16741.hi = _16744;
    Interval _16742 = _16741;
    Interval _16745 = _16742;
    Interval _16748 = _16745;
    Interval _16749 = param_var_b_1.y;
    float _16738 = as_type<float>(as_type<uint>(_16749.hi) ^ 2147483648u);
    float _16739 = as_type<float>(as_type<uint>(_16749.lo) ^ 2147483648u);
    Interval _16736;
    _16736.lo = _16738;
    _16736.hi = _16739;
    Interval _16737 = _16736;
    Interval _16740 = _16737;
    Interval _16750 = _16740;
    Interval _16751 = param_var_b_1.z;
    float _16733 = as_type<float>(as_type<uint>(_16751.hi) ^ 2147483648u);
    float _16734 = as_type<float>(as_type<uint>(_16751.lo) ^ 2147483648u);
    Interval _16731;
    _16731.lo = _16733;
    _16731.hi = _16734;
    Interval _16732 = _16731;
    Interval _16735 = _16732;
    Interval _16752 = _16735;
    Interval3 _16729;
    _16729.x = _16748;
    _16729.y = _16750;
    _16729.z = _16752;
    Interval3 _16730 = _16729;
    Interval3 _16753 = _16730;
    Interval _16719 = _16746.x;
    Interval _16720 = _16753.x;
    Interval _17555 = iadd(_16719, _16720, intervalFailed);
    Interval _16721 = _17555;
    Interval _16722 = _16746.y;
    Interval _16723 = _16753.y;
    Interval _17560 = iadd(_16722, _16723, intervalFailed);
    Interval _16724 = _17560;
    Interval _16725 = _16746.z;
    Interval _16726 = _16753.z;
    Interval _17565 = iadd(_16725, _16726, intervalFailed);
    Interval _16727 = _17565;
    Interval3 _16717;
    _16717.x = _16721;
    _16717.y = _16724;
    _16717.z = _16727;
    Interval3 _16718 = _16717;
    Interval3 _16728 = _16718;
    Interval3 _16754 = _16728;
    Interval3 param_var_b_2 = _16754;
    Interval _16695 = param_var_a_2.y;
    Interval _16696 = param_var_b_2.z;
    Interval _17580 = imul(_16695, _16696, intervalFailed, optical_product_upper);
    Interval _16697 = _17580;
    Interval _16698 = param_var_a_2.z;
    Interval _16699 = param_var_b_2.y;
    Interval _17585 = imul(_16698, _16699, intervalFailed, optical_product_upper);
    Interval _16700 = _17585;
    Interval _16691 = _16697;
    Interval _16692 = _16700;
    float _16688 = as_type<float>(as_type<uint>(_16692.hi) ^ 2147483648u);
    float _16689 = as_type<float>(as_type<uint>(_16692.lo) ^ 2147483648u);
    Interval _16686;
    _16686.lo = _16688;
    _16686.hi = _16689;
    Interval _16687 = _16686;
    Interval _16690 = _16687;
    Interval _16693 = _16690;
    Interval _17605 = iadd(_16691, _16693, intervalFailed);
    Interval _16694 = _17605;
    Interval _16701 = _16694;
    Interval _16702 = param_var_a_2.z;
    Interval _16703 = param_var_b_2.x;
    Interval _17611 = imul(_16702, _16703, intervalFailed, optical_product_upper);
    Interval _16704 = _17611;
    Interval _16705 = param_var_a_2.x;
    Interval _16706 = param_var_b_2.z;
    Interval _17616 = imul(_16705, _16706, intervalFailed, optical_product_upper);
    Interval _16707 = _17616;
    Interval _16682 = _16704;
    Interval _16683 = _16707;
    float _16679 = as_type<float>(as_type<uint>(_16683.hi) ^ 2147483648u);
    float _16680 = as_type<float>(as_type<uint>(_16683.lo) ^ 2147483648u);
    Interval _16677;
    _16677.lo = _16679;
    _16677.hi = _16680;
    Interval _16678 = _16677;
    Interval _16681 = _16678;
    Interval _16684 = _16681;
    Interval _17636 = iadd(_16682, _16684, intervalFailed);
    Interval _16685 = _17636;
    Interval _16708 = _16685;
    Interval _16709 = param_var_a_2.x;
    Interval _16710 = param_var_b_2.y;
    Interval _17642 = imul(_16709, _16710, intervalFailed, optical_product_upper);
    Interval _16711 = _17642;
    Interval _16712 = param_var_a_2.y;
    Interval _16713 = param_var_b_2.x;
    Interval _17647 = imul(_16712, _16713, intervalFailed, optical_product_upper);
    Interval _16714 = _17647;
    Interval _16673 = _16711;
    Interval _16674 = _16714;
    float _16670 = as_type<float>(as_type<uint>(_16674.hi) ^ 2147483648u);
    float _16671 = as_type<float>(as_type<uint>(_16674.lo) ^ 2147483648u);
    Interval _16668;
    _16668.lo = _16670;
    _16668.hi = _16671;
    Interval _16669 = _16668;
    Interval _16672 = _16669;
    Interval _16675 = _16672;
    Interval _17667 = iadd(_16673, _16675, intervalFailed);
    Interval _16676 = _17667;
    Interval _16715 = _16676;
    Interval3 _16666;
    _16666.x = _16701;
    _16666.y = _16708;
    _16666.z = _16715;
    Interval3 _16667 = _16666;
    Interval3 _16716 = _16667;
    Interval3 param_var_a_4 = _16716;
    Interval3 _16659 = param_var_a_4;
    float _16660 = 1.0;
    float _16656 = _16660;
    float _16657 = _16660;
    Interval _16654;
    _16654.lo = _16656;
    _16654.hi = _16657;
    Interval _16655 = _16654;
    Interval _16658 = _16655;
    Interval _16661 = _16658;
    Interval3 _16662 = param_var_a_4;
    Interval _16645 = _16662.x;
    bool _16638 = false;
    if (_16645.lo <= 0.0)
    {
        _16638 = _16645.hi >= 0.0;
    }
    float _16637;
    if (_16638)
    {
        _16637 = 0.0;
    }
    else
    {
        _16637 = precise::min(abs(_16645.lo), abs(_16645.hi));
    }
    float _16636 = _16637;
    float _16639 = precise::max(abs(_16645.lo), abs(_16645.hi));
    float _16640 = spvFMul(_16636, _16636);
    float _17720 = interval_down(_16640, intervalFailed);
    float _16641 = precise::max(0.0, _17720);
    float _16642 = spvFMul(_16639, _16639);
    float _17724 = interval_up(_16642, intervalFailed);
    float _16643 = _17724;
    Interval _16634;
    _16634.lo = _16641;
    _16634.hi = _16643;
    Interval _16635 = _16634;
    Interval _16644 = _16635;
    Interval _16646 = _16644;
    Interval _16647 = _16662.y;
    bool _16627 = false;
    if (_16647.lo <= 0.0)
    {
        _16627 = _16647.hi >= 0.0;
    }
    float _16626;
    if (_16627)
    {
        _16626 = 0.0;
    }
    else
    {
        _16626 = precise::min(abs(_16647.lo), abs(_16647.hi));
    }
    float _16625 = _16626;
    float _16628 = precise::max(abs(_16647.lo), abs(_16647.hi));
    float _16629 = spvFMul(_16625, _16625);
    float _17763 = interval_down(_16629, intervalFailed);
    float _16630 = precise::max(0.0, _17763);
    float _16631 = spvFMul(_16628, _16628);
    float _17767 = interval_up(_16631, intervalFailed);
    float _16632 = _17767;
    Interval _16623;
    _16623.lo = _16630;
    _16623.hi = _16632;
    Interval _16624 = _16623;
    Interval _16633 = _16624;
    Interval _16648 = _16633;
    Interval _17775 = iadd(_16646, _16648, intervalFailed);
    Interval _16649 = _17775;
    Interval _16650 = _16662.z;
    bool _16616 = false;
    if (_16650.lo <= 0.0)
    {
        _16616 = _16650.hi >= 0.0;
    }
    float _16615;
    if (_16616)
    {
        _16615 = 0.0;
    }
    else
    {
        _16615 = precise::min(abs(_16650.lo), abs(_16650.hi));
    }
    float _16614 = _16615;
    float _16617 = precise::max(abs(_16650.lo), abs(_16650.hi));
    float _16618 = spvFMul(_16614, _16614);
    float _17807 = interval_down(_16618, intervalFailed);
    float _16619 = precise::max(0.0, _17807);
    float _16620 = spvFMul(_16617, _16617);
    float _17811 = interval_up(_16620, intervalFailed);
    float _16621 = _17811;
    Interval _16612;
    _16612.lo = _16619;
    _16612.hi = _16621;
    Interval _16613 = _16612;
    Interval _16622 = _16613;
    Interval _16651 = _16622;
    Interval _17819 = iadd(_16649, _16651, intervalFailed);
    Interval _16652 = _17819;
    Interval _17820 = isqrt(_16652, intervalFailed);
    Interval _16653 = _17820;
    Interval _16663 = _16653;
    Interval _17822 = idiv(_16661, _16663, intervalFailed, interval_divide_upper);
    Interval _16664 = _17822;
    Interval _16602 = _16659.x;
    Interval _16603 = _16664;
    Interval _17826 = imul(_16602, _16603, intervalFailed, optical_product_upper);
    Interval _16604 = _17826;
    Interval _16605 = _16659.y;
    Interval _16606 = _16664;
    Interval _17830 = imul(_16605, _16606, intervalFailed, optical_product_upper);
    Interval _16607 = _17830;
    Interval _16608 = _16659.z;
    Interval _16609 = _16664;
    Interval _17834 = imul(_16608, _16609, intervalFailed, optical_product_upper);
    Interval _16610 = _17834;
    Interval3 _16600;
    _16600.x = _16604;
    _16600.y = _16607;
    _16600.z = _16610;
    Interval3 _16601 = _16600;
    Interval3 _16611 = _16601;
    Interval3 _16665 = _16611;
    return _16665;
}

static inline __attribute__((always_inline))
Interval3 oriented(thread const Interval3& n, thread const Interval3& direction, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval3 param_var_a = n;
    Interval3 param_var_b = direction;
    Interval _17906 = param_var_a.x;
    Interval _17907 = param_var_b.x;
    Interval _17923 = imul(_17906, _17907, intervalFailed, optical_product_upper);
    Interval _17908 = _17923;
    Interval _17909 = param_var_a.y;
    Interval _17910 = param_var_b.y;
    Interval _17928 = imul(_17909, _17910, intervalFailed, optical_product_upper);
    Interval _17911 = _17928;
    Interval _17929 = iadd(_17908, _17911, intervalFailed);
    Interval _17912 = _17929;
    Interval _17913 = param_var_a.z;
    Interval _17914 = param_var_b.z;
    Interval _17934 = imul(_17913, _17914, intervalFailed, optical_product_upper);
    Interval _17915 = _17934;
    Interval _17935 = iadd(_17912, _17915, intervalFailed);
    Interval _17916 = _17935;
    Interval _dot = _17916;
    if (_dot.lo > 0.0)
    {
        Interval3 param_var_a_1 = n;
        float param_var_x = -1.0;
        float _17903 = param_var_x;
        float _17904 = param_var_x;
        Interval _17901;
        _17901.lo = _17903;
        _17901.hi = _17904;
        Interval _17902 = _17901;
        Interval _17905 = _17902;
        Interval param_var_b_1 = _17905;
        Interval _17891 = param_var_a_1.x;
        Interval _17892 = param_var_b_1;
        Interval _17953 = imul(_17891, _17892, intervalFailed, optical_product_upper);
        Interval _17893 = _17953;
        Interval _17894 = param_var_a_1.y;
        Interval _17895 = param_var_b_1;
        Interval _17957 = imul(_17894, _17895, intervalFailed, optical_product_upper);
        Interval _17896 = _17957;
        Interval _17897 = param_var_a_1.z;
        Interval _17898 = param_var_b_1;
        Interval _17961 = imul(_17897, _17898, intervalFailed, optical_product_upper);
        Interval _17899 = _17961;
        Interval3 _17889;
        _17889.x = _17893;
        _17889.y = _17896;
        _17889.z = _17899;
        Interval3 _17890 = _17889;
        Interval3 _17900 = _17890;
        return _17900;
    }
    if (_dot.hi <= 0.0)
    {
        return n;
    }
    Interval3 param_var_a_2 = n;
    Interval3 param_var_a_3 = n;
    float param_var_x_1 = -1.0;
    float _17886 = param_var_x_1;
    float _17887 = param_var_x_1;
    Interval _17884;
    _17884.lo = _17886;
    _17884.hi = _17887;
    Interval _17885 = _17884;
    Interval _17888 = _17885;
    Interval param_var_b_2 = _17888;
    Interval _17874 = param_var_a_3.x;
    Interval _17875 = param_var_b_2;
    Interval _17989 = imul(_17874, _17875, intervalFailed, optical_product_upper);
    Interval _17876 = _17989;
    Interval _17877 = param_var_a_3.y;
    Interval _17878 = param_var_b_2;
    Interval _17993 = imul(_17877, _17878, intervalFailed, optical_product_upper);
    Interval _17879 = _17993;
    Interval _17880 = param_var_a_3.z;
    Interval _17881 = param_var_b_2;
    Interval _17997 = imul(_17880, _17881, intervalFailed, optical_product_upper);
    Interval _17882 = _17997;
    Interval3 _17872;
    _17872.x = _17876;
    _17872.y = _17879;
    _17872.z = _17882;
    Interval3 _17873 = _17872;
    Interval3 _17883 = _17873;
    Interval3 param_var_b_3 = _17883;
    Interval _17862 = param_var_a_2.x;
    Interval _17863 = param_var_b_3.x;
    float _17859 = precise::min(_17862.lo, _17863.lo);
    float _17860 = precise::max(_17862.hi, _17863.hi);
    Interval _17857;
    _17857.lo = _17859;
    _17857.hi = _17860;
    Interval _17858 = _17857;
    Interval _17861 = _17858;
    Interval _17864 = _17861;
    Interval _17865 = param_var_a_2.y;
    Interval _17866 = param_var_b_3.y;
    float _17854 = precise::min(_17865.lo, _17866.lo);
    float _17855 = precise::max(_17865.hi, _17866.hi);
    Interval _17852;
    _17852.lo = _17854;
    _17852.hi = _17855;
    Interval _17853 = _17852;
    Interval _17856 = _17853;
    Interval _17867 = _17856;
    Interval _17868 = param_var_a_2.z;
    Interval _17869 = param_var_b_3.z;
    float _17849 = precise::min(_17868.lo, _17869.lo);
    float _17850 = precise::max(_17868.hi, _17869.hi);
    Interval _17847;
    _17847.lo = _17849;
    _17847.hi = _17850;
    Interval _17848 = _17847;
    Interval _17851 = _17848;
    Interval _17870 = _17851;
    Interval3 _17845;
    _17845.x = _17864;
    _17845.y = _17867;
    _17845.z = _17870;
    Interval3 _17846 = _17845;
    Interval3 _17871 = _17846;
    return _17871;
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        float param_var_x = n;
        float _18404 = param_var_x;
        float _18405 = param_var_x;
        Interval _18402;
        _18402.lo = _18404;
        _18402.hi = _18405;
        Interval _18403 = _18402;
        Interval _18406 = _18403;
        Interval param_var_a = _18406;
        float param_var_lo = as_type<float>(1036831948u);
        float param_var_hi = as_type<float>(1036831950u);
        Interval _18400;
        _18400.lo = param_var_lo;
        _18400.hi = param_var_hi;
        Interval _18401 = _18400;
        Interval param_var_b = _18401;
        Interval _18427 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _18427;
    }
    if (d == 100.0)
    {
        float param_var_x_1 = n;
        float _18397 = param_var_x_1;
        float _18398 = param_var_x_1;
        Interval _18395;
        _18395.lo = _18397;
        _18395.hi = _18398;
        Interval _18396 = _18395;
        Interval _18399 = _18396;
        Interval param_var_a_1 = _18399;
        float param_var_lo_1 = as_type<float>(1008981769u);
        float param_var_hi_1 = as_type<float>(1008981771u);
        Interval _18393;
        _18393.lo = param_var_lo_1;
        _18393.hi = param_var_hi_1;
        Interval _18394 = _18393;
        Interval param_var_b_1 = _18394;
        Interval _18448 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _18448;
    }
    if (d == 1000.0)
    {
        float param_var_x_2 = n;
        float _18390 = param_var_x_2;
        float _18391 = param_var_x_2;
        Interval _18388;
        _18388.lo = _18390;
        _18388.hi = _18391;
        Interval _18389 = _18388;
        Interval _18392 = _18389;
        Interval param_var_a_2 = _18392;
        float param_var_lo_2 = as_type<float>(981668462u);
        float param_var_hi_2 = as_type<float>(981668464u);
        Interval _18386;
        _18386.lo = param_var_lo_2;
        _18386.hi = param_var_hi_2;
        Interval _18387 = _18386;
        Interval param_var_b_2 = _18387;
        Interval _18469 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _18469;
    }
    if (d == 10000.0)
    {
        float param_var_x_3 = n;
        float _18383 = param_var_x_3;
        float _18384 = param_var_x_3;
        Interval _18381;
        _18381.lo = _18383;
        _18381.hi = _18384;
        Interval _18382 = _18381;
        Interval _18385 = _18382;
        Interval param_var_a_3 = _18385;
        float param_var_lo_3 = as_type<float>(953267990u);
        float param_var_hi_3 = as_type<float>(953267992u);
        Interval _18379;
        _18379.lo = param_var_lo_3;
        _18379.hi = param_var_hi_3;
        Interval _18380 = _18379;
        Interval param_var_b_3 = _18380;
        Interval _18490 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _18490;
    }
    if (d == 100000.0)
    {
        float param_var_x_4 = n;
        float _18376 = param_var_x_4;
        float _18377 = param_var_x_4;
        Interval _18374;
        _18374.lo = _18376;
        _18374.hi = _18377;
        Interval _18375 = _18374;
        Interval _18378 = _18375;
        Interval param_var_a_4 = _18378;
        float param_var_lo_4 = as_type<float>(925353387u);
        float param_var_hi_4 = as_type<float>(925353389u);
        Interval _18372;
        _18372.lo = param_var_lo_4;
        _18372.hi = param_var_hi_4;
        Interval _18373 = _18372;
        Interval param_var_b_4 = _18373;
        Interval _18511 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _18511;
    }
    if (d == 128.0)
    {
        float param_var_x_5 = n;
        float _18369 = param_var_x_5;
        float _18370 = param_var_x_5;
        Interval _18367;
        _18367.lo = _18369;
        _18367.hi = _18370;
        Interval _18368 = _18367;
        Interval _18371 = _18368;
        Interval param_var_a_5 = _18371;
        float param_var_lo_5 = as_type<float>(1006632960u);
        float param_var_hi_5 = as_type<float>(1006632960u);
        Interval _18365;
        _18365.lo = param_var_lo_5;
        _18365.hi = param_var_hi_5;
        Interval _18366 = _18365;
        Interval param_var_b_5 = _18366;
        Interval _18532 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _18532;
    }
    if (d == 65535.0)
    {
        float param_var_x_6 = n;
        float _18362 = param_var_x_6;
        float _18363 = param_var_x_6;
        Interval _18360;
        _18360.lo = _18362;
        _18360.hi = _18363;
        Interval _18361 = _18360;
        Interval _18364 = _18361;
        Interval param_var_a_6 = _18364;
        float param_var_lo_6 = as_type<float>(931135615u);
        float param_var_hi_6 = as_type<float>(931135617u);
        Interval _18358;
        _18358.lo = param_var_lo_6;
        _18358.hi = param_var_hi_6;
        Interval _18359 = _18358;
        Interval param_var_b_6 = _18359;
        Interval _18553 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _18553;
    }
    float param_var_x_7 = n;
    float _18355 = param_var_x_7;
    float _18356 = param_var_x_7;
    Interval _18353;
    _18353.lo = _18355;
    _18353.hi = _18356;
    Interval _18354 = _18353;
    Interval _18357 = _18354;
    Interval param_var_a_7 = _18357;
    float param_var_x_8 = d;
    float _18350 = param_var_x_8;
    float _18351 = param_var_x_8;
    Interval _18348;
    _18348.lo = _18350;
    _18348.hi = _18351;
    Interval _18349 = _18348;
    Interval _18352 = _18349;
    Interval param_var_b_7 = _18352;
    Interval _18574 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _18574;
}

static inline __attribute__((always_inline))
Interval isin_body(thread const Interval& a, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool temp_var_logical = true;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        temp_var_logical = isnan(a.hi) || isinf(a.hi);
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        temp_var_logical_1 = precise::max(abs(a.lo), abs(a.hi)) > 1048576.0;
    }
    if (temp_var_logical_1)
    {
        intervalFailed = true;
        float param_var_lo = -1.0;
        float param_var_hi = 1.0;
        Interval _20417;
        _20417.lo = param_var_lo;
        _20417.hi = param_var_hi;
        Interval _20418 = _20417;
        return _20418;
    }
    float n = floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5));
    Interval param_var_a = a;
    float param_var_x = spvFMul(n, 2.0);
    float _20414 = param_var_x;
    float _20415 = param_var_x;
    Interval _20412;
    _20412.lo = _20414;
    _20412.hi = _20415;
    Interval _20413 = _20412;
    Interval _20416 = _20413;
    Interval param_var_a_1 = _20416;
    float _20410 = 3.1415927410125732421875;
    float _20405 = _20410;
    float _20464 = interval_down(_20405, intervalFailed);
    float _20406 = _20464;
    float _20407 = _20410;
    float _20466 = interval_up(_20407, intervalFailed);
    float _20408 = _20466;
    Interval _20403;
    _20403.lo = _20406;
    _20403.hi = _20408;
    Interval _20404 = _20403;
    Interval _20409 = _20404;
    Interval _20411 = _20409;
    Interval param_var_b = _20411;
    Interval _20475 = imul(param_var_a_1, param_var_b, intervalFailed, optical_product_upper);
    Interval param_var_b_1 = _20475;
    Interval _20399 = param_var_a;
    Interval _20400 = param_var_b_1;
    float _20396 = as_type<float>(as_type<uint>(_20400.hi) ^ 2147483648u);
    float _20397 = as_type<float>(as_type<uint>(_20400.lo) ^ 2147483648u);
    Interval _20394;
    _20394.lo = _20396;
    _20394.hi = _20397;
    Interval _20395 = _20394;
    Interval _20398 = _20395;
    Interval _20401 = _20398;
    Interval _20495 = iadd(_20399, _20401, intervalFailed);
    Interval _20402 = _20495;
    Interval x = _20402;
    float _20392 = 3.1415927410125732421875;
    float _20387 = _20392;
    float _20498 = interval_down(_20387, intervalFailed);
    float _20388 = _20498;
    float _20389 = _20392;
    float _20500 = interval_up(_20389, intervalFailed);
    float _20390 = _20500;
    Interval _20385;
    _20385.lo = _20388;
    _20385.hi = _20390;
    Interval _20386 = _20385;
    Interval _20391 = _20386;
    Interval _20393 = _20391;
    Interval param_var_a_2 = _20393;
    float param_var_x_1 = 0.5;
    float _20382 = param_var_x_1;
    float _20383 = param_var_x_1;
    Interval _20380;
    _20380.lo = _20382;
    _20380.hi = _20383;
    Interval _20381 = _20380;
    Interval _20384 = _20381;
    Interval param_var_b_2 = _20384;
    Interval _20518 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _half = _20518;
    if (x.lo > _half.hi)
    {
        float _20378 = 3.1415927410125732421875;
        float _20373 = _20378;
        float _20525 = interval_down(_20373, intervalFailed);
        float _20374 = _20525;
        float _20375 = _20378;
        float _20527 = interval_up(_20375, intervalFailed);
        float _20376 = _20527;
        Interval _20371;
        _20371.lo = _20374;
        _20371.hi = _20376;
        Interval _20372 = _20371;
        Interval _20377 = _20372;
        Interval _20379 = _20377;
        Interval param_var_a_3 = _20379;
        Interval param_var_b_3 = x;
        Interval _20367 = param_var_a_3;
        Interval _20368 = param_var_b_3;
        float _20364 = as_type<float>(as_type<uint>(_20368.hi) ^ 2147483648u);
        float _20365 = as_type<float>(as_type<uint>(_20368.lo) ^ 2147483648u);
        Interval _20362;
        _20362.lo = _20364;
        _20362.hi = _20365;
        Interval _20363 = _20362;
        Interval _20366 = _20363;
        Interval _20369 = _20366;
        Interval _20556 = iadd(_20367, _20369, intervalFailed);
        Interval _20370 = _20556;
        x = _20370;
    }
    else
    {
        if (x.hi < (-_half.hi))
        {
            float _20360 = 3.1415927410125732421875;
            float _20355 = _20360;
            float _20564 = interval_down(_20355, intervalFailed);
            float _20356 = _20564;
            float _20357 = _20360;
            float _20566 = interval_up(_20357, intervalFailed);
            float _20358 = _20566;
            Interval _20353;
            _20353.lo = _20356;
            _20353.hi = _20358;
            Interval _20354 = _20353;
            Interval _20359 = _20354;
            Interval _20361 = _20359;
            Interval param_var_a_4 = _20361;
            float _20350 = as_type<float>(as_type<uint>(param_var_a_4.hi) ^ 2147483648u);
            float _20351 = as_type<float>(as_type<uint>(param_var_a_4.lo) ^ 2147483648u);
            Interval _20348;
            _20348.lo = _20350;
            _20348.hi = _20351;
            Interval _20349 = _20348;
            Interval _20352 = _20349;
            Interval param_var_a_5 = _20352;
            Interval param_var_b_4 = x;
            Interval _20344 = param_var_a_5;
            Interval _20345 = param_var_b_4;
            float _20341 = as_type<float>(as_type<uint>(_20345.hi) ^ 2147483648u);
            float _20342 = as_type<float>(as_type<uint>(_20345.lo) ^ 2147483648u);
            Interval _20339;
            _20339.lo = _20341;
            _20339.hi = _20342;
            Interval _20340 = _20339;
            Interval _20343 = _20340;
            Interval _20346 = _20343;
            Interval _20612 = iadd(_20344, _20346, intervalFailed);
            Interval _20347 = _20612;
            x = _20347;
        }
    }
    float _1804 = -_half.hi;
    bool temp_var_logical_2 = true;
    if ((isunordered(x.lo, _1804) || x.lo >= _1804))
    {
        temp_var_logical_2 = x.hi > _half.hi;
    }
    if (temp_var_logical_2)
    {
        float param_var_lo_1 = -1.0;
        float param_var_hi_1 = 1.0;
        Interval _20337;
        _20337.lo = param_var_lo_1;
        _20337.hi = param_var_hi_1;
        Interval _20338 = _20337;
        return _20338;
    }
    Interval param_var_a_6 = x;
    bool _20330 = false;
    if (param_var_a_6.lo <= 0.0)
    {
        _20330 = param_var_a_6.hi >= 0.0;
    }
    float _20329;
    if (_20330)
    {
        _20329 = 0.0;
    }
    else
    {
        _20329 = precise::min(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    }
    float _20328 = _20329;
    float _20331 = precise::max(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    float _20332 = spvFMul(_20328, _20328);
    float _20661 = interval_down(_20332, intervalFailed);
    float _20333 = precise::max(0.0, _20661);
    float _20334 = spvFMul(_20331, _20331);
    float _20665 = interval_up(_20334, intervalFailed);
    float _20335 = _20665;
    Interval _20326;
    _20326.lo = _20333;
    _20326.hi = _20335;
    Interval _20327 = _20326;
    Interval _20336 = _20327;
    Interval square = _20336;
    float param_var_x_2 = _2254[8];
    float _20321 = param_var_x_2;
    float _20676 = interval_down(_20321, intervalFailed);
    float _20322 = _20676;
    float _20323 = param_var_x_2;
    float _20678 = interval_up(_20323, intervalFailed);
    float _20324 = _20678;
    Interval _20319;
    _20319.lo = _20322;
    _20319.hi = _20324;
    Interval _20320 = _20319;
    Interval _20325 = _20320;
    Interval sum = _20325;
    Interval _20312;
    for (int k = 7; k >= 0; k--)
    {
        Interval param_var_a_7 = sum;
        Interval param_var_b_5 = square;
        Interval _20690 = imul(param_var_a_7, param_var_b_5, intervalFailed, optical_product_upper);
        Interval param_var_a_8 = _20690;
        float param_var_x_3 = _2254[k];
        float _20314 = param_var_x_3;
        float _20695 = interval_down(_20314, intervalFailed);
        float _20315 = _20695;
        float _20316 = param_var_x_3;
        float _20697 = interval_up(_20316, intervalFailed);
        float _20317 = _20697;
        _20312.lo = _20315;
        _20312.hi = _20317;
        Interval _20313 = _20312;
        Interval _20318 = _20313;
        Interval param_var_b_6 = _20318;
        Interval _20705 = iadd(param_var_a_8, param_var_b_6, intervalFailed);
        sum = _20705;
    }
    Interval param_var_a_9 = x;
    Interval param_var_b_7 = sum;
    Interval _20709 = imul(param_var_a_9, param_var_b_7, intervalFailed, optical_product_upper);
    Interval param_var_a_10 = _20709;
    float param_var_lo_2 = -3.9999999840167888010000751819462e-12;
    float param_var_hi_2 = 3.9999999840167888010000751819462e-12;
    Interval _20310;
    _20310.lo = param_var_lo_2;
    _20310.hi = param_var_hi_2;
    Interval _20311 = _20310;
    Interval param_var_b_8 = _20311;
    Interval _20716 = iadd(param_var_a_10, param_var_b_8, intervalFailed);
    Interval param_var_a_11 = _20716;
    float param_var_x_4 = -1.0;
    float _20307 = param_var_x_4;
    float _20308 = param_var_x_4;
    Interval _20305;
    _20305.lo = _20307;
    _20305.hi = _20308;
    Interval _20306 = _20305;
    Interval _20309 = _20306;
    Interval param_var_lo_3 = _20309;
    float param_var_x_5 = 1.0;
    float _20302 = param_var_x_5;
    float _20303 = param_var_x_5;
    Interval _20300;
    _20300.lo = _20302;
    _20300.hi = _20303;
    Interval _20301 = _20300;
    Interval _20304 = _20301;
    Interval param_var_hi_3 = _20304;
    Interval _20295 = param_var_a_11;
    Interval _20296 = param_var_lo_3;
    float _20292 = precise::max(_20295.lo, _20296.lo);
    float _20293 = precise::max(_20295.hi, _20296.hi);
    Interval _20290;
    _20290.lo = _20292;
    _20290.hi = _20293;
    Interval _20291 = _20290;
    Interval _20294 = _20291;
    Interval _20297 = _20294;
    Interval _20298 = param_var_hi_3;
    float _20287 = precise::min(_20297.lo, _20298.lo);
    float _20288 = precise::min(_20297.hi, _20298.hi);
    Interval _20285;
    _20285.lo = _20287;
    _20285.hi = _20288;
    Interval _20286 = _20285;
    Interval _20289 = _20286;
    Interval _20299 = _20289;
    return _20299;
}

static inline __attribute__((always_inline))
bool outside_face(thread const ReflectionSpecularPlane& p, thread const Interval3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = p;
    bool _18923 = false;
    if (param_var_p.a.w == 2.0)
    {
        _18923 = param_var_p.b.w == 2.0;
    }
    bool _18924 = false;
    if (_18923)
    {
        _18924 = param_var_p.c.w == 2.0;
    }
    bool _18925 = _18924;
    if (_18925)
    {
        return false;
    }
    float3 param_var_v = p.b.xyz;
    float _18916 = param_var_v.x;
    float _18913 = _18916;
    float _18914 = _18916;
    Interval _18911;
    _18911.lo = _18913;
    _18911.hi = _18914;
    Interval _18912 = _18911;
    Interval _18915 = _18912;
    Interval _18917 = _18915;
    float _18918 = param_var_v.y;
    float _18908 = _18918;
    float _18909 = _18918;
    Interval _18906;
    _18906.lo = _18908;
    _18906.hi = _18909;
    Interval _18907 = _18906;
    Interval _18910 = _18907;
    Interval _18919 = _18910;
    float _18920 = param_var_v.z;
    float _18903 = _18920;
    float _18904 = _18920;
    Interval _18901;
    _18901.lo = _18903;
    _18901.hi = _18904;
    Interval _18902 = _18901;
    Interval _18905 = _18902;
    Interval _18921 = _18905;
    Interval3 _18899;
    _18899.x = _18917;
    _18899.y = _18919;
    _18899.z = _18921;
    Interval3 _18900 = _18899;
    Interval3 _18922 = _18900;
    Interval3 param_var_a = _18922;
    float3 param_var_v_1 = p.a.xyz;
    float _18892 = param_var_v_1.x;
    float _18889 = _18892;
    float _18890 = _18892;
    Interval _18887;
    _18887.lo = _18889;
    _18887.hi = _18890;
    Interval _18888 = _18887;
    Interval _18891 = _18888;
    Interval _18893 = _18891;
    float _18894 = param_var_v_1.y;
    float _18884 = _18894;
    float _18885 = _18894;
    Interval _18882;
    _18882.lo = _18884;
    _18882.hi = _18885;
    Interval _18883 = _18882;
    Interval _18886 = _18883;
    Interval _18895 = _18886;
    float _18896 = param_var_v_1.z;
    float _18879 = _18896;
    float _18880 = _18896;
    Interval _18877;
    _18877.lo = _18879;
    _18877.hi = _18880;
    Interval _18878 = _18877;
    Interval _18881 = _18878;
    Interval _18897 = _18881;
    Interval3 _18875;
    _18875.x = _18893;
    _18875.y = _18895;
    _18875.z = _18897;
    Interval3 _18876 = _18875;
    Interval3 _18898 = _18876;
    Interval3 param_var_b = _18898;
    Interval3 _18866 = param_var_a;
    Interval _18867 = param_var_b.x;
    float _18863 = as_type<float>(as_type<uint>(_18867.hi) ^ 2147483648u);
    float _18864 = as_type<float>(as_type<uint>(_18867.lo) ^ 2147483648u);
    Interval _18861;
    _18861.lo = _18863;
    _18861.hi = _18864;
    Interval _18862 = _18861;
    Interval _18865 = _18862;
    Interval _18868 = _18865;
    Interval _18869 = param_var_b.y;
    float _18858 = as_type<float>(as_type<uint>(_18869.hi) ^ 2147483648u);
    float _18859 = as_type<float>(as_type<uint>(_18869.lo) ^ 2147483648u);
    Interval _18856;
    _18856.lo = _18858;
    _18856.hi = _18859;
    Interval _18857 = _18856;
    Interval _18860 = _18857;
    Interval _18870 = _18860;
    Interval _18871 = param_var_b.z;
    float _18853 = as_type<float>(as_type<uint>(_18871.hi) ^ 2147483648u);
    float _18854 = as_type<float>(as_type<uint>(_18871.lo) ^ 2147483648u);
    Interval _18851;
    _18851.lo = _18853;
    _18851.hi = _18854;
    Interval _18852 = _18851;
    Interval _18855 = _18852;
    Interval _18872 = _18855;
    Interval3 _18849;
    _18849.x = _18868;
    _18849.y = _18870;
    _18849.z = _18872;
    Interval3 _18850 = _18849;
    Interval3 _18873 = _18850;
    Interval _18839 = _18866.x;
    Interval _18840 = _18873.x;
    Interval _19106 = iadd(_18839, _18840, intervalFailed);
    Interval _18841 = _19106;
    Interval _18842 = _18866.y;
    Interval _18843 = _18873.y;
    Interval _19111 = iadd(_18842, _18843, intervalFailed);
    Interval _18844 = _19111;
    Interval _18845 = _18866.z;
    Interval _18846 = _18873.z;
    Interval _19116 = iadd(_18845, _18846, intervalFailed);
    Interval _18847 = _19116;
    Interval3 _18837;
    _18837.x = _18841;
    _18837.y = _18844;
    _18837.z = _18847;
    Interval3 _18838 = _18837;
    Interval3 _18848 = _18838;
    Interval3 _18874 = _18848;
    Interval3 u = _18874;
    float3 param_var_v_2 = p.c.xyz;
    float _18830 = param_var_v_2.x;
    float _18827 = _18830;
    float _18828 = _18830;
    Interval _18825;
    _18825.lo = _18827;
    _18825.hi = _18828;
    Interval _18826 = _18825;
    Interval _18829 = _18826;
    Interval _18831 = _18829;
    float _18832 = param_var_v_2.y;
    float _18822 = _18832;
    float _18823 = _18832;
    Interval _18820;
    _18820.lo = _18822;
    _18820.hi = _18823;
    Interval _18821 = _18820;
    Interval _18824 = _18821;
    Interval _18833 = _18824;
    float _18834 = param_var_v_2.z;
    float _18817 = _18834;
    float _18818 = _18834;
    Interval _18815;
    _18815.lo = _18817;
    _18815.hi = _18818;
    Interval _18816 = _18815;
    Interval _18819 = _18816;
    Interval _18835 = _18819;
    Interval3 _18813;
    _18813.x = _18831;
    _18813.y = _18833;
    _18813.z = _18835;
    Interval3 _18814 = _18813;
    Interval3 _18836 = _18814;
    Interval3 param_var_a_1 = _18836;
    float3 param_var_v_3 = p.a.xyz;
    float _18806 = param_var_v_3.x;
    float _18803 = _18806;
    float _18804 = _18806;
    Interval _18801;
    _18801.lo = _18803;
    _18801.hi = _18804;
    Interval _18802 = _18801;
    Interval _18805 = _18802;
    Interval _18807 = _18805;
    float _18808 = param_var_v_3.y;
    float _18798 = _18808;
    float _18799 = _18808;
    Interval _18796;
    _18796.lo = _18798;
    _18796.hi = _18799;
    Interval _18797 = _18796;
    Interval _18800 = _18797;
    Interval _18809 = _18800;
    float _18810 = param_var_v_3.z;
    float _18793 = _18810;
    float _18794 = _18810;
    Interval _18791;
    _18791.lo = _18793;
    _18791.hi = _18794;
    Interval _18792 = _18791;
    Interval _18795 = _18792;
    Interval _18811 = _18795;
    Interval3 _18789;
    _18789.x = _18807;
    _18789.y = _18809;
    _18789.z = _18811;
    Interval3 _18790 = _18789;
    Interval3 _18812 = _18790;
    Interval3 param_var_b_1 = _18812;
    Interval3 _18780 = param_var_a_1;
    Interval _18781 = param_var_b_1.x;
    float _18777 = as_type<float>(as_type<uint>(_18781.hi) ^ 2147483648u);
    float _18778 = as_type<float>(as_type<uint>(_18781.lo) ^ 2147483648u);
    Interval _18775;
    _18775.lo = _18777;
    _18775.hi = _18778;
    Interval _18776 = _18775;
    Interval _18779 = _18776;
    Interval _18782 = _18779;
    Interval _18783 = param_var_b_1.y;
    float _18772 = as_type<float>(as_type<uint>(_18783.hi) ^ 2147483648u);
    float _18773 = as_type<float>(as_type<uint>(_18783.lo) ^ 2147483648u);
    Interval _18770;
    _18770.lo = _18772;
    _18770.hi = _18773;
    Interval _18771 = _18770;
    Interval _18774 = _18771;
    Interval _18784 = _18774;
    Interval _18785 = param_var_b_1.z;
    float _18767 = as_type<float>(as_type<uint>(_18785.hi) ^ 2147483648u);
    float _18768 = as_type<float>(as_type<uint>(_18785.lo) ^ 2147483648u);
    Interval _18765;
    _18765.lo = _18767;
    _18765.hi = _18768;
    Interval _18766 = _18765;
    Interval _18769 = _18766;
    Interval _18786 = _18769;
    Interval3 _18763;
    _18763.x = _18782;
    _18763.y = _18784;
    _18763.z = _18786;
    Interval3 _18764 = _18763;
    Interval3 _18787 = _18764;
    Interval _18753 = _18780.x;
    Interval _18754 = _18787.x;
    Interval _19287 = iadd(_18753, _18754, intervalFailed);
    Interval _18755 = _19287;
    Interval _18756 = _18780.y;
    Interval _18757 = _18787.y;
    Interval _19292 = iadd(_18756, _18757, intervalFailed);
    Interval _18758 = _19292;
    Interval _18759 = _18780.z;
    Interval _18760 = _18787.z;
    Interval _19297 = iadd(_18759, _18760, intervalFailed);
    Interval _18761 = _19297;
    Interval3 _18751;
    _18751.x = _18755;
    _18751.y = _18758;
    _18751.z = _18761;
    Interval3 _18752 = _18751;
    Interval3 _18762 = _18752;
    Interval3 _18788 = _18762;
    Interval3 v = _18788;
    Interval3 param_var_a_2 = hit;
    float3 param_var_v_4 = p.a.xyz;
    float _18744 = param_var_v_4.x;
    float _18741 = _18744;
    float _18742 = _18744;
    Interval _18739;
    _18739.lo = _18741;
    _18739.hi = _18742;
    Interval _18740 = _18739;
    Interval _18743 = _18740;
    Interval _18745 = _18743;
    float _18746 = param_var_v_4.y;
    float _18736 = _18746;
    float _18737 = _18746;
    Interval _18734;
    _18734.lo = _18736;
    _18734.hi = _18737;
    Interval _18735 = _18734;
    Interval _18738 = _18735;
    Interval _18747 = _18738;
    float _18748 = param_var_v_4.z;
    float _18731 = _18748;
    float _18732 = _18748;
    Interval _18729;
    _18729.lo = _18731;
    _18729.hi = _18732;
    Interval _18730 = _18729;
    Interval _18733 = _18730;
    Interval _18749 = _18733;
    Interval3 _18727;
    _18727.x = _18745;
    _18727.y = _18747;
    _18727.z = _18749;
    Interval3 _18728 = _18727;
    Interval3 _18750 = _18728;
    Interval3 param_var_b_2 = _18750;
    Interval3 _18718 = param_var_a_2;
    Interval _18719 = param_var_b_2.x;
    float _18715 = as_type<float>(as_type<uint>(_18719.hi) ^ 2147483648u);
    float _18716 = as_type<float>(as_type<uint>(_18719.lo) ^ 2147483648u);
    Interval _18713;
    _18713.lo = _18715;
    _18713.hi = _18716;
    Interval _18714 = _18713;
    Interval _18717 = _18714;
    Interval _18720 = _18717;
    Interval _18721 = param_var_b_2.y;
    float _18710 = as_type<float>(as_type<uint>(_18721.hi) ^ 2147483648u);
    float _18711 = as_type<float>(as_type<uint>(_18721.lo) ^ 2147483648u);
    Interval _18708;
    _18708.lo = _18710;
    _18708.hi = _18711;
    Interval _18709 = _18708;
    Interval _18712 = _18709;
    Interval _18722 = _18712;
    Interval _18723 = param_var_b_2.z;
    float _18705 = as_type<float>(as_type<uint>(_18723.hi) ^ 2147483648u);
    float _18706 = as_type<float>(as_type<uint>(_18723.lo) ^ 2147483648u);
    Interval _18703;
    _18703.lo = _18705;
    _18703.hi = _18706;
    Interval _18704 = _18703;
    Interval _18707 = _18704;
    Interval _18724 = _18707;
    Interval3 _18701;
    _18701.x = _18720;
    _18701.y = _18722;
    _18701.z = _18724;
    Interval3 _18702 = _18701;
    Interval3 _18725 = _18702;
    Interval _18691 = _18718.x;
    Interval _18692 = _18725.x;
    Interval _19424 = iadd(_18691, _18692, intervalFailed);
    Interval _18693 = _19424;
    Interval _18694 = _18718.y;
    Interval _18695 = _18725.y;
    Interval _19429 = iadd(_18694, _18695, intervalFailed);
    Interval _18696 = _19429;
    Interval _18697 = _18718.z;
    Interval _18698 = _18725.z;
    Interval _19434 = iadd(_18697, _18698, intervalFailed);
    Interval _18699 = _19434;
    Interval3 _18689;
    _18689.x = _18693;
    _18689.y = _18696;
    _18689.z = _18699;
    Interval3 _18690 = _18689;
    Interval3 _18700 = _18690;
    Interval3 _18726 = _18700;
    Interval3 r = _18726;
    Interval3 param_var_a_3 = u;
    Interval3 param_var_b_3 = u;
    Interval _18678 = param_var_a_3.x;
    Interval _18679 = param_var_b_3.x;
    Interval _19451 = imul(_18678, _18679, intervalFailed, optical_product_upper);
    Interval _18680 = _19451;
    Interval _18681 = param_var_a_3.y;
    Interval _18682 = param_var_b_3.y;
    Interval _19456 = imul(_18681, _18682, intervalFailed, optical_product_upper);
    Interval _18683 = _19456;
    Interval _19457 = iadd(_18680, _18683, intervalFailed);
    Interval _18684 = _19457;
    Interval _18685 = param_var_a_3.z;
    Interval _18686 = param_var_b_3.z;
    Interval _19462 = imul(_18685, _18686, intervalFailed, optical_product_upper);
    Interval _18687 = _19462;
    Interval _19463 = iadd(_18684, _18687, intervalFailed);
    Interval _18688 = _19463;
    Interval aa = _18688;
    Interval3 param_var_a_4 = u;
    Interval3 param_var_b_4 = v;
    Interval _18667 = param_var_a_4.x;
    Interval _18668 = param_var_b_4.x;
    Interval _19471 = imul(_18667, _18668, intervalFailed, optical_product_upper);
    Interval _18669 = _19471;
    Interval _18670 = param_var_a_4.y;
    Interval _18671 = param_var_b_4.y;
    Interval _19476 = imul(_18670, _18671, intervalFailed, optical_product_upper);
    Interval _18672 = _19476;
    Interval _19477 = iadd(_18669, _18672, intervalFailed);
    Interval _18673 = _19477;
    Interval _18674 = param_var_a_4.z;
    Interval _18675 = param_var_b_4.z;
    Interval _19482 = imul(_18674, _18675, intervalFailed, optical_product_upper);
    Interval _18676 = _19482;
    Interval _19483 = iadd(_18673, _18676, intervalFailed);
    Interval _18677 = _19483;
    Interval ab = _18677;
    Interval3 param_var_a_5 = v;
    Interval3 param_var_b_5 = v;
    Interval _18656 = param_var_a_5.x;
    Interval _18657 = param_var_b_5.x;
    Interval _19491 = imul(_18656, _18657, intervalFailed, optical_product_upper);
    Interval _18658 = _19491;
    Interval _18659 = param_var_a_5.y;
    Interval _18660 = param_var_b_5.y;
    Interval _19496 = imul(_18659, _18660, intervalFailed, optical_product_upper);
    Interval _18661 = _19496;
    Interval _19497 = iadd(_18658, _18661, intervalFailed);
    Interval _18662 = _19497;
    Interval _18663 = param_var_a_5.z;
    Interval _18664 = param_var_b_5.z;
    Interval _19502 = imul(_18663, _18664, intervalFailed, optical_product_upper);
    Interval _18665 = _19502;
    Interval _19503 = iadd(_18662, _18665, intervalFailed);
    Interval _18666 = _19503;
    Interval bb = _18666;
    Interval3 param_var_a_6 = r;
    Interval3 param_var_b_6 = u;
    Interval _18645 = param_var_a_6.x;
    Interval _18646 = param_var_b_6.x;
    Interval _19511 = imul(_18645, _18646, intervalFailed, optical_product_upper);
    Interval _18647 = _19511;
    Interval _18648 = param_var_a_6.y;
    Interval _18649 = param_var_b_6.y;
    Interval _19516 = imul(_18648, _18649, intervalFailed, optical_product_upper);
    Interval _18650 = _19516;
    Interval _19517 = iadd(_18647, _18650, intervalFailed);
    Interval _18651 = _19517;
    Interval _18652 = param_var_a_6.z;
    Interval _18653 = param_var_b_6.z;
    Interval _19522 = imul(_18652, _18653, intervalFailed, optical_product_upper);
    Interval _18654 = _19522;
    Interval _19523 = iadd(_18651, _18654, intervalFailed);
    Interval _18655 = _19523;
    Interval ra = _18655;
    Interval3 param_var_a_7 = r;
    Interval3 param_var_b_7 = v;
    Interval _18634 = param_var_a_7.x;
    Interval _18635 = param_var_b_7.x;
    Interval _19531 = imul(_18634, _18635, intervalFailed, optical_product_upper);
    Interval _18636 = _19531;
    Interval _18637 = param_var_a_7.y;
    Interval _18638 = param_var_b_7.y;
    Interval _19536 = imul(_18637, _18638, intervalFailed, optical_product_upper);
    Interval _18639 = _19536;
    Interval _19537 = iadd(_18636, _18639, intervalFailed);
    Interval _18640 = _19537;
    Interval _18641 = param_var_a_7.z;
    Interval _18642 = param_var_b_7.z;
    Interval _19542 = imul(_18641, _18642, intervalFailed, optical_product_upper);
    Interval _18643 = _19542;
    Interval _19543 = iadd(_18640, _18643, intervalFailed);
    Interval _18644 = _19543;
    Interval rb = _18644;
    Interval param_var_a_8 = aa;
    Interval param_var_b_8 = bb;
    Interval _19547 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _19547;
    Interval param_var_a_10 = ab;
    bool _18627 = false;
    if (param_var_a_10.lo <= 0.0)
    {
        _18627 = param_var_a_10.hi >= 0.0;
    }
    float _18626;
    if (_18627)
    {
        _18626 = 0.0;
    }
    else
    {
        _18626 = precise::min(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    }
    float _18625 = _18626;
    float _18628 = precise::max(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    float _18629 = spvFMul(_18625, _18625);
    float _19578 = interval_down(_18629, intervalFailed);
    float _18630 = precise::max(0.0, _19578);
    float _18631 = spvFMul(_18628, _18628);
    float _19582 = interval_up(_18631, intervalFailed);
    float _18632 = _19582;
    Interval _18623;
    _18623.lo = _18630;
    _18623.hi = _18632;
    Interval _18624 = _18623;
    Interval _18633 = _18624;
    Interval param_var_b_9 = _18633;
    Interval _18619 = param_var_a_9;
    Interval _18620 = param_var_b_9;
    float _18616 = as_type<float>(as_type<uint>(_18620.hi) ^ 2147483648u);
    float _18617 = as_type<float>(as_type<uint>(_18620.lo) ^ 2147483648u);
    Interval _18614;
    _18614.lo = _18616;
    _18614.hi = _18617;
    Interval _18615 = _18614;
    Interval _18618 = _18615;
    Interval _18621 = _18618;
    Interval _19609 = iadd(_18619, _18621, intervalFailed);
    Interval _18622 = _19609;
    Interval det = _18622;
    if (det.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_11 = bb;
    Interval param_var_b_10 = ra;
    Interval _19616 = imul(param_var_a_11, param_var_b_10, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _19616;
    Interval param_var_a_13 = ab;
    Interval param_var_b_11 = rb;
    Interval _19619 = imul(param_var_a_13, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_b_12 = _19619;
    Interval _18610 = param_var_a_12;
    Interval _18611 = param_var_b_12;
    float _18607 = as_type<float>(as_type<uint>(_18611.hi) ^ 2147483648u);
    float _18608 = as_type<float>(as_type<uint>(_18611.lo) ^ 2147483648u);
    Interval _18605;
    _18605.lo = _18607;
    _18605.hi = _18608;
    Interval _18606 = _18605;
    Interval _18609 = _18606;
    Interval _18612 = _18609;
    Interval _19639 = iadd(_18610, _18612, intervalFailed);
    Interval _18613 = _19639;
    Interval param_var_a_14 = _18613;
    Interval param_var_b_13 = det;
    Interval _19642 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
    Interval x = _19642;
    Interval param_var_a_15 = aa;
    Interval param_var_b_14 = rb;
    Interval _19645 = imul(param_var_a_15, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_16 = _19645;
    Interval param_var_a_17 = ab;
    Interval param_var_b_15 = ra;
    Interval _19648 = imul(param_var_a_17, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _19648;
    Interval _18601 = param_var_a_16;
    Interval _18602 = param_var_b_16;
    float _18598 = as_type<float>(as_type<uint>(_18602.hi) ^ 2147483648u);
    float _18599 = as_type<float>(as_type<uint>(_18602.lo) ^ 2147483648u);
    Interval _18596;
    _18596.lo = _18598;
    _18596.hi = _18599;
    Interval _18597 = _18596;
    Interval _18600 = _18597;
    Interval _18603 = _18600;
    Interval _19668 = iadd(_18601, _18603, intervalFailed);
    Interval _18604 = _19668;
    Interval param_var_a_18 = _18604;
    Interval param_var_b_17 = det;
    Interval _19671 = idiv(param_var_a_18, param_var_b_17, intervalFailed, interval_divide_upper);
    Interval y = _19671;
    float param_var_x = -9.9999997473787516355514526367188e-06;
    float _18591 = param_var_x;
    float _19675 = interval_down(_18591, intervalFailed);
    float _18592 = _19675;
    float _18593 = param_var_x;
    float _19677 = interval_up(_18593, intervalFailed);
    float _18594 = _19677;
    Interval _18589;
    _18589.lo = _18592;
    _18589.hi = _18594;
    Interval _18590 = _18589;
    Interval _18595 = _18590;
    Interval temp_var_Interval = _18595;
    bool temp_var_logical = true;
    if ((isunordered(x.hi, temp_var_Interval.lo) || x.hi >= temp_var_Interval.lo))
    {
        float param_var_x_1 = -9.9999997473787516355514526367188e-06;
        float _18584 = param_var_x_1;
        float _19691 = interval_down(_18584, intervalFailed);
        float _18585 = _19691;
        float _18586 = param_var_x_1;
        float _19693 = interval_up(_18586, intervalFailed);
        float _18587 = _19693;
        Interval _18582;
        _18582.lo = _18585;
        _18582.hi = _18587;
        Interval _18583 = _18582;
        Interval _18588 = _18583;
        Interval temp_var_Interval_1 = _18588;
        temp_var_logical = y.hi < temp_var_Interval_1.lo;
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        Interval param_var_a_19 = x;
        Interval param_var_b_18 = y;
        Interval _19708 = iadd(param_var_a_19, param_var_b_18, intervalFailed);
        Interval temp_var_Interval_2 = _19708;
        float param_var_x_2 = 1.000010013580322265625;
        float _18577 = param_var_x_2;
        float _19712 = interval_down(_18577, intervalFailed);
        float _18578 = _19712;
        float _18579 = param_var_x_2;
        float _19714 = interval_up(_18579, intervalFailed);
        float _18580 = _19714;
        Interval _18575;
        _18575.lo = _18578;
        _18575.hi = _18580;
        Interval _18576 = _18575;
        Interval _18581 = _18576;
        Interval temp_var_Interval_3 = _18581;
        temp_var_logical_1 = temp_var_Interval_2.lo > temp_var_Interval_3.hi;
    }
    return temp_var_logical_1;
}

static inline __attribute__((always_inline))
bool optical_excluded_target(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTerminal, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper)
{
    float param_var_lo = box.x;
    float param_var_hi = box.z;
    Interval _8874;
    _8874.lo = param_var_lo;
    _8874.hi = param_var_hi;
    Interval _8875 = _8874;
    Interval param_var_a = _8875;
    float param_var_x = receiver.projection.z;
    float _8871 = param_var_x;
    float _8872 = param_var_x;
    Interval _8869;
    _8869.lo = _8871;
    _8869.hi = _8872;
    Interval _8870 = _8869;
    Interval _8873 = _8870;
    Interval param_var_b = _8873;
    Interval _8865 = param_var_a;
    Interval _8866 = param_var_b;
    float _8862 = as_type<float>(as_type<uint>(_8866.hi) ^ 2147483648u);
    float _8863 = as_type<float>(as_type<uint>(_8866.lo) ^ 2147483648u);
    Interval _8860;
    _8860.lo = _8862;
    _8860.hi = _8863;
    Interval _8861 = _8860;
    Interval _8864 = _8861;
    Interval _8867 = _8864;
    Interval _8917 = iadd(_8865, _8867, intervalFailed);
    Interval _8868 = _8917;
    Interval param_var_a_1 = _8868;
    float param_var_x_1 = receiver.projection.x;
    float _8857 = param_var_x_1;
    float _8858 = param_var_x_1;
    Interval _8855;
    _8855.lo = _8857;
    _8855.hi = _8858;
    Interval _8856 = _8855;
    Interval _8859 = _8856;
    Interval param_var_b_1 = _8859;
    Interval _8931 = idiv(param_var_a_1, param_var_b_1, intervalFailed, interval_divide_upper);
    Interval param_var_x_2 = _8931;
    float param_var_lo_1 = box.y;
    float param_var_hi_1 = box.w;
    Interval _8853;
    _8853.lo = param_var_lo_1;
    _8853.hi = param_var_hi_1;
    Interval _8854 = _8853;
    Interval param_var_a_2 = _8854;
    float param_var_x_3 = receiver.projection.w;
    float _8850 = param_var_x_3;
    float _8851 = param_var_x_3;
    Interval _8848;
    _8848.lo = _8850;
    _8848.hi = _8851;
    Interval _8849 = _8848;
    Interval _8852 = _8849;
    Interval param_var_b_2 = _8852;
    Interval _8844 = param_var_a_2;
    Interval _8845 = param_var_b_2;
    float _8841 = as_type<float>(as_type<uint>(_8845.hi) ^ 2147483648u);
    float _8842 = as_type<float>(as_type<uint>(_8845.lo) ^ 2147483648u);
    Interval _8839;
    _8839.lo = _8841;
    _8839.hi = _8842;
    Interval _8840 = _8839;
    Interval _8843 = _8840;
    Interval _8846 = _8843;
    Interval _8973 = iadd(_8844, _8846, intervalFailed);
    Interval _8847 = _8973;
    Interval param_var_a_3 = _8847;
    float param_var_x_4 = receiver.projection.y;
    float _8836 = param_var_x_4;
    float _8837 = param_var_x_4;
    Interval _8834;
    _8834.lo = _8836;
    _8834.hi = _8837;
    Interval _8835 = _8834;
    Interval _8838 = _8835;
    Interval param_var_b_3 = _8838;
    Interval _8987 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_y = _8987;
    float param_var_x_5 = 1.0;
    float _8831 = param_var_x_5;
    float _8832 = param_var_x_5;
    Interval _8829;
    _8829.lo = _8831;
    _8829.hi = _8832;
    Interval _8830 = _8829;
    Interval _8833 = _8830;
    Interval param_var_z = _8833;
    Interval3 _8827;
    _8827.x = param_var_x_2;
    _8827.y = param_var_y;
    _8827.z = param_var_z;
    Interval3 _8828 = _8827;
    Interval3 param_var_a_4 = _8828;
    Interval3 _8820 = param_var_a_4;
    float _8821 = 1.0;
    float _8817 = _8821;
    float _8818 = _8821;
    Interval _8815;
    _8815.lo = _8817;
    _8815.hi = _8818;
    Interval _8816 = _8815;
    Interval _8819 = _8816;
    Interval _8822 = _8819;
    Interval3 _8823 = param_var_a_4;
    Interval _8806 = _8823.x;
    bool _8799 = false;
    if (_8806.lo <= 0.0)
    {
        _8799 = _8806.hi >= 0.0;
    }
    float _8798;
    if (_8799)
    {
        _8798 = 0.0;
    }
    else
    {
        _8798 = precise::min(abs(_8806.lo), abs(_8806.hi));
    }
    float _8797 = _8798;
    float _8800 = precise::max(abs(_8806.lo), abs(_8806.hi));
    float _8801 = spvFMul(_8797, _8797);
    float _9047 = interval_down(_8801, intervalFailed);
    float _8802 = precise::max(0.0, _9047);
    float _8803 = spvFMul(_8800, _8800);
    float _9051 = interval_up(_8803, intervalFailed);
    float _8804 = _9051;
    Interval _8795;
    _8795.lo = _8802;
    _8795.hi = _8804;
    Interval _8796 = _8795;
    Interval _8805 = _8796;
    Interval _8807 = _8805;
    Interval _8808 = _8823.y;
    bool _8788 = false;
    if (_8808.lo <= 0.0)
    {
        _8788 = _8808.hi >= 0.0;
    }
    float _8787;
    if (_8788)
    {
        _8787 = 0.0;
    }
    else
    {
        _8787 = precise::min(abs(_8808.lo), abs(_8808.hi));
    }
    float _8786 = _8787;
    float _8789 = precise::max(abs(_8808.lo), abs(_8808.hi));
    float _8790 = spvFMul(_8786, _8786);
    float _9090 = interval_down(_8790, intervalFailed);
    float _8791 = precise::max(0.0, _9090);
    float _8792 = spvFMul(_8789, _8789);
    float _9094 = interval_up(_8792, intervalFailed);
    float _8793 = _9094;
    Interval _8784;
    _8784.lo = _8791;
    _8784.hi = _8793;
    Interval _8785 = _8784;
    Interval _8794 = _8785;
    Interval _8809 = _8794;
    Interval _9102 = iadd(_8807, _8809, intervalFailed);
    Interval _8810 = _9102;
    Interval _8811 = _8823.z;
    bool _8777 = false;
    if (_8811.lo <= 0.0)
    {
        _8777 = _8811.hi >= 0.0;
    }
    float _8776;
    if (_8777)
    {
        _8776 = 0.0;
    }
    else
    {
        _8776 = precise::min(abs(_8811.lo), abs(_8811.hi));
    }
    float _8775 = _8776;
    float _8778 = precise::max(abs(_8811.lo), abs(_8811.hi));
    float _8779 = spvFMul(_8775, _8775);
    float _9134 = interval_down(_8779, intervalFailed);
    float _8780 = precise::max(0.0, _9134);
    float _8781 = spvFMul(_8778, _8778);
    float _9138 = interval_up(_8781, intervalFailed);
    float _8782 = _9138;
    Interval _8773;
    _8773.lo = _8780;
    _8773.hi = _8782;
    Interval _8774 = _8773;
    Interval _8783 = _8774;
    Interval _8812 = _8783;
    Interval _9146 = iadd(_8810, _8812, intervalFailed);
    Interval _8813 = _9146;
    Interval _9147 = isqrt(_8813, intervalFailed);
    Interval _8814 = _9147;
    Interval _8824 = _8814;
    Interval _9149 = idiv(_8822, _8824, intervalFailed, interval_divide_upper);
    Interval _8825 = _9149;
    Interval _8763 = _8820.x;
    Interval _8764 = _8825;
    Interval _9153 = imul(_8763, _8764, intervalFailed, optical_product_upper);
    Interval _8765 = _9153;
    Interval _8766 = _8820.y;
    Interval _8767 = _8825;
    Interval _9157 = imul(_8766, _8767, intervalFailed, optical_product_upper);
    Interval _8768 = _9157;
    Interval _8769 = _8820.z;
    Interval _8770 = _8825;
    Interval _9161 = imul(_8769, _8770, intervalFailed, optical_product_upper);
    Interval _8771 = _9161;
    Interval3 _8761;
    _8761.x = _8765;
    _8761.y = _8768;
    _8761.z = _8771;
    Interval3 _8762 = _8761;
    Interval3 _8772 = _8762;
    Interval3 _8826 = _8772;
    Interval3 outgoing = _8826;
    float3 param_var_v = float3(0.0);
    float _8754 = param_var_v.x;
    float _8751 = _8754;
    float _8752 = _8754;
    Interval _8749;
    _8749.lo = _8751;
    _8749.hi = _8752;
    Interval _8750 = _8749;
    Interval _8753 = _8750;
    Interval _8755 = _8753;
    float _8756 = param_var_v.y;
    float _8746 = _8756;
    float _8747 = _8756;
    Interval _8744;
    _8744.lo = _8746;
    _8744.hi = _8747;
    Interval _8745 = _8744;
    Interval _8748 = _8745;
    Interval _8757 = _8748;
    float _8758 = param_var_v.z;
    float _8741 = _8758;
    float _8742 = _8758;
    Interval _8739;
    _8739.lo = _8741;
    _8739.hi = _8742;
    Interval _8740 = _8739;
    Interval _8743 = _8740;
    Interval _8759 = _8743;
    Interval3 _8737;
    _8737.x = _8755;
    _8737.y = _8757;
    _8737.z = _8759;
    Interval3 _8738 = _8737;
    Interval3 _8760 = _8738;
    Interval3 origin = _8760;
    float param_var_x_6 = 0.0;
    float _8734 = param_var_x_6;
    float _8735 = param_var_x_6;
    Interval _8732;
    _8732.lo = _8734;
    _8732.hi = _8735;
    Interval _8733 = _8732;
    Interval _8736 = _8733;
    Interval bias0 = _8736;
    bool temp_var_ternary;
    ReflectionSpecularPlane plane;
    Interval3 n;
    float3 temp_var_ternary_1;
    Interval radius;
    int temp_var_ternary_2;
    Interval3 t;
    Interval3 _5356;
    Interval _5358;
    Interval _5363;
    Interval _5368;
    Interval3 _5394;
    Interval _5406;
    float _5409;
    Interval _5417;
    float _5420;
    Interval _5428;
    float _5431;
    Interval _5448;
    Interval3 _5460;
    Interval3 _5472;
    Interval _5484;
    float _5487;
    Interval _5495;
    Interval3 _5500;
    Interval3 _5512;
    Interval3 _5524;
    Interval _5526;
    Interval _5535;
    Interval _5544;
    Interval3 _5575;
    Interval3 _5587;
    Interval _5599;
    float _5602;
    Interval _5610;
    float _5613;
    Interval _5621;
    float _5624;
    Interval _5641;
    Interval3 _5653;
    Interval _5655;
    Interval _5664;
    Interval _5673;
    Interval3 _5704;
    Interval _5706;
    Interval _5711;
    Interval _5716;
    Interval3 _5728;
    Interval _5740;
    float _5743;
    Interval _5751;
    float _5754;
    Interval _5762;
    float _5765;
    Interval _5782;
    Interval3 _5794;
    Interval _5796;
    Interval _5805;
    Interval _5814;
    Interval3 _5845;
    Interval _5847;
    Interval _5852;
    Interval _5857;
    Interval _5869;
    float _5871;
    Interval3 _5876;
    Interval3 _5888;
    Interval _5890;
    Interval _5895;
    Interval _5900;
    Interval3 _5914;
    Interval _5937;
    Interval3 _5952;
    Interval3 _5964;
    Interval _5976;
    Interval3 _5984;
    Interval _5996;
    float _5999;
    Interval _6007;
    float _6010;
    Interval _6018;
    float _6021;
    Interval _6038;
    Interval3 _6050;
    Interval3 _6063;
    Interval _6065;
    Interval _6070;
    Interval _6075;
    Interval3 _6098;
    Interval _6100;
    Interval _6105;
    Interval _6110;
    Interval3 _6133;
    Interval _6135;
    Interval _6140;
    Interval _6145;
    Interval3 _6170;
    Interval _6172;
    Interval _6177;
    Interval _6186;
    float _6189;
    Interval _6197;
    Interval _6202;
    Interval _6207;
    float _6210;
    Interval _6230;
    Interval _6232;
    Interval _6245;
    Interval _6250;
    Interval _6266;
    float _6269;
    Interval _6277;
    Interval _6282;
    Interval _6287;
    float _6290;
    Interval _6310;
    Interval _6312;
    Interval _6325;
    Interval _6330;
    Interval _6346;
    float _6349;
    Interval _6357;
    Interval _6362;
    Interval _6367;
    float _6370;
    Interval _6390;
    Interval _6392;
    Interval _6405;
    Interval _6410;
    Interval _6426;
    float _6429;
    Interval _6437;
    Interval _6442;
    Interval _6447;
    float _6450;
    Interval _6470;
    Interval _6472;
    Interval _6485;
    Interval _6490;
    Interval _6506;
    Interval _6515;
    Interval _6524;
    Interval _6533;
    Interval3 _6542;
    Interval3 _6554;
    Interval3 _6566;
    Interval3 _6578;
    Interval _6590;
    Interval _6595;
    Interval _6605;
    Interval _6610;
    Interval _6615;
    Interval3 _6620;
    Interval _6622;
    Interval _6627;
    Interval _6632;
    Interval _6637;
    Interval _6642;
    Interval _6647;
    Interval _6652;
    Interval _6657;
    Interval _6662;
    float _6665;
    Interval _6673;
    float _6676;
    Interval _6684;
    Interval _6689;
    Interval _6694;
    float _6697;
    Interval _6717;
    Interval _6722;
    Interval _6727;
    Interval _6737;
    Interval _6742;
    Interval _6747;
    Interval _6756;
    float _6759;
    Interval _6767;
    float _6770;
    Interval _6778;
    Interval _6783;
    float _6786;
    Interval _6794;
    float _6797;
    Interval _6805;
    Interval _6807;
    Interval _6820;
    Interval _6827;
    Interval _6829;
    Interval _6838;
    Interval _6843;
    Interval _6848;
    Interval _6861;
    Interval _6870;
    Interval _6875;
    Interval _6888;
    Interval _6897;
    Interval _6906;
    Interval _6908;
    Interval _6910;
    Interval _6912;
    Interval _6917;
    Interval _6922;
    Interval _6924;
    Interval _6937;
    Interval _6942;
    Interval _6958;
    Interval _6960;
    Interval _6973;
    Interval _6978;
    Interval _6994;
    Interval _6996;
    Interval _7009;
    float _7012;
    Interval _7020;
    Interval _7025;
    Interval _7030;
    float _7033;
    Interval _7053;
    Interval _7062;
    Interval _7064;
    Interval _7077;
    Interval _7082;
    Interval _7098;
    Interval _7107;
    Interval _7109;
    Interval _7122;
    Interval _7127;
    Interval _7143;
    Interval _7145;
    Interval _7158;
    Interval _7163;
    Interval _7179;
    Interval _7188;
    Interval _7193;
    Interval _7202;
    Interval _7204;
    Interval _7217;
    Interval _7222;
    Interval _7238;
    Interval _7240;
    Interval _7253;
    Interval _7258;
    Interval _7274;
    Interval _7276;
    Interval _7289;
    Interval _7294;
    Interval _7310;
    Interval _7315;
    Interval _7317;
    Interval _7330;
    Interval _7335;
    Interval _7351;
    Interval _7353;
    Interval _7366;
    Interval _7371;
    Interval _7373;
    Interval _7386;
    Interval _7391;
    Interval _7393;
    Interval _7406;
    Interval _7411;
    float _7414;
    Interval _7422;
    Interval _7427;
    Interval _7432;
    float _7435;
    Interval _7455;
    float _7458;
    Interval _7466;
    Interval _7471;
    Interval _7476;
    float _7479;
    Interval _7499;
    float _7502;
    Interval _7510;
    Interval _7515;
    Interval _7520;
    float _7523;
    Interval _7543;
    Interval _7552;
    Interval _7561;
    Interval _7570;
    Interval _7572;
    Interval _7585;
    Interval _7594;
    float _7597;
    Interval _7605;
    Interval _7610;
    Interval _7615;
    float _7618;
    Interval _7638;
    Interval3 _8049;
    Interval3 _8061;
    Interval3 _8073;
    Interval _8075;
    Interval _8080;
    Interval _8085;
    Interval3 _8097;
    Interval3 _8109;
    Interval3 _8121;
    Interval _8123;
    Interval _8128;
    Interval _8133;
    Interval3 _8145;
    Interval3 _8157;
    Interval _8159;
    Interval _8164;
    Interval _8169;
    Interval _8195;
    Interval _8200;
    Interval3 _8205;
    Interval3 _8217;
    Interval _8219;
    Interval _8224;
    Interval _8229;
    Interval3 _8241;
    Interval3 _8253;
    Interval3 _8265;
    Interval _8267;
    Interval _8272;
    Interval _8277;
    Interval3 _8289;
    Interval3 _8301;
    Interval3 _8313;
    Interval _8315;
    Interval _8320;
    Interval _8325;
    Interval3 _8337;
    Interval3 _8349;
    Interval _8351;
    Interval _8356;
    Interval _8361;
    Interval _8395;
    Interval _8396;
    Interval _8541;
    Interval _8546;
    float _8548;
    Interval _8553;
    Interval _8558;
    float _8561;
    Interval _8569;
    float _8572;
    Interval _8580;
    float _8583;
    Interval3 _8600;
    Interval3 _8612;
    Interval3 _8635;
    Interval3 _8647;
    Interval _8649;
    Interval _8654;
    Interval _8659;
    Interval3 _8673;
    Interval _8675;
    Interval _8680;
    Interval _8685;
    Interval3 _8708;
    Interval _8710;
    Interval _8715;
    Interval _8720;
    for (uint stage = 0u; stage <= control.x; stage++)
    {
        bool primary = stage == 0u;
        if (primary)
        {
            temp_var_ternary = control.z != 0u;
        }
        else
        {
            temp_var_ternary = (control.y & (1u << ((stage - 1u) & 31u))) != 0u;
        }
        bool curved = temp_var_ternary;
        if (primary)
        {
            plane.a = receiver.a;
            plane.b = receiver.b;
            plane.c = receiver.c;
        }
        else
        {
            plane = planes[stage - 1u];
        }
        if (curved)
        {
            float3 param_var_v_1 = liquid.planeNormal.xyz;
            float _8725 = param_var_v_1.x;
            float _8722 = _8725;
            float _8723 = _8725;
            _8720.lo = _8722;
            _8720.hi = _8723;
            Interval _8721 = _8720;
            Interval _8724 = _8721;
            Interval _8726 = _8724;
            float _8727 = param_var_v_1.y;
            float _8717 = _8727;
            float _8718 = _8727;
            _8715.lo = _8717;
            _8715.hi = _8718;
            Interval _8716 = _8715;
            Interval _8719 = _8716;
            Interval _8728 = _8719;
            float _8729 = param_var_v_1.z;
            float _8712 = _8729;
            float _8713 = _8729;
            _8710.lo = _8712;
            _8710.hi = _8713;
            Interval _8711 = _8710;
            Interval _8714 = _8711;
            Interval _8730 = _8714;
            _8708.x = _8726;
            _8708.y = _8728;
            _8708.z = _8730;
            Interval3 _8709 = _8708;
            Interval3 _8731 = _8709;
            n = _8731;
        }
        else
        {
            ReflectionSpecularPlane param_var_p = plane;
            Interval3 _9301 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval3 param_var_n = _9301;
            Interval3 param_var_direction = outgoing;
            Interval3 _9303 = oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper);
            n = _9303;
        }
        Interval3 param_var_a_5 = outgoing;
        Interval3 param_var_b_4 = n;
        Interval _8697 = param_var_a_5.x;
        Interval _8698 = param_var_b_4.x;
        Interval _9310 = imul(_8697, _8698, intervalFailed, optical_product_upper);
        Interval _8699 = _9310;
        Interval _8700 = param_var_a_5.y;
        Interval _8701 = param_var_b_4.y;
        Interval _9315 = imul(_8700, _8701, intervalFailed, optical_product_upper);
        Interval _8702 = _9315;
        Interval _9316 = iadd(_8699, _8702, intervalFailed);
        Interval _8703 = _9316;
        Interval _8704 = param_var_a_5.z;
        Interval _8705 = param_var_b_4.z;
        Interval _9321 = imul(_8704, _8705, intervalFailed, optical_product_upper);
        Interval _8706 = _9321;
        Interval _9322 = iadd(_8703, _8706, intervalFailed);
        Interval _8707 = _9322;
        Interval denominator = _8707;
        if (curved)
        {
            temp_var_ternary_1 = liquid.planePoint.xyz;
        }
        else
        {
            temp_var_ternary_1 = plane.a.xyz;
        }
        float3 param_var_v_2 = temp_var_ternary_1;
        float _8690 = param_var_v_2.x;
        float _8687 = _8690;
        float _8688 = _8690;
        _8685.lo = _8687;
        _8685.hi = _8688;
        Interval _8686 = _8685;
        Interval _8689 = _8686;
        Interval _8691 = _8689;
        float _8692 = param_var_v_2.y;
        float _8682 = _8692;
        float _8683 = _8692;
        _8680.lo = _8682;
        _8680.hi = _8683;
        Interval _8681 = _8680;
        Interval _8684 = _8681;
        Interval _8693 = _8684;
        float _8694 = param_var_v_2.z;
        float _8677 = _8694;
        float _8678 = _8694;
        _8675.lo = _8677;
        _8675.hi = _8678;
        Interval _8676 = _8675;
        Interval _8679 = _8676;
        Interval _8695 = _8679;
        _8673.x = _8691;
        _8673.y = _8693;
        _8673.z = _8695;
        Interval3 _8674 = _8673;
        Interval3 _8696 = _8674;
        Interval3 param_var_a_6 = _8696;
        Interval3 param_var_b_5 = origin;
        Interval3 _8664 = param_var_a_6;
        Interval _8665 = param_var_b_5.x;
        float _8661 = as_type<float>(as_type<uint>(_8665.hi) ^ 2147483648u);
        float _8662 = as_type<float>(as_type<uint>(_8665.lo) ^ 2147483648u);
        _8659.lo = _8661;
        _8659.hi = _8662;
        Interval _8660 = _8659;
        Interval _8663 = _8660;
        Interval _8666 = _8663;
        Interval _8667 = param_var_b_5.y;
        float _8656 = as_type<float>(as_type<uint>(_8667.hi) ^ 2147483648u);
        float _8657 = as_type<float>(as_type<uint>(_8667.lo) ^ 2147483648u);
        _8654.lo = _8656;
        _8654.hi = _8657;
        Interval _8655 = _8654;
        Interval _8658 = _8655;
        Interval _8668 = _8658;
        Interval _8669 = param_var_b_5.z;
        float _8651 = as_type<float>(as_type<uint>(_8669.hi) ^ 2147483648u);
        float _8652 = as_type<float>(as_type<uint>(_8669.lo) ^ 2147483648u);
        _8649.lo = _8651;
        _8649.hi = _8652;
        Interval _8650 = _8649;
        Interval _8653 = _8650;
        Interval _8670 = _8653;
        _8647.x = _8666;
        _8647.y = _8668;
        _8647.z = _8670;
        Interval3 _8648 = _8647;
        Interval3 _8671 = _8648;
        Interval _8637 = _8664.x;
        Interval _8638 = _8671.x;
        Interval _9445 = iadd(_8637, _8638, intervalFailed);
        Interval _8639 = _9445;
        Interval _8640 = _8664.y;
        Interval _8641 = _8671.y;
        Interval _9450 = iadd(_8640, _8641, intervalFailed);
        Interval _8642 = _9450;
        Interval _8643 = _8664.z;
        Interval _8644 = _8671.z;
        Interval _9455 = iadd(_8643, _8644, intervalFailed);
        Interval _8645 = _9455;
        _8635.x = _8639;
        _8635.y = _8642;
        _8635.z = _8645;
        Interval3 _8636 = _8635;
        Interval3 _8646 = _8636;
        Interval3 _8672 = _8646;
        Interval3 param_var_a_7 = _8672;
        Interval3 param_var_b_6 = n;
        Interval _8624 = param_var_a_7.x;
        Interval _8625 = param_var_b_6.x;
        Interval _9471 = imul(_8624, _8625, intervalFailed, optical_product_upper);
        Interval _8626 = _9471;
        Interval _8627 = param_var_a_7.y;
        Interval _8628 = param_var_b_6.y;
        Interval _9476 = imul(_8627, _8628, intervalFailed, optical_product_upper);
        Interval _8629 = _9476;
        Interval _9477 = iadd(_8626, _8629, intervalFailed);
        Interval _8630 = _9477;
        Interval _8631 = param_var_a_7.z;
        Interval _8632 = param_var_b_6.z;
        Interval _9482 = imul(_8631, _8632, intervalFailed, optical_product_upper);
        Interval _8633 = _9482;
        Interval _9483 = iadd(_8630, _8633, intervalFailed);
        Interval _8634 = _9483;
        Interval param_var_a_8 = _8634;
        Interval param_var_b_7 = denominator;
        Interval _9486 = idiv(param_var_a_8, param_var_b_7, intervalFailed, interval_divide_upper);
        Interval _distance = _9486;
        if (primary)
        {
            Interval param_var_a_9 = _distance;
            Interval param_var_b_8 = outgoing.z;
            Interval _9491 = imul(param_var_a_9, param_var_b_8, intervalFailed, optical_product_upper);
            Interval depth = _9491;
            bool temp_var_logical = true;
            if ((isunordered(_distance.hi, 0.0) || _distance.hi > 0.0))
            {
                temp_var_logical = depth.hi < receiver.extentClip.z;
            }
            bool temp_var_logical_1 = true;
            if (!temp_var_logical)
            {
                temp_var_logical_1 = depth.lo > receiver.extentClip.w;
            }
            if (temp_var_logical_1)
            {
                return !intervalFailed;
            }
        }
        else
        {
            bool temp_var_logical_2 = true;
            if ((isunordered(_distance.hi, bias0.lo) || _distance.hi > bias0.lo))
            {
                temp_var_logical_2 = _distance.lo >= 65536.0;
            }
            if (temp_var_logical_2)
            {
                return !intervalFailed;
            }
        }
        Interval3 param_var_a_10 = origin;
        Interval3 param_var_a_11 = outgoing;
        Interval param_var_b_9 = _distance;
        Interval _8614 = param_var_a_11.x;
        Interval _8615 = param_var_b_9;
        Interval _9529 = imul(_8614, _8615, intervalFailed, optical_product_upper);
        Interval _8616 = _9529;
        Interval _8617 = param_var_a_11.y;
        Interval _8618 = param_var_b_9;
        Interval _9533 = imul(_8617, _8618, intervalFailed, optical_product_upper);
        Interval _8619 = _9533;
        Interval _8620 = param_var_a_11.z;
        Interval _8621 = param_var_b_9;
        Interval _9537 = imul(_8620, _8621, intervalFailed, optical_product_upper);
        Interval _8622 = _9537;
        _8612.x = _8616;
        _8612.y = _8619;
        _8612.z = _8622;
        Interval3 _8613 = _8612;
        Interval3 _8623 = _8613;
        Interval3 param_var_b_10 = _8623;
        Interval _8602 = param_var_a_10.x;
        Interval _8603 = param_var_b_10.x;
        Interval _9551 = iadd(_8602, _8603, intervalFailed);
        Interval _8604 = _9551;
        Interval _8605 = param_var_a_10.y;
        Interval _8606 = param_var_b_10.y;
        Interval _9556 = iadd(_8605, _8606, intervalFailed);
        Interval _8607 = _9556;
        Interval _8608 = param_var_a_10.z;
        Interval _8609 = param_var_b_10.z;
        Interval _9561 = iadd(_8608, _8609, intervalFailed);
        Interval _8610 = _9561;
        _8600.x = _8604;
        _8600.y = _8607;
        _8600.z = _8610;
        Interval3 _8601 = _8600;
        Interval3 _8611 = _8601;
        Interval3 hit = _8611;
        if (curved)
        {
            if (primary)
            {
                radius = _distance;
            }
            else
            {
                Interval3 param_var_a_12 = hit;
                Interval _8591 = param_var_a_12.x;
                bool _8584 = false;
                if (_8591.lo <= 0.0)
                {
                    _8584 = _8591.hi >= 0.0;
                }
                if (_8584)
                {
                    _8583 = 0.0;
                }
                else
                {
                    _8583 = precise::min(abs(_8591.lo), abs(_8591.hi));
                }
                float _8582 = _8583;
                float _8585 = precise::max(abs(_8591.lo), abs(_8591.hi));
                float _8586 = spvFMul(_8582, _8582);
                float _9606 = interval_down(_8586, intervalFailed);
                float _8587 = precise::max(0.0, _9606);
                float _8588 = spvFMul(_8585, _8585);
                float _9610 = interval_up(_8588, intervalFailed);
                float _8589 = _9610;
                _8580.lo = _8587;
                _8580.hi = _8589;
                Interval _8581 = _8580;
                Interval _8590 = _8581;
                Interval _8592 = _8590;
                Interval _8593 = param_var_a_12.y;
                bool _8573 = false;
                if (_8593.lo <= 0.0)
                {
                    _8573 = _8593.hi >= 0.0;
                }
                if (_8573)
                {
                    _8572 = 0.0;
                }
                else
                {
                    _8572 = precise::min(abs(_8593.lo), abs(_8593.hi));
                }
                float _8571 = _8572;
                float _8574 = precise::max(abs(_8593.lo), abs(_8593.hi));
                float _8575 = spvFMul(_8571, _8571);
                float _9649 = interval_down(_8575, intervalFailed);
                float _8576 = precise::max(0.0, _9649);
                float _8577 = spvFMul(_8574, _8574);
                float _9653 = interval_up(_8577, intervalFailed);
                float _8578 = _9653;
                _8569.lo = _8576;
                _8569.hi = _8578;
                Interval _8570 = _8569;
                Interval _8579 = _8570;
                Interval _8594 = _8579;
                Interval _9661 = iadd(_8592, _8594, intervalFailed);
                Interval _8595 = _9661;
                Interval _8596 = param_var_a_12.z;
                bool _8562 = false;
                if (_8596.lo <= 0.0)
                {
                    _8562 = _8596.hi >= 0.0;
                }
                if (_8562)
                {
                    _8561 = 0.0;
                }
                else
                {
                    _8561 = precise::min(abs(_8596.lo), abs(_8596.hi));
                }
                float _8560 = _8561;
                float _8563 = precise::max(abs(_8596.lo), abs(_8596.hi));
                float _8564 = spvFMul(_8560, _8560);
                float _9693 = interval_down(_8564, intervalFailed);
                float _8565 = precise::max(0.0, _9693);
                float _8566 = spvFMul(_8563, _8563);
                float _9697 = interval_up(_8566, intervalFailed);
                float _8567 = _9697;
                _8558.lo = _8565;
                _8558.hi = _8567;
                Interval _8559 = _8558;
                Interval _8568 = _8559;
                Interval _8597 = _8568;
                Interval _9705 = iadd(_8595, _8597, intervalFailed);
                Interval _8598 = _9705;
                Interval _9706 = isqrt(_8598, intervalFailed);
                Interval _8599 = _9706;
                radius = _8599;
            }
            Interval param_var_a_13 = radius;
            float param_var_x_7 = precise::max(liquid.projection.x, 1.0);
            float _8555 = param_var_x_7;
            float _8556 = param_var_x_7;
            _8553.lo = _8555;
            _8553.hi = _8556;
            Interval _8554 = _8553;
            Interval _8557 = _8554;
            Interval param_var_b_11 = _8557;
            Interval _9722 = idiv(param_var_a_13, param_var_b_11, intervalFailed, interval_divide_upper);
            Interval param_var_a_14 = _9722;
            Interval param_var_a_15 = denominator;
            bool _8549 = false;
            if (param_var_a_15.lo <= 0.0)
            {
                _8549 = param_var_a_15.hi >= 0.0;
            }
            if (_8549)
            {
                _8548 = 0.0;
            }
            else
            {
                _8548 = precise::min(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            }
            float _8550 = _8548;
            float _8551 = precise::max(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            _8546.lo = _8550;
            _8546.hi = _8551;
            Interval _8547 = _8546;
            Interval _8552 = _8547;
            Interval param_var_a_16 = _8552;
            float param_var_n_1 = 4.0;
            float param_var_d = 100.0;
            Interval _9758 = iratio(param_var_n_1, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_12 = _9758;
            float _8543 = precise::max(param_var_a_16.lo, param_var_b_12.lo);
            float _8544 = precise::max(param_var_a_16.hi, param_var_b_12.hi);
            _8541.lo = _8543;
            _8541.hi = _8544;
            Interval _8542 = _8541;
            Interval _8545 = _8542;
            Interval param_var_b_13 = _8545;
            Interval _9776 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
            Interval footprint = _9776;
            ReflectionLiquidFrame param_var_f = liquid;
            Interval3 param_var_direction_1 = outgoing;
            Interval param_var_distance = _distance;
            Interval param_var_footprint = footprint;
            Interval3 _8388 = hit;
            ReflectionLiquidFrame _8389 = param_var_f;
            float3 _8373 = _8389.rotation0.xyz;
            float _8366 = _8373.x;
            float _8363 = _8366;
            float _8364 = _8366;
            _8361.lo = _8363;
            _8361.hi = _8364;
            Interval _8362 = _8361;
            Interval _8365 = _8362;
            Interval _8367 = _8365;
            float _8368 = _8373.y;
            float _8358 = _8368;
            float _8359 = _8368;
            _8356.lo = _8358;
            _8356.hi = _8359;
            Interval _8357 = _8356;
            Interval _8360 = _8357;
            Interval _8369 = _8360;
            float _8370 = _8373.z;
            float _8353 = _8370;
            float _8354 = _8370;
            _8351.lo = _8353;
            _8351.hi = _8354;
            Interval _8352 = _8351;
            Interval _8355 = _8352;
            Interval _8371 = _8355;
            _8349.x = _8367;
            _8349.y = _8369;
            _8349.z = _8371;
            Interval3 _8350 = _8349;
            Interval3 _8372 = _8350;
            Interval3 _8374 = _8372;
            Interval _8375 = _8388.x;
            Interval _8339 = _8374.x;
            Interval _8340 = _8375;
            Interval _9833 = imul(_8339, _8340, intervalFailed, optical_product_upper);
            Interval _8341 = _9833;
            Interval _8342 = _8374.y;
            Interval _8343 = _8375;
            Interval _9837 = imul(_8342, _8343, intervalFailed, optical_product_upper);
            Interval _8344 = _9837;
            Interval _8345 = _8374.z;
            Interval _8346 = _8375;
            Interval _9841 = imul(_8345, _8346, intervalFailed, optical_product_upper);
            Interval _8347 = _9841;
            _8337.x = _8341;
            _8337.y = _8344;
            _8337.z = _8347;
            Interval3 _8338 = _8337;
            Interval3 _8348 = _8338;
            Interval3 _8376 = _8348;
            float3 _8377 = _8389.rotation1.xyz;
            float _8330 = _8377.x;
            float _8327 = _8330;
            float _8328 = _8330;
            _8325.lo = _8327;
            _8325.hi = _8328;
            Interval _8326 = _8325;
            Interval _8329 = _8326;
            Interval _8331 = _8329;
            float _8332 = _8377.y;
            float _8322 = _8332;
            float _8323 = _8332;
            _8320.lo = _8322;
            _8320.hi = _8323;
            Interval _8321 = _8320;
            Interval _8324 = _8321;
            Interval _8333 = _8324;
            float _8334 = _8377.z;
            float _8317 = _8334;
            float _8318 = _8334;
            _8315.lo = _8317;
            _8315.hi = _8318;
            Interval _8316 = _8315;
            Interval _8319 = _8316;
            Interval _8335 = _8319;
            _8313.x = _8331;
            _8313.y = _8333;
            _8313.z = _8335;
            Interval3 _8314 = _8313;
            Interval3 _8336 = _8314;
            Interval3 _8378 = _8336;
            Interval _8379 = _8388.y;
            Interval _8303 = _8378.x;
            Interval _8304 = _8379;
            Interval _9901 = imul(_8303, _8304, intervalFailed, optical_product_upper);
            Interval _8305 = _9901;
            Interval _8306 = _8378.y;
            Interval _8307 = _8379;
            Interval _9905 = imul(_8306, _8307, intervalFailed, optical_product_upper);
            Interval _8308 = _9905;
            Interval _8309 = _8378.z;
            Interval _8310 = _8379;
            Interval _9909 = imul(_8309, _8310, intervalFailed, optical_product_upper);
            Interval _8311 = _9909;
            _8301.x = _8305;
            _8301.y = _8308;
            _8301.z = _8311;
            Interval3 _8302 = _8301;
            Interval3 _8312 = _8302;
            Interval3 _8380 = _8312;
            Interval _8291 = _8376.x;
            Interval _8292 = _8380.x;
            Interval _9923 = iadd(_8291, _8292, intervalFailed);
            Interval _8293 = _9923;
            Interval _8294 = _8376.y;
            Interval _8295 = _8380.y;
            Interval _9928 = iadd(_8294, _8295, intervalFailed);
            Interval _8296 = _9928;
            Interval _8297 = _8376.z;
            Interval _8298 = _8380.z;
            Interval _9933 = iadd(_8297, _8298, intervalFailed);
            Interval _8299 = _9933;
            _8289.x = _8293;
            _8289.y = _8296;
            _8289.z = _8299;
            Interval3 _8290 = _8289;
            Interval3 _8300 = _8290;
            Interval3 _8381 = _8300;
            float3 _8382 = _8389.rotation2.xyz;
            float _8282 = _8382.x;
            float _8279 = _8282;
            float _8280 = _8282;
            _8277.lo = _8279;
            _8277.hi = _8280;
            Interval _8278 = _8277;
            Interval _8281 = _8278;
            Interval _8283 = _8281;
            float _8284 = _8382.y;
            float _8274 = _8284;
            float _8275 = _8284;
            _8272.lo = _8274;
            _8272.hi = _8275;
            Interval _8273 = _8272;
            Interval _8276 = _8273;
            Interval _8285 = _8276;
            float _8286 = _8382.z;
            float _8269 = _8286;
            float _8270 = _8286;
            _8267.lo = _8269;
            _8267.hi = _8270;
            Interval _8268 = _8267;
            Interval _8271 = _8268;
            Interval _8287 = _8271;
            _8265.x = _8283;
            _8265.y = _8285;
            _8265.z = _8287;
            Interval3 _8266 = _8265;
            Interval3 _8288 = _8266;
            Interval3 _8383 = _8288;
            Interval _8384 = _8388.z;
            Interval _8255 = _8383.x;
            Interval _8256 = _8384;
            Interval _9993 = imul(_8255, _8256, intervalFailed, optical_product_upper);
            Interval _8257 = _9993;
            Interval _8258 = _8383.y;
            Interval _8259 = _8384;
            Interval _9997 = imul(_8258, _8259, intervalFailed, optical_product_upper);
            Interval _8260 = _9997;
            Interval _8261 = _8383.z;
            Interval _8262 = _8384;
            Interval _10001 = imul(_8261, _8262, intervalFailed, optical_product_upper);
            Interval _8263 = _10001;
            _8253.x = _8257;
            _8253.y = _8260;
            _8253.z = _8263;
            Interval3 _8254 = _8253;
            Interval3 _8264 = _8254;
            Interval3 _8385 = _8264;
            Interval _8243 = _8381.x;
            Interval _8244 = _8385.x;
            Interval _10015 = iadd(_8243, _8244, intervalFailed);
            Interval _8245 = _10015;
            Interval _8246 = _8381.y;
            Interval _8247 = _8385.y;
            Interval _10020 = iadd(_8246, _8247, intervalFailed);
            Interval _8248 = _10020;
            Interval _8249 = _8381.z;
            Interval _8250 = _8385.z;
            Interval _10025 = iadd(_8249, _8250, intervalFailed);
            Interval _8251 = _10025;
            _8241.x = _8245;
            _8241.y = _8248;
            _8241.z = _8251;
            Interval3 _8242 = _8241;
            Interval3 _8252 = _8242;
            Interval3 _8386 = _8252;
            Interval3 _8390 = _8386;
            float3 _8391 = float3(param_var_f.rotation0.w, param_var_f.rotation1.w, param_var_f.rotation2.w);
            float _8234 = _8391.x;
            float _8231 = _8234;
            float _8232 = _8234;
            _8229.lo = _8231;
            _8229.hi = _8232;
            Interval _8230 = _8229;
            Interval _8233 = _8230;
            Interval _8235 = _8233;
            float _8236 = _8391.y;
            float _8226 = _8236;
            float _8227 = _8236;
            _8224.lo = _8226;
            _8224.hi = _8227;
            Interval _8225 = _8224;
            Interval _8228 = _8225;
            Interval _8237 = _8228;
            float _8238 = _8391.z;
            float _8221 = _8238;
            float _8222 = _8238;
            _8219.lo = _8221;
            _8219.hi = _8222;
            Interval _8220 = _8219;
            Interval _8223 = _8220;
            Interval _8239 = _8223;
            _8217.x = _8235;
            _8217.y = _8237;
            _8217.z = _8239;
            Interval3 _8218 = _8217;
            Interval3 _8240 = _8218;
            Interval3 _8392 = _8240;
            Interval _8207 = _8390.x;
            Interval _8208 = _8392.x;
            Interval _10092 = iadd(_8207, _8208, intervalFailed);
            Interval _8209 = _10092;
            Interval _8210 = _8390.y;
            Interval _8211 = _8392.y;
            Interval _10097 = iadd(_8210, _8211, intervalFailed);
            Interval _8212 = _10097;
            Interval _8213 = _8390.z;
            Interval _8214 = _8392.z;
            Interval _10102 = iadd(_8213, _8214, intervalFailed);
            Interval _8215 = _10102;
            _8205.x = _8209;
            _8205.y = _8212;
            _8205.z = _8215;
            Interval3 _8206 = _8205;
            Interval3 _8216 = _8206;
            Interval3 _8387 = _8216;
            float _8394 = param_var_f.settings.x;
            float _8202 = _8394;
            float _8203 = _8394;
            _8200.lo = _8202;
            _8200.hi = _8203;
            Interval _8201 = _8200;
            Interval _8204 = _8201;
            Interval _8393 = _8204;
            if (param_var_f.settings.y == 3.0)
            {
                float _8397 = 0.0;
                float _8197 = _8397;
                float _8198 = _8397;
                _8195.lo = _8197;
                _8195.hi = _8198;
                Interval _8196 = _8195;
                Interval _8199 = _8196;
                _8396 = _8199;
                _8395 = _8199;
                Interval3 _8399 = param_var_direction_1;
                ReflectionLiquidFrame _8400 = param_var_f;
                float3 _8181 = _8400.rotation0.xyz;
                float _8174 = _8181.x;
                float _8171 = _8174;
                float _8172 = _8174;
                _8169.lo = _8171;
                _8169.hi = _8172;
                Interval _8170 = _8169;
                Interval _8173 = _8170;
                Interval _8175 = _8173;
                float _8176 = _8181.y;
                float _8166 = _8176;
                float _8167 = _8176;
                _8164.lo = _8166;
                _8164.hi = _8167;
                Interval _8165 = _8164;
                Interval _8168 = _8165;
                Interval _8177 = _8168;
                float _8178 = _8181.z;
                float _8161 = _8178;
                float _8162 = _8178;
                _8159.lo = _8161;
                _8159.hi = _8162;
                Interval _8160 = _8159;
                Interval _8163 = _8160;
                Interval _8179 = _8163;
                _8157.x = _8175;
                _8157.y = _8177;
                _8157.z = _8179;
                Interval3 _8158 = _8157;
                Interval3 _8180 = _8158;
                Interval3 _8182 = _8180;
                Interval _8183 = _8399.x;
                Interval _8147 = _8182.x;
                Interval _8148 = _8183;
                Interval _10192 = imul(_8147, _8148, intervalFailed, optical_product_upper);
                Interval _8149 = _10192;
                Interval _8150 = _8182.y;
                Interval _8151 = _8183;
                Interval _10196 = imul(_8150, _8151, intervalFailed, optical_product_upper);
                Interval _8152 = _10196;
                Interval _8153 = _8182.z;
                Interval _8154 = _8183;
                Interval _10200 = imul(_8153, _8154, intervalFailed, optical_product_upper);
                Interval _8155 = _10200;
                _8145.x = _8149;
                _8145.y = _8152;
                _8145.z = _8155;
                Interval3 _8146 = _8145;
                Interval3 _8156 = _8146;
                Interval3 _8184 = _8156;
                float3 _8185 = _8400.rotation1.xyz;
                float _8138 = _8185.x;
                float _8135 = _8138;
                float _8136 = _8138;
                _8133.lo = _8135;
                _8133.hi = _8136;
                Interval _8134 = _8133;
                Interval _8137 = _8134;
                Interval _8139 = _8137;
                float _8140 = _8185.y;
                float _8130 = _8140;
                float _8131 = _8140;
                _8128.lo = _8130;
                _8128.hi = _8131;
                Interval _8129 = _8128;
                Interval _8132 = _8129;
                Interval _8141 = _8132;
                float _8142 = _8185.z;
                float _8125 = _8142;
                float _8126 = _8142;
                _8123.lo = _8125;
                _8123.hi = _8126;
                Interval _8124 = _8123;
                Interval _8127 = _8124;
                Interval _8143 = _8127;
                _8121.x = _8139;
                _8121.y = _8141;
                _8121.z = _8143;
                Interval3 _8122 = _8121;
                Interval3 _8144 = _8122;
                Interval3 _8186 = _8144;
                Interval _8187 = _8399.y;
                Interval _8111 = _8186.x;
                Interval _8112 = _8187;
                Interval _10260 = imul(_8111, _8112, intervalFailed, optical_product_upper);
                Interval _8113 = _10260;
                Interval _8114 = _8186.y;
                Interval _8115 = _8187;
                Interval _10264 = imul(_8114, _8115, intervalFailed, optical_product_upper);
                Interval _8116 = _10264;
                Interval _8117 = _8186.z;
                Interval _8118 = _8187;
                Interval _10268 = imul(_8117, _8118, intervalFailed, optical_product_upper);
                Interval _8119 = _10268;
                _8109.x = _8113;
                _8109.y = _8116;
                _8109.z = _8119;
                Interval3 _8110 = _8109;
                Interval3 _8120 = _8110;
                Interval3 _8188 = _8120;
                Interval _8099 = _8184.x;
                Interval _8100 = _8188.x;
                Interval _10282 = iadd(_8099, _8100, intervalFailed);
                Interval _8101 = _10282;
                Interval _8102 = _8184.y;
                Interval _8103 = _8188.y;
                Interval _10287 = iadd(_8102, _8103, intervalFailed);
                Interval _8104 = _10287;
                Interval _8105 = _8184.z;
                Interval _8106 = _8188.z;
                Interval _10292 = iadd(_8105, _8106, intervalFailed);
                Interval _8107 = _10292;
                _8097.x = _8101;
                _8097.y = _8104;
                _8097.z = _8107;
                Interval3 _8098 = _8097;
                Interval3 _8108 = _8098;
                Interval3 _8189 = _8108;
                float3 _8190 = _8400.rotation2.xyz;
                float _8090 = _8190.x;
                float _8087 = _8090;
                float _8088 = _8090;
                _8085.lo = _8087;
                _8085.hi = _8088;
                Interval _8086 = _8085;
                Interval _8089 = _8086;
                Interval _8091 = _8089;
                float _8092 = _8190.y;
                float _8082 = _8092;
                float _8083 = _8092;
                _8080.lo = _8082;
                _8080.hi = _8083;
                Interval _8081 = _8080;
                Interval _8084 = _8081;
                Interval _8093 = _8084;
                float _8094 = _8190.z;
                float _8077 = _8094;
                float _8078 = _8094;
                _8075.lo = _8077;
                _8075.hi = _8078;
                Interval _8076 = _8075;
                Interval _8079 = _8076;
                Interval _8095 = _8079;
                _8073.x = _8091;
                _8073.y = _8093;
                _8073.z = _8095;
                Interval3 _8074 = _8073;
                Interval3 _8096 = _8074;
                Interval3 _8191 = _8096;
                Interval _8192 = _8399.z;
                Interval _8063 = _8191.x;
                Interval _8064 = _8192;
                Interval _10352 = imul(_8063, _8064, intervalFailed, optical_product_upper);
                Interval _8065 = _10352;
                Interval _8066 = _8191.y;
                Interval _8067 = _8192;
                Interval _10356 = imul(_8066, _8067, intervalFailed, optical_product_upper);
                Interval _8068 = _10356;
                Interval _8069 = _8191.z;
                Interval _8070 = _8192;
                Interval _10360 = imul(_8069, _8070, intervalFailed, optical_product_upper);
                Interval _8071 = _10360;
                _8061.x = _8065;
                _8061.y = _8068;
                _8061.z = _8071;
                Interval3 _8062 = _8061;
                Interval3 _8072 = _8062;
                Interval3 _8193 = _8072;
                Interval _8051 = _8189.x;
                Interval _8052 = _8193.x;
                Interval _10374 = iadd(_8051, _8052, intervalFailed);
                Interval _8053 = _10374;
                Interval _8054 = _8189.y;
                Interval _8055 = _8193.y;
                Interval _10379 = iadd(_8054, _8055, intervalFailed);
                Interval _8056 = _10379;
                Interval _8057 = _8189.z;
                Interval _8058 = _8193.z;
                Interval _10384 = iadd(_8057, _8058, intervalFailed);
                Interval _8059 = _10384;
                _8049.x = _8053;
                _8049.y = _8056;
                _8049.z = _8059;
                Interval3 _8050 = _8049;
                Interval3 _8060 = _8050;
                Interval3 _8194 = _8060;
                Interval3 _8398 = _8194;
                for (uint _8401 = 0u; _8401 < 2u; _8401++)
                {
                    Interval _8403 = _8387.x;
                    Interval _8404 = _8387.z;
                    Interval _8405 = _8393;
                    Interval _8406 = param_var_footprint;
                    Interval _7648 = _8403;
                    float _7649 = 6.0;
                    float _7650 = 1000.0;
                    Interval _10408 = iratio(_7649, _7650, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7651 = _10408;
                    Interval _10409 = imul(_7648, _7651, intervalFailed, optical_product_upper);
                    Interval _7652 = _10409;
                    Interval _7653 = _8404;
                    float _7654 = 8.0;
                    float _7655 = 1000.0;
                    Interval _10411 = iratio(_7654, _7655, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7656 = _10411;
                    Interval _10412 = imul(_7653, _7656, intervalFailed, optical_product_upper);
                    Interval _7657 = _10412;
                    Interval _7643 = _7652;
                    Interval _7644 = _7657;
                    float _7640 = as_type<float>(as_type<uint>(_7644.hi) ^ 2147483648u);
                    float _7641 = as_type<float>(as_type<uint>(_7644.lo) ^ 2147483648u);
                    _7638.lo = _7640;
                    _7638.hi = _7641;
                    Interval _7639 = _7638;
                    Interval _7642 = _7639;
                    Interval _7645 = _7642;
                    Interval _10432 = iadd(_7643, _7645, intervalFailed);
                    Interval _7646 = _10432;
                    Interval _7658 = _7646;
                    Interval _7659 = _8405;
                    float _7660 = 11.0;
                    float _7661 = 100.0;
                    Interval _10435 = iratio(_7660, _7661, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7662 = _10435;
                    Interval _10436 = imul(_7659, _7662, intervalFailed, optical_product_upper);
                    Interval _7663 = _10436;
                    Interval _10437 = iadd(_7658, _7663, intervalFailed);
                    Interval _7647 = _10437;
                    Interval _7665 = _8406;
                    float _7666 = 10.0;
                    float _7667 = 1000.0;
                    Interval _10439 = iratio(_7666, _7667, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7668 = _10439;
                    Interval _7627 = _7665;
                    Interval _7628 = _7668;
                    Interval _10442 = imul(_7627, _7628, intervalFailed, optical_product_upper);
                    Interval _7629 = _10442;
                    bool _7619 = false;
                    if (_7629.lo <= 0.0)
                    {
                        _7619 = _7629.hi >= 0.0;
                    }
                    if (_7619)
                    {
                        _7618 = 0.0;
                    }
                    else
                    {
                        _7618 = precise::min(abs(_7629.lo), abs(_7629.hi));
                    }
                    float _7617 = _7618;
                    float _7620 = precise::max(abs(_7629.lo), abs(_7629.hi));
                    float _7621 = spvFMul(_7617, _7617);
                    float _10472 = interval_down(_7621, intervalFailed);
                    float _7622 = precise::max(0.0, _10472);
                    float _7623 = spvFMul(_7620, _7620);
                    float _10476 = interval_up(_7623, intervalFailed);
                    float _7624 = _10476;
                    _7615.lo = _7622;
                    _7615.hi = _7624;
                    Interval _7616 = _7615;
                    Interval _7625 = _7616;
                    Interval _7626 = _7625;
                    float _7630 = 1.0;
                    float _7612 = _7630;
                    float _7613 = _7630;
                    _7610.lo = _7612;
                    _7610.hi = _7613;
                    Interval _7611 = _7610;
                    Interval _7614 = _7611;
                    Interval _7631 = _7614;
                    float _7632 = 1.0;
                    float _7607 = _7632;
                    float _7608 = _7632;
                    _7605.lo = _7607;
                    _7605.hi = _7608;
                    Interval _7606 = _7605;
                    Interval _7609 = _7606;
                    Interval _7633 = _7609;
                    Interval _7634 = _7626;
                    bool _7598 = false;
                    if (_7634.lo <= 0.0)
                    {
                        _7598 = _7634.hi >= 0.0;
                    }
                    if (_7598)
                    {
                        _7597 = 0.0;
                    }
                    else
                    {
                        _7597 = precise::min(abs(_7634.lo), abs(_7634.hi));
                    }
                    float _7596 = _7597;
                    float _7599 = precise::max(abs(_7634.lo), abs(_7634.hi));
                    float _7600 = spvFMul(_7596, _7596);
                    float _10532 = interval_down(_7600, intervalFailed);
                    float _7601 = precise::max(0.0, _10532);
                    float _7602 = spvFMul(_7599, _7599);
                    float _10536 = interval_up(_7602, intervalFailed);
                    float _7603 = _10536;
                    _7594.lo = _7601;
                    _7594.hi = _7603;
                    Interval _7595 = _7594;
                    Interval _7604 = _7595;
                    Interval _7635 = _7604;
                    Interval _10544 = iadd(_7633, _7635, intervalFailed);
                    Interval _7636 = _10544;
                    Interval _10545 = idiv(_7631, _7636, intervalFailed, interval_divide_upper);
                    Interval _7637 = _10545;
                    Interval _7664 = _7637;
                    Interval _7670 = _8403;
                    float _7671 = 18.0;
                    float _7672 = 1000.0;
                    Interval _10548 = iratio(_7671, _7672, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7673 = _10548;
                    Interval _10549 = imul(_7670, _7673, intervalFailed, optical_product_upper);
                    Interval _7674 = _10549;
                    Interval _7675 = _8404;
                    float _7676 = 11.0;
                    float _7677 = 1000.0;
                    Interval _10551 = iratio(_7676, _7677, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7678 = _10551;
                    Interval _10552 = imul(_7675, _7678, intervalFailed, optical_product_upper);
                    Interval _7679 = _10552;
                    Interval _10553 = iadd(_7674, _7679, intervalFailed);
                    Interval _7680 = _10553;
                    Interval _7681 = _8405;
                    float _7682 = 45.0;
                    float _7683 = 100.0;
                    Interval _10555 = iratio(_7682, _7683, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7684 = _10555;
                    Interval _10556 = imul(_7681, _7684, intervalFailed, optical_product_upper);
                    Interval _7685 = _10556;
                    Interval _7590 = _7680;
                    Interval _7591 = _7685;
                    float _7587 = as_type<float>(as_type<uint>(_7591.hi) ^ 2147483648u);
                    float _7588 = as_type<float>(as_type<uint>(_7591.lo) ^ 2147483648u);
                    _7585.lo = _7587;
                    _7585.hi = _7588;
                    Interval _7586 = _7585;
                    Interval _7589 = _7586;
                    Interval _7592 = _7589;
                    Interval _10576 = iadd(_7590, _7592, intervalFailed);
                    Interval _7593 = _10576;
                    Interval _7686 = _7593;
                    float _7687 = 65.0;
                    float _7688 = 100.0;
                    Interval _10578 = iratio(_7687, _7688, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7689 = _10578;
                    Interval _7690 = _7647;
                    float _7580 = _7690.lo;
                    float _7581 = _7690.hi;
                    float _7575 = _7580;
                    float _7576 = _7581;
                    _7572.lo = _7575;
                    _7572.hi = _7576;
                    Interval _7573 = _7572;
                    Interval _7577 = _7573;
                    Interval _10592 = isin_body(_7577, intervalFailed, optical_product_upper);
                    Interval _7574 = _10592;
                    interval_sine_upper = _7574.hi;
                    float _7578 = _7574.lo;
                    float _7579 = _7578;
                    float _7582 = _7579;
                    float _7583 = interval_sine_upper;
                    _7570.lo = _7582;
                    _7570.hi = _7583;
                    Interval _7571 = _7570;
                    Interval _7584 = _7571;
                    Interval _7691 = _7584;
                    Interval _10607 = imul(_7689, _7691, intervalFailed, optical_product_upper);
                    Interval _7692 = _10607;
                    Interval _7693 = _7664;
                    Interval _10609 = imul(_7692, _7693, intervalFailed, optical_product_upper);
                    Interval _7694 = _10609;
                    Interval _10610 = iadd(_7686, _7694, intervalFailed);
                    Interval _7669 = _10610;
                    Interval _7696 = _8403;
                    float _7697 = 47.0;
                    float _7698 = 1000.0;
                    Interval _10612 = iratio(_7697, _7698, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7699 = _10612;
                    Interval _10613 = imul(_7696, _7699, intervalFailed, optical_product_upper);
                    Interval _7700 = _10613;
                    Interval _7701 = _8404;
                    float _7702 = 25.0;
                    float _7703 = 1000.0;
                    Interval _10615 = iratio(_7702, _7703, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7704 = _10615;
                    Interval _10616 = imul(_7701, _7704, intervalFailed, optical_product_upper);
                    Interval _7705 = _10616;
                    Interval _7566 = _7700;
                    Interval _7567 = _7705;
                    float _7563 = as_type<float>(as_type<uint>(_7567.hi) ^ 2147483648u);
                    float _7564 = as_type<float>(as_type<uint>(_7567.lo) ^ 2147483648u);
                    _7561.lo = _7563;
                    _7561.hi = _7564;
                    Interval _7562 = _7561;
                    Interval _7565 = _7562;
                    Interval _7568 = _7565;
                    Interval _10636 = iadd(_7566, _7568, intervalFailed);
                    Interval _7569 = _10636;
                    Interval _7706 = _7569;
                    Interval _7707 = _8405;
                    float _7708 = 60.0;
                    float _7709 = 100.0;
                    Interval _10639 = iratio(_7708, _7709, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7710 = _10639;
                    Interval _10640 = imul(_7707, _7710, intervalFailed, optical_product_upper);
                    Interval _7711 = _10640;
                    Interval _10641 = iadd(_7706, _7711, intervalFailed);
                    Interval _7695 = _10641;
                    Interval _7713 = _8404;
                    float _7714 = 22.0;
                    float _7715 = 1000.0;
                    Interval _10643 = iratio(_7714, _7715, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7716 = _10643;
                    Interval _10644 = imul(_7713, _7716, intervalFailed, optical_product_upper);
                    Interval _7717 = _10644;
                    Interval _7718 = _8403;
                    float _7719 = 9.0;
                    float _7720 = 1000.0;
                    Interval _10646 = iratio(_7719, _7720, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7721 = _10646;
                    Interval _10647 = imul(_7718, _7721, intervalFailed, optical_product_upper);
                    Interval _7722 = _10647;
                    Interval _7557 = _7717;
                    Interval _7558 = _7722;
                    float _7554 = as_type<float>(as_type<uint>(_7558.hi) ^ 2147483648u);
                    float _7555 = as_type<float>(as_type<uint>(_7558.lo) ^ 2147483648u);
                    _7552.lo = _7554;
                    _7552.hi = _7555;
                    Interval _7553 = _7552;
                    Interval _7556 = _7553;
                    Interval _7559 = _7556;
                    Interval _10667 = iadd(_7557, _7559, intervalFailed);
                    Interval _7560 = _10667;
                    Interval _7723 = _7560;
                    Interval _7724 = _8405;
                    float _7725 = 32.0;
                    float _7726 = 100.0;
                    Interval _10670 = iratio(_7725, _7726, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7727 = _10670;
                    Interval _10671 = imul(_7724, _7727, intervalFailed, optical_product_upper);
                    Interval _7728 = _10671;
                    Interval _7548 = _7723;
                    Interval _7549 = _7728;
                    float _7545 = as_type<float>(as_type<uint>(_7549.hi) ^ 2147483648u);
                    float _7546 = as_type<float>(as_type<uint>(_7549.lo) ^ 2147483648u);
                    _7543.lo = _7545;
                    _7543.hi = _7546;
                    Interval _7544 = _7543;
                    Interval _7547 = _7544;
                    Interval _7550 = _7547;
                    Interval _10691 = iadd(_7548, _7550, intervalFailed);
                    Interval _7551 = _10691;
                    Interval _7712 = _7551;
                    Interval _7730 = _8406;
                    float _7731 = 22.0;
                    float _7732 = 1000.0;
                    Interval _10694 = iratio(_7731, _7732, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7733 = _10694;
                    Interval _7532 = _7730;
                    Interval _7533 = _7733;
                    Interval _10697 = imul(_7532, _7533, intervalFailed, optical_product_upper);
                    Interval _7534 = _10697;
                    bool _7524 = false;
                    if (_7534.lo <= 0.0)
                    {
                        _7524 = _7534.hi >= 0.0;
                    }
                    if (_7524)
                    {
                        _7523 = 0.0;
                    }
                    else
                    {
                        _7523 = precise::min(abs(_7534.lo), abs(_7534.hi));
                    }
                    float _7522 = _7523;
                    float _7525 = precise::max(abs(_7534.lo), abs(_7534.hi));
                    float _7526 = spvFMul(_7522, _7522);
                    float _10727 = interval_down(_7526, intervalFailed);
                    float _7527 = precise::max(0.0, _10727);
                    float _7528 = spvFMul(_7525, _7525);
                    float _10731 = interval_up(_7528, intervalFailed);
                    float _7529 = _10731;
                    _7520.lo = _7527;
                    _7520.hi = _7529;
                    Interval _7521 = _7520;
                    Interval _7530 = _7521;
                    Interval _7531 = _7530;
                    float _7535 = 1.0;
                    float _7517 = _7535;
                    float _7518 = _7535;
                    _7515.lo = _7517;
                    _7515.hi = _7518;
                    Interval _7516 = _7515;
                    Interval _7519 = _7516;
                    Interval _7536 = _7519;
                    float _7537 = 1.0;
                    float _7512 = _7537;
                    float _7513 = _7537;
                    _7510.lo = _7512;
                    _7510.hi = _7513;
                    Interval _7511 = _7510;
                    Interval _7514 = _7511;
                    Interval _7538 = _7514;
                    Interval _7539 = _7531;
                    bool _7503 = false;
                    if (_7539.lo <= 0.0)
                    {
                        _7503 = _7539.hi >= 0.0;
                    }
                    if (_7503)
                    {
                        _7502 = 0.0;
                    }
                    else
                    {
                        _7502 = precise::min(abs(_7539.lo), abs(_7539.hi));
                    }
                    float _7501 = _7502;
                    float _7504 = precise::max(abs(_7539.lo), abs(_7539.hi));
                    float _7505 = spvFMul(_7501, _7501);
                    float _10787 = interval_down(_7505, intervalFailed);
                    float _7506 = precise::max(0.0, _10787);
                    float _7507 = spvFMul(_7504, _7504);
                    float _10791 = interval_up(_7507, intervalFailed);
                    float _7508 = _10791;
                    _7499.lo = _7506;
                    _7499.hi = _7508;
                    Interval _7500 = _7499;
                    Interval _7509 = _7500;
                    Interval _7540 = _7509;
                    Interval _10799 = iadd(_7538, _7540, intervalFailed);
                    Interval _7541 = _10799;
                    Interval _10800 = idiv(_7536, _7541, intervalFailed, interval_divide_upper);
                    Interval _7542 = _10800;
                    Interval _7729 = _7542;
                    Interval _7735 = _8406;
                    float _7736 = 54.0;
                    float _7737 = 1000.0;
                    Interval _10803 = iratio(_7736, _7737, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7738 = _10803;
                    Interval _7488 = _7735;
                    Interval _7489 = _7738;
                    Interval _10806 = imul(_7488, _7489, intervalFailed, optical_product_upper);
                    Interval _7490 = _10806;
                    bool _7480 = false;
                    if (_7490.lo <= 0.0)
                    {
                        _7480 = _7490.hi >= 0.0;
                    }
                    if (_7480)
                    {
                        _7479 = 0.0;
                    }
                    else
                    {
                        _7479 = precise::min(abs(_7490.lo), abs(_7490.hi));
                    }
                    float _7478 = _7479;
                    float _7481 = precise::max(abs(_7490.lo), abs(_7490.hi));
                    float _7482 = spvFMul(_7478, _7478);
                    float _10836 = interval_down(_7482, intervalFailed);
                    float _7483 = precise::max(0.0, _10836);
                    float _7484 = spvFMul(_7481, _7481);
                    float _10840 = interval_up(_7484, intervalFailed);
                    float _7485 = _10840;
                    _7476.lo = _7483;
                    _7476.hi = _7485;
                    Interval _7477 = _7476;
                    Interval _7486 = _7477;
                    Interval _7487 = _7486;
                    float _7491 = 1.0;
                    float _7473 = _7491;
                    float _7474 = _7491;
                    _7471.lo = _7473;
                    _7471.hi = _7474;
                    Interval _7472 = _7471;
                    Interval _7475 = _7472;
                    Interval _7492 = _7475;
                    float _7493 = 1.0;
                    float _7468 = _7493;
                    float _7469 = _7493;
                    _7466.lo = _7468;
                    _7466.hi = _7469;
                    Interval _7467 = _7466;
                    Interval _7470 = _7467;
                    Interval _7494 = _7470;
                    Interval _7495 = _7487;
                    bool _7459 = false;
                    if (_7495.lo <= 0.0)
                    {
                        _7459 = _7495.hi >= 0.0;
                    }
                    if (_7459)
                    {
                        _7458 = 0.0;
                    }
                    else
                    {
                        _7458 = precise::min(abs(_7495.lo), abs(_7495.hi));
                    }
                    float _7457 = _7458;
                    float _7460 = precise::max(abs(_7495.lo), abs(_7495.hi));
                    float _7461 = spvFMul(_7457, _7457);
                    float _10896 = interval_down(_7461, intervalFailed);
                    float _7462 = precise::max(0.0, _10896);
                    float _7463 = spvFMul(_7460, _7460);
                    float _10900 = interval_up(_7463, intervalFailed);
                    float _7464 = _10900;
                    _7455.lo = _7462;
                    _7455.hi = _7464;
                    Interval _7456 = _7455;
                    Interval _7465 = _7456;
                    Interval _7496 = _7465;
                    Interval _10908 = iadd(_7494, _7496, intervalFailed);
                    Interval _7497 = _10908;
                    Interval _10909 = idiv(_7492, _7497, intervalFailed, interval_divide_upper);
                    Interval _7498 = _10909;
                    Interval _7734 = _7498;
                    Interval _7740 = _8406;
                    float _7741 = 24.0;
                    float _7742 = 1000.0;
                    Interval _10912 = iratio(_7741, _7742, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7743 = _10912;
                    Interval _7444 = _7740;
                    Interval _7445 = _7743;
                    Interval _10915 = imul(_7444, _7445, intervalFailed, optical_product_upper);
                    Interval _7446 = _10915;
                    bool _7436 = false;
                    if (_7446.lo <= 0.0)
                    {
                        _7436 = _7446.hi >= 0.0;
                    }
                    if (_7436)
                    {
                        _7435 = 0.0;
                    }
                    else
                    {
                        _7435 = precise::min(abs(_7446.lo), abs(_7446.hi));
                    }
                    float _7434 = _7435;
                    float _7437 = precise::max(abs(_7446.lo), abs(_7446.hi));
                    float _7438 = spvFMul(_7434, _7434);
                    float _10945 = interval_down(_7438, intervalFailed);
                    float _7439 = precise::max(0.0, _10945);
                    float _7440 = spvFMul(_7437, _7437);
                    float _10949 = interval_up(_7440, intervalFailed);
                    float _7441 = _10949;
                    _7432.lo = _7439;
                    _7432.hi = _7441;
                    Interval _7433 = _7432;
                    Interval _7442 = _7433;
                    Interval _7443 = _7442;
                    float _7447 = 1.0;
                    float _7429 = _7447;
                    float _7430 = _7447;
                    _7427.lo = _7429;
                    _7427.hi = _7430;
                    Interval _7428 = _7427;
                    Interval _7431 = _7428;
                    Interval _7448 = _7431;
                    float _7449 = 1.0;
                    float _7424 = _7449;
                    float _7425 = _7449;
                    _7422.lo = _7424;
                    _7422.hi = _7425;
                    Interval _7423 = _7422;
                    Interval _7426 = _7423;
                    Interval _7450 = _7426;
                    Interval _7451 = _7443;
                    bool _7415 = false;
                    if (_7451.lo <= 0.0)
                    {
                        _7415 = _7451.hi >= 0.0;
                    }
                    if (_7415)
                    {
                        _7414 = 0.0;
                    }
                    else
                    {
                        _7414 = precise::min(abs(_7451.lo), abs(_7451.hi));
                    }
                    float _7413 = _7414;
                    float _7416 = precise::max(abs(_7451.lo), abs(_7451.hi));
                    float _7417 = spvFMul(_7413, _7413);
                    float _11005 = interval_down(_7417, intervalFailed);
                    float _7418 = precise::max(0.0, _11005);
                    float _7419 = spvFMul(_7416, _7416);
                    float _11009 = interval_up(_7419, intervalFailed);
                    float _7420 = _11009;
                    _7411.lo = _7418;
                    _7411.hi = _7420;
                    Interval _7412 = _7411;
                    Interval _7421 = _7412;
                    Interval _7452 = _7421;
                    Interval _11017 = iadd(_7450, _7452, intervalFailed);
                    Interval _7453 = _11017;
                    Interval _11018 = idiv(_7448, _7453, intervalFailed, interval_divide_upper);
                    Interval _7454 = _11018;
                    Interval _7739 = _7454;
                    float _7745 = 9.0;
                    float _7408 = _7745;
                    float _7409 = _7745;
                    _7406.lo = _7408;
                    _7406.hi = _7409;
                    Interval _7407 = _7406;
                    Interval _7410 = _7407;
                    Interval _7746 = _7410;
                    Interval _7747 = _7669;
                    float _7401 = _7747.lo;
                    float _7402 = _7747.hi;
                    float _7396 = _7401;
                    float _7397 = _7402;
                    _7393.lo = _7396;
                    _7393.hi = _7397;
                    Interval _7394 = _7393;
                    Interval _7398 = _7394;
                    Interval _11042 = isin_body(_7398, intervalFailed, optical_product_upper);
                    Interval _7395 = _11042;
                    interval_sine_upper = _7395.hi;
                    float _7399 = _7395.lo;
                    float _7400 = _7399;
                    float _7403 = _7400;
                    float _7404 = interval_sine_upper;
                    _7391.lo = _7403;
                    _7391.hi = _7404;
                    Interval _7392 = _7391;
                    Interval _7405 = _7392;
                    Interval _7748 = _7405;
                    Interval _11057 = imul(_7746, _7748, intervalFailed, optical_product_upper);
                    Interval _7749 = _11057;
                    Interval _7750 = _7729;
                    Interval _11059 = imul(_7749, _7750, intervalFailed, optical_product_upper);
                    Interval _7751 = _11059;
                    float _7752 = 2.5;
                    float _7388 = _7752;
                    float _7389 = _7752;
                    _7386.lo = _7388;
                    _7386.hi = _7389;
                    Interval _7387 = _7386;
                    Interval _7390 = _7387;
                    Interval _7753 = _7390;
                    Interval _7754 = _7695;
                    float _7381 = _7754.lo;
                    float _7382 = _7754.hi;
                    float _7376 = _7381;
                    float _7377 = _7382;
                    _7373.lo = _7376;
                    _7373.hi = _7377;
                    Interval _7374 = _7373;
                    Interval _7378 = _7374;
                    Interval _11082 = isin_body(_7378, intervalFailed, optical_product_upper);
                    Interval _7375 = _11082;
                    interval_sine_upper = _7375.hi;
                    float _7379 = _7375.lo;
                    float _7380 = _7379;
                    float _7383 = _7380;
                    float _7384 = interval_sine_upper;
                    _7371.lo = _7383;
                    _7371.hi = _7384;
                    Interval _7372 = _7371;
                    Interval _7385 = _7372;
                    Interval _7755 = _7385;
                    Interval _11097 = imul(_7753, _7755, intervalFailed, optical_product_upper);
                    Interval _7756 = _11097;
                    Interval _7757 = _7734;
                    Interval _11099 = imul(_7756, _7757, intervalFailed, optical_product_upper);
                    Interval _7758 = _11099;
                    Interval _11100 = iadd(_7751, _7758, intervalFailed);
                    Interval _7759 = _11100;
                    float _7760 = 5.0;
                    float _7368 = _7760;
                    float _7369 = _7760;
                    _7366.lo = _7368;
                    _7366.hi = _7369;
                    Interval _7367 = _7366;
                    Interval _7370 = _7367;
                    Interval _7761 = _7370;
                    Interval _7762 = _7712;
                    float _7361 = _7762.lo;
                    float _7362 = _7762.hi;
                    float _7356 = _7361;
                    float _7357 = _7362;
                    _7353.lo = _7356;
                    _7353.hi = _7357;
                    Interval _7354 = _7353;
                    Interval _7358 = _7354;
                    Interval _11123 = isin_body(_7358, intervalFailed, optical_product_upper);
                    Interval _7355 = _11123;
                    interval_sine_upper = _7355.hi;
                    float _7359 = _7355.lo;
                    float _7360 = _7359;
                    float _7363 = _7360;
                    float _7364 = interval_sine_upper;
                    _7351.lo = _7363;
                    _7351.hi = _7364;
                    Interval _7352 = _7351;
                    Interval _7365 = _7352;
                    Interval _7763 = _7365;
                    Interval _11138 = imul(_7761, _7763, intervalFailed, optical_product_upper);
                    Interval _7764 = _11138;
                    Interval _7765 = _7739;
                    Interval _11140 = imul(_7764, _7765, intervalFailed, optical_product_upper);
                    Interval _7766 = _11140;
                    Interval _11141 = iadd(_7759, _7766, intervalFailed);
                    Interval _7744 = _11141;
                    Interval _7768 = _7647;
                    Interval _7344 = _7768;
                    float _7342 = 3.1415927410125732421875;
                    float _7337 = _7342;
                    float _11145 = interval_down(_7337, intervalFailed);
                    float _7338 = _11145;
                    float _7339 = _7342;
                    float _11147 = interval_up(_7339, intervalFailed);
                    float _7340 = _11147;
                    _7335.lo = _7338;
                    _7335.hi = _7340;
                    Interval _7336 = _7335;
                    Interval _7341 = _7336;
                    Interval _7343 = _7341;
                    Interval _7345 = _7343;
                    float _7346 = 0.5;
                    float _7332 = _7346;
                    float _7333 = _7346;
                    _7330.lo = _7332;
                    _7330.hi = _7333;
                    Interval _7331 = _7330;
                    Interval _7334 = _7331;
                    Interval _7347 = _7334;
                    Interval _11165 = imul(_7345, _7347, intervalFailed, optical_product_upper);
                    Interval _7348 = _11165;
                    Interval _11166 = iadd(_7344, _7348, intervalFailed);
                    Interval _7349 = _11166;
                    float _7325 = _7349.lo;
                    float _7326 = _7349.hi;
                    float _7320 = _7325;
                    float _7321 = _7326;
                    _7317.lo = _7320;
                    _7317.hi = _7321;
                    Interval _7318 = _7317;
                    Interval _7322 = _7318;
                    Interval _11179 = isin_body(_7322, intervalFailed, optical_product_upper);
                    Interval _7319 = _11179;
                    interval_sine_upper = _7319.hi;
                    float _7323 = _7319.lo;
                    float _7324 = _7323;
                    float _7327 = _7324;
                    float _7328 = interval_sine_upper;
                    _7315.lo = _7327;
                    _7315.hi = _7328;
                    Interval _7316 = _7315;
                    Interval _7329 = _7316;
                    Interval _7350 = _7329;
                    Interval _7769 = _7350;
                    Interval _7770 = _7664;
                    Interval _11196 = imul(_7769, _7770, intervalFailed, optical_product_upper);
                    Interval _7767 = _11196;
                    float _7772 = 9.0;
                    float _7312 = _7772;
                    float _7313 = _7772;
                    _7310.lo = _7312;
                    _7310.hi = _7313;
                    Interval _7311 = _7310;
                    Interval _7314 = _7311;
                    Interval _7773 = _7314;
                    float _7774 = 18.0;
                    float _7775 = 1000.0;
                    Interval _11206 = iratio(_7774, _7775, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7776 = _11206;
                    float _7777 = 39.0;
                    float _7778 = 10000.0;
                    Interval _11207 = iratio(_7777, _7778, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7779 = _11207;
                    Interval _7780 = _7767;
                    Interval _11209 = imul(_7779, _7780, intervalFailed, optical_product_upper);
                    Interval _7781 = _11209;
                    Interval _11210 = iadd(_7776, _7781, intervalFailed);
                    Interval _7782 = _11210;
                    Interval _11211 = imul(_7773, _7782, intervalFailed, optical_product_upper);
                    Interval _7783 = _11211;
                    Interval _7784 = _7669;
                    Interval _7303 = _7784;
                    float _7301 = 3.1415927410125732421875;
                    float _7296 = _7301;
                    float _11215 = interval_down(_7296, intervalFailed);
                    float _7297 = _11215;
                    float _7298 = _7301;
                    float _11217 = interval_up(_7298, intervalFailed);
                    float _7299 = _11217;
                    _7294.lo = _7297;
                    _7294.hi = _7299;
                    Interval _7295 = _7294;
                    Interval _7300 = _7295;
                    Interval _7302 = _7300;
                    Interval _7304 = _7302;
                    float _7305 = 0.5;
                    float _7291 = _7305;
                    float _7292 = _7305;
                    _7289.lo = _7291;
                    _7289.hi = _7292;
                    Interval _7290 = _7289;
                    Interval _7293 = _7290;
                    Interval _7306 = _7293;
                    Interval _11235 = imul(_7304, _7306, intervalFailed, optical_product_upper);
                    Interval _7307 = _11235;
                    Interval _11236 = iadd(_7303, _7307, intervalFailed);
                    Interval _7308 = _11236;
                    float _7284 = _7308.lo;
                    float _7285 = _7308.hi;
                    float _7279 = _7284;
                    float _7280 = _7285;
                    _7276.lo = _7279;
                    _7276.hi = _7280;
                    Interval _7277 = _7276;
                    Interval _7281 = _7277;
                    Interval _11249 = isin_body(_7281, intervalFailed, optical_product_upper);
                    Interval _7278 = _11249;
                    interval_sine_upper = _7278.hi;
                    float _7282 = _7278.lo;
                    float _7283 = _7282;
                    float _7286 = _7283;
                    float _7287 = interval_sine_upper;
                    _7274.lo = _7286;
                    _7274.hi = _7287;
                    Interval _7275 = _7274;
                    Interval _7288 = _7275;
                    Interval _7309 = _7288;
                    Interval _7785 = _7309;
                    Interval _11265 = imul(_7783, _7785, intervalFailed, optical_product_upper);
                    Interval _7786 = _11265;
                    Interval _7787 = _7729;
                    Interval _11267 = imul(_7786, _7787, intervalFailed, optical_product_upper);
                    Interval _7788 = _11267;
                    float _7789 = 1175.0;
                    float _7790 = 10000.0;
                    Interval _11268 = iratio(_7789, _7790, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7791 = _11268;
                    Interval _7792 = _7695;
                    Interval _7267 = _7792;
                    float _7265 = 3.1415927410125732421875;
                    float _7260 = _7265;
                    float _11272 = interval_down(_7260, intervalFailed);
                    float _7261 = _11272;
                    float _7262 = _7265;
                    float _11274 = interval_up(_7262, intervalFailed);
                    float _7263 = _11274;
                    _7258.lo = _7261;
                    _7258.hi = _7263;
                    Interval _7259 = _7258;
                    Interval _7264 = _7259;
                    Interval _7266 = _7264;
                    Interval _7268 = _7266;
                    float _7269 = 0.5;
                    float _7255 = _7269;
                    float _7256 = _7269;
                    _7253.lo = _7255;
                    _7253.hi = _7256;
                    Interval _7254 = _7253;
                    Interval _7257 = _7254;
                    Interval _7270 = _7257;
                    Interval _11292 = imul(_7268, _7270, intervalFailed, optical_product_upper);
                    Interval _7271 = _11292;
                    Interval _11293 = iadd(_7267, _7271, intervalFailed);
                    Interval _7272 = _11293;
                    float _7248 = _7272.lo;
                    float _7249 = _7272.hi;
                    float _7243 = _7248;
                    float _7244 = _7249;
                    _7240.lo = _7243;
                    _7240.hi = _7244;
                    Interval _7241 = _7240;
                    Interval _7245 = _7241;
                    Interval _11306 = isin_body(_7245, intervalFailed, optical_product_upper);
                    Interval _7242 = _11306;
                    interval_sine_upper = _7242.hi;
                    float _7246 = _7242.lo;
                    float _7247 = _7246;
                    float _7250 = _7247;
                    float _7251 = interval_sine_upper;
                    _7238.lo = _7250;
                    _7238.hi = _7251;
                    Interval _7239 = _7238;
                    Interval _7252 = _7239;
                    Interval _7273 = _7252;
                    Interval _7793 = _7273;
                    Interval _11322 = imul(_7791, _7793, intervalFailed, optical_product_upper);
                    Interval _7794 = _11322;
                    Interval _7795 = _7734;
                    Interval _11324 = imul(_7794, _7795, intervalFailed, optical_product_upper);
                    Interval _7796 = _11324;
                    Interval _11325 = iadd(_7788, _7796, intervalFailed);
                    Interval _7797 = _11325;
                    float _7798 = 45.0;
                    float _7799 = 1000.0;
                    Interval _11326 = iratio(_7798, _7799, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7800 = _11326;
                    Interval _7801 = _7712;
                    Interval _7231 = _7801;
                    float _7229 = 3.1415927410125732421875;
                    float _7224 = _7229;
                    float _11330 = interval_down(_7224, intervalFailed);
                    float _7225 = _11330;
                    float _7226 = _7229;
                    float _11332 = interval_up(_7226, intervalFailed);
                    float _7227 = _11332;
                    _7222.lo = _7225;
                    _7222.hi = _7227;
                    Interval _7223 = _7222;
                    Interval _7228 = _7223;
                    Interval _7230 = _7228;
                    Interval _7232 = _7230;
                    float _7233 = 0.5;
                    float _7219 = _7233;
                    float _7220 = _7233;
                    _7217.lo = _7219;
                    _7217.hi = _7220;
                    Interval _7218 = _7217;
                    Interval _7221 = _7218;
                    Interval _7234 = _7221;
                    Interval _11350 = imul(_7232, _7234, intervalFailed, optical_product_upper);
                    Interval _7235 = _11350;
                    Interval _11351 = iadd(_7231, _7235, intervalFailed);
                    Interval _7236 = _11351;
                    float _7212 = _7236.lo;
                    float _7213 = _7236.hi;
                    float _7207 = _7212;
                    float _7208 = _7213;
                    _7204.lo = _7207;
                    _7204.hi = _7208;
                    Interval _7205 = _7204;
                    Interval _7209 = _7205;
                    Interval _11364 = isin_body(_7209, intervalFailed, optical_product_upper);
                    Interval _7206 = _11364;
                    interval_sine_upper = _7206.hi;
                    float _7210 = _7206.lo;
                    float _7211 = _7210;
                    float _7214 = _7211;
                    float _7215 = interval_sine_upper;
                    _7202.lo = _7214;
                    _7202.hi = _7215;
                    Interval _7203 = _7202;
                    Interval _7216 = _7203;
                    Interval _7237 = _7216;
                    Interval _7802 = _7237;
                    Interval _11380 = imul(_7800, _7802, intervalFailed, optical_product_upper);
                    Interval _7803 = _11380;
                    Interval _7804 = _7739;
                    Interval _11382 = imul(_7803, _7804, intervalFailed, optical_product_upper);
                    Interval _7805 = _11382;
                    Interval _7198 = _7797;
                    Interval _7199 = _7805;
                    float _7195 = as_type<float>(as_type<uint>(_7199.hi) ^ 2147483648u);
                    float _7196 = as_type<float>(as_type<uint>(_7199.lo) ^ 2147483648u);
                    _7193.lo = _7195;
                    _7193.hi = _7196;
                    Interval _7194 = _7193;
                    Interval _7197 = _7194;
                    Interval _7200 = _7197;
                    Interval _11402 = iadd(_7198, _7200, intervalFailed);
                    Interval _7201 = _11402;
                    Interval _7771 = _7201;
                    float _7807 = 9.0;
                    float _7190 = _7807;
                    float _7191 = _7807;
                    _7188.lo = _7190;
                    _7188.hi = _7191;
                    Interval _7189 = _7188;
                    Interval _7192 = _7189;
                    Interval _7808 = _7192;
                    float _7809 = 11.0;
                    float _7810 = 1000.0;
                    Interval _11413 = iratio(_7809, _7810, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7811 = _11413;
                    float _7812 = 52.0;
                    float _7813 = 10000.0;
                    Interval _11414 = iratio(_7812, _7813, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7814 = _11414;
                    Interval _7815 = _7767;
                    Interval _11416 = imul(_7814, _7815, intervalFailed, optical_product_upper);
                    Interval _7816 = _11416;
                    Interval _7184 = _7811;
                    Interval _7185 = _7816;
                    float _7181 = as_type<float>(as_type<uint>(_7185.hi) ^ 2147483648u);
                    float _7182 = as_type<float>(as_type<uint>(_7185.lo) ^ 2147483648u);
                    _7179.lo = _7181;
                    _7179.hi = _7182;
                    Interval _7180 = _7179;
                    Interval _7183 = _7180;
                    Interval _7186 = _7183;
                    Interval _11436 = iadd(_7184, _7186, intervalFailed);
                    Interval _7187 = _11436;
                    Interval _7817 = _7187;
                    Interval _11438 = imul(_7808, _7817, intervalFailed, optical_product_upper);
                    Interval _7818 = _11438;
                    Interval _7819 = _7669;
                    Interval _7172 = _7819;
                    float _7170 = 3.1415927410125732421875;
                    float _7165 = _7170;
                    float _11442 = interval_down(_7165, intervalFailed);
                    float _7166 = _11442;
                    float _7167 = _7170;
                    float _11444 = interval_up(_7167, intervalFailed);
                    float _7168 = _11444;
                    _7163.lo = _7166;
                    _7163.hi = _7168;
                    Interval _7164 = _7163;
                    Interval _7169 = _7164;
                    Interval _7171 = _7169;
                    Interval _7173 = _7171;
                    float _7174 = 0.5;
                    float _7160 = _7174;
                    float _7161 = _7174;
                    _7158.lo = _7160;
                    _7158.hi = _7161;
                    Interval _7159 = _7158;
                    Interval _7162 = _7159;
                    Interval _7175 = _7162;
                    Interval _11462 = imul(_7173, _7175, intervalFailed, optical_product_upper);
                    Interval _7176 = _11462;
                    Interval _11463 = iadd(_7172, _7176, intervalFailed);
                    Interval _7177 = _11463;
                    float _7153 = _7177.lo;
                    float _7154 = _7177.hi;
                    float _7148 = _7153;
                    float _7149 = _7154;
                    _7145.lo = _7148;
                    _7145.hi = _7149;
                    Interval _7146 = _7145;
                    Interval _7150 = _7146;
                    Interval _11476 = isin_body(_7150, intervalFailed, optical_product_upper);
                    Interval _7147 = _11476;
                    interval_sine_upper = _7147.hi;
                    float _7151 = _7147.lo;
                    float _7152 = _7151;
                    float _7155 = _7152;
                    float _7156 = interval_sine_upper;
                    _7143.lo = _7155;
                    _7143.hi = _7156;
                    Interval _7144 = _7143;
                    Interval _7157 = _7144;
                    Interval _7178 = _7157;
                    Interval _7820 = _7178;
                    Interval _11492 = imul(_7818, _7820, intervalFailed, optical_product_upper);
                    Interval _7821 = _11492;
                    Interval _7822 = _7729;
                    Interval _11494 = imul(_7821, _7822, intervalFailed, optical_product_upper);
                    Interval _7823 = _11494;
                    float _7824 = 625.0;
                    float _7825 = 10000.0;
                    Interval _11495 = iratio(_7824, _7825, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7826 = _11495;
                    Interval _7827 = _7695;
                    Interval _7136 = _7827;
                    float _7134 = 3.1415927410125732421875;
                    float _7129 = _7134;
                    float _11499 = interval_down(_7129, intervalFailed);
                    float _7130 = _11499;
                    float _7131 = _7134;
                    float _11501 = interval_up(_7131, intervalFailed);
                    float _7132 = _11501;
                    _7127.lo = _7130;
                    _7127.hi = _7132;
                    Interval _7128 = _7127;
                    Interval _7133 = _7128;
                    Interval _7135 = _7133;
                    Interval _7137 = _7135;
                    float _7138 = 0.5;
                    float _7124 = _7138;
                    float _7125 = _7138;
                    _7122.lo = _7124;
                    _7122.hi = _7125;
                    Interval _7123 = _7122;
                    Interval _7126 = _7123;
                    Interval _7139 = _7126;
                    Interval _11519 = imul(_7137, _7139, intervalFailed, optical_product_upper);
                    Interval _7140 = _11519;
                    Interval _11520 = iadd(_7136, _7140, intervalFailed);
                    Interval _7141 = _11520;
                    float _7117 = _7141.lo;
                    float _7118 = _7141.hi;
                    float _7112 = _7117;
                    float _7113 = _7118;
                    _7109.lo = _7112;
                    _7109.hi = _7113;
                    Interval _7110 = _7109;
                    Interval _7114 = _7110;
                    Interval _11533 = isin_body(_7114, intervalFailed, optical_product_upper);
                    Interval _7111 = _11533;
                    interval_sine_upper = _7111.hi;
                    float _7115 = _7111.lo;
                    float _7116 = _7115;
                    float _7119 = _7116;
                    float _7120 = interval_sine_upper;
                    _7107.lo = _7119;
                    _7107.hi = _7120;
                    Interval _7108 = _7107;
                    Interval _7121 = _7108;
                    Interval _7142 = _7121;
                    Interval _7828 = _7142;
                    Interval _11549 = imul(_7826, _7828, intervalFailed, optical_product_upper);
                    Interval _7829 = _11549;
                    Interval _7830 = _7734;
                    Interval _11551 = imul(_7829, _7830, intervalFailed, optical_product_upper);
                    Interval _7831 = _11551;
                    Interval _7103 = _7823;
                    Interval _7104 = _7831;
                    float _7100 = as_type<float>(as_type<uint>(_7104.hi) ^ 2147483648u);
                    float _7101 = as_type<float>(as_type<uint>(_7104.lo) ^ 2147483648u);
                    _7098.lo = _7100;
                    _7098.hi = _7101;
                    Interval _7099 = _7098;
                    Interval _7102 = _7099;
                    Interval _7105 = _7102;
                    Interval _11571 = iadd(_7103, _7105, intervalFailed);
                    Interval _7106 = _11571;
                    Interval _7832 = _7106;
                    float _7833 = 110.0;
                    float _7834 = 1000.0;
                    Interval _11573 = iratio(_7833, _7834, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7835 = _11573;
                    Interval _7836 = _7712;
                    Interval _7091 = _7836;
                    float _7089 = 3.1415927410125732421875;
                    float _7084 = _7089;
                    float _11577 = interval_down(_7084, intervalFailed);
                    float _7085 = _11577;
                    float _7086 = _7089;
                    float _11579 = interval_up(_7086, intervalFailed);
                    float _7087 = _11579;
                    _7082.lo = _7085;
                    _7082.hi = _7087;
                    Interval _7083 = _7082;
                    Interval _7088 = _7083;
                    Interval _7090 = _7088;
                    Interval _7092 = _7090;
                    float _7093 = 0.5;
                    float _7079 = _7093;
                    float _7080 = _7093;
                    _7077.lo = _7079;
                    _7077.hi = _7080;
                    Interval _7078 = _7077;
                    Interval _7081 = _7078;
                    Interval _7094 = _7081;
                    Interval _11597 = imul(_7092, _7094, intervalFailed, optical_product_upper);
                    Interval _7095 = _11597;
                    Interval _11598 = iadd(_7091, _7095, intervalFailed);
                    Interval _7096 = _11598;
                    float _7072 = _7096.lo;
                    float _7073 = _7096.hi;
                    float _7067 = _7072;
                    float _7068 = _7073;
                    _7064.lo = _7067;
                    _7064.hi = _7068;
                    Interval _7065 = _7064;
                    Interval _7069 = _7065;
                    Interval _11611 = isin_body(_7069, intervalFailed, optical_product_upper);
                    Interval _7066 = _11611;
                    interval_sine_upper = _7066.hi;
                    float _7070 = _7066.lo;
                    float _7071 = _7070;
                    float _7074 = _7071;
                    float _7075 = interval_sine_upper;
                    _7062.lo = _7074;
                    _7062.hi = _7075;
                    Interval _7063 = _7062;
                    Interval _7076 = _7063;
                    Interval _7097 = _7076;
                    Interval _7837 = _7097;
                    Interval _11627 = imul(_7835, _7837, intervalFailed, optical_product_upper);
                    Interval _7838 = _11627;
                    Interval _7839 = _7739;
                    Interval _11629 = imul(_7838, _7839, intervalFailed, optical_product_upper);
                    Interval _7840 = _11629;
                    Interval _11630 = iadd(_7832, _7840, intervalFailed);
                    Interval _7806 = _11630;
                    Interval _7842 = _8403;
                    float _7843 = 173.0;
                    float _7844 = 1000.0;
                    Interval _11632 = iratio(_7843, _7844, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7845 = _11632;
                    Interval _11633 = imul(_7842, _7845, intervalFailed, optical_product_upper);
                    Interval _7846 = _11633;
                    Interval _7847 = _8404;
                    float _7848 = 129.0;
                    float _7849 = 1000.0;
                    Interval _11635 = iratio(_7848, _7849, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7850 = _11635;
                    Interval _11636 = imul(_7847, _7850, intervalFailed, optical_product_upper);
                    Interval _7851 = _11636;
                    Interval _11637 = iadd(_7846, _7851, intervalFailed);
                    Interval _7852 = _11637;
                    Interval _7853 = _8405;
                    float _7854 = 73.0;
                    float _7855 = 100.0;
                    Interval _11639 = iratio(_7854, _7855, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7856 = _11639;
                    Interval _11640 = imul(_7853, _7856, intervalFailed, optical_product_upper);
                    Interval _7857 = _11640;
                    Interval _7058 = _7852;
                    Interval _7059 = _7857;
                    float _7055 = as_type<float>(as_type<uint>(_7059.hi) ^ 2147483648u);
                    float _7056 = as_type<float>(as_type<uint>(_7059.lo) ^ 2147483648u);
                    _7053.lo = _7055;
                    _7053.hi = _7056;
                    Interval _7054 = _7053;
                    Interval _7057 = _7054;
                    Interval _7060 = _7057;
                    Interval _11660 = iadd(_7058, _7060, intervalFailed);
                    Interval _7061 = _11660;
                    Interval _7841 = _7061;
                    Interval _7859 = _8406;
                    float _7860 = 216.0;
                    float _7861 = 1000.0;
                    Interval _11663 = iratio(_7860, _7861, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7862 = _11663;
                    Interval _7042 = _7859;
                    Interval _7043 = _7862;
                    Interval _11666 = imul(_7042, _7043, intervalFailed, optical_product_upper);
                    Interval _7044 = _11666;
                    bool _7034 = false;
                    if (_7044.lo <= 0.0)
                    {
                        _7034 = _7044.hi >= 0.0;
                    }
                    if (_7034)
                    {
                        _7033 = 0.0;
                    }
                    else
                    {
                        _7033 = precise::min(abs(_7044.lo), abs(_7044.hi));
                    }
                    float _7032 = _7033;
                    float _7035 = precise::max(abs(_7044.lo), abs(_7044.hi));
                    float _7036 = spvFMul(_7032, _7032);
                    float _11696 = interval_down(_7036, intervalFailed);
                    float _7037 = precise::max(0.0, _11696);
                    float _7038 = spvFMul(_7035, _7035);
                    float _11700 = interval_up(_7038, intervalFailed);
                    float _7039 = _11700;
                    _7030.lo = _7037;
                    _7030.hi = _7039;
                    Interval _7031 = _7030;
                    Interval _7040 = _7031;
                    Interval _7041 = _7040;
                    float _7045 = 1.0;
                    float _7027 = _7045;
                    float _7028 = _7045;
                    _7025.lo = _7027;
                    _7025.hi = _7028;
                    Interval _7026 = _7025;
                    Interval _7029 = _7026;
                    Interval _7046 = _7029;
                    float _7047 = 1.0;
                    float _7022 = _7047;
                    float _7023 = _7047;
                    _7020.lo = _7022;
                    _7020.hi = _7023;
                    Interval _7021 = _7020;
                    Interval _7024 = _7021;
                    Interval _7048 = _7024;
                    Interval _7049 = _7041;
                    bool _7013 = false;
                    if (_7049.lo <= 0.0)
                    {
                        _7013 = _7049.hi >= 0.0;
                    }
                    if (_7013)
                    {
                        _7012 = 0.0;
                    }
                    else
                    {
                        _7012 = precise::min(abs(_7049.lo), abs(_7049.hi));
                    }
                    float _7011 = _7012;
                    float _7014 = precise::max(abs(_7049.lo), abs(_7049.hi));
                    float _7015 = spvFMul(_7011, _7011);
                    float _11756 = interval_down(_7015, intervalFailed);
                    float _7016 = precise::max(0.0, _11756);
                    float _7017 = spvFMul(_7014, _7014);
                    float _11760 = interval_up(_7017, intervalFailed);
                    float _7018 = _11760;
                    _7009.lo = _7016;
                    _7009.hi = _7018;
                    Interval _7010 = _7009;
                    Interval _7019 = _7010;
                    Interval _7050 = _7019;
                    Interval _11768 = iadd(_7048, _7050, intervalFailed);
                    Interval _7051 = _11768;
                    Interval _11769 = idiv(_7046, _7051, intervalFailed, interval_divide_upper);
                    Interval _7052 = _11769;
                    Interval _7858 = _7052;
                    Interval _7863 = _7744;
                    float _7864 = 38.0;
                    float _7865 = 100.0;
                    Interval _11772 = iratio(_7864, _7865, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7866 = _11772;
                    Interval _7867 = _7841;
                    float _7004 = _7867.lo;
                    float _7005 = _7867.hi;
                    float _6999 = _7004;
                    float _7000 = _7005;
                    _6996.lo = _6999;
                    _6996.hi = _7000;
                    Interval _6997 = _6996;
                    Interval _7001 = _6997;
                    Interval _11786 = isin_body(_7001, intervalFailed, optical_product_upper);
                    Interval _6998 = _11786;
                    interval_sine_upper = _6998.hi;
                    float _7002 = _6998.lo;
                    float _7003 = _7002;
                    float _7006 = _7003;
                    float _7007 = interval_sine_upper;
                    _6994.lo = _7006;
                    _6994.hi = _7007;
                    Interval _6995 = _6994;
                    Interval _7008 = _6995;
                    Interval _7868 = _7008;
                    Interval _11801 = imul(_7866, _7868, intervalFailed, optical_product_upper);
                    Interval _7869 = _11801;
                    Interval _7870 = _7858;
                    Interval _11803 = imul(_7869, _7870, intervalFailed, optical_product_upper);
                    Interval _7871 = _11803;
                    Interval _11804 = iadd(_7863, _7871, intervalFailed);
                    _7744 = _11804;
                    Interval _7872 = _7771;
                    float _7873 = 6574.0;
                    float _7874 = 100000.0;
                    Interval _11806 = iratio(_7873, _7874, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7875 = _11806;
                    Interval _7876 = _7841;
                    Interval _6987 = _7876;
                    float _6985 = 3.1415927410125732421875;
                    float _6980 = _6985;
                    float _11810 = interval_down(_6980, intervalFailed);
                    float _6981 = _11810;
                    float _6982 = _6985;
                    float _11812 = interval_up(_6982, intervalFailed);
                    float _6983 = _11812;
                    _6978.lo = _6981;
                    _6978.hi = _6983;
                    Interval _6979 = _6978;
                    Interval _6984 = _6979;
                    Interval _6986 = _6984;
                    Interval _6988 = _6986;
                    float _6989 = 0.5;
                    float _6975 = _6989;
                    float _6976 = _6989;
                    _6973.lo = _6975;
                    _6973.hi = _6976;
                    Interval _6974 = _6973;
                    Interval _6977 = _6974;
                    Interval _6990 = _6977;
                    Interval _11830 = imul(_6988, _6990, intervalFailed, optical_product_upper);
                    Interval _6991 = _11830;
                    Interval _11831 = iadd(_6987, _6991, intervalFailed);
                    Interval _6992 = _11831;
                    float _6968 = _6992.lo;
                    float _6969 = _6992.hi;
                    float _6963 = _6968;
                    float _6964 = _6969;
                    _6960.lo = _6963;
                    _6960.hi = _6964;
                    Interval _6961 = _6960;
                    Interval _6965 = _6961;
                    Interval _11844 = isin_body(_6965, intervalFailed, optical_product_upper);
                    Interval _6962 = _11844;
                    interval_sine_upper = _6962.hi;
                    float _6966 = _6962.lo;
                    float _6967 = _6966;
                    float _6970 = _6967;
                    float _6971 = interval_sine_upper;
                    _6958.lo = _6970;
                    _6958.hi = _6971;
                    Interval _6959 = _6958;
                    Interval _6972 = _6959;
                    Interval _6993 = _6972;
                    Interval _7877 = _6993;
                    Interval _11860 = imul(_7875, _7877, intervalFailed, optical_product_upper);
                    Interval _7878 = _11860;
                    Interval _7879 = _7858;
                    Interval _11862 = imul(_7878, _7879, intervalFailed, optical_product_upper);
                    Interval _7880 = _11862;
                    Interval _11863 = iadd(_7872, _7880, intervalFailed);
                    _7771 = _11863;
                    Interval _7881 = _7806;
                    float _7882 = 4902.0;
                    float _7883 = 100000.0;
                    Interval _11865 = iratio(_7882, _7883, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7884 = _11865;
                    Interval _7885 = _7841;
                    Interval _6951 = _7885;
                    float _6949 = 3.1415927410125732421875;
                    float _6944 = _6949;
                    float _11869 = interval_down(_6944, intervalFailed);
                    float _6945 = _11869;
                    float _6946 = _6949;
                    float _11871 = interval_up(_6946, intervalFailed);
                    float _6947 = _11871;
                    _6942.lo = _6945;
                    _6942.hi = _6947;
                    Interval _6943 = _6942;
                    Interval _6948 = _6943;
                    Interval _6950 = _6948;
                    Interval _6952 = _6950;
                    float _6953 = 0.5;
                    float _6939 = _6953;
                    float _6940 = _6953;
                    _6937.lo = _6939;
                    _6937.hi = _6940;
                    Interval _6938 = _6937;
                    Interval _6941 = _6938;
                    Interval _6954 = _6941;
                    Interval _11889 = imul(_6952, _6954, intervalFailed, optical_product_upper);
                    Interval _6955 = _11889;
                    Interval _11890 = iadd(_6951, _6955, intervalFailed);
                    Interval _6956 = _11890;
                    float _6932 = _6956.lo;
                    float _6933 = _6956.hi;
                    float _6927 = _6932;
                    float _6928 = _6933;
                    _6924.lo = _6927;
                    _6924.hi = _6928;
                    Interval _6925 = _6924;
                    Interval _6929 = _6925;
                    Interval _11903 = isin_body(_6929, intervalFailed, optical_product_upper);
                    Interval _6926 = _11903;
                    interval_sine_upper = _6926.hi;
                    float _6930 = _6926.lo;
                    float _6931 = _6930;
                    float _6934 = _6931;
                    float _6935 = interval_sine_upper;
                    _6922.lo = _6934;
                    _6922.hi = _6935;
                    Interval _6923 = _6922;
                    Interval _6936 = _6923;
                    Interval _6957 = _6936;
                    Interval _7886 = _6957;
                    Interval _11919 = imul(_7884, _7886, intervalFailed, optical_product_upper);
                    Interval _7887 = _11919;
                    Interval _7888 = _7858;
                    Interval _11921 = imul(_7887, _7888, intervalFailed, optical_product_upper);
                    Interval _7889 = _11921;
                    Interval _11922 = iadd(_7881, _7889, intervalFailed);
                    _7806 = _11922;
                    if (_8406.lo < 24.0)
                    {
                        Interval _7891 = _8403;
                        float _7892 = 128.0;
                        float _6919 = _7892;
                        float _6920 = _7892;
                        _6917.lo = _6919;
                        _6917.hi = _6920;
                        Interval _6918 = _6917;
                        Interval _6921 = _6918;
                        Interval _7893 = _6921;
                        Interval _11938 = idiv(_7891, _7893, intervalFailed, interval_divide_upper);
                        Interval _7890 = _11938;
                        Interval _7895 = _8404;
                        float _7896 = 128.0;
                        float _6914 = _7896;
                        float _6915 = _7896;
                        _6912.lo = _6914;
                        _6912.hi = _6915;
                        Interval _6913 = _6912;
                        Interval _6916 = _6913;
                        Interval _7897 = _6916;
                        Interval _11949 = idiv(_7895, _7897, intervalFailed, interval_divide_upper);
                        Interval _7894 = _11949;
                        float _11952 = floor(_7890.lo);
                        float _11955 = floor(_7890.hi);
                        bool _7898 = true;
                        if ((isunordered(_11952, _11955) || _11952 == _11955))
                        {
                            _7898 = floor(_7894.lo) != floor(_7894.hi);
                        }
                        if (_7898)
                        {
                            Interval _7899 = _7744;
                            float _7900 = 0.0;
                            float _7901 = 10.0;
                            _6910.lo = _7900;
                            _6910.hi = _7901;
                            Interval _6911 = _6910;
                            Interval _7902 = _6911;
                            Interval _11977 = iadd(_7899, _7902, intervalFailed);
                            _7744 = _11977;
                            Interval _7903 = _7771;
                            float _7904 = -256.0;
                            float _7905 = 256.0;
                            _6908.lo = _7904;
                            _6908.hi = _7905;
                            Interval _6909 = _6908;
                            Interval _7906 = _6909;
                            Interval _11985 = iadd(_7903, _7906, intervalFailed);
                            _7771 = _11985;
                            Interval _7907 = _7806;
                            float _7908 = -256.0;
                            float _7909 = 256.0;
                            _6906.lo = _7908;
                            _6906.hi = _7909;
                            Interval _6907 = _6906;
                            Interval _7910 = _6907;
                            Interval _11993 = iadd(_7907, _7910, intervalFailed);
                            _7806 = _11993;
                        }
                        else
                        {
                            int _7911 = int(floor(_7890.lo));
                            int _7912 = int(floor(_7894.lo));
                            int _7914 = _7911;
                            int _7915 = _7912;
                            uint _6902 = (uint(_7914) * 1597334677u) ^ (uint(_7915) * 3812015801u);
                            _6902 ^= (_6902 >> 16u);
                            _6902 *= 2246822519u;
                            _6902 ^= (_6902 >> 13u);
                            float _6903 = float(_6902 & 65535u);
                            float _6904 = 65535.0;
                            Interval _12021 = iratio(_6903, _6904, intervalFailed, optical_product_upper, interval_divide_upper);
                            Interval _6905 = _12021;
                            Interval _7913 = _6905;
                            if (_7913.hi > 0.63999998569488525390625)
                            {
                                Interval _7917 = _7890;
                                float _7918 = float(_7911);
                                float _6899 = _7918;
                                float _6900 = _7918;
                                _6897.lo = _6899;
                                _6897.hi = _6900;
                                Interval _6898 = _6897;
                                Interval _6901 = _6898;
                                Interval _7919 = _6901;
                                Interval _6893 = _7917;
                                Interval _6894 = _7919;
                                float _6890 = as_type<float>(as_type<uint>(_6894.hi) ^ 2147483648u);
                                float _6891 = as_type<float>(as_type<uint>(_6894.lo) ^ 2147483648u);
                                _6888.lo = _6890;
                                _6888.hi = _6891;
                                Interval _6889 = _6888;
                                Interval _6892 = _6889;
                                Interval _6895 = _6892;
                                Interval _12059 = iadd(_6893, _6895, intervalFailed);
                                Interval _6896 = _12059;
                                Interval _7920 = _6896;
                                float _7921 = 28.0;
                                float _7922 = 100.0;
                                Interval _12061 = iratio(_7921, _7922, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7923 = _12061;
                                float _7924 = 44.0;
                                float _7925 = 100.0;
                                Interval _12062 = iratio(_7924, _7925, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7926 = _12062;
                                int _7927 = _7911 + 19;
                                int _7928 = _7912;
                                uint _6884 = (uint(_7927) * 1597334677u) ^ (uint(_7928) * 3812015801u);
                                _6884 ^= (_6884 >> 16u);
                                _6884 *= 2246822519u;
                                _6884 ^= (_6884 >> 13u);
                                float _6885 = float(_6884 & 65535u);
                                float _6886 = 65535.0;
                                Interval _12082 = iratio(_6885, _6886, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6887 = _12082;
                                Interval _7929 = _6887;
                                Interval _12084 = imul(_7926, _7929, intervalFailed, optical_product_upper);
                                Interval _7930 = _12084;
                                Interval _12085 = iadd(_7923, _7930, intervalFailed);
                                Interval _7931 = _12085;
                                Interval _6880 = _7920;
                                Interval _6881 = _7931;
                                float _6877 = as_type<float>(as_type<uint>(_6881.hi) ^ 2147483648u);
                                float _6878 = as_type<float>(as_type<uint>(_6881.lo) ^ 2147483648u);
                                _6875.lo = _6877;
                                _6875.hi = _6878;
                                Interval _6876 = _6875;
                                Interval _6879 = _6876;
                                Interval _6882 = _6879;
                                Interval _12105 = iadd(_6880, _6882, intervalFailed);
                                Interval _6883 = _12105;
                                Interval _7916 = _6883;
                                Interval _7933 = _7894;
                                float _7934 = float(_7912);
                                float _6872 = _7934;
                                float _6873 = _7934;
                                _6870.lo = _6872;
                                _6870.hi = _6873;
                                Interval _6871 = _6870;
                                Interval _6874 = _6871;
                                Interval _7935 = _6874;
                                Interval _6866 = _7933;
                                Interval _6867 = _7935;
                                float _6863 = as_type<float>(as_type<uint>(_6867.hi) ^ 2147483648u);
                                float _6864 = as_type<float>(as_type<uint>(_6867.lo) ^ 2147483648u);
                                _6861.lo = _6863;
                                _6861.hi = _6864;
                                Interval _6862 = _6861;
                                Interval _6865 = _6862;
                                Interval _6868 = _6865;
                                Interval _12138 = iadd(_6866, _6868, intervalFailed);
                                Interval _6869 = _12138;
                                Interval _7936 = _6869;
                                float _7937 = 28.0;
                                float _7938 = 100.0;
                                Interval _12140 = iratio(_7937, _7938, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7939 = _12140;
                                float _7940 = 44.0;
                                float _7941 = 100.0;
                                Interval _12141 = iratio(_7940, _7941, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7942 = _12141;
                                int _7943 = _7911;
                                int _7944 = _7912 + 29;
                                uint _6857 = (uint(_7943) * 1597334677u) ^ (uint(_7944) * 3812015801u);
                                _6857 ^= (_6857 >> 16u);
                                _6857 *= 2246822519u;
                                _6857 ^= (_6857 >> 13u);
                                float _6858 = float(_6857 & 65535u);
                                float _6859 = 65535.0;
                                Interval _12161 = iratio(_6858, _6859, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6860 = _12161;
                                Interval _7945 = _6860;
                                Interval _12163 = imul(_7942, _7945, intervalFailed, optical_product_upper);
                                Interval _7946 = _12163;
                                Interval _12164 = iadd(_7939, _7946, intervalFailed);
                                Interval _7947 = _12164;
                                Interval _6853 = _7936;
                                Interval _6854 = _7947;
                                float _6850 = as_type<float>(as_type<uint>(_6854.hi) ^ 2147483648u);
                                float _6851 = as_type<float>(as_type<uint>(_6854.lo) ^ 2147483648u);
                                _6848.lo = _6850;
                                _6848.hi = _6851;
                                Interval _6849 = _6848;
                                Interval _6852 = _6849;
                                Interval _6855 = _6852;
                                Interval _12184 = iadd(_6853, _6855, intervalFailed);
                                Interval _6856 = _12184;
                                Interval _7932 = _6856;
                                Interval _7949 = _8405;
                                float _7950 = 14.0;
                                float _7951 = 100.0;
                                Interval _12187 = iratio(_7950, _7951, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7952 = _12187;
                                Interval _12188 = imul(_7949, _7952, intervalFailed, optical_product_upper);
                                Interval _7953 = _12188;
                                Interval _7954 = _7913;
                                float _7955 = 7.0;
                                float _6845 = _7955;
                                float _6846 = _7955;
                                _6843.lo = _6845;
                                _6843.hi = _6846;
                                Interval _6844 = _6843;
                                Interval _6847 = _6844;
                                Interval _7956 = _6847;
                                Interval _12199 = imul(_7954, _7956, intervalFailed, optical_product_upper);
                                Interval _7957 = _12199;
                                Interval _12200 = iadd(_7953, _7957, intervalFailed);
                                Interval _7948 = _12200;
                                if (floor(_7948.lo) == floor(_7948.hi))
                                {
                                    Interval _7958 = _7948;
                                    float _7959 = floor(_7948.lo);
                                    float _6840 = _7959;
                                    float _6841 = _7959;
                                    _6838.lo = _6840;
                                    _6838.hi = _6841;
                                    Interval _6839 = _6838;
                                    Interval _6842 = _6839;
                                    Interval _7960 = _6842;
                                    Interval _6834 = _7958;
                                    Interval _6835 = _7960;
                                    float _6831 = as_type<float>(as_type<uint>(_6835.hi) ^ 2147483648u);
                                    float _6832 = as_type<float>(as_type<uint>(_6835.lo) ^ 2147483648u);
                                    _6829.lo = _6831;
                                    _6829.hi = _6832;
                                    Interval _6830 = _6829;
                                    Interval _6833 = _6830;
                                    Interval _6836 = _6833;
                                    Interval _12243 = iadd(_6834, _6836, intervalFailed);
                                    Interval _6837 = _12243;
                                    _7948 = _6837;
                                }
                                else
                                {
                                    float _7961 = 0.0;
                                    float _7962 = 1.0;
                                    _6827.lo = _7961;
                                    _6827.hi = _7962;
                                    Interval _6828 = _6827;
                                    _7948 = _6828;
                                }
                                Interval _7964 = _7948;
                                float _7965 = 3.1415927410125732421875;
                                float _6822 = _7965;
                                float _12253 = interval_down(_6822, intervalFailed);
                                float _6823 = _12253;
                                float _6824 = _7965;
                                float _12255 = interval_up(_6824, intervalFailed);
                                float _6825 = _12255;
                                _6820.lo = _6823;
                                _6820.hi = _6825;
                                Interval _6821 = _6820;
                                Interval _6826 = _6821;
                                Interval _7966 = _6826;
                                Interval _12263 = imul(_7964, _7966, intervalFailed, optical_product_upper);
                                Interval _7967 = _12263;
                                float _6815 = _7967.lo;
                                float _6816 = _7967.hi;
                                float _6810 = _6815;
                                float _6811 = _6816;
                                _6807.lo = _6810;
                                _6807.hi = _6811;
                                Interval _6808 = _6807;
                                Interval _6812 = _6808;
                                Interval _12276 = isin_body(_6812, intervalFailed, optical_product_upper);
                                Interval _6809 = _12276;
                                interval_sine_upper = _6809.hi;
                                float _6813 = _6809.lo;
                                float _6814 = _6813;
                                float _6817 = _6814;
                                float _6818 = interval_sine_upper;
                                _6805.lo = _6817;
                                _6805.hi = _6818;
                                Interval _6806 = _6805;
                                Interval _6819 = _6806;
                                Interval _7968 = _6819;
                                bool _6798 = false;
                                if (_7968.lo <= 0.0)
                                {
                                    _6798 = _7968.hi >= 0.0;
                                }
                                if (_6798)
                                {
                                    _6797 = 0.0;
                                }
                                else
                                {
                                    _6797 = precise::min(abs(_7968.lo), abs(_7968.hi));
                                }
                                float _6796 = _6797;
                                float _6799 = precise::max(abs(_7968.lo), abs(_7968.hi));
                                float _6800 = spvFMul(_6796, _6796);
                                float _12320 = interval_down(_6800, intervalFailed);
                                float _6801 = precise::max(0.0, _12320);
                                float _6802 = spvFMul(_6799, _6799);
                                float _12324 = interval_up(_6802, intervalFailed);
                                float _6803 = _12324;
                                _6794.lo = _6801;
                                _6794.hi = _6803;
                                Interval _6795 = _6794;
                                Interval _6804 = _6795;
                                Interval _7963 = _6804;
                                float _7970 = 55.0;
                                float _7971 = 1000.0;
                                Interval _12332 = iratio(_7970, _7971, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7972 = _12332;
                                float _7973 = 14.0;
                                float _7974 = 100.0;
                                Interval _12333 = iratio(_7973, _7974, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7975 = _12333;
                                Interval _7976 = _7948;
                                Interval _12335 = imul(_7975, _7976, intervalFailed, optical_product_upper);
                                Interval _7977 = _12335;
                                Interval _12336 = iadd(_7972, _7977, intervalFailed);
                                Interval _7969 = _12336;
                                Interval _7979 = _7969;
                                bool _6787 = false;
                                if (_7979.lo <= 0.0)
                                {
                                    _6787 = _7979.hi >= 0.0;
                                }
                                if (_6787)
                                {
                                    _6786 = 0.0;
                                }
                                else
                                {
                                    _6786 = precise::min(abs(_7979.lo), abs(_7979.hi));
                                }
                                float _6785 = _6786;
                                float _6788 = precise::max(abs(_7979.lo), abs(_7979.hi));
                                float _6789 = spvFMul(_6785, _6785);
                                float _12367 = interval_down(_6789, intervalFailed);
                                float _6790 = precise::max(0.0, _12367);
                                float _6791 = spvFMul(_6788, _6788);
                                float _12371 = interval_up(_6791, intervalFailed);
                                float _6792 = _12371;
                                _6783.lo = _6790;
                                _6783.hi = _6792;
                                Interval _6784 = _6783;
                                Interval _6793 = _6784;
                                Interval _7978 = _6793;
                                float _7981 = 1.0;
                                float _6780 = _7981;
                                float _6781 = _7981;
                                _6778.lo = _6780;
                                _6778.hi = _6781;
                                Interval _6779 = _6778;
                                Interval _6782 = _6779;
                                Interval _7982 = _6782;
                                Interval _7983 = _7916;
                                bool _6771 = false;
                                if (_7983.lo <= 0.0)
                                {
                                    _6771 = _7983.hi >= 0.0;
                                }
                                if (_6771)
                                {
                                    _6770 = 0.0;
                                }
                                else
                                {
                                    _6770 = precise::min(abs(_7983.lo), abs(_7983.hi));
                                }
                                float _6769 = _6770;
                                float _6772 = precise::max(abs(_7983.lo), abs(_7983.hi));
                                float _6773 = spvFMul(_6769, _6769);
                                float _12418 = interval_down(_6773, intervalFailed);
                                float _6774 = precise::max(0.0, _12418);
                                float _6775 = spvFMul(_6772, _6772);
                                float _12422 = interval_up(_6775, intervalFailed);
                                float _6776 = _12422;
                                _6767.lo = _6774;
                                _6767.hi = _6776;
                                Interval _6768 = _6767;
                                Interval _6777 = _6768;
                                Interval _7984 = _6777;
                                Interval _7985 = _7932;
                                bool _6760 = false;
                                if (_7985.lo <= 0.0)
                                {
                                    _6760 = _7985.hi >= 0.0;
                                }
                                if (_6760)
                                {
                                    _6759 = 0.0;
                                }
                                else
                                {
                                    _6759 = precise::min(abs(_7985.lo), abs(_7985.hi));
                                }
                                float _6758 = _6759;
                                float _6761 = precise::max(abs(_7985.lo), abs(_7985.hi));
                                float _6762 = spvFMul(_6758, _6758);
                                float _12460 = interval_down(_6762, intervalFailed);
                                float _6763 = precise::max(0.0, _12460);
                                float _6764 = spvFMul(_6761, _6761);
                                float _12464 = interval_up(_6764, intervalFailed);
                                float _6765 = _12464;
                                _6756.lo = _6763;
                                _6756.hi = _6765;
                                Interval _6757 = _6756;
                                Interval _6766 = _6757;
                                Interval _7986 = _6766;
                                Interval _12472 = iadd(_7984, _7986, intervalFailed);
                                Interval _7987 = _12472;
                                Interval _7988 = _7978;
                                Interval _12474 = idiv(_7987, _7988, intervalFailed, interval_divide_upper);
                                Interval _7989 = _12474;
                                Interval _6752 = _7982;
                                Interval _6753 = _7989;
                                float _6749 = as_type<float>(as_type<uint>(_6753.hi) ^ 2147483648u);
                                float _6750 = as_type<float>(as_type<uint>(_6753.lo) ^ 2147483648u);
                                _6747.lo = _6749;
                                _6747.hi = _6750;
                                Interval _6748 = _6747;
                                Interval _6751 = _6748;
                                Interval _6754 = _6751;
                                Interval _12494 = iadd(_6752, _6754, intervalFailed);
                                Interval _6755 = _12494;
                                Interval _7990 = _6755;
                                float _7991 = 0.0;
                                float _6744 = _7991;
                                float _6745 = _7991;
                                _6742.lo = _6744;
                                _6742.hi = _6745;
                                Interval _6743 = _6742;
                                Interval _6746 = _6743;
                                Interval _7992 = _6746;
                                float _7993 = 1.0;
                                float _6739 = _7993;
                                float _6740 = _7993;
                                _6737.lo = _6739;
                                _6737.hi = _6740;
                                Interval _6738 = _6737;
                                Interval _6741 = _6738;
                                Interval _7994 = _6741;
                                Interval _6732 = _7990;
                                Interval _6733 = _7992;
                                float _6729 = precise::max(_6732.lo, _6733.lo);
                                float _6730 = precise::max(_6732.hi, _6733.hi);
                                _6727.lo = _6729;
                                _6727.hi = _6730;
                                Interval _6728 = _6727;
                                Interval _6731 = _6728;
                                Interval _6734 = _6731;
                                Interval _6735 = _7994;
                                float _6724 = precise::min(_6734.lo, _6735.lo);
                                float _6725 = precise::min(_6734.hi, _6735.hi);
                                _6722.lo = _6724;
                                _6722.hi = _6725;
                                Interval _6723 = _6722;
                                Interval _6726 = _6723;
                                Interval _6736 = _6726;
                                Interval _7980 = _6736;
                                float _7996 = 10.0;
                                float _6719 = _7996;
                                float _6720 = _7996;
                                _6717.lo = _6719;
                                _6717.hi = _6720;
                                Interval _6718 = _6717;
                                Interval _6721 = _6718;
                                Interval _7997 = _6721;
                                Interval _7998 = _7963;
                                Interval _12562 = imul(_7997, _7998, intervalFailed, optical_product_upper);
                                Interval _7999 = _12562;
                                Interval _8000 = _8406;
                                float _8001 = 18.0;
                                float _8002 = 100.0;
                                Interval _12564 = iratio(_8001, _8002, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8003 = _12564;
                                Interval _6706 = _8000;
                                Interval _6707 = _8003;
                                Interval _12567 = imul(_6706, _6707, intervalFailed, optical_product_upper);
                                Interval _6708 = _12567;
                                bool _6698 = false;
                                if (_6708.lo <= 0.0)
                                {
                                    _6698 = _6708.hi >= 0.0;
                                }
                                if (_6698)
                                {
                                    _6697 = 0.0;
                                }
                                else
                                {
                                    _6697 = precise::min(abs(_6708.lo), abs(_6708.hi));
                                }
                                float _6696 = _6697;
                                float _6699 = precise::max(abs(_6708.lo), abs(_6708.hi));
                                float _6700 = spvFMul(_6696, _6696);
                                float _12597 = interval_down(_6700, intervalFailed);
                                float _6701 = precise::max(0.0, _12597);
                                float _6702 = spvFMul(_6699, _6699);
                                float _12601 = interval_up(_6702, intervalFailed);
                                float _6703 = _12601;
                                _6694.lo = _6701;
                                _6694.hi = _6703;
                                Interval _6695 = _6694;
                                Interval _6704 = _6695;
                                Interval _6705 = _6704;
                                float _6709 = 1.0;
                                float _6691 = _6709;
                                float _6692 = _6709;
                                _6689.lo = _6691;
                                _6689.hi = _6692;
                                Interval _6690 = _6689;
                                Interval _6693 = _6690;
                                Interval _6710 = _6693;
                                float _6711 = 1.0;
                                float _6686 = _6711;
                                float _6687 = _6711;
                                _6684.lo = _6686;
                                _6684.hi = _6687;
                                Interval _6685 = _6684;
                                Interval _6688 = _6685;
                                Interval _6712 = _6688;
                                Interval _6713 = _6705;
                                bool _6677 = false;
                                if (_6713.lo <= 0.0)
                                {
                                    _6677 = _6713.hi >= 0.0;
                                }
                                if (_6677)
                                {
                                    _6676 = 0.0;
                                }
                                else
                                {
                                    _6676 = precise::min(abs(_6713.lo), abs(_6713.hi));
                                }
                                float _6675 = _6676;
                                float _6678 = precise::max(abs(_6713.lo), abs(_6713.hi));
                                float _6679 = spvFMul(_6675, _6675);
                                float _12657 = interval_down(_6679, intervalFailed);
                                float _6680 = precise::max(0.0, _12657);
                                float _6681 = spvFMul(_6678, _6678);
                                float _12661 = interval_up(_6681, intervalFailed);
                                float _6682 = _12661;
                                _6673.lo = _6680;
                                _6673.hi = _6682;
                                Interval _6674 = _6673;
                                Interval _6683 = _6674;
                                Interval _6714 = _6683;
                                Interval _12669 = iadd(_6712, _6714, intervalFailed);
                                Interval _6715 = _12669;
                                Interval _12670 = idiv(_6710, _6715, intervalFailed, interval_divide_upper);
                                Interval _6716 = _12670;
                                Interval _8004 = _6716;
                                Interval _12672 = imul(_7999, _8004, intervalFailed, optical_product_upper);
                                Interval _7995 = _12672;
                                Interval _8006 = _7980;
                                bool _6666 = false;
                                if (_8006.lo <= 0.0)
                                {
                                    _6666 = _8006.hi >= 0.0;
                                }
                                if (_6666)
                                {
                                    _6665 = 0.0;
                                }
                                else
                                {
                                    _6665 = precise::min(abs(_8006.lo), abs(_8006.hi));
                                }
                                float _6664 = _6665;
                                float _6667 = precise::max(abs(_8006.lo), abs(_8006.hi));
                                float _6668 = spvFMul(_6664, _6664);
                                float _12703 = interval_down(_6668, intervalFailed);
                                float _6669 = precise::max(0.0, _12703);
                                float _6670 = spvFMul(_6667, _6667);
                                float _12707 = interval_up(_6670, intervalFailed);
                                float _6671 = _12707;
                                _6662.lo = _6669;
                                _6662.hi = _6671;
                                Interval _6663 = _6662;
                                Interval _6672 = _6663;
                                Interval _8005 = _6672;
                                Interval _8008 = _7995;
                                Interval _8009 = _8005;
                                Interval _12717 = imul(_8008, _8009, intervalFailed, optical_product_upper);
                                Interval _8010 = _12717;
                                Interval _8011 = _7980;
                                Interval _12719 = imul(_8010, _8011, intervalFailed, optical_product_upper);
                                Interval _8007 = _12719;
                                float _8013 = -6.0;
                                float _6659 = _8013;
                                float _6660 = _8013;
                                _6657.lo = _6659;
                                _6657.hi = _6660;
                                Interval _6658 = _6657;
                                Interval _6661 = _6658;
                                Interval _8014 = _6661;
                                Interval _8015 = _7995;
                                Interval _12730 = imul(_8014, _8015, intervalFailed, optical_product_upper);
                                Interval _8016 = _12730;
                                Interval _8017 = _8005;
                                Interval _12732 = imul(_8016, _8017, intervalFailed, optical_product_upper);
                                Interval _8018 = _12732;
                                float _8019 = 128.0;
                                float _6654 = _8019;
                                float _6655 = _8019;
                                _6652.lo = _6654;
                                _6652.hi = _6655;
                                Interval _6653 = _6652;
                                Interval _6656 = _6653;
                                Interval _8020 = _6656;
                                Interval _8021 = _7978;
                                Interval _12743 = imul(_8020, _8021, intervalFailed, optical_product_upper);
                                Interval _8022 = _12743;
                                Interval _12744 = idiv(_8018, _8022, intervalFailed, interval_divide_upper);
                                Interval _8012 = _12744;
                                Interval _8024 = _8012;
                                Interval _8025 = _7916;
                                Interval _12747 = imul(_8024, _8025, intervalFailed, optical_product_upper);
                                Interval _8023 = _12747;
                                Interval _8027 = _8012;
                                Interval _8028 = _7932;
                                Interval _12750 = imul(_8027, _8028, intervalFailed, optical_product_upper);
                                Interval _8026 = _12750;
                                bool _8029 = true;
                                if ((isunordered(_7913.lo, 0.63999998569488525390625) || _7913.lo > 0.63999998569488525390625))
                                {
                                    _8029 = _8406.hi >= 24.0;
                                }
                                if (_8029)
                                {
                                    Interval _8030 = _8007;
                                    float _8031 = 0.0;
                                    float _6649 = _8031;
                                    float _6650 = _8031;
                                    _6647.lo = _6649;
                                    _6647.hi = _6650;
                                    Interval _6648 = _6647;
                                    Interval _6651 = _6648;
                                    Interval _8032 = _6651;
                                    float _6644 = precise::min(_8030.lo, _8032.lo);
                                    float _6645 = precise::max(_8030.hi, _8032.hi);
                                    _6642.lo = _6644;
                                    _6642.hi = _6645;
                                    Interval _6643 = _6642;
                                    Interval _6646 = _6643;
                                    _8007 = _6646;
                                    Interval _8033 = _8023;
                                    float _8034 = 0.0;
                                    float _6639 = _8034;
                                    float _6640 = _8034;
                                    _6637.lo = _6639;
                                    _6637.hi = _6640;
                                    Interval _6638 = _6637;
                                    Interval _6641 = _6638;
                                    Interval _8035 = _6641;
                                    float _6634 = precise::min(_8033.lo, _8035.lo);
                                    float _6635 = precise::max(_8033.hi, _8035.hi);
                                    _6632.lo = _6634;
                                    _6632.hi = _6635;
                                    Interval _6633 = _6632;
                                    Interval _6636 = _6633;
                                    _8023 = _6636;
                                    Interval _8036 = _8026;
                                    float _8037 = 0.0;
                                    float _6629 = _8037;
                                    float _6630 = _8037;
                                    _6627.lo = _6629;
                                    _6627.hi = _6630;
                                    Interval _6628 = _6627;
                                    Interval _6631 = _6628;
                                    Interval _8038 = _6631;
                                    float _6624 = precise::min(_8036.lo, _8038.lo);
                                    float _6625 = precise::max(_8036.hi, _8038.hi);
                                    _6622.lo = _6624;
                                    _6622.hi = _6625;
                                    Interval _6623 = _6622;
                                    Interval _6626 = _6623;
                                    _8026 = _6626;
                                }
                                Interval _8039 = _7744;
                                Interval _8040 = _8007;
                                Interval _12845 = iadd(_8039, _8040, intervalFailed);
                                _7744 = _12845;
                                Interval _8041 = _7771;
                                Interval _8042 = _8023;
                                Interval _12848 = iadd(_8041, _8042, intervalFailed);
                                _7771 = _12848;
                                Interval _8043 = _7806;
                                Interval _8044 = _8026;
                                Interval _12851 = iadd(_8043, _8044, intervalFailed);
                                _7806 = _12851;
                            }
                        }
                    }
                    Interval _8045 = _7744;
                    Interval _8046 = _7771;
                    Interval _8047 = _7806;
                    _6620.x = _8045;
                    _6620.y = _8046;
                    _6620.z = _8047;
                    Interval3 _6621 = _6620;
                    Interval3 _8048 = _6621;
                    Interval3 _8402 = _8048;
                    if (_8401 == 0u)
                    {
                        Interval _8408 = param_var_distance;
                        float _8409 = 2.0;
                        float _8410 = 10.0;
                        Interval _12870 = iratio(_8409, _8410, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8411 = _12870;
                        Interval _12871 = imul(_8408, _8411, intervalFailed, optical_product_upper);
                        Interval _8407 = _12871;
                        Interval _8413 = _8402.x;
                        float _6617 = as_type<float>(as_type<uint>(_8413.hi) ^ 2147483648u);
                        float _6618 = as_type<float>(as_type<uint>(_8413.lo) ^ 2147483648u);
                        _6615.lo = _6617;
                        _6615.hi = _6618;
                        Interval _6616 = _6615;
                        Interval _6619 = _6616;
                        Interval _8414 = _6619;
                        Interval _8415 = _8398.y;
                        float _8416 = 12.0;
                        float _8417 = 100.0;
                        Interval _12893 = iratio(_8416, _8417, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8418 = _12893;
                        float _6612 = precise::max(_8415.lo, _8418.lo);
                        float _6613 = precise::max(_8415.hi, _8418.hi);
                        _6610.lo = _6612;
                        _6610.hi = _6613;
                        Interval _6611 = _6610;
                        Interval _6614 = _6611;
                        Interval _8419 = _6614;
                        Interval _12911 = idiv(_8414, _8419, intervalFailed, interval_divide_upper);
                        Interval _8420 = _12911;
                        Interval _8421 = _8407;
                        float _6607 = as_type<float>(as_type<uint>(_8421.hi) ^ 2147483648u);
                        float _6608 = as_type<float>(as_type<uint>(_8421.lo) ^ 2147483648u);
                        _6605.lo = _6607;
                        _6605.hi = _6608;
                        Interval _6606 = _6605;
                        Interval _6609 = _6606;
                        Interval _8422 = _6609;
                        Interval _8423 = _8407;
                        Interval _6600 = _8420;
                        Interval _6601 = _8422;
                        float _6597 = precise::max(_6600.lo, _6601.lo);
                        float _6598 = precise::max(_6600.hi, _6601.hi);
                        _6595.lo = _6597;
                        _6595.hi = _6598;
                        Interval _6596 = _6595;
                        Interval _6599 = _6596;
                        Interval _6602 = _6599;
                        Interval _6603 = _8423;
                        float _6592 = precise::min(_6602.lo, _6603.lo);
                        float _6593 = precise::min(_6602.hi, _6603.hi);
                        _6590.lo = _6592;
                        _6590.hi = _6593;
                        Interval _6591 = _6590;
                        Interval _6594 = _6591;
                        Interval _6604 = _6594;
                        Interval _8412 = _6604;
                        Interval3 _8424 = _8387;
                        Interval3 _8425 = _8398;
                        Interval _8426 = _8412;
                        Interval _6580 = _8425.x;
                        Interval _6581 = _8426;
                        Interval _12975 = imul(_6580, _6581, intervalFailed, optical_product_upper);
                        Interval _6582 = _12975;
                        Interval _6583 = _8425.y;
                        Interval _6584 = _8426;
                        Interval _12979 = imul(_6583, _6584, intervalFailed, optical_product_upper);
                        Interval _6585 = _12979;
                        Interval _6586 = _8425.z;
                        Interval _6587 = _8426;
                        Interval _12983 = imul(_6586, _6587, intervalFailed, optical_product_upper);
                        Interval _6588 = _12983;
                        _6578.x = _6582;
                        _6578.y = _6585;
                        _6578.z = _6588;
                        Interval3 _6579 = _6578;
                        Interval3 _6589 = _6579;
                        Interval3 _8427 = _6589;
                        Interval _6568 = _8424.x;
                        Interval _6569 = _8427.x;
                        Interval _12997 = iadd(_6568, _6569, intervalFailed);
                        Interval _6570 = _12997;
                        Interval _6571 = _8424.y;
                        Interval _6572 = _8427.y;
                        Interval _13002 = iadd(_6571, _6572, intervalFailed);
                        Interval _6573 = _13002;
                        Interval _6574 = _8424.z;
                        Interval _6575 = _8427.z;
                        Interval _13007 = iadd(_6574, _6575, intervalFailed);
                        Interval _6576 = _13007;
                        _6566.x = _6570;
                        _6566.y = _6573;
                        _6566.z = _6576;
                        Interval3 _6567 = _6566;
                        Interval3 _6577 = _6567;
                        _8387 = _6577;
                        Interval3 _8428 = hit;
                        Interval3 _8429 = param_var_direction_1;
                        Interval _8430 = _8412;
                        Interval _6556 = _8429.x;
                        Interval _6557 = _8430;
                        Interval _13023 = imul(_6556, _6557, intervalFailed, optical_product_upper);
                        Interval _6558 = _13023;
                        Interval _6559 = _8429.y;
                        Interval _6560 = _8430;
                        Interval _13027 = imul(_6559, _6560, intervalFailed, optical_product_upper);
                        Interval _6561 = _13027;
                        Interval _6562 = _8429.z;
                        Interval _6563 = _8430;
                        Interval _13031 = imul(_6562, _6563, intervalFailed, optical_product_upper);
                        Interval _6564 = _13031;
                        _6554.x = _6558;
                        _6554.y = _6561;
                        _6554.z = _6564;
                        Interval3 _6555 = _6554;
                        Interval3 _6565 = _6555;
                        Interval3 _8431 = _6565;
                        Interval _6544 = _8428.x;
                        Interval _6545 = _8431.x;
                        Interval _13045 = iadd(_6544, _6545, intervalFailed);
                        Interval _6546 = _13045;
                        Interval _6547 = _8428.y;
                        Interval _6548 = _8431.y;
                        Interval _13050 = iadd(_6547, _6548, intervalFailed);
                        Interval _6549 = _13050;
                        Interval _6550 = _8428.z;
                        Interval _6551 = _8431.z;
                        Interval _13055 = iadd(_6550, _6551, intervalFailed);
                        Interval _6552 = _13055;
                        _6542.x = _6546;
                        _6542.y = _6549;
                        _6542.z = _6552;
                        Interval3 _6543 = _6542;
                        Interval3 _6553 = _6543;
                        hit = _6553;
                    }
                    else
                    {
                        _8395 = _8402.y;
                        _8396 = _8402.z;
                    }
                }
            }
            else
            {
                Interval _8433 = _8387.x;
                float _8434 = 18.0;
                float _8435 = 1000.0;
                Interval _13072 = iratio(_8434, _8435, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8436 = _13072;
                Interval _13073 = imul(_8433, _8436, intervalFailed, optical_product_upper);
                Interval _8437 = _13073;
                Interval _8438 = _8387.z;
                float _8439 = 11.0;
                float _8440 = 1000.0;
                Interval _13076 = iratio(_8439, _8440, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8441 = _13076;
                Interval _13077 = imul(_8438, _8441, intervalFailed, optical_product_upper);
                Interval _8442 = _13077;
                Interval _13078 = iadd(_8437, _8442, intervalFailed);
                Interval _8443 = _13078;
                Interval _8444 = _8393;
                float _8445 = 8.0;
                float _8446 = 10.0;
                Interval _13080 = iratio(_8445, _8446, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8447 = _13080;
                Interval _13081 = imul(_8444, _8447, intervalFailed, optical_product_upper);
                Interval _8448 = _13081;
                Interval _6538 = _8443;
                Interval _6539 = _8448;
                float _6535 = as_type<float>(as_type<uint>(_6539.hi) ^ 2147483648u);
                float _6536 = as_type<float>(as_type<uint>(_6539.lo) ^ 2147483648u);
                _6533.lo = _6535;
                _6533.hi = _6536;
                Interval _6534 = _6533;
                Interval _6537 = _6534;
                Interval _6540 = _6537;
                Interval _13101 = iadd(_6538, _6540, intervalFailed);
                Interval _6541 = _13101;
                Interval _8432 = _6541;
                Interval _8450 = _8387.x;
                float _8451 = 47.0;
                float _8452 = 1000.0;
                Interval _13105 = iratio(_8451, _8452, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8453 = _13105;
                Interval _13106 = imul(_8450, _8453, intervalFailed, optical_product_upper);
                Interval _8454 = _13106;
                Interval _8455 = _8387.z;
                float _8456 = 25.0;
                float _8457 = 1000.0;
                Interval _13109 = iratio(_8456, _8457, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8458 = _13109;
                Interval _13110 = imul(_8455, _8458, intervalFailed, optical_product_upper);
                Interval _8459 = _13110;
                Interval _6529 = _8454;
                Interval _6530 = _8459;
                float _6526 = as_type<float>(as_type<uint>(_6530.hi) ^ 2147483648u);
                float _6527 = as_type<float>(as_type<uint>(_6530.lo) ^ 2147483648u);
                _6524.lo = _6526;
                _6524.hi = _6527;
                Interval _6525 = _6524;
                Interval _6528 = _6525;
                Interval _6531 = _6528;
                Interval _13130 = iadd(_6529, _6531, intervalFailed);
                Interval _6532 = _13130;
                Interval _8460 = _6532;
                Interval _8461 = _8393;
                float _8462 = 12.0;
                float _8463 = 10.0;
                Interval _13133 = iratio(_8462, _8463, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8464 = _13133;
                Interval _13134 = imul(_8461, _8464, intervalFailed, optical_product_upper);
                Interval _8465 = _13134;
                Interval _13135 = iadd(_8460, _8465, intervalFailed);
                Interval _8449 = _13135;
                Interval _8467 = _8387.z;
                float _8468 = 22.0;
                float _8469 = 1000.0;
                Interval _13138 = iratio(_8468, _8469, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8470 = _13138;
                Interval _13139 = imul(_8467, _8470, intervalFailed, optical_product_upper);
                Interval _8471 = _13139;
                Interval _8472 = _8387.x;
                float _8473 = 9.0;
                float _8474 = 1000.0;
                Interval _13142 = iratio(_8473, _8474, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8475 = _13142;
                Interval _13143 = imul(_8472, _8475, intervalFailed, optical_product_upper);
                Interval _8476 = _13143;
                Interval _6520 = _8471;
                Interval _6521 = _8476;
                float _6517 = as_type<float>(as_type<uint>(_6521.hi) ^ 2147483648u);
                float _6518 = as_type<float>(as_type<uint>(_6521.lo) ^ 2147483648u);
                _6515.lo = _6517;
                _6515.hi = _6518;
                Interval _6516 = _6515;
                Interval _6519 = _6516;
                Interval _6522 = _6519;
                Interval _13163 = iadd(_6520, _6522, intervalFailed);
                Interval _6523 = _13163;
                Interval _8477 = _6523;
                Interval _8478 = _8393;
                float _8479 = 65.0;
                float _8480 = 100.0;
                Interval _13166 = iratio(_8479, _8480, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8481 = _13166;
                Interval _13167 = imul(_8478, _8481, intervalFailed, optical_product_upper);
                Interval _8482 = _13167;
                Interval _6511 = _8477;
                Interval _6512 = _8482;
                float _6508 = as_type<float>(as_type<uint>(_6512.hi) ^ 2147483648u);
                float _6509 = as_type<float>(as_type<uint>(_6512.lo) ^ 2147483648u);
                _6506.lo = _6508;
                _6506.hi = _6509;
                Interval _6507 = _6506;
                Interval _6510 = _6507;
                Interval _6513 = _6510;
                Interval _13187 = iadd(_6511, _6513, intervalFailed);
                Interval _6514 = _13187;
                Interval _8466 = _6514;
                float _8483 = 55.0;
                float _8484 = 1000.0;
                Interval _13189 = iratio(_8483, _8484, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8485 = _13189;
                Interval _8486 = _8432;
                Interval _6499 = _8486;
                float _6497 = 3.1415927410125732421875;
                float _6492 = _6497;
                float _13193 = interval_down(_6492, intervalFailed);
                float _6493 = _13193;
                float _6494 = _6497;
                float _13195 = interval_up(_6494, intervalFailed);
                float _6495 = _13195;
                _6490.lo = _6493;
                _6490.hi = _6495;
                Interval _6491 = _6490;
                Interval _6496 = _6491;
                Interval _6498 = _6496;
                Interval _6500 = _6498;
                float _6501 = 0.5;
                float _6487 = _6501;
                float _6488 = _6501;
                _6485.lo = _6487;
                _6485.hi = _6488;
                Interval _6486 = _6485;
                Interval _6489 = _6486;
                Interval _6502 = _6489;
                Interval _13213 = imul(_6500, _6502, intervalFailed, optical_product_upper);
                Interval _6503 = _13213;
                Interval _13214 = iadd(_6499, _6503, intervalFailed);
                Interval _6504 = _13214;
                float _6480 = _6504.lo;
                float _6481 = _6504.hi;
                float _6475 = _6480;
                float _6476 = _6481;
                _6472.lo = _6475;
                _6472.hi = _6476;
                Interval _6473 = _6472;
                Interval _6477 = _6473;
                Interval _13227 = isin_body(_6477, intervalFailed, optical_product_upper);
                Interval _6474 = _13227;
                interval_sine_upper = _6474.hi;
                float _6478 = _6474.lo;
                float _6479 = _6478;
                float _6482 = _6479;
                float _6483 = interval_sine_upper;
                _6470.lo = _6482;
                _6470.hi = _6483;
                Interval _6471 = _6470;
                Interval _6484 = _6471;
                Interval _6505 = _6484;
                Interval _8487 = _6505;
                Interval _13243 = imul(_8485, _8487, intervalFailed, optical_product_upper);
                Interval _8488 = _13243;
                Interval _8489 = param_var_footprint;
                float _8490 = 22.0;
                float _8491 = 1000.0;
                Interval _13245 = iratio(_8490, _8491, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8492 = _13245;
                Interval _6459 = _8489;
                Interval _6460 = _8492;
                Interval _13248 = imul(_6459, _6460, intervalFailed, optical_product_upper);
                Interval _6461 = _13248;
                bool _6451 = false;
                if (_6461.lo <= 0.0)
                {
                    _6451 = _6461.hi >= 0.0;
                }
                if (_6451)
                {
                    _6450 = 0.0;
                }
                else
                {
                    _6450 = precise::min(abs(_6461.lo), abs(_6461.hi));
                }
                float _6449 = _6450;
                float _6452 = precise::max(abs(_6461.lo), abs(_6461.hi));
                float _6453 = spvFMul(_6449, _6449);
                float _13278 = interval_down(_6453, intervalFailed);
                float _6454 = precise::max(0.0, _13278);
                float _6455 = spvFMul(_6452, _6452);
                float _13282 = interval_up(_6455, intervalFailed);
                float _6456 = _13282;
                _6447.lo = _6454;
                _6447.hi = _6456;
                Interval _6448 = _6447;
                Interval _6457 = _6448;
                Interval _6458 = _6457;
                float _6462 = 1.0;
                float _6444 = _6462;
                float _6445 = _6462;
                _6442.lo = _6444;
                _6442.hi = _6445;
                Interval _6443 = _6442;
                Interval _6446 = _6443;
                Interval _6463 = _6446;
                float _6464 = 1.0;
                float _6439 = _6464;
                float _6440 = _6464;
                _6437.lo = _6439;
                _6437.hi = _6440;
                Interval _6438 = _6437;
                Interval _6441 = _6438;
                Interval _6465 = _6441;
                Interval _6466 = _6458;
                bool _6430 = false;
                if (_6466.lo <= 0.0)
                {
                    _6430 = _6466.hi >= 0.0;
                }
                if (_6430)
                {
                    _6429 = 0.0;
                }
                else
                {
                    _6429 = precise::min(abs(_6466.lo), abs(_6466.hi));
                }
                float _6428 = _6429;
                float _6431 = precise::max(abs(_6466.lo), abs(_6466.hi));
                float _6432 = spvFMul(_6428, _6428);
                float _13338 = interval_down(_6432, intervalFailed);
                float _6433 = precise::max(0.0, _13338);
                float _6434 = spvFMul(_6431, _6431);
                float _13342 = interval_up(_6434, intervalFailed);
                float _6435 = _13342;
                _6426.lo = _6433;
                _6426.hi = _6435;
                Interval _6427 = _6426;
                Interval _6436 = _6427;
                Interval _6467 = _6436;
                Interval _13350 = iadd(_6465, _6467, intervalFailed);
                Interval _6468 = _13350;
                Interval _13351 = idiv(_6463, _6468, intervalFailed, interval_divide_upper);
                Interval _6469 = _13351;
                Interval _8493 = _6469;
                Interval _13353 = imul(_8488, _8493, intervalFailed, optical_product_upper);
                Interval _8494 = _13353;
                float _8495 = 25.0;
                float _8496 = 1000.0;
                Interval _13354 = iratio(_8495, _8496, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8497 = _13354;
                Interval _8498 = _8449;
                Interval _6419 = _8498;
                float _6417 = 3.1415927410125732421875;
                float _6412 = _6417;
                float _13358 = interval_down(_6412, intervalFailed);
                float _6413 = _13358;
                float _6414 = _6417;
                float _13360 = interval_up(_6414, intervalFailed);
                float _6415 = _13360;
                _6410.lo = _6413;
                _6410.hi = _6415;
                Interval _6411 = _6410;
                Interval _6416 = _6411;
                Interval _6418 = _6416;
                Interval _6420 = _6418;
                float _6421 = 0.5;
                float _6407 = _6421;
                float _6408 = _6421;
                _6405.lo = _6407;
                _6405.hi = _6408;
                Interval _6406 = _6405;
                Interval _6409 = _6406;
                Interval _6422 = _6409;
                Interval _13378 = imul(_6420, _6422, intervalFailed, optical_product_upper);
                Interval _6423 = _13378;
                Interval _13379 = iadd(_6419, _6423, intervalFailed);
                Interval _6424 = _13379;
                float _6400 = _6424.lo;
                float _6401 = _6424.hi;
                float _6395 = _6400;
                float _6396 = _6401;
                _6392.lo = _6395;
                _6392.hi = _6396;
                Interval _6393 = _6392;
                Interval _6397 = _6393;
                Interval _13392 = isin_body(_6397, intervalFailed, optical_product_upper);
                Interval _6394 = _13392;
                interval_sine_upper = _6394.hi;
                float _6398 = _6394.lo;
                float _6399 = _6398;
                float _6402 = _6399;
                float _6403 = interval_sine_upper;
                _6390.lo = _6402;
                _6390.hi = _6403;
                Interval _6391 = _6390;
                Interval _6404 = _6391;
                Interval _6425 = _6404;
                Interval _8499 = _6425;
                Interval _13408 = imul(_8497, _8499, intervalFailed, optical_product_upper);
                Interval _8500 = _13408;
                Interval _8501 = param_var_footprint;
                float _8502 = 54.0;
                float _8503 = 1000.0;
                Interval _13410 = iratio(_8502, _8503, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8504 = _13410;
                Interval _6379 = _8501;
                Interval _6380 = _8504;
                Interval _13413 = imul(_6379, _6380, intervalFailed, optical_product_upper);
                Interval _6381 = _13413;
                bool _6371 = false;
                if (_6381.lo <= 0.0)
                {
                    _6371 = _6381.hi >= 0.0;
                }
                if (_6371)
                {
                    _6370 = 0.0;
                }
                else
                {
                    _6370 = precise::min(abs(_6381.lo), abs(_6381.hi));
                }
                float _6369 = _6370;
                float _6372 = precise::max(abs(_6381.lo), abs(_6381.hi));
                float _6373 = spvFMul(_6369, _6369);
                float _13443 = interval_down(_6373, intervalFailed);
                float _6374 = precise::max(0.0, _13443);
                float _6375 = spvFMul(_6372, _6372);
                float _13447 = interval_up(_6375, intervalFailed);
                float _6376 = _13447;
                _6367.lo = _6374;
                _6367.hi = _6376;
                Interval _6368 = _6367;
                Interval _6377 = _6368;
                Interval _6378 = _6377;
                float _6382 = 1.0;
                float _6364 = _6382;
                float _6365 = _6382;
                _6362.lo = _6364;
                _6362.hi = _6365;
                Interval _6363 = _6362;
                Interval _6366 = _6363;
                Interval _6383 = _6366;
                float _6384 = 1.0;
                float _6359 = _6384;
                float _6360 = _6384;
                _6357.lo = _6359;
                _6357.hi = _6360;
                Interval _6358 = _6357;
                Interval _6361 = _6358;
                Interval _6385 = _6361;
                Interval _6386 = _6378;
                bool _6350 = false;
                if (_6386.lo <= 0.0)
                {
                    _6350 = _6386.hi >= 0.0;
                }
                if (_6350)
                {
                    _6349 = 0.0;
                }
                else
                {
                    _6349 = precise::min(abs(_6386.lo), abs(_6386.hi));
                }
                float _6348 = _6349;
                float _6351 = precise::max(abs(_6386.lo), abs(_6386.hi));
                float _6352 = spvFMul(_6348, _6348);
                float _13503 = interval_down(_6352, intervalFailed);
                float _6353 = precise::max(0.0, _13503);
                float _6354 = spvFMul(_6351, _6351);
                float _13507 = interval_up(_6354, intervalFailed);
                float _6355 = _13507;
                _6346.lo = _6353;
                _6346.hi = _6355;
                Interval _6347 = _6346;
                Interval _6356 = _6347;
                Interval _6387 = _6356;
                Interval _13515 = iadd(_6385, _6387, intervalFailed);
                Interval _6388 = _13515;
                Interval _13516 = idiv(_6383, _6388, intervalFailed, interval_divide_upper);
                Interval _6389 = _13516;
                Interval _8505 = _6389;
                Interval _13518 = imul(_8500, _8505, intervalFailed, optical_product_upper);
                Interval _8506 = _13518;
                Interval _13519 = iadd(_8494, _8506, intervalFailed);
                _8395 = _13519;
                float _8507 = 45.0;
                float _8508 = 1000.0;
                Interval _13520 = iratio(_8507, _8508, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8509 = _13520;
                Interval _8510 = _8466;
                Interval _6339 = _8510;
                float _6337 = 3.1415927410125732421875;
                float _6332 = _6337;
                float _13524 = interval_down(_6332, intervalFailed);
                float _6333 = _13524;
                float _6334 = _6337;
                float _13526 = interval_up(_6334, intervalFailed);
                float _6335 = _13526;
                _6330.lo = _6333;
                _6330.hi = _6335;
                Interval _6331 = _6330;
                Interval _6336 = _6331;
                Interval _6338 = _6336;
                Interval _6340 = _6338;
                float _6341 = 0.5;
                float _6327 = _6341;
                float _6328 = _6341;
                _6325.lo = _6327;
                _6325.hi = _6328;
                Interval _6326 = _6325;
                Interval _6329 = _6326;
                Interval _6342 = _6329;
                Interval _13544 = imul(_6340, _6342, intervalFailed, optical_product_upper);
                Interval _6343 = _13544;
                Interval _13545 = iadd(_6339, _6343, intervalFailed);
                Interval _6344 = _13545;
                float _6320 = _6344.lo;
                float _6321 = _6344.hi;
                float _6315 = _6320;
                float _6316 = _6321;
                _6312.lo = _6315;
                _6312.hi = _6316;
                Interval _6313 = _6312;
                Interval _6317 = _6313;
                Interval _13558 = isin_body(_6317, intervalFailed, optical_product_upper);
                Interval _6314 = _13558;
                interval_sine_upper = _6314.hi;
                float _6318 = _6314.lo;
                float _6319 = _6318;
                float _6322 = _6319;
                float _6323 = interval_sine_upper;
                _6310.lo = _6322;
                _6310.hi = _6323;
                Interval _6311 = _6310;
                Interval _6324 = _6311;
                Interval _6345 = _6324;
                Interval _8511 = _6345;
                Interval _13574 = imul(_8509, _8511, intervalFailed, optical_product_upper);
                Interval _8512 = _13574;
                Interval _8513 = param_var_footprint;
                float _8514 = 24.0;
                float _8515 = 1000.0;
                Interval _13576 = iratio(_8514, _8515, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8516 = _13576;
                Interval _6299 = _8513;
                Interval _6300 = _8516;
                Interval _13579 = imul(_6299, _6300, intervalFailed, optical_product_upper);
                Interval _6301 = _13579;
                bool _6291 = false;
                if (_6301.lo <= 0.0)
                {
                    _6291 = _6301.hi >= 0.0;
                }
                if (_6291)
                {
                    _6290 = 0.0;
                }
                else
                {
                    _6290 = precise::min(abs(_6301.lo), abs(_6301.hi));
                }
                float _6289 = _6290;
                float _6292 = precise::max(abs(_6301.lo), abs(_6301.hi));
                float _6293 = spvFMul(_6289, _6289);
                float _13609 = interval_down(_6293, intervalFailed);
                float _6294 = precise::max(0.0, _13609);
                float _6295 = spvFMul(_6292, _6292);
                float _13613 = interval_up(_6295, intervalFailed);
                float _6296 = _13613;
                _6287.lo = _6294;
                _6287.hi = _6296;
                Interval _6288 = _6287;
                Interval _6297 = _6288;
                Interval _6298 = _6297;
                float _6302 = 1.0;
                float _6284 = _6302;
                float _6285 = _6302;
                _6282.lo = _6284;
                _6282.hi = _6285;
                Interval _6283 = _6282;
                Interval _6286 = _6283;
                Interval _6303 = _6286;
                float _6304 = 1.0;
                float _6279 = _6304;
                float _6280 = _6304;
                _6277.lo = _6279;
                _6277.hi = _6280;
                Interval _6278 = _6277;
                Interval _6281 = _6278;
                Interval _6305 = _6281;
                Interval _6306 = _6298;
                bool _6270 = false;
                if (_6306.lo <= 0.0)
                {
                    _6270 = _6306.hi >= 0.0;
                }
                if (_6270)
                {
                    _6269 = 0.0;
                }
                else
                {
                    _6269 = precise::min(abs(_6306.lo), abs(_6306.hi));
                }
                float _6268 = _6269;
                float _6271 = precise::max(abs(_6306.lo), abs(_6306.hi));
                float _6272 = spvFMul(_6268, _6268);
                float _13669 = interval_down(_6272, intervalFailed);
                float _6273 = precise::max(0.0, _13669);
                float _6274 = spvFMul(_6271, _6271);
                float _13673 = interval_up(_6274, intervalFailed);
                float _6275 = _13673;
                _6266.lo = _6273;
                _6266.hi = _6275;
                Interval _6267 = _6266;
                Interval _6276 = _6267;
                Interval _6307 = _6276;
                Interval _13681 = iadd(_6305, _6307, intervalFailed);
                Interval _6308 = _13681;
                Interval _13682 = idiv(_6303, _6308, intervalFailed, interval_divide_upper);
                Interval _6309 = _13682;
                Interval _8517 = _6309;
                Interval _13684 = imul(_8512, _8517, intervalFailed, optical_product_upper);
                Interval _8518 = _13684;
                float _8519 = 20.0;
                float _8520 = 1000.0;
                Interval _13685 = iratio(_8519, _8520, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8521 = _13685;
                Interval _8522 = _8449;
                Interval _6259 = _8522;
                float _6257 = 3.1415927410125732421875;
                float _6252 = _6257;
                float _13689 = interval_down(_6252, intervalFailed);
                float _6253 = _13689;
                float _6254 = _6257;
                float _13691 = interval_up(_6254, intervalFailed);
                float _6255 = _13691;
                _6250.lo = _6253;
                _6250.hi = _6255;
                Interval _6251 = _6250;
                Interval _6256 = _6251;
                Interval _6258 = _6256;
                Interval _6260 = _6258;
                float _6261 = 0.5;
                float _6247 = _6261;
                float _6248 = _6261;
                _6245.lo = _6247;
                _6245.hi = _6248;
                Interval _6246 = _6245;
                Interval _6249 = _6246;
                Interval _6262 = _6249;
                Interval _13709 = imul(_6260, _6262, intervalFailed, optical_product_upper);
                Interval _6263 = _13709;
                Interval _13710 = iadd(_6259, _6263, intervalFailed);
                Interval _6264 = _13710;
                float _6240 = _6264.lo;
                float _6241 = _6264.hi;
                float _6235 = _6240;
                float _6236 = _6241;
                _6232.lo = _6235;
                _6232.hi = _6236;
                Interval _6233 = _6232;
                Interval _6237 = _6233;
                Interval _13723 = isin_body(_6237, intervalFailed, optical_product_upper);
                Interval _6234 = _13723;
                interval_sine_upper = _6234.hi;
                float _6238 = _6234.lo;
                float _6239 = _6238;
                float _6242 = _6239;
                float _6243 = interval_sine_upper;
                _6230.lo = _6242;
                _6230.hi = _6243;
                Interval _6231 = _6230;
                Interval _6244 = _6231;
                Interval _6265 = _6244;
                Interval _8523 = _6265;
                Interval _13739 = imul(_8521, _8523, intervalFailed, optical_product_upper);
                Interval _8524 = _13739;
                Interval _8525 = param_var_footprint;
                float _8526 = 54.0;
                float _8527 = 1000.0;
                Interval _13741 = iratio(_8526, _8527, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8528 = _13741;
                Interval _6219 = _8525;
                Interval _6220 = _8528;
                Interval _13744 = imul(_6219, _6220, intervalFailed, optical_product_upper);
                Interval _6221 = _13744;
                bool _6211 = false;
                if (_6221.lo <= 0.0)
                {
                    _6211 = _6221.hi >= 0.0;
                }
                if (_6211)
                {
                    _6210 = 0.0;
                }
                else
                {
                    _6210 = precise::min(abs(_6221.lo), abs(_6221.hi));
                }
                float _6209 = _6210;
                float _6212 = precise::max(abs(_6221.lo), abs(_6221.hi));
                float _6213 = spvFMul(_6209, _6209);
                float _13774 = interval_down(_6213, intervalFailed);
                float _6214 = precise::max(0.0, _13774);
                float _6215 = spvFMul(_6212, _6212);
                float _13778 = interval_up(_6215, intervalFailed);
                float _6216 = _13778;
                _6207.lo = _6214;
                _6207.hi = _6216;
                Interval _6208 = _6207;
                Interval _6217 = _6208;
                Interval _6218 = _6217;
                float _6222 = 1.0;
                float _6204 = _6222;
                float _6205 = _6222;
                _6202.lo = _6204;
                _6202.hi = _6205;
                Interval _6203 = _6202;
                Interval _6206 = _6203;
                Interval _6223 = _6206;
                float _6224 = 1.0;
                float _6199 = _6224;
                float _6200 = _6224;
                _6197.lo = _6199;
                _6197.hi = _6200;
                Interval _6198 = _6197;
                Interval _6201 = _6198;
                Interval _6225 = _6201;
                Interval _6226 = _6218;
                bool _6190 = false;
                if (_6226.lo <= 0.0)
                {
                    _6190 = _6226.hi >= 0.0;
                }
                if (_6190)
                {
                    _6189 = 0.0;
                }
                else
                {
                    _6189 = precise::min(abs(_6226.lo), abs(_6226.hi));
                }
                float _6188 = _6189;
                float _6191 = precise::max(abs(_6226.lo), abs(_6226.hi));
                float _6192 = spvFMul(_6188, _6188);
                float _13834 = interval_down(_6192, intervalFailed);
                float _6193 = precise::max(0.0, _13834);
                float _6194 = spvFMul(_6191, _6191);
                float _13838 = interval_up(_6194, intervalFailed);
                float _6195 = _13838;
                _6186.lo = _6193;
                _6186.hi = _6195;
                Interval _6187 = _6186;
                Interval _6196 = _6187;
                Interval _6227 = _6196;
                Interval _13846 = iadd(_6225, _6227, intervalFailed);
                Interval _6228 = _13846;
                Interval _13847 = idiv(_6223, _6228, intervalFailed, interval_divide_upper);
                Interval _6229 = _13847;
                Interval _8529 = _6229;
                Interval _13849 = imul(_8524, _8529, intervalFailed, optical_product_upper);
                Interval _8530 = _13849;
                Interval _6182 = _8518;
                Interval _6183 = _8530;
                float _6179 = as_type<float>(as_type<uint>(_6183.hi) ^ 2147483648u);
                float _6180 = as_type<float>(as_type<uint>(_6183.lo) ^ 2147483648u);
                _6177.lo = _6179;
                _6177.hi = _6180;
                Interval _6178 = _6177;
                Interval _6181 = _6178;
                Interval _6184 = _6181;
                Interval _13869 = iadd(_6182, _6184, intervalFailed);
                Interval _6185 = _13869;
                _8396 = _6185;
            }
            Interval _8531 = _8395;
            float _8532 = -1.0;
            float _6174 = _8532;
            float _6175 = _8532;
            _6172.lo = _6174;
            _6172.hi = _6175;
            Interval _6173 = _6172;
            Interval _6176 = _6173;
            Interval _8533 = _6176;
            Interval _8534 = _8396;
            _6170.x = _8531;
            _6170.y = _8533;
            _6170.z = _8534;
            Interval3 _6171 = _6170;
            Interval3 _8535 = _6171;
            ReflectionLiquidFrame _8536 = param_var_f;
            float3 _6157 = _8536.rotation0.xyz;
            float _6150 = _6157.x;
            float _6147 = _6150;
            float _6148 = _6150;
            _6145.lo = _6147;
            _6145.hi = _6148;
            Interval _6146 = _6145;
            Interval _6149 = _6146;
            Interval _6151 = _6149;
            float _6152 = _6157.y;
            float _6142 = _6152;
            float _6143 = _6152;
            _6140.lo = _6142;
            _6140.hi = _6143;
            Interval _6141 = _6140;
            Interval _6144 = _6141;
            Interval _6153 = _6144;
            float _6154 = _6157.z;
            float _6137 = _6154;
            float _6138 = _6154;
            _6135.lo = _6137;
            _6135.hi = _6138;
            Interval _6136 = _6135;
            Interval _6139 = _6136;
            Interval _6155 = _6139;
            _6133.x = _6151;
            _6133.y = _6153;
            _6133.z = _6155;
            Interval3 _6134 = _6133;
            Interval3 _6156 = _6134;
            Interval3 _6158 = _6156;
            Interval3 _6159 = _8535;
            Interval _6122 = _6158.x;
            Interval _6123 = _6159.x;
            Interval _13941 = imul(_6122, _6123, intervalFailed, optical_product_upper);
            Interval _6124 = _13941;
            Interval _6125 = _6158.y;
            Interval _6126 = _6159.y;
            Interval _13946 = imul(_6125, _6126, intervalFailed, optical_product_upper);
            Interval _6127 = _13946;
            Interval _13947 = iadd(_6124, _6127, intervalFailed);
            Interval _6128 = _13947;
            Interval _6129 = _6158.z;
            Interval _6130 = _6159.z;
            Interval _13952 = imul(_6129, _6130, intervalFailed, optical_product_upper);
            Interval _6131 = _13952;
            Interval _13953 = iadd(_6128, _6131, intervalFailed);
            Interval _6132 = _13953;
            Interval _6160 = _6132;
            float3 _6161 = _8536.rotation1.xyz;
            float _6115 = _6161.x;
            float _6112 = _6115;
            float _6113 = _6115;
            _6110.lo = _6112;
            _6110.hi = _6113;
            Interval _6111 = _6110;
            Interval _6114 = _6111;
            Interval _6116 = _6114;
            float _6117 = _6161.y;
            float _6107 = _6117;
            float _6108 = _6117;
            _6105.lo = _6107;
            _6105.hi = _6108;
            Interval _6106 = _6105;
            Interval _6109 = _6106;
            Interval _6118 = _6109;
            float _6119 = _6161.z;
            float _6102 = _6119;
            float _6103 = _6119;
            _6100.lo = _6102;
            _6100.hi = _6103;
            Interval _6101 = _6100;
            Interval _6104 = _6101;
            Interval _6120 = _6104;
            _6098.x = _6116;
            _6098.y = _6118;
            _6098.z = _6120;
            Interval3 _6099 = _6098;
            Interval3 _6121 = _6099;
            Interval3 _6162 = _6121;
            Interval3 _6163 = _8535;
            Interval _6087 = _6162.x;
            Interval _6088 = _6163.x;
            Interval _14005 = imul(_6087, _6088, intervalFailed, optical_product_upper);
            Interval _6089 = _14005;
            Interval _6090 = _6162.y;
            Interval _6091 = _6163.y;
            Interval _14010 = imul(_6090, _6091, intervalFailed, optical_product_upper);
            Interval _6092 = _14010;
            Interval _14011 = iadd(_6089, _6092, intervalFailed);
            Interval _6093 = _14011;
            Interval _6094 = _6162.z;
            Interval _6095 = _6163.z;
            Interval _14016 = imul(_6094, _6095, intervalFailed, optical_product_upper);
            Interval _6096 = _14016;
            Interval _14017 = iadd(_6093, _6096, intervalFailed);
            Interval _6097 = _14017;
            Interval _6164 = _6097;
            float3 _6165 = _8536.rotation2.xyz;
            float _6080 = _6165.x;
            float _6077 = _6080;
            float _6078 = _6080;
            _6075.lo = _6077;
            _6075.hi = _6078;
            Interval _6076 = _6075;
            Interval _6079 = _6076;
            Interval _6081 = _6079;
            float _6082 = _6165.y;
            float _6072 = _6082;
            float _6073 = _6082;
            _6070.lo = _6072;
            _6070.hi = _6073;
            Interval _6071 = _6070;
            Interval _6074 = _6071;
            Interval _6083 = _6074;
            float _6084 = _6165.z;
            float _6067 = _6084;
            float _6068 = _6084;
            _6065.lo = _6067;
            _6065.hi = _6068;
            Interval _6066 = _6065;
            Interval _6069 = _6066;
            Interval _6085 = _6069;
            _6063.x = _6081;
            _6063.y = _6083;
            _6063.z = _6085;
            Interval3 _6064 = _6063;
            Interval3 _6086 = _6064;
            Interval3 _6166 = _6086;
            Interval3 _6167 = _8535;
            Interval _6052 = _6166.x;
            Interval _6053 = _6167.x;
            Interval _14069 = imul(_6052, _6053, intervalFailed, optical_product_upper);
            Interval _6054 = _14069;
            Interval _6055 = _6166.y;
            Interval _6056 = _6167.y;
            Interval _14074 = imul(_6055, _6056, intervalFailed, optical_product_upper);
            Interval _6057 = _14074;
            Interval _14075 = iadd(_6054, _6057, intervalFailed);
            Interval _6058 = _14075;
            Interval _6059 = _6166.z;
            Interval _6060 = _6167.z;
            Interval _14080 = imul(_6059, _6060, intervalFailed, optical_product_upper);
            Interval _6061 = _14080;
            Interval _14081 = iadd(_6058, _6061, intervalFailed);
            Interval _6062 = _14081;
            Interval _6168 = _6062;
            _6050.x = _6160;
            _6050.y = _6164;
            _6050.z = _6168;
            Interval3 _6051 = _6050;
            Interval3 _6169 = _6051;
            Interval3 _8537 = _6169;
            Interval3 _6043 = _8537;
            float _6044 = 1.0;
            float _6040 = _6044;
            float _6041 = _6044;
            _6038.lo = _6040;
            _6038.hi = _6041;
            Interval _6039 = _6038;
            Interval _6042 = _6039;
            Interval _6045 = _6042;
            Interval3 _6046 = _8537;
            Interval _6029 = _6046.x;
            bool _6022 = false;
            if (_6029.lo <= 0.0)
            {
                _6022 = _6029.hi >= 0.0;
            }
            if (_6022)
            {
                _6021 = 0.0;
            }
            else
            {
                _6021 = precise::min(abs(_6029.lo), abs(_6029.hi));
            }
            float _6020 = _6021;
            float _6023 = precise::max(abs(_6029.lo), abs(_6029.hi));
            float _6024 = spvFMul(_6020, _6020);
            float _14134 = interval_down(_6024, intervalFailed);
            float _6025 = precise::max(0.0, _14134);
            float _6026 = spvFMul(_6023, _6023);
            float _14138 = interval_up(_6026, intervalFailed);
            float _6027 = _14138;
            _6018.lo = _6025;
            _6018.hi = _6027;
            Interval _6019 = _6018;
            Interval _6028 = _6019;
            Interval _6030 = _6028;
            Interval _6031 = _6046.y;
            bool _6011 = false;
            if (_6031.lo <= 0.0)
            {
                _6011 = _6031.hi >= 0.0;
            }
            if (_6011)
            {
                _6010 = 0.0;
            }
            else
            {
                _6010 = precise::min(abs(_6031.lo), abs(_6031.hi));
            }
            float _6009 = _6010;
            float _6012 = precise::max(abs(_6031.lo), abs(_6031.hi));
            float _6013 = spvFMul(_6009, _6009);
            float _14177 = interval_down(_6013, intervalFailed);
            float _6014 = precise::max(0.0, _14177);
            float _6015 = spvFMul(_6012, _6012);
            float _14181 = interval_up(_6015, intervalFailed);
            float _6016 = _14181;
            _6007.lo = _6014;
            _6007.hi = _6016;
            Interval _6008 = _6007;
            Interval _6017 = _6008;
            Interval _6032 = _6017;
            Interval _14189 = iadd(_6030, _6032, intervalFailed);
            Interval _6033 = _14189;
            Interval _6034 = _6046.z;
            bool _6000 = false;
            if (_6034.lo <= 0.0)
            {
                _6000 = _6034.hi >= 0.0;
            }
            if (_6000)
            {
                _5999 = 0.0;
            }
            else
            {
                _5999 = precise::min(abs(_6034.lo), abs(_6034.hi));
            }
            float _5998 = _5999;
            float _6001 = precise::max(abs(_6034.lo), abs(_6034.hi));
            float _6002 = spvFMul(_5998, _5998);
            float _14221 = interval_down(_6002, intervalFailed);
            float _6003 = precise::max(0.0, _14221);
            float _6004 = spvFMul(_6001, _6001);
            float _14225 = interval_up(_6004, intervalFailed);
            float _6005 = _14225;
            _5996.lo = _6003;
            _5996.hi = _6005;
            Interval _5997 = _5996;
            Interval _6006 = _5997;
            Interval _6035 = _6006;
            Interval _14233 = iadd(_6033, _6035, intervalFailed);
            Interval _6036 = _14233;
            Interval _14234 = isqrt(_6036, intervalFailed);
            Interval _6037 = _14234;
            Interval _6047 = _6037;
            Interval _14236 = idiv(_6045, _6047, intervalFailed, interval_divide_upper);
            Interval _6048 = _14236;
            Interval _5986 = _6043.x;
            Interval _5987 = _6048;
            Interval _14240 = imul(_5986, _5987, intervalFailed, optical_product_upper);
            Interval _5988 = _14240;
            Interval _5989 = _6043.y;
            Interval _5990 = _6048;
            Interval _14244 = imul(_5989, _5990, intervalFailed, optical_product_upper);
            Interval _5991 = _14244;
            Interval _5992 = _6043.z;
            Interval _5993 = _6048;
            Interval _14248 = imul(_5992, _5993, intervalFailed, optical_product_upper);
            Interval _5994 = _14248;
            _5984.x = _5988;
            _5984.y = _5991;
            _5984.z = _5994;
            Interval3 _5985 = _5984;
            Interval3 _5995 = _5985;
            Interval3 _6049 = _5995;
            Interval3 _8538 = _6049;
            Interval3 _8539 = param_var_direction_1;
            Interval3 _14260 = oriented(_8538, _8539, intervalFailed, optical_product_upper);
            Interval3 _8540 = _14260;
            n = _8540;
        }
        else
        {
            ReflectionSpecularPlane param_var_p_1 = plane;
            Interval3 param_var_hit = hit;
            bool _14264 = outside_face(param_var_p_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (_14264)
            {
                return !intervalFailed;
            }
        }
        bool temp_var_logical_3 = false;
        if (primary)
        {
            temp_var_logical_3 = !curved;
        }
        bool temp_var_logical_4 = false;
        if (temp_var_logical_3)
        {
            ReflectionSpecularPlane param_var_p_2 = plane;
            bool _5981 = false;
            if (param_var_p_2.a.w == 2.0)
            {
                _5981 = param_var_p_2.b.w == 2.0;
            }
            bool _5982 = false;
            if (_5981)
            {
                _5982 = param_var_p_2.c.w == 2.0;
            }
            bool _5983 = _5982;
            temp_var_logical_4 = !_5983;
        }
        if (temp_var_logical_4)
        {
            temp_var_ternary_2 = 1;
        }
        else
        {
            temp_var_ternary_2 = 5;
        }
        float param_var_n_2 = float(temp_var_ternary_2);
        float param_var_d_1 = 100.0;
        Interval _14295 = iratio(param_var_n_2, param_var_d_1, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_a_17 = _14295;
        Interval param_var_a_18 = _distance;
        float param_var_n_3 = 1.0;
        float param_var_d_2 = 100000.0;
        Interval _14297 = iratio(param_var_n_3, param_var_d_2, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_b_14 = _14297;
        Interval _14298 = imul(param_var_a_18, param_var_b_14, intervalFailed, optical_product_upper);
        Interval param_var_b_15 = _14298;
        float _5978 = precise::max(param_var_a_17.lo, param_var_b_15.lo);
        float _5979 = precise::max(param_var_a_17.hi, param_var_b_15.hi);
        _5976.lo = _5978;
        _5976.hi = _5979;
        Interval _5977 = _5976;
        Interval _5980 = _5977;
        bias0 = _5980;
        Interval3 param_var_a_19 = hit;
        Interval3 param_var_a_20 = n;
        Interval param_var_b_16 = bias0;
        Interval _5966 = param_var_a_20.x;
        Interval _5967 = param_var_b_16;
        Interval _14322 = imul(_5966, _5967, intervalFailed, optical_product_upper);
        Interval _5968 = _14322;
        Interval _5969 = param_var_a_20.y;
        Interval _5970 = param_var_b_16;
        Interval _14326 = imul(_5969, _5970, intervalFailed, optical_product_upper);
        Interval _5971 = _14326;
        Interval _5972 = param_var_a_20.z;
        Interval _5973 = param_var_b_16;
        Interval _14330 = imul(_5972, _5973, intervalFailed, optical_product_upper);
        Interval _5974 = _14330;
        _5964.x = _5968;
        _5964.y = _5971;
        _5964.z = _5974;
        Interval3 _5965 = _5964;
        Interval3 _5975 = _5965;
        Interval3 param_var_b_17 = _5975;
        Interval _5954 = param_var_a_19.x;
        Interval _5955 = param_var_b_17.x;
        Interval _14344 = iadd(_5954, _5955, intervalFailed);
        Interval _5956 = _14344;
        Interval _5957 = param_var_a_19.y;
        Interval _5958 = param_var_b_17.y;
        Interval _14349 = iadd(_5957, _5958, intervalFailed);
        Interval _5959 = _14349;
        Interval _5960 = param_var_a_19.z;
        Interval _5961 = param_var_b_17.z;
        Interval _14354 = iadd(_5960, _5961, intervalFailed);
        Interval _5962 = _14354;
        _5952.x = _5956;
        _5952.y = _5959;
        _5952.z = _5962;
        Interval3 _5953 = _5952;
        Interval3 _5963 = _5953;
        origin = _5963;
        Interval3 param_var_a_21 = outgoing;
        Interval3 param_var_n_4 = n;
        Interval3 _5942 = param_var_a_21;
        Interval3 _5943 = param_var_n_4;
        float _5944 = 2.0;
        float _5939 = _5944;
        float _5940 = _5944;
        _5937.lo = _5939;
        _5937.hi = _5940;
        Interval _5938 = _5937;
        Interval _5941 = _5938;
        Interval _5945 = _5941;
        Interval3 _5946 = param_var_a_21;
        Interval3 _5947 = param_var_n_4;
        Interval _5926 = _5946.x;
        Interval _5927 = _5947.x;
        Interval _14383 = imul(_5926, _5927, intervalFailed, optical_product_upper);
        Interval _5928 = _14383;
        Interval _5929 = _5946.y;
        Interval _5930 = _5947.y;
        Interval _14388 = imul(_5929, _5930, intervalFailed, optical_product_upper);
        Interval _5931 = _14388;
        Interval _14389 = iadd(_5928, _5931, intervalFailed);
        Interval _5932 = _14389;
        Interval _5933 = _5946.z;
        Interval _5934 = _5947.z;
        Interval _14394 = imul(_5933, _5934, intervalFailed, optical_product_upper);
        Interval _5935 = _14394;
        Interval _14395 = iadd(_5932, _5935, intervalFailed);
        Interval _5936 = _14395;
        Interval _5948 = _5936;
        Interval _14397 = imul(_5945, _5948, intervalFailed, optical_product_upper);
        Interval _5949 = _14397;
        Interval _5916 = _5943.x;
        Interval _5917 = _5949;
        Interval _14401 = imul(_5916, _5917, intervalFailed, optical_product_upper);
        Interval _5918 = _14401;
        Interval _5919 = _5943.y;
        Interval _5920 = _5949;
        Interval _14405 = imul(_5919, _5920, intervalFailed, optical_product_upper);
        Interval _5921 = _14405;
        Interval _5922 = _5943.z;
        Interval _5923 = _5949;
        Interval _14409 = imul(_5922, _5923, intervalFailed, optical_product_upper);
        Interval _5924 = _14409;
        _5914.x = _5918;
        _5914.y = _5921;
        _5914.z = _5924;
        Interval3 _5915 = _5914;
        Interval3 _5925 = _5915;
        Interval3 _5950 = _5925;
        Interval3 _5905 = _5942;
        Interval _5906 = _5950.x;
        float _5902 = as_type<float>(as_type<uint>(_5906.hi) ^ 2147483648u);
        float _5903 = as_type<float>(as_type<uint>(_5906.lo) ^ 2147483648u);
        _5900.lo = _5902;
        _5900.hi = _5903;
        Interval _5901 = _5900;
        Interval _5904 = _5901;
        Interval _5907 = _5904;
        Interval _5908 = _5950.y;
        float _5897 = as_type<float>(as_type<uint>(_5908.hi) ^ 2147483648u);
        float _5898 = as_type<float>(as_type<uint>(_5908.lo) ^ 2147483648u);
        _5895.lo = _5897;
        _5895.hi = _5898;
        Interval _5896 = _5895;
        Interval _5899 = _5896;
        Interval _5909 = _5899;
        Interval _5910 = _5950.z;
        float _5892 = as_type<float>(as_type<uint>(_5910.hi) ^ 2147483648u);
        float _5893 = as_type<float>(as_type<uint>(_5910.lo) ^ 2147483648u);
        _5890.lo = _5892;
        _5890.hi = _5893;
        Interval _5891 = _5890;
        Interval _5894 = _5891;
        Interval _5911 = _5894;
        _5888.x = _5907;
        _5888.y = _5909;
        _5888.z = _5911;
        Interval3 _5889 = _5888;
        Interval3 _5912 = _5889;
        Interval _5878 = _5905.x;
        Interval _5879 = _5912.x;
        Interval _14489 = iadd(_5878, _5879, intervalFailed);
        Interval _5880 = _14489;
        Interval _5881 = _5905.y;
        Interval _5882 = _5912.y;
        Interval _14494 = iadd(_5881, _5882, intervalFailed);
        Interval _5883 = _14494;
        Interval _5884 = _5905.z;
        Interval _5885 = _5912.z;
        Interval _14499 = iadd(_5884, _5885, intervalFailed);
        Interval _5886 = _14499;
        _5876.x = _5880;
        _5876.y = _5883;
        _5876.z = _5886;
        Interval3 _5877 = _5876;
        Interval3 _5887 = _5877;
        Interval3 _5913 = _5887;
        Interval3 _5951 = _5913;
        outgoing = _5951;
        bool temp_var_logical_5 = false;
        if (primary)
        {
            temp_var_logical_5 = !curved;
        }
        bool temp_var_logical_6 = false;
        if (temp_var_logical_5)
        {
            temp_var_logical_6 = receiver.settings.x != 0.0;
        }
        if (temp_var_logical_6)
        {
            Interval param_var_a_22 = outgoing.y;
            bool _5872 = false;
            if (param_var_a_22.lo <= 0.0)
            {
                _5872 = param_var_a_22.hi >= 0.0;
            }
            if (_5872)
            {
                _5871 = 0.0;
            }
            else
            {
                _5871 = precise::min(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            }
            float _5873 = _5871;
            float _5874 = precise::max(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            _5869.lo = _5873;
            _5869.hi = _5874;
            Interval _5870 = _5869;
            Interval _5875 = _5870;
            Interval ay = _5875;
            if (ay.hi < 0.949999988079071044921875)
            {
                Interval3 param_var_a_23 = outgoing;
                float3 param_var_v_3 = float3(0.0, 1.0, 0.0);
                float _5862 = param_var_v_3.x;
                float _5859 = _5862;
                float _5860 = _5862;
                _5857.lo = _5859;
                _5857.hi = _5860;
                Interval _5858 = _5857;
                Interval _5861 = _5858;
                Interval _5863 = _5861;
                float _5864 = param_var_v_3.y;
                float _5854 = _5864;
                float _5855 = _5864;
                _5852.lo = _5854;
                _5852.hi = _5855;
                Interval _5853 = _5852;
                Interval _5856 = _5853;
                Interval _5865 = _5856;
                float _5866 = param_var_v_3.z;
                float _5849 = _5866;
                float _5850 = _5866;
                _5847.lo = _5849;
                _5847.hi = _5850;
                Interval _5848 = _5847;
                Interval _5851 = _5848;
                Interval _5867 = _5851;
                _5845.x = _5863;
                _5845.y = _5865;
                _5845.z = _5867;
                Interval3 _5846 = _5845;
                Interval3 _5868 = _5846;
                Interval3 param_var_b_18 = _5868;
                Interval _5823 = param_var_a_23.y;
                Interval _5824 = param_var_b_18.z;
                Interval _14606 = imul(_5823, _5824, intervalFailed, optical_product_upper);
                Interval _5825 = _14606;
                Interval _5826 = param_var_a_23.z;
                Interval _5827 = param_var_b_18.y;
                Interval _14611 = imul(_5826, _5827, intervalFailed, optical_product_upper);
                Interval _5828 = _14611;
                Interval _5819 = _5825;
                Interval _5820 = _5828;
                float _5816 = as_type<float>(as_type<uint>(_5820.hi) ^ 2147483648u);
                float _5817 = as_type<float>(as_type<uint>(_5820.lo) ^ 2147483648u);
                _5814.lo = _5816;
                _5814.hi = _5817;
                Interval _5815 = _5814;
                Interval _5818 = _5815;
                Interval _5821 = _5818;
                Interval _14631 = iadd(_5819, _5821, intervalFailed);
                Interval _5822 = _14631;
                Interval _5829 = _5822;
                Interval _5830 = param_var_a_23.z;
                Interval _5831 = param_var_b_18.x;
                Interval _14637 = imul(_5830, _5831, intervalFailed, optical_product_upper);
                Interval _5832 = _14637;
                Interval _5833 = param_var_a_23.x;
                Interval _5834 = param_var_b_18.z;
                Interval _14642 = imul(_5833, _5834, intervalFailed, optical_product_upper);
                Interval _5835 = _14642;
                Interval _5810 = _5832;
                Interval _5811 = _5835;
                float _5807 = as_type<float>(as_type<uint>(_5811.hi) ^ 2147483648u);
                float _5808 = as_type<float>(as_type<uint>(_5811.lo) ^ 2147483648u);
                _5805.lo = _5807;
                _5805.hi = _5808;
                Interval _5806 = _5805;
                Interval _5809 = _5806;
                Interval _5812 = _5809;
                Interval _14662 = iadd(_5810, _5812, intervalFailed);
                Interval _5813 = _14662;
                Interval _5836 = _5813;
                Interval _5837 = param_var_a_23.x;
                Interval _5838 = param_var_b_18.y;
                Interval _14668 = imul(_5837, _5838, intervalFailed, optical_product_upper);
                Interval _5839 = _14668;
                Interval _5840 = param_var_a_23.y;
                Interval _5841 = param_var_b_18.x;
                Interval _14673 = imul(_5840, _5841, intervalFailed, optical_product_upper);
                Interval _5842 = _14673;
                Interval _5801 = _5839;
                Interval _5802 = _5842;
                float _5798 = as_type<float>(as_type<uint>(_5802.hi) ^ 2147483648u);
                float _5799 = as_type<float>(as_type<uint>(_5802.lo) ^ 2147483648u);
                _5796.lo = _5798;
                _5796.hi = _5799;
                Interval _5797 = _5796;
                Interval _5800 = _5797;
                Interval _5803 = _5800;
                Interval _14693 = iadd(_5801, _5803, intervalFailed);
                Interval _5804 = _14693;
                Interval _5843 = _5804;
                _5794.x = _5829;
                _5794.y = _5836;
                _5794.z = _5843;
                Interval3 _5795 = _5794;
                Interval3 _5844 = _5795;
                Interval3 param_var_a_24 = _5844;
                Interval3 _5787 = param_var_a_24;
                float _5788 = 1.0;
                float _5784 = _5788;
                float _5785 = _5788;
                _5782.lo = _5784;
                _5782.hi = _5785;
                Interval _5783 = _5782;
                Interval _5786 = _5783;
                Interval _5789 = _5786;
                Interval3 _5790 = param_var_a_24;
                Interval _5773 = _5790.x;
                bool _5766 = false;
                if (_5773.lo <= 0.0)
                {
                    _5766 = _5773.hi >= 0.0;
                }
                if (_5766)
                {
                    _5765 = 0.0;
                }
                else
                {
                    _5765 = precise::min(abs(_5773.lo), abs(_5773.hi));
                }
                float _5764 = _5765;
                float _5767 = precise::max(abs(_5773.lo), abs(_5773.hi));
                float _5768 = spvFMul(_5764, _5764);
                float _14746 = interval_down(_5768, intervalFailed);
                float _5769 = precise::max(0.0, _14746);
                float _5770 = spvFMul(_5767, _5767);
                float _14750 = interval_up(_5770, intervalFailed);
                float _5771 = _14750;
                _5762.lo = _5769;
                _5762.hi = _5771;
                Interval _5763 = _5762;
                Interval _5772 = _5763;
                Interval _5774 = _5772;
                Interval _5775 = _5790.y;
                bool _5755 = false;
                if (_5775.lo <= 0.0)
                {
                    _5755 = _5775.hi >= 0.0;
                }
                if (_5755)
                {
                    _5754 = 0.0;
                }
                else
                {
                    _5754 = precise::min(abs(_5775.lo), abs(_5775.hi));
                }
                float _5753 = _5754;
                float _5756 = precise::max(abs(_5775.lo), abs(_5775.hi));
                float _5757 = spvFMul(_5753, _5753);
                float _14789 = interval_down(_5757, intervalFailed);
                float _5758 = precise::max(0.0, _14789);
                float _5759 = spvFMul(_5756, _5756);
                float _14793 = interval_up(_5759, intervalFailed);
                float _5760 = _14793;
                _5751.lo = _5758;
                _5751.hi = _5760;
                Interval _5752 = _5751;
                Interval _5761 = _5752;
                Interval _5776 = _5761;
                Interval _14801 = iadd(_5774, _5776, intervalFailed);
                Interval _5777 = _14801;
                Interval _5778 = _5790.z;
                bool _5744 = false;
                if (_5778.lo <= 0.0)
                {
                    _5744 = _5778.hi >= 0.0;
                }
                if (_5744)
                {
                    _5743 = 0.0;
                }
                else
                {
                    _5743 = precise::min(abs(_5778.lo), abs(_5778.hi));
                }
                float _5742 = _5743;
                float _5745 = precise::max(abs(_5778.lo), abs(_5778.hi));
                float _5746 = spvFMul(_5742, _5742);
                float _14833 = interval_down(_5746, intervalFailed);
                float _5747 = precise::max(0.0, _14833);
                float _5748 = spvFMul(_5745, _5745);
                float _14837 = interval_up(_5748, intervalFailed);
                float _5749 = _14837;
                _5740.lo = _5747;
                _5740.hi = _5749;
                Interval _5741 = _5740;
                Interval _5750 = _5741;
                Interval _5779 = _5750;
                Interval _14845 = iadd(_5777, _5779, intervalFailed);
                Interval _5780 = _14845;
                Interval _14846 = isqrt(_5780, intervalFailed);
                Interval _5781 = _14846;
                Interval _5791 = _5781;
                Interval _14848 = idiv(_5789, _5791, intervalFailed, interval_divide_upper);
                Interval _5792 = _14848;
                Interval _5730 = _5787.x;
                Interval _5731 = _5792;
                Interval _14852 = imul(_5730, _5731, intervalFailed, optical_product_upper);
                Interval _5732 = _14852;
                Interval _5733 = _5787.y;
                Interval _5734 = _5792;
                Interval _14856 = imul(_5733, _5734, intervalFailed, optical_product_upper);
                Interval _5735 = _14856;
                Interval _5736 = _5787.z;
                Interval _5737 = _5792;
                Interval _14860 = imul(_5736, _5737, intervalFailed, optical_product_upper);
                Interval _5738 = _14860;
                _5728.x = _5732;
                _5728.y = _5735;
                _5728.z = _5738;
                Interval3 _5729 = _5728;
                Interval3 _5739 = _5729;
                Interval3 _5793 = _5739;
                t = _5793;
            }
            else
            {
                if (ay.lo >= 0.949999988079071044921875)
                {
                    Interval3 param_var_a_25 = outgoing;
                    float3 param_var_v_4 = float3(1.0, 0.0, 0.0);
                    float _5721 = param_var_v_4.x;
                    float _5718 = _5721;
                    float _5719 = _5721;
                    _5716.lo = _5718;
                    _5716.hi = _5719;
                    Interval _5717 = _5716;
                    Interval _5720 = _5717;
                    Interval _5722 = _5720;
                    float _5723 = param_var_v_4.y;
                    float _5713 = _5723;
                    float _5714 = _5723;
                    _5711.lo = _5713;
                    _5711.hi = _5714;
                    Interval _5712 = _5711;
                    Interval _5715 = _5712;
                    Interval _5724 = _5715;
                    float _5725 = param_var_v_4.z;
                    float _5708 = _5725;
                    float _5709 = _5725;
                    _5706.lo = _5708;
                    _5706.hi = _5709;
                    Interval _5707 = _5706;
                    Interval _5710 = _5707;
                    Interval _5726 = _5710;
                    _5704.x = _5722;
                    _5704.y = _5724;
                    _5704.z = _5726;
                    Interval3 _5705 = _5704;
                    Interval3 _5727 = _5705;
                    Interval3 param_var_b_19 = _5727;
                    Interval _5682 = param_var_a_25.y;
                    Interval _5683 = param_var_b_19.z;
                    Interval _14921 = imul(_5682, _5683, intervalFailed, optical_product_upper);
                    Interval _5684 = _14921;
                    Interval _5685 = param_var_a_25.z;
                    Interval _5686 = param_var_b_19.y;
                    Interval _14926 = imul(_5685, _5686, intervalFailed, optical_product_upper);
                    Interval _5687 = _14926;
                    Interval _5678 = _5684;
                    Interval _5679 = _5687;
                    float _5675 = as_type<float>(as_type<uint>(_5679.hi) ^ 2147483648u);
                    float _5676 = as_type<float>(as_type<uint>(_5679.lo) ^ 2147483648u);
                    _5673.lo = _5675;
                    _5673.hi = _5676;
                    Interval _5674 = _5673;
                    Interval _5677 = _5674;
                    Interval _5680 = _5677;
                    Interval _14946 = iadd(_5678, _5680, intervalFailed);
                    Interval _5681 = _14946;
                    Interval _5688 = _5681;
                    Interval _5689 = param_var_a_25.z;
                    Interval _5690 = param_var_b_19.x;
                    Interval _14952 = imul(_5689, _5690, intervalFailed, optical_product_upper);
                    Interval _5691 = _14952;
                    Interval _5692 = param_var_a_25.x;
                    Interval _5693 = param_var_b_19.z;
                    Interval _14957 = imul(_5692, _5693, intervalFailed, optical_product_upper);
                    Interval _5694 = _14957;
                    Interval _5669 = _5691;
                    Interval _5670 = _5694;
                    float _5666 = as_type<float>(as_type<uint>(_5670.hi) ^ 2147483648u);
                    float _5667 = as_type<float>(as_type<uint>(_5670.lo) ^ 2147483648u);
                    _5664.lo = _5666;
                    _5664.hi = _5667;
                    Interval _5665 = _5664;
                    Interval _5668 = _5665;
                    Interval _5671 = _5668;
                    Interval _14977 = iadd(_5669, _5671, intervalFailed);
                    Interval _5672 = _14977;
                    Interval _5695 = _5672;
                    Interval _5696 = param_var_a_25.x;
                    Interval _5697 = param_var_b_19.y;
                    Interval _14983 = imul(_5696, _5697, intervalFailed, optical_product_upper);
                    Interval _5698 = _14983;
                    Interval _5699 = param_var_a_25.y;
                    Interval _5700 = param_var_b_19.x;
                    Interval _14988 = imul(_5699, _5700, intervalFailed, optical_product_upper);
                    Interval _5701 = _14988;
                    Interval _5660 = _5698;
                    Interval _5661 = _5701;
                    float _5657 = as_type<float>(as_type<uint>(_5661.hi) ^ 2147483648u);
                    float _5658 = as_type<float>(as_type<uint>(_5661.lo) ^ 2147483648u);
                    _5655.lo = _5657;
                    _5655.hi = _5658;
                    Interval _5656 = _5655;
                    Interval _5659 = _5656;
                    Interval _5662 = _5659;
                    Interval _15008 = iadd(_5660, _5662, intervalFailed);
                    Interval _5663 = _15008;
                    Interval _5702 = _5663;
                    _5653.x = _5688;
                    _5653.y = _5695;
                    _5653.z = _5702;
                    Interval3 _5654 = _5653;
                    Interval3 _5703 = _5654;
                    Interval3 param_var_a_26 = _5703;
                    Interval3 _5646 = param_var_a_26;
                    float _5647 = 1.0;
                    float _5643 = _5647;
                    float _5644 = _5647;
                    _5641.lo = _5643;
                    _5641.hi = _5644;
                    Interval _5642 = _5641;
                    Interval _5645 = _5642;
                    Interval _5648 = _5645;
                    Interval3 _5649 = param_var_a_26;
                    Interval _5632 = _5649.x;
                    bool _5625 = false;
                    if (_5632.lo <= 0.0)
                    {
                        _5625 = _5632.hi >= 0.0;
                    }
                    if (_5625)
                    {
                        _5624 = 0.0;
                    }
                    else
                    {
                        _5624 = precise::min(abs(_5632.lo), abs(_5632.hi));
                    }
                    float _5623 = _5624;
                    float _5626 = precise::max(abs(_5632.lo), abs(_5632.hi));
                    float _5627 = spvFMul(_5623, _5623);
                    float _15061 = interval_down(_5627, intervalFailed);
                    float _5628 = precise::max(0.0, _15061);
                    float _5629 = spvFMul(_5626, _5626);
                    float _15065 = interval_up(_5629, intervalFailed);
                    float _5630 = _15065;
                    _5621.lo = _5628;
                    _5621.hi = _5630;
                    Interval _5622 = _5621;
                    Interval _5631 = _5622;
                    Interval _5633 = _5631;
                    Interval _5634 = _5649.y;
                    bool _5614 = false;
                    if (_5634.lo <= 0.0)
                    {
                        _5614 = _5634.hi >= 0.0;
                    }
                    if (_5614)
                    {
                        _5613 = 0.0;
                    }
                    else
                    {
                        _5613 = precise::min(abs(_5634.lo), abs(_5634.hi));
                    }
                    float _5612 = _5613;
                    float _5615 = precise::max(abs(_5634.lo), abs(_5634.hi));
                    float _5616 = spvFMul(_5612, _5612);
                    float _15104 = interval_down(_5616, intervalFailed);
                    float _5617 = precise::max(0.0, _15104);
                    float _5618 = spvFMul(_5615, _5615);
                    float _15108 = interval_up(_5618, intervalFailed);
                    float _5619 = _15108;
                    _5610.lo = _5617;
                    _5610.hi = _5619;
                    Interval _5611 = _5610;
                    Interval _5620 = _5611;
                    Interval _5635 = _5620;
                    Interval _15116 = iadd(_5633, _5635, intervalFailed);
                    Interval _5636 = _15116;
                    Interval _5637 = _5649.z;
                    bool _5603 = false;
                    if (_5637.lo <= 0.0)
                    {
                        _5603 = _5637.hi >= 0.0;
                    }
                    if (_5603)
                    {
                        _5602 = 0.0;
                    }
                    else
                    {
                        _5602 = precise::min(abs(_5637.lo), abs(_5637.hi));
                    }
                    float _5601 = _5602;
                    float _5604 = precise::max(abs(_5637.lo), abs(_5637.hi));
                    float _5605 = spvFMul(_5601, _5601);
                    float _15148 = interval_down(_5605, intervalFailed);
                    float _5606 = precise::max(0.0, _15148);
                    float _5607 = spvFMul(_5604, _5604);
                    float _15152 = interval_up(_5607, intervalFailed);
                    float _5608 = _15152;
                    _5599.lo = _5606;
                    _5599.hi = _5608;
                    Interval _5600 = _5599;
                    Interval _5609 = _5600;
                    Interval _5638 = _5609;
                    Interval _15160 = iadd(_5636, _5638, intervalFailed);
                    Interval _5639 = _15160;
                    Interval _15161 = isqrt(_5639, intervalFailed);
                    Interval _5640 = _15161;
                    Interval _5650 = _5640;
                    Interval _15163 = idiv(_5648, _5650, intervalFailed, interval_divide_upper);
                    Interval _5651 = _15163;
                    Interval _5589 = _5646.x;
                    Interval _5590 = _5651;
                    Interval _15167 = imul(_5589, _5590, intervalFailed, optical_product_upper);
                    Interval _5591 = _15167;
                    Interval _5592 = _5646.y;
                    Interval _5593 = _5651;
                    Interval _15171 = imul(_5592, _5593, intervalFailed, optical_product_upper);
                    Interval _5594 = _15171;
                    Interval _5595 = _5646.z;
                    Interval _5596 = _5651;
                    Interval _15175 = imul(_5595, _5596, intervalFailed, optical_product_upper);
                    Interval _5597 = _15175;
                    _5587.x = _5591;
                    _5587.y = _5594;
                    _5587.z = _5597;
                    Interval3 _5588 = _5587;
                    Interval3 _5598 = _5588;
                    Interval3 _5652 = _5598;
                    t = _5652;
                }
                else
                {
                    intervalFailed = true;
                    return false;
                }
            }
            int2 tap = _2253[uint(receiver.settings.y)];
            Interval3 param_var_a_27 = outgoing;
            Interval3 param_var_a_28 = t;
            float param_var_n_5 = float(tap.x);
            float param_var_d_3 = 1000.0;
            Interval _15197 = iratio(param_var_n_5, param_var_d_3, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_20 = _15197;
            Interval _5577 = param_var_a_28.x;
            Interval _5578 = param_var_b_20;
            Interval _15201 = imul(_5577, _5578, intervalFailed, optical_product_upper);
            Interval _5579 = _15201;
            Interval _5580 = param_var_a_28.y;
            Interval _5581 = param_var_b_20;
            Interval _15205 = imul(_5580, _5581, intervalFailed, optical_product_upper);
            Interval _5582 = _15205;
            Interval _5583 = param_var_a_28.z;
            Interval _5584 = param_var_b_20;
            Interval _15209 = imul(_5583, _5584, intervalFailed, optical_product_upper);
            Interval _5585 = _15209;
            _5575.x = _5579;
            _5575.y = _5582;
            _5575.z = _5585;
            Interval3 _5576 = _5575;
            Interval3 _5586 = _5576;
            Interval3 param_var_a_29 = _5586;
            Interval3 param_var_a_30 = outgoing;
            Interval3 param_var_b_21 = t;
            Interval _5553 = param_var_a_30.y;
            Interval _5554 = param_var_b_21.z;
            Interval _15225 = imul(_5553, _5554, intervalFailed, optical_product_upper);
            Interval _5555 = _15225;
            Interval _5556 = param_var_a_30.z;
            Interval _5557 = param_var_b_21.y;
            Interval _15230 = imul(_5556, _5557, intervalFailed, optical_product_upper);
            Interval _5558 = _15230;
            Interval _5549 = _5555;
            Interval _5550 = _5558;
            float _5546 = as_type<float>(as_type<uint>(_5550.hi) ^ 2147483648u);
            float _5547 = as_type<float>(as_type<uint>(_5550.lo) ^ 2147483648u);
            _5544.lo = _5546;
            _5544.hi = _5547;
            Interval _5545 = _5544;
            Interval _5548 = _5545;
            Interval _5551 = _5548;
            Interval _15250 = iadd(_5549, _5551, intervalFailed);
            Interval _5552 = _15250;
            Interval _5559 = _5552;
            Interval _5560 = param_var_a_30.z;
            Interval _5561 = param_var_b_21.x;
            Interval _15256 = imul(_5560, _5561, intervalFailed, optical_product_upper);
            Interval _5562 = _15256;
            Interval _5563 = param_var_a_30.x;
            Interval _5564 = param_var_b_21.z;
            Interval _15261 = imul(_5563, _5564, intervalFailed, optical_product_upper);
            Interval _5565 = _15261;
            Interval _5540 = _5562;
            Interval _5541 = _5565;
            float _5537 = as_type<float>(as_type<uint>(_5541.hi) ^ 2147483648u);
            float _5538 = as_type<float>(as_type<uint>(_5541.lo) ^ 2147483648u);
            _5535.lo = _5537;
            _5535.hi = _5538;
            Interval _5536 = _5535;
            Interval _5539 = _5536;
            Interval _5542 = _5539;
            Interval _15281 = iadd(_5540, _5542, intervalFailed);
            Interval _5543 = _15281;
            Interval _5566 = _5543;
            Interval _5567 = param_var_a_30.x;
            Interval _5568 = param_var_b_21.y;
            Interval _15287 = imul(_5567, _5568, intervalFailed, optical_product_upper);
            Interval _5569 = _15287;
            Interval _5570 = param_var_a_30.y;
            Interval _5571 = param_var_b_21.x;
            Interval _15292 = imul(_5570, _5571, intervalFailed, optical_product_upper);
            Interval _5572 = _15292;
            Interval _5531 = _5569;
            Interval _5532 = _5572;
            float _5528 = as_type<float>(as_type<uint>(_5532.hi) ^ 2147483648u);
            float _5529 = as_type<float>(as_type<uint>(_5532.lo) ^ 2147483648u);
            _5526.lo = _5528;
            _5526.hi = _5529;
            Interval _5527 = _5526;
            Interval _5530 = _5527;
            Interval _5533 = _5530;
            Interval _15312 = iadd(_5531, _5533, intervalFailed);
            Interval _5534 = _15312;
            Interval _5573 = _5534;
            _5524.x = _5559;
            _5524.y = _5566;
            _5524.z = _5573;
            Interval3 _5525 = _5524;
            Interval3 _5574 = _5525;
            Interval3 param_var_a_31 = _5574;
            float param_var_n_6 = float(tap.y);
            float param_var_d_4 = 1000.0;
            Interval _15326 = iratio(param_var_n_6, param_var_d_4, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_22 = _15326;
            Interval _5514 = param_var_a_31.x;
            Interval _5515 = param_var_b_22;
            Interval _15330 = imul(_5514, _5515, intervalFailed, optical_product_upper);
            Interval _5516 = _15330;
            Interval _5517 = param_var_a_31.y;
            Interval _5518 = param_var_b_22;
            Interval _15334 = imul(_5517, _5518, intervalFailed, optical_product_upper);
            Interval _5519 = _15334;
            Interval _5520 = param_var_a_31.z;
            Interval _5521 = param_var_b_22;
            Interval _15338 = imul(_5520, _5521, intervalFailed, optical_product_upper);
            Interval _5522 = _15338;
            _5512.x = _5516;
            _5512.y = _5519;
            _5512.z = _5522;
            Interval3 _5513 = _5512;
            Interval3 _5523 = _5513;
            Interval3 param_var_b_23 = _5523;
            Interval _5502 = param_var_a_29.x;
            Interval _5503 = param_var_b_23.x;
            Interval _15352 = iadd(_5502, _5503, intervalFailed);
            Interval _5504 = _15352;
            Interval _5505 = param_var_a_29.y;
            Interval _5506 = param_var_b_23.y;
            Interval _15357 = iadd(_5505, _5506, intervalFailed);
            Interval _5507 = _15357;
            Interval _5508 = param_var_a_29.z;
            Interval _5509 = param_var_b_23.z;
            Interval _15362 = iadd(_5508, _5509, intervalFailed);
            Interval _5510 = _15362;
            _5500.x = _5504;
            _5500.y = _5507;
            _5500.z = _5510;
            Interval3 _5501 = _5500;
            Interval3 _5511 = _5501;
            Interval3 param_var_a_32 = _5511;
            float param_var_x_8 = receiver.settings.x;
            float _5497 = param_var_x_8;
            float _5498 = param_var_x_8;
            _5495.lo = _5497;
            _5495.hi = _5498;
            Interval _5496 = _5495;
            Interval _5499 = _5496;
            Interval param_var_a_33 = _5499;
            bool _5488 = false;
            if (param_var_a_33.lo <= 0.0)
            {
                _5488 = param_var_a_33.hi >= 0.0;
            }
            if (_5488)
            {
                _5487 = 0.0;
            }
            else
            {
                _5487 = precise::min(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            }
            float _5486 = _5487;
            float _5489 = precise::max(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            float _5490 = spvFMul(_5486, _5486);
            float _15413 = interval_down(_5490, intervalFailed);
            float _5491 = precise::max(0.0, _15413);
            float _5492 = spvFMul(_5489, _5489);
            float _15417 = interval_up(_5492, intervalFailed);
            float _5493 = _15417;
            _5484.lo = _5491;
            _5484.hi = _5493;
            Interval _5485 = _5484;
            Interval _5494 = _5485;
            Interval param_var_b_24 = _5494;
            Interval _5474 = param_var_a_32.x;
            Interval _5475 = param_var_b_24;
            Interval _15428 = imul(_5474, _5475, intervalFailed, optical_product_upper);
            Interval _5476 = _15428;
            Interval _5477 = param_var_a_32.y;
            Interval _5478 = param_var_b_24;
            Interval _15432 = imul(_5477, _5478, intervalFailed, optical_product_upper);
            Interval _5479 = _15432;
            Interval _5480 = param_var_a_32.z;
            Interval _5481 = param_var_b_24;
            Interval _15436 = imul(_5480, _5481, intervalFailed, optical_product_upper);
            Interval _5482 = _15436;
            _5472.x = _5476;
            _5472.y = _5479;
            _5472.z = _5482;
            Interval3 _5473 = _5472;
            Interval3 _5483 = _5473;
            Interval3 param_var_b_25 = _5483;
            Interval _5462 = param_var_a_27.x;
            Interval _5463 = param_var_b_25.x;
            Interval _15450 = iadd(_5462, _5463, intervalFailed);
            Interval _5464 = _15450;
            Interval _5465 = param_var_a_27.y;
            Interval _5466 = param_var_b_25.y;
            Interval _15455 = iadd(_5465, _5466, intervalFailed);
            Interval _5467 = _15455;
            Interval _5468 = param_var_a_27.z;
            Interval _5469 = param_var_b_25.z;
            Interval _15460 = iadd(_5468, _5469, intervalFailed);
            Interval _5470 = _15460;
            _5460.x = _5464;
            _5460.y = _5467;
            _5460.z = _5470;
            Interval3 _5461 = _5460;
            Interval3 _5471 = _5461;
            Interval3 param_var_a_34 = _5471;
            Interval3 _5453 = param_var_a_34;
            float _5454 = 1.0;
            float _5450 = _5454;
            float _5451 = _5454;
            _5448.lo = _5450;
            _5448.hi = _5451;
            Interval _5449 = _5448;
            Interval _5452 = _5449;
            Interval _5455 = _5452;
            Interval3 _5456 = param_var_a_34;
            Interval _5439 = _5456.x;
            bool _5432 = false;
            if (_5439.lo <= 0.0)
            {
                _5432 = _5439.hi >= 0.0;
            }
            if (_5432)
            {
                _5431 = 0.0;
            }
            else
            {
                _5431 = precise::min(abs(_5439.lo), abs(_5439.hi));
            }
            float _5430 = _5431;
            float _5433 = precise::max(abs(_5439.lo), abs(_5439.hi));
            float _5434 = spvFMul(_5430, _5430);
            float _15512 = interval_down(_5434, intervalFailed);
            float _5435 = precise::max(0.0, _15512);
            float _5436 = spvFMul(_5433, _5433);
            float _15516 = interval_up(_5436, intervalFailed);
            float _5437 = _15516;
            _5428.lo = _5435;
            _5428.hi = _5437;
            Interval _5429 = _5428;
            Interval _5438 = _5429;
            Interval _5440 = _5438;
            Interval _5441 = _5456.y;
            bool _5421 = false;
            if (_5441.lo <= 0.0)
            {
                _5421 = _5441.hi >= 0.0;
            }
            if (_5421)
            {
                _5420 = 0.0;
            }
            else
            {
                _5420 = precise::min(abs(_5441.lo), abs(_5441.hi));
            }
            float _5419 = _5420;
            float _5422 = precise::max(abs(_5441.lo), abs(_5441.hi));
            float _5423 = spvFMul(_5419, _5419);
            float _15555 = interval_down(_5423, intervalFailed);
            float _5424 = precise::max(0.0, _15555);
            float _5425 = spvFMul(_5422, _5422);
            float _15559 = interval_up(_5425, intervalFailed);
            float _5426 = _15559;
            _5417.lo = _5424;
            _5417.hi = _5426;
            Interval _5418 = _5417;
            Interval _5427 = _5418;
            Interval _5442 = _5427;
            Interval _15567 = iadd(_5440, _5442, intervalFailed);
            Interval _5443 = _15567;
            Interval _5444 = _5456.z;
            bool _5410 = false;
            if (_5444.lo <= 0.0)
            {
                _5410 = _5444.hi >= 0.0;
            }
            if (_5410)
            {
                _5409 = 0.0;
            }
            else
            {
                _5409 = precise::min(abs(_5444.lo), abs(_5444.hi));
            }
            float _5408 = _5409;
            float _5411 = precise::max(abs(_5444.lo), abs(_5444.hi));
            float _5412 = spvFMul(_5408, _5408);
            float _15599 = interval_down(_5412, intervalFailed);
            float _5413 = precise::max(0.0, _15599);
            float _5414 = spvFMul(_5411, _5411);
            float _15603 = interval_up(_5414, intervalFailed);
            float _5415 = _15603;
            _5406.lo = _5413;
            _5406.hi = _5415;
            Interval _5407 = _5406;
            Interval _5416 = _5407;
            Interval _5445 = _5416;
            Interval _15611 = iadd(_5443, _5445, intervalFailed);
            Interval _5446 = _15611;
            Interval _15612 = isqrt(_5446, intervalFailed);
            Interval _5447 = _15612;
            Interval _5457 = _5447;
            Interval _15614 = idiv(_5455, _5457, intervalFailed, interval_divide_upper);
            Interval _5458 = _15614;
            Interval _5396 = _5453.x;
            Interval _5397 = _5458;
            Interval _15618 = imul(_5396, _5397, intervalFailed, optical_product_upper);
            Interval _5398 = _15618;
            Interval _5399 = _5453.y;
            Interval _5400 = _5458;
            Interval _15622 = imul(_5399, _5400, intervalFailed, optical_product_upper);
            Interval _5401 = _15622;
            Interval _5402 = _5453.z;
            Interval _5403 = _5458;
            Interval _15626 = imul(_5402, _5403, intervalFailed, optical_product_upper);
            Interval _5404 = _15626;
            _5394.x = _5398;
            _5394.y = _5401;
            _5394.z = _5404;
            Interval3 _5395 = _5394;
            Interval3 _5405 = _5395;
            Interval3 _5459 = _5405;
            Interval3 rough = _5459;
            Interval3 param_var_a_35 = rough;
            Interval3 param_var_b_26 = n;
            Interval _5383 = param_var_a_35.x;
            Interval _5384 = param_var_b_26.x;
            Interval _15643 = imul(_5383, _5384, intervalFailed, optical_product_upper);
            Interval _5385 = _15643;
            Interval _5386 = param_var_a_35.y;
            Interval _5387 = param_var_b_26.y;
            Interval _15648 = imul(_5386, _5387, intervalFailed, optical_product_upper);
            Interval _5388 = _15648;
            Interval _15649 = iadd(_5385, _5388, intervalFailed);
            Interval _5389 = _15649;
            Interval _5390 = param_var_a_35.z;
            Interval _5391 = param_var_b_26.z;
            Interval _15654 = imul(_5390, _5391, intervalFailed, optical_product_upper);
            Interval _5392 = _15654;
            Interval _15655 = iadd(_5389, _5392, intervalFailed);
            Interval _5393 = _15655;
            Interval nd = _5393;
            if (nd.lo > 0.0)
            {
                outgoing = rough;
            }
            else
            {
                if (nd.hi > 0.0)
                {
                    Interval3 param_var_a_36 = outgoing;
                    Interval3 param_var_b_27 = rough;
                    Interval _5373 = param_var_a_36.x;
                    Interval _5374 = param_var_b_27.x;
                    float _5370 = precise::min(_5373.lo, _5374.lo);
                    float _5371 = precise::max(_5373.hi, _5374.hi);
                    _5368.lo = _5370;
                    _5368.hi = _5371;
                    Interval _5369 = _5368;
                    Interval _5372 = _5369;
                    Interval _5375 = _5372;
                    Interval _5376 = param_var_a_36.y;
                    Interval _5377 = param_var_b_27.y;
                    float _5365 = precise::min(_5376.lo, _5377.lo);
                    float _5366 = precise::max(_5376.hi, _5377.hi);
                    _5363.lo = _5365;
                    _5363.hi = _5366;
                    Interval _5364 = _5363;
                    Interval _5367 = _5364;
                    Interval _5378 = _5367;
                    Interval _5379 = param_var_a_36.z;
                    Interval _5380 = param_var_b_27.z;
                    float _5360 = precise::min(_5379.lo, _5380.lo);
                    float _5361 = precise::max(_5379.hi, _5380.hi);
                    _5358.lo = _5360;
                    _5358.hi = _5361;
                    Interval _5359 = _5358;
                    Interval _5362 = _5359;
                    Interval _5381 = _5362;
                    _5356.x = _5375;
                    _5356.y = _5378;
                    _5356.z = _5381;
                    Interval3 _5357 = _5356;
                    Interval3 _5382 = _5357;
                    outgoing = _5382;
                }
            }
        }
        if (intervalFailed)
        {
            return false;
        }
    }
    Interval3 travel = target;
    if (finiteTerminal)
    {
        Interval3 param_var_a_37 = travel;
        Interval3 param_var_b_28 = origin;
        Interval3 _5347 = param_var_a_37;
        Interval _5348 = param_var_b_28.x;
        float _5344 = as_type<float>(as_type<uint>(_5348.hi) ^ 2147483648u);
        float _5345 = as_type<float>(as_type<uint>(_5348.lo) ^ 2147483648u);
        Interval _5342;
        _5342.lo = _5344;
        _5342.hi = _5345;
        Interval _5343 = _5342;
        Interval _5346 = _5343;
        Interval _5349 = _5346;
        Interval _5350 = param_var_b_28.y;
        float _5339 = as_type<float>(as_type<uint>(_5350.hi) ^ 2147483648u);
        float _5340 = as_type<float>(as_type<uint>(_5350.lo) ^ 2147483648u);
        Interval _5337;
        _5337.lo = _5339;
        _5337.hi = _5340;
        Interval _5338 = _5337;
        Interval _5341 = _5338;
        Interval _5351 = _5341;
        Interval _5352 = param_var_b_28.z;
        float _5334 = as_type<float>(as_type<uint>(_5352.hi) ^ 2147483648u);
        float _5335 = as_type<float>(as_type<uint>(_5352.lo) ^ 2147483648u);
        Interval _5332;
        _5332.lo = _5334;
        _5332.hi = _5335;
        Interval _5333 = _5332;
        Interval _5336 = _5333;
        Interval _5353 = _5336;
        Interval3 _5330;
        _5330.x = _5349;
        _5330.y = _5351;
        _5330.z = _5353;
        Interval3 _5331 = _5330;
        Interval3 _5354 = _5331;
        Interval _5320 = _5347.x;
        Interval _5321 = _5354.x;
        Interval _15814 = iadd(_5320, _5321, intervalFailed);
        Interval _5322 = _15814;
        Interval _5323 = _5347.y;
        Interval _5324 = _5354.y;
        Interval _15819 = iadd(_5323, _5324, intervalFailed);
        Interval _5325 = _15819;
        Interval _5326 = _5347.z;
        Interval _5327 = _5354.z;
        Interval _15824 = iadd(_5326, _5327, intervalFailed);
        Interval _5328 = _15824;
        Interval3 _5318;
        _5318.x = _5322;
        _5318.y = _5325;
        _5318.z = _5328;
        Interval3 _5319 = _5318;
        Interval3 _5329 = _5319;
        Interval3 _5355 = _5329;
        travel = _5355;
    }
    Interval3 param_var_a_38 = travel;
    Interval _5309 = param_var_a_38.x;
    bool _5302 = false;
    if (_5309.lo <= 0.0)
    {
        _5302 = _5309.hi >= 0.0;
    }
    float _5301;
    if (_5302)
    {
        _5301 = 0.0;
    }
    else
    {
        _5301 = precise::min(abs(_5309.lo), abs(_5309.hi));
    }
    float _5300 = _5301;
    float _5303 = precise::max(abs(_5309.lo), abs(_5309.hi));
    float _5304 = spvFMul(_5300, _5300);
    float _15867 = interval_down(_5304, intervalFailed);
    float _5305 = precise::max(0.0, _15867);
    float _5306 = spvFMul(_5303, _5303);
    float _15871 = interval_up(_5306, intervalFailed);
    float _5307 = _15871;
    Interval _5298;
    _5298.lo = _5305;
    _5298.hi = _5307;
    Interval _5299 = _5298;
    Interval _5308 = _5299;
    Interval _5310 = _5308;
    Interval _5311 = param_var_a_38.y;
    bool _5291 = false;
    if (_5311.lo <= 0.0)
    {
        _5291 = _5311.hi >= 0.0;
    }
    float _5290;
    if (_5291)
    {
        _5290 = 0.0;
    }
    else
    {
        _5290 = precise::min(abs(_5311.lo), abs(_5311.hi));
    }
    float _5289 = _5290;
    float _5292 = precise::max(abs(_5311.lo), abs(_5311.hi));
    float _5293 = spvFMul(_5289, _5289);
    float _15910 = interval_down(_5293, intervalFailed);
    float _5294 = precise::max(0.0, _15910);
    float _5295 = spvFMul(_5292, _5292);
    float _15914 = interval_up(_5295, intervalFailed);
    float _5296 = _15914;
    Interval _5287;
    _5287.lo = _5294;
    _5287.hi = _5296;
    Interval _5288 = _5287;
    Interval _5297 = _5288;
    Interval _5312 = _5297;
    Interval _15922 = iadd(_5310, _5312, intervalFailed);
    Interval _5313 = _15922;
    Interval _5314 = param_var_a_38.z;
    bool _5280 = false;
    if (_5314.lo <= 0.0)
    {
        _5280 = _5314.hi >= 0.0;
    }
    float _5279;
    if (_5280)
    {
        _5279 = 0.0;
    }
    else
    {
        _5279 = precise::min(abs(_5314.lo), abs(_5314.hi));
    }
    float _5278 = _5279;
    float _5281 = precise::max(abs(_5314.lo), abs(_5314.hi));
    float _5282 = spvFMul(_5278, _5278);
    float _15954 = interval_down(_5282, intervalFailed);
    float _5283 = precise::max(0.0, _15954);
    float _5284 = spvFMul(_5281, _5281);
    float _15958 = interval_up(_5284, intervalFailed);
    float _5285 = _15958;
    Interval _5276;
    _5276.lo = _5283;
    _5276.hi = _5285;
    Interval _5277 = _5276;
    Interval _5286 = _5277;
    Interval _5315 = _5286;
    Interval _15966 = iadd(_5313, _5315, intervalFailed);
    Interval _5316 = _15966;
    Interval _15967 = isqrt(_5316, intervalFailed);
    Interval _5317 = _15967;
    Interval len = _5317;
    Interval3 param_var_a_39 = travel;
    Interval3 param_var_b_29 = outgoing;
    Interval _5265 = param_var_a_39.x;
    Interval _5266 = param_var_b_29.x;
    Interval _15975 = imul(_5265, _5266, intervalFailed, optical_product_upper);
    Interval _5267 = _15975;
    Interval _5268 = param_var_a_39.y;
    Interval _5269 = param_var_b_29.y;
    Interval _15980 = imul(_5268, _5269, intervalFailed, optical_product_upper);
    Interval _5270 = _15980;
    Interval _15981 = iadd(_5267, _5270, intervalFailed);
    Interval _5271 = _15981;
    Interval _5272 = param_var_a_39.z;
    Interval _5273 = param_var_b_29.z;
    Interval _15986 = imul(_5272, _5273, intervalFailed, optical_product_upper);
    Interval _5274 = _15986;
    Interval _15987 = iadd(_5271, _5274, intervalFailed);
    Interval _5275 = _15987;
    Interval temp_var_Interval = _5275;
    bool temp_var_logical_7 = true;
    if ((isunordered(temp_var_Interval.hi, 0.0) || temp_var_Interval.hi > 0.0))
    {
        temp_var_logical_7 = len.hi <= 0.0;
    }
    if (temp_var_logical_7)
    {
        return !intervalFailed;
    }
    Interval3 param_var_a_40 = travel;
    Interval3 param_var_b_30 = outgoing;
    Interval _5243 = param_var_a_40.y;
    Interval _5244 = param_var_b_30.z;
    Interval _16004 = imul(_5243, _5244, intervalFailed, optical_product_upper);
    Interval _5245 = _16004;
    Interval _5246 = param_var_a_40.z;
    Interval _5247 = param_var_b_30.y;
    Interval _16009 = imul(_5246, _5247, intervalFailed, optical_product_upper);
    Interval _5248 = _16009;
    Interval _5239 = _5245;
    Interval _5240 = _5248;
    float _5236 = as_type<float>(as_type<uint>(_5240.hi) ^ 2147483648u);
    float _5237 = as_type<float>(as_type<uint>(_5240.lo) ^ 2147483648u);
    Interval _5234;
    _5234.lo = _5236;
    _5234.hi = _5237;
    Interval _5235 = _5234;
    Interval _5238 = _5235;
    Interval _5241 = _5238;
    Interval _16029 = iadd(_5239, _5241, intervalFailed);
    Interval _5242 = _16029;
    Interval _5249 = _5242;
    Interval _5250 = param_var_a_40.z;
    Interval _5251 = param_var_b_30.x;
    Interval _16035 = imul(_5250, _5251, intervalFailed, optical_product_upper);
    Interval _5252 = _16035;
    Interval _5253 = param_var_a_40.x;
    Interval _5254 = param_var_b_30.z;
    Interval _16040 = imul(_5253, _5254, intervalFailed, optical_product_upper);
    Interval _5255 = _16040;
    Interval _5230 = _5252;
    Interval _5231 = _5255;
    float _5227 = as_type<float>(as_type<uint>(_5231.hi) ^ 2147483648u);
    float _5228 = as_type<float>(as_type<uint>(_5231.lo) ^ 2147483648u);
    Interval _5225;
    _5225.lo = _5227;
    _5225.hi = _5228;
    Interval _5226 = _5225;
    Interval _5229 = _5226;
    Interval _5232 = _5229;
    Interval _16060 = iadd(_5230, _5232, intervalFailed);
    Interval _5233 = _16060;
    Interval _5256 = _5233;
    Interval _5257 = param_var_a_40.x;
    Interval _5258 = param_var_b_30.y;
    Interval _16066 = imul(_5257, _5258, intervalFailed, optical_product_upper);
    Interval _5259 = _16066;
    Interval _5260 = param_var_a_40.y;
    Interval _5261 = param_var_b_30.x;
    Interval _16071 = imul(_5260, _5261, intervalFailed, optical_product_upper);
    Interval _5262 = _16071;
    Interval _5221 = _5259;
    Interval _5222 = _5262;
    float _5218 = as_type<float>(as_type<uint>(_5222.hi) ^ 2147483648u);
    float _5219 = as_type<float>(as_type<uint>(_5222.lo) ^ 2147483648u);
    Interval _5216;
    _5216.lo = _5218;
    _5216.hi = _5219;
    Interval _5217 = _5216;
    Interval _5220 = _5217;
    Interval _5223 = _5220;
    Interval _16091 = iadd(_5221, _5223, intervalFailed);
    Interval _5224 = _16091;
    Interval _5263 = _5224;
    Interval3 _5214;
    _5214.x = _5249;
    _5214.y = _5256;
    _5214.z = _5263;
    Interval3 _5215 = _5214;
    Interval3 _5264 = _5215;
    Interval3 error = _5264;
    float param_var_x_9 = spvFMul(len.hi, 0.0040000001899898052215576171875);
    float _16104 = interval_up(param_var_x_9, intervalFailed);
    float param_var_a_41 = _16104;
    float param_var_b_31 = precise::max(receiver.projection.x, receiver.projection.y);
    bool param_var_upper = true;
    float _16112 = quotient_bound(param_var_a_41, param_var_b_31, param_var_upper, intervalFailed);
    float param_var_x_10 = _16112;
    float _16113 = interval_up(param_var_x_10, intervalFailed);
    float cap = _16113;
    bool temp_var_logical_8 = false;
    if (!intervalFailed)
    {
        bool temp_var_logical_9 = true;
        if ((isunordered(error.x.lo, cap) || error.x.lo <= cap))
        {
            temp_var_logical_9 = error.x.hi < (-cap);
        }
        bool temp_var_logical_10 = true;
        if (!temp_var_logical_9)
        {
            temp_var_logical_10 = error.y.lo > cap;
        }
        bool temp_var_logical_11 = true;
        if (!temp_var_logical_10)
        {
            temp_var_logical_11 = error.y.hi < (-cap);
        }
        bool temp_var_logical_12 = true;
        if (!temp_var_logical_11)
        {
            temp_var_logical_12 = error.z.lo > cap;
        }
        bool temp_var_logical_13 = true;
        if (!temp_var_logical_12)
        {
            temp_var_logical_13 = error.z.hi < (-cap);
        }
        temp_var_logical_8 = temp_var_logical_13;
    }
    return temp_var_logical_8;
}

static inline __attribute__((always_inline))
void src_feature_optical_stream_main(thread const uint3& group, thread const uint& lane, constant type_Settings& Settings, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, constant type_OpticalSettings& OpticalSettings, device type_ByteAddressBuffer& regions, device type_ByteAddressBuffer& frames, device type_ByteAddressBuffer& queries, device type_ByteAddressBuffer& workMap, device type_RWByteAddressBuffer& results, constant type_TargetSettings& TargetSettings, threadgroup uint& keptCount, threadgroup uint& excludedCount, threadgroup uint& hullX0, threadgroup uint& hullY0, threadgroup uint& hullX1, threadgroup uint& hullY1)
{
    bool temp_var_logical = true;
    if (group.x < 8192u)
    {
        temp_var_logical = group.y != 0u;
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        temp_var_logical_1 = group.z != 0u;
    }
    if (temp_var_logical_1)
    {
        return;
    }
    uint _2273 = (group.x * 8u) >> 2u;
    uint2 task = uint2(workMap._m0[_2273], workMap._m0[_2273 + 1u]);
    uint q = task.x;
    uint r = task.y;
    bool temp_var_logical_2 = true;
    if (q < Settings.queryCount)
    {
        temp_var_logical_2 = r >= 128u;
    }
    if (temp_var_logical_2)
    {
        return;
    }
    uint _input = q * 6160u;
    uint _output = (q * 4096u) + (r * 32u);
    uint count = regions._m0[_input >> 2u];
    uint status = regions._m0[(_input + 4u) >> 2u];
    if (lane == 0u)
    {
        uint _2304 = _output >> 2u;
        results._m0[_2304] = 0u;
        results._m0[_2304 + 1u] = status;
        results._m0[_2304 + 2u] = 0u;
        results._m0[_2304 + 3u] = 0u;
        uint _2311 = (_output + 16u) >> 2u;
        results._m0[_2311] = 0u;
        results._m0[_2311 + 1u] = 0u;
        results._m0[_2311 + 2u] = 0u;
        results._m0[_2311 + 3u] = 0u;
    }
    bool temp_var_logical_3 = true;
    if (status == 0u)
    {
        temp_var_logical_3 = r >= count;
    }
    if (temp_var_logical_3)
    {
        return;
    }
    bool temp_var_logical_4 = true;
    if (count <= 128u)
    {
        temp_var_logical_4 = OpticalSettings.opticalCapacity != 128u;
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        temp_var_logical_5 = OpticalSettings.opticalBudget == 0u;
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        temp_var_logical_6 = OpticalSettings.opticalBudget > 8192u;
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        temp_var_logical_7 = OpticalSettings.opticalDepth > 12u;
    }
    if (temp_var_logical_7)
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint at = q * 512u;
    uint _2350 = (at + 496u) >> 2u;
    if (any(uint4(frames._m0[_2350], frames._m0[_2350 + 1u], frames._m0[_2350 + 2u], frames._m0[_2350 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint _2368 = at >> 2u;
    ReflectionRoughFrame receiver;
    receiver.a = as_type<float4>(uint4(frames._m0[_2368], frames._m0[_2368 + 1u], frames._m0[_2368 + 2u], frames._m0[_2368 + 3u]));
    uint _2381 = (at + 16u) >> 2u;
    receiver.b = as_type<float4>(uint4(frames._m0[_2381], frames._m0[_2381 + 1u], frames._m0[_2381 + 2u], frames._m0[_2381 + 3u]));
    uint _2394 = (at + 32u) >> 2u;
    receiver.c = as_type<float4>(uint4(frames._m0[_2394], frames._m0[_2394 + 1u], frames._m0[_2394 + 2u], frames._m0[_2394 + 3u]));
    uint _2407 = (at + 48u) >> 2u;
    receiver.projection = as_type<float4>(uint4(frames._m0[_2407], frames._m0[_2407 + 1u], frames._m0[_2407 + 2u], frames._m0[_2407 + 3u]));
    uint _2420 = (at + 64u) >> 2u;
    receiver.extentClip = as_type<float4>(uint4(frames._m0[_2420], frames._m0[_2420 + 1u], frames._m0[_2420 + 2u], frames._m0[_2420 + 3u]));
    uint _2433 = (at + 80u) >> 2u;
    receiver.settings = as_type<float4>(uint4(frames._m0[_2433], frames._m0[_2433 + 1u], frames._m0[_2433 + 2u], frames._m0[_2433 + 3u]));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint h = 0u; h < 4u; h++)
    {
        uint _2449 = ((at + 96u) + (h * 48u)) >> 2u;
        planes[h].a = as_type<float4>(uint4(frames._m0[_2449], frames._m0[_2449 + 1u], frames._m0[_2449 + 2u], frames._m0[_2449 + 3u]));
        uint _2464 = ((at + 112u) + (h * 48u)) >> 2u;
        planes[h].b = as_type<float4>(uint4(frames._m0[_2464], frames._m0[_2464 + 1u], frames._m0[_2464 + 2u], frames._m0[_2464 + 3u]));
        uint _2479 = ((at + 128u) + (h * 48u)) >> 2u;
        planes[h].c = as_type<float4>(uint4(frames._m0[_2479], frames._m0[_2479 + 1u], frames._m0[_2479 + 2u], frames._m0[_2479 + 3u]));
    }
    uint _2494 = (at + 288u) >> 2u;
    ReflectionLiquidFrame liquid;
    liquid.planePoint = as_type<float4>(uint4(frames._m0[_2494], frames._m0[_2494 + 1u], frames._m0[_2494 + 2u], frames._m0[_2494 + 3u]));
    uint _2507 = (at + 304u) >> 2u;
    liquid.planeNormal = as_type<float4>(uint4(frames._m0[_2507], frames._m0[_2507 + 1u], frames._m0[_2507 + 2u], frames._m0[_2507 + 3u]));
    uint _2520 = (at + 320u) >> 2u;
    liquid.rotation0 = as_type<float4>(uint4(frames._m0[_2520], frames._m0[_2520 + 1u], frames._m0[_2520 + 2u], frames._m0[_2520 + 3u]));
    uint _2533 = (at + 336u) >> 2u;
    liquid.rotation1 = as_type<float4>(uint4(frames._m0[_2533], frames._m0[_2533 + 1u], frames._m0[_2533 + 2u], frames._m0[_2533 + 3u]));
    uint _2546 = (at + 352u) >> 2u;
    liquid.rotation2 = as_type<float4>(uint4(frames._m0[_2546], frames._m0[_2546 + 1u], frames._m0[_2546 + 2u], frames._m0[_2546 + 3u]));
    uint _2559 = (at + 368u) >> 2u;
    liquid.projection = as_type<float4>(uint4(frames._m0[_2559], frames._m0[_2559 + 1u], frames._m0[_2559 + 2u], frames._m0[_2559 + 3u]));
    uint _2572 = (at + 384u) >> 2u;
    liquid.extentClip = as_type<float4>(uint4(frames._m0[_2572], frames._m0[_2572 + 1u], frames._m0[_2572 + 2u], frames._m0[_2572 + 3u]));
    uint _2585 = (at + 400u) >> 2u;
    liquid.settings = as_type<float4>(uint4(frames._m0[_2585], frames._m0[_2585 + 1u], frames._m0[_2585 + 2u], frames._m0[_2585 + 3u]));
    uint _2598 = (at + 416u) >> 2u;
    float4 terminal = as_type<float4>(uint4(frames._m0[_2598], frames._m0[_2598 + 1u], frames._m0[_2598 + 2u], frames._m0[_2598 + 3u]));
    uint _2610 = (at + 480u) >> 2u;
    uint4 control = uint4(frames._m0[_2610], frames._m0[_2610 + 1u], frames._m0[_2610 + 2u], frames._m0[_2610 + 3u]);
    bool temp_var_logical_8 = true;
    if (control.z == 0u)
    {
        temp_var_logical_8 = control.y != 0u;
    }
    bool needsLiquid = temp_var_logical_8;
    bool temp_var_logical_9 = true;
    if (needsLiquid)
    {
        ReflectionLiquidFrame param_var_old = liquid;
        bool temp_var_logical_10 = false;
        if (reflection_liquid_frame_valid(param_var_old))
        {
            temp_var_logical_10 = all(receiver.projection == liquid.projection);
        }
        bool temp_var_logical_11 = false;
        if (temp_var_logical_10)
        {
            temp_var_logical_11 = all(receiver.extentClip == liquid.extentClip);
        }
        temp_var_logical_9 = temp_var_logical_11;
    }
    bool liquidValid = temp_var_logical_9;
    uint _2646 = (at + 432u) >> 2u;
    ReflectionSpecularPlane terminalPlane;
    terminalPlane.a = as_type<float4>(uint4(frames._m0[_2646], frames._m0[_2646 + 1u], frames._m0[_2646 + 2u], frames._m0[_2646 + 3u]));
    uint _2659 = (at + 448u) >> 2u;
    terminalPlane.b = as_type<float4>(uint4(frames._m0[_2659], frames._m0[_2659 + 1u], frames._m0[_2659 + 2u], frames._m0[_2659 + 3u]));
    uint _2672 = (at + 464u) >> 2u;
    terminalPlane.c = as_type<float4>(uint4(frames._m0[_2672], frames._m0[_2672 + 1u], frames._m0[_2672 + 2u], frames._m0[_2672 + 3u]));
    float3 param_var_f = terminal.xyz;
    uint param_var_kind = control.w;
    bool temp_var_logical_12 = true;
    if (feature_valid(param_var_f, param_var_kind))
    {
        bool temp_var_logical_13 = false;
        if (terminal.w != 0.0)
        {
            ReflectionSpecularPlane param_var_plane = terminalPlane;
            temp_var_logical_13 = !reflection_specular_plane_valid(param_var_plane);
        }
        temp_var_logical_12 = temp_var_logical_13;
    }
    if (temp_var_logical_12)
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint queryControl = queries._m0[((q * 48u) + 20u) >> 2u];
    uint queryPrimary = queries._m0[(q * 48u) >> 2u];
    uint queryLobe = queries._m0[((q * 48u) + 44u) >> 2u];
    bool temp_var_logical_14 = true;
    if (control.x <= 4u)
    {
        temp_var_logical_14 = (control.y >> (control.x & 31u)) != 0u;
    }
    bool temp_var_logical_15 = true;
    if (!temp_var_logical_14)
    {
        temp_var_logical_15 = control.z > 1u;
    }
    bool temp_var_logical_16 = true;
    if (!temp_var_logical_15)
    {
        uint temp_var_ternary;
        if (queryPrimary == 4294967293u)
        {
            temp_var_ternary = 1u;
        }
        else
        {
            temp_var_ternary = 0u;
        }
        temp_var_logical_16 = control.z != temp_var_ternary;
    }
    bool temp_var_logical_17 = true;
    if (!temp_var_logical_16)
    {
        bool4 _2741 = isnan(receiver.settings);
        bool4 _2742 = isinf(receiver.settings);
        temp_var_logical_17 = !all(not(bool4(_2741.x || _2742.x, _2741.y || _2742.y, _2741.z || _2742.z, _2741.w || _2742.w)));
    }
    bool temp_var_logical_18 = true;
    if (!temp_var_logical_17)
    {
        temp_var_logical_18 = receiver.settings.x < 0.0;
    }
    bool temp_var_logical_19 = true;
    if (!temp_var_logical_18)
    {
        temp_var_logical_19 = receiver.settings.x > 1.0;
    }
    bool temp_var_logical_20 = true;
    if (!temp_var_logical_19)
    {
        temp_var_logical_20 = receiver.settings.y != float(queryLobe);
    }
    bool temp_var_logical_21 = true;
    if (!temp_var_logical_20)
    {
        temp_var_logical_21 = queryLobe > 7u;
    }
    bool temp_var_logical_22 = true;
    if (!temp_var_logical_21)
    {
        temp_var_logical_22 = any(receiver.settings.zw != float2(0.0));
    }
    bool temp_var_logical_23 = true;
    if (!temp_var_logical_22)
    {
        temp_var_logical_23 = control.x != (queryControl & 7u);
    }
    bool temp_var_logical_24 = true;
    if (!temp_var_logical_23)
    {
        temp_var_logical_24 = control.y != ((queryControl >> 4u) & 15u);
    }
    bool temp_var_logical_25 = true;
    if (!temp_var_logical_24)
    {
        temp_var_logical_25 = control.w != (queryControl >> 8u);
    }
    bool temp_var_logical_26 = true;
    if (!temp_var_logical_25)
    {
        bool4 _2804 = isnan(receiver.projection);
        bool4 _2805 = isinf(receiver.projection);
        temp_var_logical_26 = !all(not(bool4(_2804.x || _2805.x, _2804.y || _2805.y, _2804.z || _2805.z, _2804.w || _2805.w)));
    }
    bool temp_var_logical_27 = true;
    if (!temp_var_logical_26)
    {
        temp_var_logical_27 = any(receiver.projection.xy <= float2(0.0));
    }
    bool temp_var_logical_28 = true;
    if (!temp_var_logical_27)
    {
        bool4 _2820 = isnan(terminal);
        bool4 _2821 = isinf(terminal);
        temp_var_logical_28 = !all(not(bool4(_2820.x || _2821.x, _2820.y || _2821.y, _2820.z || _2821.z, _2820.w || _2821.w)));
    }
    bool temp_var_logical_29 = true;
    if (!temp_var_logical_28)
    {
        int temp_var_ternary_1;
        if (control.w == 1u)
        {
            temp_var_ternary_1 = 1;
        }
        else
        {
            temp_var_ternary_1 = 0;
        }
        temp_var_logical_29 = terminal.w != float(temp_var_ternary_1);
    }
    bool temp_var_logical_30 = true;
    if (!temp_var_logical_29)
    {
        temp_var_logical_30 = !liquidValid;
    }
    bool temp_var_logical_31 = true;
    if (!temp_var_logical_30)
    {
        bool temp_var_logical_32 = false;
        if (control.z == 0u)
        {
            ReflectionRoughFrame param_var_old_1 = receiver;
            temp_var_logical_32 = !reflection_rough_frame_valid(param_var_old_1);
        }
        temp_var_logical_31 = temp_var_logical_32;
    }
    if (temp_var_logical_31)
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    for (uint h_1 = 0u; h_1 < control.x; h_1++)
    {
        bool temp_var_logical_33 = false;
        if ((control.y & (1u << (h_1 & 31u))) == 0u)
        {
            ReflectionSpecularPlane param_var_plane_1 = planes[h_1];
            temp_var_logical_33 = !reflection_specular_plane_valid(param_var_plane_1);
        }
        if (temp_var_logical_33)
        {
            if (lane == 0u)
            {
                results._m0[(_output + 4u) >> 2u] = 4u;
            }
            return;
        }
    }
    uint _2880 = (((_input + 16u) + (r * 48u)) + 16u) >> 2u;
    float4 initial = spvFAdd(as_type<float4>(uint4(regions._m0[_2880], regions._m0[_2880 + 1u], regions._m0[_2880 + 2u], regions._m0[_2880 + 3u])), float4(0.5));
    float param_var_x = initial.x;
    float _2893 = interval_down(param_var_x, intervalFailed);
    float param_var_x_1 = initial.y;
    float _2896 = interval_down(param_var_x_1, intervalFailed);
    float param_var_x_2 = initial.z;
    float _2899 = interval_up(param_var_x_2, intervalFailed);
    float param_var_x_3 = initial.w;
    float _2902 = interval_up(param_var_x_3, intervalFailed);
    float4 extent = float4(_2893, _2896, _2899, _2902);
    bool4 _2905 = isnan(extent);
    bool4 _2906 = isinf(extent);
    bool temp_var_logical_34 = true;
    if (all(not(bool4(_2905.x || _2906.x, _2905.y || _2906.y, _2905.z || _2906.z, _2905.w || _2906.w))))
    {
        temp_var_logical_34 = any(extent.xy > extent.zw);
    }
    if (temp_var_logical_34)
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint limit = 1u << (((OpticalSettings.opticalDepth + 1u) / 2u) & 31u);
    uint2 bins = max(uint2(1u), min(uint2(limit), uint2(ceil(spvFSub(extent.zw, extent.xy) * 64.0))));
    uint cells = bins.x * bins.y;
    if (cells > OpticalSettings.opticalBudget)
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 32u;
        }
        return;
    }
    if (lane == 0u)
    {
        excludedCount = 0u;
        keptCount = 0u;
        hullY0 = 1900671690u;
        hullX0 = 1900671690u;
        hullY1 = 0u;
        hullX1 = 0u;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    intervalFailed = false;
    uint param_var_at = at;
    float4 param_var_feature = terminal;
    Interval3 _2953 = native_target(param_var_at, param_var_feature, intervalFailed, optical_product_upper, interval_divide_upper, frames, TargetSettings);
    Interval3 enclosedTarget = _2953;
    bool targetFailed = intervalFailed;
    uint laneKept = 0u;
    uint laneExcluded = 0u;
    uint laneX0 = 1900671690u;
    uint laneY0 = 1900671690u;
    uint laneX1 = 0u;
    uint laneY1 = 0u;
    for (uint cell = lane; cell < cells; cell += 256u)
    {
        uint2 ij = uint2(cell % bins.x, cell / bins.x);
        intervalFailed = false;
        float param_var_lo = extent.x;
        float param_var_hi = extent.z;
        uint param_var_cell = ij.x;
        uint param_var_count = bins.x;
        float param_var_x_4 = split_axis(param_var_lo, param_var_hi, param_var_cell, param_var_count);
        float _2975 = interval_down(param_var_x_4, intervalFailed);
        float param_var_lo_1 = extent.y;
        float param_var_hi_1 = extent.w;
        uint param_var_cell_1 = ij.y;
        uint param_var_count_1 = bins.y;
        float param_var_x_5 = split_axis(param_var_lo_1, param_var_hi_1, param_var_cell_1, param_var_count_1);
        float _2985 = interval_down(param_var_x_5, intervalFailed);
        float param_var_lo_2 = extent.x;
        float param_var_hi_2 = extent.z;
        uint param_var_cell_2 = ij.x + 1u;
        uint param_var_count_2 = bins.x;
        float param_var_x_6 = split_axis(param_var_lo_2, param_var_hi_2, param_var_cell_2, param_var_count_2);
        float _2995 = interval_up(param_var_x_6, intervalFailed);
        float param_var_lo_3 = extent.y;
        float param_var_hi_3 = extent.w;
        uint param_var_cell_3 = ij.y + 1u;
        uint param_var_count_3 = bins.y;
        float param_var_x_7 = split_axis(param_var_lo_3, param_var_hi_3, param_var_cell_3, param_var_count_3);
        float _3005 = interval_up(param_var_x_7, intervalFailed);
        float4 box = float4(_2975, _2985, _2995, _3005);
        bool temp_var_logical_35 = false;
        if (!targetFailed)
        {
            float4 param_var_box = box;
            ReflectionRoughFrame param_var_receiver = receiver;
            ReflectionLiquidFrame param_var_liquid = liquid;
            spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
            uint4 param_var_control = control;
            Interval3 param_var_target = enclosedTarget;
            bool param_var_finiteTerminal = terminal.w != 0.0;
            bool _3018 = optical_excluded_target(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTerminal, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper);
            temp_var_logical_35 = _3018;
        }
        bool excluded = temp_var_logical_35;
        if (excluded)
        {
            laneExcluded++;
        }
        else
        {
            laneKept++;
            laneX0 = min(laneX0, as_type<uint>(box.x));
            laneY0 = min(laneY0, as_type<uint>(box.y));
            laneX1 = max(laneX1, as_type<uint>(box.z));
            laneY1 = max(laneY1, as_type<uint>(box.w));
        }
    }
    if (laneExcluded != 0u)
    {
        uint _3047 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&excludedCount, laneExcluded, memory_order_relaxed);
    }
    if (laneKept != 0u)
    {
        uint _3051 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&keptCount, laneKept, memory_order_relaxed);
        uint _3053 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullX0, laneX0, memory_order_relaxed);
        uint _3055 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullY0, laneY0, memory_order_relaxed);
        uint _3057 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullX1, laneX1, memory_order_relaxed);
        uint _3059 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullY1, laneY1, memory_order_relaxed);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (lane == 0u)
    {
        uint _3063 = _output >> 2u;
        results._m0[_3063] = keptCount;
        results._m0[_3063 + 1u] = 0u;
        results._m0[_3063 + 2u] = cells;
        results._m0[_3063 + 3u] = excludedCount;
        uint _3072 = (_output + 16u) >> 2u;
        uint4 temp_var_ternary_2;
        if (keptCount != 0u)
        {
            temp_var_ternary_2 = uint4(hullX0, hullY0, hullX1, hullY1);
        }
        else
        {
            temp_var_ternary_2 = uint4(0u);
        }
        results._m0[_3072] = temp_var_ternary_2.x;
        results._m0[_3072 + 1u] = temp_var_ternary_2.y;
        results._m0[_3072 + 2u] = temp_var_ternary_2.z;
        results._m0[_3072 + 3u] = temp_var_ternary_2.w;
    }
}

kernel void feature_optical_stream_main(constant type_Settings& Settings [[buffer(0)]], constant type_OpticalSettings& OpticalSettings [[buffer(1)]], device type_ByteAddressBuffer& regions [[buffer(2)]], device type_ByteAddressBuffer& frames [[buffer(3)]], device type_ByteAddressBuffer& queries [[buffer(4)]], device type_ByteAddressBuffer& workMap [[buffer(5)]], device type_RWByteAddressBuffer& results [[buffer(6)]], constant type_TargetSettings& TargetSettings [[buffer(7)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]])
{
    threadgroup uint keptCount;
    threadgroup uint excludedCount;
    threadgroup uint hullX0;
    threadgroup uint hullY0;
    threadgroup uint hullX1;
    threadgroup uint hullY1;
    bool intervalFailed = false;
    float optical_product_upper = 0.0;
    float interval_divide_upper = 0.0;
    float interval_sine_upper = 0.0;
    uint3 param_var_group = gl_WorkGroupID;
    uint param_var_lane = gl_LocalInvocationIndex;
    src_feature_optical_stream_main(param_var_group, param_var_lane, Settings, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, OpticalSettings, regions, frames, queries, workMap, results, TargetSettings, keptCount, excludedCount, hullX0, hullY0, hullX1, hullY1);
}

