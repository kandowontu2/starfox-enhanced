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

constant spvUnsafeArray<int2, 8> _2351 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2352 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float normalLength = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3203 = isnan(old.planePoint);
    bool4 _3204 = isinf(old.planePoint);
    bool temp_var_logical = true;
    if (all(not(bool4(_3203.x || _3204.x, _3203.y || _3204.y, _3203.z || _3204.z, _3203.w || _3204.w))))
    {
        bool4 _3210 = isnan(old.planeNormal);
        bool4 _3211 = isinf(old.planeNormal);
        temp_var_logical = !all(not(bool4(_3210.x || _3211.x, _3210.y || _3211.y, _3210.z || _3211.z, _3210.w || _3211.w)));
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        bool4 _3220 = isnan(old.rotation0);
        bool4 _3221 = isinf(old.rotation0);
        temp_var_logical_1 = !all(not(bool4(_3220.x || _3221.x, _3220.y || _3221.y, _3220.z || _3221.z, _3220.w || _3221.w)));
    }
    bool temp_var_logical_2 = true;
    if (!temp_var_logical_1)
    {
        bool4 _3230 = isnan(old.rotation1);
        bool4 _3231 = isinf(old.rotation1);
        temp_var_logical_2 = !all(not(bool4(_3230.x || _3231.x, _3230.y || _3231.y, _3230.z || _3231.z, _3230.w || _3231.w)));
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical_2)
    {
        bool4 _3240 = isnan(old.rotation2);
        bool4 _3241 = isinf(old.rotation2);
        temp_var_logical_3 = !all(not(bool4(_3240.x || _3241.x, _3240.y || _3241.y, _3240.z || _3241.z, _3240.w || _3241.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3250 = isnan(old.projection);
        bool4 _3251 = isinf(old.projection);
        temp_var_logical_4 = !all(not(bool4(_3250.x || _3251.x, _3250.y || _3251.y, _3250.z || _3251.z, _3250.w || _3251.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3260 = isnan(old.extentClip);
        bool4 _3261 = isinf(old.extentClip);
        temp_var_logical_5 = !all(not(bool4(_3260.x || _3261.x, _3260.y || _3261.y, _3260.z || _3261.z, _3260.w || _3261.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3270 = isnan(old.settings);
        bool4 _3271 = isinf(old.settings);
        temp_var_logical_6 = !all(not(bool4(_3270.x || _3271.x, _3270.y || _3271.y, _3270.z || _3271.z, _3270.w || _3271.w)));
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
    bool3 _3408 = isnan(f);
    bool3 _3409 = isinf(f);
    if (!all(not(bool3(_3408.x || _3409.x, _3408.y || _3409.y, _3408.z || _3409.z))))
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
    bool _3440 = false;
    if (param_var_p.a.w == 2.0)
    {
        _3440 = param_var_p.b.w == 2.0;
    }
    bool _3441 = false;
    if (_3440)
    {
        _3441 = param_var_p.c.w == 2.0;
    }
    bool _3442 = _3441;
    bool analytic = _3442;
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
        bool4 _3484 = isnan(plane.a);
        bool4 _3485 = isinf(plane.a);
        temp_var_logical_3 = !all(not(bool4(_3484.x || _3485.x, _3484.y || _3485.y, _3484.z || _3485.z, _3484.w || _3485.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3494 = isnan(plane.b);
        bool4 _3495 = isinf(plane.b);
        temp_var_logical_4 = !all(not(bool4(_3494.x || _3495.x, _3494.y || _3495.y, _3494.z || _3495.z, _3494.w || _3495.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3504 = isnan(plane.c);
        bool4 _3505 = isinf(plane.c);
        temp_var_logical_5 = !all(not(bool4(_3504.x || _3505.x, _3504.y || _3505.y, _3504.z || _3505.z, _3504.w || _3505.w)));
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
    bool3 _3563 = isnan(n);
    bool3 _3564 = isinf(n);
    bool temp_var_logical_10 = false;
    if (all(not(bool3(_3563.x || _3564.x, _3563.y || _3564.y, _3563.z || _3564.z))))
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
    bool _3577 = false;
    if (param_var_old.a.w == 2.0)
    {
        _3577 = param_var_old.b.w == 2.0;
    }
    bool _3578 = false;
    if (_3577)
    {
        _3578 = param_var_old.c.w == 2.0;
    }
    bool _3579 = _3578;
    bool analytic = _3579;
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
        bool4 _3621 = isnan(old.a);
        bool4 _3622 = isinf(old.a);
        temp_var_logical_3 = !all(not(bool4(_3621.x || _3622.x, _3621.y || _3622.y, _3621.z || _3622.z, _3621.w || _3622.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3631 = isnan(old.b);
        bool4 _3632 = isinf(old.b);
        temp_var_logical_4 = !all(not(bool4(_3631.x || _3632.x, _3631.y || _3632.y, _3631.z || _3632.z, _3631.w || _3632.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3641 = isnan(old.c);
        bool4 _3642 = isinf(old.c);
        temp_var_logical_5 = !all(not(bool4(_3641.x || _3642.x, _3641.y || _3642.y, _3641.z || _3642.z, _3641.w || _3642.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3651 = isnan(old.projection);
        bool4 _3652 = isinf(old.projection);
        temp_var_logical_6 = !all(not(bool4(_3651.x || _3652.x, _3651.y || _3652.y, _3651.z || _3652.z, _3651.w || _3652.w)));
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        bool4 _3661 = isnan(old.extentClip);
        bool4 _3662 = isinf(old.extentClip);
        temp_var_logical_7 = !all(not(bool4(_3661.x || _3662.x, _3661.y || _3662.y, _3661.z || _3662.z, _3661.w || _3662.w)));
    }
    bool temp_var_logical_8 = true;
    if (!temp_var_logical_7)
    {
        bool4 _3671 = isnan(old.settings);
        bool4 _3672 = isinf(old.settings);
        temp_var_logical_8 = !all(not(bool4(_3671.x || _3672.x, _3671.y || _3672.y, _3671.z || _3672.z, _3671.w || _3672.w)));
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
    bool3 _3820 = isnan(normal);
    bool3 _3821 = isinf(normal);
    bool temp_var_logical_26 = false;
    if (all(not(bool3(_3820.x || _3821.x, _3820.y || _3821.y, _3820.z || _3821.z))))
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

static __attribute__((noinline))
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
                    float _20116 = interval_up(param_var_x, intervalFailed);
                    temp_var_ternary_2 = _20116;
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
                float _20122 = interval_down(param_var_x_1, intervalFailed);
                temp_var_ternary_3 = _20122;
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
        float _20129 = interval_up(param_var_x_2, intervalFailed);
        temp_var_ternary_4 = _20129;
    }
    else
    {
        float param_var_x_3 = sum_1;
        float _20131 = interval_down(param_var_x_3, intervalFailed);
        temp_var_ternary_4 = _20131;
    }
    return temp_var_ternary_4;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    Interval param_var_a = a;
    bool _16261 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _16261 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _16262 = false;
    if (_16261)
    {
        _16262 = param_var_a.lo <= param_var_a.hi;
    }
    bool _16263 = _16262;
    bool temp_var_logical = false;
    if (_16263)
    {
        Interval param_var_a_1 = b;
        bool _16258 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _16258 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _16259 = false;
        if (_16258)
        {
            _16259 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _16260 = _16259;
        temp_var_logical = _16260;
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
    float _16325 = optical_add_bound(param_var_a_4, param_var_b, param_var_upper, intervalFailed);
    float param_var_lo = _16325;
    float param_var_a_5 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _16330 = optical_add_bound(param_var_a_5, param_var_b_1, param_var_upper_1, intervalFailed);
    float param_var_hi = _16330;
    Interval _16256;
    _16256.lo = param_var_lo;
    _16256.hi = param_var_hi;
    Interval _16257 = _16256;
    return _16257;
}

static inline __attribute__((always_inline))
uint interval_product_extrema(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi)
{
    uint al = as_type<uint>(alo) & 2147483647u;
    uint ah = as_type<uint>(ahi) & 2147483647u;
    uint bl = as_type<uint>(blo) & 2147483647u;
    uint bh = as_type<uint>(bhi) & 2147483647u;
    bool temp_var_logical = true;
    if (al >= 813694976u)
    {
        temp_var_logical = al > 1317011456u;
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        temp_var_logical_1 = ah < 813694976u;
    }
    bool temp_var_logical_2 = true;
    if (!temp_var_logical_1)
    {
        temp_var_logical_2 = ah > 1317011456u;
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical_2)
    {
        temp_var_logical_3 = bl < 813694976u;
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        temp_var_logical_4 = bl > 1317011456u;
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        temp_var_logical_5 = bh < 813694976u;
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        temp_var_logical_6 = bh > 1317011456u;
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        temp_var_logical_7 = alo > ahi;
    }
    bool temp_var_logical_8 = true;
    if (!temp_var_logical_7)
    {
        temp_var_logical_8 = blo > bhi;
    }
    if (temp_var_logical_8)
    {
        return 0u;
    }
    if (alo > 0.0)
    {
        if (blo > 0.0)
        {
            return 28u;
        }
        if (bhi < 0.0)
        {
            return 22u;
        }
        return 30u;
    }
    if (ahi < 0.0)
    {
        if (blo > 0.0)
        {
            return 25u;
        }
        if (bhi < 0.0)
        {
            return 19u;
        }
        return 17u;
    }
    if (blo > 0.0)
    {
        return 29u;
    }
    if (bhi < 0.0)
    {
        return 18u;
    }
    return 0u;
}

static __attribute__((noinline))
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
                float _20308 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _20308;
                float param_var_x_1 = product;
                float _20310 = interval_down(param_var_x_1, intervalFailed);
                return _20310;
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
                float _20356 = interval_up(param_var_x_2, intervalFailed);
                temp_var_ternary_1 = _20356;
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
                float _20364 = interval_down(param_var_x_3, intervalFailed);
                temp_var_ternary_3 = _20364;
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
    float _20370 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _20370;
    float param_var_x_5 = product_1;
    float _20372 = interval_down(param_var_x_5, intervalFailed);
    return _20372;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval param_var_a = a;
    bool _18213 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _18213 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _18214 = false;
    if (_18213)
    {
        _18214 = param_var_a.lo <= param_var_a.hi;
    }
    bool _18215 = _18214;
    bool temp_var_logical = false;
    if (_18215)
    {
        Interval param_var_a_1 = b;
        bool _18210 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _18210 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _18211 = false;
        if (_18210)
        {
            _18211 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _18212 = _18211;
        temp_var_logical = _18212;
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
            float _18207 = param_var_x_2;
            float _18208 = param_var_x_2;
            Interval _18205;
            _18205.lo = _18207;
            _18205.hi = _18208;
            Interval _18206 = _18205;
            Interval _18209 = _18206;
            return _18209;
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
            float _18202 = as_type<float>(as_type<uint>(param_var_a_7.hi) ^ 2147483648u);
            float _18203 = as_type<float>(as_type<uint>(param_var_a_7.lo) ^ 2147483648u);
            Interval _18200;
            _18200.lo = _18202;
            _18200.hi = _18203;
            Interval _18201 = _18200;
            Interval _18204 = _18201;
            return _18204;
        }
        Interval param_var_a_8 = b;
        float param_var_x_6 = -1.0;
        if (interval_exact_point(param_var_a_8, param_var_x_6))
        {
            Interval param_var_a_9 = a;
            float _18197 = as_type<float>(as_type<uint>(param_var_a_9.hi) ^ 2147483648u);
            float _18198 = as_type<float>(as_type<uint>(param_var_a_9.lo) ^ 2147483648u);
            Interval _18195;
            _18195.lo = _18197;
            _18195.hi = _18198;
            Interval _18196 = _18195;
            Interval _18199 = _18196;
            return _18199;
        }
    }
    Interval param_var_a_10 = a;
    bool _18192 = false;
    if (!(isnan(param_var_a_10.lo) || isinf(param_var_a_10.lo)))
    {
        _18192 = !(isnan(param_var_a_10.hi) || isinf(param_var_a_10.hi));
    }
    bool _18193 = false;
    if (_18192)
    {
        _18193 = param_var_a_10.lo <= param_var_a_10.hi;
    }
    bool _18194 = _18193;
    bool temp_var_logical_2 = false;
    if (_18194)
    {
        Interval param_var_a_11 = b;
        bool _18189 = false;
        if (!(isnan(param_var_a_11.lo) || isinf(param_var_a_11.lo)))
        {
            _18189 = !(isnan(param_var_a_11.hi) || isinf(param_var_a_11.hi));
        }
        bool _18190 = false;
        if (_18189)
        {
            _18190 = param_var_a_11.lo <= param_var_a_11.hi;
        }
        bool _18191 = _18190;
        temp_var_logical_2 = _18191;
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
    bool temp_var_logical_5 = false;
    if (finiteCorners)
    {
        temp_var_logical_5 = !sameA;
    }
    bool temp_var_logical_6 = false;
    if (temp_var_logical_5)
    {
        temp_var_logical_6 = !sameB;
    }
    uint temp_var_ternary;
    if (temp_var_logical_6)
    {
        float param_var_alo = a.lo;
        float param_var_ahi = a.hi;
        float param_var_blo = b.lo;
        float param_var_bhi = b.hi;
        temp_var_ternary = interval_product_extrema(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi);
    }
    else
    {
        temp_var_ternary = 0u;
    }
    uint extrema = temp_var_ternary;
    if (extrema != 0u)
    {
        uint lower = extrema & 3u;
        uint upper = (extrema >> 2u) & 3u;
        float temp_var_ternary_1;
        if ((lower & 2u) != 0u)
        {
            temp_var_ternary_1 = a.hi;
        }
        else
        {
            temp_var_ternary_1 = a.lo;
        }
        float param_var_a_12 = temp_var_ternary_1;
        float temp_var_ternary_2;
        if ((lower & 1u) != 0u)
        {
            temp_var_ternary_2 = b.hi;
        }
        else
        {
            temp_var_ternary_2 = b.lo;
        }
        float param_var_b = temp_var_ternary_2;
        float _18437 = optical_product_bounds(param_var_a_12, param_var_b, intervalFailed, optical_product_upper);
        float resultLo = _18437;
        float temp_var_ternary_3;
        if ((upper & 2u) != 0u)
        {
            temp_var_ternary_3 = a.hi;
        }
        else
        {
            temp_var_ternary_3 = a.lo;
        }
        float param_var_a_13 = temp_var_ternary_3;
        float temp_var_ternary_4;
        if ((upper & 1u) != 0u)
        {
            temp_var_ternary_4 = b.hi;
        }
        else
        {
            temp_var_ternary_4 = b.lo;
        }
        float param_var_b_1 = temp_var_ternary_4;
        __attribute__((unused)) float _18454 = optical_product_bounds(param_var_a_13, param_var_b_1, intervalFailed, optical_product_upper);
        float param_var_lo = resultLo;
        float param_var_hi = optical_product_upper;
        Interval _18187;
        _18187.lo = param_var_lo;
        _18187.hi = param_var_hi;
        Interval _18188 = _18187;
        return _18188;
    }
    float param_var_a_14 = a.lo;
    float param_var_b_2 = b.lo;
    float _18467 = optical_product_bounds(param_var_a_14, param_var_b_2, intervalFailed, optical_product_upper);
    float p = _18467;
    float u = optical_product_upper;
    float q = p;
    float v = u;
    if (!sameB)
    {
        float param_var_a_15 = a.lo;
        float param_var_b_3 = b.hi;
        float _18477 = optical_product_bounds(param_var_a_15, param_var_b_3, intervalFailed, optical_product_upper);
        q = _18477;
        v = optical_product_upper;
    }
    float r = p;
    float w = u;
    if (!sameA)
    {
        float param_var_a_16 = a.hi;
        float param_var_b_4 = b.lo;
        float _18487 = optical_product_bounds(param_var_a_16, param_var_b_4, intervalFailed, optical_product_upper);
        r = _18487;
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
            float param_var_a_17 = a.hi;
            float param_var_b_5 = b.hi;
            float _18500 = optical_product_bounds(param_var_a_17, param_var_b_5, intervalFailed, optical_product_upper);
            s = _18500;
            x = optical_product_upper;
        }
    }
    float param_var_lo_1 = precise::min(precise::min(p, q), precise::min(r, s));
    float param_var_hi_1 = precise::max(precise::max(u, v), precise::max(w, x));
    Interval _18185;
    _18185.lo = param_var_lo_1;
    _18185.hi = param_var_hi_1;
    Interval _18186 = _18185;
    return _18186;
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float q = precise::sqrt(a);
    bool temp_var_ternary;
    float temp_var_ternary_1;
    CurvedScalar _20407;
    for (uint n = 0u; n < 8u; n++)
    {
        float param_var_ax = q;
        float param_var_ay = 0.0;
        float param_var_bx = q;
        float param_var_by = 0.0;
        float _20409 = spvFMul(param_var_ax, param_var_bx);
        float _20410 = spvFMul(param_var_ax, 4097.0);
        float _20411 = spvFSub(_20410, spvFSub(_20410, param_var_ax));
        float _20412 = spvFSub(param_var_ax, _20411);
        float _20413 = spvFMul(param_var_bx, 4097.0);
        float _20414 = spvFSub(_20413, spvFSub(_20413, param_var_bx));
        float _20415 = spvFSub(param_var_bx, _20414);
        float _20416 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_20411, _20414), _20409), spvFMul(_20411, _20415)), spvFMul(_20412, _20414)), spvFMul(_20412, _20415)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _20417 = spvFAdd(_20409, _20416);
        float _20418 = spvFSub(_20416, spvFSub(_20417, _20409));
        float _20419 = _20417;
        float _20420 = _20418;
        _20407.high = _20419;
        _20407.low = _20420;
        CurvedScalar _20408 = _20407;
        CurvedScalar _20421 = _20408;
        CurvedScalar square = _20421;
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
            float _20519 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_1 = _20519;
        }
        else
        {
            float param_var_x_1 = q;
            float _20521 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_1 = precise::max(0.0, _20521);
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
        Interval _20375;
        _20375.lo = param_var_lo;
        _20375.hi = param_var_hi;
        Interval _20376 = _20375;
        return _20376;
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _20393 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _20393;
    float _20394 = interval_down(param_var_x, intervalFailed);
    float param_var_lo_1 = precise::max(0.0, _20394);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _20399 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _20399;
    float _20400 = interval_up(param_var_x_1, intervalFailed);
    float param_var_hi_1 = _20400;
    Interval _20373;
    _20373.lo = param_var_lo_1;
    _20373.hi = param_var_hi_1;
    Interval _20374 = _20373;
    return _20374;
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float q = a / b;
    bool temp_var_ternary;
    bool temp_var_ternary_1;
    float temp_var_ternary_2;
    CurvedScalar _19900;
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
        float _19902 = spvFMul(param_var_ax, param_var_bx);
        float _19903 = spvFMul(param_var_ax, 4097.0);
        float _19904 = spvFSub(_19903, spvFSub(_19903, param_var_ax));
        float _19905 = spvFSub(param_var_ax, _19904);
        float _19906 = spvFMul(param_var_bx, 4097.0);
        float _19907 = spvFSub(_19906, spvFSub(_19906, param_var_bx));
        float _19908 = spvFSub(param_var_bx, _19907);
        float _19909 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_19904, _19907), _19902), spvFMul(_19904, _19908)), spvFMul(_19905, _19907)), spvFMul(_19905, _19908)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _19910 = spvFAdd(_19902, _19909);
        float _19911 = spvFSub(_19909, spvFSub(_19910, _19902));
        float _19912 = _19910;
        float _19913 = _19911;
        _19900.high = _19912;
        _19900.low = _19913;
        CurvedScalar _19901 = _19900;
        CurvedScalar _19914 = _19901;
        CurvedScalar product = _19914;
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
            float _20031 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_2 = _20031;
        }
        else
        {
            float param_var_x_1 = q;
            float _20033 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_2 = _20033;
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
    bool _16413 = false;
    if (param_var_a.lo <= 0.0)
    {
        _16413 = param_var_a.hi >= 0.0;
    }
    bool _16414 = _16413;
    bool temp_var_logical = true;
    if (!_16414)
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
        Interval _16411;
        _16411.lo = param_var_lo;
        _16411.hi = param_var_hi;
        Interval _16412 = _16411;
        return _16412;
    }
    Interval param_var_a_1 = a;
    bool _16408 = false;
    if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
    {
        _16408 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
    }
    bool _16409 = false;
    if (_16408)
    {
        _16409 = param_var_a_1.lo <= param_var_a_1.hi;
    }
    bool _16410 = _16409;
    bool temp_var_logical_2 = false;
    if (_16410)
    {
        Interval param_var_a_2 = b;
        bool _16405 = false;
        if (!(isnan(param_var_a_2.lo) || isinf(param_var_a_2.lo)))
        {
            _16405 = !(isnan(param_var_a_2.hi) || isinf(param_var_a_2.hi));
        }
        bool _16406 = false;
        if (_16405)
        {
            _16406 = param_var_a_2.lo <= param_var_a_2.hi;
        }
        bool _16407 = _16406;
        temp_var_logical_2 = _16407;
    }
    if (temp_var_logical_2)
    {
        Interval param_var_a_3 = a;
        float param_var_x = 0.0;
        if (interval_exact_point(param_var_a_3, param_var_x))
        {
            float param_var_x_1 = 0.0;
            float _16402 = param_var_x_1;
            float _16403 = param_var_x_1;
            Interval _16400;
            _16400.lo = _16402;
            _16400.hi = _16403;
            Interval _16401 = _16400;
            Interval _16404 = _16401;
            return _16404;
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
            float _16397 = as_type<float>(as_type<uint>(param_var_a_6.hi) ^ 2147483648u);
            float _16398 = as_type<float>(as_type<uint>(param_var_a_6.lo) ^ 2147483648u);
            Interval _16395;
            _16395.lo = _16397;
            _16395.hi = _16398;
            Interval _16396 = _16395;
            Interval _16399 = _16396;
            return _16399;
        }
    }
    float param_var_alo = a.lo;
    float param_var_ahi = a.hi;
    float param_var_blo = b.lo;
    float param_var_bhi = b.hi;
    float _16350 = param_var_alo;
    float _16351 = param_var_ahi;
    Interval _16347;
    _16347.lo = _16350;
    _16347.hi = _16351;
    Interval _16348 = _16347;
    Interval _16352 = _16348;
    bool _16344 = false;
    if (!(isnan(_16352.lo) || isinf(_16352.lo)))
    {
        _16344 = !(isnan(_16352.hi) || isinf(_16352.hi));
    }
    bool _16345 = false;
    if (_16344)
    {
        _16345 = _16352.lo <= _16352.hi;
    }
    bool _16346 = _16345;
    bool _16353 = false;
    if (_16346)
    {
        float _16354 = param_var_blo;
        float _16355 = param_var_bhi;
        Interval _16342;
        _16342.lo = _16354;
        _16342.hi = _16355;
        Interval _16343 = _16342;
        Interval _16356 = _16343;
        bool _16339 = false;
        if (!(isnan(_16356.lo) || isinf(_16356.lo)))
        {
            _16339 = !(isnan(_16356.hi) || isinf(_16356.hi));
        }
        bool _16340 = false;
        if (_16339)
        {
            _16340 = _16356.lo <= _16356.hi;
        }
        bool _16341 = _16340;
        _16353 = _16341;
    }
    bool _16349 = _16353;
    bool _16358 = false;
    if (_16349)
    {
        _16358 = as_type<uint>(param_var_alo) == as_type<uint>(param_var_ahi);
    }
    bool _16357 = _16358;
    bool _16360 = false;
    if (_16349)
    {
        _16360 = as_type<uint>(param_var_blo) == as_type<uint>(param_var_bhi);
    }
    bool _16359 = _16360;
    float _16362 = param_var_alo;
    float _16363 = param_var_blo;
    bool _16364 = false;
    float _16632 = quotient_bound(_16362, _16363, _16364, intervalFailed);
    float _16361 = _16632;
    float _16366 = param_var_alo;
    float _16367 = param_var_blo;
    bool _16368 = true;
    float _16635 = quotient_bound(_16366, _16367, _16368, intervalFailed);
    float _16365 = _16635;
    float _16369 = _16361;
    float _16370 = _16365;
    if (!_16359)
    {
        float _16371 = param_var_alo;
        float _16372 = param_var_bhi;
        bool _16373 = false;
        float _16644 = quotient_bound(_16371, _16372, _16373, intervalFailed);
        _16369 = _16644;
        float _16374 = param_var_alo;
        float _16375 = param_var_bhi;
        bool _16376 = true;
        float _16647 = quotient_bound(_16374, _16375, _16376, intervalFailed);
        _16370 = _16647;
    }
    float _16377 = _16361;
    float _16378 = _16365;
    if (!_16357)
    {
        float _16379 = param_var_ahi;
        float _16380 = param_var_blo;
        bool _16381 = false;
        float _16656 = quotient_bound(_16379, _16380, _16381, intervalFailed);
        _16377 = _16656;
        float _16382 = param_var_ahi;
        float _16383 = param_var_blo;
        bool _16384 = true;
        float _16659 = quotient_bound(_16382, _16383, _16384, intervalFailed);
        _16378 = _16659;
    }
    float _16385 = _16369;
    float _16386 = _16370;
    if (!_16357)
    {
        if (_16359)
        {
            _16385 = _16377;
            _16386 = _16378;
        }
        else
        {
            float _16387 = param_var_ahi;
            float _16388 = param_var_bhi;
            bool _16389 = false;
            float _16674 = quotient_bound(_16387, _16388, _16389, intervalFailed);
            _16385 = _16674;
            float _16390 = param_var_ahi;
            float _16391 = param_var_bhi;
            bool _16392 = true;
            float _16677 = quotient_bound(_16390, _16391, _16392, intervalFailed);
            _16386 = _16677;
        }
    }
    float _16393 = precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16361), _16369), _16377), _16385);
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16365), _16370), _16378), _16386);
    float _16394 = _16393;
    float lo = _16394;
    float param_var_x_4 = lo;
    float _16697 = interval_down(param_var_x_4, intervalFailed);
    float param_var_lo_1 = _16697;
    float param_var_x_5 = interval_divide_upper;
    float _16699 = interval_up(param_var_x_5, intervalFailed);
    float param_var_hi_1 = _16699;
    Interval _16337;
    _16337.lo = param_var_lo_1;
    _16337.hi = param_var_hi_1;
    Interval _16338 = _16337;
    return _16338;
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, device type_ByteAddressBuffer& frames, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        float param_var_x = feature.x;
        float _4319 = param_var_x;
        float _4320 = param_var_x;
        Interval _4317;
        _4317.lo = _4319;
        _4317.hi = _4320;
        Interval _4318 = _4317;
        Interval _4321 = _4318;
        Interval x = _4321;
        float param_var_x_1 = feature.y;
        float _4314 = param_var_x_1;
        float _4315 = param_var_x_1;
        Interval _4312;
        _4312.lo = _4314;
        _4312.hi = _4315;
        Interval _4313 = _4312;
        Interval _4316 = _4313;
        Interval y = _4316;
        float param_var_x_2 = 1.0;
        float _4309 = param_var_x_2;
        float _4310 = param_var_x_2;
        Interval _4307;
        _4307.lo = _4309;
        _4307.hi = _4310;
        Interval _4308 = _4307;
        Interval _4311 = _4308;
        Interval param_var_a = _4311;
        Interval param_var_a_1 = x;
        Interval param_var_b = y;
        Interval _4358 = iadd(param_var_a_1, param_var_b, intervalFailed);
        Interval param_var_b_1 = _4358;
        Interval _4303 = param_var_a;
        Interval _4304 = param_var_b_1;
        float _4300 = as_type<float>(as_type<uint>(_4304.hi) ^ 2147483648u);
        float _4301 = as_type<float>(as_type<uint>(_4304.lo) ^ 2147483648u);
        Interval _4298;
        _4298.lo = _4300;
        _4298.hi = _4301;
        Interval _4299 = _4298;
        Interval _4302 = _4299;
        Interval _4305 = _4302;
        Interval _4378 = iadd(_4303, _4305, intervalFailed);
        Interval _4306 = _4378;
        Interval a = _4306;
        uint _4381 = (at + 432u) >> 2u;
        float3 param_var_v = as_type<float4>(uint4(frames._m0[_4381], frames._m0[_4381 + 1u], frames._m0[_4381 + 2u], frames._m0[_4381 + 3u])).xyz;
        float _4291 = param_var_v.x;
        float _4288 = _4291;
        float _4289 = _4291;
        Interval _4286;
        _4286.lo = _4288;
        _4286.hi = _4289;
        Interval _4287 = _4286;
        Interval _4290 = _4287;
        Interval _4292 = _4290;
        float _4293 = param_var_v.y;
        float _4283 = _4293;
        float _4284 = _4293;
        Interval _4281;
        _4281.lo = _4283;
        _4281.hi = _4284;
        Interval _4282 = _4281;
        Interval _4285 = _4282;
        Interval _4294 = _4285;
        float _4295 = param_var_v.z;
        float _4278 = _4295;
        float _4279 = _4295;
        Interval _4276;
        _4276.lo = _4278;
        _4276.hi = _4279;
        Interval _4277 = _4276;
        Interval _4280 = _4277;
        Interval _4296 = _4280;
        Interval3 _4274;
        _4274.x = _4292;
        _4274.y = _4294;
        _4274.z = _4296;
        Interval3 _4275 = _4274;
        Interval3 _4297 = _4275;
        Interval3 param_var_a_2 = _4297;
        Interval param_var_b_2 = a;
        Interval _4264 = param_var_a_2.x;
        Interval _4265 = param_var_b_2;
        Interval _4439 = imul(_4264, _4265, intervalFailed, optical_product_upper);
        Interval _4266 = _4439;
        Interval _4267 = param_var_a_2.y;
        Interval _4268 = param_var_b_2;
        Interval _4443 = imul(_4267, _4268, intervalFailed, optical_product_upper);
        Interval _4269 = _4443;
        Interval _4270 = param_var_a_2.z;
        Interval _4271 = param_var_b_2;
        Interval _4447 = imul(_4270, _4271, intervalFailed, optical_product_upper);
        Interval _4272 = _4447;
        Interval3 _4262;
        _4262.x = _4266;
        _4262.y = _4269;
        _4262.z = _4272;
        Interval3 _4263 = _4262;
        Interval3 _4273 = _4263;
        Interval3 param_var_a_3 = _4273;
        uint _4458 = (at + 448u) >> 2u;
        float3 param_var_v_1 = as_type<float4>(uint4(frames._m0[_4458], frames._m0[_4458 + 1u], frames._m0[_4458 + 2u], frames._m0[_4458 + 3u])).xyz;
        float _4255 = param_var_v_1.x;
        float _4252 = _4255;
        float _4253 = _4255;
        Interval _4250;
        _4250.lo = _4252;
        _4250.hi = _4253;
        Interval _4251 = _4250;
        Interval _4254 = _4251;
        Interval _4256 = _4254;
        float _4257 = param_var_v_1.y;
        float _4247 = _4257;
        float _4248 = _4257;
        Interval _4245;
        _4245.lo = _4247;
        _4245.hi = _4248;
        Interval _4246 = _4245;
        Interval _4249 = _4246;
        Interval _4258 = _4249;
        float _4259 = param_var_v_1.z;
        float _4242 = _4259;
        float _4243 = _4259;
        Interval _4240;
        _4240.lo = _4242;
        _4240.hi = _4243;
        Interval _4241 = _4240;
        Interval _4244 = _4241;
        Interval _4260 = _4244;
        Interval3 _4238;
        _4238.x = _4256;
        _4238.y = _4258;
        _4238.z = _4260;
        Interval3 _4239 = _4238;
        Interval3 _4261 = _4239;
        Interval3 param_var_a_4 = _4261;
        Interval param_var_b_3 = x;
        Interval _4228 = param_var_a_4.x;
        Interval _4229 = param_var_b_3;
        Interval _4516 = imul(_4228, _4229, intervalFailed, optical_product_upper);
        Interval _4230 = _4516;
        Interval _4231 = param_var_a_4.y;
        Interval _4232 = param_var_b_3;
        Interval _4520 = imul(_4231, _4232, intervalFailed, optical_product_upper);
        Interval _4233 = _4520;
        Interval _4234 = param_var_a_4.z;
        Interval _4235 = param_var_b_3;
        Interval _4524 = imul(_4234, _4235, intervalFailed, optical_product_upper);
        Interval _4236 = _4524;
        Interval3 _4226;
        _4226.x = _4230;
        _4226.y = _4233;
        _4226.z = _4236;
        Interval3 _4227 = _4226;
        Interval3 _4237 = _4227;
        Interval3 param_var_b_4 = _4237;
        Interval _4216 = param_var_a_3.x;
        Interval _4217 = param_var_b_4.x;
        Interval _4538 = iadd(_4216, _4217, intervalFailed);
        Interval _4218 = _4538;
        Interval _4219 = param_var_a_3.y;
        Interval _4220 = param_var_b_4.y;
        Interval _4543 = iadd(_4219, _4220, intervalFailed);
        Interval _4221 = _4543;
        Interval _4222 = param_var_a_3.z;
        Interval _4223 = param_var_b_4.z;
        Interval _4548 = iadd(_4222, _4223, intervalFailed);
        Interval _4224 = _4548;
        Interval3 _4214;
        _4214.x = _4218;
        _4214.y = _4221;
        _4214.z = _4224;
        Interval3 _4215 = _4214;
        Interval3 _4225 = _4215;
        Interval3 param_var_a_5 = _4225;
        uint _4559 = (at + 464u) >> 2u;
        float3 param_var_v_2 = as_type<float4>(uint4(frames._m0[_4559], frames._m0[_4559 + 1u], frames._m0[_4559 + 2u], frames._m0[_4559 + 3u])).xyz;
        float _4207 = param_var_v_2.x;
        float _4204 = _4207;
        float _4205 = _4207;
        Interval _4202;
        _4202.lo = _4204;
        _4202.hi = _4205;
        Interval _4203 = _4202;
        Interval _4206 = _4203;
        Interval _4208 = _4206;
        float _4209 = param_var_v_2.y;
        float _4199 = _4209;
        float _4200 = _4209;
        Interval _4197;
        _4197.lo = _4199;
        _4197.hi = _4200;
        Interval _4198 = _4197;
        Interval _4201 = _4198;
        Interval _4210 = _4201;
        float _4211 = param_var_v_2.z;
        float _4194 = _4211;
        float _4195 = _4211;
        Interval _4192;
        _4192.lo = _4194;
        _4192.hi = _4195;
        Interval _4193 = _4192;
        Interval _4196 = _4193;
        Interval _4212 = _4196;
        Interval3 _4190;
        _4190.x = _4208;
        _4190.y = _4210;
        _4190.z = _4212;
        Interval3 _4191 = _4190;
        Interval3 _4213 = _4191;
        Interval3 param_var_a_6 = _4213;
        Interval param_var_b_5 = y;
        Interval _4180 = param_var_a_6.x;
        Interval _4181 = param_var_b_5;
        Interval _4617 = imul(_4180, _4181, intervalFailed, optical_product_upper);
        Interval _4182 = _4617;
        Interval _4183 = param_var_a_6.y;
        Interval _4184 = param_var_b_5;
        Interval _4621 = imul(_4183, _4184, intervalFailed, optical_product_upper);
        Interval _4185 = _4621;
        Interval _4186 = param_var_a_6.z;
        Interval _4187 = param_var_b_5;
        Interval _4625 = imul(_4186, _4187, intervalFailed, optical_product_upper);
        Interval _4188 = _4625;
        Interval3 _4178;
        _4178.x = _4182;
        _4178.y = _4185;
        _4178.z = _4188;
        Interval3 _4179 = _4178;
        Interval3 _4189 = _4179;
        Interval3 param_var_b_6 = _4189;
        Interval _4168 = param_var_a_5.x;
        Interval _4169 = param_var_b_6.x;
        Interval _4639 = iadd(_4168, _4169, intervalFailed);
        Interval _4170 = _4639;
        Interval _4171 = param_var_a_5.y;
        Interval _4172 = param_var_b_6.y;
        Interval _4644 = iadd(_4171, _4172, intervalFailed);
        Interval _4173 = _4644;
        Interval _4174 = param_var_a_5.z;
        Interval _4175 = param_var_b_6.z;
        Interval _4649 = iadd(_4174, _4175, intervalFailed);
        Interval _4176 = _4649;
        Interval3 _4166;
        _4166.x = _4170;
        _4166.y = _4173;
        _4166.z = _4176;
        Interval3 _4167 = _4166;
        Interval3 _4177 = _4167;
        return _4177;
    }
    float3 param_var_v_3 = feature.xyz;
    float _4159 = param_var_v_3.x;
    float _4156 = _4159;
    float _4157 = _4159;
    Interval _4154;
    _4154.lo = _4156;
    _4154.hi = _4157;
    Interval _4155 = _4154;
    Interval _4158 = _4155;
    Interval _4160 = _4158;
    float _4161 = param_var_v_3.y;
    float _4151 = _4161;
    float _4152 = _4161;
    Interval _4149;
    _4149.lo = _4151;
    _4149.hi = _4152;
    Interval _4150 = _4149;
    Interval _4153 = _4150;
    Interval _4162 = _4153;
    float _4163 = param_var_v_3.z;
    float _4146 = _4163;
    float _4147 = _4163;
    Interval _4144;
    _4144.lo = _4146;
    _4144.hi = _4147;
    Interval _4145 = _4144;
    Interval _4148 = _4145;
    Interval _4164 = _4148;
    Interval3 _4142;
    _4142.x = _4160;
    _4142.y = _4162;
    _4142.z = _4164;
    Interval3 _4143 = _4142;
    Interval3 _4165 = _4143;
    Interval3 raw = _4165;
    float3 param_var_v_4 = TargetSettings.targetCurrentCube[0].xyz;
    float _4135 = param_var_v_4.x;
    float _4132 = _4135;
    float _4133 = _4135;
    Interval _4130;
    _4130.lo = _4132;
    _4130.hi = _4133;
    Interval _4131 = _4130;
    Interval _4134 = _4131;
    Interval _4136 = _4134;
    float _4137 = param_var_v_4.y;
    float _4127 = _4137;
    float _4128 = _4137;
    Interval _4125;
    _4125.lo = _4127;
    _4125.hi = _4128;
    Interval _4126 = _4125;
    Interval _4129 = _4126;
    Interval _4138 = _4129;
    float _4139 = param_var_v_4.z;
    float _4122 = _4139;
    float _4123 = _4139;
    Interval _4120;
    _4120.lo = _4122;
    _4120.hi = _4123;
    Interval _4121 = _4120;
    Interval _4124 = _4121;
    Interval _4140 = _4124;
    Interval3 _4118;
    _4118.x = _4136;
    _4118.y = _4138;
    _4118.z = _4140;
    Interval3 _4119 = _4118;
    Interval3 _4141 = _4119;
    Interval3 param_var_a_7 = _4141;
    Interval3 param_var_b_7 = raw;
    Interval _4107 = param_var_a_7.x;
    Interval _4108 = param_var_b_7.x;
    Interval _4754 = imul(_4107, _4108, intervalFailed, optical_product_upper);
    Interval _4109 = _4754;
    Interval _4110 = param_var_a_7.y;
    Interval _4111 = param_var_b_7.y;
    Interval _4759 = imul(_4110, _4111, intervalFailed, optical_product_upper);
    Interval _4112 = _4759;
    Interval _4760 = iadd(_4109, _4112, intervalFailed);
    Interval _4113 = _4760;
    Interval _4114 = param_var_a_7.z;
    Interval _4115 = param_var_b_7.z;
    Interval _4765 = imul(_4114, _4115, intervalFailed, optical_product_upper);
    Interval _4116 = _4765;
    Interval _4766 = iadd(_4113, _4116, intervalFailed);
    Interval _4117 = _4766;
    Interval param_var_x_3 = _4117;
    float3 param_var_v_5 = TargetSettings.targetCurrentCube[1].xyz;
    float _4100 = param_var_v_5.x;
    float _4097 = _4100;
    float _4098 = _4100;
    Interval _4095;
    _4095.lo = _4097;
    _4095.hi = _4098;
    Interval _4096 = _4095;
    Interval _4099 = _4096;
    Interval _4101 = _4099;
    float _4102 = param_var_v_5.y;
    float _4092 = _4102;
    float _4093 = _4102;
    Interval _4090;
    _4090.lo = _4092;
    _4090.hi = _4093;
    Interval _4091 = _4090;
    Interval _4094 = _4091;
    Interval _4103 = _4094;
    float _4104 = param_var_v_5.z;
    float _4087 = _4104;
    float _4088 = _4104;
    Interval _4085;
    _4085.lo = _4087;
    _4085.hi = _4088;
    Interval _4086 = _4085;
    Interval _4089 = _4086;
    Interval _4105 = _4089;
    Interval3 _4083;
    _4083.x = _4101;
    _4083.y = _4103;
    _4083.z = _4105;
    Interval3 _4084 = _4083;
    Interval3 _4106 = _4084;
    Interval3 param_var_a_8 = _4106;
    Interval3 param_var_b_8 = raw;
    Interval _4072 = param_var_a_8.x;
    Interval _4073 = param_var_b_8.x;
    Interval _4819 = imul(_4072, _4073, intervalFailed, optical_product_upper);
    Interval _4074 = _4819;
    Interval _4075 = param_var_a_8.y;
    Interval _4076 = param_var_b_8.y;
    Interval _4824 = imul(_4075, _4076, intervalFailed, optical_product_upper);
    Interval _4077 = _4824;
    Interval _4825 = iadd(_4074, _4077, intervalFailed);
    Interval _4078 = _4825;
    Interval _4079 = param_var_a_8.z;
    Interval _4080 = param_var_b_8.z;
    Interval _4830 = imul(_4079, _4080, intervalFailed, optical_product_upper);
    Interval _4081 = _4830;
    Interval _4831 = iadd(_4078, _4081, intervalFailed);
    Interval _4082 = _4831;
    Interval param_var_y = _4082;
    float3 param_var_v_6 = TargetSettings.targetCurrentCube[2].xyz;
    float _4065 = param_var_v_6.x;
    float _4062 = _4065;
    float _4063 = _4065;
    Interval _4060;
    _4060.lo = _4062;
    _4060.hi = _4063;
    Interval _4061 = _4060;
    Interval _4064 = _4061;
    Interval _4066 = _4064;
    float _4067 = param_var_v_6.y;
    float _4057 = _4067;
    float _4058 = _4067;
    Interval _4055;
    _4055.lo = _4057;
    _4055.hi = _4058;
    Interval _4056 = _4055;
    Interval _4059 = _4056;
    Interval _4068 = _4059;
    float _4069 = param_var_v_6.z;
    float _4052 = _4069;
    float _4053 = _4069;
    Interval _4050;
    _4050.lo = _4052;
    _4050.hi = _4053;
    Interval _4051 = _4050;
    Interval _4054 = _4051;
    Interval _4070 = _4054;
    Interval3 _4048;
    _4048.x = _4066;
    _4048.y = _4068;
    _4048.z = _4070;
    Interval3 _4049 = _4048;
    Interval3 _4071 = _4049;
    Interval3 param_var_a_9 = _4071;
    Interval3 param_var_b_9 = raw;
    Interval _4037 = param_var_a_9.x;
    Interval _4038 = param_var_b_9.x;
    Interval _4884 = imul(_4037, _4038, intervalFailed, optical_product_upper);
    Interval _4039 = _4884;
    Interval _4040 = param_var_a_9.y;
    Interval _4041 = param_var_b_9.y;
    Interval _4889 = imul(_4040, _4041, intervalFailed, optical_product_upper);
    Interval _4042 = _4889;
    Interval _4890 = iadd(_4039, _4042, intervalFailed);
    Interval _4043 = _4890;
    Interval _4044 = param_var_a_9.z;
    Interval _4045 = param_var_b_9.z;
    Interval _4895 = imul(_4044, _4045, intervalFailed, optical_product_upper);
    Interval _4046 = _4895;
    Interval _4896 = iadd(_4043, _4046, intervalFailed);
    Interval _4047 = _4896;
    Interval param_var_z = _4047;
    Interval3 _4035;
    _4035.x = param_var_x_3;
    _4035.y = param_var_y;
    _4035.z = param_var_z;
    Interval3 _4036 = _4035;
    Interval3 world = _4036;
    float3 param_var_v_7 = float3(TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[2].x);
    float _4028 = param_var_v_7.x;
    float _4025 = _4028;
    float _4026 = _4028;
    Interval _4023;
    _4023.lo = _4025;
    _4023.hi = _4026;
    Interval _4024 = _4023;
    Interval _4027 = _4024;
    Interval _4029 = _4027;
    float _4030 = param_var_v_7.y;
    float _4020 = _4030;
    float _4021 = _4030;
    Interval _4018;
    _4018.lo = _4020;
    _4018.hi = _4021;
    Interval _4019 = _4018;
    Interval _4022 = _4019;
    Interval _4031 = _4022;
    float _4032 = param_var_v_7.z;
    float _4015 = _4032;
    float _4016 = _4032;
    Interval _4013;
    _4013.lo = _4015;
    _4013.hi = _4016;
    Interval _4014 = _4013;
    Interval _4017 = _4014;
    Interval _4033 = _4017;
    Interval3 _4011;
    _4011.x = _4029;
    _4011.y = _4031;
    _4011.z = _4033;
    Interval3 _4012 = _4011;
    Interval3 _4034 = _4012;
    Interval3 param_var_a_10 = _4034;
    Interval3 param_var_b_10 = world;
    Interval _4000 = param_var_a_10.x;
    Interval _4001 = param_var_b_10.x;
    Interval _4966 = imul(_4000, _4001, intervalFailed, optical_product_upper);
    Interval _4002 = _4966;
    Interval _4003 = param_var_a_10.y;
    Interval _4004 = param_var_b_10.y;
    Interval _4971 = imul(_4003, _4004, intervalFailed, optical_product_upper);
    Interval _4005 = _4971;
    Interval _4972 = iadd(_4002, _4005, intervalFailed);
    Interval _4006 = _4972;
    Interval _4007 = param_var_a_10.z;
    Interval _4008 = param_var_b_10.z;
    Interval _4977 = imul(_4007, _4008, intervalFailed, optical_product_upper);
    Interval _4009 = _4977;
    Interval _4978 = iadd(_4006, _4009, intervalFailed);
    Interval _4010 = _4978;
    Interval param_var_x_4 = _4010;
    float3 param_var_v_8 = float3(TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[2].y);
    float _3993 = param_var_v_8.x;
    float _3990 = _3993;
    float _3991 = _3993;
    Interval _3988;
    _3988.lo = _3990;
    _3988.hi = _3991;
    Interval _3989 = _3988;
    Interval _3992 = _3989;
    Interval _3994 = _3992;
    float _3995 = param_var_v_8.y;
    float _3985 = _3995;
    float _3986 = _3995;
    Interval _3983;
    _3983.lo = _3985;
    _3983.hi = _3986;
    Interval _3984 = _3983;
    Interval _3987 = _3984;
    Interval _3996 = _3987;
    float _3997 = param_var_v_8.z;
    float _3980 = _3997;
    float _3981 = _3997;
    Interval _3978;
    _3978.lo = _3980;
    _3978.hi = _3981;
    Interval _3979 = _3978;
    Interval _3982 = _3979;
    Interval _3998 = _3982;
    Interval3 _3976;
    _3976.x = _3994;
    _3976.y = _3996;
    _3976.z = _3998;
    Interval3 _3977 = _3976;
    Interval3 _3999 = _3977;
    Interval3 param_var_a_11 = _3999;
    Interval3 param_var_b_11 = world;
    Interval _3965 = param_var_a_11.x;
    Interval _3966 = param_var_b_11.x;
    Interval _5040 = imul(_3965, _3966, intervalFailed, optical_product_upper);
    Interval _3967 = _5040;
    Interval _3968 = param_var_a_11.y;
    Interval _3969 = param_var_b_11.y;
    Interval _5045 = imul(_3968, _3969, intervalFailed, optical_product_upper);
    Interval _3970 = _5045;
    Interval _5046 = iadd(_3967, _3970, intervalFailed);
    Interval _3971 = _5046;
    Interval _3972 = param_var_a_11.z;
    Interval _3973 = param_var_b_11.z;
    Interval _5051 = imul(_3972, _3973, intervalFailed, optical_product_upper);
    Interval _3974 = _5051;
    Interval _5052 = iadd(_3971, _3974, intervalFailed);
    Interval _3975 = _5052;
    Interval param_var_y_1 = _3975;
    float3 param_var_v_9 = float3(TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[2].z);
    float _3958 = param_var_v_9.x;
    float _3955 = _3958;
    float _3956 = _3958;
    Interval _3953;
    _3953.lo = _3955;
    _3953.hi = _3956;
    Interval _3954 = _3953;
    Interval _3957 = _3954;
    Interval _3959 = _3957;
    float _3960 = param_var_v_9.y;
    float _3950 = _3960;
    float _3951 = _3960;
    Interval _3948;
    _3948.lo = _3950;
    _3948.hi = _3951;
    Interval _3949 = _3948;
    Interval _3952 = _3949;
    Interval _3961 = _3952;
    float _3962 = param_var_v_9.z;
    float _3945 = _3962;
    float _3946 = _3962;
    Interval _3943;
    _3943.lo = _3945;
    _3943.hi = _3946;
    Interval _3944 = _3943;
    Interval _3947 = _3944;
    Interval _3963 = _3947;
    Interval3 _3941;
    _3941.x = _3959;
    _3941.y = _3961;
    _3941.z = _3963;
    Interval3 _3942 = _3941;
    Interval3 _3964 = _3942;
    Interval3 param_var_a_12 = _3964;
    Interval3 param_var_b_12 = world;
    Interval _3930 = param_var_a_12.x;
    Interval _3931 = param_var_b_12.x;
    Interval _5114 = imul(_3930, _3931, intervalFailed, optical_product_upper);
    Interval _3932 = _5114;
    Interval _3933 = param_var_a_12.y;
    Interval _3934 = param_var_b_12.y;
    Interval _5119 = imul(_3933, _3934, intervalFailed, optical_product_upper);
    Interval _3935 = _5119;
    Interval _5120 = iadd(_3932, _3935, intervalFailed);
    Interval _3936 = _5120;
    Interval _3937 = param_var_a_12.z;
    Interval _3938 = param_var_b_12.z;
    Interval _5125 = imul(_3937, _3938, intervalFailed, optical_product_upper);
    Interval _3939 = _5125;
    Interval _5126 = iadd(_3936, _3939, intervalFailed);
    Interval _3940 = _5126;
    Interval param_var_z_1 = _3940;
    Interval3 _3928;
    _3928.x = param_var_x_4;
    _3928.y = param_var_y_1;
    _3928.z = param_var_z_1;
    Interval3 _3929 = _3928;
    Interval3 param_var_a_13 = _3929;
    Interval3 _3921 = param_var_a_13;
    float _3922 = 1.0;
    float _3918 = _3922;
    float _3919 = _3922;
    Interval _3916;
    _3916.lo = _3918;
    _3916.hi = _3919;
    Interval _3917 = _3916;
    Interval _3920 = _3917;
    Interval _3923 = _3920;
    Interval3 _3924 = param_var_a_13;
    Interval _3907 = _3924.x;
    bool _3900 = false;
    if (_3907.lo <= 0.0)
    {
        _3900 = _3907.hi >= 0.0;
    }
    float _3899;
    if (_3900)
    {
        _3899 = 0.0;
    }
    else
    {
        _3899 = precise::min(abs(_3907.lo), abs(_3907.hi));
    }
    float _3898 = _3899;
    float _3901 = precise::max(abs(_3907.lo), abs(_3907.hi));
    float _3902 = spvFMul(_3898, _3898);
    float _5178 = interval_down(_3902, intervalFailed);
    float _3903 = precise::max(0.0, _5178);
    float _3904 = spvFMul(_3901, _3901);
    float _5182 = interval_up(_3904, intervalFailed);
    float _3905 = _5182;
    Interval _3896;
    _3896.lo = _3903;
    _3896.hi = _3905;
    Interval _3897 = _3896;
    Interval _3906 = _3897;
    Interval _3908 = _3906;
    Interval _3909 = _3924.y;
    bool _3889 = false;
    if (_3909.lo <= 0.0)
    {
        _3889 = _3909.hi >= 0.0;
    }
    float _3888;
    if (_3889)
    {
        _3888 = 0.0;
    }
    else
    {
        _3888 = precise::min(abs(_3909.lo), abs(_3909.hi));
    }
    float _3887 = _3888;
    float _3890 = precise::max(abs(_3909.lo), abs(_3909.hi));
    float _3891 = spvFMul(_3887, _3887);
    float _5221 = interval_down(_3891, intervalFailed);
    float _3892 = precise::max(0.0, _5221);
    float _3893 = spvFMul(_3890, _3890);
    float _5225 = interval_up(_3893, intervalFailed);
    float _3894 = _5225;
    Interval _3885;
    _3885.lo = _3892;
    _3885.hi = _3894;
    Interval _3886 = _3885;
    Interval _3895 = _3886;
    Interval _3910 = _3895;
    Interval _5233 = iadd(_3908, _3910, intervalFailed);
    Interval _3911 = _5233;
    Interval _3912 = _3924.z;
    bool _3878 = false;
    if (_3912.lo <= 0.0)
    {
        _3878 = _3912.hi >= 0.0;
    }
    float _3877;
    if (_3878)
    {
        _3877 = 0.0;
    }
    else
    {
        _3877 = precise::min(abs(_3912.lo), abs(_3912.hi));
    }
    float _3876 = _3877;
    float _3879 = precise::max(abs(_3912.lo), abs(_3912.hi));
    float _3880 = spvFMul(_3876, _3876);
    float _5265 = interval_down(_3880, intervalFailed);
    float _3881 = precise::max(0.0, _5265);
    float _3882 = spvFMul(_3879, _3879);
    float _5269 = interval_up(_3882, intervalFailed);
    float _3883 = _5269;
    Interval _3874;
    _3874.lo = _3881;
    _3874.hi = _3883;
    Interval _3875 = _3874;
    Interval _3884 = _3875;
    Interval _3913 = _3884;
    Interval _5277 = iadd(_3911, _3913, intervalFailed);
    Interval _3914 = _5277;
    Interval _5278 = isqrt(_3914, intervalFailed);
    Interval _3915 = _5278;
    Interval _3925 = _3915;
    Interval _5280 = idiv(_3923, _3925, intervalFailed, interval_divide_upper);
    Interval _3926 = _5280;
    Interval _3864 = _3921.x;
    Interval _3865 = _3926;
    Interval _5284 = imul(_3864, _3865, intervalFailed, optical_product_upper);
    Interval _3866 = _5284;
    Interval _3867 = _3921.y;
    Interval _3868 = _3926;
    Interval _5288 = imul(_3867, _3868, intervalFailed, optical_product_upper);
    Interval _3869 = _5288;
    Interval _3870 = _3921.z;
    Interval _3871 = _3926;
    Interval _5292 = imul(_3870, _3871, intervalFailed, optical_product_upper);
    Interval _3872 = _5292;
    Interval3 _3862;
    _3862.x = _3866;
    _3862.y = _3869;
    _3862.z = _3872;
    Interval3 _3863 = _3862;
    Interval3 _3873 = _3863;
    Interval3 _3927 = _3873;
    return _3927;
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
    bool _17085 = false;
    if (param_var_p.a.w == 2.0)
    {
        _17085 = param_var_p.b.w == 2.0;
    }
    bool _17086 = false;
    if (_17085)
    {
        _17086 = param_var_p.c.w == 2.0;
    }
    bool _17087 = _17086;
    if (_17087)
    {
        float3 param_var_v = p.b.xyz;
        float _17078 = param_var_v.x;
        float _17075 = _17078;
        float _17076 = _17078;
        Interval _17073;
        _17073.lo = _17075;
        _17073.hi = _17076;
        Interval _17074 = _17073;
        Interval _17077 = _17074;
        Interval _17079 = _17077;
        float _17080 = param_var_v.y;
        float _17070 = _17080;
        float _17071 = _17080;
        Interval _17068;
        _17068.lo = _17070;
        _17068.hi = _17071;
        Interval _17069 = _17068;
        Interval _17072 = _17069;
        Interval _17081 = _17072;
        float _17082 = param_var_v.z;
        float _17065 = _17082;
        float _17066 = _17082;
        Interval _17063;
        _17063.lo = _17065;
        _17063.hi = _17066;
        Interval _17064 = _17063;
        Interval _17067 = _17064;
        Interval _17083 = _17067;
        Interval3 _17061;
        _17061.x = _17079;
        _17061.y = _17081;
        _17061.z = _17083;
        Interval3 _17062 = _17061;
        Interval3 _17084 = _17062;
        Interval3 param_var_a = _17084;
        Interval3 _17054 = param_var_a;
        float _17055 = 1.0;
        float _17051 = _17055;
        float _17052 = _17055;
        Interval _17049;
        _17049.lo = _17051;
        _17049.hi = _17052;
        Interval _17050 = _17049;
        Interval _17053 = _17050;
        Interval _17056 = _17053;
        Interval3 _17057 = param_var_a;
        Interval _17040 = _17057.x;
        bool _17033 = false;
        if (_17040.lo <= 0.0)
        {
            _17033 = _17040.hi >= 0.0;
        }
        float _17032;
        if (_17033)
        {
            _17032 = 0.0;
        }
        else
        {
            _17032 = precise::min(abs(_17040.lo), abs(_17040.hi));
        }
        float _17031 = _17032;
        float _17034 = precise::max(abs(_17040.lo), abs(_17040.hi));
        float _17035 = spvFMul(_17031, _17031);
        float _17195 = interval_down(_17035, intervalFailed);
        float _17036 = precise::max(0.0, _17195);
        float _17037 = spvFMul(_17034, _17034);
        float _17199 = interval_up(_17037, intervalFailed);
        float _17038 = _17199;
        Interval _17029;
        _17029.lo = _17036;
        _17029.hi = _17038;
        Interval _17030 = _17029;
        Interval _17039 = _17030;
        Interval _17041 = _17039;
        Interval _17042 = _17057.y;
        bool _17022 = false;
        if (_17042.lo <= 0.0)
        {
            _17022 = _17042.hi >= 0.0;
        }
        float _17021;
        if (_17022)
        {
            _17021 = 0.0;
        }
        else
        {
            _17021 = precise::min(abs(_17042.lo), abs(_17042.hi));
        }
        float _17020 = _17021;
        float _17023 = precise::max(abs(_17042.lo), abs(_17042.hi));
        float _17024 = spvFMul(_17020, _17020);
        float _17238 = interval_down(_17024, intervalFailed);
        float _17025 = precise::max(0.0, _17238);
        float _17026 = spvFMul(_17023, _17023);
        float _17242 = interval_up(_17026, intervalFailed);
        float _17027 = _17242;
        Interval _17018;
        _17018.lo = _17025;
        _17018.hi = _17027;
        Interval _17019 = _17018;
        Interval _17028 = _17019;
        Interval _17043 = _17028;
        Interval _17250 = iadd(_17041, _17043, intervalFailed);
        Interval _17044 = _17250;
        Interval _17045 = _17057.z;
        bool _17011 = false;
        if (_17045.lo <= 0.0)
        {
            _17011 = _17045.hi >= 0.0;
        }
        float _17010;
        if (_17011)
        {
            _17010 = 0.0;
        }
        else
        {
            _17010 = precise::min(abs(_17045.lo), abs(_17045.hi));
        }
        float _17009 = _17010;
        float _17012 = precise::max(abs(_17045.lo), abs(_17045.hi));
        float _17013 = spvFMul(_17009, _17009);
        float _17282 = interval_down(_17013, intervalFailed);
        float _17014 = precise::max(0.0, _17282);
        float _17015 = spvFMul(_17012, _17012);
        float _17286 = interval_up(_17015, intervalFailed);
        float _17016 = _17286;
        Interval _17007;
        _17007.lo = _17014;
        _17007.hi = _17016;
        Interval _17008 = _17007;
        Interval _17017 = _17008;
        Interval _17046 = _17017;
        Interval _17294 = iadd(_17044, _17046, intervalFailed);
        Interval _17047 = _17294;
        Interval _17295 = isqrt(_17047, intervalFailed);
        Interval _17048 = _17295;
        Interval _17058 = _17048;
        Interval _17297 = idiv(_17056, _17058, intervalFailed, interval_divide_upper);
        Interval _17059 = _17297;
        Interval _16997 = _17054.x;
        Interval _16998 = _17059;
        Interval _17301 = imul(_16997, _16998, intervalFailed, optical_product_upper);
        Interval _16999 = _17301;
        Interval _17000 = _17054.y;
        Interval _17001 = _17059;
        Interval _17305 = imul(_17000, _17001, intervalFailed, optical_product_upper);
        Interval _17002 = _17305;
        Interval _17003 = _17054.z;
        Interval _17004 = _17059;
        Interval _17309 = imul(_17003, _17004, intervalFailed, optical_product_upper);
        Interval _17005 = _17309;
        Interval3 _16995;
        _16995.x = _16999;
        _16995.y = _17002;
        _16995.z = _17005;
        Interval3 _16996 = _16995;
        Interval3 _17006 = _16996;
        Interval3 _17060 = _17006;
        return _17060;
    }
    float3 param_var_v_1 = p.b.xyz;
    float _16988 = param_var_v_1.x;
    float _16985 = _16988;
    float _16986 = _16988;
    Interval _16983;
    _16983.lo = _16985;
    _16983.hi = _16986;
    Interval _16984 = _16983;
    Interval _16987 = _16984;
    Interval _16989 = _16987;
    float _16990 = param_var_v_1.y;
    float _16980 = _16990;
    float _16981 = _16990;
    Interval _16978;
    _16978.lo = _16980;
    _16978.hi = _16981;
    Interval _16979 = _16978;
    Interval _16982 = _16979;
    Interval _16991 = _16982;
    float _16992 = param_var_v_1.z;
    float _16975 = _16992;
    float _16976 = _16992;
    Interval _16973;
    _16973.lo = _16975;
    _16973.hi = _16976;
    Interval _16974 = _16973;
    Interval _16977 = _16974;
    Interval _16993 = _16977;
    Interval3 _16971;
    _16971.x = _16989;
    _16971.y = _16991;
    _16971.z = _16993;
    Interval3 _16972 = _16971;
    Interval3 _16994 = _16972;
    Interval3 param_var_a_1 = _16994;
    float3 param_var_v_2 = p.a.xyz;
    float _16964 = param_var_v_2.x;
    float _16961 = _16964;
    float _16962 = _16964;
    Interval _16959;
    _16959.lo = _16961;
    _16959.hi = _16962;
    Interval _16960 = _16959;
    Interval _16963 = _16960;
    Interval _16965 = _16963;
    float _16966 = param_var_v_2.y;
    float _16956 = _16966;
    float _16957 = _16966;
    Interval _16954;
    _16954.lo = _16956;
    _16954.hi = _16957;
    Interval _16955 = _16954;
    Interval _16958 = _16955;
    Interval _16967 = _16958;
    float _16968 = param_var_v_2.z;
    float _16951 = _16968;
    float _16952 = _16968;
    Interval _16949;
    _16949.lo = _16951;
    _16949.hi = _16952;
    Interval _16950 = _16949;
    Interval _16953 = _16950;
    Interval _16969 = _16953;
    Interval3 _16947;
    _16947.x = _16965;
    _16947.y = _16967;
    _16947.z = _16969;
    Interval3 _16948 = _16947;
    Interval3 _16970 = _16948;
    Interval3 param_var_b = _16970;
    Interval3 _16938 = param_var_a_1;
    Interval _16939 = param_var_b.x;
    float _16935 = as_type<float>(as_type<uint>(_16939.hi) ^ 2147483648u);
    float _16936 = as_type<float>(as_type<uint>(_16939.lo) ^ 2147483648u);
    Interval _16933;
    _16933.lo = _16935;
    _16933.hi = _16936;
    Interval _16934 = _16933;
    Interval _16937 = _16934;
    Interval _16940 = _16937;
    Interval _16941 = param_var_b.y;
    float _16930 = as_type<float>(as_type<uint>(_16941.hi) ^ 2147483648u);
    float _16931 = as_type<float>(as_type<uint>(_16941.lo) ^ 2147483648u);
    Interval _16928;
    _16928.lo = _16930;
    _16928.hi = _16931;
    Interval _16929 = _16928;
    Interval _16932 = _16929;
    Interval _16942 = _16932;
    Interval _16943 = param_var_b.z;
    float _16925 = as_type<float>(as_type<uint>(_16943.hi) ^ 2147483648u);
    float _16926 = as_type<float>(as_type<uint>(_16943.lo) ^ 2147483648u);
    Interval _16923;
    _16923.lo = _16925;
    _16923.hi = _16926;
    Interval _16924 = _16923;
    Interval _16927 = _16924;
    Interval _16944 = _16927;
    Interval3 _16921;
    _16921.x = _16940;
    _16921.y = _16942;
    _16921.z = _16944;
    Interval3 _16922 = _16921;
    Interval3 _16945 = _16922;
    Interval _16911 = _16938.x;
    Interval _16912 = _16945.x;
    Interval _17480 = iadd(_16911, _16912, intervalFailed);
    Interval _16913 = _17480;
    Interval _16914 = _16938.y;
    Interval _16915 = _16945.y;
    Interval _17485 = iadd(_16914, _16915, intervalFailed);
    Interval _16916 = _17485;
    Interval _16917 = _16938.z;
    Interval _16918 = _16945.z;
    Interval _17490 = iadd(_16917, _16918, intervalFailed);
    Interval _16919 = _17490;
    Interval3 _16909;
    _16909.x = _16913;
    _16909.y = _16916;
    _16909.z = _16919;
    Interval3 _16910 = _16909;
    Interval3 _16920 = _16910;
    Interval3 _16946 = _16920;
    Interval3 param_var_a_2 = _16946;
    float3 param_var_v_3 = p.c.xyz;
    float _16902 = param_var_v_3.x;
    float _16899 = _16902;
    float _16900 = _16902;
    Interval _16897;
    _16897.lo = _16899;
    _16897.hi = _16900;
    Interval _16898 = _16897;
    Interval _16901 = _16898;
    Interval _16903 = _16901;
    float _16904 = param_var_v_3.y;
    float _16894 = _16904;
    float _16895 = _16904;
    Interval _16892;
    _16892.lo = _16894;
    _16892.hi = _16895;
    Interval _16893 = _16892;
    Interval _16896 = _16893;
    Interval _16905 = _16896;
    float _16906 = param_var_v_3.z;
    float _16889 = _16906;
    float _16890 = _16906;
    Interval _16887;
    _16887.lo = _16889;
    _16887.hi = _16890;
    Interval _16888 = _16887;
    Interval _16891 = _16888;
    Interval _16907 = _16891;
    Interval3 _16885;
    _16885.x = _16903;
    _16885.y = _16905;
    _16885.z = _16907;
    Interval3 _16886 = _16885;
    Interval3 _16908 = _16886;
    Interval3 param_var_a_3 = _16908;
    float3 param_var_v_4 = p.a.xyz;
    float _16878 = param_var_v_4.x;
    float _16875 = _16878;
    float _16876 = _16878;
    Interval _16873;
    _16873.lo = _16875;
    _16873.hi = _16876;
    Interval _16874 = _16873;
    Interval _16877 = _16874;
    Interval _16879 = _16877;
    float _16880 = param_var_v_4.y;
    float _16870 = _16880;
    float _16871 = _16880;
    Interval _16868;
    _16868.lo = _16870;
    _16868.hi = _16871;
    Interval _16869 = _16868;
    Interval _16872 = _16869;
    Interval _16881 = _16872;
    float _16882 = param_var_v_4.z;
    float _16865 = _16882;
    float _16866 = _16882;
    Interval _16863;
    _16863.lo = _16865;
    _16863.hi = _16866;
    Interval _16864 = _16863;
    Interval _16867 = _16864;
    Interval _16883 = _16867;
    Interval3 _16861;
    _16861.x = _16879;
    _16861.y = _16881;
    _16861.z = _16883;
    Interval3 _16862 = _16861;
    Interval3 _16884 = _16862;
    Interval3 param_var_b_1 = _16884;
    Interval3 _16852 = param_var_a_3;
    Interval _16853 = param_var_b_1.x;
    float _16849 = as_type<float>(as_type<uint>(_16853.hi) ^ 2147483648u);
    float _16850 = as_type<float>(as_type<uint>(_16853.lo) ^ 2147483648u);
    Interval _16847;
    _16847.lo = _16849;
    _16847.hi = _16850;
    Interval _16848 = _16847;
    Interval _16851 = _16848;
    Interval _16854 = _16851;
    Interval _16855 = param_var_b_1.y;
    float _16844 = as_type<float>(as_type<uint>(_16855.hi) ^ 2147483648u);
    float _16845 = as_type<float>(as_type<uint>(_16855.lo) ^ 2147483648u);
    Interval _16842;
    _16842.lo = _16844;
    _16842.hi = _16845;
    Interval _16843 = _16842;
    Interval _16846 = _16843;
    Interval _16856 = _16846;
    Interval _16857 = param_var_b_1.z;
    float _16839 = as_type<float>(as_type<uint>(_16857.hi) ^ 2147483648u);
    float _16840 = as_type<float>(as_type<uint>(_16857.lo) ^ 2147483648u);
    Interval _16837;
    _16837.lo = _16839;
    _16837.hi = _16840;
    Interval _16838 = _16837;
    Interval _16841 = _16838;
    Interval _16858 = _16841;
    Interval3 _16835;
    _16835.x = _16854;
    _16835.y = _16856;
    _16835.z = _16858;
    Interval3 _16836 = _16835;
    Interval3 _16859 = _16836;
    Interval _16825 = _16852.x;
    Interval _16826 = _16859.x;
    Interval _17661 = iadd(_16825, _16826, intervalFailed);
    Interval _16827 = _17661;
    Interval _16828 = _16852.y;
    Interval _16829 = _16859.y;
    Interval _17666 = iadd(_16828, _16829, intervalFailed);
    Interval _16830 = _17666;
    Interval _16831 = _16852.z;
    Interval _16832 = _16859.z;
    Interval _17671 = iadd(_16831, _16832, intervalFailed);
    Interval _16833 = _17671;
    Interval3 _16823;
    _16823.x = _16827;
    _16823.y = _16830;
    _16823.z = _16833;
    Interval3 _16824 = _16823;
    Interval3 _16834 = _16824;
    Interval3 _16860 = _16834;
    Interval3 param_var_b_2 = _16860;
    Interval _16801 = param_var_a_2.y;
    Interval _16802 = param_var_b_2.z;
    Interval _17686 = imul(_16801, _16802, intervalFailed, optical_product_upper);
    Interval _16803 = _17686;
    Interval _16804 = param_var_a_2.z;
    Interval _16805 = param_var_b_2.y;
    Interval _17691 = imul(_16804, _16805, intervalFailed, optical_product_upper);
    Interval _16806 = _17691;
    Interval _16797 = _16803;
    Interval _16798 = _16806;
    float _16794 = as_type<float>(as_type<uint>(_16798.hi) ^ 2147483648u);
    float _16795 = as_type<float>(as_type<uint>(_16798.lo) ^ 2147483648u);
    Interval _16792;
    _16792.lo = _16794;
    _16792.hi = _16795;
    Interval _16793 = _16792;
    Interval _16796 = _16793;
    Interval _16799 = _16796;
    Interval _17711 = iadd(_16797, _16799, intervalFailed);
    Interval _16800 = _17711;
    Interval _16807 = _16800;
    Interval _16808 = param_var_a_2.z;
    Interval _16809 = param_var_b_2.x;
    Interval _17717 = imul(_16808, _16809, intervalFailed, optical_product_upper);
    Interval _16810 = _17717;
    Interval _16811 = param_var_a_2.x;
    Interval _16812 = param_var_b_2.z;
    Interval _17722 = imul(_16811, _16812, intervalFailed, optical_product_upper);
    Interval _16813 = _17722;
    Interval _16788 = _16810;
    Interval _16789 = _16813;
    float _16785 = as_type<float>(as_type<uint>(_16789.hi) ^ 2147483648u);
    float _16786 = as_type<float>(as_type<uint>(_16789.lo) ^ 2147483648u);
    Interval _16783;
    _16783.lo = _16785;
    _16783.hi = _16786;
    Interval _16784 = _16783;
    Interval _16787 = _16784;
    Interval _16790 = _16787;
    Interval _17742 = iadd(_16788, _16790, intervalFailed);
    Interval _16791 = _17742;
    Interval _16814 = _16791;
    Interval _16815 = param_var_a_2.x;
    Interval _16816 = param_var_b_2.y;
    Interval _17748 = imul(_16815, _16816, intervalFailed, optical_product_upper);
    Interval _16817 = _17748;
    Interval _16818 = param_var_a_2.y;
    Interval _16819 = param_var_b_2.x;
    Interval _17753 = imul(_16818, _16819, intervalFailed, optical_product_upper);
    Interval _16820 = _17753;
    Interval _16779 = _16817;
    Interval _16780 = _16820;
    float _16776 = as_type<float>(as_type<uint>(_16780.hi) ^ 2147483648u);
    float _16777 = as_type<float>(as_type<uint>(_16780.lo) ^ 2147483648u);
    Interval _16774;
    _16774.lo = _16776;
    _16774.hi = _16777;
    Interval _16775 = _16774;
    Interval _16778 = _16775;
    Interval _16781 = _16778;
    Interval _17773 = iadd(_16779, _16781, intervalFailed);
    Interval _16782 = _17773;
    Interval _16821 = _16782;
    Interval3 _16772;
    _16772.x = _16807;
    _16772.y = _16814;
    _16772.z = _16821;
    Interval3 _16773 = _16772;
    Interval3 _16822 = _16773;
    Interval3 param_var_a_4 = _16822;
    Interval3 _16765 = param_var_a_4;
    float _16766 = 1.0;
    float _16762 = _16766;
    float _16763 = _16766;
    Interval _16760;
    _16760.lo = _16762;
    _16760.hi = _16763;
    Interval _16761 = _16760;
    Interval _16764 = _16761;
    Interval _16767 = _16764;
    Interval3 _16768 = param_var_a_4;
    Interval _16751 = _16768.x;
    bool _16744 = false;
    if (_16751.lo <= 0.0)
    {
        _16744 = _16751.hi >= 0.0;
    }
    float _16743;
    if (_16744)
    {
        _16743 = 0.0;
    }
    else
    {
        _16743 = precise::min(abs(_16751.lo), abs(_16751.hi));
    }
    float _16742 = _16743;
    float _16745 = precise::max(abs(_16751.lo), abs(_16751.hi));
    float _16746 = spvFMul(_16742, _16742);
    float _17826 = interval_down(_16746, intervalFailed);
    float _16747 = precise::max(0.0, _17826);
    float _16748 = spvFMul(_16745, _16745);
    float _17830 = interval_up(_16748, intervalFailed);
    float _16749 = _17830;
    Interval _16740;
    _16740.lo = _16747;
    _16740.hi = _16749;
    Interval _16741 = _16740;
    Interval _16750 = _16741;
    Interval _16752 = _16750;
    Interval _16753 = _16768.y;
    bool _16733 = false;
    if (_16753.lo <= 0.0)
    {
        _16733 = _16753.hi >= 0.0;
    }
    float _16732;
    if (_16733)
    {
        _16732 = 0.0;
    }
    else
    {
        _16732 = precise::min(abs(_16753.lo), abs(_16753.hi));
    }
    float _16731 = _16732;
    float _16734 = precise::max(abs(_16753.lo), abs(_16753.hi));
    float _16735 = spvFMul(_16731, _16731);
    float _17869 = interval_down(_16735, intervalFailed);
    float _16736 = precise::max(0.0, _17869);
    float _16737 = spvFMul(_16734, _16734);
    float _17873 = interval_up(_16737, intervalFailed);
    float _16738 = _17873;
    Interval _16729;
    _16729.lo = _16736;
    _16729.hi = _16738;
    Interval _16730 = _16729;
    Interval _16739 = _16730;
    Interval _16754 = _16739;
    Interval _17881 = iadd(_16752, _16754, intervalFailed);
    Interval _16755 = _17881;
    Interval _16756 = _16768.z;
    bool _16722 = false;
    if (_16756.lo <= 0.0)
    {
        _16722 = _16756.hi >= 0.0;
    }
    float _16721;
    if (_16722)
    {
        _16721 = 0.0;
    }
    else
    {
        _16721 = precise::min(abs(_16756.lo), abs(_16756.hi));
    }
    float _16720 = _16721;
    float _16723 = precise::max(abs(_16756.lo), abs(_16756.hi));
    float _16724 = spvFMul(_16720, _16720);
    float _17913 = interval_down(_16724, intervalFailed);
    float _16725 = precise::max(0.0, _17913);
    float _16726 = spvFMul(_16723, _16723);
    float _17917 = interval_up(_16726, intervalFailed);
    float _16727 = _17917;
    Interval _16718;
    _16718.lo = _16725;
    _16718.hi = _16727;
    Interval _16719 = _16718;
    Interval _16728 = _16719;
    Interval _16757 = _16728;
    Interval _17925 = iadd(_16755, _16757, intervalFailed);
    Interval _16758 = _17925;
    Interval _17926 = isqrt(_16758, intervalFailed);
    Interval _16759 = _17926;
    Interval _16769 = _16759;
    Interval _17928 = idiv(_16767, _16769, intervalFailed, interval_divide_upper);
    Interval _16770 = _17928;
    Interval _16708 = _16765.x;
    Interval _16709 = _16770;
    Interval _17932 = imul(_16708, _16709, intervalFailed, optical_product_upper);
    Interval _16710 = _17932;
    Interval _16711 = _16765.y;
    Interval _16712 = _16770;
    Interval _17936 = imul(_16711, _16712, intervalFailed, optical_product_upper);
    Interval _16713 = _17936;
    Interval _16714 = _16765.z;
    Interval _16715 = _16770;
    Interval _17940 = imul(_16714, _16715, intervalFailed, optical_product_upper);
    Interval _16716 = _17940;
    Interval3 _16706;
    _16706.x = _16710;
    _16706.y = _16713;
    _16706.z = _16716;
    Interval3 _16707 = _16706;
    Interval3 _16717 = _16707;
    Interval3 _16771 = _16717;
    return _16771;
}

static inline __attribute__((always_inline))
Interval3 oriented(thread const Interval3& n, thread const Interval3& direction, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval3 param_var_a = n;
    Interval3 param_var_b = direction;
    Interval _18012 = param_var_a.x;
    Interval _18013 = param_var_b.x;
    Interval _18029 = imul(_18012, _18013, intervalFailed, optical_product_upper);
    Interval _18014 = _18029;
    Interval _18015 = param_var_a.y;
    Interval _18016 = param_var_b.y;
    Interval _18034 = imul(_18015, _18016, intervalFailed, optical_product_upper);
    Interval _18017 = _18034;
    Interval _18035 = iadd(_18014, _18017, intervalFailed);
    Interval _18018 = _18035;
    Interval _18019 = param_var_a.z;
    Interval _18020 = param_var_b.z;
    Interval _18040 = imul(_18019, _18020, intervalFailed, optical_product_upper);
    Interval _18021 = _18040;
    Interval _18041 = iadd(_18018, _18021, intervalFailed);
    Interval _18022 = _18041;
    Interval _dot = _18022;
    if (_dot.lo > 0.0)
    {
        Interval3 param_var_a_1 = n;
        float param_var_x = -1.0;
        float _18009 = param_var_x;
        float _18010 = param_var_x;
        Interval _18007;
        _18007.lo = _18009;
        _18007.hi = _18010;
        Interval _18008 = _18007;
        Interval _18011 = _18008;
        Interval param_var_b_1 = _18011;
        Interval _17997 = param_var_a_1.x;
        Interval _17998 = param_var_b_1;
        Interval _18059 = imul(_17997, _17998, intervalFailed, optical_product_upper);
        Interval _17999 = _18059;
        Interval _18000 = param_var_a_1.y;
        Interval _18001 = param_var_b_1;
        Interval _18063 = imul(_18000, _18001, intervalFailed, optical_product_upper);
        Interval _18002 = _18063;
        Interval _18003 = param_var_a_1.z;
        Interval _18004 = param_var_b_1;
        Interval _18067 = imul(_18003, _18004, intervalFailed, optical_product_upper);
        Interval _18005 = _18067;
        Interval3 _17995;
        _17995.x = _17999;
        _17995.y = _18002;
        _17995.z = _18005;
        Interval3 _17996 = _17995;
        Interval3 _18006 = _17996;
        return _18006;
    }
    if (_dot.hi <= 0.0)
    {
        return n;
    }
    Interval3 param_var_a_2 = n;
    Interval3 param_var_a_3 = n;
    float param_var_x_1 = -1.0;
    float _17992 = param_var_x_1;
    float _17993 = param_var_x_1;
    Interval _17990;
    _17990.lo = _17992;
    _17990.hi = _17993;
    Interval _17991 = _17990;
    Interval _17994 = _17991;
    Interval param_var_b_2 = _17994;
    Interval _17980 = param_var_a_3.x;
    Interval _17981 = param_var_b_2;
    Interval _18095 = imul(_17980, _17981, intervalFailed, optical_product_upper);
    Interval _17982 = _18095;
    Interval _17983 = param_var_a_3.y;
    Interval _17984 = param_var_b_2;
    Interval _18099 = imul(_17983, _17984, intervalFailed, optical_product_upper);
    Interval _17985 = _18099;
    Interval _17986 = param_var_a_3.z;
    Interval _17987 = param_var_b_2;
    Interval _18103 = imul(_17986, _17987, intervalFailed, optical_product_upper);
    Interval _17988 = _18103;
    Interval3 _17978;
    _17978.x = _17982;
    _17978.y = _17985;
    _17978.z = _17988;
    Interval3 _17979 = _17978;
    Interval3 _17989 = _17979;
    Interval3 param_var_b_3 = _17989;
    Interval _17968 = param_var_a_2.x;
    Interval _17969 = param_var_b_3.x;
    float _17965 = precise::min(_17968.lo, _17969.lo);
    float _17966 = precise::max(_17968.hi, _17969.hi);
    Interval _17963;
    _17963.lo = _17965;
    _17963.hi = _17966;
    Interval _17964 = _17963;
    Interval _17967 = _17964;
    Interval _17970 = _17967;
    Interval _17971 = param_var_a_2.y;
    Interval _17972 = param_var_b_3.y;
    float _17960 = precise::min(_17971.lo, _17972.lo);
    float _17961 = precise::max(_17971.hi, _17972.hi);
    Interval _17958;
    _17958.lo = _17960;
    _17958.hi = _17961;
    Interval _17959 = _17958;
    Interval _17962 = _17959;
    Interval _17973 = _17962;
    Interval _17974 = param_var_a_2.z;
    Interval _17975 = param_var_b_3.z;
    float _17955 = precise::min(_17974.lo, _17975.lo);
    float _17956 = precise::max(_17974.hi, _17975.hi);
    Interval _17953;
    _17953.lo = _17955;
    _17953.hi = _17956;
    Interval _17954 = _17953;
    Interval _17957 = _17954;
    Interval _17976 = _17957;
    Interval3 _17951;
    _17951.x = _17970;
    _17951.y = _17973;
    _17951.z = _17976;
    Interval3 _17952 = _17951;
    Interval3 _17977 = _17952;
    return _17977;
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        float param_var_x = n;
        float _18578 = param_var_x;
        float _18579 = param_var_x;
        Interval _18576;
        _18576.lo = _18578;
        _18576.hi = _18579;
        Interval _18577 = _18576;
        Interval _18580 = _18577;
        Interval param_var_a = _18580;
        float param_var_lo = as_type<float>(1036831948u);
        float param_var_hi = as_type<float>(1036831950u);
        Interval _18574;
        _18574.lo = param_var_lo;
        _18574.hi = param_var_hi;
        Interval _18575 = _18574;
        Interval param_var_b = _18575;
        Interval _18601 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _18601;
    }
    if (d == 100.0)
    {
        float param_var_x_1 = n;
        float _18571 = param_var_x_1;
        float _18572 = param_var_x_1;
        Interval _18569;
        _18569.lo = _18571;
        _18569.hi = _18572;
        Interval _18570 = _18569;
        Interval _18573 = _18570;
        Interval param_var_a_1 = _18573;
        float param_var_lo_1 = as_type<float>(1008981769u);
        float param_var_hi_1 = as_type<float>(1008981771u);
        Interval _18567;
        _18567.lo = param_var_lo_1;
        _18567.hi = param_var_hi_1;
        Interval _18568 = _18567;
        Interval param_var_b_1 = _18568;
        Interval _18622 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _18622;
    }
    if (d == 1000.0)
    {
        float param_var_x_2 = n;
        float _18564 = param_var_x_2;
        float _18565 = param_var_x_2;
        Interval _18562;
        _18562.lo = _18564;
        _18562.hi = _18565;
        Interval _18563 = _18562;
        Interval _18566 = _18563;
        Interval param_var_a_2 = _18566;
        float param_var_lo_2 = as_type<float>(981668462u);
        float param_var_hi_2 = as_type<float>(981668464u);
        Interval _18560;
        _18560.lo = param_var_lo_2;
        _18560.hi = param_var_hi_2;
        Interval _18561 = _18560;
        Interval param_var_b_2 = _18561;
        Interval _18643 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _18643;
    }
    if (d == 10000.0)
    {
        float param_var_x_3 = n;
        float _18557 = param_var_x_3;
        float _18558 = param_var_x_3;
        Interval _18555;
        _18555.lo = _18557;
        _18555.hi = _18558;
        Interval _18556 = _18555;
        Interval _18559 = _18556;
        Interval param_var_a_3 = _18559;
        float param_var_lo_3 = as_type<float>(953267990u);
        float param_var_hi_3 = as_type<float>(953267992u);
        Interval _18553;
        _18553.lo = param_var_lo_3;
        _18553.hi = param_var_hi_3;
        Interval _18554 = _18553;
        Interval param_var_b_3 = _18554;
        Interval _18664 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _18664;
    }
    if (d == 100000.0)
    {
        float param_var_x_4 = n;
        float _18550 = param_var_x_4;
        float _18551 = param_var_x_4;
        Interval _18548;
        _18548.lo = _18550;
        _18548.hi = _18551;
        Interval _18549 = _18548;
        Interval _18552 = _18549;
        Interval param_var_a_4 = _18552;
        float param_var_lo_4 = as_type<float>(925353387u);
        float param_var_hi_4 = as_type<float>(925353389u);
        Interval _18546;
        _18546.lo = param_var_lo_4;
        _18546.hi = param_var_hi_4;
        Interval _18547 = _18546;
        Interval param_var_b_4 = _18547;
        Interval _18685 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _18685;
    }
    if (d == 128.0)
    {
        float param_var_x_5 = n;
        float _18543 = param_var_x_5;
        float _18544 = param_var_x_5;
        Interval _18541;
        _18541.lo = _18543;
        _18541.hi = _18544;
        Interval _18542 = _18541;
        Interval _18545 = _18542;
        Interval param_var_a_5 = _18545;
        float param_var_lo_5 = as_type<float>(1006632960u);
        float param_var_hi_5 = as_type<float>(1006632960u);
        Interval _18539;
        _18539.lo = param_var_lo_5;
        _18539.hi = param_var_hi_5;
        Interval _18540 = _18539;
        Interval param_var_b_5 = _18540;
        Interval _18706 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _18706;
    }
    if (d == 65535.0)
    {
        float param_var_x_6 = n;
        float _18536 = param_var_x_6;
        float _18537 = param_var_x_6;
        Interval _18534;
        _18534.lo = _18536;
        _18534.hi = _18537;
        Interval _18535 = _18534;
        Interval _18538 = _18535;
        Interval param_var_a_6 = _18538;
        float param_var_lo_6 = as_type<float>(931135615u);
        float param_var_hi_6 = as_type<float>(931135617u);
        Interval _18532;
        _18532.lo = param_var_lo_6;
        _18532.hi = param_var_hi_6;
        Interval _18533 = _18532;
        Interval param_var_b_6 = _18533;
        Interval _18727 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _18727;
    }
    float param_var_x_7 = n;
    float _18529 = param_var_x_7;
    float _18530 = param_var_x_7;
    Interval _18527;
    _18527.lo = _18529;
    _18527.hi = _18530;
    Interval _18528 = _18527;
    Interval _18531 = _18528;
    Interval param_var_a_7 = _18531;
    float param_var_x_8 = d;
    float _18524 = param_var_x_8;
    float _18525 = param_var_x_8;
    Interval _18522;
    _18522.lo = _18524;
    _18522.hi = _18525;
    Interval _18523 = _18522;
    Interval _18526 = _18523;
    Interval param_var_b_7 = _18526;
    Interval _18748 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _18748;
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
        Interval _20658;
        _20658.lo = param_var_lo;
        _20658.hi = param_var_hi;
        Interval _20659 = _20658;
        return _20659;
    }
    float n = floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5));
    Interval param_var_a = a;
    float param_var_x = spvFMul(n, 2.0);
    float _20655 = param_var_x;
    float _20656 = param_var_x;
    Interval _20653;
    _20653.lo = _20655;
    _20653.hi = _20656;
    Interval _20654 = _20653;
    Interval _20657 = _20654;
    Interval param_var_a_1 = _20657;
    float _20651 = 3.1415927410125732421875;
    float _20646 = _20651;
    float _20705 = interval_down(_20646, intervalFailed);
    float _20647 = _20705;
    float _20648 = _20651;
    float _20707 = interval_up(_20648, intervalFailed);
    float _20649 = _20707;
    Interval _20644;
    _20644.lo = _20647;
    _20644.hi = _20649;
    Interval _20645 = _20644;
    Interval _20650 = _20645;
    Interval _20652 = _20650;
    Interval param_var_b = _20652;
    Interval _20716 = imul(param_var_a_1, param_var_b, intervalFailed, optical_product_upper);
    Interval param_var_b_1 = _20716;
    Interval _20640 = param_var_a;
    Interval _20641 = param_var_b_1;
    float _20637 = as_type<float>(as_type<uint>(_20641.hi) ^ 2147483648u);
    float _20638 = as_type<float>(as_type<uint>(_20641.lo) ^ 2147483648u);
    Interval _20635;
    _20635.lo = _20637;
    _20635.hi = _20638;
    Interval _20636 = _20635;
    Interval _20639 = _20636;
    Interval _20642 = _20639;
    Interval _20736 = iadd(_20640, _20642, intervalFailed);
    Interval _20643 = _20736;
    Interval x = _20643;
    float _20633 = 3.1415927410125732421875;
    float _20628 = _20633;
    float _20739 = interval_down(_20628, intervalFailed);
    float _20629 = _20739;
    float _20630 = _20633;
    float _20741 = interval_up(_20630, intervalFailed);
    float _20631 = _20741;
    Interval _20626;
    _20626.lo = _20629;
    _20626.hi = _20631;
    Interval _20627 = _20626;
    Interval _20632 = _20627;
    Interval _20634 = _20632;
    Interval param_var_a_2 = _20634;
    float param_var_x_1 = 0.5;
    float _20623 = param_var_x_1;
    float _20624 = param_var_x_1;
    Interval _20621;
    _20621.lo = _20623;
    _20621.hi = _20624;
    Interval _20622 = _20621;
    Interval _20625 = _20622;
    Interval param_var_b_2 = _20625;
    Interval _20759 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _half = _20759;
    if (x.lo > _half.hi)
    {
        float _20619 = 3.1415927410125732421875;
        float _20614 = _20619;
        float _20766 = interval_down(_20614, intervalFailed);
        float _20615 = _20766;
        float _20616 = _20619;
        float _20768 = interval_up(_20616, intervalFailed);
        float _20617 = _20768;
        Interval _20612;
        _20612.lo = _20615;
        _20612.hi = _20617;
        Interval _20613 = _20612;
        Interval _20618 = _20613;
        Interval _20620 = _20618;
        Interval param_var_a_3 = _20620;
        Interval param_var_b_3 = x;
        Interval _20608 = param_var_a_3;
        Interval _20609 = param_var_b_3;
        float _20605 = as_type<float>(as_type<uint>(_20609.hi) ^ 2147483648u);
        float _20606 = as_type<float>(as_type<uint>(_20609.lo) ^ 2147483648u);
        Interval _20603;
        _20603.lo = _20605;
        _20603.hi = _20606;
        Interval _20604 = _20603;
        Interval _20607 = _20604;
        Interval _20610 = _20607;
        Interval _20797 = iadd(_20608, _20610, intervalFailed);
        Interval _20611 = _20797;
        x = _20611;
    }
    else
    {
        if (x.hi < (-_half.hi))
        {
            float _20601 = 3.1415927410125732421875;
            float _20596 = _20601;
            float _20805 = interval_down(_20596, intervalFailed);
            float _20597 = _20805;
            float _20598 = _20601;
            float _20807 = interval_up(_20598, intervalFailed);
            float _20599 = _20807;
            Interval _20594;
            _20594.lo = _20597;
            _20594.hi = _20599;
            Interval _20595 = _20594;
            Interval _20600 = _20595;
            Interval _20602 = _20600;
            Interval param_var_a_4 = _20602;
            float _20591 = as_type<float>(as_type<uint>(param_var_a_4.hi) ^ 2147483648u);
            float _20592 = as_type<float>(as_type<uint>(param_var_a_4.lo) ^ 2147483648u);
            Interval _20589;
            _20589.lo = _20591;
            _20589.hi = _20592;
            Interval _20590 = _20589;
            Interval _20593 = _20590;
            Interval param_var_a_5 = _20593;
            Interval param_var_b_4 = x;
            Interval _20585 = param_var_a_5;
            Interval _20586 = param_var_b_4;
            float _20582 = as_type<float>(as_type<uint>(_20586.hi) ^ 2147483648u);
            float _20583 = as_type<float>(as_type<uint>(_20586.lo) ^ 2147483648u);
            Interval _20580;
            _20580.lo = _20582;
            _20580.hi = _20583;
            Interval _20581 = _20580;
            Interval _20584 = _20581;
            Interval _20587 = _20584;
            Interval _20853 = iadd(_20585, _20587, intervalFailed);
            Interval _20588 = _20853;
            x = _20588;
        }
    }
    float _1901 = -_half.hi;
    bool temp_var_logical_2 = true;
    if ((isunordered(x.lo, _1901) || x.lo >= _1901))
    {
        temp_var_logical_2 = x.hi > _half.hi;
    }
    if (temp_var_logical_2)
    {
        float param_var_lo_1 = -1.0;
        float param_var_hi_1 = 1.0;
        Interval _20578;
        _20578.lo = param_var_lo_1;
        _20578.hi = param_var_hi_1;
        Interval _20579 = _20578;
        return _20579;
    }
    Interval param_var_a_6 = x;
    bool _20571 = false;
    if (param_var_a_6.lo <= 0.0)
    {
        _20571 = param_var_a_6.hi >= 0.0;
    }
    float _20570;
    if (_20571)
    {
        _20570 = 0.0;
    }
    else
    {
        _20570 = precise::min(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    }
    float _20569 = _20570;
    float _20572 = precise::max(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    float _20573 = spvFMul(_20569, _20569);
    float _20902 = interval_down(_20573, intervalFailed);
    float _20574 = precise::max(0.0, _20902);
    float _20575 = spvFMul(_20572, _20572);
    float _20906 = interval_up(_20575, intervalFailed);
    float _20576 = _20906;
    Interval _20567;
    _20567.lo = _20574;
    _20567.hi = _20576;
    Interval _20568 = _20567;
    Interval _20577 = _20568;
    Interval square = _20577;
    float param_var_x_2 = _2352[8];
    float _20562 = param_var_x_2;
    float _20917 = interval_down(_20562, intervalFailed);
    float _20563 = _20917;
    float _20564 = param_var_x_2;
    float _20919 = interval_up(_20564, intervalFailed);
    float _20565 = _20919;
    Interval _20560;
    _20560.lo = _20563;
    _20560.hi = _20565;
    Interval _20561 = _20560;
    Interval _20566 = _20561;
    Interval sum = _20566;
    Interval _20553;
    for (int k = 7; k >= 0; k--)
    {
        Interval param_var_a_7 = sum;
        Interval param_var_b_5 = square;
        Interval _20931 = imul(param_var_a_7, param_var_b_5, intervalFailed, optical_product_upper);
        Interval param_var_a_8 = _20931;
        float param_var_x_3 = _2352[k];
        float _20555 = param_var_x_3;
        float _20936 = interval_down(_20555, intervalFailed);
        float _20556 = _20936;
        float _20557 = param_var_x_3;
        float _20938 = interval_up(_20557, intervalFailed);
        float _20558 = _20938;
        _20553.lo = _20556;
        _20553.hi = _20558;
        Interval _20554 = _20553;
        Interval _20559 = _20554;
        Interval param_var_b_6 = _20559;
        Interval _20946 = iadd(param_var_a_8, param_var_b_6, intervalFailed);
        sum = _20946;
    }
    Interval param_var_a_9 = x;
    Interval param_var_b_7 = sum;
    Interval _20950 = imul(param_var_a_9, param_var_b_7, intervalFailed, optical_product_upper);
    Interval param_var_a_10 = _20950;
    float param_var_lo_2 = -3.9999999840167888010000751819462e-12;
    float param_var_hi_2 = 3.9999999840167888010000751819462e-12;
    Interval _20551;
    _20551.lo = param_var_lo_2;
    _20551.hi = param_var_hi_2;
    Interval _20552 = _20551;
    Interval param_var_b_8 = _20552;
    Interval _20957 = iadd(param_var_a_10, param_var_b_8, intervalFailed);
    Interval param_var_a_11 = _20957;
    float param_var_x_4 = -1.0;
    float _20548 = param_var_x_4;
    float _20549 = param_var_x_4;
    Interval _20546;
    _20546.lo = _20548;
    _20546.hi = _20549;
    Interval _20547 = _20546;
    Interval _20550 = _20547;
    Interval param_var_lo_3 = _20550;
    float param_var_x_5 = 1.0;
    float _20543 = param_var_x_5;
    float _20544 = param_var_x_5;
    Interval _20541;
    _20541.lo = _20543;
    _20541.hi = _20544;
    Interval _20542 = _20541;
    Interval _20545 = _20542;
    Interval param_var_hi_3 = _20545;
    Interval _20536 = param_var_a_11;
    Interval _20537 = param_var_lo_3;
    float _20533 = precise::max(_20536.lo, _20537.lo);
    float _20534 = precise::max(_20536.hi, _20537.hi);
    Interval _20531;
    _20531.lo = _20533;
    _20531.hi = _20534;
    Interval _20532 = _20531;
    Interval _20535 = _20532;
    Interval _20538 = _20535;
    Interval _20539 = param_var_hi_3;
    float _20528 = precise::min(_20538.lo, _20539.lo);
    float _20529 = precise::min(_20538.hi, _20539.hi);
    Interval _20526;
    _20526.lo = _20528;
    _20526.hi = _20529;
    Interval _20527 = _20526;
    Interval _20530 = _20527;
    Interval _20540 = _20530;
    return _20540;
}

static inline __attribute__((always_inline))
bool outside_face(thread const ReflectionSpecularPlane& p, thread const Interval3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = p;
    bool _19097 = false;
    if (param_var_p.a.w == 2.0)
    {
        _19097 = param_var_p.b.w == 2.0;
    }
    bool _19098 = false;
    if (_19097)
    {
        _19098 = param_var_p.c.w == 2.0;
    }
    bool _19099 = _19098;
    if (_19099)
    {
        return false;
    }
    float3 param_var_v = p.b.xyz;
    float _19090 = param_var_v.x;
    float _19087 = _19090;
    float _19088 = _19090;
    Interval _19085;
    _19085.lo = _19087;
    _19085.hi = _19088;
    Interval _19086 = _19085;
    Interval _19089 = _19086;
    Interval _19091 = _19089;
    float _19092 = param_var_v.y;
    float _19082 = _19092;
    float _19083 = _19092;
    Interval _19080;
    _19080.lo = _19082;
    _19080.hi = _19083;
    Interval _19081 = _19080;
    Interval _19084 = _19081;
    Interval _19093 = _19084;
    float _19094 = param_var_v.z;
    float _19077 = _19094;
    float _19078 = _19094;
    Interval _19075;
    _19075.lo = _19077;
    _19075.hi = _19078;
    Interval _19076 = _19075;
    Interval _19079 = _19076;
    Interval _19095 = _19079;
    Interval3 _19073;
    _19073.x = _19091;
    _19073.y = _19093;
    _19073.z = _19095;
    Interval3 _19074 = _19073;
    Interval3 _19096 = _19074;
    Interval3 param_var_a = _19096;
    float3 param_var_v_1 = p.a.xyz;
    float _19066 = param_var_v_1.x;
    float _19063 = _19066;
    float _19064 = _19066;
    Interval _19061;
    _19061.lo = _19063;
    _19061.hi = _19064;
    Interval _19062 = _19061;
    Interval _19065 = _19062;
    Interval _19067 = _19065;
    float _19068 = param_var_v_1.y;
    float _19058 = _19068;
    float _19059 = _19068;
    Interval _19056;
    _19056.lo = _19058;
    _19056.hi = _19059;
    Interval _19057 = _19056;
    Interval _19060 = _19057;
    Interval _19069 = _19060;
    float _19070 = param_var_v_1.z;
    float _19053 = _19070;
    float _19054 = _19070;
    Interval _19051;
    _19051.lo = _19053;
    _19051.hi = _19054;
    Interval _19052 = _19051;
    Interval _19055 = _19052;
    Interval _19071 = _19055;
    Interval3 _19049;
    _19049.x = _19067;
    _19049.y = _19069;
    _19049.z = _19071;
    Interval3 _19050 = _19049;
    Interval3 _19072 = _19050;
    Interval3 param_var_b = _19072;
    Interval3 _19040 = param_var_a;
    Interval _19041 = param_var_b.x;
    float _19037 = as_type<float>(as_type<uint>(_19041.hi) ^ 2147483648u);
    float _19038 = as_type<float>(as_type<uint>(_19041.lo) ^ 2147483648u);
    Interval _19035;
    _19035.lo = _19037;
    _19035.hi = _19038;
    Interval _19036 = _19035;
    Interval _19039 = _19036;
    Interval _19042 = _19039;
    Interval _19043 = param_var_b.y;
    float _19032 = as_type<float>(as_type<uint>(_19043.hi) ^ 2147483648u);
    float _19033 = as_type<float>(as_type<uint>(_19043.lo) ^ 2147483648u);
    Interval _19030;
    _19030.lo = _19032;
    _19030.hi = _19033;
    Interval _19031 = _19030;
    Interval _19034 = _19031;
    Interval _19044 = _19034;
    Interval _19045 = param_var_b.z;
    float _19027 = as_type<float>(as_type<uint>(_19045.hi) ^ 2147483648u);
    float _19028 = as_type<float>(as_type<uint>(_19045.lo) ^ 2147483648u);
    Interval _19025;
    _19025.lo = _19027;
    _19025.hi = _19028;
    Interval _19026 = _19025;
    Interval _19029 = _19026;
    Interval _19046 = _19029;
    Interval3 _19023;
    _19023.x = _19042;
    _19023.y = _19044;
    _19023.z = _19046;
    Interval3 _19024 = _19023;
    Interval3 _19047 = _19024;
    Interval _19013 = _19040.x;
    Interval _19014 = _19047.x;
    Interval _19280 = iadd(_19013, _19014, intervalFailed);
    Interval _19015 = _19280;
    Interval _19016 = _19040.y;
    Interval _19017 = _19047.y;
    Interval _19285 = iadd(_19016, _19017, intervalFailed);
    Interval _19018 = _19285;
    Interval _19019 = _19040.z;
    Interval _19020 = _19047.z;
    Interval _19290 = iadd(_19019, _19020, intervalFailed);
    Interval _19021 = _19290;
    Interval3 _19011;
    _19011.x = _19015;
    _19011.y = _19018;
    _19011.z = _19021;
    Interval3 _19012 = _19011;
    Interval3 _19022 = _19012;
    Interval3 _19048 = _19022;
    Interval3 u = _19048;
    float3 param_var_v_2 = p.c.xyz;
    float _19004 = param_var_v_2.x;
    float _19001 = _19004;
    float _19002 = _19004;
    Interval _18999;
    _18999.lo = _19001;
    _18999.hi = _19002;
    Interval _19000 = _18999;
    Interval _19003 = _19000;
    Interval _19005 = _19003;
    float _19006 = param_var_v_2.y;
    float _18996 = _19006;
    float _18997 = _19006;
    Interval _18994;
    _18994.lo = _18996;
    _18994.hi = _18997;
    Interval _18995 = _18994;
    Interval _18998 = _18995;
    Interval _19007 = _18998;
    float _19008 = param_var_v_2.z;
    float _18991 = _19008;
    float _18992 = _19008;
    Interval _18989;
    _18989.lo = _18991;
    _18989.hi = _18992;
    Interval _18990 = _18989;
    Interval _18993 = _18990;
    Interval _19009 = _18993;
    Interval3 _18987;
    _18987.x = _19005;
    _18987.y = _19007;
    _18987.z = _19009;
    Interval3 _18988 = _18987;
    Interval3 _19010 = _18988;
    Interval3 param_var_a_1 = _19010;
    float3 param_var_v_3 = p.a.xyz;
    float _18980 = param_var_v_3.x;
    float _18977 = _18980;
    float _18978 = _18980;
    Interval _18975;
    _18975.lo = _18977;
    _18975.hi = _18978;
    Interval _18976 = _18975;
    Interval _18979 = _18976;
    Interval _18981 = _18979;
    float _18982 = param_var_v_3.y;
    float _18972 = _18982;
    float _18973 = _18982;
    Interval _18970;
    _18970.lo = _18972;
    _18970.hi = _18973;
    Interval _18971 = _18970;
    Interval _18974 = _18971;
    Interval _18983 = _18974;
    float _18984 = param_var_v_3.z;
    float _18967 = _18984;
    float _18968 = _18984;
    Interval _18965;
    _18965.lo = _18967;
    _18965.hi = _18968;
    Interval _18966 = _18965;
    Interval _18969 = _18966;
    Interval _18985 = _18969;
    Interval3 _18963;
    _18963.x = _18981;
    _18963.y = _18983;
    _18963.z = _18985;
    Interval3 _18964 = _18963;
    Interval3 _18986 = _18964;
    Interval3 param_var_b_1 = _18986;
    Interval3 _18954 = param_var_a_1;
    Interval _18955 = param_var_b_1.x;
    float _18951 = as_type<float>(as_type<uint>(_18955.hi) ^ 2147483648u);
    float _18952 = as_type<float>(as_type<uint>(_18955.lo) ^ 2147483648u);
    Interval _18949;
    _18949.lo = _18951;
    _18949.hi = _18952;
    Interval _18950 = _18949;
    Interval _18953 = _18950;
    Interval _18956 = _18953;
    Interval _18957 = param_var_b_1.y;
    float _18946 = as_type<float>(as_type<uint>(_18957.hi) ^ 2147483648u);
    float _18947 = as_type<float>(as_type<uint>(_18957.lo) ^ 2147483648u);
    Interval _18944;
    _18944.lo = _18946;
    _18944.hi = _18947;
    Interval _18945 = _18944;
    Interval _18948 = _18945;
    Interval _18958 = _18948;
    Interval _18959 = param_var_b_1.z;
    float _18941 = as_type<float>(as_type<uint>(_18959.hi) ^ 2147483648u);
    float _18942 = as_type<float>(as_type<uint>(_18959.lo) ^ 2147483648u);
    Interval _18939;
    _18939.lo = _18941;
    _18939.hi = _18942;
    Interval _18940 = _18939;
    Interval _18943 = _18940;
    Interval _18960 = _18943;
    Interval3 _18937;
    _18937.x = _18956;
    _18937.y = _18958;
    _18937.z = _18960;
    Interval3 _18938 = _18937;
    Interval3 _18961 = _18938;
    Interval _18927 = _18954.x;
    Interval _18928 = _18961.x;
    Interval _19461 = iadd(_18927, _18928, intervalFailed);
    Interval _18929 = _19461;
    Interval _18930 = _18954.y;
    Interval _18931 = _18961.y;
    Interval _19466 = iadd(_18930, _18931, intervalFailed);
    Interval _18932 = _19466;
    Interval _18933 = _18954.z;
    Interval _18934 = _18961.z;
    Interval _19471 = iadd(_18933, _18934, intervalFailed);
    Interval _18935 = _19471;
    Interval3 _18925;
    _18925.x = _18929;
    _18925.y = _18932;
    _18925.z = _18935;
    Interval3 _18926 = _18925;
    Interval3 _18936 = _18926;
    Interval3 _18962 = _18936;
    Interval3 v = _18962;
    Interval3 param_var_a_2 = hit;
    float3 param_var_v_4 = p.a.xyz;
    float _18918 = param_var_v_4.x;
    float _18915 = _18918;
    float _18916 = _18918;
    Interval _18913;
    _18913.lo = _18915;
    _18913.hi = _18916;
    Interval _18914 = _18913;
    Interval _18917 = _18914;
    Interval _18919 = _18917;
    float _18920 = param_var_v_4.y;
    float _18910 = _18920;
    float _18911 = _18920;
    Interval _18908;
    _18908.lo = _18910;
    _18908.hi = _18911;
    Interval _18909 = _18908;
    Interval _18912 = _18909;
    Interval _18921 = _18912;
    float _18922 = param_var_v_4.z;
    float _18905 = _18922;
    float _18906 = _18922;
    Interval _18903;
    _18903.lo = _18905;
    _18903.hi = _18906;
    Interval _18904 = _18903;
    Interval _18907 = _18904;
    Interval _18923 = _18907;
    Interval3 _18901;
    _18901.x = _18919;
    _18901.y = _18921;
    _18901.z = _18923;
    Interval3 _18902 = _18901;
    Interval3 _18924 = _18902;
    Interval3 param_var_b_2 = _18924;
    Interval3 _18892 = param_var_a_2;
    Interval _18893 = param_var_b_2.x;
    float _18889 = as_type<float>(as_type<uint>(_18893.hi) ^ 2147483648u);
    float _18890 = as_type<float>(as_type<uint>(_18893.lo) ^ 2147483648u);
    Interval _18887;
    _18887.lo = _18889;
    _18887.hi = _18890;
    Interval _18888 = _18887;
    Interval _18891 = _18888;
    Interval _18894 = _18891;
    Interval _18895 = param_var_b_2.y;
    float _18884 = as_type<float>(as_type<uint>(_18895.hi) ^ 2147483648u);
    float _18885 = as_type<float>(as_type<uint>(_18895.lo) ^ 2147483648u);
    Interval _18882;
    _18882.lo = _18884;
    _18882.hi = _18885;
    Interval _18883 = _18882;
    Interval _18886 = _18883;
    Interval _18896 = _18886;
    Interval _18897 = param_var_b_2.z;
    float _18879 = as_type<float>(as_type<uint>(_18897.hi) ^ 2147483648u);
    float _18880 = as_type<float>(as_type<uint>(_18897.lo) ^ 2147483648u);
    Interval _18877;
    _18877.lo = _18879;
    _18877.hi = _18880;
    Interval _18878 = _18877;
    Interval _18881 = _18878;
    Interval _18898 = _18881;
    Interval3 _18875;
    _18875.x = _18894;
    _18875.y = _18896;
    _18875.z = _18898;
    Interval3 _18876 = _18875;
    Interval3 _18899 = _18876;
    Interval _18865 = _18892.x;
    Interval _18866 = _18899.x;
    Interval _19598 = iadd(_18865, _18866, intervalFailed);
    Interval _18867 = _19598;
    Interval _18868 = _18892.y;
    Interval _18869 = _18899.y;
    Interval _19603 = iadd(_18868, _18869, intervalFailed);
    Interval _18870 = _19603;
    Interval _18871 = _18892.z;
    Interval _18872 = _18899.z;
    Interval _19608 = iadd(_18871, _18872, intervalFailed);
    Interval _18873 = _19608;
    Interval3 _18863;
    _18863.x = _18867;
    _18863.y = _18870;
    _18863.z = _18873;
    Interval3 _18864 = _18863;
    Interval3 _18874 = _18864;
    Interval3 _18900 = _18874;
    Interval3 r = _18900;
    Interval3 param_var_a_3 = u;
    Interval3 param_var_b_3 = u;
    Interval _18852 = param_var_a_3.x;
    Interval _18853 = param_var_b_3.x;
    Interval _19625 = imul(_18852, _18853, intervalFailed, optical_product_upper);
    Interval _18854 = _19625;
    Interval _18855 = param_var_a_3.y;
    Interval _18856 = param_var_b_3.y;
    Interval _19630 = imul(_18855, _18856, intervalFailed, optical_product_upper);
    Interval _18857 = _19630;
    Interval _19631 = iadd(_18854, _18857, intervalFailed);
    Interval _18858 = _19631;
    Interval _18859 = param_var_a_3.z;
    Interval _18860 = param_var_b_3.z;
    Interval _19636 = imul(_18859, _18860, intervalFailed, optical_product_upper);
    Interval _18861 = _19636;
    Interval _19637 = iadd(_18858, _18861, intervalFailed);
    Interval _18862 = _19637;
    Interval aa = _18862;
    Interval3 param_var_a_4 = u;
    Interval3 param_var_b_4 = v;
    Interval _18841 = param_var_a_4.x;
    Interval _18842 = param_var_b_4.x;
    Interval _19645 = imul(_18841, _18842, intervalFailed, optical_product_upper);
    Interval _18843 = _19645;
    Interval _18844 = param_var_a_4.y;
    Interval _18845 = param_var_b_4.y;
    Interval _19650 = imul(_18844, _18845, intervalFailed, optical_product_upper);
    Interval _18846 = _19650;
    Interval _19651 = iadd(_18843, _18846, intervalFailed);
    Interval _18847 = _19651;
    Interval _18848 = param_var_a_4.z;
    Interval _18849 = param_var_b_4.z;
    Interval _19656 = imul(_18848, _18849, intervalFailed, optical_product_upper);
    Interval _18850 = _19656;
    Interval _19657 = iadd(_18847, _18850, intervalFailed);
    Interval _18851 = _19657;
    Interval ab = _18851;
    Interval3 param_var_a_5 = v;
    Interval3 param_var_b_5 = v;
    Interval _18830 = param_var_a_5.x;
    Interval _18831 = param_var_b_5.x;
    Interval _19665 = imul(_18830, _18831, intervalFailed, optical_product_upper);
    Interval _18832 = _19665;
    Interval _18833 = param_var_a_5.y;
    Interval _18834 = param_var_b_5.y;
    Interval _19670 = imul(_18833, _18834, intervalFailed, optical_product_upper);
    Interval _18835 = _19670;
    Interval _19671 = iadd(_18832, _18835, intervalFailed);
    Interval _18836 = _19671;
    Interval _18837 = param_var_a_5.z;
    Interval _18838 = param_var_b_5.z;
    Interval _19676 = imul(_18837, _18838, intervalFailed, optical_product_upper);
    Interval _18839 = _19676;
    Interval _19677 = iadd(_18836, _18839, intervalFailed);
    Interval _18840 = _19677;
    Interval bb = _18840;
    Interval3 param_var_a_6 = r;
    Interval3 param_var_b_6 = u;
    Interval _18819 = param_var_a_6.x;
    Interval _18820 = param_var_b_6.x;
    Interval _19685 = imul(_18819, _18820, intervalFailed, optical_product_upper);
    Interval _18821 = _19685;
    Interval _18822 = param_var_a_6.y;
    Interval _18823 = param_var_b_6.y;
    Interval _19690 = imul(_18822, _18823, intervalFailed, optical_product_upper);
    Interval _18824 = _19690;
    Interval _19691 = iadd(_18821, _18824, intervalFailed);
    Interval _18825 = _19691;
    Interval _18826 = param_var_a_6.z;
    Interval _18827 = param_var_b_6.z;
    Interval _19696 = imul(_18826, _18827, intervalFailed, optical_product_upper);
    Interval _18828 = _19696;
    Interval _19697 = iadd(_18825, _18828, intervalFailed);
    Interval _18829 = _19697;
    Interval ra = _18829;
    Interval3 param_var_a_7 = r;
    Interval3 param_var_b_7 = v;
    Interval _18808 = param_var_a_7.x;
    Interval _18809 = param_var_b_7.x;
    Interval _19705 = imul(_18808, _18809, intervalFailed, optical_product_upper);
    Interval _18810 = _19705;
    Interval _18811 = param_var_a_7.y;
    Interval _18812 = param_var_b_7.y;
    Interval _19710 = imul(_18811, _18812, intervalFailed, optical_product_upper);
    Interval _18813 = _19710;
    Interval _19711 = iadd(_18810, _18813, intervalFailed);
    Interval _18814 = _19711;
    Interval _18815 = param_var_a_7.z;
    Interval _18816 = param_var_b_7.z;
    Interval _19716 = imul(_18815, _18816, intervalFailed, optical_product_upper);
    Interval _18817 = _19716;
    Interval _19717 = iadd(_18814, _18817, intervalFailed);
    Interval _18818 = _19717;
    Interval rb = _18818;
    Interval param_var_a_8 = aa;
    Interval param_var_b_8 = bb;
    Interval _19721 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _19721;
    Interval param_var_a_10 = ab;
    bool _18801 = false;
    if (param_var_a_10.lo <= 0.0)
    {
        _18801 = param_var_a_10.hi >= 0.0;
    }
    float _18800;
    if (_18801)
    {
        _18800 = 0.0;
    }
    else
    {
        _18800 = precise::min(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    }
    float _18799 = _18800;
    float _18802 = precise::max(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    float _18803 = spvFMul(_18799, _18799);
    float _19752 = interval_down(_18803, intervalFailed);
    float _18804 = precise::max(0.0, _19752);
    float _18805 = spvFMul(_18802, _18802);
    float _19756 = interval_up(_18805, intervalFailed);
    float _18806 = _19756;
    Interval _18797;
    _18797.lo = _18804;
    _18797.hi = _18806;
    Interval _18798 = _18797;
    Interval _18807 = _18798;
    Interval param_var_b_9 = _18807;
    Interval _18793 = param_var_a_9;
    Interval _18794 = param_var_b_9;
    float _18790 = as_type<float>(as_type<uint>(_18794.hi) ^ 2147483648u);
    float _18791 = as_type<float>(as_type<uint>(_18794.lo) ^ 2147483648u);
    Interval _18788;
    _18788.lo = _18790;
    _18788.hi = _18791;
    Interval _18789 = _18788;
    Interval _18792 = _18789;
    Interval _18795 = _18792;
    Interval _19783 = iadd(_18793, _18795, intervalFailed);
    Interval _18796 = _19783;
    Interval det = _18796;
    if (det.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_11 = bb;
    Interval param_var_b_10 = ra;
    Interval _19790 = imul(param_var_a_11, param_var_b_10, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _19790;
    Interval param_var_a_13 = ab;
    Interval param_var_b_11 = rb;
    Interval _19793 = imul(param_var_a_13, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_b_12 = _19793;
    Interval _18784 = param_var_a_12;
    Interval _18785 = param_var_b_12;
    float _18781 = as_type<float>(as_type<uint>(_18785.hi) ^ 2147483648u);
    float _18782 = as_type<float>(as_type<uint>(_18785.lo) ^ 2147483648u);
    Interval _18779;
    _18779.lo = _18781;
    _18779.hi = _18782;
    Interval _18780 = _18779;
    Interval _18783 = _18780;
    Interval _18786 = _18783;
    Interval _19813 = iadd(_18784, _18786, intervalFailed);
    Interval _18787 = _19813;
    Interval param_var_a_14 = _18787;
    Interval param_var_b_13 = det;
    Interval _19816 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
    Interval x = _19816;
    Interval param_var_a_15 = aa;
    Interval param_var_b_14 = rb;
    Interval _19819 = imul(param_var_a_15, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_16 = _19819;
    Interval param_var_a_17 = ab;
    Interval param_var_b_15 = ra;
    Interval _19822 = imul(param_var_a_17, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _19822;
    Interval _18775 = param_var_a_16;
    Interval _18776 = param_var_b_16;
    float _18772 = as_type<float>(as_type<uint>(_18776.hi) ^ 2147483648u);
    float _18773 = as_type<float>(as_type<uint>(_18776.lo) ^ 2147483648u);
    Interval _18770;
    _18770.lo = _18772;
    _18770.hi = _18773;
    Interval _18771 = _18770;
    Interval _18774 = _18771;
    Interval _18777 = _18774;
    Interval _19842 = iadd(_18775, _18777, intervalFailed);
    Interval _18778 = _19842;
    Interval param_var_a_18 = _18778;
    Interval param_var_b_17 = det;
    Interval _19845 = idiv(param_var_a_18, param_var_b_17, intervalFailed, interval_divide_upper);
    Interval y = _19845;
    float param_var_x = -9.9999997473787516355514526367188e-06;
    float _18765 = param_var_x;
    float _19849 = interval_down(_18765, intervalFailed);
    float _18766 = _19849;
    float _18767 = param_var_x;
    float _19851 = interval_up(_18767, intervalFailed);
    float _18768 = _19851;
    Interval _18763;
    _18763.lo = _18766;
    _18763.hi = _18768;
    Interval _18764 = _18763;
    Interval _18769 = _18764;
    Interval temp_var_Interval = _18769;
    bool temp_var_logical = true;
    if ((isunordered(x.hi, temp_var_Interval.lo) || x.hi >= temp_var_Interval.lo))
    {
        float param_var_x_1 = -9.9999997473787516355514526367188e-06;
        float _18758 = param_var_x_1;
        float _19865 = interval_down(_18758, intervalFailed);
        float _18759 = _19865;
        float _18760 = param_var_x_1;
        float _19867 = interval_up(_18760, intervalFailed);
        float _18761 = _19867;
        Interval _18756;
        _18756.lo = _18759;
        _18756.hi = _18761;
        Interval _18757 = _18756;
        Interval _18762 = _18757;
        Interval temp_var_Interval_1 = _18762;
        temp_var_logical = y.hi < temp_var_Interval_1.lo;
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        Interval param_var_a_19 = x;
        Interval param_var_b_18 = y;
        Interval _19882 = iadd(param_var_a_19, param_var_b_18, intervalFailed);
        Interval temp_var_Interval_2 = _19882;
        float param_var_x_2 = 1.000010013580322265625;
        float _18751 = param_var_x_2;
        float _19886 = interval_down(_18751, intervalFailed);
        float _18752 = _19886;
        float _18753 = param_var_x_2;
        float _19888 = interval_up(_18753, intervalFailed);
        float _18754 = _19888;
        Interval _18749;
        _18749.lo = _18752;
        _18749.hi = _18754;
        Interval _18750 = _18749;
        Interval _18755 = _18750;
        Interval temp_var_Interval_3 = _18755;
        temp_var_logical_1 = temp_var_Interval_2.lo > temp_var_Interval_3.hi;
    }
    return temp_var_logical_1;
}

static inline __attribute__((always_inline))
bool optical_excluded_target(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTerminal, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper)
{
    float param_var_lo = box.x;
    float param_var_hi = box.z;
    Interval _8980;
    _8980.lo = param_var_lo;
    _8980.hi = param_var_hi;
    Interval _8981 = _8980;
    Interval param_var_a = _8981;
    float param_var_x = receiver.projection.z;
    float _8977 = param_var_x;
    float _8978 = param_var_x;
    Interval _8975;
    _8975.lo = _8977;
    _8975.hi = _8978;
    Interval _8976 = _8975;
    Interval _8979 = _8976;
    Interval param_var_b = _8979;
    Interval _8971 = param_var_a;
    Interval _8972 = param_var_b;
    float _8968 = as_type<float>(as_type<uint>(_8972.hi) ^ 2147483648u);
    float _8969 = as_type<float>(as_type<uint>(_8972.lo) ^ 2147483648u);
    Interval _8966;
    _8966.lo = _8968;
    _8966.hi = _8969;
    Interval _8967 = _8966;
    Interval _8970 = _8967;
    Interval _8973 = _8970;
    Interval _9023 = iadd(_8971, _8973, intervalFailed);
    Interval _8974 = _9023;
    Interval param_var_a_1 = _8974;
    float param_var_x_1 = receiver.projection.x;
    float _8963 = param_var_x_1;
    float _8964 = param_var_x_1;
    Interval _8961;
    _8961.lo = _8963;
    _8961.hi = _8964;
    Interval _8962 = _8961;
    Interval _8965 = _8962;
    Interval param_var_b_1 = _8965;
    Interval _9037 = idiv(param_var_a_1, param_var_b_1, intervalFailed, interval_divide_upper);
    Interval param_var_x_2 = _9037;
    float param_var_lo_1 = box.y;
    float param_var_hi_1 = box.w;
    Interval _8959;
    _8959.lo = param_var_lo_1;
    _8959.hi = param_var_hi_1;
    Interval _8960 = _8959;
    Interval param_var_a_2 = _8960;
    float param_var_x_3 = receiver.projection.w;
    float _8956 = param_var_x_3;
    float _8957 = param_var_x_3;
    Interval _8954;
    _8954.lo = _8956;
    _8954.hi = _8957;
    Interval _8955 = _8954;
    Interval _8958 = _8955;
    Interval param_var_b_2 = _8958;
    Interval _8950 = param_var_a_2;
    Interval _8951 = param_var_b_2;
    float _8947 = as_type<float>(as_type<uint>(_8951.hi) ^ 2147483648u);
    float _8948 = as_type<float>(as_type<uint>(_8951.lo) ^ 2147483648u);
    Interval _8945;
    _8945.lo = _8947;
    _8945.hi = _8948;
    Interval _8946 = _8945;
    Interval _8949 = _8946;
    Interval _8952 = _8949;
    Interval _9079 = iadd(_8950, _8952, intervalFailed);
    Interval _8953 = _9079;
    Interval param_var_a_3 = _8953;
    float param_var_x_4 = receiver.projection.y;
    float _8942 = param_var_x_4;
    float _8943 = param_var_x_4;
    Interval _8940;
    _8940.lo = _8942;
    _8940.hi = _8943;
    Interval _8941 = _8940;
    Interval _8944 = _8941;
    Interval param_var_b_3 = _8944;
    Interval _9093 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_y = _9093;
    float param_var_x_5 = 1.0;
    float _8937 = param_var_x_5;
    float _8938 = param_var_x_5;
    Interval _8935;
    _8935.lo = _8937;
    _8935.hi = _8938;
    Interval _8936 = _8935;
    Interval _8939 = _8936;
    Interval param_var_z = _8939;
    Interval3 _8933;
    _8933.x = param_var_x_2;
    _8933.y = param_var_y;
    _8933.z = param_var_z;
    Interval3 _8934 = _8933;
    Interval3 param_var_a_4 = _8934;
    Interval3 _8926 = param_var_a_4;
    float _8927 = 1.0;
    float _8923 = _8927;
    float _8924 = _8927;
    Interval _8921;
    _8921.lo = _8923;
    _8921.hi = _8924;
    Interval _8922 = _8921;
    Interval _8925 = _8922;
    Interval _8928 = _8925;
    Interval3 _8929 = param_var_a_4;
    Interval _8912 = _8929.x;
    bool _8905 = false;
    if (_8912.lo <= 0.0)
    {
        _8905 = _8912.hi >= 0.0;
    }
    float _8904;
    if (_8905)
    {
        _8904 = 0.0;
    }
    else
    {
        _8904 = precise::min(abs(_8912.lo), abs(_8912.hi));
    }
    float _8903 = _8904;
    float _8906 = precise::max(abs(_8912.lo), abs(_8912.hi));
    float _8907 = spvFMul(_8903, _8903);
    float _9153 = interval_down(_8907, intervalFailed);
    float _8908 = precise::max(0.0, _9153);
    float _8909 = spvFMul(_8906, _8906);
    float _9157 = interval_up(_8909, intervalFailed);
    float _8910 = _9157;
    Interval _8901;
    _8901.lo = _8908;
    _8901.hi = _8910;
    Interval _8902 = _8901;
    Interval _8911 = _8902;
    Interval _8913 = _8911;
    Interval _8914 = _8929.y;
    bool _8894 = false;
    if (_8914.lo <= 0.0)
    {
        _8894 = _8914.hi >= 0.0;
    }
    float _8893;
    if (_8894)
    {
        _8893 = 0.0;
    }
    else
    {
        _8893 = precise::min(abs(_8914.lo), abs(_8914.hi));
    }
    float _8892 = _8893;
    float _8895 = precise::max(abs(_8914.lo), abs(_8914.hi));
    float _8896 = spvFMul(_8892, _8892);
    float _9196 = interval_down(_8896, intervalFailed);
    float _8897 = precise::max(0.0, _9196);
    float _8898 = spvFMul(_8895, _8895);
    float _9200 = interval_up(_8898, intervalFailed);
    float _8899 = _9200;
    Interval _8890;
    _8890.lo = _8897;
    _8890.hi = _8899;
    Interval _8891 = _8890;
    Interval _8900 = _8891;
    Interval _8915 = _8900;
    Interval _9208 = iadd(_8913, _8915, intervalFailed);
    Interval _8916 = _9208;
    Interval _8917 = _8929.z;
    bool _8883 = false;
    if (_8917.lo <= 0.0)
    {
        _8883 = _8917.hi >= 0.0;
    }
    float _8882;
    if (_8883)
    {
        _8882 = 0.0;
    }
    else
    {
        _8882 = precise::min(abs(_8917.lo), abs(_8917.hi));
    }
    float _8881 = _8882;
    float _8884 = precise::max(abs(_8917.lo), abs(_8917.hi));
    float _8885 = spvFMul(_8881, _8881);
    float _9240 = interval_down(_8885, intervalFailed);
    float _8886 = precise::max(0.0, _9240);
    float _8887 = spvFMul(_8884, _8884);
    float _9244 = interval_up(_8887, intervalFailed);
    float _8888 = _9244;
    Interval _8879;
    _8879.lo = _8886;
    _8879.hi = _8888;
    Interval _8880 = _8879;
    Interval _8889 = _8880;
    Interval _8918 = _8889;
    Interval _9252 = iadd(_8916, _8918, intervalFailed);
    Interval _8919 = _9252;
    Interval _9253 = isqrt(_8919, intervalFailed);
    Interval _8920 = _9253;
    Interval _8930 = _8920;
    Interval _9255 = idiv(_8928, _8930, intervalFailed, interval_divide_upper);
    Interval _8931 = _9255;
    Interval _8869 = _8926.x;
    Interval _8870 = _8931;
    Interval _9259 = imul(_8869, _8870, intervalFailed, optical_product_upper);
    Interval _8871 = _9259;
    Interval _8872 = _8926.y;
    Interval _8873 = _8931;
    Interval _9263 = imul(_8872, _8873, intervalFailed, optical_product_upper);
    Interval _8874 = _9263;
    Interval _8875 = _8926.z;
    Interval _8876 = _8931;
    Interval _9267 = imul(_8875, _8876, intervalFailed, optical_product_upper);
    Interval _8877 = _9267;
    Interval3 _8867;
    _8867.x = _8871;
    _8867.y = _8874;
    _8867.z = _8877;
    Interval3 _8868 = _8867;
    Interval3 _8878 = _8868;
    Interval3 _8932 = _8878;
    Interval3 outgoing = _8932;
    float3 param_var_v = float3(0.0);
    float _8860 = param_var_v.x;
    float _8857 = _8860;
    float _8858 = _8860;
    Interval _8855;
    _8855.lo = _8857;
    _8855.hi = _8858;
    Interval _8856 = _8855;
    Interval _8859 = _8856;
    Interval _8861 = _8859;
    float _8862 = param_var_v.y;
    float _8852 = _8862;
    float _8853 = _8862;
    Interval _8850;
    _8850.lo = _8852;
    _8850.hi = _8853;
    Interval _8851 = _8850;
    Interval _8854 = _8851;
    Interval _8863 = _8854;
    float _8864 = param_var_v.z;
    float _8847 = _8864;
    float _8848 = _8864;
    Interval _8845;
    _8845.lo = _8847;
    _8845.hi = _8848;
    Interval _8846 = _8845;
    Interval _8849 = _8846;
    Interval _8865 = _8849;
    Interval3 _8843;
    _8843.x = _8861;
    _8843.y = _8863;
    _8843.z = _8865;
    Interval3 _8844 = _8843;
    Interval3 _8866 = _8844;
    Interval3 origin = _8866;
    float param_var_x_6 = 0.0;
    float _8840 = param_var_x_6;
    float _8841 = param_var_x_6;
    Interval _8838;
    _8838.lo = _8840;
    _8838.hi = _8841;
    Interval _8839 = _8838;
    Interval _8842 = _8839;
    Interval bias0 = _8842;
    bool temp_var_ternary;
    ReflectionSpecularPlane plane;
    Interval3 n;
    float3 temp_var_ternary_1;
    Interval radius;
    int temp_var_ternary_2;
    Interval3 t;
    Interval3 _5462;
    Interval _5464;
    Interval _5469;
    Interval _5474;
    Interval3 _5500;
    Interval _5512;
    float _5515;
    Interval _5523;
    float _5526;
    Interval _5534;
    float _5537;
    Interval _5554;
    Interval3 _5566;
    Interval3 _5578;
    Interval _5590;
    float _5593;
    Interval _5601;
    Interval3 _5606;
    Interval3 _5618;
    Interval3 _5630;
    Interval _5632;
    Interval _5641;
    Interval _5650;
    Interval3 _5681;
    Interval3 _5693;
    Interval _5705;
    float _5708;
    Interval _5716;
    float _5719;
    Interval _5727;
    float _5730;
    Interval _5747;
    Interval3 _5759;
    Interval _5761;
    Interval _5770;
    Interval _5779;
    Interval3 _5810;
    Interval _5812;
    Interval _5817;
    Interval _5822;
    Interval3 _5834;
    Interval _5846;
    float _5849;
    Interval _5857;
    float _5860;
    Interval _5868;
    float _5871;
    Interval _5888;
    Interval3 _5900;
    Interval _5902;
    Interval _5911;
    Interval _5920;
    Interval3 _5951;
    Interval _5953;
    Interval _5958;
    Interval _5963;
    Interval _5975;
    float _5977;
    Interval3 _5982;
    Interval3 _5994;
    Interval _5996;
    Interval _6001;
    Interval _6006;
    Interval3 _6020;
    Interval _6043;
    Interval3 _6058;
    Interval3 _6070;
    Interval _6082;
    Interval3 _6090;
    Interval _6102;
    float _6105;
    Interval _6113;
    float _6116;
    Interval _6124;
    float _6127;
    Interval _6144;
    Interval3 _6156;
    Interval3 _6169;
    Interval _6171;
    Interval _6176;
    Interval _6181;
    Interval3 _6204;
    Interval _6206;
    Interval _6211;
    Interval _6216;
    Interval3 _6239;
    Interval _6241;
    Interval _6246;
    Interval _6251;
    Interval3 _6276;
    Interval _6278;
    Interval _6283;
    Interval _6292;
    float _6295;
    Interval _6303;
    Interval _6308;
    Interval _6313;
    float _6316;
    Interval _6336;
    Interval _6338;
    Interval _6351;
    Interval _6356;
    Interval _6372;
    float _6375;
    Interval _6383;
    Interval _6388;
    Interval _6393;
    float _6396;
    Interval _6416;
    Interval _6418;
    Interval _6431;
    Interval _6436;
    Interval _6452;
    float _6455;
    Interval _6463;
    Interval _6468;
    Interval _6473;
    float _6476;
    Interval _6496;
    Interval _6498;
    Interval _6511;
    Interval _6516;
    Interval _6532;
    float _6535;
    Interval _6543;
    Interval _6548;
    Interval _6553;
    float _6556;
    Interval _6576;
    Interval _6578;
    Interval _6591;
    Interval _6596;
    Interval _6612;
    Interval _6621;
    Interval _6630;
    Interval _6639;
    Interval3 _6648;
    Interval3 _6660;
    Interval3 _6672;
    Interval3 _6684;
    Interval _6696;
    Interval _6701;
    Interval _6711;
    Interval _6716;
    Interval _6721;
    Interval3 _6726;
    Interval _6728;
    Interval _6733;
    Interval _6738;
    Interval _6743;
    Interval _6748;
    Interval _6753;
    Interval _6758;
    Interval _6763;
    Interval _6768;
    float _6771;
    Interval _6779;
    float _6782;
    Interval _6790;
    Interval _6795;
    Interval _6800;
    float _6803;
    Interval _6823;
    Interval _6828;
    Interval _6833;
    Interval _6843;
    Interval _6848;
    Interval _6853;
    Interval _6862;
    float _6865;
    Interval _6873;
    float _6876;
    Interval _6884;
    Interval _6889;
    float _6892;
    Interval _6900;
    float _6903;
    Interval _6911;
    Interval _6913;
    Interval _6926;
    Interval _6933;
    Interval _6935;
    Interval _6944;
    Interval _6949;
    Interval _6954;
    Interval _6967;
    Interval _6976;
    Interval _6981;
    Interval _6994;
    Interval _7003;
    Interval _7012;
    Interval _7014;
    Interval _7016;
    Interval _7018;
    Interval _7023;
    Interval _7028;
    Interval _7030;
    Interval _7043;
    Interval _7048;
    Interval _7064;
    Interval _7066;
    Interval _7079;
    Interval _7084;
    Interval _7100;
    Interval _7102;
    Interval _7115;
    float _7118;
    Interval _7126;
    Interval _7131;
    Interval _7136;
    float _7139;
    Interval _7159;
    Interval _7168;
    Interval _7170;
    Interval _7183;
    Interval _7188;
    Interval _7204;
    Interval _7213;
    Interval _7215;
    Interval _7228;
    Interval _7233;
    Interval _7249;
    Interval _7251;
    Interval _7264;
    Interval _7269;
    Interval _7285;
    Interval _7294;
    Interval _7299;
    Interval _7308;
    Interval _7310;
    Interval _7323;
    Interval _7328;
    Interval _7344;
    Interval _7346;
    Interval _7359;
    Interval _7364;
    Interval _7380;
    Interval _7382;
    Interval _7395;
    Interval _7400;
    Interval _7416;
    Interval _7421;
    Interval _7423;
    Interval _7436;
    Interval _7441;
    Interval _7457;
    Interval _7459;
    Interval _7472;
    Interval _7477;
    Interval _7479;
    Interval _7492;
    Interval _7497;
    Interval _7499;
    Interval _7512;
    Interval _7517;
    float _7520;
    Interval _7528;
    Interval _7533;
    Interval _7538;
    float _7541;
    Interval _7561;
    float _7564;
    Interval _7572;
    Interval _7577;
    Interval _7582;
    float _7585;
    Interval _7605;
    float _7608;
    Interval _7616;
    Interval _7621;
    Interval _7626;
    float _7629;
    Interval _7649;
    Interval _7658;
    Interval _7667;
    Interval _7676;
    Interval _7678;
    Interval _7691;
    Interval _7700;
    float _7703;
    Interval _7711;
    Interval _7716;
    Interval _7721;
    float _7724;
    Interval _7744;
    Interval3 _8155;
    Interval3 _8167;
    Interval3 _8179;
    Interval _8181;
    Interval _8186;
    Interval _8191;
    Interval3 _8203;
    Interval3 _8215;
    Interval3 _8227;
    Interval _8229;
    Interval _8234;
    Interval _8239;
    Interval3 _8251;
    Interval3 _8263;
    Interval _8265;
    Interval _8270;
    Interval _8275;
    Interval _8301;
    Interval _8306;
    Interval3 _8311;
    Interval3 _8323;
    Interval _8325;
    Interval _8330;
    Interval _8335;
    Interval3 _8347;
    Interval3 _8359;
    Interval3 _8371;
    Interval _8373;
    Interval _8378;
    Interval _8383;
    Interval3 _8395;
    Interval3 _8407;
    Interval3 _8419;
    Interval _8421;
    Interval _8426;
    Interval _8431;
    Interval3 _8443;
    Interval3 _8455;
    Interval _8457;
    Interval _8462;
    Interval _8467;
    Interval _8501;
    Interval _8502;
    Interval _8647;
    Interval _8652;
    float _8654;
    Interval _8659;
    Interval _8664;
    float _8667;
    Interval _8675;
    float _8678;
    Interval _8686;
    float _8689;
    Interval3 _8706;
    Interval3 _8718;
    Interval3 _8741;
    Interval3 _8753;
    Interval _8755;
    Interval _8760;
    Interval _8765;
    Interval3 _8779;
    Interval _8781;
    Interval _8786;
    Interval _8791;
    Interval3 _8814;
    Interval _8816;
    Interval _8821;
    Interval _8826;
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
            float _8831 = param_var_v_1.x;
            float _8828 = _8831;
            float _8829 = _8831;
            _8826.lo = _8828;
            _8826.hi = _8829;
            Interval _8827 = _8826;
            Interval _8830 = _8827;
            Interval _8832 = _8830;
            float _8833 = param_var_v_1.y;
            float _8823 = _8833;
            float _8824 = _8833;
            _8821.lo = _8823;
            _8821.hi = _8824;
            Interval _8822 = _8821;
            Interval _8825 = _8822;
            Interval _8834 = _8825;
            float _8835 = param_var_v_1.z;
            float _8818 = _8835;
            float _8819 = _8835;
            _8816.lo = _8818;
            _8816.hi = _8819;
            Interval _8817 = _8816;
            Interval _8820 = _8817;
            Interval _8836 = _8820;
            _8814.x = _8832;
            _8814.y = _8834;
            _8814.z = _8836;
            Interval3 _8815 = _8814;
            Interval3 _8837 = _8815;
            n = _8837;
        }
        else
        {
            ReflectionSpecularPlane param_var_p = plane;
            Interval3 _9407 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval3 param_var_n = _9407;
            Interval3 param_var_direction = outgoing;
            Interval3 _9409 = oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper);
            n = _9409;
        }
        Interval3 param_var_a_5 = outgoing;
        Interval3 param_var_b_4 = n;
        Interval _8803 = param_var_a_5.x;
        Interval _8804 = param_var_b_4.x;
        Interval _9416 = imul(_8803, _8804, intervalFailed, optical_product_upper);
        Interval _8805 = _9416;
        Interval _8806 = param_var_a_5.y;
        Interval _8807 = param_var_b_4.y;
        Interval _9421 = imul(_8806, _8807, intervalFailed, optical_product_upper);
        Interval _8808 = _9421;
        Interval _9422 = iadd(_8805, _8808, intervalFailed);
        Interval _8809 = _9422;
        Interval _8810 = param_var_a_5.z;
        Interval _8811 = param_var_b_4.z;
        Interval _9427 = imul(_8810, _8811, intervalFailed, optical_product_upper);
        Interval _8812 = _9427;
        Interval _9428 = iadd(_8809, _8812, intervalFailed);
        Interval _8813 = _9428;
        Interval denominator = _8813;
        if (curved)
        {
            temp_var_ternary_1 = liquid.planePoint.xyz;
        }
        else
        {
            temp_var_ternary_1 = plane.a.xyz;
        }
        float3 param_var_v_2 = temp_var_ternary_1;
        float _8796 = param_var_v_2.x;
        float _8793 = _8796;
        float _8794 = _8796;
        _8791.lo = _8793;
        _8791.hi = _8794;
        Interval _8792 = _8791;
        Interval _8795 = _8792;
        Interval _8797 = _8795;
        float _8798 = param_var_v_2.y;
        float _8788 = _8798;
        float _8789 = _8798;
        _8786.lo = _8788;
        _8786.hi = _8789;
        Interval _8787 = _8786;
        Interval _8790 = _8787;
        Interval _8799 = _8790;
        float _8800 = param_var_v_2.z;
        float _8783 = _8800;
        float _8784 = _8800;
        _8781.lo = _8783;
        _8781.hi = _8784;
        Interval _8782 = _8781;
        Interval _8785 = _8782;
        Interval _8801 = _8785;
        _8779.x = _8797;
        _8779.y = _8799;
        _8779.z = _8801;
        Interval3 _8780 = _8779;
        Interval3 _8802 = _8780;
        Interval3 param_var_a_6 = _8802;
        Interval3 param_var_b_5 = origin;
        Interval3 _8770 = param_var_a_6;
        Interval _8771 = param_var_b_5.x;
        float _8767 = as_type<float>(as_type<uint>(_8771.hi) ^ 2147483648u);
        float _8768 = as_type<float>(as_type<uint>(_8771.lo) ^ 2147483648u);
        _8765.lo = _8767;
        _8765.hi = _8768;
        Interval _8766 = _8765;
        Interval _8769 = _8766;
        Interval _8772 = _8769;
        Interval _8773 = param_var_b_5.y;
        float _8762 = as_type<float>(as_type<uint>(_8773.hi) ^ 2147483648u);
        float _8763 = as_type<float>(as_type<uint>(_8773.lo) ^ 2147483648u);
        _8760.lo = _8762;
        _8760.hi = _8763;
        Interval _8761 = _8760;
        Interval _8764 = _8761;
        Interval _8774 = _8764;
        Interval _8775 = param_var_b_5.z;
        float _8757 = as_type<float>(as_type<uint>(_8775.hi) ^ 2147483648u);
        float _8758 = as_type<float>(as_type<uint>(_8775.lo) ^ 2147483648u);
        _8755.lo = _8757;
        _8755.hi = _8758;
        Interval _8756 = _8755;
        Interval _8759 = _8756;
        Interval _8776 = _8759;
        _8753.x = _8772;
        _8753.y = _8774;
        _8753.z = _8776;
        Interval3 _8754 = _8753;
        Interval3 _8777 = _8754;
        Interval _8743 = _8770.x;
        Interval _8744 = _8777.x;
        Interval _9551 = iadd(_8743, _8744, intervalFailed);
        Interval _8745 = _9551;
        Interval _8746 = _8770.y;
        Interval _8747 = _8777.y;
        Interval _9556 = iadd(_8746, _8747, intervalFailed);
        Interval _8748 = _9556;
        Interval _8749 = _8770.z;
        Interval _8750 = _8777.z;
        Interval _9561 = iadd(_8749, _8750, intervalFailed);
        Interval _8751 = _9561;
        _8741.x = _8745;
        _8741.y = _8748;
        _8741.z = _8751;
        Interval3 _8742 = _8741;
        Interval3 _8752 = _8742;
        Interval3 _8778 = _8752;
        Interval3 param_var_a_7 = _8778;
        Interval3 param_var_b_6 = n;
        Interval _8730 = param_var_a_7.x;
        Interval _8731 = param_var_b_6.x;
        Interval _9577 = imul(_8730, _8731, intervalFailed, optical_product_upper);
        Interval _8732 = _9577;
        Interval _8733 = param_var_a_7.y;
        Interval _8734 = param_var_b_6.y;
        Interval _9582 = imul(_8733, _8734, intervalFailed, optical_product_upper);
        Interval _8735 = _9582;
        Interval _9583 = iadd(_8732, _8735, intervalFailed);
        Interval _8736 = _9583;
        Interval _8737 = param_var_a_7.z;
        Interval _8738 = param_var_b_6.z;
        Interval _9588 = imul(_8737, _8738, intervalFailed, optical_product_upper);
        Interval _8739 = _9588;
        Interval _9589 = iadd(_8736, _8739, intervalFailed);
        Interval _8740 = _9589;
        Interval param_var_a_8 = _8740;
        Interval param_var_b_7 = denominator;
        Interval _9592 = idiv(param_var_a_8, param_var_b_7, intervalFailed, interval_divide_upper);
        Interval _distance = _9592;
        if (primary)
        {
            Interval param_var_a_9 = _distance;
            Interval param_var_b_8 = outgoing.z;
            Interval _9597 = imul(param_var_a_9, param_var_b_8, intervalFailed, optical_product_upper);
            Interval depth = _9597;
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
        Interval _8720 = param_var_a_11.x;
        Interval _8721 = param_var_b_9;
        Interval _9635 = imul(_8720, _8721, intervalFailed, optical_product_upper);
        Interval _8722 = _9635;
        Interval _8723 = param_var_a_11.y;
        Interval _8724 = param_var_b_9;
        Interval _9639 = imul(_8723, _8724, intervalFailed, optical_product_upper);
        Interval _8725 = _9639;
        Interval _8726 = param_var_a_11.z;
        Interval _8727 = param_var_b_9;
        Interval _9643 = imul(_8726, _8727, intervalFailed, optical_product_upper);
        Interval _8728 = _9643;
        _8718.x = _8722;
        _8718.y = _8725;
        _8718.z = _8728;
        Interval3 _8719 = _8718;
        Interval3 _8729 = _8719;
        Interval3 param_var_b_10 = _8729;
        Interval _8708 = param_var_a_10.x;
        Interval _8709 = param_var_b_10.x;
        Interval _9657 = iadd(_8708, _8709, intervalFailed);
        Interval _8710 = _9657;
        Interval _8711 = param_var_a_10.y;
        Interval _8712 = param_var_b_10.y;
        Interval _9662 = iadd(_8711, _8712, intervalFailed);
        Interval _8713 = _9662;
        Interval _8714 = param_var_a_10.z;
        Interval _8715 = param_var_b_10.z;
        Interval _9667 = iadd(_8714, _8715, intervalFailed);
        Interval _8716 = _9667;
        _8706.x = _8710;
        _8706.y = _8713;
        _8706.z = _8716;
        Interval3 _8707 = _8706;
        Interval3 _8717 = _8707;
        Interval3 hit = _8717;
        if (curved)
        {
            if (primary)
            {
                radius = _distance;
            }
            else
            {
                Interval3 param_var_a_12 = hit;
                Interval _8697 = param_var_a_12.x;
                bool _8690 = false;
                if (_8697.lo <= 0.0)
                {
                    _8690 = _8697.hi >= 0.0;
                }
                if (_8690)
                {
                    _8689 = 0.0;
                }
                else
                {
                    _8689 = precise::min(abs(_8697.lo), abs(_8697.hi));
                }
                float _8688 = _8689;
                float _8691 = precise::max(abs(_8697.lo), abs(_8697.hi));
                float _8692 = spvFMul(_8688, _8688);
                float _9712 = interval_down(_8692, intervalFailed);
                float _8693 = precise::max(0.0, _9712);
                float _8694 = spvFMul(_8691, _8691);
                float _9716 = interval_up(_8694, intervalFailed);
                float _8695 = _9716;
                _8686.lo = _8693;
                _8686.hi = _8695;
                Interval _8687 = _8686;
                Interval _8696 = _8687;
                Interval _8698 = _8696;
                Interval _8699 = param_var_a_12.y;
                bool _8679 = false;
                if (_8699.lo <= 0.0)
                {
                    _8679 = _8699.hi >= 0.0;
                }
                if (_8679)
                {
                    _8678 = 0.0;
                }
                else
                {
                    _8678 = precise::min(abs(_8699.lo), abs(_8699.hi));
                }
                float _8677 = _8678;
                float _8680 = precise::max(abs(_8699.lo), abs(_8699.hi));
                float _8681 = spvFMul(_8677, _8677);
                float _9755 = interval_down(_8681, intervalFailed);
                float _8682 = precise::max(0.0, _9755);
                float _8683 = spvFMul(_8680, _8680);
                float _9759 = interval_up(_8683, intervalFailed);
                float _8684 = _9759;
                _8675.lo = _8682;
                _8675.hi = _8684;
                Interval _8676 = _8675;
                Interval _8685 = _8676;
                Interval _8700 = _8685;
                Interval _9767 = iadd(_8698, _8700, intervalFailed);
                Interval _8701 = _9767;
                Interval _8702 = param_var_a_12.z;
                bool _8668 = false;
                if (_8702.lo <= 0.0)
                {
                    _8668 = _8702.hi >= 0.0;
                }
                if (_8668)
                {
                    _8667 = 0.0;
                }
                else
                {
                    _8667 = precise::min(abs(_8702.lo), abs(_8702.hi));
                }
                float _8666 = _8667;
                float _8669 = precise::max(abs(_8702.lo), abs(_8702.hi));
                float _8670 = spvFMul(_8666, _8666);
                float _9799 = interval_down(_8670, intervalFailed);
                float _8671 = precise::max(0.0, _9799);
                float _8672 = spvFMul(_8669, _8669);
                float _9803 = interval_up(_8672, intervalFailed);
                float _8673 = _9803;
                _8664.lo = _8671;
                _8664.hi = _8673;
                Interval _8665 = _8664;
                Interval _8674 = _8665;
                Interval _8703 = _8674;
                Interval _9811 = iadd(_8701, _8703, intervalFailed);
                Interval _8704 = _9811;
                Interval _9812 = isqrt(_8704, intervalFailed);
                Interval _8705 = _9812;
                radius = _8705;
            }
            Interval param_var_a_13 = radius;
            float param_var_x_7 = precise::max(liquid.projection.x, 1.0);
            float _8661 = param_var_x_7;
            float _8662 = param_var_x_7;
            _8659.lo = _8661;
            _8659.hi = _8662;
            Interval _8660 = _8659;
            Interval _8663 = _8660;
            Interval param_var_b_11 = _8663;
            Interval _9828 = idiv(param_var_a_13, param_var_b_11, intervalFailed, interval_divide_upper);
            Interval param_var_a_14 = _9828;
            Interval param_var_a_15 = denominator;
            bool _8655 = false;
            if (param_var_a_15.lo <= 0.0)
            {
                _8655 = param_var_a_15.hi >= 0.0;
            }
            if (_8655)
            {
                _8654 = 0.0;
            }
            else
            {
                _8654 = precise::min(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            }
            float _8656 = _8654;
            float _8657 = precise::max(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            _8652.lo = _8656;
            _8652.hi = _8657;
            Interval _8653 = _8652;
            Interval _8658 = _8653;
            Interval param_var_a_16 = _8658;
            float param_var_n_1 = 4.0;
            float param_var_d = 100.0;
            Interval _9864 = iratio(param_var_n_1, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_12 = _9864;
            float _8649 = precise::max(param_var_a_16.lo, param_var_b_12.lo);
            float _8650 = precise::max(param_var_a_16.hi, param_var_b_12.hi);
            _8647.lo = _8649;
            _8647.hi = _8650;
            Interval _8648 = _8647;
            Interval _8651 = _8648;
            Interval param_var_b_13 = _8651;
            Interval _9882 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
            Interval footprint = _9882;
            ReflectionLiquidFrame param_var_f = liquid;
            Interval3 param_var_direction_1 = outgoing;
            Interval param_var_distance = _distance;
            Interval param_var_footprint = footprint;
            Interval3 _8494 = hit;
            ReflectionLiquidFrame _8495 = param_var_f;
            float3 _8479 = _8495.rotation0.xyz;
            float _8472 = _8479.x;
            float _8469 = _8472;
            float _8470 = _8472;
            _8467.lo = _8469;
            _8467.hi = _8470;
            Interval _8468 = _8467;
            Interval _8471 = _8468;
            Interval _8473 = _8471;
            float _8474 = _8479.y;
            float _8464 = _8474;
            float _8465 = _8474;
            _8462.lo = _8464;
            _8462.hi = _8465;
            Interval _8463 = _8462;
            Interval _8466 = _8463;
            Interval _8475 = _8466;
            float _8476 = _8479.z;
            float _8459 = _8476;
            float _8460 = _8476;
            _8457.lo = _8459;
            _8457.hi = _8460;
            Interval _8458 = _8457;
            Interval _8461 = _8458;
            Interval _8477 = _8461;
            _8455.x = _8473;
            _8455.y = _8475;
            _8455.z = _8477;
            Interval3 _8456 = _8455;
            Interval3 _8478 = _8456;
            Interval3 _8480 = _8478;
            Interval _8481 = _8494.x;
            Interval _8445 = _8480.x;
            Interval _8446 = _8481;
            Interval _9939 = imul(_8445, _8446, intervalFailed, optical_product_upper);
            Interval _8447 = _9939;
            Interval _8448 = _8480.y;
            Interval _8449 = _8481;
            Interval _9943 = imul(_8448, _8449, intervalFailed, optical_product_upper);
            Interval _8450 = _9943;
            Interval _8451 = _8480.z;
            Interval _8452 = _8481;
            Interval _9947 = imul(_8451, _8452, intervalFailed, optical_product_upper);
            Interval _8453 = _9947;
            _8443.x = _8447;
            _8443.y = _8450;
            _8443.z = _8453;
            Interval3 _8444 = _8443;
            Interval3 _8454 = _8444;
            Interval3 _8482 = _8454;
            float3 _8483 = _8495.rotation1.xyz;
            float _8436 = _8483.x;
            float _8433 = _8436;
            float _8434 = _8436;
            _8431.lo = _8433;
            _8431.hi = _8434;
            Interval _8432 = _8431;
            Interval _8435 = _8432;
            Interval _8437 = _8435;
            float _8438 = _8483.y;
            float _8428 = _8438;
            float _8429 = _8438;
            _8426.lo = _8428;
            _8426.hi = _8429;
            Interval _8427 = _8426;
            Interval _8430 = _8427;
            Interval _8439 = _8430;
            float _8440 = _8483.z;
            float _8423 = _8440;
            float _8424 = _8440;
            _8421.lo = _8423;
            _8421.hi = _8424;
            Interval _8422 = _8421;
            Interval _8425 = _8422;
            Interval _8441 = _8425;
            _8419.x = _8437;
            _8419.y = _8439;
            _8419.z = _8441;
            Interval3 _8420 = _8419;
            Interval3 _8442 = _8420;
            Interval3 _8484 = _8442;
            Interval _8485 = _8494.y;
            Interval _8409 = _8484.x;
            Interval _8410 = _8485;
            Interval _10007 = imul(_8409, _8410, intervalFailed, optical_product_upper);
            Interval _8411 = _10007;
            Interval _8412 = _8484.y;
            Interval _8413 = _8485;
            Interval _10011 = imul(_8412, _8413, intervalFailed, optical_product_upper);
            Interval _8414 = _10011;
            Interval _8415 = _8484.z;
            Interval _8416 = _8485;
            Interval _10015 = imul(_8415, _8416, intervalFailed, optical_product_upper);
            Interval _8417 = _10015;
            _8407.x = _8411;
            _8407.y = _8414;
            _8407.z = _8417;
            Interval3 _8408 = _8407;
            Interval3 _8418 = _8408;
            Interval3 _8486 = _8418;
            Interval _8397 = _8482.x;
            Interval _8398 = _8486.x;
            Interval _10029 = iadd(_8397, _8398, intervalFailed);
            Interval _8399 = _10029;
            Interval _8400 = _8482.y;
            Interval _8401 = _8486.y;
            Interval _10034 = iadd(_8400, _8401, intervalFailed);
            Interval _8402 = _10034;
            Interval _8403 = _8482.z;
            Interval _8404 = _8486.z;
            Interval _10039 = iadd(_8403, _8404, intervalFailed);
            Interval _8405 = _10039;
            _8395.x = _8399;
            _8395.y = _8402;
            _8395.z = _8405;
            Interval3 _8396 = _8395;
            Interval3 _8406 = _8396;
            Interval3 _8487 = _8406;
            float3 _8488 = _8495.rotation2.xyz;
            float _8388 = _8488.x;
            float _8385 = _8388;
            float _8386 = _8388;
            _8383.lo = _8385;
            _8383.hi = _8386;
            Interval _8384 = _8383;
            Interval _8387 = _8384;
            Interval _8389 = _8387;
            float _8390 = _8488.y;
            float _8380 = _8390;
            float _8381 = _8390;
            _8378.lo = _8380;
            _8378.hi = _8381;
            Interval _8379 = _8378;
            Interval _8382 = _8379;
            Interval _8391 = _8382;
            float _8392 = _8488.z;
            float _8375 = _8392;
            float _8376 = _8392;
            _8373.lo = _8375;
            _8373.hi = _8376;
            Interval _8374 = _8373;
            Interval _8377 = _8374;
            Interval _8393 = _8377;
            _8371.x = _8389;
            _8371.y = _8391;
            _8371.z = _8393;
            Interval3 _8372 = _8371;
            Interval3 _8394 = _8372;
            Interval3 _8489 = _8394;
            Interval _8490 = _8494.z;
            Interval _8361 = _8489.x;
            Interval _8362 = _8490;
            Interval _10099 = imul(_8361, _8362, intervalFailed, optical_product_upper);
            Interval _8363 = _10099;
            Interval _8364 = _8489.y;
            Interval _8365 = _8490;
            Interval _10103 = imul(_8364, _8365, intervalFailed, optical_product_upper);
            Interval _8366 = _10103;
            Interval _8367 = _8489.z;
            Interval _8368 = _8490;
            Interval _10107 = imul(_8367, _8368, intervalFailed, optical_product_upper);
            Interval _8369 = _10107;
            _8359.x = _8363;
            _8359.y = _8366;
            _8359.z = _8369;
            Interval3 _8360 = _8359;
            Interval3 _8370 = _8360;
            Interval3 _8491 = _8370;
            Interval _8349 = _8487.x;
            Interval _8350 = _8491.x;
            Interval _10121 = iadd(_8349, _8350, intervalFailed);
            Interval _8351 = _10121;
            Interval _8352 = _8487.y;
            Interval _8353 = _8491.y;
            Interval _10126 = iadd(_8352, _8353, intervalFailed);
            Interval _8354 = _10126;
            Interval _8355 = _8487.z;
            Interval _8356 = _8491.z;
            Interval _10131 = iadd(_8355, _8356, intervalFailed);
            Interval _8357 = _10131;
            _8347.x = _8351;
            _8347.y = _8354;
            _8347.z = _8357;
            Interval3 _8348 = _8347;
            Interval3 _8358 = _8348;
            Interval3 _8492 = _8358;
            Interval3 _8496 = _8492;
            float3 _8497 = float3(param_var_f.rotation0.w, param_var_f.rotation1.w, param_var_f.rotation2.w);
            float _8340 = _8497.x;
            float _8337 = _8340;
            float _8338 = _8340;
            _8335.lo = _8337;
            _8335.hi = _8338;
            Interval _8336 = _8335;
            Interval _8339 = _8336;
            Interval _8341 = _8339;
            float _8342 = _8497.y;
            float _8332 = _8342;
            float _8333 = _8342;
            _8330.lo = _8332;
            _8330.hi = _8333;
            Interval _8331 = _8330;
            Interval _8334 = _8331;
            Interval _8343 = _8334;
            float _8344 = _8497.z;
            float _8327 = _8344;
            float _8328 = _8344;
            _8325.lo = _8327;
            _8325.hi = _8328;
            Interval _8326 = _8325;
            Interval _8329 = _8326;
            Interval _8345 = _8329;
            _8323.x = _8341;
            _8323.y = _8343;
            _8323.z = _8345;
            Interval3 _8324 = _8323;
            Interval3 _8346 = _8324;
            Interval3 _8498 = _8346;
            Interval _8313 = _8496.x;
            Interval _8314 = _8498.x;
            Interval _10198 = iadd(_8313, _8314, intervalFailed);
            Interval _8315 = _10198;
            Interval _8316 = _8496.y;
            Interval _8317 = _8498.y;
            Interval _10203 = iadd(_8316, _8317, intervalFailed);
            Interval _8318 = _10203;
            Interval _8319 = _8496.z;
            Interval _8320 = _8498.z;
            Interval _10208 = iadd(_8319, _8320, intervalFailed);
            Interval _8321 = _10208;
            _8311.x = _8315;
            _8311.y = _8318;
            _8311.z = _8321;
            Interval3 _8312 = _8311;
            Interval3 _8322 = _8312;
            Interval3 _8493 = _8322;
            float _8500 = param_var_f.settings.x;
            float _8308 = _8500;
            float _8309 = _8500;
            _8306.lo = _8308;
            _8306.hi = _8309;
            Interval _8307 = _8306;
            Interval _8310 = _8307;
            Interval _8499 = _8310;
            if (param_var_f.settings.y == 3.0)
            {
                float _8503 = 0.0;
                float _8303 = _8503;
                float _8304 = _8503;
                _8301.lo = _8303;
                _8301.hi = _8304;
                Interval _8302 = _8301;
                Interval _8305 = _8302;
                _8502 = _8305;
                _8501 = _8305;
                Interval3 _8505 = param_var_direction_1;
                ReflectionLiquidFrame _8506 = param_var_f;
                float3 _8287 = _8506.rotation0.xyz;
                float _8280 = _8287.x;
                float _8277 = _8280;
                float _8278 = _8280;
                _8275.lo = _8277;
                _8275.hi = _8278;
                Interval _8276 = _8275;
                Interval _8279 = _8276;
                Interval _8281 = _8279;
                float _8282 = _8287.y;
                float _8272 = _8282;
                float _8273 = _8282;
                _8270.lo = _8272;
                _8270.hi = _8273;
                Interval _8271 = _8270;
                Interval _8274 = _8271;
                Interval _8283 = _8274;
                float _8284 = _8287.z;
                float _8267 = _8284;
                float _8268 = _8284;
                _8265.lo = _8267;
                _8265.hi = _8268;
                Interval _8266 = _8265;
                Interval _8269 = _8266;
                Interval _8285 = _8269;
                _8263.x = _8281;
                _8263.y = _8283;
                _8263.z = _8285;
                Interval3 _8264 = _8263;
                Interval3 _8286 = _8264;
                Interval3 _8288 = _8286;
                Interval _8289 = _8505.x;
                Interval _8253 = _8288.x;
                Interval _8254 = _8289;
                Interval _10298 = imul(_8253, _8254, intervalFailed, optical_product_upper);
                Interval _8255 = _10298;
                Interval _8256 = _8288.y;
                Interval _8257 = _8289;
                Interval _10302 = imul(_8256, _8257, intervalFailed, optical_product_upper);
                Interval _8258 = _10302;
                Interval _8259 = _8288.z;
                Interval _8260 = _8289;
                Interval _10306 = imul(_8259, _8260, intervalFailed, optical_product_upper);
                Interval _8261 = _10306;
                _8251.x = _8255;
                _8251.y = _8258;
                _8251.z = _8261;
                Interval3 _8252 = _8251;
                Interval3 _8262 = _8252;
                Interval3 _8290 = _8262;
                float3 _8291 = _8506.rotation1.xyz;
                float _8244 = _8291.x;
                float _8241 = _8244;
                float _8242 = _8244;
                _8239.lo = _8241;
                _8239.hi = _8242;
                Interval _8240 = _8239;
                Interval _8243 = _8240;
                Interval _8245 = _8243;
                float _8246 = _8291.y;
                float _8236 = _8246;
                float _8237 = _8246;
                _8234.lo = _8236;
                _8234.hi = _8237;
                Interval _8235 = _8234;
                Interval _8238 = _8235;
                Interval _8247 = _8238;
                float _8248 = _8291.z;
                float _8231 = _8248;
                float _8232 = _8248;
                _8229.lo = _8231;
                _8229.hi = _8232;
                Interval _8230 = _8229;
                Interval _8233 = _8230;
                Interval _8249 = _8233;
                _8227.x = _8245;
                _8227.y = _8247;
                _8227.z = _8249;
                Interval3 _8228 = _8227;
                Interval3 _8250 = _8228;
                Interval3 _8292 = _8250;
                Interval _8293 = _8505.y;
                Interval _8217 = _8292.x;
                Interval _8218 = _8293;
                Interval _10366 = imul(_8217, _8218, intervalFailed, optical_product_upper);
                Interval _8219 = _10366;
                Interval _8220 = _8292.y;
                Interval _8221 = _8293;
                Interval _10370 = imul(_8220, _8221, intervalFailed, optical_product_upper);
                Interval _8222 = _10370;
                Interval _8223 = _8292.z;
                Interval _8224 = _8293;
                Interval _10374 = imul(_8223, _8224, intervalFailed, optical_product_upper);
                Interval _8225 = _10374;
                _8215.x = _8219;
                _8215.y = _8222;
                _8215.z = _8225;
                Interval3 _8216 = _8215;
                Interval3 _8226 = _8216;
                Interval3 _8294 = _8226;
                Interval _8205 = _8290.x;
                Interval _8206 = _8294.x;
                Interval _10388 = iadd(_8205, _8206, intervalFailed);
                Interval _8207 = _10388;
                Interval _8208 = _8290.y;
                Interval _8209 = _8294.y;
                Interval _10393 = iadd(_8208, _8209, intervalFailed);
                Interval _8210 = _10393;
                Interval _8211 = _8290.z;
                Interval _8212 = _8294.z;
                Interval _10398 = iadd(_8211, _8212, intervalFailed);
                Interval _8213 = _10398;
                _8203.x = _8207;
                _8203.y = _8210;
                _8203.z = _8213;
                Interval3 _8204 = _8203;
                Interval3 _8214 = _8204;
                Interval3 _8295 = _8214;
                float3 _8296 = _8506.rotation2.xyz;
                float _8196 = _8296.x;
                float _8193 = _8196;
                float _8194 = _8196;
                _8191.lo = _8193;
                _8191.hi = _8194;
                Interval _8192 = _8191;
                Interval _8195 = _8192;
                Interval _8197 = _8195;
                float _8198 = _8296.y;
                float _8188 = _8198;
                float _8189 = _8198;
                _8186.lo = _8188;
                _8186.hi = _8189;
                Interval _8187 = _8186;
                Interval _8190 = _8187;
                Interval _8199 = _8190;
                float _8200 = _8296.z;
                float _8183 = _8200;
                float _8184 = _8200;
                _8181.lo = _8183;
                _8181.hi = _8184;
                Interval _8182 = _8181;
                Interval _8185 = _8182;
                Interval _8201 = _8185;
                _8179.x = _8197;
                _8179.y = _8199;
                _8179.z = _8201;
                Interval3 _8180 = _8179;
                Interval3 _8202 = _8180;
                Interval3 _8297 = _8202;
                Interval _8298 = _8505.z;
                Interval _8169 = _8297.x;
                Interval _8170 = _8298;
                Interval _10458 = imul(_8169, _8170, intervalFailed, optical_product_upper);
                Interval _8171 = _10458;
                Interval _8172 = _8297.y;
                Interval _8173 = _8298;
                Interval _10462 = imul(_8172, _8173, intervalFailed, optical_product_upper);
                Interval _8174 = _10462;
                Interval _8175 = _8297.z;
                Interval _8176 = _8298;
                Interval _10466 = imul(_8175, _8176, intervalFailed, optical_product_upper);
                Interval _8177 = _10466;
                _8167.x = _8171;
                _8167.y = _8174;
                _8167.z = _8177;
                Interval3 _8168 = _8167;
                Interval3 _8178 = _8168;
                Interval3 _8299 = _8178;
                Interval _8157 = _8295.x;
                Interval _8158 = _8299.x;
                Interval _10480 = iadd(_8157, _8158, intervalFailed);
                Interval _8159 = _10480;
                Interval _8160 = _8295.y;
                Interval _8161 = _8299.y;
                Interval _10485 = iadd(_8160, _8161, intervalFailed);
                Interval _8162 = _10485;
                Interval _8163 = _8295.z;
                Interval _8164 = _8299.z;
                Interval _10490 = iadd(_8163, _8164, intervalFailed);
                Interval _8165 = _10490;
                _8155.x = _8159;
                _8155.y = _8162;
                _8155.z = _8165;
                Interval3 _8156 = _8155;
                Interval3 _8166 = _8156;
                Interval3 _8300 = _8166;
                Interval3 _8504 = _8300;
                for (uint _8507 = 0u; _8507 < 2u; _8507++)
                {
                    Interval _8509 = _8493.x;
                    Interval _8510 = _8493.z;
                    Interval _8511 = _8499;
                    Interval _8512 = param_var_footprint;
                    Interval _7754 = _8509;
                    float _7755 = 6.0;
                    float _7756 = 1000.0;
                    Interval _10514 = iratio(_7755, _7756, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7757 = _10514;
                    Interval _10515 = imul(_7754, _7757, intervalFailed, optical_product_upper);
                    Interval _7758 = _10515;
                    Interval _7759 = _8510;
                    float _7760 = 8.0;
                    float _7761 = 1000.0;
                    Interval _10517 = iratio(_7760, _7761, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7762 = _10517;
                    Interval _10518 = imul(_7759, _7762, intervalFailed, optical_product_upper);
                    Interval _7763 = _10518;
                    Interval _7749 = _7758;
                    Interval _7750 = _7763;
                    float _7746 = as_type<float>(as_type<uint>(_7750.hi) ^ 2147483648u);
                    float _7747 = as_type<float>(as_type<uint>(_7750.lo) ^ 2147483648u);
                    _7744.lo = _7746;
                    _7744.hi = _7747;
                    Interval _7745 = _7744;
                    Interval _7748 = _7745;
                    Interval _7751 = _7748;
                    Interval _10538 = iadd(_7749, _7751, intervalFailed);
                    Interval _7752 = _10538;
                    Interval _7764 = _7752;
                    Interval _7765 = _8511;
                    float _7766 = 11.0;
                    float _7767 = 100.0;
                    Interval _10541 = iratio(_7766, _7767, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7768 = _10541;
                    Interval _10542 = imul(_7765, _7768, intervalFailed, optical_product_upper);
                    Interval _7769 = _10542;
                    Interval _10543 = iadd(_7764, _7769, intervalFailed);
                    Interval _7753 = _10543;
                    Interval _7771 = _8512;
                    float _7772 = 10.0;
                    float _7773 = 1000.0;
                    Interval _10545 = iratio(_7772, _7773, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7774 = _10545;
                    Interval _7733 = _7771;
                    Interval _7734 = _7774;
                    Interval _10548 = imul(_7733, _7734, intervalFailed, optical_product_upper);
                    Interval _7735 = _10548;
                    bool _7725 = false;
                    if (_7735.lo <= 0.0)
                    {
                        _7725 = _7735.hi >= 0.0;
                    }
                    if (_7725)
                    {
                        _7724 = 0.0;
                    }
                    else
                    {
                        _7724 = precise::min(abs(_7735.lo), abs(_7735.hi));
                    }
                    float _7723 = _7724;
                    float _7726 = precise::max(abs(_7735.lo), abs(_7735.hi));
                    float _7727 = spvFMul(_7723, _7723);
                    float _10578 = interval_down(_7727, intervalFailed);
                    float _7728 = precise::max(0.0, _10578);
                    float _7729 = spvFMul(_7726, _7726);
                    float _10582 = interval_up(_7729, intervalFailed);
                    float _7730 = _10582;
                    _7721.lo = _7728;
                    _7721.hi = _7730;
                    Interval _7722 = _7721;
                    Interval _7731 = _7722;
                    Interval _7732 = _7731;
                    float _7736 = 1.0;
                    float _7718 = _7736;
                    float _7719 = _7736;
                    _7716.lo = _7718;
                    _7716.hi = _7719;
                    Interval _7717 = _7716;
                    Interval _7720 = _7717;
                    Interval _7737 = _7720;
                    float _7738 = 1.0;
                    float _7713 = _7738;
                    float _7714 = _7738;
                    _7711.lo = _7713;
                    _7711.hi = _7714;
                    Interval _7712 = _7711;
                    Interval _7715 = _7712;
                    Interval _7739 = _7715;
                    Interval _7740 = _7732;
                    bool _7704 = false;
                    if (_7740.lo <= 0.0)
                    {
                        _7704 = _7740.hi >= 0.0;
                    }
                    if (_7704)
                    {
                        _7703 = 0.0;
                    }
                    else
                    {
                        _7703 = precise::min(abs(_7740.lo), abs(_7740.hi));
                    }
                    float _7702 = _7703;
                    float _7705 = precise::max(abs(_7740.lo), abs(_7740.hi));
                    float _7706 = spvFMul(_7702, _7702);
                    float _10638 = interval_down(_7706, intervalFailed);
                    float _7707 = precise::max(0.0, _10638);
                    float _7708 = spvFMul(_7705, _7705);
                    float _10642 = interval_up(_7708, intervalFailed);
                    float _7709 = _10642;
                    _7700.lo = _7707;
                    _7700.hi = _7709;
                    Interval _7701 = _7700;
                    Interval _7710 = _7701;
                    Interval _7741 = _7710;
                    Interval _10650 = iadd(_7739, _7741, intervalFailed);
                    Interval _7742 = _10650;
                    Interval _10651 = idiv(_7737, _7742, intervalFailed, interval_divide_upper);
                    Interval _7743 = _10651;
                    Interval _7770 = _7743;
                    Interval _7776 = _8509;
                    float _7777 = 18.0;
                    float _7778 = 1000.0;
                    Interval _10654 = iratio(_7777, _7778, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7779 = _10654;
                    Interval _10655 = imul(_7776, _7779, intervalFailed, optical_product_upper);
                    Interval _7780 = _10655;
                    Interval _7781 = _8510;
                    float _7782 = 11.0;
                    float _7783 = 1000.0;
                    Interval _10657 = iratio(_7782, _7783, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7784 = _10657;
                    Interval _10658 = imul(_7781, _7784, intervalFailed, optical_product_upper);
                    Interval _7785 = _10658;
                    Interval _10659 = iadd(_7780, _7785, intervalFailed);
                    Interval _7786 = _10659;
                    Interval _7787 = _8511;
                    float _7788 = 45.0;
                    float _7789 = 100.0;
                    Interval _10661 = iratio(_7788, _7789, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7790 = _10661;
                    Interval _10662 = imul(_7787, _7790, intervalFailed, optical_product_upper);
                    Interval _7791 = _10662;
                    Interval _7696 = _7786;
                    Interval _7697 = _7791;
                    float _7693 = as_type<float>(as_type<uint>(_7697.hi) ^ 2147483648u);
                    float _7694 = as_type<float>(as_type<uint>(_7697.lo) ^ 2147483648u);
                    _7691.lo = _7693;
                    _7691.hi = _7694;
                    Interval _7692 = _7691;
                    Interval _7695 = _7692;
                    Interval _7698 = _7695;
                    Interval _10682 = iadd(_7696, _7698, intervalFailed);
                    Interval _7699 = _10682;
                    Interval _7792 = _7699;
                    float _7793 = 65.0;
                    float _7794 = 100.0;
                    Interval _10684 = iratio(_7793, _7794, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7795 = _10684;
                    Interval _7796 = _7753;
                    float _7686 = _7796.lo;
                    float _7687 = _7796.hi;
                    float _7681 = _7686;
                    float _7682 = _7687;
                    _7678.lo = _7681;
                    _7678.hi = _7682;
                    Interval _7679 = _7678;
                    Interval _7683 = _7679;
                    Interval _10698 = isin_body(_7683, intervalFailed, optical_product_upper);
                    Interval _7680 = _10698;
                    interval_sine_upper = _7680.hi;
                    float _7684 = _7680.lo;
                    float _7685 = _7684;
                    float _7688 = _7685;
                    float _7689 = interval_sine_upper;
                    _7676.lo = _7688;
                    _7676.hi = _7689;
                    Interval _7677 = _7676;
                    Interval _7690 = _7677;
                    Interval _7797 = _7690;
                    Interval _10713 = imul(_7795, _7797, intervalFailed, optical_product_upper);
                    Interval _7798 = _10713;
                    Interval _7799 = _7770;
                    Interval _10715 = imul(_7798, _7799, intervalFailed, optical_product_upper);
                    Interval _7800 = _10715;
                    Interval _10716 = iadd(_7792, _7800, intervalFailed);
                    Interval _7775 = _10716;
                    Interval _7802 = _8509;
                    float _7803 = 47.0;
                    float _7804 = 1000.0;
                    Interval _10718 = iratio(_7803, _7804, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7805 = _10718;
                    Interval _10719 = imul(_7802, _7805, intervalFailed, optical_product_upper);
                    Interval _7806 = _10719;
                    Interval _7807 = _8510;
                    float _7808 = 25.0;
                    float _7809 = 1000.0;
                    Interval _10721 = iratio(_7808, _7809, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7810 = _10721;
                    Interval _10722 = imul(_7807, _7810, intervalFailed, optical_product_upper);
                    Interval _7811 = _10722;
                    Interval _7672 = _7806;
                    Interval _7673 = _7811;
                    float _7669 = as_type<float>(as_type<uint>(_7673.hi) ^ 2147483648u);
                    float _7670 = as_type<float>(as_type<uint>(_7673.lo) ^ 2147483648u);
                    _7667.lo = _7669;
                    _7667.hi = _7670;
                    Interval _7668 = _7667;
                    Interval _7671 = _7668;
                    Interval _7674 = _7671;
                    Interval _10742 = iadd(_7672, _7674, intervalFailed);
                    Interval _7675 = _10742;
                    Interval _7812 = _7675;
                    Interval _7813 = _8511;
                    float _7814 = 60.0;
                    float _7815 = 100.0;
                    Interval _10745 = iratio(_7814, _7815, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7816 = _10745;
                    Interval _10746 = imul(_7813, _7816, intervalFailed, optical_product_upper);
                    Interval _7817 = _10746;
                    Interval _10747 = iadd(_7812, _7817, intervalFailed);
                    Interval _7801 = _10747;
                    Interval _7819 = _8510;
                    float _7820 = 22.0;
                    float _7821 = 1000.0;
                    Interval _10749 = iratio(_7820, _7821, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7822 = _10749;
                    Interval _10750 = imul(_7819, _7822, intervalFailed, optical_product_upper);
                    Interval _7823 = _10750;
                    Interval _7824 = _8509;
                    float _7825 = 9.0;
                    float _7826 = 1000.0;
                    Interval _10752 = iratio(_7825, _7826, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7827 = _10752;
                    Interval _10753 = imul(_7824, _7827, intervalFailed, optical_product_upper);
                    Interval _7828 = _10753;
                    Interval _7663 = _7823;
                    Interval _7664 = _7828;
                    float _7660 = as_type<float>(as_type<uint>(_7664.hi) ^ 2147483648u);
                    float _7661 = as_type<float>(as_type<uint>(_7664.lo) ^ 2147483648u);
                    _7658.lo = _7660;
                    _7658.hi = _7661;
                    Interval _7659 = _7658;
                    Interval _7662 = _7659;
                    Interval _7665 = _7662;
                    Interval _10773 = iadd(_7663, _7665, intervalFailed);
                    Interval _7666 = _10773;
                    Interval _7829 = _7666;
                    Interval _7830 = _8511;
                    float _7831 = 32.0;
                    float _7832 = 100.0;
                    Interval _10776 = iratio(_7831, _7832, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7833 = _10776;
                    Interval _10777 = imul(_7830, _7833, intervalFailed, optical_product_upper);
                    Interval _7834 = _10777;
                    Interval _7654 = _7829;
                    Interval _7655 = _7834;
                    float _7651 = as_type<float>(as_type<uint>(_7655.hi) ^ 2147483648u);
                    float _7652 = as_type<float>(as_type<uint>(_7655.lo) ^ 2147483648u);
                    _7649.lo = _7651;
                    _7649.hi = _7652;
                    Interval _7650 = _7649;
                    Interval _7653 = _7650;
                    Interval _7656 = _7653;
                    Interval _10797 = iadd(_7654, _7656, intervalFailed);
                    Interval _7657 = _10797;
                    Interval _7818 = _7657;
                    Interval _7836 = _8512;
                    float _7837 = 22.0;
                    float _7838 = 1000.0;
                    Interval _10800 = iratio(_7837, _7838, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7839 = _10800;
                    Interval _7638 = _7836;
                    Interval _7639 = _7839;
                    Interval _10803 = imul(_7638, _7639, intervalFailed, optical_product_upper);
                    Interval _7640 = _10803;
                    bool _7630 = false;
                    if (_7640.lo <= 0.0)
                    {
                        _7630 = _7640.hi >= 0.0;
                    }
                    if (_7630)
                    {
                        _7629 = 0.0;
                    }
                    else
                    {
                        _7629 = precise::min(abs(_7640.lo), abs(_7640.hi));
                    }
                    float _7628 = _7629;
                    float _7631 = precise::max(abs(_7640.lo), abs(_7640.hi));
                    float _7632 = spvFMul(_7628, _7628);
                    float _10833 = interval_down(_7632, intervalFailed);
                    float _7633 = precise::max(0.0, _10833);
                    float _7634 = spvFMul(_7631, _7631);
                    float _10837 = interval_up(_7634, intervalFailed);
                    float _7635 = _10837;
                    _7626.lo = _7633;
                    _7626.hi = _7635;
                    Interval _7627 = _7626;
                    Interval _7636 = _7627;
                    Interval _7637 = _7636;
                    float _7641 = 1.0;
                    float _7623 = _7641;
                    float _7624 = _7641;
                    _7621.lo = _7623;
                    _7621.hi = _7624;
                    Interval _7622 = _7621;
                    Interval _7625 = _7622;
                    Interval _7642 = _7625;
                    float _7643 = 1.0;
                    float _7618 = _7643;
                    float _7619 = _7643;
                    _7616.lo = _7618;
                    _7616.hi = _7619;
                    Interval _7617 = _7616;
                    Interval _7620 = _7617;
                    Interval _7644 = _7620;
                    Interval _7645 = _7637;
                    bool _7609 = false;
                    if (_7645.lo <= 0.0)
                    {
                        _7609 = _7645.hi >= 0.0;
                    }
                    if (_7609)
                    {
                        _7608 = 0.0;
                    }
                    else
                    {
                        _7608 = precise::min(abs(_7645.lo), abs(_7645.hi));
                    }
                    float _7607 = _7608;
                    float _7610 = precise::max(abs(_7645.lo), abs(_7645.hi));
                    float _7611 = spvFMul(_7607, _7607);
                    float _10893 = interval_down(_7611, intervalFailed);
                    float _7612 = precise::max(0.0, _10893);
                    float _7613 = spvFMul(_7610, _7610);
                    float _10897 = interval_up(_7613, intervalFailed);
                    float _7614 = _10897;
                    _7605.lo = _7612;
                    _7605.hi = _7614;
                    Interval _7606 = _7605;
                    Interval _7615 = _7606;
                    Interval _7646 = _7615;
                    Interval _10905 = iadd(_7644, _7646, intervalFailed);
                    Interval _7647 = _10905;
                    Interval _10906 = idiv(_7642, _7647, intervalFailed, interval_divide_upper);
                    Interval _7648 = _10906;
                    Interval _7835 = _7648;
                    Interval _7841 = _8512;
                    float _7842 = 54.0;
                    float _7843 = 1000.0;
                    Interval _10909 = iratio(_7842, _7843, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7844 = _10909;
                    Interval _7594 = _7841;
                    Interval _7595 = _7844;
                    Interval _10912 = imul(_7594, _7595, intervalFailed, optical_product_upper);
                    Interval _7596 = _10912;
                    bool _7586 = false;
                    if (_7596.lo <= 0.0)
                    {
                        _7586 = _7596.hi >= 0.0;
                    }
                    if (_7586)
                    {
                        _7585 = 0.0;
                    }
                    else
                    {
                        _7585 = precise::min(abs(_7596.lo), abs(_7596.hi));
                    }
                    float _7584 = _7585;
                    float _7587 = precise::max(abs(_7596.lo), abs(_7596.hi));
                    float _7588 = spvFMul(_7584, _7584);
                    float _10942 = interval_down(_7588, intervalFailed);
                    float _7589 = precise::max(0.0, _10942);
                    float _7590 = spvFMul(_7587, _7587);
                    float _10946 = interval_up(_7590, intervalFailed);
                    float _7591 = _10946;
                    _7582.lo = _7589;
                    _7582.hi = _7591;
                    Interval _7583 = _7582;
                    Interval _7592 = _7583;
                    Interval _7593 = _7592;
                    float _7597 = 1.0;
                    float _7579 = _7597;
                    float _7580 = _7597;
                    _7577.lo = _7579;
                    _7577.hi = _7580;
                    Interval _7578 = _7577;
                    Interval _7581 = _7578;
                    Interval _7598 = _7581;
                    float _7599 = 1.0;
                    float _7574 = _7599;
                    float _7575 = _7599;
                    _7572.lo = _7574;
                    _7572.hi = _7575;
                    Interval _7573 = _7572;
                    Interval _7576 = _7573;
                    Interval _7600 = _7576;
                    Interval _7601 = _7593;
                    bool _7565 = false;
                    if (_7601.lo <= 0.0)
                    {
                        _7565 = _7601.hi >= 0.0;
                    }
                    if (_7565)
                    {
                        _7564 = 0.0;
                    }
                    else
                    {
                        _7564 = precise::min(abs(_7601.lo), abs(_7601.hi));
                    }
                    float _7563 = _7564;
                    float _7566 = precise::max(abs(_7601.lo), abs(_7601.hi));
                    float _7567 = spvFMul(_7563, _7563);
                    float _11002 = interval_down(_7567, intervalFailed);
                    float _7568 = precise::max(0.0, _11002);
                    float _7569 = spvFMul(_7566, _7566);
                    float _11006 = interval_up(_7569, intervalFailed);
                    float _7570 = _11006;
                    _7561.lo = _7568;
                    _7561.hi = _7570;
                    Interval _7562 = _7561;
                    Interval _7571 = _7562;
                    Interval _7602 = _7571;
                    Interval _11014 = iadd(_7600, _7602, intervalFailed);
                    Interval _7603 = _11014;
                    Interval _11015 = idiv(_7598, _7603, intervalFailed, interval_divide_upper);
                    Interval _7604 = _11015;
                    Interval _7840 = _7604;
                    Interval _7846 = _8512;
                    float _7847 = 24.0;
                    float _7848 = 1000.0;
                    Interval _11018 = iratio(_7847, _7848, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7849 = _11018;
                    Interval _7550 = _7846;
                    Interval _7551 = _7849;
                    Interval _11021 = imul(_7550, _7551, intervalFailed, optical_product_upper);
                    Interval _7552 = _11021;
                    bool _7542 = false;
                    if (_7552.lo <= 0.0)
                    {
                        _7542 = _7552.hi >= 0.0;
                    }
                    if (_7542)
                    {
                        _7541 = 0.0;
                    }
                    else
                    {
                        _7541 = precise::min(abs(_7552.lo), abs(_7552.hi));
                    }
                    float _7540 = _7541;
                    float _7543 = precise::max(abs(_7552.lo), abs(_7552.hi));
                    float _7544 = spvFMul(_7540, _7540);
                    float _11051 = interval_down(_7544, intervalFailed);
                    float _7545 = precise::max(0.0, _11051);
                    float _7546 = spvFMul(_7543, _7543);
                    float _11055 = interval_up(_7546, intervalFailed);
                    float _7547 = _11055;
                    _7538.lo = _7545;
                    _7538.hi = _7547;
                    Interval _7539 = _7538;
                    Interval _7548 = _7539;
                    Interval _7549 = _7548;
                    float _7553 = 1.0;
                    float _7535 = _7553;
                    float _7536 = _7553;
                    _7533.lo = _7535;
                    _7533.hi = _7536;
                    Interval _7534 = _7533;
                    Interval _7537 = _7534;
                    Interval _7554 = _7537;
                    float _7555 = 1.0;
                    float _7530 = _7555;
                    float _7531 = _7555;
                    _7528.lo = _7530;
                    _7528.hi = _7531;
                    Interval _7529 = _7528;
                    Interval _7532 = _7529;
                    Interval _7556 = _7532;
                    Interval _7557 = _7549;
                    bool _7521 = false;
                    if (_7557.lo <= 0.0)
                    {
                        _7521 = _7557.hi >= 0.0;
                    }
                    if (_7521)
                    {
                        _7520 = 0.0;
                    }
                    else
                    {
                        _7520 = precise::min(abs(_7557.lo), abs(_7557.hi));
                    }
                    float _7519 = _7520;
                    float _7522 = precise::max(abs(_7557.lo), abs(_7557.hi));
                    float _7523 = spvFMul(_7519, _7519);
                    float _11111 = interval_down(_7523, intervalFailed);
                    float _7524 = precise::max(0.0, _11111);
                    float _7525 = spvFMul(_7522, _7522);
                    float _11115 = interval_up(_7525, intervalFailed);
                    float _7526 = _11115;
                    _7517.lo = _7524;
                    _7517.hi = _7526;
                    Interval _7518 = _7517;
                    Interval _7527 = _7518;
                    Interval _7558 = _7527;
                    Interval _11123 = iadd(_7556, _7558, intervalFailed);
                    Interval _7559 = _11123;
                    Interval _11124 = idiv(_7554, _7559, intervalFailed, interval_divide_upper);
                    Interval _7560 = _11124;
                    Interval _7845 = _7560;
                    float _7851 = 9.0;
                    float _7514 = _7851;
                    float _7515 = _7851;
                    _7512.lo = _7514;
                    _7512.hi = _7515;
                    Interval _7513 = _7512;
                    Interval _7516 = _7513;
                    Interval _7852 = _7516;
                    Interval _7853 = _7775;
                    float _7507 = _7853.lo;
                    float _7508 = _7853.hi;
                    float _7502 = _7507;
                    float _7503 = _7508;
                    _7499.lo = _7502;
                    _7499.hi = _7503;
                    Interval _7500 = _7499;
                    Interval _7504 = _7500;
                    Interval _11148 = isin_body(_7504, intervalFailed, optical_product_upper);
                    Interval _7501 = _11148;
                    interval_sine_upper = _7501.hi;
                    float _7505 = _7501.lo;
                    float _7506 = _7505;
                    float _7509 = _7506;
                    float _7510 = interval_sine_upper;
                    _7497.lo = _7509;
                    _7497.hi = _7510;
                    Interval _7498 = _7497;
                    Interval _7511 = _7498;
                    Interval _7854 = _7511;
                    Interval _11163 = imul(_7852, _7854, intervalFailed, optical_product_upper);
                    Interval _7855 = _11163;
                    Interval _7856 = _7835;
                    Interval _11165 = imul(_7855, _7856, intervalFailed, optical_product_upper);
                    Interval _7857 = _11165;
                    float _7858 = 2.5;
                    float _7494 = _7858;
                    float _7495 = _7858;
                    _7492.lo = _7494;
                    _7492.hi = _7495;
                    Interval _7493 = _7492;
                    Interval _7496 = _7493;
                    Interval _7859 = _7496;
                    Interval _7860 = _7801;
                    float _7487 = _7860.lo;
                    float _7488 = _7860.hi;
                    float _7482 = _7487;
                    float _7483 = _7488;
                    _7479.lo = _7482;
                    _7479.hi = _7483;
                    Interval _7480 = _7479;
                    Interval _7484 = _7480;
                    Interval _11188 = isin_body(_7484, intervalFailed, optical_product_upper);
                    Interval _7481 = _11188;
                    interval_sine_upper = _7481.hi;
                    float _7485 = _7481.lo;
                    float _7486 = _7485;
                    float _7489 = _7486;
                    float _7490 = interval_sine_upper;
                    _7477.lo = _7489;
                    _7477.hi = _7490;
                    Interval _7478 = _7477;
                    Interval _7491 = _7478;
                    Interval _7861 = _7491;
                    Interval _11203 = imul(_7859, _7861, intervalFailed, optical_product_upper);
                    Interval _7862 = _11203;
                    Interval _7863 = _7840;
                    Interval _11205 = imul(_7862, _7863, intervalFailed, optical_product_upper);
                    Interval _7864 = _11205;
                    Interval _11206 = iadd(_7857, _7864, intervalFailed);
                    Interval _7865 = _11206;
                    float _7866 = 5.0;
                    float _7474 = _7866;
                    float _7475 = _7866;
                    _7472.lo = _7474;
                    _7472.hi = _7475;
                    Interval _7473 = _7472;
                    Interval _7476 = _7473;
                    Interval _7867 = _7476;
                    Interval _7868 = _7818;
                    float _7467 = _7868.lo;
                    float _7468 = _7868.hi;
                    float _7462 = _7467;
                    float _7463 = _7468;
                    _7459.lo = _7462;
                    _7459.hi = _7463;
                    Interval _7460 = _7459;
                    Interval _7464 = _7460;
                    Interval _11229 = isin_body(_7464, intervalFailed, optical_product_upper);
                    Interval _7461 = _11229;
                    interval_sine_upper = _7461.hi;
                    float _7465 = _7461.lo;
                    float _7466 = _7465;
                    float _7469 = _7466;
                    float _7470 = interval_sine_upper;
                    _7457.lo = _7469;
                    _7457.hi = _7470;
                    Interval _7458 = _7457;
                    Interval _7471 = _7458;
                    Interval _7869 = _7471;
                    Interval _11244 = imul(_7867, _7869, intervalFailed, optical_product_upper);
                    Interval _7870 = _11244;
                    Interval _7871 = _7845;
                    Interval _11246 = imul(_7870, _7871, intervalFailed, optical_product_upper);
                    Interval _7872 = _11246;
                    Interval _11247 = iadd(_7865, _7872, intervalFailed);
                    Interval _7850 = _11247;
                    Interval _7874 = _7753;
                    Interval _7450 = _7874;
                    float _7448 = 3.1415927410125732421875;
                    float _7443 = _7448;
                    float _11251 = interval_down(_7443, intervalFailed);
                    float _7444 = _11251;
                    float _7445 = _7448;
                    float _11253 = interval_up(_7445, intervalFailed);
                    float _7446 = _11253;
                    _7441.lo = _7444;
                    _7441.hi = _7446;
                    Interval _7442 = _7441;
                    Interval _7447 = _7442;
                    Interval _7449 = _7447;
                    Interval _7451 = _7449;
                    float _7452 = 0.5;
                    float _7438 = _7452;
                    float _7439 = _7452;
                    _7436.lo = _7438;
                    _7436.hi = _7439;
                    Interval _7437 = _7436;
                    Interval _7440 = _7437;
                    Interval _7453 = _7440;
                    Interval _11271 = imul(_7451, _7453, intervalFailed, optical_product_upper);
                    Interval _7454 = _11271;
                    Interval _11272 = iadd(_7450, _7454, intervalFailed);
                    Interval _7455 = _11272;
                    float _7431 = _7455.lo;
                    float _7432 = _7455.hi;
                    float _7426 = _7431;
                    float _7427 = _7432;
                    _7423.lo = _7426;
                    _7423.hi = _7427;
                    Interval _7424 = _7423;
                    Interval _7428 = _7424;
                    Interval _11285 = isin_body(_7428, intervalFailed, optical_product_upper);
                    Interval _7425 = _11285;
                    interval_sine_upper = _7425.hi;
                    float _7429 = _7425.lo;
                    float _7430 = _7429;
                    float _7433 = _7430;
                    float _7434 = interval_sine_upper;
                    _7421.lo = _7433;
                    _7421.hi = _7434;
                    Interval _7422 = _7421;
                    Interval _7435 = _7422;
                    Interval _7456 = _7435;
                    Interval _7875 = _7456;
                    Interval _7876 = _7770;
                    Interval _11302 = imul(_7875, _7876, intervalFailed, optical_product_upper);
                    Interval _7873 = _11302;
                    float _7878 = 9.0;
                    float _7418 = _7878;
                    float _7419 = _7878;
                    _7416.lo = _7418;
                    _7416.hi = _7419;
                    Interval _7417 = _7416;
                    Interval _7420 = _7417;
                    Interval _7879 = _7420;
                    float _7880 = 18.0;
                    float _7881 = 1000.0;
                    Interval _11312 = iratio(_7880, _7881, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7882 = _11312;
                    float _7883 = 39.0;
                    float _7884 = 10000.0;
                    Interval _11313 = iratio(_7883, _7884, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7885 = _11313;
                    Interval _7886 = _7873;
                    Interval _11315 = imul(_7885, _7886, intervalFailed, optical_product_upper);
                    Interval _7887 = _11315;
                    Interval _11316 = iadd(_7882, _7887, intervalFailed);
                    Interval _7888 = _11316;
                    Interval _11317 = imul(_7879, _7888, intervalFailed, optical_product_upper);
                    Interval _7889 = _11317;
                    Interval _7890 = _7775;
                    Interval _7409 = _7890;
                    float _7407 = 3.1415927410125732421875;
                    float _7402 = _7407;
                    float _11321 = interval_down(_7402, intervalFailed);
                    float _7403 = _11321;
                    float _7404 = _7407;
                    float _11323 = interval_up(_7404, intervalFailed);
                    float _7405 = _11323;
                    _7400.lo = _7403;
                    _7400.hi = _7405;
                    Interval _7401 = _7400;
                    Interval _7406 = _7401;
                    Interval _7408 = _7406;
                    Interval _7410 = _7408;
                    float _7411 = 0.5;
                    float _7397 = _7411;
                    float _7398 = _7411;
                    _7395.lo = _7397;
                    _7395.hi = _7398;
                    Interval _7396 = _7395;
                    Interval _7399 = _7396;
                    Interval _7412 = _7399;
                    Interval _11341 = imul(_7410, _7412, intervalFailed, optical_product_upper);
                    Interval _7413 = _11341;
                    Interval _11342 = iadd(_7409, _7413, intervalFailed);
                    Interval _7414 = _11342;
                    float _7390 = _7414.lo;
                    float _7391 = _7414.hi;
                    float _7385 = _7390;
                    float _7386 = _7391;
                    _7382.lo = _7385;
                    _7382.hi = _7386;
                    Interval _7383 = _7382;
                    Interval _7387 = _7383;
                    Interval _11355 = isin_body(_7387, intervalFailed, optical_product_upper);
                    Interval _7384 = _11355;
                    interval_sine_upper = _7384.hi;
                    float _7388 = _7384.lo;
                    float _7389 = _7388;
                    float _7392 = _7389;
                    float _7393 = interval_sine_upper;
                    _7380.lo = _7392;
                    _7380.hi = _7393;
                    Interval _7381 = _7380;
                    Interval _7394 = _7381;
                    Interval _7415 = _7394;
                    Interval _7891 = _7415;
                    Interval _11371 = imul(_7889, _7891, intervalFailed, optical_product_upper);
                    Interval _7892 = _11371;
                    Interval _7893 = _7835;
                    Interval _11373 = imul(_7892, _7893, intervalFailed, optical_product_upper);
                    Interval _7894 = _11373;
                    float _7895 = 1175.0;
                    float _7896 = 10000.0;
                    Interval _11374 = iratio(_7895, _7896, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7897 = _11374;
                    Interval _7898 = _7801;
                    Interval _7373 = _7898;
                    float _7371 = 3.1415927410125732421875;
                    float _7366 = _7371;
                    float _11378 = interval_down(_7366, intervalFailed);
                    float _7367 = _11378;
                    float _7368 = _7371;
                    float _11380 = interval_up(_7368, intervalFailed);
                    float _7369 = _11380;
                    _7364.lo = _7367;
                    _7364.hi = _7369;
                    Interval _7365 = _7364;
                    Interval _7370 = _7365;
                    Interval _7372 = _7370;
                    Interval _7374 = _7372;
                    float _7375 = 0.5;
                    float _7361 = _7375;
                    float _7362 = _7375;
                    _7359.lo = _7361;
                    _7359.hi = _7362;
                    Interval _7360 = _7359;
                    Interval _7363 = _7360;
                    Interval _7376 = _7363;
                    Interval _11398 = imul(_7374, _7376, intervalFailed, optical_product_upper);
                    Interval _7377 = _11398;
                    Interval _11399 = iadd(_7373, _7377, intervalFailed);
                    Interval _7378 = _11399;
                    float _7354 = _7378.lo;
                    float _7355 = _7378.hi;
                    float _7349 = _7354;
                    float _7350 = _7355;
                    _7346.lo = _7349;
                    _7346.hi = _7350;
                    Interval _7347 = _7346;
                    Interval _7351 = _7347;
                    Interval _11412 = isin_body(_7351, intervalFailed, optical_product_upper);
                    Interval _7348 = _11412;
                    interval_sine_upper = _7348.hi;
                    float _7352 = _7348.lo;
                    float _7353 = _7352;
                    float _7356 = _7353;
                    float _7357 = interval_sine_upper;
                    _7344.lo = _7356;
                    _7344.hi = _7357;
                    Interval _7345 = _7344;
                    Interval _7358 = _7345;
                    Interval _7379 = _7358;
                    Interval _7899 = _7379;
                    Interval _11428 = imul(_7897, _7899, intervalFailed, optical_product_upper);
                    Interval _7900 = _11428;
                    Interval _7901 = _7840;
                    Interval _11430 = imul(_7900, _7901, intervalFailed, optical_product_upper);
                    Interval _7902 = _11430;
                    Interval _11431 = iadd(_7894, _7902, intervalFailed);
                    Interval _7903 = _11431;
                    float _7904 = 45.0;
                    float _7905 = 1000.0;
                    Interval _11432 = iratio(_7904, _7905, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7906 = _11432;
                    Interval _7907 = _7818;
                    Interval _7337 = _7907;
                    float _7335 = 3.1415927410125732421875;
                    float _7330 = _7335;
                    float _11436 = interval_down(_7330, intervalFailed);
                    float _7331 = _11436;
                    float _7332 = _7335;
                    float _11438 = interval_up(_7332, intervalFailed);
                    float _7333 = _11438;
                    _7328.lo = _7331;
                    _7328.hi = _7333;
                    Interval _7329 = _7328;
                    Interval _7334 = _7329;
                    Interval _7336 = _7334;
                    Interval _7338 = _7336;
                    float _7339 = 0.5;
                    float _7325 = _7339;
                    float _7326 = _7339;
                    _7323.lo = _7325;
                    _7323.hi = _7326;
                    Interval _7324 = _7323;
                    Interval _7327 = _7324;
                    Interval _7340 = _7327;
                    Interval _11456 = imul(_7338, _7340, intervalFailed, optical_product_upper);
                    Interval _7341 = _11456;
                    Interval _11457 = iadd(_7337, _7341, intervalFailed);
                    Interval _7342 = _11457;
                    float _7318 = _7342.lo;
                    float _7319 = _7342.hi;
                    float _7313 = _7318;
                    float _7314 = _7319;
                    _7310.lo = _7313;
                    _7310.hi = _7314;
                    Interval _7311 = _7310;
                    Interval _7315 = _7311;
                    Interval _11470 = isin_body(_7315, intervalFailed, optical_product_upper);
                    Interval _7312 = _11470;
                    interval_sine_upper = _7312.hi;
                    float _7316 = _7312.lo;
                    float _7317 = _7316;
                    float _7320 = _7317;
                    float _7321 = interval_sine_upper;
                    _7308.lo = _7320;
                    _7308.hi = _7321;
                    Interval _7309 = _7308;
                    Interval _7322 = _7309;
                    Interval _7343 = _7322;
                    Interval _7908 = _7343;
                    Interval _11486 = imul(_7906, _7908, intervalFailed, optical_product_upper);
                    Interval _7909 = _11486;
                    Interval _7910 = _7845;
                    Interval _11488 = imul(_7909, _7910, intervalFailed, optical_product_upper);
                    Interval _7911 = _11488;
                    Interval _7304 = _7903;
                    Interval _7305 = _7911;
                    float _7301 = as_type<float>(as_type<uint>(_7305.hi) ^ 2147483648u);
                    float _7302 = as_type<float>(as_type<uint>(_7305.lo) ^ 2147483648u);
                    _7299.lo = _7301;
                    _7299.hi = _7302;
                    Interval _7300 = _7299;
                    Interval _7303 = _7300;
                    Interval _7306 = _7303;
                    Interval _11508 = iadd(_7304, _7306, intervalFailed);
                    Interval _7307 = _11508;
                    Interval _7877 = _7307;
                    float _7913 = 9.0;
                    float _7296 = _7913;
                    float _7297 = _7913;
                    _7294.lo = _7296;
                    _7294.hi = _7297;
                    Interval _7295 = _7294;
                    Interval _7298 = _7295;
                    Interval _7914 = _7298;
                    float _7915 = 11.0;
                    float _7916 = 1000.0;
                    Interval _11519 = iratio(_7915, _7916, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7917 = _11519;
                    float _7918 = 52.0;
                    float _7919 = 10000.0;
                    Interval _11520 = iratio(_7918, _7919, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7920 = _11520;
                    Interval _7921 = _7873;
                    Interval _11522 = imul(_7920, _7921, intervalFailed, optical_product_upper);
                    Interval _7922 = _11522;
                    Interval _7290 = _7917;
                    Interval _7291 = _7922;
                    float _7287 = as_type<float>(as_type<uint>(_7291.hi) ^ 2147483648u);
                    float _7288 = as_type<float>(as_type<uint>(_7291.lo) ^ 2147483648u);
                    _7285.lo = _7287;
                    _7285.hi = _7288;
                    Interval _7286 = _7285;
                    Interval _7289 = _7286;
                    Interval _7292 = _7289;
                    Interval _11542 = iadd(_7290, _7292, intervalFailed);
                    Interval _7293 = _11542;
                    Interval _7923 = _7293;
                    Interval _11544 = imul(_7914, _7923, intervalFailed, optical_product_upper);
                    Interval _7924 = _11544;
                    Interval _7925 = _7775;
                    Interval _7278 = _7925;
                    float _7276 = 3.1415927410125732421875;
                    float _7271 = _7276;
                    float _11548 = interval_down(_7271, intervalFailed);
                    float _7272 = _11548;
                    float _7273 = _7276;
                    float _11550 = interval_up(_7273, intervalFailed);
                    float _7274 = _11550;
                    _7269.lo = _7272;
                    _7269.hi = _7274;
                    Interval _7270 = _7269;
                    Interval _7275 = _7270;
                    Interval _7277 = _7275;
                    Interval _7279 = _7277;
                    float _7280 = 0.5;
                    float _7266 = _7280;
                    float _7267 = _7280;
                    _7264.lo = _7266;
                    _7264.hi = _7267;
                    Interval _7265 = _7264;
                    Interval _7268 = _7265;
                    Interval _7281 = _7268;
                    Interval _11568 = imul(_7279, _7281, intervalFailed, optical_product_upper);
                    Interval _7282 = _11568;
                    Interval _11569 = iadd(_7278, _7282, intervalFailed);
                    Interval _7283 = _11569;
                    float _7259 = _7283.lo;
                    float _7260 = _7283.hi;
                    float _7254 = _7259;
                    float _7255 = _7260;
                    _7251.lo = _7254;
                    _7251.hi = _7255;
                    Interval _7252 = _7251;
                    Interval _7256 = _7252;
                    Interval _11582 = isin_body(_7256, intervalFailed, optical_product_upper);
                    Interval _7253 = _11582;
                    interval_sine_upper = _7253.hi;
                    float _7257 = _7253.lo;
                    float _7258 = _7257;
                    float _7261 = _7258;
                    float _7262 = interval_sine_upper;
                    _7249.lo = _7261;
                    _7249.hi = _7262;
                    Interval _7250 = _7249;
                    Interval _7263 = _7250;
                    Interval _7284 = _7263;
                    Interval _7926 = _7284;
                    Interval _11598 = imul(_7924, _7926, intervalFailed, optical_product_upper);
                    Interval _7927 = _11598;
                    Interval _7928 = _7835;
                    Interval _11600 = imul(_7927, _7928, intervalFailed, optical_product_upper);
                    Interval _7929 = _11600;
                    float _7930 = 625.0;
                    float _7931 = 10000.0;
                    Interval _11601 = iratio(_7930, _7931, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7932 = _11601;
                    Interval _7933 = _7801;
                    Interval _7242 = _7933;
                    float _7240 = 3.1415927410125732421875;
                    float _7235 = _7240;
                    float _11605 = interval_down(_7235, intervalFailed);
                    float _7236 = _11605;
                    float _7237 = _7240;
                    float _11607 = interval_up(_7237, intervalFailed);
                    float _7238 = _11607;
                    _7233.lo = _7236;
                    _7233.hi = _7238;
                    Interval _7234 = _7233;
                    Interval _7239 = _7234;
                    Interval _7241 = _7239;
                    Interval _7243 = _7241;
                    float _7244 = 0.5;
                    float _7230 = _7244;
                    float _7231 = _7244;
                    _7228.lo = _7230;
                    _7228.hi = _7231;
                    Interval _7229 = _7228;
                    Interval _7232 = _7229;
                    Interval _7245 = _7232;
                    Interval _11625 = imul(_7243, _7245, intervalFailed, optical_product_upper);
                    Interval _7246 = _11625;
                    Interval _11626 = iadd(_7242, _7246, intervalFailed);
                    Interval _7247 = _11626;
                    float _7223 = _7247.lo;
                    float _7224 = _7247.hi;
                    float _7218 = _7223;
                    float _7219 = _7224;
                    _7215.lo = _7218;
                    _7215.hi = _7219;
                    Interval _7216 = _7215;
                    Interval _7220 = _7216;
                    Interval _11639 = isin_body(_7220, intervalFailed, optical_product_upper);
                    Interval _7217 = _11639;
                    interval_sine_upper = _7217.hi;
                    float _7221 = _7217.lo;
                    float _7222 = _7221;
                    float _7225 = _7222;
                    float _7226 = interval_sine_upper;
                    _7213.lo = _7225;
                    _7213.hi = _7226;
                    Interval _7214 = _7213;
                    Interval _7227 = _7214;
                    Interval _7248 = _7227;
                    Interval _7934 = _7248;
                    Interval _11655 = imul(_7932, _7934, intervalFailed, optical_product_upper);
                    Interval _7935 = _11655;
                    Interval _7936 = _7840;
                    Interval _11657 = imul(_7935, _7936, intervalFailed, optical_product_upper);
                    Interval _7937 = _11657;
                    Interval _7209 = _7929;
                    Interval _7210 = _7937;
                    float _7206 = as_type<float>(as_type<uint>(_7210.hi) ^ 2147483648u);
                    float _7207 = as_type<float>(as_type<uint>(_7210.lo) ^ 2147483648u);
                    _7204.lo = _7206;
                    _7204.hi = _7207;
                    Interval _7205 = _7204;
                    Interval _7208 = _7205;
                    Interval _7211 = _7208;
                    Interval _11677 = iadd(_7209, _7211, intervalFailed);
                    Interval _7212 = _11677;
                    Interval _7938 = _7212;
                    float _7939 = 110.0;
                    float _7940 = 1000.0;
                    Interval _11679 = iratio(_7939, _7940, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7941 = _11679;
                    Interval _7942 = _7818;
                    Interval _7197 = _7942;
                    float _7195 = 3.1415927410125732421875;
                    float _7190 = _7195;
                    float _11683 = interval_down(_7190, intervalFailed);
                    float _7191 = _11683;
                    float _7192 = _7195;
                    float _11685 = interval_up(_7192, intervalFailed);
                    float _7193 = _11685;
                    _7188.lo = _7191;
                    _7188.hi = _7193;
                    Interval _7189 = _7188;
                    Interval _7194 = _7189;
                    Interval _7196 = _7194;
                    Interval _7198 = _7196;
                    float _7199 = 0.5;
                    float _7185 = _7199;
                    float _7186 = _7199;
                    _7183.lo = _7185;
                    _7183.hi = _7186;
                    Interval _7184 = _7183;
                    Interval _7187 = _7184;
                    Interval _7200 = _7187;
                    Interval _11703 = imul(_7198, _7200, intervalFailed, optical_product_upper);
                    Interval _7201 = _11703;
                    Interval _11704 = iadd(_7197, _7201, intervalFailed);
                    Interval _7202 = _11704;
                    float _7178 = _7202.lo;
                    float _7179 = _7202.hi;
                    float _7173 = _7178;
                    float _7174 = _7179;
                    _7170.lo = _7173;
                    _7170.hi = _7174;
                    Interval _7171 = _7170;
                    Interval _7175 = _7171;
                    Interval _11717 = isin_body(_7175, intervalFailed, optical_product_upper);
                    Interval _7172 = _11717;
                    interval_sine_upper = _7172.hi;
                    float _7176 = _7172.lo;
                    float _7177 = _7176;
                    float _7180 = _7177;
                    float _7181 = interval_sine_upper;
                    _7168.lo = _7180;
                    _7168.hi = _7181;
                    Interval _7169 = _7168;
                    Interval _7182 = _7169;
                    Interval _7203 = _7182;
                    Interval _7943 = _7203;
                    Interval _11733 = imul(_7941, _7943, intervalFailed, optical_product_upper);
                    Interval _7944 = _11733;
                    Interval _7945 = _7845;
                    Interval _11735 = imul(_7944, _7945, intervalFailed, optical_product_upper);
                    Interval _7946 = _11735;
                    Interval _11736 = iadd(_7938, _7946, intervalFailed);
                    Interval _7912 = _11736;
                    Interval _7948 = _8509;
                    float _7949 = 173.0;
                    float _7950 = 1000.0;
                    Interval _11738 = iratio(_7949, _7950, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7951 = _11738;
                    Interval _11739 = imul(_7948, _7951, intervalFailed, optical_product_upper);
                    Interval _7952 = _11739;
                    Interval _7953 = _8510;
                    float _7954 = 129.0;
                    float _7955 = 1000.0;
                    Interval _11741 = iratio(_7954, _7955, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7956 = _11741;
                    Interval _11742 = imul(_7953, _7956, intervalFailed, optical_product_upper);
                    Interval _7957 = _11742;
                    Interval _11743 = iadd(_7952, _7957, intervalFailed);
                    Interval _7958 = _11743;
                    Interval _7959 = _8511;
                    float _7960 = 73.0;
                    float _7961 = 100.0;
                    Interval _11745 = iratio(_7960, _7961, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7962 = _11745;
                    Interval _11746 = imul(_7959, _7962, intervalFailed, optical_product_upper);
                    Interval _7963 = _11746;
                    Interval _7164 = _7958;
                    Interval _7165 = _7963;
                    float _7161 = as_type<float>(as_type<uint>(_7165.hi) ^ 2147483648u);
                    float _7162 = as_type<float>(as_type<uint>(_7165.lo) ^ 2147483648u);
                    _7159.lo = _7161;
                    _7159.hi = _7162;
                    Interval _7160 = _7159;
                    Interval _7163 = _7160;
                    Interval _7166 = _7163;
                    Interval _11766 = iadd(_7164, _7166, intervalFailed);
                    Interval _7167 = _11766;
                    Interval _7947 = _7167;
                    Interval _7965 = _8512;
                    float _7966 = 216.0;
                    float _7967 = 1000.0;
                    Interval _11769 = iratio(_7966, _7967, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7968 = _11769;
                    Interval _7148 = _7965;
                    Interval _7149 = _7968;
                    Interval _11772 = imul(_7148, _7149, intervalFailed, optical_product_upper);
                    Interval _7150 = _11772;
                    bool _7140 = false;
                    if (_7150.lo <= 0.0)
                    {
                        _7140 = _7150.hi >= 0.0;
                    }
                    if (_7140)
                    {
                        _7139 = 0.0;
                    }
                    else
                    {
                        _7139 = precise::min(abs(_7150.lo), abs(_7150.hi));
                    }
                    float _7138 = _7139;
                    float _7141 = precise::max(abs(_7150.lo), abs(_7150.hi));
                    float _7142 = spvFMul(_7138, _7138);
                    float _11802 = interval_down(_7142, intervalFailed);
                    float _7143 = precise::max(0.0, _11802);
                    float _7144 = spvFMul(_7141, _7141);
                    float _11806 = interval_up(_7144, intervalFailed);
                    float _7145 = _11806;
                    _7136.lo = _7143;
                    _7136.hi = _7145;
                    Interval _7137 = _7136;
                    Interval _7146 = _7137;
                    Interval _7147 = _7146;
                    float _7151 = 1.0;
                    float _7133 = _7151;
                    float _7134 = _7151;
                    _7131.lo = _7133;
                    _7131.hi = _7134;
                    Interval _7132 = _7131;
                    Interval _7135 = _7132;
                    Interval _7152 = _7135;
                    float _7153 = 1.0;
                    float _7128 = _7153;
                    float _7129 = _7153;
                    _7126.lo = _7128;
                    _7126.hi = _7129;
                    Interval _7127 = _7126;
                    Interval _7130 = _7127;
                    Interval _7154 = _7130;
                    Interval _7155 = _7147;
                    bool _7119 = false;
                    if (_7155.lo <= 0.0)
                    {
                        _7119 = _7155.hi >= 0.0;
                    }
                    if (_7119)
                    {
                        _7118 = 0.0;
                    }
                    else
                    {
                        _7118 = precise::min(abs(_7155.lo), abs(_7155.hi));
                    }
                    float _7117 = _7118;
                    float _7120 = precise::max(abs(_7155.lo), abs(_7155.hi));
                    float _7121 = spvFMul(_7117, _7117);
                    float _11862 = interval_down(_7121, intervalFailed);
                    float _7122 = precise::max(0.0, _11862);
                    float _7123 = spvFMul(_7120, _7120);
                    float _11866 = interval_up(_7123, intervalFailed);
                    float _7124 = _11866;
                    _7115.lo = _7122;
                    _7115.hi = _7124;
                    Interval _7116 = _7115;
                    Interval _7125 = _7116;
                    Interval _7156 = _7125;
                    Interval _11874 = iadd(_7154, _7156, intervalFailed);
                    Interval _7157 = _11874;
                    Interval _11875 = idiv(_7152, _7157, intervalFailed, interval_divide_upper);
                    Interval _7158 = _11875;
                    Interval _7964 = _7158;
                    Interval _7969 = _7850;
                    float _7970 = 38.0;
                    float _7971 = 100.0;
                    Interval _11878 = iratio(_7970, _7971, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7972 = _11878;
                    Interval _7973 = _7947;
                    float _7110 = _7973.lo;
                    float _7111 = _7973.hi;
                    float _7105 = _7110;
                    float _7106 = _7111;
                    _7102.lo = _7105;
                    _7102.hi = _7106;
                    Interval _7103 = _7102;
                    Interval _7107 = _7103;
                    Interval _11892 = isin_body(_7107, intervalFailed, optical_product_upper);
                    Interval _7104 = _11892;
                    interval_sine_upper = _7104.hi;
                    float _7108 = _7104.lo;
                    float _7109 = _7108;
                    float _7112 = _7109;
                    float _7113 = interval_sine_upper;
                    _7100.lo = _7112;
                    _7100.hi = _7113;
                    Interval _7101 = _7100;
                    Interval _7114 = _7101;
                    Interval _7974 = _7114;
                    Interval _11907 = imul(_7972, _7974, intervalFailed, optical_product_upper);
                    Interval _7975 = _11907;
                    Interval _7976 = _7964;
                    Interval _11909 = imul(_7975, _7976, intervalFailed, optical_product_upper);
                    Interval _7977 = _11909;
                    Interval _11910 = iadd(_7969, _7977, intervalFailed);
                    _7850 = _11910;
                    Interval _7978 = _7877;
                    float _7979 = 6574.0;
                    float _7980 = 100000.0;
                    Interval _11912 = iratio(_7979, _7980, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7981 = _11912;
                    Interval _7982 = _7947;
                    Interval _7093 = _7982;
                    float _7091 = 3.1415927410125732421875;
                    float _7086 = _7091;
                    float _11916 = interval_down(_7086, intervalFailed);
                    float _7087 = _11916;
                    float _7088 = _7091;
                    float _11918 = interval_up(_7088, intervalFailed);
                    float _7089 = _11918;
                    _7084.lo = _7087;
                    _7084.hi = _7089;
                    Interval _7085 = _7084;
                    Interval _7090 = _7085;
                    Interval _7092 = _7090;
                    Interval _7094 = _7092;
                    float _7095 = 0.5;
                    float _7081 = _7095;
                    float _7082 = _7095;
                    _7079.lo = _7081;
                    _7079.hi = _7082;
                    Interval _7080 = _7079;
                    Interval _7083 = _7080;
                    Interval _7096 = _7083;
                    Interval _11936 = imul(_7094, _7096, intervalFailed, optical_product_upper);
                    Interval _7097 = _11936;
                    Interval _11937 = iadd(_7093, _7097, intervalFailed);
                    Interval _7098 = _11937;
                    float _7074 = _7098.lo;
                    float _7075 = _7098.hi;
                    float _7069 = _7074;
                    float _7070 = _7075;
                    _7066.lo = _7069;
                    _7066.hi = _7070;
                    Interval _7067 = _7066;
                    Interval _7071 = _7067;
                    Interval _11950 = isin_body(_7071, intervalFailed, optical_product_upper);
                    Interval _7068 = _11950;
                    interval_sine_upper = _7068.hi;
                    float _7072 = _7068.lo;
                    float _7073 = _7072;
                    float _7076 = _7073;
                    float _7077 = interval_sine_upper;
                    _7064.lo = _7076;
                    _7064.hi = _7077;
                    Interval _7065 = _7064;
                    Interval _7078 = _7065;
                    Interval _7099 = _7078;
                    Interval _7983 = _7099;
                    Interval _11966 = imul(_7981, _7983, intervalFailed, optical_product_upper);
                    Interval _7984 = _11966;
                    Interval _7985 = _7964;
                    Interval _11968 = imul(_7984, _7985, intervalFailed, optical_product_upper);
                    Interval _7986 = _11968;
                    Interval _11969 = iadd(_7978, _7986, intervalFailed);
                    _7877 = _11969;
                    Interval _7987 = _7912;
                    float _7988 = 4902.0;
                    float _7989 = 100000.0;
                    Interval _11971 = iratio(_7988, _7989, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7990 = _11971;
                    Interval _7991 = _7947;
                    Interval _7057 = _7991;
                    float _7055 = 3.1415927410125732421875;
                    float _7050 = _7055;
                    float _11975 = interval_down(_7050, intervalFailed);
                    float _7051 = _11975;
                    float _7052 = _7055;
                    float _11977 = interval_up(_7052, intervalFailed);
                    float _7053 = _11977;
                    _7048.lo = _7051;
                    _7048.hi = _7053;
                    Interval _7049 = _7048;
                    Interval _7054 = _7049;
                    Interval _7056 = _7054;
                    Interval _7058 = _7056;
                    float _7059 = 0.5;
                    float _7045 = _7059;
                    float _7046 = _7059;
                    _7043.lo = _7045;
                    _7043.hi = _7046;
                    Interval _7044 = _7043;
                    Interval _7047 = _7044;
                    Interval _7060 = _7047;
                    Interval _11995 = imul(_7058, _7060, intervalFailed, optical_product_upper);
                    Interval _7061 = _11995;
                    Interval _11996 = iadd(_7057, _7061, intervalFailed);
                    Interval _7062 = _11996;
                    float _7038 = _7062.lo;
                    float _7039 = _7062.hi;
                    float _7033 = _7038;
                    float _7034 = _7039;
                    _7030.lo = _7033;
                    _7030.hi = _7034;
                    Interval _7031 = _7030;
                    Interval _7035 = _7031;
                    Interval _12009 = isin_body(_7035, intervalFailed, optical_product_upper);
                    Interval _7032 = _12009;
                    interval_sine_upper = _7032.hi;
                    float _7036 = _7032.lo;
                    float _7037 = _7036;
                    float _7040 = _7037;
                    float _7041 = interval_sine_upper;
                    _7028.lo = _7040;
                    _7028.hi = _7041;
                    Interval _7029 = _7028;
                    Interval _7042 = _7029;
                    Interval _7063 = _7042;
                    Interval _7992 = _7063;
                    Interval _12025 = imul(_7990, _7992, intervalFailed, optical_product_upper);
                    Interval _7993 = _12025;
                    Interval _7994 = _7964;
                    Interval _12027 = imul(_7993, _7994, intervalFailed, optical_product_upper);
                    Interval _7995 = _12027;
                    Interval _12028 = iadd(_7987, _7995, intervalFailed);
                    _7912 = _12028;
                    if (_8512.lo < 24.0)
                    {
                        Interval _7997 = _8509;
                        float _7998 = 128.0;
                        float _7025 = _7998;
                        float _7026 = _7998;
                        _7023.lo = _7025;
                        _7023.hi = _7026;
                        Interval _7024 = _7023;
                        Interval _7027 = _7024;
                        Interval _7999 = _7027;
                        Interval _12044 = idiv(_7997, _7999, intervalFailed, interval_divide_upper);
                        Interval _7996 = _12044;
                        Interval _8001 = _8510;
                        float _8002 = 128.0;
                        float _7020 = _8002;
                        float _7021 = _8002;
                        _7018.lo = _7020;
                        _7018.hi = _7021;
                        Interval _7019 = _7018;
                        Interval _7022 = _7019;
                        Interval _8003 = _7022;
                        Interval _12055 = idiv(_8001, _8003, intervalFailed, interval_divide_upper);
                        Interval _8000 = _12055;
                        float _12058 = floor(_7996.lo);
                        float _12061 = floor(_7996.hi);
                        bool _8004 = true;
                        if ((isunordered(_12058, _12061) || _12058 == _12061))
                        {
                            _8004 = floor(_8000.lo) != floor(_8000.hi);
                        }
                        if (_8004)
                        {
                            Interval _8005 = _7850;
                            float _8006 = 0.0;
                            float _8007 = 10.0;
                            _7016.lo = _8006;
                            _7016.hi = _8007;
                            Interval _7017 = _7016;
                            Interval _8008 = _7017;
                            Interval _12083 = iadd(_8005, _8008, intervalFailed);
                            _7850 = _12083;
                            Interval _8009 = _7877;
                            float _8010 = -256.0;
                            float _8011 = 256.0;
                            _7014.lo = _8010;
                            _7014.hi = _8011;
                            Interval _7015 = _7014;
                            Interval _8012 = _7015;
                            Interval _12091 = iadd(_8009, _8012, intervalFailed);
                            _7877 = _12091;
                            Interval _8013 = _7912;
                            float _8014 = -256.0;
                            float _8015 = 256.0;
                            _7012.lo = _8014;
                            _7012.hi = _8015;
                            Interval _7013 = _7012;
                            Interval _8016 = _7013;
                            Interval _12099 = iadd(_8013, _8016, intervalFailed);
                            _7912 = _12099;
                        }
                        else
                        {
                            int _8017 = int(floor(_7996.lo));
                            int _8018 = int(floor(_8000.lo));
                            int _8020 = _8017;
                            int _8021 = _8018;
                            uint _7008 = (uint(_8020) * 1597334677u) ^ (uint(_8021) * 3812015801u);
                            _7008 ^= (_7008 >> 16u);
                            _7008 *= 2246822519u;
                            _7008 ^= (_7008 >> 13u);
                            float _7009 = float(_7008 & 65535u);
                            float _7010 = 65535.0;
                            Interval _12127 = iratio(_7009, _7010, intervalFailed, optical_product_upper, interval_divide_upper);
                            Interval _7011 = _12127;
                            Interval _8019 = _7011;
                            if (_8019.hi > 0.63999998569488525390625)
                            {
                                Interval _8023 = _7996;
                                float _8024 = float(_8017);
                                float _7005 = _8024;
                                float _7006 = _8024;
                                _7003.lo = _7005;
                                _7003.hi = _7006;
                                Interval _7004 = _7003;
                                Interval _7007 = _7004;
                                Interval _8025 = _7007;
                                Interval _6999 = _8023;
                                Interval _7000 = _8025;
                                float _6996 = as_type<float>(as_type<uint>(_7000.hi) ^ 2147483648u);
                                float _6997 = as_type<float>(as_type<uint>(_7000.lo) ^ 2147483648u);
                                _6994.lo = _6996;
                                _6994.hi = _6997;
                                Interval _6995 = _6994;
                                Interval _6998 = _6995;
                                Interval _7001 = _6998;
                                Interval _12165 = iadd(_6999, _7001, intervalFailed);
                                Interval _7002 = _12165;
                                Interval _8026 = _7002;
                                float _8027 = 28.0;
                                float _8028 = 100.0;
                                Interval _12167 = iratio(_8027, _8028, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8029 = _12167;
                                float _8030 = 44.0;
                                float _8031 = 100.0;
                                Interval _12168 = iratio(_8030, _8031, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8032 = _12168;
                                int _8033 = _8017 + 19;
                                int _8034 = _8018;
                                uint _6990 = (uint(_8033) * 1597334677u) ^ (uint(_8034) * 3812015801u);
                                _6990 ^= (_6990 >> 16u);
                                _6990 *= 2246822519u;
                                _6990 ^= (_6990 >> 13u);
                                float _6991 = float(_6990 & 65535u);
                                float _6992 = 65535.0;
                                Interval _12188 = iratio(_6991, _6992, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6993 = _12188;
                                Interval _8035 = _6993;
                                Interval _12190 = imul(_8032, _8035, intervalFailed, optical_product_upper);
                                Interval _8036 = _12190;
                                Interval _12191 = iadd(_8029, _8036, intervalFailed);
                                Interval _8037 = _12191;
                                Interval _6986 = _8026;
                                Interval _6987 = _8037;
                                float _6983 = as_type<float>(as_type<uint>(_6987.hi) ^ 2147483648u);
                                float _6984 = as_type<float>(as_type<uint>(_6987.lo) ^ 2147483648u);
                                _6981.lo = _6983;
                                _6981.hi = _6984;
                                Interval _6982 = _6981;
                                Interval _6985 = _6982;
                                Interval _6988 = _6985;
                                Interval _12211 = iadd(_6986, _6988, intervalFailed);
                                Interval _6989 = _12211;
                                Interval _8022 = _6989;
                                Interval _8039 = _8000;
                                float _8040 = float(_8018);
                                float _6978 = _8040;
                                float _6979 = _8040;
                                _6976.lo = _6978;
                                _6976.hi = _6979;
                                Interval _6977 = _6976;
                                Interval _6980 = _6977;
                                Interval _8041 = _6980;
                                Interval _6972 = _8039;
                                Interval _6973 = _8041;
                                float _6969 = as_type<float>(as_type<uint>(_6973.hi) ^ 2147483648u);
                                float _6970 = as_type<float>(as_type<uint>(_6973.lo) ^ 2147483648u);
                                _6967.lo = _6969;
                                _6967.hi = _6970;
                                Interval _6968 = _6967;
                                Interval _6971 = _6968;
                                Interval _6974 = _6971;
                                Interval _12244 = iadd(_6972, _6974, intervalFailed);
                                Interval _6975 = _12244;
                                Interval _8042 = _6975;
                                float _8043 = 28.0;
                                float _8044 = 100.0;
                                Interval _12246 = iratio(_8043, _8044, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8045 = _12246;
                                float _8046 = 44.0;
                                float _8047 = 100.0;
                                Interval _12247 = iratio(_8046, _8047, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8048 = _12247;
                                int _8049 = _8017;
                                int _8050 = _8018 + 29;
                                uint _6963 = (uint(_8049) * 1597334677u) ^ (uint(_8050) * 3812015801u);
                                _6963 ^= (_6963 >> 16u);
                                _6963 *= 2246822519u;
                                _6963 ^= (_6963 >> 13u);
                                float _6964 = float(_6963 & 65535u);
                                float _6965 = 65535.0;
                                Interval _12267 = iratio(_6964, _6965, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6966 = _12267;
                                Interval _8051 = _6966;
                                Interval _12269 = imul(_8048, _8051, intervalFailed, optical_product_upper);
                                Interval _8052 = _12269;
                                Interval _12270 = iadd(_8045, _8052, intervalFailed);
                                Interval _8053 = _12270;
                                Interval _6959 = _8042;
                                Interval _6960 = _8053;
                                float _6956 = as_type<float>(as_type<uint>(_6960.hi) ^ 2147483648u);
                                float _6957 = as_type<float>(as_type<uint>(_6960.lo) ^ 2147483648u);
                                _6954.lo = _6956;
                                _6954.hi = _6957;
                                Interval _6955 = _6954;
                                Interval _6958 = _6955;
                                Interval _6961 = _6958;
                                Interval _12290 = iadd(_6959, _6961, intervalFailed);
                                Interval _6962 = _12290;
                                Interval _8038 = _6962;
                                Interval _8055 = _8511;
                                float _8056 = 14.0;
                                float _8057 = 100.0;
                                Interval _12293 = iratio(_8056, _8057, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8058 = _12293;
                                Interval _12294 = imul(_8055, _8058, intervalFailed, optical_product_upper);
                                Interval _8059 = _12294;
                                Interval _8060 = _8019;
                                float _8061 = 7.0;
                                float _6951 = _8061;
                                float _6952 = _8061;
                                _6949.lo = _6951;
                                _6949.hi = _6952;
                                Interval _6950 = _6949;
                                Interval _6953 = _6950;
                                Interval _8062 = _6953;
                                Interval _12305 = imul(_8060, _8062, intervalFailed, optical_product_upper);
                                Interval _8063 = _12305;
                                Interval _12306 = iadd(_8059, _8063, intervalFailed);
                                Interval _8054 = _12306;
                                if (floor(_8054.lo) == floor(_8054.hi))
                                {
                                    Interval _8064 = _8054;
                                    float _8065 = floor(_8054.lo);
                                    float _6946 = _8065;
                                    float _6947 = _8065;
                                    _6944.lo = _6946;
                                    _6944.hi = _6947;
                                    Interval _6945 = _6944;
                                    Interval _6948 = _6945;
                                    Interval _8066 = _6948;
                                    Interval _6940 = _8064;
                                    Interval _6941 = _8066;
                                    float _6937 = as_type<float>(as_type<uint>(_6941.hi) ^ 2147483648u);
                                    float _6938 = as_type<float>(as_type<uint>(_6941.lo) ^ 2147483648u);
                                    _6935.lo = _6937;
                                    _6935.hi = _6938;
                                    Interval _6936 = _6935;
                                    Interval _6939 = _6936;
                                    Interval _6942 = _6939;
                                    Interval _12349 = iadd(_6940, _6942, intervalFailed);
                                    Interval _6943 = _12349;
                                    _8054 = _6943;
                                }
                                else
                                {
                                    float _8067 = 0.0;
                                    float _8068 = 1.0;
                                    _6933.lo = _8067;
                                    _6933.hi = _8068;
                                    Interval _6934 = _6933;
                                    _8054 = _6934;
                                }
                                Interval _8070 = _8054;
                                float _8071 = 3.1415927410125732421875;
                                float _6928 = _8071;
                                float _12359 = interval_down(_6928, intervalFailed);
                                float _6929 = _12359;
                                float _6930 = _8071;
                                float _12361 = interval_up(_6930, intervalFailed);
                                float _6931 = _12361;
                                _6926.lo = _6929;
                                _6926.hi = _6931;
                                Interval _6927 = _6926;
                                Interval _6932 = _6927;
                                Interval _8072 = _6932;
                                Interval _12369 = imul(_8070, _8072, intervalFailed, optical_product_upper);
                                Interval _8073 = _12369;
                                float _6921 = _8073.lo;
                                float _6922 = _8073.hi;
                                float _6916 = _6921;
                                float _6917 = _6922;
                                _6913.lo = _6916;
                                _6913.hi = _6917;
                                Interval _6914 = _6913;
                                Interval _6918 = _6914;
                                Interval _12382 = isin_body(_6918, intervalFailed, optical_product_upper);
                                Interval _6915 = _12382;
                                interval_sine_upper = _6915.hi;
                                float _6919 = _6915.lo;
                                float _6920 = _6919;
                                float _6923 = _6920;
                                float _6924 = interval_sine_upper;
                                _6911.lo = _6923;
                                _6911.hi = _6924;
                                Interval _6912 = _6911;
                                Interval _6925 = _6912;
                                Interval _8074 = _6925;
                                bool _6904 = false;
                                if (_8074.lo <= 0.0)
                                {
                                    _6904 = _8074.hi >= 0.0;
                                }
                                if (_6904)
                                {
                                    _6903 = 0.0;
                                }
                                else
                                {
                                    _6903 = precise::min(abs(_8074.lo), abs(_8074.hi));
                                }
                                float _6902 = _6903;
                                float _6905 = precise::max(abs(_8074.lo), abs(_8074.hi));
                                float _6906 = spvFMul(_6902, _6902);
                                float _12426 = interval_down(_6906, intervalFailed);
                                float _6907 = precise::max(0.0, _12426);
                                float _6908 = spvFMul(_6905, _6905);
                                float _12430 = interval_up(_6908, intervalFailed);
                                float _6909 = _12430;
                                _6900.lo = _6907;
                                _6900.hi = _6909;
                                Interval _6901 = _6900;
                                Interval _6910 = _6901;
                                Interval _8069 = _6910;
                                float _8076 = 55.0;
                                float _8077 = 1000.0;
                                Interval _12438 = iratio(_8076, _8077, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8078 = _12438;
                                float _8079 = 14.0;
                                float _8080 = 100.0;
                                Interval _12439 = iratio(_8079, _8080, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8081 = _12439;
                                Interval _8082 = _8054;
                                Interval _12441 = imul(_8081, _8082, intervalFailed, optical_product_upper);
                                Interval _8083 = _12441;
                                Interval _12442 = iadd(_8078, _8083, intervalFailed);
                                Interval _8075 = _12442;
                                Interval _8085 = _8075;
                                bool _6893 = false;
                                if (_8085.lo <= 0.0)
                                {
                                    _6893 = _8085.hi >= 0.0;
                                }
                                if (_6893)
                                {
                                    _6892 = 0.0;
                                }
                                else
                                {
                                    _6892 = precise::min(abs(_8085.lo), abs(_8085.hi));
                                }
                                float _6891 = _6892;
                                float _6894 = precise::max(abs(_8085.lo), abs(_8085.hi));
                                float _6895 = spvFMul(_6891, _6891);
                                float _12473 = interval_down(_6895, intervalFailed);
                                float _6896 = precise::max(0.0, _12473);
                                float _6897 = spvFMul(_6894, _6894);
                                float _12477 = interval_up(_6897, intervalFailed);
                                float _6898 = _12477;
                                _6889.lo = _6896;
                                _6889.hi = _6898;
                                Interval _6890 = _6889;
                                Interval _6899 = _6890;
                                Interval _8084 = _6899;
                                float _8087 = 1.0;
                                float _6886 = _8087;
                                float _6887 = _8087;
                                _6884.lo = _6886;
                                _6884.hi = _6887;
                                Interval _6885 = _6884;
                                Interval _6888 = _6885;
                                Interval _8088 = _6888;
                                Interval _8089 = _8022;
                                bool _6877 = false;
                                if (_8089.lo <= 0.0)
                                {
                                    _6877 = _8089.hi >= 0.0;
                                }
                                if (_6877)
                                {
                                    _6876 = 0.0;
                                }
                                else
                                {
                                    _6876 = precise::min(abs(_8089.lo), abs(_8089.hi));
                                }
                                float _6875 = _6876;
                                float _6878 = precise::max(abs(_8089.lo), abs(_8089.hi));
                                float _6879 = spvFMul(_6875, _6875);
                                float _12524 = interval_down(_6879, intervalFailed);
                                float _6880 = precise::max(0.0, _12524);
                                float _6881 = spvFMul(_6878, _6878);
                                float _12528 = interval_up(_6881, intervalFailed);
                                float _6882 = _12528;
                                _6873.lo = _6880;
                                _6873.hi = _6882;
                                Interval _6874 = _6873;
                                Interval _6883 = _6874;
                                Interval _8090 = _6883;
                                Interval _8091 = _8038;
                                bool _6866 = false;
                                if (_8091.lo <= 0.0)
                                {
                                    _6866 = _8091.hi >= 0.0;
                                }
                                if (_6866)
                                {
                                    _6865 = 0.0;
                                }
                                else
                                {
                                    _6865 = precise::min(abs(_8091.lo), abs(_8091.hi));
                                }
                                float _6864 = _6865;
                                float _6867 = precise::max(abs(_8091.lo), abs(_8091.hi));
                                float _6868 = spvFMul(_6864, _6864);
                                float _12566 = interval_down(_6868, intervalFailed);
                                float _6869 = precise::max(0.0, _12566);
                                float _6870 = spvFMul(_6867, _6867);
                                float _12570 = interval_up(_6870, intervalFailed);
                                float _6871 = _12570;
                                _6862.lo = _6869;
                                _6862.hi = _6871;
                                Interval _6863 = _6862;
                                Interval _6872 = _6863;
                                Interval _8092 = _6872;
                                Interval _12578 = iadd(_8090, _8092, intervalFailed);
                                Interval _8093 = _12578;
                                Interval _8094 = _8084;
                                Interval _12580 = idiv(_8093, _8094, intervalFailed, interval_divide_upper);
                                Interval _8095 = _12580;
                                Interval _6858 = _8088;
                                Interval _6859 = _8095;
                                float _6855 = as_type<float>(as_type<uint>(_6859.hi) ^ 2147483648u);
                                float _6856 = as_type<float>(as_type<uint>(_6859.lo) ^ 2147483648u);
                                _6853.lo = _6855;
                                _6853.hi = _6856;
                                Interval _6854 = _6853;
                                Interval _6857 = _6854;
                                Interval _6860 = _6857;
                                Interval _12600 = iadd(_6858, _6860, intervalFailed);
                                Interval _6861 = _12600;
                                Interval _8096 = _6861;
                                float _8097 = 0.0;
                                float _6850 = _8097;
                                float _6851 = _8097;
                                _6848.lo = _6850;
                                _6848.hi = _6851;
                                Interval _6849 = _6848;
                                Interval _6852 = _6849;
                                Interval _8098 = _6852;
                                float _8099 = 1.0;
                                float _6845 = _8099;
                                float _6846 = _8099;
                                _6843.lo = _6845;
                                _6843.hi = _6846;
                                Interval _6844 = _6843;
                                Interval _6847 = _6844;
                                Interval _8100 = _6847;
                                Interval _6838 = _8096;
                                Interval _6839 = _8098;
                                float _6835 = precise::max(_6838.lo, _6839.lo);
                                float _6836 = precise::max(_6838.hi, _6839.hi);
                                _6833.lo = _6835;
                                _6833.hi = _6836;
                                Interval _6834 = _6833;
                                Interval _6837 = _6834;
                                Interval _6840 = _6837;
                                Interval _6841 = _8100;
                                float _6830 = precise::min(_6840.lo, _6841.lo);
                                float _6831 = precise::min(_6840.hi, _6841.hi);
                                _6828.lo = _6830;
                                _6828.hi = _6831;
                                Interval _6829 = _6828;
                                Interval _6832 = _6829;
                                Interval _6842 = _6832;
                                Interval _8086 = _6842;
                                float _8102 = 10.0;
                                float _6825 = _8102;
                                float _6826 = _8102;
                                _6823.lo = _6825;
                                _6823.hi = _6826;
                                Interval _6824 = _6823;
                                Interval _6827 = _6824;
                                Interval _8103 = _6827;
                                Interval _8104 = _8069;
                                Interval _12668 = imul(_8103, _8104, intervalFailed, optical_product_upper);
                                Interval _8105 = _12668;
                                Interval _8106 = _8512;
                                float _8107 = 18.0;
                                float _8108 = 100.0;
                                Interval _12670 = iratio(_8107, _8108, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8109 = _12670;
                                Interval _6812 = _8106;
                                Interval _6813 = _8109;
                                Interval _12673 = imul(_6812, _6813, intervalFailed, optical_product_upper);
                                Interval _6814 = _12673;
                                bool _6804 = false;
                                if (_6814.lo <= 0.0)
                                {
                                    _6804 = _6814.hi >= 0.0;
                                }
                                if (_6804)
                                {
                                    _6803 = 0.0;
                                }
                                else
                                {
                                    _6803 = precise::min(abs(_6814.lo), abs(_6814.hi));
                                }
                                float _6802 = _6803;
                                float _6805 = precise::max(abs(_6814.lo), abs(_6814.hi));
                                float _6806 = spvFMul(_6802, _6802);
                                float _12703 = interval_down(_6806, intervalFailed);
                                float _6807 = precise::max(0.0, _12703);
                                float _6808 = spvFMul(_6805, _6805);
                                float _12707 = interval_up(_6808, intervalFailed);
                                float _6809 = _12707;
                                _6800.lo = _6807;
                                _6800.hi = _6809;
                                Interval _6801 = _6800;
                                Interval _6810 = _6801;
                                Interval _6811 = _6810;
                                float _6815 = 1.0;
                                float _6797 = _6815;
                                float _6798 = _6815;
                                _6795.lo = _6797;
                                _6795.hi = _6798;
                                Interval _6796 = _6795;
                                Interval _6799 = _6796;
                                Interval _6816 = _6799;
                                float _6817 = 1.0;
                                float _6792 = _6817;
                                float _6793 = _6817;
                                _6790.lo = _6792;
                                _6790.hi = _6793;
                                Interval _6791 = _6790;
                                Interval _6794 = _6791;
                                Interval _6818 = _6794;
                                Interval _6819 = _6811;
                                bool _6783 = false;
                                if (_6819.lo <= 0.0)
                                {
                                    _6783 = _6819.hi >= 0.0;
                                }
                                if (_6783)
                                {
                                    _6782 = 0.0;
                                }
                                else
                                {
                                    _6782 = precise::min(abs(_6819.lo), abs(_6819.hi));
                                }
                                float _6781 = _6782;
                                float _6784 = precise::max(abs(_6819.lo), abs(_6819.hi));
                                float _6785 = spvFMul(_6781, _6781);
                                float _12763 = interval_down(_6785, intervalFailed);
                                float _6786 = precise::max(0.0, _12763);
                                float _6787 = spvFMul(_6784, _6784);
                                float _12767 = interval_up(_6787, intervalFailed);
                                float _6788 = _12767;
                                _6779.lo = _6786;
                                _6779.hi = _6788;
                                Interval _6780 = _6779;
                                Interval _6789 = _6780;
                                Interval _6820 = _6789;
                                Interval _12775 = iadd(_6818, _6820, intervalFailed);
                                Interval _6821 = _12775;
                                Interval _12776 = idiv(_6816, _6821, intervalFailed, interval_divide_upper);
                                Interval _6822 = _12776;
                                Interval _8110 = _6822;
                                Interval _12778 = imul(_8105, _8110, intervalFailed, optical_product_upper);
                                Interval _8101 = _12778;
                                Interval _8112 = _8086;
                                bool _6772 = false;
                                if (_8112.lo <= 0.0)
                                {
                                    _6772 = _8112.hi >= 0.0;
                                }
                                if (_6772)
                                {
                                    _6771 = 0.0;
                                }
                                else
                                {
                                    _6771 = precise::min(abs(_8112.lo), abs(_8112.hi));
                                }
                                float _6770 = _6771;
                                float _6773 = precise::max(abs(_8112.lo), abs(_8112.hi));
                                float _6774 = spvFMul(_6770, _6770);
                                float _12809 = interval_down(_6774, intervalFailed);
                                float _6775 = precise::max(0.0, _12809);
                                float _6776 = spvFMul(_6773, _6773);
                                float _12813 = interval_up(_6776, intervalFailed);
                                float _6777 = _12813;
                                _6768.lo = _6775;
                                _6768.hi = _6777;
                                Interval _6769 = _6768;
                                Interval _6778 = _6769;
                                Interval _8111 = _6778;
                                Interval _8114 = _8101;
                                Interval _8115 = _8111;
                                Interval _12823 = imul(_8114, _8115, intervalFailed, optical_product_upper);
                                Interval _8116 = _12823;
                                Interval _8117 = _8086;
                                Interval _12825 = imul(_8116, _8117, intervalFailed, optical_product_upper);
                                Interval _8113 = _12825;
                                float _8119 = -6.0;
                                float _6765 = _8119;
                                float _6766 = _8119;
                                _6763.lo = _6765;
                                _6763.hi = _6766;
                                Interval _6764 = _6763;
                                Interval _6767 = _6764;
                                Interval _8120 = _6767;
                                Interval _8121 = _8101;
                                Interval _12836 = imul(_8120, _8121, intervalFailed, optical_product_upper);
                                Interval _8122 = _12836;
                                Interval _8123 = _8111;
                                Interval _12838 = imul(_8122, _8123, intervalFailed, optical_product_upper);
                                Interval _8124 = _12838;
                                float _8125 = 128.0;
                                float _6760 = _8125;
                                float _6761 = _8125;
                                _6758.lo = _6760;
                                _6758.hi = _6761;
                                Interval _6759 = _6758;
                                Interval _6762 = _6759;
                                Interval _8126 = _6762;
                                Interval _8127 = _8084;
                                Interval _12849 = imul(_8126, _8127, intervalFailed, optical_product_upper);
                                Interval _8128 = _12849;
                                Interval _12850 = idiv(_8124, _8128, intervalFailed, interval_divide_upper);
                                Interval _8118 = _12850;
                                Interval _8130 = _8118;
                                Interval _8131 = _8022;
                                Interval _12853 = imul(_8130, _8131, intervalFailed, optical_product_upper);
                                Interval _8129 = _12853;
                                Interval _8133 = _8118;
                                Interval _8134 = _8038;
                                Interval _12856 = imul(_8133, _8134, intervalFailed, optical_product_upper);
                                Interval _8132 = _12856;
                                bool _8135 = true;
                                if ((isunordered(_8019.lo, 0.63999998569488525390625) || _8019.lo > 0.63999998569488525390625))
                                {
                                    _8135 = _8512.hi >= 24.0;
                                }
                                if (_8135)
                                {
                                    Interval _8136 = _8113;
                                    float _8137 = 0.0;
                                    float _6755 = _8137;
                                    float _6756 = _8137;
                                    _6753.lo = _6755;
                                    _6753.hi = _6756;
                                    Interval _6754 = _6753;
                                    Interval _6757 = _6754;
                                    Interval _8138 = _6757;
                                    float _6750 = precise::min(_8136.lo, _8138.lo);
                                    float _6751 = precise::max(_8136.hi, _8138.hi);
                                    _6748.lo = _6750;
                                    _6748.hi = _6751;
                                    Interval _6749 = _6748;
                                    Interval _6752 = _6749;
                                    _8113 = _6752;
                                    Interval _8139 = _8129;
                                    float _8140 = 0.0;
                                    float _6745 = _8140;
                                    float _6746 = _8140;
                                    _6743.lo = _6745;
                                    _6743.hi = _6746;
                                    Interval _6744 = _6743;
                                    Interval _6747 = _6744;
                                    Interval _8141 = _6747;
                                    float _6740 = precise::min(_8139.lo, _8141.lo);
                                    float _6741 = precise::max(_8139.hi, _8141.hi);
                                    _6738.lo = _6740;
                                    _6738.hi = _6741;
                                    Interval _6739 = _6738;
                                    Interval _6742 = _6739;
                                    _8129 = _6742;
                                    Interval _8142 = _8132;
                                    float _8143 = 0.0;
                                    float _6735 = _8143;
                                    float _6736 = _8143;
                                    _6733.lo = _6735;
                                    _6733.hi = _6736;
                                    Interval _6734 = _6733;
                                    Interval _6737 = _6734;
                                    Interval _8144 = _6737;
                                    float _6730 = precise::min(_8142.lo, _8144.lo);
                                    float _6731 = precise::max(_8142.hi, _8144.hi);
                                    _6728.lo = _6730;
                                    _6728.hi = _6731;
                                    Interval _6729 = _6728;
                                    Interval _6732 = _6729;
                                    _8132 = _6732;
                                }
                                Interval _8145 = _7850;
                                Interval _8146 = _8113;
                                Interval _12951 = iadd(_8145, _8146, intervalFailed);
                                _7850 = _12951;
                                Interval _8147 = _7877;
                                Interval _8148 = _8129;
                                Interval _12954 = iadd(_8147, _8148, intervalFailed);
                                _7877 = _12954;
                                Interval _8149 = _7912;
                                Interval _8150 = _8132;
                                Interval _12957 = iadd(_8149, _8150, intervalFailed);
                                _7912 = _12957;
                            }
                        }
                    }
                    Interval _8151 = _7850;
                    Interval _8152 = _7877;
                    Interval _8153 = _7912;
                    _6726.x = _8151;
                    _6726.y = _8152;
                    _6726.z = _8153;
                    Interval3 _6727 = _6726;
                    Interval3 _8154 = _6727;
                    Interval3 _8508 = _8154;
                    if (_8507 == 0u)
                    {
                        Interval _8514 = param_var_distance;
                        float _8515 = 2.0;
                        float _8516 = 10.0;
                        Interval _12976 = iratio(_8515, _8516, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8517 = _12976;
                        Interval _12977 = imul(_8514, _8517, intervalFailed, optical_product_upper);
                        Interval _8513 = _12977;
                        Interval _8519 = _8508.x;
                        float _6723 = as_type<float>(as_type<uint>(_8519.hi) ^ 2147483648u);
                        float _6724 = as_type<float>(as_type<uint>(_8519.lo) ^ 2147483648u);
                        _6721.lo = _6723;
                        _6721.hi = _6724;
                        Interval _6722 = _6721;
                        Interval _6725 = _6722;
                        Interval _8520 = _6725;
                        Interval _8521 = _8504.y;
                        float _8522 = 12.0;
                        float _8523 = 100.0;
                        Interval _12999 = iratio(_8522, _8523, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8524 = _12999;
                        float _6718 = precise::max(_8521.lo, _8524.lo);
                        float _6719 = precise::max(_8521.hi, _8524.hi);
                        _6716.lo = _6718;
                        _6716.hi = _6719;
                        Interval _6717 = _6716;
                        Interval _6720 = _6717;
                        Interval _8525 = _6720;
                        Interval _13017 = idiv(_8520, _8525, intervalFailed, interval_divide_upper);
                        Interval _8526 = _13017;
                        Interval _8527 = _8513;
                        float _6713 = as_type<float>(as_type<uint>(_8527.hi) ^ 2147483648u);
                        float _6714 = as_type<float>(as_type<uint>(_8527.lo) ^ 2147483648u);
                        _6711.lo = _6713;
                        _6711.hi = _6714;
                        Interval _6712 = _6711;
                        Interval _6715 = _6712;
                        Interval _8528 = _6715;
                        Interval _8529 = _8513;
                        Interval _6706 = _8526;
                        Interval _6707 = _8528;
                        float _6703 = precise::max(_6706.lo, _6707.lo);
                        float _6704 = precise::max(_6706.hi, _6707.hi);
                        _6701.lo = _6703;
                        _6701.hi = _6704;
                        Interval _6702 = _6701;
                        Interval _6705 = _6702;
                        Interval _6708 = _6705;
                        Interval _6709 = _8529;
                        float _6698 = precise::min(_6708.lo, _6709.lo);
                        float _6699 = precise::min(_6708.hi, _6709.hi);
                        _6696.lo = _6698;
                        _6696.hi = _6699;
                        Interval _6697 = _6696;
                        Interval _6700 = _6697;
                        Interval _6710 = _6700;
                        Interval _8518 = _6710;
                        Interval3 _8530 = _8493;
                        Interval3 _8531 = _8504;
                        Interval _8532 = _8518;
                        Interval _6686 = _8531.x;
                        Interval _6687 = _8532;
                        Interval _13081 = imul(_6686, _6687, intervalFailed, optical_product_upper);
                        Interval _6688 = _13081;
                        Interval _6689 = _8531.y;
                        Interval _6690 = _8532;
                        Interval _13085 = imul(_6689, _6690, intervalFailed, optical_product_upper);
                        Interval _6691 = _13085;
                        Interval _6692 = _8531.z;
                        Interval _6693 = _8532;
                        Interval _13089 = imul(_6692, _6693, intervalFailed, optical_product_upper);
                        Interval _6694 = _13089;
                        _6684.x = _6688;
                        _6684.y = _6691;
                        _6684.z = _6694;
                        Interval3 _6685 = _6684;
                        Interval3 _6695 = _6685;
                        Interval3 _8533 = _6695;
                        Interval _6674 = _8530.x;
                        Interval _6675 = _8533.x;
                        Interval _13103 = iadd(_6674, _6675, intervalFailed);
                        Interval _6676 = _13103;
                        Interval _6677 = _8530.y;
                        Interval _6678 = _8533.y;
                        Interval _13108 = iadd(_6677, _6678, intervalFailed);
                        Interval _6679 = _13108;
                        Interval _6680 = _8530.z;
                        Interval _6681 = _8533.z;
                        Interval _13113 = iadd(_6680, _6681, intervalFailed);
                        Interval _6682 = _13113;
                        _6672.x = _6676;
                        _6672.y = _6679;
                        _6672.z = _6682;
                        Interval3 _6673 = _6672;
                        Interval3 _6683 = _6673;
                        _8493 = _6683;
                        Interval3 _8534 = hit;
                        Interval3 _8535 = param_var_direction_1;
                        Interval _8536 = _8518;
                        Interval _6662 = _8535.x;
                        Interval _6663 = _8536;
                        Interval _13129 = imul(_6662, _6663, intervalFailed, optical_product_upper);
                        Interval _6664 = _13129;
                        Interval _6665 = _8535.y;
                        Interval _6666 = _8536;
                        Interval _13133 = imul(_6665, _6666, intervalFailed, optical_product_upper);
                        Interval _6667 = _13133;
                        Interval _6668 = _8535.z;
                        Interval _6669 = _8536;
                        Interval _13137 = imul(_6668, _6669, intervalFailed, optical_product_upper);
                        Interval _6670 = _13137;
                        _6660.x = _6664;
                        _6660.y = _6667;
                        _6660.z = _6670;
                        Interval3 _6661 = _6660;
                        Interval3 _6671 = _6661;
                        Interval3 _8537 = _6671;
                        Interval _6650 = _8534.x;
                        Interval _6651 = _8537.x;
                        Interval _13151 = iadd(_6650, _6651, intervalFailed);
                        Interval _6652 = _13151;
                        Interval _6653 = _8534.y;
                        Interval _6654 = _8537.y;
                        Interval _13156 = iadd(_6653, _6654, intervalFailed);
                        Interval _6655 = _13156;
                        Interval _6656 = _8534.z;
                        Interval _6657 = _8537.z;
                        Interval _13161 = iadd(_6656, _6657, intervalFailed);
                        Interval _6658 = _13161;
                        _6648.x = _6652;
                        _6648.y = _6655;
                        _6648.z = _6658;
                        Interval3 _6649 = _6648;
                        Interval3 _6659 = _6649;
                        hit = _6659;
                    }
                    else
                    {
                        _8501 = _8508.y;
                        _8502 = _8508.z;
                    }
                }
            }
            else
            {
                Interval _8539 = _8493.x;
                float _8540 = 18.0;
                float _8541 = 1000.0;
                Interval _13178 = iratio(_8540, _8541, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8542 = _13178;
                Interval _13179 = imul(_8539, _8542, intervalFailed, optical_product_upper);
                Interval _8543 = _13179;
                Interval _8544 = _8493.z;
                float _8545 = 11.0;
                float _8546 = 1000.0;
                Interval _13182 = iratio(_8545, _8546, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8547 = _13182;
                Interval _13183 = imul(_8544, _8547, intervalFailed, optical_product_upper);
                Interval _8548 = _13183;
                Interval _13184 = iadd(_8543, _8548, intervalFailed);
                Interval _8549 = _13184;
                Interval _8550 = _8499;
                float _8551 = 8.0;
                float _8552 = 10.0;
                Interval _13186 = iratio(_8551, _8552, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8553 = _13186;
                Interval _13187 = imul(_8550, _8553, intervalFailed, optical_product_upper);
                Interval _8554 = _13187;
                Interval _6644 = _8549;
                Interval _6645 = _8554;
                float _6641 = as_type<float>(as_type<uint>(_6645.hi) ^ 2147483648u);
                float _6642 = as_type<float>(as_type<uint>(_6645.lo) ^ 2147483648u);
                _6639.lo = _6641;
                _6639.hi = _6642;
                Interval _6640 = _6639;
                Interval _6643 = _6640;
                Interval _6646 = _6643;
                Interval _13207 = iadd(_6644, _6646, intervalFailed);
                Interval _6647 = _13207;
                Interval _8538 = _6647;
                Interval _8556 = _8493.x;
                float _8557 = 47.0;
                float _8558 = 1000.0;
                Interval _13211 = iratio(_8557, _8558, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8559 = _13211;
                Interval _13212 = imul(_8556, _8559, intervalFailed, optical_product_upper);
                Interval _8560 = _13212;
                Interval _8561 = _8493.z;
                float _8562 = 25.0;
                float _8563 = 1000.0;
                Interval _13215 = iratio(_8562, _8563, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8564 = _13215;
                Interval _13216 = imul(_8561, _8564, intervalFailed, optical_product_upper);
                Interval _8565 = _13216;
                Interval _6635 = _8560;
                Interval _6636 = _8565;
                float _6632 = as_type<float>(as_type<uint>(_6636.hi) ^ 2147483648u);
                float _6633 = as_type<float>(as_type<uint>(_6636.lo) ^ 2147483648u);
                _6630.lo = _6632;
                _6630.hi = _6633;
                Interval _6631 = _6630;
                Interval _6634 = _6631;
                Interval _6637 = _6634;
                Interval _13236 = iadd(_6635, _6637, intervalFailed);
                Interval _6638 = _13236;
                Interval _8566 = _6638;
                Interval _8567 = _8499;
                float _8568 = 12.0;
                float _8569 = 10.0;
                Interval _13239 = iratio(_8568, _8569, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8570 = _13239;
                Interval _13240 = imul(_8567, _8570, intervalFailed, optical_product_upper);
                Interval _8571 = _13240;
                Interval _13241 = iadd(_8566, _8571, intervalFailed);
                Interval _8555 = _13241;
                Interval _8573 = _8493.z;
                float _8574 = 22.0;
                float _8575 = 1000.0;
                Interval _13244 = iratio(_8574, _8575, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8576 = _13244;
                Interval _13245 = imul(_8573, _8576, intervalFailed, optical_product_upper);
                Interval _8577 = _13245;
                Interval _8578 = _8493.x;
                float _8579 = 9.0;
                float _8580 = 1000.0;
                Interval _13248 = iratio(_8579, _8580, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8581 = _13248;
                Interval _13249 = imul(_8578, _8581, intervalFailed, optical_product_upper);
                Interval _8582 = _13249;
                Interval _6626 = _8577;
                Interval _6627 = _8582;
                float _6623 = as_type<float>(as_type<uint>(_6627.hi) ^ 2147483648u);
                float _6624 = as_type<float>(as_type<uint>(_6627.lo) ^ 2147483648u);
                _6621.lo = _6623;
                _6621.hi = _6624;
                Interval _6622 = _6621;
                Interval _6625 = _6622;
                Interval _6628 = _6625;
                Interval _13269 = iadd(_6626, _6628, intervalFailed);
                Interval _6629 = _13269;
                Interval _8583 = _6629;
                Interval _8584 = _8499;
                float _8585 = 65.0;
                float _8586 = 100.0;
                Interval _13272 = iratio(_8585, _8586, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8587 = _13272;
                Interval _13273 = imul(_8584, _8587, intervalFailed, optical_product_upper);
                Interval _8588 = _13273;
                Interval _6617 = _8583;
                Interval _6618 = _8588;
                float _6614 = as_type<float>(as_type<uint>(_6618.hi) ^ 2147483648u);
                float _6615 = as_type<float>(as_type<uint>(_6618.lo) ^ 2147483648u);
                _6612.lo = _6614;
                _6612.hi = _6615;
                Interval _6613 = _6612;
                Interval _6616 = _6613;
                Interval _6619 = _6616;
                Interval _13293 = iadd(_6617, _6619, intervalFailed);
                Interval _6620 = _13293;
                Interval _8572 = _6620;
                float _8589 = 55.0;
                float _8590 = 1000.0;
                Interval _13295 = iratio(_8589, _8590, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8591 = _13295;
                Interval _8592 = _8538;
                Interval _6605 = _8592;
                float _6603 = 3.1415927410125732421875;
                float _6598 = _6603;
                float _13299 = interval_down(_6598, intervalFailed);
                float _6599 = _13299;
                float _6600 = _6603;
                float _13301 = interval_up(_6600, intervalFailed);
                float _6601 = _13301;
                _6596.lo = _6599;
                _6596.hi = _6601;
                Interval _6597 = _6596;
                Interval _6602 = _6597;
                Interval _6604 = _6602;
                Interval _6606 = _6604;
                float _6607 = 0.5;
                float _6593 = _6607;
                float _6594 = _6607;
                _6591.lo = _6593;
                _6591.hi = _6594;
                Interval _6592 = _6591;
                Interval _6595 = _6592;
                Interval _6608 = _6595;
                Interval _13319 = imul(_6606, _6608, intervalFailed, optical_product_upper);
                Interval _6609 = _13319;
                Interval _13320 = iadd(_6605, _6609, intervalFailed);
                Interval _6610 = _13320;
                float _6586 = _6610.lo;
                float _6587 = _6610.hi;
                float _6581 = _6586;
                float _6582 = _6587;
                _6578.lo = _6581;
                _6578.hi = _6582;
                Interval _6579 = _6578;
                Interval _6583 = _6579;
                Interval _13333 = isin_body(_6583, intervalFailed, optical_product_upper);
                Interval _6580 = _13333;
                interval_sine_upper = _6580.hi;
                float _6584 = _6580.lo;
                float _6585 = _6584;
                float _6588 = _6585;
                float _6589 = interval_sine_upper;
                _6576.lo = _6588;
                _6576.hi = _6589;
                Interval _6577 = _6576;
                Interval _6590 = _6577;
                Interval _6611 = _6590;
                Interval _8593 = _6611;
                Interval _13349 = imul(_8591, _8593, intervalFailed, optical_product_upper);
                Interval _8594 = _13349;
                Interval _8595 = param_var_footprint;
                float _8596 = 22.0;
                float _8597 = 1000.0;
                Interval _13351 = iratio(_8596, _8597, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8598 = _13351;
                Interval _6565 = _8595;
                Interval _6566 = _8598;
                Interval _13354 = imul(_6565, _6566, intervalFailed, optical_product_upper);
                Interval _6567 = _13354;
                bool _6557 = false;
                if (_6567.lo <= 0.0)
                {
                    _6557 = _6567.hi >= 0.0;
                }
                if (_6557)
                {
                    _6556 = 0.0;
                }
                else
                {
                    _6556 = precise::min(abs(_6567.lo), abs(_6567.hi));
                }
                float _6555 = _6556;
                float _6558 = precise::max(abs(_6567.lo), abs(_6567.hi));
                float _6559 = spvFMul(_6555, _6555);
                float _13384 = interval_down(_6559, intervalFailed);
                float _6560 = precise::max(0.0, _13384);
                float _6561 = spvFMul(_6558, _6558);
                float _13388 = interval_up(_6561, intervalFailed);
                float _6562 = _13388;
                _6553.lo = _6560;
                _6553.hi = _6562;
                Interval _6554 = _6553;
                Interval _6563 = _6554;
                Interval _6564 = _6563;
                float _6568 = 1.0;
                float _6550 = _6568;
                float _6551 = _6568;
                _6548.lo = _6550;
                _6548.hi = _6551;
                Interval _6549 = _6548;
                Interval _6552 = _6549;
                Interval _6569 = _6552;
                float _6570 = 1.0;
                float _6545 = _6570;
                float _6546 = _6570;
                _6543.lo = _6545;
                _6543.hi = _6546;
                Interval _6544 = _6543;
                Interval _6547 = _6544;
                Interval _6571 = _6547;
                Interval _6572 = _6564;
                bool _6536 = false;
                if (_6572.lo <= 0.0)
                {
                    _6536 = _6572.hi >= 0.0;
                }
                if (_6536)
                {
                    _6535 = 0.0;
                }
                else
                {
                    _6535 = precise::min(abs(_6572.lo), abs(_6572.hi));
                }
                float _6534 = _6535;
                float _6537 = precise::max(abs(_6572.lo), abs(_6572.hi));
                float _6538 = spvFMul(_6534, _6534);
                float _13444 = interval_down(_6538, intervalFailed);
                float _6539 = precise::max(0.0, _13444);
                float _6540 = spvFMul(_6537, _6537);
                float _13448 = interval_up(_6540, intervalFailed);
                float _6541 = _13448;
                _6532.lo = _6539;
                _6532.hi = _6541;
                Interval _6533 = _6532;
                Interval _6542 = _6533;
                Interval _6573 = _6542;
                Interval _13456 = iadd(_6571, _6573, intervalFailed);
                Interval _6574 = _13456;
                Interval _13457 = idiv(_6569, _6574, intervalFailed, interval_divide_upper);
                Interval _6575 = _13457;
                Interval _8599 = _6575;
                Interval _13459 = imul(_8594, _8599, intervalFailed, optical_product_upper);
                Interval _8600 = _13459;
                float _8601 = 25.0;
                float _8602 = 1000.0;
                Interval _13460 = iratio(_8601, _8602, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8603 = _13460;
                Interval _8604 = _8555;
                Interval _6525 = _8604;
                float _6523 = 3.1415927410125732421875;
                float _6518 = _6523;
                float _13464 = interval_down(_6518, intervalFailed);
                float _6519 = _13464;
                float _6520 = _6523;
                float _13466 = interval_up(_6520, intervalFailed);
                float _6521 = _13466;
                _6516.lo = _6519;
                _6516.hi = _6521;
                Interval _6517 = _6516;
                Interval _6522 = _6517;
                Interval _6524 = _6522;
                Interval _6526 = _6524;
                float _6527 = 0.5;
                float _6513 = _6527;
                float _6514 = _6527;
                _6511.lo = _6513;
                _6511.hi = _6514;
                Interval _6512 = _6511;
                Interval _6515 = _6512;
                Interval _6528 = _6515;
                Interval _13484 = imul(_6526, _6528, intervalFailed, optical_product_upper);
                Interval _6529 = _13484;
                Interval _13485 = iadd(_6525, _6529, intervalFailed);
                Interval _6530 = _13485;
                float _6506 = _6530.lo;
                float _6507 = _6530.hi;
                float _6501 = _6506;
                float _6502 = _6507;
                _6498.lo = _6501;
                _6498.hi = _6502;
                Interval _6499 = _6498;
                Interval _6503 = _6499;
                Interval _13498 = isin_body(_6503, intervalFailed, optical_product_upper);
                Interval _6500 = _13498;
                interval_sine_upper = _6500.hi;
                float _6504 = _6500.lo;
                float _6505 = _6504;
                float _6508 = _6505;
                float _6509 = interval_sine_upper;
                _6496.lo = _6508;
                _6496.hi = _6509;
                Interval _6497 = _6496;
                Interval _6510 = _6497;
                Interval _6531 = _6510;
                Interval _8605 = _6531;
                Interval _13514 = imul(_8603, _8605, intervalFailed, optical_product_upper);
                Interval _8606 = _13514;
                Interval _8607 = param_var_footprint;
                float _8608 = 54.0;
                float _8609 = 1000.0;
                Interval _13516 = iratio(_8608, _8609, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8610 = _13516;
                Interval _6485 = _8607;
                Interval _6486 = _8610;
                Interval _13519 = imul(_6485, _6486, intervalFailed, optical_product_upper);
                Interval _6487 = _13519;
                bool _6477 = false;
                if (_6487.lo <= 0.0)
                {
                    _6477 = _6487.hi >= 0.0;
                }
                if (_6477)
                {
                    _6476 = 0.0;
                }
                else
                {
                    _6476 = precise::min(abs(_6487.lo), abs(_6487.hi));
                }
                float _6475 = _6476;
                float _6478 = precise::max(abs(_6487.lo), abs(_6487.hi));
                float _6479 = spvFMul(_6475, _6475);
                float _13549 = interval_down(_6479, intervalFailed);
                float _6480 = precise::max(0.0, _13549);
                float _6481 = spvFMul(_6478, _6478);
                float _13553 = interval_up(_6481, intervalFailed);
                float _6482 = _13553;
                _6473.lo = _6480;
                _6473.hi = _6482;
                Interval _6474 = _6473;
                Interval _6483 = _6474;
                Interval _6484 = _6483;
                float _6488 = 1.0;
                float _6470 = _6488;
                float _6471 = _6488;
                _6468.lo = _6470;
                _6468.hi = _6471;
                Interval _6469 = _6468;
                Interval _6472 = _6469;
                Interval _6489 = _6472;
                float _6490 = 1.0;
                float _6465 = _6490;
                float _6466 = _6490;
                _6463.lo = _6465;
                _6463.hi = _6466;
                Interval _6464 = _6463;
                Interval _6467 = _6464;
                Interval _6491 = _6467;
                Interval _6492 = _6484;
                bool _6456 = false;
                if (_6492.lo <= 0.0)
                {
                    _6456 = _6492.hi >= 0.0;
                }
                if (_6456)
                {
                    _6455 = 0.0;
                }
                else
                {
                    _6455 = precise::min(abs(_6492.lo), abs(_6492.hi));
                }
                float _6454 = _6455;
                float _6457 = precise::max(abs(_6492.lo), abs(_6492.hi));
                float _6458 = spvFMul(_6454, _6454);
                float _13609 = interval_down(_6458, intervalFailed);
                float _6459 = precise::max(0.0, _13609);
                float _6460 = spvFMul(_6457, _6457);
                float _13613 = interval_up(_6460, intervalFailed);
                float _6461 = _13613;
                _6452.lo = _6459;
                _6452.hi = _6461;
                Interval _6453 = _6452;
                Interval _6462 = _6453;
                Interval _6493 = _6462;
                Interval _13621 = iadd(_6491, _6493, intervalFailed);
                Interval _6494 = _13621;
                Interval _13622 = idiv(_6489, _6494, intervalFailed, interval_divide_upper);
                Interval _6495 = _13622;
                Interval _8611 = _6495;
                Interval _13624 = imul(_8606, _8611, intervalFailed, optical_product_upper);
                Interval _8612 = _13624;
                Interval _13625 = iadd(_8600, _8612, intervalFailed);
                _8501 = _13625;
                float _8613 = 45.0;
                float _8614 = 1000.0;
                Interval _13626 = iratio(_8613, _8614, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8615 = _13626;
                Interval _8616 = _8572;
                Interval _6445 = _8616;
                float _6443 = 3.1415927410125732421875;
                float _6438 = _6443;
                float _13630 = interval_down(_6438, intervalFailed);
                float _6439 = _13630;
                float _6440 = _6443;
                float _13632 = interval_up(_6440, intervalFailed);
                float _6441 = _13632;
                _6436.lo = _6439;
                _6436.hi = _6441;
                Interval _6437 = _6436;
                Interval _6442 = _6437;
                Interval _6444 = _6442;
                Interval _6446 = _6444;
                float _6447 = 0.5;
                float _6433 = _6447;
                float _6434 = _6447;
                _6431.lo = _6433;
                _6431.hi = _6434;
                Interval _6432 = _6431;
                Interval _6435 = _6432;
                Interval _6448 = _6435;
                Interval _13650 = imul(_6446, _6448, intervalFailed, optical_product_upper);
                Interval _6449 = _13650;
                Interval _13651 = iadd(_6445, _6449, intervalFailed);
                Interval _6450 = _13651;
                float _6426 = _6450.lo;
                float _6427 = _6450.hi;
                float _6421 = _6426;
                float _6422 = _6427;
                _6418.lo = _6421;
                _6418.hi = _6422;
                Interval _6419 = _6418;
                Interval _6423 = _6419;
                Interval _13664 = isin_body(_6423, intervalFailed, optical_product_upper);
                Interval _6420 = _13664;
                interval_sine_upper = _6420.hi;
                float _6424 = _6420.lo;
                float _6425 = _6424;
                float _6428 = _6425;
                float _6429 = interval_sine_upper;
                _6416.lo = _6428;
                _6416.hi = _6429;
                Interval _6417 = _6416;
                Interval _6430 = _6417;
                Interval _6451 = _6430;
                Interval _8617 = _6451;
                Interval _13680 = imul(_8615, _8617, intervalFailed, optical_product_upper);
                Interval _8618 = _13680;
                Interval _8619 = param_var_footprint;
                float _8620 = 24.0;
                float _8621 = 1000.0;
                Interval _13682 = iratio(_8620, _8621, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8622 = _13682;
                Interval _6405 = _8619;
                Interval _6406 = _8622;
                Interval _13685 = imul(_6405, _6406, intervalFailed, optical_product_upper);
                Interval _6407 = _13685;
                bool _6397 = false;
                if (_6407.lo <= 0.0)
                {
                    _6397 = _6407.hi >= 0.0;
                }
                if (_6397)
                {
                    _6396 = 0.0;
                }
                else
                {
                    _6396 = precise::min(abs(_6407.lo), abs(_6407.hi));
                }
                float _6395 = _6396;
                float _6398 = precise::max(abs(_6407.lo), abs(_6407.hi));
                float _6399 = spvFMul(_6395, _6395);
                float _13715 = interval_down(_6399, intervalFailed);
                float _6400 = precise::max(0.0, _13715);
                float _6401 = spvFMul(_6398, _6398);
                float _13719 = interval_up(_6401, intervalFailed);
                float _6402 = _13719;
                _6393.lo = _6400;
                _6393.hi = _6402;
                Interval _6394 = _6393;
                Interval _6403 = _6394;
                Interval _6404 = _6403;
                float _6408 = 1.0;
                float _6390 = _6408;
                float _6391 = _6408;
                _6388.lo = _6390;
                _6388.hi = _6391;
                Interval _6389 = _6388;
                Interval _6392 = _6389;
                Interval _6409 = _6392;
                float _6410 = 1.0;
                float _6385 = _6410;
                float _6386 = _6410;
                _6383.lo = _6385;
                _6383.hi = _6386;
                Interval _6384 = _6383;
                Interval _6387 = _6384;
                Interval _6411 = _6387;
                Interval _6412 = _6404;
                bool _6376 = false;
                if (_6412.lo <= 0.0)
                {
                    _6376 = _6412.hi >= 0.0;
                }
                if (_6376)
                {
                    _6375 = 0.0;
                }
                else
                {
                    _6375 = precise::min(abs(_6412.lo), abs(_6412.hi));
                }
                float _6374 = _6375;
                float _6377 = precise::max(abs(_6412.lo), abs(_6412.hi));
                float _6378 = spvFMul(_6374, _6374);
                float _13775 = interval_down(_6378, intervalFailed);
                float _6379 = precise::max(0.0, _13775);
                float _6380 = spvFMul(_6377, _6377);
                float _13779 = interval_up(_6380, intervalFailed);
                float _6381 = _13779;
                _6372.lo = _6379;
                _6372.hi = _6381;
                Interval _6373 = _6372;
                Interval _6382 = _6373;
                Interval _6413 = _6382;
                Interval _13787 = iadd(_6411, _6413, intervalFailed);
                Interval _6414 = _13787;
                Interval _13788 = idiv(_6409, _6414, intervalFailed, interval_divide_upper);
                Interval _6415 = _13788;
                Interval _8623 = _6415;
                Interval _13790 = imul(_8618, _8623, intervalFailed, optical_product_upper);
                Interval _8624 = _13790;
                float _8625 = 20.0;
                float _8626 = 1000.0;
                Interval _13791 = iratio(_8625, _8626, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8627 = _13791;
                Interval _8628 = _8555;
                Interval _6365 = _8628;
                float _6363 = 3.1415927410125732421875;
                float _6358 = _6363;
                float _13795 = interval_down(_6358, intervalFailed);
                float _6359 = _13795;
                float _6360 = _6363;
                float _13797 = interval_up(_6360, intervalFailed);
                float _6361 = _13797;
                _6356.lo = _6359;
                _6356.hi = _6361;
                Interval _6357 = _6356;
                Interval _6362 = _6357;
                Interval _6364 = _6362;
                Interval _6366 = _6364;
                float _6367 = 0.5;
                float _6353 = _6367;
                float _6354 = _6367;
                _6351.lo = _6353;
                _6351.hi = _6354;
                Interval _6352 = _6351;
                Interval _6355 = _6352;
                Interval _6368 = _6355;
                Interval _13815 = imul(_6366, _6368, intervalFailed, optical_product_upper);
                Interval _6369 = _13815;
                Interval _13816 = iadd(_6365, _6369, intervalFailed);
                Interval _6370 = _13816;
                float _6346 = _6370.lo;
                float _6347 = _6370.hi;
                float _6341 = _6346;
                float _6342 = _6347;
                _6338.lo = _6341;
                _6338.hi = _6342;
                Interval _6339 = _6338;
                Interval _6343 = _6339;
                Interval _13829 = isin_body(_6343, intervalFailed, optical_product_upper);
                Interval _6340 = _13829;
                interval_sine_upper = _6340.hi;
                float _6344 = _6340.lo;
                float _6345 = _6344;
                float _6348 = _6345;
                float _6349 = interval_sine_upper;
                _6336.lo = _6348;
                _6336.hi = _6349;
                Interval _6337 = _6336;
                Interval _6350 = _6337;
                Interval _6371 = _6350;
                Interval _8629 = _6371;
                Interval _13845 = imul(_8627, _8629, intervalFailed, optical_product_upper);
                Interval _8630 = _13845;
                Interval _8631 = param_var_footprint;
                float _8632 = 54.0;
                float _8633 = 1000.0;
                Interval _13847 = iratio(_8632, _8633, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8634 = _13847;
                Interval _6325 = _8631;
                Interval _6326 = _8634;
                Interval _13850 = imul(_6325, _6326, intervalFailed, optical_product_upper);
                Interval _6327 = _13850;
                bool _6317 = false;
                if (_6327.lo <= 0.0)
                {
                    _6317 = _6327.hi >= 0.0;
                }
                if (_6317)
                {
                    _6316 = 0.0;
                }
                else
                {
                    _6316 = precise::min(abs(_6327.lo), abs(_6327.hi));
                }
                float _6315 = _6316;
                float _6318 = precise::max(abs(_6327.lo), abs(_6327.hi));
                float _6319 = spvFMul(_6315, _6315);
                float _13880 = interval_down(_6319, intervalFailed);
                float _6320 = precise::max(0.0, _13880);
                float _6321 = spvFMul(_6318, _6318);
                float _13884 = interval_up(_6321, intervalFailed);
                float _6322 = _13884;
                _6313.lo = _6320;
                _6313.hi = _6322;
                Interval _6314 = _6313;
                Interval _6323 = _6314;
                Interval _6324 = _6323;
                float _6328 = 1.0;
                float _6310 = _6328;
                float _6311 = _6328;
                _6308.lo = _6310;
                _6308.hi = _6311;
                Interval _6309 = _6308;
                Interval _6312 = _6309;
                Interval _6329 = _6312;
                float _6330 = 1.0;
                float _6305 = _6330;
                float _6306 = _6330;
                _6303.lo = _6305;
                _6303.hi = _6306;
                Interval _6304 = _6303;
                Interval _6307 = _6304;
                Interval _6331 = _6307;
                Interval _6332 = _6324;
                bool _6296 = false;
                if (_6332.lo <= 0.0)
                {
                    _6296 = _6332.hi >= 0.0;
                }
                if (_6296)
                {
                    _6295 = 0.0;
                }
                else
                {
                    _6295 = precise::min(abs(_6332.lo), abs(_6332.hi));
                }
                float _6294 = _6295;
                float _6297 = precise::max(abs(_6332.lo), abs(_6332.hi));
                float _6298 = spvFMul(_6294, _6294);
                float _13940 = interval_down(_6298, intervalFailed);
                float _6299 = precise::max(0.0, _13940);
                float _6300 = spvFMul(_6297, _6297);
                float _13944 = interval_up(_6300, intervalFailed);
                float _6301 = _13944;
                _6292.lo = _6299;
                _6292.hi = _6301;
                Interval _6293 = _6292;
                Interval _6302 = _6293;
                Interval _6333 = _6302;
                Interval _13952 = iadd(_6331, _6333, intervalFailed);
                Interval _6334 = _13952;
                Interval _13953 = idiv(_6329, _6334, intervalFailed, interval_divide_upper);
                Interval _6335 = _13953;
                Interval _8635 = _6335;
                Interval _13955 = imul(_8630, _8635, intervalFailed, optical_product_upper);
                Interval _8636 = _13955;
                Interval _6288 = _8624;
                Interval _6289 = _8636;
                float _6285 = as_type<float>(as_type<uint>(_6289.hi) ^ 2147483648u);
                float _6286 = as_type<float>(as_type<uint>(_6289.lo) ^ 2147483648u);
                _6283.lo = _6285;
                _6283.hi = _6286;
                Interval _6284 = _6283;
                Interval _6287 = _6284;
                Interval _6290 = _6287;
                Interval _13975 = iadd(_6288, _6290, intervalFailed);
                Interval _6291 = _13975;
                _8502 = _6291;
            }
            Interval _8637 = _8501;
            float _8638 = -1.0;
            float _6280 = _8638;
            float _6281 = _8638;
            _6278.lo = _6280;
            _6278.hi = _6281;
            Interval _6279 = _6278;
            Interval _6282 = _6279;
            Interval _8639 = _6282;
            Interval _8640 = _8502;
            _6276.x = _8637;
            _6276.y = _8639;
            _6276.z = _8640;
            Interval3 _6277 = _6276;
            Interval3 _8641 = _6277;
            ReflectionLiquidFrame _8642 = param_var_f;
            float3 _6263 = _8642.rotation0.xyz;
            float _6256 = _6263.x;
            float _6253 = _6256;
            float _6254 = _6256;
            _6251.lo = _6253;
            _6251.hi = _6254;
            Interval _6252 = _6251;
            Interval _6255 = _6252;
            Interval _6257 = _6255;
            float _6258 = _6263.y;
            float _6248 = _6258;
            float _6249 = _6258;
            _6246.lo = _6248;
            _6246.hi = _6249;
            Interval _6247 = _6246;
            Interval _6250 = _6247;
            Interval _6259 = _6250;
            float _6260 = _6263.z;
            float _6243 = _6260;
            float _6244 = _6260;
            _6241.lo = _6243;
            _6241.hi = _6244;
            Interval _6242 = _6241;
            Interval _6245 = _6242;
            Interval _6261 = _6245;
            _6239.x = _6257;
            _6239.y = _6259;
            _6239.z = _6261;
            Interval3 _6240 = _6239;
            Interval3 _6262 = _6240;
            Interval3 _6264 = _6262;
            Interval3 _6265 = _8641;
            Interval _6228 = _6264.x;
            Interval _6229 = _6265.x;
            Interval _14047 = imul(_6228, _6229, intervalFailed, optical_product_upper);
            Interval _6230 = _14047;
            Interval _6231 = _6264.y;
            Interval _6232 = _6265.y;
            Interval _14052 = imul(_6231, _6232, intervalFailed, optical_product_upper);
            Interval _6233 = _14052;
            Interval _14053 = iadd(_6230, _6233, intervalFailed);
            Interval _6234 = _14053;
            Interval _6235 = _6264.z;
            Interval _6236 = _6265.z;
            Interval _14058 = imul(_6235, _6236, intervalFailed, optical_product_upper);
            Interval _6237 = _14058;
            Interval _14059 = iadd(_6234, _6237, intervalFailed);
            Interval _6238 = _14059;
            Interval _6266 = _6238;
            float3 _6267 = _8642.rotation1.xyz;
            float _6221 = _6267.x;
            float _6218 = _6221;
            float _6219 = _6221;
            _6216.lo = _6218;
            _6216.hi = _6219;
            Interval _6217 = _6216;
            Interval _6220 = _6217;
            Interval _6222 = _6220;
            float _6223 = _6267.y;
            float _6213 = _6223;
            float _6214 = _6223;
            _6211.lo = _6213;
            _6211.hi = _6214;
            Interval _6212 = _6211;
            Interval _6215 = _6212;
            Interval _6224 = _6215;
            float _6225 = _6267.z;
            float _6208 = _6225;
            float _6209 = _6225;
            _6206.lo = _6208;
            _6206.hi = _6209;
            Interval _6207 = _6206;
            Interval _6210 = _6207;
            Interval _6226 = _6210;
            _6204.x = _6222;
            _6204.y = _6224;
            _6204.z = _6226;
            Interval3 _6205 = _6204;
            Interval3 _6227 = _6205;
            Interval3 _6268 = _6227;
            Interval3 _6269 = _8641;
            Interval _6193 = _6268.x;
            Interval _6194 = _6269.x;
            Interval _14111 = imul(_6193, _6194, intervalFailed, optical_product_upper);
            Interval _6195 = _14111;
            Interval _6196 = _6268.y;
            Interval _6197 = _6269.y;
            Interval _14116 = imul(_6196, _6197, intervalFailed, optical_product_upper);
            Interval _6198 = _14116;
            Interval _14117 = iadd(_6195, _6198, intervalFailed);
            Interval _6199 = _14117;
            Interval _6200 = _6268.z;
            Interval _6201 = _6269.z;
            Interval _14122 = imul(_6200, _6201, intervalFailed, optical_product_upper);
            Interval _6202 = _14122;
            Interval _14123 = iadd(_6199, _6202, intervalFailed);
            Interval _6203 = _14123;
            Interval _6270 = _6203;
            float3 _6271 = _8642.rotation2.xyz;
            float _6186 = _6271.x;
            float _6183 = _6186;
            float _6184 = _6186;
            _6181.lo = _6183;
            _6181.hi = _6184;
            Interval _6182 = _6181;
            Interval _6185 = _6182;
            Interval _6187 = _6185;
            float _6188 = _6271.y;
            float _6178 = _6188;
            float _6179 = _6188;
            _6176.lo = _6178;
            _6176.hi = _6179;
            Interval _6177 = _6176;
            Interval _6180 = _6177;
            Interval _6189 = _6180;
            float _6190 = _6271.z;
            float _6173 = _6190;
            float _6174 = _6190;
            _6171.lo = _6173;
            _6171.hi = _6174;
            Interval _6172 = _6171;
            Interval _6175 = _6172;
            Interval _6191 = _6175;
            _6169.x = _6187;
            _6169.y = _6189;
            _6169.z = _6191;
            Interval3 _6170 = _6169;
            Interval3 _6192 = _6170;
            Interval3 _6272 = _6192;
            Interval3 _6273 = _8641;
            Interval _6158 = _6272.x;
            Interval _6159 = _6273.x;
            Interval _14175 = imul(_6158, _6159, intervalFailed, optical_product_upper);
            Interval _6160 = _14175;
            Interval _6161 = _6272.y;
            Interval _6162 = _6273.y;
            Interval _14180 = imul(_6161, _6162, intervalFailed, optical_product_upper);
            Interval _6163 = _14180;
            Interval _14181 = iadd(_6160, _6163, intervalFailed);
            Interval _6164 = _14181;
            Interval _6165 = _6272.z;
            Interval _6166 = _6273.z;
            Interval _14186 = imul(_6165, _6166, intervalFailed, optical_product_upper);
            Interval _6167 = _14186;
            Interval _14187 = iadd(_6164, _6167, intervalFailed);
            Interval _6168 = _14187;
            Interval _6274 = _6168;
            _6156.x = _6266;
            _6156.y = _6270;
            _6156.z = _6274;
            Interval3 _6157 = _6156;
            Interval3 _6275 = _6157;
            Interval3 _8643 = _6275;
            Interval3 _6149 = _8643;
            float _6150 = 1.0;
            float _6146 = _6150;
            float _6147 = _6150;
            _6144.lo = _6146;
            _6144.hi = _6147;
            Interval _6145 = _6144;
            Interval _6148 = _6145;
            Interval _6151 = _6148;
            Interval3 _6152 = _8643;
            Interval _6135 = _6152.x;
            bool _6128 = false;
            if (_6135.lo <= 0.0)
            {
                _6128 = _6135.hi >= 0.0;
            }
            if (_6128)
            {
                _6127 = 0.0;
            }
            else
            {
                _6127 = precise::min(abs(_6135.lo), abs(_6135.hi));
            }
            float _6126 = _6127;
            float _6129 = precise::max(abs(_6135.lo), abs(_6135.hi));
            float _6130 = spvFMul(_6126, _6126);
            float _14240 = interval_down(_6130, intervalFailed);
            float _6131 = precise::max(0.0, _14240);
            float _6132 = spvFMul(_6129, _6129);
            float _14244 = interval_up(_6132, intervalFailed);
            float _6133 = _14244;
            _6124.lo = _6131;
            _6124.hi = _6133;
            Interval _6125 = _6124;
            Interval _6134 = _6125;
            Interval _6136 = _6134;
            Interval _6137 = _6152.y;
            bool _6117 = false;
            if (_6137.lo <= 0.0)
            {
                _6117 = _6137.hi >= 0.0;
            }
            if (_6117)
            {
                _6116 = 0.0;
            }
            else
            {
                _6116 = precise::min(abs(_6137.lo), abs(_6137.hi));
            }
            float _6115 = _6116;
            float _6118 = precise::max(abs(_6137.lo), abs(_6137.hi));
            float _6119 = spvFMul(_6115, _6115);
            float _14283 = interval_down(_6119, intervalFailed);
            float _6120 = precise::max(0.0, _14283);
            float _6121 = spvFMul(_6118, _6118);
            float _14287 = interval_up(_6121, intervalFailed);
            float _6122 = _14287;
            _6113.lo = _6120;
            _6113.hi = _6122;
            Interval _6114 = _6113;
            Interval _6123 = _6114;
            Interval _6138 = _6123;
            Interval _14295 = iadd(_6136, _6138, intervalFailed);
            Interval _6139 = _14295;
            Interval _6140 = _6152.z;
            bool _6106 = false;
            if (_6140.lo <= 0.0)
            {
                _6106 = _6140.hi >= 0.0;
            }
            if (_6106)
            {
                _6105 = 0.0;
            }
            else
            {
                _6105 = precise::min(abs(_6140.lo), abs(_6140.hi));
            }
            float _6104 = _6105;
            float _6107 = precise::max(abs(_6140.lo), abs(_6140.hi));
            float _6108 = spvFMul(_6104, _6104);
            float _14327 = interval_down(_6108, intervalFailed);
            float _6109 = precise::max(0.0, _14327);
            float _6110 = spvFMul(_6107, _6107);
            float _14331 = interval_up(_6110, intervalFailed);
            float _6111 = _14331;
            _6102.lo = _6109;
            _6102.hi = _6111;
            Interval _6103 = _6102;
            Interval _6112 = _6103;
            Interval _6141 = _6112;
            Interval _14339 = iadd(_6139, _6141, intervalFailed);
            Interval _6142 = _14339;
            Interval _14340 = isqrt(_6142, intervalFailed);
            Interval _6143 = _14340;
            Interval _6153 = _6143;
            Interval _14342 = idiv(_6151, _6153, intervalFailed, interval_divide_upper);
            Interval _6154 = _14342;
            Interval _6092 = _6149.x;
            Interval _6093 = _6154;
            Interval _14346 = imul(_6092, _6093, intervalFailed, optical_product_upper);
            Interval _6094 = _14346;
            Interval _6095 = _6149.y;
            Interval _6096 = _6154;
            Interval _14350 = imul(_6095, _6096, intervalFailed, optical_product_upper);
            Interval _6097 = _14350;
            Interval _6098 = _6149.z;
            Interval _6099 = _6154;
            Interval _14354 = imul(_6098, _6099, intervalFailed, optical_product_upper);
            Interval _6100 = _14354;
            _6090.x = _6094;
            _6090.y = _6097;
            _6090.z = _6100;
            Interval3 _6091 = _6090;
            Interval3 _6101 = _6091;
            Interval3 _6155 = _6101;
            Interval3 _8644 = _6155;
            Interval3 _8645 = param_var_direction_1;
            Interval3 _14366 = oriented(_8644, _8645, intervalFailed, optical_product_upper);
            Interval3 _8646 = _14366;
            n = _8646;
        }
        else
        {
            ReflectionSpecularPlane param_var_p_1 = plane;
            Interval3 param_var_hit = hit;
            bool _14370 = outside_face(param_var_p_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (_14370)
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
            bool _6087 = false;
            if (param_var_p_2.a.w == 2.0)
            {
                _6087 = param_var_p_2.b.w == 2.0;
            }
            bool _6088 = false;
            if (_6087)
            {
                _6088 = param_var_p_2.c.w == 2.0;
            }
            bool _6089 = _6088;
            temp_var_logical_4 = !_6089;
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
        Interval _14401 = iratio(param_var_n_2, param_var_d_1, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_a_17 = _14401;
        Interval param_var_a_18 = _distance;
        float param_var_n_3 = 1.0;
        float param_var_d_2 = 100000.0;
        Interval _14403 = iratio(param_var_n_3, param_var_d_2, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_b_14 = _14403;
        Interval _14404 = imul(param_var_a_18, param_var_b_14, intervalFailed, optical_product_upper);
        Interval param_var_b_15 = _14404;
        float _6084 = precise::max(param_var_a_17.lo, param_var_b_15.lo);
        float _6085 = precise::max(param_var_a_17.hi, param_var_b_15.hi);
        _6082.lo = _6084;
        _6082.hi = _6085;
        Interval _6083 = _6082;
        Interval _6086 = _6083;
        bias0 = _6086;
        Interval3 param_var_a_19 = hit;
        Interval3 param_var_a_20 = n;
        Interval param_var_b_16 = bias0;
        Interval _6072 = param_var_a_20.x;
        Interval _6073 = param_var_b_16;
        Interval _14428 = imul(_6072, _6073, intervalFailed, optical_product_upper);
        Interval _6074 = _14428;
        Interval _6075 = param_var_a_20.y;
        Interval _6076 = param_var_b_16;
        Interval _14432 = imul(_6075, _6076, intervalFailed, optical_product_upper);
        Interval _6077 = _14432;
        Interval _6078 = param_var_a_20.z;
        Interval _6079 = param_var_b_16;
        Interval _14436 = imul(_6078, _6079, intervalFailed, optical_product_upper);
        Interval _6080 = _14436;
        _6070.x = _6074;
        _6070.y = _6077;
        _6070.z = _6080;
        Interval3 _6071 = _6070;
        Interval3 _6081 = _6071;
        Interval3 param_var_b_17 = _6081;
        Interval _6060 = param_var_a_19.x;
        Interval _6061 = param_var_b_17.x;
        Interval _14450 = iadd(_6060, _6061, intervalFailed);
        Interval _6062 = _14450;
        Interval _6063 = param_var_a_19.y;
        Interval _6064 = param_var_b_17.y;
        Interval _14455 = iadd(_6063, _6064, intervalFailed);
        Interval _6065 = _14455;
        Interval _6066 = param_var_a_19.z;
        Interval _6067 = param_var_b_17.z;
        Interval _14460 = iadd(_6066, _6067, intervalFailed);
        Interval _6068 = _14460;
        _6058.x = _6062;
        _6058.y = _6065;
        _6058.z = _6068;
        Interval3 _6059 = _6058;
        Interval3 _6069 = _6059;
        origin = _6069;
        Interval3 param_var_a_21 = outgoing;
        Interval3 param_var_n_4 = n;
        Interval3 _6048 = param_var_a_21;
        Interval3 _6049 = param_var_n_4;
        float _6050 = 2.0;
        float _6045 = _6050;
        float _6046 = _6050;
        _6043.lo = _6045;
        _6043.hi = _6046;
        Interval _6044 = _6043;
        Interval _6047 = _6044;
        Interval _6051 = _6047;
        Interval3 _6052 = param_var_a_21;
        Interval3 _6053 = param_var_n_4;
        Interval _6032 = _6052.x;
        Interval _6033 = _6053.x;
        Interval _14489 = imul(_6032, _6033, intervalFailed, optical_product_upper);
        Interval _6034 = _14489;
        Interval _6035 = _6052.y;
        Interval _6036 = _6053.y;
        Interval _14494 = imul(_6035, _6036, intervalFailed, optical_product_upper);
        Interval _6037 = _14494;
        Interval _14495 = iadd(_6034, _6037, intervalFailed);
        Interval _6038 = _14495;
        Interval _6039 = _6052.z;
        Interval _6040 = _6053.z;
        Interval _14500 = imul(_6039, _6040, intervalFailed, optical_product_upper);
        Interval _6041 = _14500;
        Interval _14501 = iadd(_6038, _6041, intervalFailed);
        Interval _6042 = _14501;
        Interval _6054 = _6042;
        Interval _14503 = imul(_6051, _6054, intervalFailed, optical_product_upper);
        Interval _6055 = _14503;
        Interval _6022 = _6049.x;
        Interval _6023 = _6055;
        Interval _14507 = imul(_6022, _6023, intervalFailed, optical_product_upper);
        Interval _6024 = _14507;
        Interval _6025 = _6049.y;
        Interval _6026 = _6055;
        Interval _14511 = imul(_6025, _6026, intervalFailed, optical_product_upper);
        Interval _6027 = _14511;
        Interval _6028 = _6049.z;
        Interval _6029 = _6055;
        Interval _14515 = imul(_6028, _6029, intervalFailed, optical_product_upper);
        Interval _6030 = _14515;
        _6020.x = _6024;
        _6020.y = _6027;
        _6020.z = _6030;
        Interval3 _6021 = _6020;
        Interval3 _6031 = _6021;
        Interval3 _6056 = _6031;
        Interval3 _6011 = _6048;
        Interval _6012 = _6056.x;
        float _6008 = as_type<float>(as_type<uint>(_6012.hi) ^ 2147483648u);
        float _6009 = as_type<float>(as_type<uint>(_6012.lo) ^ 2147483648u);
        _6006.lo = _6008;
        _6006.hi = _6009;
        Interval _6007 = _6006;
        Interval _6010 = _6007;
        Interval _6013 = _6010;
        Interval _6014 = _6056.y;
        float _6003 = as_type<float>(as_type<uint>(_6014.hi) ^ 2147483648u);
        float _6004 = as_type<float>(as_type<uint>(_6014.lo) ^ 2147483648u);
        _6001.lo = _6003;
        _6001.hi = _6004;
        Interval _6002 = _6001;
        Interval _6005 = _6002;
        Interval _6015 = _6005;
        Interval _6016 = _6056.z;
        float _5998 = as_type<float>(as_type<uint>(_6016.hi) ^ 2147483648u);
        float _5999 = as_type<float>(as_type<uint>(_6016.lo) ^ 2147483648u);
        _5996.lo = _5998;
        _5996.hi = _5999;
        Interval _5997 = _5996;
        Interval _6000 = _5997;
        Interval _6017 = _6000;
        _5994.x = _6013;
        _5994.y = _6015;
        _5994.z = _6017;
        Interval3 _5995 = _5994;
        Interval3 _6018 = _5995;
        Interval _5984 = _6011.x;
        Interval _5985 = _6018.x;
        Interval _14595 = iadd(_5984, _5985, intervalFailed);
        Interval _5986 = _14595;
        Interval _5987 = _6011.y;
        Interval _5988 = _6018.y;
        Interval _14600 = iadd(_5987, _5988, intervalFailed);
        Interval _5989 = _14600;
        Interval _5990 = _6011.z;
        Interval _5991 = _6018.z;
        Interval _14605 = iadd(_5990, _5991, intervalFailed);
        Interval _5992 = _14605;
        _5982.x = _5986;
        _5982.y = _5989;
        _5982.z = _5992;
        Interval3 _5983 = _5982;
        Interval3 _5993 = _5983;
        Interval3 _6019 = _5993;
        Interval3 _6057 = _6019;
        outgoing = _6057;
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
            bool _5978 = false;
            if (param_var_a_22.lo <= 0.0)
            {
                _5978 = param_var_a_22.hi >= 0.0;
            }
            if (_5978)
            {
                _5977 = 0.0;
            }
            else
            {
                _5977 = precise::min(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            }
            float _5979 = _5977;
            float _5980 = precise::max(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            _5975.lo = _5979;
            _5975.hi = _5980;
            Interval _5976 = _5975;
            Interval _5981 = _5976;
            Interval ay = _5981;
            if (ay.hi < 0.949999988079071044921875)
            {
                Interval3 param_var_a_23 = outgoing;
                float3 param_var_v_3 = float3(0.0, 1.0, 0.0);
                float _5968 = param_var_v_3.x;
                float _5965 = _5968;
                float _5966 = _5968;
                _5963.lo = _5965;
                _5963.hi = _5966;
                Interval _5964 = _5963;
                Interval _5967 = _5964;
                Interval _5969 = _5967;
                float _5970 = param_var_v_3.y;
                float _5960 = _5970;
                float _5961 = _5970;
                _5958.lo = _5960;
                _5958.hi = _5961;
                Interval _5959 = _5958;
                Interval _5962 = _5959;
                Interval _5971 = _5962;
                float _5972 = param_var_v_3.z;
                float _5955 = _5972;
                float _5956 = _5972;
                _5953.lo = _5955;
                _5953.hi = _5956;
                Interval _5954 = _5953;
                Interval _5957 = _5954;
                Interval _5973 = _5957;
                _5951.x = _5969;
                _5951.y = _5971;
                _5951.z = _5973;
                Interval3 _5952 = _5951;
                Interval3 _5974 = _5952;
                Interval3 param_var_b_18 = _5974;
                Interval _5929 = param_var_a_23.y;
                Interval _5930 = param_var_b_18.z;
                Interval _14712 = imul(_5929, _5930, intervalFailed, optical_product_upper);
                Interval _5931 = _14712;
                Interval _5932 = param_var_a_23.z;
                Interval _5933 = param_var_b_18.y;
                Interval _14717 = imul(_5932, _5933, intervalFailed, optical_product_upper);
                Interval _5934 = _14717;
                Interval _5925 = _5931;
                Interval _5926 = _5934;
                float _5922 = as_type<float>(as_type<uint>(_5926.hi) ^ 2147483648u);
                float _5923 = as_type<float>(as_type<uint>(_5926.lo) ^ 2147483648u);
                _5920.lo = _5922;
                _5920.hi = _5923;
                Interval _5921 = _5920;
                Interval _5924 = _5921;
                Interval _5927 = _5924;
                Interval _14737 = iadd(_5925, _5927, intervalFailed);
                Interval _5928 = _14737;
                Interval _5935 = _5928;
                Interval _5936 = param_var_a_23.z;
                Interval _5937 = param_var_b_18.x;
                Interval _14743 = imul(_5936, _5937, intervalFailed, optical_product_upper);
                Interval _5938 = _14743;
                Interval _5939 = param_var_a_23.x;
                Interval _5940 = param_var_b_18.z;
                Interval _14748 = imul(_5939, _5940, intervalFailed, optical_product_upper);
                Interval _5941 = _14748;
                Interval _5916 = _5938;
                Interval _5917 = _5941;
                float _5913 = as_type<float>(as_type<uint>(_5917.hi) ^ 2147483648u);
                float _5914 = as_type<float>(as_type<uint>(_5917.lo) ^ 2147483648u);
                _5911.lo = _5913;
                _5911.hi = _5914;
                Interval _5912 = _5911;
                Interval _5915 = _5912;
                Interval _5918 = _5915;
                Interval _14768 = iadd(_5916, _5918, intervalFailed);
                Interval _5919 = _14768;
                Interval _5942 = _5919;
                Interval _5943 = param_var_a_23.x;
                Interval _5944 = param_var_b_18.y;
                Interval _14774 = imul(_5943, _5944, intervalFailed, optical_product_upper);
                Interval _5945 = _14774;
                Interval _5946 = param_var_a_23.y;
                Interval _5947 = param_var_b_18.x;
                Interval _14779 = imul(_5946, _5947, intervalFailed, optical_product_upper);
                Interval _5948 = _14779;
                Interval _5907 = _5945;
                Interval _5908 = _5948;
                float _5904 = as_type<float>(as_type<uint>(_5908.hi) ^ 2147483648u);
                float _5905 = as_type<float>(as_type<uint>(_5908.lo) ^ 2147483648u);
                _5902.lo = _5904;
                _5902.hi = _5905;
                Interval _5903 = _5902;
                Interval _5906 = _5903;
                Interval _5909 = _5906;
                Interval _14799 = iadd(_5907, _5909, intervalFailed);
                Interval _5910 = _14799;
                Interval _5949 = _5910;
                _5900.x = _5935;
                _5900.y = _5942;
                _5900.z = _5949;
                Interval3 _5901 = _5900;
                Interval3 _5950 = _5901;
                Interval3 param_var_a_24 = _5950;
                Interval3 _5893 = param_var_a_24;
                float _5894 = 1.0;
                float _5890 = _5894;
                float _5891 = _5894;
                _5888.lo = _5890;
                _5888.hi = _5891;
                Interval _5889 = _5888;
                Interval _5892 = _5889;
                Interval _5895 = _5892;
                Interval3 _5896 = param_var_a_24;
                Interval _5879 = _5896.x;
                bool _5872 = false;
                if (_5879.lo <= 0.0)
                {
                    _5872 = _5879.hi >= 0.0;
                }
                if (_5872)
                {
                    _5871 = 0.0;
                }
                else
                {
                    _5871 = precise::min(abs(_5879.lo), abs(_5879.hi));
                }
                float _5870 = _5871;
                float _5873 = precise::max(abs(_5879.lo), abs(_5879.hi));
                float _5874 = spvFMul(_5870, _5870);
                float _14852 = interval_down(_5874, intervalFailed);
                float _5875 = precise::max(0.0, _14852);
                float _5876 = spvFMul(_5873, _5873);
                float _14856 = interval_up(_5876, intervalFailed);
                float _5877 = _14856;
                _5868.lo = _5875;
                _5868.hi = _5877;
                Interval _5869 = _5868;
                Interval _5878 = _5869;
                Interval _5880 = _5878;
                Interval _5881 = _5896.y;
                bool _5861 = false;
                if (_5881.lo <= 0.0)
                {
                    _5861 = _5881.hi >= 0.0;
                }
                if (_5861)
                {
                    _5860 = 0.0;
                }
                else
                {
                    _5860 = precise::min(abs(_5881.lo), abs(_5881.hi));
                }
                float _5859 = _5860;
                float _5862 = precise::max(abs(_5881.lo), abs(_5881.hi));
                float _5863 = spvFMul(_5859, _5859);
                float _14895 = interval_down(_5863, intervalFailed);
                float _5864 = precise::max(0.0, _14895);
                float _5865 = spvFMul(_5862, _5862);
                float _14899 = interval_up(_5865, intervalFailed);
                float _5866 = _14899;
                _5857.lo = _5864;
                _5857.hi = _5866;
                Interval _5858 = _5857;
                Interval _5867 = _5858;
                Interval _5882 = _5867;
                Interval _14907 = iadd(_5880, _5882, intervalFailed);
                Interval _5883 = _14907;
                Interval _5884 = _5896.z;
                bool _5850 = false;
                if (_5884.lo <= 0.0)
                {
                    _5850 = _5884.hi >= 0.0;
                }
                if (_5850)
                {
                    _5849 = 0.0;
                }
                else
                {
                    _5849 = precise::min(abs(_5884.lo), abs(_5884.hi));
                }
                float _5848 = _5849;
                float _5851 = precise::max(abs(_5884.lo), abs(_5884.hi));
                float _5852 = spvFMul(_5848, _5848);
                float _14939 = interval_down(_5852, intervalFailed);
                float _5853 = precise::max(0.0, _14939);
                float _5854 = spvFMul(_5851, _5851);
                float _14943 = interval_up(_5854, intervalFailed);
                float _5855 = _14943;
                _5846.lo = _5853;
                _5846.hi = _5855;
                Interval _5847 = _5846;
                Interval _5856 = _5847;
                Interval _5885 = _5856;
                Interval _14951 = iadd(_5883, _5885, intervalFailed);
                Interval _5886 = _14951;
                Interval _14952 = isqrt(_5886, intervalFailed);
                Interval _5887 = _14952;
                Interval _5897 = _5887;
                Interval _14954 = idiv(_5895, _5897, intervalFailed, interval_divide_upper);
                Interval _5898 = _14954;
                Interval _5836 = _5893.x;
                Interval _5837 = _5898;
                Interval _14958 = imul(_5836, _5837, intervalFailed, optical_product_upper);
                Interval _5838 = _14958;
                Interval _5839 = _5893.y;
                Interval _5840 = _5898;
                Interval _14962 = imul(_5839, _5840, intervalFailed, optical_product_upper);
                Interval _5841 = _14962;
                Interval _5842 = _5893.z;
                Interval _5843 = _5898;
                Interval _14966 = imul(_5842, _5843, intervalFailed, optical_product_upper);
                Interval _5844 = _14966;
                _5834.x = _5838;
                _5834.y = _5841;
                _5834.z = _5844;
                Interval3 _5835 = _5834;
                Interval3 _5845 = _5835;
                Interval3 _5899 = _5845;
                t = _5899;
            }
            else
            {
                if (ay.lo >= 0.949999988079071044921875)
                {
                    Interval3 param_var_a_25 = outgoing;
                    float3 param_var_v_4 = float3(1.0, 0.0, 0.0);
                    float _5827 = param_var_v_4.x;
                    float _5824 = _5827;
                    float _5825 = _5827;
                    _5822.lo = _5824;
                    _5822.hi = _5825;
                    Interval _5823 = _5822;
                    Interval _5826 = _5823;
                    Interval _5828 = _5826;
                    float _5829 = param_var_v_4.y;
                    float _5819 = _5829;
                    float _5820 = _5829;
                    _5817.lo = _5819;
                    _5817.hi = _5820;
                    Interval _5818 = _5817;
                    Interval _5821 = _5818;
                    Interval _5830 = _5821;
                    float _5831 = param_var_v_4.z;
                    float _5814 = _5831;
                    float _5815 = _5831;
                    _5812.lo = _5814;
                    _5812.hi = _5815;
                    Interval _5813 = _5812;
                    Interval _5816 = _5813;
                    Interval _5832 = _5816;
                    _5810.x = _5828;
                    _5810.y = _5830;
                    _5810.z = _5832;
                    Interval3 _5811 = _5810;
                    Interval3 _5833 = _5811;
                    Interval3 param_var_b_19 = _5833;
                    Interval _5788 = param_var_a_25.y;
                    Interval _5789 = param_var_b_19.z;
                    Interval _15027 = imul(_5788, _5789, intervalFailed, optical_product_upper);
                    Interval _5790 = _15027;
                    Interval _5791 = param_var_a_25.z;
                    Interval _5792 = param_var_b_19.y;
                    Interval _15032 = imul(_5791, _5792, intervalFailed, optical_product_upper);
                    Interval _5793 = _15032;
                    Interval _5784 = _5790;
                    Interval _5785 = _5793;
                    float _5781 = as_type<float>(as_type<uint>(_5785.hi) ^ 2147483648u);
                    float _5782 = as_type<float>(as_type<uint>(_5785.lo) ^ 2147483648u);
                    _5779.lo = _5781;
                    _5779.hi = _5782;
                    Interval _5780 = _5779;
                    Interval _5783 = _5780;
                    Interval _5786 = _5783;
                    Interval _15052 = iadd(_5784, _5786, intervalFailed);
                    Interval _5787 = _15052;
                    Interval _5794 = _5787;
                    Interval _5795 = param_var_a_25.z;
                    Interval _5796 = param_var_b_19.x;
                    Interval _15058 = imul(_5795, _5796, intervalFailed, optical_product_upper);
                    Interval _5797 = _15058;
                    Interval _5798 = param_var_a_25.x;
                    Interval _5799 = param_var_b_19.z;
                    Interval _15063 = imul(_5798, _5799, intervalFailed, optical_product_upper);
                    Interval _5800 = _15063;
                    Interval _5775 = _5797;
                    Interval _5776 = _5800;
                    float _5772 = as_type<float>(as_type<uint>(_5776.hi) ^ 2147483648u);
                    float _5773 = as_type<float>(as_type<uint>(_5776.lo) ^ 2147483648u);
                    _5770.lo = _5772;
                    _5770.hi = _5773;
                    Interval _5771 = _5770;
                    Interval _5774 = _5771;
                    Interval _5777 = _5774;
                    Interval _15083 = iadd(_5775, _5777, intervalFailed);
                    Interval _5778 = _15083;
                    Interval _5801 = _5778;
                    Interval _5802 = param_var_a_25.x;
                    Interval _5803 = param_var_b_19.y;
                    Interval _15089 = imul(_5802, _5803, intervalFailed, optical_product_upper);
                    Interval _5804 = _15089;
                    Interval _5805 = param_var_a_25.y;
                    Interval _5806 = param_var_b_19.x;
                    Interval _15094 = imul(_5805, _5806, intervalFailed, optical_product_upper);
                    Interval _5807 = _15094;
                    Interval _5766 = _5804;
                    Interval _5767 = _5807;
                    float _5763 = as_type<float>(as_type<uint>(_5767.hi) ^ 2147483648u);
                    float _5764 = as_type<float>(as_type<uint>(_5767.lo) ^ 2147483648u);
                    _5761.lo = _5763;
                    _5761.hi = _5764;
                    Interval _5762 = _5761;
                    Interval _5765 = _5762;
                    Interval _5768 = _5765;
                    Interval _15114 = iadd(_5766, _5768, intervalFailed);
                    Interval _5769 = _15114;
                    Interval _5808 = _5769;
                    _5759.x = _5794;
                    _5759.y = _5801;
                    _5759.z = _5808;
                    Interval3 _5760 = _5759;
                    Interval3 _5809 = _5760;
                    Interval3 param_var_a_26 = _5809;
                    Interval3 _5752 = param_var_a_26;
                    float _5753 = 1.0;
                    float _5749 = _5753;
                    float _5750 = _5753;
                    _5747.lo = _5749;
                    _5747.hi = _5750;
                    Interval _5748 = _5747;
                    Interval _5751 = _5748;
                    Interval _5754 = _5751;
                    Interval3 _5755 = param_var_a_26;
                    Interval _5738 = _5755.x;
                    bool _5731 = false;
                    if (_5738.lo <= 0.0)
                    {
                        _5731 = _5738.hi >= 0.0;
                    }
                    if (_5731)
                    {
                        _5730 = 0.0;
                    }
                    else
                    {
                        _5730 = precise::min(abs(_5738.lo), abs(_5738.hi));
                    }
                    float _5729 = _5730;
                    float _5732 = precise::max(abs(_5738.lo), abs(_5738.hi));
                    float _5733 = spvFMul(_5729, _5729);
                    float _15167 = interval_down(_5733, intervalFailed);
                    float _5734 = precise::max(0.0, _15167);
                    float _5735 = spvFMul(_5732, _5732);
                    float _15171 = interval_up(_5735, intervalFailed);
                    float _5736 = _15171;
                    _5727.lo = _5734;
                    _5727.hi = _5736;
                    Interval _5728 = _5727;
                    Interval _5737 = _5728;
                    Interval _5739 = _5737;
                    Interval _5740 = _5755.y;
                    bool _5720 = false;
                    if (_5740.lo <= 0.0)
                    {
                        _5720 = _5740.hi >= 0.0;
                    }
                    if (_5720)
                    {
                        _5719 = 0.0;
                    }
                    else
                    {
                        _5719 = precise::min(abs(_5740.lo), abs(_5740.hi));
                    }
                    float _5718 = _5719;
                    float _5721 = precise::max(abs(_5740.lo), abs(_5740.hi));
                    float _5722 = spvFMul(_5718, _5718);
                    float _15210 = interval_down(_5722, intervalFailed);
                    float _5723 = precise::max(0.0, _15210);
                    float _5724 = spvFMul(_5721, _5721);
                    float _15214 = interval_up(_5724, intervalFailed);
                    float _5725 = _15214;
                    _5716.lo = _5723;
                    _5716.hi = _5725;
                    Interval _5717 = _5716;
                    Interval _5726 = _5717;
                    Interval _5741 = _5726;
                    Interval _15222 = iadd(_5739, _5741, intervalFailed);
                    Interval _5742 = _15222;
                    Interval _5743 = _5755.z;
                    bool _5709 = false;
                    if (_5743.lo <= 0.0)
                    {
                        _5709 = _5743.hi >= 0.0;
                    }
                    if (_5709)
                    {
                        _5708 = 0.0;
                    }
                    else
                    {
                        _5708 = precise::min(abs(_5743.lo), abs(_5743.hi));
                    }
                    float _5707 = _5708;
                    float _5710 = precise::max(abs(_5743.lo), abs(_5743.hi));
                    float _5711 = spvFMul(_5707, _5707);
                    float _15254 = interval_down(_5711, intervalFailed);
                    float _5712 = precise::max(0.0, _15254);
                    float _5713 = spvFMul(_5710, _5710);
                    float _15258 = interval_up(_5713, intervalFailed);
                    float _5714 = _15258;
                    _5705.lo = _5712;
                    _5705.hi = _5714;
                    Interval _5706 = _5705;
                    Interval _5715 = _5706;
                    Interval _5744 = _5715;
                    Interval _15266 = iadd(_5742, _5744, intervalFailed);
                    Interval _5745 = _15266;
                    Interval _15267 = isqrt(_5745, intervalFailed);
                    Interval _5746 = _15267;
                    Interval _5756 = _5746;
                    Interval _15269 = idiv(_5754, _5756, intervalFailed, interval_divide_upper);
                    Interval _5757 = _15269;
                    Interval _5695 = _5752.x;
                    Interval _5696 = _5757;
                    Interval _15273 = imul(_5695, _5696, intervalFailed, optical_product_upper);
                    Interval _5697 = _15273;
                    Interval _5698 = _5752.y;
                    Interval _5699 = _5757;
                    Interval _15277 = imul(_5698, _5699, intervalFailed, optical_product_upper);
                    Interval _5700 = _15277;
                    Interval _5701 = _5752.z;
                    Interval _5702 = _5757;
                    Interval _15281 = imul(_5701, _5702, intervalFailed, optical_product_upper);
                    Interval _5703 = _15281;
                    _5693.x = _5697;
                    _5693.y = _5700;
                    _5693.z = _5703;
                    Interval3 _5694 = _5693;
                    Interval3 _5704 = _5694;
                    Interval3 _5758 = _5704;
                    t = _5758;
                }
                else
                {
                    intervalFailed = true;
                    return false;
                }
            }
            int2 tap = _2351[uint(receiver.settings.y)];
            Interval3 param_var_a_27 = outgoing;
            Interval3 param_var_a_28 = t;
            float param_var_n_5 = float(tap.x);
            float param_var_d_3 = 1000.0;
            Interval _15303 = iratio(param_var_n_5, param_var_d_3, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_20 = _15303;
            Interval _5683 = param_var_a_28.x;
            Interval _5684 = param_var_b_20;
            Interval _15307 = imul(_5683, _5684, intervalFailed, optical_product_upper);
            Interval _5685 = _15307;
            Interval _5686 = param_var_a_28.y;
            Interval _5687 = param_var_b_20;
            Interval _15311 = imul(_5686, _5687, intervalFailed, optical_product_upper);
            Interval _5688 = _15311;
            Interval _5689 = param_var_a_28.z;
            Interval _5690 = param_var_b_20;
            Interval _15315 = imul(_5689, _5690, intervalFailed, optical_product_upper);
            Interval _5691 = _15315;
            _5681.x = _5685;
            _5681.y = _5688;
            _5681.z = _5691;
            Interval3 _5682 = _5681;
            Interval3 _5692 = _5682;
            Interval3 param_var_a_29 = _5692;
            Interval3 param_var_a_30 = outgoing;
            Interval3 param_var_b_21 = t;
            Interval _5659 = param_var_a_30.y;
            Interval _5660 = param_var_b_21.z;
            Interval _15331 = imul(_5659, _5660, intervalFailed, optical_product_upper);
            Interval _5661 = _15331;
            Interval _5662 = param_var_a_30.z;
            Interval _5663 = param_var_b_21.y;
            Interval _15336 = imul(_5662, _5663, intervalFailed, optical_product_upper);
            Interval _5664 = _15336;
            Interval _5655 = _5661;
            Interval _5656 = _5664;
            float _5652 = as_type<float>(as_type<uint>(_5656.hi) ^ 2147483648u);
            float _5653 = as_type<float>(as_type<uint>(_5656.lo) ^ 2147483648u);
            _5650.lo = _5652;
            _5650.hi = _5653;
            Interval _5651 = _5650;
            Interval _5654 = _5651;
            Interval _5657 = _5654;
            Interval _15356 = iadd(_5655, _5657, intervalFailed);
            Interval _5658 = _15356;
            Interval _5665 = _5658;
            Interval _5666 = param_var_a_30.z;
            Interval _5667 = param_var_b_21.x;
            Interval _15362 = imul(_5666, _5667, intervalFailed, optical_product_upper);
            Interval _5668 = _15362;
            Interval _5669 = param_var_a_30.x;
            Interval _5670 = param_var_b_21.z;
            Interval _15367 = imul(_5669, _5670, intervalFailed, optical_product_upper);
            Interval _5671 = _15367;
            Interval _5646 = _5668;
            Interval _5647 = _5671;
            float _5643 = as_type<float>(as_type<uint>(_5647.hi) ^ 2147483648u);
            float _5644 = as_type<float>(as_type<uint>(_5647.lo) ^ 2147483648u);
            _5641.lo = _5643;
            _5641.hi = _5644;
            Interval _5642 = _5641;
            Interval _5645 = _5642;
            Interval _5648 = _5645;
            Interval _15387 = iadd(_5646, _5648, intervalFailed);
            Interval _5649 = _15387;
            Interval _5672 = _5649;
            Interval _5673 = param_var_a_30.x;
            Interval _5674 = param_var_b_21.y;
            Interval _15393 = imul(_5673, _5674, intervalFailed, optical_product_upper);
            Interval _5675 = _15393;
            Interval _5676 = param_var_a_30.y;
            Interval _5677 = param_var_b_21.x;
            Interval _15398 = imul(_5676, _5677, intervalFailed, optical_product_upper);
            Interval _5678 = _15398;
            Interval _5637 = _5675;
            Interval _5638 = _5678;
            float _5634 = as_type<float>(as_type<uint>(_5638.hi) ^ 2147483648u);
            float _5635 = as_type<float>(as_type<uint>(_5638.lo) ^ 2147483648u);
            _5632.lo = _5634;
            _5632.hi = _5635;
            Interval _5633 = _5632;
            Interval _5636 = _5633;
            Interval _5639 = _5636;
            Interval _15418 = iadd(_5637, _5639, intervalFailed);
            Interval _5640 = _15418;
            Interval _5679 = _5640;
            _5630.x = _5665;
            _5630.y = _5672;
            _5630.z = _5679;
            Interval3 _5631 = _5630;
            Interval3 _5680 = _5631;
            Interval3 param_var_a_31 = _5680;
            float param_var_n_6 = float(tap.y);
            float param_var_d_4 = 1000.0;
            Interval _15432 = iratio(param_var_n_6, param_var_d_4, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_22 = _15432;
            Interval _5620 = param_var_a_31.x;
            Interval _5621 = param_var_b_22;
            Interval _15436 = imul(_5620, _5621, intervalFailed, optical_product_upper);
            Interval _5622 = _15436;
            Interval _5623 = param_var_a_31.y;
            Interval _5624 = param_var_b_22;
            Interval _15440 = imul(_5623, _5624, intervalFailed, optical_product_upper);
            Interval _5625 = _15440;
            Interval _5626 = param_var_a_31.z;
            Interval _5627 = param_var_b_22;
            Interval _15444 = imul(_5626, _5627, intervalFailed, optical_product_upper);
            Interval _5628 = _15444;
            _5618.x = _5622;
            _5618.y = _5625;
            _5618.z = _5628;
            Interval3 _5619 = _5618;
            Interval3 _5629 = _5619;
            Interval3 param_var_b_23 = _5629;
            Interval _5608 = param_var_a_29.x;
            Interval _5609 = param_var_b_23.x;
            Interval _15458 = iadd(_5608, _5609, intervalFailed);
            Interval _5610 = _15458;
            Interval _5611 = param_var_a_29.y;
            Interval _5612 = param_var_b_23.y;
            Interval _15463 = iadd(_5611, _5612, intervalFailed);
            Interval _5613 = _15463;
            Interval _5614 = param_var_a_29.z;
            Interval _5615 = param_var_b_23.z;
            Interval _15468 = iadd(_5614, _5615, intervalFailed);
            Interval _5616 = _15468;
            _5606.x = _5610;
            _5606.y = _5613;
            _5606.z = _5616;
            Interval3 _5607 = _5606;
            Interval3 _5617 = _5607;
            Interval3 param_var_a_32 = _5617;
            float param_var_x_8 = receiver.settings.x;
            float _5603 = param_var_x_8;
            float _5604 = param_var_x_8;
            _5601.lo = _5603;
            _5601.hi = _5604;
            Interval _5602 = _5601;
            Interval _5605 = _5602;
            Interval param_var_a_33 = _5605;
            bool _5594 = false;
            if (param_var_a_33.lo <= 0.0)
            {
                _5594 = param_var_a_33.hi >= 0.0;
            }
            if (_5594)
            {
                _5593 = 0.0;
            }
            else
            {
                _5593 = precise::min(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            }
            float _5592 = _5593;
            float _5595 = precise::max(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            float _5596 = spvFMul(_5592, _5592);
            float _15519 = interval_down(_5596, intervalFailed);
            float _5597 = precise::max(0.0, _15519);
            float _5598 = spvFMul(_5595, _5595);
            float _15523 = interval_up(_5598, intervalFailed);
            float _5599 = _15523;
            _5590.lo = _5597;
            _5590.hi = _5599;
            Interval _5591 = _5590;
            Interval _5600 = _5591;
            Interval param_var_b_24 = _5600;
            Interval _5580 = param_var_a_32.x;
            Interval _5581 = param_var_b_24;
            Interval _15534 = imul(_5580, _5581, intervalFailed, optical_product_upper);
            Interval _5582 = _15534;
            Interval _5583 = param_var_a_32.y;
            Interval _5584 = param_var_b_24;
            Interval _15538 = imul(_5583, _5584, intervalFailed, optical_product_upper);
            Interval _5585 = _15538;
            Interval _5586 = param_var_a_32.z;
            Interval _5587 = param_var_b_24;
            Interval _15542 = imul(_5586, _5587, intervalFailed, optical_product_upper);
            Interval _5588 = _15542;
            _5578.x = _5582;
            _5578.y = _5585;
            _5578.z = _5588;
            Interval3 _5579 = _5578;
            Interval3 _5589 = _5579;
            Interval3 param_var_b_25 = _5589;
            Interval _5568 = param_var_a_27.x;
            Interval _5569 = param_var_b_25.x;
            Interval _15556 = iadd(_5568, _5569, intervalFailed);
            Interval _5570 = _15556;
            Interval _5571 = param_var_a_27.y;
            Interval _5572 = param_var_b_25.y;
            Interval _15561 = iadd(_5571, _5572, intervalFailed);
            Interval _5573 = _15561;
            Interval _5574 = param_var_a_27.z;
            Interval _5575 = param_var_b_25.z;
            Interval _15566 = iadd(_5574, _5575, intervalFailed);
            Interval _5576 = _15566;
            _5566.x = _5570;
            _5566.y = _5573;
            _5566.z = _5576;
            Interval3 _5567 = _5566;
            Interval3 _5577 = _5567;
            Interval3 param_var_a_34 = _5577;
            Interval3 _5559 = param_var_a_34;
            float _5560 = 1.0;
            float _5556 = _5560;
            float _5557 = _5560;
            _5554.lo = _5556;
            _5554.hi = _5557;
            Interval _5555 = _5554;
            Interval _5558 = _5555;
            Interval _5561 = _5558;
            Interval3 _5562 = param_var_a_34;
            Interval _5545 = _5562.x;
            bool _5538 = false;
            if (_5545.lo <= 0.0)
            {
                _5538 = _5545.hi >= 0.0;
            }
            if (_5538)
            {
                _5537 = 0.0;
            }
            else
            {
                _5537 = precise::min(abs(_5545.lo), abs(_5545.hi));
            }
            float _5536 = _5537;
            float _5539 = precise::max(abs(_5545.lo), abs(_5545.hi));
            float _5540 = spvFMul(_5536, _5536);
            float _15618 = interval_down(_5540, intervalFailed);
            float _5541 = precise::max(0.0, _15618);
            float _5542 = spvFMul(_5539, _5539);
            float _15622 = interval_up(_5542, intervalFailed);
            float _5543 = _15622;
            _5534.lo = _5541;
            _5534.hi = _5543;
            Interval _5535 = _5534;
            Interval _5544 = _5535;
            Interval _5546 = _5544;
            Interval _5547 = _5562.y;
            bool _5527 = false;
            if (_5547.lo <= 0.0)
            {
                _5527 = _5547.hi >= 0.0;
            }
            if (_5527)
            {
                _5526 = 0.0;
            }
            else
            {
                _5526 = precise::min(abs(_5547.lo), abs(_5547.hi));
            }
            float _5525 = _5526;
            float _5528 = precise::max(abs(_5547.lo), abs(_5547.hi));
            float _5529 = spvFMul(_5525, _5525);
            float _15661 = interval_down(_5529, intervalFailed);
            float _5530 = precise::max(0.0, _15661);
            float _5531 = spvFMul(_5528, _5528);
            float _15665 = interval_up(_5531, intervalFailed);
            float _5532 = _15665;
            _5523.lo = _5530;
            _5523.hi = _5532;
            Interval _5524 = _5523;
            Interval _5533 = _5524;
            Interval _5548 = _5533;
            Interval _15673 = iadd(_5546, _5548, intervalFailed);
            Interval _5549 = _15673;
            Interval _5550 = _5562.z;
            bool _5516 = false;
            if (_5550.lo <= 0.0)
            {
                _5516 = _5550.hi >= 0.0;
            }
            if (_5516)
            {
                _5515 = 0.0;
            }
            else
            {
                _5515 = precise::min(abs(_5550.lo), abs(_5550.hi));
            }
            float _5514 = _5515;
            float _5517 = precise::max(abs(_5550.lo), abs(_5550.hi));
            float _5518 = spvFMul(_5514, _5514);
            float _15705 = interval_down(_5518, intervalFailed);
            float _5519 = precise::max(0.0, _15705);
            float _5520 = spvFMul(_5517, _5517);
            float _15709 = interval_up(_5520, intervalFailed);
            float _5521 = _15709;
            _5512.lo = _5519;
            _5512.hi = _5521;
            Interval _5513 = _5512;
            Interval _5522 = _5513;
            Interval _5551 = _5522;
            Interval _15717 = iadd(_5549, _5551, intervalFailed);
            Interval _5552 = _15717;
            Interval _15718 = isqrt(_5552, intervalFailed);
            Interval _5553 = _15718;
            Interval _5563 = _5553;
            Interval _15720 = idiv(_5561, _5563, intervalFailed, interval_divide_upper);
            Interval _5564 = _15720;
            Interval _5502 = _5559.x;
            Interval _5503 = _5564;
            Interval _15724 = imul(_5502, _5503, intervalFailed, optical_product_upper);
            Interval _5504 = _15724;
            Interval _5505 = _5559.y;
            Interval _5506 = _5564;
            Interval _15728 = imul(_5505, _5506, intervalFailed, optical_product_upper);
            Interval _5507 = _15728;
            Interval _5508 = _5559.z;
            Interval _5509 = _5564;
            Interval _15732 = imul(_5508, _5509, intervalFailed, optical_product_upper);
            Interval _5510 = _15732;
            _5500.x = _5504;
            _5500.y = _5507;
            _5500.z = _5510;
            Interval3 _5501 = _5500;
            Interval3 _5511 = _5501;
            Interval3 _5565 = _5511;
            Interval3 rough = _5565;
            Interval3 param_var_a_35 = rough;
            Interval3 param_var_b_26 = n;
            Interval _5489 = param_var_a_35.x;
            Interval _5490 = param_var_b_26.x;
            Interval _15749 = imul(_5489, _5490, intervalFailed, optical_product_upper);
            Interval _5491 = _15749;
            Interval _5492 = param_var_a_35.y;
            Interval _5493 = param_var_b_26.y;
            Interval _15754 = imul(_5492, _5493, intervalFailed, optical_product_upper);
            Interval _5494 = _15754;
            Interval _15755 = iadd(_5491, _5494, intervalFailed);
            Interval _5495 = _15755;
            Interval _5496 = param_var_a_35.z;
            Interval _5497 = param_var_b_26.z;
            Interval _15760 = imul(_5496, _5497, intervalFailed, optical_product_upper);
            Interval _5498 = _15760;
            Interval _15761 = iadd(_5495, _5498, intervalFailed);
            Interval _5499 = _15761;
            Interval nd = _5499;
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
                    Interval _5479 = param_var_a_36.x;
                    Interval _5480 = param_var_b_27.x;
                    float _5476 = precise::min(_5479.lo, _5480.lo);
                    float _5477 = precise::max(_5479.hi, _5480.hi);
                    _5474.lo = _5476;
                    _5474.hi = _5477;
                    Interval _5475 = _5474;
                    Interval _5478 = _5475;
                    Interval _5481 = _5478;
                    Interval _5482 = param_var_a_36.y;
                    Interval _5483 = param_var_b_27.y;
                    float _5471 = precise::min(_5482.lo, _5483.lo);
                    float _5472 = precise::max(_5482.hi, _5483.hi);
                    _5469.lo = _5471;
                    _5469.hi = _5472;
                    Interval _5470 = _5469;
                    Interval _5473 = _5470;
                    Interval _5484 = _5473;
                    Interval _5485 = param_var_a_36.z;
                    Interval _5486 = param_var_b_27.z;
                    float _5466 = precise::min(_5485.lo, _5486.lo);
                    float _5467 = precise::max(_5485.hi, _5486.hi);
                    _5464.lo = _5466;
                    _5464.hi = _5467;
                    Interval _5465 = _5464;
                    Interval _5468 = _5465;
                    Interval _5487 = _5468;
                    _5462.x = _5481;
                    _5462.y = _5484;
                    _5462.z = _5487;
                    Interval3 _5463 = _5462;
                    Interval3 _5488 = _5463;
                    outgoing = _5488;
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
        Interval3 _5453 = param_var_a_37;
        Interval _5454 = param_var_b_28.x;
        float _5450 = as_type<float>(as_type<uint>(_5454.hi) ^ 2147483648u);
        float _5451 = as_type<float>(as_type<uint>(_5454.lo) ^ 2147483648u);
        Interval _5448;
        _5448.lo = _5450;
        _5448.hi = _5451;
        Interval _5449 = _5448;
        Interval _5452 = _5449;
        Interval _5455 = _5452;
        Interval _5456 = param_var_b_28.y;
        float _5445 = as_type<float>(as_type<uint>(_5456.hi) ^ 2147483648u);
        float _5446 = as_type<float>(as_type<uint>(_5456.lo) ^ 2147483648u);
        Interval _5443;
        _5443.lo = _5445;
        _5443.hi = _5446;
        Interval _5444 = _5443;
        Interval _5447 = _5444;
        Interval _5457 = _5447;
        Interval _5458 = param_var_b_28.z;
        float _5440 = as_type<float>(as_type<uint>(_5458.hi) ^ 2147483648u);
        float _5441 = as_type<float>(as_type<uint>(_5458.lo) ^ 2147483648u);
        Interval _5438;
        _5438.lo = _5440;
        _5438.hi = _5441;
        Interval _5439 = _5438;
        Interval _5442 = _5439;
        Interval _5459 = _5442;
        Interval3 _5436;
        _5436.x = _5455;
        _5436.y = _5457;
        _5436.z = _5459;
        Interval3 _5437 = _5436;
        Interval3 _5460 = _5437;
        Interval _5426 = _5453.x;
        Interval _5427 = _5460.x;
        Interval _15920 = iadd(_5426, _5427, intervalFailed);
        Interval _5428 = _15920;
        Interval _5429 = _5453.y;
        Interval _5430 = _5460.y;
        Interval _15925 = iadd(_5429, _5430, intervalFailed);
        Interval _5431 = _15925;
        Interval _5432 = _5453.z;
        Interval _5433 = _5460.z;
        Interval _15930 = iadd(_5432, _5433, intervalFailed);
        Interval _5434 = _15930;
        Interval3 _5424;
        _5424.x = _5428;
        _5424.y = _5431;
        _5424.z = _5434;
        Interval3 _5425 = _5424;
        Interval3 _5435 = _5425;
        Interval3 _5461 = _5435;
        travel = _5461;
    }
    Interval3 param_var_a_38 = travel;
    Interval _5415 = param_var_a_38.x;
    bool _5408 = false;
    if (_5415.lo <= 0.0)
    {
        _5408 = _5415.hi >= 0.0;
    }
    float _5407;
    if (_5408)
    {
        _5407 = 0.0;
    }
    else
    {
        _5407 = precise::min(abs(_5415.lo), abs(_5415.hi));
    }
    float _5406 = _5407;
    float _5409 = precise::max(abs(_5415.lo), abs(_5415.hi));
    float _5410 = spvFMul(_5406, _5406);
    float _15973 = interval_down(_5410, intervalFailed);
    float _5411 = precise::max(0.0, _15973);
    float _5412 = spvFMul(_5409, _5409);
    float _15977 = interval_up(_5412, intervalFailed);
    float _5413 = _15977;
    Interval _5404;
    _5404.lo = _5411;
    _5404.hi = _5413;
    Interval _5405 = _5404;
    Interval _5414 = _5405;
    Interval _5416 = _5414;
    Interval _5417 = param_var_a_38.y;
    bool _5397 = false;
    if (_5417.lo <= 0.0)
    {
        _5397 = _5417.hi >= 0.0;
    }
    float _5396;
    if (_5397)
    {
        _5396 = 0.0;
    }
    else
    {
        _5396 = precise::min(abs(_5417.lo), abs(_5417.hi));
    }
    float _5395 = _5396;
    float _5398 = precise::max(abs(_5417.lo), abs(_5417.hi));
    float _5399 = spvFMul(_5395, _5395);
    float _16016 = interval_down(_5399, intervalFailed);
    float _5400 = precise::max(0.0, _16016);
    float _5401 = spvFMul(_5398, _5398);
    float _16020 = interval_up(_5401, intervalFailed);
    float _5402 = _16020;
    Interval _5393;
    _5393.lo = _5400;
    _5393.hi = _5402;
    Interval _5394 = _5393;
    Interval _5403 = _5394;
    Interval _5418 = _5403;
    Interval _16028 = iadd(_5416, _5418, intervalFailed);
    Interval _5419 = _16028;
    Interval _5420 = param_var_a_38.z;
    bool _5386 = false;
    if (_5420.lo <= 0.0)
    {
        _5386 = _5420.hi >= 0.0;
    }
    float _5385;
    if (_5386)
    {
        _5385 = 0.0;
    }
    else
    {
        _5385 = precise::min(abs(_5420.lo), abs(_5420.hi));
    }
    float _5384 = _5385;
    float _5387 = precise::max(abs(_5420.lo), abs(_5420.hi));
    float _5388 = spvFMul(_5384, _5384);
    float _16060 = interval_down(_5388, intervalFailed);
    float _5389 = precise::max(0.0, _16060);
    float _5390 = spvFMul(_5387, _5387);
    float _16064 = interval_up(_5390, intervalFailed);
    float _5391 = _16064;
    Interval _5382;
    _5382.lo = _5389;
    _5382.hi = _5391;
    Interval _5383 = _5382;
    Interval _5392 = _5383;
    Interval _5421 = _5392;
    Interval _16072 = iadd(_5419, _5421, intervalFailed);
    Interval _5422 = _16072;
    Interval _16073 = isqrt(_5422, intervalFailed);
    Interval _5423 = _16073;
    Interval len = _5423;
    Interval3 param_var_a_39 = travel;
    Interval3 param_var_b_29 = outgoing;
    Interval _5371 = param_var_a_39.x;
    Interval _5372 = param_var_b_29.x;
    Interval _16081 = imul(_5371, _5372, intervalFailed, optical_product_upper);
    Interval _5373 = _16081;
    Interval _5374 = param_var_a_39.y;
    Interval _5375 = param_var_b_29.y;
    Interval _16086 = imul(_5374, _5375, intervalFailed, optical_product_upper);
    Interval _5376 = _16086;
    Interval _16087 = iadd(_5373, _5376, intervalFailed);
    Interval _5377 = _16087;
    Interval _5378 = param_var_a_39.z;
    Interval _5379 = param_var_b_29.z;
    Interval _16092 = imul(_5378, _5379, intervalFailed, optical_product_upper);
    Interval _5380 = _16092;
    Interval _16093 = iadd(_5377, _5380, intervalFailed);
    Interval _5381 = _16093;
    Interval temp_var_Interval = _5381;
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
    Interval _5349 = param_var_a_40.y;
    Interval _5350 = param_var_b_30.z;
    Interval _16110 = imul(_5349, _5350, intervalFailed, optical_product_upper);
    Interval _5351 = _16110;
    Interval _5352 = param_var_a_40.z;
    Interval _5353 = param_var_b_30.y;
    Interval _16115 = imul(_5352, _5353, intervalFailed, optical_product_upper);
    Interval _5354 = _16115;
    Interval _5345 = _5351;
    Interval _5346 = _5354;
    float _5342 = as_type<float>(as_type<uint>(_5346.hi) ^ 2147483648u);
    float _5343 = as_type<float>(as_type<uint>(_5346.lo) ^ 2147483648u);
    Interval _5340;
    _5340.lo = _5342;
    _5340.hi = _5343;
    Interval _5341 = _5340;
    Interval _5344 = _5341;
    Interval _5347 = _5344;
    Interval _16135 = iadd(_5345, _5347, intervalFailed);
    Interval _5348 = _16135;
    Interval _5355 = _5348;
    Interval _5356 = param_var_a_40.z;
    Interval _5357 = param_var_b_30.x;
    Interval _16141 = imul(_5356, _5357, intervalFailed, optical_product_upper);
    Interval _5358 = _16141;
    Interval _5359 = param_var_a_40.x;
    Interval _5360 = param_var_b_30.z;
    Interval _16146 = imul(_5359, _5360, intervalFailed, optical_product_upper);
    Interval _5361 = _16146;
    Interval _5336 = _5358;
    Interval _5337 = _5361;
    float _5333 = as_type<float>(as_type<uint>(_5337.hi) ^ 2147483648u);
    float _5334 = as_type<float>(as_type<uint>(_5337.lo) ^ 2147483648u);
    Interval _5331;
    _5331.lo = _5333;
    _5331.hi = _5334;
    Interval _5332 = _5331;
    Interval _5335 = _5332;
    Interval _5338 = _5335;
    Interval _16166 = iadd(_5336, _5338, intervalFailed);
    Interval _5339 = _16166;
    Interval _5362 = _5339;
    Interval _5363 = param_var_a_40.x;
    Interval _5364 = param_var_b_30.y;
    Interval _16172 = imul(_5363, _5364, intervalFailed, optical_product_upper);
    Interval _5365 = _16172;
    Interval _5366 = param_var_a_40.y;
    Interval _5367 = param_var_b_30.x;
    Interval _16177 = imul(_5366, _5367, intervalFailed, optical_product_upper);
    Interval _5368 = _16177;
    Interval _5327 = _5365;
    Interval _5328 = _5368;
    float _5324 = as_type<float>(as_type<uint>(_5328.hi) ^ 2147483648u);
    float _5325 = as_type<float>(as_type<uint>(_5328.lo) ^ 2147483648u);
    Interval _5322;
    _5322.lo = _5324;
    _5322.hi = _5325;
    Interval _5323 = _5322;
    Interval _5326 = _5323;
    Interval _5329 = _5326;
    Interval _16197 = iadd(_5327, _5329, intervalFailed);
    Interval _5330 = _16197;
    Interval _5369 = _5330;
    Interval3 _5320;
    _5320.x = _5355;
    _5320.y = _5362;
    _5320.z = _5369;
    Interval3 _5321 = _5320;
    Interval3 _5370 = _5321;
    Interval3 error = _5370;
    float param_var_x_9 = spvFMul(len.hi, 0.0040000001899898052215576171875);
    float _16210 = interval_up(param_var_x_9, intervalFailed);
    float param_var_a_41 = _16210;
    float param_var_b_31 = precise::max(receiver.projection.x, receiver.projection.y);
    bool param_var_upper = true;
    float _16218 = quotient_bound(param_var_a_41, param_var_b_31, param_var_upper, intervalFailed);
    float param_var_x_10 = _16218;
    float _16219 = interval_up(param_var_x_10, intervalFailed);
    float cap = _16219;
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
    uint _2379 = (group.x * 8u) >> 2u;
    uint2 task = uint2(workMap._m0[_2379], workMap._m0[_2379 + 1u]);
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
        uint _2410 = _output >> 2u;
        results._m0[_2410] = 0u;
        results._m0[_2410 + 1u] = status;
        results._m0[_2410 + 2u] = 0u;
        results._m0[_2410 + 3u] = 0u;
        uint _2417 = (_output + 16u) >> 2u;
        results._m0[_2417] = 0u;
        results._m0[_2417 + 1u] = 0u;
        results._m0[_2417 + 2u] = 0u;
        results._m0[_2417 + 3u] = 0u;
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
    uint _2456 = (at + 496u) >> 2u;
    if (any(uint4(frames._m0[_2456], frames._m0[_2456 + 1u], frames._m0[_2456 + 2u], frames._m0[_2456 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint _2474 = at >> 2u;
    ReflectionRoughFrame receiver;
    receiver.a = as_type<float4>(uint4(frames._m0[_2474], frames._m0[_2474 + 1u], frames._m0[_2474 + 2u], frames._m0[_2474 + 3u]));
    uint _2487 = (at + 16u) >> 2u;
    receiver.b = as_type<float4>(uint4(frames._m0[_2487], frames._m0[_2487 + 1u], frames._m0[_2487 + 2u], frames._m0[_2487 + 3u]));
    uint _2500 = (at + 32u) >> 2u;
    receiver.c = as_type<float4>(uint4(frames._m0[_2500], frames._m0[_2500 + 1u], frames._m0[_2500 + 2u], frames._m0[_2500 + 3u]));
    uint _2513 = (at + 48u) >> 2u;
    receiver.projection = as_type<float4>(uint4(frames._m0[_2513], frames._m0[_2513 + 1u], frames._m0[_2513 + 2u], frames._m0[_2513 + 3u]));
    uint _2526 = (at + 64u) >> 2u;
    receiver.extentClip = as_type<float4>(uint4(frames._m0[_2526], frames._m0[_2526 + 1u], frames._m0[_2526 + 2u], frames._m0[_2526 + 3u]));
    uint _2539 = (at + 80u) >> 2u;
    receiver.settings = as_type<float4>(uint4(frames._m0[_2539], frames._m0[_2539 + 1u], frames._m0[_2539 + 2u], frames._m0[_2539 + 3u]));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint h = 0u; h < 4u; h++)
    {
        uint _2555 = ((at + 96u) + (h * 48u)) >> 2u;
        planes[h].a = as_type<float4>(uint4(frames._m0[_2555], frames._m0[_2555 + 1u], frames._m0[_2555 + 2u], frames._m0[_2555 + 3u]));
        uint _2570 = ((at + 112u) + (h * 48u)) >> 2u;
        planes[h].b = as_type<float4>(uint4(frames._m0[_2570], frames._m0[_2570 + 1u], frames._m0[_2570 + 2u], frames._m0[_2570 + 3u]));
        uint _2585 = ((at + 128u) + (h * 48u)) >> 2u;
        planes[h].c = as_type<float4>(uint4(frames._m0[_2585], frames._m0[_2585 + 1u], frames._m0[_2585 + 2u], frames._m0[_2585 + 3u]));
    }
    uint _2600 = (at + 288u) >> 2u;
    ReflectionLiquidFrame liquid;
    liquid.planePoint = as_type<float4>(uint4(frames._m0[_2600], frames._m0[_2600 + 1u], frames._m0[_2600 + 2u], frames._m0[_2600 + 3u]));
    uint _2613 = (at + 304u) >> 2u;
    liquid.planeNormal = as_type<float4>(uint4(frames._m0[_2613], frames._m0[_2613 + 1u], frames._m0[_2613 + 2u], frames._m0[_2613 + 3u]));
    uint _2626 = (at + 320u) >> 2u;
    liquid.rotation0 = as_type<float4>(uint4(frames._m0[_2626], frames._m0[_2626 + 1u], frames._m0[_2626 + 2u], frames._m0[_2626 + 3u]));
    uint _2639 = (at + 336u) >> 2u;
    liquid.rotation1 = as_type<float4>(uint4(frames._m0[_2639], frames._m0[_2639 + 1u], frames._m0[_2639 + 2u], frames._m0[_2639 + 3u]));
    uint _2652 = (at + 352u) >> 2u;
    liquid.rotation2 = as_type<float4>(uint4(frames._m0[_2652], frames._m0[_2652 + 1u], frames._m0[_2652 + 2u], frames._m0[_2652 + 3u]));
    uint _2665 = (at + 368u) >> 2u;
    liquid.projection = as_type<float4>(uint4(frames._m0[_2665], frames._m0[_2665 + 1u], frames._m0[_2665 + 2u], frames._m0[_2665 + 3u]));
    uint _2678 = (at + 384u) >> 2u;
    liquid.extentClip = as_type<float4>(uint4(frames._m0[_2678], frames._m0[_2678 + 1u], frames._m0[_2678 + 2u], frames._m0[_2678 + 3u]));
    uint _2691 = (at + 400u) >> 2u;
    liquid.settings = as_type<float4>(uint4(frames._m0[_2691], frames._m0[_2691 + 1u], frames._m0[_2691 + 2u], frames._m0[_2691 + 3u]));
    uint _2704 = (at + 416u) >> 2u;
    float4 terminal = as_type<float4>(uint4(frames._m0[_2704], frames._m0[_2704 + 1u], frames._m0[_2704 + 2u], frames._m0[_2704 + 3u]));
    uint _2716 = (at + 480u) >> 2u;
    uint4 control = uint4(frames._m0[_2716], frames._m0[_2716 + 1u], frames._m0[_2716 + 2u], frames._m0[_2716 + 3u]);
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
    uint _2752 = (at + 432u) >> 2u;
    ReflectionSpecularPlane terminalPlane;
    terminalPlane.a = as_type<float4>(uint4(frames._m0[_2752], frames._m0[_2752 + 1u], frames._m0[_2752 + 2u], frames._m0[_2752 + 3u]));
    uint _2765 = (at + 448u) >> 2u;
    terminalPlane.b = as_type<float4>(uint4(frames._m0[_2765], frames._m0[_2765 + 1u], frames._m0[_2765 + 2u], frames._m0[_2765 + 3u]));
    uint _2778 = (at + 464u) >> 2u;
    terminalPlane.c = as_type<float4>(uint4(frames._m0[_2778], frames._m0[_2778 + 1u], frames._m0[_2778 + 2u], frames._m0[_2778 + 3u]));
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
        bool4 _2847 = isnan(receiver.settings);
        bool4 _2848 = isinf(receiver.settings);
        temp_var_logical_17 = !all(not(bool4(_2847.x || _2848.x, _2847.y || _2848.y, _2847.z || _2848.z, _2847.w || _2848.w)));
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
        bool4 _2910 = isnan(receiver.projection);
        bool4 _2911 = isinf(receiver.projection);
        temp_var_logical_26 = !all(not(bool4(_2910.x || _2911.x, _2910.y || _2911.y, _2910.z || _2911.z, _2910.w || _2911.w)));
    }
    bool temp_var_logical_27 = true;
    if (!temp_var_logical_26)
    {
        temp_var_logical_27 = any(receiver.projection.xy <= float2(0.0));
    }
    bool temp_var_logical_28 = true;
    if (!temp_var_logical_27)
    {
        bool4 _2926 = isnan(terminal);
        bool4 _2927 = isinf(terminal);
        temp_var_logical_28 = !all(not(bool4(_2926.x || _2927.x, _2926.y || _2927.y, _2926.z || _2927.z, _2926.w || _2927.w)));
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
    uint _2986 = (((_input + 16u) + (r * 48u)) + 16u) >> 2u;
    float4 initial = spvFAdd(as_type<float4>(uint4(regions._m0[_2986], regions._m0[_2986 + 1u], regions._m0[_2986 + 2u], regions._m0[_2986 + 3u])), float4(0.5));
    float param_var_x = initial.x;
    float _2999 = interval_down(param_var_x, intervalFailed);
    float param_var_x_1 = initial.y;
    float _3002 = interval_down(param_var_x_1, intervalFailed);
    float param_var_x_2 = initial.z;
    float _3005 = interval_up(param_var_x_2, intervalFailed);
    float param_var_x_3 = initial.w;
    float _3008 = interval_up(param_var_x_3, intervalFailed);
    float4 extent = float4(_2999, _3002, _3005, _3008);
    bool4 _3011 = isnan(extent);
    bool4 _3012 = isinf(extent);
    bool temp_var_logical_34 = true;
    if (all(not(bool4(_3011.x || _3012.x, _3011.y || _3012.y, _3011.z || _3012.z, _3011.w || _3012.w))))
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
    Interval3 _3059 = native_target(param_var_at, param_var_feature, intervalFailed, optical_product_upper, interval_divide_upper, frames, TargetSettings);
    Interval3 enclosedTarget = _3059;
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
        float _3081 = interval_down(param_var_x_4, intervalFailed);
        float param_var_lo_1 = extent.y;
        float param_var_hi_1 = extent.w;
        uint param_var_cell_1 = ij.y;
        uint param_var_count_1 = bins.y;
        float param_var_x_5 = split_axis(param_var_lo_1, param_var_hi_1, param_var_cell_1, param_var_count_1);
        float _3091 = interval_down(param_var_x_5, intervalFailed);
        float param_var_lo_2 = extent.x;
        float param_var_hi_2 = extent.z;
        uint param_var_cell_2 = ij.x + 1u;
        uint param_var_count_2 = bins.x;
        float param_var_x_6 = split_axis(param_var_lo_2, param_var_hi_2, param_var_cell_2, param_var_count_2);
        float _3101 = interval_up(param_var_x_6, intervalFailed);
        float param_var_lo_3 = extent.y;
        float param_var_hi_3 = extent.w;
        uint param_var_cell_3 = ij.y + 1u;
        uint param_var_count_3 = bins.y;
        float param_var_x_7 = split_axis(param_var_lo_3, param_var_hi_3, param_var_cell_3, param_var_count_3);
        float _3111 = interval_up(param_var_x_7, intervalFailed);
        float4 box = float4(_3081, _3091, _3101, _3111);
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
            bool _3124 = optical_excluded_target(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTerminal, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper);
            temp_var_logical_35 = _3124;
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
        __attribute__((unused)) uint _3153 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&excludedCount, laneExcluded, memory_order_relaxed);
    }
    if (laneKept != 0u)
    {
        __attribute__((unused)) uint _3157 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&keptCount, laneKept, memory_order_relaxed);
        __attribute__((unused)) uint _3159 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullX0, laneX0, memory_order_relaxed);
        __attribute__((unused)) uint _3161 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullY0, laneY0, memory_order_relaxed);
        __attribute__((unused)) uint _3163 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullX1, laneX1, memory_order_relaxed);
        __attribute__((unused)) uint _3165 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullY1, laneY1, memory_order_relaxed);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (lane == 0u)
    {
        uint _3169 = _output >> 2u;
        results._m0[_3169] = keptCount;
        results._m0[_3169 + 1u] = 0u;
        results._m0[_3169 + 2u] = cells;
        results._m0[_3169 + 3u] = excludedCount;
        uint _3178 = (_output + 16u) >> 2u;
        uint4 temp_var_ternary_2;
        if (keptCount != 0u)
        {
            temp_var_ternary_2 = uint4(hullX0, hullY0, hullX1, hullY1);
        }
        else
        {
            temp_var_ternary_2 = uint4(0u);
        }
        results._m0[_3178] = temp_var_ternary_2.x;
        results._m0[_3178 + 1u] = temp_var_ternary_2.y;
        results._m0[_3178 + 2u] = temp_var_ternary_2.z;
        results._m0[_3178 + 3u] = temp_var_ternary_2.w;
    }
}

kernel void feature_optical_stream_main(constant type_Settings& Settings [[buffer(0)]], constant type_OpticalSettings& OpticalSettings [[buffer(1)]], device type_ByteAddressBuffer& regions [[buffer(3)]], device type_ByteAddressBuffer& frames [[buffer(4)]], device type_ByteAddressBuffer& queries [[buffer(5)]], device type_ByteAddressBuffer& workMap [[buffer(6)]], device type_RWByteAddressBuffer& results [[buffer(7)]], constant type_TargetSettings& TargetSettings [[buffer(2)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]])
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

