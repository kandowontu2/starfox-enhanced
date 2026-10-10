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

constant spvUnsafeArray<int2, 8> _2325 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2326 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float normalLength = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3138 = isnan(old.planePoint);
    bool4 _3139 = isinf(old.planePoint);
    bool temp_var_logical = true;
    if (all(not(bool4(_3138.x || _3139.x, _3138.y || _3139.y, _3138.z || _3139.z, _3138.w || _3139.w))))
    {
        bool4 _3145 = isnan(old.planeNormal);
        bool4 _3146 = isinf(old.planeNormal);
        temp_var_logical = !all(not(bool4(_3145.x || _3146.x, _3145.y || _3146.y, _3145.z || _3146.z, _3145.w || _3146.w)));
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        bool4 _3155 = isnan(old.rotation0);
        bool4 _3156 = isinf(old.rotation0);
        temp_var_logical_1 = !all(not(bool4(_3155.x || _3156.x, _3155.y || _3156.y, _3155.z || _3156.z, _3155.w || _3156.w)));
    }
    bool temp_var_logical_2 = true;
    if (!temp_var_logical_1)
    {
        bool4 _3165 = isnan(old.rotation1);
        bool4 _3166 = isinf(old.rotation1);
        temp_var_logical_2 = !all(not(bool4(_3165.x || _3166.x, _3165.y || _3166.y, _3165.z || _3166.z, _3165.w || _3166.w)));
    }
    bool temp_var_logical_3 = true;
    if (!temp_var_logical_2)
    {
        bool4 _3175 = isnan(old.rotation2);
        bool4 _3176 = isinf(old.rotation2);
        temp_var_logical_3 = !all(not(bool4(_3175.x || _3176.x, _3175.y || _3176.y, _3175.z || _3176.z, _3175.w || _3176.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3185 = isnan(old.projection);
        bool4 _3186 = isinf(old.projection);
        temp_var_logical_4 = !all(not(bool4(_3185.x || _3186.x, _3185.y || _3186.y, _3185.z || _3186.z, _3185.w || _3186.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3195 = isnan(old.extentClip);
        bool4 _3196 = isinf(old.extentClip);
        temp_var_logical_5 = !all(not(bool4(_3195.x || _3196.x, _3195.y || _3196.y, _3195.z || _3196.z, _3195.w || _3196.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3205 = isnan(old.settings);
        bool4 _3206 = isinf(old.settings);
        temp_var_logical_6 = !all(not(bool4(_3205.x || _3206.x, _3205.y || _3206.y, _3205.z || _3206.z, _3205.w || _3206.w)));
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
    bool3 _3343 = isnan(f);
    bool3 _3344 = isinf(f);
    if (!all(not(bool3(_3343.x || _3344.x, _3343.y || _3344.y, _3343.z || _3344.z))))
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
    bool _3375 = false;
    if (param_var_p.a.w == 2.0)
    {
        _3375 = param_var_p.b.w == 2.0;
    }
    bool _3376 = false;
    if (_3375)
    {
        _3376 = param_var_p.c.w == 2.0;
    }
    bool _3377 = _3376;
    bool analytic = _3377;
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
        bool4 _3419 = isnan(plane.a);
        bool4 _3420 = isinf(plane.a);
        temp_var_logical_3 = !all(not(bool4(_3419.x || _3420.x, _3419.y || _3420.y, _3419.z || _3420.z, _3419.w || _3420.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3429 = isnan(plane.b);
        bool4 _3430 = isinf(plane.b);
        temp_var_logical_4 = !all(not(bool4(_3429.x || _3430.x, _3429.y || _3430.y, _3429.z || _3430.z, _3429.w || _3430.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3439 = isnan(plane.c);
        bool4 _3440 = isinf(plane.c);
        temp_var_logical_5 = !all(not(bool4(_3439.x || _3440.x, _3439.y || _3440.y, _3439.z || _3440.z, _3439.w || _3440.w)));
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
    bool3 _3498 = isnan(n);
    bool3 _3499 = isinf(n);
    bool temp_var_logical_10 = false;
    if (all(not(bool3(_3498.x || _3499.x, _3498.y || _3499.y, _3498.z || _3499.z))))
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
    bool _3512 = false;
    if (param_var_old.a.w == 2.0)
    {
        _3512 = param_var_old.b.w == 2.0;
    }
    bool _3513 = false;
    if (_3512)
    {
        _3513 = param_var_old.c.w == 2.0;
    }
    bool _3514 = _3513;
    bool analytic = _3514;
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
        bool4 _3556 = isnan(old.a);
        bool4 _3557 = isinf(old.a);
        temp_var_logical_3 = !all(not(bool4(_3556.x || _3557.x, _3556.y || _3557.y, _3556.z || _3557.z, _3556.w || _3557.w)));
    }
    bool temp_var_logical_4 = true;
    if (!temp_var_logical_3)
    {
        bool4 _3566 = isnan(old.b);
        bool4 _3567 = isinf(old.b);
        temp_var_logical_4 = !all(not(bool4(_3566.x || _3567.x, _3566.y || _3567.y, _3566.z || _3567.z, _3566.w || _3567.w)));
    }
    bool temp_var_logical_5 = true;
    if (!temp_var_logical_4)
    {
        bool4 _3576 = isnan(old.c);
        bool4 _3577 = isinf(old.c);
        temp_var_logical_5 = !all(not(bool4(_3576.x || _3577.x, _3576.y || _3577.y, _3576.z || _3577.z, _3576.w || _3577.w)));
    }
    bool temp_var_logical_6 = true;
    if (!temp_var_logical_5)
    {
        bool4 _3586 = isnan(old.projection);
        bool4 _3587 = isinf(old.projection);
        temp_var_logical_6 = !all(not(bool4(_3586.x || _3587.x, _3586.y || _3587.y, _3586.z || _3587.z, _3586.w || _3587.w)));
    }
    bool temp_var_logical_7 = true;
    if (!temp_var_logical_6)
    {
        bool4 _3596 = isnan(old.extentClip);
        bool4 _3597 = isinf(old.extentClip);
        temp_var_logical_7 = !all(not(bool4(_3596.x || _3597.x, _3596.y || _3597.y, _3596.z || _3597.z, _3596.w || _3597.w)));
    }
    bool temp_var_logical_8 = true;
    if (!temp_var_logical_7)
    {
        bool4 _3606 = isnan(old.settings);
        bool4 _3607 = isinf(old.settings);
        temp_var_logical_8 = !all(not(bool4(_3606.x || _3607.x, _3606.y || _3607.y, _3606.z || _3607.z, _3606.w || _3607.w)));
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
    bool3 _3755 = isnan(normal);
    bool3 _3756 = isinf(normal);
    bool temp_var_logical_26 = false;
    if (all(not(bool3(_3755.x || _3756.x, _3755.y || _3756.y, _3755.z || _3756.z))))
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
                    float _20051 = interval_up(param_var_x, intervalFailed);
                    temp_var_ternary_2 = _20051;
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
                float _20057 = interval_down(param_var_x_1, intervalFailed);
                temp_var_ternary_3 = _20057;
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
        float _20064 = interval_up(param_var_x_2, intervalFailed);
        temp_var_ternary_4 = _20064;
    }
    else
    {
        float param_var_x_3 = sum_1;
        float _20066 = interval_down(param_var_x_3, intervalFailed);
        temp_var_ternary_4 = _20066;
    }
    return temp_var_ternary_4;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    Interval param_var_a = a;
    bool _16196 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _16196 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _16197 = false;
    if (_16196)
    {
        _16197 = param_var_a.lo <= param_var_a.hi;
    }
    bool _16198 = _16197;
    bool temp_var_logical = false;
    if (_16198)
    {
        Interval param_var_a_1 = b;
        bool _16193 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _16193 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _16194 = false;
        if (_16193)
        {
            _16194 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _16195 = _16194;
        temp_var_logical = _16195;
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
    float _16260 = optical_add_bound(param_var_a_4, param_var_b, param_var_upper, intervalFailed);
    float param_var_lo = _16260;
    float param_var_a_5 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _16265 = optical_add_bound(param_var_a_5, param_var_b_1, param_var_upper_1, intervalFailed);
    float param_var_hi = _16265;
    Interval _16191;
    _16191.lo = param_var_lo;
    _16191.hi = param_var_hi;
    Interval _16192 = _16191;
    return _16192;
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
                float _20243 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _20243;
                float param_var_x_1 = product;
                float _20245 = interval_down(param_var_x_1, intervalFailed);
                return _20245;
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
                float _20291 = interval_up(param_var_x_2, intervalFailed);
                temp_var_ternary_1 = _20291;
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
                float _20299 = interval_down(param_var_x_3, intervalFailed);
                temp_var_ternary_3 = _20299;
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
    float _20305 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _20305;
    float param_var_x_5 = product_1;
    float _20307 = interval_down(param_var_x_5, intervalFailed);
    return _20307;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval param_var_a = a;
    bool _18148 = false;
    if (!(isnan(param_var_a.lo) || isinf(param_var_a.lo)))
    {
        _18148 = !(isnan(param_var_a.hi) || isinf(param_var_a.hi));
    }
    bool _18149 = false;
    if (_18148)
    {
        _18149 = param_var_a.lo <= param_var_a.hi;
    }
    bool _18150 = _18149;
    bool temp_var_logical = false;
    if (_18150)
    {
        Interval param_var_a_1 = b;
        bool _18145 = false;
        if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
        {
            _18145 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
        }
        bool _18146 = false;
        if (_18145)
        {
            _18146 = param_var_a_1.lo <= param_var_a_1.hi;
        }
        bool _18147 = _18146;
        temp_var_logical = _18147;
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
            float _18142 = param_var_x_2;
            float _18143 = param_var_x_2;
            Interval _18140;
            _18140.lo = _18142;
            _18140.hi = _18143;
            Interval _18141 = _18140;
            Interval _18144 = _18141;
            return _18144;
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
            float _18137 = as_type<float>(as_type<uint>(param_var_a_7.hi) ^ 2147483648u);
            float _18138 = as_type<float>(as_type<uint>(param_var_a_7.lo) ^ 2147483648u);
            Interval _18135;
            _18135.lo = _18137;
            _18135.hi = _18138;
            Interval _18136 = _18135;
            Interval _18139 = _18136;
            return _18139;
        }
        Interval param_var_a_8 = b;
        float param_var_x_6 = -1.0;
        if (interval_exact_point(param_var_a_8, param_var_x_6))
        {
            Interval param_var_a_9 = a;
            float _18132 = as_type<float>(as_type<uint>(param_var_a_9.hi) ^ 2147483648u);
            float _18133 = as_type<float>(as_type<uint>(param_var_a_9.lo) ^ 2147483648u);
            Interval _18130;
            _18130.lo = _18132;
            _18130.hi = _18133;
            Interval _18131 = _18130;
            Interval _18134 = _18131;
            return _18134;
        }
    }
    Interval param_var_a_10 = a;
    bool _18127 = false;
    if (!(isnan(param_var_a_10.lo) || isinf(param_var_a_10.lo)))
    {
        _18127 = !(isnan(param_var_a_10.hi) || isinf(param_var_a_10.hi));
    }
    bool _18128 = false;
    if (_18127)
    {
        _18128 = param_var_a_10.lo <= param_var_a_10.hi;
    }
    bool _18129 = _18128;
    bool temp_var_logical_2 = false;
    if (_18129)
    {
        Interval param_var_a_11 = b;
        bool _18124 = false;
        if (!(isnan(param_var_a_11.lo) || isinf(param_var_a_11.lo)))
        {
            _18124 = !(isnan(param_var_a_11.hi) || isinf(param_var_a_11.hi));
        }
        bool _18125 = false;
        if (_18124)
        {
            _18125 = param_var_a_11.lo <= param_var_a_11.hi;
        }
        bool _18126 = _18125;
        temp_var_logical_2 = _18126;
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
        float _18372 = optical_product_bounds(param_var_a_12, param_var_b, intervalFailed, optical_product_upper);
        float resultLo = _18372;
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
        __attribute__((unused)) float _18389 = optical_product_bounds(param_var_a_13, param_var_b_1, intervalFailed, optical_product_upper);
        float param_var_lo = resultLo;
        float param_var_hi = optical_product_upper;
        Interval _18122;
        _18122.lo = param_var_lo;
        _18122.hi = param_var_hi;
        Interval _18123 = _18122;
        return _18123;
    }
    float param_var_a_14 = a.lo;
    float param_var_b_2 = b.lo;
    float _18402 = optical_product_bounds(param_var_a_14, param_var_b_2, intervalFailed, optical_product_upper);
    float p = _18402;
    float u = optical_product_upper;
    float q = p;
    float v = u;
    if (!sameB)
    {
        float param_var_a_15 = a.lo;
        float param_var_b_3 = b.hi;
        float _18412 = optical_product_bounds(param_var_a_15, param_var_b_3, intervalFailed, optical_product_upper);
        q = _18412;
        v = optical_product_upper;
    }
    float r = p;
    float w = u;
    if (!sameA)
    {
        float param_var_a_16 = a.hi;
        float param_var_b_4 = b.lo;
        float _18422 = optical_product_bounds(param_var_a_16, param_var_b_4, intervalFailed, optical_product_upper);
        r = _18422;
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
            float _18435 = optical_product_bounds(param_var_a_17, param_var_b_5, intervalFailed, optical_product_upper);
            s = _18435;
            x = optical_product_upper;
        }
    }
    float param_var_lo_1 = precise::min(precise::min(p, q), precise::min(r, s));
    float param_var_hi_1 = precise::max(precise::max(u, v), precise::max(w, x));
    Interval _18120;
    _18120.lo = param_var_lo_1;
    _18120.hi = param_var_hi_1;
    Interval _18121 = _18120;
    return _18121;
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float q = precise::sqrt(a);
    bool temp_var_ternary;
    float temp_var_ternary_1;
    CurvedScalar _20342;
    for (uint n = 0u; n < 8u; n++)
    {
        float param_var_ax = q;
        float param_var_ay = 0.0;
        float param_var_bx = q;
        float param_var_by = 0.0;
        float _20344 = spvFMul(param_var_ax, param_var_bx);
        float _20345 = spvFMul(param_var_ax, 4097.0);
        float _20346 = spvFSub(_20345, spvFSub(_20345, param_var_ax));
        float _20347 = spvFSub(param_var_ax, _20346);
        float _20348 = spvFMul(param_var_bx, 4097.0);
        float _20349 = spvFSub(_20348, spvFSub(_20348, param_var_bx));
        float _20350 = spvFSub(param_var_bx, _20349);
        float _20351 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_20346, _20349), _20344), spvFMul(_20346, _20350)), spvFMul(_20347, _20349)), spvFMul(_20347, _20350)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _20352 = spvFAdd(_20344, _20351);
        float _20353 = spvFSub(_20351, spvFSub(_20352, _20344));
        float _20354 = _20352;
        float _20355 = _20353;
        _20342.high = _20354;
        _20342.low = _20355;
        CurvedScalar _20343 = _20342;
        CurvedScalar _20356 = _20343;
        CurvedScalar square = _20356;
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
            float _20454 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_1 = _20454;
        }
        else
        {
            float param_var_x_1 = q;
            float _20456 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_1 = precise::max(0.0, _20456);
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
        Interval _20310;
        _20310.lo = param_var_lo;
        _20310.hi = param_var_hi;
        Interval _20311 = _20310;
        return _20311;
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _20328 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _20328;
    float _20329 = interval_down(param_var_x, intervalFailed);
    float param_var_lo_1 = precise::max(0.0, _20329);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _20334 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _20334;
    float _20335 = interval_up(param_var_x_1, intervalFailed);
    float param_var_hi_1 = _20335;
    Interval _20308;
    _20308.lo = param_var_lo_1;
    _20308.hi = param_var_hi_1;
    Interval _20309 = _20308;
    return _20309;
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float q = a / b;
    bool temp_var_ternary;
    bool temp_var_ternary_1;
    float temp_var_ternary_2;
    CurvedScalar _19835;
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
        float _19837 = spvFMul(param_var_ax, param_var_bx);
        float _19838 = spvFMul(param_var_ax, 4097.0);
        float _19839 = spvFSub(_19838, spvFSub(_19838, param_var_ax));
        float _19840 = spvFSub(param_var_ax, _19839);
        float _19841 = spvFMul(param_var_bx, 4097.0);
        float _19842 = spvFSub(_19841, spvFSub(_19841, param_var_bx));
        float _19843 = spvFSub(param_var_bx, _19842);
        float _19844 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_19839, _19842), _19837), spvFMul(_19839, _19843)), spvFMul(_19840, _19842)), spvFMul(_19840, _19843)), spvFMul(param_var_ax, param_var_by)), spvFMul(param_var_ay, param_var_bx)), spvFMul(param_var_ay, param_var_by));
        float _19845 = spvFAdd(_19837, _19844);
        float _19846 = spvFSub(_19844, spvFSub(_19845, _19837));
        float _19847 = _19845;
        float _19848 = _19846;
        _19835.high = _19847;
        _19835.low = _19848;
        CurvedScalar _19836 = _19835;
        CurvedScalar _19849 = _19836;
        CurvedScalar product = _19849;
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
            float _19966 = interval_up(param_var_x, intervalFailed);
            temp_var_ternary_2 = _19966;
        }
        else
        {
            float param_var_x_1 = q;
            float _19968 = interval_down(param_var_x_1, intervalFailed);
            temp_var_ternary_2 = _19968;
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
    bool _16348 = false;
    if (param_var_a.lo <= 0.0)
    {
        _16348 = param_var_a.hi >= 0.0;
    }
    bool _16349 = _16348;
    bool temp_var_logical = true;
    if (!_16349)
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
        Interval _16346;
        _16346.lo = param_var_lo;
        _16346.hi = param_var_hi;
        Interval _16347 = _16346;
        return _16347;
    }
    Interval param_var_a_1 = a;
    bool _16343 = false;
    if (!(isnan(param_var_a_1.lo) || isinf(param_var_a_1.lo)))
    {
        _16343 = !(isnan(param_var_a_1.hi) || isinf(param_var_a_1.hi));
    }
    bool _16344 = false;
    if (_16343)
    {
        _16344 = param_var_a_1.lo <= param_var_a_1.hi;
    }
    bool _16345 = _16344;
    bool temp_var_logical_2 = false;
    if (_16345)
    {
        Interval param_var_a_2 = b;
        bool _16340 = false;
        if (!(isnan(param_var_a_2.lo) || isinf(param_var_a_2.lo)))
        {
            _16340 = !(isnan(param_var_a_2.hi) || isinf(param_var_a_2.hi));
        }
        bool _16341 = false;
        if (_16340)
        {
            _16341 = param_var_a_2.lo <= param_var_a_2.hi;
        }
        bool _16342 = _16341;
        temp_var_logical_2 = _16342;
    }
    if (temp_var_logical_2)
    {
        Interval param_var_a_3 = a;
        float param_var_x = 0.0;
        if (interval_exact_point(param_var_a_3, param_var_x))
        {
            float param_var_x_1 = 0.0;
            float _16337 = param_var_x_1;
            float _16338 = param_var_x_1;
            Interval _16335;
            _16335.lo = _16337;
            _16335.hi = _16338;
            Interval _16336 = _16335;
            Interval _16339 = _16336;
            return _16339;
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
            float _16332 = as_type<float>(as_type<uint>(param_var_a_6.hi) ^ 2147483648u);
            float _16333 = as_type<float>(as_type<uint>(param_var_a_6.lo) ^ 2147483648u);
            Interval _16330;
            _16330.lo = _16332;
            _16330.hi = _16333;
            Interval _16331 = _16330;
            Interval _16334 = _16331;
            return _16334;
        }
    }
    float param_var_alo = a.lo;
    float param_var_ahi = a.hi;
    float param_var_blo = b.lo;
    float param_var_bhi = b.hi;
    float _16285 = param_var_alo;
    float _16286 = param_var_ahi;
    Interval _16282;
    _16282.lo = _16285;
    _16282.hi = _16286;
    Interval _16283 = _16282;
    Interval _16287 = _16283;
    bool _16279 = false;
    if (!(isnan(_16287.lo) || isinf(_16287.lo)))
    {
        _16279 = !(isnan(_16287.hi) || isinf(_16287.hi));
    }
    bool _16280 = false;
    if (_16279)
    {
        _16280 = _16287.lo <= _16287.hi;
    }
    bool _16281 = _16280;
    bool _16288 = false;
    if (_16281)
    {
        float _16289 = param_var_blo;
        float _16290 = param_var_bhi;
        Interval _16277;
        _16277.lo = _16289;
        _16277.hi = _16290;
        Interval _16278 = _16277;
        Interval _16291 = _16278;
        bool _16274 = false;
        if (!(isnan(_16291.lo) || isinf(_16291.lo)))
        {
            _16274 = !(isnan(_16291.hi) || isinf(_16291.hi));
        }
        bool _16275 = false;
        if (_16274)
        {
            _16275 = _16291.lo <= _16291.hi;
        }
        bool _16276 = _16275;
        _16288 = _16276;
    }
    bool _16284 = _16288;
    bool _16293 = false;
    if (_16284)
    {
        _16293 = as_type<uint>(param_var_alo) == as_type<uint>(param_var_ahi);
    }
    bool _16292 = _16293;
    bool _16295 = false;
    if (_16284)
    {
        _16295 = as_type<uint>(param_var_blo) == as_type<uint>(param_var_bhi);
    }
    bool _16294 = _16295;
    float _16297 = param_var_alo;
    float _16298 = param_var_blo;
    bool _16299 = false;
    float _16567 = quotient_bound(_16297, _16298, _16299, intervalFailed);
    float _16296 = _16567;
    float _16301 = param_var_alo;
    float _16302 = param_var_blo;
    bool _16303 = true;
    float _16570 = quotient_bound(_16301, _16302, _16303, intervalFailed);
    float _16300 = _16570;
    float _16304 = _16296;
    float _16305 = _16300;
    if (!_16294)
    {
        float _16306 = param_var_alo;
        float _16307 = param_var_bhi;
        bool _16308 = false;
        float _16579 = quotient_bound(_16306, _16307, _16308, intervalFailed);
        _16304 = _16579;
        float _16309 = param_var_alo;
        float _16310 = param_var_bhi;
        bool _16311 = true;
        float _16582 = quotient_bound(_16309, _16310, _16311, intervalFailed);
        _16305 = _16582;
    }
    float _16312 = _16296;
    float _16313 = _16300;
    if (!_16292)
    {
        float _16314 = param_var_ahi;
        float _16315 = param_var_blo;
        bool _16316 = false;
        float _16591 = quotient_bound(_16314, _16315, _16316, intervalFailed);
        _16312 = _16591;
        float _16317 = param_var_ahi;
        float _16318 = param_var_blo;
        bool _16319 = true;
        float _16594 = quotient_bound(_16317, _16318, _16319, intervalFailed);
        _16313 = _16594;
    }
    float _16320 = _16304;
    float _16321 = _16305;
    if (!_16292)
    {
        if (_16294)
        {
            _16320 = _16312;
            _16321 = _16313;
        }
        else
        {
            float _16322 = param_var_ahi;
            float _16323 = param_var_bhi;
            bool _16324 = false;
            float _16609 = quotient_bound(_16322, _16323, _16324, intervalFailed);
            _16320 = _16609;
            float _16325 = param_var_ahi;
            float _16326 = param_var_bhi;
            bool _16327 = true;
            float _16612 = quotient_bound(_16325, _16326, _16327, intervalFailed);
            _16321 = _16612;
        }
    }
    float _16328 = precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16296), _16304), _16312), _16320);
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16300), _16305), _16313), _16321);
    float _16329 = _16328;
    float lo = _16329;
    float param_var_x_4 = lo;
    float _16632 = interval_down(param_var_x_4, intervalFailed);
    float param_var_lo_1 = _16632;
    float param_var_x_5 = interval_divide_upper;
    float _16634 = interval_up(param_var_x_5, intervalFailed);
    float param_var_hi_1 = _16634;
    Interval _16272;
    _16272.lo = param_var_lo_1;
    _16272.hi = param_var_hi_1;
    Interval _16273 = _16272;
    return _16273;
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, device type_ByteAddressBuffer& frames, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        float param_var_x = feature.x;
        float _4254 = param_var_x;
        float _4255 = param_var_x;
        Interval _4252;
        _4252.lo = _4254;
        _4252.hi = _4255;
        Interval _4253 = _4252;
        Interval _4256 = _4253;
        Interval x = _4256;
        float param_var_x_1 = feature.y;
        float _4249 = param_var_x_1;
        float _4250 = param_var_x_1;
        Interval _4247;
        _4247.lo = _4249;
        _4247.hi = _4250;
        Interval _4248 = _4247;
        Interval _4251 = _4248;
        Interval y = _4251;
        float param_var_x_2 = 1.0;
        float _4244 = param_var_x_2;
        float _4245 = param_var_x_2;
        Interval _4242;
        _4242.lo = _4244;
        _4242.hi = _4245;
        Interval _4243 = _4242;
        Interval _4246 = _4243;
        Interval param_var_a = _4246;
        Interval param_var_a_1 = x;
        Interval param_var_b = y;
        Interval _4293 = iadd(param_var_a_1, param_var_b, intervalFailed);
        Interval param_var_b_1 = _4293;
        Interval _4238 = param_var_a;
        Interval _4239 = param_var_b_1;
        float _4235 = as_type<float>(as_type<uint>(_4239.hi) ^ 2147483648u);
        float _4236 = as_type<float>(as_type<uint>(_4239.lo) ^ 2147483648u);
        Interval _4233;
        _4233.lo = _4235;
        _4233.hi = _4236;
        Interval _4234 = _4233;
        Interval _4237 = _4234;
        Interval _4240 = _4237;
        Interval _4313 = iadd(_4238, _4240, intervalFailed);
        Interval _4241 = _4313;
        Interval a = _4241;
        uint _4316 = (at + 432u) >> 2u;
        float3 param_var_v = as_type<float4>(uint4(frames._m0[_4316], frames._m0[_4316 + 1u], frames._m0[_4316 + 2u], frames._m0[_4316 + 3u])).xyz;
        float _4226 = param_var_v.x;
        float _4223 = _4226;
        float _4224 = _4226;
        Interval _4221;
        _4221.lo = _4223;
        _4221.hi = _4224;
        Interval _4222 = _4221;
        Interval _4225 = _4222;
        Interval _4227 = _4225;
        float _4228 = param_var_v.y;
        float _4218 = _4228;
        float _4219 = _4228;
        Interval _4216;
        _4216.lo = _4218;
        _4216.hi = _4219;
        Interval _4217 = _4216;
        Interval _4220 = _4217;
        Interval _4229 = _4220;
        float _4230 = param_var_v.z;
        float _4213 = _4230;
        float _4214 = _4230;
        Interval _4211;
        _4211.lo = _4213;
        _4211.hi = _4214;
        Interval _4212 = _4211;
        Interval _4215 = _4212;
        Interval _4231 = _4215;
        Interval3 _4209;
        _4209.x = _4227;
        _4209.y = _4229;
        _4209.z = _4231;
        Interval3 _4210 = _4209;
        Interval3 _4232 = _4210;
        Interval3 param_var_a_2 = _4232;
        Interval param_var_b_2 = a;
        Interval _4199 = param_var_a_2.x;
        Interval _4200 = param_var_b_2;
        Interval _4374 = imul(_4199, _4200, intervalFailed, optical_product_upper);
        Interval _4201 = _4374;
        Interval _4202 = param_var_a_2.y;
        Interval _4203 = param_var_b_2;
        Interval _4378 = imul(_4202, _4203, intervalFailed, optical_product_upper);
        Interval _4204 = _4378;
        Interval _4205 = param_var_a_2.z;
        Interval _4206 = param_var_b_2;
        Interval _4382 = imul(_4205, _4206, intervalFailed, optical_product_upper);
        Interval _4207 = _4382;
        Interval3 _4197;
        _4197.x = _4201;
        _4197.y = _4204;
        _4197.z = _4207;
        Interval3 _4198 = _4197;
        Interval3 _4208 = _4198;
        Interval3 param_var_a_3 = _4208;
        uint _4393 = (at + 448u) >> 2u;
        float3 param_var_v_1 = as_type<float4>(uint4(frames._m0[_4393], frames._m0[_4393 + 1u], frames._m0[_4393 + 2u], frames._m0[_4393 + 3u])).xyz;
        float _4190 = param_var_v_1.x;
        float _4187 = _4190;
        float _4188 = _4190;
        Interval _4185;
        _4185.lo = _4187;
        _4185.hi = _4188;
        Interval _4186 = _4185;
        Interval _4189 = _4186;
        Interval _4191 = _4189;
        float _4192 = param_var_v_1.y;
        float _4182 = _4192;
        float _4183 = _4192;
        Interval _4180;
        _4180.lo = _4182;
        _4180.hi = _4183;
        Interval _4181 = _4180;
        Interval _4184 = _4181;
        Interval _4193 = _4184;
        float _4194 = param_var_v_1.z;
        float _4177 = _4194;
        float _4178 = _4194;
        Interval _4175;
        _4175.lo = _4177;
        _4175.hi = _4178;
        Interval _4176 = _4175;
        Interval _4179 = _4176;
        Interval _4195 = _4179;
        Interval3 _4173;
        _4173.x = _4191;
        _4173.y = _4193;
        _4173.z = _4195;
        Interval3 _4174 = _4173;
        Interval3 _4196 = _4174;
        Interval3 param_var_a_4 = _4196;
        Interval param_var_b_3 = x;
        Interval _4163 = param_var_a_4.x;
        Interval _4164 = param_var_b_3;
        Interval _4451 = imul(_4163, _4164, intervalFailed, optical_product_upper);
        Interval _4165 = _4451;
        Interval _4166 = param_var_a_4.y;
        Interval _4167 = param_var_b_3;
        Interval _4455 = imul(_4166, _4167, intervalFailed, optical_product_upper);
        Interval _4168 = _4455;
        Interval _4169 = param_var_a_4.z;
        Interval _4170 = param_var_b_3;
        Interval _4459 = imul(_4169, _4170, intervalFailed, optical_product_upper);
        Interval _4171 = _4459;
        Interval3 _4161;
        _4161.x = _4165;
        _4161.y = _4168;
        _4161.z = _4171;
        Interval3 _4162 = _4161;
        Interval3 _4172 = _4162;
        Interval3 param_var_b_4 = _4172;
        Interval _4151 = param_var_a_3.x;
        Interval _4152 = param_var_b_4.x;
        Interval _4473 = iadd(_4151, _4152, intervalFailed);
        Interval _4153 = _4473;
        Interval _4154 = param_var_a_3.y;
        Interval _4155 = param_var_b_4.y;
        Interval _4478 = iadd(_4154, _4155, intervalFailed);
        Interval _4156 = _4478;
        Interval _4157 = param_var_a_3.z;
        Interval _4158 = param_var_b_4.z;
        Interval _4483 = iadd(_4157, _4158, intervalFailed);
        Interval _4159 = _4483;
        Interval3 _4149;
        _4149.x = _4153;
        _4149.y = _4156;
        _4149.z = _4159;
        Interval3 _4150 = _4149;
        Interval3 _4160 = _4150;
        Interval3 param_var_a_5 = _4160;
        uint _4494 = (at + 464u) >> 2u;
        float3 param_var_v_2 = as_type<float4>(uint4(frames._m0[_4494], frames._m0[_4494 + 1u], frames._m0[_4494 + 2u], frames._m0[_4494 + 3u])).xyz;
        float _4142 = param_var_v_2.x;
        float _4139 = _4142;
        float _4140 = _4142;
        Interval _4137;
        _4137.lo = _4139;
        _4137.hi = _4140;
        Interval _4138 = _4137;
        Interval _4141 = _4138;
        Interval _4143 = _4141;
        float _4144 = param_var_v_2.y;
        float _4134 = _4144;
        float _4135 = _4144;
        Interval _4132;
        _4132.lo = _4134;
        _4132.hi = _4135;
        Interval _4133 = _4132;
        Interval _4136 = _4133;
        Interval _4145 = _4136;
        float _4146 = param_var_v_2.z;
        float _4129 = _4146;
        float _4130 = _4146;
        Interval _4127;
        _4127.lo = _4129;
        _4127.hi = _4130;
        Interval _4128 = _4127;
        Interval _4131 = _4128;
        Interval _4147 = _4131;
        Interval3 _4125;
        _4125.x = _4143;
        _4125.y = _4145;
        _4125.z = _4147;
        Interval3 _4126 = _4125;
        Interval3 _4148 = _4126;
        Interval3 param_var_a_6 = _4148;
        Interval param_var_b_5 = y;
        Interval _4115 = param_var_a_6.x;
        Interval _4116 = param_var_b_5;
        Interval _4552 = imul(_4115, _4116, intervalFailed, optical_product_upper);
        Interval _4117 = _4552;
        Interval _4118 = param_var_a_6.y;
        Interval _4119 = param_var_b_5;
        Interval _4556 = imul(_4118, _4119, intervalFailed, optical_product_upper);
        Interval _4120 = _4556;
        Interval _4121 = param_var_a_6.z;
        Interval _4122 = param_var_b_5;
        Interval _4560 = imul(_4121, _4122, intervalFailed, optical_product_upper);
        Interval _4123 = _4560;
        Interval3 _4113;
        _4113.x = _4117;
        _4113.y = _4120;
        _4113.z = _4123;
        Interval3 _4114 = _4113;
        Interval3 _4124 = _4114;
        Interval3 param_var_b_6 = _4124;
        Interval _4103 = param_var_a_5.x;
        Interval _4104 = param_var_b_6.x;
        Interval _4574 = iadd(_4103, _4104, intervalFailed);
        Interval _4105 = _4574;
        Interval _4106 = param_var_a_5.y;
        Interval _4107 = param_var_b_6.y;
        Interval _4579 = iadd(_4106, _4107, intervalFailed);
        Interval _4108 = _4579;
        Interval _4109 = param_var_a_5.z;
        Interval _4110 = param_var_b_6.z;
        Interval _4584 = iadd(_4109, _4110, intervalFailed);
        Interval _4111 = _4584;
        Interval3 _4101;
        _4101.x = _4105;
        _4101.y = _4108;
        _4101.z = _4111;
        Interval3 _4102 = _4101;
        Interval3 _4112 = _4102;
        return _4112;
    }
    float3 param_var_v_3 = feature.xyz;
    float _4094 = param_var_v_3.x;
    float _4091 = _4094;
    float _4092 = _4094;
    Interval _4089;
    _4089.lo = _4091;
    _4089.hi = _4092;
    Interval _4090 = _4089;
    Interval _4093 = _4090;
    Interval _4095 = _4093;
    float _4096 = param_var_v_3.y;
    float _4086 = _4096;
    float _4087 = _4096;
    Interval _4084;
    _4084.lo = _4086;
    _4084.hi = _4087;
    Interval _4085 = _4084;
    Interval _4088 = _4085;
    Interval _4097 = _4088;
    float _4098 = param_var_v_3.z;
    float _4081 = _4098;
    float _4082 = _4098;
    Interval _4079;
    _4079.lo = _4081;
    _4079.hi = _4082;
    Interval _4080 = _4079;
    Interval _4083 = _4080;
    Interval _4099 = _4083;
    Interval3 _4077;
    _4077.x = _4095;
    _4077.y = _4097;
    _4077.z = _4099;
    Interval3 _4078 = _4077;
    Interval3 _4100 = _4078;
    Interval3 raw = _4100;
    float3 param_var_v_4 = TargetSettings.targetCurrentCube[0].xyz;
    float _4070 = param_var_v_4.x;
    float _4067 = _4070;
    float _4068 = _4070;
    Interval _4065;
    _4065.lo = _4067;
    _4065.hi = _4068;
    Interval _4066 = _4065;
    Interval _4069 = _4066;
    Interval _4071 = _4069;
    float _4072 = param_var_v_4.y;
    float _4062 = _4072;
    float _4063 = _4072;
    Interval _4060;
    _4060.lo = _4062;
    _4060.hi = _4063;
    Interval _4061 = _4060;
    Interval _4064 = _4061;
    Interval _4073 = _4064;
    float _4074 = param_var_v_4.z;
    float _4057 = _4074;
    float _4058 = _4074;
    Interval _4055;
    _4055.lo = _4057;
    _4055.hi = _4058;
    Interval _4056 = _4055;
    Interval _4059 = _4056;
    Interval _4075 = _4059;
    Interval3 _4053;
    _4053.x = _4071;
    _4053.y = _4073;
    _4053.z = _4075;
    Interval3 _4054 = _4053;
    Interval3 _4076 = _4054;
    Interval3 param_var_a_7 = _4076;
    Interval3 param_var_b_7 = raw;
    Interval _4042 = param_var_a_7.x;
    Interval _4043 = param_var_b_7.x;
    Interval _4689 = imul(_4042, _4043, intervalFailed, optical_product_upper);
    Interval _4044 = _4689;
    Interval _4045 = param_var_a_7.y;
    Interval _4046 = param_var_b_7.y;
    Interval _4694 = imul(_4045, _4046, intervalFailed, optical_product_upper);
    Interval _4047 = _4694;
    Interval _4695 = iadd(_4044, _4047, intervalFailed);
    Interval _4048 = _4695;
    Interval _4049 = param_var_a_7.z;
    Interval _4050 = param_var_b_7.z;
    Interval _4700 = imul(_4049, _4050, intervalFailed, optical_product_upper);
    Interval _4051 = _4700;
    Interval _4701 = iadd(_4048, _4051, intervalFailed);
    Interval _4052 = _4701;
    Interval param_var_x_3 = _4052;
    float3 param_var_v_5 = TargetSettings.targetCurrentCube[1].xyz;
    float _4035 = param_var_v_5.x;
    float _4032 = _4035;
    float _4033 = _4035;
    Interval _4030;
    _4030.lo = _4032;
    _4030.hi = _4033;
    Interval _4031 = _4030;
    Interval _4034 = _4031;
    Interval _4036 = _4034;
    float _4037 = param_var_v_5.y;
    float _4027 = _4037;
    float _4028 = _4037;
    Interval _4025;
    _4025.lo = _4027;
    _4025.hi = _4028;
    Interval _4026 = _4025;
    Interval _4029 = _4026;
    Interval _4038 = _4029;
    float _4039 = param_var_v_5.z;
    float _4022 = _4039;
    float _4023 = _4039;
    Interval _4020;
    _4020.lo = _4022;
    _4020.hi = _4023;
    Interval _4021 = _4020;
    Interval _4024 = _4021;
    Interval _4040 = _4024;
    Interval3 _4018;
    _4018.x = _4036;
    _4018.y = _4038;
    _4018.z = _4040;
    Interval3 _4019 = _4018;
    Interval3 _4041 = _4019;
    Interval3 param_var_a_8 = _4041;
    Interval3 param_var_b_8 = raw;
    Interval _4007 = param_var_a_8.x;
    Interval _4008 = param_var_b_8.x;
    Interval _4754 = imul(_4007, _4008, intervalFailed, optical_product_upper);
    Interval _4009 = _4754;
    Interval _4010 = param_var_a_8.y;
    Interval _4011 = param_var_b_8.y;
    Interval _4759 = imul(_4010, _4011, intervalFailed, optical_product_upper);
    Interval _4012 = _4759;
    Interval _4760 = iadd(_4009, _4012, intervalFailed);
    Interval _4013 = _4760;
    Interval _4014 = param_var_a_8.z;
    Interval _4015 = param_var_b_8.z;
    Interval _4765 = imul(_4014, _4015, intervalFailed, optical_product_upper);
    Interval _4016 = _4765;
    Interval _4766 = iadd(_4013, _4016, intervalFailed);
    Interval _4017 = _4766;
    Interval param_var_y = _4017;
    float3 param_var_v_6 = TargetSettings.targetCurrentCube[2].xyz;
    float _4000 = param_var_v_6.x;
    float _3997 = _4000;
    float _3998 = _4000;
    Interval _3995;
    _3995.lo = _3997;
    _3995.hi = _3998;
    Interval _3996 = _3995;
    Interval _3999 = _3996;
    Interval _4001 = _3999;
    float _4002 = param_var_v_6.y;
    float _3992 = _4002;
    float _3993 = _4002;
    Interval _3990;
    _3990.lo = _3992;
    _3990.hi = _3993;
    Interval _3991 = _3990;
    Interval _3994 = _3991;
    Interval _4003 = _3994;
    float _4004 = param_var_v_6.z;
    float _3987 = _4004;
    float _3988 = _4004;
    Interval _3985;
    _3985.lo = _3987;
    _3985.hi = _3988;
    Interval _3986 = _3985;
    Interval _3989 = _3986;
    Interval _4005 = _3989;
    Interval3 _3983;
    _3983.x = _4001;
    _3983.y = _4003;
    _3983.z = _4005;
    Interval3 _3984 = _3983;
    Interval3 _4006 = _3984;
    Interval3 param_var_a_9 = _4006;
    Interval3 param_var_b_9 = raw;
    Interval _3972 = param_var_a_9.x;
    Interval _3973 = param_var_b_9.x;
    Interval _4819 = imul(_3972, _3973, intervalFailed, optical_product_upper);
    Interval _3974 = _4819;
    Interval _3975 = param_var_a_9.y;
    Interval _3976 = param_var_b_9.y;
    Interval _4824 = imul(_3975, _3976, intervalFailed, optical_product_upper);
    Interval _3977 = _4824;
    Interval _4825 = iadd(_3974, _3977, intervalFailed);
    Interval _3978 = _4825;
    Interval _3979 = param_var_a_9.z;
    Interval _3980 = param_var_b_9.z;
    Interval _4830 = imul(_3979, _3980, intervalFailed, optical_product_upper);
    Interval _3981 = _4830;
    Interval _4831 = iadd(_3978, _3981, intervalFailed);
    Interval _3982 = _4831;
    Interval param_var_z = _3982;
    Interval3 _3970;
    _3970.x = param_var_x_3;
    _3970.y = param_var_y;
    _3970.z = param_var_z;
    Interval3 _3971 = _3970;
    Interval3 world = _3971;
    float3 param_var_v_7 = float3(TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[2].x);
    float _3963 = param_var_v_7.x;
    float _3960 = _3963;
    float _3961 = _3963;
    Interval _3958;
    _3958.lo = _3960;
    _3958.hi = _3961;
    Interval _3959 = _3958;
    Interval _3962 = _3959;
    Interval _3964 = _3962;
    float _3965 = param_var_v_7.y;
    float _3955 = _3965;
    float _3956 = _3965;
    Interval _3953;
    _3953.lo = _3955;
    _3953.hi = _3956;
    Interval _3954 = _3953;
    Interval _3957 = _3954;
    Interval _3966 = _3957;
    float _3967 = param_var_v_7.z;
    float _3950 = _3967;
    float _3951 = _3967;
    Interval _3948;
    _3948.lo = _3950;
    _3948.hi = _3951;
    Interval _3949 = _3948;
    Interval _3952 = _3949;
    Interval _3968 = _3952;
    Interval3 _3946;
    _3946.x = _3964;
    _3946.y = _3966;
    _3946.z = _3968;
    Interval3 _3947 = _3946;
    Interval3 _3969 = _3947;
    Interval3 param_var_a_10 = _3969;
    Interval3 param_var_b_10 = world;
    Interval _3935 = param_var_a_10.x;
    Interval _3936 = param_var_b_10.x;
    Interval _4901 = imul(_3935, _3936, intervalFailed, optical_product_upper);
    Interval _3937 = _4901;
    Interval _3938 = param_var_a_10.y;
    Interval _3939 = param_var_b_10.y;
    Interval _4906 = imul(_3938, _3939, intervalFailed, optical_product_upper);
    Interval _3940 = _4906;
    Interval _4907 = iadd(_3937, _3940, intervalFailed);
    Interval _3941 = _4907;
    Interval _3942 = param_var_a_10.z;
    Interval _3943 = param_var_b_10.z;
    Interval _4912 = imul(_3942, _3943, intervalFailed, optical_product_upper);
    Interval _3944 = _4912;
    Interval _4913 = iadd(_3941, _3944, intervalFailed);
    Interval _3945 = _4913;
    Interval param_var_x_4 = _3945;
    float3 param_var_v_8 = float3(TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[2].y);
    float _3928 = param_var_v_8.x;
    float _3925 = _3928;
    float _3926 = _3928;
    Interval _3923;
    _3923.lo = _3925;
    _3923.hi = _3926;
    Interval _3924 = _3923;
    Interval _3927 = _3924;
    Interval _3929 = _3927;
    float _3930 = param_var_v_8.y;
    float _3920 = _3930;
    float _3921 = _3930;
    Interval _3918;
    _3918.lo = _3920;
    _3918.hi = _3921;
    Interval _3919 = _3918;
    Interval _3922 = _3919;
    Interval _3931 = _3922;
    float _3932 = param_var_v_8.z;
    float _3915 = _3932;
    float _3916 = _3932;
    Interval _3913;
    _3913.lo = _3915;
    _3913.hi = _3916;
    Interval _3914 = _3913;
    Interval _3917 = _3914;
    Interval _3933 = _3917;
    Interval3 _3911;
    _3911.x = _3929;
    _3911.y = _3931;
    _3911.z = _3933;
    Interval3 _3912 = _3911;
    Interval3 _3934 = _3912;
    Interval3 param_var_a_11 = _3934;
    Interval3 param_var_b_11 = world;
    Interval _3900 = param_var_a_11.x;
    Interval _3901 = param_var_b_11.x;
    Interval _4975 = imul(_3900, _3901, intervalFailed, optical_product_upper);
    Interval _3902 = _4975;
    Interval _3903 = param_var_a_11.y;
    Interval _3904 = param_var_b_11.y;
    Interval _4980 = imul(_3903, _3904, intervalFailed, optical_product_upper);
    Interval _3905 = _4980;
    Interval _4981 = iadd(_3902, _3905, intervalFailed);
    Interval _3906 = _4981;
    Interval _3907 = param_var_a_11.z;
    Interval _3908 = param_var_b_11.z;
    Interval _4986 = imul(_3907, _3908, intervalFailed, optical_product_upper);
    Interval _3909 = _4986;
    Interval _4987 = iadd(_3906, _3909, intervalFailed);
    Interval _3910 = _4987;
    Interval param_var_y_1 = _3910;
    float3 param_var_v_9 = float3(TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[2].z);
    float _3893 = param_var_v_9.x;
    float _3890 = _3893;
    float _3891 = _3893;
    Interval _3888;
    _3888.lo = _3890;
    _3888.hi = _3891;
    Interval _3889 = _3888;
    Interval _3892 = _3889;
    Interval _3894 = _3892;
    float _3895 = param_var_v_9.y;
    float _3885 = _3895;
    float _3886 = _3895;
    Interval _3883;
    _3883.lo = _3885;
    _3883.hi = _3886;
    Interval _3884 = _3883;
    Interval _3887 = _3884;
    Interval _3896 = _3887;
    float _3897 = param_var_v_9.z;
    float _3880 = _3897;
    float _3881 = _3897;
    Interval _3878;
    _3878.lo = _3880;
    _3878.hi = _3881;
    Interval _3879 = _3878;
    Interval _3882 = _3879;
    Interval _3898 = _3882;
    Interval3 _3876;
    _3876.x = _3894;
    _3876.y = _3896;
    _3876.z = _3898;
    Interval3 _3877 = _3876;
    Interval3 _3899 = _3877;
    Interval3 param_var_a_12 = _3899;
    Interval3 param_var_b_12 = world;
    Interval _3865 = param_var_a_12.x;
    Interval _3866 = param_var_b_12.x;
    Interval _5049 = imul(_3865, _3866, intervalFailed, optical_product_upper);
    Interval _3867 = _5049;
    Interval _3868 = param_var_a_12.y;
    Interval _3869 = param_var_b_12.y;
    Interval _5054 = imul(_3868, _3869, intervalFailed, optical_product_upper);
    Interval _3870 = _5054;
    Interval _5055 = iadd(_3867, _3870, intervalFailed);
    Interval _3871 = _5055;
    Interval _3872 = param_var_a_12.z;
    Interval _3873 = param_var_b_12.z;
    Interval _5060 = imul(_3872, _3873, intervalFailed, optical_product_upper);
    Interval _3874 = _5060;
    Interval _5061 = iadd(_3871, _3874, intervalFailed);
    Interval _3875 = _5061;
    Interval param_var_z_1 = _3875;
    Interval3 _3863;
    _3863.x = param_var_x_4;
    _3863.y = param_var_y_1;
    _3863.z = param_var_z_1;
    Interval3 _3864 = _3863;
    Interval3 param_var_a_13 = _3864;
    Interval3 _3856 = param_var_a_13;
    float _3857 = 1.0;
    float _3853 = _3857;
    float _3854 = _3857;
    Interval _3851;
    _3851.lo = _3853;
    _3851.hi = _3854;
    Interval _3852 = _3851;
    Interval _3855 = _3852;
    Interval _3858 = _3855;
    Interval3 _3859 = param_var_a_13;
    Interval _3842 = _3859.x;
    bool _3835 = false;
    if (_3842.lo <= 0.0)
    {
        _3835 = _3842.hi >= 0.0;
    }
    float _3834;
    if (_3835)
    {
        _3834 = 0.0;
    }
    else
    {
        _3834 = precise::min(abs(_3842.lo), abs(_3842.hi));
    }
    float _3833 = _3834;
    float _3836 = precise::max(abs(_3842.lo), abs(_3842.hi));
    float _3837 = spvFMul(_3833, _3833);
    float _5113 = interval_down(_3837, intervalFailed);
    float _3838 = precise::max(0.0, _5113);
    float _3839 = spvFMul(_3836, _3836);
    float _5117 = interval_up(_3839, intervalFailed);
    float _3840 = _5117;
    Interval _3831;
    _3831.lo = _3838;
    _3831.hi = _3840;
    Interval _3832 = _3831;
    Interval _3841 = _3832;
    Interval _3843 = _3841;
    Interval _3844 = _3859.y;
    bool _3824 = false;
    if (_3844.lo <= 0.0)
    {
        _3824 = _3844.hi >= 0.0;
    }
    float _3823;
    if (_3824)
    {
        _3823 = 0.0;
    }
    else
    {
        _3823 = precise::min(abs(_3844.lo), abs(_3844.hi));
    }
    float _3822 = _3823;
    float _3825 = precise::max(abs(_3844.lo), abs(_3844.hi));
    float _3826 = spvFMul(_3822, _3822);
    float _5156 = interval_down(_3826, intervalFailed);
    float _3827 = precise::max(0.0, _5156);
    float _3828 = spvFMul(_3825, _3825);
    float _5160 = interval_up(_3828, intervalFailed);
    float _3829 = _5160;
    Interval _3820;
    _3820.lo = _3827;
    _3820.hi = _3829;
    Interval _3821 = _3820;
    Interval _3830 = _3821;
    Interval _3845 = _3830;
    Interval _5168 = iadd(_3843, _3845, intervalFailed);
    Interval _3846 = _5168;
    Interval _3847 = _3859.z;
    bool _3813 = false;
    if (_3847.lo <= 0.0)
    {
        _3813 = _3847.hi >= 0.0;
    }
    float _3812;
    if (_3813)
    {
        _3812 = 0.0;
    }
    else
    {
        _3812 = precise::min(abs(_3847.lo), abs(_3847.hi));
    }
    float _3811 = _3812;
    float _3814 = precise::max(abs(_3847.lo), abs(_3847.hi));
    float _3815 = spvFMul(_3811, _3811);
    float _5200 = interval_down(_3815, intervalFailed);
    float _3816 = precise::max(0.0, _5200);
    float _3817 = spvFMul(_3814, _3814);
    float _5204 = interval_up(_3817, intervalFailed);
    float _3818 = _5204;
    Interval _3809;
    _3809.lo = _3816;
    _3809.hi = _3818;
    Interval _3810 = _3809;
    Interval _3819 = _3810;
    Interval _3848 = _3819;
    Interval _5212 = iadd(_3846, _3848, intervalFailed);
    Interval _3849 = _5212;
    Interval _5213 = isqrt(_3849, intervalFailed);
    Interval _3850 = _5213;
    Interval _3860 = _3850;
    Interval _5215 = idiv(_3858, _3860, intervalFailed, interval_divide_upper);
    Interval _3861 = _5215;
    Interval _3799 = _3856.x;
    Interval _3800 = _3861;
    Interval _5219 = imul(_3799, _3800, intervalFailed, optical_product_upper);
    Interval _3801 = _5219;
    Interval _3802 = _3856.y;
    Interval _3803 = _3861;
    Interval _5223 = imul(_3802, _3803, intervalFailed, optical_product_upper);
    Interval _3804 = _5223;
    Interval _3805 = _3856.z;
    Interval _3806 = _3861;
    Interval _5227 = imul(_3805, _3806, intervalFailed, optical_product_upper);
    Interval _3807 = _5227;
    Interval3 _3797;
    _3797.x = _3801;
    _3797.y = _3804;
    _3797.z = _3807;
    Interval3 _3798 = _3797;
    Interval3 _3808 = _3798;
    Interval3 _3862 = _3808;
    return _3862;
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
    bool _17020 = false;
    if (param_var_p.a.w == 2.0)
    {
        _17020 = param_var_p.b.w == 2.0;
    }
    bool _17021 = false;
    if (_17020)
    {
        _17021 = param_var_p.c.w == 2.0;
    }
    bool _17022 = _17021;
    if (_17022)
    {
        float3 param_var_v = p.b.xyz;
        float _17013 = param_var_v.x;
        float _17010 = _17013;
        float _17011 = _17013;
        Interval _17008;
        _17008.lo = _17010;
        _17008.hi = _17011;
        Interval _17009 = _17008;
        Interval _17012 = _17009;
        Interval _17014 = _17012;
        float _17015 = param_var_v.y;
        float _17005 = _17015;
        float _17006 = _17015;
        Interval _17003;
        _17003.lo = _17005;
        _17003.hi = _17006;
        Interval _17004 = _17003;
        Interval _17007 = _17004;
        Interval _17016 = _17007;
        float _17017 = param_var_v.z;
        float _17000 = _17017;
        float _17001 = _17017;
        Interval _16998;
        _16998.lo = _17000;
        _16998.hi = _17001;
        Interval _16999 = _16998;
        Interval _17002 = _16999;
        Interval _17018 = _17002;
        Interval3 _16996;
        _16996.x = _17014;
        _16996.y = _17016;
        _16996.z = _17018;
        Interval3 _16997 = _16996;
        Interval3 _17019 = _16997;
        Interval3 param_var_a = _17019;
        Interval3 _16989 = param_var_a;
        float _16990 = 1.0;
        float _16986 = _16990;
        float _16987 = _16990;
        Interval _16984;
        _16984.lo = _16986;
        _16984.hi = _16987;
        Interval _16985 = _16984;
        Interval _16988 = _16985;
        Interval _16991 = _16988;
        Interval3 _16992 = param_var_a;
        Interval _16975 = _16992.x;
        bool _16968 = false;
        if (_16975.lo <= 0.0)
        {
            _16968 = _16975.hi >= 0.0;
        }
        float _16967;
        if (_16968)
        {
            _16967 = 0.0;
        }
        else
        {
            _16967 = precise::min(abs(_16975.lo), abs(_16975.hi));
        }
        float _16966 = _16967;
        float _16969 = precise::max(abs(_16975.lo), abs(_16975.hi));
        float _16970 = spvFMul(_16966, _16966);
        float _17130 = interval_down(_16970, intervalFailed);
        float _16971 = precise::max(0.0, _17130);
        float _16972 = spvFMul(_16969, _16969);
        float _17134 = interval_up(_16972, intervalFailed);
        float _16973 = _17134;
        Interval _16964;
        _16964.lo = _16971;
        _16964.hi = _16973;
        Interval _16965 = _16964;
        Interval _16974 = _16965;
        Interval _16976 = _16974;
        Interval _16977 = _16992.y;
        bool _16957 = false;
        if (_16977.lo <= 0.0)
        {
            _16957 = _16977.hi >= 0.0;
        }
        float _16956;
        if (_16957)
        {
            _16956 = 0.0;
        }
        else
        {
            _16956 = precise::min(abs(_16977.lo), abs(_16977.hi));
        }
        float _16955 = _16956;
        float _16958 = precise::max(abs(_16977.lo), abs(_16977.hi));
        float _16959 = spvFMul(_16955, _16955);
        float _17173 = interval_down(_16959, intervalFailed);
        float _16960 = precise::max(0.0, _17173);
        float _16961 = spvFMul(_16958, _16958);
        float _17177 = interval_up(_16961, intervalFailed);
        float _16962 = _17177;
        Interval _16953;
        _16953.lo = _16960;
        _16953.hi = _16962;
        Interval _16954 = _16953;
        Interval _16963 = _16954;
        Interval _16978 = _16963;
        Interval _17185 = iadd(_16976, _16978, intervalFailed);
        Interval _16979 = _17185;
        Interval _16980 = _16992.z;
        bool _16946 = false;
        if (_16980.lo <= 0.0)
        {
            _16946 = _16980.hi >= 0.0;
        }
        float _16945;
        if (_16946)
        {
            _16945 = 0.0;
        }
        else
        {
            _16945 = precise::min(abs(_16980.lo), abs(_16980.hi));
        }
        float _16944 = _16945;
        float _16947 = precise::max(abs(_16980.lo), abs(_16980.hi));
        float _16948 = spvFMul(_16944, _16944);
        float _17217 = interval_down(_16948, intervalFailed);
        float _16949 = precise::max(0.0, _17217);
        float _16950 = spvFMul(_16947, _16947);
        float _17221 = interval_up(_16950, intervalFailed);
        float _16951 = _17221;
        Interval _16942;
        _16942.lo = _16949;
        _16942.hi = _16951;
        Interval _16943 = _16942;
        Interval _16952 = _16943;
        Interval _16981 = _16952;
        Interval _17229 = iadd(_16979, _16981, intervalFailed);
        Interval _16982 = _17229;
        Interval _17230 = isqrt(_16982, intervalFailed);
        Interval _16983 = _17230;
        Interval _16993 = _16983;
        Interval _17232 = idiv(_16991, _16993, intervalFailed, interval_divide_upper);
        Interval _16994 = _17232;
        Interval _16932 = _16989.x;
        Interval _16933 = _16994;
        Interval _17236 = imul(_16932, _16933, intervalFailed, optical_product_upper);
        Interval _16934 = _17236;
        Interval _16935 = _16989.y;
        Interval _16936 = _16994;
        Interval _17240 = imul(_16935, _16936, intervalFailed, optical_product_upper);
        Interval _16937 = _17240;
        Interval _16938 = _16989.z;
        Interval _16939 = _16994;
        Interval _17244 = imul(_16938, _16939, intervalFailed, optical_product_upper);
        Interval _16940 = _17244;
        Interval3 _16930;
        _16930.x = _16934;
        _16930.y = _16937;
        _16930.z = _16940;
        Interval3 _16931 = _16930;
        Interval3 _16941 = _16931;
        Interval3 _16995 = _16941;
        return _16995;
    }
    float3 param_var_v_1 = p.b.xyz;
    float _16923 = param_var_v_1.x;
    float _16920 = _16923;
    float _16921 = _16923;
    Interval _16918;
    _16918.lo = _16920;
    _16918.hi = _16921;
    Interval _16919 = _16918;
    Interval _16922 = _16919;
    Interval _16924 = _16922;
    float _16925 = param_var_v_1.y;
    float _16915 = _16925;
    float _16916 = _16925;
    Interval _16913;
    _16913.lo = _16915;
    _16913.hi = _16916;
    Interval _16914 = _16913;
    Interval _16917 = _16914;
    Interval _16926 = _16917;
    float _16927 = param_var_v_1.z;
    float _16910 = _16927;
    float _16911 = _16927;
    Interval _16908;
    _16908.lo = _16910;
    _16908.hi = _16911;
    Interval _16909 = _16908;
    Interval _16912 = _16909;
    Interval _16928 = _16912;
    Interval3 _16906;
    _16906.x = _16924;
    _16906.y = _16926;
    _16906.z = _16928;
    Interval3 _16907 = _16906;
    Interval3 _16929 = _16907;
    Interval3 param_var_a_1 = _16929;
    float3 param_var_v_2 = p.a.xyz;
    float _16899 = param_var_v_2.x;
    float _16896 = _16899;
    float _16897 = _16899;
    Interval _16894;
    _16894.lo = _16896;
    _16894.hi = _16897;
    Interval _16895 = _16894;
    Interval _16898 = _16895;
    Interval _16900 = _16898;
    float _16901 = param_var_v_2.y;
    float _16891 = _16901;
    float _16892 = _16901;
    Interval _16889;
    _16889.lo = _16891;
    _16889.hi = _16892;
    Interval _16890 = _16889;
    Interval _16893 = _16890;
    Interval _16902 = _16893;
    float _16903 = param_var_v_2.z;
    float _16886 = _16903;
    float _16887 = _16903;
    Interval _16884;
    _16884.lo = _16886;
    _16884.hi = _16887;
    Interval _16885 = _16884;
    Interval _16888 = _16885;
    Interval _16904 = _16888;
    Interval3 _16882;
    _16882.x = _16900;
    _16882.y = _16902;
    _16882.z = _16904;
    Interval3 _16883 = _16882;
    Interval3 _16905 = _16883;
    Interval3 param_var_b = _16905;
    Interval3 _16873 = param_var_a_1;
    Interval _16874 = param_var_b.x;
    float _16870 = as_type<float>(as_type<uint>(_16874.hi) ^ 2147483648u);
    float _16871 = as_type<float>(as_type<uint>(_16874.lo) ^ 2147483648u);
    Interval _16868;
    _16868.lo = _16870;
    _16868.hi = _16871;
    Interval _16869 = _16868;
    Interval _16872 = _16869;
    Interval _16875 = _16872;
    Interval _16876 = param_var_b.y;
    float _16865 = as_type<float>(as_type<uint>(_16876.hi) ^ 2147483648u);
    float _16866 = as_type<float>(as_type<uint>(_16876.lo) ^ 2147483648u);
    Interval _16863;
    _16863.lo = _16865;
    _16863.hi = _16866;
    Interval _16864 = _16863;
    Interval _16867 = _16864;
    Interval _16877 = _16867;
    Interval _16878 = param_var_b.z;
    float _16860 = as_type<float>(as_type<uint>(_16878.hi) ^ 2147483648u);
    float _16861 = as_type<float>(as_type<uint>(_16878.lo) ^ 2147483648u);
    Interval _16858;
    _16858.lo = _16860;
    _16858.hi = _16861;
    Interval _16859 = _16858;
    Interval _16862 = _16859;
    Interval _16879 = _16862;
    Interval3 _16856;
    _16856.x = _16875;
    _16856.y = _16877;
    _16856.z = _16879;
    Interval3 _16857 = _16856;
    Interval3 _16880 = _16857;
    Interval _16846 = _16873.x;
    Interval _16847 = _16880.x;
    Interval _17415 = iadd(_16846, _16847, intervalFailed);
    Interval _16848 = _17415;
    Interval _16849 = _16873.y;
    Interval _16850 = _16880.y;
    Interval _17420 = iadd(_16849, _16850, intervalFailed);
    Interval _16851 = _17420;
    Interval _16852 = _16873.z;
    Interval _16853 = _16880.z;
    Interval _17425 = iadd(_16852, _16853, intervalFailed);
    Interval _16854 = _17425;
    Interval3 _16844;
    _16844.x = _16848;
    _16844.y = _16851;
    _16844.z = _16854;
    Interval3 _16845 = _16844;
    Interval3 _16855 = _16845;
    Interval3 _16881 = _16855;
    Interval3 param_var_a_2 = _16881;
    float3 param_var_v_3 = p.c.xyz;
    float _16837 = param_var_v_3.x;
    float _16834 = _16837;
    float _16835 = _16837;
    Interval _16832;
    _16832.lo = _16834;
    _16832.hi = _16835;
    Interval _16833 = _16832;
    Interval _16836 = _16833;
    Interval _16838 = _16836;
    float _16839 = param_var_v_3.y;
    float _16829 = _16839;
    float _16830 = _16839;
    Interval _16827;
    _16827.lo = _16829;
    _16827.hi = _16830;
    Interval _16828 = _16827;
    Interval _16831 = _16828;
    Interval _16840 = _16831;
    float _16841 = param_var_v_3.z;
    float _16824 = _16841;
    float _16825 = _16841;
    Interval _16822;
    _16822.lo = _16824;
    _16822.hi = _16825;
    Interval _16823 = _16822;
    Interval _16826 = _16823;
    Interval _16842 = _16826;
    Interval3 _16820;
    _16820.x = _16838;
    _16820.y = _16840;
    _16820.z = _16842;
    Interval3 _16821 = _16820;
    Interval3 _16843 = _16821;
    Interval3 param_var_a_3 = _16843;
    float3 param_var_v_4 = p.a.xyz;
    float _16813 = param_var_v_4.x;
    float _16810 = _16813;
    float _16811 = _16813;
    Interval _16808;
    _16808.lo = _16810;
    _16808.hi = _16811;
    Interval _16809 = _16808;
    Interval _16812 = _16809;
    Interval _16814 = _16812;
    float _16815 = param_var_v_4.y;
    float _16805 = _16815;
    float _16806 = _16815;
    Interval _16803;
    _16803.lo = _16805;
    _16803.hi = _16806;
    Interval _16804 = _16803;
    Interval _16807 = _16804;
    Interval _16816 = _16807;
    float _16817 = param_var_v_4.z;
    float _16800 = _16817;
    float _16801 = _16817;
    Interval _16798;
    _16798.lo = _16800;
    _16798.hi = _16801;
    Interval _16799 = _16798;
    Interval _16802 = _16799;
    Interval _16818 = _16802;
    Interval3 _16796;
    _16796.x = _16814;
    _16796.y = _16816;
    _16796.z = _16818;
    Interval3 _16797 = _16796;
    Interval3 _16819 = _16797;
    Interval3 param_var_b_1 = _16819;
    Interval3 _16787 = param_var_a_3;
    Interval _16788 = param_var_b_1.x;
    float _16784 = as_type<float>(as_type<uint>(_16788.hi) ^ 2147483648u);
    float _16785 = as_type<float>(as_type<uint>(_16788.lo) ^ 2147483648u);
    Interval _16782;
    _16782.lo = _16784;
    _16782.hi = _16785;
    Interval _16783 = _16782;
    Interval _16786 = _16783;
    Interval _16789 = _16786;
    Interval _16790 = param_var_b_1.y;
    float _16779 = as_type<float>(as_type<uint>(_16790.hi) ^ 2147483648u);
    float _16780 = as_type<float>(as_type<uint>(_16790.lo) ^ 2147483648u);
    Interval _16777;
    _16777.lo = _16779;
    _16777.hi = _16780;
    Interval _16778 = _16777;
    Interval _16781 = _16778;
    Interval _16791 = _16781;
    Interval _16792 = param_var_b_1.z;
    float _16774 = as_type<float>(as_type<uint>(_16792.hi) ^ 2147483648u);
    float _16775 = as_type<float>(as_type<uint>(_16792.lo) ^ 2147483648u);
    Interval _16772;
    _16772.lo = _16774;
    _16772.hi = _16775;
    Interval _16773 = _16772;
    Interval _16776 = _16773;
    Interval _16793 = _16776;
    Interval3 _16770;
    _16770.x = _16789;
    _16770.y = _16791;
    _16770.z = _16793;
    Interval3 _16771 = _16770;
    Interval3 _16794 = _16771;
    Interval _16760 = _16787.x;
    Interval _16761 = _16794.x;
    Interval _17596 = iadd(_16760, _16761, intervalFailed);
    Interval _16762 = _17596;
    Interval _16763 = _16787.y;
    Interval _16764 = _16794.y;
    Interval _17601 = iadd(_16763, _16764, intervalFailed);
    Interval _16765 = _17601;
    Interval _16766 = _16787.z;
    Interval _16767 = _16794.z;
    Interval _17606 = iadd(_16766, _16767, intervalFailed);
    Interval _16768 = _17606;
    Interval3 _16758;
    _16758.x = _16762;
    _16758.y = _16765;
    _16758.z = _16768;
    Interval3 _16759 = _16758;
    Interval3 _16769 = _16759;
    Interval3 _16795 = _16769;
    Interval3 param_var_b_2 = _16795;
    Interval _16736 = param_var_a_2.y;
    Interval _16737 = param_var_b_2.z;
    Interval _17621 = imul(_16736, _16737, intervalFailed, optical_product_upper);
    Interval _16738 = _17621;
    Interval _16739 = param_var_a_2.z;
    Interval _16740 = param_var_b_2.y;
    Interval _17626 = imul(_16739, _16740, intervalFailed, optical_product_upper);
    Interval _16741 = _17626;
    Interval _16732 = _16738;
    Interval _16733 = _16741;
    float _16729 = as_type<float>(as_type<uint>(_16733.hi) ^ 2147483648u);
    float _16730 = as_type<float>(as_type<uint>(_16733.lo) ^ 2147483648u);
    Interval _16727;
    _16727.lo = _16729;
    _16727.hi = _16730;
    Interval _16728 = _16727;
    Interval _16731 = _16728;
    Interval _16734 = _16731;
    Interval _17646 = iadd(_16732, _16734, intervalFailed);
    Interval _16735 = _17646;
    Interval _16742 = _16735;
    Interval _16743 = param_var_a_2.z;
    Interval _16744 = param_var_b_2.x;
    Interval _17652 = imul(_16743, _16744, intervalFailed, optical_product_upper);
    Interval _16745 = _17652;
    Interval _16746 = param_var_a_2.x;
    Interval _16747 = param_var_b_2.z;
    Interval _17657 = imul(_16746, _16747, intervalFailed, optical_product_upper);
    Interval _16748 = _17657;
    Interval _16723 = _16745;
    Interval _16724 = _16748;
    float _16720 = as_type<float>(as_type<uint>(_16724.hi) ^ 2147483648u);
    float _16721 = as_type<float>(as_type<uint>(_16724.lo) ^ 2147483648u);
    Interval _16718;
    _16718.lo = _16720;
    _16718.hi = _16721;
    Interval _16719 = _16718;
    Interval _16722 = _16719;
    Interval _16725 = _16722;
    Interval _17677 = iadd(_16723, _16725, intervalFailed);
    Interval _16726 = _17677;
    Interval _16749 = _16726;
    Interval _16750 = param_var_a_2.x;
    Interval _16751 = param_var_b_2.y;
    Interval _17683 = imul(_16750, _16751, intervalFailed, optical_product_upper);
    Interval _16752 = _17683;
    Interval _16753 = param_var_a_2.y;
    Interval _16754 = param_var_b_2.x;
    Interval _17688 = imul(_16753, _16754, intervalFailed, optical_product_upper);
    Interval _16755 = _17688;
    Interval _16714 = _16752;
    Interval _16715 = _16755;
    float _16711 = as_type<float>(as_type<uint>(_16715.hi) ^ 2147483648u);
    float _16712 = as_type<float>(as_type<uint>(_16715.lo) ^ 2147483648u);
    Interval _16709;
    _16709.lo = _16711;
    _16709.hi = _16712;
    Interval _16710 = _16709;
    Interval _16713 = _16710;
    Interval _16716 = _16713;
    Interval _17708 = iadd(_16714, _16716, intervalFailed);
    Interval _16717 = _17708;
    Interval _16756 = _16717;
    Interval3 _16707;
    _16707.x = _16742;
    _16707.y = _16749;
    _16707.z = _16756;
    Interval3 _16708 = _16707;
    Interval3 _16757 = _16708;
    Interval3 param_var_a_4 = _16757;
    Interval3 _16700 = param_var_a_4;
    float _16701 = 1.0;
    float _16697 = _16701;
    float _16698 = _16701;
    Interval _16695;
    _16695.lo = _16697;
    _16695.hi = _16698;
    Interval _16696 = _16695;
    Interval _16699 = _16696;
    Interval _16702 = _16699;
    Interval3 _16703 = param_var_a_4;
    Interval _16686 = _16703.x;
    bool _16679 = false;
    if (_16686.lo <= 0.0)
    {
        _16679 = _16686.hi >= 0.0;
    }
    float _16678;
    if (_16679)
    {
        _16678 = 0.0;
    }
    else
    {
        _16678 = precise::min(abs(_16686.lo), abs(_16686.hi));
    }
    float _16677 = _16678;
    float _16680 = precise::max(abs(_16686.lo), abs(_16686.hi));
    float _16681 = spvFMul(_16677, _16677);
    float _17761 = interval_down(_16681, intervalFailed);
    float _16682 = precise::max(0.0, _17761);
    float _16683 = spvFMul(_16680, _16680);
    float _17765 = interval_up(_16683, intervalFailed);
    float _16684 = _17765;
    Interval _16675;
    _16675.lo = _16682;
    _16675.hi = _16684;
    Interval _16676 = _16675;
    Interval _16685 = _16676;
    Interval _16687 = _16685;
    Interval _16688 = _16703.y;
    bool _16668 = false;
    if (_16688.lo <= 0.0)
    {
        _16668 = _16688.hi >= 0.0;
    }
    float _16667;
    if (_16668)
    {
        _16667 = 0.0;
    }
    else
    {
        _16667 = precise::min(abs(_16688.lo), abs(_16688.hi));
    }
    float _16666 = _16667;
    float _16669 = precise::max(abs(_16688.lo), abs(_16688.hi));
    float _16670 = spvFMul(_16666, _16666);
    float _17804 = interval_down(_16670, intervalFailed);
    float _16671 = precise::max(0.0, _17804);
    float _16672 = spvFMul(_16669, _16669);
    float _17808 = interval_up(_16672, intervalFailed);
    float _16673 = _17808;
    Interval _16664;
    _16664.lo = _16671;
    _16664.hi = _16673;
    Interval _16665 = _16664;
    Interval _16674 = _16665;
    Interval _16689 = _16674;
    Interval _17816 = iadd(_16687, _16689, intervalFailed);
    Interval _16690 = _17816;
    Interval _16691 = _16703.z;
    bool _16657 = false;
    if (_16691.lo <= 0.0)
    {
        _16657 = _16691.hi >= 0.0;
    }
    float _16656;
    if (_16657)
    {
        _16656 = 0.0;
    }
    else
    {
        _16656 = precise::min(abs(_16691.lo), abs(_16691.hi));
    }
    float _16655 = _16656;
    float _16658 = precise::max(abs(_16691.lo), abs(_16691.hi));
    float _16659 = spvFMul(_16655, _16655);
    float _17848 = interval_down(_16659, intervalFailed);
    float _16660 = precise::max(0.0, _17848);
    float _16661 = spvFMul(_16658, _16658);
    float _17852 = interval_up(_16661, intervalFailed);
    float _16662 = _17852;
    Interval _16653;
    _16653.lo = _16660;
    _16653.hi = _16662;
    Interval _16654 = _16653;
    Interval _16663 = _16654;
    Interval _16692 = _16663;
    Interval _17860 = iadd(_16690, _16692, intervalFailed);
    Interval _16693 = _17860;
    Interval _17861 = isqrt(_16693, intervalFailed);
    Interval _16694 = _17861;
    Interval _16704 = _16694;
    Interval _17863 = idiv(_16702, _16704, intervalFailed, interval_divide_upper);
    Interval _16705 = _17863;
    Interval _16643 = _16700.x;
    Interval _16644 = _16705;
    Interval _17867 = imul(_16643, _16644, intervalFailed, optical_product_upper);
    Interval _16645 = _17867;
    Interval _16646 = _16700.y;
    Interval _16647 = _16705;
    Interval _17871 = imul(_16646, _16647, intervalFailed, optical_product_upper);
    Interval _16648 = _17871;
    Interval _16649 = _16700.z;
    Interval _16650 = _16705;
    Interval _17875 = imul(_16649, _16650, intervalFailed, optical_product_upper);
    Interval _16651 = _17875;
    Interval3 _16641;
    _16641.x = _16645;
    _16641.y = _16648;
    _16641.z = _16651;
    Interval3 _16642 = _16641;
    Interval3 _16652 = _16642;
    Interval3 _16706 = _16652;
    return _16706;
}

static inline __attribute__((always_inline))
Interval3 oriented(thread const Interval3& n, thread const Interval3& direction, thread bool& intervalFailed, thread float& optical_product_upper)
{
    Interval3 param_var_a = n;
    Interval3 param_var_b = direction;
    Interval _17947 = param_var_a.x;
    Interval _17948 = param_var_b.x;
    Interval _17964 = imul(_17947, _17948, intervalFailed, optical_product_upper);
    Interval _17949 = _17964;
    Interval _17950 = param_var_a.y;
    Interval _17951 = param_var_b.y;
    Interval _17969 = imul(_17950, _17951, intervalFailed, optical_product_upper);
    Interval _17952 = _17969;
    Interval _17970 = iadd(_17949, _17952, intervalFailed);
    Interval _17953 = _17970;
    Interval _17954 = param_var_a.z;
    Interval _17955 = param_var_b.z;
    Interval _17975 = imul(_17954, _17955, intervalFailed, optical_product_upper);
    Interval _17956 = _17975;
    Interval _17976 = iadd(_17953, _17956, intervalFailed);
    Interval _17957 = _17976;
    Interval _dot = _17957;
    if (_dot.lo > 0.0)
    {
        Interval3 param_var_a_1 = n;
        float param_var_x = -1.0;
        float _17944 = param_var_x;
        float _17945 = param_var_x;
        Interval _17942;
        _17942.lo = _17944;
        _17942.hi = _17945;
        Interval _17943 = _17942;
        Interval _17946 = _17943;
        Interval param_var_b_1 = _17946;
        Interval _17932 = param_var_a_1.x;
        Interval _17933 = param_var_b_1;
        Interval _17994 = imul(_17932, _17933, intervalFailed, optical_product_upper);
        Interval _17934 = _17994;
        Interval _17935 = param_var_a_1.y;
        Interval _17936 = param_var_b_1;
        Interval _17998 = imul(_17935, _17936, intervalFailed, optical_product_upper);
        Interval _17937 = _17998;
        Interval _17938 = param_var_a_1.z;
        Interval _17939 = param_var_b_1;
        Interval _18002 = imul(_17938, _17939, intervalFailed, optical_product_upper);
        Interval _17940 = _18002;
        Interval3 _17930;
        _17930.x = _17934;
        _17930.y = _17937;
        _17930.z = _17940;
        Interval3 _17931 = _17930;
        Interval3 _17941 = _17931;
        return _17941;
    }
    if (_dot.hi <= 0.0)
    {
        return n;
    }
    Interval3 param_var_a_2 = n;
    Interval3 param_var_a_3 = n;
    float param_var_x_1 = -1.0;
    float _17927 = param_var_x_1;
    float _17928 = param_var_x_1;
    Interval _17925;
    _17925.lo = _17927;
    _17925.hi = _17928;
    Interval _17926 = _17925;
    Interval _17929 = _17926;
    Interval param_var_b_2 = _17929;
    Interval _17915 = param_var_a_3.x;
    Interval _17916 = param_var_b_2;
    Interval _18030 = imul(_17915, _17916, intervalFailed, optical_product_upper);
    Interval _17917 = _18030;
    Interval _17918 = param_var_a_3.y;
    Interval _17919 = param_var_b_2;
    Interval _18034 = imul(_17918, _17919, intervalFailed, optical_product_upper);
    Interval _17920 = _18034;
    Interval _17921 = param_var_a_3.z;
    Interval _17922 = param_var_b_2;
    Interval _18038 = imul(_17921, _17922, intervalFailed, optical_product_upper);
    Interval _17923 = _18038;
    Interval3 _17913;
    _17913.x = _17917;
    _17913.y = _17920;
    _17913.z = _17923;
    Interval3 _17914 = _17913;
    Interval3 _17924 = _17914;
    Interval3 param_var_b_3 = _17924;
    Interval _17903 = param_var_a_2.x;
    Interval _17904 = param_var_b_3.x;
    float _17900 = precise::min(_17903.lo, _17904.lo);
    float _17901 = precise::max(_17903.hi, _17904.hi);
    Interval _17898;
    _17898.lo = _17900;
    _17898.hi = _17901;
    Interval _17899 = _17898;
    Interval _17902 = _17899;
    Interval _17905 = _17902;
    Interval _17906 = param_var_a_2.y;
    Interval _17907 = param_var_b_3.y;
    float _17895 = precise::min(_17906.lo, _17907.lo);
    float _17896 = precise::max(_17906.hi, _17907.hi);
    Interval _17893;
    _17893.lo = _17895;
    _17893.hi = _17896;
    Interval _17894 = _17893;
    Interval _17897 = _17894;
    Interval _17908 = _17897;
    Interval _17909 = param_var_a_2.z;
    Interval _17910 = param_var_b_3.z;
    float _17890 = precise::min(_17909.lo, _17910.lo);
    float _17891 = precise::max(_17909.hi, _17910.hi);
    Interval _17888;
    _17888.lo = _17890;
    _17888.hi = _17891;
    Interval _17889 = _17888;
    Interval _17892 = _17889;
    Interval _17911 = _17892;
    Interval3 _17886;
    _17886.x = _17905;
    _17886.y = _17908;
    _17886.z = _17911;
    Interval3 _17887 = _17886;
    Interval3 _17912 = _17887;
    return _17912;
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        float param_var_x = n;
        float _18513 = param_var_x;
        float _18514 = param_var_x;
        Interval _18511;
        _18511.lo = _18513;
        _18511.hi = _18514;
        Interval _18512 = _18511;
        Interval _18515 = _18512;
        Interval param_var_a = _18515;
        float param_var_lo = as_type<float>(1036831948u);
        float param_var_hi = as_type<float>(1036831950u);
        Interval _18509;
        _18509.lo = param_var_lo;
        _18509.hi = param_var_hi;
        Interval _18510 = _18509;
        Interval param_var_b = _18510;
        Interval _18536 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _18536;
    }
    if (d == 100.0)
    {
        float param_var_x_1 = n;
        float _18506 = param_var_x_1;
        float _18507 = param_var_x_1;
        Interval _18504;
        _18504.lo = _18506;
        _18504.hi = _18507;
        Interval _18505 = _18504;
        Interval _18508 = _18505;
        Interval param_var_a_1 = _18508;
        float param_var_lo_1 = as_type<float>(1008981769u);
        float param_var_hi_1 = as_type<float>(1008981771u);
        Interval _18502;
        _18502.lo = param_var_lo_1;
        _18502.hi = param_var_hi_1;
        Interval _18503 = _18502;
        Interval param_var_b_1 = _18503;
        Interval _18557 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _18557;
    }
    if (d == 1000.0)
    {
        float param_var_x_2 = n;
        float _18499 = param_var_x_2;
        float _18500 = param_var_x_2;
        Interval _18497;
        _18497.lo = _18499;
        _18497.hi = _18500;
        Interval _18498 = _18497;
        Interval _18501 = _18498;
        Interval param_var_a_2 = _18501;
        float param_var_lo_2 = as_type<float>(981668462u);
        float param_var_hi_2 = as_type<float>(981668464u);
        Interval _18495;
        _18495.lo = param_var_lo_2;
        _18495.hi = param_var_hi_2;
        Interval _18496 = _18495;
        Interval param_var_b_2 = _18496;
        Interval _18578 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _18578;
    }
    if (d == 10000.0)
    {
        float param_var_x_3 = n;
        float _18492 = param_var_x_3;
        float _18493 = param_var_x_3;
        Interval _18490;
        _18490.lo = _18492;
        _18490.hi = _18493;
        Interval _18491 = _18490;
        Interval _18494 = _18491;
        Interval param_var_a_3 = _18494;
        float param_var_lo_3 = as_type<float>(953267990u);
        float param_var_hi_3 = as_type<float>(953267992u);
        Interval _18488;
        _18488.lo = param_var_lo_3;
        _18488.hi = param_var_hi_3;
        Interval _18489 = _18488;
        Interval param_var_b_3 = _18489;
        Interval _18599 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _18599;
    }
    if (d == 100000.0)
    {
        float param_var_x_4 = n;
        float _18485 = param_var_x_4;
        float _18486 = param_var_x_4;
        Interval _18483;
        _18483.lo = _18485;
        _18483.hi = _18486;
        Interval _18484 = _18483;
        Interval _18487 = _18484;
        Interval param_var_a_4 = _18487;
        float param_var_lo_4 = as_type<float>(925353387u);
        float param_var_hi_4 = as_type<float>(925353389u);
        Interval _18481;
        _18481.lo = param_var_lo_4;
        _18481.hi = param_var_hi_4;
        Interval _18482 = _18481;
        Interval param_var_b_4 = _18482;
        Interval _18620 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _18620;
    }
    if (d == 128.0)
    {
        float param_var_x_5 = n;
        float _18478 = param_var_x_5;
        float _18479 = param_var_x_5;
        Interval _18476;
        _18476.lo = _18478;
        _18476.hi = _18479;
        Interval _18477 = _18476;
        Interval _18480 = _18477;
        Interval param_var_a_5 = _18480;
        float param_var_lo_5 = as_type<float>(1006632960u);
        float param_var_hi_5 = as_type<float>(1006632960u);
        Interval _18474;
        _18474.lo = param_var_lo_5;
        _18474.hi = param_var_hi_5;
        Interval _18475 = _18474;
        Interval param_var_b_5 = _18475;
        Interval _18641 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _18641;
    }
    if (d == 65535.0)
    {
        float param_var_x_6 = n;
        float _18471 = param_var_x_6;
        float _18472 = param_var_x_6;
        Interval _18469;
        _18469.lo = _18471;
        _18469.hi = _18472;
        Interval _18470 = _18469;
        Interval _18473 = _18470;
        Interval param_var_a_6 = _18473;
        float param_var_lo_6 = as_type<float>(931135615u);
        float param_var_hi_6 = as_type<float>(931135617u);
        Interval _18467;
        _18467.lo = param_var_lo_6;
        _18467.hi = param_var_hi_6;
        Interval _18468 = _18467;
        Interval param_var_b_6 = _18468;
        Interval _18662 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _18662;
    }
    float param_var_x_7 = n;
    float _18464 = param_var_x_7;
    float _18465 = param_var_x_7;
    Interval _18462;
    _18462.lo = _18464;
    _18462.hi = _18465;
    Interval _18463 = _18462;
    Interval _18466 = _18463;
    Interval param_var_a_7 = _18466;
    float param_var_x_8 = d;
    float _18459 = param_var_x_8;
    float _18460 = param_var_x_8;
    Interval _18457;
    _18457.lo = _18459;
    _18457.hi = _18460;
    Interval _18458 = _18457;
    Interval _18461 = _18458;
    Interval param_var_b_7 = _18461;
    Interval _18683 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _18683;
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
        Interval _20593;
        _20593.lo = param_var_lo;
        _20593.hi = param_var_hi;
        Interval _20594 = _20593;
        return _20594;
    }
    float n = floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5));
    Interval param_var_a = a;
    float param_var_x = spvFMul(n, 2.0);
    float _20590 = param_var_x;
    float _20591 = param_var_x;
    Interval _20588;
    _20588.lo = _20590;
    _20588.hi = _20591;
    Interval _20589 = _20588;
    Interval _20592 = _20589;
    Interval param_var_a_1 = _20592;
    float _20586 = 3.1415927410125732421875;
    float _20581 = _20586;
    float _20640 = interval_down(_20581, intervalFailed);
    float _20582 = _20640;
    float _20583 = _20586;
    float _20642 = interval_up(_20583, intervalFailed);
    float _20584 = _20642;
    Interval _20579;
    _20579.lo = _20582;
    _20579.hi = _20584;
    Interval _20580 = _20579;
    Interval _20585 = _20580;
    Interval _20587 = _20585;
    Interval param_var_b = _20587;
    Interval _20651 = imul(param_var_a_1, param_var_b, intervalFailed, optical_product_upper);
    Interval param_var_b_1 = _20651;
    Interval _20575 = param_var_a;
    Interval _20576 = param_var_b_1;
    float _20572 = as_type<float>(as_type<uint>(_20576.hi) ^ 2147483648u);
    float _20573 = as_type<float>(as_type<uint>(_20576.lo) ^ 2147483648u);
    Interval _20570;
    _20570.lo = _20572;
    _20570.hi = _20573;
    Interval _20571 = _20570;
    Interval _20574 = _20571;
    Interval _20577 = _20574;
    Interval _20671 = iadd(_20575, _20577, intervalFailed);
    Interval _20578 = _20671;
    Interval x = _20578;
    float _20568 = 3.1415927410125732421875;
    float _20563 = _20568;
    float _20674 = interval_down(_20563, intervalFailed);
    float _20564 = _20674;
    float _20565 = _20568;
    float _20676 = interval_up(_20565, intervalFailed);
    float _20566 = _20676;
    Interval _20561;
    _20561.lo = _20564;
    _20561.hi = _20566;
    Interval _20562 = _20561;
    Interval _20567 = _20562;
    Interval _20569 = _20567;
    Interval param_var_a_2 = _20569;
    float param_var_x_1 = 0.5;
    float _20558 = param_var_x_1;
    float _20559 = param_var_x_1;
    Interval _20556;
    _20556.lo = _20558;
    _20556.hi = _20559;
    Interval _20557 = _20556;
    Interval _20560 = _20557;
    Interval param_var_b_2 = _20560;
    Interval _20694 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _half = _20694;
    if (x.lo > _half.hi)
    {
        float _20554 = 3.1415927410125732421875;
        float _20549 = _20554;
        float _20701 = interval_down(_20549, intervalFailed);
        float _20550 = _20701;
        float _20551 = _20554;
        float _20703 = interval_up(_20551, intervalFailed);
        float _20552 = _20703;
        Interval _20547;
        _20547.lo = _20550;
        _20547.hi = _20552;
        Interval _20548 = _20547;
        Interval _20553 = _20548;
        Interval _20555 = _20553;
        Interval param_var_a_3 = _20555;
        Interval param_var_b_3 = x;
        Interval _20543 = param_var_a_3;
        Interval _20544 = param_var_b_3;
        float _20540 = as_type<float>(as_type<uint>(_20544.hi) ^ 2147483648u);
        float _20541 = as_type<float>(as_type<uint>(_20544.lo) ^ 2147483648u);
        Interval _20538;
        _20538.lo = _20540;
        _20538.hi = _20541;
        Interval _20539 = _20538;
        Interval _20542 = _20539;
        Interval _20545 = _20542;
        Interval _20732 = iadd(_20543, _20545, intervalFailed);
        Interval _20546 = _20732;
        x = _20546;
    }
    else
    {
        if (x.hi < (-_half.hi))
        {
            float _20536 = 3.1415927410125732421875;
            float _20531 = _20536;
            float _20740 = interval_down(_20531, intervalFailed);
            float _20532 = _20740;
            float _20533 = _20536;
            float _20742 = interval_up(_20533, intervalFailed);
            float _20534 = _20742;
            Interval _20529;
            _20529.lo = _20532;
            _20529.hi = _20534;
            Interval _20530 = _20529;
            Interval _20535 = _20530;
            Interval _20537 = _20535;
            Interval param_var_a_4 = _20537;
            float _20526 = as_type<float>(as_type<uint>(param_var_a_4.hi) ^ 2147483648u);
            float _20527 = as_type<float>(as_type<uint>(param_var_a_4.lo) ^ 2147483648u);
            Interval _20524;
            _20524.lo = _20526;
            _20524.hi = _20527;
            Interval _20525 = _20524;
            Interval _20528 = _20525;
            Interval param_var_a_5 = _20528;
            Interval param_var_b_4 = x;
            Interval _20520 = param_var_a_5;
            Interval _20521 = param_var_b_4;
            float _20517 = as_type<float>(as_type<uint>(_20521.hi) ^ 2147483648u);
            float _20518 = as_type<float>(as_type<uint>(_20521.lo) ^ 2147483648u);
            Interval _20515;
            _20515.lo = _20517;
            _20515.hi = _20518;
            Interval _20516 = _20515;
            Interval _20519 = _20516;
            Interval _20522 = _20519;
            Interval _20788 = iadd(_20520, _20522, intervalFailed);
            Interval _20523 = _20788;
            x = _20523;
        }
    }
    float _1877 = -_half.hi;
    bool temp_var_logical_2 = true;
    if ((isunordered(x.lo, _1877) || x.lo >= _1877))
    {
        temp_var_logical_2 = x.hi > _half.hi;
    }
    if (temp_var_logical_2)
    {
        float param_var_lo_1 = -1.0;
        float param_var_hi_1 = 1.0;
        Interval _20513;
        _20513.lo = param_var_lo_1;
        _20513.hi = param_var_hi_1;
        Interval _20514 = _20513;
        return _20514;
    }
    Interval param_var_a_6 = x;
    bool _20506 = false;
    if (param_var_a_6.lo <= 0.0)
    {
        _20506 = param_var_a_6.hi >= 0.0;
    }
    float _20505;
    if (_20506)
    {
        _20505 = 0.0;
    }
    else
    {
        _20505 = precise::min(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    }
    float _20504 = _20505;
    float _20507 = precise::max(abs(param_var_a_6.lo), abs(param_var_a_6.hi));
    float _20508 = spvFMul(_20504, _20504);
    float _20837 = interval_down(_20508, intervalFailed);
    float _20509 = precise::max(0.0, _20837);
    float _20510 = spvFMul(_20507, _20507);
    float _20841 = interval_up(_20510, intervalFailed);
    float _20511 = _20841;
    Interval _20502;
    _20502.lo = _20509;
    _20502.hi = _20511;
    Interval _20503 = _20502;
    Interval _20512 = _20503;
    Interval square = _20512;
    float param_var_x_2 = _2326[8];
    float _20497 = param_var_x_2;
    float _20852 = interval_down(_20497, intervalFailed);
    float _20498 = _20852;
    float _20499 = param_var_x_2;
    float _20854 = interval_up(_20499, intervalFailed);
    float _20500 = _20854;
    Interval _20495;
    _20495.lo = _20498;
    _20495.hi = _20500;
    Interval _20496 = _20495;
    Interval _20501 = _20496;
    Interval sum = _20501;
    Interval _20488;
    for (int k = 7; k >= 0; k--)
    {
        Interval param_var_a_7 = sum;
        Interval param_var_b_5 = square;
        Interval _20866 = imul(param_var_a_7, param_var_b_5, intervalFailed, optical_product_upper);
        Interval param_var_a_8 = _20866;
        float param_var_x_3 = _2326[k];
        float _20490 = param_var_x_3;
        float _20871 = interval_down(_20490, intervalFailed);
        float _20491 = _20871;
        float _20492 = param_var_x_3;
        float _20873 = interval_up(_20492, intervalFailed);
        float _20493 = _20873;
        _20488.lo = _20491;
        _20488.hi = _20493;
        Interval _20489 = _20488;
        Interval _20494 = _20489;
        Interval param_var_b_6 = _20494;
        Interval _20881 = iadd(param_var_a_8, param_var_b_6, intervalFailed);
        sum = _20881;
    }
    Interval param_var_a_9 = x;
    Interval param_var_b_7 = sum;
    Interval _20885 = imul(param_var_a_9, param_var_b_7, intervalFailed, optical_product_upper);
    Interval param_var_a_10 = _20885;
    float param_var_lo_2 = -3.9999999840167888010000751819462e-12;
    float param_var_hi_2 = 3.9999999840167888010000751819462e-12;
    Interval _20486;
    _20486.lo = param_var_lo_2;
    _20486.hi = param_var_hi_2;
    Interval _20487 = _20486;
    Interval param_var_b_8 = _20487;
    Interval _20892 = iadd(param_var_a_10, param_var_b_8, intervalFailed);
    Interval param_var_a_11 = _20892;
    float param_var_x_4 = -1.0;
    float _20483 = param_var_x_4;
    float _20484 = param_var_x_4;
    Interval _20481;
    _20481.lo = _20483;
    _20481.hi = _20484;
    Interval _20482 = _20481;
    Interval _20485 = _20482;
    Interval param_var_lo_3 = _20485;
    float param_var_x_5 = 1.0;
    float _20478 = param_var_x_5;
    float _20479 = param_var_x_5;
    Interval _20476;
    _20476.lo = _20478;
    _20476.hi = _20479;
    Interval _20477 = _20476;
    Interval _20480 = _20477;
    Interval param_var_hi_3 = _20480;
    Interval _20471 = param_var_a_11;
    Interval _20472 = param_var_lo_3;
    float _20468 = precise::max(_20471.lo, _20472.lo);
    float _20469 = precise::max(_20471.hi, _20472.hi);
    Interval _20466;
    _20466.lo = _20468;
    _20466.hi = _20469;
    Interval _20467 = _20466;
    Interval _20470 = _20467;
    Interval _20473 = _20470;
    Interval _20474 = param_var_hi_3;
    float _20463 = precise::min(_20473.lo, _20474.lo);
    float _20464 = precise::min(_20473.hi, _20474.hi);
    Interval _20461;
    _20461.lo = _20463;
    _20461.hi = _20464;
    Interval _20462 = _20461;
    Interval _20465 = _20462;
    Interval _20475 = _20465;
    return _20475;
}

static inline __attribute__((always_inline))
bool outside_face(thread const ReflectionSpecularPlane& p, thread const Interval3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = p;
    bool _19032 = false;
    if (param_var_p.a.w == 2.0)
    {
        _19032 = param_var_p.b.w == 2.0;
    }
    bool _19033 = false;
    if (_19032)
    {
        _19033 = param_var_p.c.w == 2.0;
    }
    bool _19034 = _19033;
    if (_19034)
    {
        return false;
    }
    float3 param_var_v = p.b.xyz;
    float _19025 = param_var_v.x;
    float _19022 = _19025;
    float _19023 = _19025;
    Interval _19020;
    _19020.lo = _19022;
    _19020.hi = _19023;
    Interval _19021 = _19020;
    Interval _19024 = _19021;
    Interval _19026 = _19024;
    float _19027 = param_var_v.y;
    float _19017 = _19027;
    float _19018 = _19027;
    Interval _19015;
    _19015.lo = _19017;
    _19015.hi = _19018;
    Interval _19016 = _19015;
    Interval _19019 = _19016;
    Interval _19028 = _19019;
    float _19029 = param_var_v.z;
    float _19012 = _19029;
    float _19013 = _19029;
    Interval _19010;
    _19010.lo = _19012;
    _19010.hi = _19013;
    Interval _19011 = _19010;
    Interval _19014 = _19011;
    Interval _19030 = _19014;
    Interval3 _19008;
    _19008.x = _19026;
    _19008.y = _19028;
    _19008.z = _19030;
    Interval3 _19009 = _19008;
    Interval3 _19031 = _19009;
    Interval3 param_var_a = _19031;
    float3 param_var_v_1 = p.a.xyz;
    float _19001 = param_var_v_1.x;
    float _18998 = _19001;
    float _18999 = _19001;
    Interval _18996;
    _18996.lo = _18998;
    _18996.hi = _18999;
    Interval _18997 = _18996;
    Interval _19000 = _18997;
    Interval _19002 = _19000;
    float _19003 = param_var_v_1.y;
    float _18993 = _19003;
    float _18994 = _19003;
    Interval _18991;
    _18991.lo = _18993;
    _18991.hi = _18994;
    Interval _18992 = _18991;
    Interval _18995 = _18992;
    Interval _19004 = _18995;
    float _19005 = param_var_v_1.z;
    float _18988 = _19005;
    float _18989 = _19005;
    Interval _18986;
    _18986.lo = _18988;
    _18986.hi = _18989;
    Interval _18987 = _18986;
    Interval _18990 = _18987;
    Interval _19006 = _18990;
    Interval3 _18984;
    _18984.x = _19002;
    _18984.y = _19004;
    _18984.z = _19006;
    Interval3 _18985 = _18984;
    Interval3 _19007 = _18985;
    Interval3 param_var_b = _19007;
    Interval3 _18975 = param_var_a;
    Interval _18976 = param_var_b.x;
    float _18972 = as_type<float>(as_type<uint>(_18976.hi) ^ 2147483648u);
    float _18973 = as_type<float>(as_type<uint>(_18976.lo) ^ 2147483648u);
    Interval _18970;
    _18970.lo = _18972;
    _18970.hi = _18973;
    Interval _18971 = _18970;
    Interval _18974 = _18971;
    Interval _18977 = _18974;
    Interval _18978 = param_var_b.y;
    float _18967 = as_type<float>(as_type<uint>(_18978.hi) ^ 2147483648u);
    float _18968 = as_type<float>(as_type<uint>(_18978.lo) ^ 2147483648u);
    Interval _18965;
    _18965.lo = _18967;
    _18965.hi = _18968;
    Interval _18966 = _18965;
    Interval _18969 = _18966;
    Interval _18979 = _18969;
    Interval _18980 = param_var_b.z;
    float _18962 = as_type<float>(as_type<uint>(_18980.hi) ^ 2147483648u);
    float _18963 = as_type<float>(as_type<uint>(_18980.lo) ^ 2147483648u);
    Interval _18960;
    _18960.lo = _18962;
    _18960.hi = _18963;
    Interval _18961 = _18960;
    Interval _18964 = _18961;
    Interval _18981 = _18964;
    Interval3 _18958;
    _18958.x = _18977;
    _18958.y = _18979;
    _18958.z = _18981;
    Interval3 _18959 = _18958;
    Interval3 _18982 = _18959;
    Interval _18948 = _18975.x;
    Interval _18949 = _18982.x;
    Interval _19215 = iadd(_18948, _18949, intervalFailed);
    Interval _18950 = _19215;
    Interval _18951 = _18975.y;
    Interval _18952 = _18982.y;
    Interval _19220 = iadd(_18951, _18952, intervalFailed);
    Interval _18953 = _19220;
    Interval _18954 = _18975.z;
    Interval _18955 = _18982.z;
    Interval _19225 = iadd(_18954, _18955, intervalFailed);
    Interval _18956 = _19225;
    Interval3 _18946;
    _18946.x = _18950;
    _18946.y = _18953;
    _18946.z = _18956;
    Interval3 _18947 = _18946;
    Interval3 _18957 = _18947;
    Interval3 _18983 = _18957;
    Interval3 u = _18983;
    float3 param_var_v_2 = p.c.xyz;
    float _18939 = param_var_v_2.x;
    float _18936 = _18939;
    float _18937 = _18939;
    Interval _18934;
    _18934.lo = _18936;
    _18934.hi = _18937;
    Interval _18935 = _18934;
    Interval _18938 = _18935;
    Interval _18940 = _18938;
    float _18941 = param_var_v_2.y;
    float _18931 = _18941;
    float _18932 = _18941;
    Interval _18929;
    _18929.lo = _18931;
    _18929.hi = _18932;
    Interval _18930 = _18929;
    Interval _18933 = _18930;
    Interval _18942 = _18933;
    float _18943 = param_var_v_2.z;
    float _18926 = _18943;
    float _18927 = _18943;
    Interval _18924;
    _18924.lo = _18926;
    _18924.hi = _18927;
    Interval _18925 = _18924;
    Interval _18928 = _18925;
    Interval _18944 = _18928;
    Interval3 _18922;
    _18922.x = _18940;
    _18922.y = _18942;
    _18922.z = _18944;
    Interval3 _18923 = _18922;
    Interval3 _18945 = _18923;
    Interval3 param_var_a_1 = _18945;
    float3 param_var_v_3 = p.a.xyz;
    float _18915 = param_var_v_3.x;
    float _18912 = _18915;
    float _18913 = _18915;
    Interval _18910;
    _18910.lo = _18912;
    _18910.hi = _18913;
    Interval _18911 = _18910;
    Interval _18914 = _18911;
    Interval _18916 = _18914;
    float _18917 = param_var_v_3.y;
    float _18907 = _18917;
    float _18908 = _18917;
    Interval _18905;
    _18905.lo = _18907;
    _18905.hi = _18908;
    Interval _18906 = _18905;
    Interval _18909 = _18906;
    Interval _18918 = _18909;
    float _18919 = param_var_v_3.z;
    float _18902 = _18919;
    float _18903 = _18919;
    Interval _18900;
    _18900.lo = _18902;
    _18900.hi = _18903;
    Interval _18901 = _18900;
    Interval _18904 = _18901;
    Interval _18920 = _18904;
    Interval3 _18898;
    _18898.x = _18916;
    _18898.y = _18918;
    _18898.z = _18920;
    Interval3 _18899 = _18898;
    Interval3 _18921 = _18899;
    Interval3 param_var_b_1 = _18921;
    Interval3 _18889 = param_var_a_1;
    Interval _18890 = param_var_b_1.x;
    float _18886 = as_type<float>(as_type<uint>(_18890.hi) ^ 2147483648u);
    float _18887 = as_type<float>(as_type<uint>(_18890.lo) ^ 2147483648u);
    Interval _18884;
    _18884.lo = _18886;
    _18884.hi = _18887;
    Interval _18885 = _18884;
    Interval _18888 = _18885;
    Interval _18891 = _18888;
    Interval _18892 = param_var_b_1.y;
    float _18881 = as_type<float>(as_type<uint>(_18892.hi) ^ 2147483648u);
    float _18882 = as_type<float>(as_type<uint>(_18892.lo) ^ 2147483648u);
    Interval _18879;
    _18879.lo = _18881;
    _18879.hi = _18882;
    Interval _18880 = _18879;
    Interval _18883 = _18880;
    Interval _18893 = _18883;
    Interval _18894 = param_var_b_1.z;
    float _18876 = as_type<float>(as_type<uint>(_18894.hi) ^ 2147483648u);
    float _18877 = as_type<float>(as_type<uint>(_18894.lo) ^ 2147483648u);
    Interval _18874;
    _18874.lo = _18876;
    _18874.hi = _18877;
    Interval _18875 = _18874;
    Interval _18878 = _18875;
    Interval _18895 = _18878;
    Interval3 _18872;
    _18872.x = _18891;
    _18872.y = _18893;
    _18872.z = _18895;
    Interval3 _18873 = _18872;
    Interval3 _18896 = _18873;
    Interval _18862 = _18889.x;
    Interval _18863 = _18896.x;
    Interval _19396 = iadd(_18862, _18863, intervalFailed);
    Interval _18864 = _19396;
    Interval _18865 = _18889.y;
    Interval _18866 = _18896.y;
    Interval _19401 = iadd(_18865, _18866, intervalFailed);
    Interval _18867 = _19401;
    Interval _18868 = _18889.z;
    Interval _18869 = _18896.z;
    Interval _19406 = iadd(_18868, _18869, intervalFailed);
    Interval _18870 = _19406;
    Interval3 _18860;
    _18860.x = _18864;
    _18860.y = _18867;
    _18860.z = _18870;
    Interval3 _18861 = _18860;
    Interval3 _18871 = _18861;
    Interval3 _18897 = _18871;
    Interval3 v = _18897;
    Interval3 param_var_a_2 = hit;
    float3 param_var_v_4 = p.a.xyz;
    float _18853 = param_var_v_4.x;
    float _18850 = _18853;
    float _18851 = _18853;
    Interval _18848;
    _18848.lo = _18850;
    _18848.hi = _18851;
    Interval _18849 = _18848;
    Interval _18852 = _18849;
    Interval _18854 = _18852;
    float _18855 = param_var_v_4.y;
    float _18845 = _18855;
    float _18846 = _18855;
    Interval _18843;
    _18843.lo = _18845;
    _18843.hi = _18846;
    Interval _18844 = _18843;
    Interval _18847 = _18844;
    Interval _18856 = _18847;
    float _18857 = param_var_v_4.z;
    float _18840 = _18857;
    float _18841 = _18857;
    Interval _18838;
    _18838.lo = _18840;
    _18838.hi = _18841;
    Interval _18839 = _18838;
    Interval _18842 = _18839;
    Interval _18858 = _18842;
    Interval3 _18836;
    _18836.x = _18854;
    _18836.y = _18856;
    _18836.z = _18858;
    Interval3 _18837 = _18836;
    Interval3 _18859 = _18837;
    Interval3 param_var_b_2 = _18859;
    Interval3 _18827 = param_var_a_2;
    Interval _18828 = param_var_b_2.x;
    float _18824 = as_type<float>(as_type<uint>(_18828.hi) ^ 2147483648u);
    float _18825 = as_type<float>(as_type<uint>(_18828.lo) ^ 2147483648u);
    Interval _18822;
    _18822.lo = _18824;
    _18822.hi = _18825;
    Interval _18823 = _18822;
    Interval _18826 = _18823;
    Interval _18829 = _18826;
    Interval _18830 = param_var_b_2.y;
    float _18819 = as_type<float>(as_type<uint>(_18830.hi) ^ 2147483648u);
    float _18820 = as_type<float>(as_type<uint>(_18830.lo) ^ 2147483648u);
    Interval _18817;
    _18817.lo = _18819;
    _18817.hi = _18820;
    Interval _18818 = _18817;
    Interval _18821 = _18818;
    Interval _18831 = _18821;
    Interval _18832 = param_var_b_2.z;
    float _18814 = as_type<float>(as_type<uint>(_18832.hi) ^ 2147483648u);
    float _18815 = as_type<float>(as_type<uint>(_18832.lo) ^ 2147483648u);
    Interval _18812;
    _18812.lo = _18814;
    _18812.hi = _18815;
    Interval _18813 = _18812;
    Interval _18816 = _18813;
    Interval _18833 = _18816;
    Interval3 _18810;
    _18810.x = _18829;
    _18810.y = _18831;
    _18810.z = _18833;
    Interval3 _18811 = _18810;
    Interval3 _18834 = _18811;
    Interval _18800 = _18827.x;
    Interval _18801 = _18834.x;
    Interval _19533 = iadd(_18800, _18801, intervalFailed);
    Interval _18802 = _19533;
    Interval _18803 = _18827.y;
    Interval _18804 = _18834.y;
    Interval _19538 = iadd(_18803, _18804, intervalFailed);
    Interval _18805 = _19538;
    Interval _18806 = _18827.z;
    Interval _18807 = _18834.z;
    Interval _19543 = iadd(_18806, _18807, intervalFailed);
    Interval _18808 = _19543;
    Interval3 _18798;
    _18798.x = _18802;
    _18798.y = _18805;
    _18798.z = _18808;
    Interval3 _18799 = _18798;
    Interval3 _18809 = _18799;
    Interval3 _18835 = _18809;
    Interval3 r = _18835;
    Interval3 param_var_a_3 = u;
    Interval3 param_var_b_3 = u;
    Interval _18787 = param_var_a_3.x;
    Interval _18788 = param_var_b_3.x;
    Interval _19560 = imul(_18787, _18788, intervalFailed, optical_product_upper);
    Interval _18789 = _19560;
    Interval _18790 = param_var_a_3.y;
    Interval _18791 = param_var_b_3.y;
    Interval _19565 = imul(_18790, _18791, intervalFailed, optical_product_upper);
    Interval _18792 = _19565;
    Interval _19566 = iadd(_18789, _18792, intervalFailed);
    Interval _18793 = _19566;
    Interval _18794 = param_var_a_3.z;
    Interval _18795 = param_var_b_3.z;
    Interval _19571 = imul(_18794, _18795, intervalFailed, optical_product_upper);
    Interval _18796 = _19571;
    Interval _19572 = iadd(_18793, _18796, intervalFailed);
    Interval _18797 = _19572;
    Interval aa = _18797;
    Interval3 param_var_a_4 = u;
    Interval3 param_var_b_4 = v;
    Interval _18776 = param_var_a_4.x;
    Interval _18777 = param_var_b_4.x;
    Interval _19580 = imul(_18776, _18777, intervalFailed, optical_product_upper);
    Interval _18778 = _19580;
    Interval _18779 = param_var_a_4.y;
    Interval _18780 = param_var_b_4.y;
    Interval _19585 = imul(_18779, _18780, intervalFailed, optical_product_upper);
    Interval _18781 = _19585;
    Interval _19586 = iadd(_18778, _18781, intervalFailed);
    Interval _18782 = _19586;
    Interval _18783 = param_var_a_4.z;
    Interval _18784 = param_var_b_4.z;
    Interval _19591 = imul(_18783, _18784, intervalFailed, optical_product_upper);
    Interval _18785 = _19591;
    Interval _19592 = iadd(_18782, _18785, intervalFailed);
    Interval _18786 = _19592;
    Interval ab = _18786;
    Interval3 param_var_a_5 = v;
    Interval3 param_var_b_5 = v;
    Interval _18765 = param_var_a_5.x;
    Interval _18766 = param_var_b_5.x;
    Interval _19600 = imul(_18765, _18766, intervalFailed, optical_product_upper);
    Interval _18767 = _19600;
    Interval _18768 = param_var_a_5.y;
    Interval _18769 = param_var_b_5.y;
    Interval _19605 = imul(_18768, _18769, intervalFailed, optical_product_upper);
    Interval _18770 = _19605;
    Interval _19606 = iadd(_18767, _18770, intervalFailed);
    Interval _18771 = _19606;
    Interval _18772 = param_var_a_5.z;
    Interval _18773 = param_var_b_5.z;
    Interval _19611 = imul(_18772, _18773, intervalFailed, optical_product_upper);
    Interval _18774 = _19611;
    Interval _19612 = iadd(_18771, _18774, intervalFailed);
    Interval _18775 = _19612;
    Interval bb = _18775;
    Interval3 param_var_a_6 = r;
    Interval3 param_var_b_6 = u;
    Interval _18754 = param_var_a_6.x;
    Interval _18755 = param_var_b_6.x;
    Interval _19620 = imul(_18754, _18755, intervalFailed, optical_product_upper);
    Interval _18756 = _19620;
    Interval _18757 = param_var_a_6.y;
    Interval _18758 = param_var_b_6.y;
    Interval _19625 = imul(_18757, _18758, intervalFailed, optical_product_upper);
    Interval _18759 = _19625;
    Interval _19626 = iadd(_18756, _18759, intervalFailed);
    Interval _18760 = _19626;
    Interval _18761 = param_var_a_6.z;
    Interval _18762 = param_var_b_6.z;
    Interval _19631 = imul(_18761, _18762, intervalFailed, optical_product_upper);
    Interval _18763 = _19631;
    Interval _19632 = iadd(_18760, _18763, intervalFailed);
    Interval _18764 = _19632;
    Interval ra = _18764;
    Interval3 param_var_a_7 = r;
    Interval3 param_var_b_7 = v;
    Interval _18743 = param_var_a_7.x;
    Interval _18744 = param_var_b_7.x;
    Interval _19640 = imul(_18743, _18744, intervalFailed, optical_product_upper);
    Interval _18745 = _19640;
    Interval _18746 = param_var_a_7.y;
    Interval _18747 = param_var_b_7.y;
    Interval _19645 = imul(_18746, _18747, intervalFailed, optical_product_upper);
    Interval _18748 = _19645;
    Interval _19646 = iadd(_18745, _18748, intervalFailed);
    Interval _18749 = _19646;
    Interval _18750 = param_var_a_7.z;
    Interval _18751 = param_var_b_7.z;
    Interval _19651 = imul(_18750, _18751, intervalFailed, optical_product_upper);
    Interval _18752 = _19651;
    Interval _19652 = iadd(_18749, _18752, intervalFailed);
    Interval _18753 = _19652;
    Interval rb = _18753;
    Interval param_var_a_8 = aa;
    Interval param_var_b_8 = bb;
    Interval _19656 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _19656;
    Interval param_var_a_10 = ab;
    bool _18736 = false;
    if (param_var_a_10.lo <= 0.0)
    {
        _18736 = param_var_a_10.hi >= 0.0;
    }
    float _18735;
    if (_18736)
    {
        _18735 = 0.0;
    }
    else
    {
        _18735 = precise::min(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    }
    float _18734 = _18735;
    float _18737 = precise::max(abs(param_var_a_10.lo), abs(param_var_a_10.hi));
    float _18738 = spvFMul(_18734, _18734);
    float _19687 = interval_down(_18738, intervalFailed);
    float _18739 = precise::max(0.0, _19687);
    float _18740 = spvFMul(_18737, _18737);
    float _19691 = interval_up(_18740, intervalFailed);
    float _18741 = _19691;
    Interval _18732;
    _18732.lo = _18739;
    _18732.hi = _18741;
    Interval _18733 = _18732;
    Interval _18742 = _18733;
    Interval param_var_b_9 = _18742;
    Interval _18728 = param_var_a_9;
    Interval _18729 = param_var_b_9;
    float _18725 = as_type<float>(as_type<uint>(_18729.hi) ^ 2147483648u);
    float _18726 = as_type<float>(as_type<uint>(_18729.lo) ^ 2147483648u);
    Interval _18723;
    _18723.lo = _18725;
    _18723.hi = _18726;
    Interval _18724 = _18723;
    Interval _18727 = _18724;
    Interval _18730 = _18727;
    Interval _19718 = iadd(_18728, _18730, intervalFailed);
    Interval _18731 = _19718;
    Interval det = _18731;
    if (det.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_11 = bb;
    Interval param_var_b_10 = ra;
    Interval _19725 = imul(param_var_a_11, param_var_b_10, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _19725;
    Interval param_var_a_13 = ab;
    Interval param_var_b_11 = rb;
    Interval _19728 = imul(param_var_a_13, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_b_12 = _19728;
    Interval _18719 = param_var_a_12;
    Interval _18720 = param_var_b_12;
    float _18716 = as_type<float>(as_type<uint>(_18720.hi) ^ 2147483648u);
    float _18717 = as_type<float>(as_type<uint>(_18720.lo) ^ 2147483648u);
    Interval _18714;
    _18714.lo = _18716;
    _18714.hi = _18717;
    Interval _18715 = _18714;
    Interval _18718 = _18715;
    Interval _18721 = _18718;
    Interval _19748 = iadd(_18719, _18721, intervalFailed);
    Interval _18722 = _19748;
    Interval param_var_a_14 = _18722;
    Interval param_var_b_13 = det;
    Interval _19751 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
    Interval x = _19751;
    Interval param_var_a_15 = aa;
    Interval param_var_b_14 = rb;
    Interval _19754 = imul(param_var_a_15, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_16 = _19754;
    Interval param_var_a_17 = ab;
    Interval param_var_b_15 = ra;
    Interval _19757 = imul(param_var_a_17, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _19757;
    Interval _18710 = param_var_a_16;
    Interval _18711 = param_var_b_16;
    float _18707 = as_type<float>(as_type<uint>(_18711.hi) ^ 2147483648u);
    float _18708 = as_type<float>(as_type<uint>(_18711.lo) ^ 2147483648u);
    Interval _18705;
    _18705.lo = _18707;
    _18705.hi = _18708;
    Interval _18706 = _18705;
    Interval _18709 = _18706;
    Interval _18712 = _18709;
    Interval _19777 = iadd(_18710, _18712, intervalFailed);
    Interval _18713 = _19777;
    Interval param_var_a_18 = _18713;
    Interval param_var_b_17 = det;
    Interval _19780 = idiv(param_var_a_18, param_var_b_17, intervalFailed, interval_divide_upper);
    Interval y = _19780;
    float param_var_x = -9.9999997473787516355514526367188e-06;
    float _18700 = param_var_x;
    float _19784 = interval_down(_18700, intervalFailed);
    float _18701 = _19784;
    float _18702 = param_var_x;
    float _19786 = interval_up(_18702, intervalFailed);
    float _18703 = _19786;
    Interval _18698;
    _18698.lo = _18701;
    _18698.hi = _18703;
    Interval _18699 = _18698;
    Interval _18704 = _18699;
    Interval temp_var_Interval = _18704;
    bool temp_var_logical = true;
    if ((isunordered(x.hi, temp_var_Interval.lo) || x.hi >= temp_var_Interval.lo))
    {
        float param_var_x_1 = -9.9999997473787516355514526367188e-06;
        float _18693 = param_var_x_1;
        float _19800 = interval_down(_18693, intervalFailed);
        float _18694 = _19800;
        float _18695 = param_var_x_1;
        float _19802 = interval_up(_18695, intervalFailed);
        float _18696 = _19802;
        Interval _18691;
        _18691.lo = _18694;
        _18691.hi = _18696;
        Interval _18692 = _18691;
        Interval _18697 = _18692;
        Interval temp_var_Interval_1 = _18697;
        temp_var_logical = y.hi < temp_var_Interval_1.lo;
    }
    bool temp_var_logical_1 = true;
    if (!temp_var_logical)
    {
        Interval param_var_a_19 = x;
        Interval param_var_b_18 = y;
        Interval _19817 = iadd(param_var_a_19, param_var_b_18, intervalFailed);
        Interval temp_var_Interval_2 = _19817;
        float param_var_x_2 = 1.000010013580322265625;
        float _18686 = param_var_x_2;
        float _19821 = interval_down(_18686, intervalFailed);
        float _18687 = _19821;
        float _18688 = param_var_x_2;
        float _19823 = interval_up(_18688, intervalFailed);
        float _18689 = _19823;
        Interval _18684;
        _18684.lo = _18687;
        _18684.hi = _18689;
        Interval _18685 = _18684;
        Interval _18690 = _18685;
        Interval temp_var_Interval_3 = _18690;
        temp_var_logical_1 = temp_var_Interval_2.lo > temp_var_Interval_3.hi;
    }
    return temp_var_logical_1;
}

static inline __attribute__((always_inline))
bool optical_excluded_target(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTerminal, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper)
{
    float param_var_lo = box.x;
    float param_var_hi = box.z;
    Interval _8915;
    _8915.lo = param_var_lo;
    _8915.hi = param_var_hi;
    Interval _8916 = _8915;
    Interval param_var_a = _8916;
    float param_var_x = receiver.projection.z;
    float _8912 = param_var_x;
    float _8913 = param_var_x;
    Interval _8910;
    _8910.lo = _8912;
    _8910.hi = _8913;
    Interval _8911 = _8910;
    Interval _8914 = _8911;
    Interval param_var_b = _8914;
    Interval _8906 = param_var_a;
    Interval _8907 = param_var_b;
    float _8903 = as_type<float>(as_type<uint>(_8907.hi) ^ 2147483648u);
    float _8904 = as_type<float>(as_type<uint>(_8907.lo) ^ 2147483648u);
    Interval _8901;
    _8901.lo = _8903;
    _8901.hi = _8904;
    Interval _8902 = _8901;
    Interval _8905 = _8902;
    Interval _8908 = _8905;
    Interval _8958 = iadd(_8906, _8908, intervalFailed);
    Interval _8909 = _8958;
    Interval param_var_a_1 = _8909;
    float param_var_x_1 = receiver.projection.x;
    float _8898 = param_var_x_1;
    float _8899 = param_var_x_1;
    Interval _8896;
    _8896.lo = _8898;
    _8896.hi = _8899;
    Interval _8897 = _8896;
    Interval _8900 = _8897;
    Interval param_var_b_1 = _8900;
    Interval _8972 = idiv(param_var_a_1, param_var_b_1, intervalFailed, interval_divide_upper);
    Interval param_var_x_2 = _8972;
    float param_var_lo_1 = box.y;
    float param_var_hi_1 = box.w;
    Interval _8894;
    _8894.lo = param_var_lo_1;
    _8894.hi = param_var_hi_1;
    Interval _8895 = _8894;
    Interval param_var_a_2 = _8895;
    float param_var_x_3 = receiver.projection.w;
    float _8891 = param_var_x_3;
    float _8892 = param_var_x_3;
    Interval _8889;
    _8889.lo = _8891;
    _8889.hi = _8892;
    Interval _8890 = _8889;
    Interval _8893 = _8890;
    Interval param_var_b_2 = _8893;
    Interval _8885 = param_var_a_2;
    Interval _8886 = param_var_b_2;
    float _8882 = as_type<float>(as_type<uint>(_8886.hi) ^ 2147483648u);
    float _8883 = as_type<float>(as_type<uint>(_8886.lo) ^ 2147483648u);
    Interval _8880;
    _8880.lo = _8882;
    _8880.hi = _8883;
    Interval _8881 = _8880;
    Interval _8884 = _8881;
    Interval _8887 = _8884;
    Interval _9014 = iadd(_8885, _8887, intervalFailed);
    Interval _8888 = _9014;
    Interval param_var_a_3 = _8888;
    float param_var_x_4 = receiver.projection.y;
    float _8877 = param_var_x_4;
    float _8878 = param_var_x_4;
    Interval _8875;
    _8875.lo = _8877;
    _8875.hi = _8878;
    Interval _8876 = _8875;
    Interval _8879 = _8876;
    Interval param_var_b_3 = _8879;
    Interval _9028 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_y = _9028;
    float param_var_x_5 = 1.0;
    float _8872 = param_var_x_5;
    float _8873 = param_var_x_5;
    Interval _8870;
    _8870.lo = _8872;
    _8870.hi = _8873;
    Interval _8871 = _8870;
    Interval _8874 = _8871;
    Interval param_var_z = _8874;
    Interval3 _8868;
    _8868.x = param_var_x_2;
    _8868.y = param_var_y;
    _8868.z = param_var_z;
    Interval3 _8869 = _8868;
    Interval3 param_var_a_4 = _8869;
    Interval3 _8861 = param_var_a_4;
    float _8862 = 1.0;
    float _8858 = _8862;
    float _8859 = _8862;
    Interval _8856;
    _8856.lo = _8858;
    _8856.hi = _8859;
    Interval _8857 = _8856;
    Interval _8860 = _8857;
    Interval _8863 = _8860;
    Interval3 _8864 = param_var_a_4;
    Interval _8847 = _8864.x;
    bool _8840 = false;
    if (_8847.lo <= 0.0)
    {
        _8840 = _8847.hi >= 0.0;
    }
    float _8839;
    if (_8840)
    {
        _8839 = 0.0;
    }
    else
    {
        _8839 = precise::min(abs(_8847.lo), abs(_8847.hi));
    }
    float _8838 = _8839;
    float _8841 = precise::max(abs(_8847.lo), abs(_8847.hi));
    float _8842 = spvFMul(_8838, _8838);
    float _9088 = interval_down(_8842, intervalFailed);
    float _8843 = precise::max(0.0, _9088);
    float _8844 = spvFMul(_8841, _8841);
    float _9092 = interval_up(_8844, intervalFailed);
    float _8845 = _9092;
    Interval _8836;
    _8836.lo = _8843;
    _8836.hi = _8845;
    Interval _8837 = _8836;
    Interval _8846 = _8837;
    Interval _8848 = _8846;
    Interval _8849 = _8864.y;
    bool _8829 = false;
    if (_8849.lo <= 0.0)
    {
        _8829 = _8849.hi >= 0.0;
    }
    float _8828;
    if (_8829)
    {
        _8828 = 0.0;
    }
    else
    {
        _8828 = precise::min(abs(_8849.lo), abs(_8849.hi));
    }
    float _8827 = _8828;
    float _8830 = precise::max(abs(_8849.lo), abs(_8849.hi));
    float _8831 = spvFMul(_8827, _8827);
    float _9131 = interval_down(_8831, intervalFailed);
    float _8832 = precise::max(0.0, _9131);
    float _8833 = spvFMul(_8830, _8830);
    float _9135 = interval_up(_8833, intervalFailed);
    float _8834 = _9135;
    Interval _8825;
    _8825.lo = _8832;
    _8825.hi = _8834;
    Interval _8826 = _8825;
    Interval _8835 = _8826;
    Interval _8850 = _8835;
    Interval _9143 = iadd(_8848, _8850, intervalFailed);
    Interval _8851 = _9143;
    Interval _8852 = _8864.z;
    bool _8818 = false;
    if (_8852.lo <= 0.0)
    {
        _8818 = _8852.hi >= 0.0;
    }
    float _8817;
    if (_8818)
    {
        _8817 = 0.0;
    }
    else
    {
        _8817 = precise::min(abs(_8852.lo), abs(_8852.hi));
    }
    float _8816 = _8817;
    float _8819 = precise::max(abs(_8852.lo), abs(_8852.hi));
    float _8820 = spvFMul(_8816, _8816);
    float _9175 = interval_down(_8820, intervalFailed);
    float _8821 = precise::max(0.0, _9175);
    float _8822 = spvFMul(_8819, _8819);
    float _9179 = interval_up(_8822, intervalFailed);
    float _8823 = _9179;
    Interval _8814;
    _8814.lo = _8821;
    _8814.hi = _8823;
    Interval _8815 = _8814;
    Interval _8824 = _8815;
    Interval _8853 = _8824;
    Interval _9187 = iadd(_8851, _8853, intervalFailed);
    Interval _8854 = _9187;
    Interval _9188 = isqrt(_8854, intervalFailed);
    Interval _8855 = _9188;
    Interval _8865 = _8855;
    Interval _9190 = idiv(_8863, _8865, intervalFailed, interval_divide_upper);
    Interval _8866 = _9190;
    Interval _8804 = _8861.x;
    Interval _8805 = _8866;
    Interval _9194 = imul(_8804, _8805, intervalFailed, optical_product_upper);
    Interval _8806 = _9194;
    Interval _8807 = _8861.y;
    Interval _8808 = _8866;
    Interval _9198 = imul(_8807, _8808, intervalFailed, optical_product_upper);
    Interval _8809 = _9198;
    Interval _8810 = _8861.z;
    Interval _8811 = _8866;
    Interval _9202 = imul(_8810, _8811, intervalFailed, optical_product_upper);
    Interval _8812 = _9202;
    Interval3 _8802;
    _8802.x = _8806;
    _8802.y = _8809;
    _8802.z = _8812;
    Interval3 _8803 = _8802;
    Interval3 _8813 = _8803;
    Interval3 _8867 = _8813;
    Interval3 outgoing = _8867;
    float3 param_var_v = float3(0.0);
    float _8795 = param_var_v.x;
    float _8792 = _8795;
    float _8793 = _8795;
    Interval _8790;
    _8790.lo = _8792;
    _8790.hi = _8793;
    Interval _8791 = _8790;
    Interval _8794 = _8791;
    Interval _8796 = _8794;
    float _8797 = param_var_v.y;
    float _8787 = _8797;
    float _8788 = _8797;
    Interval _8785;
    _8785.lo = _8787;
    _8785.hi = _8788;
    Interval _8786 = _8785;
    Interval _8789 = _8786;
    Interval _8798 = _8789;
    float _8799 = param_var_v.z;
    float _8782 = _8799;
    float _8783 = _8799;
    Interval _8780;
    _8780.lo = _8782;
    _8780.hi = _8783;
    Interval _8781 = _8780;
    Interval _8784 = _8781;
    Interval _8800 = _8784;
    Interval3 _8778;
    _8778.x = _8796;
    _8778.y = _8798;
    _8778.z = _8800;
    Interval3 _8779 = _8778;
    Interval3 _8801 = _8779;
    Interval3 origin = _8801;
    float param_var_x_6 = 0.0;
    float _8775 = param_var_x_6;
    float _8776 = param_var_x_6;
    Interval _8773;
    _8773.lo = _8775;
    _8773.hi = _8776;
    Interval _8774 = _8773;
    Interval _8777 = _8774;
    Interval bias0 = _8777;
    bool temp_var_ternary;
    ReflectionSpecularPlane plane;
    Interval3 n;
    float3 temp_var_ternary_1;
    Interval radius;
    int temp_var_ternary_2;
    Interval3 t;
    Interval3 _5397;
    Interval _5399;
    Interval _5404;
    Interval _5409;
    Interval3 _5435;
    Interval _5447;
    float _5450;
    Interval _5458;
    float _5461;
    Interval _5469;
    float _5472;
    Interval _5489;
    Interval3 _5501;
    Interval3 _5513;
    Interval _5525;
    float _5528;
    Interval _5536;
    Interval3 _5541;
    Interval3 _5553;
    Interval3 _5565;
    Interval _5567;
    Interval _5576;
    Interval _5585;
    Interval3 _5616;
    Interval3 _5628;
    Interval _5640;
    float _5643;
    Interval _5651;
    float _5654;
    Interval _5662;
    float _5665;
    Interval _5682;
    Interval3 _5694;
    Interval _5696;
    Interval _5705;
    Interval _5714;
    Interval3 _5745;
    Interval _5747;
    Interval _5752;
    Interval _5757;
    Interval3 _5769;
    Interval _5781;
    float _5784;
    Interval _5792;
    float _5795;
    Interval _5803;
    float _5806;
    Interval _5823;
    Interval3 _5835;
    Interval _5837;
    Interval _5846;
    Interval _5855;
    Interval3 _5886;
    Interval _5888;
    Interval _5893;
    Interval _5898;
    Interval _5910;
    float _5912;
    Interval3 _5917;
    Interval3 _5929;
    Interval _5931;
    Interval _5936;
    Interval _5941;
    Interval3 _5955;
    Interval _5978;
    Interval3 _5993;
    Interval3 _6005;
    Interval _6017;
    Interval3 _6025;
    Interval _6037;
    float _6040;
    Interval _6048;
    float _6051;
    Interval _6059;
    float _6062;
    Interval _6079;
    Interval3 _6091;
    Interval3 _6104;
    Interval _6106;
    Interval _6111;
    Interval _6116;
    Interval3 _6139;
    Interval _6141;
    Interval _6146;
    Interval _6151;
    Interval3 _6174;
    Interval _6176;
    Interval _6181;
    Interval _6186;
    Interval3 _6211;
    Interval _6213;
    Interval _6218;
    Interval _6227;
    float _6230;
    Interval _6238;
    Interval _6243;
    Interval _6248;
    float _6251;
    Interval _6271;
    Interval _6273;
    Interval _6286;
    Interval _6291;
    Interval _6307;
    float _6310;
    Interval _6318;
    Interval _6323;
    Interval _6328;
    float _6331;
    Interval _6351;
    Interval _6353;
    Interval _6366;
    Interval _6371;
    Interval _6387;
    float _6390;
    Interval _6398;
    Interval _6403;
    Interval _6408;
    float _6411;
    Interval _6431;
    Interval _6433;
    Interval _6446;
    Interval _6451;
    Interval _6467;
    float _6470;
    Interval _6478;
    Interval _6483;
    Interval _6488;
    float _6491;
    Interval _6511;
    Interval _6513;
    Interval _6526;
    Interval _6531;
    Interval _6547;
    Interval _6556;
    Interval _6565;
    Interval _6574;
    Interval3 _6583;
    Interval3 _6595;
    Interval3 _6607;
    Interval3 _6619;
    Interval _6631;
    Interval _6636;
    Interval _6646;
    Interval _6651;
    Interval _6656;
    Interval3 _6661;
    Interval _6663;
    Interval _6668;
    Interval _6673;
    Interval _6678;
    Interval _6683;
    Interval _6688;
    Interval _6693;
    Interval _6698;
    Interval _6703;
    float _6706;
    Interval _6714;
    float _6717;
    Interval _6725;
    Interval _6730;
    Interval _6735;
    float _6738;
    Interval _6758;
    Interval _6763;
    Interval _6768;
    Interval _6778;
    Interval _6783;
    Interval _6788;
    Interval _6797;
    float _6800;
    Interval _6808;
    float _6811;
    Interval _6819;
    Interval _6824;
    float _6827;
    Interval _6835;
    float _6838;
    Interval _6846;
    Interval _6848;
    Interval _6861;
    Interval _6868;
    Interval _6870;
    Interval _6879;
    Interval _6884;
    Interval _6889;
    Interval _6902;
    Interval _6911;
    Interval _6916;
    Interval _6929;
    Interval _6938;
    Interval _6947;
    Interval _6949;
    Interval _6951;
    Interval _6953;
    Interval _6958;
    Interval _6963;
    Interval _6965;
    Interval _6978;
    Interval _6983;
    Interval _6999;
    Interval _7001;
    Interval _7014;
    Interval _7019;
    Interval _7035;
    Interval _7037;
    Interval _7050;
    float _7053;
    Interval _7061;
    Interval _7066;
    Interval _7071;
    float _7074;
    Interval _7094;
    Interval _7103;
    Interval _7105;
    Interval _7118;
    Interval _7123;
    Interval _7139;
    Interval _7148;
    Interval _7150;
    Interval _7163;
    Interval _7168;
    Interval _7184;
    Interval _7186;
    Interval _7199;
    Interval _7204;
    Interval _7220;
    Interval _7229;
    Interval _7234;
    Interval _7243;
    Interval _7245;
    Interval _7258;
    Interval _7263;
    Interval _7279;
    Interval _7281;
    Interval _7294;
    Interval _7299;
    Interval _7315;
    Interval _7317;
    Interval _7330;
    Interval _7335;
    Interval _7351;
    Interval _7356;
    Interval _7358;
    Interval _7371;
    Interval _7376;
    Interval _7392;
    Interval _7394;
    Interval _7407;
    Interval _7412;
    Interval _7414;
    Interval _7427;
    Interval _7432;
    Interval _7434;
    Interval _7447;
    Interval _7452;
    float _7455;
    Interval _7463;
    Interval _7468;
    Interval _7473;
    float _7476;
    Interval _7496;
    float _7499;
    Interval _7507;
    Interval _7512;
    Interval _7517;
    float _7520;
    Interval _7540;
    float _7543;
    Interval _7551;
    Interval _7556;
    Interval _7561;
    float _7564;
    Interval _7584;
    Interval _7593;
    Interval _7602;
    Interval _7611;
    Interval _7613;
    Interval _7626;
    Interval _7635;
    float _7638;
    Interval _7646;
    Interval _7651;
    Interval _7656;
    float _7659;
    Interval _7679;
    Interval3 _8090;
    Interval3 _8102;
    Interval3 _8114;
    Interval _8116;
    Interval _8121;
    Interval _8126;
    Interval3 _8138;
    Interval3 _8150;
    Interval3 _8162;
    Interval _8164;
    Interval _8169;
    Interval _8174;
    Interval3 _8186;
    Interval3 _8198;
    Interval _8200;
    Interval _8205;
    Interval _8210;
    Interval _8236;
    Interval _8241;
    Interval3 _8246;
    Interval3 _8258;
    Interval _8260;
    Interval _8265;
    Interval _8270;
    Interval3 _8282;
    Interval3 _8294;
    Interval3 _8306;
    Interval _8308;
    Interval _8313;
    Interval _8318;
    Interval3 _8330;
    Interval3 _8342;
    Interval3 _8354;
    Interval _8356;
    Interval _8361;
    Interval _8366;
    Interval3 _8378;
    Interval3 _8390;
    Interval _8392;
    Interval _8397;
    Interval _8402;
    Interval _8436;
    Interval _8437;
    Interval _8582;
    Interval _8587;
    float _8589;
    Interval _8594;
    Interval _8599;
    float _8602;
    Interval _8610;
    float _8613;
    Interval _8621;
    float _8624;
    Interval3 _8641;
    Interval3 _8653;
    Interval3 _8676;
    Interval3 _8688;
    Interval _8690;
    Interval _8695;
    Interval _8700;
    Interval3 _8714;
    Interval _8716;
    Interval _8721;
    Interval _8726;
    Interval3 _8749;
    Interval _8751;
    Interval _8756;
    Interval _8761;
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
            float _8766 = param_var_v_1.x;
            float _8763 = _8766;
            float _8764 = _8766;
            _8761.lo = _8763;
            _8761.hi = _8764;
            Interval _8762 = _8761;
            Interval _8765 = _8762;
            Interval _8767 = _8765;
            float _8768 = param_var_v_1.y;
            float _8758 = _8768;
            float _8759 = _8768;
            _8756.lo = _8758;
            _8756.hi = _8759;
            Interval _8757 = _8756;
            Interval _8760 = _8757;
            Interval _8769 = _8760;
            float _8770 = param_var_v_1.z;
            float _8753 = _8770;
            float _8754 = _8770;
            _8751.lo = _8753;
            _8751.hi = _8754;
            Interval _8752 = _8751;
            Interval _8755 = _8752;
            Interval _8771 = _8755;
            _8749.x = _8767;
            _8749.y = _8769;
            _8749.z = _8771;
            Interval3 _8750 = _8749;
            Interval3 _8772 = _8750;
            n = _8772;
        }
        else
        {
            ReflectionSpecularPlane param_var_p = plane;
            Interval3 _9342 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval3 param_var_n = _9342;
            Interval3 param_var_direction = outgoing;
            Interval3 _9344 = oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper);
            n = _9344;
        }
        Interval3 param_var_a_5 = outgoing;
        Interval3 param_var_b_4 = n;
        Interval _8738 = param_var_a_5.x;
        Interval _8739 = param_var_b_4.x;
        Interval _9351 = imul(_8738, _8739, intervalFailed, optical_product_upper);
        Interval _8740 = _9351;
        Interval _8741 = param_var_a_5.y;
        Interval _8742 = param_var_b_4.y;
        Interval _9356 = imul(_8741, _8742, intervalFailed, optical_product_upper);
        Interval _8743 = _9356;
        Interval _9357 = iadd(_8740, _8743, intervalFailed);
        Interval _8744 = _9357;
        Interval _8745 = param_var_a_5.z;
        Interval _8746 = param_var_b_4.z;
        Interval _9362 = imul(_8745, _8746, intervalFailed, optical_product_upper);
        Interval _8747 = _9362;
        Interval _9363 = iadd(_8744, _8747, intervalFailed);
        Interval _8748 = _9363;
        Interval denominator = _8748;
        if (curved)
        {
            temp_var_ternary_1 = liquid.planePoint.xyz;
        }
        else
        {
            temp_var_ternary_1 = plane.a.xyz;
        }
        float3 param_var_v_2 = temp_var_ternary_1;
        float _8731 = param_var_v_2.x;
        float _8728 = _8731;
        float _8729 = _8731;
        _8726.lo = _8728;
        _8726.hi = _8729;
        Interval _8727 = _8726;
        Interval _8730 = _8727;
        Interval _8732 = _8730;
        float _8733 = param_var_v_2.y;
        float _8723 = _8733;
        float _8724 = _8733;
        _8721.lo = _8723;
        _8721.hi = _8724;
        Interval _8722 = _8721;
        Interval _8725 = _8722;
        Interval _8734 = _8725;
        float _8735 = param_var_v_2.z;
        float _8718 = _8735;
        float _8719 = _8735;
        _8716.lo = _8718;
        _8716.hi = _8719;
        Interval _8717 = _8716;
        Interval _8720 = _8717;
        Interval _8736 = _8720;
        _8714.x = _8732;
        _8714.y = _8734;
        _8714.z = _8736;
        Interval3 _8715 = _8714;
        Interval3 _8737 = _8715;
        Interval3 param_var_a_6 = _8737;
        Interval3 param_var_b_5 = origin;
        Interval3 _8705 = param_var_a_6;
        Interval _8706 = param_var_b_5.x;
        float _8702 = as_type<float>(as_type<uint>(_8706.hi) ^ 2147483648u);
        float _8703 = as_type<float>(as_type<uint>(_8706.lo) ^ 2147483648u);
        _8700.lo = _8702;
        _8700.hi = _8703;
        Interval _8701 = _8700;
        Interval _8704 = _8701;
        Interval _8707 = _8704;
        Interval _8708 = param_var_b_5.y;
        float _8697 = as_type<float>(as_type<uint>(_8708.hi) ^ 2147483648u);
        float _8698 = as_type<float>(as_type<uint>(_8708.lo) ^ 2147483648u);
        _8695.lo = _8697;
        _8695.hi = _8698;
        Interval _8696 = _8695;
        Interval _8699 = _8696;
        Interval _8709 = _8699;
        Interval _8710 = param_var_b_5.z;
        float _8692 = as_type<float>(as_type<uint>(_8710.hi) ^ 2147483648u);
        float _8693 = as_type<float>(as_type<uint>(_8710.lo) ^ 2147483648u);
        _8690.lo = _8692;
        _8690.hi = _8693;
        Interval _8691 = _8690;
        Interval _8694 = _8691;
        Interval _8711 = _8694;
        _8688.x = _8707;
        _8688.y = _8709;
        _8688.z = _8711;
        Interval3 _8689 = _8688;
        Interval3 _8712 = _8689;
        Interval _8678 = _8705.x;
        Interval _8679 = _8712.x;
        Interval _9486 = iadd(_8678, _8679, intervalFailed);
        Interval _8680 = _9486;
        Interval _8681 = _8705.y;
        Interval _8682 = _8712.y;
        Interval _9491 = iadd(_8681, _8682, intervalFailed);
        Interval _8683 = _9491;
        Interval _8684 = _8705.z;
        Interval _8685 = _8712.z;
        Interval _9496 = iadd(_8684, _8685, intervalFailed);
        Interval _8686 = _9496;
        _8676.x = _8680;
        _8676.y = _8683;
        _8676.z = _8686;
        Interval3 _8677 = _8676;
        Interval3 _8687 = _8677;
        Interval3 _8713 = _8687;
        Interval3 param_var_a_7 = _8713;
        Interval3 param_var_b_6 = n;
        Interval _8665 = param_var_a_7.x;
        Interval _8666 = param_var_b_6.x;
        Interval _9512 = imul(_8665, _8666, intervalFailed, optical_product_upper);
        Interval _8667 = _9512;
        Interval _8668 = param_var_a_7.y;
        Interval _8669 = param_var_b_6.y;
        Interval _9517 = imul(_8668, _8669, intervalFailed, optical_product_upper);
        Interval _8670 = _9517;
        Interval _9518 = iadd(_8667, _8670, intervalFailed);
        Interval _8671 = _9518;
        Interval _8672 = param_var_a_7.z;
        Interval _8673 = param_var_b_6.z;
        Interval _9523 = imul(_8672, _8673, intervalFailed, optical_product_upper);
        Interval _8674 = _9523;
        Interval _9524 = iadd(_8671, _8674, intervalFailed);
        Interval _8675 = _9524;
        Interval param_var_a_8 = _8675;
        Interval param_var_b_7 = denominator;
        Interval _9527 = idiv(param_var_a_8, param_var_b_7, intervalFailed, interval_divide_upper);
        Interval _distance = _9527;
        if (primary)
        {
            Interval param_var_a_9 = _distance;
            Interval param_var_b_8 = outgoing.z;
            Interval _9532 = imul(param_var_a_9, param_var_b_8, intervalFailed, optical_product_upper);
            Interval depth = _9532;
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
        Interval _8655 = param_var_a_11.x;
        Interval _8656 = param_var_b_9;
        Interval _9570 = imul(_8655, _8656, intervalFailed, optical_product_upper);
        Interval _8657 = _9570;
        Interval _8658 = param_var_a_11.y;
        Interval _8659 = param_var_b_9;
        Interval _9574 = imul(_8658, _8659, intervalFailed, optical_product_upper);
        Interval _8660 = _9574;
        Interval _8661 = param_var_a_11.z;
        Interval _8662 = param_var_b_9;
        Interval _9578 = imul(_8661, _8662, intervalFailed, optical_product_upper);
        Interval _8663 = _9578;
        _8653.x = _8657;
        _8653.y = _8660;
        _8653.z = _8663;
        Interval3 _8654 = _8653;
        Interval3 _8664 = _8654;
        Interval3 param_var_b_10 = _8664;
        Interval _8643 = param_var_a_10.x;
        Interval _8644 = param_var_b_10.x;
        Interval _9592 = iadd(_8643, _8644, intervalFailed);
        Interval _8645 = _9592;
        Interval _8646 = param_var_a_10.y;
        Interval _8647 = param_var_b_10.y;
        Interval _9597 = iadd(_8646, _8647, intervalFailed);
        Interval _8648 = _9597;
        Interval _8649 = param_var_a_10.z;
        Interval _8650 = param_var_b_10.z;
        Interval _9602 = iadd(_8649, _8650, intervalFailed);
        Interval _8651 = _9602;
        _8641.x = _8645;
        _8641.y = _8648;
        _8641.z = _8651;
        Interval3 _8642 = _8641;
        Interval3 _8652 = _8642;
        Interval3 hit = _8652;
        if (curved)
        {
            if (primary)
            {
                radius = _distance;
            }
            else
            {
                Interval3 param_var_a_12 = hit;
                Interval _8632 = param_var_a_12.x;
                bool _8625 = false;
                if (_8632.lo <= 0.0)
                {
                    _8625 = _8632.hi >= 0.0;
                }
                if (_8625)
                {
                    _8624 = 0.0;
                }
                else
                {
                    _8624 = precise::min(abs(_8632.lo), abs(_8632.hi));
                }
                float _8623 = _8624;
                float _8626 = precise::max(abs(_8632.lo), abs(_8632.hi));
                float _8627 = spvFMul(_8623, _8623);
                float _9647 = interval_down(_8627, intervalFailed);
                float _8628 = precise::max(0.0, _9647);
                float _8629 = spvFMul(_8626, _8626);
                float _9651 = interval_up(_8629, intervalFailed);
                float _8630 = _9651;
                _8621.lo = _8628;
                _8621.hi = _8630;
                Interval _8622 = _8621;
                Interval _8631 = _8622;
                Interval _8633 = _8631;
                Interval _8634 = param_var_a_12.y;
                bool _8614 = false;
                if (_8634.lo <= 0.0)
                {
                    _8614 = _8634.hi >= 0.0;
                }
                if (_8614)
                {
                    _8613 = 0.0;
                }
                else
                {
                    _8613 = precise::min(abs(_8634.lo), abs(_8634.hi));
                }
                float _8612 = _8613;
                float _8615 = precise::max(abs(_8634.lo), abs(_8634.hi));
                float _8616 = spvFMul(_8612, _8612);
                float _9690 = interval_down(_8616, intervalFailed);
                float _8617 = precise::max(0.0, _9690);
                float _8618 = spvFMul(_8615, _8615);
                float _9694 = interval_up(_8618, intervalFailed);
                float _8619 = _9694;
                _8610.lo = _8617;
                _8610.hi = _8619;
                Interval _8611 = _8610;
                Interval _8620 = _8611;
                Interval _8635 = _8620;
                Interval _9702 = iadd(_8633, _8635, intervalFailed);
                Interval _8636 = _9702;
                Interval _8637 = param_var_a_12.z;
                bool _8603 = false;
                if (_8637.lo <= 0.0)
                {
                    _8603 = _8637.hi >= 0.0;
                }
                if (_8603)
                {
                    _8602 = 0.0;
                }
                else
                {
                    _8602 = precise::min(abs(_8637.lo), abs(_8637.hi));
                }
                float _8601 = _8602;
                float _8604 = precise::max(abs(_8637.lo), abs(_8637.hi));
                float _8605 = spvFMul(_8601, _8601);
                float _9734 = interval_down(_8605, intervalFailed);
                float _8606 = precise::max(0.0, _9734);
                float _8607 = spvFMul(_8604, _8604);
                float _9738 = interval_up(_8607, intervalFailed);
                float _8608 = _9738;
                _8599.lo = _8606;
                _8599.hi = _8608;
                Interval _8600 = _8599;
                Interval _8609 = _8600;
                Interval _8638 = _8609;
                Interval _9746 = iadd(_8636, _8638, intervalFailed);
                Interval _8639 = _9746;
                Interval _9747 = isqrt(_8639, intervalFailed);
                Interval _8640 = _9747;
                radius = _8640;
            }
            Interval param_var_a_13 = radius;
            float param_var_x_7 = precise::max(liquid.projection.x, 1.0);
            float _8596 = param_var_x_7;
            float _8597 = param_var_x_7;
            _8594.lo = _8596;
            _8594.hi = _8597;
            Interval _8595 = _8594;
            Interval _8598 = _8595;
            Interval param_var_b_11 = _8598;
            Interval _9763 = idiv(param_var_a_13, param_var_b_11, intervalFailed, interval_divide_upper);
            Interval param_var_a_14 = _9763;
            Interval param_var_a_15 = denominator;
            bool _8590 = false;
            if (param_var_a_15.lo <= 0.0)
            {
                _8590 = param_var_a_15.hi >= 0.0;
            }
            if (_8590)
            {
                _8589 = 0.0;
            }
            else
            {
                _8589 = precise::min(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            }
            float _8591 = _8589;
            float _8592 = precise::max(abs(param_var_a_15.lo), abs(param_var_a_15.hi));
            _8587.lo = _8591;
            _8587.hi = _8592;
            Interval _8588 = _8587;
            Interval _8593 = _8588;
            Interval param_var_a_16 = _8593;
            float param_var_n_1 = 4.0;
            float param_var_d = 100.0;
            Interval _9799 = iratio(param_var_n_1, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_12 = _9799;
            float _8584 = precise::max(param_var_a_16.lo, param_var_b_12.lo);
            float _8585 = precise::max(param_var_a_16.hi, param_var_b_12.hi);
            _8582.lo = _8584;
            _8582.hi = _8585;
            Interval _8583 = _8582;
            Interval _8586 = _8583;
            Interval param_var_b_13 = _8586;
            Interval _9817 = idiv(param_var_a_14, param_var_b_13, intervalFailed, interval_divide_upper);
            Interval footprint = _9817;
            ReflectionLiquidFrame param_var_f = liquid;
            Interval3 param_var_direction_1 = outgoing;
            Interval param_var_distance = _distance;
            Interval param_var_footprint = footprint;
            Interval3 _8429 = hit;
            ReflectionLiquidFrame _8430 = param_var_f;
            float3 _8414 = _8430.rotation0.xyz;
            float _8407 = _8414.x;
            float _8404 = _8407;
            float _8405 = _8407;
            _8402.lo = _8404;
            _8402.hi = _8405;
            Interval _8403 = _8402;
            Interval _8406 = _8403;
            Interval _8408 = _8406;
            float _8409 = _8414.y;
            float _8399 = _8409;
            float _8400 = _8409;
            _8397.lo = _8399;
            _8397.hi = _8400;
            Interval _8398 = _8397;
            Interval _8401 = _8398;
            Interval _8410 = _8401;
            float _8411 = _8414.z;
            float _8394 = _8411;
            float _8395 = _8411;
            _8392.lo = _8394;
            _8392.hi = _8395;
            Interval _8393 = _8392;
            Interval _8396 = _8393;
            Interval _8412 = _8396;
            _8390.x = _8408;
            _8390.y = _8410;
            _8390.z = _8412;
            Interval3 _8391 = _8390;
            Interval3 _8413 = _8391;
            Interval3 _8415 = _8413;
            Interval _8416 = _8429.x;
            Interval _8380 = _8415.x;
            Interval _8381 = _8416;
            Interval _9874 = imul(_8380, _8381, intervalFailed, optical_product_upper);
            Interval _8382 = _9874;
            Interval _8383 = _8415.y;
            Interval _8384 = _8416;
            Interval _9878 = imul(_8383, _8384, intervalFailed, optical_product_upper);
            Interval _8385 = _9878;
            Interval _8386 = _8415.z;
            Interval _8387 = _8416;
            Interval _9882 = imul(_8386, _8387, intervalFailed, optical_product_upper);
            Interval _8388 = _9882;
            _8378.x = _8382;
            _8378.y = _8385;
            _8378.z = _8388;
            Interval3 _8379 = _8378;
            Interval3 _8389 = _8379;
            Interval3 _8417 = _8389;
            float3 _8418 = _8430.rotation1.xyz;
            float _8371 = _8418.x;
            float _8368 = _8371;
            float _8369 = _8371;
            _8366.lo = _8368;
            _8366.hi = _8369;
            Interval _8367 = _8366;
            Interval _8370 = _8367;
            Interval _8372 = _8370;
            float _8373 = _8418.y;
            float _8363 = _8373;
            float _8364 = _8373;
            _8361.lo = _8363;
            _8361.hi = _8364;
            Interval _8362 = _8361;
            Interval _8365 = _8362;
            Interval _8374 = _8365;
            float _8375 = _8418.z;
            float _8358 = _8375;
            float _8359 = _8375;
            _8356.lo = _8358;
            _8356.hi = _8359;
            Interval _8357 = _8356;
            Interval _8360 = _8357;
            Interval _8376 = _8360;
            _8354.x = _8372;
            _8354.y = _8374;
            _8354.z = _8376;
            Interval3 _8355 = _8354;
            Interval3 _8377 = _8355;
            Interval3 _8419 = _8377;
            Interval _8420 = _8429.y;
            Interval _8344 = _8419.x;
            Interval _8345 = _8420;
            Interval _9942 = imul(_8344, _8345, intervalFailed, optical_product_upper);
            Interval _8346 = _9942;
            Interval _8347 = _8419.y;
            Interval _8348 = _8420;
            Interval _9946 = imul(_8347, _8348, intervalFailed, optical_product_upper);
            Interval _8349 = _9946;
            Interval _8350 = _8419.z;
            Interval _8351 = _8420;
            Interval _9950 = imul(_8350, _8351, intervalFailed, optical_product_upper);
            Interval _8352 = _9950;
            _8342.x = _8346;
            _8342.y = _8349;
            _8342.z = _8352;
            Interval3 _8343 = _8342;
            Interval3 _8353 = _8343;
            Interval3 _8421 = _8353;
            Interval _8332 = _8417.x;
            Interval _8333 = _8421.x;
            Interval _9964 = iadd(_8332, _8333, intervalFailed);
            Interval _8334 = _9964;
            Interval _8335 = _8417.y;
            Interval _8336 = _8421.y;
            Interval _9969 = iadd(_8335, _8336, intervalFailed);
            Interval _8337 = _9969;
            Interval _8338 = _8417.z;
            Interval _8339 = _8421.z;
            Interval _9974 = iadd(_8338, _8339, intervalFailed);
            Interval _8340 = _9974;
            _8330.x = _8334;
            _8330.y = _8337;
            _8330.z = _8340;
            Interval3 _8331 = _8330;
            Interval3 _8341 = _8331;
            Interval3 _8422 = _8341;
            float3 _8423 = _8430.rotation2.xyz;
            float _8323 = _8423.x;
            float _8320 = _8323;
            float _8321 = _8323;
            _8318.lo = _8320;
            _8318.hi = _8321;
            Interval _8319 = _8318;
            Interval _8322 = _8319;
            Interval _8324 = _8322;
            float _8325 = _8423.y;
            float _8315 = _8325;
            float _8316 = _8325;
            _8313.lo = _8315;
            _8313.hi = _8316;
            Interval _8314 = _8313;
            Interval _8317 = _8314;
            Interval _8326 = _8317;
            float _8327 = _8423.z;
            float _8310 = _8327;
            float _8311 = _8327;
            _8308.lo = _8310;
            _8308.hi = _8311;
            Interval _8309 = _8308;
            Interval _8312 = _8309;
            Interval _8328 = _8312;
            _8306.x = _8324;
            _8306.y = _8326;
            _8306.z = _8328;
            Interval3 _8307 = _8306;
            Interval3 _8329 = _8307;
            Interval3 _8424 = _8329;
            Interval _8425 = _8429.z;
            Interval _8296 = _8424.x;
            Interval _8297 = _8425;
            Interval _10034 = imul(_8296, _8297, intervalFailed, optical_product_upper);
            Interval _8298 = _10034;
            Interval _8299 = _8424.y;
            Interval _8300 = _8425;
            Interval _10038 = imul(_8299, _8300, intervalFailed, optical_product_upper);
            Interval _8301 = _10038;
            Interval _8302 = _8424.z;
            Interval _8303 = _8425;
            Interval _10042 = imul(_8302, _8303, intervalFailed, optical_product_upper);
            Interval _8304 = _10042;
            _8294.x = _8298;
            _8294.y = _8301;
            _8294.z = _8304;
            Interval3 _8295 = _8294;
            Interval3 _8305 = _8295;
            Interval3 _8426 = _8305;
            Interval _8284 = _8422.x;
            Interval _8285 = _8426.x;
            Interval _10056 = iadd(_8284, _8285, intervalFailed);
            Interval _8286 = _10056;
            Interval _8287 = _8422.y;
            Interval _8288 = _8426.y;
            Interval _10061 = iadd(_8287, _8288, intervalFailed);
            Interval _8289 = _10061;
            Interval _8290 = _8422.z;
            Interval _8291 = _8426.z;
            Interval _10066 = iadd(_8290, _8291, intervalFailed);
            Interval _8292 = _10066;
            _8282.x = _8286;
            _8282.y = _8289;
            _8282.z = _8292;
            Interval3 _8283 = _8282;
            Interval3 _8293 = _8283;
            Interval3 _8427 = _8293;
            Interval3 _8431 = _8427;
            float3 _8432 = float3(param_var_f.rotation0.w, param_var_f.rotation1.w, param_var_f.rotation2.w);
            float _8275 = _8432.x;
            float _8272 = _8275;
            float _8273 = _8275;
            _8270.lo = _8272;
            _8270.hi = _8273;
            Interval _8271 = _8270;
            Interval _8274 = _8271;
            Interval _8276 = _8274;
            float _8277 = _8432.y;
            float _8267 = _8277;
            float _8268 = _8277;
            _8265.lo = _8267;
            _8265.hi = _8268;
            Interval _8266 = _8265;
            Interval _8269 = _8266;
            Interval _8278 = _8269;
            float _8279 = _8432.z;
            float _8262 = _8279;
            float _8263 = _8279;
            _8260.lo = _8262;
            _8260.hi = _8263;
            Interval _8261 = _8260;
            Interval _8264 = _8261;
            Interval _8280 = _8264;
            _8258.x = _8276;
            _8258.y = _8278;
            _8258.z = _8280;
            Interval3 _8259 = _8258;
            Interval3 _8281 = _8259;
            Interval3 _8433 = _8281;
            Interval _8248 = _8431.x;
            Interval _8249 = _8433.x;
            Interval _10133 = iadd(_8248, _8249, intervalFailed);
            Interval _8250 = _10133;
            Interval _8251 = _8431.y;
            Interval _8252 = _8433.y;
            Interval _10138 = iadd(_8251, _8252, intervalFailed);
            Interval _8253 = _10138;
            Interval _8254 = _8431.z;
            Interval _8255 = _8433.z;
            Interval _10143 = iadd(_8254, _8255, intervalFailed);
            Interval _8256 = _10143;
            _8246.x = _8250;
            _8246.y = _8253;
            _8246.z = _8256;
            Interval3 _8247 = _8246;
            Interval3 _8257 = _8247;
            Interval3 _8428 = _8257;
            float _8435 = param_var_f.settings.x;
            float _8243 = _8435;
            float _8244 = _8435;
            _8241.lo = _8243;
            _8241.hi = _8244;
            Interval _8242 = _8241;
            Interval _8245 = _8242;
            Interval _8434 = _8245;
            if (param_var_f.settings.y == 3.0)
            {
                float _8438 = 0.0;
                float _8238 = _8438;
                float _8239 = _8438;
                _8236.lo = _8238;
                _8236.hi = _8239;
                Interval _8237 = _8236;
                Interval _8240 = _8237;
                _8437 = _8240;
                _8436 = _8240;
                Interval3 _8440 = param_var_direction_1;
                ReflectionLiquidFrame _8441 = param_var_f;
                float3 _8222 = _8441.rotation0.xyz;
                float _8215 = _8222.x;
                float _8212 = _8215;
                float _8213 = _8215;
                _8210.lo = _8212;
                _8210.hi = _8213;
                Interval _8211 = _8210;
                Interval _8214 = _8211;
                Interval _8216 = _8214;
                float _8217 = _8222.y;
                float _8207 = _8217;
                float _8208 = _8217;
                _8205.lo = _8207;
                _8205.hi = _8208;
                Interval _8206 = _8205;
                Interval _8209 = _8206;
                Interval _8218 = _8209;
                float _8219 = _8222.z;
                float _8202 = _8219;
                float _8203 = _8219;
                _8200.lo = _8202;
                _8200.hi = _8203;
                Interval _8201 = _8200;
                Interval _8204 = _8201;
                Interval _8220 = _8204;
                _8198.x = _8216;
                _8198.y = _8218;
                _8198.z = _8220;
                Interval3 _8199 = _8198;
                Interval3 _8221 = _8199;
                Interval3 _8223 = _8221;
                Interval _8224 = _8440.x;
                Interval _8188 = _8223.x;
                Interval _8189 = _8224;
                Interval _10233 = imul(_8188, _8189, intervalFailed, optical_product_upper);
                Interval _8190 = _10233;
                Interval _8191 = _8223.y;
                Interval _8192 = _8224;
                Interval _10237 = imul(_8191, _8192, intervalFailed, optical_product_upper);
                Interval _8193 = _10237;
                Interval _8194 = _8223.z;
                Interval _8195 = _8224;
                Interval _10241 = imul(_8194, _8195, intervalFailed, optical_product_upper);
                Interval _8196 = _10241;
                _8186.x = _8190;
                _8186.y = _8193;
                _8186.z = _8196;
                Interval3 _8187 = _8186;
                Interval3 _8197 = _8187;
                Interval3 _8225 = _8197;
                float3 _8226 = _8441.rotation1.xyz;
                float _8179 = _8226.x;
                float _8176 = _8179;
                float _8177 = _8179;
                _8174.lo = _8176;
                _8174.hi = _8177;
                Interval _8175 = _8174;
                Interval _8178 = _8175;
                Interval _8180 = _8178;
                float _8181 = _8226.y;
                float _8171 = _8181;
                float _8172 = _8181;
                _8169.lo = _8171;
                _8169.hi = _8172;
                Interval _8170 = _8169;
                Interval _8173 = _8170;
                Interval _8182 = _8173;
                float _8183 = _8226.z;
                float _8166 = _8183;
                float _8167 = _8183;
                _8164.lo = _8166;
                _8164.hi = _8167;
                Interval _8165 = _8164;
                Interval _8168 = _8165;
                Interval _8184 = _8168;
                _8162.x = _8180;
                _8162.y = _8182;
                _8162.z = _8184;
                Interval3 _8163 = _8162;
                Interval3 _8185 = _8163;
                Interval3 _8227 = _8185;
                Interval _8228 = _8440.y;
                Interval _8152 = _8227.x;
                Interval _8153 = _8228;
                Interval _10301 = imul(_8152, _8153, intervalFailed, optical_product_upper);
                Interval _8154 = _10301;
                Interval _8155 = _8227.y;
                Interval _8156 = _8228;
                Interval _10305 = imul(_8155, _8156, intervalFailed, optical_product_upper);
                Interval _8157 = _10305;
                Interval _8158 = _8227.z;
                Interval _8159 = _8228;
                Interval _10309 = imul(_8158, _8159, intervalFailed, optical_product_upper);
                Interval _8160 = _10309;
                _8150.x = _8154;
                _8150.y = _8157;
                _8150.z = _8160;
                Interval3 _8151 = _8150;
                Interval3 _8161 = _8151;
                Interval3 _8229 = _8161;
                Interval _8140 = _8225.x;
                Interval _8141 = _8229.x;
                Interval _10323 = iadd(_8140, _8141, intervalFailed);
                Interval _8142 = _10323;
                Interval _8143 = _8225.y;
                Interval _8144 = _8229.y;
                Interval _10328 = iadd(_8143, _8144, intervalFailed);
                Interval _8145 = _10328;
                Interval _8146 = _8225.z;
                Interval _8147 = _8229.z;
                Interval _10333 = iadd(_8146, _8147, intervalFailed);
                Interval _8148 = _10333;
                _8138.x = _8142;
                _8138.y = _8145;
                _8138.z = _8148;
                Interval3 _8139 = _8138;
                Interval3 _8149 = _8139;
                Interval3 _8230 = _8149;
                float3 _8231 = _8441.rotation2.xyz;
                float _8131 = _8231.x;
                float _8128 = _8131;
                float _8129 = _8131;
                _8126.lo = _8128;
                _8126.hi = _8129;
                Interval _8127 = _8126;
                Interval _8130 = _8127;
                Interval _8132 = _8130;
                float _8133 = _8231.y;
                float _8123 = _8133;
                float _8124 = _8133;
                _8121.lo = _8123;
                _8121.hi = _8124;
                Interval _8122 = _8121;
                Interval _8125 = _8122;
                Interval _8134 = _8125;
                float _8135 = _8231.z;
                float _8118 = _8135;
                float _8119 = _8135;
                _8116.lo = _8118;
                _8116.hi = _8119;
                Interval _8117 = _8116;
                Interval _8120 = _8117;
                Interval _8136 = _8120;
                _8114.x = _8132;
                _8114.y = _8134;
                _8114.z = _8136;
                Interval3 _8115 = _8114;
                Interval3 _8137 = _8115;
                Interval3 _8232 = _8137;
                Interval _8233 = _8440.z;
                Interval _8104 = _8232.x;
                Interval _8105 = _8233;
                Interval _10393 = imul(_8104, _8105, intervalFailed, optical_product_upper);
                Interval _8106 = _10393;
                Interval _8107 = _8232.y;
                Interval _8108 = _8233;
                Interval _10397 = imul(_8107, _8108, intervalFailed, optical_product_upper);
                Interval _8109 = _10397;
                Interval _8110 = _8232.z;
                Interval _8111 = _8233;
                Interval _10401 = imul(_8110, _8111, intervalFailed, optical_product_upper);
                Interval _8112 = _10401;
                _8102.x = _8106;
                _8102.y = _8109;
                _8102.z = _8112;
                Interval3 _8103 = _8102;
                Interval3 _8113 = _8103;
                Interval3 _8234 = _8113;
                Interval _8092 = _8230.x;
                Interval _8093 = _8234.x;
                Interval _10415 = iadd(_8092, _8093, intervalFailed);
                Interval _8094 = _10415;
                Interval _8095 = _8230.y;
                Interval _8096 = _8234.y;
                Interval _10420 = iadd(_8095, _8096, intervalFailed);
                Interval _8097 = _10420;
                Interval _8098 = _8230.z;
                Interval _8099 = _8234.z;
                Interval _10425 = iadd(_8098, _8099, intervalFailed);
                Interval _8100 = _10425;
                _8090.x = _8094;
                _8090.y = _8097;
                _8090.z = _8100;
                Interval3 _8091 = _8090;
                Interval3 _8101 = _8091;
                Interval3 _8235 = _8101;
                Interval3 _8439 = _8235;
                for (uint _8442 = 0u; _8442 < 2u; _8442++)
                {
                    Interval _8444 = _8428.x;
                    Interval _8445 = _8428.z;
                    Interval _8446 = _8434;
                    Interval _8447 = param_var_footprint;
                    Interval _7689 = _8444;
                    float _7690 = 6.0;
                    float _7691 = 1000.0;
                    Interval _10449 = iratio(_7690, _7691, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7692 = _10449;
                    Interval _10450 = imul(_7689, _7692, intervalFailed, optical_product_upper);
                    Interval _7693 = _10450;
                    Interval _7694 = _8445;
                    float _7695 = 8.0;
                    float _7696 = 1000.0;
                    Interval _10452 = iratio(_7695, _7696, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7697 = _10452;
                    Interval _10453 = imul(_7694, _7697, intervalFailed, optical_product_upper);
                    Interval _7698 = _10453;
                    Interval _7684 = _7693;
                    Interval _7685 = _7698;
                    float _7681 = as_type<float>(as_type<uint>(_7685.hi) ^ 2147483648u);
                    float _7682 = as_type<float>(as_type<uint>(_7685.lo) ^ 2147483648u);
                    _7679.lo = _7681;
                    _7679.hi = _7682;
                    Interval _7680 = _7679;
                    Interval _7683 = _7680;
                    Interval _7686 = _7683;
                    Interval _10473 = iadd(_7684, _7686, intervalFailed);
                    Interval _7687 = _10473;
                    Interval _7699 = _7687;
                    Interval _7700 = _8446;
                    float _7701 = 11.0;
                    float _7702 = 100.0;
                    Interval _10476 = iratio(_7701, _7702, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7703 = _10476;
                    Interval _10477 = imul(_7700, _7703, intervalFailed, optical_product_upper);
                    Interval _7704 = _10477;
                    Interval _10478 = iadd(_7699, _7704, intervalFailed);
                    Interval _7688 = _10478;
                    Interval _7706 = _8447;
                    float _7707 = 10.0;
                    float _7708 = 1000.0;
                    Interval _10480 = iratio(_7707, _7708, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7709 = _10480;
                    Interval _7668 = _7706;
                    Interval _7669 = _7709;
                    Interval _10483 = imul(_7668, _7669, intervalFailed, optical_product_upper);
                    Interval _7670 = _10483;
                    bool _7660 = false;
                    if (_7670.lo <= 0.0)
                    {
                        _7660 = _7670.hi >= 0.0;
                    }
                    if (_7660)
                    {
                        _7659 = 0.0;
                    }
                    else
                    {
                        _7659 = precise::min(abs(_7670.lo), abs(_7670.hi));
                    }
                    float _7658 = _7659;
                    float _7661 = precise::max(abs(_7670.lo), abs(_7670.hi));
                    float _7662 = spvFMul(_7658, _7658);
                    float _10513 = interval_down(_7662, intervalFailed);
                    float _7663 = precise::max(0.0, _10513);
                    float _7664 = spvFMul(_7661, _7661);
                    float _10517 = interval_up(_7664, intervalFailed);
                    float _7665 = _10517;
                    _7656.lo = _7663;
                    _7656.hi = _7665;
                    Interval _7657 = _7656;
                    Interval _7666 = _7657;
                    Interval _7667 = _7666;
                    float _7671 = 1.0;
                    float _7653 = _7671;
                    float _7654 = _7671;
                    _7651.lo = _7653;
                    _7651.hi = _7654;
                    Interval _7652 = _7651;
                    Interval _7655 = _7652;
                    Interval _7672 = _7655;
                    float _7673 = 1.0;
                    float _7648 = _7673;
                    float _7649 = _7673;
                    _7646.lo = _7648;
                    _7646.hi = _7649;
                    Interval _7647 = _7646;
                    Interval _7650 = _7647;
                    Interval _7674 = _7650;
                    Interval _7675 = _7667;
                    bool _7639 = false;
                    if (_7675.lo <= 0.0)
                    {
                        _7639 = _7675.hi >= 0.0;
                    }
                    if (_7639)
                    {
                        _7638 = 0.0;
                    }
                    else
                    {
                        _7638 = precise::min(abs(_7675.lo), abs(_7675.hi));
                    }
                    float _7637 = _7638;
                    float _7640 = precise::max(abs(_7675.lo), abs(_7675.hi));
                    float _7641 = spvFMul(_7637, _7637);
                    float _10573 = interval_down(_7641, intervalFailed);
                    float _7642 = precise::max(0.0, _10573);
                    float _7643 = spvFMul(_7640, _7640);
                    float _10577 = interval_up(_7643, intervalFailed);
                    float _7644 = _10577;
                    _7635.lo = _7642;
                    _7635.hi = _7644;
                    Interval _7636 = _7635;
                    Interval _7645 = _7636;
                    Interval _7676 = _7645;
                    Interval _10585 = iadd(_7674, _7676, intervalFailed);
                    Interval _7677 = _10585;
                    Interval _10586 = idiv(_7672, _7677, intervalFailed, interval_divide_upper);
                    Interval _7678 = _10586;
                    Interval _7705 = _7678;
                    Interval _7711 = _8444;
                    float _7712 = 18.0;
                    float _7713 = 1000.0;
                    Interval _10589 = iratio(_7712, _7713, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7714 = _10589;
                    Interval _10590 = imul(_7711, _7714, intervalFailed, optical_product_upper);
                    Interval _7715 = _10590;
                    Interval _7716 = _8445;
                    float _7717 = 11.0;
                    float _7718 = 1000.0;
                    Interval _10592 = iratio(_7717, _7718, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7719 = _10592;
                    Interval _10593 = imul(_7716, _7719, intervalFailed, optical_product_upper);
                    Interval _7720 = _10593;
                    Interval _10594 = iadd(_7715, _7720, intervalFailed);
                    Interval _7721 = _10594;
                    Interval _7722 = _8446;
                    float _7723 = 45.0;
                    float _7724 = 100.0;
                    Interval _10596 = iratio(_7723, _7724, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7725 = _10596;
                    Interval _10597 = imul(_7722, _7725, intervalFailed, optical_product_upper);
                    Interval _7726 = _10597;
                    Interval _7631 = _7721;
                    Interval _7632 = _7726;
                    float _7628 = as_type<float>(as_type<uint>(_7632.hi) ^ 2147483648u);
                    float _7629 = as_type<float>(as_type<uint>(_7632.lo) ^ 2147483648u);
                    _7626.lo = _7628;
                    _7626.hi = _7629;
                    Interval _7627 = _7626;
                    Interval _7630 = _7627;
                    Interval _7633 = _7630;
                    Interval _10617 = iadd(_7631, _7633, intervalFailed);
                    Interval _7634 = _10617;
                    Interval _7727 = _7634;
                    float _7728 = 65.0;
                    float _7729 = 100.0;
                    Interval _10619 = iratio(_7728, _7729, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7730 = _10619;
                    Interval _7731 = _7688;
                    float _7621 = _7731.lo;
                    float _7622 = _7731.hi;
                    float _7616 = _7621;
                    float _7617 = _7622;
                    _7613.lo = _7616;
                    _7613.hi = _7617;
                    Interval _7614 = _7613;
                    Interval _7618 = _7614;
                    Interval _10633 = isin_body(_7618, intervalFailed, optical_product_upper);
                    Interval _7615 = _10633;
                    interval_sine_upper = _7615.hi;
                    float _7619 = _7615.lo;
                    float _7620 = _7619;
                    float _7623 = _7620;
                    float _7624 = interval_sine_upper;
                    _7611.lo = _7623;
                    _7611.hi = _7624;
                    Interval _7612 = _7611;
                    Interval _7625 = _7612;
                    Interval _7732 = _7625;
                    Interval _10648 = imul(_7730, _7732, intervalFailed, optical_product_upper);
                    Interval _7733 = _10648;
                    Interval _7734 = _7705;
                    Interval _10650 = imul(_7733, _7734, intervalFailed, optical_product_upper);
                    Interval _7735 = _10650;
                    Interval _10651 = iadd(_7727, _7735, intervalFailed);
                    Interval _7710 = _10651;
                    Interval _7737 = _8444;
                    float _7738 = 47.0;
                    float _7739 = 1000.0;
                    Interval _10653 = iratio(_7738, _7739, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7740 = _10653;
                    Interval _10654 = imul(_7737, _7740, intervalFailed, optical_product_upper);
                    Interval _7741 = _10654;
                    Interval _7742 = _8445;
                    float _7743 = 25.0;
                    float _7744 = 1000.0;
                    Interval _10656 = iratio(_7743, _7744, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7745 = _10656;
                    Interval _10657 = imul(_7742, _7745, intervalFailed, optical_product_upper);
                    Interval _7746 = _10657;
                    Interval _7607 = _7741;
                    Interval _7608 = _7746;
                    float _7604 = as_type<float>(as_type<uint>(_7608.hi) ^ 2147483648u);
                    float _7605 = as_type<float>(as_type<uint>(_7608.lo) ^ 2147483648u);
                    _7602.lo = _7604;
                    _7602.hi = _7605;
                    Interval _7603 = _7602;
                    Interval _7606 = _7603;
                    Interval _7609 = _7606;
                    Interval _10677 = iadd(_7607, _7609, intervalFailed);
                    Interval _7610 = _10677;
                    Interval _7747 = _7610;
                    Interval _7748 = _8446;
                    float _7749 = 60.0;
                    float _7750 = 100.0;
                    Interval _10680 = iratio(_7749, _7750, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7751 = _10680;
                    Interval _10681 = imul(_7748, _7751, intervalFailed, optical_product_upper);
                    Interval _7752 = _10681;
                    Interval _10682 = iadd(_7747, _7752, intervalFailed);
                    Interval _7736 = _10682;
                    Interval _7754 = _8445;
                    float _7755 = 22.0;
                    float _7756 = 1000.0;
                    Interval _10684 = iratio(_7755, _7756, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7757 = _10684;
                    Interval _10685 = imul(_7754, _7757, intervalFailed, optical_product_upper);
                    Interval _7758 = _10685;
                    Interval _7759 = _8444;
                    float _7760 = 9.0;
                    float _7761 = 1000.0;
                    Interval _10687 = iratio(_7760, _7761, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7762 = _10687;
                    Interval _10688 = imul(_7759, _7762, intervalFailed, optical_product_upper);
                    Interval _7763 = _10688;
                    Interval _7598 = _7758;
                    Interval _7599 = _7763;
                    float _7595 = as_type<float>(as_type<uint>(_7599.hi) ^ 2147483648u);
                    float _7596 = as_type<float>(as_type<uint>(_7599.lo) ^ 2147483648u);
                    _7593.lo = _7595;
                    _7593.hi = _7596;
                    Interval _7594 = _7593;
                    Interval _7597 = _7594;
                    Interval _7600 = _7597;
                    Interval _10708 = iadd(_7598, _7600, intervalFailed);
                    Interval _7601 = _10708;
                    Interval _7764 = _7601;
                    Interval _7765 = _8446;
                    float _7766 = 32.0;
                    float _7767 = 100.0;
                    Interval _10711 = iratio(_7766, _7767, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7768 = _10711;
                    Interval _10712 = imul(_7765, _7768, intervalFailed, optical_product_upper);
                    Interval _7769 = _10712;
                    Interval _7589 = _7764;
                    Interval _7590 = _7769;
                    float _7586 = as_type<float>(as_type<uint>(_7590.hi) ^ 2147483648u);
                    float _7587 = as_type<float>(as_type<uint>(_7590.lo) ^ 2147483648u);
                    _7584.lo = _7586;
                    _7584.hi = _7587;
                    Interval _7585 = _7584;
                    Interval _7588 = _7585;
                    Interval _7591 = _7588;
                    Interval _10732 = iadd(_7589, _7591, intervalFailed);
                    Interval _7592 = _10732;
                    Interval _7753 = _7592;
                    Interval _7771 = _8447;
                    float _7772 = 22.0;
                    float _7773 = 1000.0;
                    Interval _10735 = iratio(_7772, _7773, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7774 = _10735;
                    Interval _7573 = _7771;
                    Interval _7574 = _7774;
                    Interval _10738 = imul(_7573, _7574, intervalFailed, optical_product_upper);
                    Interval _7575 = _10738;
                    bool _7565 = false;
                    if (_7575.lo <= 0.0)
                    {
                        _7565 = _7575.hi >= 0.0;
                    }
                    if (_7565)
                    {
                        _7564 = 0.0;
                    }
                    else
                    {
                        _7564 = precise::min(abs(_7575.lo), abs(_7575.hi));
                    }
                    float _7563 = _7564;
                    float _7566 = precise::max(abs(_7575.lo), abs(_7575.hi));
                    float _7567 = spvFMul(_7563, _7563);
                    float _10768 = interval_down(_7567, intervalFailed);
                    float _7568 = precise::max(0.0, _10768);
                    float _7569 = spvFMul(_7566, _7566);
                    float _10772 = interval_up(_7569, intervalFailed);
                    float _7570 = _10772;
                    _7561.lo = _7568;
                    _7561.hi = _7570;
                    Interval _7562 = _7561;
                    Interval _7571 = _7562;
                    Interval _7572 = _7571;
                    float _7576 = 1.0;
                    float _7558 = _7576;
                    float _7559 = _7576;
                    _7556.lo = _7558;
                    _7556.hi = _7559;
                    Interval _7557 = _7556;
                    Interval _7560 = _7557;
                    Interval _7577 = _7560;
                    float _7578 = 1.0;
                    float _7553 = _7578;
                    float _7554 = _7578;
                    _7551.lo = _7553;
                    _7551.hi = _7554;
                    Interval _7552 = _7551;
                    Interval _7555 = _7552;
                    Interval _7579 = _7555;
                    Interval _7580 = _7572;
                    bool _7544 = false;
                    if (_7580.lo <= 0.0)
                    {
                        _7544 = _7580.hi >= 0.0;
                    }
                    if (_7544)
                    {
                        _7543 = 0.0;
                    }
                    else
                    {
                        _7543 = precise::min(abs(_7580.lo), abs(_7580.hi));
                    }
                    float _7542 = _7543;
                    float _7545 = precise::max(abs(_7580.lo), abs(_7580.hi));
                    float _7546 = spvFMul(_7542, _7542);
                    float _10828 = interval_down(_7546, intervalFailed);
                    float _7547 = precise::max(0.0, _10828);
                    float _7548 = spvFMul(_7545, _7545);
                    float _10832 = interval_up(_7548, intervalFailed);
                    float _7549 = _10832;
                    _7540.lo = _7547;
                    _7540.hi = _7549;
                    Interval _7541 = _7540;
                    Interval _7550 = _7541;
                    Interval _7581 = _7550;
                    Interval _10840 = iadd(_7579, _7581, intervalFailed);
                    Interval _7582 = _10840;
                    Interval _10841 = idiv(_7577, _7582, intervalFailed, interval_divide_upper);
                    Interval _7583 = _10841;
                    Interval _7770 = _7583;
                    Interval _7776 = _8447;
                    float _7777 = 54.0;
                    float _7778 = 1000.0;
                    Interval _10844 = iratio(_7777, _7778, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7779 = _10844;
                    Interval _7529 = _7776;
                    Interval _7530 = _7779;
                    Interval _10847 = imul(_7529, _7530, intervalFailed, optical_product_upper);
                    Interval _7531 = _10847;
                    bool _7521 = false;
                    if (_7531.lo <= 0.0)
                    {
                        _7521 = _7531.hi >= 0.0;
                    }
                    if (_7521)
                    {
                        _7520 = 0.0;
                    }
                    else
                    {
                        _7520 = precise::min(abs(_7531.lo), abs(_7531.hi));
                    }
                    float _7519 = _7520;
                    float _7522 = precise::max(abs(_7531.lo), abs(_7531.hi));
                    float _7523 = spvFMul(_7519, _7519);
                    float _10877 = interval_down(_7523, intervalFailed);
                    float _7524 = precise::max(0.0, _10877);
                    float _7525 = spvFMul(_7522, _7522);
                    float _10881 = interval_up(_7525, intervalFailed);
                    float _7526 = _10881;
                    _7517.lo = _7524;
                    _7517.hi = _7526;
                    Interval _7518 = _7517;
                    Interval _7527 = _7518;
                    Interval _7528 = _7527;
                    float _7532 = 1.0;
                    float _7514 = _7532;
                    float _7515 = _7532;
                    _7512.lo = _7514;
                    _7512.hi = _7515;
                    Interval _7513 = _7512;
                    Interval _7516 = _7513;
                    Interval _7533 = _7516;
                    float _7534 = 1.0;
                    float _7509 = _7534;
                    float _7510 = _7534;
                    _7507.lo = _7509;
                    _7507.hi = _7510;
                    Interval _7508 = _7507;
                    Interval _7511 = _7508;
                    Interval _7535 = _7511;
                    Interval _7536 = _7528;
                    bool _7500 = false;
                    if (_7536.lo <= 0.0)
                    {
                        _7500 = _7536.hi >= 0.0;
                    }
                    if (_7500)
                    {
                        _7499 = 0.0;
                    }
                    else
                    {
                        _7499 = precise::min(abs(_7536.lo), abs(_7536.hi));
                    }
                    float _7498 = _7499;
                    float _7501 = precise::max(abs(_7536.lo), abs(_7536.hi));
                    float _7502 = spvFMul(_7498, _7498);
                    float _10937 = interval_down(_7502, intervalFailed);
                    float _7503 = precise::max(0.0, _10937);
                    float _7504 = spvFMul(_7501, _7501);
                    float _10941 = interval_up(_7504, intervalFailed);
                    float _7505 = _10941;
                    _7496.lo = _7503;
                    _7496.hi = _7505;
                    Interval _7497 = _7496;
                    Interval _7506 = _7497;
                    Interval _7537 = _7506;
                    Interval _10949 = iadd(_7535, _7537, intervalFailed);
                    Interval _7538 = _10949;
                    Interval _10950 = idiv(_7533, _7538, intervalFailed, interval_divide_upper);
                    Interval _7539 = _10950;
                    Interval _7775 = _7539;
                    Interval _7781 = _8447;
                    float _7782 = 24.0;
                    float _7783 = 1000.0;
                    Interval _10953 = iratio(_7782, _7783, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7784 = _10953;
                    Interval _7485 = _7781;
                    Interval _7486 = _7784;
                    Interval _10956 = imul(_7485, _7486, intervalFailed, optical_product_upper);
                    Interval _7487 = _10956;
                    bool _7477 = false;
                    if (_7487.lo <= 0.0)
                    {
                        _7477 = _7487.hi >= 0.0;
                    }
                    if (_7477)
                    {
                        _7476 = 0.0;
                    }
                    else
                    {
                        _7476 = precise::min(abs(_7487.lo), abs(_7487.hi));
                    }
                    float _7475 = _7476;
                    float _7478 = precise::max(abs(_7487.lo), abs(_7487.hi));
                    float _7479 = spvFMul(_7475, _7475);
                    float _10986 = interval_down(_7479, intervalFailed);
                    float _7480 = precise::max(0.0, _10986);
                    float _7481 = spvFMul(_7478, _7478);
                    float _10990 = interval_up(_7481, intervalFailed);
                    float _7482 = _10990;
                    _7473.lo = _7480;
                    _7473.hi = _7482;
                    Interval _7474 = _7473;
                    Interval _7483 = _7474;
                    Interval _7484 = _7483;
                    float _7488 = 1.0;
                    float _7470 = _7488;
                    float _7471 = _7488;
                    _7468.lo = _7470;
                    _7468.hi = _7471;
                    Interval _7469 = _7468;
                    Interval _7472 = _7469;
                    Interval _7489 = _7472;
                    float _7490 = 1.0;
                    float _7465 = _7490;
                    float _7466 = _7490;
                    _7463.lo = _7465;
                    _7463.hi = _7466;
                    Interval _7464 = _7463;
                    Interval _7467 = _7464;
                    Interval _7491 = _7467;
                    Interval _7492 = _7484;
                    bool _7456 = false;
                    if (_7492.lo <= 0.0)
                    {
                        _7456 = _7492.hi >= 0.0;
                    }
                    if (_7456)
                    {
                        _7455 = 0.0;
                    }
                    else
                    {
                        _7455 = precise::min(abs(_7492.lo), abs(_7492.hi));
                    }
                    float _7454 = _7455;
                    float _7457 = precise::max(abs(_7492.lo), abs(_7492.hi));
                    float _7458 = spvFMul(_7454, _7454);
                    float _11046 = interval_down(_7458, intervalFailed);
                    float _7459 = precise::max(0.0, _11046);
                    float _7460 = spvFMul(_7457, _7457);
                    float _11050 = interval_up(_7460, intervalFailed);
                    float _7461 = _11050;
                    _7452.lo = _7459;
                    _7452.hi = _7461;
                    Interval _7453 = _7452;
                    Interval _7462 = _7453;
                    Interval _7493 = _7462;
                    Interval _11058 = iadd(_7491, _7493, intervalFailed);
                    Interval _7494 = _11058;
                    Interval _11059 = idiv(_7489, _7494, intervalFailed, interval_divide_upper);
                    Interval _7495 = _11059;
                    Interval _7780 = _7495;
                    float _7786 = 9.0;
                    float _7449 = _7786;
                    float _7450 = _7786;
                    _7447.lo = _7449;
                    _7447.hi = _7450;
                    Interval _7448 = _7447;
                    Interval _7451 = _7448;
                    Interval _7787 = _7451;
                    Interval _7788 = _7710;
                    float _7442 = _7788.lo;
                    float _7443 = _7788.hi;
                    float _7437 = _7442;
                    float _7438 = _7443;
                    _7434.lo = _7437;
                    _7434.hi = _7438;
                    Interval _7435 = _7434;
                    Interval _7439 = _7435;
                    Interval _11083 = isin_body(_7439, intervalFailed, optical_product_upper);
                    Interval _7436 = _11083;
                    interval_sine_upper = _7436.hi;
                    float _7440 = _7436.lo;
                    float _7441 = _7440;
                    float _7444 = _7441;
                    float _7445 = interval_sine_upper;
                    _7432.lo = _7444;
                    _7432.hi = _7445;
                    Interval _7433 = _7432;
                    Interval _7446 = _7433;
                    Interval _7789 = _7446;
                    Interval _11098 = imul(_7787, _7789, intervalFailed, optical_product_upper);
                    Interval _7790 = _11098;
                    Interval _7791 = _7770;
                    Interval _11100 = imul(_7790, _7791, intervalFailed, optical_product_upper);
                    Interval _7792 = _11100;
                    float _7793 = 2.5;
                    float _7429 = _7793;
                    float _7430 = _7793;
                    _7427.lo = _7429;
                    _7427.hi = _7430;
                    Interval _7428 = _7427;
                    Interval _7431 = _7428;
                    Interval _7794 = _7431;
                    Interval _7795 = _7736;
                    float _7422 = _7795.lo;
                    float _7423 = _7795.hi;
                    float _7417 = _7422;
                    float _7418 = _7423;
                    _7414.lo = _7417;
                    _7414.hi = _7418;
                    Interval _7415 = _7414;
                    Interval _7419 = _7415;
                    Interval _11123 = isin_body(_7419, intervalFailed, optical_product_upper);
                    Interval _7416 = _11123;
                    interval_sine_upper = _7416.hi;
                    float _7420 = _7416.lo;
                    float _7421 = _7420;
                    float _7424 = _7421;
                    float _7425 = interval_sine_upper;
                    _7412.lo = _7424;
                    _7412.hi = _7425;
                    Interval _7413 = _7412;
                    Interval _7426 = _7413;
                    Interval _7796 = _7426;
                    Interval _11138 = imul(_7794, _7796, intervalFailed, optical_product_upper);
                    Interval _7797 = _11138;
                    Interval _7798 = _7775;
                    Interval _11140 = imul(_7797, _7798, intervalFailed, optical_product_upper);
                    Interval _7799 = _11140;
                    Interval _11141 = iadd(_7792, _7799, intervalFailed);
                    Interval _7800 = _11141;
                    float _7801 = 5.0;
                    float _7409 = _7801;
                    float _7410 = _7801;
                    _7407.lo = _7409;
                    _7407.hi = _7410;
                    Interval _7408 = _7407;
                    Interval _7411 = _7408;
                    Interval _7802 = _7411;
                    Interval _7803 = _7753;
                    float _7402 = _7803.lo;
                    float _7403 = _7803.hi;
                    float _7397 = _7402;
                    float _7398 = _7403;
                    _7394.lo = _7397;
                    _7394.hi = _7398;
                    Interval _7395 = _7394;
                    Interval _7399 = _7395;
                    Interval _11164 = isin_body(_7399, intervalFailed, optical_product_upper);
                    Interval _7396 = _11164;
                    interval_sine_upper = _7396.hi;
                    float _7400 = _7396.lo;
                    float _7401 = _7400;
                    float _7404 = _7401;
                    float _7405 = interval_sine_upper;
                    _7392.lo = _7404;
                    _7392.hi = _7405;
                    Interval _7393 = _7392;
                    Interval _7406 = _7393;
                    Interval _7804 = _7406;
                    Interval _11179 = imul(_7802, _7804, intervalFailed, optical_product_upper);
                    Interval _7805 = _11179;
                    Interval _7806 = _7780;
                    Interval _11181 = imul(_7805, _7806, intervalFailed, optical_product_upper);
                    Interval _7807 = _11181;
                    Interval _11182 = iadd(_7800, _7807, intervalFailed);
                    Interval _7785 = _11182;
                    Interval _7809 = _7688;
                    Interval _7385 = _7809;
                    float _7383 = 3.1415927410125732421875;
                    float _7378 = _7383;
                    float _11186 = interval_down(_7378, intervalFailed);
                    float _7379 = _11186;
                    float _7380 = _7383;
                    float _11188 = interval_up(_7380, intervalFailed);
                    float _7381 = _11188;
                    _7376.lo = _7379;
                    _7376.hi = _7381;
                    Interval _7377 = _7376;
                    Interval _7382 = _7377;
                    Interval _7384 = _7382;
                    Interval _7386 = _7384;
                    float _7387 = 0.5;
                    float _7373 = _7387;
                    float _7374 = _7387;
                    _7371.lo = _7373;
                    _7371.hi = _7374;
                    Interval _7372 = _7371;
                    Interval _7375 = _7372;
                    Interval _7388 = _7375;
                    Interval _11206 = imul(_7386, _7388, intervalFailed, optical_product_upper);
                    Interval _7389 = _11206;
                    Interval _11207 = iadd(_7385, _7389, intervalFailed);
                    Interval _7390 = _11207;
                    float _7366 = _7390.lo;
                    float _7367 = _7390.hi;
                    float _7361 = _7366;
                    float _7362 = _7367;
                    _7358.lo = _7361;
                    _7358.hi = _7362;
                    Interval _7359 = _7358;
                    Interval _7363 = _7359;
                    Interval _11220 = isin_body(_7363, intervalFailed, optical_product_upper);
                    Interval _7360 = _11220;
                    interval_sine_upper = _7360.hi;
                    float _7364 = _7360.lo;
                    float _7365 = _7364;
                    float _7368 = _7365;
                    float _7369 = interval_sine_upper;
                    _7356.lo = _7368;
                    _7356.hi = _7369;
                    Interval _7357 = _7356;
                    Interval _7370 = _7357;
                    Interval _7391 = _7370;
                    Interval _7810 = _7391;
                    Interval _7811 = _7705;
                    Interval _11237 = imul(_7810, _7811, intervalFailed, optical_product_upper);
                    Interval _7808 = _11237;
                    float _7813 = 9.0;
                    float _7353 = _7813;
                    float _7354 = _7813;
                    _7351.lo = _7353;
                    _7351.hi = _7354;
                    Interval _7352 = _7351;
                    Interval _7355 = _7352;
                    Interval _7814 = _7355;
                    float _7815 = 18.0;
                    float _7816 = 1000.0;
                    Interval _11247 = iratio(_7815, _7816, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7817 = _11247;
                    float _7818 = 39.0;
                    float _7819 = 10000.0;
                    Interval _11248 = iratio(_7818, _7819, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7820 = _11248;
                    Interval _7821 = _7808;
                    Interval _11250 = imul(_7820, _7821, intervalFailed, optical_product_upper);
                    Interval _7822 = _11250;
                    Interval _11251 = iadd(_7817, _7822, intervalFailed);
                    Interval _7823 = _11251;
                    Interval _11252 = imul(_7814, _7823, intervalFailed, optical_product_upper);
                    Interval _7824 = _11252;
                    Interval _7825 = _7710;
                    Interval _7344 = _7825;
                    float _7342 = 3.1415927410125732421875;
                    float _7337 = _7342;
                    float _11256 = interval_down(_7337, intervalFailed);
                    float _7338 = _11256;
                    float _7339 = _7342;
                    float _11258 = interval_up(_7339, intervalFailed);
                    float _7340 = _11258;
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
                    Interval _11276 = imul(_7345, _7347, intervalFailed, optical_product_upper);
                    Interval _7348 = _11276;
                    Interval _11277 = iadd(_7344, _7348, intervalFailed);
                    Interval _7349 = _11277;
                    float _7325 = _7349.lo;
                    float _7326 = _7349.hi;
                    float _7320 = _7325;
                    float _7321 = _7326;
                    _7317.lo = _7320;
                    _7317.hi = _7321;
                    Interval _7318 = _7317;
                    Interval _7322 = _7318;
                    Interval _11290 = isin_body(_7322, intervalFailed, optical_product_upper);
                    Interval _7319 = _11290;
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
                    Interval _7826 = _7350;
                    Interval _11306 = imul(_7824, _7826, intervalFailed, optical_product_upper);
                    Interval _7827 = _11306;
                    Interval _7828 = _7770;
                    Interval _11308 = imul(_7827, _7828, intervalFailed, optical_product_upper);
                    Interval _7829 = _11308;
                    float _7830 = 1175.0;
                    float _7831 = 10000.0;
                    Interval _11309 = iratio(_7830, _7831, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7832 = _11309;
                    Interval _7833 = _7736;
                    Interval _7308 = _7833;
                    float _7306 = 3.1415927410125732421875;
                    float _7301 = _7306;
                    float _11313 = interval_down(_7301, intervalFailed);
                    float _7302 = _11313;
                    float _7303 = _7306;
                    float _11315 = interval_up(_7303, intervalFailed);
                    float _7304 = _11315;
                    _7299.lo = _7302;
                    _7299.hi = _7304;
                    Interval _7300 = _7299;
                    Interval _7305 = _7300;
                    Interval _7307 = _7305;
                    Interval _7309 = _7307;
                    float _7310 = 0.5;
                    float _7296 = _7310;
                    float _7297 = _7310;
                    _7294.lo = _7296;
                    _7294.hi = _7297;
                    Interval _7295 = _7294;
                    Interval _7298 = _7295;
                    Interval _7311 = _7298;
                    Interval _11333 = imul(_7309, _7311, intervalFailed, optical_product_upper);
                    Interval _7312 = _11333;
                    Interval _11334 = iadd(_7308, _7312, intervalFailed);
                    Interval _7313 = _11334;
                    float _7289 = _7313.lo;
                    float _7290 = _7313.hi;
                    float _7284 = _7289;
                    float _7285 = _7290;
                    _7281.lo = _7284;
                    _7281.hi = _7285;
                    Interval _7282 = _7281;
                    Interval _7286 = _7282;
                    Interval _11347 = isin_body(_7286, intervalFailed, optical_product_upper);
                    Interval _7283 = _11347;
                    interval_sine_upper = _7283.hi;
                    float _7287 = _7283.lo;
                    float _7288 = _7287;
                    float _7291 = _7288;
                    float _7292 = interval_sine_upper;
                    _7279.lo = _7291;
                    _7279.hi = _7292;
                    Interval _7280 = _7279;
                    Interval _7293 = _7280;
                    Interval _7314 = _7293;
                    Interval _7834 = _7314;
                    Interval _11363 = imul(_7832, _7834, intervalFailed, optical_product_upper);
                    Interval _7835 = _11363;
                    Interval _7836 = _7775;
                    Interval _11365 = imul(_7835, _7836, intervalFailed, optical_product_upper);
                    Interval _7837 = _11365;
                    Interval _11366 = iadd(_7829, _7837, intervalFailed);
                    Interval _7838 = _11366;
                    float _7839 = 45.0;
                    float _7840 = 1000.0;
                    Interval _11367 = iratio(_7839, _7840, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7841 = _11367;
                    Interval _7842 = _7753;
                    Interval _7272 = _7842;
                    float _7270 = 3.1415927410125732421875;
                    float _7265 = _7270;
                    float _11371 = interval_down(_7265, intervalFailed);
                    float _7266 = _11371;
                    float _7267 = _7270;
                    float _11373 = interval_up(_7267, intervalFailed);
                    float _7268 = _11373;
                    _7263.lo = _7266;
                    _7263.hi = _7268;
                    Interval _7264 = _7263;
                    Interval _7269 = _7264;
                    Interval _7271 = _7269;
                    Interval _7273 = _7271;
                    float _7274 = 0.5;
                    float _7260 = _7274;
                    float _7261 = _7274;
                    _7258.lo = _7260;
                    _7258.hi = _7261;
                    Interval _7259 = _7258;
                    Interval _7262 = _7259;
                    Interval _7275 = _7262;
                    Interval _11391 = imul(_7273, _7275, intervalFailed, optical_product_upper);
                    Interval _7276 = _11391;
                    Interval _11392 = iadd(_7272, _7276, intervalFailed);
                    Interval _7277 = _11392;
                    float _7253 = _7277.lo;
                    float _7254 = _7277.hi;
                    float _7248 = _7253;
                    float _7249 = _7254;
                    _7245.lo = _7248;
                    _7245.hi = _7249;
                    Interval _7246 = _7245;
                    Interval _7250 = _7246;
                    Interval _11405 = isin_body(_7250, intervalFailed, optical_product_upper);
                    Interval _7247 = _11405;
                    interval_sine_upper = _7247.hi;
                    float _7251 = _7247.lo;
                    float _7252 = _7251;
                    float _7255 = _7252;
                    float _7256 = interval_sine_upper;
                    _7243.lo = _7255;
                    _7243.hi = _7256;
                    Interval _7244 = _7243;
                    Interval _7257 = _7244;
                    Interval _7278 = _7257;
                    Interval _7843 = _7278;
                    Interval _11421 = imul(_7841, _7843, intervalFailed, optical_product_upper);
                    Interval _7844 = _11421;
                    Interval _7845 = _7780;
                    Interval _11423 = imul(_7844, _7845, intervalFailed, optical_product_upper);
                    Interval _7846 = _11423;
                    Interval _7239 = _7838;
                    Interval _7240 = _7846;
                    float _7236 = as_type<float>(as_type<uint>(_7240.hi) ^ 2147483648u);
                    float _7237 = as_type<float>(as_type<uint>(_7240.lo) ^ 2147483648u);
                    _7234.lo = _7236;
                    _7234.hi = _7237;
                    Interval _7235 = _7234;
                    Interval _7238 = _7235;
                    Interval _7241 = _7238;
                    Interval _11443 = iadd(_7239, _7241, intervalFailed);
                    Interval _7242 = _11443;
                    Interval _7812 = _7242;
                    float _7848 = 9.0;
                    float _7231 = _7848;
                    float _7232 = _7848;
                    _7229.lo = _7231;
                    _7229.hi = _7232;
                    Interval _7230 = _7229;
                    Interval _7233 = _7230;
                    Interval _7849 = _7233;
                    float _7850 = 11.0;
                    float _7851 = 1000.0;
                    Interval _11454 = iratio(_7850, _7851, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7852 = _11454;
                    float _7853 = 52.0;
                    float _7854 = 10000.0;
                    Interval _11455 = iratio(_7853, _7854, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7855 = _11455;
                    Interval _7856 = _7808;
                    Interval _11457 = imul(_7855, _7856, intervalFailed, optical_product_upper);
                    Interval _7857 = _11457;
                    Interval _7225 = _7852;
                    Interval _7226 = _7857;
                    float _7222 = as_type<float>(as_type<uint>(_7226.hi) ^ 2147483648u);
                    float _7223 = as_type<float>(as_type<uint>(_7226.lo) ^ 2147483648u);
                    _7220.lo = _7222;
                    _7220.hi = _7223;
                    Interval _7221 = _7220;
                    Interval _7224 = _7221;
                    Interval _7227 = _7224;
                    Interval _11477 = iadd(_7225, _7227, intervalFailed);
                    Interval _7228 = _11477;
                    Interval _7858 = _7228;
                    Interval _11479 = imul(_7849, _7858, intervalFailed, optical_product_upper);
                    Interval _7859 = _11479;
                    Interval _7860 = _7710;
                    Interval _7213 = _7860;
                    float _7211 = 3.1415927410125732421875;
                    float _7206 = _7211;
                    float _11483 = interval_down(_7206, intervalFailed);
                    float _7207 = _11483;
                    float _7208 = _7211;
                    float _11485 = interval_up(_7208, intervalFailed);
                    float _7209 = _11485;
                    _7204.lo = _7207;
                    _7204.hi = _7209;
                    Interval _7205 = _7204;
                    Interval _7210 = _7205;
                    Interval _7212 = _7210;
                    Interval _7214 = _7212;
                    float _7215 = 0.5;
                    float _7201 = _7215;
                    float _7202 = _7215;
                    _7199.lo = _7201;
                    _7199.hi = _7202;
                    Interval _7200 = _7199;
                    Interval _7203 = _7200;
                    Interval _7216 = _7203;
                    Interval _11503 = imul(_7214, _7216, intervalFailed, optical_product_upper);
                    Interval _7217 = _11503;
                    Interval _11504 = iadd(_7213, _7217, intervalFailed);
                    Interval _7218 = _11504;
                    float _7194 = _7218.lo;
                    float _7195 = _7218.hi;
                    float _7189 = _7194;
                    float _7190 = _7195;
                    _7186.lo = _7189;
                    _7186.hi = _7190;
                    Interval _7187 = _7186;
                    Interval _7191 = _7187;
                    Interval _11517 = isin_body(_7191, intervalFailed, optical_product_upper);
                    Interval _7188 = _11517;
                    interval_sine_upper = _7188.hi;
                    float _7192 = _7188.lo;
                    float _7193 = _7192;
                    float _7196 = _7193;
                    float _7197 = interval_sine_upper;
                    _7184.lo = _7196;
                    _7184.hi = _7197;
                    Interval _7185 = _7184;
                    Interval _7198 = _7185;
                    Interval _7219 = _7198;
                    Interval _7861 = _7219;
                    Interval _11533 = imul(_7859, _7861, intervalFailed, optical_product_upper);
                    Interval _7862 = _11533;
                    Interval _7863 = _7770;
                    Interval _11535 = imul(_7862, _7863, intervalFailed, optical_product_upper);
                    Interval _7864 = _11535;
                    float _7865 = 625.0;
                    float _7866 = 10000.0;
                    Interval _11536 = iratio(_7865, _7866, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7867 = _11536;
                    Interval _7868 = _7736;
                    Interval _7177 = _7868;
                    float _7175 = 3.1415927410125732421875;
                    float _7170 = _7175;
                    float _11540 = interval_down(_7170, intervalFailed);
                    float _7171 = _11540;
                    float _7172 = _7175;
                    float _11542 = interval_up(_7172, intervalFailed);
                    float _7173 = _11542;
                    _7168.lo = _7171;
                    _7168.hi = _7173;
                    Interval _7169 = _7168;
                    Interval _7174 = _7169;
                    Interval _7176 = _7174;
                    Interval _7178 = _7176;
                    float _7179 = 0.5;
                    float _7165 = _7179;
                    float _7166 = _7179;
                    _7163.lo = _7165;
                    _7163.hi = _7166;
                    Interval _7164 = _7163;
                    Interval _7167 = _7164;
                    Interval _7180 = _7167;
                    Interval _11560 = imul(_7178, _7180, intervalFailed, optical_product_upper);
                    Interval _7181 = _11560;
                    Interval _11561 = iadd(_7177, _7181, intervalFailed);
                    Interval _7182 = _11561;
                    float _7158 = _7182.lo;
                    float _7159 = _7182.hi;
                    float _7153 = _7158;
                    float _7154 = _7159;
                    _7150.lo = _7153;
                    _7150.hi = _7154;
                    Interval _7151 = _7150;
                    Interval _7155 = _7151;
                    Interval _11574 = isin_body(_7155, intervalFailed, optical_product_upper);
                    Interval _7152 = _11574;
                    interval_sine_upper = _7152.hi;
                    float _7156 = _7152.lo;
                    float _7157 = _7156;
                    float _7160 = _7157;
                    float _7161 = interval_sine_upper;
                    _7148.lo = _7160;
                    _7148.hi = _7161;
                    Interval _7149 = _7148;
                    Interval _7162 = _7149;
                    Interval _7183 = _7162;
                    Interval _7869 = _7183;
                    Interval _11590 = imul(_7867, _7869, intervalFailed, optical_product_upper);
                    Interval _7870 = _11590;
                    Interval _7871 = _7775;
                    Interval _11592 = imul(_7870, _7871, intervalFailed, optical_product_upper);
                    Interval _7872 = _11592;
                    Interval _7144 = _7864;
                    Interval _7145 = _7872;
                    float _7141 = as_type<float>(as_type<uint>(_7145.hi) ^ 2147483648u);
                    float _7142 = as_type<float>(as_type<uint>(_7145.lo) ^ 2147483648u);
                    _7139.lo = _7141;
                    _7139.hi = _7142;
                    Interval _7140 = _7139;
                    Interval _7143 = _7140;
                    Interval _7146 = _7143;
                    Interval _11612 = iadd(_7144, _7146, intervalFailed);
                    Interval _7147 = _11612;
                    Interval _7873 = _7147;
                    float _7874 = 110.0;
                    float _7875 = 1000.0;
                    Interval _11614 = iratio(_7874, _7875, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7876 = _11614;
                    Interval _7877 = _7753;
                    Interval _7132 = _7877;
                    float _7130 = 3.1415927410125732421875;
                    float _7125 = _7130;
                    float _11618 = interval_down(_7125, intervalFailed);
                    float _7126 = _11618;
                    float _7127 = _7130;
                    float _11620 = interval_up(_7127, intervalFailed);
                    float _7128 = _11620;
                    _7123.lo = _7126;
                    _7123.hi = _7128;
                    Interval _7124 = _7123;
                    Interval _7129 = _7124;
                    Interval _7131 = _7129;
                    Interval _7133 = _7131;
                    float _7134 = 0.5;
                    float _7120 = _7134;
                    float _7121 = _7134;
                    _7118.lo = _7120;
                    _7118.hi = _7121;
                    Interval _7119 = _7118;
                    Interval _7122 = _7119;
                    Interval _7135 = _7122;
                    Interval _11638 = imul(_7133, _7135, intervalFailed, optical_product_upper);
                    Interval _7136 = _11638;
                    Interval _11639 = iadd(_7132, _7136, intervalFailed);
                    Interval _7137 = _11639;
                    float _7113 = _7137.lo;
                    float _7114 = _7137.hi;
                    float _7108 = _7113;
                    float _7109 = _7114;
                    _7105.lo = _7108;
                    _7105.hi = _7109;
                    Interval _7106 = _7105;
                    Interval _7110 = _7106;
                    Interval _11652 = isin_body(_7110, intervalFailed, optical_product_upper);
                    Interval _7107 = _11652;
                    interval_sine_upper = _7107.hi;
                    float _7111 = _7107.lo;
                    float _7112 = _7111;
                    float _7115 = _7112;
                    float _7116 = interval_sine_upper;
                    _7103.lo = _7115;
                    _7103.hi = _7116;
                    Interval _7104 = _7103;
                    Interval _7117 = _7104;
                    Interval _7138 = _7117;
                    Interval _7878 = _7138;
                    Interval _11668 = imul(_7876, _7878, intervalFailed, optical_product_upper);
                    Interval _7879 = _11668;
                    Interval _7880 = _7780;
                    Interval _11670 = imul(_7879, _7880, intervalFailed, optical_product_upper);
                    Interval _7881 = _11670;
                    Interval _11671 = iadd(_7873, _7881, intervalFailed);
                    Interval _7847 = _11671;
                    Interval _7883 = _8444;
                    float _7884 = 173.0;
                    float _7885 = 1000.0;
                    Interval _11673 = iratio(_7884, _7885, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7886 = _11673;
                    Interval _11674 = imul(_7883, _7886, intervalFailed, optical_product_upper);
                    Interval _7887 = _11674;
                    Interval _7888 = _8445;
                    float _7889 = 129.0;
                    float _7890 = 1000.0;
                    Interval _11676 = iratio(_7889, _7890, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7891 = _11676;
                    Interval _11677 = imul(_7888, _7891, intervalFailed, optical_product_upper);
                    Interval _7892 = _11677;
                    Interval _11678 = iadd(_7887, _7892, intervalFailed);
                    Interval _7893 = _11678;
                    Interval _7894 = _8446;
                    float _7895 = 73.0;
                    float _7896 = 100.0;
                    Interval _11680 = iratio(_7895, _7896, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7897 = _11680;
                    Interval _11681 = imul(_7894, _7897, intervalFailed, optical_product_upper);
                    Interval _7898 = _11681;
                    Interval _7099 = _7893;
                    Interval _7100 = _7898;
                    float _7096 = as_type<float>(as_type<uint>(_7100.hi) ^ 2147483648u);
                    float _7097 = as_type<float>(as_type<uint>(_7100.lo) ^ 2147483648u);
                    _7094.lo = _7096;
                    _7094.hi = _7097;
                    Interval _7095 = _7094;
                    Interval _7098 = _7095;
                    Interval _7101 = _7098;
                    Interval _11701 = iadd(_7099, _7101, intervalFailed);
                    Interval _7102 = _11701;
                    Interval _7882 = _7102;
                    Interval _7900 = _8447;
                    float _7901 = 216.0;
                    float _7902 = 1000.0;
                    Interval _11704 = iratio(_7901, _7902, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7903 = _11704;
                    Interval _7083 = _7900;
                    Interval _7084 = _7903;
                    Interval _11707 = imul(_7083, _7084, intervalFailed, optical_product_upper);
                    Interval _7085 = _11707;
                    bool _7075 = false;
                    if (_7085.lo <= 0.0)
                    {
                        _7075 = _7085.hi >= 0.0;
                    }
                    if (_7075)
                    {
                        _7074 = 0.0;
                    }
                    else
                    {
                        _7074 = precise::min(abs(_7085.lo), abs(_7085.hi));
                    }
                    float _7073 = _7074;
                    float _7076 = precise::max(abs(_7085.lo), abs(_7085.hi));
                    float _7077 = spvFMul(_7073, _7073);
                    float _11737 = interval_down(_7077, intervalFailed);
                    float _7078 = precise::max(0.0, _11737);
                    float _7079 = spvFMul(_7076, _7076);
                    float _11741 = interval_up(_7079, intervalFailed);
                    float _7080 = _11741;
                    _7071.lo = _7078;
                    _7071.hi = _7080;
                    Interval _7072 = _7071;
                    Interval _7081 = _7072;
                    Interval _7082 = _7081;
                    float _7086 = 1.0;
                    float _7068 = _7086;
                    float _7069 = _7086;
                    _7066.lo = _7068;
                    _7066.hi = _7069;
                    Interval _7067 = _7066;
                    Interval _7070 = _7067;
                    Interval _7087 = _7070;
                    float _7088 = 1.0;
                    float _7063 = _7088;
                    float _7064 = _7088;
                    _7061.lo = _7063;
                    _7061.hi = _7064;
                    Interval _7062 = _7061;
                    Interval _7065 = _7062;
                    Interval _7089 = _7065;
                    Interval _7090 = _7082;
                    bool _7054 = false;
                    if (_7090.lo <= 0.0)
                    {
                        _7054 = _7090.hi >= 0.0;
                    }
                    if (_7054)
                    {
                        _7053 = 0.0;
                    }
                    else
                    {
                        _7053 = precise::min(abs(_7090.lo), abs(_7090.hi));
                    }
                    float _7052 = _7053;
                    float _7055 = precise::max(abs(_7090.lo), abs(_7090.hi));
                    float _7056 = spvFMul(_7052, _7052);
                    float _11797 = interval_down(_7056, intervalFailed);
                    float _7057 = precise::max(0.0, _11797);
                    float _7058 = spvFMul(_7055, _7055);
                    float _11801 = interval_up(_7058, intervalFailed);
                    float _7059 = _11801;
                    _7050.lo = _7057;
                    _7050.hi = _7059;
                    Interval _7051 = _7050;
                    Interval _7060 = _7051;
                    Interval _7091 = _7060;
                    Interval _11809 = iadd(_7089, _7091, intervalFailed);
                    Interval _7092 = _11809;
                    Interval _11810 = idiv(_7087, _7092, intervalFailed, interval_divide_upper);
                    Interval _7093 = _11810;
                    Interval _7899 = _7093;
                    Interval _7904 = _7785;
                    float _7905 = 38.0;
                    float _7906 = 100.0;
                    Interval _11813 = iratio(_7905, _7906, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7907 = _11813;
                    Interval _7908 = _7882;
                    float _7045 = _7908.lo;
                    float _7046 = _7908.hi;
                    float _7040 = _7045;
                    float _7041 = _7046;
                    _7037.lo = _7040;
                    _7037.hi = _7041;
                    Interval _7038 = _7037;
                    Interval _7042 = _7038;
                    Interval _11827 = isin_body(_7042, intervalFailed, optical_product_upper);
                    Interval _7039 = _11827;
                    interval_sine_upper = _7039.hi;
                    float _7043 = _7039.lo;
                    float _7044 = _7043;
                    float _7047 = _7044;
                    float _7048 = interval_sine_upper;
                    _7035.lo = _7047;
                    _7035.hi = _7048;
                    Interval _7036 = _7035;
                    Interval _7049 = _7036;
                    Interval _7909 = _7049;
                    Interval _11842 = imul(_7907, _7909, intervalFailed, optical_product_upper);
                    Interval _7910 = _11842;
                    Interval _7911 = _7899;
                    Interval _11844 = imul(_7910, _7911, intervalFailed, optical_product_upper);
                    Interval _7912 = _11844;
                    Interval _11845 = iadd(_7904, _7912, intervalFailed);
                    _7785 = _11845;
                    Interval _7913 = _7812;
                    float _7914 = 6574.0;
                    float _7915 = 100000.0;
                    Interval _11847 = iratio(_7914, _7915, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7916 = _11847;
                    Interval _7917 = _7882;
                    Interval _7028 = _7917;
                    float _7026 = 3.1415927410125732421875;
                    float _7021 = _7026;
                    float _11851 = interval_down(_7021, intervalFailed);
                    float _7022 = _11851;
                    float _7023 = _7026;
                    float _11853 = interval_up(_7023, intervalFailed);
                    float _7024 = _11853;
                    _7019.lo = _7022;
                    _7019.hi = _7024;
                    Interval _7020 = _7019;
                    Interval _7025 = _7020;
                    Interval _7027 = _7025;
                    Interval _7029 = _7027;
                    float _7030 = 0.5;
                    float _7016 = _7030;
                    float _7017 = _7030;
                    _7014.lo = _7016;
                    _7014.hi = _7017;
                    Interval _7015 = _7014;
                    Interval _7018 = _7015;
                    Interval _7031 = _7018;
                    Interval _11871 = imul(_7029, _7031, intervalFailed, optical_product_upper);
                    Interval _7032 = _11871;
                    Interval _11872 = iadd(_7028, _7032, intervalFailed);
                    Interval _7033 = _11872;
                    float _7009 = _7033.lo;
                    float _7010 = _7033.hi;
                    float _7004 = _7009;
                    float _7005 = _7010;
                    _7001.lo = _7004;
                    _7001.hi = _7005;
                    Interval _7002 = _7001;
                    Interval _7006 = _7002;
                    Interval _11885 = isin_body(_7006, intervalFailed, optical_product_upper);
                    Interval _7003 = _11885;
                    interval_sine_upper = _7003.hi;
                    float _7007 = _7003.lo;
                    float _7008 = _7007;
                    float _7011 = _7008;
                    float _7012 = interval_sine_upper;
                    _6999.lo = _7011;
                    _6999.hi = _7012;
                    Interval _7000 = _6999;
                    Interval _7013 = _7000;
                    Interval _7034 = _7013;
                    Interval _7918 = _7034;
                    Interval _11901 = imul(_7916, _7918, intervalFailed, optical_product_upper);
                    Interval _7919 = _11901;
                    Interval _7920 = _7899;
                    Interval _11903 = imul(_7919, _7920, intervalFailed, optical_product_upper);
                    Interval _7921 = _11903;
                    Interval _11904 = iadd(_7913, _7921, intervalFailed);
                    _7812 = _11904;
                    Interval _7922 = _7847;
                    float _7923 = 4902.0;
                    float _7924 = 100000.0;
                    Interval _11906 = iratio(_7923, _7924, intervalFailed, optical_product_upper, interval_divide_upper);
                    Interval _7925 = _11906;
                    Interval _7926 = _7882;
                    Interval _6992 = _7926;
                    float _6990 = 3.1415927410125732421875;
                    float _6985 = _6990;
                    float _11910 = interval_down(_6985, intervalFailed);
                    float _6986 = _11910;
                    float _6987 = _6990;
                    float _11912 = interval_up(_6987, intervalFailed);
                    float _6988 = _11912;
                    _6983.lo = _6986;
                    _6983.hi = _6988;
                    Interval _6984 = _6983;
                    Interval _6989 = _6984;
                    Interval _6991 = _6989;
                    Interval _6993 = _6991;
                    float _6994 = 0.5;
                    float _6980 = _6994;
                    float _6981 = _6994;
                    _6978.lo = _6980;
                    _6978.hi = _6981;
                    Interval _6979 = _6978;
                    Interval _6982 = _6979;
                    Interval _6995 = _6982;
                    Interval _11930 = imul(_6993, _6995, intervalFailed, optical_product_upper);
                    Interval _6996 = _11930;
                    Interval _11931 = iadd(_6992, _6996, intervalFailed);
                    Interval _6997 = _11931;
                    float _6973 = _6997.lo;
                    float _6974 = _6997.hi;
                    float _6968 = _6973;
                    float _6969 = _6974;
                    _6965.lo = _6968;
                    _6965.hi = _6969;
                    Interval _6966 = _6965;
                    Interval _6970 = _6966;
                    Interval _11944 = isin_body(_6970, intervalFailed, optical_product_upper);
                    Interval _6967 = _11944;
                    interval_sine_upper = _6967.hi;
                    float _6971 = _6967.lo;
                    float _6972 = _6971;
                    float _6975 = _6972;
                    float _6976 = interval_sine_upper;
                    _6963.lo = _6975;
                    _6963.hi = _6976;
                    Interval _6964 = _6963;
                    Interval _6977 = _6964;
                    Interval _6998 = _6977;
                    Interval _7927 = _6998;
                    Interval _11960 = imul(_7925, _7927, intervalFailed, optical_product_upper);
                    Interval _7928 = _11960;
                    Interval _7929 = _7899;
                    Interval _11962 = imul(_7928, _7929, intervalFailed, optical_product_upper);
                    Interval _7930 = _11962;
                    Interval _11963 = iadd(_7922, _7930, intervalFailed);
                    _7847 = _11963;
                    if (_8447.lo < 24.0)
                    {
                        Interval _7932 = _8444;
                        float _7933 = 128.0;
                        float _6960 = _7933;
                        float _6961 = _7933;
                        _6958.lo = _6960;
                        _6958.hi = _6961;
                        Interval _6959 = _6958;
                        Interval _6962 = _6959;
                        Interval _7934 = _6962;
                        Interval _11979 = idiv(_7932, _7934, intervalFailed, interval_divide_upper);
                        Interval _7931 = _11979;
                        Interval _7936 = _8445;
                        float _7937 = 128.0;
                        float _6955 = _7937;
                        float _6956 = _7937;
                        _6953.lo = _6955;
                        _6953.hi = _6956;
                        Interval _6954 = _6953;
                        Interval _6957 = _6954;
                        Interval _7938 = _6957;
                        Interval _11990 = idiv(_7936, _7938, intervalFailed, interval_divide_upper);
                        Interval _7935 = _11990;
                        float _11993 = floor(_7931.lo);
                        float _11996 = floor(_7931.hi);
                        bool _7939 = true;
                        if ((isunordered(_11993, _11996) || _11993 == _11996))
                        {
                            _7939 = floor(_7935.lo) != floor(_7935.hi);
                        }
                        if (_7939)
                        {
                            Interval _7940 = _7785;
                            float _7941 = 0.0;
                            float _7942 = 10.0;
                            _6951.lo = _7941;
                            _6951.hi = _7942;
                            Interval _6952 = _6951;
                            Interval _7943 = _6952;
                            Interval _12018 = iadd(_7940, _7943, intervalFailed);
                            _7785 = _12018;
                            Interval _7944 = _7812;
                            float _7945 = -256.0;
                            float _7946 = 256.0;
                            _6949.lo = _7945;
                            _6949.hi = _7946;
                            Interval _6950 = _6949;
                            Interval _7947 = _6950;
                            Interval _12026 = iadd(_7944, _7947, intervalFailed);
                            _7812 = _12026;
                            Interval _7948 = _7847;
                            float _7949 = -256.0;
                            float _7950 = 256.0;
                            _6947.lo = _7949;
                            _6947.hi = _7950;
                            Interval _6948 = _6947;
                            Interval _7951 = _6948;
                            Interval _12034 = iadd(_7948, _7951, intervalFailed);
                            _7847 = _12034;
                        }
                        else
                        {
                            int _7952 = int(floor(_7931.lo));
                            int _7953 = int(floor(_7935.lo));
                            int _7955 = _7952;
                            int _7956 = _7953;
                            uint _6943 = (uint(_7955) * 1597334677u) ^ (uint(_7956) * 3812015801u);
                            _6943 ^= (_6943 >> 16u);
                            _6943 *= 2246822519u;
                            _6943 ^= (_6943 >> 13u);
                            float _6944 = float(_6943 & 65535u);
                            float _6945 = 65535.0;
                            Interval _12062 = iratio(_6944, _6945, intervalFailed, optical_product_upper, interval_divide_upper);
                            Interval _6946 = _12062;
                            Interval _7954 = _6946;
                            if (_7954.hi > 0.63999998569488525390625)
                            {
                                Interval _7958 = _7931;
                                float _7959 = float(_7952);
                                float _6940 = _7959;
                                float _6941 = _7959;
                                _6938.lo = _6940;
                                _6938.hi = _6941;
                                Interval _6939 = _6938;
                                Interval _6942 = _6939;
                                Interval _7960 = _6942;
                                Interval _6934 = _7958;
                                Interval _6935 = _7960;
                                float _6931 = as_type<float>(as_type<uint>(_6935.hi) ^ 2147483648u);
                                float _6932 = as_type<float>(as_type<uint>(_6935.lo) ^ 2147483648u);
                                _6929.lo = _6931;
                                _6929.hi = _6932;
                                Interval _6930 = _6929;
                                Interval _6933 = _6930;
                                Interval _6936 = _6933;
                                Interval _12100 = iadd(_6934, _6936, intervalFailed);
                                Interval _6937 = _12100;
                                Interval _7961 = _6937;
                                float _7962 = 28.0;
                                float _7963 = 100.0;
                                Interval _12102 = iratio(_7962, _7963, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7964 = _12102;
                                float _7965 = 44.0;
                                float _7966 = 100.0;
                                Interval _12103 = iratio(_7965, _7966, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7967 = _12103;
                                int _7968 = _7952 + 19;
                                int _7969 = _7953;
                                uint _6925 = (uint(_7968) * 1597334677u) ^ (uint(_7969) * 3812015801u);
                                _6925 ^= (_6925 >> 16u);
                                _6925 *= 2246822519u;
                                _6925 ^= (_6925 >> 13u);
                                float _6926 = float(_6925 & 65535u);
                                float _6927 = 65535.0;
                                Interval _12123 = iratio(_6926, _6927, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6928 = _12123;
                                Interval _7970 = _6928;
                                Interval _12125 = imul(_7967, _7970, intervalFailed, optical_product_upper);
                                Interval _7971 = _12125;
                                Interval _12126 = iadd(_7964, _7971, intervalFailed);
                                Interval _7972 = _12126;
                                Interval _6921 = _7961;
                                Interval _6922 = _7972;
                                float _6918 = as_type<float>(as_type<uint>(_6922.hi) ^ 2147483648u);
                                float _6919 = as_type<float>(as_type<uint>(_6922.lo) ^ 2147483648u);
                                _6916.lo = _6918;
                                _6916.hi = _6919;
                                Interval _6917 = _6916;
                                Interval _6920 = _6917;
                                Interval _6923 = _6920;
                                Interval _12146 = iadd(_6921, _6923, intervalFailed);
                                Interval _6924 = _12146;
                                Interval _7957 = _6924;
                                Interval _7974 = _7935;
                                float _7975 = float(_7953);
                                float _6913 = _7975;
                                float _6914 = _7975;
                                _6911.lo = _6913;
                                _6911.hi = _6914;
                                Interval _6912 = _6911;
                                Interval _6915 = _6912;
                                Interval _7976 = _6915;
                                Interval _6907 = _7974;
                                Interval _6908 = _7976;
                                float _6904 = as_type<float>(as_type<uint>(_6908.hi) ^ 2147483648u);
                                float _6905 = as_type<float>(as_type<uint>(_6908.lo) ^ 2147483648u);
                                _6902.lo = _6904;
                                _6902.hi = _6905;
                                Interval _6903 = _6902;
                                Interval _6906 = _6903;
                                Interval _6909 = _6906;
                                Interval _12179 = iadd(_6907, _6909, intervalFailed);
                                Interval _6910 = _12179;
                                Interval _7977 = _6910;
                                float _7978 = 28.0;
                                float _7979 = 100.0;
                                Interval _12181 = iratio(_7978, _7979, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7980 = _12181;
                                float _7981 = 44.0;
                                float _7982 = 100.0;
                                Interval _12182 = iratio(_7981, _7982, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7983 = _12182;
                                int _7984 = _7952;
                                int _7985 = _7953 + 29;
                                uint _6898 = (uint(_7984) * 1597334677u) ^ (uint(_7985) * 3812015801u);
                                _6898 ^= (_6898 >> 16u);
                                _6898 *= 2246822519u;
                                _6898 ^= (_6898 >> 13u);
                                float _6899 = float(_6898 & 65535u);
                                float _6900 = 65535.0;
                                Interval _12202 = iratio(_6899, _6900, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _6901 = _12202;
                                Interval _7986 = _6901;
                                Interval _12204 = imul(_7983, _7986, intervalFailed, optical_product_upper);
                                Interval _7987 = _12204;
                                Interval _12205 = iadd(_7980, _7987, intervalFailed);
                                Interval _7988 = _12205;
                                Interval _6894 = _7977;
                                Interval _6895 = _7988;
                                float _6891 = as_type<float>(as_type<uint>(_6895.hi) ^ 2147483648u);
                                float _6892 = as_type<float>(as_type<uint>(_6895.lo) ^ 2147483648u);
                                _6889.lo = _6891;
                                _6889.hi = _6892;
                                Interval _6890 = _6889;
                                Interval _6893 = _6890;
                                Interval _6896 = _6893;
                                Interval _12225 = iadd(_6894, _6896, intervalFailed);
                                Interval _6897 = _12225;
                                Interval _7973 = _6897;
                                Interval _7990 = _8446;
                                float _7991 = 14.0;
                                float _7992 = 100.0;
                                Interval _12228 = iratio(_7991, _7992, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _7993 = _12228;
                                Interval _12229 = imul(_7990, _7993, intervalFailed, optical_product_upper);
                                Interval _7994 = _12229;
                                Interval _7995 = _7954;
                                float _7996 = 7.0;
                                float _6886 = _7996;
                                float _6887 = _7996;
                                _6884.lo = _6886;
                                _6884.hi = _6887;
                                Interval _6885 = _6884;
                                Interval _6888 = _6885;
                                Interval _7997 = _6888;
                                Interval _12240 = imul(_7995, _7997, intervalFailed, optical_product_upper);
                                Interval _7998 = _12240;
                                Interval _12241 = iadd(_7994, _7998, intervalFailed);
                                Interval _7989 = _12241;
                                if (floor(_7989.lo) == floor(_7989.hi))
                                {
                                    Interval _7999 = _7989;
                                    float _8000 = floor(_7989.lo);
                                    float _6881 = _8000;
                                    float _6882 = _8000;
                                    _6879.lo = _6881;
                                    _6879.hi = _6882;
                                    Interval _6880 = _6879;
                                    Interval _6883 = _6880;
                                    Interval _8001 = _6883;
                                    Interval _6875 = _7999;
                                    Interval _6876 = _8001;
                                    float _6872 = as_type<float>(as_type<uint>(_6876.hi) ^ 2147483648u);
                                    float _6873 = as_type<float>(as_type<uint>(_6876.lo) ^ 2147483648u);
                                    _6870.lo = _6872;
                                    _6870.hi = _6873;
                                    Interval _6871 = _6870;
                                    Interval _6874 = _6871;
                                    Interval _6877 = _6874;
                                    Interval _12284 = iadd(_6875, _6877, intervalFailed);
                                    Interval _6878 = _12284;
                                    _7989 = _6878;
                                }
                                else
                                {
                                    float _8002 = 0.0;
                                    float _8003 = 1.0;
                                    _6868.lo = _8002;
                                    _6868.hi = _8003;
                                    Interval _6869 = _6868;
                                    _7989 = _6869;
                                }
                                Interval _8005 = _7989;
                                float _8006 = 3.1415927410125732421875;
                                float _6863 = _8006;
                                float _12294 = interval_down(_6863, intervalFailed);
                                float _6864 = _12294;
                                float _6865 = _8006;
                                float _12296 = interval_up(_6865, intervalFailed);
                                float _6866 = _12296;
                                _6861.lo = _6864;
                                _6861.hi = _6866;
                                Interval _6862 = _6861;
                                Interval _6867 = _6862;
                                Interval _8007 = _6867;
                                Interval _12304 = imul(_8005, _8007, intervalFailed, optical_product_upper);
                                Interval _8008 = _12304;
                                float _6856 = _8008.lo;
                                float _6857 = _8008.hi;
                                float _6851 = _6856;
                                float _6852 = _6857;
                                _6848.lo = _6851;
                                _6848.hi = _6852;
                                Interval _6849 = _6848;
                                Interval _6853 = _6849;
                                Interval _12317 = isin_body(_6853, intervalFailed, optical_product_upper);
                                Interval _6850 = _12317;
                                interval_sine_upper = _6850.hi;
                                float _6854 = _6850.lo;
                                float _6855 = _6854;
                                float _6858 = _6855;
                                float _6859 = interval_sine_upper;
                                _6846.lo = _6858;
                                _6846.hi = _6859;
                                Interval _6847 = _6846;
                                Interval _6860 = _6847;
                                Interval _8009 = _6860;
                                bool _6839 = false;
                                if (_8009.lo <= 0.0)
                                {
                                    _6839 = _8009.hi >= 0.0;
                                }
                                if (_6839)
                                {
                                    _6838 = 0.0;
                                }
                                else
                                {
                                    _6838 = precise::min(abs(_8009.lo), abs(_8009.hi));
                                }
                                float _6837 = _6838;
                                float _6840 = precise::max(abs(_8009.lo), abs(_8009.hi));
                                float _6841 = spvFMul(_6837, _6837);
                                float _12361 = interval_down(_6841, intervalFailed);
                                float _6842 = precise::max(0.0, _12361);
                                float _6843 = spvFMul(_6840, _6840);
                                float _12365 = interval_up(_6843, intervalFailed);
                                float _6844 = _12365;
                                _6835.lo = _6842;
                                _6835.hi = _6844;
                                Interval _6836 = _6835;
                                Interval _6845 = _6836;
                                Interval _8004 = _6845;
                                float _8011 = 55.0;
                                float _8012 = 1000.0;
                                Interval _12373 = iratio(_8011, _8012, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8013 = _12373;
                                float _8014 = 14.0;
                                float _8015 = 100.0;
                                Interval _12374 = iratio(_8014, _8015, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8016 = _12374;
                                Interval _8017 = _7989;
                                Interval _12376 = imul(_8016, _8017, intervalFailed, optical_product_upper);
                                Interval _8018 = _12376;
                                Interval _12377 = iadd(_8013, _8018, intervalFailed);
                                Interval _8010 = _12377;
                                Interval _8020 = _8010;
                                bool _6828 = false;
                                if (_8020.lo <= 0.0)
                                {
                                    _6828 = _8020.hi >= 0.0;
                                }
                                if (_6828)
                                {
                                    _6827 = 0.0;
                                }
                                else
                                {
                                    _6827 = precise::min(abs(_8020.lo), abs(_8020.hi));
                                }
                                float _6826 = _6827;
                                float _6829 = precise::max(abs(_8020.lo), abs(_8020.hi));
                                float _6830 = spvFMul(_6826, _6826);
                                float _12408 = interval_down(_6830, intervalFailed);
                                float _6831 = precise::max(0.0, _12408);
                                float _6832 = spvFMul(_6829, _6829);
                                float _12412 = interval_up(_6832, intervalFailed);
                                float _6833 = _12412;
                                _6824.lo = _6831;
                                _6824.hi = _6833;
                                Interval _6825 = _6824;
                                Interval _6834 = _6825;
                                Interval _8019 = _6834;
                                float _8022 = 1.0;
                                float _6821 = _8022;
                                float _6822 = _8022;
                                _6819.lo = _6821;
                                _6819.hi = _6822;
                                Interval _6820 = _6819;
                                Interval _6823 = _6820;
                                Interval _8023 = _6823;
                                Interval _8024 = _7957;
                                bool _6812 = false;
                                if (_8024.lo <= 0.0)
                                {
                                    _6812 = _8024.hi >= 0.0;
                                }
                                if (_6812)
                                {
                                    _6811 = 0.0;
                                }
                                else
                                {
                                    _6811 = precise::min(abs(_8024.lo), abs(_8024.hi));
                                }
                                float _6810 = _6811;
                                float _6813 = precise::max(abs(_8024.lo), abs(_8024.hi));
                                float _6814 = spvFMul(_6810, _6810);
                                float _12459 = interval_down(_6814, intervalFailed);
                                float _6815 = precise::max(0.0, _12459);
                                float _6816 = spvFMul(_6813, _6813);
                                float _12463 = interval_up(_6816, intervalFailed);
                                float _6817 = _12463;
                                _6808.lo = _6815;
                                _6808.hi = _6817;
                                Interval _6809 = _6808;
                                Interval _6818 = _6809;
                                Interval _8025 = _6818;
                                Interval _8026 = _7973;
                                bool _6801 = false;
                                if (_8026.lo <= 0.0)
                                {
                                    _6801 = _8026.hi >= 0.0;
                                }
                                if (_6801)
                                {
                                    _6800 = 0.0;
                                }
                                else
                                {
                                    _6800 = precise::min(abs(_8026.lo), abs(_8026.hi));
                                }
                                float _6799 = _6800;
                                float _6802 = precise::max(abs(_8026.lo), abs(_8026.hi));
                                float _6803 = spvFMul(_6799, _6799);
                                float _12501 = interval_down(_6803, intervalFailed);
                                float _6804 = precise::max(0.0, _12501);
                                float _6805 = spvFMul(_6802, _6802);
                                float _12505 = interval_up(_6805, intervalFailed);
                                float _6806 = _12505;
                                _6797.lo = _6804;
                                _6797.hi = _6806;
                                Interval _6798 = _6797;
                                Interval _6807 = _6798;
                                Interval _8027 = _6807;
                                Interval _12513 = iadd(_8025, _8027, intervalFailed);
                                Interval _8028 = _12513;
                                Interval _8029 = _8019;
                                Interval _12515 = idiv(_8028, _8029, intervalFailed, interval_divide_upper);
                                Interval _8030 = _12515;
                                Interval _6793 = _8023;
                                Interval _6794 = _8030;
                                float _6790 = as_type<float>(as_type<uint>(_6794.hi) ^ 2147483648u);
                                float _6791 = as_type<float>(as_type<uint>(_6794.lo) ^ 2147483648u);
                                _6788.lo = _6790;
                                _6788.hi = _6791;
                                Interval _6789 = _6788;
                                Interval _6792 = _6789;
                                Interval _6795 = _6792;
                                Interval _12535 = iadd(_6793, _6795, intervalFailed);
                                Interval _6796 = _12535;
                                Interval _8031 = _6796;
                                float _8032 = 0.0;
                                float _6785 = _8032;
                                float _6786 = _8032;
                                _6783.lo = _6785;
                                _6783.hi = _6786;
                                Interval _6784 = _6783;
                                Interval _6787 = _6784;
                                Interval _8033 = _6787;
                                float _8034 = 1.0;
                                float _6780 = _8034;
                                float _6781 = _8034;
                                _6778.lo = _6780;
                                _6778.hi = _6781;
                                Interval _6779 = _6778;
                                Interval _6782 = _6779;
                                Interval _8035 = _6782;
                                Interval _6773 = _8031;
                                Interval _6774 = _8033;
                                float _6770 = precise::max(_6773.lo, _6774.lo);
                                float _6771 = precise::max(_6773.hi, _6774.hi);
                                _6768.lo = _6770;
                                _6768.hi = _6771;
                                Interval _6769 = _6768;
                                Interval _6772 = _6769;
                                Interval _6775 = _6772;
                                Interval _6776 = _8035;
                                float _6765 = precise::min(_6775.lo, _6776.lo);
                                float _6766 = precise::min(_6775.hi, _6776.hi);
                                _6763.lo = _6765;
                                _6763.hi = _6766;
                                Interval _6764 = _6763;
                                Interval _6767 = _6764;
                                Interval _6777 = _6767;
                                Interval _8021 = _6777;
                                float _8037 = 10.0;
                                float _6760 = _8037;
                                float _6761 = _8037;
                                _6758.lo = _6760;
                                _6758.hi = _6761;
                                Interval _6759 = _6758;
                                Interval _6762 = _6759;
                                Interval _8038 = _6762;
                                Interval _8039 = _8004;
                                Interval _12603 = imul(_8038, _8039, intervalFailed, optical_product_upper);
                                Interval _8040 = _12603;
                                Interval _8041 = _8447;
                                float _8042 = 18.0;
                                float _8043 = 100.0;
                                Interval _12605 = iratio(_8042, _8043, intervalFailed, optical_product_upper, interval_divide_upper);
                                Interval _8044 = _12605;
                                Interval _6747 = _8041;
                                Interval _6748 = _8044;
                                Interval _12608 = imul(_6747, _6748, intervalFailed, optical_product_upper);
                                Interval _6749 = _12608;
                                bool _6739 = false;
                                if (_6749.lo <= 0.0)
                                {
                                    _6739 = _6749.hi >= 0.0;
                                }
                                if (_6739)
                                {
                                    _6738 = 0.0;
                                }
                                else
                                {
                                    _6738 = precise::min(abs(_6749.lo), abs(_6749.hi));
                                }
                                float _6737 = _6738;
                                float _6740 = precise::max(abs(_6749.lo), abs(_6749.hi));
                                float _6741 = spvFMul(_6737, _6737);
                                float _12638 = interval_down(_6741, intervalFailed);
                                float _6742 = precise::max(0.0, _12638);
                                float _6743 = spvFMul(_6740, _6740);
                                float _12642 = interval_up(_6743, intervalFailed);
                                float _6744 = _12642;
                                _6735.lo = _6742;
                                _6735.hi = _6744;
                                Interval _6736 = _6735;
                                Interval _6745 = _6736;
                                Interval _6746 = _6745;
                                float _6750 = 1.0;
                                float _6732 = _6750;
                                float _6733 = _6750;
                                _6730.lo = _6732;
                                _6730.hi = _6733;
                                Interval _6731 = _6730;
                                Interval _6734 = _6731;
                                Interval _6751 = _6734;
                                float _6752 = 1.0;
                                float _6727 = _6752;
                                float _6728 = _6752;
                                _6725.lo = _6727;
                                _6725.hi = _6728;
                                Interval _6726 = _6725;
                                Interval _6729 = _6726;
                                Interval _6753 = _6729;
                                Interval _6754 = _6746;
                                bool _6718 = false;
                                if (_6754.lo <= 0.0)
                                {
                                    _6718 = _6754.hi >= 0.0;
                                }
                                if (_6718)
                                {
                                    _6717 = 0.0;
                                }
                                else
                                {
                                    _6717 = precise::min(abs(_6754.lo), abs(_6754.hi));
                                }
                                float _6716 = _6717;
                                float _6719 = precise::max(abs(_6754.lo), abs(_6754.hi));
                                float _6720 = spvFMul(_6716, _6716);
                                float _12698 = interval_down(_6720, intervalFailed);
                                float _6721 = precise::max(0.0, _12698);
                                float _6722 = spvFMul(_6719, _6719);
                                float _12702 = interval_up(_6722, intervalFailed);
                                float _6723 = _12702;
                                _6714.lo = _6721;
                                _6714.hi = _6723;
                                Interval _6715 = _6714;
                                Interval _6724 = _6715;
                                Interval _6755 = _6724;
                                Interval _12710 = iadd(_6753, _6755, intervalFailed);
                                Interval _6756 = _12710;
                                Interval _12711 = idiv(_6751, _6756, intervalFailed, interval_divide_upper);
                                Interval _6757 = _12711;
                                Interval _8045 = _6757;
                                Interval _12713 = imul(_8040, _8045, intervalFailed, optical_product_upper);
                                Interval _8036 = _12713;
                                Interval _8047 = _8021;
                                bool _6707 = false;
                                if (_8047.lo <= 0.0)
                                {
                                    _6707 = _8047.hi >= 0.0;
                                }
                                if (_6707)
                                {
                                    _6706 = 0.0;
                                }
                                else
                                {
                                    _6706 = precise::min(abs(_8047.lo), abs(_8047.hi));
                                }
                                float _6705 = _6706;
                                float _6708 = precise::max(abs(_8047.lo), abs(_8047.hi));
                                float _6709 = spvFMul(_6705, _6705);
                                float _12744 = interval_down(_6709, intervalFailed);
                                float _6710 = precise::max(0.0, _12744);
                                float _6711 = spvFMul(_6708, _6708);
                                float _12748 = interval_up(_6711, intervalFailed);
                                float _6712 = _12748;
                                _6703.lo = _6710;
                                _6703.hi = _6712;
                                Interval _6704 = _6703;
                                Interval _6713 = _6704;
                                Interval _8046 = _6713;
                                Interval _8049 = _8036;
                                Interval _8050 = _8046;
                                Interval _12758 = imul(_8049, _8050, intervalFailed, optical_product_upper);
                                Interval _8051 = _12758;
                                Interval _8052 = _8021;
                                Interval _12760 = imul(_8051, _8052, intervalFailed, optical_product_upper);
                                Interval _8048 = _12760;
                                float _8054 = -6.0;
                                float _6700 = _8054;
                                float _6701 = _8054;
                                _6698.lo = _6700;
                                _6698.hi = _6701;
                                Interval _6699 = _6698;
                                Interval _6702 = _6699;
                                Interval _8055 = _6702;
                                Interval _8056 = _8036;
                                Interval _12771 = imul(_8055, _8056, intervalFailed, optical_product_upper);
                                Interval _8057 = _12771;
                                Interval _8058 = _8046;
                                Interval _12773 = imul(_8057, _8058, intervalFailed, optical_product_upper);
                                Interval _8059 = _12773;
                                float _8060 = 128.0;
                                float _6695 = _8060;
                                float _6696 = _8060;
                                _6693.lo = _6695;
                                _6693.hi = _6696;
                                Interval _6694 = _6693;
                                Interval _6697 = _6694;
                                Interval _8061 = _6697;
                                Interval _8062 = _8019;
                                Interval _12784 = imul(_8061, _8062, intervalFailed, optical_product_upper);
                                Interval _8063 = _12784;
                                Interval _12785 = idiv(_8059, _8063, intervalFailed, interval_divide_upper);
                                Interval _8053 = _12785;
                                Interval _8065 = _8053;
                                Interval _8066 = _7957;
                                Interval _12788 = imul(_8065, _8066, intervalFailed, optical_product_upper);
                                Interval _8064 = _12788;
                                Interval _8068 = _8053;
                                Interval _8069 = _7973;
                                Interval _12791 = imul(_8068, _8069, intervalFailed, optical_product_upper);
                                Interval _8067 = _12791;
                                bool _8070 = true;
                                if ((isunordered(_7954.lo, 0.63999998569488525390625) || _7954.lo > 0.63999998569488525390625))
                                {
                                    _8070 = _8447.hi >= 24.0;
                                }
                                if (_8070)
                                {
                                    Interval _8071 = _8048;
                                    float _8072 = 0.0;
                                    float _6690 = _8072;
                                    float _6691 = _8072;
                                    _6688.lo = _6690;
                                    _6688.hi = _6691;
                                    Interval _6689 = _6688;
                                    Interval _6692 = _6689;
                                    Interval _8073 = _6692;
                                    float _6685 = precise::min(_8071.lo, _8073.lo);
                                    float _6686 = precise::max(_8071.hi, _8073.hi);
                                    _6683.lo = _6685;
                                    _6683.hi = _6686;
                                    Interval _6684 = _6683;
                                    Interval _6687 = _6684;
                                    _8048 = _6687;
                                    Interval _8074 = _8064;
                                    float _8075 = 0.0;
                                    float _6680 = _8075;
                                    float _6681 = _8075;
                                    _6678.lo = _6680;
                                    _6678.hi = _6681;
                                    Interval _6679 = _6678;
                                    Interval _6682 = _6679;
                                    Interval _8076 = _6682;
                                    float _6675 = precise::min(_8074.lo, _8076.lo);
                                    float _6676 = precise::max(_8074.hi, _8076.hi);
                                    _6673.lo = _6675;
                                    _6673.hi = _6676;
                                    Interval _6674 = _6673;
                                    Interval _6677 = _6674;
                                    _8064 = _6677;
                                    Interval _8077 = _8067;
                                    float _8078 = 0.0;
                                    float _6670 = _8078;
                                    float _6671 = _8078;
                                    _6668.lo = _6670;
                                    _6668.hi = _6671;
                                    Interval _6669 = _6668;
                                    Interval _6672 = _6669;
                                    Interval _8079 = _6672;
                                    float _6665 = precise::min(_8077.lo, _8079.lo);
                                    float _6666 = precise::max(_8077.hi, _8079.hi);
                                    _6663.lo = _6665;
                                    _6663.hi = _6666;
                                    Interval _6664 = _6663;
                                    Interval _6667 = _6664;
                                    _8067 = _6667;
                                }
                                Interval _8080 = _7785;
                                Interval _8081 = _8048;
                                Interval _12886 = iadd(_8080, _8081, intervalFailed);
                                _7785 = _12886;
                                Interval _8082 = _7812;
                                Interval _8083 = _8064;
                                Interval _12889 = iadd(_8082, _8083, intervalFailed);
                                _7812 = _12889;
                                Interval _8084 = _7847;
                                Interval _8085 = _8067;
                                Interval _12892 = iadd(_8084, _8085, intervalFailed);
                                _7847 = _12892;
                            }
                        }
                    }
                    Interval _8086 = _7785;
                    Interval _8087 = _7812;
                    Interval _8088 = _7847;
                    _6661.x = _8086;
                    _6661.y = _8087;
                    _6661.z = _8088;
                    Interval3 _6662 = _6661;
                    Interval3 _8089 = _6662;
                    Interval3 _8443 = _8089;
                    if (_8442 == 0u)
                    {
                        Interval _8449 = param_var_distance;
                        float _8450 = 2.0;
                        float _8451 = 10.0;
                        Interval _12911 = iratio(_8450, _8451, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8452 = _12911;
                        Interval _12912 = imul(_8449, _8452, intervalFailed, optical_product_upper);
                        Interval _8448 = _12912;
                        Interval _8454 = _8443.x;
                        float _6658 = as_type<float>(as_type<uint>(_8454.hi) ^ 2147483648u);
                        float _6659 = as_type<float>(as_type<uint>(_8454.lo) ^ 2147483648u);
                        _6656.lo = _6658;
                        _6656.hi = _6659;
                        Interval _6657 = _6656;
                        Interval _6660 = _6657;
                        Interval _8455 = _6660;
                        Interval _8456 = _8439.y;
                        float _8457 = 12.0;
                        float _8458 = 100.0;
                        Interval _12934 = iratio(_8457, _8458, intervalFailed, optical_product_upper, interval_divide_upper);
                        Interval _8459 = _12934;
                        float _6653 = precise::max(_8456.lo, _8459.lo);
                        float _6654 = precise::max(_8456.hi, _8459.hi);
                        _6651.lo = _6653;
                        _6651.hi = _6654;
                        Interval _6652 = _6651;
                        Interval _6655 = _6652;
                        Interval _8460 = _6655;
                        Interval _12952 = idiv(_8455, _8460, intervalFailed, interval_divide_upper);
                        Interval _8461 = _12952;
                        Interval _8462 = _8448;
                        float _6648 = as_type<float>(as_type<uint>(_8462.hi) ^ 2147483648u);
                        float _6649 = as_type<float>(as_type<uint>(_8462.lo) ^ 2147483648u);
                        _6646.lo = _6648;
                        _6646.hi = _6649;
                        Interval _6647 = _6646;
                        Interval _6650 = _6647;
                        Interval _8463 = _6650;
                        Interval _8464 = _8448;
                        Interval _6641 = _8461;
                        Interval _6642 = _8463;
                        float _6638 = precise::max(_6641.lo, _6642.lo);
                        float _6639 = precise::max(_6641.hi, _6642.hi);
                        _6636.lo = _6638;
                        _6636.hi = _6639;
                        Interval _6637 = _6636;
                        Interval _6640 = _6637;
                        Interval _6643 = _6640;
                        Interval _6644 = _8464;
                        float _6633 = precise::min(_6643.lo, _6644.lo);
                        float _6634 = precise::min(_6643.hi, _6644.hi);
                        _6631.lo = _6633;
                        _6631.hi = _6634;
                        Interval _6632 = _6631;
                        Interval _6635 = _6632;
                        Interval _6645 = _6635;
                        Interval _8453 = _6645;
                        Interval3 _8465 = _8428;
                        Interval3 _8466 = _8439;
                        Interval _8467 = _8453;
                        Interval _6621 = _8466.x;
                        Interval _6622 = _8467;
                        Interval _13016 = imul(_6621, _6622, intervalFailed, optical_product_upper);
                        Interval _6623 = _13016;
                        Interval _6624 = _8466.y;
                        Interval _6625 = _8467;
                        Interval _13020 = imul(_6624, _6625, intervalFailed, optical_product_upper);
                        Interval _6626 = _13020;
                        Interval _6627 = _8466.z;
                        Interval _6628 = _8467;
                        Interval _13024 = imul(_6627, _6628, intervalFailed, optical_product_upper);
                        Interval _6629 = _13024;
                        _6619.x = _6623;
                        _6619.y = _6626;
                        _6619.z = _6629;
                        Interval3 _6620 = _6619;
                        Interval3 _6630 = _6620;
                        Interval3 _8468 = _6630;
                        Interval _6609 = _8465.x;
                        Interval _6610 = _8468.x;
                        Interval _13038 = iadd(_6609, _6610, intervalFailed);
                        Interval _6611 = _13038;
                        Interval _6612 = _8465.y;
                        Interval _6613 = _8468.y;
                        Interval _13043 = iadd(_6612, _6613, intervalFailed);
                        Interval _6614 = _13043;
                        Interval _6615 = _8465.z;
                        Interval _6616 = _8468.z;
                        Interval _13048 = iadd(_6615, _6616, intervalFailed);
                        Interval _6617 = _13048;
                        _6607.x = _6611;
                        _6607.y = _6614;
                        _6607.z = _6617;
                        Interval3 _6608 = _6607;
                        Interval3 _6618 = _6608;
                        _8428 = _6618;
                        Interval3 _8469 = hit;
                        Interval3 _8470 = param_var_direction_1;
                        Interval _8471 = _8453;
                        Interval _6597 = _8470.x;
                        Interval _6598 = _8471;
                        Interval _13064 = imul(_6597, _6598, intervalFailed, optical_product_upper);
                        Interval _6599 = _13064;
                        Interval _6600 = _8470.y;
                        Interval _6601 = _8471;
                        Interval _13068 = imul(_6600, _6601, intervalFailed, optical_product_upper);
                        Interval _6602 = _13068;
                        Interval _6603 = _8470.z;
                        Interval _6604 = _8471;
                        Interval _13072 = imul(_6603, _6604, intervalFailed, optical_product_upper);
                        Interval _6605 = _13072;
                        _6595.x = _6599;
                        _6595.y = _6602;
                        _6595.z = _6605;
                        Interval3 _6596 = _6595;
                        Interval3 _6606 = _6596;
                        Interval3 _8472 = _6606;
                        Interval _6585 = _8469.x;
                        Interval _6586 = _8472.x;
                        Interval _13086 = iadd(_6585, _6586, intervalFailed);
                        Interval _6587 = _13086;
                        Interval _6588 = _8469.y;
                        Interval _6589 = _8472.y;
                        Interval _13091 = iadd(_6588, _6589, intervalFailed);
                        Interval _6590 = _13091;
                        Interval _6591 = _8469.z;
                        Interval _6592 = _8472.z;
                        Interval _13096 = iadd(_6591, _6592, intervalFailed);
                        Interval _6593 = _13096;
                        _6583.x = _6587;
                        _6583.y = _6590;
                        _6583.z = _6593;
                        Interval3 _6584 = _6583;
                        Interval3 _6594 = _6584;
                        hit = _6594;
                    }
                    else
                    {
                        _8436 = _8443.y;
                        _8437 = _8443.z;
                    }
                }
            }
            else
            {
                Interval _8474 = _8428.x;
                float _8475 = 18.0;
                float _8476 = 1000.0;
                Interval _13113 = iratio(_8475, _8476, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8477 = _13113;
                Interval _13114 = imul(_8474, _8477, intervalFailed, optical_product_upper);
                Interval _8478 = _13114;
                Interval _8479 = _8428.z;
                float _8480 = 11.0;
                float _8481 = 1000.0;
                Interval _13117 = iratio(_8480, _8481, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8482 = _13117;
                Interval _13118 = imul(_8479, _8482, intervalFailed, optical_product_upper);
                Interval _8483 = _13118;
                Interval _13119 = iadd(_8478, _8483, intervalFailed);
                Interval _8484 = _13119;
                Interval _8485 = _8434;
                float _8486 = 8.0;
                float _8487 = 10.0;
                Interval _13121 = iratio(_8486, _8487, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8488 = _13121;
                Interval _13122 = imul(_8485, _8488, intervalFailed, optical_product_upper);
                Interval _8489 = _13122;
                Interval _6579 = _8484;
                Interval _6580 = _8489;
                float _6576 = as_type<float>(as_type<uint>(_6580.hi) ^ 2147483648u);
                float _6577 = as_type<float>(as_type<uint>(_6580.lo) ^ 2147483648u);
                _6574.lo = _6576;
                _6574.hi = _6577;
                Interval _6575 = _6574;
                Interval _6578 = _6575;
                Interval _6581 = _6578;
                Interval _13142 = iadd(_6579, _6581, intervalFailed);
                Interval _6582 = _13142;
                Interval _8473 = _6582;
                Interval _8491 = _8428.x;
                float _8492 = 47.0;
                float _8493 = 1000.0;
                Interval _13146 = iratio(_8492, _8493, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8494 = _13146;
                Interval _13147 = imul(_8491, _8494, intervalFailed, optical_product_upper);
                Interval _8495 = _13147;
                Interval _8496 = _8428.z;
                float _8497 = 25.0;
                float _8498 = 1000.0;
                Interval _13150 = iratio(_8497, _8498, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8499 = _13150;
                Interval _13151 = imul(_8496, _8499, intervalFailed, optical_product_upper);
                Interval _8500 = _13151;
                Interval _6570 = _8495;
                Interval _6571 = _8500;
                float _6567 = as_type<float>(as_type<uint>(_6571.hi) ^ 2147483648u);
                float _6568 = as_type<float>(as_type<uint>(_6571.lo) ^ 2147483648u);
                _6565.lo = _6567;
                _6565.hi = _6568;
                Interval _6566 = _6565;
                Interval _6569 = _6566;
                Interval _6572 = _6569;
                Interval _13171 = iadd(_6570, _6572, intervalFailed);
                Interval _6573 = _13171;
                Interval _8501 = _6573;
                Interval _8502 = _8434;
                float _8503 = 12.0;
                float _8504 = 10.0;
                Interval _13174 = iratio(_8503, _8504, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8505 = _13174;
                Interval _13175 = imul(_8502, _8505, intervalFailed, optical_product_upper);
                Interval _8506 = _13175;
                Interval _13176 = iadd(_8501, _8506, intervalFailed);
                Interval _8490 = _13176;
                Interval _8508 = _8428.z;
                float _8509 = 22.0;
                float _8510 = 1000.0;
                Interval _13179 = iratio(_8509, _8510, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8511 = _13179;
                Interval _13180 = imul(_8508, _8511, intervalFailed, optical_product_upper);
                Interval _8512 = _13180;
                Interval _8513 = _8428.x;
                float _8514 = 9.0;
                float _8515 = 1000.0;
                Interval _13183 = iratio(_8514, _8515, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8516 = _13183;
                Interval _13184 = imul(_8513, _8516, intervalFailed, optical_product_upper);
                Interval _8517 = _13184;
                Interval _6561 = _8512;
                Interval _6562 = _8517;
                float _6558 = as_type<float>(as_type<uint>(_6562.hi) ^ 2147483648u);
                float _6559 = as_type<float>(as_type<uint>(_6562.lo) ^ 2147483648u);
                _6556.lo = _6558;
                _6556.hi = _6559;
                Interval _6557 = _6556;
                Interval _6560 = _6557;
                Interval _6563 = _6560;
                Interval _13204 = iadd(_6561, _6563, intervalFailed);
                Interval _6564 = _13204;
                Interval _8518 = _6564;
                Interval _8519 = _8434;
                float _8520 = 65.0;
                float _8521 = 100.0;
                Interval _13207 = iratio(_8520, _8521, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8522 = _13207;
                Interval _13208 = imul(_8519, _8522, intervalFailed, optical_product_upper);
                Interval _8523 = _13208;
                Interval _6552 = _8518;
                Interval _6553 = _8523;
                float _6549 = as_type<float>(as_type<uint>(_6553.hi) ^ 2147483648u);
                float _6550 = as_type<float>(as_type<uint>(_6553.lo) ^ 2147483648u);
                _6547.lo = _6549;
                _6547.hi = _6550;
                Interval _6548 = _6547;
                Interval _6551 = _6548;
                Interval _6554 = _6551;
                Interval _13228 = iadd(_6552, _6554, intervalFailed);
                Interval _6555 = _13228;
                Interval _8507 = _6555;
                float _8524 = 55.0;
                float _8525 = 1000.0;
                Interval _13230 = iratio(_8524, _8525, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8526 = _13230;
                Interval _8527 = _8473;
                Interval _6540 = _8527;
                float _6538 = 3.1415927410125732421875;
                float _6533 = _6538;
                float _13234 = interval_down(_6533, intervalFailed);
                float _6534 = _13234;
                float _6535 = _6538;
                float _13236 = interval_up(_6535, intervalFailed);
                float _6536 = _13236;
                _6531.lo = _6534;
                _6531.hi = _6536;
                Interval _6532 = _6531;
                Interval _6537 = _6532;
                Interval _6539 = _6537;
                Interval _6541 = _6539;
                float _6542 = 0.5;
                float _6528 = _6542;
                float _6529 = _6542;
                _6526.lo = _6528;
                _6526.hi = _6529;
                Interval _6527 = _6526;
                Interval _6530 = _6527;
                Interval _6543 = _6530;
                Interval _13254 = imul(_6541, _6543, intervalFailed, optical_product_upper);
                Interval _6544 = _13254;
                Interval _13255 = iadd(_6540, _6544, intervalFailed);
                Interval _6545 = _13255;
                float _6521 = _6545.lo;
                float _6522 = _6545.hi;
                float _6516 = _6521;
                float _6517 = _6522;
                _6513.lo = _6516;
                _6513.hi = _6517;
                Interval _6514 = _6513;
                Interval _6518 = _6514;
                Interval _13268 = isin_body(_6518, intervalFailed, optical_product_upper);
                Interval _6515 = _13268;
                interval_sine_upper = _6515.hi;
                float _6519 = _6515.lo;
                float _6520 = _6519;
                float _6523 = _6520;
                float _6524 = interval_sine_upper;
                _6511.lo = _6523;
                _6511.hi = _6524;
                Interval _6512 = _6511;
                Interval _6525 = _6512;
                Interval _6546 = _6525;
                Interval _8528 = _6546;
                Interval _13284 = imul(_8526, _8528, intervalFailed, optical_product_upper);
                Interval _8529 = _13284;
                Interval _8530 = param_var_footprint;
                float _8531 = 22.0;
                float _8532 = 1000.0;
                Interval _13286 = iratio(_8531, _8532, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8533 = _13286;
                Interval _6500 = _8530;
                Interval _6501 = _8533;
                Interval _13289 = imul(_6500, _6501, intervalFailed, optical_product_upper);
                Interval _6502 = _13289;
                bool _6492 = false;
                if (_6502.lo <= 0.0)
                {
                    _6492 = _6502.hi >= 0.0;
                }
                if (_6492)
                {
                    _6491 = 0.0;
                }
                else
                {
                    _6491 = precise::min(abs(_6502.lo), abs(_6502.hi));
                }
                float _6490 = _6491;
                float _6493 = precise::max(abs(_6502.lo), abs(_6502.hi));
                float _6494 = spvFMul(_6490, _6490);
                float _13319 = interval_down(_6494, intervalFailed);
                float _6495 = precise::max(0.0, _13319);
                float _6496 = spvFMul(_6493, _6493);
                float _13323 = interval_up(_6496, intervalFailed);
                float _6497 = _13323;
                _6488.lo = _6495;
                _6488.hi = _6497;
                Interval _6489 = _6488;
                Interval _6498 = _6489;
                Interval _6499 = _6498;
                float _6503 = 1.0;
                float _6485 = _6503;
                float _6486 = _6503;
                _6483.lo = _6485;
                _6483.hi = _6486;
                Interval _6484 = _6483;
                Interval _6487 = _6484;
                Interval _6504 = _6487;
                float _6505 = 1.0;
                float _6480 = _6505;
                float _6481 = _6505;
                _6478.lo = _6480;
                _6478.hi = _6481;
                Interval _6479 = _6478;
                Interval _6482 = _6479;
                Interval _6506 = _6482;
                Interval _6507 = _6499;
                bool _6471 = false;
                if (_6507.lo <= 0.0)
                {
                    _6471 = _6507.hi >= 0.0;
                }
                if (_6471)
                {
                    _6470 = 0.0;
                }
                else
                {
                    _6470 = precise::min(abs(_6507.lo), abs(_6507.hi));
                }
                float _6469 = _6470;
                float _6472 = precise::max(abs(_6507.lo), abs(_6507.hi));
                float _6473 = spvFMul(_6469, _6469);
                float _13379 = interval_down(_6473, intervalFailed);
                float _6474 = precise::max(0.0, _13379);
                float _6475 = spvFMul(_6472, _6472);
                float _13383 = interval_up(_6475, intervalFailed);
                float _6476 = _13383;
                _6467.lo = _6474;
                _6467.hi = _6476;
                Interval _6468 = _6467;
                Interval _6477 = _6468;
                Interval _6508 = _6477;
                Interval _13391 = iadd(_6506, _6508, intervalFailed);
                Interval _6509 = _13391;
                Interval _13392 = idiv(_6504, _6509, intervalFailed, interval_divide_upper);
                Interval _6510 = _13392;
                Interval _8534 = _6510;
                Interval _13394 = imul(_8529, _8534, intervalFailed, optical_product_upper);
                Interval _8535 = _13394;
                float _8536 = 25.0;
                float _8537 = 1000.0;
                Interval _13395 = iratio(_8536, _8537, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8538 = _13395;
                Interval _8539 = _8490;
                Interval _6460 = _8539;
                float _6458 = 3.1415927410125732421875;
                float _6453 = _6458;
                float _13399 = interval_down(_6453, intervalFailed);
                float _6454 = _13399;
                float _6455 = _6458;
                float _13401 = interval_up(_6455, intervalFailed);
                float _6456 = _13401;
                _6451.lo = _6454;
                _6451.hi = _6456;
                Interval _6452 = _6451;
                Interval _6457 = _6452;
                Interval _6459 = _6457;
                Interval _6461 = _6459;
                float _6462 = 0.5;
                float _6448 = _6462;
                float _6449 = _6462;
                _6446.lo = _6448;
                _6446.hi = _6449;
                Interval _6447 = _6446;
                Interval _6450 = _6447;
                Interval _6463 = _6450;
                Interval _13419 = imul(_6461, _6463, intervalFailed, optical_product_upper);
                Interval _6464 = _13419;
                Interval _13420 = iadd(_6460, _6464, intervalFailed);
                Interval _6465 = _13420;
                float _6441 = _6465.lo;
                float _6442 = _6465.hi;
                float _6436 = _6441;
                float _6437 = _6442;
                _6433.lo = _6436;
                _6433.hi = _6437;
                Interval _6434 = _6433;
                Interval _6438 = _6434;
                Interval _13433 = isin_body(_6438, intervalFailed, optical_product_upper);
                Interval _6435 = _13433;
                interval_sine_upper = _6435.hi;
                float _6439 = _6435.lo;
                float _6440 = _6439;
                float _6443 = _6440;
                float _6444 = interval_sine_upper;
                _6431.lo = _6443;
                _6431.hi = _6444;
                Interval _6432 = _6431;
                Interval _6445 = _6432;
                Interval _6466 = _6445;
                Interval _8540 = _6466;
                Interval _13449 = imul(_8538, _8540, intervalFailed, optical_product_upper);
                Interval _8541 = _13449;
                Interval _8542 = param_var_footprint;
                float _8543 = 54.0;
                float _8544 = 1000.0;
                Interval _13451 = iratio(_8543, _8544, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8545 = _13451;
                Interval _6420 = _8542;
                Interval _6421 = _8545;
                Interval _13454 = imul(_6420, _6421, intervalFailed, optical_product_upper);
                Interval _6422 = _13454;
                bool _6412 = false;
                if (_6422.lo <= 0.0)
                {
                    _6412 = _6422.hi >= 0.0;
                }
                if (_6412)
                {
                    _6411 = 0.0;
                }
                else
                {
                    _6411 = precise::min(abs(_6422.lo), abs(_6422.hi));
                }
                float _6410 = _6411;
                float _6413 = precise::max(abs(_6422.lo), abs(_6422.hi));
                float _6414 = spvFMul(_6410, _6410);
                float _13484 = interval_down(_6414, intervalFailed);
                float _6415 = precise::max(0.0, _13484);
                float _6416 = spvFMul(_6413, _6413);
                float _13488 = interval_up(_6416, intervalFailed);
                float _6417 = _13488;
                _6408.lo = _6415;
                _6408.hi = _6417;
                Interval _6409 = _6408;
                Interval _6418 = _6409;
                Interval _6419 = _6418;
                float _6423 = 1.0;
                float _6405 = _6423;
                float _6406 = _6423;
                _6403.lo = _6405;
                _6403.hi = _6406;
                Interval _6404 = _6403;
                Interval _6407 = _6404;
                Interval _6424 = _6407;
                float _6425 = 1.0;
                float _6400 = _6425;
                float _6401 = _6425;
                _6398.lo = _6400;
                _6398.hi = _6401;
                Interval _6399 = _6398;
                Interval _6402 = _6399;
                Interval _6426 = _6402;
                Interval _6427 = _6419;
                bool _6391 = false;
                if (_6427.lo <= 0.0)
                {
                    _6391 = _6427.hi >= 0.0;
                }
                if (_6391)
                {
                    _6390 = 0.0;
                }
                else
                {
                    _6390 = precise::min(abs(_6427.lo), abs(_6427.hi));
                }
                float _6389 = _6390;
                float _6392 = precise::max(abs(_6427.lo), abs(_6427.hi));
                float _6393 = spvFMul(_6389, _6389);
                float _13544 = interval_down(_6393, intervalFailed);
                float _6394 = precise::max(0.0, _13544);
                float _6395 = spvFMul(_6392, _6392);
                float _13548 = interval_up(_6395, intervalFailed);
                float _6396 = _13548;
                _6387.lo = _6394;
                _6387.hi = _6396;
                Interval _6388 = _6387;
                Interval _6397 = _6388;
                Interval _6428 = _6397;
                Interval _13556 = iadd(_6426, _6428, intervalFailed);
                Interval _6429 = _13556;
                Interval _13557 = idiv(_6424, _6429, intervalFailed, interval_divide_upper);
                Interval _6430 = _13557;
                Interval _8546 = _6430;
                Interval _13559 = imul(_8541, _8546, intervalFailed, optical_product_upper);
                Interval _8547 = _13559;
                Interval _13560 = iadd(_8535, _8547, intervalFailed);
                _8436 = _13560;
                float _8548 = 45.0;
                float _8549 = 1000.0;
                Interval _13561 = iratio(_8548, _8549, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8550 = _13561;
                Interval _8551 = _8507;
                Interval _6380 = _8551;
                float _6378 = 3.1415927410125732421875;
                float _6373 = _6378;
                float _13565 = interval_down(_6373, intervalFailed);
                float _6374 = _13565;
                float _6375 = _6378;
                float _13567 = interval_up(_6375, intervalFailed);
                float _6376 = _13567;
                _6371.lo = _6374;
                _6371.hi = _6376;
                Interval _6372 = _6371;
                Interval _6377 = _6372;
                Interval _6379 = _6377;
                Interval _6381 = _6379;
                float _6382 = 0.5;
                float _6368 = _6382;
                float _6369 = _6382;
                _6366.lo = _6368;
                _6366.hi = _6369;
                Interval _6367 = _6366;
                Interval _6370 = _6367;
                Interval _6383 = _6370;
                Interval _13585 = imul(_6381, _6383, intervalFailed, optical_product_upper);
                Interval _6384 = _13585;
                Interval _13586 = iadd(_6380, _6384, intervalFailed);
                Interval _6385 = _13586;
                float _6361 = _6385.lo;
                float _6362 = _6385.hi;
                float _6356 = _6361;
                float _6357 = _6362;
                _6353.lo = _6356;
                _6353.hi = _6357;
                Interval _6354 = _6353;
                Interval _6358 = _6354;
                Interval _13599 = isin_body(_6358, intervalFailed, optical_product_upper);
                Interval _6355 = _13599;
                interval_sine_upper = _6355.hi;
                float _6359 = _6355.lo;
                float _6360 = _6359;
                float _6363 = _6360;
                float _6364 = interval_sine_upper;
                _6351.lo = _6363;
                _6351.hi = _6364;
                Interval _6352 = _6351;
                Interval _6365 = _6352;
                Interval _6386 = _6365;
                Interval _8552 = _6386;
                Interval _13615 = imul(_8550, _8552, intervalFailed, optical_product_upper);
                Interval _8553 = _13615;
                Interval _8554 = param_var_footprint;
                float _8555 = 24.0;
                float _8556 = 1000.0;
                Interval _13617 = iratio(_8555, _8556, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8557 = _13617;
                Interval _6340 = _8554;
                Interval _6341 = _8557;
                Interval _13620 = imul(_6340, _6341, intervalFailed, optical_product_upper);
                Interval _6342 = _13620;
                bool _6332 = false;
                if (_6342.lo <= 0.0)
                {
                    _6332 = _6342.hi >= 0.0;
                }
                if (_6332)
                {
                    _6331 = 0.0;
                }
                else
                {
                    _6331 = precise::min(abs(_6342.lo), abs(_6342.hi));
                }
                float _6330 = _6331;
                float _6333 = precise::max(abs(_6342.lo), abs(_6342.hi));
                float _6334 = spvFMul(_6330, _6330);
                float _13650 = interval_down(_6334, intervalFailed);
                float _6335 = precise::max(0.0, _13650);
                float _6336 = spvFMul(_6333, _6333);
                float _13654 = interval_up(_6336, intervalFailed);
                float _6337 = _13654;
                _6328.lo = _6335;
                _6328.hi = _6337;
                Interval _6329 = _6328;
                Interval _6338 = _6329;
                Interval _6339 = _6338;
                float _6343 = 1.0;
                float _6325 = _6343;
                float _6326 = _6343;
                _6323.lo = _6325;
                _6323.hi = _6326;
                Interval _6324 = _6323;
                Interval _6327 = _6324;
                Interval _6344 = _6327;
                float _6345 = 1.0;
                float _6320 = _6345;
                float _6321 = _6345;
                _6318.lo = _6320;
                _6318.hi = _6321;
                Interval _6319 = _6318;
                Interval _6322 = _6319;
                Interval _6346 = _6322;
                Interval _6347 = _6339;
                bool _6311 = false;
                if (_6347.lo <= 0.0)
                {
                    _6311 = _6347.hi >= 0.0;
                }
                if (_6311)
                {
                    _6310 = 0.0;
                }
                else
                {
                    _6310 = precise::min(abs(_6347.lo), abs(_6347.hi));
                }
                float _6309 = _6310;
                float _6312 = precise::max(abs(_6347.lo), abs(_6347.hi));
                float _6313 = spvFMul(_6309, _6309);
                float _13710 = interval_down(_6313, intervalFailed);
                float _6314 = precise::max(0.0, _13710);
                float _6315 = spvFMul(_6312, _6312);
                float _13714 = interval_up(_6315, intervalFailed);
                float _6316 = _13714;
                _6307.lo = _6314;
                _6307.hi = _6316;
                Interval _6308 = _6307;
                Interval _6317 = _6308;
                Interval _6348 = _6317;
                Interval _13722 = iadd(_6346, _6348, intervalFailed);
                Interval _6349 = _13722;
                Interval _13723 = idiv(_6344, _6349, intervalFailed, interval_divide_upper);
                Interval _6350 = _13723;
                Interval _8558 = _6350;
                Interval _13725 = imul(_8553, _8558, intervalFailed, optical_product_upper);
                Interval _8559 = _13725;
                float _8560 = 20.0;
                float _8561 = 1000.0;
                Interval _13726 = iratio(_8560, _8561, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8562 = _13726;
                Interval _8563 = _8490;
                Interval _6300 = _8563;
                float _6298 = 3.1415927410125732421875;
                float _6293 = _6298;
                float _13730 = interval_down(_6293, intervalFailed);
                float _6294 = _13730;
                float _6295 = _6298;
                float _13732 = interval_up(_6295, intervalFailed);
                float _6296 = _13732;
                _6291.lo = _6294;
                _6291.hi = _6296;
                Interval _6292 = _6291;
                Interval _6297 = _6292;
                Interval _6299 = _6297;
                Interval _6301 = _6299;
                float _6302 = 0.5;
                float _6288 = _6302;
                float _6289 = _6302;
                _6286.lo = _6288;
                _6286.hi = _6289;
                Interval _6287 = _6286;
                Interval _6290 = _6287;
                Interval _6303 = _6290;
                Interval _13750 = imul(_6301, _6303, intervalFailed, optical_product_upper);
                Interval _6304 = _13750;
                Interval _13751 = iadd(_6300, _6304, intervalFailed);
                Interval _6305 = _13751;
                float _6281 = _6305.lo;
                float _6282 = _6305.hi;
                float _6276 = _6281;
                float _6277 = _6282;
                _6273.lo = _6276;
                _6273.hi = _6277;
                Interval _6274 = _6273;
                Interval _6278 = _6274;
                Interval _13764 = isin_body(_6278, intervalFailed, optical_product_upper);
                Interval _6275 = _13764;
                interval_sine_upper = _6275.hi;
                float _6279 = _6275.lo;
                float _6280 = _6279;
                float _6283 = _6280;
                float _6284 = interval_sine_upper;
                _6271.lo = _6283;
                _6271.hi = _6284;
                Interval _6272 = _6271;
                Interval _6285 = _6272;
                Interval _6306 = _6285;
                Interval _8564 = _6306;
                Interval _13780 = imul(_8562, _8564, intervalFailed, optical_product_upper);
                Interval _8565 = _13780;
                Interval _8566 = param_var_footprint;
                float _8567 = 54.0;
                float _8568 = 1000.0;
                Interval _13782 = iratio(_8567, _8568, intervalFailed, optical_product_upper, interval_divide_upper);
                Interval _8569 = _13782;
                Interval _6260 = _8566;
                Interval _6261 = _8569;
                Interval _13785 = imul(_6260, _6261, intervalFailed, optical_product_upper);
                Interval _6262 = _13785;
                bool _6252 = false;
                if (_6262.lo <= 0.0)
                {
                    _6252 = _6262.hi >= 0.0;
                }
                if (_6252)
                {
                    _6251 = 0.0;
                }
                else
                {
                    _6251 = precise::min(abs(_6262.lo), abs(_6262.hi));
                }
                float _6250 = _6251;
                float _6253 = precise::max(abs(_6262.lo), abs(_6262.hi));
                float _6254 = spvFMul(_6250, _6250);
                float _13815 = interval_down(_6254, intervalFailed);
                float _6255 = precise::max(0.0, _13815);
                float _6256 = spvFMul(_6253, _6253);
                float _13819 = interval_up(_6256, intervalFailed);
                float _6257 = _13819;
                _6248.lo = _6255;
                _6248.hi = _6257;
                Interval _6249 = _6248;
                Interval _6258 = _6249;
                Interval _6259 = _6258;
                float _6263 = 1.0;
                float _6245 = _6263;
                float _6246 = _6263;
                _6243.lo = _6245;
                _6243.hi = _6246;
                Interval _6244 = _6243;
                Interval _6247 = _6244;
                Interval _6264 = _6247;
                float _6265 = 1.0;
                float _6240 = _6265;
                float _6241 = _6265;
                _6238.lo = _6240;
                _6238.hi = _6241;
                Interval _6239 = _6238;
                Interval _6242 = _6239;
                Interval _6266 = _6242;
                Interval _6267 = _6259;
                bool _6231 = false;
                if (_6267.lo <= 0.0)
                {
                    _6231 = _6267.hi >= 0.0;
                }
                if (_6231)
                {
                    _6230 = 0.0;
                }
                else
                {
                    _6230 = precise::min(abs(_6267.lo), abs(_6267.hi));
                }
                float _6229 = _6230;
                float _6232 = precise::max(abs(_6267.lo), abs(_6267.hi));
                float _6233 = spvFMul(_6229, _6229);
                float _13875 = interval_down(_6233, intervalFailed);
                float _6234 = precise::max(0.0, _13875);
                float _6235 = spvFMul(_6232, _6232);
                float _13879 = interval_up(_6235, intervalFailed);
                float _6236 = _13879;
                _6227.lo = _6234;
                _6227.hi = _6236;
                Interval _6228 = _6227;
                Interval _6237 = _6228;
                Interval _6268 = _6237;
                Interval _13887 = iadd(_6266, _6268, intervalFailed);
                Interval _6269 = _13887;
                Interval _13888 = idiv(_6264, _6269, intervalFailed, interval_divide_upper);
                Interval _6270 = _13888;
                Interval _8570 = _6270;
                Interval _13890 = imul(_8565, _8570, intervalFailed, optical_product_upper);
                Interval _8571 = _13890;
                Interval _6223 = _8559;
                Interval _6224 = _8571;
                float _6220 = as_type<float>(as_type<uint>(_6224.hi) ^ 2147483648u);
                float _6221 = as_type<float>(as_type<uint>(_6224.lo) ^ 2147483648u);
                _6218.lo = _6220;
                _6218.hi = _6221;
                Interval _6219 = _6218;
                Interval _6222 = _6219;
                Interval _6225 = _6222;
                Interval _13910 = iadd(_6223, _6225, intervalFailed);
                Interval _6226 = _13910;
                _8437 = _6226;
            }
            Interval _8572 = _8436;
            float _8573 = -1.0;
            float _6215 = _8573;
            float _6216 = _8573;
            _6213.lo = _6215;
            _6213.hi = _6216;
            Interval _6214 = _6213;
            Interval _6217 = _6214;
            Interval _8574 = _6217;
            Interval _8575 = _8437;
            _6211.x = _8572;
            _6211.y = _8574;
            _6211.z = _8575;
            Interval3 _6212 = _6211;
            Interval3 _8576 = _6212;
            ReflectionLiquidFrame _8577 = param_var_f;
            float3 _6198 = _8577.rotation0.xyz;
            float _6191 = _6198.x;
            float _6188 = _6191;
            float _6189 = _6191;
            _6186.lo = _6188;
            _6186.hi = _6189;
            Interval _6187 = _6186;
            Interval _6190 = _6187;
            Interval _6192 = _6190;
            float _6193 = _6198.y;
            float _6183 = _6193;
            float _6184 = _6193;
            _6181.lo = _6183;
            _6181.hi = _6184;
            Interval _6182 = _6181;
            Interval _6185 = _6182;
            Interval _6194 = _6185;
            float _6195 = _6198.z;
            float _6178 = _6195;
            float _6179 = _6195;
            _6176.lo = _6178;
            _6176.hi = _6179;
            Interval _6177 = _6176;
            Interval _6180 = _6177;
            Interval _6196 = _6180;
            _6174.x = _6192;
            _6174.y = _6194;
            _6174.z = _6196;
            Interval3 _6175 = _6174;
            Interval3 _6197 = _6175;
            Interval3 _6199 = _6197;
            Interval3 _6200 = _8576;
            Interval _6163 = _6199.x;
            Interval _6164 = _6200.x;
            Interval _13982 = imul(_6163, _6164, intervalFailed, optical_product_upper);
            Interval _6165 = _13982;
            Interval _6166 = _6199.y;
            Interval _6167 = _6200.y;
            Interval _13987 = imul(_6166, _6167, intervalFailed, optical_product_upper);
            Interval _6168 = _13987;
            Interval _13988 = iadd(_6165, _6168, intervalFailed);
            Interval _6169 = _13988;
            Interval _6170 = _6199.z;
            Interval _6171 = _6200.z;
            Interval _13993 = imul(_6170, _6171, intervalFailed, optical_product_upper);
            Interval _6172 = _13993;
            Interval _13994 = iadd(_6169, _6172, intervalFailed);
            Interval _6173 = _13994;
            Interval _6201 = _6173;
            float3 _6202 = _8577.rotation1.xyz;
            float _6156 = _6202.x;
            float _6153 = _6156;
            float _6154 = _6156;
            _6151.lo = _6153;
            _6151.hi = _6154;
            Interval _6152 = _6151;
            Interval _6155 = _6152;
            Interval _6157 = _6155;
            float _6158 = _6202.y;
            float _6148 = _6158;
            float _6149 = _6158;
            _6146.lo = _6148;
            _6146.hi = _6149;
            Interval _6147 = _6146;
            Interval _6150 = _6147;
            Interval _6159 = _6150;
            float _6160 = _6202.z;
            float _6143 = _6160;
            float _6144 = _6160;
            _6141.lo = _6143;
            _6141.hi = _6144;
            Interval _6142 = _6141;
            Interval _6145 = _6142;
            Interval _6161 = _6145;
            _6139.x = _6157;
            _6139.y = _6159;
            _6139.z = _6161;
            Interval3 _6140 = _6139;
            Interval3 _6162 = _6140;
            Interval3 _6203 = _6162;
            Interval3 _6204 = _8576;
            Interval _6128 = _6203.x;
            Interval _6129 = _6204.x;
            Interval _14046 = imul(_6128, _6129, intervalFailed, optical_product_upper);
            Interval _6130 = _14046;
            Interval _6131 = _6203.y;
            Interval _6132 = _6204.y;
            Interval _14051 = imul(_6131, _6132, intervalFailed, optical_product_upper);
            Interval _6133 = _14051;
            Interval _14052 = iadd(_6130, _6133, intervalFailed);
            Interval _6134 = _14052;
            Interval _6135 = _6203.z;
            Interval _6136 = _6204.z;
            Interval _14057 = imul(_6135, _6136, intervalFailed, optical_product_upper);
            Interval _6137 = _14057;
            Interval _14058 = iadd(_6134, _6137, intervalFailed);
            Interval _6138 = _14058;
            Interval _6205 = _6138;
            float3 _6206 = _8577.rotation2.xyz;
            float _6121 = _6206.x;
            float _6118 = _6121;
            float _6119 = _6121;
            _6116.lo = _6118;
            _6116.hi = _6119;
            Interval _6117 = _6116;
            Interval _6120 = _6117;
            Interval _6122 = _6120;
            float _6123 = _6206.y;
            float _6113 = _6123;
            float _6114 = _6123;
            _6111.lo = _6113;
            _6111.hi = _6114;
            Interval _6112 = _6111;
            Interval _6115 = _6112;
            Interval _6124 = _6115;
            float _6125 = _6206.z;
            float _6108 = _6125;
            float _6109 = _6125;
            _6106.lo = _6108;
            _6106.hi = _6109;
            Interval _6107 = _6106;
            Interval _6110 = _6107;
            Interval _6126 = _6110;
            _6104.x = _6122;
            _6104.y = _6124;
            _6104.z = _6126;
            Interval3 _6105 = _6104;
            Interval3 _6127 = _6105;
            Interval3 _6207 = _6127;
            Interval3 _6208 = _8576;
            Interval _6093 = _6207.x;
            Interval _6094 = _6208.x;
            Interval _14110 = imul(_6093, _6094, intervalFailed, optical_product_upper);
            Interval _6095 = _14110;
            Interval _6096 = _6207.y;
            Interval _6097 = _6208.y;
            Interval _14115 = imul(_6096, _6097, intervalFailed, optical_product_upper);
            Interval _6098 = _14115;
            Interval _14116 = iadd(_6095, _6098, intervalFailed);
            Interval _6099 = _14116;
            Interval _6100 = _6207.z;
            Interval _6101 = _6208.z;
            Interval _14121 = imul(_6100, _6101, intervalFailed, optical_product_upper);
            Interval _6102 = _14121;
            Interval _14122 = iadd(_6099, _6102, intervalFailed);
            Interval _6103 = _14122;
            Interval _6209 = _6103;
            _6091.x = _6201;
            _6091.y = _6205;
            _6091.z = _6209;
            Interval3 _6092 = _6091;
            Interval3 _6210 = _6092;
            Interval3 _8578 = _6210;
            Interval3 _6084 = _8578;
            float _6085 = 1.0;
            float _6081 = _6085;
            float _6082 = _6085;
            _6079.lo = _6081;
            _6079.hi = _6082;
            Interval _6080 = _6079;
            Interval _6083 = _6080;
            Interval _6086 = _6083;
            Interval3 _6087 = _8578;
            Interval _6070 = _6087.x;
            bool _6063 = false;
            if (_6070.lo <= 0.0)
            {
                _6063 = _6070.hi >= 0.0;
            }
            if (_6063)
            {
                _6062 = 0.0;
            }
            else
            {
                _6062 = precise::min(abs(_6070.lo), abs(_6070.hi));
            }
            float _6061 = _6062;
            float _6064 = precise::max(abs(_6070.lo), abs(_6070.hi));
            float _6065 = spvFMul(_6061, _6061);
            float _14175 = interval_down(_6065, intervalFailed);
            float _6066 = precise::max(0.0, _14175);
            float _6067 = spvFMul(_6064, _6064);
            float _14179 = interval_up(_6067, intervalFailed);
            float _6068 = _14179;
            _6059.lo = _6066;
            _6059.hi = _6068;
            Interval _6060 = _6059;
            Interval _6069 = _6060;
            Interval _6071 = _6069;
            Interval _6072 = _6087.y;
            bool _6052 = false;
            if (_6072.lo <= 0.0)
            {
                _6052 = _6072.hi >= 0.0;
            }
            if (_6052)
            {
                _6051 = 0.0;
            }
            else
            {
                _6051 = precise::min(abs(_6072.lo), abs(_6072.hi));
            }
            float _6050 = _6051;
            float _6053 = precise::max(abs(_6072.lo), abs(_6072.hi));
            float _6054 = spvFMul(_6050, _6050);
            float _14218 = interval_down(_6054, intervalFailed);
            float _6055 = precise::max(0.0, _14218);
            float _6056 = spvFMul(_6053, _6053);
            float _14222 = interval_up(_6056, intervalFailed);
            float _6057 = _14222;
            _6048.lo = _6055;
            _6048.hi = _6057;
            Interval _6049 = _6048;
            Interval _6058 = _6049;
            Interval _6073 = _6058;
            Interval _14230 = iadd(_6071, _6073, intervalFailed);
            Interval _6074 = _14230;
            Interval _6075 = _6087.z;
            bool _6041 = false;
            if (_6075.lo <= 0.0)
            {
                _6041 = _6075.hi >= 0.0;
            }
            if (_6041)
            {
                _6040 = 0.0;
            }
            else
            {
                _6040 = precise::min(abs(_6075.lo), abs(_6075.hi));
            }
            float _6039 = _6040;
            float _6042 = precise::max(abs(_6075.lo), abs(_6075.hi));
            float _6043 = spvFMul(_6039, _6039);
            float _14262 = interval_down(_6043, intervalFailed);
            float _6044 = precise::max(0.0, _14262);
            float _6045 = spvFMul(_6042, _6042);
            float _14266 = interval_up(_6045, intervalFailed);
            float _6046 = _14266;
            _6037.lo = _6044;
            _6037.hi = _6046;
            Interval _6038 = _6037;
            Interval _6047 = _6038;
            Interval _6076 = _6047;
            Interval _14274 = iadd(_6074, _6076, intervalFailed);
            Interval _6077 = _14274;
            Interval _14275 = isqrt(_6077, intervalFailed);
            Interval _6078 = _14275;
            Interval _6088 = _6078;
            Interval _14277 = idiv(_6086, _6088, intervalFailed, interval_divide_upper);
            Interval _6089 = _14277;
            Interval _6027 = _6084.x;
            Interval _6028 = _6089;
            Interval _14281 = imul(_6027, _6028, intervalFailed, optical_product_upper);
            Interval _6029 = _14281;
            Interval _6030 = _6084.y;
            Interval _6031 = _6089;
            Interval _14285 = imul(_6030, _6031, intervalFailed, optical_product_upper);
            Interval _6032 = _14285;
            Interval _6033 = _6084.z;
            Interval _6034 = _6089;
            Interval _14289 = imul(_6033, _6034, intervalFailed, optical_product_upper);
            Interval _6035 = _14289;
            _6025.x = _6029;
            _6025.y = _6032;
            _6025.z = _6035;
            Interval3 _6026 = _6025;
            Interval3 _6036 = _6026;
            Interval3 _6090 = _6036;
            Interval3 _8579 = _6090;
            Interval3 _8580 = param_var_direction_1;
            Interval3 _14301 = oriented(_8579, _8580, intervalFailed, optical_product_upper);
            Interval3 _8581 = _14301;
            n = _8581;
        }
        else
        {
            ReflectionSpecularPlane param_var_p_1 = plane;
            Interval3 param_var_hit = hit;
            bool _14305 = outside_face(param_var_p_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (_14305)
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
            bool _6022 = false;
            if (param_var_p_2.a.w == 2.0)
            {
                _6022 = param_var_p_2.b.w == 2.0;
            }
            bool _6023 = false;
            if (_6022)
            {
                _6023 = param_var_p_2.c.w == 2.0;
            }
            bool _6024 = _6023;
            temp_var_logical_4 = !_6024;
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
        Interval _14336 = iratio(param_var_n_2, param_var_d_1, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_a_17 = _14336;
        Interval param_var_a_18 = _distance;
        float param_var_n_3 = 1.0;
        float param_var_d_2 = 100000.0;
        Interval _14338 = iratio(param_var_n_3, param_var_d_2, intervalFailed, optical_product_upper, interval_divide_upper);
        Interval param_var_b_14 = _14338;
        Interval _14339 = imul(param_var_a_18, param_var_b_14, intervalFailed, optical_product_upper);
        Interval param_var_b_15 = _14339;
        float _6019 = precise::max(param_var_a_17.lo, param_var_b_15.lo);
        float _6020 = precise::max(param_var_a_17.hi, param_var_b_15.hi);
        _6017.lo = _6019;
        _6017.hi = _6020;
        Interval _6018 = _6017;
        Interval _6021 = _6018;
        bias0 = _6021;
        Interval3 param_var_a_19 = hit;
        Interval3 param_var_a_20 = n;
        Interval param_var_b_16 = bias0;
        Interval _6007 = param_var_a_20.x;
        Interval _6008 = param_var_b_16;
        Interval _14363 = imul(_6007, _6008, intervalFailed, optical_product_upper);
        Interval _6009 = _14363;
        Interval _6010 = param_var_a_20.y;
        Interval _6011 = param_var_b_16;
        Interval _14367 = imul(_6010, _6011, intervalFailed, optical_product_upper);
        Interval _6012 = _14367;
        Interval _6013 = param_var_a_20.z;
        Interval _6014 = param_var_b_16;
        Interval _14371 = imul(_6013, _6014, intervalFailed, optical_product_upper);
        Interval _6015 = _14371;
        _6005.x = _6009;
        _6005.y = _6012;
        _6005.z = _6015;
        Interval3 _6006 = _6005;
        Interval3 _6016 = _6006;
        Interval3 param_var_b_17 = _6016;
        Interval _5995 = param_var_a_19.x;
        Interval _5996 = param_var_b_17.x;
        Interval _14385 = iadd(_5995, _5996, intervalFailed);
        Interval _5997 = _14385;
        Interval _5998 = param_var_a_19.y;
        Interval _5999 = param_var_b_17.y;
        Interval _14390 = iadd(_5998, _5999, intervalFailed);
        Interval _6000 = _14390;
        Interval _6001 = param_var_a_19.z;
        Interval _6002 = param_var_b_17.z;
        Interval _14395 = iadd(_6001, _6002, intervalFailed);
        Interval _6003 = _14395;
        _5993.x = _5997;
        _5993.y = _6000;
        _5993.z = _6003;
        Interval3 _5994 = _5993;
        Interval3 _6004 = _5994;
        origin = _6004;
        Interval3 param_var_a_21 = outgoing;
        Interval3 param_var_n_4 = n;
        Interval3 _5983 = param_var_a_21;
        Interval3 _5984 = param_var_n_4;
        float _5985 = 2.0;
        float _5980 = _5985;
        float _5981 = _5985;
        _5978.lo = _5980;
        _5978.hi = _5981;
        Interval _5979 = _5978;
        Interval _5982 = _5979;
        Interval _5986 = _5982;
        Interval3 _5987 = param_var_a_21;
        Interval3 _5988 = param_var_n_4;
        Interval _5967 = _5987.x;
        Interval _5968 = _5988.x;
        Interval _14424 = imul(_5967, _5968, intervalFailed, optical_product_upper);
        Interval _5969 = _14424;
        Interval _5970 = _5987.y;
        Interval _5971 = _5988.y;
        Interval _14429 = imul(_5970, _5971, intervalFailed, optical_product_upper);
        Interval _5972 = _14429;
        Interval _14430 = iadd(_5969, _5972, intervalFailed);
        Interval _5973 = _14430;
        Interval _5974 = _5987.z;
        Interval _5975 = _5988.z;
        Interval _14435 = imul(_5974, _5975, intervalFailed, optical_product_upper);
        Interval _5976 = _14435;
        Interval _14436 = iadd(_5973, _5976, intervalFailed);
        Interval _5977 = _14436;
        Interval _5989 = _5977;
        Interval _14438 = imul(_5986, _5989, intervalFailed, optical_product_upper);
        Interval _5990 = _14438;
        Interval _5957 = _5984.x;
        Interval _5958 = _5990;
        Interval _14442 = imul(_5957, _5958, intervalFailed, optical_product_upper);
        Interval _5959 = _14442;
        Interval _5960 = _5984.y;
        Interval _5961 = _5990;
        Interval _14446 = imul(_5960, _5961, intervalFailed, optical_product_upper);
        Interval _5962 = _14446;
        Interval _5963 = _5984.z;
        Interval _5964 = _5990;
        Interval _14450 = imul(_5963, _5964, intervalFailed, optical_product_upper);
        Interval _5965 = _14450;
        _5955.x = _5959;
        _5955.y = _5962;
        _5955.z = _5965;
        Interval3 _5956 = _5955;
        Interval3 _5966 = _5956;
        Interval3 _5991 = _5966;
        Interval3 _5946 = _5983;
        Interval _5947 = _5991.x;
        float _5943 = as_type<float>(as_type<uint>(_5947.hi) ^ 2147483648u);
        float _5944 = as_type<float>(as_type<uint>(_5947.lo) ^ 2147483648u);
        _5941.lo = _5943;
        _5941.hi = _5944;
        Interval _5942 = _5941;
        Interval _5945 = _5942;
        Interval _5948 = _5945;
        Interval _5949 = _5991.y;
        float _5938 = as_type<float>(as_type<uint>(_5949.hi) ^ 2147483648u);
        float _5939 = as_type<float>(as_type<uint>(_5949.lo) ^ 2147483648u);
        _5936.lo = _5938;
        _5936.hi = _5939;
        Interval _5937 = _5936;
        Interval _5940 = _5937;
        Interval _5950 = _5940;
        Interval _5951 = _5991.z;
        float _5933 = as_type<float>(as_type<uint>(_5951.hi) ^ 2147483648u);
        float _5934 = as_type<float>(as_type<uint>(_5951.lo) ^ 2147483648u);
        _5931.lo = _5933;
        _5931.hi = _5934;
        Interval _5932 = _5931;
        Interval _5935 = _5932;
        Interval _5952 = _5935;
        _5929.x = _5948;
        _5929.y = _5950;
        _5929.z = _5952;
        Interval3 _5930 = _5929;
        Interval3 _5953 = _5930;
        Interval _5919 = _5946.x;
        Interval _5920 = _5953.x;
        Interval _14530 = iadd(_5919, _5920, intervalFailed);
        Interval _5921 = _14530;
        Interval _5922 = _5946.y;
        Interval _5923 = _5953.y;
        Interval _14535 = iadd(_5922, _5923, intervalFailed);
        Interval _5924 = _14535;
        Interval _5925 = _5946.z;
        Interval _5926 = _5953.z;
        Interval _14540 = iadd(_5925, _5926, intervalFailed);
        Interval _5927 = _14540;
        _5917.x = _5921;
        _5917.y = _5924;
        _5917.z = _5927;
        Interval3 _5918 = _5917;
        Interval3 _5928 = _5918;
        Interval3 _5954 = _5928;
        Interval3 _5992 = _5954;
        outgoing = _5992;
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
            bool _5913 = false;
            if (param_var_a_22.lo <= 0.0)
            {
                _5913 = param_var_a_22.hi >= 0.0;
            }
            if (_5913)
            {
                _5912 = 0.0;
            }
            else
            {
                _5912 = precise::min(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            }
            float _5914 = _5912;
            float _5915 = precise::max(abs(param_var_a_22.lo), abs(param_var_a_22.hi));
            _5910.lo = _5914;
            _5910.hi = _5915;
            Interval _5911 = _5910;
            Interval _5916 = _5911;
            Interval ay = _5916;
            if (ay.hi < 0.949999988079071044921875)
            {
                Interval3 param_var_a_23 = outgoing;
                float3 param_var_v_3 = float3(0.0, 1.0, 0.0);
                float _5903 = param_var_v_3.x;
                float _5900 = _5903;
                float _5901 = _5903;
                _5898.lo = _5900;
                _5898.hi = _5901;
                Interval _5899 = _5898;
                Interval _5902 = _5899;
                Interval _5904 = _5902;
                float _5905 = param_var_v_3.y;
                float _5895 = _5905;
                float _5896 = _5905;
                _5893.lo = _5895;
                _5893.hi = _5896;
                Interval _5894 = _5893;
                Interval _5897 = _5894;
                Interval _5906 = _5897;
                float _5907 = param_var_v_3.z;
                float _5890 = _5907;
                float _5891 = _5907;
                _5888.lo = _5890;
                _5888.hi = _5891;
                Interval _5889 = _5888;
                Interval _5892 = _5889;
                Interval _5908 = _5892;
                _5886.x = _5904;
                _5886.y = _5906;
                _5886.z = _5908;
                Interval3 _5887 = _5886;
                Interval3 _5909 = _5887;
                Interval3 param_var_b_18 = _5909;
                Interval _5864 = param_var_a_23.y;
                Interval _5865 = param_var_b_18.z;
                Interval _14647 = imul(_5864, _5865, intervalFailed, optical_product_upper);
                Interval _5866 = _14647;
                Interval _5867 = param_var_a_23.z;
                Interval _5868 = param_var_b_18.y;
                Interval _14652 = imul(_5867, _5868, intervalFailed, optical_product_upper);
                Interval _5869 = _14652;
                Interval _5860 = _5866;
                Interval _5861 = _5869;
                float _5857 = as_type<float>(as_type<uint>(_5861.hi) ^ 2147483648u);
                float _5858 = as_type<float>(as_type<uint>(_5861.lo) ^ 2147483648u);
                _5855.lo = _5857;
                _5855.hi = _5858;
                Interval _5856 = _5855;
                Interval _5859 = _5856;
                Interval _5862 = _5859;
                Interval _14672 = iadd(_5860, _5862, intervalFailed);
                Interval _5863 = _14672;
                Interval _5870 = _5863;
                Interval _5871 = param_var_a_23.z;
                Interval _5872 = param_var_b_18.x;
                Interval _14678 = imul(_5871, _5872, intervalFailed, optical_product_upper);
                Interval _5873 = _14678;
                Interval _5874 = param_var_a_23.x;
                Interval _5875 = param_var_b_18.z;
                Interval _14683 = imul(_5874, _5875, intervalFailed, optical_product_upper);
                Interval _5876 = _14683;
                Interval _5851 = _5873;
                Interval _5852 = _5876;
                float _5848 = as_type<float>(as_type<uint>(_5852.hi) ^ 2147483648u);
                float _5849 = as_type<float>(as_type<uint>(_5852.lo) ^ 2147483648u);
                _5846.lo = _5848;
                _5846.hi = _5849;
                Interval _5847 = _5846;
                Interval _5850 = _5847;
                Interval _5853 = _5850;
                Interval _14703 = iadd(_5851, _5853, intervalFailed);
                Interval _5854 = _14703;
                Interval _5877 = _5854;
                Interval _5878 = param_var_a_23.x;
                Interval _5879 = param_var_b_18.y;
                Interval _14709 = imul(_5878, _5879, intervalFailed, optical_product_upper);
                Interval _5880 = _14709;
                Interval _5881 = param_var_a_23.y;
                Interval _5882 = param_var_b_18.x;
                Interval _14714 = imul(_5881, _5882, intervalFailed, optical_product_upper);
                Interval _5883 = _14714;
                Interval _5842 = _5880;
                Interval _5843 = _5883;
                float _5839 = as_type<float>(as_type<uint>(_5843.hi) ^ 2147483648u);
                float _5840 = as_type<float>(as_type<uint>(_5843.lo) ^ 2147483648u);
                _5837.lo = _5839;
                _5837.hi = _5840;
                Interval _5838 = _5837;
                Interval _5841 = _5838;
                Interval _5844 = _5841;
                Interval _14734 = iadd(_5842, _5844, intervalFailed);
                Interval _5845 = _14734;
                Interval _5884 = _5845;
                _5835.x = _5870;
                _5835.y = _5877;
                _5835.z = _5884;
                Interval3 _5836 = _5835;
                Interval3 _5885 = _5836;
                Interval3 param_var_a_24 = _5885;
                Interval3 _5828 = param_var_a_24;
                float _5829 = 1.0;
                float _5825 = _5829;
                float _5826 = _5829;
                _5823.lo = _5825;
                _5823.hi = _5826;
                Interval _5824 = _5823;
                Interval _5827 = _5824;
                Interval _5830 = _5827;
                Interval3 _5831 = param_var_a_24;
                Interval _5814 = _5831.x;
                bool _5807 = false;
                if (_5814.lo <= 0.0)
                {
                    _5807 = _5814.hi >= 0.0;
                }
                if (_5807)
                {
                    _5806 = 0.0;
                }
                else
                {
                    _5806 = precise::min(abs(_5814.lo), abs(_5814.hi));
                }
                float _5805 = _5806;
                float _5808 = precise::max(abs(_5814.lo), abs(_5814.hi));
                float _5809 = spvFMul(_5805, _5805);
                float _14787 = interval_down(_5809, intervalFailed);
                float _5810 = precise::max(0.0, _14787);
                float _5811 = spvFMul(_5808, _5808);
                float _14791 = interval_up(_5811, intervalFailed);
                float _5812 = _14791;
                _5803.lo = _5810;
                _5803.hi = _5812;
                Interval _5804 = _5803;
                Interval _5813 = _5804;
                Interval _5815 = _5813;
                Interval _5816 = _5831.y;
                bool _5796 = false;
                if (_5816.lo <= 0.0)
                {
                    _5796 = _5816.hi >= 0.0;
                }
                if (_5796)
                {
                    _5795 = 0.0;
                }
                else
                {
                    _5795 = precise::min(abs(_5816.lo), abs(_5816.hi));
                }
                float _5794 = _5795;
                float _5797 = precise::max(abs(_5816.lo), abs(_5816.hi));
                float _5798 = spvFMul(_5794, _5794);
                float _14830 = interval_down(_5798, intervalFailed);
                float _5799 = precise::max(0.0, _14830);
                float _5800 = spvFMul(_5797, _5797);
                float _14834 = interval_up(_5800, intervalFailed);
                float _5801 = _14834;
                _5792.lo = _5799;
                _5792.hi = _5801;
                Interval _5793 = _5792;
                Interval _5802 = _5793;
                Interval _5817 = _5802;
                Interval _14842 = iadd(_5815, _5817, intervalFailed);
                Interval _5818 = _14842;
                Interval _5819 = _5831.z;
                bool _5785 = false;
                if (_5819.lo <= 0.0)
                {
                    _5785 = _5819.hi >= 0.0;
                }
                if (_5785)
                {
                    _5784 = 0.0;
                }
                else
                {
                    _5784 = precise::min(abs(_5819.lo), abs(_5819.hi));
                }
                float _5783 = _5784;
                float _5786 = precise::max(abs(_5819.lo), abs(_5819.hi));
                float _5787 = spvFMul(_5783, _5783);
                float _14874 = interval_down(_5787, intervalFailed);
                float _5788 = precise::max(0.0, _14874);
                float _5789 = spvFMul(_5786, _5786);
                float _14878 = interval_up(_5789, intervalFailed);
                float _5790 = _14878;
                _5781.lo = _5788;
                _5781.hi = _5790;
                Interval _5782 = _5781;
                Interval _5791 = _5782;
                Interval _5820 = _5791;
                Interval _14886 = iadd(_5818, _5820, intervalFailed);
                Interval _5821 = _14886;
                Interval _14887 = isqrt(_5821, intervalFailed);
                Interval _5822 = _14887;
                Interval _5832 = _5822;
                Interval _14889 = idiv(_5830, _5832, intervalFailed, interval_divide_upper);
                Interval _5833 = _14889;
                Interval _5771 = _5828.x;
                Interval _5772 = _5833;
                Interval _14893 = imul(_5771, _5772, intervalFailed, optical_product_upper);
                Interval _5773 = _14893;
                Interval _5774 = _5828.y;
                Interval _5775 = _5833;
                Interval _14897 = imul(_5774, _5775, intervalFailed, optical_product_upper);
                Interval _5776 = _14897;
                Interval _5777 = _5828.z;
                Interval _5778 = _5833;
                Interval _14901 = imul(_5777, _5778, intervalFailed, optical_product_upper);
                Interval _5779 = _14901;
                _5769.x = _5773;
                _5769.y = _5776;
                _5769.z = _5779;
                Interval3 _5770 = _5769;
                Interval3 _5780 = _5770;
                Interval3 _5834 = _5780;
                t = _5834;
            }
            else
            {
                if (ay.lo >= 0.949999988079071044921875)
                {
                    Interval3 param_var_a_25 = outgoing;
                    float3 param_var_v_4 = float3(1.0, 0.0, 0.0);
                    float _5762 = param_var_v_4.x;
                    float _5759 = _5762;
                    float _5760 = _5762;
                    _5757.lo = _5759;
                    _5757.hi = _5760;
                    Interval _5758 = _5757;
                    Interval _5761 = _5758;
                    Interval _5763 = _5761;
                    float _5764 = param_var_v_4.y;
                    float _5754 = _5764;
                    float _5755 = _5764;
                    _5752.lo = _5754;
                    _5752.hi = _5755;
                    Interval _5753 = _5752;
                    Interval _5756 = _5753;
                    Interval _5765 = _5756;
                    float _5766 = param_var_v_4.z;
                    float _5749 = _5766;
                    float _5750 = _5766;
                    _5747.lo = _5749;
                    _5747.hi = _5750;
                    Interval _5748 = _5747;
                    Interval _5751 = _5748;
                    Interval _5767 = _5751;
                    _5745.x = _5763;
                    _5745.y = _5765;
                    _5745.z = _5767;
                    Interval3 _5746 = _5745;
                    Interval3 _5768 = _5746;
                    Interval3 param_var_b_19 = _5768;
                    Interval _5723 = param_var_a_25.y;
                    Interval _5724 = param_var_b_19.z;
                    Interval _14962 = imul(_5723, _5724, intervalFailed, optical_product_upper);
                    Interval _5725 = _14962;
                    Interval _5726 = param_var_a_25.z;
                    Interval _5727 = param_var_b_19.y;
                    Interval _14967 = imul(_5726, _5727, intervalFailed, optical_product_upper);
                    Interval _5728 = _14967;
                    Interval _5719 = _5725;
                    Interval _5720 = _5728;
                    float _5716 = as_type<float>(as_type<uint>(_5720.hi) ^ 2147483648u);
                    float _5717 = as_type<float>(as_type<uint>(_5720.lo) ^ 2147483648u);
                    _5714.lo = _5716;
                    _5714.hi = _5717;
                    Interval _5715 = _5714;
                    Interval _5718 = _5715;
                    Interval _5721 = _5718;
                    Interval _14987 = iadd(_5719, _5721, intervalFailed);
                    Interval _5722 = _14987;
                    Interval _5729 = _5722;
                    Interval _5730 = param_var_a_25.z;
                    Interval _5731 = param_var_b_19.x;
                    Interval _14993 = imul(_5730, _5731, intervalFailed, optical_product_upper);
                    Interval _5732 = _14993;
                    Interval _5733 = param_var_a_25.x;
                    Interval _5734 = param_var_b_19.z;
                    Interval _14998 = imul(_5733, _5734, intervalFailed, optical_product_upper);
                    Interval _5735 = _14998;
                    Interval _5710 = _5732;
                    Interval _5711 = _5735;
                    float _5707 = as_type<float>(as_type<uint>(_5711.hi) ^ 2147483648u);
                    float _5708 = as_type<float>(as_type<uint>(_5711.lo) ^ 2147483648u);
                    _5705.lo = _5707;
                    _5705.hi = _5708;
                    Interval _5706 = _5705;
                    Interval _5709 = _5706;
                    Interval _5712 = _5709;
                    Interval _15018 = iadd(_5710, _5712, intervalFailed);
                    Interval _5713 = _15018;
                    Interval _5736 = _5713;
                    Interval _5737 = param_var_a_25.x;
                    Interval _5738 = param_var_b_19.y;
                    Interval _15024 = imul(_5737, _5738, intervalFailed, optical_product_upper);
                    Interval _5739 = _15024;
                    Interval _5740 = param_var_a_25.y;
                    Interval _5741 = param_var_b_19.x;
                    Interval _15029 = imul(_5740, _5741, intervalFailed, optical_product_upper);
                    Interval _5742 = _15029;
                    Interval _5701 = _5739;
                    Interval _5702 = _5742;
                    float _5698 = as_type<float>(as_type<uint>(_5702.hi) ^ 2147483648u);
                    float _5699 = as_type<float>(as_type<uint>(_5702.lo) ^ 2147483648u);
                    _5696.lo = _5698;
                    _5696.hi = _5699;
                    Interval _5697 = _5696;
                    Interval _5700 = _5697;
                    Interval _5703 = _5700;
                    Interval _15049 = iadd(_5701, _5703, intervalFailed);
                    Interval _5704 = _15049;
                    Interval _5743 = _5704;
                    _5694.x = _5729;
                    _5694.y = _5736;
                    _5694.z = _5743;
                    Interval3 _5695 = _5694;
                    Interval3 _5744 = _5695;
                    Interval3 param_var_a_26 = _5744;
                    Interval3 _5687 = param_var_a_26;
                    float _5688 = 1.0;
                    float _5684 = _5688;
                    float _5685 = _5688;
                    _5682.lo = _5684;
                    _5682.hi = _5685;
                    Interval _5683 = _5682;
                    Interval _5686 = _5683;
                    Interval _5689 = _5686;
                    Interval3 _5690 = param_var_a_26;
                    Interval _5673 = _5690.x;
                    bool _5666 = false;
                    if (_5673.lo <= 0.0)
                    {
                        _5666 = _5673.hi >= 0.0;
                    }
                    if (_5666)
                    {
                        _5665 = 0.0;
                    }
                    else
                    {
                        _5665 = precise::min(abs(_5673.lo), abs(_5673.hi));
                    }
                    float _5664 = _5665;
                    float _5667 = precise::max(abs(_5673.lo), abs(_5673.hi));
                    float _5668 = spvFMul(_5664, _5664);
                    float _15102 = interval_down(_5668, intervalFailed);
                    float _5669 = precise::max(0.0, _15102);
                    float _5670 = spvFMul(_5667, _5667);
                    float _15106 = interval_up(_5670, intervalFailed);
                    float _5671 = _15106;
                    _5662.lo = _5669;
                    _5662.hi = _5671;
                    Interval _5663 = _5662;
                    Interval _5672 = _5663;
                    Interval _5674 = _5672;
                    Interval _5675 = _5690.y;
                    bool _5655 = false;
                    if (_5675.lo <= 0.0)
                    {
                        _5655 = _5675.hi >= 0.0;
                    }
                    if (_5655)
                    {
                        _5654 = 0.0;
                    }
                    else
                    {
                        _5654 = precise::min(abs(_5675.lo), abs(_5675.hi));
                    }
                    float _5653 = _5654;
                    float _5656 = precise::max(abs(_5675.lo), abs(_5675.hi));
                    float _5657 = spvFMul(_5653, _5653);
                    float _15145 = interval_down(_5657, intervalFailed);
                    float _5658 = precise::max(0.0, _15145);
                    float _5659 = spvFMul(_5656, _5656);
                    float _15149 = interval_up(_5659, intervalFailed);
                    float _5660 = _15149;
                    _5651.lo = _5658;
                    _5651.hi = _5660;
                    Interval _5652 = _5651;
                    Interval _5661 = _5652;
                    Interval _5676 = _5661;
                    Interval _15157 = iadd(_5674, _5676, intervalFailed);
                    Interval _5677 = _15157;
                    Interval _5678 = _5690.z;
                    bool _5644 = false;
                    if (_5678.lo <= 0.0)
                    {
                        _5644 = _5678.hi >= 0.0;
                    }
                    if (_5644)
                    {
                        _5643 = 0.0;
                    }
                    else
                    {
                        _5643 = precise::min(abs(_5678.lo), abs(_5678.hi));
                    }
                    float _5642 = _5643;
                    float _5645 = precise::max(abs(_5678.lo), abs(_5678.hi));
                    float _5646 = spvFMul(_5642, _5642);
                    float _15189 = interval_down(_5646, intervalFailed);
                    float _5647 = precise::max(0.0, _15189);
                    float _5648 = spvFMul(_5645, _5645);
                    float _15193 = interval_up(_5648, intervalFailed);
                    float _5649 = _15193;
                    _5640.lo = _5647;
                    _5640.hi = _5649;
                    Interval _5641 = _5640;
                    Interval _5650 = _5641;
                    Interval _5679 = _5650;
                    Interval _15201 = iadd(_5677, _5679, intervalFailed);
                    Interval _5680 = _15201;
                    Interval _15202 = isqrt(_5680, intervalFailed);
                    Interval _5681 = _15202;
                    Interval _5691 = _5681;
                    Interval _15204 = idiv(_5689, _5691, intervalFailed, interval_divide_upper);
                    Interval _5692 = _15204;
                    Interval _5630 = _5687.x;
                    Interval _5631 = _5692;
                    Interval _15208 = imul(_5630, _5631, intervalFailed, optical_product_upper);
                    Interval _5632 = _15208;
                    Interval _5633 = _5687.y;
                    Interval _5634 = _5692;
                    Interval _15212 = imul(_5633, _5634, intervalFailed, optical_product_upper);
                    Interval _5635 = _15212;
                    Interval _5636 = _5687.z;
                    Interval _5637 = _5692;
                    Interval _15216 = imul(_5636, _5637, intervalFailed, optical_product_upper);
                    Interval _5638 = _15216;
                    _5628.x = _5632;
                    _5628.y = _5635;
                    _5628.z = _5638;
                    Interval3 _5629 = _5628;
                    Interval3 _5639 = _5629;
                    Interval3 _5693 = _5639;
                    t = _5693;
                }
                else
                {
                    intervalFailed = true;
                    return false;
                }
            }
            int2 tap = _2325[uint(receiver.settings.y)];
            Interval3 param_var_a_27 = outgoing;
            Interval3 param_var_a_28 = t;
            float param_var_n_5 = float(tap.x);
            float param_var_d_3 = 1000.0;
            Interval _15238 = iratio(param_var_n_5, param_var_d_3, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_20 = _15238;
            Interval _5618 = param_var_a_28.x;
            Interval _5619 = param_var_b_20;
            Interval _15242 = imul(_5618, _5619, intervalFailed, optical_product_upper);
            Interval _5620 = _15242;
            Interval _5621 = param_var_a_28.y;
            Interval _5622 = param_var_b_20;
            Interval _15246 = imul(_5621, _5622, intervalFailed, optical_product_upper);
            Interval _5623 = _15246;
            Interval _5624 = param_var_a_28.z;
            Interval _5625 = param_var_b_20;
            Interval _15250 = imul(_5624, _5625, intervalFailed, optical_product_upper);
            Interval _5626 = _15250;
            _5616.x = _5620;
            _5616.y = _5623;
            _5616.z = _5626;
            Interval3 _5617 = _5616;
            Interval3 _5627 = _5617;
            Interval3 param_var_a_29 = _5627;
            Interval3 param_var_a_30 = outgoing;
            Interval3 param_var_b_21 = t;
            Interval _5594 = param_var_a_30.y;
            Interval _5595 = param_var_b_21.z;
            Interval _15266 = imul(_5594, _5595, intervalFailed, optical_product_upper);
            Interval _5596 = _15266;
            Interval _5597 = param_var_a_30.z;
            Interval _5598 = param_var_b_21.y;
            Interval _15271 = imul(_5597, _5598, intervalFailed, optical_product_upper);
            Interval _5599 = _15271;
            Interval _5590 = _5596;
            Interval _5591 = _5599;
            float _5587 = as_type<float>(as_type<uint>(_5591.hi) ^ 2147483648u);
            float _5588 = as_type<float>(as_type<uint>(_5591.lo) ^ 2147483648u);
            _5585.lo = _5587;
            _5585.hi = _5588;
            Interval _5586 = _5585;
            Interval _5589 = _5586;
            Interval _5592 = _5589;
            Interval _15291 = iadd(_5590, _5592, intervalFailed);
            Interval _5593 = _15291;
            Interval _5600 = _5593;
            Interval _5601 = param_var_a_30.z;
            Interval _5602 = param_var_b_21.x;
            Interval _15297 = imul(_5601, _5602, intervalFailed, optical_product_upper);
            Interval _5603 = _15297;
            Interval _5604 = param_var_a_30.x;
            Interval _5605 = param_var_b_21.z;
            Interval _15302 = imul(_5604, _5605, intervalFailed, optical_product_upper);
            Interval _5606 = _15302;
            Interval _5581 = _5603;
            Interval _5582 = _5606;
            float _5578 = as_type<float>(as_type<uint>(_5582.hi) ^ 2147483648u);
            float _5579 = as_type<float>(as_type<uint>(_5582.lo) ^ 2147483648u);
            _5576.lo = _5578;
            _5576.hi = _5579;
            Interval _5577 = _5576;
            Interval _5580 = _5577;
            Interval _5583 = _5580;
            Interval _15322 = iadd(_5581, _5583, intervalFailed);
            Interval _5584 = _15322;
            Interval _5607 = _5584;
            Interval _5608 = param_var_a_30.x;
            Interval _5609 = param_var_b_21.y;
            Interval _15328 = imul(_5608, _5609, intervalFailed, optical_product_upper);
            Interval _5610 = _15328;
            Interval _5611 = param_var_a_30.y;
            Interval _5612 = param_var_b_21.x;
            Interval _15333 = imul(_5611, _5612, intervalFailed, optical_product_upper);
            Interval _5613 = _15333;
            Interval _5572 = _5610;
            Interval _5573 = _5613;
            float _5569 = as_type<float>(as_type<uint>(_5573.hi) ^ 2147483648u);
            float _5570 = as_type<float>(as_type<uint>(_5573.lo) ^ 2147483648u);
            _5567.lo = _5569;
            _5567.hi = _5570;
            Interval _5568 = _5567;
            Interval _5571 = _5568;
            Interval _5574 = _5571;
            Interval _15353 = iadd(_5572, _5574, intervalFailed);
            Interval _5575 = _15353;
            Interval _5614 = _5575;
            _5565.x = _5600;
            _5565.y = _5607;
            _5565.z = _5614;
            Interval3 _5566 = _5565;
            Interval3 _5615 = _5566;
            Interval3 param_var_a_31 = _5615;
            float param_var_n_6 = float(tap.y);
            float param_var_d_4 = 1000.0;
            Interval _15367 = iratio(param_var_n_6, param_var_d_4, intervalFailed, optical_product_upper, interval_divide_upper);
            Interval param_var_b_22 = _15367;
            Interval _5555 = param_var_a_31.x;
            Interval _5556 = param_var_b_22;
            Interval _15371 = imul(_5555, _5556, intervalFailed, optical_product_upper);
            Interval _5557 = _15371;
            Interval _5558 = param_var_a_31.y;
            Interval _5559 = param_var_b_22;
            Interval _15375 = imul(_5558, _5559, intervalFailed, optical_product_upper);
            Interval _5560 = _15375;
            Interval _5561 = param_var_a_31.z;
            Interval _5562 = param_var_b_22;
            Interval _15379 = imul(_5561, _5562, intervalFailed, optical_product_upper);
            Interval _5563 = _15379;
            _5553.x = _5557;
            _5553.y = _5560;
            _5553.z = _5563;
            Interval3 _5554 = _5553;
            Interval3 _5564 = _5554;
            Interval3 param_var_b_23 = _5564;
            Interval _5543 = param_var_a_29.x;
            Interval _5544 = param_var_b_23.x;
            Interval _15393 = iadd(_5543, _5544, intervalFailed);
            Interval _5545 = _15393;
            Interval _5546 = param_var_a_29.y;
            Interval _5547 = param_var_b_23.y;
            Interval _15398 = iadd(_5546, _5547, intervalFailed);
            Interval _5548 = _15398;
            Interval _5549 = param_var_a_29.z;
            Interval _5550 = param_var_b_23.z;
            Interval _15403 = iadd(_5549, _5550, intervalFailed);
            Interval _5551 = _15403;
            _5541.x = _5545;
            _5541.y = _5548;
            _5541.z = _5551;
            Interval3 _5542 = _5541;
            Interval3 _5552 = _5542;
            Interval3 param_var_a_32 = _5552;
            float param_var_x_8 = receiver.settings.x;
            float _5538 = param_var_x_8;
            float _5539 = param_var_x_8;
            _5536.lo = _5538;
            _5536.hi = _5539;
            Interval _5537 = _5536;
            Interval _5540 = _5537;
            Interval param_var_a_33 = _5540;
            bool _5529 = false;
            if (param_var_a_33.lo <= 0.0)
            {
                _5529 = param_var_a_33.hi >= 0.0;
            }
            if (_5529)
            {
                _5528 = 0.0;
            }
            else
            {
                _5528 = precise::min(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            }
            float _5527 = _5528;
            float _5530 = precise::max(abs(param_var_a_33.lo), abs(param_var_a_33.hi));
            float _5531 = spvFMul(_5527, _5527);
            float _15454 = interval_down(_5531, intervalFailed);
            float _5532 = precise::max(0.0, _15454);
            float _5533 = spvFMul(_5530, _5530);
            float _15458 = interval_up(_5533, intervalFailed);
            float _5534 = _15458;
            _5525.lo = _5532;
            _5525.hi = _5534;
            Interval _5526 = _5525;
            Interval _5535 = _5526;
            Interval param_var_b_24 = _5535;
            Interval _5515 = param_var_a_32.x;
            Interval _5516 = param_var_b_24;
            Interval _15469 = imul(_5515, _5516, intervalFailed, optical_product_upper);
            Interval _5517 = _15469;
            Interval _5518 = param_var_a_32.y;
            Interval _5519 = param_var_b_24;
            Interval _15473 = imul(_5518, _5519, intervalFailed, optical_product_upper);
            Interval _5520 = _15473;
            Interval _5521 = param_var_a_32.z;
            Interval _5522 = param_var_b_24;
            Interval _15477 = imul(_5521, _5522, intervalFailed, optical_product_upper);
            Interval _5523 = _15477;
            _5513.x = _5517;
            _5513.y = _5520;
            _5513.z = _5523;
            Interval3 _5514 = _5513;
            Interval3 _5524 = _5514;
            Interval3 param_var_b_25 = _5524;
            Interval _5503 = param_var_a_27.x;
            Interval _5504 = param_var_b_25.x;
            Interval _15491 = iadd(_5503, _5504, intervalFailed);
            Interval _5505 = _15491;
            Interval _5506 = param_var_a_27.y;
            Interval _5507 = param_var_b_25.y;
            Interval _15496 = iadd(_5506, _5507, intervalFailed);
            Interval _5508 = _15496;
            Interval _5509 = param_var_a_27.z;
            Interval _5510 = param_var_b_25.z;
            Interval _15501 = iadd(_5509, _5510, intervalFailed);
            Interval _5511 = _15501;
            _5501.x = _5505;
            _5501.y = _5508;
            _5501.z = _5511;
            Interval3 _5502 = _5501;
            Interval3 _5512 = _5502;
            Interval3 param_var_a_34 = _5512;
            Interval3 _5494 = param_var_a_34;
            float _5495 = 1.0;
            float _5491 = _5495;
            float _5492 = _5495;
            _5489.lo = _5491;
            _5489.hi = _5492;
            Interval _5490 = _5489;
            Interval _5493 = _5490;
            Interval _5496 = _5493;
            Interval3 _5497 = param_var_a_34;
            Interval _5480 = _5497.x;
            bool _5473 = false;
            if (_5480.lo <= 0.0)
            {
                _5473 = _5480.hi >= 0.0;
            }
            if (_5473)
            {
                _5472 = 0.0;
            }
            else
            {
                _5472 = precise::min(abs(_5480.lo), abs(_5480.hi));
            }
            float _5471 = _5472;
            float _5474 = precise::max(abs(_5480.lo), abs(_5480.hi));
            float _5475 = spvFMul(_5471, _5471);
            float _15553 = interval_down(_5475, intervalFailed);
            float _5476 = precise::max(0.0, _15553);
            float _5477 = spvFMul(_5474, _5474);
            float _15557 = interval_up(_5477, intervalFailed);
            float _5478 = _15557;
            _5469.lo = _5476;
            _5469.hi = _5478;
            Interval _5470 = _5469;
            Interval _5479 = _5470;
            Interval _5481 = _5479;
            Interval _5482 = _5497.y;
            bool _5462 = false;
            if (_5482.lo <= 0.0)
            {
                _5462 = _5482.hi >= 0.0;
            }
            if (_5462)
            {
                _5461 = 0.0;
            }
            else
            {
                _5461 = precise::min(abs(_5482.lo), abs(_5482.hi));
            }
            float _5460 = _5461;
            float _5463 = precise::max(abs(_5482.lo), abs(_5482.hi));
            float _5464 = spvFMul(_5460, _5460);
            float _15596 = interval_down(_5464, intervalFailed);
            float _5465 = precise::max(0.0, _15596);
            float _5466 = spvFMul(_5463, _5463);
            float _15600 = interval_up(_5466, intervalFailed);
            float _5467 = _15600;
            _5458.lo = _5465;
            _5458.hi = _5467;
            Interval _5459 = _5458;
            Interval _5468 = _5459;
            Interval _5483 = _5468;
            Interval _15608 = iadd(_5481, _5483, intervalFailed);
            Interval _5484 = _15608;
            Interval _5485 = _5497.z;
            bool _5451 = false;
            if (_5485.lo <= 0.0)
            {
                _5451 = _5485.hi >= 0.0;
            }
            if (_5451)
            {
                _5450 = 0.0;
            }
            else
            {
                _5450 = precise::min(abs(_5485.lo), abs(_5485.hi));
            }
            float _5449 = _5450;
            float _5452 = precise::max(abs(_5485.lo), abs(_5485.hi));
            float _5453 = spvFMul(_5449, _5449);
            float _15640 = interval_down(_5453, intervalFailed);
            float _5454 = precise::max(0.0, _15640);
            float _5455 = spvFMul(_5452, _5452);
            float _15644 = interval_up(_5455, intervalFailed);
            float _5456 = _15644;
            _5447.lo = _5454;
            _5447.hi = _5456;
            Interval _5448 = _5447;
            Interval _5457 = _5448;
            Interval _5486 = _5457;
            Interval _15652 = iadd(_5484, _5486, intervalFailed);
            Interval _5487 = _15652;
            Interval _15653 = isqrt(_5487, intervalFailed);
            Interval _5488 = _15653;
            Interval _5498 = _5488;
            Interval _15655 = idiv(_5496, _5498, intervalFailed, interval_divide_upper);
            Interval _5499 = _15655;
            Interval _5437 = _5494.x;
            Interval _5438 = _5499;
            Interval _15659 = imul(_5437, _5438, intervalFailed, optical_product_upper);
            Interval _5439 = _15659;
            Interval _5440 = _5494.y;
            Interval _5441 = _5499;
            Interval _15663 = imul(_5440, _5441, intervalFailed, optical_product_upper);
            Interval _5442 = _15663;
            Interval _5443 = _5494.z;
            Interval _5444 = _5499;
            Interval _15667 = imul(_5443, _5444, intervalFailed, optical_product_upper);
            Interval _5445 = _15667;
            _5435.x = _5439;
            _5435.y = _5442;
            _5435.z = _5445;
            Interval3 _5436 = _5435;
            Interval3 _5446 = _5436;
            Interval3 _5500 = _5446;
            Interval3 rough = _5500;
            Interval3 param_var_a_35 = rough;
            Interval3 param_var_b_26 = n;
            Interval _5424 = param_var_a_35.x;
            Interval _5425 = param_var_b_26.x;
            Interval _15684 = imul(_5424, _5425, intervalFailed, optical_product_upper);
            Interval _5426 = _15684;
            Interval _5427 = param_var_a_35.y;
            Interval _5428 = param_var_b_26.y;
            Interval _15689 = imul(_5427, _5428, intervalFailed, optical_product_upper);
            Interval _5429 = _15689;
            Interval _15690 = iadd(_5426, _5429, intervalFailed);
            Interval _5430 = _15690;
            Interval _5431 = param_var_a_35.z;
            Interval _5432 = param_var_b_26.z;
            Interval _15695 = imul(_5431, _5432, intervalFailed, optical_product_upper);
            Interval _5433 = _15695;
            Interval _15696 = iadd(_5430, _5433, intervalFailed);
            Interval _5434 = _15696;
            Interval nd = _5434;
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
                    Interval _5414 = param_var_a_36.x;
                    Interval _5415 = param_var_b_27.x;
                    float _5411 = precise::min(_5414.lo, _5415.lo);
                    float _5412 = precise::max(_5414.hi, _5415.hi);
                    _5409.lo = _5411;
                    _5409.hi = _5412;
                    Interval _5410 = _5409;
                    Interval _5413 = _5410;
                    Interval _5416 = _5413;
                    Interval _5417 = param_var_a_36.y;
                    Interval _5418 = param_var_b_27.y;
                    float _5406 = precise::min(_5417.lo, _5418.lo);
                    float _5407 = precise::max(_5417.hi, _5418.hi);
                    _5404.lo = _5406;
                    _5404.hi = _5407;
                    Interval _5405 = _5404;
                    Interval _5408 = _5405;
                    Interval _5419 = _5408;
                    Interval _5420 = param_var_a_36.z;
                    Interval _5421 = param_var_b_27.z;
                    float _5401 = precise::min(_5420.lo, _5421.lo);
                    float _5402 = precise::max(_5420.hi, _5421.hi);
                    _5399.lo = _5401;
                    _5399.hi = _5402;
                    Interval _5400 = _5399;
                    Interval _5403 = _5400;
                    Interval _5422 = _5403;
                    _5397.x = _5416;
                    _5397.y = _5419;
                    _5397.z = _5422;
                    Interval3 _5398 = _5397;
                    Interval3 _5423 = _5398;
                    outgoing = _5423;
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
        Interval3 _5388 = param_var_a_37;
        Interval _5389 = param_var_b_28.x;
        float _5385 = as_type<float>(as_type<uint>(_5389.hi) ^ 2147483648u);
        float _5386 = as_type<float>(as_type<uint>(_5389.lo) ^ 2147483648u);
        Interval _5383;
        _5383.lo = _5385;
        _5383.hi = _5386;
        Interval _5384 = _5383;
        Interval _5387 = _5384;
        Interval _5390 = _5387;
        Interval _5391 = param_var_b_28.y;
        float _5380 = as_type<float>(as_type<uint>(_5391.hi) ^ 2147483648u);
        float _5381 = as_type<float>(as_type<uint>(_5391.lo) ^ 2147483648u);
        Interval _5378;
        _5378.lo = _5380;
        _5378.hi = _5381;
        Interval _5379 = _5378;
        Interval _5382 = _5379;
        Interval _5392 = _5382;
        Interval _5393 = param_var_b_28.z;
        float _5375 = as_type<float>(as_type<uint>(_5393.hi) ^ 2147483648u);
        float _5376 = as_type<float>(as_type<uint>(_5393.lo) ^ 2147483648u);
        Interval _5373;
        _5373.lo = _5375;
        _5373.hi = _5376;
        Interval _5374 = _5373;
        Interval _5377 = _5374;
        Interval _5394 = _5377;
        Interval3 _5371;
        _5371.x = _5390;
        _5371.y = _5392;
        _5371.z = _5394;
        Interval3 _5372 = _5371;
        Interval3 _5395 = _5372;
        Interval _5361 = _5388.x;
        Interval _5362 = _5395.x;
        Interval _15855 = iadd(_5361, _5362, intervalFailed);
        Interval _5363 = _15855;
        Interval _5364 = _5388.y;
        Interval _5365 = _5395.y;
        Interval _15860 = iadd(_5364, _5365, intervalFailed);
        Interval _5366 = _15860;
        Interval _5367 = _5388.z;
        Interval _5368 = _5395.z;
        Interval _15865 = iadd(_5367, _5368, intervalFailed);
        Interval _5369 = _15865;
        Interval3 _5359;
        _5359.x = _5363;
        _5359.y = _5366;
        _5359.z = _5369;
        Interval3 _5360 = _5359;
        Interval3 _5370 = _5360;
        Interval3 _5396 = _5370;
        travel = _5396;
    }
    Interval3 param_var_a_38 = travel;
    Interval _5350 = param_var_a_38.x;
    bool _5343 = false;
    if (_5350.lo <= 0.0)
    {
        _5343 = _5350.hi >= 0.0;
    }
    float _5342;
    if (_5343)
    {
        _5342 = 0.0;
    }
    else
    {
        _5342 = precise::min(abs(_5350.lo), abs(_5350.hi));
    }
    float _5341 = _5342;
    float _5344 = precise::max(abs(_5350.lo), abs(_5350.hi));
    float _5345 = spvFMul(_5341, _5341);
    float _15908 = interval_down(_5345, intervalFailed);
    float _5346 = precise::max(0.0, _15908);
    float _5347 = spvFMul(_5344, _5344);
    float _15912 = interval_up(_5347, intervalFailed);
    float _5348 = _15912;
    Interval _5339;
    _5339.lo = _5346;
    _5339.hi = _5348;
    Interval _5340 = _5339;
    Interval _5349 = _5340;
    Interval _5351 = _5349;
    Interval _5352 = param_var_a_38.y;
    bool _5332 = false;
    if (_5352.lo <= 0.0)
    {
        _5332 = _5352.hi >= 0.0;
    }
    float _5331;
    if (_5332)
    {
        _5331 = 0.0;
    }
    else
    {
        _5331 = precise::min(abs(_5352.lo), abs(_5352.hi));
    }
    float _5330 = _5331;
    float _5333 = precise::max(abs(_5352.lo), abs(_5352.hi));
    float _5334 = spvFMul(_5330, _5330);
    float _15951 = interval_down(_5334, intervalFailed);
    float _5335 = precise::max(0.0, _15951);
    float _5336 = spvFMul(_5333, _5333);
    float _15955 = interval_up(_5336, intervalFailed);
    float _5337 = _15955;
    Interval _5328;
    _5328.lo = _5335;
    _5328.hi = _5337;
    Interval _5329 = _5328;
    Interval _5338 = _5329;
    Interval _5353 = _5338;
    Interval _15963 = iadd(_5351, _5353, intervalFailed);
    Interval _5354 = _15963;
    Interval _5355 = param_var_a_38.z;
    bool _5321 = false;
    if (_5355.lo <= 0.0)
    {
        _5321 = _5355.hi >= 0.0;
    }
    float _5320;
    if (_5321)
    {
        _5320 = 0.0;
    }
    else
    {
        _5320 = precise::min(abs(_5355.lo), abs(_5355.hi));
    }
    float _5319 = _5320;
    float _5322 = precise::max(abs(_5355.lo), abs(_5355.hi));
    float _5323 = spvFMul(_5319, _5319);
    float _15995 = interval_down(_5323, intervalFailed);
    float _5324 = precise::max(0.0, _15995);
    float _5325 = spvFMul(_5322, _5322);
    float _15999 = interval_up(_5325, intervalFailed);
    float _5326 = _15999;
    Interval _5317;
    _5317.lo = _5324;
    _5317.hi = _5326;
    Interval _5318 = _5317;
    Interval _5327 = _5318;
    Interval _5356 = _5327;
    Interval _16007 = iadd(_5354, _5356, intervalFailed);
    Interval _5357 = _16007;
    Interval _16008 = isqrt(_5357, intervalFailed);
    Interval _5358 = _16008;
    Interval len = _5358;
    Interval3 param_var_a_39 = travel;
    Interval3 param_var_b_29 = outgoing;
    Interval _5306 = param_var_a_39.x;
    Interval _5307 = param_var_b_29.x;
    Interval _16016 = imul(_5306, _5307, intervalFailed, optical_product_upper);
    Interval _5308 = _16016;
    Interval _5309 = param_var_a_39.y;
    Interval _5310 = param_var_b_29.y;
    Interval _16021 = imul(_5309, _5310, intervalFailed, optical_product_upper);
    Interval _5311 = _16021;
    Interval _16022 = iadd(_5308, _5311, intervalFailed);
    Interval _5312 = _16022;
    Interval _5313 = param_var_a_39.z;
    Interval _5314 = param_var_b_29.z;
    Interval _16027 = imul(_5313, _5314, intervalFailed, optical_product_upper);
    Interval _5315 = _16027;
    Interval _16028 = iadd(_5312, _5315, intervalFailed);
    Interval _5316 = _16028;
    Interval temp_var_Interval = _5316;
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
    Interval _5284 = param_var_a_40.y;
    Interval _5285 = param_var_b_30.z;
    Interval _16045 = imul(_5284, _5285, intervalFailed, optical_product_upper);
    Interval _5286 = _16045;
    Interval _5287 = param_var_a_40.z;
    Interval _5288 = param_var_b_30.y;
    Interval _16050 = imul(_5287, _5288, intervalFailed, optical_product_upper);
    Interval _5289 = _16050;
    Interval _5280 = _5286;
    Interval _5281 = _5289;
    float _5277 = as_type<float>(as_type<uint>(_5281.hi) ^ 2147483648u);
    float _5278 = as_type<float>(as_type<uint>(_5281.lo) ^ 2147483648u);
    Interval _5275;
    _5275.lo = _5277;
    _5275.hi = _5278;
    Interval _5276 = _5275;
    Interval _5279 = _5276;
    Interval _5282 = _5279;
    Interval _16070 = iadd(_5280, _5282, intervalFailed);
    Interval _5283 = _16070;
    Interval _5290 = _5283;
    Interval _5291 = param_var_a_40.z;
    Interval _5292 = param_var_b_30.x;
    Interval _16076 = imul(_5291, _5292, intervalFailed, optical_product_upper);
    Interval _5293 = _16076;
    Interval _5294 = param_var_a_40.x;
    Interval _5295 = param_var_b_30.z;
    Interval _16081 = imul(_5294, _5295, intervalFailed, optical_product_upper);
    Interval _5296 = _16081;
    Interval _5271 = _5293;
    Interval _5272 = _5296;
    float _5268 = as_type<float>(as_type<uint>(_5272.hi) ^ 2147483648u);
    float _5269 = as_type<float>(as_type<uint>(_5272.lo) ^ 2147483648u);
    Interval _5266;
    _5266.lo = _5268;
    _5266.hi = _5269;
    Interval _5267 = _5266;
    Interval _5270 = _5267;
    Interval _5273 = _5270;
    Interval _16101 = iadd(_5271, _5273, intervalFailed);
    Interval _5274 = _16101;
    Interval _5297 = _5274;
    Interval _5298 = param_var_a_40.x;
    Interval _5299 = param_var_b_30.y;
    Interval _16107 = imul(_5298, _5299, intervalFailed, optical_product_upper);
    Interval _5300 = _16107;
    Interval _5301 = param_var_a_40.y;
    Interval _5302 = param_var_b_30.x;
    Interval _16112 = imul(_5301, _5302, intervalFailed, optical_product_upper);
    Interval _5303 = _16112;
    Interval _5262 = _5300;
    Interval _5263 = _5303;
    float _5259 = as_type<float>(as_type<uint>(_5263.hi) ^ 2147483648u);
    float _5260 = as_type<float>(as_type<uint>(_5263.lo) ^ 2147483648u);
    Interval _5257;
    _5257.lo = _5259;
    _5257.hi = _5260;
    Interval _5258 = _5257;
    Interval _5261 = _5258;
    Interval _5264 = _5261;
    Interval _16132 = iadd(_5262, _5264, intervalFailed);
    Interval _5265 = _16132;
    Interval _5304 = _5265;
    Interval3 _5255;
    _5255.x = _5290;
    _5255.y = _5297;
    _5255.z = _5304;
    Interval3 _5256 = _5255;
    Interval3 _5305 = _5256;
    Interval3 error = _5305;
    float param_var_x_9 = spvFMul(len.hi, 0.0040000001899898052215576171875);
    float _16145 = interval_up(param_var_x_9, intervalFailed);
    float param_var_a_41 = _16145;
    float param_var_b_31 = precise::max(receiver.projection.x, receiver.projection.y);
    bool param_var_upper = true;
    float _16153 = quotient_bound(param_var_a_41, param_var_b_31, param_var_upper, intervalFailed);
    float param_var_x_10 = _16153;
    float _16154 = interval_up(param_var_x_10, intervalFailed);
    float cap = _16154;
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
        uint _2365 = _output >> 2u;
        results._m0[_2365] = 0u;
        results._m0[_2365 + 1u] = status;
        results._m0[_2365 + 2u] = 0u;
        results._m0[_2365 + 3u] = 0u;
        uint _2372 = (_output + 16u) >> 2u;
        results._m0[_2372] = 0u;
        results._m0[_2372 + 1u] = 0u;
        results._m0[_2372 + 2u] = 0u;
        results._m0[_2372 + 3u] = 0u;
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
    uint _2411 = (at + 496u) >> 2u;
    if (any(uint4(frames._m0[_2411], frames._m0[_2411 + 1u], frames._m0[_2411 + 2u], frames._m0[_2411 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        if (lane == 0u)
        {
            results._m0[(_output + 4u) >> 2u] = 4u;
        }
        return;
    }
    uint _2429 = at >> 2u;
    ReflectionRoughFrame receiver;
    receiver.a = as_type<float4>(uint4(frames._m0[_2429], frames._m0[_2429 + 1u], frames._m0[_2429 + 2u], frames._m0[_2429 + 3u]));
    uint _2442 = (at + 16u) >> 2u;
    receiver.b = as_type<float4>(uint4(frames._m0[_2442], frames._m0[_2442 + 1u], frames._m0[_2442 + 2u], frames._m0[_2442 + 3u]));
    uint _2455 = (at + 32u) >> 2u;
    receiver.c = as_type<float4>(uint4(frames._m0[_2455], frames._m0[_2455 + 1u], frames._m0[_2455 + 2u], frames._m0[_2455 + 3u]));
    uint _2468 = (at + 48u) >> 2u;
    receiver.projection = as_type<float4>(uint4(frames._m0[_2468], frames._m0[_2468 + 1u], frames._m0[_2468 + 2u], frames._m0[_2468 + 3u]));
    uint _2481 = (at + 64u) >> 2u;
    receiver.extentClip = as_type<float4>(uint4(frames._m0[_2481], frames._m0[_2481 + 1u], frames._m0[_2481 + 2u], frames._m0[_2481 + 3u]));
    uint _2494 = (at + 80u) >> 2u;
    receiver.settings = as_type<float4>(uint4(frames._m0[_2494], frames._m0[_2494 + 1u], frames._m0[_2494 + 2u], frames._m0[_2494 + 3u]));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint h = 0u; h < 4u; h++)
    {
        uint _2510 = ((at + 96u) + (h * 48u)) >> 2u;
        planes[h].a = as_type<float4>(uint4(frames._m0[_2510], frames._m0[_2510 + 1u], frames._m0[_2510 + 2u], frames._m0[_2510 + 3u]));
        uint _2525 = ((at + 112u) + (h * 48u)) >> 2u;
        planes[h].b = as_type<float4>(uint4(frames._m0[_2525], frames._m0[_2525 + 1u], frames._m0[_2525 + 2u], frames._m0[_2525 + 3u]));
        uint _2540 = ((at + 128u) + (h * 48u)) >> 2u;
        planes[h].c = as_type<float4>(uint4(frames._m0[_2540], frames._m0[_2540 + 1u], frames._m0[_2540 + 2u], frames._m0[_2540 + 3u]));
    }
    uint _2555 = (at + 288u) >> 2u;
    ReflectionLiquidFrame liquid;
    liquid.planePoint = as_type<float4>(uint4(frames._m0[_2555], frames._m0[_2555 + 1u], frames._m0[_2555 + 2u], frames._m0[_2555 + 3u]));
    uint _2568 = (at + 304u) >> 2u;
    liquid.planeNormal = as_type<float4>(uint4(frames._m0[_2568], frames._m0[_2568 + 1u], frames._m0[_2568 + 2u], frames._m0[_2568 + 3u]));
    uint _2581 = (at + 320u) >> 2u;
    liquid.rotation0 = as_type<float4>(uint4(frames._m0[_2581], frames._m0[_2581 + 1u], frames._m0[_2581 + 2u], frames._m0[_2581 + 3u]));
    uint _2594 = (at + 336u) >> 2u;
    liquid.rotation1 = as_type<float4>(uint4(frames._m0[_2594], frames._m0[_2594 + 1u], frames._m0[_2594 + 2u], frames._m0[_2594 + 3u]));
    uint _2607 = (at + 352u) >> 2u;
    liquid.rotation2 = as_type<float4>(uint4(frames._m0[_2607], frames._m0[_2607 + 1u], frames._m0[_2607 + 2u], frames._m0[_2607 + 3u]));
    uint _2620 = (at + 368u) >> 2u;
    liquid.projection = as_type<float4>(uint4(frames._m0[_2620], frames._m0[_2620 + 1u], frames._m0[_2620 + 2u], frames._m0[_2620 + 3u]));
    uint _2633 = (at + 384u) >> 2u;
    liquid.extentClip = as_type<float4>(uint4(frames._m0[_2633], frames._m0[_2633 + 1u], frames._m0[_2633 + 2u], frames._m0[_2633 + 3u]));
    uint _2646 = (at + 400u) >> 2u;
    liquid.settings = as_type<float4>(uint4(frames._m0[_2646], frames._m0[_2646 + 1u], frames._m0[_2646 + 2u], frames._m0[_2646 + 3u]));
    uint _2659 = (at + 416u) >> 2u;
    float4 terminal = as_type<float4>(uint4(frames._m0[_2659], frames._m0[_2659 + 1u], frames._m0[_2659 + 2u], frames._m0[_2659 + 3u]));
    uint _2671 = (at + 480u) >> 2u;
    uint4 control = uint4(frames._m0[_2671], frames._m0[_2671 + 1u], frames._m0[_2671 + 2u], frames._m0[_2671 + 3u]);
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
    uint _2707 = (at + 432u) >> 2u;
    ReflectionSpecularPlane terminalPlane;
    terminalPlane.a = as_type<float4>(uint4(frames._m0[_2707], frames._m0[_2707 + 1u], frames._m0[_2707 + 2u], frames._m0[_2707 + 3u]));
    uint _2720 = (at + 448u) >> 2u;
    terminalPlane.b = as_type<float4>(uint4(frames._m0[_2720], frames._m0[_2720 + 1u], frames._m0[_2720 + 2u], frames._m0[_2720 + 3u]));
    uint _2733 = (at + 464u) >> 2u;
    terminalPlane.c = as_type<float4>(uint4(frames._m0[_2733], frames._m0[_2733 + 1u], frames._m0[_2733 + 2u], frames._m0[_2733 + 3u]));
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
        bool4 _2802 = isnan(receiver.settings);
        bool4 _2803 = isinf(receiver.settings);
        temp_var_logical_15 = !all(not(bool4(_2802.x || _2803.x, _2802.y || _2803.y, _2802.z || _2803.z, _2802.w || _2803.w)));
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
        bool4 _2865 = isnan(receiver.projection);
        bool4 _2866 = isinf(receiver.projection);
        temp_var_logical_24 = !all(not(bool4(_2865.x || _2866.x, _2865.y || _2866.y, _2865.z || _2866.z, _2865.w || _2866.w)));
    }
    bool temp_var_logical_25 = true;
    if (!temp_var_logical_24)
    {
        temp_var_logical_25 = any(receiver.projection.xy <= float2(0.0));
    }
    bool temp_var_logical_26 = true;
    if (!temp_var_logical_25)
    {
        bool4 _2881 = isnan(terminal);
        bool4 _2882 = isinf(terminal);
        temp_var_logical_26 = !all(not(bool4(_2881.x || _2882.x, _2881.y || _2882.y, _2881.z || _2882.z, _2881.w || _2882.w)));
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
    uint _2941 = (((_input + 16u) + (r * 48u)) + 16u) >> 2u;
    float4 initial = spvFAdd(as_type<float4>(uint4(regions._m0[_2941], regions._m0[_2941 + 1u], regions._m0[_2941 + 2u], regions._m0[_2941 + 3u])), float4(0.5));
    float param_var_x = initial.x;
    float _2954 = interval_down(param_var_x, intervalFailed);
    float param_var_x_1 = initial.y;
    float _2957 = interval_down(param_var_x_1, intervalFailed);
    float param_var_x_2 = initial.z;
    float _2960 = interval_up(param_var_x_2, intervalFailed);
    float param_var_x_3 = initial.w;
    float _2963 = interval_up(param_var_x_3, intervalFailed);
    float4 extent = float4(_2954, _2957, _2960, _2963);
    bool4 _2966 = isnan(extent);
    bool4 _2967 = isinf(extent);
    bool temp_var_logical_32 = true;
    if (all(not(bool4(_2966.x || _2967.x, _2966.y || _2967.y, _2966.z || _2967.z, _2966.w || _2967.w))))
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
    Interval3 _3014 = native_target(param_var_at, param_var_feature, intervalFailed, optical_product_upper, interval_divide_upper, frames, TargetSettings);
    Interval3 enclosedTarget = _3014;
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
        float _3036 = interval_down(param_var_x_4, intervalFailed);
        float param_var_lo_1 = extent.y;
        float param_var_hi_1 = extent.w;
        uint param_var_cell_1 = ij.y;
        uint param_var_count_1 = bins.y;
        float param_var_x_5 = split_axis(param_var_lo_1, param_var_hi_1, param_var_cell_1, param_var_count_1);
        float _3046 = interval_down(param_var_x_5, intervalFailed);
        float param_var_lo_2 = extent.x;
        float param_var_hi_2 = extent.z;
        uint param_var_cell_2 = ij.x + 1u;
        uint param_var_count_2 = bins.x;
        float param_var_x_6 = split_axis(param_var_lo_2, param_var_hi_2, param_var_cell_2, param_var_count_2);
        float _3056 = interval_up(param_var_x_6, intervalFailed);
        float param_var_lo_3 = extent.y;
        float param_var_hi_3 = extent.w;
        uint param_var_cell_3 = ij.y + 1u;
        uint param_var_count_3 = bins.y;
        float param_var_x_7 = split_axis(param_var_lo_3, param_var_hi_3, param_var_cell_3, param_var_count_3);
        float _3066 = interval_up(param_var_x_7, intervalFailed);
        float4 box = float4(_3036, _3046, _3056, _3066);
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
            bool _3079 = optical_excluded_target(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTerminal, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper);
            temp_var_logical_33 = _3079;
        }
        bool excluded = temp_var_logical_33;
        if (excluded)
        {
            __attribute__((unused)) uint _3082 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&excludedCount, 1u, memory_order_relaxed);
        }
        else
        {
            __attribute__((unused)) uint _3083 = atomic_fetch_add_explicit((threadgroup atomic_uint*)&keptCount, 1u, memory_order_relaxed);
            __attribute__((unused)) uint _3087 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullX0, as_type<uint>(box.x), memory_order_relaxed);
            __attribute__((unused)) uint _3091 = atomic_fetch_min_explicit((threadgroup atomic_uint*)&hullY0, as_type<uint>(box.y), memory_order_relaxed);
            __attribute__((unused)) uint _3095 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullX1, as_type<uint>(box.z), memory_order_relaxed);
            __attribute__((unused)) uint _3099 = atomic_fetch_max_explicit((threadgroup atomic_uint*)&hullY1, as_type<uint>(box.w), memory_order_relaxed);
        }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (lane == 0u)
    {
        uint _3104 = _output >> 2u;
        results._m0[_3104] = keptCount;
        results._m0[_3104 + 1u] = 0u;
        results._m0[_3104 + 2u] = cells;
        results._m0[_3104 + 3u] = excludedCount;
        uint _3113 = (_output + 16u) >> 2u;
        uint4 temp_var_ternary_2;
        if (keptCount != 0u)
        {
            temp_var_ternary_2 = uint4(hullX0, hullY0, hullX1, hullY1);
        }
        else
        {
            temp_var_ternary_2 = uint4(0u);
        }
        results._m0[_3113] = temp_var_ternary_2.x;
        results._m0[_3113 + 1u] = temp_var_ternary_2.y;
        results._m0[_3113 + 2u] = temp_var_ternary_2.z;
        results._m0[_3113 + 3u] = temp_var_ternary_2.w;
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

