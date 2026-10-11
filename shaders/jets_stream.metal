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

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct type_RootSettings
{
    uint rootCapacity;
    uint rootFrameStride;
    uint rootResultStride;
    uint rootMode;
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

struct OpticalJet
{
    Interval v;
    Interval dx;
    Interval dy;
};

struct OpticalLocalRoot
{
    uint status;
    float4 enclosure;
    float contraction;
    short evaluated;
};

struct OpticalJet3
{
    OpticalJet x;
    OpticalJet y;
    OpticalJet z;
};

struct OpticalJetRay
{
    OpticalJet3 origin;
    OpticalJet3 outgoing;
    OpticalJet depth;
    OpticalJet bias0;
};

constant spvUnsafeArray<int2, 8> _2308 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2368 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool feature_valid(thread const float3& f, thread const uint& kind)
{
    bool3 _3091 = isnan(f);
    bool3 _3092 = isinf(f);
    if (!all(not(bool3(_3091.x || _3092.x, _3091.y || _3092.y, _3091.z || _3092.z))))
    {
        return false;
    }
    if (kind == 1u)
    {
        bool _3106;
        if (f.z == 0.0)
        {
            _3106 = all(f.xy >= float2(0.0));
        }
        else
        {
            _3106 = false;
        }
        bool _3112;
        if (_3106)
        {
            _3112 = spvFAdd(f.x, f.y) <= 1.0;
        }
        else
        {
            _3112 = false;
        }
        return _3112;
    }
    bool _3117;
    if (kind != 2u)
    {
        _3117 = kind == 3u;
    }
    else
    {
        _3117 = true;
    }
    bool _3122;
    if (_3117)
    {
        _3122 = abs(spvFSub(dot(f, f), 1.0)) < 0.00010099999781232327222824096679688;
    }
    else
    {
        _3122 = false;
    }
    return _3122;
}

static inline __attribute__((always_inline))
bool reflection_specular_plane_valid(thread const ReflectionSpecularPlane& plane)
{
    bool _3133;
    if (plane.a.w == 2.0)
    {
        _3133 = plane.b.w == 2.0;
    }
    else
    {
        _3133 = false;
    }
    bool _3138;
    if (_3133)
    {
        _3138 = plane.c.w == 2.0;
    }
    else
    {
        _3138 = false;
    }
    bool _3155;
    if (!_3138)
    {
        bool _3148;
        if ((isunordered(plane.a.w, 1.0) || plane.a.w == 1.0))
        {
            _3148 = plane.b.w != 1.0;
        }
        else
        {
            _3148 = true;
        }
        bool _3154;
        if (!_3148)
        {
            _3154 = plane.c.w != 1.0;
        }
        else
        {
            _3154 = true;
        }
        _3155 = _3154;
    }
    else
    {
        _3155 = false;
    }
    bool _3165;
    if (!_3155)
    {
        bool4 _3159 = isnan(plane.a);
        bool4 _3160 = isinf(plane.a);
        _3165 = !all(not(bool4(_3159.x || _3160.x, _3159.y || _3160.y, _3159.z || _3160.z, _3159.w || _3160.w)));
    }
    else
    {
        _3165 = true;
    }
    bool _3175;
    if (!_3165)
    {
        bool4 _3169 = isnan(plane.b);
        bool4 _3170 = isinf(plane.b);
        _3175 = !all(not(bool4(_3169.x || _3170.x, _3169.y || _3170.y, _3169.z || _3170.z, _3169.w || _3170.w)));
    }
    else
    {
        _3175 = true;
    }
    bool _3185;
    if (!_3175)
    {
        bool4 _3179 = isnan(plane.c);
        bool4 _3180 = isinf(plane.c);
        _3185 = !all(not(bool4(_3179.x || _3180.x, _3179.y || _3180.y, _3179.z || _3180.z, _3179.w || _3180.w)));
    }
    else
    {
        _3185 = true;
    }
    bool _3193;
    if (!_3185)
    {
        _3193 = any(abs(plane.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3193 = true;
    }
    bool _3201;
    if (!_3193)
    {
        _3201 = any(abs(plane.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3201 = true;
    }
    bool _3209;
    if (!_3201)
    {
        _3209 = any(abs(plane.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3209 = true;
    }
    if (_3209)
    {
        return false;
    }
    bool _3215;
    if (_3138)
    {
        _3215 = any(plane.c.xyz != float3(0.0));
    }
    else
    {
        _3215 = false;
    }
    if (_3215)
    {
        return false;
    }
    float3 _3232;
    if (_3138)
    {
        _3232 = plane.b.xyz;
    }
    else
    {
        _3232 = cross(spvFSub(plane.b.xyz, plane.a.xyz), spvFSub(plane.c.xyz, plane.a.xyz));
    }
    float _1683 = dot(_3232, _3232);
    bool3 _3233 = isnan(_3232);
    bool3 _3234 = isinf(_3232);
    bool _3242;
    if (all(not(bool3(_3233.x || _3234.x, _3233.y || _3234.y, _3233.z || _3234.z))))
    {
        _3242 = !(isnan(_1683) || isinf(_1683));
    }
    else
    {
        _3242 = false;
    }
    bool _3244;
    if (_3242)
    {
        _3244 = _1683 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3244 = false;
    }
    return _3244;
}

static inline __attribute__((always_inline))
bool reflection_rough_frame_valid(thread const ReflectionRoughFrame& old)
{
    bool _3255;
    if (old.a.w == 2.0)
    {
        _3255 = old.b.w == 2.0;
    }
    else
    {
        _3255 = false;
    }
    bool _3260;
    if (_3255)
    {
        _3260 = old.c.w == 2.0;
    }
    else
    {
        _3260 = false;
    }
    bool _3277;
    if (!_3260)
    {
        bool _3270;
        if ((isunordered(old.a.w, 1.0) || old.a.w == 1.0))
        {
            _3270 = old.b.w != 1.0;
        }
        else
        {
            _3270 = true;
        }
        bool _3276;
        if (!_3270)
        {
            _3276 = old.c.w != 1.0;
        }
        else
        {
            _3276 = true;
        }
        _3277 = _3276;
    }
    else
    {
        _3277 = false;
    }
    bool _3287;
    if (!_3277)
    {
        bool4 _3281 = isnan(old.a);
        bool4 _3282 = isinf(old.a);
        _3287 = !all(not(bool4(_3281.x || _3282.x, _3281.y || _3282.y, _3281.z || _3282.z, _3281.w || _3282.w)));
    }
    else
    {
        _3287 = true;
    }
    bool _3297;
    if (!_3287)
    {
        bool4 _3291 = isnan(old.b);
        bool4 _3292 = isinf(old.b);
        _3297 = !all(not(bool4(_3291.x || _3292.x, _3291.y || _3292.y, _3291.z || _3292.z, _3291.w || _3292.w)));
    }
    else
    {
        _3297 = true;
    }
    bool _3307;
    if (!_3297)
    {
        bool4 _3301 = isnan(old.c);
        bool4 _3302 = isinf(old.c);
        _3307 = !all(not(bool4(_3301.x || _3302.x, _3301.y || _3302.y, _3301.z || _3302.z, _3301.w || _3302.w)));
    }
    else
    {
        _3307 = true;
    }
    bool _3317;
    if (!_3307)
    {
        bool4 _3311 = isnan(old.projection);
        bool4 _3312 = isinf(old.projection);
        _3317 = !all(not(bool4(_3311.x || _3312.x, _3311.y || _3312.y, _3311.z || _3312.z, _3311.w || _3312.w)));
    }
    else
    {
        _3317 = true;
    }
    bool _3327;
    if (!_3317)
    {
        bool4 _3321 = isnan(old.extentClip);
        bool4 _3322 = isinf(old.extentClip);
        _3327 = !all(not(bool4(_3321.x || _3322.x, _3321.y || _3322.y, _3321.z || _3322.z, _3321.w || _3322.w)));
    }
    else
    {
        _3327 = true;
    }
    bool _3337;
    if (!_3327)
    {
        bool4 _3331 = isnan(old.settings);
        bool4 _3332 = isinf(old.settings);
        _3337 = !all(not(bool4(_3331.x || _3332.x, _3331.y || _3332.y, _3331.z || _3332.z, _3331.w || _3332.w)));
    }
    else
    {
        _3337 = true;
    }
    bool _3345;
    if (!_3337)
    {
        _3345 = any(abs(old.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3345 = true;
    }
    bool _3353;
    if (!_3345)
    {
        _3353 = any(abs(old.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3353 = true;
    }
    bool _3361;
    if (!_3353)
    {
        _3361 = any(abs(old.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3361 = true;
    }
    bool _3368;
    if (!_3361)
    {
        _3368 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3368 = true;
    }
    bool _3375;
    if (!_3368)
    {
        _3375 = any(abs(old.projection) > float4(999999995904.0));
    }
    else
    {
        _3375 = true;
    }
    bool _3382;
    if (!_3375)
    {
        _3382 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3382 = true;
    }
    bool _3389;
    if (!_3382)
    {
        _3389 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3389 = true;
    }
    bool _3395;
    if (!_3389)
    {
        _3395 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3395 = true;
    }
    bool _3404;
    if (!_3395)
    {
        _3404 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3404 = true;
    }
    bool _3410;
    if (!_3404)
    {
        _3410 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3410 = true;
    }
    bool _3416;
    if (!_3410)
    {
        _3416 = old.settings.x < 0.0;
    }
    else
    {
        _3416 = true;
    }
    bool _3422;
    if (!_3416)
    {
        _3422 = old.settings.x > 1.0;
    }
    else
    {
        _3422 = true;
    }
    bool _3428;
    if (!_3422)
    {
        _3428 = old.settings.y < 0.0;
    }
    else
    {
        _3428 = true;
    }
    bool _3434;
    if (!_3428)
    {
        _3434 = old.settings.y > 7.0;
    }
    else
    {
        _3434 = true;
    }
    bool _3444;
    if (!_3434)
    {
        _3444 = floor(old.settings.y) != old.settings.y;
    }
    else
    {
        _3444 = true;
    }
    bool _3451;
    if (!_3444)
    {
        _3451 = any(old.settings.zw != float2(0.0));
    }
    else
    {
        _3451 = true;
    }
    if (_3451)
    {
        return false;
    }
    bool _3457;
    if (_3260)
    {
        _3457 = any(old.c.xyz != float3(0.0));
    }
    else
    {
        _3457 = false;
    }
    if (_3457)
    {
        return false;
    }
    float3 _3474;
    if (_3260)
    {
        _3474 = old.b.xyz;
    }
    else
    {
        _3474 = cross(spvFSub(old.b.xyz, old.a.xyz), spvFSub(old.c.xyz, old.a.xyz));
    }
    float _1686 = dot(_3474, _3474);
    bool3 _3475 = isnan(_3474);
    bool3 _3476 = isinf(_3474);
    bool _3484;
    if (all(not(bool3(_3475.x || _3476.x, _3475.y || _3476.y, _3475.z || _3476.z))))
    {
        _3484 = !(isnan(_1686) || isinf(_1686));
    }
    else
    {
        _3484 = false;
    }
    bool _3486;
    if (_3484)
    {
        _3486 = _1686 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3486 = false;
    }
    return _3486;
}

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float _1687 = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3495 = isnan(old.planePoint);
    bool4 _3496 = isinf(old.planePoint);
    bool _3508;
    if (all(not(bool4(_3495.x || _3496.x, _3495.y || _3496.y, _3495.z || _3496.z, _3495.w || _3496.w))))
    {
        bool4 _3502 = isnan(old.planeNormal);
        bool4 _3503 = isinf(old.planeNormal);
        _3508 = !all(not(bool4(_3502.x || _3503.x, _3502.y || _3503.y, _3502.z || _3503.z, _3502.w || _3503.w)));
    }
    else
    {
        _3508 = true;
    }
    bool _3518;
    if (!_3508)
    {
        bool4 _3512 = isnan(old.rotation0);
        bool4 _3513 = isinf(old.rotation0);
        _3518 = !all(not(bool4(_3512.x || _3513.x, _3512.y || _3513.y, _3512.z || _3513.z, _3512.w || _3513.w)));
    }
    else
    {
        _3518 = true;
    }
    bool _3528;
    if (!_3518)
    {
        bool4 _3522 = isnan(old.rotation1);
        bool4 _3523 = isinf(old.rotation1);
        _3528 = !all(not(bool4(_3522.x || _3523.x, _3522.y || _3523.y, _3522.z || _3523.z, _3522.w || _3523.w)));
    }
    else
    {
        _3528 = true;
    }
    bool _3538;
    if (!_3528)
    {
        bool4 _3532 = isnan(old.rotation2);
        bool4 _3533 = isinf(old.rotation2);
        _3538 = !all(not(bool4(_3532.x || _3533.x, _3532.y || _3533.y, _3532.z || _3533.z, _3532.w || _3533.w)));
    }
    else
    {
        _3538 = true;
    }
    bool _3548;
    if (!_3538)
    {
        bool4 _3542 = isnan(old.projection);
        bool4 _3543 = isinf(old.projection);
        _3548 = !all(not(bool4(_3542.x || _3543.x, _3542.y || _3543.y, _3542.z || _3543.z, _3542.w || _3543.w)));
    }
    else
    {
        _3548 = true;
    }
    bool _3558;
    if (!_3548)
    {
        bool4 _3552 = isnan(old.extentClip);
        bool4 _3553 = isinf(old.extentClip);
        _3558 = !all(not(bool4(_3552.x || _3553.x, _3552.y || _3553.y, _3552.z || _3553.z, _3552.w || _3553.w)));
    }
    else
    {
        _3558 = true;
    }
    bool _3568;
    if (!_3558)
    {
        bool4 _3562 = isnan(old.settings);
        bool4 _3563 = isinf(old.settings);
        _3568 = !all(not(bool4(_3562.x || _3563.x, _3562.y || _3563.y, _3562.z || _3563.z, _3562.w || _3563.w)));
    }
    else
    {
        _3568 = true;
    }
    bool _3576;
    if (!_3568)
    {
        _3576 = any(abs(old.planePoint.xyz) > float3(999999995904.0));
    }
    else
    {
        _3576 = true;
    }
    bool _3581;
    if (!_3576)
    {
        _3581 = isnan(_1687) || isinf(_1687);
    }
    else
    {
        _3581 = true;
    }
    bool _3584;
    if (!_3581)
    {
        _3584 = _1687 <= 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3584 = true;
    }
    bool _3591;
    if (!_3584)
    {
        _3591 = any(abs(old.rotation0) > float4(999999995904.0));
    }
    else
    {
        _3591 = true;
    }
    bool _3598;
    if (!_3591)
    {
        _3598 = any(abs(old.rotation1) > float4(999999995904.0));
    }
    else
    {
        _3598 = true;
    }
    bool _3605;
    if (!_3598)
    {
        _3605 = any(abs(old.rotation2) > float4(999999995904.0));
    }
    else
    {
        _3605 = true;
    }
    bool _3612;
    if (!_3605)
    {
        _3612 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3612 = true;
    }
    bool _3619;
    if (!_3612)
    {
        _3619 = any(old.projection.xy > float2(999999995904.0));
    }
    else
    {
        _3619 = true;
    }
    bool _3626;
    if (!_3619)
    {
        _3626 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3626 = true;
    }
    bool _3633;
    if (!_3626)
    {
        _3633 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3633 = true;
    }
    bool _3639;
    if (!_3633)
    {
        _3639 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3639 = true;
    }
    bool _3648;
    if (!_3639)
    {
        _3648 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3648 = true;
    }
    bool _3654;
    if (!_3648)
    {
        _3654 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3654 = true;
    }
    bool _3660;
    if (!_3654)
    {
        _3660 = old.settings.x < 0.0;
    }
    else
    {
        _3660 = true;
    }
    bool _3666;
    if (!_3660)
    {
        _3666 = old.settings.x > 999999995904.0;
    }
    else
    {
        _3666 = true;
    }
    bool _3677;
    if (!_3666)
    {
        bool _3676;
        if (old.settings.y != 0.0)
        {
            _3676 = old.settings.y != 3.0;
        }
        else
        {
            _3676 = false;
        }
        _3677 = _3676;
    }
    else
    {
        _3677 = true;
    }
    if (_3677)
    {
        return false;
    }
    float _1688 = dot(old.rotation0.xyz, cross(old.rotation1.xyz, old.rotation2.xyz));
    bool _3694;
    if (!(isnan(_1688) || isinf(_1688)))
    {
        _3694 = abs(_1688) > 9.9999999600419720025001879548654e-13;
    }
    else
    {
        _3694 = false;
    }
    return _3694;
}

static inline __attribute__((always_inline))
bool interval_exact_point(thread const Interval& a, thread const float& x)
{
    if (x == 0.0)
    {
        return ((as_type<uint>(a.lo) | as_type<uint>(a.hi)) & 2147483647u) == 0u;
    }
    bool _11461;
    if (as_type<uint>(a.lo) == as_type<uint>(x))
    {
        _11461 = as_type<uint>(a.hi) == as_type<uint>(x);
    }
    else
    {
        _11461 = false;
    }
    return _11461;
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
    uint _16035;
    if (x > 0.0)
    {
        _16035 = 1u;
    }
    else
    {
        _16035 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _16035);
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
    uint _16049;
    if (x < 0.0)
    {
        _16049 = 1u;
    }
    else
    {
        _16049 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _16049);
}

static __attribute__((noinline))
float optical_add_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    uint _11464 = as_type<uint>(a) & 2147483647u;
    uint _11467 = as_type<uint>(b) & 2147483647u;
    bool _11470;
    if (_11464 < 2139095040u)
    {
        _11470 = _11467 < 2139095040u;
    }
    else
    {
        _11470 = false;
    }
    if (_11470)
    {
        if (_11464 == 0u)
        {
            return b;
        }
        if (_11467 == 0u)
        {
            return a;
        }
        bool _11477;
        if (_11464 >= 8388608u)
        {
            _11477 = _11467 < 8388608u;
        }
        else
        {
            _11477 = true;
        }
        if (_11477)
        {
            intervalFailed = true;
        }
        bool _11480;
        if (_11464 >= 562036736u)
        {
            _11480 = _11464 <= 1568669696u;
        }
        else
        {
            _11480 = false;
        }
        bool _11482;
        if (_11480)
        {
            _11482 = _11467 >= 562036736u;
        }
        else
        {
            _11482 = false;
        }
        bool _11484;
        if (_11482)
        {
            _11484 = _11467 <= 1568669696u;
        }
        else
        {
            _11484 = false;
        }
        if (_11484)
        {
            float _11488;
            if (_11464 >= _11467)
            {
                _11488 = a;
            }
            else
            {
                _11488 = b;
            }
            float _11492;
            if (_11464 >= _11467)
            {
                _11492 = b;
            }
            else
            {
                _11492 = a;
            }
            float _1769 = spvFAdd(_11488, _11492);
            float _1771 = spvFSub(_11492, spvFSub(_1769, _11488));
            if (upper)
            {
                float _11496;
                if (_1771 > 0.0)
                {
                    float param_var_x = _1769;
                    float _11495 = interval_up(param_var_x, intervalFailed);
                    _11496 = _11495;
                }
                else
                {
                    _11496 = _1769;
                }
                return _11496;
            }
            float _11499;
            if (_1771 < 0.0)
            {
                float param_var_x_1 = _1769;
                float _11498 = interval_down(param_var_x_1, intervalFailed);
                _11499 = _11498;
            }
            else
            {
                _11499 = _1769;
            }
            return _11499;
        }
    }
    float _1772 = spvFAdd(a, b);
    float _11505;
    if (upper)
    {
        float param_var_x_2 = _1772;
        float _11504 = interval_up(param_var_x_2, intervalFailed);
        _11505 = _11504;
    }
    else
    {
        float param_var_x_3 = _1772;
        float _11503 = interval_down(param_var_x_3, intervalFailed);
        _11505 = _11503;
    }
    return _11505;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _4558;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _4558 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _4558 = false;
    }
    bool _4562;
    if (_4558)
    {
        _4562 = a.lo <= a.hi;
    }
    else
    {
        _4562 = false;
    }
    bool _4581;
    if (_4562)
    {
        bool _4576;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _4576 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _4576 = false;
        }
        bool _4580;
        if (_4576)
        {
            _4580 = b.lo <= b.hi;
        }
        else
        {
            _4580 = false;
        }
        _4581 = _4580;
    }
    else
    {
        _4581 = false;
    }
    if (_4581)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        if (interval_exact_point(param_var_a, param_var_x))
        {
            return b;
        }
        Interval param_var_a_1 = b;
        float param_var_x_1 = 0.0;
        if (interval_exact_point(param_var_a_1, param_var_x_1))
        {
            return a;
        }
    }
    float param_var_a_2 = a.lo;
    float param_var_b = b.lo;
    bool param_var_upper = false;
    float _4592 = optical_add_bound(param_var_a_2, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_3 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _4597 = optical_add_bound(param_var_a_3, param_var_b_1, param_var_upper_1, intervalFailed);
    return Interval{ _4592, _4597 };
}

static inline __attribute__((always_inline))
uint interval_product_extrema(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi)
{
    uint _16053 = as_type<uint>(alo) & 2147483647u;
    uint _16056 = as_type<uint>(ahi) & 2147483647u;
    uint _16059 = as_type<uint>(blo) & 2147483647u;
    uint _16062 = as_type<uint>(bhi) & 2147483647u;
    bool _16065;
    if (_16053 >= 813694976u)
    {
        _16065 = _16053 > 1317011456u;
    }
    else
    {
        _16065 = true;
    }
    bool _16068;
    if (!_16065)
    {
        _16068 = _16056 < 813694976u;
    }
    else
    {
        _16068 = true;
    }
    bool _16071;
    if (!_16068)
    {
        _16071 = _16056 > 1317011456u;
    }
    else
    {
        _16071 = true;
    }
    bool _16074;
    if (!_16071)
    {
        _16074 = _16059 < 813694976u;
    }
    else
    {
        _16074 = true;
    }
    bool _16077;
    if (!_16074)
    {
        _16077 = _16059 > 1317011456u;
    }
    else
    {
        _16077 = true;
    }
    bool _16080;
    if (!_16077)
    {
        _16080 = _16062 < 813694976u;
    }
    else
    {
        _16080 = true;
    }
    bool _16083;
    if (!_16080)
    {
        _16083 = _16062 > 1317011456u;
    }
    else
    {
        _16083 = true;
    }
    bool _16088;
    if (!_16083)
    {
        _16088 = alo > ahi;
    }
    else
    {
        _16088 = true;
    }
    bool _16093;
    if (!_16088)
    {
        _16093 = blo > bhi;
    }
    else
    {
        _16093 = true;
    }
    if (_16093)
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
    uint _16111 = as_type<uint>(a);
    uint _16112 = _16111 & 2147483647u;
    uint _16114 = as_type<uint>(b);
    uint _16115 = _16114 & 2147483647u;
    bool _16118;
    if (_16112 < 2139095040u)
    {
        _16118 = _16115 < 2139095040u;
    }
    else
    {
        _16118 = false;
    }
    if (_16118)
    {
        bool _16121;
        if (_16112 != 0u)
        {
            _16121 = _16115 == 0u;
        }
        else
        {
            _16121 = true;
        }
        if (_16121)
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
            float _16138 = as_type<float>(as_type<uint>(b) ^ 2147483648u);
            optical_product_upper = _16138;
            return _16138;
        }
        if (as_type<uint>(b) == 3212836864u)
        {
            float _16145 = as_type<float>(as_type<uint>(a) ^ 2147483648u);
            optical_product_upper = _16145;
            return _16145;
        }
        bool _16148;
        if (_16112 >= 8388608u)
        {
            _16148 = _16115 < 8388608u;
        }
        else
        {
            _16148 = true;
        }
        if (_16148)
        {
            intervalFailed = true;
        }
        bool _16151;
        if (_16112 >= 813694976u)
        {
            _16151 = _16112 <= 1317011456u;
        }
        else
        {
            _16151 = false;
        }
        bool _16153;
        if (_16151)
        {
            _16153 = _16115 >= 813694976u;
        }
        else
        {
            _16153 = false;
        }
        bool _16155;
        if (_16153)
        {
            _16155 = _16115 <= 1317011456u;
        }
        else
        {
            _16155 = false;
        }
        if (_16155)
        {
            float _1778 = spvFMul(a, b);
            uint _16162 = _16111 & 65535u;
            uint _16163 = ((_16111 & 8388607u) | 8388608u) >> 16u;
            uint _16164 = _16114 & 65535u;
            uint _16165 = ((_16114 & 8388607u) | 8388608u) >> 16u;
            uint _1779 = _16162 * _16164;
            uint _1782 = (_16162 * _16165) + (_16163 * _16164);
            uint _1783 = _1779 + (_1782 << 16u);
            uint _16169;
            if (_1783 < _1779)
            {
                _16169 = 1u;
            }
            else
            {
                _16169 = 0u;
            }
            uint _1786 = ((_16163 * _16165) + (_1782 >> 16u)) + _16169;
            uint _16170 = as_type<uint>(_1778);
            uint _1788 = (((_16170 & 2147483647u) >> 23u) - (_16112 >> 23u)) - (_16115 >> 23u);
            uint _1789 = _1788 + 150u;
            bool _16177;
            if (_1789 >= 23u)
            {
                _16177 = _1789 > 24u;
            }
            else
            {
                _16177 = true;
            }
            if (_16177)
            {
                intervalFailed = true;
                float param_var_x = _1778;
                float _16178 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _16178;
                float param_var_x_1 = _1778;
                float _16179 = interval_down(param_var_x_1, intervalFailed);
                return _16179;
            }
            uint _16181 = (_16170 & 8388607u) | 8388608u;
            uint _16183 = _16181 << (_1789 & 31u);
            uint _16185 = _16181 >> ((4294967178u - _1788) & 31u);
            bool _16190;
            if (_1786 <= _16185)
            {
                bool _16189;
                if (_1786 == _16185)
                {
                    _16189 = _1783 > _16183;
                }
                else
                {
                    _16189 = false;
                }
                _16190 = _16189;
            }
            else
            {
                _16190 = true;
            }
            bool _16195;
            if (_1786 >= _16185)
            {
                bool _16194;
                if (_1786 == _16185)
                {
                    _16194 = _1783 < _16183;
                }
                else
                {
                    _16194 = false;
                }
                _16195 = _16194;
            }
            else
            {
                _16195 = true;
            }
            bool _16202 = ((as_type<uint>(a) ^ as_type<uint>(b)) & 2147483648u) != 0u;
            bool _16203;
            if (_16202)
            {
                _16203 = _16195;
            }
            else
            {
                _16203 = _16190;
            }
            float _16205;
            if (_16203)
            {
                float param_var_x_2 = _1778;
                float _16204 = interval_up(param_var_x_2, intervalFailed);
                _16205 = _16204;
            }
            else
            {
                _16205 = _1778;
            }
            optical_product_upper = _16205;
            bool _16206;
            if (_16202)
            {
                _16206 = _16190;
            }
            else
            {
                _16206 = _16195;
            }
            float _16208;
            if (_16206)
            {
                float param_var_x_3 = _1778;
                float _16207 = interval_down(param_var_x_3, intervalFailed);
                _16208 = _16207;
            }
            else
            {
                _16208 = _1778;
            }
            return _16208;
        }
    }
    float _1791 = spvFMul(a, b);
    float param_var_x_4 = _1791;
    float _16211 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _16211;
    float param_var_x_5 = _1791;
    float _16212 = interval_down(param_var_x_5, intervalFailed);
    return _16212;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _11519;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11519 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11519 = false;
    }
    bool _11523;
    if (_11519)
    {
        _11523 = a.lo <= a.hi;
    }
    else
    {
        _11523 = false;
    }
    bool _11542;
    if (_11523)
    {
        bool _11537;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11537 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11537 = false;
        }
        bool _11541;
        if (_11537)
        {
            _11541 = b.lo <= b.hi;
        }
        else
        {
            _11541 = false;
        }
        _11542 = _11541;
    }
    else
    {
        _11542 = false;
    }
    if (_11542)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        bool _11548;
        if (!interval_exact_point(param_var_a, param_var_x))
        {
            Interval param_var_a_1 = b;
            float param_var_x_1 = 0.0;
            _11548 = interval_exact_point(param_var_a_1, param_var_x_1);
        }
        else
        {
            _11548 = true;
        }
        if (_11548)
        {
            return Interval{ 0.0, 0.0 };
        }
        Interval param_var_a_2 = a;
        float param_var_x_2 = 1.0;
        if (interval_exact_point(param_var_a_2, param_var_x_2))
        {
            return b;
        }
        Interval param_var_a_3 = b;
        float param_var_x_3 = 1.0;
        if (interval_exact_point(param_var_a_3, param_var_x_3))
        {
            return a;
        }
        Interval param_var_a_4 = a;
        float param_var_x_4 = -1.0;
        if (interval_exact_point(param_var_a_4, param_var_x_4))
        {
            return Interval{ as_type<float>(as_type<uint>(b.hi) ^ 2147483648u), as_type<float>(as_type<uint>(b.lo) ^ 2147483648u) };
        }
        Interval param_var_a_5 = b;
        float param_var_x_5 = -1.0;
        if (interval_exact_point(param_var_a_5, param_var_x_5))
        {
            return Interval{ as_type<float>(as_type<uint>(a.hi) ^ 2147483648u), as_type<float>(as_type<uint>(a.lo) ^ 2147483648u) };
        }
    }
    bool _11592;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11592 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11592 = false;
    }
    bool _11596;
    if (_11592)
    {
        _11596 = a.lo <= a.hi;
    }
    else
    {
        _11596 = false;
    }
    bool _11615;
    if (_11596)
    {
        bool _11610;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11610 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11610 = false;
        }
        bool _11614;
        if (_11610)
        {
            _11614 = b.lo <= b.hi;
        }
        else
        {
            _11614 = false;
        }
        _11615 = _11614;
    }
    else
    {
        _11615 = false;
    }
    bool _11623;
    if (_11615)
    {
        _11623 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _11623 = false;
    }
    bool _11631;
    if (_11615)
    {
        _11631 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _11631 = false;
    }
    bool _11633;
    if (_11615)
    {
        _11633 = !_11623;
    }
    else
    {
        _11633 = false;
    }
    bool _11635;
    if (_11633)
    {
        _11635 = !_11631;
    }
    else
    {
        _11635 = false;
    }
    uint _11645;
    if (_11635)
    {
        float param_var_alo = a.lo;
        float param_var_ahi = a.hi;
        float param_var_blo = b.lo;
        float param_var_bhi = b.hi;
        _11645 = interval_product_extrema(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi);
    }
    else
    {
        _11645 = 0u;
    }
    if (_11645 != 0u)
    {
        uint _11647 = _11645 >> 2u;
        float _11654;
        if ((_11645 & 2u) != 0u)
        {
            _11654 = a.hi;
        }
        else
        {
            _11654 = a.lo;
        }
        float param_var_a_6 = _11654;
        float _11661;
        if ((_11645 & 1u) != 0u)
        {
            _11661 = b.hi;
        }
        else
        {
            _11661 = b.lo;
        }
        float param_var_b = _11661;
        float _11662 = optical_product_bounds(param_var_a_6, param_var_b, intervalFailed, optical_product_upper);
        float _11669;
        if ((_11647 & 2u) != 0u)
        {
            _11669 = a.hi;
        }
        else
        {
            _11669 = a.lo;
        }
        float param_var_a_7 = _11669;
        float _11676;
        if ((_11647 & 1u) != 0u)
        {
            _11676 = b.hi;
        }
        else
        {
            _11676 = b.lo;
        }
        float param_var_b_1 = _11676;
        __attribute__((unused)) float _11677 = optical_product_bounds(param_var_a_7, param_var_b_1, intervalFailed, optical_product_upper);
        return Interval{ _11662, optical_product_upper };
    }
    float param_var_a_8 = a.lo;
    float param_var_b_2 = b.lo;
    float _11684 = optical_product_bounds(param_var_a_8, param_var_b_2, intervalFailed, optical_product_upper);
    float _11693;
    float _11694;
    if (!_11631)
    {
        float param_var_a_9 = a.lo;
        float param_var_b_3 = b.hi;
        float _11691 = optical_product_bounds(param_var_a_9, param_var_b_3, intervalFailed, optical_product_upper);
        _11693 = optical_product_upper;
        _11694 = _11691;
    }
    else
    {
        _11693 = optical_product_upper;
        _11694 = _11684;
    }
    float _11702;
    float _11703;
    if (!_11623)
    {
        float param_var_a_10 = a.hi;
        float param_var_b_4 = b.lo;
        float _11700 = optical_product_bounds(param_var_a_10, param_var_b_4, intervalFailed, optical_product_upper);
        _11702 = optical_product_upper;
        _11703 = _11700;
    }
    else
    {
        _11702 = optical_product_upper;
        _11703 = _11684;
    }
    float _11713;
    float _11714;
    if (!_11623)
    {
        float _11711;
        float _11712;
        if (_11631)
        {
            _11711 = _11702;
            _11712 = _11703;
        }
        else
        {
            float param_var_a_11 = a.hi;
            float param_var_b_5 = b.hi;
            float _11709 = optical_product_bounds(param_var_a_11, param_var_b_5, intervalFailed, optical_product_upper);
            _11711 = optical_product_upper;
            _11712 = _11709;
        }
        _11713 = _11711;
        _11714 = _11712;
    }
    else
    {
        _11713 = _11693;
        _11714 = _11694;
    }
    return Interval{ precise::min(precise::min(_11684, _11694), precise::min(_11703, _11714)), precise::max(precise::max(optical_product_upper, _11693), precise::max(_11702, _11713)) };
}

static __attribute__((noinline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float _21649;
    _21649 = precise::sqrt(a);
    float _21650;
    for (uint _21651 = 0u; _21651 < 8u; _21649 = _21650, _21651++)
    {
        float _1879 = spvFMul(_21649, _21649);
        float _1880 = spvFMul(_21649, 4097.0);
        float _1882 = spvFSub(_1880, spvFSub(_1880, _21649));
        float _1883 = spvFSub(_21649, _1882);
        float _1884 = spvFMul(_21649, 4097.0);
        float _1886 = spvFSub(_1884, spvFSub(_1884, _21649));
        float _1887 = spvFSub(_21649, _1886);
        float _1901 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1882, _1886), _1879), spvFMul(_1882, _1887)), spvFMul(_1883, _1886)), spvFMul(_1883, _1887)), spvFMul(_21649, 0.0)), spvFMul(0.0, _21649)), spvFMul(0.0, 0.0));
        float _1902 = spvFAdd(_1879, _1901);
        float _1904 = spvFSub(_1901, spvFSub(_1902, _1879));
        bool _21660;
        if (!(isnan(_1902) || isinf(_1902)))
        {
            _21660 = isnan(_1904) || isinf(_1904);
        }
        else
        {
            _21660 = true;
        }
        if (_21660)
        {
            intervalFailed = true;
            return _21649;
        }
        bool _21667;
        if ((isunordered(_1902, a) || _1902 >= a))
        {
            bool _21666;
            if (_1902 == a)
            {
                _21666 = _1904 < 0.0;
            }
            else
            {
                _21666 = false;
            }
            _21667 = _21666;
        }
        else
        {
            _21667 = true;
        }
        bool _21674;
        if ((isunordered(a, _1902) || a >= _1902))
        {
            bool _21673;
            if (a == _1902)
            {
                _21673 = 0.0 < _1904;
            }
            else
            {
                _21673 = false;
            }
            _21674 = _21673;
        }
        else
        {
            _21674 = true;
        }
        bool _21678;
        if (upper)
        {
            _21678 = !_21667;
        }
        else
        {
            _21678 = !_21674;
        }
        if (_21678)
        {
            return _21649;
        }
        if (upper)
        {
            float param_var_x = _21649;
            float _21682 = interval_up(param_var_x, intervalFailed);
            _21650 = _21682;
        }
        else
        {
            float param_var_x_1 = _21649;
            float _21680 = interval_down(param_var_x_1, intervalFailed);
            _21650 = precise::max(0.0, _21680);
        }
    }
    intervalFailed = true;
    return _21649;
}

static inline __attribute__((always_inline))
Interval isqrt(thread const Interval& a, thread bool& intervalFailed)
{
    bool _16219;
    if ((isunordered(a.hi, 0.0) || a.hi >= 0.0))
    {
        _16219 = a.hi > 1000000015047466219876688855040.0;
    }
    else
    {
        _16219 = true;
    }
    if (_16219)
    {
        intervalFailed = true;
        return Interval{ 0.0, 1000000015047466219876688855040.0 };
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _16223 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _16223;
    float _16224 = interval_down(param_var_x, intervalFailed);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _16229 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _16229;
    float _16230 = interval_up(param_var_x_1, intervalFailed);
    return Interval{ precise::max(0.0, _16224), _16230 };
}

static __attribute__((noinline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float _21685;
    _21685 = a / b;
    float _21686;
    for (uint _21687 = 0u; _21687 < 8u; _21685 = _21686, _21687++)
    {
        bool _21695;
        if (!(isnan(_21685) || isinf(_21685)))
        {
            _21695 = abs(_21685) > 1000000015047466219876688855040.0;
        }
        else
        {
            _21695 = true;
        }
        if (_21695)
        {
            intervalFailed = true;
            return _21685;
        }
        float _1929 = spvFMul(_21685, b);
        float _1930 = spvFMul(_21685, 4097.0);
        float _1932 = spvFSub(_1930, spvFSub(_1930, _21685));
        float _1933 = spvFSub(_21685, _1932);
        float _1934 = spvFMul(b, 4097.0);
        float _1936 = spvFSub(_1934, spvFSub(_1934, b));
        float _1937 = spvFSub(b, _1936);
        float _1951 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1932, _1936), _1929), spvFMul(_1932, _1937)), spvFMul(_1933, _1936)), spvFMul(_1933, _1937)), spvFMul(_21685, 0.0)), spvFMul(0.0, b)), spvFMul(0.0, 0.0));
        float _1952 = spvFAdd(_1929, _1951);
        float _1954 = spvFSub(_1951, spvFSub(_1952, _1929));
        bool _21704;
        if (!(isnan(_1952) || isinf(_1952)))
        {
            _21704 = isnan(_1954) || isinf(_1954);
        }
        else
        {
            _21704 = true;
        }
        if (_21704)
        {
            intervalFailed = true;
            return _21685;
        }
        bool _21711;
        if ((isunordered(_1952, a) || _1952 >= a))
        {
            bool _21710;
            if (_1952 == a)
            {
                _21710 = _1954 < 0.0;
            }
            else
            {
                _21710 = false;
            }
            _21711 = _21710;
        }
        else
        {
            _21711 = true;
        }
        bool _21718;
        if ((isunordered(a, _1952) || a >= _1952))
        {
            bool _21717;
            if (a == _1952)
            {
                _21717 = 0.0 < _1954;
            }
            else
            {
                _21717 = false;
            }
            _21718 = _21717;
        }
        else
        {
            _21718 = true;
        }
        if (b < 0.0)
        {
            bool _21728;
            if (upper)
            {
                _21728 = !_21718;
            }
            else
            {
                _21728 = !_21711;
            }
            if (_21728)
            {
                return _21685;
            }
        }
        else
        {
            bool _21724;
            if (upper)
            {
                _21724 = !_21711;
            }
            else
            {
                _21724 = !_21718;
            }
            if (_21724)
            {
                return _21685;
            }
        }
        if (upper)
        {
            float param_var_x = _21685;
            float _21731 = interval_up(param_var_x, intervalFailed);
            _21686 = _21731;
        }
        else
        {
            float param_var_x_1 = _21685;
            float _21730 = interval_down(param_var_x_1, intervalFailed);
            _21686 = _21730;
        }
    }
    intervalFailed = true;
    return _21685;
}

static __attribute__((noinline))
float interval_divide_pair(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _16244;
    if (!(isnan(alo) || isinf(alo)))
    {
        _16244 = !(isnan(ahi) || isinf(ahi));
    }
    else
    {
        _16244 = false;
    }
    bool _16248;
    if (_16244)
    {
        _16248 = alo <= ahi;
    }
    else
    {
        _16248 = false;
    }
    bool _16266;
    if (_16248)
    {
        bool _16261;
        if (!(isnan(blo) || isinf(blo)))
        {
            _16261 = !(isnan(bhi) || isinf(bhi));
        }
        else
        {
            _16261 = false;
        }
        bool _16265;
        if (_16261)
        {
            _16265 = blo <= bhi;
        }
        else
        {
            _16265 = false;
        }
        _16266 = _16265;
    }
    else
    {
        _16266 = false;
    }
    bool _16272;
    if (_16266)
    {
        _16272 = as_type<uint>(alo) == as_type<uint>(ahi);
    }
    else
    {
        _16272 = false;
    }
    bool _16278;
    if (_16266)
    {
        _16278 = as_type<uint>(blo) == as_type<uint>(bhi);
    }
    else
    {
        _16278 = false;
    }
    float param_var_a = alo;
    float param_var_b = blo;
    bool param_var_upper = false;
    float _16281 = quotient_bound(param_var_a, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_1 = alo;
    float param_var_b_1 = blo;
    bool param_var_upper_1 = true;
    float _16284 = quotient_bound(param_var_a_1, param_var_b_1, param_var_upper_1, intervalFailed);
    float _16292;
    float _16293;
    if (!_16278)
    {
        float param_var_a_2 = alo;
        float param_var_b_2 = bhi;
        bool param_var_upper_2 = false;
        float _16288 = quotient_bound(param_var_a_2, param_var_b_2, param_var_upper_2, intervalFailed);
        float param_var_a_3 = alo;
        float param_var_b_3 = bhi;
        bool param_var_upper_3 = true;
        float _16291 = quotient_bound(param_var_a_3, param_var_b_3, param_var_upper_3, intervalFailed);
        _16292 = _16291;
        _16293 = _16288;
    }
    else
    {
        _16292 = _16284;
        _16293 = _16281;
    }
    float _16301;
    float _16302;
    if (!_16272)
    {
        float param_var_a_4 = ahi;
        float param_var_b_4 = blo;
        bool param_var_upper_4 = false;
        float _16297 = quotient_bound(param_var_a_4, param_var_b_4, param_var_upper_4, intervalFailed);
        float param_var_a_5 = ahi;
        float param_var_b_5 = blo;
        bool param_var_upper_5 = true;
        float _16300 = quotient_bound(param_var_a_5, param_var_b_5, param_var_upper_5, intervalFailed);
        _16301 = _16300;
        _16302 = _16297;
    }
    else
    {
        _16301 = _16284;
        _16302 = _16281;
    }
    float _16312;
    float _16313;
    if (!_16272)
    {
        float _16310;
        float _16311;
        if (_16278)
        {
            _16310 = _16301;
            _16311 = _16302;
        }
        else
        {
            float param_var_a_6 = ahi;
            float param_var_b_6 = bhi;
            bool param_var_upper_6 = false;
            float _16306 = quotient_bound(param_var_a_6, param_var_b_6, param_var_upper_6, intervalFailed);
            float param_var_a_7 = ahi;
            float param_var_b_7 = bhi;
            bool param_var_upper_7 = true;
            float _16309 = quotient_bound(param_var_a_7, param_var_b_7, param_var_upper_7, intervalFailed);
            _16310 = _16309;
            _16311 = _16306;
        }
        _16312 = _16310;
        _16313 = _16311;
    }
    else
    {
        _16312 = _16292;
        _16313 = _16293;
    }
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16284), _16292), _16301), _16312);
    return precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16281), _16293), _16302), _16313);
}

static inline __attribute__((always_inline))
Interval idiv(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _11729;
    if (b.lo <= 0.0)
    {
        _11729 = b.hi >= 0.0;
    }
    else
    {
        _11729 = false;
    }
    bool _11739;
    if (!_11729)
    {
        _11739 = precise::max(abs(b.lo), abs(b.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11739 = true;
    }
    bool _11749;
    if (!_11739)
    {
        _11749 = precise::max(abs(a.lo), abs(a.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11749 = true;
    }
    if (_11749)
    {
        intervalFailed = true;
        return Interval{ -1000000015047466219876688855040.0, 1000000015047466219876688855040.0 };
    }
    bool _11763;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11763 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11763 = false;
    }
    bool _11767;
    if (_11763)
    {
        _11767 = a.lo <= a.hi;
    }
    else
    {
        _11767 = false;
    }
    bool _11786;
    if (_11767)
    {
        bool _11781;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11781 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11781 = false;
        }
        bool _11785;
        if (_11781)
        {
            _11785 = b.lo <= b.hi;
        }
        else
        {
            _11785 = false;
        }
        _11786 = _11785;
    }
    else
    {
        _11786 = false;
    }
    if (_11786)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        if (interval_exact_point(param_var_a, param_var_x))
        {
            return Interval{ 0.0, 0.0 };
        }
        Interval param_var_a_1 = b;
        float param_var_x_1 = 1.0;
        if (interval_exact_point(param_var_a_1, param_var_x_1))
        {
            return a;
        }
        Interval param_var_a_2 = b;
        float param_var_x_2 = -1.0;
        if (interval_exact_point(param_var_a_2, param_var_x_2))
        {
            return Interval{ as_type<float>(as_type<uint>(a.hi) ^ 2147483648u), as_type<float>(as_type<uint>(a.lo) ^ 2147483648u) };
        }
    }
    float param_var_alo = a.lo;
    float param_var_ahi = a.hi;
    float param_var_blo = b.lo;
    float param_var_bhi = b.hi;
    float _11812 = interval_divide_pair(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi, intervalFailed, interval_divide_upper);
    float param_var_x_3 = _11812;
    float _11813 = interval_down(param_var_x_3, intervalFailed);
    float param_var_x_4 = interval_divide_upper;
    float _11815 = interval_up(param_var_x_4, intervalFailed);
    return Interval{ _11813, _11815 };
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, device type_ByteAddressBuffer& frames, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        Interval param_var_a = Interval{ feature.x, feature.x };
        Interval param_var_b = Interval{ feature.y, feature.y };
        Interval _3815 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval _3804 = Interval{ 1.0, 1.0 };
        Interval _3805 = Interval{ as_type<float>(as_type<uint>(_3815.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_3815.lo) ^ 2147483648u) };
        Interval _3825 = iadd(_3804, _3805, intervalFailed);
        uint _3827 = (at + 432u) >> 2u;
        uint _3829 = frames._m0[_3827];
        uint _3831 = frames._m0[_3827 + 1u];
        uint _3833 = frames._m0[_3827 + 2u];
        uint _3835 = frames._m0[_3827 + 3u];
        float4 _3837 = as_type<float4>(uint4(_3829, _3831, _3833, _3835));
        float _3838 = _3837.x;
        float _3839 = _3837.y;
        float _3840 = _3837.z;
        Interval _3798 = Interval{ _3838, _3838 };
        Interval _3799 = _3825;
        Interval _3842 = imul(_3798, _3799, intervalFailed, optical_product_upper);
        Interval _3800 = Interval{ _3839, _3839 };
        Interval _3801 = _3825;
        Interval _3844 = imul(_3800, _3801, intervalFailed, optical_product_upper);
        Interval _3802 = Interval{ _3840, _3840 };
        Interval _3803 = _3825;
        Interval _3846 = imul(_3802, _3803, intervalFailed, optical_product_upper);
        uint _3848 = (at + 448u) >> 2u;
        uint _3850 = frames._m0[_3848];
        uint _3852 = frames._m0[_3848 + 1u];
        uint _3854 = frames._m0[_3848 + 2u];
        uint _3856 = frames._m0[_3848 + 3u];
        float4 _3858 = as_type<float4>(uint4(_3850, _3852, _3854, _3856));
        float _3859 = _3858.x;
        float _3860 = _3858.y;
        float _3861 = _3858.z;
        Interval _3792 = Interval{ _3859, _3859 };
        Interval _3793 = Interval{ feature.x, feature.x };
        Interval _3864 = imul(_3792, _3793, intervalFailed, optical_product_upper);
        Interval _3794 = Interval{ _3860, _3860 };
        Interval _3795 = Interval{ feature.x, feature.x };
        Interval _3867 = imul(_3794, _3795, intervalFailed, optical_product_upper);
        Interval _3796 = Interval{ _3861, _3861 };
        Interval _3797 = Interval{ feature.x, feature.x };
        Interval _3870 = imul(_3796, _3797, intervalFailed, optical_product_upper);
        Interval _3786 = _3842;
        Interval _3787 = _3864;
        Interval _3871 = iadd(_3786, _3787, intervalFailed);
        Interval _3788 = _3844;
        Interval _3789 = _3867;
        Interval _3872 = iadd(_3788, _3789, intervalFailed);
        Interval _3790 = _3846;
        Interval _3791 = _3870;
        Interval _3873 = iadd(_3790, _3791, intervalFailed);
        uint _3875 = (at + 464u) >> 2u;
        uint _3877 = frames._m0[_3875];
        uint _3879 = frames._m0[_3875 + 1u];
        uint _3881 = frames._m0[_3875 + 2u];
        uint _3883 = frames._m0[_3875 + 3u];
        float4 _3885 = as_type<float4>(uint4(_3877, _3879, _3881, _3883));
        float _3886 = _3885.x;
        float _3887 = _3885.y;
        float _3888 = _3885.z;
        Interval _3780 = Interval{ _3886, _3886 };
        Interval _3781 = Interval{ feature.y, feature.y };
        Interval _3891 = imul(_3780, _3781, intervalFailed, optical_product_upper);
        Interval _3782 = Interval{ _3887, _3887 };
        Interval _3783 = Interval{ feature.y, feature.y };
        Interval _3894 = imul(_3782, _3783, intervalFailed, optical_product_upper);
        Interval _3784 = Interval{ _3888, _3888 };
        Interval _3785 = Interval{ feature.y, feature.y };
        Interval _3897 = imul(_3784, _3785, intervalFailed, optical_product_upper);
        Interval _3774 = _3871;
        Interval _3775 = _3891;
        Interval _3898 = iadd(_3774, _3775, intervalFailed);
        Interval _3776 = _3872;
        Interval _3777 = _3894;
        Interval _3899 = iadd(_3776, _3777, intervalFailed);
        Interval _3778 = _3873;
        Interval _3779 = _3897;
        Interval _3900 = iadd(_3778, _3779, intervalFailed);
        return Interval3{ _3898, _3899, _3900 };
    }
    Interval _3764 = Interval{ TargetSettings.targetCurrentCube[0].x, TargetSettings.targetCurrentCube[0].x };
    Interval _3765 = Interval{ feature.x, feature.x };
    Interval _3914 = imul(_3764, _3765, intervalFailed, optical_product_upper);
    Interval _3766 = _3914;
    Interval _3767 = Interval{ TargetSettings.targetCurrentCube[0].y, TargetSettings.targetCurrentCube[0].y };
    Interval _3768 = Interval{ feature.y, feature.y };
    Interval _3917 = imul(_3767, _3768, intervalFailed, optical_product_upper);
    Interval _3769 = _3917;
    Interval _3918 = iadd(_3766, _3769, intervalFailed);
    Interval _3770 = _3918;
    Interval _3771 = Interval{ TargetSettings.targetCurrentCube[0].z, TargetSettings.targetCurrentCube[0].z };
    Interval _3772 = Interval{ feature.z, feature.z };
    Interval _3921 = imul(_3771, _3772, intervalFailed, optical_product_upper);
    Interval _3773 = _3921;
    Interval _3922 = iadd(_3770, _3773, intervalFailed);
    Interval _3754 = Interval{ TargetSettings.targetCurrentCube[1].x, TargetSettings.targetCurrentCube[1].x };
    Interval _3755 = Interval{ feature.x, feature.x };
    Interval _3931 = imul(_3754, _3755, intervalFailed, optical_product_upper);
    Interval _3756 = _3931;
    Interval _3757 = Interval{ TargetSettings.targetCurrentCube[1].y, TargetSettings.targetCurrentCube[1].y };
    Interval _3758 = Interval{ feature.y, feature.y };
    Interval _3934 = imul(_3757, _3758, intervalFailed, optical_product_upper);
    Interval _3759 = _3934;
    Interval _3935 = iadd(_3756, _3759, intervalFailed);
    Interval _3760 = _3935;
    Interval _3761 = Interval{ TargetSettings.targetCurrentCube[1].z, TargetSettings.targetCurrentCube[1].z };
    Interval _3762 = Interval{ feature.z, feature.z };
    Interval _3938 = imul(_3761, _3762, intervalFailed, optical_product_upper);
    Interval _3763 = _3938;
    Interval _3939 = iadd(_3760, _3763, intervalFailed);
    Interval _3744 = Interval{ TargetSettings.targetCurrentCube[2].x, TargetSettings.targetCurrentCube[2].x };
    Interval _3745 = Interval{ feature.x, feature.x };
    Interval _3948 = imul(_3744, _3745, intervalFailed, optical_product_upper);
    Interval _3746 = _3948;
    Interval _3747 = Interval{ TargetSettings.targetCurrentCube[2].y, TargetSettings.targetCurrentCube[2].y };
    Interval _3748 = Interval{ feature.y, feature.y };
    Interval _3951 = imul(_3747, _3748, intervalFailed, optical_product_upper);
    Interval _3749 = _3951;
    Interval _3952 = iadd(_3746, _3749, intervalFailed);
    Interval _3750 = _3952;
    Interval _3751 = Interval{ TargetSettings.targetCurrentCube[2].z, TargetSettings.targetCurrentCube[2].z };
    Interval _3752 = Interval{ feature.z, feature.z };
    Interval _3955 = imul(_3751, _3752, intervalFailed, optical_product_upper);
    Interval _3753 = _3955;
    Interval _3956 = iadd(_3750, _3753, intervalFailed);
    Interval _3734 = Interval{ TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[0].x };
    Interval _3735 = _3922;
    Interval _3970 = imul(_3734, _3735, intervalFailed, optical_product_upper);
    Interval _3736 = _3970;
    Interval _3737 = Interval{ TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[1].x };
    Interval _3738 = _3939;
    Interval _3972 = imul(_3737, _3738, intervalFailed, optical_product_upper);
    Interval _3739 = _3972;
    Interval _3973 = iadd(_3736, _3739, intervalFailed);
    Interval _3740 = _3973;
    Interval _3741 = Interval{ TargetSettings.targetPreviousCube[2].x, TargetSettings.targetPreviousCube[2].x };
    Interval _3742 = _3956;
    Interval _3975 = imul(_3741, _3742, intervalFailed, optical_product_upper);
    Interval _3743 = _3975;
    Interval _3976 = iadd(_3740, _3743, intervalFailed);
    Interval _3724 = Interval{ TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[0].y };
    Interval _3725 = _3922;
    Interval _3990 = imul(_3724, _3725, intervalFailed, optical_product_upper);
    Interval _3726 = _3990;
    Interval _3727 = Interval{ TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[1].y };
    Interval _3728 = _3939;
    Interval _3992 = imul(_3727, _3728, intervalFailed, optical_product_upper);
    Interval _3729 = _3992;
    Interval _3993 = iadd(_3726, _3729, intervalFailed);
    Interval _3730 = _3993;
    Interval _3731 = Interval{ TargetSettings.targetPreviousCube[2].y, TargetSettings.targetPreviousCube[2].y };
    Interval _3732 = _3956;
    Interval _3995 = imul(_3731, _3732, intervalFailed, optical_product_upper);
    Interval _3733 = _3995;
    Interval _3996 = iadd(_3730, _3733, intervalFailed);
    Interval _3714 = Interval{ TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[0].z };
    Interval _3715 = _3922;
    Interval _4010 = imul(_3714, _3715, intervalFailed, optical_product_upper);
    Interval _3716 = _4010;
    Interval _3717 = Interval{ TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[1].z };
    Interval _3718 = _3939;
    Interval _4012 = imul(_3717, _3718, intervalFailed, optical_product_upper);
    Interval _3719 = _4012;
    Interval _4013 = iadd(_3716, _3719, intervalFailed);
    Interval _3720 = _4013;
    Interval _3721 = Interval{ TargetSettings.targetPreviousCube[2].z, TargetSettings.targetPreviousCube[2].z };
    Interval _3722 = _3956;
    Interval _4015 = imul(_3721, _3722, intervalFailed, optical_product_upper);
    Interval _3723 = _4015;
    Interval _4016 = iadd(_3720, _3723, intervalFailed);
    Interval _3712 = Interval{ 1.0, 1.0 };
    bool _4023;
    if (_3976.lo <= 0.0)
    {
        _4023 = _3976.hi >= 0.0;
    }
    else
    {
        _4023 = false;
    }
    float _4030;
    if (_4023)
    {
        _4030 = 0.0;
    }
    else
    {
        _4030 = precise::min(abs(_3976.lo), abs(_3976.hi));
    }
    float _4033 = precise::max(abs(_3976.lo), abs(_3976.hi));
    float _3705 = spvFMul(_4030, _4030);
    float _4034 = interval_down(_3705, intervalFailed);
    float _3706 = spvFMul(_4033, _4033);
    float _4036 = interval_up(_3706, intervalFailed);
    Interval _3707 = Interval{ precise::max(0.0, _4034), _4036 };
    bool _4044;
    if (_3996.lo <= 0.0)
    {
        _4044 = _3996.hi >= 0.0;
    }
    else
    {
        _4044 = false;
    }
    float _4051;
    if (_4044)
    {
        _4051 = 0.0;
    }
    else
    {
        _4051 = precise::min(abs(_3996.lo), abs(_3996.hi));
    }
    float _4054 = precise::max(abs(_3996.lo), abs(_3996.hi));
    float _3703 = spvFMul(_4051, _4051);
    float _4055 = interval_down(_3703, intervalFailed);
    float _3704 = spvFMul(_4054, _4054);
    float _4057 = interval_up(_3704, intervalFailed);
    Interval _3708 = Interval{ precise::max(0.0, _4055), _4057 };
    Interval _4059 = iadd(_3707, _3708, intervalFailed);
    Interval _3709 = _4059;
    bool _4066;
    if (_4016.lo <= 0.0)
    {
        _4066 = _4016.hi >= 0.0;
    }
    else
    {
        _4066 = false;
    }
    float _4073;
    if (_4066)
    {
        _4073 = 0.0;
    }
    else
    {
        _4073 = precise::min(abs(_4016.lo), abs(_4016.hi));
    }
    float _4076 = precise::max(abs(_4016.lo), abs(_4016.hi));
    float _3701 = spvFMul(_4073, _4073);
    float _4077 = interval_down(_3701, intervalFailed);
    float _3702 = spvFMul(_4076, _4076);
    float _4079 = interval_up(_3702, intervalFailed);
    Interval _3710 = Interval{ precise::max(0.0, _4077), _4079 };
    Interval _4081 = iadd(_3709, _3710, intervalFailed);
    Interval _3711 = _4081;
    Interval _4082 = isqrt(_3711, intervalFailed);
    Interval _3713 = _4082;
    Interval _4083 = idiv(_3712, _3713, intervalFailed, interval_divide_upper);
    Interval _3695 = _3976;
    Interval _3696 = _4083;
    Interval _4084 = imul(_3695, _3696, intervalFailed, optical_product_upper);
    Interval _3697 = _3996;
    Interval _3698 = _4083;
    Interval _4085 = imul(_3697, _3698, intervalFailed, optical_product_upper);
    Interval _3699 = _4016;
    Interval _3700 = _4083;
    Interval _4086 = imul(_3699, _3700, intervalFailed, optical_product_upper);
    return Interval3{ _4084, _4085, _4086 };
}

static inline __attribute__((always_inline))
Interval jet_add_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _16350;
    if (a.lo == 0.0)
    {
        _16350 = a.hi == 0.0;
    }
    else
    {
        _16350 = false;
    }
    if (_16350)
    {
        return b;
    }
    bool _16359;
    if (b.lo == 0.0)
    {
        _16359 = b.hi == 0.0;
    }
    else
    {
        _16359 = false;
    }
    if (_16359)
    {
        return a;
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16363 = iadd(param_var_a, param_var_b, intervalFailed);
    return _16363;
}

static inline __attribute__((always_inline))
Interval jet_mul_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _16329;
    if (a.lo == 0.0)
    {
        _16329 = a.hi == 0.0;
    }
    else
    {
        _16329 = false;
    }
    bool _16339;
    if (!_16329)
    {
        bool _16338;
        if (b.lo == 0.0)
        {
            _16338 = b.hi == 0.0;
        }
        else
        {
            _16338 = false;
        }
        _16339 = _16338;
    }
    else
    {
        _16339 = true;
    }
    if (_16339)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16342 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    return _16342;
}

static inline __attribute__((always_inline))
Interval jet_div_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    bool _16371;
    if (a.lo == 0.0)
    {
        _16371 = a.hi == 0.0;
    }
    else
    {
        _16371 = false;
    }
    bool _16381;
    if (_16371)
    {
        bool _16379;
        if (b.lo <= 0.0)
        {
            _16379 = b.hi >= 0.0;
        }
        else
        {
            _16379 = false;
        }
        _16381 = !_16379;
    }
    else
    {
        _16381 = false;
    }
    if (_16381)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16385 = idiv(param_var_a, param_var_b, intervalFailed, interval_divide_upper);
    bool _16396;
    if (!intervalFailed)
    {
        _16396 = intervalFailed;
    }
    else
    {
        _16396 = false;
    }
    bool _16401;
    if (_16396)
    {
        _16401 = jetFailureSite == 0u;
    }
    else
    {
        _16401 = false;
    }
    if (_16401)
    {
        jetFailureSite = 2u;
        jetFailureArguments = float4(a.lo, a.hi, b.lo, b.hi);
    }
    return _16385;
}

static inline __attribute__((always_inline))
Interval3 plane_normal(thread const ReflectionSpecularPlane& p, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _16483;
    if (p.a.w == 2.0)
    {
        _16483 = p.b.w == 2.0;
    }
    else
    {
        _16483 = false;
    }
    bool _16488;
    if (_16483)
    {
        _16488 = p.c.w == 2.0;
    }
    else
    {
        _16488 = false;
    }
    if (_16488)
    {
        Interval _16471 = Interval{ 1.0, 1.0 };
        bool _16498;
        if (p.b.x <= 0.0)
        {
            _16498 = p.b.x >= 0.0;
        }
        else
        {
            _16498 = false;
        }
        float _16505;
        if (_16498)
        {
            _16505 = 0.0;
        }
        else
        {
            _16505 = precise::min(abs(p.b.x), abs(p.b.x));
        }
        float _16508 = precise::max(abs(p.b.x), abs(p.b.x));
        float _16464 = spvFMul(_16505, _16505);
        float _16509 = interval_down(_16464, intervalFailed);
        float _16465 = spvFMul(_16508, _16508);
        float _16511 = interval_up(_16465, intervalFailed);
        Interval _16466 = Interval{ precise::max(0.0, _16509), _16511 };
        bool _16517;
        if (p.b.y <= 0.0)
        {
            _16517 = p.b.y >= 0.0;
        }
        else
        {
            _16517 = false;
        }
        float _16524;
        if (_16517)
        {
            _16524 = 0.0;
        }
        else
        {
            _16524 = precise::min(abs(p.b.y), abs(p.b.y));
        }
        float _16527 = precise::max(abs(p.b.y), abs(p.b.y));
        float _16462 = spvFMul(_16524, _16524);
        float _16528 = interval_down(_16462, intervalFailed);
        float _16463 = spvFMul(_16527, _16527);
        float _16530 = interval_up(_16463, intervalFailed);
        Interval _16467 = Interval{ precise::max(0.0, _16528), _16530 };
        Interval _16532 = iadd(_16466, _16467, intervalFailed);
        Interval _16468 = _16532;
        bool _16537;
        if (p.b.z <= 0.0)
        {
            _16537 = p.b.z >= 0.0;
        }
        else
        {
            _16537 = false;
        }
        float _16544;
        if (_16537)
        {
            _16544 = 0.0;
        }
        else
        {
            _16544 = precise::min(abs(p.b.z), abs(p.b.z));
        }
        float _16547 = precise::max(abs(p.b.z), abs(p.b.z));
        float _16460 = spvFMul(_16544, _16544);
        float _16548 = interval_down(_16460, intervalFailed);
        float _16461 = spvFMul(_16547, _16547);
        float _16550 = interval_up(_16461, intervalFailed);
        Interval _16469 = Interval{ precise::max(0.0, _16548), _16550 };
        Interval _16552 = iadd(_16468, _16469, intervalFailed);
        Interval _16470 = _16552;
        Interval _16553 = isqrt(_16470, intervalFailed);
        Interval _16472 = _16553;
        Interval _16554 = idiv(_16471, _16472, intervalFailed, interval_divide_upper);
        Interval _16454 = Interval{ p.b.x, p.b.x };
        Interval _16455 = _16554;
        Interval _16556 = imul(_16454, _16455, intervalFailed, optical_product_upper);
        Interval _16456 = Interval{ p.b.y, p.b.y };
        Interval _16457 = _16554;
        Interval _16558 = imul(_16456, _16457, intervalFailed, optical_product_upper);
        Interval _16458 = Interval{ p.b.z, p.b.z };
        Interval _16459 = _16554;
        Interval _16560 = imul(_16458, _16459, intervalFailed, optical_product_upper);
        return Interval3{ _16556, _16558, _16560 };
    }
    Interval _16448 = Interval{ p.b.x, p.b.x };
    Interval _16449 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16592 = iadd(_16448, _16449, intervalFailed);
    Interval _16450 = Interval{ p.b.y, p.b.y };
    Interval _16451 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16595 = iadd(_16450, _16451, intervalFailed);
    Interval _16452 = Interval{ p.b.z, p.b.z };
    Interval _16453 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16598 = iadd(_16452, _16453, intervalFailed);
    Interval _16442 = Interval{ p.c.x, p.c.x };
    Interval _16443 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16629 = iadd(_16442, _16443, intervalFailed);
    Interval _16444 = Interval{ p.c.y, p.c.y };
    Interval _16445 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16632 = iadd(_16444, _16445, intervalFailed);
    Interval _16446 = Interval{ p.c.z, p.c.z };
    Interval _16447 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16635 = iadd(_16446, _16447, intervalFailed);
    Interval _16430 = _16595;
    Interval _16431 = _16635;
    Interval _16636 = imul(_16430, _16431, intervalFailed, optical_product_upper);
    Interval _16432 = _16598;
    Interval _16433 = _16632;
    Interval _16637 = imul(_16432, _16433, intervalFailed, optical_product_upper);
    Interval _16428 = _16636;
    Interval _16429 = Interval{ as_type<float>(as_type<uint>(_16637.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16637.lo) ^ 2147483648u) };
    Interval _16647 = iadd(_16428, _16429, intervalFailed);
    Interval _16434 = _16598;
    Interval _16435 = _16629;
    Interval _16648 = imul(_16434, _16435, intervalFailed, optical_product_upper);
    Interval _16436 = _16592;
    Interval _16437 = _16635;
    Interval _16649 = imul(_16436, _16437, intervalFailed, optical_product_upper);
    Interval _16426 = _16648;
    Interval _16427 = Interval{ as_type<float>(as_type<uint>(_16649.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16649.lo) ^ 2147483648u) };
    Interval _16659 = iadd(_16426, _16427, intervalFailed);
    Interval _16438 = _16592;
    Interval _16439 = _16632;
    Interval _16660 = imul(_16438, _16439, intervalFailed, optical_product_upper);
    Interval _16440 = _16595;
    Interval _16441 = _16629;
    Interval _16661 = imul(_16440, _16441, intervalFailed, optical_product_upper);
    Interval _16424 = _16660;
    Interval _16425 = Interval{ as_type<float>(as_type<uint>(_16661.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16661.lo) ^ 2147483648u) };
    Interval _16671 = iadd(_16424, _16425, intervalFailed);
    Interval _16422 = Interval{ 1.0, 1.0 };
    bool _16678;
    if (_16647.lo <= 0.0)
    {
        _16678 = _16647.hi >= 0.0;
    }
    else
    {
        _16678 = false;
    }
    float _16685;
    if (_16678)
    {
        _16685 = 0.0;
    }
    else
    {
        _16685 = precise::min(abs(_16647.lo), abs(_16647.hi));
    }
    float _16688 = precise::max(abs(_16647.lo), abs(_16647.hi));
    float _16415 = spvFMul(_16685, _16685);
    float _16689 = interval_down(_16415, intervalFailed);
    float _16416 = spvFMul(_16688, _16688);
    float _16691 = interval_up(_16416, intervalFailed);
    Interval _16417 = Interval{ precise::max(0.0, _16689), _16691 };
    bool _16699;
    if (_16659.lo <= 0.0)
    {
        _16699 = _16659.hi >= 0.0;
    }
    else
    {
        _16699 = false;
    }
    float _16706;
    if (_16699)
    {
        _16706 = 0.0;
    }
    else
    {
        _16706 = precise::min(abs(_16659.lo), abs(_16659.hi));
    }
    float _16709 = precise::max(abs(_16659.lo), abs(_16659.hi));
    float _16413 = spvFMul(_16706, _16706);
    float _16710 = interval_down(_16413, intervalFailed);
    float _16414 = spvFMul(_16709, _16709);
    float _16712 = interval_up(_16414, intervalFailed);
    Interval _16418 = Interval{ precise::max(0.0, _16710), _16712 };
    Interval _16714 = iadd(_16417, _16418, intervalFailed);
    Interval _16419 = _16714;
    bool _16721;
    if (_16671.lo <= 0.0)
    {
        _16721 = _16671.hi >= 0.0;
    }
    else
    {
        _16721 = false;
    }
    float _16728;
    if (_16721)
    {
        _16728 = 0.0;
    }
    else
    {
        _16728 = precise::min(abs(_16671.lo), abs(_16671.hi));
    }
    float _16731 = precise::max(abs(_16671.lo), abs(_16671.hi));
    float _16411 = spvFMul(_16728, _16728);
    float _16732 = interval_down(_16411, intervalFailed);
    float _16412 = spvFMul(_16731, _16731);
    float _16734 = interval_up(_16412, intervalFailed);
    Interval _16420 = Interval{ precise::max(0.0, _16732), _16734 };
    Interval _16736 = iadd(_16419, _16420, intervalFailed);
    Interval _16421 = _16736;
    Interval _16737 = isqrt(_16421, intervalFailed);
    Interval _16423 = _16737;
    Interval _16738 = idiv(_16422, _16423, intervalFailed, interval_divide_upper);
    Interval _16405 = _16647;
    Interval _16406 = _16738;
    Interval _16739 = imul(_16405, _16406, intervalFailed, optical_product_upper);
    Interval _16407 = _16659;
    Interval _16408 = _16738;
    Interval _16740 = imul(_16407, _16408, intervalFailed, optical_product_upper);
    Interval _16409 = _16671;
    Interval _16410 = _16738;
    Interval _16741 = imul(_16409, _16410, intervalFailed, optical_product_upper);
    return Interval3{ _16739, _16740, _16741 };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_plane_normal(thread const ReflectionSpecularPlane& plane, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = plane;
    Interval3 _11818 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _11841;
    if (!intervalFailed)
    {
        bool _11834;
        if (plane.a.w == 2.0)
        {
            _11834 = plane.b.w == 2.0;
        }
        else
        {
            _11834 = false;
        }
        bool _11839;
        if (_11834)
        {
            _11839 = plane.c.w == 2.0;
        }
        else
        {
            _11839 = false;
        }
        _11841 = !_11839;
    }
    else
    {
        _11841 = false;
    }
    if (_11841)
    {
        for (uint _11842 = 0u; _11842 < 3u; _11842++)
        {
            bool _11854;
            if ((isunordered(plane.a[_11842], plane.b[_11842]) || plane.a[_11842] == plane.b[_11842]))
            {
                _11854 = plane.a[_11842] != plane.c[_11842];
            }
            else
            {
                _11854 = true;
            }
            if (_11854)
            {
                continue;
            }
            float _11865;
            float _11866;
            if (_11842 == 0u)
            {
                _11865 = _11818.x.hi;
                _11866 = _11818.x.lo;
            }
            else
            {
                float _11861;
                float _11862;
                if (_11842 == 1u)
                {
                    _11861 = _11818.y.hi;
                    _11862 = _11818.y.lo;
                }
                else
                {
                    _11861 = _11818.z.hi;
                    _11862 = _11818.z.lo;
                }
                _11865 = _11861;
                _11866 = _11862;
            }
            bool _11869;
            if (_11866 <= 0.0)
            {
                _11869 = _11865 >= 0.0;
            }
            else
            {
                _11869 = false;
            }
            if (_11869)
            {
                continue;
            }
            float3 exact = float3(0.0);
            int _11871;
            if (_11866 > 0.0)
            {
                _11871 = 1;
            }
            else
            {
                _11871 = -1;
            }
            exact[_11842] = float(_11871);
            return OpticalJet3{ OpticalJet{ Interval{ exact.x, exact.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.y, exact.y }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.z, exact.z }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
        }
    }
    return OpticalJet3{ OpticalJet{ _11818.x, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11818.y, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11818.z, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_oriented(thread const OpticalJet3& n, thread const OpticalJet3& direction, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    Interval _11974 = n.x.v;
    Interval _11975 = direction.x.v;
    Interval _12002 = imul(_11974, _11975, intervalFailed, optical_product_upper);
    Interval _11976 = n.x.dx;
    Interval _11977 = direction.x.v;
    Interval _12003 = jet_mul_derivative(_11976, _11977, intervalFailed, optical_product_upper);
    Interval _11978 = _12003;
    Interval _11979 = n.x.v;
    Interval _11980 = direction.x.dx;
    Interval _12004 = jet_mul_derivative(_11979, _11980, intervalFailed, optical_product_upper);
    Interval _11981 = _12004;
    Interval _12005 = jet_add_derivative(_11978, _11981, intervalFailed);
    Interval _11982 = n.x.dy;
    Interval _11983 = direction.x.v;
    Interval _12006 = jet_mul_derivative(_11982, _11983, intervalFailed, optical_product_upper);
    Interval _11984 = _12006;
    Interval _11985 = n.x.v;
    Interval _11986 = direction.x.dy;
    Interval _12007 = jet_mul_derivative(_11985, _11986, intervalFailed, optical_product_upper);
    Interval _11987 = _12007;
    Interval _12008 = jet_add_derivative(_11984, _11987, intervalFailed);
    Interval _11960 = n.y.v;
    Interval _11961 = direction.y.v;
    Interval _12015 = imul(_11960, _11961, intervalFailed, optical_product_upper);
    Interval _11962 = n.y.dx;
    Interval _11963 = direction.y.v;
    Interval _12016 = jet_mul_derivative(_11962, _11963, intervalFailed, optical_product_upper);
    Interval _11964 = _12016;
    Interval _11965 = n.y.v;
    Interval _11966 = direction.y.dx;
    Interval _12017 = jet_mul_derivative(_11965, _11966, intervalFailed, optical_product_upper);
    Interval _11967 = _12017;
    Interval _12018 = jet_add_derivative(_11964, _11967, intervalFailed);
    Interval _11968 = n.y.dy;
    Interval _11969 = direction.y.v;
    Interval _12019 = jet_mul_derivative(_11968, _11969, intervalFailed, optical_product_upper);
    Interval _11970 = _12019;
    Interval _11971 = n.y.v;
    Interval _11972 = direction.y.dy;
    Interval _12020 = jet_mul_derivative(_11971, _11972, intervalFailed, optical_product_upper);
    Interval _11973 = _12020;
    Interval _12021 = jet_add_derivative(_11970, _11973, intervalFailed);
    Interval _11954 = _12002;
    Interval _11955 = _12015;
    Interval _12022 = iadd(_11954, _11955, intervalFailed);
    Interval _11956 = _12005;
    Interval _11957 = _12018;
    Interval _12023 = jet_add_derivative(_11956, _11957, intervalFailed);
    Interval _11958 = _12008;
    Interval _11959 = _12021;
    Interval _12024 = jet_add_derivative(_11958, _11959, intervalFailed);
    Interval _11940 = n.z.v;
    Interval _11941 = direction.z.v;
    Interval _12031 = imul(_11940, _11941, intervalFailed, optical_product_upper);
    Interval _11942 = n.z.dx;
    Interval _11943 = direction.z.v;
    Interval _12032 = jet_mul_derivative(_11942, _11943, intervalFailed, optical_product_upper);
    Interval _11944 = _12032;
    Interval _11945 = n.z.v;
    Interval _11946 = direction.z.dx;
    Interval _12033 = jet_mul_derivative(_11945, _11946, intervalFailed, optical_product_upper);
    Interval _11947 = _12033;
    Interval _12034 = jet_add_derivative(_11944, _11947, intervalFailed);
    Interval _11948 = n.z.dy;
    Interval _11949 = direction.z.v;
    Interval _12035 = jet_mul_derivative(_11948, _11949, intervalFailed, optical_product_upper);
    Interval _11950 = _12035;
    Interval _11951 = n.z.v;
    Interval _11952 = direction.z.dy;
    Interval _12036 = jet_mul_derivative(_11951, _11952, intervalFailed, optical_product_upper);
    Interval _11953 = _12036;
    Interval _12037 = jet_add_derivative(_11950, _11953, intervalFailed);
    Interval _11934 = _12022;
    Interval _11935 = _12031;
    Interval _12038 = iadd(_11934, _11935, intervalFailed);
    Interval _11936 = _12023;
    Interval _11937 = _12034;
    __attribute__((unused)) Interval _12039 = jet_add_derivative(_11936, _11937, intervalFailed);
    Interval _11938 = _12024;
    Interval _11939 = _12037;
    __attribute__((unused)) Interval _12040 = jet_add_derivative(_11938, _11939, intervalFailed);
    if (_12038.lo > 0.0)
    {
        Interval _11920 = n.x.v;
        Interval _11921 = Interval{ -1.0, -1.0 };
        Interval _12051 = imul(_11920, _11921, intervalFailed, optical_product_upper);
        Interval _11922 = n.x.dx;
        Interval _11923 = Interval{ -1.0, -1.0 };
        Interval _12052 = jet_mul_derivative(_11922, _11923, intervalFailed, optical_product_upper);
        Interval _11924 = _12052;
        Interval _11925 = n.x.v;
        Interval _11926 = Interval{ 0.0, 0.0 };
        Interval _12053 = jet_mul_derivative(_11925, _11926, intervalFailed, optical_product_upper);
        Interval _11927 = _12053;
        Interval _12054 = jet_add_derivative(_11924, _11927, intervalFailed);
        Interval _11928 = n.x.dy;
        Interval _11929 = Interval{ -1.0, -1.0 };
        Interval _12055 = jet_mul_derivative(_11928, _11929, intervalFailed, optical_product_upper);
        Interval _11930 = _12055;
        Interval _11931 = n.x.v;
        Interval _11932 = Interval{ 0.0, 0.0 };
        Interval _12056 = jet_mul_derivative(_11931, _11932, intervalFailed, optical_product_upper);
        Interval _11933 = _12056;
        Interval _12057 = jet_add_derivative(_11930, _11933, intervalFailed);
        Interval _11906 = n.y.v;
        Interval _11907 = Interval{ -1.0, -1.0 };
        Interval _12061 = imul(_11906, _11907, intervalFailed, optical_product_upper);
        Interval _11908 = n.y.dx;
        Interval _11909 = Interval{ -1.0, -1.0 };
        Interval _12062 = jet_mul_derivative(_11908, _11909, intervalFailed, optical_product_upper);
        Interval _11910 = _12062;
        Interval _11911 = n.y.v;
        Interval _11912 = Interval{ 0.0, 0.0 };
        Interval _12063 = jet_mul_derivative(_11911, _11912, intervalFailed, optical_product_upper);
        Interval _11913 = _12063;
        Interval _12064 = jet_add_derivative(_11910, _11913, intervalFailed);
        Interval _11914 = n.y.dy;
        Interval _11915 = Interval{ -1.0, -1.0 };
        Interval _12065 = jet_mul_derivative(_11914, _11915, intervalFailed, optical_product_upper);
        Interval _11916 = _12065;
        Interval _11917 = n.y.v;
        Interval _11918 = Interval{ 0.0, 0.0 };
        Interval _12066 = jet_mul_derivative(_11917, _11918, intervalFailed, optical_product_upper);
        Interval _11919 = _12066;
        Interval _12067 = jet_add_derivative(_11916, _11919, intervalFailed);
        Interval _11892 = n.z.v;
        Interval _11893 = Interval{ -1.0, -1.0 };
        Interval _12071 = imul(_11892, _11893, intervalFailed, optical_product_upper);
        Interval _11894 = n.z.dx;
        Interval _11895 = Interval{ -1.0, -1.0 };
        Interval _12072 = jet_mul_derivative(_11894, _11895, intervalFailed, optical_product_upper);
        Interval _11896 = _12072;
        Interval _11897 = n.z.v;
        Interval _11898 = Interval{ 0.0, 0.0 };
        Interval _12073 = jet_mul_derivative(_11897, _11898, intervalFailed, optical_product_upper);
        Interval _11899 = _12073;
        Interval _12074 = jet_add_derivative(_11896, _11899, intervalFailed);
        Interval _11900 = n.z.dy;
        Interval _11901 = Interval{ -1.0, -1.0 };
        Interval _12075 = jet_mul_derivative(_11900, _11901, intervalFailed, optical_product_upper);
        Interval _11902 = _12075;
        Interval _11903 = n.z.v;
        Interval _11904 = Interval{ 0.0, 0.0 };
        Interval _12076 = jet_mul_derivative(_11903, _11904, intervalFailed, optical_product_upper);
        Interval _11905 = _12076;
        Interval _12077 = jet_add_derivative(_11902, _11905, intervalFailed);
        return OpticalJet3{ OpticalJet{ _12051, _12054, _12057 }, OpticalJet{ _12061, _12064, _12067 }, OpticalJet{ _12071, _12074, _12077 } };
    }
    if (_12038.hi < 0.0)
    {
        return n;
    }
    jetBranchKnown = false;
    return n;
}

static inline __attribute__((always_inline))
bool jet_inside_face(thread const ReflectionSpecularPlane& plane, thread const OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _15811;
    if (plane.a.w == 2.0)
    {
        _15811 = plane.b.w == 2.0;
    }
    else
    {
        _15811 = false;
    }
    bool _15816;
    if (_15811)
    {
        _15816 = plane.c.w == 2.0;
    }
    else
    {
        _15816 = false;
    }
    if (_15816)
    {
        return true;
    }
    Interval _15795 = Interval{ plane.b.x, plane.b.x };
    Interval _15796 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15847 = iadd(_15795, _15796, intervalFailed);
    Interval _15797 = Interval{ plane.b.y, plane.b.y };
    Interval _15798 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15850 = iadd(_15797, _15798, intervalFailed);
    Interval _15799 = Interval{ plane.b.z, plane.b.z };
    Interval _15800 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15853 = iadd(_15799, _15800, intervalFailed);
    Interval _15789 = Interval{ plane.c.x, plane.c.x };
    Interval _15790 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15884 = iadd(_15789, _15790, intervalFailed);
    Interval _15791 = Interval{ plane.c.y, plane.c.y };
    Interval _15792 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15887 = iadd(_15791, _15792, intervalFailed);
    Interval _15793 = Interval{ plane.c.z, plane.c.z };
    Interval _15794 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15890 = iadd(_15793, _15794, intervalFailed);
    Interval _15783 = hit.x.v;
    Interval _15784 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15921 = iadd(_15783, _15784, intervalFailed);
    Interval _15785 = hit.y.v;
    Interval _15786 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15923 = iadd(_15785, _15786, intervalFailed);
    Interval _15787 = hit.z.v;
    Interval _15788 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15925 = iadd(_15787, _15788, intervalFailed);
    Interval _15773 = _15847;
    Interval _15774 = _15847;
    Interval _15926 = imul(_15773, _15774, intervalFailed, optical_product_upper);
    Interval _15775 = _15926;
    Interval _15776 = _15850;
    Interval _15777 = _15850;
    Interval _15927 = imul(_15776, _15777, intervalFailed, optical_product_upper);
    Interval _15778 = _15927;
    Interval _15928 = iadd(_15775, _15778, intervalFailed);
    Interval _15779 = _15928;
    Interval _15780 = _15853;
    Interval _15781 = _15853;
    Interval _15929 = imul(_15780, _15781, intervalFailed, optical_product_upper);
    Interval _15782 = _15929;
    Interval _15930 = iadd(_15779, _15782, intervalFailed);
    Interval _15763 = _15847;
    Interval _15764 = _15884;
    Interval _15931 = imul(_15763, _15764, intervalFailed, optical_product_upper);
    Interval _15765 = _15931;
    Interval _15766 = _15850;
    Interval _15767 = _15887;
    Interval _15932 = imul(_15766, _15767, intervalFailed, optical_product_upper);
    Interval _15768 = _15932;
    Interval _15933 = iadd(_15765, _15768, intervalFailed);
    Interval _15769 = _15933;
    Interval _15770 = _15853;
    Interval _15771 = _15890;
    Interval _15934 = imul(_15770, _15771, intervalFailed, optical_product_upper);
    Interval _15772 = _15934;
    Interval _15935 = iadd(_15769, _15772, intervalFailed);
    Interval _15753 = _15884;
    Interval _15754 = _15884;
    Interval _15936 = imul(_15753, _15754, intervalFailed, optical_product_upper);
    Interval _15755 = _15936;
    Interval _15756 = _15887;
    Interval _15757 = _15887;
    Interval _15937 = imul(_15756, _15757, intervalFailed, optical_product_upper);
    Interval _15758 = _15937;
    Interval _15938 = iadd(_15755, _15758, intervalFailed);
    Interval _15759 = _15938;
    Interval _15760 = _15890;
    Interval _15761 = _15890;
    Interval _15939 = imul(_15760, _15761, intervalFailed, optical_product_upper);
    Interval _15762 = _15939;
    Interval _15940 = iadd(_15759, _15762, intervalFailed);
    Interval _15743 = _15921;
    Interval _15744 = _15847;
    Interval _15941 = imul(_15743, _15744, intervalFailed, optical_product_upper);
    Interval _15745 = _15941;
    Interval _15746 = _15923;
    Interval _15747 = _15850;
    Interval _15942 = imul(_15746, _15747, intervalFailed, optical_product_upper);
    Interval _15748 = _15942;
    Interval _15943 = iadd(_15745, _15748, intervalFailed);
    Interval _15749 = _15943;
    Interval _15750 = _15925;
    Interval _15751 = _15853;
    Interval _15944 = imul(_15750, _15751, intervalFailed, optical_product_upper);
    Interval _15752 = _15944;
    Interval _15945 = iadd(_15749, _15752, intervalFailed);
    Interval _15733 = _15921;
    Interval _15734 = _15884;
    Interval _15946 = imul(_15733, _15734, intervalFailed, optical_product_upper);
    Interval _15735 = _15946;
    Interval _15736 = _15923;
    Interval _15737 = _15887;
    Interval _15947 = imul(_15736, _15737, intervalFailed, optical_product_upper);
    Interval _15738 = _15947;
    Interval _15948 = iadd(_15735, _15738, intervalFailed);
    Interval _15739 = _15948;
    Interval _15740 = _15925;
    Interval _15741 = _15890;
    Interval _15949 = imul(_15740, _15741, intervalFailed, optical_product_upper);
    Interval _15742 = _15949;
    Interval _15950 = iadd(_15739, _15742, intervalFailed);
    Interval param_var_a = _15930;
    Interval param_var_b = _15940;
    Interval _15951 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    bool _15958;
    if (_15935.lo <= 0.0)
    {
        _15958 = _15935.hi >= 0.0;
    }
    else
    {
        _15958 = false;
    }
    float _15965;
    if (_15958)
    {
        _15965 = 0.0;
    }
    else
    {
        _15965 = precise::min(abs(_15935.lo), abs(_15935.hi));
    }
    float _15968 = precise::max(abs(_15935.lo), abs(_15935.hi));
    float _15731 = spvFMul(_15965, _15965);
    float _15969 = interval_down(_15731, intervalFailed);
    float _15732 = spvFMul(_15968, _15968);
    float _15971 = interval_up(_15732, intervalFailed);
    Interval _15729 = _15951;
    Interval _15730 = Interval{ as_type<float>(as_type<uint>(_15971) ^ 2147483648u), as_type<float>(as_type<uint>(precise::max(0.0, _15969)) ^ 2147483648u) };
    Interval _15979 = iadd(_15729, _15730, intervalFailed);
    if (_15979.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_1 = _15940;
    Interval param_var_b_1 = _15945;
    Interval _15982 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    Interval param_var_a_2 = _15935;
    Interval param_var_b_2 = _15950;
    Interval _15983 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _15727 = _15982;
    Interval _15728 = Interval{ as_type<float>(as_type<uint>(_15983.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15983.lo) ^ 2147483648u) };
    Interval _15993 = iadd(_15727, _15728, intervalFailed);
    Interval param_var_a_3 = _15993;
    Interval param_var_b_3 = _15979;
    Interval _15994 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_a_4 = _15930;
    Interval param_var_b_4 = _15950;
    Interval _15996 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _15935;
    Interval param_var_b_5 = _15945;
    Interval _15997 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval _15725 = _15996;
    Interval _15726 = Interval{ as_type<float>(as_type<uint>(_15997.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15997.lo) ^ 2147483648u) };
    Interval _16007 = iadd(_15725, _15726, intervalFailed);
    Interval param_var_a_6 = _16007;
    Interval param_var_b_6 = _15979;
    Interval _16008 = idiv(param_var_a_6, param_var_b_6, intervalFailed, interval_divide_upper);
    float _15723 = -9.9999997473787516355514526367188e-06;
    __attribute__((unused)) float _16010 = interval_down(_15723, intervalFailed);
    float _15724 = -9.9999997473787516355514526367188e-06;
    float _16011 = interval_up(_15724, intervalFailed);
    bool _16016;
    if (_15994.lo >= _16011)
    {
        float _15721 = -9.9999997473787516355514526367188e-06;
        __attribute__((unused)) float _16013 = interval_down(_15721, intervalFailed);
        float _15722 = -9.9999997473787516355514526367188e-06;
        float _16014 = interval_up(_15722, intervalFailed);
        _16016 = _16008.lo >= _16014;
    }
    else
    {
        _16016 = false;
    }
    bool _16022;
    if (_16016)
    {
        Interval param_var_a_7 = _15994;
        Interval param_var_b_7 = _16008;
        Interval _16017 = iadd(param_var_a_7, param_var_b_7, intervalFailed);
        float _15719 = 1.000010013580322265625;
        float _16019 = interval_down(_15719, intervalFailed);
        float _15720 = 1.000010013580322265625;
        __attribute__((unused)) float _16020 = interval_up(_15720, intervalFailed);
        _16022 = _16017.hi <= _16019;
    }
    else
    {
        _16022 = false;
    }
    return _16022;
}

static inline __attribute__((always_inline))
OpticalJet jabs(thread const OpticalJet& a)
{
    if (a.v.lo >= 0.0)
    {
        return a;
    }
    if (a.v.hi <= 0.0)
    {
        return OpticalJet{ Interval{ as_type<float>(as_type<uint>(a.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(a.v.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(a.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(a.dx.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(a.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(a.dy.lo) ^ 2147483648u) } };
    }
    bool _12162;
    if (a.v.lo <= 0.0)
    {
        _12162 = a.v.hi >= 0.0;
    }
    else
    {
        _12162 = false;
    }
    float _12169;
    if (_12162)
    {
        _12169 = 0.0;
    }
    else
    {
        _12169 = precise::min(abs(a.v.lo), abs(a.v.hi));
    }
    return OpticalJet{ Interval{ _12169, precise::max(abs(a.v.lo), abs(a.v.hi)) }, Interval{ precise::min(a.dx.lo, as_type<float>(as_type<uint>(a.dx.hi) ^ 2147483648u)), precise::max(a.dx.hi, as_type<float>(as_type<uint>(a.dx.lo) ^ 2147483648u)) }, Interval{ precise::min(a.dy.lo, as_type<float>(as_type<uint>(a.dy.hi) ^ 2147483648u)), precise::max(a.dy.hi, as_type<float>(as_type<uint>(a.dy.lo) ^ 2147483648u)) } };
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        Interval param_var_a = Interval{ n, n };
        Interval param_var_b = Interval{ as_type<float>(1036831948u), as_type<float>(1036831950u) };
        Interval _16750 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _16750;
    }
    if (d == 100.0)
    {
        Interval param_var_a_1 = Interval{ n, n };
        Interval param_var_b_1 = Interval{ as_type<float>(1008981769u), as_type<float>(1008981771u) };
        Interval _16758 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _16758;
    }
    if (d == 1000.0)
    {
        Interval param_var_a_2 = Interval{ n, n };
        Interval param_var_b_2 = Interval{ as_type<float>(981668462u), as_type<float>(981668464u) };
        Interval _16766 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _16766;
    }
    if (d == 10000.0)
    {
        Interval param_var_a_3 = Interval{ n, n };
        Interval param_var_b_3 = Interval{ as_type<float>(953267990u), as_type<float>(953267992u) };
        Interval _16774 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _16774;
    }
    if (d == 100000.0)
    {
        Interval param_var_a_4 = Interval{ n, n };
        Interval param_var_b_4 = Interval{ as_type<float>(925353387u), as_type<float>(925353389u) };
        Interval _16782 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _16782;
    }
    if (d == 128.0)
    {
        Interval param_var_a_5 = Interval{ n, n };
        Interval param_var_b_5 = Interval{ as_type<float>(1006632960u), as_type<float>(1006632960u) };
        Interval _16790 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _16790;
    }
    if (d == 65535.0)
    {
        Interval param_var_a_6 = Interval{ n, n };
        Interval param_var_b_6 = Interval{ as_type<float>(931135615u), as_type<float>(931135617u) };
        Interval _16798 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _16798;
    }
    Interval param_var_a_7 = Interval{ n, n };
    Interval param_var_b_7 = Interval{ d, d };
    Interval _16803 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _16803;
}

static inline __attribute__((always_inline))
OpticalJet jmax(thread const OpticalJet& a, thread const OpticalJet& b)
{
    if (a.v.lo > b.v.hi)
    {
        return a;
    }
    if (b.v.lo > a.v.hi)
    {
        return b;
    }
    return OpticalJet{ Interval{ precise::max(a.v.lo, b.v.lo), precise::max(a.v.hi, b.v.hi) }, Interval{ precise::min(a.dx.lo, b.dx.lo), precise::max(a.dx.hi, b.dx.hi) }, Interval{ precise::min(a.dy.lo, b.dy.lo), precise::max(a.dy.hi, b.dy.hi) } };
}

static inline __attribute__((always_inline))
Interval isin_body(thread const Interval& a, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _21813;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _21813 = isnan(a.hi) || isinf(a.hi);
    }
    else
    {
        _21813 = true;
    }
    bool _21823;
    if (!_21813)
    {
        _21823 = precise::max(abs(a.lo), abs(a.hi)) > 1048576.0;
    }
    else
    {
        _21823 = true;
    }
    if (_21823)
    {
        intervalFailed = true;
        return Interval{ -1.0, 1.0 };
    }
    float _1801 = spvFMul(floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5)), 2.0);
    Interval param_var_a = Interval{ _1801, _1801 };
    float _21800 = 3.1415927410125732421875;
    float _21831 = interval_down(_21800, intervalFailed);
    float _21801 = 3.1415927410125732421875;
    float _21832 = interval_up(_21801, intervalFailed);
    Interval param_var_b = Interval{ _21831, _21832 };
    Interval _21834 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    Interval _21798 = a;
    Interval _21799 = Interval{ as_type<float>(as_type<uint>(_21834.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21834.lo) ^ 2147483648u) };
    Interval _21844 = iadd(_21798, _21799, intervalFailed);
    float _21796 = 3.1415927410125732421875;
    float _21847 = interval_down(_21796, intervalFailed);
    float _21797 = 3.1415927410125732421875;
    float _21848 = interval_up(_21797, intervalFailed);
    Interval param_var_a_1 = Interval{ _21847, _21848 };
    Interval param_var_b_1 = Interval{ 0.5, 0.5 };
    Interval _21850 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    float _21892;
    float _21893;
    if (_21844.lo > _21850.hi)
    {
        float _21794 = 3.1415927410125732421875;
        float _21877 = interval_down(_21794, intervalFailed);
        float _21795 = 3.1415927410125732421875;
        float _21878 = interval_up(_21795, intervalFailed);
        Interval _21792 = Interval{ _21877, _21878 };
        Interval _21793 = Interval{ as_type<float>(as_type<uint>(_21844.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21844.lo) ^ 2147483648u) };
        Interval _21889 = iadd(_21792, _21793, intervalFailed);
        _21892 = _21889.hi;
        _21893 = _21889.lo;
    }
    else
    {
        float _21875;
        float _21876;
        if (_21844.hi < (-_21850.hi))
        {
            float _21790 = 3.1415927410125732421875;
            float _21854 = interval_down(_21790, intervalFailed);
            float _21791 = 3.1415927410125732421875;
            float _21855 = interval_up(_21791, intervalFailed);
            Interval _21788 = Interval{ as_type<float>(as_type<uint>(_21855) ^ 2147483648u), as_type<float>(as_type<uint>(_21854) ^ 2147483648u) };
            Interval _21789 = Interval{ as_type<float>(as_type<uint>(_21844.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21844.lo) ^ 2147483648u) };
            Interval _21872 = iadd(_21788, _21789, intervalFailed);
            _21875 = _21872.hi;
            _21876 = _21872.lo;
        }
        else
        {
            _21875 = _21844.hi;
            _21876 = _21844.lo;
        }
        _21892 = _21875;
        _21893 = _21876;
    }
    float _1803 = -_21850.hi;
    bool _21896;
    if ((isunordered(_21893, _1803) || _21893 >= _1803))
    {
        _21896 = _21892 > _21850.hi;
    }
    else
    {
        _21896 = true;
    }
    if (_21896)
    {
        return Interval{ -1.0, 1.0 };
    }
    bool _21901;
    if (_21893 <= 0.0)
    {
        _21901 = _21892 >= 0.0;
    }
    else
    {
        _21901 = false;
    }
    float _21908;
    if (_21901)
    {
        _21908 = 0.0;
    }
    else
    {
        _21908 = precise::min(abs(_21893), abs(_21892));
    }
    float _21911 = precise::max(abs(_21893), abs(_21892));
    float _21786 = spvFMul(_21908, _21908);
    float _21912 = interval_down(_21786, intervalFailed);
    float _21913 = precise::max(0.0, _21912);
    float _21787 = spvFMul(_21911, _21911);
    float _21914 = interval_up(_21787, intervalFailed);
    float _21784 = _2368[8];
    float _21917 = interval_down(_21784, intervalFailed);
    float _21785 = _2368[8];
    float _21918 = interval_up(_21785, intervalFailed);
    float _21919;
    float _21921;
    _21919 = _21917;
    _21921 = _21918;
    float _21920;
    float _21922;
    for (int _21923 = 7; _21923 >= 0; _21919 = _21920, _21921 = _21922, _21923--)
    {
        Interval param_var_a_2 = Interval{ _21919, _21921 };
        Interval param_var_b_2 = Interval{ _21913, _21914 };
        Interval _21927 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        Interval param_var_a_3 = _21927;
        float _21782 = _2368[_21923];
        float _21930 = interval_down(_21782, intervalFailed);
        float _21783 = _2368[_21923];
        float _21931 = interval_up(_21783, intervalFailed);
        Interval param_var_b_3 = Interval{ _21930, _21931 };
        Interval _21933 = iadd(param_var_a_3, param_var_b_3, intervalFailed);
        _21920 = _21933.lo;
        _21922 = _21933.hi;
    }
    Interval param_var_a_4 = Interval{ _21893, _21892 };
    Interval param_var_b_4 = Interval{ _21919, _21921 };
    Interval _21936 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _21936;
    Interval param_var_b_5 = Interval{ -3.9999999840167888010000751819462e-12, 3.9999999840167888010000751819462e-12 };
    Interval _21937 = iadd(param_var_a_5, param_var_b_5, intervalFailed);
    return Interval{ precise::min(precise::max(_21937.lo, -1.0), 1.0), precise::min(precise::max(_21937.hi, -1.0), 1.0) };
}

static __attribute__((noinline))
float sine_bounds(thread const float& lo, thread const float& hi, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_sine_upper)
{
    Interval param_var_a = Interval{ lo, hi };
    Interval _21779 = isin_body(param_var_a, intervalFailed, optical_product_upper);
    interval_sine_upper = _21779.hi;
    return _21779.lo;
}

static inline __attribute__((always_inline))
OpticalJet jmin(thread const OpticalJet& a, thread const OpticalJet& b)
{
    if (a.v.hi < b.v.lo)
    {
        return a;
    }
    if (b.v.hi < a.v.lo)
    {
        return b;
    }
    return OpticalJet{ Interval{ precise::min(a.v.lo, b.v.lo), precise::min(a.v.hi, b.v.hi) }, Interval{ precise::min(a.dx.lo, b.dx.lo), precise::max(a.dx.hi, b.dx.hi) }, Interval{ precise::min(a.dy.lo, b.dy.lo), precise::max(a.dy.hi, b.dy.hi) } };
}

static __attribute__((noinline))
OpticalJet3 jlava(thread const OpticalJet& x, thread const OpticalJet& z, thread const OpticalJet& t, thread const OpticalJet& footprint, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    float _18604 = 6.0;
    float _18605 = 1000.0;
    Interval _18611 = iratio(_18604, _18605, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18616;
    if (!intervalFailed)
    {
        _18616 = intervalFailed;
    }
    else
    {
        _18616 = false;
    }
    bool _18621;
    if (_18616)
    {
        _18621 = jetFailureSite == 0u;
    }
    else
    {
        _18621 = false;
    }
    if (_18621)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6.0, 6.0, 1000.0, 1000.0);
    }
    Interval _18590 = x.v;
    Interval _18591 = _18611;
    Interval _18624 = imul(_18590, _18591, intervalFailed, optical_product_upper);
    Interval _18592 = x.dx;
    Interval _18593 = _18611;
    Interval _18625 = jet_mul_derivative(_18592, _18593, intervalFailed, optical_product_upper);
    Interval _18594 = _18625;
    Interval _18595 = x.v;
    Interval _18596 = Interval{ 0.0, 0.0 };
    Interval _18626 = jet_mul_derivative(_18595, _18596, intervalFailed, optical_product_upper);
    Interval _18597 = _18626;
    Interval _18627 = jet_add_derivative(_18594, _18597, intervalFailed);
    Interval _18598 = x.dy;
    Interval _18599 = _18611;
    Interval _18628 = jet_mul_derivative(_18598, _18599, intervalFailed, optical_product_upper);
    Interval _18600 = _18628;
    Interval _18601 = x.v;
    Interval _18602 = Interval{ 0.0, 0.0 };
    Interval _18629 = jet_mul_derivative(_18601, _18602, intervalFailed, optical_product_upper);
    Interval _18603 = _18629;
    Interval _18630 = jet_add_derivative(_18600, _18603, intervalFailed);
    float _18588 = 8.0;
    float _18589 = 1000.0;
    Interval _18636 = iratio(_18588, _18589, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18641;
    if (!intervalFailed)
    {
        _18641 = intervalFailed;
    }
    else
    {
        _18641 = false;
    }
    bool _18646;
    if (_18641)
    {
        _18646 = jetFailureSite == 0u;
    }
    else
    {
        _18646 = false;
    }
    if (_18646)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(8.0, 8.0, 1000.0, 1000.0);
    }
    Interval _18574 = z.v;
    Interval _18575 = _18636;
    Interval _18649 = imul(_18574, _18575, intervalFailed, optical_product_upper);
    Interval _18576 = z.dx;
    Interval _18577 = _18636;
    Interval _18650 = jet_mul_derivative(_18576, _18577, intervalFailed, optical_product_upper);
    Interval _18578 = _18650;
    Interval _18579 = z.v;
    Interval _18580 = Interval{ 0.0, 0.0 };
    Interval _18651 = jet_mul_derivative(_18579, _18580, intervalFailed, optical_product_upper);
    Interval _18581 = _18651;
    Interval _18652 = jet_add_derivative(_18578, _18581, intervalFailed);
    Interval _18582 = z.dy;
    Interval _18583 = _18636;
    Interval _18653 = jet_mul_derivative(_18582, _18583, intervalFailed, optical_product_upper);
    Interval _18584 = _18653;
    Interval _18585 = z.v;
    Interval _18586 = Interval{ 0.0, 0.0 };
    Interval _18654 = jet_mul_derivative(_18585, _18586, intervalFailed, optical_product_upper);
    Interval _18587 = _18654;
    Interval _18655 = jet_add_derivative(_18584, _18587, intervalFailed);
    Interval _18568 = _18624;
    Interval _18569 = Interval{ as_type<float>(as_type<uint>(_18649.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18649.lo) ^ 2147483648u) };
    Interval _18681 = iadd(_18568, _18569, intervalFailed);
    Interval _18570 = _18627;
    Interval _18571 = Interval{ as_type<float>(as_type<uint>(_18652.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18652.lo) ^ 2147483648u) };
    Interval _18683 = jet_add_derivative(_18570, _18571, intervalFailed);
    Interval _18572 = _18630;
    Interval _18573 = Interval{ as_type<float>(as_type<uint>(_18655.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18655.lo) ^ 2147483648u) };
    Interval _18685 = jet_add_derivative(_18572, _18573, intervalFailed);
    float _18566 = 11.0;
    float _18567 = 100.0;
    Interval _18691 = iratio(_18566, _18567, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18696;
    if (!intervalFailed)
    {
        _18696 = intervalFailed;
    }
    else
    {
        _18696 = false;
    }
    bool _18701;
    if (_18696)
    {
        _18701 = jetFailureSite == 0u;
    }
    else
    {
        _18701 = false;
    }
    if (_18701)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 100.0, 100.0);
    }
    Interval _18552 = t.v;
    Interval _18553 = _18691;
    Interval _18704 = imul(_18552, _18553, intervalFailed, optical_product_upper);
    Interval _18554 = t.dx;
    Interval _18555 = _18691;
    Interval _18705 = jet_mul_derivative(_18554, _18555, intervalFailed, optical_product_upper);
    Interval _18556 = _18705;
    Interval _18557 = t.v;
    Interval _18558 = Interval{ 0.0, 0.0 };
    Interval _18706 = jet_mul_derivative(_18557, _18558, intervalFailed, optical_product_upper);
    Interval _18559 = _18706;
    Interval _18707 = jet_add_derivative(_18556, _18559, intervalFailed);
    Interval _18560 = t.dy;
    Interval _18561 = _18691;
    Interval _18708 = jet_mul_derivative(_18560, _18561, intervalFailed, optical_product_upper);
    Interval _18562 = _18708;
    Interval _18563 = t.v;
    Interval _18564 = Interval{ 0.0, 0.0 };
    Interval _18709 = jet_mul_derivative(_18563, _18564, intervalFailed, optical_product_upper);
    Interval _18565 = _18709;
    Interval _18710 = jet_add_derivative(_18562, _18565, intervalFailed);
    Interval _18546 = _18681;
    Interval _18547 = _18704;
    Interval _18711 = iadd(_18546, _18547, intervalFailed);
    Interval _18548 = _18683;
    Interval _18549 = _18707;
    Interval _18712 = jet_add_derivative(_18548, _18549, intervalFailed);
    Interval _18550 = _18685;
    Interval _18551 = _18710;
    Interval _18713 = jet_add_derivative(_18550, _18551, intervalFailed);
    float _18544 = 10.0;
    float _18545 = 1000.0;
    Interval _18716 = iratio(_18544, _18545, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18721;
    if (!intervalFailed)
    {
        _18721 = intervalFailed;
    }
    else
    {
        _18721 = false;
    }
    bool _18726;
    if (_18721)
    {
        _18726 = jetFailureSite == 0u;
    }
    else
    {
        _18726 = false;
    }
    if (_18726)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(10.0, 10.0, 1000.0, 1000.0);
    }
    Interval _18530 = footprint.v;
    Interval _18531 = _18716;
    Interval _18732 = imul(_18530, _18531, intervalFailed, optical_product_upper);
    Interval _18532 = footprint.dx;
    Interval _18533 = _18716;
    Interval _18733 = jet_mul_derivative(_18532, _18533, intervalFailed, optical_product_upper);
    Interval _18534 = _18733;
    Interval _18535 = footprint.v;
    Interval _18536 = Interval{ 0.0, 0.0 };
    Interval _18734 = jet_mul_derivative(_18535, _18536, intervalFailed, optical_product_upper);
    Interval _18537 = _18734;
    Interval _18735 = jet_add_derivative(_18534, _18537, intervalFailed);
    Interval _18538 = footprint.dy;
    Interval _18539 = _18716;
    Interval _18736 = jet_mul_derivative(_18538, _18539, intervalFailed, optical_product_upper);
    Interval _18540 = _18736;
    Interval _18541 = footprint.v;
    Interval _18542 = Interval{ 0.0, 0.0 };
    Interval _18737 = jet_mul_derivative(_18541, _18542, intervalFailed, optical_product_upper);
    Interval _18543 = _18737;
    Interval _18738 = jet_add_derivative(_18540, _18543, intervalFailed);
    bool _18745;
    if (_18732.lo <= 0.0)
    {
        _18745 = _18732.hi >= 0.0;
    }
    else
    {
        _18745 = false;
    }
    float _18752;
    if (_18745)
    {
        _18752 = 0.0;
    }
    else
    {
        _18752 = precise::min(abs(_18732.lo), abs(_18732.hi));
    }
    float _18755 = precise::max(abs(_18732.lo), abs(_18732.hi));
    float _18520 = spvFMul(_18752, _18752);
    float _18756 = interval_down(_18520, intervalFailed);
    float _18757 = precise::max(0.0, _18756);
    float _18521 = spvFMul(_18755, _18755);
    float _18758 = interval_up(_18521, intervalFailed);
    Interval _18522 = Interval{ 2.0, 2.0 };
    Interval _18523 = _18732;
    Interval _18759 = imul(_18522, _18523, intervalFailed, optical_product_upper);
    Interval _18524 = _18759;
    Interval _18525 = _18735;
    Interval _18760 = jet_mul_derivative(_18524, _18525, intervalFailed, optical_product_upper);
    Interval _18526 = Interval{ 2.0, 2.0 };
    Interval _18527 = _18732;
    Interval _18761 = imul(_18526, _18527, intervalFailed, optical_product_upper);
    Interval _18528 = _18761;
    Interval _18529 = _18738;
    Interval _18762 = jet_mul_derivative(_18528, _18529, intervalFailed, optical_product_upper);
    bool _18767;
    if (_18757 <= 0.0)
    {
        _18767 = _18758 >= 0.0;
    }
    else
    {
        _18767 = false;
    }
    float _18774;
    if (_18767)
    {
        _18774 = 0.0;
    }
    else
    {
        _18774 = precise::min(abs(_18757), abs(_18758));
    }
    float _18777 = precise::max(abs(_18757), abs(_18758));
    float _18510 = spvFMul(_18774, _18774);
    float _18778 = interval_down(_18510, intervalFailed);
    float _18511 = spvFMul(_18777, _18777);
    float _18780 = interval_up(_18511, intervalFailed);
    Interval _18512 = Interval{ 2.0, 2.0 };
    Interval _18513 = Interval{ _18757, _18758 };
    Interval _18782 = imul(_18512, _18513, intervalFailed, optical_product_upper);
    Interval _18514 = _18782;
    Interval _18515 = _18760;
    Interval _18783 = jet_mul_derivative(_18514, _18515, intervalFailed, optical_product_upper);
    Interval _18516 = Interval{ 2.0, 2.0 };
    Interval _18517 = Interval{ _18757, _18758 };
    Interval _18785 = imul(_18516, _18517, intervalFailed, optical_product_upper);
    Interval _18518 = _18785;
    Interval _18519 = _18762;
    Interval _18786 = jet_mul_derivative(_18518, _18519, intervalFailed, optical_product_upper);
    Interval _18504 = Interval{ 1.0, 1.0 };
    Interval _18505 = Interval{ precise::max(0.0, _18778), _18780 };
    Interval _18788 = iadd(_18504, _18505, intervalFailed);
    Interval _18506 = Interval{ 0.0, 0.0 };
    Interval _18507 = _18783;
    Interval _18789 = jet_add_derivative(_18506, _18507, intervalFailed);
    Interval _18508 = Interval{ 0.0, 0.0 };
    Interval _18509 = _18786;
    Interval _18790 = jet_add_derivative(_18508, _18509, intervalFailed);
    Interval _18490 = Interval{ 1.0, 1.0 };
    Interval _18491 = _18788;
    Interval _18792 = idiv(_18490, _18491, intervalFailed, interval_divide_upper);
    bool _18799;
    if (!intervalFailed)
    {
        _18799 = intervalFailed;
    }
    else
    {
        _18799 = false;
    }
    bool _18804;
    if (_18799)
    {
        _18804 = jetFailureSite == 0u;
    }
    else
    {
        _18804 = false;
    }
    if (_18804)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _18788.lo, _18788.hi);
    }
    Interval _18492 = Interval{ 0.0, 0.0 };
    Interval _18493 = _18792;
    Interval _18494 = _18789;
    Interval _18808 = jet_mul_derivative(_18493, _18494, intervalFailed, optical_product_upper);
    Interval _18495 = Interval{ as_type<float>(as_type<uint>(_18808.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18808.lo) ^ 2147483648u) };
    Interval _18818 = jet_add_derivative(_18492, _18495, intervalFailed);
    Interval _18496 = _18818;
    Interval _18497 = _18788;
    Interval _18819 = jet_div_derivative(_18496, _18497, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18498 = Interval{ 0.0, 0.0 };
    Interval _18499 = _18792;
    Interval _18500 = _18790;
    Interval _18820 = jet_mul_derivative(_18499, _18500, intervalFailed, optical_product_upper);
    Interval _18501 = Interval{ as_type<float>(as_type<uint>(_18820.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18820.lo) ^ 2147483648u) };
    Interval _18830 = jet_add_derivative(_18498, _18501, intervalFailed);
    Interval _18502 = _18830;
    Interval _18503 = _18788;
    Interval _18831 = jet_div_derivative(_18502, _18503, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18488 = 18.0;
    float _18489 = 1000.0;
    Interval _18837 = iratio(_18488, _18489, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18842;
    if (!intervalFailed)
    {
        _18842 = intervalFailed;
    }
    else
    {
        _18842 = false;
    }
    bool _18847;
    if (_18842)
    {
        _18847 = jetFailureSite == 0u;
    }
    else
    {
        _18847 = false;
    }
    if (_18847)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    Interval _18474 = x.v;
    Interval _18475 = _18837;
    Interval _18850 = imul(_18474, _18475, intervalFailed, optical_product_upper);
    Interval _18476 = x.dx;
    Interval _18477 = _18837;
    Interval _18851 = jet_mul_derivative(_18476, _18477, intervalFailed, optical_product_upper);
    Interval _18478 = _18851;
    Interval _18479 = x.v;
    Interval _18480 = Interval{ 0.0, 0.0 };
    Interval _18852 = jet_mul_derivative(_18479, _18480, intervalFailed, optical_product_upper);
    Interval _18481 = _18852;
    Interval _18853 = jet_add_derivative(_18478, _18481, intervalFailed);
    Interval _18482 = x.dy;
    Interval _18483 = _18837;
    Interval _18854 = jet_mul_derivative(_18482, _18483, intervalFailed, optical_product_upper);
    Interval _18484 = _18854;
    Interval _18485 = x.v;
    Interval _18486 = Interval{ 0.0, 0.0 };
    Interval _18855 = jet_mul_derivative(_18485, _18486, intervalFailed, optical_product_upper);
    Interval _18487 = _18855;
    Interval _18856 = jet_add_derivative(_18484, _18487, intervalFailed);
    float _18472 = 11.0;
    float _18473 = 1000.0;
    Interval _18862 = iratio(_18472, _18473, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18867;
    if (!intervalFailed)
    {
        _18867 = intervalFailed;
    }
    else
    {
        _18867 = false;
    }
    bool _18872;
    if (_18867)
    {
        _18872 = jetFailureSite == 0u;
    }
    else
    {
        _18872 = false;
    }
    if (_18872)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    Interval _18458 = z.v;
    Interval _18459 = _18862;
    Interval _18875 = imul(_18458, _18459, intervalFailed, optical_product_upper);
    Interval _18460 = z.dx;
    Interval _18461 = _18862;
    Interval _18876 = jet_mul_derivative(_18460, _18461, intervalFailed, optical_product_upper);
    Interval _18462 = _18876;
    Interval _18463 = z.v;
    Interval _18464 = Interval{ 0.0, 0.0 };
    Interval _18877 = jet_mul_derivative(_18463, _18464, intervalFailed, optical_product_upper);
    Interval _18465 = _18877;
    Interval _18878 = jet_add_derivative(_18462, _18465, intervalFailed);
    Interval _18466 = z.dy;
    Interval _18467 = _18862;
    Interval _18879 = jet_mul_derivative(_18466, _18467, intervalFailed, optical_product_upper);
    Interval _18468 = _18879;
    Interval _18469 = z.v;
    Interval _18470 = Interval{ 0.0, 0.0 };
    Interval _18880 = jet_mul_derivative(_18469, _18470, intervalFailed, optical_product_upper);
    Interval _18471 = _18880;
    Interval _18881 = jet_add_derivative(_18468, _18471, intervalFailed);
    Interval _18452 = _18850;
    Interval _18453 = _18875;
    Interval _18882 = iadd(_18452, _18453, intervalFailed);
    Interval _18454 = _18853;
    Interval _18455 = _18878;
    Interval _18883 = jet_add_derivative(_18454, _18455, intervalFailed);
    Interval _18456 = _18856;
    Interval _18457 = _18881;
    Interval _18884 = jet_add_derivative(_18456, _18457, intervalFailed);
    float _18450 = 45.0;
    float _18451 = 100.0;
    Interval _18890 = iratio(_18450, _18451, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18895;
    if (!intervalFailed)
    {
        _18895 = intervalFailed;
    }
    else
    {
        _18895 = false;
    }
    bool _18900;
    if (_18895)
    {
        _18900 = jetFailureSite == 0u;
    }
    else
    {
        _18900 = false;
    }
    if (_18900)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 100.0, 100.0);
    }
    Interval _18436 = t.v;
    Interval _18437 = _18890;
    Interval _18903 = imul(_18436, _18437, intervalFailed, optical_product_upper);
    Interval _18438 = t.dx;
    Interval _18439 = _18890;
    Interval _18904 = jet_mul_derivative(_18438, _18439, intervalFailed, optical_product_upper);
    Interval _18440 = _18904;
    Interval _18441 = t.v;
    Interval _18442 = Interval{ 0.0, 0.0 };
    Interval _18905 = jet_mul_derivative(_18441, _18442, intervalFailed, optical_product_upper);
    Interval _18443 = _18905;
    Interval _18906 = jet_add_derivative(_18440, _18443, intervalFailed);
    Interval _18444 = t.dy;
    Interval _18445 = _18890;
    Interval _18907 = jet_mul_derivative(_18444, _18445, intervalFailed, optical_product_upper);
    Interval _18446 = _18907;
    Interval _18447 = t.v;
    Interval _18448 = Interval{ 0.0, 0.0 };
    Interval _18908 = jet_mul_derivative(_18447, _18448, intervalFailed, optical_product_upper);
    Interval _18449 = _18908;
    Interval _18909 = jet_add_derivative(_18446, _18449, intervalFailed);
    Interval _18430 = _18882;
    Interval _18431 = Interval{ as_type<float>(as_type<uint>(_18903.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18903.lo) ^ 2147483648u) };
    Interval _18935 = iadd(_18430, _18431, intervalFailed);
    Interval _18432 = _18883;
    Interval _18433 = Interval{ as_type<float>(as_type<uint>(_18906.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18906.lo) ^ 2147483648u) };
    Interval _18937 = jet_add_derivative(_18432, _18433, intervalFailed);
    Interval _18434 = _18884;
    Interval _18435 = Interval{ as_type<float>(as_type<uint>(_18909.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18909.lo) ^ 2147483648u) };
    Interval _18939 = jet_add_derivative(_18434, _18435, intervalFailed);
    float _18428 = 65.0;
    float _18429 = 100.0;
    Interval _18941 = iratio(_18428, _18429, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18946;
    if (!intervalFailed)
    {
        _18946 = intervalFailed;
    }
    else
    {
        _18946 = false;
    }
    bool _18951;
    if (_18946)
    {
        _18951 = jetFailureSite == 0u;
    }
    else
    {
        _18951 = false;
    }
    if (_18951)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
    }
    Interval _18420 = _18711;
    float _18418 = 3.1415927410125732421875;
    float _18954 = interval_down(_18418, intervalFailed);
    float _18419 = 3.1415927410125732421875;
    float _18955 = interval_up(_18419, intervalFailed);
    Interval _18421 = Interval{ _18954, _18955 };
    Interval _18422 = Interval{ 0.5, 0.5 };
    Interval _18957 = imul(_18421, _18422, intervalFailed, optical_product_upper);
    Interval _18423 = _18957;
    Interval _18958 = iadd(_18420, _18423, intervalFailed);
    float _18416 = _18958.lo;
    float _18417 = _18958.hi;
    float _18961 = sine_bounds(_18416, _18417, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18414 = _18711.lo;
    float _18415 = _18711.hi;
    float _18965 = sine_bounds(_18414, _18415, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18424 = Interval{ _18961, interval_sine_upper };
    Interval _18425 = _18712;
    Interval _18968 = jet_mul_derivative(_18424, _18425, intervalFailed, optical_product_upper);
    Interval _18426 = Interval{ _18961, interval_sine_upper };
    Interval _18427 = _18713;
    Interval _18970 = jet_mul_derivative(_18426, _18427, intervalFailed, optical_product_upper);
    Interval _18400 = _18941;
    Interval _18401 = Interval{ _18965, interval_sine_upper };
    Interval _18972 = imul(_18400, _18401, intervalFailed, optical_product_upper);
    Interval _18402 = Interval{ 0.0, 0.0 };
    Interval _18403 = Interval{ _18965, interval_sine_upper };
    Interval _18974 = jet_mul_derivative(_18402, _18403, intervalFailed, optical_product_upper);
    Interval _18404 = _18974;
    Interval _18405 = _18941;
    Interval _18406 = _18968;
    Interval _18975 = jet_mul_derivative(_18405, _18406, intervalFailed, optical_product_upper);
    Interval _18407 = _18975;
    Interval _18976 = jet_add_derivative(_18404, _18407, intervalFailed);
    Interval _18408 = Interval{ 0.0, 0.0 };
    Interval _18409 = Interval{ _18965, interval_sine_upper };
    Interval _18978 = jet_mul_derivative(_18408, _18409, intervalFailed, optical_product_upper);
    Interval _18410 = _18978;
    Interval _18411 = _18941;
    Interval _18412 = _18970;
    Interval _18979 = jet_mul_derivative(_18411, _18412, intervalFailed, optical_product_upper);
    Interval _18413 = _18979;
    Interval _18980 = jet_add_derivative(_18410, _18413, intervalFailed);
    Interval _18386 = _18972;
    Interval _18387 = _18792;
    Interval _18981 = imul(_18386, _18387, intervalFailed, optical_product_upper);
    Interval _18388 = _18976;
    Interval _18389 = _18792;
    Interval _18982 = jet_mul_derivative(_18388, _18389, intervalFailed, optical_product_upper);
    Interval _18390 = _18982;
    Interval _18391 = _18972;
    Interval _18392 = _18819;
    Interval _18983 = jet_mul_derivative(_18391, _18392, intervalFailed, optical_product_upper);
    Interval _18393 = _18983;
    Interval _18984 = jet_add_derivative(_18390, _18393, intervalFailed);
    Interval _18394 = _18980;
    Interval _18395 = _18792;
    Interval _18985 = jet_mul_derivative(_18394, _18395, intervalFailed, optical_product_upper);
    Interval _18396 = _18985;
    Interval _18397 = _18972;
    Interval _18398 = _18831;
    Interval _18986 = jet_mul_derivative(_18397, _18398, intervalFailed, optical_product_upper);
    Interval _18399 = _18986;
    Interval _18987 = jet_add_derivative(_18396, _18399, intervalFailed);
    Interval _18380 = _18935;
    Interval _18381 = _18981;
    Interval _18988 = iadd(_18380, _18381, intervalFailed);
    Interval _18382 = _18937;
    Interval _18383 = _18984;
    Interval _18989 = jet_add_derivative(_18382, _18383, intervalFailed);
    Interval _18384 = _18939;
    Interval _18385 = _18987;
    Interval _18990 = jet_add_derivative(_18384, _18385, intervalFailed);
    float _18378 = 47.0;
    float _18379 = 1000.0;
    Interval _18996 = iratio(_18378, _18379, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19001;
    if (!intervalFailed)
    {
        _19001 = intervalFailed;
    }
    else
    {
        _19001 = false;
    }
    bool _19006;
    if (_19001)
    {
        _19006 = jetFailureSite == 0u;
    }
    else
    {
        _19006 = false;
    }
    if (_19006)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
    }
    Interval _18364 = x.v;
    Interval _18365 = _18996;
    Interval _19009 = imul(_18364, _18365, intervalFailed, optical_product_upper);
    Interval _18366 = x.dx;
    Interval _18367 = _18996;
    Interval _19010 = jet_mul_derivative(_18366, _18367, intervalFailed, optical_product_upper);
    Interval _18368 = _19010;
    Interval _18369 = x.v;
    Interval _18370 = Interval{ 0.0, 0.0 };
    Interval _19011 = jet_mul_derivative(_18369, _18370, intervalFailed, optical_product_upper);
    Interval _18371 = _19011;
    Interval _19012 = jet_add_derivative(_18368, _18371, intervalFailed);
    Interval _18372 = x.dy;
    Interval _18373 = _18996;
    Interval _19013 = jet_mul_derivative(_18372, _18373, intervalFailed, optical_product_upper);
    Interval _18374 = _19013;
    Interval _18375 = x.v;
    Interval _18376 = Interval{ 0.0, 0.0 };
    Interval _19014 = jet_mul_derivative(_18375, _18376, intervalFailed, optical_product_upper);
    Interval _18377 = _19014;
    Interval _19015 = jet_add_derivative(_18374, _18377, intervalFailed);
    float _18362 = 25.0;
    float _18363 = 1000.0;
    Interval _19021 = iratio(_18362, _18363, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19026;
    if (!intervalFailed)
    {
        _19026 = intervalFailed;
    }
    else
    {
        _19026 = false;
    }
    bool _19031;
    if (_19026)
    {
        _19031 = jetFailureSite == 0u;
    }
    else
    {
        _19031 = false;
    }
    if (_19031)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
    }
    Interval _18348 = z.v;
    Interval _18349 = _19021;
    Interval _19034 = imul(_18348, _18349, intervalFailed, optical_product_upper);
    Interval _18350 = z.dx;
    Interval _18351 = _19021;
    Interval _19035 = jet_mul_derivative(_18350, _18351, intervalFailed, optical_product_upper);
    Interval _18352 = _19035;
    Interval _18353 = z.v;
    Interval _18354 = Interval{ 0.0, 0.0 };
    Interval _19036 = jet_mul_derivative(_18353, _18354, intervalFailed, optical_product_upper);
    Interval _18355 = _19036;
    Interval _19037 = jet_add_derivative(_18352, _18355, intervalFailed);
    Interval _18356 = z.dy;
    Interval _18357 = _19021;
    Interval _19038 = jet_mul_derivative(_18356, _18357, intervalFailed, optical_product_upper);
    Interval _18358 = _19038;
    Interval _18359 = z.v;
    Interval _18360 = Interval{ 0.0, 0.0 };
    Interval _19039 = jet_mul_derivative(_18359, _18360, intervalFailed, optical_product_upper);
    Interval _18361 = _19039;
    Interval _19040 = jet_add_derivative(_18358, _18361, intervalFailed);
    Interval _18342 = _19009;
    Interval _18343 = Interval{ as_type<float>(as_type<uint>(_19034.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19034.lo) ^ 2147483648u) };
    Interval _19066 = iadd(_18342, _18343, intervalFailed);
    Interval _18344 = _19012;
    Interval _18345 = Interval{ as_type<float>(as_type<uint>(_19037.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19037.lo) ^ 2147483648u) };
    Interval _19068 = jet_add_derivative(_18344, _18345, intervalFailed);
    Interval _18346 = _19015;
    Interval _18347 = Interval{ as_type<float>(as_type<uint>(_19040.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19040.lo) ^ 2147483648u) };
    Interval _19070 = jet_add_derivative(_18346, _18347, intervalFailed);
    float _18340 = 60.0;
    float _18341 = 100.0;
    Interval _19076 = iratio(_18340, _18341, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19081;
    if (!intervalFailed)
    {
        _19081 = intervalFailed;
    }
    else
    {
        _19081 = false;
    }
    bool _19086;
    if (_19081)
    {
        _19086 = jetFailureSite == 0u;
    }
    else
    {
        _19086 = false;
    }
    if (_19086)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(60.0, 60.0, 100.0, 100.0);
    }
    Interval _18326 = t.v;
    Interval _18327 = _19076;
    Interval _19089 = imul(_18326, _18327, intervalFailed, optical_product_upper);
    Interval _18328 = t.dx;
    Interval _18329 = _19076;
    Interval _19090 = jet_mul_derivative(_18328, _18329, intervalFailed, optical_product_upper);
    Interval _18330 = _19090;
    Interval _18331 = t.v;
    Interval _18332 = Interval{ 0.0, 0.0 };
    Interval _19091 = jet_mul_derivative(_18331, _18332, intervalFailed, optical_product_upper);
    Interval _18333 = _19091;
    Interval _19092 = jet_add_derivative(_18330, _18333, intervalFailed);
    Interval _18334 = t.dy;
    Interval _18335 = _19076;
    Interval _19093 = jet_mul_derivative(_18334, _18335, intervalFailed, optical_product_upper);
    Interval _18336 = _19093;
    Interval _18337 = t.v;
    Interval _18338 = Interval{ 0.0, 0.0 };
    Interval _19094 = jet_mul_derivative(_18337, _18338, intervalFailed, optical_product_upper);
    Interval _18339 = _19094;
    Interval _19095 = jet_add_derivative(_18336, _18339, intervalFailed);
    Interval _18320 = _19066;
    Interval _18321 = _19089;
    Interval _19096 = iadd(_18320, _18321, intervalFailed);
    Interval _18322 = _19068;
    Interval _18323 = _19092;
    Interval _19097 = jet_add_derivative(_18322, _18323, intervalFailed);
    Interval _18324 = _19070;
    Interval _18325 = _19095;
    Interval _19098 = jet_add_derivative(_18324, _18325, intervalFailed);
    float _18318 = 22.0;
    float _18319 = 1000.0;
    Interval _19104 = iratio(_18318, _18319, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19109;
    if (!intervalFailed)
    {
        _19109 = intervalFailed;
    }
    else
    {
        _19109 = false;
    }
    bool _19114;
    if (_19109)
    {
        _19114 = jetFailureSite == 0u;
    }
    else
    {
        _19114 = false;
    }
    if (_19114)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18304 = z.v;
    Interval _18305 = _19104;
    Interval _19117 = imul(_18304, _18305, intervalFailed, optical_product_upper);
    Interval _18306 = z.dx;
    Interval _18307 = _19104;
    Interval _19118 = jet_mul_derivative(_18306, _18307, intervalFailed, optical_product_upper);
    Interval _18308 = _19118;
    Interval _18309 = z.v;
    Interval _18310 = Interval{ 0.0, 0.0 };
    Interval _19119 = jet_mul_derivative(_18309, _18310, intervalFailed, optical_product_upper);
    Interval _18311 = _19119;
    Interval _19120 = jet_add_derivative(_18308, _18311, intervalFailed);
    Interval _18312 = z.dy;
    Interval _18313 = _19104;
    Interval _19121 = jet_mul_derivative(_18312, _18313, intervalFailed, optical_product_upper);
    Interval _18314 = _19121;
    Interval _18315 = z.v;
    Interval _18316 = Interval{ 0.0, 0.0 };
    Interval _19122 = jet_mul_derivative(_18315, _18316, intervalFailed, optical_product_upper);
    Interval _18317 = _19122;
    Interval _19123 = jet_add_derivative(_18314, _18317, intervalFailed);
    float _18302 = 9.0;
    float _18303 = 1000.0;
    Interval _19129 = iratio(_18302, _18303, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19134;
    if (!intervalFailed)
    {
        _19134 = intervalFailed;
    }
    else
    {
        _19134 = false;
    }
    bool _19139;
    if (_19134)
    {
        _19139 = jetFailureSite == 0u;
    }
    else
    {
        _19139 = false;
    }
    if (_19139)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
    }
    Interval _18288 = x.v;
    Interval _18289 = _19129;
    Interval _19142 = imul(_18288, _18289, intervalFailed, optical_product_upper);
    Interval _18290 = x.dx;
    Interval _18291 = _19129;
    Interval _19143 = jet_mul_derivative(_18290, _18291, intervalFailed, optical_product_upper);
    Interval _18292 = _19143;
    Interval _18293 = x.v;
    Interval _18294 = Interval{ 0.0, 0.0 };
    Interval _19144 = jet_mul_derivative(_18293, _18294, intervalFailed, optical_product_upper);
    Interval _18295 = _19144;
    Interval _19145 = jet_add_derivative(_18292, _18295, intervalFailed);
    Interval _18296 = x.dy;
    Interval _18297 = _19129;
    Interval _19146 = jet_mul_derivative(_18296, _18297, intervalFailed, optical_product_upper);
    Interval _18298 = _19146;
    Interval _18299 = x.v;
    Interval _18300 = Interval{ 0.0, 0.0 };
    Interval _19147 = jet_mul_derivative(_18299, _18300, intervalFailed, optical_product_upper);
    Interval _18301 = _19147;
    Interval _19148 = jet_add_derivative(_18298, _18301, intervalFailed);
    Interval _18282 = _19117;
    Interval _18283 = Interval{ as_type<float>(as_type<uint>(_19142.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19142.lo) ^ 2147483648u) };
    Interval _19174 = iadd(_18282, _18283, intervalFailed);
    Interval _18284 = _19120;
    Interval _18285 = Interval{ as_type<float>(as_type<uint>(_19145.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19145.lo) ^ 2147483648u) };
    Interval _19176 = jet_add_derivative(_18284, _18285, intervalFailed);
    Interval _18286 = _19123;
    Interval _18287 = Interval{ as_type<float>(as_type<uint>(_19148.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19148.lo) ^ 2147483648u) };
    Interval _19178 = jet_add_derivative(_18286, _18287, intervalFailed);
    float _18280 = 32.0;
    float _18281 = 100.0;
    Interval _19184 = iratio(_18280, _18281, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19189;
    if (!intervalFailed)
    {
        _19189 = intervalFailed;
    }
    else
    {
        _19189 = false;
    }
    bool _19194;
    if (_19189)
    {
        _19194 = jetFailureSite == 0u;
    }
    else
    {
        _19194 = false;
    }
    if (_19194)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(32.0, 32.0, 100.0, 100.0);
    }
    Interval _18266 = t.v;
    Interval _18267 = _19184;
    Interval _19197 = imul(_18266, _18267, intervalFailed, optical_product_upper);
    Interval _18268 = t.dx;
    Interval _18269 = _19184;
    Interval _19198 = jet_mul_derivative(_18268, _18269, intervalFailed, optical_product_upper);
    Interval _18270 = _19198;
    Interval _18271 = t.v;
    Interval _18272 = Interval{ 0.0, 0.0 };
    Interval _19199 = jet_mul_derivative(_18271, _18272, intervalFailed, optical_product_upper);
    Interval _18273 = _19199;
    Interval _19200 = jet_add_derivative(_18270, _18273, intervalFailed);
    Interval _18274 = t.dy;
    Interval _18275 = _19184;
    Interval _19201 = jet_mul_derivative(_18274, _18275, intervalFailed, optical_product_upper);
    Interval _18276 = _19201;
    Interval _18277 = t.v;
    Interval _18278 = Interval{ 0.0, 0.0 };
    Interval _19202 = jet_mul_derivative(_18277, _18278, intervalFailed, optical_product_upper);
    Interval _18279 = _19202;
    Interval _19203 = jet_add_derivative(_18276, _18279, intervalFailed);
    Interval _18260 = _19174;
    Interval _18261 = Interval{ as_type<float>(as_type<uint>(_19197.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19197.lo) ^ 2147483648u) };
    Interval _19229 = iadd(_18260, _18261, intervalFailed);
    Interval _18262 = _19176;
    Interval _18263 = Interval{ as_type<float>(as_type<uint>(_19200.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19200.lo) ^ 2147483648u) };
    Interval _19231 = jet_add_derivative(_18262, _18263, intervalFailed);
    Interval _18264 = _19178;
    Interval _18265 = Interval{ as_type<float>(as_type<uint>(_19203.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19203.lo) ^ 2147483648u) };
    Interval _19233 = jet_add_derivative(_18264, _18265, intervalFailed);
    float _18258 = 22.0;
    float _18259 = 1000.0;
    Interval _19236 = iratio(_18258, _18259, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19241;
    if (!intervalFailed)
    {
        _19241 = intervalFailed;
    }
    else
    {
        _19241 = false;
    }
    bool _19246;
    if (_19241)
    {
        _19246 = jetFailureSite == 0u;
    }
    else
    {
        _19246 = false;
    }
    if (_19246)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18244 = footprint.v;
    Interval _18245 = _19236;
    Interval _19252 = imul(_18244, _18245, intervalFailed, optical_product_upper);
    Interval _18246 = footprint.dx;
    Interval _18247 = _19236;
    Interval _19253 = jet_mul_derivative(_18246, _18247, intervalFailed, optical_product_upper);
    Interval _18248 = _19253;
    Interval _18249 = footprint.v;
    Interval _18250 = Interval{ 0.0, 0.0 };
    Interval _19254 = jet_mul_derivative(_18249, _18250, intervalFailed, optical_product_upper);
    Interval _18251 = _19254;
    Interval _19255 = jet_add_derivative(_18248, _18251, intervalFailed);
    Interval _18252 = footprint.dy;
    Interval _18253 = _19236;
    Interval _19256 = jet_mul_derivative(_18252, _18253, intervalFailed, optical_product_upper);
    Interval _18254 = _19256;
    Interval _18255 = footprint.v;
    Interval _18256 = Interval{ 0.0, 0.0 };
    Interval _19257 = jet_mul_derivative(_18255, _18256, intervalFailed, optical_product_upper);
    Interval _18257 = _19257;
    Interval _19258 = jet_add_derivative(_18254, _18257, intervalFailed);
    bool _19265;
    if (_19252.lo <= 0.0)
    {
        _19265 = _19252.hi >= 0.0;
    }
    else
    {
        _19265 = false;
    }
    float _19272;
    if (_19265)
    {
        _19272 = 0.0;
    }
    else
    {
        _19272 = precise::min(abs(_19252.lo), abs(_19252.hi));
    }
    float _19275 = precise::max(abs(_19252.lo), abs(_19252.hi));
    float _18234 = spvFMul(_19272, _19272);
    float _19276 = interval_down(_18234, intervalFailed);
    float _19277 = precise::max(0.0, _19276);
    float _18235 = spvFMul(_19275, _19275);
    float _19278 = interval_up(_18235, intervalFailed);
    Interval _18236 = Interval{ 2.0, 2.0 };
    Interval _18237 = _19252;
    Interval _19279 = imul(_18236, _18237, intervalFailed, optical_product_upper);
    Interval _18238 = _19279;
    Interval _18239 = _19255;
    Interval _19280 = jet_mul_derivative(_18238, _18239, intervalFailed, optical_product_upper);
    Interval _18240 = Interval{ 2.0, 2.0 };
    Interval _18241 = _19252;
    Interval _19281 = imul(_18240, _18241, intervalFailed, optical_product_upper);
    Interval _18242 = _19281;
    Interval _18243 = _19258;
    Interval _19282 = jet_mul_derivative(_18242, _18243, intervalFailed, optical_product_upper);
    bool _19287;
    if (_19277 <= 0.0)
    {
        _19287 = _19278 >= 0.0;
    }
    else
    {
        _19287 = false;
    }
    float _19294;
    if (_19287)
    {
        _19294 = 0.0;
    }
    else
    {
        _19294 = precise::min(abs(_19277), abs(_19278));
    }
    float _19297 = precise::max(abs(_19277), abs(_19278));
    float _18224 = spvFMul(_19294, _19294);
    float _19298 = interval_down(_18224, intervalFailed);
    float _18225 = spvFMul(_19297, _19297);
    float _19300 = interval_up(_18225, intervalFailed);
    Interval _18226 = Interval{ 2.0, 2.0 };
    Interval _18227 = Interval{ _19277, _19278 };
    Interval _19302 = imul(_18226, _18227, intervalFailed, optical_product_upper);
    Interval _18228 = _19302;
    Interval _18229 = _19280;
    Interval _19303 = jet_mul_derivative(_18228, _18229, intervalFailed, optical_product_upper);
    Interval _18230 = Interval{ 2.0, 2.0 };
    Interval _18231 = Interval{ _19277, _19278 };
    Interval _19305 = imul(_18230, _18231, intervalFailed, optical_product_upper);
    Interval _18232 = _19305;
    Interval _18233 = _19282;
    Interval _19306 = jet_mul_derivative(_18232, _18233, intervalFailed, optical_product_upper);
    Interval _18218 = Interval{ 1.0, 1.0 };
    Interval _18219 = Interval{ precise::max(0.0, _19298), _19300 };
    Interval _19308 = iadd(_18218, _18219, intervalFailed);
    Interval _18220 = Interval{ 0.0, 0.0 };
    Interval _18221 = _19303;
    Interval _19309 = jet_add_derivative(_18220, _18221, intervalFailed);
    Interval _18222 = Interval{ 0.0, 0.0 };
    Interval _18223 = _19306;
    Interval _19310 = jet_add_derivative(_18222, _18223, intervalFailed);
    Interval _18204 = Interval{ 1.0, 1.0 };
    Interval _18205 = _19308;
    Interval _19312 = idiv(_18204, _18205, intervalFailed, interval_divide_upper);
    bool _19319;
    if (!intervalFailed)
    {
        _19319 = intervalFailed;
    }
    else
    {
        _19319 = false;
    }
    bool _19324;
    if (_19319)
    {
        _19324 = jetFailureSite == 0u;
    }
    else
    {
        _19324 = false;
    }
    if (_19324)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19308.lo, _19308.hi);
    }
    Interval _18206 = Interval{ 0.0, 0.0 };
    Interval _18207 = _19312;
    Interval _18208 = _19309;
    Interval _19328 = jet_mul_derivative(_18207, _18208, intervalFailed, optical_product_upper);
    Interval _18209 = Interval{ as_type<float>(as_type<uint>(_19328.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19328.lo) ^ 2147483648u) };
    Interval _19338 = jet_add_derivative(_18206, _18209, intervalFailed);
    Interval _18210 = _19338;
    Interval _18211 = _19308;
    Interval _19339 = jet_div_derivative(_18210, _18211, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18212 = Interval{ 0.0, 0.0 };
    Interval _18213 = _19312;
    Interval _18214 = _19310;
    Interval _19340 = jet_mul_derivative(_18213, _18214, intervalFailed, optical_product_upper);
    Interval _18215 = Interval{ as_type<float>(as_type<uint>(_19340.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19340.lo) ^ 2147483648u) };
    Interval _19350 = jet_add_derivative(_18212, _18215, intervalFailed);
    Interval _18216 = _19350;
    Interval _18217 = _19308;
    Interval _19351 = jet_div_derivative(_18216, _18217, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18202 = 54.0;
    float _18203 = 1000.0;
    Interval _19354 = iratio(_18202, _18203, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19359;
    if (!intervalFailed)
    {
        _19359 = intervalFailed;
    }
    else
    {
        _19359 = false;
    }
    bool _19364;
    if (_19359)
    {
        _19364 = jetFailureSite == 0u;
    }
    else
    {
        _19364 = false;
    }
    if (_19364)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
    }
    Interval _18188 = footprint.v;
    Interval _18189 = _19354;
    Interval _19370 = imul(_18188, _18189, intervalFailed, optical_product_upper);
    Interval _18190 = footprint.dx;
    Interval _18191 = _19354;
    Interval _19371 = jet_mul_derivative(_18190, _18191, intervalFailed, optical_product_upper);
    Interval _18192 = _19371;
    Interval _18193 = footprint.v;
    Interval _18194 = Interval{ 0.0, 0.0 };
    Interval _19372 = jet_mul_derivative(_18193, _18194, intervalFailed, optical_product_upper);
    Interval _18195 = _19372;
    Interval _19373 = jet_add_derivative(_18192, _18195, intervalFailed);
    Interval _18196 = footprint.dy;
    Interval _18197 = _19354;
    Interval _19374 = jet_mul_derivative(_18196, _18197, intervalFailed, optical_product_upper);
    Interval _18198 = _19374;
    Interval _18199 = footprint.v;
    Interval _18200 = Interval{ 0.0, 0.0 };
    Interval _19375 = jet_mul_derivative(_18199, _18200, intervalFailed, optical_product_upper);
    Interval _18201 = _19375;
    Interval _19376 = jet_add_derivative(_18198, _18201, intervalFailed);
    bool _19383;
    if (_19370.lo <= 0.0)
    {
        _19383 = _19370.hi >= 0.0;
    }
    else
    {
        _19383 = false;
    }
    float _19390;
    if (_19383)
    {
        _19390 = 0.0;
    }
    else
    {
        _19390 = precise::min(abs(_19370.lo), abs(_19370.hi));
    }
    float _19393 = precise::max(abs(_19370.lo), abs(_19370.hi));
    float _18178 = spvFMul(_19390, _19390);
    float _19394 = interval_down(_18178, intervalFailed);
    float _19395 = precise::max(0.0, _19394);
    float _18179 = spvFMul(_19393, _19393);
    float _19396 = interval_up(_18179, intervalFailed);
    Interval _18180 = Interval{ 2.0, 2.0 };
    Interval _18181 = _19370;
    Interval _19397 = imul(_18180, _18181, intervalFailed, optical_product_upper);
    Interval _18182 = _19397;
    Interval _18183 = _19373;
    Interval _19398 = jet_mul_derivative(_18182, _18183, intervalFailed, optical_product_upper);
    Interval _18184 = Interval{ 2.0, 2.0 };
    Interval _18185 = _19370;
    Interval _19399 = imul(_18184, _18185, intervalFailed, optical_product_upper);
    Interval _18186 = _19399;
    Interval _18187 = _19376;
    Interval _19400 = jet_mul_derivative(_18186, _18187, intervalFailed, optical_product_upper);
    bool _19405;
    if (_19395 <= 0.0)
    {
        _19405 = _19396 >= 0.0;
    }
    else
    {
        _19405 = false;
    }
    float _19412;
    if (_19405)
    {
        _19412 = 0.0;
    }
    else
    {
        _19412 = precise::min(abs(_19395), abs(_19396));
    }
    float _19415 = precise::max(abs(_19395), abs(_19396));
    float _18168 = spvFMul(_19412, _19412);
    float _19416 = interval_down(_18168, intervalFailed);
    float _18169 = spvFMul(_19415, _19415);
    float _19418 = interval_up(_18169, intervalFailed);
    Interval _18170 = Interval{ 2.0, 2.0 };
    Interval _18171 = Interval{ _19395, _19396 };
    Interval _19420 = imul(_18170, _18171, intervalFailed, optical_product_upper);
    Interval _18172 = _19420;
    Interval _18173 = _19398;
    Interval _19421 = jet_mul_derivative(_18172, _18173, intervalFailed, optical_product_upper);
    Interval _18174 = Interval{ 2.0, 2.0 };
    Interval _18175 = Interval{ _19395, _19396 };
    Interval _19423 = imul(_18174, _18175, intervalFailed, optical_product_upper);
    Interval _18176 = _19423;
    Interval _18177 = _19400;
    Interval _19424 = jet_mul_derivative(_18176, _18177, intervalFailed, optical_product_upper);
    Interval _18162 = Interval{ 1.0, 1.0 };
    Interval _18163 = Interval{ precise::max(0.0, _19416), _19418 };
    Interval _19426 = iadd(_18162, _18163, intervalFailed);
    Interval _18164 = Interval{ 0.0, 0.0 };
    Interval _18165 = _19421;
    Interval _19427 = jet_add_derivative(_18164, _18165, intervalFailed);
    Interval _18166 = Interval{ 0.0, 0.0 };
    Interval _18167 = _19424;
    Interval _19428 = jet_add_derivative(_18166, _18167, intervalFailed);
    Interval _18148 = Interval{ 1.0, 1.0 };
    Interval _18149 = _19426;
    Interval _19430 = idiv(_18148, _18149, intervalFailed, interval_divide_upper);
    bool _19437;
    if (!intervalFailed)
    {
        _19437 = intervalFailed;
    }
    else
    {
        _19437 = false;
    }
    bool _19442;
    if (_19437)
    {
        _19442 = jetFailureSite == 0u;
    }
    else
    {
        _19442 = false;
    }
    if (_19442)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19426.lo, _19426.hi);
    }
    Interval _18150 = Interval{ 0.0, 0.0 };
    Interval _18151 = _19430;
    Interval _18152 = _19427;
    Interval _19446 = jet_mul_derivative(_18151, _18152, intervalFailed, optical_product_upper);
    Interval _18153 = Interval{ as_type<float>(as_type<uint>(_19446.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19446.lo) ^ 2147483648u) };
    Interval _19456 = jet_add_derivative(_18150, _18153, intervalFailed);
    Interval _18154 = _19456;
    Interval _18155 = _19426;
    Interval _19457 = jet_div_derivative(_18154, _18155, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18156 = Interval{ 0.0, 0.0 };
    Interval _18157 = _19430;
    Interval _18158 = _19428;
    Interval _19458 = jet_mul_derivative(_18157, _18158, intervalFailed, optical_product_upper);
    Interval _18159 = Interval{ as_type<float>(as_type<uint>(_19458.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19458.lo) ^ 2147483648u) };
    Interval _19468 = jet_add_derivative(_18156, _18159, intervalFailed);
    Interval _18160 = _19468;
    Interval _18161 = _19426;
    Interval _19469 = jet_div_derivative(_18160, _18161, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18146 = 24.0;
    float _18147 = 1000.0;
    Interval _19472 = iratio(_18146, _18147, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19477;
    if (!intervalFailed)
    {
        _19477 = intervalFailed;
    }
    else
    {
        _19477 = false;
    }
    bool _19482;
    if (_19477)
    {
        _19482 = jetFailureSite == 0u;
    }
    else
    {
        _19482 = false;
    }
    if (_19482)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
    }
    Interval _18132 = footprint.v;
    Interval _18133 = _19472;
    Interval _19488 = imul(_18132, _18133, intervalFailed, optical_product_upper);
    Interval _18134 = footprint.dx;
    Interval _18135 = _19472;
    Interval _19489 = jet_mul_derivative(_18134, _18135, intervalFailed, optical_product_upper);
    Interval _18136 = _19489;
    Interval _18137 = footprint.v;
    Interval _18138 = Interval{ 0.0, 0.0 };
    Interval _19490 = jet_mul_derivative(_18137, _18138, intervalFailed, optical_product_upper);
    Interval _18139 = _19490;
    Interval _19491 = jet_add_derivative(_18136, _18139, intervalFailed);
    Interval _18140 = footprint.dy;
    Interval _18141 = _19472;
    Interval _19492 = jet_mul_derivative(_18140, _18141, intervalFailed, optical_product_upper);
    Interval _18142 = _19492;
    Interval _18143 = footprint.v;
    Interval _18144 = Interval{ 0.0, 0.0 };
    Interval _19493 = jet_mul_derivative(_18143, _18144, intervalFailed, optical_product_upper);
    Interval _18145 = _19493;
    Interval _19494 = jet_add_derivative(_18142, _18145, intervalFailed);
    bool _19501;
    if (_19488.lo <= 0.0)
    {
        _19501 = _19488.hi >= 0.0;
    }
    else
    {
        _19501 = false;
    }
    float _19508;
    if (_19501)
    {
        _19508 = 0.0;
    }
    else
    {
        _19508 = precise::min(abs(_19488.lo), abs(_19488.hi));
    }
    float _19511 = precise::max(abs(_19488.lo), abs(_19488.hi));
    float _18122 = spvFMul(_19508, _19508);
    float _19512 = interval_down(_18122, intervalFailed);
    float _19513 = precise::max(0.0, _19512);
    float _18123 = spvFMul(_19511, _19511);
    float _19514 = interval_up(_18123, intervalFailed);
    Interval _18124 = Interval{ 2.0, 2.0 };
    Interval _18125 = _19488;
    Interval _19515 = imul(_18124, _18125, intervalFailed, optical_product_upper);
    Interval _18126 = _19515;
    Interval _18127 = _19491;
    Interval _19516 = jet_mul_derivative(_18126, _18127, intervalFailed, optical_product_upper);
    Interval _18128 = Interval{ 2.0, 2.0 };
    Interval _18129 = _19488;
    Interval _19517 = imul(_18128, _18129, intervalFailed, optical_product_upper);
    Interval _18130 = _19517;
    Interval _18131 = _19494;
    Interval _19518 = jet_mul_derivative(_18130, _18131, intervalFailed, optical_product_upper);
    bool _19523;
    if (_19513 <= 0.0)
    {
        _19523 = _19514 >= 0.0;
    }
    else
    {
        _19523 = false;
    }
    float _19530;
    if (_19523)
    {
        _19530 = 0.0;
    }
    else
    {
        _19530 = precise::min(abs(_19513), abs(_19514));
    }
    float _19533 = precise::max(abs(_19513), abs(_19514));
    float _18112 = spvFMul(_19530, _19530);
    float _19534 = interval_down(_18112, intervalFailed);
    float _18113 = spvFMul(_19533, _19533);
    float _19536 = interval_up(_18113, intervalFailed);
    Interval _18114 = Interval{ 2.0, 2.0 };
    Interval _18115 = Interval{ _19513, _19514 };
    Interval _19538 = imul(_18114, _18115, intervalFailed, optical_product_upper);
    Interval _18116 = _19538;
    Interval _18117 = _19516;
    Interval _19539 = jet_mul_derivative(_18116, _18117, intervalFailed, optical_product_upper);
    Interval _18118 = Interval{ 2.0, 2.0 };
    Interval _18119 = Interval{ _19513, _19514 };
    Interval _19541 = imul(_18118, _18119, intervalFailed, optical_product_upper);
    Interval _18120 = _19541;
    Interval _18121 = _19518;
    Interval _19542 = jet_mul_derivative(_18120, _18121, intervalFailed, optical_product_upper);
    Interval _18106 = Interval{ 1.0, 1.0 };
    Interval _18107 = Interval{ precise::max(0.0, _19534), _19536 };
    Interval _19544 = iadd(_18106, _18107, intervalFailed);
    Interval _18108 = Interval{ 0.0, 0.0 };
    Interval _18109 = _19539;
    Interval _19545 = jet_add_derivative(_18108, _18109, intervalFailed);
    Interval _18110 = Interval{ 0.0, 0.0 };
    Interval _18111 = _19542;
    Interval _19546 = jet_add_derivative(_18110, _18111, intervalFailed);
    Interval _18092 = Interval{ 1.0, 1.0 };
    Interval _18093 = _19544;
    Interval _19548 = idiv(_18092, _18093, intervalFailed, interval_divide_upper);
    bool _19555;
    if (!intervalFailed)
    {
        _19555 = intervalFailed;
    }
    else
    {
        _19555 = false;
    }
    bool _19560;
    if (_19555)
    {
        _19560 = jetFailureSite == 0u;
    }
    else
    {
        _19560 = false;
    }
    if (_19560)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19544.lo, _19544.hi);
    }
    Interval _18094 = Interval{ 0.0, 0.0 };
    Interval _18095 = _19548;
    Interval _18096 = _19545;
    Interval _19564 = jet_mul_derivative(_18095, _18096, intervalFailed, optical_product_upper);
    Interval _18097 = Interval{ as_type<float>(as_type<uint>(_19564.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19564.lo) ^ 2147483648u) };
    Interval _19574 = jet_add_derivative(_18094, _18097, intervalFailed);
    Interval _18098 = _19574;
    Interval _18099 = _19544;
    Interval _19575 = jet_div_derivative(_18098, _18099, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18100 = Interval{ 0.0, 0.0 };
    Interval _18101 = _19548;
    Interval _18102 = _19546;
    Interval _19576 = jet_mul_derivative(_18101, _18102, intervalFailed, optical_product_upper);
    Interval _18103 = Interval{ as_type<float>(as_type<uint>(_19576.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19576.lo) ^ 2147483648u) };
    Interval _19586 = jet_add_derivative(_18100, _18103, intervalFailed);
    Interval _18104 = _19586;
    Interval _18105 = _19544;
    Interval _19587 = jet_div_derivative(_18104, _18105, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18084 = _18988;
    float _18082 = 3.1415927410125732421875;
    float _19588 = interval_down(_18082, intervalFailed);
    float _18083 = 3.1415927410125732421875;
    float _19589 = interval_up(_18083, intervalFailed);
    Interval _18085 = Interval{ _19588, _19589 };
    Interval _18086 = Interval{ 0.5, 0.5 };
    Interval _19591 = imul(_18085, _18086, intervalFailed, optical_product_upper);
    Interval _18087 = _19591;
    Interval _19592 = iadd(_18084, _18087, intervalFailed);
    float _18080 = _19592.lo;
    float _18081 = _19592.hi;
    float _19595 = sine_bounds(_18080, _18081, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18078 = _18988.lo;
    float _18079 = _18988.hi;
    float _19599 = sine_bounds(_18078, _18079, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18088 = Interval{ _19595, interval_sine_upper };
    Interval _18089 = _18989;
    Interval _19602 = jet_mul_derivative(_18088, _18089, intervalFailed, optical_product_upper);
    Interval _18090 = Interval{ _19595, interval_sine_upper };
    Interval _18091 = _18990;
    Interval _19604 = jet_mul_derivative(_18090, _18091, intervalFailed, optical_product_upper);
    Interval _18064 = Interval{ 9.0, 9.0 };
    Interval _18065 = Interval{ _19599, interval_sine_upper };
    Interval _19606 = imul(_18064, _18065, intervalFailed, optical_product_upper);
    Interval _18066 = Interval{ 0.0, 0.0 };
    Interval _18067 = Interval{ _19599, interval_sine_upper };
    Interval _19608 = jet_mul_derivative(_18066, _18067, intervalFailed, optical_product_upper);
    Interval _18068 = _19608;
    Interval _18069 = Interval{ 9.0, 9.0 };
    Interval _18070 = _19602;
    Interval _19609 = jet_mul_derivative(_18069, _18070, intervalFailed, optical_product_upper);
    Interval _18071 = _19609;
    Interval _19610 = jet_add_derivative(_18068, _18071, intervalFailed);
    Interval _18072 = Interval{ 0.0, 0.0 };
    Interval _18073 = Interval{ _19599, interval_sine_upper };
    Interval _19612 = jet_mul_derivative(_18072, _18073, intervalFailed, optical_product_upper);
    Interval _18074 = _19612;
    Interval _18075 = Interval{ 9.0, 9.0 };
    Interval _18076 = _19604;
    Interval _19613 = jet_mul_derivative(_18075, _18076, intervalFailed, optical_product_upper);
    Interval _18077 = _19613;
    Interval _19614 = jet_add_derivative(_18074, _18077, intervalFailed);
    Interval _18050 = _19606;
    Interval _18051 = _19312;
    Interval _19615 = imul(_18050, _18051, intervalFailed, optical_product_upper);
    Interval _18052 = _19610;
    Interval _18053 = _19312;
    Interval _19616 = jet_mul_derivative(_18052, _18053, intervalFailed, optical_product_upper);
    Interval _18054 = _19616;
    Interval _18055 = _19606;
    Interval _18056 = _19339;
    Interval _19617 = jet_mul_derivative(_18055, _18056, intervalFailed, optical_product_upper);
    Interval _18057 = _19617;
    Interval _19618 = jet_add_derivative(_18054, _18057, intervalFailed);
    Interval _18058 = _19614;
    Interval _18059 = _19312;
    Interval _19619 = jet_mul_derivative(_18058, _18059, intervalFailed, optical_product_upper);
    Interval _18060 = _19619;
    Interval _18061 = _19606;
    Interval _18062 = _19351;
    Interval _19620 = jet_mul_derivative(_18061, _18062, intervalFailed, optical_product_upper);
    Interval _18063 = _19620;
    Interval _19621 = jet_add_derivative(_18060, _18063, intervalFailed);
    Interval _18042 = _19096;
    float _18040 = 3.1415927410125732421875;
    float _19622 = interval_down(_18040, intervalFailed);
    float _18041 = 3.1415927410125732421875;
    float _19623 = interval_up(_18041, intervalFailed);
    Interval _18043 = Interval{ _19622, _19623 };
    Interval _18044 = Interval{ 0.5, 0.5 };
    Interval _19625 = imul(_18043, _18044, intervalFailed, optical_product_upper);
    Interval _18045 = _19625;
    Interval _19626 = iadd(_18042, _18045, intervalFailed);
    float _18038 = _19626.lo;
    float _18039 = _19626.hi;
    float _19629 = sine_bounds(_18038, _18039, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18036 = _19096.lo;
    float _18037 = _19096.hi;
    float _19633 = sine_bounds(_18036, _18037, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18046 = Interval{ _19629, interval_sine_upper };
    Interval _18047 = _19097;
    Interval _19636 = jet_mul_derivative(_18046, _18047, intervalFailed, optical_product_upper);
    Interval _18048 = Interval{ _19629, interval_sine_upper };
    Interval _18049 = _19098;
    Interval _19638 = jet_mul_derivative(_18048, _18049, intervalFailed, optical_product_upper);
    Interval _18022 = Interval{ 2.5, 2.5 };
    Interval _18023 = Interval{ _19633, interval_sine_upper };
    Interval _19640 = imul(_18022, _18023, intervalFailed, optical_product_upper);
    Interval _18024 = Interval{ 0.0, 0.0 };
    Interval _18025 = Interval{ _19633, interval_sine_upper };
    Interval _19642 = jet_mul_derivative(_18024, _18025, intervalFailed, optical_product_upper);
    Interval _18026 = _19642;
    Interval _18027 = Interval{ 2.5, 2.5 };
    Interval _18028 = _19636;
    Interval _19643 = jet_mul_derivative(_18027, _18028, intervalFailed, optical_product_upper);
    Interval _18029 = _19643;
    Interval _19644 = jet_add_derivative(_18026, _18029, intervalFailed);
    Interval _18030 = Interval{ 0.0, 0.0 };
    Interval _18031 = Interval{ _19633, interval_sine_upper };
    Interval _19646 = jet_mul_derivative(_18030, _18031, intervalFailed, optical_product_upper);
    Interval _18032 = _19646;
    Interval _18033 = Interval{ 2.5, 2.5 };
    Interval _18034 = _19638;
    Interval _19647 = jet_mul_derivative(_18033, _18034, intervalFailed, optical_product_upper);
    Interval _18035 = _19647;
    Interval _19648 = jet_add_derivative(_18032, _18035, intervalFailed);
    Interval _18008 = _19640;
    Interval _18009 = _19430;
    Interval _19649 = imul(_18008, _18009, intervalFailed, optical_product_upper);
    Interval _18010 = _19644;
    Interval _18011 = _19430;
    Interval _19650 = jet_mul_derivative(_18010, _18011, intervalFailed, optical_product_upper);
    Interval _18012 = _19650;
    Interval _18013 = _19640;
    Interval _18014 = _19457;
    Interval _19651 = jet_mul_derivative(_18013, _18014, intervalFailed, optical_product_upper);
    Interval _18015 = _19651;
    Interval _19652 = jet_add_derivative(_18012, _18015, intervalFailed);
    Interval _18016 = _19648;
    Interval _18017 = _19430;
    Interval _19653 = jet_mul_derivative(_18016, _18017, intervalFailed, optical_product_upper);
    Interval _18018 = _19653;
    Interval _18019 = _19640;
    Interval _18020 = _19469;
    Interval _19654 = jet_mul_derivative(_18019, _18020, intervalFailed, optical_product_upper);
    Interval _18021 = _19654;
    Interval _19655 = jet_add_derivative(_18018, _18021, intervalFailed);
    Interval _18002 = _19615;
    Interval _18003 = _19649;
    Interval _19656 = iadd(_18002, _18003, intervalFailed);
    Interval _18004 = _19618;
    Interval _18005 = _19652;
    Interval _19657 = jet_add_derivative(_18004, _18005, intervalFailed);
    Interval _18006 = _19621;
    Interval _18007 = _19655;
    Interval _19658 = jet_add_derivative(_18006, _18007, intervalFailed);
    Interval _17994 = _19229;
    float _17992 = 3.1415927410125732421875;
    float _19659 = interval_down(_17992, intervalFailed);
    float _17993 = 3.1415927410125732421875;
    float _19660 = interval_up(_17993, intervalFailed);
    Interval _17995 = Interval{ _19659, _19660 };
    Interval _17996 = Interval{ 0.5, 0.5 };
    Interval _19662 = imul(_17995, _17996, intervalFailed, optical_product_upper);
    Interval _17997 = _19662;
    Interval _19663 = iadd(_17994, _17997, intervalFailed);
    float _17990 = _19663.lo;
    float _17991 = _19663.hi;
    float _19666 = sine_bounds(_17990, _17991, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17988 = _19229.lo;
    float _17989 = _19229.hi;
    float _19670 = sine_bounds(_17988, _17989, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17998 = Interval{ _19666, interval_sine_upper };
    Interval _17999 = _19231;
    Interval _19673 = jet_mul_derivative(_17998, _17999, intervalFailed, optical_product_upper);
    Interval _18000 = Interval{ _19666, interval_sine_upper };
    Interval _18001 = _19233;
    Interval _19675 = jet_mul_derivative(_18000, _18001, intervalFailed, optical_product_upper);
    Interval _17974 = Interval{ 5.0, 5.0 };
    Interval _17975 = Interval{ _19670, interval_sine_upper };
    Interval _19677 = imul(_17974, _17975, intervalFailed, optical_product_upper);
    Interval _17976 = Interval{ 0.0, 0.0 };
    Interval _17977 = Interval{ _19670, interval_sine_upper };
    Interval _19679 = jet_mul_derivative(_17976, _17977, intervalFailed, optical_product_upper);
    Interval _17978 = _19679;
    Interval _17979 = Interval{ 5.0, 5.0 };
    Interval _17980 = _19673;
    Interval _19680 = jet_mul_derivative(_17979, _17980, intervalFailed, optical_product_upper);
    Interval _17981 = _19680;
    Interval _19681 = jet_add_derivative(_17978, _17981, intervalFailed);
    Interval _17982 = Interval{ 0.0, 0.0 };
    Interval _17983 = Interval{ _19670, interval_sine_upper };
    Interval _19683 = jet_mul_derivative(_17982, _17983, intervalFailed, optical_product_upper);
    Interval _17984 = _19683;
    Interval _17985 = Interval{ 5.0, 5.0 };
    Interval _17986 = _19675;
    Interval _19684 = jet_mul_derivative(_17985, _17986, intervalFailed, optical_product_upper);
    Interval _17987 = _19684;
    Interval _19685 = jet_add_derivative(_17984, _17987, intervalFailed);
    Interval _17960 = _19677;
    Interval _17961 = _19548;
    Interval _19686 = imul(_17960, _17961, intervalFailed, optical_product_upper);
    Interval _17962 = _19681;
    Interval _17963 = _19548;
    Interval _19687 = jet_mul_derivative(_17962, _17963, intervalFailed, optical_product_upper);
    Interval _17964 = _19687;
    Interval _17965 = _19677;
    Interval _17966 = _19575;
    Interval _19688 = jet_mul_derivative(_17965, _17966, intervalFailed, optical_product_upper);
    Interval _17967 = _19688;
    Interval _19689 = jet_add_derivative(_17964, _17967, intervalFailed);
    Interval _17968 = _19685;
    Interval _17969 = _19548;
    Interval _19690 = jet_mul_derivative(_17968, _17969, intervalFailed, optical_product_upper);
    Interval _17970 = _19690;
    Interval _17971 = _19677;
    Interval _17972 = _19587;
    Interval _19691 = jet_mul_derivative(_17971, _17972, intervalFailed, optical_product_upper);
    Interval _17973 = _19691;
    Interval _19692 = jet_add_derivative(_17970, _17973, intervalFailed);
    Interval _17954 = _19656;
    Interval _17955 = _19686;
    Interval _19693 = iadd(_17954, _17955, intervalFailed);
    Interval _17956 = _19657;
    Interval _17957 = _19689;
    Interval _19694 = jet_add_derivative(_17956, _17957, intervalFailed);
    Interval _17958 = _19658;
    Interval _17959 = _19692;
    Interval _19695 = jet_add_derivative(_17958, _17959, intervalFailed);
    float _17948 = _18711.lo;
    float _17949 = _18711.hi;
    float _19698 = sine_bounds(_17948, _17949, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19702 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19705 = as_type<float>(as_type<uint>(_19698) ^ 2147483648u);
    Interval _17944 = _18711;
    float _17942 = 3.1415927410125732421875;
    float _19706 = interval_down(_17942, intervalFailed);
    float _17943 = 3.1415927410125732421875;
    float _19707 = interval_up(_17943, intervalFailed);
    Interval _17945 = Interval{ _19706, _19707 };
    Interval _17946 = Interval{ 0.5, 0.5 };
    Interval _19709 = imul(_17945, _17946, intervalFailed, optical_product_upper);
    Interval _17947 = _19709;
    Interval _19710 = iadd(_17944, _17947, intervalFailed);
    float _17940 = _19710.lo;
    float _17941 = _19710.hi;
    float _19713 = sine_bounds(_17940, _17941, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17950 = Interval{ _19702, _19705 };
    Interval _17951 = _18712;
    Interval _19716 = jet_mul_derivative(_17950, _17951, intervalFailed, optical_product_upper);
    Interval _17952 = Interval{ _19702, _19705 };
    Interval _17953 = _18713;
    Interval _19718 = jet_mul_derivative(_17952, _17953, intervalFailed, optical_product_upper);
    Interval _17926 = Interval{ _19713, interval_sine_upper };
    Interval _17927 = _18792;
    Interval _19720 = imul(_17926, _17927, intervalFailed, optical_product_upper);
    Interval _17928 = _19716;
    Interval _17929 = _18792;
    Interval _19721 = jet_mul_derivative(_17928, _17929, intervalFailed, optical_product_upper);
    Interval _17930 = _19721;
    Interval _17931 = Interval{ _19713, interval_sine_upper };
    Interval _17932 = _18819;
    Interval _19723 = jet_mul_derivative(_17931, _17932, intervalFailed, optical_product_upper);
    Interval _17933 = _19723;
    Interval _19724 = jet_add_derivative(_17930, _17933, intervalFailed);
    Interval _17934 = _19718;
    Interval _17935 = _18792;
    Interval _19725 = jet_mul_derivative(_17934, _17935, intervalFailed, optical_product_upper);
    Interval _17936 = _19725;
    Interval _17937 = Interval{ _19713, interval_sine_upper };
    Interval _17938 = _18831;
    Interval _19727 = jet_mul_derivative(_17937, _17938, intervalFailed, optical_product_upper);
    Interval _17939 = _19727;
    Interval _19728 = jet_add_derivative(_17936, _17939, intervalFailed);
    float _17924 = 18.0;
    float _17925 = 1000.0;
    Interval _19730 = iratio(_17924, _17925, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19735;
    if (!intervalFailed)
    {
        _19735 = intervalFailed;
    }
    else
    {
        _19735 = false;
    }
    bool _19740;
    if (_19735)
    {
        _19740 = jetFailureSite == 0u;
    }
    else
    {
        _19740 = false;
    }
    if (_19740)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    float _17922 = 39.0;
    float _17923 = 10000.0;
    Interval _19744 = iratio(_17922, _17923, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19749;
    if (!intervalFailed)
    {
        _19749 = intervalFailed;
    }
    else
    {
        _19749 = false;
    }
    bool _19754;
    if (_19749)
    {
        _19754 = jetFailureSite == 0u;
    }
    else
    {
        _19754 = false;
    }
    if (_19754)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(39.0, 39.0, 10000.0, 10000.0);
    }
    Interval _17908 = _19744;
    Interval _17909 = _19720;
    Interval _19757 = imul(_17908, _17909, intervalFailed, optical_product_upper);
    Interval _17910 = Interval{ 0.0, 0.0 };
    Interval _17911 = _19720;
    Interval _19758 = jet_mul_derivative(_17910, _17911, intervalFailed, optical_product_upper);
    Interval _17912 = _19758;
    Interval _17913 = _19744;
    Interval _17914 = _19724;
    Interval _19759 = jet_mul_derivative(_17913, _17914, intervalFailed, optical_product_upper);
    Interval _17915 = _19759;
    Interval _19760 = jet_add_derivative(_17912, _17915, intervalFailed);
    Interval _17916 = Interval{ 0.0, 0.0 };
    Interval _17917 = _19720;
    Interval _19761 = jet_mul_derivative(_17916, _17917, intervalFailed, optical_product_upper);
    Interval _17918 = _19761;
    Interval _17919 = _19744;
    Interval _17920 = _19728;
    Interval _19762 = jet_mul_derivative(_17919, _17920, intervalFailed, optical_product_upper);
    Interval _17921 = _19762;
    Interval _19763 = jet_add_derivative(_17918, _17921, intervalFailed);
    Interval _17902 = _19730;
    Interval _17903 = _19757;
    Interval _19764 = iadd(_17902, _17903, intervalFailed);
    Interval _17904 = Interval{ 0.0, 0.0 };
    Interval _17905 = _19760;
    Interval _19765 = jet_add_derivative(_17904, _17905, intervalFailed);
    Interval _17906 = Interval{ 0.0, 0.0 };
    Interval _17907 = _19763;
    Interval _19766 = jet_add_derivative(_17906, _17907, intervalFailed);
    Interval _17888 = Interval{ 9.0, 9.0 };
    Interval _17889 = _19764;
    Interval _19767 = imul(_17888, _17889, intervalFailed, optical_product_upper);
    Interval _17890 = Interval{ 0.0, 0.0 };
    Interval _17891 = _19764;
    Interval _19768 = jet_mul_derivative(_17890, _17891, intervalFailed, optical_product_upper);
    Interval _17892 = _19768;
    Interval _17893 = Interval{ 9.0, 9.0 };
    Interval _17894 = _19765;
    Interval _19769 = jet_mul_derivative(_17893, _17894, intervalFailed, optical_product_upper);
    Interval _17895 = _19769;
    Interval _19770 = jet_add_derivative(_17892, _17895, intervalFailed);
    Interval _17896 = Interval{ 0.0, 0.0 };
    Interval _17897 = _19764;
    Interval _19771 = jet_mul_derivative(_17896, _17897, intervalFailed, optical_product_upper);
    Interval _17898 = _19771;
    Interval _17899 = Interval{ 9.0, 9.0 };
    Interval _17900 = _19766;
    Interval _19772 = jet_mul_derivative(_17899, _17900, intervalFailed, optical_product_upper);
    Interval _17901 = _19772;
    Interval _19773 = jet_add_derivative(_17898, _17901, intervalFailed);
    float _17882 = _18988.lo;
    float _17883 = _18988.hi;
    float _19776 = sine_bounds(_17882, _17883, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19780 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19783 = as_type<float>(as_type<uint>(_19776) ^ 2147483648u);
    Interval _17878 = _18988;
    float _17876 = 3.1415927410125732421875;
    float _19784 = interval_down(_17876, intervalFailed);
    float _17877 = 3.1415927410125732421875;
    float _19785 = interval_up(_17877, intervalFailed);
    Interval _17879 = Interval{ _19784, _19785 };
    Interval _17880 = Interval{ 0.5, 0.5 };
    Interval _19787 = imul(_17879, _17880, intervalFailed, optical_product_upper);
    Interval _17881 = _19787;
    Interval _19788 = iadd(_17878, _17881, intervalFailed);
    float _17874 = _19788.lo;
    float _17875 = _19788.hi;
    float _19791 = sine_bounds(_17874, _17875, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17884 = Interval{ _19780, _19783 };
    Interval _17885 = _18989;
    Interval _19794 = jet_mul_derivative(_17884, _17885, intervalFailed, optical_product_upper);
    Interval _17886 = Interval{ _19780, _19783 };
    Interval _17887 = _18990;
    Interval _19796 = jet_mul_derivative(_17886, _17887, intervalFailed, optical_product_upper);
    Interval _17860 = _19767;
    Interval _17861 = Interval{ _19791, interval_sine_upper };
    Interval _19798 = imul(_17860, _17861, intervalFailed, optical_product_upper);
    Interval _17862 = _19770;
    Interval _17863 = Interval{ _19791, interval_sine_upper };
    Interval _19800 = jet_mul_derivative(_17862, _17863, intervalFailed, optical_product_upper);
    Interval _17864 = _19800;
    Interval _17865 = _19767;
    Interval _17866 = _19794;
    Interval _19801 = jet_mul_derivative(_17865, _17866, intervalFailed, optical_product_upper);
    Interval _17867 = _19801;
    Interval _19802 = jet_add_derivative(_17864, _17867, intervalFailed);
    Interval _17868 = _19773;
    Interval _17869 = Interval{ _19791, interval_sine_upper };
    Interval _19804 = jet_mul_derivative(_17868, _17869, intervalFailed, optical_product_upper);
    Interval _17870 = _19804;
    Interval _17871 = _19767;
    Interval _17872 = _19796;
    Interval _19805 = jet_mul_derivative(_17871, _17872, intervalFailed, optical_product_upper);
    Interval _17873 = _19805;
    Interval _19806 = jet_add_derivative(_17870, _17873, intervalFailed);
    Interval _17846 = _19798;
    Interval _17847 = _19312;
    Interval _19807 = imul(_17846, _17847, intervalFailed, optical_product_upper);
    Interval _17848 = _19802;
    Interval _17849 = _19312;
    Interval _19808 = jet_mul_derivative(_17848, _17849, intervalFailed, optical_product_upper);
    Interval _17850 = _19808;
    Interval _17851 = _19798;
    Interval _17852 = _19339;
    Interval _19809 = jet_mul_derivative(_17851, _17852, intervalFailed, optical_product_upper);
    Interval _17853 = _19809;
    Interval _19810 = jet_add_derivative(_17850, _17853, intervalFailed);
    Interval _17854 = _19806;
    Interval _17855 = _19312;
    Interval _19811 = jet_mul_derivative(_17854, _17855, intervalFailed, optical_product_upper);
    Interval _17856 = _19811;
    Interval _17857 = _19798;
    Interval _17858 = _19351;
    Interval _19812 = jet_mul_derivative(_17857, _17858, intervalFailed, optical_product_upper);
    Interval _17859 = _19812;
    Interval _19813 = jet_add_derivative(_17856, _17859, intervalFailed);
    float _17844 = 1175.0;
    float _17845 = 10000.0;
    Interval _19815 = iratio(_17844, _17845, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19820;
    if (!intervalFailed)
    {
        _19820 = intervalFailed;
    }
    else
    {
        _19820 = false;
    }
    bool _19825;
    if (_19820)
    {
        _19825 = jetFailureSite == 0u;
    }
    else
    {
        _19825 = false;
    }
    if (_19825)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(1175.0, 1175.0, 10000.0, 10000.0);
    }
    float _17838 = _19096.lo;
    float _17839 = _19096.hi;
    float _19830 = sine_bounds(_17838, _17839, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19834 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19837 = as_type<float>(as_type<uint>(_19830) ^ 2147483648u);
    Interval _17834 = _19096;
    float _17832 = 3.1415927410125732421875;
    float _19838 = interval_down(_17832, intervalFailed);
    float _17833 = 3.1415927410125732421875;
    float _19839 = interval_up(_17833, intervalFailed);
    Interval _17835 = Interval{ _19838, _19839 };
    Interval _17836 = Interval{ 0.5, 0.5 };
    Interval _19841 = imul(_17835, _17836, intervalFailed, optical_product_upper);
    Interval _17837 = _19841;
    Interval _19842 = iadd(_17834, _17837, intervalFailed);
    float _17830 = _19842.lo;
    float _17831 = _19842.hi;
    float _19845 = sine_bounds(_17830, _17831, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17840 = Interval{ _19834, _19837 };
    Interval _17841 = _19097;
    Interval _19848 = jet_mul_derivative(_17840, _17841, intervalFailed, optical_product_upper);
    Interval _17842 = Interval{ _19834, _19837 };
    Interval _17843 = _19098;
    Interval _19850 = jet_mul_derivative(_17842, _17843, intervalFailed, optical_product_upper);
    Interval _17816 = _19815;
    Interval _17817 = Interval{ _19845, interval_sine_upper };
    Interval _19852 = imul(_17816, _17817, intervalFailed, optical_product_upper);
    Interval _17818 = Interval{ 0.0, 0.0 };
    Interval _17819 = Interval{ _19845, interval_sine_upper };
    Interval _19854 = jet_mul_derivative(_17818, _17819, intervalFailed, optical_product_upper);
    Interval _17820 = _19854;
    Interval _17821 = _19815;
    Interval _17822 = _19848;
    Interval _19855 = jet_mul_derivative(_17821, _17822, intervalFailed, optical_product_upper);
    Interval _17823 = _19855;
    Interval _19856 = jet_add_derivative(_17820, _17823, intervalFailed);
    Interval _17824 = Interval{ 0.0, 0.0 };
    Interval _17825 = Interval{ _19845, interval_sine_upper };
    Interval _19858 = jet_mul_derivative(_17824, _17825, intervalFailed, optical_product_upper);
    Interval _17826 = _19858;
    Interval _17827 = _19815;
    Interval _17828 = _19850;
    Interval _19859 = jet_mul_derivative(_17827, _17828, intervalFailed, optical_product_upper);
    Interval _17829 = _19859;
    Interval _19860 = jet_add_derivative(_17826, _17829, intervalFailed);
    Interval _17802 = _19852;
    Interval _17803 = _19430;
    Interval _19861 = imul(_17802, _17803, intervalFailed, optical_product_upper);
    Interval _17804 = _19856;
    Interval _17805 = _19430;
    Interval _19862 = jet_mul_derivative(_17804, _17805, intervalFailed, optical_product_upper);
    Interval _17806 = _19862;
    Interval _17807 = _19852;
    Interval _17808 = _19457;
    Interval _19863 = jet_mul_derivative(_17807, _17808, intervalFailed, optical_product_upper);
    Interval _17809 = _19863;
    Interval _19864 = jet_add_derivative(_17806, _17809, intervalFailed);
    Interval _17810 = _19860;
    Interval _17811 = _19430;
    Interval _19865 = jet_mul_derivative(_17810, _17811, intervalFailed, optical_product_upper);
    Interval _17812 = _19865;
    Interval _17813 = _19852;
    Interval _17814 = _19469;
    Interval _19866 = jet_mul_derivative(_17813, _17814, intervalFailed, optical_product_upper);
    Interval _17815 = _19866;
    Interval _19867 = jet_add_derivative(_17812, _17815, intervalFailed);
    Interval _17796 = _19807;
    Interval _17797 = _19861;
    Interval _19868 = iadd(_17796, _17797, intervalFailed);
    Interval _17798 = _19810;
    Interval _17799 = _19864;
    Interval _19869 = jet_add_derivative(_17798, _17799, intervalFailed);
    Interval _17800 = _19813;
    Interval _17801 = _19867;
    Interval _19870 = jet_add_derivative(_17800, _17801, intervalFailed);
    float _17794 = 45.0;
    float _17795 = 1000.0;
    Interval _19872 = iratio(_17794, _17795, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19877;
    if (!intervalFailed)
    {
        _19877 = intervalFailed;
    }
    else
    {
        _19877 = false;
    }
    bool _19882;
    if (_19877)
    {
        _19882 = jetFailureSite == 0u;
    }
    else
    {
        _19882 = false;
    }
    if (_19882)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
    }
    float _17788 = _19229.lo;
    float _17789 = _19229.hi;
    float _19887 = sine_bounds(_17788, _17789, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19891 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19894 = as_type<float>(as_type<uint>(_19887) ^ 2147483648u);
    Interval _17784 = _19229;
    float _17782 = 3.1415927410125732421875;
    float _19895 = interval_down(_17782, intervalFailed);
    float _17783 = 3.1415927410125732421875;
    float _19896 = interval_up(_17783, intervalFailed);
    Interval _17785 = Interval{ _19895, _19896 };
    Interval _17786 = Interval{ 0.5, 0.5 };
    Interval _19898 = imul(_17785, _17786, intervalFailed, optical_product_upper);
    Interval _17787 = _19898;
    Interval _19899 = iadd(_17784, _17787, intervalFailed);
    float _17780 = _19899.lo;
    float _17781 = _19899.hi;
    float _19902 = sine_bounds(_17780, _17781, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17790 = Interval{ _19891, _19894 };
    Interval _17791 = _19231;
    Interval _19905 = jet_mul_derivative(_17790, _17791, intervalFailed, optical_product_upper);
    Interval _17792 = Interval{ _19891, _19894 };
    Interval _17793 = _19233;
    Interval _19907 = jet_mul_derivative(_17792, _17793, intervalFailed, optical_product_upper);
    Interval _17766 = _19872;
    Interval _17767 = Interval{ _19902, interval_sine_upper };
    Interval _19909 = imul(_17766, _17767, intervalFailed, optical_product_upper);
    Interval _17768 = Interval{ 0.0, 0.0 };
    Interval _17769 = Interval{ _19902, interval_sine_upper };
    Interval _19911 = jet_mul_derivative(_17768, _17769, intervalFailed, optical_product_upper);
    Interval _17770 = _19911;
    Interval _17771 = _19872;
    Interval _17772 = _19905;
    Interval _19912 = jet_mul_derivative(_17771, _17772, intervalFailed, optical_product_upper);
    Interval _17773 = _19912;
    Interval _19913 = jet_add_derivative(_17770, _17773, intervalFailed);
    Interval _17774 = Interval{ 0.0, 0.0 };
    Interval _17775 = Interval{ _19902, interval_sine_upper };
    Interval _19915 = jet_mul_derivative(_17774, _17775, intervalFailed, optical_product_upper);
    Interval _17776 = _19915;
    Interval _17777 = _19872;
    Interval _17778 = _19907;
    Interval _19916 = jet_mul_derivative(_17777, _17778, intervalFailed, optical_product_upper);
    Interval _17779 = _19916;
    Interval _19917 = jet_add_derivative(_17776, _17779, intervalFailed);
    Interval _17752 = _19909;
    Interval _17753 = _19548;
    Interval _19918 = imul(_17752, _17753, intervalFailed, optical_product_upper);
    Interval _17754 = _19913;
    Interval _17755 = _19548;
    Interval _19919 = jet_mul_derivative(_17754, _17755, intervalFailed, optical_product_upper);
    Interval _17756 = _19919;
    Interval _17757 = _19909;
    Interval _17758 = _19575;
    Interval _19920 = jet_mul_derivative(_17757, _17758, intervalFailed, optical_product_upper);
    Interval _17759 = _19920;
    Interval _19921 = jet_add_derivative(_17756, _17759, intervalFailed);
    Interval _17760 = _19917;
    Interval _17761 = _19548;
    Interval _19922 = jet_mul_derivative(_17760, _17761, intervalFailed, optical_product_upper);
    Interval _17762 = _19922;
    Interval _17763 = _19909;
    Interval _17764 = _19587;
    Interval _19923 = jet_mul_derivative(_17763, _17764, intervalFailed, optical_product_upper);
    Interval _17765 = _19923;
    Interval _19924 = jet_add_derivative(_17762, _17765, intervalFailed);
    Interval _17746 = _19868;
    Interval _17747 = Interval{ as_type<float>(as_type<uint>(_19918.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19918.lo) ^ 2147483648u) };
    Interval _19950 = iadd(_17746, _17747, intervalFailed);
    Interval _17748 = _19869;
    Interval _17749 = Interval{ as_type<float>(as_type<uint>(_19921.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19921.lo) ^ 2147483648u) };
    Interval _19952 = jet_add_derivative(_17748, _17749, intervalFailed);
    Interval _17750 = _19870;
    Interval _17751 = Interval{ as_type<float>(as_type<uint>(_19924.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19924.lo) ^ 2147483648u) };
    Interval _19954 = jet_add_derivative(_17750, _17751, intervalFailed);
    float _17744 = 11.0;
    float _17745 = 1000.0;
    Interval _19956 = iratio(_17744, _17745, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19961;
    if (!intervalFailed)
    {
        _19961 = intervalFailed;
    }
    else
    {
        _19961 = false;
    }
    bool _19966;
    if (_19961)
    {
        _19966 = jetFailureSite == 0u;
    }
    else
    {
        _19966 = false;
    }
    if (_19966)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    float _17742 = 52.0;
    float _17743 = 10000.0;
    Interval _19970 = iratio(_17742, _17743, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19975;
    if (!intervalFailed)
    {
        _19975 = intervalFailed;
    }
    else
    {
        _19975 = false;
    }
    bool _19980;
    if (_19975)
    {
        _19980 = jetFailureSite == 0u;
    }
    else
    {
        _19980 = false;
    }
    if (_19980)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(52.0, 52.0, 10000.0, 10000.0);
    }
    Interval _17728 = _19970;
    Interval _17729 = _19720;
    Interval _19983 = imul(_17728, _17729, intervalFailed, optical_product_upper);
    Interval _17730 = Interval{ 0.0, 0.0 };
    Interval _17731 = _19720;
    Interval _19984 = jet_mul_derivative(_17730, _17731, intervalFailed, optical_product_upper);
    Interval _17732 = _19984;
    Interval _17733 = _19970;
    Interval _17734 = _19724;
    Interval _19985 = jet_mul_derivative(_17733, _17734, intervalFailed, optical_product_upper);
    Interval _17735 = _19985;
    Interval _19986 = jet_add_derivative(_17732, _17735, intervalFailed);
    Interval _17736 = Interval{ 0.0, 0.0 };
    Interval _17737 = _19720;
    Interval _19987 = jet_mul_derivative(_17736, _17737, intervalFailed, optical_product_upper);
    Interval _17738 = _19987;
    Interval _17739 = _19970;
    Interval _17740 = _19728;
    Interval _19988 = jet_mul_derivative(_17739, _17740, intervalFailed, optical_product_upper);
    Interval _17741 = _19988;
    Interval _19989 = jet_add_derivative(_17738, _17741, intervalFailed);
    Interval _17722 = _19956;
    Interval _17723 = Interval{ as_type<float>(as_type<uint>(_19983.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19983.lo) ^ 2147483648u) };
    Interval _20015 = iadd(_17722, _17723, intervalFailed);
    Interval _17724 = Interval{ 0.0, 0.0 };
    Interval _17725 = Interval{ as_type<float>(as_type<uint>(_19986.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19986.lo) ^ 2147483648u) };
    Interval _20017 = jet_add_derivative(_17724, _17725, intervalFailed);
    Interval _17726 = Interval{ 0.0, 0.0 };
    Interval _17727 = Interval{ as_type<float>(as_type<uint>(_19989.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19989.lo) ^ 2147483648u) };
    Interval _20019 = jet_add_derivative(_17726, _17727, intervalFailed);
    Interval _17708 = Interval{ 9.0, 9.0 };
    Interval _17709 = _20015;
    Interval _20020 = imul(_17708, _17709, intervalFailed, optical_product_upper);
    Interval _17710 = Interval{ 0.0, 0.0 };
    Interval _17711 = _20015;
    Interval _20021 = jet_mul_derivative(_17710, _17711, intervalFailed, optical_product_upper);
    Interval _17712 = _20021;
    Interval _17713 = Interval{ 9.0, 9.0 };
    Interval _17714 = _20017;
    Interval _20022 = jet_mul_derivative(_17713, _17714, intervalFailed, optical_product_upper);
    Interval _17715 = _20022;
    Interval _20023 = jet_add_derivative(_17712, _17715, intervalFailed);
    Interval _17716 = Interval{ 0.0, 0.0 };
    Interval _17717 = _20015;
    Interval _20024 = jet_mul_derivative(_17716, _17717, intervalFailed, optical_product_upper);
    Interval _17718 = _20024;
    Interval _17719 = Interval{ 9.0, 9.0 };
    Interval _17720 = _20019;
    Interval _20025 = jet_mul_derivative(_17719, _17720, intervalFailed, optical_product_upper);
    Interval _17721 = _20025;
    Interval _20026 = jet_add_derivative(_17718, _17721, intervalFailed);
    float _17702 = _18988.lo;
    float _17703 = _18988.hi;
    float _20029 = sine_bounds(_17702, _17703, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20033 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20036 = as_type<float>(as_type<uint>(_20029) ^ 2147483648u);
    Interval _17698 = _18988;
    float _17696 = 3.1415927410125732421875;
    float _20037 = interval_down(_17696, intervalFailed);
    float _17697 = 3.1415927410125732421875;
    float _20038 = interval_up(_17697, intervalFailed);
    Interval _17699 = Interval{ _20037, _20038 };
    Interval _17700 = Interval{ 0.5, 0.5 };
    Interval _20040 = imul(_17699, _17700, intervalFailed, optical_product_upper);
    Interval _17701 = _20040;
    Interval _20041 = iadd(_17698, _17701, intervalFailed);
    float _17694 = _20041.lo;
    float _17695 = _20041.hi;
    float _20044 = sine_bounds(_17694, _17695, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17704 = Interval{ _20033, _20036 };
    Interval _17705 = _18989;
    Interval _20047 = jet_mul_derivative(_17704, _17705, intervalFailed, optical_product_upper);
    Interval _17706 = Interval{ _20033, _20036 };
    Interval _17707 = _18990;
    Interval _20049 = jet_mul_derivative(_17706, _17707, intervalFailed, optical_product_upper);
    Interval _17680 = _20020;
    Interval _17681 = Interval{ _20044, interval_sine_upper };
    Interval _20051 = imul(_17680, _17681, intervalFailed, optical_product_upper);
    Interval _17682 = _20023;
    Interval _17683 = Interval{ _20044, interval_sine_upper };
    Interval _20053 = jet_mul_derivative(_17682, _17683, intervalFailed, optical_product_upper);
    Interval _17684 = _20053;
    Interval _17685 = _20020;
    Interval _17686 = _20047;
    Interval _20054 = jet_mul_derivative(_17685, _17686, intervalFailed, optical_product_upper);
    Interval _17687 = _20054;
    Interval _20055 = jet_add_derivative(_17684, _17687, intervalFailed);
    Interval _17688 = _20026;
    Interval _17689 = Interval{ _20044, interval_sine_upper };
    Interval _20057 = jet_mul_derivative(_17688, _17689, intervalFailed, optical_product_upper);
    Interval _17690 = _20057;
    Interval _17691 = _20020;
    Interval _17692 = _20049;
    Interval _20058 = jet_mul_derivative(_17691, _17692, intervalFailed, optical_product_upper);
    Interval _17693 = _20058;
    Interval _20059 = jet_add_derivative(_17690, _17693, intervalFailed);
    Interval _17666 = _20051;
    Interval _17667 = _19312;
    Interval _20060 = imul(_17666, _17667, intervalFailed, optical_product_upper);
    Interval _17668 = _20055;
    Interval _17669 = _19312;
    Interval _20061 = jet_mul_derivative(_17668, _17669, intervalFailed, optical_product_upper);
    Interval _17670 = _20061;
    Interval _17671 = _20051;
    Interval _17672 = _19339;
    Interval _20062 = jet_mul_derivative(_17671, _17672, intervalFailed, optical_product_upper);
    Interval _17673 = _20062;
    Interval _20063 = jet_add_derivative(_17670, _17673, intervalFailed);
    Interval _17674 = _20059;
    Interval _17675 = _19312;
    Interval _20064 = jet_mul_derivative(_17674, _17675, intervalFailed, optical_product_upper);
    Interval _17676 = _20064;
    Interval _17677 = _20051;
    Interval _17678 = _19351;
    Interval _20065 = jet_mul_derivative(_17677, _17678, intervalFailed, optical_product_upper);
    Interval _17679 = _20065;
    Interval _20066 = jet_add_derivative(_17676, _17679, intervalFailed);
    float _17664 = 625.0;
    float _17665 = 10000.0;
    Interval _20068 = iratio(_17664, _17665, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20073;
    if (!intervalFailed)
    {
        _20073 = intervalFailed;
    }
    else
    {
        _20073 = false;
    }
    bool _20078;
    if (_20073)
    {
        _20078 = jetFailureSite == 0u;
    }
    else
    {
        _20078 = false;
    }
    if (_20078)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(625.0, 625.0, 10000.0, 10000.0);
    }
    float _17658 = _19096.lo;
    float _17659 = _19096.hi;
    float _20083 = sine_bounds(_17658, _17659, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20087 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20090 = as_type<float>(as_type<uint>(_20083) ^ 2147483648u);
    Interval _17654 = _19096;
    float _17652 = 3.1415927410125732421875;
    float _20091 = interval_down(_17652, intervalFailed);
    float _17653 = 3.1415927410125732421875;
    float _20092 = interval_up(_17653, intervalFailed);
    Interval _17655 = Interval{ _20091, _20092 };
    Interval _17656 = Interval{ 0.5, 0.5 };
    Interval _20094 = imul(_17655, _17656, intervalFailed, optical_product_upper);
    Interval _17657 = _20094;
    Interval _20095 = iadd(_17654, _17657, intervalFailed);
    float _17650 = _20095.lo;
    float _17651 = _20095.hi;
    float _20098 = sine_bounds(_17650, _17651, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17660 = Interval{ _20087, _20090 };
    Interval _17661 = _19097;
    Interval _20101 = jet_mul_derivative(_17660, _17661, intervalFailed, optical_product_upper);
    Interval _17662 = Interval{ _20087, _20090 };
    Interval _17663 = _19098;
    Interval _20103 = jet_mul_derivative(_17662, _17663, intervalFailed, optical_product_upper);
    Interval _17636 = _20068;
    Interval _17637 = Interval{ _20098, interval_sine_upper };
    Interval _20105 = imul(_17636, _17637, intervalFailed, optical_product_upper);
    Interval _17638 = Interval{ 0.0, 0.0 };
    Interval _17639 = Interval{ _20098, interval_sine_upper };
    Interval _20107 = jet_mul_derivative(_17638, _17639, intervalFailed, optical_product_upper);
    Interval _17640 = _20107;
    Interval _17641 = _20068;
    Interval _17642 = _20101;
    Interval _20108 = jet_mul_derivative(_17641, _17642, intervalFailed, optical_product_upper);
    Interval _17643 = _20108;
    Interval _20109 = jet_add_derivative(_17640, _17643, intervalFailed);
    Interval _17644 = Interval{ 0.0, 0.0 };
    Interval _17645 = Interval{ _20098, interval_sine_upper };
    Interval _20111 = jet_mul_derivative(_17644, _17645, intervalFailed, optical_product_upper);
    Interval _17646 = _20111;
    Interval _17647 = _20068;
    Interval _17648 = _20103;
    Interval _20112 = jet_mul_derivative(_17647, _17648, intervalFailed, optical_product_upper);
    Interval _17649 = _20112;
    Interval _20113 = jet_add_derivative(_17646, _17649, intervalFailed);
    Interval _17622 = _20105;
    Interval _17623 = _19430;
    Interval _20114 = imul(_17622, _17623, intervalFailed, optical_product_upper);
    Interval _17624 = _20109;
    Interval _17625 = _19430;
    Interval _20115 = jet_mul_derivative(_17624, _17625, intervalFailed, optical_product_upper);
    Interval _17626 = _20115;
    Interval _17627 = _20105;
    Interval _17628 = _19457;
    Interval _20116 = jet_mul_derivative(_17627, _17628, intervalFailed, optical_product_upper);
    Interval _17629 = _20116;
    Interval _20117 = jet_add_derivative(_17626, _17629, intervalFailed);
    Interval _17630 = _20113;
    Interval _17631 = _19430;
    Interval _20118 = jet_mul_derivative(_17630, _17631, intervalFailed, optical_product_upper);
    Interval _17632 = _20118;
    Interval _17633 = _20105;
    Interval _17634 = _19469;
    Interval _20119 = jet_mul_derivative(_17633, _17634, intervalFailed, optical_product_upper);
    Interval _17635 = _20119;
    Interval _20120 = jet_add_derivative(_17632, _17635, intervalFailed);
    Interval _17616 = _20060;
    Interval _17617 = Interval{ as_type<float>(as_type<uint>(_20114.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20114.lo) ^ 2147483648u) };
    Interval _20146 = iadd(_17616, _17617, intervalFailed);
    Interval _17618 = _20063;
    Interval _17619 = Interval{ as_type<float>(as_type<uint>(_20117.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20117.lo) ^ 2147483648u) };
    Interval _20148 = jet_add_derivative(_17618, _17619, intervalFailed);
    Interval _17620 = _20066;
    Interval _17621 = Interval{ as_type<float>(as_type<uint>(_20120.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20120.lo) ^ 2147483648u) };
    Interval _20150 = jet_add_derivative(_17620, _17621, intervalFailed);
    float _17614 = 110.0;
    float _17615 = 1000.0;
    Interval _20152 = iratio(_17614, _17615, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20157;
    if (!intervalFailed)
    {
        _20157 = intervalFailed;
    }
    else
    {
        _20157 = false;
    }
    bool _20162;
    if (_20157)
    {
        _20162 = jetFailureSite == 0u;
    }
    else
    {
        _20162 = false;
    }
    if (_20162)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(110.0, 110.0, 1000.0, 1000.0);
    }
    float _17608 = _19229.lo;
    float _17609 = _19229.hi;
    float _20167 = sine_bounds(_17608, _17609, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20171 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20174 = as_type<float>(as_type<uint>(_20167) ^ 2147483648u);
    Interval _17604 = _19229;
    float _17602 = 3.1415927410125732421875;
    float _20175 = interval_down(_17602, intervalFailed);
    float _17603 = 3.1415927410125732421875;
    float _20176 = interval_up(_17603, intervalFailed);
    Interval _17605 = Interval{ _20175, _20176 };
    Interval _17606 = Interval{ 0.5, 0.5 };
    Interval _20178 = imul(_17605, _17606, intervalFailed, optical_product_upper);
    Interval _17607 = _20178;
    Interval _20179 = iadd(_17604, _17607, intervalFailed);
    float _17600 = _20179.lo;
    float _17601 = _20179.hi;
    float _20182 = sine_bounds(_17600, _17601, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17610 = Interval{ _20171, _20174 };
    Interval _17611 = _19231;
    Interval _20185 = jet_mul_derivative(_17610, _17611, intervalFailed, optical_product_upper);
    Interval _17612 = Interval{ _20171, _20174 };
    Interval _17613 = _19233;
    Interval _20187 = jet_mul_derivative(_17612, _17613, intervalFailed, optical_product_upper);
    Interval _17586 = _20152;
    Interval _17587 = Interval{ _20182, interval_sine_upper };
    Interval _20189 = imul(_17586, _17587, intervalFailed, optical_product_upper);
    Interval _17588 = Interval{ 0.0, 0.0 };
    Interval _17589 = Interval{ _20182, interval_sine_upper };
    Interval _20191 = jet_mul_derivative(_17588, _17589, intervalFailed, optical_product_upper);
    Interval _17590 = _20191;
    Interval _17591 = _20152;
    Interval _17592 = _20185;
    Interval _20192 = jet_mul_derivative(_17591, _17592, intervalFailed, optical_product_upper);
    Interval _17593 = _20192;
    Interval _20193 = jet_add_derivative(_17590, _17593, intervalFailed);
    Interval _17594 = Interval{ 0.0, 0.0 };
    Interval _17595 = Interval{ _20182, interval_sine_upper };
    Interval _20195 = jet_mul_derivative(_17594, _17595, intervalFailed, optical_product_upper);
    Interval _17596 = _20195;
    Interval _17597 = _20152;
    Interval _17598 = _20187;
    Interval _20196 = jet_mul_derivative(_17597, _17598, intervalFailed, optical_product_upper);
    Interval _17599 = _20196;
    Interval _20197 = jet_add_derivative(_17596, _17599, intervalFailed);
    Interval _17572 = _20189;
    Interval _17573 = _19548;
    Interval _20198 = imul(_17572, _17573, intervalFailed, optical_product_upper);
    Interval _17574 = _20193;
    Interval _17575 = _19548;
    Interval _20199 = jet_mul_derivative(_17574, _17575, intervalFailed, optical_product_upper);
    Interval _17576 = _20199;
    Interval _17577 = _20189;
    Interval _17578 = _19575;
    Interval _20200 = jet_mul_derivative(_17577, _17578, intervalFailed, optical_product_upper);
    Interval _17579 = _20200;
    Interval _20201 = jet_add_derivative(_17576, _17579, intervalFailed);
    Interval _17580 = _20197;
    Interval _17581 = _19548;
    Interval _20202 = jet_mul_derivative(_17580, _17581, intervalFailed, optical_product_upper);
    Interval _17582 = _20202;
    Interval _17583 = _20189;
    Interval _17584 = _19587;
    Interval _20203 = jet_mul_derivative(_17583, _17584, intervalFailed, optical_product_upper);
    Interval _17585 = _20203;
    Interval _20204 = jet_add_derivative(_17582, _17585, intervalFailed);
    Interval _17566 = _20146;
    Interval _17567 = _20198;
    Interval _20205 = iadd(_17566, _17567, intervalFailed);
    Interval _17568 = _20148;
    Interval _17569 = _20201;
    Interval _20206 = jet_add_derivative(_17568, _17569, intervalFailed);
    Interval _17570 = _20150;
    Interval _17571 = _20204;
    Interval _20207 = jet_add_derivative(_17570, _17571, intervalFailed);
    float _17564 = 173.0;
    float _17565 = 1000.0;
    Interval _20213 = iratio(_17564, _17565, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20218;
    if (!intervalFailed)
    {
        _20218 = intervalFailed;
    }
    else
    {
        _20218 = false;
    }
    bool _20223;
    if (_20218)
    {
        _20223 = jetFailureSite == 0u;
    }
    else
    {
        _20223 = false;
    }
    if (_20223)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(173.0, 173.0, 1000.0, 1000.0);
    }
    Interval _17550 = x.v;
    Interval _17551 = _20213;
    Interval _20226 = imul(_17550, _17551, intervalFailed, optical_product_upper);
    Interval _17552 = x.dx;
    Interval _17553 = _20213;
    Interval _20227 = jet_mul_derivative(_17552, _17553, intervalFailed, optical_product_upper);
    Interval _17554 = _20227;
    Interval _17555 = x.v;
    Interval _17556 = Interval{ 0.0, 0.0 };
    Interval _20228 = jet_mul_derivative(_17555, _17556, intervalFailed, optical_product_upper);
    Interval _17557 = _20228;
    Interval _20229 = jet_add_derivative(_17554, _17557, intervalFailed);
    Interval _17558 = x.dy;
    Interval _17559 = _20213;
    Interval _20230 = jet_mul_derivative(_17558, _17559, intervalFailed, optical_product_upper);
    Interval _17560 = _20230;
    Interval _17561 = x.v;
    Interval _17562 = Interval{ 0.0, 0.0 };
    Interval _20231 = jet_mul_derivative(_17561, _17562, intervalFailed, optical_product_upper);
    Interval _17563 = _20231;
    Interval _20232 = jet_add_derivative(_17560, _17563, intervalFailed);
    float _17548 = 129.0;
    float _17549 = 1000.0;
    Interval _20238 = iratio(_17548, _17549, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20243;
    if (!intervalFailed)
    {
        _20243 = intervalFailed;
    }
    else
    {
        _20243 = false;
    }
    bool _20248;
    if (_20243)
    {
        _20248 = jetFailureSite == 0u;
    }
    else
    {
        _20248 = false;
    }
    if (_20248)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(129.0, 129.0, 1000.0, 1000.0);
    }
    Interval _17534 = z.v;
    Interval _17535 = _20238;
    Interval _20251 = imul(_17534, _17535, intervalFailed, optical_product_upper);
    Interval _17536 = z.dx;
    Interval _17537 = _20238;
    Interval _20252 = jet_mul_derivative(_17536, _17537, intervalFailed, optical_product_upper);
    Interval _17538 = _20252;
    Interval _17539 = z.v;
    Interval _17540 = Interval{ 0.0, 0.0 };
    Interval _20253 = jet_mul_derivative(_17539, _17540, intervalFailed, optical_product_upper);
    Interval _17541 = _20253;
    Interval _20254 = jet_add_derivative(_17538, _17541, intervalFailed);
    Interval _17542 = z.dy;
    Interval _17543 = _20238;
    Interval _20255 = jet_mul_derivative(_17542, _17543, intervalFailed, optical_product_upper);
    Interval _17544 = _20255;
    Interval _17545 = z.v;
    Interval _17546 = Interval{ 0.0, 0.0 };
    Interval _20256 = jet_mul_derivative(_17545, _17546, intervalFailed, optical_product_upper);
    Interval _17547 = _20256;
    Interval _20257 = jet_add_derivative(_17544, _17547, intervalFailed);
    Interval _17528 = _20226;
    Interval _17529 = _20251;
    Interval _20258 = iadd(_17528, _17529, intervalFailed);
    Interval _17530 = _20229;
    Interval _17531 = _20254;
    Interval _20259 = jet_add_derivative(_17530, _17531, intervalFailed);
    Interval _17532 = _20232;
    Interval _17533 = _20257;
    Interval _20260 = jet_add_derivative(_17532, _17533, intervalFailed);
    float _17526 = 73.0;
    float _17527 = 100.0;
    Interval _20266 = iratio(_17526, _17527, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20271;
    if (!intervalFailed)
    {
        _20271 = intervalFailed;
    }
    else
    {
        _20271 = false;
    }
    bool _20276;
    if (_20271)
    {
        _20276 = jetFailureSite == 0u;
    }
    else
    {
        _20276 = false;
    }
    if (_20276)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(73.0, 73.0, 100.0, 100.0);
    }
    Interval _17512 = t.v;
    Interval _17513 = _20266;
    Interval _20279 = imul(_17512, _17513, intervalFailed, optical_product_upper);
    Interval _17514 = t.dx;
    Interval _17515 = _20266;
    Interval _20280 = jet_mul_derivative(_17514, _17515, intervalFailed, optical_product_upper);
    Interval _17516 = _20280;
    Interval _17517 = t.v;
    Interval _17518 = Interval{ 0.0, 0.0 };
    Interval _20281 = jet_mul_derivative(_17517, _17518, intervalFailed, optical_product_upper);
    Interval _17519 = _20281;
    Interval _20282 = jet_add_derivative(_17516, _17519, intervalFailed);
    Interval _17520 = t.dy;
    Interval _17521 = _20266;
    Interval _20283 = jet_mul_derivative(_17520, _17521, intervalFailed, optical_product_upper);
    Interval _17522 = _20283;
    Interval _17523 = t.v;
    Interval _17524 = Interval{ 0.0, 0.0 };
    Interval _20284 = jet_mul_derivative(_17523, _17524, intervalFailed, optical_product_upper);
    Interval _17525 = _20284;
    Interval _20285 = jet_add_derivative(_17522, _17525, intervalFailed);
    Interval _17506 = _20258;
    Interval _17507 = Interval{ as_type<float>(as_type<uint>(_20279.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20279.lo) ^ 2147483648u) };
    Interval _20311 = iadd(_17506, _17507, intervalFailed);
    Interval _17508 = _20259;
    Interval _17509 = Interval{ as_type<float>(as_type<uint>(_20282.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20282.lo) ^ 2147483648u) };
    Interval _20313 = jet_add_derivative(_17508, _17509, intervalFailed);
    Interval _17510 = _20260;
    Interval _17511 = Interval{ as_type<float>(as_type<uint>(_20285.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20285.lo) ^ 2147483648u) };
    Interval _20315 = jet_add_derivative(_17510, _17511, intervalFailed);
    float _17504 = 216.0;
    float _17505 = 1000.0;
    Interval _20318 = iratio(_17504, _17505, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20323;
    if (!intervalFailed)
    {
        _20323 = intervalFailed;
    }
    else
    {
        _20323 = false;
    }
    bool _20328;
    if (_20323)
    {
        _20328 = jetFailureSite == 0u;
    }
    else
    {
        _20328 = false;
    }
    if (_20328)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(216.0, 216.0, 1000.0, 1000.0);
    }
    Interval _17490 = footprint.v;
    Interval _17491 = _20318;
    Interval _20334 = imul(_17490, _17491, intervalFailed, optical_product_upper);
    Interval _17492 = footprint.dx;
    Interval _17493 = _20318;
    Interval _20335 = jet_mul_derivative(_17492, _17493, intervalFailed, optical_product_upper);
    Interval _17494 = _20335;
    Interval _17495 = footprint.v;
    Interval _17496 = Interval{ 0.0, 0.0 };
    Interval _20336 = jet_mul_derivative(_17495, _17496, intervalFailed, optical_product_upper);
    Interval _17497 = _20336;
    Interval _20337 = jet_add_derivative(_17494, _17497, intervalFailed);
    Interval _17498 = footprint.dy;
    Interval _17499 = _20318;
    Interval _20338 = jet_mul_derivative(_17498, _17499, intervalFailed, optical_product_upper);
    Interval _17500 = _20338;
    Interval _17501 = footprint.v;
    Interval _17502 = Interval{ 0.0, 0.0 };
    Interval _20339 = jet_mul_derivative(_17501, _17502, intervalFailed, optical_product_upper);
    Interval _17503 = _20339;
    Interval _20340 = jet_add_derivative(_17500, _17503, intervalFailed);
    bool _20347;
    if (_20334.lo <= 0.0)
    {
        _20347 = _20334.hi >= 0.0;
    }
    else
    {
        _20347 = false;
    }
    float _20354;
    if (_20347)
    {
        _20354 = 0.0;
    }
    else
    {
        _20354 = precise::min(abs(_20334.lo), abs(_20334.hi));
    }
    float _20357 = precise::max(abs(_20334.lo), abs(_20334.hi));
    float _17480 = spvFMul(_20354, _20354);
    float _20358 = interval_down(_17480, intervalFailed);
    float _20359 = precise::max(0.0, _20358);
    float _17481 = spvFMul(_20357, _20357);
    float _20360 = interval_up(_17481, intervalFailed);
    Interval _17482 = Interval{ 2.0, 2.0 };
    Interval _17483 = _20334;
    Interval _20361 = imul(_17482, _17483, intervalFailed, optical_product_upper);
    Interval _17484 = _20361;
    Interval _17485 = _20337;
    Interval _20362 = jet_mul_derivative(_17484, _17485, intervalFailed, optical_product_upper);
    Interval _17486 = Interval{ 2.0, 2.0 };
    Interval _17487 = _20334;
    Interval _20363 = imul(_17486, _17487, intervalFailed, optical_product_upper);
    Interval _17488 = _20363;
    Interval _17489 = _20340;
    Interval _20364 = jet_mul_derivative(_17488, _17489, intervalFailed, optical_product_upper);
    bool _20369;
    if (_20359 <= 0.0)
    {
        _20369 = _20360 >= 0.0;
    }
    else
    {
        _20369 = false;
    }
    float _20376;
    if (_20369)
    {
        _20376 = 0.0;
    }
    else
    {
        _20376 = precise::min(abs(_20359), abs(_20360));
    }
    float _20379 = precise::max(abs(_20359), abs(_20360));
    float _17470 = spvFMul(_20376, _20376);
    float _20380 = interval_down(_17470, intervalFailed);
    float _17471 = spvFMul(_20379, _20379);
    float _20382 = interval_up(_17471, intervalFailed);
    Interval _17472 = Interval{ 2.0, 2.0 };
    Interval _17473 = Interval{ _20359, _20360 };
    Interval _20384 = imul(_17472, _17473, intervalFailed, optical_product_upper);
    Interval _17474 = _20384;
    Interval _17475 = _20362;
    Interval _20385 = jet_mul_derivative(_17474, _17475, intervalFailed, optical_product_upper);
    Interval _17476 = Interval{ 2.0, 2.0 };
    Interval _17477 = Interval{ _20359, _20360 };
    Interval _20387 = imul(_17476, _17477, intervalFailed, optical_product_upper);
    Interval _17478 = _20387;
    Interval _17479 = _20364;
    Interval _20388 = jet_mul_derivative(_17478, _17479, intervalFailed, optical_product_upper);
    Interval _17464 = Interval{ 1.0, 1.0 };
    Interval _17465 = Interval{ precise::max(0.0, _20380), _20382 };
    Interval _20390 = iadd(_17464, _17465, intervalFailed);
    Interval _17466 = Interval{ 0.0, 0.0 };
    Interval _17467 = _20385;
    Interval _20391 = jet_add_derivative(_17466, _17467, intervalFailed);
    Interval _17468 = Interval{ 0.0, 0.0 };
    Interval _17469 = _20388;
    Interval _20392 = jet_add_derivative(_17468, _17469, intervalFailed);
    Interval _17450 = Interval{ 1.0, 1.0 };
    Interval _17451 = _20390;
    Interval _20394 = idiv(_17450, _17451, intervalFailed, interval_divide_upper);
    bool _20401;
    if (!intervalFailed)
    {
        _20401 = intervalFailed;
    }
    else
    {
        _20401 = false;
    }
    bool _20406;
    if (_20401)
    {
        _20406 = jetFailureSite == 0u;
    }
    else
    {
        _20406 = false;
    }
    if (_20406)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _20390.lo, _20390.hi);
    }
    Interval _17452 = Interval{ 0.0, 0.0 };
    Interval _17453 = _20394;
    Interval _17454 = _20391;
    Interval _20410 = jet_mul_derivative(_17453, _17454, intervalFailed, optical_product_upper);
    Interval _17455 = Interval{ as_type<float>(as_type<uint>(_20410.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20410.lo) ^ 2147483648u) };
    Interval _20420 = jet_add_derivative(_17452, _17455, intervalFailed);
    Interval _17456 = _20420;
    Interval _17457 = _20390;
    Interval _20421 = jet_div_derivative(_17456, _17457, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17458 = Interval{ 0.0, 0.0 };
    Interval _17459 = _20394;
    Interval _17460 = _20392;
    Interval _20422 = jet_mul_derivative(_17459, _17460, intervalFailed, optical_product_upper);
    Interval _17461 = Interval{ as_type<float>(as_type<uint>(_20422.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20422.lo) ^ 2147483648u) };
    Interval _20432 = jet_add_derivative(_17458, _17461, intervalFailed);
    Interval _17462 = _20432;
    Interval _17463 = _20390;
    Interval _20433 = jet_div_derivative(_17462, _17463, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _17448 = 38.0;
    float _17449 = 100.0;
    Interval _20435 = iratio(_17448, _17449, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20440;
    if (!intervalFailed)
    {
        _20440 = intervalFailed;
    }
    else
    {
        _20440 = false;
    }
    bool _20445;
    if (_20440)
    {
        _20445 = jetFailureSite == 0u;
    }
    else
    {
        _20445 = false;
    }
    if (_20445)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(38.0, 38.0, 100.0, 100.0);
    }
    Interval _17440 = _20311;
    float _17438 = 3.1415927410125732421875;
    float _20448 = interval_down(_17438, intervalFailed);
    float _17439 = 3.1415927410125732421875;
    float _20449 = interval_up(_17439, intervalFailed);
    Interval _17441 = Interval{ _20448, _20449 };
    Interval _17442 = Interval{ 0.5, 0.5 };
    Interval _20451 = imul(_17441, _17442, intervalFailed, optical_product_upper);
    Interval _17443 = _20451;
    Interval _20452 = iadd(_17440, _17443, intervalFailed);
    float _17436 = _20452.lo;
    float _17437 = _20452.hi;
    float _20455 = sine_bounds(_17436, _17437, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17434 = _20311.lo;
    float _17435 = _20311.hi;
    float _20459 = sine_bounds(_17434, _17435, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17444 = Interval{ _20455, interval_sine_upper };
    Interval _17445 = _20313;
    Interval _20462 = jet_mul_derivative(_17444, _17445, intervalFailed, optical_product_upper);
    Interval _17446 = Interval{ _20455, interval_sine_upper };
    Interval _17447 = _20315;
    Interval _20464 = jet_mul_derivative(_17446, _17447, intervalFailed, optical_product_upper);
    Interval _17420 = _20435;
    Interval _17421 = Interval{ _20459, interval_sine_upper };
    Interval _20466 = imul(_17420, _17421, intervalFailed, optical_product_upper);
    Interval _17422 = Interval{ 0.0, 0.0 };
    Interval _17423 = Interval{ _20459, interval_sine_upper };
    Interval _20468 = jet_mul_derivative(_17422, _17423, intervalFailed, optical_product_upper);
    Interval _17424 = _20468;
    Interval _17425 = _20435;
    Interval _17426 = _20462;
    Interval _20469 = jet_mul_derivative(_17425, _17426, intervalFailed, optical_product_upper);
    Interval _17427 = _20469;
    Interval _20470 = jet_add_derivative(_17424, _17427, intervalFailed);
    Interval _17428 = Interval{ 0.0, 0.0 };
    Interval _17429 = Interval{ _20459, interval_sine_upper };
    Interval _20472 = jet_mul_derivative(_17428, _17429, intervalFailed, optical_product_upper);
    Interval _17430 = _20472;
    Interval _17431 = _20435;
    Interval _17432 = _20464;
    Interval _20473 = jet_mul_derivative(_17431, _17432, intervalFailed, optical_product_upper);
    Interval _17433 = _20473;
    Interval _20474 = jet_add_derivative(_17430, _17433, intervalFailed);
    Interval _17406 = _20466;
    Interval _17407 = _20394;
    Interval _20475 = imul(_17406, _17407, intervalFailed, optical_product_upper);
    Interval _17408 = _20470;
    Interval _17409 = _20394;
    Interval _20476 = jet_mul_derivative(_17408, _17409, intervalFailed, optical_product_upper);
    Interval _17410 = _20476;
    Interval _17411 = _20466;
    Interval _17412 = _20421;
    Interval _20477 = jet_mul_derivative(_17411, _17412, intervalFailed, optical_product_upper);
    Interval _17413 = _20477;
    Interval _20478 = jet_add_derivative(_17410, _17413, intervalFailed);
    Interval _17414 = _20474;
    Interval _17415 = _20394;
    Interval _20479 = jet_mul_derivative(_17414, _17415, intervalFailed, optical_product_upper);
    Interval _17416 = _20479;
    Interval _17417 = _20466;
    Interval _17418 = _20433;
    Interval _20480 = jet_mul_derivative(_17417, _17418, intervalFailed, optical_product_upper);
    Interval _17419 = _20480;
    Interval _20481 = jet_add_derivative(_17416, _17419, intervalFailed);
    Interval _17400 = _19693;
    Interval _17401 = _20475;
    Interval _20482 = iadd(_17400, _17401, intervalFailed);
    Interval _17402 = _19694;
    Interval _17403 = _20478;
    Interval _20483 = jet_add_derivative(_17402, _17403, intervalFailed);
    Interval _17404 = _19695;
    Interval _17405 = _20481;
    Interval _20484 = jet_add_derivative(_17404, _17405, intervalFailed);
    float _17398 = 6574.0;
    float _17399 = 100000.0;
    Interval _20492 = iratio(_17398, _17399, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20497;
    if (!intervalFailed)
    {
        _20497 = intervalFailed;
    }
    else
    {
        _20497 = false;
    }
    bool _20502;
    if (_20497)
    {
        _20502 = jetFailureSite == 0u;
    }
    else
    {
        _20502 = false;
    }
    if (_20502)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6574.0, 6574.0, 100000.0, 100000.0);
    }
    float _17392 = _20311.lo;
    float _17393 = _20311.hi;
    float _20507 = sine_bounds(_17392, _17393, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20511 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20514 = as_type<float>(as_type<uint>(_20507) ^ 2147483648u);
    Interval _17388 = _20311;
    float _17386 = 3.1415927410125732421875;
    float _20515 = interval_down(_17386, intervalFailed);
    float _17387 = 3.1415927410125732421875;
    float _20516 = interval_up(_17387, intervalFailed);
    Interval _17389 = Interval{ _20515, _20516 };
    Interval _17390 = Interval{ 0.5, 0.5 };
    Interval _20518 = imul(_17389, _17390, intervalFailed, optical_product_upper);
    Interval _17391 = _20518;
    Interval _20519 = iadd(_17388, _17391, intervalFailed);
    float _17384 = _20519.lo;
    float _17385 = _20519.hi;
    float _20522 = sine_bounds(_17384, _17385, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17394 = Interval{ _20511, _20514 };
    Interval _17395 = _20313;
    Interval _20525 = jet_mul_derivative(_17394, _17395, intervalFailed, optical_product_upper);
    Interval _17396 = Interval{ _20511, _20514 };
    Interval _17397 = _20315;
    Interval _20527 = jet_mul_derivative(_17396, _17397, intervalFailed, optical_product_upper);
    Interval _17370 = _20492;
    Interval _17371 = Interval{ _20522, interval_sine_upper };
    Interval _20529 = imul(_17370, _17371, intervalFailed, optical_product_upper);
    Interval _17372 = Interval{ 0.0, 0.0 };
    Interval _17373 = Interval{ _20522, interval_sine_upper };
    Interval _20531 = jet_mul_derivative(_17372, _17373, intervalFailed, optical_product_upper);
    Interval _17374 = _20531;
    Interval _17375 = _20492;
    Interval _17376 = _20525;
    Interval _20532 = jet_mul_derivative(_17375, _17376, intervalFailed, optical_product_upper);
    Interval _17377 = _20532;
    Interval _20533 = jet_add_derivative(_17374, _17377, intervalFailed);
    Interval _17378 = Interval{ 0.0, 0.0 };
    Interval _17379 = Interval{ _20522, interval_sine_upper };
    Interval _20535 = jet_mul_derivative(_17378, _17379, intervalFailed, optical_product_upper);
    Interval _17380 = _20535;
    Interval _17381 = _20492;
    Interval _17382 = _20527;
    Interval _20536 = jet_mul_derivative(_17381, _17382, intervalFailed, optical_product_upper);
    Interval _17383 = _20536;
    Interval _20537 = jet_add_derivative(_17380, _17383, intervalFailed);
    Interval _17356 = _20529;
    Interval _17357 = _20394;
    Interval _20538 = imul(_17356, _17357, intervalFailed, optical_product_upper);
    Interval _17358 = _20533;
    Interval _17359 = _20394;
    Interval _20539 = jet_mul_derivative(_17358, _17359, intervalFailed, optical_product_upper);
    Interval _17360 = _20539;
    Interval _17361 = _20529;
    Interval _17362 = _20421;
    Interval _20540 = jet_mul_derivative(_17361, _17362, intervalFailed, optical_product_upper);
    Interval _17363 = _20540;
    Interval _20541 = jet_add_derivative(_17360, _17363, intervalFailed);
    Interval _17364 = _20537;
    Interval _17365 = _20394;
    Interval _20542 = jet_mul_derivative(_17364, _17365, intervalFailed, optical_product_upper);
    Interval _17366 = _20542;
    Interval _17367 = _20529;
    Interval _17368 = _20433;
    Interval _20543 = jet_mul_derivative(_17367, _17368, intervalFailed, optical_product_upper);
    Interval _17369 = _20543;
    Interval _20544 = jet_add_derivative(_17366, _17369, intervalFailed);
    Interval _17350 = _19950;
    Interval _17351 = _20538;
    Interval _20545 = iadd(_17350, _17351, intervalFailed);
    Interval _17352 = _19952;
    Interval _17353 = _20541;
    Interval _20546 = jet_add_derivative(_17352, _17353, intervalFailed);
    Interval _17354 = _19954;
    Interval _17355 = _20544;
    Interval _20547 = jet_add_derivative(_17354, _17355, intervalFailed);
    float _17348 = 4902.0;
    float _17349 = 100000.0;
    Interval _20555 = iratio(_17348, _17349, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20560;
    if (!intervalFailed)
    {
        _20560 = intervalFailed;
    }
    else
    {
        _20560 = false;
    }
    bool _20565;
    if (_20560)
    {
        _20565 = jetFailureSite == 0u;
    }
    else
    {
        _20565 = false;
    }
    if (_20565)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(4902.0, 4902.0, 100000.0, 100000.0);
    }
    float _17342 = _20311.lo;
    float _17343 = _20311.hi;
    float _20570 = sine_bounds(_17342, _17343, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20574 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20577 = as_type<float>(as_type<uint>(_20570) ^ 2147483648u);
    Interval _17338 = _20311;
    float _17336 = 3.1415927410125732421875;
    float _20578 = interval_down(_17336, intervalFailed);
    float _17337 = 3.1415927410125732421875;
    float _20579 = interval_up(_17337, intervalFailed);
    Interval _17339 = Interval{ _20578, _20579 };
    Interval _17340 = Interval{ 0.5, 0.5 };
    Interval _20581 = imul(_17339, _17340, intervalFailed, optical_product_upper);
    Interval _17341 = _20581;
    Interval _20582 = iadd(_17338, _17341, intervalFailed);
    float _17334 = _20582.lo;
    float _17335 = _20582.hi;
    float _20585 = sine_bounds(_17334, _17335, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17344 = Interval{ _20574, _20577 };
    Interval _17345 = _20313;
    Interval _20588 = jet_mul_derivative(_17344, _17345, intervalFailed, optical_product_upper);
    Interval _17346 = Interval{ _20574, _20577 };
    Interval _17347 = _20315;
    Interval _20590 = jet_mul_derivative(_17346, _17347, intervalFailed, optical_product_upper);
    Interval _17320 = _20555;
    Interval _17321 = Interval{ _20585, interval_sine_upper };
    Interval _20592 = imul(_17320, _17321, intervalFailed, optical_product_upper);
    Interval _17322 = Interval{ 0.0, 0.0 };
    Interval _17323 = Interval{ _20585, interval_sine_upper };
    Interval _20594 = jet_mul_derivative(_17322, _17323, intervalFailed, optical_product_upper);
    Interval _17324 = _20594;
    Interval _17325 = _20555;
    Interval _17326 = _20588;
    Interval _20595 = jet_mul_derivative(_17325, _17326, intervalFailed, optical_product_upper);
    Interval _17327 = _20595;
    Interval _20596 = jet_add_derivative(_17324, _17327, intervalFailed);
    Interval _17328 = Interval{ 0.0, 0.0 };
    Interval _17329 = Interval{ _20585, interval_sine_upper };
    Interval _20598 = jet_mul_derivative(_17328, _17329, intervalFailed, optical_product_upper);
    Interval _17330 = _20598;
    Interval _17331 = _20555;
    Interval _17332 = _20590;
    Interval _20599 = jet_mul_derivative(_17331, _17332, intervalFailed, optical_product_upper);
    Interval _17333 = _20599;
    Interval _20600 = jet_add_derivative(_17330, _17333, intervalFailed);
    Interval _17306 = _20592;
    Interval _17307 = _20394;
    Interval _20601 = imul(_17306, _17307, intervalFailed, optical_product_upper);
    Interval _17308 = _20596;
    Interval _17309 = _20394;
    Interval _20602 = jet_mul_derivative(_17308, _17309, intervalFailed, optical_product_upper);
    Interval _17310 = _20602;
    Interval _17311 = _20592;
    Interval _17312 = _20421;
    Interval _20603 = jet_mul_derivative(_17311, _17312, intervalFailed, optical_product_upper);
    Interval _17313 = _20603;
    Interval _20604 = jet_add_derivative(_17310, _17313, intervalFailed);
    Interval _17314 = _20600;
    Interval _17315 = _20394;
    Interval _20605 = jet_mul_derivative(_17314, _17315, intervalFailed, optical_product_upper);
    Interval _17316 = _20605;
    Interval _17317 = _20592;
    Interval _17318 = _20433;
    Interval _20606 = jet_mul_derivative(_17317, _17318, intervalFailed, optical_product_upper);
    Interval _17319 = _20606;
    Interval _20607 = jet_add_derivative(_17316, _17319, intervalFailed);
    Interval _17300 = _20205;
    Interval _17301 = _20601;
    Interval _20608 = iadd(_17300, _17301, intervalFailed);
    Interval _17302 = _20206;
    Interval _17303 = _20604;
    Interval _20609 = jet_add_derivative(_17302, _17303, intervalFailed);
    Interval _17304 = _20207;
    Interval _17305 = _20607;
    Interval _20610 = jet_add_derivative(_17304, _17305, intervalFailed);
    float _21616;
    float _21617;
    float _21618;
    float _21619;
    float _21620;
    float _21621;
    float _21622;
    float _21623;
    float _21624;
    float _21625;
    float _21626;
    float _21627;
    float _21628;
    float _21629;
    float _21630;
    float _21631;
    float _21632;
    float _21633;
    if (footprint.v.lo < 24.0)
    {
        if (footprint.v.hi >= 24.0)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20482, _20483, _20484 }, OpticalJet{ _20545, _20546, _20547 }, OpticalJet{ _20608, _20609, _20610 } };
        }
        Interval _17286 = x.v;
        Interval _17287 = Interval{ 128.0, 128.0 };
        Interval _20632 = idiv(_17286, _17287, intervalFailed, interval_divide_upper);
        bool _20639;
        if (!intervalFailed)
        {
            _20639 = intervalFailed;
        }
        else
        {
            _20639 = false;
        }
        bool _20644;
        if (_20639)
        {
            _20644 = jetFailureSite == 0u;
        }
        else
        {
            _20644 = false;
        }
        if (_20644)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(x.v.lo, x.v.hi, 128.0, 128.0);
        }
        Interval _17288 = x.dx;
        Interval _17289 = _20632;
        Interval _17290 = Interval{ 0.0, 0.0 };
        Interval _20648 = jet_mul_derivative(_17289, _17290, intervalFailed, optical_product_upper);
        Interval _17291 = Interval{ as_type<float>(as_type<uint>(_20648.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20648.lo) ^ 2147483648u) };
        Interval _20658 = jet_add_derivative(_17288, _17291, intervalFailed);
        Interval _17292 = _20658;
        Interval _17293 = Interval{ 128.0, 128.0 };
        Interval _20659 = jet_div_derivative(_17292, _17293, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17294 = x.dy;
        Interval _17295 = _20632;
        Interval _17296 = Interval{ 0.0, 0.0 };
        Interval _20660 = jet_mul_derivative(_17295, _17296, intervalFailed, optical_product_upper);
        Interval _17297 = Interval{ as_type<float>(as_type<uint>(_20660.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20660.lo) ^ 2147483648u) };
        Interval _20670 = jet_add_derivative(_17294, _17297, intervalFailed);
        Interval _17298 = _20670;
        Interval _17299 = Interval{ 128.0, 128.0 };
        Interval _20671 = jet_div_derivative(_17298, _17299, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17272 = z.v;
        Interval _17273 = Interval{ 128.0, 128.0 };
        Interval _20679 = idiv(_17272, _17273, intervalFailed, interval_divide_upper);
        bool _20686;
        if (!intervalFailed)
        {
            _20686 = intervalFailed;
        }
        else
        {
            _20686 = false;
        }
        bool _20691;
        if (_20686)
        {
            _20691 = jetFailureSite == 0u;
        }
        else
        {
            _20691 = false;
        }
        if (_20691)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(z.v.lo, z.v.hi, 128.0, 128.0);
        }
        Interval _17274 = z.dx;
        Interval _17275 = _20679;
        Interval _17276 = Interval{ 0.0, 0.0 };
        Interval _20695 = jet_mul_derivative(_17275, _17276, intervalFailed, optical_product_upper);
        Interval _17277 = Interval{ as_type<float>(as_type<uint>(_20695.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20695.lo) ^ 2147483648u) };
        Interval _20705 = jet_add_derivative(_17274, _17277, intervalFailed);
        Interval _17278 = _20705;
        Interval _17279 = Interval{ 128.0, 128.0 };
        Interval _20706 = jet_div_derivative(_17278, _17279, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17280 = z.dy;
        Interval _17281 = _20679;
        Interval _17282 = Interval{ 0.0, 0.0 };
        Interval _20707 = jet_mul_derivative(_17281, _17282, intervalFailed, optical_product_upper);
        Interval _17283 = Interval{ as_type<float>(as_type<uint>(_20707.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20707.lo) ^ 2147483648u) };
        Interval _20717 = jet_add_derivative(_17280, _17283, intervalFailed);
        Interval _17284 = _20717;
        Interval _17285 = Interval{ 128.0, 128.0 };
        Interval _20718 = jet_div_derivative(_17284, _17285, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        float _20721 = floor(_20632.lo);
        float _20722 = floor(_20632.hi);
        bool _20727;
        if ((isunordered(_20721, _20722) || _20721 == _20722))
        {
            _20727 = floor(_20679.lo) != floor(_20679.hi);
        }
        else
        {
            _20727 = true;
        }
        float _21594;
        float _21595;
        float _21596;
        float _21597;
        float _21598;
        float _21599;
        float _21600;
        float _21601;
        float _21602;
        float _21603;
        float _21604;
        float _21605;
        float _21606;
        float _21607;
        float _21608;
        float _21609;
        float _21610;
        float _21611;
        if (_20727)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20482, _20483, _20484 }, OpticalJet{ _20545, _20546, _20547 }, OpticalJet{ _20608, _20609, _20610 } };
        }
        else
        {
            int _20729 = int(floor(_20632.lo));
            int _20731 = int(floor(_20679.lo));
            uint _20735 = (uint(_20729) * 1597334677u) ^ (uint(_20731) * 3812015801u);
            uint _1989 = (_20735 ^ (_20735 >> 16u)) * 2246822519u;
            float _17270 = float((_1989 ^ (_1989 >> 13u)) & 65535u);
            float _17271 = 65535.0;
            Interval _20742 = iratio(_17270, _17271, intervalFailed, optical_product_upper, interval_divide_upper);
            float _20743 = float(_20729);
            float _20744 = float(_20731);
            bool _20749;
            if (!intervalFailed)
            {
                _20749 = intervalFailed;
            }
            else
            {
                _20749 = false;
            }
            bool _20754;
            if (_20749)
            {
                _20754 = jetFailureSite == 0u;
            }
            else
            {
                _20754 = false;
            }
            if (_20754)
            {
                jetFailureSite = 7u;
                jetFailureArguments = float4(_20743, _20743, _20744, _20744);
            }
            float param_var_n = 64.0;
            float param_var_d = 100.0;
            Interval _20760 = iratio(param_var_n, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _20765;
            if (_20742.lo <= _20760.hi)
            {
                _20765 = _20742.hi > _20760.lo;
            }
            else
            {
                _20765 = false;
            }
            if (_20765)
            {
                jetBranchKnown = false;
                return OpticalJet3{ OpticalJet{ _20482, _20483, _20484 }, OpticalJet{ _20545, _20546, _20547 }, OpticalJet{ _20608, _20609, _20610 } };
            }
            if (_20742.lo > _20760.hi)
            {
                float _20771 = float(_20729);
                Interval _17264 = _20632;
                Interval _17265 = Interval{ as_type<float>(as_type<uint>(_20771) ^ 2147483648u), as_type<float>(as_type<uint>(_20771) ^ 2147483648u) };
                Interval _20783 = iadd(_17264, _17265, intervalFailed);
                Interval _17266 = _20659;
                Interval _17267 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20785 = jet_add_derivative(_17266, _17267, intervalFailed);
                Interval _17268 = _20671;
                Interval _17269 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20787 = jet_add_derivative(_17268, _17269, intervalFailed);
                float _17262 = 28.0;
                float _17263 = 100.0;
                Interval _20789 = iratio(_17262, _17263, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20794;
                if (!intervalFailed)
                {
                    _20794 = intervalFailed;
                }
                else
                {
                    _20794 = false;
                }
                bool _20799;
                if (_20794)
                {
                    _20799 = jetFailureSite == 0u;
                }
                else
                {
                    _20799 = false;
                }
                if (_20799)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17260 = 44.0;
                float _17261 = 100.0;
                Interval _20803 = iratio(_17260, _17261, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20808;
                if (!intervalFailed)
                {
                    _20808 = intervalFailed;
                }
                else
                {
                    _20808 = false;
                }
                bool _20813;
                if (_20808)
                {
                    _20813 = jetFailureSite == 0u;
                }
                else
                {
                    _20813 = false;
                }
                if (_20813)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1792 = _20729 + 19;
                uint _20819 = (uint(_1792) * 1597334677u) ^ (uint(_20731) * 3812015801u);
                uint _1992 = (_20819 ^ (_20819 >> 16u)) * 2246822519u;
                float _17258 = float((_1992 ^ (_1992 >> 13u)) & 65535u);
                float _17259 = 65535.0;
                Interval _20826 = iratio(_17258, _17259, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20827 = float(_1792);
                float _20828 = float(_20731);
                bool _20833;
                if (!intervalFailed)
                {
                    _20833 = intervalFailed;
                }
                else
                {
                    _20833 = false;
                }
                bool _20838;
                if (_20833)
                {
                    _20838 = jetFailureSite == 0u;
                }
                else
                {
                    _20838 = false;
                }
                if (_20838)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20827, _20827, _20828, _20828);
                }
                Interval _17244 = _20803;
                Interval _17245 = _20826;
                Interval _20842 = imul(_17244, _17245, intervalFailed, optical_product_upper);
                Interval _17246 = Interval{ 0.0, 0.0 };
                Interval _17247 = _20826;
                Interval _20843 = jet_mul_derivative(_17246, _17247, intervalFailed, optical_product_upper);
                Interval _17248 = _20843;
                Interval _17249 = _20803;
                Interval _17250 = Interval{ 0.0, 0.0 };
                Interval _20844 = jet_mul_derivative(_17249, _17250, intervalFailed, optical_product_upper);
                Interval _17251 = _20844;
                Interval _20845 = jet_add_derivative(_17248, _17251, intervalFailed);
                Interval _17252 = Interval{ 0.0, 0.0 };
                Interval _17253 = _20826;
                Interval _20846 = jet_mul_derivative(_17252, _17253, intervalFailed, optical_product_upper);
                Interval _17254 = _20846;
                Interval _17255 = _20803;
                Interval _17256 = Interval{ 0.0, 0.0 };
                Interval _20847 = jet_mul_derivative(_17255, _17256, intervalFailed, optical_product_upper);
                Interval _17257 = _20847;
                Interval _20848 = jet_add_derivative(_17254, _17257, intervalFailed);
                Interval _17238 = _20789;
                Interval _17239 = _20842;
                Interval _20849 = iadd(_17238, _17239, intervalFailed);
                Interval _17240 = Interval{ 0.0, 0.0 };
                Interval _17241 = _20845;
                Interval _20850 = jet_add_derivative(_17240, _17241, intervalFailed);
                Interval _17242 = Interval{ 0.0, 0.0 };
                Interval _17243 = _20848;
                Interval _20851 = jet_add_derivative(_17242, _17243, intervalFailed);
                Interval _17232 = _20783;
                Interval _17233 = Interval{ as_type<float>(as_type<uint>(_20849.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20849.lo) ^ 2147483648u) };
                Interval _20877 = iadd(_17232, _17233, intervalFailed);
                Interval _17234 = _20785;
                Interval _17235 = Interval{ as_type<float>(as_type<uint>(_20850.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20850.lo) ^ 2147483648u) };
                Interval _20879 = jet_add_derivative(_17234, _17235, intervalFailed);
                Interval _17236 = _20787;
                Interval _17237 = Interval{ as_type<float>(as_type<uint>(_20851.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20851.lo) ^ 2147483648u) };
                Interval _20881 = jet_add_derivative(_17236, _17237, intervalFailed);
                float _20882 = float(_20731);
                Interval _17226 = _20679;
                Interval _17227 = Interval{ as_type<float>(as_type<uint>(_20882) ^ 2147483648u), as_type<float>(as_type<uint>(_20882) ^ 2147483648u) };
                Interval _20894 = iadd(_17226, _17227, intervalFailed);
                Interval _17228 = _20706;
                Interval _17229 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20896 = jet_add_derivative(_17228, _17229, intervalFailed);
                Interval _17230 = _20718;
                Interval _17231 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20898 = jet_add_derivative(_17230, _17231, intervalFailed);
                float _17224 = 28.0;
                float _17225 = 100.0;
                Interval _20900 = iratio(_17224, _17225, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20905;
                if (!intervalFailed)
                {
                    _20905 = intervalFailed;
                }
                else
                {
                    _20905 = false;
                }
                bool _20910;
                if (_20905)
                {
                    _20910 = jetFailureSite == 0u;
                }
                else
                {
                    _20910 = false;
                }
                if (_20910)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17222 = 44.0;
                float _17223 = 100.0;
                Interval _20914 = iratio(_17222, _17223, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20919;
                if (!intervalFailed)
                {
                    _20919 = intervalFailed;
                }
                else
                {
                    _20919 = false;
                }
                bool _20924;
                if (_20919)
                {
                    _20924 = jetFailureSite == 0u;
                }
                else
                {
                    _20924 = false;
                }
                if (_20924)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1793 = _20731 + 29;
                uint _20930 = (uint(_20729) * 1597334677u) ^ (uint(_1793) * 3812015801u);
                uint _1995 = (_20930 ^ (_20930 >> 16u)) * 2246822519u;
                float _17220 = float((_1995 ^ (_1995 >> 13u)) & 65535u);
                float _17221 = 65535.0;
                Interval _20937 = iratio(_17220, _17221, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20938 = float(_20729);
                float _20939 = float(_1793);
                bool _20944;
                if (!intervalFailed)
                {
                    _20944 = intervalFailed;
                }
                else
                {
                    _20944 = false;
                }
                bool _20949;
                if (_20944)
                {
                    _20949 = jetFailureSite == 0u;
                }
                else
                {
                    _20949 = false;
                }
                if (_20949)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20938, _20938, _20939, _20939);
                }
                Interval _17206 = _20914;
                Interval _17207 = _20937;
                Interval _20953 = imul(_17206, _17207, intervalFailed, optical_product_upper);
                Interval _17208 = Interval{ 0.0, 0.0 };
                Interval _17209 = _20937;
                Interval _20954 = jet_mul_derivative(_17208, _17209, intervalFailed, optical_product_upper);
                Interval _17210 = _20954;
                Interval _17211 = _20914;
                Interval _17212 = Interval{ 0.0, 0.0 };
                Interval _20955 = jet_mul_derivative(_17211, _17212, intervalFailed, optical_product_upper);
                Interval _17213 = _20955;
                Interval _20956 = jet_add_derivative(_17210, _17213, intervalFailed);
                Interval _17214 = Interval{ 0.0, 0.0 };
                Interval _17215 = _20937;
                Interval _20957 = jet_mul_derivative(_17214, _17215, intervalFailed, optical_product_upper);
                Interval _17216 = _20957;
                Interval _17217 = _20914;
                Interval _17218 = Interval{ 0.0, 0.0 };
                Interval _20958 = jet_mul_derivative(_17217, _17218, intervalFailed, optical_product_upper);
                Interval _17219 = _20958;
                Interval _20959 = jet_add_derivative(_17216, _17219, intervalFailed);
                Interval _17200 = _20900;
                Interval _17201 = _20953;
                Interval _20960 = iadd(_17200, _17201, intervalFailed);
                Interval _17202 = Interval{ 0.0, 0.0 };
                Interval _17203 = _20956;
                Interval _20961 = jet_add_derivative(_17202, _17203, intervalFailed);
                Interval _17204 = Interval{ 0.0, 0.0 };
                Interval _17205 = _20959;
                Interval _20962 = jet_add_derivative(_17204, _17205, intervalFailed);
                Interval _17194 = _20894;
                Interval _17195 = Interval{ as_type<float>(as_type<uint>(_20960.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20960.lo) ^ 2147483648u) };
                Interval _20988 = iadd(_17194, _17195, intervalFailed);
                Interval _17196 = _20896;
                Interval _17197 = Interval{ as_type<float>(as_type<uint>(_20961.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20961.lo) ^ 2147483648u) };
                Interval _20990 = jet_add_derivative(_17196, _17197, intervalFailed);
                Interval _17198 = _20898;
                Interval _17199 = Interval{ as_type<float>(as_type<uint>(_20962.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20962.lo) ^ 2147483648u) };
                Interval _20992 = jet_add_derivative(_17198, _17199, intervalFailed);
                float _17192 = 14.0;
                float _17193 = 100.0;
                Interval _20998 = iratio(_17192, _17193, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21003;
                if (!intervalFailed)
                {
                    _21003 = intervalFailed;
                }
                else
                {
                    _21003 = false;
                }
                bool _21008;
                if (_21003)
                {
                    _21008 = jetFailureSite == 0u;
                }
                else
                {
                    _21008 = false;
                }
                if (_21008)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _21049;
                Interval _21051;
                Interval _21053;
                Interval _17178 = t.v;
                Interval _17179 = _20998;
                Interval _21011 = imul(_17178, _17179, intervalFailed, optical_product_upper);
                Interval _17180 = t.dx;
                Interval _17181 = _20998;
                Interval _21012 = jet_mul_derivative(_17180, _17181, intervalFailed, optical_product_upper);
                Interval _17182 = _21012;
                Interval _17183 = t.v;
                Interval _17184 = Interval{ 0.0, 0.0 };
                Interval _21013 = jet_mul_derivative(_17183, _17184, intervalFailed, optical_product_upper);
                Interval _17185 = _21013;
                Interval _21014 = jet_add_derivative(_17182, _17185, intervalFailed);
                Interval _17186 = t.dy;
                Interval _17187 = _20998;
                Interval _21015 = jet_mul_derivative(_17186, _17187, intervalFailed, optical_product_upper);
                Interval _17188 = _21015;
                Interval _17189 = t.v;
                Interval _17190 = Interval{ 0.0, 0.0 };
                Interval _21016 = jet_mul_derivative(_17189, _17190, intervalFailed, optical_product_upper);
                Interval _17191 = _21016;
                Interval _21017 = jet_add_derivative(_17188, _17191, intervalFailed);
                Interval _17164 = _20742;
                Interval _17165 = Interval{ 7.0, 7.0 };
                Interval _21018 = imul(_17164, _17165, intervalFailed, optical_product_upper);
                Interval _17166 = Interval{ 0.0, 0.0 };
                Interval _17167 = Interval{ 7.0, 7.0 };
                Interval _21019 = jet_mul_derivative(_17166, _17167, intervalFailed, optical_product_upper);
                Interval _17168 = _21019;
                Interval _17169 = _20742;
                Interval _17170 = Interval{ 0.0, 0.0 };
                Interval _21020 = jet_mul_derivative(_17169, _17170, intervalFailed, optical_product_upper);
                Interval _17171 = _21020;
                Interval _21021 = jet_add_derivative(_17168, _17171, intervalFailed);
                Interval _17172 = Interval{ 0.0, 0.0 };
                Interval _17173 = Interval{ 7.0, 7.0 };
                Interval _21022 = jet_mul_derivative(_17172, _17173, intervalFailed, optical_product_upper);
                Interval _17174 = _21022;
                Interval _17175 = _20742;
                Interval _17176 = Interval{ 0.0, 0.0 };
                Interval _21023 = jet_mul_derivative(_17175, _17176, intervalFailed, optical_product_upper);
                Interval _17177 = _21023;
                Interval _21024 = jet_add_derivative(_17174, _17177, intervalFailed);
                Interval _17158 = _21011;
                Interval _17159 = _21018;
                Interval _21025 = iadd(_17158, _17159, intervalFailed);
                Interval _17160 = _21014;
                Interval _17161 = _21021;
                Interval _21026 = jet_add_derivative(_17160, _17161, intervalFailed);
                Interval _17162 = _21017;
                Interval _17163 = _21024;
                Interval _21027 = jet_add_derivative(_17162, _17163, intervalFailed);
                if (floor(_21025.lo) == floor(_21025.hi))
                {
                    float _21037 = floor(_21025.lo);
                    Interval _17152 = _21025;
                    Interval _17153 = Interval{ as_type<float>(as_type<uint>(_21037) ^ 2147483648u), as_type<float>(as_type<uint>(_21037) ^ 2147483648u) };
                    _21049 = iadd(_17152, _17153, intervalFailed);
                    Interval _17154 = _21026;
                    Interval _17155 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _21051 = jet_add_derivative(_17154, _17155, intervalFailed);
                    Interval _17156 = _21027;
                    Interval _17157 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _21053 = jet_add_derivative(_17156, _17157, intervalFailed);
                }
                else
                {
                    jetBranchKnown = false;
                    return OpticalJet3{ OpticalJet{ _20482, _20483, _20484 }, OpticalJet{ _20545, _20546, _20547 }, OpticalJet{ _20608, _20609, _20610 } };
                }
                float _17150 = 3.1415927410125732421875;
                float _21054 = interval_down(_17150, intervalFailed);
                float _17151 = 3.1415927410125732421875;
                float _21055 = interval_up(_17151, intervalFailed);
                Interval _17136 = _21049;
                Interval _17137 = Interval{ _21054, _21055 };
                Interval _21057 = imul(_17136, _17137, intervalFailed, optical_product_upper);
                Interval _17138 = _21051;
                Interval _17139 = Interval{ _21054, _21055 };
                Interval _21059 = jet_mul_derivative(_17138, _17139, intervalFailed, optical_product_upper);
                Interval _17140 = _21059;
                Interval _17141 = _21049;
                Interval _17142 = Interval{ 0.0, 0.0 };
                Interval _21060 = jet_mul_derivative(_17141, _17142, intervalFailed, optical_product_upper);
                Interval _17143 = _21060;
                Interval _21061 = jet_add_derivative(_17140, _17143, intervalFailed);
                Interval _17144 = _21053;
                Interval _17145 = Interval{ _21054, _21055 };
                Interval _21063 = jet_mul_derivative(_17144, _17145, intervalFailed, optical_product_upper);
                Interval _17146 = _21063;
                Interval _17147 = _21049;
                Interval _17148 = Interval{ 0.0, 0.0 };
                Interval _21064 = jet_mul_derivative(_17147, _17148, intervalFailed, optical_product_upper);
                Interval _17149 = _21064;
                Interval _21065 = jet_add_derivative(_17146, _17149, intervalFailed);
                Interval _17128 = _21057;
                float _17126 = 3.1415927410125732421875;
                float _21066 = interval_down(_17126, intervalFailed);
                float _17127 = 3.1415927410125732421875;
                float _21067 = interval_up(_17127, intervalFailed);
                Interval _17129 = Interval{ _21066, _21067 };
                Interval _17130 = Interval{ 0.5, 0.5 };
                Interval _21069 = imul(_17129, _17130, intervalFailed, optical_product_upper);
                Interval _17131 = _21069;
                Interval _21070 = iadd(_17128, _17131, intervalFailed);
                float _17124 = _21070.lo;
                float _17125 = _21070.hi;
                float _21073 = sine_bounds(_17124, _17125, intervalFailed, optical_product_upper, interval_sine_upper);
                float _17122 = _21057.lo;
                float _17123 = _21057.hi;
                float _21077 = sine_bounds(_17122, _17123, intervalFailed, optical_product_upper, interval_sine_upper);
                Interval _17132 = Interval{ _21073, interval_sine_upper };
                Interval _17133 = _21061;
                Interval _21080 = jet_mul_derivative(_17132, _17133, intervalFailed, optical_product_upper);
                Interval _17134 = Interval{ _21073, interval_sine_upper };
                Interval _17135 = _21065;
                Interval _21082 = jet_mul_derivative(_17134, _17135, intervalFailed, optical_product_upper);
                bool _21087;
                if (_21077 <= 0.0)
                {
                    _21087 = interval_sine_upper >= 0.0;
                }
                else
                {
                    _21087 = false;
                }
                float _21094;
                if (_21087)
                {
                    _21094 = 0.0;
                }
                else
                {
                    _21094 = precise::min(abs(_21077), abs(interval_sine_upper));
                }
                float _21097 = precise::max(abs(_21077), abs(interval_sine_upper));
                float _17112 = spvFMul(_21094, _21094);
                float _21098 = interval_down(_17112, intervalFailed);
                float _21099 = precise::max(0.0, _21098);
                float _17113 = spvFMul(_21097, _21097);
                float _21100 = interval_up(_17113, intervalFailed);
                Interval _17114 = Interval{ 2.0, 2.0 };
                Interval _17115 = Interval{ _21077, interval_sine_upper };
                Interval _21102 = imul(_17114, _17115, intervalFailed, optical_product_upper);
                Interval _17116 = _21102;
                Interval _17117 = _21080;
                Interval _21103 = jet_mul_derivative(_17116, _17117, intervalFailed, optical_product_upper);
                Interval _17118 = Interval{ 2.0, 2.0 };
                Interval _17119 = Interval{ _21077, interval_sine_upper };
                Interval _21105 = imul(_17118, _17119, intervalFailed, optical_product_upper);
                Interval _17120 = _21105;
                Interval _17121 = _21082;
                Interval _21106 = jet_mul_derivative(_17120, _17121, intervalFailed, optical_product_upper);
                float _17110 = 55.0;
                float _17111 = 1000.0;
                Interval _21108 = iratio(_17110, _17111, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21113;
                if (!intervalFailed)
                {
                    _21113 = intervalFailed;
                }
                else
                {
                    _21113 = false;
                }
                bool _21118;
                if (_21113)
                {
                    _21118 = jetFailureSite == 0u;
                }
                else
                {
                    _21118 = false;
                }
                if (_21118)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
                }
                float _17108 = 14.0;
                float _17109 = 100.0;
                Interval _21122 = iratio(_17108, _17109, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21127;
                if (!intervalFailed)
                {
                    _21127 = intervalFailed;
                }
                else
                {
                    _21127 = false;
                }
                bool _21132;
                if (_21127)
                {
                    _21132 = jetFailureSite == 0u;
                }
                else
                {
                    _21132 = false;
                }
                if (_21132)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _17094 = _21122;
                Interval _17095 = _21049;
                Interval _21135 = imul(_17094, _17095, intervalFailed, optical_product_upper);
                Interval _17096 = Interval{ 0.0, 0.0 };
                Interval _17097 = _21049;
                Interval _21136 = jet_mul_derivative(_17096, _17097, intervalFailed, optical_product_upper);
                Interval _17098 = _21136;
                Interval _17099 = _21122;
                Interval _17100 = _21051;
                Interval _21137 = jet_mul_derivative(_17099, _17100, intervalFailed, optical_product_upper);
                Interval _17101 = _21137;
                Interval _21138 = jet_add_derivative(_17098, _17101, intervalFailed);
                Interval _17102 = Interval{ 0.0, 0.0 };
                Interval _17103 = _21049;
                Interval _21139 = jet_mul_derivative(_17102, _17103, intervalFailed, optical_product_upper);
                Interval _17104 = _21139;
                Interval _17105 = _21122;
                Interval _17106 = _21053;
                Interval _21140 = jet_mul_derivative(_17105, _17106, intervalFailed, optical_product_upper);
                Interval _17107 = _21140;
                Interval _21141 = jet_add_derivative(_17104, _17107, intervalFailed);
                Interval _17088 = _21108;
                Interval _17089 = _21135;
                Interval _21142 = iadd(_17088, _17089, intervalFailed);
                Interval _17090 = Interval{ 0.0, 0.0 };
                Interval _17091 = _21138;
                Interval _21143 = jet_add_derivative(_17090, _17091, intervalFailed);
                Interval _17092 = Interval{ 0.0, 0.0 };
                Interval _17093 = _21141;
                Interval _21144 = jet_add_derivative(_17092, _17093, intervalFailed);
                bool _21151;
                if (_21142.lo <= 0.0)
                {
                    _21151 = _21142.hi >= 0.0;
                }
                else
                {
                    _21151 = false;
                }
                float _21158;
                if (_21151)
                {
                    _21158 = 0.0;
                }
                else
                {
                    _21158 = precise::min(abs(_21142.lo), abs(_21142.hi));
                }
                float _21161 = precise::max(abs(_21142.lo), abs(_21142.hi));
                float _17078 = spvFMul(_21158, _21158);
                float _21162 = interval_down(_17078, intervalFailed);
                float _21163 = precise::max(0.0, _21162);
                float _17079 = spvFMul(_21161, _21161);
                float _21164 = interval_up(_17079, intervalFailed);
                Interval _17080 = Interval{ 2.0, 2.0 };
                Interval _17081 = _21142;
                Interval _21165 = imul(_17080, _17081, intervalFailed, optical_product_upper);
                Interval _17082 = _21165;
                Interval _17083 = _21143;
                Interval _21166 = jet_mul_derivative(_17082, _17083, intervalFailed, optical_product_upper);
                Interval _17084 = Interval{ 2.0, 2.0 };
                Interval _17085 = _21142;
                Interval _21167 = imul(_17084, _17085, intervalFailed, optical_product_upper);
                Interval _17086 = _21167;
                Interval _17087 = _21144;
                Interval _21168 = jet_mul_derivative(_17086, _17087, intervalFailed, optical_product_upper);
                bool _21175;
                if (_20877.lo <= 0.0)
                {
                    _21175 = _20877.hi >= 0.0;
                }
                else
                {
                    _21175 = false;
                }
                float _21182;
                if (_21175)
                {
                    _21182 = 0.0;
                }
                else
                {
                    _21182 = precise::min(abs(_20877.lo), abs(_20877.hi));
                }
                float _21185 = precise::max(abs(_20877.lo), abs(_20877.hi));
                float _17068 = spvFMul(_21182, _21182);
                float _21186 = interval_down(_17068, intervalFailed);
                float _17069 = spvFMul(_21185, _21185);
                float _21188 = interval_up(_17069, intervalFailed);
                Interval _17070 = Interval{ 2.0, 2.0 };
                Interval _17071 = _20877;
                Interval _21189 = imul(_17070, _17071, intervalFailed, optical_product_upper);
                Interval _17072 = _21189;
                Interval _17073 = _20879;
                Interval _21190 = jet_mul_derivative(_17072, _17073, intervalFailed, optical_product_upper);
                Interval _17074 = Interval{ 2.0, 2.0 };
                Interval _17075 = _20877;
                Interval _21191 = imul(_17074, _17075, intervalFailed, optical_product_upper);
                Interval _17076 = _21191;
                Interval _17077 = _20881;
                Interval _21192 = jet_mul_derivative(_17076, _17077, intervalFailed, optical_product_upper);
                bool _21199;
                if (_20988.lo <= 0.0)
                {
                    _21199 = _20988.hi >= 0.0;
                }
                else
                {
                    _21199 = false;
                }
                float _21206;
                if (_21199)
                {
                    _21206 = 0.0;
                }
                else
                {
                    _21206 = precise::min(abs(_20988.lo), abs(_20988.hi));
                }
                float _21209 = precise::max(abs(_20988.lo), abs(_20988.hi));
                float _17058 = spvFMul(_21206, _21206);
                float _21210 = interval_down(_17058, intervalFailed);
                float _17059 = spvFMul(_21209, _21209);
                float _21212 = interval_up(_17059, intervalFailed);
                Interval _17060 = Interval{ 2.0, 2.0 };
                Interval _17061 = _20988;
                Interval _21213 = imul(_17060, _17061, intervalFailed, optical_product_upper);
                Interval _17062 = _21213;
                Interval _17063 = _20990;
                Interval _21214 = jet_mul_derivative(_17062, _17063, intervalFailed, optical_product_upper);
                Interval _17064 = Interval{ 2.0, 2.0 };
                Interval _17065 = _20988;
                Interval _21215 = imul(_17064, _17065, intervalFailed, optical_product_upper);
                Interval _17066 = _21215;
                Interval _17067 = _20992;
                Interval _21216 = jet_mul_derivative(_17066, _17067, intervalFailed, optical_product_upper);
                Interval _17052 = Interval{ precise::max(0.0, _21186), _21188 };
                Interval _17053 = Interval{ precise::max(0.0, _21210), _21212 };
                Interval _21219 = iadd(_17052, _17053, intervalFailed);
                Interval _17054 = _21190;
                Interval _17055 = _21214;
                Interval _21220 = jet_add_derivative(_17054, _17055, intervalFailed);
                Interval _17056 = _21192;
                Interval _17057 = _21216;
                Interval _21221 = jet_add_derivative(_17056, _17057, intervalFailed);
                float _21224 = precise::max(0.0, _21219.lo);
                Interval _17038 = Interval{ _21224, _21219.hi };
                Interval _17039 = Interval{ _21163, _21164 };
                Interval _21228 = idiv(_17038, _17039, intervalFailed, interval_divide_upper);
                bool _21233;
                if (!intervalFailed)
                {
                    _21233 = intervalFailed;
                }
                else
                {
                    _21233 = false;
                }
                bool _21238;
                if (_21233)
                {
                    _21238 = jetFailureSite == 0u;
                }
                else
                {
                    _21238 = false;
                }
                if (_21238)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21224, _21219.hi, _21163, _21164);
                }
                Interval _17040 = _21220;
                Interval _17041 = _21228;
                Interval _17042 = _21166;
                Interval _21242 = jet_mul_derivative(_17041, _17042, intervalFailed, optical_product_upper);
                Interval _17043 = Interval{ as_type<float>(as_type<uint>(_21242.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21242.lo) ^ 2147483648u) };
                Interval _21252 = jet_add_derivative(_17040, _17043, intervalFailed);
                Interval _17044 = _21252;
                Interval _17045 = Interval{ _21163, _21164 };
                Interval _21254 = jet_div_derivative(_17044, _17045, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _17046 = _21221;
                Interval _17047 = _21228;
                Interval _17048 = _21168;
                Interval _21255 = jet_mul_derivative(_17047, _17048, intervalFailed, optical_product_upper);
                Interval _17049 = Interval{ as_type<float>(as_type<uint>(_21255.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21255.lo) ^ 2147483648u) };
                Interval _21265 = jet_add_derivative(_17046, _17049, intervalFailed);
                Interval _17050 = _21265;
                Interval _17051 = Interval{ _21163, _21164 };
                Interval _21267 = jet_div_derivative(_17050, _17051, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _17032 = Interval{ 1.0, 1.0 };
                Interval _17033 = Interval{ as_type<float>(as_type<uint>(_21228.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21228.lo) ^ 2147483648u) };
                Interval _21293 = iadd(_17032, _17033, intervalFailed);
                Interval _17034 = Interval{ 0.0, 0.0 };
                Interval _17035 = Interval{ as_type<float>(as_type<uint>(_21254.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21254.lo) ^ 2147483648u) };
                Interval _21295 = jet_add_derivative(_17034, _17035, intervalFailed);
                Interval _17036 = Interval{ 0.0, 0.0 };
                Interval _17037 = Interval{ as_type<float>(as_type<uint>(_21267.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21267.lo) ^ 2147483648u) };
                Interval _21297 = jet_add_derivative(_17036, _17037, intervalFailed);
                OpticalJet _17028 = OpticalJet{ _21293, _21295, _21297 };
                OpticalJet _17029 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _17030 = jmax(_17028, _17029);
                OpticalJet _17031 = OpticalJet{ Interval{ 1.0, 1.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _21300 = jmin(_17030, _17031);
                Interval _17014 = Interval{ 10.0, 10.0 };
                Interval _17015 = Interval{ _21099, _21100 };
                Interval _21302 = imul(_17014, _17015, intervalFailed, optical_product_upper);
                Interval _17016 = Interval{ 0.0, 0.0 };
                Interval _17017 = Interval{ _21099, _21100 };
                Interval _21304 = jet_mul_derivative(_17016, _17017, intervalFailed, optical_product_upper);
                Interval _17018 = _21304;
                Interval _17019 = Interval{ 10.0, 10.0 };
                Interval _17020 = _21103;
                Interval _21305 = jet_mul_derivative(_17019, _17020, intervalFailed, optical_product_upper);
                Interval _17021 = _21305;
                Interval _21306 = jet_add_derivative(_17018, _17021, intervalFailed);
                Interval _17022 = Interval{ 0.0, 0.0 };
                Interval _17023 = Interval{ _21099, _21100 };
                Interval _21308 = jet_mul_derivative(_17022, _17023, intervalFailed, optical_product_upper);
                Interval _17024 = _21308;
                Interval _17025 = Interval{ 10.0, 10.0 };
                Interval _17026 = _21106;
                Interval _21309 = jet_mul_derivative(_17025, _17026, intervalFailed, optical_product_upper);
                Interval _17027 = _21309;
                Interval _21310 = jet_add_derivative(_17024, _17027, intervalFailed);
                float _17012 = 18.0;
                float _17013 = 100.0;
                Interval _21313 = iratio(_17012, _17013, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21318;
                if (!intervalFailed)
                {
                    _21318 = intervalFailed;
                }
                else
                {
                    _21318 = false;
                }
                bool _21323;
                if (_21318)
                {
                    _21323 = jetFailureSite == 0u;
                }
                else
                {
                    _21323 = false;
                }
                if (_21323)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(18.0, 18.0, 100.0, 100.0);
                }
                Interval _16998 = footprint.v;
                Interval _16999 = _21313;
                Interval _21329 = imul(_16998, _16999, intervalFailed, optical_product_upper);
                Interval _17000 = footprint.dx;
                Interval _17001 = _21313;
                Interval _21330 = jet_mul_derivative(_17000, _17001, intervalFailed, optical_product_upper);
                Interval _17002 = _21330;
                Interval _17003 = footprint.v;
                Interval _17004 = Interval{ 0.0, 0.0 };
                Interval _21331 = jet_mul_derivative(_17003, _17004, intervalFailed, optical_product_upper);
                Interval _17005 = _21331;
                Interval _21332 = jet_add_derivative(_17002, _17005, intervalFailed);
                Interval _17006 = footprint.dy;
                Interval _17007 = _21313;
                Interval _21333 = jet_mul_derivative(_17006, _17007, intervalFailed, optical_product_upper);
                Interval _17008 = _21333;
                Interval _17009 = footprint.v;
                Interval _17010 = Interval{ 0.0, 0.0 };
                Interval _21334 = jet_mul_derivative(_17009, _17010, intervalFailed, optical_product_upper);
                Interval _17011 = _21334;
                Interval _21335 = jet_add_derivative(_17008, _17011, intervalFailed);
                bool _21342;
                if (_21329.lo <= 0.0)
                {
                    _21342 = _21329.hi >= 0.0;
                }
                else
                {
                    _21342 = false;
                }
                float _21349;
                if (_21342)
                {
                    _21349 = 0.0;
                }
                else
                {
                    _21349 = precise::min(abs(_21329.lo), abs(_21329.hi));
                }
                float _21352 = precise::max(abs(_21329.lo), abs(_21329.hi));
                float _16988 = spvFMul(_21349, _21349);
                float _21353 = interval_down(_16988, intervalFailed);
                float _21354 = precise::max(0.0, _21353);
                float _16989 = spvFMul(_21352, _21352);
                float _21355 = interval_up(_16989, intervalFailed);
                Interval _16990 = Interval{ 2.0, 2.0 };
                Interval _16991 = _21329;
                Interval _21356 = imul(_16990, _16991, intervalFailed, optical_product_upper);
                Interval _16992 = _21356;
                Interval _16993 = _21332;
                Interval _21357 = jet_mul_derivative(_16992, _16993, intervalFailed, optical_product_upper);
                Interval _16994 = Interval{ 2.0, 2.0 };
                Interval _16995 = _21329;
                Interval _21358 = imul(_16994, _16995, intervalFailed, optical_product_upper);
                Interval _16996 = _21358;
                Interval _16997 = _21335;
                Interval _21359 = jet_mul_derivative(_16996, _16997, intervalFailed, optical_product_upper);
                bool _21364;
                if (_21354 <= 0.0)
                {
                    _21364 = _21355 >= 0.0;
                }
                else
                {
                    _21364 = false;
                }
                float _21371;
                if (_21364)
                {
                    _21371 = 0.0;
                }
                else
                {
                    _21371 = precise::min(abs(_21354), abs(_21355));
                }
                float _21374 = precise::max(abs(_21354), abs(_21355));
                float _16978 = spvFMul(_21371, _21371);
                float _21375 = interval_down(_16978, intervalFailed);
                float _16979 = spvFMul(_21374, _21374);
                float _21377 = interval_up(_16979, intervalFailed);
                Interval _16980 = Interval{ 2.0, 2.0 };
                Interval _16981 = Interval{ _21354, _21355 };
                Interval _21379 = imul(_16980, _16981, intervalFailed, optical_product_upper);
                Interval _16982 = _21379;
                Interval _16983 = _21357;
                Interval _21380 = jet_mul_derivative(_16982, _16983, intervalFailed, optical_product_upper);
                Interval _16984 = Interval{ 2.0, 2.0 };
                Interval _16985 = Interval{ _21354, _21355 };
                Interval _21382 = imul(_16984, _16985, intervalFailed, optical_product_upper);
                Interval _16986 = _21382;
                Interval _16987 = _21359;
                Interval _21383 = jet_mul_derivative(_16986, _16987, intervalFailed, optical_product_upper);
                Interval _16972 = Interval{ 1.0, 1.0 };
                Interval _16973 = Interval{ precise::max(0.0, _21375), _21377 };
                Interval _21385 = iadd(_16972, _16973, intervalFailed);
                Interval _16974 = Interval{ 0.0, 0.0 };
                Interval _16975 = _21380;
                Interval _21386 = jet_add_derivative(_16974, _16975, intervalFailed);
                Interval _16976 = Interval{ 0.0, 0.0 };
                Interval _16977 = _21383;
                Interval _21387 = jet_add_derivative(_16976, _16977, intervalFailed);
                Interval _16958 = Interval{ 1.0, 1.0 };
                Interval _16959 = _21385;
                Interval _21389 = idiv(_16958, _16959, intervalFailed, interval_divide_upper);
                bool _21396;
                if (!intervalFailed)
                {
                    _21396 = intervalFailed;
                }
                else
                {
                    _21396 = false;
                }
                bool _21401;
                if (_21396)
                {
                    _21401 = jetFailureSite == 0u;
                }
                else
                {
                    _21401 = false;
                }
                if (_21401)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _21385.lo, _21385.hi);
                }
                Interval _16960 = Interval{ 0.0, 0.0 };
                Interval _16961 = _21389;
                Interval _16962 = _21386;
                Interval _21405 = jet_mul_derivative(_16961, _16962, intervalFailed, optical_product_upper);
                Interval _16963 = Interval{ as_type<float>(as_type<uint>(_21405.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21405.lo) ^ 2147483648u) };
                Interval _21415 = jet_add_derivative(_16960, _16963, intervalFailed);
                Interval _16964 = _21415;
                Interval _16965 = _21385;
                Interval _21416 = jet_div_derivative(_16964, _16965, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16966 = Interval{ 0.0, 0.0 };
                Interval _16967 = _21389;
                Interval _16968 = _21387;
                Interval _21417 = jet_mul_derivative(_16967, _16968, intervalFailed, optical_product_upper);
                Interval _16969 = Interval{ as_type<float>(as_type<uint>(_21417.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21417.lo) ^ 2147483648u) };
                Interval _21427 = jet_add_derivative(_16966, _16969, intervalFailed);
                Interval _16970 = _21427;
                Interval _16971 = _21385;
                Interval _21428 = jet_div_derivative(_16970, _16971, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16944 = _21302;
                Interval _16945 = _21389;
                Interval _21429 = imul(_16944, _16945, intervalFailed, optical_product_upper);
                Interval _16946 = _21306;
                Interval _16947 = _21389;
                Interval _21430 = jet_mul_derivative(_16946, _16947, intervalFailed, optical_product_upper);
                Interval _16948 = _21430;
                Interval _16949 = _21302;
                Interval _16950 = _21416;
                Interval _21431 = jet_mul_derivative(_16949, _16950, intervalFailed, optical_product_upper);
                Interval _16951 = _21431;
                Interval _21432 = jet_add_derivative(_16948, _16951, intervalFailed);
                Interval _16952 = _21310;
                Interval _16953 = _21389;
                Interval _21433 = jet_mul_derivative(_16952, _16953, intervalFailed, optical_product_upper);
                Interval _16954 = _21433;
                Interval _16955 = _21302;
                Interval _16956 = _21428;
                Interval _21434 = jet_mul_derivative(_16955, _16956, intervalFailed, optical_product_upper);
                Interval _16957 = _21434;
                Interval _21435 = jet_add_derivative(_16954, _16957, intervalFailed);
                Interval _21436 = _21300.v;
                float _21439 = _21436.lo;
                float _21440 = _21436.hi;
                bool _21445;
                if (_21439 <= 0.0)
                {
                    _21445 = _21440 >= 0.0;
                }
                else
                {
                    _21445 = false;
                }
                float _21452;
                if (_21445)
                {
                    _21452 = 0.0;
                }
                else
                {
                    _21452 = precise::min(abs(_21439), abs(_21440));
                }
                float _21455 = precise::max(abs(_21439), abs(_21440));
                float _16934 = spvFMul(_21452, _21452);
                float _21456 = interval_down(_16934, intervalFailed);
                float _21457 = precise::max(0.0, _21456);
                float _16935 = spvFMul(_21455, _21455);
                float _21458 = interval_up(_16935, intervalFailed);
                Interval _16936 = Interval{ 2.0, 2.0 };
                Interval _16937 = _21436;
                Interval _21459 = imul(_16936, _16937, intervalFailed, optical_product_upper);
                Interval _16938 = _21459;
                Interval _16939 = _21300.dx;
                Interval _21460 = jet_mul_derivative(_16938, _16939, intervalFailed, optical_product_upper);
                Interval _16940 = Interval{ 2.0, 2.0 };
                Interval _16941 = _21436;
                Interval _21461 = imul(_16940, _16941, intervalFailed, optical_product_upper);
                Interval _16942 = _21461;
                Interval _16943 = _21300.dy;
                Interval _21462 = jet_mul_derivative(_16942, _16943, intervalFailed, optical_product_upper);
                Interval _16920 = _21429;
                Interval _16921 = Interval{ _21457, _21458 };
                Interval _21464 = imul(_16920, _16921, intervalFailed, optical_product_upper);
                Interval _16922 = _21432;
                Interval _16923 = Interval{ _21457, _21458 };
                Interval _21466 = jet_mul_derivative(_16922, _16923, intervalFailed, optical_product_upper);
                Interval _16924 = _21466;
                Interval _16925 = _21429;
                Interval _16926 = _21460;
                Interval _21467 = jet_mul_derivative(_16925, _16926, intervalFailed, optical_product_upper);
                Interval _16927 = _21467;
                Interval _21468 = jet_add_derivative(_16924, _16927, intervalFailed);
                Interval _16928 = _21435;
                Interval _16929 = Interval{ _21457, _21458 };
                Interval _21470 = jet_mul_derivative(_16928, _16929, intervalFailed, optical_product_upper);
                Interval _16930 = _21470;
                Interval _16931 = _21429;
                Interval _16932 = _21462;
                Interval _21471 = jet_mul_derivative(_16931, _16932, intervalFailed, optical_product_upper);
                Interval _16933 = _21471;
                Interval _21472 = jet_add_derivative(_16930, _16933, intervalFailed);
                Interval _21473 = _21300.v;
                Interval _16906 = _21464;
                Interval _16907 = _21473;
                Interval _21476 = imul(_16906, _16907, intervalFailed, optical_product_upper);
                Interval _16908 = _21468;
                Interval _16909 = _21473;
                Interval _21477 = jet_mul_derivative(_16908, _16909, intervalFailed, optical_product_upper);
                Interval _16910 = _21477;
                Interval _16911 = _21464;
                Interval _16912 = _21300.dx;
                Interval _21478 = jet_mul_derivative(_16911, _16912, intervalFailed, optical_product_upper);
                Interval _16913 = _21478;
                Interval _21479 = jet_add_derivative(_16910, _16913, intervalFailed);
                Interval _16914 = _21472;
                Interval _16915 = _21473;
                Interval _21480 = jet_mul_derivative(_16914, _16915, intervalFailed, optical_product_upper);
                Interval _16916 = _21480;
                Interval _16917 = _21464;
                Interval _16918 = _21300.dy;
                Interval _21481 = jet_mul_derivative(_16917, _16918, intervalFailed, optical_product_upper);
                Interval _16919 = _21481;
                Interval _21482 = jet_add_derivative(_16916, _16919, intervalFailed);
                Interval _16892 = Interval{ -6.0, -6.0 };
                Interval _16893 = _21429;
                Interval _21483 = imul(_16892, _16893, intervalFailed, optical_product_upper);
                Interval _16894 = Interval{ 0.0, 0.0 };
                Interval _16895 = _21429;
                Interval _21484 = jet_mul_derivative(_16894, _16895, intervalFailed, optical_product_upper);
                Interval _16896 = _21484;
                Interval _16897 = Interval{ -6.0, -6.0 };
                Interval _16898 = _21432;
                Interval _21485 = jet_mul_derivative(_16897, _16898, intervalFailed, optical_product_upper);
                Interval _16899 = _21485;
                Interval _21486 = jet_add_derivative(_16896, _16899, intervalFailed);
                Interval _16900 = Interval{ 0.0, 0.0 };
                Interval _16901 = _21429;
                Interval _21487 = jet_mul_derivative(_16900, _16901, intervalFailed, optical_product_upper);
                Interval _16902 = _21487;
                Interval _16903 = Interval{ -6.0, -6.0 };
                Interval _16904 = _21435;
                Interval _21488 = jet_mul_derivative(_16903, _16904, intervalFailed, optical_product_upper);
                Interval _16905 = _21488;
                Interval _21489 = jet_add_derivative(_16902, _16905, intervalFailed);
                Interval _16878 = _21483;
                Interval _16879 = Interval{ _21457, _21458 };
                Interval _21491 = imul(_16878, _16879, intervalFailed, optical_product_upper);
                Interval _16880 = _21486;
                Interval _16881 = Interval{ _21457, _21458 };
                Interval _21493 = jet_mul_derivative(_16880, _16881, intervalFailed, optical_product_upper);
                Interval _16882 = _21493;
                Interval _16883 = _21483;
                Interval _16884 = _21460;
                Interval _21494 = jet_mul_derivative(_16883, _16884, intervalFailed, optical_product_upper);
                Interval _16885 = _21494;
                Interval _21495 = jet_add_derivative(_16882, _16885, intervalFailed);
                Interval _16886 = _21489;
                Interval _16887 = Interval{ _21457, _21458 };
                Interval _21497 = jet_mul_derivative(_16886, _16887, intervalFailed, optical_product_upper);
                Interval _16888 = _21497;
                Interval _16889 = _21483;
                Interval _16890 = _21462;
                Interval _21498 = jet_mul_derivative(_16889, _16890, intervalFailed, optical_product_upper);
                Interval _16891 = _21498;
                Interval _21499 = jet_add_derivative(_16888, _16891, intervalFailed);
                Interval _16864 = Interval{ 128.0, 128.0 };
                Interval _16865 = Interval{ _21163, _21164 };
                Interval _21501 = imul(_16864, _16865, intervalFailed, optical_product_upper);
                Interval _16866 = Interval{ 0.0, 0.0 };
                Interval _16867 = Interval{ _21163, _21164 };
                Interval _21503 = jet_mul_derivative(_16866, _16867, intervalFailed, optical_product_upper);
                Interval _16868 = _21503;
                Interval _16869 = Interval{ 128.0, 128.0 };
                Interval _16870 = _21166;
                Interval _21504 = jet_mul_derivative(_16869, _16870, intervalFailed, optical_product_upper);
                Interval _16871 = _21504;
                Interval _21505 = jet_add_derivative(_16868, _16871, intervalFailed);
                Interval _16872 = Interval{ 0.0, 0.0 };
                Interval _16873 = Interval{ _21163, _21164 };
                Interval _21507 = jet_mul_derivative(_16872, _16873, intervalFailed, optical_product_upper);
                Interval _16874 = _21507;
                Interval _16875 = Interval{ 128.0, 128.0 };
                Interval _16876 = _21168;
                Interval _21508 = jet_mul_derivative(_16875, _16876, intervalFailed, optical_product_upper);
                Interval _16877 = _21508;
                Interval _21509 = jet_add_derivative(_16874, _16877, intervalFailed);
                Interval _16850 = _21491;
                Interval _16851 = _21501;
                Interval _21511 = idiv(_16850, _16851, intervalFailed, interval_divide_upper);
                bool _21520;
                if (!intervalFailed)
                {
                    _21520 = intervalFailed;
                }
                else
                {
                    _21520 = false;
                }
                bool _21525;
                if (_21520)
                {
                    _21525 = jetFailureSite == 0u;
                }
                else
                {
                    _21525 = false;
                }
                if (_21525)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21491.lo, _21491.hi, _21501.lo, _21501.hi);
                }
                Interval _16852 = _21495;
                Interval _16853 = _21511;
                Interval _16854 = _21505;
                Interval _21529 = jet_mul_derivative(_16853, _16854, intervalFailed, optical_product_upper);
                Interval _16855 = Interval{ as_type<float>(as_type<uint>(_21529.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21529.lo) ^ 2147483648u) };
                Interval _21539 = jet_add_derivative(_16852, _16855, intervalFailed);
                Interval _16856 = _21539;
                Interval _16857 = _21501;
                Interval _21540 = jet_div_derivative(_16856, _16857, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16858 = _21499;
                Interval _16859 = _21511;
                Interval _16860 = _21509;
                Interval _21541 = jet_mul_derivative(_16859, _16860, intervalFailed, optical_product_upper);
                Interval _16861 = Interval{ as_type<float>(as_type<uint>(_21541.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21541.lo) ^ 2147483648u) };
                Interval _21551 = jet_add_derivative(_16858, _16861, intervalFailed);
                Interval _16862 = _21551;
                Interval _16863 = _21501;
                Interval _21552 = jet_div_derivative(_16862, _16863, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16836 = _21511;
                Interval _16837 = _20877;
                Interval _21553 = imul(_16836, _16837, intervalFailed, optical_product_upper);
                Interval _16838 = _21540;
                Interval _16839 = _20877;
                Interval _21554 = jet_mul_derivative(_16838, _16839, intervalFailed, optical_product_upper);
                Interval _16840 = _21554;
                Interval _16841 = _21511;
                Interval _16842 = _20879;
                Interval _21555 = jet_mul_derivative(_16841, _16842, intervalFailed, optical_product_upper);
                Interval _16843 = _21555;
                Interval _21556 = jet_add_derivative(_16840, _16843, intervalFailed);
                Interval _16844 = _21552;
                Interval _16845 = _20877;
                Interval _21557 = jet_mul_derivative(_16844, _16845, intervalFailed, optical_product_upper);
                Interval _16846 = _21557;
                Interval _16847 = _21511;
                Interval _16848 = _20881;
                Interval _21558 = jet_mul_derivative(_16847, _16848, intervalFailed, optical_product_upper);
                Interval _16849 = _21558;
                Interval _21559 = jet_add_derivative(_16846, _16849, intervalFailed);
                Interval _16822 = _21511;
                Interval _16823 = _20988;
                Interval _21560 = imul(_16822, _16823, intervalFailed, optical_product_upper);
                Interval _16824 = _21540;
                Interval _16825 = _20988;
                Interval _21561 = jet_mul_derivative(_16824, _16825, intervalFailed, optical_product_upper);
                Interval _16826 = _21561;
                Interval _16827 = _21511;
                Interval _16828 = _20990;
                Interval _21562 = jet_mul_derivative(_16827, _16828, intervalFailed, optical_product_upper);
                Interval _16829 = _21562;
                Interval _21563 = jet_add_derivative(_16826, _16829, intervalFailed);
                Interval _16830 = _21552;
                Interval _16831 = _20988;
                Interval _21564 = jet_mul_derivative(_16830, _16831, intervalFailed, optical_product_upper);
                Interval _16832 = _21564;
                Interval _16833 = _21511;
                Interval _16834 = _20992;
                Interval _21565 = jet_mul_derivative(_16833, _16834, intervalFailed, optical_product_upper);
                Interval _16835 = _21565;
                Interval _21566 = jet_add_derivative(_16832, _16835, intervalFailed);
                Interval _16816 = _20482;
                Interval _16817 = _21476;
                Interval _21567 = iadd(_16816, _16817, intervalFailed);
                Interval _16818 = _20483;
                Interval _16819 = _21479;
                Interval _21568 = jet_add_derivative(_16818, _16819, intervalFailed);
                Interval _16820 = _20484;
                Interval _16821 = _21482;
                Interval _21569 = jet_add_derivative(_16820, _16821, intervalFailed);
                Interval _16810 = _20545;
                Interval _16811 = _21553;
                Interval _21576 = iadd(_16810, _16811, intervalFailed);
                Interval _16812 = _20546;
                Interval _16813 = _21556;
                Interval _21577 = jet_add_derivative(_16812, _16813, intervalFailed);
                Interval _16814 = _20547;
                Interval _16815 = _21559;
                Interval _21578 = jet_add_derivative(_16814, _16815, intervalFailed);
                Interval _16804 = _20608;
                Interval _16805 = _21560;
                Interval _21585 = iadd(_16804, _16805, intervalFailed);
                Interval _16806 = _20609;
                Interval _16807 = _21563;
                Interval _21586 = jet_add_derivative(_16806, _16807, intervalFailed);
                Interval _16808 = _20610;
                Interval _16809 = _21566;
                Interval _21587 = jet_add_derivative(_16808, _16809, intervalFailed);
                _21594 = _21585.lo;
                _21595 = _21585.hi;
                _21596 = _21586.lo;
                _21597 = _21586.hi;
                _21598 = _21587.lo;
                _21599 = _21587.hi;
                _21600 = _21576.lo;
                _21601 = _21576.hi;
                _21602 = _21577.lo;
                _21603 = _21577.hi;
                _21604 = _21578.lo;
                _21605 = _21578.hi;
                _21606 = _21567.lo;
                _21607 = _21567.hi;
                _21608 = _21568.lo;
                _21609 = _21568.hi;
                _21610 = _21569.lo;
                _21611 = _21569.hi;
            }
            else
            {
                _21594 = _20608.lo;
                _21595 = _20608.hi;
                _21596 = _20609.lo;
                _21597 = _20609.hi;
                _21598 = _20610.lo;
                _21599 = _20610.hi;
                _21600 = _20545.lo;
                _21601 = _20545.hi;
                _21602 = _20546.lo;
                _21603 = _20546.hi;
                _21604 = _20547.lo;
                _21605 = _20547.hi;
                _21606 = _20482.lo;
                _21607 = _20482.hi;
                _21608 = _20483.lo;
                _21609 = _20483.hi;
                _21610 = _20484.lo;
                _21611 = _20484.hi;
            }
        }
        _21616 = _21594;
        _21617 = _21595;
        _21618 = _21596;
        _21619 = _21597;
        _21620 = _21598;
        _21621 = _21599;
        _21622 = _21600;
        _21623 = _21601;
        _21624 = _21602;
        _21625 = _21603;
        _21626 = _21604;
        _21627 = _21605;
        _21628 = _21606;
        _21629 = _21607;
        _21630 = _21608;
        _21631 = _21609;
        _21632 = _21610;
        _21633 = _21611;
    }
    else
    {
        _21616 = _20608.lo;
        _21617 = _20608.hi;
        _21618 = _20609.lo;
        _21619 = _20609.hi;
        _21620 = _20610.lo;
        _21621 = _20610.hi;
        _21622 = _20545.lo;
        _21623 = _20545.hi;
        _21624 = _20546.lo;
        _21625 = _20546.hi;
        _21626 = _20547.lo;
        _21627 = _20547.hi;
        _21628 = _20482.lo;
        _21629 = _20482.hi;
        _21630 = _20483.lo;
        _21631 = _20483.hi;
        _21632 = _20484.lo;
        _21633 = _20484.hi;
    }
    return OpticalJet3{ OpticalJet{ Interval{ _21628, _21629 }, Interval{ _21630, _21631 }, Interval{ _21632, _21633 } }, OpticalJet{ Interval{ _21622, _21623 }, Interval{ _21624, _21625 }, Interval{ _21626, _21627 } }, OpticalJet{ Interval{ _21616, _21617 }, Interval{ _21618, _21619 }, Interval{ _21620, _21621 } } };
}

static __attribute__((noinline))
OpticalJet3 jet_liquid_normal(thread const ReflectionLiquidFrame& f, thread const OpticalJet3& direction, thread const OpticalJet& _distance, thread const OpticalJet& footprint, thread OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    Interval _13566 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13567 = hit.x.v;
    Interval _13595 = imul(_13566, _13567, intervalFailed, optical_product_upper);
    Interval _13568 = Interval{ 0.0, 0.0 };
    Interval _13569 = hit.x.v;
    Interval _13596 = jet_mul_derivative(_13568, _13569, intervalFailed, optical_product_upper);
    Interval _13570 = _13596;
    Interval _13571 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13572 = hit.x.dx;
    Interval _13598 = jet_mul_derivative(_13571, _13572, intervalFailed, optical_product_upper);
    Interval _13573 = _13598;
    Interval _13599 = jet_add_derivative(_13570, _13573, intervalFailed);
    Interval _13574 = Interval{ 0.0, 0.0 };
    Interval _13575 = hit.x.v;
    Interval _13600 = jet_mul_derivative(_13574, _13575, intervalFailed, optical_product_upper);
    Interval _13576 = _13600;
    Interval _13577 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13578 = hit.x.dy;
    Interval _13602 = jet_mul_derivative(_13577, _13578, intervalFailed, optical_product_upper);
    Interval _13579 = _13602;
    Interval _13603 = jet_add_derivative(_13576, _13579, intervalFailed);
    Interval _13552 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13553 = hit.x.v;
    Interval _13608 = imul(_13552, _13553, intervalFailed, optical_product_upper);
    Interval _13554 = Interval{ 0.0, 0.0 };
    Interval _13555 = hit.x.v;
    Interval _13609 = jet_mul_derivative(_13554, _13555, intervalFailed, optical_product_upper);
    Interval _13556 = _13609;
    Interval _13557 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13558 = hit.x.dx;
    Interval _13611 = jet_mul_derivative(_13557, _13558, intervalFailed, optical_product_upper);
    Interval _13559 = _13611;
    Interval _13612 = jet_add_derivative(_13556, _13559, intervalFailed);
    Interval _13560 = Interval{ 0.0, 0.0 };
    Interval _13561 = hit.x.v;
    Interval _13613 = jet_mul_derivative(_13560, _13561, intervalFailed, optical_product_upper);
    Interval _13562 = _13613;
    Interval _13563 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13564 = hit.x.dy;
    Interval _13615 = jet_mul_derivative(_13563, _13564, intervalFailed, optical_product_upper);
    Interval _13565 = _13615;
    Interval _13616 = jet_add_derivative(_13562, _13565, intervalFailed);
    Interval _13538 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13539 = hit.x.v;
    Interval _13621 = imul(_13538, _13539, intervalFailed, optical_product_upper);
    Interval _13540 = Interval{ 0.0, 0.0 };
    Interval _13541 = hit.x.v;
    Interval _13622 = jet_mul_derivative(_13540, _13541, intervalFailed, optical_product_upper);
    Interval _13542 = _13622;
    Interval _13543 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13544 = hit.x.dx;
    Interval _13624 = jet_mul_derivative(_13543, _13544, intervalFailed, optical_product_upper);
    Interval _13545 = _13624;
    Interval _13625 = jet_add_derivative(_13542, _13545, intervalFailed);
    Interval _13546 = Interval{ 0.0, 0.0 };
    Interval _13547 = hit.x.v;
    Interval _13626 = jet_mul_derivative(_13546, _13547, intervalFailed, optical_product_upper);
    Interval _13548 = _13626;
    Interval _13549 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13550 = hit.x.dy;
    Interval _13628 = jet_mul_derivative(_13549, _13550, intervalFailed, optical_product_upper);
    Interval _13551 = _13628;
    Interval _13629 = jet_add_derivative(_13548, _13551, intervalFailed);
    Interval _13524 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13525 = hit.y.v;
    Interval _13637 = imul(_13524, _13525, intervalFailed, optical_product_upper);
    Interval _13526 = Interval{ 0.0, 0.0 };
    Interval _13527 = hit.y.v;
    Interval _13638 = jet_mul_derivative(_13526, _13527, intervalFailed, optical_product_upper);
    Interval _13528 = _13638;
    Interval _13529 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13530 = hit.y.dx;
    Interval _13640 = jet_mul_derivative(_13529, _13530, intervalFailed, optical_product_upper);
    Interval _13531 = _13640;
    Interval _13641 = jet_add_derivative(_13528, _13531, intervalFailed);
    Interval _13532 = Interval{ 0.0, 0.0 };
    Interval _13533 = hit.y.v;
    Interval _13642 = jet_mul_derivative(_13532, _13533, intervalFailed, optical_product_upper);
    Interval _13534 = _13642;
    Interval _13535 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13536 = hit.y.dy;
    Interval _13644 = jet_mul_derivative(_13535, _13536, intervalFailed, optical_product_upper);
    Interval _13537 = _13644;
    Interval _13645 = jet_add_derivative(_13534, _13537, intervalFailed);
    Interval _13510 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13511 = hit.y.v;
    Interval _13650 = imul(_13510, _13511, intervalFailed, optical_product_upper);
    Interval _13512 = Interval{ 0.0, 0.0 };
    Interval _13513 = hit.y.v;
    Interval _13651 = jet_mul_derivative(_13512, _13513, intervalFailed, optical_product_upper);
    Interval _13514 = _13651;
    Interval _13515 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13516 = hit.y.dx;
    Interval _13653 = jet_mul_derivative(_13515, _13516, intervalFailed, optical_product_upper);
    Interval _13517 = _13653;
    Interval _13654 = jet_add_derivative(_13514, _13517, intervalFailed);
    Interval _13518 = Interval{ 0.0, 0.0 };
    Interval _13519 = hit.y.v;
    Interval _13655 = jet_mul_derivative(_13518, _13519, intervalFailed, optical_product_upper);
    Interval _13520 = _13655;
    Interval _13521 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13522 = hit.y.dy;
    Interval _13657 = jet_mul_derivative(_13521, _13522, intervalFailed, optical_product_upper);
    Interval _13523 = _13657;
    Interval _13658 = jet_add_derivative(_13520, _13523, intervalFailed);
    Interval _13496 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13497 = hit.y.v;
    Interval _13663 = imul(_13496, _13497, intervalFailed, optical_product_upper);
    Interval _13498 = Interval{ 0.0, 0.0 };
    Interval _13499 = hit.y.v;
    Interval _13664 = jet_mul_derivative(_13498, _13499, intervalFailed, optical_product_upper);
    Interval _13500 = _13664;
    Interval _13501 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13502 = hit.y.dx;
    Interval _13666 = jet_mul_derivative(_13501, _13502, intervalFailed, optical_product_upper);
    Interval _13503 = _13666;
    Interval _13667 = jet_add_derivative(_13500, _13503, intervalFailed);
    Interval _13504 = Interval{ 0.0, 0.0 };
    Interval _13505 = hit.y.v;
    Interval _13668 = jet_mul_derivative(_13504, _13505, intervalFailed, optical_product_upper);
    Interval _13506 = _13668;
    Interval _13507 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13508 = hit.y.dy;
    Interval _13670 = jet_mul_derivative(_13507, _13508, intervalFailed, optical_product_upper);
    Interval _13509 = _13670;
    Interval _13671 = jet_add_derivative(_13506, _13509, intervalFailed);
    Interval _13490 = _13595;
    Interval _13491 = _13637;
    Interval _13672 = iadd(_13490, _13491, intervalFailed);
    Interval _13492 = _13599;
    Interval _13493 = _13641;
    Interval _13673 = jet_add_derivative(_13492, _13493, intervalFailed);
    Interval _13494 = _13603;
    Interval _13495 = _13645;
    Interval _13674 = jet_add_derivative(_13494, _13495, intervalFailed);
    Interval _13484 = _13608;
    Interval _13485 = _13650;
    Interval _13675 = iadd(_13484, _13485, intervalFailed);
    Interval _13486 = _13612;
    Interval _13487 = _13654;
    Interval _13676 = jet_add_derivative(_13486, _13487, intervalFailed);
    Interval _13488 = _13616;
    Interval _13489 = _13658;
    Interval _13677 = jet_add_derivative(_13488, _13489, intervalFailed);
    Interval _13478 = _13621;
    Interval _13479 = _13663;
    Interval _13678 = iadd(_13478, _13479, intervalFailed);
    Interval _13480 = _13625;
    Interval _13481 = _13667;
    Interval _13679 = jet_add_derivative(_13480, _13481, intervalFailed);
    Interval _13482 = _13629;
    Interval _13483 = _13671;
    Interval _13680 = jet_add_derivative(_13482, _13483, intervalFailed);
    Interval _13464 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13465 = hit.z.v;
    Interval _13688 = imul(_13464, _13465, intervalFailed, optical_product_upper);
    Interval _13466 = Interval{ 0.0, 0.0 };
    Interval _13467 = hit.z.v;
    Interval _13689 = jet_mul_derivative(_13466, _13467, intervalFailed, optical_product_upper);
    Interval _13468 = _13689;
    Interval _13469 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13470 = hit.z.dx;
    Interval _13691 = jet_mul_derivative(_13469, _13470, intervalFailed, optical_product_upper);
    Interval _13471 = _13691;
    Interval _13692 = jet_add_derivative(_13468, _13471, intervalFailed);
    Interval _13472 = Interval{ 0.0, 0.0 };
    Interval _13473 = hit.z.v;
    Interval _13693 = jet_mul_derivative(_13472, _13473, intervalFailed, optical_product_upper);
    Interval _13474 = _13693;
    Interval _13475 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13476 = hit.z.dy;
    Interval _13695 = jet_mul_derivative(_13475, _13476, intervalFailed, optical_product_upper);
    Interval _13477 = _13695;
    Interval _13696 = jet_add_derivative(_13474, _13477, intervalFailed);
    Interval _13450 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13451 = hit.z.v;
    Interval _13701 = imul(_13450, _13451, intervalFailed, optical_product_upper);
    Interval _13452 = Interval{ 0.0, 0.0 };
    Interval _13453 = hit.z.v;
    Interval _13702 = jet_mul_derivative(_13452, _13453, intervalFailed, optical_product_upper);
    Interval _13454 = _13702;
    Interval _13455 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13456 = hit.z.dx;
    Interval _13704 = jet_mul_derivative(_13455, _13456, intervalFailed, optical_product_upper);
    Interval _13457 = _13704;
    Interval _13705 = jet_add_derivative(_13454, _13457, intervalFailed);
    Interval _13458 = Interval{ 0.0, 0.0 };
    Interval _13459 = hit.z.v;
    Interval _13706 = jet_mul_derivative(_13458, _13459, intervalFailed, optical_product_upper);
    Interval _13460 = _13706;
    Interval _13461 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13462 = hit.z.dy;
    Interval _13708 = jet_mul_derivative(_13461, _13462, intervalFailed, optical_product_upper);
    Interval _13463 = _13708;
    Interval _13709 = jet_add_derivative(_13460, _13463, intervalFailed);
    Interval _13436 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13437 = hit.z.v;
    Interval _13714 = imul(_13436, _13437, intervalFailed, optical_product_upper);
    Interval _13438 = Interval{ 0.0, 0.0 };
    Interval _13439 = hit.z.v;
    Interval _13715 = jet_mul_derivative(_13438, _13439, intervalFailed, optical_product_upper);
    Interval _13440 = _13715;
    Interval _13441 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13442 = hit.z.dx;
    Interval _13717 = jet_mul_derivative(_13441, _13442, intervalFailed, optical_product_upper);
    Interval _13443 = _13717;
    Interval _13718 = jet_add_derivative(_13440, _13443, intervalFailed);
    Interval _13444 = Interval{ 0.0, 0.0 };
    Interval _13445 = hit.z.v;
    Interval _13719 = jet_mul_derivative(_13444, _13445, intervalFailed, optical_product_upper);
    Interval _13446 = _13719;
    Interval _13447 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13448 = hit.z.dy;
    Interval _13721 = jet_mul_derivative(_13447, _13448, intervalFailed, optical_product_upper);
    Interval _13449 = _13721;
    Interval _13722 = jet_add_derivative(_13446, _13449, intervalFailed);
    Interval _13430 = _13672;
    Interval _13431 = _13688;
    Interval _13723 = iadd(_13430, _13431, intervalFailed);
    Interval _13432 = _13673;
    Interval _13433 = _13692;
    Interval _13724 = jet_add_derivative(_13432, _13433, intervalFailed);
    Interval _13434 = _13674;
    Interval _13435 = _13696;
    Interval _13725 = jet_add_derivative(_13434, _13435, intervalFailed);
    Interval _13424 = _13675;
    Interval _13425 = _13701;
    Interval _13726 = iadd(_13424, _13425, intervalFailed);
    Interval _13426 = _13676;
    Interval _13427 = _13705;
    Interval _13727 = jet_add_derivative(_13426, _13427, intervalFailed);
    Interval _13428 = _13677;
    Interval _13429 = _13709;
    Interval _13728 = jet_add_derivative(_13428, _13429, intervalFailed);
    Interval _13418 = _13678;
    Interval _13419 = _13714;
    Interval _13729 = iadd(_13418, _13419, intervalFailed);
    Interval _13420 = _13679;
    Interval _13421 = _13718;
    Interval _13730 = jet_add_derivative(_13420, _13421, intervalFailed);
    Interval _13422 = _13680;
    Interval _13423 = _13722;
    Interval _13731 = jet_add_derivative(_13422, _13423, intervalFailed);
    Interval _13412 = _13723;
    Interval _13413 = Interval{ f.rotation0.w, f.rotation0.w };
    Interval _13742 = iadd(_13412, _13413, intervalFailed);
    Interval _13414 = _13724;
    Interval _13415 = Interval{ 0.0, 0.0 };
    Interval _13743 = jet_add_derivative(_13414, _13415, intervalFailed);
    Interval _13416 = _13725;
    Interval _13417 = Interval{ 0.0, 0.0 };
    Interval _13744 = jet_add_derivative(_13416, _13417, intervalFailed);
    Interval _13406 = _13726;
    Interval _13407 = Interval{ f.rotation1.w, f.rotation1.w };
    Interval _13746 = iadd(_13406, _13407, intervalFailed);
    Interval _13408 = _13727;
    Interval _13409 = Interval{ 0.0, 0.0 };
    Interval _13747 = jet_add_derivative(_13408, _13409, intervalFailed);
    Interval _13410 = _13728;
    Interval _13411 = Interval{ 0.0, 0.0 };
    Interval _13748 = jet_add_derivative(_13410, _13411, intervalFailed);
    Interval _13400 = _13729;
    Interval _13401 = Interval{ f.rotation2.w, f.rotation2.w };
    Interval _13750 = iadd(_13400, _13401, intervalFailed);
    Interval _13402 = _13730;
    Interval _13403 = Interval{ 0.0, 0.0 };
    Interval _13751 = jet_add_derivative(_13402, _13403, intervalFailed);
    Interval _13404 = _13731;
    Interval _13405 = Interval{ 0.0, 0.0 };
    Interval _13752 = jet_add_derivative(_13404, _13405, intervalFailed);
    float _15366;
    float _15367;
    float _15368;
    float _15369;
    float _15370;
    float _15371;
    float _15372;
    float _15373;
    float _15374;
    float _15375;
    float _15376;
    float _15377;
    if (f.settings.y == 3.0)
    {
        Interval _13386 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13387 = direction.x.v;
        Interval _14850 = imul(_13386, _13387, intervalFailed, optical_product_upper);
        Interval _13388 = Interval{ 0.0, 0.0 };
        Interval _13389 = direction.x.v;
        Interval _14851 = jet_mul_derivative(_13388, _13389, intervalFailed, optical_product_upper);
        Interval _13390 = _14851;
        Interval _13391 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13392 = direction.x.dx;
        Interval _14853 = jet_mul_derivative(_13391, _13392, intervalFailed, optical_product_upper);
        Interval _13393 = _14853;
        Interval _14854 = jet_add_derivative(_13390, _13393, intervalFailed);
        Interval _13394 = Interval{ 0.0, 0.0 };
        Interval _13395 = direction.x.v;
        Interval _14855 = jet_mul_derivative(_13394, _13395, intervalFailed, optical_product_upper);
        Interval _13396 = _14855;
        Interval _13397 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13398 = direction.x.dy;
        Interval _14857 = jet_mul_derivative(_13397, _13398, intervalFailed, optical_product_upper);
        Interval _13399 = _14857;
        Interval _14858 = jet_add_derivative(_13396, _13399, intervalFailed);
        Interval _13372 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13373 = direction.x.v;
        Interval _14863 = imul(_13372, _13373, intervalFailed, optical_product_upper);
        Interval _13374 = Interval{ 0.0, 0.0 };
        Interval _13375 = direction.x.v;
        Interval _14864 = jet_mul_derivative(_13374, _13375, intervalFailed, optical_product_upper);
        Interval _13376 = _14864;
        Interval _13377 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13378 = direction.x.dx;
        Interval _14866 = jet_mul_derivative(_13377, _13378, intervalFailed, optical_product_upper);
        Interval _13379 = _14866;
        Interval _14867 = jet_add_derivative(_13376, _13379, intervalFailed);
        Interval _13380 = Interval{ 0.0, 0.0 };
        Interval _13381 = direction.x.v;
        Interval _14868 = jet_mul_derivative(_13380, _13381, intervalFailed, optical_product_upper);
        Interval _13382 = _14868;
        Interval _13383 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13384 = direction.x.dy;
        Interval _14870 = jet_mul_derivative(_13383, _13384, intervalFailed, optical_product_upper);
        Interval _13385 = _14870;
        Interval _14871 = jet_add_derivative(_13382, _13385, intervalFailed);
        Interval _13358 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13359 = direction.x.v;
        Interval _14876 = imul(_13358, _13359, intervalFailed, optical_product_upper);
        Interval _13360 = Interval{ 0.0, 0.0 };
        Interval _13361 = direction.x.v;
        Interval _14877 = jet_mul_derivative(_13360, _13361, intervalFailed, optical_product_upper);
        Interval _13362 = _14877;
        Interval _13363 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13364 = direction.x.dx;
        Interval _14879 = jet_mul_derivative(_13363, _13364, intervalFailed, optical_product_upper);
        Interval _13365 = _14879;
        Interval _14880 = jet_add_derivative(_13362, _13365, intervalFailed);
        Interval _13366 = Interval{ 0.0, 0.0 };
        Interval _13367 = direction.x.v;
        Interval _14881 = jet_mul_derivative(_13366, _13367, intervalFailed, optical_product_upper);
        Interval _13368 = _14881;
        Interval _13369 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13370 = direction.x.dy;
        Interval _14883 = jet_mul_derivative(_13369, _13370, intervalFailed, optical_product_upper);
        Interval _13371 = _14883;
        Interval _14884 = jet_add_derivative(_13368, _13371, intervalFailed);
        Interval _13344 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13345 = direction.y.v;
        Interval _14892 = imul(_13344, _13345, intervalFailed, optical_product_upper);
        Interval _13346 = Interval{ 0.0, 0.0 };
        Interval _13347 = direction.y.v;
        Interval _14893 = jet_mul_derivative(_13346, _13347, intervalFailed, optical_product_upper);
        Interval _13348 = _14893;
        Interval _13349 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13350 = direction.y.dx;
        Interval _14895 = jet_mul_derivative(_13349, _13350, intervalFailed, optical_product_upper);
        Interval _13351 = _14895;
        Interval _14896 = jet_add_derivative(_13348, _13351, intervalFailed);
        Interval _13352 = Interval{ 0.0, 0.0 };
        Interval _13353 = direction.y.v;
        Interval _14897 = jet_mul_derivative(_13352, _13353, intervalFailed, optical_product_upper);
        Interval _13354 = _14897;
        Interval _13355 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13356 = direction.y.dy;
        Interval _14899 = jet_mul_derivative(_13355, _13356, intervalFailed, optical_product_upper);
        Interval _13357 = _14899;
        Interval _14900 = jet_add_derivative(_13354, _13357, intervalFailed);
        Interval _13330 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13331 = direction.y.v;
        Interval _14905 = imul(_13330, _13331, intervalFailed, optical_product_upper);
        Interval _13332 = Interval{ 0.0, 0.0 };
        Interval _13333 = direction.y.v;
        Interval _14906 = jet_mul_derivative(_13332, _13333, intervalFailed, optical_product_upper);
        Interval _13334 = _14906;
        Interval _13335 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13336 = direction.y.dx;
        Interval _14908 = jet_mul_derivative(_13335, _13336, intervalFailed, optical_product_upper);
        Interval _13337 = _14908;
        Interval _14909 = jet_add_derivative(_13334, _13337, intervalFailed);
        Interval _13338 = Interval{ 0.0, 0.0 };
        Interval _13339 = direction.y.v;
        Interval _14910 = jet_mul_derivative(_13338, _13339, intervalFailed, optical_product_upper);
        Interval _13340 = _14910;
        Interval _13341 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13342 = direction.y.dy;
        Interval _14912 = jet_mul_derivative(_13341, _13342, intervalFailed, optical_product_upper);
        Interval _13343 = _14912;
        Interval _14913 = jet_add_derivative(_13340, _13343, intervalFailed);
        Interval _13316 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13317 = direction.y.v;
        Interval _14918 = imul(_13316, _13317, intervalFailed, optical_product_upper);
        Interval _13318 = Interval{ 0.0, 0.0 };
        Interval _13319 = direction.y.v;
        Interval _14919 = jet_mul_derivative(_13318, _13319, intervalFailed, optical_product_upper);
        Interval _13320 = _14919;
        Interval _13321 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13322 = direction.y.dx;
        Interval _14921 = jet_mul_derivative(_13321, _13322, intervalFailed, optical_product_upper);
        Interval _13323 = _14921;
        Interval _14922 = jet_add_derivative(_13320, _13323, intervalFailed);
        Interval _13324 = Interval{ 0.0, 0.0 };
        Interval _13325 = direction.y.v;
        Interval _14923 = jet_mul_derivative(_13324, _13325, intervalFailed, optical_product_upper);
        Interval _13326 = _14923;
        Interval _13327 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13328 = direction.y.dy;
        Interval _14925 = jet_mul_derivative(_13327, _13328, intervalFailed, optical_product_upper);
        Interval _13329 = _14925;
        Interval _14926 = jet_add_derivative(_13326, _13329, intervalFailed);
        Interval _13310 = _14850;
        Interval _13311 = _14892;
        Interval _14927 = iadd(_13310, _13311, intervalFailed);
        Interval _13312 = _14854;
        Interval _13313 = _14896;
        Interval _14928 = jet_add_derivative(_13312, _13313, intervalFailed);
        Interval _13314 = _14858;
        Interval _13315 = _14900;
        Interval _14929 = jet_add_derivative(_13314, _13315, intervalFailed);
        Interval _13304 = _14863;
        Interval _13305 = _14905;
        Interval _14930 = iadd(_13304, _13305, intervalFailed);
        Interval _13306 = _14867;
        Interval _13307 = _14909;
        Interval _14931 = jet_add_derivative(_13306, _13307, intervalFailed);
        Interval _13308 = _14871;
        Interval _13309 = _14913;
        Interval _14932 = jet_add_derivative(_13308, _13309, intervalFailed);
        Interval _13298 = _14876;
        Interval _13299 = _14918;
        Interval _14933 = iadd(_13298, _13299, intervalFailed);
        Interval _13300 = _14880;
        Interval _13301 = _14922;
        Interval _14934 = jet_add_derivative(_13300, _13301, intervalFailed);
        Interval _13302 = _14884;
        Interval _13303 = _14926;
        Interval _14935 = jet_add_derivative(_13302, _13303, intervalFailed);
        Interval _13284 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13285 = direction.z.v;
        Interval _14943 = imul(_13284, _13285, intervalFailed, optical_product_upper);
        Interval _13286 = Interval{ 0.0, 0.0 };
        Interval _13287 = direction.z.v;
        Interval _14944 = jet_mul_derivative(_13286, _13287, intervalFailed, optical_product_upper);
        Interval _13288 = _14944;
        Interval _13289 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13290 = direction.z.dx;
        Interval _14946 = jet_mul_derivative(_13289, _13290, intervalFailed, optical_product_upper);
        Interval _13291 = _14946;
        Interval _14947 = jet_add_derivative(_13288, _13291, intervalFailed);
        Interval _13292 = Interval{ 0.0, 0.0 };
        Interval _13293 = direction.z.v;
        Interval _14948 = jet_mul_derivative(_13292, _13293, intervalFailed, optical_product_upper);
        Interval _13294 = _14948;
        Interval _13295 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13296 = direction.z.dy;
        Interval _14950 = jet_mul_derivative(_13295, _13296, intervalFailed, optical_product_upper);
        Interval _13297 = _14950;
        Interval _14951 = jet_add_derivative(_13294, _13297, intervalFailed);
        Interval _13270 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13271 = direction.z.v;
        Interval _14956 = imul(_13270, _13271, intervalFailed, optical_product_upper);
        Interval _13272 = Interval{ 0.0, 0.0 };
        Interval _13273 = direction.z.v;
        Interval _14957 = jet_mul_derivative(_13272, _13273, intervalFailed, optical_product_upper);
        Interval _13274 = _14957;
        Interval _13275 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13276 = direction.z.dx;
        Interval _14959 = jet_mul_derivative(_13275, _13276, intervalFailed, optical_product_upper);
        Interval _13277 = _14959;
        Interval _14960 = jet_add_derivative(_13274, _13277, intervalFailed);
        Interval _13278 = Interval{ 0.0, 0.0 };
        Interval _13279 = direction.z.v;
        Interval _14961 = jet_mul_derivative(_13278, _13279, intervalFailed, optical_product_upper);
        Interval _13280 = _14961;
        Interval _13281 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13282 = direction.z.dy;
        Interval _14963 = jet_mul_derivative(_13281, _13282, intervalFailed, optical_product_upper);
        Interval _13283 = _14963;
        Interval _14964 = jet_add_derivative(_13280, _13283, intervalFailed);
        Interval _13256 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13257 = direction.z.v;
        Interval _14969 = imul(_13256, _13257, intervalFailed, optical_product_upper);
        Interval _13258 = Interval{ 0.0, 0.0 };
        Interval _13259 = direction.z.v;
        Interval _14970 = jet_mul_derivative(_13258, _13259, intervalFailed, optical_product_upper);
        Interval _13260 = _14970;
        Interval _13261 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13262 = direction.z.dx;
        Interval _14972 = jet_mul_derivative(_13261, _13262, intervalFailed, optical_product_upper);
        Interval _13263 = _14972;
        Interval _14973 = jet_add_derivative(_13260, _13263, intervalFailed);
        Interval _13264 = Interval{ 0.0, 0.0 };
        Interval _13265 = direction.z.v;
        Interval _14974 = jet_mul_derivative(_13264, _13265, intervalFailed, optical_product_upper);
        Interval _13266 = _14974;
        Interval _13267 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13268 = direction.z.dy;
        Interval _14976 = jet_mul_derivative(_13267, _13268, intervalFailed, optical_product_upper);
        Interval _13269 = _14976;
        Interval _14977 = jet_add_derivative(_13266, _13269, intervalFailed);
        Interval _13250 = _14927;
        Interval _13251 = _14943;
        Interval _14978 = iadd(_13250, _13251, intervalFailed);
        Interval _13252 = _14928;
        Interval _13253 = _14947;
        Interval _14979 = jet_add_derivative(_13252, _13253, intervalFailed);
        Interval _13254 = _14929;
        Interval _13255 = _14951;
        Interval _14980 = jet_add_derivative(_13254, _13255, intervalFailed);
        Interval _13244 = _14930;
        Interval _13245 = _14956;
        Interval _14981 = iadd(_13244, _13245, intervalFailed);
        Interval _13246 = _14931;
        Interval _13247 = _14960;
        Interval _14982 = jet_add_derivative(_13246, _13247, intervalFailed);
        Interval _13248 = _14932;
        Interval _13249 = _14964;
        Interval _14983 = jet_add_derivative(_13248, _13249, intervalFailed);
        Interval _13238 = _14933;
        Interval _13239 = _14969;
        Interval _14984 = iadd(_13238, _13239, intervalFailed);
        Interval _13240 = _14934;
        Interval _13241 = _14973;
        Interval _14985 = jet_add_derivative(_13240, _13241, intervalFailed);
        Interval _13242 = _14935;
        Interval _13243 = _14977;
        Interval _14986 = jet_add_derivative(_13242, _13243, intervalFailed);
        float _15023;
        float _15025;
        float _15027;
        float _15029;
        float _15031;
        float _15033;
        float _15035;
        float _15037;
        float _15039;
        float _15041;
        float _15043;
        float _15045;
        _15023 = 0.0;
        _15025 = 0.0;
        _15027 = 0.0;
        _15029 = 0.0;
        _15031 = 0.0;
        _15033 = 0.0;
        _15035 = 0.0;
        _15037 = 0.0;
        _15039 = 0.0;
        _15041 = 0.0;
        _15043 = 0.0;
        _15045 = 0.0;
        float _14988;
        float _14990;
        float _14992;
        float _14994;
        float _14996;
        float _14998;
        float _15000;
        float _15002;
        float _15004;
        float _15006;
        float _15008;
        float _15010;
        float _15012;
        float _15014;
        float _15016;
        float _15018;
        float _15020;
        float _15022;
        float _15024;
        float _15026;
        float _15028;
        float _15030;
        float _15032;
        float _15034;
        float _15036;
        float _15038;
        float _15040;
        float _15042;
        float _15044;
        float _15046;
        float _14987 = _13746.lo;
        float _14989 = _13746.hi;
        float _14991 = _13747.lo;
        float _14993 = _13747.hi;
        float _14995 = _13748.lo;
        float _14997 = _13748.hi;
        float _14999 = _13750.lo;
        float _15001 = _13750.hi;
        float _15003 = _13751.lo;
        float _15005 = _13751.hi;
        float _15007 = _13752.lo;
        float _15009 = _13752.hi;
        float _15011 = _13742.lo;
        float _15013 = _13742.hi;
        float _15015 = _13743.lo;
        float _15017 = _13743.hi;
        float _15019 = _13744.lo;
        float _15021 = _13744.hi;
        uint _15047 = 0u;
        for (; _15047 < 2u; _14987 = _14988, _14989 = _14990, _14991 = _14992, _14993 = _14994, _14995 = _14996, _14997 = _14998, _14999 = _15000, _15001 = _15002, _15003 = _15004, _15005 = _15006, _15007 = _15008, _15009 = _15010, _15011 = _15012, _15013 = _15014, _15015 = _15016, _15017 = _15018, _15019 = _15020, _15021 = _15022, _15023 = _15024, _15025 = _15026, _15027 = _15028, _15029 = _15030, _15031 = _15032, _15033 = _15034, _15035 = _15036, _15037 = _15038, _15039 = _15040, _15041 = _15042, _15043 = _15044, _15045 = _15046, _15047++)
        {
            OpticalJet param_var_x = OpticalJet{ Interval{ _15011, _15013 }, Interval{ _15015, _15017 }, Interval{ _15019, _15021 } };
            OpticalJet param_var_z = OpticalJet{ Interval{ _14999, _15001 }, Interval{ _15003, _15005 }, Interval{ _15007, _15009 } };
            OpticalJet param_var_t = OpticalJet{ Interval{ f.settings.x, f.settings.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet param_var_footprint = footprint;
            OpticalJet3 _15060 = jlava(param_var_x, param_var_z, param_var_t, param_var_footprint, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (_15047 == 0u)
            {
                float _13236 = 2.0;
                float _13237 = 10.0;
                Interval _15088 = iratio(_13236, _13237, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _15093;
                if (!intervalFailed)
                {
                    _15093 = intervalFailed;
                }
                else
                {
                    _15093 = false;
                }
                bool _15098;
                if (_15093)
                {
                    _15098 = jetFailureSite == 0u;
                }
                else
                {
                    _15098 = false;
                }
                if (_15098)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(2.0, 2.0, 10.0, 10.0);
                }
                Interval _13222 = _distance.v;
                Interval _13223 = _15088;
                Interval _15101 = imul(_13222, _13223, intervalFailed, optical_product_upper);
                Interval _13224 = _distance.dx;
                Interval _13225 = _15088;
                Interval _15102 = jet_mul_derivative(_13224, _13225, intervalFailed, optical_product_upper);
                Interval _13226 = _15102;
                Interval _13227 = _distance.v;
                Interval _13228 = Interval{ 0.0, 0.0 };
                Interval _15103 = jet_mul_derivative(_13227, _13228, intervalFailed, optical_product_upper);
                Interval _13229 = _15103;
                Interval _15104 = jet_add_derivative(_13226, _13229, intervalFailed);
                Interval _13230 = _distance.dy;
                Interval _13231 = _15088;
                Interval _15105 = jet_mul_derivative(_13230, _13231, intervalFailed, optical_product_upper);
                Interval _13232 = _15105;
                Interval _13233 = _distance.v;
                Interval _13234 = Interval{ 0.0, 0.0 };
                Interval _15106 = jet_mul_derivative(_13233, _13234, intervalFailed, optical_product_upper);
                Interval _13235 = _15106;
                Interval _15107 = jet_add_derivative(_13232, _13235, intervalFailed);
                float _15115 = as_type<float>(as_type<uint>(_15060.x.v.hi) ^ 2147483648u);
                float _15118 = as_type<float>(as_type<uint>(_15060.x.v.lo) ^ 2147483648u);
                OpticalJet param_var_a = OpticalJet{ _14981, _14982, _14983 };
                float _13220 = 12.0;
                float _13221 = 100.0;
                Interval _15137 = iratio(_13220, _13221, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _15142;
                if (!intervalFailed)
                {
                    _15142 = intervalFailed;
                }
                else
                {
                    _15142 = false;
                }
                bool _15147;
                if (_15142)
                {
                    _15147 = jetFailureSite == 0u;
                }
                else
                {
                    _15147 = false;
                }
                if (_15147)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(12.0, 12.0, 100.0, 100.0);
                }
                OpticalJet param_var_b = OpticalJet{ _15137, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _15151 = jmax(param_var_a, param_var_b);
                Interval _15152 = _15151.v;
                Interval _13206 = Interval{ _15115, _15118 };
                Interval _13207 = _15152;
                Interval _15157 = idiv(_13206, _13207, intervalFailed, interval_divide_upper);
                bool _15164;
                if (!intervalFailed)
                {
                    _15164 = intervalFailed;
                }
                else
                {
                    _15164 = false;
                }
                bool _15169;
                if (_15164)
                {
                    _15169 = jetFailureSite == 0u;
                }
                else
                {
                    _15169 = false;
                }
                if (_15169)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_15115, _15118, _15152.lo, _15152.hi);
                }
                Interval _13208 = Interval{ as_type<float>(as_type<uint>(_15060.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15060.x.dx.lo) ^ 2147483648u) };
                Interval _13209 = _15157;
                Interval _13210 = _15151.dx;
                Interval _15174 = jet_mul_derivative(_13209, _13210, intervalFailed, optical_product_upper);
                Interval _13211 = Interval{ as_type<float>(as_type<uint>(_15174.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15174.lo) ^ 2147483648u) };
                Interval _15184 = jet_add_derivative(_13208, _13211, intervalFailed);
                Interval _13212 = _15184;
                Interval _13213 = _15152;
                Interval _15185 = jet_div_derivative(_13212, _13213, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _13214 = Interval{ as_type<float>(as_type<uint>(_15060.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15060.x.dy.lo) ^ 2147483648u) };
                Interval _13215 = _15157;
                Interval _13216 = _15151.dy;
                Interval _15187 = jet_mul_derivative(_13215, _13216, intervalFailed, optical_product_upper);
                Interval _13217 = Interval{ as_type<float>(as_type<uint>(_15187.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15187.lo) ^ 2147483648u) };
                Interval _15197 = jet_add_derivative(_13214, _13217, intervalFailed);
                Interval _13218 = _15197;
                Interval _13219 = _15152;
                Interval _15198 = jet_div_derivative(_13218, _13219, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                OpticalJet _13202 = OpticalJet{ _15157, _15185, _15198 };
                OpticalJet _13203 = OpticalJet{ Interval{ as_type<float>(as_type<uint>(_15101.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15101.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_15104.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15104.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_15107.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15107.lo) ^ 2147483648u) } };
                OpticalJet _13204 = jmax(_13202, _13203);
                OpticalJet _13205 = OpticalJet{ _15101, _15104, _15107 };
                OpticalJet _15230 = jmin(_13204, _13205);
                Interval _15231 = _15230.v;
                Interval _13188 = _14978;
                Interval _13189 = _15231;
                Interval _15234 = imul(_13188, _13189, intervalFailed, optical_product_upper);
                Interval _13190 = _14979;
                Interval _13191 = _15231;
                Interval _15235 = jet_mul_derivative(_13190, _13191, intervalFailed, optical_product_upper);
                Interval _13192 = _15235;
                Interval _13193 = _14978;
                Interval _13194 = _15230.dx;
                Interval _15236 = jet_mul_derivative(_13193, _13194, intervalFailed, optical_product_upper);
                Interval _13195 = _15236;
                Interval _15237 = jet_add_derivative(_13192, _13195, intervalFailed);
                Interval _13196 = _14980;
                Interval _13197 = _15231;
                Interval _15238 = jet_mul_derivative(_13196, _13197, intervalFailed, optical_product_upper);
                Interval _13198 = _15238;
                Interval _13199 = _14978;
                Interval _13200 = _15230.dy;
                Interval _15239 = jet_mul_derivative(_13199, _13200, intervalFailed, optical_product_upper);
                Interval _13201 = _15239;
                Interval _15240 = jet_add_derivative(_13198, _13201, intervalFailed);
                Interval _15241 = _15230.v;
                Interval _13174 = _14981;
                Interval _13175 = _15241;
                Interval _15244 = imul(_13174, _13175, intervalFailed, optical_product_upper);
                Interval _13176 = _14982;
                Interval _13177 = _15241;
                Interval _15245 = jet_mul_derivative(_13176, _13177, intervalFailed, optical_product_upper);
                Interval _13178 = _15245;
                Interval _13179 = _14981;
                Interval _13180 = _15230.dx;
                Interval _15246 = jet_mul_derivative(_13179, _13180, intervalFailed, optical_product_upper);
                Interval _13181 = _15246;
                Interval _15247 = jet_add_derivative(_13178, _13181, intervalFailed);
                Interval _13182 = _14983;
                Interval _13183 = _15241;
                Interval _15248 = jet_mul_derivative(_13182, _13183, intervalFailed, optical_product_upper);
                Interval _13184 = _15248;
                Interval _13185 = _14981;
                Interval _13186 = _15230.dy;
                Interval _15249 = jet_mul_derivative(_13185, _13186, intervalFailed, optical_product_upper);
                Interval _13187 = _15249;
                Interval _15250 = jet_add_derivative(_13184, _13187, intervalFailed);
                Interval _15251 = _15230.v;
                Interval _13160 = _14984;
                Interval _13161 = _15251;
                Interval _15254 = imul(_13160, _13161, intervalFailed, optical_product_upper);
                Interval _13162 = _14985;
                Interval _13163 = _15251;
                Interval _15255 = jet_mul_derivative(_13162, _13163, intervalFailed, optical_product_upper);
                Interval _13164 = _15255;
                Interval _13165 = _14984;
                Interval _13166 = _15230.dx;
                Interval _15256 = jet_mul_derivative(_13165, _13166, intervalFailed, optical_product_upper);
                Interval _13167 = _15256;
                Interval _15257 = jet_add_derivative(_13164, _13167, intervalFailed);
                Interval _13168 = _14986;
                Interval _13169 = _15251;
                Interval _15258 = jet_mul_derivative(_13168, _13169, intervalFailed, optical_product_upper);
                Interval _13170 = _15258;
                Interval _13171 = _14984;
                Interval _13172 = _15230.dy;
                Interval _15259 = jet_mul_derivative(_13171, _13172, intervalFailed, optical_product_upper);
                Interval _13173 = _15259;
                Interval _15260 = jet_add_derivative(_13170, _13173, intervalFailed);
                Interval _13154 = Interval{ _15011, _15013 };
                Interval _13155 = _15234;
                Interval _15262 = iadd(_13154, _13155, intervalFailed);
                Interval _13156 = Interval{ _15015, _15017 };
                Interval _13157 = _15237;
                Interval _15264 = jet_add_derivative(_13156, _13157, intervalFailed);
                Interval _13158 = Interval{ _15019, _15021 };
                Interval _13159 = _15240;
                Interval _15266 = jet_add_derivative(_13158, _13159, intervalFailed);
                Interval _13148 = Interval{ _14987, _14989 };
                Interval _13149 = _15244;
                Interval _15268 = iadd(_13148, _13149, intervalFailed);
                Interval _13150 = Interval{ _14991, _14993 };
                Interval _13151 = _15247;
                Interval _15270 = jet_add_derivative(_13150, _13151, intervalFailed);
                Interval _13152 = Interval{ _14995, _14997 };
                Interval _13153 = _15250;
                Interval _15272 = jet_add_derivative(_13152, _13153, intervalFailed);
                Interval _13142 = Interval{ _14999, _15001 };
                Interval _13143 = _15254;
                Interval _15274 = iadd(_13142, _13143, intervalFailed);
                Interval _13144 = Interval{ _15003, _15005 };
                Interval _13145 = _15257;
                Interval _15276 = jet_add_derivative(_13144, _13145, intervalFailed);
                Interval _13146 = Interval{ _15007, _15009 };
                Interval _13147 = _15260;
                Interval _15278 = jet_add_derivative(_13146, _13147, intervalFailed);
                Interval _15308 = _15230.v;
                Interval _13128 = direction.x.v;
                Interval _13129 = _15308;
                Interval _15311 = imul(_13128, _13129, intervalFailed, optical_product_upper);
                Interval _13130 = direction.x.dx;
                Interval _13131 = _15308;
                Interval _15312 = jet_mul_derivative(_13130, _13131, intervalFailed, optical_product_upper);
                Interval _13132 = _15312;
                Interval _13133 = direction.x.v;
                Interval _13134 = _15230.dx;
                Interval _15313 = jet_mul_derivative(_13133, _13134, intervalFailed, optical_product_upper);
                Interval _13135 = _15313;
                Interval _15314 = jet_add_derivative(_13132, _13135, intervalFailed);
                Interval _13136 = direction.x.dy;
                Interval _13137 = _15308;
                Interval _15315 = jet_mul_derivative(_13136, _13137, intervalFailed, optical_product_upper);
                Interval _13138 = _15315;
                Interval _13139 = direction.x.v;
                Interval _13140 = _15230.dy;
                Interval _15316 = jet_mul_derivative(_13139, _13140, intervalFailed, optical_product_upper);
                Interval _13141 = _15316;
                Interval _15317 = jet_add_derivative(_13138, _13141, intervalFailed);
                Interval _15321 = _15230.v;
                Interval _13114 = direction.y.v;
                Interval _13115 = _15321;
                Interval _15324 = imul(_13114, _13115, intervalFailed, optical_product_upper);
                Interval _13116 = direction.y.dx;
                Interval _13117 = _15321;
                Interval _15325 = jet_mul_derivative(_13116, _13117, intervalFailed, optical_product_upper);
                Interval _13118 = _15325;
                Interval _13119 = direction.y.v;
                Interval _13120 = _15230.dx;
                Interval _15326 = jet_mul_derivative(_13119, _13120, intervalFailed, optical_product_upper);
                Interval _13121 = _15326;
                Interval _15327 = jet_add_derivative(_13118, _13121, intervalFailed);
                Interval _13122 = direction.y.dy;
                Interval _13123 = _15321;
                Interval _15328 = jet_mul_derivative(_13122, _13123, intervalFailed, optical_product_upper);
                Interval _13124 = _15328;
                Interval _13125 = direction.y.v;
                Interval _13126 = _15230.dy;
                Interval _15329 = jet_mul_derivative(_13125, _13126, intervalFailed, optical_product_upper);
                Interval _13127 = _15329;
                Interval _15330 = jet_add_derivative(_13124, _13127, intervalFailed);
                Interval _15334 = _15230.v;
                Interval _13100 = direction.z.v;
                Interval _13101 = _15334;
                Interval _15337 = imul(_13100, _13101, intervalFailed, optical_product_upper);
                Interval _13102 = direction.z.dx;
                Interval _13103 = _15334;
                Interval _15338 = jet_mul_derivative(_13102, _13103, intervalFailed, optical_product_upper);
                Interval _13104 = _15338;
                Interval _13105 = direction.z.v;
                Interval _13106 = _15230.dx;
                Interval _15339 = jet_mul_derivative(_13105, _13106, intervalFailed, optical_product_upper);
                Interval _13107 = _15339;
                Interval _15340 = jet_add_derivative(_13104, _13107, intervalFailed);
                Interval _13108 = direction.z.dy;
                Interval _13109 = _15334;
                Interval _15341 = jet_mul_derivative(_13108, _13109, intervalFailed, optical_product_upper);
                Interval _13110 = _15341;
                Interval _13111 = direction.z.v;
                Interval _13112 = _15230.dy;
                Interval _15342 = jet_mul_derivative(_13111, _13112, intervalFailed, optical_product_upper);
                Interval _13113 = _15342;
                Interval _15343 = jet_add_derivative(_13110, _13113, intervalFailed);
                Interval _13094 = hit.x.v;
                Interval _13095 = _15311;
                Interval _15347 = iadd(_13094, _13095, intervalFailed);
                Interval _13096 = hit.x.dx;
                Interval _13097 = _15314;
                Interval _15348 = jet_add_derivative(_13096, _13097, intervalFailed);
                Interval _13098 = hit.x.dy;
                Interval _13099 = _15317;
                Interval _15349 = jet_add_derivative(_13098, _13099, intervalFailed);
                Interval _13088 = hit.y.v;
                Interval _13089 = _15324;
                Interval _15353 = iadd(_13088, _13089, intervalFailed);
                Interval _13090 = hit.y.dx;
                Interval _13091 = _15327;
                Interval _15354 = jet_add_derivative(_13090, _13091, intervalFailed);
                Interval _13092 = hit.y.dy;
                Interval _13093 = _15330;
                Interval _15355 = jet_add_derivative(_13092, _13093, intervalFailed);
                Interval _13082 = hit.z.v;
                Interval _13083 = _15337;
                Interval _15359 = iadd(_13082, _13083, intervalFailed);
                Interval _13084 = hit.z.dx;
                Interval _13085 = _15340;
                Interval _15360 = jet_add_derivative(_13084, _13085, intervalFailed);
                Interval _13086 = hit.z.dy;
                Interval _13087 = _15343;
                Interval _15361 = jet_add_derivative(_13086, _13087, intervalFailed);
                hit = OpticalJet3{ OpticalJet{ _15347, _15348, _15349 }, OpticalJet{ _15353, _15354, _15355 }, OpticalJet{ _15359, _15360, _15361 } };
                _14988 = _15268.lo;
                _14990 = _15268.hi;
                _14992 = _15270.lo;
                _14994 = _15270.hi;
                _14996 = _15272.lo;
                _14998 = _15272.hi;
                _15000 = _15274.lo;
                _15002 = _15274.hi;
                _15004 = _15276.lo;
                _15006 = _15276.hi;
                _15008 = _15278.lo;
                _15010 = _15278.hi;
                _15012 = _15262.lo;
                _15014 = _15262.hi;
                _15016 = _15264.lo;
                _15018 = _15264.hi;
                _15020 = _15266.lo;
                _15022 = _15266.hi;
                _15024 = _15023;
                _15026 = _15025;
                _15028 = _15027;
                _15030 = _15029;
                _15032 = _15031;
                _15034 = _15033;
                _15036 = _15035;
                _15038 = _15037;
                _15040 = _15039;
                _15042 = _15041;
                _15044 = _15043;
                _15046 = _15045;
            }
            else
            {
                _14988 = _14987;
                _14990 = _14989;
                _14992 = _14991;
                _14994 = _14993;
                _14996 = _14995;
                _14998 = _14997;
                _15000 = _14999;
                _15002 = _15001;
                _15004 = _15003;
                _15006 = _15005;
                _15008 = _15007;
                _15010 = _15009;
                _15012 = _15011;
                _15014 = _15013;
                _15016 = _15015;
                _15018 = _15017;
                _15020 = _15019;
                _15022 = _15021;
                _15024 = _15060.z.v.lo;
                _15026 = _15060.z.v.hi;
                _15028 = _15060.z.dx.lo;
                _15030 = _15060.z.dx.hi;
                _15032 = _15060.z.dy.lo;
                _15034 = _15060.z.dy.hi;
                _15036 = _15060.y.v.lo;
                _15038 = _15060.y.v.hi;
                _15040 = _15060.y.dx.lo;
                _15042 = _15060.y.dx.hi;
                _15044 = _15060.y.dy.lo;
                _15046 = _15060.y.dy.hi;
            }
        }
        _15366 = _15023;
        _15367 = _15025;
        _15368 = _15027;
        _15369 = _15029;
        _15370 = _15031;
        _15371 = _15033;
        _15372 = _15035;
        _15373 = _15037;
        _15374 = _15039;
        _15375 = _15041;
        _15376 = _15043;
        _15377 = _15045;
    }
    else
    {
        float _13080 = 18.0;
        float _13081 = 1000.0;
        Interval _13779 = iratio(_13080, _13081, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13784;
        if (!intervalFailed)
        {
            _13784 = intervalFailed;
        }
        else
        {
            _13784 = false;
        }
        bool _13789;
        if (_13784)
        {
            _13789 = jetFailureSite == 0u;
        }
        else
        {
            _13789 = false;
        }
        if (_13789)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
        }
        Interval _13066 = _13742;
        Interval _13067 = _13779;
        Interval _13792 = imul(_13066, _13067, intervalFailed, optical_product_upper);
        Interval _13068 = _13743;
        Interval _13069 = _13779;
        Interval _13793 = jet_mul_derivative(_13068, _13069, intervalFailed, optical_product_upper);
        Interval _13070 = _13793;
        Interval _13071 = _13742;
        Interval _13072 = Interval{ 0.0, 0.0 };
        Interval _13794 = jet_mul_derivative(_13071, _13072, intervalFailed, optical_product_upper);
        Interval _13073 = _13794;
        Interval _13795 = jet_add_derivative(_13070, _13073, intervalFailed);
        Interval _13074 = _13744;
        Interval _13075 = _13779;
        Interval _13796 = jet_mul_derivative(_13074, _13075, intervalFailed, optical_product_upper);
        Interval _13076 = _13796;
        Interval _13077 = _13742;
        Interval _13078 = Interval{ 0.0, 0.0 };
        Interval _13797 = jet_mul_derivative(_13077, _13078, intervalFailed, optical_product_upper);
        Interval _13079 = _13797;
        Interval _13798 = jet_add_derivative(_13076, _13079, intervalFailed);
        float _13064 = 11.0;
        float _13065 = 1000.0;
        Interval _13800 = iratio(_13064, _13065, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13805;
        if (!intervalFailed)
        {
            _13805 = intervalFailed;
        }
        else
        {
            _13805 = false;
        }
        bool _13810;
        if (_13805)
        {
            _13810 = jetFailureSite == 0u;
        }
        else
        {
            _13810 = false;
        }
        if (_13810)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
        }
        Interval _13050 = _13750;
        Interval _13051 = _13800;
        Interval _13813 = imul(_13050, _13051, intervalFailed, optical_product_upper);
        Interval _13052 = _13751;
        Interval _13053 = _13800;
        Interval _13814 = jet_mul_derivative(_13052, _13053, intervalFailed, optical_product_upper);
        Interval _13054 = _13814;
        Interval _13055 = _13750;
        Interval _13056 = Interval{ 0.0, 0.0 };
        Interval _13815 = jet_mul_derivative(_13055, _13056, intervalFailed, optical_product_upper);
        Interval _13057 = _13815;
        Interval _13816 = jet_add_derivative(_13054, _13057, intervalFailed);
        Interval _13058 = _13752;
        Interval _13059 = _13800;
        Interval _13817 = jet_mul_derivative(_13058, _13059, intervalFailed, optical_product_upper);
        Interval _13060 = _13817;
        Interval _13061 = _13750;
        Interval _13062 = Interval{ 0.0, 0.0 };
        Interval _13818 = jet_mul_derivative(_13061, _13062, intervalFailed, optical_product_upper);
        Interval _13063 = _13818;
        Interval _13819 = jet_add_derivative(_13060, _13063, intervalFailed);
        Interval _13044 = _13792;
        Interval _13045 = _13813;
        Interval _13820 = iadd(_13044, _13045, intervalFailed);
        Interval _13046 = _13795;
        Interval _13047 = _13816;
        Interval _13821 = jet_add_derivative(_13046, _13047, intervalFailed);
        Interval _13048 = _13798;
        Interval _13049 = _13819;
        Interval _13822 = jet_add_derivative(_13048, _13049, intervalFailed);
        float _13042 = 8.0;
        float _13043 = 10.0;
        Interval _13824 = iratio(_13042, _13043, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13829;
        if (!intervalFailed)
        {
            _13829 = intervalFailed;
        }
        else
        {
            _13829 = false;
        }
        bool _13834;
        if (_13829)
        {
            _13834 = jetFailureSite == 0u;
        }
        else
        {
            _13834 = false;
        }
        if (_13834)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(8.0, 8.0, 10.0, 10.0);
        }
        Interval _13028 = Interval{ f.settings.x, f.settings.x };
        Interval _13029 = _13824;
        Interval _13838 = imul(_13028, _13029, intervalFailed, optical_product_upper);
        Interval _13030 = Interval{ 0.0, 0.0 };
        Interval _13031 = _13824;
        Interval _13839 = jet_mul_derivative(_13030, _13031, intervalFailed, optical_product_upper);
        Interval _13032 = _13839;
        Interval _13033 = Interval{ f.settings.x, f.settings.x };
        Interval _13034 = Interval{ 0.0, 0.0 };
        Interval _13841 = jet_mul_derivative(_13033, _13034, intervalFailed, optical_product_upper);
        Interval _13035 = _13841;
        Interval _13842 = jet_add_derivative(_13032, _13035, intervalFailed);
        Interval _13036 = Interval{ 0.0, 0.0 };
        Interval _13037 = _13824;
        Interval _13843 = jet_mul_derivative(_13036, _13037, intervalFailed, optical_product_upper);
        Interval _13038 = _13843;
        Interval _13039 = Interval{ f.settings.x, f.settings.x };
        Interval _13040 = Interval{ 0.0, 0.0 };
        Interval _13845 = jet_mul_derivative(_13039, _13040, intervalFailed, optical_product_upper);
        Interval _13041 = _13845;
        Interval _13846 = jet_add_derivative(_13038, _13041, intervalFailed);
        Interval _13022 = _13820;
        Interval _13023 = Interval{ as_type<float>(as_type<uint>(_13838.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13838.lo) ^ 2147483648u) };
        Interval _13872 = iadd(_13022, _13023, intervalFailed);
        Interval _13024 = _13821;
        Interval _13025 = Interval{ as_type<float>(as_type<uint>(_13842.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13842.lo) ^ 2147483648u) };
        Interval _13874 = jet_add_derivative(_13024, _13025, intervalFailed);
        Interval _13026 = _13822;
        Interval _13027 = Interval{ as_type<float>(as_type<uint>(_13846.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13846.lo) ^ 2147483648u) };
        Interval _13876 = jet_add_derivative(_13026, _13027, intervalFailed);
        float _13020 = 47.0;
        float _13021 = 1000.0;
        Interval _13878 = iratio(_13020, _13021, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13883;
        if (!intervalFailed)
        {
            _13883 = intervalFailed;
        }
        else
        {
            _13883 = false;
        }
        bool _13888;
        if (_13883)
        {
            _13888 = jetFailureSite == 0u;
        }
        else
        {
            _13888 = false;
        }
        if (_13888)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
        }
        Interval _13006 = _13742;
        Interval _13007 = _13878;
        Interval _13891 = imul(_13006, _13007, intervalFailed, optical_product_upper);
        Interval _13008 = _13743;
        Interval _13009 = _13878;
        Interval _13892 = jet_mul_derivative(_13008, _13009, intervalFailed, optical_product_upper);
        Interval _13010 = _13892;
        Interval _13011 = _13742;
        Interval _13012 = Interval{ 0.0, 0.0 };
        Interval _13893 = jet_mul_derivative(_13011, _13012, intervalFailed, optical_product_upper);
        Interval _13013 = _13893;
        Interval _13894 = jet_add_derivative(_13010, _13013, intervalFailed);
        Interval _13014 = _13744;
        Interval _13015 = _13878;
        Interval _13895 = jet_mul_derivative(_13014, _13015, intervalFailed, optical_product_upper);
        Interval _13016 = _13895;
        Interval _13017 = _13742;
        Interval _13018 = Interval{ 0.0, 0.0 };
        Interval _13896 = jet_mul_derivative(_13017, _13018, intervalFailed, optical_product_upper);
        Interval _13019 = _13896;
        Interval _13897 = jet_add_derivative(_13016, _13019, intervalFailed);
        float _13004 = 25.0;
        float _13005 = 1000.0;
        Interval _13899 = iratio(_13004, _13005, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13904;
        if (!intervalFailed)
        {
            _13904 = intervalFailed;
        }
        else
        {
            _13904 = false;
        }
        bool _13909;
        if (_13904)
        {
            _13909 = jetFailureSite == 0u;
        }
        else
        {
            _13909 = false;
        }
        if (_13909)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        Interval _12990 = _13750;
        Interval _12991 = _13899;
        Interval _13912 = imul(_12990, _12991, intervalFailed, optical_product_upper);
        Interval _12992 = _13751;
        Interval _12993 = _13899;
        Interval _13913 = jet_mul_derivative(_12992, _12993, intervalFailed, optical_product_upper);
        Interval _12994 = _13913;
        Interval _12995 = _13750;
        Interval _12996 = Interval{ 0.0, 0.0 };
        Interval _13914 = jet_mul_derivative(_12995, _12996, intervalFailed, optical_product_upper);
        Interval _12997 = _13914;
        Interval _13915 = jet_add_derivative(_12994, _12997, intervalFailed);
        Interval _12998 = _13752;
        Interval _12999 = _13899;
        Interval _13916 = jet_mul_derivative(_12998, _12999, intervalFailed, optical_product_upper);
        Interval _13000 = _13916;
        Interval _13001 = _13750;
        Interval _13002 = Interval{ 0.0, 0.0 };
        Interval _13917 = jet_mul_derivative(_13001, _13002, intervalFailed, optical_product_upper);
        Interval _13003 = _13917;
        Interval _13918 = jet_add_derivative(_13000, _13003, intervalFailed);
        Interval _12984 = _13891;
        Interval _12985 = Interval{ as_type<float>(as_type<uint>(_13912.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13912.lo) ^ 2147483648u) };
        Interval _13944 = iadd(_12984, _12985, intervalFailed);
        Interval _12986 = _13894;
        Interval _12987 = Interval{ as_type<float>(as_type<uint>(_13915.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13915.lo) ^ 2147483648u) };
        Interval _13946 = jet_add_derivative(_12986, _12987, intervalFailed);
        Interval _12988 = _13897;
        Interval _12989 = Interval{ as_type<float>(as_type<uint>(_13918.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13918.lo) ^ 2147483648u) };
        Interval _13948 = jet_add_derivative(_12988, _12989, intervalFailed);
        float _12982 = 12.0;
        float _12983 = 10.0;
        Interval _13950 = iratio(_12982, _12983, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13955;
        if (!intervalFailed)
        {
            _13955 = intervalFailed;
        }
        else
        {
            _13955 = false;
        }
        bool _13960;
        if (_13955)
        {
            _13960 = jetFailureSite == 0u;
        }
        else
        {
            _13960 = false;
        }
        if (_13960)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(12.0, 12.0, 10.0, 10.0);
        }
        Interval _12968 = Interval{ f.settings.x, f.settings.x };
        Interval _12969 = _13950;
        Interval _13964 = imul(_12968, _12969, intervalFailed, optical_product_upper);
        Interval _12970 = Interval{ 0.0, 0.0 };
        Interval _12971 = _13950;
        Interval _13965 = jet_mul_derivative(_12970, _12971, intervalFailed, optical_product_upper);
        Interval _12972 = _13965;
        Interval _12973 = Interval{ f.settings.x, f.settings.x };
        Interval _12974 = Interval{ 0.0, 0.0 };
        Interval _13967 = jet_mul_derivative(_12973, _12974, intervalFailed, optical_product_upper);
        Interval _12975 = _13967;
        Interval _13968 = jet_add_derivative(_12972, _12975, intervalFailed);
        Interval _12976 = Interval{ 0.0, 0.0 };
        Interval _12977 = _13950;
        Interval _13969 = jet_mul_derivative(_12976, _12977, intervalFailed, optical_product_upper);
        Interval _12978 = _13969;
        Interval _12979 = Interval{ f.settings.x, f.settings.x };
        Interval _12980 = Interval{ 0.0, 0.0 };
        Interval _13971 = jet_mul_derivative(_12979, _12980, intervalFailed, optical_product_upper);
        Interval _12981 = _13971;
        Interval _13972 = jet_add_derivative(_12978, _12981, intervalFailed);
        Interval _12962 = _13944;
        Interval _12963 = _13964;
        Interval _13973 = iadd(_12962, _12963, intervalFailed);
        Interval _12964 = _13946;
        Interval _12965 = _13968;
        Interval _13974 = jet_add_derivative(_12964, _12965, intervalFailed);
        Interval _12966 = _13948;
        Interval _12967 = _13972;
        Interval _13975 = jet_add_derivative(_12966, _12967, intervalFailed);
        float _12960 = 22.0;
        float _12961 = 1000.0;
        Interval _13977 = iratio(_12960, _12961, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13982;
        if (!intervalFailed)
        {
            _13982 = intervalFailed;
        }
        else
        {
            _13982 = false;
        }
        bool _13987;
        if (_13982)
        {
            _13987 = jetFailureSite == 0u;
        }
        else
        {
            _13987 = false;
        }
        if (_13987)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12946 = _13750;
        Interval _12947 = _13977;
        Interval _13990 = imul(_12946, _12947, intervalFailed, optical_product_upper);
        Interval _12948 = _13751;
        Interval _12949 = _13977;
        Interval _13991 = jet_mul_derivative(_12948, _12949, intervalFailed, optical_product_upper);
        Interval _12950 = _13991;
        Interval _12951 = _13750;
        Interval _12952 = Interval{ 0.0, 0.0 };
        Interval _13992 = jet_mul_derivative(_12951, _12952, intervalFailed, optical_product_upper);
        Interval _12953 = _13992;
        Interval _13993 = jet_add_derivative(_12950, _12953, intervalFailed);
        Interval _12954 = _13752;
        Interval _12955 = _13977;
        Interval _13994 = jet_mul_derivative(_12954, _12955, intervalFailed, optical_product_upper);
        Interval _12956 = _13994;
        Interval _12957 = _13750;
        Interval _12958 = Interval{ 0.0, 0.0 };
        Interval _13995 = jet_mul_derivative(_12957, _12958, intervalFailed, optical_product_upper);
        Interval _12959 = _13995;
        Interval _13996 = jet_add_derivative(_12956, _12959, intervalFailed);
        float _12944 = 9.0;
        float _12945 = 1000.0;
        Interval _13998 = iratio(_12944, _12945, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14003;
        if (!intervalFailed)
        {
            _14003 = intervalFailed;
        }
        else
        {
            _14003 = false;
        }
        bool _14008;
        if (_14003)
        {
            _14008 = jetFailureSite == 0u;
        }
        else
        {
            _14008 = false;
        }
        if (_14008)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
        }
        Interval _12930 = _13742;
        Interval _12931 = _13998;
        Interval _14011 = imul(_12930, _12931, intervalFailed, optical_product_upper);
        Interval _12932 = _13743;
        Interval _12933 = _13998;
        Interval _14012 = jet_mul_derivative(_12932, _12933, intervalFailed, optical_product_upper);
        Interval _12934 = _14012;
        Interval _12935 = _13742;
        Interval _12936 = Interval{ 0.0, 0.0 };
        Interval _14013 = jet_mul_derivative(_12935, _12936, intervalFailed, optical_product_upper);
        Interval _12937 = _14013;
        Interval _14014 = jet_add_derivative(_12934, _12937, intervalFailed);
        Interval _12938 = _13744;
        Interval _12939 = _13998;
        Interval _14015 = jet_mul_derivative(_12938, _12939, intervalFailed, optical_product_upper);
        Interval _12940 = _14015;
        Interval _12941 = _13742;
        Interval _12942 = Interval{ 0.0, 0.0 };
        Interval _14016 = jet_mul_derivative(_12941, _12942, intervalFailed, optical_product_upper);
        Interval _12943 = _14016;
        Interval _14017 = jet_add_derivative(_12940, _12943, intervalFailed);
        Interval _12924 = _13990;
        Interval _12925 = Interval{ as_type<float>(as_type<uint>(_14011.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14011.lo) ^ 2147483648u) };
        Interval _14043 = iadd(_12924, _12925, intervalFailed);
        Interval _12926 = _13993;
        Interval _12927 = Interval{ as_type<float>(as_type<uint>(_14014.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14014.lo) ^ 2147483648u) };
        Interval _14045 = jet_add_derivative(_12926, _12927, intervalFailed);
        Interval _12928 = _13996;
        Interval _12929 = Interval{ as_type<float>(as_type<uint>(_14017.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14017.lo) ^ 2147483648u) };
        Interval _14047 = jet_add_derivative(_12928, _12929, intervalFailed);
        float _12922 = 65.0;
        float _12923 = 100.0;
        Interval _14049 = iratio(_12922, _12923, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14054;
        if (!intervalFailed)
        {
            _14054 = intervalFailed;
        }
        else
        {
            _14054 = false;
        }
        bool _14059;
        if (_14054)
        {
            _14059 = jetFailureSite == 0u;
        }
        else
        {
            _14059 = false;
        }
        if (_14059)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
        }
        Interval _12908 = Interval{ f.settings.x, f.settings.x };
        Interval _12909 = _14049;
        Interval _14063 = imul(_12908, _12909, intervalFailed, optical_product_upper);
        Interval _12910 = Interval{ 0.0, 0.0 };
        Interval _12911 = _14049;
        Interval _14064 = jet_mul_derivative(_12910, _12911, intervalFailed, optical_product_upper);
        Interval _12912 = _14064;
        Interval _12913 = Interval{ f.settings.x, f.settings.x };
        Interval _12914 = Interval{ 0.0, 0.0 };
        Interval _14066 = jet_mul_derivative(_12913, _12914, intervalFailed, optical_product_upper);
        Interval _12915 = _14066;
        Interval _14067 = jet_add_derivative(_12912, _12915, intervalFailed);
        Interval _12916 = Interval{ 0.0, 0.0 };
        Interval _12917 = _14049;
        Interval _14068 = jet_mul_derivative(_12916, _12917, intervalFailed, optical_product_upper);
        Interval _12918 = _14068;
        Interval _12919 = Interval{ f.settings.x, f.settings.x };
        Interval _12920 = Interval{ 0.0, 0.0 };
        Interval _14070 = jet_mul_derivative(_12919, _12920, intervalFailed, optical_product_upper);
        Interval _12921 = _14070;
        Interval _14071 = jet_add_derivative(_12918, _12921, intervalFailed);
        Interval _12902 = _14043;
        Interval _12903 = Interval{ as_type<float>(as_type<uint>(_14063.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14063.lo) ^ 2147483648u) };
        Interval _14097 = iadd(_12902, _12903, intervalFailed);
        Interval _12904 = _14045;
        Interval _12905 = Interval{ as_type<float>(as_type<uint>(_14067.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14067.lo) ^ 2147483648u) };
        Interval _14099 = jet_add_derivative(_12904, _12905, intervalFailed);
        Interval _12906 = _14047;
        Interval _12907 = Interval{ as_type<float>(as_type<uint>(_14071.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14071.lo) ^ 2147483648u) };
        Interval _14101 = jet_add_derivative(_12906, _12907, intervalFailed);
        float _12900 = 55.0;
        float _12901 = 1000.0;
        Interval _14103 = iratio(_12900, _12901, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14108;
        if (!intervalFailed)
        {
            _14108 = intervalFailed;
        }
        else
        {
            _14108 = false;
        }
        bool _14113;
        if (_14108)
        {
            _14113 = jetFailureSite == 0u;
        }
        else
        {
            _14113 = false;
        }
        if (_14113)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
        }
        float _12894 = _13872.lo;
        float _12895 = _13872.hi;
        float _14118 = sine_bounds(_12894, _12895, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14122 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14125 = as_type<float>(as_type<uint>(_14118) ^ 2147483648u);
        Interval _12890 = _13872;
        float _12888 = 3.1415927410125732421875;
        float _14126 = interval_down(_12888, intervalFailed);
        float _12889 = 3.1415927410125732421875;
        float _14127 = interval_up(_12889, intervalFailed);
        Interval _12891 = Interval{ _14126, _14127 };
        Interval _12892 = Interval{ 0.5, 0.5 };
        Interval _14129 = imul(_12891, _12892, intervalFailed, optical_product_upper);
        Interval _12893 = _14129;
        Interval _14130 = iadd(_12890, _12893, intervalFailed);
        float _12886 = _14130.lo;
        float _12887 = _14130.hi;
        float _14133 = sine_bounds(_12886, _12887, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12896 = Interval{ _14122, _14125 };
        Interval _12897 = _13874;
        Interval _14136 = jet_mul_derivative(_12896, _12897, intervalFailed, optical_product_upper);
        Interval _12898 = Interval{ _14122, _14125 };
        Interval _12899 = _13876;
        Interval _14138 = jet_mul_derivative(_12898, _12899, intervalFailed, optical_product_upper);
        Interval _12872 = _14103;
        Interval _12873 = Interval{ _14133, interval_sine_upper };
        Interval _14140 = imul(_12872, _12873, intervalFailed, optical_product_upper);
        Interval _12874 = Interval{ 0.0, 0.0 };
        Interval _12875 = Interval{ _14133, interval_sine_upper };
        Interval _14142 = jet_mul_derivative(_12874, _12875, intervalFailed, optical_product_upper);
        Interval _12876 = _14142;
        Interval _12877 = _14103;
        Interval _12878 = _14136;
        Interval _14143 = jet_mul_derivative(_12877, _12878, intervalFailed, optical_product_upper);
        Interval _12879 = _14143;
        Interval _14144 = jet_add_derivative(_12876, _12879, intervalFailed);
        Interval _12880 = Interval{ 0.0, 0.0 };
        Interval _12881 = Interval{ _14133, interval_sine_upper };
        Interval _14146 = jet_mul_derivative(_12880, _12881, intervalFailed, optical_product_upper);
        Interval _12882 = _14146;
        Interval _12883 = _14103;
        Interval _12884 = _14138;
        Interval _14147 = jet_mul_derivative(_12883, _12884, intervalFailed, optical_product_upper);
        Interval _12885 = _14147;
        Interval _14148 = jet_add_derivative(_12882, _12885, intervalFailed);
        float _12870 = 22.0;
        float _12871 = 1000.0;
        Interval _14151 = iratio(_12870, _12871, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14156;
        if (!intervalFailed)
        {
            _14156 = intervalFailed;
        }
        else
        {
            _14156 = false;
        }
        bool _14161;
        if (_14156)
        {
            _14161 = jetFailureSite == 0u;
        }
        else
        {
            _14161 = false;
        }
        if (_14161)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12856 = footprint.v;
        Interval _12857 = _14151;
        Interval _14167 = imul(_12856, _12857, intervalFailed, optical_product_upper);
        Interval _12858 = footprint.dx;
        Interval _12859 = _14151;
        Interval _14168 = jet_mul_derivative(_12858, _12859, intervalFailed, optical_product_upper);
        Interval _12860 = _14168;
        Interval _12861 = footprint.v;
        Interval _12862 = Interval{ 0.0, 0.0 };
        Interval _14169 = jet_mul_derivative(_12861, _12862, intervalFailed, optical_product_upper);
        Interval _12863 = _14169;
        Interval _14170 = jet_add_derivative(_12860, _12863, intervalFailed);
        Interval _12864 = footprint.dy;
        Interval _12865 = _14151;
        Interval _14171 = jet_mul_derivative(_12864, _12865, intervalFailed, optical_product_upper);
        Interval _12866 = _14171;
        Interval _12867 = footprint.v;
        Interval _12868 = Interval{ 0.0, 0.0 };
        Interval _14172 = jet_mul_derivative(_12867, _12868, intervalFailed, optical_product_upper);
        Interval _12869 = _14172;
        Interval _14173 = jet_add_derivative(_12866, _12869, intervalFailed);
        bool _14180;
        if (_14167.lo <= 0.0)
        {
            _14180 = _14167.hi >= 0.0;
        }
        else
        {
            _14180 = false;
        }
        float _14187;
        if (_14180)
        {
            _14187 = 0.0;
        }
        else
        {
            _14187 = precise::min(abs(_14167.lo), abs(_14167.hi));
        }
        float _14190 = precise::max(abs(_14167.lo), abs(_14167.hi));
        float _12846 = spvFMul(_14187, _14187);
        float _14191 = interval_down(_12846, intervalFailed);
        float _14192 = precise::max(0.0, _14191);
        float _12847 = spvFMul(_14190, _14190);
        float _14193 = interval_up(_12847, intervalFailed);
        Interval _12848 = Interval{ 2.0, 2.0 };
        Interval _12849 = _14167;
        Interval _14194 = imul(_12848, _12849, intervalFailed, optical_product_upper);
        Interval _12850 = _14194;
        Interval _12851 = _14170;
        Interval _14195 = jet_mul_derivative(_12850, _12851, intervalFailed, optical_product_upper);
        Interval _12852 = Interval{ 2.0, 2.0 };
        Interval _12853 = _14167;
        Interval _14196 = imul(_12852, _12853, intervalFailed, optical_product_upper);
        Interval _12854 = _14196;
        Interval _12855 = _14173;
        Interval _14197 = jet_mul_derivative(_12854, _12855, intervalFailed, optical_product_upper);
        bool _14202;
        if (_14192 <= 0.0)
        {
            _14202 = _14193 >= 0.0;
        }
        else
        {
            _14202 = false;
        }
        float _14209;
        if (_14202)
        {
            _14209 = 0.0;
        }
        else
        {
            _14209 = precise::min(abs(_14192), abs(_14193));
        }
        float _14212 = precise::max(abs(_14192), abs(_14193));
        float _12836 = spvFMul(_14209, _14209);
        float _14213 = interval_down(_12836, intervalFailed);
        float _12837 = spvFMul(_14212, _14212);
        float _14215 = interval_up(_12837, intervalFailed);
        Interval _12838 = Interval{ 2.0, 2.0 };
        Interval _12839 = Interval{ _14192, _14193 };
        Interval _14217 = imul(_12838, _12839, intervalFailed, optical_product_upper);
        Interval _12840 = _14217;
        Interval _12841 = _14195;
        Interval _14218 = jet_mul_derivative(_12840, _12841, intervalFailed, optical_product_upper);
        Interval _12842 = Interval{ 2.0, 2.0 };
        Interval _12843 = Interval{ _14192, _14193 };
        Interval _14220 = imul(_12842, _12843, intervalFailed, optical_product_upper);
        Interval _12844 = _14220;
        Interval _12845 = _14197;
        Interval _14221 = jet_mul_derivative(_12844, _12845, intervalFailed, optical_product_upper);
        Interval _12830 = Interval{ 1.0, 1.0 };
        Interval _12831 = Interval{ precise::max(0.0, _14213), _14215 };
        Interval _14223 = iadd(_12830, _12831, intervalFailed);
        Interval _12832 = Interval{ 0.0, 0.0 };
        Interval _12833 = _14218;
        Interval _14224 = jet_add_derivative(_12832, _12833, intervalFailed);
        Interval _12834 = Interval{ 0.0, 0.0 };
        Interval _12835 = _14221;
        Interval _14225 = jet_add_derivative(_12834, _12835, intervalFailed);
        Interval _12816 = Interval{ 1.0, 1.0 };
        Interval _12817 = _14223;
        Interval _14227 = idiv(_12816, _12817, intervalFailed, interval_divide_upper);
        bool _14234;
        if (!intervalFailed)
        {
            _14234 = intervalFailed;
        }
        else
        {
            _14234 = false;
        }
        bool _14239;
        if (_14234)
        {
            _14239 = jetFailureSite == 0u;
        }
        else
        {
            _14239 = false;
        }
        if (_14239)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14223.lo, _14223.hi);
        }
        Interval _12818 = Interval{ 0.0, 0.0 };
        Interval _12819 = _14227;
        Interval _12820 = _14224;
        Interval _14243 = jet_mul_derivative(_12819, _12820, intervalFailed, optical_product_upper);
        Interval _12821 = Interval{ as_type<float>(as_type<uint>(_14243.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14243.lo) ^ 2147483648u) };
        Interval _14253 = jet_add_derivative(_12818, _12821, intervalFailed);
        Interval _12822 = _14253;
        Interval _12823 = _14223;
        Interval _14254 = jet_div_derivative(_12822, _12823, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12824 = Interval{ 0.0, 0.0 };
        Interval _12825 = _14227;
        Interval _12826 = _14225;
        Interval _14255 = jet_mul_derivative(_12825, _12826, intervalFailed, optical_product_upper);
        Interval _12827 = Interval{ as_type<float>(as_type<uint>(_14255.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14255.lo) ^ 2147483648u) };
        Interval _14265 = jet_add_derivative(_12824, _12827, intervalFailed);
        Interval _12828 = _14265;
        Interval _12829 = _14223;
        Interval _14266 = jet_div_derivative(_12828, _12829, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12802 = _14140;
        Interval _12803 = _14227;
        Interval _14267 = imul(_12802, _12803, intervalFailed, optical_product_upper);
        Interval _12804 = _14144;
        Interval _12805 = _14227;
        Interval _14268 = jet_mul_derivative(_12804, _12805, intervalFailed, optical_product_upper);
        Interval _12806 = _14268;
        Interval _12807 = _14140;
        Interval _12808 = _14254;
        Interval _14269 = jet_mul_derivative(_12807, _12808, intervalFailed, optical_product_upper);
        Interval _12809 = _14269;
        Interval _14270 = jet_add_derivative(_12806, _12809, intervalFailed);
        Interval _12810 = _14148;
        Interval _12811 = _14227;
        Interval _14271 = jet_mul_derivative(_12810, _12811, intervalFailed, optical_product_upper);
        Interval _12812 = _14271;
        Interval _12813 = _14140;
        Interval _12814 = _14266;
        Interval _14272 = jet_mul_derivative(_12813, _12814, intervalFailed, optical_product_upper);
        Interval _12815 = _14272;
        Interval _14273 = jet_add_derivative(_12812, _12815, intervalFailed);
        float _12800 = 25.0;
        float _12801 = 1000.0;
        Interval _14275 = iratio(_12800, _12801, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14280;
        if (!intervalFailed)
        {
            _14280 = intervalFailed;
        }
        else
        {
            _14280 = false;
        }
        bool _14285;
        if (_14280)
        {
            _14285 = jetFailureSite == 0u;
        }
        else
        {
            _14285 = false;
        }
        if (_14285)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        float _12794 = _13973.lo;
        float _12795 = _13973.hi;
        float _14290 = sine_bounds(_12794, _12795, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14294 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14297 = as_type<float>(as_type<uint>(_14290) ^ 2147483648u);
        Interval _12790 = _13973;
        float _12788 = 3.1415927410125732421875;
        float _14298 = interval_down(_12788, intervalFailed);
        float _12789 = 3.1415927410125732421875;
        float _14299 = interval_up(_12789, intervalFailed);
        Interval _12791 = Interval{ _14298, _14299 };
        Interval _12792 = Interval{ 0.5, 0.5 };
        Interval _14301 = imul(_12791, _12792, intervalFailed, optical_product_upper);
        Interval _12793 = _14301;
        Interval _14302 = iadd(_12790, _12793, intervalFailed);
        float _12786 = _14302.lo;
        float _12787 = _14302.hi;
        float _14305 = sine_bounds(_12786, _12787, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12796 = Interval{ _14294, _14297 };
        Interval _12797 = _13974;
        Interval _14308 = jet_mul_derivative(_12796, _12797, intervalFailed, optical_product_upper);
        Interval _12798 = Interval{ _14294, _14297 };
        Interval _12799 = _13975;
        Interval _14310 = jet_mul_derivative(_12798, _12799, intervalFailed, optical_product_upper);
        Interval _12772 = _14275;
        Interval _12773 = Interval{ _14305, interval_sine_upper };
        Interval _14312 = imul(_12772, _12773, intervalFailed, optical_product_upper);
        Interval _12774 = Interval{ 0.0, 0.0 };
        Interval _12775 = Interval{ _14305, interval_sine_upper };
        Interval _14314 = jet_mul_derivative(_12774, _12775, intervalFailed, optical_product_upper);
        Interval _12776 = _14314;
        Interval _12777 = _14275;
        Interval _12778 = _14308;
        Interval _14315 = jet_mul_derivative(_12777, _12778, intervalFailed, optical_product_upper);
        Interval _12779 = _14315;
        Interval _14316 = jet_add_derivative(_12776, _12779, intervalFailed);
        Interval _12780 = Interval{ 0.0, 0.0 };
        Interval _12781 = Interval{ _14305, interval_sine_upper };
        Interval _14318 = jet_mul_derivative(_12780, _12781, intervalFailed, optical_product_upper);
        Interval _12782 = _14318;
        Interval _12783 = _14275;
        Interval _12784 = _14310;
        Interval _14319 = jet_mul_derivative(_12783, _12784, intervalFailed, optical_product_upper);
        Interval _12785 = _14319;
        Interval _14320 = jet_add_derivative(_12782, _12785, intervalFailed);
        float _12770 = 54.0;
        float _12771 = 1000.0;
        Interval _14323 = iratio(_12770, _12771, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14328;
        if (!intervalFailed)
        {
            _14328 = intervalFailed;
        }
        else
        {
            _14328 = false;
        }
        bool _14333;
        if (_14328)
        {
            _14333 = jetFailureSite == 0u;
        }
        else
        {
            _14333 = false;
        }
        if (_14333)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12756 = footprint.v;
        Interval _12757 = _14323;
        Interval _14339 = imul(_12756, _12757, intervalFailed, optical_product_upper);
        Interval _12758 = footprint.dx;
        Interval _12759 = _14323;
        Interval _14340 = jet_mul_derivative(_12758, _12759, intervalFailed, optical_product_upper);
        Interval _12760 = _14340;
        Interval _12761 = footprint.v;
        Interval _12762 = Interval{ 0.0, 0.0 };
        Interval _14341 = jet_mul_derivative(_12761, _12762, intervalFailed, optical_product_upper);
        Interval _12763 = _14341;
        Interval _14342 = jet_add_derivative(_12760, _12763, intervalFailed);
        Interval _12764 = footprint.dy;
        Interval _12765 = _14323;
        Interval _14343 = jet_mul_derivative(_12764, _12765, intervalFailed, optical_product_upper);
        Interval _12766 = _14343;
        Interval _12767 = footprint.v;
        Interval _12768 = Interval{ 0.0, 0.0 };
        Interval _14344 = jet_mul_derivative(_12767, _12768, intervalFailed, optical_product_upper);
        Interval _12769 = _14344;
        Interval _14345 = jet_add_derivative(_12766, _12769, intervalFailed);
        bool _14352;
        if (_14339.lo <= 0.0)
        {
            _14352 = _14339.hi >= 0.0;
        }
        else
        {
            _14352 = false;
        }
        float _14359;
        if (_14352)
        {
            _14359 = 0.0;
        }
        else
        {
            _14359 = precise::min(abs(_14339.lo), abs(_14339.hi));
        }
        float _14362 = precise::max(abs(_14339.lo), abs(_14339.hi));
        float _12746 = spvFMul(_14359, _14359);
        float _14363 = interval_down(_12746, intervalFailed);
        float _14364 = precise::max(0.0, _14363);
        float _12747 = spvFMul(_14362, _14362);
        float _14365 = interval_up(_12747, intervalFailed);
        Interval _12748 = Interval{ 2.0, 2.0 };
        Interval _12749 = _14339;
        Interval _14366 = imul(_12748, _12749, intervalFailed, optical_product_upper);
        Interval _12750 = _14366;
        Interval _12751 = _14342;
        Interval _14367 = jet_mul_derivative(_12750, _12751, intervalFailed, optical_product_upper);
        Interval _12752 = Interval{ 2.0, 2.0 };
        Interval _12753 = _14339;
        Interval _14368 = imul(_12752, _12753, intervalFailed, optical_product_upper);
        Interval _12754 = _14368;
        Interval _12755 = _14345;
        Interval _14369 = jet_mul_derivative(_12754, _12755, intervalFailed, optical_product_upper);
        bool _14374;
        if (_14364 <= 0.0)
        {
            _14374 = _14365 >= 0.0;
        }
        else
        {
            _14374 = false;
        }
        float _14381;
        if (_14374)
        {
            _14381 = 0.0;
        }
        else
        {
            _14381 = precise::min(abs(_14364), abs(_14365));
        }
        float _14384 = precise::max(abs(_14364), abs(_14365));
        float _12736 = spvFMul(_14381, _14381);
        float _14385 = interval_down(_12736, intervalFailed);
        float _12737 = spvFMul(_14384, _14384);
        float _14387 = interval_up(_12737, intervalFailed);
        Interval _12738 = Interval{ 2.0, 2.0 };
        Interval _12739 = Interval{ _14364, _14365 };
        Interval _14389 = imul(_12738, _12739, intervalFailed, optical_product_upper);
        Interval _12740 = _14389;
        Interval _12741 = _14367;
        Interval _14390 = jet_mul_derivative(_12740, _12741, intervalFailed, optical_product_upper);
        Interval _12742 = Interval{ 2.0, 2.0 };
        Interval _12743 = Interval{ _14364, _14365 };
        Interval _14392 = imul(_12742, _12743, intervalFailed, optical_product_upper);
        Interval _12744 = _14392;
        Interval _12745 = _14369;
        Interval _14393 = jet_mul_derivative(_12744, _12745, intervalFailed, optical_product_upper);
        Interval _12730 = Interval{ 1.0, 1.0 };
        Interval _12731 = Interval{ precise::max(0.0, _14385), _14387 };
        Interval _14395 = iadd(_12730, _12731, intervalFailed);
        Interval _12732 = Interval{ 0.0, 0.0 };
        Interval _12733 = _14390;
        Interval _14396 = jet_add_derivative(_12732, _12733, intervalFailed);
        Interval _12734 = Interval{ 0.0, 0.0 };
        Interval _12735 = _14393;
        Interval _14397 = jet_add_derivative(_12734, _12735, intervalFailed);
        Interval _12716 = Interval{ 1.0, 1.0 };
        Interval _12717 = _14395;
        Interval _14399 = idiv(_12716, _12717, intervalFailed, interval_divide_upper);
        bool _14406;
        if (!intervalFailed)
        {
            _14406 = intervalFailed;
        }
        else
        {
            _14406 = false;
        }
        bool _14411;
        if (_14406)
        {
            _14411 = jetFailureSite == 0u;
        }
        else
        {
            _14411 = false;
        }
        if (_14411)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14395.lo, _14395.hi);
        }
        Interval _12718 = Interval{ 0.0, 0.0 };
        Interval _12719 = _14399;
        Interval _12720 = _14396;
        Interval _14415 = jet_mul_derivative(_12719, _12720, intervalFailed, optical_product_upper);
        Interval _12721 = Interval{ as_type<float>(as_type<uint>(_14415.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14415.lo) ^ 2147483648u) };
        Interval _14425 = jet_add_derivative(_12718, _12721, intervalFailed);
        Interval _12722 = _14425;
        Interval _12723 = _14395;
        Interval _14426 = jet_div_derivative(_12722, _12723, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12724 = Interval{ 0.0, 0.0 };
        Interval _12725 = _14399;
        Interval _12726 = _14397;
        Interval _14427 = jet_mul_derivative(_12725, _12726, intervalFailed, optical_product_upper);
        Interval _12727 = Interval{ as_type<float>(as_type<uint>(_14427.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14427.lo) ^ 2147483648u) };
        Interval _14437 = jet_add_derivative(_12724, _12727, intervalFailed);
        Interval _12728 = _14437;
        Interval _12729 = _14395;
        Interval _14438 = jet_div_derivative(_12728, _12729, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12702 = _14312;
        Interval _12703 = _14399;
        Interval _14439 = imul(_12702, _12703, intervalFailed, optical_product_upper);
        Interval _12704 = _14316;
        Interval _12705 = _14399;
        Interval _14440 = jet_mul_derivative(_12704, _12705, intervalFailed, optical_product_upper);
        Interval _12706 = _14440;
        Interval _12707 = _14312;
        Interval _12708 = _14426;
        Interval _14441 = jet_mul_derivative(_12707, _12708, intervalFailed, optical_product_upper);
        Interval _12709 = _14441;
        Interval _14442 = jet_add_derivative(_12706, _12709, intervalFailed);
        Interval _12710 = _14320;
        Interval _12711 = _14399;
        Interval _14443 = jet_mul_derivative(_12710, _12711, intervalFailed, optical_product_upper);
        Interval _12712 = _14443;
        Interval _12713 = _14312;
        Interval _12714 = _14438;
        Interval _14444 = jet_mul_derivative(_12713, _12714, intervalFailed, optical_product_upper);
        Interval _12715 = _14444;
        Interval _14445 = jet_add_derivative(_12712, _12715, intervalFailed);
        Interval _12696 = _14267;
        Interval _12697 = _14439;
        Interval _14446 = iadd(_12696, _12697, intervalFailed);
        Interval _12698 = _14270;
        Interval _12699 = _14442;
        Interval _14447 = jet_add_derivative(_12698, _12699, intervalFailed);
        Interval _12700 = _14273;
        Interval _12701 = _14445;
        Interval _14448 = jet_add_derivative(_12700, _12701, intervalFailed);
        float _12694 = 45.0;
        float _12695 = 1000.0;
        Interval _14456 = iratio(_12694, _12695, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14461;
        if (!intervalFailed)
        {
            _14461 = intervalFailed;
        }
        else
        {
            _14461 = false;
        }
        bool _14466;
        if (_14461)
        {
            _14466 = jetFailureSite == 0u;
        }
        else
        {
            _14466 = false;
        }
        if (_14466)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
        }
        float _12688 = _14097.lo;
        float _12689 = _14097.hi;
        float _14471 = sine_bounds(_12688, _12689, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14475 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14478 = as_type<float>(as_type<uint>(_14471) ^ 2147483648u);
        Interval _12684 = _14097;
        float _12682 = 3.1415927410125732421875;
        float _14479 = interval_down(_12682, intervalFailed);
        float _12683 = 3.1415927410125732421875;
        float _14480 = interval_up(_12683, intervalFailed);
        Interval _12685 = Interval{ _14479, _14480 };
        Interval _12686 = Interval{ 0.5, 0.5 };
        Interval _14482 = imul(_12685, _12686, intervalFailed, optical_product_upper);
        Interval _12687 = _14482;
        Interval _14483 = iadd(_12684, _12687, intervalFailed);
        float _12680 = _14483.lo;
        float _12681 = _14483.hi;
        float _14486 = sine_bounds(_12680, _12681, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12690 = Interval{ _14475, _14478 };
        Interval _12691 = _14099;
        Interval _14489 = jet_mul_derivative(_12690, _12691, intervalFailed, optical_product_upper);
        Interval _12692 = Interval{ _14475, _14478 };
        Interval _12693 = _14101;
        Interval _14491 = jet_mul_derivative(_12692, _12693, intervalFailed, optical_product_upper);
        Interval _12666 = _14456;
        Interval _12667 = Interval{ _14486, interval_sine_upper };
        Interval _14493 = imul(_12666, _12667, intervalFailed, optical_product_upper);
        Interval _12668 = Interval{ 0.0, 0.0 };
        Interval _12669 = Interval{ _14486, interval_sine_upper };
        Interval _14495 = jet_mul_derivative(_12668, _12669, intervalFailed, optical_product_upper);
        Interval _12670 = _14495;
        Interval _12671 = _14456;
        Interval _12672 = _14489;
        Interval _14496 = jet_mul_derivative(_12671, _12672, intervalFailed, optical_product_upper);
        Interval _12673 = _14496;
        Interval _14497 = jet_add_derivative(_12670, _12673, intervalFailed);
        Interval _12674 = Interval{ 0.0, 0.0 };
        Interval _12675 = Interval{ _14486, interval_sine_upper };
        Interval _14499 = jet_mul_derivative(_12674, _12675, intervalFailed, optical_product_upper);
        Interval _12676 = _14499;
        Interval _12677 = _14456;
        Interval _12678 = _14491;
        Interval _14500 = jet_mul_derivative(_12677, _12678, intervalFailed, optical_product_upper);
        Interval _12679 = _14500;
        Interval _14501 = jet_add_derivative(_12676, _12679, intervalFailed);
        float _12664 = 24.0;
        float _12665 = 1000.0;
        Interval _14504 = iratio(_12664, _12665, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14509;
        if (!intervalFailed)
        {
            _14509 = intervalFailed;
        }
        else
        {
            _14509 = false;
        }
        bool _14514;
        if (_14509)
        {
            _14514 = jetFailureSite == 0u;
        }
        else
        {
            _14514 = false;
        }
        if (_14514)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
        }
        Interval _12650 = footprint.v;
        Interval _12651 = _14504;
        Interval _14520 = imul(_12650, _12651, intervalFailed, optical_product_upper);
        Interval _12652 = footprint.dx;
        Interval _12653 = _14504;
        Interval _14521 = jet_mul_derivative(_12652, _12653, intervalFailed, optical_product_upper);
        Interval _12654 = _14521;
        Interval _12655 = footprint.v;
        Interval _12656 = Interval{ 0.0, 0.0 };
        Interval _14522 = jet_mul_derivative(_12655, _12656, intervalFailed, optical_product_upper);
        Interval _12657 = _14522;
        Interval _14523 = jet_add_derivative(_12654, _12657, intervalFailed);
        Interval _12658 = footprint.dy;
        Interval _12659 = _14504;
        Interval _14524 = jet_mul_derivative(_12658, _12659, intervalFailed, optical_product_upper);
        Interval _12660 = _14524;
        Interval _12661 = footprint.v;
        Interval _12662 = Interval{ 0.0, 0.0 };
        Interval _14525 = jet_mul_derivative(_12661, _12662, intervalFailed, optical_product_upper);
        Interval _12663 = _14525;
        Interval _14526 = jet_add_derivative(_12660, _12663, intervalFailed);
        bool _14533;
        if (_14520.lo <= 0.0)
        {
            _14533 = _14520.hi >= 0.0;
        }
        else
        {
            _14533 = false;
        }
        float _14540;
        if (_14533)
        {
            _14540 = 0.0;
        }
        else
        {
            _14540 = precise::min(abs(_14520.lo), abs(_14520.hi));
        }
        float _14543 = precise::max(abs(_14520.lo), abs(_14520.hi));
        float _12640 = spvFMul(_14540, _14540);
        float _14544 = interval_down(_12640, intervalFailed);
        float _14545 = precise::max(0.0, _14544);
        float _12641 = spvFMul(_14543, _14543);
        float _14546 = interval_up(_12641, intervalFailed);
        Interval _12642 = Interval{ 2.0, 2.0 };
        Interval _12643 = _14520;
        Interval _14547 = imul(_12642, _12643, intervalFailed, optical_product_upper);
        Interval _12644 = _14547;
        Interval _12645 = _14523;
        Interval _14548 = jet_mul_derivative(_12644, _12645, intervalFailed, optical_product_upper);
        Interval _12646 = Interval{ 2.0, 2.0 };
        Interval _12647 = _14520;
        Interval _14549 = imul(_12646, _12647, intervalFailed, optical_product_upper);
        Interval _12648 = _14549;
        Interval _12649 = _14526;
        Interval _14550 = jet_mul_derivative(_12648, _12649, intervalFailed, optical_product_upper);
        bool _14555;
        if (_14545 <= 0.0)
        {
            _14555 = _14546 >= 0.0;
        }
        else
        {
            _14555 = false;
        }
        float _14562;
        if (_14555)
        {
            _14562 = 0.0;
        }
        else
        {
            _14562 = precise::min(abs(_14545), abs(_14546));
        }
        float _14565 = precise::max(abs(_14545), abs(_14546));
        float _12630 = spvFMul(_14562, _14562);
        float _14566 = interval_down(_12630, intervalFailed);
        float _12631 = spvFMul(_14565, _14565);
        float _14568 = interval_up(_12631, intervalFailed);
        Interval _12632 = Interval{ 2.0, 2.0 };
        Interval _12633 = Interval{ _14545, _14546 };
        Interval _14570 = imul(_12632, _12633, intervalFailed, optical_product_upper);
        Interval _12634 = _14570;
        Interval _12635 = _14548;
        Interval _14571 = jet_mul_derivative(_12634, _12635, intervalFailed, optical_product_upper);
        Interval _12636 = Interval{ 2.0, 2.0 };
        Interval _12637 = Interval{ _14545, _14546 };
        Interval _14573 = imul(_12636, _12637, intervalFailed, optical_product_upper);
        Interval _12638 = _14573;
        Interval _12639 = _14550;
        Interval _14574 = jet_mul_derivative(_12638, _12639, intervalFailed, optical_product_upper);
        Interval _12624 = Interval{ 1.0, 1.0 };
        Interval _12625 = Interval{ precise::max(0.0, _14566), _14568 };
        Interval _14576 = iadd(_12624, _12625, intervalFailed);
        Interval _12626 = Interval{ 0.0, 0.0 };
        Interval _12627 = _14571;
        Interval _14577 = jet_add_derivative(_12626, _12627, intervalFailed);
        Interval _12628 = Interval{ 0.0, 0.0 };
        Interval _12629 = _14574;
        Interval _14578 = jet_add_derivative(_12628, _12629, intervalFailed);
        Interval _12610 = Interval{ 1.0, 1.0 };
        Interval _12611 = _14576;
        Interval _14580 = idiv(_12610, _12611, intervalFailed, interval_divide_upper);
        bool _14587;
        if (!intervalFailed)
        {
            _14587 = intervalFailed;
        }
        else
        {
            _14587 = false;
        }
        bool _14592;
        if (_14587)
        {
            _14592 = jetFailureSite == 0u;
        }
        else
        {
            _14592 = false;
        }
        if (_14592)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14576.lo, _14576.hi);
        }
        Interval _12612 = Interval{ 0.0, 0.0 };
        Interval _12613 = _14580;
        Interval _12614 = _14577;
        Interval _14596 = jet_mul_derivative(_12613, _12614, intervalFailed, optical_product_upper);
        Interval _12615 = Interval{ as_type<float>(as_type<uint>(_14596.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14596.lo) ^ 2147483648u) };
        Interval _14606 = jet_add_derivative(_12612, _12615, intervalFailed);
        Interval _12616 = _14606;
        Interval _12617 = _14576;
        Interval _14607 = jet_div_derivative(_12616, _12617, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12618 = Interval{ 0.0, 0.0 };
        Interval _12619 = _14580;
        Interval _12620 = _14578;
        Interval _14608 = jet_mul_derivative(_12619, _12620, intervalFailed, optical_product_upper);
        Interval _12621 = Interval{ as_type<float>(as_type<uint>(_14608.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14608.lo) ^ 2147483648u) };
        Interval _14618 = jet_add_derivative(_12618, _12621, intervalFailed);
        Interval _12622 = _14618;
        Interval _12623 = _14576;
        Interval _14619 = jet_div_derivative(_12622, _12623, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12596 = _14493;
        Interval _12597 = _14580;
        Interval _14620 = imul(_12596, _12597, intervalFailed, optical_product_upper);
        Interval _12598 = _14497;
        Interval _12599 = _14580;
        Interval _14621 = jet_mul_derivative(_12598, _12599, intervalFailed, optical_product_upper);
        Interval _12600 = _14621;
        Interval _12601 = _14493;
        Interval _12602 = _14607;
        Interval _14622 = jet_mul_derivative(_12601, _12602, intervalFailed, optical_product_upper);
        Interval _12603 = _14622;
        Interval _14623 = jet_add_derivative(_12600, _12603, intervalFailed);
        Interval _12604 = _14501;
        Interval _12605 = _14580;
        Interval _14624 = jet_mul_derivative(_12604, _12605, intervalFailed, optical_product_upper);
        Interval _12606 = _14624;
        Interval _12607 = _14493;
        Interval _12608 = _14619;
        Interval _14625 = jet_mul_derivative(_12607, _12608, intervalFailed, optical_product_upper);
        Interval _12609 = _14625;
        Interval _14626 = jet_add_derivative(_12606, _12609, intervalFailed);
        float _12594 = 20.0;
        float _12595 = 1000.0;
        Interval _14628 = iratio(_12594, _12595, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14633;
        if (!intervalFailed)
        {
            _14633 = intervalFailed;
        }
        else
        {
            _14633 = false;
        }
        bool _14638;
        if (_14633)
        {
            _14638 = jetFailureSite == 0u;
        }
        else
        {
            _14638 = false;
        }
        if (_14638)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(20.0, 20.0, 1000.0, 1000.0);
        }
        float _12588 = _13973.lo;
        float _12589 = _13973.hi;
        float _14643 = sine_bounds(_12588, _12589, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14647 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14650 = as_type<float>(as_type<uint>(_14643) ^ 2147483648u);
        Interval _12584 = _13973;
        float _12582 = 3.1415927410125732421875;
        float _14651 = interval_down(_12582, intervalFailed);
        float _12583 = 3.1415927410125732421875;
        float _14652 = interval_up(_12583, intervalFailed);
        Interval _12585 = Interval{ _14651, _14652 };
        Interval _12586 = Interval{ 0.5, 0.5 };
        Interval _14654 = imul(_12585, _12586, intervalFailed, optical_product_upper);
        Interval _12587 = _14654;
        Interval _14655 = iadd(_12584, _12587, intervalFailed);
        float _12580 = _14655.lo;
        float _12581 = _14655.hi;
        float _14658 = sine_bounds(_12580, _12581, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12590 = Interval{ _14647, _14650 };
        Interval _12591 = _13974;
        Interval _14661 = jet_mul_derivative(_12590, _12591, intervalFailed, optical_product_upper);
        Interval _12592 = Interval{ _14647, _14650 };
        Interval _12593 = _13975;
        Interval _14663 = jet_mul_derivative(_12592, _12593, intervalFailed, optical_product_upper);
        Interval _12566 = _14628;
        Interval _12567 = Interval{ _14658, interval_sine_upper };
        Interval _14665 = imul(_12566, _12567, intervalFailed, optical_product_upper);
        Interval _12568 = Interval{ 0.0, 0.0 };
        Interval _12569 = Interval{ _14658, interval_sine_upper };
        Interval _14667 = jet_mul_derivative(_12568, _12569, intervalFailed, optical_product_upper);
        Interval _12570 = _14667;
        Interval _12571 = _14628;
        Interval _12572 = _14661;
        Interval _14668 = jet_mul_derivative(_12571, _12572, intervalFailed, optical_product_upper);
        Interval _12573 = _14668;
        Interval _14669 = jet_add_derivative(_12570, _12573, intervalFailed);
        Interval _12574 = Interval{ 0.0, 0.0 };
        Interval _12575 = Interval{ _14658, interval_sine_upper };
        Interval _14671 = jet_mul_derivative(_12574, _12575, intervalFailed, optical_product_upper);
        Interval _12576 = _14671;
        Interval _12577 = _14628;
        Interval _12578 = _14663;
        Interval _14672 = jet_mul_derivative(_12577, _12578, intervalFailed, optical_product_upper);
        Interval _12579 = _14672;
        Interval _14673 = jet_add_derivative(_12576, _12579, intervalFailed);
        float _12564 = 54.0;
        float _12565 = 1000.0;
        Interval _14676 = iratio(_12564, _12565, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14681;
        if (!intervalFailed)
        {
            _14681 = intervalFailed;
        }
        else
        {
            _14681 = false;
        }
        bool _14686;
        if (_14681)
        {
            _14686 = jetFailureSite == 0u;
        }
        else
        {
            _14686 = false;
        }
        if (_14686)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12550 = footprint.v;
        Interval _12551 = _14676;
        Interval _14692 = imul(_12550, _12551, intervalFailed, optical_product_upper);
        Interval _12552 = footprint.dx;
        Interval _12553 = _14676;
        Interval _14693 = jet_mul_derivative(_12552, _12553, intervalFailed, optical_product_upper);
        Interval _12554 = _14693;
        Interval _12555 = footprint.v;
        Interval _12556 = Interval{ 0.0, 0.0 };
        Interval _14694 = jet_mul_derivative(_12555, _12556, intervalFailed, optical_product_upper);
        Interval _12557 = _14694;
        Interval _14695 = jet_add_derivative(_12554, _12557, intervalFailed);
        Interval _12558 = footprint.dy;
        Interval _12559 = _14676;
        Interval _14696 = jet_mul_derivative(_12558, _12559, intervalFailed, optical_product_upper);
        Interval _12560 = _14696;
        Interval _12561 = footprint.v;
        Interval _12562 = Interval{ 0.0, 0.0 };
        Interval _14697 = jet_mul_derivative(_12561, _12562, intervalFailed, optical_product_upper);
        Interval _12563 = _14697;
        Interval _14698 = jet_add_derivative(_12560, _12563, intervalFailed);
        bool _14705;
        if (_14692.lo <= 0.0)
        {
            _14705 = _14692.hi >= 0.0;
        }
        else
        {
            _14705 = false;
        }
        float _14712;
        if (_14705)
        {
            _14712 = 0.0;
        }
        else
        {
            _14712 = precise::min(abs(_14692.lo), abs(_14692.hi));
        }
        float _14715 = precise::max(abs(_14692.lo), abs(_14692.hi));
        float _12540 = spvFMul(_14712, _14712);
        float _14716 = interval_down(_12540, intervalFailed);
        float _14717 = precise::max(0.0, _14716);
        float _12541 = spvFMul(_14715, _14715);
        float _14718 = interval_up(_12541, intervalFailed);
        Interval _12542 = Interval{ 2.0, 2.0 };
        Interval _12543 = _14692;
        Interval _14719 = imul(_12542, _12543, intervalFailed, optical_product_upper);
        Interval _12544 = _14719;
        Interval _12545 = _14695;
        Interval _14720 = jet_mul_derivative(_12544, _12545, intervalFailed, optical_product_upper);
        Interval _12546 = Interval{ 2.0, 2.0 };
        Interval _12547 = _14692;
        Interval _14721 = imul(_12546, _12547, intervalFailed, optical_product_upper);
        Interval _12548 = _14721;
        Interval _12549 = _14698;
        Interval _14722 = jet_mul_derivative(_12548, _12549, intervalFailed, optical_product_upper);
        bool _14727;
        if (_14717 <= 0.0)
        {
            _14727 = _14718 >= 0.0;
        }
        else
        {
            _14727 = false;
        }
        float _14734;
        if (_14727)
        {
            _14734 = 0.0;
        }
        else
        {
            _14734 = precise::min(abs(_14717), abs(_14718));
        }
        float _14737 = precise::max(abs(_14717), abs(_14718));
        float _12530 = spvFMul(_14734, _14734);
        float _14738 = interval_down(_12530, intervalFailed);
        float _12531 = spvFMul(_14737, _14737);
        float _14740 = interval_up(_12531, intervalFailed);
        Interval _12532 = Interval{ 2.0, 2.0 };
        Interval _12533 = Interval{ _14717, _14718 };
        Interval _14742 = imul(_12532, _12533, intervalFailed, optical_product_upper);
        Interval _12534 = _14742;
        Interval _12535 = _14720;
        Interval _14743 = jet_mul_derivative(_12534, _12535, intervalFailed, optical_product_upper);
        Interval _12536 = Interval{ 2.0, 2.0 };
        Interval _12537 = Interval{ _14717, _14718 };
        Interval _14745 = imul(_12536, _12537, intervalFailed, optical_product_upper);
        Interval _12538 = _14745;
        Interval _12539 = _14722;
        Interval _14746 = jet_mul_derivative(_12538, _12539, intervalFailed, optical_product_upper);
        Interval _12524 = Interval{ 1.0, 1.0 };
        Interval _12525 = Interval{ precise::max(0.0, _14738), _14740 };
        Interval _14748 = iadd(_12524, _12525, intervalFailed);
        Interval _12526 = Interval{ 0.0, 0.0 };
        Interval _12527 = _14743;
        Interval _14749 = jet_add_derivative(_12526, _12527, intervalFailed);
        Interval _12528 = Interval{ 0.0, 0.0 };
        Interval _12529 = _14746;
        Interval _14750 = jet_add_derivative(_12528, _12529, intervalFailed);
        Interval _12510 = Interval{ 1.0, 1.0 };
        Interval _12511 = _14748;
        Interval _14752 = idiv(_12510, _12511, intervalFailed, interval_divide_upper);
        bool _14759;
        if (!intervalFailed)
        {
            _14759 = intervalFailed;
        }
        else
        {
            _14759 = false;
        }
        bool _14764;
        if (_14759)
        {
            _14764 = jetFailureSite == 0u;
        }
        else
        {
            _14764 = false;
        }
        if (_14764)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14748.lo, _14748.hi);
        }
        Interval _12512 = Interval{ 0.0, 0.0 };
        Interval _12513 = _14752;
        Interval _12514 = _14749;
        Interval _14768 = jet_mul_derivative(_12513, _12514, intervalFailed, optical_product_upper);
        Interval _12515 = Interval{ as_type<float>(as_type<uint>(_14768.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14768.lo) ^ 2147483648u) };
        Interval _14778 = jet_add_derivative(_12512, _12515, intervalFailed);
        Interval _12516 = _14778;
        Interval _12517 = _14748;
        Interval _14779 = jet_div_derivative(_12516, _12517, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12518 = Interval{ 0.0, 0.0 };
        Interval _12519 = _14752;
        Interval _12520 = _14750;
        Interval _14780 = jet_mul_derivative(_12519, _12520, intervalFailed, optical_product_upper);
        Interval _12521 = Interval{ as_type<float>(as_type<uint>(_14780.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14780.lo) ^ 2147483648u) };
        Interval _14790 = jet_add_derivative(_12518, _12521, intervalFailed);
        Interval _12522 = _14790;
        Interval _12523 = _14748;
        Interval _14791 = jet_div_derivative(_12522, _12523, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12496 = _14665;
        Interval _12497 = _14752;
        Interval _14792 = imul(_12496, _12497, intervalFailed, optical_product_upper);
        Interval _12498 = _14669;
        Interval _12499 = _14752;
        Interval _14793 = jet_mul_derivative(_12498, _12499, intervalFailed, optical_product_upper);
        Interval _12500 = _14793;
        Interval _12501 = _14665;
        Interval _12502 = _14779;
        Interval _14794 = jet_mul_derivative(_12501, _12502, intervalFailed, optical_product_upper);
        Interval _12503 = _14794;
        Interval _14795 = jet_add_derivative(_12500, _12503, intervalFailed);
        Interval _12504 = _14673;
        Interval _12505 = _14752;
        Interval _14796 = jet_mul_derivative(_12504, _12505, intervalFailed, optical_product_upper);
        Interval _12506 = _14796;
        Interval _12507 = _14665;
        Interval _12508 = _14791;
        Interval _14797 = jet_mul_derivative(_12507, _12508, intervalFailed, optical_product_upper);
        Interval _12509 = _14797;
        Interval _14798 = jet_add_derivative(_12506, _12509, intervalFailed);
        Interval _12490 = _14620;
        Interval _12491 = Interval{ as_type<float>(as_type<uint>(_14792.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14792.lo) ^ 2147483648u) };
        Interval _14824 = iadd(_12490, _12491, intervalFailed);
        Interval _12492 = _14623;
        Interval _12493 = Interval{ as_type<float>(as_type<uint>(_14795.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14795.lo) ^ 2147483648u) };
        Interval _14826 = jet_add_derivative(_12492, _12493, intervalFailed);
        Interval _12494 = _14626;
        Interval _12495 = Interval{ as_type<float>(as_type<uint>(_14798.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14798.lo) ^ 2147483648u) };
        Interval _14828 = jet_add_derivative(_12494, _12495, intervalFailed);
        _15366 = _14824.lo;
        _15367 = _14824.hi;
        _15368 = _14826.lo;
        _15369 = _14826.hi;
        _15370 = _14828.lo;
        _15371 = _14828.hi;
        _15372 = _14446.lo;
        _15373 = _14446.hi;
        _15374 = _14447.lo;
        _15375 = _14447.hi;
        _15376 = _14448.lo;
        _15377 = _14448.hi;
    }
    Interval _12476 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12477 = Interval{ _15372, _15373 };
    Interval _15387 = imul(_12476, _12477, intervalFailed, optical_product_upper);
    Interval _12478 = Interval{ 0.0, 0.0 };
    Interval _12479 = Interval{ _15372, _15373 };
    Interval _15389 = jet_mul_derivative(_12478, _12479, intervalFailed, optical_product_upper);
    Interval _12480 = _15389;
    Interval _12481 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12482 = Interval{ _15374, _15375 };
    Interval _15392 = jet_mul_derivative(_12481, _12482, intervalFailed, optical_product_upper);
    Interval _12483 = _15392;
    Interval _15393 = jet_add_derivative(_12480, _12483, intervalFailed);
    Interval _12484 = Interval{ 0.0, 0.0 };
    Interval _12485 = Interval{ _15372, _15373 };
    Interval _15395 = jet_mul_derivative(_12484, _12485, intervalFailed, optical_product_upper);
    Interval _12486 = _15395;
    Interval _12487 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12488 = Interval{ _15376, _15377 };
    Interval _15398 = jet_mul_derivative(_12487, _12488, intervalFailed, optical_product_upper);
    Interval _12489 = _15398;
    Interval _15399 = jet_add_derivative(_12486, _12489, intervalFailed);
    Interval _12462 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12463 = Interval{ -1.0, -1.0 };
    Interval _15401 = imul(_12462, _12463, intervalFailed, optical_product_upper);
    Interval _12464 = Interval{ 0.0, 0.0 };
    Interval _12465 = Interval{ -1.0, -1.0 };
    Interval _15402 = jet_mul_derivative(_12464, _12465, intervalFailed, optical_product_upper);
    Interval _12466 = _15402;
    Interval _12467 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12468 = Interval{ 0.0, 0.0 };
    Interval _15404 = jet_mul_derivative(_12467, _12468, intervalFailed, optical_product_upper);
    Interval _12469 = _15404;
    Interval _15405 = jet_add_derivative(_12466, _12469, intervalFailed);
    Interval _12470 = Interval{ 0.0, 0.0 };
    Interval _12471 = Interval{ -1.0, -1.0 };
    Interval _15406 = jet_mul_derivative(_12470, _12471, intervalFailed, optical_product_upper);
    Interval _12472 = _15406;
    Interval _12473 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12474 = Interval{ 0.0, 0.0 };
    Interval _15408 = jet_mul_derivative(_12473, _12474, intervalFailed, optical_product_upper);
    Interval _12475 = _15408;
    Interval _15409 = jet_add_derivative(_12472, _12475, intervalFailed);
    Interval _12456 = _15387;
    Interval _12457 = _15401;
    Interval _15410 = iadd(_12456, _12457, intervalFailed);
    Interval _12458 = _15393;
    Interval _12459 = _15405;
    Interval _15411 = jet_add_derivative(_12458, _12459, intervalFailed);
    Interval _12460 = _15399;
    Interval _12461 = _15409;
    Interval _15412 = jet_add_derivative(_12460, _12461, intervalFailed);
    Interval _12442 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12443 = Interval{ _15366, _15367 };
    Interval _15415 = imul(_12442, _12443, intervalFailed, optical_product_upper);
    Interval _12444 = Interval{ 0.0, 0.0 };
    Interval _12445 = Interval{ _15366, _15367 };
    Interval _15417 = jet_mul_derivative(_12444, _12445, intervalFailed, optical_product_upper);
    Interval _12446 = _15417;
    Interval _12447 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12448 = Interval{ _15368, _15369 };
    Interval _15420 = jet_mul_derivative(_12447, _12448, intervalFailed, optical_product_upper);
    Interval _12449 = _15420;
    Interval _15421 = jet_add_derivative(_12446, _12449, intervalFailed);
    Interval _12450 = Interval{ 0.0, 0.0 };
    Interval _12451 = Interval{ _15366, _15367 };
    Interval _15423 = jet_mul_derivative(_12450, _12451, intervalFailed, optical_product_upper);
    Interval _12452 = _15423;
    Interval _12453 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12454 = Interval{ _15370, _15371 };
    Interval _15426 = jet_mul_derivative(_12453, _12454, intervalFailed, optical_product_upper);
    Interval _12455 = _15426;
    Interval _15427 = jet_add_derivative(_12452, _12455, intervalFailed);
    Interval _12436 = _15410;
    Interval _12437 = _15415;
    Interval _15428 = iadd(_12436, _12437, intervalFailed);
    Interval _12438 = _15411;
    Interval _12439 = _15421;
    Interval _15429 = jet_add_derivative(_12438, _12439, intervalFailed);
    Interval _12440 = _15412;
    Interval _12441 = _15427;
    Interval _15430 = jet_add_derivative(_12440, _12441, intervalFailed);
    Interval _12422 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12423 = Interval{ _15372, _15373 };
    Interval _15436 = imul(_12422, _12423, intervalFailed, optical_product_upper);
    Interval _12424 = Interval{ 0.0, 0.0 };
    Interval _12425 = Interval{ _15372, _15373 };
    Interval _15438 = jet_mul_derivative(_12424, _12425, intervalFailed, optical_product_upper);
    Interval _12426 = _15438;
    Interval _12427 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12428 = Interval{ _15374, _15375 };
    Interval _15441 = jet_mul_derivative(_12427, _12428, intervalFailed, optical_product_upper);
    Interval _12429 = _15441;
    Interval _15442 = jet_add_derivative(_12426, _12429, intervalFailed);
    Interval _12430 = Interval{ 0.0, 0.0 };
    Interval _12431 = Interval{ _15372, _15373 };
    Interval _15444 = jet_mul_derivative(_12430, _12431, intervalFailed, optical_product_upper);
    Interval _12432 = _15444;
    Interval _12433 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12434 = Interval{ _15376, _15377 };
    Interval _15447 = jet_mul_derivative(_12433, _12434, intervalFailed, optical_product_upper);
    Interval _12435 = _15447;
    Interval _15448 = jet_add_derivative(_12432, _12435, intervalFailed);
    Interval _12408 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12409 = Interval{ -1.0, -1.0 };
    Interval _15450 = imul(_12408, _12409, intervalFailed, optical_product_upper);
    Interval _12410 = Interval{ 0.0, 0.0 };
    Interval _12411 = Interval{ -1.0, -1.0 };
    Interval _15451 = jet_mul_derivative(_12410, _12411, intervalFailed, optical_product_upper);
    Interval _12412 = _15451;
    Interval _12413 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12414 = Interval{ 0.0, 0.0 };
    Interval _15453 = jet_mul_derivative(_12413, _12414, intervalFailed, optical_product_upper);
    Interval _12415 = _15453;
    Interval _15454 = jet_add_derivative(_12412, _12415, intervalFailed);
    Interval _12416 = Interval{ 0.0, 0.0 };
    Interval _12417 = Interval{ -1.0, -1.0 };
    Interval _15455 = jet_mul_derivative(_12416, _12417, intervalFailed, optical_product_upper);
    Interval _12418 = _15455;
    Interval _12419 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12420 = Interval{ 0.0, 0.0 };
    Interval _15457 = jet_mul_derivative(_12419, _12420, intervalFailed, optical_product_upper);
    Interval _12421 = _15457;
    Interval _15458 = jet_add_derivative(_12418, _12421, intervalFailed);
    Interval _12402 = _15436;
    Interval _12403 = _15450;
    Interval _15459 = iadd(_12402, _12403, intervalFailed);
    Interval _12404 = _15442;
    Interval _12405 = _15454;
    Interval _15460 = jet_add_derivative(_12404, _12405, intervalFailed);
    Interval _12406 = _15448;
    Interval _12407 = _15458;
    Interval _15461 = jet_add_derivative(_12406, _12407, intervalFailed);
    Interval _12388 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12389 = Interval{ _15366, _15367 };
    Interval _15464 = imul(_12388, _12389, intervalFailed, optical_product_upper);
    Interval _12390 = Interval{ 0.0, 0.0 };
    Interval _12391 = Interval{ _15366, _15367 };
    Interval _15466 = jet_mul_derivative(_12390, _12391, intervalFailed, optical_product_upper);
    Interval _12392 = _15466;
    Interval _12393 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12394 = Interval{ _15368, _15369 };
    Interval _15469 = jet_mul_derivative(_12393, _12394, intervalFailed, optical_product_upper);
    Interval _12395 = _15469;
    Interval _15470 = jet_add_derivative(_12392, _12395, intervalFailed);
    Interval _12396 = Interval{ 0.0, 0.0 };
    Interval _12397 = Interval{ _15366, _15367 };
    Interval _15472 = jet_mul_derivative(_12396, _12397, intervalFailed, optical_product_upper);
    Interval _12398 = _15472;
    Interval _12399 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12400 = Interval{ _15370, _15371 };
    Interval _15475 = jet_mul_derivative(_12399, _12400, intervalFailed, optical_product_upper);
    Interval _12401 = _15475;
    Interval _15476 = jet_add_derivative(_12398, _12401, intervalFailed);
    Interval _12382 = _15459;
    Interval _12383 = _15464;
    Interval _15477 = iadd(_12382, _12383, intervalFailed);
    Interval _12384 = _15460;
    Interval _12385 = _15470;
    Interval _15478 = jet_add_derivative(_12384, _12385, intervalFailed);
    Interval _12386 = _15461;
    Interval _12387 = _15476;
    Interval _15479 = jet_add_derivative(_12386, _12387, intervalFailed);
    Interval _12368 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12369 = Interval{ _15372, _15373 };
    Interval _15485 = imul(_12368, _12369, intervalFailed, optical_product_upper);
    Interval _12370 = Interval{ 0.0, 0.0 };
    Interval _12371 = Interval{ _15372, _15373 };
    Interval _15487 = jet_mul_derivative(_12370, _12371, intervalFailed, optical_product_upper);
    Interval _12372 = _15487;
    Interval _12373 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12374 = Interval{ _15374, _15375 };
    Interval _15490 = jet_mul_derivative(_12373, _12374, intervalFailed, optical_product_upper);
    Interval _12375 = _15490;
    Interval _15491 = jet_add_derivative(_12372, _12375, intervalFailed);
    Interval _12376 = Interval{ 0.0, 0.0 };
    Interval _12377 = Interval{ _15372, _15373 };
    Interval _15493 = jet_mul_derivative(_12376, _12377, intervalFailed, optical_product_upper);
    Interval _12378 = _15493;
    Interval _12379 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12380 = Interval{ _15376, _15377 };
    Interval _15496 = jet_mul_derivative(_12379, _12380, intervalFailed, optical_product_upper);
    Interval _12381 = _15496;
    Interval _15497 = jet_add_derivative(_12378, _12381, intervalFailed);
    Interval _12354 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12355 = Interval{ -1.0, -1.0 };
    Interval _15499 = imul(_12354, _12355, intervalFailed, optical_product_upper);
    Interval _12356 = Interval{ 0.0, 0.0 };
    Interval _12357 = Interval{ -1.0, -1.0 };
    Interval _15500 = jet_mul_derivative(_12356, _12357, intervalFailed, optical_product_upper);
    Interval _12358 = _15500;
    Interval _12359 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12360 = Interval{ 0.0, 0.0 };
    Interval _15502 = jet_mul_derivative(_12359, _12360, intervalFailed, optical_product_upper);
    Interval _12361 = _15502;
    Interval _15503 = jet_add_derivative(_12358, _12361, intervalFailed);
    Interval _12362 = Interval{ 0.0, 0.0 };
    Interval _12363 = Interval{ -1.0, -1.0 };
    Interval _15504 = jet_mul_derivative(_12362, _12363, intervalFailed, optical_product_upper);
    Interval _12364 = _15504;
    Interval _12365 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12366 = Interval{ 0.0, 0.0 };
    Interval _15506 = jet_mul_derivative(_12365, _12366, intervalFailed, optical_product_upper);
    Interval _12367 = _15506;
    Interval _15507 = jet_add_derivative(_12364, _12367, intervalFailed);
    Interval _12348 = _15485;
    Interval _12349 = _15499;
    Interval _15508 = iadd(_12348, _12349, intervalFailed);
    Interval _12350 = _15491;
    Interval _12351 = _15503;
    Interval _15509 = jet_add_derivative(_12350, _12351, intervalFailed);
    Interval _12352 = _15497;
    Interval _12353 = _15507;
    Interval _15510 = jet_add_derivative(_12352, _12353, intervalFailed);
    Interval _12334 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12335 = Interval{ _15366, _15367 };
    Interval _15513 = imul(_12334, _12335, intervalFailed, optical_product_upper);
    Interval _12336 = Interval{ 0.0, 0.0 };
    Interval _12337 = Interval{ _15366, _15367 };
    Interval _15515 = jet_mul_derivative(_12336, _12337, intervalFailed, optical_product_upper);
    Interval _12338 = _15515;
    Interval _12339 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12340 = Interval{ _15368, _15369 };
    Interval _15518 = jet_mul_derivative(_12339, _12340, intervalFailed, optical_product_upper);
    Interval _12341 = _15518;
    Interval _15519 = jet_add_derivative(_12338, _12341, intervalFailed);
    Interval _12342 = Interval{ 0.0, 0.0 };
    Interval _12343 = Interval{ _15366, _15367 };
    Interval _15521 = jet_mul_derivative(_12342, _12343, intervalFailed, optical_product_upper);
    Interval _12344 = _15521;
    Interval _12345 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12346 = Interval{ _15370, _15371 };
    Interval _15524 = jet_mul_derivative(_12345, _12346, intervalFailed, optical_product_upper);
    Interval _12347 = _15524;
    Interval _15525 = jet_add_derivative(_12344, _12347, intervalFailed);
    Interval _12328 = _15508;
    Interval _12329 = _15513;
    Interval _15526 = iadd(_12328, _12329, intervalFailed);
    Interval _12330 = _15509;
    Interval _12331 = _15519;
    Interval _15527 = jet_add_derivative(_12330, _12331, intervalFailed);
    Interval _12332 = _15510;
    Interval _12333 = _15525;
    Interval _15528 = jet_add_derivative(_12332, _12333, intervalFailed);
    bool _15535;
    if (_15428.lo <= 0.0)
    {
        _15535 = _15428.hi >= 0.0;
    }
    else
    {
        _15535 = false;
    }
    float _15542;
    if (_15535)
    {
        _15542 = 0.0;
    }
    else
    {
        _15542 = precise::min(abs(_15428.lo), abs(_15428.hi));
    }
    float _15545 = precise::max(abs(_15428.lo), abs(_15428.hi));
    float _12318 = spvFMul(_15542, _15542);
    float _15546 = interval_down(_12318, intervalFailed);
    float _12319 = spvFMul(_15545, _15545);
    float _15548 = interval_up(_12319, intervalFailed);
    Interval _12320 = Interval{ 2.0, 2.0 };
    Interval _12321 = _15428;
    Interval _15549 = imul(_12320, _12321, intervalFailed, optical_product_upper);
    Interval _12322 = _15549;
    Interval _12323 = _15429;
    Interval _15550 = jet_mul_derivative(_12322, _12323, intervalFailed, optical_product_upper);
    Interval _12324 = Interval{ 2.0, 2.0 };
    Interval _12325 = _15428;
    Interval _15551 = imul(_12324, _12325, intervalFailed, optical_product_upper);
    Interval _12326 = _15551;
    Interval _12327 = _15430;
    Interval _15552 = jet_mul_derivative(_12326, _12327, intervalFailed, optical_product_upper);
    bool _15559;
    if (_15477.lo <= 0.0)
    {
        _15559 = _15477.hi >= 0.0;
    }
    else
    {
        _15559 = false;
    }
    float _15566;
    if (_15559)
    {
        _15566 = 0.0;
    }
    else
    {
        _15566 = precise::min(abs(_15477.lo), abs(_15477.hi));
    }
    float _15569 = precise::max(abs(_15477.lo), abs(_15477.hi));
    float _12308 = spvFMul(_15566, _15566);
    float _15570 = interval_down(_12308, intervalFailed);
    float _12309 = spvFMul(_15569, _15569);
    float _15572 = interval_up(_12309, intervalFailed);
    Interval _12310 = Interval{ 2.0, 2.0 };
    Interval _12311 = _15477;
    Interval _15573 = imul(_12310, _12311, intervalFailed, optical_product_upper);
    Interval _12312 = _15573;
    Interval _12313 = _15478;
    Interval _15574 = jet_mul_derivative(_12312, _12313, intervalFailed, optical_product_upper);
    Interval _12314 = Interval{ 2.0, 2.0 };
    Interval _12315 = _15477;
    Interval _15575 = imul(_12314, _12315, intervalFailed, optical_product_upper);
    Interval _12316 = _15575;
    Interval _12317 = _15479;
    Interval _15576 = jet_mul_derivative(_12316, _12317, intervalFailed, optical_product_upper);
    Interval _12302 = Interval{ precise::max(0.0, _15546), _15548 };
    Interval _12303 = Interval{ precise::max(0.0, _15570), _15572 };
    Interval _15579 = iadd(_12302, _12303, intervalFailed);
    Interval _12304 = _15550;
    Interval _12305 = _15574;
    Interval _15580 = jet_add_derivative(_12304, _12305, intervalFailed);
    Interval _12306 = _15552;
    Interval _12307 = _15576;
    Interval _15581 = jet_add_derivative(_12306, _12307, intervalFailed);
    bool _15588;
    if (_15526.lo <= 0.0)
    {
        _15588 = _15526.hi >= 0.0;
    }
    else
    {
        _15588 = false;
    }
    float _15595;
    if (_15588)
    {
        _15595 = 0.0;
    }
    else
    {
        _15595 = precise::min(abs(_15526.lo), abs(_15526.hi));
    }
    float _15598 = precise::max(abs(_15526.lo), abs(_15526.hi));
    float _12292 = spvFMul(_15595, _15595);
    float _15599 = interval_down(_12292, intervalFailed);
    float _12293 = spvFMul(_15598, _15598);
    float _15601 = interval_up(_12293, intervalFailed);
    Interval _12294 = Interval{ 2.0, 2.0 };
    Interval _12295 = _15526;
    Interval _15602 = imul(_12294, _12295, intervalFailed, optical_product_upper);
    Interval _12296 = _15602;
    Interval _12297 = _15527;
    Interval _15603 = jet_mul_derivative(_12296, _12297, intervalFailed, optical_product_upper);
    Interval _12298 = Interval{ 2.0, 2.0 };
    Interval _12299 = _15526;
    Interval _15604 = imul(_12298, _12299, intervalFailed, optical_product_upper);
    Interval _12300 = _15604;
    Interval _12301 = _15528;
    Interval _15605 = jet_mul_derivative(_12300, _12301, intervalFailed, optical_product_upper);
    Interval _12286 = _15579;
    Interval _12287 = Interval{ precise::max(0.0, _15599), _15601 };
    Interval _15607 = iadd(_12286, _12287, intervalFailed);
    Interval _12288 = _15580;
    Interval _12289 = _15603;
    Interval _15608 = jet_add_derivative(_12288, _12289, intervalFailed);
    Interval _12290 = _15581;
    Interval _12291 = _15605;
    Interval _15609 = jet_add_derivative(_12290, _12291, intervalFailed);
    Interval _12277 = _15607;
    Interval _15611 = isqrt(_12277, intervalFailed);
    bool _15619;
    if (!intervalFailed)
    {
        _15619 = intervalFailed;
    }
    else
    {
        _15619 = false;
    }
    bool _15624;
    if (_15619)
    {
        _15624 = jetFailureSite == 0u;
    }
    else
    {
        _15624 = false;
    }
    if (_15624)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_15607.lo, _15607.hi, 0.0, 0.0);
    }
    if (_15611.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _12278 = Interval{ 2.0, 2.0 };
    Interval _12279 = _15611;
    Interval _15631 = imul(_12278, _12279, intervalFailed, optical_product_upper);
    Interval _12280 = Interval{ 1.0, 1.0 };
    Interval _12281 = _15631;
    Interval _15633 = idiv(_12280, _12281, intervalFailed, interval_divide_upper);
    bool _15640;
    if (!intervalFailed)
    {
        _15640 = intervalFailed;
    }
    else
    {
        _15640 = false;
    }
    bool _15645;
    if (_15640)
    {
        _15645 = jetFailureSite == 0u;
    }
    else
    {
        _15645 = false;
    }
    if (_15645)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _15631.lo, _15631.hi);
    }
    Interval _12282 = _15633;
    Interval _12283 = _15608;
    Interval _15649 = jet_mul_derivative(_12282, _12283, intervalFailed, optical_product_upper);
    Interval _12284 = _15633;
    Interval _12285 = _15609;
    Interval _15650 = jet_mul_derivative(_12284, _12285, intervalFailed, optical_product_upper);
    Interval _12263 = Interval{ 1.0, 1.0 };
    Interval _12264 = _15611;
    Interval _15652 = idiv(_12263, _12264, intervalFailed, interval_divide_upper);
    bool _15659;
    if (!intervalFailed)
    {
        _15659 = intervalFailed;
    }
    else
    {
        _15659 = false;
    }
    bool _15664;
    if (_15659)
    {
        _15664 = jetFailureSite == 0u;
    }
    else
    {
        _15664 = false;
    }
    if (_15664)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _15611.lo, _15611.hi);
    }
    Interval _12265 = Interval{ 0.0, 0.0 };
    Interval _12266 = _15652;
    Interval _12267 = _15649;
    Interval _15668 = jet_mul_derivative(_12266, _12267, intervalFailed, optical_product_upper);
    Interval _12268 = Interval{ as_type<float>(as_type<uint>(_15668.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15668.lo) ^ 2147483648u) };
    Interval _15678 = jet_add_derivative(_12265, _12268, intervalFailed);
    Interval _12269 = _15678;
    Interval _12270 = _15611;
    Interval _15679 = jet_div_derivative(_12269, _12270, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12271 = Interval{ 0.0, 0.0 };
    Interval _12272 = _15652;
    Interval _12273 = _15650;
    Interval _15680 = jet_mul_derivative(_12272, _12273, intervalFailed, optical_product_upper);
    Interval _12274 = Interval{ as_type<float>(as_type<uint>(_15680.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15680.lo) ^ 2147483648u) };
    Interval _15690 = jet_add_derivative(_12271, _12274, intervalFailed);
    Interval _12275 = _15690;
    Interval _12276 = _15611;
    Interval _15691 = jet_div_derivative(_12275, _12276, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12249 = _15428;
    Interval _12250 = _15652;
    Interval _15692 = imul(_12249, _12250, intervalFailed, optical_product_upper);
    Interval _12251 = _15429;
    Interval _12252 = _15652;
    Interval _15693 = jet_mul_derivative(_12251, _12252, intervalFailed, optical_product_upper);
    Interval _12253 = _15693;
    Interval _12254 = _15428;
    Interval _12255 = _15679;
    Interval _15694 = jet_mul_derivative(_12254, _12255, intervalFailed, optical_product_upper);
    Interval _12256 = _15694;
    Interval _15695 = jet_add_derivative(_12253, _12256, intervalFailed);
    Interval _12257 = _15430;
    Interval _12258 = _15652;
    Interval _15696 = jet_mul_derivative(_12257, _12258, intervalFailed, optical_product_upper);
    Interval _12259 = _15696;
    Interval _12260 = _15428;
    Interval _12261 = _15691;
    Interval _15697 = jet_mul_derivative(_12260, _12261, intervalFailed, optical_product_upper);
    Interval _12262 = _15697;
    Interval _15698 = jet_add_derivative(_12259, _12262, intervalFailed);
    Interval _12235 = _15477;
    Interval _12236 = _15652;
    Interval _15699 = imul(_12235, _12236, intervalFailed, optical_product_upper);
    Interval _12237 = _15478;
    Interval _12238 = _15652;
    Interval _15700 = jet_mul_derivative(_12237, _12238, intervalFailed, optical_product_upper);
    Interval _12239 = _15700;
    Interval _12240 = _15477;
    Interval _12241 = _15679;
    Interval _15701 = jet_mul_derivative(_12240, _12241, intervalFailed, optical_product_upper);
    Interval _12242 = _15701;
    Interval _15702 = jet_add_derivative(_12239, _12242, intervalFailed);
    Interval _12243 = _15479;
    Interval _12244 = _15652;
    Interval _15703 = jet_mul_derivative(_12243, _12244, intervalFailed, optical_product_upper);
    Interval _12245 = _15703;
    Interval _12246 = _15477;
    Interval _12247 = _15691;
    Interval _15704 = jet_mul_derivative(_12246, _12247, intervalFailed, optical_product_upper);
    Interval _12248 = _15704;
    Interval _15705 = jet_add_derivative(_12245, _12248, intervalFailed);
    Interval _12221 = _15526;
    Interval _12222 = _15652;
    Interval _15706 = imul(_12221, _12222, intervalFailed, optical_product_upper);
    Interval _12223 = _15527;
    Interval _12224 = _15652;
    Interval _15707 = jet_mul_derivative(_12223, _12224, intervalFailed, optical_product_upper);
    Interval _12225 = _15707;
    Interval _12226 = _15526;
    Interval _12227 = _15679;
    Interval _15708 = jet_mul_derivative(_12226, _12227, intervalFailed, optical_product_upper);
    Interval _12228 = _15708;
    Interval _15709 = jet_add_derivative(_12225, _12228, intervalFailed);
    Interval _12229 = _15528;
    Interval _12230 = _15652;
    Interval _15710 = jet_mul_derivative(_12229, _12230, intervalFailed, optical_product_upper);
    Interval _12231 = _15710;
    Interval _12232 = _15526;
    Interval _12233 = _15691;
    Interval _15711 = jet_mul_derivative(_12232, _12233, intervalFailed, optical_product_upper);
    Interval _12234 = _15711;
    Interval _15712 = jet_add_derivative(_12231, _12234, intervalFailed);
    OpticalJet3 param_var_n = OpticalJet3{ OpticalJet{ _15692, _15695, _15698 }, OpticalJet{ _15699, _15702, _15705 }, OpticalJet{ _15706, _15709, _15712 } };
    OpticalJet3 param_var_direction = direction;
    OpticalJet3 _15718 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
    return _15718;
}

static __attribute__((noinline))
bool optical_jet_forward(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread OpticalJetRay& ray, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.bias0 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    ray.depth = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    bool4 _6361 = isnan(box);
    bool4 _6362 = isinf(box);
    bool _6372;
    if (all(not(bool4(_6361.x || _6362.x, _6361.y || _6362.y, _6361.z || _6362.z, _6361.w || _6362.w))))
    {
        _6372 = any(box.xy > box.zw);
    }
    else
    {
        _6372 = true;
    }
    bool _6378;
    if (!_6372)
    {
        _6378 = any(box.xy < float2(0.0));
    }
    else
    {
        _6378 = true;
    }
    bool _6386;
    if (!_6378)
    {
        _6386 = box.z >= receiver.extentClip.x;
    }
    else
    {
        _6386 = true;
    }
    bool _6394;
    if (!_6386)
    {
        _6394 = box.w >= receiver.extentClip.y;
    }
    else
    {
        _6394 = true;
    }
    bool _6399;
    if (!_6394)
    {
        _6399 = control.x > 4u;
    }
    else
    {
        _6399 = true;
    }
    bool _6408;
    if (!_6399)
    {
        _6408 = (control.y >> (control.x & 31u)) != 0u;
    }
    else
    {
        _6408 = true;
    }
    if (_6408)
    {
        return false;
    }
    Interval _6350 = Interval{ box.x, box.z };
    Interval _6351 = Interval{ as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u) };
    Interval _6432 = iadd(_6350, _6351, intervalFailed);
    Interval _6352 = Interval{ 1.0, 1.0 };
    Interval _6353 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6434 = jet_add_derivative(_6352, _6353, intervalFailed);
    Interval _6354 = Interval{ 0.0, 0.0 };
    Interval _6355 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6436 = jet_add_derivative(_6354, _6355, intervalFailed);
    Interval _6336 = _6432;
    Interval _6337 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6442 = idiv(_6336, _6337, intervalFailed, interval_divide_upper);
    bool _6449;
    if (!intervalFailed)
    {
        _6449 = intervalFailed;
    }
    else
    {
        _6449 = false;
    }
    bool _6454;
    if (_6449)
    {
        _6454 = jetFailureSite == 0u;
    }
    else
    {
        _6454 = false;
    }
    if (_6454)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6432.lo, _6432.hi, receiver.projection.x, receiver.projection.x);
    }
    Interval _6338 = _6434;
    Interval _6339 = _6442;
    Interval _6340 = Interval{ 0.0, 0.0 };
    Interval _6458 = jet_mul_derivative(_6339, _6340, intervalFailed, optical_product_upper);
    Interval _6341 = Interval{ as_type<float>(as_type<uint>(_6458.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6458.lo) ^ 2147483648u) };
    Interval _6468 = jet_add_derivative(_6338, _6341, intervalFailed);
    Interval _6342 = _6468;
    Interval _6343 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6470 = jet_div_derivative(_6342, _6343, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6344 = _6436;
    Interval _6345 = _6442;
    Interval _6346 = Interval{ 0.0, 0.0 };
    Interval _6471 = jet_mul_derivative(_6345, _6346, intervalFailed, optical_product_upper);
    Interval _6347 = Interval{ as_type<float>(as_type<uint>(_6471.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6471.lo) ^ 2147483648u) };
    Interval _6481 = jet_add_derivative(_6344, _6347, intervalFailed);
    Interval _6348 = _6481;
    Interval _6349 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6483 = jet_div_derivative(_6348, _6349, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6330 = Interval{ box.y, box.w };
    Interval _6331 = Interval{ as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u) };
    Interval _6499 = iadd(_6330, _6331, intervalFailed);
    Interval _6332 = Interval{ 0.0, 0.0 };
    Interval _6333 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6501 = jet_add_derivative(_6332, _6333, intervalFailed);
    Interval _6334 = Interval{ 1.0, 1.0 };
    Interval _6335 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6503 = jet_add_derivative(_6334, _6335, intervalFailed);
    Interval _6316 = _6499;
    Interval _6317 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6509 = idiv(_6316, _6317, intervalFailed, interval_divide_upper);
    bool _6516;
    if (!intervalFailed)
    {
        _6516 = intervalFailed;
    }
    else
    {
        _6516 = false;
    }
    bool _6521;
    if (_6516)
    {
        _6521 = jetFailureSite == 0u;
    }
    else
    {
        _6521 = false;
    }
    if (_6521)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6499.lo, _6499.hi, receiver.projection.y, receiver.projection.y);
    }
    Interval _6318 = _6501;
    Interval _6319 = _6509;
    Interval _6320 = Interval{ 0.0, 0.0 };
    Interval _6525 = jet_mul_derivative(_6319, _6320, intervalFailed, optical_product_upper);
    Interval _6321 = Interval{ as_type<float>(as_type<uint>(_6525.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6525.lo) ^ 2147483648u) };
    Interval _6535 = jet_add_derivative(_6318, _6321, intervalFailed);
    Interval _6322 = _6535;
    Interval _6323 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6537 = jet_div_derivative(_6322, _6323, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6324 = _6503;
    Interval _6325 = _6509;
    Interval _6326 = Interval{ 0.0, 0.0 };
    Interval _6538 = jet_mul_derivative(_6325, _6326, intervalFailed, optical_product_upper);
    Interval _6327 = Interval{ as_type<float>(as_type<uint>(_6538.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6538.lo) ^ 2147483648u) };
    Interval _6548 = jet_add_derivative(_6324, _6327, intervalFailed);
    Interval _6328 = _6548;
    Interval _6329 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6550 = jet_div_derivative(_6328, _6329, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    bool _6557;
    if (_6442.lo <= 0.0)
    {
        _6557 = _6442.hi >= 0.0;
    }
    else
    {
        _6557 = false;
    }
    float _6564;
    if (_6557)
    {
        _6564 = 0.0;
    }
    else
    {
        _6564 = precise::min(abs(_6442.lo), abs(_6442.hi));
    }
    float _6567 = precise::max(abs(_6442.lo), abs(_6442.hi));
    float _6306 = spvFMul(_6564, _6564);
    float _6568 = interval_down(_6306, intervalFailed);
    float _6307 = spvFMul(_6567, _6567);
    float _6570 = interval_up(_6307, intervalFailed);
    Interval _6308 = Interval{ 2.0, 2.0 };
    Interval _6309 = _6442;
    Interval _6571 = imul(_6308, _6309, intervalFailed, optical_product_upper);
    Interval _6310 = _6571;
    Interval _6311 = _6470;
    Interval _6572 = jet_mul_derivative(_6310, _6311, intervalFailed, optical_product_upper);
    Interval _6312 = Interval{ 2.0, 2.0 };
    Interval _6313 = _6442;
    Interval _6573 = imul(_6312, _6313, intervalFailed, optical_product_upper);
    Interval _6314 = _6573;
    Interval _6315 = _6483;
    Interval _6574 = jet_mul_derivative(_6314, _6315, intervalFailed, optical_product_upper);
    bool _6581;
    if (_6509.lo <= 0.0)
    {
        _6581 = _6509.hi >= 0.0;
    }
    else
    {
        _6581 = false;
    }
    float _6588;
    if (_6581)
    {
        _6588 = 0.0;
    }
    else
    {
        _6588 = precise::min(abs(_6509.lo), abs(_6509.hi));
    }
    float _6591 = precise::max(abs(_6509.lo), abs(_6509.hi));
    float _6296 = spvFMul(_6588, _6588);
    float _6592 = interval_down(_6296, intervalFailed);
    float _6297 = spvFMul(_6591, _6591);
    float _6594 = interval_up(_6297, intervalFailed);
    Interval _6298 = Interval{ 2.0, 2.0 };
    Interval _6299 = _6509;
    Interval _6595 = imul(_6298, _6299, intervalFailed, optical_product_upper);
    Interval _6300 = _6595;
    Interval _6301 = _6537;
    Interval _6596 = jet_mul_derivative(_6300, _6301, intervalFailed, optical_product_upper);
    Interval _6302 = Interval{ 2.0, 2.0 };
    Interval _6303 = _6509;
    Interval _6597 = imul(_6302, _6303, intervalFailed, optical_product_upper);
    Interval _6304 = _6597;
    Interval _6305 = _6550;
    Interval _6598 = jet_mul_derivative(_6304, _6305, intervalFailed, optical_product_upper);
    Interval _6290 = Interval{ precise::max(0.0, _6568), _6570 };
    Interval _6291 = Interval{ precise::max(0.0, _6592), _6594 };
    Interval _6601 = iadd(_6290, _6291, intervalFailed);
    Interval _6292 = _6572;
    Interval _6293 = _6596;
    Interval _6602 = jet_add_derivative(_6292, _6293, intervalFailed);
    Interval _6294 = _6574;
    Interval _6295 = _6598;
    Interval _6603 = jet_add_derivative(_6294, _6295, intervalFailed);
    bool _6608;
    if (1.0 <= 0.0)
    {
        _6608 = 1.0 >= 0.0;
    }
    else
    {
        _6608 = false;
    }
    float _6615;
    if (_6608)
    {
        _6615 = 0.0;
    }
    else
    {
        _6615 = precise::min(abs(1.0), abs(1.0));
    }
    float _6618 = precise::max(abs(1.0), abs(1.0));
    float _6280 = spvFMul(_6615, _6615);
    float _6619 = interval_down(_6280, intervalFailed);
    float _6281 = spvFMul(_6618, _6618);
    float _6621 = interval_up(_6281, intervalFailed);
    Interval _6282 = Interval{ 2.0, 2.0 };
    Interval _6283 = Interval{ 1.0, 1.0 };
    Interval _6622 = imul(_6282, _6283, intervalFailed, optical_product_upper);
    Interval _6284 = _6622;
    Interval _6285 = Interval{ 0.0, 0.0 };
    Interval _6623 = jet_mul_derivative(_6284, _6285, intervalFailed, optical_product_upper);
    Interval _6286 = Interval{ 2.0, 2.0 };
    Interval _6287 = Interval{ 1.0, 1.0 };
    Interval _6624 = imul(_6286, _6287, intervalFailed, optical_product_upper);
    Interval _6288 = _6624;
    Interval _6289 = Interval{ 0.0, 0.0 };
    Interval _6625 = jet_mul_derivative(_6288, _6289, intervalFailed, optical_product_upper);
    Interval _6274 = _6601;
    Interval _6275 = Interval{ precise::max(0.0, _6619), _6621 };
    Interval _6627 = iadd(_6274, _6275, intervalFailed);
    Interval _6276 = _6602;
    Interval _6277 = _6623;
    Interval _6628 = jet_add_derivative(_6276, _6277, intervalFailed);
    Interval _6278 = _6603;
    Interval _6279 = _6625;
    Interval _6629 = jet_add_derivative(_6278, _6279, intervalFailed);
    Interval _6265 = _6627;
    Interval _6631 = isqrt(_6265, intervalFailed);
    bool _6639;
    if (!intervalFailed)
    {
        _6639 = intervalFailed;
    }
    else
    {
        _6639 = false;
    }
    bool _6644;
    if (_6639)
    {
        _6644 = jetFailureSite == 0u;
    }
    else
    {
        _6644 = false;
    }
    if (_6644)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_6627.lo, _6627.hi, 0.0, 0.0);
    }
    if (_6631.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _6266 = Interval{ 2.0, 2.0 };
    Interval _6267 = _6631;
    Interval _6651 = imul(_6266, _6267, intervalFailed, optical_product_upper);
    Interval _6268 = Interval{ 1.0, 1.0 };
    Interval _6269 = _6651;
    Interval _6653 = idiv(_6268, _6269, intervalFailed, interval_divide_upper);
    bool _6660;
    if (!intervalFailed)
    {
        _6660 = intervalFailed;
    }
    else
    {
        _6660 = false;
    }
    bool _6665;
    if (_6660)
    {
        _6665 = jetFailureSite == 0u;
    }
    else
    {
        _6665 = false;
    }
    if (_6665)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _6651.lo, _6651.hi);
    }
    Interval _6270 = _6653;
    Interval _6271 = _6628;
    Interval _6669 = jet_mul_derivative(_6270, _6271, intervalFailed, optical_product_upper);
    Interval _6272 = _6653;
    Interval _6273 = _6629;
    Interval _6670 = jet_mul_derivative(_6272, _6273, intervalFailed, optical_product_upper);
    Interval _6251 = Interval{ 1.0, 1.0 };
    Interval _6252 = _6631;
    Interval _6672 = idiv(_6251, _6252, intervalFailed, interval_divide_upper);
    bool _6679;
    if (!intervalFailed)
    {
        _6679 = intervalFailed;
    }
    else
    {
        _6679 = false;
    }
    bool _6684;
    if (_6679)
    {
        _6684 = jetFailureSite == 0u;
    }
    else
    {
        _6684 = false;
    }
    if (_6684)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _6631.lo, _6631.hi);
    }
    Interval _6253 = Interval{ 0.0, 0.0 };
    Interval _6254 = _6672;
    Interval _6255 = _6669;
    Interval _6688 = jet_mul_derivative(_6254, _6255, intervalFailed, optical_product_upper);
    Interval _6256 = Interval{ as_type<float>(as_type<uint>(_6688.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6688.lo) ^ 2147483648u) };
    Interval _6698 = jet_add_derivative(_6253, _6256, intervalFailed);
    Interval _6257 = _6698;
    Interval _6258 = _6631;
    Interval _6699 = jet_div_derivative(_6257, _6258, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6259 = Interval{ 0.0, 0.0 };
    Interval _6260 = _6672;
    Interval _6261 = _6670;
    Interval _6700 = jet_mul_derivative(_6260, _6261, intervalFailed, optical_product_upper);
    Interval _6262 = Interval{ as_type<float>(as_type<uint>(_6700.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6700.lo) ^ 2147483648u) };
    Interval _6710 = jet_add_derivative(_6259, _6262, intervalFailed);
    Interval _6263 = _6710;
    Interval _6264 = _6631;
    Interval _6711 = jet_div_derivative(_6263, _6264, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6237 = _6442;
    Interval _6238 = _6672;
    Interval _6712 = imul(_6237, _6238, intervalFailed, optical_product_upper);
    Interval _6239 = _6470;
    Interval _6240 = _6672;
    Interval _6713 = jet_mul_derivative(_6239, _6240, intervalFailed, optical_product_upper);
    Interval _6241 = _6713;
    Interval _6242 = _6442;
    Interval _6243 = _6699;
    Interval _6714 = jet_mul_derivative(_6242, _6243, intervalFailed, optical_product_upper);
    Interval _6244 = _6714;
    Interval _6715 = jet_add_derivative(_6241, _6244, intervalFailed);
    Interval _6245 = _6483;
    Interval _6246 = _6672;
    Interval _6716 = jet_mul_derivative(_6245, _6246, intervalFailed, optical_product_upper);
    Interval _6247 = _6716;
    Interval _6248 = _6442;
    Interval _6249 = _6711;
    Interval _6717 = jet_mul_derivative(_6248, _6249, intervalFailed, optical_product_upper);
    Interval _6250 = _6717;
    Interval _6718 = jet_add_derivative(_6247, _6250, intervalFailed);
    Interval _6223 = _6509;
    Interval _6224 = _6672;
    Interval _6719 = imul(_6223, _6224, intervalFailed, optical_product_upper);
    Interval _6225 = _6537;
    Interval _6226 = _6672;
    Interval _6720 = jet_mul_derivative(_6225, _6226, intervalFailed, optical_product_upper);
    Interval _6227 = _6720;
    Interval _6228 = _6509;
    Interval _6229 = _6699;
    Interval _6721 = jet_mul_derivative(_6228, _6229, intervalFailed, optical_product_upper);
    Interval _6230 = _6721;
    Interval _6722 = jet_add_derivative(_6227, _6230, intervalFailed);
    Interval _6231 = _6550;
    Interval _6232 = _6672;
    Interval _6723 = jet_mul_derivative(_6231, _6232, intervalFailed, optical_product_upper);
    Interval _6233 = _6723;
    Interval _6234 = _6509;
    Interval _6235 = _6711;
    Interval _6724 = jet_mul_derivative(_6234, _6235, intervalFailed, optical_product_upper);
    Interval _6236 = _6724;
    Interval _6725 = jet_add_derivative(_6233, _6236, intervalFailed);
    Interval _6209 = Interval{ 1.0, 1.0 };
    Interval _6210 = _6672;
    Interval _6726 = imul(_6209, _6210, intervalFailed, optical_product_upper);
    Interval _6211 = Interval{ 0.0, 0.0 };
    Interval _6212 = _6672;
    Interval _6727 = jet_mul_derivative(_6211, _6212, intervalFailed, optical_product_upper);
    Interval _6213 = _6727;
    Interval _6214 = Interval{ 1.0, 1.0 };
    Interval _6215 = _6699;
    Interval _6728 = jet_mul_derivative(_6214, _6215, intervalFailed, optical_product_upper);
    Interval _6216 = _6728;
    Interval _6729 = jet_add_derivative(_6213, _6216, intervalFailed);
    Interval _6217 = Interval{ 0.0, 0.0 };
    Interval _6218 = _6672;
    Interval _6730 = jet_mul_derivative(_6217, _6218, intervalFailed, optical_product_upper);
    Interval _6219 = _6730;
    Interval _6220 = Interval{ 1.0, 1.0 };
    Interval _6221 = _6711;
    Interval _6731 = jet_mul_derivative(_6220, _6221, intervalFailed, optical_product_upper);
    Interval _6222 = _6731;
    Interval _6732 = jet_add_derivative(_6219, _6222, intervalFailed);
    float _6751;
    float _6753;
    float _6755;
    float _6757;
    float _6759;
    float _6761;
    float _6763;
    float _6765;
    float _6767;
    float _6769;
    float _6771;
    float _6773;
    float _6775;
    float _6777;
    float _6779;
    float _6781;
    float _6783;
    float _6785;
    float _6787;
    float _6789;
    float _6791;
    float _6793;
    float _6795;
    float _6797;
    float _6799;
    float _6801;
    float _6803;
    float _6805;
    float _6807;
    float _6809;
    float _6811;
    float _6813;
    float _6815;
    float _6817;
    float _6819;
    float _6821;
    float _6823;
    float _6825;
    float _6827;
    float _6829;
    float _6831;
    float _6833;
    _6751 = 0.0;
    _6753 = 0.0;
    _6755 = 0.0;
    _6757 = 0.0;
    _6759 = 0.0;
    _6761 = 0.0;
    _6763 = _6712.lo;
    _6765 = _6712.hi;
    _6767 = _6715.lo;
    _6769 = _6715.hi;
    _6771 = _6718.lo;
    _6773 = _6718.hi;
    _6775 = _6719.lo;
    _6777 = _6719.hi;
    _6779 = _6722.lo;
    _6781 = _6722.hi;
    _6783 = _6725.lo;
    _6785 = _6725.hi;
    _6787 = _6726.lo;
    _6789 = _6726.hi;
    _6791 = _6729.lo;
    _6793 = _6729.hi;
    _6795 = _6732.lo;
    _6797 = _6732.hi;
    _6799 = 0.0;
    _6801 = 0.0;
    _6803 = 0.0;
    _6805 = 0.0;
    _6807 = 0.0;
    _6809 = 0.0;
    _6811 = 0.0;
    _6813 = 0.0;
    _6815 = 0.0;
    _6817 = 0.0;
    _6819 = 0.0;
    _6821 = 0.0;
    _6823 = 0.0;
    _6825 = 0.0;
    _6827 = 0.0;
    _6829 = 0.0;
    _6831 = 0.0;
    _6833 = 0.0;
    float _6752;
    float _6754;
    float _6756;
    float _6758;
    float _6760;
    float _6762;
    float _6800;
    float _6802;
    float _6804;
    float _6806;
    float _6808;
    float _6810;
    float _6812;
    float _6814;
    float _6816;
    float _6818;
    float _6820;
    float _6822;
    float _6824;
    float _6826;
    float _6828;
    float _6830;
    float _6832;
    float _6834;
    OpticalJet3 hit;
    float _6764;
    float _6766;
    float _6768;
    float _6770;
    float _6772;
    float _6774;
    float _6776;
    float _6778;
    float _6780;
    float _6782;
    float _6784;
    float _6786;
    float _6788;
    float _6790;
    float _6792;
    float _6794;
    float _6796;
    float _6798;
    for (uint _6835 = 0u; _6835 <= control.x; _6751 = _6752, _6753 = _6754, _6755 = _6756, _6757 = _6758, _6759 = _6760, _6761 = _6762, _6763 = _6764, _6765 = _6766, _6767 = _6768, _6769 = _6770, _6771 = _6772, _6773 = _6774, _6775 = _6776, _6777 = _6778, _6779 = _6780, _6781 = _6782, _6783 = _6784, _6785 = _6786, _6787 = _6788, _6789 = _6790, _6791 = _6792, _6793 = _6794, _6795 = _6796, _6797 = _6798, _6799 = _6800, _6801 = _6802, _6803 = _6804, _6805 = _6806, _6807 = _6808, _6809 = _6810, _6811 = _6812, _6813 = _6814, _6815 = _6816, _6817 = _6818, _6819 = _6820, _6821 = _6822, _6823 = _6824, _6825 = _6826, _6827 = _6828, _6829 = _6830, _6831 = _6832, _6833 = _6834, _6835++)
    {
        bool _6839 = _6835 == 0u;
        bool _6849;
        if (_6839)
        {
            _6849 = control.z != 0u;
        }
        else
        {
            _6849 = (control.y & (1u << ((_6835 - 1u) & 31u))) != 0u;
        }
        float4 _6861;
        float4 _6862;
        float4 _6863;
        if (_6839)
        {
            _6861 = receiver.a;
            _6862 = receiver.b;
            _6863 = receiver.c;
        }
        else
        {
            uint _1753 = _6835 - 1u;
            _6861 = planes[_1753].a;
            _6862 = planes[_1753].b;
            _6863 = planes[_1753].c;
        }
        float _6915;
        float _6916;
        float _6917;
        float _6918;
        float _6919;
        float _6920;
        float _6921;
        float _6922;
        float _6923;
        float _6924;
        float _6925;
        float _6926;
        float _6927;
        float _6928;
        float _6929;
        float _6930;
        float _6931;
        float _6932;
        if (_6849)
        {
            _6915 = liquid.planeNormal.x;
            _6916 = liquid.planeNormal.x;
            _6917 = 0.0;
            _6918 = 0.0;
            _6919 = 0.0;
            _6920 = 0.0;
            _6921 = liquid.planeNormal.y;
            _6922 = liquid.planeNormal.y;
            _6923 = 0.0;
            _6924 = 0.0;
            _6925 = 0.0;
            _6926 = 0.0;
            _6927 = liquid.planeNormal.z;
            _6928 = liquid.planeNormal.z;
            _6929 = 0.0;
            _6930 = 0.0;
            _6931 = 0.0;
            _6932 = 0.0;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ _6861, _6862, _6863 };
            OpticalJet3 _6865 = jet_plane_normal(param_var_plane, intervalFailed, optical_product_upper, interval_divide_upper);
            OpticalJet3 param_var_n = _6865;
            OpticalJet3 param_var_direction = OpticalJet3{ OpticalJet{ Interval{ _6763, _6765 }, Interval{ _6767, _6769 }, Interval{ _6771, _6773 } }, OpticalJet{ Interval{ _6775, _6777 }, Interval{ _6779, _6781 }, Interval{ _6783, _6785 } }, OpticalJet{ Interval{ _6787, _6789 }, Interval{ _6791, _6793 }, Interval{ _6795, _6797 } } };
            OpticalJet3 _6879 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
            _6915 = _6879.x.v.lo;
            _6916 = _6879.x.v.hi;
            _6917 = _6879.x.dx.lo;
            _6918 = _6879.x.dx.hi;
            _6919 = _6879.x.dy.lo;
            _6920 = _6879.x.dy.hi;
            _6921 = _6879.y.v.lo;
            _6922 = _6879.y.v.hi;
            _6923 = _6879.y.dx.lo;
            _6924 = _6879.y.dx.hi;
            _6925 = _6879.y.dy.lo;
            _6926 = _6879.y.dy.hi;
            _6927 = _6879.z.v.lo;
            _6928 = _6879.z.v.hi;
            _6929 = _6879.z.dx.lo;
            _6930 = _6879.z.dx.hi;
            _6931 = _6879.z.dy.lo;
            _6932 = _6879.z.dy.hi;
        }
        Interval _6195 = Interval{ _6763, _6765 };
        Interval _6196 = Interval{ _6915, _6916 };
        Interval _6935 = imul(_6195, _6196, intervalFailed, optical_product_upper);
        Interval _6197 = Interval{ _6767, _6769 };
        Interval _6198 = Interval{ _6915, _6916 };
        Interval _6938 = jet_mul_derivative(_6197, _6198, intervalFailed, optical_product_upper);
        Interval _6199 = _6938;
        Interval _6200 = Interval{ _6763, _6765 };
        Interval _6201 = Interval{ _6917, _6918 };
        Interval _6941 = jet_mul_derivative(_6200, _6201, intervalFailed, optical_product_upper);
        Interval _6202 = _6941;
        Interval _6942 = jet_add_derivative(_6199, _6202, intervalFailed);
        Interval _6203 = Interval{ _6771, _6773 };
        Interval _6204 = Interval{ _6915, _6916 };
        Interval _6945 = jet_mul_derivative(_6203, _6204, intervalFailed, optical_product_upper);
        Interval _6205 = _6945;
        Interval _6206 = Interval{ _6763, _6765 };
        Interval _6207 = Interval{ _6919, _6920 };
        Interval _6948 = jet_mul_derivative(_6206, _6207, intervalFailed, optical_product_upper);
        Interval _6208 = _6948;
        Interval _6949 = jet_add_derivative(_6205, _6208, intervalFailed);
        Interval _6181 = Interval{ _6775, _6777 };
        Interval _6182 = Interval{ _6921, _6922 };
        Interval _6952 = imul(_6181, _6182, intervalFailed, optical_product_upper);
        Interval _6183 = Interval{ _6779, _6781 };
        Interval _6184 = Interval{ _6921, _6922 };
        Interval _6955 = jet_mul_derivative(_6183, _6184, intervalFailed, optical_product_upper);
        Interval _6185 = _6955;
        Interval _6186 = Interval{ _6775, _6777 };
        Interval _6187 = Interval{ _6923, _6924 };
        Interval _6958 = jet_mul_derivative(_6186, _6187, intervalFailed, optical_product_upper);
        Interval _6188 = _6958;
        Interval _6959 = jet_add_derivative(_6185, _6188, intervalFailed);
        Interval _6189 = Interval{ _6783, _6785 };
        Interval _6190 = Interval{ _6921, _6922 };
        Interval _6962 = jet_mul_derivative(_6189, _6190, intervalFailed, optical_product_upper);
        Interval _6191 = _6962;
        Interval _6192 = Interval{ _6775, _6777 };
        Interval _6193 = Interval{ _6925, _6926 };
        Interval _6965 = jet_mul_derivative(_6192, _6193, intervalFailed, optical_product_upper);
        Interval _6194 = _6965;
        Interval _6966 = jet_add_derivative(_6191, _6194, intervalFailed);
        Interval _6175 = _6935;
        Interval _6176 = _6952;
        Interval _6967 = iadd(_6175, _6176, intervalFailed);
        Interval _6177 = _6942;
        Interval _6178 = _6959;
        Interval _6968 = jet_add_derivative(_6177, _6178, intervalFailed);
        Interval _6179 = _6949;
        Interval _6180 = _6966;
        Interval _6969 = jet_add_derivative(_6179, _6180, intervalFailed);
        Interval _6161 = Interval{ _6787, _6789 };
        Interval _6162 = Interval{ _6927, _6928 };
        Interval _6972 = imul(_6161, _6162, intervalFailed, optical_product_upper);
        Interval _6163 = Interval{ _6791, _6793 };
        Interval _6164 = Interval{ _6927, _6928 };
        Interval _6975 = jet_mul_derivative(_6163, _6164, intervalFailed, optical_product_upper);
        Interval _6165 = _6975;
        Interval _6166 = Interval{ _6787, _6789 };
        Interval _6167 = Interval{ _6929, _6930 };
        Interval _6978 = jet_mul_derivative(_6166, _6167, intervalFailed, optical_product_upper);
        Interval _6168 = _6978;
        Interval _6979 = jet_add_derivative(_6165, _6168, intervalFailed);
        Interval _6169 = Interval{ _6795, _6797 };
        Interval _6170 = Interval{ _6927, _6928 };
        Interval _6982 = jet_mul_derivative(_6169, _6170, intervalFailed, optical_product_upper);
        Interval _6171 = _6982;
        Interval _6172 = Interval{ _6787, _6789 };
        Interval _6173 = Interval{ _6931, _6932 };
        Interval _6985 = jet_mul_derivative(_6172, _6173, intervalFailed, optical_product_upper);
        Interval _6174 = _6985;
        Interval _6986 = jet_add_derivative(_6171, _6174, intervalFailed);
        Interval _6155 = _6967;
        Interval _6156 = _6972;
        Interval _6987 = iadd(_6155, _6156, intervalFailed);
        Interval _6157 = _6968;
        Interval _6158 = _6979;
        Interval _6988 = jet_add_derivative(_6157, _6158, intervalFailed);
        Interval _6159 = _6969;
        Interval _6160 = _6986;
        Interval _6989 = jet_add_derivative(_6159, _6160, intervalFailed);
        bool _6996;
        if (_6987.lo <= 0.0)
        {
            _6996 = _6987.hi >= 0.0;
        }
        else
        {
            _6996 = false;
        }
        float _7003;
        if (_6996)
        {
            _7003 = 0.0;
        }
        else
        {
            _7003 = precise::min(abs(_6987.lo), abs(_6987.hi));
        }
        bool _7005;
        if (!_6839)
        {
            _7005 = _6849;
        }
        else
        {
            _7005 = false;
        }
        float _7006;
        if (_7005)
        {
            _7006 = 9.9999999392252902907785028219223e-09;
        }
        else
        {
            _7006 = 9.9999999600419720025001879548654e-13;
        }
        float _6153 = _7006;
        __attribute__((unused)) float _7007 = interval_down(_6153, intervalFailed);
        float _6154 = _7006;
        float _7008 = interval_up(_6154, intervalFailed);
        if (_7003 <= _7008)
        {
            return false;
        }
        float _7603;
        float _7604;
        float _7605;
        float _7606;
        float _7607;
        float _7608;
        if (_6839)
        {
            float3 _7249;
            if (_6849)
            {
                _7249 = liquid.planePoint.xyz;
            }
            else
            {
                _7249 = _6861.xyz;
            }
            Interval _6139 = Interval{ _7249.x, _7249.x };
            Interval _6140 = Interval{ _6915, _6916 };
            Interval _7255 = imul(_6139, _6140, intervalFailed, optical_product_upper);
            Interval _6141 = Interval{ 0.0, 0.0 };
            Interval _6142 = Interval{ _6915, _6916 };
            Interval _7257 = jet_mul_derivative(_6141, _6142, intervalFailed, optical_product_upper);
            Interval _6143 = _7257;
            Interval _6144 = Interval{ _7249.x, _7249.x };
            Interval _6145 = Interval{ _6917, _6918 };
            Interval _7260 = jet_mul_derivative(_6144, _6145, intervalFailed, optical_product_upper);
            Interval _6146 = _7260;
            Interval _7261 = jet_add_derivative(_6143, _6146, intervalFailed);
            Interval _6147 = Interval{ 0.0, 0.0 };
            Interval _6148 = Interval{ _6915, _6916 };
            Interval _7263 = jet_mul_derivative(_6147, _6148, intervalFailed, optical_product_upper);
            Interval _6149 = _7263;
            Interval _6150 = Interval{ _7249.x, _7249.x };
            Interval _6151 = Interval{ _6919, _6920 };
            Interval _7266 = jet_mul_derivative(_6150, _6151, intervalFailed, optical_product_upper);
            Interval _6152 = _7266;
            Interval _7267 = jet_add_derivative(_6149, _6152, intervalFailed);
            Interval _6125 = Interval{ _7249.y, _7249.y };
            Interval _6126 = Interval{ _6921, _6922 };
            Interval _7270 = imul(_6125, _6126, intervalFailed, optical_product_upper);
            Interval _6127 = Interval{ 0.0, 0.0 };
            Interval _6128 = Interval{ _6921, _6922 };
            Interval _7272 = jet_mul_derivative(_6127, _6128, intervalFailed, optical_product_upper);
            Interval _6129 = _7272;
            Interval _6130 = Interval{ _7249.y, _7249.y };
            Interval _6131 = Interval{ _6923, _6924 };
            Interval _7275 = jet_mul_derivative(_6130, _6131, intervalFailed, optical_product_upper);
            Interval _6132 = _7275;
            Interval _7276 = jet_add_derivative(_6129, _6132, intervalFailed);
            Interval _6133 = Interval{ 0.0, 0.0 };
            Interval _6134 = Interval{ _6921, _6922 };
            Interval _7278 = jet_mul_derivative(_6133, _6134, intervalFailed, optical_product_upper);
            Interval _6135 = _7278;
            Interval _6136 = Interval{ _7249.y, _7249.y };
            Interval _6137 = Interval{ _6925, _6926 };
            Interval _7281 = jet_mul_derivative(_6136, _6137, intervalFailed, optical_product_upper);
            Interval _6138 = _7281;
            Interval _7282 = jet_add_derivative(_6135, _6138, intervalFailed);
            Interval _6119 = _7255;
            Interval _6120 = _7270;
            Interval _7283 = iadd(_6119, _6120, intervalFailed);
            Interval _6121 = _7261;
            Interval _6122 = _7276;
            Interval _7284 = jet_add_derivative(_6121, _6122, intervalFailed);
            Interval _6123 = _7267;
            Interval _6124 = _7282;
            Interval _7285 = jet_add_derivative(_6123, _6124, intervalFailed);
            Interval _6105 = Interval{ _7249.z, _7249.z };
            Interval _6106 = Interval{ _6927, _6928 };
            Interval _7288 = imul(_6105, _6106, intervalFailed, optical_product_upper);
            Interval _6107 = Interval{ 0.0, 0.0 };
            Interval _6108 = Interval{ _6927, _6928 };
            Interval _7290 = jet_mul_derivative(_6107, _6108, intervalFailed, optical_product_upper);
            Interval _6109 = _7290;
            Interval _6110 = Interval{ _7249.z, _7249.z };
            Interval _6111 = Interval{ _6929, _6930 };
            Interval _7293 = jet_mul_derivative(_6110, _6111, intervalFailed, optical_product_upper);
            Interval _6112 = _7293;
            Interval _7294 = jet_add_derivative(_6109, _6112, intervalFailed);
            Interval _6113 = Interval{ 0.0, 0.0 };
            Interval _6114 = Interval{ _6927, _6928 };
            Interval _7296 = jet_mul_derivative(_6113, _6114, intervalFailed, optical_product_upper);
            Interval _6115 = _7296;
            Interval _6116 = Interval{ _7249.z, _7249.z };
            Interval _6117 = Interval{ _6931, _6932 };
            Interval _7299 = jet_mul_derivative(_6116, _6117, intervalFailed, optical_product_upper);
            Interval _6118 = _7299;
            Interval _7300 = jet_add_derivative(_6115, _6118, intervalFailed);
            Interval _6099 = _7283;
            Interval _6100 = _7288;
            Interval _7301 = iadd(_6099, _6100, intervalFailed);
            Interval _6101 = _7284;
            Interval _6102 = _7294;
            Interval _7302 = jet_add_derivative(_6101, _6102, intervalFailed);
            Interval _6103 = _7285;
            Interval _6104 = _7300;
            Interval _7303 = jet_add_derivative(_6103, _6104, intervalFailed);
            Interval _6085 = _6442;
            Interval _6086 = Interval{ _6915, _6916 };
            Interval _7305 = imul(_6085, _6086, intervalFailed, optical_product_upper);
            Interval _6087 = _6470;
            Interval _6088 = Interval{ _6915, _6916 };
            Interval _7307 = jet_mul_derivative(_6087, _6088, intervalFailed, optical_product_upper);
            Interval _6089 = _7307;
            Interval _6090 = _6442;
            Interval _6091 = Interval{ _6917, _6918 };
            Interval _7309 = jet_mul_derivative(_6090, _6091, intervalFailed, optical_product_upper);
            Interval _6092 = _7309;
            Interval _7310 = jet_add_derivative(_6089, _6092, intervalFailed);
            Interval _6093 = _6483;
            Interval _6094 = Interval{ _6915, _6916 };
            Interval _7312 = jet_mul_derivative(_6093, _6094, intervalFailed, optical_product_upper);
            Interval _6095 = _7312;
            Interval _6096 = _6442;
            Interval _6097 = Interval{ _6919, _6920 };
            Interval _7314 = jet_mul_derivative(_6096, _6097, intervalFailed, optical_product_upper);
            Interval _6098 = _7314;
            Interval _7315 = jet_add_derivative(_6095, _6098, intervalFailed);
            Interval _6071 = _6509;
            Interval _6072 = Interval{ _6921, _6922 };
            Interval _7317 = imul(_6071, _6072, intervalFailed, optical_product_upper);
            Interval _6073 = _6537;
            Interval _6074 = Interval{ _6921, _6922 };
            Interval _7319 = jet_mul_derivative(_6073, _6074, intervalFailed, optical_product_upper);
            Interval _6075 = _7319;
            Interval _6076 = _6509;
            Interval _6077 = Interval{ _6923, _6924 };
            Interval _7321 = jet_mul_derivative(_6076, _6077, intervalFailed, optical_product_upper);
            Interval _6078 = _7321;
            Interval _7322 = jet_add_derivative(_6075, _6078, intervalFailed);
            Interval _6079 = _6550;
            Interval _6080 = Interval{ _6921, _6922 };
            Interval _7324 = jet_mul_derivative(_6079, _6080, intervalFailed, optical_product_upper);
            Interval _6081 = _7324;
            Interval _6082 = _6509;
            Interval _6083 = Interval{ _6925, _6926 };
            Interval _7326 = jet_mul_derivative(_6082, _6083, intervalFailed, optical_product_upper);
            Interval _6084 = _7326;
            Interval _7327 = jet_add_derivative(_6081, _6084, intervalFailed);
            Interval _6065 = _7305;
            Interval _6066 = _7317;
            Interval _7328 = iadd(_6065, _6066, intervalFailed);
            Interval _6067 = _7310;
            Interval _6068 = _7322;
            Interval _7329 = jet_add_derivative(_6067, _6068, intervalFailed);
            Interval _6069 = _7315;
            Interval _6070 = _7327;
            Interval _7330 = jet_add_derivative(_6069, _6070, intervalFailed);
            Interval _6051 = Interval{ 1.0, 1.0 };
            Interval _6052 = Interval{ _6927, _6928 };
            Interval _7332 = imul(_6051, _6052, intervalFailed, optical_product_upper);
            Interval _6053 = Interval{ 0.0, 0.0 };
            Interval _6054 = Interval{ _6927, _6928 };
            Interval _7334 = jet_mul_derivative(_6053, _6054, intervalFailed, optical_product_upper);
            Interval _6055 = _7334;
            Interval _6056 = Interval{ 1.0, 1.0 };
            Interval _6057 = Interval{ _6929, _6930 };
            Interval _7336 = jet_mul_derivative(_6056, _6057, intervalFailed, optical_product_upper);
            Interval _6058 = _7336;
            Interval _7337 = jet_add_derivative(_6055, _6058, intervalFailed);
            Interval _6059 = Interval{ 0.0, 0.0 };
            Interval _6060 = Interval{ _6927, _6928 };
            Interval _7339 = jet_mul_derivative(_6059, _6060, intervalFailed, optical_product_upper);
            Interval _6061 = _7339;
            Interval _6062 = Interval{ 1.0, 1.0 };
            Interval _6063 = Interval{ _6931, _6932 };
            Interval _7341 = jet_mul_derivative(_6062, _6063, intervalFailed, optical_product_upper);
            Interval _6064 = _7341;
            Interval _7342 = jet_add_derivative(_6061, _6064, intervalFailed);
            Interval _6045 = _7328;
            Interval _6046 = _7332;
            Interval _7343 = iadd(_6045, _6046, intervalFailed);
            Interval _6047 = _7329;
            Interval _6048 = _7337;
            Interval _7344 = jet_add_derivative(_6047, _6048, intervalFailed);
            Interval _6049 = _7330;
            Interval _6050 = _7342;
            Interval _7345 = jet_add_derivative(_6049, _6050, intervalFailed);
            Interval _6031 = _7301;
            Interval _6032 = _7343;
            Interval _7347 = idiv(_6031, _6032, intervalFailed, interval_divide_upper);
            bool _7356;
            if (!intervalFailed)
            {
                _7356 = intervalFailed;
            }
            else
            {
                _7356 = false;
            }
            bool _7361;
            if (_7356)
            {
                _7361 = jetFailureSite == 0u;
            }
            else
            {
                _7361 = false;
            }
            if (_7361)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7301.lo, _7301.hi, _7343.lo, _7343.hi);
            }
            Interval _6033 = _7302;
            Interval _6034 = _7347;
            Interval _6035 = _7344;
            Interval _7365 = jet_mul_derivative(_6034, _6035, intervalFailed, optical_product_upper);
            Interval _6036 = Interval{ as_type<float>(as_type<uint>(_7365.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7365.lo) ^ 2147483648u) };
            Interval _7375 = jet_add_derivative(_6033, _6036, intervalFailed);
            Interval _6037 = _7375;
            Interval _6038 = _7343;
            Interval _7376 = jet_div_derivative(_6037, _6038, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _6039 = _7303;
            Interval _6040 = _7347;
            Interval _6041 = _7345;
            Interval _7377 = jet_mul_derivative(_6040, _6041, intervalFailed, optical_product_upper);
            Interval _6042 = Interval{ as_type<float>(as_type<uint>(_7377.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7377.lo) ^ 2147483648u) };
            Interval _7387 = jet_add_derivative(_6039, _6042, intervalFailed);
            Interval _6043 = _7387;
            Interval _6044 = _7343;
            Interval _7388 = jet_div_derivative(_6043, _6044, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ray.depth = OpticalJet{ _7347, _7376, _7388 };
            Interval _6017 = ray.depth.v;
            Interval _6018 = _6631;
            Interval _7396 = imul(_6017, _6018, intervalFailed, optical_product_upper);
            Interval _6019 = ray.depth.dx;
            Interval _6020 = _6631;
            Interval _7397 = jet_mul_derivative(_6019, _6020, intervalFailed, optical_product_upper);
            Interval _6021 = _7397;
            Interval _6022 = ray.depth.v;
            Interval _6023 = _6669;
            Interval _7398 = jet_mul_derivative(_6022, _6023, intervalFailed, optical_product_upper);
            Interval _6024 = _7398;
            Interval _7399 = jet_add_derivative(_6021, _6024, intervalFailed);
            Interval _6025 = ray.depth.dy;
            Interval _6026 = _6631;
            Interval _7400 = jet_mul_derivative(_6025, _6026, intervalFailed, optical_product_upper);
            Interval _6027 = _7400;
            Interval _6028 = ray.depth.v;
            Interval _6029 = _6670;
            Interval _7401 = jet_mul_derivative(_6028, _6029, intervalFailed, optical_product_upper);
            Interval _6030 = _7401;
            Interval _7402 = jet_add_derivative(_6027, _6030, intervalFailed);
            Interval _6003 = _6442;
            Interval _6004 = ray.depth.v;
            Interval _7414 = imul(_6003, _6004, intervalFailed, optical_product_upper);
            Interval _6005 = _6470;
            Interval _6006 = ray.depth.v;
            Interval _7415 = jet_mul_derivative(_6005, _6006, intervalFailed, optical_product_upper);
            Interval _6007 = _7415;
            Interval _6008 = _6442;
            Interval _6009 = ray.depth.dx;
            Interval _7416 = jet_mul_derivative(_6008, _6009, intervalFailed, optical_product_upper);
            Interval _6010 = _7416;
            Interval _7417 = jet_add_derivative(_6007, _6010, intervalFailed);
            Interval _6011 = _6483;
            Interval _6012 = ray.depth.v;
            Interval _7418 = jet_mul_derivative(_6011, _6012, intervalFailed, optical_product_upper);
            Interval _6013 = _7418;
            Interval _6014 = _6442;
            Interval _6015 = ray.depth.dy;
            Interval _7419 = jet_mul_derivative(_6014, _6015, intervalFailed, optical_product_upper);
            Interval _6016 = _7419;
            Interval _7420 = jet_add_derivative(_6013, _6016, intervalFailed);
            Interval _5989 = _6509;
            Interval _5990 = ray.depth.v;
            Interval _7424 = imul(_5989, _5990, intervalFailed, optical_product_upper);
            Interval _5991 = _6537;
            Interval _5992 = ray.depth.v;
            Interval _7425 = jet_mul_derivative(_5991, _5992, intervalFailed, optical_product_upper);
            Interval _5993 = _7425;
            Interval _5994 = _6509;
            Interval _5995 = ray.depth.dx;
            Interval _7426 = jet_mul_derivative(_5994, _5995, intervalFailed, optical_product_upper);
            Interval _5996 = _7426;
            Interval _7427 = jet_add_derivative(_5993, _5996, intervalFailed);
            Interval _5997 = _6550;
            Interval _5998 = ray.depth.v;
            Interval _7428 = jet_mul_derivative(_5997, _5998, intervalFailed, optical_product_upper);
            Interval _5999 = _7428;
            Interval _6000 = _6509;
            Interval _6001 = ray.depth.dy;
            Interval _7429 = jet_mul_derivative(_6000, _6001, intervalFailed, optical_product_upper);
            Interval _6002 = _7429;
            Interval _7430 = jet_add_derivative(_5999, _6002, intervalFailed);
            Interval _5975 = Interval{ 1.0, 1.0 };
            Interval _5976 = ray.depth.v;
            Interval _7434 = imul(_5975, _5976, intervalFailed, optical_product_upper);
            Interval _5977 = Interval{ 0.0, 0.0 };
            Interval _5978 = ray.depth.v;
            Interval _7435 = jet_mul_derivative(_5977, _5978, intervalFailed, optical_product_upper);
            Interval _5979 = _7435;
            Interval _5980 = Interval{ 1.0, 1.0 };
            Interval _5981 = ray.depth.dx;
            Interval _7436 = jet_mul_derivative(_5980, _5981, intervalFailed, optical_product_upper);
            Interval _5982 = _7436;
            Interval _7437 = jet_add_derivative(_5979, _5982, intervalFailed);
            Interval _5983 = Interval{ 0.0, 0.0 };
            Interval _5984 = ray.depth.v;
            Interval _7438 = jet_mul_derivative(_5983, _5984, intervalFailed, optical_product_upper);
            Interval _5985 = _7438;
            Interval _5986 = Interval{ 1.0, 1.0 };
            Interval _5987 = ray.depth.dy;
            Interval _7439 = jet_mul_derivative(_5986, _5987, intervalFailed, optical_product_upper);
            Interval _5988 = _7439;
            Interval _7440 = jet_add_derivative(_5985, _5988, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7414, _7417, _7420 }, OpticalJet{ _7424, _7427, _7430 }, OpticalJet{ _7434, _7437, _7440 } };
            float3 _7449;
            if (_6849)
            {
                _7449 = liquid.planePoint.xyz;
            }
            else
            {
                _7449 = _6861.xyz;
            }
            Interval _5961 = Interval{ _7449.x, _7449.x };
            Interval _5962 = Interval{ _6915, _6916 };
            Interval _7455 = imul(_5961, _5962, intervalFailed, optical_product_upper);
            Interval _5963 = Interval{ 0.0, 0.0 };
            Interval _5964 = Interval{ _6915, _6916 };
            Interval _7457 = jet_mul_derivative(_5963, _5964, intervalFailed, optical_product_upper);
            Interval _5965 = _7457;
            Interval _5966 = Interval{ _7449.x, _7449.x };
            Interval _5967 = Interval{ _6917, _6918 };
            Interval _7460 = jet_mul_derivative(_5966, _5967, intervalFailed, optical_product_upper);
            Interval _5968 = _7460;
            Interval _7461 = jet_add_derivative(_5965, _5968, intervalFailed);
            Interval _5969 = Interval{ 0.0, 0.0 };
            Interval _5970 = Interval{ _6915, _6916 };
            Interval _7463 = jet_mul_derivative(_5969, _5970, intervalFailed, optical_product_upper);
            Interval _5971 = _7463;
            Interval _5972 = Interval{ _7449.x, _7449.x };
            Interval _5973 = Interval{ _6919, _6920 };
            Interval _7466 = jet_mul_derivative(_5972, _5973, intervalFailed, optical_product_upper);
            Interval _5974 = _7466;
            Interval _7467 = jet_add_derivative(_5971, _5974, intervalFailed);
            Interval _5947 = Interval{ _7449.y, _7449.y };
            Interval _5948 = Interval{ _6921, _6922 };
            Interval _7470 = imul(_5947, _5948, intervalFailed, optical_product_upper);
            Interval _5949 = Interval{ 0.0, 0.0 };
            Interval _5950 = Interval{ _6921, _6922 };
            Interval _7472 = jet_mul_derivative(_5949, _5950, intervalFailed, optical_product_upper);
            Interval _5951 = _7472;
            Interval _5952 = Interval{ _7449.y, _7449.y };
            Interval _5953 = Interval{ _6923, _6924 };
            Interval _7475 = jet_mul_derivative(_5952, _5953, intervalFailed, optical_product_upper);
            Interval _5954 = _7475;
            Interval _7476 = jet_add_derivative(_5951, _5954, intervalFailed);
            Interval _5955 = Interval{ 0.0, 0.0 };
            Interval _5956 = Interval{ _6921, _6922 };
            Interval _7478 = jet_mul_derivative(_5955, _5956, intervalFailed, optical_product_upper);
            Interval _5957 = _7478;
            Interval _5958 = Interval{ _7449.y, _7449.y };
            Interval _5959 = Interval{ _6925, _6926 };
            Interval _7481 = jet_mul_derivative(_5958, _5959, intervalFailed, optical_product_upper);
            Interval _5960 = _7481;
            Interval _7482 = jet_add_derivative(_5957, _5960, intervalFailed);
            Interval _5941 = _7455;
            Interval _5942 = _7470;
            Interval _7483 = iadd(_5941, _5942, intervalFailed);
            Interval _5943 = _7461;
            Interval _5944 = _7476;
            Interval _7484 = jet_add_derivative(_5943, _5944, intervalFailed);
            Interval _5945 = _7467;
            Interval _5946 = _7482;
            Interval _7485 = jet_add_derivative(_5945, _5946, intervalFailed);
            Interval _5927 = Interval{ _7449.z, _7449.z };
            Interval _5928 = Interval{ _6927, _6928 };
            Interval _7488 = imul(_5927, _5928, intervalFailed, optical_product_upper);
            Interval _5929 = Interval{ 0.0, 0.0 };
            Interval _5930 = Interval{ _6927, _6928 };
            Interval _7490 = jet_mul_derivative(_5929, _5930, intervalFailed, optical_product_upper);
            Interval _5931 = _7490;
            Interval _5932 = Interval{ _7449.z, _7449.z };
            Interval _5933 = Interval{ _6929, _6930 };
            Interval _7493 = jet_mul_derivative(_5932, _5933, intervalFailed, optical_product_upper);
            Interval _5934 = _7493;
            Interval _7494 = jet_add_derivative(_5931, _5934, intervalFailed);
            Interval _5935 = Interval{ 0.0, 0.0 };
            Interval _5936 = Interval{ _6927, _6928 };
            Interval _7496 = jet_mul_derivative(_5935, _5936, intervalFailed, optical_product_upper);
            Interval _5937 = _7496;
            Interval _5938 = Interval{ _7449.z, _7449.z };
            Interval _5939 = Interval{ _6931, _6932 };
            Interval _7499 = jet_mul_derivative(_5938, _5939, intervalFailed, optical_product_upper);
            Interval _5940 = _7499;
            Interval _7500 = jet_add_derivative(_5937, _5940, intervalFailed);
            Interval _5921 = _7483;
            Interval _5922 = _7488;
            Interval _7501 = iadd(_5921, _5922, intervalFailed);
            Interval _5923 = _7484;
            Interval _5924 = _7494;
            Interval _7502 = jet_add_derivative(_5923, _5924, intervalFailed);
            Interval _5925 = _7485;
            Interval _5926 = _7500;
            Interval _7503 = jet_add_derivative(_5925, _5926, intervalFailed);
            Interval _5907 = _7501;
            Interval _5908 = _6987;
            Interval _7505 = idiv(_5907, _5908, intervalFailed, interval_divide_upper);
            bool _7514;
            if (!intervalFailed)
            {
                _7514 = intervalFailed;
            }
            else
            {
                _7514 = false;
            }
            bool _7519;
            if (_7514)
            {
                _7519 = jetFailureSite == 0u;
            }
            else
            {
                _7519 = false;
            }
            if (_7519)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7501.lo, _7501.hi, _6987.lo, _6987.hi);
            }
            Interval _5909 = _7502;
            Interval _5910 = _7505;
            Interval _5911 = _6988;
            Interval _7523 = jet_mul_derivative(_5910, _5911, intervalFailed, optical_product_upper);
            Interval _5912 = Interval{ as_type<float>(as_type<uint>(_7523.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7523.lo) ^ 2147483648u) };
            Interval _7533 = jet_add_derivative(_5909, _5912, intervalFailed);
            Interval _5913 = _7533;
            Interval _5914 = _6987;
            Interval _7534 = jet_div_derivative(_5913, _5914, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5915 = _7503;
            Interval _5916 = _7505;
            Interval _5917 = _6989;
            Interval _7535 = jet_mul_derivative(_5916, _5917, intervalFailed, optical_product_upper);
            Interval _5918 = Interval{ as_type<float>(as_type<uint>(_7535.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7535.lo) ^ 2147483648u) };
            Interval _7545 = jet_add_derivative(_5915, _5918, intervalFailed);
            Interval _5919 = _7545;
            Interval _5920 = _6987;
            Interval _7546 = jet_div_derivative(_5919, _5920, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5893 = _7505;
            Interval _5894 = Interval{ _6787, _6789 };
            Interval _7553 = imul(_5893, _5894, intervalFailed, optical_product_upper);
            Interval _5895 = _7534;
            Interval _5896 = Interval{ _6787, _6789 };
            Interval _7555 = jet_mul_derivative(_5895, _5896, intervalFailed, optical_product_upper);
            Interval _5897 = _7555;
            Interval _5898 = _7505;
            Interval _5899 = Interval{ _6791, _6793 };
            Interval _7557 = jet_mul_derivative(_5898, _5899, intervalFailed, optical_product_upper);
            Interval _5900 = _7557;
            Interval _7558 = jet_add_derivative(_5897, _5900, intervalFailed);
            Interval _5901 = _7546;
            Interval _5902 = Interval{ _6787, _6789 };
            Interval _7560 = jet_mul_derivative(_5901, _5902, intervalFailed, optical_product_upper);
            Interval _5903 = _7560;
            Interval _5904 = _7505;
            Interval _5905 = Interval{ _6795, _6797 };
            Interval _7562 = jet_mul_derivative(_5904, _5905, intervalFailed, optical_product_upper);
            Interval _5906 = _7562;
            Interval _7563 = jet_add_derivative(_5903, _5906, intervalFailed);
            ray.depth = OpticalJet{ Interval{ precise::min(ray.depth.v.lo, _7553.lo), precise::max(ray.depth.v.hi, _7553.hi) }, Interval{ precise::min(ray.depth.dx.lo, _7558.lo), precise::max(ray.depth.dx.hi, _7558.hi) }, Interval{ precise::min(ray.depth.dy.lo, _7563.lo), precise::max(ray.depth.dy.hi, _7563.hi) } };
            bool _7594;
            if ((isunordered(_7396.lo, 0.0) || _7396.lo > 0.0))
            {
                _7594 = ray.depth.v.lo < receiver.extentClip.z;
            }
            else
            {
                _7594 = true;
            }
            bool _7602;
            if (!_7594)
            {
                _7602 = ray.depth.v.hi > receiver.extentClip.w;
            }
            else
            {
                _7602 = true;
            }
            if (_7602)
            {
                return false;
            }
            _7603 = _7396.lo;
            _7604 = _7396.hi;
            _7605 = _7399.lo;
            _7606 = _7399.hi;
            _7607 = _7402.lo;
            _7608 = _7402.hi;
        }
        else
        {
            float3 _7014;
            if (_6849)
            {
                _7014 = liquid.planePoint.xyz;
            }
            else
            {
                _7014 = _6861.xyz;
            }
            Interval _5887 = Interval{ _7014.x, _7014.x };
            Interval _5888 = Interval{ as_type<float>(as_type<uint>(_6801) ^ 2147483648u), as_type<float>(as_type<uint>(_6799) ^ 2147483648u) };
            Interval _7074 = iadd(_5887, _5888, intervalFailed);
            Interval _5889 = Interval{ 0.0, 0.0 };
            Interval _5890 = Interval{ as_type<float>(as_type<uint>(_6805) ^ 2147483648u), as_type<float>(as_type<uint>(_6803) ^ 2147483648u) };
            Interval _7076 = jet_add_derivative(_5889, _5890, intervalFailed);
            Interval _5891 = Interval{ 0.0, 0.0 };
            Interval _5892 = Interval{ as_type<float>(as_type<uint>(_6809) ^ 2147483648u), as_type<float>(as_type<uint>(_6807) ^ 2147483648u) };
            Interval _7078 = jet_add_derivative(_5891, _5892, intervalFailed);
            Interval _5881 = Interval{ _7014.y, _7014.y };
            Interval _5882 = Interval{ as_type<float>(as_type<uint>(_6813) ^ 2147483648u), as_type<float>(as_type<uint>(_6811) ^ 2147483648u) };
            Interval _7081 = iadd(_5881, _5882, intervalFailed);
            Interval _5883 = Interval{ 0.0, 0.0 };
            Interval _5884 = Interval{ as_type<float>(as_type<uint>(_6817) ^ 2147483648u), as_type<float>(as_type<uint>(_6815) ^ 2147483648u) };
            Interval _7083 = jet_add_derivative(_5883, _5884, intervalFailed);
            Interval _5885 = Interval{ 0.0, 0.0 };
            Interval _5886 = Interval{ as_type<float>(as_type<uint>(_6821) ^ 2147483648u), as_type<float>(as_type<uint>(_6819) ^ 2147483648u) };
            Interval _7085 = jet_add_derivative(_5885, _5886, intervalFailed);
            Interval _5875 = Interval{ _7014.z, _7014.z };
            Interval _5876 = Interval{ as_type<float>(as_type<uint>(_6825) ^ 2147483648u), as_type<float>(as_type<uint>(_6823) ^ 2147483648u) };
            Interval _7088 = iadd(_5875, _5876, intervalFailed);
            Interval _5877 = Interval{ 0.0, 0.0 };
            Interval _5878 = Interval{ as_type<float>(as_type<uint>(_6829) ^ 2147483648u), as_type<float>(as_type<uint>(_6827) ^ 2147483648u) };
            Interval _7090 = jet_add_derivative(_5877, _5878, intervalFailed);
            Interval _5879 = Interval{ 0.0, 0.0 };
            Interval _5880 = Interval{ as_type<float>(as_type<uint>(_6833) ^ 2147483648u), as_type<float>(as_type<uint>(_6831) ^ 2147483648u) };
            Interval _7092 = jet_add_derivative(_5879, _5880, intervalFailed);
            Interval _5861 = _7074;
            Interval _5862 = Interval{ _6915, _6916 };
            Interval _7094 = imul(_5861, _5862, intervalFailed, optical_product_upper);
            Interval _5863 = _7076;
            Interval _5864 = Interval{ _6915, _6916 };
            Interval _7096 = jet_mul_derivative(_5863, _5864, intervalFailed, optical_product_upper);
            Interval _5865 = _7096;
            Interval _5866 = _7074;
            Interval _5867 = Interval{ _6917, _6918 };
            Interval _7098 = jet_mul_derivative(_5866, _5867, intervalFailed, optical_product_upper);
            Interval _5868 = _7098;
            Interval _7099 = jet_add_derivative(_5865, _5868, intervalFailed);
            Interval _5869 = _7078;
            Interval _5870 = Interval{ _6915, _6916 };
            Interval _7101 = jet_mul_derivative(_5869, _5870, intervalFailed, optical_product_upper);
            Interval _5871 = _7101;
            Interval _5872 = _7074;
            Interval _5873 = Interval{ _6919, _6920 };
            Interval _7103 = jet_mul_derivative(_5872, _5873, intervalFailed, optical_product_upper);
            Interval _5874 = _7103;
            Interval _7104 = jet_add_derivative(_5871, _5874, intervalFailed);
            Interval _5847 = _7081;
            Interval _5848 = Interval{ _6921, _6922 };
            Interval _7106 = imul(_5847, _5848, intervalFailed, optical_product_upper);
            Interval _5849 = _7083;
            Interval _5850 = Interval{ _6921, _6922 };
            Interval _7108 = jet_mul_derivative(_5849, _5850, intervalFailed, optical_product_upper);
            Interval _5851 = _7108;
            Interval _5852 = _7081;
            Interval _5853 = Interval{ _6923, _6924 };
            Interval _7110 = jet_mul_derivative(_5852, _5853, intervalFailed, optical_product_upper);
            Interval _5854 = _7110;
            Interval _7111 = jet_add_derivative(_5851, _5854, intervalFailed);
            Interval _5855 = _7085;
            Interval _5856 = Interval{ _6921, _6922 };
            Interval _7113 = jet_mul_derivative(_5855, _5856, intervalFailed, optical_product_upper);
            Interval _5857 = _7113;
            Interval _5858 = _7081;
            Interval _5859 = Interval{ _6925, _6926 };
            Interval _7115 = jet_mul_derivative(_5858, _5859, intervalFailed, optical_product_upper);
            Interval _5860 = _7115;
            Interval _7116 = jet_add_derivative(_5857, _5860, intervalFailed);
            Interval _5841 = _7094;
            Interval _5842 = _7106;
            Interval _7117 = iadd(_5841, _5842, intervalFailed);
            Interval _5843 = _7099;
            Interval _5844 = _7111;
            Interval _7118 = jet_add_derivative(_5843, _5844, intervalFailed);
            Interval _5845 = _7104;
            Interval _5846 = _7116;
            Interval _7119 = jet_add_derivative(_5845, _5846, intervalFailed);
            Interval _5827 = _7088;
            Interval _5828 = Interval{ _6927, _6928 };
            Interval _7121 = imul(_5827, _5828, intervalFailed, optical_product_upper);
            Interval _5829 = _7090;
            Interval _5830 = Interval{ _6927, _6928 };
            Interval _7123 = jet_mul_derivative(_5829, _5830, intervalFailed, optical_product_upper);
            Interval _5831 = _7123;
            Interval _5832 = _7088;
            Interval _5833 = Interval{ _6929, _6930 };
            Interval _7125 = jet_mul_derivative(_5832, _5833, intervalFailed, optical_product_upper);
            Interval _5834 = _7125;
            Interval _7126 = jet_add_derivative(_5831, _5834, intervalFailed);
            Interval _5835 = _7092;
            Interval _5836 = Interval{ _6927, _6928 };
            Interval _7128 = jet_mul_derivative(_5835, _5836, intervalFailed, optical_product_upper);
            Interval _5837 = _7128;
            Interval _5838 = _7088;
            Interval _5839 = Interval{ _6931, _6932 };
            Interval _7130 = jet_mul_derivative(_5838, _5839, intervalFailed, optical_product_upper);
            Interval _5840 = _7130;
            Interval _7131 = jet_add_derivative(_5837, _5840, intervalFailed);
            Interval _5821 = _7117;
            Interval _5822 = _7121;
            Interval _7132 = iadd(_5821, _5822, intervalFailed);
            Interval _5823 = _7118;
            Interval _5824 = _7126;
            Interval _7133 = jet_add_derivative(_5823, _5824, intervalFailed);
            Interval _5825 = _7119;
            Interval _5826 = _7131;
            Interval _7134 = jet_add_derivative(_5825, _5826, intervalFailed);
            Interval _5807 = _7132;
            Interval _5808 = _6987;
            Interval _7136 = idiv(_5807, _5808, intervalFailed, interval_divide_upper);
            bool _7145;
            if (!intervalFailed)
            {
                _7145 = intervalFailed;
            }
            else
            {
                _7145 = false;
            }
            bool _7150;
            if (_7145)
            {
                _7150 = jetFailureSite == 0u;
            }
            else
            {
                _7150 = false;
            }
            if (_7150)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7132.lo, _7132.hi, _6987.lo, _6987.hi);
            }
            Interval _5809 = _7133;
            Interval _5810 = _7136;
            Interval _5811 = _6988;
            Interval _7154 = jet_mul_derivative(_5810, _5811, intervalFailed, optical_product_upper);
            Interval _5812 = Interval{ as_type<float>(as_type<uint>(_7154.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7154.lo) ^ 2147483648u) };
            Interval _7164 = jet_add_derivative(_5809, _5812, intervalFailed);
            Interval _5813 = _7164;
            Interval _5814 = _6987;
            Interval _7165 = jet_div_derivative(_5813, _5814, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5815 = _7134;
            Interval _5816 = _7136;
            Interval _5817 = _6989;
            Interval _7166 = jet_mul_derivative(_5816, _5817, intervalFailed, optical_product_upper);
            Interval _5818 = Interval{ as_type<float>(as_type<uint>(_7166.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7166.lo) ^ 2147483648u) };
            Interval _7176 = jet_add_derivative(_5815, _5818, intervalFailed);
            Interval _5819 = _7176;
            Interval _5820 = _6987;
            Interval _7177 = jet_div_derivative(_5819, _5820, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            bool _7186;
            if ((isunordered(_7136.lo, _6753) || _7136.lo > _6753))
            {
                _7186 = _7136.hi >= 65536.0;
            }
            else
            {
                _7186 = true;
            }
            if (_7186)
            {
                return false;
            }
            Interval _5793 = Interval{ _6763, _6765 };
            Interval _5794 = _7136;
            Interval _7188 = imul(_5793, _5794, intervalFailed, optical_product_upper);
            Interval _5795 = Interval{ _6767, _6769 };
            Interval _5796 = _7136;
            Interval _7190 = jet_mul_derivative(_5795, _5796, intervalFailed, optical_product_upper);
            Interval _5797 = _7190;
            Interval _5798 = Interval{ _6763, _6765 };
            Interval _5799 = _7165;
            Interval _7192 = jet_mul_derivative(_5798, _5799, intervalFailed, optical_product_upper);
            Interval _5800 = _7192;
            Interval _7193 = jet_add_derivative(_5797, _5800, intervalFailed);
            Interval _5801 = Interval{ _6771, _6773 };
            Interval _5802 = _7136;
            Interval _7195 = jet_mul_derivative(_5801, _5802, intervalFailed, optical_product_upper);
            Interval _5803 = _7195;
            Interval _5804 = Interval{ _6763, _6765 };
            Interval _5805 = _7177;
            Interval _7197 = jet_mul_derivative(_5804, _5805, intervalFailed, optical_product_upper);
            Interval _5806 = _7197;
            Interval _7198 = jet_add_derivative(_5803, _5806, intervalFailed);
            Interval _5779 = Interval{ _6775, _6777 };
            Interval _5780 = _7136;
            Interval _7200 = imul(_5779, _5780, intervalFailed, optical_product_upper);
            Interval _5781 = Interval{ _6779, _6781 };
            Interval _5782 = _7136;
            Interval _7202 = jet_mul_derivative(_5781, _5782, intervalFailed, optical_product_upper);
            Interval _5783 = _7202;
            Interval _5784 = Interval{ _6775, _6777 };
            Interval _5785 = _7165;
            Interval _7204 = jet_mul_derivative(_5784, _5785, intervalFailed, optical_product_upper);
            Interval _5786 = _7204;
            Interval _7205 = jet_add_derivative(_5783, _5786, intervalFailed);
            Interval _5787 = Interval{ _6783, _6785 };
            Interval _5788 = _7136;
            Interval _7207 = jet_mul_derivative(_5787, _5788, intervalFailed, optical_product_upper);
            Interval _5789 = _7207;
            Interval _5790 = Interval{ _6775, _6777 };
            Interval _5791 = _7177;
            Interval _7209 = jet_mul_derivative(_5790, _5791, intervalFailed, optical_product_upper);
            Interval _5792 = _7209;
            Interval _7210 = jet_add_derivative(_5789, _5792, intervalFailed);
            Interval _5765 = Interval{ _6787, _6789 };
            Interval _5766 = _7136;
            Interval _7212 = imul(_5765, _5766, intervalFailed, optical_product_upper);
            Interval _5767 = Interval{ _6791, _6793 };
            Interval _5768 = _7136;
            Interval _7214 = jet_mul_derivative(_5767, _5768, intervalFailed, optical_product_upper);
            Interval _5769 = _7214;
            Interval _5770 = Interval{ _6787, _6789 };
            Interval _5771 = _7165;
            Interval _7216 = jet_mul_derivative(_5770, _5771, intervalFailed, optical_product_upper);
            Interval _5772 = _7216;
            Interval _7217 = jet_add_derivative(_5769, _5772, intervalFailed);
            Interval _5773 = Interval{ _6795, _6797 };
            Interval _5774 = _7136;
            Interval _7219 = jet_mul_derivative(_5773, _5774, intervalFailed, optical_product_upper);
            Interval _5775 = _7219;
            Interval _5776 = Interval{ _6787, _6789 };
            Interval _5777 = _7177;
            Interval _7221 = jet_mul_derivative(_5776, _5777, intervalFailed, optical_product_upper);
            Interval _5778 = _7221;
            Interval _7222 = jet_add_derivative(_5775, _5778, intervalFailed);
            Interval _5759 = Interval{ _6799, _6801 };
            Interval _5760 = _7188;
            Interval _7224 = iadd(_5759, _5760, intervalFailed);
            Interval _5761 = Interval{ _6803, _6805 };
            Interval _5762 = _7193;
            Interval _7226 = jet_add_derivative(_5761, _5762, intervalFailed);
            Interval _5763 = Interval{ _6807, _6809 };
            Interval _5764 = _7198;
            Interval _7228 = jet_add_derivative(_5763, _5764, intervalFailed);
            Interval _5753 = Interval{ _6811, _6813 };
            Interval _5754 = _7200;
            Interval _7230 = iadd(_5753, _5754, intervalFailed);
            Interval _5755 = Interval{ _6815, _6817 };
            Interval _5756 = _7205;
            Interval _7232 = jet_add_derivative(_5755, _5756, intervalFailed);
            Interval _5757 = Interval{ _6819, _6821 };
            Interval _5758 = _7210;
            Interval _7234 = jet_add_derivative(_5757, _5758, intervalFailed);
            Interval _5747 = Interval{ _6823, _6825 };
            Interval _5748 = _7212;
            Interval _7236 = iadd(_5747, _5748, intervalFailed);
            Interval _5749 = Interval{ _6827, _6829 };
            Interval _5750 = _7217;
            Interval _7238 = jet_add_derivative(_5749, _5750, intervalFailed);
            Interval _5751 = Interval{ _6831, _6833 };
            Interval _5752 = _7222;
            Interval _7240 = jet_add_derivative(_5751, _5752, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7224, _7226, _7228 }, OpticalJet{ _7230, _7232, _7234 }, OpticalJet{ _7236, _7238, _7240 } };
            _7603 = _7136.lo;
            _7604 = _7136.hi;
            _7605 = _7165.lo;
            _7606 = _7165.hi;
            _7607 = _7177.lo;
            _7608 = _7177.hi;
        }
        float _7923;
        float _7924;
        float _7925;
        float _7926;
        float _7927;
        float _7928;
        float _7929;
        float _7930;
        float _7931;
        float _7932;
        float _7933;
        float _7934;
        float _7935;
        float _7936;
        float _7937;
        float _7938;
        float _7939;
        float _7940;
        if (_6849)
        {
            float _7754;
            float _7755;
            float _7756;
            float _7757;
            float _7758;
            float _7759;
            if (_6839)
            {
                _7754 = _7603;
                _7755 = _7604;
                _7756 = _7605;
                _7757 = _7606;
                _7758 = _7607;
                _7759 = _7608;
            }
            else
            {
                bool _7626;
                if (hit.x.v.lo <= 0.0)
                {
                    _7626 = hit.x.v.hi >= 0.0;
                }
                else
                {
                    _7626 = false;
                }
                float _7633;
                if (_7626)
                {
                    _7633 = 0.0;
                }
                else
                {
                    _7633 = precise::min(abs(hit.x.v.lo), abs(hit.x.v.hi));
                }
                float _7636 = precise::max(abs(hit.x.v.lo), abs(hit.x.v.hi));
                float _5737 = spvFMul(_7633, _7633);
                float _7637 = interval_down(_5737, intervalFailed);
                float _5738 = spvFMul(_7636, _7636);
                float _7639 = interval_up(_5738, intervalFailed);
                Interval _5739 = Interval{ 2.0, 2.0 };
                Interval _5740 = hit.x.v;
                Interval _7640 = imul(_5739, _5740, intervalFailed, optical_product_upper);
                Interval _5741 = _7640;
                Interval _5742 = hit.x.dx;
                Interval _7641 = jet_mul_derivative(_5741, _5742, intervalFailed, optical_product_upper);
                Interval _5743 = Interval{ 2.0, 2.0 };
                Interval _5744 = hit.x.v;
                Interval _7642 = imul(_5743, _5744, intervalFailed, optical_product_upper);
                Interval _5745 = _7642;
                Interval _5746 = hit.x.dy;
                Interval _7643 = jet_mul_derivative(_5745, _5746, intervalFailed, optical_product_upper);
                bool _7653;
                if (hit.y.v.lo <= 0.0)
                {
                    _7653 = hit.y.v.hi >= 0.0;
                }
                else
                {
                    _7653 = false;
                }
                float _7660;
                if (_7653)
                {
                    _7660 = 0.0;
                }
                else
                {
                    _7660 = precise::min(abs(hit.y.v.lo), abs(hit.y.v.hi));
                }
                float _7663 = precise::max(abs(hit.y.v.lo), abs(hit.y.v.hi));
                float _5727 = spvFMul(_7660, _7660);
                float _7664 = interval_down(_5727, intervalFailed);
                float _5728 = spvFMul(_7663, _7663);
                float _7666 = interval_up(_5728, intervalFailed);
                Interval _5729 = Interval{ 2.0, 2.0 };
                Interval _5730 = hit.y.v;
                Interval _7667 = imul(_5729, _5730, intervalFailed, optical_product_upper);
                Interval _5731 = _7667;
                Interval _5732 = hit.y.dx;
                Interval _7668 = jet_mul_derivative(_5731, _5732, intervalFailed, optical_product_upper);
                Interval _5733 = Interval{ 2.0, 2.0 };
                Interval _5734 = hit.y.v;
                Interval _7669 = imul(_5733, _5734, intervalFailed, optical_product_upper);
                Interval _5735 = _7669;
                Interval _5736 = hit.y.dy;
                Interval _7670 = jet_mul_derivative(_5735, _5736, intervalFailed, optical_product_upper);
                Interval _5721 = Interval{ precise::max(0.0, _7637), _7639 };
                Interval _5722 = Interval{ precise::max(0.0, _7664), _7666 };
                Interval _7673 = iadd(_5721, _5722, intervalFailed);
                Interval _5723 = _7641;
                Interval _5724 = _7668;
                Interval _7674 = jet_add_derivative(_5723, _5724, intervalFailed);
                Interval _5725 = _7643;
                Interval _5726 = _7670;
                Interval _7675 = jet_add_derivative(_5725, _5726, intervalFailed);
                bool _7685;
                if (hit.z.v.lo <= 0.0)
                {
                    _7685 = hit.z.v.hi >= 0.0;
                }
                else
                {
                    _7685 = false;
                }
                float _7692;
                if (_7685)
                {
                    _7692 = 0.0;
                }
                else
                {
                    _7692 = precise::min(abs(hit.z.v.lo), abs(hit.z.v.hi));
                }
                float _7695 = precise::max(abs(hit.z.v.lo), abs(hit.z.v.hi));
                float _5711 = spvFMul(_7692, _7692);
                float _7696 = interval_down(_5711, intervalFailed);
                float _5712 = spvFMul(_7695, _7695);
                float _7698 = interval_up(_5712, intervalFailed);
                Interval _5713 = Interval{ 2.0, 2.0 };
                Interval _5714 = hit.z.v;
                Interval _7699 = imul(_5713, _5714, intervalFailed, optical_product_upper);
                Interval _5715 = _7699;
                Interval _5716 = hit.z.dx;
                Interval _7700 = jet_mul_derivative(_5715, _5716, intervalFailed, optical_product_upper);
                Interval _5717 = Interval{ 2.0, 2.0 };
                Interval _5718 = hit.z.v;
                Interval _7701 = imul(_5717, _5718, intervalFailed, optical_product_upper);
                Interval _5719 = _7701;
                Interval _5720 = hit.z.dy;
                Interval _7702 = jet_mul_derivative(_5719, _5720, intervalFailed, optical_product_upper);
                Interval _5705 = _7673;
                Interval _5706 = Interval{ precise::max(0.0, _7696), _7698 };
                Interval _7704 = iadd(_5705, _5706, intervalFailed);
                Interval _5707 = _7674;
                Interval _5708 = _7700;
                Interval _7705 = jet_add_derivative(_5707, _5708, intervalFailed);
                Interval _5709 = _7675;
                Interval _5710 = _7702;
                Interval _7706 = jet_add_derivative(_5709, _5710, intervalFailed);
                Interval _5696 = _7704;
                Interval _7708 = isqrt(_5696, intervalFailed);
                bool _7716;
                if (!intervalFailed)
                {
                    _7716 = intervalFailed;
                }
                else
                {
                    _7716 = false;
                }
                bool _7721;
                if (_7716)
                {
                    _7721 = jetFailureSite == 0u;
                }
                else
                {
                    _7721 = false;
                }
                if (_7721)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_7704.lo, _7704.hi, 0.0, 0.0);
                }
                if (_7708.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5697 = Interval{ 2.0, 2.0 };
                Interval _5698 = _7708;
                Interval _7728 = imul(_5697, _5698, intervalFailed, optical_product_upper);
                Interval _5699 = Interval{ 1.0, 1.0 };
                Interval _5700 = _7728;
                Interval _7730 = idiv(_5699, _5700, intervalFailed, interval_divide_upper);
                bool _7737;
                if (!intervalFailed)
                {
                    _7737 = intervalFailed;
                }
                else
                {
                    _7737 = false;
                }
                bool _7742;
                if (_7737)
                {
                    _7742 = jetFailureSite == 0u;
                }
                else
                {
                    _7742 = false;
                }
                if (_7742)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _7728.lo, _7728.hi);
                }
                Interval _5701 = _7730;
                Interval _5702 = _7705;
                Interval _7746 = jet_mul_derivative(_5701, _5702, intervalFailed, optical_product_upper);
                Interval _5703 = _7730;
                Interval _5704 = _7706;
                Interval _7747 = jet_mul_derivative(_5703, _5704, intervalFailed, optical_product_upper);
                _7754 = _7708.lo;
                _7755 = _7708.hi;
                _7756 = _7746.lo;
                _7757 = _7746.hi;
                _7758 = _7747.lo;
                _7759 = _7747.hi;
            }
            float _7763 = precise::max(liquid.projection.x, 1.0);
            Interval _5682 = Interval{ _7754, _7755 };
            Interval _5683 = Interval{ _7763, _7763 };
            Interval _7767 = idiv(_5682, _5683, intervalFailed, interval_divide_upper);
            bool _7772;
            if (!intervalFailed)
            {
                _7772 = intervalFailed;
            }
            else
            {
                _7772 = false;
            }
            bool _7777;
            if (_7772)
            {
                _7777 = jetFailureSite == 0u;
            }
            else
            {
                _7777 = false;
            }
            if (_7777)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7754, _7755, _7763, _7763);
            }
            Interval _5684 = Interval{ _7756, _7757 };
            Interval _5685 = _7767;
            Interval _5686 = Interval{ 0.0, 0.0 };
            Interval _7782 = jet_mul_derivative(_5685, _5686, intervalFailed, optical_product_upper);
            Interval _5687 = Interval{ as_type<float>(as_type<uint>(_7782.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7782.lo) ^ 2147483648u) };
            Interval _7792 = jet_add_derivative(_5684, _5687, intervalFailed);
            Interval _5688 = _7792;
            Interval _5689 = Interval{ _7763, _7763 };
            Interval _7794 = jet_div_derivative(_5688, _5689, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5690 = Interval{ _7758, _7759 };
            Interval _5691 = _7767;
            Interval _5692 = Interval{ 0.0, 0.0 };
            Interval _7796 = jet_mul_derivative(_5691, _5692, intervalFailed, optical_product_upper);
            Interval _5693 = Interval{ as_type<float>(as_type<uint>(_7796.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7796.lo) ^ 2147483648u) };
            Interval _7806 = jet_add_derivative(_5690, _5693, intervalFailed);
            Interval _5694 = _7806;
            Interval _5695 = Interval{ _7763, _7763 };
            Interval _7808 = jet_div_derivative(_5694, _5695, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            OpticalJet param_var_a = OpticalJet{ _6987, _6988, _6989 };
            OpticalJet param_var_a_1 = jabs(param_var_a);
            float _5680 = 4.0;
            float _5681 = 100.0;
            Interval _7812 = iratio(_5680, _5681, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _7817;
            if (!intervalFailed)
            {
                _7817 = intervalFailed;
            }
            else
            {
                _7817 = false;
            }
            bool _7822;
            if (_7817)
            {
                _7822 = jetFailureSite == 0u;
            }
            else
            {
                _7822 = false;
            }
            if (_7822)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(4.0, 4.0, 100.0, 100.0);
            }
            OpticalJet param_var_b = OpticalJet{ _7812, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet _7826 = jmax(param_var_a_1, param_var_b);
            Interval _7827 = _7826.v;
            Interval _5666 = _7767;
            Interval _5667 = _7827;
            Interval _7831 = idiv(_5666, _5667, intervalFailed, interval_divide_upper);
            bool _7840;
            if (!intervalFailed)
            {
                _7840 = intervalFailed;
            }
            else
            {
                _7840 = false;
            }
            bool _7845;
            if (_7840)
            {
                _7845 = jetFailureSite == 0u;
            }
            else
            {
                _7845 = false;
            }
            if (_7845)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7767.lo, _7767.hi, _7827.lo, _7827.hi);
            }
            Interval _5668 = _7794;
            Interval _5669 = _7831;
            Interval _5670 = _7826.dx;
            Interval _7849 = jet_mul_derivative(_5669, _5670, intervalFailed, optical_product_upper);
            Interval _5671 = Interval{ as_type<float>(as_type<uint>(_7849.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7849.lo) ^ 2147483648u) };
            Interval _7859 = jet_add_derivative(_5668, _5671, intervalFailed);
            Interval _5672 = _7859;
            Interval _5673 = _7827;
            Interval _7860 = jet_div_derivative(_5672, _5673, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5674 = _7808;
            Interval _5675 = _7831;
            Interval _5676 = _7826.dy;
            Interval _7861 = jet_mul_derivative(_5675, _5676, intervalFailed, optical_product_upper);
            Interval _5677 = Interval{ as_type<float>(as_type<uint>(_7861.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7861.lo) ^ 2147483648u) };
            Interval _7871 = jet_add_derivative(_5674, _5677, intervalFailed);
            Interval _5678 = _7871;
            Interval _5679 = _7827;
            Interval _7872 = jet_div_derivative(_5678, _5679, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ReflectionLiquidFrame param_var_f = liquid;
            OpticalJet3 param_var_direction_1 = OpticalJet3{ OpticalJet{ Interval{ _6763, _6765 }, Interval{ _6767, _6769 }, Interval{ _6771, _6773 } }, OpticalJet{ Interval{ _6775, _6777 }, Interval{ _6779, _6781 }, Interval{ _6783, _6785 } }, OpticalJet{ Interval{ _6787, _6789 }, Interval{ _6791, _6793 }, Interval{ _6795, _6797 } } };
            OpticalJet param_var_distance = OpticalJet{ Interval{ _7603, _7604 }, Interval{ _7605, _7606 }, Interval{ _7607, _7608 } };
            OpticalJet param_var_footprint = OpticalJet{ _7831, _7860, _7872 };
            OpticalJet3 _7892 = jet_liquid_normal(param_var_f, param_var_direction_1, param_var_distance, param_var_footprint, hit, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            _7923 = _7892.x.v.lo;
            _7924 = _7892.x.v.hi;
            _7925 = _7892.x.dx.lo;
            _7926 = _7892.x.dx.hi;
            _7927 = _7892.x.dy.lo;
            _7928 = _7892.x.dy.hi;
            _7929 = _7892.y.v.lo;
            _7930 = _7892.y.v.hi;
            _7931 = _7892.y.dx.lo;
            _7932 = _7892.y.dx.hi;
            _7933 = _7892.y.dy.lo;
            _7934 = _7892.y.dy.hi;
            _7935 = _7892.z.v.lo;
            _7936 = _7892.z.v.hi;
            _7937 = _7892.z.dx.lo;
            _7938 = _7892.z.dx.hi;
            _7939 = _7892.z.dy.lo;
            _7940 = _7892.z.dy.hi;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane_1 = ReflectionSpecularPlane{ _6861, _6862, _6863 };
            OpticalJet3 param_var_hit = hit;
            bool _7611 = jet_inside_face(param_var_plane_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (!_7611)
            {
                return false;
            }
            _7923 = _6915;
            _7924 = _6916;
            _7925 = _6917;
            _7926 = _6918;
            _7927 = _6919;
            _7928 = _6920;
            _7929 = _6921;
            _7930 = _6922;
            _7931 = _6923;
            _7932 = _6924;
            _7933 = _6925;
            _7934 = _6926;
            _7935 = _6927;
            _7936 = _6928;
            _7937 = _6929;
            _7938 = _6930;
            _7939 = _6931;
            _7940 = _6932;
        }
        bool _7942;
        if (_6839)
        {
            _7942 = !_6849;
        }
        else
        {
            _7942 = false;
        }
        bool _7956;
        if (_7942)
        {
            bool _7949;
            if (_6861.w == 2.0)
            {
                _7949 = _6862.w == 2.0;
            }
            else
            {
                _7949 = false;
            }
            bool _7954;
            if (_7949)
            {
                _7954 = _6863.w == 2.0;
            }
            else
            {
                _7954 = false;
            }
            _7956 = !_7954;
        }
        else
        {
            _7956 = false;
        }
        int _7957;
        if (_7956)
        {
            _7957 = 1;
        }
        else
        {
            _7957 = 5;
        }
        float _7958 = float(_7957);
        float _5664 = _7958;
        float _5665 = 100.0;
        Interval _7960 = iratio(_5664, _5665, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7965;
        if (!intervalFailed)
        {
            _7965 = intervalFailed;
        }
        else
        {
            _7965 = false;
        }
        bool _7970;
        if (_7965)
        {
            _7970 = jetFailureSite == 0u;
        }
        else
        {
            _7970 = false;
        }
        if (_7970)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(_7958, _7958, 100.0, 100.0);
        }
        OpticalJet param_var_a_2 = OpticalJet{ _7960, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
        float _5662 = 1.0;
        float _5663 = 100000.0;
        Interval _7976 = iratio(_5662, _5663, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7981;
        if (!intervalFailed)
        {
            _7981 = intervalFailed;
        }
        else
        {
            _7981 = false;
        }
        bool _7986;
        if (_7981)
        {
            _7986 = jetFailureSite == 0u;
        }
        else
        {
            _7986 = false;
        }
        if (_7986)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(1.0, 1.0, 100000.0, 100000.0);
        }
        Interval _5648 = Interval{ _7603, _7604 };
        Interval _5649 = _7976;
        Interval _7990 = imul(_5648, _5649, intervalFailed, optical_product_upper);
        Interval _5650 = Interval{ _7605, _7606 };
        Interval _5651 = _7976;
        Interval _7992 = jet_mul_derivative(_5650, _5651, intervalFailed, optical_product_upper);
        Interval _5652 = _7992;
        Interval _5653 = Interval{ _7603, _7604 };
        Interval _5654 = Interval{ 0.0, 0.0 };
        Interval _7994 = jet_mul_derivative(_5653, _5654, intervalFailed, optical_product_upper);
        Interval _5655 = _7994;
        Interval _7995 = jet_add_derivative(_5652, _5655, intervalFailed);
        Interval _5656 = Interval{ _7607, _7608 };
        Interval _5657 = _7976;
        Interval _7997 = jet_mul_derivative(_5656, _5657, intervalFailed, optical_product_upper);
        Interval _5658 = _7997;
        Interval _5659 = Interval{ _7603, _7604 };
        Interval _5660 = Interval{ 0.0, 0.0 };
        Interval _7999 = jet_mul_derivative(_5659, _5660, intervalFailed, optical_product_upper);
        Interval _5661 = _7999;
        Interval _8000 = jet_add_derivative(_5658, _5661, intervalFailed);
        OpticalJet param_var_b_1 = OpticalJet{ _7990, _7995, _8000 };
        OpticalJet _8002 = jmax(param_var_a_2, param_var_b_1);
        Interval _8003 = _8002.v;
        _6752 = _8003.lo;
        _6754 = _8003.hi;
        Interval _8004 = _8002.dx;
        _6756 = _8004.lo;
        _6758 = _8004.hi;
        Interval _8005 = _8002.dy;
        _6760 = _8005.lo;
        _6762 = _8005.hi;
        Interval _8010 = _8002.v;
        Interval _5634 = Interval{ _7923, _7924 };
        Interval _5635 = _8010;
        Interval _8014 = imul(_5634, _5635, intervalFailed, optical_product_upper);
        Interval _5636 = Interval{ _7925, _7926 };
        Interval _5637 = _8010;
        Interval _8016 = jet_mul_derivative(_5636, _5637, intervalFailed, optical_product_upper);
        Interval _5638 = _8016;
        Interval _5639 = Interval{ _7923, _7924 };
        Interval _5640 = _8002.dx;
        Interval _8018 = jet_mul_derivative(_5639, _5640, intervalFailed, optical_product_upper);
        Interval _5641 = _8018;
        Interval _8019 = jet_add_derivative(_5638, _5641, intervalFailed);
        Interval _5642 = Interval{ _7927, _7928 };
        Interval _5643 = _8010;
        Interval _8021 = jet_mul_derivative(_5642, _5643, intervalFailed, optical_product_upper);
        Interval _5644 = _8021;
        Interval _5645 = Interval{ _7923, _7924 };
        Interval _5646 = _8002.dy;
        Interval _8023 = jet_mul_derivative(_5645, _5646, intervalFailed, optical_product_upper);
        Interval _5647 = _8023;
        Interval _8024 = jet_add_derivative(_5644, _5647, intervalFailed);
        Interval _8025 = _8002.v;
        Interval _5620 = Interval{ _7929, _7930 };
        Interval _5621 = _8025;
        Interval _8029 = imul(_5620, _5621, intervalFailed, optical_product_upper);
        Interval _5622 = Interval{ _7931, _7932 };
        Interval _5623 = _8025;
        Interval _8031 = jet_mul_derivative(_5622, _5623, intervalFailed, optical_product_upper);
        Interval _5624 = _8031;
        Interval _5625 = Interval{ _7929, _7930 };
        Interval _5626 = _8002.dx;
        Interval _8033 = jet_mul_derivative(_5625, _5626, intervalFailed, optical_product_upper);
        Interval _5627 = _8033;
        Interval _8034 = jet_add_derivative(_5624, _5627, intervalFailed);
        Interval _5628 = Interval{ _7933, _7934 };
        Interval _5629 = _8025;
        Interval _8036 = jet_mul_derivative(_5628, _5629, intervalFailed, optical_product_upper);
        Interval _5630 = _8036;
        Interval _5631 = Interval{ _7929, _7930 };
        Interval _5632 = _8002.dy;
        Interval _8038 = jet_mul_derivative(_5631, _5632, intervalFailed, optical_product_upper);
        Interval _5633 = _8038;
        Interval _8039 = jet_add_derivative(_5630, _5633, intervalFailed);
        Interval _8040 = _8002.v;
        Interval _5606 = Interval{ _7935, _7936 };
        Interval _5607 = _8040;
        Interval _8044 = imul(_5606, _5607, intervalFailed, optical_product_upper);
        Interval _5608 = Interval{ _7937, _7938 };
        Interval _5609 = _8040;
        Interval _8046 = jet_mul_derivative(_5608, _5609, intervalFailed, optical_product_upper);
        Interval _5610 = _8046;
        Interval _5611 = Interval{ _7935, _7936 };
        Interval _5612 = _8002.dx;
        Interval _8048 = jet_mul_derivative(_5611, _5612, intervalFailed, optical_product_upper);
        Interval _5613 = _8048;
        Interval _8049 = jet_add_derivative(_5610, _5613, intervalFailed);
        Interval _5614 = Interval{ _7939, _7940 };
        Interval _5615 = _8040;
        Interval _8051 = jet_mul_derivative(_5614, _5615, intervalFailed, optical_product_upper);
        Interval _5616 = _8051;
        Interval _5617 = Interval{ _7935, _7936 };
        Interval _5618 = _8002.dy;
        Interval _8053 = jet_mul_derivative(_5617, _5618, intervalFailed, optical_product_upper);
        Interval _5619 = _8053;
        Interval _8054 = jet_add_derivative(_5616, _5619, intervalFailed);
        Interval _5600 = hit.x.v;
        Interval _5601 = _8014;
        Interval _8058 = iadd(_5600, _5601, intervalFailed);
        Interval _5602 = hit.x.dx;
        Interval _5603 = _8019;
        Interval _8059 = jet_add_derivative(_5602, _5603, intervalFailed);
        Interval _5604 = hit.x.dy;
        Interval _5605 = _8024;
        Interval _8060 = jet_add_derivative(_5604, _5605, intervalFailed);
        Interval _5594 = hit.y.v;
        Interval _5595 = _8029;
        Interval _8064 = iadd(_5594, _5595, intervalFailed);
        Interval _5596 = hit.y.dx;
        Interval _5597 = _8034;
        Interval _8065 = jet_add_derivative(_5596, _5597, intervalFailed);
        Interval _5598 = hit.y.dy;
        Interval _5599 = _8039;
        Interval _8066 = jet_add_derivative(_5598, _5599, intervalFailed);
        Interval _5588 = hit.z.v;
        Interval _5589 = _8044;
        Interval _8070 = iadd(_5588, _5589, intervalFailed);
        Interval _5590 = hit.z.dx;
        Interval _5591 = _8049;
        Interval _8071 = jet_add_derivative(_5590, _5591, intervalFailed);
        Interval _5592 = hit.z.dy;
        Interval _5593 = _8054;
        Interval _8072 = jet_add_derivative(_5592, _5593, intervalFailed);
        _6800 = _8058.lo;
        _6802 = _8058.hi;
        _6804 = _8059.lo;
        _6806 = _8059.hi;
        _6808 = _8060.lo;
        _6810 = _8060.hi;
        _6812 = _8064.lo;
        _6814 = _8064.hi;
        _6816 = _8065.lo;
        _6818 = _8065.hi;
        _6820 = _8066.lo;
        _6822 = _8066.hi;
        _6824 = _8070.lo;
        _6826 = _8070.hi;
        _6828 = _8071.lo;
        _6830 = _8071.hi;
        _6832 = _8072.lo;
        _6834 = _8072.hi;
        Interval _5574 = Interval{ _6763, _6765 };
        Interval _5575 = Interval{ _7923, _7924 };
        Interval _8075 = imul(_5574, _5575, intervalFailed, optical_product_upper);
        Interval _5576 = Interval{ _6767, _6769 };
        Interval _5577 = Interval{ _7923, _7924 };
        Interval _8078 = jet_mul_derivative(_5576, _5577, intervalFailed, optical_product_upper);
        Interval _5578 = _8078;
        Interval _5579 = Interval{ _6763, _6765 };
        Interval _5580 = Interval{ _7925, _7926 };
        Interval _8081 = jet_mul_derivative(_5579, _5580, intervalFailed, optical_product_upper);
        Interval _5581 = _8081;
        Interval _8082 = jet_add_derivative(_5578, _5581, intervalFailed);
        Interval _5582 = Interval{ _6771, _6773 };
        Interval _5583 = Interval{ _7923, _7924 };
        Interval _8085 = jet_mul_derivative(_5582, _5583, intervalFailed, optical_product_upper);
        Interval _5584 = _8085;
        Interval _5585 = Interval{ _6763, _6765 };
        Interval _5586 = Interval{ _7927, _7928 };
        Interval _8088 = jet_mul_derivative(_5585, _5586, intervalFailed, optical_product_upper);
        Interval _5587 = _8088;
        Interval _8089 = jet_add_derivative(_5584, _5587, intervalFailed);
        Interval _5560 = Interval{ _6775, _6777 };
        Interval _5561 = Interval{ _7929, _7930 };
        Interval _8092 = imul(_5560, _5561, intervalFailed, optical_product_upper);
        Interval _5562 = Interval{ _6779, _6781 };
        Interval _5563 = Interval{ _7929, _7930 };
        Interval _8095 = jet_mul_derivative(_5562, _5563, intervalFailed, optical_product_upper);
        Interval _5564 = _8095;
        Interval _5565 = Interval{ _6775, _6777 };
        Interval _5566 = Interval{ _7931, _7932 };
        Interval _8098 = jet_mul_derivative(_5565, _5566, intervalFailed, optical_product_upper);
        Interval _5567 = _8098;
        Interval _8099 = jet_add_derivative(_5564, _5567, intervalFailed);
        Interval _5568 = Interval{ _6783, _6785 };
        Interval _5569 = Interval{ _7929, _7930 };
        Interval _8102 = jet_mul_derivative(_5568, _5569, intervalFailed, optical_product_upper);
        Interval _5570 = _8102;
        Interval _5571 = Interval{ _6775, _6777 };
        Interval _5572 = Interval{ _7933, _7934 };
        Interval _8105 = jet_mul_derivative(_5571, _5572, intervalFailed, optical_product_upper);
        Interval _5573 = _8105;
        Interval _8106 = jet_add_derivative(_5570, _5573, intervalFailed);
        Interval _5554 = _8075;
        Interval _5555 = _8092;
        Interval _8107 = iadd(_5554, _5555, intervalFailed);
        Interval _5556 = _8082;
        Interval _5557 = _8099;
        Interval _8108 = jet_add_derivative(_5556, _5557, intervalFailed);
        Interval _5558 = _8089;
        Interval _5559 = _8106;
        Interval _8109 = jet_add_derivative(_5558, _5559, intervalFailed);
        Interval _5540 = Interval{ _6787, _6789 };
        Interval _5541 = Interval{ _7935, _7936 };
        Interval _8112 = imul(_5540, _5541, intervalFailed, optical_product_upper);
        Interval _5542 = Interval{ _6791, _6793 };
        Interval _5543 = Interval{ _7935, _7936 };
        Interval _8115 = jet_mul_derivative(_5542, _5543, intervalFailed, optical_product_upper);
        Interval _5544 = _8115;
        Interval _5545 = Interval{ _6787, _6789 };
        Interval _5546 = Interval{ _7937, _7938 };
        Interval _8118 = jet_mul_derivative(_5545, _5546, intervalFailed, optical_product_upper);
        Interval _5547 = _8118;
        Interval _8119 = jet_add_derivative(_5544, _5547, intervalFailed);
        Interval _5548 = Interval{ _6795, _6797 };
        Interval _5549 = Interval{ _7935, _7936 };
        Interval _8122 = jet_mul_derivative(_5548, _5549, intervalFailed, optical_product_upper);
        Interval _5550 = _8122;
        Interval _5551 = Interval{ _6787, _6789 };
        Interval _5552 = Interval{ _7939, _7940 };
        Interval _8125 = jet_mul_derivative(_5551, _5552, intervalFailed, optical_product_upper);
        Interval _5553 = _8125;
        Interval _8126 = jet_add_derivative(_5550, _5553, intervalFailed);
        Interval _5534 = _8107;
        Interval _5535 = _8112;
        Interval _8127 = iadd(_5534, _5535, intervalFailed);
        Interval _5536 = _8108;
        Interval _5537 = _8119;
        Interval _8128 = jet_add_derivative(_5536, _5537, intervalFailed);
        Interval _5538 = _8109;
        Interval _5539 = _8126;
        Interval _8129 = jet_add_derivative(_5538, _5539, intervalFailed);
        Interval _5520 = Interval{ 2.0, 2.0 };
        Interval _5521 = _8127;
        Interval _8130 = imul(_5520, _5521, intervalFailed, optical_product_upper);
        Interval _5522 = Interval{ 0.0, 0.0 };
        Interval _5523 = _8127;
        Interval _8131 = jet_mul_derivative(_5522, _5523, intervalFailed, optical_product_upper);
        Interval _5524 = _8131;
        Interval _5525 = Interval{ 2.0, 2.0 };
        Interval _5526 = _8128;
        Interval _8132 = jet_mul_derivative(_5525, _5526, intervalFailed, optical_product_upper);
        Interval _5527 = _8132;
        Interval _8133 = jet_add_derivative(_5524, _5527, intervalFailed);
        Interval _5528 = Interval{ 0.0, 0.0 };
        Interval _5529 = _8127;
        Interval _8134 = jet_mul_derivative(_5528, _5529, intervalFailed, optical_product_upper);
        Interval _5530 = _8134;
        Interval _5531 = Interval{ 2.0, 2.0 };
        Interval _5532 = _8129;
        Interval _8135 = jet_mul_derivative(_5531, _5532, intervalFailed, optical_product_upper);
        Interval _5533 = _8135;
        Interval _8136 = jet_add_derivative(_5530, _5533, intervalFailed);
        Interval _5506 = Interval{ _7923, _7924 };
        Interval _5507 = _8130;
        Interval _8138 = imul(_5506, _5507, intervalFailed, optical_product_upper);
        Interval _5508 = Interval{ _7925, _7926 };
        Interval _5509 = _8130;
        Interval _8140 = jet_mul_derivative(_5508, _5509, intervalFailed, optical_product_upper);
        Interval _5510 = _8140;
        Interval _5511 = Interval{ _7923, _7924 };
        Interval _5512 = _8133;
        Interval _8142 = jet_mul_derivative(_5511, _5512, intervalFailed, optical_product_upper);
        Interval _5513 = _8142;
        Interval _8143 = jet_add_derivative(_5510, _5513, intervalFailed);
        Interval _5514 = Interval{ _7927, _7928 };
        Interval _5515 = _8130;
        Interval _8145 = jet_mul_derivative(_5514, _5515, intervalFailed, optical_product_upper);
        Interval _5516 = _8145;
        Interval _5517 = Interval{ _7923, _7924 };
        Interval _5518 = _8136;
        Interval _8147 = jet_mul_derivative(_5517, _5518, intervalFailed, optical_product_upper);
        Interval _5519 = _8147;
        Interval _8148 = jet_add_derivative(_5516, _5519, intervalFailed);
        Interval _5492 = Interval{ _7929, _7930 };
        Interval _5493 = _8130;
        Interval _8150 = imul(_5492, _5493, intervalFailed, optical_product_upper);
        Interval _5494 = Interval{ _7931, _7932 };
        Interval _5495 = _8130;
        Interval _8152 = jet_mul_derivative(_5494, _5495, intervalFailed, optical_product_upper);
        Interval _5496 = _8152;
        Interval _5497 = Interval{ _7929, _7930 };
        Interval _5498 = _8133;
        Interval _8154 = jet_mul_derivative(_5497, _5498, intervalFailed, optical_product_upper);
        Interval _5499 = _8154;
        Interval _8155 = jet_add_derivative(_5496, _5499, intervalFailed);
        Interval _5500 = Interval{ _7933, _7934 };
        Interval _5501 = _8130;
        Interval _8157 = jet_mul_derivative(_5500, _5501, intervalFailed, optical_product_upper);
        Interval _5502 = _8157;
        Interval _5503 = Interval{ _7929, _7930 };
        Interval _5504 = _8136;
        Interval _8159 = jet_mul_derivative(_5503, _5504, intervalFailed, optical_product_upper);
        Interval _5505 = _8159;
        Interval _8160 = jet_add_derivative(_5502, _5505, intervalFailed);
        Interval _5478 = Interval{ _7935, _7936 };
        Interval _5479 = _8130;
        Interval _8162 = imul(_5478, _5479, intervalFailed, optical_product_upper);
        Interval _5480 = Interval{ _7937, _7938 };
        Interval _5481 = _8130;
        Interval _8164 = jet_mul_derivative(_5480, _5481, intervalFailed, optical_product_upper);
        Interval _5482 = _8164;
        Interval _5483 = Interval{ _7935, _7936 };
        Interval _5484 = _8133;
        Interval _8166 = jet_mul_derivative(_5483, _5484, intervalFailed, optical_product_upper);
        Interval _5485 = _8166;
        Interval _8167 = jet_add_derivative(_5482, _5485, intervalFailed);
        Interval _5486 = Interval{ _7939, _7940 };
        Interval _5487 = _8130;
        Interval _8169 = jet_mul_derivative(_5486, _5487, intervalFailed, optical_product_upper);
        Interval _5488 = _8169;
        Interval _5489 = Interval{ _7935, _7936 };
        Interval _5490 = _8136;
        Interval _8171 = jet_mul_derivative(_5489, _5490, intervalFailed, optical_product_upper);
        Interval _5491 = _8171;
        Interval _8172 = jet_add_derivative(_5488, _5491, intervalFailed);
        Interval _5472 = Interval{ _6763, _6765 };
        Interval _5473 = Interval{ as_type<float>(as_type<uint>(_8138.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8138.lo) ^ 2147483648u) };
        Interval _8247 = iadd(_5472, _5473, intervalFailed);
        Interval _5474 = Interval{ _6767, _6769 };
        Interval _5475 = Interval{ as_type<float>(as_type<uint>(_8143.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8143.lo) ^ 2147483648u) };
        Interval _8250 = jet_add_derivative(_5474, _5475, intervalFailed);
        Interval _5476 = Interval{ _6771, _6773 };
        Interval _5477 = Interval{ as_type<float>(as_type<uint>(_8148.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8148.lo) ^ 2147483648u) };
        Interval _8253 = jet_add_derivative(_5476, _5477, intervalFailed);
        Interval _5466 = Interval{ _6775, _6777 };
        Interval _5467 = Interval{ as_type<float>(as_type<uint>(_8150.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8150.lo) ^ 2147483648u) };
        Interval _8256 = iadd(_5466, _5467, intervalFailed);
        Interval _5468 = Interval{ _6779, _6781 };
        Interval _5469 = Interval{ as_type<float>(as_type<uint>(_8155.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8155.lo) ^ 2147483648u) };
        Interval _8259 = jet_add_derivative(_5468, _5469, intervalFailed);
        Interval _5470 = Interval{ _6783, _6785 };
        Interval _5471 = Interval{ as_type<float>(as_type<uint>(_8160.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8160.lo) ^ 2147483648u) };
        Interval _8262 = jet_add_derivative(_5470, _5471, intervalFailed);
        Interval _5460 = Interval{ _6787, _6789 };
        Interval _5461 = Interval{ as_type<float>(as_type<uint>(_8162.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8162.lo) ^ 2147483648u) };
        Interval _8265 = iadd(_5460, _5461, intervalFailed);
        Interval _5462 = Interval{ _6791, _6793 };
        Interval _5463 = Interval{ as_type<float>(as_type<uint>(_8167.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8167.lo) ^ 2147483648u) };
        Interval _8268 = jet_add_derivative(_5462, _5463, intervalFailed);
        Interval _5464 = Interval{ _6795, _6797 };
        Interval _5465 = Interval{ as_type<float>(as_type<uint>(_8172.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8172.lo) ^ 2147483648u) };
        Interval _8271 = jet_add_derivative(_5464, _5465, intervalFailed);
        bool _8291;
        if (_6839)
        {
            _8291 = !_6849;
        }
        else
        {
            _8291 = false;
        }
        bool _8296;
        if (_8291)
        {
            _8296 = receiver.settings.x != 0.0;
        }
        else
        {
            _8296 = false;
        }
        if (_8296)
        {
            bool _8303;
            if (_8256.lo <= 0.0)
            {
                _8303 = _8256.hi >= 0.0;
            }
            else
            {
                _8303 = false;
            }
            float _8310;
            if (_8303)
            {
                _8310 = 0.0;
            }
            else
            {
                _8310 = precise::min(abs(_8256.lo), abs(_8256.hi));
            }
            float _5458 = 0.949999988079071044921875;
            float _8314 = interval_down(_5458, intervalFailed);
            float _5459 = 0.949999988079071044921875;
            __attribute__((unused)) float _8315 = interval_up(_5459, intervalFailed);
            float _8988;
            float _8989;
            float _8990;
            float _8991;
            float _8992;
            float _8993;
            float _8994;
            float _8995;
            float _8996;
            float _8997;
            float _8998;
            float _8999;
            float _9000;
            float _9001;
            float _9002;
            float _9003;
            float _9004;
            float _9005;
            if (precise::max(abs(_8256.lo), abs(_8256.hi)) < _8314)
            {
                Interval _5444 = _8256;
                Interval _5445 = Interval{ 0.0, 0.0 };
                Interval _8654 = imul(_5444, _5445, intervalFailed, optical_product_upper);
                Interval _5446 = _8259;
                Interval _5447 = Interval{ 0.0, 0.0 };
                Interval _8655 = jet_mul_derivative(_5446, _5447, intervalFailed, optical_product_upper);
                Interval _5448 = _8655;
                Interval _5449 = _8256;
                Interval _5450 = Interval{ 0.0, 0.0 };
                Interval _8656 = jet_mul_derivative(_5449, _5450, intervalFailed, optical_product_upper);
                Interval _5451 = _8656;
                Interval _8657 = jet_add_derivative(_5448, _5451, intervalFailed);
                Interval _5452 = _8262;
                Interval _5453 = Interval{ 0.0, 0.0 };
                Interval _8658 = jet_mul_derivative(_5452, _5453, intervalFailed, optical_product_upper);
                Interval _5454 = _8658;
                Interval _5455 = _8256;
                Interval _5456 = Interval{ 0.0, 0.0 };
                Interval _8659 = jet_mul_derivative(_5455, _5456, intervalFailed, optical_product_upper);
                Interval _5457 = _8659;
                Interval _8660 = jet_add_derivative(_5454, _5457, intervalFailed);
                Interval _5430 = _8265;
                Interval _5431 = Interval{ 1.0, 1.0 };
                Interval _8661 = imul(_5430, _5431, intervalFailed, optical_product_upper);
                Interval _5432 = _8268;
                Interval _5433 = Interval{ 1.0, 1.0 };
                Interval _8662 = jet_mul_derivative(_5432, _5433, intervalFailed, optical_product_upper);
                Interval _5434 = _8662;
                Interval _5435 = _8265;
                Interval _5436 = Interval{ 0.0, 0.0 };
                Interval _8663 = jet_mul_derivative(_5435, _5436, intervalFailed, optical_product_upper);
                Interval _5437 = _8663;
                Interval _8664 = jet_add_derivative(_5434, _5437, intervalFailed);
                Interval _5438 = _8271;
                Interval _5439 = Interval{ 1.0, 1.0 };
                Interval _8665 = jet_mul_derivative(_5438, _5439, intervalFailed, optical_product_upper);
                Interval _5440 = _8665;
                Interval _5441 = _8265;
                Interval _5442 = Interval{ 0.0, 0.0 };
                Interval _8666 = jet_mul_derivative(_5441, _5442, intervalFailed, optical_product_upper);
                Interval _5443 = _8666;
                Interval _8667 = jet_add_derivative(_5440, _5443, intervalFailed);
                Interval _5424 = _8654;
                Interval _5425 = Interval{ as_type<float>(as_type<uint>(_8661.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8661.lo) ^ 2147483648u) };
                Interval _8693 = iadd(_5424, _5425, intervalFailed);
                Interval _5426 = _8657;
                Interval _5427 = Interval{ as_type<float>(as_type<uint>(_8664.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8664.lo) ^ 2147483648u) };
                Interval _8695 = jet_add_derivative(_5426, _5427, intervalFailed);
                Interval _5428 = _8660;
                Interval _5429 = Interval{ as_type<float>(as_type<uint>(_8667.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8667.lo) ^ 2147483648u) };
                Interval _8697 = jet_add_derivative(_5428, _5429, intervalFailed);
                Interval _5410 = _8265;
                Interval _5411 = Interval{ 0.0, 0.0 };
                Interval _8698 = imul(_5410, _5411, intervalFailed, optical_product_upper);
                Interval _5412 = _8268;
                Interval _5413 = Interval{ 0.0, 0.0 };
                Interval _8699 = jet_mul_derivative(_5412, _5413, intervalFailed, optical_product_upper);
                Interval _5414 = _8699;
                Interval _5415 = _8265;
                Interval _5416 = Interval{ 0.0, 0.0 };
                Interval _8700 = jet_mul_derivative(_5415, _5416, intervalFailed, optical_product_upper);
                Interval _5417 = _8700;
                Interval _8701 = jet_add_derivative(_5414, _5417, intervalFailed);
                Interval _5418 = _8271;
                Interval _5419 = Interval{ 0.0, 0.0 };
                Interval _8702 = jet_mul_derivative(_5418, _5419, intervalFailed, optical_product_upper);
                Interval _5420 = _8702;
                Interval _5421 = _8265;
                Interval _5422 = Interval{ 0.0, 0.0 };
                Interval _8703 = jet_mul_derivative(_5421, _5422, intervalFailed, optical_product_upper);
                Interval _5423 = _8703;
                Interval _8704 = jet_add_derivative(_5420, _5423, intervalFailed);
                Interval _5396 = _8247;
                Interval _5397 = Interval{ 0.0, 0.0 };
                Interval _8705 = imul(_5396, _5397, intervalFailed, optical_product_upper);
                Interval _5398 = _8250;
                Interval _5399 = Interval{ 0.0, 0.0 };
                Interval _8706 = jet_mul_derivative(_5398, _5399, intervalFailed, optical_product_upper);
                Interval _5400 = _8706;
                Interval _5401 = _8247;
                Interval _5402 = Interval{ 0.0, 0.0 };
                Interval _8707 = jet_mul_derivative(_5401, _5402, intervalFailed, optical_product_upper);
                Interval _5403 = _8707;
                Interval _8708 = jet_add_derivative(_5400, _5403, intervalFailed);
                Interval _5404 = _8253;
                Interval _5405 = Interval{ 0.0, 0.0 };
                Interval _8709 = jet_mul_derivative(_5404, _5405, intervalFailed, optical_product_upper);
                Interval _5406 = _8709;
                Interval _5407 = _8247;
                Interval _5408 = Interval{ 0.0, 0.0 };
                Interval _8710 = jet_mul_derivative(_5407, _5408, intervalFailed, optical_product_upper);
                Interval _5409 = _8710;
                Interval _8711 = jet_add_derivative(_5406, _5409, intervalFailed);
                Interval _5390 = _8698;
                Interval _5391 = Interval{ as_type<float>(as_type<uint>(_8705.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8705.lo) ^ 2147483648u) };
                Interval _8737 = iadd(_5390, _5391, intervalFailed);
                Interval _5392 = _8701;
                Interval _5393 = Interval{ as_type<float>(as_type<uint>(_8708.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8708.lo) ^ 2147483648u) };
                Interval _8739 = jet_add_derivative(_5392, _5393, intervalFailed);
                Interval _5394 = _8704;
                Interval _5395 = Interval{ as_type<float>(as_type<uint>(_8711.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8711.lo) ^ 2147483648u) };
                Interval _8741 = jet_add_derivative(_5394, _5395, intervalFailed);
                Interval _5376 = _8247;
                Interval _5377 = Interval{ 1.0, 1.0 };
                Interval _8742 = imul(_5376, _5377, intervalFailed, optical_product_upper);
                Interval _5378 = _8250;
                Interval _5379 = Interval{ 1.0, 1.0 };
                Interval _8743 = jet_mul_derivative(_5378, _5379, intervalFailed, optical_product_upper);
                Interval _5380 = _8743;
                Interval _5381 = _8247;
                Interval _5382 = Interval{ 0.0, 0.0 };
                Interval _8744 = jet_mul_derivative(_5381, _5382, intervalFailed, optical_product_upper);
                Interval _5383 = _8744;
                Interval _8745 = jet_add_derivative(_5380, _5383, intervalFailed);
                Interval _5384 = _8253;
                Interval _5385 = Interval{ 1.0, 1.0 };
                Interval _8746 = jet_mul_derivative(_5384, _5385, intervalFailed, optical_product_upper);
                Interval _5386 = _8746;
                Interval _5387 = _8247;
                Interval _5388 = Interval{ 0.0, 0.0 };
                Interval _8747 = jet_mul_derivative(_5387, _5388, intervalFailed, optical_product_upper);
                Interval _5389 = _8747;
                Interval _8748 = jet_add_derivative(_5386, _5389, intervalFailed);
                Interval _5362 = _8256;
                Interval _5363 = Interval{ 0.0, 0.0 };
                Interval _8749 = imul(_5362, _5363, intervalFailed, optical_product_upper);
                Interval _5364 = _8259;
                Interval _5365 = Interval{ 0.0, 0.0 };
                Interval _8750 = jet_mul_derivative(_5364, _5365, intervalFailed, optical_product_upper);
                Interval _5366 = _8750;
                Interval _5367 = _8256;
                Interval _5368 = Interval{ 0.0, 0.0 };
                Interval _8751 = jet_mul_derivative(_5367, _5368, intervalFailed, optical_product_upper);
                Interval _5369 = _8751;
                Interval _8752 = jet_add_derivative(_5366, _5369, intervalFailed);
                Interval _5370 = _8262;
                Interval _5371 = Interval{ 0.0, 0.0 };
                Interval _8753 = jet_mul_derivative(_5370, _5371, intervalFailed, optical_product_upper);
                Interval _5372 = _8753;
                Interval _5373 = _8256;
                Interval _5374 = Interval{ 0.0, 0.0 };
                Interval _8754 = jet_mul_derivative(_5373, _5374, intervalFailed, optical_product_upper);
                Interval _5375 = _8754;
                Interval _8755 = jet_add_derivative(_5372, _5375, intervalFailed);
                Interval _5356 = _8742;
                Interval _5357 = Interval{ as_type<float>(as_type<uint>(_8749.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8749.lo) ^ 2147483648u) };
                Interval _8781 = iadd(_5356, _5357, intervalFailed);
                Interval _5358 = _8745;
                Interval _5359 = Interval{ as_type<float>(as_type<uint>(_8752.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8752.lo) ^ 2147483648u) };
                Interval _8783 = jet_add_derivative(_5358, _5359, intervalFailed);
                Interval _5360 = _8748;
                Interval _5361 = Interval{ as_type<float>(as_type<uint>(_8755.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8755.lo) ^ 2147483648u) };
                Interval _8785 = jet_add_derivative(_5360, _5361, intervalFailed);
                bool _8792;
                if (_8693.lo <= 0.0)
                {
                    _8792 = _8693.hi >= 0.0;
                }
                else
                {
                    _8792 = false;
                }
                float _8799;
                if (_8792)
                {
                    _8799 = 0.0;
                }
                else
                {
                    _8799 = precise::min(abs(_8693.lo), abs(_8693.hi));
                }
                float _8802 = precise::max(abs(_8693.lo), abs(_8693.hi));
                float _5346 = spvFMul(_8799, _8799);
                float _8803 = interval_down(_5346, intervalFailed);
                float _5347 = spvFMul(_8802, _8802);
                float _8805 = interval_up(_5347, intervalFailed);
                Interval _5348 = Interval{ 2.0, 2.0 };
                Interval _5349 = _8693;
                Interval _8806 = imul(_5348, _5349, intervalFailed, optical_product_upper);
                Interval _5350 = _8806;
                Interval _5351 = _8695;
                Interval _8807 = jet_mul_derivative(_5350, _5351, intervalFailed, optical_product_upper);
                Interval _5352 = Interval{ 2.0, 2.0 };
                Interval _5353 = _8693;
                Interval _8808 = imul(_5352, _5353, intervalFailed, optical_product_upper);
                Interval _5354 = _8808;
                Interval _5355 = _8697;
                Interval _8809 = jet_mul_derivative(_5354, _5355, intervalFailed, optical_product_upper);
                bool _8816;
                if (_8737.lo <= 0.0)
                {
                    _8816 = _8737.hi >= 0.0;
                }
                else
                {
                    _8816 = false;
                }
                float _8823;
                if (_8816)
                {
                    _8823 = 0.0;
                }
                else
                {
                    _8823 = precise::min(abs(_8737.lo), abs(_8737.hi));
                }
                float _8826 = precise::max(abs(_8737.lo), abs(_8737.hi));
                float _5336 = spvFMul(_8823, _8823);
                float _8827 = interval_down(_5336, intervalFailed);
                float _5337 = spvFMul(_8826, _8826);
                float _8829 = interval_up(_5337, intervalFailed);
                Interval _5338 = Interval{ 2.0, 2.0 };
                Interval _5339 = _8737;
                Interval _8830 = imul(_5338, _5339, intervalFailed, optical_product_upper);
                Interval _5340 = _8830;
                Interval _5341 = _8739;
                Interval _8831 = jet_mul_derivative(_5340, _5341, intervalFailed, optical_product_upper);
                Interval _5342 = Interval{ 2.0, 2.0 };
                Interval _5343 = _8737;
                Interval _8832 = imul(_5342, _5343, intervalFailed, optical_product_upper);
                Interval _5344 = _8832;
                Interval _5345 = _8741;
                Interval _8833 = jet_mul_derivative(_5344, _5345, intervalFailed, optical_product_upper);
                Interval _5330 = Interval{ precise::max(0.0, _8803), _8805 };
                Interval _5331 = Interval{ precise::max(0.0, _8827), _8829 };
                Interval _8836 = iadd(_5330, _5331, intervalFailed);
                Interval _5332 = _8807;
                Interval _5333 = _8831;
                Interval _8837 = jet_add_derivative(_5332, _5333, intervalFailed);
                Interval _5334 = _8809;
                Interval _5335 = _8833;
                Interval _8838 = jet_add_derivative(_5334, _5335, intervalFailed);
                bool _8845;
                if (_8781.lo <= 0.0)
                {
                    _8845 = _8781.hi >= 0.0;
                }
                else
                {
                    _8845 = false;
                }
                float _8852;
                if (_8845)
                {
                    _8852 = 0.0;
                }
                else
                {
                    _8852 = precise::min(abs(_8781.lo), abs(_8781.hi));
                }
                float _8855 = precise::max(abs(_8781.lo), abs(_8781.hi));
                float _5320 = spvFMul(_8852, _8852);
                float _8856 = interval_down(_5320, intervalFailed);
                float _5321 = spvFMul(_8855, _8855);
                float _8858 = interval_up(_5321, intervalFailed);
                Interval _5322 = Interval{ 2.0, 2.0 };
                Interval _5323 = _8781;
                Interval _8859 = imul(_5322, _5323, intervalFailed, optical_product_upper);
                Interval _5324 = _8859;
                Interval _5325 = _8783;
                Interval _8860 = jet_mul_derivative(_5324, _5325, intervalFailed, optical_product_upper);
                Interval _5326 = Interval{ 2.0, 2.0 };
                Interval _5327 = _8781;
                Interval _8861 = imul(_5326, _5327, intervalFailed, optical_product_upper);
                Interval _5328 = _8861;
                Interval _5329 = _8785;
                Interval _8862 = jet_mul_derivative(_5328, _5329, intervalFailed, optical_product_upper);
                Interval _5314 = _8836;
                Interval _5315 = Interval{ precise::max(0.0, _8856), _8858 };
                Interval _8864 = iadd(_5314, _5315, intervalFailed);
                Interval _5316 = _8837;
                Interval _5317 = _8860;
                Interval _8865 = jet_add_derivative(_5316, _5317, intervalFailed);
                Interval _5318 = _8838;
                Interval _5319 = _8862;
                Interval _8866 = jet_add_derivative(_5318, _5319, intervalFailed);
                Interval _5305 = _8864;
                Interval _8868 = isqrt(_5305, intervalFailed);
                bool _8876;
                if (!intervalFailed)
                {
                    _8876 = intervalFailed;
                }
                else
                {
                    _8876 = false;
                }
                bool _8881;
                if (_8876)
                {
                    _8881 = jetFailureSite == 0u;
                }
                else
                {
                    _8881 = false;
                }
                if (_8881)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_8864.lo, _8864.hi, 0.0, 0.0);
                }
                if (_8868.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5306 = Interval{ 2.0, 2.0 };
                Interval _5307 = _8868;
                Interval _8888 = imul(_5306, _5307, intervalFailed, optical_product_upper);
                Interval _5308 = Interval{ 1.0, 1.0 };
                Interval _5309 = _8888;
                Interval _8890 = idiv(_5308, _5309, intervalFailed, interval_divide_upper);
                bool _8897;
                if (!intervalFailed)
                {
                    _8897 = intervalFailed;
                }
                else
                {
                    _8897 = false;
                }
                bool _8902;
                if (_8897)
                {
                    _8902 = jetFailureSite == 0u;
                }
                else
                {
                    _8902 = false;
                }
                if (_8902)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _8888.lo, _8888.hi);
                }
                Interval _5310 = _8890;
                Interval _5311 = _8865;
                Interval _8906 = jet_mul_derivative(_5310, _5311, intervalFailed, optical_product_upper);
                Interval _5312 = _8890;
                Interval _5313 = _8866;
                Interval _8907 = jet_mul_derivative(_5312, _5313, intervalFailed, optical_product_upper);
                Interval _5291 = Interval{ 1.0, 1.0 };
                Interval _5292 = _8868;
                Interval _8909 = idiv(_5291, _5292, intervalFailed, interval_divide_upper);
                bool _8916;
                if (!intervalFailed)
                {
                    _8916 = intervalFailed;
                }
                else
                {
                    _8916 = false;
                }
                bool _8921;
                if (_8916)
                {
                    _8921 = jetFailureSite == 0u;
                }
                else
                {
                    _8921 = false;
                }
                if (_8921)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _8868.lo, _8868.hi);
                }
                Interval _5293 = Interval{ 0.0, 0.0 };
                Interval _5294 = _8909;
                Interval _5295 = _8906;
                Interval _8925 = jet_mul_derivative(_5294, _5295, intervalFailed, optical_product_upper);
                Interval _5296 = Interval{ as_type<float>(as_type<uint>(_8925.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8925.lo) ^ 2147483648u) };
                Interval _8935 = jet_add_derivative(_5293, _5296, intervalFailed);
                Interval _5297 = _8935;
                Interval _5298 = _8868;
                Interval _8936 = jet_div_derivative(_5297, _5298, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5299 = Interval{ 0.0, 0.0 };
                Interval _5300 = _8909;
                Interval _5301 = _8907;
                Interval _8937 = jet_mul_derivative(_5300, _5301, intervalFailed, optical_product_upper);
                Interval _5302 = Interval{ as_type<float>(as_type<uint>(_8937.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8937.lo) ^ 2147483648u) };
                Interval _8947 = jet_add_derivative(_5299, _5302, intervalFailed);
                Interval _5303 = _8947;
                Interval _5304 = _8868;
                Interval _8948 = jet_div_derivative(_5303, _5304, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5277 = _8693;
                Interval _5278 = _8909;
                Interval _8949 = imul(_5277, _5278, intervalFailed, optical_product_upper);
                Interval _5279 = _8695;
                Interval _5280 = _8909;
                Interval _8950 = jet_mul_derivative(_5279, _5280, intervalFailed, optical_product_upper);
                Interval _5281 = _8950;
                Interval _5282 = _8693;
                Interval _5283 = _8936;
                Interval _8951 = jet_mul_derivative(_5282, _5283, intervalFailed, optical_product_upper);
                Interval _5284 = _8951;
                Interval _8952 = jet_add_derivative(_5281, _5284, intervalFailed);
                Interval _5285 = _8697;
                Interval _5286 = _8909;
                Interval _8953 = jet_mul_derivative(_5285, _5286, intervalFailed, optical_product_upper);
                Interval _5287 = _8953;
                Interval _5288 = _8693;
                Interval _5289 = _8948;
                Interval _8954 = jet_mul_derivative(_5288, _5289, intervalFailed, optical_product_upper);
                Interval _5290 = _8954;
                Interval _8955 = jet_add_derivative(_5287, _5290, intervalFailed);
                Interval _5263 = _8737;
                Interval _5264 = _8909;
                Interval _8956 = imul(_5263, _5264, intervalFailed, optical_product_upper);
                Interval _5265 = _8739;
                Interval _5266 = _8909;
                Interval _8957 = jet_mul_derivative(_5265, _5266, intervalFailed, optical_product_upper);
                Interval _5267 = _8957;
                Interval _5268 = _8737;
                Interval _5269 = _8936;
                Interval _8958 = jet_mul_derivative(_5268, _5269, intervalFailed, optical_product_upper);
                Interval _5270 = _8958;
                Interval _8959 = jet_add_derivative(_5267, _5270, intervalFailed);
                Interval _5271 = _8741;
                Interval _5272 = _8909;
                Interval _8960 = jet_mul_derivative(_5271, _5272, intervalFailed, optical_product_upper);
                Interval _5273 = _8960;
                Interval _5274 = _8737;
                Interval _5275 = _8948;
                Interval _8961 = jet_mul_derivative(_5274, _5275, intervalFailed, optical_product_upper);
                Interval _5276 = _8961;
                Interval _8962 = jet_add_derivative(_5273, _5276, intervalFailed);
                Interval _5249 = _8781;
                Interval _5250 = _8909;
                Interval _8963 = imul(_5249, _5250, intervalFailed, optical_product_upper);
                Interval _5251 = _8783;
                Interval _5252 = _8909;
                Interval _8964 = jet_mul_derivative(_5251, _5252, intervalFailed, optical_product_upper);
                Interval _5253 = _8964;
                Interval _5254 = _8781;
                Interval _5255 = _8936;
                Interval _8965 = jet_mul_derivative(_5254, _5255, intervalFailed, optical_product_upper);
                Interval _5256 = _8965;
                Interval _8966 = jet_add_derivative(_5253, _5256, intervalFailed);
                Interval _5257 = _8785;
                Interval _5258 = _8909;
                Interval _8967 = jet_mul_derivative(_5257, _5258, intervalFailed, optical_product_upper);
                Interval _5259 = _8967;
                Interval _5260 = _8781;
                Interval _5261 = _8948;
                Interval _8968 = jet_mul_derivative(_5260, _5261, intervalFailed, optical_product_upper);
                Interval _5262 = _8968;
                Interval _8969 = jet_add_derivative(_5259, _5262, intervalFailed);
                _8988 = _8949.lo;
                _8989 = _8949.hi;
                _8990 = _8952.lo;
                _8991 = _8952.hi;
                _8992 = _8955.lo;
                _8993 = _8955.hi;
                _8994 = _8956.lo;
                _8995 = _8956.hi;
                _8996 = _8959.lo;
                _8997 = _8959.hi;
                _8998 = _8962.lo;
                _8999 = _8962.hi;
                _9000 = _8963.lo;
                _9001 = _8963.hi;
                _9002 = _8966.lo;
                _9003 = _8966.hi;
                _9004 = _8969.lo;
                _9005 = _8969.hi;
            }
            else
            {
                float _8636;
                float _8637;
                float _8638;
                float _8639;
                float _8640;
                float _8641;
                float _8642;
                float _8643;
                float _8644;
                float _8645;
                float _8646;
                float _8647;
                float _8648;
                float _8649;
                float _8650;
                float _8651;
                float _8652;
                float _8653;
                float _5247 = 0.949999988079071044921875;
                __attribute__((unused)) float _8317 = interval_down(_5247, intervalFailed);
                float _5248 = 0.949999988079071044921875;
                float _8318 = interval_up(_5248, intervalFailed);
                if (_8310 > _8318)
                {
                    Interval _5233 = _8256;
                    Interval _5234 = Interval{ 0.0, 0.0 };
                    Interval _8320 = imul(_5233, _5234, intervalFailed, optical_product_upper);
                    Interval _5235 = _8259;
                    Interval _5236 = Interval{ 0.0, 0.0 };
                    Interval _8321 = jet_mul_derivative(_5235, _5236, intervalFailed, optical_product_upper);
                    Interval _5237 = _8321;
                    Interval _5238 = _8256;
                    Interval _5239 = Interval{ 0.0, 0.0 };
                    Interval _8322 = jet_mul_derivative(_5238, _5239, intervalFailed, optical_product_upper);
                    Interval _5240 = _8322;
                    Interval _8323 = jet_add_derivative(_5237, _5240, intervalFailed);
                    Interval _5241 = _8262;
                    Interval _5242 = Interval{ 0.0, 0.0 };
                    Interval _8324 = jet_mul_derivative(_5241, _5242, intervalFailed, optical_product_upper);
                    Interval _5243 = _8324;
                    Interval _5244 = _8256;
                    Interval _5245 = Interval{ 0.0, 0.0 };
                    Interval _8325 = jet_mul_derivative(_5244, _5245, intervalFailed, optical_product_upper);
                    Interval _5246 = _8325;
                    Interval _8326 = jet_add_derivative(_5243, _5246, intervalFailed);
                    Interval _5219 = _8265;
                    Interval _5220 = Interval{ 0.0, 0.0 };
                    Interval _8327 = imul(_5219, _5220, intervalFailed, optical_product_upper);
                    Interval _5221 = _8268;
                    Interval _5222 = Interval{ 0.0, 0.0 };
                    Interval _8328 = jet_mul_derivative(_5221, _5222, intervalFailed, optical_product_upper);
                    Interval _5223 = _8328;
                    Interval _5224 = _8265;
                    Interval _5225 = Interval{ 0.0, 0.0 };
                    Interval _8329 = jet_mul_derivative(_5224, _5225, intervalFailed, optical_product_upper);
                    Interval _5226 = _8329;
                    Interval _8330 = jet_add_derivative(_5223, _5226, intervalFailed);
                    Interval _5227 = _8271;
                    Interval _5228 = Interval{ 0.0, 0.0 };
                    Interval _8331 = jet_mul_derivative(_5227, _5228, intervalFailed, optical_product_upper);
                    Interval _5229 = _8331;
                    Interval _5230 = _8265;
                    Interval _5231 = Interval{ 0.0, 0.0 };
                    Interval _8332 = jet_mul_derivative(_5230, _5231, intervalFailed, optical_product_upper);
                    Interval _5232 = _8332;
                    Interval _8333 = jet_add_derivative(_5229, _5232, intervalFailed);
                    Interval _5213 = _8320;
                    Interval _5214 = Interval{ as_type<float>(as_type<uint>(_8327.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8327.lo) ^ 2147483648u) };
                    Interval _8359 = iadd(_5213, _5214, intervalFailed);
                    Interval _5215 = _8323;
                    Interval _5216 = Interval{ as_type<float>(as_type<uint>(_8330.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8330.lo) ^ 2147483648u) };
                    Interval _8361 = jet_add_derivative(_5215, _5216, intervalFailed);
                    Interval _5217 = _8326;
                    Interval _5218 = Interval{ as_type<float>(as_type<uint>(_8333.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8333.lo) ^ 2147483648u) };
                    Interval _8363 = jet_add_derivative(_5217, _5218, intervalFailed);
                    Interval _5199 = _8265;
                    Interval _5200 = Interval{ 1.0, 1.0 };
                    Interval _8364 = imul(_5199, _5200, intervalFailed, optical_product_upper);
                    Interval _5201 = _8268;
                    Interval _5202 = Interval{ 1.0, 1.0 };
                    Interval _8365 = jet_mul_derivative(_5201, _5202, intervalFailed, optical_product_upper);
                    Interval _5203 = _8365;
                    Interval _5204 = _8265;
                    Interval _5205 = Interval{ 0.0, 0.0 };
                    Interval _8366 = jet_mul_derivative(_5204, _5205, intervalFailed, optical_product_upper);
                    Interval _5206 = _8366;
                    Interval _8367 = jet_add_derivative(_5203, _5206, intervalFailed);
                    Interval _5207 = _8271;
                    Interval _5208 = Interval{ 1.0, 1.0 };
                    Interval _8368 = jet_mul_derivative(_5207, _5208, intervalFailed, optical_product_upper);
                    Interval _5209 = _8368;
                    Interval _5210 = _8265;
                    Interval _5211 = Interval{ 0.0, 0.0 };
                    Interval _8369 = jet_mul_derivative(_5210, _5211, intervalFailed, optical_product_upper);
                    Interval _5212 = _8369;
                    Interval _8370 = jet_add_derivative(_5209, _5212, intervalFailed);
                    Interval _5185 = _8247;
                    Interval _5186 = Interval{ 0.0, 0.0 };
                    Interval _8371 = imul(_5185, _5186, intervalFailed, optical_product_upper);
                    Interval _5187 = _8250;
                    Interval _5188 = Interval{ 0.0, 0.0 };
                    Interval _8372 = jet_mul_derivative(_5187, _5188, intervalFailed, optical_product_upper);
                    Interval _5189 = _8372;
                    Interval _5190 = _8247;
                    Interval _5191 = Interval{ 0.0, 0.0 };
                    Interval _8373 = jet_mul_derivative(_5190, _5191, intervalFailed, optical_product_upper);
                    Interval _5192 = _8373;
                    Interval _8374 = jet_add_derivative(_5189, _5192, intervalFailed);
                    Interval _5193 = _8253;
                    Interval _5194 = Interval{ 0.0, 0.0 };
                    Interval _8375 = jet_mul_derivative(_5193, _5194, intervalFailed, optical_product_upper);
                    Interval _5195 = _8375;
                    Interval _5196 = _8247;
                    Interval _5197 = Interval{ 0.0, 0.0 };
                    Interval _8376 = jet_mul_derivative(_5196, _5197, intervalFailed, optical_product_upper);
                    Interval _5198 = _8376;
                    Interval _8377 = jet_add_derivative(_5195, _5198, intervalFailed);
                    Interval _5179 = _8364;
                    Interval _5180 = Interval{ as_type<float>(as_type<uint>(_8371.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8371.lo) ^ 2147483648u) };
                    Interval _8403 = iadd(_5179, _5180, intervalFailed);
                    Interval _5181 = _8367;
                    Interval _5182 = Interval{ as_type<float>(as_type<uint>(_8374.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8374.lo) ^ 2147483648u) };
                    Interval _8405 = jet_add_derivative(_5181, _5182, intervalFailed);
                    Interval _5183 = _8370;
                    Interval _5184 = Interval{ as_type<float>(as_type<uint>(_8377.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8377.lo) ^ 2147483648u) };
                    Interval _8407 = jet_add_derivative(_5183, _5184, intervalFailed);
                    Interval _5165 = _8247;
                    Interval _5166 = Interval{ 0.0, 0.0 };
                    Interval _8408 = imul(_5165, _5166, intervalFailed, optical_product_upper);
                    Interval _5167 = _8250;
                    Interval _5168 = Interval{ 0.0, 0.0 };
                    Interval _8409 = jet_mul_derivative(_5167, _5168, intervalFailed, optical_product_upper);
                    Interval _5169 = _8409;
                    Interval _5170 = _8247;
                    Interval _5171 = Interval{ 0.0, 0.0 };
                    Interval _8410 = jet_mul_derivative(_5170, _5171, intervalFailed, optical_product_upper);
                    Interval _5172 = _8410;
                    Interval _8411 = jet_add_derivative(_5169, _5172, intervalFailed);
                    Interval _5173 = _8253;
                    Interval _5174 = Interval{ 0.0, 0.0 };
                    Interval _8412 = jet_mul_derivative(_5173, _5174, intervalFailed, optical_product_upper);
                    Interval _5175 = _8412;
                    Interval _5176 = _8247;
                    Interval _5177 = Interval{ 0.0, 0.0 };
                    Interval _8413 = jet_mul_derivative(_5176, _5177, intervalFailed, optical_product_upper);
                    Interval _5178 = _8413;
                    Interval _8414 = jet_add_derivative(_5175, _5178, intervalFailed);
                    Interval _5151 = _8256;
                    Interval _5152 = Interval{ 1.0, 1.0 };
                    Interval _8415 = imul(_5151, _5152, intervalFailed, optical_product_upper);
                    Interval _5153 = _8259;
                    Interval _5154 = Interval{ 1.0, 1.0 };
                    Interval _8416 = jet_mul_derivative(_5153, _5154, intervalFailed, optical_product_upper);
                    Interval _5155 = _8416;
                    Interval _5156 = _8256;
                    Interval _5157 = Interval{ 0.0, 0.0 };
                    Interval _8417 = jet_mul_derivative(_5156, _5157, intervalFailed, optical_product_upper);
                    Interval _5158 = _8417;
                    Interval _8418 = jet_add_derivative(_5155, _5158, intervalFailed);
                    Interval _5159 = _8262;
                    Interval _5160 = Interval{ 1.0, 1.0 };
                    Interval _8419 = jet_mul_derivative(_5159, _5160, intervalFailed, optical_product_upper);
                    Interval _5161 = _8419;
                    Interval _5162 = _8256;
                    Interval _5163 = Interval{ 0.0, 0.0 };
                    Interval _8420 = jet_mul_derivative(_5162, _5163, intervalFailed, optical_product_upper);
                    Interval _5164 = _8420;
                    Interval _8421 = jet_add_derivative(_5161, _5164, intervalFailed);
                    Interval _5145 = _8408;
                    Interval _5146 = Interval{ as_type<float>(as_type<uint>(_8415.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8415.lo) ^ 2147483648u) };
                    Interval _8447 = iadd(_5145, _5146, intervalFailed);
                    Interval _5147 = _8411;
                    Interval _5148 = Interval{ as_type<float>(as_type<uint>(_8418.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8418.lo) ^ 2147483648u) };
                    Interval _8449 = jet_add_derivative(_5147, _5148, intervalFailed);
                    Interval _5149 = _8414;
                    Interval _5150 = Interval{ as_type<float>(as_type<uint>(_8421.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8421.lo) ^ 2147483648u) };
                    Interval _8451 = jet_add_derivative(_5149, _5150, intervalFailed);
                    bool _8458;
                    if (_8359.lo <= 0.0)
                    {
                        _8458 = _8359.hi >= 0.0;
                    }
                    else
                    {
                        _8458 = false;
                    }
                    float _8465;
                    if (_8458)
                    {
                        _8465 = 0.0;
                    }
                    else
                    {
                        _8465 = precise::min(abs(_8359.lo), abs(_8359.hi));
                    }
                    float _8468 = precise::max(abs(_8359.lo), abs(_8359.hi));
                    float _5135 = spvFMul(_8465, _8465);
                    float _8469 = interval_down(_5135, intervalFailed);
                    float _5136 = spvFMul(_8468, _8468);
                    float _8471 = interval_up(_5136, intervalFailed);
                    Interval _5137 = Interval{ 2.0, 2.0 };
                    Interval _5138 = _8359;
                    Interval _8472 = imul(_5137, _5138, intervalFailed, optical_product_upper);
                    Interval _5139 = _8472;
                    Interval _5140 = _8361;
                    Interval _8473 = jet_mul_derivative(_5139, _5140, intervalFailed, optical_product_upper);
                    Interval _5141 = Interval{ 2.0, 2.0 };
                    Interval _5142 = _8359;
                    Interval _8474 = imul(_5141, _5142, intervalFailed, optical_product_upper);
                    Interval _5143 = _8474;
                    Interval _5144 = _8363;
                    Interval _8475 = jet_mul_derivative(_5143, _5144, intervalFailed, optical_product_upper);
                    bool _8482;
                    if (_8403.lo <= 0.0)
                    {
                        _8482 = _8403.hi >= 0.0;
                    }
                    else
                    {
                        _8482 = false;
                    }
                    float _8489;
                    if (_8482)
                    {
                        _8489 = 0.0;
                    }
                    else
                    {
                        _8489 = precise::min(abs(_8403.lo), abs(_8403.hi));
                    }
                    float _8492 = precise::max(abs(_8403.lo), abs(_8403.hi));
                    float _5125 = spvFMul(_8489, _8489);
                    float _8493 = interval_down(_5125, intervalFailed);
                    float _5126 = spvFMul(_8492, _8492);
                    float _8495 = interval_up(_5126, intervalFailed);
                    Interval _5127 = Interval{ 2.0, 2.0 };
                    Interval _5128 = _8403;
                    Interval _8496 = imul(_5127, _5128, intervalFailed, optical_product_upper);
                    Interval _5129 = _8496;
                    Interval _5130 = _8405;
                    Interval _8497 = jet_mul_derivative(_5129, _5130, intervalFailed, optical_product_upper);
                    Interval _5131 = Interval{ 2.0, 2.0 };
                    Interval _5132 = _8403;
                    Interval _8498 = imul(_5131, _5132, intervalFailed, optical_product_upper);
                    Interval _5133 = _8498;
                    Interval _5134 = _8407;
                    Interval _8499 = jet_mul_derivative(_5133, _5134, intervalFailed, optical_product_upper);
                    Interval _5119 = Interval{ precise::max(0.0, _8469), _8471 };
                    Interval _5120 = Interval{ precise::max(0.0, _8493), _8495 };
                    Interval _8502 = iadd(_5119, _5120, intervalFailed);
                    Interval _5121 = _8473;
                    Interval _5122 = _8497;
                    Interval _8503 = jet_add_derivative(_5121, _5122, intervalFailed);
                    Interval _5123 = _8475;
                    Interval _5124 = _8499;
                    Interval _8504 = jet_add_derivative(_5123, _5124, intervalFailed);
                    bool _8511;
                    if (_8447.lo <= 0.0)
                    {
                        _8511 = _8447.hi >= 0.0;
                    }
                    else
                    {
                        _8511 = false;
                    }
                    float _8518;
                    if (_8511)
                    {
                        _8518 = 0.0;
                    }
                    else
                    {
                        _8518 = precise::min(abs(_8447.lo), abs(_8447.hi));
                    }
                    float _8521 = precise::max(abs(_8447.lo), abs(_8447.hi));
                    float _5109 = spvFMul(_8518, _8518);
                    float _8522 = interval_down(_5109, intervalFailed);
                    float _5110 = spvFMul(_8521, _8521);
                    float _8524 = interval_up(_5110, intervalFailed);
                    Interval _5111 = Interval{ 2.0, 2.0 };
                    Interval _5112 = _8447;
                    Interval _8525 = imul(_5111, _5112, intervalFailed, optical_product_upper);
                    Interval _5113 = _8525;
                    Interval _5114 = _8449;
                    Interval _8526 = jet_mul_derivative(_5113, _5114, intervalFailed, optical_product_upper);
                    Interval _5115 = Interval{ 2.0, 2.0 };
                    Interval _5116 = _8447;
                    Interval _8527 = imul(_5115, _5116, intervalFailed, optical_product_upper);
                    Interval _5117 = _8527;
                    Interval _5118 = _8451;
                    Interval _8528 = jet_mul_derivative(_5117, _5118, intervalFailed, optical_product_upper);
                    Interval _5103 = _8502;
                    Interval _5104 = Interval{ precise::max(0.0, _8522), _8524 };
                    Interval _8530 = iadd(_5103, _5104, intervalFailed);
                    Interval _5105 = _8503;
                    Interval _5106 = _8526;
                    Interval _8531 = jet_add_derivative(_5105, _5106, intervalFailed);
                    Interval _5107 = _8504;
                    Interval _5108 = _8528;
                    Interval _8532 = jet_add_derivative(_5107, _5108, intervalFailed);
                    Interval _5094 = _8530;
                    Interval _8534 = isqrt(_5094, intervalFailed);
                    bool _8542;
                    if (!intervalFailed)
                    {
                        _8542 = intervalFailed;
                    }
                    else
                    {
                        _8542 = false;
                    }
                    bool _8547;
                    if (_8542)
                    {
                        _8547 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8547 = false;
                    }
                    if (_8547)
                    {
                        jetFailureSite = 3u;
                        jetFailureArguments = float4(_8530.lo, _8530.hi, 0.0, 0.0);
                    }
                    if (_8534.lo <= 0.0)
                    {
                        jetBranchKnown = false;
                    }
                    Interval _5095 = Interval{ 2.0, 2.0 };
                    Interval _5096 = _8534;
                    Interval _8554 = imul(_5095, _5096, intervalFailed, optical_product_upper);
                    Interval _5097 = Interval{ 1.0, 1.0 };
                    Interval _5098 = _8554;
                    Interval _8556 = idiv(_5097, _5098, intervalFailed, interval_divide_upper);
                    bool _8563;
                    if (!intervalFailed)
                    {
                        _8563 = intervalFailed;
                    }
                    else
                    {
                        _8563 = false;
                    }
                    bool _8568;
                    if (_8563)
                    {
                        _8568 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8568 = false;
                    }
                    if (_8568)
                    {
                        jetFailureSite = 4u;
                        jetFailureArguments = float4(1.0, 1.0, _8554.lo, _8554.hi);
                    }
                    Interval _5099 = _8556;
                    Interval _5100 = _8531;
                    Interval _8572 = jet_mul_derivative(_5099, _5100, intervalFailed, optical_product_upper);
                    Interval _5101 = _8556;
                    Interval _5102 = _8532;
                    Interval _8573 = jet_mul_derivative(_5101, _5102, intervalFailed, optical_product_upper);
                    Interval _5080 = Interval{ 1.0, 1.0 };
                    Interval _5081 = _8534;
                    Interval _8575 = idiv(_5080, _5081, intervalFailed, interval_divide_upper);
                    bool _8582;
                    if (!intervalFailed)
                    {
                        _8582 = intervalFailed;
                    }
                    else
                    {
                        _8582 = false;
                    }
                    bool _8587;
                    if (_8582)
                    {
                        _8587 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8587 = false;
                    }
                    if (_8587)
                    {
                        jetFailureSite = 1u;
                        jetFailureArguments = float4(1.0, 1.0, _8534.lo, _8534.hi);
                    }
                    Interval _5082 = Interval{ 0.0, 0.0 };
                    Interval _5083 = _8575;
                    Interval _5084 = _8572;
                    Interval _8591 = jet_mul_derivative(_5083, _5084, intervalFailed, optical_product_upper);
                    Interval _5085 = Interval{ as_type<float>(as_type<uint>(_8591.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8591.lo) ^ 2147483648u) };
                    Interval _8601 = jet_add_derivative(_5082, _5085, intervalFailed);
                    Interval _5086 = _8601;
                    Interval _5087 = _8534;
                    Interval _8602 = jet_div_derivative(_5086, _5087, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _5088 = Interval{ 0.0, 0.0 };
                    Interval _5089 = _8575;
                    Interval _5090 = _8573;
                    Interval _8603 = jet_mul_derivative(_5089, _5090, intervalFailed, optical_product_upper);
                    Interval _5091 = Interval{ as_type<float>(as_type<uint>(_8603.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8603.lo) ^ 2147483648u) };
                    Interval _8613 = jet_add_derivative(_5088, _5091, intervalFailed);
                    Interval _5092 = _8613;
                    Interval _5093 = _8534;
                    Interval _8614 = jet_div_derivative(_5092, _5093, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _5066 = _8359;
                    Interval _5067 = _8575;
                    Interval _8615 = imul(_5066, _5067, intervalFailed, optical_product_upper);
                    Interval _5068 = _8361;
                    Interval _5069 = _8575;
                    Interval _8616 = jet_mul_derivative(_5068, _5069, intervalFailed, optical_product_upper);
                    Interval _5070 = _8616;
                    Interval _5071 = _8359;
                    Interval _5072 = _8602;
                    Interval _8617 = jet_mul_derivative(_5071, _5072, intervalFailed, optical_product_upper);
                    Interval _5073 = _8617;
                    Interval _8618 = jet_add_derivative(_5070, _5073, intervalFailed);
                    Interval _5074 = _8363;
                    Interval _5075 = _8575;
                    Interval _8619 = jet_mul_derivative(_5074, _5075, intervalFailed, optical_product_upper);
                    Interval _5076 = _8619;
                    Interval _5077 = _8359;
                    Interval _5078 = _8614;
                    Interval _8620 = jet_mul_derivative(_5077, _5078, intervalFailed, optical_product_upper);
                    Interval _5079 = _8620;
                    Interval _8621 = jet_add_derivative(_5076, _5079, intervalFailed);
                    Interval _5052 = _8403;
                    Interval _5053 = _8575;
                    Interval _8622 = imul(_5052, _5053, intervalFailed, optical_product_upper);
                    Interval _5054 = _8405;
                    Interval _5055 = _8575;
                    Interval _8623 = jet_mul_derivative(_5054, _5055, intervalFailed, optical_product_upper);
                    Interval _5056 = _8623;
                    Interval _5057 = _8403;
                    Interval _5058 = _8602;
                    Interval _8624 = jet_mul_derivative(_5057, _5058, intervalFailed, optical_product_upper);
                    Interval _5059 = _8624;
                    Interval _8625 = jet_add_derivative(_5056, _5059, intervalFailed);
                    Interval _5060 = _8407;
                    Interval _5061 = _8575;
                    Interval _8626 = jet_mul_derivative(_5060, _5061, intervalFailed, optical_product_upper);
                    Interval _5062 = _8626;
                    Interval _5063 = _8403;
                    Interval _5064 = _8614;
                    Interval _8627 = jet_mul_derivative(_5063, _5064, intervalFailed, optical_product_upper);
                    Interval _5065 = _8627;
                    Interval _8628 = jet_add_derivative(_5062, _5065, intervalFailed);
                    Interval _5038 = _8447;
                    Interval _5039 = _8575;
                    Interval _8629 = imul(_5038, _5039, intervalFailed, optical_product_upper);
                    Interval _5040 = _8449;
                    Interval _5041 = _8575;
                    Interval _8630 = jet_mul_derivative(_5040, _5041, intervalFailed, optical_product_upper);
                    Interval _5042 = _8630;
                    Interval _5043 = _8447;
                    Interval _5044 = _8602;
                    Interval _8631 = jet_mul_derivative(_5043, _5044, intervalFailed, optical_product_upper);
                    Interval _5045 = _8631;
                    Interval _8632 = jet_add_derivative(_5042, _5045, intervalFailed);
                    Interval _5046 = _8451;
                    Interval _5047 = _8575;
                    Interval _8633 = jet_mul_derivative(_5046, _5047, intervalFailed, optical_product_upper);
                    Interval _5048 = _8633;
                    Interval _5049 = _8447;
                    Interval _5050 = _8614;
                    Interval _8634 = jet_mul_derivative(_5049, _5050, intervalFailed, optical_product_upper);
                    Interval _5051 = _8634;
                    Interval _8635 = jet_add_derivative(_5048, _5051, intervalFailed);
                    _8636 = _8615.lo;
                    _8637 = _8615.hi;
                    _8638 = _8618.lo;
                    _8639 = _8618.hi;
                    _8640 = _8621.lo;
                    _8641 = _8621.hi;
                    _8642 = _8622.lo;
                    _8643 = _8622.hi;
                    _8644 = _8625.lo;
                    _8645 = _8625.hi;
                    _8646 = _8628.lo;
                    _8647 = _8628.hi;
                    _8648 = _8629.lo;
                    _8649 = _8629.hi;
                    _8650 = _8632.lo;
                    _8651 = _8632.hi;
                    _8652 = _8635.lo;
                    _8653 = _8635.hi;
                }
                else
                {
                    return false;
                }
                _8988 = _8636;
                _8989 = _8637;
                _8990 = _8638;
                _8991 = _8639;
                _8992 = _8640;
                _8993 = _8641;
                _8994 = _8642;
                _8995 = _8643;
                _8996 = _8644;
                _8997 = _8645;
                _8998 = _8646;
                _8999 = _8647;
                _9000 = _8648;
                _9001 = _8649;
                _9002 = _8650;
                _9003 = _8651;
                _9004 = _8652;
                _9005 = _8653;
            }
            uint _9009 = uint(receiver.settings.y);
            float _9013 = float(_2308[_9009].x);
            float _5036 = _9013;
            float _5037 = 1000.0;
            Interval _9015 = iratio(_5036, _5037, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _9020;
            if (!intervalFailed)
            {
                _9020 = intervalFailed;
            }
            else
            {
                _9020 = false;
            }
            bool _9025;
            if (_9020)
            {
                _9025 = jetFailureSite == 0u;
            }
            else
            {
                _9025 = false;
            }
            if (_9025)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_9013, _9013, 1000.0, 1000.0);
            }
            Interval _5022 = Interval{ _8988, _8989 };
            Interval _5023 = _9015;
            Interval _9030 = imul(_5022, _5023, intervalFailed, optical_product_upper);
            Interval _5024 = Interval{ _8990, _8991 };
            Interval _5025 = _9015;
            Interval _9032 = jet_mul_derivative(_5024, _5025, intervalFailed, optical_product_upper);
            Interval _5026 = _9032;
            Interval _5027 = Interval{ _8988, _8989 };
            Interval _5028 = Interval{ 0.0, 0.0 };
            Interval _9034 = jet_mul_derivative(_5027, _5028, intervalFailed, optical_product_upper);
            Interval _5029 = _9034;
            Interval _9035 = jet_add_derivative(_5026, _5029, intervalFailed);
            Interval _5030 = Interval{ _8992, _8993 };
            Interval _5031 = _9015;
            Interval _9037 = jet_mul_derivative(_5030, _5031, intervalFailed, optical_product_upper);
            Interval _5032 = _9037;
            Interval _5033 = Interval{ _8988, _8989 };
            Interval _5034 = Interval{ 0.0, 0.0 };
            Interval _9039 = jet_mul_derivative(_5033, _5034, intervalFailed, optical_product_upper);
            Interval _5035 = _9039;
            Interval _9040 = jet_add_derivative(_5032, _5035, intervalFailed);
            Interval _5008 = Interval{ _8994, _8995 };
            Interval _5009 = _9015;
            Interval _9042 = imul(_5008, _5009, intervalFailed, optical_product_upper);
            Interval _5010 = Interval{ _8996, _8997 };
            Interval _5011 = _9015;
            Interval _9044 = jet_mul_derivative(_5010, _5011, intervalFailed, optical_product_upper);
            Interval _5012 = _9044;
            Interval _5013 = Interval{ _8994, _8995 };
            Interval _5014 = Interval{ 0.0, 0.0 };
            Interval _9046 = jet_mul_derivative(_5013, _5014, intervalFailed, optical_product_upper);
            Interval _5015 = _9046;
            Interval _9047 = jet_add_derivative(_5012, _5015, intervalFailed);
            Interval _5016 = Interval{ _8998, _8999 };
            Interval _5017 = _9015;
            Interval _9049 = jet_mul_derivative(_5016, _5017, intervalFailed, optical_product_upper);
            Interval _5018 = _9049;
            Interval _5019 = Interval{ _8994, _8995 };
            Interval _5020 = Interval{ 0.0, 0.0 };
            Interval _9051 = jet_mul_derivative(_5019, _5020, intervalFailed, optical_product_upper);
            Interval _5021 = _9051;
            Interval _9052 = jet_add_derivative(_5018, _5021, intervalFailed);
            Interval _4994 = Interval{ _9000, _9001 };
            Interval _4995 = _9015;
            Interval _9054 = imul(_4994, _4995, intervalFailed, optical_product_upper);
            Interval _4996 = Interval{ _9002, _9003 };
            Interval _4997 = _9015;
            Interval _9056 = jet_mul_derivative(_4996, _4997, intervalFailed, optical_product_upper);
            Interval _4998 = _9056;
            Interval _4999 = Interval{ _9000, _9001 };
            Interval _5000 = Interval{ 0.0, 0.0 };
            Interval _9058 = jet_mul_derivative(_4999, _5000, intervalFailed, optical_product_upper);
            Interval _5001 = _9058;
            Interval _9059 = jet_add_derivative(_4998, _5001, intervalFailed);
            Interval _5002 = Interval{ _9004, _9005 };
            Interval _5003 = _9015;
            Interval _9061 = jet_mul_derivative(_5002, _5003, intervalFailed, optical_product_upper);
            Interval _5004 = _9061;
            Interval _5005 = Interval{ _9000, _9001 };
            Interval _5006 = Interval{ 0.0, 0.0 };
            Interval _9063 = jet_mul_derivative(_5005, _5006, intervalFailed, optical_product_upper);
            Interval _5007 = _9063;
            Interval _9064 = jet_add_derivative(_5004, _5007, intervalFailed);
            Interval _4980 = _8256;
            Interval _4981 = Interval{ _9000, _9001 };
            Interval _9066 = imul(_4980, _4981, intervalFailed, optical_product_upper);
            Interval _4982 = _8259;
            Interval _4983 = Interval{ _9000, _9001 };
            Interval _9068 = jet_mul_derivative(_4982, _4983, intervalFailed, optical_product_upper);
            Interval _4984 = _9068;
            Interval _4985 = _8256;
            Interval _4986 = Interval{ _9002, _9003 };
            Interval _9070 = jet_mul_derivative(_4985, _4986, intervalFailed, optical_product_upper);
            Interval _4987 = _9070;
            Interval _9071 = jet_add_derivative(_4984, _4987, intervalFailed);
            Interval _4988 = _8262;
            Interval _4989 = Interval{ _9000, _9001 };
            Interval _9073 = jet_mul_derivative(_4988, _4989, intervalFailed, optical_product_upper);
            Interval _4990 = _9073;
            Interval _4991 = _8256;
            Interval _4992 = Interval{ _9004, _9005 };
            Interval _9075 = jet_mul_derivative(_4991, _4992, intervalFailed, optical_product_upper);
            Interval _4993 = _9075;
            Interval _9076 = jet_add_derivative(_4990, _4993, intervalFailed);
            Interval _4966 = _8265;
            Interval _4967 = Interval{ _8994, _8995 };
            Interval _9078 = imul(_4966, _4967, intervalFailed, optical_product_upper);
            Interval _4968 = _8268;
            Interval _4969 = Interval{ _8994, _8995 };
            Interval _9080 = jet_mul_derivative(_4968, _4969, intervalFailed, optical_product_upper);
            Interval _4970 = _9080;
            Interval _4971 = _8265;
            Interval _4972 = Interval{ _8996, _8997 };
            Interval _9082 = jet_mul_derivative(_4971, _4972, intervalFailed, optical_product_upper);
            Interval _4973 = _9082;
            Interval _9083 = jet_add_derivative(_4970, _4973, intervalFailed);
            Interval _4974 = _8271;
            Interval _4975 = Interval{ _8994, _8995 };
            Interval _9085 = jet_mul_derivative(_4974, _4975, intervalFailed, optical_product_upper);
            Interval _4976 = _9085;
            Interval _4977 = _8265;
            Interval _4978 = Interval{ _8998, _8999 };
            Interval _9087 = jet_mul_derivative(_4977, _4978, intervalFailed, optical_product_upper);
            Interval _4979 = _9087;
            Interval _9088 = jet_add_derivative(_4976, _4979, intervalFailed);
            Interval _4960 = _9066;
            Interval _4961 = Interval{ as_type<float>(as_type<uint>(_9078.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9078.lo) ^ 2147483648u) };
            Interval _9114 = iadd(_4960, _4961, intervalFailed);
            Interval _4962 = _9071;
            Interval _4963 = Interval{ as_type<float>(as_type<uint>(_9083.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9083.lo) ^ 2147483648u) };
            Interval _9116 = jet_add_derivative(_4962, _4963, intervalFailed);
            Interval _4964 = _9076;
            Interval _4965 = Interval{ as_type<float>(as_type<uint>(_9088.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9088.lo) ^ 2147483648u) };
            Interval _9118 = jet_add_derivative(_4964, _4965, intervalFailed);
            Interval _4946 = _8265;
            Interval _4947 = Interval{ _8988, _8989 };
            Interval _9120 = imul(_4946, _4947, intervalFailed, optical_product_upper);
            Interval _4948 = _8268;
            Interval _4949 = Interval{ _8988, _8989 };
            Interval _9122 = jet_mul_derivative(_4948, _4949, intervalFailed, optical_product_upper);
            Interval _4950 = _9122;
            Interval _4951 = _8265;
            Interval _4952 = Interval{ _8990, _8991 };
            Interval _9124 = jet_mul_derivative(_4951, _4952, intervalFailed, optical_product_upper);
            Interval _4953 = _9124;
            Interval _9125 = jet_add_derivative(_4950, _4953, intervalFailed);
            Interval _4954 = _8271;
            Interval _4955 = Interval{ _8988, _8989 };
            Interval _9127 = jet_mul_derivative(_4954, _4955, intervalFailed, optical_product_upper);
            Interval _4956 = _9127;
            Interval _4957 = _8265;
            Interval _4958 = Interval{ _8992, _8993 };
            Interval _9129 = jet_mul_derivative(_4957, _4958, intervalFailed, optical_product_upper);
            Interval _4959 = _9129;
            Interval _9130 = jet_add_derivative(_4956, _4959, intervalFailed);
            Interval _4932 = _8247;
            Interval _4933 = Interval{ _9000, _9001 };
            Interval _9132 = imul(_4932, _4933, intervalFailed, optical_product_upper);
            Interval _4934 = _8250;
            Interval _4935 = Interval{ _9000, _9001 };
            Interval _9134 = jet_mul_derivative(_4934, _4935, intervalFailed, optical_product_upper);
            Interval _4936 = _9134;
            Interval _4937 = _8247;
            Interval _4938 = Interval{ _9002, _9003 };
            Interval _9136 = jet_mul_derivative(_4937, _4938, intervalFailed, optical_product_upper);
            Interval _4939 = _9136;
            Interval _9137 = jet_add_derivative(_4936, _4939, intervalFailed);
            Interval _4940 = _8253;
            Interval _4941 = Interval{ _9000, _9001 };
            Interval _9139 = jet_mul_derivative(_4940, _4941, intervalFailed, optical_product_upper);
            Interval _4942 = _9139;
            Interval _4943 = _8247;
            Interval _4944 = Interval{ _9004, _9005 };
            Interval _9141 = jet_mul_derivative(_4943, _4944, intervalFailed, optical_product_upper);
            Interval _4945 = _9141;
            Interval _9142 = jet_add_derivative(_4942, _4945, intervalFailed);
            Interval _4926 = _9120;
            Interval _4927 = Interval{ as_type<float>(as_type<uint>(_9132.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9132.lo) ^ 2147483648u) };
            Interval _9168 = iadd(_4926, _4927, intervalFailed);
            Interval _4928 = _9125;
            Interval _4929 = Interval{ as_type<float>(as_type<uint>(_9137.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9137.lo) ^ 2147483648u) };
            Interval _9170 = jet_add_derivative(_4928, _4929, intervalFailed);
            Interval _4930 = _9130;
            Interval _4931 = Interval{ as_type<float>(as_type<uint>(_9142.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9142.lo) ^ 2147483648u) };
            Interval _9172 = jet_add_derivative(_4930, _4931, intervalFailed);
            Interval _4912 = _8247;
            Interval _4913 = Interval{ _8994, _8995 };
            Interval _9174 = imul(_4912, _4913, intervalFailed, optical_product_upper);
            Interval _4914 = _8250;
            Interval _4915 = Interval{ _8994, _8995 };
            Interval _9176 = jet_mul_derivative(_4914, _4915, intervalFailed, optical_product_upper);
            Interval _4916 = _9176;
            Interval _4917 = _8247;
            Interval _4918 = Interval{ _8996, _8997 };
            Interval _9178 = jet_mul_derivative(_4917, _4918, intervalFailed, optical_product_upper);
            Interval _4919 = _9178;
            Interval _9179 = jet_add_derivative(_4916, _4919, intervalFailed);
            Interval _4920 = _8253;
            Interval _4921 = Interval{ _8994, _8995 };
            Interval _9181 = jet_mul_derivative(_4920, _4921, intervalFailed, optical_product_upper);
            Interval _4922 = _9181;
            Interval _4923 = _8247;
            Interval _4924 = Interval{ _8998, _8999 };
            Interval _9183 = jet_mul_derivative(_4923, _4924, intervalFailed, optical_product_upper);
            Interval _4925 = _9183;
            Interval _9184 = jet_add_derivative(_4922, _4925, intervalFailed);
            Interval _4898 = _8256;
            Interval _4899 = Interval{ _8988, _8989 };
            Interval _9186 = imul(_4898, _4899, intervalFailed, optical_product_upper);
            Interval _4900 = _8259;
            Interval _4901 = Interval{ _8988, _8989 };
            Interval _9188 = jet_mul_derivative(_4900, _4901, intervalFailed, optical_product_upper);
            Interval _4902 = _9188;
            Interval _4903 = _8256;
            Interval _4904 = Interval{ _8990, _8991 };
            Interval _9190 = jet_mul_derivative(_4903, _4904, intervalFailed, optical_product_upper);
            Interval _4905 = _9190;
            Interval _9191 = jet_add_derivative(_4902, _4905, intervalFailed);
            Interval _4906 = _8262;
            Interval _4907 = Interval{ _8988, _8989 };
            Interval _9193 = jet_mul_derivative(_4906, _4907, intervalFailed, optical_product_upper);
            Interval _4908 = _9193;
            Interval _4909 = _8256;
            Interval _4910 = Interval{ _8992, _8993 };
            Interval _9195 = jet_mul_derivative(_4909, _4910, intervalFailed, optical_product_upper);
            Interval _4911 = _9195;
            Interval _9196 = jet_add_derivative(_4908, _4911, intervalFailed);
            Interval _4892 = _9174;
            Interval _4893 = Interval{ as_type<float>(as_type<uint>(_9186.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9186.lo) ^ 2147483648u) };
            Interval _9222 = iadd(_4892, _4893, intervalFailed);
            Interval _4894 = _9179;
            Interval _4895 = Interval{ as_type<float>(as_type<uint>(_9191.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9191.lo) ^ 2147483648u) };
            Interval _9224 = jet_add_derivative(_4894, _4895, intervalFailed);
            Interval _4896 = _9184;
            Interval _4897 = Interval{ as_type<float>(as_type<uint>(_9196.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9196.lo) ^ 2147483648u) };
            Interval _9226 = jet_add_derivative(_4896, _4897, intervalFailed);
            float _9228 = float(_2308[_9009].y);
            float _4890 = _9228;
            float _4891 = 1000.0;
            Interval _9230 = iratio(_4890, _4891, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _9235;
            if (!intervalFailed)
            {
                _9235 = intervalFailed;
            }
            else
            {
                _9235 = false;
            }
            bool _9240;
            if (_9235)
            {
                _9240 = jetFailureSite == 0u;
            }
            else
            {
                _9240 = false;
            }
            if (_9240)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_9228, _9228, 1000.0, 1000.0);
            }
            Interval _4876 = _9114;
            Interval _4877 = _9230;
            Interval _9244 = imul(_4876, _4877, intervalFailed, optical_product_upper);
            Interval _4878 = _9116;
            Interval _4879 = _9230;
            Interval _9245 = jet_mul_derivative(_4878, _4879, intervalFailed, optical_product_upper);
            Interval _4880 = _9245;
            Interval _4881 = _9114;
            Interval _4882 = Interval{ 0.0, 0.0 };
            Interval _9246 = jet_mul_derivative(_4881, _4882, intervalFailed, optical_product_upper);
            Interval _4883 = _9246;
            Interval _9247 = jet_add_derivative(_4880, _4883, intervalFailed);
            Interval _4884 = _9118;
            Interval _4885 = _9230;
            Interval _9248 = jet_mul_derivative(_4884, _4885, intervalFailed, optical_product_upper);
            Interval _4886 = _9248;
            Interval _4887 = _9114;
            Interval _4888 = Interval{ 0.0, 0.0 };
            Interval _9249 = jet_mul_derivative(_4887, _4888, intervalFailed, optical_product_upper);
            Interval _4889 = _9249;
            Interval _9250 = jet_add_derivative(_4886, _4889, intervalFailed);
            Interval _4862 = _9168;
            Interval _4863 = _9230;
            Interval _9251 = imul(_4862, _4863, intervalFailed, optical_product_upper);
            Interval _4864 = _9170;
            Interval _4865 = _9230;
            Interval _9252 = jet_mul_derivative(_4864, _4865, intervalFailed, optical_product_upper);
            Interval _4866 = _9252;
            Interval _4867 = _9168;
            Interval _4868 = Interval{ 0.0, 0.0 };
            Interval _9253 = jet_mul_derivative(_4867, _4868, intervalFailed, optical_product_upper);
            Interval _4869 = _9253;
            Interval _9254 = jet_add_derivative(_4866, _4869, intervalFailed);
            Interval _4870 = _9172;
            Interval _4871 = _9230;
            Interval _9255 = jet_mul_derivative(_4870, _4871, intervalFailed, optical_product_upper);
            Interval _4872 = _9255;
            Interval _4873 = _9168;
            Interval _4874 = Interval{ 0.0, 0.0 };
            Interval _9256 = jet_mul_derivative(_4873, _4874, intervalFailed, optical_product_upper);
            Interval _4875 = _9256;
            Interval _9257 = jet_add_derivative(_4872, _4875, intervalFailed);
            Interval _4848 = _9222;
            Interval _4849 = _9230;
            Interval _9258 = imul(_4848, _4849, intervalFailed, optical_product_upper);
            Interval _4850 = _9224;
            Interval _4851 = _9230;
            Interval _9259 = jet_mul_derivative(_4850, _4851, intervalFailed, optical_product_upper);
            Interval _4852 = _9259;
            Interval _4853 = _9222;
            Interval _4854 = Interval{ 0.0, 0.0 };
            Interval _9260 = jet_mul_derivative(_4853, _4854, intervalFailed, optical_product_upper);
            Interval _4855 = _9260;
            Interval _9261 = jet_add_derivative(_4852, _4855, intervalFailed);
            Interval _4856 = _9226;
            Interval _4857 = _9230;
            Interval _9262 = jet_mul_derivative(_4856, _4857, intervalFailed, optical_product_upper);
            Interval _4858 = _9262;
            Interval _4859 = _9222;
            Interval _4860 = Interval{ 0.0, 0.0 };
            Interval _9263 = jet_mul_derivative(_4859, _4860, intervalFailed, optical_product_upper);
            Interval _4861 = _9263;
            Interval _9264 = jet_add_derivative(_4858, _4861, intervalFailed);
            Interval _4842 = _9030;
            Interval _4843 = _9244;
            Interval _9265 = iadd(_4842, _4843, intervalFailed);
            Interval _4844 = _9035;
            Interval _4845 = _9247;
            Interval _9266 = jet_add_derivative(_4844, _4845, intervalFailed);
            Interval _4846 = _9040;
            Interval _4847 = _9250;
            Interval _9267 = jet_add_derivative(_4846, _4847, intervalFailed);
            Interval _4836 = _9042;
            Interval _4837 = _9251;
            Interval _9268 = iadd(_4836, _4837, intervalFailed);
            Interval _4838 = _9047;
            Interval _4839 = _9254;
            Interval _9269 = jet_add_derivative(_4838, _4839, intervalFailed);
            Interval _4840 = _9052;
            Interval _4841 = _9257;
            Interval _9270 = jet_add_derivative(_4840, _4841, intervalFailed);
            Interval _4830 = _9054;
            Interval _4831 = _9258;
            Interval _9271 = iadd(_4830, _4831, intervalFailed);
            Interval _4832 = _9059;
            Interval _4833 = _9261;
            Interval _9272 = jet_add_derivative(_4832, _4833, intervalFailed);
            Interval _4834 = _9064;
            Interval _4835 = _9264;
            Interval _9273 = jet_add_derivative(_4834, _4835, intervalFailed);
            bool _9281;
            if (receiver.settings.x <= 0.0)
            {
                _9281 = receiver.settings.x >= 0.0;
            }
            else
            {
                _9281 = false;
            }
            float _9288;
            if (_9281)
            {
                _9288 = 0.0;
            }
            else
            {
                _9288 = precise::min(abs(receiver.settings.x), abs(receiver.settings.x));
            }
            float _9291 = precise::max(abs(receiver.settings.x), abs(receiver.settings.x));
            float _4820 = spvFMul(_9288, _9288);
            float _9292 = interval_down(_4820, intervalFailed);
            float _9293 = precise::max(0.0, _9292);
            float _4821 = spvFMul(_9291, _9291);
            float _9294 = interval_up(_4821, intervalFailed);
            Interval _4822 = Interval{ 2.0, 2.0 };
            Interval _4823 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9296 = imul(_4822, _4823, intervalFailed, optical_product_upper);
            Interval _4824 = _9296;
            Interval _4825 = Interval{ 0.0, 0.0 };
            Interval _9297 = jet_mul_derivative(_4824, _4825, intervalFailed, optical_product_upper);
            Interval _4826 = Interval{ 2.0, 2.0 };
            Interval _4827 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9299 = imul(_4826, _4827, intervalFailed, optical_product_upper);
            Interval _4828 = _9299;
            Interval _4829 = Interval{ 0.0, 0.0 };
            Interval _9300 = jet_mul_derivative(_4828, _4829, intervalFailed, optical_product_upper);
            Interval _4806 = _9265;
            Interval _4807 = Interval{ _9293, _9294 };
            Interval _9302 = imul(_4806, _4807, intervalFailed, optical_product_upper);
            Interval _4808 = _9266;
            Interval _4809 = Interval{ _9293, _9294 };
            Interval _9304 = jet_mul_derivative(_4808, _4809, intervalFailed, optical_product_upper);
            Interval _4810 = _9304;
            Interval _4811 = _9265;
            Interval _4812 = _9297;
            Interval _9305 = jet_mul_derivative(_4811, _4812, intervalFailed, optical_product_upper);
            Interval _4813 = _9305;
            Interval _9306 = jet_add_derivative(_4810, _4813, intervalFailed);
            Interval _4814 = _9267;
            Interval _4815 = Interval{ _9293, _9294 };
            Interval _9308 = jet_mul_derivative(_4814, _4815, intervalFailed, optical_product_upper);
            Interval _4816 = _9308;
            Interval _4817 = _9265;
            Interval _4818 = _9300;
            Interval _9309 = jet_mul_derivative(_4817, _4818, intervalFailed, optical_product_upper);
            Interval _4819 = _9309;
            Interval _9310 = jet_add_derivative(_4816, _4819, intervalFailed);
            Interval _4792 = _9268;
            Interval _4793 = Interval{ _9293, _9294 };
            Interval _9312 = imul(_4792, _4793, intervalFailed, optical_product_upper);
            Interval _4794 = _9269;
            Interval _4795 = Interval{ _9293, _9294 };
            Interval _9314 = jet_mul_derivative(_4794, _4795, intervalFailed, optical_product_upper);
            Interval _4796 = _9314;
            Interval _4797 = _9268;
            Interval _4798 = _9297;
            Interval _9315 = jet_mul_derivative(_4797, _4798, intervalFailed, optical_product_upper);
            Interval _4799 = _9315;
            Interval _9316 = jet_add_derivative(_4796, _4799, intervalFailed);
            Interval _4800 = _9270;
            Interval _4801 = Interval{ _9293, _9294 };
            Interval _9318 = jet_mul_derivative(_4800, _4801, intervalFailed, optical_product_upper);
            Interval _4802 = _9318;
            Interval _4803 = _9268;
            Interval _4804 = _9300;
            Interval _9319 = jet_mul_derivative(_4803, _4804, intervalFailed, optical_product_upper);
            Interval _4805 = _9319;
            Interval _9320 = jet_add_derivative(_4802, _4805, intervalFailed);
            Interval _4778 = _9271;
            Interval _4779 = Interval{ _9293, _9294 };
            Interval _9322 = imul(_4778, _4779, intervalFailed, optical_product_upper);
            Interval _4780 = _9272;
            Interval _4781 = Interval{ _9293, _9294 };
            Interval _9324 = jet_mul_derivative(_4780, _4781, intervalFailed, optical_product_upper);
            Interval _4782 = _9324;
            Interval _4783 = _9271;
            Interval _4784 = _9297;
            Interval _9325 = jet_mul_derivative(_4783, _4784, intervalFailed, optical_product_upper);
            Interval _4785 = _9325;
            Interval _9326 = jet_add_derivative(_4782, _4785, intervalFailed);
            Interval _4786 = _9273;
            Interval _4787 = Interval{ _9293, _9294 };
            Interval _9328 = jet_mul_derivative(_4786, _4787, intervalFailed, optical_product_upper);
            Interval _4788 = _9328;
            Interval _4789 = _9271;
            Interval _4790 = _9300;
            Interval _9329 = jet_mul_derivative(_4789, _4790, intervalFailed, optical_product_upper);
            Interval _4791 = _9329;
            Interval _9330 = jet_add_derivative(_4788, _4791, intervalFailed);
            Interval _4772 = _8247;
            Interval _4773 = _9302;
            Interval _9331 = iadd(_4772, _4773, intervalFailed);
            Interval _4774 = _8250;
            Interval _4775 = _9306;
            Interval _9332 = jet_add_derivative(_4774, _4775, intervalFailed);
            Interval _4776 = _8253;
            Interval _4777 = _9310;
            Interval _9333 = jet_add_derivative(_4776, _4777, intervalFailed);
            Interval _4766 = _8256;
            Interval _4767 = _9312;
            Interval _9334 = iadd(_4766, _4767, intervalFailed);
            Interval _4768 = _8259;
            Interval _4769 = _9316;
            Interval _9335 = jet_add_derivative(_4768, _4769, intervalFailed);
            Interval _4770 = _8262;
            Interval _4771 = _9320;
            Interval _9336 = jet_add_derivative(_4770, _4771, intervalFailed);
            Interval _4760 = _8265;
            Interval _4761 = _9322;
            Interval _9337 = iadd(_4760, _4761, intervalFailed);
            Interval _4762 = _8268;
            Interval _4763 = _9326;
            Interval _9338 = jet_add_derivative(_4762, _4763, intervalFailed);
            Interval _4764 = _8271;
            Interval _4765 = _9330;
            Interval _9339 = jet_add_derivative(_4764, _4765, intervalFailed);
            bool _9346;
            if (_9331.lo <= 0.0)
            {
                _9346 = _9331.hi >= 0.0;
            }
            else
            {
                _9346 = false;
            }
            float _9353;
            if (_9346)
            {
                _9353 = 0.0;
            }
            else
            {
                _9353 = precise::min(abs(_9331.lo), abs(_9331.hi));
            }
            float _9356 = precise::max(abs(_9331.lo), abs(_9331.hi));
            float _4750 = spvFMul(_9353, _9353);
            float _9357 = interval_down(_4750, intervalFailed);
            float _4751 = spvFMul(_9356, _9356);
            float _9359 = interval_up(_4751, intervalFailed);
            Interval _4752 = Interval{ 2.0, 2.0 };
            Interval _4753 = _9331;
            Interval _9360 = imul(_4752, _4753, intervalFailed, optical_product_upper);
            Interval _4754 = _9360;
            Interval _4755 = _9332;
            Interval _9361 = jet_mul_derivative(_4754, _4755, intervalFailed, optical_product_upper);
            Interval _4756 = Interval{ 2.0, 2.0 };
            Interval _4757 = _9331;
            Interval _9362 = imul(_4756, _4757, intervalFailed, optical_product_upper);
            Interval _4758 = _9362;
            Interval _4759 = _9333;
            Interval _9363 = jet_mul_derivative(_4758, _4759, intervalFailed, optical_product_upper);
            bool _9370;
            if (_9334.lo <= 0.0)
            {
                _9370 = _9334.hi >= 0.0;
            }
            else
            {
                _9370 = false;
            }
            float _9377;
            if (_9370)
            {
                _9377 = 0.0;
            }
            else
            {
                _9377 = precise::min(abs(_9334.lo), abs(_9334.hi));
            }
            float _9380 = precise::max(abs(_9334.lo), abs(_9334.hi));
            float _4740 = spvFMul(_9377, _9377);
            float _9381 = interval_down(_4740, intervalFailed);
            float _4741 = spvFMul(_9380, _9380);
            float _9383 = interval_up(_4741, intervalFailed);
            Interval _4742 = Interval{ 2.0, 2.0 };
            Interval _4743 = _9334;
            Interval _9384 = imul(_4742, _4743, intervalFailed, optical_product_upper);
            Interval _4744 = _9384;
            Interval _4745 = _9335;
            Interval _9385 = jet_mul_derivative(_4744, _4745, intervalFailed, optical_product_upper);
            Interval _4746 = Interval{ 2.0, 2.0 };
            Interval _4747 = _9334;
            Interval _9386 = imul(_4746, _4747, intervalFailed, optical_product_upper);
            Interval _4748 = _9386;
            Interval _4749 = _9336;
            Interval _9387 = jet_mul_derivative(_4748, _4749, intervalFailed, optical_product_upper);
            Interval _4734 = Interval{ precise::max(0.0, _9357), _9359 };
            Interval _4735 = Interval{ precise::max(0.0, _9381), _9383 };
            Interval _9390 = iadd(_4734, _4735, intervalFailed);
            Interval _4736 = _9361;
            Interval _4737 = _9385;
            Interval _9391 = jet_add_derivative(_4736, _4737, intervalFailed);
            Interval _4738 = _9363;
            Interval _4739 = _9387;
            Interval _9392 = jet_add_derivative(_4738, _4739, intervalFailed);
            bool _9399;
            if (_9337.lo <= 0.0)
            {
                _9399 = _9337.hi >= 0.0;
            }
            else
            {
                _9399 = false;
            }
            float _9406;
            if (_9399)
            {
                _9406 = 0.0;
            }
            else
            {
                _9406 = precise::min(abs(_9337.lo), abs(_9337.hi));
            }
            float _9409 = precise::max(abs(_9337.lo), abs(_9337.hi));
            float _4724 = spvFMul(_9406, _9406);
            float _9410 = interval_down(_4724, intervalFailed);
            float _4725 = spvFMul(_9409, _9409);
            float _9412 = interval_up(_4725, intervalFailed);
            Interval _4726 = Interval{ 2.0, 2.0 };
            Interval _4727 = _9337;
            Interval _9413 = imul(_4726, _4727, intervalFailed, optical_product_upper);
            Interval _4728 = _9413;
            Interval _4729 = _9338;
            Interval _9414 = jet_mul_derivative(_4728, _4729, intervalFailed, optical_product_upper);
            Interval _4730 = Interval{ 2.0, 2.0 };
            Interval _4731 = _9337;
            Interval _9415 = imul(_4730, _4731, intervalFailed, optical_product_upper);
            Interval _4732 = _9415;
            Interval _4733 = _9339;
            Interval _9416 = jet_mul_derivative(_4732, _4733, intervalFailed, optical_product_upper);
            Interval _4718 = _9390;
            Interval _4719 = Interval{ precise::max(0.0, _9410), _9412 };
            Interval _9418 = iadd(_4718, _4719, intervalFailed);
            Interval _4720 = _9391;
            Interval _4721 = _9414;
            Interval _9419 = jet_add_derivative(_4720, _4721, intervalFailed);
            Interval _4722 = _9392;
            Interval _4723 = _9416;
            Interval _9420 = jet_add_derivative(_4722, _4723, intervalFailed);
            Interval _4709 = _9418;
            Interval _9422 = isqrt(_4709, intervalFailed);
            bool _9430;
            if (!intervalFailed)
            {
                _9430 = intervalFailed;
            }
            else
            {
                _9430 = false;
            }
            bool _9435;
            if (_9430)
            {
                _9435 = jetFailureSite == 0u;
            }
            else
            {
                _9435 = false;
            }
            if (_9435)
            {
                jetFailureSite = 3u;
                jetFailureArguments = float4(_9418.lo, _9418.hi, 0.0, 0.0);
            }
            if (_9422.lo <= 0.0)
            {
                jetBranchKnown = false;
            }
            Interval _4710 = Interval{ 2.0, 2.0 };
            Interval _4711 = _9422;
            Interval _9442 = imul(_4710, _4711, intervalFailed, optical_product_upper);
            Interval _4712 = Interval{ 1.0, 1.0 };
            Interval _4713 = _9442;
            Interval _9444 = idiv(_4712, _4713, intervalFailed, interval_divide_upper);
            bool _9451;
            if (!intervalFailed)
            {
                _9451 = intervalFailed;
            }
            else
            {
                _9451 = false;
            }
            bool _9456;
            if (_9451)
            {
                _9456 = jetFailureSite == 0u;
            }
            else
            {
                _9456 = false;
            }
            if (_9456)
            {
                jetFailureSite = 4u;
                jetFailureArguments = float4(1.0, 1.0, _9442.lo, _9442.hi);
            }
            Interval _4714 = _9444;
            Interval _4715 = _9419;
            Interval _9460 = jet_mul_derivative(_4714, _4715, intervalFailed, optical_product_upper);
            Interval _4716 = _9444;
            Interval _4717 = _9420;
            Interval _9461 = jet_mul_derivative(_4716, _4717, intervalFailed, optical_product_upper);
            Interval _4695 = Interval{ 1.0, 1.0 };
            Interval _4696 = _9422;
            Interval _9463 = idiv(_4695, _4696, intervalFailed, interval_divide_upper);
            bool _9470;
            if (!intervalFailed)
            {
                _9470 = intervalFailed;
            }
            else
            {
                _9470 = false;
            }
            bool _9475;
            if (_9470)
            {
                _9475 = jetFailureSite == 0u;
            }
            else
            {
                _9475 = false;
            }
            if (_9475)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(1.0, 1.0, _9422.lo, _9422.hi);
            }
            Interval _4697 = Interval{ 0.0, 0.0 };
            Interval _4698 = _9463;
            Interval _4699 = _9460;
            Interval _9479 = jet_mul_derivative(_4698, _4699, intervalFailed, optical_product_upper);
            Interval _4700 = Interval{ as_type<float>(as_type<uint>(_9479.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9479.lo) ^ 2147483648u) };
            Interval _9489 = jet_add_derivative(_4697, _4700, intervalFailed);
            Interval _4701 = _9489;
            Interval _4702 = _9422;
            Interval _9490 = jet_div_derivative(_4701, _4702, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4703 = Interval{ 0.0, 0.0 };
            Interval _4704 = _9463;
            Interval _4705 = _9461;
            Interval _9491 = jet_mul_derivative(_4704, _4705, intervalFailed, optical_product_upper);
            Interval _4706 = Interval{ as_type<float>(as_type<uint>(_9491.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9491.lo) ^ 2147483648u) };
            Interval _9501 = jet_add_derivative(_4703, _4706, intervalFailed);
            Interval _4707 = _9501;
            Interval _4708 = _9422;
            Interval _9502 = jet_div_derivative(_4707, _4708, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4681 = _9331;
            Interval _4682 = _9463;
            Interval _9503 = imul(_4681, _4682, intervalFailed, optical_product_upper);
            Interval _4683 = _9332;
            Interval _4684 = _9463;
            Interval _9504 = jet_mul_derivative(_4683, _4684, intervalFailed, optical_product_upper);
            Interval _4685 = _9504;
            Interval _4686 = _9331;
            Interval _4687 = _9490;
            Interval _9505 = jet_mul_derivative(_4686, _4687, intervalFailed, optical_product_upper);
            Interval _4688 = _9505;
            Interval _9506 = jet_add_derivative(_4685, _4688, intervalFailed);
            Interval _4689 = _9333;
            Interval _4690 = _9463;
            Interval _9507 = jet_mul_derivative(_4689, _4690, intervalFailed, optical_product_upper);
            Interval _4691 = _9507;
            Interval _4692 = _9331;
            Interval _4693 = _9502;
            Interval _9508 = jet_mul_derivative(_4692, _4693, intervalFailed, optical_product_upper);
            Interval _4694 = _9508;
            Interval _9509 = jet_add_derivative(_4691, _4694, intervalFailed);
            Interval _4667 = _9334;
            Interval _4668 = _9463;
            Interval _9510 = imul(_4667, _4668, intervalFailed, optical_product_upper);
            Interval _4669 = _9335;
            Interval _4670 = _9463;
            Interval _9511 = jet_mul_derivative(_4669, _4670, intervalFailed, optical_product_upper);
            Interval _4671 = _9511;
            Interval _4672 = _9334;
            Interval _4673 = _9490;
            Interval _9512 = jet_mul_derivative(_4672, _4673, intervalFailed, optical_product_upper);
            Interval _4674 = _9512;
            Interval _9513 = jet_add_derivative(_4671, _4674, intervalFailed);
            Interval _4675 = _9336;
            Interval _4676 = _9463;
            Interval _9514 = jet_mul_derivative(_4675, _4676, intervalFailed, optical_product_upper);
            Interval _4677 = _9514;
            Interval _4678 = _9334;
            Interval _4679 = _9502;
            Interval _9515 = jet_mul_derivative(_4678, _4679, intervalFailed, optical_product_upper);
            Interval _4680 = _9515;
            Interval _9516 = jet_add_derivative(_4677, _4680, intervalFailed);
            Interval _4653 = _9337;
            Interval _4654 = _9463;
            Interval _9517 = imul(_4653, _4654, intervalFailed, optical_product_upper);
            Interval _4655 = _9338;
            Interval _4656 = _9463;
            Interval _9518 = jet_mul_derivative(_4655, _4656, intervalFailed, optical_product_upper);
            Interval _4657 = _9518;
            Interval _4658 = _9337;
            Interval _4659 = _9490;
            Interval _9519 = jet_mul_derivative(_4658, _4659, intervalFailed, optical_product_upper);
            Interval _4660 = _9519;
            Interval _9520 = jet_add_derivative(_4657, _4660, intervalFailed);
            Interval _4661 = _9339;
            Interval _4662 = _9463;
            Interval _9521 = jet_mul_derivative(_4661, _4662, intervalFailed, optical_product_upper);
            Interval _4663 = _9521;
            Interval _4664 = _9337;
            Interval _4665 = _9502;
            Interval _9522 = jet_mul_derivative(_4664, _4665, intervalFailed, optical_product_upper);
            Interval _4666 = _9522;
            Interval _9523 = jet_add_derivative(_4663, _4666, intervalFailed);
            Interval _4639 = _9503;
            Interval _4640 = Interval{ _7923, _7924 };
            Interval _9525 = imul(_4639, _4640, intervalFailed, optical_product_upper);
            Interval _4641 = _9506;
            Interval _4642 = Interval{ _7923, _7924 };
            Interval _9527 = jet_mul_derivative(_4641, _4642, intervalFailed, optical_product_upper);
            Interval _4643 = _9527;
            Interval _4644 = _9503;
            Interval _4645 = Interval{ _7925, _7926 };
            Interval _9529 = jet_mul_derivative(_4644, _4645, intervalFailed, optical_product_upper);
            Interval _4646 = _9529;
            Interval _9530 = jet_add_derivative(_4643, _4646, intervalFailed);
            Interval _4647 = _9509;
            Interval _4648 = Interval{ _7923, _7924 };
            Interval _9532 = jet_mul_derivative(_4647, _4648, intervalFailed, optical_product_upper);
            Interval _4649 = _9532;
            Interval _4650 = _9503;
            Interval _4651 = Interval{ _7927, _7928 };
            Interval _9534 = jet_mul_derivative(_4650, _4651, intervalFailed, optical_product_upper);
            Interval _4652 = _9534;
            Interval _9535 = jet_add_derivative(_4649, _4652, intervalFailed);
            Interval _4625 = _9510;
            Interval _4626 = Interval{ _7929, _7930 };
            Interval _9537 = imul(_4625, _4626, intervalFailed, optical_product_upper);
            Interval _4627 = _9513;
            Interval _4628 = Interval{ _7929, _7930 };
            Interval _9539 = jet_mul_derivative(_4627, _4628, intervalFailed, optical_product_upper);
            Interval _4629 = _9539;
            Interval _4630 = _9510;
            Interval _4631 = Interval{ _7931, _7932 };
            Interval _9541 = jet_mul_derivative(_4630, _4631, intervalFailed, optical_product_upper);
            Interval _4632 = _9541;
            Interval _9542 = jet_add_derivative(_4629, _4632, intervalFailed);
            Interval _4633 = _9516;
            Interval _4634 = Interval{ _7929, _7930 };
            Interval _9544 = jet_mul_derivative(_4633, _4634, intervalFailed, optical_product_upper);
            Interval _4635 = _9544;
            Interval _4636 = _9510;
            Interval _4637 = Interval{ _7933, _7934 };
            Interval _9546 = jet_mul_derivative(_4636, _4637, intervalFailed, optical_product_upper);
            Interval _4638 = _9546;
            Interval _9547 = jet_add_derivative(_4635, _4638, intervalFailed);
            Interval _4619 = _9525;
            Interval _4620 = _9537;
            Interval _9548 = iadd(_4619, _4620, intervalFailed);
            Interval _4621 = _9530;
            Interval _4622 = _9542;
            Interval _9549 = jet_add_derivative(_4621, _4622, intervalFailed);
            Interval _4623 = _9535;
            Interval _4624 = _9547;
            Interval _9550 = jet_add_derivative(_4623, _4624, intervalFailed);
            Interval _4605 = _9517;
            Interval _4606 = Interval{ _7935, _7936 };
            Interval _9552 = imul(_4605, _4606, intervalFailed, optical_product_upper);
            Interval _4607 = _9520;
            Interval _4608 = Interval{ _7935, _7936 };
            Interval _9554 = jet_mul_derivative(_4607, _4608, intervalFailed, optical_product_upper);
            Interval _4609 = _9554;
            Interval _4610 = _9517;
            Interval _4611 = Interval{ _7937, _7938 };
            Interval _9556 = jet_mul_derivative(_4610, _4611, intervalFailed, optical_product_upper);
            Interval _4612 = _9556;
            Interval _9557 = jet_add_derivative(_4609, _4612, intervalFailed);
            Interval _4613 = _9523;
            Interval _4614 = Interval{ _7935, _7936 };
            Interval _9559 = jet_mul_derivative(_4613, _4614, intervalFailed, optical_product_upper);
            Interval _4615 = _9559;
            Interval _4616 = _9517;
            Interval _4617 = Interval{ _7939, _7940 };
            Interval _9561 = jet_mul_derivative(_4616, _4617, intervalFailed, optical_product_upper);
            Interval _4618 = _9561;
            Interval _9562 = jet_add_derivative(_4615, _4618, intervalFailed);
            Interval _4599 = _9548;
            Interval _4600 = _9552;
            Interval _9563 = iadd(_4599, _4600, intervalFailed);
            Interval _4601 = _9549;
            Interval _4602 = _9557;
            __attribute__((unused)) Interval _9564 = jet_add_derivative(_4601, _4602, intervalFailed);
            Interval _4603 = _9550;
            Interval _4604 = _9562;
            __attribute__((unused)) Interval _9565 = jet_add_derivative(_4603, _4604, intervalFailed);
            float _9588;
            float _9589;
            float _9590;
            float _9591;
            float _9592;
            float _9593;
            float _9594;
            float _9595;
            float _9596;
            float _9597;
            float _9598;
            float _9599;
            float _9600;
            float _9601;
            float _9602;
            float _9603;
            float _9604;
            float _9605;
            if (_9563.lo > 0.0)
            {
                _9588 = _9503.lo;
                _9589 = _9503.hi;
                _9590 = _9506.lo;
                _9591 = _9506.hi;
                _9592 = _9509.lo;
                _9593 = _9509.hi;
                _9594 = _9510.lo;
                _9595 = _9510.hi;
                _9596 = _9513.lo;
                _9597 = _9513.hi;
                _9598 = _9516.lo;
                _9599 = _9516.hi;
                _9600 = _9517.lo;
                _9601 = _9517.hi;
                _9602 = _9520.lo;
                _9603 = _9520.hi;
                _9604 = _9523.lo;
                _9605 = _9523.hi;
            }
            else
            {
                if (_9563.hi > 0.0)
                {
                    return false;
                }
                _9588 = _8247.lo;
                _9589 = _8247.hi;
                _9590 = _8250.lo;
                _9591 = _8250.hi;
                _9592 = _8253.lo;
                _9593 = _8253.hi;
                _9594 = _8256.lo;
                _9595 = _8256.hi;
                _9596 = _8259.lo;
                _9597 = _8259.hi;
                _9598 = _8262.lo;
                _9599 = _8262.hi;
                _9600 = _8265.lo;
                _9601 = _8265.hi;
                _9602 = _8268.lo;
                _9603 = _8268.hi;
                _9604 = _8271.lo;
                _9605 = _8271.hi;
            }
            _6764 = _9588;
            _6766 = _9589;
            _6768 = _9590;
            _6770 = _9591;
            _6772 = _9592;
            _6774 = _9593;
            _6776 = _9594;
            _6778 = _9595;
            _6780 = _9596;
            _6782 = _9597;
            _6784 = _9598;
            _6786 = _9599;
            _6788 = _9600;
            _6790 = _9601;
            _6792 = _9602;
            _6794 = _9603;
            _6796 = _9604;
            _6798 = _9605;
        }
        else
        {
            _6764 = _8247.lo;
            _6766 = _8247.hi;
            _6768 = _8250.lo;
            _6770 = _8250.hi;
            _6772 = _8253.lo;
            _6774 = _8253.hi;
            _6776 = _8256.lo;
            _6778 = _8256.hi;
            _6780 = _8259.lo;
            _6782 = _8259.hi;
            _6784 = _8262.lo;
            _6786 = _8262.hi;
            _6788 = _8265.lo;
            _6790 = _8265.hi;
            _6792 = _8268.lo;
            _6794 = _8268.hi;
            _6796 = _8271.lo;
            _6798 = _8271.hi;
        }
        bool _9610;
        if (!intervalFailed)
        {
            _9610 = !jetBranchKnown;
        }
        else
        {
            _9610 = true;
        }
        if (_9610)
        {
            return false;
        }
    }
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ _6799, _6801 }, Interval{ _6803, _6805 }, Interval{ _6807, _6809 } }, OpticalJet{ Interval{ _6811, _6813 }, Interval{ _6815, _6817 }, Interval{ _6819, _6821 } }, OpticalJet{ Interval{ _6823, _6825 }, Interval{ _6827, _6829 }, Interval{ _6831, _6833 } } };
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ _6763, _6765 }, Interval{ _6767, _6769 }, Interval{ _6771, _6773 } }, OpticalJet{ Interval{ _6775, _6777 }, Interval{ _6779, _6781 }, Interval{ _6783, _6785 } }, OpticalJet{ Interval{ _6787, _6789 }, Interval{ _6791, _6793 }, Interval{ _6795, _6797 } } };
    ray.bias0 = OpticalJet{ Interval{ _6751, _6753 }, Interval{ _6755, _6757 }, Interval{ _6759, _6761 } };
    bool _9647;
    if (!intervalFailed)
    {
        _9647 = jetBranchKnown;
    }
    else
    {
        _9647 = false;
    }
    return _9647;
}

static inline __attribute__((always_inline))
bool optical_jet_residual(thread const OpticalJetRay& ray, thread const Interval3& target, thread const bool& finiteTerminal, thread const float& focal, thread const uint& omitted, thread OpticalJet& e0, thread OpticalJet& e1, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    e1 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    e0 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    if (omitted > 2u)
    {
        return false;
    }
    float _10157;
    float _10158;
    float _10159;
    float _10160;
    float _10161;
    float _10162;
    float _10163;
    float _10164;
    float _10165;
    float _10166;
    float _10167;
    float _10168;
    float _10169;
    float _10170;
    float _10171;
    float _10172;
    float _10173;
    float _10174;
    if (finiteTerminal)
    {
        Interval _10016 = target.x;
        Interval _10017 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.v.lo) ^ 2147483648u) };
        Interval _10122 = iadd(_10016, _10017, intervalFailed);
        Interval _10018 = Interval{ 0.0, 0.0 };
        Interval _10019 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dx.lo) ^ 2147483648u) };
        Interval _10124 = jet_add_derivative(_10018, _10019, intervalFailed);
        Interval _10020 = Interval{ 0.0, 0.0 };
        Interval _10021 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dy.lo) ^ 2147483648u) };
        Interval _10126 = jet_add_derivative(_10020, _10021, intervalFailed);
        Interval _10010 = target.y;
        Interval _10011 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.v.lo) ^ 2147483648u) };
        Interval _10128 = iadd(_10010, _10011, intervalFailed);
        Interval _10012 = Interval{ 0.0, 0.0 };
        Interval _10013 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dx.lo) ^ 2147483648u) };
        Interval _10130 = jet_add_derivative(_10012, _10013, intervalFailed);
        Interval _10014 = Interval{ 0.0, 0.0 };
        Interval _10015 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dy.lo) ^ 2147483648u) };
        Interval _10132 = jet_add_derivative(_10014, _10015, intervalFailed);
        Interval _10004 = target.z;
        Interval _10005 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.v.lo) ^ 2147483648u) };
        Interval _10134 = iadd(_10004, _10005, intervalFailed);
        Interval _10006 = Interval{ 0.0, 0.0 };
        Interval _10007 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dx.lo) ^ 2147483648u) };
        Interval _10136 = jet_add_derivative(_10006, _10007, intervalFailed);
        Interval _10008 = Interval{ 0.0, 0.0 };
        Interval _10009 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dy.lo) ^ 2147483648u) };
        Interval _10138 = jet_add_derivative(_10008, _10009, intervalFailed);
        _10157 = _10122.lo;
        _10158 = _10122.hi;
        _10159 = _10124.lo;
        _10160 = _10124.hi;
        _10161 = _10126.lo;
        _10162 = _10126.hi;
        _10163 = _10128.lo;
        _10164 = _10128.hi;
        _10165 = _10130.lo;
        _10166 = _10130.hi;
        _10167 = _10132.lo;
        _10168 = _10132.hi;
        _10169 = _10134.lo;
        _10170 = _10134.hi;
        _10171 = _10136.lo;
        _10172 = _10136.hi;
        _10173 = _10138.lo;
        _10174 = _10138.hi;
    }
    else
    {
        _10157 = target.x.lo;
        _10158 = target.x.hi;
        _10159 = 0.0;
        _10160 = 0.0;
        _10161 = 0.0;
        _10162 = 0.0;
        _10163 = target.y.lo;
        _10164 = target.y.hi;
        _10165 = 0.0;
        _10166 = 0.0;
        _10167 = 0.0;
        _10168 = 0.0;
        _10169 = target.z.lo;
        _10170 = target.z.hi;
        _10171 = 0.0;
        _10172 = 0.0;
        _10173 = 0.0;
        _10174 = 0.0;
    }
    bool _10179;
    if (_10157 <= 0.0)
    {
        _10179 = _10158 >= 0.0;
    }
    else
    {
        _10179 = false;
    }
    float _10186;
    if (_10179)
    {
        _10186 = 0.0;
    }
    else
    {
        _10186 = precise::min(abs(_10157), abs(_10158));
    }
    float _10189 = precise::max(abs(_10157), abs(_10158));
    float _9994 = spvFMul(_10186, _10186);
    float _10190 = interval_down(_9994, intervalFailed);
    float _9995 = spvFMul(_10189, _10189);
    float _10192 = interval_up(_9995, intervalFailed);
    Interval _9996 = Interval{ 2.0, 2.0 };
    Interval _9997 = Interval{ _10157, _10158 };
    Interval _10194 = imul(_9996, _9997, intervalFailed, optical_product_upper);
    Interval _9998 = _10194;
    Interval _9999 = Interval{ _10159, _10160 };
    Interval _10196 = jet_mul_derivative(_9998, _9999, intervalFailed, optical_product_upper);
    Interval _10000 = Interval{ 2.0, 2.0 };
    Interval _10001 = Interval{ _10157, _10158 };
    Interval _10198 = imul(_10000, _10001, intervalFailed, optical_product_upper);
    Interval _10002 = _10198;
    Interval _10003 = Interval{ _10161, _10162 };
    Interval _10200 = jet_mul_derivative(_10002, _10003, intervalFailed, optical_product_upper);
    bool _10205;
    if (_10163 <= 0.0)
    {
        _10205 = _10164 >= 0.0;
    }
    else
    {
        _10205 = false;
    }
    float _10212;
    if (_10205)
    {
        _10212 = 0.0;
    }
    else
    {
        _10212 = precise::min(abs(_10163), abs(_10164));
    }
    float _10215 = precise::max(abs(_10163), abs(_10164));
    float _9984 = spvFMul(_10212, _10212);
    float _10216 = interval_down(_9984, intervalFailed);
    float _9985 = spvFMul(_10215, _10215);
    float _10218 = interval_up(_9985, intervalFailed);
    Interval _9986 = Interval{ 2.0, 2.0 };
    Interval _9987 = Interval{ _10163, _10164 };
    Interval _10220 = imul(_9986, _9987, intervalFailed, optical_product_upper);
    Interval _9988 = _10220;
    Interval _9989 = Interval{ _10165, _10166 };
    Interval _10222 = jet_mul_derivative(_9988, _9989, intervalFailed, optical_product_upper);
    Interval _9990 = Interval{ 2.0, 2.0 };
    Interval _9991 = Interval{ _10163, _10164 };
    Interval _10224 = imul(_9990, _9991, intervalFailed, optical_product_upper);
    Interval _9992 = _10224;
    Interval _9993 = Interval{ _10167, _10168 };
    Interval _10226 = jet_mul_derivative(_9992, _9993, intervalFailed, optical_product_upper);
    Interval _9978 = Interval{ precise::max(0.0, _10190), _10192 };
    Interval _9979 = Interval{ precise::max(0.0, _10216), _10218 };
    Interval _10229 = iadd(_9978, _9979, intervalFailed);
    Interval _9980 = _10196;
    Interval _9981 = _10222;
    Interval _10230 = jet_add_derivative(_9980, _9981, intervalFailed);
    Interval _9982 = _10200;
    Interval _9983 = _10226;
    Interval _10231 = jet_add_derivative(_9982, _9983, intervalFailed);
    bool _10236;
    if (_10169 <= 0.0)
    {
        _10236 = _10170 >= 0.0;
    }
    else
    {
        _10236 = false;
    }
    float _10243;
    if (_10236)
    {
        _10243 = 0.0;
    }
    else
    {
        _10243 = precise::min(abs(_10169), abs(_10170));
    }
    float _10246 = precise::max(abs(_10169), abs(_10170));
    float _9968 = spvFMul(_10243, _10243);
    float _10247 = interval_down(_9968, intervalFailed);
    float _9969 = spvFMul(_10246, _10246);
    float _10249 = interval_up(_9969, intervalFailed);
    Interval _9970 = Interval{ 2.0, 2.0 };
    Interval _9971 = Interval{ _10169, _10170 };
    Interval _10251 = imul(_9970, _9971, intervalFailed, optical_product_upper);
    Interval _9972 = _10251;
    Interval _9973 = Interval{ _10171, _10172 };
    Interval _10253 = jet_mul_derivative(_9972, _9973, intervalFailed, optical_product_upper);
    Interval _9974 = Interval{ 2.0, 2.0 };
    Interval _9975 = Interval{ _10169, _10170 };
    Interval _10255 = imul(_9974, _9975, intervalFailed, optical_product_upper);
    Interval _9976 = _10255;
    Interval _9977 = Interval{ _10173, _10174 };
    Interval _10257 = jet_mul_derivative(_9976, _9977, intervalFailed, optical_product_upper);
    Interval _9962 = _10229;
    Interval _9963 = Interval{ precise::max(0.0, _10247), _10249 };
    Interval _10259 = iadd(_9962, _9963, intervalFailed);
    Interval _9964 = _10230;
    Interval _9965 = _10253;
    Interval _10260 = jet_add_derivative(_9964, _9965, intervalFailed);
    Interval _9966 = _10231;
    Interval _9967 = _10257;
    Interval _10261 = jet_add_derivative(_9966, _9967, intervalFailed);
    Interval _9953 = _10259;
    Interval _10263 = isqrt(_9953, intervalFailed);
    bool _10271;
    if (!intervalFailed)
    {
        _10271 = intervalFailed;
    }
    else
    {
        _10271 = false;
    }
    bool _10276;
    if (_10271)
    {
        _10276 = jetFailureSite == 0u;
    }
    else
    {
        _10276 = false;
    }
    if (_10276)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10259.lo, _10259.hi, 0.0, 0.0);
    }
    if (_10263.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9954 = Interval{ 2.0, 2.0 };
    Interval _9955 = _10263;
    Interval _10283 = imul(_9954, _9955, intervalFailed, optical_product_upper);
    Interval _9956 = Interval{ 1.0, 1.0 };
    Interval _9957 = _10283;
    Interval _10285 = idiv(_9956, _9957, intervalFailed, interval_divide_upper);
    bool _10292;
    if (!intervalFailed)
    {
        _10292 = intervalFailed;
    }
    else
    {
        _10292 = false;
    }
    bool _10297;
    if (_10292)
    {
        _10297 = jetFailureSite == 0u;
    }
    else
    {
        _10297 = false;
    }
    if (_10297)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10283.lo, _10283.hi);
    }
    Interval _9958 = _10285;
    Interval _9959 = _10260;
    __attribute__((unused)) Interval _10301 = jet_mul_derivative(_9958, _9959, intervalFailed, optical_product_upper);
    Interval _9960 = _10285;
    Interval _9961 = _10261;
    __attribute__((unused)) Interval _10302 = jet_mul_derivative(_9960, _9961, intervalFailed, optical_product_upper);
    float _10307;
    if (finiteTerminal)
    {
        _10307 = ray.bias0.v.hi;
    }
    else
    {
        _10307 = 0.0;
    }
    bool _10367;
    if ((isunordered(_10263.lo, _10307) || _10263.lo > _10307))
    {
        Interval _9939 = Interval{ _10157, _10158 };
        Interval _9940 = ray.outgoing.x.v;
        Interval _10318 = imul(_9939, _9940, intervalFailed, optical_product_upper);
        Interval _9941 = Interval{ _10159, _10160 };
        Interval _9942 = ray.outgoing.x.v;
        Interval _10320 = jet_mul_derivative(_9941, _9942, intervalFailed, optical_product_upper);
        Interval _9943 = _10320;
        Interval _9944 = Interval{ _10157, _10158 };
        Interval _9945 = ray.outgoing.x.dx;
        Interval _10322 = jet_mul_derivative(_9944, _9945, intervalFailed, optical_product_upper);
        Interval _9946 = _10322;
        Interval _10323 = jet_add_derivative(_9943, _9946, intervalFailed);
        Interval _9947 = Interval{ _10161, _10162 };
        Interval _9948 = ray.outgoing.x.v;
        Interval _10325 = jet_mul_derivative(_9947, _9948, intervalFailed, optical_product_upper);
        Interval _9949 = _10325;
        Interval _9950 = Interval{ _10157, _10158 };
        Interval _9951 = ray.outgoing.x.dy;
        Interval _10327 = jet_mul_derivative(_9950, _9951, intervalFailed, optical_product_upper);
        Interval _9952 = _10327;
        Interval _10328 = jet_add_derivative(_9949, _9952, intervalFailed);
        Interval _9925 = Interval{ _10163, _10164 };
        Interval _9926 = ray.outgoing.y.v;
        Interval _10333 = imul(_9925, _9926, intervalFailed, optical_product_upper);
        Interval _9927 = Interval{ _10165, _10166 };
        Interval _9928 = ray.outgoing.y.v;
        Interval _10335 = jet_mul_derivative(_9927, _9928, intervalFailed, optical_product_upper);
        Interval _9929 = _10335;
        Interval _9930 = Interval{ _10163, _10164 };
        Interval _9931 = ray.outgoing.y.dx;
        Interval _10337 = jet_mul_derivative(_9930, _9931, intervalFailed, optical_product_upper);
        Interval _9932 = _10337;
        Interval _10338 = jet_add_derivative(_9929, _9932, intervalFailed);
        Interval _9933 = Interval{ _10167, _10168 };
        Interval _9934 = ray.outgoing.y.v;
        Interval _10340 = jet_mul_derivative(_9933, _9934, intervalFailed, optical_product_upper);
        Interval _9935 = _10340;
        Interval _9936 = Interval{ _10163, _10164 };
        Interval _9937 = ray.outgoing.y.dy;
        Interval _10342 = jet_mul_derivative(_9936, _9937, intervalFailed, optical_product_upper);
        Interval _9938 = _10342;
        Interval _10343 = jet_add_derivative(_9935, _9938, intervalFailed);
        Interval _9919 = _10318;
        Interval _9920 = _10333;
        Interval _10344 = iadd(_9919, _9920, intervalFailed);
        Interval _9921 = _10323;
        Interval _9922 = _10338;
        Interval _10345 = jet_add_derivative(_9921, _9922, intervalFailed);
        Interval _9923 = _10328;
        Interval _9924 = _10343;
        Interval _10346 = jet_add_derivative(_9923, _9924, intervalFailed);
        Interval _9905 = Interval{ _10169, _10170 };
        Interval _9906 = ray.outgoing.z.v;
        Interval _10351 = imul(_9905, _9906, intervalFailed, optical_product_upper);
        Interval _9907 = Interval{ _10171, _10172 };
        Interval _9908 = ray.outgoing.z.v;
        Interval _10353 = jet_mul_derivative(_9907, _9908, intervalFailed, optical_product_upper);
        Interval _9909 = _10353;
        Interval _9910 = Interval{ _10169, _10170 };
        Interval _9911 = ray.outgoing.z.dx;
        Interval _10355 = jet_mul_derivative(_9910, _9911, intervalFailed, optical_product_upper);
        Interval _9912 = _10355;
        Interval _10356 = jet_add_derivative(_9909, _9912, intervalFailed);
        Interval _9913 = Interval{ _10173, _10174 };
        Interval _9914 = ray.outgoing.z.v;
        Interval _10358 = jet_mul_derivative(_9913, _9914, intervalFailed, optical_product_upper);
        Interval _9915 = _10358;
        Interval _9916 = Interval{ _10169, _10170 };
        Interval _9917 = ray.outgoing.z.dy;
        Interval _10360 = jet_mul_derivative(_9916, _9917, intervalFailed, optical_product_upper);
        Interval _9918 = _10360;
        Interval _10361 = jet_add_derivative(_9915, _9918, intervalFailed);
        Interval _9899 = _10344;
        Interval _9900 = _10351;
        Interval _10362 = iadd(_9899, _9900, intervalFailed);
        Interval _9901 = _10345;
        Interval _9902 = _10356;
        __attribute__((unused)) Interval _10363 = jet_add_derivative(_9901, _9902, intervalFailed);
        Interval _9903 = _10346;
        Interval _9904 = _10361;
        __attribute__((unused)) Interval _10364 = jet_add_derivative(_9903, _9904, intervalFailed);
        _10367 = _10362.lo <= 0.0;
    }
    else
    {
        _10367 = true;
    }
    if (_10367)
    {
        return false;
    }
    float _10386;
    float _10387;
    if (omitted == 0u)
    {
        _10386 = ray.outgoing.x.v.lo;
        _10387 = ray.outgoing.x.v.hi;
    }
    else
    {
        float _10380;
        float _10381;
        if (omitted == 1u)
        {
            _10380 = ray.outgoing.y.v.lo;
            _10381 = ray.outgoing.y.v.hi;
        }
        else
        {
            _10380 = ray.outgoing.z.v.lo;
            _10381 = ray.outgoing.z.v.hi;
        }
        _10386 = _10380;
        _10387 = _10381;
    }
    bool _10392;
    if (_10386 <= 0.0)
    {
        _10392 = _10387 >= 0.0;
    }
    else
    {
        _10392 = false;
    }
    if (_10392)
    {
        return false;
    }
    bool _10397;
    if (_10157 <= 0.0)
    {
        _10397 = _10158 >= 0.0;
    }
    else
    {
        _10397 = false;
    }
    float _10404;
    if (_10397)
    {
        _10404 = 0.0;
    }
    else
    {
        _10404 = precise::min(abs(_10157), abs(_10158));
    }
    float _10407 = precise::max(abs(_10157), abs(_10158));
    float _9889 = spvFMul(_10404, _10404);
    float _10408 = interval_down(_9889, intervalFailed);
    float _9890 = spvFMul(_10407, _10407);
    float _10410 = interval_up(_9890, intervalFailed);
    Interval _9891 = Interval{ 2.0, 2.0 };
    Interval _9892 = Interval{ _10157, _10158 };
    Interval _10412 = imul(_9891, _9892, intervalFailed, optical_product_upper);
    Interval _9893 = _10412;
    Interval _9894 = Interval{ _10159, _10160 };
    Interval _10414 = jet_mul_derivative(_9893, _9894, intervalFailed, optical_product_upper);
    Interval _9895 = Interval{ 2.0, 2.0 };
    Interval _9896 = Interval{ _10157, _10158 };
    Interval _10416 = imul(_9895, _9896, intervalFailed, optical_product_upper);
    Interval _9897 = _10416;
    Interval _9898 = Interval{ _10161, _10162 };
    Interval _10418 = jet_mul_derivative(_9897, _9898, intervalFailed, optical_product_upper);
    bool _10423;
    if (_10163 <= 0.0)
    {
        _10423 = _10164 >= 0.0;
    }
    else
    {
        _10423 = false;
    }
    float _10430;
    if (_10423)
    {
        _10430 = 0.0;
    }
    else
    {
        _10430 = precise::min(abs(_10163), abs(_10164));
    }
    float _10433 = precise::max(abs(_10163), abs(_10164));
    float _9879 = spvFMul(_10430, _10430);
    float _10434 = interval_down(_9879, intervalFailed);
    float _9880 = spvFMul(_10433, _10433);
    float _10436 = interval_up(_9880, intervalFailed);
    Interval _9881 = Interval{ 2.0, 2.0 };
    Interval _9882 = Interval{ _10163, _10164 };
    Interval _10438 = imul(_9881, _9882, intervalFailed, optical_product_upper);
    Interval _9883 = _10438;
    Interval _9884 = Interval{ _10165, _10166 };
    Interval _10440 = jet_mul_derivative(_9883, _9884, intervalFailed, optical_product_upper);
    Interval _9885 = Interval{ 2.0, 2.0 };
    Interval _9886 = Interval{ _10163, _10164 };
    Interval _10442 = imul(_9885, _9886, intervalFailed, optical_product_upper);
    Interval _9887 = _10442;
    Interval _9888 = Interval{ _10167, _10168 };
    Interval _10444 = jet_mul_derivative(_9887, _9888, intervalFailed, optical_product_upper);
    Interval _9873 = Interval{ precise::max(0.0, _10408), _10410 };
    Interval _9874 = Interval{ precise::max(0.0, _10434), _10436 };
    Interval _10447 = iadd(_9873, _9874, intervalFailed);
    Interval _9875 = _10414;
    Interval _9876 = _10440;
    Interval _10448 = jet_add_derivative(_9875, _9876, intervalFailed);
    Interval _9877 = _10418;
    Interval _9878 = _10444;
    Interval _10449 = jet_add_derivative(_9877, _9878, intervalFailed);
    bool _10454;
    if (_10169 <= 0.0)
    {
        _10454 = _10170 >= 0.0;
    }
    else
    {
        _10454 = false;
    }
    float _10461;
    if (_10454)
    {
        _10461 = 0.0;
    }
    else
    {
        _10461 = precise::min(abs(_10169), abs(_10170));
    }
    float _10464 = precise::max(abs(_10169), abs(_10170));
    float _9863 = spvFMul(_10461, _10461);
    float _10465 = interval_down(_9863, intervalFailed);
    float _9864 = spvFMul(_10464, _10464);
    float _10467 = interval_up(_9864, intervalFailed);
    Interval _9865 = Interval{ 2.0, 2.0 };
    Interval _9866 = Interval{ _10169, _10170 };
    Interval _10469 = imul(_9865, _9866, intervalFailed, optical_product_upper);
    Interval _9867 = _10469;
    Interval _9868 = Interval{ _10171, _10172 };
    Interval _10471 = jet_mul_derivative(_9867, _9868, intervalFailed, optical_product_upper);
    Interval _9869 = Interval{ 2.0, 2.0 };
    Interval _9870 = Interval{ _10169, _10170 };
    Interval _10473 = imul(_9869, _9870, intervalFailed, optical_product_upper);
    Interval _9871 = _10473;
    Interval _9872 = Interval{ _10173, _10174 };
    Interval _10475 = jet_mul_derivative(_9871, _9872, intervalFailed, optical_product_upper);
    Interval _9857 = _10447;
    Interval _9858 = Interval{ precise::max(0.0, _10465), _10467 };
    Interval _10477 = iadd(_9857, _9858, intervalFailed);
    Interval _9859 = _10448;
    Interval _9860 = _10471;
    Interval _10478 = jet_add_derivative(_9859, _9860, intervalFailed);
    Interval _9861 = _10449;
    Interval _9862 = _10475;
    Interval _10479 = jet_add_derivative(_9861, _9862, intervalFailed);
    Interval _9848 = _10477;
    Interval _10481 = isqrt(_9848, intervalFailed);
    bool _10489;
    if (!intervalFailed)
    {
        _10489 = intervalFailed;
    }
    else
    {
        _10489 = false;
    }
    bool _10494;
    if (_10489)
    {
        _10494 = jetFailureSite == 0u;
    }
    else
    {
        _10494 = false;
    }
    if (_10494)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10477.lo, _10477.hi, 0.0, 0.0);
    }
    if (_10481.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9849 = Interval{ 2.0, 2.0 };
    Interval _9850 = _10481;
    Interval _10501 = imul(_9849, _9850, intervalFailed, optical_product_upper);
    Interval _9851 = Interval{ 1.0, 1.0 };
    Interval _9852 = _10501;
    Interval _10503 = idiv(_9851, _9852, intervalFailed, interval_divide_upper);
    bool _10510;
    if (!intervalFailed)
    {
        _10510 = intervalFailed;
    }
    else
    {
        _10510 = false;
    }
    bool _10515;
    if (_10510)
    {
        _10515 = jetFailureSite == 0u;
    }
    else
    {
        _10515 = false;
    }
    if (_10515)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10501.lo, _10501.hi);
    }
    Interval _9853 = _10503;
    Interval _9854 = _10478;
    Interval _10519 = jet_mul_derivative(_9853, _9854, intervalFailed, optical_product_upper);
    Interval _9855 = _10503;
    Interval _9856 = _10479;
    Interval _10520 = jet_mul_derivative(_9855, _9856, intervalFailed, optical_product_upper);
    Interval _9834 = Interval{ 1.0, 1.0 };
    Interval _9835 = _10481;
    Interval _10522 = idiv(_9834, _9835, intervalFailed, interval_divide_upper);
    bool _10529;
    if (!intervalFailed)
    {
        _10529 = intervalFailed;
    }
    else
    {
        _10529 = false;
    }
    bool _10534;
    if (_10529)
    {
        _10534 = jetFailureSite == 0u;
    }
    else
    {
        _10534 = false;
    }
    if (_10534)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _10481.lo, _10481.hi);
    }
    Interval _9836 = Interval{ 0.0, 0.0 };
    Interval _9837 = _10522;
    Interval _9838 = _10519;
    Interval _10538 = jet_mul_derivative(_9837, _9838, intervalFailed, optical_product_upper);
    Interval _9839 = Interval{ as_type<float>(as_type<uint>(_10538.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10538.lo) ^ 2147483648u) };
    Interval _10548 = jet_add_derivative(_9836, _9839, intervalFailed);
    Interval _9840 = _10548;
    Interval _9841 = _10481;
    Interval _10549 = jet_div_derivative(_9840, _9841, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9842 = Interval{ 0.0, 0.0 };
    Interval _9843 = _10522;
    Interval _9844 = _10520;
    Interval _10550 = jet_mul_derivative(_9843, _9844, intervalFailed, optical_product_upper);
    Interval _9845 = Interval{ as_type<float>(as_type<uint>(_10550.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10550.lo) ^ 2147483648u) };
    Interval _10560 = jet_add_derivative(_9842, _9845, intervalFailed);
    Interval _9846 = _10560;
    Interval _9847 = _10481;
    Interval _10561 = jet_div_derivative(_9846, _9847, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9820 = Interval{ _10157, _10158 };
    Interval _9821 = _10522;
    Interval _10563 = imul(_9820, _9821, intervalFailed, optical_product_upper);
    Interval _9822 = Interval{ _10159, _10160 };
    Interval _9823 = _10522;
    Interval _10565 = jet_mul_derivative(_9822, _9823, intervalFailed, optical_product_upper);
    Interval _9824 = _10565;
    Interval _9825 = Interval{ _10157, _10158 };
    Interval _9826 = _10549;
    Interval _10567 = jet_mul_derivative(_9825, _9826, intervalFailed, optical_product_upper);
    Interval _9827 = _10567;
    Interval _10568 = jet_add_derivative(_9824, _9827, intervalFailed);
    Interval _9828 = Interval{ _10161, _10162 };
    Interval _9829 = _10522;
    Interval _10570 = jet_mul_derivative(_9828, _9829, intervalFailed, optical_product_upper);
    Interval _9830 = _10570;
    Interval _9831 = Interval{ _10157, _10158 };
    Interval _9832 = _10561;
    Interval _10572 = jet_mul_derivative(_9831, _9832, intervalFailed, optical_product_upper);
    Interval _9833 = _10572;
    Interval _10573 = jet_add_derivative(_9830, _9833, intervalFailed);
    Interval _9806 = Interval{ _10163, _10164 };
    Interval _9807 = _10522;
    Interval _10575 = imul(_9806, _9807, intervalFailed, optical_product_upper);
    Interval _9808 = Interval{ _10165, _10166 };
    Interval _9809 = _10522;
    Interval _10577 = jet_mul_derivative(_9808, _9809, intervalFailed, optical_product_upper);
    Interval _9810 = _10577;
    Interval _9811 = Interval{ _10163, _10164 };
    Interval _9812 = _10549;
    Interval _10579 = jet_mul_derivative(_9811, _9812, intervalFailed, optical_product_upper);
    Interval _9813 = _10579;
    Interval _10580 = jet_add_derivative(_9810, _9813, intervalFailed);
    Interval _9814 = Interval{ _10167, _10168 };
    Interval _9815 = _10522;
    Interval _10582 = jet_mul_derivative(_9814, _9815, intervalFailed, optical_product_upper);
    Interval _9816 = _10582;
    Interval _9817 = Interval{ _10163, _10164 };
    Interval _9818 = _10561;
    Interval _10584 = jet_mul_derivative(_9817, _9818, intervalFailed, optical_product_upper);
    Interval _9819 = _10584;
    Interval _10585 = jet_add_derivative(_9816, _9819, intervalFailed);
    Interval _9792 = Interval{ _10169, _10170 };
    Interval _9793 = _10522;
    Interval _10587 = imul(_9792, _9793, intervalFailed, optical_product_upper);
    Interval _9794 = Interval{ _10171, _10172 };
    Interval _9795 = _10522;
    Interval _10589 = jet_mul_derivative(_9794, _9795, intervalFailed, optical_product_upper);
    Interval _9796 = _10589;
    Interval _9797 = Interval{ _10169, _10170 };
    Interval _9798 = _10549;
    Interval _10591 = jet_mul_derivative(_9797, _9798, intervalFailed, optical_product_upper);
    Interval _9799 = _10591;
    Interval _10592 = jet_add_derivative(_9796, _9799, intervalFailed);
    Interval _9800 = Interval{ _10173, _10174 };
    Interval _9801 = _10522;
    Interval _10594 = jet_mul_derivative(_9800, _9801, intervalFailed, optical_product_upper);
    Interval _9802 = _10594;
    Interval _9803 = Interval{ _10169, _10170 };
    Interval _9804 = _10561;
    Interval _10596 = jet_mul_derivative(_9803, _9804, intervalFailed, optical_product_upper);
    Interval _9805 = _10596;
    Interval _10597 = jet_add_derivative(_9802, _9805, intervalFailed);
    Interval _9778 = _10575;
    Interval _9779 = ray.outgoing.z.v;
    Interval _10606 = imul(_9778, _9779, intervalFailed, optical_product_upper);
    Interval _9780 = _10580;
    Interval _9781 = ray.outgoing.z.v;
    Interval _10607 = jet_mul_derivative(_9780, _9781, intervalFailed, optical_product_upper);
    Interval _9782 = _10607;
    Interval _9783 = _10575;
    Interval _9784 = ray.outgoing.z.dx;
    Interval _10608 = jet_mul_derivative(_9783, _9784, intervalFailed, optical_product_upper);
    Interval _9785 = _10608;
    Interval _10609 = jet_add_derivative(_9782, _9785, intervalFailed);
    Interval _9786 = _10585;
    Interval _9787 = ray.outgoing.z.v;
    Interval _10610 = jet_mul_derivative(_9786, _9787, intervalFailed, optical_product_upper);
    Interval _9788 = _10610;
    Interval _9789 = _10575;
    Interval _9790 = ray.outgoing.z.dy;
    Interval _10611 = jet_mul_derivative(_9789, _9790, intervalFailed, optical_product_upper);
    Interval _9791 = _10611;
    Interval _10612 = jet_add_derivative(_9788, _9791, intervalFailed);
    Interval _9764 = _10587;
    Interval _9765 = ray.outgoing.y.v;
    Interval _10616 = imul(_9764, _9765, intervalFailed, optical_product_upper);
    Interval _9766 = _10592;
    Interval _9767 = ray.outgoing.y.v;
    Interval _10617 = jet_mul_derivative(_9766, _9767, intervalFailed, optical_product_upper);
    Interval _9768 = _10617;
    Interval _9769 = _10587;
    Interval _9770 = ray.outgoing.y.dx;
    Interval _10618 = jet_mul_derivative(_9769, _9770, intervalFailed, optical_product_upper);
    Interval _9771 = _10618;
    Interval _10619 = jet_add_derivative(_9768, _9771, intervalFailed);
    Interval _9772 = _10597;
    Interval _9773 = ray.outgoing.y.v;
    Interval _10620 = jet_mul_derivative(_9772, _9773, intervalFailed, optical_product_upper);
    Interval _9774 = _10620;
    Interval _9775 = _10587;
    Interval _9776 = ray.outgoing.y.dy;
    Interval _10621 = jet_mul_derivative(_9775, _9776, intervalFailed, optical_product_upper);
    Interval _9777 = _10621;
    Interval _10622 = jet_add_derivative(_9774, _9777, intervalFailed);
    Interval _9758 = _10606;
    Interval _9759 = Interval{ as_type<float>(as_type<uint>(_10616.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10616.lo) ^ 2147483648u) };
    Interval _10648 = iadd(_9758, _9759, intervalFailed);
    Interval _9760 = _10609;
    Interval _9761 = Interval{ as_type<float>(as_type<uint>(_10619.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10619.lo) ^ 2147483648u) };
    Interval _10650 = jet_add_derivative(_9760, _9761, intervalFailed);
    Interval _9762 = _10612;
    Interval _9763 = Interval{ as_type<float>(as_type<uint>(_10622.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10622.lo) ^ 2147483648u) };
    Interval _10652 = jet_add_derivative(_9762, _9763, intervalFailed);
    Interval _9744 = _10587;
    Interval _9745 = ray.outgoing.x.v;
    Interval _10656 = imul(_9744, _9745, intervalFailed, optical_product_upper);
    Interval _9746 = _10592;
    Interval _9747 = ray.outgoing.x.v;
    Interval _10657 = jet_mul_derivative(_9746, _9747, intervalFailed, optical_product_upper);
    Interval _9748 = _10657;
    Interval _9749 = _10587;
    Interval _9750 = ray.outgoing.x.dx;
    Interval _10658 = jet_mul_derivative(_9749, _9750, intervalFailed, optical_product_upper);
    Interval _9751 = _10658;
    Interval _10659 = jet_add_derivative(_9748, _9751, intervalFailed);
    Interval _9752 = _10597;
    Interval _9753 = ray.outgoing.x.v;
    Interval _10660 = jet_mul_derivative(_9752, _9753, intervalFailed, optical_product_upper);
    Interval _9754 = _10660;
    Interval _9755 = _10587;
    Interval _9756 = ray.outgoing.x.dy;
    Interval _10661 = jet_mul_derivative(_9755, _9756, intervalFailed, optical_product_upper);
    Interval _9757 = _10661;
    Interval _10662 = jet_add_derivative(_9754, _9757, intervalFailed);
    Interval _9730 = _10563;
    Interval _9731 = ray.outgoing.z.v;
    Interval _10666 = imul(_9730, _9731, intervalFailed, optical_product_upper);
    Interval _9732 = _10568;
    Interval _9733 = ray.outgoing.z.v;
    Interval _10667 = jet_mul_derivative(_9732, _9733, intervalFailed, optical_product_upper);
    Interval _9734 = _10667;
    Interval _9735 = _10563;
    Interval _9736 = ray.outgoing.z.dx;
    Interval _10668 = jet_mul_derivative(_9735, _9736, intervalFailed, optical_product_upper);
    Interval _9737 = _10668;
    Interval _10669 = jet_add_derivative(_9734, _9737, intervalFailed);
    Interval _9738 = _10573;
    Interval _9739 = ray.outgoing.z.v;
    Interval _10670 = jet_mul_derivative(_9738, _9739, intervalFailed, optical_product_upper);
    Interval _9740 = _10670;
    Interval _9741 = _10563;
    Interval _9742 = ray.outgoing.z.dy;
    Interval _10671 = jet_mul_derivative(_9741, _9742, intervalFailed, optical_product_upper);
    Interval _9743 = _10671;
    Interval _10672 = jet_add_derivative(_9740, _9743, intervalFailed);
    Interval _9724 = _10656;
    Interval _9725 = Interval{ as_type<float>(as_type<uint>(_10666.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10666.lo) ^ 2147483648u) };
    Interval _10698 = iadd(_9724, _9725, intervalFailed);
    Interval _9726 = _10659;
    Interval _9727 = Interval{ as_type<float>(as_type<uint>(_10669.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10669.lo) ^ 2147483648u) };
    Interval _10700 = jet_add_derivative(_9726, _9727, intervalFailed);
    Interval _9728 = _10662;
    Interval _9729 = Interval{ as_type<float>(as_type<uint>(_10672.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10672.lo) ^ 2147483648u) };
    Interval _10702 = jet_add_derivative(_9728, _9729, intervalFailed);
    Interval _9710 = _10563;
    Interval _9711 = ray.outgoing.y.v;
    Interval _10706 = imul(_9710, _9711, intervalFailed, optical_product_upper);
    Interval _9712 = _10568;
    Interval _9713 = ray.outgoing.y.v;
    Interval _10707 = jet_mul_derivative(_9712, _9713, intervalFailed, optical_product_upper);
    Interval _9714 = _10707;
    Interval _9715 = _10563;
    Interval _9716 = ray.outgoing.y.dx;
    Interval _10708 = jet_mul_derivative(_9715, _9716, intervalFailed, optical_product_upper);
    Interval _9717 = _10708;
    Interval _10709 = jet_add_derivative(_9714, _9717, intervalFailed);
    Interval _9718 = _10573;
    Interval _9719 = ray.outgoing.y.v;
    Interval _10710 = jet_mul_derivative(_9718, _9719, intervalFailed, optical_product_upper);
    Interval _9720 = _10710;
    Interval _9721 = _10563;
    Interval _9722 = ray.outgoing.y.dy;
    Interval _10711 = jet_mul_derivative(_9721, _9722, intervalFailed, optical_product_upper);
    Interval _9723 = _10711;
    Interval _10712 = jet_add_derivative(_9720, _9723, intervalFailed);
    Interval _9696 = _10575;
    Interval _9697 = ray.outgoing.x.v;
    Interval _10716 = imul(_9696, _9697, intervalFailed, optical_product_upper);
    Interval _9698 = _10580;
    Interval _9699 = ray.outgoing.x.v;
    Interval _10717 = jet_mul_derivative(_9698, _9699, intervalFailed, optical_product_upper);
    Interval _9700 = _10717;
    Interval _9701 = _10575;
    Interval _9702 = ray.outgoing.x.dx;
    Interval _10718 = jet_mul_derivative(_9701, _9702, intervalFailed, optical_product_upper);
    Interval _9703 = _10718;
    Interval _10719 = jet_add_derivative(_9700, _9703, intervalFailed);
    Interval _9704 = _10585;
    Interval _9705 = ray.outgoing.x.v;
    Interval _10720 = jet_mul_derivative(_9704, _9705, intervalFailed, optical_product_upper);
    Interval _9706 = _10720;
    Interval _9707 = _10575;
    Interval _9708 = ray.outgoing.x.dy;
    Interval _10721 = jet_mul_derivative(_9707, _9708, intervalFailed, optical_product_upper);
    Interval _9709 = _10721;
    Interval _10722 = jet_add_derivative(_9706, _9709, intervalFailed);
    Interval _9690 = _10706;
    Interval _9691 = Interval{ as_type<float>(as_type<uint>(_10716.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10716.lo) ^ 2147483648u) };
    Interval _10748 = iadd(_9690, _9691, intervalFailed);
    Interval _9692 = _10709;
    Interval _9693 = Interval{ as_type<float>(as_type<uint>(_10719.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10719.lo) ^ 2147483648u) };
    Interval _10750 = jet_add_derivative(_9692, _9693, intervalFailed);
    Interval _9694 = _10712;
    Interval _9695 = Interval{ as_type<float>(as_type<uint>(_10722.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10722.lo) ^ 2147483648u) };
    Interval _10752 = jet_add_derivative(_9694, _9695, intervalFailed);
    Interval _9676 = _10648;
    Interval _9677 = Interval{ focal, focal };
    Interval _10755 = imul(_9676, _9677, intervalFailed, optical_product_upper);
    Interval _9678 = _10650;
    Interval _9679 = Interval{ focal, focal };
    Interval _10757 = jet_mul_derivative(_9678, _9679, intervalFailed, optical_product_upper);
    Interval _9680 = _10757;
    Interval _9681 = _10648;
    Interval _9682 = Interval{ 0.0, 0.0 };
    Interval _10758 = jet_mul_derivative(_9681, _9682, intervalFailed, optical_product_upper);
    Interval _9683 = _10758;
    Interval _10759 = jet_add_derivative(_9680, _9683, intervalFailed);
    Interval _9684 = _10652;
    Interval _9685 = Interval{ focal, focal };
    Interval _10761 = jet_mul_derivative(_9684, _9685, intervalFailed, optical_product_upper);
    Interval _9686 = _10761;
    Interval _9687 = _10648;
    Interval _9688 = Interval{ 0.0, 0.0 };
    Interval _10762 = jet_mul_derivative(_9687, _9688, intervalFailed, optical_product_upper);
    Interval _9689 = _10762;
    Interval _10763 = jet_add_derivative(_9686, _9689, intervalFailed);
    Interval _9662 = _10698;
    Interval _9663 = Interval{ focal, focal };
    Interval _10765 = imul(_9662, _9663, intervalFailed, optical_product_upper);
    Interval _9664 = _10700;
    Interval _9665 = Interval{ focal, focal };
    Interval _10767 = jet_mul_derivative(_9664, _9665, intervalFailed, optical_product_upper);
    Interval _9666 = _10767;
    Interval _9667 = _10698;
    Interval _9668 = Interval{ 0.0, 0.0 };
    Interval _10768 = jet_mul_derivative(_9667, _9668, intervalFailed, optical_product_upper);
    Interval _9669 = _10768;
    Interval _10769 = jet_add_derivative(_9666, _9669, intervalFailed);
    Interval _9670 = _10702;
    Interval _9671 = Interval{ focal, focal };
    Interval _10771 = jet_mul_derivative(_9670, _9671, intervalFailed, optical_product_upper);
    Interval _9672 = _10771;
    Interval _9673 = _10698;
    Interval _9674 = Interval{ 0.0, 0.0 };
    Interval _10772 = jet_mul_derivative(_9673, _9674, intervalFailed, optical_product_upper);
    Interval _9675 = _10772;
    Interval _10773 = jet_add_derivative(_9672, _9675, intervalFailed);
    Interval _9648 = _10748;
    Interval _9649 = Interval{ focal, focal };
    Interval _10775 = imul(_9648, _9649, intervalFailed, optical_product_upper);
    Interval _9650 = _10750;
    Interval _9651 = Interval{ focal, focal };
    Interval _10777 = jet_mul_derivative(_9650, _9651, intervalFailed, optical_product_upper);
    Interval _9652 = _10777;
    Interval _9653 = _10748;
    Interval _9654 = Interval{ 0.0, 0.0 };
    Interval _10778 = jet_mul_derivative(_9653, _9654, intervalFailed, optical_product_upper);
    Interval _9655 = _10778;
    Interval _10779 = jet_add_derivative(_9652, _9655, intervalFailed);
    Interval _9656 = _10752;
    Interval _9657 = Interval{ focal, focal };
    Interval _10781 = jet_mul_derivative(_9656, _9657, intervalFailed, optical_product_upper);
    Interval _9658 = _10781;
    Interval _9659 = _10748;
    Interval _9660 = Interval{ 0.0, 0.0 };
    Interval _10782 = jet_mul_derivative(_9659, _9660, intervalFailed, optical_product_upper);
    Interval _9661 = _10782;
    Interval _10783 = jet_add_derivative(_9658, _9661, intervalFailed);
    if (omitted == 0u)
    {
        e0 = OpticalJet{ _10765, _10769, _10773 };
        e1 = OpticalJet{ _10775, _10779, _10783 };
    }
    else
    {
        if (omitted == 1u)
        {
            e0 = OpticalJet{ _10775, _10779, _10783 };
            e1 = OpticalJet{ _10755, _10759, _10763 };
        }
        else
        {
            e0 = OpticalJet{ _10755, _10759, _10763 };
            e1 = OpticalJet{ _10765, _10769, _10773 };
        }
    }
    bool _10797;
    if (!intervalFailed)
    {
        _10797 = jetBranchKnown;
    }
    else
    {
        _10797 = false;
    }
    return _10797;
}

static inline __attribute__((always_inline))
OpticalLocalRoot optical_local_root(thread const float4& box, thread const float2& centre, thread const Interval& f0, thread const Interval& f1, thread const Interval& j00, thread const Interval& j01, thread const Interval& j10, thread const Interval& j11, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    bool _10816;
    if (!intervalFailed)
    {
        _10816 = !jetBranchKnown;
    }
    else
    {
        _10816 = true;
    }
    bool _10825;
    if (!_10816)
    {
        bool4 _10819 = isnan(box);
        bool4 _10820 = isinf(box);
        _10825 = !all(not(bool4(_10819.x || _10820.x, _10819.y || _10820.y, _10819.z || _10820.z, _10819.w || _10820.w)));
    }
    else
    {
        _10825 = true;
    }
    bool _10834;
    if (!_10825)
    {
        bool2 _10828 = isnan(centre);
        bool2 _10829 = isinf(centre);
        _10834 = !all(not(bool2(_10828.x || _10829.x, _10828.y || _10829.y)));
    }
    else
    {
        _10834 = true;
    }
    bool _10842;
    if (!_10834)
    {
        _10842 = any(box.xy >= box.zw);
    }
    else
    {
        _10842 = true;
    }
    bool _10849;
    if (!_10842)
    {
        _10849 = any(centre <= box.xy);
    }
    else
    {
        _10849 = true;
    }
    bool _10856;
    if (!_10849)
    {
        _10856 = any(centre >= box.zw);
    }
    else
    {
        _10856 = true;
    }
    bool _10877;
    if (!_10856)
    {
        bool _10871;
        if (!(isnan(f0.lo) || isinf(f0.lo)))
        {
            _10871 = !(isnan(f0.hi) || isinf(f0.hi));
        }
        else
        {
            _10871 = false;
        }
        bool _10875;
        if (_10871)
        {
            _10875 = f0.lo <= f0.hi;
        }
        else
        {
            _10875 = false;
        }
        _10877 = !_10875;
    }
    else
    {
        _10877 = true;
    }
    bool _10898;
    if (!_10877)
    {
        bool _10892;
        if (!(isnan(f1.lo) || isinf(f1.lo)))
        {
            _10892 = !(isnan(f1.hi) || isinf(f1.hi));
        }
        else
        {
            _10892 = false;
        }
        bool _10896;
        if (_10892)
        {
            _10896 = f1.lo <= f1.hi;
        }
        else
        {
            _10896 = false;
        }
        _10898 = !_10896;
    }
    else
    {
        _10898 = true;
    }
    bool _10919;
    if (!_10898)
    {
        bool _10913;
        if (!(isnan(j00.lo) || isinf(j00.lo)))
        {
            _10913 = !(isnan(j00.hi) || isinf(j00.hi));
        }
        else
        {
            _10913 = false;
        }
        bool _10917;
        if (_10913)
        {
            _10917 = j00.lo <= j00.hi;
        }
        else
        {
            _10917 = false;
        }
        _10919 = !_10917;
    }
    else
    {
        _10919 = true;
    }
    bool _10940;
    if (!_10919)
    {
        bool _10934;
        if (!(isnan(j01.lo) || isinf(j01.lo)))
        {
            _10934 = !(isnan(j01.hi) || isinf(j01.hi));
        }
        else
        {
            _10934 = false;
        }
        bool _10938;
        if (_10934)
        {
            _10938 = j01.lo <= j01.hi;
        }
        else
        {
            _10938 = false;
        }
        _10940 = !_10938;
    }
    else
    {
        _10940 = true;
    }
    bool _10961;
    if (!_10940)
    {
        bool _10955;
        if (!(isnan(j10.lo) || isinf(j10.lo)))
        {
            _10955 = !(isnan(j10.hi) || isinf(j10.hi));
        }
        else
        {
            _10955 = false;
        }
        bool _10959;
        if (_10955)
        {
            _10959 = j10.lo <= j10.hi;
        }
        else
        {
            _10959 = false;
        }
        _10961 = !_10959;
    }
    else
    {
        _10961 = true;
    }
    bool _10982;
    if (!_10961)
    {
        bool _10976;
        if (!(isnan(j11.lo) || isinf(j11.lo)))
        {
            _10976 = !(isnan(j11.hi) || isinf(j11.hi));
        }
        else
        {
            _10976 = false;
        }
        bool _10980;
        if (_10976)
        {
            _10980 = j11.lo <= j11.hi;
        }
        else
        {
            _10980 = false;
        }
        _10982 = !_10980;
    }
    else
    {
        _10982 = true;
    }
    if (_10982)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _1756 = spvFMul(spvFAdd(j00.lo, j00.hi), 0.5);
    float _1758 = spvFMul(spvFAdd(j01.lo, j01.hi), 0.5);
    float _1760 = spvFMul(spvFAdd(j10.lo, j10.hi), 0.5);
    float _1762 = spvFMul(spvFAdd(j11.lo, j11.hi), 0.5);
    float4 _10999 = float4(_1756, _1758, _1760, _1762);
    float _1765 = spvFSub(spvFMul(_1756, _1762), spvFMul(_1758, _1760));
    bool4 _11000 = isnan(_10999);
    bool4 _11001 = isinf(_10999);
    bool _11008;
    if (all(not(bool4(_11000.x || _11001.x, _11000.y || _11001.y, _11000.z || _11001.z, _11000.w || _11001.w))))
    {
        _11008 = isnan(_1765) || isinf(_1765);
    }
    else
    {
        _11008 = true;
    }
    bool _11011;
    if (!_11008)
    {
        _11011 = _1765 == 0.0;
    }
    else
    {
        _11011 = true;
    }
    if (_11011)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float4 _1768 = float4(_1762, -_1758, -_1760, _1756) / float4(_1765);
    bool4 _11014 = isnan(_1768);
    bool4 _11015 = isinf(_1768);
    bool _11046;
    if (all(not(bool4(_11014.x || _11015.x, _11014.y || _11015.y, _11014.z || _11015.z, _11014.w || _11015.w))))
    {
        float _11019 = _1768.x;
        Interval param_var_a = Interval{ _11019, _11019 };
        float _11021 = _1768.w;
        Interval param_var_b = Interval{ _11021, _11021 };
        Interval _11023 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        float _11024 = _1768.y;
        Interval param_var_a_1 = Interval{ _11024, _11024 };
        float _11026 = _1768.z;
        Interval param_var_b_1 = Interval{ _11026, _11026 };
        Interval _11028 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        Interval _10810 = _11023;
        Interval _10811 = Interval{ as_type<float>(as_type<uint>(_11028.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11028.lo) ^ 2147483648u) };
        Interval _11038 = iadd(_10810, _10811, intervalFailed);
        bool _11045;
        if (_11038.lo <= 0.0)
        {
            _11045 = _11038.hi >= 0.0;
        }
        else
        {
            _11045 = false;
        }
        _11046 = _11045;
    }
    else
    {
        _11046 = true;
    }
    if (_11046)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _11047 = _1768.x;
    Interval param_var_a_2 = Interval{ _11047, _11047 };
    Interval param_var_b_2 = j00;
    Interval _11050 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval param_var_a_3 = _11050;
    float _11051 = _1768.y;
    Interval param_var_a_4 = Interval{ _11051, _11051 };
    Interval param_var_b_3 = j10;
    Interval _11054 = imul(param_var_a_4, param_var_b_3, intervalFailed, optical_product_upper);
    Interval param_var_b_4 = _11054;
    Interval _11055 = iadd(param_var_a_3, param_var_b_4, intervalFailed);
    Interval _10808 = Interval{ 1.0, 1.0 };
    Interval _10809 = Interval{ as_type<float>(as_type<uint>(_11055.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11055.lo) ^ 2147483648u) };
    Interval _11065 = iadd(_10808, _10809, intervalFailed);
    float _11066 = _1768.x;
    Interval param_var_a_5 = Interval{ _11066, _11066 };
    Interval param_var_b_5 = j01;
    Interval _11069 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval param_var_a_6 = _11069;
    float _11070 = _1768.y;
    Interval param_var_a_7 = Interval{ _11070, _11070 };
    Interval param_var_b_6 = j11;
    Interval _11073 = imul(param_var_a_7, param_var_b_6, intervalFailed, optical_product_upper);
    Interval param_var_b_7 = _11073;
    Interval _11074 = iadd(param_var_a_6, param_var_b_7, intervalFailed);
    float _11079 = as_type<float>(as_type<uint>(_11074.hi) ^ 2147483648u);
    float _11082 = as_type<float>(as_type<uint>(_11074.lo) ^ 2147483648u);
    float _11083 = _1768.z;
    Interval param_var_a_8 = Interval{ _11083, _11083 };
    Interval param_var_b_8 = j00;
    Interval _11086 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _11086;
    float _11087 = _1768.w;
    Interval param_var_a_10 = Interval{ _11087, _11087 };
    Interval param_var_b_9 = j10;
    Interval _11090 = imul(param_var_a_10, param_var_b_9, intervalFailed, optical_product_upper);
    Interval param_var_b_10 = _11090;
    Interval _11091 = iadd(param_var_a_9, param_var_b_10, intervalFailed);
    float _11096 = as_type<float>(as_type<uint>(_11091.hi) ^ 2147483648u);
    float _11099 = as_type<float>(as_type<uint>(_11091.lo) ^ 2147483648u);
    float _11100 = _1768.z;
    Interval param_var_a_11 = Interval{ _11100, _11100 };
    Interval param_var_b_11 = j01;
    Interval _11103 = imul(param_var_a_11, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _11103;
    float _11104 = _1768.w;
    Interval param_var_a_13 = Interval{ _11104, _11104 };
    Interval param_var_b_12 = j11;
    Interval _11107 = imul(param_var_a_13, param_var_b_12, intervalFailed, optical_product_upper);
    Interval param_var_b_13 = _11107;
    Interval _11108 = iadd(param_var_a_12, param_var_b_13, intervalFailed);
    Interval _10806 = Interval{ 1.0, 1.0 };
    Interval _10807 = Interval{ as_type<float>(as_type<uint>(_11108.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11108.lo) ^ 2147483648u) };
    Interval _11118 = iadd(_10806, _10807, intervalFailed);
    Interval _10804 = Interval{ box.x, box.z };
    Interval _10805 = Interval{ as_type<float>(as_type<uint>(centre.x) ^ 2147483648u), as_type<float>(as_type<uint>(centre.x) ^ 2147483648u) };
    Interval _11133 = iadd(_10804, _10805, intervalFailed);
    Interval _10802 = Interval{ box.y, box.w };
    Interval _10803 = Interval{ as_type<float>(as_type<uint>(centre.y) ^ 2147483648u), as_type<float>(as_type<uint>(centre.y) ^ 2147483648u) };
    Interval _11148 = iadd(_10802, _10803, intervalFailed);
    float _11151 = _1768.x;
    Interval param_var_a_14 = Interval{ _11151, _11151 };
    Interval param_var_b_14 = f0;
    Interval _11154 = imul(param_var_a_14, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_15 = _11154;
    float _11155 = _1768.y;
    Interval param_var_a_16 = Interval{ _11155, _11155 };
    Interval param_var_b_15 = f1;
    Interval _11158 = imul(param_var_a_16, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _11158;
    Interval _11159 = iadd(param_var_a_15, param_var_b_16, intervalFailed);
    Interval _10800 = Interval{ centre.x, centre.x };
    Interval _10801 = Interval{ as_type<float>(as_type<uint>(_11159.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11159.lo) ^ 2147483648u) };
    Interval _11170 = iadd(_10800, _10801, intervalFailed);
    float _11173 = _1768.z;
    Interval param_var_a_17 = Interval{ _11173, _11173 };
    Interval param_var_b_17 = f0;
    Interval _11176 = imul(param_var_a_17, param_var_b_17, intervalFailed, optical_product_upper);
    Interval param_var_a_18 = _11176;
    float _11177 = _1768.w;
    Interval param_var_a_19 = Interval{ _11177, _11177 };
    Interval param_var_b_18 = f1;
    Interval _11180 = imul(param_var_a_19, param_var_b_18, intervalFailed, optical_product_upper);
    Interval param_var_b_19 = _11180;
    Interval _11181 = iadd(param_var_a_18, param_var_b_19, intervalFailed);
    Interval _10798 = Interval{ centre.y, centre.y };
    Interval _10799 = Interval{ as_type<float>(as_type<uint>(_11181.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11181.lo) ^ 2147483648u) };
    Interval _11192 = iadd(_10798, _10799, intervalFailed);
    Interval param_var_a_20 = _11170;
    Interval param_var_a_21 = _11065;
    Interval param_var_b_20 = _11133;
    Interval _11193 = imul(param_var_a_21, param_var_b_20, intervalFailed, optical_product_upper);
    Interval param_var_a_22 = _11193;
    Interval param_var_a_23 = Interval{ _11079, _11082 };
    Interval param_var_b_21 = _11148;
    Interval _11195 = imul(param_var_a_23, param_var_b_21, intervalFailed, optical_product_upper);
    Interval param_var_b_22 = _11195;
    Interval _11196 = iadd(param_var_a_22, param_var_b_22, intervalFailed);
    Interval param_var_b_23 = _11196;
    Interval _11197 = iadd(param_var_a_20, param_var_b_23, intervalFailed);
    Interval param_var_a_24 = _11192;
    Interval param_var_a_25 = Interval{ _11096, _11099 };
    Interval param_var_b_24 = _11133;
    Interval _11201 = imul(param_var_a_25, param_var_b_24, intervalFailed, optical_product_upper);
    Interval param_var_a_26 = _11201;
    Interval param_var_a_27 = _11118;
    Interval param_var_b_25 = _11148;
    Interval _11202 = imul(param_var_a_27, param_var_b_25, intervalFailed, optical_product_upper);
    Interval param_var_b_26 = _11202;
    Interval _11203 = iadd(param_var_a_26, param_var_b_26, intervalFailed);
    Interval param_var_b_27 = _11203;
    Interval _11204 = iadd(param_var_a_24, param_var_b_27, intervalFailed);
    bool _11213;
    if (_11065.lo <= 0.0)
    {
        _11213 = _11065.hi >= 0.0;
    }
    else
    {
        _11213 = false;
    }
    float _11220;
    if (_11213)
    {
        _11220 = 0.0;
    }
    else
    {
        _11220 = precise::min(abs(_11065.lo), abs(_11065.hi));
    }
    Interval param_var_a_28 = Interval{ _11220, precise::max(abs(_11065.lo), abs(_11065.hi)) };
    bool _11229;
    if (_11079 <= 0.0)
    {
        _11229 = _11082 >= 0.0;
    }
    else
    {
        _11229 = false;
    }
    float _11236;
    if (_11229)
    {
        _11236 = 0.0;
    }
    else
    {
        _11236 = precise::min(abs(_11079), abs(_11082));
    }
    Interval param_var_b_28 = Interval{ _11236, precise::max(abs(_11079), abs(_11082)) };
    Interval _11241 = iadd(param_var_a_28, param_var_b_28, intervalFailed);
    bool _11248;
    if (_11096 <= 0.0)
    {
        _11248 = _11099 >= 0.0;
    }
    else
    {
        _11248 = false;
    }
    float _11255;
    if (_11248)
    {
        _11255 = 0.0;
    }
    else
    {
        _11255 = precise::min(abs(_11096), abs(_11099));
    }
    Interval param_var_a_29 = Interval{ _11255, precise::max(abs(_11096), abs(_11099)) };
    bool _11266;
    if (_11118.lo <= 0.0)
    {
        _11266 = _11118.hi >= 0.0;
    }
    else
    {
        _11266 = false;
    }
    float _11273;
    if (_11266)
    {
        _11273 = 0.0;
    }
    else
    {
        _11273 = precise::min(abs(_11118.lo), abs(_11118.hi));
    }
    Interval param_var_b_29 = Interval{ _11273, precise::max(abs(_11118.lo), abs(_11118.hi)) };
    Interval _11278 = iadd(param_var_a_29, param_var_b_29, intervalFailed);
    float _11281 = precise::max(_11241.lo, _11278.lo);
    float _11282 = precise::max(_11241.hi, _11278.hi);
    bool _11287;
    if (!intervalFailed)
    {
        _11287 = !jetBranchKnown;
    }
    else
    {
        _11287 = true;
    }
    bool _11307;
    if (!_11287)
    {
        bool _11301;
        if (!(isnan(_11197.lo) || isinf(_11197.lo)))
        {
            _11301 = !(isnan(_11197.hi) || isinf(_11197.hi));
        }
        else
        {
            _11301 = false;
        }
        bool _11305;
        if (_11301)
        {
            _11305 = _11197.lo <= _11197.hi;
        }
        else
        {
            _11305 = false;
        }
        _11307 = !_11305;
    }
    else
    {
        _11307 = true;
    }
    bool _11327;
    if (!_11307)
    {
        bool _11321;
        if (!(isnan(_11204.lo) || isinf(_11204.lo)))
        {
            _11321 = !(isnan(_11204.hi) || isinf(_11204.hi));
        }
        else
        {
            _11321 = false;
        }
        bool _11325;
        if (_11321)
        {
            _11325 = _11204.lo <= _11204.hi;
        }
        else
        {
            _11325 = false;
        }
        _11327 = !_11325;
    }
    else
    {
        _11327 = true;
    }
    bool _11345;
    if (!_11327)
    {
        bool _11339;
        if (!(isnan(_11281) || isinf(_11281)))
        {
            _11339 = !(isnan(_11282) || isinf(_11282));
        }
        else
        {
            _11339 = false;
        }
        bool _11343;
        if (_11339)
        {
            _11343 = _11281 <= _11282;
        }
        else
        {
            _11343 = false;
        }
        _11345 = !_11343;
    }
    else
    {
        _11345 = true;
    }
    if (_11345)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    bool _11353;
    if ((isunordered(_11197.hi, box.x) || _11197.hi >= box.x))
    {
        _11353 = _11197.lo > box.z;
    }
    else
    {
        _11353 = true;
    }
    bool _11358;
    if (!_11353)
    {
        _11358 = _11204.hi < box.y;
    }
    else
    {
        _11358 = true;
    }
    bool _11363;
    if (!_11358)
    {
        _11363 = _11204.lo > box.w;
    }
    else
    {
        _11363 = true;
    }
    uint _11382;
    if (_11363)
    {
        _11382 = 2u;
    }
    else
    {
        bool _11368;
        if (_11282 < 1.0)
        {
            _11368 = _11197.lo > box.x;
        }
        else
        {
            _11368 = false;
        }
        bool _11372;
        if (_11368)
        {
            _11372 = _11197.hi < box.z;
        }
        else
        {
            _11372 = false;
        }
        bool _11376;
        if (_11372)
        {
            _11376 = _11204.lo > box.y;
        }
        else
        {
            _11376 = false;
        }
        bool _11380;
        if (_11376)
        {
            _11380 = _11204.hi < box.w;
        }
        else
        {
            _11380 = false;
        }
        uint _11381;
        if (_11380)
        {
            _11381 = 1u;
        }
        else
        {
            _11381 = 0u;
        }
        _11382 = _11381;
    }
    return OpticalLocalRoot{ _11382, float4(_11197.lo, _11204.lo, _11197.hi, _11204.hi), _11282, short(true) };
}

static inline __attribute__((always_inline))
bool optical_intersect_inherited_root(thread OpticalLocalRoot& proved, thread const OpticalLocalRoot& next, thread bool& intervalFailed, thread bool& jetBranchKnown)
{
    bool _11390;
    if (proved.status == 1u)
    {
        _11390 = !bool(proved.evaluated);
    }
    else
    {
        _11390 = true;
    }
    bool _11395;
    if (!_11390)
    {
        _11395 = !bool(next.evaluated);
    }
    else
    {
        _11395 = true;
    }
    bool _11400;
    if (!_11395)
    {
        _11400 = next.status == 2u;
    }
    else
    {
        _11400 = true;
    }
    bool _11403;
    if (!_11400)
    {
        _11403 = intervalFailed;
    }
    else
    {
        _11403 = true;
    }
    bool _11407;
    if (!_11403)
    {
        _11407 = !jetBranchKnown;
    }
    else
    {
        _11407 = true;
    }
    if (_11407)
    {
        return false;
    }
    float4 _11426 = float4(precise::max(proved.enclosure.xy, next.enclosure.xy), precise::min(proved.enclosure.zw, next.enclosure.zw));
    bool4 _11427 = isnan(_11426);
    bool4 _11428 = isinf(_11426);
    bool _11436;
    if (all(not(bool4(_11427.x || _11428.x, _11427.y || _11428.y, _11427.z || _11428.z, _11427.w || _11428.w))))
    {
        _11436 = any(_11426.xy > _11426.zw);
    }
    else
    {
        _11436 = true;
    }
    if (_11436)
    {
        return false;
    }
    proved.enclosure = _11426;
    return true;
}

static inline __attribute__((always_inline))
void write_optical_local_root(thread const uint& outAt, thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTarget, thread const uint& refinements, device type_RWByteAddressBuffer& results, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    float2 _1702 = spvFAdd(box.xy, box.zw) * 0.5;
    OpticalJet a = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalJet b = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalLocalRoot root;
    root.status = 0u;
    root.enclosure = float4(0.0);
    root.contraction = 1000000015047466219876688855040.0;
    root.evaluated = short(false);
    bool _4098 = min(refinements, 2u) == 2u;
    float _4134;
    float _4136;
    float _4138;
    float _4140;
    _4134 = 0.0;
    _4136 = 0.0;
    _4138 = 0.0;
    _4140 = 0.0;
    OpticalJetRay ray;
    float _4100;
    float _4102;
    float _4104;
    float _4106;
    float _4108;
    float _4110;
    float _4112;
    float _4114;
    float _4116;
    float _4118;
    float _4120;
    float _4122;
    float _4124;
    float _4126;
    float _4128;
    float _4130;
    uint _4132;
    float _4135;
    float _4137;
    float _4139;
    float _4141;
    uint _4479;
    float _4099 = 0.0;
    float _4101 = 0.0;
    float _4103 = 0.0;
    float _4105 = 0.0;
    float _4107 = 0.0;
    float _4109 = 0.0;
    float _4111 = 0.0;
    float _4113 = 0.0;
    float _4115 = 0.0;
    float _4117 = 0.0;
    float _4119 = 0.0;
    float _4121 = 0.0;
    float _4123 = 0.0;
    float _4125 = 0.0;
    float _4127 = 0.0;
    float _4129 = 0.0;
    uint _4131 = 0u;
    uint _4133 = 0u;
    for (;;)
    {
        if (_4133 < (2u + min(refinements, 2u)))
        {
            bool _4149;
            if (_4133 >= 2u)
            {
                _4149 = root.status != 1u;
            }
            else
            {
                _4149 = false;
            }
            if (_4149)
            {
                _4479 = _4131;
                break;
            }
            float4 _4151 = root.enclosure;
            float2 _4155;
            if (_4133 >= 2u)
            {
                _4155 = spvFAdd(_4151.xy, _4151.zw) * 0.5;
            }
            else
            {
                _4155 = _1702;
            }
            intervalFailed = false;
            jetBranchKnown = true;
            float4 _4166;
            if (_4133 == 0u)
            {
                _4166 = box;
            }
            else
            {
                bool _4158;
                if (_4133 == 2u)
                {
                    _4158 = _4098;
                }
                else
                {
                    _4158 = false;
                }
                float4 _4164;
                if (_4158)
                {
                    _4164 = _4151;
                }
                else
                {
                    _4164 = float4(_4155.xyxy);
                }
                _4166 = _4164;
            }
            float4 param_var_box = _4166;
            ReflectionRoughFrame param_var_receiver = receiver;
            ReflectionLiquidFrame param_var_liquid = liquid;
            spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
            uint4 param_var_control = control;
            bool _4171 = optical_jet_forward(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, ray, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4171)
            {
                if (_4133 >= 2u)
                {
                    _4479 = _4131;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 9u : 8u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4133 == 0u)
            {
                float3 midpoint = float3(spvFMul(spvFAdd(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi), 0.5));
                int _4201;
                if (abs(midpoint.x) > abs(midpoint.y))
                {
                    _4201 = 0;
                }
                else
                {
                    _4201 = 1;
                }
                uint _4202 = uint(_4201);
                uint _4210;
                if (abs(midpoint.z) > abs(midpoint[_4202]))
                {
                    _4210 = 2u;
                }
                else
                {
                    _4210 = _4202;
                }
                uint _4216 = (outAt + 32u) >> 2u;
                uint2 _4218 = as_type<uint2>(float2(ray.origin.x.v.lo, ray.origin.x.v.hi));
                results._m0[_4216] = _4218.x;
                results._m0[_4216 + 1u] = _4218.y;
                uint _4228 = (outAt + 40u) >> 2u;
                uint2 _4230 = as_type<uint2>(float2(ray.origin.y.v.lo, ray.origin.y.v.hi));
                results._m0[_4228] = _4230.x;
                results._m0[_4228 + 1u] = _4230.y;
                uint _4240 = (outAt + 48u) >> 2u;
                uint2 _4242 = as_type<uint2>(float2(ray.origin.z.v.lo, ray.origin.z.v.hi));
                results._m0[_4240] = _4242.x;
                results._m0[_4240 + 1u] = _4242.y;
                uint _4252 = (outAt + 56u) >> 2u;
                uint2 _4254 = as_type<uint2>(float2(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi));
                results._m0[_4252] = _4254.x;
                results._m0[_4252 + 1u] = _4254.y;
                uint _4264 = (outAt + 64u) >> 2u;
                uint2 _4266 = as_type<uint2>(float2(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi));
                results._m0[_4264] = _4266.x;
                results._m0[_4264 + 1u] = _4266.y;
                uint _4276 = (outAt + 72u) >> 2u;
                uint2 _4278 = as_type<uint2>(float2(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi));
                results._m0[_4276] = _4278.x;
                results._m0[_4276 + 1u] = _4278.y;
                uint _4288 = (outAt + 80u) >> 2u;
                uint2 _4290 = as_type<uint2>(float2(ray.depth.v.lo, ray.depth.v.hi));
                results._m0[_4288] = _4290.x;
                results._m0[_4288 + 1u] = _4290.y;
                uint _4300 = (outAt + 88u) >> 2u;
                uint2 _4302 = as_type<uint2>(float2(ray.bias0.v.lo, ray.bias0.v.hi));
                results._m0[_4300] = _4302.x;
                results._m0[_4300 + 1u] = _4302.y;
                _4132 = _4210;
            }
            else
            {
                _4132 = _4131;
            }
            OpticalJetRay param_var_ray = ray;
            Interval3 param_var_target = target;
            bool param_var_finiteTerminal = finiteTarget;
            float param_var_focal = precise::max(receiver.projection.x, receiver.projection.y);
            uint param_var_omitted = _4132;
            bool _4317 = optical_jet_residual(param_var_ray, param_var_target, param_var_finiteTerminal, param_var_focal, param_var_omitted, a, b, intervalFailed, optical_product_upper, interval_divide_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4317)
            {
                if (_4133 >= 2u)
                {
                    _4479 = _4132;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 17u : 16u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4133 == 0u)
            {
                uint _4404 = (outAt + 96u) >> 2u;
                uint2 _4406 = as_type<uint2>(float2(a.v.lo, a.v.hi));
                results._m0[_4404] = _4406.x;
                results._m0[_4404 + 1u] = _4406.y;
                uint _4416 = (outAt + 104u) >> 2u;
                uint2 _4418 = as_type<uint2>(float2(b.v.lo, b.v.hi));
                results._m0[_4416] = _4418.x;
                results._m0[_4416 + 1u] = _4418.y;
                uint _4442 = (outAt + 112u) >> 2u;
                uint2 _4444 = as_type<uint2>(float2(a.dx.lo, a.dx.hi));
                results._m0[_4442] = _4444.x;
                results._m0[_4442 + 1u] = _4444.y;
                uint _4452 = (outAt + 120u) >> 2u;
                uint2 _4454 = as_type<uint2>(float2(a.dy.lo, a.dy.hi));
                results._m0[_4452] = _4454.x;
                results._m0[_4452 + 1u] = _4454.y;
                uint _4462 = (outAt + 128u) >> 2u;
                uint2 _4464 = as_type<uint2>(float2(b.dx.lo, b.dx.hi));
                results._m0[_4462] = _4464.x;
                results._m0[_4462 + 1u] = _4464.y;
                uint _4472 = (outAt + 136u) >> 2u;
                uint2 _4474 = as_type<uint2>(float2(b.dy.lo, b.dy.hi));
                results._m0[_4472] = _4474.x;
                results._m0[_4472 + 1u] = _4474.y;
                _4100 = _4099;
                _4102 = _4101;
                _4104 = _4103;
                _4106 = _4105;
                _4108 = _4107;
                _4110 = _4109;
                _4112 = _4111;
                _4114 = _4113;
                _4116 = b.dy.lo;
                _4118 = b.dy.hi;
                _4120 = b.dx.lo;
                _4122 = b.dx.hi;
                _4124 = a.dy.lo;
                _4126 = a.dy.hi;
                _4128 = a.dx.lo;
                _4130 = a.dx.hi;
                _4135 = _4134;
                _4137 = _4136;
                _4139 = _4138;
                _4141 = _4140;
            }
            else
            {
                float _4387;
                float _4388;
                float _4389;
                float _4390;
                float _4391;
                float _4392;
                float _4393;
                float _4394;
                float _4395;
                float _4396;
                float _4397;
                float _4398;
                if (_4133 == 1u)
                {
                    float4 param_var_box_1 = box;
                    float2 param_var_centre = _1702;
                    Interval param_var_f0 = a.v;
                    Interval param_var_f1 = b.v;
                    Interval param_var_j00 = Interval{ _4127, _4129 };
                    Interval param_var_j01 = Interval{ _4123, _4125 };
                    Interval param_var_j10 = Interval{ _4119, _4121 };
                    Interval param_var_j11 = Interval{ _4115, _4117 };
                    OpticalLocalRoot _4386 = optical_local_root(param_var_box_1, param_var_centre, param_var_f0, param_var_f1, param_var_j00, param_var_j01, param_var_j10, param_var_j11, intervalFailed, optical_product_upper, jetBranchKnown);
                    root = _4386;
                    _4387 = _4099;
                    _4388 = _4101;
                    _4389 = _4103;
                    _4390 = _4105;
                    _4391 = _4107;
                    _4392 = _4109;
                    _4393 = _4111;
                    _4394 = _4113;
                    _4395 = b.v.lo;
                    _4396 = b.v.hi;
                    _4397 = a.v.lo;
                    _4398 = a.v.hi;
                }
                else
                {
                    bool _4329;
                    if (_4133 == 2u)
                    {
                        _4329 = _4098;
                    }
                    else
                    {
                        _4329 = false;
                    }
                    float _4365;
                    float _4366;
                    float _4367;
                    float _4368;
                    float _4369;
                    float _4370;
                    float _4371;
                    float _4372;
                    if (_4329)
                    {
                        _4365 = b.dy.lo;
                        _4366 = b.dy.hi;
                        _4367 = b.dx.lo;
                        _4368 = b.dx.hi;
                        _4369 = a.dy.lo;
                        _4370 = a.dy.hi;
                        _4371 = a.dx.lo;
                        _4372 = a.dx.hi;
                    }
                    else
                    {
                        float _4330;
                        float _4331;
                        float _4332;
                        float _4333;
                        float _4334;
                        float _4335;
                        float _4336;
                        float _4337;
                        if (_4098)
                        {
                            _4330 = _4099;
                            _4331 = _4101;
                            _4332 = _4103;
                            _4333 = _4105;
                            _4334 = _4107;
                            _4335 = _4109;
                            _4336 = _4111;
                            _4337 = _4113;
                        }
                        else
                        {
                            _4330 = _4115;
                            _4331 = _4117;
                            _4332 = _4119;
                            _4333 = _4121;
                            _4334 = _4123;
                            _4335 = _4125;
                            _4336 = _4127;
                            _4337 = _4129;
                        }
                        float4 param_var_box_2 = _4151;
                        float2 param_var_centre_1 = _4155;
                        Interval param_var_f0_1 = a.v;
                        Interval param_var_f1_1 = b.v;
                        Interval param_var_j00_1 = Interval{ _4336, _4337 };
                        Interval param_var_j01_1 = Interval{ _4334, _4335 };
                        Interval param_var_j10_1 = Interval{ _4332, _4333 };
                        Interval param_var_j11_1 = Interval{ _4330, _4331 };
                        OpticalLocalRoot _4346 = optical_local_root(param_var_box_2, param_var_centre_1, param_var_f0_1, param_var_f1_1, param_var_j00_1, param_var_j01_1, param_var_j10_1, param_var_j11_1, intervalFailed, optical_product_upper, jetBranchKnown);
                        OpticalLocalRoot param_var_next = _4346;
                        bool _4347 = optical_intersect_inherited_root(root, param_var_next, intervalFailed, jetBranchKnown);
                        if (!_4347)
                        {
                            _4479 = _4132;
                            break;
                        }
                        _4365 = _4099;
                        _4366 = _4101;
                        _4367 = _4103;
                        _4368 = _4105;
                        _4369 = _4107;
                        _4370 = _4109;
                        _4371 = _4111;
                        _4372 = _4113;
                    }
                    _4387 = _4365;
                    _4388 = _4366;
                    _4389 = _4367;
                    _4390 = _4368;
                    _4391 = _4369;
                    _4392 = _4370;
                    _4393 = _4371;
                    _4394 = _4372;
                    _4395 = _4134;
                    _4396 = _4136;
                    _4397 = _4138;
                    _4398 = _4140;
                }
                _4100 = _4387;
                _4102 = _4388;
                _4104 = _4389;
                _4106 = _4390;
                _4108 = _4391;
                _4110 = _4392;
                _4112 = _4393;
                _4114 = _4394;
                _4116 = _4115;
                _4118 = _4117;
                _4120 = _4119;
                _4122 = _4121;
                _4124 = _4123;
                _4126 = _4125;
                _4128 = _4127;
                _4130 = _4129;
                _4135 = _4395;
                _4137 = _4396;
                _4139 = _4397;
                _4141 = _4398;
            }
            _4099 = _4100;
            _4101 = _4102;
            _4103 = _4104;
            _4105 = _4106;
            _4107 = _4108;
            _4109 = _4110;
            _4111 = _4112;
            _4113 = _4114;
            _4115 = _4116;
            _4117 = _4118;
            _4119 = _4120;
            _4121 = _4122;
            _4123 = _4124;
            _4125 = _4126;
            _4127 = _4128;
            _4129 = _4130;
            _4131 = _4132;
            _4133++;
            _4134 = _4135;
            _4136 = _4137;
            _4138 = _4139;
            _4140 = _4141;
            continue;
        }
        else
        {
            _4479 = _4131;
            break;
        }
    }
    uint _4481 = outAt >> 2u;
    results._m0[_4481] = 1u;
    results._m0[_4481 + 1u] = root.status;
    results._m0[_4481 + 2u] = _4479;
    results._m0[_4481 + 3u] = 0u;
    uint _4489 = (outAt + 16u) >> 2u;
    uint4 _4491 = as_type<uint4>(box);
    results._m0[_4489] = _4491.x;
    results._m0[_4489 + 1u] = _4491.y;
    results._m0[_4489 + 2u] = _4491.z;
    results._m0[_4489 + 3u] = _4491.w;
    uint _4501 = (outAt + 144u) >> 2u;
    uint4 _4504 = as_type<uint4>(root.enclosure);
    results._m0[_4501] = _4504.x;
    results._m0[_4501 + 1u] = _4504.y;
    results._m0[_4501 + 2u] = _4504.z;
    results._m0[_4501 + 3u] = _4504.w;
    uint _4514 = (outAt + 160u) >> 2u;
    uint4 _4520 = as_type<uint4>(float4(root.contraction, _1702, 0.0));
    results._m0[_4514] = _4520.x;
    results._m0[_4514 + 1u] = _4520.y;
    results._m0[_4514 + 2u] = _4520.z;
    results._m0[_4514 + 3u] = _4520.w;
    uint _4530 = (outAt + 176u) >> 2u;
    uint2 _4532 = as_type<uint2>(float2(_4138, _4140));
    results._m0[_4530] = _4532.x;
    results._m0[_4530 + 1u] = _4532.y;
    uint _4538 = (outAt + 184u) >> 2u;
    uint2 _4540 = as_type<uint2>(float2(_4134, _4136));
    results._m0[_4538] = _4540.x;
    results._m0[_4538 + 1u] = _4540.y;
}

static inline __attribute__((always_inline))
void src_feature_jets_stream_main(thread const uint3& id, constant type_Settings& Settings, device type_ByteAddressBuffer& frames, device type_ByteAddressBuffer& regions, device type_ByteAddressBuffer& queries, device type_ByteAddressBuffer& optical, device type_RWByteAddressBuffer& results, constant type_RootSettings& RootSettings, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments, constant type_TargetSettings& TargetSettings)
{
    bool _2386;
    if (id.x < Settings.queryCount)
    {
        _2386 = id.x >= 64u;
    }
    else
    {
        _2386 = true;
    }
    bool _2388;
    if (!_2386)
    {
        _2388 = false;
    }
    else
    {
        _2388 = true;
    }
    if (_2388)
    {
        return;
    }
    uint _1526 = id.x * 24576u;
    uint _1527 = id.x * 6160u;
    uint _1528 = id.x * 512u;
    for (uint _2389 = 0u; _2389 < 36u; _2389++)
    {
        uint _2391 = (_1526 + (_2389 * 16u)) >> 2u;
        results._m0[_2391] = 0u;
        results._m0[_2391 + 1u] = 0u;
        results._m0[_2391 + 2u] = 0u;
        results._m0[_2391 + 3u] = 0u;
    }
    for (uint _2396 = 3u; _2396 < 128u; _2396++)
    {
        results._m0[((_1526 + (_2396 * 192u)) + 12u) >> 2u] = 0u;
    }
    if (RootSettings.rootMode != 1u)
    {
        for (uint _2404 = 0u; _2404 < 128u; _2404++)
        {
            results._m0[((_1526 + (_2404 * 192u)) + 12u) >> 2u] = 32u;
        }
        return;
    }
    uint _2411 = _1527 >> 2u;
    uint _2413 = regions._m0[_2411];
    bool _2423;
    if (Settings.queryCount <= 64u)
    {
        _2423 = RootSettings.rootCapacity != 128u;
    }
    else
    {
        _2423 = true;
    }
    bool _2428;
    if (!_2423)
    {
        _2428 = RootSettings.rootFrameStride != 512u;
    }
    else
    {
        _2428 = true;
    }
    bool _2433;
    if (!_2428)
    {
        _2433 = RootSettings.rootResultStride != 192u;
    }
    else
    {
        _2433 = true;
    }
    bool _2438;
    if (!_2433)
    {
        _2438 = RootSettings.rootMode > 1u;
    }
    else
    {
        _2438 = true;
    }
    bool _2441;
    if (!_2438)
    {
        _2441 = _2413 > 128u;
    }
    else
    {
        _2441 = true;
    }
    bool _2444;
    if (!_2441)
    {
        _2444 = regions._m0[(_1527 + 4u) >> 2u] != 0u;
    }
    else
    {
        _2444 = true;
    }
    if (_2444)
    {
        for (uint _2446 = 0u; _2446 < 128u; _2446++)
        {
            results._m0[((_1526 + (_2446 * 192u)) + 12u) >> 2u] = 32u;
        }
        return;
    }
    float4 box = float4(1000000015047466219876688855040.0, 1000000015047466219876688855040.0, -1000000015047466219876688855040.0, -1000000015047466219876688855040.0);
    bool _2453;
    _2453 = false;
    bool _2454;
    uint _2455 = 0u;
    for (;;)
    {
        if (_2455 < _2413)
        {
            uint _2457 = (((id.x * 128u) + _2455) * 32u) >> 2u;
            uint _2459 = optical._m0[_2457];
            uint _1544 = _2457 + 2u;
            bool _2468;
            if (optical._m0[_2457 + 1u] == 0u)
            {
                _2468 = optical._m0[_1544] == 0u;
            }
            else
            {
                _2468 = true;
            }
            bool _2471;
            if (!_2468)
            {
                _2471 = optical._m0[_1544] > 8192u;
            }
            else
            {
                _2471 = true;
            }
            bool _2474;
            if (!_2471)
            {
                _2474 = _2459 > optical._m0[_1544];
            }
            else
            {
                _2474 = true;
            }
            bool _2477;
            if (!_2474)
            {
                _2477 = optical._m0[_2457 + 3u] != (optical._m0[_1544] - _2459);
            }
            else
            {
                _2477 = true;
            }
            if (_2477)
            {
                results._m0[(_1526 + 12u) >> 2u] = 32u;
                return;
            }
            if (_2459 == 0u)
            {
                _2454 = _2453;
                uint _1556 = _2455 + 1u;
                _2453 = _2454;
                _2455 = _1556;
                continue;
            }
            uint _2481 = ((((id.x * 128u) + _2455) * 32u) + 16u) >> 2u;
            uint _2483 = optical._m0[_2481];
            uint _2485 = optical._m0[_2481 + 1u];
            uint _2487 = optical._m0[_2481 + 2u];
            uint _2489 = optical._m0[_2481 + 3u];
            float4 _2491 = as_type<float4>(uint4(_2483, _2485, _2487, _2489));
            bool4 _2492 = isnan(_2491);
            bool4 _2493 = isinf(_2491);
            bool _2501;
            if (all(not(bool4(_2492.x || _2493.x, _2492.y || _2493.y, _2492.z || _2493.z, _2492.w || _2493.w))))
            {
                _2501 = any(_2491.xy > _2491.zw);
            }
            else
            {
                _2501 = true;
            }
            if (_2501)
            {
                results._m0[(_1526 + 12u) >> 2u] = 64u;
                return;
            }
            box = float4(precise::min(box.xy, _2491.xy), precise::max(box.zw, _2491.zw));
            _2454 = true;
            uint _1556 = _2455 + 1u;
            _2453 = _2454;
            _2455 = _1556;
            continue;
        }
        else
        {
            break;
        }
    }
    uint _2516 = (_1528 + 496u) >> 2u;
    if (any(uint4(frames._m0[_2516], frames._m0[_2516 + 1u], frames._m0[_2516 + 2u], frames._m0[_2516 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        results._m0[(_1526 + 12u) >> 2u] = 64u;
        return;
    }
    uint _2530 = _1528 >> 2u;
    uint _2532 = frames._m0[_2530];
    uint _2534 = frames._m0[_2530 + 1u];
    uint _2536 = frames._m0[_2530 + 2u];
    uint _2538 = frames._m0[_2530 + 3u];
    float4 _2540 = as_type<float4>(uint4(_2532, _2534, _2536, _2538));
    uint _2541 = (_1528 + 16u) >> 2u;
    uint _2543 = frames._m0[_2541];
    uint _2545 = frames._m0[_2541 + 1u];
    uint _2547 = frames._m0[_2541 + 2u];
    uint _2549 = frames._m0[_2541 + 3u];
    float4 _2551 = as_type<float4>(uint4(_2543, _2545, _2547, _2549));
    uint _2552 = (_1528 + 32u) >> 2u;
    uint _2554 = frames._m0[_2552];
    uint _2556 = frames._m0[_2552 + 1u];
    uint _2558 = frames._m0[_2552 + 2u];
    uint _2560 = frames._m0[_2552 + 3u];
    float4 _2562 = as_type<float4>(uint4(_2554, _2556, _2558, _2560));
    uint _2563 = (_1528 + 48u) >> 2u;
    uint _2565 = frames._m0[_2563];
    uint _2567 = frames._m0[_2563 + 1u];
    uint _2569 = frames._m0[_2563 + 2u];
    uint _2571 = frames._m0[_2563 + 3u];
    float4 _2573 = as_type<float4>(uint4(_2565, _2567, _2569, _2571));
    uint _2574 = (_1528 + 64u) >> 2u;
    uint _2576 = frames._m0[_2574];
    uint _2578 = frames._m0[_2574 + 1u];
    uint _2580 = frames._m0[_2574 + 2u];
    uint _2582 = frames._m0[_2574 + 3u];
    float4 _2584 = as_type<float4>(uint4(_2576, _2578, _2580, _2582));
    uint _2585 = (_1528 + 80u) >> 2u;
    uint _2587 = frames._m0[_2585];
    uint _2589 = frames._m0[_2585 + 1u];
    uint _2591 = frames._m0[_2585 + 2u];
    uint _2593 = frames._m0[_2585 + 3u];
    float4 _2595 = as_type<float4>(uint4(_2587, _2589, _2591, _2593));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint _2596 = 0u; _2596 < 4u; _2596++)
    {
        uint _2598 = ((_1528 + 96u) + (_2596 * 48u)) >> 2u;
        planes[_2596].a = as_type<float4>(uint4(frames._m0[_2598], frames._m0[_2598 + 1u], frames._m0[_2598 + 2u], frames._m0[_2598 + 3u]));
        uint _2610 = ((_1528 + 112u) + (_2596 * 48u)) >> 2u;
        planes[_2596].b = as_type<float4>(uint4(frames._m0[_2610], frames._m0[_2610 + 1u], frames._m0[_2610 + 2u], frames._m0[_2610 + 3u]));
        uint _2622 = ((_1528 + 128u) + (_2596 * 48u)) >> 2u;
        planes[_2596].c = as_type<float4>(uint4(frames._m0[_2622], frames._m0[_2622 + 1u], frames._m0[_2622 + 2u], frames._m0[_2622 + 3u]));
    }
    uint _2634 = (_1528 + 288u) >> 2u;
    uint _2636 = frames._m0[_2634];
    uint _2638 = frames._m0[_2634 + 1u];
    uint _2640 = frames._m0[_2634 + 2u];
    uint _2642 = frames._m0[_2634 + 3u];
    float4 _2644 = as_type<float4>(uint4(_2636, _2638, _2640, _2642));
    uint _2645 = (_1528 + 304u) >> 2u;
    uint _2647 = frames._m0[_2645];
    uint _2649 = frames._m0[_2645 + 1u];
    uint _2651 = frames._m0[_2645 + 2u];
    uint _2653 = frames._m0[_2645 + 3u];
    float4 _2655 = as_type<float4>(uint4(_2647, _2649, _2651, _2653));
    uint _2656 = (_1528 + 320u) >> 2u;
    uint _2658 = frames._m0[_2656];
    uint _2660 = frames._m0[_2656 + 1u];
    uint _2662 = frames._m0[_2656 + 2u];
    uint _2664 = frames._m0[_2656 + 3u];
    float4 _2666 = as_type<float4>(uint4(_2658, _2660, _2662, _2664));
    uint _2667 = (_1528 + 336u) >> 2u;
    uint _2669 = frames._m0[_2667];
    uint _2671 = frames._m0[_2667 + 1u];
    uint _2673 = frames._m0[_2667 + 2u];
    uint _2675 = frames._m0[_2667 + 3u];
    float4 _2677 = as_type<float4>(uint4(_2669, _2671, _2673, _2675));
    uint _2678 = (_1528 + 352u) >> 2u;
    uint _2680 = frames._m0[_2678];
    uint _2682 = frames._m0[_2678 + 1u];
    uint _2684 = frames._m0[_2678 + 2u];
    uint _2686 = frames._m0[_2678 + 3u];
    float4 _2688 = as_type<float4>(uint4(_2680, _2682, _2684, _2686));
    uint _2689 = (_1528 + 368u) >> 2u;
    uint _2691 = frames._m0[_2689];
    uint _2693 = frames._m0[_2689 + 1u];
    uint _2695 = frames._m0[_2689 + 2u];
    uint _2697 = frames._m0[_2689 + 3u];
    float4 _2699 = as_type<float4>(uint4(_2691, _2693, _2695, _2697));
    uint _2700 = (_1528 + 384u) >> 2u;
    uint _2702 = frames._m0[_2700];
    uint _2704 = frames._m0[_2700 + 1u];
    uint _2706 = frames._m0[_2700 + 2u];
    uint _2708 = frames._m0[_2700 + 3u];
    float4 _2710 = as_type<float4>(uint4(_2702, _2704, _2706, _2708));
    uint _2711 = (_1528 + 400u) >> 2u;
    uint _2713 = frames._m0[_2711];
    uint _2715 = frames._m0[_2711 + 1u];
    uint _2717 = frames._m0[_2711 + 2u];
    uint _2719 = frames._m0[_2711 + 3u];
    float4 _2721 = as_type<float4>(uint4(_2713, _2715, _2717, _2719));
    uint _2722 = (_1528 + 416u) >> 2u;
    uint _2724 = frames._m0[_2722];
    uint _2726 = frames._m0[_2722 + 1u];
    uint _2728 = frames._m0[_2722 + 2u];
    uint _2730 = frames._m0[_2722 + 3u];
    float4 _2732 = as_type<float4>(uint4(_2724, _2726, _2728, _2730));
    uint _2733 = (_1528 + 480u) >> 2u;
    uint _2735 = frames._m0[_2733];
    uint _1641 = _2733 + 1u;
    uint _2737 = frames._m0[_1641];
    uint _1642 = _2733 + 2u;
    uint _2739 = frames._m0[_1642];
    uint _1643 = _2733 + 3u;
    uint _2741 = frames._m0[_1643];
    uint4 _2742 = uint4(_2735, _2737, _2739, _2741);
    uint _2746 = (id.x * 48u) >> 2u;
    uint _2749 = ((id.x * 48u) + 44u) >> 2u;
    uint _2752 = (_1528 + 432u) >> 2u;
    uint _2763 = (_1528 + 448u) >> 2u;
    uint _2774 = (_1528 + 464u) >> 2u;
    bool _2789;
    if (_2735 <= 4u)
    {
        _2789 = (_2737 >> (_2735 & 31u)) != 0u;
    }
    else
    {
        _2789 = true;
    }
    bool _2792;
    if (!_2789)
    {
        _2792 = _2739 > 1u;
    }
    else
    {
        _2792 = true;
    }
    bool _2795;
    if (!_2792)
    {
        _2795 = _2741 < 1u;
    }
    else
    {
        _2795 = true;
    }
    bool _2798;
    if (!_2795)
    {
        _2798 = _2741 > 3u;
    }
    else
    {
        _2798 = true;
    }
    bool _2804;
    if (!_2798)
    {
        bool _2803;
        if (_2741 == 3u)
        {
            _2803 = _2735 != 4u;
        }
        else
        {
            _2803 = _2735 == 4u;
        }
        _2804 = _2803;
    }
    else
    {
        _2804 = true;
    }
    bool _2807;
    if (!_2804)
    {
        _2807 = queries._m0[_2746] == 4294967295u;
    }
    else
    {
        _2807 = true;
    }
    bool _2812;
    if (!_2807)
    {
        uint _2810;
        if (queries._m0[_2746] == 4294967293u)
        {
            _2810 = 1u;
        }
        else
        {
            _2810 = 0u;
        }
        _2812 = _2739 != _2810;
    }
    else
    {
        _2812 = true;
    }
    bool _2817;
    if (!_2812)
    {
        _2817 = queries._m0[_2749] >= Settings.lobes;
    }
    else
    {
        _2817 = true;
    }
    bool _2820;
    if (!_2817)
    {
        _2820 = queries._m0[_2749] > 7u;
    }
    else
    {
        _2820 = true;
    }
    bool _2827;
    if (!_2820)
    {
        _2827 = queries._m0[((id.x * 48u) + 20u) >> 2u] != (((_2741 << 8u) | (_2737 << 4u)) | _2735);
    }
    else
    {
        _2827 = true;
    }
    bool _2835;
    if (!_2827)
    {
        bool4 _2829 = isnan(_2595);
        bool4 _2830 = isinf(_2595);
        _2835 = !all(not(bool4(_2829.x || _2830.x, _2829.y || _2830.y, _2829.z || _2830.z, _2829.w || _2830.w)));
    }
    else
    {
        _2835 = true;
    }
    bool _2839;
    if (!_2835)
    {
        _2839 = _2595.x < 0.0;
    }
    else
    {
        _2839 = true;
    }
    bool _2843;
    if (!_2839)
    {
        _2843 = _2595.x > 1.0;
    }
    else
    {
        _2843 = true;
    }
    bool _2848;
    if (!_2843)
    {
        _2848 = _2595.y != float(queries._m0[_2749]);
    }
    else
    {
        _2848 = true;
    }
    bool _2853;
    if (!_2848)
    {
        _2853 = any(_2595.zw != float2(0.0));
    }
    else
    {
        _2853 = true;
    }
    bool _2861;
    if (!_2853)
    {
        bool4 _2855 = isnan(_2573);
        bool4 _2856 = isinf(_2573);
        _2861 = !all(not(bool4(_2855.x || _2856.x, _2855.y || _2856.y, _2855.z || _2856.z, _2855.w || _2856.w)));
    }
    else
    {
        _2861 = true;
    }
    bool _2866;
    if (!_2861)
    {
        _2866 = any(_2573.xy <= float2(0.0));
    }
    else
    {
        _2866 = true;
    }
    bool _2874;
    if (!_2866)
    {
        bool4 _2868 = isnan(_2584);
        bool4 _2869 = isinf(_2584);
        _2874 = !all(not(bool4(_2868.x || _2869.x, _2868.y || _2869.y, _2868.z || _2869.z, _2868.w || _2869.w)));
    }
    else
    {
        _2874 = true;
    }
    bool _2886;
    if (!_2874)
    {
        _2886 = any(_2584.xy != float2(float(Settings.width), float(Settings.height)));
    }
    else
    {
        _2886 = true;
    }
    bool _2891;
    if (!_2886)
    {
        _2891 = any(_2584.xy < float2(1.0));
    }
    else
    {
        _2891 = true;
    }
    bool _2896;
    if (!_2891)
    {
        _2896 = any(_2584.xy > float2(16384.0));
    }
    else
    {
        _2896 = true;
    }
    bool _2900;
    if (!_2896)
    {
        _2900 = _2584.z <= 0.0;
    }
    else
    {
        _2900 = true;
    }
    bool _2905;
    if (!_2900)
    {
        _2905 = _2584.w <= _2584.z;
    }
    else
    {
        _2905 = true;
    }
    bool _2913;
    if (!_2905)
    {
        bool4 _2907 = isnan(_2732);
        bool4 _2908 = isinf(_2732);
        _2913 = !all(not(bool4(_2907.x || _2908.x, _2907.y || _2908.y, _2907.z || _2908.z, _2907.w || _2908.w)));
    }
    else
    {
        _2913 = true;
    }
    bool _2920;
    if (!_2913)
    {
        int _2917;
        if (_2741 == 1u)
        {
            _2917 = 1;
        }
        else
        {
            _2917 = 0;
        }
        _2920 = _2732.w != float(_2917);
    }
    else
    {
        _2920 = true;
    }
    bool _2925;
    if (!_2920)
    {
        float3 param_var_f = _2732.xyz;
        uint param_var_kind = _2741;
        _2925 = !feature_valid(param_var_f, param_var_kind);
    }
    else
    {
        _2925 = true;
    }
    bool _2933;
    if (!_2925)
    {
        bool _2932;
        if (_2732.w != 0.0)
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ as_type<float4>(uint4(frames._m0[_2752], frames._m0[_2752 + 1u], frames._m0[_2752 + 2u], frames._m0[_2752 + 3u])), as_type<float4>(uint4(frames._m0[_2763], frames._m0[_2763 + 1u], frames._m0[_2763 + 2u], frames._m0[_2763 + 3u])), as_type<float4>(uint4(frames._m0[_2774], frames._m0[_2774 + 1u], frames._m0[_2774 + 2u], frames._m0[_2774 + 3u])) };
            _2932 = !reflection_specular_plane_valid(param_var_plane);
        }
        else
        {
            _2932 = false;
        }
        _2933 = _2932;
    }
    else
    {
        _2933 = true;
    }
    bool _2940;
    if (!_2933)
    {
        bool _2939;
        if (_2739 == 0u)
        {
            ReflectionRoughFrame param_var_old = ReflectionRoughFrame{ _2540, _2551, _2562, _2573, _2584, _2595 };
            _2939 = !reflection_rough_frame_valid(param_var_old);
        }
        else
        {
            _2939 = false;
        }
        _2940 = _2939;
    }
    else
    {
        _2940 = true;
    }
    if (_2940)
    {
        results._m0[(_1526 + 12u) >> 2u] = 64u;
        return;
    }
    bool _2945;
    if (_2739 == 0u)
    {
        _2945 = _2737 != 0u;
    }
    else
    {
        _2945 = true;
    }
    if (_2945)
    {
        ReflectionLiquidFrame param_var_old_1 = ReflectionLiquidFrame{ _2644, _2655, _2666, _2677, _2688, _2699, _2710, _2721 };
        bool _2950;
        if (reflection_liquid_frame_valid(param_var_old_1))
        {
            _2950 = any(_2573 != _2699);
        }
        else
        {
            _2950 = true;
        }
        bool _2954;
        if (!_2950)
        {
            _2954 = any(_2584 != _2710);
        }
        else
        {
            _2954 = true;
        }
        if (_2954)
        {
            results._m0[(_1526 + 12u) >> 2u] = 64u;
            return;
        }
    }
    for (uint _2957 = 0u; _2957 < _2735; _2957++)
    {
        bool _2967;
        if ((_2737 & (1u << (_2957 & 31u))) == 0u)
        {
            ReflectionSpecularPlane param_var_plane_1 = planes[_2957];
            _2967 = !reflection_specular_plane_valid(param_var_plane_1);
        }
        else
        {
            _2967 = false;
        }
        if (_2967)
        {
            results._m0[(_1526 + 12u) >> 2u] = 64u;
            return;
        }
    }
    if (!_2453)
    {
        results._m0[(_1526 + 12u) >> 2u] = 4096u;
        return;
    }
    bool4 _2974 = isnan(box);
    bool4 _2975 = isinf(box);
    bool _2984;
    if (all(not(bool4(_2974.x || _2975.x, _2974.y || _2975.y, _2974.z || _2975.z, _2974.w || _2975.w))))
    {
        _2984 = any(box.xy > box.zw);
    }
    else
    {
        _2984 = true;
    }
    if (_2984)
    {
        results._m0[(_1526 + 12u) >> 2u] = 64u;
        return;
    }
    intervalFailed = false;
    uint param_var_at = _1528;
    float4 param_var_feature = _2732;
    Interval3 _2987 = native_target(param_var_at, param_var_feature, frames, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
    if (intervalFailed)
    {
        results._m0[(_1526 + 12u) >> 2u] = 128u;
        return;
    }
    for (uint _2991 = 0u; _2991 < 2u; _2991++)
    {
        for (uint _2993 = 0u; _2993 < 12u; _2993++)
        {
            uint _2995 = (_1526 + (_2993 * 16u)) >> 2u;
            results._m0[_2995] = 0u;
            results._m0[_2995 + 1u] = 0u;
            results._m0[_2995 + 2u] = 0u;
            results._m0[_2995 + 3u] = 0u;
        }
        uint param_var_outAt = _1526;
        float4 param_var_box = box;
        ReflectionRoughFrame param_var_receiver = ReflectionRoughFrame{ _2540, _2551, _2562, _2573, _2584, _2595 };
        ReflectionLiquidFrame param_var_liquid = ReflectionLiquidFrame{ _2644, _2655, _2666, _2677, _2688, _2699, _2710, _2721 };
        spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
        uint4 param_var_control = _2742;
        Interval3 param_var_target = _2987;
        bool param_var_finiteTarget = _2732.w != 0.0;
        uint param_var_refinements = 2u;
        write_optical_local_root(param_var_outAt, param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTarget, param_var_refinements, results, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
        bool _3012;
        if (_2991 == 0u)
        {
            _3012 = results._m0[_1526 >> 2u] != 1u;
        }
        else
        {
            _3012 = true;
        }
        bool _3018;
        if (!_3012)
        {
            _3018 = results._m0[(_1526 + 4u) >> 2u] != 0u;
        }
        else
        {
            _3018 = true;
        }
        bool _3024;
        if (!_3018)
        {
            _3024 = results._m0[(_1526 + 12u) >> 2u] != 0u;
        }
        else
        {
            _3024 = true;
        }
        if (_3024)
        {
            return;
        }
        intervalFailed = false;
        Interval _2378 = Interval{ box.x, box.x };
        Interval _2379 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _3031 = iadd(_2378, _2379, intervalFailed);
        Interval _2376 = Interval{ box.y, box.y };
        Interval _2377 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _3039 = iadd(_2376, _2377, intervalFailed);
        Interval param_var_a = Interval{ box.z, box.z };
        Interval param_var_b = Interval{ 0.125, 0.125 };
        Interval _3044 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval param_var_a_1 = Interval{ box.w, box.w };
        Interval param_var_b_1 = Interval{ 0.125, 0.125 };
        Interval _3049 = iadd(param_var_a_1, param_var_b_1, intervalFailed);
        if (intervalFailed)
        {
            return;
        }
        float4 _3066 = float4(precise::min(box.xy, precise::max(float2(0.5), float2(_3031.lo, _3039.lo))), precise::max(box.zw, precise::min(spvFSub(_2584.xy, float2(0.5)), float2(_3044.hi, _3049.hi))));
        bool4 _3067 = isnan(_3066);
        bool4 _3068 = isinf(_3066);
        bool _3077;
        if (all(not(bool4(_3067.x || _3068.x, _3067.y || _3068.y, _3067.z || _3068.z, _3067.w || _3068.w))))
        {
            _3077 = any(_3066.xy > box.xy);
        }
        else
        {
            _3077 = true;
        }
        bool _3084;
        if (!_3077)
        {
            _3084 = any(_3066.zw < box.zw);
        }
        else
        {
            _3084 = true;
        }
        bool _3089;
        if (!_3084)
        {
            _3089 = all(_3066 == box);
        }
        else
        {
            _3089 = true;
        }
        if (_3089)
        {
            return;
        }
        box = _3066;
    }
}

kernel void feature_jets_stream_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& frames [[buffer(3)]], device type_ByteAddressBuffer& regions [[buffer(4)]], device type_ByteAddressBuffer& queries [[buffer(5)]], device type_ByteAddressBuffer& optical [[buffer(6)]], device type_RWByteAddressBuffer& results [[buffer(7)]], constant type_RootSettings& RootSettings [[buffer(1)]], constant type_TargetSettings& TargetSettings [[buffer(2)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    bool intervalFailed = false;
    float optical_product_upper = 0.0;
    float interval_divide_upper = 0.0;
    float interval_sine_upper = 0.0;
    bool jetBranchKnown = true;
    uint jetFailureSite = 0u;
    float4 jetFailureArguments = float4(0.0);
    uint3 param_var_id = gl_GlobalInvocationID;
    src_feature_jets_stream_main(param_var_id, Settings, frames, regions, queries, optical, results, RootSettings, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments, TargetSettings);
}

