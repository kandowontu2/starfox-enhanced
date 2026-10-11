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

constant spvUnsafeArray<int2, 8> _2306 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2366 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool feature_valid(thread const float3& f, thread const uint& kind)
{
    bool3 _3097 = isnan(f);
    bool3 _3098 = isinf(f);
    if (!all(not(bool3(_3097.x || _3098.x, _3097.y || _3098.y, _3097.z || _3098.z))))
    {
        return false;
    }
    if (kind == 1u)
    {
        bool _3112;
        if (f.z == 0.0)
        {
            _3112 = all(f.xy >= float2(0.0));
        }
        else
        {
            _3112 = false;
        }
        bool _3118;
        if (_3112)
        {
            _3118 = spvFAdd(f.x, f.y) <= 1.0;
        }
        else
        {
            _3118 = false;
        }
        return _3118;
    }
    bool _3123;
    if (kind != 2u)
    {
        _3123 = kind == 3u;
    }
    else
    {
        _3123 = true;
    }
    bool _3128;
    if (_3123)
    {
        _3128 = abs(spvFSub(dot(f, f), 1.0)) < 0.00010099999781232327222824096679688;
    }
    else
    {
        _3128 = false;
    }
    return _3128;
}

static inline __attribute__((always_inline))
bool reflection_specular_plane_valid(thread const ReflectionSpecularPlane& plane)
{
    bool _3139;
    if (plane.a.w == 2.0)
    {
        _3139 = plane.b.w == 2.0;
    }
    else
    {
        _3139 = false;
    }
    bool _3144;
    if (_3139)
    {
        _3144 = plane.c.w == 2.0;
    }
    else
    {
        _3144 = false;
    }
    bool _3161;
    if (!_3144)
    {
        bool _3154;
        if ((isunordered(plane.a.w, 1.0) || plane.a.w == 1.0))
        {
            _3154 = plane.b.w != 1.0;
        }
        else
        {
            _3154 = true;
        }
        bool _3160;
        if (!_3154)
        {
            _3160 = plane.c.w != 1.0;
        }
        else
        {
            _3160 = true;
        }
        _3161 = _3160;
    }
    else
    {
        _3161 = false;
    }
    bool _3171;
    if (!_3161)
    {
        bool4 _3165 = isnan(plane.a);
        bool4 _3166 = isinf(plane.a);
        _3171 = !all(not(bool4(_3165.x || _3166.x, _3165.y || _3166.y, _3165.z || _3166.z, _3165.w || _3166.w)));
    }
    else
    {
        _3171 = true;
    }
    bool _3181;
    if (!_3171)
    {
        bool4 _3175 = isnan(plane.b);
        bool4 _3176 = isinf(plane.b);
        _3181 = !all(not(bool4(_3175.x || _3176.x, _3175.y || _3176.y, _3175.z || _3176.z, _3175.w || _3176.w)));
    }
    else
    {
        _3181 = true;
    }
    bool _3191;
    if (!_3181)
    {
        bool4 _3185 = isnan(plane.c);
        bool4 _3186 = isinf(plane.c);
        _3191 = !all(not(bool4(_3185.x || _3186.x, _3185.y || _3186.y, _3185.z || _3186.z, _3185.w || _3186.w)));
    }
    else
    {
        _3191 = true;
    }
    bool _3199;
    if (!_3191)
    {
        _3199 = any(abs(plane.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3199 = true;
    }
    bool _3207;
    if (!_3199)
    {
        _3207 = any(abs(plane.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3207 = true;
    }
    bool _3215;
    if (!_3207)
    {
        _3215 = any(abs(plane.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3215 = true;
    }
    if (_3215)
    {
        return false;
    }
    bool _3221;
    if (_3144)
    {
        _3221 = any(plane.c.xyz != float3(0.0));
    }
    else
    {
        _3221 = false;
    }
    if (_3221)
    {
        return false;
    }
    float3 _3238;
    if (_3144)
    {
        _3238 = plane.b.xyz;
    }
    else
    {
        _3238 = cross(spvFSub(plane.b.xyz, plane.a.xyz), spvFSub(plane.c.xyz, plane.a.xyz));
    }
    float _1690 = dot(_3238, _3238);
    bool3 _3239 = isnan(_3238);
    bool3 _3240 = isinf(_3238);
    bool _3248;
    if (all(not(bool3(_3239.x || _3240.x, _3239.y || _3240.y, _3239.z || _3240.z))))
    {
        _3248 = !(isnan(_1690) || isinf(_1690));
    }
    else
    {
        _3248 = false;
    }
    bool _3250;
    if (_3248)
    {
        _3250 = _1690 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3250 = false;
    }
    return _3250;
}

static inline __attribute__((always_inline))
bool reflection_rough_frame_valid(thread const ReflectionRoughFrame& old)
{
    bool _3261;
    if (old.a.w == 2.0)
    {
        _3261 = old.b.w == 2.0;
    }
    else
    {
        _3261 = false;
    }
    bool _3266;
    if (_3261)
    {
        _3266 = old.c.w == 2.0;
    }
    else
    {
        _3266 = false;
    }
    bool _3283;
    if (!_3266)
    {
        bool _3276;
        if ((isunordered(old.a.w, 1.0) || old.a.w == 1.0))
        {
            _3276 = old.b.w != 1.0;
        }
        else
        {
            _3276 = true;
        }
        bool _3282;
        if (!_3276)
        {
            _3282 = old.c.w != 1.0;
        }
        else
        {
            _3282 = true;
        }
        _3283 = _3282;
    }
    else
    {
        _3283 = false;
    }
    bool _3293;
    if (!_3283)
    {
        bool4 _3287 = isnan(old.a);
        bool4 _3288 = isinf(old.a);
        _3293 = !all(not(bool4(_3287.x || _3288.x, _3287.y || _3288.y, _3287.z || _3288.z, _3287.w || _3288.w)));
    }
    else
    {
        _3293 = true;
    }
    bool _3303;
    if (!_3293)
    {
        bool4 _3297 = isnan(old.b);
        bool4 _3298 = isinf(old.b);
        _3303 = !all(not(bool4(_3297.x || _3298.x, _3297.y || _3298.y, _3297.z || _3298.z, _3297.w || _3298.w)));
    }
    else
    {
        _3303 = true;
    }
    bool _3313;
    if (!_3303)
    {
        bool4 _3307 = isnan(old.c);
        bool4 _3308 = isinf(old.c);
        _3313 = !all(not(bool4(_3307.x || _3308.x, _3307.y || _3308.y, _3307.z || _3308.z, _3307.w || _3308.w)));
    }
    else
    {
        _3313 = true;
    }
    bool _3323;
    if (!_3313)
    {
        bool4 _3317 = isnan(old.projection);
        bool4 _3318 = isinf(old.projection);
        _3323 = !all(not(bool4(_3317.x || _3318.x, _3317.y || _3318.y, _3317.z || _3318.z, _3317.w || _3318.w)));
    }
    else
    {
        _3323 = true;
    }
    bool _3333;
    if (!_3323)
    {
        bool4 _3327 = isnan(old.extentClip);
        bool4 _3328 = isinf(old.extentClip);
        _3333 = !all(not(bool4(_3327.x || _3328.x, _3327.y || _3328.y, _3327.z || _3328.z, _3327.w || _3328.w)));
    }
    else
    {
        _3333 = true;
    }
    bool _3343;
    if (!_3333)
    {
        bool4 _3337 = isnan(old.settings);
        bool4 _3338 = isinf(old.settings);
        _3343 = !all(not(bool4(_3337.x || _3338.x, _3337.y || _3338.y, _3337.z || _3338.z, _3337.w || _3338.w)));
    }
    else
    {
        _3343 = true;
    }
    bool _3351;
    if (!_3343)
    {
        _3351 = any(abs(old.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3351 = true;
    }
    bool _3359;
    if (!_3351)
    {
        _3359 = any(abs(old.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3359 = true;
    }
    bool _3367;
    if (!_3359)
    {
        _3367 = any(abs(old.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3367 = true;
    }
    bool _3374;
    if (!_3367)
    {
        _3374 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3374 = true;
    }
    bool _3381;
    if (!_3374)
    {
        _3381 = any(abs(old.projection) > float4(999999995904.0));
    }
    else
    {
        _3381 = true;
    }
    bool _3388;
    if (!_3381)
    {
        _3388 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3388 = true;
    }
    bool _3395;
    if (!_3388)
    {
        _3395 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3395 = true;
    }
    bool _3401;
    if (!_3395)
    {
        _3401 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3401 = true;
    }
    bool _3410;
    if (!_3401)
    {
        _3410 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3410 = true;
    }
    bool _3416;
    if (!_3410)
    {
        _3416 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3416 = true;
    }
    bool _3422;
    if (!_3416)
    {
        _3422 = old.settings.x < 0.0;
    }
    else
    {
        _3422 = true;
    }
    bool _3428;
    if (!_3422)
    {
        _3428 = old.settings.x > 1.0;
    }
    else
    {
        _3428 = true;
    }
    bool _3434;
    if (!_3428)
    {
        _3434 = old.settings.y < 0.0;
    }
    else
    {
        _3434 = true;
    }
    bool _3440;
    if (!_3434)
    {
        _3440 = old.settings.y > 7.0;
    }
    else
    {
        _3440 = true;
    }
    bool _3450;
    if (!_3440)
    {
        _3450 = floor(old.settings.y) != old.settings.y;
    }
    else
    {
        _3450 = true;
    }
    bool _3457;
    if (!_3450)
    {
        _3457 = any(old.settings.zw != float2(0.0));
    }
    else
    {
        _3457 = true;
    }
    if (_3457)
    {
        return false;
    }
    bool _3463;
    if (_3266)
    {
        _3463 = any(old.c.xyz != float3(0.0));
    }
    else
    {
        _3463 = false;
    }
    if (_3463)
    {
        return false;
    }
    float3 _3480;
    if (_3266)
    {
        _3480 = old.b.xyz;
    }
    else
    {
        _3480 = cross(spvFSub(old.b.xyz, old.a.xyz), spvFSub(old.c.xyz, old.a.xyz));
    }
    float _1693 = dot(_3480, _3480);
    bool3 _3481 = isnan(_3480);
    bool3 _3482 = isinf(_3480);
    bool _3490;
    if (all(not(bool3(_3481.x || _3482.x, _3481.y || _3482.y, _3481.z || _3482.z))))
    {
        _3490 = !(isnan(_1693) || isinf(_1693));
    }
    else
    {
        _3490 = false;
    }
    bool _3492;
    if (_3490)
    {
        _3492 = _1693 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3492 = false;
    }
    return _3492;
}

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float _1694 = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3501 = isnan(old.planePoint);
    bool4 _3502 = isinf(old.planePoint);
    bool _3514;
    if (all(not(bool4(_3501.x || _3502.x, _3501.y || _3502.y, _3501.z || _3502.z, _3501.w || _3502.w))))
    {
        bool4 _3508 = isnan(old.planeNormal);
        bool4 _3509 = isinf(old.planeNormal);
        _3514 = !all(not(bool4(_3508.x || _3509.x, _3508.y || _3509.y, _3508.z || _3509.z, _3508.w || _3509.w)));
    }
    else
    {
        _3514 = true;
    }
    bool _3524;
    if (!_3514)
    {
        bool4 _3518 = isnan(old.rotation0);
        bool4 _3519 = isinf(old.rotation0);
        _3524 = !all(not(bool4(_3518.x || _3519.x, _3518.y || _3519.y, _3518.z || _3519.z, _3518.w || _3519.w)));
    }
    else
    {
        _3524 = true;
    }
    bool _3534;
    if (!_3524)
    {
        bool4 _3528 = isnan(old.rotation1);
        bool4 _3529 = isinf(old.rotation1);
        _3534 = !all(not(bool4(_3528.x || _3529.x, _3528.y || _3529.y, _3528.z || _3529.z, _3528.w || _3529.w)));
    }
    else
    {
        _3534 = true;
    }
    bool _3544;
    if (!_3534)
    {
        bool4 _3538 = isnan(old.rotation2);
        bool4 _3539 = isinf(old.rotation2);
        _3544 = !all(not(bool4(_3538.x || _3539.x, _3538.y || _3539.y, _3538.z || _3539.z, _3538.w || _3539.w)));
    }
    else
    {
        _3544 = true;
    }
    bool _3554;
    if (!_3544)
    {
        bool4 _3548 = isnan(old.projection);
        bool4 _3549 = isinf(old.projection);
        _3554 = !all(not(bool4(_3548.x || _3549.x, _3548.y || _3549.y, _3548.z || _3549.z, _3548.w || _3549.w)));
    }
    else
    {
        _3554 = true;
    }
    bool _3564;
    if (!_3554)
    {
        bool4 _3558 = isnan(old.extentClip);
        bool4 _3559 = isinf(old.extentClip);
        _3564 = !all(not(bool4(_3558.x || _3559.x, _3558.y || _3559.y, _3558.z || _3559.z, _3558.w || _3559.w)));
    }
    else
    {
        _3564 = true;
    }
    bool _3574;
    if (!_3564)
    {
        bool4 _3568 = isnan(old.settings);
        bool4 _3569 = isinf(old.settings);
        _3574 = !all(not(bool4(_3568.x || _3569.x, _3568.y || _3569.y, _3568.z || _3569.z, _3568.w || _3569.w)));
    }
    else
    {
        _3574 = true;
    }
    bool _3582;
    if (!_3574)
    {
        _3582 = any(abs(old.planePoint.xyz) > float3(999999995904.0));
    }
    else
    {
        _3582 = true;
    }
    bool _3587;
    if (!_3582)
    {
        _3587 = isnan(_1694) || isinf(_1694);
    }
    else
    {
        _3587 = true;
    }
    bool _3590;
    if (!_3587)
    {
        _3590 = _1694 <= 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3590 = true;
    }
    bool _3597;
    if (!_3590)
    {
        _3597 = any(abs(old.rotation0) > float4(999999995904.0));
    }
    else
    {
        _3597 = true;
    }
    bool _3604;
    if (!_3597)
    {
        _3604 = any(abs(old.rotation1) > float4(999999995904.0));
    }
    else
    {
        _3604 = true;
    }
    bool _3611;
    if (!_3604)
    {
        _3611 = any(abs(old.rotation2) > float4(999999995904.0));
    }
    else
    {
        _3611 = true;
    }
    bool _3618;
    if (!_3611)
    {
        _3618 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3618 = true;
    }
    bool _3625;
    if (!_3618)
    {
        _3625 = any(old.projection.xy > float2(999999995904.0));
    }
    else
    {
        _3625 = true;
    }
    bool _3632;
    if (!_3625)
    {
        _3632 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3632 = true;
    }
    bool _3639;
    if (!_3632)
    {
        _3639 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3639 = true;
    }
    bool _3645;
    if (!_3639)
    {
        _3645 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3645 = true;
    }
    bool _3654;
    if (!_3645)
    {
        _3654 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3654 = true;
    }
    bool _3660;
    if (!_3654)
    {
        _3660 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3660 = true;
    }
    bool _3666;
    if (!_3660)
    {
        _3666 = old.settings.x < 0.0;
    }
    else
    {
        _3666 = true;
    }
    bool _3672;
    if (!_3666)
    {
        _3672 = old.settings.x > 999999995904.0;
    }
    else
    {
        _3672 = true;
    }
    bool _3683;
    if (!_3672)
    {
        bool _3682;
        if (old.settings.y != 0.0)
        {
            _3682 = old.settings.y != 3.0;
        }
        else
        {
            _3682 = false;
        }
        _3683 = _3682;
    }
    else
    {
        _3683 = true;
    }
    if (_3683)
    {
        return false;
    }
    float _1695 = dot(old.rotation0.xyz, cross(old.rotation1.xyz, old.rotation2.xyz));
    bool _3700;
    if (!(isnan(_1695) || isinf(_1695)))
    {
        _3700 = abs(_1695) > 9.9999999600419720025001879548654e-13;
    }
    else
    {
        _3700 = false;
    }
    return _3700;
}

static inline __attribute__((always_inline))
bool interval_exact_point(thread const Interval& a, thread const float& x)
{
    if (x == 0.0)
    {
        return ((as_type<uint>(a.lo) | as_type<uint>(a.hi)) & 2147483647u) == 0u;
    }
    bool _11467;
    if (as_type<uint>(a.lo) == as_type<uint>(x))
    {
        _11467 = as_type<uint>(a.hi) == as_type<uint>(x);
    }
    else
    {
        _11467 = false;
    }
    return _11467;
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
    uint _16041;
    if (x > 0.0)
    {
        _16041 = 1u;
    }
    else
    {
        _16041 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _16041);
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
    uint _16055;
    if (x < 0.0)
    {
        _16055 = 1u;
    }
    else
    {
        _16055 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _16055);
}

static __attribute__((noinline))
float optical_add_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    uint _11470 = as_type<uint>(a) & 2147483647u;
    uint _11473 = as_type<uint>(b) & 2147483647u;
    bool _11476;
    if (_11470 < 2139095040u)
    {
        _11476 = _11473 < 2139095040u;
    }
    else
    {
        _11476 = false;
    }
    if (_11476)
    {
        if (_11470 == 0u)
        {
            return b;
        }
        if (_11473 == 0u)
        {
            return a;
        }
        bool _11483;
        if (_11470 >= 8388608u)
        {
            _11483 = _11473 < 8388608u;
        }
        else
        {
            _11483 = true;
        }
        if (_11483)
        {
            intervalFailed = true;
        }
        bool _11486;
        if (_11470 >= 562036736u)
        {
            _11486 = _11470 <= 1568669696u;
        }
        else
        {
            _11486 = false;
        }
        bool _11488;
        if (_11486)
        {
            _11488 = _11473 >= 562036736u;
        }
        else
        {
            _11488 = false;
        }
        bool _11490;
        if (_11488)
        {
            _11490 = _11473 <= 1568669696u;
        }
        else
        {
            _11490 = false;
        }
        if (_11490)
        {
            float _11494;
            if (_11470 >= _11473)
            {
                _11494 = a;
            }
            else
            {
                _11494 = b;
            }
            float _11498;
            if (_11470 >= _11473)
            {
                _11498 = b;
            }
            else
            {
                _11498 = a;
            }
            float _1776 = spvFAdd(_11494, _11498);
            float _1778 = spvFSub(_11498, spvFSub(_1776, _11494));
            if (upper)
            {
                float _11502;
                if (_1778 > 0.0)
                {
                    float param_var_x = _1776;
                    float _11501 = interval_up(param_var_x, intervalFailed);
                    _11502 = _11501;
                }
                else
                {
                    _11502 = _1776;
                }
                return _11502;
            }
            float _11505;
            if (_1778 < 0.0)
            {
                float param_var_x_1 = _1776;
                float _11504 = interval_down(param_var_x_1, intervalFailed);
                _11505 = _11504;
            }
            else
            {
                _11505 = _1776;
            }
            return _11505;
        }
    }
    float _1779 = spvFAdd(a, b);
    float _11511;
    if (upper)
    {
        float param_var_x_2 = _1779;
        float _11509 = interval_up(param_var_x_2, intervalFailed);
        _11511 = _11509;
    }
    else
    {
        float param_var_x_3 = _1779;
        float _11510 = interval_down(param_var_x_3, intervalFailed);
        _11511 = _11510;
    }
    return _11511;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _4564;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _4564 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _4564 = false;
    }
    bool _4568;
    if (_4564)
    {
        _4568 = a.lo <= a.hi;
    }
    else
    {
        _4568 = false;
    }
    bool _4587;
    if (_4568)
    {
        bool _4582;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _4582 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _4582 = false;
        }
        bool _4586;
        if (_4582)
        {
            _4586 = b.lo <= b.hi;
        }
        else
        {
            _4586 = false;
        }
        _4587 = _4586;
    }
    else
    {
        _4587 = false;
    }
    if (_4587)
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
    float _4598 = optical_add_bound(param_var_a_2, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_3 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _4603 = optical_add_bound(param_var_a_3, param_var_b_1, param_var_upper_1, intervalFailed);
    return Interval{ _4598, _4603 };
}

static inline __attribute__((always_inline))
uint interval_product_extrema(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi)
{
    uint _16059 = as_type<uint>(alo) & 2147483647u;
    uint _16062 = as_type<uint>(ahi) & 2147483647u;
    uint _16065 = as_type<uint>(blo) & 2147483647u;
    uint _16068 = as_type<uint>(bhi) & 2147483647u;
    bool _16071;
    if (_16059 >= 813694976u)
    {
        _16071 = _16059 > 1317011456u;
    }
    else
    {
        _16071 = true;
    }
    bool _16074;
    if (!_16071)
    {
        _16074 = _16062 < 813694976u;
    }
    else
    {
        _16074 = true;
    }
    bool _16077;
    if (!_16074)
    {
        _16077 = _16062 > 1317011456u;
    }
    else
    {
        _16077 = true;
    }
    bool _16080;
    if (!_16077)
    {
        _16080 = _16065 < 813694976u;
    }
    else
    {
        _16080 = true;
    }
    bool _16083;
    if (!_16080)
    {
        _16083 = _16065 > 1317011456u;
    }
    else
    {
        _16083 = true;
    }
    bool _16086;
    if (!_16083)
    {
        _16086 = _16068 < 813694976u;
    }
    else
    {
        _16086 = true;
    }
    bool _16089;
    if (!_16086)
    {
        _16089 = _16068 > 1317011456u;
    }
    else
    {
        _16089 = true;
    }
    bool _16094;
    if (!_16089)
    {
        _16094 = alo > ahi;
    }
    else
    {
        _16094 = true;
    }
    bool _16099;
    if (!_16094)
    {
        _16099 = blo > bhi;
    }
    else
    {
        _16099 = true;
    }
    if (_16099)
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
    uint _16117 = as_type<uint>(a);
    uint _16118 = _16117 & 2147483647u;
    uint _16120 = as_type<uint>(b);
    uint _16121 = _16120 & 2147483647u;
    bool _16124;
    if (_16118 < 2139095040u)
    {
        _16124 = _16121 < 2139095040u;
    }
    else
    {
        _16124 = false;
    }
    if (_16124)
    {
        bool _16127;
        if (_16118 != 0u)
        {
            _16127 = _16121 == 0u;
        }
        else
        {
            _16127 = true;
        }
        if (_16127)
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
            float _16144 = as_type<float>(as_type<uint>(b) ^ 2147483648u);
            optical_product_upper = _16144;
            return _16144;
        }
        if (as_type<uint>(b) == 3212836864u)
        {
            float _16151 = as_type<float>(as_type<uint>(a) ^ 2147483648u);
            optical_product_upper = _16151;
            return _16151;
        }
        bool _16154;
        if (_16118 >= 8388608u)
        {
            _16154 = _16121 < 8388608u;
        }
        else
        {
            _16154 = true;
        }
        if (_16154)
        {
            intervalFailed = true;
        }
        bool _16157;
        if (_16118 >= 813694976u)
        {
            _16157 = _16118 <= 1317011456u;
        }
        else
        {
            _16157 = false;
        }
        bool _16159;
        if (_16157)
        {
            _16159 = _16121 >= 813694976u;
        }
        else
        {
            _16159 = false;
        }
        bool _16161;
        if (_16159)
        {
            _16161 = _16121 <= 1317011456u;
        }
        else
        {
            _16161 = false;
        }
        if (_16161)
        {
            float _1785 = spvFMul(a, b);
            uint _16168 = _16117 & 65535u;
            uint _16169 = ((_16117 & 8388607u) | 8388608u) >> 16u;
            uint _16170 = _16120 & 65535u;
            uint _16171 = ((_16120 & 8388607u) | 8388608u) >> 16u;
            uint _1786 = _16168 * _16170;
            uint _1789 = (_16168 * _16171) + (_16169 * _16170);
            uint _1790 = _1786 + (_1789 << 16u);
            uint _16175;
            if (_1790 < _1786)
            {
                _16175 = 1u;
            }
            else
            {
                _16175 = 0u;
            }
            uint _1793 = ((_16169 * _16171) + (_1789 >> 16u)) + _16175;
            uint _16176 = as_type<uint>(_1785);
            uint _1795 = (((_16176 & 2147483647u) >> 23u) - (_16118 >> 23u)) - (_16121 >> 23u);
            uint _1796 = _1795 + 150u;
            bool _16183;
            if (_1796 >= 23u)
            {
                _16183 = _1796 > 24u;
            }
            else
            {
                _16183 = true;
            }
            if (_16183)
            {
                intervalFailed = true;
                float param_var_x = _1785;
                float _16184 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _16184;
                float param_var_x_1 = _1785;
                float _16185 = interval_down(param_var_x_1, intervalFailed);
                return _16185;
            }
            uint _16187 = (_16176 & 8388607u) | 8388608u;
            uint _16189 = _16187 << (_1796 & 31u);
            uint _16191 = _16187 >> ((4294967178u - _1795) & 31u);
            bool _16196;
            if (_1793 <= _16191)
            {
                bool _16195;
                if (_1793 == _16191)
                {
                    _16195 = _1790 > _16189;
                }
                else
                {
                    _16195 = false;
                }
                _16196 = _16195;
            }
            else
            {
                _16196 = true;
            }
            bool _16201;
            if (_1793 >= _16191)
            {
                bool _16200;
                if (_1793 == _16191)
                {
                    _16200 = _1790 < _16189;
                }
                else
                {
                    _16200 = false;
                }
                _16201 = _16200;
            }
            else
            {
                _16201 = true;
            }
            bool _16208 = ((as_type<uint>(a) ^ as_type<uint>(b)) & 2147483648u) != 0u;
            bool _16209;
            if (_16208)
            {
                _16209 = _16201;
            }
            else
            {
                _16209 = _16196;
            }
            float _16211;
            if (_16209)
            {
                float param_var_x_2 = _1785;
                float _16210 = interval_up(param_var_x_2, intervalFailed);
                _16211 = _16210;
            }
            else
            {
                _16211 = _1785;
            }
            optical_product_upper = _16211;
            bool _16212;
            if (_16208)
            {
                _16212 = _16196;
            }
            else
            {
                _16212 = _16201;
            }
            float _16214;
            if (_16212)
            {
                float param_var_x_3 = _1785;
                float _16213 = interval_down(param_var_x_3, intervalFailed);
                _16214 = _16213;
            }
            else
            {
                _16214 = _1785;
            }
            return _16214;
        }
    }
    float _1798 = spvFMul(a, b);
    float param_var_x_4 = _1798;
    float _16217 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _16217;
    float param_var_x_5 = _1798;
    float _16218 = interval_down(param_var_x_5, intervalFailed);
    return _16218;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _11525;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11525 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11525 = false;
    }
    bool _11529;
    if (_11525)
    {
        _11529 = a.lo <= a.hi;
    }
    else
    {
        _11529 = false;
    }
    bool _11548;
    if (_11529)
    {
        bool _11543;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11543 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11543 = false;
        }
        bool _11547;
        if (_11543)
        {
            _11547 = b.lo <= b.hi;
        }
        else
        {
            _11547 = false;
        }
        _11548 = _11547;
    }
    else
    {
        _11548 = false;
    }
    if (_11548)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        bool _11554;
        if (!interval_exact_point(param_var_a, param_var_x))
        {
            Interval param_var_a_1 = b;
            float param_var_x_1 = 0.0;
            _11554 = interval_exact_point(param_var_a_1, param_var_x_1);
        }
        else
        {
            _11554 = true;
        }
        if (_11554)
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
    bool _11598;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11598 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11598 = false;
    }
    bool _11602;
    if (_11598)
    {
        _11602 = a.lo <= a.hi;
    }
    else
    {
        _11602 = false;
    }
    bool _11621;
    if (_11602)
    {
        bool _11616;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11616 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11616 = false;
        }
        bool _11620;
        if (_11616)
        {
            _11620 = b.lo <= b.hi;
        }
        else
        {
            _11620 = false;
        }
        _11621 = _11620;
    }
    else
    {
        _11621 = false;
    }
    bool _11629;
    if (_11621)
    {
        _11629 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _11629 = false;
    }
    bool _11637;
    if (_11621)
    {
        _11637 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _11637 = false;
    }
    bool _11639;
    if (_11621)
    {
        _11639 = !_11629;
    }
    else
    {
        _11639 = false;
    }
    bool _11641;
    if (_11639)
    {
        _11641 = !_11637;
    }
    else
    {
        _11641 = false;
    }
    uint _11651;
    if (_11641)
    {
        float param_var_alo = a.lo;
        float param_var_ahi = a.hi;
        float param_var_blo = b.lo;
        float param_var_bhi = b.hi;
        _11651 = interval_product_extrema(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi);
    }
    else
    {
        _11651 = 0u;
    }
    if (_11651 != 0u)
    {
        uint _11653 = _11651 >> 2u;
        float _11660;
        if ((_11651 & 2u) != 0u)
        {
            _11660 = a.hi;
        }
        else
        {
            _11660 = a.lo;
        }
        float param_var_a_6 = _11660;
        float _11667;
        if ((_11651 & 1u) != 0u)
        {
            _11667 = b.hi;
        }
        else
        {
            _11667 = b.lo;
        }
        float param_var_b = _11667;
        float _11668 = optical_product_bounds(param_var_a_6, param_var_b, intervalFailed, optical_product_upper);
        float _11675;
        if ((_11653 & 2u) != 0u)
        {
            _11675 = a.hi;
        }
        else
        {
            _11675 = a.lo;
        }
        float param_var_a_7 = _11675;
        float _11682;
        if ((_11653 & 1u) != 0u)
        {
            _11682 = b.hi;
        }
        else
        {
            _11682 = b.lo;
        }
        float param_var_b_1 = _11682;
        __attribute__((unused)) float _11683 = optical_product_bounds(param_var_a_7, param_var_b_1, intervalFailed, optical_product_upper);
        return Interval{ _11668, optical_product_upper };
    }
    float param_var_a_8 = a.lo;
    float param_var_b_2 = b.lo;
    float _11690 = optical_product_bounds(param_var_a_8, param_var_b_2, intervalFailed, optical_product_upper);
    float _11699;
    float _11700;
    if (!_11637)
    {
        float param_var_a_9 = a.lo;
        float param_var_b_3 = b.hi;
        float _11697 = optical_product_bounds(param_var_a_9, param_var_b_3, intervalFailed, optical_product_upper);
        _11699 = optical_product_upper;
        _11700 = _11697;
    }
    else
    {
        _11699 = optical_product_upper;
        _11700 = _11690;
    }
    float _11708;
    float _11709;
    if (!_11629)
    {
        float param_var_a_10 = a.hi;
        float param_var_b_4 = b.lo;
        float _11706 = optical_product_bounds(param_var_a_10, param_var_b_4, intervalFailed, optical_product_upper);
        _11708 = optical_product_upper;
        _11709 = _11706;
    }
    else
    {
        _11708 = optical_product_upper;
        _11709 = _11690;
    }
    float _11719;
    float _11720;
    if (!_11629)
    {
        float _11717;
        float _11718;
        if (_11637)
        {
            _11717 = _11708;
            _11718 = _11709;
        }
        else
        {
            float param_var_a_11 = a.hi;
            float param_var_b_5 = b.hi;
            float _11715 = optical_product_bounds(param_var_a_11, param_var_b_5, intervalFailed, optical_product_upper);
            _11717 = optical_product_upper;
            _11718 = _11715;
        }
        _11719 = _11717;
        _11720 = _11718;
    }
    else
    {
        _11719 = _11699;
        _11720 = _11700;
    }
    return Interval{ precise::min(precise::min(_11690, _11700), precise::min(_11709, _11720)), precise::max(precise::max(optical_product_upper, _11699), precise::max(_11708, _11719)) };
}

static __attribute__((noinline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float _21655;
    _21655 = precise::sqrt(a);
    float _21656;
    for (uint _21657 = 0u; _21657 < 8u; _21655 = _21656, _21657++)
    {
        float _1878 = spvFMul(_21655, _21655);
        float _1879 = spvFMul(_21655, 4097.0);
        float _1881 = spvFSub(_1879, spvFSub(_1879, _21655));
        float _1882 = spvFSub(_21655, _1881);
        float _1883 = spvFMul(_21655, 4097.0);
        float _1885 = spvFSub(_1883, spvFSub(_1883, _21655));
        float _1886 = spvFSub(_21655, _1885);
        float _1900 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1881, _1885), _1878), spvFMul(_1881, _1886)), spvFMul(_1882, _1885)), spvFMul(_1882, _1886)), spvFMul(_21655, 0.0)), spvFMul(0.0, _21655)), spvFMul(0.0, 0.0));
        float _1901 = spvFAdd(_1878, _1900);
        float _1903 = spvFSub(_1900, spvFSub(_1901, _1878));
        bool _21666;
        if (!(isnan(_1901) || isinf(_1901)))
        {
            _21666 = isnan(_1903) || isinf(_1903);
        }
        else
        {
            _21666 = true;
        }
        if (_21666)
        {
            intervalFailed = true;
            return _21655;
        }
        bool _21673;
        if ((isunordered(_1901, a) || _1901 >= a))
        {
            bool _21672;
            if (_1901 == a)
            {
                _21672 = _1903 < 0.0;
            }
            else
            {
                _21672 = false;
            }
            _21673 = _21672;
        }
        else
        {
            _21673 = true;
        }
        bool _21680;
        if ((isunordered(a, _1901) || a >= _1901))
        {
            bool _21679;
            if (a == _1901)
            {
                _21679 = 0.0 < _1903;
            }
            else
            {
                _21679 = false;
            }
            _21680 = _21679;
        }
        else
        {
            _21680 = true;
        }
        bool _21684;
        if (upper)
        {
            _21684 = !_21673;
        }
        else
        {
            _21684 = !_21680;
        }
        if (_21684)
        {
            return _21655;
        }
        if (upper)
        {
            float param_var_x = _21655;
            float _21686 = interval_up(param_var_x, intervalFailed);
            _21656 = _21686;
        }
        else
        {
            float param_var_x_1 = _21655;
            float _21687 = interval_down(param_var_x_1, intervalFailed);
            _21656 = precise::max(0.0, _21687);
        }
    }
    intervalFailed = true;
    return _21655;
}

static inline __attribute__((always_inline))
Interval isqrt(thread const Interval& a, thread bool& intervalFailed)
{
    bool _16225;
    if ((isunordered(a.hi, 0.0) || a.hi >= 0.0))
    {
        _16225 = a.hi > 1000000015047466219876688855040.0;
    }
    else
    {
        _16225 = true;
    }
    if (_16225)
    {
        intervalFailed = true;
        return Interval{ 0.0, 1000000015047466219876688855040.0 };
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _16229 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _16229;
    float _16230 = interval_down(param_var_x, intervalFailed);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _16235 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _16235;
    float _16236 = interval_up(param_var_x_1, intervalFailed);
    return Interval{ precise::max(0.0, _16230), _16236 };
}

static __attribute__((noinline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float _21691;
    _21691 = a / b;
    float _21692;
    for (uint _21693 = 0u; _21693 < 8u; _21691 = _21692, _21693++)
    {
        bool _21701;
        if (!(isnan(_21691) || isinf(_21691)))
        {
            _21701 = abs(_21691) > 1000000015047466219876688855040.0;
        }
        else
        {
            _21701 = true;
        }
        if (_21701)
        {
            intervalFailed = true;
            return _21691;
        }
        float _1928 = spvFMul(_21691, b);
        float _1929 = spvFMul(_21691, 4097.0);
        float _1931 = spvFSub(_1929, spvFSub(_1929, _21691));
        float _1932 = spvFSub(_21691, _1931);
        float _1933 = spvFMul(b, 4097.0);
        float _1935 = spvFSub(_1933, spvFSub(_1933, b));
        float _1936 = spvFSub(b, _1935);
        float _1950 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1931, _1935), _1928), spvFMul(_1931, _1936)), spvFMul(_1932, _1935)), spvFMul(_1932, _1936)), spvFMul(_21691, 0.0)), spvFMul(0.0, b)), spvFMul(0.0, 0.0));
        float _1951 = spvFAdd(_1928, _1950);
        float _1953 = spvFSub(_1950, spvFSub(_1951, _1928));
        bool _21710;
        if (!(isnan(_1951) || isinf(_1951)))
        {
            _21710 = isnan(_1953) || isinf(_1953);
        }
        else
        {
            _21710 = true;
        }
        if (_21710)
        {
            intervalFailed = true;
            return _21691;
        }
        bool _21717;
        if ((isunordered(_1951, a) || _1951 >= a))
        {
            bool _21716;
            if (_1951 == a)
            {
                _21716 = _1953 < 0.0;
            }
            else
            {
                _21716 = false;
            }
            _21717 = _21716;
        }
        else
        {
            _21717 = true;
        }
        bool _21724;
        if ((isunordered(a, _1951) || a >= _1951))
        {
            bool _21723;
            if (a == _1951)
            {
                _21723 = 0.0 < _1953;
            }
            else
            {
                _21723 = false;
            }
            _21724 = _21723;
        }
        else
        {
            _21724 = true;
        }
        if (b < 0.0)
        {
            bool _21730;
            if (upper)
            {
                _21730 = !_21724;
            }
            else
            {
                _21730 = !_21717;
            }
            if (_21730)
            {
                return _21691;
            }
        }
        else
        {
            bool _21734;
            if (upper)
            {
                _21734 = !_21717;
            }
            else
            {
                _21734 = !_21724;
            }
            if (_21734)
            {
                return _21691;
            }
        }
        if (upper)
        {
            float param_var_x = _21691;
            float _21736 = interval_up(param_var_x, intervalFailed);
            _21692 = _21736;
        }
        else
        {
            float param_var_x_1 = _21691;
            float _21737 = interval_down(param_var_x_1, intervalFailed);
            _21692 = _21737;
        }
    }
    intervalFailed = true;
    return _21691;
}

static __attribute__((noinline))
float interval_divide_pair(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _16250;
    if (!(isnan(alo) || isinf(alo)))
    {
        _16250 = !(isnan(ahi) || isinf(ahi));
    }
    else
    {
        _16250 = false;
    }
    bool _16254;
    if (_16250)
    {
        _16254 = alo <= ahi;
    }
    else
    {
        _16254 = false;
    }
    bool _16272;
    if (_16254)
    {
        bool _16267;
        if (!(isnan(blo) || isinf(blo)))
        {
            _16267 = !(isnan(bhi) || isinf(bhi));
        }
        else
        {
            _16267 = false;
        }
        bool _16271;
        if (_16267)
        {
            _16271 = blo <= bhi;
        }
        else
        {
            _16271 = false;
        }
        _16272 = _16271;
    }
    else
    {
        _16272 = false;
    }
    bool _16278;
    if (_16272)
    {
        _16278 = as_type<uint>(alo) == as_type<uint>(ahi);
    }
    else
    {
        _16278 = false;
    }
    bool _16284;
    if (_16272)
    {
        _16284 = as_type<uint>(blo) == as_type<uint>(bhi);
    }
    else
    {
        _16284 = false;
    }
    float param_var_a = alo;
    float param_var_b = blo;
    bool param_var_upper = false;
    float _16287 = quotient_bound(param_var_a, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_1 = alo;
    float param_var_b_1 = blo;
    bool param_var_upper_1 = true;
    float _16290 = quotient_bound(param_var_a_1, param_var_b_1, param_var_upper_1, intervalFailed);
    float _16298;
    float _16299;
    if (!_16284)
    {
        float param_var_a_2 = alo;
        float param_var_b_2 = bhi;
        bool param_var_upper_2 = false;
        float _16294 = quotient_bound(param_var_a_2, param_var_b_2, param_var_upper_2, intervalFailed);
        float param_var_a_3 = alo;
        float param_var_b_3 = bhi;
        bool param_var_upper_3 = true;
        float _16297 = quotient_bound(param_var_a_3, param_var_b_3, param_var_upper_3, intervalFailed);
        _16298 = _16297;
        _16299 = _16294;
    }
    else
    {
        _16298 = _16290;
        _16299 = _16287;
    }
    float _16307;
    float _16308;
    if (!_16278)
    {
        float param_var_a_4 = ahi;
        float param_var_b_4 = blo;
        bool param_var_upper_4 = false;
        float _16303 = quotient_bound(param_var_a_4, param_var_b_4, param_var_upper_4, intervalFailed);
        float param_var_a_5 = ahi;
        float param_var_b_5 = blo;
        bool param_var_upper_5 = true;
        float _16306 = quotient_bound(param_var_a_5, param_var_b_5, param_var_upper_5, intervalFailed);
        _16307 = _16306;
        _16308 = _16303;
    }
    else
    {
        _16307 = _16290;
        _16308 = _16287;
    }
    float _16318;
    float _16319;
    if (!_16278)
    {
        float _16316;
        float _16317;
        if (_16284)
        {
            _16316 = _16307;
            _16317 = _16308;
        }
        else
        {
            float param_var_a_6 = ahi;
            float param_var_b_6 = bhi;
            bool param_var_upper_6 = false;
            float _16312 = quotient_bound(param_var_a_6, param_var_b_6, param_var_upper_6, intervalFailed);
            float param_var_a_7 = ahi;
            float param_var_b_7 = bhi;
            bool param_var_upper_7 = true;
            float _16315 = quotient_bound(param_var_a_7, param_var_b_7, param_var_upper_7, intervalFailed);
            _16316 = _16315;
            _16317 = _16312;
        }
        _16318 = _16316;
        _16319 = _16317;
    }
    else
    {
        _16318 = _16298;
        _16319 = _16299;
    }
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16290), _16298), _16307), _16318);
    return precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16287), _16299), _16308), _16319);
}

static inline __attribute__((always_inline))
Interval idiv(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _11735;
    if (b.lo <= 0.0)
    {
        _11735 = b.hi >= 0.0;
    }
    else
    {
        _11735 = false;
    }
    bool _11745;
    if (!_11735)
    {
        _11745 = precise::max(abs(b.lo), abs(b.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11745 = true;
    }
    bool _11755;
    if (!_11745)
    {
        _11755 = precise::max(abs(a.lo), abs(a.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11755 = true;
    }
    if (_11755)
    {
        intervalFailed = true;
        return Interval{ -1000000015047466219876688855040.0, 1000000015047466219876688855040.0 };
    }
    bool _11769;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11769 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11769 = false;
    }
    bool _11773;
    if (_11769)
    {
        _11773 = a.lo <= a.hi;
    }
    else
    {
        _11773 = false;
    }
    bool _11792;
    if (_11773)
    {
        bool _11787;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11787 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11787 = false;
        }
        bool _11791;
        if (_11787)
        {
            _11791 = b.lo <= b.hi;
        }
        else
        {
            _11791 = false;
        }
        _11792 = _11791;
    }
    else
    {
        _11792 = false;
    }
    if (_11792)
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
    float _11818 = interval_divide_pair(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi, intervalFailed, interval_divide_upper);
    float param_var_x_3 = _11818;
    float _11819 = interval_down(param_var_x_3, intervalFailed);
    float param_var_x_4 = interval_divide_upper;
    float _11821 = interval_up(param_var_x_4, intervalFailed);
    return Interval{ _11819, _11821 };
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, device type_ByteAddressBuffer& frames, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        Interval param_var_a = Interval{ feature.x, feature.x };
        Interval param_var_b = Interval{ feature.y, feature.y };
        Interval _3821 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval _3810 = Interval{ 1.0, 1.0 };
        Interval _3811 = Interval{ as_type<float>(as_type<uint>(_3821.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_3821.lo) ^ 2147483648u) };
        Interval _3831 = iadd(_3810, _3811, intervalFailed);
        uint _3833 = (at + 432u) >> 2u;
        uint _3835 = frames._m0[_3833];
        uint _3837 = frames._m0[_3833 + 1u];
        uint _3839 = frames._m0[_3833 + 2u];
        uint _3841 = frames._m0[_3833 + 3u];
        float4 _3843 = as_type<float4>(uint4(_3835, _3837, _3839, _3841));
        float _3844 = _3843.x;
        float _3845 = _3843.y;
        float _3846 = _3843.z;
        Interval _3804 = Interval{ _3844, _3844 };
        Interval _3805 = _3831;
        Interval _3848 = imul(_3804, _3805, intervalFailed, optical_product_upper);
        Interval _3806 = Interval{ _3845, _3845 };
        Interval _3807 = _3831;
        Interval _3850 = imul(_3806, _3807, intervalFailed, optical_product_upper);
        Interval _3808 = Interval{ _3846, _3846 };
        Interval _3809 = _3831;
        Interval _3852 = imul(_3808, _3809, intervalFailed, optical_product_upper);
        uint _3854 = (at + 448u) >> 2u;
        uint _3856 = frames._m0[_3854];
        uint _3858 = frames._m0[_3854 + 1u];
        uint _3860 = frames._m0[_3854 + 2u];
        uint _3862 = frames._m0[_3854 + 3u];
        float4 _3864 = as_type<float4>(uint4(_3856, _3858, _3860, _3862));
        float _3865 = _3864.x;
        float _3866 = _3864.y;
        float _3867 = _3864.z;
        Interval _3798 = Interval{ _3865, _3865 };
        Interval _3799 = Interval{ feature.x, feature.x };
        Interval _3870 = imul(_3798, _3799, intervalFailed, optical_product_upper);
        Interval _3800 = Interval{ _3866, _3866 };
        Interval _3801 = Interval{ feature.x, feature.x };
        Interval _3873 = imul(_3800, _3801, intervalFailed, optical_product_upper);
        Interval _3802 = Interval{ _3867, _3867 };
        Interval _3803 = Interval{ feature.x, feature.x };
        Interval _3876 = imul(_3802, _3803, intervalFailed, optical_product_upper);
        Interval _3792 = _3848;
        Interval _3793 = _3870;
        Interval _3877 = iadd(_3792, _3793, intervalFailed);
        Interval _3794 = _3850;
        Interval _3795 = _3873;
        Interval _3878 = iadd(_3794, _3795, intervalFailed);
        Interval _3796 = _3852;
        Interval _3797 = _3876;
        Interval _3879 = iadd(_3796, _3797, intervalFailed);
        uint _3881 = (at + 464u) >> 2u;
        uint _3883 = frames._m0[_3881];
        uint _3885 = frames._m0[_3881 + 1u];
        uint _3887 = frames._m0[_3881 + 2u];
        uint _3889 = frames._m0[_3881 + 3u];
        float4 _3891 = as_type<float4>(uint4(_3883, _3885, _3887, _3889));
        float _3892 = _3891.x;
        float _3893 = _3891.y;
        float _3894 = _3891.z;
        Interval _3786 = Interval{ _3892, _3892 };
        Interval _3787 = Interval{ feature.y, feature.y };
        Interval _3897 = imul(_3786, _3787, intervalFailed, optical_product_upper);
        Interval _3788 = Interval{ _3893, _3893 };
        Interval _3789 = Interval{ feature.y, feature.y };
        Interval _3900 = imul(_3788, _3789, intervalFailed, optical_product_upper);
        Interval _3790 = Interval{ _3894, _3894 };
        Interval _3791 = Interval{ feature.y, feature.y };
        Interval _3903 = imul(_3790, _3791, intervalFailed, optical_product_upper);
        Interval _3780 = _3877;
        Interval _3781 = _3897;
        Interval _3904 = iadd(_3780, _3781, intervalFailed);
        Interval _3782 = _3878;
        Interval _3783 = _3900;
        Interval _3905 = iadd(_3782, _3783, intervalFailed);
        Interval _3784 = _3879;
        Interval _3785 = _3903;
        Interval _3906 = iadd(_3784, _3785, intervalFailed);
        return Interval3{ _3904, _3905, _3906 };
    }
    Interval _3770 = Interval{ TargetSettings.targetCurrentCube[0].x, TargetSettings.targetCurrentCube[0].x };
    Interval _3771 = Interval{ feature.x, feature.x };
    Interval _3920 = imul(_3770, _3771, intervalFailed, optical_product_upper);
    Interval _3772 = _3920;
    Interval _3773 = Interval{ TargetSettings.targetCurrentCube[0].y, TargetSettings.targetCurrentCube[0].y };
    Interval _3774 = Interval{ feature.y, feature.y };
    Interval _3923 = imul(_3773, _3774, intervalFailed, optical_product_upper);
    Interval _3775 = _3923;
    Interval _3924 = iadd(_3772, _3775, intervalFailed);
    Interval _3776 = _3924;
    Interval _3777 = Interval{ TargetSettings.targetCurrentCube[0].z, TargetSettings.targetCurrentCube[0].z };
    Interval _3778 = Interval{ feature.z, feature.z };
    Interval _3927 = imul(_3777, _3778, intervalFailed, optical_product_upper);
    Interval _3779 = _3927;
    Interval _3928 = iadd(_3776, _3779, intervalFailed);
    Interval _3760 = Interval{ TargetSettings.targetCurrentCube[1].x, TargetSettings.targetCurrentCube[1].x };
    Interval _3761 = Interval{ feature.x, feature.x };
    Interval _3937 = imul(_3760, _3761, intervalFailed, optical_product_upper);
    Interval _3762 = _3937;
    Interval _3763 = Interval{ TargetSettings.targetCurrentCube[1].y, TargetSettings.targetCurrentCube[1].y };
    Interval _3764 = Interval{ feature.y, feature.y };
    Interval _3940 = imul(_3763, _3764, intervalFailed, optical_product_upper);
    Interval _3765 = _3940;
    Interval _3941 = iadd(_3762, _3765, intervalFailed);
    Interval _3766 = _3941;
    Interval _3767 = Interval{ TargetSettings.targetCurrentCube[1].z, TargetSettings.targetCurrentCube[1].z };
    Interval _3768 = Interval{ feature.z, feature.z };
    Interval _3944 = imul(_3767, _3768, intervalFailed, optical_product_upper);
    Interval _3769 = _3944;
    Interval _3945 = iadd(_3766, _3769, intervalFailed);
    Interval _3750 = Interval{ TargetSettings.targetCurrentCube[2].x, TargetSettings.targetCurrentCube[2].x };
    Interval _3751 = Interval{ feature.x, feature.x };
    Interval _3954 = imul(_3750, _3751, intervalFailed, optical_product_upper);
    Interval _3752 = _3954;
    Interval _3753 = Interval{ TargetSettings.targetCurrentCube[2].y, TargetSettings.targetCurrentCube[2].y };
    Interval _3754 = Interval{ feature.y, feature.y };
    Interval _3957 = imul(_3753, _3754, intervalFailed, optical_product_upper);
    Interval _3755 = _3957;
    Interval _3958 = iadd(_3752, _3755, intervalFailed);
    Interval _3756 = _3958;
    Interval _3757 = Interval{ TargetSettings.targetCurrentCube[2].z, TargetSettings.targetCurrentCube[2].z };
    Interval _3758 = Interval{ feature.z, feature.z };
    Interval _3961 = imul(_3757, _3758, intervalFailed, optical_product_upper);
    Interval _3759 = _3961;
    Interval _3962 = iadd(_3756, _3759, intervalFailed);
    Interval _3740 = Interval{ TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[0].x };
    Interval _3741 = _3928;
    Interval _3976 = imul(_3740, _3741, intervalFailed, optical_product_upper);
    Interval _3742 = _3976;
    Interval _3743 = Interval{ TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[1].x };
    Interval _3744 = _3945;
    Interval _3978 = imul(_3743, _3744, intervalFailed, optical_product_upper);
    Interval _3745 = _3978;
    Interval _3979 = iadd(_3742, _3745, intervalFailed);
    Interval _3746 = _3979;
    Interval _3747 = Interval{ TargetSettings.targetPreviousCube[2].x, TargetSettings.targetPreviousCube[2].x };
    Interval _3748 = _3962;
    Interval _3981 = imul(_3747, _3748, intervalFailed, optical_product_upper);
    Interval _3749 = _3981;
    Interval _3982 = iadd(_3746, _3749, intervalFailed);
    Interval _3730 = Interval{ TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[0].y };
    Interval _3731 = _3928;
    Interval _3996 = imul(_3730, _3731, intervalFailed, optical_product_upper);
    Interval _3732 = _3996;
    Interval _3733 = Interval{ TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[1].y };
    Interval _3734 = _3945;
    Interval _3998 = imul(_3733, _3734, intervalFailed, optical_product_upper);
    Interval _3735 = _3998;
    Interval _3999 = iadd(_3732, _3735, intervalFailed);
    Interval _3736 = _3999;
    Interval _3737 = Interval{ TargetSettings.targetPreviousCube[2].y, TargetSettings.targetPreviousCube[2].y };
    Interval _3738 = _3962;
    Interval _4001 = imul(_3737, _3738, intervalFailed, optical_product_upper);
    Interval _3739 = _4001;
    Interval _4002 = iadd(_3736, _3739, intervalFailed);
    Interval _3720 = Interval{ TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[0].z };
    Interval _3721 = _3928;
    Interval _4016 = imul(_3720, _3721, intervalFailed, optical_product_upper);
    Interval _3722 = _4016;
    Interval _3723 = Interval{ TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[1].z };
    Interval _3724 = _3945;
    Interval _4018 = imul(_3723, _3724, intervalFailed, optical_product_upper);
    Interval _3725 = _4018;
    Interval _4019 = iadd(_3722, _3725, intervalFailed);
    Interval _3726 = _4019;
    Interval _3727 = Interval{ TargetSettings.targetPreviousCube[2].z, TargetSettings.targetPreviousCube[2].z };
    Interval _3728 = _3962;
    Interval _4021 = imul(_3727, _3728, intervalFailed, optical_product_upper);
    Interval _3729 = _4021;
    Interval _4022 = iadd(_3726, _3729, intervalFailed);
    Interval _3718 = Interval{ 1.0, 1.0 };
    bool _4029;
    if (_3982.lo <= 0.0)
    {
        _4029 = _3982.hi >= 0.0;
    }
    else
    {
        _4029 = false;
    }
    float _4036;
    if (_4029)
    {
        _4036 = 0.0;
    }
    else
    {
        _4036 = precise::min(abs(_3982.lo), abs(_3982.hi));
    }
    float _4039 = precise::max(abs(_3982.lo), abs(_3982.hi));
    float _3711 = spvFMul(_4036, _4036);
    float _4040 = interval_down(_3711, intervalFailed);
    float _3712 = spvFMul(_4039, _4039);
    float _4042 = interval_up(_3712, intervalFailed);
    Interval _3713 = Interval{ precise::max(0.0, _4040), _4042 };
    bool _4050;
    if (_4002.lo <= 0.0)
    {
        _4050 = _4002.hi >= 0.0;
    }
    else
    {
        _4050 = false;
    }
    float _4057;
    if (_4050)
    {
        _4057 = 0.0;
    }
    else
    {
        _4057 = precise::min(abs(_4002.lo), abs(_4002.hi));
    }
    float _4060 = precise::max(abs(_4002.lo), abs(_4002.hi));
    float _3709 = spvFMul(_4057, _4057);
    float _4061 = interval_down(_3709, intervalFailed);
    float _3710 = spvFMul(_4060, _4060);
    float _4063 = interval_up(_3710, intervalFailed);
    Interval _3714 = Interval{ precise::max(0.0, _4061), _4063 };
    Interval _4065 = iadd(_3713, _3714, intervalFailed);
    Interval _3715 = _4065;
    bool _4072;
    if (_4022.lo <= 0.0)
    {
        _4072 = _4022.hi >= 0.0;
    }
    else
    {
        _4072 = false;
    }
    float _4079;
    if (_4072)
    {
        _4079 = 0.0;
    }
    else
    {
        _4079 = precise::min(abs(_4022.lo), abs(_4022.hi));
    }
    float _4082 = precise::max(abs(_4022.lo), abs(_4022.hi));
    float _3707 = spvFMul(_4079, _4079);
    float _4083 = interval_down(_3707, intervalFailed);
    float _3708 = spvFMul(_4082, _4082);
    float _4085 = interval_up(_3708, intervalFailed);
    Interval _3716 = Interval{ precise::max(0.0, _4083), _4085 };
    Interval _4087 = iadd(_3715, _3716, intervalFailed);
    Interval _3717 = _4087;
    Interval _4088 = isqrt(_3717, intervalFailed);
    Interval _3719 = _4088;
    Interval _4089 = idiv(_3718, _3719, intervalFailed, interval_divide_upper);
    Interval _3701 = _3982;
    Interval _3702 = _4089;
    Interval _4090 = imul(_3701, _3702, intervalFailed, optical_product_upper);
    Interval _3703 = _4002;
    Interval _3704 = _4089;
    Interval _4091 = imul(_3703, _3704, intervalFailed, optical_product_upper);
    Interval _3705 = _4022;
    Interval _3706 = _4089;
    Interval _4092 = imul(_3705, _3706, intervalFailed, optical_product_upper);
    return Interval3{ _4090, _4091, _4092 };
}

static inline __attribute__((always_inline))
Interval jet_add_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _16356;
    if (a.lo == 0.0)
    {
        _16356 = a.hi == 0.0;
    }
    else
    {
        _16356 = false;
    }
    if (_16356)
    {
        return b;
    }
    bool _16365;
    if (b.lo == 0.0)
    {
        _16365 = b.hi == 0.0;
    }
    else
    {
        _16365 = false;
    }
    if (_16365)
    {
        return a;
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16369 = iadd(param_var_a, param_var_b, intervalFailed);
    return _16369;
}

static inline __attribute__((always_inline))
Interval jet_mul_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _16335;
    if (a.lo == 0.0)
    {
        _16335 = a.hi == 0.0;
    }
    else
    {
        _16335 = false;
    }
    bool _16345;
    if (!_16335)
    {
        bool _16344;
        if (b.lo == 0.0)
        {
            _16344 = b.hi == 0.0;
        }
        else
        {
            _16344 = false;
        }
        _16345 = _16344;
    }
    else
    {
        _16345 = true;
    }
    if (_16345)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16348 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    return _16348;
}

static inline __attribute__((always_inline))
Interval jet_div_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    bool _16377;
    if (a.lo == 0.0)
    {
        _16377 = a.hi == 0.0;
    }
    else
    {
        _16377 = false;
    }
    bool _16387;
    if (_16377)
    {
        bool _16385;
        if (b.lo <= 0.0)
        {
            _16385 = b.hi >= 0.0;
        }
        else
        {
            _16385 = false;
        }
        _16387 = !_16385;
    }
    else
    {
        _16387 = false;
    }
    if (_16387)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16391 = idiv(param_var_a, param_var_b, intervalFailed, interval_divide_upper);
    bool _16402;
    if (!intervalFailed)
    {
        _16402 = intervalFailed;
    }
    else
    {
        _16402 = false;
    }
    bool _16407;
    if (_16402)
    {
        _16407 = jetFailureSite == 0u;
    }
    else
    {
        _16407 = false;
    }
    if (_16407)
    {
        jetFailureSite = 2u;
        jetFailureArguments = float4(a.lo, a.hi, b.lo, b.hi);
    }
    return _16391;
}

static inline __attribute__((always_inline))
Interval3 plane_normal(thread const ReflectionSpecularPlane& p, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _16489;
    if (p.a.w == 2.0)
    {
        _16489 = p.b.w == 2.0;
    }
    else
    {
        _16489 = false;
    }
    bool _16494;
    if (_16489)
    {
        _16494 = p.c.w == 2.0;
    }
    else
    {
        _16494 = false;
    }
    if (_16494)
    {
        Interval _16477 = Interval{ 1.0, 1.0 };
        bool _16504;
        if (p.b.x <= 0.0)
        {
            _16504 = p.b.x >= 0.0;
        }
        else
        {
            _16504 = false;
        }
        float _16511;
        if (_16504)
        {
            _16511 = 0.0;
        }
        else
        {
            _16511 = precise::min(abs(p.b.x), abs(p.b.x));
        }
        float _16514 = precise::max(abs(p.b.x), abs(p.b.x));
        float _16470 = spvFMul(_16511, _16511);
        float _16515 = interval_down(_16470, intervalFailed);
        float _16471 = spvFMul(_16514, _16514);
        float _16517 = interval_up(_16471, intervalFailed);
        Interval _16472 = Interval{ precise::max(0.0, _16515), _16517 };
        bool _16523;
        if (p.b.y <= 0.0)
        {
            _16523 = p.b.y >= 0.0;
        }
        else
        {
            _16523 = false;
        }
        float _16530;
        if (_16523)
        {
            _16530 = 0.0;
        }
        else
        {
            _16530 = precise::min(abs(p.b.y), abs(p.b.y));
        }
        float _16533 = precise::max(abs(p.b.y), abs(p.b.y));
        float _16468 = spvFMul(_16530, _16530);
        float _16534 = interval_down(_16468, intervalFailed);
        float _16469 = spvFMul(_16533, _16533);
        float _16536 = interval_up(_16469, intervalFailed);
        Interval _16473 = Interval{ precise::max(0.0, _16534), _16536 };
        Interval _16538 = iadd(_16472, _16473, intervalFailed);
        Interval _16474 = _16538;
        bool _16543;
        if (p.b.z <= 0.0)
        {
            _16543 = p.b.z >= 0.0;
        }
        else
        {
            _16543 = false;
        }
        float _16550;
        if (_16543)
        {
            _16550 = 0.0;
        }
        else
        {
            _16550 = precise::min(abs(p.b.z), abs(p.b.z));
        }
        float _16553 = precise::max(abs(p.b.z), abs(p.b.z));
        float _16466 = spvFMul(_16550, _16550);
        float _16554 = interval_down(_16466, intervalFailed);
        float _16467 = spvFMul(_16553, _16553);
        float _16556 = interval_up(_16467, intervalFailed);
        Interval _16475 = Interval{ precise::max(0.0, _16554), _16556 };
        Interval _16558 = iadd(_16474, _16475, intervalFailed);
        Interval _16476 = _16558;
        Interval _16559 = isqrt(_16476, intervalFailed);
        Interval _16478 = _16559;
        Interval _16560 = idiv(_16477, _16478, intervalFailed, interval_divide_upper);
        Interval _16460 = Interval{ p.b.x, p.b.x };
        Interval _16461 = _16560;
        Interval _16562 = imul(_16460, _16461, intervalFailed, optical_product_upper);
        Interval _16462 = Interval{ p.b.y, p.b.y };
        Interval _16463 = _16560;
        Interval _16564 = imul(_16462, _16463, intervalFailed, optical_product_upper);
        Interval _16464 = Interval{ p.b.z, p.b.z };
        Interval _16465 = _16560;
        Interval _16566 = imul(_16464, _16465, intervalFailed, optical_product_upper);
        return Interval3{ _16562, _16564, _16566 };
    }
    Interval _16454 = Interval{ p.b.x, p.b.x };
    Interval _16455 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16598 = iadd(_16454, _16455, intervalFailed);
    Interval _16456 = Interval{ p.b.y, p.b.y };
    Interval _16457 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16601 = iadd(_16456, _16457, intervalFailed);
    Interval _16458 = Interval{ p.b.z, p.b.z };
    Interval _16459 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16604 = iadd(_16458, _16459, intervalFailed);
    Interval _16448 = Interval{ p.c.x, p.c.x };
    Interval _16449 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16635 = iadd(_16448, _16449, intervalFailed);
    Interval _16450 = Interval{ p.c.y, p.c.y };
    Interval _16451 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16638 = iadd(_16450, _16451, intervalFailed);
    Interval _16452 = Interval{ p.c.z, p.c.z };
    Interval _16453 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16641 = iadd(_16452, _16453, intervalFailed);
    Interval _16436 = _16601;
    Interval _16437 = _16641;
    Interval _16642 = imul(_16436, _16437, intervalFailed, optical_product_upper);
    Interval _16438 = _16604;
    Interval _16439 = _16638;
    Interval _16643 = imul(_16438, _16439, intervalFailed, optical_product_upper);
    Interval _16434 = _16642;
    Interval _16435 = Interval{ as_type<float>(as_type<uint>(_16643.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16643.lo) ^ 2147483648u) };
    Interval _16653 = iadd(_16434, _16435, intervalFailed);
    Interval _16440 = _16604;
    Interval _16441 = _16635;
    Interval _16654 = imul(_16440, _16441, intervalFailed, optical_product_upper);
    Interval _16442 = _16598;
    Interval _16443 = _16641;
    Interval _16655 = imul(_16442, _16443, intervalFailed, optical_product_upper);
    Interval _16432 = _16654;
    Interval _16433 = Interval{ as_type<float>(as_type<uint>(_16655.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16655.lo) ^ 2147483648u) };
    Interval _16665 = iadd(_16432, _16433, intervalFailed);
    Interval _16444 = _16598;
    Interval _16445 = _16638;
    Interval _16666 = imul(_16444, _16445, intervalFailed, optical_product_upper);
    Interval _16446 = _16601;
    Interval _16447 = _16635;
    Interval _16667 = imul(_16446, _16447, intervalFailed, optical_product_upper);
    Interval _16430 = _16666;
    Interval _16431 = Interval{ as_type<float>(as_type<uint>(_16667.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16667.lo) ^ 2147483648u) };
    Interval _16677 = iadd(_16430, _16431, intervalFailed);
    Interval _16428 = Interval{ 1.0, 1.0 };
    bool _16684;
    if (_16653.lo <= 0.0)
    {
        _16684 = _16653.hi >= 0.0;
    }
    else
    {
        _16684 = false;
    }
    float _16691;
    if (_16684)
    {
        _16691 = 0.0;
    }
    else
    {
        _16691 = precise::min(abs(_16653.lo), abs(_16653.hi));
    }
    float _16694 = precise::max(abs(_16653.lo), abs(_16653.hi));
    float _16421 = spvFMul(_16691, _16691);
    float _16695 = interval_down(_16421, intervalFailed);
    float _16422 = spvFMul(_16694, _16694);
    float _16697 = interval_up(_16422, intervalFailed);
    Interval _16423 = Interval{ precise::max(0.0, _16695), _16697 };
    bool _16705;
    if (_16665.lo <= 0.0)
    {
        _16705 = _16665.hi >= 0.0;
    }
    else
    {
        _16705 = false;
    }
    float _16712;
    if (_16705)
    {
        _16712 = 0.0;
    }
    else
    {
        _16712 = precise::min(abs(_16665.lo), abs(_16665.hi));
    }
    float _16715 = precise::max(abs(_16665.lo), abs(_16665.hi));
    float _16419 = spvFMul(_16712, _16712);
    float _16716 = interval_down(_16419, intervalFailed);
    float _16420 = spvFMul(_16715, _16715);
    float _16718 = interval_up(_16420, intervalFailed);
    Interval _16424 = Interval{ precise::max(0.0, _16716), _16718 };
    Interval _16720 = iadd(_16423, _16424, intervalFailed);
    Interval _16425 = _16720;
    bool _16727;
    if (_16677.lo <= 0.0)
    {
        _16727 = _16677.hi >= 0.0;
    }
    else
    {
        _16727 = false;
    }
    float _16734;
    if (_16727)
    {
        _16734 = 0.0;
    }
    else
    {
        _16734 = precise::min(abs(_16677.lo), abs(_16677.hi));
    }
    float _16737 = precise::max(abs(_16677.lo), abs(_16677.hi));
    float _16417 = spvFMul(_16734, _16734);
    float _16738 = interval_down(_16417, intervalFailed);
    float _16418 = spvFMul(_16737, _16737);
    float _16740 = interval_up(_16418, intervalFailed);
    Interval _16426 = Interval{ precise::max(0.0, _16738), _16740 };
    Interval _16742 = iadd(_16425, _16426, intervalFailed);
    Interval _16427 = _16742;
    Interval _16743 = isqrt(_16427, intervalFailed);
    Interval _16429 = _16743;
    Interval _16744 = idiv(_16428, _16429, intervalFailed, interval_divide_upper);
    Interval _16411 = _16653;
    Interval _16412 = _16744;
    Interval _16745 = imul(_16411, _16412, intervalFailed, optical_product_upper);
    Interval _16413 = _16665;
    Interval _16414 = _16744;
    Interval _16746 = imul(_16413, _16414, intervalFailed, optical_product_upper);
    Interval _16415 = _16677;
    Interval _16416 = _16744;
    Interval _16747 = imul(_16415, _16416, intervalFailed, optical_product_upper);
    return Interval3{ _16745, _16746, _16747 };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_plane_normal(thread const ReflectionSpecularPlane& plane, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = plane;
    Interval3 _11824 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _11847;
    if (!intervalFailed)
    {
        bool _11840;
        if (plane.a.w == 2.0)
        {
            _11840 = plane.b.w == 2.0;
        }
        else
        {
            _11840 = false;
        }
        bool _11845;
        if (_11840)
        {
            _11845 = plane.c.w == 2.0;
        }
        else
        {
            _11845 = false;
        }
        _11847 = !_11845;
    }
    else
    {
        _11847 = false;
    }
    if (_11847)
    {
        for (uint _11848 = 0u; _11848 < 3u; _11848++)
        {
            bool _11860;
            if ((isunordered(plane.a[_11848], plane.b[_11848]) || plane.a[_11848] == plane.b[_11848]))
            {
                _11860 = plane.a[_11848] != plane.c[_11848];
            }
            else
            {
                _11860 = true;
            }
            if (_11860)
            {
                continue;
            }
            float _11871;
            float _11872;
            if (_11848 == 0u)
            {
                _11871 = _11824.x.hi;
                _11872 = _11824.x.lo;
            }
            else
            {
                float _11869;
                float _11870;
                if (_11848 == 1u)
                {
                    _11869 = _11824.y.hi;
                    _11870 = _11824.y.lo;
                }
                else
                {
                    _11869 = _11824.z.hi;
                    _11870 = _11824.z.lo;
                }
                _11871 = _11869;
                _11872 = _11870;
            }
            bool _11875;
            if (_11872 <= 0.0)
            {
                _11875 = _11871 >= 0.0;
            }
            else
            {
                _11875 = false;
            }
            if (_11875)
            {
                continue;
            }
            float3 exact = float3(0.0);
            int _11877;
            if (_11872 > 0.0)
            {
                _11877 = 1;
            }
            else
            {
                _11877 = -1;
            }
            exact[_11848] = float(_11877);
            return OpticalJet3{ OpticalJet{ Interval{ exact.x, exact.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.y, exact.y }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.z, exact.z }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
        }
    }
    return OpticalJet3{ OpticalJet{ _11824.x, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11824.y, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11824.z, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_oriented(thread const OpticalJet3& n, thread const OpticalJet3& direction, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    Interval _11980 = n.x.v;
    Interval _11981 = direction.x.v;
    Interval _12008 = imul(_11980, _11981, intervalFailed, optical_product_upper);
    Interval _11982 = n.x.dx;
    Interval _11983 = direction.x.v;
    Interval _12009 = jet_mul_derivative(_11982, _11983, intervalFailed, optical_product_upper);
    Interval _11984 = _12009;
    Interval _11985 = n.x.v;
    Interval _11986 = direction.x.dx;
    Interval _12010 = jet_mul_derivative(_11985, _11986, intervalFailed, optical_product_upper);
    Interval _11987 = _12010;
    Interval _12011 = jet_add_derivative(_11984, _11987, intervalFailed);
    Interval _11988 = n.x.dy;
    Interval _11989 = direction.x.v;
    Interval _12012 = jet_mul_derivative(_11988, _11989, intervalFailed, optical_product_upper);
    Interval _11990 = _12012;
    Interval _11991 = n.x.v;
    Interval _11992 = direction.x.dy;
    Interval _12013 = jet_mul_derivative(_11991, _11992, intervalFailed, optical_product_upper);
    Interval _11993 = _12013;
    Interval _12014 = jet_add_derivative(_11990, _11993, intervalFailed);
    Interval _11966 = n.y.v;
    Interval _11967 = direction.y.v;
    Interval _12021 = imul(_11966, _11967, intervalFailed, optical_product_upper);
    Interval _11968 = n.y.dx;
    Interval _11969 = direction.y.v;
    Interval _12022 = jet_mul_derivative(_11968, _11969, intervalFailed, optical_product_upper);
    Interval _11970 = _12022;
    Interval _11971 = n.y.v;
    Interval _11972 = direction.y.dx;
    Interval _12023 = jet_mul_derivative(_11971, _11972, intervalFailed, optical_product_upper);
    Interval _11973 = _12023;
    Interval _12024 = jet_add_derivative(_11970, _11973, intervalFailed);
    Interval _11974 = n.y.dy;
    Interval _11975 = direction.y.v;
    Interval _12025 = jet_mul_derivative(_11974, _11975, intervalFailed, optical_product_upper);
    Interval _11976 = _12025;
    Interval _11977 = n.y.v;
    Interval _11978 = direction.y.dy;
    Interval _12026 = jet_mul_derivative(_11977, _11978, intervalFailed, optical_product_upper);
    Interval _11979 = _12026;
    Interval _12027 = jet_add_derivative(_11976, _11979, intervalFailed);
    Interval _11960 = _12008;
    Interval _11961 = _12021;
    Interval _12028 = iadd(_11960, _11961, intervalFailed);
    Interval _11962 = _12011;
    Interval _11963 = _12024;
    Interval _12029 = jet_add_derivative(_11962, _11963, intervalFailed);
    Interval _11964 = _12014;
    Interval _11965 = _12027;
    Interval _12030 = jet_add_derivative(_11964, _11965, intervalFailed);
    Interval _11946 = n.z.v;
    Interval _11947 = direction.z.v;
    Interval _12037 = imul(_11946, _11947, intervalFailed, optical_product_upper);
    Interval _11948 = n.z.dx;
    Interval _11949 = direction.z.v;
    Interval _12038 = jet_mul_derivative(_11948, _11949, intervalFailed, optical_product_upper);
    Interval _11950 = _12038;
    Interval _11951 = n.z.v;
    Interval _11952 = direction.z.dx;
    Interval _12039 = jet_mul_derivative(_11951, _11952, intervalFailed, optical_product_upper);
    Interval _11953 = _12039;
    Interval _12040 = jet_add_derivative(_11950, _11953, intervalFailed);
    Interval _11954 = n.z.dy;
    Interval _11955 = direction.z.v;
    Interval _12041 = jet_mul_derivative(_11954, _11955, intervalFailed, optical_product_upper);
    Interval _11956 = _12041;
    Interval _11957 = n.z.v;
    Interval _11958 = direction.z.dy;
    Interval _12042 = jet_mul_derivative(_11957, _11958, intervalFailed, optical_product_upper);
    Interval _11959 = _12042;
    Interval _12043 = jet_add_derivative(_11956, _11959, intervalFailed);
    Interval _11940 = _12028;
    Interval _11941 = _12037;
    Interval _12044 = iadd(_11940, _11941, intervalFailed);
    Interval _11942 = _12029;
    Interval _11943 = _12040;
    __attribute__((unused)) Interval _12045 = jet_add_derivative(_11942, _11943, intervalFailed);
    Interval _11944 = _12030;
    Interval _11945 = _12043;
    __attribute__((unused)) Interval _12046 = jet_add_derivative(_11944, _11945, intervalFailed);
    if (_12044.lo > 0.0)
    {
        Interval _11926 = n.x.v;
        Interval _11927 = Interval{ -1.0, -1.0 };
        Interval _12057 = imul(_11926, _11927, intervalFailed, optical_product_upper);
        Interval _11928 = n.x.dx;
        Interval _11929 = Interval{ -1.0, -1.0 };
        Interval _12058 = jet_mul_derivative(_11928, _11929, intervalFailed, optical_product_upper);
        Interval _11930 = _12058;
        Interval _11931 = n.x.v;
        Interval _11932 = Interval{ 0.0, 0.0 };
        Interval _12059 = jet_mul_derivative(_11931, _11932, intervalFailed, optical_product_upper);
        Interval _11933 = _12059;
        Interval _12060 = jet_add_derivative(_11930, _11933, intervalFailed);
        Interval _11934 = n.x.dy;
        Interval _11935 = Interval{ -1.0, -1.0 };
        Interval _12061 = jet_mul_derivative(_11934, _11935, intervalFailed, optical_product_upper);
        Interval _11936 = _12061;
        Interval _11937 = n.x.v;
        Interval _11938 = Interval{ 0.0, 0.0 };
        Interval _12062 = jet_mul_derivative(_11937, _11938, intervalFailed, optical_product_upper);
        Interval _11939 = _12062;
        Interval _12063 = jet_add_derivative(_11936, _11939, intervalFailed);
        Interval _11912 = n.y.v;
        Interval _11913 = Interval{ -1.0, -1.0 };
        Interval _12067 = imul(_11912, _11913, intervalFailed, optical_product_upper);
        Interval _11914 = n.y.dx;
        Interval _11915 = Interval{ -1.0, -1.0 };
        Interval _12068 = jet_mul_derivative(_11914, _11915, intervalFailed, optical_product_upper);
        Interval _11916 = _12068;
        Interval _11917 = n.y.v;
        Interval _11918 = Interval{ 0.0, 0.0 };
        Interval _12069 = jet_mul_derivative(_11917, _11918, intervalFailed, optical_product_upper);
        Interval _11919 = _12069;
        Interval _12070 = jet_add_derivative(_11916, _11919, intervalFailed);
        Interval _11920 = n.y.dy;
        Interval _11921 = Interval{ -1.0, -1.0 };
        Interval _12071 = jet_mul_derivative(_11920, _11921, intervalFailed, optical_product_upper);
        Interval _11922 = _12071;
        Interval _11923 = n.y.v;
        Interval _11924 = Interval{ 0.0, 0.0 };
        Interval _12072 = jet_mul_derivative(_11923, _11924, intervalFailed, optical_product_upper);
        Interval _11925 = _12072;
        Interval _12073 = jet_add_derivative(_11922, _11925, intervalFailed);
        Interval _11898 = n.z.v;
        Interval _11899 = Interval{ -1.0, -1.0 };
        Interval _12077 = imul(_11898, _11899, intervalFailed, optical_product_upper);
        Interval _11900 = n.z.dx;
        Interval _11901 = Interval{ -1.0, -1.0 };
        Interval _12078 = jet_mul_derivative(_11900, _11901, intervalFailed, optical_product_upper);
        Interval _11902 = _12078;
        Interval _11903 = n.z.v;
        Interval _11904 = Interval{ 0.0, 0.0 };
        Interval _12079 = jet_mul_derivative(_11903, _11904, intervalFailed, optical_product_upper);
        Interval _11905 = _12079;
        Interval _12080 = jet_add_derivative(_11902, _11905, intervalFailed);
        Interval _11906 = n.z.dy;
        Interval _11907 = Interval{ -1.0, -1.0 };
        Interval _12081 = jet_mul_derivative(_11906, _11907, intervalFailed, optical_product_upper);
        Interval _11908 = _12081;
        Interval _11909 = n.z.v;
        Interval _11910 = Interval{ 0.0, 0.0 };
        Interval _12082 = jet_mul_derivative(_11909, _11910, intervalFailed, optical_product_upper);
        Interval _11911 = _12082;
        Interval _12083 = jet_add_derivative(_11908, _11911, intervalFailed);
        return OpticalJet3{ OpticalJet{ _12057, _12060, _12063 }, OpticalJet{ _12067, _12070, _12073 }, OpticalJet{ _12077, _12080, _12083 } };
    }
    if (_12044.hi < 0.0)
    {
        return n;
    }
    jetBranchKnown = false;
    return n;
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
    bool _12168;
    if (a.v.lo <= 0.0)
    {
        _12168 = a.v.hi >= 0.0;
    }
    else
    {
        _12168 = false;
    }
    float _12175;
    if (_12168)
    {
        _12175 = 0.0;
    }
    else
    {
        _12175 = precise::min(abs(a.v.lo), abs(a.v.hi));
    }
    return OpticalJet{ Interval{ _12175, precise::max(abs(a.v.lo), abs(a.v.hi)) }, Interval{ precise::min(a.dx.lo, as_type<float>(as_type<uint>(a.dx.hi) ^ 2147483648u)), precise::max(a.dx.hi, as_type<float>(as_type<uint>(a.dx.lo) ^ 2147483648u)) }, Interval{ precise::min(a.dy.lo, as_type<float>(as_type<uint>(a.dy.hi) ^ 2147483648u)), precise::max(a.dy.hi, as_type<float>(as_type<uint>(a.dy.lo) ^ 2147483648u)) } };
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        Interval param_var_a = Interval{ n, n };
        Interval param_var_b = Interval{ as_type<float>(1036831948u), as_type<float>(1036831950u) };
        Interval _16756 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _16756;
    }
    if (d == 100.0)
    {
        Interval param_var_a_1 = Interval{ n, n };
        Interval param_var_b_1 = Interval{ as_type<float>(1008981769u), as_type<float>(1008981771u) };
        Interval _16764 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _16764;
    }
    if (d == 1000.0)
    {
        Interval param_var_a_2 = Interval{ n, n };
        Interval param_var_b_2 = Interval{ as_type<float>(981668462u), as_type<float>(981668464u) };
        Interval _16772 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _16772;
    }
    if (d == 10000.0)
    {
        Interval param_var_a_3 = Interval{ n, n };
        Interval param_var_b_3 = Interval{ as_type<float>(953267990u), as_type<float>(953267992u) };
        Interval _16780 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _16780;
    }
    if (d == 100000.0)
    {
        Interval param_var_a_4 = Interval{ n, n };
        Interval param_var_b_4 = Interval{ as_type<float>(925353387u), as_type<float>(925353389u) };
        Interval _16788 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _16788;
    }
    if (d == 128.0)
    {
        Interval param_var_a_5 = Interval{ n, n };
        Interval param_var_b_5 = Interval{ as_type<float>(1006632960u), as_type<float>(1006632960u) };
        Interval _16796 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _16796;
    }
    if (d == 65535.0)
    {
        Interval param_var_a_6 = Interval{ n, n };
        Interval param_var_b_6 = Interval{ as_type<float>(931135615u), as_type<float>(931135617u) };
        Interval _16804 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _16804;
    }
    Interval param_var_a_7 = Interval{ n, n };
    Interval param_var_b_7 = Interval{ d, d };
    Interval _16809 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _16809;
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
    bool _21819;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _21819 = isnan(a.hi) || isinf(a.hi);
    }
    else
    {
        _21819 = true;
    }
    bool _21829;
    if (!_21819)
    {
        _21829 = precise::max(abs(a.lo), abs(a.hi)) > 1048576.0;
    }
    else
    {
        _21829 = true;
    }
    if (_21829)
    {
        intervalFailed = true;
        return Interval{ -1.0, 1.0 };
    }
    float _1808 = spvFMul(floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5)), 2.0);
    Interval param_var_a = Interval{ _1808, _1808 };
    float _21806 = 3.1415927410125732421875;
    float _21837 = interval_down(_21806, intervalFailed);
    float _21807 = 3.1415927410125732421875;
    float _21838 = interval_up(_21807, intervalFailed);
    Interval param_var_b = Interval{ _21837, _21838 };
    Interval _21840 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    Interval _21804 = a;
    Interval _21805 = Interval{ as_type<float>(as_type<uint>(_21840.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21840.lo) ^ 2147483648u) };
    Interval _21850 = iadd(_21804, _21805, intervalFailed);
    float _21802 = 3.1415927410125732421875;
    float _21853 = interval_down(_21802, intervalFailed);
    float _21803 = 3.1415927410125732421875;
    float _21854 = interval_up(_21803, intervalFailed);
    Interval param_var_a_1 = Interval{ _21853, _21854 };
    Interval param_var_b_1 = Interval{ 0.5, 0.5 };
    Interval _21856 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    float _21898;
    float _21899;
    if (_21850.lo > _21856.hi)
    {
        float _21800 = 3.1415927410125732421875;
        float _21859 = interval_down(_21800, intervalFailed);
        float _21801 = 3.1415927410125732421875;
        float _21860 = interval_up(_21801, intervalFailed);
        Interval _21798 = Interval{ _21859, _21860 };
        Interval _21799 = Interval{ as_type<float>(as_type<uint>(_21850.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21850.lo) ^ 2147483648u) };
        Interval _21871 = iadd(_21798, _21799, intervalFailed);
        _21898 = _21871.hi;
        _21899 = _21871.lo;
    }
    else
    {
        float _21896;
        float _21897;
        if (_21850.hi < (-_21856.hi))
        {
            float _21796 = 3.1415927410125732421875;
            float _21875 = interval_down(_21796, intervalFailed);
            float _21797 = 3.1415927410125732421875;
            float _21876 = interval_up(_21797, intervalFailed);
            Interval _21794 = Interval{ as_type<float>(as_type<uint>(_21876) ^ 2147483648u), as_type<float>(as_type<uint>(_21875) ^ 2147483648u) };
            Interval _21795 = Interval{ as_type<float>(as_type<uint>(_21850.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21850.lo) ^ 2147483648u) };
            Interval _21893 = iadd(_21794, _21795, intervalFailed);
            _21896 = _21893.hi;
            _21897 = _21893.lo;
        }
        else
        {
            _21896 = _21850.hi;
            _21897 = _21850.lo;
        }
        _21898 = _21896;
        _21899 = _21897;
    }
    float _1810 = -_21856.hi;
    bool _21902;
    if ((isunordered(_21899, _1810) || _21899 >= _1810))
    {
        _21902 = _21898 > _21856.hi;
    }
    else
    {
        _21902 = true;
    }
    if (_21902)
    {
        return Interval{ -1.0, 1.0 };
    }
    bool _21907;
    if (_21899 <= 0.0)
    {
        _21907 = _21898 >= 0.0;
    }
    else
    {
        _21907 = false;
    }
    float _21914;
    if (_21907)
    {
        _21914 = 0.0;
    }
    else
    {
        _21914 = precise::min(abs(_21899), abs(_21898));
    }
    float _21917 = precise::max(abs(_21899), abs(_21898));
    float _21792 = spvFMul(_21914, _21914);
    float _21918 = interval_down(_21792, intervalFailed);
    float _21919 = precise::max(0.0, _21918);
    float _21793 = spvFMul(_21917, _21917);
    float _21920 = interval_up(_21793, intervalFailed);
    float _21790 = _2366[8];
    float _21923 = interval_down(_21790, intervalFailed);
    float _21791 = _2366[8];
    float _21924 = interval_up(_21791, intervalFailed);
    float _21925;
    float _21927;
    _21925 = _21923;
    _21927 = _21924;
    float _21926;
    float _21928;
    for (int _21929 = 7; _21929 >= 0; _21925 = _21926, _21927 = _21928, _21929--)
    {
        Interval param_var_a_2 = Interval{ _21925, _21927 };
        Interval param_var_b_2 = Interval{ _21919, _21920 };
        Interval _21933 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        Interval param_var_a_3 = _21933;
        float _21788 = _2366[_21929];
        float _21936 = interval_down(_21788, intervalFailed);
        float _21789 = _2366[_21929];
        float _21937 = interval_up(_21789, intervalFailed);
        Interval param_var_b_3 = Interval{ _21936, _21937 };
        Interval _21939 = iadd(param_var_a_3, param_var_b_3, intervalFailed);
        _21926 = _21939.lo;
        _21928 = _21939.hi;
    }
    Interval param_var_a_4 = Interval{ _21899, _21898 };
    Interval param_var_b_4 = Interval{ _21925, _21927 };
    Interval _21942 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _21942;
    Interval param_var_b_5 = Interval{ -3.9999999840167888010000751819462e-12, 3.9999999840167888010000751819462e-12 };
    Interval _21943 = iadd(param_var_a_5, param_var_b_5, intervalFailed);
    return Interval{ precise::min(precise::max(_21943.lo, -1.0), 1.0), precise::min(precise::max(_21943.hi, -1.0), 1.0) };
}

static __attribute__((noinline))
float sine_bounds(thread const float& lo, thread const float& hi, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_sine_upper)
{
    Interval param_var_a = Interval{ lo, hi };
    Interval _21785 = isin_body(param_var_a, intervalFailed, optical_product_upper);
    interval_sine_upper = _21785.hi;
    return _21785.lo;
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
    float _18610 = 6.0;
    float _18611 = 1000.0;
    Interval _18617 = iratio(_18610, _18611, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18622;
    if (!intervalFailed)
    {
        _18622 = intervalFailed;
    }
    else
    {
        _18622 = false;
    }
    bool _18627;
    if (_18622)
    {
        _18627 = jetFailureSite == 0u;
    }
    else
    {
        _18627 = false;
    }
    if (_18627)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6.0, 6.0, 1000.0, 1000.0);
    }
    Interval _18596 = x.v;
    Interval _18597 = _18617;
    Interval _18630 = imul(_18596, _18597, intervalFailed, optical_product_upper);
    Interval _18598 = x.dx;
    Interval _18599 = _18617;
    Interval _18631 = jet_mul_derivative(_18598, _18599, intervalFailed, optical_product_upper);
    Interval _18600 = _18631;
    Interval _18601 = x.v;
    Interval _18602 = Interval{ 0.0, 0.0 };
    Interval _18632 = jet_mul_derivative(_18601, _18602, intervalFailed, optical_product_upper);
    Interval _18603 = _18632;
    Interval _18633 = jet_add_derivative(_18600, _18603, intervalFailed);
    Interval _18604 = x.dy;
    Interval _18605 = _18617;
    Interval _18634 = jet_mul_derivative(_18604, _18605, intervalFailed, optical_product_upper);
    Interval _18606 = _18634;
    Interval _18607 = x.v;
    Interval _18608 = Interval{ 0.0, 0.0 };
    Interval _18635 = jet_mul_derivative(_18607, _18608, intervalFailed, optical_product_upper);
    Interval _18609 = _18635;
    Interval _18636 = jet_add_derivative(_18606, _18609, intervalFailed);
    float _18594 = 8.0;
    float _18595 = 1000.0;
    Interval _18642 = iratio(_18594, _18595, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18647;
    if (!intervalFailed)
    {
        _18647 = intervalFailed;
    }
    else
    {
        _18647 = false;
    }
    bool _18652;
    if (_18647)
    {
        _18652 = jetFailureSite == 0u;
    }
    else
    {
        _18652 = false;
    }
    if (_18652)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(8.0, 8.0, 1000.0, 1000.0);
    }
    Interval _18580 = z.v;
    Interval _18581 = _18642;
    Interval _18655 = imul(_18580, _18581, intervalFailed, optical_product_upper);
    Interval _18582 = z.dx;
    Interval _18583 = _18642;
    Interval _18656 = jet_mul_derivative(_18582, _18583, intervalFailed, optical_product_upper);
    Interval _18584 = _18656;
    Interval _18585 = z.v;
    Interval _18586 = Interval{ 0.0, 0.0 };
    Interval _18657 = jet_mul_derivative(_18585, _18586, intervalFailed, optical_product_upper);
    Interval _18587 = _18657;
    Interval _18658 = jet_add_derivative(_18584, _18587, intervalFailed);
    Interval _18588 = z.dy;
    Interval _18589 = _18642;
    Interval _18659 = jet_mul_derivative(_18588, _18589, intervalFailed, optical_product_upper);
    Interval _18590 = _18659;
    Interval _18591 = z.v;
    Interval _18592 = Interval{ 0.0, 0.0 };
    Interval _18660 = jet_mul_derivative(_18591, _18592, intervalFailed, optical_product_upper);
    Interval _18593 = _18660;
    Interval _18661 = jet_add_derivative(_18590, _18593, intervalFailed);
    Interval _18574 = _18630;
    Interval _18575 = Interval{ as_type<float>(as_type<uint>(_18655.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18655.lo) ^ 2147483648u) };
    Interval _18687 = iadd(_18574, _18575, intervalFailed);
    Interval _18576 = _18633;
    Interval _18577 = Interval{ as_type<float>(as_type<uint>(_18658.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18658.lo) ^ 2147483648u) };
    Interval _18689 = jet_add_derivative(_18576, _18577, intervalFailed);
    Interval _18578 = _18636;
    Interval _18579 = Interval{ as_type<float>(as_type<uint>(_18661.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18661.lo) ^ 2147483648u) };
    Interval _18691 = jet_add_derivative(_18578, _18579, intervalFailed);
    float _18572 = 11.0;
    float _18573 = 100.0;
    Interval _18697 = iratio(_18572, _18573, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18702;
    if (!intervalFailed)
    {
        _18702 = intervalFailed;
    }
    else
    {
        _18702 = false;
    }
    bool _18707;
    if (_18702)
    {
        _18707 = jetFailureSite == 0u;
    }
    else
    {
        _18707 = false;
    }
    if (_18707)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 100.0, 100.0);
    }
    Interval _18558 = t.v;
    Interval _18559 = _18697;
    Interval _18710 = imul(_18558, _18559, intervalFailed, optical_product_upper);
    Interval _18560 = t.dx;
    Interval _18561 = _18697;
    Interval _18711 = jet_mul_derivative(_18560, _18561, intervalFailed, optical_product_upper);
    Interval _18562 = _18711;
    Interval _18563 = t.v;
    Interval _18564 = Interval{ 0.0, 0.0 };
    Interval _18712 = jet_mul_derivative(_18563, _18564, intervalFailed, optical_product_upper);
    Interval _18565 = _18712;
    Interval _18713 = jet_add_derivative(_18562, _18565, intervalFailed);
    Interval _18566 = t.dy;
    Interval _18567 = _18697;
    Interval _18714 = jet_mul_derivative(_18566, _18567, intervalFailed, optical_product_upper);
    Interval _18568 = _18714;
    Interval _18569 = t.v;
    Interval _18570 = Interval{ 0.0, 0.0 };
    Interval _18715 = jet_mul_derivative(_18569, _18570, intervalFailed, optical_product_upper);
    Interval _18571 = _18715;
    Interval _18716 = jet_add_derivative(_18568, _18571, intervalFailed);
    Interval _18552 = _18687;
    Interval _18553 = _18710;
    Interval _18717 = iadd(_18552, _18553, intervalFailed);
    Interval _18554 = _18689;
    Interval _18555 = _18713;
    Interval _18718 = jet_add_derivative(_18554, _18555, intervalFailed);
    Interval _18556 = _18691;
    Interval _18557 = _18716;
    Interval _18719 = jet_add_derivative(_18556, _18557, intervalFailed);
    float _18550 = 10.0;
    float _18551 = 1000.0;
    Interval _18722 = iratio(_18550, _18551, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18727;
    if (!intervalFailed)
    {
        _18727 = intervalFailed;
    }
    else
    {
        _18727 = false;
    }
    bool _18732;
    if (_18727)
    {
        _18732 = jetFailureSite == 0u;
    }
    else
    {
        _18732 = false;
    }
    if (_18732)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(10.0, 10.0, 1000.0, 1000.0);
    }
    Interval _18536 = footprint.v;
    Interval _18537 = _18722;
    Interval _18738 = imul(_18536, _18537, intervalFailed, optical_product_upper);
    Interval _18538 = footprint.dx;
    Interval _18539 = _18722;
    Interval _18739 = jet_mul_derivative(_18538, _18539, intervalFailed, optical_product_upper);
    Interval _18540 = _18739;
    Interval _18541 = footprint.v;
    Interval _18542 = Interval{ 0.0, 0.0 };
    Interval _18740 = jet_mul_derivative(_18541, _18542, intervalFailed, optical_product_upper);
    Interval _18543 = _18740;
    Interval _18741 = jet_add_derivative(_18540, _18543, intervalFailed);
    Interval _18544 = footprint.dy;
    Interval _18545 = _18722;
    Interval _18742 = jet_mul_derivative(_18544, _18545, intervalFailed, optical_product_upper);
    Interval _18546 = _18742;
    Interval _18547 = footprint.v;
    Interval _18548 = Interval{ 0.0, 0.0 };
    Interval _18743 = jet_mul_derivative(_18547, _18548, intervalFailed, optical_product_upper);
    Interval _18549 = _18743;
    Interval _18744 = jet_add_derivative(_18546, _18549, intervalFailed);
    bool _18751;
    if (_18738.lo <= 0.0)
    {
        _18751 = _18738.hi >= 0.0;
    }
    else
    {
        _18751 = false;
    }
    float _18758;
    if (_18751)
    {
        _18758 = 0.0;
    }
    else
    {
        _18758 = precise::min(abs(_18738.lo), abs(_18738.hi));
    }
    float _18761 = precise::max(abs(_18738.lo), abs(_18738.hi));
    float _18526 = spvFMul(_18758, _18758);
    float _18762 = interval_down(_18526, intervalFailed);
    float _18763 = precise::max(0.0, _18762);
    float _18527 = spvFMul(_18761, _18761);
    float _18764 = interval_up(_18527, intervalFailed);
    Interval _18528 = Interval{ 2.0, 2.0 };
    Interval _18529 = _18738;
    Interval _18765 = imul(_18528, _18529, intervalFailed, optical_product_upper);
    Interval _18530 = _18765;
    Interval _18531 = _18741;
    Interval _18766 = jet_mul_derivative(_18530, _18531, intervalFailed, optical_product_upper);
    Interval _18532 = Interval{ 2.0, 2.0 };
    Interval _18533 = _18738;
    Interval _18767 = imul(_18532, _18533, intervalFailed, optical_product_upper);
    Interval _18534 = _18767;
    Interval _18535 = _18744;
    Interval _18768 = jet_mul_derivative(_18534, _18535, intervalFailed, optical_product_upper);
    bool _18773;
    if (_18763 <= 0.0)
    {
        _18773 = _18764 >= 0.0;
    }
    else
    {
        _18773 = false;
    }
    float _18780;
    if (_18773)
    {
        _18780 = 0.0;
    }
    else
    {
        _18780 = precise::min(abs(_18763), abs(_18764));
    }
    float _18783 = precise::max(abs(_18763), abs(_18764));
    float _18516 = spvFMul(_18780, _18780);
    float _18784 = interval_down(_18516, intervalFailed);
    float _18517 = spvFMul(_18783, _18783);
    float _18786 = interval_up(_18517, intervalFailed);
    Interval _18518 = Interval{ 2.0, 2.0 };
    Interval _18519 = Interval{ _18763, _18764 };
    Interval _18788 = imul(_18518, _18519, intervalFailed, optical_product_upper);
    Interval _18520 = _18788;
    Interval _18521 = _18766;
    Interval _18789 = jet_mul_derivative(_18520, _18521, intervalFailed, optical_product_upper);
    Interval _18522 = Interval{ 2.0, 2.0 };
    Interval _18523 = Interval{ _18763, _18764 };
    Interval _18791 = imul(_18522, _18523, intervalFailed, optical_product_upper);
    Interval _18524 = _18791;
    Interval _18525 = _18768;
    Interval _18792 = jet_mul_derivative(_18524, _18525, intervalFailed, optical_product_upper);
    Interval _18510 = Interval{ 1.0, 1.0 };
    Interval _18511 = Interval{ precise::max(0.0, _18784), _18786 };
    Interval _18794 = iadd(_18510, _18511, intervalFailed);
    Interval _18512 = Interval{ 0.0, 0.0 };
    Interval _18513 = _18789;
    Interval _18795 = jet_add_derivative(_18512, _18513, intervalFailed);
    Interval _18514 = Interval{ 0.0, 0.0 };
    Interval _18515 = _18792;
    Interval _18796 = jet_add_derivative(_18514, _18515, intervalFailed);
    Interval _18496 = Interval{ 1.0, 1.0 };
    Interval _18497 = _18794;
    Interval _18798 = idiv(_18496, _18497, intervalFailed, interval_divide_upper);
    bool _18805;
    if (!intervalFailed)
    {
        _18805 = intervalFailed;
    }
    else
    {
        _18805 = false;
    }
    bool _18810;
    if (_18805)
    {
        _18810 = jetFailureSite == 0u;
    }
    else
    {
        _18810 = false;
    }
    if (_18810)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _18794.lo, _18794.hi);
    }
    Interval _18498 = Interval{ 0.0, 0.0 };
    Interval _18499 = _18798;
    Interval _18500 = _18795;
    Interval _18814 = jet_mul_derivative(_18499, _18500, intervalFailed, optical_product_upper);
    Interval _18501 = Interval{ as_type<float>(as_type<uint>(_18814.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18814.lo) ^ 2147483648u) };
    Interval _18824 = jet_add_derivative(_18498, _18501, intervalFailed);
    Interval _18502 = _18824;
    Interval _18503 = _18794;
    Interval _18825 = jet_div_derivative(_18502, _18503, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18504 = Interval{ 0.0, 0.0 };
    Interval _18505 = _18798;
    Interval _18506 = _18796;
    Interval _18826 = jet_mul_derivative(_18505, _18506, intervalFailed, optical_product_upper);
    Interval _18507 = Interval{ as_type<float>(as_type<uint>(_18826.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18826.lo) ^ 2147483648u) };
    Interval _18836 = jet_add_derivative(_18504, _18507, intervalFailed);
    Interval _18508 = _18836;
    Interval _18509 = _18794;
    Interval _18837 = jet_div_derivative(_18508, _18509, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18494 = 18.0;
    float _18495 = 1000.0;
    Interval _18843 = iratio(_18494, _18495, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18848;
    if (!intervalFailed)
    {
        _18848 = intervalFailed;
    }
    else
    {
        _18848 = false;
    }
    bool _18853;
    if (_18848)
    {
        _18853 = jetFailureSite == 0u;
    }
    else
    {
        _18853 = false;
    }
    if (_18853)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    Interval _18480 = x.v;
    Interval _18481 = _18843;
    Interval _18856 = imul(_18480, _18481, intervalFailed, optical_product_upper);
    Interval _18482 = x.dx;
    Interval _18483 = _18843;
    Interval _18857 = jet_mul_derivative(_18482, _18483, intervalFailed, optical_product_upper);
    Interval _18484 = _18857;
    Interval _18485 = x.v;
    Interval _18486 = Interval{ 0.0, 0.0 };
    Interval _18858 = jet_mul_derivative(_18485, _18486, intervalFailed, optical_product_upper);
    Interval _18487 = _18858;
    Interval _18859 = jet_add_derivative(_18484, _18487, intervalFailed);
    Interval _18488 = x.dy;
    Interval _18489 = _18843;
    Interval _18860 = jet_mul_derivative(_18488, _18489, intervalFailed, optical_product_upper);
    Interval _18490 = _18860;
    Interval _18491 = x.v;
    Interval _18492 = Interval{ 0.0, 0.0 };
    Interval _18861 = jet_mul_derivative(_18491, _18492, intervalFailed, optical_product_upper);
    Interval _18493 = _18861;
    Interval _18862 = jet_add_derivative(_18490, _18493, intervalFailed);
    float _18478 = 11.0;
    float _18479 = 1000.0;
    Interval _18868 = iratio(_18478, _18479, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18873;
    if (!intervalFailed)
    {
        _18873 = intervalFailed;
    }
    else
    {
        _18873 = false;
    }
    bool _18878;
    if (_18873)
    {
        _18878 = jetFailureSite == 0u;
    }
    else
    {
        _18878 = false;
    }
    if (_18878)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    Interval _18464 = z.v;
    Interval _18465 = _18868;
    Interval _18881 = imul(_18464, _18465, intervalFailed, optical_product_upper);
    Interval _18466 = z.dx;
    Interval _18467 = _18868;
    Interval _18882 = jet_mul_derivative(_18466, _18467, intervalFailed, optical_product_upper);
    Interval _18468 = _18882;
    Interval _18469 = z.v;
    Interval _18470 = Interval{ 0.0, 0.0 };
    Interval _18883 = jet_mul_derivative(_18469, _18470, intervalFailed, optical_product_upper);
    Interval _18471 = _18883;
    Interval _18884 = jet_add_derivative(_18468, _18471, intervalFailed);
    Interval _18472 = z.dy;
    Interval _18473 = _18868;
    Interval _18885 = jet_mul_derivative(_18472, _18473, intervalFailed, optical_product_upper);
    Interval _18474 = _18885;
    Interval _18475 = z.v;
    Interval _18476 = Interval{ 0.0, 0.0 };
    Interval _18886 = jet_mul_derivative(_18475, _18476, intervalFailed, optical_product_upper);
    Interval _18477 = _18886;
    Interval _18887 = jet_add_derivative(_18474, _18477, intervalFailed);
    Interval _18458 = _18856;
    Interval _18459 = _18881;
    Interval _18888 = iadd(_18458, _18459, intervalFailed);
    Interval _18460 = _18859;
    Interval _18461 = _18884;
    Interval _18889 = jet_add_derivative(_18460, _18461, intervalFailed);
    Interval _18462 = _18862;
    Interval _18463 = _18887;
    Interval _18890 = jet_add_derivative(_18462, _18463, intervalFailed);
    float _18456 = 45.0;
    float _18457 = 100.0;
    Interval _18896 = iratio(_18456, _18457, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18901;
    if (!intervalFailed)
    {
        _18901 = intervalFailed;
    }
    else
    {
        _18901 = false;
    }
    bool _18906;
    if (_18901)
    {
        _18906 = jetFailureSite == 0u;
    }
    else
    {
        _18906 = false;
    }
    if (_18906)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 100.0, 100.0);
    }
    Interval _18442 = t.v;
    Interval _18443 = _18896;
    Interval _18909 = imul(_18442, _18443, intervalFailed, optical_product_upper);
    Interval _18444 = t.dx;
    Interval _18445 = _18896;
    Interval _18910 = jet_mul_derivative(_18444, _18445, intervalFailed, optical_product_upper);
    Interval _18446 = _18910;
    Interval _18447 = t.v;
    Interval _18448 = Interval{ 0.0, 0.0 };
    Interval _18911 = jet_mul_derivative(_18447, _18448, intervalFailed, optical_product_upper);
    Interval _18449 = _18911;
    Interval _18912 = jet_add_derivative(_18446, _18449, intervalFailed);
    Interval _18450 = t.dy;
    Interval _18451 = _18896;
    Interval _18913 = jet_mul_derivative(_18450, _18451, intervalFailed, optical_product_upper);
    Interval _18452 = _18913;
    Interval _18453 = t.v;
    Interval _18454 = Interval{ 0.0, 0.0 };
    Interval _18914 = jet_mul_derivative(_18453, _18454, intervalFailed, optical_product_upper);
    Interval _18455 = _18914;
    Interval _18915 = jet_add_derivative(_18452, _18455, intervalFailed);
    Interval _18436 = _18888;
    Interval _18437 = Interval{ as_type<float>(as_type<uint>(_18909.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18909.lo) ^ 2147483648u) };
    Interval _18941 = iadd(_18436, _18437, intervalFailed);
    Interval _18438 = _18889;
    Interval _18439 = Interval{ as_type<float>(as_type<uint>(_18912.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18912.lo) ^ 2147483648u) };
    Interval _18943 = jet_add_derivative(_18438, _18439, intervalFailed);
    Interval _18440 = _18890;
    Interval _18441 = Interval{ as_type<float>(as_type<uint>(_18915.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18915.lo) ^ 2147483648u) };
    Interval _18945 = jet_add_derivative(_18440, _18441, intervalFailed);
    float _18434 = 65.0;
    float _18435 = 100.0;
    Interval _18947 = iratio(_18434, _18435, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18952;
    if (!intervalFailed)
    {
        _18952 = intervalFailed;
    }
    else
    {
        _18952 = false;
    }
    bool _18957;
    if (_18952)
    {
        _18957 = jetFailureSite == 0u;
    }
    else
    {
        _18957 = false;
    }
    if (_18957)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
    }
    Interval _18426 = _18717;
    float _18424 = 3.1415927410125732421875;
    float _18960 = interval_down(_18424, intervalFailed);
    float _18425 = 3.1415927410125732421875;
    float _18961 = interval_up(_18425, intervalFailed);
    Interval _18427 = Interval{ _18960, _18961 };
    Interval _18428 = Interval{ 0.5, 0.5 };
    Interval _18963 = imul(_18427, _18428, intervalFailed, optical_product_upper);
    Interval _18429 = _18963;
    Interval _18964 = iadd(_18426, _18429, intervalFailed);
    float _18422 = _18964.lo;
    float _18423 = _18964.hi;
    float _18967 = sine_bounds(_18422, _18423, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18420 = _18717.lo;
    float _18421 = _18717.hi;
    float _18971 = sine_bounds(_18420, _18421, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18430 = Interval{ _18967, interval_sine_upper };
    Interval _18431 = _18718;
    Interval _18974 = jet_mul_derivative(_18430, _18431, intervalFailed, optical_product_upper);
    Interval _18432 = Interval{ _18967, interval_sine_upper };
    Interval _18433 = _18719;
    Interval _18976 = jet_mul_derivative(_18432, _18433, intervalFailed, optical_product_upper);
    Interval _18406 = _18947;
    Interval _18407 = Interval{ _18971, interval_sine_upper };
    Interval _18978 = imul(_18406, _18407, intervalFailed, optical_product_upper);
    Interval _18408 = Interval{ 0.0, 0.0 };
    Interval _18409 = Interval{ _18971, interval_sine_upper };
    Interval _18980 = jet_mul_derivative(_18408, _18409, intervalFailed, optical_product_upper);
    Interval _18410 = _18980;
    Interval _18411 = _18947;
    Interval _18412 = _18974;
    Interval _18981 = jet_mul_derivative(_18411, _18412, intervalFailed, optical_product_upper);
    Interval _18413 = _18981;
    Interval _18982 = jet_add_derivative(_18410, _18413, intervalFailed);
    Interval _18414 = Interval{ 0.0, 0.0 };
    Interval _18415 = Interval{ _18971, interval_sine_upper };
    Interval _18984 = jet_mul_derivative(_18414, _18415, intervalFailed, optical_product_upper);
    Interval _18416 = _18984;
    Interval _18417 = _18947;
    Interval _18418 = _18976;
    Interval _18985 = jet_mul_derivative(_18417, _18418, intervalFailed, optical_product_upper);
    Interval _18419 = _18985;
    Interval _18986 = jet_add_derivative(_18416, _18419, intervalFailed);
    Interval _18392 = _18978;
    Interval _18393 = _18798;
    Interval _18987 = imul(_18392, _18393, intervalFailed, optical_product_upper);
    Interval _18394 = _18982;
    Interval _18395 = _18798;
    Interval _18988 = jet_mul_derivative(_18394, _18395, intervalFailed, optical_product_upper);
    Interval _18396 = _18988;
    Interval _18397 = _18978;
    Interval _18398 = _18825;
    Interval _18989 = jet_mul_derivative(_18397, _18398, intervalFailed, optical_product_upper);
    Interval _18399 = _18989;
    Interval _18990 = jet_add_derivative(_18396, _18399, intervalFailed);
    Interval _18400 = _18986;
    Interval _18401 = _18798;
    Interval _18991 = jet_mul_derivative(_18400, _18401, intervalFailed, optical_product_upper);
    Interval _18402 = _18991;
    Interval _18403 = _18978;
    Interval _18404 = _18837;
    Interval _18992 = jet_mul_derivative(_18403, _18404, intervalFailed, optical_product_upper);
    Interval _18405 = _18992;
    Interval _18993 = jet_add_derivative(_18402, _18405, intervalFailed);
    Interval _18386 = _18941;
    Interval _18387 = _18987;
    Interval _18994 = iadd(_18386, _18387, intervalFailed);
    Interval _18388 = _18943;
    Interval _18389 = _18990;
    Interval _18995 = jet_add_derivative(_18388, _18389, intervalFailed);
    Interval _18390 = _18945;
    Interval _18391 = _18993;
    Interval _18996 = jet_add_derivative(_18390, _18391, intervalFailed);
    float _18384 = 47.0;
    float _18385 = 1000.0;
    Interval _19002 = iratio(_18384, _18385, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19007;
    if (!intervalFailed)
    {
        _19007 = intervalFailed;
    }
    else
    {
        _19007 = false;
    }
    bool _19012;
    if (_19007)
    {
        _19012 = jetFailureSite == 0u;
    }
    else
    {
        _19012 = false;
    }
    if (_19012)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
    }
    Interval _18370 = x.v;
    Interval _18371 = _19002;
    Interval _19015 = imul(_18370, _18371, intervalFailed, optical_product_upper);
    Interval _18372 = x.dx;
    Interval _18373 = _19002;
    Interval _19016 = jet_mul_derivative(_18372, _18373, intervalFailed, optical_product_upper);
    Interval _18374 = _19016;
    Interval _18375 = x.v;
    Interval _18376 = Interval{ 0.0, 0.0 };
    Interval _19017 = jet_mul_derivative(_18375, _18376, intervalFailed, optical_product_upper);
    Interval _18377 = _19017;
    Interval _19018 = jet_add_derivative(_18374, _18377, intervalFailed);
    Interval _18378 = x.dy;
    Interval _18379 = _19002;
    Interval _19019 = jet_mul_derivative(_18378, _18379, intervalFailed, optical_product_upper);
    Interval _18380 = _19019;
    Interval _18381 = x.v;
    Interval _18382 = Interval{ 0.0, 0.0 };
    Interval _19020 = jet_mul_derivative(_18381, _18382, intervalFailed, optical_product_upper);
    Interval _18383 = _19020;
    Interval _19021 = jet_add_derivative(_18380, _18383, intervalFailed);
    float _18368 = 25.0;
    float _18369 = 1000.0;
    Interval _19027 = iratio(_18368, _18369, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19032;
    if (!intervalFailed)
    {
        _19032 = intervalFailed;
    }
    else
    {
        _19032 = false;
    }
    bool _19037;
    if (_19032)
    {
        _19037 = jetFailureSite == 0u;
    }
    else
    {
        _19037 = false;
    }
    if (_19037)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
    }
    Interval _18354 = z.v;
    Interval _18355 = _19027;
    Interval _19040 = imul(_18354, _18355, intervalFailed, optical_product_upper);
    Interval _18356 = z.dx;
    Interval _18357 = _19027;
    Interval _19041 = jet_mul_derivative(_18356, _18357, intervalFailed, optical_product_upper);
    Interval _18358 = _19041;
    Interval _18359 = z.v;
    Interval _18360 = Interval{ 0.0, 0.0 };
    Interval _19042 = jet_mul_derivative(_18359, _18360, intervalFailed, optical_product_upper);
    Interval _18361 = _19042;
    Interval _19043 = jet_add_derivative(_18358, _18361, intervalFailed);
    Interval _18362 = z.dy;
    Interval _18363 = _19027;
    Interval _19044 = jet_mul_derivative(_18362, _18363, intervalFailed, optical_product_upper);
    Interval _18364 = _19044;
    Interval _18365 = z.v;
    Interval _18366 = Interval{ 0.0, 0.0 };
    Interval _19045 = jet_mul_derivative(_18365, _18366, intervalFailed, optical_product_upper);
    Interval _18367 = _19045;
    Interval _19046 = jet_add_derivative(_18364, _18367, intervalFailed);
    Interval _18348 = _19015;
    Interval _18349 = Interval{ as_type<float>(as_type<uint>(_19040.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19040.lo) ^ 2147483648u) };
    Interval _19072 = iadd(_18348, _18349, intervalFailed);
    Interval _18350 = _19018;
    Interval _18351 = Interval{ as_type<float>(as_type<uint>(_19043.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19043.lo) ^ 2147483648u) };
    Interval _19074 = jet_add_derivative(_18350, _18351, intervalFailed);
    Interval _18352 = _19021;
    Interval _18353 = Interval{ as_type<float>(as_type<uint>(_19046.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19046.lo) ^ 2147483648u) };
    Interval _19076 = jet_add_derivative(_18352, _18353, intervalFailed);
    float _18346 = 60.0;
    float _18347 = 100.0;
    Interval _19082 = iratio(_18346, _18347, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19087;
    if (!intervalFailed)
    {
        _19087 = intervalFailed;
    }
    else
    {
        _19087 = false;
    }
    bool _19092;
    if (_19087)
    {
        _19092 = jetFailureSite == 0u;
    }
    else
    {
        _19092 = false;
    }
    if (_19092)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(60.0, 60.0, 100.0, 100.0);
    }
    Interval _18332 = t.v;
    Interval _18333 = _19082;
    Interval _19095 = imul(_18332, _18333, intervalFailed, optical_product_upper);
    Interval _18334 = t.dx;
    Interval _18335 = _19082;
    Interval _19096 = jet_mul_derivative(_18334, _18335, intervalFailed, optical_product_upper);
    Interval _18336 = _19096;
    Interval _18337 = t.v;
    Interval _18338 = Interval{ 0.0, 0.0 };
    Interval _19097 = jet_mul_derivative(_18337, _18338, intervalFailed, optical_product_upper);
    Interval _18339 = _19097;
    Interval _19098 = jet_add_derivative(_18336, _18339, intervalFailed);
    Interval _18340 = t.dy;
    Interval _18341 = _19082;
    Interval _19099 = jet_mul_derivative(_18340, _18341, intervalFailed, optical_product_upper);
    Interval _18342 = _19099;
    Interval _18343 = t.v;
    Interval _18344 = Interval{ 0.0, 0.0 };
    Interval _19100 = jet_mul_derivative(_18343, _18344, intervalFailed, optical_product_upper);
    Interval _18345 = _19100;
    Interval _19101 = jet_add_derivative(_18342, _18345, intervalFailed);
    Interval _18326 = _19072;
    Interval _18327 = _19095;
    Interval _19102 = iadd(_18326, _18327, intervalFailed);
    Interval _18328 = _19074;
    Interval _18329 = _19098;
    Interval _19103 = jet_add_derivative(_18328, _18329, intervalFailed);
    Interval _18330 = _19076;
    Interval _18331 = _19101;
    Interval _19104 = jet_add_derivative(_18330, _18331, intervalFailed);
    float _18324 = 22.0;
    float _18325 = 1000.0;
    Interval _19110 = iratio(_18324, _18325, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19115;
    if (!intervalFailed)
    {
        _19115 = intervalFailed;
    }
    else
    {
        _19115 = false;
    }
    bool _19120;
    if (_19115)
    {
        _19120 = jetFailureSite == 0u;
    }
    else
    {
        _19120 = false;
    }
    if (_19120)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18310 = z.v;
    Interval _18311 = _19110;
    Interval _19123 = imul(_18310, _18311, intervalFailed, optical_product_upper);
    Interval _18312 = z.dx;
    Interval _18313 = _19110;
    Interval _19124 = jet_mul_derivative(_18312, _18313, intervalFailed, optical_product_upper);
    Interval _18314 = _19124;
    Interval _18315 = z.v;
    Interval _18316 = Interval{ 0.0, 0.0 };
    Interval _19125 = jet_mul_derivative(_18315, _18316, intervalFailed, optical_product_upper);
    Interval _18317 = _19125;
    Interval _19126 = jet_add_derivative(_18314, _18317, intervalFailed);
    Interval _18318 = z.dy;
    Interval _18319 = _19110;
    Interval _19127 = jet_mul_derivative(_18318, _18319, intervalFailed, optical_product_upper);
    Interval _18320 = _19127;
    Interval _18321 = z.v;
    Interval _18322 = Interval{ 0.0, 0.0 };
    Interval _19128 = jet_mul_derivative(_18321, _18322, intervalFailed, optical_product_upper);
    Interval _18323 = _19128;
    Interval _19129 = jet_add_derivative(_18320, _18323, intervalFailed);
    float _18308 = 9.0;
    float _18309 = 1000.0;
    Interval _19135 = iratio(_18308, _18309, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19140;
    if (!intervalFailed)
    {
        _19140 = intervalFailed;
    }
    else
    {
        _19140 = false;
    }
    bool _19145;
    if (_19140)
    {
        _19145 = jetFailureSite == 0u;
    }
    else
    {
        _19145 = false;
    }
    if (_19145)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
    }
    Interval _18294 = x.v;
    Interval _18295 = _19135;
    Interval _19148 = imul(_18294, _18295, intervalFailed, optical_product_upper);
    Interval _18296 = x.dx;
    Interval _18297 = _19135;
    Interval _19149 = jet_mul_derivative(_18296, _18297, intervalFailed, optical_product_upper);
    Interval _18298 = _19149;
    Interval _18299 = x.v;
    Interval _18300 = Interval{ 0.0, 0.0 };
    Interval _19150 = jet_mul_derivative(_18299, _18300, intervalFailed, optical_product_upper);
    Interval _18301 = _19150;
    Interval _19151 = jet_add_derivative(_18298, _18301, intervalFailed);
    Interval _18302 = x.dy;
    Interval _18303 = _19135;
    Interval _19152 = jet_mul_derivative(_18302, _18303, intervalFailed, optical_product_upper);
    Interval _18304 = _19152;
    Interval _18305 = x.v;
    Interval _18306 = Interval{ 0.0, 0.0 };
    Interval _19153 = jet_mul_derivative(_18305, _18306, intervalFailed, optical_product_upper);
    Interval _18307 = _19153;
    Interval _19154 = jet_add_derivative(_18304, _18307, intervalFailed);
    Interval _18288 = _19123;
    Interval _18289 = Interval{ as_type<float>(as_type<uint>(_19148.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19148.lo) ^ 2147483648u) };
    Interval _19180 = iadd(_18288, _18289, intervalFailed);
    Interval _18290 = _19126;
    Interval _18291 = Interval{ as_type<float>(as_type<uint>(_19151.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19151.lo) ^ 2147483648u) };
    Interval _19182 = jet_add_derivative(_18290, _18291, intervalFailed);
    Interval _18292 = _19129;
    Interval _18293 = Interval{ as_type<float>(as_type<uint>(_19154.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19154.lo) ^ 2147483648u) };
    Interval _19184 = jet_add_derivative(_18292, _18293, intervalFailed);
    float _18286 = 32.0;
    float _18287 = 100.0;
    Interval _19190 = iratio(_18286, _18287, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19195;
    if (!intervalFailed)
    {
        _19195 = intervalFailed;
    }
    else
    {
        _19195 = false;
    }
    bool _19200;
    if (_19195)
    {
        _19200 = jetFailureSite == 0u;
    }
    else
    {
        _19200 = false;
    }
    if (_19200)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(32.0, 32.0, 100.0, 100.0);
    }
    Interval _18272 = t.v;
    Interval _18273 = _19190;
    Interval _19203 = imul(_18272, _18273, intervalFailed, optical_product_upper);
    Interval _18274 = t.dx;
    Interval _18275 = _19190;
    Interval _19204 = jet_mul_derivative(_18274, _18275, intervalFailed, optical_product_upper);
    Interval _18276 = _19204;
    Interval _18277 = t.v;
    Interval _18278 = Interval{ 0.0, 0.0 };
    Interval _19205 = jet_mul_derivative(_18277, _18278, intervalFailed, optical_product_upper);
    Interval _18279 = _19205;
    Interval _19206 = jet_add_derivative(_18276, _18279, intervalFailed);
    Interval _18280 = t.dy;
    Interval _18281 = _19190;
    Interval _19207 = jet_mul_derivative(_18280, _18281, intervalFailed, optical_product_upper);
    Interval _18282 = _19207;
    Interval _18283 = t.v;
    Interval _18284 = Interval{ 0.0, 0.0 };
    Interval _19208 = jet_mul_derivative(_18283, _18284, intervalFailed, optical_product_upper);
    Interval _18285 = _19208;
    Interval _19209 = jet_add_derivative(_18282, _18285, intervalFailed);
    Interval _18266 = _19180;
    Interval _18267 = Interval{ as_type<float>(as_type<uint>(_19203.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19203.lo) ^ 2147483648u) };
    Interval _19235 = iadd(_18266, _18267, intervalFailed);
    Interval _18268 = _19182;
    Interval _18269 = Interval{ as_type<float>(as_type<uint>(_19206.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19206.lo) ^ 2147483648u) };
    Interval _19237 = jet_add_derivative(_18268, _18269, intervalFailed);
    Interval _18270 = _19184;
    Interval _18271 = Interval{ as_type<float>(as_type<uint>(_19209.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19209.lo) ^ 2147483648u) };
    Interval _19239 = jet_add_derivative(_18270, _18271, intervalFailed);
    float _18264 = 22.0;
    float _18265 = 1000.0;
    Interval _19242 = iratio(_18264, _18265, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19247;
    if (!intervalFailed)
    {
        _19247 = intervalFailed;
    }
    else
    {
        _19247 = false;
    }
    bool _19252;
    if (_19247)
    {
        _19252 = jetFailureSite == 0u;
    }
    else
    {
        _19252 = false;
    }
    if (_19252)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18250 = footprint.v;
    Interval _18251 = _19242;
    Interval _19258 = imul(_18250, _18251, intervalFailed, optical_product_upper);
    Interval _18252 = footprint.dx;
    Interval _18253 = _19242;
    Interval _19259 = jet_mul_derivative(_18252, _18253, intervalFailed, optical_product_upper);
    Interval _18254 = _19259;
    Interval _18255 = footprint.v;
    Interval _18256 = Interval{ 0.0, 0.0 };
    Interval _19260 = jet_mul_derivative(_18255, _18256, intervalFailed, optical_product_upper);
    Interval _18257 = _19260;
    Interval _19261 = jet_add_derivative(_18254, _18257, intervalFailed);
    Interval _18258 = footprint.dy;
    Interval _18259 = _19242;
    Interval _19262 = jet_mul_derivative(_18258, _18259, intervalFailed, optical_product_upper);
    Interval _18260 = _19262;
    Interval _18261 = footprint.v;
    Interval _18262 = Interval{ 0.0, 0.0 };
    Interval _19263 = jet_mul_derivative(_18261, _18262, intervalFailed, optical_product_upper);
    Interval _18263 = _19263;
    Interval _19264 = jet_add_derivative(_18260, _18263, intervalFailed);
    bool _19271;
    if (_19258.lo <= 0.0)
    {
        _19271 = _19258.hi >= 0.0;
    }
    else
    {
        _19271 = false;
    }
    float _19278;
    if (_19271)
    {
        _19278 = 0.0;
    }
    else
    {
        _19278 = precise::min(abs(_19258.lo), abs(_19258.hi));
    }
    float _19281 = precise::max(abs(_19258.lo), abs(_19258.hi));
    float _18240 = spvFMul(_19278, _19278);
    float _19282 = interval_down(_18240, intervalFailed);
    float _19283 = precise::max(0.0, _19282);
    float _18241 = spvFMul(_19281, _19281);
    float _19284 = interval_up(_18241, intervalFailed);
    Interval _18242 = Interval{ 2.0, 2.0 };
    Interval _18243 = _19258;
    Interval _19285 = imul(_18242, _18243, intervalFailed, optical_product_upper);
    Interval _18244 = _19285;
    Interval _18245 = _19261;
    Interval _19286 = jet_mul_derivative(_18244, _18245, intervalFailed, optical_product_upper);
    Interval _18246 = Interval{ 2.0, 2.0 };
    Interval _18247 = _19258;
    Interval _19287 = imul(_18246, _18247, intervalFailed, optical_product_upper);
    Interval _18248 = _19287;
    Interval _18249 = _19264;
    Interval _19288 = jet_mul_derivative(_18248, _18249, intervalFailed, optical_product_upper);
    bool _19293;
    if (_19283 <= 0.0)
    {
        _19293 = _19284 >= 0.0;
    }
    else
    {
        _19293 = false;
    }
    float _19300;
    if (_19293)
    {
        _19300 = 0.0;
    }
    else
    {
        _19300 = precise::min(abs(_19283), abs(_19284));
    }
    float _19303 = precise::max(abs(_19283), abs(_19284));
    float _18230 = spvFMul(_19300, _19300);
    float _19304 = interval_down(_18230, intervalFailed);
    float _18231 = spvFMul(_19303, _19303);
    float _19306 = interval_up(_18231, intervalFailed);
    Interval _18232 = Interval{ 2.0, 2.0 };
    Interval _18233 = Interval{ _19283, _19284 };
    Interval _19308 = imul(_18232, _18233, intervalFailed, optical_product_upper);
    Interval _18234 = _19308;
    Interval _18235 = _19286;
    Interval _19309 = jet_mul_derivative(_18234, _18235, intervalFailed, optical_product_upper);
    Interval _18236 = Interval{ 2.0, 2.0 };
    Interval _18237 = Interval{ _19283, _19284 };
    Interval _19311 = imul(_18236, _18237, intervalFailed, optical_product_upper);
    Interval _18238 = _19311;
    Interval _18239 = _19288;
    Interval _19312 = jet_mul_derivative(_18238, _18239, intervalFailed, optical_product_upper);
    Interval _18224 = Interval{ 1.0, 1.0 };
    Interval _18225 = Interval{ precise::max(0.0, _19304), _19306 };
    Interval _19314 = iadd(_18224, _18225, intervalFailed);
    Interval _18226 = Interval{ 0.0, 0.0 };
    Interval _18227 = _19309;
    Interval _19315 = jet_add_derivative(_18226, _18227, intervalFailed);
    Interval _18228 = Interval{ 0.0, 0.0 };
    Interval _18229 = _19312;
    Interval _19316 = jet_add_derivative(_18228, _18229, intervalFailed);
    Interval _18210 = Interval{ 1.0, 1.0 };
    Interval _18211 = _19314;
    Interval _19318 = idiv(_18210, _18211, intervalFailed, interval_divide_upper);
    bool _19325;
    if (!intervalFailed)
    {
        _19325 = intervalFailed;
    }
    else
    {
        _19325 = false;
    }
    bool _19330;
    if (_19325)
    {
        _19330 = jetFailureSite == 0u;
    }
    else
    {
        _19330 = false;
    }
    if (_19330)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19314.lo, _19314.hi);
    }
    Interval _18212 = Interval{ 0.0, 0.0 };
    Interval _18213 = _19318;
    Interval _18214 = _19315;
    Interval _19334 = jet_mul_derivative(_18213, _18214, intervalFailed, optical_product_upper);
    Interval _18215 = Interval{ as_type<float>(as_type<uint>(_19334.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19334.lo) ^ 2147483648u) };
    Interval _19344 = jet_add_derivative(_18212, _18215, intervalFailed);
    Interval _18216 = _19344;
    Interval _18217 = _19314;
    Interval _19345 = jet_div_derivative(_18216, _18217, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18218 = Interval{ 0.0, 0.0 };
    Interval _18219 = _19318;
    Interval _18220 = _19316;
    Interval _19346 = jet_mul_derivative(_18219, _18220, intervalFailed, optical_product_upper);
    Interval _18221 = Interval{ as_type<float>(as_type<uint>(_19346.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19346.lo) ^ 2147483648u) };
    Interval _19356 = jet_add_derivative(_18218, _18221, intervalFailed);
    Interval _18222 = _19356;
    Interval _18223 = _19314;
    Interval _19357 = jet_div_derivative(_18222, _18223, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18208 = 54.0;
    float _18209 = 1000.0;
    Interval _19360 = iratio(_18208, _18209, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19365;
    if (!intervalFailed)
    {
        _19365 = intervalFailed;
    }
    else
    {
        _19365 = false;
    }
    bool _19370;
    if (_19365)
    {
        _19370 = jetFailureSite == 0u;
    }
    else
    {
        _19370 = false;
    }
    if (_19370)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
    }
    Interval _18194 = footprint.v;
    Interval _18195 = _19360;
    Interval _19376 = imul(_18194, _18195, intervalFailed, optical_product_upper);
    Interval _18196 = footprint.dx;
    Interval _18197 = _19360;
    Interval _19377 = jet_mul_derivative(_18196, _18197, intervalFailed, optical_product_upper);
    Interval _18198 = _19377;
    Interval _18199 = footprint.v;
    Interval _18200 = Interval{ 0.0, 0.0 };
    Interval _19378 = jet_mul_derivative(_18199, _18200, intervalFailed, optical_product_upper);
    Interval _18201 = _19378;
    Interval _19379 = jet_add_derivative(_18198, _18201, intervalFailed);
    Interval _18202 = footprint.dy;
    Interval _18203 = _19360;
    Interval _19380 = jet_mul_derivative(_18202, _18203, intervalFailed, optical_product_upper);
    Interval _18204 = _19380;
    Interval _18205 = footprint.v;
    Interval _18206 = Interval{ 0.0, 0.0 };
    Interval _19381 = jet_mul_derivative(_18205, _18206, intervalFailed, optical_product_upper);
    Interval _18207 = _19381;
    Interval _19382 = jet_add_derivative(_18204, _18207, intervalFailed);
    bool _19389;
    if (_19376.lo <= 0.0)
    {
        _19389 = _19376.hi >= 0.0;
    }
    else
    {
        _19389 = false;
    }
    float _19396;
    if (_19389)
    {
        _19396 = 0.0;
    }
    else
    {
        _19396 = precise::min(abs(_19376.lo), abs(_19376.hi));
    }
    float _19399 = precise::max(abs(_19376.lo), abs(_19376.hi));
    float _18184 = spvFMul(_19396, _19396);
    float _19400 = interval_down(_18184, intervalFailed);
    float _19401 = precise::max(0.0, _19400);
    float _18185 = spvFMul(_19399, _19399);
    float _19402 = interval_up(_18185, intervalFailed);
    Interval _18186 = Interval{ 2.0, 2.0 };
    Interval _18187 = _19376;
    Interval _19403 = imul(_18186, _18187, intervalFailed, optical_product_upper);
    Interval _18188 = _19403;
    Interval _18189 = _19379;
    Interval _19404 = jet_mul_derivative(_18188, _18189, intervalFailed, optical_product_upper);
    Interval _18190 = Interval{ 2.0, 2.0 };
    Interval _18191 = _19376;
    Interval _19405 = imul(_18190, _18191, intervalFailed, optical_product_upper);
    Interval _18192 = _19405;
    Interval _18193 = _19382;
    Interval _19406 = jet_mul_derivative(_18192, _18193, intervalFailed, optical_product_upper);
    bool _19411;
    if (_19401 <= 0.0)
    {
        _19411 = _19402 >= 0.0;
    }
    else
    {
        _19411 = false;
    }
    float _19418;
    if (_19411)
    {
        _19418 = 0.0;
    }
    else
    {
        _19418 = precise::min(abs(_19401), abs(_19402));
    }
    float _19421 = precise::max(abs(_19401), abs(_19402));
    float _18174 = spvFMul(_19418, _19418);
    float _19422 = interval_down(_18174, intervalFailed);
    float _18175 = spvFMul(_19421, _19421);
    float _19424 = interval_up(_18175, intervalFailed);
    Interval _18176 = Interval{ 2.0, 2.0 };
    Interval _18177 = Interval{ _19401, _19402 };
    Interval _19426 = imul(_18176, _18177, intervalFailed, optical_product_upper);
    Interval _18178 = _19426;
    Interval _18179 = _19404;
    Interval _19427 = jet_mul_derivative(_18178, _18179, intervalFailed, optical_product_upper);
    Interval _18180 = Interval{ 2.0, 2.0 };
    Interval _18181 = Interval{ _19401, _19402 };
    Interval _19429 = imul(_18180, _18181, intervalFailed, optical_product_upper);
    Interval _18182 = _19429;
    Interval _18183 = _19406;
    Interval _19430 = jet_mul_derivative(_18182, _18183, intervalFailed, optical_product_upper);
    Interval _18168 = Interval{ 1.0, 1.0 };
    Interval _18169 = Interval{ precise::max(0.0, _19422), _19424 };
    Interval _19432 = iadd(_18168, _18169, intervalFailed);
    Interval _18170 = Interval{ 0.0, 0.0 };
    Interval _18171 = _19427;
    Interval _19433 = jet_add_derivative(_18170, _18171, intervalFailed);
    Interval _18172 = Interval{ 0.0, 0.0 };
    Interval _18173 = _19430;
    Interval _19434 = jet_add_derivative(_18172, _18173, intervalFailed);
    Interval _18154 = Interval{ 1.0, 1.0 };
    Interval _18155 = _19432;
    Interval _19436 = idiv(_18154, _18155, intervalFailed, interval_divide_upper);
    bool _19443;
    if (!intervalFailed)
    {
        _19443 = intervalFailed;
    }
    else
    {
        _19443 = false;
    }
    bool _19448;
    if (_19443)
    {
        _19448 = jetFailureSite == 0u;
    }
    else
    {
        _19448 = false;
    }
    if (_19448)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19432.lo, _19432.hi);
    }
    Interval _18156 = Interval{ 0.0, 0.0 };
    Interval _18157 = _19436;
    Interval _18158 = _19433;
    Interval _19452 = jet_mul_derivative(_18157, _18158, intervalFailed, optical_product_upper);
    Interval _18159 = Interval{ as_type<float>(as_type<uint>(_19452.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19452.lo) ^ 2147483648u) };
    Interval _19462 = jet_add_derivative(_18156, _18159, intervalFailed);
    Interval _18160 = _19462;
    Interval _18161 = _19432;
    Interval _19463 = jet_div_derivative(_18160, _18161, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18162 = Interval{ 0.0, 0.0 };
    Interval _18163 = _19436;
    Interval _18164 = _19434;
    Interval _19464 = jet_mul_derivative(_18163, _18164, intervalFailed, optical_product_upper);
    Interval _18165 = Interval{ as_type<float>(as_type<uint>(_19464.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19464.lo) ^ 2147483648u) };
    Interval _19474 = jet_add_derivative(_18162, _18165, intervalFailed);
    Interval _18166 = _19474;
    Interval _18167 = _19432;
    Interval _19475 = jet_div_derivative(_18166, _18167, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18152 = 24.0;
    float _18153 = 1000.0;
    Interval _19478 = iratio(_18152, _18153, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19483;
    if (!intervalFailed)
    {
        _19483 = intervalFailed;
    }
    else
    {
        _19483 = false;
    }
    bool _19488;
    if (_19483)
    {
        _19488 = jetFailureSite == 0u;
    }
    else
    {
        _19488 = false;
    }
    if (_19488)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
    }
    Interval _18138 = footprint.v;
    Interval _18139 = _19478;
    Interval _19494 = imul(_18138, _18139, intervalFailed, optical_product_upper);
    Interval _18140 = footprint.dx;
    Interval _18141 = _19478;
    Interval _19495 = jet_mul_derivative(_18140, _18141, intervalFailed, optical_product_upper);
    Interval _18142 = _19495;
    Interval _18143 = footprint.v;
    Interval _18144 = Interval{ 0.0, 0.0 };
    Interval _19496 = jet_mul_derivative(_18143, _18144, intervalFailed, optical_product_upper);
    Interval _18145 = _19496;
    Interval _19497 = jet_add_derivative(_18142, _18145, intervalFailed);
    Interval _18146 = footprint.dy;
    Interval _18147 = _19478;
    Interval _19498 = jet_mul_derivative(_18146, _18147, intervalFailed, optical_product_upper);
    Interval _18148 = _19498;
    Interval _18149 = footprint.v;
    Interval _18150 = Interval{ 0.0, 0.0 };
    Interval _19499 = jet_mul_derivative(_18149, _18150, intervalFailed, optical_product_upper);
    Interval _18151 = _19499;
    Interval _19500 = jet_add_derivative(_18148, _18151, intervalFailed);
    bool _19507;
    if (_19494.lo <= 0.0)
    {
        _19507 = _19494.hi >= 0.0;
    }
    else
    {
        _19507 = false;
    }
    float _19514;
    if (_19507)
    {
        _19514 = 0.0;
    }
    else
    {
        _19514 = precise::min(abs(_19494.lo), abs(_19494.hi));
    }
    float _19517 = precise::max(abs(_19494.lo), abs(_19494.hi));
    float _18128 = spvFMul(_19514, _19514);
    float _19518 = interval_down(_18128, intervalFailed);
    float _19519 = precise::max(0.0, _19518);
    float _18129 = spvFMul(_19517, _19517);
    float _19520 = interval_up(_18129, intervalFailed);
    Interval _18130 = Interval{ 2.0, 2.0 };
    Interval _18131 = _19494;
    Interval _19521 = imul(_18130, _18131, intervalFailed, optical_product_upper);
    Interval _18132 = _19521;
    Interval _18133 = _19497;
    Interval _19522 = jet_mul_derivative(_18132, _18133, intervalFailed, optical_product_upper);
    Interval _18134 = Interval{ 2.0, 2.0 };
    Interval _18135 = _19494;
    Interval _19523 = imul(_18134, _18135, intervalFailed, optical_product_upper);
    Interval _18136 = _19523;
    Interval _18137 = _19500;
    Interval _19524 = jet_mul_derivative(_18136, _18137, intervalFailed, optical_product_upper);
    bool _19529;
    if (_19519 <= 0.0)
    {
        _19529 = _19520 >= 0.0;
    }
    else
    {
        _19529 = false;
    }
    float _19536;
    if (_19529)
    {
        _19536 = 0.0;
    }
    else
    {
        _19536 = precise::min(abs(_19519), abs(_19520));
    }
    float _19539 = precise::max(abs(_19519), abs(_19520));
    float _18118 = spvFMul(_19536, _19536);
    float _19540 = interval_down(_18118, intervalFailed);
    float _18119 = spvFMul(_19539, _19539);
    float _19542 = interval_up(_18119, intervalFailed);
    Interval _18120 = Interval{ 2.0, 2.0 };
    Interval _18121 = Interval{ _19519, _19520 };
    Interval _19544 = imul(_18120, _18121, intervalFailed, optical_product_upper);
    Interval _18122 = _19544;
    Interval _18123 = _19522;
    Interval _19545 = jet_mul_derivative(_18122, _18123, intervalFailed, optical_product_upper);
    Interval _18124 = Interval{ 2.0, 2.0 };
    Interval _18125 = Interval{ _19519, _19520 };
    Interval _19547 = imul(_18124, _18125, intervalFailed, optical_product_upper);
    Interval _18126 = _19547;
    Interval _18127 = _19524;
    Interval _19548 = jet_mul_derivative(_18126, _18127, intervalFailed, optical_product_upper);
    Interval _18112 = Interval{ 1.0, 1.0 };
    Interval _18113 = Interval{ precise::max(0.0, _19540), _19542 };
    Interval _19550 = iadd(_18112, _18113, intervalFailed);
    Interval _18114 = Interval{ 0.0, 0.0 };
    Interval _18115 = _19545;
    Interval _19551 = jet_add_derivative(_18114, _18115, intervalFailed);
    Interval _18116 = Interval{ 0.0, 0.0 };
    Interval _18117 = _19548;
    Interval _19552 = jet_add_derivative(_18116, _18117, intervalFailed);
    Interval _18098 = Interval{ 1.0, 1.0 };
    Interval _18099 = _19550;
    Interval _19554 = idiv(_18098, _18099, intervalFailed, interval_divide_upper);
    bool _19561;
    if (!intervalFailed)
    {
        _19561 = intervalFailed;
    }
    else
    {
        _19561 = false;
    }
    bool _19566;
    if (_19561)
    {
        _19566 = jetFailureSite == 0u;
    }
    else
    {
        _19566 = false;
    }
    if (_19566)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19550.lo, _19550.hi);
    }
    Interval _18100 = Interval{ 0.0, 0.0 };
    Interval _18101 = _19554;
    Interval _18102 = _19551;
    Interval _19570 = jet_mul_derivative(_18101, _18102, intervalFailed, optical_product_upper);
    Interval _18103 = Interval{ as_type<float>(as_type<uint>(_19570.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19570.lo) ^ 2147483648u) };
    Interval _19580 = jet_add_derivative(_18100, _18103, intervalFailed);
    Interval _18104 = _19580;
    Interval _18105 = _19550;
    Interval _19581 = jet_div_derivative(_18104, _18105, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18106 = Interval{ 0.0, 0.0 };
    Interval _18107 = _19554;
    Interval _18108 = _19552;
    Interval _19582 = jet_mul_derivative(_18107, _18108, intervalFailed, optical_product_upper);
    Interval _18109 = Interval{ as_type<float>(as_type<uint>(_19582.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19582.lo) ^ 2147483648u) };
    Interval _19592 = jet_add_derivative(_18106, _18109, intervalFailed);
    Interval _18110 = _19592;
    Interval _18111 = _19550;
    Interval _19593 = jet_div_derivative(_18110, _18111, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18090 = _18994;
    float _18088 = 3.1415927410125732421875;
    float _19594 = interval_down(_18088, intervalFailed);
    float _18089 = 3.1415927410125732421875;
    float _19595 = interval_up(_18089, intervalFailed);
    Interval _18091 = Interval{ _19594, _19595 };
    Interval _18092 = Interval{ 0.5, 0.5 };
    Interval _19597 = imul(_18091, _18092, intervalFailed, optical_product_upper);
    Interval _18093 = _19597;
    Interval _19598 = iadd(_18090, _18093, intervalFailed);
    float _18086 = _19598.lo;
    float _18087 = _19598.hi;
    float _19601 = sine_bounds(_18086, _18087, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18084 = _18994.lo;
    float _18085 = _18994.hi;
    float _19605 = sine_bounds(_18084, _18085, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18094 = Interval{ _19601, interval_sine_upper };
    Interval _18095 = _18995;
    Interval _19608 = jet_mul_derivative(_18094, _18095, intervalFailed, optical_product_upper);
    Interval _18096 = Interval{ _19601, interval_sine_upper };
    Interval _18097 = _18996;
    Interval _19610 = jet_mul_derivative(_18096, _18097, intervalFailed, optical_product_upper);
    Interval _18070 = Interval{ 9.0, 9.0 };
    Interval _18071 = Interval{ _19605, interval_sine_upper };
    Interval _19612 = imul(_18070, _18071, intervalFailed, optical_product_upper);
    Interval _18072 = Interval{ 0.0, 0.0 };
    Interval _18073 = Interval{ _19605, interval_sine_upper };
    Interval _19614 = jet_mul_derivative(_18072, _18073, intervalFailed, optical_product_upper);
    Interval _18074 = _19614;
    Interval _18075 = Interval{ 9.0, 9.0 };
    Interval _18076 = _19608;
    Interval _19615 = jet_mul_derivative(_18075, _18076, intervalFailed, optical_product_upper);
    Interval _18077 = _19615;
    Interval _19616 = jet_add_derivative(_18074, _18077, intervalFailed);
    Interval _18078 = Interval{ 0.0, 0.0 };
    Interval _18079 = Interval{ _19605, interval_sine_upper };
    Interval _19618 = jet_mul_derivative(_18078, _18079, intervalFailed, optical_product_upper);
    Interval _18080 = _19618;
    Interval _18081 = Interval{ 9.0, 9.0 };
    Interval _18082 = _19610;
    Interval _19619 = jet_mul_derivative(_18081, _18082, intervalFailed, optical_product_upper);
    Interval _18083 = _19619;
    Interval _19620 = jet_add_derivative(_18080, _18083, intervalFailed);
    Interval _18056 = _19612;
    Interval _18057 = _19318;
    Interval _19621 = imul(_18056, _18057, intervalFailed, optical_product_upper);
    Interval _18058 = _19616;
    Interval _18059 = _19318;
    Interval _19622 = jet_mul_derivative(_18058, _18059, intervalFailed, optical_product_upper);
    Interval _18060 = _19622;
    Interval _18061 = _19612;
    Interval _18062 = _19345;
    Interval _19623 = jet_mul_derivative(_18061, _18062, intervalFailed, optical_product_upper);
    Interval _18063 = _19623;
    Interval _19624 = jet_add_derivative(_18060, _18063, intervalFailed);
    Interval _18064 = _19620;
    Interval _18065 = _19318;
    Interval _19625 = jet_mul_derivative(_18064, _18065, intervalFailed, optical_product_upper);
    Interval _18066 = _19625;
    Interval _18067 = _19612;
    Interval _18068 = _19357;
    Interval _19626 = jet_mul_derivative(_18067, _18068, intervalFailed, optical_product_upper);
    Interval _18069 = _19626;
    Interval _19627 = jet_add_derivative(_18066, _18069, intervalFailed);
    Interval _18048 = _19102;
    float _18046 = 3.1415927410125732421875;
    float _19628 = interval_down(_18046, intervalFailed);
    float _18047 = 3.1415927410125732421875;
    float _19629 = interval_up(_18047, intervalFailed);
    Interval _18049 = Interval{ _19628, _19629 };
    Interval _18050 = Interval{ 0.5, 0.5 };
    Interval _19631 = imul(_18049, _18050, intervalFailed, optical_product_upper);
    Interval _18051 = _19631;
    Interval _19632 = iadd(_18048, _18051, intervalFailed);
    float _18044 = _19632.lo;
    float _18045 = _19632.hi;
    float _19635 = sine_bounds(_18044, _18045, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18042 = _19102.lo;
    float _18043 = _19102.hi;
    float _19639 = sine_bounds(_18042, _18043, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18052 = Interval{ _19635, interval_sine_upper };
    Interval _18053 = _19103;
    Interval _19642 = jet_mul_derivative(_18052, _18053, intervalFailed, optical_product_upper);
    Interval _18054 = Interval{ _19635, interval_sine_upper };
    Interval _18055 = _19104;
    Interval _19644 = jet_mul_derivative(_18054, _18055, intervalFailed, optical_product_upper);
    Interval _18028 = Interval{ 2.5, 2.5 };
    Interval _18029 = Interval{ _19639, interval_sine_upper };
    Interval _19646 = imul(_18028, _18029, intervalFailed, optical_product_upper);
    Interval _18030 = Interval{ 0.0, 0.0 };
    Interval _18031 = Interval{ _19639, interval_sine_upper };
    Interval _19648 = jet_mul_derivative(_18030, _18031, intervalFailed, optical_product_upper);
    Interval _18032 = _19648;
    Interval _18033 = Interval{ 2.5, 2.5 };
    Interval _18034 = _19642;
    Interval _19649 = jet_mul_derivative(_18033, _18034, intervalFailed, optical_product_upper);
    Interval _18035 = _19649;
    Interval _19650 = jet_add_derivative(_18032, _18035, intervalFailed);
    Interval _18036 = Interval{ 0.0, 0.0 };
    Interval _18037 = Interval{ _19639, interval_sine_upper };
    Interval _19652 = jet_mul_derivative(_18036, _18037, intervalFailed, optical_product_upper);
    Interval _18038 = _19652;
    Interval _18039 = Interval{ 2.5, 2.5 };
    Interval _18040 = _19644;
    Interval _19653 = jet_mul_derivative(_18039, _18040, intervalFailed, optical_product_upper);
    Interval _18041 = _19653;
    Interval _19654 = jet_add_derivative(_18038, _18041, intervalFailed);
    Interval _18014 = _19646;
    Interval _18015 = _19436;
    Interval _19655 = imul(_18014, _18015, intervalFailed, optical_product_upper);
    Interval _18016 = _19650;
    Interval _18017 = _19436;
    Interval _19656 = jet_mul_derivative(_18016, _18017, intervalFailed, optical_product_upper);
    Interval _18018 = _19656;
    Interval _18019 = _19646;
    Interval _18020 = _19463;
    Interval _19657 = jet_mul_derivative(_18019, _18020, intervalFailed, optical_product_upper);
    Interval _18021 = _19657;
    Interval _19658 = jet_add_derivative(_18018, _18021, intervalFailed);
    Interval _18022 = _19654;
    Interval _18023 = _19436;
    Interval _19659 = jet_mul_derivative(_18022, _18023, intervalFailed, optical_product_upper);
    Interval _18024 = _19659;
    Interval _18025 = _19646;
    Interval _18026 = _19475;
    Interval _19660 = jet_mul_derivative(_18025, _18026, intervalFailed, optical_product_upper);
    Interval _18027 = _19660;
    Interval _19661 = jet_add_derivative(_18024, _18027, intervalFailed);
    Interval _18008 = _19621;
    Interval _18009 = _19655;
    Interval _19662 = iadd(_18008, _18009, intervalFailed);
    Interval _18010 = _19624;
    Interval _18011 = _19658;
    Interval _19663 = jet_add_derivative(_18010, _18011, intervalFailed);
    Interval _18012 = _19627;
    Interval _18013 = _19661;
    Interval _19664 = jet_add_derivative(_18012, _18013, intervalFailed);
    Interval _18000 = _19235;
    float _17998 = 3.1415927410125732421875;
    float _19665 = interval_down(_17998, intervalFailed);
    float _17999 = 3.1415927410125732421875;
    float _19666 = interval_up(_17999, intervalFailed);
    Interval _18001 = Interval{ _19665, _19666 };
    Interval _18002 = Interval{ 0.5, 0.5 };
    Interval _19668 = imul(_18001, _18002, intervalFailed, optical_product_upper);
    Interval _18003 = _19668;
    Interval _19669 = iadd(_18000, _18003, intervalFailed);
    float _17996 = _19669.lo;
    float _17997 = _19669.hi;
    float _19672 = sine_bounds(_17996, _17997, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17994 = _19235.lo;
    float _17995 = _19235.hi;
    float _19676 = sine_bounds(_17994, _17995, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18004 = Interval{ _19672, interval_sine_upper };
    Interval _18005 = _19237;
    Interval _19679 = jet_mul_derivative(_18004, _18005, intervalFailed, optical_product_upper);
    Interval _18006 = Interval{ _19672, interval_sine_upper };
    Interval _18007 = _19239;
    Interval _19681 = jet_mul_derivative(_18006, _18007, intervalFailed, optical_product_upper);
    Interval _17980 = Interval{ 5.0, 5.0 };
    Interval _17981 = Interval{ _19676, interval_sine_upper };
    Interval _19683 = imul(_17980, _17981, intervalFailed, optical_product_upper);
    Interval _17982 = Interval{ 0.0, 0.0 };
    Interval _17983 = Interval{ _19676, interval_sine_upper };
    Interval _19685 = jet_mul_derivative(_17982, _17983, intervalFailed, optical_product_upper);
    Interval _17984 = _19685;
    Interval _17985 = Interval{ 5.0, 5.0 };
    Interval _17986 = _19679;
    Interval _19686 = jet_mul_derivative(_17985, _17986, intervalFailed, optical_product_upper);
    Interval _17987 = _19686;
    Interval _19687 = jet_add_derivative(_17984, _17987, intervalFailed);
    Interval _17988 = Interval{ 0.0, 0.0 };
    Interval _17989 = Interval{ _19676, interval_sine_upper };
    Interval _19689 = jet_mul_derivative(_17988, _17989, intervalFailed, optical_product_upper);
    Interval _17990 = _19689;
    Interval _17991 = Interval{ 5.0, 5.0 };
    Interval _17992 = _19681;
    Interval _19690 = jet_mul_derivative(_17991, _17992, intervalFailed, optical_product_upper);
    Interval _17993 = _19690;
    Interval _19691 = jet_add_derivative(_17990, _17993, intervalFailed);
    Interval _17966 = _19683;
    Interval _17967 = _19554;
    Interval _19692 = imul(_17966, _17967, intervalFailed, optical_product_upper);
    Interval _17968 = _19687;
    Interval _17969 = _19554;
    Interval _19693 = jet_mul_derivative(_17968, _17969, intervalFailed, optical_product_upper);
    Interval _17970 = _19693;
    Interval _17971 = _19683;
    Interval _17972 = _19581;
    Interval _19694 = jet_mul_derivative(_17971, _17972, intervalFailed, optical_product_upper);
    Interval _17973 = _19694;
    Interval _19695 = jet_add_derivative(_17970, _17973, intervalFailed);
    Interval _17974 = _19691;
    Interval _17975 = _19554;
    Interval _19696 = jet_mul_derivative(_17974, _17975, intervalFailed, optical_product_upper);
    Interval _17976 = _19696;
    Interval _17977 = _19683;
    Interval _17978 = _19593;
    Interval _19697 = jet_mul_derivative(_17977, _17978, intervalFailed, optical_product_upper);
    Interval _17979 = _19697;
    Interval _19698 = jet_add_derivative(_17976, _17979, intervalFailed);
    Interval _17960 = _19662;
    Interval _17961 = _19692;
    Interval _19699 = iadd(_17960, _17961, intervalFailed);
    Interval _17962 = _19663;
    Interval _17963 = _19695;
    Interval _19700 = jet_add_derivative(_17962, _17963, intervalFailed);
    Interval _17964 = _19664;
    Interval _17965 = _19698;
    Interval _19701 = jet_add_derivative(_17964, _17965, intervalFailed);
    float _17954 = _18717.lo;
    float _17955 = _18717.hi;
    float _19704 = sine_bounds(_17954, _17955, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19708 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19711 = as_type<float>(as_type<uint>(_19704) ^ 2147483648u);
    Interval _17950 = _18717;
    float _17948 = 3.1415927410125732421875;
    float _19712 = interval_down(_17948, intervalFailed);
    float _17949 = 3.1415927410125732421875;
    float _19713 = interval_up(_17949, intervalFailed);
    Interval _17951 = Interval{ _19712, _19713 };
    Interval _17952 = Interval{ 0.5, 0.5 };
    Interval _19715 = imul(_17951, _17952, intervalFailed, optical_product_upper);
    Interval _17953 = _19715;
    Interval _19716 = iadd(_17950, _17953, intervalFailed);
    float _17946 = _19716.lo;
    float _17947 = _19716.hi;
    float _19719 = sine_bounds(_17946, _17947, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17956 = Interval{ _19708, _19711 };
    Interval _17957 = _18718;
    Interval _19722 = jet_mul_derivative(_17956, _17957, intervalFailed, optical_product_upper);
    Interval _17958 = Interval{ _19708, _19711 };
    Interval _17959 = _18719;
    Interval _19724 = jet_mul_derivative(_17958, _17959, intervalFailed, optical_product_upper);
    Interval _17932 = Interval{ _19719, interval_sine_upper };
    Interval _17933 = _18798;
    Interval _19726 = imul(_17932, _17933, intervalFailed, optical_product_upper);
    Interval _17934 = _19722;
    Interval _17935 = _18798;
    Interval _19727 = jet_mul_derivative(_17934, _17935, intervalFailed, optical_product_upper);
    Interval _17936 = _19727;
    Interval _17937 = Interval{ _19719, interval_sine_upper };
    Interval _17938 = _18825;
    Interval _19729 = jet_mul_derivative(_17937, _17938, intervalFailed, optical_product_upper);
    Interval _17939 = _19729;
    Interval _19730 = jet_add_derivative(_17936, _17939, intervalFailed);
    Interval _17940 = _19724;
    Interval _17941 = _18798;
    Interval _19731 = jet_mul_derivative(_17940, _17941, intervalFailed, optical_product_upper);
    Interval _17942 = _19731;
    Interval _17943 = Interval{ _19719, interval_sine_upper };
    Interval _17944 = _18837;
    Interval _19733 = jet_mul_derivative(_17943, _17944, intervalFailed, optical_product_upper);
    Interval _17945 = _19733;
    Interval _19734 = jet_add_derivative(_17942, _17945, intervalFailed);
    float _17930 = 18.0;
    float _17931 = 1000.0;
    Interval _19736 = iratio(_17930, _17931, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19741;
    if (!intervalFailed)
    {
        _19741 = intervalFailed;
    }
    else
    {
        _19741 = false;
    }
    bool _19746;
    if (_19741)
    {
        _19746 = jetFailureSite == 0u;
    }
    else
    {
        _19746 = false;
    }
    if (_19746)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    float _17928 = 39.0;
    float _17929 = 10000.0;
    Interval _19750 = iratio(_17928, _17929, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19755;
    if (!intervalFailed)
    {
        _19755 = intervalFailed;
    }
    else
    {
        _19755 = false;
    }
    bool _19760;
    if (_19755)
    {
        _19760 = jetFailureSite == 0u;
    }
    else
    {
        _19760 = false;
    }
    if (_19760)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(39.0, 39.0, 10000.0, 10000.0);
    }
    Interval _17914 = _19750;
    Interval _17915 = _19726;
    Interval _19763 = imul(_17914, _17915, intervalFailed, optical_product_upper);
    Interval _17916 = Interval{ 0.0, 0.0 };
    Interval _17917 = _19726;
    Interval _19764 = jet_mul_derivative(_17916, _17917, intervalFailed, optical_product_upper);
    Interval _17918 = _19764;
    Interval _17919 = _19750;
    Interval _17920 = _19730;
    Interval _19765 = jet_mul_derivative(_17919, _17920, intervalFailed, optical_product_upper);
    Interval _17921 = _19765;
    Interval _19766 = jet_add_derivative(_17918, _17921, intervalFailed);
    Interval _17922 = Interval{ 0.0, 0.0 };
    Interval _17923 = _19726;
    Interval _19767 = jet_mul_derivative(_17922, _17923, intervalFailed, optical_product_upper);
    Interval _17924 = _19767;
    Interval _17925 = _19750;
    Interval _17926 = _19734;
    Interval _19768 = jet_mul_derivative(_17925, _17926, intervalFailed, optical_product_upper);
    Interval _17927 = _19768;
    Interval _19769 = jet_add_derivative(_17924, _17927, intervalFailed);
    Interval _17908 = _19736;
    Interval _17909 = _19763;
    Interval _19770 = iadd(_17908, _17909, intervalFailed);
    Interval _17910 = Interval{ 0.0, 0.0 };
    Interval _17911 = _19766;
    Interval _19771 = jet_add_derivative(_17910, _17911, intervalFailed);
    Interval _17912 = Interval{ 0.0, 0.0 };
    Interval _17913 = _19769;
    Interval _19772 = jet_add_derivative(_17912, _17913, intervalFailed);
    Interval _17894 = Interval{ 9.0, 9.0 };
    Interval _17895 = _19770;
    Interval _19773 = imul(_17894, _17895, intervalFailed, optical_product_upper);
    Interval _17896 = Interval{ 0.0, 0.0 };
    Interval _17897 = _19770;
    Interval _19774 = jet_mul_derivative(_17896, _17897, intervalFailed, optical_product_upper);
    Interval _17898 = _19774;
    Interval _17899 = Interval{ 9.0, 9.0 };
    Interval _17900 = _19771;
    Interval _19775 = jet_mul_derivative(_17899, _17900, intervalFailed, optical_product_upper);
    Interval _17901 = _19775;
    Interval _19776 = jet_add_derivative(_17898, _17901, intervalFailed);
    Interval _17902 = Interval{ 0.0, 0.0 };
    Interval _17903 = _19770;
    Interval _19777 = jet_mul_derivative(_17902, _17903, intervalFailed, optical_product_upper);
    Interval _17904 = _19777;
    Interval _17905 = Interval{ 9.0, 9.0 };
    Interval _17906 = _19772;
    Interval _19778 = jet_mul_derivative(_17905, _17906, intervalFailed, optical_product_upper);
    Interval _17907 = _19778;
    Interval _19779 = jet_add_derivative(_17904, _17907, intervalFailed);
    float _17888 = _18994.lo;
    float _17889 = _18994.hi;
    float _19782 = sine_bounds(_17888, _17889, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19786 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19789 = as_type<float>(as_type<uint>(_19782) ^ 2147483648u);
    Interval _17884 = _18994;
    float _17882 = 3.1415927410125732421875;
    float _19790 = interval_down(_17882, intervalFailed);
    float _17883 = 3.1415927410125732421875;
    float _19791 = interval_up(_17883, intervalFailed);
    Interval _17885 = Interval{ _19790, _19791 };
    Interval _17886 = Interval{ 0.5, 0.5 };
    Interval _19793 = imul(_17885, _17886, intervalFailed, optical_product_upper);
    Interval _17887 = _19793;
    Interval _19794 = iadd(_17884, _17887, intervalFailed);
    float _17880 = _19794.lo;
    float _17881 = _19794.hi;
    float _19797 = sine_bounds(_17880, _17881, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17890 = Interval{ _19786, _19789 };
    Interval _17891 = _18995;
    Interval _19800 = jet_mul_derivative(_17890, _17891, intervalFailed, optical_product_upper);
    Interval _17892 = Interval{ _19786, _19789 };
    Interval _17893 = _18996;
    Interval _19802 = jet_mul_derivative(_17892, _17893, intervalFailed, optical_product_upper);
    Interval _17866 = _19773;
    Interval _17867 = Interval{ _19797, interval_sine_upper };
    Interval _19804 = imul(_17866, _17867, intervalFailed, optical_product_upper);
    Interval _17868 = _19776;
    Interval _17869 = Interval{ _19797, interval_sine_upper };
    Interval _19806 = jet_mul_derivative(_17868, _17869, intervalFailed, optical_product_upper);
    Interval _17870 = _19806;
    Interval _17871 = _19773;
    Interval _17872 = _19800;
    Interval _19807 = jet_mul_derivative(_17871, _17872, intervalFailed, optical_product_upper);
    Interval _17873 = _19807;
    Interval _19808 = jet_add_derivative(_17870, _17873, intervalFailed);
    Interval _17874 = _19779;
    Interval _17875 = Interval{ _19797, interval_sine_upper };
    Interval _19810 = jet_mul_derivative(_17874, _17875, intervalFailed, optical_product_upper);
    Interval _17876 = _19810;
    Interval _17877 = _19773;
    Interval _17878 = _19802;
    Interval _19811 = jet_mul_derivative(_17877, _17878, intervalFailed, optical_product_upper);
    Interval _17879 = _19811;
    Interval _19812 = jet_add_derivative(_17876, _17879, intervalFailed);
    Interval _17852 = _19804;
    Interval _17853 = _19318;
    Interval _19813 = imul(_17852, _17853, intervalFailed, optical_product_upper);
    Interval _17854 = _19808;
    Interval _17855 = _19318;
    Interval _19814 = jet_mul_derivative(_17854, _17855, intervalFailed, optical_product_upper);
    Interval _17856 = _19814;
    Interval _17857 = _19804;
    Interval _17858 = _19345;
    Interval _19815 = jet_mul_derivative(_17857, _17858, intervalFailed, optical_product_upper);
    Interval _17859 = _19815;
    Interval _19816 = jet_add_derivative(_17856, _17859, intervalFailed);
    Interval _17860 = _19812;
    Interval _17861 = _19318;
    Interval _19817 = jet_mul_derivative(_17860, _17861, intervalFailed, optical_product_upper);
    Interval _17862 = _19817;
    Interval _17863 = _19804;
    Interval _17864 = _19357;
    Interval _19818 = jet_mul_derivative(_17863, _17864, intervalFailed, optical_product_upper);
    Interval _17865 = _19818;
    Interval _19819 = jet_add_derivative(_17862, _17865, intervalFailed);
    float _17850 = 1175.0;
    float _17851 = 10000.0;
    Interval _19821 = iratio(_17850, _17851, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19826;
    if (!intervalFailed)
    {
        _19826 = intervalFailed;
    }
    else
    {
        _19826 = false;
    }
    bool _19831;
    if (_19826)
    {
        _19831 = jetFailureSite == 0u;
    }
    else
    {
        _19831 = false;
    }
    if (_19831)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(1175.0, 1175.0, 10000.0, 10000.0);
    }
    float _17844 = _19102.lo;
    float _17845 = _19102.hi;
    float _19836 = sine_bounds(_17844, _17845, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19840 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19843 = as_type<float>(as_type<uint>(_19836) ^ 2147483648u);
    Interval _17840 = _19102;
    float _17838 = 3.1415927410125732421875;
    float _19844 = interval_down(_17838, intervalFailed);
    float _17839 = 3.1415927410125732421875;
    float _19845 = interval_up(_17839, intervalFailed);
    Interval _17841 = Interval{ _19844, _19845 };
    Interval _17842 = Interval{ 0.5, 0.5 };
    Interval _19847 = imul(_17841, _17842, intervalFailed, optical_product_upper);
    Interval _17843 = _19847;
    Interval _19848 = iadd(_17840, _17843, intervalFailed);
    float _17836 = _19848.lo;
    float _17837 = _19848.hi;
    float _19851 = sine_bounds(_17836, _17837, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17846 = Interval{ _19840, _19843 };
    Interval _17847 = _19103;
    Interval _19854 = jet_mul_derivative(_17846, _17847, intervalFailed, optical_product_upper);
    Interval _17848 = Interval{ _19840, _19843 };
    Interval _17849 = _19104;
    Interval _19856 = jet_mul_derivative(_17848, _17849, intervalFailed, optical_product_upper);
    Interval _17822 = _19821;
    Interval _17823 = Interval{ _19851, interval_sine_upper };
    Interval _19858 = imul(_17822, _17823, intervalFailed, optical_product_upper);
    Interval _17824 = Interval{ 0.0, 0.0 };
    Interval _17825 = Interval{ _19851, interval_sine_upper };
    Interval _19860 = jet_mul_derivative(_17824, _17825, intervalFailed, optical_product_upper);
    Interval _17826 = _19860;
    Interval _17827 = _19821;
    Interval _17828 = _19854;
    Interval _19861 = jet_mul_derivative(_17827, _17828, intervalFailed, optical_product_upper);
    Interval _17829 = _19861;
    Interval _19862 = jet_add_derivative(_17826, _17829, intervalFailed);
    Interval _17830 = Interval{ 0.0, 0.0 };
    Interval _17831 = Interval{ _19851, interval_sine_upper };
    Interval _19864 = jet_mul_derivative(_17830, _17831, intervalFailed, optical_product_upper);
    Interval _17832 = _19864;
    Interval _17833 = _19821;
    Interval _17834 = _19856;
    Interval _19865 = jet_mul_derivative(_17833, _17834, intervalFailed, optical_product_upper);
    Interval _17835 = _19865;
    Interval _19866 = jet_add_derivative(_17832, _17835, intervalFailed);
    Interval _17808 = _19858;
    Interval _17809 = _19436;
    Interval _19867 = imul(_17808, _17809, intervalFailed, optical_product_upper);
    Interval _17810 = _19862;
    Interval _17811 = _19436;
    Interval _19868 = jet_mul_derivative(_17810, _17811, intervalFailed, optical_product_upper);
    Interval _17812 = _19868;
    Interval _17813 = _19858;
    Interval _17814 = _19463;
    Interval _19869 = jet_mul_derivative(_17813, _17814, intervalFailed, optical_product_upper);
    Interval _17815 = _19869;
    Interval _19870 = jet_add_derivative(_17812, _17815, intervalFailed);
    Interval _17816 = _19866;
    Interval _17817 = _19436;
    Interval _19871 = jet_mul_derivative(_17816, _17817, intervalFailed, optical_product_upper);
    Interval _17818 = _19871;
    Interval _17819 = _19858;
    Interval _17820 = _19475;
    Interval _19872 = jet_mul_derivative(_17819, _17820, intervalFailed, optical_product_upper);
    Interval _17821 = _19872;
    Interval _19873 = jet_add_derivative(_17818, _17821, intervalFailed);
    Interval _17802 = _19813;
    Interval _17803 = _19867;
    Interval _19874 = iadd(_17802, _17803, intervalFailed);
    Interval _17804 = _19816;
    Interval _17805 = _19870;
    Interval _19875 = jet_add_derivative(_17804, _17805, intervalFailed);
    Interval _17806 = _19819;
    Interval _17807 = _19873;
    Interval _19876 = jet_add_derivative(_17806, _17807, intervalFailed);
    float _17800 = 45.0;
    float _17801 = 1000.0;
    Interval _19878 = iratio(_17800, _17801, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19883;
    if (!intervalFailed)
    {
        _19883 = intervalFailed;
    }
    else
    {
        _19883 = false;
    }
    bool _19888;
    if (_19883)
    {
        _19888 = jetFailureSite == 0u;
    }
    else
    {
        _19888 = false;
    }
    if (_19888)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
    }
    float _17794 = _19235.lo;
    float _17795 = _19235.hi;
    float _19893 = sine_bounds(_17794, _17795, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19897 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19900 = as_type<float>(as_type<uint>(_19893) ^ 2147483648u);
    Interval _17790 = _19235;
    float _17788 = 3.1415927410125732421875;
    float _19901 = interval_down(_17788, intervalFailed);
    float _17789 = 3.1415927410125732421875;
    float _19902 = interval_up(_17789, intervalFailed);
    Interval _17791 = Interval{ _19901, _19902 };
    Interval _17792 = Interval{ 0.5, 0.5 };
    Interval _19904 = imul(_17791, _17792, intervalFailed, optical_product_upper);
    Interval _17793 = _19904;
    Interval _19905 = iadd(_17790, _17793, intervalFailed);
    float _17786 = _19905.lo;
    float _17787 = _19905.hi;
    float _19908 = sine_bounds(_17786, _17787, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17796 = Interval{ _19897, _19900 };
    Interval _17797 = _19237;
    Interval _19911 = jet_mul_derivative(_17796, _17797, intervalFailed, optical_product_upper);
    Interval _17798 = Interval{ _19897, _19900 };
    Interval _17799 = _19239;
    Interval _19913 = jet_mul_derivative(_17798, _17799, intervalFailed, optical_product_upper);
    Interval _17772 = _19878;
    Interval _17773 = Interval{ _19908, interval_sine_upper };
    Interval _19915 = imul(_17772, _17773, intervalFailed, optical_product_upper);
    Interval _17774 = Interval{ 0.0, 0.0 };
    Interval _17775 = Interval{ _19908, interval_sine_upper };
    Interval _19917 = jet_mul_derivative(_17774, _17775, intervalFailed, optical_product_upper);
    Interval _17776 = _19917;
    Interval _17777 = _19878;
    Interval _17778 = _19911;
    Interval _19918 = jet_mul_derivative(_17777, _17778, intervalFailed, optical_product_upper);
    Interval _17779 = _19918;
    Interval _19919 = jet_add_derivative(_17776, _17779, intervalFailed);
    Interval _17780 = Interval{ 0.0, 0.0 };
    Interval _17781 = Interval{ _19908, interval_sine_upper };
    Interval _19921 = jet_mul_derivative(_17780, _17781, intervalFailed, optical_product_upper);
    Interval _17782 = _19921;
    Interval _17783 = _19878;
    Interval _17784 = _19913;
    Interval _19922 = jet_mul_derivative(_17783, _17784, intervalFailed, optical_product_upper);
    Interval _17785 = _19922;
    Interval _19923 = jet_add_derivative(_17782, _17785, intervalFailed);
    Interval _17758 = _19915;
    Interval _17759 = _19554;
    Interval _19924 = imul(_17758, _17759, intervalFailed, optical_product_upper);
    Interval _17760 = _19919;
    Interval _17761 = _19554;
    Interval _19925 = jet_mul_derivative(_17760, _17761, intervalFailed, optical_product_upper);
    Interval _17762 = _19925;
    Interval _17763 = _19915;
    Interval _17764 = _19581;
    Interval _19926 = jet_mul_derivative(_17763, _17764, intervalFailed, optical_product_upper);
    Interval _17765 = _19926;
    Interval _19927 = jet_add_derivative(_17762, _17765, intervalFailed);
    Interval _17766 = _19923;
    Interval _17767 = _19554;
    Interval _19928 = jet_mul_derivative(_17766, _17767, intervalFailed, optical_product_upper);
    Interval _17768 = _19928;
    Interval _17769 = _19915;
    Interval _17770 = _19593;
    Interval _19929 = jet_mul_derivative(_17769, _17770, intervalFailed, optical_product_upper);
    Interval _17771 = _19929;
    Interval _19930 = jet_add_derivative(_17768, _17771, intervalFailed);
    Interval _17752 = _19874;
    Interval _17753 = Interval{ as_type<float>(as_type<uint>(_19924.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19924.lo) ^ 2147483648u) };
    Interval _19956 = iadd(_17752, _17753, intervalFailed);
    Interval _17754 = _19875;
    Interval _17755 = Interval{ as_type<float>(as_type<uint>(_19927.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19927.lo) ^ 2147483648u) };
    Interval _19958 = jet_add_derivative(_17754, _17755, intervalFailed);
    Interval _17756 = _19876;
    Interval _17757 = Interval{ as_type<float>(as_type<uint>(_19930.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19930.lo) ^ 2147483648u) };
    Interval _19960 = jet_add_derivative(_17756, _17757, intervalFailed);
    float _17750 = 11.0;
    float _17751 = 1000.0;
    Interval _19962 = iratio(_17750, _17751, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19967;
    if (!intervalFailed)
    {
        _19967 = intervalFailed;
    }
    else
    {
        _19967 = false;
    }
    bool _19972;
    if (_19967)
    {
        _19972 = jetFailureSite == 0u;
    }
    else
    {
        _19972 = false;
    }
    if (_19972)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    float _17748 = 52.0;
    float _17749 = 10000.0;
    Interval _19976 = iratio(_17748, _17749, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19981;
    if (!intervalFailed)
    {
        _19981 = intervalFailed;
    }
    else
    {
        _19981 = false;
    }
    bool _19986;
    if (_19981)
    {
        _19986 = jetFailureSite == 0u;
    }
    else
    {
        _19986 = false;
    }
    if (_19986)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(52.0, 52.0, 10000.0, 10000.0);
    }
    Interval _17734 = _19976;
    Interval _17735 = _19726;
    Interval _19989 = imul(_17734, _17735, intervalFailed, optical_product_upper);
    Interval _17736 = Interval{ 0.0, 0.0 };
    Interval _17737 = _19726;
    Interval _19990 = jet_mul_derivative(_17736, _17737, intervalFailed, optical_product_upper);
    Interval _17738 = _19990;
    Interval _17739 = _19976;
    Interval _17740 = _19730;
    Interval _19991 = jet_mul_derivative(_17739, _17740, intervalFailed, optical_product_upper);
    Interval _17741 = _19991;
    Interval _19992 = jet_add_derivative(_17738, _17741, intervalFailed);
    Interval _17742 = Interval{ 0.0, 0.0 };
    Interval _17743 = _19726;
    Interval _19993 = jet_mul_derivative(_17742, _17743, intervalFailed, optical_product_upper);
    Interval _17744 = _19993;
    Interval _17745 = _19976;
    Interval _17746 = _19734;
    Interval _19994 = jet_mul_derivative(_17745, _17746, intervalFailed, optical_product_upper);
    Interval _17747 = _19994;
    Interval _19995 = jet_add_derivative(_17744, _17747, intervalFailed);
    Interval _17728 = _19962;
    Interval _17729 = Interval{ as_type<float>(as_type<uint>(_19989.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19989.lo) ^ 2147483648u) };
    Interval _20021 = iadd(_17728, _17729, intervalFailed);
    Interval _17730 = Interval{ 0.0, 0.0 };
    Interval _17731 = Interval{ as_type<float>(as_type<uint>(_19992.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19992.lo) ^ 2147483648u) };
    Interval _20023 = jet_add_derivative(_17730, _17731, intervalFailed);
    Interval _17732 = Interval{ 0.0, 0.0 };
    Interval _17733 = Interval{ as_type<float>(as_type<uint>(_19995.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19995.lo) ^ 2147483648u) };
    Interval _20025 = jet_add_derivative(_17732, _17733, intervalFailed);
    Interval _17714 = Interval{ 9.0, 9.0 };
    Interval _17715 = _20021;
    Interval _20026 = imul(_17714, _17715, intervalFailed, optical_product_upper);
    Interval _17716 = Interval{ 0.0, 0.0 };
    Interval _17717 = _20021;
    Interval _20027 = jet_mul_derivative(_17716, _17717, intervalFailed, optical_product_upper);
    Interval _17718 = _20027;
    Interval _17719 = Interval{ 9.0, 9.0 };
    Interval _17720 = _20023;
    Interval _20028 = jet_mul_derivative(_17719, _17720, intervalFailed, optical_product_upper);
    Interval _17721 = _20028;
    Interval _20029 = jet_add_derivative(_17718, _17721, intervalFailed);
    Interval _17722 = Interval{ 0.0, 0.0 };
    Interval _17723 = _20021;
    Interval _20030 = jet_mul_derivative(_17722, _17723, intervalFailed, optical_product_upper);
    Interval _17724 = _20030;
    Interval _17725 = Interval{ 9.0, 9.0 };
    Interval _17726 = _20025;
    Interval _20031 = jet_mul_derivative(_17725, _17726, intervalFailed, optical_product_upper);
    Interval _17727 = _20031;
    Interval _20032 = jet_add_derivative(_17724, _17727, intervalFailed);
    float _17708 = _18994.lo;
    float _17709 = _18994.hi;
    float _20035 = sine_bounds(_17708, _17709, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20039 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20042 = as_type<float>(as_type<uint>(_20035) ^ 2147483648u);
    Interval _17704 = _18994;
    float _17702 = 3.1415927410125732421875;
    float _20043 = interval_down(_17702, intervalFailed);
    float _17703 = 3.1415927410125732421875;
    float _20044 = interval_up(_17703, intervalFailed);
    Interval _17705 = Interval{ _20043, _20044 };
    Interval _17706 = Interval{ 0.5, 0.5 };
    Interval _20046 = imul(_17705, _17706, intervalFailed, optical_product_upper);
    Interval _17707 = _20046;
    Interval _20047 = iadd(_17704, _17707, intervalFailed);
    float _17700 = _20047.lo;
    float _17701 = _20047.hi;
    float _20050 = sine_bounds(_17700, _17701, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17710 = Interval{ _20039, _20042 };
    Interval _17711 = _18995;
    Interval _20053 = jet_mul_derivative(_17710, _17711, intervalFailed, optical_product_upper);
    Interval _17712 = Interval{ _20039, _20042 };
    Interval _17713 = _18996;
    Interval _20055 = jet_mul_derivative(_17712, _17713, intervalFailed, optical_product_upper);
    Interval _17686 = _20026;
    Interval _17687 = Interval{ _20050, interval_sine_upper };
    Interval _20057 = imul(_17686, _17687, intervalFailed, optical_product_upper);
    Interval _17688 = _20029;
    Interval _17689 = Interval{ _20050, interval_sine_upper };
    Interval _20059 = jet_mul_derivative(_17688, _17689, intervalFailed, optical_product_upper);
    Interval _17690 = _20059;
    Interval _17691 = _20026;
    Interval _17692 = _20053;
    Interval _20060 = jet_mul_derivative(_17691, _17692, intervalFailed, optical_product_upper);
    Interval _17693 = _20060;
    Interval _20061 = jet_add_derivative(_17690, _17693, intervalFailed);
    Interval _17694 = _20032;
    Interval _17695 = Interval{ _20050, interval_sine_upper };
    Interval _20063 = jet_mul_derivative(_17694, _17695, intervalFailed, optical_product_upper);
    Interval _17696 = _20063;
    Interval _17697 = _20026;
    Interval _17698 = _20055;
    Interval _20064 = jet_mul_derivative(_17697, _17698, intervalFailed, optical_product_upper);
    Interval _17699 = _20064;
    Interval _20065 = jet_add_derivative(_17696, _17699, intervalFailed);
    Interval _17672 = _20057;
    Interval _17673 = _19318;
    Interval _20066 = imul(_17672, _17673, intervalFailed, optical_product_upper);
    Interval _17674 = _20061;
    Interval _17675 = _19318;
    Interval _20067 = jet_mul_derivative(_17674, _17675, intervalFailed, optical_product_upper);
    Interval _17676 = _20067;
    Interval _17677 = _20057;
    Interval _17678 = _19345;
    Interval _20068 = jet_mul_derivative(_17677, _17678, intervalFailed, optical_product_upper);
    Interval _17679 = _20068;
    Interval _20069 = jet_add_derivative(_17676, _17679, intervalFailed);
    Interval _17680 = _20065;
    Interval _17681 = _19318;
    Interval _20070 = jet_mul_derivative(_17680, _17681, intervalFailed, optical_product_upper);
    Interval _17682 = _20070;
    Interval _17683 = _20057;
    Interval _17684 = _19357;
    Interval _20071 = jet_mul_derivative(_17683, _17684, intervalFailed, optical_product_upper);
    Interval _17685 = _20071;
    Interval _20072 = jet_add_derivative(_17682, _17685, intervalFailed);
    float _17670 = 625.0;
    float _17671 = 10000.0;
    Interval _20074 = iratio(_17670, _17671, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20079;
    if (!intervalFailed)
    {
        _20079 = intervalFailed;
    }
    else
    {
        _20079 = false;
    }
    bool _20084;
    if (_20079)
    {
        _20084 = jetFailureSite == 0u;
    }
    else
    {
        _20084 = false;
    }
    if (_20084)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(625.0, 625.0, 10000.0, 10000.0);
    }
    float _17664 = _19102.lo;
    float _17665 = _19102.hi;
    float _20089 = sine_bounds(_17664, _17665, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20093 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20096 = as_type<float>(as_type<uint>(_20089) ^ 2147483648u);
    Interval _17660 = _19102;
    float _17658 = 3.1415927410125732421875;
    float _20097 = interval_down(_17658, intervalFailed);
    float _17659 = 3.1415927410125732421875;
    float _20098 = interval_up(_17659, intervalFailed);
    Interval _17661 = Interval{ _20097, _20098 };
    Interval _17662 = Interval{ 0.5, 0.5 };
    Interval _20100 = imul(_17661, _17662, intervalFailed, optical_product_upper);
    Interval _17663 = _20100;
    Interval _20101 = iadd(_17660, _17663, intervalFailed);
    float _17656 = _20101.lo;
    float _17657 = _20101.hi;
    float _20104 = sine_bounds(_17656, _17657, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17666 = Interval{ _20093, _20096 };
    Interval _17667 = _19103;
    Interval _20107 = jet_mul_derivative(_17666, _17667, intervalFailed, optical_product_upper);
    Interval _17668 = Interval{ _20093, _20096 };
    Interval _17669 = _19104;
    Interval _20109 = jet_mul_derivative(_17668, _17669, intervalFailed, optical_product_upper);
    Interval _17642 = _20074;
    Interval _17643 = Interval{ _20104, interval_sine_upper };
    Interval _20111 = imul(_17642, _17643, intervalFailed, optical_product_upper);
    Interval _17644 = Interval{ 0.0, 0.0 };
    Interval _17645 = Interval{ _20104, interval_sine_upper };
    Interval _20113 = jet_mul_derivative(_17644, _17645, intervalFailed, optical_product_upper);
    Interval _17646 = _20113;
    Interval _17647 = _20074;
    Interval _17648 = _20107;
    Interval _20114 = jet_mul_derivative(_17647, _17648, intervalFailed, optical_product_upper);
    Interval _17649 = _20114;
    Interval _20115 = jet_add_derivative(_17646, _17649, intervalFailed);
    Interval _17650 = Interval{ 0.0, 0.0 };
    Interval _17651 = Interval{ _20104, interval_sine_upper };
    Interval _20117 = jet_mul_derivative(_17650, _17651, intervalFailed, optical_product_upper);
    Interval _17652 = _20117;
    Interval _17653 = _20074;
    Interval _17654 = _20109;
    Interval _20118 = jet_mul_derivative(_17653, _17654, intervalFailed, optical_product_upper);
    Interval _17655 = _20118;
    Interval _20119 = jet_add_derivative(_17652, _17655, intervalFailed);
    Interval _17628 = _20111;
    Interval _17629 = _19436;
    Interval _20120 = imul(_17628, _17629, intervalFailed, optical_product_upper);
    Interval _17630 = _20115;
    Interval _17631 = _19436;
    Interval _20121 = jet_mul_derivative(_17630, _17631, intervalFailed, optical_product_upper);
    Interval _17632 = _20121;
    Interval _17633 = _20111;
    Interval _17634 = _19463;
    Interval _20122 = jet_mul_derivative(_17633, _17634, intervalFailed, optical_product_upper);
    Interval _17635 = _20122;
    Interval _20123 = jet_add_derivative(_17632, _17635, intervalFailed);
    Interval _17636 = _20119;
    Interval _17637 = _19436;
    Interval _20124 = jet_mul_derivative(_17636, _17637, intervalFailed, optical_product_upper);
    Interval _17638 = _20124;
    Interval _17639 = _20111;
    Interval _17640 = _19475;
    Interval _20125 = jet_mul_derivative(_17639, _17640, intervalFailed, optical_product_upper);
    Interval _17641 = _20125;
    Interval _20126 = jet_add_derivative(_17638, _17641, intervalFailed);
    Interval _17622 = _20066;
    Interval _17623 = Interval{ as_type<float>(as_type<uint>(_20120.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20120.lo) ^ 2147483648u) };
    Interval _20152 = iadd(_17622, _17623, intervalFailed);
    Interval _17624 = _20069;
    Interval _17625 = Interval{ as_type<float>(as_type<uint>(_20123.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20123.lo) ^ 2147483648u) };
    Interval _20154 = jet_add_derivative(_17624, _17625, intervalFailed);
    Interval _17626 = _20072;
    Interval _17627 = Interval{ as_type<float>(as_type<uint>(_20126.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20126.lo) ^ 2147483648u) };
    Interval _20156 = jet_add_derivative(_17626, _17627, intervalFailed);
    float _17620 = 110.0;
    float _17621 = 1000.0;
    Interval _20158 = iratio(_17620, _17621, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20163;
    if (!intervalFailed)
    {
        _20163 = intervalFailed;
    }
    else
    {
        _20163 = false;
    }
    bool _20168;
    if (_20163)
    {
        _20168 = jetFailureSite == 0u;
    }
    else
    {
        _20168 = false;
    }
    if (_20168)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(110.0, 110.0, 1000.0, 1000.0);
    }
    float _17614 = _19235.lo;
    float _17615 = _19235.hi;
    float _20173 = sine_bounds(_17614, _17615, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20177 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20180 = as_type<float>(as_type<uint>(_20173) ^ 2147483648u);
    Interval _17610 = _19235;
    float _17608 = 3.1415927410125732421875;
    float _20181 = interval_down(_17608, intervalFailed);
    float _17609 = 3.1415927410125732421875;
    float _20182 = interval_up(_17609, intervalFailed);
    Interval _17611 = Interval{ _20181, _20182 };
    Interval _17612 = Interval{ 0.5, 0.5 };
    Interval _20184 = imul(_17611, _17612, intervalFailed, optical_product_upper);
    Interval _17613 = _20184;
    Interval _20185 = iadd(_17610, _17613, intervalFailed);
    float _17606 = _20185.lo;
    float _17607 = _20185.hi;
    float _20188 = sine_bounds(_17606, _17607, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17616 = Interval{ _20177, _20180 };
    Interval _17617 = _19237;
    Interval _20191 = jet_mul_derivative(_17616, _17617, intervalFailed, optical_product_upper);
    Interval _17618 = Interval{ _20177, _20180 };
    Interval _17619 = _19239;
    Interval _20193 = jet_mul_derivative(_17618, _17619, intervalFailed, optical_product_upper);
    Interval _17592 = _20158;
    Interval _17593 = Interval{ _20188, interval_sine_upper };
    Interval _20195 = imul(_17592, _17593, intervalFailed, optical_product_upper);
    Interval _17594 = Interval{ 0.0, 0.0 };
    Interval _17595 = Interval{ _20188, interval_sine_upper };
    Interval _20197 = jet_mul_derivative(_17594, _17595, intervalFailed, optical_product_upper);
    Interval _17596 = _20197;
    Interval _17597 = _20158;
    Interval _17598 = _20191;
    Interval _20198 = jet_mul_derivative(_17597, _17598, intervalFailed, optical_product_upper);
    Interval _17599 = _20198;
    Interval _20199 = jet_add_derivative(_17596, _17599, intervalFailed);
    Interval _17600 = Interval{ 0.0, 0.0 };
    Interval _17601 = Interval{ _20188, interval_sine_upper };
    Interval _20201 = jet_mul_derivative(_17600, _17601, intervalFailed, optical_product_upper);
    Interval _17602 = _20201;
    Interval _17603 = _20158;
    Interval _17604 = _20193;
    Interval _20202 = jet_mul_derivative(_17603, _17604, intervalFailed, optical_product_upper);
    Interval _17605 = _20202;
    Interval _20203 = jet_add_derivative(_17602, _17605, intervalFailed);
    Interval _17578 = _20195;
    Interval _17579 = _19554;
    Interval _20204 = imul(_17578, _17579, intervalFailed, optical_product_upper);
    Interval _17580 = _20199;
    Interval _17581 = _19554;
    Interval _20205 = jet_mul_derivative(_17580, _17581, intervalFailed, optical_product_upper);
    Interval _17582 = _20205;
    Interval _17583 = _20195;
    Interval _17584 = _19581;
    Interval _20206 = jet_mul_derivative(_17583, _17584, intervalFailed, optical_product_upper);
    Interval _17585 = _20206;
    Interval _20207 = jet_add_derivative(_17582, _17585, intervalFailed);
    Interval _17586 = _20203;
    Interval _17587 = _19554;
    Interval _20208 = jet_mul_derivative(_17586, _17587, intervalFailed, optical_product_upper);
    Interval _17588 = _20208;
    Interval _17589 = _20195;
    Interval _17590 = _19593;
    Interval _20209 = jet_mul_derivative(_17589, _17590, intervalFailed, optical_product_upper);
    Interval _17591 = _20209;
    Interval _20210 = jet_add_derivative(_17588, _17591, intervalFailed);
    Interval _17572 = _20152;
    Interval _17573 = _20204;
    Interval _20211 = iadd(_17572, _17573, intervalFailed);
    Interval _17574 = _20154;
    Interval _17575 = _20207;
    Interval _20212 = jet_add_derivative(_17574, _17575, intervalFailed);
    Interval _17576 = _20156;
    Interval _17577 = _20210;
    Interval _20213 = jet_add_derivative(_17576, _17577, intervalFailed);
    float _17570 = 173.0;
    float _17571 = 1000.0;
    Interval _20219 = iratio(_17570, _17571, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20224;
    if (!intervalFailed)
    {
        _20224 = intervalFailed;
    }
    else
    {
        _20224 = false;
    }
    bool _20229;
    if (_20224)
    {
        _20229 = jetFailureSite == 0u;
    }
    else
    {
        _20229 = false;
    }
    if (_20229)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(173.0, 173.0, 1000.0, 1000.0);
    }
    Interval _17556 = x.v;
    Interval _17557 = _20219;
    Interval _20232 = imul(_17556, _17557, intervalFailed, optical_product_upper);
    Interval _17558 = x.dx;
    Interval _17559 = _20219;
    Interval _20233 = jet_mul_derivative(_17558, _17559, intervalFailed, optical_product_upper);
    Interval _17560 = _20233;
    Interval _17561 = x.v;
    Interval _17562 = Interval{ 0.0, 0.0 };
    Interval _20234 = jet_mul_derivative(_17561, _17562, intervalFailed, optical_product_upper);
    Interval _17563 = _20234;
    Interval _20235 = jet_add_derivative(_17560, _17563, intervalFailed);
    Interval _17564 = x.dy;
    Interval _17565 = _20219;
    Interval _20236 = jet_mul_derivative(_17564, _17565, intervalFailed, optical_product_upper);
    Interval _17566 = _20236;
    Interval _17567 = x.v;
    Interval _17568 = Interval{ 0.0, 0.0 };
    Interval _20237 = jet_mul_derivative(_17567, _17568, intervalFailed, optical_product_upper);
    Interval _17569 = _20237;
    Interval _20238 = jet_add_derivative(_17566, _17569, intervalFailed);
    float _17554 = 129.0;
    float _17555 = 1000.0;
    Interval _20244 = iratio(_17554, _17555, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20249;
    if (!intervalFailed)
    {
        _20249 = intervalFailed;
    }
    else
    {
        _20249 = false;
    }
    bool _20254;
    if (_20249)
    {
        _20254 = jetFailureSite == 0u;
    }
    else
    {
        _20254 = false;
    }
    if (_20254)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(129.0, 129.0, 1000.0, 1000.0);
    }
    Interval _17540 = z.v;
    Interval _17541 = _20244;
    Interval _20257 = imul(_17540, _17541, intervalFailed, optical_product_upper);
    Interval _17542 = z.dx;
    Interval _17543 = _20244;
    Interval _20258 = jet_mul_derivative(_17542, _17543, intervalFailed, optical_product_upper);
    Interval _17544 = _20258;
    Interval _17545 = z.v;
    Interval _17546 = Interval{ 0.0, 0.0 };
    Interval _20259 = jet_mul_derivative(_17545, _17546, intervalFailed, optical_product_upper);
    Interval _17547 = _20259;
    Interval _20260 = jet_add_derivative(_17544, _17547, intervalFailed);
    Interval _17548 = z.dy;
    Interval _17549 = _20244;
    Interval _20261 = jet_mul_derivative(_17548, _17549, intervalFailed, optical_product_upper);
    Interval _17550 = _20261;
    Interval _17551 = z.v;
    Interval _17552 = Interval{ 0.0, 0.0 };
    Interval _20262 = jet_mul_derivative(_17551, _17552, intervalFailed, optical_product_upper);
    Interval _17553 = _20262;
    Interval _20263 = jet_add_derivative(_17550, _17553, intervalFailed);
    Interval _17534 = _20232;
    Interval _17535 = _20257;
    Interval _20264 = iadd(_17534, _17535, intervalFailed);
    Interval _17536 = _20235;
    Interval _17537 = _20260;
    Interval _20265 = jet_add_derivative(_17536, _17537, intervalFailed);
    Interval _17538 = _20238;
    Interval _17539 = _20263;
    Interval _20266 = jet_add_derivative(_17538, _17539, intervalFailed);
    float _17532 = 73.0;
    float _17533 = 100.0;
    Interval _20272 = iratio(_17532, _17533, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20277;
    if (!intervalFailed)
    {
        _20277 = intervalFailed;
    }
    else
    {
        _20277 = false;
    }
    bool _20282;
    if (_20277)
    {
        _20282 = jetFailureSite == 0u;
    }
    else
    {
        _20282 = false;
    }
    if (_20282)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(73.0, 73.0, 100.0, 100.0);
    }
    Interval _17518 = t.v;
    Interval _17519 = _20272;
    Interval _20285 = imul(_17518, _17519, intervalFailed, optical_product_upper);
    Interval _17520 = t.dx;
    Interval _17521 = _20272;
    Interval _20286 = jet_mul_derivative(_17520, _17521, intervalFailed, optical_product_upper);
    Interval _17522 = _20286;
    Interval _17523 = t.v;
    Interval _17524 = Interval{ 0.0, 0.0 };
    Interval _20287 = jet_mul_derivative(_17523, _17524, intervalFailed, optical_product_upper);
    Interval _17525 = _20287;
    Interval _20288 = jet_add_derivative(_17522, _17525, intervalFailed);
    Interval _17526 = t.dy;
    Interval _17527 = _20272;
    Interval _20289 = jet_mul_derivative(_17526, _17527, intervalFailed, optical_product_upper);
    Interval _17528 = _20289;
    Interval _17529 = t.v;
    Interval _17530 = Interval{ 0.0, 0.0 };
    Interval _20290 = jet_mul_derivative(_17529, _17530, intervalFailed, optical_product_upper);
    Interval _17531 = _20290;
    Interval _20291 = jet_add_derivative(_17528, _17531, intervalFailed);
    Interval _17512 = _20264;
    Interval _17513 = Interval{ as_type<float>(as_type<uint>(_20285.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20285.lo) ^ 2147483648u) };
    Interval _20317 = iadd(_17512, _17513, intervalFailed);
    Interval _17514 = _20265;
    Interval _17515 = Interval{ as_type<float>(as_type<uint>(_20288.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20288.lo) ^ 2147483648u) };
    Interval _20319 = jet_add_derivative(_17514, _17515, intervalFailed);
    Interval _17516 = _20266;
    Interval _17517 = Interval{ as_type<float>(as_type<uint>(_20291.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20291.lo) ^ 2147483648u) };
    Interval _20321 = jet_add_derivative(_17516, _17517, intervalFailed);
    float _17510 = 216.0;
    float _17511 = 1000.0;
    Interval _20324 = iratio(_17510, _17511, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20329;
    if (!intervalFailed)
    {
        _20329 = intervalFailed;
    }
    else
    {
        _20329 = false;
    }
    bool _20334;
    if (_20329)
    {
        _20334 = jetFailureSite == 0u;
    }
    else
    {
        _20334 = false;
    }
    if (_20334)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(216.0, 216.0, 1000.0, 1000.0);
    }
    Interval _17496 = footprint.v;
    Interval _17497 = _20324;
    Interval _20340 = imul(_17496, _17497, intervalFailed, optical_product_upper);
    Interval _17498 = footprint.dx;
    Interval _17499 = _20324;
    Interval _20341 = jet_mul_derivative(_17498, _17499, intervalFailed, optical_product_upper);
    Interval _17500 = _20341;
    Interval _17501 = footprint.v;
    Interval _17502 = Interval{ 0.0, 0.0 };
    Interval _20342 = jet_mul_derivative(_17501, _17502, intervalFailed, optical_product_upper);
    Interval _17503 = _20342;
    Interval _20343 = jet_add_derivative(_17500, _17503, intervalFailed);
    Interval _17504 = footprint.dy;
    Interval _17505 = _20324;
    Interval _20344 = jet_mul_derivative(_17504, _17505, intervalFailed, optical_product_upper);
    Interval _17506 = _20344;
    Interval _17507 = footprint.v;
    Interval _17508 = Interval{ 0.0, 0.0 };
    Interval _20345 = jet_mul_derivative(_17507, _17508, intervalFailed, optical_product_upper);
    Interval _17509 = _20345;
    Interval _20346 = jet_add_derivative(_17506, _17509, intervalFailed);
    bool _20353;
    if (_20340.lo <= 0.0)
    {
        _20353 = _20340.hi >= 0.0;
    }
    else
    {
        _20353 = false;
    }
    float _20360;
    if (_20353)
    {
        _20360 = 0.0;
    }
    else
    {
        _20360 = precise::min(abs(_20340.lo), abs(_20340.hi));
    }
    float _20363 = precise::max(abs(_20340.lo), abs(_20340.hi));
    float _17486 = spvFMul(_20360, _20360);
    float _20364 = interval_down(_17486, intervalFailed);
    float _20365 = precise::max(0.0, _20364);
    float _17487 = spvFMul(_20363, _20363);
    float _20366 = interval_up(_17487, intervalFailed);
    Interval _17488 = Interval{ 2.0, 2.0 };
    Interval _17489 = _20340;
    Interval _20367 = imul(_17488, _17489, intervalFailed, optical_product_upper);
    Interval _17490 = _20367;
    Interval _17491 = _20343;
    Interval _20368 = jet_mul_derivative(_17490, _17491, intervalFailed, optical_product_upper);
    Interval _17492 = Interval{ 2.0, 2.0 };
    Interval _17493 = _20340;
    Interval _20369 = imul(_17492, _17493, intervalFailed, optical_product_upper);
    Interval _17494 = _20369;
    Interval _17495 = _20346;
    Interval _20370 = jet_mul_derivative(_17494, _17495, intervalFailed, optical_product_upper);
    bool _20375;
    if (_20365 <= 0.0)
    {
        _20375 = _20366 >= 0.0;
    }
    else
    {
        _20375 = false;
    }
    float _20382;
    if (_20375)
    {
        _20382 = 0.0;
    }
    else
    {
        _20382 = precise::min(abs(_20365), abs(_20366));
    }
    float _20385 = precise::max(abs(_20365), abs(_20366));
    float _17476 = spvFMul(_20382, _20382);
    float _20386 = interval_down(_17476, intervalFailed);
    float _17477 = spvFMul(_20385, _20385);
    float _20388 = interval_up(_17477, intervalFailed);
    Interval _17478 = Interval{ 2.0, 2.0 };
    Interval _17479 = Interval{ _20365, _20366 };
    Interval _20390 = imul(_17478, _17479, intervalFailed, optical_product_upper);
    Interval _17480 = _20390;
    Interval _17481 = _20368;
    Interval _20391 = jet_mul_derivative(_17480, _17481, intervalFailed, optical_product_upper);
    Interval _17482 = Interval{ 2.0, 2.0 };
    Interval _17483 = Interval{ _20365, _20366 };
    Interval _20393 = imul(_17482, _17483, intervalFailed, optical_product_upper);
    Interval _17484 = _20393;
    Interval _17485 = _20370;
    Interval _20394 = jet_mul_derivative(_17484, _17485, intervalFailed, optical_product_upper);
    Interval _17470 = Interval{ 1.0, 1.0 };
    Interval _17471 = Interval{ precise::max(0.0, _20386), _20388 };
    Interval _20396 = iadd(_17470, _17471, intervalFailed);
    Interval _17472 = Interval{ 0.0, 0.0 };
    Interval _17473 = _20391;
    Interval _20397 = jet_add_derivative(_17472, _17473, intervalFailed);
    Interval _17474 = Interval{ 0.0, 0.0 };
    Interval _17475 = _20394;
    Interval _20398 = jet_add_derivative(_17474, _17475, intervalFailed);
    Interval _17456 = Interval{ 1.0, 1.0 };
    Interval _17457 = _20396;
    Interval _20400 = idiv(_17456, _17457, intervalFailed, interval_divide_upper);
    bool _20407;
    if (!intervalFailed)
    {
        _20407 = intervalFailed;
    }
    else
    {
        _20407 = false;
    }
    bool _20412;
    if (_20407)
    {
        _20412 = jetFailureSite == 0u;
    }
    else
    {
        _20412 = false;
    }
    if (_20412)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _20396.lo, _20396.hi);
    }
    Interval _17458 = Interval{ 0.0, 0.0 };
    Interval _17459 = _20400;
    Interval _17460 = _20397;
    Interval _20416 = jet_mul_derivative(_17459, _17460, intervalFailed, optical_product_upper);
    Interval _17461 = Interval{ as_type<float>(as_type<uint>(_20416.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20416.lo) ^ 2147483648u) };
    Interval _20426 = jet_add_derivative(_17458, _17461, intervalFailed);
    Interval _17462 = _20426;
    Interval _17463 = _20396;
    Interval _20427 = jet_div_derivative(_17462, _17463, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17464 = Interval{ 0.0, 0.0 };
    Interval _17465 = _20400;
    Interval _17466 = _20398;
    Interval _20428 = jet_mul_derivative(_17465, _17466, intervalFailed, optical_product_upper);
    Interval _17467 = Interval{ as_type<float>(as_type<uint>(_20428.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20428.lo) ^ 2147483648u) };
    Interval _20438 = jet_add_derivative(_17464, _17467, intervalFailed);
    Interval _17468 = _20438;
    Interval _17469 = _20396;
    Interval _20439 = jet_div_derivative(_17468, _17469, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _17454 = 38.0;
    float _17455 = 100.0;
    Interval _20441 = iratio(_17454, _17455, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20446;
    if (!intervalFailed)
    {
        _20446 = intervalFailed;
    }
    else
    {
        _20446 = false;
    }
    bool _20451;
    if (_20446)
    {
        _20451 = jetFailureSite == 0u;
    }
    else
    {
        _20451 = false;
    }
    if (_20451)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(38.0, 38.0, 100.0, 100.0);
    }
    Interval _17446 = _20317;
    float _17444 = 3.1415927410125732421875;
    float _20454 = interval_down(_17444, intervalFailed);
    float _17445 = 3.1415927410125732421875;
    float _20455 = interval_up(_17445, intervalFailed);
    Interval _17447 = Interval{ _20454, _20455 };
    Interval _17448 = Interval{ 0.5, 0.5 };
    Interval _20457 = imul(_17447, _17448, intervalFailed, optical_product_upper);
    Interval _17449 = _20457;
    Interval _20458 = iadd(_17446, _17449, intervalFailed);
    float _17442 = _20458.lo;
    float _17443 = _20458.hi;
    float _20461 = sine_bounds(_17442, _17443, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17440 = _20317.lo;
    float _17441 = _20317.hi;
    float _20465 = sine_bounds(_17440, _17441, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17450 = Interval{ _20461, interval_sine_upper };
    Interval _17451 = _20319;
    Interval _20468 = jet_mul_derivative(_17450, _17451, intervalFailed, optical_product_upper);
    Interval _17452 = Interval{ _20461, interval_sine_upper };
    Interval _17453 = _20321;
    Interval _20470 = jet_mul_derivative(_17452, _17453, intervalFailed, optical_product_upper);
    Interval _17426 = _20441;
    Interval _17427 = Interval{ _20465, interval_sine_upper };
    Interval _20472 = imul(_17426, _17427, intervalFailed, optical_product_upper);
    Interval _17428 = Interval{ 0.0, 0.0 };
    Interval _17429 = Interval{ _20465, interval_sine_upper };
    Interval _20474 = jet_mul_derivative(_17428, _17429, intervalFailed, optical_product_upper);
    Interval _17430 = _20474;
    Interval _17431 = _20441;
    Interval _17432 = _20468;
    Interval _20475 = jet_mul_derivative(_17431, _17432, intervalFailed, optical_product_upper);
    Interval _17433 = _20475;
    Interval _20476 = jet_add_derivative(_17430, _17433, intervalFailed);
    Interval _17434 = Interval{ 0.0, 0.0 };
    Interval _17435 = Interval{ _20465, interval_sine_upper };
    Interval _20478 = jet_mul_derivative(_17434, _17435, intervalFailed, optical_product_upper);
    Interval _17436 = _20478;
    Interval _17437 = _20441;
    Interval _17438 = _20470;
    Interval _20479 = jet_mul_derivative(_17437, _17438, intervalFailed, optical_product_upper);
    Interval _17439 = _20479;
    Interval _20480 = jet_add_derivative(_17436, _17439, intervalFailed);
    Interval _17412 = _20472;
    Interval _17413 = _20400;
    Interval _20481 = imul(_17412, _17413, intervalFailed, optical_product_upper);
    Interval _17414 = _20476;
    Interval _17415 = _20400;
    Interval _20482 = jet_mul_derivative(_17414, _17415, intervalFailed, optical_product_upper);
    Interval _17416 = _20482;
    Interval _17417 = _20472;
    Interval _17418 = _20427;
    Interval _20483 = jet_mul_derivative(_17417, _17418, intervalFailed, optical_product_upper);
    Interval _17419 = _20483;
    Interval _20484 = jet_add_derivative(_17416, _17419, intervalFailed);
    Interval _17420 = _20480;
    Interval _17421 = _20400;
    Interval _20485 = jet_mul_derivative(_17420, _17421, intervalFailed, optical_product_upper);
    Interval _17422 = _20485;
    Interval _17423 = _20472;
    Interval _17424 = _20439;
    Interval _20486 = jet_mul_derivative(_17423, _17424, intervalFailed, optical_product_upper);
    Interval _17425 = _20486;
    Interval _20487 = jet_add_derivative(_17422, _17425, intervalFailed);
    Interval _17406 = _19699;
    Interval _17407 = _20481;
    Interval _20488 = iadd(_17406, _17407, intervalFailed);
    Interval _17408 = _19700;
    Interval _17409 = _20484;
    Interval _20489 = jet_add_derivative(_17408, _17409, intervalFailed);
    Interval _17410 = _19701;
    Interval _17411 = _20487;
    Interval _20490 = jet_add_derivative(_17410, _17411, intervalFailed);
    float _17404 = 6574.0;
    float _17405 = 100000.0;
    Interval _20498 = iratio(_17404, _17405, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20503;
    if (!intervalFailed)
    {
        _20503 = intervalFailed;
    }
    else
    {
        _20503 = false;
    }
    bool _20508;
    if (_20503)
    {
        _20508 = jetFailureSite == 0u;
    }
    else
    {
        _20508 = false;
    }
    if (_20508)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6574.0, 6574.0, 100000.0, 100000.0);
    }
    float _17398 = _20317.lo;
    float _17399 = _20317.hi;
    float _20513 = sine_bounds(_17398, _17399, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20517 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20520 = as_type<float>(as_type<uint>(_20513) ^ 2147483648u);
    Interval _17394 = _20317;
    float _17392 = 3.1415927410125732421875;
    float _20521 = interval_down(_17392, intervalFailed);
    float _17393 = 3.1415927410125732421875;
    float _20522 = interval_up(_17393, intervalFailed);
    Interval _17395 = Interval{ _20521, _20522 };
    Interval _17396 = Interval{ 0.5, 0.5 };
    Interval _20524 = imul(_17395, _17396, intervalFailed, optical_product_upper);
    Interval _17397 = _20524;
    Interval _20525 = iadd(_17394, _17397, intervalFailed);
    float _17390 = _20525.lo;
    float _17391 = _20525.hi;
    float _20528 = sine_bounds(_17390, _17391, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17400 = Interval{ _20517, _20520 };
    Interval _17401 = _20319;
    Interval _20531 = jet_mul_derivative(_17400, _17401, intervalFailed, optical_product_upper);
    Interval _17402 = Interval{ _20517, _20520 };
    Interval _17403 = _20321;
    Interval _20533 = jet_mul_derivative(_17402, _17403, intervalFailed, optical_product_upper);
    Interval _17376 = _20498;
    Interval _17377 = Interval{ _20528, interval_sine_upper };
    Interval _20535 = imul(_17376, _17377, intervalFailed, optical_product_upper);
    Interval _17378 = Interval{ 0.0, 0.0 };
    Interval _17379 = Interval{ _20528, interval_sine_upper };
    Interval _20537 = jet_mul_derivative(_17378, _17379, intervalFailed, optical_product_upper);
    Interval _17380 = _20537;
    Interval _17381 = _20498;
    Interval _17382 = _20531;
    Interval _20538 = jet_mul_derivative(_17381, _17382, intervalFailed, optical_product_upper);
    Interval _17383 = _20538;
    Interval _20539 = jet_add_derivative(_17380, _17383, intervalFailed);
    Interval _17384 = Interval{ 0.0, 0.0 };
    Interval _17385 = Interval{ _20528, interval_sine_upper };
    Interval _20541 = jet_mul_derivative(_17384, _17385, intervalFailed, optical_product_upper);
    Interval _17386 = _20541;
    Interval _17387 = _20498;
    Interval _17388 = _20533;
    Interval _20542 = jet_mul_derivative(_17387, _17388, intervalFailed, optical_product_upper);
    Interval _17389 = _20542;
    Interval _20543 = jet_add_derivative(_17386, _17389, intervalFailed);
    Interval _17362 = _20535;
    Interval _17363 = _20400;
    Interval _20544 = imul(_17362, _17363, intervalFailed, optical_product_upper);
    Interval _17364 = _20539;
    Interval _17365 = _20400;
    Interval _20545 = jet_mul_derivative(_17364, _17365, intervalFailed, optical_product_upper);
    Interval _17366 = _20545;
    Interval _17367 = _20535;
    Interval _17368 = _20427;
    Interval _20546 = jet_mul_derivative(_17367, _17368, intervalFailed, optical_product_upper);
    Interval _17369 = _20546;
    Interval _20547 = jet_add_derivative(_17366, _17369, intervalFailed);
    Interval _17370 = _20543;
    Interval _17371 = _20400;
    Interval _20548 = jet_mul_derivative(_17370, _17371, intervalFailed, optical_product_upper);
    Interval _17372 = _20548;
    Interval _17373 = _20535;
    Interval _17374 = _20439;
    Interval _20549 = jet_mul_derivative(_17373, _17374, intervalFailed, optical_product_upper);
    Interval _17375 = _20549;
    Interval _20550 = jet_add_derivative(_17372, _17375, intervalFailed);
    Interval _17356 = _19956;
    Interval _17357 = _20544;
    Interval _20551 = iadd(_17356, _17357, intervalFailed);
    Interval _17358 = _19958;
    Interval _17359 = _20547;
    Interval _20552 = jet_add_derivative(_17358, _17359, intervalFailed);
    Interval _17360 = _19960;
    Interval _17361 = _20550;
    Interval _20553 = jet_add_derivative(_17360, _17361, intervalFailed);
    float _17354 = 4902.0;
    float _17355 = 100000.0;
    Interval _20561 = iratio(_17354, _17355, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20566;
    if (!intervalFailed)
    {
        _20566 = intervalFailed;
    }
    else
    {
        _20566 = false;
    }
    bool _20571;
    if (_20566)
    {
        _20571 = jetFailureSite == 0u;
    }
    else
    {
        _20571 = false;
    }
    if (_20571)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(4902.0, 4902.0, 100000.0, 100000.0);
    }
    float _17348 = _20317.lo;
    float _17349 = _20317.hi;
    float _20576 = sine_bounds(_17348, _17349, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20580 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20583 = as_type<float>(as_type<uint>(_20576) ^ 2147483648u);
    Interval _17344 = _20317;
    float _17342 = 3.1415927410125732421875;
    float _20584 = interval_down(_17342, intervalFailed);
    float _17343 = 3.1415927410125732421875;
    float _20585 = interval_up(_17343, intervalFailed);
    Interval _17345 = Interval{ _20584, _20585 };
    Interval _17346 = Interval{ 0.5, 0.5 };
    Interval _20587 = imul(_17345, _17346, intervalFailed, optical_product_upper);
    Interval _17347 = _20587;
    Interval _20588 = iadd(_17344, _17347, intervalFailed);
    float _17340 = _20588.lo;
    float _17341 = _20588.hi;
    float _20591 = sine_bounds(_17340, _17341, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17350 = Interval{ _20580, _20583 };
    Interval _17351 = _20319;
    Interval _20594 = jet_mul_derivative(_17350, _17351, intervalFailed, optical_product_upper);
    Interval _17352 = Interval{ _20580, _20583 };
    Interval _17353 = _20321;
    Interval _20596 = jet_mul_derivative(_17352, _17353, intervalFailed, optical_product_upper);
    Interval _17326 = _20561;
    Interval _17327 = Interval{ _20591, interval_sine_upper };
    Interval _20598 = imul(_17326, _17327, intervalFailed, optical_product_upper);
    Interval _17328 = Interval{ 0.0, 0.0 };
    Interval _17329 = Interval{ _20591, interval_sine_upper };
    Interval _20600 = jet_mul_derivative(_17328, _17329, intervalFailed, optical_product_upper);
    Interval _17330 = _20600;
    Interval _17331 = _20561;
    Interval _17332 = _20594;
    Interval _20601 = jet_mul_derivative(_17331, _17332, intervalFailed, optical_product_upper);
    Interval _17333 = _20601;
    Interval _20602 = jet_add_derivative(_17330, _17333, intervalFailed);
    Interval _17334 = Interval{ 0.0, 0.0 };
    Interval _17335 = Interval{ _20591, interval_sine_upper };
    Interval _20604 = jet_mul_derivative(_17334, _17335, intervalFailed, optical_product_upper);
    Interval _17336 = _20604;
    Interval _17337 = _20561;
    Interval _17338 = _20596;
    Interval _20605 = jet_mul_derivative(_17337, _17338, intervalFailed, optical_product_upper);
    Interval _17339 = _20605;
    Interval _20606 = jet_add_derivative(_17336, _17339, intervalFailed);
    Interval _17312 = _20598;
    Interval _17313 = _20400;
    Interval _20607 = imul(_17312, _17313, intervalFailed, optical_product_upper);
    Interval _17314 = _20602;
    Interval _17315 = _20400;
    Interval _20608 = jet_mul_derivative(_17314, _17315, intervalFailed, optical_product_upper);
    Interval _17316 = _20608;
    Interval _17317 = _20598;
    Interval _17318 = _20427;
    Interval _20609 = jet_mul_derivative(_17317, _17318, intervalFailed, optical_product_upper);
    Interval _17319 = _20609;
    Interval _20610 = jet_add_derivative(_17316, _17319, intervalFailed);
    Interval _17320 = _20606;
    Interval _17321 = _20400;
    Interval _20611 = jet_mul_derivative(_17320, _17321, intervalFailed, optical_product_upper);
    Interval _17322 = _20611;
    Interval _17323 = _20598;
    Interval _17324 = _20439;
    Interval _20612 = jet_mul_derivative(_17323, _17324, intervalFailed, optical_product_upper);
    Interval _17325 = _20612;
    Interval _20613 = jet_add_derivative(_17322, _17325, intervalFailed);
    Interval _17306 = _20211;
    Interval _17307 = _20607;
    Interval _20614 = iadd(_17306, _17307, intervalFailed);
    Interval _17308 = _20212;
    Interval _17309 = _20610;
    Interval _20615 = jet_add_derivative(_17308, _17309, intervalFailed);
    Interval _17310 = _20213;
    Interval _17311 = _20613;
    Interval _20616 = jet_add_derivative(_17310, _17311, intervalFailed);
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
    float _21634;
    float _21635;
    float _21636;
    float _21637;
    float _21638;
    float _21639;
    if (footprint.v.lo < 24.0)
    {
        if (footprint.v.hi >= 24.0)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20488, _20489, _20490 }, OpticalJet{ _20551, _20552, _20553 }, OpticalJet{ _20614, _20615, _20616 } };
        }
        Interval _17292 = x.v;
        Interval _17293 = Interval{ 128.0, 128.0 };
        Interval _20638 = idiv(_17292, _17293, intervalFailed, interval_divide_upper);
        bool _20645;
        if (!intervalFailed)
        {
            _20645 = intervalFailed;
        }
        else
        {
            _20645 = false;
        }
        bool _20650;
        if (_20645)
        {
            _20650 = jetFailureSite == 0u;
        }
        else
        {
            _20650 = false;
        }
        if (_20650)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(x.v.lo, x.v.hi, 128.0, 128.0);
        }
        Interval _17294 = x.dx;
        Interval _17295 = _20638;
        Interval _17296 = Interval{ 0.0, 0.0 };
        Interval _20654 = jet_mul_derivative(_17295, _17296, intervalFailed, optical_product_upper);
        Interval _17297 = Interval{ as_type<float>(as_type<uint>(_20654.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20654.lo) ^ 2147483648u) };
        Interval _20664 = jet_add_derivative(_17294, _17297, intervalFailed);
        Interval _17298 = _20664;
        Interval _17299 = Interval{ 128.0, 128.0 };
        Interval _20665 = jet_div_derivative(_17298, _17299, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17300 = x.dy;
        Interval _17301 = _20638;
        Interval _17302 = Interval{ 0.0, 0.0 };
        Interval _20666 = jet_mul_derivative(_17301, _17302, intervalFailed, optical_product_upper);
        Interval _17303 = Interval{ as_type<float>(as_type<uint>(_20666.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20666.lo) ^ 2147483648u) };
        Interval _20676 = jet_add_derivative(_17300, _17303, intervalFailed);
        Interval _17304 = _20676;
        Interval _17305 = Interval{ 128.0, 128.0 };
        Interval _20677 = jet_div_derivative(_17304, _17305, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17278 = z.v;
        Interval _17279 = Interval{ 128.0, 128.0 };
        Interval _20685 = idiv(_17278, _17279, intervalFailed, interval_divide_upper);
        bool _20692;
        if (!intervalFailed)
        {
            _20692 = intervalFailed;
        }
        else
        {
            _20692 = false;
        }
        bool _20697;
        if (_20692)
        {
            _20697 = jetFailureSite == 0u;
        }
        else
        {
            _20697 = false;
        }
        if (_20697)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(z.v.lo, z.v.hi, 128.0, 128.0);
        }
        Interval _17280 = z.dx;
        Interval _17281 = _20685;
        Interval _17282 = Interval{ 0.0, 0.0 };
        Interval _20701 = jet_mul_derivative(_17281, _17282, intervalFailed, optical_product_upper);
        Interval _17283 = Interval{ as_type<float>(as_type<uint>(_20701.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20701.lo) ^ 2147483648u) };
        Interval _20711 = jet_add_derivative(_17280, _17283, intervalFailed);
        Interval _17284 = _20711;
        Interval _17285 = Interval{ 128.0, 128.0 };
        Interval _20712 = jet_div_derivative(_17284, _17285, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17286 = z.dy;
        Interval _17287 = _20685;
        Interval _17288 = Interval{ 0.0, 0.0 };
        Interval _20713 = jet_mul_derivative(_17287, _17288, intervalFailed, optical_product_upper);
        Interval _17289 = Interval{ as_type<float>(as_type<uint>(_20713.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20713.lo) ^ 2147483648u) };
        Interval _20723 = jet_add_derivative(_17286, _17289, intervalFailed);
        Interval _17290 = _20723;
        Interval _17291 = Interval{ 128.0, 128.0 };
        Interval _20724 = jet_div_derivative(_17290, _17291, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        float _20727 = floor(_20638.lo);
        float _20728 = floor(_20638.hi);
        bool _20733;
        if ((isunordered(_20727, _20728) || _20727 == _20728))
        {
            _20733 = floor(_20685.lo) != floor(_20685.hi);
        }
        else
        {
            _20733 = true;
        }
        float _21604;
        float _21605;
        float _21606;
        float _21607;
        float _21608;
        float _21609;
        float _21610;
        float _21611;
        float _21612;
        float _21613;
        float _21614;
        float _21615;
        float _21616;
        float _21617;
        float _21618;
        float _21619;
        float _21620;
        float _21621;
        if (_20733)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20488, _20489, _20490 }, OpticalJet{ _20551, _20552, _20553 }, OpticalJet{ _20614, _20615, _20616 } };
        }
        else
        {
            int _20739 = int(floor(_20638.lo));
            int _20741 = int(floor(_20685.lo));
            uint _20745 = (uint(_20739) * 1597334677u) ^ (uint(_20741) * 3812015801u);
            uint _1988 = (_20745 ^ (_20745 >> 16u)) * 2246822519u;
            float _17276 = float((_1988 ^ (_1988 >> 13u)) & 65535u);
            float _17277 = 65535.0;
            Interval _20752 = iratio(_17276, _17277, intervalFailed, optical_product_upper, interval_divide_upper);
            float _20753 = float(_20739);
            float _20754 = float(_20741);
            bool _20759;
            if (!intervalFailed)
            {
                _20759 = intervalFailed;
            }
            else
            {
                _20759 = false;
            }
            bool _20764;
            if (_20759)
            {
                _20764 = jetFailureSite == 0u;
            }
            else
            {
                _20764 = false;
            }
            if (_20764)
            {
                jetFailureSite = 7u;
                jetFailureArguments = float4(_20753, _20753, _20754, _20754);
            }
            float param_var_n = 64.0;
            float param_var_d = 100.0;
            Interval _20770 = iratio(param_var_n, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _20775;
            if (_20752.lo <= _20770.hi)
            {
                _20775 = _20752.hi > _20770.lo;
            }
            else
            {
                _20775 = false;
            }
            if (_20775)
            {
                jetBranchKnown = false;
                return OpticalJet3{ OpticalJet{ _20488, _20489, _20490 }, OpticalJet{ _20551, _20552, _20553 }, OpticalJet{ _20614, _20615, _20616 } };
            }
            if (_20752.lo > _20770.hi)
            {
                float _20781 = float(_20739);
                Interval _17270 = _20638;
                Interval _17271 = Interval{ as_type<float>(as_type<uint>(_20781) ^ 2147483648u), as_type<float>(as_type<uint>(_20781) ^ 2147483648u) };
                Interval _20793 = iadd(_17270, _17271, intervalFailed);
                Interval _17272 = _20665;
                Interval _17273 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20795 = jet_add_derivative(_17272, _17273, intervalFailed);
                Interval _17274 = _20677;
                Interval _17275 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20797 = jet_add_derivative(_17274, _17275, intervalFailed);
                float _17268 = 28.0;
                float _17269 = 100.0;
                Interval _20799 = iratio(_17268, _17269, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20804;
                if (!intervalFailed)
                {
                    _20804 = intervalFailed;
                }
                else
                {
                    _20804 = false;
                }
                bool _20809;
                if (_20804)
                {
                    _20809 = jetFailureSite == 0u;
                }
                else
                {
                    _20809 = false;
                }
                if (_20809)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17266 = 44.0;
                float _17267 = 100.0;
                Interval _20813 = iratio(_17266, _17267, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20818;
                if (!intervalFailed)
                {
                    _20818 = intervalFailed;
                }
                else
                {
                    _20818 = false;
                }
                bool _20823;
                if (_20818)
                {
                    _20823 = jetFailureSite == 0u;
                }
                else
                {
                    _20823 = false;
                }
                if (_20823)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1799 = _20739 + 19;
                uint _20829 = (uint(_1799) * 1597334677u) ^ (uint(_20741) * 3812015801u);
                uint _1991 = (_20829 ^ (_20829 >> 16u)) * 2246822519u;
                float _17264 = float((_1991 ^ (_1991 >> 13u)) & 65535u);
                float _17265 = 65535.0;
                Interval _20836 = iratio(_17264, _17265, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20837 = float(_1799);
                float _20838 = float(_20741);
                bool _20843;
                if (!intervalFailed)
                {
                    _20843 = intervalFailed;
                }
                else
                {
                    _20843 = false;
                }
                bool _20848;
                if (_20843)
                {
                    _20848 = jetFailureSite == 0u;
                }
                else
                {
                    _20848 = false;
                }
                if (_20848)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20837, _20837, _20838, _20838);
                }
                Interval _17250 = _20813;
                Interval _17251 = _20836;
                Interval _20852 = imul(_17250, _17251, intervalFailed, optical_product_upper);
                Interval _17252 = Interval{ 0.0, 0.0 };
                Interval _17253 = _20836;
                Interval _20853 = jet_mul_derivative(_17252, _17253, intervalFailed, optical_product_upper);
                Interval _17254 = _20853;
                Interval _17255 = _20813;
                Interval _17256 = Interval{ 0.0, 0.0 };
                Interval _20854 = jet_mul_derivative(_17255, _17256, intervalFailed, optical_product_upper);
                Interval _17257 = _20854;
                Interval _20855 = jet_add_derivative(_17254, _17257, intervalFailed);
                Interval _17258 = Interval{ 0.0, 0.0 };
                Interval _17259 = _20836;
                Interval _20856 = jet_mul_derivative(_17258, _17259, intervalFailed, optical_product_upper);
                Interval _17260 = _20856;
                Interval _17261 = _20813;
                Interval _17262 = Interval{ 0.0, 0.0 };
                Interval _20857 = jet_mul_derivative(_17261, _17262, intervalFailed, optical_product_upper);
                Interval _17263 = _20857;
                Interval _20858 = jet_add_derivative(_17260, _17263, intervalFailed);
                Interval _17244 = _20799;
                Interval _17245 = _20852;
                Interval _20859 = iadd(_17244, _17245, intervalFailed);
                Interval _17246 = Interval{ 0.0, 0.0 };
                Interval _17247 = _20855;
                Interval _20860 = jet_add_derivative(_17246, _17247, intervalFailed);
                Interval _17248 = Interval{ 0.0, 0.0 };
                Interval _17249 = _20858;
                Interval _20861 = jet_add_derivative(_17248, _17249, intervalFailed);
                Interval _17238 = _20793;
                Interval _17239 = Interval{ as_type<float>(as_type<uint>(_20859.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20859.lo) ^ 2147483648u) };
                Interval _20887 = iadd(_17238, _17239, intervalFailed);
                Interval _17240 = _20795;
                Interval _17241 = Interval{ as_type<float>(as_type<uint>(_20860.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20860.lo) ^ 2147483648u) };
                Interval _20889 = jet_add_derivative(_17240, _17241, intervalFailed);
                Interval _17242 = _20797;
                Interval _17243 = Interval{ as_type<float>(as_type<uint>(_20861.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20861.lo) ^ 2147483648u) };
                Interval _20891 = jet_add_derivative(_17242, _17243, intervalFailed);
                float _20892 = float(_20741);
                Interval _17232 = _20685;
                Interval _17233 = Interval{ as_type<float>(as_type<uint>(_20892) ^ 2147483648u), as_type<float>(as_type<uint>(_20892) ^ 2147483648u) };
                Interval _20904 = iadd(_17232, _17233, intervalFailed);
                Interval _17234 = _20712;
                Interval _17235 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20906 = jet_add_derivative(_17234, _17235, intervalFailed);
                Interval _17236 = _20724;
                Interval _17237 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20908 = jet_add_derivative(_17236, _17237, intervalFailed);
                float _17230 = 28.0;
                float _17231 = 100.0;
                Interval _20910 = iratio(_17230, _17231, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20915;
                if (!intervalFailed)
                {
                    _20915 = intervalFailed;
                }
                else
                {
                    _20915 = false;
                }
                bool _20920;
                if (_20915)
                {
                    _20920 = jetFailureSite == 0u;
                }
                else
                {
                    _20920 = false;
                }
                if (_20920)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17228 = 44.0;
                float _17229 = 100.0;
                Interval _20924 = iratio(_17228, _17229, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20929;
                if (!intervalFailed)
                {
                    _20929 = intervalFailed;
                }
                else
                {
                    _20929 = false;
                }
                bool _20934;
                if (_20929)
                {
                    _20934 = jetFailureSite == 0u;
                }
                else
                {
                    _20934 = false;
                }
                if (_20934)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1800 = _20741 + 29;
                uint _20940 = (uint(_20739) * 1597334677u) ^ (uint(_1800) * 3812015801u);
                uint _1994 = (_20940 ^ (_20940 >> 16u)) * 2246822519u;
                float _17226 = float((_1994 ^ (_1994 >> 13u)) & 65535u);
                float _17227 = 65535.0;
                Interval _20947 = iratio(_17226, _17227, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20948 = float(_20739);
                float _20949 = float(_1800);
                bool _20954;
                if (!intervalFailed)
                {
                    _20954 = intervalFailed;
                }
                else
                {
                    _20954 = false;
                }
                bool _20959;
                if (_20954)
                {
                    _20959 = jetFailureSite == 0u;
                }
                else
                {
                    _20959 = false;
                }
                if (_20959)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20948, _20948, _20949, _20949);
                }
                Interval _17212 = _20924;
                Interval _17213 = _20947;
                Interval _20963 = imul(_17212, _17213, intervalFailed, optical_product_upper);
                Interval _17214 = Interval{ 0.0, 0.0 };
                Interval _17215 = _20947;
                Interval _20964 = jet_mul_derivative(_17214, _17215, intervalFailed, optical_product_upper);
                Interval _17216 = _20964;
                Interval _17217 = _20924;
                Interval _17218 = Interval{ 0.0, 0.0 };
                Interval _20965 = jet_mul_derivative(_17217, _17218, intervalFailed, optical_product_upper);
                Interval _17219 = _20965;
                Interval _20966 = jet_add_derivative(_17216, _17219, intervalFailed);
                Interval _17220 = Interval{ 0.0, 0.0 };
                Interval _17221 = _20947;
                Interval _20967 = jet_mul_derivative(_17220, _17221, intervalFailed, optical_product_upper);
                Interval _17222 = _20967;
                Interval _17223 = _20924;
                Interval _17224 = Interval{ 0.0, 0.0 };
                Interval _20968 = jet_mul_derivative(_17223, _17224, intervalFailed, optical_product_upper);
                Interval _17225 = _20968;
                Interval _20969 = jet_add_derivative(_17222, _17225, intervalFailed);
                Interval _17206 = _20910;
                Interval _17207 = _20963;
                Interval _20970 = iadd(_17206, _17207, intervalFailed);
                Interval _17208 = Interval{ 0.0, 0.0 };
                Interval _17209 = _20966;
                Interval _20971 = jet_add_derivative(_17208, _17209, intervalFailed);
                Interval _17210 = Interval{ 0.0, 0.0 };
                Interval _17211 = _20969;
                Interval _20972 = jet_add_derivative(_17210, _17211, intervalFailed);
                Interval _17200 = _20904;
                Interval _17201 = Interval{ as_type<float>(as_type<uint>(_20970.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20970.lo) ^ 2147483648u) };
                Interval _20998 = iadd(_17200, _17201, intervalFailed);
                Interval _17202 = _20906;
                Interval _17203 = Interval{ as_type<float>(as_type<uint>(_20971.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20971.lo) ^ 2147483648u) };
                Interval _21000 = jet_add_derivative(_17202, _17203, intervalFailed);
                Interval _17204 = _20908;
                Interval _17205 = Interval{ as_type<float>(as_type<uint>(_20972.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20972.lo) ^ 2147483648u) };
                Interval _21002 = jet_add_derivative(_17204, _17205, intervalFailed);
                float _17198 = 14.0;
                float _17199 = 100.0;
                Interval _21008 = iratio(_17198, _17199, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21013;
                if (!intervalFailed)
                {
                    _21013 = intervalFailed;
                }
                else
                {
                    _21013 = false;
                }
                bool _21018;
                if (_21013)
                {
                    _21018 = jetFailureSite == 0u;
                }
                else
                {
                    _21018 = false;
                }
                if (_21018)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _21055;
                Interval _21057;
                Interval _21059;
                Interval _17184 = t.v;
                Interval _17185 = _21008;
                Interval _21021 = imul(_17184, _17185, intervalFailed, optical_product_upper);
                Interval _17186 = t.dx;
                Interval _17187 = _21008;
                Interval _21022 = jet_mul_derivative(_17186, _17187, intervalFailed, optical_product_upper);
                Interval _17188 = _21022;
                Interval _17189 = t.v;
                Interval _17190 = Interval{ 0.0, 0.0 };
                Interval _21023 = jet_mul_derivative(_17189, _17190, intervalFailed, optical_product_upper);
                Interval _17191 = _21023;
                Interval _21024 = jet_add_derivative(_17188, _17191, intervalFailed);
                Interval _17192 = t.dy;
                Interval _17193 = _21008;
                Interval _21025 = jet_mul_derivative(_17192, _17193, intervalFailed, optical_product_upper);
                Interval _17194 = _21025;
                Interval _17195 = t.v;
                Interval _17196 = Interval{ 0.0, 0.0 };
                Interval _21026 = jet_mul_derivative(_17195, _17196, intervalFailed, optical_product_upper);
                Interval _17197 = _21026;
                Interval _21027 = jet_add_derivative(_17194, _17197, intervalFailed);
                Interval _17170 = _20752;
                Interval _17171 = Interval{ 7.0, 7.0 };
                Interval _21028 = imul(_17170, _17171, intervalFailed, optical_product_upper);
                Interval _17172 = Interval{ 0.0, 0.0 };
                Interval _17173 = Interval{ 7.0, 7.0 };
                Interval _21029 = jet_mul_derivative(_17172, _17173, intervalFailed, optical_product_upper);
                Interval _17174 = _21029;
                Interval _17175 = _20752;
                Interval _17176 = Interval{ 0.0, 0.0 };
                Interval _21030 = jet_mul_derivative(_17175, _17176, intervalFailed, optical_product_upper);
                Interval _17177 = _21030;
                Interval _21031 = jet_add_derivative(_17174, _17177, intervalFailed);
                Interval _17178 = Interval{ 0.0, 0.0 };
                Interval _17179 = Interval{ 7.0, 7.0 };
                Interval _21032 = jet_mul_derivative(_17178, _17179, intervalFailed, optical_product_upper);
                Interval _17180 = _21032;
                Interval _17181 = _20752;
                Interval _17182 = Interval{ 0.0, 0.0 };
                Interval _21033 = jet_mul_derivative(_17181, _17182, intervalFailed, optical_product_upper);
                Interval _17183 = _21033;
                Interval _21034 = jet_add_derivative(_17180, _17183, intervalFailed);
                Interval _17164 = _21021;
                Interval _17165 = _21028;
                Interval _21035 = iadd(_17164, _17165, intervalFailed);
                Interval _17166 = _21024;
                Interval _17167 = _21031;
                Interval _21036 = jet_add_derivative(_17166, _17167, intervalFailed);
                Interval _17168 = _21027;
                Interval _17169 = _21034;
                Interval _21037 = jet_add_derivative(_17168, _17169, intervalFailed);
                if (floor(_21035.lo) == floor(_21035.hi))
                {
                    float _21043 = floor(_21035.lo);
                    Interval _17158 = _21035;
                    Interval _17159 = Interval{ as_type<float>(as_type<uint>(_21043) ^ 2147483648u), as_type<float>(as_type<uint>(_21043) ^ 2147483648u) };
                    _21055 = iadd(_17158, _17159, intervalFailed);
                    Interval _17160 = _21036;
                    Interval _17161 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _21057 = jet_add_derivative(_17160, _17161, intervalFailed);
                    Interval _17162 = _21037;
                    Interval _17163 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _21059 = jet_add_derivative(_17162, _17163, intervalFailed);
                }
                else
                {
                    jetBranchKnown = false;
                    return OpticalJet3{ OpticalJet{ _20488, _20489, _20490 }, OpticalJet{ _20551, _20552, _20553 }, OpticalJet{ _20614, _20615, _20616 } };
                }
                float _17156 = 3.1415927410125732421875;
                float _21064 = interval_down(_17156, intervalFailed);
                float _17157 = 3.1415927410125732421875;
                float _21065 = interval_up(_17157, intervalFailed);
                Interval _17142 = _21055;
                Interval _17143 = Interval{ _21064, _21065 };
                Interval _21067 = imul(_17142, _17143, intervalFailed, optical_product_upper);
                Interval _17144 = _21057;
                Interval _17145 = Interval{ _21064, _21065 };
                Interval _21069 = jet_mul_derivative(_17144, _17145, intervalFailed, optical_product_upper);
                Interval _17146 = _21069;
                Interval _17147 = _21055;
                Interval _17148 = Interval{ 0.0, 0.0 };
                Interval _21070 = jet_mul_derivative(_17147, _17148, intervalFailed, optical_product_upper);
                Interval _17149 = _21070;
                Interval _21071 = jet_add_derivative(_17146, _17149, intervalFailed);
                Interval _17150 = _21059;
                Interval _17151 = Interval{ _21064, _21065 };
                Interval _21073 = jet_mul_derivative(_17150, _17151, intervalFailed, optical_product_upper);
                Interval _17152 = _21073;
                Interval _17153 = _21055;
                Interval _17154 = Interval{ 0.0, 0.0 };
                Interval _21074 = jet_mul_derivative(_17153, _17154, intervalFailed, optical_product_upper);
                Interval _17155 = _21074;
                Interval _21075 = jet_add_derivative(_17152, _17155, intervalFailed);
                Interval _17134 = _21067;
                float _17132 = 3.1415927410125732421875;
                float _21076 = interval_down(_17132, intervalFailed);
                float _17133 = 3.1415927410125732421875;
                float _21077 = interval_up(_17133, intervalFailed);
                Interval _17135 = Interval{ _21076, _21077 };
                Interval _17136 = Interval{ 0.5, 0.5 };
                Interval _21079 = imul(_17135, _17136, intervalFailed, optical_product_upper);
                Interval _17137 = _21079;
                Interval _21080 = iadd(_17134, _17137, intervalFailed);
                float _17130 = _21080.lo;
                float _17131 = _21080.hi;
                float _21083 = sine_bounds(_17130, _17131, intervalFailed, optical_product_upper, interval_sine_upper);
                float _17128 = _21067.lo;
                float _17129 = _21067.hi;
                float _21087 = sine_bounds(_17128, _17129, intervalFailed, optical_product_upper, interval_sine_upper);
                Interval _17138 = Interval{ _21083, interval_sine_upper };
                Interval _17139 = _21071;
                Interval _21090 = jet_mul_derivative(_17138, _17139, intervalFailed, optical_product_upper);
                Interval _17140 = Interval{ _21083, interval_sine_upper };
                Interval _17141 = _21075;
                Interval _21092 = jet_mul_derivative(_17140, _17141, intervalFailed, optical_product_upper);
                bool _21097;
                if (_21087 <= 0.0)
                {
                    _21097 = interval_sine_upper >= 0.0;
                }
                else
                {
                    _21097 = false;
                }
                float _21104;
                if (_21097)
                {
                    _21104 = 0.0;
                }
                else
                {
                    _21104 = precise::min(abs(_21087), abs(interval_sine_upper));
                }
                float _21107 = precise::max(abs(_21087), abs(interval_sine_upper));
                float _17118 = spvFMul(_21104, _21104);
                float _21108 = interval_down(_17118, intervalFailed);
                float _21109 = precise::max(0.0, _21108);
                float _17119 = spvFMul(_21107, _21107);
                float _21110 = interval_up(_17119, intervalFailed);
                Interval _17120 = Interval{ 2.0, 2.0 };
                Interval _17121 = Interval{ _21087, interval_sine_upper };
                Interval _21112 = imul(_17120, _17121, intervalFailed, optical_product_upper);
                Interval _17122 = _21112;
                Interval _17123 = _21090;
                Interval _21113 = jet_mul_derivative(_17122, _17123, intervalFailed, optical_product_upper);
                Interval _17124 = Interval{ 2.0, 2.0 };
                Interval _17125 = Interval{ _21087, interval_sine_upper };
                Interval _21115 = imul(_17124, _17125, intervalFailed, optical_product_upper);
                Interval _17126 = _21115;
                Interval _17127 = _21092;
                Interval _21116 = jet_mul_derivative(_17126, _17127, intervalFailed, optical_product_upper);
                float _17116 = 55.0;
                float _17117 = 1000.0;
                Interval _21118 = iratio(_17116, _17117, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21123;
                if (!intervalFailed)
                {
                    _21123 = intervalFailed;
                }
                else
                {
                    _21123 = false;
                }
                bool _21128;
                if (_21123)
                {
                    _21128 = jetFailureSite == 0u;
                }
                else
                {
                    _21128 = false;
                }
                if (_21128)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
                }
                float _17114 = 14.0;
                float _17115 = 100.0;
                Interval _21132 = iratio(_17114, _17115, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21137;
                if (!intervalFailed)
                {
                    _21137 = intervalFailed;
                }
                else
                {
                    _21137 = false;
                }
                bool _21142;
                if (_21137)
                {
                    _21142 = jetFailureSite == 0u;
                }
                else
                {
                    _21142 = false;
                }
                if (_21142)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _17100 = _21132;
                Interval _17101 = _21055;
                Interval _21145 = imul(_17100, _17101, intervalFailed, optical_product_upper);
                Interval _17102 = Interval{ 0.0, 0.0 };
                Interval _17103 = _21055;
                Interval _21146 = jet_mul_derivative(_17102, _17103, intervalFailed, optical_product_upper);
                Interval _17104 = _21146;
                Interval _17105 = _21132;
                Interval _17106 = _21057;
                Interval _21147 = jet_mul_derivative(_17105, _17106, intervalFailed, optical_product_upper);
                Interval _17107 = _21147;
                Interval _21148 = jet_add_derivative(_17104, _17107, intervalFailed);
                Interval _17108 = Interval{ 0.0, 0.0 };
                Interval _17109 = _21055;
                Interval _21149 = jet_mul_derivative(_17108, _17109, intervalFailed, optical_product_upper);
                Interval _17110 = _21149;
                Interval _17111 = _21132;
                Interval _17112 = _21059;
                Interval _21150 = jet_mul_derivative(_17111, _17112, intervalFailed, optical_product_upper);
                Interval _17113 = _21150;
                Interval _21151 = jet_add_derivative(_17110, _17113, intervalFailed);
                Interval _17094 = _21118;
                Interval _17095 = _21145;
                Interval _21152 = iadd(_17094, _17095, intervalFailed);
                Interval _17096 = Interval{ 0.0, 0.0 };
                Interval _17097 = _21148;
                Interval _21153 = jet_add_derivative(_17096, _17097, intervalFailed);
                Interval _17098 = Interval{ 0.0, 0.0 };
                Interval _17099 = _21151;
                Interval _21154 = jet_add_derivative(_17098, _17099, intervalFailed);
                bool _21161;
                if (_21152.lo <= 0.0)
                {
                    _21161 = _21152.hi >= 0.0;
                }
                else
                {
                    _21161 = false;
                }
                float _21168;
                if (_21161)
                {
                    _21168 = 0.0;
                }
                else
                {
                    _21168 = precise::min(abs(_21152.lo), abs(_21152.hi));
                }
                float _21171 = precise::max(abs(_21152.lo), abs(_21152.hi));
                float _17084 = spvFMul(_21168, _21168);
                float _21172 = interval_down(_17084, intervalFailed);
                float _21173 = precise::max(0.0, _21172);
                float _17085 = spvFMul(_21171, _21171);
                float _21174 = interval_up(_17085, intervalFailed);
                Interval _17086 = Interval{ 2.0, 2.0 };
                Interval _17087 = _21152;
                Interval _21175 = imul(_17086, _17087, intervalFailed, optical_product_upper);
                Interval _17088 = _21175;
                Interval _17089 = _21153;
                Interval _21176 = jet_mul_derivative(_17088, _17089, intervalFailed, optical_product_upper);
                Interval _17090 = Interval{ 2.0, 2.0 };
                Interval _17091 = _21152;
                Interval _21177 = imul(_17090, _17091, intervalFailed, optical_product_upper);
                Interval _17092 = _21177;
                Interval _17093 = _21154;
                Interval _21178 = jet_mul_derivative(_17092, _17093, intervalFailed, optical_product_upper);
                bool _21185;
                if (_20887.lo <= 0.0)
                {
                    _21185 = _20887.hi >= 0.0;
                }
                else
                {
                    _21185 = false;
                }
                float _21192;
                if (_21185)
                {
                    _21192 = 0.0;
                }
                else
                {
                    _21192 = precise::min(abs(_20887.lo), abs(_20887.hi));
                }
                float _21195 = precise::max(abs(_20887.lo), abs(_20887.hi));
                float _17074 = spvFMul(_21192, _21192);
                float _21196 = interval_down(_17074, intervalFailed);
                float _17075 = spvFMul(_21195, _21195);
                float _21198 = interval_up(_17075, intervalFailed);
                Interval _17076 = Interval{ 2.0, 2.0 };
                Interval _17077 = _20887;
                Interval _21199 = imul(_17076, _17077, intervalFailed, optical_product_upper);
                Interval _17078 = _21199;
                Interval _17079 = _20889;
                Interval _21200 = jet_mul_derivative(_17078, _17079, intervalFailed, optical_product_upper);
                Interval _17080 = Interval{ 2.0, 2.0 };
                Interval _17081 = _20887;
                Interval _21201 = imul(_17080, _17081, intervalFailed, optical_product_upper);
                Interval _17082 = _21201;
                Interval _17083 = _20891;
                Interval _21202 = jet_mul_derivative(_17082, _17083, intervalFailed, optical_product_upper);
                bool _21209;
                if (_20998.lo <= 0.0)
                {
                    _21209 = _20998.hi >= 0.0;
                }
                else
                {
                    _21209 = false;
                }
                float _21216;
                if (_21209)
                {
                    _21216 = 0.0;
                }
                else
                {
                    _21216 = precise::min(abs(_20998.lo), abs(_20998.hi));
                }
                float _21219 = precise::max(abs(_20998.lo), abs(_20998.hi));
                float _17064 = spvFMul(_21216, _21216);
                float _21220 = interval_down(_17064, intervalFailed);
                float _17065 = spvFMul(_21219, _21219);
                float _21222 = interval_up(_17065, intervalFailed);
                Interval _17066 = Interval{ 2.0, 2.0 };
                Interval _17067 = _20998;
                Interval _21223 = imul(_17066, _17067, intervalFailed, optical_product_upper);
                Interval _17068 = _21223;
                Interval _17069 = _21000;
                Interval _21224 = jet_mul_derivative(_17068, _17069, intervalFailed, optical_product_upper);
                Interval _17070 = Interval{ 2.0, 2.0 };
                Interval _17071 = _20998;
                Interval _21225 = imul(_17070, _17071, intervalFailed, optical_product_upper);
                Interval _17072 = _21225;
                Interval _17073 = _21002;
                Interval _21226 = jet_mul_derivative(_17072, _17073, intervalFailed, optical_product_upper);
                Interval _17058 = Interval{ precise::max(0.0, _21196), _21198 };
                Interval _17059 = Interval{ precise::max(0.0, _21220), _21222 };
                Interval _21229 = iadd(_17058, _17059, intervalFailed);
                Interval _17060 = _21200;
                Interval _17061 = _21224;
                Interval _21230 = jet_add_derivative(_17060, _17061, intervalFailed);
                Interval _17062 = _21202;
                Interval _17063 = _21226;
                Interval _21231 = jet_add_derivative(_17062, _17063, intervalFailed);
                float _21234 = precise::max(0.0, _21229.lo);
                Interval _17044 = Interval{ _21234, _21229.hi };
                Interval _17045 = Interval{ _21173, _21174 };
                Interval _21238 = idiv(_17044, _17045, intervalFailed, interval_divide_upper);
                bool _21243;
                if (!intervalFailed)
                {
                    _21243 = intervalFailed;
                }
                else
                {
                    _21243 = false;
                }
                bool _21248;
                if (_21243)
                {
                    _21248 = jetFailureSite == 0u;
                }
                else
                {
                    _21248 = false;
                }
                if (_21248)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21234, _21229.hi, _21173, _21174);
                }
                Interval _17046 = _21230;
                Interval _17047 = _21238;
                Interval _17048 = _21176;
                Interval _21252 = jet_mul_derivative(_17047, _17048, intervalFailed, optical_product_upper);
                Interval _17049 = Interval{ as_type<float>(as_type<uint>(_21252.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21252.lo) ^ 2147483648u) };
                Interval _21262 = jet_add_derivative(_17046, _17049, intervalFailed);
                Interval _17050 = _21262;
                Interval _17051 = Interval{ _21173, _21174 };
                Interval _21264 = jet_div_derivative(_17050, _17051, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _17052 = _21231;
                Interval _17053 = _21238;
                Interval _17054 = _21178;
                Interval _21265 = jet_mul_derivative(_17053, _17054, intervalFailed, optical_product_upper);
                Interval _17055 = Interval{ as_type<float>(as_type<uint>(_21265.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21265.lo) ^ 2147483648u) };
                Interval _21275 = jet_add_derivative(_17052, _17055, intervalFailed);
                Interval _17056 = _21275;
                Interval _17057 = Interval{ _21173, _21174 };
                Interval _21277 = jet_div_derivative(_17056, _17057, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _17038 = Interval{ 1.0, 1.0 };
                Interval _17039 = Interval{ as_type<float>(as_type<uint>(_21238.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21238.lo) ^ 2147483648u) };
                Interval _21303 = iadd(_17038, _17039, intervalFailed);
                Interval _17040 = Interval{ 0.0, 0.0 };
                Interval _17041 = Interval{ as_type<float>(as_type<uint>(_21264.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21264.lo) ^ 2147483648u) };
                Interval _21305 = jet_add_derivative(_17040, _17041, intervalFailed);
                Interval _17042 = Interval{ 0.0, 0.0 };
                Interval _17043 = Interval{ as_type<float>(as_type<uint>(_21277.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21277.lo) ^ 2147483648u) };
                Interval _21307 = jet_add_derivative(_17042, _17043, intervalFailed);
                OpticalJet _17034 = OpticalJet{ _21303, _21305, _21307 };
                OpticalJet _17035 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _17036 = jmax(_17034, _17035);
                OpticalJet _17037 = OpticalJet{ Interval{ 1.0, 1.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _21310 = jmin(_17036, _17037);
                Interval _17020 = Interval{ 10.0, 10.0 };
                Interval _17021 = Interval{ _21109, _21110 };
                Interval _21312 = imul(_17020, _17021, intervalFailed, optical_product_upper);
                Interval _17022 = Interval{ 0.0, 0.0 };
                Interval _17023 = Interval{ _21109, _21110 };
                Interval _21314 = jet_mul_derivative(_17022, _17023, intervalFailed, optical_product_upper);
                Interval _17024 = _21314;
                Interval _17025 = Interval{ 10.0, 10.0 };
                Interval _17026 = _21113;
                Interval _21315 = jet_mul_derivative(_17025, _17026, intervalFailed, optical_product_upper);
                Interval _17027 = _21315;
                Interval _21316 = jet_add_derivative(_17024, _17027, intervalFailed);
                Interval _17028 = Interval{ 0.0, 0.0 };
                Interval _17029 = Interval{ _21109, _21110 };
                Interval _21318 = jet_mul_derivative(_17028, _17029, intervalFailed, optical_product_upper);
                Interval _17030 = _21318;
                Interval _17031 = Interval{ 10.0, 10.0 };
                Interval _17032 = _21116;
                Interval _21319 = jet_mul_derivative(_17031, _17032, intervalFailed, optical_product_upper);
                Interval _17033 = _21319;
                Interval _21320 = jet_add_derivative(_17030, _17033, intervalFailed);
                float _17018 = 18.0;
                float _17019 = 100.0;
                Interval _21323 = iratio(_17018, _17019, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21328;
                if (!intervalFailed)
                {
                    _21328 = intervalFailed;
                }
                else
                {
                    _21328 = false;
                }
                bool _21333;
                if (_21328)
                {
                    _21333 = jetFailureSite == 0u;
                }
                else
                {
                    _21333 = false;
                }
                if (_21333)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(18.0, 18.0, 100.0, 100.0);
                }
                Interval _17004 = footprint.v;
                Interval _17005 = _21323;
                Interval _21339 = imul(_17004, _17005, intervalFailed, optical_product_upper);
                Interval _17006 = footprint.dx;
                Interval _17007 = _21323;
                Interval _21340 = jet_mul_derivative(_17006, _17007, intervalFailed, optical_product_upper);
                Interval _17008 = _21340;
                Interval _17009 = footprint.v;
                Interval _17010 = Interval{ 0.0, 0.0 };
                Interval _21341 = jet_mul_derivative(_17009, _17010, intervalFailed, optical_product_upper);
                Interval _17011 = _21341;
                Interval _21342 = jet_add_derivative(_17008, _17011, intervalFailed);
                Interval _17012 = footprint.dy;
                Interval _17013 = _21323;
                Interval _21343 = jet_mul_derivative(_17012, _17013, intervalFailed, optical_product_upper);
                Interval _17014 = _21343;
                Interval _17015 = footprint.v;
                Interval _17016 = Interval{ 0.0, 0.0 };
                Interval _21344 = jet_mul_derivative(_17015, _17016, intervalFailed, optical_product_upper);
                Interval _17017 = _21344;
                Interval _21345 = jet_add_derivative(_17014, _17017, intervalFailed);
                bool _21352;
                if (_21339.lo <= 0.0)
                {
                    _21352 = _21339.hi >= 0.0;
                }
                else
                {
                    _21352 = false;
                }
                float _21359;
                if (_21352)
                {
                    _21359 = 0.0;
                }
                else
                {
                    _21359 = precise::min(abs(_21339.lo), abs(_21339.hi));
                }
                float _21362 = precise::max(abs(_21339.lo), abs(_21339.hi));
                float _16994 = spvFMul(_21359, _21359);
                float _21363 = interval_down(_16994, intervalFailed);
                float _21364 = precise::max(0.0, _21363);
                float _16995 = spvFMul(_21362, _21362);
                float _21365 = interval_up(_16995, intervalFailed);
                Interval _16996 = Interval{ 2.0, 2.0 };
                Interval _16997 = _21339;
                Interval _21366 = imul(_16996, _16997, intervalFailed, optical_product_upper);
                Interval _16998 = _21366;
                Interval _16999 = _21342;
                Interval _21367 = jet_mul_derivative(_16998, _16999, intervalFailed, optical_product_upper);
                Interval _17000 = Interval{ 2.0, 2.0 };
                Interval _17001 = _21339;
                Interval _21368 = imul(_17000, _17001, intervalFailed, optical_product_upper);
                Interval _17002 = _21368;
                Interval _17003 = _21345;
                Interval _21369 = jet_mul_derivative(_17002, _17003, intervalFailed, optical_product_upper);
                bool _21374;
                if (_21364 <= 0.0)
                {
                    _21374 = _21365 >= 0.0;
                }
                else
                {
                    _21374 = false;
                }
                float _21381;
                if (_21374)
                {
                    _21381 = 0.0;
                }
                else
                {
                    _21381 = precise::min(abs(_21364), abs(_21365));
                }
                float _21384 = precise::max(abs(_21364), abs(_21365));
                float _16984 = spvFMul(_21381, _21381);
                float _21385 = interval_down(_16984, intervalFailed);
                float _16985 = spvFMul(_21384, _21384);
                float _21387 = interval_up(_16985, intervalFailed);
                Interval _16986 = Interval{ 2.0, 2.0 };
                Interval _16987 = Interval{ _21364, _21365 };
                Interval _21389 = imul(_16986, _16987, intervalFailed, optical_product_upper);
                Interval _16988 = _21389;
                Interval _16989 = _21367;
                Interval _21390 = jet_mul_derivative(_16988, _16989, intervalFailed, optical_product_upper);
                Interval _16990 = Interval{ 2.0, 2.0 };
                Interval _16991 = Interval{ _21364, _21365 };
                Interval _21392 = imul(_16990, _16991, intervalFailed, optical_product_upper);
                Interval _16992 = _21392;
                Interval _16993 = _21369;
                Interval _21393 = jet_mul_derivative(_16992, _16993, intervalFailed, optical_product_upper);
                Interval _16978 = Interval{ 1.0, 1.0 };
                Interval _16979 = Interval{ precise::max(0.0, _21385), _21387 };
                Interval _21395 = iadd(_16978, _16979, intervalFailed);
                Interval _16980 = Interval{ 0.0, 0.0 };
                Interval _16981 = _21390;
                Interval _21396 = jet_add_derivative(_16980, _16981, intervalFailed);
                Interval _16982 = Interval{ 0.0, 0.0 };
                Interval _16983 = _21393;
                Interval _21397 = jet_add_derivative(_16982, _16983, intervalFailed);
                Interval _16964 = Interval{ 1.0, 1.0 };
                Interval _16965 = _21395;
                Interval _21399 = idiv(_16964, _16965, intervalFailed, interval_divide_upper);
                bool _21406;
                if (!intervalFailed)
                {
                    _21406 = intervalFailed;
                }
                else
                {
                    _21406 = false;
                }
                bool _21411;
                if (_21406)
                {
                    _21411 = jetFailureSite == 0u;
                }
                else
                {
                    _21411 = false;
                }
                if (_21411)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _21395.lo, _21395.hi);
                }
                Interval _16966 = Interval{ 0.0, 0.0 };
                Interval _16967 = _21399;
                Interval _16968 = _21396;
                Interval _21415 = jet_mul_derivative(_16967, _16968, intervalFailed, optical_product_upper);
                Interval _16969 = Interval{ as_type<float>(as_type<uint>(_21415.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21415.lo) ^ 2147483648u) };
                Interval _21425 = jet_add_derivative(_16966, _16969, intervalFailed);
                Interval _16970 = _21425;
                Interval _16971 = _21395;
                Interval _21426 = jet_div_derivative(_16970, _16971, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16972 = Interval{ 0.0, 0.0 };
                Interval _16973 = _21399;
                Interval _16974 = _21397;
                Interval _21427 = jet_mul_derivative(_16973, _16974, intervalFailed, optical_product_upper);
                Interval _16975 = Interval{ as_type<float>(as_type<uint>(_21427.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21427.lo) ^ 2147483648u) };
                Interval _21437 = jet_add_derivative(_16972, _16975, intervalFailed);
                Interval _16976 = _21437;
                Interval _16977 = _21395;
                Interval _21438 = jet_div_derivative(_16976, _16977, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16950 = _21312;
                Interval _16951 = _21399;
                Interval _21439 = imul(_16950, _16951, intervalFailed, optical_product_upper);
                Interval _16952 = _21316;
                Interval _16953 = _21399;
                Interval _21440 = jet_mul_derivative(_16952, _16953, intervalFailed, optical_product_upper);
                Interval _16954 = _21440;
                Interval _16955 = _21312;
                Interval _16956 = _21426;
                Interval _21441 = jet_mul_derivative(_16955, _16956, intervalFailed, optical_product_upper);
                Interval _16957 = _21441;
                Interval _21442 = jet_add_derivative(_16954, _16957, intervalFailed);
                Interval _16958 = _21320;
                Interval _16959 = _21399;
                Interval _21443 = jet_mul_derivative(_16958, _16959, intervalFailed, optical_product_upper);
                Interval _16960 = _21443;
                Interval _16961 = _21312;
                Interval _16962 = _21438;
                Interval _21444 = jet_mul_derivative(_16961, _16962, intervalFailed, optical_product_upper);
                Interval _16963 = _21444;
                Interval _21445 = jet_add_derivative(_16960, _16963, intervalFailed);
                Interval _21446 = _21310.v;
                float _21449 = _21446.lo;
                float _21450 = _21446.hi;
                bool _21455;
                if (_21449 <= 0.0)
                {
                    _21455 = _21450 >= 0.0;
                }
                else
                {
                    _21455 = false;
                }
                float _21462;
                if (_21455)
                {
                    _21462 = 0.0;
                }
                else
                {
                    _21462 = precise::min(abs(_21449), abs(_21450));
                }
                float _21465 = precise::max(abs(_21449), abs(_21450));
                float _16940 = spvFMul(_21462, _21462);
                float _21466 = interval_down(_16940, intervalFailed);
                float _21467 = precise::max(0.0, _21466);
                float _16941 = spvFMul(_21465, _21465);
                float _21468 = interval_up(_16941, intervalFailed);
                Interval _16942 = Interval{ 2.0, 2.0 };
                Interval _16943 = _21446;
                Interval _21469 = imul(_16942, _16943, intervalFailed, optical_product_upper);
                Interval _16944 = _21469;
                Interval _16945 = _21310.dx;
                Interval _21470 = jet_mul_derivative(_16944, _16945, intervalFailed, optical_product_upper);
                Interval _16946 = Interval{ 2.0, 2.0 };
                Interval _16947 = _21446;
                Interval _21471 = imul(_16946, _16947, intervalFailed, optical_product_upper);
                Interval _16948 = _21471;
                Interval _16949 = _21310.dy;
                Interval _21472 = jet_mul_derivative(_16948, _16949, intervalFailed, optical_product_upper);
                Interval _16926 = _21439;
                Interval _16927 = Interval{ _21467, _21468 };
                Interval _21474 = imul(_16926, _16927, intervalFailed, optical_product_upper);
                Interval _16928 = _21442;
                Interval _16929 = Interval{ _21467, _21468 };
                Interval _21476 = jet_mul_derivative(_16928, _16929, intervalFailed, optical_product_upper);
                Interval _16930 = _21476;
                Interval _16931 = _21439;
                Interval _16932 = _21470;
                Interval _21477 = jet_mul_derivative(_16931, _16932, intervalFailed, optical_product_upper);
                Interval _16933 = _21477;
                Interval _21478 = jet_add_derivative(_16930, _16933, intervalFailed);
                Interval _16934 = _21445;
                Interval _16935 = Interval{ _21467, _21468 };
                Interval _21480 = jet_mul_derivative(_16934, _16935, intervalFailed, optical_product_upper);
                Interval _16936 = _21480;
                Interval _16937 = _21439;
                Interval _16938 = _21472;
                Interval _21481 = jet_mul_derivative(_16937, _16938, intervalFailed, optical_product_upper);
                Interval _16939 = _21481;
                Interval _21482 = jet_add_derivative(_16936, _16939, intervalFailed);
                Interval _21483 = _21310.v;
                Interval _16912 = _21474;
                Interval _16913 = _21483;
                Interval _21486 = imul(_16912, _16913, intervalFailed, optical_product_upper);
                Interval _16914 = _21478;
                Interval _16915 = _21483;
                Interval _21487 = jet_mul_derivative(_16914, _16915, intervalFailed, optical_product_upper);
                Interval _16916 = _21487;
                Interval _16917 = _21474;
                Interval _16918 = _21310.dx;
                Interval _21488 = jet_mul_derivative(_16917, _16918, intervalFailed, optical_product_upper);
                Interval _16919 = _21488;
                Interval _21489 = jet_add_derivative(_16916, _16919, intervalFailed);
                Interval _16920 = _21482;
                Interval _16921 = _21483;
                Interval _21490 = jet_mul_derivative(_16920, _16921, intervalFailed, optical_product_upper);
                Interval _16922 = _21490;
                Interval _16923 = _21474;
                Interval _16924 = _21310.dy;
                Interval _21491 = jet_mul_derivative(_16923, _16924, intervalFailed, optical_product_upper);
                Interval _16925 = _21491;
                Interval _21492 = jet_add_derivative(_16922, _16925, intervalFailed);
                Interval _16898 = Interval{ -6.0, -6.0 };
                Interval _16899 = _21439;
                Interval _21493 = imul(_16898, _16899, intervalFailed, optical_product_upper);
                Interval _16900 = Interval{ 0.0, 0.0 };
                Interval _16901 = _21439;
                Interval _21494 = jet_mul_derivative(_16900, _16901, intervalFailed, optical_product_upper);
                Interval _16902 = _21494;
                Interval _16903 = Interval{ -6.0, -6.0 };
                Interval _16904 = _21442;
                Interval _21495 = jet_mul_derivative(_16903, _16904, intervalFailed, optical_product_upper);
                Interval _16905 = _21495;
                Interval _21496 = jet_add_derivative(_16902, _16905, intervalFailed);
                Interval _16906 = Interval{ 0.0, 0.0 };
                Interval _16907 = _21439;
                Interval _21497 = jet_mul_derivative(_16906, _16907, intervalFailed, optical_product_upper);
                Interval _16908 = _21497;
                Interval _16909 = Interval{ -6.0, -6.0 };
                Interval _16910 = _21445;
                Interval _21498 = jet_mul_derivative(_16909, _16910, intervalFailed, optical_product_upper);
                Interval _16911 = _21498;
                Interval _21499 = jet_add_derivative(_16908, _16911, intervalFailed);
                Interval _16884 = _21493;
                Interval _16885 = Interval{ _21467, _21468 };
                Interval _21501 = imul(_16884, _16885, intervalFailed, optical_product_upper);
                Interval _16886 = _21496;
                Interval _16887 = Interval{ _21467, _21468 };
                Interval _21503 = jet_mul_derivative(_16886, _16887, intervalFailed, optical_product_upper);
                Interval _16888 = _21503;
                Interval _16889 = _21493;
                Interval _16890 = _21470;
                Interval _21504 = jet_mul_derivative(_16889, _16890, intervalFailed, optical_product_upper);
                Interval _16891 = _21504;
                Interval _21505 = jet_add_derivative(_16888, _16891, intervalFailed);
                Interval _16892 = _21499;
                Interval _16893 = Interval{ _21467, _21468 };
                Interval _21507 = jet_mul_derivative(_16892, _16893, intervalFailed, optical_product_upper);
                Interval _16894 = _21507;
                Interval _16895 = _21493;
                Interval _16896 = _21472;
                Interval _21508 = jet_mul_derivative(_16895, _16896, intervalFailed, optical_product_upper);
                Interval _16897 = _21508;
                Interval _21509 = jet_add_derivative(_16894, _16897, intervalFailed);
                Interval _16870 = Interval{ 128.0, 128.0 };
                Interval _16871 = Interval{ _21173, _21174 };
                Interval _21511 = imul(_16870, _16871, intervalFailed, optical_product_upper);
                Interval _16872 = Interval{ 0.0, 0.0 };
                Interval _16873 = Interval{ _21173, _21174 };
                Interval _21513 = jet_mul_derivative(_16872, _16873, intervalFailed, optical_product_upper);
                Interval _16874 = _21513;
                Interval _16875 = Interval{ 128.0, 128.0 };
                Interval _16876 = _21176;
                Interval _21514 = jet_mul_derivative(_16875, _16876, intervalFailed, optical_product_upper);
                Interval _16877 = _21514;
                Interval _21515 = jet_add_derivative(_16874, _16877, intervalFailed);
                Interval _16878 = Interval{ 0.0, 0.0 };
                Interval _16879 = Interval{ _21173, _21174 };
                Interval _21517 = jet_mul_derivative(_16878, _16879, intervalFailed, optical_product_upper);
                Interval _16880 = _21517;
                Interval _16881 = Interval{ 128.0, 128.0 };
                Interval _16882 = _21178;
                Interval _21518 = jet_mul_derivative(_16881, _16882, intervalFailed, optical_product_upper);
                Interval _16883 = _21518;
                Interval _21519 = jet_add_derivative(_16880, _16883, intervalFailed);
                Interval _16856 = _21501;
                Interval _16857 = _21511;
                Interval _21521 = idiv(_16856, _16857, intervalFailed, interval_divide_upper);
                bool _21530;
                if (!intervalFailed)
                {
                    _21530 = intervalFailed;
                }
                else
                {
                    _21530 = false;
                }
                bool _21535;
                if (_21530)
                {
                    _21535 = jetFailureSite == 0u;
                }
                else
                {
                    _21535 = false;
                }
                if (_21535)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21501.lo, _21501.hi, _21511.lo, _21511.hi);
                }
                Interval _16858 = _21505;
                Interval _16859 = _21521;
                Interval _16860 = _21515;
                Interval _21539 = jet_mul_derivative(_16859, _16860, intervalFailed, optical_product_upper);
                Interval _16861 = Interval{ as_type<float>(as_type<uint>(_21539.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21539.lo) ^ 2147483648u) };
                Interval _21549 = jet_add_derivative(_16858, _16861, intervalFailed);
                Interval _16862 = _21549;
                Interval _16863 = _21511;
                Interval _21550 = jet_div_derivative(_16862, _16863, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16864 = _21509;
                Interval _16865 = _21521;
                Interval _16866 = _21519;
                Interval _21551 = jet_mul_derivative(_16865, _16866, intervalFailed, optical_product_upper);
                Interval _16867 = Interval{ as_type<float>(as_type<uint>(_21551.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21551.lo) ^ 2147483648u) };
                Interval _21561 = jet_add_derivative(_16864, _16867, intervalFailed);
                Interval _16868 = _21561;
                Interval _16869 = _21511;
                Interval _21562 = jet_div_derivative(_16868, _16869, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16842 = _21521;
                Interval _16843 = _20887;
                Interval _21563 = imul(_16842, _16843, intervalFailed, optical_product_upper);
                Interval _16844 = _21550;
                Interval _16845 = _20887;
                Interval _21564 = jet_mul_derivative(_16844, _16845, intervalFailed, optical_product_upper);
                Interval _16846 = _21564;
                Interval _16847 = _21521;
                Interval _16848 = _20889;
                Interval _21565 = jet_mul_derivative(_16847, _16848, intervalFailed, optical_product_upper);
                Interval _16849 = _21565;
                Interval _21566 = jet_add_derivative(_16846, _16849, intervalFailed);
                Interval _16850 = _21562;
                Interval _16851 = _20887;
                Interval _21567 = jet_mul_derivative(_16850, _16851, intervalFailed, optical_product_upper);
                Interval _16852 = _21567;
                Interval _16853 = _21521;
                Interval _16854 = _20891;
                Interval _21568 = jet_mul_derivative(_16853, _16854, intervalFailed, optical_product_upper);
                Interval _16855 = _21568;
                Interval _21569 = jet_add_derivative(_16852, _16855, intervalFailed);
                Interval _16828 = _21521;
                Interval _16829 = _20998;
                Interval _21570 = imul(_16828, _16829, intervalFailed, optical_product_upper);
                Interval _16830 = _21550;
                Interval _16831 = _20998;
                Interval _21571 = jet_mul_derivative(_16830, _16831, intervalFailed, optical_product_upper);
                Interval _16832 = _21571;
                Interval _16833 = _21521;
                Interval _16834 = _21000;
                Interval _21572 = jet_mul_derivative(_16833, _16834, intervalFailed, optical_product_upper);
                Interval _16835 = _21572;
                Interval _21573 = jet_add_derivative(_16832, _16835, intervalFailed);
                Interval _16836 = _21562;
                Interval _16837 = _20998;
                Interval _21574 = jet_mul_derivative(_16836, _16837, intervalFailed, optical_product_upper);
                Interval _16838 = _21574;
                Interval _16839 = _21521;
                Interval _16840 = _21002;
                Interval _21575 = jet_mul_derivative(_16839, _16840, intervalFailed, optical_product_upper);
                Interval _16841 = _21575;
                Interval _21576 = jet_add_derivative(_16838, _16841, intervalFailed);
                Interval _16822 = _20488;
                Interval _16823 = _21486;
                Interval _21577 = iadd(_16822, _16823, intervalFailed);
                Interval _16824 = _20489;
                Interval _16825 = _21489;
                Interval _21578 = jet_add_derivative(_16824, _16825, intervalFailed);
                Interval _16826 = _20490;
                Interval _16827 = _21492;
                Interval _21579 = jet_add_derivative(_16826, _16827, intervalFailed);
                Interval _16816 = _20551;
                Interval _16817 = _21563;
                Interval _21586 = iadd(_16816, _16817, intervalFailed);
                Interval _16818 = _20552;
                Interval _16819 = _21566;
                Interval _21587 = jet_add_derivative(_16818, _16819, intervalFailed);
                Interval _16820 = _20553;
                Interval _16821 = _21569;
                Interval _21588 = jet_add_derivative(_16820, _16821, intervalFailed);
                Interval _16810 = _20614;
                Interval _16811 = _21570;
                Interval _21595 = iadd(_16810, _16811, intervalFailed);
                Interval _16812 = _20615;
                Interval _16813 = _21573;
                Interval _21596 = jet_add_derivative(_16812, _16813, intervalFailed);
                Interval _16814 = _20616;
                Interval _16815 = _21576;
                Interval _21597 = jet_add_derivative(_16814, _16815, intervalFailed);
                _21604 = _21595.lo;
                _21605 = _21595.hi;
                _21606 = _21596.lo;
                _21607 = _21596.hi;
                _21608 = _21597.lo;
                _21609 = _21597.hi;
                _21610 = _21586.lo;
                _21611 = _21586.hi;
                _21612 = _21587.lo;
                _21613 = _21587.hi;
                _21614 = _21588.lo;
                _21615 = _21588.hi;
                _21616 = _21577.lo;
                _21617 = _21577.hi;
                _21618 = _21578.lo;
                _21619 = _21578.hi;
                _21620 = _21579.lo;
                _21621 = _21579.hi;
            }
            else
            {
                _21604 = _20614.lo;
                _21605 = _20614.hi;
                _21606 = _20615.lo;
                _21607 = _20615.hi;
                _21608 = _20616.lo;
                _21609 = _20616.hi;
                _21610 = _20551.lo;
                _21611 = _20551.hi;
                _21612 = _20552.lo;
                _21613 = _20552.hi;
                _21614 = _20553.lo;
                _21615 = _20553.hi;
                _21616 = _20488.lo;
                _21617 = _20488.hi;
                _21618 = _20489.lo;
                _21619 = _20489.hi;
                _21620 = _20490.lo;
                _21621 = _20490.hi;
            }
        }
        _21622 = _21604;
        _21623 = _21605;
        _21624 = _21606;
        _21625 = _21607;
        _21626 = _21608;
        _21627 = _21609;
        _21628 = _21610;
        _21629 = _21611;
        _21630 = _21612;
        _21631 = _21613;
        _21632 = _21614;
        _21633 = _21615;
        _21634 = _21616;
        _21635 = _21617;
        _21636 = _21618;
        _21637 = _21619;
        _21638 = _21620;
        _21639 = _21621;
    }
    else
    {
        _21622 = _20614.lo;
        _21623 = _20614.hi;
        _21624 = _20615.lo;
        _21625 = _20615.hi;
        _21626 = _20616.lo;
        _21627 = _20616.hi;
        _21628 = _20551.lo;
        _21629 = _20551.hi;
        _21630 = _20552.lo;
        _21631 = _20552.hi;
        _21632 = _20553.lo;
        _21633 = _20553.hi;
        _21634 = _20488.lo;
        _21635 = _20488.hi;
        _21636 = _20489.lo;
        _21637 = _20489.hi;
        _21638 = _20490.lo;
        _21639 = _20490.hi;
    }
    return OpticalJet3{ OpticalJet{ Interval{ _21634, _21635 }, Interval{ _21636, _21637 }, Interval{ _21638, _21639 } }, OpticalJet{ Interval{ _21628, _21629 }, Interval{ _21630, _21631 }, Interval{ _21632, _21633 } }, OpticalJet{ Interval{ _21622, _21623 }, Interval{ _21624, _21625 }, Interval{ _21626, _21627 } } };
}

static __attribute__((noinline))
OpticalJet3 jet_liquid_normal(thread const ReflectionLiquidFrame& f, thread const OpticalJet3& direction, thread const OpticalJet& _distance, thread const OpticalJet& footprint, thread OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    Interval _13572 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13573 = hit.x.v;
    Interval _13601 = imul(_13572, _13573, intervalFailed, optical_product_upper);
    Interval _13574 = Interval{ 0.0, 0.0 };
    Interval _13575 = hit.x.v;
    Interval _13602 = jet_mul_derivative(_13574, _13575, intervalFailed, optical_product_upper);
    Interval _13576 = _13602;
    Interval _13577 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13578 = hit.x.dx;
    Interval _13604 = jet_mul_derivative(_13577, _13578, intervalFailed, optical_product_upper);
    Interval _13579 = _13604;
    Interval _13605 = jet_add_derivative(_13576, _13579, intervalFailed);
    Interval _13580 = Interval{ 0.0, 0.0 };
    Interval _13581 = hit.x.v;
    Interval _13606 = jet_mul_derivative(_13580, _13581, intervalFailed, optical_product_upper);
    Interval _13582 = _13606;
    Interval _13583 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13584 = hit.x.dy;
    Interval _13608 = jet_mul_derivative(_13583, _13584, intervalFailed, optical_product_upper);
    Interval _13585 = _13608;
    Interval _13609 = jet_add_derivative(_13582, _13585, intervalFailed);
    Interval _13558 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13559 = hit.x.v;
    Interval _13614 = imul(_13558, _13559, intervalFailed, optical_product_upper);
    Interval _13560 = Interval{ 0.0, 0.0 };
    Interval _13561 = hit.x.v;
    Interval _13615 = jet_mul_derivative(_13560, _13561, intervalFailed, optical_product_upper);
    Interval _13562 = _13615;
    Interval _13563 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13564 = hit.x.dx;
    Interval _13617 = jet_mul_derivative(_13563, _13564, intervalFailed, optical_product_upper);
    Interval _13565 = _13617;
    Interval _13618 = jet_add_derivative(_13562, _13565, intervalFailed);
    Interval _13566 = Interval{ 0.0, 0.0 };
    Interval _13567 = hit.x.v;
    Interval _13619 = jet_mul_derivative(_13566, _13567, intervalFailed, optical_product_upper);
    Interval _13568 = _13619;
    Interval _13569 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13570 = hit.x.dy;
    Interval _13621 = jet_mul_derivative(_13569, _13570, intervalFailed, optical_product_upper);
    Interval _13571 = _13621;
    Interval _13622 = jet_add_derivative(_13568, _13571, intervalFailed);
    Interval _13544 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13545 = hit.x.v;
    Interval _13627 = imul(_13544, _13545, intervalFailed, optical_product_upper);
    Interval _13546 = Interval{ 0.0, 0.0 };
    Interval _13547 = hit.x.v;
    Interval _13628 = jet_mul_derivative(_13546, _13547, intervalFailed, optical_product_upper);
    Interval _13548 = _13628;
    Interval _13549 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13550 = hit.x.dx;
    Interval _13630 = jet_mul_derivative(_13549, _13550, intervalFailed, optical_product_upper);
    Interval _13551 = _13630;
    Interval _13631 = jet_add_derivative(_13548, _13551, intervalFailed);
    Interval _13552 = Interval{ 0.0, 0.0 };
    Interval _13553 = hit.x.v;
    Interval _13632 = jet_mul_derivative(_13552, _13553, intervalFailed, optical_product_upper);
    Interval _13554 = _13632;
    Interval _13555 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13556 = hit.x.dy;
    Interval _13634 = jet_mul_derivative(_13555, _13556, intervalFailed, optical_product_upper);
    Interval _13557 = _13634;
    Interval _13635 = jet_add_derivative(_13554, _13557, intervalFailed);
    Interval _13530 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13531 = hit.y.v;
    Interval _13643 = imul(_13530, _13531, intervalFailed, optical_product_upper);
    Interval _13532 = Interval{ 0.0, 0.0 };
    Interval _13533 = hit.y.v;
    Interval _13644 = jet_mul_derivative(_13532, _13533, intervalFailed, optical_product_upper);
    Interval _13534 = _13644;
    Interval _13535 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13536 = hit.y.dx;
    Interval _13646 = jet_mul_derivative(_13535, _13536, intervalFailed, optical_product_upper);
    Interval _13537 = _13646;
    Interval _13647 = jet_add_derivative(_13534, _13537, intervalFailed);
    Interval _13538 = Interval{ 0.0, 0.0 };
    Interval _13539 = hit.y.v;
    Interval _13648 = jet_mul_derivative(_13538, _13539, intervalFailed, optical_product_upper);
    Interval _13540 = _13648;
    Interval _13541 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13542 = hit.y.dy;
    Interval _13650 = jet_mul_derivative(_13541, _13542, intervalFailed, optical_product_upper);
    Interval _13543 = _13650;
    Interval _13651 = jet_add_derivative(_13540, _13543, intervalFailed);
    Interval _13516 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13517 = hit.y.v;
    Interval _13656 = imul(_13516, _13517, intervalFailed, optical_product_upper);
    Interval _13518 = Interval{ 0.0, 0.0 };
    Interval _13519 = hit.y.v;
    Interval _13657 = jet_mul_derivative(_13518, _13519, intervalFailed, optical_product_upper);
    Interval _13520 = _13657;
    Interval _13521 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13522 = hit.y.dx;
    Interval _13659 = jet_mul_derivative(_13521, _13522, intervalFailed, optical_product_upper);
    Interval _13523 = _13659;
    Interval _13660 = jet_add_derivative(_13520, _13523, intervalFailed);
    Interval _13524 = Interval{ 0.0, 0.0 };
    Interval _13525 = hit.y.v;
    Interval _13661 = jet_mul_derivative(_13524, _13525, intervalFailed, optical_product_upper);
    Interval _13526 = _13661;
    Interval _13527 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13528 = hit.y.dy;
    Interval _13663 = jet_mul_derivative(_13527, _13528, intervalFailed, optical_product_upper);
    Interval _13529 = _13663;
    Interval _13664 = jet_add_derivative(_13526, _13529, intervalFailed);
    Interval _13502 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13503 = hit.y.v;
    Interval _13669 = imul(_13502, _13503, intervalFailed, optical_product_upper);
    Interval _13504 = Interval{ 0.0, 0.0 };
    Interval _13505 = hit.y.v;
    Interval _13670 = jet_mul_derivative(_13504, _13505, intervalFailed, optical_product_upper);
    Interval _13506 = _13670;
    Interval _13507 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13508 = hit.y.dx;
    Interval _13672 = jet_mul_derivative(_13507, _13508, intervalFailed, optical_product_upper);
    Interval _13509 = _13672;
    Interval _13673 = jet_add_derivative(_13506, _13509, intervalFailed);
    Interval _13510 = Interval{ 0.0, 0.0 };
    Interval _13511 = hit.y.v;
    Interval _13674 = jet_mul_derivative(_13510, _13511, intervalFailed, optical_product_upper);
    Interval _13512 = _13674;
    Interval _13513 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13514 = hit.y.dy;
    Interval _13676 = jet_mul_derivative(_13513, _13514, intervalFailed, optical_product_upper);
    Interval _13515 = _13676;
    Interval _13677 = jet_add_derivative(_13512, _13515, intervalFailed);
    Interval _13496 = _13601;
    Interval _13497 = _13643;
    Interval _13678 = iadd(_13496, _13497, intervalFailed);
    Interval _13498 = _13605;
    Interval _13499 = _13647;
    Interval _13679 = jet_add_derivative(_13498, _13499, intervalFailed);
    Interval _13500 = _13609;
    Interval _13501 = _13651;
    Interval _13680 = jet_add_derivative(_13500, _13501, intervalFailed);
    Interval _13490 = _13614;
    Interval _13491 = _13656;
    Interval _13681 = iadd(_13490, _13491, intervalFailed);
    Interval _13492 = _13618;
    Interval _13493 = _13660;
    Interval _13682 = jet_add_derivative(_13492, _13493, intervalFailed);
    Interval _13494 = _13622;
    Interval _13495 = _13664;
    Interval _13683 = jet_add_derivative(_13494, _13495, intervalFailed);
    Interval _13484 = _13627;
    Interval _13485 = _13669;
    Interval _13684 = iadd(_13484, _13485, intervalFailed);
    Interval _13486 = _13631;
    Interval _13487 = _13673;
    Interval _13685 = jet_add_derivative(_13486, _13487, intervalFailed);
    Interval _13488 = _13635;
    Interval _13489 = _13677;
    Interval _13686 = jet_add_derivative(_13488, _13489, intervalFailed);
    Interval _13470 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13471 = hit.z.v;
    Interval _13694 = imul(_13470, _13471, intervalFailed, optical_product_upper);
    Interval _13472 = Interval{ 0.0, 0.0 };
    Interval _13473 = hit.z.v;
    Interval _13695 = jet_mul_derivative(_13472, _13473, intervalFailed, optical_product_upper);
    Interval _13474 = _13695;
    Interval _13475 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13476 = hit.z.dx;
    Interval _13697 = jet_mul_derivative(_13475, _13476, intervalFailed, optical_product_upper);
    Interval _13477 = _13697;
    Interval _13698 = jet_add_derivative(_13474, _13477, intervalFailed);
    Interval _13478 = Interval{ 0.0, 0.0 };
    Interval _13479 = hit.z.v;
    Interval _13699 = jet_mul_derivative(_13478, _13479, intervalFailed, optical_product_upper);
    Interval _13480 = _13699;
    Interval _13481 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13482 = hit.z.dy;
    Interval _13701 = jet_mul_derivative(_13481, _13482, intervalFailed, optical_product_upper);
    Interval _13483 = _13701;
    Interval _13702 = jet_add_derivative(_13480, _13483, intervalFailed);
    Interval _13456 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13457 = hit.z.v;
    Interval _13707 = imul(_13456, _13457, intervalFailed, optical_product_upper);
    Interval _13458 = Interval{ 0.0, 0.0 };
    Interval _13459 = hit.z.v;
    Interval _13708 = jet_mul_derivative(_13458, _13459, intervalFailed, optical_product_upper);
    Interval _13460 = _13708;
    Interval _13461 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13462 = hit.z.dx;
    Interval _13710 = jet_mul_derivative(_13461, _13462, intervalFailed, optical_product_upper);
    Interval _13463 = _13710;
    Interval _13711 = jet_add_derivative(_13460, _13463, intervalFailed);
    Interval _13464 = Interval{ 0.0, 0.0 };
    Interval _13465 = hit.z.v;
    Interval _13712 = jet_mul_derivative(_13464, _13465, intervalFailed, optical_product_upper);
    Interval _13466 = _13712;
    Interval _13467 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13468 = hit.z.dy;
    Interval _13714 = jet_mul_derivative(_13467, _13468, intervalFailed, optical_product_upper);
    Interval _13469 = _13714;
    Interval _13715 = jet_add_derivative(_13466, _13469, intervalFailed);
    Interval _13442 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13443 = hit.z.v;
    Interval _13720 = imul(_13442, _13443, intervalFailed, optical_product_upper);
    Interval _13444 = Interval{ 0.0, 0.0 };
    Interval _13445 = hit.z.v;
    Interval _13721 = jet_mul_derivative(_13444, _13445, intervalFailed, optical_product_upper);
    Interval _13446 = _13721;
    Interval _13447 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13448 = hit.z.dx;
    Interval _13723 = jet_mul_derivative(_13447, _13448, intervalFailed, optical_product_upper);
    Interval _13449 = _13723;
    Interval _13724 = jet_add_derivative(_13446, _13449, intervalFailed);
    Interval _13450 = Interval{ 0.0, 0.0 };
    Interval _13451 = hit.z.v;
    Interval _13725 = jet_mul_derivative(_13450, _13451, intervalFailed, optical_product_upper);
    Interval _13452 = _13725;
    Interval _13453 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13454 = hit.z.dy;
    Interval _13727 = jet_mul_derivative(_13453, _13454, intervalFailed, optical_product_upper);
    Interval _13455 = _13727;
    Interval _13728 = jet_add_derivative(_13452, _13455, intervalFailed);
    Interval _13436 = _13678;
    Interval _13437 = _13694;
    Interval _13729 = iadd(_13436, _13437, intervalFailed);
    Interval _13438 = _13679;
    Interval _13439 = _13698;
    Interval _13730 = jet_add_derivative(_13438, _13439, intervalFailed);
    Interval _13440 = _13680;
    Interval _13441 = _13702;
    Interval _13731 = jet_add_derivative(_13440, _13441, intervalFailed);
    Interval _13430 = _13681;
    Interval _13431 = _13707;
    Interval _13732 = iadd(_13430, _13431, intervalFailed);
    Interval _13432 = _13682;
    Interval _13433 = _13711;
    Interval _13733 = jet_add_derivative(_13432, _13433, intervalFailed);
    Interval _13434 = _13683;
    Interval _13435 = _13715;
    Interval _13734 = jet_add_derivative(_13434, _13435, intervalFailed);
    Interval _13424 = _13684;
    Interval _13425 = _13720;
    Interval _13735 = iadd(_13424, _13425, intervalFailed);
    Interval _13426 = _13685;
    Interval _13427 = _13724;
    Interval _13736 = jet_add_derivative(_13426, _13427, intervalFailed);
    Interval _13428 = _13686;
    Interval _13429 = _13728;
    Interval _13737 = jet_add_derivative(_13428, _13429, intervalFailed);
    Interval _13418 = _13729;
    Interval _13419 = Interval{ f.rotation0.w, f.rotation0.w };
    Interval _13748 = iadd(_13418, _13419, intervalFailed);
    Interval _13420 = _13730;
    Interval _13421 = Interval{ 0.0, 0.0 };
    Interval _13749 = jet_add_derivative(_13420, _13421, intervalFailed);
    Interval _13422 = _13731;
    Interval _13423 = Interval{ 0.0, 0.0 };
    Interval _13750 = jet_add_derivative(_13422, _13423, intervalFailed);
    Interval _13412 = _13732;
    Interval _13413 = Interval{ f.rotation1.w, f.rotation1.w };
    Interval _13752 = iadd(_13412, _13413, intervalFailed);
    Interval _13414 = _13733;
    Interval _13415 = Interval{ 0.0, 0.0 };
    Interval _13753 = jet_add_derivative(_13414, _13415, intervalFailed);
    Interval _13416 = _13734;
    Interval _13417 = Interval{ 0.0, 0.0 };
    Interval _13754 = jet_add_derivative(_13416, _13417, intervalFailed);
    Interval _13406 = _13735;
    Interval _13407 = Interval{ f.rotation2.w, f.rotation2.w };
    Interval _13756 = iadd(_13406, _13407, intervalFailed);
    Interval _13408 = _13736;
    Interval _13409 = Interval{ 0.0, 0.0 };
    Interval _13757 = jet_add_derivative(_13408, _13409, intervalFailed);
    Interval _13410 = _13737;
    Interval _13411 = Interval{ 0.0, 0.0 };
    Interval _13758 = jet_add_derivative(_13410, _13411, intervalFailed);
    float _15372;
    float _15373;
    float _15374;
    float _15375;
    float _15376;
    float _15377;
    float _15378;
    float _15379;
    float _15380;
    float _15381;
    float _15382;
    float _15383;
    if (f.settings.y == 3.0)
    {
        Interval _13392 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13393 = direction.x.v;
        Interval _13799 = imul(_13392, _13393, intervalFailed, optical_product_upper);
        Interval _13394 = Interval{ 0.0, 0.0 };
        Interval _13395 = direction.x.v;
        Interval _13800 = jet_mul_derivative(_13394, _13395, intervalFailed, optical_product_upper);
        Interval _13396 = _13800;
        Interval _13397 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13398 = direction.x.dx;
        Interval _13802 = jet_mul_derivative(_13397, _13398, intervalFailed, optical_product_upper);
        Interval _13399 = _13802;
        Interval _13803 = jet_add_derivative(_13396, _13399, intervalFailed);
        Interval _13400 = Interval{ 0.0, 0.0 };
        Interval _13401 = direction.x.v;
        Interval _13804 = jet_mul_derivative(_13400, _13401, intervalFailed, optical_product_upper);
        Interval _13402 = _13804;
        Interval _13403 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13404 = direction.x.dy;
        Interval _13806 = jet_mul_derivative(_13403, _13404, intervalFailed, optical_product_upper);
        Interval _13405 = _13806;
        Interval _13807 = jet_add_derivative(_13402, _13405, intervalFailed);
        Interval _13378 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13379 = direction.x.v;
        Interval _13812 = imul(_13378, _13379, intervalFailed, optical_product_upper);
        Interval _13380 = Interval{ 0.0, 0.0 };
        Interval _13381 = direction.x.v;
        Interval _13813 = jet_mul_derivative(_13380, _13381, intervalFailed, optical_product_upper);
        Interval _13382 = _13813;
        Interval _13383 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13384 = direction.x.dx;
        Interval _13815 = jet_mul_derivative(_13383, _13384, intervalFailed, optical_product_upper);
        Interval _13385 = _13815;
        Interval _13816 = jet_add_derivative(_13382, _13385, intervalFailed);
        Interval _13386 = Interval{ 0.0, 0.0 };
        Interval _13387 = direction.x.v;
        Interval _13817 = jet_mul_derivative(_13386, _13387, intervalFailed, optical_product_upper);
        Interval _13388 = _13817;
        Interval _13389 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13390 = direction.x.dy;
        Interval _13819 = jet_mul_derivative(_13389, _13390, intervalFailed, optical_product_upper);
        Interval _13391 = _13819;
        Interval _13820 = jet_add_derivative(_13388, _13391, intervalFailed);
        Interval _13364 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13365 = direction.x.v;
        Interval _13825 = imul(_13364, _13365, intervalFailed, optical_product_upper);
        Interval _13366 = Interval{ 0.0, 0.0 };
        Interval _13367 = direction.x.v;
        Interval _13826 = jet_mul_derivative(_13366, _13367, intervalFailed, optical_product_upper);
        Interval _13368 = _13826;
        Interval _13369 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13370 = direction.x.dx;
        Interval _13828 = jet_mul_derivative(_13369, _13370, intervalFailed, optical_product_upper);
        Interval _13371 = _13828;
        Interval _13829 = jet_add_derivative(_13368, _13371, intervalFailed);
        Interval _13372 = Interval{ 0.0, 0.0 };
        Interval _13373 = direction.x.v;
        Interval _13830 = jet_mul_derivative(_13372, _13373, intervalFailed, optical_product_upper);
        Interval _13374 = _13830;
        Interval _13375 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13376 = direction.x.dy;
        Interval _13832 = jet_mul_derivative(_13375, _13376, intervalFailed, optical_product_upper);
        Interval _13377 = _13832;
        Interval _13833 = jet_add_derivative(_13374, _13377, intervalFailed);
        Interval _13350 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13351 = direction.y.v;
        Interval _13841 = imul(_13350, _13351, intervalFailed, optical_product_upper);
        Interval _13352 = Interval{ 0.0, 0.0 };
        Interval _13353 = direction.y.v;
        Interval _13842 = jet_mul_derivative(_13352, _13353, intervalFailed, optical_product_upper);
        Interval _13354 = _13842;
        Interval _13355 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13356 = direction.y.dx;
        Interval _13844 = jet_mul_derivative(_13355, _13356, intervalFailed, optical_product_upper);
        Interval _13357 = _13844;
        Interval _13845 = jet_add_derivative(_13354, _13357, intervalFailed);
        Interval _13358 = Interval{ 0.0, 0.0 };
        Interval _13359 = direction.y.v;
        Interval _13846 = jet_mul_derivative(_13358, _13359, intervalFailed, optical_product_upper);
        Interval _13360 = _13846;
        Interval _13361 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13362 = direction.y.dy;
        Interval _13848 = jet_mul_derivative(_13361, _13362, intervalFailed, optical_product_upper);
        Interval _13363 = _13848;
        Interval _13849 = jet_add_derivative(_13360, _13363, intervalFailed);
        Interval _13336 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13337 = direction.y.v;
        Interval _13854 = imul(_13336, _13337, intervalFailed, optical_product_upper);
        Interval _13338 = Interval{ 0.0, 0.0 };
        Interval _13339 = direction.y.v;
        Interval _13855 = jet_mul_derivative(_13338, _13339, intervalFailed, optical_product_upper);
        Interval _13340 = _13855;
        Interval _13341 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13342 = direction.y.dx;
        Interval _13857 = jet_mul_derivative(_13341, _13342, intervalFailed, optical_product_upper);
        Interval _13343 = _13857;
        Interval _13858 = jet_add_derivative(_13340, _13343, intervalFailed);
        Interval _13344 = Interval{ 0.0, 0.0 };
        Interval _13345 = direction.y.v;
        Interval _13859 = jet_mul_derivative(_13344, _13345, intervalFailed, optical_product_upper);
        Interval _13346 = _13859;
        Interval _13347 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13348 = direction.y.dy;
        Interval _13861 = jet_mul_derivative(_13347, _13348, intervalFailed, optical_product_upper);
        Interval _13349 = _13861;
        Interval _13862 = jet_add_derivative(_13346, _13349, intervalFailed);
        Interval _13322 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13323 = direction.y.v;
        Interval _13867 = imul(_13322, _13323, intervalFailed, optical_product_upper);
        Interval _13324 = Interval{ 0.0, 0.0 };
        Interval _13325 = direction.y.v;
        Interval _13868 = jet_mul_derivative(_13324, _13325, intervalFailed, optical_product_upper);
        Interval _13326 = _13868;
        Interval _13327 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13328 = direction.y.dx;
        Interval _13870 = jet_mul_derivative(_13327, _13328, intervalFailed, optical_product_upper);
        Interval _13329 = _13870;
        Interval _13871 = jet_add_derivative(_13326, _13329, intervalFailed);
        Interval _13330 = Interval{ 0.0, 0.0 };
        Interval _13331 = direction.y.v;
        Interval _13872 = jet_mul_derivative(_13330, _13331, intervalFailed, optical_product_upper);
        Interval _13332 = _13872;
        Interval _13333 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13334 = direction.y.dy;
        Interval _13874 = jet_mul_derivative(_13333, _13334, intervalFailed, optical_product_upper);
        Interval _13335 = _13874;
        Interval _13875 = jet_add_derivative(_13332, _13335, intervalFailed);
        Interval _13316 = _13799;
        Interval _13317 = _13841;
        Interval _13876 = iadd(_13316, _13317, intervalFailed);
        Interval _13318 = _13803;
        Interval _13319 = _13845;
        Interval _13877 = jet_add_derivative(_13318, _13319, intervalFailed);
        Interval _13320 = _13807;
        Interval _13321 = _13849;
        Interval _13878 = jet_add_derivative(_13320, _13321, intervalFailed);
        Interval _13310 = _13812;
        Interval _13311 = _13854;
        Interval _13879 = iadd(_13310, _13311, intervalFailed);
        Interval _13312 = _13816;
        Interval _13313 = _13858;
        Interval _13880 = jet_add_derivative(_13312, _13313, intervalFailed);
        Interval _13314 = _13820;
        Interval _13315 = _13862;
        Interval _13881 = jet_add_derivative(_13314, _13315, intervalFailed);
        Interval _13304 = _13825;
        Interval _13305 = _13867;
        Interval _13882 = iadd(_13304, _13305, intervalFailed);
        Interval _13306 = _13829;
        Interval _13307 = _13871;
        Interval _13883 = jet_add_derivative(_13306, _13307, intervalFailed);
        Interval _13308 = _13833;
        Interval _13309 = _13875;
        Interval _13884 = jet_add_derivative(_13308, _13309, intervalFailed);
        Interval _13290 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13291 = direction.z.v;
        Interval _13892 = imul(_13290, _13291, intervalFailed, optical_product_upper);
        Interval _13292 = Interval{ 0.0, 0.0 };
        Interval _13293 = direction.z.v;
        Interval _13893 = jet_mul_derivative(_13292, _13293, intervalFailed, optical_product_upper);
        Interval _13294 = _13893;
        Interval _13295 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13296 = direction.z.dx;
        Interval _13895 = jet_mul_derivative(_13295, _13296, intervalFailed, optical_product_upper);
        Interval _13297 = _13895;
        Interval _13896 = jet_add_derivative(_13294, _13297, intervalFailed);
        Interval _13298 = Interval{ 0.0, 0.0 };
        Interval _13299 = direction.z.v;
        Interval _13897 = jet_mul_derivative(_13298, _13299, intervalFailed, optical_product_upper);
        Interval _13300 = _13897;
        Interval _13301 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13302 = direction.z.dy;
        Interval _13899 = jet_mul_derivative(_13301, _13302, intervalFailed, optical_product_upper);
        Interval _13303 = _13899;
        Interval _13900 = jet_add_derivative(_13300, _13303, intervalFailed);
        Interval _13276 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13277 = direction.z.v;
        Interval _13905 = imul(_13276, _13277, intervalFailed, optical_product_upper);
        Interval _13278 = Interval{ 0.0, 0.0 };
        Interval _13279 = direction.z.v;
        Interval _13906 = jet_mul_derivative(_13278, _13279, intervalFailed, optical_product_upper);
        Interval _13280 = _13906;
        Interval _13281 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13282 = direction.z.dx;
        Interval _13908 = jet_mul_derivative(_13281, _13282, intervalFailed, optical_product_upper);
        Interval _13283 = _13908;
        Interval _13909 = jet_add_derivative(_13280, _13283, intervalFailed);
        Interval _13284 = Interval{ 0.0, 0.0 };
        Interval _13285 = direction.z.v;
        Interval _13910 = jet_mul_derivative(_13284, _13285, intervalFailed, optical_product_upper);
        Interval _13286 = _13910;
        Interval _13287 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13288 = direction.z.dy;
        Interval _13912 = jet_mul_derivative(_13287, _13288, intervalFailed, optical_product_upper);
        Interval _13289 = _13912;
        Interval _13913 = jet_add_derivative(_13286, _13289, intervalFailed);
        Interval _13262 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13263 = direction.z.v;
        Interval _13918 = imul(_13262, _13263, intervalFailed, optical_product_upper);
        Interval _13264 = Interval{ 0.0, 0.0 };
        Interval _13265 = direction.z.v;
        Interval _13919 = jet_mul_derivative(_13264, _13265, intervalFailed, optical_product_upper);
        Interval _13266 = _13919;
        Interval _13267 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13268 = direction.z.dx;
        Interval _13921 = jet_mul_derivative(_13267, _13268, intervalFailed, optical_product_upper);
        Interval _13269 = _13921;
        Interval _13922 = jet_add_derivative(_13266, _13269, intervalFailed);
        Interval _13270 = Interval{ 0.0, 0.0 };
        Interval _13271 = direction.z.v;
        Interval _13923 = jet_mul_derivative(_13270, _13271, intervalFailed, optical_product_upper);
        Interval _13272 = _13923;
        Interval _13273 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13274 = direction.z.dy;
        Interval _13925 = jet_mul_derivative(_13273, _13274, intervalFailed, optical_product_upper);
        Interval _13275 = _13925;
        Interval _13926 = jet_add_derivative(_13272, _13275, intervalFailed);
        Interval _13256 = _13876;
        Interval _13257 = _13892;
        Interval _13927 = iadd(_13256, _13257, intervalFailed);
        Interval _13258 = _13877;
        Interval _13259 = _13896;
        Interval _13928 = jet_add_derivative(_13258, _13259, intervalFailed);
        Interval _13260 = _13878;
        Interval _13261 = _13900;
        Interval _13929 = jet_add_derivative(_13260, _13261, intervalFailed);
        Interval _13250 = _13879;
        Interval _13251 = _13905;
        Interval _13930 = iadd(_13250, _13251, intervalFailed);
        Interval _13252 = _13880;
        Interval _13253 = _13909;
        Interval _13931 = jet_add_derivative(_13252, _13253, intervalFailed);
        Interval _13254 = _13881;
        Interval _13255 = _13913;
        Interval _13932 = jet_add_derivative(_13254, _13255, intervalFailed);
        Interval _13244 = _13882;
        Interval _13245 = _13918;
        Interval _13933 = iadd(_13244, _13245, intervalFailed);
        Interval _13246 = _13883;
        Interval _13247 = _13922;
        Interval _13934 = jet_add_derivative(_13246, _13247, intervalFailed);
        Interval _13248 = _13884;
        Interval _13249 = _13926;
        Interval _13935 = jet_add_derivative(_13248, _13249, intervalFailed);
        float _13972;
        float _13974;
        float _13976;
        float _13978;
        float _13980;
        float _13982;
        float _13984;
        float _13986;
        float _13988;
        float _13990;
        float _13992;
        float _13994;
        _13972 = 0.0;
        _13974 = 0.0;
        _13976 = 0.0;
        _13978 = 0.0;
        _13980 = 0.0;
        _13982 = 0.0;
        _13984 = 0.0;
        _13986 = 0.0;
        _13988 = 0.0;
        _13990 = 0.0;
        _13992 = 0.0;
        _13994 = 0.0;
        float _13937;
        float _13939;
        float _13941;
        float _13943;
        float _13945;
        float _13947;
        float _13949;
        float _13951;
        float _13953;
        float _13955;
        float _13957;
        float _13959;
        float _13961;
        float _13963;
        float _13965;
        float _13967;
        float _13969;
        float _13971;
        float _13973;
        float _13975;
        float _13977;
        float _13979;
        float _13981;
        float _13983;
        float _13985;
        float _13987;
        float _13989;
        float _13991;
        float _13993;
        float _13995;
        float _13936 = _13752.lo;
        float _13938 = _13752.hi;
        float _13940 = _13753.lo;
        float _13942 = _13753.hi;
        float _13944 = _13754.lo;
        float _13946 = _13754.hi;
        float _13948 = _13756.lo;
        float _13950 = _13756.hi;
        float _13952 = _13757.lo;
        float _13954 = _13757.hi;
        float _13956 = _13758.lo;
        float _13958 = _13758.hi;
        float _13960 = _13748.lo;
        float _13962 = _13748.hi;
        float _13964 = _13749.lo;
        float _13966 = _13749.hi;
        float _13968 = _13750.lo;
        float _13970 = _13750.hi;
        uint _13996 = 0u;
        for (; _13996 < 2u; _13936 = _13937, _13938 = _13939, _13940 = _13941, _13942 = _13943, _13944 = _13945, _13946 = _13947, _13948 = _13949, _13950 = _13951, _13952 = _13953, _13954 = _13955, _13956 = _13957, _13958 = _13959, _13960 = _13961, _13962 = _13963, _13964 = _13965, _13966 = _13967, _13968 = _13969, _13970 = _13971, _13972 = _13973, _13974 = _13975, _13976 = _13977, _13978 = _13979, _13980 = _13981, _13982 = _13983, _13984 = _13985, _13986 = _13987, _13988 = _13989, _13990 = _13991, _13992 = _13993, _13994 = _13995, _13996++)
        {
            OpticalJet param_var_x = OpticalJet{ Interval{ _13960, _13962 }, Interval{ _13964, _13966 }, Interval{ _13968, _13970 } };
            OpticalJet param_var_z = OpticalJet{ Interval{ _13948, _13950 }, Interval{ _13952, _13954 }, Interval{ _13956, _13958 } };
            OpticalJet param_var_t = OpticalJet{ Interval{ f.settings.x, f.settings.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet param_var_footprint = footprint;
            OpticalJet3 _14009 = jlava(param_var_x, param_var_z, param_var_t, param_var_footprint, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (_13996 == 0u)
            {
                float _13242 = 2.0;
                float _13243 = 10.0;
                Interval _14019 = iratio(_13242, _13243, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _14024;
                if (!intervalFailed)
                {
                    _14024 = intervalFailed;
                }
                else
                {
                    _14024 = false;
                }
                bool _14029;
                if (_14024)
                {
                    _14029 = jetFailureSite == 0u;
                }
                else
                {
                    _14029 = false;
                }
                if (_14029)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(2.0, 2.0, 10.0, 10.0);
                }
                Interval _13228 = _distance.v;
                Interval _13229 = _14019;
                Interval _14032 = imul(_13228, _13229, intervalFailed, optical_product_upper);
                Interval _13230 = _distance.dx;
                Interval _13231 = _14019;
                Interval _14033 = jet_mul_derivative(_13230, _13231, intervalFailed, optical_product_upper);
                Interval _13232 = _14033;
                Interval _13233 = _distance.v;
                Interval _13234 = Interval{ 0.0, 0.0 };
                Interval _14034 = jet_mul_derivative(_13233, _13234, intervalFailed, optical_product_upper);
                Interval _13235 = _14034;
                Interval _14035 = jet_add_derivative(_13232, _13235, intervalFailed);
                Interval _13236 = _distance.dy;
                Interval _13237 = _14019;
                Interval _14036 = jet_mul_derivative(_13236, _13237, intervalFailed, optical_product_upper);
                Interval _13238 = _14036;
                Interval _13239 = _distance.v;
                Interval _13240 = Interval{ 0.0, 0.0 };
                Interval _14037 = jet_mul_derivative(_13239, _13240, intervalFailed, optical_product_upper);
                Interval _13241 = _14037;
                Interval _14038 = jet_add_derivative(_13238, _13241, intervalFailed);
                float _14046 = as_type<float>(as_type<uint>(_14009.x.v.hi) ^ 2147483648u);
                float _14049 = as_type<float>(as_type<uint>(_14009.x.v.lo) ^ 2147483648u);
                OpticalJet param_var_a = OpticalJet{ _13930, _13931, _13932 };
                float _13226 = 12.0;
                float _13227 = 100.0;
                Interval _14068 = iratio(_13226, _13227, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _14073;
                if (!intervalFailed)
                {
                    _14073 = intervalFailed;
                }
                else
                {
                    _14073 = false;
                }
                bool _14078;
                if (_14073)
                {
                    _14078 = jetFailureSite == 0u;
                }
                else
                {
                    _14078 = false;
                }
                if (_14078)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(12.0, 12.0, 100.0, 100.0);
                }
                OpticalJet param_var_b = OpticalJet{ _14068, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _14082 = jmax(param_var_a, param_var_b);
                Interval _14083 = _14082.v;
                Interval _13212 = Interval{ _14046, _14049 };
                Interval _13213 = _14083;
                Interval _14088 = idiv(_13212, _13213, intervalFailed, interval_divide_upper);
                bool _14095;
                if (!intervalFailed)
                {
                    _14095 = intervalFailed;
                }
                else
                {
                    _14095 = false;
                }
                bool _14100;
                if (_14095)
                {
                    _14100 = jetFailureSite == 0u;
                }
                else
                {
                    _14100 = false;
                }
                if (_14100)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_14046, _14049, _14083.lo, _14083.hi);
                }
                Interval _13214 = Interval{ as_type<float>(as_type<uint>(_14009.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14009.x.dx.lo) ^ 2147483648u) };
                Interval _13215 = _14088;
                Interval _13216 = _14082.dx;
                Interval _14105 = jet_mul_derivative(_13215, _13216, intervalFailed, optical_product_upper);
                Interval _13217 = Interval{ as_type<float>(as_type<uint>(_14105.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14105.lo) ^ 2147483648u) };
                Interval _14115 = jet_add_derivative(_13214, _13217, intervalFailed);
                Interval _13218 = _14115;
                Interval _13219 = _14083;
                Interval _14116 = jet_div_derivative(_13218, _13219, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _13220 = Interval{ as_type<float>(as_type<uint>(_14009.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14009.x.dy.lo) ^ 2147483648u) };
                Interval _13221 = _14088;
                Interval _13222 = _14082.dy;
                Interval _14118 = jet_mul_derivative(_13221, _13222, intervalFailed, optical_product_upper);
                Interval _13223 = Interval{ as_type<float>(as_type<uint>(_14118.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14118.lo) ^ 2147483648u) };
                Interval _14128 = jet_add_derivative(_13220, _13223, intervalFailed);
                Interval _13224 = _14128;
                Interval _13225 = _14083;
                Interval _14129 = jet_div_derivative(_13224, _13225, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                OpticalJet _13208 = OpticalJet{ _14088, _14116, _14129 };
                OpticalJet _13209 = OpticalJet{ Interval{ as_type<float>(as_type<uint>(_14032.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14032.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_14035.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14035.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_14038.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14038.lo) ^ 2147483648u) } };
                OpticalJet _13210 = jmax(_13208, _13209);
                OpticalJet _13211 = OpticalJet{ _14032, _14035, _14038 };
                OpticalJet _14161 = jmin(_13210, _13211);
                Interval _14162 = _14161.v;
                Interval _13194 = _13927;
                Interval _13195 = _14162;
                Interval _14165 = imul(_13194, _13195, intervalFailed, optical_product_upper);
                Interval _13196 = _13928;
                Interval _13197 = _14162;
                Interval _14166 = jet_mul_derivative(_13196, _13197, intervalFailed, optical_product_upper);
                Interval _13198 = _14166;
                Interval _13199 = _13927;
                Interval _13200 = _14161.dx;
                Interval _14167 = jet_mul_derivative(_13199, _13200, intervalFailed, optical_product_upper);
                Interval _13201 = _14167;
                Interval _14168 = jet_add_derivative(_13198, _13201, intervalFailed);
                Interval _13202 = _13929;
                Interval _13203 = _14162;
                Interval _14169 = jet_mul_derivative(_13202, _13203, intervalFailed, optical_product_upper);
                Interval _13204 = _14169;
                Interval _13205 = _13927;
                Interval _13206 = _14161.dy;
                Interval _14170 = jet_mul_derivative(_13205, _13206, intervalFailed, optical_product_upper);
                Interval _13207 = _14170;
                Interval _14171 = jet_add_derivative(_13204, _13207, intervalFailed);
                Interval _14172 = _14161.v;
                Interval _13180 = _13930;
                Interval _13181 = _14172;
                Interval _14175 = imul(_13180, _13181, intervalFailed, optical_product_upper);
                Interval _13182 = _13931;
                Interval _13183 = _14172;
                Interval _14176 = jet_mul_derivative(_13182, _13183, intervalFailed, optical_product_upper);
                Interval _13184 = _14176;
                Interval _13185 = _13930;
                Interval _13186 = _14161.dx;
                Interval _14177 = jet_mul_derivative(_13185, _13186, intervalFailed, optical_product_upper);
                Interval _13187 = _14177;
                Interval _14178 = jet_add_derivative(_13184, _13187, intervalFailed);
                Interval _13188 = _13932;
                Interval _13189 = _14172;
                Interval _14179 = jet_mul_derivative(_13188, _13189, intervalFailed, optical_product_upper);
                Interval _13190 = _14179;
                Interval _13191 = _13930;
                Interval _13192 = _14161.dy;
                Interval _14180 = jet_mul_derivative(_13191, _13192, intervalFailed, optical_product_upper);
                Interval _13193 = _14180;
                Interval _14181 = jet_add_derivative(_13190, _13193, intervalFailed);
                Interval _14182 = _14161.v;
                Interval _13166 = _13933;
                Interval _13167 = _14182;
                Interval _14185 = imul(_13166, _13167, intervalFailed, optical_product_upper);
                Interval _13168 = _13934;
                Interval _13169 = _14182;
                Interval _14186 = jet_mul_derivative(_13168, _13169, intervalFailed, optical_product_upper);
                Interval _13170 = _14186;
                Interval _13171 = _13933;
                Interval _13172 = _14161.dx;
                Interval _14187 = jet_mul_derivative(_13171, _13172, intervalFailed, optical_product_upper);
                Interval _13173 = _14187;
                Interval _14188 = jet_add_derivative(_13170, _13173, intervalFailed);
                Interval _13174 = _13935;
                Interval _13175 = _14182;
                Interval _14189 = jet_mul_derivative(_13174, _13175, intervalFailed, optical_product_upper);
                Interval _13176 = _14189;
                Interval _13177 = _13933;
                Interval _13178 = _14161.dy;
                Interval _14190 = jet_mul_derivative(_13177, _13178, intervalFailed, optical_product_upper);
                Interval _13179 = _14190;
                Interval _14191 = jet_add_derivative(_13176, _13179, intervalFailed);
                Interval _13160 = Interval{ _13960, _13962 };
                Interval _13161 = _14165;
                Interval _14193 = iadd(_13160, _13161, intervalFailed);
                Interval _13162 = Interval{ _13964, _13966 };
                Interval _13163 = _14168;
                Interval _14195 = jet_add_derivative(_13162, _13163, intervalFailed);
                Interval _13164 = Interval{ _13968, _13970 };
                Interval _13165 = _14171;
                Interval _14197 = jet_add_derivative(_13164, _13165, intervalFailed);
                Interval _13154 = Interval{ _13936, _13938 };
                Interval _13155 = _14175;
                Interval _14199 = iadd(_13154, _13155, intervalFailed);
                Interval _13156 = Interval{ _13940, _13942 };
                Interval _13157 = _14178;
                Interval _14201 = jet_add_derivative(_13156, _13157, intervalFailed);
                Interval _13158 = Interval{ _13944, _13946 };
                Interval _13159 = _14181;
                Interval _14203 = jet_add_derivative(_13158, _13159, intervalFailed);
                Interval _13148 = Interval{ _13948, _13950 };
                Interval _13149 = _14185;
                Interval _14205 = iadd(_13148, _13149, intervalFailed);
                Interval _13150 = Interval{ _13952, _13954 };
                Interval _13151 = _14188;
                Interval _14207 = jet_add_derivative(_13150, _13151, intervalFailed);
                Interval _13152 = Interval{ _13956, _13958 };
                Interval _13153 = _14191;
                Interval _14209 = jet_add_derivative(_13152, _13153, intervalFailed);
                Interval _14239 = _14161.v;
                Interval _13134 = direction.x.v;
                Interval _13135 = _14239;
                Interval _14242 = imul(_13134, _13135, intervalFailed, optical_product_upper);
                Interval _13136 = direction.x.dx;
                Interval _13137 = _14239;
                Interval _14243 = jet_mul_derivative(_13136, _13137, intervalFailed, optical_product_upper);
                Interval _13138 = _14243;
                Interval _13139 = direction.x.v;
                Interval _13140 = _14161.dx;
                Interval _14244 = jet_mul_derivative(_13139, _13140, intervalFailed, optical_product_upper);
                Interval _13141 = _14244;
                Interval _14245 = jet_add_derivative(_13138, _13141, intervalFailed);
                Interval _13142 = direction.x.dy;
                Interval _13143 = _14239;
                Interval _14246 = jet_mul_derivative(_13142, _13143, intervalFailed, optical_product_upper);
                Interval _13144 = _14246;
                Interval _13145 = direction.x.v;
                Interval _13146 = _14161.dy;
                Interval _14247 = jet_mul_derivative(_13145, _13146, intervalFailed, optical_product_upper);
                Interval _13147 = _14247;
                Interval _14248 = jet_add_derivative(_13144, _13147, intervalFailed);
                Interval _14252 = _14161.v;
                Interval _13120 = direction.y.v;
                Interval _13121 = _14252;
                Interval _14255 = imul(_13120, _13121, intervalFailed, optical_product_upper);
                Interval _13122 = direction.y.dx;
                Interval _13123 = _14252;
                Interval _14256 = jet_mul_derivative(_13122, _13123, intervalFailed, optical_product_upper);
                Interval _13124 = _14256;
                Interval _13125 = direction.y.v;
                Interval _13126 = _14161.dx;
                Interval _14257 = jet_mul_derivative(_13125, _13126, intervalFailed, optical_product_upper);
                Interval _13127 = _14257;
                Interval _14258 = jet_add_derivative(_13124, _13127, intervalFailed);
                Interval _13128 = direction.y.dy;
                Interval _13129 = _14252;
                Interval _14259 = jet_mul_derivative(_13128, _13129, intervalFailed, optical_product_upper);
                Interval _13130 = _14259;
                Interval _13131 = direction.y.v;
                Interval _13132 = _14161.dy;
                Interval _14260 = jet_mul_derivative(_13131, _13132, intervalFailed, optical_product_upper);
                Interval _13133 = _14260;
                Interval _14261 = jet_add_derivative(_13130, _13133, intervalFailed);
                Interval _14265 = _14161.v;
                Interval _13106 = direction.z.v;
                Interval _13107 = _14265;
                Interval _14268 = imul(_13106, _13107, intervalFailed, optical_product_upper);
                Interval _13108 = direction.z.dx;
                Interval _13109 = _14265;
                Interval _14269 = jet_mul_derivative(_13108, _13109, intervalFailed, optical_product_upper);
                Interval _13110 = _14269;
                Interval _13111 = direction.z.v;
                Interval _13112 = _14161.dx;
                Interval _14270 = jet_mul_derivative(_13111, _13112, intervalFailed, optical_product_upper);
                Interval _13113 = _14270;
                Interval _14271 = jet_add_derivative(_13110, _13113, intervalFailed);
                Interval _13114 = direction.z.dy;
                Interval _13115 = _14265;
                Interval _14272 = jet_mul_derivative(_13114, _13115, intervalFailed, optical_product_upper);
                Interval _13116 = _14272;
                Interval _13117 = direction.z.v;
                Interval _13118 = _14161.dy;
                Interval _14273 = jet_mul_derivative(_13117, _13118, intervalFailed, optical_product_upper);
                Interval _13119 = _14273;
                Interval _14274 = jet_add_derivative(_13116, _13119, intervalFailed);
                Interval _13100 = hit.x.v;
                Interval _13101 = _14242;
                Interval _14278 = iadd(_13100, _13101, intervalFailed);
                Interval _13102 = hit.x.dx;
                Interval _13103 = _14245;
                Interval _14279 = jet_add_derivative(_13102, _13103, intervalFailed);
                Interval _13104 = hit.x.dy;
                Interval _13105 = _14248;
                Interval _14280 = jet_add_derivative(_13104, _13105, intervalFailed);
                Interval _13094 = hit.y.v;
                Interval _13095 = _14255;
                Interval _14284 = iadd(_13094, _13095, intervalFailed);
                Interval _13096 = hit.y.dx;
                Interval _13097 = _14258;
                Interval _14285 = jet_add_derivative(_13096, _13097, intervalFailed);
                Interval _13098 = hit.y.dy;
                Interval _13099 = _14261;
                Interval _14286 = jet_add_derivative(_13098, _13099, intervalFailed);
                Interval _13088 = hit.z.v;
                Interval _13089 = _14268;
                Interval _14290 = iadd(_13088, _13089, intervalFailed);
                Interval _13090 = hit.z.dx;
                Interval _13091 = _14271;
                Interval _14291 = jet_add_derivative(_13090, _13091, intervalFailed);
                Interval _13092 = hit.z.dy;
                Interval _13093 = _14274;
                Interval _14292 = jet_add_derivative(_13092, _13093, intervalFailed);
                hit = OpticalJet3{ OpticalJet{ _14278, _14279, _14280 }, OpticalJet{ _14284, _14285, _14286 }, OpticalJet{ _14290, _14291, _14292 } };
                _13937 = _14199.lo;
                _13939 = _14199.hi;
                _13941 = _14201.lo;
                _13943 = _14201.hi;
                _13945 = _14203.lo;
                _13947 = _14203.hi;
                _13949 = _14205.lo;
                _13951 = _14205.hi;
                _13953 = _14207.lo;
                _13955 = _14207.hi;
                _13957 = _14209.lo;
                _13959 = _14209.hi;
                _13961 = _14193.lo;
                _13963 = _14193.hi;
                _13965 = _14195.lo;
                _13967 = _14195.hi;
                _13969 = _14197.lo;
                _13971 = _14197.hi;
                _13973 = _13972;
                _13975 = _13974;
                _13977 = _13976;
                _13979 = _13978;
                _13981 = _13980;
                _13983 = _13982;
                _13985 = _13984;
                _13987 = _13986;
                _13989 = _13988;
                _13991 = _13990;
                _13993 = _13992;
                _13995 = _13994;
            }
            else
            {
                _13937 = _13936;
                _13939 = _13938;
                _13941 = _13940;
                _13943 = _13942;
                _13945 = _13944;
                _13947 = _13946;
                _13949 = _13948;
                _13951 = _13950;
                _13953 = _13952;
                _13955 = _13954;
                _13957 = _13956;
                _13959 = _13958;
                _13961 = _13960;
                _13963 = _13962;
                _13965 = _13964;
                _13967 = _13966;
                _13969 = _13968;
                _13971 = _13970;
                _13973 = _14009.z.v.lo;
                _13975 = _14009.z.v.hi;
                _13977 = _14009.z.dx.lo;
                _13979 = _14009.z.dx.hi;
                _13981 = _14009.z.dy.lo;
                _13983 = _14009.z.dy.hi;
                _13985 = _14009.y.v.lo;
                _13987 = _14009.y.v.hi;
                _13989 = _14009.y.dx.lo;
                _13991 = _14009.y.dx.hi;
                _13993 = _14009.y.dy.lo;
                _13995 = _14009.y.dy.hi;
            }
        }
        _15372 = _13972;
        _15373 = _13974;
        _15374 = _13976;
        _15375 = _13978;
        _15376 = _13980;
        _15377 = _13982;
        _15378 = _13984;
        _15379 = _13986;
        _15380 = _13988;
        _15381 = _13990;
        _15382 = _13992;
        _15383 = _13994;
    }
    else
    {
        float _13086 = 18.0;
        float _13087 = 1000.0;
        Interval _14316 = iratio(_13086, _13087, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14321;
        if (!intervalFailed)
        {
            _14321 = intervalFailed;
        }
        else
        {
            _14321 = false;
        }
        bool _14326;
        if (_14321)
        {
            _14326 = jetFailureSite == 0u;
        }
        else
        {
            _14326 = false;
        }
        if (_14326)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
        }
        Interval _13072 = _13748;
        Interval _13073 = _14316;
        Interval _14329 = imul(_13072, _13073, intervalFailed, optical_product_upper);
        Interval _13074 = _13749;
        Interval _13075 = _14316;
        Interval _14330 = jet_mul_derivative(_13074, _13075, intervalFailed, optical_product_upper);
        Interval _13076 = _14330;
        Interval _13077 = _13748;
        Interval _13078 = Interval{ 0.0, 0.0 };
        Interval _14331 = jet_mul_derivative(_13077, _13078, intervalFailed, optical_product_upper);
        Interval _13079 = _14331;
        Interval _14332 = jet_add_derivative(_13076, _13079, intervalFailed);
        Interval _13080 = _13750;
        Interval _13081 = _14316;
        Interval _14333 = jet_mul_derivative(_13080, _13081, intervalFailed, optical_product_upper);
        Interval _13082 = _14333;
        Interval _13083 = _13748;
        Interval _13084 = Interval{ 0.0, 0.0 };
        Interval _14334 = jet_mul_derivative(_13083, _13084, intervalFailed, optical_product_upper);
        Interval _13085 = _14334;
        Interval _14335 = jet_add_derivative(_13082, _13085, intervalFailed);
        float _13070 = 11.0;
        float _13071 = 1000.0;
        Interval _14337 = iratio(_13070, _13071, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14342;
        if (!intervalFailed)
        {
            _14342 = intervalFailed;
        }
        else
        {
            _14342 = false;
        }
        bool _14347;
        if (_14342)
        {
            _14347 = jetFailureSite == 0u;
        }
        else
        {
            _14347 = false;
        }
        if (_14347)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
        }
        Interval _13056 = _13756;
        Interval _13057 = _14337;
        Interval _14350 = imul(_13056, _13057, intervalFailed, optical_product_upper);
        Interval _13058 = _13757;
        Interval _13059 = _14337;
        Interval _14351 = jet_mul_derivative(_13058, _13059, intervalFailed, optical_product_upper);
        Interval _13060 = _14351;
        Interval _13061 = _13756;
        Interval _13062 = Interval{ 0.0, 0.0 };
        Interval _14352 = jet_mul_derivative(_13061, _13062, intervalFailed, optical_product_upper);
        Interval _13063 = _14352;
        Interval _14353 = jet_add_derivative(_13060, _13063, intervalFailed);
        Interval _13064 = _13758;
        Interval _13065 = _14337;
        Interval _14354 = jet_mul_derivative(_13064, _13065, intervalFailed, optical_product_upper);
        Interval _13066 = _14354;
        Interval _13067 = _13756;
        Interval _13068 = Interval{ 0.0, 0.0 };
        Interval _14355 = jet_mul_derivative(_13067, _13068, intervalFailed, optical_product_upper);
        Interval _13069 = _14355;
        Interval _14356 = jet_add_derivative(_13066, _13069, intervalFailed);
        Interval _13050 = _14329;
        Interval _13051 = _14350;
        Interval _14357 = iadd(_13050, _13051, intervalFailed);
        Interval _13052 = _14332;
        Interval _13053 = _14353;
        Interval _14358 = jet_add_derivative(_13052, _13053, intervalFailed);
        Interval _13054 = _14335;
        Interval _13055 = _14356;
        Interval _14359 = jet_add_derivative(_13054, _13055, intervalFailed);
        float _13048 = 8.0;
        float _13049 = 10.0;
        Interval _14361 = iratio(_13048, _13049, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14366;
        if (!intervalFailed)
        {
            _14366 = intervalFailed;
        }
        else
        {
            _14366 = false;
        }
        bool _14371;
        if (_14366)
        {
            _14371 = jetFailureSite == 0u;
        }
        else
        {
            _14371 = false;
        }
        if (_14371)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(8.0, 8.0, 10.0, 10.0);
        }
        Interval _13034 = Interval{ f.settings.x, f.settings.x };
        Interval _13035 = _14361;
        Interval _14375 = imul(_13034, _13035, intervalFailed, optical_product_upper);
        Interval _13036 = Interval{ 0.0, 0.0 };
        Interval _13037 = _14361;
        Interval _14376 = jet_mul_derivative(_13036, _13037, intervalFailed, optical_product_upper);
        Interval _13038 = _14376;
        Interval _13039 = Interval{ f.settings.x, f.settings.x };
        Interval _13040 = Interval{ 0.0, 0.0 };
        Interval _14378 = jet_mul_derivative(_13039, _13040, intervalFailed, optical_product_upper);
        Interval _13041 = _14378;
        Interval _14379 = jet_add_derivative(_13038, _13041, intervalFailed);
        Interval _13042 = Interval{ 0.0, 0.0 };
        Interval _13043 = _14361;
        Interval _14380 = jet_mul_derivative(_13042, _13043, intervalFailed, optical_product_upper);
        Interval _13044 = _14380;
        Interval _13045 = Interval{ f.settings.x, f.settings.x };
        Interval _13046 = Interval{ 0.0, 0.0 };
        Interval _14382 = jet_mul_derivative(_13045, _13046, intervalFailed, optical_product_upper);
        Interval _13047 = _14382;
        Interval _14383 = jet_add_derivative(_13044, _13047, intervalFailed);
        Interval _13028 = _14357;
        Interval _13029 = Interval{ as_type<float>(as_type<uint>(_14375.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14375.lo) ^ 2147483648u) };
        Interval _14409 = iadd(_13028, _13029, intervalFailed);
        Interval _13030 = _14358;
        Interval _13031 = Interval{ as_type<float>(as_type<uint>(_14379.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14379.lo) ^ 2147483648u) };
        Interval _14411 = jet_add_derivative(_13030, _13031, intervalFailed);
        Interval _13032 = _14359;
        Interval _13033 = Interval{ as_type<float>(as_type<uint>(_14383.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14383.lo) ^ 2147483648u) };
        Interval _14413 = jet_add_derivative(_13032, _13033, intervalFailed);
        float _13026 = 47.0;
        float _13027 = 1000.0;
        Interval _14415 = iratio(_13026, _13027, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14420;
        if (!intervalFailed)
        {
            _14420 = intervalFailed;
        }
        else
        {
            _14420 = false;
        }
        bool _14425;
        if (_14420)
        {
            _14425 = jetFailureSite == 0u;
        }
        else
        {
            _14425 = false;
        }
        if (_14425)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
        }
        Interval _13012 = _13748;
        Interval _13013 = _14415;
        Interval _14428 = imul(_13012, _13013, intervalFailed, optical_product_upper);
        Interval _13014 = _13749;
        Interval _13015 = _14415;
        Interval _14429 = jet_mul_derivative(_13014, _13015, intervalFailed, optical_product_upper);
        Interval _13016 = _14429;
        Interval _13017 = _13748;
        Interval _13018 = Interval{ 0.0, 0.0 };
        Interval _14430 = jet_mul_derivative(_13017, _13018, intervalFailed, optical_product_upper);
        Interval _13019 = _14430;
        Interval _14431 = jet_add_derivative(_13016, _13019, intervalFailed);
        Interval _13020 = _13750;
        Interval _13021 = _14415;
        Interval _14432 = jet_mul_derivative(_13020, _13021, intervalFailed, optical_product_upper);
        Interval _13022 = _14432;
        Interval _13023 = _13748;
        Interval _13024 = Interval{ 0.0, 0.0 };
        Interval _14433 = jet_mul_derivative(_13023, _13024, intervalFailed, optical_product_upper);
        Interval _13025 = _14433;
        Interval _14434 = jet_add_derivative(_13022, _13025, intervalFailed);
        float _13010 = 25.0;
        float _13011 = 1000.0;
        Interval _14436 = iratio(_13010, _13011, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14441;
        if (!intervalFailed)
        {
            _14441 = intervalFailed;
        }
        else
        {
            _14441 = false;
        }
        bool _14446;
        if (_14441)
        {
            _14446 = jetFailureSite == 0u;
        }
        else
        {
            _14446 = false;
        }
        if (_14446)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        Interval _12996 = _13756;
        Interval _12997 = _14436;
        Interval _14449 = imul(_12996, _12997, intervalFailed, optical_product_upper);
        Interval _12998 = _13757;
        Interval _12999 = _14436;
        Interval _14450 = jet_mul_derivative(_12998, _12999, intervalFailed, optical_product_upper);
        Interval _13000 = _14450;
        Interval _13001 = _13756;
        Interval _13002 = Interval{ 0.0, 0.0 };
        Interval _14451 = jet_mul_derivative(_13001, _13002, intervalFailed, optical_product_upper);
        Interval _13003 = _14451;
        Interval _14452 = jet_add_derivative(_13000, _13003, intervalFailed);
        Interval _13004 = _13758;
        Interval _13005 = _14436;
        Interval _14453 = jet_mul_derivative(_13004, _13005, intervalFailed, optical_product_upper);
        Interval _13006 = _14453;
        Interval _13007 = _13756;
        Interval _13008 = Interval{ 0.0, 0.0 };
        Interval _14454 = jet_mul_derivative(_13007, _13008, intervalFailed, optical_product_upper);
        Interval _13009 = _14454;
        Interval _14455 = jet_add_derivative(_13006, _13009, intervalFailed);
        Interval _12990 = _14428;
        Interval _12991 = Interval{ as_type<float>(as_type<uint>(_14449.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14449.lo) ^ 2147483648u) };
        Interval _14481 = iadd(_12990, _12991, intervalFailed);
        Interval _12992 = _14431;
        Interval _12993 = Interval{ as_type<float>(as_type<uint>(_14452.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14452.lo) ^ 2147483648u) };
        Interval _14483 = jet_add_derivative(_12992, _12993, intervalFailed);
        Interval _12994 = _14434;
        Interval _12995 = Interval{ as_type<float>(as_type<uint>(_14455.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14455.lo) ^ 2147483648u) };
        Interval _14485 = jet_add_derivative(_12994, _12995, intervalFailed);
        float _12988 = 12.0;
        float _12989 = 10.0;
        Interval _14487 = iratio(_12988, _12989, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14492;
        if (!intervalFailed)
        {
            _14492 = intervalFailed;
        }
        else
        {
            _14492 = false;
        }
        bool _14497;
        if (_14492)
        {
            _14497 = jetFailureSite == 0u;
        }
        else
        {
            _14497 = false;
        }
        if (_14497)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(12.0, 12.0, 10.0, 10.0);
        }
        Interval _12974 = Interval{ f.settings.x, f.settings.x };
        Interval _12975 = _14487;
        Interval _14501 = imul(_12974, _12975, intervalFailed, optical_product_upper);
        Interval _12976 = Interval{ 0.0, 0.0 };
        Interval _12977 = _14487;
        Interval _14502 = jet_mul_derivative(_12976, _12977, intervalFailed, optical_product_upper);
        Interval _12978 = _14502;
        Interval _12979 = Interval{ f.settings.x, f.settings.x };
        Interval _12980 = Interval{ 0.0, 0.0 };
        Interval _14504 = jet_mul_derivative(_12979, _12980, intervalFailed, optical_product_upper);
        Interval _12981 = _14504;
        Interval _14505 = jet_add_derivative(_12978, _12981, intervalFailed);
        Interval _12982 = Interval{ 0.0, 0.0 };
        Interval _12983 = _14487;
        Interval _14506 = jet_mul_derivative(_12982, _12983, intervalFailed, optical_product_upper);
        Interval _12984 = _14506;
        Interval _12985 = Interval{ f.settings.x, f.settings.x };
        Interval _12986 = Interval{ 0.0, 0.0 };
        Interval _14508 = jet_mul_derivative(_12985, _12986, intervalFailed, optical_product_upper);
        Interval _12987 = _14508;
        Interval _14509 = jet_add_derivative(_12984, _12987, intervalFailed);
        Interval _12968 = _14481;
        Interval _12969 = _14501;
        Interval _14510 = iadd(_12968, _12969, intervalFailed);
        Interval _12970 = _14483;
        Interval _12971 = _14505;
        Interval _14511 = jet_add_derivative(_12970, _12971, intervalFailed);
        Interval _12972 = _14485;
        Interval _12973 = _14509;
        Interval _14512 = jet_add_derivative(_12972, _12973, intervalFailed);
        float _12966 = 22.0;
        float _12967 = 1000.0;
        Interval _14514 = iratio(_12966, _12967, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14519;
        if (!intervalFailed)
        {
            _14519 = intervalFailed;
        }
        else
        {
            _14519 = false;
        }
        bool _14524;
        if (_14519)
        {
            _14524 = jetFailureSite == 0u;
        }
        else
        {
            _14524 = false;
        }
        if (_14524)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12952 = _13756;
        Interval _12953 = _14514;
        Interval _14527 = imul(_12952, _12953, intervalFailed, optical_product_upper);
        Interval _12954 = _13757;
        Interval _12955 = _14514;
        Interval _14528 = jet_mul_derivative(_12954, _12955, intervalFailed, optical_product_upper);
        Interval _12956 = _14528;
        Interval _12957 = _13756;
        Interval _12958 = Interval{ 0.0, 0.0 };
        Interval _14529 = jet_mul_derivative(_12957, _12958, intervalFailed, optical_product_upper);
        Interval _12959 = _14529;
        Interval _14530 = jet_add_derivative(_12956, _12959, intervalFailed);
        Interval _12960 = _13758;
        Interval _12961 = _14514;
        Interval _14531 = jet_mul_derivative(_12960, _12961, intervalFailed, optical_product_upper);
        Interval _12962 = _14531;
        Interval _12963 = _13756;
        Interval _12964 = Interval{ 0.0, 0.0 };
        Interval _14532 = jet_mul_derivative(_12963, _12964, intervalFailed, optical_product_upper);
        Interval _12965 = _14532;
        Interval _14533 = jet_add_derivative(_12962, _12965, intervalFailed);
        float _12950 = 9.0;
        float _12951 = 1000.0;
        Interval _14535 = iratio(_12950, _12951, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14540;
        if (!intervalFailed)
        {
            _14540 = intervalFailed;
        }
        else
        {
            _14540 = false;
        }
        bool _14545;
        if (_14540)
        {
            _14545 = jetFailureSite == 0u;
        }
        else
        {
            _14545 = false;
        }
        if (_14545)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
        }
        Interval _12936 = _13748;
        Interval _12937 = _14535;
        Interval _14548 = imul(_12936, _12937, intervalFailed, optical_product_upper);
        Interval _12938 = _13749;
        Interval _12939 = _14535;
        Interval _14549 = jet_mul_derivative(_12938, _12939, intervalFailed, optical_product_upper);
        Interval _12940 = _14549;
        Interval _12941 = _13748;
        Interval _12942 = Interval{ 0.0, 0.0 };
        Interval _14550 = jet_mul_derivative(_12941, _12942, intervalFailed, optical_product_upper);
        Interval _12943 = _14550;
        Interval _14551 = jet_add_derivative(_12940, _12943, intervalFailed);
        Interval _12944 = _13750;
        Interval _12945 = _14535;
        Interval _14552 = jet_mul_derivative(_12944, _12945, intervalFailed, optical_product_upper);
        Interval _12946 = _14552;
        Interval _12947 = _13748;
        Interval _12948 = Interval{ 0.0, 0.0 };
        Interval _14553 = jet_mul_derivative(_12947, _12948, intervalFailed, optical_product_upper);
        Interval _12949 = _14553;
        Interval _14554 = jet_add_derivative(_12946, _12949, intervalFailed);
        Interval _12930 = _14527;
        Interval _12931 = Interval{ as_type<float>(as_type<uint>(_14548.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14548.lo) ^ 2147483648u) };
        Interval _14580 = iadd(_12930, _12931, intervalFailed);
        Interval _12932 = _14530;
        Interval _12933 = Interval{ as_type<float>(as_type<uint>(_14551.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14551.lo) ^ 2147483648u) };
        Interval _14582 = jet_add_derivative(_12932, _12933, intervalFailed);
        Interval _12934 = _14533;
        Interval _12935 = Interval{ as_type<float>(as_type<uint>(_14554.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14554.lo) ^ 2147483648u) };
        Interval _14584 = jet_add_derivative(_12934, _12935, intervalFailed);
        float _12928 = 65.0;
        float _12929 = 100.0;
        Interval _14586 = iratio(_12928, _12929, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14591;
        if (!intervalFailed)
        {
            _14591 = intervalFailed;
        }
        else
        {
            _14591 = false;
        }
        bool _14596;
        if (_14591)
        {
            _14596 = jetFailureSite == 0u;
        }
        else
        {
            _14596 = false;
        }
        if (_14596)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
        }
        Interval _12914 = Interval{ f.settings.x, f.settings.x };
        Interval _12915 = _14586;
        Interval _14600 = imul(_12914, _12915, intervalFailed, optical_product_upper);
        Interval _12916 = Interval{ 0.0, 0.0 };
        Interval _12917 = _14586;
        Interval _14601 = jet_mul_derivative(_12916, _12917, intervalFailed, optical_product_upper);
        Interval _12918 = _14601;
        Interval _12919 = Interval{ f.settings.x, f.settings.x };
        Interval _12920 = Interval{ 0.0, 0.0 };
        Interval _14603 = jet_mul_derivative(_12919, _12920, intervalFailed, optical_product_upper);
        Interval _12921 = _14603;
        Interval _14604 = jet_add_derivative(_12918, _12921, intervalFailed);
        Interval _12922 = Interval{ 0.0, 0.0 };
        Interval _12923 = _14586;
        Interval _14605 = jet_mul_derivative(_12922, _12923, intervalFailed, optical_product_upper);
        Interval _12924 = _14605;
        Interval _12925 = Interval{ f.settings.x, f.settings.x };
        Interval _12926 = Interval{ 0.0, 0.0 };
        Interval _14607 = jet_mul_derivative(_12925, _12926, intervalFailed, optical_product_upper);
        Interval _12927 = _14607;
        Interval _14608 = jet_add_derivative(_12924, _12927, intervalFailed);
        Interval _12908 = _14580;
        Interval _12909 = Interval{ as_type<float>(as_type<uint>(_14600.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14600.lo) ^ 2147483648u) };
        Interval _14634 = iadd(_12908, _12909, intervalFailed);
        Interval _12910 = _14582;
        Interval _12911 = Interval{ as_type<float>(as_type<uint>(_14604.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14604.lo) ^ 2147483648u) };
        Interval _14636 = jet_add_derivative(_12910, _12911, intervalFailed);
        Interval _12912 = _14584;
        Interval _12913 = Interval{ as_type<float>(as_type<uint>(_14608.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14608.lo) ^ 2147483648u) };
        Interval _14638 = jet_add_derivative(_12912, _12913, intervalFailed);
        float _12906 = 55.0;
        float _12907 = 1000.0;
        Interval _14640 = iratio(_12906, _12907, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14645;
        if (!intervalFailed)
        {
            _14645 = intervalFailed;
        }
        else
        {
            _14645 = false;
        }
        bool _14650;
        if (_14645)
        {
            _14650 = jetFailureSite == 0u;
        }
        else
        {
            _14650 = false;
        }
        if (_14650)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
        }
        float _12900 = _14409.lo;
        float _12901 = _14409.hi;
        float _14655 = sine_bounds(_12900, _12901, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14659 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14662 = as_type<float>(as_type<uint>(_14655) ^ 2147483648u);
        Interval _12896 = _14409;
        float _12894 = 3.1415927410125732421875;
        float _14663 = interval_down(_12894, intervalFailed);
        float _12895 = 3.1415927410125732421875;
        float _14664 = interval_up(_12895, intervalFailed);
        Interval _12897 = Interval{ _14663, _14664 };
        Interval _12898 = Interval{ 0.5, 0.5 };
        Interval _14666 = imul(_12897, _12898, intervalFailed, optical_product_upper);
        Interval _12899 = _14666;
        Interval _14667 = iadd(_12896, _12899, intervalFailed);
        float _12892 = _14667.lo;
        float _12893 = _14667.hi;
        float _14670 = sine_bounds(_12892, _12893, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12902 = Interval{ _14659, _14662 };
        Interval _12903 = _14411;
        Interval _14673 = jet_mul_derivative(_12902, _12903, intervalFailed, optical_product_upper);
        Interval _12904 = Interval{ _14659, _14662 };
        Interval _12905 = _14413;
        Interval _14675 = jet_mul_derivative(_12904, _12905, intervalFailed, optical_product_upper);
        Interval _12878 = _14640;
        Interval _12879 = Interval{ _14670, interval_sine_upper };
        Interval _14677 = imul(_12878, _12879, intervalFailed, optical_product_upper);
        Interval _12880 = Interval{ 0.0, 0.0 };
        Interval _12881 = Interval{ _14670, interval_sine_upper };
        Interval _14679 = jet_mul_derivative(_12880, _12881, intervalFailed, optical_product_upper);
        Interval _12882 = _14679;
        Interval _12883 = _14640;
        Interval _12884 = _14673;
        Interval _14680 = jet_mul_derivative(_12883, _12884, intervalFailed, optical_product_upper);
        Interval _12885 = _14680;
        Interval _14681 = jet_add_derivative(_12882, _12885, intervalFailed);
        Interval _12886 = Interval{ 0.0, 0.0 };
        Interval _12887 = Interval{ _14670, interval_sine_upper };
        Interval _14683 = jet_mul_derivative(_12886, _12887, intervalFailed, optical_product_upper);
        Interval _12888 = _14683;
        Interval _12889 = _14640;
        Interval _12890 = _14675;
        Interval _14684 = jet_mul_derivative(_12889, _12890, intervalFailed, optical_product_upper);
        Interval _12891 = _14684;
        Interval _14685 = jet_add_derivative(_12888, _12891, intervalFailed);
        float _12876 = 22.0;
        float _12877 = 1000.0;
        Interval _14688 = iratio(_12876, _12877, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14693;
        if (!intervalFailed)
        {
            _14693 = intervalFailed;
        }
        else
        {
            _14693 = false;
        }
        bool _14698;
        if (_14693)
        {
            _14698 = jetFailureSite == 0u;
        }
        else
        {
            _14698 = false;
        }
        if (_14698)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12862 = footprint.v;
        Interval _12863 = _14688;
        Interval _14704 = imul(_12862, _12863, intervalFailed, optical_product_upper);
        Interval _12864 = footprint.dx;
        Interval _12865 = _14688;
        Interval _14705 = jet_mul_derivative(_12864, _12865, intervalFailed, optical_product_upper);
        Interval _12866 = _14705;
        Interval _12867 = footprint.v;
        Interval _12868 = Interval{ 0.0, 0.0 };
        Interval _14706 = jet_mul_derivative(_12867, _12868, intervalFailed, optical_product_upper);
        Interval _12869 = _14706;
        Interval _14707 = jet_add_derivative(_12866, _12869, intervalFailed);
        Interval _12870 = footprint.dy;
        Interval _12871 = _14688;
        Interval _14708 = jet_mul_derivative(_12870, _12871, intervalFailed, optical_product_upper);
        Interval _12872 = _14708;
        Interval _12873 = footprint.v;
        Interval _12874 = Interval{ 0.0, 0.0 };
        Interval _14709 = jet_mul_derivative(_12873, _12874, intervalFailed, optical_product_upper);
        Interval _12875 = _14709;
        Interval _14710 = jet_add_derivative(_12872, _12875, intervalFailed);
        bool _14717;
        if (_14704.lo <= 0.0)
        {
            _14717 = _14704.hi >= 0.0;
        }
        else
        {
            _14717 = false;
        }
        float _14724;
        if (_14717)
        {
            _14724 = 0.0;
        }
        else
        {
            _14724 = precise::min(abs(_14704.lo), abs(_14704.hi));
        }
        float _14727 = precise::max(abs(_14704.lo), abs(_14704.hi));
        float _12852 = spvFMul(_14724, _14724);
        float _14728 = interval_down(_12852, intervalFailed);
        float _14729 = precise::max(0.0, _14728);
        float _12853 = spvFMul(_14727, _14727);
        float _14730 = interval_up(_12853, intervalFailed);
        Interval _12854 = Interval{ 2.0, 2.0 };
        Interval _12855 = _14704;
        Interval _14731 = imul(_12854, _12855, intervalFailed, optical_product_upper);
        Interval _12856 = _14731;
        Interval _12857 = _14707;
        Interval _14732 = jet_mul_derivative(_12856, _12857, intervalFailed, optical_product_upper);
        Interval _12858 = Interval{ 2.0, 2.0 };
        Interval _12859 = _14704;
        Interval _14733 = imul(_12858, _12859, intervalFailed, optical_product_upper);
        Interval _12860 = _14733;
        Interval _12861 = _14710;
        Interval _14734 = jet_mul_derivative(_12860, _12861, intervalFailed, optical_product_upper);
        bool _14739;
        if (_14729 <= 0.0)
        {
            _14739 = _14730 >= 0.0;
        }
        else
        {
            _14739 = false;
        }
        float _14746;
        if (_14739)
        {
            _14746 = 0.0;
        }
        else
        {
            _14746 = precise::min(abs(_14729), abs(_14730));
        }
        float _14749 = precise::max(abs(_14729), abs(_14730));
        float _12842 = spvFMul(_14746, _14746);
        float _14750 = interval_down(_12842, intervalFailed);
        float _12843 = spvFMul(_14749, _14749);
        float _14752 = interval_up(_12843, intervalFailed);
        Interval _12844 = Interval{ 2.0, 2.0 };
        Interval _12845 = Interval{ _14729, _14730 };
        Interval _14754 = imul(_12844, _12845, intervalFailed, optical_product_upper);
        Interval _12846 = _14754;
        Interval _12847 = _14732;
        Interval _14755 = jet_mul_derivative(_12846, _12847, intervalFailed, optical_product_upper);
        Interval _12848 = Interval{ 2.0, 2.0 };
        Interval _12849 = Interval{ _14729, _14730 };
        Interval _14757 = imul(_12848, _12849, intervalFailed, optical_product_upper);
        Interval _12850 = _14757;
        Interval _12851 = _14734;
        Interval _14758 = jet_mul_derivative(_12850, _12851, intervalFailed, optical_product_upper);
        Interval _12836 = Interval{ 1.0, 1.0 };
        Interval _12837 = Interval{ precise::max(0.0, _14750), _14752 };
        Interval _14760 = iadd(_12836, _12837, intervalFailed);
        Interval _12838 = Interval{ 0.0, 0.0 };
        Interval _12839 = _14755;
        Interval _14761 = jet_add_derivative(_12838, _12839, intervalFailed);
        Interval _12840 = Interval{ 0.0, 0.0 };
        Interval _12841 = _14758;
        Interval _14762 = jet_add_derivative(_12840, _12841, intervalFailed);
        Interval _12822 = Interval{ 1.0, 1.0 };
        Interval _12823 = _14760;
        Interval _14764 = idiv(_12822, _12823, intervalFailed, interval_divide_upper);
        bool _14771;
        if (!intervalFailed)
        {
            _14771 = intervalFailed;
        }
        else
        {
            _14771 = false;
        }
        bool _14776;
        if (_14771)
        {
            _14776 = jetFailureSite == 0u;
        }
        else
        {
            _14776 = false;
        }
        if (_14776)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14760.lo, _14760.hi);
        }
        Interval _12824 = Interval{ 0.0, 0.0 };
        Interval _12825 = _14764;
        Interval _12826 = _14761;
        Interval _14780 = jet_mul_derivative(_12825, _12826, intervalFailed, optical_product_upper);
        Interval _12827 = Interval{ as_type<float>(as_type<uint>(_14780.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14780.lo) ^ 2147483648u) };
        Interval _14790 = jet_add_derivative(_12824, _12827, intervalFailed);
        Interval _12828 = _14790;
        Interval _12829 = _14760;
        Interval _14791 = jet_div_derivative(_12828, _12829, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12830 = Interval{ 0.0, 0.0 };
        Interval _12831 = _14764;
        Interval _12832 = _14762;
        Interval _14792 = jet_mul_derivative(_12831, _12832, intervalFailed, optical_product_upper);
        Interval _12833 = Interval{ as_type<float>(as_type<uint>(_14792.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14792.lo) ^ 2147483648u) };
        Interval _14802 = jet_add_derivative(_12830, _12833, intervalFailed);
        Interval _12834 = _14802;
        Interval _12835 = _14760;
        Interval _14803 = jet_div_derivative(_12834, _12835, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12808 = _14677;
        Interval _12809 = _14764;
        Interval _14804 = imul(_12808, _12809, intervalFailed, optical_product_upper);
        Interval _12810 = _14681;
        Interval _12811 = _14764;
        Interval _14805 = jet_mul_derivative(_12810, _12811, intervalFailed, optical_product_upper);
        Interval _12812 = _14805;
        Interval _12813 = _14677;
        Interval _12814 = _14791;
        Interval _14806 = jet_mul_derivative(_12813, _12814, intervalFailed, optical_product_upper);
        Interval _12815 = _14806;
        Interval _14807 = jet_add_derivative(_12812, _12815, intervalFailed);
        Interval _12816 = _14685;
        Interval _12817 = _14764;
        Interval _14808 = jet_mul_derivative(_12816, _12817, intervalFailed, optical_product_upper);
        Interval _12818 = _14808;
        Interval _12819 = _14677;
        Interval _12820 = _14803;
        Interval _14809 = jet_mul_derivative(_12819, _12820, intervalFailed, optical_product_upper);
        Interval _12821 = _14809;
        Interval _14810 = jet_add_derivative(_12818, _12821, intervalFailed);
        float _12806 = 25.0;
        float _12807 = 1000.0;
        Interval _14812 = iratio(_12806, _12807, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14817;
        if (!intervalFailed)
        {
            _14817 = intervalFailed;
        }
        else
        {
            _14817 = false;
        }
        bool _14822;
        if (_14817)
        {
            _14822 = jetFailureSite == 0u;
        }
        else
        {
            _14822 = false;
        }
        if (_14822)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        float _12800 = _14510.lo;
        float _12801 = _14510.hi;
        float _14827 = sine_bounds(_12800, _12801, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14831 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14834 = as_type<float>(as_type<uint>(_14827) ^ 2147483648u);
        Interval _12796 = _14510;
        float _12794 = 3.1415927410125732421875;
        float _14835 = interval_down(_12794, intervalFailed);
        float _12795 = 3.1415927410125732421875;
        float _14836 = interval_up(_12795, intervalFailed);
        Interval _12797 = Interval{ _14835, _14836 };
        Interval _12798 = Interval{ 0.5, 0.5 };
        Interval _14838 = imul(_12797, _12798, intervalFailed, optical_product_upper);
        Interval _12799 = _14838;
        Interval _14839 = iadd(_12796, _12799, intervalFailed);
        float _12792 = _14839.lo;
        float _12793 = _14839.hi;
        float _14842 = sine_bounds(_12792, _12793, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12802 = Interval{ _14831, _14834 };
        Interval _12803 = _14511;
        Interval _14845 = jet_mul_derivative(_12802, _12803, intervalFailed, optical_product_upper);
        Interval _12804 = Interval{ _14831, _14834 };
        Interval _12805 = _14512;
        Interval _14847 = jet_mul_derivative(_12804, _12805, intervalFailed, optical_product_upper);
        Interval _12778 = _14812;
        Interval _12779 = Interval{ _14842, interval_sine_upper };
        Interval _14849 = imul(_12778, _12779, intervalFailed, optical_product_upper);
        Interval _12780 = Interval{ 0.0, 0.0 };
        Interval _12781 = Interval{ _14842, interval_sine_upper };
        Interval _14851 = jet_mul_derivative(_12780, _12781, intervalFailed, optical_product_upper);
        Interval _12782 = _14851;
        Interval _12783 = _14812;
        Interval _12784 = _14845;
        Interval _14852 = jet_mul_derivative(_12783, _12784, intervalFailed, optical_product_upper);
        Interval _12785 = _14852;
        Interval _14853 = jet_add_derivative(_12782, _12785, intervalFailed);
        Interval _12786 = Interval{ 0.0, 0.0 };
        Interval _12787 = Interval{ _14842, interval_sine_upper };
        Interval _14855 = jet_mul_derivative(_12786, _12787, intervalFailed, optical_product_upper);
        Interval _12788 = _14855;
        Interval _12789 = _14812;
        Interval _12790 = _14847;
        Interval _14856 = jet_mul_derivative(_12789, _12790, intervalFailed, optical_product_upper);
        Interval _12791 = _14856;
        Interval _14857 = jet_add_derivative(_12788, _12791, intervalFailed);
        float _12776 = 54.0;
        float _12777 = 1000.0;
        Interval _14860 = iratio(_12776, _12777, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14865;
        if (!intervalFailed)
        {
            _14865 = intervalFailed;
        }
        else
        {
            _14865 = false;
        }
        bool _14870;
        if (_14865)
        {
            _14870 = jetFailureSite == 0u;
        }
        else
        {
            _14870 = false;
        }
        if (_14870)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12762 = footprint.v;
        Interval _12763 = _14860;
        Interval _14876 = imul(_12762, _12763, intervalFailed, optical_product_upper);
        Interval _12764 = footprint.dx;
        Interval _12765 = _14860;
        Interval _14877 = jet_mul_derivative(_12764, _12765, intervalFailed, optical_product_upper);
        Interval _12766 = _14877;
        Interval _12767 = footprint.v;
        Interval _12768 = Interval{ 0.0, 0.0 };
        Interval _14878 = jet_mul_derivative(_12767, _12768, intervalFailed, optical_product_upper);
        Interval _12769 = _14878;
        Interval _14879 = jet_add_derivative(_12766, _12769, intervalFailed);
        Interval _12770 = footprint.dy;
        Interval _12771 = _14860;
        Interval _14880 = jet_mul_derivative(_12770, _12771, intervalFailed, optical_product_upper);
        Interval _12772 = _14880;
        Interval _12773 = footprint.v;
        Interval _12774 = Interval{ 0.0, 0.0 };
        Interval _14881 = jet_mul_derivative(_12773, _12774, intervalFailed, optical_product_upper);
        Interval _12775 = _14881;
        Interval _14882 = jet_add_derivative(_12772, _12775, intervalFailed);
        bool _14889;
        if (_14876.lo <= 0.0)
        {
            _14889 = _14876.hi >= 0.0;
        }
        else
        {
            _14889 = false;
        }
        float _14896;
        if (_14889)
        {
            _14896 = 0.0;
        }
        else
        {
            _14896 = precise::min(abs(_14876.lo), abs(_14876.hi));
        }
        float _14899 = precise::max(abs(_14876.lo), abs(_14876.hi));
        float _12752 = spvFMul(_14896, _14896);
        float _14900 = interval_down(_12752, intervalFailed);
        float _14901 = precise::max(0.0, _14900);
        float _12753 = spvFMul(_14899, _14899);
        float _14902 = interval_up(_12753, intervalFailed);
        Interval _12754 = Interval{ 2.0, 2.0 };
        Interval _12755 = _14876;
        Interval _14903 = imul(_12754, _12755, intervalFailed, optical_product_upper);
        Interval _12756 = _14903;
        Interval _12757 = _14879;
        Interval _14904 = jet_mul_derivative(_12756, _12757, intervalFailed, optical_product_upper);
        Interval _12758 = Interval{ 2.0, 2.0 };
        Interval _12759 = _14876;
        Interval _14905 = imul(_12758, _12759, intervalFailed, optical_product_upper);
        Interval _12760 = _14905;
        Interval _12761 = _14882;
        Interval _14906 = jet_mul_derivative(_12760, _12761, intervalFailed, optical_product_upper);
        bool _14911;
        if (_14901 <= 0.0)
        {
            _14911 = _14902 >= 0.0;
        }
        else
        {
            _14911 = false;
        }
        float _14918;
        if (_14911)
        {
            _14918 = 0.0;
        }
        else
        {
            _14918 = precise::min(abs(_14901), abs(_14902));
        }
        float _14921 = precise::max(abs(_14901), abs(_14902));
        float _12742 = spvFMul(_14918, _14918);
        float _14922 = interval_down(_12742, intervalFailed);
        float _12743 = spvFMul(_14921, _14921);
        float _14924 = interval_up(_12743, intervalFailed);
        Interval _12744 = Interval{ 2.0, 2.0 };
        Interval _12745 = Interval{ _14901, _14902 };
        Interval _14926 = imul(_12744, _12745, intervalFailed, optical_product_upper);
        Interval _12746 = _14926;
        Interval _12747 = _14904;
        Interval _14927 = jet_mul_derivative(_12746, _12747, intervalFailed, optical_product_upper);
        Interval _12748 = Interval{ 2.0, 2.0 };
        Interval _12749 = Interval{ _14901, _14902 };
        Interval _14929 = imul(_12748, _12749, intervalFailed, optical_product_upper);
        Interval _12750 = _14929;
        Interval _12751 = _14906;
        Interval _14930 = jet_mul_derivative(_12750, _12751, intervalFailed, optical_product_upper);
        Interval _12736 = Interval{ 1.0, 1.0 };
        Interval _12737 = Interval{ precise::max(0.0, _14922), _14924 };
        Interval _14932 = iadd(_12736, _12737, intervalFailed);
        Interval _12738 = Interval{ 0.0, 0.0 };
        Interval _12739 = _14927;
        Interval _14933 = jet_add_derivative(_12738, _12739, intervalFailed);
        Interval _12740 = Interval{ 0.0, 0.0 };
        Interval _12741 = _14930;
        Interval _14934 = jet_add_derivative(_12740, _12741, intervalFailed);
        Interval _12722 = Interval{ 1.0, 1.0 };
        Interval _12723 = _14932;
        Interval _14936 = idiv(_12722, _12723, intervalFailed, interval_divide_upper);
        bool _14943;
        if (!intervalFailed)
        {
            _14943 = intervalFailed;
        }
        else
        {
            _14943 = false;
        }
        bool _14948;
        if (_14943)
        {
            _14948 = jetFailureSite == 0u;
        }
        else
        {
            _14948 = false;
        }
        if (_14948)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14932.lo, _14932.hi);
        }
        Interval _12724 = Interval{ 0.0, 0.0 };
        Interval _12725 = _14936;
        Interval _12726 = _14933;
        Interval _14952 = jet_mul_derivative(_12725, _12726, intervalFailed, optical_product_upper);
        Interval _12727 = Interval{ as_type<float>(as_type<uint>(_14952.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14952.lo) ^ 2147483648u) };
        Interval _14962 = jet_add_derivative(_12724, _12727, intervalFailed);
        Interval _12728 = _14962;
        Interval _12729 = _14932;
        Interval _14963 = jet_div_derivative(_12728, _12729, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12730 = Interval{ 0.0, 0.0 };
        Interval _12731 = _14936;
        Interval _12732 = _14934;
        Interval _14964 = jet_mul_derivative(_12731, _12732, intervalFailed, optical_product_upper);
        Interval _12733 = Interval{ as_type<float>(as_type<uint>(_14964.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14964.lo) ^ 2147483648u) };
        Interval _14974 = jet_add_derivative(_12730, _12733, intervalFailed);
        Interval _12734 = _14974;
        Interval _12735 = _14932;
        Interval _14975 = jet_div_derivative(_12734, _12735, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12708 = _14849;
        Interval _12709 = _14936;
        Interval _14976 = imul(_12708, _12709, intervalFailed, optical_product_upper);
        Interval _12710 = _14853;
        Interval _12711 = _14936;
        Interval _14977 = jet_mul_derivative(_12710, _12711, intervalFailed, optical_product_upper);
        Interval _12712 = _14977;
        Interval _12713 = _14849;
        Interval _12714 = _14963;
        Interval _14978 = jet_mul_derivative(_12713, _12714, intervalFailed, optical_product_upper);
        Interval _12715 = _14978;
        Interval _14979 = jet_add_derivative(_12712, _12715, intervalFailed);
        Interval _12716 = _14857;
        Interval _12717 = _14936;
        Interval _14980 = jet_mul_derivative(_12716, _12717, intervalFailed, optical_product_upper);
        Interval _12718 = _14980;
        Interval _12719 = _14849;
        Interval _12720 = _14975;
        Interval _14981 = jet_mul_derivative(_12719, _12720, intervalFailed, optical_product_upper);
        Interval _12721 = _14981;
        Interval _14982 = jet_add_derivative(_12718, _12721, intervalFailed);
        Interval _12702 = _14804;
        Interval _12703 = _14976;
        Interval _14983 = iadd(_12702, _12703, intervalFailed);
        Interval _12704 = _14807;
        Interval _12705 = _14979;
        Interval _14984 = jet_add_derivative(_12704, _12705, intervalFailed);
        Interval _12706 = _14810;
        Interval _12707 = _14982;
        Interval _14985 = jet_add_derivative(_12706, _12707, intervalFailed);
        float _12700 = 45.0;
        float _12701 = 1000.0;
        Interval _14993 = iratio(_12700, _12701, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14998;
        if (!intervalFailed)
        {
            _14998 = intervalFailed;
        }
        else
        {
            _14998 = false;
        }
        bool _15003;
        if (_14998)
        {
            _15003 = jetFailureSite == 0u;
        }
        else
        {
            _15003 = false;
        }
        if (_15003)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
        }
        float _12694 = _14634.lo;
        float _12695 = _14634.hi;
        float _15008 = sine_bounds(_12694, _12695, intervalFailed, optical_product_upper, interval_sine_upper);
        float _15012 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _15015 = as_type<float>(as_type<uint>(_15008) ^ 2147483648u);
        Interval _12690 = _14634;
        float _12688 = 3.1415927410125732421875;
        float _15016 = interval_down(_12688, intervalFailed);
        float _12689 = 3.1415927410125732421875;
        float _15017 = interval_up(_12689, intervalFailed);
        Interval _12691 = Interval{ _15016, _15017 };
        Interval _12692 = Interval{ 0.5, 0.5 };
        Interval _15019 = imul(_12691, _12692, intervalFailed, optical_product_upper);
        Interval _12693 = _15019;
        Interval _15020 = iadd(_12690, _12693, intervalFailed);
        float _12686 = _15020.lo;
        float _12687 = _15020.hi;
        float _15023 = sine_bounds(_12686, _12687, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12696 = Interval{ _15012, _15015 };
        Interval _12697 = _14636;
        Interval _15026 = jet_mul_derivative(_12696, _12697, intervalFailed, optical_product_upper);
        Interval _12698 = Interval{ _15012, _15015 };
        Interval _12699 = _14638;
        Interval _15028 = jet_mul_derivative(_12698, _12699, intervalFailed, optical_product_upper);
        Interval _12672 = _14993;
        Interval _12673 = Interval{ _15023, interval_sine_upper };
        Interval _15030 = imul(_12672, _12673, intervalFailed, optical_product_upper);
        Interval _12674 = Interval{ 0.0, 0.0 };
        Interval _12675 = Interval{ _15023, interval_sine_upper };
        Interval _15032 = jet_mul_derivative(_12674, _12675, intervalFailed, optical_product_upper);
        Interval _12676 = _15032;
        Interval _12677 = _14993;
        Interval _12678 = _15026;
        Interval _15033 = jet_mul_derivative(_12677, _12678, intervalFailed, optical_product_upper);
        Interval _12679 = _15033;
        Interval _15034 = jet_add_derivative(_12676, _12679, intervalFailed);
        Interval _12680 = Interval{ 0.0, 0.0 };
        Interval _12681 = Interval{ _15023, interval_sine_upper };
        Interval _15036 = jet_mul_derivative(_12680, _12681, intervalFailed, optical_product_upper);
        Interval _12682 = _15036;
        Interval _12683 = _14993;
        Interval _12684 = _15028;
        Interval _15037 = jet_mul_derivative(_12683, _12684, intervalFailed, optical_product_upper);
        Interval _12685 = _15037;
        Interval _15038 = jet_add_derivative(_12682, _12685, intervalFailed);
        float _12670 = 24.0;
        float _12671 = 1000.0;
        Interval _15041 = iratio(_12670, _12671, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _15046;
        if (!intervalFailed)
        {
            _15046 = intervalFailed;
        }
        else
        {
            _15046 = false;
        }
        bool _15051;
        if (_15046)
        {
            _15051 = jetFailureSite == 0u;
        }
        else
        {
            _15051 = false;
        }
        if (_15051)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
        }
        Interval _12656 = footprint.v;
        Interval _12657 = _15041;
        Interval _15057 = imul(_12656, _12657, intervalFailed, optical_product_upper);
        Interval _12658 = footprint.dx;
        Interval _12659 = _15041;
        Interval _15058 = jet_mul_derivative(_12658, _12659, intervalFailed, optical_product_upper);
        Interval _12660 = _15058;
        Interval _12661 = footprint.v;
        Interval _12662 = Interval{ 0.0, 0.0 };
        Interval _15059 = jet_mul_derivative(_12661, _12662, intervalFailed, optical_product_upper);
        Interval _12663 = _15059;
        Interval _15060 = jet_add_derivative(_12660, _12663, intervalFailed);
        Interval _12664 = footprint.dy;
        Interval _12665 = _15041;
        Interval _15061 = jet_mul_derivative(_12664, _12665, intervalFailed, optical_product_upper);
        Interval _12666 = _15061;
        Interval _12667 = footprint.v;
        Interval _12668 = Interval{ 0.0, 0.0 };
        Interval _15062 = jet_mul_derivative(_12667, _12668, intervalFailed, optical_product_upper);
        Interval _12669 = _15062;
        Interval _15063 = jet_add_derivative(_12666, _12669, intervalFailed);
        bool _15070;
        if (_15057.lo <= 0.0)
        {
            _15070 = _15057.hi >= 0.0;
        }
        else
        {
            _15070 = false;
        }
        float _15077;
        if (_15070)
        {
            _15077 = 0.0;
        }
        else
        {
            _15077 = precise::min(abs(_15057.lo), abs(_15057.hi));
        }
        float _15080 = precise::max(abs(_15057.lo), abs(_15057.hi));
        float _12646 = spvFMul(_15077, _15077);
        float _15081 = interval_down(_12646, intervalFailed);
        float _15082 = precise::max(0.0, _15081);
        float _12647 = spvFMul(_15080, _15080);
        float _15083 = interval_up(_12647, intervalFailed);
        Interval _12648 = Interval{ 2.0, 2.0 };
        Interval _12649 = _15057;
        Interval _15084 = imul(_12648, _12649, intervalFailed, optical_product_upper);
        Interval _12650 = _15084;
        Interval _12651 = _15060;
        Interval _15085 = jet_mul_derivative(_12650, _12651, intervalFailed, optical_product_upper);
        Interval _12652 = Interval{ 2.0, 2.0 };
        Interval _12653 = _15057;
        Interval _15086 = imul(_12652, _12653, intervalFailed, optical_product_upper);
        Interval _12654 = _15086;
        Interval _12655 = _15063;
        Interval _15087 = jet_mul_derivative(_12654, _12655, intervalFailed, optical_product_upper);
        bool _15092;
        if (_15082 <= 0.0)
        {
            _15092 = _15083 >= 0.0;
        }
        else
        {
            _15092 = false;
        }
        float _15099;
        if (_15092)
        {
            _15099 = 0.0;
        }
        else
        {
            _15099 = precise::min(abs(_15082), abs(_15083));
        }
        float _15102 = precise::max(abs(_15082), abs(_15083));
        float _12636 = spvFMul(_15099, _15099);
        float _15103 = interval_down(_12636, intervalFailed);
        float _12637 = spvFMul(_15102, _15102);
        float _15105 = interval_up(_12637, intervalFailed);
        Interval _12638 = Interval{ 2.0, 2.0 };
        Interval _12639 = Interval{ _15082, _15083 };
        Interval _15107 = imul(_12638, _12639, intervalFailed, optical_product_upper);
        Interval _12640 = _15107;
        Interval _12641 = _15085;
        Interval _15108 = jet_mul_derivative(_12640, _12641, intervalFailed, optical_product_upper);
        Interval _12642 = Interval{ 2.0, 2.0 };
        Interval _12643 = Interval{ _15082, _15083 };
        Interval _15110 = imul(_12642, _12643, intervalFailed, optical_product_upper);
        Interval _12644 = _15110;
        Interval _12645 = _15087;
        Interval _15111 = jet_mul_derivative(_12644, _12645, intervalFailed, optical_product_upper);
        Interval _12630 = Interval{ 1.0, 1.0 };
        Interval _12631 = Interval{ precise::max(0.0, _15103), _15105 };
        Interval _15113 = iadd(_12630, _12631, intervalFailed);
        Interval _12632 = Interval{ 0.0, 0.0 };
        Interval _12633 = _15108;
        Interval _15114 = jet_add_derivative(_12632, _12633, intervalFailed);
        Interval _12634 = Interval{ 0.0, 0.0 };
        Interval _12635 = _15111;
        Interval _15115 = jet_add_derivative(_12634, _12635, intervalFailed);
        Interval _12616 = Interval{ 1.0, 1.0 };
        Interval _12617 = _15113;
        Interval _15117 = idiv(_12616, _12617, intervalFailed, interval_divide_upper);
        bool _15124;
        if (!intervalFailed)
        {
            _15124 = intervalFailed;
        }
        else
        {
            _15124 = false;
        }
        bool _15129;
        if (_15124)
        {
            _15129 = jetFailureSite == 0u;
        }
        else
        {
            _15129 = false;
        }
        if (_15129)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _15113.lo, _15113.hi);
        }
        Interval _12618 = Interval{ 0.0, 0.0 };
        Interval _12619 = _15117;
        Interval _12620 = _15114;
        Interval _15133 = jet_mul_derivative(_12619, _12620, intervalFailed, optical_product_upper);
        Interval _12621 = Interval{ as_type<float>(as_type<uint>(_15133.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15133.lo) ^ 2147483648u) };
        Interval _15143 = jet_add_derivative(_12618, _12621, intervalFailed);
        Interval _12622 = _15143;
        Interval _12623 = _15113;
        Interval _15144 = jet_div_derivative(_12622, _12623, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12624 = Interval{ 0.0, 0.0 };
        Interval _12625 = _15117;
        Interval _12626 = _15115;
        Interval _15145 = jet_mul_derivative(_12625, _12626, intervalFailed, optical_product_upper);
        Interval _12627 = Interval{ as_type<float>(as_type<uint>(_15145.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15145.lo) ^ 2147483648u) };
        Interval _15155 = jet_add_derivative(_12624, _12627, intervalFailed);
        Interval _12628 = _15155;
        Interval _12629 = _15113;
        Interval _15156 = jet_div_derivative(_12628, _12629, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12602 = _15030;
        Interval _12603 = _15117;
        Interval _15157 = imul(_12602, _12603, intervalFailed, optical_product_upper);
        Interval _12604 = _15034;
        Interval _12605 = _15117;
        Interval _15158 = jet_mul_derivative(_12604, _12605, intervalFailed, optical_product_upper);
        Interval _12606 = _15158;
        Interval _12607 = _15030;
        Interval _12608 = _15144;
        Interval _15159 = jet_mul_derivative(_12607, _12608, intervalFailed, optical_product_upper);
        Interval _12609 = _15159;
        Interval _15160 = jet_add_derivative(_12606, _12609, intervalFailed);
        Interval _12610 = _15038;
        Interval _12611 = _15117;
        Interval _15161 = jet_mul_derivative(_12610, _12611, intervalFailed, optical_product_upper);
        Interval _12612 = _15161;
        Interval _12613 = _15030;
        Interval _12614 = _15156;
        Interval _15162 = jet_mul_derivative(_12613, _12614, intervalFailed, optical_product_upper);
        Interval _12615 = _15162;
        Interval _15163 = jet_add_derivative(_12612, _12615, intervalFailed);
        float _12600 = 20.0;
        float _12601 = 1000.0;
        Interval _15165 = iratio(_12600, _12601, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _15170;
        if (!intervalFailed)
        {
            _15170 = intervalFailed;
        }
        else
        {
            _15170 = false;
        }
        bool _15175;
        if (_15170)
        {
            _15175 = jetFailureSite == 0u;
        }
        else
        {
            _15175 = false;
        }
        if (_15175)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(20.0, 20.0, 1000.0, 1000.0);
        }
        float _12594 = _14510.lo;
        float _12595 = _14510.hi;
        float _15180 = sine_bounds(_12594, _12595, intervalFailed, optical_product_upper, interval_sine_upper);
        float _15184 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _15187 = as_type<float>(as_type<uint>(_15180) ^ 2147483648u);
        Interval _12590 = _14510;
        float _12588 = 3.1415927410125732421875;
        float _15188 = interval_down(_12588, intervalFailed);
        float _12589 = 3.1415927410125732421875;
        float _15189 = interval_up(_12589, intervalFailed);
        Interval _12591 = Interval{ _15188, _15189 };
        Interval _12592 = Interval{ 0.5, 0.5 };
        Interval _15191 = imul(_12591, _12592, intervalFailed, optical_product_upper);
        Interval _12593 = _15191;
        Interval _15192 = iadd(_12590, _12593, intervalFailed);
        float _12586 = _15192.lo;
        float _12587 = _15192.hi;
        float _15195 = sine_bounds(_12586, _12587, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12596 = Interval{ _15184, _15187 };
        Interval _12597 = _14511;
        Interval _15198 = jet_mul_derivative(_12596, _12597, intervalFailed, optical_product_upper);
        Interval _12598 = Interval{ _15184, _15187 };
        Interval _12599 = _14512;
        Interval _15200 = jet_mul_derivative(_12598, _12599, intervalFailed, optical_product_upper);
        Interval _12572 = _15165;
        Interval _12573 = Interval{ _15195, interval_sine_upper };
        Interval _15202 = imul(_12572, _12573, intervalFailed, optical_product_upper);
        Interval _12574 = Interval{ 0.0, 0.0 };
        Interval _12575 = Interval{ _15195, interval_sine_upper };
        Interval _15204 = jet_mul_derivative(_12574, _12575, intervalFailed, optical_product_upper);
        Interval _12576 = _15204;
        Interval _12577 = _15165;
        Interval _12578 = _15198;
        Interval _15205 = jet_mul_derivative(_12577, _12578, intervalFailed, optical_product_upper);
        Interval _12579 = _15205;
        Interval _15206 = jet_add_derivative(_12576, _12579, intervalFailed);
        Interval _12580 = Interval{ 0.0, 0.0 };
        Interval _12581 = Interval{ _15195, interval_sine_upper };
        Interval _15208 = jet_mul_derivative(_12580, _12581, intervalFailed, optical_product_upper);
        Interval _12582 = _15208;
        Interval _12583 = _15165;
        Interval _12584 = _15200;
        Interval _15209 = jet_mul_derivative(_12583, _12584, intervalFailed, optical_product_upper);
        Interval _12585 = _15209;
        Interval _15210 = jet_add_derivative(_12582, _12585, intervalFailed);
        float _12570 = 54.0;
        float _12571 = 1000.0;
        Interval _15213 = iratio(_12570, _12571, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _15218;
        if (!intervalFailed)
        {
            _15218 = intervalFailed;
        }
        else
        {
            _15218 = false;
        }
        bool _15223;
        if (_15218)
        {
            _15223 = jetFailureSite == 0u;
        }
        else
        {
            _15223 = false;
        }
        if (_15223)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12556 = footprint.v;
        Interval _12557 = _15213;
        Interval _15229 = imul(_12556, _12557, intervalFailed, optical_product_upper);
        Interval _12558 = footprint.dx;
        Interval _12559 = _15213;
        Interval _15230 = jet_mul_derivative(_12558, _12559, intervalFailed, optical_product_upper);
        Interval _12560 = _15230;
        Interval _12561 = footprint.v;
        Interval _12562 = Interval{ 0.0, 0.0 };
        Interval _15231 = jet_mul_derivative(_12561, _12562, intervalFailed, optical_product_upper);
        Interval _12563 = _15231;
        Interval _15232 = jet_add_derivative(_12560, _12563, intervalFailed);
        Interval _12564 = footprint.dy;
        Interval _12565 = _15213;
        Interval _15233 = jet_mul_derivative(_12564, _12565, intervalFailed, optical_product_upper);
        Interval _12566 = _15233;
        Interval _12567 = footprint.v;
        Interval _12568 = Interval{ 0.0, 0.0 };
        Interval _15234 = jet_mul_derivative(_12567, _12568, intervalFailed, optical_product_upper);
        Interval _12569 = _15234;
        Interval _15235 = jet_add_derivative(_12566, _12569, intervalFailed);
        bool _15242;
        if (_15229.lo <= 0.0)
        {
            _15242 = _15229.hi >= 0.0;
        }
        else
        {
            _15242 = false;
        }
        float _15249;
        if (_15242)
        {
            _15249 = 0.0;
        }
        else
        {
            _15249 = precise::min(abs(_15229.lo), abs(_15229.hi));
        }
        float _15252 = precise::max(abs(_15229.lo), abs(_15229.hi));
        float _12546 = spvFMul(_15249, _15249);
        float _15253 = interval_down(_12546, intervalFailed);
        float _15254 = precise::max(0.0, _15253);
        float _12547 = spvFMul(_15252, _15252);
        float _15255 = interval_up(_12547, intervalFailed);
        Interval _12548 = Interval{ 2.0, 2.0 };
        Interval _12549 = _15229;
        Interval _15256 = imul(_12548, _12549, intervalFailed, optical_product_upper);
        Interval _12550 = _15256;
        Interval _12551 = _15232;
        Interval _15257 = jet_mul_derivative(_12550, _12551, intervalFailed, optical_product_upper);
        Interval _12552 = Interval{ 2.0, 2.0 };
        Interval _12553 = _15229;
        Interval _15258 = imul(_12552, _12553, intervalFailed, optical_product_upper);
        Interval _12554 = _15258;
        Interval _12555 = _15235;
        Interval _15259 = jet_mul_derivative(_12554, _12555, intervalFailed, optical_product_upper);
        bool _15264;
        if (_15254 <= 0.0)
        {
            _15264 = _15255 >= 0.0;
        }
        else
        {
            _15264 = false;
        }
        float _15271;
        if (_15264)
        {
            _15271 = 0.0;
        }
        else
        {
            _15271 = precise::min(abs(_15254), abs(_15255));
        }
        float _15274 = precise::max(abs(_15254), abs(_15255));
        float _12536 = spvFMul(_15271, _15271);
        float _15275 = interval_down(_12536, intervalFailed);
        float _12537 = spvFMul(_15274, _15274);
        float _15277 = interval_up(_12537, intervalFailed);
        Interval _12538 = Interval{ 2.0, 2.0 };
        Interval _12539 = Interval{ _15254, _15255 };
        Interval _15279 = imul(_12538, _12539, intervalFailed, optical_product_upper);
        Interval _12540 = _15279;
        Interval _12541 = _15257;
        Interval _15280 = jet_mul_derivative(_12540, _12541, intervalFailed, optical_product_upper);
        Interval _12542 = Interval{ 2.0, 2.0 };
        Interval _12543 = Interval{ _15254, _15255 };
        Interval _15282 = imul(_12542, _12543, intervalFailed, optical_product_upper);
        Interval _12544 = _15282;
        Interval _12545 = _15259;
        Interval _15283 = jet_mul_derivative(_12544, _12545, intervalFailed, optical_product_upper);
        Interval _12530 = Interval{ 1.0, 1.0 };
        Interval _12531 = Interval{ precise::max(0.0, _15275), _15277 };
        Interval _15285 = iadd(_12530, _12531, intervalFailed);
        Interval _12532 = Interval{ 0.0, 0.0 };
        Interval _12533 = _15280;
        Interval _15286 = jet_add_derivative(_12532, _12533, intervalFailed);
        Interval _12534 = Interval{ 0.0, 0.0 };
        Interval _12535 = _15283;
        Interval _15287 = jet_add_derivative(_12534, _12535, intervalFailed);
        Interval _12516 = Interval{ 1.0, 1.0 };
        Interval _12517 = _15285;
        Interval _15289 = idiv(_12516, _12517, intervalFailed, interval_divide_upper);
        bool _15296;
        if (!intervalFailed)
        {
            _15296 = intervalFailed;
        }
        else
        {
            _15296 = false;
        }
        bool _15301;
        if (_15296)
        {
            _15301 = jetFailureSite == 0u;
        }
        else
        {
            _15301 = false;
        }
        if (_15301)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _15285.lo, _15285.hi);
        }
        Interval _12518 = Interval{ 0.0, 0.0 };
        Interval _12519 = _15289;
        Interval _12520 = _15286;
        Interval _15305 = jet_mul_derivative(_12519, _12520, intervalFailed, optical_product_upper);
        Interval _12521 = Interval{ as_type<float>(as_type<uint>(_15305.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15305.lo) ^ 2147483648u) };
        Interval _15315 = jet_add_derivative(_12518, _12521, intervalFailed);
        Interval _12522 = _15315;
        Interval _12523 = _15285;
        Interval _15316 = jet_div_derivative(_12522, _12523, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12524 = Interval{ 0.0, 0.0 };
        Interval _12525 = _15289;
        Interval _12526 = _15287;
        Interval _15317 = jet_mul_derivative(_12525, _12526, intervalFailed, optical_product_upper);
        Interval _12527 = Interval{ as_type<float>(as_type<uint>(_15317.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15317.lo) ^ 2147483648u) };
        Interval _15327 = jet_add_derivative(_12524, _12527, intervalFailed);
        Interval _12528 = _15327;
        Interval _12529 = _15285;
        Interval _15328 = jet_div_derivative(_12528, _12529, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12502 = _15202;
        Interval _12503 = _15289;
        Interval _15329 = imul(_12502, _12503, intervalFailed, optical_product_upper);
        Interval _12504 = _15206;
        Interval _12505 = _15289;
        Interval _15330 = jet_mul_derivative(_12504, _12505, intervalFailed, optical_product_upper);
        Interval _12506 = _15330;
        Interval _12507 = _15202;
        Interval _12508 = _15316;
        Interval _15331 = jet_mul_derivative(_12507, _12508, intervalFailed, optical_product_upper);
        Interval _12509 = _15331;
        Interval _15332 = jet_add_derivative(_12506, _12509, intervalFailed);
        Interval _12510 = _15210;
        Interval _12511 = _15289;
        Interval _15333 = jet_mul_derivative(_12510, _12511, intervalFailed, optical_product_upper);
        Interval _12512 = _15333;
        Interval _12513 = _15202;
        Interval _12514 = _15328;
        Interval _15334 = jet_mul_derivative(_12513, _12514, intervalFailed, optical_product_upper);
        Interval _12515 = _15334;
        Interval _15335 = jet_add_derivative(_12512, _12515, intervalFailed);
        Interval _12496 = _15157;
        Interval _12497 = Interval{ as_type<float>(as_type<uint>(_15329.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15329.lo) ^ 2147483648u) };
        Interval _15361 = iadd(_12496, _12497, intervalFailed);
        Interval _12498 = _15160;
        Interval _12499 = Interval{ as_type<float>(as_type<uint>(_15332.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15332.lo) ^ 2147483648u) };
        Interval _15363 = jet_add_derivative(_12498, _12499, intervalFailed);
        Interval _12500 = _15163;
        Interval _12501 = Interval{ as_type<float>(as_type<uint>(_15335.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15335.lo) ^ 2147483648u) };
        Interval _15365 = jet_add_derivative(_12500, _12501, intervalFailed);
        _15372 = _15361.lo;
        _15373 = _15361.hi;
        _15374 = _15363.lo;
        _15375 = _15363.hi;
        _15376 = _15365.lo;
        _15377 = _15365.hi;
        _15378 = _14983.lo;
        _15379 = _14983.hi;
        _15380 = _14984.lo;
        _15381 = _14984.hi;
        _15382 = _14985.lo;
        _15383 = _14985.hi;
    }
    Interval _12482 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12483 = Interval{ _15378, _15379 };
    Interval _15393 = imul(_12482, _12483, intervalFailed, optical_product_upper);
    Interval _12484 = Interval{ 0.0, 0.0 };
    Interval _12485 = Interval{ _15378, _15379 };
    Interval _15395 = jet_mul_derivative(_12484, _12485, intervalFailed, optical_product_upper);
    Interval _12486 = _15395;
    Interval _12487 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12488 = Interval{ _15380, _15381 };
    Interval _15398 = jet_mul_derivative(_12487, _12488, intervalFailed, optical_product_upper);
    Interval _12489 = _15398;
    Interval _15399 = jet_add_derivative(_12486, _12489, intervalFailed);
    Interval _12490 = Interval{ 0.0, 0.0 };
    Interval _12491 = Interval{ _15378, _15379 };
    Interval _15401 = jet_mul_derivative(_12490, _12491, intervalFailed, optical_product_upper);
    Interval _12492 = _15401;
    Interval _12493 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12494 = Interval{ _15382, _15383 };
    Interval _15404 = jet_mul_derivative(_12493, _12494, intervalFailed, optical_product_upper);
    Interval _12495 = _15404;
    Interval _15405 = jet_add_derivative(_12492, _12495, intervalFailed);
    Interval _12468 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12469 = Interval{ -1.0, -1.0 };
    Interval _15407 = imul(_12468, _12469, intervalFailed, optical_product_upper);
    Interval _12470 = Interval{ 0.0, 0.0 };
    Interval _12471 = Interval{ -1.0, -1.0 };
    Interval _15408 = jet_mul_derivative(_12470, _12471, intervalFailed, optical_product_upper);
    Interval _12472 = _15408;
    Interval _12473 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12474 = Interval{ 0.0, 0.0 };
    Interval _15410 = jet_mul_derivative(_12473, _12474, intervalFailed, optical_product_upper);
    Interval _12475 = _15410;
    Interval _15411 = jet_add_derivative(_12472, _12475, intervalFailed);
    Interval _12476 = Interval{ 0.0, 0.0 };
    Interval _12477 = Interval{ -1.0, -1.0 };
    Interval _15412 = jet_mul_derivative(_12476, _12477, intervalFailed, optical_product_upper);
    Interval _12478 = _15412;
    Interval _12479 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12480 = Interval{ 0.0, 0.0 };
    Interval _15414 = jet_mul_derivative(_12479, _12480, intervalFailed, optical_product_upper);
    Interval _12481 = _15414;
    Interval _15415 = jet_add_derivative(_12478, _12481, intervalFailed);
    Interval _12462 = _15393;
    Interval _12463 = _15407;
    Interval _15416 = iadd(_12462, _12463, intervalFailed);
    Interval _12464 = _15399;
    Interval _12465 = _15411;
    Interval _15417 = jet_add_derivative(_12464, _12465, intervalFailed);
    Interval _12466 = _15405;
    Interval _12467 = _15415;
    Interval _15418 = jet_add_derivative(_12466, _12467, intervalFailed);
    Interval _12448 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12449 = Interval{ _15372, _15373 };
    Interval _15421 = imul(_12448, _12449, intervalFailed, optical_product_upper);
    Interval _12450 = Interval{ 0.0, 0.0 };
    Interval _12451 = Interval{ _15372, _15373 };
    Interval _15423 = jet_mul_derivative(_12450, _12451, intervalFailed, optical_product_upper);
    Interval _12452 = _15423;
    Interval _12453 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12454 = Interval{ _15374, _15375 };
    Interval _15426 = jet_mul_derivative(_12453, _12454, intervalFailed, optical_product_upper);
    Interval _12455 = _15426;
    Interval _15427 = jet_add_derivative(_12452, _12455, intervalFailed);
    Interval _12456 = Interval{ 0.0, 0.0 };
    Interval _12457 = Interval{ _15372, _15373 };
    Interval _15429 = jet_mul_derivative(_12456, _12457, intervalFailed, optical_product_upper);
    Interval _12458 = _15429;
    Interval _12459 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12460 = Interval{ _15376, _15377 };
    Interval _15432 = jet_mul_derivative(_12459, _12460, intervalFailed, optical_product_upper);
    Interval _12461 = _15432;
    Interval _15433 = jet_add_derivative(_12458, _12461, intervalFailed);
    Interval _12442 = _15416;
    Interval _12443 = _15421;
    Interval _15434 = iadd(_12442, _12443, intervalFailed);
    Interval _12444 = _15417;
    Interval _12445 = _15427;
    Interval _15435 = jet_add_derivative(_12444, _12445, intervalFailed);
    Interval _12446 = _15418;
    Interval _12447 = _15433;
    Interval _15436 = jet_add_derivative(_12446, _12447, intervalFailed);
    Interval _12428 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12429 = Interval{ _15378, _15379 };
    Interval _15442 = imul(_12428, _12429, intervalFailed, optical_product_upper);
    Interval _12430 = Interval{ 0.0, 0.0 };
    Interval _12431 = Interval{ _15378, _15379 };
    Interval _15444 = jet_mul_derivative(_12430, _12431, intervalFailed, optical_product_upper);
    Interval _12432 = _15444;
    Interval _12433 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12434 = Interval{ _15380, _15381 };
    Interval _15447 = jet_mul_derivative(_12433, _12434, intervalFailed, optical_product_upper);
    Interval _12435 = _15447;
    Interval _15448 = jet_add_derivative(_12432, _12435, intervalFailed);
    Interval _12436 = Interval{ 0.0, 0.0 };
    Interval _12437 = Interval{ _15378, _15379 };
    Interval _15450 = jet_mul_derivative(_12436, _12437, intervalFailed, optical_product_upper);
    Interval _12438 = _15450;
    Interval _12439 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12440 = Interval{ _15382, _15383 };
    Interval _15453 = jet_mul_derivative(_12439, _12440, intervalFailed, optical_product_upper);
    Interval _12441 = _15453;
    Interval _15454 = jet_add_derivative(_12438, _12441, intervalFailed);
    Interval _12414 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12415 = Interval{ -1.0, -1.0 };
    Interval _15456 = imul(_12414, _12415, intervalFailed, optical_product_upper);
    Interval _12416 = Interval{ 0.0, 0.0 };
    Interval _12417 = Interval{ -1.0, -1.0 };
    Interval _15457 = jet_mul_derivative(_12416, _12417, intervalFailed, optical_product_upper);
    Interval _12418 = _15457;
    Interval _12419 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12420 = Interval{ 0.0, 0.0 };
    Interval _15459 = jet_mul_derivative(_12419, _12420, intervalFailed, optical_product_upper);
    Interval _12421 = _15459;
    Interval _15460 = jet_add_derivative(_12418, _12421, intervalFailed);
    Interval _12422 = Interval{ 0.0, 0.0 };
    Interval _12423 = Interval{ -1.0, -1.0 };
    Interval _15461 = jet_mul_derivative(_12422, _12423, intervalFailed, optical_product_upper);
    Interval _12424 = _15461;
    Interval _12425 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12426 = Interval{ 0.0, 0.0 };
    Interval _15463 = jet_mul_derivative(_12425, _12426, intervalFailed, optical_product_upper);
    Interval _12427 = _15463;
    Interval _15464 = jet_add_derivative(_12424, _12427, intervalFailed);
    Interval _12408 = _15442;
    Interval _12409 = _15456;
    Interval _15465 = iadd(_12408, _12409, intervalFailed);
    Interval _12410 = _15448;
    Interval _12411 = _15460;
    Interval _15466 = jet_add_derivative(_12410, _12411, intervalFailed);
    Interval _12412 = _15454;
    Interval _12413 = _15464;
    Interval _15467 = jet_add_derivative(_12412, _12413, intervalFailed);
    Interval _12394 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12395 = Interval{ _15372, _15373 };
    Interval _15470 = imul(_12394, _12395, intervalFailed, optical_product_upper);
    Interval _12396 = Interval{ 0.0, 0.0 };
    Interval _12397 = Interval{ _15372, _15373 };
    Interval _15472 = jet_mul_derivative(_12396, _12397, intervalFailed, optical_product_upper);
    Interval _12398 = _15472;
    Interval _12399 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12400 = Interval{ _15374, _15375 };
    Interval _15475 = jet_mul_derivative(_12399, _12400, intervalFailed, optical_product_upper);
    Interval _12401 = _15475;
    Interval _15476 = jet_add_derivative(_12398, _12401, intervalFailed);
    Interval _12402 = Interval{ 0.0, 0.0 };
    Interval _12403 = Interval{ _15372, _15373 };
    Interval _15478 = jet_mul_derivative(_12402, _12403, intervalFailed, optical_product_upper);
    Interval _12404 = _15478;
    Interval _12405 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12406 = Interval{ _15376, _15377 };
    Interval _15481 = jet_mul_derivative(_12405, _12406, intervalFailed, optical_product_upper);
    Interval _12407 = _15481;
    Interval _15482 = jet_add_derivative(_12404, _12407, intervalFailed);
    Interval _12388 = _15465;
    Interval _12389 = _15470;
    Interval _15483 = iadd(_12388, _12389, intervalFailed);
    Interval _12390 = _15466;
    Interval _12391 = _15476;
    Interval _15484 = jet_add_derivative(_12390, _12391, intervalFailed);
    Interval _12392 = _15467;
    Interval _12393 = _15482;
    Interval _15485 = jet_add_derivative(_12392, _12393, intervalFailed);
    Interval _12374 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12375 = Interval{ _15378, _15379 };
    Interval _15491 = imul(_12374, _12375, intervalFailed, optical_product_upper);
    Interval _12376 = Interval{ 0.0, 0.0 };
    Interval _12377 = Interval{ _15378, _15379 };
    Interval _15493 = jet_mul_derivative(_12376, _12377, intervalFailed, optical_product_upper);
    Interval _12378 = _15493;
    Interval _12379 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12380 = Interval{ _15380, _15381 };
    Interval _15496 = jet_mul_derivative(_12379, _12380, intervalFailed, optical_product_upper);
    Interval _12381 = _15496;
    Interval _15497 = jet_add_derivative(_12378, _12381, intervalFailed);
    Interval _12382 = Interval{ 0.0, 0.0 };
    Interval _12383 = Interval{ _15378, _15379 };
    Interval _15499 = jet_mul_derivative(_12382, _12383, intervalFailed, optical_product_upper);
    Interval _12384 = _15499;
    Interval _12385 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12386 = Interval{ _15382, _15383 };
    Interval _15502 = jet_mul_derivative(_12385, _12386, intervalFailed, optical_product_upper);
    Interval _12387 = _15502;
    Interval _15503 = jet_add_derivative(_12384, _12387, intervalFailed);
    Interval _12360 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12361 = Interval{ -1.0, -1.0 };
    Interval _15505 = imul(_12360, _12361, intervalFailed, optical_product_upper);
    Interval _12362 = Interval{ 0.0, 0.0 };
    Interval _12363 = Interval{ -1.0, -1.0 };
    Interval _15506 = jet_mul_derivative(_12362, _12363, intervalFailed, optical_product_upper);
    Interval _12364 = _15506;
    Interval _12365 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12366 = Interval{ 0.0, 0.0 };
    Interval _15508 = jet_mul_derivative(_12365, _12366, intervalFailed, optical_product_upper);
    Interval _12367 = _15508;
    Interval _15509 = jet_add_derivative(_12364, _12367, intervalFailed);
    Interval _12368 = Interval{ 0.0, 0.0 };
    Interval _12369 = Interval{ -1.0, -1.0 };
    Interval _15510 = jet_mul_derivative(_12368, _12369, intervalFailed, optical_product_upper);
    Interval _12370 = _15510;
    Interval _12371 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12372 = Interval{ 0.0, 0.0 };
    Interval _15512 = jet_mul_derivative(_12371, _12372, intervalFailed, optical_product_upper);
    Interval _12373 = _15512;
    Interval _15513 = jet_add_derivative(_12370, _12373, intervalFailed);
    Interval _12354 = _15491;
    Interval _12355 = _15505;
    Interval _15514 = iadd(_12354, _12355, intervalFailed);
    Interval _12356 = _15497;
    Interval _12357 = _15509;
    Interval _15515 = jet_add_derivative(_12356, _12357, intervalFailed);
    Interval _12358 = _15503;
    Interval _12359 = _15513;
    Interval _15516 = jet_add_derivative(_12358, _12359, intervalFailed);
    Interval _12340 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12341 = Interval{ _15372, _15373 };
    Interval _15519 = imul(_12340, _12341, intervalFailed, optical_product_upper);
    Interval _12342 = Interval{ 0.0, 0.0 };
    Interval _12343 = Interval{ _15372, _15373 };
    Interval _15521 = jet_mul_derivative(_12342, _12343, intervalFailed, optical_product_upper);
    Interval _12344 = _15521;
    Interval _12345 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12346 = Interval{ _15374, _15375 };
    Interval _15524 = jet_mul_derivative(_12345, _12346, intervalFailed, optical_product_upper);
    Interval _12347 = _15524;
    Interval _15525 = jet_add_derivative(_12344, _12347, intervalFailed);
    Interval _12348 = Interval{ 0.0, 0.0 };
    Interval _12349 = Interval{ _15372, _15373 };
    Interval _15527 = jet_mul_derivative(_12348, _12349, intervalFailed, optical_product_upper);
    Interval _12350 = _15527;
    Interval _12351 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12352 = Interval{ _15376, _15377 };
    Interval _15530 = jet_mul_derivative(_12351, _12352, intervalFailed, optical_product_upper);
    Interval _12353 = _15530;
    Interval _15531 = jet_add_derivative(_12350, _12353, intervalFailed);
    Interval _12334 = _15514;
    Interval _12335 = _15519;
    Interval _15532 = iadd(_12334, _12335, intervalFailed);
    Interval _12336 = _15515;
    Interval _12337 = _15525;
    Interval _15533 = jet_add_derivative(_12336, _12337, intervalFailed);
    Interval _12338 = _15516;
    Interval _12339 = _15531;
    Interval _15534 = jet_add_derivative(_12338, _12339, intervalFailed);
    bool _15541;
    if (_15434.lo <= 0.0)
    {
        _15541 = _15434.hi >= 0.0;
    }
    else
    {
        _15541 = false;
    }
    float _15548;
    if (_15541)
    {
        _15548 = 0.0;
    }
    else
    {
        _15548 = precise::min(abs(_15434.lo), abs(_15434.hi));
    }
    float _15551 = precise::max(abs(_15434.lo), abs(_15434.hi));
    float _12324 = spvFMul(_15548, _15548);
    float _15552 = interval_down(_12324, intervalFailed);
    float _12325 = spvFMul(_15551, _15551);
    float _15554 = interval_up(_12325, intervalFailed);
    Interval _12326 = Interval{ 2.0, 2.0 };
    Interval _12327 = _15434;
    Interval _15555 = imul(_12326, _12327, intervalFailed, optical_product_upper);
    Interval _12328 = _15555;
    Interval _12329 = _15435;
    Interval _15556 = jet_mul_derivative(_12328, _12329, intervalFailed, optical_product_upper);
    Interval _12330 = Interval{ 2.0, 2.0 };
    Interval _12331 = _15434;
    Interval _15557 = imul(_12330, _12331, intervalFailed, optical_product_upper);
    Interval _12332 = _15557;
    Interval _12333 = _15436;
    Interval _15558 = jet_mul_derivative(_12332, _12333, intervalFailed, optical_product_upper);
    bool _15565;
    if (_15483.lo <= 0.0)
    {
        _15565 = _15483.hi >= 0.0;
    }
    else
    {
        _15565 = false;
    }
    float _15572;
    if (_15565)
    {
        _15572 = 0.0;
    }
    else
    {
        _15572 = precise::min(abs(_15483.lo), abs(_15483.hi));
    }
    float _15575 = precise::max(abs(_15483.lo), abs(_15483.hi));
    float _12314 = spvFMul(_15572, _15572);
    float _15576 = interval_down(_12314, intervalFailed);
    float _12315 = spvFMul(_15575, _15575);
    float _15578 = interval_up(_12315, intervalFailed);
    Interval _12316 = Interval{ 2.0, 2.0 };
    Interval _12317 = _15483;
    Interval _15579 = imul(_12316, _12317, intervalFailed, optical_product_upper);
    Interval _12318 = _15579;
    Interval _12319 = _15484;
    Interval _15580 = jet_mul_derivative(_12318, _12319, intervalFailed, optical_product_upper);
    Interval _12320 = Interval{ 2.0, 2.0 };
    Interval _12321 = _15483;
    Interval _15581 = imul(_12320, _12321, intervalFailed, optical_product_upper);
    Interval _12322 = _15581;
    Interval _12323 = _15485;
    Interval _15582 = jet_mul_derivative(_12322, _12323, intervalFailed, optical_product_upper);
    Interval _12308 = Interval{ precise::max(0.0, _15552), _15554 };
    Interval _12309 = Interval{ precise::max(0.0, _15576), _15578 };
    Interval _15585 = iadd(_12308, _12309, intervalFailed);
    Interval _12310 = _15556;
    Interval _12311 = _15580;
    Interval _15586 = jet_add_derivative(_12310, _12311, intervalFailed);
    Interval _12312 = _15558;
    Interval _12313 = _15582;
    Interval _15587 = jet_add_derivative(_12312, _12313, intervalFailed);
    bool _15594;
    if (_15532.lo <= 0.0)
    {
        _15594 = _15532.hi >= 0.0;
    }
    else
    {
        _15594 = false;
    }
    float _15601;
    if (_15594)
    {
        _15601 = 0.0;
    }
    else
    {
        _15601 = precise::min(abs(_15532.lo), abs(_15532.hi));
    }
    float _15604 = precise::max(abs(_15532.lo), abs(_15532.hi));
    float _12298 = spvFMul(_15601, _15601);
    float _15605 = interval_down(_12298, intervalFailed);
    float _12299 = spvFMul(_15604, _15604);
    float _15607 = interval_up(_12299, intervalFailed);
    Interval _12300 = Interval{ 2.0, 2.0 };
    Interval _12301 = _15532;
    Interval _15608 = imul(_12300, _12301, intervalFailed, optical_product_upper);
    Interval _12302 = _15608;
    Interval _12303 = _15533;
    Interval _15609 = jet_mul_derivative(_12302, _12303, intervalFailed, optical_product_upper);
    Interval _12304 = Interval{ 2.0, 2.0 };
    Interval _12305 = _15532;
    Interval _15610 = imul(_12304, _12305, intervalFailed, optical_product_upper);
    Interval _12306 = _15610;
    Interval _12307 = _15534;
    Interval _15611 = jet_mul_derivative(_12306, _12307, intervalFailed, optical_product_upper);
    Interval _12292 = _15585;
    Interval _12293 = Interval{ precise::max(0.0, _15605), _15607 };
    Interval _15613 = iadd(_12292, _12293, intervalFailed);
    Interval _12294 = _15586;
    Interval _12295 = _15609;
    Interval _15614 = jet_add_derivative(_12294, _12295, intervalFailed);
    Interval _12296 = _15587;
    Interval _12297 = _15611;
    Interval _15615 = jet_add_derivative(_12296, _12297, intervalFailed);
    Interval _12283 = _15613;
    Interval _15617 = isqrt(_12283, intervalFailed);
    bool _15625;
    if (!intervalFailed)
    {
        _15625 = intervalFailed;
    }
    else
    {
        _15625 = false;
    }
    bool _15630;
    if (_15625)
    {
        _15630 = jetFailureSite == 0u;
    }
    else
    {
        _15630 = false;
    }
    if (_15630)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_15613.lo, _15613.hi, 0.0, 0.0);
    }
    if (_15617.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _12284 = Interval{ 2.0, 2.0 };
    Interval _12285 = _15617;
    Interval _15637 = imul(_12284, _12285, intervalFailed, optical_product_upper);
    Interval _12286 = Interval{ 1.0, 1.0 };
    Interval _12287 = _15637;
    Interval _15639 = idiv(_12286, _12287, intervalFailed, interval_divide_upper);
    bool _15646;
    if (!intervalFailed)
    {
        _15646 = intervalFailed;
    }
    else
    {
        _15646 = false;
    }
    bool _15651;
    if (_15646)
    {
        _15651 = jetFailureSite == 0u;
    }
    else
    {
        _15651 = false;
    }
    if (_15651)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _15637.lo, _15637.hi);
    }
    Interval _12288 = _15639;
    Interval _12289 = _15614;
    Interval _15655 = jet_mul_derivative(_12288, _12289, intervalFailed, optical_product_upper);
    Interval _12290 = _15639;
    Interval _12291 = _15615;
    Interval _15656 = jet_mul_derivative(_12290, _12291, intervalFailed, optical_product_upper);
    Interval _12269 = Interval{ 1.0, 1.0 };
    Interval _12270 = _15617;
    Interval _15658 = idiv(_12269, _12270, intervalFailed, interval_divide_upper);
    bool _15665;
    if (!intervalFailed)
    {
        _15665 = intervalFailed;
    }
    else
    {
        _15665 = false;
    }
    bool _15670;
    if (_15665)
    {
        _15670 = jetFailureSite == 0u;
    }
    else
    {
        _15670 = false;
    }
    if (_15670)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _15617.lo, _15617.hi);
    }
    Interval _12271 = Interval{ 0.0, 0.0 };
    Interval _12272 = _15658;
    Interval _12273 = _15655;
    Interval _15674 = jet_mul_derivative(_12272, _12273, intervalFailed, optical_product_upper);
    Interval _12274 = Interval{ as_type<float>(as_type<uint>(_15674.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15674.lo) ^ 2147483648u) };
    Interval _15684 = jet_add_derivative(_12271, _12274, intervalFailed);
    Interval _12275 = _15684;
    Interval _12276 = _15617;
    Interval _15685 = jet_div_derivative(_12275, _12276, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12277 = Interval{ 0.0, 0.0 };
    Interval _12278 = _15658;
    Interval _12279 = _15656;
    Interval _15686 = jet_mul_derivative(_12278, _12279, intervalFailed, optical_product_upper);
    Interval _12280 = Interval{ as_type<float>(as_type<uint>(_15686.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15686.lo) ^ 2147483648u) };
    Interval _15696 = jet_add_derivative(_12277, _12280, intervalFailed);
    Interval _12281 = _15696;
    Interval _12282 = _15617;
    Interval _15697 = jet_div_derivative(_12281, _12282, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12255 = _15434;
    Interval _12256 = _15658;
    Interval _15698 = imul(_12255, _12256, intervalFailed, optical_product_upper);
    Interval _12257 = _15435;
    Interval _12258 = _15658;
    Interval _15699 = jet_mul_derivative(_12257, _12258, intervalFailed, optical_product_upper);
    Interval _12259 = _15699;
    Interval _12260 = _15434;
    Interval _12261 = _15685;
    Interval _15700 = jet_mul_derivative(_12260, _12261, intervalFailed, optical_product_upper);
    Interval _12262 = _15700;
    Interval _15701 = jet_add_derivative(_12259, _12262, intervalFailed);
    Interval _12263 = _15436;
    Interval _12264 = _15658;
    Interval _15702 = jet_mul_derivative(_12263, _12264, intervalFailed, optical_product_upper);
    Interval _12265 = _15702;
    Interval _12266 = _15434;
    Interval _12267 = _15697;
    Interval _15703 = jet_mul_derivative(_12266, _12267, intervalFailed, optical_product_upper);
    Interval _12268 = _15703;
    Interval _15704 = jet_add_derivative(_12265, _12268, intervalFailed);
    Interval _12241 = _15483;
    Interval _12242 = _15658;
    Interval _15705 = imul(_12241, _12242, intervalFailed, optical_product_upper);
    Interval _12243 = _15484;
    Interval _12244 = _15658;
    Interval _15706 = jet_mul_derivative(_12243, _12244, intervalFailed, optical_product_upper);
    Interval _12245 = _15706;
    Interval _12246 = _15483;
    Interval _12247 = _15685;
    Interval _15707 = jet_mul_derivative(_12246, _12247, intervalFailed, optical_product_upper);
    Interval _12248 = _15707;
    Interval _15708 = jet_add_derivative(_12245, _12248, intervalFailed);
    Interval _12249 = _15485;
    Interval _12250 = _15658;
    Interval _15709 = jet_mul_derivative(_12249, _12250, intervalFailed, optical_product_upper);
    Interval _12251 = _15709;
    Interval _12252 = _15483;
    Interval _12253 = _15697;
    Interval _15710 = jet_mul_derivative(_12252, _12253, intervalFailed, optical_product_upper);
    Interval _12254 = _15710;
    Interval _15711 = jet_add_derivative(_12251, _12254, intervalFailed);
    Interval _12227 = _15532;
    Interval _12228 = _15658;
    Interval _15712 = imul(_12227, _12228, intervalFailed, optical_product_upper);
    Interval _12229 = _15533;
    Interval _12230 = _15658;
    Interval _15713 = jet_mul_derivative(_12229, _12230, intervalFailed, optical_product_upper);
    Interval _12231 = _15713;
    Interval _12232 = _15532;
    Interval _12233 = _15685;
    Interval _15714 = jet_mul_derivative(_12232, _12233, intervalFailed, optical_product_upper);
    Interval _12234 = _15714;
    Interval _15715 = jet_add_derivative(_12231, _12234, intervalFailed);
    Interval _12235 = _15534;
    Interval _12236 = _15658;
    Interval _15716 = jet_mul_derivative(_12235, _12236, intervalFailed, optical_product_upper);
    Interval _12237 = _15716;
    Interval _12238 = _15532;
    Interval _12239 = _15697;
    Interval _15717 = jet_mul_derivative(_12238, _12239, intervalFailed, optical_product_upper);
    Interval _12240 = _15717;
    Interval _15718 = jet_add_derivative(_12237, _12240, intervalFailed);
    OpticalJet3 param_var_n = OpticalJet3{ OpticalJet{ _15698, _15701, _15704 }, OpticalJet{ _15705, _15708, _15711 }, OpticalJet{ _15712, _15715, _15718 } };
    OpticalJet3 param_var_direction = direction;
    OpticalJet3 _15724 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
    return _15724;
}

static inline __attribute__((always_inline))
bool jet_inside_face(thread const ReflectionSpecularPlane& plane, thread const OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _15817;
    if (plane.a.w == 2.0)
    {
        _15817 = plane.b.w == 2.0;
    }
    else
    {
        _15817 = false;
    }
    bool _15822;
    if (_15817)
    {
        _15822 = plane.c.w == 2.0;
    }
    else
    {
        _15822 = false;
    }
    if (_15822)
    {
        return true;
    }
    Interval _15801 = Interval{ plane.b.x, plane.b.x };
    Interval _15802 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15853 = iadd(_15801, _15802, intervalFailed);
    Interval _15803 = Interval{ plane.b.y, plane.b.y };
    Interval _15804 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15856 = iadd(_15803, _15804, intervalFailed);
    Interval _15805 = Interval{ plane.b.z, plane.b.z };
    Interval _15806 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15859 = iadd(_15805, _15806, intervalFailed);
    Interval _15795 = Interval{ plane.c.x, plane.c.x };
    Interval _15796 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15890 = iadd(_15795, _15796, intervalFailed);
    Interval _15797 = Interval{ plane.c.y, plane.c.y };
    Interval _15798 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15893 = iadd(_15797, _15798, intervalFailed);
    Interval _15799 = Interval{ plane.c.z, plane.c.z };
    Interval _15800 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15896 = iadd(_15799, _15800, intervalFailed);
    Interval _15789 = hit.x.v;
    Interval _15790 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15927 = iadd(_15789, _15790, intervalFailed);
    Interval _15791 = hit.y.v;
    Interval _15792 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15929 = iadd(_15791, _15792, intervalFailed);
    Interval _15793 = hit.z.v;
    Interval _15794 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15931 = iadd(_15793, _15794, intervalFailed);
    Interval _15779 = _15853;
    Interval _15780 = _15853;
    Interval _15932 = imul(_15779, _15780, intervalFailed, optical_product_upper);
    Interval _15781 = _15932;
    Interval _15782 = _15856;
    Interval _15783 = _15856;
    Interval _15933 = imul(_15782, _15783, intervalFailed, optical_product_upper);
    Interval _15784 = _15933;
    Interval _15934 = iadd(_15781, _15784, intervalFailed);
    Interval _15785 = _15934;
    Interval _15786 = _15859;
    Interval _15787 = _15859;
    Interval _15935 = imul(_15786, _15787, intervalFailed, optical_product_upper);
    Interval _15788 = _15935;
    Interval _15936 = iadd(_15785, _15788, intervalFailed);
    Interval _15769 = _15853;
    Interval _15770 = _15890;
    Interval _15937 = imul(_15769, _15770, intervalFailed, optical_product_upper);
    Interval _15771 = _15937;
    Interval _15772 = _15856;
    Interval _15773 = _15893;
    Interval _15938 = imul(_15772, _15773, intervalFailed, optical_product_upper);
    Interval _15774 = _15938;
    Interval _15939 = iadd(_15771, _15774, intervalFailed);
    Interval _15775 = _15939;
    Interval _15776 = _15859;
    Interval _15777 = _15896;
    Interval _15940 = imul(_15776, _15777, intervalFailed, optical_product_upper);
    Interval _15778 = _15940;
    Interval _15941 = iadd(_15775, _15778, intervalFailed);
    Interval _15759 = _15890;
    Interval _15760 = _15890;
    Interval _15942 = imul(_15759, _15760, intervalFailed, optical_product_upper);
    Interval _15761 = _15942;
    Interval _15762 = _15893;
    Interval _15763 = _15893;
    Interval _15943 = imul(_15762, _15763, intervalFailed, optical_product_upper);
    Interval _15764 = _15943;
    Interval _15944 = iadd(_15761, _15764, intervalFailed);
    Interval _15765 = _15944;
    Interval _15766 = _15896;
    Interval _15767 = _15896;
    Interval _15945 = imul(_15766, _15767, intervalFailed, optical_product_upper);
    Interval _15768 = _15945;
    Interval _15946 = iadd(_15765, _15768, intervalFailed);
    Interval _15749 = _15927;
    Interval _15750 = _15853;
    Interval _15947 = imul(_15749, _15750, intervalFailed, optical_product_upper);
    Interval _15751 = _15947;
    Interval _15752 = _15929;
    Interval _15753 = _15856;
    Interval _15948 = imul(_15752, _15753, intervalFailed, optical_product_upper);
    Interval _15754 = _15948;
    Interval _15949 = iadd(_15751, _15754, intervalFailed);
    Interval _15755 = _15949;
    Interval _15756 = _15931;
    Interval _15757 = _15859;
    Interval _15950 = imul(_15756, _15757, intervalFailed, optical_product_upper);
    Interval _15758 = _15950;
    Interval _15951 = iadd(_15755, _15758, intervalFailed);
    Interval _15739 = _15927;
    Interval _15740 = _15890;
    Interval _15952 = imul(_15739, _15740, intervalFailed, optical_product_upper);
    Interval _15741 = _15952;
    Interval _15742 = _15929;
    Interval _15743 = _15893;
    Interval _15953 = imul(_15742, _15743, intervalFailed, optical_product_upper);
    Interval _15744 = _15953;
    Interval _15954 = iadd(_15741, _15744, intervalFailed);
    Interval _15745 = _15954;
    Interval _15746 = _15931;
    Interval _15747 = _15896;
    Interval _15955 = imul(_15746, _15747, intervalFailed, optical_product_upper);
    Interval _15748 = _15955;
    Interval _15956 = iadd(_15745, _15748, intervalFailed);
    Interval param_var_a = _15936;
    Interval param_var_b = _15946;
    Interval _15957 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    bool _15964;
    if (_15941.lo <= 0.0)
    {
        _15964 = _15941.hi >= 0.0;
    }
    else
    {
        _15964 = false;
    }
    float _15971;
    if (_15964)
    {
        _15971 = 0.0;
    }
    else
    {
        _15971 = precise::min(abs(_15941.lo), abs(_15941.hi));
    }
    float _15974 = precise::max(abs(_15941.lo), abs(_15941.hi));
    float _15737 = spvFMul(_15971, _15971);
    float _15975 = interval_down(_15737, intervalFailed);
    float _15738 = spvFMul(_15974, _15974);
    float _15977 = interval_up(_15738, intervalFailed);
    Interval _15735 = _15957;
    Interval _15736 = Interval{ as_type<float>(as_type<uint>(_15977) ^ 2147483648u), as_type<float>(as_type<uint>(precise::max(0.0, _15975)) ^ 2147483648u) };
    Interval _15985 = iadd(_15735, _15736, intervalFailed);
    if (_15985.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_1 = _15946;
    Interval param_var_b_1 = _15951;
    Interval _15988 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    Interval param_var_a_2 = _15941;
    Interval param_var_b_2 = _15956;
    Interval _15989 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _15733 = _15988;
    Interval _15734 = Interval{ as_type<float>(as_type<uint>(_15989.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15989.lo) ^ 2147483648u) };
    Interval _15999 = iadd(_15733, _15734, intervalFailed);
    Interval param_var_a_3 = _15999;
    Interval param_var_b_3 = _15985;
    Interval _16000 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_a_4 = _15936;
    Interval param_var_b_4 = _15956;
    Interval _16002 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _15941;
    Interval param_var_b_5 = _15951;
    Interval _16003 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval _15731 = _16002;
    Interval _15732 = Interval{ as_type<float>(as_type<uint>(_16003.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16003.lo) ^ 2147483648u) };
    Interval _16013 = iadd(_15731, _15732, intervalFailed);
    Interval param_var_a_6 = _16013;
    Interval param_var_b_6 = _15985;
    Interval _16014 = idiv(param_var_a_6, param_var_b_6, intervalFailed, interval_divide_upper);
    float _15729 = -9.9999997473787516355514526367188e-06;
    __attribute__((unused)) float _16016 = interval_down(_15729, intervalFailed);
    float _15730 = -9.9999997473787516355514526367188e-06;
    float _16017 = interval_up(_15730, intervalFailed);
    bool _16022;
    if (_16000.lo >= _16017)
    {
        float _15727 = -9.9999997473787516355514526367188e-06;
        __attribute__((unused)) float _16019 = interval_down(_15727, intervalFailed);
        float _15728 = -9.9999997473787516355514526367188e-06;
        float _16020 = interval_up(_15728, intervalFailed);
        _16022 = _16014.lo >= _16020;
    }
    else
    {
        _16022 = false;
    }
    bool _16028;
    if (_16022)
    {
        Interval param_var_a_7 = _16000;
        Interval param_var_b_7 = _16014;
        Interval _16023 = iadd(param_var_a_7, param_var_b_7, intervalFailed);
        float _15725 = 1.000010013580322265625;
        float _16025 = interval_down(_15725, intervalFailed);
        float _15726 = 1.000010013580322265625;
        __attribute__((unused)) float _16026 = interval_up(_15726, intervalFailed);
        _16028 = _16023.hi <= _16025;
    }
    else
    {
        _16028 = false;
    }
    return _16028;
}

static __attribute__((noinline))
bool optical_jet_forward(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread OpticalJetRay& ray, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.bias0 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    ray.depth = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    bool4 _6367 = isnan(box);
    bool4 _6368 = isinf(box);
    bool _6378;
    if (all(not(bool4(_6367.x || _6368.x, _6367.y || _6368.y, _6367.z || _6368.z, _6367.w || _6368.w))))
    {
        _6378 = any(box.xy > box.zw);
    }
    else
    {
        _6378 = true;
    }
    bool _6384;
    if (!_6378)
    {
        _6384 = any(box.xy < float2(0.0));
    }
    else
    {
        _6384 = true;
    }
    bool _6392;
    if (!_6384)
    {
        _6392 = box.z >= receiver.extentClip.x;
    }
    else
    {
        _6392 = true;
    }
    bool _6400;
    if (!_6392)
    {
        _6400 = box.w >= receiver.extentClip.y;
    }
    else
    {
        _6400 = true;
    }
    bool _6405;
    if (!_6400)
    {
        _6405 = control.x > 4u;
    }
    else
    {
        _6405 = true;
    }
    bool _6414;
    if (!_6405)
    {
        _6414 = (control.y >> (control.x & 31u)) != 0u;
    }
    else
    {
        _6414 = true;
    }
    if (_6414)
    {
        return false;
    }
    Interval _6356 = Interval{ box.x, box.z };
    Interval _6357 = Interval{ as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u) };
    Interval _6438 = iadd(_6356, _6357, intervalFailed);
    Interval _6358 = Interval{ 1.0, 1.0 };
    Interval _6359 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6440 = jet_add_derivative(_6358, _6359, intervalFailed);
    Interval _6360 = Interval{ 0.0, 0.0 };
    Interval _6361 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6442 = jet_add_derivative(_6360, _6361, intervalFailed);
    Interval _6342 = _6438;
    Interval _6343 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6448 = idiv(_6342, _6343, intervalFailed, interval_divide_upper);
    bool _6455;
    if (!intervalFailed)
    {
        _6455 = intervalFailed;
    }
    else
    {
        _6455 = false;
    }
    bool _6460;
    if (_6455)
    {
        _6460 = jetFailureSite == 0u;
    }
    else
    {
        _6460 = false;
    }
    if (_6460)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6438.lo, _6438.hi, receiver.projection.x, receiver.projection.x);
    }
    Interval _6344 = _6440;
    Interval _6345 = _6448;
    Interval _6346 = Interval{ 0.0, 0.0 };
    Interval _6464 = jet_mul_derivative(_6345, _6346, intervalFailed, optical_product_upper);
    Interval _6347 = Interval{ as_type<float>(as_type<uint>(_6464.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6464.lo) ^ 2147483648u) };
    Interval _6474 = jet_add_derivative(_6344, _6347, intervalFailed);
    Interval _6348 = _6474;
    Interval _6349 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6476 = jet_div_derivative(_6348, _6349, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6350 = _6442;
    Interval _6351 = _6448;
    Interval _6352 = Interval{ 0.0, 0.0 };
    Interval _6477 = jet_mul_derivative(_6351, _6352, intervalFailed, optical_product_upper);
    Interval _6353 = Interval{ as_type<float>(as_type<uint>(_6477.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6477.lo) ^ 2147483648u) };
    Interval _6487 = jet_add_derivative(_6350, _6353, intervalFailed);
    Interval _6354 = _6487;
    Interval _6355 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6489 = jet_div_derivative(_6354, _6355, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6336 = Interval{ box.y, box.w };
    Interval _6337 = Interval{ as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u) };
    Interval _6505 = iadd(_6336, _6337, intervalFailed);
    Interval _6338 = Interval{ 0.0, 0.0 };
    Interval _6339 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6507 = jet_add_derivative(_6338, _6339, intervalFailed);
    Interval _6340 = Interval{ 1.0, 1.0 };
    Interval _6341 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6509 = jet_add_derivative(_6340, _6341, intervalFailed);
    Interval _6322 = _6505;
    Interval _6323 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6515 = idiv(_6322, _6323, intervalFailed, interval_divide_upper);
    bool _6522;
    if (!intervalFailed)
    {
        _6522 = intervalFailed;
    }
    else
    {
        _6522 = false;
    }
    bool _6527;
    if (_6522)
    {
        _6527 = jetFailureSite == 0u;
    }
    else
    {
        _6527 = false;
    }
    if (_6527)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6505.lo, _6505.hi, receiver.projection.y, receiver.projection.y);
    }
    Interval _6324 = _6507;
    Interval _6325 = _6515;
    Interval _6326 = Interval{ 0.0, 0.0 };
    Interval _6531 = jet_mul_derivative(_6325, _6326, intervalFailed, optical_product_upper);
    Interval _6327 = Interval{ as_type<float>(as_type<uint>(_6531.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6531.lo) ^ 2147483648u) };
    Interval _6541 = jet_add_derivative(_6324, _6327, intervalFailed);
    Interval _6328 = _6541;
    Interval _6329 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6543 = jet_div_derivative(_6328, _6329, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6330 = _6509;
    Interval _6331 = _6515;
    Interval _6332 = Interval{ 0.0, 0.0 };
    Interval _6544 = jet_mul_derivative(_6331, _6332, intervalFailed, optical_product_upper);
    Interval _6333 = Interval{ as_type<float>(as_type<uint>(_6544.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6544.lo) ^ 2147483648u) };
    Interval _6554 = jet_add_derivative(_6330, _6333, intervalFailed);
    Interval _6334 = _6554;
    Interval _6335 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6556 = jet_div_derivative(_6334, _6335, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    bool _6563;
    if (_6448.lo <= 0.0)
    {
        _6563 = _6448.hi >= 0.0;
    }
    else
    {
        _6563 = false;
    }
    float _6570;
    if (_6563)
    {
        _6570 = 0.0;
    }
    else
    {
        _6570 = precise::min(abs(_6448.lo), abs(_6448.hi));
    }
    float _6573 = precise::max(abs(_6448.lo), abs(_6448.hi));
    float _6312 = spvFMul(_6570, _6570);
    float _6574 = interval_down(_6312, intervalFailed);
    float _6313 = spvFMul(_6573, _6573);
    float _6576 = interval_up(_6313, intervalFailed);
    Interval _6314 = Interval{ 2.0, 2.0 };
    Interval _6315 = _6448;
    Interval _6577 = imul(_6314, _6315, intervalFailed, optical_product_upper);
    Interval _6316 = _6577;
    Interval _6317 = _6476;
    Interval _6578 = jet_mul_derivative(_6316, _6317, intervalFailed, optical_product_upper);
    Interval _6318 = Interval{ 2.0, 2.0 };
    Interval _6319 = _6448;
    Interval _6579 = imul(_6318, _6319, intervalFailed, optical_product_upper);
    Interval _6320 = _6579;
    Interval _6321 = _6489;
    Interval _6580 = jet_mul_derivative(_6320, _6321, intervalFailed, optical_product_upper);
    bool _6587;
    if (_6515.lo <= 0.0)
    {
        _6587 = _6515.hi >= 0.0;
    }
    else
    {
        _6587 = false;
    }
    float _6594;
    if (_6587)
    {
        _6594 = 0.0;
    }
    else
    {
        _6594 = precise::min(abs(_6515.lo), abs(_6515.hi));
    }
    float _6597 = precise::max(abs(_6515.lo), abs(_6515.hi));
    float _6302 = spvFMul(_6594, _6594);
    float _6598 = interval_down(_6302, intervalFailed);
    float _6303 = spvFMul(_6597, _6597);
    float _6600 = interval_up(_6303, intervalFailed);
    Interval _6304 = Interval{ 2.0, 2.0 };
    Interval _6305 = _6515;
    Interval _6601 = imul(_6304, _6305, intervalFailed, optical_product_upper);
    Interval _6306 = _6601;
    Interval _6307 = _6543;
    Interval _6602 = jet_mul_derivative(_6306, _6307, intervalFailed, optical_product_upper);
    Interval _6308 = Interval{ 2.0, 2.0 };
    Interval _6309 = _6515;
    Interval _6603 = imul(_6308, _6309, intervalFailed, optical_product_upper);
    Interval _6310 = _6603;
    Interval _6311 = _6556;
    Interval _6604 = jet_mul_derivative(_6310, _6311, intervalFailed, optical_product_upper);
    Interval _6296 = Interval{ precise::max(0.0, _6574), _6576 };
    Interval _6297 = Interval{ precise::max(0.0, _6598), _6600 };
    Interval _6607 = iadd(_6296, _6297, intervalFailed);
    Interval _6298 = _6578;
    Interval _6299 = _6602;
    Interval _6608 = jet_add_derivative(_6298, _6299, intervalFailed);
    Interval _6300 = _6580;
    Interval _6301 = _6604;
    Interval _6609 = jet_add_derivative(_6300, _6301, intervalFailed);
    bool _6614;
    if (1.0 <= 0.0)
    {
        _6614 = 1.0 >= 0.0;
    }
    else
    {
        _6614 = false;
    }
    float _6621;
    if (_6614)
    {
        _6621 = 0.0;
    }
    else
    {
        _6621 = precise::min(abs(1.0), abs(1.0));
    }
    float _6624 = precise::max(abs(1.0), abs(1.0));
    float _6286 = spvFMul(_6621, _6621);
    float _6625 = interval_down(_6286, intervalFailed);
    float _6287 = spvFMul(_6624, _6624);
    float _6627 = interval_up(_6287, intervalFailed);
    Interval _6288 = Interval{ 2.0, 2.0 };
    Interval _6289 = Interval{ 1.0, 1.0 };
    Interval _6628 = imul(_6288, _6289, intervalFailed, optical_product_upper);
    Interval _6290 = _6628;
    Interval _6291 = Interval{ 0.0, 0.0 };
    Interval _6629 = jet_mul_derivative(_6290, _6291, intervalFailed, optical_product_upper);
    Interval _6292 = Interval{ 2.0, 2.0 };
    Interval _6293 = Interval{ 1.0, 1.0 };
    Interval _6630 = imul(_6292, _6293, intervalFailed, optical_product_upper);
    Interval _6294 = _6630;
    Interval _6295 = Interval{ 0.0, 0.0 };
    Interval _6631 = jet_mul_derivative(_6294, _6295, intervalFailed, optical_product_upper);
    Interval _6280 = _6607;
    Interval _6281 = Interval{ precise::max(0.0, _6625), _6627 };
    Interval _6633 = iadd(_6280, _6281, intervalFailed);
    Interval _6282 = _6608;
    Interval _6283 = _6629;
    Interval _6634 = jet_add_derivative(_6282, _6283, intervalFailed);
    Interval _6284 = _6609;
    Interval _6285 = _6631;
    Interval _6635 = jet_add_derivative(_6284, _6285, intervalFailed);
    Interval _6271 = _6633;
    Interval _6637 = isqrt(_6271, intervalFailed);
    bool _6645;
    if (!intervalFailed)
    {
        _6645 = intervalFailed;
    }
    else
    {
        _6645 = false;
    }
    bool _6650;
    if (_6645)
    {
        _6650 = jetFailureSite == 0u;
    }
    else
    {
        _6650 = false;
    }
    if (_6650)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_6633.lo, _6633.hi, 0.0, 0.0);
    }
    if (_6637.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _6272 = Interval{ 2.0, 2.0 };
    Interval _6273 = _6637;
    Interval _6657 = imul(_6272, _6273, intervalFailed, optical_product_upper);
    Interval _6274 = Interval{ 1.0, 1.0 };
    Interval _6275 = _6657;
    Interval _6659 = idiv(_6274, _6275, intervalFailed, interval_divide_upper);
    bool _6666;
    if (!intervalFailed)
    {
        _6666 = intervalFailed;
    }
    else
    {
        _6666 = false;
    }
    bool _6671;
    if (_6666)
    {
        _6671 = jetFailureSite == 0u;
    }
    else
    {
        _6671 = false;
    }
    if (_6671)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _6657.lo, _6657.hi);
    }
    Interval _6276 = _6659;
    Interval _6277 = _6634;
    Interval _6675 = jet_mul_derivative(_6276, _6277, intervalFailed, optical_product_upper);
    Interval _6278 = _6659;
    Interval _6279 = _6635;
    Interval _6676 = jet_mul_derivative(_6278, _6279, intervalFailed, optical_product_upper);
    Interval _6257 = Interval{ 1.0, 1.0 };
    Interval _6258 = _6637;
    Interval _6678 = idiv(_6257, _6258, intervalFailed, interval_divide_upper);
    bool _6685;
    if (!intervalFailed)
    {
        _6685 = intervalFailed;
    }
    else
    {
        _6685 = false;
    }
    bool _6690;
    if (_6685)
    {
        _6690 = jetFailureSite == 0u;
    }
    else
    {
        _6690 = false;
    }
    if (_6690)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _6637.lo, _6637.hi);
    }
    Interval _6259 = Interval{ 0.0, 0.0 };
    Interval _6260 = _6678;
    Interval _6261 = _6675;
    Interval _6694 = jet_mul_derivative(_6260, _6261, intervalFailed, optical_product_upper);
    Interval _6262 = Interval{ as_type<float>(as_type<uint>(_6694.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6694.lo) ^ 2147483648u) };
    Interval _6704 = jet_add_derivative(_6259, _6262, intervalFailed);
    Interval _6263 = _6704;
    Interval _6264 = _6637;
    Interval _6705 = jet_div_derivative(_6263, _6264, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6265 = Interval{ 0.0, 0.0 };
    Interval _6266 = _6678;
    Interval _6267 = _6676;
    Interval _6706 = jet_mul_derivative(_6266, _6267, intervalFailed, optical_product_upper);
    Interval _6268 = Interval{ as_type<float>(as_type<uint>(_6706.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6706.lo) ^ 2147483648u) };
    Interval _6716 = jet_add_derivative(_6265, _6268, intervalFailed);
    Interval _6269 = _6716;
    Interval _6270 = _6637;
    Interval _6717 = jet_div_derivative(_6269, _6270, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6243 = _6448;
    Interval _6244 = _6678;
    Interval _6718 = imul(_6243, _6244, intervalFailed, optical_product_upper);
    Interval _6245 = _6476;
    Interval _6246 = _6678;
    Interval _6719 = jet_mul_derivative(_6245, _6246, intervalFailed, optical_product_upper);
    Interval _6247 = _6719;
    Interval _6248 = _6448;
    Interval _6249 = _6705;
    Interval _6720 = jet_mul_derivative(_6248, _6249, intervalFailed, optical_product_upper);
    Interval _6250 = _6720;
    Interval _6721 = jet_add_derivative(_6247, _6250, intervalFailed);
    Interval _6251 = _6489;
    Interval _6252 = _6678;
    Interval _6722 = jet_mul_derivative(_6251, _6252, intervalFailed, optical_product_upper);
    Interval _6253 = _6722;
    Interval _6254 = _6448;
    Interval _6255 = _6717;
    Interval _6723 = jet_mul_derivative(_6254, _6255, intervalFailed, optical_product_upper);
    Interval _6256 = _6723;
    Interval _6724 = jet_add_derivative(_6253, _6256, intervalFailed);
    Interval _6229 = _6515;
    Interval _6230 = _6678;
    Interval _6725 = imul(_6229, _6230, intervalFailed, optical_product_upper);
    Interval _6231 = _6543;
    Interval _6232 = _6678;
    Interval _6726 = jet_mul_derivative(_6231, _6232, intervalFailed, optical_product_upper);
    Interval _6233 = _6726;
    Interval _6234 = _6515;
    Interval _6235 = _6705;
    Interval _6727 = jet_mul_derivative(_6234, _6235, intervalFailed, optical_product_upper);
    Interval _6236 = _6727;
    Interval _6728 = jet_add_derivative(_6233, _6236, intervalFailed);
    Interval _6237 = _6556;
    Interval _6238 = _6678;
    Interval _6729 = jet_mul_derivative(_6237, _6238, intervalFailed, optical_product_upper);
    Interval _6239 = _6729;
    Interval _6240 = _6515;
    Interval _6241 = _6717;
    Interval _6730 = jet_mul_derivative(_6240, _6241, intervalFailed, optical_product_upper);
    Interval _6242 = _6730;
    Interval _6731 = jet_add_derivative(_6239, _6242, intervalFailed);
    Interval _6215 = Interval{ 1.0, 1.0 };
    Interval _6216 = _6678;
    Interval _6732 = imul(_6215, _6216, intervalFailed, optical_product_upper);
    Interval _6217 = Interval{ 0.0, 0.0 };
    Interval _6218 = _6678;
    Interval _6733 = jet_mul_derivative(_6217, _6218, intervalFailed, optical_product_upper);
    Interval _6219 = _6733;
    Interval _6220 = Interval{ 1.0, 1.0 };
    Interval _6221 = _6705;
    Interval _6734 = jet_mul_derivative(_6220, _6221, intervalFailed, optical_product_upper);
    Interval _6222 = _6734;
    Interval _6735 = jet_add_derivative(_6219, _6222, intervalFailed);
    Interval _6223 = Interval{ 0.0, 0.0 };
    Interval _6224 = _6678;
    Interval _6736 = jet_mul_derivative(_6223, _6224, intervalFailed, optical_product_upper);
    Interval _6225 = _6736;
    Interval _6226 = Interval{ 1.0, 1.0 };
    Interval _6227 = _6717;
    Interval _6737 = jet_mul_derivative(_6226, _6227, intervalFailed, optical_product_upper);
    Interval _6228 = _6737;
    Interval _6738 = jet_add_derivative(_6225, _6228, intervalFailed);
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
    float _6835;
    float _6837;
    float _6839;
    _6757 = 0.0;
    _6759 = 0.0;
    _6761 = 0.0;
    _6763 = 0.0;
    _6765 = 0.0;
    _6767 = 0.0;
    _6769 = _6718.lo;
    _6771 = _6718.hi;
    _6773 = _6721.lo;
    _6775 = _6721.hi;
    _6777 = _6724.lo;
    _6779 = _6724.hi;
    _6781 = _6725.lo;
    _6783 = _6725.hi;
    _6785 = _6728.lo;
    _6787 = _6728.hi;
    _6789 = _6731.lo;
    _6791 = _6731.hi;
    _6793 = _6732.lo;
    _6795 = _6732.hi;
    _6797 = _6735.lo;
    _6799 = _6735.hi;
    _6801 = _6738.lo;
    _6803 = _6738.hi;
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
    _6835 = 0.0;
    _6837 = 0.0;
    _6839 = 0.0;
    float _6758;
    float _6760;
    float _6762;
    float _6764;
    float _6766;
    float _6768;
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
    float _6836;
    float _6838;
    float _6840;
    OpticalJet3 hit;
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
    float _6800;
    float _6802;
    float _6804;
    for (uint _6841 = 0u; _6841 <= control.x; _6757 = _6758, _6759 = _6760, _6761 = _6762, _6763 = _6764, _6765 = _6766, _6767 = _6768, _6769 = _6770, _6771 = _6772, _6773 = _6774, _6775 = _6776, _6777 = _6778, _6779 = _6780, _6781 = _6782, _6783 = _6784, _6785 = _6786, _6787 = _6788, _6789 = _6790, _6791 = _6792, _6793 = _6794, _6795 = _6796, _6797 = _6798, _6799 = _6800, _6801 = _6802, _6803 = _6804, _6805 = _6806, _6807 = _6808, _6809 = _6810, _6811 = _6812, _6813 = _6814, _6815 = _6816, _6817 = _6818, _6819 = _6820, _6821 = _6822, _6823 = _6824, _6825 = _6826, _6827 = _6828, _6829 = _6830, _6831 = _6832, _6833 = _6834, _6835 = _6836, _6837 = _6838, _6839 = _6840, _6841++)
    {
        bool _6845 = _6841 == 0u;
        bool _6855;
        if (_6845)
        {
            _6855 = control.z != 0u;
        }
        else
        {
            _6855 = (control.y & (1u << ((_6841 - 1u) & 31u))) != 0u;
        }
        float4 _6867;
        float4 _6868;
        float4 _6869;
        if (_6845)
        {
            _6867 = receiver.a;
            _6868 = receiver.b;
            _6869 = receiver.c;
        }
        else
        {
            uint _1760 = _6841 - 1u;
            _6867 = planes[_1760].a;
            _6868 = planes[_1760].b;
            _6869 = planes[_1760].c;
        }
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
        float _6933;
        float _6934;
        float _6935;
        float _6936;
        float _6937;
        float _6938;
        if (_6855)
        {
            _6921 = liquid.planeNormal.x;
            _6922 = liquid.planeNormal.x;
            _6923 = 0.0;
            _6924 = 0.0;
            _6925 = 0.0;
            _6926 = 0.0;
            _6927 = liquid.planeNormal.y;
            _6928 = liquid.planeNormal.y;
            _6929 = 0.0;
            _6930 = 0.0;
            _6931 = 0.0;
            _6932 = 0.0;
            _6933 = liquid.planeNormal.z;
            _6934 = liquid.planeNormal.z;
            _6935 = 0.0;
            _6936 = 0.0;
            _6937 = 0.0;
            _6938 = 0.0;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ _6867, _6868, _6869 };
            OpticalJet3 _6876 = jet_plane_normal(param_var_plane, intervalFailed, optical_product_upper, interval_divide_upper);
            OpticalJet3 param_var_n = _6876;
            OpticalJet3 param_var_direction = OpticalJet3{ OpticalJet{ Interval{ _6769, _6771 }, Interval{ _6773, _6775 }, Interval{ _6777, _6779 } }, OpticalJet{ Interval{ _6781, _6783 }, Interval{ _6785, _6787 }, Interval{ _6789, _6791 } }, OpticalJet{ Interval{ _6793, _6795 }, Interval{ _6797, _6799 }, Interval{ _6801, _6803 } } };
            OpticalJet3 _6890 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
            _6921 = _6890.x.v.lo;
            _6922 = _6890.x.v.hi;
            _6923 = _6890.x.dx.lo;
            _6924 = _6890.x.dx.hi;
            _6925 = _6890.x.dy.lo;
            _6926 = _6890.x.dy.hi;
            _6927 = _6890.y.v.lo;
            _6928 = _6890.y.v.hi;
            _6929 = _6890.y.dx.lo;
            _6930 = _6890.y.dx.hi;
            _6931 = _6890.y.dy.lo;
            _6932 = _6890.y.dy.hi;
            _6933 = _6890.z.v.lo;
            _6934 = _6890.z.v.hi;
            _6935 = _6890.z.dx.lo;
            _6936 = _6890.z.dx.hi;
            _6937 = _6890.z.dy.lo;
            _6938 = _6890.z.dy.hi;
        }
        Interval _6201 = Interval{ _6769, _6771 };
        Interval _6202 = Interval{ _6921, _6922 };
        Interval _6941 = imul(_6201, _6202, intervalFailed, optical_product_upper);
        Interval _6203 = Interval{ _6773, _6775 };
        Interval _6204 = Interval{ _6921, _6922 };
        Interval _6944 = jet_mul_derivative(_6203, _6204, intervalFailed, optical_product_upper);
        Interval _6205 = _6944;
        Interval _6206 = Interval{ _6769, _6771 };
        Interval _6207 = Interval{ _6923, _6924 };
        Interval _6947 = jet_mul_derivative(_6206, _6207, intervalFailed, optical_product_upper);
        Interval _6208 = _6947;
        Interval _6948 = jet_add_derivative(_6205, _6208, intervalFailed);
        Interval _6209 = Interval{ _6777, _6779 };
        Interval _6210 = Interval{ _6921, _6922 };
        Interval _6951 = jet_mul_derivative(_6209, _6210, intervalFailed, optical_product_upper);
        Interval _6211 = _6951;
        Interval _6212 = Interval{ _6769, _6771 };
        Interval _6213 = Interval{ _6925, _6926 };
        Interval _6954 = jet_mul_derivative(_6212, _6213, intervalFailed, optical_product_upper);
        Interval _6214 = _6954;
        Interval _6955 = jet_add_derivative(_6211, _6214, intervalFailed);
        Interval _6187 = Interval{ _6781, _6783 };
        Interval _6188 = Interval{ _6927, _6928 };
        Interval _6958 = imul(_6187, _6188, intervalFailed, optical_product_upper);
        Interval _6189 = Interval{ _6785, _6787 };
        Interval _6190 = Interval{ _6927, _6928 };
        Interval _6961 = jet_mul_derivative(_6189, _6190, intervalFailed, optical_product_upper);
        Interval _6191 = _6961;
        Interval _6192 = Interval{ _6781, _6783 };
        Interval _6193 = Interval{ _6929, _6930 };
        Interval _6964 = jet_mul_derivative(_6192, _6193, intervalFailed, optical_product_upper);
        Interval _6194 = _6964;
        Interval _6965 = jet_add_derivative(_6191, _6194, intervalFailed);
        Interval _6195 = Interval{ _6789, _6791 };
        Interval _6196 = Interval{ _6927, _6928 };
        Interval _6968 = jet_mul_derivative(_6195, _6196, intervalFailed, optical_product_upper);
        Interval _6197 = _6968;
        Interval _6198 = Interval{ _6781, _6783 };
        Interval _6199 = Interval{ _6931, _6932 };
        Interval _6971 = jet_mul_derivative(_6198, _6199, intervalFailed, optical_product_upper);
        Interval _6200 = _6971;
        Interval _6972 = jet_add_derivative(_6197, _6200, intervalFailed);
        Interval _6181 = _6941;
        Interval _6182 = _6958;
        Interval _6973 = iadd(_6181, _6182, intervalFailed);
        Interval _6183 = _6948;
        Interval _6184 = _6965;
        Interval _6974 = jet_add_derivative(_6183, _6184, intervalFailed);
        Interval _6185 = _6955;
        Interval _6186 = _6972;
        Interval _6975 = jet_add_derivative(_6185, _6186, intervalFailed);
        Interval _6167 = Interval{ _6793, _6795 };
        Interval _6168 = Interval{ _6933, _6934 };
        Interval _6978 = imul(_6167, _6168, intervalFailed, optical_product_upper);
        Interval _6169 = Interval{ _6797, _6799 };
        Interval _6170 = Interval{ _6933, _6934 };
        Interval _6981 = jet_mul_derivative(_6169, _6170, intervalFailed, optical_product_upper);
        Interval _6171 = _6981;
        Interval _6172 = Interval{ _6793, _6795 };
        Interval _6173 = Interval{ _6935, _6936 };
        Interval _6984 = jet_mul_derivative(_6172, _6173, intervalFailed, optical_product_upper);
        Interval _6174 = _6984;
        Interval _6985 = jet_add_derivative(_6171, _6174, intervalFailed);
        Interval _6175 = Interval{ _6801, _6803 };
        Interval _6176 = Interval{ _6933, _6934 };
        Interval _6988 = jet_mul_derivative(_6175, _6176, intervalFailed, optical_product_upper);
        Interval _6177 = _6988;
        Interval _6178 = Interval{ _6793, _6795 };
        Interval _6179 = Interval{ _6937, _6938 };
        Interval _6991 = jet_mul_derivative(_6178, _6179, intervalFailed, optical_product_upper);
        Interval _6180 = _6991;
        Interval _6992 = jet_add_derivative(_6177, _6180, intervalFailed);
        Interval _6161 = _6973;
        Interval _6162 = _6978;
        Interval _6993 = iadd(_6161, _6162, intervalFailed);
        Interval _6163 = _6974;
        Interval _6164 = _6985;
        Interval _6994 = jet_add_derivative(_6163, _6164, intervalFailed);
        Interval _6165 = _6975;
        Interval _6166 = _6992;
        Interval _6995 = jet_add_derivative(_6165, _6166, intervalFailed);
        bool _7002;
        if (_6993.lo <= 0.0)
        {
            _7002 = _6993.hi >= 0.0;
        }
        else
        {
            _7002 = false;
        }
        float _7009;
        if (_7002)
        {
            _7009 = 0.0;
        }
        else
        {
            _7009 = precise::min(abs(_6993.lo), abs(_6993.hi));
        }
        bool _7011;
        if (!_6845)
        {
            _7011 = _6855;
        }
        else
        {
            _7011 = false;
        }
        float _7012;
        if (_7011)
        {
            _7012 = 9.9999999392252902907785028219223e-09;
        }
        else
        {
            _7012 = 9.9999999600419720025001879548654e-13;
        }
        float _6159 = _7012;
        __attribute__((unused)) float _7013 = interval_down(_6159, intervalFailed);
        float _6160 = _7012;
        float _7014 = interval_up(_6160, intervalFailed);
        if (_7009 <= _7014)
        {
            return false;
        }
        float _7609;
        float _7610;
        float _7611;
        float _7612;
        float _7613;
        float _7614;
        if (_6845)
        {
            float3 _7020;
            if (_6855)
            {
                _7020 = liquid.planePoint.xyz;
            }
            else
            {
                _7020 = _6867.xyz;
            }
            Interval _6145 = Interval{ _7020.x, _7020.x };
            Interval _6146 = Interval{ _6921, _6922 };
            Interval _7026 = imul(_6145, _6146, intervalFailed, optical_product_upper);
            Interval _6147 = Interval{ 0.0, 0.0 };
            Interval _6148 = Interval{ _6921, _6922 };
            Interval _7028 = jet_mul_derivative(_6147, _6148, intervalFailed, optical_product_upper);
            Interval _6149 = _7028;
            Interval _6150 = Interval{ _7020.x, _7020.x };
            Interval _6151 = Interval{ _6923, _6924 };
            Interval _7031 = jet_mul_derivative(_6150, _6151, intervalFailed, optical_product_upper);
            Interval _6152 = _7031;
            Interval _7032 = jet_add_derivative(_6149, _6152, intervalFailed);
            Interval _6153 = Interval{ 0.0, 0.0 };
            Interval _6154 = Interval{ _6921, _6922 };
            Interval _7034 = jet_mul_derivative(_6153, _6154, intervalFailed, optical_product_upper);
            Interval _6155 = _7034;
            Interval _6156 = Interval{ _7020.x, _7020.x };
            Interval _6157 = Interval{ _6925, _6926 };
            Interval _7037 = jet_mul_derivative(_6156, _6157, intervalFailed, optical_product_upper);
            Interval _6158 = _7037;
            Interval _7038 = jet_add_derivative(_6155, _6158, intervalFailed);
            Interval _6131 = Interval{ _7020.y, _7020.y };
            Interval _6132 = Interval{ _6927, _6928 };
            Interval _7041 = imul(_6131, _6132, intervalFailed, optical_product_upper);
            Interval _6133 = Interval{ 0.0, 0.0 };
            Interval _6134 = Interval{ _6927, _6928 };
            Interval _7043 = jet_mul_derivative(_6133, _6134, intervalFailed, optical_product_upper);
            Interval _6135 = _7043;
            Interval _6136 = Interval{ _7020.y, _7020.y };
            Interval _6137 = Interval{ _6929, _6930 };
            Interval _7046 = jet_mul_derivative(_6136, _6137, intervalFailed, optical_product_upper);
            Interval _6138 = _7046;
            Interval _7047 = jet_add_derivative(_6135, _6138, intervalFailed);
            Interval _6139 = Interval{ 0.0, 0.0 };
            Interval _6140 = Interval{ _6927, _6928 };
            Interval _7049 = jet_mul_derivative(_6139, _6140, intervalFailed, optical_product_upper);
            Interval _6141 = _7049;
            Interval _6142 = Interval{ _7020.y, _7020.y };
            Interval _6143 = Interval{ _6931, _6932 };
            Interval _7052 = jet_mul_derivative(_6142, _6143, intervalFailed, optical_product_upper);
            Interval _6144 = _7052;
            Interval _7053 = jet_add_derivative(_6141, _6144, intervalFailed);
            Interval _6125 = _7026;
            Interval _6126 = _7041;
            Interval _7054 = iadd(_6125, _6126, intervalFailed);
            Interval _6127 = _7032;
            Interval _6128 = _7047;
            Interval _7055 = jet_add_derivative(_6127, _6128, intervalFailed);
            Interval _6129 = _7038;
            Interval _6130 = _7053;
            Interval _7056 = jet_add_derivative(_6129, _6130, intervalFailed);
            Interval _6111 = Interval{ _7020.z, _7020.z };
            Interval _6112 = Interval{ _6933, _6934 };
            Interval _7059 = imul(_6111, _6112, intervalFailed, optical_product_upper);
            Interval _6113 = Interval{ 0.0, 0.0 };
            Interval _6114 = Interval{ _6933, _6934 };
            Interval _7061 = jet_mul_derivative(_6113, _6114, intervalFailed, optical_product_upper);
            Interval _6115 = _7061;
            Interval _6116 = Interval{ _7020.z, _7020.z };
            Interval _6117 = Interval{ _6935, _6936 };
            Interval _7064 = jet_mul_derivative(_6116, _6117, intervalFailed, optical_product_upper);
            Interval _6118 = _7064;
            Interval _7065 = jet_add_derivative(_6115, _6118, intervalFailed);
            Interval _6119 = Interval{ 0.0, 0.0 };
            Interval _6120 = Interval{ _6933, _6934 };
            Interval _7067 = jet_mul_derivative(_6119, _6120, intervalFailed, optical_product_upper);
            Interval _6121 = _7067;
            Interval _6122 = Interval{ _7020.z, _7020.z };
            Interval _6123 = Interval{ _6937, _6938 };
            Interval _7070 = jet_mul_derivative(_6122, _6123, intervalFailed, optical_product_upper);
            Interval _6124 = _7070;
            Interval _7071 = jet_add_derivative(_6121, _6124, intervalFailed);
            Interval _6105 = _7054;
            Interval _6106 = _7059;
            Interval _7072 = iadd(_6105, _6106, intervalFailed);
            Interval _6107 = _7055;
            Interval _6108 = _7065;
            Interval _7073 = jet_add_derivative(_6107, _6108, intervalFailed);
            Interval _6109 = _7056;
            Interval _6110 = _7071;
            Interval _7074 = jet_add_derivative(_6109, _6110, intervalFailed);
            Interval _6091 = _6448;
            Interval _6092 = Interval{ _6921, _6922 };
            Interval _7076 = imul(_6091, _6092, intervalFailed, optical_product_upper);
            Interval _6093 = _6476;
            Interval _6094 = Interval{ _6921, _6922 };
            Interval _7078 = jet_mul_derivative(_6093, _6094, intervalFailed, optical_product_upper);
            Interval _6095 = _7078;
            Interval _6096 = _6448;
            Interval _6097 = Interval{ _6923, _6924 };
            Interval _7080 = jet_mul_derivative(_6096, _6097, intervalFailed, optical_product_upper);
            Interval _6098 = _7080;
            Interval _7081 = jet_add_derivative(_6095, _6098, intervalFailed);
            Interval _6099 = _6489;
            Interval _6100 = Interval{ _6921, _6922 };
            Interval _7083 = jet_mul_derivative(_6099, _6100, intervalFailed, optical_product_upper);
            Interval _6101 = _7083;
            Interval _6102 = _6448;
            Interval _6103 = Interval{ _6925, _6926 };
            Interval _7085 = jet_mul_derivative(_6102, _6103, intervalFailed, optical_product_upper);
            Interval _6104 = _7085;
            Interval _7086 = jet_add_derivative(_6101, _6104, intervalFailed);
            Interval _6077 = _6515;
            Interval _6078 = Interval{ _6927, _6928 };
            Interval _7088 = imul(_6077, _6078, intervalFailed, optical_product_upper);
            Interval _6079 = _6543;
            Interval _6080 = Interval{ _6927, _6928 };
            Interval _7090 = jet_mul_derivative(_6079, _6080, intervalFailed, optical_product_upper);
            Interval _6081 = _7090;
            Interval _6082 = _6515;
            Interval _6083 = Interval{ _6929, _6930 };
            Interval _7092 = jet_mul_derivative(_6082, _6083, intervalFailed, optical_product_upper);
            Interval _6084 = _7092;
            Interval _7093 = jet_add_derivative(_6081, _6084, intervalFailed);
            Interval _6085 = _6556;
            Interval _6086 = Interval{ _6927, _6928 };
            Interval _7095 = jet_mul_derivative(_6085, _6086, intervalFailed, optical_product_upper);
            Interval _6087 = _7095;
            Interval _6088 = _6515;
            Interval _6089 = Interval{ _6931, _6932 };
            Interval _7097 = jet_mul_derivative(_6088, _6089, intervalFailed, optical_product_upper);
            Interval _6090 = _7097;
            Interval _7098 = jet_add_derivative(_6087, _6090, intervalFailed);
            Interval _6071 = _7076;
            Interval _6072 = _7088;
            Interval _7099 = iadd(_6071, _6072, intervalFailed);
            Interval _6073 = _7081;
            Interval _6074 = _7093;
            Interval _7100 = jet_add_derivative(_6073, _6074, intervalFailed);
            Interval _6075 = _7086;
            Interval _6076 = _7098;
            Interval _7101 = jet_add_derivative(_6075, _6076, intervalFailed);
            Interval _6057 = Interval{ 1.0, 1.0 };
            Interval _6058 = Interval{ _6933, _6934 };
            Interval _7103 = imul(_6057, _6058, intervalFailed, optical_product_upper);
            Interval _6059 = Interval{ 0.0, 0.0 };
            Interval _6060 = Interval{ _6933, _6934 };
            Interval _7105 = jet_mul_derivative(_6059, _6060, intervalFailed, optical_product_upper);
            Interval _6061 = _7105;
            Interval _6062 = Interval{ 1.0, 1.0 };
            Interval _6063 = Interval{ _6935, _6936 };
            Interval _7107 = jet_mul_derivative(_6062, _6063, intervalFailed, optical_product_upper);
            Interval _6064 = _7107;
            Interval _7108 = jet_add_derivative(_6061, _6064, intervalFailed);
            Interval _6065 = Interval{ 0.0, 0.0 };
            Interval _6066 = Interval{ _6933, _6934 };
            Interval _7110 = jet_mul_derivative(_6065, _6066, intervalFailed, optical_product_upper);
            Interval _6067 = _7110;
            Interval _6068 = Interval{ 1.0, 1.0 };
            Interval _6069 = Interval{ _6937, _6938 };
            Interval _7112 = jet_mul_derivative(_6068, _6069, intervalFailed, optical_product_upper);
            Interval _6070 = _7112;
            Interval _7113 = jet_add_derivative(_6067, _6070, intervalFailed);
            Interval _6051 = _7099;
            Interval _6052 = _7103;
            Interval _7114 = iadd(_6051, _6052, intervalFailed);
            Interval _6053 = _7100;
            Interval _6054 = _7108;
            Interval _7115 = jet_add_derivative(_6053, _6054, intervalFailed);
            Interval _6055 = _7101;
            Interval _6056 = _7113;
            Interval _7116 = jet_add_derivative(_6055, _6056, intervalFailed);
            Interval _6037 = _7072;
            Interval _6038 = _7114;
            Interval _7118 = idiv(_6037, _6038, intervalFailed, interval_divide_upper);
            bool _7127;
            if (!intervalFailed)
            {
                _7127 = intervalFailed;
            }
            else
            {
                _7127 = false;
            }
            bool _7132;
            if (_7127)
            {
                _7132 = jetFailureSite == 0u;
            }
            else
            {
                _7132 = false;
            }
            if (_7132)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7072.lo, _7072.hi, _7114.lo, _7114.hi);
            }
            Interval _6039 = _7073;
            Interval _6040 = _7118;
            Interval _6041 = _7115;
            Interval _7136 = jet_mul_derivative(_6040, _6041, intervalFailed, optical_product_upper);
            Interval _6042 = Interval{ as_type<float>(as_type<uint>(_7136.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7136.lo) ^ 2147483648u) };
            Interval _7146 = jet_add_derivative(_6039, _6042, intervalFailed);
            Interval _6043 = _7146;
            Interval _6044 = _7114;
            Interval _7147 = jet_div_derivative(_6043, _6044, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _6045 = _7074;
            Interval _6046 = _7118;
            Interval _6047 = _7116;
            Interval _7148 = jet_mul_derivative(_6046, _6047, intervalFailed, optical_product_upper);
            Interval _6048 = Interval{ as_type<float>(as_type<uint>(_7148.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7148.lo) ^ 2147483648u) };
            Interval _7158 = jet_add_derivative(_6045, _6048, intervalFailed);
            Interval _6049 = _7158;
            Interval _6050 = _7114;
            Interval _7159 = jet_div_derivative(_6049, _6050, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ray.depth = OpticalJet{ _7118, _7147, _7159 };
            Interval _6023 = ray.depth.v;
            Interval _6024 = _6637;
            Interval _7167 = imul(_6023, _6024, intervalFailed, optical_product_upper);
            Interval _6025 = ray.depth.dx;
            Interval _6026 = _6637;
            Interval _7168 = jet_mul_derivative(_6025, _6026, intervalFailed, optical_product_upper);
            Interval _6027 = _7168;
            Interval _6028 = ray.depth.v;
            Interval _6029 = _6675;
            Interval _7169 = jet_mul_derivative(_6028, _6029, intervalFailed, optical_product_upper);
            Interval _6030 = _7169;
            Interval _7170 = jet_add_derivative(_6027, _6030, intervalFailed);
            Interval _6031 = ray.depth.dy;
            Interval _6032 = _6637;
            Interval _7171 = jet_mul_derivative(_6031, _6032, intervalFailed, optical_product_upper);
            Interval _6033 = _7171;
            Interval _6034 = ray.depth.v;
            Interval _6035 = _6676;
            Interval _7172 = jet_mul_derivative(_6034, _6035, intervalFailed, optical_product_upper);
            Interval _6036 = _7172;
            Interval _7173 = jet_add_derivative(_6033, _6036, intervalFailed);
            Interval _6009 = _6448;
            Interval _6010 = ray.depth.v;
            Interval _7185 = imul(_6009, _6010, intervalFailed, optical_product_upper);
            Interval _6011 = _6476;
            Interval _6012 = ray.depth.v;
            Interval _7186 = jet_mul_derivative(_6011, _6012, intervalFailed, optical_product_upper);
            Interval _6013 = _7186;
            Interval _6014 = _6448;
            Interval _6015 = ray.depth.dx;
            Interval _7187 = jet_mul_derivative(_6014, _6015, intervalFailed, optical_product_upper);
            Interval _6016 = _7187;
            Interval _7188 = jet_add_derivative(_6013, _6016, intervalFailed);
            Interval _6017 = _6489;
            Interval _6018 = ray.depth.v;
            Interval _7189 = jet_mul_derivative(_6017, _6018, intervalFailed, optical_product_upper);
            Interval _6019 = _7189;
            Interval _6020 = _6448;
            Interval _6021 = ray.depth.dy;
            Interval _7190 = jet_mul_derivative(_6020, _6021, intervalFailed, optical_product_upper);
            Interval _6022 = _7190;
            Interval _7191 = jet_add_derivative(_6019, _6022, intervalFailed);
            Interval _5995 = _6515;
            Interval _5996 = ray.depth.v;
            Interval _7195 = imul(_5995, _5996, intervalFailed, optical_product_upper);
            Interval _5997 = _6543;
            Interval _5998 = ray.depth.v;
            Interval _7196 = jet_mul_derivative(_5997, _5998, intervalFailed, optical_product_upper);
            Interval _5999 = _7196;
            Interval _6000 = _6515;
            Interval _6001 = ray.depth.dx;
            Interval _7197 = jet_mul_derivative(_6000, _6001, intervalFailed, optical_product_upper);
            Interval _6002 = _7197;
            Interval _7198 = jet_add_derivative(_5999, _6002, intervalFailed);
            Interval _6003 = _6556;
            Interval _6004 = ray.depth.v;
            Interval _7199 = jet_mul_derivative(_6003, _6004, intervalFailed, optical_product_upper);
            Interval _6005 = _7199;
            Interval _6006 = _6515;
            Interval _6007 = ray.depth.dy;
            Interval _7200 = jet_mul_derivative(_6006, _6007, intervalFailed, optical_product_upper);
            Interval _6008 = _7200;
            Interval _7201 = jet_add_derivative(_6005, _6008, intervalFailed);
            Interval _5981 = Interval{ 1.0, 1.0 };
            Interval _5982 = ray.depth.v;
            Interval _7205 = imul(_5981, _5982, intervalFailed, optical_product_upper);
            Interval _5983 = Interval{ 0.0, 0.0 };
            Interval _5984 = ray.depth.v;
            Interval _7206 = jet_mul_derivative(_5983, _5984, intervalFailed, optical_product_upper);
            Interval _5985 = _7206;
            Interval _5986 = Interval{ 1.0, 1.0 };
            Interval _5987 = ray.depth.dx;
            Interval _7207 = jet_mul_derivative(_5986, _5987, intervalFailed, optical_product_upper);
            Interval _5988 = _7207;
            Interval _7208 = jet_add_derivative(_5985, _5988, intervalFailed);
            Interval _5989 = Interval{ 0.0, 0.0 };
            Interval _5990 = ray.depth.v;
            Interval _7209 = jet_mul_derivative(_5989, _5990, intervalFailed, optical_product_upper);
            Interval _5991 = _7209;
            Interval _5992 = Interval{ 1.0, 1.0 };
            Interval _5993 = ray.depth.dy;
            Interval _7210 = jet_mul_derivative(_5992, _5993, intervalFailed, optical_product_upper);
            Interval _5994 = _7210;
            Interval _7211 = jet_add_derivative(_5991, _5994, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7185, _7188, _7191 }, OpticalJet{ _7195, _7198, _7201 }, OpticalJet{ _7205, _7208, _7211 } };
            float3 _7220;
            if (_6855)
            {
                _7220 = liquid.planePoint.xyz;
            }
            else
            {
                _7220 = _6867.xyz;
            }
            Interval _5967 = Interval{ _7220.x, _7220.x };
            Interval _5968 = Interval{ _6921, _6922 };
            Interval _7226 = imul(_5967, _5968, intervalFailed, optical_product_upper);
            Interval _5969 = Interval{ 0.0, 0.0 };
            Interval _5970 = Interval{ _6921, _6922 };
            Interval _7228 = jet_mul_derivative(_5969, _5970, intervalFailed, optical_product_upper);
            Interval _5971 = _7228;
            Interval _5972 = Interval{ _7220.x, _7220.x };
            Interval _5973 = Interval{ _6923, _6924 };
            Interval _7231 = jet_mul_derivative(_5972, _5973, intervalFailed, optical_product_upper);
            Interval _5974 = _7231;
            Interval _7232 = jet_add_derivative(_5971, _5974, intervalFailed);
            Interval _5975 = Interval{ 0.0, 0.0 };
            Interval _5976 = Interval{ _6921, _6922 };
            Interval _7234 = jet_mul_derivative(_5975, _5976, intervalFailed, optical_product_upper);
            Interval _5977 = _7234;
            Interval _5978 = Interval{ _7220.x, _7220.x };
            Interval _5979 = Interval{ _6925, _6926 };
            Interval _7237 = jet_mul_derivative(_5978, _5979, intervalFailed, optical_product_upper);
            Interval _5980 = _7237;
            Interval _7238 = jet_add_derivative(_5977, _5980, intervalFailed);
            Interval _5953 = Interval{ _7220.y, _7220.y };
            Interval _5954 = Interval{ _6927, _6928 };
            Interval _7241 = imul(_5953, _5954, intervalFailed, optical_product_upper);
            Interval _5955 = Interval{ 0.0, 0.0 };
            Interval _5956 = Interval{ _6927, _6928 };
            Interval _7243 = jet_mul_derivative(_5955, _5956, intervalFailed, optical_product_upper);
            Interval _5957 = _7243;
            Interval _5958 = Interval{ _7220.y, _7220.y };
            Interval _5959 = Interval{ _6929, _6930 };
            Interval _7246 = jet_mul_derivative(_5958, _5959, intervalFailed, optical_product_upper);
            Interval _5960 = _7246;
            Interval _7247 = jet_add_derivative(_5957, _5960, intervalFailed);
            Interval _5961 = Interval{ 0.0, 0.0 };
            Interval _5962 = Interval{ _6927, _6928 };
            Interval _7249 = jet_mul_derivative(_5961, _5962, intervalFailed, optical_product_upper);
            Interval _5963 = _7249;
            Interval _5964 = Interval{ _7220.y, _7220.y };
            Interval _5965 = Interval{ _6931, _6932 };
            Interval _7252 = jet_mul_derivative(_5964, _5965, intervalFailed, optical_product_upper);
            Interval _5966 = _7252;
            Interval _7253 = jet_add_derivative(_5963, _5966, intervalFailed);
            Interval _5947 = _7226;
            Interval _5948 = _7241;
            Interval _7254 = iadd(_5947, _5948, intervalFailed);
            Interval _5949 = _7232;
            Interval _5950 = _7247;
            Interval _7255 = jet_add_derivative(_5949, _5950, intervalFailed);
            Interval _5951 = _7238;
            Interval _5952 = _7253;
            Interval _7256 = jet_add_derivative(_5951, _5952, intervalFailed);
            Interval _5933 = Interval{ _7220.z, _7220.z };
            Interval _5934 = Interval{ _6933, _6934 };
            Interval _7259 = imul(_5933, _5934, intervalFailed, optical_product_upper);
            Interval _5935 = Interval{ 0.0, 0.0 };
            Interval _5936 = Interval{ _6933, _6934 };
            Interval _7261 = jet_mul_derivative(_5935, _5936, intervalFailed, optical_product_upper);
            Interval _5937 = _7261;
            Interval _5938 = Interval{ _7220.z, _7220.z };
            Interval _5939 = Interval{ _6935, _6936 };
            Interval _7264 = jet_mul_derivative(_5938, _5939, intervalFailed, optical_product_upper);
            Interval _5940 = _7264;
            Interval _7265 = jet_add_derivative(_5937, _5940, intervalFailed);
            Interval _5941 = Interval{ 0.0, 0.0 };
            Interval _5942 = Interval{ _6933, _6934 };
            Interval _7267 = jet_mul_derivative(_5941, _5942, intervalFailed, optical_product_upper);
            Interval _5943 = _7267;
            Interval _5944 = Interval{ _7220.z, _7220.z };
            Interval _5945 = Interval{ _6937, _6938 };
            Interval _7270 = jet_mul_derivative(_5944, _5945, intervalFailed, optical_product_upper);
            Interval _5946 = _7270;
            Interval _7271 = jet_add_derivative(_5943, _5946, intervalFailed);
            Interval _5927 = _7254;
            Interval _5928 = _7259;
            Interval _7272 = iadd(_5927, _5928, intervalFailed);
            Interval _5929 = _7255;
            Interval _5930 = _7265;
            Interval _7273 = jet_add_derivative(_5929, _5930, intervalFailed);
            Interval _5931 = _7256;
            Interval _5932 = _7271;
            Interval _7274 = jet_add_derivative(_5931, _5932, intervalFailed);
            Interval _5913 = _7272;
            Interval _5914 = _6993;
            Interval _7276 = idiv(_5913, _5914, intervalFailed, interval_divide_upper);
            bool _7285;
            if (!intervalFailed)
            {
                _7285 = intervalFailed;
            }
            else
            {
                _7285 = false;
            }
            bool _7290;
            if (_7285)
            {
                _7290 = jetFailureSite == 0u;
            }
            else
            {
                _7290 = false;
            }
            if (_7290)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7272.lo, _7272.hi, _6993.lo, _6993.hi);
            }
            Interval _5915 = _7273;
            Interval _5916 = _7276;
            Interval _5917 = _6994;
            Interval _7294 = jet_mul_derivative(_5916, _5917, intervalFailed, optical_product_upper);
            Interval _5918 = Interval{ as_type<float>(as_type<uint>(_7294.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7294.lo) ^ 2147483648u) };
            Interval _7304 = jet_add_derivative(_5915, _5918, intervalFailed);
            Interval _5919 = _7304;
            Interval _5920 = _6993;
            Interval _7305 = jet_div_derivative(_5919, _5920, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5921 = _7274;
            Interval _5922 = _7276;
            Interval _5923 = _6995;
            Interval _7306 = jet_mul_derivative(_5922, _5923, intervalFailed, optical_product_upper);
            Interval _5924 = Interval{ as_type<float>(as_type<uint>(_7306.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7306.lo) ^ 2147483648u) };
            Interval _7316 = jet_add_derivative(_5921, _5924, intervalFailed);
            Interval _5925 = _7316;
            Interval _5926 = _6993;
            Interval _7317 = jet_div_derivative(_5925, _5926, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5899 = _7276;
            Interval _5900 = Interval{ _6793, _6795 };
            Interval _7324 = imul(_5899, _5900, intervalFailed, optical_product_upper);
            Interval _5901 = _7305;
            Interval _5902 = Interval{ _6793, _6795 };
            Interval _7326 = jet_mul_derivative(_5901, _5902, intervalFailed, optical_product_upper);
            Interval _5903 = _7326;
            Interval _5904 = _7276;
            Interval _5905 = Interval{ _6797, _6799 };
            Interval _7328 = jet_mul_derivative(_5904, _5905, intervalFailed, optical_product_upper);
            Interval _5906 = _7328;
            Interval _7329 = jet_add_derivative(_5903, _5906, intervalFailed);
            Interval _5907 = _7317;
            Interval _5908 = Interval{ _6793, _6795 };
            Interval _7331 = jet_mul_derivative(_5907, _5908, intervalFailed, optical_product_upper);
            Interval _5909 = _7331;
            Interval _5910 = _7276;
            Interval _5911 = Interval{ _6801, _6803 };
            Interval _7333 = jet_mul_derivative(_5910, _5911, intervalFailed, optical_product_upper);
            Interval _5912 = _7333;
            Interval _7334 = jet_add_derivative(_5909, _5912, intervalFailed);
            ray.depth = OpticalJet{ Interval{ precise::min(ray.depth.v.lo, _7324.lo), precise::max(ray.depth.v.hi, _7324.hi) }, Interval{ precise::min(ray.depth.dx.lo, _7329.lo), precise::max(ray.depth.dx.hi, _7329.hi) }, Interval{ precise::min(ray.depth.dy.lo, _7334.lo), precise::max(ray.depth.dy.hi, _7334.hi) } };
            bool _7365;
            if ((isunordered(_7167.lo, 0.0) || _7167.lo > 0.0))
            {
                _7365 = ray.depth.v.lo < receiver.extentClip.z;
            }
            else
            {
                _7365 = true;
            }
            bool _7373;
            if (!_7365)
            {
                _7373 = ray.depth.v.hi > receiver.extentClip.w;
            }
            else
            {
                _7373 = true;
            }
            if (_7373)
            {
                return false;
            }
            _7609 = _7167.lo;
            _7610 = _7167.hi;
            _7611 = _7170.lo;
            _7612 = _7170.hi;
            _7613 = _7173.lo;
            _7614 = _7173.hi;
        }
        else
        {
            float3 _7378;
            if (_6855)
            {
                _7378 = liquid.planePoint.xyz;
            }
            else
            {
                _7378 = _6867.xyz;
            }
            Interval _5893 = Interval{ _7378.x, _7378.x };
            Interval _5894 = Interval{ as_type<float>(as_type<uint>(_6807) ^ 2147483648u), as_type<float>(as_type<uint>(_6805) ^ 2147483648u) };
            Interval _7438 = iadd(_5893, _5894, intervalFailed);
            Interval _5895 = Interval{ 0.0, 0.0 };
            Interval _5896 = Interval{ as_type<float>(as_type<uint>(_6811) ^ 2147483648u), as_type<float>(as_type<uint>(_6809) ^ 2147483648u) };
            Interval _7440 = jet_add_derivative(_5895, _5896, intervalFailed);
            Interval _5897 = Interval{ 0.0, 0.0 };
            Interval _5898 = Interval{ as_type<float>(as_type<uint>(_6815) ^ 2147483648u), as_type<float>(as_type<uint>(_6813) ^ 2147483648u) };
            Interval _7442 = jet_add_derivative(_5897, _5898, intervalFailed);
            Interval _5887 = Interval{ _7378.y, _7378.y };
            Interval _5888 = Interval{ as_type<float>(as_type<uint>(_6819) ^ 2147483648u), as_type<float>(as_type<uint>(_6817) ^ 2147483648u) };
            Interval _7445 = iadd(_5887, _5888, intervalFailed);
            Interval _5889 = Interval{ 0.0, 0.0 };
            Interval _5890 = Interval{ as_type<float>(as_type<uint>(_6823) ^ 2147483648u), as_type<float>(as_type<uint>(_6821) ^ 2147483648u) };
            Interval _7447 = jet_add_derivative(_5889, _5890, intervalFailed);
            Interval _5891 = Interval{ 0.0, 0.0 };
            Interval _5892 = Interval{ as_type<float>(as_type<uint>(_6827) ^ 2147483648u), as_type<float>(as_type<uint>(_6825) ^ 2147483648u) };
            Interval _7449 = jet_add_derivative(_5891, _5892, intervalFailed);
            Interval _5881 = Interval{ _7378.z, _7378.z };
            Interval _5882 = Interval{ as_type<float>(as_type<uint>(_6831) ^ 2147483648u), as_type<float>(as_type<uint>(_6829) ^ 2147483648u) };
            Interval _7452 = iadd(_5881, _5882, intervalFailed);
            Interval _5883 = Interval{ 0.0, 0.0 };
            Interval _5884 = Interval{ as_type<float>(as_type<uint>(_6835) ^ 2147483648u), as_type<float>(as_type<uint>(_6833) ^ 2147483648u) };
            Interval _7454 = jet_add_derivative(_5883, _5884, intervalFailed);
            Interval _5885 = Interval{ 0.0, 0.0 };
            Interval _5886 = Interval{ as_type<float>(as_type<uint>(_6839) ^ 2147483648u), as_type<float>(as_type<uint>(_6837) ^ 2147483648u) };
            Interval _7456 = jet_add_derivative(_5885, _5886, intervalFailed);
            Interval _5867 = _7438;
            Interval _5868 = Interval{ _6921, _6922 };
            Interval _7458 = imul(_5867, _5868, intervalFailed, optical_product_upper);
            Interval _5869 = _7440;
            Interval _5870 = Interval{ _6921, _6922 };
            Interval _7460 = jet_mul_derivative(_5869, _5870, intervalFailed, optical_product_upper);
            Interval _5871 = _7460;
            Interval _5872 = _7438;
            Interval _5873 = Interval{ _6923, _6924 };
            Interval _7462 = jet_mul_derivative(_5872, _5873, intervalFailed, optical_product_upper);
            Interval _5874 = _7462;
            Interval _7463 = jet_add_derivative(_5871, _5874, intervalFailed);
            Interval _5875 = _7442;
            Interval _5876 = Interval{ _6921, _6922 };
            Interval _7465 = jet_mul_derivative(_5875, _5876, intervalFailed, optical_product_upper);
            Interval _5877 = _7465;
            Interval _5878 = _7438;
            Interval _5879 = Interval{ _6925, _6926 };
            Interval _7467 = jet_mul_derivative(_5878, _5879, intervalFailed, optical_product_upper);
            Interval _5880 = _7467;
            Interval _7468 = jet_add_derivative(_5877, _5880, intervalFailed);
            Interval _5853 = _7445;
            Interval _5854 = Interval{ _6927, _6928 };
            Interval _7470 = imul(_5853, _5854, intervalFailed, optical_product_upper);
            Interval _5855 = _7447;
            Interval _5856 = Interval{ _6927, _6928 };
            Interval _7472 = jet_mul_derivative(_5855, _5856, intervalFailed, optical_product_upper);
            Interval _5857 = _7472;
            Interval _5858 = _7445;
            Interval _5859 = Interval{ _6929, _6930 };
            Interval _7474 = jet_mul_derivative(_5858, _5859, intervalFailed, optical_product_upper);
            Interval _5860 = _7474;
            Interval _7475 = jet_add_derivative(_5857, _5860, intervalFailed);
            Interval _5861 = _7449;
            Interval _5862 = Interval{ _6927, _6928 };
            Interval _7477 = jet_mul_derivative(_5861, _5862, intervalFailed, optical_product_upper);
            Interval _5863 = _7477;
            Interval _5864 = _7445;
            Interval _5865 = Interval{ _6931, _6932 };
            Interval _7479 = jet_mul_derivative(_5864, _5865, intervalFailed, optical_product_upper);
            Interval _5866 = _7479;
            Interval _7480 = jet_add_derivative(_5863, _5866, intervalFailed);
            Interval _5847 = _7458;
            Interval _5848 = _7470;
            Interval _7481 = iadd(_5847, _5848, intervalFailed);
            Interval _5849 = _7463;
            Interval _5850 = _7475;
            Interval _7482 = jet_add_derivative(_5849, _5850, intervalFailed);
            Interval _5851 = _7468;
            Interval _5852 = _7480;
            Interval _7483 = jet_add_derivative(_5851, _5852, intervalFailed);
            Interval _5833 = _7452;
            Interval _5834 = Interval{ _6933, _6934 };
            Interval _7485 = imul(_5833, _5834, intervalFailed, optical_product_upper);
            Interval _5835 = _7454;
            Interval _5836 = Interval{ _6933, _6934 };
            Interval _7487 = jet_mul_derivative(_5835, _5836, intervalFailed, optical_product_upper);
            Interval _5837 = _7487;
            Interval _5838 = _7452;
            Interval _5839 = Interval{ _6935, _6936 };
            Interval _7489 = jet_mul_derivative(_5838, _5839, intervalFailed, optical_product_upper);
            Interval _5840 = _7489;
            Interval _7490 = jet_add_derivative(_5837, _5840, intervalFailed);
            Interval _5841 = _7456;
            Interval _5842 = Interval{ _6933, _6934 };
            Interval _7492 = jet_mul_derivative(_5841, _5842, intervalFailed, optical_product_upper);
            Interval _5843 = _7492;
            Interval _5844 = _7452;
            Interval _5845 = Interval{ _6937, _6938 };
            Interval _7494 = jet_mul_derivative(_5844, _5845, intervalFailed, optical_product_upper);
            Interval _5846 = _7494;
            Interval _7495 = jet_add_derivative(_5843, _5846, intervalFailed);
            Interval _5827 = _7481;
            Interval _5828 = _7485;
            Interval _7496 = iadd(_5827, _5828, intervalFailed);
            Interval _5829 = _7482;
            Interval _5830 = _7490;
            Interval _7497 = jet_add_derivative(_5829, _5830, intervalFailed);
            Interval _5831 = _7483;
            Interval _5832 = _7495;
            Interval _7498 = jet_add_derivative(_5831, _5832, intervalFailed);
            Interval _5813 = _7496;
            Interval _5814 = _6993;
            Interval _7500 = idiv(_5813, _5814, intervalFailed, interval_divide_upper);
            bool _7509;
            if (!intervalFailed)
            {
                _7509 = intervalFailed;
            }
            else
            {
                _7509 = false;
            }
            bool _7514;
            if (_7509)
            {
                _7514 = jetFailureSite == 0u;
            }
            else
            {
                _7514 = false;
            }
            if (_7514)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7496.lo, _7496.hi, _6993.lo, _6993.hi);
            }
            Interval _5815 = _7497;
            Interval _5816 = _7500;
            Interval _5817 = _6994;
            Interval _7518 = jet_mul_derivative(_5816, _5817, intervalFailed, optical_product_upper);
            Interval _5818 = Interval{ as_type<float>(as_type<uint>(_7518.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7518.lo) ^ 2147483648u) };
            Interval _7528 = jet_add_derivative(_5815, _5818, intervalFailed);
            Interval _5819 = _7528;
            Interval _5820 = _6993;
            Interval _7529 = jet_div_derivative(_5819, _5820, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5821 = _7498;
            Interval _5822 = _7500;
            Interval _5823 = _6995;
            Interval _7530 = jet_mul_derivative(_5822, _5823, intervalFailed, optical_product_upper);
            Interval _5824 = Interval{ as_type<float>(as_type<uint>(_7530.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7530.lo) ^ 2147483648u) };
            Interval _7540 = jet_add_derivative(_5821, _5824, intervalFailed);
            Interval _5825 = _7540;
            Interval _5826 = _6993;
            Interval _7541 = jet_div_derivative(_5825, _5826, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            bool _7550;
            if ((isunordered(_7500.lo, _6759) || _7500.lo > _6759))
            {
                _7550 = _7500.hi >= 65536.0;
            }
            else
            {
                _7550 = true;
            }
            if (_7550)
            {
                return false;
            }
            Interval _5799 = Interval{ _6769, _6771 };
            Interval _5800 = _7500;
            Interval _7552 = imul(_5799, _5800, intervalFailed, optical_product_upper);
            Interval _5801 = Interval{ _6773, _6775 };
            Interval _5802 = _7500;
            Interval _7554 = jet_mul_derivative(_5801, _5802, intervalFailed, optical_product_upper);
            Interval _5803 = _7554;
            Interval _5804 = Interval{ _6769, _6771 };
            Interval _5805 = _7529;
            Interval _7556 = jet_mul_derivative(_5804, _5805, intervalFailed, optical_product_upper);
            Interval _5806 = _7556;
            Interval _7557 = jet_add_derivative(_5803, _5806, intervalFailed);
            Interval _5807 = Interval{ _6777, _6779 };
            Interval _5808 = _7500;
            Interval _7559 = jet_mul_derivative(_5807, _5808, intervalFailed, optical_product_upper);
            Interval _5809 = _7559;
            Interval _5810 = Interval{ _6769, _6771 };
            Interval _5811 = _7541;
            Interval _7561 = jet_mul_derivative(_5810, _5811, intervalFailed, optical_product_upper);
            Interval _5812 = _7561;
            Interval _7562 = jet_add_derivative(_5809, _5812, intervalFailed);
            Interval _5785 = Interval{ _6781, _6783 };
            Interval _5786 = _7500;
            Interval _7564 = imul(_5785, _5786, intervalFailed, optical_product_upper);
            Interval _5787 = Interval{ _6785, _6787 };
            Interval _5788 = _7500;
            Interval _7566 = jet_mul_derivative(_5787, _5788, intervalFailed, optical_product_upper);
            Interval _5789 = _7566;
            Interval _5790 = Interval{ _6781, _6783 };
            Interval _5791 = _7529;
            Interval _7568 = jet_mul_derivative(_5790, _5791, intervalFailed, optical_product_upper);
            Interval _5792 = _7568;
            Interval _7569 = jet_add_derivative(_5789, _5792, intervalFailed);
            Interval _5793 = Interval{ _6789, _6791 };
            Interval _5794 = _7500;
            Interval _7571 = jet_mul_derivative(_5793, _5794, intervalFailed, optical_product_upper);
            Interval _5795 = _7571;
            Interval _5796 = Interval{ _6781, _6783 };
            Interval _5797 = _7541;
            Interval _7573 = jet_mul_derivative(_5796, _5797, intervalFailed, optical_product_upper);
            Interval _5798 = _7573;
            Interval _7574 = jet_add_derivative(_5795, _5798, intervalFailed);
            Interval _5771 = Interval{ _6793, _6795 };
            Interval _5772 = _7500;
            Interval _7576 = imul(_5771, _5772, intervalFailed, optical_product_upper);
            Interval _5773 = Interval{ _6797, _6799 };
            Interval _5774 = _7500;
            Interval _7578 = jet_mul_derivative(_5773, _5774, intervalFailed, optical_product_upper);
            Interval _5775 = _7578;
            Interval _5776 = Interval{ _6793, _6795 };
            Interval _5777 = _7529;
            Interval _7580 = jet_mul_derivative(_5776, _5777, intervalFailed, optical_product_upper);
            Interval _5778 = _7580;
            Interval _7581 = jet_add_derivative(_5775, _5778, intervalFailed);
            Interval _5779 = Interval{ _6801, _6803 };
            Interval _5780 = _7500;
            Interval _7583 = jet_mul_derivative(_5779, _5780, intervalFailed, optical_product_upper);
            Interval _5781 = _7583;
            Interval _5782 = Interval{ _6793, _6795 };
            Interval _5783 = _7541;
            Interval _7585 = jet_mul_derivative(_5782, _5783, intervalFailed, optical_product_upper);
            Interval _5784 = _7585;
            Interval _7586 = jet_add_derivative(_5781, _5784, intervalFailed);
            Interval _5765 = Interval{ _6805, _6807 };
            Interval _5766 = _7552;
            Interval _7588 = iadd(_5765, _5766, intervalFailed);
            Interval _5767 = Interval{ _6809, _6811 };
            Interval _5768 = _7557;
            Interval _7590 = jet_add_derivative(_5767, _5768, intervalFailed);
            Interval _5769 = Interval{ _6813, _6815 };
            Interval _5770 = _7562;
            Interval _7592 = jet_add_derivative(_5769, _5770, intervalFailed);
            Interval _5759 = Interval{ _6817, _6819 };
            Interval _5760 = _7564;
            Interval _7594 = iadd(_5759, _5760, intervalFailed);
            Interval _5761 = Interval{ _6821, _6823 };
            Interval _5762 = _7569;
            Interval _7596 = jet_add_derivative(_5761, _5762, intervalFailed);
            Interval _5763 = Interval{ _6825, _6827 };
            Interval _5764 = _7574;
            Interval _7598 = jet_add_derivative(_5763, _5764, intervalFailed);
            Interval _5753 = Interval{ _6829, _6831 };
            Interval _5754 = _7576;
            Interval _7600 = iadd(_5753, _5754, intervalFailed);
            Interval _5755 = Interval{ _6833, _6835 };
            Interval _5756 = _7581;
            Interval _7602 = jet_add_derivative(_5755, _5756, intervalFailed);
            Interval _5757 = Interval{ _6837, _6839 };
            Interval _5758 = _7586;
            Interval _7604 = jet_add_derivative(_5757, _5758, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7588, _7590, _7592 }, OpticalJet{ _7594, _7596, _7598 }, OpticalJet{ _7600, _7602, _7604 } };
            _7609 = _7500.lo;
            _7610 = _7500.hi;
            _7611 = _7529.lo;
            _7612 = _7529.hi;
            _7613 = _7541.lo;
            _7614 = _7541.hi;
        }
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
        float _7941;
        float _7942;
        float _7943;
        float _7944;
        float _7945;
        float _7946;
        if (_6855)
        {
            float _7756;
            float _7757;
            float _7758;
            float _7759;
            float _7760;
            float _7761;
            if (_6845)
            {
                _7756 = _7609;
                _7757 = _7610;
                _7758 = _7611;
                _7759 = _7612;
                _7760 = _7613;
                _7761 = _7614;
            }
            else
            {
                bool _7628;
                if (hit.x.v.lo <= 0.0)
                {
                    _7628 = hit.x.v.hi >= 0.0;
                }
                else
                {
                    _7628 = false;
                }
                float _7635;
                if (_7628)
                {
                    _7635 = 0.0;
                }
                else
                {
                    _7635 = precise::min(abs(hit.x.v.lo), abs(hit.x.v.hi));
                }
                float _7638 = precise::max(abs(hit.x.v.lo), abs(hit.x.v.hi));
                float _5743 = spvFMul(_7635, _7635);
                float _7639 = interval_down(_5743, intervalFailed);
                float _5744 = spvFMul(_7638, _7638);
                float _7641 = interval_up(_5744, intervalFailed);
                Interval _5745 = Interval{ 2.0, 2.0 };
                Interval _5746 = hit.x.v;
                Interval _7642 = imul(_5745, _5746, intervalFailed, optical_product_upper);
                Interval _5747 = _7642;
                Interval _5748 = hit.x.dx;
                Interval _7643 = jet_mul_derivative(_5747, _5748, intervalFailed, optical_product_upper);
                Interval _5749 = Interval{ 2.0, 2.0 };
                Interval _5750 = hit.x.v;
                Interval _7644 = imul(_5749, _5750, intervalFailed, optical_product_upper);
                Interval _5751 = _7644;
                Interval _5752 = hit.x.dy;
                Interval _7645 = jet_mul_derivative(_5751, _5752, intervalFailed, optical_product_upper);
                bool _7655;
                if (hit.y.v.lo <= 0.0)
                {
                    _7655 = hit.y.v.hi >= 0.0;
                }
                else
                {
                    _7655 = false;
                }
                float _7662;
                if (_7655)
                {
                    _7662 = 0.0;
                }
                else
                {
                    _7662 = precise::min(abs(hit.y.v.lo), abs(hit.y.v.hi));
                }
                float _7665 = precise::max(abs(hit.y.v.lo), abs(hit.y.v.hi));
                float _5733 = spvFMul(_7662, _7662);
                float _7666 = interval_down(_5733, intervalFailed);
                float _5734 = spvFMul(_7665, _7665);
                float _7668 = interval_up(_5734, intervalFailed);
                Interval _5735 = Interval{ 2.0, 2.0 };
                Interval _5736 = hit.y.v;
                Interval _7669 = imul(_5735, _5736, intervalFailed, optical_product_upper);
                Interval _5737 = _7669;
                Interval _5738 = hit.y.dx;
                Interval _7670 = jet_mul_derivative(_5737, _5738, intervalFailed, optical_product_upper);
                Interval _5739 = Interval{ 2.0, 2.0 };
                Interval _5740 = hit.y.v;
                Interval _7671 = imul(_5739, _5740, intervalFailed, optical_product_upper);
                Interval _5741 = _7671;
                Interval _5742 = hit.y.dy;
                Interval _7672 = jet_mul_derivative(_5741, _5742, intervalFailed, optical_product_upper);
                Interval _5727 = Interval{ precise::max(0.0, _7639), _7641 };
                Interval _5728 = Interval{ precise::max(0.0, _7666), _7668 };
                Interval _7675 = iadd(_5727, _5728, intervalFailed);
                Interval _5729 = _7643;
                Interval _5730 = _7670;
                Interval _7676 = jet_add_derivative(_5729, _5730, intervalFailed);
                Interval _5731 = _7645;
                Interval _5732 = _7672;
                Interval _7677 = jet_add_derivative(_5731, _5732, intervalFailed);
                bool _7687;
                if (hit.z.v.lo <= 0.0)
                {
                    _7687 = hit.z.v.hi >= 0.0;
                }
                else
                {
                    _7687 = false;
                }
                float _7694;
                if (_7687)
                {
                    _7694 = 0.0;
                }
                else
                {
                    _7694 = precise::min(abs(hit.z.v.lo), abs(hit.z.v.hi));
                }
                float _7697 = precise::max(abs(hit.z.v.lo), abs(hit.z.v.hi));
                float _5717 = spvFMul(_7694, _7694);
                float _7698 = interval_down(_5717, intervalFailed);
                float _5718 = spvFMul(_7697, _7697);
                float _7700 = interval_up(_5718, intervalFailed);
                Interval _5719 = Interval{ 2.0, 2.0 };
                Interval _5720 = hit.z.v;
                Interval _7701 = imul(_5719, _5720, intervalFailed, optical_product_upper);
                Interval _5721 = _7701;
                Interval _5722 = hit.z.dx;
                Interval _7702 = jet_mul_derivative(_5721, _5722, intervalFailed, optical_product_upper);
                Interval _5723 = Interval{ 2.0, 2.0 };
                Interval _5724 = hit.z.v;
                Interval _7703 = imul(_5723, _5724, intervalFailed, optical_product_upper);
                Interval _5725 = _7703;
                Interval _5726 = hit.z.dy;
                Interval _7704 = jet_mul_derivative(_5725, _5726, intervalFailed, optical_product_upper);
                Interval _5711 = _7675;
                Interval _5712 = Interval{ precise::max(0.0, _7698), _7700 };
                Interval _7706 = iadd(_5711, _5712, intervalFailed);
                Interval _5713 = _7676;
                Interval _5714 = _7702;
                Interval _7707 = jet_add_derivative(_5713, _5714, intervalFailed);
                Interval _5715 = _7677;
                Interval _5716 = _7704;
                Interval _7708 = jet_add_derivative(_5715, _5716, intervalFailed);
                Interval _5702 = _7706;
                Interval _7710 = isqrt(_5702, intervalFailed);
                bool _7718;
                if (!intervalFailed)
                {
                    _7718 = intervalFailed;
                }
                else
                {
                    _7718 = false;
                }
                bool _7723;
                if (_7718)
                {
                    _7723 = jetFailureSite == 0u;
                }
                else
                {
                    _7723 = false;
                }
                if (_7723)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_7706.lo, _7706.hi, 0.0, 0.0);
                }
                if (_7710.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5703 = Interval{ 2.0, 2.0 };
                Interval _5704 = _7710;
                Interval _7730 = imul(_5703, _5704, intervalFailed, optical_product_upper);
                Interval _5705 = Interval{ 1.0, 1.0 };
                Interval _5706 = _7730;
                Interval _7732 = idiv(_5705, _5706, intervalFailed, interval_divide_upper);
                bool _7739;
                if (!intervalFailed)
                {
                    _7739 = intervalFailed;
                }
                else
                {
                    _7739 = false;
                }
                bool _7744;
                if (_7739)
                {
                    _7744 = jetFailureSite == 0u;
                }
                else
                {
                    _7744 = false;
                }
                if (_7744)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _7730.lo, _7730.hi);
                }
                Interval _5707 = _7732;
                Interval _5708 = _7707;
                Interval _7748 = jet_mul_derivative(_5707, _5708, intervalFailed, optical_product_upper);
                Interval _5709 = _7732;
                Interval _5710 = _7708;
                Interval _7749 = jet_mul_derivative(_5709, _5710, intervalFailed, optical_product_upper);
                _7756 = _7710.lo;
                _7757 = _7710.hi;
                _7758 = _7748.lo;
                _7759 = _7748.hi;
                _7760 = _7749.lo;
                _7761 = _7749.hi;
            }
            float _7765 = precise::max(liquid.projection.x, 1.0);
            Interval _5688 = Interval{ _7756, _7757 };
            Interval _5689 = Interval{ _7765, _7765 };
            Interval _7769 = idiv(_5688, _5689, intervalFailed, interval_divide_upper);
            bool _7774;
            if (!intervalFailed)
            {
                _7774 = intervalFailed;
            }
            else
            {
                _7774 = false;
            }
            bool _7779;
            if (_7774)
            {
                _7779 = jetFailureSite == 0u;
            }
            else
            {
                _7779 = false;
            }
            if (_7779)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7756, _7757, _7765, _7765);
            }
            Interval _5690 = Interval{ _7758, _7759 };
            Interval _5691 = _7769;
            Interval _5692 = Interval{ 0.0, 0.0 };
            Interval _7784 = jet_mul_derivative(_5691, _5692, intervalFailed, optical_product_upper);
            Interval _5693 = Interval{ as_type<float>(as_type<uint>(_7784.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7784.lo) ^ 2147483648u) };
            Interval _7794 = jet_add_derivative(_5690, _5693, intervalFailed);
            Interval _5694 = _7794;
            Interval _5695 = Interval{ _7765, _7765 };
            Interval _7796 = jet_div_derivative(_5694, _5695, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5696 = Interval{ _7760, _7761 };
            Interval _5697 = _7769;
            Interval _5698 = Interval{ 0.0, 0.0 };
            Interval _7798 = jet_mul_derivative(_5697, _5698, intervalFailed, optical_product_upper);
            Interval _5699 = Interval{ as_type<float>(as_type<uint>(_7798.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7798.lo) ^ 2147483648u) };
            Interval _7808 = jet_add_derivative(_5696, _5699, intervalFailed);
            Interval _5700 = _7808;
            Interval _5701 = Interval{ _7765, _7765 };
            Interval _7810 = jet_div_derivative(_5700, _5701, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            OpticalJet param_var_a = OpticalJet{ _6993, _6994, _6995 };
            OpticalJet param_var_a_1 = jabs(param_var_a);
            float _5686 = 4.0;
            float _5687 = 100.0;
            Interval _7814 = iratio(_5686, _5687, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _7819;
            if (!intervalFailed)
            {
                _7819 = intervalFailed;
            }
            else
            {
                _7819 = false;
            }
            bool _7824;
            if (_7819)
            {
                _7824 = jetFailureSite == 0u;
            }
            else
            {
                _7824 = false;
            }
            if (_7824)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(4.0, 4.0, 100.0, 100.0);
            }
            OpticalJet param_var_b = OpticalJet{ _7814, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet _7828 = jmax(param_var_a_1, param_var_b);
            Interval _7829 = _7828.v;
            Interval _5672 = _7769;
            Interval _5673 = _7829;
            Interval _7833 = idiv(_5672, _5673, intervalFailed, interval_divide_upper);
            bool _7842;
            if (!intervalFailed)
            {
                _7842 = intervalFailed;
            }
            else
            {
                _7842 = false;
            }
            bool _7847;
            if (_7842)
            {
                _7847 = jetFailureSite == 0u;
            }
            else
            {
                _7847 = false;
            }
            if (_7847)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7769.lo, _7769.hi, _7829.lo, _7829.hi);
            }
            Interval _5674 = _7796;
            Interval _5675 = _7833;
            Interval _5676 = _7828.dx;
            Interval _7851 = jet_mul_derivative(_5675, _5676, intervalFailed, optical_product_upper);
            Interval _5677 = Interval{ as_type<float>(as_type<uint>(_7851.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7851.lo) ^ 2147483648u) };
            Interval _7861 = jet_add_derivative(_5674, _5677, intervalFailed);
            Interval _5678 = _7861;
            Interval _5679 = _7829;
            Interval _7862 = jet_div_derivative(_5678, _5679, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5680 = _7810;
            Interval _5681 = _7833;
            Interval _5682 = _7828.dy;
            Interval _7863 = jet_mul_derivative(_5681, _5682, intervalFailed, optical_product_upper);
            Interval _5683 = Interval{ as_type<float>(as_type<uint>(_7863.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7863.lo) ^ 2147483648u) };
            Interval _7873 = jet_add_derivative(_5680, _5683, intervalFailed);
            Interval _5684 = _7873;
            Interval _5685 = _7829;
            Interval _7874 = jet_div_derivative(_5684, _5685, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ReflectionLiquidFrame param_var_f = liquid;
            OpticalJet3 param_var_direction_1 = OpticalJet3{ OpticalJet{ Interval{ _6769, _6771 }, Interval{ _6773, _6775 }, Interval{ _6777, _6779 } }, OpticalJet{ Interval{ _6781, _6783 }, Interval{ _6785, _6787 }, Interval{ _6789, _6791 } }, OpticalJet{ Interval{ _6793, _6795 }, Interval{ _6797, _6799 }, Interval{ _6801, _6803 } } };
            OpticalJet param_var_distance = OpticalJet{ Interval{ _7609, _7610 }, Interval{ _7611, _7612 }, Interval{ _7613, _7614 } };
            OpticalJet param_var_footprint = OpticalJet{ _7833, _7862, _7874 };
            OpticalJet3 _7894 = jet_liquid_normal(param_var_f, param_var_direction_1, param_var_distance, param_var_footprint, hit, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            _7929 = _7894.x.v.lo;
            _7930 = _7894.x.v.hi;
            _7931 = _7894.x.dx.lo;
            _7932 = _7894.x.dx.hi;
            _7933 = _7894.x.dy.lo;
            _7934 = _7894.x.dy.hi;
            _7935 = _7894.y.v.lo;
            _7936 = _7894.y.v.hi;
            _7937 = _7894.y.dx.lo;
            _7938 = _7894.y.dx.hi;
            _7939 = _7894.y.dy.lo;
            _7940 = _7894.y.dy.hi;
            _7941 = _7894.z.v.lo;
            _7942 = _7894.z.v.hi;
            _7943 = _7894.z.dx.lo;
            _7944 = _7894.z.dx.hi;
            _7945 = _7894.z.dy.lo;
            _7946 = _7894.z.dy.hi;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane_1 = ReflectionSpecularPlane{ _6867, _6868, _6869 };
            OpticalJet3 param_var_hit = hit;
            bool _7927 = jet_inside_face(param_var_plane_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (!_7927)
            {
                return false;
            }
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
            _7941 = _6933;
            _7942 = _6934;
            _7943 = _6935;
            _7944 = _6936;
            _7945 = _6937;
            _7946 = _6938;
        }
        bool _7948;
        if (_6845)
        {
            _7948 = !_6855;
        }
        else
        {
            _7948 = false;
        }
        bool _7962;
        if (_7948)
        {
            bool _7955;
            if (_6867.w == 2.0)
            {
                _7955 = _6868.w == 2.0;
            }
            else
            {
                _7955 = false;
            }
            bool _7960;
            if (_7955)
            {
                _7960 = _6869.w == 2.0;
            }
            else
            {
                _7960 = false;
            }
            _7962 = !_7960;
        }
        else
        {
            _7962 = false;
        }
        int _7963;
        if (_7962)
        {
            _7963 = 1;
        }
        else
        {
            _7963 = 5;
        }
        float _7964 = float(_7963);
        float _5670 = _7964;
        float _5671 = 100.0;
        Interval _7966 = iratio(_5670, _5671, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7971;
        if (!intervalFailed)
        {
            _7971 = intervalFailed;
        }
        else
        {
            _7971 = false;
        }
        bool _7976;
        if (_7971)
        {
            _7976 = jetFailureSite == 0u;
        }
        else
        {
            _7976 = false;
        }
        if (_7976)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(_7964, _7964, 100.0, 100.0);
        }
        OpticalJet param_var_a_2 = OpticalJet{ _7966, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
        float _5668 = 1.0;
        float _5669 = 100000.0;
        Interval _7982 = iratio(_5668, _5669, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7987;
        if (!intervalFailed)
        {
            _7987 = intervalFailed;
        }
        else
        {
            _7987 = false;
        }
        bool _7992;
        if (_7987)
        {
            _7992 = jetFailureSite == 0u;
        }
        else
        {
            _7992 = false;
        }
        if (_7992)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(1.0, 1.0, 100000.0, 100000.0);
        }
        Interval _5654 = Interval{ _7609, _7610 };
        Interval _5655 = _7982;
        Interval _7996 = imul(_5654, _5655, intervalFailed, optical_product_upper);
        Interval _5656 = Interval{ _7611, _7612 };
        Interval _5657 = _7982;
        Interval _7998 = jet_mul_derivative(_5656, _5657, intervalFailed, optical_product_upper);
        Interval _5658 = _7998;
        Interval _5659 = Interval{ _7609, _7610 };
        Interval _5660 = Interval{ 0.0, 0.0 };
        Interval _8000 = jet_mul_derivative(_5659, _5660, intervalFailed, optical_product_upper);
        Interval _5661 = _8000;
        Interval _8001 = jet_add_derivative(_5658, _5661, intervalFailed);
        Interval _5662 = Interval{ _7613, _7614 };
        Interval _5663 = _7982;
        Interval _8003 = jet_mul_derivative(_5662, _5663, intervalFailed, optical_product_upper);
        Interval _5664 = _8003;
        Interval _5665 = Interval{ _7609, _7610 };
        Interval _5666 = Interval{ 0.0, 0.0 };
        Interval _8005 = jet_mul_derivative(_5665, _5666, intervalFailed, optical_product_upper);
        Interval _5667 = _8005;
        Interval _8006 = jet_add_derivative(_5664, _5667, intervalFailed);
        OpticalJet param_var_b_1 = OpticalJet{ _7996, _8001, _8006 };
        OpticalJet _8008 = jmax(param_var_a_2, param_var_b_1);
        Interval _8009 = _8008.v;
        _6758 = _8009.lo;
        _6760 = _8009.hi;
        Interval _8010 = _8008.dx;
        _6762 = _8010.lo;
        _6764 = _8010.hi;
        Interval _8011 = _8008.dy;
        _6766 = _8011.lo;
        _6768 = _8011.hi;
        Interval _8016 = _8008.v;
        Interval _5640 = Interval{ _7929, _7930 };
        Interval _5641 = _8016;
        Interval _8020 = imul(_5640, _5641, intervalFailed, optical_product_upper);
        Interval _5642 = Interval{ _7931, _7932 };
        Interval _5643 = _8016;
        Interval _8022 = jet_mul_derivative(_5642, _5643, intervalFailed, optical_product_upper);
        Interval _5644 = _8022;
        Interval _5645 = Interval{ _7929, _7930 };
        Interval _5646 = _8008.dx;
        Interval _8024 = jet_mul_derivative(_5645, _5646, intervalFailed, optical_product_upper);
        Interval _5647 = _8024;
        Interval _8025 = jet_add_derivative(_5644, _5647, intervalFailed);
        Interval _5648 = Interval{ _7933, _7934 };
        Interval _5649 = _8016;
        Interval _8027 = jet_mul_derivative(_5648, _5649, intervalFailed, optical_product_upper);
        Interval _5650 = _8027;
        Interval _5651 = Interval{ _7929, _7930 };
        Interval _5652 = _8008.dy;
        Interval _8029 = jet_mul_derivative(_5651, _5652, intervalFailed, optical_product_upper);
        Interval _5653 = _8029;
        Interval _8030 = jet_add_derivative(_5650, _5653, intervalFailed);
        Interval _8031 = _8008.v;
        Interval _5626 = Interval{ _7935, _7936 };
        Interval _5627 = _8031;
        Interval _8035 = imul(_5626, _5627, intervalFailed, optical_product_upper);
        Interval _5628 = Interval{ _7937, _7938 };
        Interval _5629 = _8031;
        Interval _8037 = jet_mul_derivative(_5628, _5629, intervalFailed, optical_product_upper);
        Interval _5630 = _8037;
        Interval _5631 = Interval{ _7935, _7936 };
        Interval _5632 = _8008.dx;
        Interval _8039 = jet_mul_derivative(_5631, _5632, intervalFailed, optical_product_upper);
        Interval _5633 = _8039;
        Interval _8040 = jet_add_derivative(_5630, _5633, intervalFailed);
        Interval _5634 = Interval{ _7939, _7940 };
        Interval _5635 = _8031;
        Interval _8042 = jet_mul_derivative(_5634, _5635, intervalFailed, optical_product_upper);
        Interval _5636 = _8042;
        Interval _5637 = Interval{ _7935, _7936 };
        Interval _5638 = _8008.dy;
        Interval _8044 = jet_mul_derivative(_5637, _5638, intervalFailed, optical_product_upper);
        Interval _5639 = _8044;
        Interval _8045 = jet_add_derivative(_5636, _5639, intervalFailed);
        Interval _8046 = _8008.v;
        Interval _5612 = Interval{ _7941, _7942 };
        Interval _5613 = _8046;
        Interval _8050 = imul(_5612, _5613, intervalFailed, optical_product_upper);
        Interval _5614 = Interval{ _7943, _7944 };
        Interval _5615 = _8046;
        Interval _8052 = jet_mul_derivative(_5614, _5615, intervalFailed, optical_product_upper);
        Interval _5616 = _8052;
        Interval _5617 = Interval{ _7941, _7942 };
        Interval _5618 = _8008.dx;
        Interval _8054 = jet_mul_derivative(_5617, _5618, intervalFailed, optical_product_upper);
        Interval _5619 = _8054;
        Interval _8055 = jet_add_derivative(_5616, _5619, intervalFailed);
        Interval _5620 = Interval{ _7945, _7946 };
        Interval _5621 = _8046;
        Interval _8057 = jet_mul_derivative(_5620, _5621, intervalFailed, optical_product_upper);
        Interval _5622 = _8057;
        Interval _5623 = Interval{ _7941, _7942 };
        Interval _5624 = _8008.dy;
        Interval _8059 = jet_mul_derivative(_5623, _5624, intervalFailed, optical_product_upper);
        Interval _5625 = _8059;
        Interval _8060 = jet_add_derivative(_5622, _5625, intervalFailed);
        Interval _5606 = hit.x.v;
        Interval _5607 = _8020;
        Interval _8064 = iadd(_5606, _5607, intervalFailed);
        Interval _5608 = hit.x.dx;
        Interval _5609 = _8025;
        Interval _8065 = jet_add_derivative(_5608, _5609, intervalFailed);
        Interval _5610 = hit.x.dy;
        Interval _5611 = _8030;
        Interval _8066 = jet_add_derivative(_5610, _5611, intervalFailed);
        Interval _5600 = hit.y.v;
        Interval _5601 = _8035;
        Interval _8070 = iadd(_5600, _5601, intervalFailed);
        Interval _5602 = hit.y.dx;
        Interval _5603 = _8040;
        Interval _8071 = jet_add_derivative(_5602, _5603, intervalFailed);
        Interval _5604 = hit.y.dy;
        Interval _5605 = _8045;
        Interval _8072 = jet_add_derivative(_5604, _5605, intervalFailed);
        Interval _5594 = hit.z.v;
        Interval _5595 = _8050;
        Interval _8076 = iadd(_5594, _5595, intervalFailed);
        Interval _5596 = hit.z.dx;
        Interval _5597 = _8055;
        Interval _8077 = jet_add_derivative(_5596, _5597, intervalFailed);
        Interval _5598 = hit.z.dy;
        Interval _5599 = _8060;
        Interval _8078 = jet_add_derivative(_5598, _5599, intervalFailed);
        _6806 = _8064.lo;
        _6808 = _8064.hi;
        _6810 = _8065.lo;
        _6812 = _8065.hi;
        _6814 = _8066.lo;
        _6816 = _8066.hi;
        _6818 = _8070.lo;
        _6820 = _8070.hi;
        _6822 = _8071.lo;
        _6824 = _8071.hi;
        _6826 = _8072.lo;
        _6828 = _8072.hi;
        _6830 = _8076.lo;
        _6832 = _8076.hi;
        _6834 = _8077.lo;
        _6836 = _8077.hi;
        _6838 = _8078.lo;
        _6840 = _8078.hi;
        Interval _5580 = Interval{ _6769, _6771 };
        Interval _5581 = Interval{ _7929, _7930 };
        Interval _8081 = imul(_5580, _5581, intervalFailed, optical_product_upper);
        Interval _5582 = Interval{ _6773, _6775 };
        Interval _5583 = Interval{ _7929, _7930 };
        Interval _8084 = jet_mul_derivative(_5582, _5583, intervalFailed, optical_product_upper);
        Interval _5584 = _8084;
        Interval _5585 = Interval{ _6769, _6771 };
        Interval _5586 = Interval{ _7931, _7932 };
        Interval _8087 = jet_mul_derivative(_5585, _5586, intervalFailed, optical_product_upper);
        Interval _5587 = _8087;
        Interval _8088 = jet_add_derivative(_5584, _5587, intervalFailed);
        Interval _5588 = Interval{ _6777, _6779 };
        Interval _5589 = Interval{ _7929, _7930 };
        Interval _8091 = jet_mul_derivative(_5588, _5589, intervalFailed, optical_product_upper);
        Interval _5590 = _8091;
        Interval _5591 = Interval{ _6769, _6771 };
        Interval _5592 = Interval{ _7933, _7934 };
        Interval _8094 = jet_mul_derivative(_5591, _5592, intervalFailed, optical_product_upper);
        Interval _5593 = _8094;
        Interval _8095 = jet_add_derivative(_5590, _5593, intervalFailed);
        Interval _5566 = Interval{ _6781, _6783 };
        Interval _5567 = Interval{ _7935, _7936 };
        Interval _8098 = imul(_5566, _5567, intervalFailed, optical_product_upper);
        Interval _5568 = Interval{ _6785, _6787 };
        Interval _5569 = Interval{ _7935, _7936 };
        Interval _8101 = jet_mul_derivative(_5568, _5569, intervalFailed, optical_product_upper);
        Interval _5570 = _8101;
        Interval _5571 = Interval{ _6781, _6783 };
        Interval _5572 = Interval{ _7937, _7938 };
        Interval _8104 = jet_mul_derivative(_5571, _5572, intervalFailed, optical_product_upper);
        Interval _5573 = _8104;
        Interval _8105 = jet_add_derivative(_5570, _5573, intervalFailed);
        Interval _5574 = Interval{ _6789, _6791 };
        Interval _5575 = Interval{ _7935, _7936 };
        Interval _8108 = jet_mul_derivative(_5574, _5575, intervalFailed, optical_product_upper);
        Interval _5576 = _8108;
        Interval _5577 = Interval{ _6781, _6783 };
        Interval _5578 = Interval{ _7939, _7940 };
        Interval _8111 = jet_mul_derivative(_5577, _5578, intervalFailed, optical_product_upper);
        Interval _5579 = _8111;
        Interval _8112 = jet_add_derivative(_5576, _5579, intervalFailed);
        Interval _5560 = _8081;
        Interval _5561 = _8098;
        Interval _8113 = iadd(_5560, _5561, intervalFailed);
        Interval _5562 = _8088;
        Interval _5563 = _8105;
        Interval _8114 = jet_add_derivative(_5562, _5563, intervalFailed);
        Interval _5564 = _8095;
        Interval _5565 = _8112;
        Interval _8115 = jet_add_derivative(_5564, _5565, intervalFailed);
        Interval _5546 = Interval{ _6793, _6795 };
        Interval _5547 = Interval{ _7941, _7942 };
        Interval _8118 = imul(_5546, _5547, intervalFailed, optical_product_upper);
        Interval _5548 = Interval{ _6797, _6799 };
        Interval _5549 = Interval{ _7941, _7942 };
        Interval _8121 = jet_mul_derivative(_5548, _5549, intervalFailed, optical_product_upper);
        Interval _5550 = _8121;
        Interval _5551 = Interval{ _6793, _6795 };
        Interval _5552 = Interval{ _7943, _7944 };
        Interval _8124 = jet_mul_derivative(_5551, _5552, intervalFailed, optical_product_upper);
        Interval _5553 = _8124;
        Interval _8125 = jet_add_derivative(_5550, _5553, intervalFailed);
        Interval _5554 = Interval{ _6801, _6803 };
        Interval _5555 = Interval{ _7941, _7942 };
        Interval _8128 = jet_mul_derivative(_5554, _5555, intervalFailed, optical_product_upper);
        Interval _5556 = _8128;
        Interval _5557 = Interval{ _6793, _6795 };
        Interval _5558 = Interval{ _7945, _7946 };
        Interval _8131 = jet_mul_derivative(_5557, _5558, intervalFailed, optical_product_upper);
        Interval _5559 = _8131;
        Interval _8132 = jet_add_derivative(_5556, _5559, intervalFailed);
        Interval _5540 = _8113;
        Interval _5541 = _8118;
        Interval _8133 = iadd(_5540, _5541, intervalFailed);
        Interval _5542 = _8114;
        Interval _5543 = _8125;
        Interval _8134 = jet_add_derivative(_5542, _5543, intervalFailed);
        Interval _5544 = _8115;
        Interval _5545 = _8132;
        Interval _8135 = jet_add_derivative(_5544, _5545, intervalFailed);
        Interval _5526 = Interval{ 2.0, 2.0 };
        Interval _5527 = _8133;
        Interval _8136 = imul(_5526, _5527, intervalFailed, optical_product_upper);
        Interval _5528 = Interval{ 0.0, 0.0 };
        Interval _5529 = _8133;
        Interval _8137 = jet_mul_derivative(_5528, _5529, intervalFailed, optical_product_upper);
        Interval _5530 = _8137;
        Interval _5531 = Interval{ 2.0, 2.0 };
        Interval _5532 = _8134;
        Interval _8138 = jet_mul_derivative(_5531, _5532, intervalFailed, optical_product_upper);
        Interval _5533 = _8138;
        Interval _8139 = jet_add_derivative(_5530, _5533, intervalFailed);
        Interval _5534 = Interval{ 0.0, 0.0 };
        Interval _5535 = _8133;
        Interval _8140 = jet_mul_derivative(_5534, _5535, intervalFailed, optical_product_upper);
        Interval _5536 = _8140;
        Interval _5537 = Interval{ 2.0, 2.0 };
        Interval _5538 = _8135;
        Interval _8141 = jet_mul_derivative(_5537, _5538, intervalFailed, optical_product_upper);
        Interval _5539 = _8141;
        Interval _8142 = jet_add_derivative(_5536, _5539, intervalFailed);
        Interval _5512 = Interval{ _7929, _7930 };
        Interval _5513 = _8136;
        Interval _8144 = imul(_5512, _5513, intervalFailed, optical_product_upper);
        Interval _5514 = Interval{ _7931, _7932 };
        Interval _5515 = _8136;
        Interval _8146 = jet_mul_derivative(_5514, _5515, intervalFailed, optical_product_upper);
        Interval _5516 = _8146;
        Interval _5517 = Interval{ _7929, _7930 };
        Interval _5518 = _8139;
        Interval _8148 = jet_mul_derivative(_5517, _5518, intervalFailed, optical_product_upper);
        Interval _5519 = _8148;
        Interval _8149 = jet_add_derivative(_5516, _5519, intervalFailed);
        Interval _5520 = Interval{ _7933, _7934 };
        Interval _5521 = _8136;
        Interval _8151 = jet_mul_derivative(_5520, _5521, intervalFailed, optical_product_upper);
        Interval _5522 = _8151;
        Interval _5523 = Interval{ _7929, _7930 };
        Interval _5524 = _8142;
        Interval _8153 = jet_mul_derivative(_5523, _5524, intervalFailed, optical_product_upper);
        Interval _5525 = _8153;
        Interval _8154 = jet_add_derivative(_5522, _5525, intervalFailed);
        Interval _5498 = Interval{ _7935, _7936 };
        Interval _5499 = _8136;
        Interval _8156 = imul(_5498, _5499, intervalFailed, optical_product_upper);
        Interval _5500 = Interval{ _7937, _7938 };
        Interval _5501 = _8136;
        Interval _8158 = jet_mul_derivative(_5500, _5501, intervalFailed, optical_product_upper);
        Interval _5502 = _8158;
        Interval _5503 = Interval{ _7935, _7936 };
        Interval _5504 = _8139;
        Interval _8160 = jet_mul_derivative(_5503, _5504, intervalFailed, optical_product_upper);
        Interval _5505 = _8160;
        Interval _8161 = jet_add_derivative(_5502, _5505, intervalFailed);
        Interval _5506 = Interval{ _7939, _7940 };
        Interval _5507 = _8136;
        Interval _8163 = jet_mul_derivative(_5506, _5507, intervalFailed, optical_product_upper);
        Interval _5508 = _8163;
        Interval _5509 = Interval{ _7935, _7936 };
        Interval _5510 = _8142;
        Interval _8165 = jet_mul_derivative(_5509, _5510, intervalFailed, optical_product_upper);
        Interval _5511 = _8165;
        Interval _8166 = jet_add_derivative(_5508, _5511, intervalFailed);
        Interval _5484 = Interval{ _7941, _7942 };
        Interval _5485 = _8136;
        Interval _8168 = imul(_5484, _5485, intervalFailed, optical_product_upper);
        Interval _5486 = Interval{ _7943, _7944 };
        Interval _5487 = _8136;
        Interval _8170 = jet_mul_derivative(_5486, _5487, intervalFailed, optical_product_upper);
        Interval _5488 = _8170;
        Interval _5489 = Interval{ _7941, _7942 };
        Interval _5490 = _8139;
        Interval _8172 = jet_mul_derivative(_5489, _5490, intervalFailed, optical_product_upper);
        Interval _5491 = _8172;
        Interval _8173 = jet_add_derivative(_5488, _5491, intervalFailed);
        Interval _5492 = Interval{ _7945, _7946 };
        Interval _5493 = _8136;
        Interval _8175 = jet_mul_derivative(_5492, _5493, intervalFailed, optical_product_upper);
        Interval _5494 = _8175;
        Interval _5495 = Interval{ _7941, _7942 };
        Interval _5496 = _8142;
        Interval _8177 = jet_mul_derivative(_5495, _5496, intervalFailed, optical_product_upper);
        Interval _5497 = _8177;
        Interval _8178 = jet_add_derivative(_5494, _5497, intervalFailed);
        Interval _5478 = Interval{ _6769, _6771 };
        Interval _5479 = Interval{ as_type<float>(as_type<uint>(_8144.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8144.lo) ^ 2147483648u) };
        Interval _8253 = iadd(_5478, _5479, intervalFailed);
        Interval _5480 = Interval{ _6773, _6775 };
        Interval _5481 = Interval{ as_type<float>(as_type<uint>(_8149.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8149.lo) ^ 2147483648u) };
        Interval _8256 = jet_add_derivative(_5480, _5481, intervalFailed);
        Interval _5482 = Interval{ _6777, _6779 };
        Interval _5483 = Interval{ as_type<float>(as_type<uint>(_8154.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8154.lo) ^ 2147483648u) };
        Interval _8259 = jet_add_derivative(_5482, _5483, intervalFailed);
        Interval _5472 = Interval{ _6781, _6783 };
        Interval _5473 = Interval{ as_type<float>(as_type<uint>(_8156.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8156.lo) ^ 2147483648u) };
        Interval _8262 = iadd(_5472, _5473, intervalFailed);
        Interval _5474 = Interval{ _6785, _6787 };
        Interval _5475 = Interval{ as_type<float>(as_type<uint>(_8161.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8161.lo) ^ 2147483648u) };
        Interval _8265 = jet_add_derivative(_5474, _5475, intervalFailed);
        Interval _5476 = Interval{ _6789, _6791 };
        Interval _5477 = Interval{ as_type<float>(as_type<uint>(_8166.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8166.lo) ^ 2147483648u) };
        Interval _8268 = jet_add_derivative(_5476, _5477, intervalFailed);
        Interval _5466 = Interval{ _6793, _6795 };
        Interval _5467 = Interval{ as_type<float>(as_type<uint>(_8168.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8168.lo) ^ 2147483648u) };
        Interval _8271 = iadd(_5466, _5467, intervalFailed);
        Interval _5468 = Interval{ _6797, _6799 };
        Interval _5469 = Interval{ as_type<float>(as_type<uint>(_8173.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8173.lo) ^ 2147483648u) };
        Interval _8274 = jet_add_derivative(_5468, _5469, intervalFailed);
        Interval _5470 = Interval{ _6801, _6803 };
        Interval _5471 = Interval{ as_type<float>(as_type<uint>(_8178.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8178.lo) ^ 2147483648u) };
        Interval _8277 = jet_add_derivative(_5470, _5471, intervalFailed);
        bool _8297;
        if (_6845)
        {
            _8297 = !_6855;
        }
        else
        {
            _8297 = false;
        }
        bool _8302;
        if (_8297)
        {
            _8302 = receiver.settings.x != 0.0;
        }
        else
        {
            _8302 = false;
        }
        if (_8302)
        {
            bool _8309;
            if (_8262.lo <= 0.0)
            {
                _8309 = _8262.hi >= 0.0;
            }
            else
            {
                _8309 = false;
            }
            float _8316;
            if (_8309)
            {
                _8316 = 0.0;
            }
            else
            {
                _8316 = precise::min(abs(_8262.lo), abs(_8262.hi));
            }
            float _5464 = 0.949999988079071044921875;
            float _8320 = interval_down(_5464, intervalFailed);
            float _5465 = 0.949999988079071044921875;
            __attribute__((unused)) float _8321 = interval_up(_5465, intervalFailed);
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
            float _9006;
            float _9007;
            float _9008;
            float _9009;
            float _9010;
            float _9011;
            if (precise::max(abs(_8262.lo), abs(_8262.hi)) < _8320)
            {
                Interval _5450 = _8262;
                Interval _5451 = Interval{ 0.0, 0.0 };
                Interval _8323 = imul(_5450, _5451, intervalFailed, optical_product_upper);
                Interval _5452 = _8265;
                Interval _5453 = Interval{ 0.0, 0.0 };
                Interval _8324 = jet_mul_derivative(_5452, _5453, intervalFailed, optical_product_upper);
                Interval _5454 = _8324;
                Interval _5455 = _8262;
                Interval _5456 = Interval{ 0.0, 0.0 };
                Interval _8325 = jet_mul_derivative(_5455, _5456, intervalFailed, optical_product_upper);
                Interval _5457 = _8325;
                Interval _8326 = jet_add_derivative(_5454, _5457, intervalFailed);
                Interval _5458 = _8268;
                Interval _5459 = Interval{ 0.0, 0.0 };
                Interval _8327 = jet_mul_derivative(_5458, _5459, intervalFailed, optical_product_upper);
                Interval _5460 = _8327;
                Interval _5461 = _8262;
                Interval _5462 = Interval{ 0.0, 0.0 };
                Interval _8328 = jet_mul_derivative(_5461, _5462, intervalFailed, optical_product_upper);
                Interval _5463 = _8328;
                Interval _8329 = jet_add_derivative(_5460, _5463, intervalFailed);
                Interval _5436 = _8271;
                Interval _5437 = Interval{ 1.0, 1.0 };
                Interval _8330 = imul(_5436, _5437, intervalFailed, optical_product_upper);
                Interval _5438 = _8274;
                Interval _5439 = Interval{ 1.0, 1.0 };
                Interval _8331 = jet_mul_derivative(_5438, _5439, intervalFailed, optical_product_upper);
                Interval _5440 = _8331;
                Interval _5441 = _8271;
                Interval _5442 = Interval{ 0.0, 0.0 };
                Interval _8332 = jet_mul_derivative(_5441, _5442, intervalFailed, optical_product_upper);
                Interval _5443 = _8332;
                Interval _8333 = jet_add_derivative(_5440, _5443, intervalFailed);
                Interval _5444 = _8277;
                Interval _5445 = Interval{ 1.0, 1.0 };
                Interval _8334 = jet_mul_derivative(_5444, _5445, intervalFailed, optical_product_upper);
                Interval _5446 = _8334;
                Interval _5447 = _8271;
                Interval _5448 = Interval{ 0.0, 0.0 };
                Interval _8335 = jet_mul_derivative(_5447, _5448, intervalFailed, optical_product_upper);
                Interval _5449 = _8335;
                Interval _8336 = jet_add_derivative(_5446, _5449, intervalFailed);
                Interval _5430 = _8323;
                Interval _5431 = Interval{ as_type<float>(as_type<uint>(_8330.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8330.lo) ^ 2147483648u) };
                Interval _8362 = iadd(_5430, _5431, intervalFailed);
                Interval _5432 = _8326;
                Interval _5433 = Interval{ as_type<float>(as_type<uint>(_8333.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8333.lo) ^ 2147483648u) };
                Interval _8364 = jet_add_derivative(_5432, _5433, intervalFailed);
                Interval _5434 = _8329;
                Interval _5435 = Interval{ as_type<float>(as_type<uint>(_8336.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8336.lo) ^ 2147483648u) };
                Interval _8366 = jet_add_derivative(_5434, _5435, intervalFailed);
                Interval _5416 = _8271;
                Interval _5417 = Interval{ 0.0, 0.0 };
                Interval _8367 = imul(_5416, _5417, intervalFailed, optical_product_upper);
                Interval _5418 = _8274;
                Interval _5419 = Interval{ 0.0, 0.0 };
                Interval _8368 = jet_mul_derivative(_5418, _5419, intervalFailed, optical_product_upper);
                Interval _5420 = _8368;
                Interval _5421 = _8271;
                Interval _5422 = Interval{ 0.0, 0.0 };
                Interval _8369 = jet_mul_derivative(_5421, _5422, intervalFailed, optical_product_upper);
                Interval _5423 = _8369;
                Interval _8370 = jet_add_derivative(_5420, _5423, intervalFailed);
                Interval _5424 = _8277;
                Interval _5425 = Interval{ 0.0, 0.0 };
                Interval _8371 = jet_mul_derivative(_5424, _5425, intervalFailed, optical_product_upper);
                Interval _5426 = _8371;
                Interval _5427 = _8271;
                Interval _5428 = Interval{ 0.0, 0.0 };
                Interval _8372 = jet_mul_derivative(_5427, _5428, intervalFailed, optical_product_upper);
                Interval _5429 = _8372;
                Interval _8373 = jet_add_derivative(_5426, _5429, intervalFailed);
                Interval _5402 = _8253;
                Interval _5403 = Interval{ 0.0, 0.0 };
                Interval _8374 = imul(_5402, _5403, intervalFailed, optical_product_upper);
                Interval _5404 = _8256;
                Interval _5405 = Interval{ 0.0, 0.0 };
                Interval _8375 = jet_mul_derivative(_5404, _5405, intervalFailed, optical_product_upper);
                Interval _5406 = _8375;
                Interval _5407 = _8253;
                Interval _5408 = Interval{ 0.0, 0.0 };
                Interval _8376 = jet_mul_derivative(_5407, _5408, intervalFailed, optical_product_upper);
                Interval _5409 = _8376;
                Interval _8377 = jet_add_derivative(_5406, _5409, intervalFailed);
                Interval _5410 = _8259;
                Interval _5411 = Interval{ 0.0, 0.0 };
                Interval _8378 = jet_mul_derivative(_5410, _5411, intervalFailed, optical_product_upper);
                Interval _5412 = _8378;
                Interval _5413 = _8253;
                Interval _5414 = Interval{ 0.0, 0.0 };
                Interval _8379 = jet_mul_derivative(_5413, _5414, intervalFailed, optical_product_upper);
                Interval _5415 = _8379;
                Interval _8380 = jet_add_derivative(_5412, _5415, intervalFailed);
                Interval _5396 = _8367;
                Interval _5397 = Interval{ as_type<float>(as_type<uint>(_8374.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8374.lo) ^ 2147483648u) };
                Interval _8406 = iadd(_5396, _5397, intervalFailed);
                Interval _5398 = _8370;
                Interval _5399 = Interval{ as_type<float>(as_type<uint>(_8377.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8377.lo) ^ 2147483648u) };
                Interval _8408 = jet_add_derivative(_5398, _5399, intervalFailed);
                Interval _5400 = _8373;
                Interval _5401 = Interval{ as_type<float>(as_type<uint>(_8380.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8380.lo) ^ 2147483648u) };
                Interval _8410 = jet_add_derivative(_5400, _5401, intervalFailed);
                Interval _5382 = _8253;
                Interval _5383 = Interval{ 1.0, 1.0 };
                Interval _8411 = imul(_5382, _5383, intervalFailed, optical_product_upper);
                Interval _5384 = _8256;
                Interval _5385 = Interval{ 1.0, 1.0 };
                Interval _8412 = jet_mul_derivative(_5384, _5385, intervalFailed, optical_product_upper);
                Interval _5386 = _8412;
                Interval _5387 = _8253;
                Interval _5388 = Interval{ 0.0, 0.0 };
                Interval _8413 = jet_mul_derivative(_5387, _5388, intervalFailed, optical_product_upper);
                Interval _5389 = _8413;
                Interval _8414 = jet_add_derivative(_5386, _5389, intervalFailed);
                Interval _5390 = _8259;
                Interval _5391 = Interval{ 1.0, 1.0 };
                Interval _8415 = jet_mul_derivative(_5390, _5391, intervalFailed, optical_product_upper);
                Interval _5392 = _8415;
                Interval _5393 = _8253;
                Interval _5394 = Interval{ 0.0, 0.0 };
                Interval _8416 = jet_mul_derivative(_5393, _5394, intervalFailed, optical_product_upper);
                Interval _5395 = _8416;
                Interval _8417 = jet_add_derivative(_5392, _5395, intervalFailed);
                Interval _5368 = _8262;
                Interval _5369 = Interval{ 0.0, 0.0 };
                Interval _8418 = imul(_5368, _5369, intervalFailed, optical_product_upper);
                Interval _5370 = _8265;
                Interval _5371 = Interval{ 0.0, 0.0 };
                Interval _8419 = jet_mul_derivative(_5370, _5371, intervalFailed, optical_product_upper);
                Interval _5372 = _8419;
                Interval _5373 = _8262;
                Interval _5374 = Interval{ 0.0, 0.0 };
                Interval _8420 = jet_mul_derivative(_5373, _5374, intervalFailed, optical_product_upper);
                Interval _5375 = _8420;
                Interval _8421 = jet_add_derivative(_5372, _5375, intervalFailed);
                Interval _5376 = _8268;
                Interval _5377 = Interval{ 0.0, 0.0 };
                Interval _8422 = jet_mul_derivative(_5376, _5377, intervalFailed, optical_product_upper);
                Interval _5378 = _8422;
                Interval _5379 = _8262;
                Interval _5380 = Interval{ 0.0, 0.0 };
                Interval _8423 = jet_mul_derivative(_5379, _5380, intervalFailed, optical_product_upper);
                Interval _5381 = _8423;
                Interval _8424 = jet_add_derivative(_5378, _5381, intervalFailed);
                Interval _5362 = _8411;
                Interval _5363 = Interval{ as_type<float>(as_type<uint>(_8418.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8418.lo) ^ 2147483648u) };
                Interval _8450 = iadd(_5362, _5363, intervalFailed);
                Interval _5364 = _8414;
                Interval _5365 = Interval{ as_type<float>(as_type<uint>(_8421.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8421.lo) ^ 2147483648u) };
                Interval _8452 = jet_add_derivative(_5364, _5365, intervalFailed);
                Interval _5366 = _8417;
                Interval _5367 = Interval{ as_type<float>(as_type<uint>(_8424.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8424.lo) ^ 2147483648u) };
                Interval _8454 = jet_add_derivative(_5366, _5367, intervalFailed);
                bool _8461;
                if (_8362.lo <= 0.0)
                {
                    _8461 = _8362.hi >= 0.0;
                }
                else
                {
                    _8461 = false;
                }
                float _8468;
                if (_8461)
                {
                    _8468 = 0.0;
                }
                else
                {
                    _8468 = precise::min(abs(_8362.lo), abs(_8362.hi));
                }
                float _8471 = precise::max(abs(_8362.lo), abs(_8362.hi));
                float _5352 = spvFMul(_8468, _8468);
                float _8472 = interval_down(_5352, intervalFailed);
                float _5353 = spvFMul(_8471, _8471);
                float _8474 = interval_up(_5353, intervalFailed);
                Interval _5354 = Interval{ 2.0, 2.0 };
                Interval _5355 = _8362;
                Interval _8475 = imul(_5354, _5355, intervalFailed, optical_product_upper);
                Interval _5356 = _8475;
                Interval _5357 = _8364;
                Interval _8476 = jet_mul_derivative(_5356, _5357, intervalFailed, optical_product_upper);
                Interval _5358 = Interval{ 2.0, 2.0 };
                Interval _5359 = _8362;
                Interval _8477 = imul(_5358, _5359, intervalFailed, optical_product_upper);
                Interval _5360 = _8477;
                Interval _5361 = _8366;
                Interval _8478 = jet_mul_derivative(_5360, _5361, intervalFailed, optical_product_upper);
                bool _8485;
                if (_8406.lo <= 0.0)
                {
                    _8485 = _8406.hi >= 0.0;
                }
                else
                {
                    _8485 = false;
                }
                float _8492;
                if (_8485)
                {
                    _8492 = 0.0;
                }
                else
                {
                    _8492 = precise::min(abs(_8406.lo), abs(_8406.hi));
                }
                float _8495 = precise::max(abs(_8406.lo), abs(_8406.hi));
                float _5342 = spvFMul(_8492, _8492);
                float _8496 = interval_down(_5342, intervalFailed);
                float _5343 = spvFMul(_8495, _8495);
                float _8498 = interval_up(_5343, intervalFailed);
                Interval _5344 = Interval{ 2.0, 2.0 };
                Interval _5345 = _8406;
                Interval _8499 = imul(_5344, _5345, intervalFailed, optical_product_upper);
                Interval _5346 = _8499;
                Interval _5347 = _8408;
                Interval _8500 = jet_mul_derivative(_5346, _5347, intervalFailed, optical_product_upper);
                Interval _5348 = Interval{ 2.0, 2.0 };
                Interval _5349 = _8406;
                Interval _8501 = imul(_5348, _5349, intervalFailed, optical_product_upper);
                Interval _5350 = _8501;
                Interval _5351 = _8410;
                Interval _8502 = jet_mul_derivative(_5350, _5351, intervalFailed, optical_product_upper);
                Interval _5336 = Interval{ precise::max(0.0, _8472), _8474 };
                Interval _5337 = Interval{ precise::max(0.0, _8496), _8498 };
                Interval _8505 = iadd(_5336, _5337, intervalFailed);
                Interval _5338 = _8476;
                Interval _5339 = _8500;
                Interval _8506 = jet_add_derivative(_5338, _5339, intervalFailed);
                Interval _5340 = _8478;
                Interval _5341 = _8502;
                Interval _8507 = jet_add_derivative(_5340, _5341, intervalFailed);
                bool _8514;
                if (_8450.lo <= 0.0)
                {
                    _8514 = _8450.hi >= 0.0;
                }
                else
                {
                    _8514 = false;
                }
                float _8521;
                if (_8514)
                {
                    _8521 = 0.0;
                }
                else
                {
                    _8521 = precise::min(abs(_8450.lo), abs(_8450.hi));
                }
                float _8524 = precise::max(abs(_8450.lo), abs(_8450.hi));
                float _5326 = spvFMul(_8521, _8521);
                float _8525 = interval_down(_5326, intervalFailed);
                float _5327 = spvFMul(_8524, _8524);
                float _8527 = interval_up(_5327, intervalFailed);
                Interval _5328 = Interval{ 2.0, 2.0 };
                Interval _5329 = _8450;
                Interval _8528 = imul(_5328, _5329, intervalFailed, optical_product_upper);
                Interval _5330 = _8528;
                Interval _5331 = _8452;
                Interval _8529 = jet_mul_derivative(_5330, _5331, intervalFailed, optical_product_upper);
                Interval _5332 = Interval{ 2.0, 2.0 };
                Interval _5333 = _8450;
                Interval _8530 = imul(_5332, _5333, intervalFailed, optical_product_upper);
                Interval _5334 = _8530;
                Interval _5335 = _8454;
                Interval _8531 = jet_mul_derivative(_5334, _5335, intervalFailed, optical_product_upper);
                Interval _5320 = _8505;
                Interval _5321 = Interval{ precise::max(0.0, _8525), _8527 };
                Interval _8533 = iadd(_5320, _5321, intervalFailed);
                Interval _5322 = _8506;
                Interval _5323 = _8529;
                Interval _8534 = jet_add_derivative(_5322, _5323, intervalFailed);
                Interval _5324 = _8507;
                Interval _5325 = _8531;
                Interval _8535 = jet_add_derivative(_5324, _5325, intervalFailed);
                Interval _5311 = _8533;
                Interval _8537 = isqrt(_5311, intervalFailed);
                bool _8545;
                if (!intervalFailed)
                {
                    _8545 = intervalFailed;
                }
                else
                {
                    _8545 = false;
                }
                bool _8550;
                if (_8545)
                {
                    _8550 = jetFailureSite == 0u;
                }
                else
                {
                    _8550 = false;
                }
                if (_8550)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_8533.lo, _8533.hi, 0.0, 0.0);
                }
                if (_8537.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5312 = Interval{ 2.0, 2.0 };
                Interval _5313 = _8537;
                Interval _8557 = imul(_5312, _5313, intervalFailed, optical_product_upper);
                Interval _5314 = Interval{ 1.0, 1.0 };
                Interval _5315 = _8557;
                Interval _8559 = idiv(_5314, _5315, intervalFailed, interval_divide_upper);
                bool _8566;
                if (!intervalFailed)
                {
                    _8566 = intervalFailed;
                }
                else
                {
                    _8566 = false;
                }
                bool _8571;
                if (_8566)
                {
                    _8571 = jetFailureSite == 0u;
                }
                else
                {
                    _8571 = false;
                }
                if (_8571)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _8557.lo, _8557.hi);
                }
                Interval _5316 = _8559;
                Interval _5317 = _8534;
                Interval _8575 = jet_mul_derivative(_5316, _5317, intervalFailed, optical_product_upper);
                Interval _5318 = _8559;
                Interval _5319 = _8535;
                Interval _8576 = jet_mul_derivative(_5318, _5319, intervalFailed, optical_product_upper);
                Interval _5297 = Interval{ 1.0, 1.0 };
                Interval _5298 = _8537;
                Interval _8578 = idiv(_5297, _5298, intervalFailed, interval_divide_upper);
                bool _8585;
                if (!intervalFailed)
                {
                    _8585 = intervalFailed;
                }
                else
                {
                    _8585 = false;
                }
                bool _8590;
                if (_8585)
                {
                    _8590 = jetFailureSite == 0u;
                }
                else
                {
                    _8590 = false;
                }
                if (_8590)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _8537.lo, _8537.hi);
                }
                Interval _5299 = Interval{ 0.0, 0.0 };
                Interval _5300 = _8578;
                Interval _5301 = _8575;
                Interval _8594 = jet_mul_derivative(_5300, _5301, intervalFailed, optical_product_upper);
                Interval _5302 = Interval{ as_type<float>(as_type<uint>(_8594.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8594.lo) ^ 2147483648u) };
                Interval _8604 = jet_add_derivative(_5299, _5302, intervalFailed);
                Interval _5303 = _8604;
                Interval _5304 = _8537;
                Interval _8605 = jet_div_derivative(_5303, _5304, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5305 = Interval{ 0.0, 0.0 };
                Interval _5306 = _8578;
                Interval _5307 = _8576;
                Interval _8606 = jet_mul_derivative(_5306, _5307, intervalFailed, optical_product_upper);
                Interval _5308 = Interval{ as_type<float>(as_type<uint>(_8606.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8606.lo) ^ 2147483648u) };
                Interval _8616 = jet_add_derivative(_5305, _5308, intervalFailed);
                Interval _5309 = _8616;
                Interval _5310 = _8537;
                Interval _8617 = jet_div_derivative(_5309, _5310, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5283 = _8362;
                Interval _5284 = _8578;
                Interval _8618 = imul(_5283, _5284, intervalFailed, optical_product_upper);
                Interval _5285 = _8364;
                Interval _5286 = _8578;
                Interval _8619 = jet_mul_derivative(_5285, _5286, intervalFailed, optical_product_upper);
                Interval _5287 = _8619;
                Interval _5288 = _8362;
                Interval _5289 = _8605;
                Interval _8620 = jet_mul_derivative(_5288, _5289, intervalFailed, optical_product_upper);
                Interval _5290 = _8620;
                Interval _8621 = jet_add_derivative(_5287, _5290, intervalFailed);
                Interval _5291 = _8366;
                Interval _5292 = _8578;
                Interval _8622 = jet_mul_derivative(_5291, _5292, intervalFailed, optical_product_upper);
                Interval _5293 = _8622;
                Interval _5294 = _8362;
                Interval _5295 = _8617;
                Interval _8623 = jet_mul_derivative(_5294, _5295, intervalFailed, optical_product_upper);
                Interval _5296 = _8623;
                Interval _8624 = jet_add_derivative(_5293, _5296, intervalFailed);
                Interval _5269 = _8406;
                Interval _5270 = _8578;
                Interval _8625 = imul(_5269, _5270, intervalFailed, optical_product_upper);
                Interval _5271 = _8408;
                Interval _5272 = _8578;
                Interval _8626 = jet_mul_derivative(_5271, _5272, intervalFailed, optical_product_upper);
                Interval _5273 = _8626;
                Interval _5274 = _8406;
                Interval _5275 = _8605;
                Interval _8627 = jet_mul_derivative(_5274, _5275, intervalFailed, optical_product_upper);
                Interval _5276 = _8627;
                Interval _8628 = jet_add_derivative(_5273, _5276, intervalFailed);
                Interval _5277 = _8410;
                Interval _5278 = _8578;
                Interval _8629 = jet_mul_derivative(_5277, _5278, intervalFailed, optical_product_upper);
                Interval _5279 = _8629;
                Interval _5280 = _8406;
                Interval _5281 = _8617;
                Interval _8630 = jet_mul_derivative(_5280, _5281, intervalFailed, optical_product_upper);
                Interval _5282 = _8630;
                Interval _8631 = jet_add_derivative(_5279, _5282, intervalFailed);
                Interval _5255 = _8450;
                Interval _5256 = _8578;
                Interval _8632 = imul(_5255, _5256, intervalFailed, optical_product_upper);
                Interval _5257 = _8452;
                Interval _5258 = _8578;
                Interval _8633 = jet_mul_derivative(_5257, _5258, intervalFailed, optical_product_upper);
                Interval _5259 = _8633;
                Interval _5260 = _8450;
                Interval _5261 = _8605;
                Interval _8634 = jet_mul_derivative(_5260, _5261, intervalFailed, optical_product_upper);
                Interval _5262 = _8634;
                Interval _8635 = jet_add_derivative(_5259, _5262, intervalFailed);
                Interval _5263 = _8454;
                Interval _5264 = _8578;
                Interval _8636 = jet_mul_derivative(_5263, _5264, intervalFailed, optical_product_upper);
                Interval _5265 = _8636;
                Interval _5266 = _8450;
                Interval _5267 = _8617;
                Interval _8637 = jet_mul_derivative(_5266, _5267, intervalFailed, optical_product_upper);
                Interval _5268 = _8637;
                Interval _8638 = jet_add_derivative(_5265, _5268, intervalFailed);
                _8994 = _8618.lo;
                _8995 = _8618.hi;
                _8996 = _8621.lo;
                _8997 = _8621.hi;
                _8998 = _8624.lo;
                _8999 = _8624.hi;
                _9000 = _8625.lo;
                _9001 = _8625.hi;
                _9002 = _8628.lo;
                _9003 = _8628.hi;
                _9004 = _8631.lo;
                _9005 = _8631.hi;
                _9006 = _8632.lo;
                _9007 = _8632.hi;
                _9008 = _8635.lo;
                _9009 = _8635.hi;
                _9010 = _8638.lo;
                _9011 = _8638.hi;
            }
            else
            {
                float _8976;
                float _8977;
                float _8978;
                float _8979;
                float _8980;
                float _8981;
                float _8982;
                float _8983;
                float _8984;
                float _8985;
                float _8986;
                float _8987;
                float _8988;
                float _8989;
                float _8990;
                float _8991;
                float _8992;
                float _8993;
                float _5253 = 0.949999988079071044921875;
                __attribute__((unused)) float _8657 = interval_down(_5253, intervalFailed);
                float _5254 = 0.949999988079071044921875;
                float _8658 = interval_up(_5254, intervalFailed);
                if (_8316 > _8658)
                {
                    Interval _5239 = _8262;
                    Interval _5240 = Interval{ 0.0, 0.0 };
                    Interval _8660 = imul(_5239, _5240, intervalFailed, optical_product_upper);
                    Interval _5241 = _8265;
                    Interval _5242 = Interval{ 0.0, 0.0 };
                    Interval _8661 = jet_mul_derivative(_5241, _5242, intervalFailed, optical_product_upper);
                    Interval _5243 = _8661;
                    Interval _5244 = _8262;
                    Interval _5245 = Interval{ 0.0, 0.0 };
                    Interval _8662 = jet_mul_derivative(_5244, _5245, intervalFailed, optical_product_upper);
                    Interval _5246 = _8662;
                    Interval _8663 = jet_add_derivative(_5243, _5246, intervalFailed);
                    Interval _5247 = _8268;
                    Interval _5248 = Interval{ 0.0, 0.0 };
                    Interval _8664 = jet_mul_derivative(_5247, _5248, intervalFailed, optical_product_upper);
                    Interval _5249 = _8664;
                    Interval _5250 = _8262;
                    Interval _5251 = Interval{ 0.0, 0.0 };
                    Interval _8665 = jet_mul_derivative(_5250, _5251, intervalFailed, optical_product_upper);
                    Interval _5252 = _8665;
                    Interval _8666 = jet_add_derivative(_5249, _5252, intervalFailed);
                    Interval _5225 = _8271;
                    Interval _5226 = Interval{ 0.0, 0.0 };
                    Interval _8667 = imul(_5225, _5226, intervalFailed, optical_product_upper);
                    Interval _5227 = _8274;
                    Interval _5228 = Interval{ 0.0, 0.0 };
                    Interval _8668 = jet_mul_derivative(_5227, _5228, intervalFailed, optical_product_upper);
                    Interval _5229 = _8668;
                    Interval _5230 = _8271;
                    Interval _5231 = Interval{ 0.0, 0.0 };
                    Interval _8669 = jet_mul_derivative(_5230, _5231, intervalFailed, optical_product_upper);
                    Interval _5232 = _8669;
                    Interval _8670 = jet_add_derivative(_5229, _5232, intervalFailed);
                    Interval _5233 = _8277;
                    Interval _5234 = Interval{ 0.0, 0.0 };
                    Interval _8671 = jet_mul_derivative(_5233, _5234, intervalFailed, optical_product_upper);
                    Interval _5235 = _8671;
                    Interval _5236 = _8271;
                    Interval _5237 = Interval{ 0.0, 0.0 };
                    Interval _8672 = jet_mul_derivative(_5236, _5237, intervalFailed, optical_product_upper);
                    Interval _5238 = _8672;
                    Interval _8673 = jet_add_derivative(_5235, _5238, intervalFailed);
                    Interval _5219 = _8660;
                    Interval _5220 = Interval{ as_type<float>(as_type<uint>(_8667.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8667.lo) ^ 2147483648u) };
                    Interval _8699 = iadd(_5219, _5220, intervalFailed);
                    Interval _5221 = _8663;
                    Interval _5222 = Interval{ as_type<float>(as_type<uint>(_8670.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8670.lo) ^ 2147483648u) };
                    Interval _8701 = jet_add_derivative(_5221, _5222, intervalFailed);
                    Interval _5223 = _8666;
                    Interval _5224 = Interval{ as_type<float>(as_type<uint>(_8673.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8673.lo) ^ 2147483648u) };
                    Interval _8703 = jet_add_derivative(_5223, _5224, intervalFailed);
                    Interval _5205 = _8271;
                    Interval _5206 = Interval{ 1.0, 1.0 };
                    Interval _8704 = imul(_5205, _5206, intervalFailed, optical_product_upper);
                    Interval _5207 = _8274;
                    Interval _5208 = Interval{ 1.0, 1.0 };
                    Interval _8705 = jet_mul_derivative(_5207, _5208, intervalFailed, optical_product_upper);
                    Interval _5209 = _8705;
                    Interval _5210 = _8271;
                    Interval _5211 = Interval{ 0.0, 0.0 };
                    Interval _8706 = jet_mul_derivative(_5210, _5211, intervalFailed, optical_product_upper);
                    Interval _5212 = _8706;
                    Interval _8707 = jet_add_derivative(_5209, _5212, intervalFailed);
                    Interval _5213 = _8277;
                    Interval _5214 = Interval{ 1.0, 1.0 };
                    Interval _8708 = jet_mul_derivative(_5213, _5214, intervalFailed, optical_product_upper);
                    Interval _5215 = _8708;
                    Interval _5216 = _8271;
                    Interval _5217 = Interval{ 0.0, 0.0 };
                    Interval _8709 = jet_mul_derivative(_5216, _5217, intervalFailed, optical_product_upper);
                    Interval _5218 = _8709;
                    Interval _8710 = jet_add_derivative(_5215, _5218, intervalFailed);
                    Interval _5191 = _8253;
                    Interval _5192 = Interval{ 0.0, 0.0 };
                    Interval _8711 = imul(_5191, _5192, intervalFailed, optical_product_upper);
                    Interval _5193 = _8256;
                    Interval _5194 = Interval{ 0.0, 0.0 };
                    Interval _8712 = jet_mul_derivative(_5193, _5194, intervalFailed, optical_product_upper);
                    Interval _5195 = _8712;
                    Interval _5196 = _8253;
                    Interval _5197 = Interval{ 0.0, 0.0 };
                    Interval _8713 = jet_mul_derivative(_5196, _5197, intervalFailed, optical_product_upper);
                    Interval _5198 = _8713;
                    Interval _8714 = jet_add_derivative(_5195, _5198, intervalFailed);
                    Interval _5199 = _8259;
                    Interval _5200 = Interval{ 0.0, 0.0 };
                    Interval _8715 = jet_mul_derivative(_5199, _5200, intervalFailed, optical_product_upper);
                    Interval _5201 = _8715;
                    Interval _5202 = _8253;
                    Interval _5203 = Interval{ 0.0, 0.0 };
                    Interval _8716 = jet_mul_derivative(_5202, _5203, intervalFailed, optical_product_upper);
                    Interval _5204 = _8716;
                    Interval _8717 = jet_add_derivative(_5201, _5204, intervalFailed);
                    Interval _5185 = _8704;
                    Interval _5186 = Interval{ as_type<float>(as_type<uint>(_8711.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8711.lo) ^ 2147483648u) };
                    Interval _8743 = iadd(_5185, _5186, intervalFailed);
                    Interval _5187 = _8707;
                    Interval _5188 = Interval{ as_type<float>(as_type<uint>(_8714.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8714.lo) ^ 2147483648u) };
                    Interval _8745 = jet_add_derivative(_5187, _5188, intervalFailed);
                    Interval _5189 = _8710;
                    Interval _5190 = Interval{ as_type<float>(as_type<uint>(_8717.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8717.lo) ^ 2147483648u) };
                    Interval _8747 = jet_add_derivative(_5189, _5190, intervalFailed);
                    Interval _5171 = _8253;
                    Interval _5172 = Interval{ 0.0, 0.0 };
                    Interval _8748 = imul(_5171, _5172, intervalFailed, optical_product_upper);
                    Interval _5173 = _8256;
                    Interval _5174 = Interval{ 0.0, 0.0 };
                    Interval _8749 = jet_mul_derivative(_5173, _5174, intervalFailed, optical_product_upper);
                    Interval _5175 = _8749;
                    Interval _5176 = _8253;
                    Interval _5177 = Interval{ 0.0, 0.0 };
                    Interval _8750 = jet_mul_derivative(_5176, _5177, intervalFailed, optical_product_upper);
                    Interval _5178 = _8750;
                    Interval _8751 = jet_add_derivative(_5175, _5178, intervalFailed);
                    Interval _5179 = _8259;
                    Interval _5180 = Interval{ 0.0, 0.0 };
                    Interval _8752 = jet_mul_derivative(_5179, _5180, intervalFailed, optical_product_upper);
                    Interval _5181 = _8752;
                    Interval _5182 = _8253;
                    Interval _5183 = Interval{ 0.0, 0.0 };
                    Interval _8753 = jet_mul_derivative(_5182, _5183, intervalFailed, optical_product_upper);
                    Interval _5184 = _8753;
                    Interval _8754 = jet_add_derivative(_5181, _5184, intervalFailed);
                    Interval _5157 = _8262;
                    Interval _5158 = Interval{ 1.0, 1.0 };
                    Interval _8755 = imul(_5157, _5158, intervalFailed, optical_product_upper);
                    Interval _5159 = _8265;
                    Interval _5160 = Interval{ 1.0, 1.0 };
                    Interval _8756 = jet_mul_derivative(_5159, _5160, intervalFailed, optical_product_upper);
                    Interval _5161 = _8756;
                    Interval _5162 = _8262;
                    Interval _5163 = Interval{ 0.0, 0.0 };
                    Interval _8757 = jet_mul_derivative(_5162, _5163, intervalFailed, optical_product_upper);
                    Interval _5164 = _8757;
                    Interval _8758 = jet_add_derivative(_5161, _5164, intervalFailed);
                    Interval _5165 = _8268;
                    Interval _5166 = Interval{ 1.0, 1.0 };
                    Interval _8759 = jet_mul_derivative(_5165, _5166, intervalFailed, optical_product_upper);
                    Interval _5167 = _8759;
                    Interval _5168 = _8262;
                    Interval _5169 = Interval{ 0.0, 0.0 };
                    Interval _8760 = jet_mul_derivative(_5168, _5169, intervalFailed, optical_product_upper);
                    Interval _5170 = _8760;
                    Interval _8761 = jet_add_derivative(_5167, _5170, intervalFailed);
                    Interval _5151 = _8748;
                    Interval _5152 = Interval{ as_type<float>(as_type<uint>(_8755.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8755.lo) ^ 2147483648u) };
                    Interval _8787 = iadd(_5151, _5152, intervalFailed);
                    Interval _5153 = _8751;
                    Interval _5154 = Interval{ as_type<float>(as_type<uint>(_8758.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8758.lo) ^ 2147483648u) };
                    Interval _8789 = jet_add_derivative(_5153, _5154, intervalFailed);
                    Interval _5155 = _8754;
                    Interval _5156 = Interval{ as_type<float>(as_type<uint>(_8761.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8761.lo) ^ 2147483648u) };
                    Interval _8791 = jet_add_derivative(_5155, _5156, intervalFailed);
                    bool _8798;
                    if (_8699.lo <= 0.0)
                    {
                        _8798 = _8699.hi >= 0.0;
                    }
                    else
                    {
                        _8798 = false;
                    }
                    float _8805;
                    if (_8798)
                    {
                        _8805 = 0.0;
                    }
                    else
                    {
                        _8805 = precise::min(abs(_8699.lo), abs(_8699.hi));
                    }
                    float _8808 = precise::max(abs(_8699.lo), abs(_8699.hi));
                    float _5141 = spvFMul(_8805, _8805);
                    float _8809 = interval_down(_5141, intervalFailed);
                    float _5142 = spvFMul(_8808, _8808);
                    float _8811 = interval_up(_5142, intervalFailed);
                    Interval _5143 = Interval{ 2.0, 2.0 };
                    Interval _5144 = _8699;
                    Interval _8812 = imul(_5143, _5144, intervalFailed, optical_product_upper);
                    Interval _5145 = _8812;
                    Interval _5146 = _8701;
                    Interval _8813 = jet_mul_derivative(_5145, _5146, intervalFailed, optical_product_upper);
                    Interval _5147 = Interval{ 2.0, 2.0 };
                    Interval _5148 = _8699;
                    Interval _8814 = imul(_5147, _5148, intervalFailed, optical_product_upper);
                    Interval _5149 = _8814;
                    Interval _5150 = _8703;
                    Interval _8815 = jet_mul_derivative(_5149, _5150, intervalFailed, optical_product_upper);
                    bool _8822;
                    if (_8743.lo <= 0.0)
                    {
                        _8822 = _8743.hi >= 0.0;
                    }
                    else
                    {
                        _8822 = false;
                    }
                    float _8829;
                    if (_8822)
                    {
                        _8829 = 0.0;
                    }
                    else
                    {
                        _8829 = precise::min(abs(_8743.lo), abs(_8743.hi));
                    }
                    float _8832 = precise::max(abs(_8743.lo), abs(_8743.hi));
                    float _5131 = spvFMul(_8829, _8829);
                    float _8833 = interval_down(_5131, intervalFailed);
                    float _5132 = spvFMul(_8832, _8832);
                    float _8835 = interval_up(_5132, intervalFailed);
                    Interval _5133 = Interval{ 2.0, 2.0 };
                    Interval _5134 = _8743;
                    Interval _8836 = imul(_5133, _5134, intervalFailed, optical_product_upper);
                    Interval _5135 = _8836;
                    Interval _5136 = _8745;
                    Interval _8837 = jet_mul_derivative(_5135, _5136, intervalFailed, optical_product_upper);
                    Interval _5137 = Interval{ 2.0, 2.0 };
                    Interval _5138 = _8743;
                    Interval _8838 = imul(_5137, _5138, intervalFailed, optical_product_upper);
                    Interval _5139 = _8838;
                    Interval _5140 = _8747;
                    Interval _8839 = jet_mul_derivative(_5139, _5140, intervalFailed, optical_product_upper);
                    Interval _5125 = Interval{ precise::max(0.0, _8809), _8811 };
                    Interval _5126 = Interval{ precise::max(0.0, _8833), _8835 };
                    Interval _8842 = iadd(_5125, _5126, intervalFailed);
                    Interval _5127 = _8813;
                    Interval _5128 = _8837;
                    Interval _8843 = jet_add_derivative(_5127, _5128, intervalFailed);
                    Interval _5129 = _8815;
                    Interval _5130 = _8839;
                    Interval _8844 = jet_add_derivative(_5129, _5130, intervalFailed);
                    bool _8851;
                    if (_8787.lo <= 0.0)
                    {
                        _8851 = _8787.hi >= 0.0;
                    }
                    else
                    {
                        _8851 = false;
                    }
                    float _8858;
                    if (_8851)
                    {
                        _8858 = 0.0;
                    }
                    else
                    {
                        _8858 = precise::min(abs(_8787.lo), abs(_8787.hi));
                    }
                    float _8861 = precise::max(abs(_8787.lo), abs(_8787.hi));
                    float _5115 = spvFMul(_8858, _8858);
                    float _8862 = interval_down(_5115, intervalFailed);
                    float _5116 = spvFMul(_8861, _8861);
                    float _8864 = interval_up(_5116, intervalFailed);
                    Interval _5117 = Interval{ 2.0, 2.0 };
                    Interval _5118 = _8787;
                    Interval _8865 = imul(_5117, _5118, intervalFailed, optical_product_upper);
                    Interval _5119 = _8865;
                    Interval _5120 = _8789;
                    Interval _8866 = jet_mul_derivative(_5119, _5120, intervalFailed, optical_product_upper);
                    Interval _5121 = Interval{ 2.0, 2.0 };
                    Interval _5122 = _8787;
                    Interval _8867 = imul(_5121, _5122, intervalFailed, optical_product_upper);
                    Interval _5123 = _8867;
                    Interval _5124 = _8791;
                    Interval _8868 = jet_mul_derivative(_5123, _5124, intervalFailed, optical_product_upper);
                    Interval _5109 = _8842;
                    Interval _5110 = Interval{ precise::max(0.0, _8862), _8864 };
                    Interval _8870 = iadd(_5109, _5110, intervalFailed);
                    Interval _5111 = _8843;
                    Interval _5112 = _8866;
                    Interval _8871 = jet_add_derivative(_5111, _5112, intervalFailed);
                    Interval _5113 = _8844;
                    Interval _5114 = _8868;
                    Interval _8872 = jet_add_derivative(_5113, _5114, intervalFailed);
                    Interval _5100 = _8870;
                    Interval _8874 = isqrt(_5100, intervalFailed);
                    bool _8882;
                    if (!intervalFailed)
                    {
                        _8882 = intervalFailed;
                    }
                    else
                    {
                        _8882 = false;
                    }
                    bool _8887;
                    if (_8882)
                    {
                        _8887 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8887 = false;
                    }
                    if (_8887)
                    {
                        jetFailureSite = 3u;
                        jetFailureArguments = float4(_8870.lo, _8870.hi, 0.0, 0.0);
                    }
                    if (_8874.lo <= 0.0)
                    {
                        jetBranchKnown = false;
                    }
                    Interval _5101 = Interval{ 2.0, 2.0 };
                    Interval _5102 = _8874;
                    Interval _8894 = imul(_5101, _5102, intervalFailed, optical_product_upper);
                    Interval _5103 = Interval{ 1.0, 1.0 };
                    Interval _5104 = _8894;
                    Interval _8896 = idiv(_5103, _5104, intervalFailed, interval_divide_upper);
                    bool _8903;
                    if (!intervalFailed)
                    {
                        _8903 = intervalFailed;
                    }
                    else
                    {
                        _8903 = false;
                    }
                    bool _8908;
                    if (_8903)
                    {
                        _8908 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8908 = false;
                    }
                    if (_8908)
                    {
                        jetFailureSite = 4u;
                        jetFailureArguments = float4(1.0, 1.0, _8894.lo, _8894.hi);
                    }
                    Interval _5105 = _8896;
                    Interval _5106 = _8871;
                    Interval _8912 = jet_mul_derivative(_5105, _5106, intervalFailed, optical_product_upper);
                    Interval _5107 = _8896;
                    Interval _5108 = _8872;
                    Interval _8913 = jet_mul_derivative(_5107, _5108, intervalFailed, optical_product_upper);
                    Interval _5086 = Interval{ 1.0, 1.0 };
                    Interval _5087 = _8874;
                    Interval _8915 = idiv(_5086, _5087, intervalFailed, interval_divide_upper);
                    bool _8922;
                    if (!intervalFailed)
                    {
                        _8922 = intervalFailed;
                    }
                    else
                    {
                        _8922 = false;
                    }
                    bool _8927;
                    if (_8922)
                    {
                        _8927 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8927 = false;
                    }
                    if (_8927)
                    {
                        jetFailureSite = 1u;
                        jetFailureArguments = float4(1.0, 1.0, _8874.lo, _8874.hi);
                    }
                    Interval _5088 = Interval{ 0.0, 0.0 };
                    Interval _5089 = _8915;
                    Interval _5090 = _8912;
                    Interval _8931 = jet_mul_derivative(_5089, _5090, intervalFailed, optical_product_upper);
                    Interval _5091 = Interval{ as_type<float>(as_type<uint>(_8931.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8931.lo) ^ 2147483648u) };
                    Interval _8941 = jet_add_derivative(_5088, _5091, intervalFailed);
                    Interval _5092 = _8941;
                    Interval _5093 = _8874;
                    Interval _8942 = jet_div_derivative(_5092, _5093, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _5094 = Interval{ 0.0, 0.0 };
                    Interval _5095 = _8915;
                    Interval _5096 = _8913;
                    Interval _8943 = jet_mul_derivative(_5095, _5096, intervalFailed, optical_product_upper);
                    Interval _5097 = Interval{ as_type<float>(as_type<uint>(_8943.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8943.lo) ^ 2147483648u) };
                    Interval _8953 = jet_add_derivative(_5094, _5097, intervalFailed);
                    Interval _5098 = _8953;
                    Interval _5099 = _8874;
                    Interval _8954 = jet_div_derivative(_5098, _5099, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _5072 = _8699;
                    Interval _5073 = _8915;
                    Interval _8955 = imul(_5072, _5073, intervalFailed, optical_product_upper);
                    Interval _5074 = _8701;
                    Interval _5075 = _8915;
                    Interval _8956 = jet_mul_derivative(_5074, _5075, intervalFailed, optical_product_upper);
                    Interval _5076 = _8956;
                    Interval _5077 = _8699;
                    Interval _5078 = _8942;
                    Interval _8957 = jet_mul_derivative(_5077, _5078, intervalFailed, optical_product_upper);
                    Interval _5079 = _8957;
                    Interval _8958 = jet_add_derivative(_5076, _5079, intervalFailed);
                    Interval _5080 = _8703;
                    Interval _5081 = _8915;
                    Interval _8959 = jet_mul_derivative(_5080, _5081, intervalFailed, optical_product_upper);
                    Interval _5082 = _8959;
                    Interval _5083 = _8699;
                    Interval _5084 = _8954;
                    Interval _8960 = jet_mul_derivative(_5083, _5084, intervalFailed, optical_product_upper);
                    Interval _5085 = _8960;
                    Interval _8961 = jet_add_derivative(_5082, _5085, intervalFailed);
                    Interval _5058 = _8743;
                    Interval _5059 = _8915;
                    Interval _8962 = imul(_5058, _5059, intervalFailed, optical_product_upper);
                    Interval _5060 = _8745;
                    Interval _5061 = _8915;
                    Interval _8963 = jet_mul_derivative(_5060, _5061, intervalFailed, optical_product_upper);
                    Interval _5062 = _8963;
                    Interval _5063 = _8743;
                    Interval _5064 = _8942;
                    Interval _8964 = jet_mul_derivative(_5063, _5064, intervalFailed, optical_product_upper);
                    Interval _5065 = _8964;
                    Interval _8965 = jet_add_derivative(_5062, _5065, intervalFailed);
                    Interval _5066 = _8747;
                    Interval _5067 = _8915;
                    Interval _8966 = jet_mul_derivative(_5066, _5067, intervalFailed, optical_product_upper);
                    Interval _5068 = _8966;
                    Interval _5069 = _8743;
                    Interval _5070 = _8954;
                    Interval _8967 = jet_mul_derivative(_5069, _5070, intervalFailed, optical_product_upper);
                    Interval _5071 = _8967;
                    Interval _8968 = jet_add_derivative(_5068, _5071, intervalFailed);
                    Interval _5044 = _8787;
                    Interval _5045 = _8915;
                    Interval _8969 = imul(_5044, _5045, intervalFailed, optical_product_upper);
                    Interval _5046 = _8789;
                    Interval _5047 = _8915;
                    Interval _8970 = jet_mul_derivative(_5046, _5047, intervalFailed, optical_product_upper);
                    Interval _5048 = _8970;
                    Interval _5049 = _8787;
                    Interval _5050 = _8942;
                    Interval _8971 = jet_mul_derivative(_5049, _5050, intervalFailed, optical_product_upper);
                    Interval _5051 = _8971;
                    Interval _8972 = jet_add_derivative(_5048, _5051, intervalFailed);
                    Interval _5052 = _8791;
                    Interval _5053 = _8915;
                    Interval _8973 = jet_mul_derivative(_5052, _5053, intervalFailed, optical_product_upper);
                    Interval _5054 = _8973;
                    Interval _5055 = _8787;
                    Interval _5056 = _8954;
                    Interval _8974 = jet_mul_derivative(_5055, _5056, intervalFailed, optical_product_upper);
                    Interval _5057 = _8974;
                    Interval _8975 = jet_add_derivative(_5054, _5057, intervalFailed);
                    _8976 = _8955.lo;
                    _8977 = _8955.hi;
                    _8978 = _8958.lo;
                    _8979 = _8958.hi;
                    _8980 = _8961.lo;
                    _8981 = _8961.hi;
                    _8982 = _8962.lo;
                    _8983 = _8962.hi;
                    _8984 = _8965.lo;
                    _8985 = _8965.hi;
                    _8986 = _8968.lo;
                    _8987 = _8968.hi;
                    _8988 = _8969.lo;
                    _8989 = _8969.hi;
                    _8990 = _8972.lo;
                    _8991 = _8972.hi;
                    _8992 = _8975.lo;
                    _8993 = _8975.hi;
                }
                else
                {
                    return false;
                }
                _8994 = _8976;
                _8995 = _8977;
                _8996 = _8978;
                _8997 = _8979;
                _8998 = _8980;
                _8999 = _8981;
                _9000 = _8982;
                _9001 = _8983;
                _9002 = _8984;
                _9003 = _8985;
                _9004 = _8986;
                _9005 = _8987;
                _9006 = _8988;
                _9007 = _8989;
                _9008 = _8990;
                _9009 = _8991;
                _9010 = _8992;
                _9011 = _8993;
            }
            uint _9015 = uint(receiver.settings.y);
            float _9019 = float(_2306[_9015].x);
            float _5042 = _9019;
            float _5043 = 1000.0;
            Interval _9021 = iratio(_5042, _5043, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _9026;
            if (!intervalFailed)
            {
                _9026 = intervalFailed;
            }
            else
            {
                _9026 = false;
            }
            bool _9031;
            if (_9026)
            {
                _9031 = jetFailureSite == 0u;
            }
            else
            {
                _9031 = false;
            }
            if (_9031)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_9019, _9019, 1000.0, 1000.0);
            }
            Interval _5028 = Interval{ _8994, _8995 };
            Interval _5029 = _9021;
            Interval _9036 = imul(_5028, _5029, intervalFailed, optical_product_upper);
            Interval _5030 = Interval{ _8996, _8997 };
            Interval _5031 = _9021;
            Interval _9038 = jet_mul_derivative(_5030, _5031, intervalFailed, optical_product_upper);
            Interval _5032 = _9038;
            Interval _5033 = Interval{ _8994, _8995 };
            Interval _5034 = Interval{ 0.0, 0.0 };
            Interval _9040 = jet_mul_derivative(_5033, _5034, intervalFailed, optical_product_upper);
            Interval _5035 = _9040;
            Interval _9041 = jet_add_derivative(_5032, _5035, intervalFailed);
            Interval _5036 = Interval{ _8998, _8999 };
            Interval _5037 = _9021;
            Interval _9043 = jet_mul_derivative(_5036, _5037, intervalFailed, optical_product_upper);
            Interval _5038 = _9043;
            Interval _5039 = Interval{ _8994, _8995 };
            Interval _5040 = Interval{ 0.0, 0.0 };
            Interval _9045 = jet_mul_derivative(_5039, _5040, intervalFailed, optical_product_upper);
            Interval _5041 = _9045;
            Interval _9046 = jet_add_derivative(_5038, _5041, intervalFailed);
            Interval _5014 = Interval{ _9000, _9001 };
            Interval _5015 = _9021;
            Interval _9048 = imul(_5014, _5015, intervalFailed, optical_product_upper);
            Interval _5016 = Interval{ _9002, _9003 };
            Interval _5017 = _9021;
            Interval _9050 = jet_mul_derivative(_5016, _5017, intervalFailed, optical_product_upper);
            Interval _5018 = _9050;
            Interval _5019 = Interval{ _9000, _9001 };
            Interval _5020 = Interval{ 0.0, 0.0 };
            Interval _9052 = jet_mul_derivative(_5019, _5020, intervalFailed, optical_product_upper);
            Interval _5021 = _9052;
            Interval _9053 = jet_add_derivative(_5018, _5021, intervalFailed);
            Interval _5022 = Interval{ _9004, _9005 };
            Interval _5023 = _9021;
            Interval _9055 = jet_mul_derivative(_5022, _5023, intervalFailed, optical_product_upper);
            Interval _5024 = _9055;
            Interval _5025 = Interval{ _9000, _9001 };
            Interval _5026 = Interval{ 0.0, 0.0 };
            Interval _9057 = jet_mul_derivative(_5025, _5026, intervalFailed, optical_product_upper);
            Interval _5027 = _9057;
            Interval _9058 = jet_add_derivative(_5024, _5027, intervalFailed);
            Interval _5000 = Interval{ _9006, _9007 };
            Interval _5001 = _9021;
            Interval _9060 = imul(_5000, _5001, intervalFailed, optical_product_upper);
            Interval _5002 = Interval{ _9008, _9009 };
            Interval _5003 = _9021;
            Interval _9062 = jet_mul_derivative(_5002, _5003, intervalFailed, optical_product_upper);
            Interval _5004 = _9062;
            Interval _5005 = Interval{ _9006, _9007 };
            Interval _5006 = Interval{ 0.0, 0.0 };
            Interval _9064 = jet_mul_derivative(_5005, _5006, intervalFailed, optical_product_upper);
            Interval _5007 = _9064;
            Interval _9065 = jet_add_derivative(_5004, _5007, intervalFailed);
            Interval _5008 = Interval{ _9010, _9011 };
            Interval _5009 = _9021;
            Interval _9067 = jet_mul_derivative(_5008, _5009, intervalFailed, optical_product_upper);
            Interval _5010 = _9067;
            Interval _5011 = Interval{ _9006, _9007 };
            Interval _5012 = Interval{ 0.0, 0.0 };
            Interval _9069 = jet_mul_derivative(_5011, _5012, intervalFailed, optical_product_upper);
            Interval _5013 = _9069;
            Interval _9070 = jet_add_derivative(_5010, _5013, intervalFailed);
            Interval _4986 = _8262;
            Interval _4987 = Interval{ _9006, _9007 };
            Interval _9072 = imul(_4986, _4987, intervalFailed, optical_product_upper);
            Interval _4988 = _8265;
            Interval _4989 = Interval{ _9006, _9007 };
            Interval _9074 = jet_mul_derivative(_4988, _4989, intervalFailed, optical_product_upper);
            Interval _4990 = _9074;
            Interval _4991 = _8262;
            Interval _4992 = Interval{ _9008, _9009 };
            Interval _9076 = jet_mul_derivative(_4991, _4992, intervalFailed, optical_product_upper);
            Interval _4993 = _9076;
            Interval _9077 = jet_add_derivative(_4990, _4993, intervalFailed);
            Interval _4994 = _8268;
            Interval _4995 = Interval{ _9006, _9007 };
            Interval _9079 = jet_mul_derivative(_4994, _4995, intervalFailed, optical_product_upper);
            Interval _4996 = _9079;
            Interval _4997 = _8262;
            Interval _4998 = Interval{ _9010, _9011 };
            Interval _9081 = jet_mul_derivative(_4997, _4998, intervalFailed, optical_product_upper);
            Interval _4999 = _9081;
            Interval _9082 = jet_add_derivative(_4996, _4999, intervalFailed);
            Interval _4972 = _8271;
            Interval _4973 = Interval{ _9000, _9001 };
            Interval _9084 = imul(_4972, _4973, intervalFailed, optical_product_upper);
            Interval _4974 = _8274;
            Interval _4975 = Interval{ _9000, _9001 };
            Interval _9086 = jet_mul_derivative(_4974, _4975, intervalFailed, optical_product_upper);
            Interval _4976 = _9086;
            Interval _4977 = _8271;
            Interval _4978 = Interval{ _9002, _9003 };
            Interval _9088 = jet_mul_derivative(_4977, _4978, intervalFailed, optical_product_upper);
            Interval _4979 = _9088;
            Interval _9089 = jet_add_derivative(_4976, _4979, intervalFailed);
            Interval _4980 = _8277;
            Interval _4981 = Interval{ _9000, _9001 };
            Interval _9091 = jet_mul_derivative(_4980, _4981, intervalFailed, optical_product_upper);
            Interval _4982 = _9091;
            Interval _4983 = _8271;
            Interval _4984 = Interval{ _9004, _9005 };
            Interval _9093 = jet_mul_derivative(_4983, _4984, intervalFailed, optical_product_upper);
            Interval _4985 = _9093;
            Interval _9094 = jet_add_derivative(_4982, _4985, intervalFailed);
            Interval _4966 = _9072;
            Interval _4967 = Interval{ as_type<float>(as_type<uint>(_9084.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9084.lo) ^ 2147483648u) };
            Interval _9120 = iadd(_4966, _4967, intervalFailed);
            Interval _4968 = _9077;
            Interval _4969 = Interval{ as_type<float>(as_type<uint>(_9089.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9089.lo) ^ 2147483648u) };
            Interval _9122 = jet_add_derivative(_4968, _4969, intervalFailed);
            Interval _4970 = _9082;
            Interval _4971 = Interval{ as_type<float>(as_type<uint>(_9094.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9094.lo) ^ 2147483648u) };
            Interval _9124 = jet_add_derivative(_4970, _4971, intervalFailed);
            Interval _4952 = _8271;
            Interval _4953 = Interval{ _8994, _8995 };
            Interval _9126 = imul(_4952, _4953, intervalFailed, optical_product_upper);
            Interval _4954 = _8274;
            Interval _4955 = Interval{ _8994, _8995 };
            Interval _9128 = jet_mul_derivative(_4954, _4955, intervalFailed, optical_product_upper);
            Interval _4956 = _9128;
            Interval _4957 = _8271;
            Interval _4958 = Interval{ _8996, _8997 };
            Interval _9130 = jet_mul_derivative(_4957, _4958, intervalFailed, optical_product_upper);
            Interval _4959 = _9130;
            Interval _9131 = jet_add_derivative(_4956, _4959, intervalFailed);
            Interval _4960 = _8277;
            Interval _4961 = Interval{ _8994, _8995 };
            Interval _9133 = jet_mul_derivative(_4960, _4961, intervalFailed, optical_product_upper);
            Interval _4962 = _9133;
            Interval _4963 = _8271;
            Interval _4964 = Interval{ _8998, _8999 };
            Interval _9135 = jet_mul_derivative(_4963, _4964, intervalFailed, optical_product_upper);
            Interval _4965 = _9135;
            Interval _9136 = jet_add_derivative(_4962, _4965, intervalFailed);
            Interval _4938 = _8253;
            Interval _4939 = Interval{ _9006, _9007 };
            Interval _9138 = imul(_4938, _4939, intervalFailed, optical_product_upper);
            Interval _4940 = _8256;
            Interval _4941 = Interval{ _9006, _9007 };
            Interval _9140 = jet_mul_derivative(_4940, _4941, intervalFailed, optical_product_upper);
            Interval _4942 = _9140;
            Interval _4943 = _8253;
            Interval _4944 = Interval{ _9008, _9009 };
            Interval _9142 = jet_mul_derivative(_4943, _4944, intervalFailed, optical_product_upper);
            Interval _4945 = _9142;
            Interval _9143 = jet_add_derivative(_4942, _4945, intervalFailed);
            Interval _4946 = _8259;
            Interval _4947 = Interval{ _9006, _9007 };
            Interval _9145 = jet_mul_derivative(_4946, _4947, intervalFailed, optical_product_upper);
            Interval _4948 = _9145;
            Interval _4949 = _8253;
            Interval _4950 = Interval{ _9010, _9011 };
            Interval _9147 = jet_mul_derivative(_4949, _4950, intervalFailed, optical_product_upper);
            Interval _4951 = _9147;
            Interval _9148 = jet_add_derivative(_4948, _4951, intervalFailed);
            Interval _4932 = _9126;
            Interval _4933 = Interval{ as_type<float>(as_type<uint>(_9138.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9138.lo) ^ 2147483648u) };
            Interval _9174 = iadd(_4932, _4933, intervalFailed);
            Interval _4934 = _9131;
            Interval _4935 = Interval{ as_type<float>(as_type<uint>(_9143.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9143.lo) ^ 2147483648u) };
            Interval _9176 = jet_add_derivative(_4934, _4935, intervalFailed);
            Interval _4936 = _9136;
            Interval _4937 = Interval{ as_type<float>(as_type<uint>(_9148.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9148.lo) ^ 2147483648u) };
            Interval _9178 = jet_add_derivative(_4936, _4937, intervalFailed);
            Interval _4918 = _8253;
            Interval _4919 = Interval{ _9000, _9001 };
            Interval _9180 = imul(_4918, _4919, intervalFailed, optical_product_upper);
            Interval _4920 = _8256;
            Interval _4921 = Interval{ _9000, _9001 };
            Interval _9182 = jet_mul_derivative(_4920, _4921, intervalFailed, optical_product_upper);
            Interval _4922 = _9182;
            Interval _4923 = _8253;
            Interval _4924 = Interval{ _9002, _9003 };
            Interval _9184 = jet_mul_derivative(_4923, _4924, intervalFailed, optical_product_upper);
            Interval _4925 = _9184;
            Interval _9185 = jet_add_derivative(_4922, _4925, intervalFailed);
            Interval _4926 = _8259;
            Interval _4927 = Interval{ _9000, _9001 };
            Interval _9187 = jet_mul_derivative(_4926, _4927, intervalFailed, optical_product_upper);
            Interval _4928 = _9187;
            Interval _4929 = _8253;
            Interval _4930 = Interval{ _9004, _9005 };
            Interval _9189 = jet_mul_derivative(_4929, _4930, intervalFailed, optical_product_upper);
            Interval _4931 = _9189;
            Interval _9190 = jet_add_derivative(_4928, _4931, intervalFailed);
            Interval _4904 = _8262;
            Interval _4905 = Interval{ _8994, _8995 };
            Interval _9192 = imul(_4904, _4905, intervalFailed, optical_product_upper);
            Interval _4906 = _8265;
            Interval _4907 = Interval{ _8994, _8995 };
            Interval _9194 = jet_mul_derivative(_4906, _4907, intervalFailed, optical_product_upper);
            Interval _4908 = _9194;
            Interval _4909 = _8262;
            Interval _4910 = Interval{ _8996, _8997 };
            Interval _9196 = jet_mul_derivative(_4909, _4910, intervalFailed, optical_product_upper);
            Interval _4911 = _9196;
            Interval _9197 = jet_add_derivative(_4908, _4911, intervalFailed);
            Interval _4912 = _8268;
            Interval _4913 = Interval{ _8994, _8995 };
            Interval _9199 = jet_mul_derivative(_4912, _4913, intervalFailed, optical_product_upper);
            Interval _4914 = _9199;
            Interval _4915 = _8262;
            Interval _4916 = Interval{ _8998, _8999 };
            Interval _9201 = jet_mul_derivative(_4915, _4916, intervalFailed, optical_product_upper);
            Interval _4917 = _9201;
            Interval _9202 = jet_add_derivative(_4914, _4917, intervalFailed);
            Interval _4898 = _9180;
            Interval _4899 = Interval{ as_type<float>(as_type<uint>(_9192.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9192.lo) ^ 2147483648u) };
            Interval _9228 = iadd(_4898, _4899, intervalFailed);
            Interval _4900 = _9185;
            Interval _4901 = Interval{ as_type<float>(as_type<uint>(_9197.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9197.lo) ^ 2147483648u) };
            Interval _9230 = jet_add_derivative(_4900, _4901, intervalFailed);
            Interval _4902 = _9190;
            Interval _4903 = Interval{ as_type<float>(as_type<uint>(_9202.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9202.lo) ^ 2147483648u) };
            Interval _9232 = jet_add_derivative(_4902, _4903, intervalFailed);
            float _9234 = float(_2306[_9015].y);
            float _4896 = _9234;
            float _4897 = 1000.0;
            Interval _9236 = iratio(_4896, _4897, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _9241;
            if (!intervalFailed)
            {
                _9241 = intervalFailed;
            }
            else
            {
                _9241 = false;
            }
            bool _9246;
            if (_9241)
            {
                _9246 = jetFailureSite == 0u;
            }
            else
            {
                _9246 = false;
            }
            if (_9246)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_9234, _9234, 1000.0, 1000.0);
            }
            Interval _4882 = _9120;
            Interval _4883 = _9236;
            Interval _9250 = imul(_4882, _4883, intervalFailed, optical_product_upper);
            Interval _4884 = _9122;
            Interval _4885 = _9236;
            Interval _9251 = jet_mul_derivative(_4884, _4885, intervalFailed, optical_product_upper);
            Interval _4886 = _9251;
            Interval _4887 = _9120;
            Interval _4888 = Interval{ 0.0, 0.0 };
            Interval _9252 = jet_mul_derivative(_4887, _4888, intervalFailed, optical_product_upper);
            Interval _4889 = _9252;
            Interval _9253 = jet_add_derivative(_4886, _4889, intervalFailed);
            Interval _4890 = _9124;
            Interval _4891 = _9236;
            Interval _9254 = jet_mul_derivative(_4890, _4891, intervalFailed, optical_product_upper);
            Interval _4892 = _9254;
            Interval _4893 = _9120;
            Interval _4894 = Interval{ 0.0, 0.0 };
            Interval _9255 = jet_mul_derivative(_4893, _4894, intervalFailed, optical_product_upper);
            Interval _4895 = _9255;
            Interval _9256 = jet_add_derivative(_4892, _4895, intervalFailed);
            Interval _4868 = _9174;
            Interval _4869 = _9236;
            Interval _9257 = imul(_4868, _4869, intervalFailed, optical_product_upper);
            Interval _4870 = _9176;
            Interval _4871 = _9236;
            Interval _9258 = jet_mul_derivative(_4870, _4871, intervalFailed, optical_product_upper);
            Interval _4872 = _9258;
            Interval _4873 = _9174;
            Interval _4874 = Interval{ 0.0, 0.0 };
            Interval _9259 = jet_mul_derivative(_4873, _4874, intervalFailed, optical_product_upper);
            Interval _4875 = _9259;
            Interval _9260 = jet_add_derivative(_4872, _4875, intervalFailed);
            Interval _4876 = _9178;
            Interval _4877 = _9236;
            Interval _9261 = jet_mul_derivative(_4876, _4877, intervalFailed, optical_product_upper);
            Interval _4878 = _9261;
            Interval _4879 = _9174;
            Interval _4880 = Interval{ 0.0, 0.0 };
            Interval _9262 = jet_mul_derivative(_4879, _4880, intervalFailed, optical_product_upper);
            Interval _4881 = _9262;
            Interval _9263 = jet_add_derivative(_4878, _4881, intervalFailed);
            Interval _4854 = _9228;
            Interval _4855 = _9236;
            Interval _9264 = imul(_4854, _4855, intervalFailed, optical_product_upper);
            Interval _4856 = _9230;
            Interval _4857 = _9236;
            Interval _9265 = jet_mul_derivative(_4856, _4857, intervalFailed, optical_product_upper);
            Interval _4858 = _9265;
            Interval _4859 = _9228;
            Interval _4860 = Interval{ 0.0, 0.0 };
            Interval _9266 = jet_mul_derivative(_4859, _4860, intervalFailed, optical_product_upper);
            Interval _4861 = _9266;
            Interval _9267 = jet_add_derivative(_4858, _4861, intervalFailed);
            Interval _4862 = _9232;
            Interval _4863 = _9236;
            Interval _9268 = jet_mul_derivative(_4862, _4863, intervalFailed, optical_product_upper);
            Interval _4864 = _9268;
            Interval _4865 = _9228;
            Interval _4866 = Interval{ 0.0, 0.0 };
            Interval _9269 = jet_mul_derivative(_4865, _4866, intervalFailed, optical_product_upper);
            Interval _4867 = _9269;
            Interval _9270 = jet_add_derivative(_4864, _4867, intervalFailed);
            Interval _4848 = _9036;
            Interval _4849 = _9250;
            Interval _9271 = iadd(_4848, _4849, intervalFailed);
            Interval _4850 = _9041;
            Interval _4851 = _9253;
            Interval _9272 = jet_add_derivative(_4850, _4851, intervalFailed);
            Interval _4852 = _9046;
            Interval _4853 = _9256;
            Interval _9273 = jet_add_derivative(_4852, _4853, intervalFailed);
            Interval _4842 = _9048;
            Interval _4843 = _9257;
            Interval _9274 = iadd(_4842, _4843, intervalFailed);
            Interval _4844 = _9053;
            Interval _4845 = _9260;
            Interval _9275 = jet_add_derivative(_4844, _4845, intervalFailed);
            Interval _4846 = _9058;
            Interval _4847 = _9263;
            Interval _9276 = jet_add_derivative(_4846, _4847, intervalFailed);
            Interval _4836 = _9060;
            Interval _4837 = _9264;
            Interval _9277 = iadd(_4836, _4837, intervalFailed);
            Interval _4838 = _9065;
            Interval _4839 = _9267;
            Interval _9278 = jet_add_derivative(_4838, _4839, intervalFailed);
            Interval _4840 = _9070;
            Interval _4841 = _9270;
            Interval _9279 = jet_add_derivative(_4840, _4841, intervalFailed);
            bool _9287;
            if (receiver.settings.x <= 0.0)
            {
                _9287 = receiver.settings.x >= 0.0;
            }
            else
            {
                _9287 = false;
            }
            float _9294;
            if (_9287)
            {
                _9294 = 0.0;
            }
            else
            {
                _9294 = precise::min(abs(receiver.settings.x), abs(receiver.settings.x));
            }
            float _9297 = precise::max(abs(receiver.settings.x), abs(receiver.settings.x));
            float _4826 = spvFMul(_9294, _9294);
            float _9298 = interval_down(_4826, intervalFailed);
            float _9299 = precise::max(0.0, _9298);
            float _4827 = spvFMul(_9297, _9297);
            float _9300 = interval_up(_4827, intervalFailed);
            Interval _4828 = Interval{ 2.0, 2.0 };
            Interval _4829 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9302 = imul(_4828, _4829, intervalFailed, optical_product_upper);
            Interval _4830 = _9302;
            Interval _4831 = Interval{ 0.0, 0.0 };
            Interval _9303 = jet_mul_derivative(_4830, _4831, intervalFailed, optical_product_upper);
            Interval _4832 = Interval{ 2.0, 2.0 };
            Interval _4833 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9305 = imul(_4832, _4833, intervalFailed, optical_product_upper);
            Interval _4834 = _9305;
            Interval _4835 = Interval{ 0.0, 0.0 };
            Interval _9306 = jet_mul_derivative(_4834, _4835, intervalFailed, optical_product_upper);
            Interval _4812 = _9271;
            Interval _4813 = Interval{ _9299, _9300 };
            Interval _9308 = imul(_4812, _4813, intervalFailed, optical_product_upper);
            Interval _4814 = _9272;
            Interval _4815 = Interval{ _9299, _9300 };
            Interval _9310 = jet_mul_derivative(_4814, _4815, intervalFailed, optical_product_upper);
            Interval _4816 = _9310;
            Interval _4817 = _9271;
            Interval _4818 = _9303;
            Interval _9311 = jet_mul_derivative(_4817, _4818, intervalFailed, optical_product_upper);
            Interval _4819 = _9311;
            Interval _9312 = jet_add_derivative(_4816, _4819, intervalFailed);
            Interval _4820 = _9273;
            Interval _4821 = Interval{ _9299, _9300 };
            Interval _9314 = jet_mul_derivative(_4820, _4821, intervalFailed, optical_product_upper);
            Interval _4822 = _9314;
            Interval _4823 = _9271;
            Interval _4824 = _9306;
            Interval _9315 = jet_mul_derivative(_4823, _4824, intervalFailed, optical_product_upper);
            Interval _4825 = _9315;
            Interval _9316 = jet_add_derivative(_4822, _4825, intervalFailed);
            Interval _4798 = _9274;
            Interval _4799 = Interval{ _9299, _9300 };
            Interval _9318 = imul(_4798, _4799, intervalFailed, optical_product_upper);
            Interval _4800 = _9275;
            Interval _4801 = Interval{ _9299, _9300 };
            Interval _9320 = jet_mul_derivative(_4800, _4801, intervalFailed, optical_product_upper);
            Interval _4802 = _9320;
            Interval _4803 = _9274;
            Interval _4804 = _9303;
            Interval _9321 = jet_mul_derivative(_4803, _4804, intervalFailed, optical_product_upper);
            Interval _4805 = _9321;
            Interval _9322 = jet_add_derivative(_4802, _4805, intervalFailed);
            Interval _4806 = _9276;
            Interval _4807 = Interval{ _9299, _9300 };
            Interval _9324 = jet_mul_derivative(_4806, _4807, intervalFailed, optical_product_upper);
            Interval _4808 = _9324;
            Interval _4809 = _9274;
            Interval _4810 = _9306;
            Interval _9325 = jet_mul_derivative(_4809, _4810, intervalFailed, optical_product_upper);
            Interval _4811 = _9325;
            Interval _9326 = jet_add_derivative(_4808, _4811, intervalFailed);
            Interval _4784 = _9277;
            Interval _4785 = Interval{ _9299, _9300 };
            Interval _9328 = imul(_4784, _4785, intervalFailed, optical_product_upper);
            Interval _4786 = _9278;
            Interval _4787 = Interval{ _9299, _9300 };
            Interval _9330 = jet_mul_derivative(_4786, _4787, intervalFailed, optical_product_upper);
            Interval _4788 = _9330;
            Interval _4789 = _9277;
            Interval _4790 = _9303;
            Interval _9331 = jet_mul_derivative(_4789, _4790, intervalFailed, optical_product_upper);
            Interval _4791 = _9331;
            Interval _9332 = jet_add_derivative(_4788, _4791, intervalFailed);
            Interval _4792 = _9279;
            Interval _4793 = Interval{ _9299, _9300 };
            Interval _9334 = jet_mul_derivative(_4792, _4793, intervalFailed, optical_product_upper);
            Interval _4794 = _9334;
            Interval _4795 = _9277;
            Interval _4796 = _9306;
            Interval _9335 = jet_mul_derivative(_4795, _4796, intervalFailed, optical_product_upper);
            Interval _4797 = _9335;
            Interval _9336 = jet_add_derivative(_4794, _4797, intervalFailed);
            Interval _4778 = _8253;
            Interval _4779 = _9308;
            Interval _9337 = iadd(_4778, _4779, intervalFailed);
            Interval _4780 = _8256;
            Interval _4781 = _9312;
            Interval _9338 = jet_add_derivative(_4780, _4781, intervalFailed);
            Interval _4782 = _8259;
            Interval _4783 = _9316;
            Interval _9339 = jet_add_derivative(_4782, _4783, intervalFailed);
            Interval _4772 = _8262;
            Interval _4773 = _9318;
            Interval _9340 = iadd(_4772, _4773, intervalFailed);
            Interval _4774 = _8265;
            Interval _4775 = _9322;
            Interval _9341 = jet_add_derivative(_4774, _4775, intervalFailed);
            Interval _4776 = _8268;
            Interval _4777 = _9326;
            Interval _9342 = jet_add_derivative(_4776, _4777, intervalFailed);
            Interval _4766 = _8271;
            Interval _4767 = _9328;
            Interval _9343 = iadd(_4766, _4767, intervalFailed);
            Interval _4768 = _8274;
            Interval _4769 = _9332;
            Interval _9344 = jet_add_derivative(_4768, _4769, intervalFailed);
            Interval _4770 = _8277;
            Interval _4771 = _9336;
            Interval _9345 = jet_add_derivative(_4770, _4771, intervalFailed);
            bool _9352;
            if (_9337.lo <= 0.0)
            {
                _9352 = _9337.hi >= 0.0;
            }
            else
            {
                _9352 = false;
            }
            float _9359;
            if (_9352)
            {
                _9359 = 0.0;
            }
            else
            {
                _9359 = precise::min(abs(_9337.lo), abs(_9337.hi));
            }
            float _9362 = precise::max(abs(_9337.lo), abs(_9337.hi));
            float _4756 = spvFMul(_9359, _9359);
            float _9363 = interval_down(_4756, intervalFailed);
            float _4757 = spvFMul(_9362, _9362);
            float _9365 = interval_up(_4757, intervalFailed);
            Interval _4758 = Interval{ 2.0, 2.0 };
            Interval _4759 = _9337;
            Interval _9366 = imul(_4758, _4759, intervalFailed, optical_product_upper);
            Interval _4760 = _9366;
            Interval _4761 = _9338;
            Interval _9367 = jet_mul_derivative(_4760, _4761, intervalFailed, optical_product_upper);
            Interval _4762 = Interval{ 2.0, 2.0 };
            Interval _4763 = _9337;
            Interval _9368 = imul(_4762, _4763, intervalFailed, optical_product_upper);
            Interval _4764 = _9368;
            Interval _4765 = _9339;
            Interval _9369 = jet_mul_derivative(_4764, _4765, intervalFailed, optical_product_upper);
            bool _9376;
            if (_9340.lo <= 0.0)
            {
                _9376 = _9340.hi >= 0.0;
            }
            else
            {
                _9376 = false;
            }
            float _9383;
            if (_9376)
            {
                _9383 = 0.0;
            }
            else
            {
                _9383 = precise::min(abs(_9340.lo), abs(_9340.hi));
            }
            float _9386 = precise::max(abs(_9340.lo), abs(_9340.hi));
            float _4746 = spvFMul(_9383, _9383);
            float _9387 = interval_down(_4746, intervalFailed);
            float _4747 = spvFMul(_9386, _9386);
            float _9389 = interval_up(_4747, intervalFailed);
            Interval _4748 = Interval{ 2.0, 2.0 };
            Interval _4749 = _9340;
            Interval _9390 = imul(_4748, _4749, intervalFailed, optical_product_upper);
            Interval _4750 = _9390;
            Interval _4751 = _9341;
            Interval _9391 = jet_mul_derivative(_4750, _4751, intervalFailed, optical_product_upper);
            Interval _4752 = Interval{ 2.0, 2.0 };
            Interval _4753 = _9340;
            Interval _9392 = imul(_4752, _4753, intervalFailed, optical_product_upper);
            Interval _4754 = _9392;
            Interval _4755 = _9342;
            Interval _9393 = jet_mul_derivative(_4754, _4755, intervalFailed, optical_product_upper);
            Interval _4740 = Interval{ precise::max(0.0, _9363), _9365 };
            Interval _4741 = Interval{ precise::max(0.0, _9387), _9389 };
            Interval _9396 = iadd(_4740, _4741, intervalFailed);
            Interval _4742 = _9367;
            Interval _4743 = _9391;
            Interval _9397 = jet_add_derivative(_4742, _4743, intervalFailed);
            Interval _4744 = _9369;
            Interval _4745 = _9393;
            Interval _9398 = jet_add_derivative(_4744, _4745, intervalFailed);
            bool _9405;
            if (_9343.lo <= 0.0)
            {
                _9405 = _9343.hi >= 0.0;
            }
            else
            {
                _9405 = false;
            }
            float _9412;
            if (_9405)
            {
                _9412 = 0.0;
            }
            else
            {
                _9412 = precise::min(abs(_9343.lo), abs(_9343.hi));
            }
            float _9415 = precise::max(abs(_9343.lo), abs(_9343.hi));
            float _4730 = spvFMul(_9412, _9412);
            float _9416 = interval_down(_4730, intervalFailed);
            float _4731 = spvFMul(_9415, _9415);
            float _9418 = interval_up(_4731, intervalFailed);
            Interval _4732 = Interval{ 2.0, 2.0 };
            Interval _4733 = _9343;
            Interval _9419 = imul(_4732, _4733, intervalFailed, optical_product_upper);
            Interval _4734 = _9419;
            Interval _4735 = _9344;
            Interval _9420 = jet_mul_derivative(_4734, _4735, intervalFailed, optical_product_upper);
            Interval _4736 = Interval{ 2.0, 2.0 };
            Interval _4737 = _9343;
            Interval _9421 = imul(_4736, _4737, intervalFailed, optical_product_upper);
            Interval _4738 = _9421;
            Interval _4739 = _9345;
            Interval _9422 = jet_mul_derivative(_4738, _4739, intervalFailed, optical_product_upper);
            Interval _4724 = _9396;
            Interval _4725 = Interval{ precise::max(0.0, _9416), _9418 };
            Interval _9424 = iadd(_4724, _4725, intervalFailed);
            Interval _4726 = _9397;
            Interval _4727 = _9420;
            Interval _9425 = jet_add_derivative(_4726, _4727, intervalFailed);
            Interval _4728 = _9398;
            Interval _4729 = _9422;
            Interval _9426 = jet_add_derivative(_4728, _4729, intervalFailed);
            Interval _4715 = _9424;
            Interval _9428 = isqrt(_4715, intervalFailed);
            bool _9436;
            if (!intervalFailed)
            {
                _9436 = intervalFailed;
            }
            else
            {
                _9436 = false;
            }
            bool _9441;
            if (_9436)
            {
                _9441 = jetFailureSite == 0u;
            }
            else
            {
                _9441 = false;
            }
            if (_9441)
            {
                jetFailureSite = 3u;
                jetFailureArguments = float4(_9424.lo, _9424.hi, 0.0, 0.0);
            }
            if (_9428.lo <= 0.0)
            {
                jetBranchKnown = false;
            }
            Interval _4716 = Interval{ 2.0, 2.0 };
            Interval _4717 = _9428;
            Interval _9448 = imul(_4716, _4717, intervalFailed, optical_product_upper);
            Interval _4718 = Interval{ 1.0, 1.0 };
            Interval _4719 = _9448;
            Interval _9450 = idiv(_4718, _4719, intervalFailed, interval_divide_upper);
            bool _9457;
            if (!intervalFailed)
            {
                _9457 = intervalFailed;
            }
            else
            {
                _9457 = false;
            }
            bool _9462;
            if (_9457)
            {
                _9462 = jetFailureSite == 0u;
            }
            else
            {
                _9462 = false;
            }
            if (_9462)
            {
                jetFailureSite = 4u;
                jetFailureArguments = float4(1.0, 1.0, _9448.lo, _9448.hi);
            }
            Interval _4720 = _9450;
            Interval _4721 = _9425;
            Interval _9466 = jet_mul_derivative(_4720, _4721, intervalFailed, optical_product_upper);
            Interval _4722 = _9450;
            Interval _4723 = _9426;
            Interval _9467 = jet_mul_derivative(_4722, _4723, intervalFailed, optical_product_upper);
            Interval _4701 = Interval{ 1.0, 1.0 };
            Interval _4702 = _9428;
            Interval _9469 = idiv(_4701, _4702, intervalFailed, interval_divide_upper);
            bool _9476;
            if (!intervalFailed)
            {
                _9476 = intervalFailed;
            }
            else
            {
                _9476 = false;
            }
            bool _9481;
            if (_9476)
            {
                _9481 = jetFailureSite == 0u;
            }
            else
            {
                _9481 = false;
            }
            if (_9481)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(1.0, 1.0, _9428.lo, _9428.hi);
            }
            Interval _4703 = Interval{ 0.0, 0.0 };
            Interval _4704 = _9469;
            Interval _4705 = _9466;
            Interval _9485 = jet_mul_derivative(_4704, _4705, intervalFailed, optical_product_upper);
            Interval _4706 = Interval{ as_type<float>(as_type<uint>(_9485.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9485.lo) ^ 2147483648u) };
            Interval _9495 = jet_add_derivative(_4703, _4706, intervalFailed);
            Interval _4707 = _9495;
            Interval _4708 = _9428;
            Interval _9496 = jet_div_derivative(_4707, _4708, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4709 = Interval{ 0.0, 0.0 };
            Interval _4710 = _9469;
            Interval _4711 = _9467;
            Interval _9497 = jet_mul_derivative(_4710, _4711, intervalFailed, optical_product_upper);
            Interval _4712 = Interval{ as_type<float>(as_type<uint>(_9497.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9497.lo) ^ 2147483648u) };
            Interval _9507 = jet_add_derivative(_4709, _4712, intervalFailed);
            Interval _4713 = _9507;
            Interval _4714 = _9428;
            Interval _9508 = jet_div_derivative(_4713, _4714, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4687 = _9337;
            Interval _4688 = _9469;
            Interval _9509 = imul(_4687, _4688, intervalFailed, optical_product_upper);
            Interval _4689 = _9338;
            Interval _4690 = _9469;
            Interval _9510 = jet_mul_derivative(_4689, _4690, intervalFailed, optical_product_upper);
            Interval _4691 = _9510;
            Interval _4692 = _9337;
            Interval _4693 = _9496;
            Interval _9511 = jet_mul_derivative(_4692, _4693, intervalFailed, optical_product_upper);
            Interval _4694 = _9511;
            Interval _9512 = jet_add_derivative(_4691, _4694, intervalFailed);
            Interval _4695 = _9339;
            Interval _4696 = _9469;
            Interval _9513 = jet_mul_derivative(_4695, _4696, intervalFailed, optical_product_upper);
            Interval _4697 = _9513;
            Interval _4698 = _9337;
            Interval _4699 = _9508;
            Interval _9514 = jet_mul_derivative(_4698, _4699, intervalFailed, optical_product_upper);
            Interval _4700 = _9514;
            Interval _9515 = jet_add_derivative(_4697, _4700, intervalFailed);
            Interval _4673 = _9340;
            Interval _4674 = _9469;
            Interval _9516 = imul(_4673, _4674, intervalFailed, optical_product_upper);
            Interval _4675 = _9341;
            Interval _4676 = _9469;
            Interval _9517 = jet_mul_derivative(_4675, _4676, intervalFailed, optical_product_upper);
            Interval _4677 = _9517;
            Interval _4678 = _9340;
            Interval _4679 = _9496;
            Interval _9518 = jet_mul_derivative(_4678, _4679, intervalFailed, optical_product_upper);
            Interval _4680 = _9518;
            Interval _9519 = jet_add_derivative(_4677, _4680, intervalFailed);
            Interval _4681 = _9342;
            Interval _4682 = _9469;
            Interval _9520 = jet_mul_derivative(_4681, _4682, intervalFailed, optical_product_upper);
            Interval _4683 = _9520;
            Interval _4684 = _9340;
            Interval _4685 = _9508;
            Interval _9521 = jet_mul_derivative(_4684, _4685, intervalFailed, optical_product_upper);
            Interval _4686 = _9521;
            Interval _9522 = jet_add_derivative(_4683, _4686, intervalFailed);
            Interval _4659 = _9343;
            Interval _4660 = _9469;
            Interval _9523 = imul(_4659, _4660, intervalFailed, optical_product_upper);
            Interval _4661 = _9344;
            Interval _4662 = _9469;
            Interval _9524 = jet_mul_derivative(_4661, _4662, intervalFailed, optical_product_upper);
            Interval _4663 = _9524;
            Interval _4664 = _9343;
            Interval _4665 = _9496;
            Interval _9525 = jet_mul_derivative(_4664, _4665, intervalFailed, optical_product_upper);
            Interval _4666 = _9525;
            Interval _9526 = jet_add_derivative(_4663, _4666, intervalFailed);
            Interval _4667 = _9345;
            Interval _4668 = _9469;
            Interval _9527 = jet_mul_derivative(_4667, _4668, intervalFailed, optical_product_upper);
            Interval _4669 = _9527;
            Interval _4670 = _9343;
            Interval _4671 = _9508;
            Interval _9528 = jet_mul_derivative(_4670, _4671, intervalFailed, optical_product_upper);
            Interval _4672 = _9528;
            Interval _9529 = jet_add_derivative(_4669, _4672, intervalFailed);
            Interval _4645 = _9509;
            Interval _4646 = Interval{ _7929, _7930 };
            Interval _9531 = imul(_4645, _4646, intervalFailed, optical_product_upper);
            Interval _4647 = _9512;
            Interval _4648 = Interval{ _7929, _7930 };
            Interval _9533 = jet_mul_derivative(_4647, _4648, intervalFailed, optical_product_upper);
            Interval _4649 = _9533;
            Interval _4650 = _9509;
            Interval _4651 = Interval{ _7931, _7932 };
            Interval _9535 = jet_mul_derivative(_4650, _4651, intervalFailed, optical_product_upper);
            Interval _4652 = _9535;
            Interval _9536 = jet_add_derivative(_4649, _4652, intervalFailed);
            Interval _4653 = _9515;
            Interval _4654 = Interval{ _7929, _7930 };
            Interval _9538 = jet_mul_derivative(_4653, _4654, intervalFailed, optical_product_upper);
            Interval _4655 = _9538;
            Interval _4656 = _9509;
            Interval _4657 = Interval{ _7933, _7934 };
            Interval _9540 = jet_mul_derivative(_4656, _4657, intervalFailed, optical_product_upper);
            Interval _4658 = _9540;
            Interval _9541 = jet_add_derivative(_4655, _4658, intervalFailed);
            Interval _4631 = _9516;
            Interval _4632 = Interval{ _7935, _7936 };
            Interval _9543 = imul(_4631, _4632, intervalFailed, optical_product_upper);
            Interval _4633 = _9519;
            Interval _4634 = Interval{ _7935, _7936 };
            Interval _9545 = jet_mul_derivative(_4633, _4634, intervalFailed, optical_product_upper);
            Interval _4635 = _9545;
            Interval _4636 = _9516;
            Interval _4637 = Interval{ _7937, _7938 };
            Interval _9547 = jet_mul_derivative(_4636, _4637, intervalFailed, optical_product_upper);
            Interval _4638 = _9547;
            Interval _9548 = jet_add_derivative(_4635, _4638, intervalFailed);
            Interval _4639 = _9522;
            Interval _4640 = Interval{ _7935, _7936 };
            Interval _9550 = jet_mul_derivative(_4639, _4640, intervalFailed, optical_product_upper);
            Interval _4641 = _9550;
            Interval _4642 = _9516;
            Interval _4643 = Interval{ _7939, _7940 };
            Interval _9552 = jet_mul_derivative(_4642, _4643, intervalFailed, optical_product_upper);
            Interval _4644 = _9552;
            Interval _9553 = jet_add_derivative(_4641, _4644, intervalFailed);
            Interval _4625 = _9531;
            Interval _4626 = _9543;
            Interval _9554 = iadd(_4625, _4626, intervalFailed);
            Interval _4627 = _9536;
            Interval _4628 = _9548;
            Interval _9555 = jet_add_derivative(_4627, _4628, intervalFailed);
            Interval _4629 = _9541;
            Interval _4630 = _9553;
            Interval _9556 = jet_add_derivative(_4629, _4630, intervalFailed);
            Interval _4611 = _9523;
            Interval _4612 = Interval{ _7941, _7942 };
            Interval _9558 = imul(_4611, _4612, intervalFailed, optical_product_upper);
            Interval _4613 = _9526;
            Interval _4614 = Interval{ _7941, _7942 };
            Interval _9560 = jet_mul_derivative(_4613, _4614, intervalFailed, optical_product_upper);
            Interval _4615 = _9560;
            Interval _4616 = _9523;
            Interval _4617 = Interval{ _7943, _7944 };
            Interval _9562 = jet_mul_derivative(_4616, _4617, intervalFailed, optical_product_upper);
            Interval _4618 = _9562;
            Interval _9563 = jet_add_derivative(_4615, _4618, intervalFailed);
            Interval _4619 = _9529;
            Interval _4620 = Interval{ _7941, _7942 };
            Interval _9565 = jet_mul_derivative(_4619, _4620, intervalFailed, optical_product_upper);
            Interval _4621 = _9565;
            Interval _4622 = _9523;
            Interval _4623 = Interval{ _7945, _7946 };
            Interval _9567 = jet_mul_derivative(_4622, _4623, intervalFailed, optical_product_upper);
            Interval _4624 = _9567;
            Interval _9568 = jet_add_derivative(_4621, _4624, intervalFailed);
            Interval _4605 = _9554;
            Interval _4606 = _9558;
            Interval _9569 = iadd(_4605, _4606, intervalFailed);
            Interval _4607 = _9555;
            Interval _4608 = _9563;
            __attribute__((unused)) Interval _9570 = jet_add_derivative(_4607, _4608, intervalFailed);
            Interval _4609 = _9556;
            Interval _4610 = _9568;
            __attribute__((unused)) Interval _9571 = jet_add_derivative(_4609, _4610, intervalFailed);
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
            float _9606;
            float _9607;
            float _9608;
            float _9609;
            float _9610;
            float _9611;
            if (_9569.lo > 0.0)
            {
                _9594 = _9509.lo;
                _9595 = _9509.hi;
                _9596 = _9512.lo;
                _9597 = _9512.hi;
                _9598 = _9515.lo;
                _9599 = _9515.hi;
                _9600 = _9516.lo;
                _9601 = _9516.hi;
                _9602 = _9519.lo;
                _9603 = _9519.hi;
                _9604 = _9522.lo;
                _9605 = _9522.hi;
                _9606 = _9523.lo;
                _9607 = _9523.hi;
                _9608 = _9526.lo;
                _9609 = _9526.hi;
                _9610 = _9529.lo;
                _9611 = _9529.hi;
            }
            else
            {
                if (_9569.hi > 0.0)
                {
                    return false;
                }
                _9594 = _8253.lo;
                _9595 = _8253.hi;
                _9596 = _8256.lo;
                _9597 = _8256.hi;
                _9598 = _8259.lo;
                _9599 = _8259.hi;
                _9600 = _8262.lo;
                _9601 = _8262.hi;
                _9602 = _8265.lo;
                _9603 = _8265.hi;
                _9604 = _8268.lo;
                _9605 = _8268.hi;
                _9606 = _8271.lo;
                _9607 = _8271.hi;
                _9608 = _8274.lo;
                _9609 = _8274.hi;
                _9610 = _8277.lo;
                _9611 = _8277.hi;
            }
            _6770 = _9594;
            _6772 = _9595;
            _6774 = _9596;
            _6776 = _9597;
            _6778 = _9598;
            _6780 = _9599;
            _6782 = _9600;
            _6784 = _9601;
            _6786 = _9602;
            _6788 = _9603;
            _6790 = _9604;
            _6792 = _9605;
            _6794 = _9606;
            _6796 = _9607;
            _6798 = _9608;
            _6800 = _9609;
            _6802 = _9610;
            _6804 = _9611;
        }
        else
        {
            _6770 = _8253.lo;
            _6772 = _8253.hi;
            _6774 = _8256.lo;
            _6776 = _8256.hi;
            _6778 = _8259.lo;
            _6780 = _8259.hi;
            _6782 = _8262.lo;
            _6784 = _8262.hi;
            _6786 = _8265.lo;
            _6788 = _8265.hi;
            _6790 = _8268.lo;
            _6792 = _8268.hi;
            _6794 = _8271.lo;
            _6796 = _8271.hi;
            _6798 = _8274.lo;
            _6800 = _8274.hi;
            _6802 = _8277.lo;
            _6804 = _8277.hi;
        }
        bool _9616;
        if (!intervalFailed)
        {
            _9616 = !jetBranchKnown;
        }
        else
        {
            _9616 = true;
        }
        if (_9616)
        {
            return false;
        }
    }
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ _6805, _6807 }, Interval{ _6809, _6811 }, Interval{ _6813, _6815 } }, OpticalJet{ Interval{ _6817, _6819 }, Interval{ _6821, _6823 }, Interval{ _6825, _6827 } }, OpticalJet{ Interval{ _6829, _6831 }, Interval{ _6833, _6835 }, Interval{ _6837, _6839 } } };
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ _6769, _6771 }, Interval{ _6773, _6775 }, Interval{ _6777, _6779 } }, OpticalJet{ Interval{ _6781, _6783 }, Interval{ _6785, _6787 }, Interval{ _6789, _6791 } }, OpticalJet{ Interval{ _6793, _6795 }, Interval{ _6797, _6799 }, Interval{ _6801, _6803 } } };
    ray.bias0 = OpticalJet{ Interval{ _6757, _6759 }, Interval{ _6761, _6763 }, Interval{ _6765, _6767 } };
    bool _9653;
    if (!intervalFailed)
    {
        _9653 = jetBranchKnown;
    }
    else
    {
        _9653 = false;
    }
    return _9653;
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
    float _10175;
    float _10176;
    float _10177;
    float _10178;
    float _10179;
    float _10180;
    if (finiteTerminal)
    {
        Interval _10022 = target.x;
        Interval _10023 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.v.lo) ^ 2147483648u) };
        Interval _10128 = iadd(_10022, _10023, intervalFailed);
        Interval _10024 = Interval{ 0.0, 0.0 };
        Interval _10025 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dx.lo) ^ 2147483648u) };
        Interval _10130 = jet_add_derivative(_10024, _10025, intervalFailed);
        Interval _10026 = Interval{ 0.0, 0.0 };
        Interval _10027 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dy.lo) ^ 2147483648u) };
        Interval _10132 = jet_add_derivative(_10026, _10027, intervalFailed);
        Interval _10016 = target.y;
        Interval _10017 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.v.lo) ^ 2147483648u) };
        Interval _10134 = iadd(_10016, _10017, intervalFailed);
        Interval _10018 = Interval{ 0.0, 0.0 };
        Interval _10019 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dx.lo) ^ 2147483648u) };
        Interval _10136 = jet_add_derivative(_10018, _10019, intervalFailed);
        Interval _10020 = Interval{ 0.0, 0.0 };
        Interval _10021 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dy.lo) ^ 2147483648u) };
        Interval _10138 = jet_add_derivative(_10020, _10021, intervalFailed);
        Interval _10010 = target.z;
        Interval _10011 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.v.lo) ^ 2147483648u) };
        Interval _10140 = iadd(_10010, _10011, intervalFailed);
        Interval _10012 = Interval{ 0.0, 0.0 };
        Interval _10013 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dx.lo) ^ 2147483648u) };
        Interval _10142 = jet_add_derivative(_10012, _10013, intervalFailed);
        Interval _10014 = Interval{ 0.0, 0.0 };
        Interval _10015 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dy.lo) ^ 2147483648u) };
        Interval _10144 = jet_add_derivative(_10014, _10015, intervalFailed);
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
        _10175 = _10140.lo;
        _10176 = _10140.hi;
        _10177 = _10142.lo;
        _10178 = _10142.hi;
        _10179 = _10144.lo;
        _10180 = _10144.hi;
    }
    else
    {
        _10163 = target.x.lo;
        _10164 = target.x.hi;
        _10165 = 0.0;
        _10166 = 0.0;
        _10167 = 0.0;
        _10168 = 0.0;
        _10169 = target.y.lo;
        _10170 = target.y.hi;
        _10171 = 0.0;
        _10172 = 0.0;
        _10173 = 0.0;
        _10174 = 0.0;
        _10175 = target.z.lo;
        _10176 = target.z.hi;
        _10177 = 0.0;
        _10178 = 0.0;
        _10179 = 0.0;
        _10180 = 0.0;
    }
    bool _10185;
    if (_10163 <= 0.0)
    {
        _10185 = _10164 >= 0.0;
    }
    else
    {
        _10185 = false;
    }
    float _10192;
    if (_10185)
    {
        _10192 = 0.0;
    }
    else
    {
        _10192 = precise::min(abs(_10163), abs(_10164));
    }
    float _10195 = precise::max(abs(_10163), abs(_10164));
    float _10000 = spvFMul(_10192, _10192);
    float _10196 = interval_down(_10000, intervalFailed);
    float _10001 = spvFMul(_10195, _10195);
    float _10198 = interval_up(_10001, intervalFailed);
    Interval _10002 = Interval{ 2.0, 2.0 };
    Interval _10003 = Interval{ _10163, _10164 };
    Interval _10200 = imul(_10002, _10003, intervalFailed, optical_product_upper);
    Interval _10004 = _10200;
    Interval _10005 = Interval{ _10165, _10166 };
    Interval _10202 = jet_mul_derivative(_10004, _10005, intervalFailed, optical_product_upper);
    Interval _10006 = Interval{ 2.0, 2.0 };
    Interval _10007 = Interval{ _10163, _10164 };
    Interval _10204 = imul(_10006, _10007, intervalFailed, optical_product_upper);
    Interval _10008 = _10204;
    Interval _10009 = Interval{ _10167, _10168 };
    Interval _10206 = jet_mul_derivative(_10008, _10009, intervalFailed, optical_product_upper);
    bool _10211;
    if (_10169 <= 0.0)
    {
        _10211 = _10170 >= 0.0;
    }
    else
    {
        _10211 = false;
    }
    float _10218;
    if (_10211)
    {
        _10218 = 0.0;
    }
    else
    {
        _10218 = precise::min(abs(_10169), abs(_10170));
    }
    float _10221 = precise::max(abs(_10169), abs(_10170));
    float _9990 = spvFMul(_10218, _10218);
    float _10222 = interval_down(_9990, intervalFailed);
    float _9991 = spvFMul(_10221, _10221);
    float _10224 = interval_up(_9991, intervalFailed);
    Interval _9992 = Interval{ 2.0, 2.0 };
    Interval _9993 = Interval{ _10169, _10170 };
    Interval _10226 = imul(_9992, _9993, intervalFailed, optical_product_upper);
    Interval _9994 = _10226;
    Interval _9995 = Interval{ _10171, _10172 };
    Interval _10228 = jet_mul_derivative(_9994, _9995, intervalFailed, optical_product_upper);
    Interval _9996 = Interval{ 2.0, 2.0 };
    Interval _9997 = Interval{ _10169, _10170 };
    Interval _10230 = imul(_9996, _9997, intervalFailed, optical_product_upper);
    Interval _9998 = _10230;
    Interval _9999 = Interval{ _10173, _10174 };
    Interval _10232 = jet_mul_derivative(_9998, _9999, intervalFailed, optical_product_upper);
    Interval _9984 = Interval{ precise::max(0.0, _10196), _10198 };
    Interval _9985 = Interval{ precise::max(0.0, _10222), _10224 };
    Interval _10235 = iadd(_9984, _9985, intervalFailed);
    Interval _9986 = _10202;
    Interval _9987 = _10228;
    Interval _10236 = jet_add_derivative(_9986, _9987, intervalFailed);
    Interval _9988 = _10206;
    Interval _9989 = _10232;
    Interval _10237 = jet_add_derivative(_9988, _9989, intervalFailed);
    bool _10242;
    if (_10175 <= 0.0)
    {
        _10242 = _10176 >= 0.0;
    }
    else
    {
        _10242 = false;
    }
    float _10249;
    if (_10242)
    {
        _10249 = 0.0;
    }
    else
    {
        _10249 = precise::min(abs(_10175), abs(_10176));
    }
    float _10252 = precise::max(abs(_10175), abs(_10176));
    float _9974 = spvFMul(_10249, _10249);
    float _10253 = interval_down(_9974, intervalFailed);
    float _9975 = spvFMul(_10252, _10252);
    float _10255 = interval_up(_9975, intervalFailed);
    Interval _9976 = Interval{ 2.0, 2.0 };
    Interval _9977 = Interval{ _10175, _10176 };
    Interval _10257 = imul(_9976, _9977, intervalFailed, optical_product_upper);
    Interval _9978 = _10257;
    Interval _9979 = Interval{ _10177, _10178 };
    Interval _10259 = jet_mul_derivative(_9978, _9979, intervalFailed, optical_product_upper);
    Interval _9980 = Interval{ 2.0, 2.0 };
    Interval _9981 = Interval{ _10175, _10176 };
    Interval _10261 = imul(_9980, _9981, intervalFailed, optical_product_upper);
    Interval _9982 = _10261;
    Interval _9983 = Interval{ _10179, _10180 };
    Interval _10263 = jet_mul_derivative(_9982, _9983, intervalFailed, optical_product_upper);
    Interval _9968 = _10235;
    Interval _9969 = Interval{ precise::max(0.0, _10253), _10255 };
    Interval _10265 = iadd(_9968, _9969, intervalFailed);
    Interval _9970 = _10236;
    Interval _9971 = _10259;
    Interval _10266 = jet_add_derivative(_9970, _9971, intervalFailed);
    Interval _9972 = _10237;
    Interval _9973 = _10263;
    Interval _10267 = jet_add_derivative(_9972, _9973, intervalFailed);
    Interval _9959 = _10265;
    Interval _10269 = isqrt(_9959, intervalFailed);
    bool _10277;
    if (!intervalFailed)
    {
        _10277 = intervalFailed;
    }
    else
    {
        _10277 = false;
    }
    bool _10282;
    if (_10277)
    {
        _10282 = jetFailureSite == 0u;
    }
    else
    {
        _10282 = false;
    }
    if (_10282)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10265.lo, _10265.hi, 0.0, 0.0);
    }
    if (_10269.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9960 = Interval{ 2.0, 2.0 };
    Interval _9961 = _10269;
    Interval _10289 = imul(_9960, _9961, intervalFailed, optical_product_upper);
    Interval _9962 = Interval{ 1.0, 1.0 };
    Interval _9963 = _10289;
    Interval _10291 = idiv(_9962, _9963, intervalFailed, interval_divide_upper);
    bool _10298;
    if (!intervalFailed)
    {
        _10298 = intervalFailed;
    }
    else
    {
        _10298 = false;
    }
    bool _10303;
    if (_10298)
    {
        _10303 = jetFailureSite == 0u;
    }
    else
    {
        _10303 = false;
    }
    if (_10303)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10289.lo, _10289.hi);
    }
    Interval _9964 = _10291;
    Interval _9965 = _10266;
    __attribute__((unused)) Interval _10307 = jet_mul_derivative(_9964, _9965, intervalFailed, optical_product_upper);
    Interval _9966 = _10291;
    Interval _9967 = _10267;
    __attribute__((unused)) Interval _10308 = jet_mul_derivative(_9966, _9967, intervalFailed, optical_product_upper);
    float _10313;
    if (finiteTerminal)
    {
        _10313 = ray.bias0.v.hi;
    }
    else
    {
        _10313 = 0.0;
    }
    bool _10373;
    if ((isunordered(_10269.lo, _10313) || _10269.lo > _10313))
    {
        Interval _9945 = Interval{ _10163, _10164 };
        Interval _9946 = ray.outgoing.x.v;
        Interval _10324 = imul(_9945, _9946, intervalFailed, optical_product_upper);
        Interval _9947 = Interval{ _10165, _10166 };
        Interval _9948 = ray.outgoing.x.v;
        Interval _10326 = jet_mul_derivative(_9947, _9948, intervalFailed, optical_product_upper);
        Interval _9949 = _10326;
        Interval _9950 = Interval{ _10163, _10164 };
        Interval _9951 = ray.outgoing.x.dx;
        Interval _10328 = jet_mul_derivative(_9950, _9951, intervalFailed, optical_product_upper);
        Interval _9952 = _10328;
        Interval _10329 = jet_add_derivative(_9949, _9952, intervalFailed);
        Interval _9953 = Interval{ _10167, _10168 };
        Interval _9954 = ray.outgoing.x.v;
        Interval _10331 = jet_mul_derivative(_9953, _9954, intervalFailed, optical_product_upper);
        Interval _9955 = _10331;
        Interval _9956 = Interval{ _10163, _10164 };
        Interval _9957 = ray.outgoing.x.dy;
        Interval _10333 = jet_mul_derivative(_9956, _9957, intervalFailed, optical_product_upper);
        Interval _9958 = _10333;
        Interval _10334 = jet_add_derivative(_9955, _9958, intervalFailed);
        Interval _9931 = Interval{ _10169, _10170 };
        Interval _9932 = ray.outgoing.y.v;
        Interval _10339 = imul(_9931, _9932, intervalFailed, optical_product_upper);
        Interval _9933 = Interval{ _10171, _10172 };
        Interval _9934 = ray.outgoing.y.v;
        Interval _10341 = jet_mul_derivative(_9933, _9934, intervalFailed, optical_product_upper);
        Interval _9935 = _10341;
        Interval _9936 = Interval{ _10169, _10170 };
        Interval _9937 = ray.outgoing.y.dx;
        Interval _10343 = jet_mul_derivative(_9936, _9937, intervalFailed, optical_product_upper);
        Interval _9938 = _10343;
        Interval _10344 = jet_add_derivative(_9935, _9938, intervalFailed);
        Interval _9939 = Interval{ _10173, _10174 };
        Interval _9940 = ray.outgoing.y.v;
        Interval _10346 = jet_mul_derivative(_9939, _9940, intervalFailed, optical_product_upper);
        Interval _9941 = _10346;
        Interval _9942 = Interval{ _10169, _10170 };
        Interval _9943 = ray.outgoing.y.dy;
        Interval _10348 = jet_mul_derivative(_9942, _9943, intervalFailed, optical_product_upper);
        Interval _9944 = _10348;
        Interval _10349 = jet_add_derivative(_9941, _9944, intervalFailed);
        Interval _9925 = _10324;
        Interval _9926 = _10339;
        Interval _10350 = iadd(_9925, _9926, intervalFailed);
        Interval _9927 = _10329;
        Interval _9928 = _10344;
        Interval _10351 = jet_add_derivative(_9927, _9928, intervalFailed);
        Interval _9929 = _10334;
        Interval _9930 = _10349;
        Interval _10352 = jet_add_derivative(_9929, _9930, intervalFailed);
        Interval _9911 = Interval{ _10175, _10176 };
        Interval _9912 = ray.outgoing.z.v;
        Interval _10357 = imul(_9911, _9912, intervalFailed, optical_product_upper);
        Interval _9913 = Interval{ _10177, _10178 };
        Interval _9914 = ray.outgoing.z.v;
        Interval _10359 = jet_mul_derivative(_9913, _9914, intervalFailed, optical_product_upper);
        Interval _9915 = _10359;
        Interval _9916 = Interval{ _10175, _10176 };
        Interval _9917 = ray.outgoing.z.dx;
        Interval _10361 = jet_mul_derivative(_9916, _9917, intervalFailed, optical_product_upper);
        Interval _9918 = _10361;
        Interval _10362 = jet_add_derivative(_9915, _9918, intervalFailed);
        Interval _9919 = Interval{ _10179, _10180 };
        Interval _9920 = ray.outgoing.z.v;
        Interval _10364 = jet_mul_derivative(_9919, _9920, intervalFailed, optical_product_upper);
        Interval _9921 = _10364;
        Interval _9922 = Interval{ _10175, _10176 };
        Interval _9923 = ray.outgoing.z.dy;
        Interval _10366 = jet_mul_derivative(_9922, _9923, intervalFailed, optical_product_upper);
        Interval _9924 = _10366;
        Interval _10367 = jet_add_derivative(_9921, _9924, intervalFailed);
        Interval _9905 = _10350;
        Interval _9906 = _10357;
        Interval _10368 = iadd(_9905, _9906, intervalFailed);
        Interval _9907 = _10351;
        Interval _9908 = _10362;
        __attribute__((unused)) Interval _10369 = jet_add_derivative(_9907, _9908, intervalFailed);
        Interval _9909 = _10352;
        Interval _9910 = _10367;
        __attribute__((unused)) Interval _10370 = jet_add_derivative(_9909, _9910, intervalFailed);
        _10373 = _10368.lo <= 0.0;
    }
    else
    {
        _10373 = true;
    }
    if (_10373)
    {
        return false;
    }
    float _10392;
    float _10393;
    if (omitted == 0u)
    {
        _10392 = ray.outgoing.x.v.lo;
        _10393 = ray.outgoing.x.v.hi;
    }
    else
    {
        float _10390;
        float _10391;
        if (omitted == 1u)
        {
            _10390 = ray.outgoing.y.v.lo;
            _10391 = ray.outgoing.y.v.hi;
        }
        else
        {
            _10390 = ray.outgoing.z.v.lo;
            _10391 = ray.outgoing.z.v.hi;
        }
        _10392 = _10390;
        _10393 = _10391;
    }
    bool _10398;
    if (_10392 <= 0.0)
    {
        _10398 = _10393 >= 0.0;
    }
    else
    {
        _10398 = false;
    }
    if (_10398)
    {
        return false;
    }
    bool _10403;
    if (_10163 <= 0.0)
    {
        _10403 = _10164 >= 0.0;
    }
    else
    {
        _10403 = false;
    }
    float _10410;
    if (_10403)
    {
        _10410 = 0.0;
    }
    else
    {
        _10410 = precise::min(abs(_10163), abs(_10164));
    }
    float _10413 = precise::max(abs(_10163), abs(_10164));
    float _9895 = spvFMul(_10410, _10410);
    float _10414 = interval_down(_9895, intervalFailed);
    float _9896 = spvFMul(_10413, _10413);
    float _10416 = interval_up(_9896, intervalFailed);
    Interval _9897 = Interval{ 2.0, 2.0 };
    Interval _9898 = Interval{ _10163, _10164 };
    Interval _10418 = imul(_9897, _9898, intervalFailed, optical_product_upper);
    Interval _9899 = _10418;
    Interval _9900 = Interval{ _10165, _10166 };
    Interval _10420 = jet_mul_derivative(_9899, _9900, intervalFailed, optical_product_upper);
    Interval _9901 = Interval{ 2.0, 2.0 };
    Interval _9902 = Interval{ _10163, _10164 };
    Interval _10422 = imul(_9901, _9902, intervalFailed, optical_product_upper);
    Interval _9903 = _10422;
    Interval _9904 = Interval{ _10167, _10168 };
    Interval _10424 = jet_mul_derivative(_9903, _9904, intervalFailed, optical_product_upper);
    bool _10429;
    if (_10169 <= 0.0)
    {
        _10429 = _10170 >= 0.0;
    }
    else
    {
        _10429 = false;
    }
    float _10436;
    if (_10429)
    {
        _10436 = 0.0;
    }
    else
    {
        _10436 = precise::min(abs(_10169), abs(_10170));
    }
    float _10439 = precise::max(abs(_10169), abs(_10170));
    float _9885 = spvFMul(_10436, _10436);
    float _10440 = interval_down(_9885, intervalFailed);
    float _9886 = spvFMul(_10439, _10439);
    float _10442 = interval_up(_9886, intervalFailed);
    Interval _9887 = Interval{ 2.0, 2.0 };
    Interval _9888 = Interval{ _10169, _10170 };
    Interval _10444 = imul(_9887, _9888, intervalFailed, optical_product_upper);
    Interval _9889 = _10444;
    Interval _9890 = Interval{ _10171, _10172 };
    Interval _10446 = jet_mul_derivative(_9889, _9890, intervalFailed, optical_product_upper);
    Interval _9891 = Interval{ 2.0, 2.0 };
    Interval _9892 = Interval{ _10169, _10170 };
    Interval _10448 = imul(_9891, _9892, intervalFailed, optical_product_upper);
    Interval _9893 = _10448;
    Interval _9894 = Interval{ _10173, _10174 };
    Interval _10450 = jet_mul_derivative(_9893, _9894, intervalFailed, optical_product_upper);
    Interval _9879 = Interval{ precise::max(0.0, _10414), _10416 };
    Interval _9880 = Interval{ precise::max(0.0, _10440), _10442 };
    Interval _10453 = iadd(_9879, _9880, intervalFailed);
    Interval _9881 = _10420;
    Interval _9882 = _10446;
    Interval _10454 = jet_add_derivative(_9881, _9882, intervalFailed);
    Interval _9883 = _10424;
    Interval _9884 = _10450;
    Interval _10455 = jet_add_derivative(_9883, _9884, intervalFailed);
    bool _10460;
    if (_10175 <= 0.0)
    {
        _10460 = _10176 >= 0.0;
    }
    else
    {
        _10460 = false;
    }
    float _10467;
    if (_10460)
    {
        _10467 = 0.0;
    }
    else
    {
        _10467 = precise::min(abs(_10175), abs(_10176));
    }
    float _10470 = precise::max(abs(_10175), abs(_10176));
    float _9869 = spvFMul(_10467, _10467);
    float _10471 = interval_down(_9869, intervalFailed);
    float _9870 = spvFMul(_10470, _10470);
    float _10473 = interval_up(_9870, intervalFailed);
    Interval _9871 = Interval{ 2.0, 2.0 };
    Interval _9872 = Interval{ _10175, _10176 };
    Interval _10475 = imul(_9871, _9872, intervalFailed, optical_product_upper);
    Interval _9873 = _10475;
    Interval _9874 = Interval{ _10177, _10178 };
    Interval _10477 = jet_mul_derivative(_9873, _9874, intervalFailed, optical_product_upper);
    Interval _9875 = Interval{ 2.0, 2.0 };
    Interval _9876 = Interval{ _10175, _10176 };
    Interval _10479 = imul(_9875, _9876, intervalFailed, optical_product_upper);
    Interval _9877 = _10479;
    Interval _9878 = Interval{ _10179, _10180 };
    Interval _10481 = jet_mul_derivative(_9877, _9878, intervalFailed, optical_product_upper);
    Interval _9863 = _10453;
    Interval _9864 = Interval{ precise::max(0.0, _10471), _10473 };
    Interval _10483 = iadd(_9863, _9864, intervalFailed);
    Interval _9865 = _10454;
    Interval _9866 = _10477;
    Interval _10484 = jet_add_derivative(_9865, _9866, intervalFailed);
    Interval _9867 = _10455;
    Interval _9868 = _10481;
    Interval _10485 = jet_add_derivative(_9867, _9868, intervalFailed);
    Interval _9854 = _10483;
    Interval _10487 = isqrt(_9854, intervalFailed);
    bool _10495;
    if (!intervalFailed)
    {
        _10495 = intervalFailed;
    }
    else
    {
        _10495 = false;
    }
    bool _10500;
    if (_10495)
    {
        _10500 = jetFailureSite == 0u;
    }
    else
    {
        _10500 = false;
    }
    if (_10500)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10483.lo, _10483.hi, 0.0, 0.0);
    }
    if (_10487.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9855 = Interval{ 2.0, 2.0 };
    Interval _9856 = _10487;
    Interval _10507 = imul(_9855, _9856, intervalFailed, optical_product_upper);
    Interval _9857 = Interval{ 1.0, 1.0 };
    Interval _9858 = _10507;
    Interval _10509 = idiv(_9857, _9858, intervalFailed, interval_divide_upper);
    bool _10516;
    if (!intervalFailed)
    {
        _10516 = intervalFailed;
    }
    else
    {
        _10516 = false;
    }
    bool _10521;
    if (_10516)
    {
        _10521 = jetFailureSite == 0u;
    }
    else
    {
        _10521 = false;
    }
    if (_10521)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10507.lo, _10507.hi);
    }
    Interval _9859 = _10509;
    Interval _9860 = _10484;
    Interval _10525 = jet_mul_derivative(_9859, _9860, intervalFailed, optical_product_upper);
    Interval _9861 = _10509;
    Interval _9862 = _10485;
    Interval _10526 = jet_mul_derivative(_9861, _9862, intervalFailed, optical_product_upper);
    Interval _9840 = Interval{ 1.0, 1.0 };
    Interval _9841 = _10487;
    Interval _10528 = idiv(_9840, _9841, intervalFailed, interval_divide_upper);
    bool _10535;
    if (!intervalFailed)
    {
        _10535 = intervalFailed;
    }
    else
    {
        _10535 = false;
    }
    bool _10540;
    if (_10535)
    {
        _10540 = jetFailureSite == 0u;
    }
    else
    {
        _10540 = false;
    }
    if (_10540)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _10487.lo, _10487.hi);
    }
    Interval _9842 = Interval{ 0.0, 0.0 };
    Interval _9843 = _10528;
    Interval _9844 = _10525;
    Interval _10544 = jet_mul_derivative(_9843, _9844, intervalFailed, optical_product_upper);
    Interval _9845 = Interval{ as_type<float>(as_type<uint>(_10544.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10544.lo) ^ 2147483648u) };
    Interval _10554 = jet_add_derivative(_9842, _9845, intervalFailed);
    Interval _9846 = _10554;
    Interval _9847 = _10487;
    Interval _10555 = jet_div_derivative(_9846, _9847, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9848 = Interval{ 0.0, 0.0 };
    Interval _9849 = _10528;
    Interval _9850 = _10526;
    Interval _10556 = jet_mul_derivative(_9849, _9850, intervalFailed, optical_product_upper);
    Interval _9851 = Interval{ as_type<float>(as_type<uint>(_10556.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10556.lo) ^ 2147483648u) };
    Interval _10566 = jet_add_derivative(_9848, _9851, intervalFailed);
    Interval _9852 = _10566;
    Interval _9853 = _10487;
    Interval _10567 = jet_div_derivative(_9852, _9853, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9826 = Interval{ _10163, _10164 };
    Interval _9827 = _10528;
    Interval _10569 = imul(_9826, _9827, intervalFailed, optical_product_upper);
    Interval _9828 = Interval{ _10165, _10166 };
    Interval _9829 = _10528;
    Interval _10571 = jet_mul_derivative(_9828, _9829, intervalFailed, optical_product_upper);
    Interval _9830 = _10571;
    Interval _9831 = Interval{ _10163, _10164 };
    Interval _9832 = _10555;
    Interval _10573 = jet_mul_derivative(_9831, _9832, intervalFailed, optical_product_upper);
    Interval _9833 = _10573;
    Interval _10574 = jet_add_derivative(_9830, _9833, intervalFailed);
    Interval _9834 = Interval{ _10167, _10168 };
    Interval _9835 = _10528;
    Interval _10576 = jet_mul_derivative(_9834, _9835, intervalFailed, optical_product_upper);
    Interval _9836 = _10576;
    Interval _9837 = Interval{ _10163, _10164 };
    Interval _9838 = _10567;
    Interval _10578 = jet_mul_derivative(_9837, _9838, intervalFailed, optical_product_upper);
    Interval _9839 = _10578;
    Interval _10579 = jet_add_derivative(_9836, _9839, intervalFailed);
    Interval _9812 = Interval{ _10169, _10170 };
    Interval _9813 = _10528;
    Interval _10581 = imul(_9812, _9813, intervalFailed, optical_product_upper);
    Interval _9814 = Interval{ _10171, _10172 };
    Interval _9815 = _10528;
    Interval _10583 = jet_mul_derivative(_9814, _9815, intervalFailed, optical_product_upper);
    Interval _9816 = _10583;
    Interval _9817 = Interval{ _10169, _10170 };
    Interval _9818 = _10555;
    Interval _10585 = jet_mul_derivative(_9817, _9818, intervalFailed, optical_product_upper);
    Interval _9819 = _10585;
    Interval _10586 = jet_add_derivative(_9816, _9819, intervalFailed);
    Interval _9820 = Interval{ _10173, _10174 };
    Interval _9821 = _10528;
    Interval _10588 = jet_mul_derivative(_9820, _9821, intervalFailed, optical_product_upper);
    Interval _9822 = _10588;
    Interval _9823 = Interval{ _10169, _10170 };
    Interval _9824 = _10567;
    Interval _10590 = jet_mul_derivative(_9823, _9824, intervalFailed, optical_product_upper);
    Interval _9825 = _10590;
    Interval _10591 = jet_add_derivative(_9822, _9825, intervalFailed);
    Interval _9798 = Interval{ _10175, _10176 };
    Interval _9799 = _10528;
    Interval _10593 = imul(_9798, _9799, intervalFailed, optical_product_upper);
    Interval _9800 = Interval{ _10177, _10178 };
    Interval _9801 = _10528;
    Interval _10595 = jet_mul_derivative(_9800, _9801, intervalFailed, optical_product_upper);
    Interval _9802 = _10595;
    Interval _9803 = Interval{ _10175, _10176 };
    Interval _9804 = _10555;
    Interval _10597 = jet_mul_derivative(_9803, _9804, intervalFailed, optical_product_upper);
    Interval _9805 = _10597;
    Interval _10598 = jet_add_derivative(_9802, _9805, intervalFailed);
    Interval _9806 = Interval{ _10179, _10180 };
    Interval _9807 = _10528;
    Interval _10600 = jet_mul_derivative(_9806, _9807, intervalFailed, optical_product_upper);
    Interval _9808 = _10600;
    Interval _9809 = Interval{ _10175, _10176 };
    Interval _9810 = _10567;
    Interval _10602 = jet_mul_derivative(_9809, _9810, intervalFailed, optical_product_upper);
    Interval _9811 = _10602;
    Interval _10603 = jet_add_derivative(_9808, _9811, intervalFailed);
    Interval _9784 = _10581;
    Interval _9785 = ray.outgoing.z.v;
    Interval _10612 = imul(_9784, _9785, intervalFailed, optical_product_upper);
    Interval _9786 = _10586;
    Interval _9787 = ray.outgoing.z.v;
    Interval _10613 = jet_mul_derivative(_9786, _9787, intervalFailed, optical_product_upper);
    Interval _9788 = _10613;
    Interval _9789 = _10581;
    Interval _9790 = ray.outgoing.z.dx;
    Interval _10614 = jet_mul_derivative(_9789, _9790, intervalFailed, optical_product_upper);
    Interval _9791 = _10614;
    Interval _10615 = jet_add_derivative(_9788, _9791, intervalFailed);
    Interval _9792 = _10591;
    Interval _9793 = ray.outgoing.z.v;
    Interval _10616 = jet_mul_derivative(_9792, _9793, intervalFailed, optical_product_upper);
    Interval _9794 = _10616;
    Interval _9795 = _10581;
    Interval _9796 = ray.outgoing.z.dy;
    Interval _10617 = jet_mul_derivative(_9795, _9796, intervalFailed, optical_product_upper);
    Interval _9797 = _10617;
    Interval _10618 = jet_add_derivative(_9794, _9797, intervalFailed);
    Interval _9770 = _10593;
    Interval _9771 = ray.outgoing.y.v;
    Interval _10622 = imul(_9770, _9771, intervalFailed, optical_product_upper);
    Interval _9772 = _10598;
    Interval _9773 = ray.outgoing.y.v;
    Interval _10623 = jet_mul_derivative(_9772, _9773, intervalFailed, optical_product_upper);
    Interval _9774 = _10623;
    Interval _9775 = _10593;
    Interval _9776 = ray.outgoing.y.dx;
    Interval _10624 = jet_mul_derivative(_9775, _9776, intervalFailed, optical_product_upper);
    Interval _9777 = _10624;
    Interval _10625 = jet_add_derivative(_9774, _9777, intervalFailed);
    Interval _9778 = _10603;
    Interval _9779 = ray.outgoing.y.v;
    Interval _10626 = jet_mul_derivative(_9778, _9779, intervalFailed, optical_product_upper);
    Interval _9780 = _10626;
    Interval _9781 = _10593;
    Interval _9782 = ray.outgoing.y.dy;
    Interval _10627 = jet_mul_derivative(_9781, _9782, intervalFailed, optical_product_upper);
    Interval _9783 = _10627;
    Interval _10628 = jet_add_derivative(_9780, _9783, intervalFailed);
    Interval _9764 = _10612;
    Interval _9765 = Interval{ as_type<float>(as_type<uint>(_10622.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10622.lo) ^ 2147483648u) };
    Interval _10654 = iadd(_9764, _9765, intervalFailed);
    Interval _9766 = _10615;
    Interval _9767 = Interval{ as_type<float>(as_type<uint>(_10625.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10625.lo) ^ 2147483648u) };
    Interval _10656 = jet_add_derivative(_9766, _9767, intervalFailed);
    Interval _9768 = _10618;
    Interval _9769 = Interval{ as_type<float>(as_type<uint>(_10628.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10628.lo) ^ 2147483648u) };
    Interval _10658 = jet_add_derivative(_9768, _9769, intervalFailed);
    Interval _9750 = _10593;
    Interval _9751 = ray.outgoing.x.v;
    Interval _10662 = imul(_9750, _9751, intervalFailed, optical_product_upper);
    Interval _9752 = _10598;
    Interval _9753 = ray.outgoing.x.v;
    Interval _10663 = jet_mul_derivative(_9752, _9753, intervalFailed, optical_product_upper);
    Interval _9754 = _10663;
    Interval _9755 = _10593;
    Interval _9756 = ray.outgoing.x.dx;
    Interval _10664 = jet_mul_derivative(_9755, _9756, intervalFailed, optical_product_upper);
    Interval _9757 = _10664;
    Interval _10665 = jet_add_derivative(_9754, _9757, intervalFailed);
    Interval _9758 = _10603;
    Interval _9759 = ray.outgoing.x.v;
    Interval _10666 = jet_mul_derivative(_9758, _9759, intervalFailed, optical_product_upper);
    Interval _9760 = _10666;
    Interval _9761 = _10593;
    Interval _9762 = ray.outgoing.x.dy;
    Interval _10667 = jet_mul_derivative(_9761, _9762, intervalFailed, optical_product_upper);
    Interval _9763 = _10667;
    Interval _10668 = jet_add_derivative(_9760, _9763, intervalFailed);
    Interval _9736 = _10569;
    Interval _9737 = ray.outgoing.z.v;
    Interval _10672 = imul(_9736, _9737, intervalFailed, optical_product_upper);
    Interval _9738 = _10574;
    Interval _9739 = ray.outgoing.z.v;
    Interval _10673 = jet_mul_derivative(_9738, _9739, intervalFailed, optical_product_upper);
    Interval _9740 = _10673;
    Interval _9741 = _10569;
    Interval _9742 = ray.outgoing.z.dx;
    Interval _10674 = jet_mul_derivative(_9741, _9742, intervalFailed, optical_product_upper);
    Interval _9743 = _10674;
    Interval _10675 = jet_add_derivative(_9740, _9743, intervalFailed);
    Interval _9744 = _10579;
    Interval _9745 = ray.outgoing.z.v;
    Interval _10676 = jet_mul_derivative(_9744, _9745, intervalFailed, optical_product_upper);
    Interval _9746 = _10676;
    Interval _9747 = _10569;
    Interval _9748 = ray.outgoing.z.dy;
    Interval _10677 = jet_mul_derivative(_9747, _9748, intervalFailed, optical_product_upper);
    Interval _9749 = _10677;
    Interval _10678 = jet_add_derivative(_9746, _9749, intervalFailed);
    Interval _9730 = _10662;
    Interval _9731 = Interval{ as_type<float>(as_type<uint>(_10672.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10672.lo) ^ 2147483648u) };
    Interval _10704 = iadd(_9730, _9731, intervalFailed);
    Interval _9732 = _10665;
    Interval _9733 = Interval{ as_type<float>(as_type<uint>(_10675.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10675.lo) ^ 2147483648u) };
    Interval _10706 = jet_add_derivative(_9732, _9733, intervalFailed);
    Interval _9734 = _10668;
    Interval _9735 = Interval{ as_type<float>(as_type<uint>(_10678.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10678.lo) ^ 2147483648u) };
    Interval _10708 = jet_add_derivative(_9734, _9735, intervalFailed);
    Interval _9716 = _10569;
    Interval _9717 = ray.outgoing.y.v;
    Interval _10712 = imul(_9716, _9717, intervalFailed, optical_product_upper);
    Interval _9718 = _10574;
    Interval _9719 = ray.outgoing.y.v;
    Interval _10713 = jet_mul_derivative(_9718, _9719, intervalFailed, optical_product_upper);
    Interval _9720 = _10713;
    Interval _9721 = _10569;
    Interval _9722 = ray.outgoing.y.dx;
    Interval _10714 = jet_mul_derivative(_9721, _9722, intervalFailed, optical_product_upper);
    Interval _9723 = _10714;
    Interval _10715 = jet_add_derivative(_9720, _9723, intervalFailed);
    Interval _9724 = _10579;
    Interval _9725 = ray.outgoing.y.v;
    Interval _10716 = jet_mul_derivative(_9724, _9725, intervalFailed, optical_product_upper);
    Interval _9726 = _10716;
    Interval _9727 = _10569;
    Interval _9728 = ray.outgoing.y.dy;
    Interval _10717 = jet_mul_derivative(_9727, _9728, intervalFailed, optical_product_upper);
    Interval _9729 = _10717;
    Interval _10718 = jet_add_derivative(_9726, _9729, intervalFailed);
    Interval _9702 = _10581;
    Interval _9703 = ray.outgoing.x.v;
    Interval _10722 = imul(_9702, _9703, intervalFailed, optical_product_upper);
    Interval _9704 = _10586;
    Interval _9705 = ray.outgoing.x.v;
    Interval _10723 = jet_mul_derivative(_9704, _9705, intervalFailed, optical_product_upper);
    Interval _9706 = _10723;
    Interval _9707 = _10581;
    Interval _9708 = ray.outgoing.x.dx;
    Interval _10724 = jet_mul_derivative(_9707, _9708, intervalFailed, optical_product_upper);
    Interval _9709 = _10724;
    Interval _10725 = jet_add_derivative(_9706, _9709, intervalFailed);
    Interval _9710 = _10591;
    Interval _9711 = ray.outgoing.x.v;
    Interval _10726 = jet_mul_derivative(_9710, _9711, intervalFailed, optical_product_upper);
    Interval _9712 = _10726;
    Interval _9713 = _10581;
    Interval _9714 = ray.outgoing.x.dy;
    Interval _10727 = jet_mul_derivative(_9713, _9714, intervalFailed, optical_product_upper);
    Interval _9715 = _10727;
    Interval _10728 = jet_add_derivative(_9712, _9715, intervalFailed);
    Interval _9696 = _10712;
    Interval _9697 = Interval{ as_type<float>(as_type<uint>(_10722.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10722.lo) ^ 2147483648u) };
    Interval _10754 = iadd(_9696, _9697, intervalFailed);
    Interval _9698 = _10715;
    Interval _9699 = Interval{ as_type<float>(as_type<uint>(_10725.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10725.lo) ^ 2147483648u) };
    Interval _10756 = jet_add_derivative(_9698, _9699, intervalFailed);
    Interval _9700 = _10718;
    Interval _9701 = Interval{ as_type<float>(as_type<uint>(_10728.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10728.lo) ^ 2147483648u) };
    Interval _10758 = jet_add_derivative(_9700, _9701, intervalFailed);
    Interval _9682 = _10654;
    Interval _9683 = Interval{ focal, focal };
    Interval _10761 = imul(_9682, _9683, intervalFailed, optical_product_upper);
    Interval _9684 = _10656;
    Interval _9685 = Interval{ focal, focal };
    Interval _10763 = jet_mul_derivative(_9684, _9685, intervalFailed, optical_product_upper);
    Interval _9686 = _10763;
    Interval _9687 = _10654;
    Interval _9688 = Interval{ 0.0, 0.0 };
    Interval _10764 = jet_mul_derivative(_9687, _9688, intervalFailed, optical_product_upper);
    Interval _9689 = _10764;
    Interval _10765 = jet_add_derivative(_9686, _9689, intervalFailed);
    Interval _9690 = _10658;
    Interval _9691 = Interval{ focal, focal };
    Interval _10767 = jet_mul_derivative(_9690, _9691, intervalFailed, optical_product_upper);
    Interval _9692 = _10767;
    Interval _9693 = _10654;
    Interval _9694 = Interval{ 0.0, 0.0 };
    Interval _10768 = jet_mul_derivative(_9693, _9694, intervalFailed, optical_product_upper);
    Interval _9695 = _10768;
    Interval _10769 = jet_add_derivative(_9692, _9695, intervalFailed);
    Interval _9668 = _10704;
    Interval _9669 = Interval{ focal, focal };
    Interval _10771 = imul(_9668, _9669, intervalFailed, optical_product_upper);
    Interval _9670 = _10706;
    Interval _9671 = Interval{ focal, focal };
    Interval _10773 = jet_mul_derivative(_9670, _9671, intervalFailed, optical_product_upper);
    Interval _9672 = _10773;
    Interval _9673 = _10704;
    Interval _9674 = Interval{ 0.0, 0.0 };
    Interval _10774 = jet_mul_derivative(_9673, _9674, intervalFailed, optical_product_upper);
    Interval _9675 = _10774;
    Interval _10775 = jet_add_derivative(_9672, _9675, intervalFailed);
    Interval _9676 = _10708;
    Interval _9677 = Interval{ focal, focal };
    Interval _10777 = jet_mul_derivative(_9676, _9677, intervalFailed, optical_product_upper);
    Interval _9678 = _10777;
    Interval _9679 = _10704;
    Interval _9680 = Interval{ 0.0, 0.0 };
    Interval _10778 = jet_mul_derivative(_9679, _9680, intervalFailed, optical_product_upper);
    Interval _9681 = _10778;
    Interval _10779 = jet_add_derivative(_9678, _9681, intervalFailed);
    Interval _9654 = _10754;
    Interval _9655 = Interval{ focal, focal };
    Interval _10781 = imul(_9654, _9655, intervalFailed, optical_product_upper);
    Interval _9656 = _10756;
    Interval _9657 = Interval{ focal, focal };
    Interval _10783 = jet_mul_derivative(_9656, _9657, intervalFailed, optical_product_upper);
    Interval _9658 = _10783;
    Interval _9659 = _10754;
    Interval _9660 = Interval{ 0.0, 0.0 };
    Interval _10784 = jet_mul_derivative(_9659, _9660, intervalFailed, optical_product_upper);
    Interval _9661 = _10784;
    Interval _10785 = jet_add_derivative(_9658, _9661, intervalFailed);
    Interval _9662 = _10758;
    Interval _9663 = Interval{ focal, focal };
    Interval _10787 = jet_mul_derivative(_9662, _9663, intervalFailed, optical_product_upper);
    Interval _9664 = _10787;
    Interval _9665 = _10754;
    Interval _9666 = Interval{ 0.0, 0.0 };
    Interval _10788 = jet_mul_derivative(_9665, _9666, intervalFailed, optical_product_upper);
    Interval _9667 = _10788;
    Interval _10789 = jet_add_derivative(_9664, _9667, intervalFailed);
    if (omitted == 0u)
    {
        e0 = OpticalJet{ _10771, _10775, _10779 };
        e1 = OpticalJet{ _10781, _10785, _10789 };
    }
    else
    {
        if (omitted == 1u)
        {
            e0 = OpticalJet{ _10781, _10785, _10789 };
            e1 = OpticalJet{ _10761, _10765, _10769 };
        }
        else
        {
            e0 = OpticalJet{ _10761, _10765, _10769 };
            e1 = OpticalJet{ _10771, _10775, _10779 };
        }
    }
    bool _10803;
    if (!intervalFailed)
    {
        _10803 = jetBranchKnown;
    }
    else
    {
        _10803 = false;
    }
    return _10803;
}

static inline __attribute__((always_inline))
OpticalLocalRoot optical_local_root(thread const float4& box, thread const float2& centre, thread const Interval& f0, thread const Interval& f1, thread const Interval& j00, thread const Interval& j01, thread const Interval& j10, thread const Interval& j11, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    bool _10822;
    if (!intervalFailed)
    {
        _10822 = !jetBranchKnown;
    }
    else
    {
        _10822 = true;
    }
    bool _10831;
    if (!_10822)
    {
        bool4 _10825 = isnan(box);
        bool4 _10826 = isinf(box);
        _10831 = !all(not(bool4(_10825.x || _10826.x, _10825.y || _10826.y, _10825.z || _10826.z, _10825.w || _10826.w)));
    }
    else
    {
        _10831 = true;
    }
    bool _10840;
    if (!_10831)
    {
        bool2 _10834 = isnan(centre);
        bool2 _10835 = isinf(centre);
        _10840 = !all(not(bool2(_10834.x || _10835.x, _10834.y || _10835.y)));
    }
    else
    {
        _10840 = true;
    }
    bool _10848;
    if (!_10840)
    {
        _10848 = any(box.xy >= box.zw);
    }
    else
    {
        _10848 = true;
    }
    bool _10855;
    if (!_10848)
    {
        _10855 = any(centre <= box.xy);
    }
    else
    {
        _10855 = true;
    }
    bool _10862;
    if (!_10855)
    {
        _10862 = any(centre >= box.zw);
    }
    else
    {
        _10862 = true;
    }
    bool _10883;
    if (!_10862)
    {
        bool _10877;
        if (!(isnan(f0.lo) || isinf(f0.lo)))
        {
            _10877 = !(isnan(f0.hi) || isinf(f0.hi));
        }
        else
        {
            _10877 = false;
        }
        bool _10881;
        if (_10877)
        {
            _10881 = f0.lo <= f0.hi;
        }
        else
        {
            _10881 = false;
        }
        _10883 = !_10881;
    }
    else
    {
        _10883 = true;
    }
    bool _10904;
    if (!_10883)
    {
        bool _10898;
        if (!(isnan(f1.lo) || isinf(f1.lo)))
        {
            _10898 = !(isnan(f1.hi) || isinf(f1.hi));
        }
        else
        {
            _10898 = false;
        }
        bool _10902;
        if (_10898)
        {
            _10902 = f1.lo <= f1.hi;
        }
        else
        {
            _10902 = false;
        }
        _10904 = !_10902;
    }
    else
    {
        _10904 = true;
    }
    bool _10925;
    if (!_10904)
    {
        bool _10919;
        if (!(isnan(j00.lo) || isinf(j00.lo)))
        {
            _10919 = !(isnan(j00.hi) || isinf(j00.hi));
        }
        else
        {
            _10919 = false;
        }
        bool _10923;
        if (_10919)
        {
            _10923 = j00.lo <= j00.hi;
        }
        else
        {
            _10923 = false;
        }
        _10925 = !_10923;
    }
    else
    {
        _10925 = true;
    }
    bool _10946;
    if (!_10925)
    {
        bool _10940;
        if (!(isnan(j01.lo) || isinf(j01.lo)))
        {
            _10940 = !(isnan(j01.hi) || isinf(j01.hi));
        }
        else
        {
            _10940 = false;
        }
        bool _10944;
        if (_10940)
        {
            _10944 = j01.lo <= j01.hi;
        }
        else
        {
            _10944 = false;
        }
        _10946 = !_10944;
    }
    else
    {
        _10946 = true;
    }
    bool _10967;
    if (!_10946)
    {
        bool _10961;
        if (!(isnan(j10.lo) || isinf(j10.lo)))
        {
            _10961 = !(isnan(j10.hi) || isinf(j10.hi));
        }
        else
        {
            _10961 = false;
        }
        bool _10965;
        if (_10961)
        {
            _10965 = j10.lo <= j10.hi;
        }
        else
        {
            _10965 = false;
        }
        _10967 = !_10965;
    }
    else
    {
        _10967 = true;
    }
    bool _10988;
    if (!_10967)
    {
        bool _10982;
        if (!(isnan(j11.lo) || isinf(j11.lo)))
        {
            _10982 = !(isnan(j11.hi) || isinf(j11.hi));
        }
        else
        {
            _10982 = false;
        }
        bool _10986;
        if (_10982)
        {
            _10986 = j11.lo <= j11.hi;
        }
        else
        {
            _10986 = false;
        }
        _10988 = !_10986;
    }
    else
    {
        _10988 = true;
    }
    if (_10988)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _1763 = spvFMul(spvFAdd(j00.lo, j00.hi), 0.5);
    float _1765 = spvFMul(spvFAdd(j01.lo, j01.hi), 0.5);
    float _1767 = spvFMul(spvFAdd(j10.lo, j10.hi), 0.5);
    float _1769 = spvFMul(spvFAdd(j11.lo, j11.hi), 0.5);
    float4 _11005 = float4(_1763, _1765, _1767, _1769);
    float _1772 = spvFSub(spvFMul(_1763, _1769), spvFMul(_1765, _1767));
    bool4 _11006 = isnan(_11005);
    bool4 _11007 = isinf(_11005);
    bool _11014;
    if (all(not(bool4(_11006.x || _11007.x, _11006.y || _11007.y, _11006.z || _11007.z, _11006.w || _11007.w))))
    {
        _11014 = isnan(_1772) || isinf(_1772);
    }
    else
    {
        _11014 = true;
    }
    bool _11017;
    if (!_11014)
    {
        _11017 = _1772 == 0.0;
    }
    else
    {
        _11017 = true;
    }
    if (_11017)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float4 _1775 = float4(_1769, -_1765, -_1767, _1763) / float4(_1772);
    bool4 _11020 = isnan(_1775);
    bool4 _11021 = isinf(_1775);
    bool _11052;
    if (all(not(bool4(_11020.x || _11021.x, _11020.y || _11021.y, _11020.z || _11021.z, _11020.w || _11021.w))))
    {
        float _11025 = _1775.x;
        Interval param_var_a = Interval{ _11025, _11025 };
        float _11027 = _1775.w;
        Interval param_var_b = Interval{ _11027, _11027 };
        Interval _11029 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        float _11030 = _1775.y;
        Interval param_var_a_1 = Interval{ _11030, _11030 };
        float _11032 = _1775.z;
        Interval param_var_b_1 = Interval{ _11032, _11032 };
        Interval _11034 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        Interval _10816 = _11029;
        Interval _10817 = Interval{ as_type<float>(as_type<uint>(_11034.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11034.lo) ^ 2147483648u) };
        Interval _11044 = iadd(_10816, _10817, intervalFailed);
        bool _11051;
        if (_11044.lo <= 0.0)
        {
            _11051 = _11044.hi >= 0.0;
        }
        else
        {
            _11051 = false;
        }
        _11052 = _11051;
    }
    else
    {
        _11052 = true;
    }
    if (_11052)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _11053 = _1775.x;
    Interval param_var_a_2 = Interval{ _11053, _11053 };
    Interval param_var_b_2 = j00;
    Interval _11056 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval param_var_a_3 = _11056;
    float _11057 = _1775.y;
    Interval param_var_a_4 = Interval{ _11057, _11057 };
    Interval param_var_b_3 = j10;
    Interval _11060 = imul(param_var_a_4, param_var_b_3, intervalFailed, optical_product_upper);
    Interval param_var_b_4 = _11060;
    Interval _11061 = iadd(param_var_a_3, param_var_b_4, intervalFailed);
    Interval _10814 = Interval{ 1.0, 1.0 };
    Interval _10815 = Interval{ as_type<float>(as_type<uint>(_11061.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11061.lo) ^ 2147483648u) };
    Interval _11071 = iadd(_10814, _10815, intervalFailed);
    float _11072 = _1775.x;
    Interval param_var_a_5 = Interval{ _11072, _11072 };
    Interval param_var_b_5 = j01;
    Interval _11075 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval param_var_a_6 = _11075;
    float _11076 = _1775.y;
    Interval param_var_a_7 = Interval{ _11076, _11076 };
    Interval param_var_b_6 = j11;
    Interval _11079 = imul(param_var_a_7, param_var_b_6, intervalFailed, optical_product_upper);
    Interval param_var_b_7 = _11079;
    Interval _11080 = iadd(param_var_a_6, param_var_b_7, intervalFailed);
    float _11085 = as_type<float>(as_type<uint>(_11080.hi) ^ 2147483648u);
    float _11088 = as_type<float>(as_type<uint>(_11080.lo) ^ 2147483648u);
    float _11089 = _1775.z;
    Interval param_var_a_8 = Interval{ _11089, _11089 };
    Interval param_var_b_8 = j00;
    Interval _11092 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _11092;
    float _11093 = _1775.w;
    Interval param_var_a_10 = Interval{ _11093, _11093 };
    Interval param_var_b_9 = j10;
    Interval _11096 = imul(param_var_a_10, param_var_b_9, intervalFailed, optical_product_upper);
    Interval param_var_b_10 = _11096;
    Interval _11097 = iadd(param_var_a_9, param_var_b_10, intervalFailed);
    float _11102 = as_type<float>(as_type<uint>(_11097.hi) ^ 2147483648u);
    float _11105 = as_type<float>(as_type<uint>(_11097.lo) ^ 2147483648u);
    float _11106 = _1775.z;
    Interval param_var_a_11 = Interval{ _11106, _11106 };
    Interval param_var_b_11 = j01;
    Interval _11109 = imul(param_var_a_11, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _11109;
    float _11110 = _1775.w;
    Interval param_var_a_13 = Interval{ _11110, _11110 };
    Interval param_var_b_12 = j11;
    Interval _11113 = imul(param_var_a_13, param_var_b_12, intervalFailed, optical_product_upper);
    Interval param_var_b_13 = _11113;
    Interval _11114 = iadd(param_var_a_12, param_var_b_13, intervalFailed);
    Interval _10812 = Interval{ 1.0, 1.0 };
    Interval _10813 = Interval{ as_type<float>(as_type<uint>(_11114.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11114.lo) ^ 2147483648u) };
    Interval _11124 = iadd(_10812, _10813, intervalFailed);
    Interval _10810 = Interval{ box.x, box.z };
    Interval _10811 = Interval{ as_type<float>(as_type<uint>(centre.x) ^ 2147483648u), as_type<float>(as_type<uint>(centre.x) ^ 2147483648u) };
    Interval _11139 = iadd(_10810, _10811, intervalFailed);
    Interval _10808 = Interval{ box.y, box.w };
    Interval _10809 = Interval{ as_type<float>(as_type<uint>(centre.y) ^ 2147483648u), as_type<float>(as_type<uint>(centre.y) ^ 2147483648u) };
    Interval _11154 = iadd(_10808, _10809, intervalFailed);
    float _11157 = _1775.x;
    Interval param_var_a_14 = Interval{ _11157, _11157 };
    Interval param_var_b_14 = f0;
    Interval _11160 = imul(param_var_a_14, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_15 = _11160;
    float _11161 = _1775.y;
    Interval param_var_a_16 = Interval{ _11161, _11161 };
    Interval param_var_b_15 = f1;
    Interval _11164 = imul(param_var_a_16, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _11164;
    Interval _11165 = iadd(param_var_a_15, param_var_b_16, intervalFailed);
    Interval _10806 = Interval{ centre.x, centre.x };
    Interval _10807 = Interval{ as_type<float>(as_type<uint>(_11165.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11165.lo) ^ 2147483648u) };
    Interval _11176 = iadd(_10806, _10807, intervalFailed);
    float _11179 = _1775.z;
    Interval param_var_a_17 = Interval{ _11179, _11179 };
    Interval param_var_b_17 = f0;
    Interval _11182 = imul(param_var_a_17, param_var_b_17, intervalFailed, optical_product_upper);
    Interval param_var_a_18 = _11182;
    float _11183 = _1775.w;
    Interval param_var_a_19 = Interval{ _11183, _11183 };
    Interval param_var_b_18 = f1;
    Interval _11186 = imul(param_var_a_19, param_var_b_18, intervalFailed, optical_product_upper);
    Interval param_var_b_19 = _11186;
    Interval _11187 = iadd(param_var_a_18, param_var_b_19, intervalFailed);
    Interval _10804 = Interval{ centre.y, centre.y };
    Interval _10805 = Interval{ as_type<float>(as_type<uint>(_11187.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11187.lo) ^ 2147483648u) };
    Interval _11198 = iadd(_10804, _10805, intervalFailed);
    Interval param_var_a_20 = _11176;
    Interval param_var_a_21 = _11071;
    Interval param_var_b_20 = _11139;
    Interval _11199 = imul(param_var_a_21, param_var_b_20, intervalFailed, optical_product_upper);
    Interval param_var_a_22 = _11199;
    Interval param_var_a_23 = Interval{ _11085, _11088 };
    Interval param_var_b_21 = _11154;
    Interval _11201 = imul(param_var_a_23, param_var_b_21, intervalFailed, optical_product_upper);
    Interval param_var_b_22 = _11201;
    Interval _11202 = iadd(param_var_a_22, param_var_b_22, intervalFailed);
    Interval param_var_b_23 = _11202;
    Interval _11203 = iadd(param_var_a_20, param_var_b_23, intervalFailed);
    Interval param_var_a_24 = _11198;
    Interval param_var_a_25 = Interval{ _11102, _11105 };
    Interval param_var_b_24 = _11139;
    Interval _11207 = imul(param_var_a_25, param_var_b_24, intervalFailed, optical_product_upper);
    Interval param_var_a_26 = _11207;
    Interval param_var_a_27 = _11124;
    Interval param_var_b_25 = _11154;
    Interval _11208 = imul(param_var_a_27, param_var_b_25, intervalFailed, optical_product_upper);
    Interval param_var_b_26 = _11208;
    Interval _11209 = iadd(param_var_a_26, param_var_b_26, intervalFailed);
    Interval param_var_b_27 = _11209;
    Interval _11210 = iadd(param_var_a_24, param_var_b_27, intervalFailed);
    bool _11219;
    if (_11071.lo <= 0.0)
    {
        _11219 = _11071.hi >= 0.0;
    }
    else
    {
        _11219 = false;
    }
    float _11226;
    if (_11219)
    {
        _11226 = 0.0;
    }
    else
    {
        _11226 = precise::min(abs(_11071.lo), abs(_11071.hi));
    }
    Interval param_var_a_28 = Interval{ _11226, precise::max(abs(_11071.lo), abs(_11071.hi)) };
    bool _11235;
    if (_11085 <= 0.0)
    {
        _11235 = _11088 >= 0.0;
    }
    else
    {
        _11235 = false;
    }
    float _11242;
    if (_11235)
    {
        _11242 = 0.0;
    }
    else
    {
        _11242 = precise::min(abs(_11085), abs(_11088));
    }
    Interval param_var_b_28 = Interval{ _11242, precise::max(abs(_11085), abs(_11088)) };
    Interval _11247 = iadd(param_var_a_28, param_var_b_28, intervalFailed);
    bool _11254;
    if (_11102 <= 0.0)
    {
        _11254 = _11105 >= 0.0;
    }
    else
    {
        _11254 = false;
    }
    float _11261;
    if (_11254)
    {
        _11261 = 0.0;
    }
    else
    {
        _11261 = precise::min(abs(_11102), abs(_11105));
    }
    Interval param_var_a_29 = Interval{ _11261, precise::max(abs(_11102), abs(_11105)) };
    bool _11272;
    if (_11124.lo <= 0.0)
    {
        _11272 = _11124.hi >= 0.0;
    }
    else
    {
        _11272 = false;
    }
    float _11279;
    if (_11272)
    {
        _11279 = 0.0;
    }
    else
    {
        _11279 = precise::min(abs(_11124.lo), abs(_11124.hi));
    }
    Interval param_var_b_29 = Interval{ _11279, precise::max(abs(_11124.lo), abs(_11124.hi)) };
    Interval _11284 = iadd(param_var_a_29, param_var_b_29, intervalFailed);
    float _11287 = precise::max(_11247.lo, _11284.lo);
    float _11288 = precise::max(_11247.hi, _11284.hi);
    bool _11293;
    if (!intervalFailed)
    {
        _11293 = !jetBranchKnown;
    }
    else
    {
        _11293 = true;
    }
    bool _11313;
    if (!_11293)
    {
        bool _11307;
        if (!(isnan(_11203.lo) || isinf(_11203.lo)))
        {
            _11307 = !(isnan(_11203.hi) || isinf(_11203.hi));
        }
        else
        {
            _11307 = false;
        }
        bool _11311;
        if (_11307)
        {
            _11311 = _11203.lo <= _11203.hi;
        }
        else
        {
            _11311 = false;
        }
        _11313 = !_11311;
    }
    else
    {
        _11313 = true;
    }
    bool _11333;
    if (!_11313)
    {
        bool _11327;
        if (!(isnan(_11210.lo) || isinf(_11210.lo)))
        {
            _11327 = !(isnan(_11210.hi) || isinf(_11210.hi));
        }
        else
        {
            _11327 = false;
        }
        bool _11331;
        if (_11327)
        {
            _11331 = _11210.lo <= _11210.hi;
        }
        else
        {
            _11331 = false;
        }
        _11333 = !_11331;
    }
    else
    {
        _11333 = true;
    }
    bool _11351;
    if (!_11333)
    {
        bool _11345;
        if (!(isnan(_11287) || isinf(_11287)))
        {
            _11345 = !(isnan(_11288) || isinf(_11288));
        }
        else
        {
            _11345 = false;
        }
        bool _11349;
        if (_11345)
        {
            _11349 = _11287 <= _11288;
        }
        else
        {
            _11349 = false;
        }
        _11351 = !_11349;
    }
    else
    {
        _11351 = true;
    }
    if (_11351)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    bool _11359;
    if ((isunordered(_11203.hi, box.x) || _11203.hi >= box.x))
    {
        _11359 = _11203.lo > box.z;
    }
    else
    {
        _11359 = true;
    }
    bool _11364;
    if (!_11359)
    {
        _11364 = _11210.hi < box.y;
    }
    else
    {
        _11364 = true;
    }
    bool _11369;
    if (!_11364)
    {
        _11369 = _11210.lo > box.w;
    }
    else
    {
        _11369 = true;
    }
    uint _11388;
    if (_11369)
    {
        _11388 = 2u;
    }
    else
    {
        bool _11374;
        if (_11288 < 1.0)
        {
            _11374 = _11203.lo > box.x;
        }
        else
        {
            _11374 = false;
        }
        bool _11378;
        if (_11374)
        {
            _11378 = _11203.hi < box.z;
        }
        else
        {
            _11378 = false;
        }
        bool _11382;
        if (_11378)
        {
            _11382 = _11210.lo > box.y;
        }
        else
        {
            _11382 = false;
        }
        bool _11386;
        if (_11382)
        {
            _11386 = _11210.hi < box.w;
        }
        else
        {
            _11386 = false;
        }
        uint _11387;
        if (_11386)
        {
            _11387 = 1u;
        }
        else
        {
            _11387 = 0u;
        }
        _11388 = _11387;
    }
    return OpticalLocalRoot{ _11388, float4(_11203.lo, _11210.lo, _11203.hi, _11210.hi), _11288, short(true) };
}

static inline __attribute__((always_inline))
bool optical_intersect_inherited_root(thread OpticalLocalRoot& proved, thread const OpticalLocalRoot& next, thread bool& intervalFailed, thread bool& jetBranchKnown)
{
    bool _11396;
    if (proved.status == 1u)
    {
        _11396 = !bool(proved.evaluated);
    }
    else
    {
        _11396 = true;
    }
    bool _11401;
    if (!_11396)
    {
        _11401 = !bool(next.evaluated);
    }
    else
    {
        _11401 = true;
    }
    bool _11406;
    if (!_11401)
    {
        _11406 = next.status == 2u;
    }
    else
    {
        _11406 = true;
    }
    bool _11409;
    if (!_11406)
    {
        _11409 = intervalFailed;
    }
    else
    {
        _11409 = true;
    }
    bool _11413;
    if (!_11409)
    {
        _11413 = !jetBranchKnown;
    }
    else
    {
        _11413 = true;
    }
    if (_11413)
    {
        return false;
    }
    float4 _11432 = float4(precise::max(proved.enclosure.xy, next.enclosure.xy), precise::min(proved.enclosure.zw, next.enclosure.zw));
    bool4 _11433 = isnan(_11432);
    bool4 _11434 = isinf(_11432);
    bool _11442;
    if (all(not(bool4(_11433.x || _11434.x, _11433.y || _11434.y, _11433.z || _11434.z, _11433.w || _11434.w))))
    {
        _11442 = any(_11432.xy > _11432.zw);
    }
    else
    {
        _11442 = true;
    }
    if (_11442)
    {
        return false;
    }
    proved.enclosure = _11432;
    return true;
}

static inline __attribute__((always_inline))
void write_optical_local_root(thread const uint& outAt, thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTarget, thread const uint& refinements, device type_RWByteAddressBuffer& results, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    float2 _1709 = spvFAdd(box.xy, box.zw) * 0.5;
    OpticalJet a = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalJet b = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalLocalRoot root;
    root.status = 0u;
    root.enclosure = float4(0.0);
    root.contraction = 1000000015047466219876688855040.0;
    root.evaluated = short(false);
    bool _4104 = min(refinements, 2u) == 2u;
    float _4140;
    float _4142;
    float _4144;
    float _4146;
    _4140 = 0.0;
    _4142 = 0.0;
    _4144 = 0.0;
    _4146 = 0.0;
    OpticalJetRay ray;
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
    float _4132;
    float _4134;
    float _4136;
    uint _4138;
    float _4141;
    float _4143;
    float _4145;
    float _4147;
    uint _4485;
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
    float _4131 = 0.0;
    float _4133 = 0.0;
    float _4135 = 0.0;
    uint _4137 = 0u;
    uint _4139 = 0u;
    for (;;)
    {
        if (_4139 < (2u + min(refinements, 2u)))
        {
            bool _4155;
            if (_4139 >= 2u)
            {
                _4155 = root.status != 1u;
            }
            else
            {
                _4155 = false;
            }
            if (_4155)
            {
                _4485 = _4137;
                break;
            }
            float4 _4157 = root.enclosure;
            float2 _4161;
            if (_4139 >= 2u)
            {
                _4161 = spvFAdd(_4157.xy, _4157.zw) * 0.5;
            }
            else
            {
                _4161 = _1709;
            }
            intervalFailed = false;
            jetBranchKnown = true;
            float4 _4172;
            if (_4139 == 0u)
            {
                _4172 = box;
            }
            else
            {
                bool _4165;
                if (_4139 == 2u)
                {
                    _4165 = _4104;
                }
                else
                {
                    _4165 = false;
                }
                float4 _4171;
                if (_4165)
                {
                    _4171 = _4157;
                }
                else
                {
                    _4171 = float4(_4161.xyxy);
                }
                _4172 = _4171;
            }
            float4 param_var_box = _4172;
            ReflectionRoughFrame param_var_receiver = receiver;
            ReflectionLiquidFrame param_var_liquid = liquid;
            spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
            uint4 param_var_control = control;
            bool _4177 = optical_jet_forward(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, ray, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4177)
            {
                if (_4139 >= 2u)
                {
                    _4485 = _4137;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 9u : 8u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4139 == 0u)
            {
                float3 midpoint = float3(spvFMul(spvFAdd(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi), 0.5));
                int _4207;
                if (abs(midpoint.x) > abs(midpoint.y))
                {
                    _4207 = 0;
                }
                else
                {
                    _4207 = 1;
                }
                uint _4208 = uint(_4207);
                uint _4216;
                if (abs(midpoint.z) > abs(midpoint[_4208]))
                {
                    _4216 = 2u;
                }
                else
                {
                    _4216 = _4208;
                }
                uint _4222 = (outAt + 32u) >> 2u;
                uint2 _4224 = as_type<uint2>(float2(ray.origin.x.v.lo, ray.origin.x.v.hi));
                results._m0[_4222] = _4224.x;
                results._m0[_4222 + 1u] = _4224.y;
                uint _4234 = (outAt + 40u) >> 2u;
                uint2 _4236 = as_type<uint2>(float2(ray.origin.y.v.lo, ray.origin.y.v.hi));
                results._m0[_4234] = _4236.x;
                results._m0[_4234 + 1u] = _4236.y;
                uint _4246 = (outAt + 48u) >> 2u;
                uint2 _4248 = as_type<uint2>(float2(ray.origin.z.v.lo, ray.origin.z.v.hi));
                results._m0[_4246] = _4248.x;
                results._m0[_4246 + 1u] = _4248.y;
                uint _4258 = (outAt + 56u) >> 2u;
                uint2 _4260 = as_type<uint2>(float2(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi));
                results._m0[_4258] = _4260.x;
                results._m0[_4258 + 1u] = _4260.y;
                uint _4270 = (outAt + 64u) >> 2u;
                uint2 _4272 = as_type<uint2>(float2(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi));
                results._m0[_4270] = _4272.x;
                results._m0[_4270 + 1u] = _4272.y;
                uint _4282 = (outAt + 72u) >> 2u;
                uint2 _4284 = as_type<uint2>(float2(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi));
                results._m0[_4282] = _4284.x;
                results._m0[_4282 + 1u] = _4284.y;
                uint _4294 = (outAt + 80u) >> 2u;
                uint2 _4296 = as_type<uint2>(float2(ray.depth.v.lo, ray.depth.v.hi));
                results._m0[_4294] = _4296.x;
                results._m0[_4294 + 1u] = _4296.y;
                uint _4306 = (outAt + 88u) >> 2u;
                uint2 _4308 = as_type<uint2>(float2(ray.bias0.v.lo, ray.bias0.v.hi));
                results._m0[_4306] = _4308.x;
                results._m0[_4306 + 1u] = _4308.y;
                _4138 = _4216;
            }
            else
            {
                _4138 = _4137;
            }
            OpticalJetRay param_var_ray = ray;
            Interval3 param_var_target = target;
            bool param_var_finiteTerminal = finiteTarget;
            float param_var_focal = precise::max(receiver.projection.x, receiver.projection.y);
            uint param_var_omitted = _4138;
            bool _4323 = optical_jet_residual(param_var_ray, param_var_target, param_var_finiteTerminal, param_var_focal, param_var_omitted, a, b, intervalFailed, optical_product_upper, interval_divide_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4323)
            {
                if (_4139 >= 2u)
                {
                    _4485 = _4138;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 17u : 16u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4139 == 0u)
            {
                uint _4338 = (outAt + 96u) >> 2u;
                uint2 _4340 = as_type<uint2>(float2(a.v.lo, a.v.hi));
                results._m0[_4338] = _4340.x;
                results._m0[_4338 + 1u] = _4340.y;
                uint _4350 = (outAt + 104u) >> 2u;
                uint2 _4352 = as_type<uint2>(float2(b.v.lo, b.v.hi));
                results._m0[_4350] = _4352.x;
                results._m0[_4350 + 1u] = _4352.y;
                uint _4376 = (outAt + 112u) >> 2u;
                uint2 _4378 = as_type<uint2>(float2(a.dx.lo, a.dx.hi));
                results._m0[_4376] = _4378.x;
                results._m0[_4376 + 1u] = _4378.y;
                uint _4386 = (outAt + 120u) >> 2u;
                uint2 _4388 = as_type<uint2>(float2(a.dy.lo, a.dy.hi));
                results._m0[_4386] = _4388.x;
                results._m0[_4386 + 1u] = _4388.y;
                uint _4396 = (outAt + 128u) >> 2u;
                uint2 _4398 = as_type<uint2>(float2(b.dx.lo, b.dx.hi));
                results._m0[_4396] = _4398.x;
                results._m0[_4396 + 1u] = _4398.y;
                uint _4406 = (outAt + 136u) >> 2u;
                uint2 _4408 = as_type<uint2>(float2(b.dy.lo, b.dy.hi));
                results._m0[_4406] = _4408.x;
                results._m0[_4406 + 1u] = _4408.y;
                _4106 = _4105;
                _4108 = _4107;
                _4110 = _4109;
                _4112 = _4111;
                _4114 = _4113;
                _4116 = _4115;
                _4118 = _4117;
                _4120 = _4119;
                _4122 = b.dy.lo;
                _4124 = b.dy.hi;
                _4126 = b.dx.lo;
                _4128 = b.dx.hi;
                _4130 = a.dy.lo;
                _4132 = a.dy.hi;
                _4134 = a.dx.lo;
                _4136 = a.dx.hi;
                _4141 = _4140;
                _4143 = _4142;
                _4145 = _4144;
                _4147 = _4146;
            }
            else
            {
                float _4473;
                float _4474;
                float _4475;
                float _4476;
                float _4477;
                float _4478;
                float _4479;
                float _4480;
                float _4481;
                float _4482;
                float _4483;
                float _4484;
                if (_4139 == 1u)
                {
                    float4 param_var_box_1 = box;
                    float2 param_var_centre = _1709;
                    Interval param_var_f0 = a.v;
                    Interval param_var_f1 = b.v;
                    Interval param_var_j00 = Interval{ _4133, _4135 };
                    Interval param_var_j01 = Interval{ _4129, _4131 };
                    Interval param_var_j10 = Interval{ _4125, _4127 };
                    Interval param_var_j11 = Interval{ _4121, _4123 };
                    OpticalLocalRoot _4427 = optical_local_root(param_var_box_1, param_var_centre, param_var_f0, param_var_f1, param_var_j00, param_var_j01, param_var_j10, param_var_j11, intervalFailed, optical_product_upper, jetBranchKnown);
                    root = _4427;
                    _4473 = _4105;
                    _4474 = _4107;
                    _4475 = _4109;
                    _4476 = _4111;
                    _4477 = _4113;
                    _4478 = _4115;
                    _4479 = _4117;
                    _4480 = _4119;
                    _4481 = b.v.lo;
                    _4482 = b.v.hi;
                    _4483 = a.v.lo;
                    _4484 = a.v.hi;
                }
                else
                {
                    bool _4429;
                    if (_4139 == 2u)
                    {
                        _4429 = _4104;
                    }
                    else
                    {
                        _4429 = false;
                    }
                    float _4465;
                    float _4466;
                    float _4467;
                    float _4468;
                    float _4469;
                    float _4470;
                    float _4471;
                    float _4472;
                    if (_4429)
                    {
                        _4465 = b.dy.lo;
                        _4466 = b.dy.hi;
                        _4467 = b.dx.lo;
                        _4468 = b.dx.hi;
                        _4469 = a.dy.lo;
                        _4470 = a.dy.hi;
                        _4471 = a.dx.lo;
                        _4472 = a.dx.hi;
                    }
                    else
                    {
                        float _4446;
                        float _4447;
                        float _4448;
                        float _4449;
                        float _4450;
                        float _4451;
                        float _4452;
                        float _4453;
                        if (_4104)
                        {
                            _4446 = _4105;
                            _4447 = _4107;
                            _4448 = _4109;
                            _4449 = _4111;
                            _4450 = _4113;
                            _4451 = _4115;
                            _4452 = _4117;
                            _4453 = _4119;
                        }
                        else
                        {
                            _4446 = _4121;
                            _4447 = _4123;
                            _4448 = _4125;
                            _4449 = _4127;
                            _4450 = _4129;
                            _4451 = _4131;
                            _4452 = _4133;
                            _4453 = _4135;
                        }
                        float4 param_var_box_2 = _4157;
                        float2 param_var_centre_1 = _4161;
                        Interval param_var_f0_1 = a.v;
                        Interval param_var_f1_1 = b.v;
                        Interval param_var_j00_1 = Interval{ _4452, _4453 };
                        Interval param_var_j01_1 = Interval{ _4450, _4451 };
                        Interval param_var_j10_1 = Interval{ _4448, _4449 };
                        Interval param_var_j11_1 = Interval{ _4446, _4447 };
                        OpticalLocalRoot _4462 = optical_local_root(param_var_box_2, param_var_centre_1, param_var_f0_1, param_var_f1_1, param_var_j00_1, param_var_j01_1, param_var_j10_1, param_var_j11_1, intervalFailed, optical_product_upper, jetBranchKnown);
                        OpticalLocalRoot param_var_next = _4462;
                        bool _4463 = optical_intersect_inherited_root(root, param_var_next, intervalFailed, jetBranchKnown);
                        if (!_4463)
                        {
                            _4485 = _4138;
                            break;
                        }
                        _4465 = _4105;
                        _4466 = _4107;
                        _4467 = _4109;
                        _4468 = _4111;
                        _4469 = _4113;
                        _4470 = _4115;
                        _4471 = _4117;
                        _4472 = _4119;
                    }
                    _4473 = _4465;
                    _4474 = _4466;
                    _4475 = _4467;
                    _4476 = _4468;
                    _4477 = _4469;
                    _4478 = _4470;
                    _4479 = _4471;
                    _4480 = _4472;
                    _4481 = _4140;
                    _4482 = _4142;
                    _4483 = _4144;
                    _4484 = _4146;
                }
                _4106 = _4473;
                _4108 = _4474;
                _4110 = _4475;
                _4112 = _4476;
                _4114 = _4477;
                _4116 = _4478;
                _4118 = _4479;
                _4120 = _4480;
                _4122 = _4121;
                _4124 = _4123;
                _4126 = _4125;
                _4128 = _4127;
                _4130 = _4129;
                _4132 = _4131;
                _4134 = _4133;
                _4136 = _4135;
                _4141 = _4481;
                _4143 = _4482;
                _4145 = _4483;
                _4147 = _4484;
            }
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
            _4133 = _4134;
            _4135 = _4136;
            _4137 = _4138;
            _4139++;
            _4140 = _4141;
            _4142 = _4143;
            _4144 = _4145;
            _4146 = _4147;
            continue;
        }
        else
        {
            _4485 = _4137;
            break;
        }
    }
    uint _4487 = outAt >> 2u;
    results._m0[_4487] = 1u;
    results._m0[_4487 + 1u] = root.status;
    results._m0[_4487 + 2u] = _4485;
    results._m0[_4487 + 3u] = 0u;
    uint _4495 = (outAt + 16u) >> 2u;
    uint4 _4497 = as_type<uint4>(box);
    results._m0[_4495] = _4497.x;
    results._m0[_4495 + 1u] = _4497.y;
    results._m0[_4495 + 2u] = _4497.z;
    results._m0[_4495 + 3u] = _4497.w;
    uint _4507 = (outAt + 144u) >> 2u;
    uint4 _4510 = as_type<uint4>(root.enclosure);
    results._m0[_4507] = _4510.x;
    results._m0[_4507 + 1u] = _4510.y;
    results._m0[_4507 + 2u] = _4510.z;
    results._m0[_4507 + 3u] = _4510.w;
    uint _4520 = (outAt + 160u) >> 2u;
    uint4 _4526 = as_type<uint4>(float4(root.contraction, _1709, 0.0));
    results._m0[_4520] = _4526.x;
    results._m0[_4520 + 1u] = _4526.y;
    results._m0[_4520 + 2u] = _4526.z;
    results._m0[_4520 + 3u] = _4526.w;
    uint _4536 = (outAt + 176u) >> 2u;
    uint2 _4538 = as_type<uint2>(float2(_4144, _4146));
    results._m0[_4536] = _4538.x;
    results._m0[_4536 + 1u] = _4538.y;
    uint _4544 = (outAt + 184u) >> 2u;
    uint2 _4546 = as_type<uint2>(float2(_4140, _4142));
    results._m0[_4544] = _4546.x;
    results._m0[_4544 + 1u] = _4546.y;
}

static inline __attribute__((always_inline))
void src_feature_jets_main(thread const uint3& id, constant type_Settings& Settings, device type_ByteAddressBuffer& frames, device type_ByteAddressBuffer& regions, device type_ByteAddressBuffer& queries, device type_ByteAddressBuffer& optical, device type_RWByteAddressBuffer& results, constant type_RootSettings& RootSettings, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments, constant type_TargetSettings& TargetSettings)
{
    bool _2385;
    if (id.y < Settings.queryCount)
    {
        _2385 = id.y >= 64u;
    }
    else
    {
        _2385 = true;
    }
    bool _2388;
    if (!_2385)
    {
        _2388 = id.x >= 128u;
    }
    else
    {
        _2388 = true;
    }
    if (_2388)
    {
        return;
    }
    uint _1535 = ((id.y * 128u) + id.x) * 192u;
    uint _1536 = id.y * 6160u;
    uint _1537 = id.y * 512u;
    for (uint _2389 = 0u; _2389 < 12u; _2389++)
    {
        uint _2391 = (_1535 + (_2389 * 16u)) >> 2u;
        results._m0[_2391] = 0u;
        results._m0[_2391 + 1u] = 0u;
        results._m0[_2391 + 2u] = 0u;
        results._m0[_2391 + 3u] = 0u;
    }
    uint _2396 = _1536 >> 2u;
    uint _2398 = regions._m0[_2396];
    bool _2408;
    if (Settings.queryCount <= 64u)
    {
        _2408 = RootSettings.rootCapacity != 128u;
    }
    else
    {
        _2408 = true;
    }
    bool _2413;
    if (!_2408)
    {
        _2413 = RootSettings.rootFrameStride != 512u;
    }
    else
    {
        _2413 = true;
    }
    bool _2418;
    if (!_2413)
    {
        _2418 = RootSettings.rootResultStride != 192u;
    }
    else
    {
        _2418 = true;
    }
    bool _2423;
    if (!_2418)
    {
        _2423 = RootSettings.rootMode > 1u;
    }
    else
    {
        _2423 = true;
    }
    bool _2426;
    if (!_2423)
    {
        _2426 = _2398 > 128u;
    }
    else
    {
        _2426 = true;
    }
    bool _2429;
    if (!_2426)
    {
        _2429 = regions._m0[(_1536 + 4u) >> 2u] != 0u;
    }
    else
    {
        _2429 = true;
    }
    if (_2429)
    {
        results._m0[(_1535 + 12u) >> 2u] = 32u;
        return;
    }
    bool _2436;
    if (RootSettings.rootMode != 0u)
    {
        _2436 = id.x != 0u;
    }
    else
    {
        _2436 = false;
    }
    if (_2436)
    {
        return;
    }
    bool _2441;
    if (RootSettings.rootMode == 0u)
    {
        _2441 = id.x >= _2398;
    }
    else
    {
        _2441 = false;
    }
    if (_2441)
    {
        return;
    }
    float4 box = float4(1000000015047466219876688855040.0, 1000000015047466219876688855040.0, -1000000015047466219876688855040.0, -1000000015047466219876688855040.0);
    uint _2445;
    if (RootSettings.rootMode != 0u)
    {
        _2445 = 0u;
    }
    else
    {
        _2445 = id.x;
    }
    bool _2446;
    _2446 = false;
    bool _2447;
    uint _2448 = _2445;
    for (;;)
    {
        uint _2452;
        if (RootSettings.rootMode != 0u)
        {
            _2452 = _2398;
        }
        else
        {
            _2452 = id.x + 1u;
        }
        if (_2448 < _2452)
        {
            uint _2454 = (((id.y * 128u) + _2448) * 32u) >> 2u;
            uint _2456 = optical._m0[_2454];
            uint _1551 = _2454 + 2u;
            bool _2465;
            if (optical._m0[_2454 + 1u] == 0u)
            {
                _2465 = optical._m0[_1551] == 0u;
            }
            else
            {
                _2465 = true;
            }
            bool _2468;
            if (!_2465)
            {
                _2468 = optical._m0[_1551] > 8192u;
            }
            else
            {
                _2468 = true;
            }
            bool _2471;
            if (!_2468)
            {
                _2471 = _2456 > optical._m0[_1551];
            }
            else
            {
                _2471 = true;
            }
            bool _2474;
            if (!_2471)
            {
                _2474 = optical._m0[_2454 + 3u] != (optical._m0[_1551] - _2456);
            }
            else
            {
                _2474 = true;
            }
            if (_2474)
            {
                results._m0[(_1535 + 12u) >> 2u] = 32u;
                return;
            }
            if (_2456 == 0u)
            {
                _2447 = _2446;
                uint _1563 = _2448 + 1u;
                _2446 = _2447;
                _2448 = _1563;
                continue;
            }
            uint _2478 = ((((id.y * 128u) + _2448) * 32u) + 16u) >> 2u;
            uint _2480 = optical._m0[_2478];
            uint _2482 = optical._m0[_2478 + 1u];
            uint _2484 = optical._m0[_2478 + 2u];
            uint _2486 = optical._m0[_2478 + 3u];
            float4 _2488 = as_type<float4>(uint4(_2480, _2482, _2484, _2486));
            bool4 _2489 = isnan(_2488);
            bool4 _2490 = isinf(_2488);
            bool _2498;
            if (all(not(bool4(_2489.x || _2490.x, _2489.y || _2490.y, _2489.z || _2490.z, _2489.w || _2490.w))))
            {
                _2498 = any(_2488.xy > _2488.zw);
            }
            else
            {
                _2498 = true;
            }
            if (_2498)
            {
                results._m0[(_1535 + 12u) >> 2u] = 64u;
                return;
            }
            box = float4(precise::min(box.xy, _2488.xy), precise::max(box.zw, _2488.zw));
            _2447 = true;
            uint _1563 = _2448 + 1u;
            _2446 = _2447;
            _2448 = _1563;
            continue;
        }
        else
        {
            break;
        }
    }
    uint _2513 = (_1537 + 496u) >> 2u;
    if (any(uint4(frames._m0[_2513], frames._m0[_2513 + 1u], frames._m0[_2513 + 2u], frames._m0[_2513 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        results._m0[(_1535 + 12u) >> 2u] = 64u;
        return;
    }
    uint _2527 = _1537 >> 2u;
    uint _2529 = frames._m0[_2527];
    uint _2531 = frames._m0[_2527 + 1u];
    uint _2533 = frames._m0[_2527 + 2u];
    uint _2535 = frames._m0[_2527 + 3u];
    float4 _2537 = as_type<float4>(uint4(_2529, _2531, _2533, _2535));
    uint _2538 = (_1537 + 16u) >> 2u;
    uint _2540 = frames._m0[_2538];
    uint _2542 = frames._m0[_2538 + 1u];
    uint _2544 = frames._m0[_2538 + 2u];
    uint _2546 = frames._m0[_2538 + 3u];
    float4 _2548 = as_type<float4>(uint4(_2540, _2542, _2544, _2546));
    uint _2549 = (_1537 + 32u) >> 2u;
    uint _2551 = frames._m0[_2549];
    uint _2553 = frames._m0[_2549 + 1u];
    uint _2555 = frames._m0[_2549 + 2u];
    uint _2557 = frames._m0[_2549 + 3u];
    float4 _2559 = as_type<float4>(uint4(_2551, _2553, _2555, _2557));
    uint _2560 = (_1537 + 48u) >> 2u;
    uint _2562 = frames._m0[_2560];
    uint _2564 = frames._m0[_2560 + 1u];
    uint _2566 = frames._m0[_2560 + 2u];
    uint _2568 = frames._m0[_2560 + 3u];
    float4 _2570 = as_type<float4>(uint4(_2562, _2564, _2566, _2568));
    uint _2571 = (_1537 + 64u) >> 2u;
    uint _2573 = frames._m0[_2571];
    uint _2575 = frames._m0[_2571 + 1u];
    uint _2577 = frames._m0[_2571 + 2u];
    uint _2579 = frames._m0[_2571 + 3u];
    float4 _2581 = as_type<float4>(uint4(_2573, _2575, _2577, _2579));
    uint _2582 = (_1537 + 80u) >> 2u;
    uint _2584 = frames._m0[_2582];
    uint _2586 = frames._m0[_2582 + 1u];
    uint _2588 = frames._m0[_2582 + 2u];
    uint _2590 = frames._m0[_2582 + 3u];
    float4 _2592 = as_type<float4>(uint4(_2584, _2586, _2588, _2590));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint _2593 = 0u; _2593 < 4u; _2593++)
    {
        uint _2595 = ((_1537 + 96u) + (_2593 * 48u)) >> 2u;
        planes[_2593].a = as_type<float4>(uint4(frames._m0[_2595], frames._m0[_2595 + 1u], frames._m0[_2595 + 2u], frames._m0[_2595 + 3u]));
        uint _2607 = ((_1537 + 112u) + (_2593 * 48u)) >> 2u;
        planes[_2593].b = as_type<float4>(uint4(frames._m0[_2607], frames._m0[_2607 + 1u], frames._m0[_2607 + 2u], frames._m0[_2607 + 3u]));
        uint _2619 = ((_1537 + 128u) + (_2593 * 48u)) >> 2u;
        planes[_2593].c = as_type<float4>(uint4(frames._m0[_2619], frames._m0[_2619 + 1u], frames._m0[_2619 + 2u], frames._m0[_2619 + 3u]));
    }
    uint _2631 = (_1537 + 288u) >> 2u;
    uint _2633 = frames._m0[_2631];
    uint _2635 = frames._m0[_2631 + 1u];
    uint _2637 = frames._m0[_2631 + 2u];
    uint _2639 = frames._m0[_2631 + 3u];
    float4 _2641 = as_type<float4>(uint4(_2633, _2635, _2637, _2639));
    uint _2642 = (_1537 + 304u) >> 2u;
    uint _2644 = frames._m0[_2642];
    uint _2646 = frames._m0[_2642 + 1u];
    uint _2648 = frames._m0[_2642 + 2u];
    uint _2650 = frames._m0[_2642 + 3u];
    float4 _2652 = as_type<float4>(uint4(_2644, _2646, _2648, _2650));
    uint _2653 = (_1537 + 320u) >> 2u;
    uint _2655 = frames._m0[_2653];
    uint _2657 = frames._m0[_2653 + 1u];
    uint _2659 = frames._m0[_2653 + 2u];
    uint _2661 = frames._m0[_2653 + 3u];
    float4 _2663 = as_type<float4>(uint4(_2655, _2657, _2659, _2661));
    uint _2664 = (_1537 + 336u) >> 2u;
    uint _2666 = frames._m0[_2664];
    uint _2668 = frames._m0[_2664 + 1u];
    uint _2670 = frames._m0[_2664 + 2u];
    uint _2672 = frames._m0[_2664 + 3u];
    float4 _2674 = as_type<float4>(uint4(_2666, _2668, _2670, _2672));
    uint _2675 = (_1537 + 352u) >> 2u;
    uint _2677 = frames._m0[_2675];
    uint _2679 = frames._m0[_2675 + 1u];
    uint _2681 = frames._m0[_2675 + 2u];
    uint _2683 = frames._m0[_2675 + 3u];
    float4 _2685 = as_type<float4>(uint4(_2677, _2679, _2681, _2683));
    uint _2686 = (_1537 + 368u) >> 2u;
    uint _2688 = frames._m0[_2686];
    uint _2690 = frames._m0[_2686 + 1u];
    uint _2692 = frames._m0[_2686 + 2u];
    uint _2694 = frames._m0[_2686 + 3u];
    float4 _2696 = as_type<float4>(uint4(_2688, _2690, _2692, _2694));
    uint _2697 = (_1537 + 384u) >> 2u;
    uint _2699 = frames._m0[_2697];
    uint _2701 = frames._m0[_2697 + 1u];
    uint _2703 = frames._m0[_2697 + 2u];
    uint _2705 = frames._m0[_2697 + 3u];
    float4 _2707 = as_type<float4>(uint4(_2699, _2701, _2703, _2705));
    uint _2708 = (_1537 + 400u) >> 2u;
    uint _2710 = frames._m0[_2708];
    uint _2712 = frames._m0[_2708 + 1u];
    uint _2714 = frames._m0[_2708 + 2u];
    uint _2716 = frames._m0[_2708 + 3u];
    float4 _2718 = as_type<float4>(uint4(_2710, _2712, _2714, _2716));
    uint _2719 = (_1537 + 416u) >> 2u;
    uint _2721 = frames._m0[_2719];
    uint _2723 = frames._m0[_2719 + 1u];
    uint _2725 = frames._m0[_2719 + 2u];
    uint _2727 = frames._m0[_2719 + 3u];
    float4 _2729 = as_type<float4>(uint4(_2721, _2723, _2725, _2727));
    uint _2730 = (_1537 + 480u) >> 2u;
    uint _2732 = frames._m0[_2730];
    uint _1648 = _2730 + 1u;
    uint _2734 = frames._m0[_1648];
    uint _1649 = _2730 + 2u;
    uint _2736 = frames._m0[_1649];
    uint _1650 = _2730 + 3u;
    uint _2738 = frames._m0[_1650];
    uint4 _2739 = uint4(_2732, _2734, _2736, _2738);
    uint _2743 = (id.y * 48u) >> 2u;
    uint _2746 = ((id.y * 48u) + 44u) >> 2u;
    uint _2749 = (_1537 + 432u) >> 2u;
    uint _2760 = (_1537 + 448u) >> 2u;
    uint _2771 = (_1537 + 464u) >> 2u;
    bool _2786;
    if (_2732 <= 4u)
    {
        _2786 = (_2734 >> (_2732 & 31u)) != 0u;
    }
    else
    {
        _2786 = true;
    }
    bool _2789;
    if (!_2786)
    {
        _2789 = _2736 > 1u;
    }
    else
    {
        _2789 = true;
    }
    bool _2792;
    if (!_2789)
    {
        _2792 = _2738 < 1u;
    }
    else
    {
        _2792 = true;
    }
    bool _2795;
    if (!_2792)
    {
        _2795 = _2738 > 3u;
    }
    else
    {
        _2795 = true;
    }
    bool _2801;
    if (!_2795)
    {
        bool _2800;
        if (_2738 == 3u)
        {
            _2800 = _2732 != 4u;
        }
        else
        {
            _2800 = _2732 == 4u;
        }
        _2801 = _2800;
    }
    else
    {
        _2801 = true;
    }
    bool _2804;
    if (!_2801)
    {
        _2804 = queries._m0[_2743] == 4294967295u;
    }
    else
    {
        _2804 = true;
    }
    bool _2809;
    if (!_2804)
    {
        uint _2807;
        if (queries._m0[_2743] == 4294967293u)
        {
            _2807 = 1u;
        }
        else
        {
            _2807 = 0u;
        }
        _2809 = _2736 != _2807;
    }
    else
    {
        _2809 = true;
    }
    bool _2814;
    if (!_2809)
    {
        _2814 = queries._m0[_2746] >= Settings.lobes;
    }
    else
    {
        _2814 = true;
    }
    bool _2817;
    if (!_2814)
    {
        _2817 = queries._m0[_2746] > 7u;
    }
    else
    {
        _2817 = true;
    }
    bool _2824;
    if (!_2817)
    {
        _2824 = queries._m0[((id.y * 48u) + 20u) >> 2u] != (((_2738 << 8u) | (_2734 << 4u)) | _2732);
    }
    else
    {
        _2824 = true;
    }
    bool _2832;
    if (!_2824)
    {
        bool4 _2826 = isnan(_2592);
        bool4 _2827 = isinf(_2592);
        _2832 = !all(not(bool4(_2826.x || _2827.x, _2826.y || _2827.y, _2826.z || _2827.z, _2826.w || _2827.w)));
    }
    else
    {
        _2832 = true;
    }
    bool _2836;
    if (!_2832)
    {
        _2836 = _2592.x < 0.0;
    }
    else
    {
        _2836 = true;
    }
    bool _2840;
    if (!_2836)
    {
        _2840 = _2592.x > 1.0;
    }
    else
    {
        _2840 = true;
    }
    bool _2845;
    if (!_2840)
    {
        _2845 = _2592.y != float(queries._m0[_2746]);
    }
    else
    {
        _2845 = true;
    }
    bool _2850;
    if (!_2845)
    {
        _2850 = any(_2592.zw != float2(0.0));
    }
    else
    {
        _2850 = true;
    }
    bool _2858;
    if (!_2850)
    {
        bool4 _2852 = isnan(_2570);
        bool4 _2853 = isinf(_2570);
        _2858 = !all(not(bool4(_2852.x || _2853.x, _2852.y || _2853.y, _2852.z || _2853.z, _2852.w || _2853.w)));
    }
    else
    {
        _2858 = true;
    }
    bool _2863;
    if (!_2858)
    {
        _2863 = any(_2570.xy <= float2(0.0));
    }
    else
    {
        _2863 = true;
    }
    bool _2871;
    if (!_2863)
    {
        bool4 _2865 = isnan(_2581);
        bool4 _2866 = isinf(_2581);
        _2871 = !all(not(bool4(_2865.x || _2866.x, _2865.y || _2866.y, _2865.z || _2866.z, _2865.w || _2866.w)));
    }
    else
    {
        _2871 = true;
    }
    bool _2883;
    if (!_2871)
    {
        _2883 = any(_2581.xy != float2(float(Settings.width), float(Settings.height)));
    }
    else
    {
        _2883 = true;
    }
    bool _2888;
    if (!_2883)
    {
        _2888 = any(_2581.xy < float2(1.0));
    }
    else
    {
        _2888 = true;
    }
    bool _2893;
    if (!_2888)
    {
        _2893 = any(_2581.xy > float2(16384.0));
    }
    else
    {
        _2893 = true;
    }
    bool _2897;
    if (!_2893)
    {
        _2897 = _2581.z <= 0.0;
    }
    else
    {
        _2897 = true;
    }
    bool _2902;
    if (!_2897)
    {
        _2902 = _2581.w <= _2581.z;
    }
    else
    {
        _2902 = true;
    }
    bool _2910;
    if (!_2902)
    {
        bool4 _2904 = isnan(_2729);
        bool4 _2905 = isinf(_2729);
        _2910 = !all(not(bool4(_2904.x || _2905.x, _2904.y || _2905.y, _2904.z || _2905.z, _2904.w || _2905.w)));
    }
    else
    {
        _2910 = true;
    }
    bool _2917;
    if (!_2910)
    {
        int _2914;
        if (_2738 == 1u)
        {
            _2914 = 1;
        }
        else
        {
            _2914 = 0;
        }
        _2917 = _2729.w != float(_2914);
    }
    else
    {
        _2917 = true;
    }
    bool _2922;
    if (!_2917)
    {
        float3 param_var_f = _2729.xyz;
        uint param_var_kind = _2738;
        _2922 = !feature_valid(param_var_f, param_var_kind);
    }
    else
    {
        _2922 = true;
    }
    bool _2930;
    if (!_2922)
    {
        bool _2929;
        if (_2729.w != 0.0)
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ as_type<float4>(uint4(frames._m0[_2749], frames._m0[_2749 + 1u], frames._m0[_2749 + 2u], frames._m0[_2749 + 3u])), as_type<float4>(uint4(frames._m0[_2760], frames._m0[_2760 + 1u], frames._m0[_2760 + 2u], frames._m0[_2760 + 3u])), as_type<float4>(uint4(frames._m0[_2771], frames._m0[_2771 + 1u], frames._m0[_2771 + 2u], frames._m0[_2771 + 3u])) };
            _2929 = !reflection_specular_plane_valid(param_var_plane);
        }
        else
        {
            _2929 = false;
        }
        _2930 = _2929;
    }
    else
    {
        _2930 = true;
    }
    bool _2937;
    if (!_2930)
    {
        bool _2936;
        if (_2736 == 0u)
        {
            ReflectionRoughFrame param_var_old = ReflectionRoughFrame{ _2537, _2548, _2559, _2570, _2581, _2592 };
            _2936 = !reflection_rough_frame_valid(param_var_old);
        }
        else
        {
            _2936 = false;
        }
        _2937 = _2936;
    }
    else
    {
        _2937 = true;
    }
    if (_2937)
    {
        results._m0[(_1535 + 12u) >> 2u] = 64u;
        return;
    }
    bool _2942;
    if (_2736 == 0u)
    {
        _2942 = _2734 != 0u;
    }
    else
    {
        _2942 = true;
    }
    if (_2942)
    {
        ReflectionLiquidFrame param_var_old_1 = ReflectionLiquidFrame{ _2641, _2652, _2663, _2674, _2685, _2696, _2707, _2718 };
        bool _2947;
        if (reflection_liquid_frame_valid(param_var_old_1))
        {
            _2947 = any(_2570 != _2696);
        }
        else
        {
            _2947 = true;
        }
        bool _2951;
        if (!_2947)
        {
            _2951 = any(_2581 != _2707);
        }
        else
        {
            _2951 = true;
        }
        if (_2951)
        {
            results._m0[(_1535 + 12u) >> 2u] = 64u;
            return;
        }
    }
    for (uint _2954 = 0u; _2954 < _2732; _2954++)
    {
        bool _2964;
        if ((_2734 & (1u << (_2954 & 31u))) == 0u)
        {
            ReflectionSpecularPlane param_var_plane_1 = planes[_2954];
            _2964 = !reflection_specular_plane_valid(param_var_plane_1);
        }
        else
        {
            _2964 = false;
        }
        if (_2964)
        {
            results._m0[(_1535 + 12u) >> 2u] = 64u;
            return;
        }
    }
    if (!_2446)
    {
        results._m0[(_1535 + 12u) >> 2u] = 4096u;
        return;
    }
    bool4 _2971 = isnan(box);
    bool4 _2972 = isinf(box);
    bool _2981;
    if (all(not(bool4(_2971.x || _2972.x, _2971.y || _2972.y, _2971.z || _2972.z, _2971.w || _2972.w))))
    {
        _2981 = any(box.xy > box.zw);
    }
    else
    {
        _2981 = true;
    }
    if (_2981)
    {
        results._m0[(_1535 + 12u) >> 2u] = 64u;
        return;
    }
    intervalFailed = false;
    uint param_var_at = _1537;
    float4 param_var_feature = _2729;
    Interval3 _2984 = native_target(param_var_at, param_var_feature, frames, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
    if (intervalFailed)
    {
        results._m0[(_1535 + 12u) >> 2u] = 128u;
        return;
    }
    for (uint _2988 = 0u; _2988 < 2u; _2988++)
    {
        for (uint _2990 = 0u; _2990 < 12u; _2990++)
        {
            uint _2992 = (_1535 + (_2990 * 16u)) >> 2u;
            results._m0[_2992] = 0u;
            results._m0[_2992 + 1u] = 0u;
            results._m0[_2992 + 2u] = 0u;
            results._m0[_2992 + 3u] = 0u;
        }
        uint param_var_outAt = _1535;
        float4 param_var_box = box;
        ReflectionRoughFrame param_var_receiver = ReflectionRoughFrame{ _2537, _2548, _2559, _2570, _2581, _2592 };
        ReflectionLiquidFrame param_var_liquid = ReflectionLiquidFrame{ _2641, _2652, _2663, _2674, _2685, _2696, _2707, _2718 };
        spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
        uint4 param_var_control = _2739;
        Interval3 param_var_target = _2984;
        bool param_var_finiteTarget = _2729.w != 0.0;
        uint _3006;
        if (RootSettings.rootMode != 0u)
        {
            _3006 = 2u;
        }
        else
        {
            _3006 = 0u;
        }
        uint param_var_refinements = _3006;
        write_optical_local_root(param_var_outAt, param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTarget, param_var_refinements, results, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
        bool _3012;
        if (RootSettings.rootMode != 0u)
        {
            _3012 = _2988 != 0u;
        }
        else
        {
            _3012 = true;
        }
        bool _3018;
        if (!_3012)
        {
            _3018 = results._m0[_1535 >> 2u] != 1u;
        }
        else
        {
            _3018 = true;
        }
        bool _3024;
        if (!_3018)
        {
            _3024 = results._m0[(_1535 + 4u) >> 2u] != 0u;
        }
        else
        {
            _3024 = true;
        }
        bool _3030;
        if (!_3024)
        {
            _3030 = results._m0[(_1535 + 12u) >> 2u] != 0u;
        }
        else
        {
            _3030 = true;
        }
        if (_3030)
        {
            return;
        }
        intervalFailed = false;
        Interval _2375 = Interval{ box.x, box.x };
        Interval _2376 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _3037 = iadd(_2375, _2376, intervalFailed);
        Interval _2373 = Interval{ box.y, box.y };
        Interval _2374 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _3045 = iadd(_2373, _2374, intervalFailed);
        Interval param_var_a = Interval{ box.z, box.z };
        Interval param_var_b = Interval{ 0.125, 0.125 };
        Interval _3050 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval param_var_a_1 = Interval{ box.w, box.w };
        Interval param_var_b_1 = Interval{ 0.125, 0.125 };
        Interval _3055 = iadd(param_var_a_1, param_var_b_1, intervalFailed);
        if (intervalFailed)
        {
            return;
        }
        float4 _3072 = float4(precise::min(box.xy, precise::max(float2(0.5), float2(_3037.lo, _3045.lo))), precise::max(box.zw, precise::min(spvFSub(_2581.xy, float2(0.5)), float2(_3050.hi, _3055.hi))));
        bool4 _3073 = isnan(_3072);
        bool4 _3074 = isinf(_3072);
        bool _3083;
        if (all(not(bool4(_3073.x || _3074.x, _3073.y || _3074.y, _3073.z || _3074.z, _3073.w || _3074.w))))
        {
            _3083 = any(_3072.xy > box.xy);
        }
        else
        {
            _3083 = true;
        }
        bool _3090;
        if (!_3083)
        {
            _3090 = any(_3072.zw < box.zw);
        }
        else
        {
            _3090 = true;
        }
        bool _3095;
        if (!_3090)
        {
            _3095 = all(_3072 == box);
        }
        else
        {
            _3095 = true;
        }
        if (_3095)
        {
            return;
        }
        box = _3072;
    }
}

kernel void feature_jets_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& frames [[buffer(3)]], device type_ByteAddressBuffer& regions [[buffer(4)]], device type_ByteAddressBuffer& queries [[buffer(5)]], device type_ByteAddressBuffer& optical [[buffer(6)]], device type_RWByteAddressBuffer& results [[buffer(7)]], constant type_RootSettings& RootSettings [[buffer(1)]], constant type_TargetSettings& TargetSettings [[buffer(2)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    bool intervalFailed = false;
    float optical_product_upper = 0.0;
    float interval_divide_upper = 0.0;
    float interval_sine_upper = 0.0;
    bool jetBranchKnown = true;
    uint jetFailureSite = 0u;
    float4 jetFailureArguments = float4(0.0);
    uint3 param_var_id = gl_GlobalInvocationID;
    src_feature_jets_main(param_var_id, Settings, frames, regions, queries, optical, results, RootSettings, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments, TargetSettings);
}

