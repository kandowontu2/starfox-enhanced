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

constant spvUnsafeArray<int2, 8> _2227 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2228 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float normalLength = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3032 = isnan(old.planePoint);
    bool4 _3033 = isinf(old.planePoint);
    bool temp_var_logical = true;
    if (all(not(bool4(_3032.x || _3033.x, _3032.y || _3033.y, _3032.z || _3033.z, _3032.w || _3033.w))))
    {
        bool4 _3039 = isnan(old.planeNormal);
        bool4 _3040 = isinf(old.planeNormal);
        temp_var_logical = !all(not(bool4(_3039.x || _3040.x, _3039.y || _3040.y, _3039.z || _3040.z, _3039.w || _3040.w)));
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        bool4 _3049 = isnan(old.rotation0);
        bool4 _3050 = isinf(old.rotation0);
        temp_var_logical_1 = !all(not(bool4(_3049.x || _3050.x, _3049.y || _3050.y, _3049.z || _3050.z, _3049.w || _3050.w)));
    }
    bool temp_var_logical_2 = true;
    if (!temp_var_logical_1)
    {
        bool4 _3059 = isnan(old.rotation1);
        bool4 _3060 = isinf(old.rotation1);
        temp_var_logical_2 = !all(not(bool4(_3059.x || _3060.x, _3059.y || _3060.y, _3059.z || _3060.z, _3059.w || _3060.w)));
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical_2)
    {
        bool4 _3069 = isnan(old.rotation2);
        bool4 _3070 = isinf(old.rotation2);
        temp_var_logical_3 = !all(not(bool4(_3069.x || _3070.x, _3069.y || _3070.y, _3069.z || _3070.z, _3069.w || _3070.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3079 = isnan(old.projection);
        bool4 _3080 = isinf(old.projection);
        temp_var_logical_4 = !all(not(bool4(_3079.x || _3080.x, _3079.y || _3080.y, _3079.z || _3080.z, _3079.w || _3080.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3089 = isnan(old.extentClip);
        bool4 _3090 = isinf(old.extentClip);
        temp_var_logical_5 = !all(not(bool4(_3089.x || _3090.x, _3089.y || _3090.y, _3089.z || _3090.z, _3089.w || _3090.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3099 = isnan(old.settings);
        bool4 _3100 = isinf(old.settings);
        temp_var_logical_6 = !all(not(bool4(_3099.x || _3100.x, _3099.y || _3100.y, _3099.z || _3100.z, _3099.w || _3100.w)));
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
    bool3 _3237 = isnan(f);
    bool3 _3238 = isinf(f);
    if (!all(not(bool3(_3237.x || _3238.x, _3237.y || _3238.y, _3237.z || _3238.z))))
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
    bool _3269 = false;
    if (param_var_p.a.w == 2.0)
    {
        _3269 = param_var_p.b.w == 2.0;
    }
    bool _3270 = false;
    if (_3269)
    {
        _3270 = param_var_p.c.w == 2.0;
    }
    bool _3271 = _3270;
    bool analytic = _3271;
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
        bool4 _3313 = isnan(plane.a);
        bool4 _3314 = isinf(plane.a);
        temp_var_logical_3 = !all(not(bool4(_3313.x || _3314.x, _3313.y || _3314.y, _3313.z || _3314.z, _3313.w || _3314.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3323 = isnan(plane.b);
        bool4 _3324 = isinf(plane.b);
        temp_var_logical_4 = !all(not(bool4(_3323.x || _3324.x, _3323.y || _3324.y, _3323.z || _3324.z, _3323.w || _3324.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3333 = isnan(plane.c);
        bool4 _3334 = isinf(plane.c);
        temp_var_logical_5 = !all(not(bool4(_3333.x || _3334.x, _3333.y || _3334.y, _3333.z || _3334.z, _3333.w || _3334.w)));
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
    bool3 _3392 = isnan(n);
    bool3 _3393 = isinf(n);
    bool temp_var_logical_10 = false;
    if (all(not(bool3(_3392.x || _3393.x, _3392.y || _3393.y, _3392.z || _3393.z))))
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
    bool _3406 = false;
    if (param_var_old.a.w == 2.0)
    {
        _3406 = param_var_old.b.w == 2.0;
    }
    bool _3407 = false;
    if (_3406)
    {
        _3407 = param_var_old.c.w == 2.0;
    }
    bool _3408 = _3407;
    bool analytic = _3408;
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
        bool4 _3450 = isnan(old.a);
        bool4 _3451 = isinf(old.a);
        temp_var_logical_3 = !all(not(bool4(_3450.x || _3451.x, _3450.y || _3451.y, _3450.z || _3451.z, _3450.w || _3451.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3460 = isnan(old.b);
        bool4 _3461 = isinf(old.b);
        temp_var_logical_4 = !all(not(bool4(_3460.x || _3461.x, _3460.y || _3461.y, _3460.z || _3461.z, _3460.w || _3461.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3470 = isnan(old.c);
        bool4 _3471 = isinf(old.c);
        temp_var_logical_5 = !all(not(bool4(_3470.x || _3471.x, _3470.y || _3471.y, _3470.z || _3471.z, _3470.w || _3471.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3480 = isnan(old.projection);
        bool4 _3481 = isinf(old.projection);
        temp_var_logical_6 = !all(not(bool4(_3480.x || _3481.x, _3480.y || _3481.y, _3480.z || _3481.z, _3480.w || _3481.w)));
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        bool4 _3490 = isnan(old.extentClip);
        bool4 _3491 = isinf(old.extentClip);
        temp_var_logical_7 = !all(not(bool4(_3490.x || _3491.x, _3490.y || _3491.y, _3490.z || _3491.z, _3490.w || _3491.w)));
    }
    bool temp_var_logical_8 = true;
    if (!temp_var_logical_7)
    {
        bool4 _3500 = isnan(old.settings);
        bool4 _3501 = isinf(old.settings);
        temp_var_logical_8 = !all(not(bool4(_3500.x || _3501.x, _3500.y || _3501.y, _3500.z || _3501.z, _3500.w || _3501.w)));
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
    bool3 _3649 = isnan(normal);
    bool3 _3650 = isinf(normal);
    bool temp_var_logical_26 = false;
    if (all(not(bool3(_3649.x || _3650.x, _3649.y || _3650.y, _3649.z || _3650.z))))
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
                    float _19877 = interval_up(param_var_x, intervalFailed);
                    temp_var_ternary_2 = _19877;
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
                float _19883 = interval_down(param_var_x_1, intervalFailed);
                temp_var_ternary_3 = _19883;
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
        float _19890 = interval_up(param_var_x_2, intervalFailed);
        temp_var_ternary_4 = _19890;
    }
    else
    {
        float param_var_x_3 = sum_1;
        float _19892 = interval_down(param_var_x_3, intervalFailed);
        temp_var_ternary_4 = _19892;
    }
    return temp_var_ternary_4;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    Interval param_var_a = a;
    bool _16090 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _16090 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _16091 = false;
    if (_16090)
    {
        _16091 = param_var_a.lo <= param_var_a.hi;
    }
    bool _16092 = _16091;
    bool temp_var_logical = false;
    if (_16092)
    {
        Interval param_var_a_1 = b;
        bool _16087 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _16087 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _16088 = false;
        if (_16087)
        {
            _16088 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _16089 = _16088;
        temp_var_logical = _16089;
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
    float _16154 = optical_add_bound(param_var_a_4, param_var_b, param_var_upper, intervalFailed);
    float param_var_lo = _16154;
    float param_var_a_5 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _16159 = optical_add_bound(param_var_a_5, param_var_b_1, param_var_upper_1, intervalFailed);
    float param_var_hi = _16159;
    Interval _16085;
    _16085.lo = param_var_lo;
    _16085.hi = param_var_hi;
    Interval _16086 = _16085;
    return _16086;
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
                float _20002 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _20002;
                float param_var_x_1 = product;
                float _20004 = interval_down(param_var_x_1, intervalFailed);
                return _20004;
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
                float _20050 = interval_up(param_var_x_2, intervalFailed);
                temp_var_ternary_1 = _20050;
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
                float _20058 = interval_down(param_var_x_3, intervalFailed);
                temp_var_ternary_3 = _20058;
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
    float _20064 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _20064;
    float param_var_x_5 = product_1;
    float _20066 = interval_down(param_var_x_5, intervalFailed);
    return _20066;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval param_var_a = a;
    bool _18040 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _18040 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _18041 = false;
    if (_18040)
    {
        _18041 = param_var_a.lo <= param_var_a.hi;
    }
    bool _18042 = _18041;
    bool temp_var_logical = false;
    if (_18042)
    {
        Interval param_var_a_1 = b;
        bool _18037 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _18037 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _18038 = false;
        if (_18037)
        {
            _18038 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _18039 = _18038;
        temp_var_logical = _18039;
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
            float _18034 = param_var_x_2;
            float _18035 = param_var_x_2;
            Interval _18032;
            _18032.lo = _18034;
            _18032.hi = _18035;
            Interval _18033 = _18032;
            Interval _18036 = _18033;
            return _18036;
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
            float _18029 = as_type<float>(as_type<uint>(param_var_a_7.hi) ^ 2147483648u);
            float _18030 = as_type<float>(as_type<uint>(param_var_a_7.lo) ^ 2147483648u);
            Interval _18027;
            _18027.lo = _18029;
            _18027.hi = _18030;
            Interval _18028 = _18027;
            Interval _18031 = _18028;
            return _18031;
        }
        Interval param_var_a_8 = b;
        float param_var_x_6 = -1.0;
        if (interval_exact_point(param_var_a_8, param_var_x_6))
        {
            Interval param_var_a_9 = a;
            float _18024 = as_type<float>(as_type<uint>(param_var_a_9.hi) ^ 2147483648u);
            float _18025 = as_type<float>(as_type<uint>(param_var_a_9.lo) ^ 2147483648u);
            Interval _18022;
            _18022.lo = _18024;
            _18022.hi = _18025;
            Interval _18023 = _18022;
            Interval _18026 = _18023;
            return _18026;
        }
    }
    Interval param_var_a_10 = a;
    bool _18019 = false;
    if (!(isnan(param_var_a_10.lo) || isinf(param_var_a_10.lo)))
    {
        _18019 = !(isnan(param_var_a_10.hi) || isinf(param_var_a_10.hi));
    }
    bool _18020 = false;
    if (_18019)
    {
        _18020 = param_var_a_10.lo <= param_var_a_10.hi;
    }
    bool _18021 = _18020;
    bool temp_var_logical_2 = false;
    if (_18021)
    {
        Interval param_var_a_11 = b;
        bool _18016 = false;
        if (!(isnan(param_var_a_11.lo) || isinf(param_var_a_11.lo)))
        {
            _18016 = !(isnan(param_var_a_11.hi) || isinf(param_var_a_11.hi));
        }
        bool _18017 = false;
        if (_18016)
        {
            _18017 = param_var_a_11.lo <= param_var_a_11.hi;
        }
        bool _18018 = _18017;
        temp_var_logical_2 = _18018;
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
    float _18228 = optical_product_bounds(param_var_a_12, param_var_b, intervalFailed, optical_product_upper);
    float p = _18228;
    float u = optical_product_upper;
    float q = p;
    float v = u;
    if (!sameB)
    {
        float param_var_a_13 = a.lo;
        float param_var_b_1 = b.hi;
        float _18238 = optical_product_bounds(param_var_a_13, param_var_b_1, intervalFailed, optical_product_upper);
        q = _18238;
        v = optical_product_upper;
    }
    float r = p;
    float w = u;
    if (!sameA)
    {
        float param_var_a_14 = a.hi;
        float param_var_b_2 = b.lo;
        float _18248 = optical_product_bounds(param_var_a_14, param_var_b_2, intervalFailed, optical_product_upper);
        r = _18248;
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
            float _18261 = optical_product_bounds(param_var_a_15, param_var_b_3, intervalFailed, optical_product_upper);
            s = _18261;
            x = optical_product_upper;
        }
    }
    float param_var_lo = precise::min(precise::min(p, q), precise::min(r, s));
    float param_var_hi = precise::max(precise::max(u, v), precise::max(w, x));
    Interval _18014;
    _18014.lo = param_var_lo;
    _18014.hi = param_var_hi;
    Interval _18015 = _18014;
    return _18015;
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float q = precise::sqrt(a);
    bool temp_var_ternary;
    float temp_var_ternary_1;
    CurvedScalar _20101;
    for (uint n = 0u; n < 8u; n++)
    {
        float param_var_ax = q;
        float param_var_ay = 0.0;
        float param_var_bx = q;
        float param_var_by = 0.0;
        float _20103 = spvFMul(param_var_ax, param_var_bx);
        float _20104 = spvFMul(param_var_ax, 4097.0);
        float _20105 = spvFSub(_20104, spvFSub(_20104, param_var_ax));
        float _20106 = spvFSub(param_var_ax, _20105);
        float _20107 = spvFMul(param_var_bx, 4097.0);
        float _20108 = spvFSub(_20107, spvFSub(_20107, param_var_bx));
        float _20109 = spvFSub(param_var_bx, _20108);
        float _20110 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_20105, _20108), _20103), spvFMul(_20105, _20109)), spvFMul(_20106, _20108)), spvFMul(_20106, _20109)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _20111 = spvFAdd(_20103, _20110);
        float _20112 = spvFSub(_20110, spvFSub(_20111, _20103));
        float _20113 = _20111;
        float _20114 = _20112;
        _20101.high = _20113;
        _20101.low = _20114;
        CurvedScalar _20102 = _20101;
        CurvedScalar _20115 = _20102;
        CurvedScalar square = _20115;
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
            float _20213 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_1 = _20213;
        }
        else
        {
            float param_var_x_1 = q;
            float _20215 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_1 = precise::max(0.0, _20215);
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
        Interval _20069;
        _20069.lo = param_var_lo;
        _20069.hi = param_var_hi;
        Interval _20070 = _20069;
        return _20070;
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _20087 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _20087;
    float _20088 = interval_down(param_var_x, intervalFailed);
    float param_var_lo_1 = precise::max(0.0, _20088);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _20093 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _20093;
    float _20094 = interval_up(param_var_x_1, intervalFailed);
    float param_var_hi_1 = _20094;
    Interval _20067;
    _20067.lo = param_var_lo_1;
    _20067.hi = param_var_hi_1;
    Interval _20068 = _20067;
    return _20068;
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float q = a / b;
    bool temp_var_ternary;
    bool temp_var_ternary_1;
    float temp_var_ternary_2;
    CurvedScalar _19661;
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
        float _19663 = spvFMul(param_var_ax, param_var_bx);
        float _19664 = spvFMul(param_var_ax, 4097.0);
        float _19665 = spvFSub(_19664, spvFSub(_19664, param_var_ax));
        float _19666 = spvFSub(param_var_ax, _19665);
        float _19667 = spvFMul(param_var_bx, 4097.0);
        float _19668 = spvFSub(_19667, spvFSub(_19667, param_var_bx));
        float _19669 = spvFSub(param_var_bx, _19668);
        float _19670 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_19665, _19668), _19663), spvFMul(_19665, _19669)), spvFMul(_19666, _19668)), spvFMul(_19666, _19669)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _19671 = spvFAdd(_19663, _19670);
        float _19672 = spvFSub(_19670, spvFSub(_19671, _19663));
        float _19673 = _19671;
        float _19674 = _19672;
        _19661.high = _19673;
        _19661.low = _19674;
        CurvedScalar _19662 = _19661;
        CurvedScalar _19675 = _19662;
        CurvedScalar product = _19675;
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
            float _19792 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_2 = _19792;
        }
        else
        {
            float param_var_x_1 = q;
            float _19794 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_2 = _19794;
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
    bool _16242 = false;
    if (param_var_a.lo <= 0.0)
    {
        _16242 = param_var_a.hi >= 0.0;
    }
    bool _16243 = _16242;
    bool temp_var_logical = true;
    if (!_16243)
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
        Interval _16240;
        _16240.lo = param_var_lo;
        _16240.hi = param_var_hi;
        Interval _16241 = _16240;
        return _16241;
    }
    Interval param_var_a_1 = a;
    bool _16237 = false;
    if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
    {
        _16237 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
    }
    bool _16238 = false;
    if (_16237)
    {
        _16238 = param_var_a_1.lo <= param_var_a_1.hi;
    }
    bool _16239 = _16238;
    bool temp_var_logical_2 = false;
    if (_16239)
    {
        Interval param_var_a_2 = b;
        bool _16234 = false;
        if (!(isnan(param_var_a_2.lo) || isinf(param_var_a_2.lo)))
        {
            _16234 = !(isnan(param_var_a_2.hi) || isinf(param_var_a_2.hi));
        }
        bool _16235 = false;
        if (_16234)
        {
            _16235 = param_var_a_2.lo <= param_var_a_2.hi;
        }
        bool _16236 = _16235;
        temp_var_logical_2 = _16236;
    }
    if (temp_var_logical_2)
    {
        Interval param_var_a_3 = a;
        float param_var_x = 0.0;
        if (interval_exact_point(param_var_a_3, param_var_x))
        {
            float param_var_x_1 = 0.0;
            float _16231 = param_var_x_1;
            float _16232 = param_var_x_1;
            Interval _16229;
            _16229.lo = _16231;
            _16229.hi = _16232;
            Interval _16230 = _16229;
            Interval _16233 = _16230;
            return _16233;
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
            float _16226 = as_type<float>(as_type<uint>(param_var_a_6.hi) ^ 2147483648u);
            float _16227 = as_type<float>(as_type<uint>(param_var_a_6.lo) ^ 2147483648u);
            Interval _16224;
            _16224.lo = _16226;
            _16224.hi = _16227;
            Interval _16225 = _16224;
            Interval _16228 = _16225;
            return _16228;
        }
    }
    float param_var_alo = a.lo;
    float param_var_ahi = a.hi;
    float param_var_blo = b.lo;
    float param_var_bhi = b.hi;
    float _16179 = param_var_alo;
    float _16180 = param_var_ahi;
    Interval _16176;
    _16176.lo = _16179;
    _16176.hi = _16180;
    Interval _16177 = _16176;
    Interval _16181 = _16177;
    bool _16173 = false;
    if (!(isnan(_16181.lo) || isinf(_16181.lo)))
    {
        _16173 = !(isnan(_16181.hi) || isinf(_16181.hi));
    }
    bool _16174 = false;
    if (_16173)
    {
        _16174 = _16181.lo <= _16181.hi;
    }
    bool _16175 = _16174;
    bool _16182 = false;
    if (_16175)
    {
        float _16183 = param_var_blo;
        float _16184 = param_var_bhi;
        Interval _16171;
        _16171.lo = _16183;
        _16171.hi = _16184;
        Interval _16172 = _16171;
        Interval _16185 = _16172;
        bool _16168 = false;
        if (!(isnan(_16185.lo) || isinf(_16185.lo)))
        {
            _16168 = !(isnan(_16185.hi) || isinf(_16185.hi));
        }
        bool _16169 = false;
        if (_16168)
        {
            _16169 = _16185.lo <= _16185.hi;
        }
        bool _16170 = _16169;
        _16182 = _16170;
    }
    bool _16178 = _16182;
    bool _16187 = false;
    if (_16178)
    {
        _16187 = as_type<uint>(param_var_alo) == as_type<uint>(param_var_ahi);
    }
    bool _16186 = _16187;
    bool _16189 = false;
    if (_16178)
    {
        _16189 = as_type<uint>(param_var_blo) == as_type<uint>(param_var_bhi);
    }
    bool _16188 = _16189;
    float _16191 = param_var_alo;
    float _16192 = param_var_blo;
    bool _16193 = false;
    float _16461 = quotient_bound(_16191, _16192, _16193, intervalFailed);
    float _16190 = _16461;
    float _16195 = param_var_alo;
    float _16196 = param_var_blo;
    bool _16197 = true;
    float _16464 = quotient_bound(_16195, _16196, _16197, intervalFailed);
    float _16194 = _16464;
    float _16198 = _16190;
    float _16199 = _16194;
    if (!_16188)
    {
        float _16200 = param_var_alo;
        float _16201 = param_var_bhi;
        bool _16202 = false;
        float _16473 = quotient_bound(_16200, _16201, _16202, intervalFailed);
        _16198 = _16473;
        float _16203 = param_var_alo;
        float _16204 = param_var_bhi;
        bool _16205 = true;
        float _16476 = quotient_bound(_16203, _16204, _16205, intervalFailed);
        _16199 = _16476;
    }
    float _16206 = _16190;
    float _16207 = _16194;
    if (!_16186)
    {
        float _16208 = param_var_ahi;
        float _16209 = param_var_blo;
        bool _16210 = false;
        float _16485 = quotient_bound(_16208, _16209, _16210, intervalFailed);
        _16206 = _16485;
        float _16211 = param_var_ahi;
        float _16212 = param_var_blo;
        bool _16213 = true;
        float _16488 = quotient_bound(_16211, _16212, _16213, intervalFailed);
        _16207 = _16488;
    }
    float _16214 = _16198;
    float _16215 = _16199;
    if (!_16186)
    {
        if (_16188)
        {
            _16214 = _16206;
            _16215 = _16207;
        }
        else
        {
            float _16216 = param_var_ahi;
            float _16217 = param_var_bhi;
            bool _16218 = false;
            float _16503 = quotient_bound(_16216, _16217, _16218, intervalFailed);
            _16214 = _16503;
            float _16219 = param_var_ahi;
            float _16220 = param_var_bhi;
            bool _16221 = true;
            float _16506 = quotient_bound(_16219, _16220, _16221, intervalFailed);
            _16215 = _16506;
        }
    }
    float _16222 = precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16190), _16198), _16206), _16214);
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16194), _16199), _16207), _16215);
    float _16223 = _16222;
    float lo = _16223;
    float param_var_x_4 = lo;
    float _16526 = interval_down(param_var_x_4, intervalFailed);
    float param_var_lo_1 = _16526;
    float param_var_x_5 = interval_divide_upper;
    float _16528 = interval_up(param_var_x_5, intervalFailed);
    float param_var_hi_1 = _16528;
    Interval _16166;
    _16166.lo = param_var_lo_1;
    _16166.hi = param_var_hi_1;
    Interval _16167 = _16166;
    return _16167;
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, device type_ByteAddressBuffer& frames, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        float param_var_x = feature.x;
        float _4148 = param_var_x;
        float _4149 = param_var_x;
        Interval _4146;
        _4146.lo = _4148;
        _4146.hi = _4149;
        Interval _4147 = _4146;
        Interval _4150 = _4147;
        Interval x = _4150;
        float param_var_x_1 = feature.y;
        float _4143 = param_var_x_1;
        float _4144 = param_var_x_1;
        Interval _4141;
        _4141.lo = _4143;
        _4141.hi = _4144;
        Interval _4142 = _4141;
        Interval _4145 = _4142;
        Interval y = _4145;
        float param_var_x_2 = 1.0;
        float _4138 = param_var_x_2;
        float _4139 = param_var_x_2;
        Interval _4136;
        _4136.lo = _4138;
        _4136.hi = _4139;
        Interval _4137 = _4136;
        Interval _4140 = _4137;
        Interval param_var_a = _4140;
        Interval param_var_a_1 = x;
        Interval param_var_b = y;
        Interval _4187 = iadd(param_var_a_1, param_var_b, intervalFailed);
        Interval param_var_b_1 = _4187;
        Interval _4132 = param_var_a;
        Interval _4133 = param_var_b_1;
        float _4129 = as_type<float>(as_type<uint>(_4133.hi) ^ 2147483648u);
        float _4130 = as_type<float>(as_type<uint>(_4133.lo) ^ 2147483648u);
        Interval _4127;
        _4127.lo = _4129;
        _4127.hi = _4130;
        Interval _4128 = _4127;
        Interval _4131 = _4128;
        Interval _4134 = _4131;
        Interval _4207 = iadd(_4132, _4134, intervalFailed);
        Interval _4135 = _4207;
        Interval a = _4135;
        uint _4210 = (at + 432u) >> 2u;
        float3 param_var_v = as_type<float4>(uint4(frames._m0[_4210], frames._m0[_4210 + 1u], frames._m0[_4210 + 2u], frames._m0[_4210 + 3u])).xyz;
        float _4120 = param_var_v.x;
        float _4117 = _4120;
        float _4118 = _4120;
        Interval _4115;
        _4115.lo = _4117;
        _4115.hi = _4118;
        Interval _4116 = _4115;
        Interval _4119 = _4116;
        Interval _4121 = _4119;
        float _4122 = param_var_v.y;
        float _4112 = _4122;
        float _4113 = _4122;
        Interval _4110;
        _4110.lo = _4112;
        _4110.hi = _4113;
        Interval _4111 = _4110;
        Interval _4114 = _4111;
        Interval _4123 = _4114;
        float _4124 = param_var_v.z;
        float _4107 = _4124;
        float _4108 = _4124;
        Interval _4105;
        _4105.lo = _4107;
        _4105.hi = _4108;
        Interval _4106 = _4105;
        Interval _4109 = _4106;
        Interval _4125 = _4109;
        Interval3 _4103;
        _4103.x = _4121;
        _4103.y = _4123;
        _4103.z = _4125;
        Interval3 _4104 = _4103;
        Interval3 _4126 = _4104;
        Interval3 param_var_a_2 = _4126;
        Interval param_var_b_2 = a;
        Interval _4093 = param_var_a_2.x;
        Interval _4094 = param_var_b_2;
        Interval _4268 = imul(_4093, _4094, intervalFailed, optical_product_upper);
        Interval _4095 = _4268;
        Interval _4096 = param_var_a_2.y;
        Interval _4097 = param_var_b_2;
        Interval _4272 = imul(_4096, _4097, intervalFailed, optical_product_upper);
        Interval _4098 = _4272;
        Interval _4099 = param_var_a_2.z;
        Interval _4100 = param_var_b_2;
        Interval _4276 = imul(_4099, _4100, intervalFailed, optical_product_upper);
        Interval _4101 = _4276;
        Interval3 _4091;
        _4091.x = _4095;
        _4091.y = _4098;
        _4091.z = _4101;
        Interval3 _4092 = _4091;
        Interval3 _4102 = _4092;
        Interval3 param_var_a_3 = _4102;
        uint _4287 = (at + 448u) >> 2u;
        float3 param_var_v_1 = as_type<float4>(uint4(frames._m0[_4287], frames._m0[_4287 + 1u], frames._m0[_4287 + 2u], frames._m0[_4287 + 3u])).xyz;
        float _4084 = param_var_v_1.x;
        float _4081 = _4084;
        float _4082 = _4084;
        Interval _4079;
        _4079.lo = _4081;
        _4079.hi = _4082;
        Interval _4080 = _4079;
        Interval _4083 = _4080;
        Interval _4085 = _4083;
        float _4086 = param_var_v_1.y;
        float _4076 = _4086;
        float _4077 = _4086;
        Interval _4074;
        _4074.lo = _4076;
        _4074.hi = _4077;
        Interval _4075 = _4074;
        Interval _4078 = _4075;
        Interval _4087 = _4078;
        float _4088 = param_var_v_1.z;
        float _4071 = _4088;
        float _4072 = _4088;
        Interval _4069;
        _4069.lo = _4071;
        _4069.hi = _4072;
        Interval _4070 = _4069;
        Interval _4073 = _4070;
        Interval _4089 = _4073;
        Interval3 _4067;
        _4067.x = _4085;
        _4067.y = _4087;
        _4067.z = _4089;
        Interval3 _4068 = _4067;
        Interval3 _4090 = _4068;
        Interval3 param_var_a_4 = _4090;
        Interval param_var_b_3 = x;
        Interval _4057 = param_var_a_4.x;
        Interval _4058 = param_var_b_3;
        Interval _4345 = imul(_4057, _4058, intervalFailed, optical_product_upper);
        Interval _4059 = _4345;
        Interval _4060 = param_var_a_4.y;
        Interval _4061 = param_var_b_3;
        Interval _4349 = imul(_4060, _4061, intervalFailed, optical_product_upper);
        Interval _4062 = _4349;
        Interval _4063 = param_var_a_4.z;
        Interval _4064 = param_var_b_3;
        Interval _4353 = imul(_4063, _4064, intervalFailed, optical_product_upper);
        Interval _4065 = _4353;
        Interval3 _4055;
        _4055.x = _4059;
        _4055.y = _4062;
        _4055.z = _4065;
        Interval3 _4056 = _4055;
        Interval3 _4066 = _4056;
        Interval3 param_var_b_4 = _4066;
        Interval _4045 = param_var_a_3.x;
        Interval _4046 = param_var_b_4.x;
        Interval _4367 = iadd(_4045, _4046, intervalFailed);
        Interval _4047 = _4367;
        Interval _4048 = param_var_a_3.y;
        Interval _4049 = param_var_b_4.y;
        Interval _4372 = iadd(_4048, _4049, intervalFailed);
        Interval _4050 = _4372;
        Interval _4051 = param_var_a_3.z;
        Interval _4052 = param_var_b_4.z;
        Interval _4377 = iadd(_4051, _4052, intervalFailed);
        Interval _4053 = _4377;
        Interval3 _4043;
        _4043.x = _4047;
        _4043.y = _4050;
        _4043.z = _4053;
        Interval3 _4044 = _4043;
        Interval3 _4054 = _4044;
        Interval3 param_var_a_5 = _4054;
        uint _4388 = (at + 464u) >> 2u;
        float3 param_var_v_2 = as_type<float4>(uint4(frames._m0[_4388], frames._m0[_4388 + 1u], frames._m0[_4388 + 2u], frames._m0[_4388 + 3u])).xyz;
        float _4036 = param_var_v_2.x;
        float _4033 = _4036;
        float _4034 = _4036;
        Interval _4031;
        _4031.lo = _4033;
        _4031.hi = _4034;
        Interval _4032 = _4031;
        Interval _4035 = _4032;
        Interval _4037 = _4035;
        float _4038 = param_var_v_2.y;
        float _4028 = _4038;
        float _4029 = _4038;
        Interval _4026;
        _4026.lo = _4028;
        _4026.hi = _4029;
        Interval _4027 = _4026;
        Interval _4030 = _4027;
        Interval _4039 = _4030;
        float _4040 = param_var_v_2.z;
        float _4023 = _4040;
        float _4024 = _4040;
        Interval _4021;
        _4021.lo = _4023;
        _4021.hi = _4024;
        Interval _4022 = _4021;
        Interval _4025 = _4022;
        Interval _4041 = _4025;
        Interval3 _4019;
        _4019.x = _4037;
        _4019.y = _4039;
        _4019.z = _4041;
        Interval3 _4020 = _4019;
        Interval3 _4042 = _4020;
        Interval3 param_var_a_6 = _4042;
        Interval param_var_b_5 = y;
        Interval _4009 = param_var_a_6.x;
        Interval _4010 = param_var_b_5;
        Interval _4446 = imul(_4009, _4010, intervalFailed, optical_product_upper);
        Interval _4011 = _4446;
        Interval _4012 = param_var_a_6.y;
        Interval _4013 = param_var_b_5;
        Interval _4450 = imul(_4012, _4013, intervalFailed, optical_product_upper);
        Interval _4014 = _4450;
        Interval _4015 = param_var_a_6.z;
        Interval _4016 = param_var_b_5;
        Interval _4454 = imul(_4015, _4016, intervalFailed, optical_product_upper);
        Interval _4017 = _4454;
        Interval3 _4007;
        _4007.x = _4011;
        _4007.y = _4014;
        _4007.z = _4017;
        Interval3 _4008 = _4007;
        Interval3 _4018 = _4008;
        Interval3 param_var_b_6 = _4018;
        Interval _3997 = param_var_a_5.x;
        Interval _3998 = param_var_b_6.x;
        Interval _4468 = iadd(_3997, _3998, intervalFailed);
        Interval _3999 = _4468;
        Interval _4000 = param_var_a_5.y;
        Interval _4001 = param_var_b_6.y;
        Interval _4473 = iadd(_4000, _4001, intervalFailed);
        Interval _4002 = _4473;
        Interval _4003 = param_var_a_5.z;
        Interval _4004 = param_var_b_6.z;
        Interval _4478 = iadd(_4003, _4004, intervalFailed);
        Interval _4005 = _4478;
        Interval3 _3995;
        _3995.x = _3999;
        _3995.y = _4002;
        _3995.z = _4005;
        Interval3 _3996 = _3995;
        Interval3 _4006 = _3996;
        return _4006;
    }
    float3 param_var_v_3 = feature.xyz;
    float _3988 = param_var_v_3.x;
    float _3985 = _3988;
    float _3986 = _3988;
    Interval _3983;
    _3983.lo = _3985;
    _3983.hi = _3986;
    Interval _3984 = _3983;
    Interval _3987 = _3984;
    Interval _3989 = _3987;
    float _3990 = param_var_v_3.y;
    float _3980 = _3990;
    float _3981 = _3990;
    Interval _3978;
    _3978.lo = _3980;
    _3978.hi = _3981;
    Interval _3979 = _3978;
    Interval _3982 = _3979;
    Interval _3991 = _3982;
    float _3992 = param_var_v_3.z;
    float _3975 = _3992;
    float _3976 = _3992;
    Interval _3973;
    _3973.lo = _3975;
    _3973.hi = _3976;
    Interval _3974 = _3973;
    Interval _3977 = _3974;
    Interval _3993 = _3977;
    Interval3 _3971;
    _3971.x = _3989;
    _3971.y = _3991;
    _3971.z = _3993;
    Interval3 _3972 = _3971;
    Interval3 _3994 = _3972;
    Interval3 raw = _3994;
    float3 param_var_v_4 = TargetSettings.targetCurrentCube[0].xyz;
    float _3964 = param_var_v_4.x;
    float _3961 = _3964;
    float _3962 = _3964;
    Interval _3959;
    _3959.lo = _3961;
    _3959.hi = _3962;
    Interval _3960 = _3959;
    Interval _3963 = _3960;
    Interval _3965 = _3963;
    float _3966 = param_var_v_4.y;
    float _3956 = _3966;
    float _3957 = _3966;
    Interval _3954;
    _3954.lo = _3956;
    _3954.hi = _3957;
    Interval _3955 = _3954;
    Interval _3958 = _3955;
    Interval _3967 = _3958;
    float _3968 = param_var_v_4.z;
    float _3951 = _3968;
    float _3952 = _3968;
    Interval _3949;
    _3949.lo = _3951;
    _3949.hi = _3952;
    Interval _3950 = _3949;
    Interval _3953 = _3950;
    Interval _3969 = _3953;
    Interval3 _3947;
    _3947.x = _3965;
    _3947.y = _3967;
    _3947.z = _3969;
    Interval3 _3948 = _3947;
    Interval3 _3970 = _3948;
    Interval3 param_var_a_7 = _3970;
    Interval3 param_var_b_7 = raw;
    Interval _3936 = param_var_a_7.x;
    Interval _3937 = param_var_b_7.x;
    Interval _4583 = imul(_3936, _3937, intervalFailed, optical_product_upper);
    Interval _3938 = _4583;
    Interval _3939 = param_var_a_7.y;
    Interval _3940 = param_var_b_7.y;
    Interval _4588 = imul(_3939, _3940, intervalFailed, optical_product_upper);
    Interval _3941 = _4588;
    Interval _4589 = iadd(_3938, _3941, intervalFailed);
    Interval _3942 = _4589;
    Interval _3943 = param_var_a_7.z;
    Interval _3944 = param_var_b_7.z;
    Interval _4594 = imul(_3943, _3944, intervalFailed, optical_product_upper);
    Interval _3945 = _4594;
    Interval _4595 = iadd(_3942, _3945, intervalFailed);
    Interval _3946 = _4595;
    Interval param_var_x_3 = _3946;
    float3 param_var_v_5 = TargetSettings.targetCurrentCube[1].xyz;
    float _3929 = param_var_v_5.x;
    float _3926 = _3929;
    float _3927 = _3929;
    Interval _3924;
    _3924.lo = _3926;
    _3924.hi = _3927;
    Interval _3925 = _3924;
    Interval _3928 = _3925;
    Interval _3930 = _3928;
    float _3931 = param_var_v_5.y;
    float _3921 = _3931;
    float _3922 = _3931;
    Interval _3919;
    _3919.lo = _3921;
    _3919.hi = _3922;
    Interval _3920 = _3919;
    Interval _3923 = _3920;
    Interval _3932 = _3923;
    float _3933 = param_var_v_5.z;
    float _3916 = _3933;
    float _3917 = _3933;
    Interval _3914;
    _3914.lo = _3916;
    _3914.hi = _3917;
    Interval _3915 = _3914;
    Interval _3918 = _3915;
    Interval _3934 = _3918;
    Interval3 _3912;
    _3912.x = _3930;
    _3912.y = _3932;
    _3912.z = _3934;
    Interval3 _3913 = _3912;
    Interval3 _3935 = _3913;
    Interval3 param_var_a_8 = _3935;
    Interval3 param_var_b_8 = raw;
    Interval _3901 = param_var_a_8.x;
    Interval _3902 = param_var_b_8.x;
    Interval _4648 = imul(_3901, _3902, intervalFailed, optical_product_upper);
    Interval _3903 = _4648;
    Interval _3904 = param_var_a_8.y;
    Interval _3905 = param_var_b_8.y;
    Interval _4653 = imul(_3904, _3905, intervalFailed, optical_product_upper);
    Interval _3906 = _4653;
    Interval _4654 = iadd(_3903, _3906, intervalFailed);
    Interval _3907 = _4654;
    Interval _3908 = param_var_a_8.z;
    Interval _3909 = param_var_b_8.z;
    Interval _4659 = imul(_3908, _3909, intervalFailed, optical_product_upper);
    Interval _3910 = _4659;
    Interval _4660 = iadd(_3907, _3910, intervalFailed);
    Interval _3911 = _4660;
    Interval param_var_y = _3911;
    float3 param_var_v_6 = TargetSettings.targetCurrentCube[2].xyz;
    float _3894 = param_var_v_6.x;
    float _3891 = _3894;
    float _3892 = _3894;
    Interval _3889;
    _3889.lo = _3891;
    _3889.hi = _3892;
    Interval _3890 = _3889;
    Interval _3893 = _3890;
    Interval _3895 = _3893;
    float _3896 = param_var_v_6.y;
    float _3886 = _3896;
    float _3887 = _3896;
    Interval _3884;
    _3884.lo = _3886;
    _3884.hi = _3887;
    Interval _3885 = _3884;
    Interval _3888 = _3885;
    Interval _3897 = _3888;
    float _3898 = param_var_v_6.z;
    float _3881 = _3898;
    float _3882 = _3898;
    Interval _3879;
    _3879.lo = _3881;
    _3879.hi = _3882;
    Interval _3880 = _3879;
    Interval _3883 = _3880;
    Interval _3899 = _3883;
    Interval3 _3877;
    _3877.x = _3895;
    _3877.y = _3897;
    _3877.z = _3899;
    Interval3 _3878 = _3877;
    Interval3 _3900 = _3878;
    Interval3 param_var_a_9 = _3900;
    Interval3 param_var_b_9 = raw;
    Interval _3866 = param_var_a_9.x;
    Interval _3867 = param_var_b_9.x;
    Interval _4713 = imul(_3866, _3867, intervalFailed, optical_product_upper);
    Interval _3868 = _4713;
    Interval _3869 = param_var_a_9.y;
    Interval _3870 = param_var_b_9.y;
    Interval _4718 = imul(_3869, _3870, intervalFailed, optical_product_upper);
    Interval _3871 = _4718;
    Interval _4719 = iadd(_3868, _3871, intervalFailed);
    Interval _3872 = _4719;
    Interval _3873 = param_var_a_9.z;
    Interval _3874 = param_var_b_9.z;
    Interval _4724 = imul(_3873, _3874, intervalFailed, optical_product_upper);
    Interval _3875 = _4724;
    Interval _4725 = iadd(_3872, _3875, intervalFailed);
    Interval _3876 = _4725;
    Interval param_var_z = _3876;
    Interval3 _3864;
    _3864.x = param_var_x_3;
    _3864.y = param_var_y;
    _3864.z = param_var_z;
    Interval3 _3865 = _3864;
    Interval3 world = _3865;
    float3 param_var_v_7 = float3(TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[2].x);
    float _3857 = param_var_v_7.x;
    float _3854 = _3857;
    float _3855 = _3857;
    Interval _3852;
    _3852.lo = _3854;
    _3852.hi = _3855;
    Interval _3853 = _3852;
    Interval _3856 = _3853;
    Interval _3858 = _3856;
    float _3859 = param_var_v_7.y;
    float _3849 = _3859;
    float _3850 = _3859;
    Interval _3847;
    _3847.lo = _3849;
    _3847.hi = _3850;
    Interval _3848 = _3847;
    Interval _3851 = _3848;
    Interval _3860 = _3851;
    float _3861 = param_var_v_7.z;
    float _3844 = _3861;
    float _3845 = _3861;
    Interval _3842;
    _3842.lo = _3844;
    _3842.hi = _3845;
    Interval _3843 = _3842;
    Interval _3846 = _3843;
    Interval _3862 = _3846;
    Interval3 _3840;
    _3840.x = _3858;
    _3840.y = _3860;
    _3840.z = _3862;
    Interval3 _3841 = _3840;
    Interval3 _3863 = _3841;
    Interval3 param_var_a_10 = _3863;
    Interval3 param_var_b_10 = world;
    Interval _3829 = param_var_a_10.x;
    Interval _3830 = param_var_b_10.x;
    Interval _4795 = imul(_3829, _3830, intervalFailed, optical_product_upper);
    Interval _3831 = _4795;
    Interval _3832 = param_var_a_10.y;
    Interval _3833 = param_var_b_10.y;
    Interval _4800 = imul(_3832, _3833, intervalFailed, optical_product_upper);
    Interval _3834 = _4800;
    Interval _4801 = iadd(_3831, _3834, intervalFailed);
    Interval _3835 = _4801;
    Interval _3836 = param_var_a_10.z;
    Interval _3837 = param_var_b_10.z;
    Interval _4806 = imul(_3836, _3837, intervalFailed, optical_product_upper);
    Interval _3838 = _4806;
    Interval _4807 = iadd(_3835, _3838, intervalFailed);
    Interval _3839 = _4807;
    Interval param_var_x_4 = _3839;
    float3 param_var_v_8 = float3(TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[2].y);
    float _3822 = param_var_v_8.x;
    float _3819 = _3822;
    float _3820 = _3822;
    Interval _3817;
    _3817.lo = _3819;
    _3817.hi = _3820;
    Interval _3818 = _3817;
    Interval _3821 = _3818;
    Interval _3823 = _3821;
    float _3824 = param_var_v_8.y;
    float _3814 = _3824;
    float _3815 = _3824;
    Interval _3812;
    _3812.lo = _3814;
    _3812.hi = _3815;
    Interval _3813 = _3812;
    Interval _3816 = _3813;
    Interval _3825 = _3816;
    float _3826 = param_var_v_8.z;
    float _3809 = _3826;
    float _3810 = _3826;
    Interval _3807;
    _3807.lo = _3809;
    _3807.hi = _3810;
    Interval _3808 = _3807;
    Interval _3811 = _3808;
    Interval _3827 = _3811;
    Interval3 _3805;
    _3805.x = _3823;
    _3805.y = _3825;
    _3805.z = _3827;
    Interval3 _3806 = _3805;
    Interval3 _3828 = _3806;
    Interval3 param_var_a_11 = _3828;
    Interval3 param_var_b_11 = world;
    Interval _3794 = param_var_a_11.x;
    Interval _3795 = param_var_b_11.x;
    Interval _4869 = imul(_3794, _3795, intervalFailed, optical_product_upper);
    Interval _3796 = _4869;
    Interval _3797 = param_var_a_11.y;
    Interval _3798 = param_var_b_11.y;
    Interval _4874 = imul(_3797, _3798, intervalFailed, optical_product_upper);
    Interval _3799 = _4874;
    Interval _4875 = iadd(_3796, _3799, intervalFailed);
    Interval _3800 = _4875;
    Interval _3801 = param_var_a_11.z;
    Interval _3802 = param_var_b_11.z;
    Interval _4880 = imul(_3801, _3802, intervalFailed, optical_product_upper);
    Interval _3803 = _4880;
    Interval _4881 = iadd(_3800, _3803, intervalFailed);
    Interval _3804 = _4881;
    Interval param_var_y_1 = _3804;
    float3 param_var_v_9 = float3(TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[2].z);
    float _3787 = param_var_v_9.x;
    float _3784 = _3787;
    float _3785 = _3787;
    Interval _3782;
    _3782.lo = _3784;
    _3782.hi = _3785;
    Interval _3783 = _3782;
    Interval _3786 = _3783;
    Interval _3788 = _3786;
    float _3789 = param_var_v_9.y;
    float _3779 = _3789;
    float _3780 = _3789;
    Interval _3777;
    _3777.lo = _3779;
    _3777.hi = _3780;
    Interval _3778 = _3777;
    Interval _3781 = _3778;
    Interval _3790 = _3781;
    float _3791 = param_var_v_9.z;
    float _3774 = _3791;
    float _3775 = _3791;
    Interval _3772;
    _3772.lo = _3774;
    _3772.hi = _3775;
    Interval _3773 = _3772;
    Interval _3776 = _3773;
    Interval _3792 = _3776;
    Interval3 _3770;
    _3770.x = _3788;
    _3770.y = _3790;
    _3770.z = _3792;
    Interval3 _3771 = _3770;
    Interval3 _3793 = _3771;
    Interval3 param_var_a_12 = _3793;
    Interval3 param_var_b_12 = world;
    Interval _3759 = param_var_a_12.x;
    Interval _3760 = param_var_b_12.x;
    Interval _4943 = imul(_3759, _3760, intervalFailed, optical_product_upper);
    Interval _3761 = _4943;
    Interval _3762 = param_var_a_12.y;
    Interval _3763 = param_var_b_12.y;
    Interval _4948 = imul(_3762, _3763, intervalFailed, optical_product_upper);
    Interval _3764 = _4948;
    Interval _4949 = iadd(_3761, _3764, intervalFailed);
    Interval _3765 = _4949;
    Interval _3766 = param_var_a_12.z;
    Interval _3767 = param_var_b_12.z;
    Interval _4954 = imul(_3766, _3767, intervalFailed, optical_product_upper);
    Interval _3768 = _4954;
    Interval _4955 = iadd(_3765, _3768, intervalFailed);
    Interval _3769 = _4955;
    Interval param_var_z_1 = _3769;
    Interval3 _3757;
    _3757.x = param_var_x_4;
    _3757.y = param_var_y_1;
    _3757.z = param_var_z_1;
    Interval3 _3758 = _3757;
    Interval3 param_var_a_13 = _3758;
    Interval3 _3750 = param_var_a_13;
    float _3751 = 1.0;
    float _3747 = _3751;
    float _3748 = _3751;
    Interval _3745;
    _3745.lo = _3747;
    _3745.hi = _3748;
    Interval _3746 = _3745;
    Interval _3749 = _3746;
    Interval _3752 = _3749;
    Interval3 _3753 = param_var_a_13;
    Interval _3736 = _3753.x;
    bool _3729 = false;
    if (_3736.lo <= 0.0)
    {
        _3729 = _3736.hi >= 0.0;
    }
    float _3728;
    if (_3729)
    {
        _3728 = 0.0;
    }
    else
    {
        _3728 = precise::min(abs(_3736.lo), abs(_3736.hi));
    }
    float _3727 = _3728;
    float _3730 = precise::max(abs(_3736.lo), abs(_3736.hi));
    float _3731 = spvFMul(_3727, _3727);
    float _5007 = interval_down(_3731, intervalFailed);
    float _3732 = precise::max(0.0, _5007);
    float _3733 = spvFMul(_3730, _3730);
    float _5011 = interval_up(_3733, intervalFailed);
    float _3734 = _5011;
    Interval _3725;
    _3725.lo = _3732;
    _3725.hi = _3734;
    Interval _3726 = _3725;
    Interval _3735 = _3726;
    Interval _3737 = _3735;
    Interval _3738 = _3753.y;
    bool _3718 = false;
    if (_3738.lo <= 0.0)
    {
        _3718 = _3738.hi >= 0.0;
    }
    float _3717;
    if (_3718)
    {
        _3717 = 0.0;
    }
    else
    {
        _3717 = precise::min(abs(_3738.lo), abs(_3738.hi));
    }
    float _3716 = _3717;
    float _3719 = precise::max(abs(_3738.lo), abs(_3738.hi));
    float _3720 = spvFMul(_3716, _3716);
    float _5050 = interval_down(_3720, intervalFailed);
    float _3721 = precise::max(0.0, _5050);
    float _3722 = spvFMul(_3719, _3719);
    float _5054 = interval_up(_3722, intervalFailed);
    float _3723 = _5054;
    Interval _3714;
    _3714.lo = _3721;
    _3714.hi = _3723;
    Interval _3715 = _3714;
    Interval _3724 = _3715;
    Interval _3739 = _3724;
    Interval _5062 = iadd(_3737, _3739, intervalFailed);
    Interval _3740 = _5062;
    Interval _3741 = _3753.z;
    bool _3707 = false;
    if (_3741.lo <= 0.0)
    {
        _3707 = _3741.hi >= 0.0;
    }
    float _3706;
    if (_3707)
    {
        _3706 = 0.0;
    }
    else
    {
        _3706 = precise::min(abs(_3741.lo), abs(_3741.hi));
    }
    float _3705 = _3706;
    float _3708 = precise::max(abs(_3741.lo), abs(_3741.hi));
    float _3709 = spvFMul(_3705, _3705);
    float _5094 = interval_down(_3709, intervalFailed);
    float _3710 = precise::max(0.0, _5094);
    float _3711 = spvFMul(_3708, _3708);
    float _5098 = interval_up(_3711, intervalFailed);
    float _3712 = _5098;
    Interval _3703;
    _3703.lo = _3710;
    _3703.hi = _3712;
    Interval _3704 = _3703;
    Interval _3713 = _3704;
    Interval _3742 = _3713;
    Interval _5106 = iadd(_3740, _3742, intervalFailed);
    Interval _3743 = _5106;
    Interval _5107 = isqrt(_3743, intervalFailed);
    Interval _3744 = _5107;
    Interval _3754 = _3744;
    Interval _5109 = idiv(_3752, _3754, intervalFailed, interval_divide_upper);
    Interval _3755 = _5109;
    Interval _3693 = _3750.x;
    Interval _3694 = _3755;
    Interval _5113 = imul(_3693, _3694, intervalFailed, optical_product_upper);
    Interval _3695 = _5113;
    Interval _3696 = _3750.y;
    Interval _3697 = _3755;
    Interval _5117 = imul(_3696, _3697, intervalFailed, optical_product_upper);
    Interval _3698 = _5117;
    Interval _3699 = _3750.z;
    Interval _3700 = _3755;
    Interval _5121 = imul(_3699, _3700, intervalFailed, optical_product_upper);
    Interval _3701 = _5121;
    Interval3 _3691;
    _3691.x = _3695;
    _3691.y = _3698;
    _3691.z = _3701;
    Interval3 _3692 = _3691;
    Interval3 _3702 = _3692;
    Interval3 _3756 = _3702;
    return _3756;
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
    bool _16914 = false;
    if (param_var_p.a.w == 2.0)
    {
        _16914 = param_var_p.b.w == 2.0;
    }
    bool _16915 = false;
    if (_16914)
    {
        _16915 = param_var_p.c.w == 2.0;
    }
    bool _16916 = _16915;
    if (_16916)
    {
        float3 param_var_v = p.b.xyz;
        float _16907 = param_var_v.x;
        float _16904 = _16907;
        float _16905 = _16907;
        Interval _16902;
        _16902.lo = _16904;
        _16902.hi = _16905;
        Interval _16903 = _16902;
        Interval _16906 = _16903;
        Interval _16908 = _16906;
        float _16909 = param_var_v.y;
        float _16899 = _16909;
        float _16900 = _16909;
        Interval _16897;
        _16897.lo = _16899;
        _16897.hi = _16900;
        Interval _16898 = _16897;
        Interval _16901 = _16898;
        Interval _16910 = _16901;
        float _16911 = param_var_v.z;
        float _16894 = _16911;
        float _16895 = _16911;
        Interval _16892;
        _16892.lo = _16894;
        _16892.hi = _16895;
        Interval _16893 = _16892;
        Interval _16896 = _16893;
        Interval _16912 = _16896;
        Interval3 _16890;
        _16890.x = _16908;
        _16890.y = _16910;
        _16890.z = _16912;
        Interval3 _16891 = _16890;
        Interval3 _16913 = _16891;
        Interval3 param_var_a = _16913;
        Interval3 _16883 = param_var_a;
        float _16884 = 1.0;
        float _16880 = _16884;
        float _16881 = _16884;
        Interval _16878;
        _16878.lo = _16880;
        _16878.hi = _16881;
        Interval _16879 = _16878;
        Interval _16882 = _16879;
        Interval _16885 = _16882;
        Interval3 _16886 = param_var_a;
        Interval _16869 = _16886.x;
        bool _16862 = false;
        if (_16869.lo <= 0.0)
        {
            _16862 = _16869.hi >= 0.0;
        }
        float _16861;
        if (_16862)
        {
            _16861 = 0.0;
        }
        else
        {
            _16861 = precise::min(abs(_16869.lo), abs(_16869.hi));
        }
        float _16860 = _16861;
        float _16863 = precise::max(abs(_16869.lo), abs(_16869.hi));
        float _16864 = spvFMul(_16860, _16860);
        float _17024 = interval_down(_16864, intervalFailed);
        float _16865 = precise::max(0.0, _17024);
        float _16866 = spvFMul(_16863, _16863);
        float _17028 = interval_up(_16866, intervalFailed);
        float _16867 = _17028;
        Interval _16858;
        _16858.lo = _16865;
        _16858.hi = _16867;
        Interval _16859 = _16858;
        Interval _16868 = _16859;
        Interval _16870 = _16868;
        Interval _16871 = _16886.y;
        bool _16851 = false;
        if (_16871.lo <= 0.0)
        {
            _16851 = _16871.hi >= 0.0;
        }
        float _16850;
        if (_16851)
        {
            _16850 = 0.0;
        }
        else
        {
            _16850 = precise::min(abs(_16871.lo), abs(_16871.hi));
        }
        float _16849 = _16850;
        float _16852 = precise::max(abs(_16871.lo), abs(_16871.hi));
        float _16853 = spvFMul(_16849, _16849);
        float _17067 = interval_down(_16853, intervalFailed);
        float _16854 = precise::max(0.0, _17067);
        float _16855 = spvFMul(_16852, _16852);
        float _17071 = interval_up(_16855, intervalFailed);
        float _16856 = _17071;
        Interval _16847;
        _16847.lo = _16854;
        _16847.hi = _16856;
        Interval _16848 = _16847;
        Interval _16857 = _16848;
        Interval _16872 = _16857;
        Interval _17079 = iadd(_16870, _16872, intervalFailed);
        Interval _16873 = _17079;
        Interval _16874 = _16886.z;
        bool _16840 = false;
        if (_16874.lo <= 0.0)
        {
            _16840 = _16874.hi >= 0.0;
        }
        float _16839;
        if (_16840)
        {
            _16839 = 0.0;
        }
        else
        {
            _16839 = precise::min(abs(_16874.lo), abs(_16874.hi));
        }
        float _16838 = _16839;
        float _16841 = precise::max(abs(_16874.lo), abs(_16874.hi));
        float _16842 = spvFMul(_16838, _16838);
        float _17111 = interval_down(_16842, intervalFailed);
        float _16843 = precise::max(0.0, _17111);
        float _16844 = spvFMul(_16841, _16841);
        float _17115 = interval_up(_16844, intervalFailed);
        float _16845 = _17115;
        Interval _16836;
        _16836.lo = _16843;
        _16836.hi = _16845;
        Interval _16837 = _16836;
        Interval _16846 = _16837;
        Interval _16875 = _16846;
        Interval _17123 = iadd(_16873, _16875, intervalFailed);
        Interval _16876 = _17123;
        Interval _17124 = isqrt(_16876, intervalFailed);
        Interval _16877 = _17124;
        Interval _16887 = _16877;
        Interval _17126 = idiv(_16885, _16887, intervalFailed, interval_divide_upper);
        Interval _16888 = _17126;
        Interval _16826 = _16883.x;
        Interval _16827 = _16888;
        Interval _17130 = imul(_16826, _16827, intervalFailed, optical_product_upper);
        Interval _16828 = _17130;
        Interval _16829 = _16883.y;
        Interval _16830 = _16888;
        Interval _17134 = imul(_16829, _16830, intervalFailed, optical_product_upper);
        Interval _16831 = _17134;
        Interval _16832 = _16883.z;
        Interval _16833 = _16888;
        Interval _17138 = imul(_16832, _16833, intervalFailed, optical_product_upper);
        Interval _16834 = _17138;
        Interval3 _16824;
        _16824.x = _16828;
        _16824.y = _16831;
        _16824.z = _16834;
        Interval3 _16825 = _16824;
        Interval3 _16835 = _16825;
        Interval3 _16889 = _16835;
        return _16889;
    }
    float3 param_var_v_1 = p.b.xyz;
    float _16817 = param_var_v_1.x;
    float _16814 = _16817;
    float _16815 = _16817;
    Interval _16812;
    _16812.lo = _16814;
    _16812.hi = _16815;
    Interval _16813 = _16812;
    Interval _16816 = _16813;
    Interval _16818 = _16816;
    float _16819 = param_var_v_1.y;
    float _16809 = _16819;
    float _16810 = _16819;
    Interval _16807;
    _16807.lo = _16809;
    _16807.hi = _16810;
    Interval _16808 = _16807;
    Interval _16811 = _16808;
    Interval _16820 = _16811;
    float _16821 = param_var_v_1.z;
    float _16804 = _16821;
    float _16805 = _16821;
    Interval _16802;
    _16802.lo = _16804;
    _16802.hi = _16805;
    Interval _16803 = _16802;
    Interval _16806 = _16803;
    Interval _16822 = _16806;
    Interval3 _16800;
    _16800.x = _16818;
    _16800.y = _16820;
    _16800.z = _16822;
    Interval3 _16801 = _16800;
    Interval3 _16823 = _16801;
    Interval3 param_var_a_1 = _16823;
    float3 param_var_v_2 = p.a.xyz;
    float _16793 = param_var_v_2.x;
    float _16790 = _16793;
    float _16791 = _16793;
    Interval _16788;
    _16788.lo = _16790;
    _16788.hi = _16791;
    Interval _16789 = _16788;
    Interval _16792 = _16789;
    Interval _16794 = _16792;
    float _16795 = param_var_v_2.y;
    float _16785 = _16795;
    float _16786 = _16795;
    Interval _16783;
    _16783.lo = _16785;
    _16783.hi = _16786;
    Interval _16784 = _16783;
    Interval _16787 = _16784;
    Interval _16796 = _16787;
    float _16797 = param_var_v_2.z;
    float _16780 = _16797;
    float _16781 = _16797;
    Interval _16778;
    _16778.lo = _16780;
    _16778.hi = _16781;
    Interval _16779 = _16778;
    Interval _16782 = _16779;
    Interval _16798 = _16782;
    Interval3 _16776;
    _16776.x = _16794;
    _16776.y = _16796;
    _16776.z = _16798;
    Interval3 _16777 = _16776;
    Interval3 _16799 = _16777;
    Interval3 param_var_b = _16799;
    Interval3 _16767 = param_var_a_1;
    Interval _16768 = param_var_b.x;
    float _16764 = as_type<float>(as_type<uint>(_16768.hi) ^ 2147483648u);
    float _16765 = as_type<float>(as_type<uint>(_16768.lo) ^ 2147483648u);
    Interval _16762;
    _16762.lo = _16764;
    _16762.hi = _16765;
    Interval _16763 = _16762;
    Interval _16766 = _16763;
    Interval _16769 = _16766;
    Interval _16770 = param_var_b.y;
    float _16759 = as_type<float>(as_type<uint>(_16770.hi) ^ 2147483648u);
    float _16760 = as_type<float>(as_type<uint>(_16770.lo) ^ 2147483648u);
    Interval _16757;
    _16757.lo = _16759;
    _16757.hi = _16760;
    Interval _16758 = _16757;
    Interval _16761 = _16758;
    Interval _16771 = _16761;
    Interval _16772 = param_var_b.z;
    float _16754 = as_type<float>(as_type<uint>(_16772.hi) ^ 2147483648u);
    float _16755 = as_type<float>(as_type<uint>(_16772.lo) ^ 2147483648u);
    Interval _16752;
    _16752.lo = _16754;
    _16752.hi = _16755;
    Interval _16753 = _16752;
    Interval _16756 = _16753;
    Interval _16773 = _16756;
    Interval3 _16750;
    _16750.x = _16769;
    _16750.y = _16771;
    _16750.z = _16773;
    Interval3 _16751 = _16750;
    Interval3 _16774 = _16751;
    Interval _16740 = _16767.x;
    Interval _16741 = _16774.x;
    Interval _17309 = iadd(_16740, _16741, intervalFailed);
    Interval _16742 = _17309;
    Interval _16743 = _16767.y;
    Interval _16744 = _16774.y;
    Interval _17314 = iadd(_16743, _16744, intervalFailed);
    Interval _16745 = _17314;
    Interval _16746 = _16767.z;
    Interval _16747 = _16774.z;
    Interval _17319 = iadd(_16746, _16747, intervalFailed);
    Interval _16748 = _17319;
    Interval3 _16738;
    _16738.x = _16742;
    _16738.y = _16745;
    _16738.z = _16748;
    Interval3 _16739 = _16738;
    Interval3 _16749 = _16739;
    Interval3 _16775 = _16749;
    Interval3 param_var_a_2 = _16775;
    float3 param_var_v_3 = p.c.xyz;
    float _16731 = param_var_v_3.x;
    float _16728 = _16731;
    float _16729 = _16731;
    Interval _16726;
    _16726.lo = _16728;
    _16726.hi = _16729;
    Interval _16727 = _16726;
    Interval _16730 = _16727;
    Interval _16732 = _16730;
    float _16733 = param_var_v_3.y;
    float _16723 = _16733;
    float _16724 = _16733;
    Interval _16721;
    _16721.lo = _16723;
    _16721.hi = _16724;
    Interval _16722 = _16721;
    Interval _16725 = _16722;
    Interval _16734 = _16725;
    float _16735 = param_var_v_3.z;
    float _16718 = _16735;
    float _16719 = _16735;
    Interval _16716;
    _16716.lo = _16718;
    _16716.hi = _16719;
    Interval _16717 = _16716;
    Interval _16720 = _16717;
    Interval _16736 = _16720;
    Interval3 _16714;
    _16714.x = _16732;
    _16714.y = _16734;
    _16714.z = _16736;
    Interval3 _16715 = _16714;
    Interval3 _16737 = _16715;
    Interval3 param_var_a_3 = _16737;
    float3 param_var_v_4 = p.a.xyz;
    float _16707 = param_var_v_4.x;
    float _16704 = _16707;
    float _16705 = _16707;
    Interval _16702;
    _16702.lo = _16704;
    _16702.hi = _16705;
    Interval _16703 = _16702;
    Interval _16706 = _16703;
    Interval _16708 = _16706;
    float _16709 = param_var_v_4.y;
    float _16699 = _16709;
    float _16700 = _16709;
    Interval _16697;
    _16697.lo = _16699;
    _16697.hi = _16700;
    Interval _16698 = _16697;
    Interval _16701 = _16698;
    Interval _16710 = _16701;
    float _16711 = param_var_v_4.z;
    float _16694 = _16711;
    float _16695 = _16711;
    Interval _16692;
    _16692.lo = _16694;
    _16692.hi = _16695;
    Interval _16693 = _16692;
    Interval _16696 = _16693;
    Interval _16712 = _16696;
    Interval3 _16690;
    _16690.x = _16708;
    _16690.y = _16710;
    _16690.z = _16712;
    Interval3 _16691 = _16690;
    Interval3 _16713 = _16691;
    Interval3 param_var_b_1 = _16713;
    Interval3 _16681 = param_var_a_3;
    Interval _16682 = param_var_b_1.x;
    float _16678 = as_type<float>(as_type<uint>(_16682.hi) ^ 2147483648u);
    float _16679 = as_type<float>(as_type<uint>(_16682.lo) ^ 2147483648u);
    Interval _16676;
    _16676.lo = _16678;
    _16676.hi = _16679;
    Interval _16677 = _16676;
    Interval _16680 = _16677;
    Interval _16683 = _16680;
    Interval _16684 = param_var_b_1.y;
    float _16673 = as_type<float>(as_type<uint>(_16684.hi) ^ 2147483648u);
    float _16674 = as_type<float>(as_type<uint>(_16684.lo) ^ 2147483648u);
    Interval _16671;
    _16671.lo = _16673;
    _16671.hi = _16674;
    Interval _16672 = _16671;
    Interval _16675 = _16672;
    Interval _16685 = _16675;
    Interval _16686 = param_var_b_1.z;
    float _16668 = as_type<float>(as_type<uint>(_16686.hi) ^ 2147483648u);
    float _16669 = as_type<float>(as_type<uint>(_16686.lo) ^ 2147483648u);
    Interval _16666;
    _16666.lo = _16668;
    _16666.hi = _16669;
    Interval _16667 = _16666;
    Interval _16670 = _16667;
    Interval _16687 = _16670;
    Interval3 _16664;
    _16664.x = _16683;
    _16664.y = _16685;
    _16664.z = _16687;
    Interval3 _16665 = _16664;
    Interval3 _16688 = _16665;
    Interval _16654 = _16681.x;
    Interval _16655 = _16688.x;
    Interval _17490 = iadd(_16654, _16655, intervalFailed);
    Interval _16656 = _17490;
    Interval _16657 = _16681.y;
    Interval _16658 = _16688.y;
    Interval _17495 = iadd(_16657, _16658, intervalFailed);
    Interval _16659 = _17495;
    Interval _16660 = _16681.z;
    Interval _16661 = _16688.z;
    Interval _17500 = iadd(_16660, _16661, intervalFailed);
    Interval _16662 = _17500;
    Interval3 _16652;
    _16652.x = _16656;
    _16652.y = _16659;
    _16652.z = _16662;
    Interval3 _16653 = _16652;
    Interval3 _16663 = _16653;
    Interval3 _16689 = _16663;
    Interval3 param_var_b_2 = _16689;
    Interval _16630 = param_var_a_2.y;
    Interval _16631 = param_var_b_2.z;
    Interval _17515 = imul(_16630, _16631, intervalFailed, optical_product_upper);
    Interval _16632 = _17515;
    Interval _16633 = param_var_a_2.z;
    Interval _16634 = param_var_b_2.y;
    Interval _17520 = imul(_16633, _16634, intervalFailed, optical_product_upper);
    Interval _16635 = _17520;
    Interval _16626 = _16632;
    Interval _16627 = _16635;
    float _16623 = as_type<float>(as_type<uint>(_16627.hi) ^ 2147483648u);
    float _16624 = as_type<float>(as_type<uint>(_16627.lo) ^ 2147483648u);
    Interval _16621;
    _16621.lo = _16623;
    _16621.hi = _16624;
    Interval _16622 = _16621;
    Interval _16625 = _16622;
    Interval _16628 = _16625;
    Interval _17540 = iadd(_16626, _16628, intervalFailed);
    Interval _16629 = _17540;
    Interval _16636 = _16629;
    Interval _16637 = param_var_a_2.z;
    Interval _16638 = param_var_b_2.x;
    Interval _17546 = imul(_16637, _16638, intervalFailed, optical_product_upper);
    Interval _16639 = _17546;
    Interval _16640 = param_var_a_2.x;
    Interval _16641 = param_var_b_2.z;
    Interval _17551 = imul(_16640, _16641, intervalFailed, optical_product_upper);
    Interval _16642 = _17551;
    Interval _16617 = _16639;
    Interval _16618 = _16642;
    float _16614 = as_type<float>(as_type<uint>(_16618.hi) ^ 2147483648u);
    float _16615 = as_type<float>(as_type<uint>(_16618.lo) ^ 2147483648u);
    Interval _16612;
    _16612.lo = _16614;
    _16612.hi = _16615;
    Interval _16613 = _16612;
    Interval _16616 = _16613;
    Interval _16619 = _16616;
    Interval _17571 = iadd(_16617, _16619, intervalFailed);
    Interval _16620 = _17571;
    Interval _16643 = _16620;
    Interval _16644 = param_var_a_2.x;
    Interval _16645 = param_var_b_2.y;
    Interval _17577 = imul(_16644, _16645, intervalFailed, optical_product_upper);
    Interval _16646 = _17577;
    Interval _16647 = param_var_a_2.y;
    Interval _16648 = param_var_b_2.x;
    Interval _17582 = imul(_16647, _16648, intervalFailed, optical_product_upper);
    Interval _16649 = _17582;
    Interval _16608 = _16646;
    Interval _16609 = _16649;
    float _16605 = as_type<float>(as_type<uint>(_16609.hi) ^ 2147483648u);
    float _16606 = as_type<float>(as_type<uint>(_16609.lo) ^ 2147483648u);
    Interval _16603;
    _16603.lo = _16605;
    _16603.hi = _16606;
    Interval _16604 = _16603;
    Interval _16607 = _16604;
    Interval _16610 = _16607;
    Interval _17602 = iadd(_16608, _16610, intervalFailed);
    Interval _16611 = _17602;
    Interval _16650 = _16611;
    Interval3 _16601;
    _16601.x = _16636;
    _16601.y = _16643;
    _16601.z = _16650;
    Interval3 _16602 = _16601;
    Interval3 _16651 = _16602;
    Interval3 param_var_a_4 = _16651;
    Interval3 _16594 = param_var_a_4;
    float _16595 = 1.0;
    float _16591 = _16595;
    float _16592 = _16595;
    Interval _16589;
    _16589.lo = _16591;
    _16589.hi = _16592;
    Interval _16590 = _16589;
    Interval _16593 = _16590;
    Interval _16596 = _16593;
    Interval3 _16597 = param_var_a_4;
    Interval _16580 = _16597.x;
    bool _16573 = false;
    if (_16580.lo <= 0.0)
    {
        _16573 = _16580.hi >= 0.0;
    }
    float _16572;
    if (_16573)
    {
        _16572 = 0.0;
    }
    else
    {
        _16572 = precise::min(abs(_16580.lo), abs(_16580.hi));
    }
    float _16571 = _16572;
    float _16574 = precise::max(abs(_16580.lo), abs(_16580.hi));
    float _16575 = spvFMul(_16571, _16571);
    float _17655 = interval_down(_16575, intervalFailed);
    float _16576 = precise::max(0.0, _17655);
    float _16577 = spvFMul(_16574, _16574);
    float _17659 = interval_up(_16577, intervalFailed);
    float _16578 = _17659;
    Interval _16569;
    _16569.lo = _16576;
    _16569.hi = _16578;
    Interval _16570 = _16569;
    Interval _16579 = _16570;
    Interval _16581 = _16579;
    Interval _16582 = _16597.y;
    bool _16562 = false;
    if (_16582.lo <= 0.0)
    {
        _16562 = _16582.hi >= 0.0;
    }
    float _16561;
    if (_16562)
    {
        _16561 = 0.0;
    }
    else
    {
        _16561 = precise::min(abs(_16582.lo), abs(_16582.hi));
    }
    float _16560 = _16561;
    float _16563 = precise::max(abs(_16582.lo), abs(_16582.hi));
    float _16564 = spvFMul(_16560, _16560);
    float _17698 = interval_down(_16564, intervalFailed);
    float _16565 = precise::max(0.0, _17698);
    float _16566 = spvFMul(_16563, _16563);
    float _17702 = interval_up(_16566, intervalFailed);
    float _16567 = _17702;
    Interval _16558;
    _16558.lo = _16565;
    _16558.hi = _16567;
    Interval _16559 = _16558;
    Interval _16568 = _16559;
    Interval _16583 = _16568;
    Interval _17710 = iadd(_16581, _16583, intervalFailed);
    Interval _16584 = _17710;
    Interval _16585 = _16597.z;
    bool _16551 = false;
    if (_16585.lo <= 0.0)
    {
        _16551 = _16585.hi >= 0.0;
    }
    float _16550;
    if (_16551)
    {
        _16550 = 0.0;
    }
    else
    {
        _16550 = precise::min(abs(_16585.lo), abs(_16585.hi));
    }
    float _16549 = _16550;
    float _16552 = precise::max(abs(_16585.lo), abs(_16585.hi));
    float _16553 = spvFMul(_16549, _16549);
    float _17742 = interval_down(_16553, intervalFailed);
    float _16554 = precise::max(0.0, _17742);
    float _16555 = spvFMul(_16552, _16552);
    float _17746 = interval_up(_16555, intervalFailed);
    float _16556 = _17746;
    Interval _16547;
    _16547.lo = _16554;
    _16547.hi = _16556;
    Interval _16548 = _16547;
    Interval _16557 = _16548;
    Interval _16586 = _16557;
    Interval _17754 = iadd(_16584, _16586, intervalFailed);
    Interval _16587 = _17754;
    Interval _17755 = isqrt(_16587, intervalFailed);
    Interval _16588 = _17755;
    Interval _16598 = _16588;
    Interval _17757 = idiv(_16596, _16598, intervalFailed, interval_divide_upper);
    Interval _16599 = _17757;
    Interval _16537 = _16594.x;
    Interval _16538 = _16599;
    Interval _17761 = imul(_16537, _16538, intervalFailed, optical_product_upper);
    Interval _16539 = _17761;
    Interval _16540 = _16594.y;
    Interval _16541 = _16599;
    Interval _17765 = imul(_16540, _16541, intervalFailed, optical_product_upper);
    Interval _16542 = _17765;
    Interval _16543 = _16594.z;
    Interval _16544 = _16599;
    Interval _17769 = imul(_16543, _16544, intervalFailed, optical_product_upper);
    Interval _16545 = _17769;
    Interval3 _16535;
    _16535.x = _16539;
    _16535.y = _16542;
    _16535.z = _16545;
    Interval3 _16536 = _16535;
    Interval3 _16546 = _16536;
    Interval3 _16600 = _16546;
    return _16600;
}

static inline __attribute__((always_inline))
Interval3 oriented(thread const Interval3& n, thread const Interval3& direction, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval3 param_var_a = n;
    Interval3 param_var_b = direction;
    Interval _17841 = param_var_a.x;
    Interval _17842 = param_var_b.x;
    Interval _17858 = imul(_17841, _17842, intervalFailed, optical_product_upper);
    Interval _17843 = _17858;
    Interval _17844 = param_var_a.y;
    Interval _17845 = param_var_b.y;
    Interval _17863 = imul(_17844, _17845, intervalFailed, optical_product_upper);
    Interval _17846 = _17863;
    Interval _17864 = iadd(_17843, _17846, intervalFailed);
    Interval _17847 = _17864;
    Interval _17848 = param_var_a.z;
    Interval _17849 = param_var_b.z;
    Interval _17869 = imul(_17848, _17849, intervalFailed, optical_product_upper);
    Interval _17850 = _17869;
    Interval _17870 = iadd(_17847, _17850, intervalFailed);
    Interval _17851 = _17870;
    Interval _dot = _17851;
    if (_dot.lo > 0.0)
    {
        Interval3 param_var_a_1 = n;
        float param_var_x = -1.0;
        float _17838 = param_var_x;
        float _17839 = param_var_x;
        Interval _17836;
        _17836.lo = _17838;
        _17836.hi = _17839;
        Interval _17837 = _17836;
        Interval _17840 = _17837;
        Interval param_var_b_1 = _17840;
        Interval _17826 = param_var_a_1.x;
        Interval _17827 = param_var_b_1;
        Interval _17888 = imul(_17826, _17827, intervalFailed, optical_product_upper);
        Interval _17828 = _17888;
        Interval _17829 = param_var_a_1.y;
        Interval _17830 = param_var_b_1;
        Interval _17892 = imul(_17829, _17830, intervalFailed, optical_product_upper);
        Interval _17831 = _17892;
        Interval _17832 = param_var_a_1.z;
        Interval _17833 = param_var_b_1;
        Interval _17896 = imul(_17832, _17833, intervalFailed, optical_product_upper);
        Interval _17834 = _17896;
        Interval3 _17824;
        _17824.x = _17828;
        _17824.y = _17831;
        _17824.z = _17834;
        Interval3 _17825 = _17824;
        Interval3 _17835 = _17825;
        return _17835;
    }
    if (_dot.hi <= 0.0)
    {
        return n;
    }
    Interval3 param_var_a_2 = n;
    Interval3 param_var_a_3 = n;
    float param_var_x_1 = -1.0;
    float _17821 = param_var_x_1;
    float _17822 = param_var_x_1;
    Interval _17819;
    _17819.lo = _17821;
    _17819.hi = _17822;
    Interval _17820 = _17819;
    Interval _17823 = _17820;
    Interval param_var_b_2 = _17823;
    Interval _17809 = param_var_a_3.x;
    Interval _17810 = param_var_b_2;
    Interval _17924 = imul(_17809, _17810, intervalFailed, optical_product_upper);
    Interval _17811 = _17924;
    Interval _17812 = param_var_a_3.y;
    Interval _17813 = param_var_b_2;
    Interval _17928 = imul(_17812, _17813, intervalFailed, optical_product_upper);
    Interval _17814 = _17928;
    Interval _17815 = param_var_a_3.z;
    Interval _17816 = param_var_b_2;
    Interval _17932 = imul(_17815, _17816, intervalFailed, optical_product_upper);
    Interval _17817 = _17932;
    Interval3 _17807;
    _17807.x = _17811;
    _17807.y = _17814;
    _17807.z = _17817;
    Interval3 _17808 = _17807;
    Interval3 _17818 = _17808;
    Interval3 param_var_b_3 = _17818;
    Interval _17797 = param_var_a_2.x;
    Interval _17798 = param_var_b_3.x;
    float _17794 = precise::min(_17797.lo, _17798.lo);
    float _17795 = precise::max(_17797.hi, _17798.hi);
    Interval _17792;
    _17792.lo = _17794;
    _17792.hi = _17795;
    Interval _17793 = _17792;
    Interval _17796 = _17793;
    Interval _17799 = _17796;
    Interval _17800 = param_var_a_2.y;
    Interval _17801 = param_var_b_3.y;
    float _17789 = precise::min(_17800.lo, _17801.lo);
    float _17790 = precise::max(_17800.hi, _17801.hi);
    Interval _17787;
    _17787.lo = _17789;
    _17787.hi = _17790;
    Interval _17788 = _17787;
    Interval _17791 = _17788;
    Interval _17802 = _17791;
    Interval _17803 = param_var_a_2.z;
    Interval _17804 = param_var_b_3.z;
    float _17784 = precise::min(_17803.lo, _17804.lo);
    float _17785 = precise::max(_17803.hi, _17804.hi);
    Interval _17782;
    _17782.lo = _17784;
    _17782.hi = _17785;
    Interval _17783 = _17782;
    Interval _17786 = _17783;
    Interval _17805 = _17786;
    Interval3 _17780;
    _17780.x = _17799;
    _17780.y = _17802;
    _17780.z = _17805;
    Interval3 _17781 = _17780;
    Interval3 _17806 = _17781;
    return _17806;
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        float param_var_x = n;
        float _18339 = param_var_x;
        float _18340 = param_var_x;
        Interval _18337;
        _18337.lo = _18339;
        _18337.hi = _18340;
        Interval _18338 = _18337;
        Interval _18341 = _18338;
        Interval param_var_a = _18341;
        float param_var_lo = as_type<float>(1036831948u);
        float param_var_hi = as_type<float>(1036831950u);
        Interval _18335;
        _18335.lo = param_var_lo;
        _18335.hi = param_var_hi;
        Interval _18336 = _18335;
        Interval param_var_b = _18336;
        Interval _18362 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _18362;
    }
    if (d == 100.0)
    {
        float param_var_x_1 = n;
        float _18332 = param_var_x_1;
        float _18333 = param_var_x_1;
        Interval _18330;
        _18330.lo = _18332;
        _18330.hi = _18333;
        Interval _18331 = _18330;
        Interval _18334 = _18331;
        Interval param_var_a_1 = _18334;
        float param_var_lo_1 = as_type<float>(1008981769u);
        float param_var_hi_1 = as_type<float>(1008981771u);
        Interval _18328;
        _18328.lo = param_var_lo_1;
        _18328.hi = param_var_hi_1;
        Interval _18329 = _18328;
        Interval param_var_b_1 = _18329;
        Interval _18383 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _18383;
    }
    if (d == 1000.0)
    {
        float param_var_x_2 = n;
        float _18325 = param_var_x_2;
        float _18326 = param_var_x_2;
        Interval _18323;
        _18323.lo = _18325;
        _18323.hi = _18326;
        Interval _18324 = _18323;
        Interval _18327 = _18324;
        Interval param_var_a_2 = _18327;
        float param_var_lo_2 = as_type<float>(981668462u);
        float param_var_hi_2 = as_type<float>(981668464u);
        Interval _18321;
        _18321.lo = param_var_lo_2;
        _18321.hi = param_var_hi_2;
        Interval _18322 = _18321;
        Interval param_var_b_2 = _18322;
        Interval _18404 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _18404;
    }
    if (d == 10000.0)
    {
        float param_var_x_3 = n;
        float _18318 = param_var_x_3;
        float _18319 = param_var_x_3;
        Interval _18316;
        _18316.lo = _18318;
        _18316.hi = _18319;
        Interval _18317 = _18316;
        Interval _18320 = _18317;
        Interval param_var_a_3 = _18320;
        float param_var_lo_3 = as_type<float>(953267990u);
        float param_var_hi_3 = as_type<float>(953267992u);
        Interval _18314;
        _18314.lo = param_var_lo_3;
        _18314.hi = param_var_hi_3;
        Interval _18315 = _18314;
        Interval param_var_b_3 = _18315;
        Interval _18425 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _18425;
    }
    if (d == 100000.0)
    {
        float param_var_x_4 = n;
        float _18311 = param_var_x_4;
        float _18312 = param_var_x_4;
        Interval _18309;
        _18309.lo = _18311;
        _18309.hi = _18312;
        Interval _18310 = _18309;
        Interval _18313 = _18310;
        Interval param_var_a_4 = _18313;
        float param_var_lo_4 = as_type<float>(925353387u);
        float param_var_hi_4 = as_type<float>(925353389u);
        Interval _18307;
        _18307.lo = param_var_lo_4;
        _18307.hi = param_var_hi_4;
        Interval _18308 = _18307;
        Interval param_var_b_4 = _18308;
        Interval _18446 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _18446;
    }
    if (d == 128.0)
    {
        float param_var_x_5 = n;
        float _18304 = param_var_x_5;
        float _18305 = param_var_x_5;
        Interval _18302;
        _18302.lo = _18304;
        _18302.hi = _18305;
        Interval _18303 = _18302;
        Interval _18306 = _18303;
        Interval param_var_a_5 = _18306;
        float param_var_lo_5 = as_type<float>(1006632960u);
        float param_var_hi_5 = as_type<float>(1006632960u);
        Interval _18300;
        _18300.lo = param_var_lo_5;
        _18300.hi = param_var_hi_5;
        Interval _18301 = _18300;
        Interval param_var_b_5 = _18301;
        Interval _18467 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _18467;
    }
    if (d == 65535.0)
    {
        float param_var_x_6 = n;
        float _18297 = param_var_x_6;
        float _18298 = param_var_x_6;
        Interval _18295;
        _18295.lo = _18297;
        _18295.hi = _18298;
        Interval _18296 = _18295;
        Interval _18299 = _18296;
        Interval param_var_a_6 = _18299;
        float param_var_lo_6 = as_type<float>(931135615u);
        float param_var_hi_6 = as_type<float>(931135617u);
        Interval _18293;
        _18293.lo = param_var_lo_6;
        _18293.hi = param_var_hi_6;
        Interval _18294 = _18293;
        Interval param_var_b_6 = _18294;
        Interval _18488 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _18488;
    }
    float param_var_x_7 = n;
    float _18290 = param_var_x_7;
    float _18291 = param_var_x_7;
    Interval _18288;
    _18288.lo = _18290;
    _18288.hi = _18291;
    Interval _18289 = _18288;
    Interval _18292 = _18289;
    Interval param_var_a_7 = _18292;
    float param_var_x_8 = d;
    float _18285 = param_var_x_8;
    float _18286 = param_var_x_8;
    Interval _18283;
    _18283.lo = _18285;
    _18283.hi = _18286;
    Interval _18284 = _18283;
    Interval _18287 = _18284;
    Interval param_var_b_7 = _18287;
    Interval _18509 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _18509;
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
        Interval _20352;
        _20352.lo = param_var_lo;
        _20352.hi = param_var_hi;
        Interval _20353 = _20352;
        return _20353;
    }
    float n = floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5));
    Interval param_var_a = a;
    float param_var_x = spvFMul(n, 2.0);
    float _20349 = param_var_x;
    float _20350 = param_var_x;
    Interval _20347;
    _20347.lo = _20349;
    _20347.hi = _20350;
    Interval _20348 = _20347;
    Interval _20351 = _20348;
    Interval param_var_a_1 = _20351;
    float _20345 = 3.1415927410125732421875;
    float _20340 = _20345;
    float _20399 = interval_down(_20340, intervalFailed);
    float _20341 = _20399;
    float _20342 = _20345;
    float _20401 = interval_up(_20342, intervalFailed);
    float _20343 = _20401;
    Interval _20338;
    _20338.lo = _20341;
    _20338.hi = _20343;
    Interval _20339 = _20338;
    Interval _20344 = _20339;
    Interval _20346 = _20344;
    Interval param_var_b = _20346;
    Interval _20410 = imul(param_var_a_1, param_var_b, intervalFailed, optical_product_upper);
    Interval param_var_b_1 = _20410;
    Interval _20334 = param_var_a;
    Interval _20335 = param_var_b_1;
    float _20331 = as_type<float>(as_type<uint>(_20335.hi) ^ 2147483648u);
    float _20332 = as_type<float>(as_type<uint>(_20335.lo) ^ 2147483648u);
    Interval _20329;
    _20329.lo = _20331;
    _20329.hi = _20332;
    Interval _20330 = _20329;
    Interval _20333 = _20330;
    Interval _20336 = _20333;
    Interval _20430 = iadd(_20334, _20336, intervalFailed);
    Interval _20337 = _20430;
    Interval x = _20337;
    float _20327 = 3.1415927410125732421875;
    float _20322 = _20327;
    float _20433 = interval_down(_20322, intervalFailed);
    float _20323 = _20433;
    float _20324 = _20327;
    float _20435 = interval_up(_20324, intervalFailed);
    float _20325 = _20435;
    Interval _20320;
    _20320.lo = _20323;
    _20320.hi = _20325;
    Interval _20321 = _20320;
    Interval _20326 = _20321;
    Interval _20328 = _20326;
    Interval param_var_a_2 = _20328;
    float param_var_x_1 = 0.5;
    float _20317 = param_var_x_1;
    float _20318 = param_var_x_1;
    Interval _20315;
    _20315.lo = _20317;
    _20315.hi = _20318;
    Interval _20316 = _20315;
    Interval _20319 = _20316;
    Interval param_var_b_2 = _20319;
    Interval _20453 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _half = _20453;
    if (x.lo > _half.hi)
    {
        float _20313 = 3.1415927410125732421875;
        float _20308 = _20313;
        float _20460 = interval_down(_20308, intervalFailed);
        float _20309 = _20460;
        float _20310 = _20313;
        float _20462 = interval_up(_20310, intervalFailed);
        float _20311 = _20462;
        Interval _20306;
        _20306.lo = _20309;
        _20306.hi = _20311;
        Interval _20307 = _20306;
        Interval _20312 = _20307;
        Interval _20314 = _20312;
        Interval param_var_a_3 = _20314;
        Interval param_var_b_3 = x;
        Interval _20302 = param_var_a_3;
        Interval _20303 = param_var_b_3;
        float _20299 = as_type<float>(as_type<uint>(_20303.hi) ^ 2147483648u);
        float _20300 = as_type<float>(as_type<uint>(_20303.lo) ^ 2147483648u);
        Interval _20297;
        _20297.lo = _20299;
        _20297.hi = _20300;
        Interval _20298 = _20297;
        Interval _20301 = _20298;
        Interval _20304 = _20301;
        Interval _20491 = iadd(_20302, _20304, intervalFailed);
        Interval _20305 = _20491;
        x = _20305;
    }
    else
    {
        if (x.hi < (-_half.hi))
        {
            float _20295 = 3.1415927410125732421875;
            float _20290 = _20295;
            float _20499 = interval_down(_20290, intervalFailed);
            float _20291 = _20499;
            float _20292 = _20295;
            float _20501 = interval_up(_20292, intervalFailed);
            float _20293 = _20501;
            Interval _20288;
            _20288.lo = _20291;
            _20288.hi = _20293;
            Interval _20289 = _20288;
            Interval _20294 = _20289;
            Interval _20296 = _20294;
            Interval param_var_a_4 = _20296;
            float _20285 = as_type<float>(as_type<uint>(param_var_a_4.hi) ^ 2147483648u);
            float _20286 = as_type<float>(as_type<uint>(param_var_a_4.lo) ^ 2147483648u);
            Interval _20283;
            _20283.lo = _20285;
            _20283.hi = _20286;
            Interval _20284 = _20283;
            Interval _20287 = _20284;
            Interval param_var_a_5 = _20287;
            Interval param_var_b_4 = x;
            Interval _20279 = param_var_a_5;
            Interval _20280 = param_var_b_4;
            float _20276 = as_type<float>(as_type<uint>(_20280.hi) ^ 2147483648u);
            float _20277 = as_type<float>(as_type<uint>(_20280.lo) ^ 2147483648u);
            Interval _20274;
            _20274.lo = _20276;
            _20274.hi = _20277;
            Interval _20275 = _20274;
            Interval _20278 = _20275;
            Interval _20281 = _20278;
            Interval _20547 = iadd(_20279, _20281, intervalFailed);
            Interval _20282 = _20547;
            x = _20282;
        }
    }
    float _1780 = -_half.hi;
    bool temp_var_logical_2 = true;
    if ((isunordered(x.lo, _1780) || x.lo >= _1780))
    {
        temp_var_logical_2 = x.hi > _half.hi;
    }
    if (temp_var_logical_2)
    {
        float param_var_lo_1 = -1.0;
        float param_var_hi_1 = 1.0;
        Interval _20272;
        _20272.lo = param_var_lo_1;
        _20272.hi = param_var_hi_1;
        Interval _20273 = _20272;
        return _20273;
    }
    Interval param_var_a_6 = x;
    bool _20265 = false;
    if (param_var_a_6.lo <= 0.0)
    {
        _20265 = param_var_a_6.hi >= 0.0;
    }
    float _20264;
    if (_20265)
    {
        _20264 = 0.0;
    }
    else
    {
        _20264 = precise::min(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    }
    float _20263 = _20264;
    float _20266 = precise::max(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    float _20267 = spvFMul(_20263, _20263);
    float _20596 = interval_down(_20267, intervalFailed);
    float _20268 = precise::max(0.0, _20596);
    float _20269 = spvFMul(_20266, _20266);
    float _20600 = interval_up(_20269, intervalFailed);
    float _20270 = _20600;
    Interval _20261;
    _20261.lo = _20268;
    _20261.hi = _20270;
    Interval _20262 = _20261;
    Interval _20271 = _20262;
    Interval square = _20271;
    float param_var_x_2 = _2228[8];
    float _20256 = param_var_x_2;
    float _20611 = interval_down(_20256, intervalFailed);
    float _20257 = _20611;
    float _20258 = param_var_x_2;
    float _20613 = interval_up(_20258, intervalFailed);
    float _20259 = _20613;
    Interval _20254;
    _20254.lo = _20257;
    _20254.hi = _20259;
    Interval _20255 = _20254;
    Interval _20260 = _20255;
    Interval sum = _20260;
    Interval _20247;
    for (int k = 7; k >= 0; k--)
    {
        Interval param_var_a_7 = sum;
        Interval param_var_b_5 = square;
        Interval _20625 = imul(param_var_a_7, param_var_b_5, intervalFailed, optical_product_upper);
        Interval param_var_a_8 = _20625;
        float param_var_x_3 = _2228[k];
        float _20249 = param_var_x_3;
        float _20630 = interval_down(_20249, intervalFailed);
        float _20250 = _20630;
        float _20251 = param_var_x_3;
        float _20632 = interval_up(_20251, intervalFailed);
        float _20252 = _20632;
        _20247.lo = _20250;
        _20247.hi = _20252;
        Interval _20248 = _20247;
        Interval _20253 = _20248;
        Interval param_var_b_6 = _20253;
        Interval _20640 = iadd(param_var_a_8, param_var_b_6, intervalFailed);
        sum = _20640;
    }
    Interval param_var_a_9 = x;
    Interval param_var_b_7 = sum;
    Interval _20644 = imul(param_var_a_9, param_var_b_7, intervalFailed, optical_product_upper);
    Interval param_var_a_10 = _20644;
    float param_var_lo_2 = -3.9999999840167888010000751819462e-12;
    float param_var_hi_2 = 3.9999999840167888010000751819462e-12;
    Interval _20245;
    _20245.lo = param_var_lo_2;
    _20245.hi = param_var_hi_2;
    Interval _20246 = _20245;
    Interval param_var_b_8 = _20246;
    Interval _20651 = iadd(param_var_a_10, param_var_b_8, intervalFailed);
    Interval param_var_a_11 = _20651;
    float param_var_x_4 = -1.0;
    float _20242 = param_var_x_4;
    float _20243 = param_var_x_4;
    Interval _20240;
    _20240.lo = _20242;
    _20240.hi = _20243;
    Interval _20241 = _20240;
    Interval _20244 = _20241;
    Interval param_var_lo_3 = _20244;
    float param_var_x_5 = 1.0;
    float _20237 = param_var_x_5;
    float _20238 = param_var_x_5;
    Interval _20235;
    _20235.lo = _20237;
    _20235.hi = _20238;
    Interval _20236 = _20235;
    Interval _20239 = _20236;
    Interval param_var_hi_3 = _20239;
    Interval _20230 = param_var_a_11;
    Interval _20231 = param_var_lo_3;
    float _20227 = precise::max(_20230.lo, _20231.lo);
    float _20228 = precise::max(_20230.hi, _20231.hi);
    Interval _20225;
    _20225.lo = _20227;
    _20225.hi = _20228;
    Interval _20226 = _20225;
    Interval _20229 = _20226;
    Interval _20232 = _20229;
    Interval _20233 = param_var_hi_3;
    float _20222 = precise::min(_20232.lo, _20233.lo);
    float _20223 = precise::min(_20232.hi, _20233.hi);
    Interval _20220;
    _20220.lo = _20222;
    _20220.hi = _20223;
    Interval _20221 = _20220;
    Interval _20224 = _20221;
    Interval _20234 = _20224;
    return _20234;
}

static inline __attribute__((always_inline))
bool outside_face(thread const ReflectionSpecularPlane& p, thread const Interval3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = p;
    bool _18858 = false;
    if (param_var_p.a.w == 2.0)
    {
        _18858 = param_var_p.b.w == 2.0;
    }
    bool _18859 = false;
    if (_18858)
    {
        _18859 = param_var_p.c.w == 2.0;
    }
    bool _18860 = _18859;
    if (_18860)
    {
        return false;
    }
    float3 param_var_v = p.b.xyz;
    float _18851 = param_var_v.x;
    float _18848 = _18851;
    float _18849 = _18851;
    Interval _18846;
    _18846.lo = _18848;
    _18846.hi = _18849;
    Interval _18847 = _18846;
    Interval _18850 = _18847;
    Interval _18852 = _18850;
    float _18853 = param_var_v.y;
    float _18843 = _18853;
    float _18844 = _18853;
    Interval _18841;
    _18841.lo = _18843;
    _18841.hi = _18844;
    Interval _18842 = _18841;
    Interval _18845 = _18842;
    Interval _18854 = _18845;
    float _18855 = param_var_v.z;
    float _18838 = _18855;
    float _18839 = _18855;
    Interval _18836;
    _18836.lo = _18838;
    _18836.hi = _18839;
    Interval _18837 = _18836;
    Interval _18840 = _18837;
    Interval _18856 = _18840;
    Interval3 _18834;
    _18834.x = _18852;
    _18834.y = _18854;
    _18834.z = _18856;
    Interval3 _18835 = _18834;
    Interval3 _18857 = _18835;
    Interval3 param_var_a = _18857;
    float3 param_var_v_1 = p.a.xyz;
    float _18827 = param_var_v_1.x;
    float _18824 = _18827;
    float _18825 = _18827;
    Interval _18822;
    _18822.lo = _18824;
    _18822.hi = _18825;
    Interval _18823 = _18822;
    Interval _18826 = _18823;
    Interval _18828 = _18826;
    float _18829 = param_var_v_1.y;
    float _18819 = _18829;
    float _18820 = _18829;
    Interval _18817;
    _18817.lo = _18819;
    _18817.hi = _18820;
    Interval _18818 = _18817;
    Interval _18821 = _18818;
    Interval _18830 = _18821;
    float _18831 = param_var_v_1.z;
    float _18814 = _18831;
    float _18815 = _18831;
    Interval _18812;
    _18812.lo = _18814;
    _18812.hi = _18815;
    Interval _18813 = _18812;
    Interval _18816 = _18813;
    Interval _18832 = _18816;
    Interval3 _18810;
    _18810.x = _18828;
    _18810.y = _18830;
    _18810.z = _18832;
    Interval3 _18811 = _18810;
    Interval3 _18833 = _18811;
    Interval3 param_var_b = _18833;
    Interval3 _18801 = param_var_a;
    Interval _18802 = param_var_b.x;
    float _18798 = as_type<float>(as_type<uint>(_18802.hi) ^ 2147483648u);
    float _18799 = as_type<float>(as_type<uint>(_18802.lo) ^ 2147483648u);
    Interval _18796;
    _18796.lo = _18798;
    _18796.hi = _18799;
    Interval _18797 = _18796;
    Interval _18800 = _18797;
    Interval _18803 = _18800;
    Interval _18804 = param_var_b.y;
    float _18793 = as_type<float>(as_type<uint>(_18804.hi) ^ 2147483648u);
    float _18794 = as_type<float>(as_type<uint>(_18804.lo) ^ 2147483648u);
    Interval _18791;
    _18791.lo = _18793;
    _18791.hi = _18794;
    Interval _18792 = _18791;
    Interval _18795 = _18792;
    Interval _18805 = _18795;
    Interval _18806 = param_var_b.z;
    float _18788 = as_type<float>(as_type<uint>(_18806.hi) ^ 2147483648u);
    float _18789 = as_type<float>(as_type<uint>(_18806.lo) ^ 2147483648u);
    Interval _18786;
    _18786.lo = _18788;
    _18786.hi = _18789;
    Interval _18787 = _18786;
    Interval _18790 = _18787;
    Interval _18807 = _18790;
    Interval3 _18784;
    _18784.x = _18803;
    _18784.y = _18805;
    _18784.z = _18807;
    Interval3 _18785 = _18784;
    Interval3 _18808 = _18785;
    Interval _18774 = _18801.x;
    Interval _18775 = _18808.x;
    Interval _19041 = iadd(_18774, _18775, intervalFailed);
    Interval _18776 = _19041;
    Interval _18777 = _18801.y;
    Interval _18778 = _18808.y;
    Interval _19046 = iadd(_18777, _18778, intervalFailed);
    Interval _18779 = _19046;
    Interval _18780 = _18801.z;
    Interval _18781 = _18808.z;
    Interval _19051 = iadd(_18780, _18781, intervalFailed);
    Interval _18782 = _19051;
    Interval3 _18772;
    _18772.x = _18776;
    _18772.y = _18779;
    _18772.z = _18782;
    Interval3 _18773 = _18772;
    Interval3 _18783 = _18773;
    Interval3 _18809 = _18783;
    Interval3 u = _18809;
    float3 param_var_v_2 = p.c.xyz;
    float _18765 = param_var_v_2.x;
    float _18762 = _18765;
    float _18763 = _18765;
    Interval _18760;
    _18760.lo = _18762;
    _18760.hi = _18763;
    Interval _18761 = _18760;
    Interval _18764 = _18761;
    Interval _18766 = _18764;
    float _18767 = param_var_v_2.y;
    float _18757 = _18767;
    float _18758 = _18767;
    Interval _18755;
    _18755.lo = _18757;
    _18755.hi = _18758;
    Interval _18756 = _18755;
    Interval _18759 = _18756;
    Interval _18768 = _18759;
    float _18769 = param_var_v_2.z;
    float _18752 = _18769;
    float _18753 = _18769;
    Interval _18750;
    _18750.lo = _18752;
    _18750.hi = _18753;
    Interval _18751 = _18750;
    Interval _18754 = _18751;
    Interval _18770 = _18754;
    Interval3 _18748;
    _18748.x = _18766;
    _18748.y = _18768;
    _18748.z = _18770;
    Interval3 _18749 = _18748;
    Interval3 _18771 = _18749;
    Interval3 param_var_a_1 = _18771;
    float3 param_var_v_3 = p.a.xyz;
    float _18741 = param_var_v_3.x;
    float _18738 = _18741;
    float _18739 = _18741;
    Interval _18736;
    _18736.lo = _18738;
    _18736.hi = _18739;
    Interval _18737 = _18736;
    Interval _18740 = _18737;
    Interval _18742 = _18740;
    float _18743 = param_var_v_3.y;
    float _18733 = _18743;
    float _18734 = _18743;
    Interval _18731;
    _18731.lo = _18733;
    _18731.hi = _18734;
    Interval _18732 = _18731;
    Interval _18735 = _18732;
    Interval _18744 = _18735;
    float _18745 = param_var_v_3.z;
    float _18728 = _18745;
    float _18729 = _18745;
    Interval _18726;
    _18726.lo = _18728;
    _18726.hi = _18729;
    Interval _18727 = _18726;
    Interval _18730 = _18727;
    Interval _18746 = _18730;
    Interval3 _18724;
    _18724.x = _18742;
    _18724.y = _18744;
    _18724.z = _18746;
    Interval3 _18725 = _18724;
    Interval3 _18747 = _18725;
    Interval3 param_var_b_1 = _18747;
    Interval3 _18715 = param_var_a_1;
    Interval _18716 = param_var_b_1.x;
    float _18712 = as_type<float>(as_type<uint>(_18716.hi) ^ 2147483648u);
    float _18713 = as_type<float>(as_type<uint>(_18716.lo) ^ 2147483648u);
    Interval _18710;
    _18710.lo = _18712;
    _18710.hi = _18713;
    Interval _18711 = _18710;
    Interval _18714 = _18711;
    Interval _18717 = _18714;
    Interval _18718 = param_var_b_1.y;
    float _18707 = as_type<float>(as_type<uint>(_18718.hi) ^ 2147483648u);
    float _18708 = as_type<float>(as_type<uint>(_18718.lo) ^ 2147483648u);
    Interval _18705;
    _18705.lo = _18707;
    _18705.hi = _18708;
    Interval _18706 = _18705;
    Interval _18709 = _18706;
    Interval _18719 = _18709;
    Interval _18720 = param_var_b_1.z;
    float _18702 = as_type<float>(as_type<uint>(_18720.hi) ^ 2147483648u);
    float _18703 = as_type<float>(as_type<uint>(_18720.lo) ^ 2147483648u);
    Interval _18700;
    _18700.lo = _18702;
    _18700.hi = _18703;
    Interval _18701 = _18700;
    Interval _18704 = _18701;
    Interval _18721 = _18704;
    Interval3 _18698;
    _18698.x = _18717;
    _18698.y = _18719;
    _18698.z = _18721;
    Interval3 _18699 = _18698;
    Interval3 _18722 = _18699;
    Interval _18688 = _18715.x;
    Interval _18689 = _18722.x;
    Interval _19222 = iadd(_18688, _18689, intervalFailed);
    Interval _18690 = _19222;
    Interval _18691 = _18715.y;
    Interval _18692 = _18722.y;
    Interval _19227 = iadd(_18691, _18692, intervalFailed);
    Interval _18693 = _19227;
    Interval _18694 = _18715.z;
    Interval _18695 = _18722.z;
    Interval _19232 = iadd(_18694, _18695, intervalFailed);
    Interval _18696 = _19232;
    Interval3 _18686;
    _18686.x = _18690;
    _18686.y = _18693;
    _18686.z = _18696;
    Interval3 _18687 = _18686;
    Interval3 _18697 = _18687;
    Interval3 _18723 = _18697;
    Interval3 v = _18723;
    Interval3 param_var_a_2 = hit;
    float3 param_var_v_4 = p.a.xyz;
    float _18679 = param_var_v_4.x;
    float _18676 = _18679;
    float _18677 = _18679;
    Interval _18674;
    _18674.lo = _18676;
    _18674.hi = _18677;
    Interval _18675 = _18674;
    Interval _18678 = _18675;
    Interval _18680 = _18678;
    float _18681 = param_var_v_4.y;
    float _18671 = _18681;
    float _18672 = _18681;
    Interval _18669;
    _18669.lo = _18671;
    _18669.hi = _18672;
    Interval _18670 = _18669;
    Interval _18673 = _18670;
    Interval _18682 = _18673;
    float _18683 = param_var_v_4.z;
    float _18666 = _18683;
    float _18667 = _18683;
    Interval _18664;
    _18664.lo = _18666;
    _18664.hi = _18667;
    Interval _18665 = _18664;
    Interval _18668 = _18665;
    Interval _18684 = _18668;
    Interval3 _18662;
    _18662.x = _18680;
    _18662.y = _18682;
    _18662.z = _18684;
    Interval3 _18663 = _18662;
    Interval3 _18685 = _18663;
    Interval3 param_var_b_2 = _18685;
    Interval3 _18653 = param_var_a_2;
    Interval _18654 = param_var_b_2.x;
    float _18650 = as_type<float>(as_type<uint>(_18654.hi) ^ 2147483648u);
    float _18651 = as_type<float>(as_type<uint>(_18654.lo) ^ 2147483648u);
    Interval _18648;
    _18648.lo = _18650;
    _18648.hi = _18651;
    Interval _18649 = _18648;
    Interval _18652 = _18649;
    Interval _18655 = _18652;
    Interval _18656 = param_var_b_2.y;
    float _18645 = as_type<float>(as_type<uint>(_18656.hi) ^ 2147483648u);
    float _18646 = as_type<float>(as_type<uint>(_18656.lo) ^ 2147483648u);
    Interval _18643;
    _18643.lo = _18645;
    _18643.hi = _18646;
    Interval _18644 = _18643;
    Interval _18647 = _18644;
    Interval _18657 = _18647;
    Interval _18658 = param_var_b_2.z;
    float _18640 = as_type<float>(as_type<uint>(_18658.hi) ^ 2147483648u);
    float _18641 = as_type<float>(as_type<uint>(_18658.lo) ^ 2147483648u);
    Interval _18638;
    _18638.lo = _18640;
    _18638.hi = _18641;
    Interval _18639 = _18638;
    Interval _18642 = _18639;
    Interval _18659 = _18642;
    Interval3 _18636;
    _18636.x = _18655;
    _18636.y = _18657;
    _18636.z = _18659;
    Interval3 _18637 = _18636;
    Interval3 _18660 = _18637;
    Interval _18626 = _18653.x;
    Interval _18627 = _18660.x;
    Interval _19359 = iadd(_18626, _18627, intervalFailed);
    Interval _18628 = _19359;
    Interval _18629 = _18653.y;
    Interval _18630 = _18660.y;
    Interval _19364 = iadd(_18629, _18630, intervalFailed);
    Interval _18631 = _19364;
    Interval _18632 = _18653.z;
    Interval _18633 = _18660.z;
    Interval _19369 = iadd(_18632, _18633, intervalFailed);
    Interval _18634 = _19369;
    Interval3 _18624;
    _18624.x = _18628;
    _18624.y = _18631;
    _18624.z = _18634;
    Interval3 _18625 = _18624;
    Interval3 _18635 = _18625;
    Interval3 _18661 = _18635;
    Interval3 r = _18661;
    Interval3 param_var_a_3 = u;
    Interval3 param_var_b_3 = u;
    Interval _18613 = param_var_a_3.x;
    Interval _18614 = param_var_b_3.x;
    Interval _19386 = imul(_18613, _18614, intervalFailed, optical_product_upper);
    Interval _18615 = _19386;
    Interval _18616 = param_var_a_3.y;
    Interval _18617 = param_var_b_3.y;
    Interval _19391 = imul(_18616, _18617, intervalFailed, optical_product_upper);
    Interval _18618 = _19391;
    Interval _19392 = iadd(_18615, _18618, intervalFailed);
    Interval _18619 = _19392;
    Interval _18620 = param_var_a_3.z;
    Interval _18621 = param_var_b_3.z;
    Interval _19397 = imul(_18620, _18621, intervalFailed, optical_product_upper);
    Interval _18622 = _19397;
    Interval _19398 = iadd(_18619, _18622, intervalFailed);
    Interval _18623 = _19398;
    Interval aa = _18623;
    Interval3 param_var_a_4 = u;
    Interval3 param_var_b_4 = v;
    Interval _18602 = param_var_a_4.x;
    Interval _18603 = param_var_b_4.x;
    Interval _19406 = imul(_18602, _18603, intervalFailed, optical_product_upper);
    Interval _18604 = _19406;
    Interval _18605 = param_var_a_4.y;
    Interval _18606 = param_var_b_4.y;
    Interval _19411 = imul(_18605, _18606, intervalFailed, optical_product_upper);
    Interval _18607 = _19411;
    Interval _19412 = iadd(_18604, _18607, intervalFailed);
    Interval _18608 = _19412;
    Interval _18609 = param_var_a_4.z;
    Interval _18610 = param_var_b_4.z;
    Interval _19417 = imul(_18609, _18610, intervalFailed, optical_product_upper);
    Interval _18611 = _19417;
    Interval _19418 = iadd(_18608, _18611, intervalFailed);
    Interval _18612 = _19418;
    Interval ab = _18612;
    Interval3 param_var_a_5 = v;
    Interval3 param_var_b_5 = v;
    Interval _18591 = param_var_a_5.x;
    Interval _18592 = param_var_b_5.x;
    Interval _19426 = imul(_18591, _18592, intervalFailed, optical_product_upper);
    Interval _18593 = _19426;
    Interval _18594 = param_var_a_5.y;
    Interval _18595 = param_var_b_5.y;
    Interval _19431 = imul(_18594, _18595, intervalFailed, optical_product_upper);
    Interval _18596 = _19431;
    Interval _19432 = iadd(_18593, _18596, intervalFailed);
    Interval _18597 = _19432;
    Interval _18598 = param_var_a_5.z;
    Interval _18599 = param_var_b_5.z;
    Interval _19437 = imul(_18598, _18599, intervalFailed, optical_product_upper);
    Interval _18600 = _19437;
    Interval _19438 = iadd(_18597, _18600, intervalFailed);
    Interval _18601 = _19438;
    Interval bb = _18601;
    Interval3 param_var_a_6 = r;
    Interval3 param_var_b_6 = u;
    Interval _18580 = param_var_a_6.x;
    Interval _18581 = param_var_b_6.x;
    Interval _19446 = imul(_18580, _18581, intervalFailed, optical_product_upper);
    Interval _18582 = _19446;
    Interval _18583 = param_var_a_6.y;
    Interval _18584 = param_var_b_6.y;
    Interval _19451 = imul(_18583, _18584, intervalFailed, optical_product_upper);
    Interval _18585 = _19451;
    Interval _19452 = iadd(_18582, _18585, intervalFailed);
    Interval _18586 = _19452;
    Interval _18587 = param_var_a_6.z;
    Interval _18588 = param_var_b_6.z;
    Interval _19457 = imul(_18587, _18588, intervalFailed, optical_product_upper);
    Interval _18589 = _19457;
    Interval _19458 = iadd(_18586, _18589, intervalFailed);
    Interval _18590 = _19458;
    Interval ra = _18590;
    Interval3 param_var_a_7 = r;
    Interval3 param_var_b_7 = v;
    Interval _18569 = param_var_a_7.x;
    Interval _18570 = param_var_b_7.x;
    Interval _19466 = imul(_18569, _18570, intervalFailed, optical_product_upper);
    Interval _18571 = _19466;
    Interval _18572 = param_var_a_7.y;
    Interval _18573 = param_var_b_7.y;
    Interval _19471 = imul(_18572, _18573, intervalFailed, optical_product_upper);
    Interval _18574 = _19471;
    Interval _19472 = iadd(_18571, _18574, intervalFailed);
    Interval _18575 = _19472;
    Interval _18576 = param_var_a_7.z;
    Interval _18577 = param_var_b_7.z;
    Interval _19477 = imul(_18576, _18577, intervalFailed, optical_product_upper);
    Interval _18578 = _19477;
    Interval _19478 = iadd(_18575, _18578, intervalFailed);
    Interval _18579 = _19478;
    Interval rb = _18579;
    Interval param_var_a_8 = aa;
    Interval param_var_b_8 = bb;
    Interval _19482 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _19482;
    Interval param_var_a_10 = ab;
    bool _18562 = false;
    if (param_var_a_10.lo <= 0.0)
    {
        _18562 = param_var_a_10.hi >= 0.0;
    }
    float _18561;
    if (_18562)
    {
        _18561 = 0.0;
    }
    else
    {
        _18561 = precise::min(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    }
    float _18560 = _18561;
    float _18563 = precise::max(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    float _18564 = spvFMul(_18560, _18560);
    float _19513 = interval_down(_18564, intervalFailed);
    float _18565 = precise::max(0.0, _19513);
    float _18566 = spvFMul(_18563, _18563);
    float _19517 = interval_up(_18566, intervalFailed);
    float _18567 = _19517;
    Interval _18558;
    _18558.lo = _18565;
    _18558.hi = _18567;
    Interval _18559 = _18558;
    Interval _18568 = _18559;
    Interval param_var_b_9 = _18568;
    Interval _18554 = param_var_a_9;
    Interval _18555 = param_var_b_9;
    float _18551 = as_type<float>(as_type<uint>(_18555.hi) ^ 2147483648u);
    float _18552 = as_type<float>(as_type<uint>(_18555.lo) ^ 2147483648u);
    Interval _18549;
    _18549.lo = _18551;
    _18549.hi = _18552;
    Interval _18550 = _18549;
    Interval _18553 = _18550;
    Interval _18556 = _18553;
    Interval _19544 = iadd(_18554, _18556, intervalFailed);
    Interval _18557 = _19544;
    Interval det = _18557;
    if (det.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_11 = bb;
    Interval param_var_b_10 = ra;
    Interval _19551 = imul(param_var_a_11, param_var_b_10, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _19551;
    Interval param_var_a_13 = ab;
    Interval param_var_b_11 = rb;
    Interval _19554 = imul(param_var_a_13, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_b_12 = _19554;
    Interval _18545 = param_var_a_12;
    Interval _18546 = param_var_b_12;
    float _18542 = as_type<float>(as_type<uint>(_18546.hi) ^ 2147483648u);
    float _18543 = as_type<float>(as_type<uint>(_18546.lo) ^ 2147483648u);
    Interval _18540;
    _18540.lo = _18542;
    _18540.hi = _18543;
    Interval _18541 = _18540;
    Interval _18544 = _18541;
    Interval _18547 = _18544;
    Interval _19574 = iadd(_18545, _18547, intervalFailed);
    Interval _18548 = _19574;
    Interval param_var_a_14 = _18548;
    Interval param_var_b_13 = det;
    Interval _19577 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
    Interval x = _19577;
    Interval param_var_a_15 = aa;
    Interval param_var_b_14 = rb;
    Interval _19580 = imul(param_var_a_15, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_16 = _19580;
    Interval param_var_a_17 = ab;
    Interval param_var_b_15 = ra;
    Interval _19583 = imul(param_var_a_17, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _19583;
    Interval _18536 = param_var_a_16;
    Interval _18537 = param_var_b_16;
    float _18533 = as_type<float>(as_type<uint>(_18537.hi) ^ 2147483648u);
    float _18534 = as_type<float>(as_type<uint>(_18537.lo) ^ 2147483648u);
    Interval _18531;
    _18531.lo = _18533;
    _18531.hi = _18534;
    Interval _18532 = _18531;
    Interval _18535 = _18532;
    Interval _18538 = _18535;
    Interval _19603 = iadd(_18536, _18538, intervalFailed);
    Interval _18539 = _19603;
    Interval param_var_a_18 = _18539;
    Interval param_var_b_17 = det;
    Interval _19606 = idiv(param_var_a_18, param_var_b_17, intervalFailed, interval_divide_upper);
    Interval y = _19606;
    float param_var_x = -9.9999997473787516355514526367188e-06;
    float _18526 = param_var_x;
    float _19610 = interval_down(_18526, intervalFailed);
    float _18527 = _19610;
    float _18528 = param_var_x;
    float _19612 = interval_up(_18528, intervalFailed);
    float _18529 = _19612;
    Interval _18524;
    _18524.lo = _18527;
    _18524.hi = _18529;
    Interval _18525 = _18524;
    Interval _18530 = _18525;
    Interval temp_var_Interval = _18530;
    bool temp_var_logical = true;
    if ((isunordered(x.hi, temp_var_Interval.lo) || x.hi >= temp_var_Interval.lo))
    {
        float param_var_x_1 = -9.9999997473787516355514526367188e-06;
        float _18519 = param_var_x_1;
        float _19626 = interval_down(_18519, intervalFailed);
        float _18520 = _19626;
        float _18521 = param_var_x_1;
        float _19628 = interval_up(_18521, intervalFailed);
        float _18522 = _19628;
        Interval _18517;
        _18517.lo = _18520;
        _18517.hi = _18522;
        Interval _18518 = _18517;
        Interval _18523 = _18518;
        Interval temp_var_Interval_1 = _18523;
        temp_var_logical = y.hi < temp_var_Interval_1.lo;
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        Interval param_var_a_19 = x;
        Interval param_var_b_18 = y;
        Interval _19643 = iadd(param_var_a_19, param_var_b_18, intervalFailed);
        Interval temp_var_Interval_2 = _19643;
        float param_var_x_2 = 1.000010013580322265625;
        float _18512 = param_var_x_2;
        float _19647 = interval_down(_18512, intervalFailed);
        float _18513 = _19647;
        float _18514 = param_var_x_2;
        float _19649 = interval_up(_18514, intervalFailed);
        float _18515 = _19649;
        Interval _18510;
        _18510.lo = _18513;
        _18510.hi = _18515;
        Interval _18511 = _18510;
        Interval _18516 = _18511;
        Interval temp_var_Interval_3 = _18516;
        temp_var_logical_1 = temp_var_Interval_2.lo > temp_var_Interval_3.hi;
    }
    return temp_var_logical_1;
}

static inline __attribute__((always_inline))
bool optical_excluded_target(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTerminal, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper)
{
    float param_var_lo = box.x;
    float param_var_hi = box.z;
    Interval _8809;
    _8809.lo = param_var_lo;
    _8809.hi = param_var_hi;
    Interval _8810 = _8809;
    Interval param_var_a = _8810;
    float param_var_x = receiver.projection.z;
    float _8806 = param_var_x;
    float _8807 = param_var_x;
    Interval _8804;
    _8804.lo = _8806;
    _8804.hi = _8807;
    Interval _8805 = _8804;
    Interval _8808 = _8805;
    Interval param_var_b = _8808;
    Interval _8800 = param_var_a;
    Interval _8801 = param_var_b;
    float _8797 = as_type<float>(as_type<uint>(_8801.hi) ^ 2147483648u);
    float _8798 = as_type<float>(as_type<uint>(_8801.lo) ^ 2147483648u);
    Interval _8795;
    _8795.lo = _8797;
    _8795.hi = _8798;
    Interval _8796 = _8795;
    Interval _8799 = _8796;
    Interval _8802 = _8799;
    Interval _8852 = iadd(_8800, _8802, intervalFailed);
    Interval _8803 = _8852;
    Interval param_var_a_1 = _8803;
    float param_var_x_1 = receiver.projection.x;
    float _8792 = param_var_x_1;
    float _8793 = param_var_x_1;
    Interval _8790;
    _8790.lo = _8792;
    _8790.hi = _8793;
    Interval _8791 = _8790;
    Interval _8794 = _8791;
    Interval param_var_b_1 = _8794;
    Interval _8866 = idiv(param_var_a_1, param_var_b_1, intervalFailed, interval_divide_upper);
    Interval param_var_x_2 = _8866;
    float param_var_lo_1 = box.y;
    float param_var_hi_1 = box.w;
    Interval _8788;
    _8788.lo = param_var_lo_1;
    _8788.hi = param_var_hi_1;
    Interval _8789 = _8788;
    Interval param_var_a_2 = _8789;
    float param_var_x_3 = receiver.projection.w;
    float _8785 = param_var_x_3;
    float _8786 = param_var_x_3;
    Interval _8783;
    _8783.lo = _8785;
    _8783.hi = _8786;
    Interval _8784 = _8783;
    Interval _8787 = _8784;
    Interval param_var_b_2 = _8787;
    Interval _8779 = param_var_a_2;
    Interval _8780 = param_var_b_2;
    float _8776 = as_type<float>(as_type<uint>(_8780.hi) ^ 2147483648u);
    float _8777 = as_type<float>(as_type<uint>(_8780.lo) ^ 2147483648u);
    Interval _8774;
    _8774.lo = _8776;
    _8774.hi = _8777;
    Interval _8775 = _8774;
    Interval _8778 = _8775;
    Interval _8781 = _8778;
    Interval _8908 = iadd(_8779, _8781, intervalFailed);
    Interval _8782 = _8908;
    Interval param_var_a_3 = _8782;
    float param_var_x_4 = receiver.projection.y;
    float _8771 = param_var_x_4;
    float _8772 = param_var_x_4;
    Interval _8769;
    _8769.lo = _8771;
    _8769.hi = _8772;
    Interval _8770 = _8769;
    Interval _8773 = _8770;
    Interval param_var_b_3 = _8773;
    Interval _8922 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_y = _8922;
    float param_var_x_5 = 1.0;
    float _8766 = param_var_x_5;
    float _8767 = param_var_x_5;
    Interval _8764;
    _8764.lo = _8766;
    _8764.hi = _8767;
    Interval _8765 = _8764;
    Interval _8768 = _8765;
    Interval param_var_z = _8768;
    Interval3 _8762;
    _8762.x = param_var_x_2;
    _8762.y = param_var_y;
    _8762.z = param_var_z;
    Interval3 _8763 = _8762;
    Interval3 param_var_a_4 = _8763;
    Interval3 _8755 = param_var_a_4;
    float _8756 = 1.0;
    float _8752 = _8756;
    float _8753 = _8756;
    Interval _8750;
    _8750.lo = _8752;
    _8750.hi = _8753;
    Interval _8751 = _8750;
    Interval _8754 = _8751;
    Interval _8757 = _8754;
    Interval3 _8758 = param_var_a_4;
    Interval _8741 = _8758.x;
    bool _8734 = false;
    if (_8741.lo <= 0.0)
    {
        _8734 = _8741.hi >= 0.0;
    }
    float _8733;
    if (_8734)
    {
        _8733 = 0.0;
    }
    else
    {
        _8733 = precise::min(abs(_8741.lo), abs(_8741.hi));
    }
    float _8732 = _8733;
    float _8735 = precise::max(abs(_8741.lo), abs(_8741.hi));
    float _8736 = spvFMul(_8732, _8732);
    float _8982 = interval_down(_8736, intervalFailed);
    float _8737 = precise::max(0.0, _8982);
    float _8738 = spvFMul(_8735, _8735);
    float _8986 = interval_up(_8738, intervalFailed);
    float _8739 = _8986;
    Interval _8730;
    _8730.lo = _8737;
    _8730.hi = _8739;
    Interval _8731 = _8730;
    Interval _8740 = _8731;
    Interval _8742 = _8740;
    Interval _8743 = _8758.y;
    bool _8723 = false;
    if (_8743.lo <= 0.0)
    {
        _8723 = _8743.hi >= 0.0;
    }
    float _8722;
    if (_8723)
    {
        _8722 = 0.0;
    }
    else
    {
        _8722 = precise::min(abs(_8743.lo), abs(_8743.hi));
    }
    float _8721 = _8722;
    float _8724 = precise::max(abs(_8743.lo), abs(_8743.hi));
    float _8725 = spvFMul(_8721, _8721);
    float _9025 = interval_down(_8725, intervalFailed);
    float _8726 = precise::max(0.0, _9025);
    float _8727 = spvFMul(_8724, _8724);
    float _9029 = interval_up(_8727, intervalFailed);
    float _8728 = _9029;
    Interval _8719;
    _8719.lo = _8726;
    _8719.hi = _8728;
    Interval _8720 = _8719;
    Interval _8729 = _8720;
    Interval _8744 = _8729;
    Interval _9037 = iadd(_8742, _8744, intervalFailed);
    Interval _8745 = _9037;
    Interval _8746 = _8758.z;
    bool _8712 = false;
    if (_8746.lo <= 0.0)
    {
        _8712 = _8746.hi >= 0.0;
    }
    float _8711;
    if (_8712)
    {
        _8711 = 0.0;
    }
    else
    {
        _8711 = precise::min(abs(_8746.lo), abs(_8746.hi));
    }
    float _8710 = _8711;
    float _8713 = precise::max(abs(_8746.lo), abs(_8746.hi));
    float _8714 = spvFMul(_8710, _8710);
    float _9069 = interval_down(_8714, intervalFailed);
    float _8715 = precise::max(0.0, _9069);
    float _8716 = spvFMul(_8713, _8713);
    float _9073 = interval_up(_8716, intervalFailed);
    float _8717 = _9073;
    Interval _8708;
    _8708.lo = _8715;
    _8708.hi = _8717;
    Interval _8709 = _8708;
    Interval _8718 = _8709;
    Interval _8747 = _8718;
    Interval _9081 = iadd(_8745, _8747, intervalFailed);
    Interval _8748 = _9081;
    Interval _9082 = isqrt(_8748, intervalFailed);
    Interval _8749 = _9082;
    Interval _8759 = _8749;
    Interval _9084 = idiv(_8757, _8759, intervalFailed, interval_divide_upper);
    Interval _8760 = _9084;
    Interval _8698 = _8755.x;
    Interval _8699 = _8760;
    Interval _9088 = imul(_8698, _8699, intervalFailed, optical_product_upper);
    Interval _8700 = _9088;
    Interval _8701 = _8755.y;
    Interval _8702 = _8760;
    Interval _9092 = imul(_8701, _8702, intervalFailed, optical_product_upper);
    Interval _8703 = _9092;
    Interval _8704 = _8755.z;
    Interval _8705 = _8760;
    Interval _9096 = imul(_8704, _8705, intervalFailed, optical_product_upper);
    Interval _8706 = _9096;
    Interval3 _8696;
    _8696.x = _8700;
    _8696.y = _8703;
    _8696.z = _8706;
    Interval3 _8697 = _8696;
    Interval3 _8707 = _8697;
    Interval3 _8761 = _8707;
    Interval3 outgoing = _8761;
    float3 param_var_v = float3(0.0);
    float _8689 = param_var_v.x;
    float _8686 = _8689;
    float _8687 = _8689;
    Interval _8684;
    _8684.lo = _8686;
    _8684.hi = _8687;
    Interval _8685 = _8684;
    Interval _8688 = _8685;
    Interval _8690 = _8688;
    float _8691 = param_var_v.y;
    float _8681 = _8691;
    float _8682 = _8691;
    Interval _8679;
    _8679.lo = _8681;
    _8679.hi = _8682;
    Interval _8680 = _8679;
    Interval _8683 = _8680;
    Interval _8692 = _8683;
    float _8693 = param_var_v.z;
    float _8676 = _8693;
    float _8677 = _8693;
    Interval _8674;
    _8674.lo = _8676;
    _8674.hi = _8677;
    Interval _8675 = _8674;
    Interval _8678 = _8675;
    Interval _8694 = _8678;
    Interval3 _8672;
    _8672.x = _8690;
    _8672.y = _8692;
    _8672.z = _8694;
    Interval3 _8673 = _8672;
    Interval3 _8695 = _8673;
    Interval3 origin = _8695;
    float param_var_x_6 = 0.0;
    float _8669 = param_var_x_6;
    float _8670 = param_var_x_6;
    Interval _8667;
    _8667.lo = _8669;
    _8667.hi = _8670;
    Interval _8668 = _8667;
    Interval _8671 = _8668;
    Interval bias0 = _8671;
    bool temp_var_ternary;
    ReflectionSpecularPlane plane;
    Interval3 n;
    float3 temp_var_ternary_1;
    Interval radius;
    int temp_var_ternary_2;
    Interval3 t;
    Interval3 _5291;
    Interval _5293;
    Interval _5298;
    Interval _5303;
    Interval3 _5329;
    Interval _5341;
    float _5344;
    Interval _5352;
    float _5355;
    Interval _5363;
    float _5366;
    Interval _5383;
    Interval3 _5395;
    Interval3 _5407;
    Interval _5419;
    float _5422;
    Interval _5430;
    Interval3 _5435;
    Interval3 _5447;
    Interval3 _5459;
    Interval _5461;
    Interval _5470;
    Interval _5479;
    Interval3 _5510;
    Interval3 _5522;
    Interval _5534;
    float _5537;
    Interval _5545;
    float _5548;
    Interval _5556;
    float _5559;
    Interval _5576;
    Interval3 _5588;
    Interval _5590;
    Interval _5599;
    Interval _5608;
    Interval3 _5639;
    Interval _5641;
    Interval _5646;
    Interval _5651;
    Interval3 _5663;
    Interval _5675;
    float _5678;
    Interval _5686;
    float _5689;
    Interval _5697;
    float _5700;
    Interval _5717;
    Interval3 _5729;
    Interval _5731;
    Interval _5740;
    Interval _5749;
    Interval3 _5780;
    Interval _5782;
    Interval _5787;
    Interval _5792;
    Interval _5804;
    float _5806;
    Interval3 _5811;
    Interval3 _5823;
    Interval _5825;
    Interval _5830;
    Interval _5835;
    Interval3 _5849;
    Interval _5872;
    Interval3 _5887;
    Interval3 _5899;
    Interval _5911;
    Interval3 _5919;
    Interval _5931;
    float _5934;
    Interval _5942;
    float _5945;
    Interval _5953;
    float _5956;
    Interval _5973;
    Interval3 _5985;
    Interval3 _5998;
    Interval _6000;
    Interval _6005;
    Interval _6010;
    Interval3 _6033;
    Interval _6035;
    Interval _6040;
    Interval _6045;
    Interval3 _6068;
    Interval _6070;
    Interval _6075;
    Interval _6080;
    Interval3 _6105;
    Interval _6107;
    Interval _6112;
    Interval _6121;
    float _6124;
    Interval _6132;
    Interval _6137;
    Interval _6142;
    float _6145;
    Interval _6165;
    Interval _6167;
    Interval _6180;
    Interval _6185;
    Interval _6201;
    float _6204;
    Interval _6212;
    Interval _6217;
    Interval _6222;
    float _6225;
    Interval _6245;
    Interval _6247;
    Interval _6260;
    Interval _6265;
    Interval _6281;
    float _6284;
    Interval _6292;
    Interval _6297;
    Interval _6302;
    float _6305;
    Interval _6325;
    Interval _6327;
    Interval _6340;
    Interval _6345;
    Interval _6361;
    float _6364;
    Interval _6372;
    Interval _6377;
    Interval _6382;
    float _6385;
    Interval _6405;
    Interval _6407;
    Interval _6420;
    Interval _6425;
    Interval _6441;
    Interval _6450;
    Interval _6459;
    Interval _6468;
    Interval3 _6477;
    Interval3 _6489;
    Interval3 _6501;
    Interval3 _6513;
    Interval _6525;
    Interval _6530;
    Interval _6540;
    Interval _6545;
    Interval _6550;
    Interval3 _6555;
    Interval _6557;
    Interval _6562;
    Interval _6567;
    Interval _6572;
    Interval _6577;
    Interval _6582;
    Interval _6587;
    Interval _6592;
    Interval _6597;
    float _6600;
    Interval _6608;
    float _6611;
    Interval _6619;
    Interval _6624;
    Interval _6629;
    float _6632;
    Interval _6652;
    Interval _6657;
    Interval _6662;
    Interval _6672;
    Interval _6677;
    Interval _6682;
    Interval _6691;
    float _6694;
    Interval _6702;
    float _6705;
    Interval _6713;
    Interval _6718;
    float _6721;
    Interval _6729;
    float _6732;
    Interval _6740;
    Interval _6742;
    Interval _6755;
    Interval _6762;
    Interval _6764;
    Interval _6773;
    Interval _6778;
    Interval _6783;
    Interval _6796;
    Interval _6805;
    Interval _6810;
    Interval _6823;
    Interval _6832;
    Interval _6841;
    Interval _6843;
    Interval _6845;
    Interval _6847;
    Interval _6852;
    Interval _6857;
    Interval _6859;
    Interval _6872;
    Interval _6877;
    Interval _6893;
    Interval _6895;
    Interval _6908;
    Interval _6913;
    Interval _6929;
    Interval _6931;
    Interval _6944;
    float _6947;
    Interval _6955;
    Interval _6960;
    Interval _6965;
    float _6968;
    Interval _6988;
    Interval _6997;
    Interval _6999;
    Interval _7012;
    Interval _7017;
    Interval _7033;
    Interval _7042;
    Interval _7044;
    Interval _7057;
    Interval _7062;
    Interval _7078;
    Interval _7080;
    Interval _7093;
    Interval _7098;
    Interval _7114;
    Interval _7123;
    Interval _7128;
    Interval _7137;
    Interval _7139;
    Interval _7152;
    Interval _7157;
    Interval _7173;
    Interval _7175;
    Interval _7188;
    Interval _7193;
    Interval _7209;
    Interval _7211;
    Interval _7224;
    Interval _7229;
    Interval _7245;
    Interval _7250;
    Interval _7252;
    Interval _7265;
    Interval _7270;
    Interval _7286;
    Interval _7288;
    Interval _7301;
    Interval _7306;
    Interval _7308;
    Interval _7321;
    Interval _7326;
    Interval _7328;
    Interval _7341;
    Interval _7346;
    float _7349;
    Interval _7357;
    Interval _7362;
    Interval _7367;
    float _7370;
    Interval _7390;
    float _7393;
    Interval _7401;
    Interval _7406;
    Interval _7411;
    float _7414;
    Interval _7434;
    float _7437;
    Interval _7445;
    Interval _7450;
    Interval _7455;
    float _7458;
    Interval _7478;
    Interval _7487;
    Interval _7496;
    Interval _7505;
    Interval _7507;
    Interval _7520;
    Interval _7529;
    float _7532;
    Interval _7540;
    Interval _7545;
    Interval _7550;
    float _7553;
    Interval _7573;
    Interval3 _7984;
    Interval3 _7996;
    Interval3 _8008;
    Interval _8010;
    Interval _8015;
    Interval _8020;
    Interval3 _8032;
    Interval3 _8044;
    Interval3 _8056;
    Interval _8058;
    Interval _8063;
    Interval _8068;
    Interval3 _8080;
    Interval3 _8092;
    Interval _8094;
    Interval _8099;
    Interval _8104;
    Interval _8130;
    Interval _8135;
    Interval3 _8140;
    Interval3 _8152;
    Interval _8154;
    Interval _8159;
    Interval _8164;
    Interval3 _8176;
    Interval3 _8188;
    Interval3 _8200;
    Interval _8202;
    Interval _8207;
    Interval _8212;
    Interval3 _8224;
    Interval3 _8236;
    Interval3 _8248;
    Interval _8250;
    Interval _8255;
    Interval _8260;
    Interval3 _8272;
    Interval3 _8284;
    Interval _8286;
    Interval _8291;
    Interval _8296;
    Interval _8330;
    Interval _8331;
    Interval _8476;
    Interval _8481;
    float _8483;
    Interval _8488;
    Interval _8493;
    float _8496;
    Interval _8504;
    float _8507;
    Interval _8515;
    float _8518;
    Interval3 _8535;
    Interval3 _8547;
    Interval3 _8570;
    Interval3 _8582;
    Interval _8584;
    Interval _8589;
    Interval _8594;
    Interval3 _8608;
    Interval _8610;
    Interval _8615;
    Interval _8620;
    Interval3 _8643;
    Interval _8645;
    Interval _8650;
    Interval _8655;
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
            float _8660 = param_var_v_1.x;
            float _8657 = _8660;
            float _8658 = _8660;
            _8655.lo = _8657;
            _8655.hi = _8658;
            Interval _8656 = _8655;
            Interval _8659 = _8656;
            Interval _8661 = _8659;
            float _8662 = param_var_v_1.y;
            float _8652 = _8662;
            float _8653 = _8662;
            _8650.lo = _8652;
            _8650.hi = _8653;
            Interval _8651 = _8650;
            Interval _8654 = _8651;
            Interval _8663 = _8654;
            float _8664 = param_var_v_1.z;
            float _8647 = _8664;
            float _8648 = _8664;
            _8645.lo = _8647;
            _8645.hi = _8648;
            Interval _8646 = _8645;
            Interval _8649 = _8646;
            Interval _8665 = _8649;
            _8643.x = _8661;
            _8643.y = _8663;
            _8643.z = _8665;
            Interval3 _8644 = _8643;
            Interval3 _8666 = _8644;
            n = _8666;
        }
        else
        {
            ReflectionSpecularPlane param_var_p = plane;
            Interval3 _9236 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval3 param_var_n = _9236;
            Interval3 param_var_direction = outgoing;
            Interval3 _9238 = oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper);
            n = _9238;
        }
        Interval3 param_var_a_5 = outgoing;
        Interval3 param_var_b_4 = n;
        Interval _8632 = param_var_a_5.x;
        Interval _8633 = param_var_b_4.x;
        Interval _9245 = imul(_8632, _8633, intervalFailed, optical_product_upper);
        Interval _8634 = _9245;
        Interval _8635 = param_var_a_5.y;
        Interval _8636 = param_var_b_4.y;
        Interval _9250 = imul(_8635, _8636, intervalFailed, optical_product_upper);
        Interval _8637 = _9250;
        Interval _9251 = iadd(_8634, _8637, intervalFailed);
        Interval _8638 = _9251;
        Interval _8639 = param_var_a_5.z;
        Interval _8640 = param_var_b_4.z;
        Interval _9256 = imul(_8639, _8640, intervalFailed, optical_product_upper);
        Interval _8641 = _9256;
        Interval _9257 = iadd(_8638, _8641, intervalFailed);
        Interval _8642 = _9257;
        Interval denominator = _8642;
        if (curved)
        {
            temp_var_ternary_1 = liquid.planePoint.xyz;
        }
        else
        {
            temp_var_ternary_1 = plane.a.xyz;
        }
        float3 param_var_v_2 = temp_var_ternary_1;
        float _8625 = param_var_v_2.x;
        float _8622 = _8625;
        float _8623 = _8625;
        _8620.lo = _8622;
        _8620.hi = _8623;
        Interval _8621 = _8620;
        Interval _8624 = _8621;
        Interval _8626 = _8624;
        float _8627 = param_var_v_2.y;
        float _8617 = _8627;
        float _8618 = _8627;
        _8615.lo = _8617;
        _8615.hi = _8618;
        Interval _8616 = _8615;
        Interval _8619 = _8616;
        Interval _8628 = _8619;
        float _8629 = param_var_v_2.z;
        float _8612 = _8629;
        float _8613 = _8629;
        _8610.lo = _8612;
        _8610.hi = _8613;
        Interval _8611 = _8610;
        Interval _8614 = _8611;
        Interval _8630 = _8614;
        _8608.x = _8626;
        _8608.y = _8628;
        _8608.z = _8630;
        Interval3 _8609 = _8608;
        Interval3 _8631 = _8609;
        Interval3 param_var_a_6 = _8631;
        Interval3 param_var_b_5 = origin;
        Interval3 _8599 = param_var_a_6;
        Interval _8600 = param_var_b_5.x;
        float _8596 = as_type<float>(as_type<uint>(_8600.hi) ^ 2147483648u);
        float _8597 = as_type<float>(as_type<uint>(_8600.lo) ^ 2147483648u);
        _8594.lo = _8596;
        _8594.hi = _8597;
        Interval _8595 = _8594;
        Interval _8598 = _8595;
        Interval _8601 = _8598;
        Interval _8602 = param_var_b_5.y;
        float _8591 = as_type<float>(as_type<uint>(_8602.hi) ^ 2147483648u);
        float _8592 = as_type<float>(as_type<uint>(_8602.lo) ^ 2147483648u);
        _8589.lo = _8591;
        _8589.hi = _8592;
        Interval _8590 = _8589;
        Interval _8593 = _8590;
        Interval _8603 = _8593;
        Interval _8604 = param_var_b_5.z;
        float _8586 = as_type<float>(as_type<uint>(_8604.hi) ^ 2147483648u);
        float _8587 = as_type<float>(as_type<uint>(_8604.lo) ^ 2147483648u);
        _8584.lo = _8586;
        _8584.hi = _8587;
        Interval _8585 = _8584;
        Interval _8588 = _8585;
        Interval _8605 = _8588;
        _8582.x = _8601;
        _8582.y = _8603;
        _8582.z = _8605;
        Interval3 _8583 = _8582;
        Interval3 _8606 = _8583;
        Interval _8572 = _8599.x;
        Interval _8573 = _8606.x;
        Interval _9380 = iadd(_8572, _8573, intervalFailed);
        Interval _8574 = _9380;
        Interval _8575 = _8599.y;
        Interval _8576 = _8606.y;
        Interval _9385 = iadd(_8575, _8576, intervalFailed);
        Interval _8577 = _9385;
        Interval _8578 = _8599.z;
        Interval _8579 = _8606.z;
        Interval _9390 = iadd(_8578, _8579, intervalFailed);
        Interval _8580 = _9390;
        _8570.x = _8574;
        _8570.y = _8577;
        _8570.z = _8580;
        Interval3 _8571 = _8570;
        Interval3 _8581 = _8571;
        Interval3 _8607 = _8581;
        Interval3 param_var_a_7 = _8607;
        Interval3 param_var_b_6 = n;
        Interval _8559 = param_var_a_7.x;
        Interval _8560 = param_var_b_6.x;
        Interval _9406 = imul(_8559, _8560, intervalFailed, optical_product_upper);
        Interval _8561 = _9406;
        Interval _8562 = param_var_a_7.y;
        Interval _8563 = param_var_b_6.y;
        Interval _9411 = imul(_8562, _8563, intervalFailed, optical_product_upper);
        Interval _8564 = _9411;
        Interval _9412 = iadd(_8561, _8564, intervalFailed);
        Interval _8565 = _9412;
        Interval _8566 = param_var_a_7.z;
        Interval _8567 = param_var_b_6.z;
        Interval _9417 = imul(_8566, _8567, intervalFailed, optical_product_upper);
        Interval _8568 = _9417;
        Interval _9418 = iadd(_8565, _8568, intervalFailed);
        Interval _8569 = _9418;
        Interval param_var_a_8 = _8569;
        Interval param_var_b_7 = denominator;
        Interval _9421 = idiv(param_var_a_8, param_var_b_7, intervalFailed, interval_divide_upper);
        Interval _distance = _9421;
        if (primary)
        {
            Interval param_var_a_9 = _distance;
            Interval param_var_b_8 = outgoing.z;
            Interval _9426 = imul(param_var_a_9, param_var_b_8, intervalFailed, optical_product_upper);
            Interval depth = _9426;
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
        Interval _8549 = param_var_a_11.x;
        Interval _8550 = param_var_b_9;
        Interval _9464 = imul(_8549, _8550, intervalFailed, optical_product_upper);
        Interval _8551 = _9464;
        Interval _8552 = param_var_a_11.y;
        Interval _8553 = param_var_b_9;
        Interval _9468 = imul(_8552, _8553, intervalFailed, optical_product_upper);
        Interval _8554 = _9468;
        Interval _8555 = param_var_a_11.z;
        Interval _8556 = param_var_b_9;
        Interval _9472 = imul(_8555, _8556, intervalFailed, optical_product_upper);
        Interval _8557 = _9472;
        _8547.x = _8551;
        _8547.y = _8554;
        _8547.z = _8557;
        Interval3 _8548 = _8547;
        Interval3 _8558 = _8548;
        Interval3 param_var_b_10 = _8558;
        Interval _8537 = param_var_a_10.x;
        Interval _8538 = param_var_b_10.x;
        Interval _9486 = iadd(_8537, _8538, intervalFailed);
        Interval _8539 = _9486;
        Interval _8540 = param_var_a_10.y;
        Interval _8541 = param_var_b_10.y;
        Interval _9491 = iadd(_8540, _8541, intervalFailed);
        Interval _8542 = _9491;
        Interval _8543 = param_var_a_10.z;
        Interval _8544 = param_var_b_10.z;
        Interval _9496 = iadd(_8543, _8544, intervalFailed);
        Interval _8545 = _9496;
        _8535.x = _8539;
        _8535.y = _8542;
        _8535.z = _8545;
        Interval3 _8536 = _8535;
        Interval3 _8546 = _8536;
        Interval3 hit = _8546;
        if (curved)
        {
            if (primary)
            {
                radius = _distance;
            }
            else
            {
                Interval3 param_var_a_12 = hit;
                Interval _8526 = param_var_a_12.x;
                bool _8519 = false;
                if (_8526.lo <= 0.0)
                {
                    _8519 = _8526.hi >= 0.0;
                }
                if (_8519)
                {
                    _8518 = 0.0;
                }
                else
                {
                    _8518 = precise::min(abs(_8526.lo), abs(_8526.hi));
                }
                float _8517 = _8518;
                float _8520 = precise::max(abs(_8526.lo), abs(_8526.hi));
                float _8521 = spvFMul(_8517, _8517);
                float _9541 = interval_down(_8521, intervalFailed);
                float _8522 = precise::max(0.0, _9541);
                float _8523 = spvFMul(_8520, _8520);
                float _9545 = interval_up(_8523, intervalFailed);
                float _8524 = _9545;
                _8515.lo = _8522;
                _8515.hi = _8524;
                Interval _8516 = _8515;
                Interval _8525 = _8516;
                Interval _8527 = _8525;
                Interval _8528 = param_var_a_12.y;
                bool _8508 = false;
                if (_8528.lo <= 0.0)
                {
                    _8508 = _8528.hi >= 0.0;
                }
                if (_8508)
                {
                    _8507 = 0.0;
                }
                else
                {
                    _8507 = precise::min(abs(_8528.lo), abs(_8528.hi));
                }
                float _8506 = _8507;
                float _8509 = precise::max(abs(_8528.lo), abs(_8528.hi));
                float _8510 = spvFMul(_8506, _8506);
                float _9584 = interval_down(_8510, intervalFailed);
                float _8511 = precise::max(0.0, _9584);
                float _8512 = spvFMul(_8509, _8509);
                float _9588 = interval_up(_8512, intervalFailed);
                float _8513 = _9588;
                _8504.lo = _8511;
                _8504.hi = _8513;
                Interval _8505 = _8504;
                Interval _8514 = _8505;
                Interval _8529 = _8514;
                Interval _9596 = iadd(_8527, _8529, intervalFailed);
                Interval _8530 = _9596;
                Interval _8531 = param_var_a_12.z;
                bool _8497 = false;
                if (_8531.lo <= 0.0)
                {
                    _8497 = _8531.hi >= 0.0;
                }
                if (_8497)
                {
                    _8496 = 0.0;
                }
                else
                {
                    _8496 = precise::min(abs(_8531.lo), abs(_8531.hi));
                }
                float _8495 = _8496;
                float _8498 = precise::max(abs(_8531.lo), abs(_8531.hi));
                float _8499 = spvFMul(_8495, _8495);
                float _9628 = interval_down(_8499, intervalFailed);
                float _8500 = precise::max(0.0, _9628);
                float _8501 = spvFMul(_8498, _8498);
                float _9632 = interval_up(_8501, intervalFailed);
                float _8502 = _9632;
                _8493.lo = _8500;
                _8493.hi = _8502;
                Interval _8494 = _8493;
                Interval _8503 = _8494;
                Interval _8532 = _8503;
                Interval _9640 = iadd(_8530, _8532, intervalFailed);
                Interval _8533 = _9640;
                Interval _9641 = isqrt(_8533, intervalFailed);
                Interval _8534 = _9641;
                radius = _8534;
            }
            Interval param_var_a_13 = radius;
            float param_var_x_7 = precise::max(liquid.projection.x, 1.0);
            float _8490 = param_var_x_7;
            float _8491 = param_var_x_7;
            _8488.lo = _8490;
            _8488.hi = _8491;
            Interval _8489 = _8488;
            Interval _8492 = _8489;
            Interval param_var_b_11 = _8492;
            Interval _9657 = idiv(param_var_a_13, param_var_b_11, intervalFailed, interval_divide_upper);
            Interval param_var_a_14 = _9657;
            Interval param_var_a_15 = denominator;
            bool _8484 = false;
            if (param_var_a_15.lo <= 0.0)
            {
                _8484 = param_var_a_15.hi >= 0.0;
            }
            if (_8484)
            {
                _8483 = 0.0;
            }
            else
            {
                _8483 = precise::min(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            }
            float _8485 = _8483;
            float _8486 = precise::max(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            _8481.lo = _8485;
            _8481.hi = _8486;
            Interval _8482 = _8481;
            Interval _8487 = _8482;
            Interval param_var_a_16 = _8487;
            float param_var_n_1 = 4.0;
            float param_var_d = 100.0;
            Interval _9693 = iratio(param_var_n_1, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_12 = _9693;
            float _8478 = precise::max(param_var_a_16.lo, param_var_b_12.lo);
            float _8479 = precise::max(param_var_a_16.hi, param_var_b_12.hi);
            _8476.lo = _8478;
            _8476.hi = _8479;
            Interval _8477 = _8476;
            Interval _8480 = _8477;
            Interval param_var_b_13 = _8480;
            Interval _9711 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
            Interval footprint = _9711;
            ReflectionLiquidFrame param_var_f = liquid;
            Interval3 param_var_direction_1 = outgoing;
            Interval param_var_distance = _distance;
            Interval param_var_footprint = footprint;
            Interval3 _8323 = hit;
            ReflectionLiquidFrame _8324 = param_var_f;
            float3 _8308 = _8324.rotation0.xyz;
            float _8301 = _8308.x;
            float _8298 = _8301;
            float _8299 = _8301;
            _8296.lo = _8298;
            _8296.hi = _8299;
            Interval _8297 = _8296;
            Interval _8300 = _8297;
            Interval _8302 = _8300;
            float _8303 = _8308.y;
            float _8293 = _8303;
            float _8294 = _8303;
            _8291.lo = _8293;
            _8291.hi = _8294;
            Interval _8292 = _8291;
            Interval _8295 = _8292;
            Interval _8304 = _8295;
            float _8305 = _8308.z;
            float _8288 = _8305;
            float _8289 = _8305;
            _8286.lo = _8288;
            _8286.hi = _8289;
            Interval _8287 = _8286;
            Interval _8290 = _8287;
            Interval _8306 = _8290;
            _8284.x = _8302;
            _8284.y = _8304;
            _8284.z = _8306;
            Interval3 _8285 = _8284;
            Interval3 _8307 = _8285;
            Interval3 _8309 = _8307;
            Interval _8310 = _8323.x;
            Interval _8274 = _8309.x;
            Interval _8275 = _8310;
            Interval _9768 = imul(_8274, _8275, intervalFailed, optical_product_upper);
            Interval _8276 = _9768;
            Interval _8277 = _8309.y;
            Interval _8278 = _8310;
            Interval _9772 = imul(_8277, _8278, intervalFailed, optical_product_upper);
            Interval _8279 = _9772;
            Interval _8280 = _8309.z;
            Interval _8281 = _8310;
            Interval _9776 = imul(_8280, _8281, intervalFailed, optical_product_upper);
            Interval _8282 = _9776;
            _8272.x = _8276;
            _8272.y = _8279;
            _8272.z = _8282;
            Interval3 _8273 = _8272;
            Interval3 _8283 = _8273;
            Interval3 _8311 = _8283;
            float3 _8312 = _8324.rotation1.xyz;
            float _8265 = _8312.x;
            float _8262 = _8265;
            float _8263 = _8265;
            _8260.lo = _8262;
            _8260.hi = _8263;
            Interval _8261 = _8260;
            Interval _8264 = _8261;
            Interval _8266 = _8264;
            float _8267 = _8312.y;
            float _8257 = _8267;
            float _8258 = _8267;
            _8255.lo = _8257;
            _8255.hi = _8258;
            Interval _8256 = _8255;
            Interval _8259 = _8256;
            Interval _8268 = _8259;
            float _8269 = _8312.z;
            float _8252 = _8269;
            float _8253 = _8269;
            _8250.lo = _8252;
            _8250.hi = _8253;
            Interval _8251 = _8250;
            Interval _8254 = _8251;
            Interval _8270 = _8254;
            _8248.x = _8266;
            _8248.y = _8268;
            _8248.z = _8270;
            Interval3 _8249 = _8248;
            Interval3 _8271 = _8249;
            Interval3 _8313 = _8271;
            Interval _8314 = _8323.y;
            Interval _8238 = _8313.x;
            Interval _8239 = _8314;
            Interval _9836 = imul(_8238, _8239, intervalFailed, optical_product_upper);
            Interval _8240 = _9836;
            Interval _8241 = _8313.y;
            Interval _8242 = _8314;
            Interval _9840 = imul(_8241, _8242, intervalFailed, optical_product_upper);
            Interval _8243 = _9840;
            Interval _8244 = _8313.z;
            Interval _8245 = _8314;
            Interval _9844 = imul(_8244, _8245, intervalFailed, optical_product_upper);
            Interval _8246 = _9844;
            _8236.x = _8240;
            _8236.y = _8243;
            _8236.z = _8246;
            Interval3 _8237 = _8236;
            Interval3 _8247 = _8237;
            Interval3 _8315 = _8247;
            Interval _8226 = _8311.x;
            Interval _8227 = _8315.x;
            Interval _9858 = iadd(_8226, _8227, intervalFailed);
            Interval _8228 = _9858;
            Interval _8229 = _8311.y;
            Interval _8230 = _8315.y;
            Interval _9863 = iadd(_8229, _8230, intervalFailed);
            Interval _8231 = _9863;
            Interval _8232 = _8311.z;
            Interval _8233 = _8315.z;
            Interval _9868 = iadd(_8232, _8233, intervalFailed);
            Interval _8234 = _9868;
            _8224.x = _8228;
            _8224.y = _8231;
            _8224.z = _8234;
            Interval3 _8225 = _8224;
            Interval3 _8235 = _8225;
            Interval3 _8316 = _8235;
            float3 _8317 = _8324.rotation2.xyz;
            float _8217 = _8317.x;
            float _8214 = _8217;
            float _8215 = _8217;
            _8212.lo = _8214;
            _8212.hi = _8215;
            Interval _8213 = _8212;
            Interval _8216 = _8213;
            Interval _8218 = _8216;
            float _8219 = _8317.y;
            float _8209 = _8219;
            float _8210 = _8219;
            _8207.lo = _8209;
            _8207.hi = _8210;
            Interval _8208 = _8207;
            Interval _8211 = _8208;
            Interval _8220 = _8211;
            float _8221 = _8317.z;
            float _8204 = _8221;
            float _8205 = _8221;
            _8202.lo = _8204;
            _8202.hi = _8205;
            Interval _8203 = _8202;
            Interval _8206 = _8203;
            Interval _8222 = _8206;
            _8200.x = _8218;
            _8200.y = _8220;
            _8200.z = _8222;
            Interval3 _8201 = _8200;
            Interval3 _8223 = _8201;
            Interval3 _8318 = _8223;
            Interval _8319 = _8323.z;
            Interval _8190 = _8318.x;
            Interval _8191 = _8319;
            Interval _9928 = imul(_8190, _8191, intervalFailed, optical_product_upper);
            Interval _8192 = _9928;
            Interval _8193 = _8318.y;
            Interval _8194 = _8319;
            Interval _9932 = imul(_8193, _8194, intervalFailed, optical_product_upper);
            Interval _8195 = _9932;
            Interval _8196 = _8318.z;
            Interval _8197 = _8319;
            Interval _9936 = imul(_8196, _8197, intervalFailed, optical_product_upper);
            Interval _8198 = _9936;
            _8188.x = _8192;
            _8188.y = _8195;
            _8188.z = _8198;
            Interval3 _8189 = _8188;
            Interval3 _8199 = _8189;
            Interval3 _8320 = _8199;
            Interval _8178 = _8316.x;
            Interval _8179 = _8320.x;
            Interval _9950 = iadd(_8178, _8179, intervalFailed);
            Interval _8180 = _9950;
            Interval _8181 = _8316.y;
            Interval _8182 = _8320.y;
            Interval _9955 = iadd(_8181, _8182, intervalFailed);
            Interval _8183 = _9955;
            Interval _8184 = _8316.z;
            Interval _8185 = _8320.z;
            Interval _9960 = iadd(_8184, _8185, intervalFailed);
            Interval _8186 = _9960;
            _8176.x = _8180;
            _8176.y = _8183;
            _8176.z = _8186;
            Interval3 _8177 = _8176;
            Interval3 _8187 = _8177;
            Interval3 _8321 = _8187;
            Interval3 _8325 = _8321;
            float3 _8326 = float3(param_var_f.rotation0.w, param_var_f.rotation1.w, param_var_f.rotation2.w);
            float _8169 = _8326.x;
            float _8166 = _8169;
            float _8167 = _8169;
            _8164.lo = _8166;
            _8164.hi = _8167;
            Interval _8165 = _8164;
            Interval _8168 = _8165;
            Interval _8170 = _8168;
            float _8171 = _8326.y;
            float _8161 = _8171;
            float _8162 = _8171;
            _8159.lo = _8161;
            _8159.hi = _8162;
            Interval _8160 = _8159;
            Interval _8163 = _8160;
            Interval _8172 = _8163;
            float _8173 = _8326.z;
            float _8156 = _8173;
            float _8157 = _8173;
            _8154.lo = _8156;
            _8154.hi = _8157;
            Interval _8155 = _8154;
            Interval _8158 = _8155;
            Interval _8174 = _8158;
            _8152.x = _8170;
            _8152.y = _8172;
            _8152.z = _8174;
            Interval3 _8153 = _8152;
            Interval3 _8175 = _8153;
            Interval3 _8327 = _8175;
            Interval _8142 = _8325.x;
            Interval _8143 = _8327.x;
            Interval _10027 = iadd(_8142, _8143, intervalFailed);
            Interval _8144 = _10027;
            Interval _8145 = _8325.y;
            Interval _8146 = _8327.y;
            Interval _10032 = iadd(_8145, _8146, intervalFailed);
            Interval _8147 = _10032;
            Interval _8148 = _8325.z;
            Interval _8149 = _8327.z;
            Interval _10037 = iadd(_8148, _8149, intervalFailed);
            Interval _8150 = _10037;
            _8140.x = _8144;
            _8140.y = _8147;
            _8140.z = _8150;
            Interval3 _8141 = _8140;
            Interval3 _8151 = _8141;
            Interval3 _8322 = _8151;
            float _8329 = param_var_f.settings.x;
            float _8137 = _8329;
            float _8138 = _8329;
            _8135.lo = _8137;
            _8135.hi = _8138;
            Interval _8136 = _8135;
            Interval _8139 = _8136;
            Interval _8328 = _8139;
            if (param_var_f.settings.y == 3.0)
            {
                float _8332 = 0.0;
                float _8132 = _8332;
                float _8133 = _8332;
                _8130.lo = _8132;
                _8130.hi = _8133;
                Interval _8131 = _8130;
                Interval _8134 = _8131;
                _8331 = _8134;
                _8330 = _8134;
                Interval3 _8334 = param_var_direction_1;
                ReflectionLiquidFrame _8335 = param_var_f;
                float3 _8116 = _8335.rotation0.xyz;
                float _8109 = _8116.x;
                float _8106 = _8109;
                float _8107 = _8109;
                _8104.lo = _8106;
                _8104.hi = _8107;
                Interval _8105 = _8104;
                Interval _8108 = _8105;
                Interval _8110 = _8108;
                float _8111 = _8116.y;
                float _8101 = _8111;
                float _8102 = _8111;
                _8099.lo = _8101;
                _8099.hi = _8102;
                Interval _8100 = _8099;
                Interval _8103 = _8100;
                Interval _8112 = _8103;
                float _8113 = _8116.z;
                float _8096 = _8113;
                float _8097 = _8113;
                _8094.lo = _8096;
                _8094.hi = _8097;
                Interval _8095 = _8094;
                Interval _8098 = _8095;
                Interval _8114 = _8098;
                _8092.x = _8110;
                _8092.y = _8112;
                _8092.z = _8114;
                Interval3 _8093 = _8092;
                Interval3 _8115 = _8093;
                Interval3 _8117 = _8115;
                Interval _8118 = _8334.x;
                Interval _8082 = _8117.x;
                Interval _8083 = _8118;
                Interval _10127 = imul(_8082, _8083, intervalFailed, optical_product_upper);
                Interval _8084 = _10127;
                Interval _8085 = _8117.y;
                Interval _8086 = _8118;
                Interval _10131 = imul(_8085, _8086, intervalFailed, optical_product_upper);
                Interval _8087 = _10131;
                Interval _8088 = _8117.z;
                Interval _8089 = _8118;
                Interval _10135 = imul(_8088, _8089, intervalFailed, optical_product_upper);
                Interval _8090 = _10135;
                _8080.x = _8084;
                _8080.y = _8087;
                _8080.z = _8090;
                Interval3 _8081 = _8080;
                Interval3 _8091 = _8081;
                Interval3 _8119 = _8091;
                float3 _8120 = _8335.rotation1.xyz;
                float _8073 = _8120.x;
                float _8070 = _8073;
                float _8071 = _8073;
                _8068.lo = _8070;
                _8068.hi = _8071;
                Interval _8069 = _8068;
                Interval _8072 = _8069;
                Interval _8074 = _8072;
                float _8075 = _8120.y;
                float _8065 = _8075;
                float _8066 = _8075;
                _8063.lo = _8065;
                _8063.hi = _8066;
                Interval _8064 = _8063;
                Interval _8067 = _8064;
                Interval _8076 = _8067;
                float _8077 = _8120.z;
                float _8060 = _8077;
                float _8061 = _8077;
                _8058.lo = _8060;
                _8058.hi = _8061;
                Interval _8059 = _8058;
                Interval _8062 = _8059;
                Interval _8078 = _8062;
                _8056.x = _8074;
                _8056.y = _8076;
                _8056.z = _8078;
                Interval3 _8057 = _8056;
                Interval3 _8079 = _8057;
                Interval3 _8121 = _8079;
                Interval _8122 = _8334.y;
                Interval _8046 = _8121.x;
                Interval _8047 = _8122;
                Interval _10195 = imul(_8046, _8047, intervalFailed, optical_product_upper);
                Interval _8048 = _10195;
                Interval _8049 = _8121.y;
                Interval _8050 = _8122;
                Interval _10199 = imul(_8049, _8050, intervalFailed, optical_product_upper);
                Interval _8051 = _10199;
                Interval _8052 = _8121.z;
                Interval _8053 = _8122;
                Interval _10203 = imul(_8052, _8053, intervalFailed, optical_product_upper);
                Interval _8054 = _10203;
                _8044.x = _8048;
                _8044.y = _8051;
                _8044.z = _8054;
                Interval3 _8045 = _8044;
                Interval3 _8055 = _8045;
                Interval3 _8123 = _8055;
                Interval _8034 = _8119.x;
                Interval _8035 = _8123.x;
                Interval _10217 = iadd(_8034, _8035, intervalFailed);
                Interval _8036 = _10217;
                Interval _8037 = _8119.y;
                Interval _8038 = _8123.y;
                Interval _10222 = iadd(_8037, _8038, intervalFailed);
                Interval _8039 = _10222;
                Interval _8040 = _8119.z;
                Interval _8041 = _8123.z;
                Interval _10227 = iadd(_8040, _8041, intervalFailed);
                Interval _8042 = _10227;
                _8032.x = _8036;
                _8032.y = _8039;
                _8032.z = _8042;
                Interval3 _8033 = _8032;
                Interval3 _8043 = _8033;
                Interval3 _8124 = _8043;
                float3 _8125 = _8335.rotation2.xyz;
                float _8025 = _8125.x;
                float _8022 = _8025;
                float _8023 = _8025;
                _8020.lo = _8022;
                _8020.hi = _8023;
                Interval _8021 = _8020;
                Interval _8024 = _8021;
                Interval _8026 = _8024;
                float _8027 = _8125.y;
                float _8017 = _8027;
                float _8018 = _8027;
                _8015.lo = _8017;
                _8015.hi = _8018;
                Interval _8016 = _8015;
                Interval _8019 = _8016;
                Interval _8028 = _8019;
                float _8029 = _8125.z;
                float _8012 = _8029;
                float _8013 = _8029;
                _8010.lo = _8012;
                _8010.hi = _8013;
                Interval _8011 = _8010;
                Interval _8014 = _8011;
                Interval _8030 = _8014;
                _8008.x = _8026;
                _8008.y = _8028;
                _8008.z = _8030;
                Interval3 _8009 = _8008;
                Interval3 _8031 = _8009;
                Interval3 _8126 = _8031;
                Interval _8127 = _8334.z;
                Interval _7998 = _8126.x;
                Interval _7999 = _8127;
                Interval _10287 = imul(_7998, _7999, intervalFailed, optical_product_upper);
                Interval _8000 = _10287;
                Interval _8001 = _8126.y;
                Interval _8002 = _8127;
                Interval _10291 = imul(_8001, _8002, intervalFailed, optical_product_upper);
                Interval _8003 = _10291;
                Interval _8004 = _8126.z;
                Interval _8005 = _8127;
                Interval _10295 = imul(_8004, _8005, intervalFailed, optical_product_upper);
                Interval _8006 = _10295;
                _7996.x = _8000;
                _7996.y = _8003;
                _7996.z = _8006;
                Interval3 _7997 = _7996;
                Interval3 _8007 = _7997;
                Interval3 _8128 = _8007;
                Interval _7986 = _8124.x;
                Interval _7987 = _8128.x;
                Interval _10309 = iadd(_7986, _7987, intervalFailed);
                Interval _7988 = _10309;
                Interval _7989 = _8124.y;
                Interval _7990 = _8128.y;
                Interval _10314 = iadd(_7989, _7990, intervalFailed);
                Interval _7991 = _10314;
                Interval _7992 = _8124.z;
                Interval _7993 = _8128.z;
                Interval _10319 = iadd(_7992, _7993, intervalFailed);
                Interval _7994 = _10319;
                _7984.x = _7988;
                _7984.y = _7991;
                _7984.z = _7994;
                Interval3 _7985 = _7984;
                Interval3 _7995 = _7985;
                Interval3 _8129 = _7995;
                Interval3 _8333 = _8129;
                for (uint _8336 = 0u; _8336 < 2u; _8336++)
                {
                    Interval _8338 = _8322.x;
                    Interval _8339 = _8322.z;
                    Interval _8340 = _8328;
                    Interval _8341 = param_var_footprint;
                    Interval _7583 = _8338;
                    float _7584 = 6.0;
                    float _7585 = 1000.0;
                    Interval _10343 = iratio(_7584, _7585, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7586 = _10343;
                    Interval _10344 = imul(_7583, _7586, intervalFailed, optical_product_upper);
                    Interval _7587 = _10344;
                    Interval _7588 = _8339;
                    float _7589 = 8.0;
                    float _7590 = 1000.0;
                    Interval _10346 = iratio(_7589, _7590, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7591 = _10346;
                    Interval _10347 = imul(_7588, _7591, intervalFailed, optical_product_upper);
                    Interval _7592 = _10347;
                    Interval _7578 = _7587;
                    Interval _7579 = _7592;
                    float _7575 = as_type<float>(as_type<uint>(_7579.hi) ^ 2147483648u);
                    float _7576 = as_type<float>(as_type<uint>(_7579.lo) ^ 2147483648u);
                    _7573.lo = _7575;
                    _7573.hi = _7576;
                    Interval _7574 = _7573;
                    Interval _7577 = _7574;
                    Interval _7580 = _7577;
                    Interval _10367 = iadd(_7578, _7580, intervalFailed);
                    Interval _7581 = _10367;
                    Interval _7593 = _7581;
                    Interval _7594 = _8340;
                    float _7595 = 11.0;
                    float _7596 = 100.0;
                    Interval _10370 = iratio(_7595, _7596, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7597 = _10370;
                    Interval _10371 = imul(_7594, _7597, intervalFailed, optical_product_upper);
                    Interval _7598 = _10371;
                    Interval _10372 = iadd(_7593, _7598, intervalFailed);
                    Interval _7582 = _10372;
                    Interval _7600 = _8341;
                    float _7601 = 10.0;
                    float _7602 = 1000.0;
                    Interval _10374 = iratio(_7601, _7602, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7603 = _10374;
                    Interval _7562 = _7600;
                    Interval _7563 = _7603;
                    Interval _10377 = imul(_7562, _7563, intervalFailed, optical_product_upper);
                    Interval _7564 = _10377;
                    bool _7554 = false;
                    if (_7564.lo <= 0.0)
                    {
                        _7554 = _7564.hi >= 0.0;
                    }
                    if (_7554)
                    {
                        _7553 = 0.0;
                    }
                    else
                    {
                        _7553 = precise::min(abs(_7564.lo), abs(_7564.hi));
                    }
                    float _7552 = _7553;
                    float _7555 = precise::max(abs(_7564.lo), abs(_7564.hi));
                    float _7556 = spvFMul(_7552, _7552);
                    float _10407 = interval_down(_7556, intervalFailed);
                    float _7557 = precise::max(0.0, _10407);
                    float _7558 = spvFMul(_7555, _7555);
                    float _10411 = interval_up(_7558, intervalFailed);
                    float _7559 = _10411;
                    _7550.lo = _7557;
                    _7550.hi = _7559;
                    Interval _7551 = _7550;
                    Interval _7560 = _7551;
                    Interval _7561 = _7560;
                    float _7565 = 1.0;
                    float _7547 = _7565;
                    float _7548 = _7565;
                    _7545.lo = _7547;
                    _7545.hi = _7548;
                    Interval _7546 = _7545;
                    Interval _7549 = _7546;
                    Interval _7566 = _7549;
                    float _7567 = 1.0;
                    float _7542 = _7567;
                    float _7543 = _7567;
                    _7540.lo = _7542;
                    _7540.hi = _7543;
                    Interval _7541 = _7540;
                    Interval _7544 = _7541;
                    Interval _7568 = _7544;
                    Interval _7569 = _7561;
                    bool _7533 = false;
                    if (_7569.lo <= 0.0)
                    {
                        _7533 = _7569.hi >= 0.0;
                    }
                    if (_7533)
                    {
                        _7532 = 0.0;
                    }
                    else
                    {
                        _7532 = precise::min(abs(_7569.lo), abs(_7569.hi));
                    }
                    float _7531 = _7532;
                    float _7534 = precise::max(abs(_7569.lo), abs(_7569.hi));
                    float _7535 = spvFMul(_7531, _7531);
                    float _10467 = interval_down(_7535, intervalFailed);
                    float _7536 = precise::max(0.0, _10467);
                    float _7537 = spvFMul(_7534, _7534);
                    float _10471 = interval_up(_7537, intervalFailed);
                    float _7538 = _10471;
                    _7529.lo = _7536;
                    _7529.hi = _7538;
                    Interval _7530 = _7529;
                    Interval _7539 = _7530;
                    Interval _7570 = _7539;
                    Interval _10479 = iadd(_7568, _7570, intervalFailed);
                    Interval _7571 = _10479;
                    Interval _10480 = idiv(_7566, _7571, intervalFailed, interval_divide_upper);
                    Interval _7572 = _10480;
                    Interval _7599 = _7572;
                    Interval _7605 = _8338;
                    float _7606 = 18.0;
                    float _7607 = 1000.0;
                    Interval _10483 = iratio(_7606, _7607, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7608 = _10483;
                    Interval _10484 = imul(_7605, _7608, intervalFailed, optical_product_upper);
                    Interval _7609 = _10484;
                    Interval _7610 = _8339;
                    float _7611 = 11.0;
                    float _7612 = 1000.0;
                    Interval _10486 = iratio(_7611, _7612, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7613 = _10486;
                    Interval _10487 = imul(_7610, _7613, intervalFailed, optical_product_upper);
                    Interval _7614 = _10487;
                    Interval _10488 = iadd(_7609, _7614, intervalFailed);
                    Interval _7615 = _10488;
                    Interval _7616 = _8340;
                    float _7617 = 45.0;
                    float _7618 = 100.0;
                    Interval _10490 = iratio(_7617, _7618, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7619 = _10490;
                    Interval _10491 = imul(_7616, _7619, intervalFailed, optical_product_upper);
                    Interval _7620 = _10491;
                    Interval _7525 = _7615;
                    Interval _7526 = _7620;
                    float _7522 = as_type<float>(as_type<uint>(_7526.hi) ^ 2147483648u);
                    float _7523 = as_type<float>(as_type<uint>(_7526.lo) ^ 2147483648u);
                    _7520.lo = _7522;
                    _7520.hi = _7523;
                    Interval _7521 = _7520;
                    Interval _7524 = _7521;
                    Interval _7527 = _7524;
                    Interval _10511 = iadd(_7525, _7527, intervalFailed);
                    Interval _7528 = _10511;
                    Interval _7621 = _7528;
                    float _7622 = 65.0;
                    float _7623 = 100.0;
                    Interval _10513 = iratio(_7622, _7623, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7624 = _10513;
                    Interval _7625 = _7582;
                    float _7515 = _7625.lo;
                    float _7516 = _7625.hi;
                    float _7510 = _7515;
                    float _7511 = _7516;
                    _7507.lo = _7510;
                    _7507.hi = _7511;
                    Interval _7508 = _7507;
                    Interval _7512 = _7508;
                    Interval _10527 = isin_body(_7512, intervalFailed, optical_product_upper);
                    Interval _7509 = _10527;
                    interval_sine_upper = _7509.hi;
                    float _7513 = _7509.lo;
                    float _7514 = _7513;
                    float _7517 = _7514;
                    float _7518 = interval_sine_upper;
                    _7505.lo = _7517;
                    _7505.hi = _7518;
                    Interval _7506 = _7505;
                    Interval _7519 = _7506;
                    Interval _7626 = _7519;
                    Interval _10542 = imul(_7624, _7626, intervalFailed, optical_product_upper);
                    Interval _7627 = _10542;
                    Interval _7628 = _7599;
                    Interval _10544 = imul(_7627, _7628, intervalFailed, optical_product_upper);
                    Interval _7629 = _10544;
                    Interval _10545 = iadd(_7621, _7629, intervalFailed);
                    Interval _7604 = _10545;
                    Interval _7631 = _8338;
                    float _7632 = 47.0;
                    float _7633 = 1000.0;
                    Interval _10547 = iratio(_7632, _7633, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7634 = _10547;
                    Interval _10548 = imul(_7631, _7634, intervalFailed, optical_product_upper);
                    Interval _7635 = _10548;
                    Interval _7636 = _8339;
                    float _7637 = 25.0;
                    float _7638 = 1000.0;
                    Interval _10550 = iratio(_7637, _7638, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7639 = _10550;
                    Interval _10551 = imul(_7636, _7639, intervalFailed, optical_product_upper);
                    Interval _7640 = _10551;
                    Interval _7501 = _7635;
                    Interval _7502 = _7640;
                    float _7498 = as_type<float>(as_type<uint>(_7502.hi) ^ 2147483648u);
                    float _7499 = as_type<float>(as_type<uint>(_7502.lo) ^ 2147483648u);
                    _7496.lo = _7498;
                    _7496.hi = _7499;
                    Interval _7497 = _7496;
                    Interval _7500 = _7497;
                    Interval _7503 = _7500;
                    Interval _10571 = iadd(_7501, _7503, intervalFailed);
                    Interval _7504 = _10571;
                    Interval _7641 = _7504;
                    Interval _7642 = _8340;
                    float _7643 = 60.0;
                    float _7644 = 100.0;
                    Interval _10574 = iratio(_7643, _7644, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7645 = _10574;
                    Interval _10575 = imul(_7642, _7645, intervalFailed, optical_product_upper);
                    Interval _7646 = _10575;
                    Interval _10576 = iadd(_7641, _7646, intervalFailed);
                    Interval _7630 = _10576;
                    Interval _7648 = _8339;
                    float _7649 = 22.0;
                    float _7650 = 1000.0;
                    Interval _10578 = iratio(_7649, _7650, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7651 = _10578;
                    Interval _10579 = imul(_7648, _7651, intervalFailed, optical_product_upper);
                    Interval _7652 = _10579;
                    Interval _7653 = _8338;
                    float _7654 = 9.0;
                    float _7655 = 1000.0;
                    Interval _10581 = iratio(_7654, _7655, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7656 = _10581;
                    Interval _10582 = imul(_7653, _7656, intervalFailed, optical_product_upper);
                    Interval _7657 = _10582;
                    Interval _7492 = _7652;
                    Interval _7493 = _7657;
                    float _7489 = as_type<float>(as_type<uint>(_7493.hi) ^ 2147483648u);
                    float _7490 = as_type<float>(as_type<uint>(_7493.lo) ^ 2147483648u);
                    _7487.lo = _7489;
                    _7487.hi = _7490;
                    Interval _7488 = _7487;
                    Interval _7491 = _7488;
                    Interval _7494 = _7491;
                    Interval _10602 = iadd(_7492, _7494, intervalFailed);
                    Interval _7495 = _10602;
                    Interval _7658 = _7495;
                    Interval _7659 = _8340;
                    float _7660 = 32.0;
                    float _7661 = 100.0;
                    Interval _10605 = iratio(_7660, _7661, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7662 = _10605;
                    Interval _10606 = imul(_7659, _7662, intervalFailed, optical_product_upper);
                    Interval _7663 = _10606;
                    Interval _7483 = _7658;
                    Interval _7484 = _7663;
                    float _7480 = as_type<float>(as_type<uint>(_7484.hi) ^ 2147483648u);
                    float _7481 = as_type<float>(as_type<uint>(_7484.lo) ^ 2147483648u);
                    _7478.lo = _7480;
                    _7478.hi = _7481;
                    Interval _7479 = _7478;
                    Interval _7482 = _7479;
                    Interval _7485 = _7482;
                    Interval _10626 = iadd(_7483, _7485, intervalFailed);
                    Interval _7486 = _10626;
                    Interval _7647 = _7486;
                    Interval _7665 = _8341;
                    float _7666 = 22.0;
                    float _7667 = 1000.0;
                    Interval _10629 = iratio(_7666, _7667, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7668 = _10629;
                    Interval _7467 = _7665;
                    Interval _7468 = _7668;
                    Interval _10632 = imul(_7467, _7468, intervalFailed, optical_product_upper);
                    Interval _7469 = _10632;
                    bool _7459 = false;
                    if (_7469.lo <= 0.0)
                    {
                        _7459 = _7469.hi >= 0.0;
                    }
                    if (_7459)
                    {
                        _7458 = 0.0;
                    }
                    else
                    {
                        _7458 = precise::min(abs(_7469.lo), abs(_7469.hi));
                    }
                    float _7457 = _7458;
                    float _7460 = precise::max(abs(_7469.lo), abs(_7469.hi));
                    float _7461 = spvFMul(_7457, _7457);
                    float _10662 = interval_down(_7461, intervalFailed);
                    float _7462 = precise::max(0.0, _10662);
                    float _7463 = spvFMul(_7460, _7460);
                    float _10666 = interval_up(_7463, intervalFailed);
                    float _7464 = _10666;
                    _7455.lo = _7462;
                    _7455.hi = _7464;
                    Interval _7456 = _7455;
                    Interval _7465 = _7456;
                    Interval _7466 = _7465;
                    float _7470 = 1.0;
                    float _7452 = _7470;
                    float _7453 = _7470;
                    _7450.lo = _7452;
                    _7450.hi = _7453;
                    Interval _7451 = _7450;
                    Interval _7454 = _7451;
                    Interval _7471 = _7454;
                    float _7472 = 1.0;
                    float _7447 = _7472;
                    float _7448 = _7472;
                    _7445.lo = _7447;
                    _7445.hi = _7448;
                    Interval _7446 = _7445;
                    Interval _7449 = _7446;
                    Interval _7473 = _7449;
                    Interval _7474 = _7466;
                    bool _7438 = false;
                    if (_7474.lo <= 0.0)
                    {
                        _7438 = _7474.hi >= 0.0;
                    }
                    if (_7438)
                    {
                        _7437 = 0.0;
                    }
                    else
                    {
                        _7437 = precise::min(abs(_7474.lo), abs(_7474.hi));
                    }
                    float _7436 = _7437;
                    float _7439 = precise::max(abs(_7474.lo), abs(_7474.hi));
                    float _7440 = spvFMul(_7436, _7436);
                    float _10722 = interval_down(_7440, intervalFailed);
                    float _7441 = precise::max(0.0, _10722);
                    float _7442 = spvFMul(_7439, _7439);
                    float _10726 = interval_up(_7442, intervalFailed);
                    float _7443 = _10726;
                    _7434.lo = _7441;
                    _7434.hi = _7443;
                    Interval _7435 = _7434;
                    Interval _7444 = _7435;
                    Interval _7475 = _7444;
                    Interval _10734 = iadd(_7473, _7475, intervalFailed);
                    Interval _7476 = _10734;
                    Interval _10735 = idiv(_7471, _7476, intervalFailed, interval_divide_upper);
                    Interval _7477 = _10735;
                    Interval _7664 = _7477;
                    Interval _7670 = _8341;
                    float _7671 = 54.0;
                    float _7672 = 1000.0;
                    Interval _10738 = iratio(_7671, _7672, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7673 = _10738;
                    Interval _7423 = _7670;
                    Interval _7424 = _7673;
                    Interval _10741 = imul(_7423, _7424, intervalFailed, optical_product_upper);
                    Interval _7425 = _10741;
                    bool _7415 = false;
                    if (_7425.lo <= 0.0)
                    {
                        _7415 = _7425.hi >= 0.0;
                    }
                    if (_7415)
                    {
                        _7414 = 0.0;
                    }
                    else
                    {
                        _7414 = precise::min(abs(_7425.lo), abs(_7425.hi));
                    }
                    float _7413 = _7414;
                    float _7416 = precise::max(abs(_7425.lo), abs(_7425.hi));
                    float _7417 = spvFMul(_7413, _7413);
                    float _10771 = interval_down(_7417, intervalFailed);
                    float _7418 = precise::max(0.0, _10771);
                    float _7419 = spvFMul(_7416, _7416);
                    float _10775 = interval_up(_7419, intervalFailed);
                    float _7420 = _10775;
                    _7411.lo = _7418;
                    _7411.hi = _7420;
                    Interval _7412 = _7411;
                    Interval _7421 = _7412;
                    Interval _7422 = _7421;
                    float _7426 = 1.0;
                    float _7408 = _7426;
                    float _7409 = _7426;
                    _7406.lo = _7408;
                    _7406.hi = _7409;
                    Interval _7407 = _7406;
                    Interval _7410 = _7407;
                    Interval _7427 = _7410;
                    float _7428 = 1.0;
                    float _7403 = _7428;
                    float _7404 = _7428;
                    _7401.lo = _7403;
                    _7401.hi = _7404;
                    Interval _7402 = _7401;
                    Interval _7405 = _7402;
                    Interval _7429 = _7405;
                    Interval _7430 = _7422;
                    bool _7394 = false;
                    if (_7430.lo <= 0.0)
                    {
                        _7394 = _7430.hi >= 0.0;
                    }
                    if (_7394)
                    {
                        _7393 = 0.0;
                    }
                    else
                    {
                        _7393 = precise::min(abs(_7430.lo), abs(_7430.hi));
                    }
                    float _7392 = _7393;
                    float _7395 = precise::max(abs(_7430.lo), abs(_7430.hi));
                    float _7396 = spvFMul(_7392, _7392);
                    float _10831 = interval_down(_7396, intervalFailed);
                    float _7397 = precise::max(0.0, _10831);
                    float _7398 = spvFMul(_7395, _7395);
                    float _10835 = interval_up(_7398, intervalFailed);
                    float _7399 = _10835;
                    _7390.lo = _7397;
                    _7390.hi = _7399;
                    Interval _7391 = _7390;
                    Interval _7400 = _7391;
                    Interval _7431 = _7400;
                    Interval _10843 = iadd(_7429, _7431, intervalFailed);
                    Interval _7432 = _10843;
                    Interval _10844 = idiv(_7427, _7432, intervalFailed, interval_divide_upper);
                    Interval _7433 = _10844;
                    Interval _7669 = _7433;
                    Interval _7675 = _8341;
                    float _7676 = 24.0;
                    float _7677 = 1000.0;
                    Interval _10847 = iratio(_7676, _7677, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7678 = _10847;
                    Interval _7379 = _7675;
                    Interval _7380 = _7678;
                    Interval _10850 = imul(_7379, _7380, intervalFailed, optical_product_upper);
                    Interval _7381 = _10850;
                    bool _7371 = false;
                    if (_7381.lo <= 0.0)
                    {
                        _7371 = _7381.hi >= 0.0;
                    }
                    if (_7371)
                    {
                        _7370 = 0.0;
                    }
                    else
                    {
                        _7370 = precise::min(abs(_7381.lo), abs(_7381.hi));
                    }
                    float _7369 = _7370;
                    float _7372 = precise::max(abs(_7381.lo), abs(_7381.hi));
                    float _7373 = spvFMul(_7369, _7369);
                    float _10880 = interval_down(_7373, intervalFailed);
                    float _7374 = precise::max(0.0, _10880);
                    float _7375 = spvFMul(_7372, _7372);
                    float _10884 = interval_up(_7375, intervalFailed);
                    float _7376 = _10884;
                    _7367.lo = _7374;
                    _7367.hi = _7376;
                    Interval _7368 = _7367;
                    Interval _7377 = _7368;
                    Interval _7378 = _7377;
                    float _7382 = 1.0;
                    float _7364 = _7382;
                    float _7365 = _7382;
                    _7362.lo = _7364;
                    _7362.hi = _7365;
                    Interval _7363 = _7362;
                    Interval _7366 = _7363;
                    Interval _7383 = _7366;
                    float _7384 = 1.0;
                    float _7359 = _7384;
                    float _7360 = _7384;
                    _7357.lo = _7359;
                    _7357.hi = _7360;
                    Interval _7358 = _7357;
                    Interval _7361 = _7358;
                    Interval _7385 = _7361;
                    Interval _7386 = _7378;
                    bool _7350 = false;
                    if (_7386.lo <= 0.0)
                    {
                        _7350 = _7386.hi >= 0.0;
                    }
                    if (_7350)
                    {
                        _7349 = 0.0;
                    }
                    else
                    {
                        _7349 = precise::min(abs(_7386.lo), abs(_7386.hi));
                    }
                    float _7348 = _7349;
                    float _7351 = precise::max(abs(_7386.lo), abs(_7386.hi));
                    float _7352 = spvFMul(_7348, _7348);
                    float _10940 = interval_down(_7352, intervalFailed);
                    float _7353 = precise::max(0.0, _10940);
                    float _7354 = spvFMul(_7351, _7351);
                    float _10944 = interval_up(_7354, intervalFailed);
                    float _7355 = _10944;
                    _7346.lo = _7353;
                    _7346.hi = _7355;
                    Interval _7347 = _7346;
                    Interval _7356 = _7347;
                    Interval _7387 = _7356;
                    Interval _10952 = iadd(_7385, _7387, intervalFailed);
                    Interval _7388 = _10952;
                    Interval _10953 = idiv(_7383, _7388, intervalFailed, interval_divide_upper);
                    Interval _7389 = _10953;
                    Interval _7674 = _7389;
                    float _7680 = 9.0;
                    float _7343 = _7680;
                    float _7344 = _7680;
                    _7341.lo = _7343;
                    _7341.hi = _7344;
                    Interval _7342 = _7341;
                    Interval _7345 = _7342;
                    Interval _7681 = _7345;
                    Interval _7682 = _7604;
                    float _7336 = _7682.lo;
                    float _7337 = _7682.hi;
                    float _7331 = _7336;
                    float _7332 = _7337;
                    _7328.lo = _7331;
                    _7328.hi = _7332;
                    Interval _7329 = _7328;
                    Interval _7333 = _7329;
                    Interval _10977 = isin_body(_7333, intervalFailed, optical_product_upper);
                    Interval _7330 = _10977;
                    interval_sine_upper = _7330.hi;
                    float _7334 = _7330.lo;
                    float _7335 = _7334;
                    float _7338 = _7335;
                    float _7339 = interval_sine_upper;
                    _7326.lo = _7338;
                    _7326.hi = _7339;
                    Interval _7327 = _7326;
                    Interval _7340 = _7327;
                    Interval _7683 = _7340;
                    Interval _10992 = imul(_7681, _7683, intervalFailed, optical_product_upper);
                    Interval _7684 = _10992;
                    Interval _7685 = _7664;
                    Interval _10994 = imul(_7684, _7685, intervalFailed, optical_product_upper);
                    Interval _7686 = _10994;
                    float _7687 = 2.5;
                    float _7323 = _7687;
                    float _7324 = _7687;
                    _7321.lo = _7323;
                    _7321.hi = _7324;
                    Interval _7322 = _7321;
                    Interval _7325 = _7322;
                    Interval _7688 = _7325;
                    Interval _7689 = _7630;
                    float _7316 = _7689.lo;
                    float _7317 = _7689.hi;
                    float _7311 = _7316;
                    float _7312 = _7317;
                    _7308.lo = _7311;
                    _7308.hi = _7312;
                    Interval _7309 = _7308;
                    Interval _7313 = _7309;
                    Interval _11017 = isin_body(_7313, intervalFailed, optical_product_upper);
                    Interval _7310 = _11017;
                    interval_sine_upper = _7310.hi;
                    float _7314 = _7310.lo;
                    float _7315 = _7314;
                    float _7318 = _7315;
                    float _7319 = interval_sine_upper;
                    _7306.lo = _7318;
                    _7306.hi = _7319;
                    Interval _7307 = _7306;
                    Interval _7320 = _7307;
                    Interval _7690 = _7320;
                    Interval _11032 = imul(_7688, _7690, intervalFailed, optical_product_upper);
                    Interval _7691 = _11032;
                    Interval _7692 = _7669;
                    Interval _11034 = imul(_7691, _7692, intervalFailed, optical_product_upper);
                    Interval _7693 = _11034;
                    Interval _11035 = iadd(_7686, _7693, intervalFailed);
                    Interval _7694 = _11035;
                    float _7695 = 5.0;
                    float _7303 = _7695;
                    float _7304 = _7695;
                    _7301.lo = _7303;
                    _7301.hi = _7304;
                    Interval _7302 = _7301;
                    Interval _7305 = _7302;
                    Interval _7696 = _7305;
                    Interval _7697 = _7647;
                    float _7296 = _7697.lo;
                    float _7297 = _7697.hi;
                    float _7291 = _7296;
                    float _7292 = _7297;
                    _7288.lo = _7291;
                    _7288.hi = _7292;
                    Interval _7289 = _7288;
                    Interval _7293 = _7289;
                    Interval _11058 = isin_body(_7293, intervalFailed, optical_product_upper);
                    Interval _7290 = _11058;
                    interval_sine_upper = _7290.hi;
                    float _7294 = _7290.lo;
                    float _7295 = _7294;
                    float _7298 = _7295;
                    float _7299 = interval_sine_upper;
                    _7286.lo = _7298;
                    _7286.hi = _7299;
                    Interval _7287 = _7286;
                    Interval _7300 = _7287;
                    Interval _7698 = _7300;
                    Interval _11073 = imul(_7696, _7698, intervalFailed, optical_product_upper);
                    Interval _7699 = _11073;
                    Interval _7700 = _7674;
                    Interval _11075 = imul(_7699, _7700, intervalFailed, optical_product_upper);
                    Interval _7701 = _11075;
                    Interval _11076 = iadd(_7694, _7701, intervalFailed);
                    Interval _7679 = _11076;
                    Interval _7703 = _7582;
                    Interval _7279 = _7703;
                    float _7277 = 3.1415927410125732421875;
                    float _7272 = _7277;
                    float _11080 = interval_down(_7272, intervalFailed);
                    float _7273 = _11080;
                    float _7274 = _7277;
                    float _11082 = interval_up(_7274, intervalFailed);
                    float _7275 = _11082;
                    _7270.lo = _7273;
                    _7270.hi = _7275;
                    Interval _7271 = _7270;
                    Interval _7276 = _7271;
                    Interval _7278 = _7276;
                    Interval _7280 = _7278;
                    float _7281 = 0.5;
                    float _7267 = _7281;
                    float _7268 = _7281;
                    _7265.lo = _7267;
                    _7265.hi = _7268;
                    Interval _7266 = _7265;
                    Interval _7269 = _7266;
                    Interval _7282 = _7269;
                    Interval _11100 = imul(_7280, _7282, intervalFailed, optical_product_upper);
                    Interval _7283 = _11100;
                    Interval _11101 = iadd(_7279, _7283, intervalFailed);
                    Interval _7284 = _11101;
                    float _7260 = _7284.lo;
                    float _7261 = _7284.hi;
                    float _7255 = _7260;
                    float _7256 = _7261;
                    _7252.lo = _7255;
                    _7252.hi = _7256;
                    Interval _7253 = _7252;
                    Interval _7257 = _7253;
                    Interval _11114 = isin_body(_7257, intervalFailed, optical_product_upper);
                    Interval _7254 = _11114;
                    interval_sine_upper = _7254.hi;
                    float _7258 = _7254.lo;
                    float _7259 = _7258;
                    float _7262 = _7259;
                    float _7263 = interval_sine_upper;
                    _7250.lo = _7262;
                    _7250.hi = _7263;
                    Interval _7251 = _7250;
                    Interval _7264 = _7251;
                    Interval _7285 = _7264;
                    Interval _7704 = _7285;
                    Interval _7705 = _7599;
                    Interval _11131 = imul(_7704, _7705, intervalFailed, optical_product_upper);
                    Interval _7702 = _11131;
                    float _7707 = 9.0;
                    float _7247 = _7707;
                    float _7248 = _7707;
                    _7245.lo = _7247;
                    _7245.hi = _7248;
                    Interval _7246 = _7245;
                    Interval _7249 = _7246;
                    Interval _7708 = _7249;
                    float _7709 = 18.0;
                    float _7710 = 1000.0;
                    Interval _11141 = iratio(_7709, _7710, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7711 = _11141;
                    float _7712 = 39.0;
                    float _7713 = 10000.0;
                    Interval _11142 = iratio(_7712, _7713, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7714 = _11142;
                    Interval _7715 = _7702;
                    Interval _11144 = imul(_7714, _7715, intervalFailed, optical_product_upper);
                    Interval _7716 = _11144;
                    Interval _11145 = iadd(_7711, _7716, intervalFailed);
                    Interval _7717 = _11145;
                    Interval _11146 = imul(_7708, _7717, intervalFailed, optical_product_upper);
                    Interval _7718 = _11146;
                    Interval _7719 = _7604;
                    Interval _7238 = _7719;
                    float _7236 = 3.1415927410125732421875;
                    float _7231 = _7236;
                    float _11150 = interval_down(_7231, intervalFailed);
                    float _7232 = _11150;
                    float _7233 = _7236;
                    float _11152 = interval_up(_7233, intervalFailed);
                    float _7234 = _11152;
                    _7229.lo = _7232;
                    _7229.hi = _7234;
                    Interval _7230 = _7229;
                    Interval _7235 = _7230;
                    Interval _7237 = _7235;
                    Interval _7239 = _7237;
                    float _7240 = 0.5;
                    float _7226 = _7240;
                    float _7227 = _7240;
                    _7224.lo = _7226;
                    _7224.hi = _7227;
                    Interval _7225 = _7224;
                    Interval _7228 = _7225;
                    Interval _7241 = _7228;
                    Interval _11170 = imul(_7239, _7241, intervalFailed, optical_product_upper);
                    Interval _7242 = _11170;
                    Interval _11171 = iadd(_7238, _7242, intervalFailed);
                    Interval _7243 = _11171;
                    float _7219 = _7243.lo;
                    float _7220 = _7243.hi;
                    float _7214 = _7219;
                    float _7215 = _7220;
                    _7211.lo = _7214;
                    _7211.hi = _7215;
                    Interval _7212 = _7211;
                    Interval _7216 = _7212;
                    Interval _11184 = isin_body(_7216, intervalFailed, optical_product_upper);
                    Interval _7213 = _11184;
                    interval_sine_upper = _7213.hi;
                    float _7217 = _7213.lo;
                    float _7218 = _7217;
                    float _7221 = _7218;
                    float _7222 = interval_sine_upper;
                    _7209.lo = _7221;
                    _7209.hi = _7222;
                    Interval _7210 = _7209;
                    Interval _7223 = _7210;
                    Interval _7244 = _7223;
                    Interval _7720 = _7244;
                    Interval _11200 = imul(_7718, _7720, intervalFailed, optical_product_upper);
                    Interval _7721 = _11200;
                    Interval _7722 = _7664;
                    Interval _11202 = imul(_7721, _7722, intervalFailed, optical_product_upper);
                    Interval _7723 = _11202;
                    float _7724 = 1175.0;
                    float _7725 = 10000.0;
                    Interval _11203 = iratio(_7724, _7725, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7726 = _11203;
                    Interval _7727 = _7630;
                    Interval _7202 = _7727;
                    float _7200 = 3.1415927410125732421875;
                    float _7195 = _7200;
                    float _11207 = interval_down(_7195, intervalFailed);
                    float _7196 = _11207;
                    float _7197 = _7200;
                    float _11209 = interval_up(_7197, intervalFailed);
                    float _7198 = _11209;
                    _7193.lo = _7196;
                    _7193.hi = _7198;
                    Interval _7194 = _7193;
                    Interval _7199 = _7194;
                    Interval _7201 = _7199;
                    Interval _7203 = _7201;
                    float _7204 = 0.5;
                    float _7190 = _7204;
                    float _7191 = _7204;
                    _7188.lo = _7190;
                    _7188.hi = _7191;
                    Interval _7189 = _7188;
                    Interval _7192 = _7189;
                    Interval _7205 = _7192;
                    Interval _11227 = imul(_7203, _7205, intervalFailed, optical_product_upper);
                    Interval _7206 = _11227;
                    Interval _11228 = iadd(_7202, _7206, intervalFailed);
                    Interval _7207 = _11228;
                    float _7183 = _7207.lo;
                    float _7184 = _7207.hi;
                    float _7178 = _7183;
                    float _7179 = _7184;
                    _7175.lo = _7178;
                    _7175.hi = _7179;
                    Interval _7176 = _7175;
                    Interval _7180 = _7176;
                    Interval _11241 = isin_body(_7180, intervalFailed, optical_product_upper);
                    Interval _7177 = _11241;
                    interval_sine_upper = _7177.hi;
                    float _7181 = _7177.lo;
                    float _7182 = _7181;
                    float _7185 = _7182;
                    float _7186 = interval_sine_upper;
                    _7173.lo = _7185;
                    _7173.hi = _7186;
                    Interval _7174 = _7173;
                    Interval _7187 = _7174;
                    Interval _7208 = _7187;
                    Interval _7728 = _7208;
                    Interval _11257 = imul(_7726, _7728, intervalFailed, optical_product_upper);
                    Interval _7729 = _11257;
                    Interval _7730 = _7669;
                    Interval _11259 = imul(_7729, _7730, intervalFailed, optical_product_upper);
                    Interval _7731 = _11259;
                    Interval _11260 = iadd(_7723, _7731, intervalFailed);
                    Interval _7732 = _11260;
                    float _7733 = 45.0;
                    float _7734 = 1000.0;
                    Interval _11261 = iratio(_7733, _7734, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7735 = _11261;
                    Interval _7736 = _7647;
                    Interval _7166 = _7736;
                    float _7164 = 3.1415927410125732421875;
                    float _7159 = _7164;
                    float _11265 = interval_down(_7159, intervalFailed);
                    float _7160 = _11265;
                    float _7161 = _7164;
                    float _11267 = interval_up(_7161, intervalFailed);
                    float _7162 = _11267;
                    _7157.lo = _7160;
                    _7157.hi = _7162;
                    Interval _7158 = _7157;
                    Interval _7163 = _7158;
                    Interval _7165 = _7163;
                    Interval _7167 = _7165;
                    float _7168 = 0.5;
                    float _7154 = _7168;
                    float _7155 = _7168;
                    _7152.lo = _7154;
                    _7152.hi = _7155;
                    Interval _7153 = _7152;
                    Interval _7156 = _7153;
                    Interval _7169 = _7156;
                    Interval _11285 = imul(_7167, _7169, intervalFailed, optical_product_upper);
                    Interval _7170 = _11285;
                    Interval _11286 = iadd(_7166, _7170, intervalFailed);
                    Interval _7171 = _11286;
                    float _7147 = _7171.lo;
                    float _7148 = _7171.hi;
                    float _7142 = _7147;
                    float _7143 = _7148;
                    _7139.lo = _7142;
                    _7139.hi = _7143;
                    Interval _7140 = _7139;
                    Interval _7144 = _7140;
                    Interval _11299 = isin_body(_7144, intervalFailed, optical_product_upper);
                    Interval _7141 = _11299;
                    interval_sine_upper = _7141.hi;
                    float _7145 = _7141.lo;
                    float _7146 = _7145;
                    float _7149 = _7146;
                    float _7150 = interval_sine_upper;
                    _7137.lo = _7149;
                    _7137.hi = _7150;
                    Interval _7138 = _7137;
                    Interval _7151 = _7138;
                    Interval _7172 = _7151;
                    Interval _7737 = _7172;
                    Interval _11315 = imul(_7735, _7737, intervalFailed, optical_product_upper);
                    Interval _7738 = _11315;
                    Interval _7739 = _7674;
                    Interval _11317 = imul(_7738, _7739, intervalFailed, optical_product_upper);
                    Interval _7740 = _11317;
                    Interval _7133 = _7732;
                    Interval _7134 = _7740;
                    float _7130 = as_type<float>(as_type<uint>(_7134.hi) ^ 2147483648u);
                    float _7131 = as_type<float>(as_type<uint>(_7134.lo) ^ 2147483648u);
                    _7128.lo = _7130;
                    _7128.hi = _7131;
                    Interval _7129 = _7128;
                    Interval _7132 = _7129;
                    Interval _7135 = _7132;
                    Interval _11337 = iadd(_7133, _7135, intervalFailed);
                    Interval _7136 = _11337;
                    Interval _7706 = _7136;
                    float _7742 = 9.0;
                    float _7125 = _7742;
                    float _7126 = _7742;
                    _7123.lo = _7125;
                    _7123.hi = _7126;
                    Interval _7124 = _7123;
                    Interval _7127 = _7124;
                    Interval _7743 = _7127;
                    float _7744 = 11.0;
                    float _7745 = 1000.0;
                    Interval _11348 = iratio(_7744, _7745, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7746 = _11348;
                    float _7747 = 52.0;
                    float _7748 = 10000.0;
                    Interval _11349 = iratio(_7747, _7748, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7749 = _11349;
                    Interval _7750 = _7702;
                    Interval _11351 = imul(_7749, _7750, intervalFailed, optical_product_upper);
                    Interval _7751 = _11351;
                    Interval _7119 = _7746;
                    Interval _7120 = _7751;
                    float _7116 = as_type<float>(as_type<uint>(_7120.hi) ^ 2147483648u);
                    float _7117 = as_type<float>(as_type<uint>(_7120.lo) ^ 2147483648u);
                    _7114.lo = _7116;
                    _7114.hi = _7117;
                    Interval _7115 = _7114;
                    Interval _7118 = _7115;
                    Interval _7121 = _7118;
                    Interval _11371 = iadd(_7119, _7121, intervalFailed);
                    Interval _7122 = _11371;
                    Interval _7752 = _7122;
                    Interval _11373 = imul(_7743, _7752, intervalFailed, optical_product_upper);
                    Interval _7753 = _11373;
                    Interval _7754 = _7604;
                    Interval _7107 = _7754;
                    float _7105 = 3.1415927410125732421875;
                    float _7100 = _7105;
                    float _11377 = interval_down(_7100, intervalFailed);
                    float _7101 = _11377;
                    float _7102 = _7105;
                    float _11379 = interval_up(_7102, intervalFailed);
                    float _7103 = _11379;
                    _7098.lo = _7101;
                    _7098.hi = _7103;
                    Interval _7099 = _7098;
                    Interval _7104 = _7099;
                    Interval _7106 = _7104;
                    Interval _7108 = _7106;
                    float _7109 = 0.5;
                    float _7095 = _7109;
                    float _7096 = _7109;
                    _7093.lo = _7095;
                    _7093.hi = _7096;
                    Interval _7094 = _7093;
                    Interval _7097 = _7094;
                    Interval _7110 = _7097;
                    Interval _11397 = imul(_7108, _7110, intervalFailed, optical_product_upper);
                    Interval _7111 = _11397;
                    Interval _11398 = iadd(_7107, _7111, intervalFailed);
                    Interval _7112 = _11398;
                    float _7088 = _7112.lo;
                    float _7089 = _7112.hi;
                    float _7083 = _7088;
                    float _7084 = _7089;
                    _7080.lo = _7083;
                    _7080.hi = _7084;
                    Interval _7081 = _7080;
                    Interval _7085 = _7081;
                    Interval _11411 = isin_body(_7085, intervalFailed, optical_product_upper);
                    Interval _7082 = _11411;
                    interval_sine_upper = _7082.hi;
                    float _7086 = _7082.lo;
                    float _7087 = _7086;
                    float _7090 = _7087;
                    float _7091 = interval_sine_upper;
                    _7078.lo = _7090;
                    _7078.hi = _7091;
                    Interval _7079 = _7078;
                    Interval _7092 = _7079;
                    Interval _7113 = _7092;
                    Interval _7755 = _7113;
                    Interval _11427 = imul(_7753, _7755, intervalFailed, optical_product_upper);
                    Interval _7756 = _11427;
                    Interval _7757 = _7664;
                    Interval _11429 = imul(_7756, _7757, intervalFailed, optical_product_upper);
                    Interval _7758 = _11429;
                    float _7759 = 625.0;
                    float _7760 = 10000.0;
                    Interval _11430 = iratio(_7759, _7760, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7761 = _11430;
                    Interval _7762 = _7630;
                    Interval _7071 = _7762;
                    float _7069 = 3.1415927410125732421875;
                    float _7064 = _7069;
                    float _11434 = interval_down(_7064, intervalFailed);
                    float _7065 = _11434;
                    float _7066 = _7069;
                    float _11436 = interval_up(_7066, intervalFailed);
                    float _7067 = _11436;
                    _7062.lo = _7065;
                    _7062.hi = _7067;
                    Interval _7063 = _7062;
                    Interval _7068 = _7063;
                    Interval _7070 = _7068;
                    Interval _7072 = _7070;
                    float _7073 = 0.5;
                    float _7059 = _7073;
                    float _7060 = _7073;
                    _7057.lo = _7059;
                    _7057.hi = _7060;
                    Interval _7058 = _7057;
                    Interval _7061 = _7058;
                    Interval _7074 = _7061;
                    Interval _11454 = imul(_7072, _7074, intervalFailed, optical_product_upper);
                    Interval _7075 = _11454;
                    Interval _11455 = iadd(_7071, _7075, intervalFailed);
                    Interval _7076 = _11455;
                    float _7052 = _7076.lo;
                    float _7053 = _7076.hi;
                    float _7047 = _7052;
                    float _7048 = _7053;
                    _7044.lo = _7047;
                    _7044.hi = _7048;
                    Interval _7045 = _7044;
                    Interval _7049 = _7045;
                    Interval _11468 = isin_body(_7049, intervalFailed, optical_product_upper);
                    Interval _7046 = _11468;
                    interval_sine_upper = _7046.hi;
                    float _7050 = _7046.lo;
                    float _7051 = _7050;
                    float _7054 = _7051;
                    float _7055 = interval_sine_upper;
                    _7042.lo = _7054;
                    _7042.hi = _7055;
                    Interval _7043 = _7042;
                    Interval _7056 = _7043;
                    Interval _7077 = _7056;
                    Interval _7763 = _7077;
                    Interval _11484 = imul(_7761, _7763, intervalFailed, optical_product_upper);
                    Interval _7764 = _11484;
                    Interval _7765 = _7669;
                    Interval _11486 = imul(_7764, _7765, intervalFailed, optical_product_upper);
                    Interval _7766 = _11486;
                    Interval _7038 = _7758;
                    Interval _7039 = _7766;
                    float _7035 = as_type<float>(as_type<uint>(_7039.hi) ^ 2147483648u);
                    float _7036 = as_type<float>(as_type<uint>(_7039.lo) ^ 2147483648u);
                    _7033.lo = _7035;
                    _7033.hi = _7036;
                    Interval _7034 = _7033;
                    Interval _7037 = _7034;
                    Interval _7040 = _7037;
                    Interval _11506 = iadd(_7038, _7040, intervalFailed);
                    Interval _7041 = _11506;
                    Interval _7767 = _7041;
                    float _7768 = 110.0;
                    float _7769 = 1000.0;
                    Interval _11508 = iratio(_7768, _7769, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7770 = _11508;
                    Interval _7771 = _7647;
                    Interval _7026 = _7771;
                    float _7024 = 3.1415927410125732421875;
                    float _7019 = _7024;
                    float _11512 = interval_down(_7019, intervalFailed);
                    float _7020 = _11512;
                    float _7021 = _7024;
                    float _11514 = interval_up(_7021, intervalFailed);
                    float _7022 = _11514;
                    _7017.lo = _7020;
                    _7017.hi = _7022;
                    Interval _7018 = _7017;
                    Interval _7023 = _7018;
                    Interval _7025 = _7023;
                    Interval _7027 = _7025;
                    float _7028 = 0.5;
                    float _7014 = _7028;
                    float _7015 = _7028;
                    _7012.lo = _7014;
                    _7012.hi = _7015;
                    Interval _7013 = _7012;
                    Interval _7016 = _7013;
                    Interval _7029 = _7016;
                    Interval _11532 = imul(_7027, _7029, intervalFailed, optical_product_upper);
                    Interval _7030 = _11532;
                    Interval _11533 = iadd(_7026, _7030, intervalFailed);
                    Interval _7031 = _11533;
                    float _7007 = _7031.lo;
                    float _7008 = _7031.hi;
                    float _7002 = _7007;
                    float _7003 = _7008;
                    _6999.lo = _7002;
                    _6999.hi = _7003;
                    Interval _7000 = _6999;
                    Interval _7004 = _7000;
                    Interval _11546 = isin_body(_7004, intervalFailed, optical_product_upper);
                    Interval _7001 = _11546;
                    interval_sine_upper = _7001.hi;
                    float _7005 = _7001.lo;
                    float _7006 = _7005;
                    float _7009 = _7006;
                    float _7010 = interval_sine_upper;
                    _6997.lo = _7009;
                    _6997.hi = _7010;
                    Interval _6998 = _6997;
                    Interval _7011 = _6998;
                    Interval _7032 = _7011;
                    Interval _7772 = _7032;
                    Interval _11562 = imul(_7770, _7772, intervalFailed, optical_product_upper);
                    Interval _7773 = _11562;
                    Interval _7774 = _7674;
                    Interval _11564 = imul(_7773, _7774, intervalFailed, optical_product_upper);
                    Interval _7775 = _11564;
                    Interval _11565 = iadd(_7767, _7775, intervalFailed);
                    Interval _7741 = _11565;
                    Interval _7777 = _8338;
                    float _7778 = 173.0;
                    float _7779 = 1000.0;
                    Interval _11567 = iratio(_7778, _7779, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7780 = _11567;
                    Interval _11568 = imul(_7777, _7780, intervalFailed, optical_product_upper);
                    Interval _7781 = _11568;
                    Interval _7782 = _8339;
                    float _7783 = 129.0;
                    float _7784 = 1000.0;
                    Interval _11570 = iratio(_7783, _7784, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7785 = _11570;
                    Interval _11571 = imul(_7782, _7785, intervalFailed, optical_product_upper);
                    Interval _7786 = _11571;
                    Interval _11572 = iadd(_7781, _7786, intervalFailed);
                    Interval _7787 = _11572;
                    Interval _7788 = _8340;
                    float _7789 = 73.0;
                    float _7790 = 100.0;
                    Interval _11574 = iratio(_7789, _7790, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7791 = _11574;
                    Interval _11575 = imul(_7788, _7791, intervalFailed, optical_product_upper);
                    Interval _7792 = _11575;
                    Interval _6993 = _7787;
                    Interval _6994 = _7792;
                    float _6990 = as_type<float>(as_type<uint>(_6994.hi) ^ 2147483648u);
                    float _6991 = as_type<float>(as_type<uint>(_6994.lo) ^ 2147483648u);
                    _6988.lo = _6990;
                    _6988.hi = _6991;
                    Interval _6989 = _6988;
                    Interval _6992 = _6989;
                    Interval _6995 = _6992;
                    Interval _11595 = iadd(_6993, _6995, intervalFailed);
                    Interval _6996 = _11595;
                    Interval _7776 = _6996;
                    Interval _7794 = _8341;
                    float _7795 = 216.0;
                    float _7796 = 1000.0;
                    Interval _11598 = iratio(_7795, _7796, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7797 = _11598;
                    Interval _6977 = _7794;
                    Interval _6978 = _7797;
                    Interval _11601 = imul(_6977, _6978, intervalFailed, optical_product_upper);
                    Interval _6979 = _11601;
                    bool _6969 = false;
                    if (_6979.lo <= 0.0)
                    {
                        _6969 = _6979.hi >= 0.0;
                    }
                    if (_6969)
                    {
                        _6968 = 0.0;
                    }
                    else
                    {
                        _6968 = precise::min(abs(_6979.lo), abs(_6979.hi));
                    }
                    float _6967 = _6968;
                    float _6970 = precise::max(abs(_6979.lo), abs(_6979.hi));
                    float _6971 = spvFMul(_6967, _6967);
                    float _11631 = interval_down(_6971, intervalFailed);
                    float _6972 = precise::max(0.0, _11631);
                    float _6973 = spvFMul(_6970, _6970);
                    float _11635 = interval_up(_6973, intervalFailed);
                    float _6974 = _11635;
                    _6965.lo = _6972;
                    _6965.hi = _6974;
                    Interval _6966 = _6965;
                    Interval _6975 = _6966;
                    Interval _6976 = _6975;
                    float _6980 = 1.0;
                    float _6962 = _6980;
                    float _6963 = _6980;
                    _6960.lo = _6962;
                    _6960.hi = _6963;
                    Interval _6961 = _6960;
                    Interval _6964 = _6961;
                    Interval _6981 = _6964;
                    float _6982 = 1.0;
                    float _6957 = _6982;
                    float _6958 = _6982;
                    _6955.lo = _6957;
                    _6955.hi = _6958;
                    Interval _6956 = _6955;
                    Interval _6959 = _6956;
                    Interval _6983 = _6959;
                    Interval _6984 = _6976;
                    bool _6948 = false;
                    if (_6984.lo <= 0.0)
                    {
                        _6948 = _6984.hi >= 0.0;
                    }
                    if (_6948)
                    {
                        _6947 = 0.0;
                    }
                    else
                    {
                        _6947 = precise::min(abs(_6984.lo), abs(_6984.hi));
                    }
                    float _6946 = _6947;
                    float _6949 = precise::max(abs(_6984.lo), abs(_6984.hi));
                    float _6950 = spvFMul(_6946, _6946);
                    float _11691 = interval_down(_6950, intervalFailed);
                    float _6951 = precise::max(0.0, _11691);
                    float _6952 = spvFMul(_6949, _6949);
                    float _11695 = interval_up(_6952, intervalFailed);
                    float _6953 = _11695;
                    _6944.lo = _6951;
                    _6944.hi = _6953;
                    Interval _6945 = _6944;
                    Interval _6954 = _6945;
                    Interval _6985 = _6954;
                    Interval _11703 = iadd(_6983, _6985, intervalFailed);
                    Interval _6986 = _11703;
                    Interval _11704 = idiv(_6981, _6986, intervalFailed, interval_divide_upper);
                    Interval _6987 = _11704;
                    Interval _7793 = _6987;
                    Interval _7798 = _7679;
                    float _7799 = 38.0;
                    float _7800 = 100.0;
                    Interval _11707 = iratio(_7799, _7800, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7801 = _11707;
                    Interval _7802 = _7776;
                    float _6939 = _7802.lo;
                    float _6940 = _7802.hi;
                    float _6934 = _6939;
                    float _6935 = _6940;
                    _6931.lo = _6934;
                    _6931.hi = _6935;
                    Interval _6932 = _6931;
                    Interval _6936 = _6932;
                    Interval _11721 = isin_body(_6936, intervalFailed, optical_product_upper);
                    Interval _6933 = _11721;
                    interval_sine_upper = _6933.hi;
                    float _6937 = _6933.lo;
                    float _6938 = _6937;
                    float _6941 = _6938;
                    float _6942 = interval_sine_upper;
                    _6929.lo = _6941;
                    _6929.hi = _6942;
                    Interval _6930 = _6929;
                    Interval _6943 = _6930;
                    Interval _7803 = _6943;
                    Interval _11736 = imul(_7801, _7803, intervalFailed, optical_product_upper);
                    Interval _7804 = _11736;
                    Interval _7805 = _7793;
                    Interval _11738 = imul(_7804, _7805, intervalFailed, optical_product_upper);
                    Interval _7806 = _11738;
                    Interval _11739 = iadd(_7798, _7806, intervalFailed);
                    _7679 = _11739;
                    Interval _7807 = _7706;
                    float _7808 = 6574.0;
                    float _7809 = 100000.0;
                    Interval _11741 = iratio(_7808, _7809, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7810 = _11741;
                    Interval _7811 = _7776;
                    Interval _6922 = _7811;
                    float _6920 = 3.1415927410125732421875;
                    float _6915 = _6920;
                    float _11745 = interval_down(_6915, intervalFailed);
                    float _6916 = _11745;
                    float _6917 = _6920;
                    float _11747 = interval_up(_6917, intervalFailed);
                    float _6918 = _11747;
                    _6913.lo = _6916;
                    _6913.hi = _6918;
                    Interval _6914 = _6913;
                    Interval _6919 = _6914;
                    Interval _6921 = _6919;
                    Interval _6923 = _6921;
                    float _6924 = 0.5;
                    float _6910 = _6924;
                    float _6911 = _6924;
                    _6908.lo = _6910;
                    _6908.hi = _6911;
                    Interval _6909 = _6908;
                    Interval _6912 = _6909;
                    Interval _6925 = _6912;
                    Interval _11765 = imul(_6923, _6925, intervalFailed, optical_product_upper);
                    Interval _6926 = _11765;
                    Interval _11766 = iadd(_6922, _6926, intervalFailed);
                    Interval _6927 = _11766;
                    float _6903 = _6927.lo;
                    float _6904 = _6927.hi;
                    float _6898 = _6903;
                    float _6899 = _6904;
                    _6895.lo = _6898;
                    _6895.hi = _6899;
                    Interval _6896 = _6895;
                    Interval _6900 = _6896;
                    Interval _11779 = isin_body(_6900, intervalFailed, optical_product_upper);
                    Interval _6897 = _11779;
                    interval_sine_upper = _6897.hi;
                    float _6901 = _6897.lo;
                    float _6902 = _6901;
                    float _6905 = _6902;
                    float _6906 = interval_sine_upper;
                    _6893.lo = _6905;
                    _6893.hi = _6906;
                    Interval _6894 = _6893;
                    Interval _6907 = _6894;
                    Interval _6928 = _6907;
                    Interval _7812 = _6928;
                    Interval _11795 = imul(_7810, _7812, intervalFailed, optical_product_upper);
                    Interval _7813 = _11795;
                    Interval _7814 = _7793;
                    Interval _11797 = imul(_7813, _7814, intervalFailed, optical_product_upper);
                    Interval _7815 = _11797;
                    Interval _11798 = iadd(_7807, _7815, intervalFailed);
                    _7706 = _11798;
                    Interval _7816 = _7741;
                    float _7817 = 4902.0;
                    float _7818 = 100000.0;
                    Interval _11800 = iratio(_7817, _7818, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7819 = _11800;
                    Interval _7820 = _7776;
                    Interval _6886 = _7820;
                    float _6884 = 3.1415927410125732421875;
                    float _6879 = _6884;
                    float _11804 = interval_down(_6879, intervalFailed);
                    float _6880 = _11804;
                    float _6881 = _6884;
                    float _11806 = interval_up(_6881, intervalFailed);
                    float _6882 = _11806;
                    _6877.lo = _6880;
                    _6877.hi = _6882;
                    Interval _6878 = _6877;
                    Interval _6883 = _6878;
                    Interval _6885 = _6883;
                    Interval _6887 = _6885;
                    float _6888 = 0.5;
                    float _6874 = _6888;
                    float _6875 = _6888;
                    _6872.lo = _6874;
                    _6872.hi = _6875;
                    Interval _6873 = _6872;
                    Interval _6876 = _6873;
                    Interval _6889 = _6876;
                    Interval _11824 = imul(_6887, _6889, intervalFailed, optical_product_upper);
                    Interval _6890 = _11824;
                    Interval _11825 = iadd(_6886, _6890, intervalFailed);
                    Interval _6891 = _11825;
                    float _6867 = _6891.lo;
                    float _6868 = _6891.hi;
                    float _6862 = _6867;
                    float _6863 = _6868;
                    _6859.lo = _6862;
                    _6859.hi = _6863;
                    Interval _6860 = _6859;
                    Interval _6864 = _6860;
                    Interval _11838 = isin_body(_6864, intervalFailed, optical_product_upper);
                    Interval _6861 = _11838;
                    interval_sine_upper = _6861.hi;
                    float _6865 = _6861.lo;
                    float _6866 = _6865;
                    float _6869 = _6866;
                    float _6870 = interval_sine_upper;
                    _6857.lo = _6869;
                    _6857.hi = _6870;
                    Interval _6858 = _6857;
                    Interval _6871 = _6858;
                    Interval _6892 = _6871;
                    Interval _7821 = _6892;
                    Interval _11854 = imul(_7819, _7821, intervalFailed, optical_product_upper);
                    Interval _7822 = _11854;
                    Interval _7823 = _7793;
                    Interval _11856 = imul(_7822, _7823, intervalFailed, optical_product_upper);
                    Interval _7824 = _11856;
                    Interval _11857 = iadd(_7816, _7824, intervalFailed);
                    _7741 = _11857;
                    if (_8341.lo < 24.0)
                    {
                        Interval _7826 = _8338;
                        float _7827 = 128.0;
                        float _6854 = _7827;
                        float _6855 = _7827;
                        _6852.lo = _6854;
                        _6852.hi = _6855;
                        Interval _6853 = _6852;
                        Interval _6856 = _6853;
                        Interval _7828 = _6856;
                        Interval _11873 = idiv(_7826, _7828, intervalFailed, interval_divide_upper);
                        Interval _7825 = _11873;
                        Interval _7830 = _8339;
                        float _7831 = 128.0;
                        float _6849 = _7831;
                        float _6850 = _7831;
                        _6847.lo = _6849;
                        _6847.hi = _6850;
                        Interval _6848 = _6847;
                        Interval _6851 = _6848;
                        Interval _7832 = _6851;
                        Interval _11884 = idiv(_7830, _7832, intervalFailed, interval_divide_upper);
                        Interval _7829 = _11884;
                        float _11887 = floor(_7825.lo);
                        float _11890 = floor(_7825.hi);
                        bool _7833 = true;
                        if ((isunordered(_11887, _11890) || _11887 == _11890))
                        {
                            _7833 = floor(_7829.lo) != floor(_7829.hi);
                        }
                        if (_7833)
                        {
                            Interval _7834 = _7679;
                            float _7835 = 0.0;
                            float _7836 = 10.0;
                            _6845.lo = _7835;
                            _6845.hi = _7836;
                            Interval _6846 = _6845;
                            Interval _7837 = _6846;
                            Interval _11912 = iadd(_7834, _7837, intervalFailed);
                            _7679 = _11912;
                            Interval _7838 = _7706;
                            float _7839 = -256.0;
                            float _7840 = 256.0;
                            _6843.lo = _7839;
                            _6843.hi = _7840;
                            Interval _6844 = _6843;
                            Interval _7841 = _6844;
                            Interval _11920 = iadd(_7838, _7841, intervalFailed);
                            _7706 = _11920;
                            Interval _7842 = _7741;
                            float _7843 = -256.0;
                            float _7844 = 256.0;
                            _6841.lo = _7843;
                            _6841.hi = _7844;
                            Interval _6842 = _6841;
                            Interval _7845 = _6842;
                            Interval _11928 = iadd(_7842, _7845, intervalFailed);
                            _7741 = _11928;
                        }
                        else
                        {
                            int _7846 = int(floor(_7825.lo));
                            int _7847 = int(floor(_7829.lo));
                            int _7849 = _7846;
                            int _7850 = _7847;
                            uint _6837 = (uint(_7849) * 1597334677u) ^ (uint(_7850) * 3812015801u);
                            _6837 ^= (_6837 >> 16u);
                            _6837 *= 2246822519u;
                            _6837 ^= (_6837 >> 13u);
                            float _6838 = float(_6837 & 65535u);
                            float _6839 = 65535.0;
                            Interval _11956 = iratio(_6838, _6839, intervalFailed, optical_product_upper, interval_divide_upper);
                            Interval _6840 = _11956;
                            Interval _7848 = _6840;
                            if (_7848.hi > 0.63999998569488525390625)
                            {
                                Interval _7852 = _7825;
                                float _7853 = float(_7846);
                                float _6834 = _7853;
                                float _6835 = _7853;
                                _6832.lo = _6834;
                                _6832.hi = _6835;
                                Interval _6833 = _6832;
                                Interval _6836 = _6833;
                                Interval _7854 = _6836;
                                Interval _6828 = _7852;
                                Interval _6829 = _7854;
                                float _6825 = as_type<float>(as_type<uint>(_6829.hi) ^ 2147483648u);
                                float _6826 = as_type<float>(as_type<uint>(_6829.lo) ^ 2147483648u);
                                _6823.lo = _6825;
                                _6823.hi = _6826;
                                Interval _6824 = _6823;
                                Interval _6827 = _6824;
                                Interval _6830 = _6827;
                                Interval _11994 = iadd(_6828, _6830, intervalFailed);
                                Interval _6831 = _11994;
                                Interval _7855 = _6831;
                                float _7856 = 28.0;
                                float _7857 = 100.0;
                                Interval _11996 = iratio(_7856, _7857, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7858 = _11996;
                                float _7859 = 44.0;
                                float _7860 = 100.0;
                                Interval _11997 = iratio(_7859, _7860, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7861 = _11997;
                                int _7862 = _7846 + 19;
                                int _7863 = _7847;
                                uint _6819 = (uint(_7862) * 1597334677u) ^ (uint(_7863) * 3812015801u);
                                _6819 ^= (_6819 >> 16u);
                                _6819 *= 2246822519u;
                                _6819 ^= (_6819 >> 13u);
                                float _6820 = float(_6819 & 65535u);
                                float _6821 = 65535.0;
                                Interval _12017 = iratio(_6820, _6821, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6822 = _12017;
                                Interval _7864 = _6822;
                                Interval _12019 = imul(_7861, _7864, intervalFailed, optical_product_upper);
                                Interval _7865 = _12019;
                                Interval _12020 = iadd(_7858, _7865, intervalFailed);
                                Interval _7866 = _12020;
                                Interval _6815 = _7855;
                                Interval _6816 = _7866;
                                float _6812 = as_type<float>(as_type<uint>(_6816.hi) ^ 2147483648u);
                                float _6813 = as_type<float>(as_type<uint>(_6816.lo) ^ 2147483648u);
                                _6810.lo = _6812;
                                _6810.hi = _6813;
                                Interval _6811 = _6810;
                                Interval _6814 = _6811;
                                Interval _6817 = _6814;
                                Interval _12040 = iadd(_6815, _6817, intervalFailed);
                                Interval _6818 = _12040;
                                Interval _7851 = _6818;
                                Interval _7868 = _7829;
                                float _7869 = float(_7847);
                                float _6807 = _7869;
                                float _6808 = _7869;
                                _6805.lo = _6807;
                                _6805.hi = _6808;
                                Interval _6806 = _6805;
                                Interval _6809 = _6806;
                                Interval _7870 = _6809;
                                Interval _6801 = _7868;
                                Interval _6802 = _7870;
                                float _6798 = as_type<float>(as_type<uint>(_6802.hi) ^ 2147483648u);
                                float _6799 = as_type<float>(as_type<uint>(_6802.lo) ^ 2147483648u);
                                _6796.lo = _6798;
                                _6796.hi = _6799;
                                Interval _6797 = _6796;
                                Interval _6800 = _6797;
                                Interval _6803 = _6800;
                                Interval _12073 = iadd(_6801, _6803, intervalFailed);
                                Interval _6804 = _12073;
                                Interval _7871 = _6804;
                                float _7872 = 28.0;
                                float _7873 = 100.0;
                                Interval _12075 = iratio(_7872, _7873, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7874 = _12075;
                                float _7875 = 44.0;
                                float _7876 = 100.0;
                                Interval _12076 = iratio(_7875, _7876, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7877 = _12076;
                                int _7878 = _7846;
                                int _7879 = _7847 + 29;
                                uint _6792 = (uint(_7878) * 1597334677u) ^ (uint(_7879) * 3812015801u);
                                _6792 ^= (_6792 >> 16u);
                                _6792 *= 2246822519u;
                                _6792 ^= (_6792 >> 13u);
                                float _6793 = float(_6792 & 65535u);
                                float _6794 = 65535.0;
                                Interval _12096 = iratio(_6793, _6794, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6795 = _12096;
                                Interval _7880 = _6795;
                                Interval _12098 = imul(_7877, _7880, intervalFailed, optical_product_upper);
                                Interval _7881 = _12098;
                                Interval _12099 = iadd(_7874, _7881, intervalFailed);
                                Interval _7882 = _12099;
                                Interval _6788 = _7871;
                                Interval _6789 = _7882;
                                float _6785 = as_type<float>(as_type<uint>(_6789.hi) ^ 2147483648u);
                                float _6786 = as_type<float>(as_type<uint>(_6789.lo) ^ 2147483648u);
                                _6783.lo = _6785;
                                _6783.hi = _6786;
                                Interval _6784 = _6783;
                                Interval _6787 = _6784;
                                Interval _6790 = _6787;
                                Interval _12119 = iadd(_6788, _6790, intervalFailed);
                                Interval _6791 = _12119;
                                Interval _7867 = _6791;
                                Interval _7884 = _8340;
                                float _7885 = 14.0;
                                float _7886 = 100.0;
                                Interval _12122 = iratio(_7885, _7886, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7887 = _12122;
                                Interval _12123 = imul(_7884, _7887, intervalFailed, optical_product_upper);
                                Interval _7888 = _12123;
                                Interval _7889 = _7848;
                                float _7890 = 7.0;
                                float _6780 = _7890;
                                float _6781 = _7890;
                                _6778.lo = _6780;
                                _6778.hi = _6781;
                                Interval _6779 = _6778;
                                Interval _6782 = _6779;
                                Interval _7891 = _6782;
                                Interval _12134 = imul(_7889, _7891, intervalFailed, optical_product_upper);
                                Interval _7892 = _12134;
                                Interval _12135 = iadd(_7888, _7892, intervalFailed);
                                Interval _7883 = _12135;
                                if (floor(_7883.lo) == floor(_7883.hi))
                                {
                                    Interval _7893 = _7883;
                                    float _7894 = floor(_7883.lo);
                                    float _6775 = _7894;
                                    float _6776 = _7894;
                                    _6773.lo = _6775;
                                    _6773.hi = _6776;
                                    Interval _6774 = _6773;
                                    Interval _6777 = _6774;
                                    Interval _7895 = _6777;
                                    Interval _6769 = _7893;
                                    Interval _6770 = _7895;
                                    float _6766 = as_type<float>(as_type<uint>(_6770.hi) ^ 2147483648u);
                                    float _6767 = as_type<float>(as_type<uint>(_6770.lo) ^ 2147483648u);
                                    _6764.lo = _6766;
                                    _6764.hi = _6767;
                                    Interval _6765 = _6764;
                                    Interval _6768 = _6765;
                                    Interval _6771 = _6768;
                                    Interval _12178 = iadd(_6769, _6771, intervalFailed);
                                    Interval _6772 = _12178;
                                    _7883 = _6772;
                                }
                                else
                                {
                                    float _7896 = 0.0;
                                    float _7897 = 1.0;
                                    _6762.lo = _7896;
                                    _6762.hi = _7897;
                                    Interval _6763 = _6762;
                                    _7883 = _6763;
                                }
                                Interval _7899 = _7883;
                                float _7900 = 3.1415927410125732421875;
                                float _6757 = _7900;
                                float _12188 = interval_down(_6757, intervalFailed);
                                float _6758 = _12188;
                                float _6759 = _7900;
                                float _12190 = interval_up(_6759, intervalFailed);
                                float _6760 = _12190;
                                _6755.lo = _6758;
                                _6755.hi = _6760;
                                Interval _6756 = _6755;
                                Interval _6761 = _6756;
                                Interval _7901 = _6761;
                                Interval _12198 = imul(_7899, _7901, intervalFailed, optical_product_upper);
                                Interval _7902 = _12198;
                                float _6750 = _7902.lo;
                                float _6751 = _7902.hi;
                                float _6745 = _6750;
                                float _6746 = _6751;
                                _6742.lo = _6745;
                                _6742.hi = _6746;
                                Interval _6743 = _6742;
                                Interval _6747 = _6743;
                                Interval _12211 = isin_body(_6747, intervalFailed, optical_product_upper);
                                Interval _6744 = _12211;
                                interval_sine_upper = _6744.hi;
                                float _6748 = _6744.lo;
                                float _6749 = _6748;
                                float _6752 = _6749;
                                float _6753 = interval_sine_upper;
                                _6740.lo = _6752;
                                _6740.hi = _6753;
                                Interval _6741 = _6740;
                                Interval _6754 = _6741;
                                Interval _7903 = _6754;
                                bool _6733 = false;
                                if (_7903.lo <= 0.0)
                                {
                                    _6733 = _7903.hi >= 0.0;
                                }
                                if (_6733)
                                {
                                    _6732 = 0.0;
                                }
                                else
                                {
                                    _6732 = precise::min(abs(_7903.lo), abs(_7903.hi));
                                }
                                float _6731 = _6732;
                                float _6734 = precise::max(abs(_7903.lo), abs(_7903.hi));
                                float _6735 = spvFMul(_6731, _6731);
                                float _12255 = interval_down(_6735, intervalFailed);
                                float _6736 = precise::max(0.0, _12255);
                                float _6737 = spvFMul(_6734, _6734);
                                float _12259 = interval_up(_6737, intervalFailed);
                                float _6738 = _12259;
                                _6729.lo = _6736;
                                _6729.hi = _6738;
                                Interval _6730 = _6729;
                                Interval _6739 = _6730;
                                Interval _7898 = _6739;
                                float _7905 = 55.0;
                                float _7906 = 1000.0;
                                Interval _12267 = iratio(_7905, _7906, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7907 = _12267;
                                float _7908 = 14.0;
                                float _7909 = 100.0;
                                Interval _12268 = iratio(_7908, _7909, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7910 = _12268;
                                Interval _7911 = _7883;
                                Interval _12270 = imul(_7910, _7911, intervalFailed, optical_product_upper);
                                Interval _7912 = _12270;
                                Interval _12271 = iadd(_7907, _7912, intervalFailed);
                                Interval _7904 = _12271;
                                Interval _7914 = _7904;
                                bool _6722 = false;
                                if (_7914.lo <= 0.0)
                                {
                                    _6722 = _7914.hi >= 0.0;
                                }
                                if (_6722)
                                {
                                    _6721 = 0.0;
                                }
                                else
                                {
                                    _6721 = precise::min(abs(_7914.lo), abs(_7914.hi));
                                }
                                float _6720 = _6721;
                                float _6723 = precise::max(abs(_7914.lo), abs(_7914.hi));
                                float _6724 = spvFMul(_6720, _6720);
                                float _12302 = interval_down(_6724, intervalFailed);
                                float _6725 = precise::max(0.0, _12302);
                                float _6726 = spvFMul(_6723, _6723);
                                float _12306 = interval_up(_6726, intervalFailed);
                                float _6727 = _12306;
                                _6718.lo = _6725;
                                _6718.hi = _6727;
                                Interval _6719 = _6718;
                                Interval _6728 = _6719;
                                Interval _7913 = _6728;
                                float _7916 = 1.0;
                                float _6715 = _7916;
                                float _6716 = _7916;
                                _6713.lo = _6715;
                                _6713.hi = _6716;
                                Interval _6714 = _6713;
                                Interval _6717 = _6714;
                                Interval _7917 = _6717;
                                Interval _7918 = _7851;
                                bool _6706 = false;
                                if (_7918.lo <= 0.0)
                                {
                                    _6706 = _7918.hi >= 0.0;
                                }
                                if (_6706)
                                {
                                    _6705 = 0.0;
                                }
                                else
                                {
                                    _6705 = precise::min(abs(_7918.lo), abs(_7918.hi));
                                }
                                float _6704 = _6705;
                                float _6707 = precise::max(abs(_7918.lo), abs(_7918.hi));
                                float _6708 = spvFMul(_6704, _6704);
                                float _12353 = interval_down(_6708, intervalFailed);
                                float _6709 = precise::max(0.0, _12353);
                                float _6710 = spvFMul(_6707, _6707);
                                float _12357 = interval_up(_6710, intervalFailed);
                                float _6711 = _12357;
                                _6702.lo = _6709;
                                _6702.hi = _6711;
                                Interval _6703 = _6702;
                                Interval _6712 = _6703;
                                Interval _7919 = _6712;
                                Interval _7920 = _7867;
                                bool _6695 = false;
                                if (_7920.lo <= 0.0)
                                {
                                    _6695 = _7920.hi >= 0.0;
                                }
                                if (_6695)
                                {
                                    _6694 = 0.0;
                                }
                                else
                                {
                                    _6694 = precise::min(abs(_7920.lo), abs(_7920.hi));
                                }
                                float _6693 = _6694;
                                float _6696 = precise::max(abs(_7920.lo), abs(_7920.hi));
                                float _6697 = spvFMul(_6693, _6693);
                                float _12395 = interval_down(_6697, intervalFailed);
                                float _6698 = precise::max(0.0, _12395);
                                float _6699 = spvFMul(_6696, _6696);
                                float _12399 = interval_up(_6699, intervalFailed);
                                float _6700 = _12399;
                                _6691.lo = _6698;
                                _6691.hi = _6700;
                                Interval _6692 = _6691;
                                Interval _6701 = _6692;
                                Interval _7921 = _6701;
                                Interval _12407 = iadd(_7919, _7921, intervalFailed);
                                Interval _7922 = _12407;
                                Interval _7923 = _7913;
                                Interval _12409 = idiv(_7922, _7923, intervalFailed, interval_divide_upper);
                                Interval _7924 = _12409;
                                Interval _6687 = _7917;
                                Interval _6688 = _7924;
                                float _6684 = as_type<float>(as_type<uint>(_6688.hi) ^ 2147483648u);
                                float _6685 = as_type<float>(as_type<uint>(_6688.lo) ^ 2147483648u);
                                _6682.lo = _6684;
                                _6682.hi = _6685;
                                Interval _6683 = _6682;
                                Interval _6686 = _6683;
                                Interval _6689 = _6686;
                                Interval _12429 = iadd(_6687, _6689, intervalFailed);
                                Interval _6690 = _12429;
                                Interval _7925 = _6690;
                                float _7926 = 0.0;
                                float _6679 = _7926;
                                float _6680 = _7926;
                                _6677.lo = _6679;
                                _6677.hi = _6680;
                                Interval _6678 = _6677;
                                Interval _6681 = _6678;
                                Interval _7927 = _6681;
                                float _7928 = 1.0;
                                float _6674 = _7928;
                                float _6675 = _7928;
                                _6672.lo = _6674;
                                _6672.hi = _6675;
                                Interval _6673 = _6672;
                                Interval _6676 = _6673;
                                Interval _7929 = _6676;
                                Interval _6667 = _7925;
                                Interval _6668 = _7927;
                                float _6664 = precise::max(_6667.lo, _6668.lo);
                                float _6665 = precise::max(_6667.hi, _6668.hi);
                                _6662.lo = _6664;
                                _6662.hi = _6665;
                                Interval _6663 = _6662;
                                Interval _6666 = _6663;
                                Interval _6669 = _6666;
                                Interval _6670 = _7929;
                                float _6659 = precise::min(_6669.lo, _6670.lo);
                                float _6660 = precise::min(_6669.hi, _6670.hi);
                                _6657.lo = _6659;
                                _6657.hi = _6660;
                                Interval _6658 = _6657;
                                Interval _6661 = _6658;
                                Interval _6671 = _6661;
                                Interval _7915 = _6671;
                                float _7931 = 10.0;
                                float _6654 = _7931;
                                float _6655 = _7931;
                                _6652.lo = _6654;
                                _6652.hi = _6655;
                                Interval _6653 = _6652;
                                Interval _6656 = _6653;
                                Interval _7932 = _6656;
                                Interval _7933 = _7898;
                                Interval _12497 = imul(_7932, _7933, intervalFailed, optical_product_upper);
                                Interval _7934 = _12497;
                                Interval _7935 = _8341;
                                float _7936 = 18.0;
                                float _7937 = 100.0;
                                Interval _12499 = iratio(_7936, _7937, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7938 = _12499;
                                Interval _6641 = _7935;
                                Interval _6642 = _7938;
                                Interval _12502 = imul(_6641, _6642, intervalFailed, optical_product_upper);
                                Interval _6643 = _12502;
                                bool _6633 = false;
                                if (_6643.lo <= 0.0)
                                {
                                    _6633 = _6643.hi >= 0.0;
                                }
                                if (_6633)
                                {
                                    _6632 = 0.0;
                                }
                                else
                                {
                                    _6632 = precise::min(abs(_6643.lo), abs(_6643.hi));
                                }
                                float _6631 = _6632;
                                float _6634 = precise::max(abs(_6643.lo), abs(_6643.hi));
                                float _6635 = spvFMul(_6631, _6631);
                                float _12532 = interval_down(_6635, intervalFailed);
                                float _6636 = precise::max(0.0, _12532);
                                float _6637 = spvFMul(_6634, _6634);
                                float _12536 = interval_up(_6637, intervalFailed);
                                float _6638 = _12536;
                                _6629.lo = _6636;
                                _6629.hi = _6638;
                                Interval _6630 = _6629;
                                Interval _6639 = _6630;
                                Interval _6640 = _6639;
                                float _6644 = 1.0;
                                float _6626 = _6644;
                                float _6627 = _6644;
                                _6624.lo = _6626;
                                _6624.hi = _6627;
                                Interval _6625 = _6624;
                                Interval _6628 = _6625;
                                Interval _6645 = _6628;
                                float _6646 = 1.0;
                                float _6621 = _6646;
                                float _6622 = _6646;
                                _6619.lo = _6621;
                                _6619.hi = _6622;
                                Interval _6620 = _6619;
                                Interval _6623 = _6620;
                                Interval _6647 = _6623;
                                Interval _6648 = _6640;
                                bool _6612 = false;
                                if (_6648.lo <= 0.0)
                                {
                                    _6612 = _6648.hi >= 0.0;
                                }
                                if (_6612)
                                {
                                    _6611 = 0.0;
                                }
                                else
                                {
                                    _6611 = precise::min(abs(_6648.lo), abs(_6648.hi));
                                }
                                float _6610 = _6611;
                                float _6613 = precise::max(abs(_6648.lo), abs(_6648.hi));
                                float _6614 = spvFMul(_6610, _6610);
                                float _12592 = interval_down(_6614, intervalFailed);
                                float _6615 = precise::max(0.0, _12592);
                                float _6616 = spvFMul(_6613, _6613);
                                float _12596 = interval_up(_6616, intervalFailed);
                                float _6617 = _12596;
                                _6608.lo = _6615;
                                _6608.hi = _6617;
                                Interval _6609 = _6608;
                                Interval _6618 = _6609;
                                Interval _6649 = _6618;
                                Interval _12604 = iadd(_6647, _6649, intervalFailed);
                                Interval _6650 = _12604;
                                Interval _12605 = idiv(_6645, _6650, intervalFailed, interval_divide_upper);
                                Interval _6651 = _12605;
                                Interval _7939 = _6651;
                                Interval _12607 = imul(_7934, _7939, intervalFailed, optical_product_upper);
                                Interval _7930 = _12607;
                                Interval _7941 = _7915;
                                bool _6601 = false;
                                if (_7941.lo <= 0.0)
                                {
                                    _6601 = _7941.hi >= 0.0;
                                }
                                if (_6601)
                                {
                                    _6600 = 0.0;
                                }
                                else
                                {
                                    _6600 = precise::min(abs(_7941.lo), abs(_7941.hi));
                                }
                                float _6599 = _6600;
                                float _6602 = precise::max(abs(_7941.lo), abs(_7941.hi));
                                float _6603 = spvFMul(_6599, _6599);
                                float _12638 = interval_down(_6603, intervalFailed);
                                float _6604 = precise::max(0.0, _12638);
                                float _6605 = spvFMul(_6602, _6602);
                                float _12642 = interval_up(_6605, intervalFailed);
                                float _6606 = _12642;
                                _6597.lo = _6604;
                                _6597.hi = _6606;
                                Interval _6598 = _6597;
                                Interval _6607 = _6598;
                                Interval _7940 = _6607;
                                Interval _7943 = _7930;
                                Interval _7944 = _7940;
                                Interval _12652 = imul(_7943, _7944, intervalFailed, optical_product_upper);
                                Interval _7945 = _12652;
                                Interval _7946 = _7915;
                                Interval _12654 = imul(_7945, _7946, intervalFailed, optical_product_upper);
                                Interval _7942 = _12654;
                                float _7948 = -6.0;
                                float _6594 = _7948;
                                float _6595 = _7948;
                                _6592.lo = _6594;
                                _6592.hi = _6595;
                                Interval _6593 = _6592;
                                Interval _6596 = _6593;
                                Interval _7949 = _6596;
                                Interval _7950 = _7930;
                                Interval _12665 = imul(_7949, _7950, intervalFailed, optical_product_upper);
                                Interval _7951 = _12665;
                                Interval _7952 = _7940;
                                Interval _12667 = imul(_7951, _7952, intervalFailed, optical_product_upper);
                                Interval _7953 = _12667;
                                float _7954 = 128.0;
                                float _6589 = _7954;
                                float _6590 = _7954;
                                _6587.lo = _6589;
                                _6587.hi = _6590;
                                Interval _6588 = _6587;
                                Interval _6591 = _6588;
                                Interval _7955 = _6591;
                                Interval _7956 = _7913;
                                Interval _12678 = imul(_7955, _7956, intervalFailed, optical_product_upper);
                                Interval _7957 = _12678;
                                Interval _12679 = idiv(_7953, _7957, intervalFailed, interval_divide_upper);
                                Interval _7947 = _12679;
                                Interval _7959 = _7947;
                                Interval _7960 = _7851;
                                Interval _12682 = imul(_7959, _7960, intervalFailed, optical_product_upper);
                                Interval _7958 = _12682;
                                Interval _7962 = _7947;
                                Interval _7963 = _7867;
                                Interval _12685 = imul(_7962, _7963, intervalFailed, optical_product_upper);
                                Interval _7961 = _12685;
                                bool _7964 = true;
                                if ((isunordered(_7848.lo, 0.63999998569488525390625) || _7848.lo > 0.63999998569488525390625))
                                {
                                    _7964 = _8341.hi >= 24.0;
                                }
                                if (_7964)
                                {
                                    Interval _7965 = _7942;
                                    float _7966 = 0.0;
                                    float _6584 = _7966;
                                    float _6585 = _7966;
                                    _6582.lo = _6584;
                                    _6582.hi = _6585;
                                    Interval _6583 = _6582;
                                    Interval _6586 = _6583;
                                    Interval _7967 = _6586;
                                    float _6579 = precise::min(_7965.lo, _7967.lo);
                                    float _6580 = precise::max(_7965.hi, _7967.hi);
                                    _6577.lo = _6579;
                                    _6577.hi = _6580;
                                    Interval _6578 = _6577;
                                    Interval _6581 = _6578;
                                    _7942 = _6581;
                                    Interval _7968 = _7958;
                                    float _7969 = 0.0;
                                    float _6574 = _7969;
                                    float _6575 = _7969;
                                    _6572.lo = _6574;
                                    _6572.hi = _6575;
                                    Interval _6573 = _6572;
                                    Interval _6576 = _6573;
                                    Interval _7970 = _6576;
                                    float _6569 = precise::min(_7968.lo, _7970.lo);
                                    float _6570 = precise::max(_7968.hi, _7970.hi);
                                    _6567.lo = _6569;
                                    _6567.hi = _6570;
                                    Interval _6568 = _6567;
                                    Interval _6571 = _6568;
                                    _7958 = _6571;
                                    Interval _7971 = _7961;
                                    float _7972 = 0.0;
                                    float _6564 = _7972;
                                    float _6565 = _7972;
                                    _6562.lo = _6564;
                                    _6562.hi = _6565;
                                    Interval _6563 = _6562;
                                    Interval _6566 = _6563;
                                    Interval _7973 = _6566;
                                    float _6559 = precise::min(_7971.lo, _7973.lo);
                                    float _6560 = precise::max(_7971.hi, _7973.hi);
                                    _6557.lo = _6559;
                                    _6557.hi = _6560;
                                    Interval _6558 = _6557;
                                    Interval _6561 = _6558;
                                    _7961 = _6561;
                                }
                                Interval _7974 = _7679;
                                Interval _7975 = _7942;
                                Interval _12780 = iadd(_7974, _7975, intervalFailed);
                                _7679 = _12780;
                                Interval _7976 = _7706;
                                Interval _7977 = _7958;
                                Interval _12783 = iadd(_7976, _7977, intervalFailed);
                                _7706 = _12783;
                                Interval _7978 = _7741;
                                Interval _7979 = _7961;
                                Interval _12786 = iadd(_7978, _7979, intervalFailed);
                                _7741 = _12786;
                            }
                        }
                    }
                    Interval _7980 = _7679;
                    Interval _7981 = _7706;
                    Interval _7982 = _7741;
                    _6555.x = _7980;
                    _6555.y = _7981;
                    _6555.z = _7982;
                    Interval3 _6556 = _6555;
                    Interval3 _7983 = _6556;
                    Interval3 _8337 = _7983;
                    if (_8336 == 0u)
                    {
                        Interval _8343 = param_var_distance;
                        float _8344 = 2.0;
                        float _8345 = 10.0;
                        Interval _12805 = iratio(_8344, _8345, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8346 = _12805;
                        Interval _12806 = imul(_8343, _8346, intervalFailed, optical_product_upper);
                        Interval _8342 = _12806;
                        Interval _8348 = _8337.x;
                        float _6552 = as_type<float>(as_type<uint>(_8348.hi) ^ 2147483648u);
                        float _6553 = as_type<float>(as_type<uint>(_8348.lo) ^ 2147483648u);
                        _6550.lo = _6552;
                        _6550.hi = _6553;
                        Interval _6551 = _6550;
                        Interval _6554 = _6551;
                        Interval _8349 = _6554;
                        Interval _8350 = _8333.y;
                        float _8351 = 12.0;
                        float _8352 = 100.0;
                        Interval _12828 = iratio(_8351, _8352, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8353 = _12828;
                        float _6547 = precise::max(_8350.lo, _8353.lo);
                        float _6548 = precise::max(_8350.hi, _8353.hi);
                        _6545.lo = _6547;
                        _6545.hi = _6548;
                        Interval _6546 = _6545;
                        Interval _6549 = _6546;
                        Interval _8354 = _6549;
                        Interval _12846 = idiv(_8349, _8354, intervalFailed, interval_divide_upper);
                        Interval _8355 = _12846;
                        Interval _8356 = _8342;
                        float _6542 = as_type<float>(as_type<uint>(_8356.hi) ^ 2147483648u);
                        float _6543 = as_type<float>(as_type<uint>(_8356.lo) ^ 2147483648u);
                        _6540.lo = _6542;
                        _6540.hi = _6543;
                        Interval _6541 = _6540;
                        Interval _6544 = _6541;
                        Interval _8357 = _6544;
                        Interval _8358 = _8342;
                        Interval _6535 = _8355;
                        Interval _6536 = _8357;
                        float _6532 = precise::max(_6535.lo, _6536.lo);
                        float _6533 = precise::max(_6535.hi, _6536.hi);
                        _6530.lo = _6532;
                        _6530.hi = _6533;
                        Interval _6531 = _6530;
                        Interval _6534 = _6531;
                        Interval _6537 = _6534;
                        Interval _6538 = _8358;
                        float _6527 = precise::min(_6537.lo, _6538.lo);
                        float _6528 = precise::min(_6537.hi, _6538.hi);
                        _6525.lo = _6527;
                        _6525.hi = _6528;
                        Interval _6526 = _6525;
                        Interval _6529 = _6526;
                        Interval _6539 = _6529;
                        Interval _8347 = _6539;
                        Interval3 _8359 = _8322;
                        Interval3 _8360 = _8333;
                        Interval _8361 = _8347;
                        Interval _6515 = _8360.x;
                        Interval _6516 = _8361;
                        Interval _12910 = imul(_6515, _6516, intervalFailed, optical_product_upper);
                        Interval _6517 = _12910;
                        Interval _6518 = _8360.y;
                        Interval _6519 = _8361;
                        Interval _12914 = imul(_6518, _6519, intervalFailed, optical_product_upper);
                        Interval _6520 = _12914;
                        Interval _6521 = _8360.z;
                        Interval _6522 = _8361;
                        Interval _12918 = imul(_6521, _6522, intervalFailed, optical_product_upper);
                        Interval _6523 = _12918;
                        _6513.x = _6517;
                        _6513.y = _6520;
                        _6513.z = _6523;
                        Interval3 _6514 = _6513;
                        Interval3 _6524 = _6514;
                        Interval3 _8362 = _6524;
                        Interval _6503 = _8359.x;
                        Interval _6504 = _8362.x;
                        Interval _12932 = iadd(_6503, _6504, intervalFailed);
                        Interval _6505 = _12932;
                        Interval _6506 = _8359.y;
                        Interval _6507 = _8362.y;
                        Interval _12937 = iadd(_6506, _6507, intervalFailed);
                        Interval _6508 = _12937;
                        Interval _6509 = _8359.z;
                        Interval _6510 = _8362.z;
                        Interval _12942 = iadd(_6509, _6510, intervalFailed);
                        Interval _6511 = _12942;
                        _6501.x = _6505;
                        _6501.y = _6508;
                        _6501.z = _6511;
                        Interval3 _6502 = _6501;
                        Interval3 _6512 = _6502;
                        _8322 = _6512;
                        Interval3 _8363 = hit;
                        Interval3 _8364 = param_var_direction_1;
                        Interval _8365 = _8347;
                        Interval _6491 = _8364.x;
                        Interval _6492 = _8365;
                        Interval _12958 = imul(_6491, _6492, intervalFailed, optical_product_upper);
                        Interval _6493 = _12958;
                        Interval _6494 = _8364.y;
                        Interval _6495 = _8365;
                        Interval _12962 = imul(_6494, _6495, intervalFailed, optical_product_upper);
                        Interval _6496 = _12962;
                        Interval _6497 = _8364.z;
                        Interval _6498 = _8365;
                        Interval _12966 = imul(_6497, _6498, intervalFailed, optical_product_upper);
                        Interval _6499 = _12966;
                        _6489.x = _6493;
                        _6489.y = _6496;
                        _6489.z = _6499;
                        Interval3 _6490 = _6489;
                        Interval3 _6500 = _6490;
                        Interval3 _8366 = _6500;
                        Interval _6479 = _8363.x;
                        Interval _6480 = _8366.x;
                        Interval _12980 = iadd(_6479, _6480, intervalFailed);
                        Interval _6481 = _12980;
                        Interval _6482 = _8363.y;
                        Interval _6483 = _8366.y;
                        Interval _12985 = iadd(_6482, _6483, intervalFailed);
                        Interval _6484 = _12985;
                        Interval _6485 = _8363.z;
                        Interval _6486 = _8366.z;
                        Interval _12990 = iadd(_6485, _6486, intervalFailed);
                        Interval _6487 = _12990;
                        _6477.x = _6481;
                        _6477.y = _6484;
                        _6477.z = _6487;
                        Interval3 _6478 = _6477;
                        Interval3 _6488 = _6478;
                        hit = _6488;
                    }
                    else
                    {
                        _8330 = _8337.y;
                        _8331 = _8337.z;
                    }
                }
            }
            else
            {
                Interval _8368 = _8322.x;
                float _8369 = 18.0;
                float _8370 = 1000.0;
                Interval _13007 = iratio(_8369, _8370, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8371 = _13007;
                Interval _13008 = imul(_8368, _8371, intervalFailed, optical_product_upper);
                Interval _8372 = _13008;
                Interval _8373 = _8322.z;
                float _8374 = 11.0;
                float _8375 = 1000.0;
                Interval _13011 = iratio(_8374, _8375, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8376 = _13011;
                Interval _13012 = imul(_8373, _8376, intervalFailed, optical_product_upper);
                Interval _8377 = _13012;
                Interval _13013 = iadd(_8372, _8377, intervalFailed);
                Interval _8378 = _13013;
                Interval _8379 = _8328;
                float _8380 = 8.0;
                float _8381 = 10.0;
                Interval _13015 = iratio(_8380, _8381, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8382 = _13015;
                Interval _13016 = imul(_8379, _8382, intervalFailed, optical_product_upper);
                Interval _8383 = _13016;
                Interval _6473 = _8378;
                Interval _6474 = _8383;
                float _6470 = as_type<float>(as_type<uint>(_6474.hi) ^ 2147483648u);
                float _6471 = as_type<float>(as_type<uint>(_6474.lo) ^ 2147483648u);
                _6468.lo = _6470;
                _6468.hi = _6471;
                Interval _6469 = _6468;
                Interval _6472 = _6469;
                Interval _6475 = _6472;
                Interval _13036 = iadd(_6473, _6475, intervalFailed);
                Interval _6476 = _13036;
                Interval _8367 = _6476;
                Interval _8385 = _8322.x;
                float _8386 = 47.0;
                float _8387 = 1000.0;
                Interval _13040 = iratio(_8386, _8387, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8388 = _13040;
                Interval _13041 = imul(_8385, _8388, intervalFailed, optical_product_upper);
                Interval _8389 = _13041;
                Interval _8390 = _8322.z;
                float _8391 = 25.0;
                float _8392 = 1000.0;
                Interval _13044 = iratio(_8391, _8392, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8393 = _13044;
                Interval _13045 = imul(_8390, _8393, intervalFailed, optical_product_upper);
                Interval _8394 = _13045;
                Interval _6464 = _8389;
                Interval _6465 = _8394;
                float _6461 = as_type<float>(as_type<uint>(_6465.hi) ^ 2147483648u);
                float _6462 = as_type<float>(as_type<uint>(_6465.lo) ^ 2147483648u);
                _6459.lo = _6461;
                _6459.hi = _6462;
                Interval _6460 = _6459;
                Interval _6463 = _6460;
                Interval _6466 = _6463;
                Interval _13065 = iadd(_6464, _6466, intervalFailed);
                Interval _6467 = _13065;
                Interval _8395 = _6467;
                Interval _8396 = _8328;
                float _8397 = 12.0;
                float _8398 = 10.0;
                Interval _13068 = iratio(_8397, _8398, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8399 = _13068;
                Interval _13069 = imul(_8396, _8399, intervalFailed, optical_product_upper);
                Interval _8400 = _13069;
                Interval _13070 = iadd(_8395, _8400, intervalFailed);
                Interval _8384 = _13070;
                Interval _8402 = _8322.z;
                float _8403 = 22.0;
                float _8404 = 1000.0;
                Interval _13073 = iratio(_8403, _8404, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8405 = _13073;
                Interval _13074 = imul(_8402, _8405, intervalFailed, optical_product_upper);
                Interval _8406 = _13074;
                Interval _8407 = _8322.x;
                float _8408 = 9.0;
                float _8409 = 1000.0;
                Interval _13077 = iratio(_8408, _8409, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8410 = _13077;
                Interval _13078 = imul(_8407, _8410, intervalFailed, optical_product_upper);
                Interval _8411 = _13078;
                Interval _6455 = _8406;
                Interval _6456 = _8411;
                float _6452 = as_type<float>(as_type<uint>(_6456.hi) ^ 2147483648u);
                float _6453 = as_type<float>(as_type<uint>(_6456.lo) ^ 2147483648u);
                _6450.lo = _6452;
                _6450.hi = _6453;
                Interval _6451 = _6450;
                Interval _6454 = _6451;
                Interval _6457 = _6454;
                Interval _13098 = iadd(_6455, _6457, intervalFailed);
                Interval _6458 = _13098;
                Interval _8412 = _6458;
                Interval _8413 = _8328;
                float _8414 = 65.0;
                float _8415 = 100.0;
                Interval _13101 = iratio(_8414, _8415, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8416 = _13101;
                Interval _13102 = imul(_8413, _8416, intervalFailed, optical_product_upper);
                Interval _8417 = _13102;
                Interval _6446 = _8412;
                Interval _6447 = _8417;
                float _6443 = as_type<float>(as_type<uint>(_6447.hi) ^ 2147483648u);
                float _6444 = as_type<float>(as_type<uint>(_6447.lo) ^ 2147483648u);
                _6441.lo = _6443;
                _6441.hi = _6444;
                Interval _6442 = _6441;
                Interval _6445 = _6442;
                Interval _6448 = _6445;
                Interval _13122 = iadd(_6446, _6448, intervalFailed);
                Interval _6449 = _13122;
                Interval _8401 = _6449;
                float _8418 = 55.0;
                float _8419 = 1000.0;
                Interval _13124 = iratio(_8418, _8419, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8420 = _13124;
                Interval _8421 = _8367;
                Interval _6434 = _8421;
                float _6432 = 3.1415927410125732421875;
                float _6427 = _6432;
                float _13128 = interval_down(_6427, intervalFailed);
                float _6428 = _13128;
                float _6429 = _6432;
                float _13130 = interval_up(_6429, intervalFailed);
                float _6430 = _13130;
                _6425.lo = _6428;
                _6425.hi = _6430;
                Interval _6426 = _6425;
                Interval _6431 = _6426;
                Interval _6433 = _6431;
                Interval _6435 = _6433;
                float _6436 = 0.5;
                float _6422 = _6436;
                float _6423 = _6436;
                _6420.lo = _6422;
                _6420.hi = _6423;
                Interval _6421 = _6420;
                Interval _6424 = _6421;
                Interval _6437 = _6424;
                Interval _13148 = imul(_6435, _6437, intervalFailed, optical_product_upper);
                Interval _6438 = _13148;
                Interval _13149 = iadd(_6434, _6438, intervalFailed);
                Interval _6439 = _13149;
                float _6415 = _6439.lo;
                float _6416 = _6439.hi;
                float _6410 = _6415;
                float _6411 = _6416;
                _6407.lo = _6410;
                _6407.hi = _6411;
                Interval _6408 = _6407;
                Interval _6412 = _6408;
                Interval _13162 = isin_body(_6412, intervalFailed, optical_product_upper);
                Interval _6409 = _13162;
                interval_sine_upper = _6409.hi;
                float _6413 = _6409.lo;
                float _6414 = _6413;
                float _6417 = _6414;
                float _6418 = interval_sine_upper;
                _6405.lo = _6417;
                _6405.hi = _6418;
                Interval _6406 = _6405;
                Interval _6419 = _6406;
                Interval _6440 = _6419;
                Interval _8422 = _6440;
                Interval _13178 = imul(_8420, _8422, intervalFailed, optical_product_upper);
                Interval _8423 = _13178;
                Interval _8424 = param_var_footprint;
                float _8425 = 22.0;
                float _8426 = 1000.0;
                Interval _13180 = iratio(_8425, _8426, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8427 = _13180;
                Interval _6394 = _8424;
                Interval _6395 = _8427;
                Interval _13183 = imul(_6394, _6395, intervalFailed, optical_product_upper);
                Interval _6396 = _13183;
                bool _6386 = false;
                if (_6396.lo <= 0.0)
                {
                    _6386 = _6396.hi >= 0.0;
                }
                if (_6386)
                {
                    _6385 = 0.0;
                }
                else
                {
                    _6385 = precise::min(abs(_6396.lo), abs(_6396.hi));
                }
                float _6384 = _6385;
                float _6387 = precise::max(abs(_6396.lo), abs(_6396.hi));
                float _6388 = spvFMul(_6384, _6384);
                float _13213 = interval_down(_6388, intervalFailed);
                float _6389 = precise::max(0.0, _13213);
                float _6390 = spvFMul(_6387, _6387);
                float _13217 = interval_up(_6390, intervalFailed);
                float _6391 = _13217;
                _6382.lo = _6389;
                _6382.hi = _6391;
                Interval _6383 = _6382;
                Interval _6392 = _6383;
                Interval _6393 = _6392;
                float _6397 = 1.0;
                float _6379 = _6397;
                float _6380 = _6397;
                _6377.lo = _6379;
                _6377.hi = _6380;
                Interval _6378 = _6377;
                Interval _6381 = _6378;
                Interval _6398 = _6381;
                float _6399 = 1.0;
                float _6374 = _6399;
                float _6375 = _6399;
                _6372.lo = _6374;
                _6372.hi = _6375;
                Interval _6373 = _6372;
                Interval _6376 = _6373;
                Interval _6400 = _6376;
                Interval _6401 = _6393;
                bool _6365 = false;
                if (_6401.lo <= 0.0)
                {
                    _6365 = _6401.hi >= 0.0;
                }
                if (_6365)
                {
                    _6364 = 0.0;
                }
                else
                {
                    _6364 = precise::min(abs(_6401.lo), abs(_6401.hi));
                }
                float _6363 = _6364;
                float _6366 = precise::max(abs(_6401.lo), abs(_6401.hi));
                float _6367 = spvFMul(_6363, _6363);
                float _13273 = interval_down(_6367, intervalFailed);
                float _6368 = precise::max(0.0, _13273);
                float _6369 = spvFMul(_6366, _6366);
                float _13277 = interval_up(_6369, intervalFailed);
                float _6370 = _13277;
                _6361.lo = _6368;
                _6361.hi = _6370;
                Interval _6362 = _6361;
                Interval _6371 = _6362;
                Interval _6402 = _6371;
                Interval _13285 = iadd(_6400, _6402, intervalFailed);
                Interval _6403 = _13285;
                Interval _13286 = idiv(_6398, _6403, intervalFailed, interval_divide_upper);
                Interval _6404 = _13286;
                Interval _8428 = _6404;
                Interval _13288 = imul(_8423, _8428, intervalFailed, optical_product_upper);
                Interval _8429 = _13288;
                float _8430 = 25.0;
                float _8431 = 1000.0;
                Interval _13289 = iratio(_8430, _8431, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8432 = _13289;
                Interval _8433 = _8384;
                Interval _6354 = _8433;
                float _6352 = 3.1415927410125732421875;
                float _6347 = _6352;
                float _13293 = interval_down(_6347, intervalFailed);
                float _6348 = _13293;
                float _6349 = _6352;
                float _13295 = interval_up(_6349, intervalFailed);
                float _6350 = _13295;
                _6345.lo = _6348;
                _6345.hi = _6350;
                Interval _6346 = _6345;
                Interval _6351 = _6346;
                Interval _6353 = _6351;
                Interval _6355 = _6353;
                float _6356 = 0.5;
                float _6342 = _6356;
                float _6343 = _6356;
                _6340.lo = _6342;
                _6340.hi = _6343;
                Interval _6341 = _6340;
                Interval _6344 = _6341;
                Interval _6357 = _6344;
                Interval _13313 = imul(_6355, _6357, intervalFailed, optical_product_upper);
                Interval _6358 = _13313;
                Interval _13314 = iadd(_6354, _6358, intervalFailed);
                Interval _6359 = _13314;
                float _6335 = _6359.lo;
                float _6336 = _6359.hi;
                float _6330 = _6335;
                float _6331 = _6336;
                _6327.lo = _6330;
                _6327.hi = _6331;
                Interval _6328 = _6327;
                Interval _6332 = _6328;
                Interval _13327 = isin_body(_6332, intervalFailed, optical_product_upper);
                Interval _6329 = _13327;
                interval_sine_upper = _6329.hi;
                float _6333 = _6329.lo;
                float _6334 = _6333;
                float _6337 = _6334;
                float _6338 = interval_sine_upper;
                _6325.lo = _6337;
                _6325.hi = _6338;
                Interval _6326 = _6325;
                Interval _6339 = _6326;
                Interval _6360 = _6339;
                Interval _8434 = _6360;
                Interval _13343 = imul(_8432, _8434, intervalFailed, optical_product_upper);
                Interval _8435 = _13343;
                Interval _8436 = param_var_footprint;
                float _8437 = 54.0;
                float _8438 = 1000.0;
                Interval _13345 = iratio(_8437, _8438, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8439 = _13345;
                Interval _6314 = _8436;
                Interval _6315 = _8439;
                Interval _13348 = imul(_6314, _6315, intervalFailed, optical_product_upper);
                Interval _6316 = _13348;
                bool _6306 = false;
                if (_6316.lo <= 0.0)
                {
                    _6306 = _6316.hi >= 0.0;
                }
                if (_6306)
                {
                    _6305 = 0.0;
                }
                else
                {
                    _6305 = precise::min(abs(_6316.lo), abs(_6316.hi));
                }
                float _6304 = _6305;
                float _6307 = precise::max(abs(_6316.lo), abs(_6316.hi));
                float _6308 = spvFMul(_6304, _6304);
                float _13378 = interval_down(_6308, intervalFailed);
                float _6309 = precise::max(0.0, _13378);
                float _6310 = spvFMul(_6307, _6307);
                float _13382 = interval_up(_6310, intervalFailed);
                float _6311 = _13382;
                _6302.lo = _6309;
                _6302.hi = _6311;
                Interval _6303 = _6302;
                Interval _6312 = _6303;
                Interval _6313 = _6312;
                float _6317 = 1.0;
                float _6299 = _6317;
                float _6300 = _6317;
                _6297.lo = _6299;
                _6297.hi = _6300;
                Interval _6298 = _6297;
                Interval _6301 = _6298;
                Interval _6318 = _6301;
                float _6319 = 1.0;
                float _6294 = _6319;
                float _6295 = _6319;
                _6292.lo = _6294;
                _6292.hi = _6295;
                Interval _6293 = _6292;
                Interval _6296 = _6293;
                Interval _6320 = _6296;
                Interval _6321 = _6313;
                bool _6285 = false;
                if (_6321.lo <= 0.0)
                {
                    _6285 = _6321.hi >= 0.0;
                }
                if (_6285)
                {
                    _6284 = 0.0;
                }
                else
                {
                    _6284 = precise::min(abs(_6321.lo), abs(_6321.hi));
                }
                float _6283 = _6284;
                float _6286 = precise::max(abs(_6321.lo), abs(_6321.hi));
                float _6287 = spvFMul(_6283, _6283);
                float _13438 = interval_down(_6287, intervalFailed);
                float _6288 = precise::max(0.0, _13438);
                float _6289 = spvFMul(_6286, _6286);
                float _13442 = interval_up(_6289, intervalFailed);
                float _6290 = _13442;
                _6281.lo = _6288;
                _6281.hi = _6290;
                Interval _6282 = _6281;
                Interval _6291 = _6282;
                Interval _6322 = _6291;
                Interval _13450 = iadd(_6320, _6322, intervalFailed);
                Interval _6323 = _13450;
                Interval _13451 = idiv(_6318, _6323, intervalFailed, interval_divide_upper);
                Interval _6324 = _13451;
                Interval _8440 = _6324;
                Interval _13453 = imul(_8435, _8440, intervalFailed, optical_product_upper);
                Interval _8441 = _13453;
                Interval _13454 = iadd(_8429, _8441, intervalFailed);
                _8330 = _13454;
                float _8442 = 45.0;
                float _8443 = 1000.0;
                Interval _13455 = iratio(_8442, _8443, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8444 = _13455;
                Interval _8445 = _8401;
                Interval _6274 = _8445;
                float _6272 = 3.1415927410125732421875;
                float _6267 = _6272;
                float _13459 = interval_down(_6267, intervalFailed);
                float _6268 = _13459;
                float _6269 = _6272;
                float _13461 = interval_up(_6269, intervalFailed);
                float _6270 = _13461;
                _6265.lo = _6268;
                _6265.hi = _6270;
                Interval _6266 = _6265;
                Interval _6271 = _6266;
                Interval _6273 = _6271;
                Interval _6275 = _6273;
                float _6276 = 0.5;
                float _6262 = _6276;
                float _6263 = _6276;
                _6260.lo = _6262;
                _6260.hi = _6263;
                Interval _6261 = _6260;
                Interval _6264 = _6261;
                Interval _6277 = _6264;
                Interval _13479 = imul(_6275, _6277, intervalFailed, optical_product_upper);
                Interval _6278 = _13479;
                Interval _13480 = iadd(_6274, _6278, intervalFailed);
                Interval _6279 = _13480;
                float _6255 = _6279.lo;
                float _6256 = _6279.hi;
                float _6250 = _6255;
                float _6251 = _6256;
                _6247.lo = _6250;
                _6247.hi = _6251;
                Interval _6248 = _6247;
                Interval _6252 = _6248;
                Interval _13493 = isin_body(_6252, intervalFailed, optical_product_upper);
                Interval _6249 = _13493;
                interval_sine_upper = _6249.hi;
                float _6253 = _6249.lo;
                float _6254 = _6253;
                float _6257 = _6254;
                float _6258 = interval_sine_upper;
                _6245.lo = _6257;
                _6245.hi = _6258;
                Interval _6246 = _6245;
                Interval _6259 = _6246;
                Interval _6280 = _6259;
                Interval _8446 = _6280;
                Interval _13509 = imul(_8444, _8446, intervalFailed, optical_product_upper);
                Interval _8447 = _13509;
                Interval _8448 = param_var_footprint;
                float _8449 = 24.0;
                float _8450 = 1000.0;
                Interval _13511 = iratio(_8449, _8450, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8451 = _13511;
                Interval _6234 = _8448;
                Interval _6235 = _8451;
                Interval _13514 = imul(_6234, _6235, intervalFailed, optical_product_upper);
                Interval _6236 = _13514;
                bool _6226 = false;
                if (_6236.lo <= 0.0)
                {
                    _6226 = _6236.hi >= 0.0;
                }
                if (_6226)
                {
                    _6225 = 0.0;
                }
                else
                {
                    _6225 = precise::min(abs(_6236.lo), abs(_6236.hi));
                }
                float _6224 = _6225;
                float _6227 = precise::max(abs(_6236.lo), abs(_6236.hi));
                float _6228 = spvFMul(_6224, _6224);
                float _13544 = interval_down(_6228, intervalFailed);
                float _6229 = precise::max(0.0, _13544);
                float _6230 = spvFMul(_6227, _6227);
                float _13548 = interval_up(_6230, intervalFailed);
                float _6231 = _13548;
                _6222.lo = _6229;
                _6222.hi = _6231;
                Interval _6223 = _6222;
                Interval _6232 = _6223;
                Interval _6233 = _6232;
                float _6237 = 1.0;
                float _6219 = _6237;
                float _6220 = _6237;
                _6217.lo = _6219;
                _6217.hi = _6220;
                Interval _6218 = _6217;
                Interval _6221 = _6218;
                Interval _6238 = _6221;
                float _6239 = 1.0;
                float _6214 = _6239;
                float _6215 = _6239;
                _6212.lo = _6214;
                _6212.hi = _6215;
                Interval _6213 = _6212;
                Interval _6216 = _6213;
                Interval _6240 = _6216;
                Interval _6241 = _6233;
                bool _6205 = false;
                if (_6241.lo <= 0.0)
                {
                    _6205 = _6241.hi >= 0.0;
                }
                if (_6205)
                {
                    _6204 = 0.0;
                }
                else
                {
                    _6204 = precise::min(abs(_6241.lo), abs(_6241.hi));
                }
                float _6203 = _6204;
                float _6206 = precise::max(abs(_6241.lo), abs(_6241.hi));
                float _6207 = spvFMul(_6203, _6203);
                float _13604 = interval_down(_6207, intervalFailed);
                float _6208 = precise::max(0.0, _13604);
                float _6209 = spvFMul(_6206, _6206);
                float _13608 = interval_up(_6209, intervalFailed);
                float _6210 = _13608;
                _6201.lo = _6208;
                _6201.hi = _6210;
                Interval _6202 = _6201;
                Interval _6211 = _6202;
                Interval _6242 = _6211;
                Interval _13616 = iadd(_6240, _6242, intervalFailed);
                Interval _6243 = _13616;
                Interval _13617 = idiv(_6238, _6243, intervalFailed, interval_divide_upper);
                Interval _6244 = _13617;
                Interval _8452 = _6244;
                Interval _13619 = imul(_8447, _8452, intervalFailed, optical_product_upper);
                Interval _8453 = _13619;
                float _8454 = 20.0;
                float _8455 = 1000.0;
                Interval _13620 = iratio(_8454, _8455, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8456 = _13620;
                Interval _8457 = _8384;
                Interval _6194 = _8457;
                float _6192 = 3.1415927410125732421875;
                float _6187 = _6192;
                float _13624 = interval_down(_6187, intervalFailed);
                float _6188 = _13624;
                float _6189 = _6192;
                float _13626 = interval_up(_6189, intervalFailed);
                float _6190 = _13626;
                _6185.lo = _6188;
                _6185.hi = _6190;
                Interval _6186 = _6185;
                Interval _6191 = _6186;
                Interval _6193 = _6191;
                Interval _6195 = _6193;
                float _6196 = 0.5;
                float _6182 = _6196;
                float _6183 = _6196;
                _6180.lo = _6182;
                _6180.hi = _6183;
                Interval _6181 = _6180;
                Interval _6184 = _6181;
                Interval _6197 = _6184;
                Interval _13644 = imul(_6195, _6197, intervalFailed, optical_product_upper);
                Interval _6198 = _13644;
                Interval _13645 = iadd(_6194, _6198, intervalFailed);
                Interval _6199 = _13645;
                float _6175 = _6199.lo;
                float _6176 = _6199.hi;
                float _6170 = _6175;
                float _6171 = _6176;
                _6167.lo = _6170;
                _6167.hi = _6171;
                Interval _6168 = _6167;
                Interval _6172 = _6168;
                Interval _13658 = isin_body(_6172, intervalFailed, optical_product_upper);
                Interval _6169 = _13658;
                interval_sine_upper = _6169.hi;
                float _6173 = _6169.lo;
                float _6174 = _6173;
                float _6177 = _6174;
                float _6178 = interval_sine_upper;
                _6165.lo = _6177;
                _6165.hi = _6178;
                Interval _6166 = _6165;
                Interval _6179 = _6166;
                Interval _6200 = _6179;
                Interval _8458 = _6200;
                Interval _13674 = imul(_8456, _8458, intervalFailed, optical_product_upper);
                Interval _8459 = _13674;
                Interval _8460 = param_var_footprint;
                float _8461 = 54.0;
                float _8462 = 1000.0;
                Interval _13676 = iratio(_8461, _8462, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8463 = _13676;
                Interval _6154 = _8460;
                Interval _6155 = _8463;
                Interval _13679 = imul(_6154, _6155, intervalFailed, optical_product_upper);
                Interval _6156 = _13679;
                bool _6146 = false;
                if (_6156.lo <= 0.0)
                {
                    _6146 = _6156.hi >= 0.0;
                }
                if (_6146)
                {
                    _6145 = 0.0;
                }
                else
                {
                    _6145 = precise::min(abs(_6156.lo), abs(_6156.hi));
                }
                float _6144 = _6145;
                float _6147 = precise::max(abs(_6156.lo), abs(_6156.hi));
                float _6148 = spvFMul(_6144, _6144);
                float _13709 = interval_down(_6148, intervalFailed);
                float _6149 = precise::max(0.0, _13709);
                float _6150 = spvFMul(_6147, _6147);
                float _13713 = interval_up(_6150, intervalFailed);
                float _6151 = _13713;
                _6142.lo = _6149;
                _6142.hi = _6151;
                Interval _6143 = _6142;
                Interval _6152 = _6143;
                Interval _6153 = _6152;
                float _6157 = 1.0;
                float _6139 = _6157;
                float _6140 = _6157;
                _6137.lo = _6139;
                _6137.hi = _6140;
                Interval _6138 = _6137;
                Interval _6141 = _6138;
                Interval _6158 = _6141;
                float _6159 = 1.0;
                float _6134 = _6159;
                float _6135 = _6159;
                _6132.lo = _6134;
                _6132.hi = _6135;
                Interval _6133 = _6132;
                Interval _6136 = _6133;
                Interval _6160 = _6136;
                Interval _6161 = _6153;
                bool _6125 = false;
                if (_6161.lo <= 0.0)
                {
                    _6125 = _6161.hi >= 0.0;
                }
                if (_6125)
                {
                    _6124 = 0.0;
                }
                else
                {
                    _6124 = precise::min(abs(_6161.lo), abs(_6161.hi));
                }
                float _6123 = _6124;
                float _6126 = precise::max(abs(_6161.lo), abs(_6161.hi));
                float _6127 = spvFMul(_6123, _6123);
                float _13769 = interval_down(_6127, intervalFailed);
                float _6128 = precise::max(0.0, _13769);
                float _6129 = spvFMul(_6126, _6126);
                float _13773 = interval_up(_6129, intervalFailed);
                float _6130 = _13773;
                _6121.lo = _6128;
                _6121.hi = _6130;
                Interval _6122 = _6121;
                Interval _6131 = _6122;
                Interval _6162 = _6131;
                Interval _13781 = iadd(_6160, _6162, intervalFailed);
                Interval _6163 = _13781;
                Interval _13782 = idiv(_6158, _6163, intervalFailed, interval_divide_upper);
                Interval _6164 = _13782;
                Interval _8464 = _6164;
                Interval _13784 = imul(_8459, _8464, intervalFailed, optical_product_upper);
                Interval _8465 = _13784;
                Interval _6117 = _8453;
                Interval _6118 = _8465;
                float _6114 = as_type<float>(as_type<uint>(_6118.hi) ^ 2147483648u);
                float _6115 = as_type<float>(as_type<uint>(_6118.lo) ^ 2147483648u);
                _6112.lo = _6114;
                _6112.hi = _6115;
                Interval _6113 = _6112;
                Interval _6116 = _6113;
                Interval _6119 = _6116;
                Interval _13804 = iadd(_6117, _6119, intervalFailed);
                Interval _6120 = _13804;
                _8331 = _6120;
            }
            Interval _8466 = _8330;
            float _8467 = -1.0;
            float _6109 = _8467;
            float _6110 = _8467;
            _6107.lo = _6109;
            _6107.hi = _6110;
            Interval _6108 = _6107;
            Interval _6111 = _6108;
            Interval _8468 = _6111;
            Interval _8469 = _8331;
            _6105.x = _8466;
            _6105.y = _8468;
            _6105.z = _8469;
            Interval3 _6106 = _6105;
            Interval3 _8470 = _6106;
            ReflectionLiquidFrame _8471 = param_var_f;
            float3 _6092 = _8471.rotation0.xyz;
            float _6085 = _6092.x;
            float _6082 = _6085;
            float _6083 = _6085;
            _6080.lo = _6082;
            _6080.hi = _6083;
            Interval _6081 = _6080;
            Interval _6084 = _6081;
            Interval _6086 = _6084;
            float _6087 = _6092.y;
            float _6077 = _6087;
            float _6078 = _6087;
            _6075.lo = _6077;
            _6075.hi = _6078;
            Interval _6076 = _6075;
            Interval _6079 = _6076;
            Interval _6088 = _6079;
            float _6089 = _6092.z;
            float _6072 = _6089;
            float _6073 = _6089;
            _6070.lo = _6072;
            _6070.hi = _6073;
            Interval _6071 = _6070;
            Interval _6074 = _6071;
            Interval _6090 = _6074;
            _6068.x = _6086;
            _6068.y = _6088;
            _6068.z = _6090;
            Interval3 _6069 = _6068;
            Interval3 _6091 = _6069;
            Interval3 _6093 = _6091;
            Interval3 _6094 = _8470;
            Interval _6057 = _6093.x;
            Interval _6058 = _6094.x;
            Interval _13876 = imul(_6057, _6058, intervalFailed, optical_product_upper);
            Interval _6059 = _13876;
            Interval _6060 = _6093.y;
            Interval _6061 = _6094.y;
            Interval _13881 = imul(_6060, _6061, intervalFailed, optical_product_upper);
            Interval _6062 = _13881;
            Interval _13882 = iadd(_6059, _6062, intervalFailed);
            Interval _6063 = _13882;
            Interval _6064 = _6093.z;
            Interval _6065 = _6094.z;
            Interval _13887 = imul(_6064, _6065, intervalFailed, optical_product_upper);
            Interval _6066 = _13887;
            Interval _13888 = iadd(_6063, _6066, intervalFailed);
            Interval _6067 = _13888;
            Interval _6095 = _6067;
            float3 _6096 = _8471.rotation1.xyz;
            float _6050 = _6096.x;
            float _6047 = _6050;
            float _6048 = _6050;
            _6045.lo = _6047;
            _6045.hi = _6048;
            Interval _6046 = _6045;
            Interval _6049 = _6046;
            Interval _6051 = _6049;
            float _6052 = _6096.y;
            float _6042 = _6052;
            float _6043 = _6052;
            _6040.lo = _6042;
            _6040.hi = _6043;
            Interval _6041 = _6040;
            Interval _6044 = _6041;
            Interval _6053 = _6044;
            float _6054 = _6096.z;
            float _6037 = _6054;
            float _6038 = _6054;
            _6035.lo = _6037;
            _6035.hi = _6038;
            Interval _6036 = _6035;
            Interval _6039 = _6036;
            Interval _6055 = _6039;
            _6033.x = _6051;
            _6033.y = _6053;
            _6033.z = _6055;
            Interval3 _6034 = _6033;
            Interval3 _6056 = _6034;
            Interval3 _6097 = _6056;
            Interval3 _6098 = _8470;
            Interval _6022 = _6097.x;
            Interval _6023 = _6098.x;
            Interval _13940 = imul(_6022, _6023, intervalFailed, optical_product_upper);
            Interval _6024 = _13940;
            Interval _6025 = _6097.y;
            Interval _6026 = _6098.y;
            Interval _13945 = imul(_6025, _6026, intervalFailed, optical_product_upper);
            Interval _6027 = _13945;
            Interval _13946 = iadd(_6024, _6027, intervalFailed);
            Interval _6028 = _13946;
            Interval _6029 = _6097.z;
            Interval _6030 = _6098.z;
            Interval _13951 = imul(_6029, _6030, intervalFailed, optical_product_upper);
            Interval _6031 = _13951;
            Interval _13952 = iadd(_6028, _6031, intervalFailed);
            Interval _6032 = _13952;
            Interval _6099 = _6032;
            float3 _6100 = _8471.rotation2.xyz;
            float _6015 = _6100.x;
            float _6012 = _6015;
            float _6013 = _6015;
            _6010.lo = _6012;
            _6010.hi = _6013;
            Interval _6011 = _6010;
            Interval _6014 = _6011;
            Interval _6016 = _6014;
            float _6017 = _6100.y;
            float _6007 = _6017;
            float _6008 = _6017;
            _6005.lo = _6007;
            _6005.hi = _6008;
            Interval _6006 = _6005;
            Interval _6009 = _6006;
            Interval _6018 = _6009;
            float _6019 = _6100.z;
            float _6002 = _6019;
            float _6003 = _6019;
            _6000.lo = _6002;
            _6000.hi = _6003;
            Interval _6001 = _6000;
            Interval _6004 = _6001;
            Interval _6020 = _6004;
            _5998.x = _6016;
            _5998.y = _6018;
            _5998.z = _6020;
            Interval3 _5999 = _5998;
            Interval3 _6021 = _5999;
            Interval3 _6101 = _6021;
            Interval3 _6102 = _8470;
            Interval _5987 = _6101.x;
            Interval _5988 = _6102.x;
            Interval _14004 = imul(_5987, _5988, intervalFailed, optical_product_upper);
            Interval _5989 = _14004;
            Interval _5990 = _6101.y;
            Interval _5991 = _6102.y;
            Interval _14009 = imul(_5990, _5991, intervalFailed, optical_product_upper);
            Interval _5992 = _14009;
            Interval _14010 = iadd(_5989, _5992, intervalFailed);
            Interval _5993 = _14010;
            Interval _5994 = _6101.z;
            Interval _5995 = _6102.z;
            Interval _14015 = imul(_5994, _5995, intervalFailed, optical_product_upper);
            Interval _5996 = _14015;
            Interval _14016 = iadd(_5993, _5996, intervalFailed);
            Interval _5997 = _14016;
            Interval _6103 = _5997;
            _5985.x = _6095;
            _5985.y = _6099;
            _5985.z = _6103;
            Interval3 _5986 = _5985;
            Interval3 _6104 = _5986;
            Interval3 _8472 = _6104;
            Interval3 _5978 = _8472;
            float _5979 = 1.0;
            float _5975 = _5979;
            float _5976 = _5979;
            _5973.lo = _5975;
            _5973.hi = _5976;
            Interval _5974 = _5973;
            Interval _5977 = _5974;
            Interval _5980 = _5977;
            Interval3 _5981 = _8472;
            Interval _5964 = _5981.x;
            bool _5957 = false;
            if (_5964.lo <= 0.0)
            {
                _5957 = _5964.hi >= 0.0;
            }
            if (_5957)
            {
                _5956 = 0.0;
            }
            else
            {
                _5956 = precise::min(abs(_5964.lo), abs(_5964.hi));
            }
            float _5955 = _5956;
            float _5958 = precise::max(abs(_5964.lo), abs(_5964.hi));
            float _5959 = spvFMul(_5955, _5955);
            float _14069 = interval_down(_5959, intervalFailed);
            float _5960 = precise::max(0.0, _14069);
            float _5961 = spvFMul(_5958, _5958);
            float _14073 = interval_up(_5961, intervalFailed);
            float _5962 = _14073;
            _5953.lo = _5960;
            _5953.hi = _5962;
            Interval _5954 = _5953;
            Interval _5963 = _5954;
            Interval _5965 = _5963;
            Interval _5966 = _5981.y;
            bool _5946 = false;
            if (_5966.lo <= 0.0)
            {
                _5946 = _5966.hi >= 0.0;
            }
            if (_5946)
            {
                _5945 = 0.0;
            }
            else
            {
                _5945 = precise::min(abs(_5966.lo), abs(_5966.hi));
            }
            float _5944 = _5945;
            float _5947 = precise::max(abs(_5966.lo), abs(_5966.hi));
            float _5948 = spvFMul(_5944, _5944);
            float _14112 = interval_down(_5948, intervalFailed);
            float _5949 = precise::max(0.0, _14112);
            float _5950 = spvFMul(_5947, _5947);
            float _14116 = interval_up(_5950, intervalFailed);
            float _5951 = _14116;
            _5942.lo = _5949;
            _5942.hi = _5951;
            Interval _5943 = _5942;
            Interval _5952 = _5943;
            Interval _5967 = _5952;
            Interval _14124 = iadd(_5965, _5967, intervalFailed);
            Interval _5968 = _14124;
            Interval _5969 = _5981.z;
            bool _5935 = false;
            if (_5969.lo <= 0.0)
            {
                _5935 = _5969.hi >= 0.0;
            }
            if (_5935)
            {
                _5934 = 0.0;
            }
            else
            {
                _5934 = precise::min(abs(_5969.lo), abs(_5969.hi));
            }
            float _5933 = _5934;
            float _5936 = precise::max(abs(_5969.lo), abs(_5969.hi));
            float _5937 = spvFMul(_5933, _5933);
            float _14156 = interval_down(_5937, intervalFailed);
            float _5938 = precise::max(0.0, _14156);
            float _5939 = spvFMul(_5936, _5936);
            float _14160 = interval_up(_5939, intervalFailed);
            float _5940 = _14160;
            _5931.lo = _5938;
            _5931.hi = _5940;
            Interval _5932 = _5931;
            Interval _5941 = _5932;
            Interval _5970 = _5941;
            Interval _14168 = iadd(_5968, _5970, intervalFailed);
            Interval _5971 = _14168;
            Interval _14169 = isqrt(_5971, intervalFailed);
            Interval _5972 = _14169;
            Interval _5982 = _5972;
            Interval _14171 = idiv(_5980, _5982, intervalFailed, interval_divide_upper);
            Interval _5983 = _14171;
            Interval _5921 = _5978.x;
            Interval _5922 = _5983;
            Interval _14175 = imul(_5921, _5922, intervalFailed, optical_product_upper);
            Interval _5923 = _14175;
            Interval _5924 = _5978.y;
            Interval _5925 = _5983;
            Interval _14179 = imul(_5924, _5925, intervalFailed, optical_product_upper);
            Interval _5926 = _14179;
            Interval _5927 = _5978.z;
            Interval _5928 = _5983;
            Interval _14183 = imul(_5927, _5928, intervalFailed, optical_product_upper);
            Interval _5929 = _14183;
            _5919.x = _5923;
            _5919.y = _5926;
            _5919.z = _5929;
            Interval3 _5920 = _5919;
            Interval3 _5930 = _5920;
            Interval3 _5984 = _5930;
            Interval3 _8473 = _5984;
            Interval3 _8474 = param_var_direction_1;
            Interval3 _14195 = oriented(_8473, _8474, intervalFailed, optical_product_upper);
            Interval3 _8475 = _14195;
            n = _8475;
        }
        else
        {
            ReflectionSpecularPlane param_var_p_1 = plane;
            Interval3 param_var_hit = hit;
            bool _14199 = outside_face(param_var_p_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (_14199)
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
            bool _5916 = false;
            if (param_var_p_2.a.w == 2.0)
            {
                _5916 = param_var_p_2.b.w == 2.0;
            }
            bool _5917 = false;
            if (_5916)
            {
                _5917 = param_var_p_2.c.w == 2.0;
            }
            bool _5918 = _5917;
            temp_var_logical_4 = !_5918;
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
        Interval _14230 = iratio(param_var_n_2, param_var_d_1, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_a_17 = _14230;
        Interval param_var_a_18 = _distance;
        float param_var_n_3 = 1.0;
        float param_var_d_2 = 100000.0;
        Interval _14232 = iratio(param_var_n_3, param_var_d_2, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_b_14 = _14232;
        Interval _14233 = imul(param_var_a_18, param_var_b_14, intervalFailed, optical_product_upper);
        Interval param_var_b_15 = _14233;
        float _5913 = precise::max(param_var_a_17.lo, param_var_b_15.lo);
        float _5914 = precise::max(param_var_a_17.hi, param_var_b_15.hi);
        _5911.lo = _5913;
        _5911.hi = _5914;
        Interval _5912 = _5911;
        Interval _5915 = _5912;
        bias0 = _5915;
        Interval3 param_var_a_19 = hit;
        Interval3 param_var_a_20 = n;
        Interval param_var_b_16 = bias0;
        Interval _5901 = param_var_a_20.x;
        Interval _5902 = param_var_b_16;
        Interval _14257 = imul(_5901, _5902, intervalFailed, optical_product_upper);
        Interval _5903 = _14257;
        Interval _5904 = param_var_a_20.y;
        Interval _5905 = param_var_b_16;
        Interval _14261 = imul(_5904, _5905, intervalFailed, optical_product_upper);
        Interval _5906 = _14261;
        Interval _5907 = param_var_a_20.z;
        Interval _5908 = param_var_b_16;
        Interval _14265 = imul(_5907, _5908, intervalFailed, optical_product_upper);
        Interval _5909 = _14265;
        _5899.x = _5903;
        _5899.y = _5906;
        _5899.z = _5909;
        Interval3 _5900 = _5899;
        Interval3 _5910 = _5900;
        Interval3 param_var_b_17 = _5910;
        Interval _5889 = param_var_a_19.x;
        Interval _5890 = param_var_b_17.x;
        Interval _14279 = iadd(_5889, _5890, intervalFailed);
        Interval _5891 = _14279;
        Interval _5892 = param_var_a_19.y;
        Interval _5893 = param_var_b_17.y;
        Interval _14284 = iadd(_5892, _5893, intervalFailed);
        Interval _5894 = _14284;
        Interval _5895 = param_var_a_19.z;
        Interval _5896 = param_var_b_17.z;
        Interval _14289 = iadd(_5895, _5896, intervalFailed);
        Interval _5897 = _14289;
        _5887.x = _5891;
        _5887.y = _5894;
        _5887.z = _5897;
        Interval3 _5888 = _5887;
        Interval3 _5898 = _5888;
        origin = _5898;
        Interval3 param_var_a_21 = outgoing;
        Interval3 param_var_n_4 = n;
        Interval3 _5877 = param_var_a_21;
        Interval3 _5878 = param_var_n_4;
        float _5879 = 2.0;
        float _5874 = _5879;
        float _5875 = _5879;
        _5872.lo = _5874;
        _5872.hi = _5875;
        Interval _5873 = _5872;
        Interval _5876 = _5873;
        Interval _5880 = _5876;
        Interval3 _5881 = param_var_a_21;
        Interval3 _5882 = param_var_n_4;
        Interval _5861 = _5881.x;
        Interval _5862 = _5882.x;
        Interval _14318 = imul(_5861, _5862, intervalFailed, optical_product_upper);
        Interval _5863 = _14318;
        Interval _5864 = _5881.y;
        Interval _5865 = _5882.y;
        Interval _14323 = imul(_5864, _5865, intervalFailed, optical_product_upper);
        Interval _5866 = _14323;
        Interval _14324 = iadd(_5863, _5866, intervalFailed);
        Interval _5867 = _14324;
        Interval _5868 = _5881.z;
        Interval _5869 = _5882.z;
        Interval _14329 = imul(_5868, _5869, intervalFailed, optical_product_upper);
        Interval _5870 = _14329;
        Interval _14330 = iadd(_5867, _5870, intervalFailed);
        Interval _5871 = _14330;
        Interval _5883 = _5871;
        Interval _14332 = imul(_5880, _5883, intervalFailed, optical_product_upper);
        Interval _5884 = _14332;
        Interval _5851 = _5878.x;
        Interval _5852 = _5884;
        Interval _14336 = imul(_5851, _5852, intervalFailed, optical_product_upper);
        Interval _5853 = _14336;
        Interval _5854 = _5878.y;
        Interval _5855 = _5884;
        Interval _14340 = imul(_5854, _5855, intervalFailed, optical_product_upper);
        Interval _5856 = _14340;
        Interval _5857 = _5878.z;
        Interval _5858 = _5884;
        Interval _14344 = imul(_5857, _5858, intervalFailed, optical_product_upper);
        Interval _5859 = _14344;
        _5849.x = _5853;
        _5849.y = _5856;
        _5849.z = _5859;
        Interval3 _5850 = _5849;
        Interval3 _5860 = _5850;
        Interval3 _5885 = _5860;
        Interval3 _5840 = _5877;
        Interval _5841 = _5885.x;
        float _5837 = as_type<float>(as_type<uint>(_5841.hi) ^ 2147483648u);
        float _5838 = as_type<float>(as_type<uint>(_5841.lo) ^ 2147483648u);
        _5835.lo = _5837;
        _5835.hi = _5838;
        Interval _5836 = _5835;
        Interval _5839 = _5836;
        Interval _5842 = _5839;
        Interval _5843 = _5885.y;
        float _5832 = as_type<float>(as_type<uint>(_5843.hi) ^ 2147483648u);
        float _5833 = as_type<float>(as_type<uint>(_5843.lo) ^ 2147483648u);
        _5830.lo = _5832;
        _5830.hi = _5833;
        Interval _5831 = _5830;
        Interval _5834 = _5831;
        Interval _5844 = _5834;
        Interval _5845 = _5885.z;
        float _5827 = as_type<float>(as_type<uint>(_5845.hi) ^ 2147483648u);
        float _5828 = as_type<float>(as_type<uint>(_5845.lo) ^ 2147483648u);
        _5825.lo = _5827;
        _5825.hi = _5828;
        Interval _5826 = _5825;
        Interval _5829 = _5826;
        Interval _5846 = _5829;
        _5823.x = _5842;
        _5823.y = _5844;
        _5823.z = _5846;
        Interval3 _5824 = _5823;
        Interval3 _5847 = _5824;
        Interval _5813 = _5840.x;
        Interval _5814 = _5847.x;
        Interval _14424 = iadd(_5813, _5814, intervalFailed);
        Interval _5815 = _14424;
        Interval _5816 = _5840.y;
        Interval _5817 = _5847.y;
        Interval _14429 = iadd(_5816, _5817, intervalFailed);
        Interval _5818 = _14429;
        Interval _5819 = _5840.z;
        Interval _5820 = _5847.z;
        Interval _14434 = iadd(_5819, _5820, intervalFailed);
        Interval _5821 = _14434;
        _5811.x = _5815;
        _5811.y = _5818;
        _5811.z = _5821;
        Interval3 _5812 = _5811;
        Interval3 _5822 = _5812;
        Interval3 _5848 = _5822;
        Interval3 _5886 = _5848;
        outgoing = _5886;
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
            bool _5807 = false;
            if (param_var_a_22.lo <= 0.0)
            {
                _5807 = param_var_a_22.hi >= 0.0;
            }
            if (_5807)
            {
                _5806 = 0.0;
            }
            else
            {
                _5806 = precise::min(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            }
            float _5808 = _5806;
            float _5809 = precise::max(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            _5804.lo = _5808;
            _5804.hi = _5809;
            Interval _5805 = _5804;
            Interval _5810 = _5805;
            Interval ay = _5810;
            if (ay.hi < 0.949999988079071044921875)
            {
                Interval3 param_var_a_23 = outgoing;
                float3 param_var_v_3 = float3(0.0, 1.0, 0.0);
                float _5797 = param_var_v_3.x;
                float _5794 = _5797;
                float _5795 = _5797;
                _5792.lo = _5794;
                _5792.hi = _5795;
                Interval _5793 = _5792;
                Interval _5796 = _5793;
                Interval _5798 = _5796;
                float _5799 = param_var_v_3.y;
                float _5789 = _5799;
                float _5790 = _5799;
                _5787.lo = _5789;
                _5787.hi = _5790;
                Interval _5788 = _5787;
                Interval _5791 = _5788;
                Interval _5800 = _5791;
                float _5801 = param_var_v_3.z;
                float _5784 = _5801;
                float _5785 = _5801;
                _5782.lo = _5784;
                _5782.hi = _5785;
                Interval _5783 = _5782;
                Interval _5786 = _5783;
                Interval _5802 = _5786;
                _5780.x = _5798;
                _5780.y = _5800;
                _5780.z = _5802;
                Interval3 _5781 = _5780;
                Interval3 _5803 = _5781;
                Interval3 param_var_b_18 = _5803;
                Interval _5758 = param_var_a_23.y;
                Interval _5759 = param_var_b_18.z;
                Interval _14541 = imul(_5758, _5759, intervalFailed, optical_product_upper);
                Interval _5760 = _14541;
                Interval _5761 = param_var_a_23.z;
                Interval _5762 = param_var_b_18.y;
                Interval _14546 = imul(_5761, _5762, intervalFailed, optical_product_upper);
                Interval _5763 = _14546;
                Interval _5754 = _5760;
                Interval _5755 = _5763;
                float _5751 = as_type<float>(as_type<uint>(_5755.hi) ^ 2147483648u);
                float _5752 = as_type<float>(as_type<uint>(_5755.lo) ^ 2147483648u);
                _5749.lo = _5751;
                _5749.hi = _5752;
                Interval _5750 = _5749;
                Interval _5753 = _5750;
                Interval _5756 = _5753;
                Interval _14566 = iadd(_5754, _5756, intervalFailed);
                Interval _5757 = _14566;
                Interval _5764 = _5757;
                Interval _5765 = param_var_a_23.z;
                Interval _5766 = param_var_b_18.x;
                Interval _14572 = imul(_5765, _5766, intervalFailed, optical_product_upper);
                Interval _5767 = _14572;
                Interval _5768 = param_var_a_23.x;
                Interval _5769 = param_var_b_18.z;
                Interval _14577 = imul(_5768, _5769, intervalFailed, optical_product_upper);
                Interval _5770 = _14577;
                Interval _5745 = _5767;
                Interval _5746 = _5770;
                float _5742 = as_type<float>(as_type<uint>(_5746.hi) ^ 2147483648u);
                float _5743 = as_type<float>(as_type<uint>(_5746.lo) ^ 2147483648u);
                _5740.lo = _5742;
                _5740.hi = _5743;
                Interval _5741 = _5740;
                Interval _5744 = _5741;
                Interval _5747 = _5744;
                Interval _14597 = iadd(_5745, _5747, intervalFailed);
                Interval _5748 = _14597;
                Interval _5771 = _5748;
                Interval _5772 = param_var_a_23.x;
                Interval _5773 = param_var_b_18.y;
                Interval _14603 = imul(_5772, _5773, intervalFailed, optical_product_upper);
                Interval _5774 = _14603;
                Interval _5775 = param_var_a_23.y;
                Interval _5776 = param_var_b_18.x;
                Interval _14608 = imul(_5775, _5776, intervalFailed, optical_product_upper);
                Interval _5777 = _14608;
                Interval _5736 = _5774;
                Interval _5737 = _5777;
                float _5733 = as_type<float>(as_type<uint>(_5737.hi) ^ 2147483648u);
                float _5734 = as_type<float>(as_type<uint>(_5737.lo) ^ 2147483648u);
                _5731.lo = _5733;
                _5731.hi = _5734;
                Interval _5732 = _5731;
                Interval _5735 = _5732;
                Interval _5738 = _5735;
                Interval _14628 = iadd(_5736, _5738, intervalFailed);
                Interval _5739 = _14628;
                Interval _5778 = _5739;
                _5729.x = _5764;
                _5729.y = _5771;
                _5729.z = _5778;
                Interval3 _5730 = _5729;
                Interval3 _5779 = _5730;
                Interval3 param_var_a_24 = _5779;
                Interval3 _5722 = param_var_a_24;
                float _5723 = 1.0;
                float _5719 = _5723;
                float _5720 = _5723;
                _5717.lo = _5719;
                _5717.hi = _5720;
                Interval _5718 = _5717;
                Interval _5721 = _5718;
                Interval _5724 = _5721;
                Interval3 _5725 = param_var_a_24;
                Interval _5708 = _5725.x;
                bool _5701 = false;
                if (_5708.lo <= 0.0)
                {
                    _5701 = _5708.hi >= 0.0;
                }
                if (_5701)
                {
                    _5700 = 0.0;
                }
                else
                {
                    _5700 = precise::min(abs(_5708.lo), abs(_5708.hi));
                }
                float _5699 = _5700;
                float _5702 = precise::max(abs(_5708.lo), abs(_5708.hi));
                float _5703 = spvFMul(_5699, _5699);
                float _14681 = interval_down(_5703, intervalFailed);
                float _5704 = precise::max(0.0, _14681);
                float _5705 = spvFMul(_5702, _5702);
                float _14685 = interval_up(_5705, intervalFailed);
                float _5706 = _14685;
                _5697.lo = _5704;
                _5697.hi = _5706;
                Interval _5698 = _5697;
                Interval _5707 = _5698;
                Interval _5709 = _5707;
                Interval _5710 = _5725.y;
                bool _5690 = false;
                if (_5710.lo <= 0.0)
                {
                    _5690 = _5710.hi >= 0.0;
                }
                if (_5690)
                {
                    _5689 = 0.0;
                }
                else
                {
                    _5689 = precise::min(abs(_5710.lo), abs(_5710.hi));
                }
                float _5688 = _5689;
                float _5691 = precise::max(abs(_5710.lo), abs(_5710.hi));
                float _5692 = spvFMul(_5688, _5688);
                float _14724 = interval_down(_5692, intervalFailed);
                float _5693 = precise::max(0.0, _14724);
                float _5694 = spvFMul(_5691, _5691);
                float _14728 = interval_up(_5694, intervalFailed);
                float _5695 = _14728;
                _5686.lo = _5693;
                _5686.hi = _5695;
                Interval _5687 = _5686;
                Interval _5696 = _5687;
                Interval _5711 = _5696;
                Interval _14736 = iadd(_5709, _5711, intervalFailed);
                Interval _5712 = _14736;
                Interval _5713 = _5725.z;
                bool _5679 = false;
                if (_5713.lo <= 0.0)
                {
                    _5679 = _5713.hi >= 0.0;
                }
                if (_5679)
                {
                    _5678 = 0.0;
                }
                else
                {
                    _5678 = precise::min(abs(_5713.lo), abs(_5713.hi));
                }
                float _5677 = _5678;
                float _5680 = precise::max(abs(_5713.lo), abs(_5713.hi));
                float _5681 = spvFMul(_5677, _5677);
                float _14768 = interval_down(_5681, intervalFailed);
                float _5682 = precise::max(0.0, _14768);
                float _5683 = spvFMul(_5680, _5680);
                float _14772 = interval_up(_5683, intervalFailed);
                float _5684 = _14772;
                _5675.lo = _5682;
                _5675.hi = _5684;
                Interval _5676 = _5675;
                Interval _5685 = _5676;
                Interval _5714 = _5685;
                Interval _14780 = iadd(_5712, _5714, intervalFailed);
                Interval _5715 = _14780;
                Interval _14781 = isqrt(_5715, intervalFailed);
                Interval _5716 = _14781;
                Interval _5726 = _5716;
                Interval _14783 = idiv(_5724, _5726, intervalFailed, interval_divide_upper);
                Interval _5727 = _14783;
                Interval _5665 = _5722.x;
                Interval _5666 = _5727;
                Interval _14787 = imul(_5665, _5666, intervalFailed, optical_product_upper);
                Interval _5667 = _14787;
                Interval _5668 = _5722.y;
                Interval _5669 = _5727;
                Interval _14791 = imul(_5668, _5669, intervalFailed, optical_product_upper);
                Interval _5670 = _14791;
                Interval _5671 = _5722.z;
                Interval _5672 = _5727;
                Interval _14795 = imul(_5671, _5672, intervalFailed, optical_product_upper);
                Interval _5673 = _14795;
                _5663.x = _5667;
                _5663.y = _5670;
                _5663.z = _5673;
                Interval3 _5664 = _5663;
                Interval3 _5674 = _5664;
                Interval3 _5728 = _5674;
                t = _5728;
            }
            else
            {
                if (ay.lo >= 0.949999988079071044921875)
                {
                    Interval3 param_var_a_25 = outgoing;
                    float3 param_var_v_4 = float3(1.0, 0.0, 0.0);
                    float _5656 = param_var_v_4.x;
                    float _5653 = _5656;
                    float _5654 = _5656;
                    _5651.lo = _5653;
                    _5651.hi = _5654;
                    Interval _5652 = _5651;
                    Interval _5655 = _5652;
                    Interval _5657 = _5655;
                    float _5658 = param_var_v_4.y;
                    float _5648 = _5658;
                    float _5649 = _5658;
                    _5646.lo = _5648;
                    _5646.hi = _5649;
                    Interval _5647 = _5646;
                    Interval _5650 = _5647;
                    Interval _5659 = _5650;
                    float _5660 = param_var_v_4.z;
                    float _5643 = _5660;
                    float _5644 = _5660;
                    _5641.lo = _5643;
                    _5641.hi = _5644;
                    Interval _5642 = _5641;
                    Interval _5645 = _5642;
                    Interval _5661 = _5645;
                    _5639.x = _5657;
                    _5639.y = _5659;
                    _5639.z = _5661;
                    Interval3 _5640 = _5639;
                    Interval3 _5662 = _5640;
                    Interval3 param_var_b_19 = _5662;
                    Interval _5617 = param_var_a_25.y;
                    Interval _5618 = param_var_b_19.z;
                    Interval _14856 = imul(_5617, _5618, intervalFailed, optical_product_upper);
                    Interval _5619 = _14856;
                    Interval _5620 = param_var_a_25.z;
                    Interval _5621 = param_var_b_19.y;
                    Interval _14861 = imul(_5620, _5621, intervalFailed, optical_product_upper);
                    Interval _5622 = _14861;
                    Interval _5613 = _5619;
                    Interval _5614 = _5622;
                    float _5610 = as_type<float>(as_type<uint>(_5614.hi) ^ 2147483648u);
                    float _5611 = as_type<float>(as_type<uint>(_5614.lo) ^ 2147483648u);
                    _5608.lo = _5610;
                    _5608.hi = _5611;
                    Interval _5609 = _5608;
                    Interval _5612 = _5609;
                    Interval _5615 = _5612;
                    Interval _14881 = iadd(_5613, _5615, intervalFailed);
                    Interval _5616 = _14881;
                    Interval _5623 = _5616;
                    Interval _5624 = param_var_a_25.z;
                    Interval _5625 = param_var_b_19.x;
                    Interval _14887 = imul(_5624, _5625, intervalFailed, optical_product_upper);
                    Interval _5626 = _14887;
                    Interval _5627 = param_var_a_25.x;
                    Interval _5628 = param_var_b_19.z;
                    Interval _14892 = imul(_5627, _5628, intervalFailed, optical_product_upper);
                    Interval _5629 = _14892;
                    Interval _5604 = _5626;
                    Interval _5605 = _5629;
                    float _5601 = as_type<float>(as_type<uint>(_5605.hi) ^ 2147483648u);
                    float _5602 = as_type<float>(as_type<uint>(_5605.lo) ^ 2147483648u);
                    _5599.lo = _5601;
                    _5599.hi = _5602;
                    Interval _5600 = _5599;
                    Interval _5603 = _5600;
                    Interval _5606 = _5603;
                    Interval _14912 = iadd(_5604, _5606, intervalFailed);
                    Interval _5607 = _14912;
                    Interval _5630 = _5607;
                    Interval _5631 = param_var_a_25.x;
                    Interval _5632 = param_var_b_19.y;
                    Interval _14918 = imul(_5631, _5632, intervalFailed, optical_product_upper);
                    Interval _5633 = _14918;
                    Interval _5634 = param_var_a_25.y;
                    Interval _5635 = param_var_b_19.x;
                    Interval _14923 = imul(_5634, _5635, intervalFailed, optical_product_upper);
                    Interval _5636 = _14923;
                    Interval _5595 = _5633;
                    Interval _5596 = _5636;
                    float _5592 = as_type<float>(as_type<uint>(_5596.hi) ^ 2147483648u);
                    float _5593 = as_type<float>(as_type<uint>(_5596.lo) ^ 2147483648u);
                    _5590.lo = _5592;
                    _5590.hi = _5593;
                    Interval _5591 = _5590;
                    Interval _5594 = _5591;
                    Interval _5597 = _5594;
                    Interval _14943 = iadd(_5595, _5597, intervalFailed);
                    Interval _5598 = _14943;
                    Interval _5637 = _5598;
                    _5588.x = _5623;
                    _5588.y = _5630;
                    _5588.z = _5637;
                    Interval3 _5589 = _5588;
                    Interval3 _5638 = _5589;
                    Interval3 param_var_a_26 = _5638;
                    Interval3 _5581 = param_var_a_26;
                    float _5582 = 1.0;
                    float _5578 = _5582;
                    float _5579 = _5582;
                    _5576.lo = _5578;
                    _5576.hi = _5579;
                    Interval _5577 = _5576;
                    Interval _5580 = _5577;
                    Interval _5583 = _5580;
                    Interval3 _5584 = param_var_a_26;
                    Interval _5567 = _5584.x;
                    bool _5560 = false;
                    if (_5567.lo <= 0.0)
                    {
                        _5560 = _5567.hi >= 0.0;
                    }
                    if (_5560)
                    {
                        _5559 = 0.0;
                    }
                    else
                    {
                        _5559 = precise::min(abs(_5567.lo), abs(_5567.hi));
                    }
                    float _5558 = _5559;
                    float _5561 = precise::max(abs(_5567.lo), abs(_5567.hi));
                    float _5562 = spvFMul(_5558, _5558);
                    float _14996 = interval_down(_5562, intervalFailed);
                    float _5563 = precise::max(0.0, _14996);
                    float _5564 = spvFMul(_5561, _5561);
                    float _15000 = interval_up(_5564, intervalFailed);
                    float _5565 = _15000;
                    _5556.lo = _5563;
                    _5556.hi = _5565;
                    Interval _5557 = _5556;
                    Interval _5566 = _5557;
                    Interval _5568 = _5566;
                    Interval _5569 = _5584.y;
                    bool _5549 = false;
                    if (_5569.lo <= 0.0)
                    {
                        _5549 = _5569.hi >= 0.0;
                    }
                    if (_5549)
                    {
                        _5548 = 0.0;
                    }
                    else
                    {
                        _5548 = precise::min(abs(_5569.lo), abs(_5569.hi));
                    }
                    float _5547 = _5548;
                    float _5550 = precise::max(abs(_5569.lo), abs(_5569.hi));
                    float _5551 = spvFMul(_5547, _5547);
                    float _15039 = interval_down(_5551, intervalFailed);
                    float _5552 = precise::max(0.0, _15039);
                    float _5553 = spvFMul(_5550, _5550);
                    float _15043 = interval_up(_5553, intervalFailed);
                    float _5554 = _15043;
                    _5545.lo = _5552;
                    _5545.hi = _5554;
                    Interval _5546 = _5545;
                    Interval _5555 = _5546;
                    Interval _5570 = _5555;
                    Interval _15051 = iadd(_5568, _5570, intervalFailed);
                    Interval _5571 = _15051;
                    Interval _5572 = _5584.z;
                    bool _5538 = false;
                    if (_5572.lo <= 0.0)
                    {
                        _5538 = _5572.hi >= 0.0;
                    }
                    if (_5538)
                    {
                        _5537 = 0.0;
                    }
                    else
                    {
                        _5537 = precise::min(abs(_5572.lo), abs(_5572.hi));
                    }
                    float _5536 = _5537;
                    float _5539 = precise::max(abs(_5572.lo), abs(_5572.hi));
                    float _5540 = spvFMul(_5536, _5536);
                    float _15083 = interval_down(_5540, intervalFailed);
                    float _5541 = precise::max(0.0, _15083);
                    float _5542 = spvFMul(_5539, _5539);
                    float _15087 = interval_up(_5542, intervalFailed);
                    float _5543 = _15087;
                    _5534.lo = _5541;
                    _5534.hi = _5543;
                    Interval _5535 = _5534;
                    Interval _5544 = _5535;
                    Interval _5573 = _5544;
                    Interval _15095 = iadd(_5571, _5573, intervalFailed);
                    Interval _5574 = _15095;
                    Interval _15096 = isqrt(_5574, intervalFailed);
                    Interval _5575 = _15096;
                    Interval _5585 = _5575;
                    Interval _15098 = idiv(_5583, _5585, intervalFailed, interval_divide_upper);
                    Interval _5586 = _15098;
                    Interval _5524 = _5581.x;
                    Interval _5525 = _5586;
                    Interval _15102 = imul(_5524, _5525, intervalFailed, optical_product_upper);
                    Interval _5526 = _15102;
                    Interval _5527 = _5581.y;
                    Interval _5528 = _5586;
                    Interval _15106 = imul(_5527, _5528, intervalFailed, optical_product_upper);
                    Interval _5529 = _15106;
                    Interval _5530 = _5581.z;
                    Interval _5531 = _5586;
                    Interval _15110 = imul(_5530, _5531, intervalFailed, optical_product_upper);
                    Interval _5532 = _15110;
                    _5522.x = _5526;
                    _5522.y = _5529;
                    _5522.z = _5532;
                    Interval3 _5523 = _5522;
                    Interval3 _5533 = _5523;
                    Interval3 _5587 = _5533;
                    t = _5587;
                }
                else
                {
                    intervalFailed = true;
                    return false;
                }
            }
            int2 tap = _2227[uint(receiver.settings.y)];
            Interval3 param_var_a_27 = outgoing;
            Interval3 param_var_a_28 = t;
            float param_var_n_5 = float(tap.x);
            float param_var_d_3 = 1000.0;
            Interval _15132 = iratio(param_var_n_5, param_var_d_3, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_20 = _15132;
            Interval _5512 = param_var_a_28.x;
            Interval _5513 = param_var_b_20;
            Interval _15136 = imul(_5512, _5513, intervalFailed, optical_product_upper);
            Interval _5514 = _15136;
            Interval _5515 = param_var_a_28.y;
            Interval _5516 = param_var_b_20;
            Interval _15140 = imul(_5515, _5516, intervalFailed, optical_product_upper);
            Interval _5517 = _15140;
            Interval _5518 = param_var_a_28.z;
            Interval _5519 = param_var_b_20;
            Interval _15144 = imul(_5518, _5519, intervalFailed, optical_product_upper);
            Interval _5520 = _15144;
            _5510.x = _5514;
            _5510.y = _5517;
            _5510.z = _5520;
            Interval3 _5511 = _5510;
            Interval3 _5521 = _5511;
            Interval3 param_var_a_29 = _5521;
            Interval3 param_var_a_30 = outgoing;
            Interval3 param_var_b_21 = t;
            Interval _5488 = param_var_a_30.y;
            Interval _5489 = param_var_b_21.z;
            Interval _15160 = imul(_5488, _5489, intervalFailed, optical_product_upper);
            Interval _5490 = _15160;
            Interval _5491 = param_var_a_30.z;
            Interval _5492 = param_var_b_21.y;
            Interval _15165 = imul(_5491, _5492, intervalFailed, optical_product_upper);
            Interval _5493 = _15165;
            Interval _5484 = _5490;
            Interval _5485 = _5493;
            float _5481 = as_type<float>(as_type<uint>(_5485.hi) ^ 2147483648u);
            float _5482 = as_type<float>(as_type<uint>(_5485.lo) ^ 2147483648u);
            _5479.lo = _5481;
            _5479.hi = _5482;
            Interval _5480 = _5479;
            Interval _5483 = _5480;
            Interval _5486 = _5483;
            Interval _15185 = iadd(_5484, _5486, intervalFailed);
            Interval _5487 = _15185;
            Interval _5494 = _5487;
            Interval _5495 = param_var_a_30.z;
            Interval _5496 = param_var_b_21.x;
            Interval _15191 = imul(_5495, _5496, intervalFailed, optical_product_upper);
            Interval _5497 = _15191;
            Interval _5498 = param_var_a_30.x;
            Interval _5499 = param_var_b_21.z;
            Interval _15196 = imul(_5498, _5499, intervalFailed, optical_product_upper);
            Interval _5500 = _15196;
            Interval _5475 = _5497;
            Interval _5476 = _5500;
            float _5472 = as_type<float>(as_type<uint>(_5476.hi) ^ 2147483648u);
            float _5473 = as_type<float>(as_type<uint>(_5476.lo) ^ 2147483648u);
            _5470.lo = _5472;
            _5470.hi = _5473;
            Interval _5471 = _5470;
            Interval _5474 = _5471;
            Interval _5477 = _5474;
            Interval _15216 = iadd(_5475, _5477, intervalFailed);
            Interval _5478 = _15216;
            Interval _5501 = _5478;
            Interval _5502 = param_var_a_30.x;
            Interval _5503 = param_var_b_21.y;
            Interval _15222 = imul(_5502, _5503, intervalFailed, optical_product_upper);
            Interval _5504 = _15222;
            Interval _5505 = param_var_a_30.y;
            Interval _5506 = param_var_b_21.x;
            Interval _15227 = imul(_5505, _5506, intervalFailed, optical_product_upper);
            Interval _5507 = _15227;
            Interval _5466 = _5504;
            Interval _5467 = _5507;
            float _5463 = as_type<float>(as_type<uint>(_5467.hi) ^ 2147483648u);
            float _5464 = as_type<float>(as_type<uint>(_5467.lo) ^ 2147483648u);
            _5461.lo = _5463;
            _5461.hi = _5464;
            Interval _5462 = _5461;
            Interval _5465 = _5462;
            Interval _5468 = _5465;
            Interval _15247 = iadd(_5466, _5468, intervalFailed);
            Interval _5469 = _15247;
            Interval _5508 = _5469;
            _5459.x = _5494;
            _5459.y = _5501;
            _5459.z = _5508;
            Interval3 _5460 = _5459;
            Interval3 _5509 = _5460;
            Interval3 param_var_a_31 = _5509;
            float param_var_n_6 = float(tap.y);
            float param_var_d_4 = 1000.0;
            Interval _15261 = iratio(param_var_n_6, param_var_d_4, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_22 = _15261;
            Interval _5449 = param_var_a_31.x;
            Interval _5450 = param_var_b_22;
            Interval _15265 = imul(_5449, _5450, intervalFailed, optical_product_upper);
            Interval _5451 = _15265;
            Interval _5452 = param_var_a_31.y;
            Interval _5453 = param_var_b_22;
            Interval _15269 = imul(_5452, _5453, intervalFailed, optical_product_upper);
            Interval _5454 = _15269;
            Interval _5455 = param_var_a_31.z;
            Interval _5456 = param_var_b_22;
            Interval _15273 = imul(_5455, _5456, intervalFailed, optical_product_upper);
            Interval _5457 = _15273;
            _5447.x = _5451;
            _5447.y = _5454;
            _5447.z = _5457;
            Interval3 _5448 = _5447;
            Interval3 _5458 = _5448;
            Interval3 param_var_b_23 = _5458;
            Interval _5437 = param_var_a_29.x;
            Interval _5438 = param_var_b_23.x;
            Interval _15287 = iadd(_5437, _5438, intervalFailed);
            Interval _5439 = _15287;
            Interval _5440 = param_var_a_29.y;
            Interval _5441 = param_var_b_23.y;
            Interval _15292 = iadd(_5440, _5441, intervalFailed);
            Interval _5442 = _15292;
            Interval _5443 = param_var_a_29.z;
            Interval _5444 = param_var_b_23.z;
            Interval _15297 = iadd(_5443, _5444, intervalFailed);
            Interval _5445 = _15297;
            _5435.x = _5439;
            _5435.y = _5442;
            _5435.z = _5445;
            Interval3 _5436 = _5435;
            Interval3 _5446 = _5436;
            Interval3 param_var_a_32 = _5446;
            float param_var_x_8 = receiver.settings.x;
            float _5432 = param_var_x_8;
            float _5433 = param_var_x_8;
            _5430.lo = _5432;
            _5430.hi = _5433;
            Interval _5431 = _5430;
            Interval _5434 = _5431;
            Interval param_var_a_33 = _5434;
            bool _5423 = false;
            if (param_var_a_33.lo <= 0.0)
            {
                _5423 = param_var_a_33.hi >= 0.0;
            }
            if (_5423)
            {
                _5422 = 0.0;
            }
            else
            {
                _5422 = precise::min(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            }
            float _5421 = _5422;
            float _5424 = precise::max(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            float _5425 = spvFMul(_5421, _5421);
            float _15348 = interval_down(_5425, intervalFailed);
            float _5426 = precise::max(0.0, _15348);
            float _5427 = spvFMul(_5424, _5424);
            float _15352 = interval_up(_5427, intervalFailed);
            float _5428 = _15352;
            _5419.lo = _5426;
            _5419.hi = _5428;
            Interval _5420 = _5419;
            Interval _5429 = _5420;
            Interval param_var_b_24 = _5429;
            Interval _5409 = param_var_a_32.x;
            Interval _5410 = param_var_b_24;
            Interval _15363 = imul(_5409, _5410, intervalFailed, optical_product_upper);
            Interval _5411 = _15363;
            Interval _5412 = param_var_a_32.y;
            Interval _5413 = param_var_b_24;
            Interval _15367 = imul(_5412, _5413, intervalFailed, optical_product_upper);
            Interval _5414 = _15367;
            Interval _5415 = param_var_a_32.z;
            Interval _5416 = param_var_b_24;
            Interval _15371 = imul(_5415, _5416, intervalFailed, optical_product_upper);
            Interval _5417 = _15371;
            _5407.x = _5411;
            _5407.y = _5414;
            _5407.z = _5417;
            Interval3 _5408 = _5407;
            Interval3 _5418 = _5408;
            Interval3 param_var_b_25 = _5418;
            Interval _5397 = param_var_a_27.x;
            Interval _5398 = param_var_b_25.x;
            Interval _15385 = iadd(_5397, _5398, intervalFailed);
            Interval _5399 = _15385;
            Interval _5400 = param_var_a_27.y;
            Interval _5401 = param_var_b_25.y;
            Interval _15390 = iadd(_5400, _5401, intervalFailed);
            Interval _5402 = _15390;
            Interval _5403 = param_var_a_27.z;
            Interval _5404 = param_var_b_25.z;
            Interval _15395 = iadd(_5403, _5404, intervalFailed);
            Interval _5405 = _15395;
            _5395.x = _5399;
            _5395.y = _5402;
            _5395.z = _5405;
            Interval3 _5396 = _5395;
            Interval3 _5406 = _5396;
            Interval3 param_var_a_34 = _5406;
            Interval3 _5388 = param_var_a_34;
            float _5389 = 1.0;
            float _5385 = _5389;
            float _5386 = _5389;
            _5383.lo = _5385;
            _5383.hi = _5386;
            Interval _5384 = _5383;
            Interval _5387 = _5384;
            Interval _5390 = _5387;
            Interval3 _5391 = param_var_a_34;
            Interval _5374 = _5391.x;
            bool _5367 = false;
            if (_5374.lo <= 0.0)
            {
                _5367 = _5374.hi >= 0.0;
            }
            if (_5367)
            {
                _5366 = 0.0;
            }
            else
            {
                _5366 = precise::min(abs(_5374.lo), abs(_5374.hi));
            }
            float _5365 = _5366;
            float _5368 = precise::max(abs(_5374.lo), abs(_5374.hi));
            float _5369 = spvFMul(_5365, _5365);
            float _15447 = interval_down(_5369, intervalFailed);
            float _5370 = precise::max(0.0, _15447);
            float _5371 = spvFMul(_5368, _5368);
            float _15451 = interval_up(_5371, intervalFailed);
            float _5372 = _15451;
            _5363.lo = _5370;
            _5363.hi = _5372;
            Interval _5364 = _5363;
            Interval _5373 = _5364;
            Interval _5375 = _5373;
            Interval _5376 = _5391.y;
            bool _5356 = false;
            if (_5376.lo <= 0.0)
            {
                _5356 = _5376.hi >= 0.0;
            }
            if (_5356)
            {
                _5355 = 0.0;
            }
            else
            {
                _5355 = precise::min(abs(_5376.lo), abs(_5376.hi));
            }
            float _5354 = _5355;
            float _5357 = precise::max(abs(_5376.lo), abs(_5376.hi));
            float _5358 = spvFMul(_5354, _5354);
            float _15490 = interval_down(_5358, intervalFailed);
            float _5359 = precise::max(0.0, _15490);
            float _5360 = spvFMul(_5357, _5357);
            float _15494 = interval_up(_5360, intervalFailed);
            float _5361 = _15494;
            _5352.lo = _5359;
            _5352.hi = _5361;
            Interval _5353 = _5352;
            Interval _5362 = _5353;
            Interval _5377 = _5362;
            Interval _15502 = iadd(_5375, _5377, intervalFailed);
            Interval _5378 = _15502;
            Interval _5379 = _5391.z;
            bool _5345 = false;
            if (_5379.lo <= 0.0)
            {
                _5345 = _5379.hi >= 0.0;
            }
            if (_5345)
            {
                _5344 = 0.0;
            }
            else
            {
                _5344 = precise::min(abs(_5379.lo), abs(_5379.hi));
            }
            float _5343 = _5344;
            float _5346 = precise::max(abs(_5379.lo), abs(_5379.hi));
            float _5347 = spvFMul(_5343, _5343);
            float _15534 = interval_down(_5347, intervalFailed);
            float _5348 = precise::max(0.0, _15534);
            float _5349 = spvFMul(_5346, _5346);
            float _15538 = interval_up(_5349, intervalFailed);
            float _5350 = _15538;
            _5341.lo = _5348;
            _5341.hi = _5350;
            Interval _5342 = _5341;
            Interval _5351 = _5342;
            Interval _5380 = _5351;
            Interval _15546 = iadd(_5378, _5380, intervalFailed);
            Interval _5381 = _15546;
            Interval _15547 = isqrt(_5381, intervalFailed);
            Interval _5382 = _15547;
            Interval _5392 = _5382;
            Interval _15549 = idiv(_5390, _5392, intervalFailed, interval_divide_upper);
            Interval _5393 = _15549;
            Interval _5331 = _5388.x;
            Interval _5332 = _5393;
            Interval _15553 = imul(_5331, _5332, intervalFailed, optical_product_upper);
            Interval _5333 = _15553;
            Interval _5334 = _5388.y;
            Interval _5335 = _5393;
            Interval _15557 = imul(_5334, _5335, intervalFailed, optical_product_upper);
            Interval _5336 = _15557;
            Interval _5337 = _5388.z;
            Interval _5338 = _5393;
            Interval _15561 = imul(_5337, _5338, intervalFailed, optical_product_upper);
            Interval _5339 = _15561;
            _5329.x = _5333;
            _5329.y = _5336;
            _5329.z = _5339;
            Interval3 _5330 = _5329;
            Interval3 _5340 = _5330;
            Interval3 _5394 = _5340;
            Interval3 rough = _5394;
            Interval3 param_var_a_35 = rough;
            Interval3 param_var_b_26 = n;
            Interval _5318 = param_var_a_35.x;
            Interval _5319 = param_var_b_26.x;
            Interval _15578 = imul(_5318, _5319, intervalFailed, optical_product_upper);
            Interval _5320 = _15578;
            Interval _5321 = param_var_a_35.y;
            Interval _5322 = param_var_b_26.y;
            Interval _15583 = imul(_5321, _5322, intervalFailed, optical_product_upper);
            Interval _5323 = _15583;
            Interval _15584 = iadd(_5320, _5323, intervalFailed);
            Interval _5324 = _15584;
            Interval _5325 = param_var_a_35.z;
            Interval _5326 = param_var_b_26.z;
            Interval _15589 = imul(_5325, _5326, intervalFailed, optical_product_upper);
            Interval _5327 = _15589;
            Interval _15590 = iadd(_5324, _5327, intervalFailed);
            Interval _5328 = _15590;
            Interval nd = _5328;
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
                    Interval _5308 = param_var_a_36.x;
                    Interval _5309 = param_var_b_27.x;
                    float _5305 = precise::min(_5308.lo, _5309.lo);
                    float _5306 = precise::max(_5308.hi, _5309.hi);
                    _5303.lo = _5305;
                    _5303.hi = _5306;
                    Interval _5304 = _5303;
                    Interval _5307 = _5304;
                    Interval _5310 = _5307;
                    Interval _5311 = param_var_a_36.y;
                    Interval _5312 = param_var_b_27.y;
                    float _5300 = precise::min(_5311.lo, _5312.lo);
                    float _5301 = precise::max(_5311.hi, _5312.hi);
                    _5298.lo = _5300;
                    _5298.hi = _5301;
                    Interval _5299 = _5298;
                    Interval _5302 = _5299;
                    Interval _5313 = _5302;
                    Interval _5314 = param_var_a_36.z;
                    Interval _5315 = param_var_b_27.z;
                    float _5295 = precise::min(_5314.lo, _5315.lo);
                    float _5296 = precise::max(_5314.hi, _5315.hi);
                    _5293.lo = _5295;
                    _5293.hi = _5296;
                    Interval _5294 = _5293;
                    Interval _5297 = _5294;
                    Interval _5316 = _5297;
                    _5291.x = _5310;
                    _5291.y = _5313;
                    _5291.z = _5316;
                    Interval3 _5292 = _5291;
                    Interval3 _5317 = _5292;
                    outgoing = _5317;
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
        Interval3 _5282 = param_var_a_37;
        Interval _5283 = param_var_b_28.x;
        float _5279 = as_type<float>(as_type<uint>(_5283.hi) ^ 2147483648u);
        float _5280 = as_type<float>(as_type<uint>(_5283.lo) ^ 2147483648u);
        Interval _5277;
        _5277.lo = _5279;
        _5277.hi = _5280;
        Interval _5278 = _5277;
        Interval _5281 = _5278;
        Interval _5284 = _5281;
        Interval _5285 = param_var_b_28.y;
        float _5274 = as_type<float>(as_type<uint>(_5285.hi) ^ 2147483648u);
        float _5275 = as_type<float>(as_type<uint>(_5285.lo) ^ 2147483648u);
        Interval _5272;
        _5272.lo = _5274;
        _5272.hi = _5275;
        Interval _5273 = _5272;
        Interval _5276 = _5273;
        Interval _5286 = _5276;
        Interval _5287 = param_var_b_28.z;
        float _5269 = as_type<float>(as_type<uint>(_5287.hi) ^ 2147483648u);
        float _5270 = as_type<float>(as_type<uint>(_5287.lo) ^ 2147483648u);
        Interval _5267;
        _5267.lo = _5269;
        _5267.hi = _5270;
        Interval _5268 = _5267;
        Interval _5271 = _5268;
        Interval _5288 = _5271;
        Interval3 _5265;
        _5265.x = _5284;
        _5265.y = _5286;
        _5265.z = _5288;
        Interval3 _5266 = _5265;
        Interval3 _5289 = _5266;
        Interval _5255 = _5282.x;
        Interval _5256 = _5289.x;
        Interval _15749 = iadd(_5255, _5256, intervalFailed);
        Interval _5257 = _15749;
        Interval _5258 = _5282.y;
        Interval _5259 = _5289.y;
        Interval _15754 = iadd(_5258, _5259, intervalFailed);
        Interval _5260 = _15754;
        Interval _5261 = _5282.z;
        Interval _5262 = _5289.z;
        Interval _15759 = iadd(_5261, _5262, intervalFailed);
        Interval _5263 = _15759;
        Interval3 _5253;
        _5253.x = _5257;
        _5253.y = _5260;
        _5253.z = _5263;
        Interval3 _5254 = _5253;
        Interval3 _5264 = _5254;
        Interval3 _5290 = _5264;
        travel = _5290;
    }
    Interval3 param_var_a_38 = travel;
    Interval _5244 = param_var_a_38.x;
    bool _5237 = false;
    if (_5244.lo <= 0.0)
    {
        _5237 = _5244.hi >= 0.0;
    }
    float _5236;
    if (_5237)
    {
        _5236 = 0.0;
    }
    else
    {
        _5236 = precise::min(abs(_5244.lo), abs(_5244.hi));
    }
    float _5235 = _5236;
    float _5238 = precise::max(abs(_5244.lo), abs(_5244.hi));
    float _5239 = spvFMul(_5235, _5235);
    float _15802 = interval_down(_5239, intervalFailed);
    float _5240 = precise::max(0.0, _15802);
    float _5241 = spvFMul(_5238, _5238);
    float _15806 = interval_up(_5241, intervalFailed);
    float _5242 = _15806;
    Interval _5233;
    _5233.lo = _5240;
    _5233.hi = _5242;
    Interval _5234 = _5233;
    Interval _5243 = _5234;
    Interval _5245 = _5243;
    Interval _5246 = param_var_a_38.y;
    bool _5226 = false;
    if (_5246.lo <= 0.0)
    {
        _5226 = _5246.hi >= 0.0;
    }
    float _5225;
    if (_5226)
    {
        _5225 = 0.0;
    }
    else
    {
        _5225 = precise::min(abs(_5246.lo), abs(_5246.hi));
    }
    float _5224 = _5225;
    float _5227 = precise::max(abs(_5246.lo), abs(_5246.hi));
    float _5228 = spvFMul(_5224, _5224);
    float _15845 = interval_down(_5228, intervalFailed);
    float _5229 = precise::max(0.0, _15845);
    float _5230 = spvFMul(_5227, _5227);
    float _15849 = interval_up(_5230, intervalFailed);
    float _5231 = _15849;
    Interval _5222;
    _5222.lo = _5229;
    _5222.hi = _5231;
    Interval _5223 = _5222;
    Interval _5232 = _5223;
    Interval _5247 = _5232;
    Interval _15857 = iadd(_5245, _5247, intervalFailed);
    Interval _5248 = _15857;
    Interval _5249 = param_var_a_38.z;
    bool _5215 = false;
    if (_5249.lo <= 0.0)
    {
        _5215 = _5249.hi >= 0.0;
    }
    float _5214;
    if (_5215)
    {
        _5214 = 0.0;
    }
    else
    {
        _5214 = precise::min(abs(_5249.lo), abs(_5249.hi));
    }
    float _5213 = _5214;
    float _5216 = precise::max(abs(_5249.lo), abs(_5249.hi));
    float _5217 = spvFMul(_5213, _5213);
    float _15889 = interval_down(_5217, intervalFailed);
    float _5218 = precise::max(0.0, _15889);
    float _5219 = spvFMul(_5216, _5216);
    float _15893 = interval_up(_5219, intervalFailed);
    float _5220 = _15893;
    Interval _5211;
    _5211.lo = _5218;
    _5211.hi = _5220;
    Interval _5212 = _5211;
    Interval _5221 = _5212;
    Interval _5250 = _5221;
    Interval _15901 = iadd(_5248, _5250, intervalFailed);
    Interval _5251 = _15901;
    Interval _15902 = isqrt(_5251, intervalFailed);
    Interval _5252 = _15902;
    Interval len = _5252;
    Interval3 param_var_a_39 = travel;
    Interval3 param_var_b_29 = outgoing;
    Interval _5200 = param_var_a_39.x;
    Interval _5201 = param_var_b_29.x;
    Interval _15910 = imul(_5200, _5201, intervalFailed, optical_product_upper);
    Interval _5202 = _15910;
    Interval _5203 = param_var_a_39.y;
    Interval _5204 = param_var_b_29.y;
    Interval _15915 = imul(_5203, _5204, intervalFailed, optical_product_upper);
    Interval _5205 = _15915;
    Interval _15916 = iadd(_5202, _5205, intervalFailed);
    Interval _5206 = _15916;
    Interval _5207 = param_var_a_39.z;
    Interval _5208 = param_var_b_29.z;
    Interval _15921 = imul(_5207, _5208, intervalFailed, optical_product_upper);
    Interval _5209 = _15921;
    Interval _15922 = iadd(_5206, _5209, intervalFailed);
    Interval _5210 = _15922;
    Interval temp_var_Interval = _5210;
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
    Interval _5178 = param_var_a_40.y;
    Interval _5179 = param_var_b_30.z;
    Interval _15939 = imul(_5178, _5179, intervalFailed, optical_product_upper);
    Interval _5180 = _15939;
    Interval _5181 = param_var_a_40.z;
    Interval _5182 = param_var_b_30.y;
    Interval _15944 = imul(_5181, _5182, intervalFailed, optical_product_upper);
    Interval _5183 = _15944;
    Interval _5174 = _5180;
    Interval _5175 = _5183;
    float _5171 = as_type<float>(as_type<uint>(_5175.hi) ^ 2147483648u);
    float _5172 = as_type<float>(as_type<uint>(_5175.lo) ^ 2147483648u);
    Interval _5169;
    _5169.lo = _5171;
    _5169.hi = _5172;
    Interval _5170 = _5169;
    Interval _5173 = _5170;
    Interval _5176 = _5173;
    Interval _15964 = iadd(_5174, _5176, intervalFailed);
    Interval _5177 = _15964;
    Interval _5184 = _5177;
    Interval _5185 = param_var_a_40.z;
    Interval _5186 = param_var_b_30.x;
    Interval _15970 = imul(_5185, _5186, intervalFailed, optical_product_upper);
    Interval _5187 = _15970;
    Interval _5188 = param_var_a_40.x;
    Interval _5189 = param_var_b_30.z;
    Interval _15975 = imul(_5188, _5189, intervalFailed, optical_product_upper);
    Interval _5190 = _15975;
    Interval _5165 = _5187;
    Interval _5166 = _5190;
    float _5162 = as_type<float>(as_type<uint>(_5166.hi) ^ 2147483648u);
    float _5163 = as_type<float>(as_type<uint>(_5166.lo) ^ 2147483648u);
    Interval _5160;
    _5160.lo = _5162;
    _5160.hi = _5163;
    Interval _5161 = _5160;
    Interval _5164 = _5161;
    Interval _5167 = _5164;
    Interval _15995 = iadd(_5165, _5167, intervalFailed);
    Interval _5168 = _15995;
    Interval _5191 = _5168;
    Interval _5192 = param_var_a_40.x;
    Interval _5193 = param_var_b_30.y;
    Interval _16001 = imul(_5192, _5193, intervalFailed, optical_product_upper);
    Interval _5194 = _16001;
    Interval _5195 = param_var_a_40.y;
    Interval _5196 = param_var_b_30.x;
    Interval _16006 = imul(_5195, _5196, intervalFailed, optical_product_upper);
    Interval _5197 = _16006;
    Interval _5156 = _5194;
    Interval _5157 = _5197;
    float _5153 = as_type<float>(as_type<uint>(_5157.hi) ^ 2147483648u);
    float _5154 = as_type<float>(as_type<uint>(_5157.lo) ^ 2147483648u);
    Interval _5151;
    _5151.lo = _5153;
    _5151.hi = _5154;
    Interval _5152 = _5151;
    Interval _5155 = _5152;
    Interval _5158 = _5155;
    Interval _16026 = iadd(_5156, _5158, intervalFailed);
    Interval _5159 = _16026;
    Interval _5198 = _5159;
    Interval3 _5149;
    _5149.x = _5184;
    _5149.y = _5191;
    _5149.z = _5198;
    Interval3 _5150 = _5149;
    Interval3 _5199 = _5150;
    Interval3 error = _5199;
    float param_var_x_9 = spvFMul(len.hi, 0.0040000001899898052215576171875);
    float _16039 = interval_up(param_var_x_9, intervalFailed);
    float param_var_a_41 = _16039;
    float param_var_b_31 = precise::max(receiver.projection.x, receiver.projection.y);
    bool param_var_upper = true;
    float _16047 = quotient_bound(param_var_a_41, param_var_b_31, param_var_upper, intervalFailed);
    float param_var_x_10 = _16047;
    float _16048 = interval_up(param_var_x_10, intervalFailed);
    float cap = _16048;
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
void src_feature_optical_main(thread const uint3& group, thread const uint& lane, constant type_Settings& Settings, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, constant type_OpticalSettings& OpticalSettings, device type_ByteAddressBuffer& regions, device type_ByteAddressBuffer& frames, device type_ByteAddressBuffer& queries, device type_RWByteAddressBuffer& results, constant type_TargetSettings& TargetSettings, threadgroup uint& keptCount, threadgroup uint& excludedCount, threadgroup uint& hullX0, threadgroup uint& hullY0, threadgroup uint& hullX1, threadgroup uint& hullY1)
{
    uint q = group.y;
    uint r = group.x;
    bool temp_var_logical = true;
    if (q < Settings.queryCount)
    {
        temp_var_logical = r >= 128u;
    }
    if (temp_var_logical)
    {
        return;
    }
    uint _input = q * 6160u;
    uint _output = (q * 4096u) + (r * 32u);
    uint count = regions._m0[_input >> 2u];
    uint status = regions._m0[(_input + 4u) >> 2u];
    if (lane == 0u)
    {
        uint _2259 = _output >> 2u;
        results._m0[_2259] = 0u;
        results._m0[_2259 + 1u] = status;
        results._m0[_2259 + 2u] = 0u;
        results._m0[_2259 + 3u] = 0u;
        uint _2266 = (_output + 16u) >> 2u;
        results._m0[_2266] = 0u;
        results._m0[_2266 + 1u] = 0u;
        results._m0[_2266 + 2u] = 0u;
        results._m0[_2266 + 3u] = 0u;
    }
    bool temp_var_logical_1 = true;
    if (status == 0u)
    {
        temp_var_logical_1 = r >= count;
    }
    if (temp_var_logical_1)
    {
        return;
    }
    bool temp_var_logical_2 = true;
    if (count <= 128u)
    {
        temp_var_logical_2 = OpticalSettings.opticalCapacity != 128u;
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical_2)
    {
        temp_var_logical_3 = OpticalSettings.opticalBudget == 0u;
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        temp_var_logical_4 = OpticalSettings.opticalBudget > 8192u;
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        temp_var_logical_5 = OpticalSettings.opticalDepth > 12u;
    }
    if (temp_var_logical_5)
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint at = q * 512u;
    uint _2305 = (at + 496u) >> 2u;
    if (any(uint4(frames._m0[_2305], frames._m0[_2305 + 1u], frames._m0[_2305 + 2u], frames._m0[_2305 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint _2323 = at >> 2u;
    ReflectionRoughFrame receiver;
    receiver.a = as_type<float4>(uint4(frames._m0[_2323], frames._m0[_2323 + 1u], frames._m0[_2323 + 2u], frames._m0[_2323 + 3u]));
    uint _2336 = (at + 16u) >> 2u;
    receiver.b = as_type<float4>(uint4(frames._m0[_2336], frames._m0[_2336 + 1u], frames._m0[_2336 + 2u], frames._m0[_2336 + 3u]));
    uint _2349 = (at + 32u) >> 2u;
    receiver.c = as_type<float4>(uint4(frames._m0[_2349], frames._m0[_2349 + 1u], frames._m0[_2349 + 2u], frames._m0[_2349 + 3u]));
    uint _2362 = (at + 48u) >> 2u;
    receiver.projection = as_type<float4>(uint4(frames._m0[_2362], frames._m0[_2362 + 1u], frames._m0[_2362 + 2u], frames._m0[_2362 + 3u]));
    uint _2375 = (at + 64u) >> 2u;
    receiver.extentClip = as_type<float4>(uint4(frames._m0[_2375], frames._m0[_2375 + 1u], frames._m0[_2375 + 2u], frames._m0[_2375 + 3u]));
    uint _2388 = (at + 80u) >> 2u;
    receiver.settings = as_type<float4>(uint4(frames._m0[_2388], frames._m0[_2388 + 1u], frames._m0[_2388 + 2u], frames._m0[_2388 + 3u]));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint h = 0u; h < 4u; h++)
    {
        uint _2404 = ((at + 96u) + (h * 48u)) >> 2u;
        planes[h].a = as_type<float4>(uint4(frames._m0[_2404], frames._m0[_2404 + 1u], frames._m0[_2404 + 2u], frames._m0[_2404 + 3u]));
        uint _2419 = ((at + 112u) + (h * 48u)) >> 2u;
        planes[h].b = as_type<float4>(uint4(frames._m0[_2419], frames._m0[_2419 + 1u], frames._m0[_2419 + 2u], frames._m0[_2419 + 3u]));
        uint _2434 = ((at + 128u) + (h * 48u)) >> 2u;
        planes[h].c = as_type<float4>(uint4(frames._m0[_2434], frames._m0[_2434 + 1u], frames._m0[_2434 + 2u], frames._m0[_2434 + 3u]));
    }
    uint _2449 = (at + 288u) >> 2u;
    ReflectionLiquidFrame liquid;
    liquid.planePoint = as_type<float4>(uint4(frames._m0[_2449], frames._m0[_2449 + 1u], frames._m0[_2449 + 2u], frames._m0[_2449 + 3u]));
    uint _2462 = (at + 304u) >> 2u;
    liquid.planeNormal = as_type<float4>(uint4(frames._m0[_2462], frames._m0[_2462 + 1u], frames._m0[_2462 + 2u], frames._m0[_2462 + 3u]));
    uint _2475 = (at + 320u) >> 2u;
    liquid.rotation0 = as_type<float4>(uint4(frames._m0[_2475], frames._m0[_2475 + 1u], frames._m0[_2475 + 2u], frames._m0[_2475 + 3u]));
    uint _2488 = (at + 336u) >> 2u;
    liquid.rotation1 = as_type<float4>(uint4(frames._m0[_2488], frames._m0[_2488 + 1u], frames._m0[_2488 + 2u], frames._m0[_2488 + 3u]));
    uint _2501 = (at + 352u) >> 2u;
    liquid.rotation2 = as_type<float4>(uint4(frames._m0[_2501], frames._m0[_2501 + 1u], frames._m0[_2501 + 2u], frames._m0[_2501 + 3u]));
    uint _2514 = (at + 368u) >> 2u;
    liquid.projection = as_type<float4>(uint4(frames._m0[_2514], frames._m0[_2514 + 1u], frames._m0[_2514 + 2u], frames._m0[_2514 + 3u]));
    uint _2527 = (at + 384u) >> 2u;
    liquid.extentClip = as_type<float4>(uint4(frames._m0[_2527], frames._m0[_2527 + 1u], frames._m0[_2527 + 2u], frames._m0[_2527 + 3u]));
    uint _2540 = (at + 400u) >> 2u;
    liquid.settings = as_type<float4>(uint4(frames._m0[_2540], frames._m0[_2540 + 1u], frames._m0[_2540 + 2u], frames._m0[_2540 + 3u]));
    uint _2553 = (at + 416u) >> 2u;
    float4 terminal = as_type<float4>(uint4(frames._m0[_2553], frames._m0[_2553 + 1u], frames._m0[_2553 + 2u], frames._m0[_2553 + 3u]));
    uint _2565 = (at + 480u) >> 2u;
    uint4 control = uint4(frames._m0[_2565], frames._m0[_2565 + 1u], frames._m0[_2565 + 2u], frames._m0[_2565 + 3u]);
    bool temp_var_logical_6 = true;
    if (control.z == 0u)
    {
        temp_var_logical_6 = control.y != 0u;
    }
    bool needsLiquid = temp_var_logical_6;
    bool temp_var_logical_7 = true;
    if (needsLiquid)
    {
        ReflectionLiquidFrame param_var_old = liquid;
        bool temp_var_logical_8 = false;
        if (reflection_liquid_frame_valid(param_var_old))
        {
            temp_var_logical_8 = all(receiver.projection == liquid.projection);
        }
        bool temp_var_logical_9 = false;
        if (temp_var_logical_8)
        {
            temp_var_logical_9 = all(receiver.extentClip == liquid.extentClip);
        }
        temp_var_logical_7 = temp_var_logical_9;
    }
    bool liquidValid = temp_var_logical_7;
    uint _2601 = (at + 432u) >> 2u;
    ReflectionSpecularPlane terminalPlane;
    terminalPlane.a = as_type<float4>(uint4(frames._m0[_2601], frames._m0[_2601 + 1u], frames._m0[_2601 + 2u], frames._m0[_2601 + 3u]));
    uint _2614 = (at + 448u) >> 2u;
    terminalPlane.b = as_type<float4>(uint4(frames._m0[_2614], frames._m0[_2614 + 1u], frames._m0[_2614 + 2u], frames._m0[_2614 + 3u]));
    uint _2627 = (at + 464u) >> 2u;
    terminalPlane.c = as_type<float4>(uint4(frames._m0[_2627], frames._m0[_2627 + 1u], frames._m0[_2627 + 2u], frames._m0[_2627 + 3u]));
    float3 param_var_f = terminal.xyz;
    uint param_var_kind = control.w;
    bool temp_var_logical_10 = true;
    if (feature_valid(param_var_f, param_var_kind))
    {
        bool temp_var_logical_11 = false;
        if (terminal.w != 0.0)
        {
            ReflectionSpecularPlane param_var_plane = terminalPlane;
            temp_var_logical_11 = !reflection_specular_plane_valid(param_var_plane);
        }
        temp_var_logical_10 = temp_var_logical_11;
    }
    if (temp_var_logical_10)
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
    bool temp_var_logical_12 = true;
    if (control.x <= 4u)
    {
        temp_var_logical_12 = (control.y >> (control.x & 31u)) != 0u;
    }
    bool temp_var_logical_13 = true;
    if (!temp_var_logical_12)
    {
        temp_var_logical_13 = control.z > 1u;
    }
    bool temp_var_logical_14 = true;
    if (!temp_var_logical_13)
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
        temp_var_logical_14 = control.z != temp_var_ternary;
    }
    bool temp_var_logical_15 = true;
    if (!temp_var_logical_14)
    {
        bool4 _2696 = isnan(receiver.settings);
        bool4 _2697 = isinf(receiver.settings);
        temp_var_logical_15 = !all(not(bool4(_2696.x || _2697.x, _2696.y || _2697.y, _2696.z || _2697.z, _2696.w || _2697.w)));
    }
    bool temp_var_logical_16 = true;
    if (!temp_var_logical_15)
    {
        temp_var_logical_16 = receiver.settings.x < 0.0;
    }
    bool temp_var_logical_17 = true;
    if (!temp_var_logical_16)
    {
        temp_var_logical_17 = receiver.settings.x > 1.0;
    }
    bool temp_var_logical_18 = true;
    if (!temp_var_logical_17)
    {
        temp_var_logical_18 = receiver.settings.y != float(queryLobe);
    }
    bool temp_var_logical_19 = true;
    if (!temp_var_logical_18)
    {
        temp_var_logical_19 = queryLobe > 7u;
    }
    bool temp_var_logical_20 = true;
    if (!temp_var_logical_19)
    {
        temp_var_logical_20 = any(receiver.settings.zw != float2(0.0));
    }
    bool temp_var_logical_21 = true;
    if (!temp_var_logical_20)
    {
        temp_var_logical_21 = control.x != (queryControl & 7u);
    }
    bool temp_var_logical_22 = true;
    if (!temp_var_logical_21)
    {
        temp_var_logical_22 = control.y != ((queryControl >> 4u) & 15u);
    }
    bool temp_var_logical_23 = true;
    if (!temp_var_logical_22)
    {
        temp_var_logical_23 = control.w != (queryControl >> 8u);
    }
    bool temp_var_logical_24 = true;
    if (!temp_var_logical_23)
    {
        bool4 _2759 = isnan(receiver.projection);
        bool4 _2760 = isinf(receiver.projection);
        temp_var_logical_24 = !all(not(bool4(_2759.x || _2760.x, _2759.y || _2760.y, _2759.z || _2760.z, _2759.w || _2760.w)));
    }
    bool temp_var_logical_25 = true;
    if (!temp_var_logical_24)
    {
        temp_var_logical_25 = any(receiver.projection.xy <= float2(0.0));
    }
    bool temp_var_logical_26 = true;
    if (!temp_var_logical_25)
    {
        bool4 _2775 = isnan(terminal);
        bool4 _2776 = isinf(terminal);
        temp_var_logical_26 = !all(not(bool4(_2775.x || _2776.x, _2775.y || _2776.y, _2775.z || _2776.z, _2775.w || _2776.w)));
    }
    bool temp_var_logical_27 = true;
    if (!temp_var_logical_26)
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
        temp_var_logical_27 = terminal.w != float(temp_var_ternary_1);
    }
    bool temp_var_logical_28 = true;
    if (!temp_var_logical_27)
    {
        temp_var_logical_28 = !liquidValid;
    }
    bool temp_var_logical_29 = true;
    if (!temp_var_logical_28)
    {
        bool temp_var_logical_30 = false;
        if (control.z == 0u)
        {
            ReflectionRoughFrame param_var_old_1 = receiver;
            temp_var_logical_30 = !reflection_rough_frame_valid(param_var_old_1);
        }
        temp_var_logical_29 = temp_var_logical_30;
    }
    if (temp_var_logical_29)
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    for (uint h_1 = 0u; h_1 < control.x; h_1++)
    {
        bool temp_var_logical_31 = false;
        if ((control.y & (1u << (h_1 & 31u))) == 0u)
        {
            ReflectionSpecularPlane param_var_plane_1 = planes[h_1];
            temp_var_logical_31 = !reflection_specular_plane_valid(param_var_plane_1);
        }
        if (temp_var_logical_31)
        {
            if (lane == 0u)
            {
                results._m0[(_output + 4u) >> 2u] = 4u;
            }
            return;
        }
    }
    uint _2835 = (((_input + 16u) + (r * 48u)) + 16u) >> 2u;
    float4 initial = spvFAdd(as_type<float4>(uint4(regions._m0[_2835], regions._m0[_2835 + 1u], regions._m0[_2835 + 2u], regions._m0[_2835 + 3u])), float4(0.5));
    float param_var_x = initial.x;
    float _2848 = interval_down(param_var_x, intervalFailed);
    float param_var_x_1 = initial.y;
    float _2851 = interval_down(param_var_x_1, intervalFailed);
    float param_var_x_2 = initial.z;
    float _2854 = interval_up(param_var_x_2, intervalFailed);
    float param_var_x_3 = initial.w;
    float _2857 = interval_up(param_var_x_3, intervalFailed);
    float4 extent = float4(_2848, _2851, _2854, _2857);
    bool4 _2860 = isnan(extent);
    bool4 _2861 = isinf(extent);
    bool temp_var_logical_32 = true;
    if (all(not(bool4(_2860.x || _2861.x, _2860.y || _2861.y, _2860.z || _2861.z, _2860.w || _2861.w))))
    {
        temp_var_logical_32 = any(extent.xy > extent.zw);
    }
    if (temp_var_logical_32)
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
    Interval3 _2908 = native_target(param_var_at, param_var_feature, intervalFailed, optical_product_upper, interval_divide_upper, frames, TargetSettings);
    Interval3 enclosedTarget = _2908;
    bool targetFailed = intervalFailed;
    for (uint cell = lane; cell < cells; cell += 64u)
    {
        uint2 ij = uint2(cell % bins.x, cell / bins.x);
        intervalFailed = false;
        float param_var_lo = extent.x;
        float param_var_hi = extent.z;
        uint param_var_cell = ij.x;
        uint param_var_count = bins.x;
        float param_var_x_4 = split_axis(param_var_lo, param_var_hi, param_var_cell, param_var_count);
        float _2930 = interval_down(param_var_x_4, intervalFailed);
        float param_var_lo_1 = extent.y;
        float param_var_hi_1 = extent.w;
        uint param_var_cell_1 = ij.y;
        uint param_var_count_1 = bins.y;
        float param_var_x_5 = split_axis(param_var_lo_1, param_var_hi_1, param_var_cell_1, param_var_count_1);
        float _2940 = interval_down(param_var_x_5, intervalFailed);
        float param_var_lo_2 = extent.x;
        float param_var_hi_2 = extent.z;
        uint param_var_cell_2 = ij.x + 1u;
        uint param_var_count_2 = bins.x;
        float param_var_x_6 = split_axis(param_var_lo_2, param_var_hi_2, param_var_cell_2, param_var_count_2);
        float _2950 = interval_up(param_var_x_6, intervalFailed);
        float param_var_lo_3 = extent.y;
        float param_var_hi_3 = extent.w;
        uint param_var_cell_3 = ij.y + 1u;
        uint param_var_count_3 = bins.y;
        float param_var_x_7 = split_axis(param_var_lo_3, param_var_hi_3, param_var_cell_3, param_var_count_3);
        float _2960 = interval_up(param_var_x_7, intervalFailed);
        float4 box = float4(_2930, _2940, _2950, _2960);
        bool temp_var_logical_33 = false;
        if (!targetFailed)
        {
            float4 param_var_box = box;
            ReflectionRoughFrame param_var_receiver = receiver;
            ReflectionLiquidFrame param_var_liquid = liquid;
            spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
            uint4 param_var_control = control;
            Interval3 param_var_target = enclosedTarget;
            bool param_var_finiteTerminal = terminal.w != 0.0;
            bool _2973 = optical_excluded_target(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTerminal, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper);
            temp_var_logical_33 = _2973;
        }
        bool excluded = temp_var_logical_33;
        if (excluded)
        {
            __attribute__((unused)) uint _2976 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&excludedCount, 1u, memory_order_relaxed);
        }
        else
        {
            __attribute__((unused)) uint _2977 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&keptCount, 1u, memory_order_relaxed);
            __attribute__((unused)) uint _2981 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullX0, as_type<uint>(box.x), memory_order_relaxed);
            __attribute__((unused)) uint _2985 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullY0, as_type<uint>(box.y), memory_order_relaxed);
            __attribute__((unused)) uint _2989 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullX1, as_type<uint>(box.z), memory_order_relaxed);
            __attribute__((unused)) uint _2993 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullY1, as_type<uint>(box.w), memory_order_relaxed);
        }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (lane == 0u)
    {
        uint _2998 = _output >> 2u;
        results._m0[_2998] = keptCount;
        results._m0[_2998 + 1u] = 0u;
        results._m0[_2998 + 2u] = cells;
        results._m0[_2998 + 3u] = excludedCount;
        uint _3007 = (_output + 16u) >> 2u;
        uint4 temp_var_ternary_2;
        if (keptCount != 0u)
        {
            temp_var_ternary_2 = uint4(hullX0, hullY0, hullX1, hullY1);
        }
        else
        {
            temp_var_ternary_2 = uint4(0u);
        }
        results._m0[_3007] = temp_var_ternary_2.x;
        results._m0[_3007 + 1u] = temp_var_ternary_2.y;
        results._m0[_3007 + 2u] = temp_var_ternary_2.z;
        results._m0[_3007 + 3u] = temp_var_ternary_2.w;
    }
}

kernel void feature_optical_main(constant type_Settings& Settings [[buffer(0)]], constant type_OpticalSettings& OpticalSettings [[buffer(1)]], device type_ByteAddressBuffer& regions [[buffer(3)]], device type_ByteAddressBuffer& frames [[buffer(4)]], device type_ByteAddressBuffer& queries [[buffer(5)]], device type_RWByteAddressBuffer& results [[buffer(6)]], constant type_TargetSettings& TargetSettings [[buffer(2)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]])
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
    src_feature_optical_main(param_var_group, param_var_lane, Settings, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, OpticalSettings, regions, frames, queries, results, TargetSettings, keptCount, excludedCount, hullX0, hullY0, hullX1, hullY1);
}

